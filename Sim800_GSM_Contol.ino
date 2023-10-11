//#include <Arduino.h>
#define MODEM_RST             5
#define MODEM_PWRKEY          4
#define MODEM_POWER_ON       23
#define MODEM_TX             27
#define MODEM_RX             26

// Initialize the indicator as an output
//#define LED_GPIO             13
// #define LED_ON               HIGH
// #define LED_OFF              LOW

// #define DIGIT_IN_PHONENAMBER 9

const char str_relay[] PROGMEM = "Relay #";
const char str_ON[] PROGMEM = " is ON";
const char str_OFF[] PROGMEM = " is OFF";

// char PhoneOnSIM[250][DIGIT_IN_PHONENAMBER];
// char CommentOnSIM[250][15];

// Set serial for debug console (to the Serial Monitor, default speed 115200)
//#define SerialMon Serial
// Set serial for AT commands (to the module)
//#define SIM800  Serial1

String _response = "";              // Переменная для хранения ответа модуля
String whiteListPhones ; //= "69202891"; // Белый список телефонов максимум 3 номера по 8 симолов

uint8_t command_type =0; //тип отправленной в модем команды 
                         // 1 - считать весь список телефонов с СИМ, создать файл PhoneBook.txt с текстом номеров
                         // 2 - создать массив из бинарных значений номеров на СИМ карте
                         // 3 - создать бинарный файл номеров PhoneBook.bin
                         // 4 - скопировать с файла PhoneBookNew.txt все номера на СИМ
                         // 5 - удалить все номера из сим карты
                         // 6 - reset sim800
                         // 7 - ответить на вызов "ATA"
                         // 8 - завершить вызов "ATH"
                         // 9 - удалить все SMS
                         // 11 - тестовый опрос модема раз в 5 минут

//uint8_t alloc_num[2]={0,0}; //Количество имеющихся в телефонной книге номеров и общее возможное количество номеров

unsigned long t_rst = 0; //120*1000; // отследить интервал для перезапуска модема

bool modemOK = false; 
int8_t _step = 0; //текущий шаг в процедуре GPRS_traffic -глобальная /признак, что процедура занята
bool PIN_ready = false;
bool CALL_ready = false;
bool comand_OK = false; // признак успешного выполнения текущей команды
TaskHandle_t Task3; // Задача для ядра 0

//Переменные для работы с SMS
char SMS_incoming_num[DIGIT_IN_PHONENAMBER+7]; // номер с которого пришло СМС - для ответной СМС
char SMS_text_num[DIGIT_IN_PHONENAMBER+1];  // номер телефона из СМС
char SMS_text_comment[5+1]; // комментарий к номеру из СМС
char SMS_text_comanda[3+1]; // команда из СМС
int SMS_phoneBookIndex=0; // если номер уже есть в симке - его индекс, нет - ноль
bool IsComment=false;  //признак наличия прикрепленного к номеру комментария

unsigned long t_last_command = 0;  // последняя команда от модема для отслеживания ОК
uint8_t flag_modem_resp = 0; // Признак сообщения полученного от модема (если необходимо обработать следующую строку с ОК)
                            // 1 - +CMGS: попытка отправить сообщение OK или ERROR
                            // 2 - +CPBF: попытка найти одиночный номер на симке
                            // 3 - +CPBW: попытка добавить / редактировать одиночный номер на симке
                            // 4 - +CPBW: завершено одиночное удаление номера из СМС - отправить ответ
                            // 5 - +CPBF: просмотр всех номеров из СМС - создание текстового файла
/*
void setupModem()
{ // Физическое отключение питания модема - пины ESP32
#ifdef MODEM_RST
    // Keep reset high
    pinMode(MODEM_RST, OUTPUT);
    digitalWrite(MODEM_RST, HIGH);
#endif

    pinMode(MODEM_PWRKEY, OUTPUT);
    pinMode(MODEM_POWER_ON, OUTPUT);

    // Turn on the Modem power first
    digitalWrite(MODEM_POWER_ON, HIGH);

    // Pull down PWRKEY for more than 1 second according to manual requirements
    digitalWrite(MODEM_PWRKEY, HIGH);
    vTaskDelay(100);
    digitalWrite(MODEM_PWRKEY, LOW);
    vTaskDelay(1000);
    digitalWrite(MODEM_PWRKEY, HIGH);

    // Initialize the indicator as an output
    // pinMode(LED_GPIO, OUTPUT);
    // digitalWrite(LED_GPIO, LED_OFF);

    t_rst = millis();
}
*/

