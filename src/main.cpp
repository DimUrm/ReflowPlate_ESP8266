#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <LittleFS.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "config.h"
#include "buzzer.h"
#include "profile.h"
#include "web_index_gz.h"

// ================= ДРАЙВЕР РАСШИРИТЕЛЯ PCF8574 =================
uint8_t pcf_address = 0;
bool pcf_found = false;
uint8_t pcf_output_byte = 0x00;
bool fan_manual = false;
bool fan_active = false;

// Предварительное объявление проверки I2C
bool i2cPresent(uint8_t a);

void pcfWritePin(uint8_t pin, bool state) {
  if (!pcf_found) return;
  if (state) pcf_output_byte |= (1 << pin);
  else pcf_output_byte &= ~(1 << pin);

  // Пины кнопок (P2, P3) всегда должны иметь '1' в выходном регистре для работы подтяжки на вход
  pcf_output_byte |= (1 << PCF_PIN_BTN_BACK) | (1 << PCF_PIN_BTN_CONF);

  Wire.beginTransmission(pcf_address);
  Wire.write(pcf_output_byte);
  Wire.endTransmission();
}

uint8_t pcfReadByte() {
  if (!pcf_found) return 0xFF;
  Wire.requestFrom(pcf_address, (uint8_t)1);
  if (Wire.available()) {
    return Wire.read();
  }
  return 0xFF;
}

bool initPCF8574() {
  uint8_t addrs[] = {PCF8574_ADDR_1, PCF8574_ADDR_2, PCF8574A_ADDR};
  for (uint8_t a : addrs) {
    if (i2cPresent(a)) {
      pcf_address = a;
      pcf_found = true;
      // Включаем подтяжку '1' на пинах кнопок (P2 и P3), выходы P0/P1 гасим в 0
      pcf_output_byte = (1 << PCF_PIN_BTN_BACK) | (1 << PCF_PIN_BTN_CONF);
      Wire.beginTransmission(pcf_address);
      Wire.write(pcf_output_byte);
      Wire.endTransmission();
      return true;
    }
  }
  return false;
}

// Создание экземпляров классов
BuzzerEngine buzzer;
ProfileEngine profile;

// Переменные системы Auto-Sleep / Standby
uint32_t last_activity_time = 0;       // Время последнего действия пользователя
uint32_t standby_timeout_min = 10;     // Минут до перехода в Standby (0 = выключено)
StandbyMode standby_state = STBY_ACTIVE;
float pre_standby_target = 150.0f;     // Запоминаем уставку перед сном
bool oled_sleeping = false; // Флаг: спит ли экран

// Переменные PID Auto-Tune (Метод релейной обратной связи)
TuneState tune_state = TUNE_OFF;
uint8_t   tune_cycles = 0;
uint32_t  tune_t1 = 0, tune_t2 = 0;
float     tune_t_low = 0, tune_t_high = 0;
bool      tune_relay_high = false;
float     tune_target = 150.0f;

// Статистика и учет энергии
float    session_wh = 0.0f;          // Потребленная энергия за текущую сессию (Ватт-часы)
uint32_t session_heat_sec = 0;       // Время активного нагрева за сессию (сек)
uint32_t lifetime_heat_min = 0;      // Общие моточасы ТЭНа за всё время (минуты, сохраняются в флеш)
uint32_t reflow_cycles_count = 0;    // Количество успешно проведенных циклов пайки
uint32_t last_energy_calc = 0;

Adafruit_SSD1306 display(128, 64, &Wire, -1);
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// Конфигурация и переменные состояния
char wifi_ssid[33] = "", wifi_pass[65] = "", web_pin[33] = "";
char mqtt_server[64] = "192.168.1.100", mqtt_user[32] = "", mqtt_pass[32] = "";
char mqtt_topic[64] = "home/reflow_plate";
int  mqtt_port = 1883;
bool mqtt_enabled = false;
uint8_t cfg_sensor = CFG_AUTO;

float Kp = 5.0f, Ki = 0.05f, Kd = 20.0f;

uint8_t sensor = SN_SIM;
uint8_t sensor_st = SS_OK;
uint8_t fault = F_NONE;
bool    temp_valid = false, filter_init = false;
float   temp_f = 0, temp_prev = 0, rate_f = 0;
float   target_temp = 150.0f;
bool    is_heating = false, ssr_on = false;
float   PID_P = 0, PID_I = 0, PID_D = 0, output_power = 0;
uint32_t heat_start = 0, window_start = 0, last_tick = 0;
bool    nr_active = false; uint32_t nr_start = 0; float nr_temp = 0;
float   sim_T = SIM_AMB, sim_S = SIM_AMB;

bool    oled_ok = false;
uint32_t oled_probe = 0;
volatile int enc_delta = 0;

bool cfg_save = false, apply_sensor = false, mqtt_reconf = true, forceTele = true;
uint32_t restart_at = 0;

void IRAM_ATTR isrEncoder() {
  static uint32_t last = 0;
  uint32_t t = micros();
  if (t - last < 3000) return;
  last = t;
  if (digitalRead(PIN_ENC_DT)) enc_delta++; else enc_delta--;
}

