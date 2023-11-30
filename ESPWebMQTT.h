#ifndef __ESPWEBMQTT_H
#define __ESPWEBMQTT_H

#include "ESPWeb.h"
#include <PubSubClient.h>

#ifdef ESP8266 
  #include <ESP8266WiFi.h>
#else
  #include <WiFi.h>
#endif  

#ifdef ESP8266 
const char defMQTTClient[] PROGMEM = "ESP8266_"; // Префикс имени клиента для MQTT-брокера по умолчанию
#else
const char defMQTTClient[] PROGMEM = "ESP32_"; // Префикс имени клиента для MQTT-брокера по умолчанию
#endif

const uint16_t defMQTTPort = 1883; // Порт MQTT-брокера по умолчанию

const char pathMQTT[] PROGMEM = "/mqtt"; // Путь до страницы конфигурации параметров MQTT

// Имена JSON-переменных
const char jsonMQTTConnected[] PROGMEM = "mqttconnected";

// Имена параметров для Web-форм
const char paramMQTTServer[] PROGMEM = "mqttserver";
const char paramMQTTPort[] PROGMEM = "mqttport";
const char paramMQTTUser[] PROGMEM = "mqttuser";
const char paramMQTTPassword[] PROGMEM = "mqttpswd";
const char paramMQTTClient[] PROGMEM = "mqttclient";
//***** добавлено настройка LWT
  const char mqttDeviceStatusTopic[] PROGMEM = "/Status";
  const char mqttDeviceStatusOn[] PROGMEM = "online";
  const char mqttDeviceStatusOff[] PROGMEM = "offline";
  const char mqttDeviceIPTopic[] PROGMEM = "/LocalIP";
  const uint8_t mqttDeviceStatusQos = 1;
  const bool mqttDeviceStatusRetained = true;

// Организация стека номеров команд и текста команд для отправки в модуль 
// для предотвращения одновременной отправки команды в модем при выполнении текущей команды
const int max_queue = 30;
const int max_text_com = 350;
 typedef struct{
     int com;     // номер команды 
     int com_flag;    // флаг команды, для отслеживания ее выполнения при обработке сообщения "OK" от модема SIM800
     char text_com[max_text_com]; // максимальная длина строки команд - 556 символов
   } mod_com;

const char MQTT_type[15] PROGMEM = "MQTT";  //"MQIsdp";   // тип протокола НЕ ТРОГАТЬ !

class ESPWebMQTTBase : public ESPWebBase { // Расширение базового класса с поддержкой MQTT
public:
  ESPWebMQTTBase();

  PubSubClient* pubSubClient; // Клиент MQTT-брокера
  String _mqttServer; // MQTT-брокер
  uint16_t _mqttPort; // Порт MQTT-брокера
  String _mqttUser; // Имя пользователя для авторизации
  String _mqttPassword; // Пароль для авторизации  
  String _mqttClient; // Имя клиента для MQTT-брокера (используется при формировании имени топика для публикации в целях различия между несколькими клиентами с идентичным скетчем)
  virtual void mqttCallback(char* topic, byte* payload, unsigned int length); // Callback-функция, вызываемая MQTT-брокером при получении топика, на которое оформлена подписка
  virtual void add_in_queue_comand(int _inncomand, const char* _inn_text_comand, int _com_flag);
  QueueHandle_t queue_comand; // очередь передачи команд в модуль SIM800 размер - int8_t [max_queue]
 bool GPRS_ready = false; // признак подключения GPRS
 bool MQTT_connect = false; //признак подключения к MQTT серверу
 bool modemOK = false;  // признак работоспособности модема SIM800
 bool IsOpros = false; // признак однократной отправки Opros в модем
 bool TCP_ready=false;
 bool SIM_fatal_error=false; //признак не вставленной СИМ карты или полного сбоя GSM модема

protected:
  void setupExtra();
  void loopExtra();
  uint16_t readConfig();
  uint16_t writeConfig(bool commit = true);
  void defaultConfig(uint8_t level = 0);
  bool setConfigParam(const String& name, const String& value);
  void setupHttpServer();
  void handleRootPage();
  virtual void handleMQTTConfig(); // Обработчик страницы настройки параметров MQTT
  String jsonData(); // Формирование JSON-пакета данных

  virtual String btnMQTTConfig(); // HTML-код кнопки параметров MQTT
  String navigator();

  virtual bool mqttReconnect(); // Восстановление соединения с MQTT-брокером
 // virtual void mqttCallback(char* topic, byte* payload, unsigned int length); // Callback-функция, вызываемая MQTT-брокером при получении топика, на которое оформлена подписка
  virtual void mqttResubscribe(); // Осуществление подписки на топики
  bool mqttSubscribe(const String& topic); // Хэлпер для подписки на топик
  bool mqttPublish(const String& topic, const String& value); // Хэлпер для публикации топика

  virtual void waitingMQTT(); // Ожидание подключения к MQTT-брокеру (можно использовать для визуализации процесса ожидания)
  virtual void waitedMQTT(); // Окончание ожидания подключения к MQTT-брокеру

  virtual char *IPAddress2String(IPAddress ip);
  virtual void GPRS_MQTT_Reconnect();
  virtual void GPRS_MQTT_connect();
  virtual void GPRS_MQTT_pub (const String& _topic, const String& _messege);
  virtual void GPRS_MQTT_sub (const String& _topic);
  virtual void GPRS_MQTT_ping ();
  WiFiClient* _espClient;
};

#endif