void GPRS_modem_traffic( void * pvParameters ){
  int _num_index = 0; //счетчик номеров из телефонной книги при записи номеров из массива на СИМ
  bool _AT_ret =false; // возврат от сегмента sendATCommand
  String _comm =String(); //исполняемая команда без AT
  int _povtor = 0; //возможное количество повторов текущей команды
  uint8_t g = 0; // счетчик повторов отправки команды в модем
  unsigned long _timeout = 0;        // Переменная для отслеживания таймаута (10 секунд)  

  for (;;){

  if (!_AT_ret && _step !=0)   // если предидущая команда неудачно прекратить попытки  
      { _step=14; modemOK = false; }

  if (command_type == 6 || command_type == 16) { //6 Reset SIM800 16 init без рестарта
  switch (_step) {
    case 0:
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

    // Initialize the indicator as an output
    // pinMode(LED_GPIO, OUTPUT);
    // digitalWrite(LED_GPIO, LED_OFF);
     vTaskDelay(2000);
    t_rst = millis();    
    //   goto EndATCommand; ++_step;
    //   break;    
    // case 0:    
      PIN_ready = false;
      CALL_ready = false;
      //GPRS_ready = false; // признак подключения GPRS
      //GET_GPRS_OK = false; // признак удачного HTTP GET запроса
      modemOK = false; 
      comand_OK = false;    
      _comm=""; _povtor = 1; // Автонастройка скорости
      goto sendATCommand;
      break;
    case 1:
      _comm=F("&W"); _povtor = 1;
      goto sendATCommand;
      break;  
    case 2:
      _comm=F("+CIURC=1"); _povtor = 2;// включить отображение состояния
      goto sendATCommand;
      break;  
    case 3:
      _comm=F("+CFUN=0"); _povtor = 2; //Set Phone Functionality - Minimum functionality
      goto sendATCommand;
      break;   
    case 4:
      _comm=F("+CFUN=1,1"); _povtor = 2; //Reset the MT before setting it to <fun> power level.
      goto sendATCommand;
      break;  
    case 5:
      _comm=F("+CFUN=1"); _povtor = 2;  // Full functionality (Default)
      goto sendATCommand;
      break;   
    case 6:
      _comm=F("E0"); _povtor = 1;       //E0 отлючаем Echo Mode  
      goto sendATCommand;
      break;      
    case 7:  
      _comm=F("+CMGF=1"); _povtor = 1;  // Включить TextMode для SMS
      goto sendATCommand;
      break;       
    case 8:
      _comm=F("+CPIN?;+CCALR?"); _povtor = 1;// запрос на готовность симки (отсутствие PIN) и готовность звонков
      goto sendATCommand;
      break;      
    case 9:
      _comm=F("+CPBS=\"SM\""); _povtor = 2;// указать место хранения номеров - SIM
      goto sendATCommand;
      break;   
    case 10: 
      _comm=F("+CLIP=1;"); _povtor = 2;// Включаем АОН
      goto sendATCommand;
      break;    
   
//  sendSMS("+37369202891", "test message");
// Отключить вывод текущей временной зоны при каждом входящем звонке
// AT+CLTS=0
    case 11:
      _comm=F("+CLTS=0;+CCALR?"); _povtor = 2;
      goto sendATCommand;
      break;   
   
// Отключить вывод дополнительной информации при каждом входящем звонке
// AT+CUSD=0
    case 12:
      _comm=F("+CUSD=0;+CCALR?;+CPBS?"); _povtor = 2; // выяснить количество номеров на СИМ
      goto sendATCommand;
      break;  
   
    case 13:
  _timeout = millis() + 35000;             // Переменная для отслеживания таймаута (35 секунд)
  while (!PIN_ready && !CALL_ready && millis() < _timeout)  {vTaskDelay(5);}; // Ждем ответа 35 секунд, если пришел ответ или наступил таймаут, то...   
      if (PIN_ready && CALL_ready){
         modemOK=true; 
         #ifndef NOSERIAL   
           Serial.println("MODEM OK");               // ... оповещаем об этом и...
         #endif    
       }
      else
      {                                       // Если пришел таймаут, то...
        #ifndef NOSERIAL   
          Serial.println("modem Timeout...");               // ... оповещаем об этом и...
        #endif  
       }
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
               _comm += ",\""; 
             for (uint8_t g=0; g<DIGIT_IN_PHONENAMBER; ++g) _comm += app->PhoneOnSIM[_num_index][g]; 
               _comm += "\",129,\""; 
               _comm += String(app->CommentOnSIM[_num_index]);
               _comm +="\"";
        _povtor = 0;
        ++_num_index;        
        goto sendATCommand;
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
      goto sendATCommand;
      }
      else _step = 14; // создать условие для выхода из команды      
   }

 else if (command_type == 9){ // удалить все SMS
     if (_step == 0){ _step = 13; // создать условие для одноразового прохода
       _comm=F("+CMGDA=\"DEL ALL\""); _povtor = 2;
       goto sendATCommand;
     }
   }

 else if (command_type == 11){ // Тестовая команда, раз в 5 минут
     if (_step == 0){ 
       _comm=""; _povtor = 0; t_rst=millis();
       goto sendATCommand;        
     }
     else if(_step == 1) {
       PIN_ready = false; command_type = 6; CALL_ready = false;// переход к команде сброса, шаг 13 - ожидание ответа PIN READY
       _step = 12; modemOK=false; // создать условие для одноразового прохода
      _comm=F("+CCALR?;+CPIN?"); _povtor = 1;// запрос на готовность симки (отсутствие PIN) и готовность сети
      goto sendATCommand;
     }
   }

  else if (command_type != 0)  // если тип команды задан, но не обработан - сбросить все значения
   {_step = 0; command_type = 0; _comm="";}

 if (_step > 13) {_step = 0; command_type = 0; _comm="";} 

sendATCommand:

  if (command_type != 0) {
   _AT_ret=false;
   _comm = "AT" + _comm; 
  #ifndef NOSERIAL
   //if (_comm.indexOf("+HTTPINIT") > -1 || _comm.indexOf("+HTTPTERM") > -1)
   // {
   Serial.print("Command ");  Serial.println(_comm);   // Дублируем команду в монитор порта
   // }
  #endif
   g=0;
  do {
  comand_OK = false;
  SIM800.println(_comm);       // Отправляем команду модулю
  unsigned long _timeout = 0;        // Переменная для отслеживания таймаута (5 секунд)  
    _timeout = millis() + 5000;             // Переменная для отслеживания таймаута (5 секунд)
    while (!comand_OK && millis() < _timeout)  // Ждем ответа 5 секунд, если пришел ответ или наступил таймаут, то...  
               vTaskDelay(100);
      
        if (comand_OK){
          // #ifndef NOSERIAL  
          //   Serial.print(_comm);               // ... оповещаем об этом и... 
          //   Serial.println(" OK");               // ... оповещаем об этом и...
          // #endif    
          _AT_ret=true;
        }
       else
        {                                       // Если пришел таймаут, то...
          _AT_ret = false;        
          #ifndef NOSERIAL   
          Serial.println("AT Timeout...");               // ... оповещаем об этом и...
          Serial.print("_comm="); Serial.print(_comm);
          Serial.print(" command_type="); Serial.print(command_type);
          Serial.print(" _step="); Serial.println(_step);
          #endif  
         }      

        if (g > _povtor) break; //  return modemStatus;}
     ++g;
   } while ( !comand_OK );   // Не пускать дальше, пока модем не вернет ОК  
    comand_OK=false; // снять признак выполнения команды
    ++_step; //увеличить шаг на 1 для перехода к следующей команде
  } // end ATCommand

  EndATCommand:
    vTaskDelay(3);
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


 SMS_incoming_num[DIGIT_IN_PHONENAMBER]=NULL; // номер с которого пришло СМС - для ответной СМС
 SMS_text_num[DIGIT_IN_PHONENAMBER]=NULL;  // номер телефона из СМС
 SMS_text_comment[5]=NULL; // комментарий к номеру из СМС
 SMS_text_comanda[3]=NULL; // команда из СМС
    // Set GSM module baud rate and UART pins
    SIM800.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);

    //setupModem(); // Физическое отключение питания модема - пины ESP32
  whiteListPhones = app->_whiteListPhones; // скопировать список белых номеров сохраненных в EEPROM
  for (int v = 0; v < total_bin_num; ++v) // обнулить все номера телефонов в массиве
    app->phones_on_sim[v] = 0;

   app->readBINfile();

 for (int v = 0; v < 250; ++v) // обнулить все номера телефонов в массиве
    {
     app->PhoneOnSIM[v][0] = NULL;
     app->CommentOnSIM[v][14] = NULL;
     app->indexOnSim[v]=0;
    }

// Создаем задачу с кодом из функции Task1code(),
  // с приоритетом 1 и выполняемую на ядре 0:
  xTaskCreatePinnedToCore(
                    GPRS_modem_traffic,   /* Функция задачи */
                    "Task3",     /* Название задачи */
                    10000,       /* Размер стека задачи */
                    NULL,        /* Параметр задачи */
                    1,           /* Приоритет задачи */
                    &Task3,      /* Идентификатор задачи, чтобы ее можно было отслеживать */
                    1);          /* Ядро для выполнения задачи (0) */

  vTaskDelay(30);
  _step = 0;
  // Физическое отключение питания модема - пины ESP32
  command_type = 6; // стартовые настройки модема

  /*
  command_type = 2; // установить тип команды для модема
  sendATCommand(F("AT+CPBF"),true); //Сохраняем в массив номера телефонов с СИМ в двоичном виде
  Serial.println("Массив номеров BIN");  
  for (uint8_t v = 0; v < 250; ++v){
    if (app->phones_on_sim[v] > 0) {
      //Serial.println(app->phones_on_sim[v], BIN);
      Serial.println(BINnum_to_string(app->phones_on_sim[v]));      
    } 
  }
  
  command_type = 0;
  */

}

