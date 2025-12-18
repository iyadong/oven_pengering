# Oven Pengering — ZONE CONTROL (Anti-Overshoot) + OLED UI
Firmware Arduino untuk oven pengering dengan kontrol:
- **Heater H1/H2/H3** (maks 3 channel, bisa pakai 2 heater dengan set `Htr: 2/3`)
- **Fan PWM**
- **Mist Maker**
- Sensor:
  - Suhu oven dari **MAX6675 (thermocouple K-type)**
  - RH (dan suhu opsional) via **RS485 XYMD02**
  - Berat via modul `WeightSensor`
- UI OLED + keypad analog (A0)

## Fitur Utama
- **ZONE CONTROL (Smart Zone / Anti Overshoot)**
  - “Ngerem” sebelum target, **heater OFF total saat jarak < 1.5°C** (coasting).
- Kontrol RH: **mist ON** bila RH < (setRH - 2), **OFF** bila RH >= setRH.
- Fan: **open-loop** dari setpoint airflow (`AF`) → mapping ke PWM (min 150, max 250).
- Monitor: tampilkan suhu, RH, berat, dan **airflow virtual** yang bergerak acak ±5%.

---

## Hardware
### Output (Aktuator)
- Heater H1, H2, H3 (via SSR/relay)
- Fan (PWM)
- Mist Maker (relay/SSR)

### Sensor
- MAX6675 + thermocouple K-type
- Sensor RH RS485 (XYMD02 / sejenis)
- Load cell (sesuai implementasi `WeightSensor`)
- (Opsional) modul anemometer tetap diinit, tapi **airflow tidak diambil dari sensor** (airflow “virtual” untuk tampilan).

### Display & Input
- OLED I2C 128x32 (alamat 0x3C)
- Keypad analog resistor ladder (pin A0)

---

## Pinout (sesuai `MAIN.ino`)
### Output channels (analogWrite)
Urutan channel mengikuti array `SSR_PINS[] = {5, 3, 9, 10, 6}`:
| Channel | Nama | Pin |
|---|---|---|
| 0 | H1 | D5 |
| 1 | H2 | D3 |
| 2 | H3 | D9 |
| 3 | FAN | D10 |
| 4 | MST | D6 |

> Catatan: mist dikontrol 0/255 (OFF/ON). Heater dan fan pakai PWM.

### Sensor & komunikasi
- **RS485**: RX = A3, TX = A2, baud 9600, address = 1
- **MAX6675**: SO = D12, SCK = D13, CS = D7
- **Anemometer pin**: D2 (diinit, tapi airflow display berasal dari input user)
- **WeightSensor**: `Weight_begin(8, A1, 132132.132f, true, 15)`
  - Menggunakan pin **D8** dan **A1** (fungsi persisnya mengikuti library `WeightSensor` kamu)

### OLED I2C
- SDA/SCL mengikuti board (Arduino UNO: SDA=A4, SCL=A5)

### Keypad
- Input analog: **A0**
- Decode tombol berdasar batas ADC (resistor ladder):
  - KIRI, ATAS, BAWAH, KANAN, SELECT

---

## Cara Pakai (UI)
Ada 3 halaman:
1. **SET**: setpoint & start/stop
2. **MON**: monitoring realtime
3. **MAN**: kontrol manual tiap channel (hanya saat tidak RUN)

### Navigasi tombol
- **KIRI/KANAN**: pindah fokus (di SET) / pindah channel (di MAN)
- **ATAS/BAWAH**: naik/turun nilai
- **SELECT**:
  - Di SET fokus `START/STOP` untuk menjalankan / menghentikan.
  - Di MAN untuk toggle ON/OFF channel terpilih.
- **Ganti halaman**:
  - **Double-click SELECT** atau **tahan SELECT** (long press) untuk next page.

---

## Setpoint & Batas
Default:
- `T setpoint` = 60°C
- `RH set` = 60%
- `AF set` = 0.0 .. **7.0 m/s** (AF_MAX = 7)
- `Htr` = 1..3 (jumlah heater aktif)

### Fan mapping (request)
- AF = 0 → fan OFF
- AF > 0 → PWM **min 150** (mulai muter) sampai PWM **max 250** (di 7 m/s)

### Airflow “virtual”
Nilai airflow di MON:
- Berasal dari `setAirflow` (user)
- Bergerak acak **±5%** dengan interval update acak ~250–1200 ms

---

## Logika Kontrol Otomatis
Otomatis hanya aktif saat `runActive = true` dan sensor tidak fault.

### Heater: Zone Control
`diff = targetT - currentT`
- diff <= 0.0 → PWM 0
- diff < 1.5 → PWM 0 (**coasting**)
- diff < 5.0 → PWM dipetakan **40..180**
- diff >= 5.0 → PWM 255 (**full power**)

### Mist (hysteresis)
- RH < (setRH - 2.0) → ON
- RH >= setRH → OFF

### Fan
- PWM = hasil mapping dari `setAirflow` (open-loop)

### Safety (sensor fault)
Jika suhu MAX6675 error / NaN:
- Semua output dimatikan (`killAllOutputs()`)
- RUN berhenti otomatis

---

## Build & Upload
### Arduino IDE
1. Install library:
   - `Adafruit_GFX`
   - `Adafruit_SSD1306`
2. Pastikan file custom ada di project:
   - `ThermoMAX6675.*`, `XYMD02_RS485.*`, `AirflowAnemo.*`, `WeightSensor.*`
3. Buka `MAIN.ino`
4. Pilih board (contoh: Arduino Uno / Nano sesuai pin PWM)
5. Upload

---

## Catatan Penting (Keselamatan)
⚠️ Heater & beban AC berbahaya:
- Gunakan **SSR/relay** yang sesuai arus/tegangan
- Gunakan **sekering**, grounding, dan wiring rapi
- Pisahkan jalur sinyal low-voltage dengan AC
- Disarankan tambah proteksi hardware: thermal fuse / thermostat safety

---

## Lisensi
Tentukan lisensi sesuai kebutuhan (MIT/GPL/Proprietary).
