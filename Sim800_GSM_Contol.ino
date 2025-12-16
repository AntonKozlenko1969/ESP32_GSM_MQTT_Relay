//#include <Arduino.h>
#define MODEM_RST             5
#define MODEM_PWRKEY          4
#define MODEM_POWER_ON       23
#define MODEM_TX             27
#define MODEM_RX             26

// Initialize the indicator as an output
#define STATUS_LED_GPIO         19
// #define LED_ON               HIGH
// #define LED_OFF              LOW

// #define DIGIT_IN_PHONENAMBER 9

const char str_relay[] PROGMEM = "Relay #";
const char str_ON[] PROGMEM = " is ON";
const char str_OFF[] PROGMEM = " is OFF";

const int number_comant_type PROGMEM = 23; // общее количество возможных СМС команд
// Текстовое значение СМС команды
const char* const comand_nume[number_comant_type] PROGMEM ={
                            "Add",  //Добавить новый номер на СИМ карту или в бинарный массив если на сим уже нет места.
                            "Del", // удалить один номер с СИМ карты
                            "Rds", // прочитать список номеров с сим карты и создать файл
                            "Bin", // создать бинарный файл из BIN64 массива
                            "Rtb", //Read text to bin прочитать номера из файла Nomera2000.txt и заполнить BIN64 массив
                            "Dan", //Delete All Numbers удалить все номера из СИМ карты
                            "Rms", //Передать СМС со списком мастер номеров
                            "Wms", //Добавить номер в список мастер номеров
                            "Dms", //Удалить номер из списка мастер номеров
                            "Res", //Restore сохранить все номера из файла PhoneBookNew.txt в СИМ карту
                            "R11", "R21", "R31", "R41", "R51","Ra1", // включить одно реле или все реле сразу - RA1
                            "R10", "R20", "R30", "R40", "R50","Ra0",  // выключить одно реле или все реле сразу - RA0
                            "Cnf" // Создать с заменой новый файл Nomera2000.txt из имеющегося в памяти массива 2000 номеров
                            };
// Признак поведения для каждой команды
   // - первый элемент 1 - есть прикрепленный к команде номер : 0 - нет номера в команде (не проверять)
   // - второй элемент 1 - ответить СМС : 0 - не отвечать
const int8_t comand_prop[number_comant_type][2] PROGMEM ={{1,1},{1,1},{0,1},{0,1},{0,1}, {0,1}, {0,1}, {1,1}, {1,1}, {0,1},
                                                          {0,0},{0,0},{0,0},{0,0},{0,0},{0,1},
                                                          {0,0},{0,0},{0,0},{0,0},{0,0},{0,1}, {0,1}}; 

unsigned long t_rst = 0; //120*1000; // отследить интервал для перезапуска модема
 QueueHandle_t queue_IN_SMS; // очередь обработки входящих СМС
bool IsRestart = false; // признак однократной отправки Restart в модем
bool PIN_ready = false;
bool CALL_ready = false;
bool GET_GPRS_OK = false; // признак удачного HTTP GET запроса
bool comand_OK = false; // признак успешного выполнения текущей команды
bool one_call = false; // признак однократного снятия трубки, т.к. при повторных сигналах вызова и сбое в модеме реле может срабатывать многократно 
TaskHandle_t Task3 = NULL; // Задача для ядра 0
//Переменные для работы с SMS
char SMS_incoming_num[DIGIT_IN_PHONENAMBER+7]; // номер с которого пришло СМС - для ответной СМС
char SMS_text_num[DIGIT_IN_PHONENAMBER+1];  // номер телефона из СМС
char SMS_text_comment[5+1]; // комментарий к номеру из СМС
int num_text_comanda = -1; //номер команды из СМС в массиве команд comand_nume
int sms_answer = 0 ; // надо ли отвечать на входящую СМС
int SMS_phoneBookIndex=0; // если номер уже есть в симке - его индекс, нет - ноль
int SMS_currentIndex = 0; // текущая СМС в обработке, если ноль ничего нет в обработке
bool IsComment=false;  //признак наличия прикрепленного к номеру комментария

unsigned long t_last_command = 0;  // последняя команда от модема для отслеживания ОК
int8_t flag_modem_resp = 0; // Признак сообщения полученного от модема (если необходимо обработать следующую строку с ОК)
                            // 1 - +CMGS: попытка отправить сообщение OK или ERROR
                            // 2 - +CPBF: попытка найти одиночный номер на симке
                            // 3 - +CPBW: попытка добавить / редактировать одиночный номер на симке
                            // 4 - +CPBW: завершено одиночное удаление номера из СМС - отправить ответ
                            // 5 - +CPBF: просмотр всех номеров из СМС - создание текстового файла
                            // 6 - > - запрос на ввод текста сообщения при его отправке после команды +CMGS
                            // 8 - > - запрос на ввод данных для их отправки на MQTT сервер

