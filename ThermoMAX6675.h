#pragma once
#include <Arduino.h>

void  thermo_init(uint8_t pinSO, uint8_t pinSCK, uint8_t pinCS, float offsetC = 0.0f);
void  thermo_update();         // panggil sering (mis. tiap loop)
float thermo_getC();           // hasil terakhir (°C), bisa NAN kalau fault/belum baca
bool  thermo_ok();             // true kalau bacaan terakhir valid
void  thermo_set_offset(float offsetC);  // koreksi kalibrasi (+/- °C)
