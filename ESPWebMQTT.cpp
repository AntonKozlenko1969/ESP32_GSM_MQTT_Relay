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
   queue_comand = xQueueCreate(max_queue, sizeof(mod_com)); // очередь передачи команд в модуль SIM800 размер - int8_t [max_queue]
}

void ESPWebMQTTBase::loopExtra() {
  if (_mqttServer != strEmpty && WiFi.getMode() == WIFI_STA) {
    if (! pubSubClient->connected())
      mqttReconnect();
    if (pubSubClient->connected())
      pubSubClient->loop(); // НЕ Изменено
  }
  if (_gsmMode && _mqttServer != strEmpty && WiFi.getMode() != WIFI_STA && modemOK && !IsOpros && !SIM_fatal_error) {
      GPRS_MQTT_Reconnect();
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

   #ifndef NOSERIAL 
    Serial.print("MQTT server: ");  Serial.print(_mqttServer); 
    Serial.print(" port: "); Serial.print(_mqttPort);             
    Serial.print(" user: "); Serial.print(_mqttUser);    
    Serial.print(" pass: "); Serial.print(_mqttPassword);  
    Serial.print(" client: "); Serial.println(_mqttClient);              
   #endif   

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
  page += ESPWebBase::tagInput(FPSTR(typeText), FPSTR(paramMQTTServer), _mqttServer, String(F("maxlength=")) + String(maxStringLen) + String(F(" size=")) + String(maxStringLen));
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
  if (pubSubClient->connected() || MQTT_connect) //** add 10/11/2023
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
          //topic += charSlash;// 21/11/2023
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
      String IP_Topic; 
      if (_mqttClient != strEmpty) {
         //IP_Topic +=charSlash; // 21/11/2023
         IP_Topic +=_mqttClient;   
        }
       IP_Topic +=FPSTR(mqttDeviceIPTopic);  
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
    //topic += charSlash; // 21/11/2023
    topic += _mqttClient;
    topic += F("/#");
    mqttSubscribe(topic);
  }
}

bool ESPWebMQTTBase::mqttSubscribe(const String& topic) {
  _log->print(F("MQTT subscribe to topic \""));
  _log->print(topic);
  _log->println('\"');
  if (pubSubClient->connected())
    return pubSubClient->subscribe(topic.c_str());
  if (MQTT_connect) {
     GPRS_MQTT_sub(topic); 
    return true;
  } 
}

bool ESPWebMQTTBase::mqttPublish(const String& topic, const String& value) {
  _log->print(F("MQTT publish topic \""));
  _log->print(topic);
  _log->print(F("\" with value \""));
  _log->print(value);
  _log->println('\"');
  if (pubSubClient->connected())
    return pubSubClient->publish(topic.c_str(), value.c_str(), mqttDeviceStatusRetained);
  if (_gsmMode && MQTT_connect) {
     GPRS_MQTT_pub(topic, value); 
    return true;
  }
}

void ESPWebMQTTBase::waitingMQTT() {
#ifndef NOBLED
  digitalWrite(LED_BUILTIN, LOW);
#endif
}

// добавление команды и текста команды в очередь
void ESPWebMQTTBase::add_in_queue_comand(int _inncomand, const char* _inn_text_comand, int _com_flag){
   mod_com modem_comand;

   modem_comand.com = _inncomand;
   modem_comand.com_flag = _com_flag;
   //_inn_text_comand.toCharArray(modem_comand.text_com, _inn_text_comand.length());
   for (int v=0; v<max_text_com; ++v) {
     modem_comand.text_com[v] = _inn_text_comand[v];
     if (_inncomand !=8) {if (_inn_text_comand[v] == NULL) break;}
   }
   bool add_in_queue; // признак добавления команды в очередь
  if (_inncomand ==30 && (String(modem_comand.text_com) == "+CIPCLOSE"))
    add_in_queue = xQueueSendToFront(queue_comand, &modem_comand, 0);  
  else
    add_in_queue = xQueueSend(queue_comand, &modem_comand, 0);

   #ifndef NOSERIAL   
    if (add_in_queue == pdTRUE) {
        Serial.print("Add in QUEUE comand - "); Serial.print(_inncomand);
        Serial.print(" text : "); Serial.println(_inn_text_comand);
     }
    else Serial.println("QUEUE is FULL");  
   #endif   
}

