#ifndef WEIGHTSENSOR_H
#define WEIGHTSENSOR_H

#include <Arduino.h>

// Default pin (ubah sesuai wiring kamu)
#ifndef WEIGHT_DOUT_DEFAULT
#define WEIGHT_DOUT_DEFAULT 12    // HX711 DOUT -> D12
#endif

#ifndef WEIGHT_SCK_DEFAULT
#define WEIGHT_SCK_DEFAULT  3     // HX711 SCK  -> D3
#endif

// API sederhana yang dipakai main
void  Weight_begin(uint8_t doutPin = WEIGHT_DOUT_DEFAULT,
                   uint8_t sckPin  = WEIGHT_SCK_DEFAULT,
                   float   scale   = 2280.0f,    // faktor scale; dengan 2280 ≈ gram
                   bool    doTare  = true,
                   uint16_t tareSamples = 15);

bool  Weight_poll(float &units);            // true jika ada bacaan baru (non-blocking)

// API tambahan (opsional)
void  Weight_tare(uint16_t samples = 15);
void  Weight_setScale(float scale);
float Weight_getScale();
void  Weight_setSampleInterval(uint16_t ms); // default 120 ms
void  Weight_setAverageSamples(uint8_t n);   // default 2

// Smoothing (dua cara, pilih salah satu):
// 1) Legacy EMA manual: alpha 0..1. alpha di luar (0,1) -> dinonaktifkan
void  Weight_setSmoothing(float alpha);

// 2) EMA berbasis konstanta waktu (detik). tau <= 0 -> dinonaktifkan
void  Weight_setTimeConstant(float tau_seconds);

// Filter tambahan
// Median window ganjil 1..9; 1 = OFF
void  Weight_setMedianWindow(uint8_t n);

// Batasi lompatan input per update (dalam "units"); 0 = OFF
void  Weight_setSpikeLimit(float limit_units);

// Info terakhir
float         Weight_lastKg();          // alias: nilai terakhir (filtered) dalam "units"
unsigned long Weight_lastUpdateMs();    // millis() terakhir update
bool          Weight_isReady();         // HX711 ready?

#endif // WEIGHTSENSOR_H
