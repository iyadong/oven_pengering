#pragma once
#include <Arduino.h>

// Inisialisasi port RS485 + alamat & register yang akan dibaca
// startReg default 0x0001, qty default 2 (temp, RH)
void  xymd02_begin(uint8_t rxPin, uint8_t txPin, uint32_t baud,
                   uint8_t addr, uint16_t startReg = 0x0001, uint16_t qty = 2);

// Poll sesuai konfigurasi begin(); return true kalau data valid.
// Mengisi tempC (°C, 0.1 resolusi) dan rh (%RH, 0.1 resolusi)
bool  xymd02_poll(float &tempC, float &rh);

// Opsi: baca sekali untuk alamat tertentu (pakai reg default 0x0001, qty=2)
bool  xymd02_read(uint8_t addr, float &tempC, float &rh);

// Status pembacaan terakhir
bool  xymd02_ok();