void GPRS_modem_traffic( void * pvParameters ){
  int _num_index = 0; //счетчик номеров из телефонной книги при записи номеров из массива на СИМ
  bool _AT_ret =false; // возврат от сегмента sendATCommand
  String _comm = String(); //исполняемая команда без AT
  int _povtor = 0; //возможное количество повторов текущей команды
  uint8_t g = 0; // счетчик повторов отправки команды в модем
  uint8_t _interval = 5; // интервал в секундах ожидания ответа от модема
  unsigned long _timeout = 0;        // Переменная для отслеживания таймаута (10 секунд)  
  String _first_com;  // строковая переменная с командой из очереди команд
  _first_com.reserve(max_text_com+1);
//************************************************
  //const String apn = F("wap.orange.md"); // vremenno
  const char GPRScomsnt[] PROGMEM = "+SAPBR=";
// ********************************************************
uint8_t command_type =0; //тип отправленной в модем команды 
                         // 1 - считать весь список телефонов с СИМ, создать файл PhoneBook.txt с текстом номеров
                         // 2 - создать массив из бинарных значений номеров на СИМ карте
                         // 3 - создать бинарный файл номеров PhoneBook.bin
                         // 4 - скопировать с файла PhoneBookNew.txt все номера на СИМ
                         // 5 - удалить все номера из сим карты
                         // 6 - reset sim800
                         // 7 - GET GPRS запрос
                         // 8 - подключиться к MQTT серверу
                         // 9 - удалить все SMS
                         // 11 - тестовый опрос модема раз в 5 минут
                         // 20 - команда начальной отправки СМС
                         // 30 - одиночная текстовая команда для модема

int8_t _step = 0; //текущий шаг в процедуре GPRS_traffic -глобальная /признак, что процедура занята
    mod_com  modem_comand; //  структура принимающая данные из очереди команд для SIM800

  for (;;){
   _interval = 5; // интервал в секундах ожидания ответа от модема (по умолчанию для всех команд)  
  if (!_AT_ret && _step !=0)   // если предидущая команда неудачно прекратить попытки  
      { _step=14; SMS_currentIndex = 0; // сбросить текущую смс
        _AT_ret=true; 
      if (command_type == 7) retGetZapros(); // если сбой при выполнении GET запроса, закрыть запрос  
      else if (command_type == 8) retTCPconnect(); // если сбой при подключении к сайту MQTT, закрыть соединение  
      else app->modemOK = false; 
         }

  // если никакая команда не исполняется и очередь пуста - задача останавливается до появления элементов в очереди
  if (command_type == 0 && _step == 0 ) {
   if (xQueueReceive(app->queue_comand, &modem_comand, portMAX_DELAY) == pdTRUE){
      _first_com = String(modem_comand.text_com);
      command_type = modem_comand.com;  
      flag_modem_resp = modem_comand.com_flag;
     //t_rst - задать отсчет до следующей проверки модема 
      if (command_type == 6 || command_type == 16) {t_rst=millis(); IsRestart = false;} // признак однократной отправки Restart в модем
      if (command_type == 11) {t_rst=millis(); app->IsOpros = false;}

     #ifndef NOSERIAL      
        Serial.print("                             Read from QUEUE comand - ");  Serial.print(command_type); 
        Serial.print(" text : "); Serial.println(_first_com);
     #endif     
   }
  }

  if (command_type == 6 || command_type == 16) { //6 Reset SIM800 16 init без рестарта
  switch (_step) {
    case 0:
     t_rst = millis();   //Установить интервал, для предотвращения повторного сброса 
     digitalWrite(MODEM_POWER_ON, LOW);
     vTaskDelay(800);

    // Turn on the Modem power first
     digitalWrite(MODEM_POWER_ON, HIGH);

    // Pull down PWRKEY for more than 1 second according to manual requirements
    digitalWrite(MODEM_PWRKEY, HIGH);
    vTaskDelay(100);
    digitalWrite(MODEM_PWRKEY, LOW);
    vTaskDelay(1000);
    digitalWrite(MODEM_PWRKEY, HIGH);

     vTaskDelay(2300); // меньше чем 2,3 секунды модем еще не готов
       PIN_ready = false;
      CALL_ready = false;
      app->GPRS_ready = false; // признак подключения GPRS
      GET_GPRS_OK = false; // признак удачного HTTP GET запроса
      app->MQTT_connect = false;
      app->TCP_ready=false;
      app->modemOK = false; 
      comand_OK = false;    
      _comm=""; _povtor = 1; //AT - Автонастройка скорости
      break;
    case 1:
      _comm=F("+CFUN=1,1"); _povtor = 2; //Reset the MT before setting it to <fun> power level.
      break;  
    case 2:
      _comm=F("+CFUN=0"); _povtor = 2; //Set Phone Functionality - Minimum functionality
      break;   
    case 3:
      _comm=F("+CIURC=1"); _povtor = 2;// включить отображение состояния
      break;  
    case 4:
      _comm=F("+CFUN=1"); _povtor = 2;  // Full functionality (Default)
      break;   
    case 5:
      _comm=F("E0"); _povtor = 1;       //E0 отлючаем Echo Mode  
      break;      
    case 6:  
      _comm=F("+CMGF=1;+CMEE=2"); _povtor = 1;  // Включить TextMode для SMS (0-PDU mode) Задать расширенный ответ при получении ошибки +CMEE=2;
      break;       
    case 7:
      _comm=F("+CPIN?;+CCALR?"); _povtor = 1;// запрос на готовность симки (отсутствие PIN) и готовность звонков +CCALR?
      break;      
    case 8:
      _comm=F("+CPBS=\"SM\""); _povtor = 2;// указать место хранения номеров - SIM
      break;   
    case 9: 
      _comm=F("+CLIP=1;+CCALR?"); _povtor = 2;// Включаем АОН 
      _interval = 20; // интервал в секундах ожидания ответа от модема
      break;    
       // AT+CLTS=0 Отключить вывод текущей временной зоны при каждом входящем звонке
       // AT+CUSD=0 Отключить вывод дополнительной информации при каждом входящем звонке
    case 10:
     // _comm=F("+CLTS=0;+CUSD=0"); _povtor = 2;
      _comm=F("+CLTS=1;+CUSD=0"); _povtor = 2;     
      break;   
   
    case 11:
      _comm=F("&W"); _povtor = 1; //сохраняем значение (AT&W)!
      break;  
    case 12:
      _comm=F("+CCALR?;+CPBS?"); _povtor = 2; // выяснить количество номеров на СИМ
      break;     
    case 13:
  _timeout = millis();             // Переменная для отслеживания таймаута (35 секунд)
  while (!PIN_ready && !CALL_ready && millis()-_timeout <= 35000) vTaskDelay(5); // Ждем ответа 35 секунд, если пришел ответ или наступил таймаут, то...   
      if (PIN_ready && CALL_ready){
        t_rst=millis(); // задать отсчет до следующей проверки модема
         app->modemOK=true;    // модем готов к работе
         #ifndef NOSERIAL   
           Serial.println("                              MODEM OK");               // ... оповещаем об этом и...
           app->_log->println("MODEM OK");
         #endif    
       }
      #ifndef NOSERIAL         
        else // Если пришел таймаут, то... модем не готов к работе
          Serial.println("modem Timeout...");               // ... оповещаем об этом и...
      #endif       
     command_type = 0; //не повторять больше
     _step = 0; // дать возможность запросов из вне
     _comm="";
       break; 
    }//end select
  } // end if comm=6

  else if (command_type == 4){ // сохранить номера на СИМ из массива
     if (_step == 0)
      _num_index = 0; //счетчик номеров из телефонной книги при записи номеров из массива на СИМ
      _step = 2; 
     while (app->PhoneOnSIM[_num_index][0]==NULL && _num_index < app->alloc_num[1])
               ++_num_index;
       
     if (_num_index < app->alloc_num[1])
      {
       _comm = FPSTR("+CPBW=");
             if (app->indexOnSim[_num_index]>0) _comm += String(app->indexOnSim[_num_index]);               
               _comm += F(",\""); 
             for (uint8_t g=0; g<DIGIT_IN_PHONENAMBER; ++g) _comm += app->PhoneOnSIM[_num_index][g]; 
               _comm += F("\",129,\""); 
               _comm += String(app->CommentOnSIM[_num_index]);
               _comm += charQuote; //"\"";
        _povtor = 0;
        ++_num_index;
      } 
      else _step = 14; // создать условие для выхода из команды
   }

  else if (command_type == 5) // 5 - удалить все номера из сим карты
   { if (_step == 0)  _num_index = 0; //счетчик номеров из телефонной книги при записи номеров из массива на СИМ
       _step = 2;
     if (_num_index < app->alloc_num[1])  
      { _comm = FPSTR("+CPBW=");
       _comm += String(_num_index+1);
       ++_num_index;
        //  #ifndef NOSERIAL            
        //     Serial.println(_comm);
        //  #endif   
        //  goto EndATCommand;
      }
      else _step = 14; // создать условие для выхода из команды      
   }

  else if (command_type == 9){ // удалить все SMS
     if (_step == 0){
        _step = 13; // создать условие для одноразового прохода
        _comm=F("+CMGDA=\"DEL ALL\""); _povtor = 2;
     }
   }

  else if (command_type == 11){ // Тестовая команда, раз в 5 минут
     if (_step == 0){ 
       _comm=""; _povtor = 0;  
     }
     else if(_step == 1) {
       PIN_ready = false; command_type = 6; CALL_ready = false; 
       _step = 12;// переход к команде сброса, следующий шаг 13 - ожидание ответа PIN READY
       app->modemOK = false; 
      _comm=F("+CCALR?;+CPIN?"); _povtor = 1;// запрос на готовность симки (отсутствие PIN) и готовность сети
     }
   }
  else if (command_type == 30) //выполнить одиночную команду для модема
   { if (_step == 0){ 
       _step = 13;  // создать условие для одноразового прохода
       _comm=_first_com; 
       if (flag_modem_resp == -1) { _povtor = -1; flag_modem_resp=0;} else _povtor = 2;
     }
   }
  else if (command_type == 20) //выполнить команду для отправки СМС
   {  
      if (_step == 0){ //выполнить начальную команду для отправки СМС
        flag_modem_resp = 6; // флаг на ожидание приглашения для ввода текста '>'
        _comm = _first_com.substring(0, _first_com.indexOf(charCR)); // сначала передать команду до перевода каретки и дождаться приглашения для ввода текста '>'
         _povtor = -1;
      }
     else if (_step == 1){ 
        _step = 13;  // создать условие для одноразового прохода
        _comm = _first_com.substring(_first_com.indexOf(charCR)); // передать текст сообщения после символа перевода каретки
        _povtor = -1;
       _interval = 55; // интервал в секундах ожидания ответа от модема
      }  
   }

  else if (command_type == 7) { // GPRS GET
    _interval = 55; // интервал в секундах ожидания ответа от модема  
    switch (_step) {
      case 0: 
        ++_step; goto EndATCommand;//пропустить в рабочем скетче
       _comm=F("+HTTPSTATUS?"); _povtor = 0; 
        break;                 
     case 1:
       _povtor = 1; // установить для всех команд из этой серии
       if (!app->GPRS_ready) // если модем еще не настроен на GPRS
         { _comm = FPSTR(GPRScomsnt);
           _comm += F("3,1,\"Contype\", \"GPRS\""); 
         }
       else _step=13; 
        break;
     case 2:
         _comm = FPSTR(GPRScomsnt);
         _comm += F("3,1,\"APN\", \"");
         _comm += app->_gprsapn +"\"" ; 
         if (app->_gprsuser != strEmpty) {
         _comm += F(";");
         _comm += FPSTR(GPRScomsnt);           
         _comm += F("3,1,\"USER\", \"");
         _comm += app->_gprsuser +"\"" ;            
         }
         if (app->_gprspwd != strEmpty) {
         _comm += F(";");
         _comm += FPSTR(GPRScomsnt);           
         _comm += F("3,1,\"PWD\", \"");
         _comm += app->_gprspwd +"\"" ;                
         }       
        break;  
     case 3:
         _comm=FPSTR(GPRScomsnt);
         _comm += F("1,1");        
        break; 
     case 4:// проверить подключение и получить GPRS_ready
         _comm = FPSTR(GPRScomsnt);
         _comm += F("2,1"); 
         _step=13; // временно, чтобы перескочить запрос, только установить GPRS соединение
        break; 
    //  case 5:
    //      _comm =F("+HTTPINIT"); 
    //     _comm += colon;
    //     _comm +=F("+HTTPPARA=\"CID\",1"); 
    //     _comm += colon;

    //     _comm += F("+HTTPPARA=\"URL\",\"");
    //     _comm += _first_com; // атрибут адреса из очереди команд
    //     _comm += "\"";

    //     _comm += colon;
    //     _comm +=F("+HTTPACTION=0");
    //     GET_GPRS_OK = false;
    //     goto sendATCommand;        
    //     break; 
    //  case 6: 
    //     if (GET_GPRS_OK)  //если сервер ответил с кодом 200, считать содержимое ответа
    //       {
    //         _comm=F("+HTTPREAD");
    //         goto sendATCommand; }
    //     else 
    //      { 
    //       #ifndef NOSERIAL
    //        Serial.println(" web access fail");
    //       #endif         
    //        _step=7; goto EndATCommand;
    //      }
    //     break; 
      case 7: 
       _comm=F("+HTTPTERM"); 
         _step = 13;    
        break; 

      } // end swith select
  } //end    if comm=7    
  else if (command_type == 8) { // connect to MQTT server
     _interval = 21; // интервал в секундах ожидания ответа от модема  
    switch (_step) {
      case 0: 
      if(app->TCP_ready) {_step=1; goto EndATCommand;}//признак подключения к MQTT серверу
       app->MQTT_connect=false;
       _comm  = F("+CIPSTART=\"TCP\",\"");
       _comm += app->_mqttServer; //MQTT_SERVER;
       _comm += F("\",\"");
       _comm.reserve(_comm.length()+8);
       {String st_temp8=String(app->_mqttPort);
       _comm += st_temp8; } //PORT;
       _comm += F("\""); _povtor = 0;     
        break;      
      case 1: 
        //if (!app->MQTT_connect) {_step=14; goto EndATCommand;} //признак неудачного TCP подключения 
       _comm  = F("+CIPSEND="); _comm += String(modem_comand.text_com[1] + 2); // отправить определенное количество байт в модем
        _povtor = 0;       
        break;    
      case 2: 
       _step = 13;         
        break;                            
      } // end swith select  
  } // end    if comm=8 
  else if (command_type != 0)  // если тип команды задан, но не обработан - сбросить все значения
   {_step = 0; command_type = 0; _comm="";}

  if (_step > 13) {_step = 0; command_type = 0; _comm="";}  // Максимальное число итераций в команде - 13

// START AT Comand 
  if (command_type != 0) {
// только при отправке текста СМС или при отправке данных, после получения приглашения > не добавлять AT в начало команды и '\r' в конце
     if (!((flag_modem_resp == 6 || flag_modem_resp == 8) && _step == 13)) {
       String _temp_com = _comm;
      _comm = F("AT");
      _comm += _temp_com;
      _comm += String(charCR); //Добавить в конце командной строки <CR>
       if (command_type==30 && _comm.indexOf(F("ATH")) > -1) one_call = false; //Serial.println(" ****** COMMAND ATH *********");}      
      } 

     if (flag_modem_resp != 0) t_last_command = millis(); // засечь время начала исполнения маркированной команды
   g=0;
  do {  // Цикл для организации повторной отправки команды, в случае не удачи 
  comand_OK = false; // при каждой попытке сбросить признак удачного выполнения команды
  //if (flag_modem_resp == 8 && _step > 1) comand_OK = true; //при отправке определнного количества байт не ждать ответа, перейти к след. команде
  if (_step == 13 && (flag_modem_resp == 6 || flag_modem_resp == 8)) { //надо отправить модуль данные после приглашения на ввод '>'
     if (flag_modem_resp == 6) {
        SIM800.write(_comm.c_str());               // Отправляем текст модулю из строки
        // #ifndef NOSERIAL
        //  Serial.print("Try to send SMS text "); Serial.println(_comm);  // Дублируем команду в монитор порта
        // #endif          
      }
     else {
       if (flag_modem_resp == 8) {// Отправляем битовый массив модулю
        for (int8_t f=0; f<2; ++f) {// Отправить фиксированный заголовок 2 байта
         SIM800.write(modem_comand.text_com[f]); 
        // Serial.print(modem_comand.text_com[f],HEX); Serial.print(' ');
         if (f==1) {// второй байт это длина остального сообщения которое надо отправить
          for (int r=0; r<modem_comand.text_com[f]; ++r) { 
              SIM800.write(modem_comand.text_com[f+r+1]);  
             // Serial.print(modem_comand.text_com[f+r+1],HEX); Serial.print(' ');
          }
         } 
        }
         //сбросить флаг только при признаке 6 (нужен чтобы отследить приглашение на ввод данных ">" при отправке СМС или данных в TCP соединение)
         flag_modem_resp = 0;  
       }
     }
     //сбросить флаг только при признаке 6 или 8 (нужен чтобы отследить приглашение на ввод данных ">" при отправке СМС или данных в TCP соединение)
     // flag_modem_resp = 0;   
  }
  else {  SIM800.write(_comm.c_str());       // Отправляем AT команду модулю из строки
     #ifndef NOSERIAL
       Serial.print("                              Command : ");  Serial.println(_comm);   // Дублируем команду в монитор порта
     #endif  
  }  
  _timeout = millis();// + _interval * 1000;     // Переменная для отслеживания таймаута (_interval секунд)
    while (!comand_OK && millis()-_timeout <= _interval * 1000)  // Ждем ответа _interval секунд, если пришел ответ или наступил таймаут, то...  
               vTaskDelay(50);

         _AT_ret=comand_OK;
         if (_AT_ret) t_rst=millis();
        #ifndef NOSERIAL 
         else { // Если пришел таймаут, то...
          Serial.println("                              AT Timeout...");               // ... оповещаем об этом и...
          Serial.print("                              _comm= "); Serial.print(_comm);
          Serial.print(" command_type= "); Serial.print(command_type);
          Serial.print(" _step= "); Serial.println(_step);
          }      
        #endif 
        if (g > _povtor) break; // при превышении количества установленных попыток отправки - выйти из цикла
     ++g;  // счетчик попыток отправки текущей команды
   } while (!comand_OK && !app->SIM_fatal_error);   // Не пускать дальше, пока модем не вернет ОК  

   if (_comm.indexOf(F("+HTTPACTION")) > -1){ // ожидание ответа от модема "+HTTPACTION: 0,200"
     g=0;  _povtor= -1; _AT_ret = false;
  do {  
    _timeout = millis(); // + 6000;             // Переменная для отслеживания таймаута (6 секунд)
    while (!GET_GPRS_OK && millis()-_timeout<=6000)  // Ждем ответа 6 секунд, если пришел ответ или наступил таймаут, то...  
          vTaskDelay(100);
         _AT_ret=GET_GPRS_OK;
       #ifndef NOSERIAL            
        if (!_AT_ret)  // Если пришел таймаут, то...
          Serial.println("GET_GPRS_OK Timeout...");               // ... оповещаем об этом и...
       #endif                
        if (g > _povtor) break; //  return modemStatus;}
     ++g;
     } while (!GET_GPRS_OK && !app->SIM_fatal_error);   // Не пускать дальше, пока модем не вернет ОК       
   }

   if (_comm.indexOf(F("+CIPSTART")) > -1){ // ожидание ответа от MQTT сервера с удачным подключением CONNECT OK
     g=0; _povtor = -1; _AT_ret = false;
  do {  
    _timeout = millis();// + 6000;             // Переменная для отслеживания таймаута (6 секунд)
    while (!app->TCP_ready && millis()-_timeout<=6000)  // Ждем ответа 6 секунд, если пришел ответ или наступил таймаут, то...  
          vTaskDelay(100);
        _AT_ret=app->TCP_ready;
          #ifndef NOSERIAL           
           if (!_AT_ret)   // Если пришел таймаут, то...
             Serial.println("TCP_ready Timeout...");               // ... оповещаем об этом и...
          #endif 
        if (g > _povtor) break; //  return modemStatus;}
     ++g;
     } while (!app->TCP_ready && !app->SIM_fatal_error);   // Не пускать дальше, пока модем не вернет ОК  
   }  
       
    ++_step; //увеличить шаг на 1 для перехода к следующей команде
  } // end ATCommand

  EndATCommand:
    vTaskDelay(1);
  }
}

