//#define USEIRREMOTE // Закомментируйте это макроопределение, если не нужна поддержка ИК-пульта
//#define USEDHT // Закомментируйте это макроопределение, если не нужна поддержка датчиков DHTxx
//#define USEDS1820 // Закомментируйте это макроопределение, если не нужна поддержка датчиков DS18x20
//#define USELDR // Закомментируйте это макроопределение, если не нужна поддержка датчика освещенности

// https://www.youtube.com/watch?v=0gjqXPSK4yE

// **********  Платформа ESP8266 не позднее 2.4.2 На более поздних не собирается !!!!
/* !!!!!! You only need to format SPIFFS the first time you run a
   test or else use the SPIFFS plugin to create a partition
    FORMAT_SPIFFS_IF_FAILED true !!!! in ESPWeb.cpp file
   https://github.com/me-no-dev/arduino-esp32fs-plugin */
/* !!!! При первой загрузе скетча в ESP32 необходимо ОТФОРМАТИРОВАТЬ SPIFFS
    Для этого установить  FORMAT_SPIFFS_IF_FAILED true !!!! в файле ESPWeb.cpp
    Потом исправить в false и загрузить скетч повторно !!!!! */

#include <pgmspace.h>
#ifdef USEIRREMOTE
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#endif
#ifdef USEDHT
#include <DHT.h>
#endif
#ifdef USEDS1820
#include "DS1820.h"
#endif
#include "ESPWebMQTT.h"
#include "Date.h"
//#include "time.h"
#include "Schedule.h"
#include "RTCmem.h"

const int8_t maxRelays = 5; // Количество каналов реле
const int8_t maxSchedules = 10; // Количество элементов расписания

const char overSSID[] PROGMEM = "ESP_GSMRelay_"; // Префикс имени точки доступа по умолчанию
const char overMQTTClient[] PROGMEM = "ESP_GSMRelay"; // Префикс имени MQTT-клиента по умолчанию

const int8_t defRelayPin[maxRelays]={2,12,14,25,13}; // Пин реле по умолчанию (-1 - не подключено) // изменено 28/11/2023
const bool defRelayLevel = HIGH; // Уровень срабатывания реле по умолчанию
const bool defRelayOnBoot = false; // Состояние реле при старте модуля по умолчанию
const uint32_t defRelayAutoRelease[maxRelays] = {0,0,0,0,360000}; // Время в миллисекундах до автоотключения реле по умолчанию (0 - нет автоотключения) // изменено 19/04/2023

const int8_t defBtnPin[maxRelays] = {-1,-1,-1,-1,36}; // Пин кнопки по умолчанию (-1 - не подключено) // изменено 19/04/2023
const bool defBtnLevel = HIGH; // Уровень кнопки при замыкании по умолчанию
const uint32_t defDebounceTime = 20; // Время стабилизации уровня в миллисекундах для борьбы с дребезгом по умолчанию (0 - не используется)

#ifdef USEIRREMOTE
const int8_t defIRPin = -1; // Пин, к которому подключен ИК-датчик по умолчанию (-1 - не подключено)
const int8_t defIRType = NEC; // Тип пульта ДУ по умолчанию
const uint32_t defIROn = 0; // Код кнопки пульта ДУ для включения реле по умолчанию
const uint32_t defIRToggle = 0; // Код кнопки пульта ДУ для переключения реле по умолчанию
const uint32_t defIROff = 0; // Код кнопки пульта ДУ для выключения реле по умолчанию
#endif

#ifdef USEDHT
const int8_t defDHTPin = -1; // Пин, к которому подключен датчик DHT по умолчанию (-1 - не подключено)
const bool defDHTType = DHT11; // Тип датчика DHT по умолчанию
#endif

#ifdef USEDS1820
const int8_t defDSPin = -1; // Пин, к которому подключен датчик DS1820 по умолчанию (-1 - не подключено)
#endif

const char pathIndexCss[] PROGMEM = "/index.css";
const char pathIndexJs[] PROGMEM = "/index.js";
const char pathRelay[] PROGMEM = "/relay"; // Путь до страницы настройки параметров реле
const char pathControl[] PROGMEM = "/control"; // Путь до страницы настройки параметров кнопок/ДУ
const char pathSwitch[] PROGMEM = "/switch"; // Путь до страницы управления переключением реле
const char pathSchedule[] PROGMEM = "/schedule"; // Путь до страницы настройки параметров расписания
const char pathGetSchedule[] PROGMEM = "/getschedule"; // Путь до страницы, возвращающей JSON-пакет элемента расписания
const char pathSetSchedule[] PROGMEM = "/setschedule"; // Путь до страницы изменения элемента расписания
#ifdef USEIRREMOTE
const char pathIRData[] PROGMEM = "/irdata"; // Путь до страницы, возвращающей JSON-пакет данных о последней нажатой кнопке пульта ДУ
#endif
#if defined(USEDHT) || defined(USEDS1820)
const char pathClimate[] PROGMEM = "/climate"; // Путь до страницы настройки параметров датчиков температуры
#endif
#ifdef USELDR
const char pathLDR[] PROGMEM = "/ldr"; // Путь до страницы настройки параметров датчика освещенности
#endif

// Имена параметров для Web-форм
const char paramGPIO[] PROGMEM = "relaygpio";
const char paramLevel[] PROGMEM = "relaylevel";
const char paramOnBoot[] PROGMEM = "relayonboot";
const char paramAutoRelease[] PROGMEM = "relayautorelease";
const char paramBtnGPIO[] PROGMEM = "btngpio";
const char paramBtnLevel[] PROGMEM = "btnlevel";
const char paramBtnSwitch[] PROGMEM = "btnswitch";
const char paramDebounce[] PROGMEM = "debounce";
const char paramSchedulePeriod[] PROGMEM = "period";
const char paramScheduleHour[] PROGMEM = "hour";
const char paramScheduleMinute[] PROGMEM = "minute";
const char paramScheduleSecond[] PROGMEM = "second";
const char paramScheduleWeekdays[] PROGMEM = "weekdays";
const char paramScheduleDay[] PROGMEM = "day";
const char paramScheduleMonth[] PROGMEM = "month";
const char paramScheduleYear[] PROGMEM = "year";
const char paramScheduleRelay[] PROGMEM = "relay";
const char paramScheduleTurn[] PROGMEM = "turn";
#ifdef USEIRREMOTE
const char paramIRGPIO[] PROGMEM = "irgpio";
const char paramIRType[] PROGMEM = "irtype";
const char paramIROn[] PROGMEM = "iron";
const char paramIRToggle[] PROGMEM = "irtoggle";
const char paramIROff[] PROGMEM = "iroff";
#endif
#ifdef USEDHT
const char paramDHTGPIO[] PROGMEM = "dhtgpio";
const char paramDHTType[] PROGMEM = "dhttype";
const char paramDHTMinTemp[] PROGMEM = "dhtmintemp";
const char paramDHTMaxTemp[] PROGMEM = "dhtmaxtemp";
const char paramDHTMinTempRelay[] PROGMEM = "dhtmintemprelay";
const char paramDHTMaxTempRelay[] PROGMEM = "dhtmaxtemprelay";
const char paramDHTMinTempTurn[] PROGMEM = "dhtmintempturn";
const char paramDHTMaxTempTurn[] PROGMEM = "dhtmaxtempturn";
#endif
#ifdef USEDS1820
const char paramDSGPIO[] PROGMEM = "dsgpio";
const char paramDSMinTemp[] PROGMEM = "dsmintemp";
const char paramDSMaxTemp[] PROGMEM = "dsmaxtemp";
const char paramDSMinTempRelay[] PROGMEM = "dsmintemprelay";
const char paramDSMaxTempRelay[] PROGMEM = "dsmaxtemprelay";
const char paramDSMinTempTurn[] PROGMEM = "dsmintempturn";
const char paramDSMaxTempTurn[] PROGMEM = "dsmaxtempturn";
#endif
#ifdef USELDR
const char paramLDRMinBright[] PROGMEM = "ldrminbright";
const char paramLDRMaxBright[] PROGMEM = "ldrmaxbright";
const char paramLDRMinBrightRelay[] PROGMEM = "ldrminbrightrelay";
const char paramLDRMaxBrightRelay[] PROGMEM = "ldrmaxbrightrelay";
const char paramLDRMinBrightTurn[] PROGMEM = "ldrminbrightturn";
const char paramLDRMaxBrightTurn[] PROGMEM = "ldrmaxbrightturn";
#endif

// Имена JSON-переменных
const char jsonRelay[] PROGMEM = "relay";
const char jsonRelayAutoRelease[] PROGMEM = "relayautorelease";
const char jsonSchedulePeriod[] PROGMEM = "period";
const char jsonScheduleHour[] PROGMEM = "hour";
const char jsonScheduleMinute[] PROGMEM = "minute";
const char jsonScheduleSecond[] PROGMEM = "second";
const char jsonScheduleWeekdays[] PROGMEM = "weekdays";
const char jsonScheduleDay[] PROGMEM = "day";
const char jsonScheduleMonth[] PROGMEM = "month";
const char jsonScheduleYear[] PROGMEM = "year";
const char jsonScheduleRelay[] PROGMEM = "relay";
const char jsonScheduleTurn[] PROGMEM = "turn";
#ifdef USEIRREMOTE
const char jsonIRType[] PROGMEM = "irtype";
const char jsonIRValue[] PROGMEM = "irvalue";
#endif
#ifdef USEDHT
const char jsonTemperature[] PROGMEM = "temperature";
const char jsonHumidity[] PROGMEM = "humidity";
#endif
#ifdef USEDS1820
const char jsonTemperature2[] PROGMEM = "temperature2";
#endif
#ifdef USELDR
const char jsonLDR[] PROGMEM = "ldr";
#endif

// Названия топиков для MQTT
const char mqttRelayTopic[] PROGMEM = "/Relay";

#ifdef CONFIRM_CONFIG_TOPIC
const char mqttRelayConfirmTopic[] PROGMEM = "/Confirm"; //топик для публикаций роложения реле
const char mqttRelayConfigTopic[] PROGMEM = "/Config"; //топик для подписки и получения сообщений
#endif

#ifdef USEDHT
const char mqttTemperatureTopic[] PROGMEM = "/DHT/Temperature";
const char mqttHumidityTopic[] PROGMEM = "/DHT/Humidity";
#endif
#ifdef USEDS1820
const char mqttTemperatureTopic2[] PROGMEM = "/DS1820/Temperature";
#endif
#ifdef USELDR
const char mqttLDRTopic[] PROGMEM = "/LDR";
#endif

// Доступные для подключения GPIO
#ifdef ESP8266
const int8_t gpios[] PROGMEM = { -1, 0, 1, 2, 3, 4, 5, 12, 13, 14, 15, 16 }; 
#else
const int8_t gpios[] PROGMEM = { -1, 0, 2, 5, 12, 13, 14, 18, 23, 25, 32, 36};
#endif
const char strNone[] PROGMEM = "(None)";

class ESPWebMQTTRelay : public ESPWebMQTTBase {
public:
  ESPWebMQTTRelay() : ESPWebMQTTBase() {}
  void switchRelay(int8_t id, bool on); // Процедура включения/выключения реле
  int8_t relayPin[maxRelays]; // Пины, к которым подключены реле (-1 - не подключено)
  uint16_t writeConfig(bool commit = true);
  void mqttCallback(char* topic, byte* payload, unsigned int length);  
  void add_in_queue_comand(int _inncomand, const char* _inn_text_comand, int _com_flag);

protected:
  void setupExtra();
  void loopExtra();

  uint16_t readRTCmemory();
  uint16_t writeRTCmemory();
  uint16_t readConfig();

  void defaultConfig(uint8_t level = 0);
  bool setConfigParam(const String& name, const String& value);

  void setupHttpServer();
  void handleRootPage();
  String jsonData();
  void handleIndexCss();
  void handleIndexJs();
#ifdef USEIRREMOTE
  void handleIRData(); // Обработчик страницы, возвращающей JSON-пакет данных о последней нажатой кнопке пульта ДУ
#endif
  void handleRelayConfig(); // Обработчик страницы настройки параметров реле
  void handleControlConfig(); // Обработчик страницы настройки параметров кнопок и пульта ДУ
  void handleRelaySwitch(); // Обработчик страницы управления переключением реле
  void handleScheduleConfig(); // Обработчик страницы настройки параметров расписания
  void handleGetSchedule(); // Обработчик страницы, возвращающей JSON-пакет элемента расписания
  void handleSetSchedule(); // Обработчик страницы изменения элемента расписания
#if defined(USEDHT) || defined(USEDS1820)
  void handleClimateConfig(); // Обработчик страницы настройки параметров датчиков температуры
#endif
#ifdef USELDR
  void handleLDRConfig(); // Обработчик страницы настройки параметров датчика освещенности
#endif

  String navigator();
  String btnRelayConfig(); // HTML-код кнопки вызова настройки реле
  String btnControlConfig(); // HTML-код кнопки вызова настройки кнопок и пульта ДУ
  String btnScheduleConfig(); // HTML-код кнопки вызова настройки расписания
#if defined(USEDHT) || defined(USEDS1820)
  String btnClimateConfig(); // HTML-код кнопки вызова настройки датчика температуры
#endif
#ifdef USELDR
  String btnLDRConfig(); // HTML-код кнопки вызова настройки датчика температуры
#endif

  //void mqttCallback(char* topic, byte* payload, unsigned int length);
  void mqttResubscribe();
  
private:
  //void switchRelay(int8_t id, bool on); // Процедура включения/выключения реле
  void toggleRelay(int8_t id); // Процедура переключения реле

  bool debounceRead(int8_t id, uint32_t debounceTime); // Чтение кнопки с подавлением дребезга

  uint16_t readScheduleConfig(uint16_t offset); // Чтение из EEPROM порции параметров расписания
  uint16_t writeScheduleConfig(uint16_t offset); // Запись в EEPROM порции параметров расписания

#ifdef USEDHT
  void publishTemperature(); // Публикация температуры в MQTT
  void publishHumidity(); // Публикация влажности в MQTT
#endif

#ifdef USEDS1820
  void publishTemperature2(); // Публикация температуры DS1820 в MQTT
#endif

#ifdef USELDR
  void publishLDR(); // Публикация освещенности в MQTT
#endif