// ================= РАБОТА С ФАЙЛОМ PROFILES.JSON =================
void createDefaultProfiles() {
  File f = LittleFS.open("/profiles.json", "w");
  if (!f) return;
  const char* def_json = "["
    "{\"id\":0,\"name\":\"Sn63\",\"desc\":\"Leaded Sn63Pb37\",\"pre\":150,\"soak\":165,\"soak_t\":60,\"ref\":215,\"ref_t\":25},"
    "{\"id\":1,\"name\":\"LeadFree\",\"desc\":\"Lead-Free SAC305\",\"pre\":160,\"soak\":180,\"soak_t\":75,\"ref\":235,\"ref_t\":30},"
    "{\"id\":2,\"name\":\"Bi58_Low\",\"desc\":\"Low-temp Sn42Bi58\",\"pre\":100,\"soak\":120,\"soak_t\":45,\"ref\":165,\"ref_t\":20},"
    "{\"id\":3,\"name\":\"LED_Alu\",\"desc\":\"Alu LED Strip\",\"pre\":120,\"soak\":140,\"soak_t\":30,\"ref\":190,\"ref_t\":25}"
  "]";
  f.print(def_json);
  f.close();
  Serial.println("[FS] Created default /profiles.json");
}

void loadProfiles() {
  if (!LittleFS.exists("/profiles.json")) {
    createDefaultProfiles();
  }
  File f = LittleFS.open("/profiles.json", "r");
  if (!f) return;
  DynamicJsonDocument doc(1024);
  DeserializationError err = deserializeJson(doc, f);
  f.close();

  if (err) {
    Serial.println("[FS] profiles.json corrupted, recreating defaults...");
    createDefaultProfiles();
    return;
  }

  JsonArray arr = doc.as<JsonArray>();
  for (size_t i = 0; i < 4 && i < arr.size(); i++) {
    JsonObject obj = arr[i];
    strlcpy(profile.list[i].name, obj["name"] | "Prof", sizeof(profile.list[i].name));
    strlcpy(profile.list[i].desc, obj["desc"] | "", sizeof(profile.list[i].desc));
    profile.list[i].preheat  = obj["pre"] | 150.0f;
    profile.list[i].soak     = obj["soak"] | 165.0f;
    profile.list[i].soak_t   = obj["soak_t"] | 60;
    profile.list[i].reflow   = obj["ref"] | 215.0f;
    profile.list[i].reflow_t = obj["ref_t"] | 25;
  }
  Serial.println("[FS] 4 Profiles loaded from /profiles.json");
}

void saveProfiles() {
  File f = LittleFS.open("/profiles.json", "w");
  if (!f) return;
  DynamicJsonDocument doc(1024);
  JsonArray arr = doc.to<JsonArray>();
  for (uint8_t i = 0; i < 4; i++) {
    JsonObject obj = arr.createNestedObject();
    obj["id"]     = i;
    obj["name"]   = profile.list[i].name;
    obj["desc"]   = profile.list[i].desc;
    obj["pre"]    = (int)profile.list[i].preheat;
    obj["soak"]   = (int)profile.list[i].soak;
    obj["soak_t"] = profile.list[i].soak_t;
    obj["ref"]    = (int)profile.list[i].reflow;
    obj["ref_t"]  = profile.list[i].reflow_t;
  }
  serializeJson(doc, f);
  f.close();
  Serial.println("[FS] Profiles saved to LittleFS");
}

void loadConfig() {
  if (!LittleFS.begin()) return;
  File f = LittleFS.open("/config.json", "r");
  if (!f) return;
  DynamicJsonDocument d(1536);
  if (!deserializeJson(d, f)) {
    strlcpy(wifi_ssid,   d["wifi_ssid"] | "", sizeof(wifi_ssid));
    strlcpy(wifi_pass,   d["wifi_pass"] | "", sizeof(wifi_pass));
    strlcpy(web_pin,     d["pin"] | "", sizeof(web_pin));
    strlcpy(mqtt_server, d["server"] | "192.168.1.100", sizeof(mqtt_server));
    strlcpy(mqtt_user,   d["user"] | "", sizeof(mqtt_user));
    strlcpy(mqtt_pass,   d["pass"] | "", sizeof(mqtt_pass));
    strlcpy(mqtt_topic,  d["topic"] | "home/reflow_plate", sizeof(mqtt_topic));
    mqtt_port    = d["port"] | 1883;
    mqtt_enabled = d["enabled"] | false;
    buzzer.mute  = d["mute"] | false;
    standby_timeout_min = d["stby_m"] | 10;
    lifetime_heat_min   = d["tot_m"] | 0;
    reflow_cycles_count = d["r_cyc"] | 0;
    cfg_sensor   = d["sensor"] | 0;
    if (cfg_sensor > CFG_SIM) cfg_sensor = CFG_AUTO;
    Kp = d["kp"] | Kp; Ki = d["ki"] | Ki; Kd = d["kd"] | Kd;
  }
  f.close();
}

void saveConfig() {
  File f = LittleFS.open("/config.json", "w");
  if (!f) return;
  DynamicJsonDocument d(1536);
  d["wifi_ssid"] = wifi_ssid; d["wifi_pass"] = wifi_pass; d["pin"] = web_pin;
  d["server"] = mqtt_server; d["port"] = mqtt_port; d["user"] = mqtt_user;
  d["pass"] = mqtt_pass; d["topic"] = mqtt_topic; d["enabled"] = mqtt_enabled;
  d["sensor"] = cfg_sensor; d["kp"] = Kp; d["ki"] = Ki; d["kd"] = Kd;
  d["mute"] = buzzer.mute;
  d["stby_m"] = standby_timeout_min;
  d["tot_m"]  = lifetime_heat_min;
  d["r_cyc"]  = reflow_cycles_count;
  // Передаем 4 профиля в браузер
  JsonArray prof_arr = d.createNestedArray("profiles");
  for (uint8_t i = 0; i < 4; i++) {
    JsonObject p = prof_arr.createNestedObject();
    p["id"]     = i;
    p["name"]   = profile.list[i].name;
    p["desc"]   = profile.list[i].desc;
    p["pre"]    = (int)profile.list[i].preheat;
    p["soak"]   = (int)profile.list[i].soak;
    p["soak_t"] = profile.list[i].soak_t;
    p["ref"]    = (int)profile.list[i].reflow;
    p["ref_t"]  = profile.list[i].reflow_t;
  }

  String out;
  serializeJson(d, f);
  f.close();
}

