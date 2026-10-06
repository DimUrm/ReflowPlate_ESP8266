#pragma once
#include <Arduino.h>
#include "config.h"

enum SoundAlert { SND_NONE, SND_CLICK, SND_REFLOW_START, SND_COMPLETE, SND_FAULT, SND_COOLED };

class BuzzerEngine {
public:
  bool mute = false;

  void init() {
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);
  }

  void play(SoundAlert alert) {
    if (mute && alert != SND_FAULT) return;
    current_sound = alert;
    sound_step = 0;
    sound_step_time = millis();
  }

  void tick(uint32_t now) {
    if (current_sound == SND_NONE) return;
    uint32_t dt = now - sound_step_time;

    switch (current_sound) {
      case SND_CLICK:
        if (sound_step == 0) { tone(PIN_BUZZER, 2400); sound_step++; sound_step_time = now; }
        else if (dt > 25) { noTone(PIN_BUZZER); current_sound = SND_NONE; }
        break;

      case SND_REFLOW_START:
        if (sound_step == 0) { tone(PIN_BUZZER, 2200); sound_step++; sound_step_time = now; }
        else if (sound_step == 1 && dt > 120) { noTone(PIN_BUZZER); sound_step++; sound_step_time = now; }
        else if (sound_step == 2 && dt > 80)  { tone(PIN_BUZZER, 2600); sound_step++; sound_step_time = now; }
        else if (sound_step == 3 && dt > 150) { noTone(PIN_BUZZER); current_sound = SND_NONE; }
        break;

      case SND_COMPLETE:
        if (sound_step == 0) { tone(PIN_BUZZER, 1800); sound_step++; sound_step_time = now; }
        else if (sound_step == 1 && dt > 120) { tone(PIN_BUZZER, 2200); sound_step++; sound_step_time = now; }
        else if (sound_step == 2 && dt > 120) { tone(PIN_BUZZER, 2700); sound_step++; sound_step_time = now; }
        else if (sound_step == 3 && dt > 300) { noTone(PIN_BUZZER); current_sound = SND_NONE; }
        break;

      case SND_COOLED:
        if (sound_step == 0) { tone(PIN_BUZZER, 1500); sound_step++; sound_step_time = now; }
        else if (sound_step == 1 && dt > 200) { noTone(PIN_BUZZER); current_sound = SND_NONE; }
        break;

      case SND_FAULT:
        if (sound_step % 2 == 0) {
          tone(PIN_BUZZER, 1200);
          if (dt > 250) { noTone(PIN_BUZZER); sound_step++; sound_step_time = now; }
        } else {
          if (dt > 150) {
            sound_step++; sound_step_time = now;
            if (sound_step >= 6) current_sound = SND_NONE;
          }
        }
        break;

      default:
        noTone(PIN_BUZZER);
        current_sound = SND_NONE;
        break;
    }
  }

private:
  SoundAlert current_sound = SND_NONE;
  uint32_t sound_step_time = 0;
  uint8_t sound_step = 0;
};

extern BuzzerEngine buzzer;