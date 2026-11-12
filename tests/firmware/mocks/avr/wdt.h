// Host-side stand-in for <avr/wdt.h> (the watchdog does nothing on a PC)
#pragma once
#define WDTO_2S 7
inline void wdt_enable(int) {}
inline void wdt_disable() {}
inline void wdt_reset() {}