void Sim800_setup() {
    #ifdef MODEM_RST
      // Keep reset high
      pinMode(MODEM_RST, OUTPUT);
      digitalWrite(MODEM_RST, HIGH);
    #endif

    pinMode(MODEM_PWRKEY, OUTPUT);
    pinMode(MODEM_POWER_ON, OUTPUT);


 SMS_incoming_num[0]=NULL; // номер с которого пришло СМС - для ответной СМС
 SMS_text_num[0]=NULL;  // номер телефона из СМС
 SMS_text_comment[0]=NULL; // комментарий к номеру из СМС

    // Set GSM module baud rate and UART pins
    SIM800.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);

   ReLoadBinMassiv(); // обнулить все номера телефонов в массиве и загрузить из BIN файла все номера 
  
  //  #ifndef NOSERIAL 
  //  for (int16_t n=0; n < app->alloc_num[2]; ++n){
  //       Serial.print(String(n)); Serial.print(" - "); Serial.println(BINnum_to_string(app->phones_on_sim[n])); // для отладки отправляем по UART все что прочитали с карты.
  //  }
  //  #endif 

   queue_IN_SMS = xQueueCreate(max_queue, sizeof(int)); // очередь обработки СМС

  for (int v = 0; v < 250; ++v) // обнулить все номера телефонов в массиве
    {
     app->PhoneOnSIM[v][0] = NULL;
     app->CommentOnSIM[v][0] = NULL;
     app->indexOnSim[v]=0;
    }

// Создаем задачу с кодом из функции Task1code(),
  // с приоритетом 1 и выполняемую на ядре 0:
  xTaskCreatePinnedToCore(
                    GPRS_modem_traffic,   /* Функция задачи */
                    "Task3",     /* Название задачи */
                    2000,       /* Размер стека задачи */
                    NULL,        /* Параметр задачи */
                    1,           /* Приоритет задачи */
                    &Task3,      /* Идентификатор задачи, чтобы ее можно было отслеживать */
                    1);          /* Ядро для выполнения задачи (0) */

  vTaskDelay(30);
  //command_type = 6; // стартовые настройки модема
  app->add_in_queue_comand(6,"", 0);
  IsRestart = true; // признак однократной отправки Restart в модем
  app->add_in_queue_comand(9,"", 0); //удалить все смс сохраненные на СИМ карте
    // #ifndef NOSERIAL    
    //   for (int g=0; g<number_comant_type; ++g) {
    //     Serial.print(FPSTR(comand_nume[g]));
    //     Serial.print(F(" num "));  Serial.print((const int8_t)pgm_read_word(&comand_prop[g][0]));
    //     Serial.print(F(" sms "));Serial.println((const int8_t)pgm_read_word(&comand_prop[g][1]));
    //   } 
    // #endif    
}

//Добавление нового СМС в очередь на обработку
void add_in_queue_SMS (int _innSMSindex){
  //AT+CMGD=n,0 - удалить сообщение с номером n
  //AT+CMGL="ALL" - прочитать все сообщения
  if (xQueueSend(queue_IN_SMS, &_innSMSindex, 0) == pdTRUE){
      #ifndef NOSERIAL      
        Serial.print("Add in QUEUE SMS - "); Serial.println(_innSMSindex);
      #endif       
  }
}

void ReLoadBinMassiv(){
    for (int v = 0; v < total_bin_num; ++v) {// обнулить все номера телефонов в массиве
    app->phones_on_sim[v] = 0;}
   
    app->readBINfile(); // загрузить из BIN файла все номера
}

