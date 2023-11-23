extern "C" {
#ifdef ESP8266  
  #include <sntp.h>
#else
  #include <esp_sntp.h>
#endif  
}
#include <pgmspace.h>

#ifdef ESP8266 
  #include <ESP8266WiFi.h>
  #include <WiFiUdp.h>
  #include <ESP8266mDNS.h>
  #include <FS.h>
#else
   #include <WiFi.h>
   #include <WiFiUdp.h>
   #include <ESPmDNS.h>
   #include "FS.h"
   #include "SPIFFS.h"
/* !!!!!! You only need to format SPIFFS the first time you run a
   test or else use the SPIFFS plugin to create a partition
   https://github.com/me-no-dev/arduino-esp32fs-plugin */
/* !!!! При первой загрузе скетча в ESP32 необходимо ОТФОРМАТИРОВАТЬ SPIFFS
    Для этого установить  FORMAT_SPIFFS_IF_FAILED true !!!!
    Потом исправить в false и загрузить скетч повторно !!!!! */
  #define FORMAT_SPIFFS_IF_FAILED true
  #include <Update.h>
#endif

#include "ESPWeb.h"

#ifdef USEDS3231
#include "DS3231.h"
#endif
#ifdef USEAT24C32
#include "AT24C32.h"
#else
#include <EEPROM.h>
#endif
#include "Date.h"

#ifdef ESP8266 
  #include "RTCmem.h"
#endif

static void halt() {
#ifndef NOBLED
  digitalWrite(LED_BUILTIN, LOW); // Зажигаем светодиод "смерти"
#endif
#ifndef NOSERIAL
  Serial.println(F("System halted!"));
  Serial.flush();
#endif
  while (true) {
    delay(1);
  }
}

/*
 *   ESPWebBase class implementation
 */

ESPWebBase::ESPWebBase() {
#ifdef ESP8266
  httpServer = new ESP8266WebServer(80);
#else
  httpServer = new WebServer(80);
#endif

#ifndef NOSERIAL
  _log = new StringLog(&Serial);
#else
  _log = new StringLog();
#endif
}

void ESPWebBase::_setup() {// Изменено
#ifndef NOBLED
  pinMode(LED_BUILTIN, OUTPUT);
#endif

#ifdef USEDS3231
  ds3231.init(pinSDA, pinSCL, fastI2C);
  if (! ds3231.begin()) {
#ifndef NOSERIAL
    Serial.println(F("DS3231 not detected!"));
#endif
    halt();
  }
#endif

#ifdef USEAT24C32
#ifndef USEDS3231
  at24c32.init(pinSDA, pinSCL, fastI2C); // Wire initialization must be single!
#endif
  if (! at24c32.begin()) {
#ifndef NOSERIAL
    Serial.println(F("AT24C32 EEPROM not detected!"));
#endif
    halt();
  }
#else
  EEPROM.begin(1024);
#endif

#ifndef NOEEPROMERASE
  if (analogRead(A0) > eepromEraseLevel) {
    uint32_t startTime = millis();
    bool eepromErase = true;
    while (millis() - startTime < eepromEraseTime) {
      if (analogRead(A0) < eepromEraseLevel) {
        eepromErase = false;
        break;
      }
      delay(1);
    }
    if (eepromErase) {
      for (uint16_t i = 0; i < 4096; ++i)
        writeEEPROM(i, 0xFF);
      commitEEPROM();
      _log->println(F("EEPROM erased succefully!"));
    }
  }
#endif

  if (! readConfig()) {
    _log->println(F("EEPROM is empty!"));
  }

#ifdef ESP8266 
  if (! readRTCmemory()) {
    _log->println(F("RTC memory is empty!"));
  }

  if (! SPIFFS.begin()) {
    _log->println(F("Unable to mount SPIFFS!"));
  }
#else
  if (! SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)) {
    _log->println(F("Unable to mount SPIFFS!"));
  }
#endif

  if (_ntpServer1.length() || _ntpServer2.length() || _ntpServer3.length()) {
    #ifdef ESP8266
    sntp_set_timezone(_ntpTimeZone); //Установка временного пояса для ESP8266
    #else
    //Установка временного пояса для ESP32 НЕ РАБОТАЕТ !!! часовой пояс надо устанавливать в веб интерфейсе
    //setenv("TZ","EET-2EEST,M3.5.0,M10.5.0/3",1); // #define TZ_Europe_Chisinau
    //setenv("TZ", "IST-2IDT,M3.4.4/26,M10.5.0",1); // #define TZ_Asia_Jerusalem	PSTR("IST-2IDT,M3.4.4/26,M10.5.0")
    //setenv("TZ", "<+03>-3",1); // #define TZ_Europe_Istanbul	PSTR("<+03>-3") 
    //setenv("TZ", "EET-2",1); // define TZ_Europe_Bucharest	PSTR("EET-2EEST,M3.5.0/3,M10.5.0/4")
    //tzset(); //Установка временного пояса для ESP32
    sntp_set_sync_interval(_ntpUpdateInterval); // установка периода синхронизации для ESP32
    sntp_setoperatingmode(SNTP_OPMODE_POLL);// установка спрособа синхронизации для ESP32
    #endif
    if (_ntpServer1.length())
      sntp_setservername(0, (char*)_ntpServer1.c_str());
    if (_ntpServer2.length())
      sntp_setservername(1, (char*)_ntpServer2.c_str());
    if (_ntpServer3.length())
      sntp_setservername(2, (char*)_ntpServer3.c_str());
    sntp_init();
  }

  setupExtra();

  setupWiFi();
  setupHttpServer();
}

void ESPWebBase::_loop() { // Изменено
  const uint32_t timeout = 7*60000; // 7 min.
  static uint32_t nextTime = timeout;
  // если указано работать как стнация, а подключения к сети нет, попробовать подключиться если такая сеть в зоне доступа
  if ((!_apMode) && (WiFi.status() != WL_CONNECTED) && ((WiFi.getMode() == WIFI_STA) || ((int32_t)(millis() - nextTime) >= 0))) {

  int nwifi = WiFi.scanNetworks(false,false,true,100U);
  bool WiFi_found = false;
     if (nwifi > 0) {
        String namewifi = "" ;
        String ssidwifi=_ssid.c_str();
        //_log->println(F("Scan WiFi environment."));
        for (int g = 0; g < nwifi; ++g) {
            // Print SSID and RSSI for each network found
            namewifi = WiFi.SSID(g); 
            if (ssidwifi==namewifi && _ssid.length()>0 && ssidwifi.length()>0) {
                WiFi_found = true; break;
             }
          }
       }

    if (WiFi_found) setupWiFi();
    nextTime = millis() + timeout;
  }

  httpServer->handleClient();
  loopExtra();

  delay(1); // For WiFi maintenance
}

void ESPWebBase::reboot() {
#ifndef NOBLED
  digitalWrite(LED_BUILTIN, LOW);
#endif
#ifndef NOSERIAL
  Serial.println(F("Rebooting..."));
  Serial.flush();
#endif
  ESP.restart();
}

void ESPWebBase::setupExtra() {
  // Stub
}

void ESPWebBase::loopExtra() {
  // Stub
}

String ESPWebBase::getBoardId() {
  String result;

  result=String(WiFi.macAddress()).substring(9);
  result=result.substring(0,2)+result.substring(3,5)+result.substring(6,8);

// #ifdef ESP8266 
//   result = String(ESP.getChipId(), HEX);
// #else
//   result = String(ESP.getEfuseMac()); // String(ESP.getChipModel()); //getChipId(), HEX);
// #endif
  result.toUpperCase();

  return result;
}

uint16_t ESPWebBase::readRTCmemory() {
 uint16_t offset = 0;

 _log->println(F("Reading RTC memory"));
#ifdef ESP8266 
 for (uint8_t i = 0; i < sizeof(ESPWebBase::_signEEPROM); ++i) {
   char c = pgm_read_byte(ESPWebBase::_signEEPROM + i);
   if (RTCmem.read(offset++) != c)
     break;
 }
 #endif
 if (offset < sizeof(ESPWebBase::_signEEPROM)) {
   _log->println(F("No signature found in RTC!"));
   return 0;
 }

 return offset;
}