void setSecret(char* dst, size_t n, const char* in) {
  if (!in || !in[0]) return;
  if (strcmp(in, "-") == 0) dst[0] = 0; else strlcpy(dst, in, n);
}

uint16_t maxRaw() {
  digitalWrite(PIN_MAX_CS, LOW);
  delayMicroseconds(10);
  uint16_t v = 0;
  for (int i = 0; i < 16; i++) {
    digitalWrite(PIN_MAX_SCK, LOW);
    delayMicroseconds(5);
    v = (v << 1) | (digitalRead(PIN_MAX_SO) ? 1 : 0);
    digitalWrite(PIN_MAX_SCK, HIGH);
    delayMicroseconds(5);
  }
  digitalWrite(PIN_MAX_CS, HIGH);
  return v;
}

uint8_t readMax(float &t) {
  uint16_t v = maxRaw();
  if (v == 0x0000 || (v & 0x8002)) return SS_ABSENT;
  if (v & 0x0004) return SS_OPEN;
  t = (v >> 3) * 0.25f;
  return SS_OK;
}

uint8_t readNtc(float &t) {
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) sum += analogRead(A0);
  float avg = sum / 8.0f;
  if (avg < 5.0f || avg > 1020.0f) return SS_OPEN;
  float ratio = (avg / 1023.0f) * (ADC_FULL_V / NTC_VCC);
  float r = NTC_R_FIXED * (1.0f - ratio) / ratio;
  float inv = logf(r / NTC_R0) / NTC_BETA + 1.0f / (25.0f + 273.15f);
  t = 1.0f / inv - 273.15f;
  if (t < -30.0f || t > 400.0f) return SS_RANGE;
  return SS_OK;
}

bool maxPresent() {
  int found = 0;
  delay(300);
  for (int i = 0; i < 3; i++) {
    float t;
    if (readMax(t) != SS_ABSENT) found++;
    delay(260);
  }
  return found == 3;
}

void simStep(float dt) {
  float p = (is_heating && fault == F_NONE) ? SIM_PMAX * output_power / 100.0f : 0.0f;
  sim_T += (p - SIM_K * (sim_T - SIM_AMB)) / SIM_C * dt;
  sim_S += (sim_T - sim_S) * (1.0f - expf(-dt / SIM_TAU_S));
}
float simReading() {
  float v = sim_S + random(-100, 101) / 400.0f;
  return roundf(v * 4.0f) / 4.0f;
}

static inline bool okf(float v) { return !isnan(v) && fabsf(v) < 1e30f; }
float limitTemp() { return sensor == SN_NTC ? LIMIT_NTC : LIMIT_TC; }

void setTarget(float v) {
  if (!okf(v)) return;
  float mx = limitTemp() - TARGET_MARGIN;
  v = roundf(v);
  target_temp = v < 0 ? 0 : (v > mx ? mx : v);
}

void pidReset() { PID_I = 0; PID_P = 0; PID_D = 0; rate_f = 0; temp_prev = temp_f; nr_active = false; }

void stopHeating() { 
  is_heating = false; 
  output_power = 0; 
  profile.stop();
  tune_state = TUNE_OFF;
  pidReset(); 
}

// ================= ДВИЖОК PID AUTO-TUNE =================
void startAutoTune() {
  if (fault != F_NONE || !temp_valid) return;
  stopHeating();
  tune_target = 150.0f;
  setTarget(tune_target);
  tune_state = TUNE_RUNNING;
  tune_cycles = 0;
  tune_t_low = 500.0f;
  tune_t_high = 0.0f;
  tune_relay_high = true;
  tune_t1 = millis();
  is_heating = true;
  output_power = 100.0f;
  strlcpy(profile.name, "Tune", sizeof(profile.name));
  buzzer.play(SND_REFLOW_START);
  forceTele = true;
}

void runAutoTune(uint32_t now) {
  if (tune_state != TUNE_RUNNING) return;

  // Фиксируем пики экстремумов волны
  if (temp_f > tune_t_high) tune_t_high = temp_f;
  if (temp_f < tune_t_low)  tune_t_low  = temp_f;

  // Релейное переключение мощности (100% или 0%)
  if (tune_relay_high && temp_f >= tune_target) {
    tune_relay_high = false;
    output_power = 0.0f; // Перевалили за 150°C -> глушим ТЭН
  } 
  else if (!tune_relay_high && temp_f <= tune_target) {
    tune_relay_high = true;
    output_power = 100.0f; // Упали ниже 150°C -> жарим на 100%
    tune_cycles++;

    if (tune_cycles == 1) {
      tune_t1 = now; // Отсечка старта стабильной волны
      tune_t_high = temp_f;
      tune_t_low = temp_f;
    } 
    else if (tune_cycles >= 4) { // Сделали 3 полных периода колебаний!
      tune_t2 = now;
      float pu = ((tune_t2 - tune_t1) / 3000.0f);          // Период колебаний (сек)
      float a  = (tune_t_high - tune_t_low) / 2.0f;        // Амплитуда перелета (°C)
      if (a < 1.0f) a = 1.0f;

      // Формулы Циглера-Никольса для ПИД:
      float ku = (4.0f * 100.0f) / (3.14159f * a);        // Предельный коэффициент
      Kp = 0.6f * ku;
      Ki = 1.2f * ku / pu;
      Kd = 0.075f * ku * pu;

      // Ограничиваем адекватными пределами
      Kp = constrain(Kp, 0.5f, 50.0f);
      Ki = constrain(Ki, 0.001f, 1.0f);
      Kd = constrain(Kd, 1.0f, 200.0f);

      tune_state = TUNE_DONE;
      stopHeating();
      cfg_save = true; // Сохраняем идеальный ПИД в память!
      buzzer.play(SND_COMPLETE); // Победный сигнал!
      forceTele = true;
    }
  }
}

