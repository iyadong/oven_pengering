#include "ThermoMAX6675.h"
#include <max6675.h>   // Install via Library Manager: "MAX6675"

// Objek & state internal
static MAX6675* g_tc = nullptr;
static unsigned long lastMs = 0;
static const unsigned long SAMPLE_MS = 250;  // MAX6675 ~220ms, kita kasih margin
static float lastC   = NAN;
static bool  lastOk  = false;
static float offsetC = 0.0f;
static const float ALPHA = 0.2f;             // filter EMA (0..1), 0.2 = cukup halus

void thermo_init(uint8_t pinSO, uint8_t pinSCK, uint8_t pinCS, float offset) {
  offsetC = offset;
  if (g_tc) { delete g_tc; g_tc = nullptr; }
  // Konstruktor: MAX6675(sck, cs, so)
  g_tc = new MAX6675(pinSCK, pinCS, pinSO);

  // Tunggu konversi pertama
  delay(250);
  lastMs = 0;
  lastC  = NAN;
  lastOk = false;
}

void thermo_update() {
  if (!g_tc) return;
  unsigned long now = millis();
  if (now - lastMs < SAMPLE_MS) return;
  lastMs = now;

  double raw = g_tc->readCelsius();
  if (isnan(raw)) {
    lastOk = false;
    return;
  }

  float v = (float)raw + offsetC;
  if (isnan(lastC)) lastC = v;
  else              lastC = lastC + ALPHA * (v - lastC);
  lastOk = true;
}

float thermo_getC()              { return lastC; }
bool  thermo_ok()                { return lastOk; }
void  thermo_set_offset(float o) { offsetC = o; }
