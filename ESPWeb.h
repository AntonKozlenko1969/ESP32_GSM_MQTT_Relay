#ifndef __ESPWEB_H
#define __ESPWEB_H

#include "ESPWebCfg.h"
#ifdef ESP8266 
  #include <ESP8266WebServer.h>
#else
  #include <WebServer.h>
#endif

#include "StringLog.h"

#define SIM800  Serial1
//uint8_t command_type =0; //тип отправленной в модем команды 1 - считать весь список телефонов с СИМ

#define DIGIT_IN_PHONENAMBER 8

// Односимвольные константы
const char charCR = '\r';
const char charLF = '\n';
const char charSlash = '/';
const char charHash = '#';
const char charSpace = ' ';
const char charDot = '.';
const char charComma = ',';
const char charColon = ':';
const char charSemicolon = ';';
const char charQuote = '"';
const char charApostroph = '\'';
const char charOpenBrace = '{';
const char charCloseBrace = '}';
const char charEqual = '=';
const char charLess = '<';
const char charGreater = '>';

const char* const strEmpty = "";
const char* const strSlash = "/";

const char defSSID[] PROGMEM = "ESP_GSMRelay_"; // Префикс имени точки доступа по умолчанию
const char defPassword[] PROGMEM = "P@$$w0rd"; // Пароль точки доступа по умолчанию
const char defNtpServer[] PROGMEM = "pool.ntp.org"; // NTP-сервер по умолчанию
const int8_t defNtpTimeZone = 3; // Временная зона по умолчанию (-11..13, +3 - Москва)
const uint32_t defNtpUpdateInterval = 3600000; // Интервал в миллисекундах для обновления времени с NTP-серверов (по умолчанию 1 час)

const char defwhiteListPhones[] PROGMEM = "123456789"; // Белый список телефонов максимум 3 номера по DIGIT_IN_PHONENAMBER симолов
const char defgprsapn[] PROGMEM = "wap.orange.md"; //GPRS APN по умолчанию

const char pathSPIFFS[] PROGMEM = "/spiffs"; // Путь до страницы просмотра содержимого SPIFFS
const char pathUpdate[] PROGMEM = "/update"; // Путь до страницы OTA-обновления
const char pathWiFi[] PROGMEM = "/wifi"; // Путь до страницы конфигурации параметров беспроводной сети
const char pathTime[] PROGMEM = "/time"; // Путь до страницы конфигурации параметров времени
const char pathGetTime[] PROGMEM = "/gettime"; // Путь до страницы получения JSON-пакета времени
const char pathSetTime[] PROGMEM = "/settime"; // Путь до страницы ручной установки времени
const char pathLog[] PROGMEM = "/log"; // Путь до страницы просмотра содержимого логов
const char pathClearLog[] PROGMEM = "/clearlog"; // Путь до страницы очистки логов
const char pathStore[] PROGMEM = "/store"; // Путь до страницы сохранения параметров
const char pathReboot[] PROGMEM = "/reboot"; // Путь до страницы перезагрузки
const char pathData[] PROGMEM = "/data"; // Путь до страницы получения JSON-пакета данных

const char textPlain[] PROGMEM = "text/plain";
const char textHtml[] PROGMEM = "text/html";
const char textJson[] PROGMEM = "text/json";
const char textCss[] PROGMEM = "text/css";
const char applicationJavascript[] PROGMEM = "application/javascript";

const char fileNotFound[] PROGMEM = "FileNotFound";
const char indexHtml[] PROGMEM = "index.html";