void tripFault(uint8_t f) {
  if (fault == F_NONE) {
    fault = f;
    buzzer.play(SND_FAULT);
  }
  stopHeating();
  forceTele = true;
}

bool startHeating() {
  if (fault != F_NONE || !temp_valid) return false;
  pidReset();
  is_heating = true;
  heat_start = millis();
  buzzer.play(SND_CLICK);
  return true;
}

void applySensor() {
  switch (cfg_sensor) {
    case CFG_MAX: sensor = SN_MAX; break;
    case CFG_NTC: sensor = SN_NTC; break;
    case CFG_SIM: sensor = SN_SIM; break;
    default:      sensor = maxPresent() ? SN_MAX : SN_SIM;
  }
  filter_init = false; temp_valid = false;
  sim_T = sim_S = SIM_AMB;
  setTarget(target_temp);
  Serial.printf("Sensor: %s\n", SENSOR_NAMES[sensor]);
}

void controlTick(uint32_t now) {
  float dt = (now - last_tick) / 1000.0f;
  last_tick = now;
  if (dt <= 0 || dt > 5) dt = SAMPLE_MS / 1000.0f;

  static uint32_t probe_t = 0; static uint8_t hits = 0;
  if (cfg_sensor == CFG_AUTO && sensor == SN_SIM && !is_heating && now - probe_t > 3000) {
    probe_t = now;
    float tt;
    if (readMax(tt) != SS_ABSENT) { if (++hits >= 3) { sensor = SN_MAX; filter_init = false; hits = 0; } }
    else hits = 0;
  }

  float raw = 0; uint8_t st = SS_OK;
  if (sensor == SN_SIM)      { simStep(dt); raw = simReading(); }
  else if (sensor == SN_MAX) st = readMax(raw);
  else                       st = readNtc(raw);

  sensor_st = st;
  temp_valid = (st == SS_OK);

  if (!temp_valid) {
    filter_init = false;
    output_power = 0;
    if (is_heating) tripFault(st == SS_ABSENT ? F_NO_SENSOR : (st == SS_OPEN ? F_SENSOR_OPEN : F_SENSOR_RANGE));
    return;
  }

  if (!filter_init) { temp_f = raw; temp_prev = raw; rate_f = 0; filter_init = true; }
  else { temp_prev = temp_f; temp_f += 0.35f * (raw - temp_f); }

if (raw > limitTemp()) tripFault(F_OVERTEMP);
  if (is_heating && now - heat_start > HEAT_MAX_MS) tripFault(F_TIMEOUT);

  // Если запущен автотюн — работает его алгоритм
  if (tune_state == TUNE_RUNNING) {
    runAutoTune(now);
    return; // Обычный ПИД не нужен, пока идет автокалибровка!
  }

  // Обслуживание профиля с запросом на охлаждение
  bool auto_fan = false;
  ReflowPhase prev_phase = profile.current_phase;
  profile.tick(now, temp_f, target_temp, is_heating, auto_fan);

  // Если фаза REFLOW успешно завершилась и перешла в COOLDOWN -> засчитываем цикл пайки!
  if (prev_phase == PHASE_REFLOW && profile.current_phase == PHASE_COOLDOWN) {
    reflow_cycles_count++;
    cfg_save = true; // Запоминаем в память
  }

  // Вентилятор активен, если включен кнопкой вручную ИЛИ идет фаза Cooldown
  fan_active = fan_manual || auto_fan;
  pcfWritePin(PCF_PIN_FAN, fan_active);

  // Проверка бездействия в ручном режиме
  if (is_heating && !profile.is_active && standby_timeout_min > 0) {
    uint32_t idle_ms = now - last_activity_time;
    uint32_t t_standby = standby_timeout_min * 60UL * 1000UL;
    uint32_t t_off     = t_standby + (15UL * 60UL * 1000UL);

    if (standby_state == STBY_ACTIVE && idle_ms >= t_standby) {
      standby_state = STBY_STANDBY;
      pre_standby_target = target_temp;
      target_temp = STANDBY_TEMP;
      buzzer.play(SND_REFLOW_START);
      forceTele = true;
    } else if (standby_state == STBY_STANDBY && idle_ms >= t_off) {
      standby_state = STBY_SLEEP_OFF;
      stopHeating();
      buzzer.play(SND_COMPLETE);
      forceTele = true;
    }
  }

  if (is_heating) {
    float e = target_temp - temp_f;
    rate_f += 0.3f * ((temp_f - temp_prev) / dt - rate_f);
    PID_P = Kp * e;
    if (fabsf(e) < I_ZONE) { PID_I += Ki * e * dt; if (PID_I < 0) PID_I = 0; if (PID_I > 100) PID_I = 100; }
    else if (e < 0) PID_I = 0;
    PID_D = -Kd * rate_f;
    float u = PID_P + PID_I + PID_D;
    output_power = u < 0 ? 0 : (u > 100 ? 100 : u);

    if (output_power >= 90.0f) {
      if (!nr_active) { nr_active = true; nr_start = now; nr_temp = temp_f; }
      else if (temp_f - nr_temp >= NO_RISE_DT) { nr_start = now; nr_temp = temp_f; }
      else if (now - nr_start > NO_RISE_MS) tripFault(F_NO_RISE);
    } else nr_active = false;
  } else {
    output_power = 0; PID_P = 0; PID_D = 0; nr_active = false;
  }
}