uint16_t ESPWebBase::writeRTCmemory() {
 uint16_t offset = 0;

 _log->println(F("Writing config to RTC"));
#ifdef ESP8266 
 for (uint8_t i = 0; i < sizeof(ESPWebBase::_signEEPROM); ++i) {
   char c = pgm_read_byte(ESPWebBase::_signEEPROM + i);
   RTCmem.write(offset++, c);
 }
#endif
 return offset;
}

uint8_t ESPWebBase::readEEPROM(uint16_t offset) {
#ifdef USEAT24C32
  return at24c32.read(offset);
#else
  return EEPROM.read(offset);
#endif
}

void ESPWebBase::readEEPROM(uint16_t offset, uint8_t* buf, uint16_t len) {
#ifdef USEAT24C32
  at24c32.read(offset, buf, len);
#else
  while (len--)
    *buf++ = EEPROM.read(offset++);
#endif
}

void ESPWebBase::writeEEPROM(uint16_t offset, uint8_t data) {
#ifdef USEAT24C32
  at24c32.write(offset, data);
#else
  EEPROM.write(offset, data);
#endif
}

void ESPWebBase::writeEEPROM(uint16_t offset, const uint8_t* buf, uint16_t len) {
#ifdef USEAT24C32
  at24c32.write(offset, buf, len);
#else
  while (len--)
    EEPROM.write(offset++, *buf++);
#endif
}

uint16_t ESPWebBase::readEEPROMString(uint16_t offset, String& str, uint16_t maxlen) {
  str = strEmpty;
  for (uint16_t i = 0; i < maxlen; ++i) {
    char c = readEEPROM(offset + i);
    if (! c)
      break;
    else
      str += c;
  }

  return offset + maxlen;
}

uint16_t ESPWebBase::writeEEPROMString(uint16_t offset, const String& str, uint16_t maxlen) {
  int slen = str.length();

  for (uint16_t i = 0; i < maxlen; ++i) {
    if (i < slen)
      writeEEPROM(offset + i, str[i]);
    else
      writeEEPROM(offset + i, 0);
  }

  return offset + maxlen;
}

void ESPWebBase::commitEEPROM() {
#ifndef USEAT24C32
  EEPROM.commit();
#endif
}

uint8_t ESPWebBase::crc8EEPROM(uint16_t start, uint16_t end) {
  uint8_t crc = 0;

  while (start < end) {
    crc ^= readEEPROM(start++);

    for (uint8_t i = 0; i < 8; ++i)
      crc = crc & 0x80 ? (crc << 1) ^ 0x31 : crc << 1;
  }

  return crc;
}

uint16_t ESPWebBase::readConfig() {
  uint16_t offset = 0;

  _log->println(F("Reading config from EEPROM"));
  for (uint8_t i = 0; i < sizeof(ESPWebBase::_signEEPROM); ++i) {
    char c = pgm_read_byte(ESPWebBase::_signEEPROM + i);
    if (readEEPROM(offset++) != c)
      break;
  }
  if (offset < sizeof(ESPWebBase::_signEEPROM)) {
    defaultConfig();
    return 0;
  }
  getEEPROM(offset, _apMode);
  offset += sizeof(_apMode);
  getEEPROM(offset, _gsmMode);
  offset += sizeof(_gsmMode);  
  offset = readEEPROMString(offset, _ssid, maxStringLen);
  offset = readEEPROMString(offset, _password, maxStringLen);
  offset = readEEPROMString(offset, _domain, maxStringLen);
  offset = readEEPROMString(offset, _ntpServer1, maxStringLen);
  offset = readEEPROMString(offset, _ntpServer2, maxStringLen);
  offset = readEEPROMString(offset, _ntpServer3, maxStringLen);
  getEEPROM(offset, _ntpTimeZone);
  offset += sizeof(_ntpTimeZone);
  getEEPROM(offset, _ntpUpdateInterval);
  offset += sizeof(_ntpUpdateInterval);
  offset = readEEPROMString(offset, _whiteListPhones, maxStringLen);  //считать список из 3-х белых номеров по 8 симовлов
  uint8_t crc = crc8EEPROM(0, offset);
  if (readEEPROM(offset++) != crc) {
    _log->println(F("CRC mismatch! Use default WiFi parameters."));
    defaultConfig();
  }

   #ifndef NOSERIAL 
    Serial.print("SSID: ");  Serial.print(_ssid); 
    Serial.print(" pass: "); Serial.println(_password);              
   #endif  

  return offset;
}

uint16_t ESPWebBase::writeConfig(bool commit) {
  uint16_t offset = 0;

  _log->println(F("Writing config to EEPROM"));
  for (uint8_t i = 0; i < sizeof(ESPWebBase::_signEEPROM); ++i) {
    char c = pgm_read_byte(ESPWebBase::_signEEPROM + i);
    writeEEPROM(offset++, c);
  }
  putEEPROM(offset, _apMode);
  offset += sizeof(_apMode);
  putEEPROM(offset, _gsmMode);
  offset += sizeof(_gsmMode);  
  offset = writeEEPROMString(offset, _ssid, maxStringLen);
  offset = writeEEPROMString(offset, _password, maxStringLen);
  offset = writeEEPROMString(offset, _domain, maxStringLen);
  offset = writeEEPROMString(offset, _ntpServer1, maxStringLen);
  offset = writeEEPROMString(offset, _ntpServer2, maxStringLen);
  offset = writeEEPROMString(offset, _ntpServer3, maxStringLen);
  putEEPROM(offset, _ntpTimeZone);
  offset += sizeof(_ntpTimeZone);
  putEEPROM(offset, _ntpUpdateInterval);
  offset += sizeof(_ntpUpdateInterval);
  offset = writeEEPROMString(offset, _whiteListPhones, maxStringLen);  //Сохранить список из 3-х белых номеров
  uint8_t crc = crc8EEPROM(0, offset);
  writeEEPROM(offset++, crc);
  if (commit)
    commitConfig();

  return offset;
}

inline void ESPWebBase::commitConfig() {
  commitEEPROM();
}

void ESPWebBase::defaultConfig(uint8_t level) {
  if (level < 1) {
    _apMode = true;
    _gsmMode = false;
    _ssid = FPSTR(defSSID);
    _ssid += getBoardId();
    _password = FPSTR(defPassword);
    _domain = "Relay_" + getBoardId(); //String();
    _ntpServer1 = FPSTR(defNtpServer);
    _ntpServer2 = String();
    _ntpServer3 = String();
    _ntpTimeZone = defNtpTimeZone;
    _ntpUpdateInterval = defNtpUpdateInterval;
    _whiteListPhones = FPSTR(defwhiteListPhones);
  }
}

bool ESPWebBase::setConfigParam(const String& name, const String& value) {
  if (name.equals(FPSTR(paramApMode)))
    _apMode = constrain(value.toInt(), 0, 1);
  else if (name.equals(FPSTR(paramGSMMode)))
    _gsmMode = constrain(value.toInt(), 0, 1);    
  else if (name.equals(FPSTR(paramSSID)))
    _ssid = value;
  else if (name.equals(FPSTR(paramPassword)))
    _password = value;
  else if (name.equals(FPSTR(paramDomain)))
    _domain = value;
  else if (name.equals(FPSTR(paramNtpServer1)))
    _ntpServer1 = value;
  else if (name.equals(FPSTR(paramNtpServer2)))
    _ntpServer2 = value;
  else if (name.equals(FPSTR(paramNtpServer3)))
    _ntpServer3 = value;
  else if (name.equals(FPSTR(paramNtpTimeZone)))
    _ntpTimeZone = constrain(value.toInt(), -11, 13);
  else if (name.equals(FPSTR(paramNtpUpdateInterval)))
    _ntpUpdateInterval = _max(0, value.toInt()) * 1000;
  else if (name.equals(FPSTR(paramWhiteList))) // Параметр белых номеров 
    _whiteListPhones = value;   
  else
    return false;

  return true;
}