/*
String sendATCommand(String cmd, bool waiting) {
  String _resp = "";                            // Переменная для хранения результата
  Serial.print("Command ");  Serial.println(cmd);   // Дублируем команду в монитор порта
  SIM800.println(cmd);                          // Отправляем команду модулю
  if (waiting) {                                // Если необходимо дождаться ответа...
    _resp = app->waitResponse(command_type);                     // ... ждем, когда будет передан ответ
    // Если Echo Mode выключен (ATE0), то эти 3 строки можно закомментировать
//    if (_resp.startsWith(cmd)) {  // Убираем из ответа дублирующуюся команду
//      _resp = _resp.substring(_resp.indexOf("\r", cmd.length()) + 2);
//    }
    Serial.print("Respons ");
    Serial.print(_resp);                      // Дублируем ответ в монитор порта
    Serial.println(' ');
  }
  return _resp;                                 // Возвращаем результат. Пусто, если проблема
}
*/

void Sim800_loop() {
// сбрасывать модуль через интервал - раз в 30 часов
if (millis() > 60*60*1000*30)   ESP.restart();

// Опросить модем раз в указанный интервал
  if (millis() - t_rst > 15*60*1000 && modemOK ) 
 { 
   if (_step == 0 && command_type == 0 && flag_modem_resp==0)
    {command_type = 11;
      #ifndef NOSERIAL      
        Serial.println("Opros Modem"); 
      #endif   
   }   
 }

 // Если есть проблемы с модемом попытаться сбросить модем
 if (!modemOK && millis() - t_rst > 18*60*1000) 
   {    
     if (_step == 0 && command_type == 0)
      {command_type = 6; 
      #ifndef NOSERIAL      
        Serial.println("Restart Modem"); 
      #endif    
      }
   }

if (SIM800.available())   {                   // Если модем, что-то отправил...
    //_response = app->waitResponse(0);                 // Получаем ответ от модема для анализа
    _response = SIM800.readStringUntil('\n');             // Получаем ответ от модема для анализа
    //_response = SIM800.readString();             // Получаем ответ от модема для анализа    
      #ifndef NOSERIAL      
        Serial.println(_response);                  // Если нужно выводим в монитор порта
     //   Serial.write(_response, DEC);                  // Если нужно выводим в монитор порта        
      #endif 
    int firstIndex = 0;
    String textnumber = "";                    // переменая с текстовым значением номера из телеф. книги
    String textnumbercomment = "";            // переменая с текстовым значением коментария из телеф. книги  (не больше 6 символов)
    // ... здесь можно анализировать данные полученные от GSM-модуля
    //********** Прием звонка от избранного номера
    if (_response.indexOf(F("+CPIN: READY")) > -1) PIN_ready = true;
    else if (_response.indexOf("+CCALR: 1") > -1) CALL_ready = true;
    else if (_response.indexOf("+CCALR: 0") > -1) CALL_ready = false;
    else if (_response.indexOf("+CLIP:") > -1) { // Есть входящий вызов  +CLIP: "069202891",129,"",0,"069202891asdmm",0   
    //else if (_response.indexOf("RING") > -1) { // Есть входящий вызов    
      #ifndef NOSERIAL        
        Serial.println("Incoming CALL");
      #endif
      int phoneindex = _response.indexOf("+CLIP: \"");// Есть ли информация об определении номера, если да, то phoneindex>-1
      String innerPhone = "";                   // Переменная для хранения определенного номера
      if (phoneindex >= 0) {                    // Если информация была найдена
        phoneindex += 8;                        // Парсим строку и ...
         innerPhone = _response.substring(_response.indexOf("\"", phoneindex)-9, _response.indexOf("\"", phoneindex)); //innerPhone = _response.substring(phoneindex, _response.indexOf("\"", phoneindex)); // ...получаем номер
      #ifndef NOSERIAL          
        Serial.print("Number: " + innerPhone); // Выводим номер в монитор порта
        Serial.println(" BIN #: " + String(poisk_num(innerPhone))); // Выводим номер в монитор порта        
      #endif  
        //поиск текстового поля в ответе +CLIP: "069071267",129,"",0,"",0
        int last_comma_index = _response.lastIndexOf(',');
        int fist_comma_index = String(_response.substring(0,last_comma_index-1)).lastIndexOf(',');
        if ((last_comma_index-fist_comma_index) >= DIGIT_IN_PHONENAMBER)
          textnumber=_response.substring(fist_comma_index+2, fist_comma_index+2+DIGIT_IN_PHONENAMBER); 
        else textnumber="";

        if (_response.length() > fist_comma_index+2+DIGIT_IN_PHONENAMBER) //если в текстовом поле еще есть коментарий
        { 
          textnumbercomment=_response.substring(fist_comma_index+2+DIGIT_IN_PHONENAMBER, _response.length()-4);
         #ifndef NOSERIAL            
          Serial.println("TextNumberComment: " + textnumbercomment);
         #endif 
        }
        #ifndef NOSERIAL  
          Serial.println("TextNumber: " + textnumber);
        #endif  
      }
      // Проверяем, чтобы длина номера была больше 6 цифр, и номер должен быть в списке
      if (innerPhone.length() > 6 && whiteListPhones.indexOf(innerPhone) > -1) 
         regular_call(); // Если звонок от БЕЛОГО номера из EEPROM - ответить, включить реле и сбросить вызов
      else if (innerPhone == textnumber && textnumber.length() == DIGIT_IN_PHONENAMBER)
        regular_call(); // Если звонок от БЕЛОГО номера из СИМ карты - ответить, включить реле и сбросить вызов  
      else if (poisk_num(innerPhone)>-1)
        regular_call(); // Если звонок от БЕЛОГО номера из BIN массива - ответить, включить реле и сбросить вызов        
      else  SIM800.println("ATH"); // Если нет, то отклоняем вызов

    }
    //********* провера отправки SMS ***********
    else if (_response.indexOf("+CMGS:") > -1) {       // Пришло сообщение об отправке SMS
      flag_modem_resp = 1;
      //t_last_command = millis();  
      #ifndef NOSERIAL        
        Serial.println("Sending SMS");
      #endif
      // int index = _response.lastIndexOf("\r\n");// Находим последний перенос строки, перед статусом
      // String result = _response.substring(index + 2, _response.length()); // Получаем статус
      // result.trim();                            // Убираем пробельные символы в начале/конце
    }
    //********** проверка приема SMS ***********
    else if (_response.indexOf("+CMTI:") > -1) {       // Пришло сообщение о приеме SMS
      #ifndef NOSERIAL       
        Serial.println("Incoming SMS");
      #endif  
      int index = _response.lastIndexOf(',');   // Находим последнюю запятую, перед индексом
      String result = _response.substring(index + 1, _response.length()); // Получаем индекс
      result.trim();                            // Убираем пробельные символы в начале/конце
      #ifndef NOSERIAL        
        Serial.print("new mess "); Serial.println(result);
      #endif
      SIM800.println("AT+CMGR=" + result);// Получить содержимое SMS
    }
    else if (_response.indexOf("+CMGR:") > -1) {    // Пришел текст SMS сообщения 
      // #ifndef NOSERIAL  
      //   Serial.println("***********************************************************");           
      //   Serial.print("+CMGR ??? - ");
      //   Serial.println(_response);
      //   Serial.println("***********************************************************");         
      // #endif      
      _response =_response + '\r' + SIM800.readString();
      parseSMS(_response);        // Распарсить SMS на элементы
    }
    else if (_response.indexOf("+CPBS:") > -1){ // выяснить количество занятых номеров на СИМ и общее возможное количество
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
    else if (_response.indexOf("+CPBF:") > -1) { // +CPBF: 4,"078091083",129,"078091083Manip"
      String phonen_index; 
        firstIndex = _response.indexOf(',');
        phonen_index = _response.substring(7, firstIndex); 
        textnumber =  _response.substring(firstIndex+2, firstIndex+2+DIGIT_IN_PHONENAMBER);
        textnumbercomment =_response.substring(_response.lastIndexOf(',')+2, _response.lastIndexOf('\"'));
           #ifndef NOSERIAL     
             Serial.print("File String +CPBF: index= " + phonen_index); 
             Serial.print(" ; number= " + textnumber);                          
             Serial.println(" ; comment= " + textnumbercomment); 
            // Serial.print("csv text= " + _response);              
           #endif 
      if (flag_modem_resp == 2)  // одиночный поиск номера из СМС 
       {
        SMS_phoneBookIndex = phonen_index.toInt();
           #ifndef NOSERIAL     
            // Serial.println("+CPBF: index=" + phonen_index); 
             Serial.println("+CPBF: INT index=" + String(SMS_phoneBookIndex));              
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
          _response +="\r\n"; 
       //Заполнить массив номерами и коментариями для записи в текстовый файл
        for (uint8_t m=0; m<DIGIT_IN_PHONENAMBER; ++m) app->PhoneOnSIM[phonen_index.toInt()][m] = textnumber[m];   
        for (uint8_t m=0; m<15; ++m)    //textnumbercomment.length()        
            { if (textnumbercomment.length() < m)
                {app->CommentOnSIM[phonen_index.toInt()][m] = NULL; 
                break;
                }
              else                
                app->CommentOnSIM[phonen_index.toInt()][m] = textnumbercomment[m];                   
            }     

           #ifndef NOSERIAL     
             Serial.println("numer text= " + _response);              
           #endif                                  
       }    
      // else 
      //  {
      //      #ifndef NOSERIAL     
      //        Serial.println("String +CPBF: index=" + phonen_index);             
      //      #endif  
      //  }  
    }    
    if (_response.indexOf(F("OK")) > -1) {
      comand_OK = true;
      if (flag_modem_resp==1){
        #ifndef NOSERIAL        
          Serial.println ("Message was sent. OK");
        #endif
        command_type = 9; // удалить все SMS, чтобы не забивали память модуля  
        flag_modem_resp=0;        
      }
      else if (flag_modem_resp==2 && millis() > t_last_command) // завершен одиночный поиск номера из СМС - приступить к выполнению команды
        {
           #ifndef NOSERIAL     
             Serial.println("Global phonenumber index=" + String(SMS_phoneBookIndex)); //.toInt()
           #endif        
          flag_modem_resp=0; 
          made_action();       
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
                   _response +="\r\n";  
                   #ifndef NOSERIAL     
                     Serial.println("csv text= " + _response);              
                    #endif  
                   if (app-> writeTXTstring(_response))  // записать строку с номером в текстовый файл
                     { ++count_row;
                       #ifndef NOSERIAL     
                        Serial.println("String append OK");              
                       #endif    
                     }
                    else
                     {
                      #ifndef NOSERIAL     
                        Serial.println("String append ERROR");              
                       #endif    
                    }                                                                                                                             
                 } 
             } 
          flag_modem_resp=0;                    
           if (count_row>0)  
           {                    
           #ifndef NOSERIAL     
             Serial.println("TXT create File"); 
           #endif 
           }
           else
           {
           #ifndef NOSERIAL     
             Serial.println("NO TXT File"); 
           #endif 
           }       
        }        
      else if (flag_modem_resp==3 && millis() > t_last_command) // завершено одиночное добавление / редактирование номера из СМС - отправить ответ
      { String SMSResp_Mess;
         exist_numer(); // обновить количества использованных и доступных номеров на СИМ
         if (SMS_phoneBookIndex==0) { //Добавить новый номер
          SMSResp_Mess  =F("Phone-");
          SMSResp_Mess += String(SMS_text_num);
          SMSResp_Mess += F(" was successfully INCLUDED in White List!");
          // else if (temp_respons.indexOf("ERROR")>-1)
          // SMSResp_Mess += F(" NOT INCLUDED in White List! ERROR"); 
           }
         else if (SMS_phoneBookIndex > 0) { // перезаписать существующий номер   
          SMSResp_Mess  =F("Phone-");
          SMSResp_Mess +=String(SMS_text_num); 
          SMSResp_Mess += F(" already exist in White List! "); 
          SMSResp_Mess += "\r\n";
          SMSResp_Mess +=F("Index -"); 
          SMSResp_Mess +=String(SMS_phoneBookIndex); 
    // if (temp_respons.indexOf("ERROR")>-1) {      
    //   SMSResp_Mess += "\r\n"; 
    //   SMSResp_Mess +=F("Editing ERROR");
    //   }
    //   else {
          SMSResp_Mess += "\r\n"; 
          SMSResp_Mess +=F("Editing OK");
    //  }
         }
        flag_modem_resp=0;
        sendSMS(String(SMS_incoming_num), SMSResp_Mess);    // отправить СМС с ответом
      } 
      else if (flag_modem_resp==4 && millis() > t_last_command) // завершено одиночное удаление номера из СМС - отправить ответ
      { String SMSResp_Mess;
        exist_numer(); // обновить количества использованных и доступных номеров на СИМ      
       SMSResp_Mess  =F("Phone-");
       SMSResp_Mess += String(SMS_text_num);
       if (SMS_phoneBookIndex > 0)
         SMSResp_Mess += F(" was successfully REMOVED in White List!");
       else
         SMSResp_Mess += F(" ERROR!! with REMOVED in White List!");   
        sendSMS(String(SMS_incoming_num), SMSResp_Mess); 
      }
     }
     if (_response.indexOf(F("ERROR")) > -1) {
      if (flag_modem_resp == 1){
        #ifndef NOSERIAL        
         Serial.println ("Message was not sent. Error");
        #endif
        flag_modem_resp=0;  
      }
      else if (flag_modem_resp==2) // завершен одиночный поиск номера из СМС - приступить к выполнению команды
        flag_modem_resp=0;   

     }
    //*************************************************
  }

  #ifndef NOSERIAL   
    if (Serial.available())                      // Ожидаем команды по Serial...
       SIM800.write(Serial.read());                // ...и отправляем полученную команду модему
  #endif
}