  //int8_t relayPin[maxRelays]; // Пины, к которым подключены реле (-1 - не подключено)
  bool relayLevel[maxRelays]; // Уровни срабатывания реле
  bool relayOnBoot[maxRelays]; // Состояние реле при старте модуля
  uint32_t relayAutoRelease[maxRelays]; // Значения задержки реле в миллисекундах до автоотключения (0 - нет автоотключения)
  uint32_t autoRelease[maxRelays]; // Значение в миллисекундах для сравнения с millis(), когда реле должно отключиться автоматически (0 - нет автоотключения)
  uint32_t lastStates; // Битовое поле состояния реле для восстановления после перезагрузки

  int8_t btnPin[maxRelays]; // Пины, к которым подключены кнопки (-1 - не подключено)
  uint8_t btnLevel[maxRelays]; // Уровни на цифровом входе при замыкании кнопки (младший бит) и признак фиксируемого выключателя (старший бит)
  uint32_t debounceTime[maxRelays]; // Длительность в миллисекундах для подавления дребезга (0 - не используется, например для сенсорных кнопок)

  Schedule schedule[maxSchedules]; // Массив расписания событий
  uint8_t scheduleRelay[maxSchedules]; // Какой канал реле и что с ним делать по срабатыванию события (6 младших бит - номер канала реле, 2 старших бита - вкл/выкл/перекл)

#ifdef USEIRREMOTE
  int8_t irPin; // Пин, к которому подключен ИК датчик (-1 - не подключено)
  int8_t irType; // Тип пульта ДУ
  uint32_t irOn[maxRelays], irToggle[maxRelays], irOff[maxRelays]; // Коды кнопок пульта ДУ для включения, переключения и выключения реле (достаточно одного переключения)

  int8_t lastIRType; // Тип пульта ДУ последнего успешного декодирования
  uint32_t lastIRValue; // Код кнопки ДУ последнего успешного декодирования

  IRrecv* ir;
#endif

#ifdef USEDHT
  int8_t dhtPin; // Пин, к которому подключен датчик DHT
  int8_t dhtType; // Тип датчика DHTxx

  uint32_t dhtReadTime; // Время в миллисекундах, после которого можно считывать новое значение температуры
  float dhtTemperature; // Значение успешно прочитанной температуры
  float dhtHumidity; // Значение успешно прочитанной влажности
  float dhtMinTemp, dhtMaxTemp; // Минимальное и максимальное значение температуры срабатывания реле
  uint8_t dhtMinTempRelay, dhtMaxTempRelay; // Какой канал реле и что с ним делать по срабатыванию события (6 младших бит - номер канала реле, 2 старших бита - вкл/выкл/перекл)
  bool dhtMinTempTriggered, dhtMaxTempTriggered; // Было ли срабатывание реле по порогу температуры?

  DHT* dht;
#endif

#ifdef USEDS1820
  int8_t dsPin; // Пин, к которому подключен датчик DS1820

  uint32_t dsReadTime; // Время в миллисекундах, после которого можно считывать новое значение температуры
  float dsTemperature; // Значение успешно прочитанной температуры
  float dsMinTemp, dsMaxTemp; // Минимальное и максимальное значение температуры срабатывания реле
  uint8_t dsMinTempRelay, dsMaxTempRelay; // Какой канал реле и что с ним делать по срабатыванию события (6 младших бит - номер канала реле, 2 старших бита - вкл/выкл/перекл)
  bool dsMinTempTriggered, dsMaxTempTriggered; // Было ли срабатывание реле по порогу температуры?

  DS1820* ds;
#endif

#ifdef USELDR
  uint32_t ldrReadTime; // Время в миллисекундах, после которого можно считывать новое значение освещенности
  int16_t ldr; // Значение успешно прочитанной освещенности
  int16_t ldrMinBright, ldrMaxBright; // Минимальное и максимальное значение освещенности срабатывания реле
  uint8_t ldrMinBrightRelay, ldrMaxBrightRelay; // Какой канал реле и что с ним делать по срабатыванию события (6 младших бит - номер канала реле, 2 старших бита - вкл/выкл/перекл)
  bool ldrMinBrightTriggered, ldrMaxBrightTriggered; // Было ли срабатывание реле по порогу освещенности?
#endif
};

// добавление команды и текста команды в очередь
void ESPWebMQTTRelay::add_in_queue_comand(int _inncomand, const char* _inn_text_comand, int _com_flag){
  ESPWebMQTTBase::add_in_queue_comand(_inncomand, _inn_text_comand, _com_flag);
 
}

void ESPWebMQTTRelay::setupExtra() {
  ESPWebMQTTBase::setupExtra();

  for (int8_t i = 0; i < maxRelays; ++i) {
    autoRelease[i] = 0;
    
    if (relayPin[i] != -1) {
      pinMode(relayPin[i], OUTPUT);  //removing
 
      if (lastStates != (uint32_t)-1) {
        if (lastStates & ((uint32_t)1 << i)) {
          digitalWrite(relayPin[i], relayLevel[i]);        
          if (relayAutoRelease[i])
            autoRelease[i] = millis() + relayAutoRelease[i];
        } else
          digitalWrite(relayPin[i], ! relayLevel[i]);
      } else {
        digitalWrite(relayPin[i], relayLevel[i] == relayOnBoot[i]);
        }
      #ifndef ESP8266  // если не ESP8266
        //digitalWrite(relayPin[i], LOW);  //added        
        digitalWrite(relayPin[i], ! relayLevel[i]);  //added   
      #endif           
    }
      
    if (btnPin[i] != -1)
      pinMode(btnPin[i], (btnLevel[i] & 0x01) != 0 ? INPUT : INPUT_PULLUP);
  }

#ifdef USEIRREMOTE
  if (irPin != -1) {
    ir = new IRrecv(irPin);
    ir->enableIRIn();
    lastIRType = UNKNOWN;
    lastIRValue = 0;
  }
#endif

#ifdef USEDHT
  if (dhtPin != -1) {
    dht = new DHT(dhtPin, dhtType, 11);
    dht->begin();
    dhtReadTime = millis() + 2000;
  }
  dhtTemperature = NAN;
  dhtHumidity = NAN;
  dhtMinTempTriggered = false;
  dhtMaxTempTriggered = false;
#endif

#ifdef USEDS1820
  if (dsPin != -1) {
    ds = new DS1820(dsPin);
    if (! ds->find()) {
      _log->println(F("DS18x20 device not detected!"));
      delete ds;
      ds = NULL;
    } else {
      ds->update();
      dsReadTime = millis() + ds->MEASURE_TIME;
    }
  }
  dsTemperature = NAN;
  dsMinTempTriggered = false;
  dsMaxTempTriggered = false;
#endif

#ifdef USELDR
  pinMode(A0, INPUT);
  ldrReadTime = 0;
  ldr = -1;
  ldrMinBrightTriggered = false;
  ldrMaxBrightTriggered = false;
#endif
// временно
getBoardId();

}

void ESPWebMQTTRelay::loopExtra() {
  ESPWebMQTTBase::loopExtra();

  for (int8_t i = 0; i < maxRelays; i++) {
    if ((relayPin[i] != -1) && autoRelease[i] && ((int32_t)(millis() - autoRelease[i]) >= 0)) {
      switchRelay(i, false);
      autoRelease[i] = 0;
    }

    if (btnPin[i] != -1) {
      static bool btnLast[maxRelays];
      bool btnPressed = debounceRead(i, debounceTime[i]);

      if (btnPressed != btnLast[i]) {
        if ((btnLevel[i] & 0x80) != 0) { // Switch
          switchRelay(i, btnPressed);
        } else { // Button
          if (btnPressed)
            toggleRelay(i);
        }
        _log->print(F("Button["));
        _log->print(i + 1);
        _log->print(F("] "));
        _log->println(btnPressed ? F("pressed") : F("released"));
        btnLast[i] = btnPressed;
      }
    }
   //добавлено - обработка состояния псевдо реле 5 
    if (i==4){
      if (relayPin[i] != -1) {
       if (pubSubClient->connected()) {
           if (digitalRead(relayPin[i]) == relayLevel[i]){
              local_WEB_access = true; // разблокировать WEB интерфейс реле 5 - включено
             }
              else{
             local_WEB_access = false; // заблокировать WEB интерфейс реле 5 - отключено
              }
       }
       else local_WEB_access = true; // разблокировать WEB интерфейс если нет связи с MQTT
      }
      else local_WEB_access = true; // разблокировать WEB интерфейс если пин не определен
    }    
  }

  uint32_t now = getTime();
  if (now) {
    for (int8_t i = 0; i < maxSchedules; i++) {
      if ((schedule[i].period() != Schedule::NONE) && ((scheduleRelay[i] & 0x3F) < maxRelays)) {
        if (schedule[i].check(now)) {
          if ((scheduleRelay[i] & 0x80) != 0) // toggle bit is set
            toggleRelay(scheduleRelay[i] & 0x3F);
          else
            switchRelay(scheduleRelay[i] & 0x3F, (scheduleRelay[i] >> 6) & 0x01);
          _log->print(dateTimeToStr(now));
          _log->print(F(" Schedule \""));
          _log->print(schedule[i]);
          _log->print(F("\" turned relay #"));
          _log->print((scheduleRelay[i] & 0x3F) + 1);
          if ((scheduleRelay[i] & 0x80) != 0)
            _log->println(F(" opposite"));
          else
            _log->println((scheduleRelay[i] & 0x40) != 0 ? F(" on") : F(" off"));
        }
      }
    }
  }

#ifdef USEIRREMOTE
  if ((irPin != -1) && (ir != NULL)) {
    static decode_results results;

    if (ir->decode(&results)) {
      lastIRType = results.decode_type;
      if (results.value != 0xFFFFFFFF) // repeat
        lastIRValue = (uint32_t)results.value;
      if (results.decode_type == irType) {
        _log->print("IR value 0x");
        _log->println((uint32_t)results.value, HEX);
        for (int8_t i = 0; i < maxRelays; i++) {
          if (irOn[i] && (results.value == irOn[i])) {
            switchRelay(i, true);
            _log->print(F("On["));
            _log->print(i + 1);
            _log->println(F("] button"));
          } else if (irToggle[i] && (results.value == irToggle[i])) {
            toggleRelay(i);
            _log->print(F("Toggle["));
            _log->print(i + 1);
            _log->println(F("] button"));
          } else if (irOff[i] && (results.value == irOff[i])) {
            switchRelay(i, false);
            _log->print(F("Off["));
            _log->print(i + 1);
            _log->println(F("] button"));
          }
        }
      } else {
        _log->print(F("IR unexpected type "));
        _log->print(results.decode_type);
        _log->print(F(" with value 0x"));
        _log->println((uint32_t)results.value, HEX);
      }
      ir->resume(); // Receive the next value
    }
  }
#endif

#ifdef USEDHT
  if ((dhtPin != -1) && (dht != NULL)) {
    if ((int32_t)(millis() - dhtReadTime) >= 0) {
      float v;

      v = dht->readTemperature();
      if (! isnan(v)) {
        if (isnan(dhtTemperature) || (dhtTemperature != v)) {
          dhtTemperature = v;
          publishTemperature();
          if (! isnan(dhtMinTemp)) {
            if (dhtTemperature < dhtMinTemp) {
              if (! dhtMinTempTriggered) {
                if ((dhtMinTempRelay & 0x80) != 0) // toggle bit is set
                  toggleRelay(dhtMinTempRelay & 0x3F);
                else
                  switchRelay(dhtMinTempRelay & 0x3F, (dhtMinTempRelay >> 6) & 0x01);
                _log->println(F("DHT minimal temperature triggered"));
                dhtMinTempTriggered = true;
              }
            } else
              dhtMinTempTriggered = false;
          }
          if (! isnan(dhtMaxTemp)) {
            if (dhtTemperature > dhtMaxTemp) {
              if (! dhtMaxTempTriggered) {
                if ((dhtMaxTempRelay & 0x80) != 0) // toggle bit is set
                  toggleRelay(dhtMaxTempRelay & 0x3F);
                else
                  switchRelay(dhtMaxTempRelay & 0x3F, (dhtMaxTempRelay >> 6) & 0x01);
                _log->println(F("DHT maximal temperature triggered"));
                dhtMaxTempTriggered = true;
              }
            } else
              dhtMaxTempTriggered = false;
          }
        }
      } else {
        _log->println(F("DHT temperature read error!"));
      }
      v = dht->readHumidity();
      if (! isnan(v)) {
        if (isnan(dhtHumidity) || (dhtHumidity != v)) {
          dhtHumidity = v;
          publishHumidity();
        }
      } else {
        _log->println(F("DHT humidity read error!"));
      }
      dhtReadTime = millis() + 2000;
    }
  }
#endif

#ifdef USEDS1820
  if ((dsPin != -1) && (ds != NULL)) {
    if ((int32_t)(millis() - dsReadTime) >= 0) {
      float v;

      v = ds->readTemperature();
      ds->update();
      if (! isnan(v) && (v >= -50.0) && (v <= 50.0)) {
        if (isnan(dsTemperature) || (dsTemperature != v)) {
          dsTemperature = v;
          publishTemperature2();
          if (! isnan(dsMinTemp)) {
            if (dsTemperature < dsMinTemp) {
              if (! dsMinTempTriggered) {
                if ((dsMinTempRelay & 0x80) != 0) // toggle bit is set
                  toggleRelay(dsMinTempRelay & 0x3F);
                else
                  switchRelay(dsMinTempRelay & 0x3F, (dsMinTempRelay >> 6) & 0x01);
                _log->println(F("DS1820 minimal temperature triggered"));
                dsMinTempTriggered = true;
              }
            } else
              dsMinTempTriggered = false;
          }
          if (! isnan(dsMaxTemp)) {
            if (dsTemperature > dsMaxTemp) {
              if (! dsMaxTempTriggered) {
                if ((dsMaxTempRelay & 0x80) != 0) // toggle bit is set
                  toggleRelay(dsMaxTempRelay & 0x3F);
                else
                  switchRelay(dsMaxTempRelay & 0x3F, (dsMaxTempRelay >> 6) & 0x01);
                _log->println(F("DS1820 maximal temperature triggered"));
                dsMaxTempTriggered = true;
              }
            } else
              dsMaxTempTriggered = false;
          }
        }
      } else {
        _log->println(F("DS1820 temperature read error!"));
      }
      dsReadTime = millis() + 1000; // ds->MEASURE_TIME;
    }
  }
#endif

#ifdef USELDR
  if ((int32_t)(millis() - ldrReadTime) >= 0) {
    static const int16_t LDR_TOLERANCE = 120;
    static const uint32_t LDR_TIMEOUT = 500;

    int16_t v;

    v = analogRead(A0);
    if (v != -1) {
      if ((ldr == -1) || (abs(ldr - v) > LDR_TOLERANCE)) {
        ldr = v;
        publishLDR();
        if (ldrMinBright != -1) {
          if (ldr <= ldrMinBright) {
            if (! ldrMinBrightTriggered) {
              if ((ldrMinBrightRelay & 0x80) != 0) // toggle bit is set
                toggleRelay(ldrMinBrightRelay & 0x3F);
              else
                switchRelay(ldrMinBrightRelay & 0x3F, (ldrMinBrightRelay >> 6) & 0x01);
              _log->println(F("LDR minimal brightness triggered"));
              ldrMinBrightTriggered = true;
            }
          } else
            ldrMinBrightTriggered = false;
        }
        if (ldrMaxBright != -1) {
          if (ldr >= ldrMaxBright) {
            if (! ldrMaxBrightTriggered) {
              if ((ldrMaxBrightRelay & 0x80) != 0) // toggle bit is set
                toggleRelay(ldrMaxBrightRelay & 0x3F);
              else
                switchRelay(ldrMaxBrightRelay & 0x3F, (ldrMaxBrightRelay >> 6) & 0x01);
              _log->println(F("LDR maximal brightness triggered"));
              ldrMaxBrightTriggered = true;
            }
          } else
            ldrMaxBrightTriggered = false;
        }
      }
    } else {
      _log->println(F("LDR brightness read error!"));
    }
    ldrReadTime = millis() + LDR_TIMEOUT;
  }
#endif
}