void updateSSR(uint32_t now) {
  if (now - window_start >= WINDOW_MS) window_start = now;
  bool on = is_heating && fault == F_NONE && temp_valid &&
            (output_power * WINDOW_MS / 100.0f > (float)(now - window_start));
  ssr_on = on;
  digitalWrite(PIN_SSR, (on && sensor != SN_SIM) ? HIGH : LOW);
}

void resetActivityTimer() {
  last_activity_time = millis();
  
  // Если экран спал — зажигаем его обратно!
  if (oled_sleeping && oled_ok) {
    display.ssd1306_command(SSD1306_DISPLAYON);
    oled_sleeping = false;
  }

  if (standby_state == STBY_STANDBY) {
    standby_state = STBY_ACTIVE;
    target_temp = pre_standby_target;
    buzzer.play(SND_CLICK);
    forceTele = true;
  }
}

void pollInputs(uint32_t now) {
  noInterrupts(); int d = enc_delta; enc_delta = 0; interrupts();
  if (d) { 
    if (profile.is_active) stopHeating();
    setTarget(target_temp + d * 5); 
    strlcpy(profile.name, "Manual", sizeof(profile.name));
    buzzer.play(SND_CLICK);
    forceTele = true; 
  }

  static bool last = HIGH, stable = HIGH; static uint32_t t0 = 0;
  bool r = digitalRead(PIN_ENC_SW);
  if (r != last) { last = r; t0 = now; }
  if (now - t0 > 40 && r != stable) {
    stable = r;
    if (stable == LOW) {
      buzzer.play(SND_CLICK);
      if (fault != F_NONE) fault = F_NONE;
      else if (is_heating) stopHeating();
      else startHeating();
      forceTele = true;
    }
  }

  // --- Опрос кнопок Back и Confirm через PCF8574 (каждые 60 мс) ---
  static uint32_t pcf_poll_timer = 0;
  static bool last_btn_back = HIGH, last_btn_conf = HIGH;

  if (pcf_found && now - pcf_poll_timer > 60) {
    pcf_poll_timer = now;
    uint8_t pcf_data = pcfReadByte();

    bool btn_back = (pcf_data & (1 << PCF_PIN_BTN_BACK)) ? HIGH : LOW;
    bool btn_conf = (pcf_data & (1 << PCF_PIN_BTN_CONF)) ? HIGH : LOW;

    // Нажатие кнопки BACK (P2)
    if (btn_back == LOW && last_btn_back == HIGH) {
      resetActivityTimer();
      buzzer.play(SND_CLICK);
      if (fault != F_NONE) {
        fault = F_NONE; // Сброс аварии
      } else if (is_heating) {
        stopHeating();
        fan_manual = true; // Выключили ТЭН и включили вентилятор обдува!
      } else {
        fan_manual = !fan_manual; // Ручной тумблер обдува
      }
      forceTele = true;
    }
    last_btn_back = btn_back;

    // Нажатие кнопки CONFIRM (P3): смена профилей из файла profiles.json по кругу
    if (btn_conf == LOW && last_btn_conf == HIGH) {
      resetActivityTimer();
      buzzer.play(SND_CLICK);
      if (profile.is_active) stopHeating();

      static uint8_t preset_cycle = 0;
      preset_cycle = (preset_cycle + 1) % 4;
      setTarget(profile.list[preset_cycle].reflow);
      strlcpy(profile.name, profile.list[preset_cycle].name, sizeof(profile.name));
      forceTele = true;
    }
    last_btn_conf = btn_conf;
  }
}

bool i2cPresent(uint8_t a) { Wire.beginTransmission(a); return Wire.endTransmission() == 0; }

bool initOled() {
  const uint8_t addrs[2] = {0x3C, 0x3D};
  for (uint8_t i = 0; i < 2; i++) {
    if (!i2cPresent(addrs[i])) continue;
    if (display.begin(SSD1306_SWITCHCAPVCC, addrs[i], true, false)) {
      display.clearDisplay();
      display.setTextColor(SSD1306_WHITE);
      display.display();
      return true;
    }
  }
  return false;
}

void drawOled() {
  if (!oled_ok) return;
  display.clearDisplay();

  // 1. Верхняя строка: Сеть слева, значок Mute справа
  display.setTextSize(1);
  display.setCursor(2, 2);
  if (WiFi.status() == WL_CONNECTED) display.print(WiFi.localIP());
  else { display.print("AP "); display.print(WiFi.softAPIP()); }
  
  if (buzzer.mute) {
    display.setCursor(110, 2);
    display.print("[M]");
  }

  // Тонкая разделительная линия сверху
  display.drawFastHLine(0, 11, 128, SSD1306_WHITE);

  // 2. Вторая строка: Статус / Профиль (ЦЕНТРИРУЕМ)
  char status_str[24];
  if (fault != F_NONE) {
    snprintf(status_str, sizeof(status_str), "FAULT: %s", FAULT_NAMES[fault]);
  } else if (tune_state == TUNE_RUNNING) {
    snprintf(status_str, sizeof(status_str), "TUNE: Cyc %d/3", tune_cycles);
  } else if (standby_state == STBY_STANDBY) {
    snprintf(status_str, sizeof(status_str), "ZZZ [STANDBY 100C]");
  } else {
    if (profile.is_active) snprintf(status_str, sizeof(status_str), "%s: %s", profile.name, PHASE_NAMES[profile.current_phase]);
    else snprintf(status_str, sizeof(status_str), "%s [%s]", is_heating ? "HEAT" : "IDLE", profile.name);
  }
  
  // Авто-центровка текста: (128 - длина_в_пикселях) / 2
  int16_t status_x = (128 - (strlen(status_str) * 6)) / 2;
  if (status_x < 0) status_x = 0;
  display.setCursor(status_x, 14);
  display.print(status_str);

  // 3. Главная цифра: ТЕМПЕРАТУРА (ОГРОМНЫЙ ШРИФТ, СТРОГО ПО ЦЕНТРУ!)
  char temp_str[16];
  if (temp_valid) {
    snprintf(temp_str, sizeof(temp_str), "%.1f%cC", temp_f, (char)247);
  } else {
    snprintf(temp_str, sizeof(temp_str), "NO SENSOR");
  }

  display.setTextSize(2);
  // Длина символа при размере 2 = 12 пикселей
  int16_t temp_x = (128 - (strlen(temp_str) * 12)) / 2;
  if (temp_x < 0) temp_x = 0;
  display.setCursor(temp_x, 26);
  display.print(temp_str);

  // Тонкая разделительная линия снизу
  display.drawFastHLine(0, 44, 128, SSD1306_WHITE);

  // 4. Подвал (Строка 1): Уставка слева, Мощность справа
  display.setTextSize(1);
  display.setCursor(4, 46);
  display.printf("Set:%d%cC", (int)target_temp, (char)247);

  display.setCursor(76, 46);
  display.printf("Pwr:%d%%", (int)output_power);

  // 5. Подвал (Строка 2): Реле, Вентилятор и SIM-режим
  display.setCursor(4, 56);
  display.print(ssr_on ? "SSR: ON" : "SSR: off");

  if (fan_active) {
    display.setCursor(64, 56);
    display.print("[FAN]");
  }
  if (sensor == SN_SIM) {
    display.setCursor(96, 56);
    display.print("[SIM]");
  }

  display.display();
}

