#pragma once
#include <Arduino.h>
#include "buzzer.h"

enum ReflowPhase { PHASE_IDLE, PHASE_PREHEAT, PHASE_SOAK, PHASE_REFLOW, PHASE_COOLDOWN };
const char* const PHASE_NAMES[] = {"IDLE", "PREHEAT", "SOAK", "REFLOW", "COOLDOWN"};

// Структура одного профиля пайки
struct ReflowProfileItem {
  char name[16];     // Имя профиля (например, "Sn63")
  char desc[32];     // Краткое описание
  float preheat;     // Температура преднагрева (°C)
  float soak;        // Температура выдержки флюса (°C)
  uint32_t soak_t;   // Время выдержки (сек)
  float reflow;      // Пик оплавления (°C)
  uint32_t reflow_t; // Время пика (сек)
};

class ProfileEngine {
public:
  ReflowPhase current_phase = PHASE_IDLE;
  bool is_active = false;
  char name[24] = "Manual";

  // Массив из 4 профилей в оперативной памяти
  ReflowProfileItem list[4];

  float active_preheat = 150.0f;
  float active_soak    = 165.0f;
  uint32_t active_soak_t = 60000;
  float active_reflow  = 215.0f;
  uint32_t active_reflow_t = 25000;

  void start(const char* prof_name, float pre, float soak, uint32_t s_time, float ref, uint32_t r_time) {
    strlcpy(name, prof_name, sizeof(name));
    active_preheat  = pre;
    active_soak     = soak;
    active_soak_t   = s_time;
    active_reflow   = ref;
    active_reflow_t = r_time;

    is_active = true;
    current_phase = PHASE_PREHEAT;
    reflow_reached = false;
    phase_start = millis();
    buzzer.play(SND_CLICK);
  }

  void startByIndex(uint8_t idx) {
    if (idx >= 4) return;
    start(list[idx].name, list[idx].preheat, list[idx].soak, list[idx].soak_t * 1000UL, list[idx].reflow, list[idx].reflow_t * 1000UL);
  }

  void stop() {
    is_active = false;
    current_phase = PHASE_IDLE;
  }

  void tick(uint32_t now, float current_temp, float &target_out, bool &heater_enable, bool &fan_request) {
    if (!is_active) return;
    uint32_t elapsed = now - phase_start;

    switch (current_phase) {
      case PHASE_IDLE:
        break;

      case PHASE_PREHEAT:
        target_out = active_preheat;
        heater_enable = true;
        if (current_temp >= active_preheat - 2.0f) {
          current_phase = PHASE_SOAK;
          phase_start = now;
          buzzer.play(SND_CLICK);
        }
        break;

      case PHASE_SOAK:
        target_out = active_soak;
        heater_enable = true;
        if (elapsed >= active_soak_t) {
          current_phase = PHASE_REFLOW;
          phase_start = now;
          reflow_reached = false;
          buzzer.play(SND_REFLOW_START);
        }
        break;

      case PHASE_REFLOW:
        target_out = active_reflow;
        heater_enable = true;
        if (!reflow_reached) {
          if (current_temp >= active_reflow - 4.0f) {
            reflow_reached = true;
            phase_start = now;
          }
        } else {
          if (elapsed >= active_reflow_t) {
            current_phase = PHASE_COOLDOWN;
            phase_start = now;
            heater_enable = false;
            buzzer.play(SND_COMPLETE);
          }
        }
        break;

      case PHASE_COOLDOWN:
        heater_enable = false;
        fan_request = true; // Включаем обдув!
        target_out = 0;
        if (current_temp <= 45.0f) {
          current_phase = PHASE_IDLE;
          is_active = false;
          fan_request = false;
          buzzer.play(SND_COOLED);
        }
        break;
    }
  }

private:
  uint32_t phase_start = 0;
  bool reflow_reached = false;
};

extern ProfileEngine profile;