void ESPWebMQTTBase::waitedMQTT() {
#ifndef NOBLED
  digitalWrite(LED_BUILTIN, HIGH);
#endif
}

void ESPWebMQTTBase::GPRS_MQTT_Reconnect(){
  static uint32_t timeout; //  = 30000;
  static uint32_t lastTime;
  static bool resub; // признак, что переподключение и переподпика прошла успешно
  static uint8_t connect_attempt; // количество попыток подключения к MQTT серверу
  // шаг в процессе подключения, для исключения передачи в очередь одинаковых команд. 
  // С момента ее первой подачи, до момента ее передачи в модем и исполнения.
  static uint8_t reconnect_step; 
  
   if ((millis() - lastTime) >= timeout) {
   lastTime = millis();         
      //   #ifndef NOSERIAL  
      //    Serial.print("reconnect_step = ");  
      //    Serial.print(reconnect_step);
      //    Serial.print(" connect_attempt = ");  
      //    Serial.print(connect_attempt);  
      //    Serial.print(" resub = "); Serial.print(resub ? F(" TRUE ") : F(" FALSE "));              
      //    Serial.print(" GPRS_ready = "); Serial.print(GPRS_ready ? F(" TRUE ") : F(" FALSE "));         
      //    Serial.print(" TCP_ready = "); Serial.print(TCP_ready ? F(" TRUE ") : F(" FALSE "));
      //    Serial.print(" MQTT_connect = "); Serial.println(MQTT_connect ? F(" TRUE ") : F(" FALSE "));          
      //  #endif 

   if( !GPRS_ready && reconnect_step == 0) { // признак подключения GPRS
       add_in_queue_comand(7,"", 0); //включить режим GPRS 
      reconnect_step = 1; timeout = 5000; resub=false; return;  // Не подавать следующую команду пока не подключимся
      }
   if (!GPRS_ready && reconnect_step > 0) {++connect_attempt; resub=false;}  
   if (GPRS_ready && reconnect_step == 0) {++reconnect_step;  resub=false;} 
   if(!TCP_ready && GPRS_ready && reconnect_step == 1) {//признак подключения к MQTT серверу
         GPRS_MQTT_connect (); reconnect_step = 2; timeout = 800; ++connect_attempt; resub=false; return; // Не подавать следующую команду пока не подключимся
      }
   if (reconnect_step > 1) {
      if (MQTT_connect) {
        connect_attempt=0;
        if (!resub) {
           String topic ;
           //topic += charSlash;// 21/11/2023          
           topic = _mqttClient;   
           topic += mqttDeviceStatusTopic;    
           mqttPublish(topic, mqttDeviceStatusOn);   
           topic =_mqttClient; 
           topic += FPSTR(mqttDeviceIPTopic);  
           String String_IP;
               String_IP = F("GPRS");//IPAddress2String(WiFi.localIP());
           mqttPublish(topic, String_IP);  // добавлено для отображения на MQTT локально IP адреса            
           mqttResubscribe(); 
           resub=true;
          }
         GPRS_MQTT_ping(); //только поддержать соединение          
         reconnect_step = 7; timeout = 52000;
       }
      else { ++reconnect_step; }  
     }

    // если подключились к MQTT серверу, но сервер скинул подключение (не верный пользователь или пароль)
    if (reconnect_step > 9) {reconnect_step=0; timeout = 30000; resub=false;}//создать условие для нового прохода подключений через 20 * timeout
    // если пытаемся подключиться, но сервер вообще не отвечает (сервер не доступен, не верный адрес, URL)
    if (connect_attempt > 6) timeout = 3*60*1000; // пробовать через 3 минуты
    if (connect_attempt > 15) timeout = 7*60*1000; // пробовать через 7 минут
    if (connect_attempt == 20) {timeout = 30*1000; reconnect_step=0; connect_attempt=0; resub=false;} // начать попытки заново
    if (!modemOK) {reconnect_step=0; timeout = 30000; resub=false;}//создать условие для нового прохода подключений
    
  }
}