void Sim800_loop() {
  static String _logCallstring ="";
// сбрасывать модуль через интервал - раз в 30 часов
if (millis() > 60*60*1000*30)   ESP.restart();

// Опросить модем раз в указанный интервал
  if (millis() - t_rst > 11*60*1000 && app->modemOK && !app->IsOpros && !app->SIM_fatal_error) 
 {    app->IsOpros = true;    
      app->add_in_queue_comand (11,"", 0);
 }

 // Если есть проблемы с модемом попытаться сбросить модем
 if (!app->modemOK && millis() - t_rst > 3*60*1000 && !IsRestart && !app->SIM_fatal_error) 
   {      IsRestart = true; // признак однократной отправки Restart в модем 
         app->add_in_queue_comand (6,"", 0);
   }

if (SIM800.available())   {                   // Если модем, что-то отправил...
    String _response = "";              // Переменная для хранения ответа модуля
    // _response = SIM800.readStringUntil('\n');             // Получаем ответ от модема для анализа
    // _response.trim();
    // if (_response == strEmpty) _response = SIM800.readStringUntil('\n');
   // _response = SIM800.readString();
       char inchar; static int8_t inn_r=0; // количество раз получения символа \r \n
    _response=strEmpty;
   while (SIM800.available()){
      int16_t num_in_simvol=0; // порядковый номер входящего символа
        inchar = SIM800.read(); ++num_in_simvol;
        // Serial.print(' ');Serial.print(inchar,HEX);
  //ответ от SIM800 заключен в "скобки" из двух символов <CR><CN> -- respons -- <CR><CN>
  //получиь, в каждом заходе, чистый ОДИНОЧНЫЙ ответ без этих "скобок"
  //данные из TCP соединения приходят "голые" без заголовка и окончания
  // приглашение на ввод текста СМС или данных приходит с начальными символами <CR><CN> но без завершающих 
  // последний символ пробел 0D 0A 3E 20 <CR><CN> '>' '_'
  // это надо разделять при получении
        if (inchar == '\r') {
          char inchar_n; inchar_n = SIM800.read(); ++num_in_simvol;// Serial.print(' '); Serial.print(inchar_n, HEX);// считать следующий символ
          if (inchar_n == '\n') {// если он \n обработать
           ++inn_r;
           // если первые скобки <CR><CN> пришли в одной строке с ответом от TCP соединения
           if (inn_r == 1 && num_in_simvol > 1) break; //дочитать остаток строки в следующем цикле переменная  inn_r - static
           if (inn_r == 2) {inn_r=0; break;}
          }
          else { // если след символ не \n добавить оба символа в строку ответа _response
           _response += inchar; _response += inchar_n;
          }
        }
        else if (inchar == 0x3E && inn_r == 1){ // после первых скобок <CR><CN> пришел символ > для ввода данных ...
           _response += inchar; 
           inchar = SIM800.read(); ++num_in_simvol; //считать след симол
           if (inchar == 0x20) { // если это пробел завершить прием строки ответа и не ждать вторых скобок <CR><CN>
             _response += inchar; inn_r=0; break;
           }
           else _response += inchar; 
        }
        else { _response += inchar; }
    } 
      #ifndef NOSERIAL      
        Serial.println("          " + _response);                  // Если нужно выводим в монитор порта  
        // for (int f=0;f<_response.length();++f){
        //   Serial.print(_response[f],HEX); Serial.print(' ');
        // }
      #endif 
    int firstIndex = 0;
    String textnumber = "";                    // переменая с текстовым значением номера из телеф. книги
    String textnumbercomment = "";            // переменая с текстовым значением коментария из телеф. книги  (не больше 6 символов)
    // ... здесь можно анализировать данные полученные от GSM-модуля
    if (flag_modem_resp == 8)  {
        if (_response.indexOf(F("CONNECT OK")) > -1) app->TCP_ready=true;
        if (_response.indexOf(F("CONNECT FAIL")) > -1) { app->MQTT_connect = false; app->TCP_ready=false;}      
    }  
    if ( _response.indexOf('>') > -1 && (flag_modem_resp == 6 || flag_modem_resp == 8)){ // запрос от модема на ввод текста сообщения
       comand_OK = true; 
      // #ifndef NOSERIAL      
      //   Serial.println("Need sms text or MQTT data");
      // #endif       
    }
    else if (_response.indexOf(F("+CPIN: READY")) > -1) {PIN_ready = true; app->SIM_fatal_error = false;}
    else if (_response.indexOf(F("+CPIN: NOT INSERTED")) > -1) simNotReady();
    else if (_response.indexOf(F("+CPIN: NOT READY")) > -1 ) CALL_ready = false;    
    else if (_response.indexOf(F("+CCALR: 1")) > -1) CALL_ready = true;
    else if (_response.indexOf(F("+CCALR: 0")) > -1) CALL_ready = false;
    else if (_response.indexOf(F("+CCLK")) > -1) 
     { String _tempTime = _response.substring(_response.indexOf(charQuote)+1, _response.indexOf(charQuote)+18); // дата / время внутри кавычек
      _tempTime[_tempTime.indexOf(charComma)] = charSemicolon; // заменить запятую на точку с запятой, чтобы в CSV правильно отображались столбцы
      String tempcallTimeLog = _tempTime + _logCallstring;
      tempcallTimeLog[0] = _tempTime[6]; tempcallTimeLog[1] = _tempTime[7];
      tempcallTimeLog[6] = _tempTime[0]; tempcallTimeLog[7] = _tempTime[1];   
      app->_log->println(_tempTime);         
      app->writeTXTstring(tempcallTimeLog,4);
      //app->_checklogFileSize();      
     } 
    else if (_response.indexOf(F("PSUTTZ:")) > -1) {
      app->carrYar =_response.substring(_response.indexOf(charComma)-4, _response.indexOf(charComma));
      //  #ifndef NOSERIAL 
      //   Serial.print("app->carrYar: "); Serial.println(app->carrYar);   
      // #endif      
    }      
    else if (_response.indexOf(F("+CLIP:")) > -1) { // Есть входящий вызов  +CLIP: "069123456",129,"",0,"069123456asdmm",0  

      //one_call - признак однократного снятия трубки, т.к. при повторных сигналах вызова и сбое в модеме реле может срабатывать многократно 
      // и очередь команд забьется одинаковыми командами
        if (one_call) return; 
           one_call = true;

      int phoneindex = _response.indexOf(F("+CLIP: \""));// Есть ли информация об определении номера, если да, то phoneindex>-1
      String innerPhone = "";                   // Переменная для хранения определенного номера
      if (phoneindex >= 0) {                    // Если информация была найдена
        phoneindex = _response.indexOf(charQuote, phoneindex)+1; //первые кавычки
        int _quote2 = _response.indexOf(charQuote, phoneindex);  //вторые кавычки
        if ((_quote2 - phoneindex) - DIGIT_IN_PHONENAMBER > 0) phoneindex = phoneindex + ((_quote2 - phoneindex) - DIGIT_IN_PHONENAMBER);
        innerPhone = _response.substring(phoneindex, _quote2 ); // номер внутри кавычек
        
      #ifndef NOSERIAL 
        Serial.print("phoneindex: "); Serial.println(phoneindex);             
        Serial.print("Number: "); Serial.println(innerPhone); // Выводим номер в монитор порта   
        //Serial.print("_quote2: "); Serial.println(_quote2);
        poisk_num(innerPhone); // Выводим номер в монитор порта  
      #endif          

        //поиск текстового поля в ответе +CLIP: "069071234",129,"",0,"",0
        int last_comma_index = _response.lastIndexOf(',');
        int fist_comma_index = String(_response.substring(0,last_comma_index-1)).lastIndexOf(',');
        if ((last_comma_index-fist_comma_index) >= DIGIT_IN_PHONENAMBER)
          textnumber=_response.substring(fist_comma_index+2, fist_comma_index+2+DIGIT_IN_PHONENAMBER); 
        else textnumber="";

        if (_response.length() > fist_comma_index+2+DIGIT_IN_PHONENAMBER) //если в текстовом поле еще есть коментарий
        { 
          textnumbercomment=_response.substring(fist_comma_index+2+DIGIT_IN_PHONENAMBER, _response.length()-3);
         #ifndef NOSERIAL            
          Serial.print("TextNumberComment: "); Serial.println(textnumbercomment);
         #endif 
        }
        #ifndef NOSERIAL  
          Serial.print("TextNumber: "); Serial.println(textnumber);
        #endif  
      }
      // Проверяем, чтобы длина номера была больше 6 цифр, и номер должен быть в списке
      // app ->_whiteListPhones Белый список телефонов максимум 3 номера по 8 симолов
      // uint32_t _now = app->getTime();
       String _tempString ="";
      // if (_now < 1483228800UL)
      //  _tempString +=dateTimeToStr(_now);
      // else {
      //   app->_log->print("Time NOT set ");
      //   _tempString +=F("Time NOT set ");
      // }
      _tempString +=F("Call from:");      
      _tempString +=innerPhone;
      if (innerPhone.length() > DIGIT_IN_PHONENAMBER-3 && app->_whiteListPhones.indexOf(innerPhone) > -1) {
         regular_call(); // Если звонок от БЕЛОГО номера из EEPROM - ответить, включить реле и сбросить вызов
        _tempString +=F(" from WhiteList");
        _logCallstring =";";
        _logCallstring += innerPhone;
        _logCallstring += ";;;";        
      }         
      else if (innerPhone == textnumber && textnumber.length() == DIGIT_IN_PHONENAMBER){
        regular_call(); // Если звонок от БЕЛОГО номера из СИМ карты - ответить, включить реле и сбросить вызов
        _tempString +=F(" from SIM number");
        _logCallstring =";;";
        _logCallstring += innerPhone;
        _logCallstring += ";;";                
      }  
      else if (poisk_num(innerPhone)>-1) {
        regular_call(); // Если звонок от БЕЛОГО номера из BIN массива - ответить, включить реле и сбросить вызов   
        _tempString +=F(" from BIN number");  
        _logCallstring =";;;";
        _logCallstring += innerPhone;
        _logCallstring += ";";                   
      }  
    // Если нет, то отклоняем вызов  
      else { 
        app->add_in_queue_comand(30, "H", 0);
        _tempString +=F(" WRONG number"); 
        _logCallstring =";;;;";
        _logCallstring += innerPhone;      
        }
        _tempString += F(" on ");
        // #ifndef NOSERIAL  
        //   Serial.print("_logCallstring - ");        
        //   Serial.println(_logCallstring);
        // #endif            
        app->_log->print(_tempString);     
        app->add_in_queue_comand(30, "+CCLK?", 0); //Запрос на текущее время 08/12/2025                
    }
    //********* проверка отправки SMS ***********
    else if (_response.indexOf(F("+CMGS:")) > -1) {       // Пришло сообщение об отправке SMS
      //t_last_command = millis();  
      #ifndef NOSERIAL        
        Serial.print("Sending SMS ");
        Serial.print("flag_modem_resp = "); Serial.println(String(flag_modem_resp));   
      #endif
    }
    //********** проверка приема SMS ***********
    else if (_response.indexOf(F("+CMTI:")) > -1) {       // Пришло сообщение о приеме SMS
      #ifndef NOSERIAL       
        Serial.println("Incoming SMS");

      #endif  
      int index = _response.lastIndexOf(',');   // Находим последнюю запятую, перед индексом
      String result = _response.substring(index + 1, _response.length()); // Получаем индекс
      result.trim();                            // Убираем пробельные символы в начале/конце
      #ifndef NOSERIAL        
        Serial.print("new mess "); Serial.println(result);
      #endif
      //Добавляем текущую СМС в очередь на обработку
      add_in_queue_SMS(result.toInt());
    }
    else if (_response.indexOf(F("+CMGR:")) > -1) {    // Пришел текст SMS сообщения 
        _response += '\r' + SIM800.readStringUntil('\n'); // читаем до конца строки (без OK) 
     //********** 30_05_2025 *************************
         if (SIM800.available()) {
           String dop_string = ""; // читаем до конца строки (без OK) 
          char dop_char;     
        while (SIM800.available()) {
          dop_char = SIM800.read();
          dop_string += dop_char;
        }  
         if (dop_string.indexOf(F("OK")) > -1) comand_OK = true; // принудительно завершить команду (не дожидаясь Ok от модема)        
          #ifndef NOSERIAL        
            Serial.print("dop_string "); Serial.println(dop_string);
          #endif              
         }            
     //********** 30_05_2025 *************************
        parseSMS(_response);        // Распарсить SMS на элементы
    }
    else if (_response.indexOf(F("+CPBS:")) > -1){ // выяснить количество занятых номеров на СИМ и общее возможное количество
      uint8_t index1 = _response.indexOf(',') + 1;   // Находим запятую, перед количеством имеющихся номеров
      uint8_t index2 = _response.lastIndexOf(',');   // Находим последнюю запятую, перед общим количеством номеров
     String used_num = _response.substring(index1, index2);
       app->alloc_num[0] = used_num.toInt();
        used_num = _response.substring(index2+1);
       app->alloc_num[1] = used_num.toInt();
      #ifndef NOSERIAL          
        Serial.print("exist_numer - "); 
        Serial.print(app->alloc_num[0]);  
        Serial.print(" : total_numer - "); 
        Serial.println(app->alloc_num[1]); 
      #endif        
    }
    else if (_response.indexOf(F("+CPBF:")) > -1) { // +CPBF: 4,"078123456",129,"078123456Manip"
      String phonen_index; 
        firstIndex = _response.indexOf(',');
        phonen_index = _response.substring(7, firstIndex); 
        textnumber =  _response.substring(firstIndex+2, firstIndex+2+DIGIT_IN_PHONENAMBER);
        textnumbercomment =_response.substring(_response.lastIndexOf(',')+2, _response.lastIndexOf('\"'));
           #ifndef NOSERIAL     
             Serial.print("File String +CPBF: index= "); Serial.print(phonen_index); 
             Serial.print(" ; number= "); Serial.print(textnumber);                          
             Serial.print(" ; comment= "); Serial.print(textnumbercomment); 
             Serial.print(" flag_modem_resp = "); Serial.println(flag_modem_resp);    
             app->_log->print("File String +CPBF: index= "); app->_log->print(phonen_index); 
             app->_log->print(" ; number= "); app->_log->print(textnumber);                          
             app->_log->print(" ; comment= "); app->_log->println(textnumbercomment);         
           #endif 
      if (flag_modem_resp == 2)  // одиночный поиск номера из СМС 
       {
        SMS_phoneBookIndex = phonen_index.toInt();
           #ifndef NOSERIAL   
             Serial.print("+CPBF: INT index= "); Serial.println(SMS_phoneBookIndex);              
           #endif            
       // t_last_command = millis();  
       }
      else if (flag_modem_resp == 5)  // перебор всех номеров СИМ и создание текстового файла
       {  // сформировать строку для добавления в CSV(TXT) файл
          _response=phonen_index;
          _response +=';';
          _response +=textnumber;  
          _response +=';'; 
          _response +=textnumbercomment;  
          //_response +=charCR + charLF;//"\r\n"; 
       //Заполнить массив номерами и коментариями для записи в текстовый файл
        for (uint8_t m=0; m<DIGIT_IN_PHONENAMBER; ++m) app->PhoneOnSIM[phonen_index.toInt()][m] = textnumber[m];   
        for (uint8_t m=0; m<(DIGIT_IN_PHONENAMBER+6); ++m)    //textnumbercomment.length()        
            { if (textnumbercomment.length() < m)
                {app->CommentOnSIM[phonen_index.toInt()][m] = NULL; 
                break;
                }
              else                
                app->CommentOnSIM[phonen_index.toInt()][m] = textnumbercomment[m];                   
            }     

           #ifndef NOSERIAL     
             Serial.print("numer text= "); Serial.println(_response);
             app->_log->print("numer text= "); app->_log->println(_response);
           #endif                                  
       }    
    }    
    else if (_response.indexOf(F("+SAPBR:")) > -1){
         if (_response.indexOf(F("+SAPBR: 1,1")) > -1) 
            {
              app->GPRS_ready = true;
        #ifndef NOSERIAL   
          Serial.println("+SAPBR OBMEN OK GPRS_ready *********************************** !!!!");
        #endif               
              }
         else if (_response.indexOf(F("+SAPBR: 1,2")) > -1 || _response.indexOf(F("+SAPBR: 1,3")) > -1)
           {
             app->GPRS_ready = false;
        #ifndef NOSERIAL   
          Serial.println("+SAPBR OBMEN NOT GPRS_ready xxxxxxxxxxxxxxxxxxxx !!!!");
        #endif             
           }
       }  
    else if (_response.indexOf(F("+HTTPACTION:")) > -1){
        if (_response.indexOf(F("+HTTPACTION: 0,200")) > -1) {
           GET_GPRS_OK = true;
        // #ifndef NOSERIAL   
        //   Serial.println("OBMEN OK GET_GPRS_OK *********************************** !!!!");
        // #endif             
           }
         else {GET_GPRS_OK = false;
        // #ifndef NOSERIAL   
        //   Serial.println("OBMEN NOT GET_GPRS_OK xxxxxxxxxxxxxxxxxxxx !!!!");
        // #endif           
          }
       } 
    else if (_response.indexOf(F("+HTTPREAD:")) > -1) { 
          _response = SIM800.readStringUntil('\n');
         #ifndef NOSERIAL   
           Serial.print("   HTTPREAD "); Serial.println(_response);
         #endif          
        //  Ans_parse(_response); //Разбор ответа от Сервера
     }                             

    if (_response.indexOf(F("OK")) > -1) { // если происходит соединение с MQTT сервером отследить CONNECT OK
      comand_OK = true;
      String SMSResp_Mess;
     if (SMS_currentIndex !=0 && flag_modem_resp == 6){ //&& millis() > t_last_command
        #ifndef NOSERIAL        
          Serial.println ("Message was sent. OK");
        #endif
        EraseCurrSMS();// Удалить текущую СМС из памяти модуля
        flag_modem_resp=0;        
      }
      else if (flag_modem_resp==2 && millis() > t_last_command) // завершен одиночный поиск номера из СМС - приступить к выполнению команды
        {
           #ifndef NOSERIAL     
             Serial.print("Global phonenumber index="); Serial.println(String(SMS_phoneBookIndex)); //.toInt()
           #endif        
          flag_modem_resp=0; 
          made_action(num_text_comanda, sms_answer);       
        }
      else if (flag_modem_resp==5 && millis() > t_last_command) // завершено создание файла с перечнем номеров
        { uint8_t count_row=0;
          for (uint8_t j=0; j<250; ++j)
             {
                if (app->PhoneOnSIM[j][0] != NULL)
                 { _response=String(j);
                   _response +=';';                 
                  for (uint8_t m=0; m<DIGIT_IN_PHONENAMBER; ++m)
                   _response +=app->PhoneOnSIM[j][m];
                   _response +=';';
                   _response += String(app->CommentOnSIM[j]); 
                   //_response +=charCR + charLF;//"\r\n";  
                   #ifndef NOSERIAL     
                     Serial.println("csv text= " + _response);              
                    #endif  
                   if (app-> writeTXTstring(_response, 1))  // записать строку с номером в текстовый файл
                     { ++count_row;
                       #ifndef NOSERIAL     
                        Serial.println("String append OK");              
                       #endif    
                     }
                    #ifndef NOSERIAL 
                    else   Serial.println("String append ERROR");    
                    #endif                                                                                                                           
                 } 
             } 
          flag_modem_resp=0;                    
           if (count_row>0)  
           { SMSResp_Mess = F("New TXT File created"); 
           #ifndef NOSERIAL     
             Serial.println("TXT create File"); 
           #endif 
           }
           else
           { SMSResp_Mess = F("NO TXT File created"); 
           #ifndef NOSERIAL     
             Serial.println("NO TXT File"); 
           #endif 
           }    
         sendSMS(String(SMS_incoming_num), SMSResp_Mess);    // отправить СМС с ответом              
        }        
      else if (flag_modem_resp==3 && millis() > t_last_command) // завершено одиночное добавление / редактирование номера из СМС - отправить ответ
      { 
         exist_numer(); // обновить количества использованных и доступных номеров на СИМ
         if (SMS_phoneBookIndex==0) { //Добавить новый номер
          SMSResp_Mess  =F("Phone-");
          SMSResp_Mess.reserve(SMSResp_Mess.length() + DIGIT_IN_PHONENAMBER + 1);
          {String temp1 = String(SMS_text_num);
          SMSResp_Mess += temp1;}
          SMSResp_Mess += F(" was successfully INCLUDED in White List!");
           }
         else if (SMS_phoneBookIndex > 0) { // перезаписать существующий номер   
          SMSResp_Mess  =F("Phone-");
          SMSResp_Mess.reserve(SMSResp_Mess.length() + DIGIT_IN_PHONENAMBER + 1);
          {String temp1 = String(SMS_text_num);
          SMSResp_Mess += temp1;}
          SMSResp_Mess += F(" already exist in White List! "); 
         //SMSResp_Mess += charCR + charLF;
          SMSResp_Mess +=F("Index -"); 
          SMSResp_Mess.reserve(SMSResp_Mess.length() + 5);
          {String temp1 = String(SMS_phoneBookIndex);
          SMSResp_Mess += temp1;}
          //SMSResp_Mess += charCR + charLF;; 
          SMSResp_Mess +=F(" Editing OK");
         }
        flag_modem_resp=0;
        sendSMS(String(SMS_incoming_num), SMSResp_Mess);    // отправить СМС с ответом
      } 
      else if (flag_modem_resp==4 && millis() > t_last_command) // завершено одиночное удаление номера из СМС - отправить ответ
      { //String SMSResp_Mess;
        SMSResp_Mess  =F("Phone-");
        SMSResp_Mess.reserve(SMSResp_Mess.length() + DIGIT_IN_PHONENAMBER + 1);
         {String temp1 = String(SMS_text_num);
        SMSResp_Mess += temp1;}
       if (SMS_phoneBookIndex > 0)
         SMSResp_Mess += F(" was successfully REMOVED in White List!");
       else
         SMSResp_Mess += F(" ERROR!! with REMOVED in White List!");   
        flag_modem_resp=0;         
        sendSMS(String(SMS_incoming_num), SMSResp_Mess); 
        exist_numer(); // обновить количества использованных и доступных номеров на СИМ           
      }
      // comand_OK = true;
     }
     if (_response.indexOf(F("ERROR")) > -1) {
      if (SMS_currentIndex != 0){
        #ifndef NOSERIAL        
         Serial.println ("Message was not sent. Error");
        #endif
        //flag_modem_resp=0;  
      }
      else if (flag_modem_resp==2) // завершен одиночный поиск номера из СМС - приступить к выполнению команды
        flag_modem_resp=0;   
      else if (flag_modem_resp==8) {// Неудачное подключение  TCP / сервер не отвечае не правильный IP - закрыть соединение
        if (app->TCP_ready) app->add_in_queue_comand(30,"+CIPCLOSE",-1); // Закрыть текущее соединение               
        app->MQTT_connect = false; app->TCP_ready=false;
        flag_modem_resp=0;
      }    
      if (_response.indexOf(F("SIM not inserted")) > -1) simNotReady();
     }
    //*************************************************
    // Обработка MQTT
    if (app->TCP_ready){
     
      if (_response[0] == 0x30)  { app->MQTT_connect = true; //print_MQTTrespons_to_serial(_response); // пришел ответ на публикацию в подписанном топике
         String s1; 
            s1 += _response.substring(4 , 4 + _response[3]);                 
        char _topik_path[s1.length()+1];
        for (int k=0; k < s1.length()+1; ++k) _topik_path[k]=s1[k];
         s1 = _response.substring(4 + _response[3]);
        byte* _payload;
        byte sb1 = (byte)s1[0];
        _payload = &sb1;
         app->mqttCallback(_topik_path, _payload, 1);
        }
      else if (_response[0] == 0x20 || _response[0] == 0x90) {app->MQTT_connect = true; } // print_MQTTrespons_to_serial(_response);}// пришло сообщение о подключении или удачной подписке на топик
      //else if (_response[0] == 0x40) {print_MQTTrespons_to_serial(_response);} //пришло сообщение о удачной публикации топика
      else if (_response.indexOf(F("CLOSED")) > -1) {app->MQTT_connect = false; app->TCP_ready=false;}
      else if (_response[0] == 0xD0) {app->MQTT_connect = true; } //print_MQTTrespons_to_serial(_response);} // подтверждения ping от MQTT сервера
     }  
  }

  #ifndef NOSERIAL   
    if (Serial.available())   {                   // Ожидаем команды по Serial...
       SIM800.write(Serial.read());                // ...и отправляем полученную команду модему
    }
  #endif
  
  if (SMS_currentIndex == 0 && app->modemOK) {// Если нет СМС в обработке  - проверить очередь
     if (xQueueReceive(queue_IN_SMS, &SMS_currentIndex, 0) == pdTRUE) {  //Если нет СМС в обработке - записать в переменную SMS_currentIndex - номер СМС из очереди
        String temp2 = "+CMGR=" + String(SMS_currentIndex);
        //char temp1[] = temp2;
        app->add_in_queue_comand(30, temp2.c_str(), 0);  //ОТправить входящую СМС на считывание содержания и обработку
     }   
  }

//************ Индикация состояния на GRIO19 *********************************
   static unsigned long t_Led; 
   static int count_led; // счетчик миганий
   static int16_t frequency_led; // время смены состояния
   static int16_t next_led;
   if (count_led == 0) { //исходное состояние
     frequency_led=1000;
   }

   if (millis()-t_Led > next_led) {

      if ( app->modemOK ) {
       if (count_led == -1) {
          if (app->MQTT_connect ) {count_led=3; frequency_led=400;} //моргает 4 раз потом пауза 
          else if (app->GPRS_ready) {count_led=2; frequency_led=400;} //моргает 3 раз потом пауза           
        } 
       else if (count_led < -1) {
        count_led=1; frequency_led=1000; }
       }      
      else {
         if ( count_led == -1) {count_led=4; frequency_led=400; //моргает 5 раз потом пауза
           if (app->SIM_fatal_error) {count_led=7; frequency_led=400;} //моргает 8 раз потом пауза      
         }  
      }

        digitalWrite(STATUS_LED_GPIO, !digitalRead(STATUS_LED_GPIO));
        t_Led=millis();  
        if (!digitalRead(STATUS_LED_GPIO)) {--count_led; next_led=frequency_led;} 
        else next_led=400;
     // Serial.print("count_led = ");Serial.print(count_led); Serial.print(" frequency_led = ");Serial.println(frequency_led);
   } 

}