void sendConfig(AsyncWebSocketClient *c) {
  DynamicJsonDocument d(1024);
  d["type"] = "config";
  d["ssid"] = wifi_ssid;
  d["mq_en"] = mqtt_enabled; d["mq_srv"] = mqtt_server; d["mq_prt"] = mqtt_port;
  d["mq_top"] = mqtt_topic;  d["mq_usr"] = mqtt_user;
  d["sensor"] = CFG_NAMES[cfg_sensor];
  d["kp"] = Kp; d["ki"] = Ki; d["kd"] = Kd;
  d["mute"] = buzzer.mute;
  d["stby_m"] = standby_timeout_min;
  d["tot_m"] = lifetime_heat_min;
  d["r_cyc"] = reflow_cycles_count;
  String out;
  serializeJson(d, out);
  c->text(out);
}

const char* mqStatus() {
  return mqtt_enabled ? (mqttClient.connected() ? "CONNECTED" : "CONNECTING") : "DISABLED";
}

void sendTelemetry() {
  if (ws.count() == 0) return;
  char buf[384];
  bool w_ok = (WiFi.status() == WL_CONNECTED);
  
  // Пакет 1: Быстрая телеметрия управления (Температура, ПИД, Фазы)
  snprintf(buf, sizeof(buf),
    "{\"type\":\"t\",\"t\":%.2f,\"g\":%.0f,\"p\":%.0f,\"h\":%d,\"s\":%d,\"f\":%d,\"ft\":\"%s\","
    "\"sn\":\"%s\",\"ok\":%d,\"w\":\"%s\",\"mq\":\"%s\",\"oled\":%d,"
    "\"wifi_ok\":%d,\"ssid\":\"%s\",\"ip\":\"%s\",\"prof\":\"%s\",\"phase\":\"%s\",\"stby\":%d,\"fan\":%d,\"tune\":%d,\"tune_c\":%d}",
    temp_valid ? temp_f : 0.0f, target_temp, output_power, is_heating ? 1 : 0, ssr_on ? 1 : 0,
    fault, FAULT_NAMES[fault], SENSOR_NAMES[sensor], temp_valid ? 1 : 0,
    temp_valid ? "" : SS_NAMES[sensor_st], mqStatus(), oled_ok ? 1 : 0,
    w_ok ? 1 : 0, w_ok ? WiFi.SSID().c_str() : "", w_ok ? WiFi.localIP().toString().c_str() : "",
    profile.name, PHASE_NAMES[profile.current_phase],
    (standby_state == STBY_STANDBY) ? 1 : 0,
    fan_active ? 1 : 0,
    (tune_state == TUNE_RUNNING) ? 1 : 0,
    tune_cycles);
  ws.textAll(buf);

  // Пакет 2: Статистика и счетчики (Компактный легкий JSON)
  char sbuf[192];
  snprintf(sbuf, sizeof(sbuf),
    "{\"type\":\"stat\",\"wh\":%.2f,\"s_sec\":%u,\"r_cyc\":%u,\"tot_m\":%u}",
    session_wh, session_heat_sec, reflow_cycles_count, lifetime_heat_min);
  ws.textAll(sbuf);
}

void performWifiScan(AsyncWebSocketClient *c) {
  int n = WiFi.scanNetworks();
  DynamicJsonDocument doc(1536);
  doc["type"] = "scan";
  JsonArray arr = doc.createNestedArray("list");
  for (int i = 0; i < n; ++i) {
    JsonObject obj = arr.createNestedObject();
    obj["s"] = WiFi.SSID(i);
    int q = 2 * (WiFi.RSSI(i) + 100);
    obj["r"] = constrain(q, 0, 100);
  }
  String out;
  serializeJson(doc, out);
  c->text(out);
}