uint16_t ESPWebMQTTRelay::readRTCmemory() {
 uint16_t offset=0;
#ifdef ESP8266
  offset = ESPWebMQTTBase::readRTCmemory();

  if (offset) {
    uint32_t controlStates;

    RTCmem.get(offset, lastStates);
    offset += sizeof(lastStates);
    RTCmem.get(offset, controlStates);
    offset += sizeof(controlStates);
    if (controlStates != ~lastStates) {
      _log->println(F("Last relay states in RTC memory is illegal!"));
      lastStates = (uint32_t)-1;
    } else {
      _log->println(F("Last relay states restored from RTC memory"));
    }
  } else
    lastStates = (uint32_t)-1;
#endif
  return offset;
}

uint16_t ESPWebMQTTRelay::writeRTCmemory() {
  uint16_t offset=0;
#ifdef ESP8266  
   offset = ESPWebMQTTBase::writeRTCmemory();

  if (offset) {
    uint32_t controlStates;

    controlStates = ~lastStates;
    RTCmem.put(offset, lastStates);
    offset += sizeof(lastStates);
    RTCmem.put(offset, controlStates);
    offset += sizeof(controlStates);
    _log->println(F("Last relay states stored to RTC memory"));
  }
#endif
  return offset;
}

uint16_t ESPWebMQTTRelay::readConfig() {
  uint16_t offset = ESPWebMQTTBase::readConfig();

  if (offset) {
    uint16_t start = offset;
    int8_t i;

    for (i = 0; i < maxRelays; i++) {
      getEEPROM(offset, relayPin[i]);
      offset += sizeof(relayPin[i]);
      getEEPROM(offset, relayLevel[i]);
      offset += sizeof(relayLevel[i]);
      getEEPROM(offset, relayOnBoot[i]);
      offset += sizeof(relayOnBoot[i]);
      getEEPROM(offset, relayAutoRelease[i]);
      offset += sizeof(relayAutoRelease[i]);
      getEEPROM(offset, btnPin[i]);
      offset += sizeof(btnPin[i]);
      getEEPROM(offset, btnLevel[i]);
      offset += sizeof(btnLevel[i]);
      getEEPROM(offset, debounceTime[i]);
      offset += sizeof(debounceTime[i]);
    }
    offset = readScheduleConfig(offset);

#ifdef USEIRREMOTE
    getEEPROM(offset, irPin);
    offset += sizeof(irPin);
    getEEPROM(offset, irType);
    offset += sizeof(irType);
    for (i = 0; i < maxRelays; ++i) {
      getEEPROM(offset, irOn[i]);
      offset += sizeof(irOn[i]);
      getEEPROM(offset, irToggle[i]);
      offset += sizeof(irToggle[i]);
      getEEPROM(offset, irOff[i]);
      offset += sizeof(irOff[i]);
    }
#endif

#ifdef USEDHT
    getEEPROM(offset, dhtPin);
    offset += sizeof(dhtPin);
    getEEPROM(offset, dhtType);
    offset += sizeof(dhtType);
    getEEPROM(offset, dhtMinTemp);
    offset += sizeof(dhtMinTemp);
    getEEPROM(offset, dhtMaxTemp);
    offset += sizeof(dhtMaxTemp);
    getEEPROM(offset, dhtMinTempRelay);
    offset += sizeof(dhtMinTempRelay);
    getEEPROM(offset, dhtMaxTempRelay);
    offset += sizeof(dhtMaxTempRelay);
#endif

#ifdef USEDS1820
    getEEPROM(offset, dsPin);
    offset += sizeof(dsPin);
    getEEPROM(offset, dsMinTemp);
    offset += sizeof(dsMinTemp);
    getEEPROM(offset, dsMaxTemp);
    offset += sizeof(dsMaxTemp);
    getEEPROM(offset, dsMinTempRelay);
    offset += sizeof(dsMinTempRelay);
    getEEPROM(offset, dsMaxTempRelay);
    offset += sizeof(dsMaxTempRelay);
#endif

#ifdef USELDR
    getEEPROM(offset, ldrMinBright);
    offset += sizeof(ldrMinBright);
    getEEPROM(offset, ldrMaxBright);
    offset += sizeof(ldrMaxBright);
    getEEPROM(offset, ldrMinBrightRelay);
    offset += sizeof(ldrMinBrightRelay);
    getEEPROM(offset, ldrMaxBrightRelay);
    offset += sizeof(ldrMaxBrightRelay);
#endif

    uint8_t crc = crc8EEPROM(start, offset);
    if (readEEPROM(offset++) != crc) {
      _log->println(F("CRC mismatch! Use default relay parameters."));
      defaultConfig(2);
    }
  }

  return offset;
}

uint16_t ESPWebMQTTRelay::writeConfig(bool commit) {
  uint16_t offset = ESPWebMQTTBase::writeConfig(false);
  uint16_t start = offset;
  int8_t i;

  for (i = 0; i < maxRelays; i++) {
    putEEPROM(offset, relayPin[i]);
    offset += sizeof(relayPin[i]);
    putEEPROM(offset, relayLevel[i]);
    offset += sizeof(relayLevel[i]);
    putEEPROM(offset, relayOnBoot[i]);
    offset += sizeof(relayOnBoot[i]);
    putEEPROM(offset, relayAutoRelease[i]);
    offset += sizeof(relayAutoRelease[i]);
    putEEPROM(offset, btnPin[i]);
    offset += sizeof(btnPin[i]);
    putEEPROM(offset, btnLevel[i]);
    offset += sizeof(btnLevel[i]);
    putEEPROM(offset, debounceTime[i]);
    offset += sizeof(debounceTime[i]);
  }
  offset = writeScheduleConfig(offset);

#ifdef USEIRREMOTE
  putEEPROM(offset, irPin);
  offset += sizeof(irPin);
  putEEPROM(offset, irType);
  offset += sizeof(irType);
  for (i = 0; i < maxRelays; ++i) {
    putEEPROM(offset, irOn[i]);
    offset += sizeof(irOn[i]);
    putEEPROM(offset, irToggle[i]);
    offset += sizeof(irToggle[i]);
    putEEPROM(offset, irOff[i]);
    offset += sizeof(irOff[i]);
  }
#endif

#ifdef USEDHT
  putEEPROM(offset, dhtPin);
  offset += sizeof(dhtPin);
  putEEPROM(offset, dhtType);
  offset += sizeof(dhtType);
  putEEPROM(offset, dhtMinTemp);
  offset += sizeof(dhtMinTemp);
  putEEPROM(offset, dhtMaxTemp);
  offset += sizeof(dhtMaxTemp);
  putEEPROM(offset, dhtMinTempRelay);
  offset += sizeof(dhtMinTempRelay);
  putEEPROM(offset, dhtMaxTempRelay);
  offset += sizeof(dhtMaxTempRelay);
#endif

#ifdef USEDS1820
  putEEPROM(offset, dsPin);
  offset += sizeof(dsPin);
  putEEPROM(offset, dsMinTemp);
  offset += sizeof(dsMinTemp);
  putEEPROM(offset, dsMaxTemp);
  offset += sizeof(dsMaxTemp);
  putEEPROM(offset, dsMinTempRelay);
  offset += sizeof(dsMinTempRelay);
  putEEPROM(offset, dsMaxTempRelay);
  offset += sizeof(dsMaxTempRelay);
#endif

#ifdef USELDR
  putEEPROM(offset, ldrMinBright);
  offset += sizeof(ldrMinBright);
  putEEPROM(offset, ldrMaxBright);
  offset += sizeof(ldrMaxBright);
  putEEPROM(offset, ldrMinBrightRelay);
  offset += sizeof(ldrMinBrightRelay);
  putEEPROM(offset, ldrMaxBrightRelay);
  offset += sizeof(ldrMaxBrightRelay);
#endif

  uint8_t crc = crc8EEPROM(start, offset);
  writeEEPROM(offset++, crc);
  if (commit)
    commitConfig();

  return offset;
}

void ESPWebMQTTRelay::defaultConfig(uint8_t level) {
  int8_t i;

  if (level < 2) {
    ESPWebMQTTBase::defaultConfig(level);

    if (level < 1) {
      _ssid = FPSTR(overSSID);
      _ssid += getBoardId();
    }
    _mqttClient = FPSTR(overMQTTClient);
    _mqttClient += getBoardId();
  }

  if (level < 3) {
    for (i = 0; i < maxRelays; ++i) {
      relayPin[i] = defRelayPin[i]; // изменено 19/04/2023
      relayLevel[i] = defRelayLevel;
      relayOnBoot[i] = defRelayOnBoot;
      relayAutoRelease[i] = defRelayAutoRelease[i]; // изменено 19/04/2023
      btnPin[i] = defBtnPin[i]; // изменено 19/04/2023
      btnLevel[i] = defBtnLevel;
      debounceTime[i] = defDebounceTime;
    }
    for (i = 0; i < maxSchedules; ++i) {
      schedule[i].clear();
      scheduleRelay[i] = 0;
    }

#ifdef USEIRREMOTE
    irPin = defIRPin;
    irType = defIRType;
    for (i = 0; i < maxRelays; ++i) {
      irOn[i] = defIROn;
      irToggle[i] = defIRToggle;
      irOff[i] = defIROff;
    }
#endif

#ifdef USEDHT
    dhtPin = defDHTPin;
    dhtType = defDHTType;
    dhtMinTemp = NAN;
    dhtMaxTemp = NAN;
    dhtMinTempRelay = 0;
    dhtMaxTempRelay = 0;
#endif

#ifdef USEDS1820
    dsPin = defDSPin;
    dsMinTemp = NAN;
    dsMaxTemp = NAN;
    dsMinTempRelay = 0;
    dsMaxTempRelay = 0;
#endif

#ifdef USELDR
    ldrMinBright = -1;
    ldrMaxBright = -1;
    ldrMinBrightRelay = 0;
    ldrMaxBrightRelay = 0;
#endif
  }
}