void simNotReady(){
    PIN_ready = false; app->MQTT_connect = false; app->TCP_ready=false; CALL_ready = false; app->modemOK = false;
    app->SIM_fatal_error=true;
    #ifndef NOSERIAL        
    Serial.println ("SIM FATAL ERROR");
    #endif    
    app->_log->println(F("SIM FATAL ERROR"));
    digitalWrite(MODEM_POWER_ON, LOW); // отключить модем  
}  

void print_MQTTrespons_to_serial(const String& _resp){
    #ifndef NOSERIAL
     for (int h=0; h<_resp.length();++h){
         Serial.print(_resp[h], HEX); Serial.print(" ");
     }
      Serial.println();
    #endif  
}

void parseSMS(const String& msg) {                                   // Парсим SMS
  String msgheader  = "";
  String msgbody    = "";
  String msgphone   = "";
  int firstIndex =msg.indexOf("+CMGR: ");
  //msg = msg.substring(msg.indexOf("+CMGR: "));
  msgheader = msg.substring(firstIndex, msg.indexOf('\r'));           
  #ifndef NOSERIAL
    Serial.println("NEW !!! msgheader: " + msgheader);     
  #endif  
  msgbody = msg.substring(msg.lastIndexOf("\"")+1);  // Выдергиваем текст SMS
  
  msgbody = msgbody.substring(0,30);
  msgbody.trim();

   firstIndex = msgheader.indexOf("\",\"") + 3;
  int secondIndex = msgheader.indexOf("\",\"", firstIndex);
  msgphone = msgheader.substring(firstIndex, secondIndex); // Выдергиваем телефон
  // Записать номер с которого пришло СМС в глобальную переменную для общего доступа
  for (uint8_t j=0; j < msgphone.length()+1; ++j){
      if (j == msgphone.length())
        SMS_incoming_num[j] = NULL;  // если последний символ - добавить нулл в массив для финализации строки
      else  SMS_incoming_num[j] = msgphone[j];
  }
 // получить короткий номер с которого было послано СМС - последние симолы 
  String short_INnumber =String(SMS_incoming_num).substring(String(SMS_incoming_num).length()-DIGIT_IN_PHONENAMBER);
   #ifndef NOSERIAL 
    Serial.print("Phone: "); Serial.println(msgphone);                       // Выводим номер телефона
    Serial.print("Message: " ); Serial.println(msgbody);                      // Выводим текст SMS
    Serial.print("SMS_incoming_num : "); Serial.println(String(SMS_incoming_num)); //.c_str());  // Выводим текст SMS    
    Serial.print("short_INnumber: "); Serial.println(short_INnumber);     
  #endif
// Далее пишем логику обработки SMS-команд.
  // Здесь также можно реализовывать проверку по номеру телефона
  // И если номер некорректный, то просто удалить сообщение.
  
 // Если телефон в белом списке, то...
  if (String(SMS_incoming_num).length() > 6 && app ->_whiteListPhones.indexOf(short_INnumber) > -1) {
      msgbody = probel_remove(msgbody);
      madeSMSCommand(msgbody, msgphone);
     }
  else {   // если номер некорректный, то просто удалить сообщение.
   #ifndef NOSERIAL       
    Serial.println("Unknown phonenumber");
   #endif     
    EraseCurrSMS();// Удалить текущую СМС из памяти модуля
    }
}