bool ESPWebBase::setupWiFiAsStation() {
  const uint32_t timeout = 20000; // 20 sec.
  uint32_t maxTime = millis() + timeout;

  if (! _ssid.length()) {
    _log->println(F("Empty SSID!"));
    return false;
  }

  _log->print(F("Connecting to \""));
  _log->print(_ssid);
  _log->print(charQuote);

  WiFi.mode(WIFI_STA);
  WiFi.begin(_ssid.c_str(), _password.c_str());

  while (WiFi.status() != WL_CONNECTED) {
    waitingWiFi();
    if ((int32_t)(millis() - maxTime) >= 0) {
      waitedWiFi();
      _log->println(F("FAIL!"));
      return false;
    }
  }
  waitedWiFi();
  _log->println(WiFi.localIP());

  return true;
}

void ESPWebBase::setupWiFiAsAP() {
  String ssid, password;

  if (_apMode) {
    ssid = _ssid;
    password = _password;
  } else {
    ssid = FPSTR(defSSID);
    ssid += getBoardId();
    password = FPSTR(defPassword);
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid.c_str(), password.c_str());

  _log->print(F("Configuring access point \""));
  _log->print(ssid);
  _log->print(F("\" with password \""));
  _log->print(password);
  _log->print(F("\" on IP address "));
  _log->println(WiFi.softAPIP());
}

void ESPWebBase::setupWiFi() {
  if (_apMode || (! setupWiFiAsStation()))
    setupWiFiAsAP();

  if (_domain.length()) {
    if (MDNS.begin(_domain.c_str())) {
      MDNS.addService("http", "tcp", 80);
      _log->println(F("mDNS responder started"));
    } else {
      _log->println(F("Error setting up mDNS responder!"));
    }
  }

  onWiFiConnected();
}

void ESPWebBase::onWiFiConnected() {
  httpServer->begin();
  _log->println(F("HTTP server started"));
}

void ESPWebBase::waitingWiFi() {
#ifndef NOBLED
  digitalWrite(LED_BUILTIN, ! digitalRead(LED_BUILTIN));
#endif
  delay(500);
  _log->print(charDot);
}

void ESPWebBase::waitedWiFi() {
#ifndef NOBLED
  digitalWrite(LED_BUILTIN, HIGH);
#endif
  _log->print(charSpace);
}

uint32_t ESPWebBase::getTime() {
#ifdef USEDS3231
  return ds3231.get();
#else
  if ((WiFi.getMode() == WIFI_STA) && (_ntpServer1.length() || _ntpServer2.length() || _ntpServer3.length()) && ((! _lastNtpTime) || (_ntpUpdateInterval && (millis() - _lastNtpUpdate >= _ntpUpdateInterval)))) {
  uint32_t now = 0;

  #ifdef ESP8266 
    now = sntp_get_current_timestamp();
  #else
    now = time(NULL)+_ntpTimeZone*60*60; // Added
   #endif  

    if (now > 1483228800UL) { // 01.01.2017 0:00:00
      _lastNtpTime = now;
      _lastNtpUpdate = millis();
      logDateTime(now);
      _log->println(F(" time updated successfully"));
    }
  }
  if (_lastNtpTime)
    return _lastNtpTime + (millis() - _lastNtpUpdate) / 1000;
  else
    return 0;
#endif
}

void ESPWebBase::setTime(uint32_t now) {
#ifdef USEDS3231
  ds3231.set(now);
#else
  _lastNtpTime = now;
  _lastNtpUpdate = millis();
#endif
  logDateTime(now);
  _log->println(F(" time updated manualy"));
}

void ESPWebBase::logDate(uint32_t now) {
  if (! now)
    now = getTime();
  if (now)
    _log->print(dateToStr(now));
}

void ESPWebBase::logTime(uint32_t now) {
  if (! now)
    now = getTime();
  if (now)
    _log->print(timeToStr(now));
}

void ESPWebBase::logDateTime(uint32_t now) {
  if (! now)
    now = getTime();
  if (now)
    _log->print(dateTimeToStr(now));
}

void ESPWebBase::logTimeDate(uint32_t now) {
  if (! now)
    now = getTime();
  if (now)
    _log->print(timeDateToStr(now));
}

void ESPWebBase::setupHttpServer() {
  httpServer->onNotFound(std::bind(&ESPWebBase::handleNotFound, this));
  httpServer->on(strSlash, HTTP_GET, std::bind(&ESPWebBase::handleRootPage, this));
  httpServer->on(String(String(charSlash) + FPSTR(indexHtml)).c_str(), HTTP_GET, std::bind(&ESPWebBase::handleRootPage, this));
  httpServer->on(String(FPSTR(pathSPIFFS)).c_str(), HTTP_GET, std::bind(&ESPWebBase::handleSPIFFS, this));
  httpServer->on(String(FPSTR(pathSPIFFS)).c_str(), HTTP_POST, std::bind(&ESPWebBase::handleFileUploaded, this), std::bind(&ESPWebBase::handleFileUpload, this));
  httpServer->on(String(FPSTR(pathSPIFFS)).c_str(), HTTP_DELETE, std::bind(&ESPWebBase::handleFileDelete, this));
  httpServer->on(String(FPSTR(pathUpdate)).c_str(), HTTP_GET, std::bind(&ESPWebBase::handleUpdate, this));
  httpServer->on(String(FPSTR(pathUpdate)).c_str(), HTTP_POST, std::bind(&ESPWebBase::handleSketchUpdated, this), std::bind(&ESPWebBase::handleSketchUpdate, this));
  httpServer->on(String(FPSTR(pathWiFi)).c_str(), HTTP_GET, std::bind(&ESPWebBase::handleWiFiConfig, this));
  httpServer->on(String(FPSTR(pathTime)).c_str(), HTTP_GET, std::bind(&ESPWebBase::handleTimeConfig, this));
  httpServer->on(String(FPSTR(pathStore)).c_str(), HTTP_GET, std::bind(&ESPWebBase::handleStoreConfig, this));
  httpServer->on(String(FPSTR(pathStore)).c_str(), HTTP_POST, std::bind(&ESPWebBase::handleStoreConfig, this));
  httpServer->on(String(FPSTR(pathLog)).c_str(), HTTP_GET, std::bind(&ESPWebBase::handleLog, this));
  httpServer->on(String(FPSTR(pathClearLog)).c_str(), HTTP_GET, std::bind(&ESPWebBase::handleClearLog, this));
  httpServer->on(String(FPSTR(pathGetTime)).c_str(), HTTP_GET, std::bind(&ESPWebBase::handleGetTime, this));
  httpServer->on(String(FPSTR(pathSetTime)).c_str(), HTTP_GET, std::bind(&ESPWebBase::handleSetTime, this));
  httpServer->on(String(FPSTR(pathReboot)).c_str(), HTTP_GET, std::bind(&ESPWebBase::handleReboot, this));
  httpServer->on(String(FPSTR(pathData)).c_str(), HTTP_GET, std::bind(&ESPWebBase::handleData, this));
}

void ESPWebBase::handleNotFound() {
  if (! handleFileRead(httpServer->uri()))
    httpServer->send(404, FPSTR(textPlain), FPSTR(fileNotFound));
}

void ESPWebBase::handleRootPage() {
  String script = FPSTR(getXmlHttpRequest);
  script += F("function refreshData() {\n\
var request = getXmlHttpRequest();\n\
request.open('GET', '");
  script += FPSTR(pathData);
  script += F("?dummy=' + Date.now(), true);\n\
request.onreadystatechange = function() {\n\
if (request.readyState == 4) {\n\
var data = JSON.parse(request.responseText);\n");
  script += FPSTR(getElementById);
  script += FPSTR(jsonFreeHeap);
  script += F("').innerHTML = data.");
  script += FPSTR(jsonFreeHeap);
  script += F(";\n");
  script += FPSTR(getElementById);
  script += FPSTR(jsonUptime);
  script += F("').innerHTML = data.");
  script += FPSTR(jsonUptime);
  script += F(";\n\
}\n\
}\n\
request.send(null);\n\
}\n\
setInterval(refreshData, 1000);\n");

  String page;
  #ifdef ESP8266 
  page = ESPWebBase::webPageStart(F("ESP8266"));
  #else
  page = ESPWebBase::webPageStart(F("ESP32"));
  #endif
  page += ESPWebBase::webPageScript(script);
  page += ESPWebBase::webPageBody();
  #ifdef ESP8266   
  page += F("<h3>ESP8266</h3>\n\
<p>\n\
Heap free size: <span id=\"");
  #else  
  page += F("<h3>ESP32</h3>\n\
<p>\n\
Heap free size: <span id=\"");
  #endif
  page += FPSTR(jsonFreeHeap);
  page += F("\">0</span> bytes<br/>\n\
Uptime: <span id=\"");
  page += FPSTR(jsonUptime);
  page += F("\">0</span> seconds\n\
<p>\n");
  page += navigator();
  page += ESPWebBase::webPageEnd();

  httpServer->send(200, FPSTR(textHtml), page);
}

