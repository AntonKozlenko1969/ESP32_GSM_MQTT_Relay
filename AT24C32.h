#ifndef __AT24C32_H
#define __AT24C32_H

#define SINGLE_INSTANCE // comment this define to use multiple class instances

#ifdef SINGLE_INSTANCE
#define STATIC static
#else
#define STATIC
#endif

#include <Arduino.h>

class AT24C32 {
public:
  static const uint8_t AT24C32_ADDRESS = 0x57;

  STATIC void init(bool fast = false);
#ifdef ESP8266
  STATIC void init(int8_t pinSDA, int8_t pinSCL, bool fast = false);
#endif
  STATIC bool begin(uint8_t address = AT24C32_ADDRESS);
  STATIC uint8_t read(uint16_t index);
  STATIC void read(uint16_t index, uint8_t* buf, uint16_t len);
  STATIC void write(uint16_t index, uint8_t data);
  STATIC void write(uint16_t index, const uint8_t* buf, uint16_t len);
  template<typename T> STATIC T& get(uint16_t index, T& t) {
    read(index, (uint8_t*)&t, sizeof(T));
    return t;
  }
  template<typename T> STATIC const T& put(uint16_t index, const T& t) {
    write(index, (const uint8_t*)&t, sizeof(T));
    return t;
  }

private:
  STATIC uint8_t _address;
};

#ifdef SINGLE_INSTANCE
extern AT24C32 at24c32;
#endif

#endif