// Удалить текущую СМС из памяти модуля
void EraseCurrSMS(){
   // удалить SMS, чтобы не забивали память модуля   
     if (SMS_currentIndex != 0) { // удалить текущую SMS, чтобы не забивали память модуля  
        String  temp_string = "+CMGD=" + String(SMS_currentIndex) + ",0";
          app->add_in_queue_comand(30, temp_string.c_str(), 0);
        #ifndef NOSERIAL        
          app->_log->println("Message was sent. OK ");
          app->_log->println(temp_string);          
        #endif          
          SMS_currentIndex=0;
          num_text_comanda = -1; //номер команды из СМС в массиве команд comand_nume
      }
}

// Удаление любых пробелов из строки
String probel_remove(const String& msg){
   String temp_resp="";
  for (uint8_t j=0; j < msg.length(); ++j) {
     if (msg[j] != ' ') temp_resp += msg[j];
  }
  return temp_resp; 
}

// проверка номера на вхождение в белый список 
bool number_on_white_list(const String& num_phone){
  bool temp_ret = false;  
  if (num_phone.length() > 6 && app ->_whiteListPhones.indexOf(num_phone) > -1)  // Если телефон в белом списке
     temp_ret = true;

  return temp_ret;
}

//выяснение полученой по SMS команды
void madeSMSCommand(const String& msg, const String& incoming_phone){
  // очистить все переменные
  SMS_phoneBookIndex = 0; // сбросить индекс искомого номера из СМС
  num_text_comanda = -1; //номер команды из СМС в массиве команд comand_nume
  int firstIndex = msg.indexOf('#'); 
  int is_phonenumber = 0; //в СМС должен быть номер телефона
  sms_answer = 0 ; // надо ли отвечать на входящую СМС
  IsComment=false;  //признак наличия прикрепленного к номеру комментария  
    for (uint8_t j=0; j < DIGIT_IN_PHONENAMBER; ++j)
      {             SMS_text_num[j] = ' ';
       if (j<5) SMS_text_comment[j] = ' ';       
      }
  // Сначала выясняем команду (первые три символа) для дальнейших действий
  String comment = msg.substring(0,firstIndex);  // команда из СМС  
          #ifndef NOSERIAL 
             Serial.print("*** mess - "); 
             Serial.print(msg);         
             Serial.print(" SMS Comanda - "); 
             Serial.println(comment);
             app->_log->print("*** mess - "); 
             app->_log->print(msg);   
             app->_log->print(" SMS Comanda - ");                         
             app->_log->println(comment);              
          #endif
   //От команды зависит, что делать дальше 
    // - проверять наличие прикрепленного телефона 
    // - отправлять ответную СМС или нет (ели нет то удалить текущую СМС сразу)
      for (int g=0; g < number_comant_type; ++g) {
        if  (comment == FPSTR(comand_nume[g])) { //(comment.compareTo(FPSTR(comand_nume[g])) == 0) { 
         num_text_comanda = g; // номер команды найден в массиве команд
         is_phonenumber = (const int8_t)pgm_read_word(&comand_prop[g][0]); //в СМС должен быть номер телефона
         sms_answer = (const int8_t)pgm_read_word(&comand_prop[g][1]); // надо ли отвечать на входящую СМС
        }
      } 

  String SMSResp_Mess =""; 
   if (num_text_comanda == -1) {//не найден номер команды из СМС в массиве команд comand_nume
       SMSResp_Mess = F("Wrong SMS command");
        // Не верная команда указана в СМС
       //SMSResp_Mess += comment;
       sendSMS(incoming_phone, SMSResp_Mess); 
      return;
     }

  if ((firstIndex == -1 || firstIndex > 3) || (is_phonenumber == 1 && msg.length() < (4+DIGIT_IN_PHONENAMBER))) //неверный формат SMS сообщения
  {
    //String eol_eor = F("\r\n");
    SMSResp_Mess  = F("Wrong SMS format");
    SMSResp_Mess += charCR + charLF;//eol_eor;
    SMSResp_Mess +=F("Must be: COM#PhoneComment"); 
    // SMSResp_Mess += eol_eor;
    // SMSResp_Mess +=F("COM-3 simvols (necessary) command");  
    // SMSResp_Mess += eol_eor;
    // SMSResp_Mess +=F("PHONE- DIGIT_IN_PHONENAMBER digit (necessary)");      
    // SMSResp_Mess += eol_eor;
    // SMSResp_Mess +=F("COMMENT-5 simvols (not necessary)");            

    sendSMS(incoming_phone, SMSResp_Mess);
    return;
  }

  String phoneNUM = msg.substring(firstIndex+1, firstIndex+1+DIGIT_IN_PHONENAMBER); //номер телефона для операций
  
  if (is_phonenumber == 0){// если номер телефона при выполнении СМС команды не требуется - перейти сразу к выполнению команды
    made_action(num_text_comanda, sms_answer);
  }
  else {  // нужен номер телефона (и комментарий к нему) из СМС 

    if (msg.length() > (4+DIGIT_IN_PHONENAMBER))  // если  комментария тоже нет
     {
       comment = msg.substring((5+DIGIT_IN_PHONENAMBER*2));
       if (comment.length() > 6) comment = comment.substring(0, 5);
       IsComment=true; 
     }
    #ifndef NOSERIAL      
      Serial.print("SMS phonenumber "); Serial.println(phoneNUM);
      if (IsComment) // если есть прикрепленный к номеру комментари
      {Serial.print("SMS comment "); Serial.println(comment);
        app->_log->print("SMS comment ");                    
        app->_log->println(comment);          
      }  
      else 
      {
        app->_log->println("NO comment in SMS");             
      }
    #endif

    SMSResp_Mess = F("Phone-");
    SMSResp_Mess += phoneNUM;
    SMSResp_Mess += charCR + charLF;//"\r\n";

  if (phoneNUM.length() > DIGIT_IN_PHONENAMBER){
    SMSResp_Mess += F("Wrong phonenumber: more then ");
    SMSResp_Mess += DIGIT_IN_PHONENAMBER;
    SMSResp_Mess += F(" digits.");
    #ifndef NOSERIAL     
      Serial.println(SMSResp_Mess);
    #endif
    sendSMS(incoming_phone, SMSResp_Mess);
     return;
  }
  if (phoneNUM.length() < DIGIT_IN_PHONENAMBER){
    SMSResp_Mess += F("Wrong phonenumber: less then ");
    SMSResp_Mess += DIGIT_IN_PHONENAMBER;
    SMSResp_Mess += F(" digits.");
    #ifndef NOSERIAL     
      Serial.println(SMSResp_Mess);
    #endif
    sendSMS(incoming_phone, SMSResp_Mess);
     return;
  }  
  for (int8_t z=0; z<phoneNUM.length(); z++){ // проверить что все символы номера это цифры
    if (phoneNUM.charAt(z) <= 47 || phoneNUM.charAt(z) >= 58){
     SMSResp_Mess += F("Wrong phonenumber ");  
     #ifndef NOSERIAL 
      Serial.println(SMSResp_Mess);
     #endif
      sendSMS(incoming_phone, SMSResp_Mess);
    return;
    }
  }
 
// Все проверки пройдены сохранить в глобальные переменные команду, номер и комментарий из СМС
    for (uint8_t j=0; j < DIGIT_IN_PHONENAMBER; ++j)
      {             SMS_text_num[j] = phoneNUM[j] ;   // номер телефона из СМС
       if (j<5 && IsComment) SMS_text_comment[j] = comment[j] ; // комментарий к номеру из СМС      
      } 
 //проверить если номер есть в двоичном массиве номеров
  if (poisk_num(phoneNUM) > -1) {
       made_action(num_text_comanda, sms_answer);
   }    
  else { // если номера нет - поискать в СИМ карте
     SMSResp_Mess = F("+CPBF=\"");
     SMSResp_Mess += phoneNUM;
     SMSResp_Mess += charQuote; 
   #ifndef NOSERIAL 
    Serial.print("madeSMSCommand Comanda: "); Serial.println(SMSResp_Mess); 
    Serial.print("flag_modem_resp = "); Serial.println(String(flag_modem_resp));     
   #endif
    //Найти номер в книге, phonen_index если нет 0 
    app->add_in_queue_comand(30, SMSResp_Mess.c_str(), 2);// установить флаг ослеживания ответа OK для однократного поиска номера "+CPBF:"
  // ответ будет "+CPBF:"  - если номер найден или только OK если не найден.
  // после поиска номера надо передать номер исполняемой команды и надо / не надо отправлять СМС  
   }

  }

}
 