const char headerTitleOpen[] PROGMEM = "<!DOCTYPE html>\n\
<html>\n\
<head>\n\
<title>";
const char headerTitleClose[] PROGMEM = "</title>\n";
const char headerStyleOpen[] PROGMEM = "<style type=\"text/css\">\n";
const char headerStyleClose[] PROGMEM = "</style>\n";
const char headerStyleExtOpen[] PROGMEM = "<link rel=\"stylesheet\" href=\"";
const char headerStyleExtClose[] PROGMEM = "\">\n";
const char headerScriptOpen[] PROGMEM = "<script type=\"text/javascript\">\n";
const char headerScriptClose[] PROGMEM = "</script>\n";
const char headerScriptExtOpen[] PROGMEM = "<script type=\"text/javascript\" src=\"";
const char headerScriptExtClose[] PROGMEM = "\"></script>\n";
const char headerBodyOpen[] PROGMEM = "</head>\n\
<body";
const char footerBodyClose[] PROGMEM = "</body>\n\
</html>";
const char getXmlHttpRequest[] PROGMEM = "function getXmlHttpRequest() {\n\
var xmlhttp;\n\
try {\n\
xmlhttp = new ActiveXObject(\"Msxml2.XMLHTTP\");\n\
} catch (e) {\n\
try {\n\
xmlhttp = new ActiveXObject(\"Microsoft.XMLHTTP\");\n\
} catch (E) {\n\
xmlhttp = false;\n\
}\n\
}\n\
if ((! xmlhttp) && (typeof XMLHttpRequest != 'undefined')) {\n\
xmlhttp = new XMLHttpRequest();\n\
}\n\
return xmlhttp;\n\
}\n";
const char inputTypeOpen[] PROGMEM = "<input type=\"";
const char inputNameOpen[] PROGMEM = " name=\"";
const char inputValueOpen[] PROGMEM = " value=\"";
const char simpleTagClose[] PROGMEM = " />";
const char typeText[] PROGMEM = "text";
const char typePassword[] PROGMEM = "password";
const char typeRadio[] PROGMEM = "radio";
const char typeCheckbox[] PROGMEM = "checkbox";
const char typeButton[] PROGMEM = "button";
const char typeSubmit[] PROGMEM = "submit";
const char typeReset[] PROGMEM = "reset";
const char typeHidden[] PROGMEM = "hidden";
const char typeFile[] PROGMEM = "file";
const char extraChecked[] PROGMEM = "checked";
const char getElementById[] PROGMEM = "document.getElementById('";

const char selectTypeOpen[] PROGMEM = "<select type=\"";   //  **********  ДОБАВЛЕНО

// Имена JSON-переменных
const char jsonFreeHeap[] PROGMEM = "freeheap";
const char jsonUptime[] PROGMEM = "uptime";
const char jsonUnixTime[] PROGMEM = "unixtime";
const char jsonDate[] PROGMEM = "date";
const char jsonTime[] PROGMEM = "time";
const char jsonLog[] PROGMEM = "log";

const char bools[][6] PROGMEM = { "false", "true" };

// Имена параметров для Web-форм
const char paramApMode[] PROGMEM = "apmode";
const char paramSSID[] PROGMEM = "ssid";
const char paramPassword[] PROGMEM = "password";
const char paramDomain[] PROGMEM = "domain";
const char paramNtpServer1[] PROGMEM = "ntpserver1";
const char paramNtpServer2[] PROGMEM = "ntpserver2";
const char paramNtpServer3[] PROGMEM = "ntpserver3";
const char paramNtpTimeZone[] PROGMEM = "ntptimezone";
const char paramNtpUpdateInterval[] PROGMEM = "ntpupdateinterval";
const char paramTime[] PROGMEM = "time";
const char paramReboot[] PROGMEM = "reboot";
//Добавлено для использования GPRS
const char paramGSMMode[] PROGMEM = "gsmmode"; 
const char paramGPRS_apn[] PROGMEM = "gprsapn"; 
const char paramGPRS_user[] PROGMEM = "gprsuser"; 
const char paramGPRS_pwd[] PROGMEM = "gprspwd";

const char paramWhiteList[] PROGMEM = "whiteList"; // Параметр белых номеров 
const int total_bin_num = 2000; //2000 имеющихся в SPIFFS телефонной книге номеров
const uint16_t maxStringLen = 32; // Максимальная длина строковых параметров в Web-интерфейсе

class ESPWebBase { // Базовый класс
public:
  ESPWebBase();
  virtual void _setup(); // Метод должен быть вызван из функции setup() скетча // Изменено
  virtual void _loop(); // Метод должен быть вызван из функции loop() скетча // Изменено

  virtual void reboot(); // Перезагрузка модуля
  virtual String _CreateFile(uint8_t command_type); 
  virtual bool writeTXTstring(const String& file_num_string, uint8_t command_type); // добавить строку в указанный 1/2 текстовый файл
  virtual bool saveFile(const String& Fname); //процедура сохранения нового BIN файла с номерами телефонов в SPIFFS 

  int64_t phones_on_sim[total_bin_num]; //+ 2000 имеющихся в телефонной книге номеров
  virtual void readBINfile(); // прочитать двоичный файл и заполить номерами массив
  virtual void readTXTfile(); // прочитать текстовый файл и заполить номерами массив
  virtual void readTXTCSVfile(); // прочитать текстовый файл c 2000 номерами и заполить номерами массив и создать новый BIN файл

#ifdef ESP8266 
  ESP8266WebServer* httpServer; // Web-сервер
#else
  WebServer* httpServer; // Web-сервер
#endif
  // переменная открывающая доступ к WEB интерфейсу
  bool local_WEB_access = false; 