void ESPWebBase::handleFileUploaded() {
  httpServer->send(200, FPSTR(textHtml), F("<META http-equiv=\"refresh\" content=\"2;URL=\">Upload successful."));
}

void ESPWebBase::handleFileUpload() {
  static File uploadFile;

  if (httpServer->uri() != FPSTR(pathSPIFFS))
    return;
  HTTPUpload& upload = httpServer->upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (! filename.startsWith(strSlash))
      filename = charSlash + filename;
    uploadFile = SPIFFS.open(filename, "w");
    filename = String();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile)
      uploadFile.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile)
      uploadFile.close();
  }
}

void ESPWebBase::handleFileDelete() {
  if (httpServer->args() == 0)
    return httpServer->send(500, FPSTR(textPlain), F("BAD ARGS"));
  String path = httpServer->arg(0);
  if (path == strSlash)
    return httpServer->send(500, FPSTR(textPlain), F("BAD PATH"));
  if (! SPIFFS.exists(path))
    return httpServer->send(404, FPSTR(textPlain), FPSTR(fileNotFound));
  SPIFFS.remove(path);
  httpServer->send(200, FPSTR(textPlain), strEmpty);
  path = String();
}

void ESPWebBase::handleSPIFFS() {
  String script = FPSTR(getXmlHttpRequest);
  script += F("function openUrl(url, method) {\n\
var request = getXmlHttpRequest();\n\
request.open(method, url, false);\n\
request.send(null);\n\
if (request.status != 200)\n\
alert(request.responseText);\n\
}\n\
function getSelectedCount() {\n\
var inputs = document.getElementsByTagName(\"input\");\n\
var result = 0;\n\
for (var i = 0; i < inputs.length; i++) {\n\
if (inputs[i].type == \"checkbox\") {\n\
if (inputs[i].checked == true)\n\
result++;\n\
}\n\
}\n\
return result;\n\
}\n\
function updateSelected() {\n\
document.getElementsByName(\"delete\")[0].disabled = (getSelectedCount() > 0) ? false : true;\n\
}\n\
function deleteSelected() {\n\
var inputs = document.getElementsByTagName(\"input\");\n\
for (var i = 0; i < inputs.length; i++) {\n\
if (inputs[i].type == \"checkbox\") {\n\
if (inputs[i].checked == true)\n\
openUrl(\"");
  script += FPSTR(pathSPIFFS);
  script += F("?filename=/\" + encodeURIComponent(inputs[i].value) + '&dummy=' + Date.now(), \"DELETE\");\n\
}\n\
}\n\
location.reload(true);\n\
}\n");

  String page = ESPWebBase::webPageStart(F("SPIFFS"));
  page += ESPWebBase::webPageScript(script);
  page += ESPWebBase::webPageBody();
  page += F("<form method=\"POST\" action=\"\" enctype=\"multipart/form-data\" onsubmit=\"if (document.getElementsByName('upload')[0].files.length == 0) { alert('No file to upload!'); return false; }\">\n\
<h3>SPIFFS</h3>\n\
<p>\n");
  int cnt = 0;
#ifdef ESP8266
  Dir dir = SPIFFS.openDir("/");
  while (dir.next()) {
    cnt++;
    String fileName = dir.fileName();
    size_t fileSize = dir.fileSize();
    if (fileName.startsWith(strSlash))
      fileName = fileName.substring(1);
    page += F("<input type=\"checkbox\" name=\"file");
    page += String(cnt);
    page += F("\" value=\"");
    page += fileName;
    page += F("\" onchange=\"updateSelected()\"><a href=\"/");
    page += fileName;
    page += F("\" download>");
    page += fileName;
    page += F("</a>\t");
    page += String(fileSize);
    page += F("<br/>\n");
  }   
#else
  File dir = SPIFFS.open("/");
  if (dir.isDirectory()) {
    File file= dir.openNextFile();
  while (file) {
    cnt++;
    String fileName = file.name(); //dir.fileName();
    size_t fileSize = file.size(); //dir.fileSize();
    if (fileName.startsWith(strSlash))
      fileName = fileName.substring(1);
    page += F("<input type=\"checkbox\" name=\"file");
    page += String(cnt);
    page += F("\" value=\"");
    page += fileName;
    page += F("\" onchange=\"updateSelected()\"><a href=\"/");
    page += fileName;
    page += F("\" download>");
    page += fileName;
    page += F("</a>\t");
    page += String(fileSize);
    page += F("<br/>\n");
    file= dir.openNextFile();
  }
 }
#endif

  page += String(cnt);
  page += F(" file(s)<br/>\n\
<p>\n");
  page += ESPWebBase::tagInput(FPSTR(typeButton), F("delete"), F("Delete"), F("onclick=\"if (confirm('Are you sure to delete selected file(s)?') == true) deleteSelected()\" disabled"));
  page += F("\n\
<p>\n\
Upload new file:<br/>\n");
  page += ESPWebBase::tagInput(FPSTR(typeFile), F("upload"), strEmpty);
  page += charLF;
  page += ESPWebBase::tagInput(FPSTR(typeSubmit), strEmpty, F("Upload"));
  page += F("\n\
</form>\n");

  page += ESPWebBase::webPageEnd();

  httpServer->send(200, FPSTR(textHtml), page);
}

void ESPWebBase::handleUpdate() {
  String page = ESPWebBase::webPageStart(F("Sketch Update"));
  page += ESPWebBase::webPageBody();
  page += F("<form method=\"POST\" action=\"\" enctype=\"multipart/form-data\" onsubmit=\"if (document.getElementsByName('update')[0].files.length == 0) { alert('No file to update!'); return false; }\">\n\
Select compiled sketch to upload:<br/>\n");
  page += ESPWebBase::tagInput(FPSTR(typeFile), F("update"), strEmpty);
  page += charLF;
  page += ESPWebBase::tagInput(FPSTR(typeSubmit), strEmpty, F("Update"));
  page += F("\n\
</form>\n");
  page += ESPWebBase::webPageEnd();

  httpServer->send(200, FPSTR(textHtml), page);
}

void ESPWebBase::handleSketchUpdated() {
  static const char updateFailed[] PROGMEM = "Update failed!";
  static const char updateSuccess[] PROGMEM = "<META http-equiv=\"refresh\" content=\"15;URL=\">Update successful! Rebooting...";

  httpServer->send(200, FPSTR(textHtml), Update.hasError() ? FPSTR(updateFailed) : FPSTR(updateSuccess));

  reboot();
}

void ESPWebBase::handleSketchUpdate() {
  if (httpServer->uri() != FPSTR(pathUpdate))
    return;
  HTTPUpload& upload = httpServer->upload();
  if (upload.status == UPLOAD_FILE_START) {
    //WiFiUDP::stopAll();
    _log->print(F("Update sketch from file \""));
    _log->print(upload.filename.c_str());
    _log->println(charQuote);
    uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
    if (! Update.begin(maxSketchSpace)) { // start with max available size
#ifndef NOSERIAL
      Update.printError(Serial);
#endif
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
#ifndef NOSERIAL
    Serial.print(charDot);
#endif
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
#ifndef NOSERIAL
      Update.printError(Serial);
#endif
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) { // true to set the size to the current progress
#ifndef NOSERIAL
      Serial.println();
#endif
      _log->print(F("Updated "));
      _log->print(upload.totalSize);
      _log->println(F(" byte(s) successful. Rebooting..."));
    } else {
#ifndef NOSERIAL
      Update.printError(Serial);
#endif
    }
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    Update.end();
#ifndef NOSERIAL
      Serial.println();
#endif
    _log->println(F("Update was aborted"));
  }
  yield();
}