bool ESPWebMQTTRelay::setConfigParam(const String& name, const String& value) {
  if (! ESPWebMQTTBase::setConfigParam(name, value)) {
    int8_t id = name.length();

    while ((id > 0) && ((name[id - 1] >= '0') && (name[id - 1] <= '9')))
      id--;
    if (id < name.length()) { // Name ends with digits
      id = name.substring(id).toInt();
      if ((id < 0) || (id >= maxRelays)) {
        _log->println(F("Wrong relay index!"));
        return false;
      }
      if (name.startsWith(FPSTR(paramGPIO)))
        relayPin[id] = constrain(value.toInt(), -1, 40);
      else if (name.startsWith(FPSTR(paramLevel)))
        relayLevel[id] = constrain(value.toInt(), 0, 1);
      else if (name.startsWith(FPSTR(paramOnBoot)))
        relayOnBoot[id] = constrain(value.toInt(), 0, 1);
      else if (name.startsWith(FPSTR(paramAutoRelease)))
        relayAutoRelease[id] = _max(0, value.toInt());
      else if (name.startsWith(FPSTR(paramBtnGPIO)))
        btnPin[id] = constrain(value.toInt(), -1, 40);
      else if (name.startsWith(FPSTR(paramBtnLevel)))
        btnLevel[id] = value.toInt() & 0x01;
      else if (name.startsWith(FPSTR(paramBtnSwitch)))
        btnLevel[id] |= (value.toInt() & 0x80);
      else if (name.startsWith(FPSTR(paramDebounce)))
        debounceTime[id] = _max(0, value.toInt());
#ifdef USEIRREMOTE
      else if (name.startsWith(FPSTR(paramIROn)))
        irOn[id] = value.toInt();
      else if (name.startsWith(FPSTR(paramIRToggle)))
        irToggle[id] = value.toInt();
      else if (name.startsWith(FPSTR(paramIROff)))
        irOff[id] = value.toInt();
#endif
      else
        return false;
    } else {
#ifdef USEIRREMOTE
      if (name.equals(FPSTR(paramIRGPIO)))
        irPin = constrain(value.toInt(), -1, 40); //09/01/2024 было последнее значение 16
      else if (name.equals(FPSTR(paramIRType)))
        irType = constrain(value.toInt(), 1, 15);
      else
#endif
#ifdef USEDHT
      if (name.equals(FPSTR(paramDHTGPIO)))
        dhtPin = constrain(value.toInt(), -1, 40); //09/01/2024 было последнее значение 16
      else if (name.equals(FPSTR(paramDHTType)))
        dhtType = value.toInt();
      else if (name.equals(FPSTR(paramDHTMinTemp))) {
        if (value.length())
          dhtMinTemp = value.toFloat();
        else
          dhtMinTemp = NAN;
      } else if (name.equals(FPSTR(paramDHTMaxTemp))) {
        if (value.length())
          dhtMaxTemp = value.toFloat();
        else
          dhtMaxTemp = NAN;
      } else if (name.equals(FPSTR(paramDHTMinTempRelay))) {
        dhtMinTempRelay &= 0B11000000;
        dhtMinTempRelay |= (value.toInt() & 0B00111111);
      } else if (name.equals(FPSTR(paramDHTMaxTempRelay))) {
        dhtMaxTempRelay &= 0B11000000;
        dhtMaxTempRelay |= (value.toInt() & 0B00111111);
      } else if (name.equals(FPSTR(paramDHTMinTempTurn))) {
        dhtMinTempRelay &= 0B00111111;
        dhtMinTempRelay |= ((value.toInt() & 0x03) << 6);
      } else if (name.equals(FPSTR(paramDHTMaxTempTurn))) {
        dhtMaxTempRelay &= 0B00111111;
        dhtMaxTempRelay |= ((value.toInt() & 0x03) << 6);
      } else
#endif
#ifdef USEDS1820
      if (name.equals(FPSTR(paramDSGPIO)))
        dsPin = constrain(value.toInt(), -1, 40); //09/01/2024 было последнее значение 16
      else if (name.equals(FPSTR(paramDSMinTemp))) {
        if (value.length())
          dsMinTemp = value.toFloat();
        else
          dsMinTemp = NAN;
      } else if (name.equals(FPSTR(paramDSMaxTemp))) {
        if (value.length())
          dsMaxTemp = value.toFloat();
        else
          dsMaxTemp = NAN;
      } else if (name.equals(FPSTR(paramDSMinTempRelay))) {
        dsMinTempRelay &= 0B11000000;
        dsMinTempRelay |= (value.toInt() & 0B00111111);
      } else if (name.equals(FPSTR(paramDSMaxTempRelay))) {
        dsMaxTempRelay &= 0B11000000;
        dsMaxTempRelay |= (value.toInt() & 0B00111111);
      } else if (name.equals(FPSTR(paramDSMinTempTurn))) {
        dsMinTempRelay &= 0B00111111;
        dsMinTempRelay |= ((value.toInt() & 0x03) << 6);
      } else if (name.equals(FPSTR(paramDSMaxTempTurn))) {
        dsMaxTempRelay &= 0B00111111;
        dsMaxTempRelay |= ((value.toInt() & 0x03) << 6);
      } else
#endif
#ifdef USELDR
      if (name.equals(FPSTR(paramLDRMinBright))) {
        if (value.length())
          ldrMinBright = constrain(value.toInt(), -1, 1023);
        else
          ldrMinBright = -1;
      } else if (name.equals(FPSTR(paramLDRMaxBright))) {
        if (value.length())
          ldrMaxBright = constrain(value.toInt(), -1, 1023);
        else
          ldrMaxBright = -1;
      } else if (name.equals(FPSTR(paramLDRMinBrightRelay))) {
        ldrMinBrightRelay &= 0B11000000;
        ldrMinBrightRelay |= (value.toInt() & 0B00111111);
      } else if (name.equals(FPSTR(paramLDRMaxBrightRelay))) {
        ldrMaxBrightRelay &= 0B11000000;
        ldrMaxBrightRelay |= (value.toInt() & 0B00111111);
      } else if (name.equals(FPSTR(paramLDRMinBrightTurn))) {
        ldrMinBrightRelay &= 0B00111111;
        ldrMinBrightRelay |= ((value.toInt() & 0x03) << 6);
      } else if (name.equals(FPSTR(paramLDRMaxBrightTurn))) {
        ldrMaxBrightRelay &= 0B00111111;
        ldrMaxBrightRelay |= ((value.toInt() & 0x03) << 6);
      } else
#endif
        return false;
    }
  }

  return true;
}

void ESPWebMQTTRelay::setupHttpServer() {
  ESPWebMQTTBase::setupHttpServer();
  httpServer->on(String(FPSTR(pathIndexCss)).c_str(), std::bind(&ESPWebMQTTRelay::handleIndexCss, this));
  httpServer->on(String(FPSTR(pathIndexJs)).c_str(), std::bind(&ESPWebMQTTRelay::handleIndexJs, this));
#ifdef USEIRREMOTE
  httpServer->on(String(FPSTR(pathIRData)).c_str(), std::bind(&ESPWebMQTTRelay::handleIRData, this));
#endif
  httpServer->on(String(FPSTR(pathRelay)).c_str(), std::bind(&ESPWebMQTTRelay::handleRelayConfig, this));
  httpServer->on(String(FPSTR(pathControl)).c_str(), std::bind(&ESPWebMQTTRelay::handleControlConfig, this));
  httpServer->on(String(FPSTR(pathSwitch)).c_str(), std::bind(&ESPWebMQTTRelay::handleRelaySwitch, this));
#if defined(USEDHT) || defined(USEDS1820)
  httpServer->on(String(FPSTR(pathClimate)).c_str(), std::bind(&ESPWebMQTTRelay::handleClimateConfig, this));
#endif
#ifdef USELDR
  httpServer->on(String(FPSTR(pathLDR)).c_str(), std::bind(&ESPWebMQTTRelay::handleLDRConfig, this));
#endif
  httpServer->on(String(FPSTR(pathSchedule)).c_str(), std::bind(&ESPWebMQTTRelay::handleScheduleConfig, this));
  httpServer->on(String(FPSTR(pathGetSchedule)).c_str(), std::bind(&ESPWebMQTTRelay::handleGetSchedule, this));
  httpServer->on(String(FPSTR(pathSetSchedule)).c_str(), std::bind(&ESPWebMQTTRelay::handleSetSchedule, this));
}

void ESPWebMQTTRelay::handleRootPage() {
  String page = ESPWebBase::webPageStart(F("ESP Relay"));
  page += ESPWebBase::webPageStyle(pathIndexCss, true);
  page += ESPWebBase::webPageScript(pathIndexJs, true);
  page += ESPWebBase::webPageBody();
  page += F("<h3>ESP Relay</h3>\n\
<p>\n\
MQTT broker: <span id=\"");
  page += FPSTR(jsonMQTTConnected);
  page += F("\">?</span><br/>\n\
Heap free size: <span id=\"");
  page += FPSTR(jsonFreeHeap);
  page += F("\">0</span> bytes<br/>\n\
Uptime: <span id=\"");
  page += FPSTR(jsonUptime);
  page += F("\">0</span> seconds<br/>\n");
#ifdef USEDHT
  if ((dhtPin != -1) && (dht != NULL)) {
    page += F("Temperature (DHT): <span id=\"");
    page += FPSTR(jsonTemperature);
    page += F("\">?</span> <sup>o</sup>C<br/>\n\
Humidity (DHT): <span id=\"");
    page += FPSTR(jsonHumidity);
    page += F("\">?</span> %<br/>\n");
  }
#endif
#ifdef USEDS1820
  if ((dsPin != -1) && (ds != NULL)) {
    page += F("Temperature (DS18x20): <span id=\"");
    page += FPSTR(jsonTemperature2);
    page += F("\">?</span> <sup>o</sup>C<br/>\n");
  }
#endif
#ifdef USELDR
  page += F("Brightness: <span id=\"");
  page += FPSTR(jsonLDR);
  page += F("\">?</span><br/>\n");
#endif
  page += F("</p>\n");
  for (int8_t id = 0; id < maxRelays; id++) {
    page += F("<input type=\"checkbox\" class=\"checkbox\" id=\"");
    page += FPSTR(jsonRelay);
    page += String(id);
    page += F("\" onchange=\"openUrl('");
    page += FPSTR(pathSwitch);
    page += F("?id=");
    page += String(id);
    page += F("&on=' + this.checked + '&dummy=' + Date.now());\" ");
    if (relayPin[id] == -1)
      page += F("disabled ");
    else {
      if (!local_WEB_access) page += F("disabled ");  // блокировать изменение, если доступ к WEB интерфейсу закрыт      
      if (digitalRead(relayPin[id]) == relayLevel[id])
        page += FPSTR(extraChecked);
    }
    page += F(">\n\
<label for=\"");
    page += FPSTR(jsonRelay);
    page += String(id);
    page += F("\">Relay ");
    page += String(id + 1);
    page += F("</label>\n\
<span id=\"");
    page += FPSTR(jsonRelayAutoRelease);
    page += String(id);
    page += F("\"></span>\n\
<p>\n");
  }
  page += "MAC - " + String(WiFi.macAddress()) + "<p>\n"; // добавлен MAC чтобы легче найти IP
  page += navigator();
  page += ESPWebBase::webPageEnd();

  httpServer->send(200, FPSTR(textHtml), page);
}

String ESPWebMQTTRelay::jsonData() {
  String result = ESPWebMQTTBase::jsonData();

  for (int8_t id = 0; id < maxRelays; id++) {
    result += F(",\"");
    result += FPSTR(jsonRelay);
    result += String(id);
    result += F("\":");
    if ((relayPin[id] != -1) && (digitalRead(relayPin[id]) == relayLevel[id]))
      result += FPSTR(bools[1]);
    else
      result += FPSTR(bools[0]);
    result += F(",\"");
    result += FPSTR(jsonRelayAutoRelease);
    result += String(id);
    result += F("\":");
    if (autoRelease[id])
      result += String((int32_t)(autoRelease[id] - millis()) / 1000);
    else
      result += '0';
  }
#ifdef USEDHT
  result += F(",\"");
  result += FPSTR(jsonTemperature);
  result += F("\":");
  result += isnan(dhtTemperature) ? F("\"?\"") : String(dhtTemperature);
  result += F(",\"");
  result += FPSTR(jsonHumidity);
  result += F("\":");
  result += isnan(dhtHumidity) ? F("\"?\"") : String(dhtHumidity);
#endif
#ifdef USEDS1820
  result += F(",\"");
  result += FPSTR(jsonTemperature2);
  result += F("\":");
  result += isnan(dsTemperature) ? F("\"?\"") : String(dsTemperature);
#endif
#ifdef USELDR
  result += F(",\"");
  result += FPSTR(jsonLDR);
  result += F("\":");
  result += String(ldr);
#endif

  return result;
}

void ESPWebMQTTRelay::handleIndexCss() {
  String style = F(".checkbox {\n\
vertical-align:top;\n\
margin:0 3px 0 0;\n\
width:17px;\n\
height:17px;\n\
}\n\
.checkbox + label {\n\
cursor:pointer;\n\
}\n\
.checkbox:not(checked) {\n\
position:absolute;\n\
opacity:0;\n\
}\n\
.checkbox:not(checked) + label {\n\
position:relative;\n\
padding:0 0 0 60px;\n\
}\n\
.checkbox:not(checked) + label:before {\n\
content:'';\n\
position:absolute;\n\
top:-4px;\n\
left:0;\n\
width:50px;\n\
height:26px;\n\
border-radius:13px;\n\
background:#CDD1DA;\n\
box-shadow:inset 0 2px 3px rgba(0,0,0,.2);\n\
}\n\
.checkbox:not(checked) + label:after {\n\
content:'';\n\
position:absolute;\n\
top:-2px;\n\
left:2px;\n\
width:22px;\n\
height:22px;\n\
border-radius:10px;\n\
background:#FFF;\n\
box-shadow:0 2px 5px rgba(0,0,0,.3);\n\
transition:all .2s;\n\
}\n\
.checkbox:checked + label:before {\n\
background:#9FD468;\n\
}\n\
.checkbox:checked + label:after {\n\
left:26px;\n\
}\n");

  httpServer->send(200, FPSTR(textCss), style);
}

void ESPWebMQTTRelay::handleIndexJs() {
  String script = FPSTR(getXmlHttpRequest);
  script += F("function openUrl(url) {\n\
var request = getXmlHttpRequest();\n\
request.open('GET', url, false);\n\
request.send(null);\n\
}\n\
function refreshData() {\n\
var request = getXmlHttpRequest();\n\
request.open('GET', '");
  script += FPSTR(pathData);
  script += F("?dummy=' + Date.now(), true);\n\
request.onreadystatechange = function() {\n\
if (request.readyState == 4) {\n\
var data = JSON.parse(request.responseText);\n");
  script += FPSTR(getElementById);
  script += FPSTR(jsonMQTTConnected);
  script += F("').innerHTML = (data.");
  script += FPSTR(jsonMQTTConnected);
  script += F(" != true ? \"not \" : \"\") + \"connected\";\n");
  script += FPSTR(getElementById);
  script += FPSTR(jsonFreeHeap);
  script += F("').innerHTML = data.");
  script += FPSTR(jsonFreeHeap);
  script += F(";\n");
  script += FPSTR(getElementById);
  script += FPSTR(jsonUptime);
  script += F("').innerHTML = data.");
  script += FPSTR(jsonUptime);
  script += F(";\n");
  for (int8_t id = 0; id < maxRelays; id++) {
    script += FPSTR(getElementById);
    script += FPSTR(jsonRelay);
    script += String(id);
    script += F("').checked = data.");
    script += FPSTR(jsonRelay);
    script += String(id);
    script += F(";\n\
if (data.");
    script += FPSTR(jsonRelayAutoRelease);
    script += String(id);
    script += F(" > 0)\n");
    script += FPSTR(getElementById);
    script += FPSTR(jsonRelayAutoRelease);
    script += String(id);
    script += F("').innerHTML = \" (\" + data.");
    script += FPSTR(jsonRelayAutoRelease);
    script += String(id);
    script += F(" + \" sec. to auto off)\";\n\
else\n");
    script += FPSTR(getElementById);
    script += FPSTR(jsonRelayAutoRelease);
    script += String(id);
    script += F("').innerHTML = \"\";\n");
  }
#ifdef USEDHT
  if ((dhtPin != -1) && (dht != NULL)) {
    script += FPSTR(getElementById);
    script += FPSTR(jsonTemperature);
    script += F("').innerHTML = data.");
    script += FPSTR(jsonTemperature);
    script += F(";\n");
    script += FPSTR(getElementById);
    script += FPSTR(jsonHumidity);
    script += F("').innerHTML = data.");
    script += FPSTR(jsonHumidity);
    script += F(";\n");
  }
#endif
#ifdef USEDS1820
  if ((dsPin != -1) && (ds != NULL)) {
    script += FPSTR(getElementById);
    script += FPSTR(jsonTemperature2);
    script += F("').innerHTML = data.");
    script += FPSTR(jsonTemperature2);
    script += F(";\n");
  }
#endif
#ifdef USELDR
  script += FPSTR(getElementById);
  script += FPSTR(jsonLDR);
  script += F("').innerHTML = data.");
  script += FPSTR(jsonLDR);
  script += F(";\n");
#endif
  script += F("}\n\
}\n\
request.send(null);\n\
}\n\
setInterval(refreshData, 500);\n");

  httpServer->send(200, FPSTR(applicationJavascript), script);
}