// Функция выполнения команды полученной по СМС
void made_action(int _command, int _answer)
 {  //String _command =String(SMS_text_comanda);
   String temp_respons;
   int16_t bin_num_index = poisk_num(String(SMS_text_num));// проверить наличие такого номера в массиве
  //Выполнить комаду
  if (_command == 0) //F("Add"))  //Добавить новый номер на СИМ карту или в бинарный массив если на сим уже нет места.
    {  
    //  #ifndef NOSERIAL 
    //      Serial.print("app->alloc_num[0] - "); Serial.println(String(app->alloc_num[0])); 
    //      Serial.print("app->alloc_num[1] - "); Serial.println(String(app->alloc_num[1])); 
    //      Serial.print("bin_num_index - "); Serial.println(String(bin_num_index)); 
    //  #endif      
      if (bin_num_index == -1 && (app->alloc_num[1] > app->alloc_num[0])) // если возможных номеров меньше существующих номеров (на сим карте)
        { AddEditNewNumber();
           return;
        }
      else if (bin_num_index == -1 && ((total_bin_num > app->alloc_num[2]) && SMS_phoneBookIndex == 0) ) {//Если в массиве бинарных номеров еще не все элементы заняты
          app->phones_on_sim[app->alloc_num[2]] = stringnum_to_bin(String(SMS_text_num));          
            //++app->alloc_num[2];
            app->_CreateFile(3);
            app-> saveFile(F("/PhoneBook.bin"));
            ReLoadBinMassiv(); // обнулить все номера телефонов в массиве и загрузить из BIN файла все номера 
            temp_respons =  F("New BIN File genereted"); 
      }   
      else if (bin_num_index == -1 && ((app->alloc_num[1] == app->alloc_num[0]) || (total_bin_num == app->alloc_num[2])))
      // Все номера на СИМ карте и в памяти заняты
        temp_respons = F("Memory is FULL ! Delete some numbers before adding NEW.");
      else temp_respons = F("Number allready exists.");
    }  
  else if (_command == 1) //F("Del")) // удалить один номер с СИМ карты
    {
    if (SMS_phoneBookIndex > 0)
      {
      temp_respons = F("+CPBW=");
      temp_respons += String(SMS_phoneBookIndex);
      app->add_in_queue_comand(30, temp_respons.c_str(), 4);//Выставляем флаг для отслеживания OK 
      return;
     }
    else if (bin_num_index != -1) {
      app->phones_on_sim[bin_num_index] = 0; 
      app->_CreateFile(3);
      app-> saveFile(F("/PhoneBook.bin"));
      ReLoadBinMassiv(); // обнулить все номера телефонов в массиве и загрузить из BIN файла все номера 
      temp_respons = F("New BIN File genereted");
     }
    else { 
      temp_respons  = F("Phone-");
      temp_respons += String(SMS_text_num);
      temp_respons += F(" NOT exist in White List!");
      }
  } 
 
  else if (_command == 2) //F("Rds")) { // прочитать список номеров с сим карты и создать файл
   {
     if (app->alloc_num[0] == 0) 
        temp_respons = F("Phone Book is EMPTY. NO File genereted");
     else {
         clear_arrey();
         app->add_in_queue_comand(30,"+CPBF", 5);//Выставляем флаг для отслеживания OK
         app->_CreateFile(1);
         return;
     }
    }
    
  else if (_command == 3) //F("Bin")) // создать бинарный файл из BIN64 массива
    {
     if (app->alloc_num[2] == 0) 
        temp_respons = F("BIN Phone Book is EMPTY. NO File genereted");
     else {
         app->_CreateFile(3);
         app-> saveFile(F("/PhoneBook.bin"));
        temp_respons = F("New BIN File genereted");
      }
    }  
  else if (_command == 4) //F("Rtb")) { //Read text to bin прочитать номера из текстового Nomera2000.txt файла и заполнить BIN64 массив
    {
     app->readTXTCSVfile();  
  // #ifndef NOSERIAL 
  //  for (int16_t n=0; n < app->alloc_num[2]; ++n){
  //       Serial.print(String(n)); Serial.print(" - "); Serial.println(BINnum_to_string(app->phones_on_sim[n])); // для отладки отправляем по UART все что прочитали с карты.
  //  }
  //  #endif      
    temp_respons = F("Allocated in array ");
    temp_respons.reserve(temp_respons.length() + 5);
    temp_respons += String(app->alloc_num[2]);
    temp_respons += F(" numbers from TXT CSV file") ;   
  } 
  else if (_command == 5) { //F("Dan")) //удалить все номера из сим карты
      app->add_in_queue_comand(5,"", 0);
      temp_respons = F("All numbers have been deleted from the SIM card !!");
  }
  else if (_command == 6) //F("Rms")) //Передать СМС с списком мастер номеров
     temp_respons = app ->_whiteListPhones;
  else if (_command == 7) //F("Wms")) //Добавить номер в список мастер номеров
    {
      if (app ->_whiteListPhones.indexOf(String(SMS_text_num)) > -1) {  //если номер уже есть в белом списке - выйти
         temp_respons = F("Number ");
         temp_respons += String(SMS_text_num);
         temp_respons += F(" already exists in WhiteList.");
      }  
      else if (app ->_whiteListPhones.length() >= 2+DIGIT_IN_PHONENAMBER*3) {  //если уже есть 3 номера в белом списке - выйти
        temp_respons = F("WhiteList is FULL.");
      }        
      else if (app ->_whiteListPhones.length() >= DIGIT_IN_PHONENAMBER) { 
        app ->_whiteListPhones += ',' + String(SMS_text_num);
        app -> writeConfig();
        temp_respons =("New WHITE number ");
        temp_respons += String(SMS_text_num);
        temp_respons += F(" Added successfully. New WhiteList: ");
        temp_respons += app ->_whiteListPhones;  
      }   
  } 
  else if (_command == 8) //F("Dms")) //Удалить номер из списка мастер номеров
    {
      if (app ->_whiteListPhones.indexOf(String(SMS_text_num)) == -1) { //если номера нет в белом списке - выйти  
        temp_respons = F("The number ");
        temp_respons += String(SMS_text_num);
        temp_respons += F(" is not included in the white list.");
        sendSMS(String(SMS_incoming_num), temp_respons);      
      return;   
      }      
 // получить короткий номер с которого было послано СМС - последние симолы 
    String short_INnumber =String(SMS_text_num).substring(String(SMS_text_num).length()-DIGIT_IN_PHONENAMBER); 
   #ifndef NOSERIAL 
    Serial.print("short_INnumber: "); Serial.println(short_INnumber);     
   #endif       
      if (String(SMS_incoming_num).indexOf(short_INnumber) > -1) {//если есть попытка удалить свой номер из белого списка - выйти  
       temp_respons = F("It is not possible to delete your own number ");
        temp_respons += String(SMS_text_num);       
       sendSMS(String(SMS_incoming_num), temp_respons);      
       return;   
      }  
      char WtNum[3][DIGIT_IN_PHONENAMBER+1];
      int wn=0; int cwn=0;
      for (int j=0; j<3; ++j) {WtNum[j][0]=NULL; WtNum[j][DIGIT_IN_PHONENAMBER]=NULL;}

      for (int j=0; j<app ->_whiteListPhones.length(); ++j){
        if (cwn == DIGIT_IN_PHONENAMBER) {
          WtNum[wn][cwn]=NULL;
          ++wn; cwn=0;
         }
        else {
          WtNum[wn][cwn] = app ->_whiteListPhones[j]; ++cwn;          
        }
      } 
    //  #ifndef NOSERIAL 
    //      Serial.println("*****************"); 
    //      Serial.println(String(WtNum[0])); Serial.println(String(WtNum[1])); Serial.println(String(WtNum[2]));
    //      Serial.println("*****************");
    //  #endif
      String NewWhiteList="";    
      for (int j=0; j<3; ++j) {  
        if (String(WtNum[j]).indexOf(String(SMS_text_num)) > -1) { //если удаляемый номер
          WtNum[j][0]=NULL;
        }
        if (WtNum[j][0] !=NULL)
          if (j == 0) NewWhiteList += String(WtNum[j]); 
          else {NewWhiteList +=','; NewWhiteList += String(WtNum[j]);}
      }  
  //  #ifndef NOSERIAL       
  //        Serial.println("*** FAZA 2 ***"); 
  //        Serial.println(String(WtNum[0])); Serial.println(String(WtNum[1])); Serial.println(String(WtNum[2]));
  //        Serial.println(NewWhiteList); 
  //        Serial.println("*****************");
  //  #endif
        //whiteListPhones = NewWhiteList;
        app ->_whiteListPhones = NewWhiteList; //whiteListPhones;
        app -> writeConfig();     
        temp_respons = F("New WHITE number ");
        temp_respons += String(SMS_text_num);
        temp_respons += F(" Deleted successfully. New WhiteList: ");
        temp_respons += app ->_whiteListPhones;  
  }               
  else if (_command == 9) //F("Res"))  //Restore сохранить все номера из файла PhoneBookNew.txt в СИМ карту
   { clear_arrey();  // чистим массив номеров и коментариев
      app->readTXTfile();   
      app->add_in_queue_comand(4,"",0);// 4 - скопировать с файла PhoneBookNew.txt все номера на СИМ  
      return;      
  } 
  else if (_command > 9 && _command < 15) // F("R11")) { // Управлять реле через SMS  
    { if (app->relayPin[_command-10] != -1) {
       app->switchRelay(_command-10, true);
       temp_respons = ""; //String(str_relay) + '1' + String(str_ON);
       }
   }
  else if (_command == 15) // F("RA1")) { // Управлять реле через SMS  
     { temp_respons = String(str_relay);
      int8_t h=0;
      for (int8_t i=0; i<maxRelays; ++i){
        if (app->relayPin[i] != -1) {
          app->switchRelay(i, true); 
          if (h==0){
           temp_respons += String(i+1);
           h += 1;
          }
          else
            temp_respons += ", #" + String(i+1);
        }    
      }
          temp_respons += String(str_ON);
   }   
   else if (_command > 15 && _command < 21) { //F("R10")) { // Управлять реле через SMS  
     if (app->relayPin[_command-16] != -1) {
       app->switchRelay(_command-16, false);
       temp_respons = ""; //String(str_relay) + '1' + String(str_OFF);
       }          
   }
   else if (_command == 21) { // F("RA0")) Управлять реле через SMS  
     temp_respons = String(str_relay);   
       int8_t h=0;
      for (int8_t i=0; i<maxRelays; ++i){
        if (app->relayPin[i] != -1) {
          app->switchRelay(i, false); 
         if (h==0){
            temp_respons += String(i+1);
           h += 1;
          }
          else
            temp_respons += ", #" + String(i+1);
        }    
      }
          temp_respons += String(str_OFF);     
   }  
   else if (_command == 22) {//"Cnf" Создать с заменой новый файл Nomera2000.txt из имеющегося в памяти массива 2000 номеров  
       if (app->alloc_num[2]>0) {
         app->_CreateFile(2);
         for (int v = 0; v < app->alloc_num[2]; ++v) {// перебрать все номера телефонов в массиве
           //if (app->phones_on_sim[v] == 0) break;
           app->writeTXTstring(BINnum_to_string(app->phones_on_sim[v]),2);
           vTaskDelay(25);
         }
        temp_respons=F("New file Nomera2000.txt generated.");
       }
       else temp_respons=F("No BIN data! File Nomera2000.txt NOT generated.");
   }

  if (_answer == 1)
    sendSMS(String(SMS_incoming_num), temp_respons); 
  else   
    EraseCurrSMS();// Удалить текущую СМС из памяти модуля

 }

