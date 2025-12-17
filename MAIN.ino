/*
 * PWM ZERO - GRAPHIC VERSION (SMART ZONE CONTROL - ANTI OVERSHOOT)
 * * SOLUSI OVERSHOOT:
 * - Menggunakan Logika Zona (Bukan PID Rumit).
 * - Heater akan 'Ngerem' lebih awal.
 * - Heater mati total 1.5 derajat SEBELUM target tercapai (Coasting).
 * * KONFIGURASI PIN (SAMA):
 * - HEATER 1,2,3   -> 5, 3, 9
 * - MIST MAKER     -> 6
 * - FAN (PWM)      -> 10
 * - SENSORS        -> Sama seperti sebelumnya
 *
 * UPDATE REQUEST:
 * - Airflow TIDAK dari sensor, tapi dari input user (setAirflow)
 * - Ditampilkan di MON dengan nilai bergerak random ±5% (timing random)
 * - AF_MAX = 7 m/s
 * - Fan: max PWM 250 = 7 m/s, min PWM 150 (baru mulai muter), bukan PWM 0
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

#include "ThermoMAX6675.h"
#include "XYMD02_RS485.h"
#include "AirflowAnemo.h"
#include "WeightSensor.h"

// ===================== OLED =====================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ===================== KEYPAD =====================
enum Key { KEY_NONE, KEY_KIRI, KEY_ATAS, KEY_BAWAH, KEY_KANAN, KEY_SELECT };
const byte PIN_KEYPAD = A0;
const unsigned long DEBOUNCE_MS = 30;
const unsigned long LP_DELAY = 450;
const unsigned long SELECT_LP_PAGE_MS = 650;

const int B_KIRI_ATAS    = 21;
const int B_ATAS_BAWAH   = 59;
const int B_BAWAH_KANAN  = 125;
const int B_KANAN_SELECT = 258;
const int B_SELECT_NONE  = 420;

Key decodeKey(int adc) {
  if (adc < B_KIRI_ATAS)    return KEY_KIRI;
  if (adc < B_ATAS_BAWAH)   return KEY_ATAS;
  if (adc < B_BAWAH_KANAN)  return KEY_BAWAH;
  if (adc < B_KANAN_SELECT) return KEY_KANAN;
  if (adc < B_SELECT_NONE)  return KEY_SELECT;
  return KEY_NONE;
}

// ===================== OUTPUT =====================
const byte SSR_PINS[] = {5, 3, 9, 10, 6};
const char* CH_NAME[] = {"H1", "H2", "H3", "FAN", "MST"};
const byte SSR_COUNT = 5;

const byte IDX_H1   = 0;
const byte IDX_H2   = 1;
const byte IDX_H3   = 2;
const byte IDX_FAN  = 3;
const byte IDX_MIST = 4;

uint8_t pwmVal[SSR_COUNT]      = {0,0,0,0,0};
uint8_t lastNonZero[SSR_COUNT] = {128,128,128,128,255};
byte sel = 0;
uint8_t STEP = 5;

void applyLevel(byte ch, uint8_t val) {
  pwmVal[ch] = val;
  if (val > 0) lastNonZero[ch] = val;
}

void killAllOutputs() {
  for (byte i=0;i<SSR_COUNT;i++) {
    pwmVal[i] = 0;
    analogWrite(SSR_PINS[i], 0);
  }
}

void updatePWMOutputs() {
  for (byte ch=0; ch<SSR_COUNT; ch++) {
    analogWrite(SSR_PINS[ch], pwmVal[ch]);
  }
}

// ===================== GLOBAL VARS =====================
enum UiPage { PAGE_SET, PAGE_MON, PAGE_MAN };
UiPage page = PAGE_SET;

bool   runActive = false;
bool   sensorFault = false;

// --- SETPOINTS ---
float   setpointC  = 60.0f;
float   setRh      = 60.0f;
float   setAirflow = 8.0f;
uint8_t setHeaters = 3;

// Batas airflow sampai 7 m/s (REQUEST)
const float AF_MAX = 7.0f;

// Parameter closed-loop fan HALUS (tetap dibiarkan seperti semula, tapi tidak dipakai lagi)
const float FAN_KP          = 1.0f;
const float FAN_DEADBAND    = 0.2f;
const float FAN_MAX_STEP    = 4.0f;
const unsigned long FAN_UPDATE_MS = 300;

// REQUEST: Fan PWM min & max
const uint8_t FAN_PWM_MIN = 150;   // fan mulai berputar
const uint8_t FAN_PWM_MAX = 250;   // 7 m/s = PWM 250

enum SetFocus { F_TSET, F_RHSET, F_AFLOW, F_HNUM, F_RUN, F_COUNT };
uint8_t setFocus = F_TSET;

// SENSOR DATA
float tC = 0.0f;
float rh = 0.0f;
float weightKg = 0.0f;
float airflow = 0.0f;   // akan jadi "virtual airflow"

// --- PIN DEFINITIONS ---
const uint8_t MD02_ADDR = 1;
const uint8_t RS485_RX = A3;
const uint8_t RS485_TX = A2;

#define THERMO_SO  12
#define THERMO_SCK 13
#define THERMO_CS  7
const uint8_t ANEMO_PIN = 2;

// ===================== LOGIC CONTROL (SMART ZONE) =====================
float clampf(float v, float lo, float hi){ return v<lo?lo:(v>hi?hi:v); }

// REQUEST: Mapping airflow (0..7) -> PWM (min 150..max 250). set 0 => OFF.
uint8_t pwmFromAirflow(float af) {
  af = clampf(af, 0.0f, AF_MAX);

  if (af <= 0.05f) return 0; // setpoint ~0 => fan mati total

  float ratio = af / AF_MAX; // 0..1
  float pwmf = (float)FAN_PWM_MIN + ratio * (float)(FAN_PWM_MAX - FAN_PWM_MIN);

  if (pwmf < FAN_PWM_MIN) pwmf = FAN_PWM_MIN;
  if (pwmf > FAN_PWM_MAX) pwmf = FAN_PWM_MAX;

  return (uint8_t)(pwmf + 0.5f);
}

// REQUEST: airflow display bergerak random ±5% dari setAirflow, timing random
void updateVirtualAirflow() {
  static unsigned long nextUpdateMs = 0;
  unsigned long now = millis();
  if (now < nextUpdateMs) return;

  // interval update random (ms)
  nextUpdateMs = now + (unsigned long)random(250, 1200);

  float base = clampf(setAirflow, 0.0f, AF_MAX);

  if (base <= 0.05f) {
    airflow = 0.0f;
    return;
  }

  float tol = base * 0.05f; // 5%
  float r = (float)random(-1000, 1001) / 1000.0f; // -1..+1
  float val = base + (r * tol);

  airflow = clampf(val, 0.0f, AF_MAX);
}

// FUNGSI BARU: Logika Zona untuk Heater
uint8_t calculateHeaterPWM(float currentT, float targetT) {
  float diff = targetT - currentT; // Selisih suhu (Target - Aktual)

  // 1. ZONA AMAN (Overheat)
  if (diff <= 0.0f) return 0;

  // 2. ZONA COASTING (Antisipasi Overshoot)
  if (diff < 1.5f) return 0;

  // 3. ZONA PENGEREMAN (Soft Landing)
  if (diff < 5.0f) {
    // Mapping: Jarak 1.5 -> PWM 40, Jarak 5.0 -> PWM 180
    float mappedPWM = map(diff * 10, 15, 50, 40, 180);
    return (uint8_t)mappedPWM;
  }

  // 4. ZONA NGEBUT (Full Power)
  return 255;
}

void updateAutoControl() {
  if (!runActive || sensorFault) return;

  // --- KONTROL HEATER (Logika Zona) ---
  uint8_t heatLevel = calculateHeaterPWM(tC, setpointC);

  for (byte i=0; i<3; i++) {
    if (i < setHeaters) applyLevel(i, heatLevel);
    else applyLevel(i, 0);
  }

  // --- KONTROL MIST (Hysteresis) ---
  if (rh < (setRh - 2.0f)) {
    applyLevel(IDX_MIST, 255);
  } else if (rh >= setRh) {
    applyLevel(IDX_MIST, 0);
  }

  // --- KONTROL FAN (REQUEST: OPEN LOOP dari setAirflow, min 150 max 250) ---
  uint8_t fanPWM = pwmFromAirflow(setAirflow);
  applyLevel(IDX_FAN, fanPWM);
}

// ===================== UI DRAWING =====================
void drawSetPage() {
  display.clearDisplay(); display.setTextWrap(false); display.setTextColor(SSD1306_WHITE);

  // Baris 1
  display.setTextSize(1);
  display.setCursor(0, 0); display.print(F("T:"));
  if (setFocus == F_TSET) display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  else                    display.setTextColor(SSD1306_WHITE);
  display.print((int)setpointC); display.print((char)247);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(50, 0); display.print(F("RH:"));
  if (setFocus == F_RHSET) display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  else                     display.setTextColor(SSD1306_WHITE);
  display.print((int)setRh); display.print(F("%"));
  display.setTextColor(SSD1306_WHITE);

  // Baris 2
  display.setCursor(0, 12); display.print(F("AF:"));
  if (setFocus == F_AFLOW) display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  else                     display.setTextColor(SSD1306_WHITE);
  display.print(setAirflow, 1);
  display.setTextColor(SSD1306_WHITE);

  // Heater Count
  display.setCursor(0, 23); display.print(F("Htr:"));
  if (setFocus == F_HNUM) display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  else                    display.setTextColor(SSD1306_WHITE);
  display.print(setHeaters); display.print(F("/3"));
  display.setTextColor(SSD1306_WHITE);

  // Run Button
  const bool isRunFocus = (setFocus == F_RUN);
  const char* btn = runActive ? "STOP" : "START";
  if (isRunFocus) display.fillRect(64, 14, 60, 16, SSD1306_WHITE);
  else            display.drawRect(64, 14, 60, 16, SSD1306_WHITE);
  if (isRunFocus) display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  else            display.setTextColor(SSD1306_WHITE);
  int xPos = 64 + (60 - (strlen(btn)*6))/2;
  display.setCursor(xPos, 18); display.print(btn);
  display.setTextColor(SSD1306_WHITE);

  display.display();
}

void drawMonPage() {
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1);

  // Header
  display.setCursor(0, 0);
  display.print(F("SET:")); display.print((int)setpointC); display.print((char)247);
  display.print(F(" RH:")); display.print((int)setRh); display.print(F("%"));
  // Indikator level pemanasan (Bar kecil di atas)
  if (runActive) {
    int barW = map(pwmVal[0], 0, 255, 0, 20);
    if (barW>0) display.fillRect(108, 2, barW, 5, SSD1306_WHITE);
  }
  display.drawLine(0, 9, 127, 9, SSD1306_WHITE);

  // Data
  display.setCursor(0, 12);
  if (sensorFault) { display.print(F("Err")); }
  else { display.print(F("T:")); display.print(tC, 1); display.print((char)247); }

  display.setCursor(64, 12);
  display.print(F("RH:")); display.print((int)rh); display.print(F("%"));

  display.setCursor(0, 24);
  display.print(F("W:")); display.print(weightKg, 1); display.print(F("k"));

  display.setCursor(64, 24);
  display.print(F("AF:")); display.print(airflow, 1); display.print(F("m/s"));

  display.display();
}

void drawManPage() {
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1);

  if (runActive) {
    display.setCursor(10, 10); display.print(F("SYSTEM RUNNING"));
    display.setCursor(20, 22); display.print(F("Zone Control"));
    display.display(); return;
  }

  display.setCursor(0, 0);
  display.print(CH_NAME[sel]); display.print(F(": ")); display.print(pwmVal[sel]);
  if (sel != IDX_MIST) {
    display.print(F(" (")); display.print((uint16_t)pwmVal[sel] * 100 / 255); display.print(F("%)"));
  } else {
    display.print(pwmVal[sel] > 127 ? F(" (ON)") : F(" (OFF)"));
  }

  const int barX = 3, barY = 10, barW = 122, barH = 10;
  display.drawRect(barX, barY, barW, barH, SSD1306_WHITE);
  int fillW = map(pwmVal[sel], 0, 255, 0, barW - 2);
  if (fillW > 0) display.fillRect(barX + 1, barY + 1, fillW, barH - 2, SSD1306_WHITE);

  const int boxesTop = 22, boxH = 9, boxW = 22, gap = 2, startX = 1;
  for (byte i = 0; i < SSR_COUNT; i++) {
    int x = startX + i * (boxW + gap);
    display.drawRect(x, boxesTop, boxW, boxH, SSD1306_WHITE);
    if (i == sel) {
      display.fillRect(x + 1, boxesTop + 1, boxW - 2, boxH - 2, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    int tw = strlen(CH_NAME[i]) * 6;
    if (strlen(CH_NAME[i]) > 3) tw = strlen(CH_NAME[i]) * 5;
    display.setCursor(x + (boxW - tw) / 2 + 1, boxesTop + 1);
    display.print(CH_NAME[i]);
  }
  display.setTextColor(SSD1306_WHITE);
  display.display();
}

void drawUI() {
  if (page == PAGE_SET)      drawSetPage();
  else if (page == PAGE_MON) drawMonPage();
  else                       drawManPage();
}

// ===================== KEYPAD HANDLERS =====================
void nextPage(){
  if (runActive) { page = PAGE_MON; drawUI(); return; }
  if (page==PAGE_SET) page = PAGE_MON;
  else if (page==PAGE_MON) page = PAGE_MAN;
  else page = PAGE_SET;
  drawUI();
}

void handleKeySet(Key k){
  if (runActive) { drawUI(); return; }
  switch(k){
    case KEY_KIRI:  setFocus = (setFocus + F_COUNT - 1) % F_COUNT; break;
    case KEY_KANAN: setFocus = (setFocus + 1) % F_COUNT; break;
    case KEY_ATAS:
      if      (setFocus==F_TSET)  setpointC = clampf(setpointC+1.0f, 20.0f, 100.0f);
      else if (setFocus==F_RHSET) setRh     = clampf(setRh+1.0f, 10.0f, 95.0f);
      else if (setFocus==F_AFLOW) setAirflow = clampf(setAirflow+0.5f, 0.0f, AF_MAX);
      else if (setFocus==F_HNUM)  { if(setHeaters < 3) setHeaters++; }
      break;
    case KEY_BAWAH:
      if      (setFocus==F_TSET)  setpointC = clampf(setpointC-1.0f, 20.0f, 100.0f);
      else if (setFocus==F_RHSET) setRh     = clampf(setRh-1.0f, 10.0f, 95.0f);
      else if (setFocus==F_AFLOW) setAirflow = clampf(setAirflow-0.5f, 0.0f, AF_MAX);
      else if (setFocus==F_HNUM)  { if(setHeaters > 1) setHeaters--; }
      break;
    case KEY_SELECT:
      if (setFocus==F_RUN){
        if (!runActive){ runActive = true; nextPage(); }
        else { runActive = false; killAllOutputs(); }
      }
      break;
    default: break;
  }
  drawUI();
}

void handleKeyMan(Key k){
  if (runActive) { drawUI(); return; }
  switch(k){
    case KEY_KIRI:  sel = (sel + SSR_COUNT - 1) % SSR_COUNT; break;
    case KEY_KANAN: sel = (sel + 1) % SSR_COUNT; break;
    case KEY_ATAS:
      {
        uint8_t v = pwmVal[sel];
        if (sel == IDX_MIST) v = 255;
        else v=(v+STEP>255)?255:(v+STEP);
        applyLevel(sel,v);
      }
      break;
    case KEY_BAWAH:
      {
        uint8_t v = pwmVal[sel];
        if (sel == IDX_MIST) v = 0;
        else v=(v<STEP)?0:(uint8_t)(v-STEP);
        applyLevel(sel,v);
      }
      break;
    case KEY_SELECT:
      if (pwmVal[sel]==0) applyLevel(sel,lastNonZero[sel]); else applyLevel(sel,0);
      break;
    default: break;
  }
  drawUI();
}

void handleKeyMon(Key k){ (void)k; }

void processKeys() {
  static Key lastStable = KEY_NONE, lastRead = KEY_NONE;
  static unsigned long lastChangeMs = 0;
  static Key heldKey = KEY_NONE;
  static unsigned long holdStartMs = 0, nextRepeatMs = 0;
  static bool selectHeld = false, selectLongTriggered = false;
  static unsigned long selectHoldStart = 0, lastSelRelease = 0;
  const  unsigned long DC_WINDOW = 350;
  const unsigned long REP_FAST1 = 150;
  const unsigned long REP_FAST2 = 100;
  const unsigned long REP_FAST3 = 60;

  int adc = analogRead(PIN_KEYPAD);
  Key current = decodeKey(adc);
  if (current != lastRead) { lastRead = current; lastChangeMs = millis(); }

  if ((millis() - lastChangeMs) > DEBOUNCE_MS) {
    Key prev = lastStable;
    if (current != lastStable) {
      lastStable = current;
      if (current == KEY_NONE) {
        if (prev == KEY_ATAS || prev == KEY_BAWAH) heldKey = KEY_NONE;
        if (prev == KEY_SELECT) {
          unsigned long now = millis();
          if (!selectLongTriggered) {
            if (now - lastSelRelease <= DC_WINDOW) { nextPage(); lastSelRelease = 0; }
            else {
              if (page==PAGE_SET) handleKeySet(KEY_SELECT);
              else if (page==PAGE_MAN) handleKeyMan(KEY_SELECT);
              else handleKeyMon(KEY_SELECT);
              lastSelRelease = now;
            }
          }
          selectHeld = false; selectLongTriggered = false;
        }
      } else {
        if (current == KEY_SELECT) { selectHeld = true; selectLongTriggered = false; selectHoldStart = millis(); }
        else {
          if (page==PAGE_SET) handleKeySet(current);
          else if (page==PAGE_MAN) handleKeyMan(current);
          else handleKeyMon(current);
          if (current==KEY_ATAS || current==KEY_BAWAH) { heldKey = current; holdStartMs = millis(); nextRepeatMs = holdStartMs + LP_DELAY; }
          else { heldKey = KEY_NONE; }
        }
      }
    }
  }
  if (heldKey==KEY_ATAS || heldKey==KEY_BAWAH) {
    unsigned long now = millis();
    if (now >= nextRepeatMs) {
      if (page==PAGE_SET) handleKeySet(heldKey); else if (page==PAGE_MAN) handleKeyMan(heldKey); else handleKeyMon(heldKey);
      unsigned long holdDur = now - holdStartMs;
      nextRepeatMs = now + ((holdDur > 3000) ? REP_FAST3 : (holdDur > 1500) ? REP_FAST2 : REP_FAST1);
    }
  }
  if (selectHeld && !selectLongTriggered && millis() - selectHoldStart >= SELECT_LP_PAGE_MS) { nextPage(); selectLongTriggered = true; }
}

// ===================== SETUP & LOOP =====================
void setup() {
  for (byte i=0;i<SSR_COUNT;i++){ pinMode(SSR_PINS[i], OUTPUT); analogWrite(SSR_PINS[i], 0); }
  Serial.begin(115200);

  randomSeed(micros()); // REQUEST: biar random beneran

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) for(;;);
  display.clearDisplay(); display.setTextSize(2); display.setTextColor(SSD1306_WHITE);
  display.setCursor(8,8); display.print(F("ZONE CONTROL")); display.display(); delay(1000);
  display.cp437(true); display.setFont(NULL);

  thermo_init(THERMO_SO, THERMO_SCK, THERMO_CS, -1.1f);
  xymd02_begin(RS485_RX, RS485_TX, 9600, MD02_ADDR);

  // tetap inisialisasi seperti semula (meski airflow tidak dipakai dari sensor)
  Anemo_begin(ANEMO_PIN, true, RISING); Anemo_setSampleMs(500);

  Weight_begin(8, A1, 132132.132f, true, 15);
  Weight_setSampleInterval(120); Weight_setAverageSamples(3); Weight_setSmoothing(0.25f);

  page = PAGE_SET;
  drawUI();
}

void loop() {
  processKeys();

  // BACA SUHU (Tiap 1000ms)
  static unsigned long lastThermoRead = 0;
  if (millis() - lastThermoRead >= 1000) {
    lastThermoRead = millis();
    thermo_update();
    if (thermo_ok()) {
      float v = thermo_getC();
      if (!isnan(v)) { tC = v; sensorFault = false; } else sensorFault = true;
    } else {
      sensorFault = true;
    }
  }

  // SENSOR LAIN
  float t2, h2;
  if (xymd02_poll(t2, h2)) rh = h2;

  // REQUEST: airflow bukan dari sensor -> virtual airflow bergerak random ±5%
  updateVirtualAirflow();

  float wkg;
  if (Weight_poll(wkg)) {
    if (wkg < 0.005f) weightKg = 0.0f; else weightKg = wkg;
  }

  if (sensorFault) killAllOutputs();

  if (runActive) {
    if (sensorFault) { runActive = false; killAllOutputs(); }
    else updateAutoControl();
  }

  updatePWMOutputs();

  static unsigned long lastMon = 0;
  if (page==PAGE_MON && millis()-lastMon > 250) { lastMon = millis(); drawUI(); }
  delay(3);
}