void ESPWebBase::handleWiFiConfig() {
  String script = F("function validateForm(form) {\n\
if (form.");
  script += FPSTR(paramSSID);
  script += F(".value == \"\") {\n\
alert(\"SSID must be set!\");\n\
form.");
  script += FPSTR(paramSSID);
  script += F(".focus();\n\
return false;\n\
}\n\
if (form.");
  script += FPSTR(paramPassword);
  script += F(".value == \"\") {\n\
alert(\"Password must be set!\");\n\
form.");
  script += FPSTR(paramPassword);
  script += F(".focus();\n\
return false;\n\
}\n\
return true;\n\
}\n");

  String page = ESPWebBase::webPageStart(F("WiFi Setup"));
  page += ESPWebBase::webPageScript(script);
  page += ESPWebBase::webPageBody();
  page += F("<form name=\"wifi\" method=\"GET\" action=\"");
  page += FPSTR(pathStore);
  page += F("\" onsubmit=\"return validateForm(this)\">\n\
<h3>WiFi Setup</h3>\n\
<label>Mode:</label><br/>\n");
  if (_apMode)
    page += ESPWebBase::tagInput(FPSTR(typeRadio), FPSTR(paramApMode), "1", FPSTR(extraChecked));
  else
    page += ESPWebBase::tagInput(FPSTR(typeRadio), FPSTR(paramApMode), "1");
  page += F("AP\n");
  if (! _apMode)
    page += ESPWebBase::tagInput(FPSTR(typeRadio), FPSTR(paramApMode), "0", FPSTR(extraChecked));
  else
    page += ESPWebBase::tagInput(FPSTR(typeRadio), FPSTR(paramApMode), "0");
  page += F("Infrastructure\n<br/>\n\
<label>SSID:</label><br/>\n");
 //if (_apMode)
  //page += ESPWebBase::tagInput(FPSTR(typeText), FPSTR(paramSSID), _ssid, String(F("maxlength=")) + String(maxStringLen));
 //else{
 
  //Сканирование WiFi окружения
  page +=F("<select type=\"text\" name=\"");
  page += FPSTR(paramSSID);  page += F("\" >");
  int nwifi = WiFi.scanNetworks(false,false,true,100U);
     if (nwifi > 0) {
        String namewifi = "" ;
        const String ssidwifi = _ssid.c_str();
        _log->println(F("Scan WiFi environment."));
        for (int g = 0; g < nwifi; ++g) {
            // Print SSID and RSSI for each network found
            namewifi = WiFi.SSID(g); 
            _log->print(g + 1);
            _log->print(": ");
            _log->print(namewifi);
            _log->print(" (");
            _log->print(WiFi.RSSI(g));
            _log->print(")");            
            _log->println((WiFi.encryptionType(g) == WIFI_AUTH_OPEN)?" ":"*");
            page += F("\n<option maxlength="); 
            page.reserve(page.length() + 3);
            {String str1 = String(maxStringLen);
            page += str1; } 
             if (!_apMode && ssidwifi==namewifi && _ssid.length()>0 && ssidwifi.length()>0)
                 page +=F(" selected value=\"");
             else    
                 page +=F(" value=\"");
             page += namewifi; page += F("\" >"); page += namewifi +  " ";
             page += WiFi.RSSI(g);  page += F("dBm </option>");  
          }
         // vTaskDelay(25);
       }
      
   //** Добавить свое имя сети для выбора точки доступа ************
   const String boardID = getBoardId();
   page +="\n<option maxlength=";
    page.reserve(page.length() + 3);
  {String str2 = String(maxStringLen);
   page += str2; }  
   if (_apMode) page += F(" selected value=\"");
   else page += F(" value=\"");
   page += FPSTR(defSSID);
   page += boardID;
   page += F("\" >");
   page += FPSTR(defSSID);
   page += boardID;
   page += F("</option>");    
   //*****************************************
   page += F("\n</select>");
  //}
  page += F("\n*<br/>\n\
<label>Password:</label><br/>\n");
  page += ESPWebBase::tagInput(FPSTR(typePassword), FPSTR(paramPassword), _password, String(F("maxlength=")) + String(maxStringLen));
  page += F("\n*<br/>\n\
<label>mDNS domain:</label><br/>\n");
  page += ESPWebBase::tagInput(FPSTR(typeText), FPSTR(paramDomain), _domain, String(F("maxlength=")) + String(maxStringLen));
  page += F("\n\
.local (leave blank to ignore mDNS)\n\
<p>\n");
//*****  Добавлено для выбора подключен GSM модем или нет
  page += F("<label>USE GSM/GPRS Modem:</label><br/>\n");
  if (_gsmMode)
    page += ESPWebBase::tagInput(FPSTR(typeRadio), FPSTR(paramGSMMode), "1", FPSTR(extraChecked));
  else
    page += ESPWebBase::tagInput(FPSTR(typeRadio), FPSTR(paramGSMMode), "1");
  page += F("GSM modem connected\n");
  if (! _gsmMode)
    page += ESPWebBase::tagInput(FPSTR(typeRadio), FPSTR(paramGSMMode), "0", FPSTR(extraChecked));
  else
    page += ESPWebBase::tagInput(FPSTR(typeRadio), FPSTR(paramGSMMode), "0");
  page += F("NOT use GSM/GPRS modem\n<br/><p>\n");

 if (_gsmMode){
// Добавлено для сохранения 3-х белых номеров по 8 симолов
  page += F("<label>If use Lilygo T-CALL board NOT use GPIO 5 and GPIO 23 for Relay !</label><br/><p>\n");
  page += F("\n\
<label>White Phones List (max 3 numbers per ");
  page += DIGIT_IN_PHONENAMBER;
  page += F(" simbols) 123456789,123456789,123456789 :</label><br/>\n");
  page += ESPWebBase::tagInput(FPSTR(typeText), FPSTR(paramWhiteList), _whiteListPhones, String(F("maxlength=")) + String(2+DIGIT_IN_PHONENAMBER*3) + String(F(" size=")) + String(2+DIGIT_IN_PHONENAMBER*3));
  page += F("\n*<p>\n");
 }  
  page += ESPWebBase::tagInput(FPSTR(typeSubmit), strEmpty, F("Save"));
  page += charLF;
  page += btnBack();
  page += ESPWebBase::tagInput(FPSTR(typeHidden), FPSTR(paramReboot), "1");
  page += F("\n\
</form>\n");
  page += ESPWebBase::webPageEnd();
  httpServer->send(200, FPSTR(textHtml), page);
}

