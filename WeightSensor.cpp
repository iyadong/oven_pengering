#include "WeightSensor.h"
#include <HX711.h>   // install "HX711" by bogde (Library Manager)
#include <math.h>

// ===== State internal =====
static HX711 hx;
static bool   inited = false;

static float    g_scale           = 2280.0f;   // faktor kalibrasi (satuan bebas; default ~gram)
static uint16_t g_sampleInterval  = 120;       // ms (throttle polling)
static uint8_t  g_avgSamples      = 2;         // get_units(N)

// --- Smoothing / filtering ---
static float g_alphaManual = 0.20f;  // EMA manual 0..1
static bool  g_useEMA      = true;   // aktifkan EMA manual?
static float g_tauSec      = 0.0f;   // EMA berbasis konstanta waktu; 0 = OFF (prioritas > EMA manual)

static uint8_t g_medianN   = 1;      // 1..9, ganjil; 1 = OFF
static float   g_spikeLimit = 0.0f;   // 0 = OFF (dalam units)

// buffer median (maks 9)
static float mBuf[9];
static uint8_t mCount = 0;
static uint8_t mIndex = 0;

static float  lastUnitsFilt = 0.0f;  // nilai terakhir (filtered)
static float  lastUnitsRaw  = 0.0f;  // nilai terakhir (raw)
static unsigned long lastUpdate  = 0;
static unsigned long lastAttempt = 0;

// --- utils kecil ---
static uint8_t makeOddInRange(uint8_t n, uint8_t lo, uint8_t hi) {
  if (n < lo) n = lo;
  if (n > hi) n = hi;
  if ((n & 1u) == 0) n++;  // jadikan ganjil
  if (n > hi) n -= 2;      // jaga agar tetap dalam rentang & ganjil
  return n;
}

static float medianCurrent() {
  uint8_t n = mCount;
  if (n == 0) return NAN;
  float tmp[9];
  for (uint8_t i = 0; i < n; ++i) tmp[i] = mBuf[i];
  // insertion sort (n sangat kecil)
  for (uint8_t i = 1; i < n; ++i) {
    float key = tmp[i];
    int8_t j = i - 1;
    while (j >= 0 && tmp[j] > key) { tmp[j + 1] = tmp[j]; j--; }
    tmp[j + 1] = key;
  }
  return tmp[n / 2]; // n ganjil -> median elemen tengah
}

void Weight_begin(uint8_t doutPin, uint8_t sckPin, float scale, bool doTare, uint16_t tareSamples) {
  g_scale = scale;
  hx.begin(doutPin, sckPin);
  hx.set_scale(g_scale);
  if (doTare) {
    hx.tare(tareSamples);
  }
  inited = true;

  // reset state
  lastUnitsFilt = 0.0f;
  lastUnitsRaw  = 0.0f;
  lastUpdate    = 0;
  lastAttempt   = 0;

  mCount = 0;
  mIndex = 0;
}

bool Weight_poll(float &unitsOut) {
  if (!inited) return false;

  unsigned long now = millis();

  // Throttle pembacaan supaya stabil (dan hemat CPU)
  if (now - lastAttempt < g_sampleInterval) return false;
  lastAttempt = now;

  if (!hx.is_ready()) return false;  // non-blocking, tunggu siklus berikutnya

  // Ambil rata-rata N sampel (HX711 blocking singkat, tapi cepat)
  float v = hx.get_units(g_avgSamples);  // satuan sama dengan "scale"
  if (isnan(v) || !isfinite(v)) return false;

  lastUnitsRaw = v;

  // Masukkan ke buffer median (jika aktif)
  float vMed = v;
  if (g_medianN > 1) {
    if (mCount < g_medianN) {
      mBuf[mCount++] = v;
    } else {
      mBuf[mIndex] = v;
      mIndex = (uint8_t)((mIndex + 1) % g_medianN);
    }
    vMed = medianCurrent();
  }

  // Spike clamp (sebelum EMA)
  float vLimited = vMed;
  if (g_spikeLimit > 0.0f && lastUpdate != 0) {
    float delta = vMed - lastUnitsFilt;
    if (delta >  g_spikeLimit) vLimited = lastUnitsFilt + g_spikeLimit;
    if (delta < -g_spikeLimit) vLimited = lastUnitsFilt - g_spikeLimit;
  }

  // Hitung alpha: prioritas tau > EMA manual; kalau keduanya OFF, alpha=1 (passthrough)
  float alpha = 1.0f; // default: tanpa smoothing
  bool doSmooth = false;

  if (g_tauSec > 0.0f) {
    float dt = (lastUpdate == 0)
                 ? (g_sampleInterval / 1000.0f)
                 : ((now - lastUpdate) / 1000.0f);
    if (dt < 0.0001f) dt = g_sampleInterval / 1000.0f;
    alpha = dt / (g_tauSec + dt);
    // jaga batas numerik
    if (alpha < 0.0001f) alpha = 0.0001f;
    if (alpha > 1.0f)    alpha = 1.0f;
    doSmooth = true;
  } else if (g_useEMA && g_alphaManual > 0.0f && g_alphaManual < 1.0f) {
    alpha = g_alphaManual;
    doSmooth = true;
  }

  if (lastUpdate == 0) {
    // sample pertama
    lastUnitsFilt = vLimited;
  } else if (doSmooth) {
    lastUnitsFilt = alpha * vLimited + (1.0f - alpha) * lastUnitsFilt;
  } else {
    lastUnitsFilt = vLimited;
  }

  lastUpdate = now;
  unitsOut   = lastUnitsFilt;
  return true;
}

// ===== Opsional helpers =====
void Weight_tare(uint16_t samples) {
  if (!inited) return;
  hx.tare(samples);
}

void Weight_setScale(float scale) {
  g_scale = scale;
  if (inited) hx.set_scale(g_scale);
}

float Weight_getScale() {
  return g_scale;
}

void Weight_setSampleInterval(uint16_t ms) {
  if (ms < 10) ms = 10; // hindari terlalu cepat
  g_sampleInterval = ms;
}

void Weight_setAverageSamples(uint8_t n) {
  if (n == 0) n = 1;
  if (n > 16) n = 16; // batas aman
  g_avgSamples = n;
}

void Weight_setSmoothing(float alpha) {
  // alpha di luar (0,1) -> matikan EMA manual
  if (alpha <= 0.0f || alpha >= 1.0f) {
    g_useEMA = false;
    g_alphaManual = 0.0f;
  } else {
    g_useEMA = true;
    g_alphaManual = alpha;
  }
}

void Weight_setTimeConstant(float tau_seconds) {
  if (tau_seconds <= 0.0f) {
    g_tauSec = 0.0f; // OFF
  } else {
    g_tauSec = tau_seconds;
  }
}

void Weight_setMedianWindow(uint8_t n) {
  n = makeOddInRange(n, 1, 9);
  g_medianN = n;
  // reset buffer supaya transisi halus
  mCount = 0;
  mIndex = 0;
}

void Weight_setSpikeLimit(float limit_units) {
  if (limit_units < 0.0f) limit_units = 0.0f;
  g_spikeLimit = limit_units;
}

// ===== Info =====
float Weight_lastKg() {
  return lastUnitsFilt; // "kg" di sini = "units" sesuai scale
}

unsigned long Weight_lastUpdateMs() {
  return lastUpdate;
}

bool Weight_isReady() {
  return inited && hx.is_ready();
}