#ifdef USEIRREMOTE
void ESPWebMQTTRelay::handleIRData() {
  if (lastIRType == UNKNOWN)
    httpServer->send(204, FPSTR(textJson), strEmpty); // No content
  else {
    String page;

    page += charOpenBrace;
    page += charQuote;
    page += FPSTR(jsonIRType);
    page += F("\":");
    page += String(lastIRType);
    page += F(",\"");
    page += FPSTR(jsonIRValue);
    page += F("\":");
    page += String(lastIRValue);
    page += charCloseBrace;

    lastIRType = UNKNOWN;
    lastIRValue = 0;

    httpServer->send(200, FPSTR(textJson), page);
  }
}
#endif

void ESPWebMQTTRelay::handleRelayConfig() {
  String page = ESPWebBase::webPageStart(F("Relay Setup"));
  page += ESPWebBase::webPageBody();
  page += F("<form name=\"relay\" method=\"GET\" action=\"");
  page += FPSTR(pathStore);
  page += F("\">\n\
<table><caption><h3>Relay Setup</h3></caption>\n\
<tr><th>#</th><th>GPIO</th><th>Level</th><th>On boot</th><th>Auto release<sup>*</sup></th></tr>\n");

  for (int8_t id = 0; id < maxRelays; id++) {
    page += F("<tr><td>");
    page += String(id + 1);
    page += F("</td><td><select name=\"");
    page += FPSTR(paramGPIO);
    page += String(id);
    page += F("\" size=1>\n");

    for (byte i = 0; i < sizeof(gpios) / sizeof(gpios[0]); i++) {
      int8_t gpio = pgm_read_byte(gpios + i);
      page += F("<option value=\"");
      page += String(gpio);
      page += charQuote;
      if (relayPin[id] == gpio)
        page += F(" selected");
      page += charGreater;
      if (gpio == -1)
        page += FPSTR(strNone);
      else
        page += String(gpio);
      page += F("</option>\n");
    }
    page += F("</select></td>\n\
<td><input type=\"radio\" name=\"");
    page += FPSTR(paramLevel);
    page += String(id);
    page += F("\" value=\"1\" ");
    if (relayLevel[id])
      page += FPSTR(extraChecked);
    page += F(">HIGH\n\
<input type=\"radio\" name=\"");
    page += FPSTR(paramLevel);
    page += String(id);
    page += F("\" value=\"0\" ");
    if (! relayLevel[id])
      page += FPSTR(extraChecked);
    page += F(">LOW</td>\n\
<td><input type=\"radio\" name=\"");
    page += FPSTR(paramOnBoot);
    page += String(id);
    page += F("\" value=\"1\" ");
    if (relayOnBoot[id])
      page += FPSTR(extraChecked);
    page += F(">On\n\
<input type=\"radio\" name=\"");
    page += FPSTR(paramOnBoot);
    page += String(id);
    page += F("\" value=\"0\" ");
    if (! relayOnBoot[id])
      page += FPSTR(extraChecked);
    page += F(">Off</td>\n\
<td><input type=\"text\" name=\"");
    page += FPSTR(paramAutoRelease);
    page += String(id);
    page += F("\" value=\"");
    page += String(relayAutoRelease[id]);
    page += F("\" maxlength=10></td>\n\
</tr>\n");
  }
  page += F("</table>\n\
<sup>*</sup> time in milliseconds to auto off relay (0 to disable this feature)\n\
<p>\n");

  page += ESPWebBase::tagInput(FPSTR(typeSubmit), strEmpty, F("Save"));
  page += charLF;
  page += btnBack();
  page += ESPWebBase::tagInput(FPSTR(typeHidden), FPSTR(paramReboot), "1");
  page += F("\n\
</form>\n");
  page += ESPWebBase::webPageEnd();

  httpServer->send(200, FPSTR(textHtml), page);
}

void ESPWebMQTTRelay::handleControlConfig() {
#ifdef USEIRREMOTE
  static const char irTypes[][13] PROGMEM = { "NEC", "SONY", "RC5", "RC6", "DISH", "SHARP", "PANASONIC", "JVC", "SANYO", "MITSUBISHI", "SAMSUNG", "LG", "WHYNTER", "AIWA_RC_T501", "COOLIX" };

  String script = FPSTR(getXmlHttpRequest);
  script += F("var currentInput = null;\n\
var timeoutId;\n\
function refreshData() {\n\
if (currentInput) {\n\
var request = getXmlHttpRequest();\n\
request.open('GET', '");
  script += FPSTR(pathIRData);
  script += F("?dummy=' + Date.now(), true);\n\
request.onreadystatechange = function() {\n\
if (request.readyState == 4) {\n\
if (request.status == 200) {\n\
var data = JSON.parse(request.responseText);\n\
document.getElementsByName('");
  script += FPSTR(jsonIRType);
  script += F("')[0].value = data.");
  script += FPSTR(jsonIRType);
  script += F(";\n\
currentInput.value = data.");
  script += FPSTR(jsonIRValue);
  script += F(";\n\
}\n\
timeoutId = setTimeout(refreshData, 500);\n\
}\n\
}\n\
request.send(null);\n\
}\n\
}\n\
function gotFocus(control) {\n\
currentInput = control;\n\
refreshData();\n\
}\n\
function lostFocus() {\n\
currentInput = null;\n\
clearTimeout(timeoutId);\n\
}\n");
#endif

  String page = ESPWebBase::webPageStart(F("Control Setup"));
#ifdef USEIRREMOTE
  page += ESPWebBase::webPageScript(script);
#endif
  page += ESPWebBase::webPageBody();
  page += F("<form name=\"control\" method=\"GET\" action=\"");
  page += FPSTR(pathStore);
  page += F("\">\n\
<table><caption><h3>Button Setup</h3></caption>\n\
<tr><th>#</th><th>GPIO</th><th>Level</th><th>Switch</th><th>Debounce</th></tr>\n");

  for (int8_t id = 0; id < maxRelays; id++) {
    page += F("<tr><td>");
    page += String(id + 1);
    page += F("</td><td><select name=\"");
    page += FPSTR(paramBtnGPIO);
    page += String(id);
    page += F("\" size=1>\n");

    for (byte i = 0; i < sizeof(gpios) / sizeof(gpios[0]); i++) {
      int8_t gpio = pgm_read_byte(gpios + i);
      page += F("<option value=\"");
      page += String(gpio);
      page += charQuote;
      if (btnPin[id] == gpio)
        page += F(" selected");
      page += charGreater;
      if (gpio == -1)
        page += FPSTR(strNone);
      else
        page += String(gpio);
      page += F("</option>\n");
    }
    page += F("</select></td>\n\
<td><input type=\"radio\" name=\"");
    page += FPSTR(paramBtnLevel);
    page += String(id);
    page += F("\" value=\"1\" ");
    if (btnLevel[id] & 0x01)
      page += FPSTR(extraChecked);
    page += F(">HIGH\n\
<input type=\"radio\" name=\"");
    page += FPSTR(paramBtnLevel);
    page += String(id);
    page += F("\" value=\"0\" ");
    if (! (btnLevel[id] & 0x01))
      page += FPSTR(extraChecked);
    page += F(">LOW</td>\n\
<td><center><input type=\"checkbox\" name=\"");
    page += FPSTR(paramBtnSwitch);
    page += String(id);
    page += F("\" value=\"128\" ");
    if (btnLevel[id] & 0x80)
      page += FPSTR(extraChecked);
    page += F("></center></td>\n\
<td><input type=\"text\" name=\"");
    page += FPSTR(paramDebounce);
    page += String(id);
    page += F("\" value=\"");
    page += String(debounceTime[id]);
    page += F("\" maxlength=10></td>\n\
</tr>\n");
  }
  page += F("</table>\n\
<p>\n");
#ifdef USEIRREMOTE
  page += F("<table><caption><h3>IR Setup</h3></caption>\n\
<tr><th>GPIO</th><th>Type</th></tr>\n\
<tr><td><select name=\"");
  page += FPSTR(paramIRGPIO);
  page += F("\" size=1>\n");

  for (byte i = 0; i < sizeof(gpios) / sizeof(gpios[0]); i++) {
    int8_t gpio = pgm_read_byte(gpios + i);
    page += F("<option value=\"");
    page += String(gpio);
    page += charQuote;
    if (irPin == gpio)
      page += F(" selected");
    page += charGreater;
    if (gpio == -1)
      page += FPSTR(strNone);
    else
      page += String(gpio);
    page += F("</option>\n");
  }
  page += F("</select></td>\n\
<td><select name=\"");
  page += FPSTR(paramIRType);
  page += F("\" size=1>\n");

  for (byte i = 0; i < sizeof(irTypes) / sizeof(irTypes[0]); i++) {
    page += F("<option value=\"");
    page += String(i + 1);
    page += charQuote;
    if (irType == i + 1)
      page += F(" selected");
    page += charGreater;
    page += FPSTR(irTypes[i]);
    page += F("</option>\n");
  }
  page += F("</select></td>\n\
</tr></table>\n\
<p>\n\
<table><caption><h3>IR Button Setup</h3></caption>\n\
<tr><th>#</th><th>On</th><th>Toggle</th><th>Off</th></tr>\n");

  for (int8_t id = 0; id < maxRelays; id++) {
    page += F("<tr><td>");
    page += String(id + 1);
    page += F("</td><td><input type=\"text\" name=\"");
    page += FPSTR(paramIROn);
    page += String(id);
    page += F("\" value=\"");
    page += String(irOn[id]);
    page += F("\" maxlength=10 onfocus=\"gotFocus(this)\" onblur=\"lostFocus()\"></td>\n\
<td><input type=\"text\" name=\"");
    page += FPSTR(paramIRToggle);
    page += String(id);
    page += F("\" value=\"");
    page += String(irToggle[id]);
    page += F("\" maxlength=10 onfocus=\"gotFocus(this)\" onblur=\"lostFocus()\"></td>\n\
<td><input type=\"text\" name=\"");
    page += FPSTR(paramIROff);
    page += String(id);
    page += F("\" value=\"");
    page += String(irOff[id]);
    page += F("\" maxlength=10 onfocus=\"gotFocus(this)\" onblur=\"lostFocus()\"></td>\n\
</tr>\n");
  }
  page += F("</table>\n\
<p>\n");
#endif

  page += ESPWebBase::tagInput(FPSTR(typeSubmit), strEmpty, F("Save"));
  page += charLF;
  page += btnBack();
  page += ESPWebBase::tagInput(FPSTR(typeHidden), FPSTR(paramReboot), "1");
  page += F("\n\
</form>\n");
  page += ESPWebBase::webPageEnd();

  httpServer->send(200, FPSTR(textHtml), page);
}

void ESPWebMQTTRelay::handleRelaySwitch() {
  String id = httpServer->arg("id");
  String on = httpServer->arg("on");

  if (local_WEB_access) //Разрешать включать, если есть доступ к Web интерфейсу
  switchRelay(id.toInt(), on == "true");

  httpServer->send(200, FPSTR(textHtml), strEmpty);
}

