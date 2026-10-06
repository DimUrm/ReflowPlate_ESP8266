#pragma once
#include <Arduino.h>

// ================= ПИНЫ =================
#define PIN_SSR       D8   // GPIO15 (Твердотельное реле 220V)
#define PIN_BUZZER    3    // GPIO3 / RX (ШИМ-зуммер с поддержкой tone)
#define PIN_ENC_CLK   D5   // GPIO14 (Энкодер CLK)
#define PIN_ENC_DT    D6   // GPIO12 (Энкодер DT)
#define PIN_ENC_SW    D7   // GPIO13 (Кнопка энкодера)

#define PIN_MAX_SO    D3   // GPIO0 (MAX6675 MISO)
#define PIN_MAX_CS    D4   // GPIO2 (MAX6675 CS)
#define PIN_MAX_SCK   D0   // GPIO16 (MAX6675 SCK)
// OLED I2C: SDA = D2 (GPIO4), SCL = D1 (GPIO5)

// ================= СИСТЕМНЫЕ КОНСТАНТЫ =================
static const char*    AP_SSID = "ReflowPlate";
static const char*    AP_PASS = "reflow1234";
static const char*    HOST    = "reflow";

static const float    HEATER_RATED_WATTS = 450.0f; // Паспортная мощность ТЭНа (450W)
static const float    LIMIT_TC      = 270.0f;
static const float    LIMIT_NTC     = 120.0f;
static const float    TARGET_MARGIN = 10.0f;
static const uint32_t SAMPLE_MS     = 250;
static const uint32_t WINDOW_MS     = 1000;
static const uint32_t HEAT_MAX_MS   = 60UL * 60UL * 1000UL;
static const uint32_t NO_RISE_MS    = 60000UL;
static const float    NO_RISE_DT    = 3.0f;
static const float    I_ZONE        = 10.0f;

static const float    NTC_R_FIXED   = 100000.0f, NTC_R0 = 100000.0f, NTC_BETA = 3950.0f;
static const float    NTC_VCC       = 3.3f, ADC_FULL_V = 3.2f;

static const float    SIM_AMB       = 25.0f;
static const float    SIM_PMAX      = 400.0f;
static const float    SIM_C         = 300.0f;
static const float    SIM_K         = 1.0f;
static const float    SIM_TAU_S     = 3.0f;

// ================= ПЕРЕЧИСЛЕНИЯ СТАТУСОВ =================
enum SensorType { SN_MAX, SN_NTC, SN_SIM };
enum SensorState { SS_OK, SS_ABSENT, SS_OPEN, SS_RANGE };
enum FaultCode { F_NONE, F_NO_SENSOR, F_SENSOR_OPEN, F_SENSOR_RANGE, F_OVERTEMP, F_NO_RISE, F_TIMEOUT };

const char* const SENSOR_NAMES[] = {"MAX6675", "NTC", "SIM"};
const char* const SS_NAMES[]     = {"OK", "ABSENT", "OPEN/SHORT", "RANGE"};
const char* const FAULT_NAMES[]  = {"OK", "SENSOR ABSENT", "SENSOR OPEN", "SENSOR RANGE", "OVERTEMP", "NO HEATING", "TIMEOUT"};

enum CfgSensor { CFG_AUTO, CFG_MAX, CFG_NTC, CFG_SIM };
const char* const CFG_NAMES[]    = {"auto", "max", "ntc", "sim"};

// ================= НАСТРОЙКИ СНА И ОЖИДАНИЯ =================
static const float STANDBY_TEMP = 100.0f; // Дежурная температура в режиме ожидания (°C)

enum StandbyMode {
  STBY_ACTIVE,   // Обычный рабочий нагрев
  STBY_STANDBY,  // Сброс до 100°C из-за бездействия
  STBY_SLEEP_OFF // Полное отключение нагрева (Auto-OFF)
  };

// ================= РАСШИРИТЕЛЬ ПОРТОВ PCF8574 (I2C) =================
#define PCF8574_ADDR_1   0x20  // Базовый адрес PCF8574 (A0=A1=A2=GND)
#define PCF8574_ADDR_2   0x27  // Альтернативный адрес
#define PCF8574A_ADDR    0x38  // Адрес для версии PCF8574A

// ================= СТАТУС AUTOTUNE =================
enum TuneState { TUNE_OFF, TUNE_RUNNING, TUNE_DONE, TUNE_FAILED };

// Номера пинов на гребенке PCF8574 (P0 - P7)
#define PCF_PIN_FAN      0     // P0 -> Мосфет вентилятора охлаждения
#define PCF_PIN_LIGHT    1     // P1 -> Реле вытяжки дыма или подсветка
#define PCF_PIN_BTN_BACK 2     // P2 -> Кнопка Back (Стоп / Обдув Fan / Сброс аварии)
#define PCF_PIN_BTN_CONF 3     // P3 -> Кнопка Confirm (Смена пресетов по кругу)