void ESPWebMQTTBase::GPRS_MQTT_connect (){
  char _inn_comm[max_text_com];
  int _curr_poz = 2; // текущая позиция в массиве
  uint16_t rest_length =0; // общее количество байт в пакете (крме первых двух)
  String topic ;
         //topic += charSlash;
         topic += _mqttClient;   
         topic += mqttDeviceStatusTopic;         

    //  #ifndef NOSERIAL  
    //     Serial.print("connect topic / mess ");     
    //     Serial.print(topic); 
    //     Serial.print(" / ");         
    //     Serial.println(String(mqttDeviceStatusOff));         
    //   #endif 

  _inn_comm[0] = 0x10; //#0 идентификатор пакета на соединение
  // оставшееся количество байт без логина и пароля пользователя
  //  пример: _inn_comm[1] = strlen(MQTT_type)+_mqttClient.length()+_mqttUser.length()+_mqttPassword.length()+topic.length()+strlen(mqttDeviceStatusOff)+16;   
  rest_length = strlen(MQTT_type)+_mqttClient.length()+topic.length()+strlen(mqttDeviceStatusOff)+12; 
  _inn_comm[_curr_poz] =0x00; ++_curr_poz; //#2 старший байт длины названия протокола
  _inn_comm[_curr_poz] =strlen(MQTT_type); ++_curr_poz; //#3 младший байт длины названия протокола
  for (int v=0;v<strlen(MQTT_type);++v) {_inn_comm[_curr_poz] = MQTT_type[v]; ++_curr_poz;} //байты типа протокола  
  _inn_comm[_curr_poz] =0x04; ++_curr_poz; // Protocol Level byte 00000100
  //Connect Flag bits: 7-User Name Flag, 6-Password Flag, 5-Will Retain, 4-Will QoS, 3-Will QoS, 2-Will Flag, 1-Clean Session, 0-Reserved
  if (_mqttClient == strEmpty) {
     _inn_comm[_curr_poz] =0x2E; ++_curr_poz;  //Connect Flag bits без логина и пароля 0x2E 00101110, 0x2C 00101100
  }   
  else {
     _inn_comm[_curr_poz] =0xEE; ++_curr_poz;  //Connect Flag bits с логином и паролем 0xEE 11101110, 0xEC 11101100
     rest_length += _mqttUser.length()+_mqttPassword.length()+4;
  }
  _inn_comm[1] = rest_length;  // оставшееся количество байт без логина и пароля пользователя
  _inn_comm[_curr_poz] =0x00; ++_curr_poz; _inn_comm[_curr_poz] =0x3C; ++_curr_poz; // время жизни сессии (2 байта) 0x23-35sec, 0x28-40sec, 0x3C-60sec
  _inn_comm[_curr_poz] =0x00; ++_curr_poz; _inn_comm[_curr_poz] =_mqttClient.length(); ++_curr_poz; // длина идентификатора (2 байта)
  for (int v=0;v<_mqttClient.length();++v) {_inn_comm[_curr_poz] = _mqttClient[v]; ++_curr_poz;}  // MQTT  идентификатор устройства
  _inn_comm[_curr_poz] =0x00; ++_curr_poz; _inn_comm[_curr_poz]=topic.length(); ++_curr_poz;  // длина LWT топика (2 байта) 
  for (int v=0;v<topic.length();++v) {_inn_comm[_curr_poz] = topic[v]; ++_curr_poz;}  // LWT топик    
  _inn_comm[_curr_poz] =0x00; ++_curr_poz; _inn_comm[_curr_poz]=strlen(mqttDeviceStatusOff); ++_curr_poz; // длина LWT сообщения (2 байта) 
  for (int v=0;v<strlen(mqttDeviceStatusOff);++v) {_inn_comm[_curr_poz] = mqttDeviceStatusOff[v]; ++_curr_poz;}   // LWT сообщение  

  if (_mqttClient != strEmpty) {
      _inn_comm[_curr_poz] =0x00; ++_curr_poz; _inn_comm[_curr_poz]=_mqttUser.length(); ++_curr_poz;// длина MQTT логина (2 байта) 
     for (int v=0;v<_mqttUser.length();++v) {_inn_comm[_curr_poz] = _mqttUser[v]; ++_curr_poz;} // MQTT логин
     _inn_comm[_curr_poz] =0x00; ++_curr_poz; _inn_comm[_curr_poz]=_mqttPassword.length(); ++_curr_poz;// длина MQTT пароля (2 байта) 
     for (int v=0;v<_mqttPassword.length();++v) {_inn_comm[_curr_poz] = _mqttPassword[v]; ++_curr_poz;} // MQTT пароль
  }
  add_in_queue_comand(30, String(F("+GSMBUSY=1")).c_str(), 0);
  add_in_queue_comand(8, _inn_comm, 8);
  add_in_queue_comand(30, String(F("+GSMBUSY=0")).c_str(), 0);  
}

 void ESPWebMQTTBase::GPRS_MQTT_pub (const String& _topic, const String& _messege) {          // пакет на публикацию
  char _inn_comm[max_text_com];
  int _curr_poz = 4; // текущая позиция в массиве

    //  #ifndef NOSERIAL  
    //     Serial.print("pub topic / mess ");     
    //     Serial.print(_topic); 
    //     Serial.print(" / ");         
    //     Serial.println(_messege);         
    //   #endif 

    _inn_comm[0]=0x33; // было 0x30 без retain Qos0, 0x31 с retain Qos0, 0x33 Qos1 (не работает??)
    _inn_comm[1]=_topic.length()+_messege.length()+2+2; //отсавшаяся длина пакета
    _inn_comm[2]=0x00; _inn_comm[3]=_topic.length();
    for (int8_t v=0; v<_topic.length();++v) {_inn_comm[_curr_poz]=_topic[v]; ++_curr_poz;}// топик
    _inn_comm[_curr_poz]=0x00; ++_curr_poz; _inn_comm[_curr_poz]=0x10; ++_curr_poz; // идентификатор отправленного пакета для подтвержения публикации
    for (int8_t v=0; v<_messege.length();++v) {_inn_comm[_curr_poz]=_messege[v]; ++_curr_poz;}   // сообщение  

  add_in_queue_comand(30, String(F("+GSMBUSY=1")).c_str(), 0);
  add_in_queue_comand(8, _inn_comm, 8);
  add_in_queue_comand(30, String(F("+GSMBUSY=0")).c_str(), 0);  
  }                                                 