void ESPWebMQTTRelay::handleScheduleConfig() {
  int8_t i;

  String style = F(".modal {\n\
display: none;\n\
position: fixed;\n\
z-index: 1;\n\
left: 0;\n\
top: 0;\n\
width: 100%;\n\
height: 100%;\n\
overflow: auto;\n\
background-color: rgb(0,0,0);\n\
background-color: rgba(0,0,0,0.4);\n\
}\n\
.modal-content {\n\
background-color: #fefefe;\n\
margin: 15% auto;\n\
padding: 20px;\n\
border: 1px solid #888;\n\
width: 400px;\n\
}\n\
.close {\n\
color: #aaa;\n\
float: right;\n\
font-size: 28px;\n\
font-weight: bold;\n\
}\n\
.close:hover,\n\
.close:focus {\n\
color: black;\n\
text-decoration: none;\n\
cursor: pointer;\n\
}\n\
.hidden {\n\
display: none;\n\
}\n");

  String script = FPSTR(getXmlHttpRequest);
  script += F("function loadData(form) {\n\
var request = getXmlHttpRequest();\n\
request.open('GET', '");
  script += FPSTR(pathGetSchedule);
  script += F("?id=' + form.id.value + '&dummy=' + Date.now(), false);\n\
request.send(null);\n\
if (request.status == 200) {\n\
var data = JSON.parse(request.responseText);\n\
form.");
  script += FPSTR(paramSchedulePeriod);
  script += F(".value = data.");
  script += FPSTR(jsonSchedulePeriod);
  script += F(";\n\
form.");
  script += FPSTR(paramScheduleHour);
  script += F(".value = data.");
  script += FPSTR(jsonScheduleHour);
  script += F(";\n\
form.");
  script += FPSTR(paramScheduleMinute);
  script += F(".value = data.");
  script += FPSTR(jsonScheduleMinute);
  script += F(";\n\
form.");
  script += FPSTR(paramScheduleSecond);
  script += F(".value = data.");
  script += FPSTR(jsonScheduleSecond);
  script += F(";\n\
if (data.");
  script += FPSTR(jsonSchedulePeriod);
  script += F(" == 3) {\n\
var weekdaysdiv = document.getElementById('weekdays');\n\
var elements = weekdaysdiv.getElementsByTagName('input');\n\
for (var i = 0; i < elements.length; i++) {\n\
if (elements[i].type == 'checkbox') {\n\
if ((data.");
  script += FPSTR(jsonScheduleWeekdays);
  script += F(" & elements[i].value) != 0)\n\
elements[i].checked = true;\n\
else\n\
elements[i].checked = false;\n\
}\n\
}\n\
form.");
  script += FPSTR(paramScheduleWeekdays);
  script += F(".value = data.");
  script += FPSTR(jsonScheduleWeekdays);
  script += F(";\n\
} else {\n\
form.");
  script += FPSTR(paramScheduleWeekdays);
  script += F(".value = 0;\n\
form.");
  script += FPSTR(paramScheduleDay);
  script += F(".value = data.");
  script += FPSTR(jsonScheduleDay);
  script += F(";\n\
form.");
  script += FPSTR(paramScheduleMonth);
  script += F(".value = data.");
  script += FPSTR(jsonScheduleMonth);
  script += F(";\n\
form.");
  script += FPSTR(paramScheduleYear);
  script += F(".value = data.");
  script += FPSTR(jsonScheduleYear);
  script += F(";\n\
}\n\
form.");
  script += FPSTR(paramScheduleRelay);
  script += F(".value = data.");
  script += FPSTR(jsonScheduleRelay);
  script += F(";\n\
var radios = document.getElementsByName('");
  script += FPSTR(paramScheduleTurn);
  script += F("');\n\
for (var i = 0; i < radios.length; i++) {\n\
if (radios[i].value == data.");
  script += FPSTR(jsonScheduleTurn);
  script += F(") radios[i].checked = true;\n\
}\n\
}\n\
}\n\
function openForm(form, id) {\n\
form.id.value = id;\n\
loadData(form);\n\
form.");
  script += FPSTR(paramSchedulePeriod);
  script += F(".onchange();\n\
document.getElementById(\"form\").style.display = \"block\";\n\
}\n\
function closeForm() {\n\
document.getElementById(\"form\").style.display = \"none\";\n\
}\n\
function checkNumber(field, minvalue, maxvalue) {\n\
var val = parseInt(field.value);\n\
if (isNaN(val) || (val < minvalue) || (val > maxvalue))\n\
return false;\n\
return true;\n\
}\n\
function validateForm(form) {\n\
if (form.");
  script += FPSTR(paramSchedulePeriod);
  script += F(".value > 0) {\n\
if ((form.");
  script += FPSTR(paramSchedulePeriod);
  script += F(".value > 2) && (! checkNumber(form.");
  script += FPSTR(paramScheduleHour);
  script += F(", 0, 23))) {\n\
alert(\"Wrong hour!\");\n\
form.");
  script += FPSTR(paramScheduleHour);
  script += F(".focus();\n\
return false;\n\
}\n\
if ((form.");
  script += FPSTR(paramSchedulePeriod);
  script += F(".value > 1) && (! checkNumber(form.");
  script += FPSTR(paramScheduleMinute);
  script += F(", 0, 59))) {\n\
alert(\"Wrong minute!\");\n\
form.");
  script += FPSTR(paramScheduleMinute);
  script += F(".focus();\n\
return false;\n\
}\n\
if (! checkNumber(form.");
  script += FPSTR(paramScheduleSecond);
  script += F(", 0, 59)) {\n\
alert(\"Wrong second!\");\n\
form.");
  script += FPSTR(paramScheduleSecond);
  script += F(".focus();\n\
return false;\n\
}\n\
if ((form.");
  script += FPSTR(paramSchedulePeriod);
  script += F(".value == 3) && (form.");
  script += FPSTR(paramScheduleWeekdays);
  script += F(".value == 0)) {\n\
alert(\"None of weekdays selected!\");\n\
return false;\n\
}\n\
if ((form.");
  script += FPSTR(paramSchedulePeriod);
  script += F(".value >= 4) && (! checkNumber(form.");
  script += FPSTR(paramScheduleDay);
  script += F(", 1, ");
  script += String(Schedule::LASTDAYOFMONTH);
  script += F("))) {\n\
alert(\"Wrong day!\");\n\
form.");
  script += FPSTR(paramScheduleDay);
  script += F(".focus();\n\
return false;\n\
}\n\
if ((form.");
  script += FPSTR(paramSchedulePeriod);
  script += F(".value >= 5) && (! checkNumber(form.");
  script += FPSTR(paramScheduleMonth);
  script += F(", 1, 12))) {\n\
alert(\"Wrong month!\");\n\
form.");
  script += FPSTR(paramScheduleMonth);
  script += F(".focus();\n\
return false;\n\
}\n\
if ((form.");
  script += FPSTR(paramSchedulePeriod);
  script += F(".value == 6) && (! checkNumber(form.");
  script += FPSTR(paramScheduleYear);
  script += F(", 2017, 2099))) {\n\
alert(\"Wrong year!\");\n\
form.");
  script += FPSTR(paramScheduleYear);
  script += F(".focus();\n\
return false;\n\
}\n\
if (! checkNumber(form.");
  script += FPSTR(paramScheduleRelay);
  script += F(", 0, ");
  script += String(maxRelays - 1);
  script += F(")) {\n\
alert(\"Wrong relay id!\");\n\
form.");
  script += FPSTR(paramScheduleRelay);
  script += F(".focus();\n\
return false;\n\
}\n\
var radios = document.getElementsByName('");
  script += FPSTR(paramScheduleTurn);
  script += F("');\n\
var checkedCount = 0;\n\
for (var i = 0; i < radios.length; i++) {\n\
if (radios[i].checked == true) checkedCount++;\n\
}\n\
if (checkedCount != 1) {\n\
alert(\"Wrong relay turn!\");\n\
return false;\n\
}\n\
}\n\
return true;\n\
}\n\
function periodChanged(period) {\n\
document.getElementById(\"time\").style.display = (period.value != 0) ? \"inline\" : \"none\";\n\
document.getElementById(\"hh\").style.display = (period.value > 2) ? \"inline\" : \"none\";\n\
document.getElementById(\"mm\").style.display = (period.value > 1) ? \"inline\" : \"none\";\n\
document.getElementById(\"weekdays\").style.display = (period.value == 3) ? \"block\" : \"none\";\n\
document.getElementById(\"date\").style.display = (period.value > 3) ? \"block\" : \"none\";\n\
document.getElementById(\"month\").style.display = (period.value > 4) ? \"inline\" : \"none\";\n\
document.getElementById(\"year\").style.display = (period.value == 6) ? \"inline\" : \"none\";\n\
document.getElementById(\"relay\").style.display = (period.value != 0) ? \"block\" : \"none\";\n\
}\n\
function weekChanged(wd) {\n\
var weekdays = document.form.");
  script += FPSTR(paramScheduleWeekdays);
  script += F(".value;\n\
if (wd.checked == \"\") weekdays &= ~wd.value; else weekdays |= wd.value;\n\
document.form.");
  script += FPSTR(paramScheduleWeekdays);
  script += F(".value = weekdays;\n\
}\n");

  String page = ESPWebBase::webPageStart(F("Schedule Setup"));
  page += ESPWebBase::webPageStyle(style);
  page += ESPWebBase::webPageScript(script);
  page += ESPWebBase::webPageBody();
  page += F("<table><caption><h3>Schedule Setup</h3></caption>\n\
<tr><th>#</th><th>Event</th><th>Next time</th><th>Relay</th></tr>\n");

  for (i = 0; i < maxSchedules; i++) {
    page += F("<tr><td><a href=\"#\" onclick=\"openForm(document.form, ");
    page += String(i);
    page += F(")\">");
    page += String(i + 1);
    page += F("</a></td><td>");
    page += schedule[i];
    page += F("</td><td>");
    page += schedule[i].nextTimeStr();
    page += F("</td><td>");
    if (schedule[i].period() != Schedule::NONE) {
      if ((scheduleRelay[i] & 0x3F) < maxRelays) {
        page += String((scheduleRelay[i] & 0x3F) + 1);
        if ((scheduleRelay[i] & 0x80) != 0)
          page += F(" toggle");
        else
          page += (scheduleRelay[i] & 0x40) != 0 ? F(" on") : F(" off");
      }
    }
    page += F("</td></tr>\n");
  }
  page += F("</table>\n\
<p>\n\
<i>Don't forget to save changes!</i>\n\
<p>\n");

  page += ESPWebBase::tagInput(FPSTR(typeButton), strEmpty, F("Save"), String(F("onclick=\"location.href='")) + String(FPSTR(pathStore)) + String(F("?reboot=0'\"")));
  page += charLF;
  page += btnBack();
  page += ESPWebBase::tagInput(FPSTR(typeHidden), FPSTR(paramReboot), "0");
  page += F("\n\
<div id=\"form\" class=\"modal\">\n\
<div class=\"modal-content\">\n\
<span class=\"close\" onclick=\"closeForm()\">&times;</span>\n\
<form name=\"form\" method=\"GET\" action=\"");
  page += FPSTR(pathSetSchedule);
  page += F("\" onsubmit=\"if (validateForm(this)) closeForm(); else return false;\">\n\
<input type=\"hidden\" name=\"id\" value=\"0\">\n\
<select name=\"");
  page += FPSTR(paramSchedulePeriod);
  page += F("\" size=\"1\" onchange=\"periodChanged(this)\">\n\
<option value=\"0\">Never!</option>\n\
<option value=\"1\">Every minute</option>\n\
<option value=\"2\">Every hour</option>\n\
<option value=\"3\">Every week</option>\n\
<option value=\"4\">Every month</option>\n\
<option value=\"5\">Every year</option>\n\
<option value=\"6\">Once</option>\n\
</select>\n\
<span id=\"time\" class=\"hidden\">at\n\
<span id=\"hh\" class=\"hidden\">");
  page += ESPWebBase::tagInput(typeText, FPSTR(paramScheduleHour), "0", F("size=\"2\" maxlength=\"2\""));
  page += F("\n:</span>\n\
<span id=\"mm\" class=\"hidden\">");
  page += ESPWebBase::tagInput(typeText, FPSTR(paramScheduleMinute), "0", F("size=\"2\" maxlength=\"2\""));
  page += F("\n:</span>\n");
  page += ESPWebBase::tagInput(typeText, FPSTR(paramScheduleSecond), "0", F("size=\"2\" maxlength=\"2\""));
  page += F("</span><br/>\n\
<div id=\"weekdays\" class=\"hidden\">\n\
<input type=\"hidden\" name=\"");
  page += FPSTR(paramScheduleWeekdays);
  page += F("\" value=\"0\">\n");

  for (i = 0; i < 7; i++) {
    page += F("<input type=\"checkbox\" value=\"");
    page += String(1 << i);
    page += F("\" onchange=\"weekChanged(this)\">");
    page += weekdayName(i);
    page += charLF;
  }
  page += F("</div>\n\
<div id=\"date\" class=\"hidden\">\n\
<select name=\"");
  page += FPSTR(paramScheduleDay);
  page += F("\" size=\"1\">\n");

  for (i = 1; i <= 31; i++) {
    page += F("<option value=\"");
    page += String(i);
    page += F("\">");
    page += String(i);
    page += F("</option>\n");
  }
  page += F("<option value=\"");
  page += String(Schedule::LASTDAYOFMONTH);
  page += F("\">Last</option>\n\
</select>\n\
day\n\
<span id=\"month\" class=\"hidden\">of\n\
<select name=\"");
  page += FPSTR(paramScheduleMonth);
  page += F("\" size=\"1\">\n");

  for (i = 1; i <= 12; i++) {
    page += F("<option value=\"");
    page += String(i);
    page += F("\">");
    page += monthName(i);
    page += F("</option>\n");
  }
  page += F("</select>\n\
</span>\n\
<span id=\"year\" class=\"hidden\">");
  page += ESPWebBase::tagInput(typeText, FPSTR(paramScheduleYear), "2017", F("size=\"4\" maxlength=\"4\""));
  page += F("</span>\n\
</div>\n\
<div id=\"relay\" class=\"hidden\">\n\
<label>Relay #</label>\n\
<select name=\"");
  page += FPSTR(paramScheduleRelay);
  page += F("\" size=\"1\">\n");

  for (i = 0; i < maxRelays; i++) {
    page += F("<option value=\"");
    page += String(i);
    page += F("\">");
    page += String(i + 1);
    page += F("</option>\n");
  }
  page += F("</select>\n\
turn\n\
<input type=\"radio\" name=\"");
  page += FPSTR(paramScheduleTurn);
  page += F("\" value=\"1\">ON\n\
<input type=\"radio\" name=\"");
  page += FPSTR(paramScheduleTurn);
  page += F("\" value=\"0\">OFF\n\
<input type=\"radio\" name=\"");
  page += FPSTR(paramScheduleTurn);
  page += F("\" value=\"2\">TOGGLE\n\
</div>\n\
<p>\n\
<input type=\"submit\" value=\"Update\">\n\
</form>\n\
</div>\n\
</div>\n");
  page += ESPWebBase::webPageEnd();

  httpServer->send(200, FPSTR(textHtml), page);
}

void ESPWebMQTTRelay::handleGetSchedule() {
  int id = -1;

  if (httpServer->hasArg("id"))
    id = httpServer->arg("id").toInt();

  if ((id >= 0) && (id < maxSchedules)) {
    String page;

    page += charOpenBrace;
    page += charQuote;
    page += FPSTR(jsonSchedulePeriod);
    page += F("\":");
    page += String(schedule[id].period());
    page += F(",\"");
    page += FPSTR(jsonScheduleHour);
    page += F("\":");
    page += String(schedule[id].hour());
    page += F(",\"");
    page += FPSTR(jsonScheduleMinute);
    page += F("\":");
    page += String(schedule[id].minute());
    page += F(",\"");
    page += FPSTR(jsonScheduleSecond);
    page += F("\":");
    page += String(schedule[id].second());
    page += F(",\"");
    page += FPSTR(jsonScheduleWeekdays);
    page += F("\":");
    page += String(schedule[id].weekdays());
    page += F(",\"");
    page += FPSTR(jsonScheduleDay);
    page += F("\":");
    page += String(schedule[id].day());
    page += F(",\"");
    page += FPSTR(jsonScheduleMonth);
    page += F("\":");
    page += String(schedule[id].month());
    page += F(",\"");
    page += FPSTR(jsonScheduleYear);
    page += F("\":");
    page += String(schedule[id].year());
    page += F(",\"");
    page += FPSTR(jsonScheduleRelay);
    page += F("\":");
    page += String(scheduleRelay[id] & 0x3F);
    page += F(",\"");
    page += FPSTR(jsonScheduleTurn);
    page += F("\":");
    page += String((scheduleRelay[id] >> 6) & 0x03);
    page += charCloseBrace;

    httpServer->send(200, FPSTR(textJson), page);
  } else {
    httpServer->send(204, FPSTR(textJson), strEmpty); // No content
  }
}