void ESPWebBase::handleTimeConfig() {
  String script = FPSTR(getXmlHttpRequest);
  script += F("function validateForm(form) {\n\
if (form.");
  script += FPSTR(paramNtpServer1);
  script += F(".value == \"\") {\n\
alert(\"NTP server #1 must be set!\");\n\
form.");
  script += FPSTR(paramNtpServer1);
  script += F(".focus();\n\
return false;\n\
}\n\
return true;\n\
}\n\
function openUrl(url) {\n\
var request = getXmlHttpRequest();\n\
request.open('GET', url, false);\n\
request.send(null);\n\
}\n\
function updateTime() {\n\
openUrl('");
  script += FPSTR(pathSetTime);
  script += F("?time=' + Math.floor(Date.now() / 1000) + '&dummy=' + Date.now());\n\
}\n\
function refreshData() {\n\
var request = getXmlHttpRequest();\n\
request.open('GET', '");
  script += FPSTR(pathGetTime);
  script += F("?dummy=' + Date.now(), true);\n\
request.onreadystatechange = function() {\n\
if (request.readyState == 4) {\n\
var data = JSON.parse(request.responseText);\n\
if (data.");
  script += FPSTR(jsonUnixTime);
  script += F(" == 0) {\n");
  script += FPSTR(getElementById);
  script += FPSTR(jsonDate);
  script += F("').innerHTML = \"Unset\";\n");
  script += FPSTR(getElementById);
  script += FPSTR(jsonTime);
  script += F("').innerHTML = \"\";\n\
} else {\n");
  script += FPSTR(getElementById);
  script += FPSTR(jsonDate);
  script += F("').innerHTML = data.");
  script += FPSTR(jsonDate);
  script += F(";\n");
  script += FPSTR(getElementById);
  script += FPSTR(jsonTime);
  script += F("').innerHTML = data.");
  script += FPSTR(jsonTime);
  script += F(";\n\
}\n\
}\n\
}\n\
request.send(null);\n\
}\n\
setInterval(refreshData, 1000);\n");

  String page = ESPWebBase::webPageStart(F("Time Setup"));
  page += ESPWebBase::webPageScript(script);
  page += ESPWebBase::webPageBody();
  page += F("<form name=\"time\" method=\"GET\" action=\"");
  page += FPSTR(pathStore);
  page += F("\" onsubmit=\"return validateForm(this)\">\n\
<h3>Time Setup</h3>\n\
Current date and time: <span id=\"");
  page += FPSTR(jsonDate);
  page += F("\"></span> <span id=\"");
  page += FPSTR(jsonTime);
  page += F("\"></span>\n\
<p>\n\
<label>NTP server #1:</label><br/>\n");
  page += ESPWebBase::tagInput(FPSTR(typeText), FPSTR(paramNtpServer1), _ntpServer1, String(F("maxlength=")) + String(maxStringLen));
  page += F("\n*<br/>\n\
<label>NTP server #2:</label><br/>\n");
  page += ESPWebBase::tagInput(FPSTR(typeText), FPSTR(paramNtpServer2), _ntpServer2, String(F("maxlength=")) + String(maxStringLen));
  page += F("\n<br/>\n\
<label>NTP server #3:</label><br/>\n");
  page += ESPWebBase::tagInput(FPSTR(typeText), FPSTR(paramNtpServer3), _ntpServer3, String(F("maxlength=")) + String(maxStringLen));
  page += F("\n<br/>\n\
<label>Time zone:</label><br/>\n\
<select name=\"");
  page += FPSTR(paramNtpTimeZone);
  page += F("\" size=1>\n");

  for (int8_t i = -11; i <= 13; i++) {
    page += F("<option value=\"");
    page += String(i);
    page += charQuote;
    if (_ntpTimeZone == i)
      page += F(" selected");
    page += charGreater;
    page += F("GMT");
    if (i > 0)
      page += '+';
    page += String(i);
    page += F("</option>\n");
  }
  page += F("</select>\n\
<br/>\n\
<label>Update interval (in sec.):</label><br/>\n");
  page += ESPWebBase::tagInput(FPSTR(typeText), FPSTR(paramNtpUpdateInterval), String(_ntpUpdateInterval / 1000), String(F("maxlength=10")));
  page += F("\n<p>\n");
  page += ESPWebBase::tagInput(FPSTR(typeButton), strEmpty, F("Update time from browser"), F("onclick='updateTime()'"));
  page += F("\n<p>\n");
  page += ESPWebBase::tagInput(FPSTR(typeSubmit), strEmpty, F("Save"));
  page += charLF;
  page += btnBack();
  page += ESPWebBase::tagInput(FPSTR(typeHidden), FPSTR(paramReboot), "1");
  page += F("\n\
</form>\n");
  page += ESPWebBase::webPageEnd();

  httpServer->send(200, FPSTR(textHtml), page);
}

void ESPWebBase::handleStoreConfig() {
  String argName, argValue;

  for (uint8_t i = 0; i < httpServer->args(); ++i) {
    argName = httpServer->argName(i);
    argValue = httpServer->arg(i);
    setConfigParam(argName, argValue);
  }

  writeConfig();

  String page = ESPWebBase::webPageStart(F("Store Setup"));
  page += F("<meta http-equiv=\"refresh\" content=\"5;URL=/\">\n");
  page += ESPWebBase::webPageBody();
  page += F("Configuration stored successfully.\n");
  if (httpServer->arg(FPSTR(paramReboot)) == "1")
    page += F("<br/>\n\
<i>You must reboot module to apply new configuration!</i>\n");
  page += F("<p>\n\
Wait for 5 sec. or click <a href=\"/\">this</a> to return to main page.\n");
  page += ESPWebBase::webPageEnd();

  httpServer->send(200, FPSTR(textHtml), page);
}

void ESPWebBase::handleLog() {
  String script = FPSTR(getXmlHttpRequest);
  script += F("function openUrl(url) {\n\
var request = getXmlHttpRequest();\n\
request.open('GET', url, false);\n\
request.send(null);\n\
}\n\
function clearLog() {\n\
openUrl('");
  script += FPSTR(pathClearLog);
  script += F("?dummy=' + Date.now());\n\
}\n");

  String page = ESPWebBase::webPageStart(F("Log View"));
  page += F("<META http-equiv=\"refresh\" content=\"2;URL=\">\n");
  page += ESPWebBase::webPageScript(script);
  page += ESPWebBase::webPageBody();
  page += F("<h3>Log View</h3>\n\
<textarea cols=\"80\" rows=\"25\" readonly id=\"");
  page += FPSTR(jsonLog);
  page += F("\">\n");
  page += _log->encodeStr(_log->text());
  page += F("</textarea>\n\
<p>\n");
  page += ESPWebBase::tagInput(FPSTR(typeButton), strEmpty, F("Clear!"), F("onclick=\"if (confirm('Are you sure to clear log?') == true) clearLog()\""));
  page += charLF;
  page += btnBack();
  page += ESPWebBase::webPageEnd();

  httpServer->send(200, FPSTR(textHtml), page);
}

void ESPWebBase::handleClearLog() {
  _log->clear();

  httpServer->send(200, FPSTR(textHtml), strEmpty);
}

void ESPWebBase::handleGetTime() {
  uint32_t now = getTime();
  String page;

  page += charOpenBrace;
  page += charQuote;
  page += FPSTR(jsonUnixTime);
  page += F("\":");
  page += String(now);
  if (now) {
    int8_t hh, mm, ss;
    uint8_t wd;
    int8_t d, m;
    int16_t y;

    parseUnixTime(now, hh, mm, ss, wd, d, m, y);
    page += F(",\"");
    page += FPSTR(jsonDate);
    page += F("\":\"");
    page += dateToStr(d, m, y);
    page += F("\",\"");
    page += FPSTR(jsonTime);
    page += F("\":\"");
    page += timeToStr(hh, mm, ss);
    page += charQuote;
  }
  page += charCloseBrace;

  httpServer->send(200, FPSTR(textJson), page);
}

void ESPWebBase::handleSetTime() {
  setTime(_max(0, httpServer->arg(FPSTR(paramTime)).toInt()) + _ntpTimeZone * 3600);

  httpServer->send(200, FPSTR(textHtml), strEmpty);
}

void ESPWebBase::handleReboot() {
  String page = ESPWebBase::webPageStart(F("Reboot"));
  page += F("<meta http-equiv=\"refresh\" content=\"5;URL=/\">\n");
  page += ESPWebBase::webPageBody();
  page += F("Rebooting...\n");
  page += ESPWebBase::webPageEnd();

  httpServer->send(200, FPSTR(textHtml), page);

  reboot();
}

void ESPWebBase::handleData() {
  String page;

  page += charOpenBrace;
  page += jsonData();
  page += charCloseBrace;

  httpServer->send(200, FPSTR(textJson), page);
}

String ESPWebBase::jsonData() {
  String result;

  result += charQuote;
  result += FPSTR(jsonFreeHeap);
  result += F("\":");
  result += String(ESP.getFreeHeap());
  result += F(",\"");
  result += FPSTR(jsonUptime);
  result += F("\":");
  result += String(millis() / 1000);

  return result;
}

String ESPWebBase::btnBack() {
//  String result = ESPWebBase::tagInput(FPSTR(typeButton), strEmpty, F("Back"), F("onclick=\"history.back()\""));
  String result = ESPWebBase::tagInput(FPSTR(typeButton), strEmpty, F("Back"), F("onclick=\"location.href='/'\""));
  result += charLF;

  return result;
}

String ESPWebBase::btnWiFiConfig() {
  String result = ESPWebBase::tagInput(FPSTR(typeButton), strEmpty, F("WiFi Setup"), String(F("onclick=\"location.href='")) + String(FPSTR(pathWiFi)) + String(F("'\"")));
  result += charLF;

  return result;
}

