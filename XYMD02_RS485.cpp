#include "XYMD02_RS485.h"
#include <SoftwareSerial.h>

static SoftwareSerial* _ser = nullptr;
static uint8_t  _addr = 1;
static uint16_t _reg  = 0x0001;
static uint16_t _qty  = 2;
static bool     _ok   = false;

static uint16_t crc16_modbus(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (uint8_t b = 0; b < 8; ++b) {
      if (crc & 0x0001) crc = (crc >> 1) ^ 0xA001;
      else              crc = (crc >> 1);
    }
  }
  return crc; // kirim low-byte dulu
}

void xymd02_begin(uint8_t rxPin, uint8_t txPin, uint32_t baud,
                  uint8_t addr, uint16_t startReg, uint16_t qty) {
  if (_ser) { delete _ser; _ser = nullptr; }
  _ser = new SoftwareSerial(rxPin, txPin);  // (RX, TX)
  _ser->begin(baud);
  _addr = addr; _reg = startReg; _qty = qty;
  _ok = false;
}

static bool read_regs(uint8_t addr, uint16_t start, uint16_t qty, float &tempC, float &rh) {
  if (!_ser) return (_ok=false);

  uint8_t req[8];
  req[0]=addr; req[1]=0x04;             // Function: Read Input Registers
  req[2]=(start>>8)&0xFF; req[3]=start&0xFF;
  req[4]=(qty>>8)&0xFF;   req[5]=qty&0xFF;
  uint16_t c = crc16_modbus(req,6);
  req[6]= c & 0xFF; req[7]=(c>>8)&0xFF;

  while (_ser->available()) _ser->read(); // bersihkan buffer
  _ser->write(req, sizeof(req));
  _ser->flush();

  // expected: addr, 0x04, bytecount(=4), T_hi, T_lo, H_hi, H_lo, CRC_lo, CRC_hi
  uint8_t resp[9];
  size_t n=0;
  unsigned long t0=millis();
  const unsigned long TIMEOUT=300;
  while (millis()-t0 < TIMEOUT) {
    if (_ser->available()) {
      if (n<sizeof(resp)) resp[n++] = _ser->read();
      else _ser->read();
      if (n>=sizeof(resp)) break;
    }
  }
  if (n<sizeof(resp)) return (_ok=false);

  uint16_t rcrc = (uint16_t)resp[8]<<8 | resp[7];
  uint16_t calc = crc16_modbus(resp, 7);
  if (rcrc != calc) return (_ok=false);

  if (resp[0]!=addr || resp[1]!=0x04 || resp[2]!=4) return (_ok=false);

  int16_t  tRaw = (int16_t)((resp[3]<<8)|resp[4]);
  uint16_t hRaw = (uint16_t)((resp[5]<<8)|resp[6]);
  tempC = tRaw/10.0f;
  rh    = hRaw/10.0f;
  return (_ok=true);
}

bool xymd02_poll(float &tempC, float &rh) {
  return read_regs(_addr, _reg, _qty, tempC, rh);
}

bool xymd02_read(uint8_t addr, float &tempC, float &rh) {
  return read_regs(addr, 0x0001, 2, tempC, rh);
}

bool xymd02_ok() { return _ok; }