void ESPWebMQTTRelay::handleSetSchedule() {
  String argName, argValue;
  int8_t id = -1;
  Schedule::period_t period = Schedule::NONE;
  int8_t hour = -1;
  int8_t minute = -1;
  int8_t second = -1;
  uint8_t weekdays = 0;
  int8_t day = 0;
  int8_t month = 0;
  int16_t year = 0;
  int8_t relay = 0;
  uint8_t turn = 0;

  for (byte i = 0; i < httpServer->args(); i++) {
    argName = httpServer->argName(i);
    argValue = httpServer->arg(i);
    if (argName.equals("id")) {
      id = argValue.toInt();
    } else if (argName.equals(FPSTR(paramSchedulePeriod))) {
      period = (Schedule::period_t)argValue.toInt();
    } else if (argName.equals(FPSTR(paramScheduleHour))) {
      hour = argValue.toInt();
    } else if (argName.equals(FPSTR(paramScheduleMinute))) {
      minute = argValue.toInt();
    } else if (argName.equals(FPSTR(paramScheduleSecond))) {
      second = argValue.toInt();
    } else if (argName.equals(FPSTR(paramScheduleWeekdays))) {
      weekdays = argValue.toInt();
    } else if (argName.equals(FPSTR(paramScheduleDay))) {
      day = argValue.toInt();
    } else if (argName.equals(FPSTR(paramScheduleMonth))) {
      month = argValue.toInt();
    } else if (argName.equals(FPSTR(paramScheduleYear))) {
      year = argValue.toInt();
    } else if (argName.equals(FPSTR(paramScheduleRelay))) {
      relay = argValue.toInt();
    } else if (argName.equals(FPSTR(paramScheduleTurn))) {
      turn = argValue.toInt() & 0x03;
    } else {
      _log->print(F("Unknown parameter \""));
      _log->print(argName);
      _log->print(F("\"!"));
    }
  }

  if ((id >= 0) && (id < maxSchedules)) {
    if (period == Schedule::NONE)
      schedule[id].clear();
    else
      schedule[id].set(period, hour, minute, second, weekdays, day, month, year);
    scheduleRelay[id] = (relay & 0x3F) | (turn << 6);

    String page = ESPWebBase::webPageStart(F("Store Schedule"));
    page += F("<meta http-equiv=\"refresh\" content=\"1;URL=");
    page += FPSTR(pathSchedule);
    page += F("\">\n");
    page += ESPWebBase::webPageBody();
    page += F("Configuration stored successfully.\n\
Wait for 1 sec. to return to previous page.\n");
    page += ESPWebBase::webPageEnd();

    httpServer->send(200, FPSTR(textHtml), page);
  } else {
    httpServer->send(204, FPSTR(textHtml), strEmpty);
  }
}

#if defined(USEDHT) || defined(USEDS1820)
void ESPWebMQTTRelay::handleClimateConfig() {
#ifdef USEDHT
  static const char dhtTypes[][6] PROGMEM = { "DHT11", "DHT21", "DHT22" };
  static const uint8_t dhtTypeValues[] PROGMEM = { DHT11, DHT21, DHT22 };
#endif

  String page = ESPWebBase::webPageStart(F("Climate Setup"));
  page += ESPWebBase::webPageBody();
  page += F("<form name=\"climate\" method=\"GET\" action=\"");
  page += FPSTR(pathStore);
  page += F("\">\n");
#ifdef USEDHT
  page += F("<h3>DHT Setup</h3>\n\
<label>GPIO:</label><br/>\n\
<select name=\"");
  page += FPSTR(paramDHTGPIO);
  page += F("\" size=1>\n");

  for (byte i = 0; i < sizeof(gpios) / sizeof(gpios[0]); i++) {
    int8_t gpio = pgm_read_byte(gpios + i);
    page += F("<option value=\"");
    page += String(gpio);
    page += charQuote;
    if (dhtPin == gpio)
      page += F(" selected");
    page += charGreater;
    if (gpio == -1)
      page += FPSTR(strNone);
    else
      page += String(gpio);
    page += F("</option>\n");
  }
  page += F("</select><br/>\n\
<label>Type:</label><br/>\n\
<select name=\"");
  page += FPSTR(paramDHTType);
  page += F("\" size=1>\n");

  for (byte i = 0; i < sizeof(dhtTypes) / sizeof(dhtTypes[0]); i++) {
    page += F("<option value=\"");
    page += String(pgm_read_byte(dhtTypeValues + i));
    page += charQuote;
    if (dhtType == pgm_read_byte(dhtTypeValues + i))
      page += F(" selected");
    page += charGreater;
    page += FPSTR(dhtTypes[i]);
    page += F("</option>\n");
  }
  page += F("</select><br/>\n\
<label>Minimal temperature:</label></br>\n\
<input type=\"text\" name=\"");
  page += FPSTR(paramDHTMinTemp);
  page += F("\" value=\"");

  if (! isnan(dhtMinTemp))
    page += String(dhtMinTemp);
  page += F("\" maxlength=10>\n\
(leave blank if not used)</br>\n\
<label>Relay #</label>\n\
<select name=\"");
  page += FPSTR(paramDHTMinTempRelay);
  page += F("\" size=\"1\">\n");

  for (byte i = 0; i < maxRelays; ++i) {
    page += F("<option value=\"");
    page += String(i);
    page += charQuote;
    if ((dhtMinTempRelay & 0B00111111) == i)
      page += F(" selected");
    page += charGreater;
    page += String(i + 1);
    page += F("</option>\n");
  }
  page += F("</select>\n\
turn\n\
<input type=\"radio\" name=\"");
  page += FPSTR(paramDHTMinTempTurn);
  page += F("\" value=\"1\"");
  if ((dhtMinTempRelay & 0B11000000) == 0B01000000)
    page += F(" checked");
  page += F(">ON\n\
<input type=\"radio\" name=\"");
  page += FPSTR(paramDHTMinTempTurn);
  page += F("\" value=\"0\"");
  if ((dhtMinTempRelay & 0B11000000) == 0B00000000)
    page += F(" checked");
  page += F(">OFF\n\
<input type=\"radio\" name=\"");
  page += FPSTR(paramDHTMinTempTurn);
  page += F("\" value=\"2\"");
  if ((dhtMinTempRelay & 0B11000000) == 0B10000000)
    page += F(" checked");
  page += F(">TOGGLE\n\
</br>\n\
<label>Maximal temperature:</label></br>\n\
<input type=\"text\" name=\"");
  page += FPSTR(paramDHTMaxTemp);
  page += F("\" value=\"");

  if (! isnan(dhtMaxTemp))
    page += String(dhtMaxTemp);
  page += F("\" maxlength=10>\n\
(leave blank if not used)</br>\n\
<label>Relay #</label>\n\
<select name=\"");
  page += FPSTR(paramDHTMaxTempRelay);
  page += F("\" size=\"1\">\n");

  for (byte i = 0; i < maxRelays; ++i) {
    page += F("<option value=\"");
    page += String(i);
    page += charQuote;
    if ((dhtMaxTempRelay & 0B00111111) == i)
      page += F(" selected");
    page += charGreater;
    page += String(i + 1);
    page += F("</option>\n");
  }
  page += F("</select>\n\
turn\n\
<input type=\"radio\" name=\"");
  page += FPSTR(paramDHTMaxTempTurn);
  page += F("\" value=\"1\"");
  if ((dhtMaxTempRelay & 0B11000000) == 0B01000000)
    page += F(" checked");
  page += F(">ON\n\
<input type=\"radio\" name=\"");
  page += FPSTR(paramDHTMaxTempTurn);
  page += F("\" value=\"0\"");
  if ((dhtMaxTempRelay & 0B11000000) == 0B00000000)
    page += F(" checked");
  page += F(">OFF\n\
<input type=\"radio\" name=\"");
  page += FPSTR(paramDHTMaxTempTurn);
  page += F("\" value=\"2\"");
  if ((dhtMaxTempRelay & 0B11000000) == 0B10000000)
    page += F(" checked");
  page += F(">TOGGLE\n\
</br>\n");
#endif

#ifdef USEDS1820
  page += F("<h3>DS18x20 Setup</h3>\n\
<label>GPIO:</label><br/>\n\
<select name=\"");
  page += FPSTR(paramDSGPIO);
  page += F("\" size=1>\n");

  for (byte i = 0; i < sizeof(gpios) / sizeof(gpios[0]); i++) {
    int8_t gpio = pgm_read_byte(gpios + i);
    page += F("<option value=\"");
    page += String(gpio);
    page += charQuote;
    if (dsPin == gpio)
      page += F(" selected");
    page += charGreater;
    if (gpio == -1)
      page += FPSTR(strNone);
    else
      page += String(gpio);
    page += F("</option>\n");
  }
  page += F("</select><br/>\n\
<label>Minimal temperature:</label></br>\n\
<input type=\"text\" name=\"");
  page += FPSTR(paramDSMinTemp);
  page += F("\" value=\"");

  if (! isnan(dsMinTemp))
    page += String(dsMinTemp);
  page += F("\" maxlength=10>\n\
(leave blank if not used)</br>\n\
<label>Relay #</label>\n\
<select name=\"");
  page += FPSTR(paramDSMinTempRelay);
  page += F("\" size=\"1\">\n");

  for (byte i = 0; i < maxRelays; ++i) {
    page += F("<option value=\"");
    page += String(i);
    page += charQuote;
    if ((dsMinTempRelay & 0B00111111) == i)
      page += F(" selected");
    page += charGreater;
    page += String(i + 1);
    page += F("</option>\n");
  }
  page += F("</select>\n\
turn\n\
<input type=\"radio\" name=\"");
  page += FPSTR(paramDSMinTempTurn);
  page += F("\" value=\"1\"");
  if ((dsMinTempRelay & 0B11000000) == 0B01000000)
    page += F(" checked");
  page += F(">ON\n\
<input type=\"radio\" name=\"");
  page += FPSTR(paramDSMinTempTurn);
  page += F("\" value=\"0\"");
  if ((dsMinTempRelay & 0B11000000) == 0B00000000)
    page += F(" checked");
  page += F(">OFF\n\
<input type=\"radio\" name=\"");
  page += FPSTR(paramDSMinTempTurn);
  page += F("\" value=\"2\"");
  if ((dsMinTempRelay & 0B11000000) == 0B10000000)
    page += F(" checked");
  page += F(">TOGGLE\n\
</br>\n\
<label>Maximal temperature:</label></br>\n\
<input type=\"text\" name=\"");
  page += FPSTR(paramDSMaxTemp);
  page += F("\" value=\"");

  if (! isnan(dsMaxTemp))
    page += String(dsMaxTemp);
  page += F("\" maxlength=10>\n\
(leave blank if not used)</br>\n\
<label>Relay #</label>\n\
<select name=\"");
  page += FPSTR(paramDSMaxTempRelay);
  page += F("\" size=\"1\">\n");

  for (byte i = 0; i < maxRelays; ++i) {
    page += F("<option value=\"");
    page += String(i);
    page += charQuote;
    if ((dsMaxTempRelay & 0B00111111) == i)
      page += F(" selected");
    page += charGreater;
    page += String(i + 1);
    page += F("</option>\n");
  }
  page += F("</select>\n\
turn\n\
<input type=\"radio\" name=\"");
  page += FPSTR(paramDSMaxTempTurn);
  page += F("\" value=\"1\"");
  if ((dsMaxTempRelay & 0B11000000) == 0B01000000)
    page += F(" checked");
  page += F(">ON\n\
<input type=\"radio\" name=\"");
  page += FPSTR(paramDSMaxTempTurn);
  page += F("\" value=\"0\"");
  if ((dsMaxTempRelay & 0B11000000) == 0B00000000)
    page += F(" checked");
  page += F(">OFF\n\
<input type=\"radio\" name=\"");
  page += FPSTR(paramDSMaxTempTurn);
  page += F("\" value=\"2\"");
  if ((dsMaxTempRelay & 0B11000000) == 0B10000000)
    page += F(" checked");
  page += F(">TOGGLE\n\
</br>\n");
#endif
  page += F("<p>\n");

  page += ESPWebBase::tagInput(FPSTR(typeSubmit), strEmpty, F("Save"));
  page += charLF;
  page += btnBack();
  page += ESPWebBase::tagInput(FPSTR(typeHidden), FPSTR(paramReboot), "1");
  page += F("\n\
</form>\n");
  page += ESPWebBase::webPageEnd();

  httpServer->send(200, FPSTR(textHtml), page);
}
#endif