void handleCommand(AsyncWebSocketClient *c, JsonDocument &d) {
  if (web_pin[0] && strcmp(d["pin"] | "", web_pin) != 0) { c->text("{\"type\":\"denied\"}"); return; }
  const char *a = d["a"] | "";
  resetActivityTimer(); // Команда из Web будит столик!

 if (!strcmp(a, "set_stby")) {
    standby_timeout_min = d["m"] | 10;
    cfg_save = true;
  } else if (!strcmp(a, "reset_stats")) {
    session_wh = 0.0f;
    session_heat_sec = 0;
    if (d["all"] | false) {
      lifetime_heat_min = 0;
      reflow_cycles_count = 0;
      cfg_save = true; // Записываем обнуление в флеш
    }
    buzzer.play(SND_CLICK);
    forceTele = true;
  } else if (!strcmp(a, "tune_start")) {
    startAutoTune();
  } else if (!strcmp(a, "fan")) {
    fan_manual = d["on"] | false;
    buzzer.play(SND_CLICK);
    forceTele = true;
  } else if (!strcmp(a, "heat")) {
    if (d["on"] | false) { profile.stop(); startHeating(); }
    else stopHeating();
  } else if (!strcmp(a, "mute")) {
    buzzer.mute = d["v"] | false;
    cfg_save = true;
    if (!buzzer.mute) buzzer.play(SND_CLICK);
  } else if (!strcmp(a, "start_prof_idx")) {
    uint8_t idx = d["idx"] | 0;
    if (fault == F_NONE && temp_valid && idx < 4) {
      profile.startByIndex(idx);
      startHeating();
    }
  } else if (!strcmp(a, "save_prof")) {
    uint8_t idx = d["idx"] | 0;
    if (idx < 4) {
      strlcpy(profile.list[idx].name, d["name"] | "Prof", sizeof(profile.list[idx].name));
      strlcpy(profile.list[idx].desc, d["desc"] | "", sizeof(profile.list[idx].desc));
      profile.list[idx].preheat  = d["pre"] | 150;
      profile.list[idx].soak     = d["soak"] | 165;
      profile.list[idx].soak_t   = d["soak_t"] | 60;
      profile.list[idx].reflow   = d["ref"] | 215;
      profile.list[idx].reflow_t = d["ref_t"] | 25;
      saveProfiles();
      buzzer.play(SND_CLICK);
      forceTele = true;
    }
  } else if (!strcmp(a, "target")) {
    if (profile.is_active) stopHeating();
    setTarget(d["v"] | target_temp);
    if (d.containsKey("name")) strlcpy(profile.name, d["name"] | "Manual", sizeof(profile.name));
    else strlcpy(profile.name, "Manual", sizeof(profile.name));
    buzzer.play(SND_CLICK);
  } else if (!strcmp(a, "ack")) {
    fault = F_NONE;
    buzzer.play(SND_CLICK);
  } else if (!strcmp(a, "scan")) {
    performWifiScan(c);
  } else if (!strcmp(a, "sensor")) {
    const char *v = d["v"] | "auto";
    for (uint8_t i = 0; i < 4; i++) if (!strcmp(v, CFG_NAMES[i])) cfg_sensor = i;
    stopHeating(); fault = F_NONE;
    apply_sensor = true; cfg_save = true;
  } else if (!strcmp(a, "pid")) {
    float kp = d["kp"] | Kp, ki = d["ki"] | Ki, kd = d["kd"] | Kd;
    if (okf(kp) && okf(ki) && okf(kd) &&
        kp >= 0 && kp <= 1000 && ki >= 0 && ki <= 100 && kd >= 0 && kd <= 1000) {
      Kp = kp; Ki = ki; Kd = kd; PID_I = 0; cfg_save = true;
    }
  } else if (!strcmp(a, "net")) {
    stopHeating();
    strlcpy(wifi_ssid, d["ssid"] | "", sizeof(wifi_ssid));
    setSecret(wifi_pass, sizeof(wifi_pass), d["pass"] | "");
    setSecret(web_pin, sizeof(web_pin), d["newpin"] | "");
    cfg_save = true;
    restart_at = millis() + 1000;
  } else if (!strcmp(a, "mqtt")) {
    mqtt_enabled = d["en"] | false;
    strlcpy(mqtt_server, d["srv"] | "", sizeof(mqtt_server));
    mqtt_port = d["port"] | 1883;
    strlcpy(mqtt_topic, d["top"] | "home/reflow_plate", sizeof(mqtt_topic));
    strlcpy(mqtt_user, d["usr"] | "", sizeof(mqtt_user));
    setSecret(mqtt_pass, sizeof(mqtt_pass), d["pass"] | "");
    cfg_save = true; mqtt_reconf = true;
  }
  forceTele = true;
}

void onWsEvent(AsyncWebSocket *s, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) { sendConfig(client); forceTele = true; }
  else if (type == WS_EVT_DATA) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (!(info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)) return;
    DynamicJsonDocument d(1024);
    if (!deserializeJson(d, data, len)) handleCommand(client, d);
  }
}

void handleMQTT(uint32_t now) {
  if (mqtt_reconf) {
    mqtt_reconf = false;
    if (mqttClient.connected()) mqttClient.disconnect();
    mqttClient.setServer(mqtt_server, mqtt_port);
  }
  if (!mqtt_enabled || WiFi.status() != WL_CONNECTED) return;

  if (!mqttClient.connected()) {
    static uint32_t last_try = 0;
    if (now - last_try < 10000) return;
    last_try = now;
    digitalWrite(PIN_SSR, LOW);
    char cid[32], will[80];
    snprintf(cid, sizeof(cid), "ReflowPlate-%06x", (unsigned)ESP.getChipId());
    snprintf(will, sizeof(will), "%s/status", mqtt_topic);
    if (mqttClient.connect(cid, mqtt_user[0] ? mqtt_user : NULL, mqtt_pass[0] ? mqtt_pass : NULL, will, 0, true, "offline")) {
      mqttClient.publish(will, "online", true);
    }
    return;
  }

  mqttClient.loop();
  static uint32_t last_pub = 0;
  if (now - last_pub >= 1000) {
    last_pub = now;
    char buf[256];
    snprintf(buf, sizeof(buf),
      "{\"temperature\":%.1f,\"target\":%.0f,\"power\":%.0f,\"state\":\"%s\",\"fault\":\"%s\",\"sensor\":\"%s\",\"phase\":\"%s\"}",
      temp_valid ? temp_f : 0.0f, target_temp, output_power, is_heating ? "ON" : "OFF",
      FAULT_NAMES[fault], SENSOR_NAMES[sensor], PHASE_NAMES[profile.current_phase]);
    mqttClient.publish(mqtt_topic, buf);
  }
}