String ESPWebBase::btnTimeConfig() {
  String result = ESPWebBase::tagInput(FPSTR(typeButton), strEmpty, F("Time Setup"), String(F("onclick=\"location.href='")) + String(FPSTR(pathTime)) + String(F("'\"")));
  result += charLF;

  return result;
}

String ESPWebBase::btnLog() {
  String result = ESPWebBase::tagInput(FPSTR(typeButton), strEmpty, F("Log View"), String(F("onclick=\"location.href='")) + String(FPSTR(pathLog)) + String(F("'\"")));
  result += charLF;

  return result;
}

String ESPWebBase::btnReboot() {
  String result = ESPWebBase::tagInput(FPSTR(typeButton), strEmpty, F("Reboot!"), String(F("onclick=\"if (confirm('Are you sure to reboot?')) location.href='")) + String(FPSTR(pathReboot)) + String(F("'\"")));
  result += charLF;

  return result;
}

String ESPWebBase::navigator() {
  String result = btnWiFiConfig();
  result += btnTimeConfig();
  result += btnLog();
  result += btnReboot();

  return result;
}

String ESPWebBase::getContentType(const String& fileName) {
  if (httpServer->hasArg(F("download")))
    return String(F("application/octet-stream"));
  else if (fileName.endsWith(F(".htm")) || fileName.endsWith(F(".html")))
    return String(FPSTR(textHtml));
  else if (fileName.endsWith(F(".css")))
    return String(FPSTR(textCss));
  else if (fileName.endsWith(F(".js")))
    return String(FPSTR(applicationJavascript));
  else if (fileName.endsWith(F(".png")))
    return String(F("image/png"));
  else if (fileName.endsWith(F(".gif")))
    return String(F("image/gif"));
  else if (fileName.endsWith(F(".jpg")) || fileName.endsWith(F(".jpeg")))
    return String(F("image/jpeg"));
  else if (fileName.endsWith(F(".ico")))
    return String(F("image/x-icon"));
  else if (fileName.endsWith(F(".xml")))
    return String(F("text/xml"));
  else if (fileName.endsWith(F(".pdf")))
    return String(F("application/x-pdf"));
  else if (fileName.endsWith(F(".zip")))
    return String(F("application/x-zip"));
  else if (fileName.endsWith(F(".gz")))
    return String(F("application/x-gzip"));

  return String(FPSTR(textPlain));
}

bool ESPWebBase::handleFileRead(const String& path) {
  String fileName = path;
  if (fileName.endsWith(strSlash))
    fileName += FPSTR(indexHtml);
  String contentType = getContentType(fileName);
  String pathWithGz = fileName + F(".gz");
  if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(fileName)) {
    if (SPIFFS.exists(pathWithGz))
      fileName = pathWithGz;
    File file = SPIFFS.open(fileName, "r");
    size_t sent = httpServer->streamFile(file, contentType);
    file.close();

    return true;
  }

  return false;
}

String ESPWebBase::webPageStart(const String& title) {
  String result = FPSTR(headerTitleOpen);
  result += title;
  result += FPSTR(headerTitleClose);

  return result;
}

String ESPWebBase::webPageStyle(const String& style, bool file) {
  String result;

  if (file) {
    result = FPSTR(headerStyleExtOpen);
    result += style;
    result += FPSTR(headerStyleExtClose);
  } else {
    result = FPSTR(headerStyleOpen);
    result += style;
    result += FPSTR(headerStyleClose);
  }

  return result;
}

String ESPWebBase::webPageScript(const String& script, bool file) {
  String result;

  if (file) {
    result = FPSTR(headerScriptExtOpen);
    result += script;
    result += FPSTR(headerScriptExtClose);
  } else {
    result = FPSTR(headerScriptOpen);
    result += script;
    result += FPSTR(headerScriptClose);
  }

  return result;
}

String ESPWebBase::webPageBody() {
  String result = FPSTR(headerBodyOpen);
  result += charGreater;
  result += charLF;

  return result;
}

String ESPWebBase::webPageBody(const String& extra) {
  String result = FPSTR(headerBodyOpen);
  result += charSpace;
  result += extra;
  result += charGreater;
  result += charLF;

  return result;
}

String ESPWebBase::webPageEnd() {
  String result = FPSTR(footerBodyClose);

  return result;
}

String ESPWebBase::escapeQuote(const String& str) {
  String result;
  int start = 0, pos;

  while (start < str.length()) {
    pos = str.indexOf(charQuote, start);
    if (pos != -1) {
      result += str.substring(start, pos) + F("&quot;");
      start = pos + 1;
    } else {
      result += str.substring(start);
      break;
    }
  }

  return result;
}

String ESPWebBase::tagInput(const String& type, const String& name, const String& value) {
  String result = FPSTR(inputTypeOpen);

  result += type;
  result += charQuote;
  if (name != strEmpty) {
    result += FPSTR(inputNameOpen);
    result += name;
    result += charQuote;
  }
  if (value != strEmpty) {
    result += FPSTR(inputValueOpen);
    result += ESPWebBase::escapeQuote(value);
      result += charQuote;
  }
  if (! local_WEB_access) result += FPSTR(" disabled");  // блокировать изменение, если доступ к WEB интерфейсу закрыт  
  result += charGreater;
   
  return result;
}

String ESPWebBase::tagInput(const String& type, const String& name, const String& value, const String& extra) {
  String result = FPSTR(inputTypeOpen);

  result += type;
  result += charQuote;
  if (name != strEmpty) {
    result += FPSTR(inputNameOpen);
    result += name;
    result += charQuote;
  }
  if (value != strEmpty) {
    result += FPSTR(inputValueOpen);
    result += ESPWebBase::escapeQuote(value);
    result += charQuote;
  }
  result += charSpace;
  result += extra;
  if (! local_WEB_access) result += FPSTR(" disabled");  // блокировать изменение, если доступ к WEB интерфейсу закрыт  
  result += charGreater;

  return result;
}

String ESPWebBase::_CreateFile(uint8_t command_type) { 
  String _resp = "";                            // Переменная для хранения результата
  String file_nume="";
  if (command_type == 1) file_nume=F("/PhoneBook.txt"); // создать файл с текстом номеров
  else if (command_type == 2) file_nume=F("/Nomera2000.txt"); // создать текстовый из массива бинарных номеров  
  else if (command_type == 3) file_nume=F("/PhoneBook.bin"); // создать бинарный файл номеров
  // if (command_type == 6) { // reset SIM800
  //   uint32_t now = 0;

  // #ifdef ESP8266 
  //   now = sntp_get_current_timestamp();
  // #else
  //   now = time(NULL)+_ntpTimeZone*60*60; // Added
  //  #endif  
  //     logDateTime(now);
  //    _log->println(F(" reset SIM800"));
  //  }

    if (command_type == 1 || command_type == 2 || command_type == 3 ){
             if (SPIFFS.exists(file_nume)) {
                  SPIFFS.remove(file_nume);
                  _log->println(F("File deliting"));  //Serial.println(F("File deliting"));
             }
             else _log->println(F("File not found")); //Serial.println(F("File not found"));
    }

//*********************************************
                   // упаковка символов цифр в элемент массива по 4 бита
                  // номер цифры в телефоне = pos-abs_pos_comma-2   +CPBF: 1,"069202891",129,"069202891ofGU1"
                    //phones_on_sim[phone_index_int] = int64_t(1)<<63; 
                    //Serial.print("_BV(63) - "); Serial.println(int64_t(1)<<63,BIN);                       
                    // Serial.print("Start - "); Serial.println(phones_on_sim[phone_index_int],BIN);
                    // Serial.print("pos = "); Serial.print(pos); Serial.print(" abs_pos_comma = "); Serial.println(abs_pos_comma);   
                    //***************************************************************************************    
                   // phones_on_sim[phone_index_int] |= int64_t(resiv - '0') << (60-(pos-abs_pos_comma-2)*4);  
                    //***************************************************************************************                         
                  //  Serial.print("Char - "); Serial.print(resiv); Serial.print(" Int - "); Serial.print(resiv - '0', DEC); 
                  //  Serial.print(" Bin - "); Serial.println(resiv - '0', BIN);   
                  //  Serial.println(phones_on_sim[phone_index_int], BIN);    

  // }
  return _resp;                                 // ... возвращаем результат. Пусто, если проблема
}