#ifdef USELDR
void ESPWebMQTTRelay::handleLDRConfig() {
  String page = ESPWebBase::webPageStart(F("LDR Setup"));
  page += ESPWebBase::webPageBody();
  page += F("<form name=\"ldr\" method=\"GET\" action=\"");
  page += FPSTR(pathStore);
  page += F("\">\n");
  page += F("<h3>LDR Setup</h3>\n\
<label>Minimal brightness:</label></br>\n\
<input type=\"text\" name=\"");
  page += FPSTR(paramLDRMinBright);
  page += F("\" value=\"");

  if (ldrMinBright != -1)
    page += String(ldrMinBright);
  page += F("\" maxlength=4>\n\
(leave blank if not used)</br>\n\
<label>Relay #</label>\n\
<select name=\"");
  page += FPSTR(paramLDRMinBrightRelay);
  page += F("\" size=\"1\">\n");

  for (byte i = 0; i < maxRelays; ++i) {
    page += F("<option value=\"");
    page += String(i);
    page += charQuote;
    if ((ldrMinBrightRelay & 0B00111111) == i)
      page += F(" selected");
    page += charGreater;
    page += String(i + 1);
    page += F("</option>\n");
  }
  page += F("</select>\n\
turn\n\
<input type=\"radio\" name=\"");
  page += FPSTR(paramLDRMinBrightTurn);
  page += F("\" value=\"1\"");
  if ((ldrMinBrightRelay & 0B11000000) == 0B01000000)
    page += F(" checked");
  page += F(">ON\n\
<input type=\"radio\" name=\"");
  page += FPSTR(paramLDRMinBrightTurn);
  page += F("\" value=\"0\"");
  if ((ldrMinBrightRelay & 0B11000000) == 0B00000000)
    page += F(" checked");
  page += F(">OFF\n\
<input type=\"radio\" name=\"");
  page += FPSTR(paramLDRMinBrightTurn);
  page += F("\" value=\"2\"");
  if ((ldrMinBrightRelay & 0B11000000) == 0B10000000)
    page += F(" checked");
  page += F(">TOGGLE\n\
</br>\n\
<label>Maximal brightness:</label></br>\n\
<input type=\"text\" name=\"");
  page += FPSTR(paramLDRMaxBright);
  page += F("\" value=\"");

  if (ldrMaxBright != -1)
    page += String(ldrMaxBright);
  page += F("\" maxlength=4>\n\
(leave blank if not used)</br>\n\
<label>Relay #</label>\n\
<select name=\"");
  page += FPSTR(paramLDRMaxBrightRelay);
  page += F("\" size=\"1\">\n");

  for (byte i = 0; i < maxRelays; ++i) {
    page += F("<option value=\"");
    page += String(i);
    page += charQuote;
    if ((ldrMaxBrightRelay & 0B00111111) == i)
      page += F(" selected");
    page += charGreater;
    page += String(i + 1);
    page += F("</option>\n");
  }
  page += F("</select>\n\
turn\n\
<input type=\"radio\" name=\"");
  page += FPSTR(paramLDRMaxBrightTurn);
  page += F("\" value=\"1\"");
  if ((ldrMaxBrightRelay & 0B11000000) == 0B01000000)
    page += F(" checked");
  page += F(">ON\n\
<input type=\"radio\" name=\"");
  page += FPSTR(paramLDRMaxBrightTurn);
  page += F("\" value=\"0\"");
  if ((ldrMaxBrightRelay & 0B11000000) == 0B00000000)
    page += F(" checked");
  page += F(">OFF\n\
<input type=\"radio\" name=\"");
  page += FPSTR(paramLDRMaxBrightTurn);
  page += F("\" value=\"2\"");
  if ((ldrMaxBrightRelay & 0B11000000) == 0B10000000)
    page += F(" checked");
  page += F(">TOGGLE\n\
</br>\n\
<p>\n");

  page += ESPWebBase::tagInput(FPSTR(typeSubmit), strEmpty, F("Save"));
  page += charLF;
  page += btnBack();
  page += ESPWebBase::tagInput(FPSTR(typeHidden), FPSTR(paramReboot), "0");
  page += F("\n\
</form>\n");
  page += ESPWebBase::webPageEnd();

  httpServer->send(200, FPSTR(textHtml), page);
}
#endif

String ESPWebMQTTRelay::navigator() {
  String result = btnWiFiConfig();
  result += btnTimeConfig();
  result += btnMQTTConfig();
  result += btnRelayConfig();
  result += btnControlConfig();
  result += btnScheduleConfig();
#if defined(USEDHT) || defined(USEDS1820)
  result += btnClimateConfig();
#endif
#ifdef USELDR
  result += btnLDRConfig();
#endif
  result += btnLog();
  result += btnReboot();

  return result;
}

String ESPWebMQTTRelay::btnRelayConfig() {
  String result = ESPWebBase::tagInput(FPSTR(typeButton), strEmpty, F("Relay Setup"), String(F("onclick=\"location.href='")) + String(FPSTR(pathRelay)) + String(F("'\"")));
  result += charLF;

  return result;
}

String ESPWebMQTTRelay::btnControlConfig() {
  String result = ESPWebBase::tagInput(FPSTR(typeButton), strEmpty, F("Control Setup"), String(F("onclick=\"location.href='")) + String(FPSTR(pathControl)) + String(F("'\"")));
  result += charLF;

  return result;
}

String ESPWebMQTTRelay::btnScheduleConfig() {
  String result = ESPWebBase::tagInput(FPSTR(typeButton), strEmpty, F("Schedule Setup"), String(F("onclick=\"location.href='")) + String(FPSTR(pathSchedule)) + String(F("'\"")));
  result += charLF;

  return result;
}

#if defined(USEDHT) || defined(USEDS1820)
String ESPWebMQTTRelay::btnClimateConfig() {
  String result = ESPWebBase::tagInput(FPSTR(typeButton), strEmpty, F("Climate Setup"), String(F("onclick=\"location.href='")) + String(FPSTR(pathClimate)) + String(F("'\"")));
  result += charLF;

  return result;
}
#endif

#ifdef USELDR
String ESPWebMQTTRelay::btnLDRConfig() {
  String result = ESPWebBase::tagInput(FPSTR(typeButton), strEmpty, F("LDR Setup"), String(F("onclick=\"location.href='")) + String(FPSTR(pathLDR)) + String(F("'\"")));
  result += charLF;

  return result;
}
#endif

void ESPWebMQTTRelay::mqttCallback(char* topic, byte* payload, unsigned int length) {
  ESPWebMQTTBase::mqttCallback(topic, payload, length);

  String _mqttTopic = FPSTR(mqttRelayTopic);
  //***********
  #ifdef CONFIRM_CONFIG_TOPIC
  _mqttTopic +=FPSTR(mqttRelayConfigTopic);
  #endif
  //***********
  char* topicBody = topic + _mqttClient.length();// + 1; // Skip "/ClientName" from topic
  //strncmp(str1, str2, num) Сравнивает первые num символов из строк str1 и str2. Возвращает 0, если эти участки одинаковы.
  if (! strncmp(topicBody, _mqttTopic.c_str(), _mqttTopic.length())) { 
    topicBody += _mqttTopic.length();
    if (*topicBody++ == charSlash) {
      int8_t id = 0;

      while ((*topicBody >= '0') && (*topicBody <= '9')) {
        id *= 10;
        id += *topicBody++ - '0';
      }
      if ((id > 0) && (id <= maxRelays)) {
        id--;
        switch ((char)payload[0]) {
          case '0':
            switchRelay(id, false);
            break;
          case '1':
            switchRelay(id, true);
            break;
          default:
            if (relayPin[id] != -1) {
              bool relay = digitalRead(relayPin[id]);
              if (! relayLevel[id])
                relay = ! relay;
              mqttPublish(String(topic), String(relay));
            }
        }
      } else {
        _log->println(F("Wrong relay index!"));
      }
    } else {
      _log->println(F("Unexpected topic!"));
    }
  } else {
    _log->println(F("Unexpected topic!"));
  }
}

void ESPWebMQTTRelay::mqttResubscribe() {
  String topic;
 
  if (_mqttClient != strEmpty) {
    //topic += charSlash; // 21/11/2023
    topic += _mqttClient;
  }

  topic += FPSTR(mqttRelayTopic);
  //**********
  #ifdef CONFIRM_CONFIG_TOPIC
     topic += FPSTR(mqttRelayConfirmTopic);
  #endif
  //*******
  topic += charSlash;

  for (int8_t i = 0; i < maxRelays; i++) {
    if (relayPin[i] != -1) {
      mqttPublish(topic + String(i + 1), String(digitalRead(relayPin[i]) == relayLevel[i]));
    }
  }

//***********************
#ifdef CONFIRM_CONFIG_TOPIC
if (_mqttClient != strEmpty) {
   // topic = charSlash; // 21/11/2023
    topic = _mqttClient;
  }
  topic += FPSTR(mqttRelayTopic);
  topic += FPSTR(mqttRelayConfigTopic);
  topic += charSlash;
#endif  
//***************************
  topic += charHash;
  mqttSubscribe(topic);
}

void ESPWebMQTTRelay::switchRelay(int8_t id, bool on) {
  if (((id < 0) || (id >= maxRelays)) || (relayPin[id] == -1))
    return;

  bool relay = digitalRead(relayPin[id]);

  if (! relayLevel[id])
    relay = ! relay;
  if (relay != on) {
    if (relayAutoRelease[id]) {
      if (on)
        autoRelease[id] = millis() + relayAutoRelease[id];
      else
        autoRelease[id] = 0;
    }

    digitalWrite(relayPin[id], relayLevel[id] == on);

    if (pubSubClient->connected() || MQTT_connect) {
      String topic;

      if (_mqttClient != strEmpty) {
        //topic += charSlash;// 21/11/2023
        topic += _mqttClient;
      }
      topic += FPSTR(mqttRelayTopic);
      //**********  добавляем топик подверждения
      #ifdef CONFIRM_CONFIG_TOPIC
      //if (id<3) 
      topic += FPSTR(mqttRelayConfirmTopic);
      #endif
      //*******
      topic += charSlash;
      topic += String(id + 1);
      mqttPublish(topic, String(on));
    }

    if (lastStates == (uint32_t)-1)
      lastStates = 0;
    else
      lastStates &= ~((uint32_t)1 << id);
    if (on)
      lastStates |= ((uint32_t)1 << id);
    writeRTCmemory();
  }
  // ** только для четвертого реле и только для индикации состояния режима замка "ПОСТОЯННО ОТКРЫТ"
  // if (relayPin[3] != -1 && (id==1 || id==2)) {
  //   if (id==1 && on) switchRelay(3, true);  // если включалось реле №2 (режим постоянно открыто) переключить индикацию (реле №4) во включено
  //   if (id==2 && on) switchRelay(3, false); // если включалось реле №3 (выход из режима постоянно открыто) переключить индикацию (реле №4) в выключено
  // }
  //*****************************
}

inline void ESPWebMQTTRelay::toggleRelay(int8_t id) {
  switchRelay(id, digitalRead(relayPin[id]) != relayLevel[id]);
}

bool ESPWebMQTTRelay::debounceRead(int8_t id, uint32_t debounceTime) {
  if (((id < 0) || (id >= maxRelays)) || (btnPin[id] == -1))
    return false;

  if (! debounceTime)
    return (digitalRead(btnPin[id]) == (btnLevel[id] & 0x01));

  if (digitalRead(btnPin[id]) == (btnLevel[id] & 0x01)) { // Button pressed
    uint32_t startTime = millis();

    while (millis() - startTime < debounceTime) {
      if (digitalRead(btnPin[id]) != (btnLevel[id] & 0x01))
        return false;
      delay(1);
    }

    return true;
  }

  return false;
}

uint16_t ESPWebMQTTRelay::readScheduleConfig(uint16_t offset) {
  if (offset) {
    Schedule::period_t period;
    int8_t hour;
    int8_t minute;
    int8_t second;
    uint8_t weekdays;
    int8_t day;
    int8_t month;
    int16_t year;

    for (int8_t i = 0; i < maxSchedules; ++i) {
      getEEPROM(offset, period);
      offset += sizeof(period);
      getEEPROM(offset, hour);
      offset += sizeof(hour);
      getEEPROM(offset, minute);
      offset += sizeof(minute);
      getEEPROM(offset, second);
      offset += sizeof(second);
      if (period == Schedule::WEEKLY) {
        getEEPROM(offset, weekdays);
        offset += sizeof(weekdays);
      } else {
        getEEPROM(offset, day);
        offset += sizeof(day);
        getEEPROM(offset, month);
        offset += sizeof(month);
        getEEPROM(offset, year);
        offset += sizeof(year);
      }
      getEEPROM(offset, scheduleRelay[i]);
      offset += sizeof(scheduleRelay[i]);

      if ((period == Schedule::NONE) || ((scheduleRelay[i] & 0x3F) >= maxRelays))
        schedule[i].clear();
      else
        schedule[i].set(period, hour, minute, second, weekdays, day, month, year);
    }
  }

  return offset;
}

uint16_t ESPWebMQTTRelay::writeScheduleConfig(uint16_t offset) {
  if (offset) {
    Schedule::period_t period;
    int8_t hour;
    int8_t minute;
    int8_t second;
    uint8_t weekdays;
    int8_t day;
    int8_t month;
    int16_t year;

    for (int8_t i = 0; i < maxSchedules; ++i) {
      period = schedule[i].period();
      hour = schedule[i].hour();
      minute = schedule[i].minute();
      second = schedule[i].second();
      if (period == Schedule::WEEKLY) {
        weekdays = schedule[i].weekdays();
      } else {
        day = schedule[i].day();
        month = schedule[i].month();
        year = schedule[i].year();
      }

      putEEPROM(offset, period);
      offset += sizeof(period);
      putEEPROM(offset, hour);
      offset += sizeof(hour);
      putEEPROM(offset, minute);
      offset += sizeof(minute);
      putEEPROM(offset, second);
      offset += sizeof(second);
      if (period == Schedule::WEEKLY) {
        putEEPROM(offset, weekdays);
        offset += sizeof(weekdays);
      } else {
        putEEPROM(offset, day);
        offset += sizeof(day);
        putEEPROM(offset, month);
        offset += sizeof(month);
        putEEPROM(offset, year);
        offset += sizeof(year);
      }
      putEEPROM(offset, scheduleRelay[i]);
      offset += sizeof(scheduleRelay[i]);
    }
  }

  return offset;
}

#ifdef USEDHT
void ESPWebMQTTRelay::publishTemperature() {
  if (pubSubClient->connected()) {
    String topic;

    if (_mqttClient != strEmpty) {
     // topic += charSlash; // 21/11/2023
      topic += _mqttClient;
    }
    topic += FPSTR(mqttTemperatureTopic);
    mqttPublish(topic, String(dhtTemperature));
  }
}

void ESPWebMQTTRelay::publishHumidity() {
  if (pubSubClient->connected()) {
    String topic;

    if (_mqttClient != strEmpty) {
      //topic += charSlash; // 21/11/2023
      topic += _mqttClient;
    }
    topic += FPSTR(mqttHumidityTopic);
    mqttPublish(topic, String(dhtHumidity));
  }
}
#endif

#ifdef USEDS1820
void ESPWebMQTTRelay::publishTemperature2() {
  if (pubSubClient->connected()) {
    String topic;

    if (_mqttClient != strEmpty) {
      //topic += charSlash; // 21/11/2023
      topic += _mqttClient;
    }
    topic += FPSTR(mqttTemperatureTopic2);
    mqttPublish(topic, String(dsTemperature));
  }
}
#endif

#ifdef USELDR
void ESPWebMQTTRelay::publishLDR() {
  if (pubSubClient->connected()) {
    String topic;

    if (_mqttClient != strEmpty) {
      //topic += charSlash; // 21/11/2023
      topic += _mqttClient;
    }
    topic += FPSTR(mqttLDRTopic);
    mqttPublish(topic, String(ldr));
  }
}
#endif

ESPWebMQTTRelay* app = new ESPWebMQTTRelay();

void setup() {
#ifndef NOSERIAL
  Serial.begin(115200); //, SERIAL_8N1, SERIAL_TX_ONLY);
  Serial.println();
#endif
  app->_setup();
  if (app->_gsmMode) Sim800_setup();
}

void loop() {
  app->_loop();
   if (app->_gsmMode) Sim800_loop();
}