void ESPWebMQTTBase::GPRS_MQTT_ping () {                                // пакет пинга MQTT сервера для поддержания соединения
  char _inn_comm[max_text_com];
  _inn_comm[0]=0xC0; _inn_comm[1]=0x00;

  add_in_queue_comand(30, String(F("+GSMBUSY=1")).c_str(), 0);
  add_in_queue_comand(8, _inn_comm, 8);
  add_in_queue_comand(30, String(F("+GSMBUSY=0")).c_str(), 0);   
}

 void ESPWebMQTTBase::GPRS_MQTT_sub (const String& _topic) {                                       // пакет подписки на топик
  char _inn_comm[max_text_com];
  int _curr_poz = 6; // текущая позиция в массиве
      //  #ifndef NOSERIAL  
      //   Serial.print("sub topic ");     
      //   Serial.println(_topic); 
      // #endif    

  _inn_comm[0]=0x82; 
  _inn_comm[1]=_topic.length()+5;   // сумма пакета 
  _inn_comm[2]=0x00; _inn_comm[3]=0x01; _inn_comm[4]=0x00;
  _inn_comm[5]=_topic.length();  // топик
    for (int8_t v=0; v<_topic.length();++v) {_inn_comm[_curr_poz]=_topic[v]; ++_curr_poz;}  
  _inn_comm[_curr_poz]=0x00;   

  add_in_queue_comand(30, String(F("+GSMBUSY=1")).c_str(), 0);
  add_in_queue_comand(8, _inn_comm, 8);
  add_in_queue_comand(30, String(F("+GSMBUSY=0")).c_str(), 0);  
   }     
