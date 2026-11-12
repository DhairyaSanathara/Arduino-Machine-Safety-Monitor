// Host-side stand-in for the LiquidCrystal_I2C library: the "screen" is two strings.
#pragma once
#include "Arduino.h"
extern char lcdScreen[2][17];
class LiquidCrystal_I2C {
  int r = 0, c = 0;
 public:
  LiquidCrystal_I2C(uint8_t, uint8_t, uint8_t) {}
  void init() {}
  void backlight() {}
  void command(uint8_t) {}
  void home() { c = 0; r = 0; }
  void setCursor(int col, int row) { c = col; r = row; }
  size_t write(uint8_t ch) { if (c < 16) lcdScreen[r][c++] = ch; return 1; }
  void print(const char* s) { for (; *s && c < 16; s++) lcdScreen[r][c++] = *s; }
};
