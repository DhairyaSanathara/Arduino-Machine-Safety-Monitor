// sim.cpp - the simulated hardware behind the mocks (see sim.h).
#include "sim.h"

#include <random>

#include "app.h"   // the firmware's pins and state

World world;

// ------------------------------------------------------------ mock globals
uint64_t simUs = 0;
int pinState[32], pinOut[32], pwmOut[32], pinModeArr[32];
uint8_t MCUSR = 0;
isr_t simIsr[2] = { nullptr, nullptr };
int simIsrMode[2];
SerialMock Serial;
WireMock Wire;
char lcdScreen[2][17];

float worldTempC() { return world.tempC; }
bool worldDhtPresent() { return world.dhtPresent; }
int worldDhtModel() { return world.dhtModel; }

static std::string serialIn, partial;
static std::vector<std::string> lines;
static std::mt19937 rng(1);                         // fixed seed: every run is identical
static std::normal_distribution<float> noise(0, 3.0f);   // sensor noise, mg

// ------------------------------------------------------------ serial port
int SerialMock::available() { return (int)serialIn.size(); }
int SerialMock::read() {
  if (serialIn.empty()) return -1;
  char c = serialIn[0];
  serialIn.erase(0, 1);
  return c;
}
size_t SerialMock::write(uint8_t c) {
  if (c == '\r') return 1;
  if (c == '\n') { lines.push_back(partial); partial.clear(); }
  else partial += (char)c;
  return 1;
}

// ------------------------------------------------------------ motor and power
bool relayClosed() { return (pinOut[PIN_RELAY] == HIGH) != RELAY_ACTIVE_LOW; }
int motorPwm() { return pwmOut[PIN_MOTOR_EN]; }
static float duty() { return relayClosed() ? pwmOut[PIN_MOTOR_EN] / 255.0f : 0.0f; }

int analogRead(uint8_t) { return 1023; }            // no 12 V sense divider fitted

// ------------------------------------------------------------ HC-SR04
uint32_t pulseIn(uint8_t pin, uint8_t, uint32_t timeout) {
  if (pin != PIN_US_ECHO || world.distCm <= 0) return 0;   // unplugged: no pulse at all
  float us = world.distCm * 2 / 0.0343f;
  if (us > timeout) { simUs += timeout; return 0; }
  simUs += (uint64_t)us + 400;
  return (uint32_t)us;
}

// ------------------------------------------------------------ I2C devices
uint8_t WireMock::endTransmission(bool) {
  bool present = (addr == 0x68 && world.mpuPresent) || (addr == 0x27 && world.lcdPresent);
  if (!present) return 2;                          // NACK: nobody at that address
  if (nw >= 1) reg = wbuf[0];
  return 0;
}

static void put16(uint8_t* b, float counts) {
  int16_t v = (int16_t)counts;
  b[0] = (uint16_t)v >> 8;
  b[1] = (uint16_t)v & 0xFF;
}

uint8_t WireMock::requestFrom(uint8_t a, uint8_t n) {
  rlen = n; rpos = 0;
  if (a == 0x68 && world.mpuPresent && reg == 0x3B) {
    double t = simUs / 1e6;
    float amp = (world.jam ? 0.0f : duty() * 25) + world.extraVibMg;   // mg; a jammed shaft stops shaking
    float vx = amp * 1.41f * std::sin(2 * M_PI * 23 * t) + noise(rng);
    float vy = amp * 1.41f * std::cos(2 * M_PI * 23 * t) + noise(rng);
    float vz = noise(rng);
    float tilt = world.tiltDeg * (float)M_PI / 180;
    put16(rbuf + 0, vx / 1000 * 8192);
    put16(rbuf + 2, (std::sin(tilt) * 1000 + vy) / 1000 * 8192);
    put16(rbuf + 4, (std::cos(tilt) * 1000 + vz) / 1000 * 8192);
    return n;
  }
  rlen = 0;
  return 0;
}

// ------------------------------------------------------------ time and people
static bool goHeld = false;
static double goReleaseAt = 0;

void boot() {
  for (int i = 0; i < 32; i++) pinState[i] = HIGH;   // pull-ups: nothing pressed
  setup();
}

double now() { return simUs / 1e6; }

void run(double seconds) {
  double end = now() + seconds;
  while (now() < end) {
    if (goHeld && now() >= goReleaseAt) { pinState[PIN_BTN_GO] = HIGH; goHeld = false; }
    loop();
    simUs += 250;                                    // the loop's own run time
  }
}

void pressGo() {
  pinState[PIN_BTN_GO] = LOW;
  goHeld = true;
  goReleaseAt = now() + 0.15;
}

void pressEstop() {
  pinState[PIN_BTN_ESTOP] = LOW;
  if (simIsr[1]) simIsr[1]();                        // falling edge: the hardware interrupt fires
}

void releaseEstop() { pinState[PIN_BTN_ESTOP] = HIGH; }

void sendSerial(const std::string& text) { serialIn += text; }

// ------------------------------------------------------------ observation
const std::vector<std::string>& serialLines() { return lines; }

bool sawLine(const std::string& needle, size_t fromLine) {
  for (size_t i = fromLine; i < lines.size(); i++)
    if (lines[i].find(needle) != std::string::npos) return true;
  return false;
}

size_t countLines(const std::string& needle) {
  size_t n = 0;
  for (const auto& l : lines) if (l.find(needle) != std::string::npos) n++;
  return n;
}

std::string lcdText(int row) { return std::string(lcdScreen[row], 16); }
