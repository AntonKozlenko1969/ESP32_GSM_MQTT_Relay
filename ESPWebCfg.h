#ifndef __ESPWEBCFG_H
#define __ESPWEBCFG_H

#include <Arduino.h>

//#define NOSERIAL // Раскомментируйте это макроопределение, чтобы не использовать отладочный вывод в Serial (можно будет использовать пины RX и TX после загрузки скетча для полезной нагрузки)
#define NOBLED // Раскомментируйте это макроопределение, чтобы не использовать мигание встроенного светодиода (можно будет использовать пин LED_BUILTIN для полезной нагрузки)
// #ifndef ESP8266 
//   #ifndef NOBLED
//     #define LED_BUILTIN 32  // для ESP32 определить GPIO LED
//   #endif
// #endif

//#define NOEEPROMERASE // Раскомментируйте это макроопределение, чтобы не использовать возможность стирания EEPROM замыканием A0 (GPIO36) на VCC при старте

// для работы MQTT с подтверждением получения команды на управление реле ESP_Relay/Relay/Confirm/1/x (x-состояние реле 0 или 1)
#define CONFIRM_CONFIG_TOPIC //Раскомментируйте это макроопределение, для создания двух топиков для одного реле. Confirm - sub, Config - pub

#ifndef NOEEPROMERASE
const uint16_t eepromEraseLevel = 900;
const uint32_t eepromEraseTime = 2000;
#endif

//#define USEDS3231 // Закомментируйте это макроопределение, если вы не планируете использовать часы реального времени DS3231
//#define USEAT24C32 // Закомментируйте это макроопределение, если вы не планируете использовать I2C EEPROM

#if defined(USEDS3231) || defined(USEAT24C32) // Определите пины, на которые подключены I2C устройства
const int8_t pinSDA = SDA; // I2C SDA pin
const int8_t pinSCL = SCL; // I2C SCL pin
const bool fastI2C = true; // I2C Fast mode (400 kHz)
#endif

#endif