void sendSMS(const String& phone, const String& message){
  //AT+CSCS="GSM"
  String _tempSTR = F("+CMGS=\"");
  _tempSTR += phone;
  _tempSTR += charQuote; 
  _tempSTR += charCR; // F("\r"); //*********!!!!!!!!!!!!!!******************
  _tempSTR += message;
  _tempSTR += (String)((char)26);
   #ifndef NOSERIAL 
    Serial.println("SMS out: " + _tempSTR);
  #endif  
   // 20 - признак отправки СМС 
    app->add_in_queue_comand(20,_tempSTR.c_str(),0);
}

//Добавление (или изменение) номера в справочную книгу
void AddEditNewNumber(){
 const String temp1=String(SMS_phoneBookIndex);
 const String temp2=String(SMS_text_num);
 const String temp3=String(SMS_text_comment);
  String temp_resp="";  
    temp_resp = FPSTR("+CPBW=");
    if (SMS_phoneBookIndex>0)  temp_resp += temp1; // если такой номер уже есть - изменить его, а не добавлять новый
    temp_resp += F(",\""); 
    temp_resp += temp2; 
    temp_resp += F("\",129,\""); 
    temp_resp += temp2;
    if (IsComment) temp_resp += temp3 ; // если есть прикрепленный к номеру комментари
    temp_resp += charQuote; 
  //Выставляем флаг 3 для отслеживания OK 
   app->add_in_queue_comand(30,temp_resp.c_str(), 3);
   app->_log->println(temp_resp); 
}

// процедура выясняет количество имеющихся номеров в книге и общее возможное количество и сохранет их в массив alloc_num[]
void exist_numer(){
  app->add_in_queue_comand(30,"+CPBS?",0);
  return;
}

// Если звонок от БЕЛОГО номера - ответить, включить реле и сбросить вызов
void regular_call()
{   //Команды ответа на вызов и сброса передаются в обратной последовательности
  // т.к. будут добавлены в начало очереди и исполнены "последняя - первой" для ответа на звонок с наивысшим приоритетом
  app->add_in_queue_comand(30,"A", -1) ; // отвечаем на вызов
  app->add_in_queue_comand(30, "H", -1);  // Завершаем вызов  
  app->switchRelay(0, true); // включаем RELAY
}

void clear_arrey(){
   for (int v = 0; v < 250; ++v) // обнулить все номера телефонов в массиве
      {
        app->PhoneOnSIM[v][0] = NULL;
        app->CommentOnSIM[v][14] = NULL;
        app->indexOnSim[v]=0;
      }         
}
// преобразовать двоичный вид номера в строку с цифрами
String BINnum_to_string(uint64_t bin_num){
    String _retnum;
    char digit;
    for(uint8_t h=0; h < DIGIT_IN_PHONENAMBER; ++h){
        digit =0;
        for (int8_t j=0; j<4; ++j)
          bitWrite(digit, 3-j, bitRead(bin_num, 63 - (h*4+j)));
        digit += '0';
        _retnum += digit;
    }
    return _retnum;
}
// преобразовать строку с номером в двоичный код
int64_t stringnum_to_bin(const String& string_num){
  int64_t _retbin=0;
  for (int8_t g=0; g < string_num.length(); ++g)
  {
    _retbin |= uint64_t(string_num[g] - '0') << (60-g*4); 
  }
  return _retbin;
}
//поиск номера в массиве двоичных номеров
int16_t poisk_num(const String& txt_num){
  int16_t _ret=-1;
  uint64_t found_num = stringnum_to_bin(txt_num);
  for (int v = 0; v < total_bin_num; ++v)
  { if (app->phones_on_sim[v] != 0)
       if (app->phones_on_sim[v] == found_num)
            _ret=v;
  }
   #ifndef NOSERIAL 
    Serial.print("BIN num "); Serial.print(txt_num); 
    Serial.print(" is: "); Serial.println(String(_ret));
  #endif    
  return _ret;
}

void retGetZapros(){
      app->add_in_queue_comand(30,"+HTTPTERM",-1); // Закрыть текущий запрос
    // GPRS_ready = false;
     #ifndef NOSERIAL          
           Serial.print("GPRS_ready = false; "); 
     #endif 
}

void retTCPconnect(){
      app->add_in_queue_comand(30,"+CIPCLOSE",-1); // Закрыть текущий запрос
      app->TCP_ready = false; app->MQTT_connect=false; 
     #ifndef NOSERIAL          
           Serial.print("TCP_ready = false; "); 
     #endif 
}

/*
Первоначальный вход в WEB интерфейс :

Подключиться к WiFi точке доступа с именем ESP_GSMRelayXXXXXX
с паролем P@$$w0rd

В названии точки доступа последние 6 символов - это последние 6 символов MAC адреса
Также эти 6 симолов использутся в адресе для доступа по локальной сети, если устройство 
настроено как станция (а не как точка доступа).
Т.е. если выбран режим работы Infrastructur (а не AP) задана WiFi сеть к которой надо подключиться
и пароль для входа в нее.
В броузере набрать адрес например - Relay_77E2C4.local и откроется WEB интерфейс для настройки 
параметров устройства.

1. подготовить текстовые файлы с номерами

PhoneBookNew.txt

Файл для максимум 250 номеров.
Эти номера можно сохранить на СИМ карту (предварительно ее очистив SMS командой Dan# расшифровывается как Delete All Numbers)
Формат записи строки для одного номера
порядковый номер  ### ;  DIGIT_IN_PHONENAMBER (8 ) цифр номера ; те же 8 цифр номера и 6 символов комментария (только латинские буквы!!)

пример:
1;12345678;12345678kommt
2;98765432;98765432tmmok

Nomera2000.txt

Файл для максимум 2000 номеров
в каждой строке только номер из DIGIT_IN_PHONENAMBER (= 8) цифр

пример:
987650000
987651000
987652000
987655000
........
987656000
987657000
987651000
987651100

2. Загрузить эти два файла в память ESP32_GSMRelay через WEB интерфейс 
   Пароль точки доступа по умолчанию P@$$w0rd (можно изменить через WEB интерфейс)
    IP устройства и /spiffs
    пример: 192.168.4.1/spiffs

3. сохранить 250 номеров на СИМ карту (предварительно удалить все номера с СИМ карты командой Dan#)
   1. Отправить СМС команду Res# с одного из трех мастер номеров
   2. В последующем при изменении, добавлении или удалении номеров из СИМ карты
      создавать актуальный файл PhoneBook.txt СМС командой Rds# 
      Также этот файл можно использовать для клонирования содержания СИМ карт разных устройст.

4. Сохранение 2000 номеров в памяти для последующего использования
   1. Передать СМС команду Rtb# для создания временного массива номеров из файла Nomera2000.txt
   2. Передать СМС команду Bin# для сохранения этого массива номеров в файл PhoneBook.bin в памяти устройства.


                           СПИСОК СМС команд

 "Add",  //Добавить новый номер на СИМ карту или в бинарный массив если на сим уже нет места.
 "Del", // удалить один номер с СИМ карты
 "Rds", // прочитать список номеров с сим карты и создать файл PhoneBook.txt
 "Bin", // создать бинарный файл PhoneBook.bin из BIN64 массива в памяти устройства
 "Rtb", //Read text to bin прочитать номера из файла Nomera2000.txt и заполнить BIN64 массив в памяти устройства
 "Dan", //Delete All Numbers удалить все номера из СИМ карты
 "Rms", //Передать СМС со списком мастер номеров
 "Wms", //Добавить номер в список мастер номеров
 "Dms", //Удалить номер из списка мастер номеров
 "Res", //Restore сохранить все номера из файла PhoneBookNew.txt в СИМ карту
 "R11", "R21", "R31", "R41", "R51","RA1", // включить одно реле или все реле сразу - RA1
 "R10", "R20", "R30", "R40", "R50","RA0"  // выключить одно реле или все реле сразу - RA0 
 */