void parseSMS(String msg) {                                   // Парсим SMS
  String msgheader  = "";
  String msgbody    = "";
  String msgphone   = "";

  msg = msg.substring(msg.indexOf("+CMGR: "));
  msgheader = msg.substring(0, msg.indexOf('\r'));            // Выдергиваем телефон

  msgbody = msg.substring(msgheader.length() + 2);

  msgbody = msgbody.substring(0, msgbody.lastIndexOf("OK"));  // Выдергиваем текст SMS
  msgbody.trim();

  int firstIndex = msgheader.indexOf("\",\"") + 3;
  int secondIndex = msgheader.indexOf("\",\"", firstIndex);

  //firstIndex = secondIndex - 8;
  msgphone = msgheader.substring(firstIndex, secondIndex);
  // Записать номер с которого пришло СМС в глобальную переменную для общего доступа
  for (uint8_t j=0; j < msgphone.length()+1; ++j){
      if (j == msgphone.length())
        SMS_incoming_num[j] = NULL;  // если последний символ - добавить нулл в массив для финализации строки
      else  SMS_incoming_num[j] = msgphone[j];
  }
 // получить короткий номер с которого было послано СМС - последние симолы 
  String short_INnumber =String(SMS_incoming_num).substring(String(SMS_incoming_num).length()-(DIGIT_IN_PHONENAMBER-1));
   #ifndef NOSERIAL 
    Serial.println("Phone: " + msgphone);                       // Выводим номер телефона
    Serial.println("Message: " + msgbody);                      // Выводим текст SMS
    Serial.println("SMS_incoming_num : " + String(SMS_incoming_num)); //.c_str());  // Выводим текст SMS    
    Serial.println("short_INnumber: " + short_INnumber);     
  #endif
// Далее пишем логику обработки SMS-команд.
  // Здесь также можно реализовывать проверку по номеру телефона
  // И если номер некорректный, то просто удалить сообщение.
  
 // if (msgphone.length() > 6 && whiteListPhones.indexOf(msgphone) > -1) { // Если телефон в белом списке, то...
  if (String(SMS_incoming_num).length() > 6 && whiteListPhones.indexOf(short_INnumber) > -1) {
    #ifndef NOSERIAL 
     Serial.println("Comand from WHITE phonenumber");                          // ...выполняем команду
    #endif
      msgbody = probel_remove(msgbody);
      madeSMSCommand(msgbody, msgphone);
     }
   #ifndef NOSERIAL     
  else {
    Serial.println("Unknown phonenumber");
    command_type = 9; // удалить все SMS, чтобы не забивали память модуля      
    }
   #endif 
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
  if (num_phone.length() > 6 && whiteListPhones.indexOf(num_phone) > -1)  // Если телефон в белом списке
     temp_ret = true;

  return temp_ret;
}

