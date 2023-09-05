#include <pgmspace.h>
#include "ESPWebMQTT.h"

/*
 * ESPWebMQTTBase class implementation
 */

ESPWebMQTTBase::ESPWebMQTTBase() : ESPWebBase() {
  _espClient = new WiFiClient();
  pubSubClient = new PubSubClient(*_espClient);
}

void ESPWebMQTTBase::setupExtra() {
  if (_mqttServer != strEmpty) {
    pubSubClient->setServer(_mqttServer.c_str(), _mqttPort);
    pubSubClient->setCallback(std::bind(&ESPWebMQTTBase::mqttCallback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
  }
}

void ESPWebMQTTBase::loopExtra() {
  if (_mqttServer != strEmpty) {
    if (! pubSubClient->connected())
      mqttReconnect();
    if (pubSubClient->connected())
      pubSubClient->loop(); // НЕ Изменено
  }
}

uint16_t ESPWebMQTTBase::readConfig() {
  uint16_t offset = ESPWebBase::readConfig();

  if (offset) {
    uint16_t start = offset;

    offset = readEEPROMString(offset, _mqttServer, maxStringLen);
    getEEPROM(offset, _mqttPort);
    offset += sizeof(_mqttPort);
    offset = readEEPROMString(offset, _mqttUser, maxStringLen);
    offset = readEEPROMString(offset, _mqttPassword, maxStringLen);
    offset = readEEPROMString(offset, _mqttClient, maxStringLen);
    uint8_t crc = crc8EEPROM(start, offset);
    if (readEEPROM(offset++) != crc) {
      _log->println(F("CRC mismatch! Use default MQTT parameters."));
      defaultConfig(1);
    }
  }

  return offset;
}

uint16_t ESPWebMQTTBase::writeConfig(bool commit) {
  uint16_t offset = ESPWebBase::writeConfig(false);
  uint16_t start = offset;

  offset = writeEEPROMString(offset, _mqttServer, maxStringLen);
  putEEPROM(offset, _mqttPort);
  offset += sizeof(_mqttPort);
  offset = writeEEPROMString(offset, _mqttUser, maxStringLen);
  offset = writeEEPROMString(offset, _mqttPassword, maxStringLen);
  offset = writeEEPROMString(offset, _mqttClient, maxStringLen);
  uint8_t crc = crc8EEPROM(start, offset);
  writeEEPROM(offset++, crc);
  if (commit)
    commitConfig();

  return offset;
}

void ESPWebMQTTBase::defaultConfig(uint8_t level) {
  if (level < 1)
    ESPWebBase::defaultConfig(level);

  if (level < 2) {
    _mqttServer = String();
    _mqttPort = defMQTTPort;
    _mqttUser = String();
    _mqttPassword = String();
    _mqttClient = FPSTR(defMQTTClient);
    _mqttClient += getBoardId();
  }
}

bool ESPWebMQTTBase::setConfigParam(const String& name, const String& value) {
  if (! ESPWebBase::setConfigParam(name, value)) {
    if (name.equals(FPSTR(paramMQTTServer)))
      _mqttServer = value;
    else if (name.equals(FPSTR(paramMQTTPort)))
      _mqttPort = value.toInt();
    else if (name.equals(FPSTR(paramMQTTUser)))
      _mqttUser = value;
    else if (name.equals(FPSTR(paramMQTTPassword)))
      _mqttPassword = value;
    else if (name.equals(FPSTR(paramMQTTClient)))
      _mqttClient = value;
    else
      return false;
  }

  return true;
}

void ESPWebMQTTBase::setupHttpServer() {
  ESPWebBase::setupHttpServer();
  httpServer->on(String(FPSTR(pathMQTT)).c_str(), std::bind(&ESPWebMQTTBase::handleMQTTConfig, this));
}

void ESPWebMQTTBase::handleRootPage() {
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
  script += F(";\n\
}\n\
}\n\
request.send(null);\n\
}\n\
setInterval(refreshData, 1000);\n");

  String page = ESPWebBase::webPageStart(F("ESP8266"));
  page += ESPWebBase::webPageScript(script);
  page += ESPWebBase::webPageBody();
  page += F("<h3>ESP8266</h3>\n\
<p>\n\
MQTT broker: <span id=\"");
  page += FPSTR(jsonMQTTConnected);
  page += F("\">?</span><br/>\n\
Heap free size: <span id=\"");
  page += FPSTR(jsonFreeHeap);
  page += F("\">0</span> bytes<br/>\n\
Uptime: <span id=\"");
  page += FPSTR(jsonUptime);
  page += F("\">0</span> seconds</p>\n\
<p>\n");
  page += navigator();
  page += ESPWebBase::webPageEnd();

  httpServer->send(200, FPSTR(textHtml), page);
}

void ESPWebMQTTBase::handleMQTTConfig() {
  String page = ESPWebBase::webPageStart(F("MQTT Setup"));
  page += ESPWebBase::webPageBody();
  page += F("<form name=\"mqtt\" method=\"GET\" action=\"");
  page += FPSTR(pathStore);
  page += F("\">\n\
<h3>MQTT Setup</h3>\n\
<label>Server:</label><br/>\n");
  page += ESPWebBase::tagInput(FPSTR(typeText), FPSTR(paramMQTTServer), _mqttServer, String(F("maxlength=")) + String(maxStringLen));
  page += F("\n(leave blank to ignore MQTT)<br/>\n\
<label>Port:</label><br/>\n");
  page += ESPWebBase::tagInput(FPSTR(typeText), FPSTR(paramMQTTPort), String(_mqttPort), F("maxlength=5"));
  page += F("<br/>\n\
<label>User:</label><br/>\n");
  page += ESPWebBase::tagInput(FPSTR(typeText), FPSTR(paramMQTTUser), _mqttUser, String(F("maxlength=")) + String(maxStringLen));
  page += F("\n(leave blank if authorization is not required)<br/>\n\
<label>Password:</label><br/>\n");
  page += ESPWebBase::tagInput(FPSTR(typePassword), FPSTR(paramMQTTPassword), _mqttPassword, String(F("maxlength=")) + String(maxStringLen));
  page += F("<br/>\n\
<label>Client:</label><br/>\n");
  page += ESPWebBase::tagInput(FPSTR(typeText), FPSTR(paramMQTTClient), _mqttClient, String(F("maxlength=")) + String(maxStringLen));
  page += F("\n\
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

String ESPWebMQTTBase::jsonData() {
  String result = ESPWebBase::jsonData();
  result += F(",\"");
  result += FPSTR(jsonMQTTConnected);
  result += F("\":");
  if (pubSubClient->connected())
    result += FPSTR(bools[1]);
  else
    result += FPSTR(bools[0]);

  return result;
}

String ESPWebMQTTBase::btnMQTTConfig() {
  String result = ESPWebBase::tagInput(FPSTR(typeButton), strEmpty, F("MQTT Setup"), String(F("onclick=\"location.href='")) + String(FPSTR(pathMQTT)) + String(F("'\"")));
  result += charLF;

  return result;
}

String ESPWebMQTTBase::navigator() {
  String result = btnWiFiConfig();
  result += btnTimeConfig();
  result += btnMQTTConfig();
  result += btnLog();
  result += btnReboot();

  return result;
}

bool ESPWebMQTTBase::mqttReconnect() {
  const uint32_t timeout = 30000;
  static uint32_t nextTime;
  bool result = false;

  if ((int32_t)(millis() - nextTime) >= 0) {
    _log->print(F("Attempting MQTT connection..."));
    waitingMQTT();
    // **** добавлено для LWT  
     String topic;
       if (_mqttClient != strEmpty) {
          topic += charSlash;
          topic += _mqttClient;
      }   
          topic += mqttDeviceStatusTopic;     
//***********              
    if (_mqttUser != strEmpty){
     // result = pubSubClient->connect(_mqttClient.c_str(), _mqttUser.c_str(), _mqttPassword.c_str()); // было в оргинале без LWT
      result = pubSubClient->connect(_mqttClient.c_str(), _mqttUser.c_str(), _mqttPassword.c_str(),  
                                topic.c_str(), mqttDeviceStatusQos, mqttDeviceStatusRetained, mqttDeviceStatusOff); // c LWT      
    }
    else{
     // result = pubSubClient->connect(_mqttClient.c_str());// было в оргинале без LWT
      result = pubSubClient->connect(_mqttClient.c_str(), NULL, NULL,  
                                topic.c_str(), mqttDeviceStatusQos, mqttDeviceStatusRetained, mqttDeviceStatusOff); // c LWT                                     
  }
    waitedMQTT();
    if (result) {
      _log->println(F(" connected"));
      mqttResubscribe();      
     // **** добавлено для LWT     
     // vTaskDelay(1200);
      mqttPublish(topic, mqttDeviceStatusOn);  // добавлено для LWT 
      String IP_Topic = charSlash + _mqttClient + "/LocalIP";
      String String_IP = IPAddress2String(WiFi.localIP());
      mqttPublish(IP_Topic, String_IP);  // добавлено для отображения на MQTT локально IP адреса       
     // ******
    } else {
      _log->print(F(" failed, rc="));
      _log->println(pubSubClient->state());
    }
    nextTime = millis() + timeout;
  }

  return result;
}

char *ESPWebMQTTBase::IPAddress2String(IPAddress ip) {
      static char str_IP[16];
      char * last = str_IP;
      for (int8_t i = 0; i < 4; i++) {
          itoa(ip[i], last, 10);
          last = last + strlen(last);
          if (i == 3) *last = '\0'; else *last++ = '.';
      }
      return str_IP;
  }  

void ESPWebMQTTBase::mqttCallback(char* topic, byte* payload, unsigned int length) {
  _log->print(F("MQTT message arrived ["));
  _log->print(topic);
  _log->print(F("] "));
  for (int i = 0; i < length; ++i) {
    _log->print((char)payload[i]);
  }
  _log->println();
}

void ESPWebMQTTBase::mqttResubscribe() {
  String topic;

  if (_mqttClient != strEmpty) {
    topic += charSlash;
    topic += _mqttClient;
    topic += F("/#");
    mqttSubscribe(topic);
  }
}

bool ESPWebMQTTBase::mqttSubscribe(const String& topic) {
  _log->print(F("MQTT subscribe to topic \""));
  _log->print(topic);
  _log->println('\"');

  return pubSubClient->subscribe(topic.c_str());
}

bool ESPWebMQTTBase::mqttPublish(const String& topic, const String& value) {
  _log->print(F("MQTT publish topic \""));
  _log->print(topic);
  _log->print(F("\" with value \""));
  _log->print(value);
  _log->println('\"');

  return pubSubClient->publish(topic.c_str(), value.c_str(), true);
}

void ESPWebMQTTBase::waitingMQTT() {
#ifndef NOBLED
  digitalWrite(LED_BUILTIN, LOW);
#endif
}

void ESPWebMQTTBase::waitedMQTT() {
#ifndef NOBLED
  digitalWrite(LED_BUILTIN, HIGH);
#endif
}