  char PhoneOnSIM[250][DIGIT_IN_PHONENAMBER];
  char CommentOnSIM[250][15];  
  uint8_t indexOnSim[250]; // индекс номера на СИМ карте, если 0 - записать в любой свободный индекс (только для записи на сим)
  int16_t alloc_num[3]={0,0,0}; //Количество имеющихся в телефонной книге номеров и общее возможное количество номеров
  // а также число сохраненных бинарных номеров из BIN файла
  bool _gsmMode; // использовать GSM модем или нет
  String _whiteListPhones; // Белый список телефонов максимум 3 номера по 8 симолов
  String _gprsapn; // Настройка GPRS соединения
  String _gprsuser;
  String _gprspwd;

  StringLog* _log; // Логи скетча 30/05/2025

protected:
  virtual void setupExtra(); // Дополнительный код инициализации
  virtual void loopExtra(); // Дополнительный код главного цикла

  virtual String getBoardId(); // Строковый идентификатор модуля ESP8266

  virtual uint16_t readRTCmemory(); // Чтение параметров из RTC-памяти ESP8266
  virtual uint16_t writeRTCmemory(); // Запись параметров в RTC-память ESP8266

  virtual uint8_t readEEPROM(uint16_t offset); // Чтение одного байта из EEPROM
  virtual void readEEPROM(uint16_t offset, uint8_t* buf, uint16_t len); // Чтение буфера из EEPROM
  virtual void writeEEPROM(uint16_t offset, uint8_t data); // Запись одного байта в EEPROM
  virtual void writeEEPROM(uint16_t offset, const uint8_t* buf, uint16_t len); // Запись буфера в EEPROM
  virtual uint16_t readEEPROMString(uint16_t offset, String& str, uint16_t maxlen); // Чтение строкового параметра из EEPROM, при успехе возвращает смещение следующего параметра
  virtual uint16_t writeEEPROMString(uint16_t offset, const String& str, uint16_t maxlen); // Запись строкового параметра в EEPROM, возвращает смещение следующего параметра
  template<typename T> T& getEEPROM(uint16_t offset, T& t) { // Шаблон чтения переменной из EEPROM
    readEEPROM(offset, (uint8_t*)&t, sizeof(T));
    return t;
  }
  template<typename T> const T& putEEPROM(uint16_t offset, const T& t) { // Шаблон записи переменной в EEPROM
    writeEEPROM(offset, (const uint8_t*)&t, sizeof(T));
    return t;
  }
  virtual void commitEEPROM(); // Завершает запись в EEPROM
  virtual uint8_t crc8EEPROM(uint16_t start, uint16_t end); // Вычисление 8-ми битной контрольной суммы участка EEPROM

  virtual uint16_t readConfig(); // Чтение конфигурационных параметров из EEPROM
  virtual uint16_t writeConfig(bool commit = true); // Запись конфигурационных параметров в EEPROM

  virtual void commitConfig(); // Подтверждение сохранения EEPROM
  virtual void defaultConfig(uint8_t level = 0); // Установление параметров в значения по умолчанию
  virtual bool setConfigParam(const String& name, const String& value); // Присвоение значений параметрам по их имени

  virtual bool setupWiFiAsStation(); // Настройка модуля в режиме инфраструктуры
  virtual void setupWiFiAsAP(); // Настройка модуля в режиме точки доступа
  virtual void setupWiFi(); // Попытка настройки модуля в заданный параметрами режим, при неудаче принудительный переход в режим точки доступа
  virtual void onWiFiConnected(); // Вызывается после активации беспроводной сети

  virtual void waitingWiFi(); // Ожидание подключения к инфраструктуре (можно использовать для визуализации процесса ожидания)
  virtual void waitedWiFi(); // Окончание ожидания подключения к инфраструктуре

  virtual uint32_t getTime(); // Возвращает время в формате UNIX-time с учетом часового пояса или 0, если ни разу не удалось получить точное время
  virtual void setTime(uint32_t now); // Ручная установка времени в формате UNIX-time
  virtual void logDate(uint32_t now = 0); // Записать в лог переданную или текущую дату
  virtual void logTime(uint32_t now = 0); // Записать в лог переданное или текущее время
  virtual void logDateTime(uint32_t now = 0); // Записать в лог переданные или текущие дату и время
  virtual void logTimeDate(uint32_t now = 0); // Записать в лог переданные или текущие время и дату