//выяснение полученой по SMS команды
void madeSMSCommand(const String& msg, const String& incoming_phone){
  // очистить все переменные
  SMS_phoneBookIndex=0; // сбросить индекс искомого номера из СМС
  IsComment=false;  //признак наличия прикрепленного к номеру комментария  
    for (uint8_t j=0; j < DIGIT_IN_PHONENAMBER; ++j)
      {             SMS_text_num[j] = ' ';
             //   SMS_incoming_num[j] = ' ';
       if (j<3) SMS_text_comanda[j] = ' ';
       if (j<5) SMS_text_comment[j] = ' ';       
      }

  int firstIndex = msg.indexOf('#'); 
 
  String SMSResp_Mess =""; 
  if (firstIndex == -1 || firstIndex > 3 || msg.length() < 13) //неверный формат SMS сообщения
  {// SMSResp_Mess="Wrong SMS format '" + msg + "' " + "\r\n" + "Must be: COM#PHONECOMMENT" + "\r\n" + "COM-3 simvols (necessarily) command" + "\r\n" + "PHONE-9 digit (necessarily)" + "\r\n" + "COMMENT-5 simvols (not necessarily)" ;
    SMSResp_Mess  =F("Wrong SMS format");
    SMSResp_Mess += "\r\n";
    SMSResp_Mess +=F("Must be: COM#PHONECOMMENT"); 
    SMSResp_Mess +="\r\n";
    SMSResp_Mess +=F("COM-3 simvols (necessarily) command");  
    SMSResp_Mess +="\r\n";
    SMSResp_Mess +=F("PHONE-9 digit (necessarily)");      
    SMSResp_Mess +="\r\n";
    SMSResp_Mess +=F("COMMENT-5 simvols (not necessarily)");            
    #ifndef NOSERIAL     
    Serial.println(SMSResp_Mess);
    #endif
    sendSMS(incoming_phone, SMSResp_Mess);
    return;
  }

  String comment = "";     //прикрепленный к номеру коментари (не более 5 символов)
  
  String phoneNUM =msg.substring(firstIndex+1, firstIndex+1+DIGIT_IN_PHONENAMBER);//номер телефона для операций

    if (msg.length() > 13)  // если  комментария тоже нет
     {
       comment = msg.substring(13);
       if (comment.length() > 6) comment = comment.substring(0, 5);
       IsComment=true; 
     }
    #ifndef NOSERIAL      
      Serial.println("SMS phonenumber " + phoneNUM);
      if (IsComment) Serial.println("SMS comment " + comment);  // если есть прикрепленный к номеру комментари
    #endif

  if (phoneNUM.length()>9){
    SMSResp_Mess="Phone-" + phoneNUM + " not added in White List! " + "\r\n" + "Wrong phonenumber: more then 9 digits.";
    #ifndef NOSERIAL     
      Serial.println(SMSResp_Mess);
    #endif
    sendSMS(incoming_phone, SMSResp_Mess);
     return;
  }
  for (int8_t z=0; z<phoneNUM.length(); z++){ // проверить что все символы номера это цифры
    if (phoneNUM.charAt(z) <= 47 || phoneNUM.charAt(z) >= 58){
    SMSResp_Mess="Phone-" + phoneNUM + " not added in White List! " + "\r\n" + "Wrong phonenumber ";  
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
       if (j<3) SMS_text_comanda[j] = msg[j];   // команда из СМС
       if (j<5 && IsComment) SMS_text_comment[j] = comment[j] ; // комментарий к номеру из СМС      
      }

  //  #ifndef NOSERIAL 
  //   Serial.println("Glob Phone SMS_text_num: " + String(SMS_text_num));     
  //   Serial.println("Glob comment SMS_text_comment: " + String(SMS_text_comment));      
  //   Serial.println("Glob command SMS_text_comanda: " + String(SMS_text_comanda)); 
  // #endif

   // ответ будет "+CPBF:"
  flag_modem_resp = 2; // установить флаг ослеживания ответа OK для одинократного поиска номера "+CPBF:"
  t_last_command = millis(); 
  SIM800.println("AT+CPBF=\"" + phoneNUM + "\"");//Найти номер в книге, phonen_index если нет 0  

}

 
// Функция выполнения команды полученной по СМС
void made_action()
 { String _command =String(SMS_text_comanda);
   String temp_respons;
   int bin_num_index = poisk_num(String(SMS_text_num));// проверить наличие такого номера в массиве
  //Выполнить комаду
  if (_command == "Add")  //Добавить новый номер на СИМ карту или в бинарный массив если на сим уже нет места.
    {         
       if (bin_num_index == -1 && ((app->alloc_num[1] > app->alloc_num[0]) || SMS_phoneBookIndex > 0) ) // если возможных номеров меньше существующих номеров (на сим карте)
         AddEditNewNumber();
      else if (bin_num_index == -1 && ((total_bin_num > app->alloc_num[2]) && SMS_phoneBookIndex == 0) ) {//Если в массиве бинарных номеров еще не все элементы заняты
          app->phones_on_sim[app->alloc_num[2]] = stringnum_to_bin(String(SMS_text_num));          
            //++app->alloc_num[2];
            app->_CreateFile(3);
            app-> saveFile("/PhoneBook.bin");
            sendSMS(String(SMS_incoming_num), F("New BIN File genereted"));  
      }   
      else if ((app->alloc_num[1] == app->alloc_num[0]) || (total_bin_num == app->alloc_num[2]))
      // Все номера на СИМ карте и в памяти заняты
           sendSMS(String(SMS_incoming_num), F("Memory is FULL ! Delete some numbers before adding NEW."));
      else sendSMS(String(SMS_incoming_num), F("Number allready exist."));
     return;
    }  
  else if (_command == "Del"){ // удалить один номер с СИМ карты
    if (SMS_phoneBookIndex > 0)
    {
      flag_modem_resp = 4; //Выставляем флаг для отслеживания OK 
      t_last_command = millis(); 
      SIM800.println("AT+CPBW=" + String(SMS_phoneBookIndex));
    }
    if (bin_num_index != -1) {
      app->phones_on_sim[bin_num_index] = 0;          
            //++app->alloc_num[2];
      app->_CreateFile(3);
      app-> saveFile("/PhoneBook.bin");
      sendSMS(String(SMS_incoming_num), F("New BIN File genereted"));  
    }    
    return;
  } 
 
  else if (_command == "Rds") { // прочитать список номеров с сим карты и создать файл
    // exist_numer();  //на старте выяснить сколько номеров уже есть в текущей книге и сколько всего возможно
     if (app->alloc_num[0] == 0) 
       sendSMS(String(SMS_incoming_num), F("Phone Book is EMPTY. NO File genereted"));
     else {
         flag_modem_resp = 5; //Выставляем флаг для отслеживания OK
         clear_arrey();
         t_last_command = millis(); 
         SIM800.println(F("AT+CPBF"));          
         app->_CreateFile(1);
     }
      return;
    }
    
  else if (_command == "Bin") { // прочитать список номеров с сим карты и создать бинарный файл
     if (app->alloc_num[2] == 0) 
       sendSMS(String(SMS_incoming_num), "BIN Phone Book is EMPTY. NO File genereted");
     else {
         app->_CreateFile(3);
         app-> saveFile("/PhoneBook.bin");
       sendSMS(String(SMS_incoming_num), "New BIN File genereted");         
      // просмотреть сохраненные в двоичном массиве номера
      //  for (uint8_t v = 0; v < total_bin_num; ++v){
      //       if (app->phones_on_sim[v] > 0){
      //         Serial.println(app->phones_on_sim[v], BIN);              
      //         Serial.println(app->phones_on_sim[v], DEC);
      //         Serial.println(BINnum_to_string(app->phones_on_sim[v])); // перевести двоичный вид номера в строку
      //       } 
      //   }
     }
    }  

  else if (_command == "Dan") { //Delete All Numbers удалить все номера из СИМ карты
      clear_arrey();  // чистим массив номеров и коментариев
      command_type = 5;   // 5 -  удалить все номера из СИМ карты
      return;      
  }     
  else if (_command == "Rms") { //Передать СМС с списком мастер номеров
      sendSMS(String(SMS_incoming_num), whiteListPhones);
      return;      
  } 
  else if (_command == "Wms") { //Добавить номер в список мастер номеров
      if (whiteListPhones.indexOf(String(SMS_text_num)) > -1) {  //если номер уже есть в белом списке - выйти
         sendSMS(String(SMS_incoming_num), "Number " + String(SMS_text_num) + "already exists in WhiteList.");        
        return;
      }  
      if (whiteListPhones.length() > 20) {  //если уже есть 3 номера в белом списке - выйти
        sendSMS(String(SMS_incoming_num), "WhiteList is FULL.");        
        return;
      }        
      if (whiteListPhones.length() > 8) whiteListPhones += ',' + String(SMS_text_num);
      app ->_whiteListPhones = whiteListPhones;
      app -> writeConfig();
      sendSMS(String(SMS_incoming_num), "New WHITE number " + String(SMS_text_num) + " Added successfully. New WhiteList: " + whiteListPhones);      
      return;      
  } 
  else if (_command == "Dms") { //Удалить номер из списка мастер номеров
      if (whiteListPhones.indexOf(String(SMS_text_num)) == -1) { //если номера нет в белом списке - выйти  
      sendSMS(String(SMS_incoming_num), "The number " + String(SMS_text_num) + " is not included in the white list.");      
      return;   
      }      
 // получить короткий номер с которого было послано СМС - последние симолы 
  String short_INnumber =String(SMS_text_num).substring(String(SMS_text_num).length()-(DIGIT_IN_PHONENAMBER-1)); 
   #ifndef NOSERIAL 
    Serial.println("short_INnumber: " + short_INnumber);     
   #endif       
      if (String(SMS_incoming_num).indexOf(short_INnumber) > -1) {//если есть попытка удалить свой номер из белого списка - выйти  
       sendSMS(String(SMS_incoming_num), "It is not possible to delete your own number " + String(SMS_text_num));      
      return;   
      }  
      char WtNum[3][DIGIT_IN_PHONENAMBER+1];
      int wn=0; int cwn=0;
      for (int j=0; j<3; ++j) {WtNum[j][0]=NULL; WtNum[j][DIGIT_IN_PHONENAMBER]=NULL;}

      for (int j=0; j<whiteListPhones.length(); ++j){
        if (cwn == DIGIT_IN_PHONENAMBER) {
          WtNum[wn][cwn]=NULL;
          ++wn; cwn=0;
         }
        else {
          WtNum[wn][cwn] = whiteListPhones[j]; ++cwn;          
        }
      } 
     #ifndef NOSERIAL 
         Serial.println("*****************"); 
         Serial.println(String(WtNum[0])); Serial.println(String(WtNum[1])); Serial.println(String(WtNum[2]));
         Serial.println("*****************");
     #endif
      String NewWhiteList="";    
      for (int j=0; j<3; ++j) {  
        if (String(WtNum[j]).indexOf(String(SMS_text_num)) > -1) { //если удаляемый номер
          WtNum[j][0]=NULL;
        }
        if (WtNum[j][0] !=NULL)
          if (j == 0) NewWhiteList += String(WtNum[j]); 
          else {NewWhiteList +=','; NewWhiteList += String(WtNum[j]);}
      }  
   #ifndef NOSERIAL       
         Serial.println("*** FAZA 2 ***"); 
         Serial.println(String(WtNum[0])); Serial.println(String(WtNum[1])); Serial.println(String(WtNum[2]));
         Serial.println(NewWhiteList); 
         Serial.println("*****************");
   #endif
        whiteListPhones = NewWhiteList;
        app ->_whiteListPhones = whiteListPhones;
        app -> writeConfig();     
      sendSMS(String(SMS_incoming_num), "New WHITE number " + String(SMS_text_num) + " Deleted successfully. New WhiteList: " + whiteListPhones);      
         
      return;      
  }               
  else if (_command == "Res") { //Restore сохранить все номера из файла PhoneBookNew.txt в СИМ карту
      clear_arrey();  // чистим массив номеров и коментариев
      app->readTXTfile();
      command_type = 4;   // 4 - скопировать с файла PhoneBookNew.txt все номера на СИМ      
      return;      
  } 
  /*  
  else if (msg.indexOf("Brd") > -1) { 
    // прочитать список номеров с бинарного файла
    //  for (int v = 0; v < 250; ++v) app->phones_on_sim[v] = 0;
      app->readBINfile();
    //   // просмотреть сохраненные в двоичном массиве номера
    //    for (uint8_t v = 0; v < 250; ++v){
    //         if (app->phones_on_sim[v] > 0){
    //           Serial.println(app->phones_on_sim[v], BIN);
    //           Serial.println(app->phones_on_sim[v], DEC);
    //           Serial.println(BINnum_to_string(app->phones_on_sim[v])); // перевести двоичный вид номера в строку
    //         } 
    //     }
    } 
    */
   else if (_command == "R11") { // Управлять реле через SMS  
     if (app->relayPin[0] != -1) {
       app->switchRelay(0, true);
       temp_respons = String(str_relay) + '1' + String(str_ON);
       }
   }
   else if (_command == "R21") { // Управлять реле через SMS
     if (app->relayPin[1] != -1) {
       app->switchRelay(1, true);
       temp_respons = String(str_relay) + '2' + String(str_ON);
       }       
   }
   else if (_command == "R31") { // Управлять реле через SMS
     if (app->relayPin[2] != -1) {
       app->switchRelay(2, true);    
       temp_respons = String(str_relay) + '3' + String(str_ON);
       }
   }         
   else if (_command == "R41") { // Управлять реле через SMS 
     if (app->relayPin[3] != -1) {
       app->switchRelay(3, true);
       temp_respons = String(str_relay) + '4' + String(str_ON);
       }       
   }     
   else if (_command == "RA1") { // Управлять реле через SMS  
      temp_respons = String(str_relay);
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
   else if (_command == "R10") { // Управлять реле через SMS  
     if (app->relayPin[0] != -1) {
       app->switchRelay(0, false);
       temp_respons = String(str_relay) + '1' + String(str_OFF);
       }          
   }
   else if (_command == "R20") { // Управлять реле через SMS
     if (app->relayPin[1] != -1) {
       app->switchRelay(1, false);
       temp_respons = String(str_relay) + '2' + String(str_OFF);
       }         
   }
   else if (_command == "R30") { // Управлять реле через SMS
     if (app->relayPin[2] != -1) {
       app->switchRelay(2, false);
       temp_respons = String(str_relay) + '3' + String(str_OFF);
       }         
   }         
   else if (_command == "R40") { // Управлять реле через SMS 
     if (app->relayPin[3] != -1) {
       app->switchRelay(3, false);    
       temp_respons = String(str_relay) + '4' + String(str_OFF);
       }         
   }     
   else if (_command == "RA0") { // Управлять реле через SMS  
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

  sendSMS(String(SMS_incoming_num), temp_respons); 
    
}

void sendSMS(String phone, String message){
  String _tempSTR = "AT+CMGS=\"";
  _tempSTR += phone;
  _tempSTR += "\"\n"; //*********!!!!!!!!!!!!!!******************S
  _tempSTR += message;
  _tempSTR += "\r\n";
  _tempSTR += (String)((char)26);
   #ifndef NOSERIAL 
    Serial.println("SMS out: " + _tempSTR);
  #endif  
    SIM800.println(_tempSTR);
  return;
}

//Добавление (или изменение) номера в справочную книгу
void AddEditNewNumber(){
  String temp_resp="";
    temp_resp = FPSTR("AT+CPBW=");
    if (SMS_phoneBookIndex>0)  temp_resp += String(SMS_phoneBookIndex); // если такой номер уже есть - изменить его, а не добавлять новый
    temp_resp += ",\""; 
    temp_resp += String(SMS_text_num); 
    temp_resp += "\",129,\""; 
    temp_resp += String(SMS_text_num);
    if (IsComment) temp_resp += String(SMS_text_comment) ; // если есть прикрепленный к номеру комментари
    temp_resp +="\"";
   flag_modem_resp = 3; //Выставляем флаг для отслеживания OK 
   t_last_command = millis(); 
   SIM800.println(temp_resp);
}

// процедура выясняет количество имеющихся номеров в книге и общее возможное количество и сохранет их в массив alloc_num[]
void exist_numer(){
  SIM800.println("AT+CPBS?");
  return;
}

// Если звонок от БЕЛОГО номера - ответить, включить реле и сбросить вызов
void regular_call()
{ SIM800.println("ATA");   // Если да, то отвечаем на вызов    
  app->switchRelay(0, true); // Если да, то включаем LED
  SIM800.println("ATH"); // Завершаем вызов
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
int poisk_num(const String& txt_num){
  int _ret=-1;
  uint64_t found_num = stringnum_to_bin(txt_num);
  for (int v = 0; v < total_bin_num; ++v)
  { if (app->phones_on_sim[v] != 0)
       if (app->phones_on_sim[v] == found_num)
            _ret=v;
  }
  return _ret;
}