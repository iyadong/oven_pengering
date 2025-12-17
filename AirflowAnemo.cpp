#include "AirflowAnemo.h"

static uint8_t  _pin = 2;
static float    _K = 0.30f;            // m/s per Hz
static float    _B = 0.00f;            // offset m/s
static unsigned long _minAcceptUs = 5000;
static unsigned long _sampleMs    = 500;

static volatile unsigned long _count = 0;
static volatile unsigned long _lastUs = 0;

static unsigned long _lastSampleMs = 0;
static unsigned long _lastCountSnap = 0;

static float _hz  = 0.0f;
static float _ms  = 0.0f;
static float _rpm = 0.0f;

static void isrAnemo() {
  unsigned long now = micros();
  unsigned long dt  = now - _lastUs;
  if (dt >= _minAcceptUs) {
    _count++;
  }
  _lastUs = now;
}

void Anemo_begin(uint8_t pin, bool usePullup, int edge) {
  _pin = pin;
  if (usePullup) pinMode(_pin, INPUT_PULLUP);
  else           pinMode(_pin, INPUT);
  attachInterrupt(digitalPinToInterrupt(_pin), isrAnemo, edge);

  _lastSampleMs  = millis();
  _lastCountSnap = _count;
}

void Anemo_setK(float k_ms_per_hz) { _K = k_ms_per_hz; }
void Anemo_setB(float b_ms)         { _B = b_ms; }
void Anemo_setMinPulseUs(unsigned long us) { _minAcceptUs = us; }
void Anemo_setSampleMs(unsigned long ms)   { _sampleMs = ms; }

void Anemo_update() {
  unsigned long now  = millis();
  unsigned long dtMs = now - _lastSampleMs;
  if (dtMs < _sampleMs) return;

  noInterrupts();
  unsigned long curCount = _count;
  interrupts();

  unsigned long dC = curCount - _lastCountSnap;
  _hz  = (dtMs > 0) ? (float)dC * 1000.0f / (float)dtMs : 0.0f;
  _ms  = _K * _hz + _B;
  if (_ms < 0) _ms = 0;
  _rpm = _hz * 60.0f;

  _lastCountSnap = curCount;
  _lastSampleMs  = now;
}

float Anemo_hz()     { return _hz; }
float Anemo_ms()     { return _ms; }
float Anemo_rpm()    { return _rpm; }
unsigned long Anemo_pulses() { return _count; }
