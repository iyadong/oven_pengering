# Oven Pengering — Kontrol 2 Heater + Kipas + Mist Maker + Sensor Berat + Sensor Suhu/Kelembapan

Firmware mikrokontroler untuk **oven pengering** dengan konfigurasi:
- **2 heater** (atas/bawah atau kiri/kanan)
- **1 kipas** sirkulasi
- **1 mist maker / pembuat kabut** (opsional, untuk kontrol kelembapan)
- Sensor:
  - **Suhu oven** (thermocouple via modul MAX6675)
  - **Suhu & kelembapan** di dalam oven (sensor via RS485)
  - **Berat** (load cell + amplifier, dibaca oleh modul `WeightSensor`)
  - (Opsional) **airflow / anemometer** untuk memantau aliran udara kipas

> Entry point program ada di: `MAIN.ino`  
> Modul yang tersedia di repo:
- `AirflowAnemo.cpp/.h`
- `ThermoMAX6675.cpp/.h`
- `WeightSensor.cpp/.h`
- `XYMD02_RS485.cpp/.h`

---

## Fitur Utama
- Monitoring suhu oven (thermocouple MAX6675)
- Monitoring suhu & kelembapan (RS485)
- Monitoring berat (load cell)
- Kontrol aktuator:
  - Heater 1 & Heater 2 (relay/SSR)
  - Kipas (relay/PWM tergantung rangkaian)
  - Mist maker (relay/SSR)
- Mode kontrol sederhana (ON/OFF hysteresis) atau bisa dikembangkan ke PID (opsional)

---

## Hardware yang Dibutuhkan
### Sensor
- Thermocouple K-type + modul **MAX6675**
- Sensor suhu & kelembapan **RS485** (contoh keluarga XY-MD/XYMD series)
- Load cell + amplifier (mis. HX711 atau rangkaian sejenis, sesuai implementasi kamu)
- (Opsional) sensor airflow/anemometer

### Aktuator
- 2x heater (AC/DC sesuai oven)
- 1x kipas (AC/DC)
- 1x mist maker / humidifier (AC/DC)
- Relay/SSR untuk beban AC (disarankan SSR untuk heater AC)

### Komunikasi
- RS485 transceiver (mis. MAX485) jika sensor RH/T memakai RS485

---

## Wiring (Sesuaikan dengan Pin di `MAIN.ino`)
> Karena tiap build beda, **jadikan ini sebagai template**. Pastikan cocok dengan definisi pin di `MAIN.ino`.

### Output (Aktuator)
| Aktuator | Tipe kontrol | Catatan |
|---|---|---|
| Heater 1 | Relay/SSR | Elemen pemanas 1 |
| Heater 2 | Relay/SSR | Elemen pemanas 2 |
| Kipas | Relay atau PWM | Jika PWM, pastikan driver sesuai |
| Mist maker | Relay/SSR | Opsional (untuk kontrol RH) |

### Input (Sensor)
| Sensor | Interface | File modul |
|---|---|---|
| Thermocouple | SPI-like (MAX6675) | `ThermoMAX6675.*` |
| Suhu & RH | RS485 | `XYMD02_RS485.*` |
| Berat | ADC/amplifier | `WeightSensor.*` |
| Airflow | Pulse/ADC | `AirflowAnemo.*` |

---

## Instalasi & Build
### Opsi A — Arduino IDE
1. Install Arduino IDE
2. Buka `MAIN.ino`
3. Pilih board & COM port
4. Upload

### Opsi B — PlatformIO (opsional)
1. Buat project PlatformIO
2. Masukkan file `.ino/.cpp/.h`
3. Set `platformio.ini` sesuai board
4. Build & upload

---

## Konfigurasi Kontrol (yang biasanya perlu kamu edit)
Cari dan ubah parameter di `MAIN.ino` (atau jadikan section config sendiri), misalnya:
- `targetTempC` (setpoint suhu)
- `tempHysteresisC` (hysteresis ON/OFF)
- `targetRH` (setpoint kelembapan, kalau pakai mist maker)
- `rhHysteresis` (hysteresis kelembapan)
- `fanMode` / `fanSpeed`
- `weightCalFactor` (kalibrasi load cell)
- `weightStopThreshold` (berat target/stop condition)

---

## Alur Kerja Sistem (Contoh)
1. Baca sensor suhu (MAX6675)
2. Baca sensor suhu & RH (RS485)
3. Baca berat (load cell)
4. Kontrol heater:
   - Jika suhu < setpoint - hysteresis → heater ON
   - Jika suhu > setpoint + hysteresis → heater OFF
   - (Opsional) bagi duty antara Heater1 & Heater2
5. Kontrol mist maker (opsional):
   - Jika RH < setpoint - hysteresis → mist ON
   - Jika RH > setpoint + hysteresis → mist OFF
6. Kipas:
   - ON terus, atau mengikuti mode tertentu (mis. ON saat heater ON)
7. Stop condition (opsional):
   - Berhenti saat berat stabil / mencapai target pengeringan

---

## Kalibrasi Berat (Load Cell)
1. Pastikan load cell terpasang stabil (tidak goyang)
2. Tare (nolkan) saat kosong
3. Taruh beban referensi (mis. 100g / 500g)
4. Sesuaikan `weightCalFactor` sampai pembacaan sesuai
5. Simpan nilai faktor kalibrasi

---

## Troubleshooting
- **Suhu 0 / NaN**: cek wiring MAX6675 + thermocouple.
- **Data RS485 tidak terbaca**: cek A/B terbalik, baudrate, alamat sensor, dan transceiver.
- **Berat ngaco**: cek grounding, supply amplifier, dan lakukan kalibrasi + averaging.
- **Heater nyala terus**: cek logika relay (active LOW/HIGH) dan hysteresis.

---

## Catatan Keselamatan
- Tegangan AC berbahaya. Gunakan **SSR/relay yang sesuai**, sekering, grounding, dan box panel.
- Pisahkan jalur AC dan jalur sinyal sensor.
- Gunakan kabel tahan panas untuk bagian dalam oven.

---

## Lisensi
Belum ditentukan. Tambahkan `LICENSE` jika ingin open-source (MIT/GPL/Apache).