void setupWifi() {
  WiFi.persistent(false);
  WiFi.disconnect(true);
  delay(100);
  WiFi.hostname(HOST);

  bool connected = false;
  if (wifi_ssid[0] != 0) {
    Serial.printf("[NET] Connecting to: %s\n", wifi_ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifi_ssid, wifi_pass);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) { delay(300); Serial.print("."); }
    Serial.println();
    connected = (WiFi.status() == WL_CONNECTED);
  }

  if (connected) {
    WiFi.enableAP(false);
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    Serial.printf("[NET] SUCCESS! Connected. IP: %s (AP DISABLED)\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("[NET] Router connection failed! Starting AP mode...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);
    WiFi.enableAP(true);
    Serial.printf("[NET] AP SSID: %s (IP: 192.168.4.1)\n", AP_SSID);
  }
}


void setup() {
  pinMode(PIN_SSR, OUTPUT);
  digitalWrite(PIN_SSR, LOW);

  buzzer.init();

  Serial.begin(115200);
  Serial.println();
  loadConfig();
  loadProfiles(); // Загружаем profiles.json из LittleFS

  pinMode(PIN_MAX_CS, OUTPUT);  digitalWrite(PIN_MAX_CS, HIGH);
  pinMode(PIN_MAX_SCK, OUTPUT); digitalWrite(PIN_MAX_SCK, LOW);
  pinMode(PIN_MAX_SO, INPUT);

  pinMode(PIN_ENC_CLK, INPUT_PULLUP);
  pinMode(PIN_ENC_DT, INPUT_PULLUP);
  pinMode(PIN_ENC_SW, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_CLK), isrEncoder, FALLING);

  Wire.begin(D2, D1);
  Wire.setClock(400000);
  oled_ok = initOled();

  pcf_found = initPCF8574();
  Serial.printf("PCF8574 Expander: %s (Addr: 0x%02X)\n", pcf_found ? "found" : "not found", pcf_address);

  applySensor();
  setupWifi();

  espClient.setTimeout(2000);
  mqttClient.setSocketTimeout(2);
  mqttClient.setBufferSize(512);

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *r) {
    AsyncWebServerResponse *response = r->beginResponse_P(200, "text/html", index_html_gz, index_html_gz_len);
    response->addHeader("Content-Encoding", "gzip");
    r->send(response);
  });

  // OTA
  server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", Update.hasError() ? "FAIL" : "OK");
    restart_at = millis() + 1000;
  }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (!index) {
      stopHeating();
      Update.runAsync(true);
      uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & ~0xFFF;
      Update.begin(maxSketchSpace);
    }
    if (!Update.hasError()) Update.write(data, len);
    if (final) Update.end(true);
  });

  server.onNotFound([](AsyncWebServerRequest *r) { r->send(404, "text/plain", "Not found"); });
  server.begin();

  last_tick = window_start = millis();
  buzzer.play(SND_CLICK);
}

void loop() {
  uint32_t now = millis();
  ws.cleanupClients();

  buzzer.tick(now);

  if (cfg_save)     { cfg_save = false; saveConfig(); }
  if (apply_sensor) { apply_sensor = false; applySensor(); }
  if (restart_at && now > restart_at) { digitalWrite(PIN_SSR, LOW); ESP.restart(); }

  pollInputs(now);

  if (now - last_tick >= SAMPLE_MS) {
    controlTick(now);
    if (!oled_ok && now - oled_probe > 10000) { oled_probe = now; oled_ok = initOled(); }

    // Авто-гашение экрана через 15 минут бездействия (когда ТЭН выключен и остыл)
    if (!is_heating && !oled_sleeping && (now - last_activity_time > 15UL * 60UL * 1000UL)) {
      if (oled_ok) {
        display.ssd1306_command(SSD1306_DISPLAYOFF); // Физически гасим матрицу дисплея
        oled_sleeping = true;
      }
    }

    // Рисуем на экране только если он не спит
    if (!oled_sleeping) {
      drawOled();
    }
  }
  updateSSR(now);

  // Интегратор энергопотребления (каждые 1000 мс)
  if (now - last_energy_calc >= 1000) {
    last_energy_calc = now;
    if (is_heating && fault == F_NONE) {
      session_heat_sec++;
      // Энергия (Wh) = Мощность(W) * (Power%/100) * (1 сек / 3600 сек в часе)
      float current_watts = HEATER_RATED_WATTS * (output_power / 100.0f);
      session_wh += current_watts / 3600.0f;

      // Каждые 60 секунд работы ТЭНа увеличиваем общий счетчик моточасов
      if (session_heat_sec % 60 == 0) {
        lifetime_heat_min++;
        // Сохраняем в память каждые 30 минут работы
        if (lifetime_heat_min % 30 == 0) cfg_save = true;
      }
    }
  }

  static uint32_t last_tele = 0;
  if (forceTele || now - last_tele >= 1000) { forceTele = false; last_tele = now; sendTelemetry(); }

  static bool mdns_up = false;
  if (!mdns_up && WiFi.status() == WL_CONNECTED) {
    mdns_up = MDNS.begin(HOST);
    if (mdns_up) MDNS.addService("http", "tcp", 80);
  }
  if (mdns_up) MDNS.update();

  handleMQTT(now);
}