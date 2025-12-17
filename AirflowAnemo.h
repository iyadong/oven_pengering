#pragma once
#include <Arduino.h>

// Inisialisasi anemometer di pin interrupt (default D2/INT0)
void Anemo_begin(uint8_t pin = 2, bool usePullup = true, int edge = RISING);

// Kalibrasi v = K * Hz + B
void Anemo_setK(float k_ms_per_hz);
void Anemo_setB(float b_ms);

// Filter pulsa & periode sampling
void Anemo_setMinPulseUs(unsigned long us);
void Anemo_setSampleMs(unsigned long ms);

// Panggil rutin (mis. tiap loop)
void Anemo_update();

// Hasil terakhir
float Anemo_hz();
float Anemo_ms();
float Anemo_rpm();
unsigned long Anemo_pulses();
