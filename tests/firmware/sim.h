// sim.h - a simulated machine for testing the firmware on a PC.
//
// The real firmware (firmware/MachineSafetyMonitor) is compiled against small
// stand-ins for the Arduino core and libraries (tests/firmware/mocks). This file
// describes the "world" around it: a person in front of the sensor, the air
// temperature, the motor's vibration, a jammed shaft, unplugged sensors...
// Tests change the world, let the firmware run for a while, and check what it did.
#pragma once
#include <string>
#include <vector>

struct World {
  float distCm     = 120;    // nearest object in front of the HC-SR04 (0 = sensor unplugged)
  float tempC      = 30;     // air temperature at the DHT
  int   dhtModel   = 22;     // which DHT is fitted: 11 or 22
  bool  dhtPresent = true;
  bool  mpuPresent = true;
  bool  lcdPresent = true;
  float extraVibMg = 0;      // extra shaking, e.g. an unbalanced weight on the shaft
  float tiltDeg    = 0;      // the base tilted about one axis
  bool  jam        = false;  // the shaft is blocked: the motor no longer vibrates
};
extern World world;

// Time
void   boot();                        // power-up: runs setup()
void   run(double seconds);           // runs loop() for this much simulated time
double now();                         // seconds since power-up

// What a person can do
void pressGo();                       // press and release GO (0.15 s)
void pressEstop();                    // press E-STOP and keep it pressed (fires the interrupt)
void releaseEstop();
void sendSerial(const std::string& text);

// What can be observed from outside
bool relayClosed();                   // is the motor's 12 V switched on?
int  motorPwm();                      // duty on the ENA pin, 0..255
const std::vector<std::string>& serialLines();
bool sawLine(const std::string& needle, size_t fromLine = 0);
size_t countLines(const std::string& needle);
std::string lcdText(int row);