bool ESPWebBase::writeTXTstring(const String& file_num_string, uint8_t command_type) {
  bool _resp=false;
  String file_nume; 
   if (command_type == 1) file_nume=F("/PhoneBook.txt"); // создать файл с текстом номеров
  else if (command_type == 2) file_nume=F("/Nomera2000.txt"); // создать текстовый из массива бинарных номеров
  File PhoneFile =  SPIFFS.open(file_nume, FILE_APPEND);
    if (PhoneFile) {
      PhoneFile.println(file_num_string);
       PhoneFile.close(); _resp=true;
      //  _log->print(F("append row - "));       
      //  _log->print(file_num_string);
      }  
  return  _resp;
}

// прочитать текстовый файл c 2000 номерами и заполить номерами массив и создать новый BIN файл
void ESPWebBase::readTXTCSVfile() {
  bool isnum=true; // признак, что весь текст из цифр
  int16_t ind =0; //текущий индекс считанного номера
  String buffer;
  String file_nume=F("/Nomera2000.txt");
   File PhoneFile =  SPIFFS.open(file_nume, FILE_READ);
 if (!PhoneFile) { 
   _log->println(F("- failed to open CSV file")); 
      #ifndef NOSERIAL 
       Serial.println("- failed to open CSV file");
      #endif
   return;
  }
  while (PhoneFile.available()) {  //Читаем содержимое файла
    buffer = PhoneFile.readStringUntil('\n');//Считываем с карты весь дотекст в строку до символа окончания.
    if (buffer.length() == DIGIT_IN_PHONENAMBER+1){
    //char charBuf[buffer.length()+1];
    //buffer.toCharArray(charBuf, buffer.length());  
     isnum=true; // признак, что весь текст из цифр
     for (int v=0; v < DIGIT_IN_PHONENAMBER; ++v) {
       if (buffer[v]<48 || buffer[v]>57) // если число ничего не делать
           {isnum=false; 
             #ifndef NOSERIAL 
                Serial.print ("NOT INT value = "); Serial.println(buffer[v]);
             #endif
           break;} // если не числовой символ выставить флаг и выйти из цикла
     }
     if (isnum){
         phones_on_sim[ind]=0;
       for (int8_t g=0; g < buffer.length()-1; ++g)
         phones_on_sim[ind] |= uint64_t(buffer[g] - '0') << (60-g*4); 

      //  #ifndef NOSERIAL 
      //   Serial.print(String(ind)); Serial.print(" - "); Serial.println(buffer); // для отладки отправляем по UART все что прочитали с карты.
      //  #endif  
       ++ind;            
     } 
    }
  } 
  alloc_num[2] = ind;          
}

void ESPWebBase::readBINfile() {
  String file_nume=F("/PhoneBook.bin");
  File PhoneFile =  SPIFFS.open(file_nume, FILE_READ);
  uint16_t curr_num=0; // номер текщего считываемого телефонного номера  
  if (!PhoneFile) { 
   _log->println(F("- failed to open BIN file")); 
    //Serial.println("- failed to open BIN file");
   return;
  }
  else{
    uint8_t dig = 0; //Номер текущего считанного байта из файла (всего для одного номера надо считать 8 байт)
    byte byte_read =0; // текущий считываемый байт из файла (всего для одного номера надо считать 8 байт)
    while(PhoneFile.available()){
        byte_read = PhoneFile.read();
        phones_on_sim[curr_num] |= uint64_t(byte_read) << (56 - dig*8); 
        if (dig < 7) ++dig;
        else { ++curr_num; dig=0; }
      } 
    }
      PhoneFile.close();
      alloc_num[2] = curr_num;      
      #ifndef NOSERIAL 
        Serial.print("Total BIN_numer - "); 
        Serial.println(alloc_num[2]);
      #endif
 }

void ESPWebBase::readTXTfile() {
  String file_nume=F("/PhoneBookNew.txt");
  String phone_index;
  String phone_num;
  String phone_comment;
  uint8_t string_position=0; //позиция в строке с 0 и т.тд.
  int v=0; // счетчик номеров - не больше возможно допустимого для сим карты значение из alloc_num[1]
  File PhoneFile =  SPIFFS.open(file_nume, FILE_READ);
  if (!PhoneFile) { 
   _log->println(F("- failed to open TXT file")); 
    //Serial.println("- failed to open TXT file");
   return;
  }
  else{
     String buffer;
     while (PhoneFile.available()) //Читаем содержимое файла
    { if (v<alloc_num[1]){
        buffer = PhoneFile.readStringUntil('\n'); //Считываем весь дотекст в строку до символа окончания.
          //Разделяем строку на значения с указанием разделителя
    char charBuf[buffer.length()+1];
    buffer.toCharArray(charBuf, buffer.length());

    //Serial.println(buffer);

    char * pch;
    pch = strtok (charBuf,";");
    //String temp_resp;  
    string_position=0; //позиция в строке с 0 и т.тд.
      while (pch != NULL)
      {
        //Serial.println(pch);
        switch (string_position){
         case 0:
           phone_index=pch;          
           break;
         case 1:
           phone_num=pch;
           break;         
         case 2:
           phone_comment=pch;
           //phone_comment=phone_comment.substring(9);
           break;
     
      } 
        string_position++; //позиция в строке с 0 и т.тд.
        pch = strtok (NULL, ";");
        if (pch == NULL) {
           for (uint8_t g=0; g<DIGIT_IN_PHONENAMBER; ++g)
             PhoneOnSIM[v][g]=phone_num[g]; 
           for (uint8_t g=0; g<15; ++g)    //textnumbercomment.length()        
            { if (phone_comment.length() < g)
                {CommentOnSIM[v][g] = NULL; 
                break;
                }
              else                
               if (g == 15-1) CommentOnSIM[v][g] = NULL;
               else CommentOnSIM[v][g] = phone_comment[g];                   
            }  
             //temp_resp = FPSTR("AT+CPBW=");
             if (phone_index.toInt()>0)
              {
               indexOnSim[v]=phone_index.toInt();
              // temp_resp += String(indexOnSim[v]); 
              }
             else indexOnSim[v]=0; 
              
          //    temp_resp += ",\""; 
          //    for (uint8_t g=0; g<DIGIT_IN_PHONENAMBER; ++g) temp_resp += PhoneOnSIM[v][g]; 
          //    temp_resp += "\",129,\""; 
          //    temp_resp += String(CommentOnSIM[v]);
          //    temp_resp +="\"";    
          //  #ifndef NOSERIAL            
          //   Serial.println(temp_resp);
          // #endif
        }
       }   
       ++v; // увеличить счетчик номеров
      }
      else break; // если число строк в файле больше возможного количества номеров на СИМ - выйти из цикла
     }

      PhoneFile.close();
     }
}

// процедура сохранения нового BIN файла с номерами телефонов в SPIFFS
bool ESPWebBase::saveFile(const String& Fname){
  bool _ret = false;
  byte buf[8]; 
  int total_save_num = 0; // количество сохраненных номеров в файл 
//содать файл для сохранения
  File PhoneFile =  SPIFFS.open(Fname, FILE_APPEND); 
  if (PhoneFile){
     // write_bayt = PhoneFile.write(buf, 8);
     for (int16_t l=0; l < total_bin_num; ++l)
      { if  (phones_on_sim[l] !=0 ) { // Если номер не равен нулю - записать в файл
        // Разделить 64 битное число на 8 отбельных байт для последовательной записи в файл
        for (int8_t j=0; j<8; ++j) {
          buf[j] = phones_on_sim[l] >> (56 - (8*j)) ;
          //Serial.print("bf["); Serial.print(j); Serial.print("] = "); Serial.println(buf[j], BIN);
          }  
         PhoneFile.write(buf, 8); //последовательно записать байты в файл
          ++total_save_num;
        }
      } 
      //Serial.print("write_bayt = "); Serial.println(write_bayt);
    PhoneFile.close(); 
    alloc_num[2] = total_save_num; //сохранить количество номеров в массиве
    _ret = true;
   }  
   //else  Serial.println("- failed to open file for appending"); 

  return _ret;
}

const char ESPWebBase::_signEEPROM[4] PROGMEM = { '#', 'E', 'S', 'P' };
