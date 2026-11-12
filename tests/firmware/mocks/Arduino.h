// Host-side stand-in for the Arduino core, just enough to run the firmware's
// logic on a PC. Time is simulated (simUs), pins are arrays, and the hardware
// behind them is modelled in tests/firmware/sim.cpp.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

typedef uint8_t byte;
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define A0 14
#define A1 15
#define A2 16
#define A3 17
#define FALLING 2
#define RISING 3
#define CHANGE 1
#define DEC 10
#define HEX 16
#define WDRF 3
#define _BV(b) (1 << (b))

// Program memory: on a PC everything is ordinary memory
#define PROGMEM
#define PSTR(x) (x)
#define PGM_P const char*
#define pgm_read_word(a) (*(a))
#define strcpy_P strcpy
#define strncpy_P strncpy
#define snprintf_P snprintf
class __FlashStringHelper;
#define F(s) (reinterpret_cast<const __FlashStringHelper*>(s))

// Simulated hardware state (defined in sim.cpp)
extern uint64_t simUs;
extern int pinState[32], pinOut[32], pwmOut[32], pinModeArr[32];
extern uint8_t MCUSR;
typedef void (*isr_t)();
extern isr_t simIsr[2];
extern int simIsrMode[2];

inline uint32_t millis() { return (uint32_t)(simUs / 1000); }
inline uint32_t micros() { return (uint32_t)simUs; }
inline void delay(uint32_t ms) { simUs += (uint64_t)ms * 1000; }
inline void delayMicroseconds(uint32_t us) { simUs += us; }
inline void pinMode(uint8_t p, uint8_t m) { pinModeArr[p] = m; }
inline void digitalWrite(uint8_t p, uint8_t v) { pinOut[p] = v; }
inline int digitalRead(uint8_t p) { return pinState[p]; }
inline void analogWrite(uint8_t p, int v) { pwmOut[p] = v; }
int analogRead(uint8_t pin);
uint32_t pulseIn(uint8_t pin, uint8_t level, uint32_t timeout);
inline int digitalPinToInterrupt(int p) { return p == 2 ? 0 : (p == 3 ? 1 : -1); }
inline void attachInterrupt(int n, isr_t f, int m) { simIsr[n] = f; simIsrMode[n] = m; }
inline void noInterrupts() {}
inline void interrupts() {}
inline char* dtostrf(double v, signed char w, unsigned char p, char* b) { sprintf(b, "%*.*f", w, p, v); return b; }
inline char* itoa(int v, char* b, int) { sprintf(b, "%d", v); return b; }
inline char* ultoa(unsigned long v, char* b, int) { sprintf(b, "%lu", v); return b; }

class Print {
 public:
  virtual ~Print() {}
  virtual size_t write(uint8_t c) = 0;
  size_t write(const uint8_t* b, size_t n) { for (size_t i = 0; i < n; i++) write(b[i]); return n; }
  void ps(const std::string& s) { for (char c : s) write((uint8_t)c); }
  void print(const char* s) { ps(s); }
  void print(const __FlashStringHelper* s) { ps((const char*)s); }
  void print(char c) { write((uint8_t)c); }
  void print(int v) { ps(std::to_string(v)); }
  void print(unsigned int v) { ps(std::to_string(v)); }
  void print(long v) { ps(std::to_string(v)); }
  void print(unsigned long v) { ps(std::to_string(v)); }
  void print(uint8_t v, int base) { char b[8]; sprintf(b, base == 16 ? "%X" : "%u", v); ps(b); }
  void print(double v, int d = 2) { char b[32]; if (std::isnan(v)) strcpy(b, "nan"); else sprintf(b, "%.*f", d, v); ps(b); }
  template <typename T> void println(T v) { print(v); ps("\r\n"); }
  template <typename T> void println(T v, int d) { print(v, d); ps("\r\n"); }
  void println() { ps("\r\n"); }
};

class SerialMock : public Print {
 public:
  void begin(long) {}
  int available();
  int read();
  using Print::write;
  size_t write(uint8_t c) override;
  int availableForWrite() { return 63; }
};
extern SerialMock Serial;

void setup();
void loop();