  virtual void setupHttpServer(); // Настройка Web-сервера (переопределяется для добавления обработчиков новых страниц)
  virtual void handleNotFound(); // Обработчик несуществующей страницы
  virtual void handleRootPage(); // Обработчик главной страницы
  virtual void handleFileUploaded(); // Обработчик страницы окончания загрузки файла в SPIFFS
  virtual void handleFileUpload(); // Обработчик страницы загрузки файла в SPIFFS
  virtual void handleFileDelete(); // Обработчик страницы удаления файла из SPIFFS
  virtual void handleSPIFFS(); // Обработчик страницы просмотра списка файлов в SPIFFS
  virtual void handleUpdate(); // Обработчик страницы выбора файла для OTA-обновления скетча
  virtual void handleSketchUpdated(); // Обработчик страницы окончания OTA-обновления скетча
  virtual void handleSketchUpdate(); // Обработчик страницы OTA-обновления скетча
  virtual void handleWiFiConfig(); // Обработчик страницы настройки параметров беспроводной сети
  virtual void handleTimeConfig(); // Обработчик страницы настройки параметров времени
  virtual void handleLog(); // Обработчик страницы просмотра логов
  virtual void handleClearLog(); // Обработчик страницы, очищающей логи
  virtual void handleStoreConfig(); // Обработчик страницы сохранения параметров
  virtual void handleReboot(); // Обработчик страницы перезагрузки модуля
  virtual void handleGetTime(); // Обработчик страницы, возвращающей JSON-пакет времени
  virtual void handleSetTime(); // Обработчик страницы ручной установки времени
  virtual void handleData(); // Обработчик страницы, возвращающей JSON-пакет данных
  virtual String jsonData(); // Формирование JSON-пакета данных

  virtual String btnBack(); // HTML-код кнопки "назад" для интерфейса
  virtual String btnWiFiConfig(); // HTML-код кнопки настройки параметров беспроводной сети
  virtual String btnTimeConfig(); // HTML-код кнопки настройки параметров времени
  virtual String btnLog(); // HTML-код кнопки просмотра логов
  virtual String btnReboot(); // HTML-код кнопки перезагрузки
  virtual String navigator(); // HTML-код кнопок интерфейса главной страницы

  virtual String getContentType(const String& fileName); // MIME-тип фала по его расширению
  virtual bool handleFileRead(const String& path); // Чтение файла из SPIFFS

  static String webPageStart(const String& title); // HTML-код заголовка Web-страницы
  static String webPageStyle(const String& style, bool file = false); // HTML-код стилевого блока или файла
  static String webPageScript(const String& script, bool file = false); // HTML-код скриптового блока или файла
  static String webPageBody(); // HTML-код заголовка тела страницы
  static String webPageBody(const String& extra); // HTML-код заголовка тела страницы с дополнительными параметрами
  static String webPageEnd(); // HTML-код завершения Web-страницы
  static String escapeQuote(const String& str); // Экранирование двойных кавычек для строковых значений в Web-формах
  virtual String tagInput(const String& type, const String& name, const String& value); // HTML-код для тэга INPUT
  virtual String tagInput(const String& type, const String& name, const String& value, const String& extra); // HTML-код для тэга INPUT с дополнительными параметрами
 
  //StringLog* _log; // Логи скетча
  bool _apMode; // Режим точки доступа (true) или инфраструктуры (false)
  String _ssid; // Имя сети или точки доступа
  String _password; // Пароль сети
  String _domain; // mDNS домен
  String _ntpServer1; // NTP-серверы
  String _ntpServer2;
  String _ntpServer3;
  int8_t _ntpTimeZone; // Временная зона (в часах от UTC)
  uint32_t _ntpUpdateInterval; // Период в миллисекундах для обновления времени с NTP-серверов
  static const char _signEEPROM[4] PROGMEM; // Сигнатура в начале EEPROM для определения, что параметры имеет смысл пытаться прочитать

#ifndef USEDS3231
private:
  uint32_t _lastNtpTime; // Последнее полученное от NTP-серверов время в формате UNIX-time
  uint32_t _lastNtpUpdate; // Значение millis() в момент последней синхронизации времени
#endif
};

#endif
