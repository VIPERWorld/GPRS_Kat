/*
Программа передачи данных по каналу GPRS
24.01.2017г.



*/

#include <SoftwareSerial.h>
#include <OneWire.h> 
#include <DallasTemperature.h>
#include <avr/pgmspace.h>
#include <EEPROM.h>
#include <avr/wdt.h>
#include <MsTimer2.h> 


#define con Serial
static const char* url1 = "http://trm7.xyz/r.php";
//static const char* url1   = "http://vps3908.vps.host.ru/recieveReadings.php";
static const char* urlssl = "https://trm7.xyz/r.php";
static const char* url_ping = "www.yandex.ru";

#define PIN_TX           7                              // Подключить  к выводу 7 сигнал RX модуля GPRS
#define PIN_RX           8                              // Подключить  к выводу 8 сигнал TX модуля GPRS

SoftwareSerial SIM_SERIAL(PIN_RX, PIN_TX);
 

#define PWR_On           5                              // Включение питания модуля SIM800
#define SIM800_RESET_PIN 6                              // Сброс модуля SIM800
#define LED13           13                              // Индикация светодиодом
#define NETLIGHT         3                              // Индикация NETLIGHT
#define STATUS           9                              // Индикация STATUS
												
//#define COMMON_ANODE                                  // Если светодиод с общим катодом - раскомментировать
#define LED_RED      10                                 // Индикация светодиодом RED
#define LED_BLUE     15                                 // Индикация светодиодом BLUE
#define LED_GREEN    14                                 // Индикация светодиодом GREEN

#define COLOR_NONE LOW, LOW, LOW                        // Отключить все светодиоды
#define COLOR_GREEN LOW, HIGH, LOW                      // Включить зеленый светодиод
#define COLOR_BLUE LOW, LOW, HIGH                       // Включить синий светодиод
#define COLOR_RED HIGH, LOW, LOW                        // Включить красный светодиод
volatile int stateLed = LOW;                            // Состояние светодиода при прерывистой индикации на старте
volatile int state_device = 0;                          // Состояние модуля при запуске 

												        // 1 - Не зарегистрирован в сети, поиск
												        // 2 - Зарегистрировано в сети
												        // 3 - GPRS связь установлена
volatile int metering_NETLIGHT       = 0;
volatile unsigned long metering_temp = 0;
volatile int count_blink1            = 0;               // Счетчик попыток подключиться к базовой станции
volatile int count_blink2            = 0;               // Счетчик попыток подключиться к базовой станции
bool send_ok                         = false;           // Признак успешной передачи данных
bool count_All_reset                 = false;           // Признак выполнения команды сброса счетчика ошибок.

//CGPRS_SIM800 gprs;
uint32_t count      = 0;
uint32_t errors     = 0;
//String imei       = "";
String imei         = "861445030362268";                // Тест IMEI
String CSQ          = "";                               // Уровень сигнала приема
String SMS_center   = "";
String zero_tel     = "";
String SIMCCID      = "";
String CMTE         = "";                               // Внутренний датчик температуры
#define DELIM "@"

unsigned long time             = 0;                     // Переменная для суточного сброса
unsigned long time_day         = 86400;                 // Переменная секунд в сутках
unsigned long previousMillis   = 0;                     //  
unsigned long interval         = 60;                    // Интервал передачи данных 60 секунд
//unsigned long interval         = 300;                   // Интервал передачи данных 5 минут
bool time_set                  = false;                 // Фиксировать интервал заданный СМС
bool ssl_set                   = false;                 // Признак шифрования
unsigned long time_ping        = 60;                   // Интервал проверки ping 6 минут.
unsigned long previousPing     = 0;                     // Временный Интервал проверки ping

int Address_tel1       = 100;                           // Адрес в EEPROM телефона 1
int Address_ssl        = 120;                           // Адрес в EEPROM признака шифрования
int Address_errorAll   = 160;                           // Адрес в EEPROM счетчика общих ошибок
int Address_interval   = 200;                           // Адрес в EEPROM величины интервала
int Address_SMS_center = 220;                           // Адрес в EEPROM SMS центра

char data_tel[16];                                      // Буфер для номера телефоа
byte ret        = 6;                                    // Признак "0" успешного подключения к интернету
byte operator_Num = 0;                                  // Порядковый номер оператора
char txt_Ok[]   = "OK\r";
char txt_ERROR[]= "ERROR\r";

// ++++++++++++++ Переменные библиотеки ++++++++++++++++++++



typedef enum {
	HTTP_DISABLED = 0,
	HTTP_READY,
	HTTP_CONNECTING,
	HTTP_READING,
	HTTP_ERROR,
} HTTP_STATES;


#define DEBUG Serial

byte m_bytesRecv;                         // 
uint32_t m_checkTimer;
String apn = "";
String user = "";
String pwd = "";
String cont = "";
char buffer[200];
byte httpState;
String val = "";


//bool begin(Stream &port);
// setup network
//byte setup();
void close_GPRS();

uint8_t getNetworkStatus();

// get network operator name
bool getOperatorName();
bool getIMEI();

bool getSIMCCID();

bool ping(const char* url);
// check for incoming SMS
bool checkSMS();
bool deleteSMS(int n_sms);
// get signal quality level (in dB)
int getSignalQuality();
// initialize HTTP connection
bool httpInit();
// terminate HTTP connection
void httpUninit();
// connect to HTTP server
bool httpConnect(const char* url, const char* args = 0);
bool httpConnectStr(const char* url, String args = "");
boolean HTTP_ssl(boolean onoff);
// check if HTTP connection is established
// return 0 for in progress, 1 for success, 2 for error
byte httpIsConnected();
// read data from HTTP connection
void httpRead();
// check if HTTP connection is established
// return 0 for in progress, -1 for error, bytes of http payload on success
int httpIsRead();
// send AT command and check for expected response
byte sendCommand(const char* cmd, unsigned int timeout = 2000, const char* expected = 0);
// send AT command and check for two possible responses
byte sendCommand(const char* cmd, const char* expected1, const char* expected2, unsigned int timeout = 2000);

// check if there is available serial data
bool available();
void cleanStr(String & str);

byte checkbuffer(const char* expected1, const char* expected2 = 0, unsigned int timeout = 2000);  // По умолчанию ожидание 2 секунды
void purgeSerial();


const char  txt_CREG[]               PROGMEM = "AT+CREG?";                            //***
const char  txt_CSQ[]                PROGMEM = "AT+CSQ";                              //***
const char  txt_CGATT[]              PROGMEM = "AT+CGATT?";                           //***
const char  txt_SAPBR0[]             PROGMEM = "AT+SAPBR=3,1,\"Contype\",\"GPRS\"";   //***
const char  txt_internet_mts_ru[]    PROGMEM = "internet.mts.ru";                     //***
const char  txt_mts[]                PROGMEM = "mts";                                 //*** 
const char  txt_internet_beeline[]   PROGMEM = "internet.beeline.ru";                 //***
const char  txt_beeline[]            PROGMEM = "beeline";                             //*** 
const char  txt_internet[]           PROGMEM = "internet";                            //***
const char  txt_CCID[]               PROGMEM = "AT+CCID";                             //***
const char  txt_SAPBR4[]             PROGMEM = "AT+SAPBR=1,1";                        //***
const char  txt_SAPBR5[]             PROGMEM = "AT+SAPBR=2,1";                        //***
const char  txt_CPMS[]               PROGMEM = "AT+CPMS=\"SM\",\"SM\",\"SM\"";
const char  txt_GSN[]                PROGMEM = "AT+GSN";                              //***
const char  txt_COPS[]               PROGMEM = "AT+COPS?";                            //***
const char  txt_HTTPTERM[]           PROGMEM = "AT+HTTPTERM";                         //***
const char  txt_HTTPINIT[]           PROGMEM = "AT+HTTPINIT";                         //***
const char  txt_HTTPPARA1[]          PROGMEM = "AT+HTTPPARA=\"CID\",1";               //***    
const char  txt_200[]                PROGMEM = "0,200";                               //***
const char  txt_60[]                 PROGMEM = "0,60";                                //***
const char  txt_HTTPREAD[]           PROGMEM = "+HTTPREAD: ";                         //***
const char  txt_Error1[]             PROGMEM = "Error";                               //***
const char  txt_r_n[]                PROGMEM = "\r\n";                                //***
const char  txt_CIPSHUT[]            PROGMEM = "AT+CIPSHUT";                          //***
const char  txt_CSQ1[]               PROGMEM = "CSQ: ";                               //***
const char  txt_ERROR1[]             PROGMEM = "ERROR";                               //*** 
const char  txt_CSTT[]               PROGMEM = "AT+CSTT=\"internet\"";                //***
const char  txt_CIICR[]              PROGMEM = "AT+CIICR";                            //***
const char  txt_CIFSR[]              PROGMEM = "AT+CIFSR";                            //*** 
const char  txt_PCIPPING[]           PROGMEM = "+CIPPING";                            //***
const char  txt_CMGDA[]              PROGMEM = "AT+CMGDA=\"DEL ALL\"";   //***
const char  txt_CSTT_mts[]           PROGMEM = "AT+CSTT=\"internet.mts.ru\"";
const char  txt_CSTT_Bee[]           PROGMEM = "AT+CSTT=\"internet.beeline.ru\"";
const char  txt_CSTT_internet[]      PROGMEM = "AT+CSTT=\"internet\"";
const char  txt_SSL1[]               PROGMEM = "AT+HTTPSSL=1";
const char  txt_SSL0C[]              PROGMEM = "AT+HTTPSSL=0";


const char* const table_message[] PROGMEM =
{
	txt_CREG,                    // 0 "AT+CREG?"
	txt_CSQ,                     // 1 "AT+CSQ"
	txt_CGATT,                   // 2 "AT+CGATT?"
	txt_SAPBR0,                  // 3 "AT+SAPBR=3,1,\"Contype\",\"GPRS\""
	txt_internet_mts_ru,         // 4 "internet.mts.ru"
	txt_mts,                     // 5 "mts
	txt_internet_beeline,        // 6 "internet.beeline.ru";
	txt_beeline,                 // 7 "beeline";
	txt_internet,                // 8 "internet"; 
	txt_CCID,                    // 9 ""AT+CCID"";
	txt_SAPBR4,                  // 10 "AT+SAPBR=1,1";
	txt_SAPBR5,                  // 11 "AT+SAPBR=2,1";
	txt_CPMS,                    // 12 "AT+CPMS=\"SM\",\"SM\",\"SM\"";
	txt_GSN,                     // 13 "AT+GSN";
	txt_COPS,                    // 14 "AT+COPS?";
	txt_HTTPTERM,                // 15 "AT+HTTPTERM";
	txt_HTTPINIT,                // 16 "AT+HTTPINIT";
	txt_HTTPPARA1,               // 17 "AT+HTTPPARA=\"CID\",1";
	txt_200,                     // 18 "0,200" ;
	txt_60,                      // 19 "0,60";
	txt_HTTPREAD,                // 20 "+HTTPREAD: ";
	txt_Error1,                  // 21 "Error";
	txt_r_n,                     // 22 "\r\n";  
	txt_CIPSHUT,                 // 23 "AT+CIPSHUT";
	txt_CSQ1,                    // 24 "CSQ: "; 
	txt_ERROR1,                  // 25 "ERROR"
	txt_CSTT,                    // 26 "AT+CSTT=\"internet\"";
	txt_CIICR,                   // 27 "AT+CIICR";
	txt_CIFSR,                   // 28 "AT+CIFSR";
	txt_PCIPPING,                // 29 "+CIPPING";
	txt_CMGDA,                   // 30 "AT+CMGDA=\"DEL ALL\"";
	txt_CSTT_mts,                // 31 "AT+CSTT=\"internet.mts.ru\"";
	txt_CSTT_Bee,                // 32 "AT+CSTT=\"internet.beeline.ru\"";
	txt_CSTT_internet,           // 33 "AT+CSTT=\"internet\"";
	txt_SSL1,                    // 34 "AT+HTTPSSL=1";
	txt_SSL0C                    // 35 "AT+HTTPSSL=0";

};





//===============================================================================================================================


uint8_t oneWirePins[]={16, 17, 4};                      //номера датчиков температуры DS18x20. Переставляя номера можно устанавливать очередность передачи в строке.
													    // Сейчас первым идет внутренний датчик.
uint8_t oneWirePinsCount=sizeof(oneWirePins)/sizeof(int);

OneWire ds18x20_1(oneWirePins[0]);
OneWire ds18x20_2(oneWirePins[1]);
OneWire ds18x20_3(oneWirePins[2]);
DallasTemperature sensor1(&ds18x20_1);
DallasTemperature sensor2(&ds18x20_2);
DallasTemperature sensor3(&ds18x20_3);

void(* resetFunc) (void) = 0;                           // объявляем функцию reset

void flash_time()                                      // Программа обработчик прерывистого свечения светодиодов при старте
{
	if (state_device == 0)
	{
		setColor(COLOR_NONE);
	}
	if (state_device == 1)
	{
		stateLed = !stateLed;
		if (!stateLed)
		{
			setColor(COLOR_RED);
		}
		else
		{
			setColor(COLOR_NONE);
		}
	}

	if (state_device == 2)
	{
		stateLed = !stateLed;
		if (!stateLed)
		{
			setColor(COLOR_NONE);
		}
		else
		{
			setColor(COLOR_BLUE);
		}
	}

	if (state_device == 3)
	{
		stateLed = !stateLed;
		if (!stateLed)
		{
			setColor(COLOR_NONE);
		}
		else
		{
			setColor(COLOR_GREEN);
		}
	}
}

 void setColor(bool red, bool green, bool blue)        // Включение цвета свечения трехцветного светодиода.
 {
	  #ifdef COMMON_ANODE                              // Если светодиод с общим катодом
		red = !red;
		green = !green;
		blue = !blue;
	  #endif 
	  digitalWrite(LED_RED, red);
	  digitalWrite(LED_GREEN, green);
	  digitalWrite(LED_BLUE, blue);    
 }
 
void sendTemps()
{
	Serial.println(F("\nSend the data to the site"));
	sensor1.requestTemperatures();
	sensor2.requestTemperatures();
	sensor3.requestTemperatures();
	float t1 = sensor1.getTempCByIndex(0);
	float t2 = sensor2.getTempCByIndex(0); 
	float t3 = sensor3.getTempCByIndex(0);
	float tsumma = t1 + t2 + t3 + 88.88;
	int signal = getSignalQuality();
	int error_All = 0;
	EEPROM.get(Address_errorAll, error_All);
	//+ "17/2/1,21:2:28%2000"
	String toSend = "t1=";// +imei + DELIM + String(t1) + DELIM + String(t2) + DELIM + String(t3) + DELIM + String(signal) + DELIM + String(errors) + DELIM + String(error_All) + formEnd() + DELIM + String(tsumma);

//	Serial.println(toSend);

	Serial.println(toSend.length());

	int count_send = 0;
	gprs_send(toSend);

	//while (1)
	//{
	//	if (gprs_send(toSend))
	//	{
	//		return;
	//	}
	//	else
	//	{
	//		count_send++;
	//		Serial.print("Attempt to transfer data .."); Serial.println(count_send);
	//		if (count_send>5) resetFunc();                                // 5 попыток. Что то пошло не так с интернетом
	//		//ping();
	//	}
	//	delay(6000);
	//}

} 


String formEnd() 
{
	char buf[13] ;
	EEPROM.get(Address_tel1, buf);
	String master_tel1(buf);
	EEPROM.get(Address_SMS_center, SMS_center);   //Получить из EEPROM СМС центр
	return DELIM + master_tel1 + DELIM + SIMCCID;
}

bool gprs_send(String data) 
{
	con.print(F("Requesting .. Wait"));   
	ret = connect_GPRS();                                   // Подключение к интернету с применением стека HTTP
	Serial.print(F("ret - ")); Serial.print(ret);
	int count_init = 0;
   // t1=861445030362268@17/2/12,15:15:27 00@-127.00@-127.00@-127.00@13@0@0@+79162632701@89701010063818618608@-292.12
	//if (check_connect(ret))
	//{
	// //setup_ok = true;
	//}
	//else
	//{
	//   count_init++;             // Увеличить счетчик количества попыток 
	//}

 // int count_init = 0;                                    // Счетчик количества попыток подключиться к HTTP
  for (;;)                                               // Бесконечный цикл пока не наступит, какое то состояние для выхода
  {
	  if (httpInit()) break;                        // Все нормально, модуль ответил , Прервать попытки и выйти из цикла
	  con.print(">");
	  con.println(buffer);                          // Не получилось, ("ERROR") 
	  String stringError = buffer;
	  if (stringError.indexOf(F("ERROR")) > -1)          
	  {
		  con.println(F("\nNo internet connection"));
		  delay(1000);
	  }
	   httpUninit();                                 // Не получилось, разединить и  попробовать снова 
	  delay(1000);                                       // Подождать секунду.
	  count_init++;
	  if(count_init > 60)  resetFunc();                 //вызываем reset при отсутствии доступа к серверу в течении 60 секунд
  }

  if (ssl_set == true)
  {
	  if (HTTP_ssl(true))
	  {
		  con.println(F("\nHTTP_ssl: set ON successfully!"));
	  }
	  else
	  {
		  con.println(F("\nHTTP_ssl: set ON false!"));
	  }
  }
  else
  {
	  if (HTTP_ssl(false))
	  {
		  con.println(F("\nHTTP_ssl: set OFF successfully!"));
	  }
	  else
	  {
		  con.println(F("\nHTTP_ssl: set OFF false!"));
	  }

  }

  //while (1)
  //{
  //	if (gprs_send(toSend))
  //	{
  //		return;
  //	}
  //	else
  //	{
  //		count_send++;
  //		Serial.print("Attempt to transfer data .."); Serial.println(count_send);
  //		if (count_send>5) resetFunc();                                // 5 попыток. Что то пошло не так с интернетом
  //		//ping();
  //	}
  //	delay(6000);
  //}

 
  if (ssl_set == true)
  {
	  con.print(urlssl);
	  con.print('?');
	  con.println(data);

	  httpConnectStr(urlssl, data);
  }
  else
  {
	  data = "t1 = 861445030362268@17/2 / 12, 15:15 : 27 00@-127.00@-127.00@-127.00@13@0@0@+79162632701@89701010063818618608@-292.12";
	  con.print(url1);
	  con.print('?');
	  con.println(data);
	 
	  httpConnectStr(url1, data);
  }
    
  count++;
  send_ok = false;
    
  while (httpIsConnected() == 0) 
  {
	con.write('.');
	for (byte n = 0; n < 25 && !available(); n++) 
	{
	  delay(15);
	}
  }
  if (httpState == HTTP_ERROR) 
  {
	con.println(F("Connect error"));
		errors++;
		errorAllmem();
	
	if (errors > 20)
	  {
			con.println(F("Errors exceeded"));
			delay(3000);
			resetFunc();          // вызываем reset после 20 ошибок
	  }
	delay(3000);
	return; 
  }
  
  con.println();
  httpRead();
  int ret;
  while ((ret = httpIsRead()) == 0)  //  Ожидаем сообщение HTTP
  {
	// может сделать что-то здесь, во время ожидания
  }

  if (httpState == HTTP_ERROR)        // Ответ не пришел
  {
	errors++;
	errorAllmem();
	if (errors > 20)
	  {
			con.println(F("The number of server errors exceeded 20"));
			delay(3000);
			resetFunc();         // вызываем reset после 20 ошибок
	  }
	delay(3000);
	return; 
  } 

  


  // Теперь мы получили сообщение от сайта.
   con.print(F("[Payload] "));                        
   con.println(buffer);
   String val = buffer;                            // Получить строку данных с сервера
  // gprs.httpUninit();                                    // Разорвать соединение

   int p0[5];
  // String val = "&010145&0202+79162632701&0303+79162632701&0400123456789#";  // Пример строки, принятой с сервера
   send_ok = true;                                                             // Команда принята успешно

   if (val.indexOf("&") > -1)              // Определить адреса (позиции) команд в строке 
   {
	   p0[0] = val.indexOf("&01");         // Адрес 1 команды
	   p0[1] = val.indexOf("&02");         // Адрес 2 команды
	   p0[2] = val.indexOf("&03");         // Адрес 3 команды
	   p0[3] = val.indexOf("&04");         // Адрес 4 команды
	   p0[4] = val.indexOf('#');           // Адрес конца команд
   }

   for (int i=0;i<4;i++)
   {
	  String comm = val.substring(p0[i]+3, p0[i]+5);          // Выделить строку с номером команды
	  int comm1 = comm.toInt(); 
	  Serial.println(comm1);
	  comm = val.substring(p0[i] + 5, p0[i+1]);               // Выделить строку с данными
	  int len_str = comm.length();
	  comm[len_str] = '\0';
	  Serial.println(comm);

	  run_command(comm1, comm);                               // Последовательно выполнить все команды

   }

  // Показать статистику
  con.print(F("Total: "));                                   
  con.print(count);
  if (errors)                                           // Если есть ошибки - сообщить
  {
	con.print(F(" Errors: "));                                
	con.print(errors);
  }
  con.println();
  Serial.print(F("Inteval: "));
  Serial.println(interval);
  httpUninit();                                    // Разорвать соединение

   return send_ok;
}

void run_command(int command, String data)
{
	unsigned long interval1 = 0;
	switch (command) 
	{
		case 1:
			interval1 = data.toInt();                              // Преобразовать строку данных в число 
			con.println(interval1);
			if (interval1 > 29 && interval1 < 86401)               // Ограничить интервалы от 29  секунд до 24 часов.
			{
				if (interval1 != interval)                         // Если информиция не изменилась - не писать в EEPROM
				{
					if (!time_set)                                 // Если нет команды фиксации интервала от СМС 
					{
						interval = interval1;                      // Переключить интервал передачи на сервер
						EEPROM.put(Address_interval, interval1);   // Записать интервал EEPROM , полученный от сервера
					}
				}
			}
			con.println(interval);
			break;
		case 2:
			EEPROM.get(Address_tel1, data_tel);             // Получить номер телефона из EEPROM
			Serial.println(data_tel);
			if (data != data_tel)                           // Если информиция не изменилась - не писать в EEPROM
			{
				con.println(F("no compare"));               //Serial.println("no compare");
				char buf[16];
				for (int i = 0; i<13; i++)
				{
					EEPROM.write(i + Address_tel1, data[i]);
				}
				EEPROM.get(Address_tel1, buf);
				Serial.println(buf);
			}
			else
			{
				con.println(F("Tel compare"));                
			}
			break;
		case 3:
			EEPROM.get(Address_SMS_center, data_tel);      // Получить из EEPROM СМС центр
			if (data != data_tel)                        // Если информиция не изменилась - не писать в EEPROM
			{
				Serial.println(F("no compare"));
				for (int i = 0; i<13; i++)
				{
					EEPROM.write(i + Address_SMS_center, data[i]);
				}
			}

			break;
		case 4:
			if (count_All_reset = false)                   // Признак выполнения команды сброса счетчика ошибок
			{
				count_All_reset == true;                   // Команда сброса выполнена. Повторный сброс возможен после перезагрузки
				EEPROM.put(Address_errorAll, 0);             // Сбросить счетчик ошибок Предусмотреть блокировку повторной записи???
			}
			break;
		case 5:
			time_set = false;                              // Снять фиксацию интервала заданного СМС
			break;
		//case 6:
		//	//выполняется когда  var равно 6
		//	break;
		//case 7:
		//	//выполняется, когда var равно 7
		//	break;
		//case 8:
		//	//выполняется когда  var равно 8
		//	break;
		default:
			break;
			// выполняется, если не выбрана ни одна альтернатива
			// default необязателен
	}
}

void errorAllmem()                                // Запись всех ошибок в память EEPROM
{
  int error_All;
  EEPROM.get(Address_errorAll, error_All);
  error_All++;
  EEPROM.put(Address_errorAll, error_All);            
}

int freeRam()                                   // Определить свободную память
{
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}

void setTime(String val, String f_phone)
{
  if (val.indexOf(F("Timeset")) > -1)                   // 
  {
	 interval = 40;                                     // Установить интервал 40 секунд
	 time_set = true;                                   // Установить фиксацию интервала заданного СМС
	 Serial.println(interval);
  } 
  else if (val.indexOf(F("Restart")) > -1) 
  {
	  Serial.print(f_phone);
	  Serial.print("..");
	  Serial.println(F("Restart"));
	  delay(2000);
	  resetFunc();                                     //вызываем reset
  } 
  else if (val.indexOf(F("Timeoff")) > -1) 
  {
	 time_set = false;                              // Снять фиксацию интервала заданного СМС
	 Serial.println(F("Timeoff"));
  } 
  else if (val.indexOf(F("Sslon")) > -1)
  {
	  EEPROM.write(Address_ssl, true);                // Включить шифрование
	  Serial.println(F("HTTP SSL ON"));
	  delay(2000);
	  resetFunc();
  }
  else if (val.indexOf(F("Ssloff")) > -1)
  {
	  EEPROM.write(Address_ssl, false);                  // Отключить шифрование
	  Serial.println(F("HTTP SSL OFF"));
	  delay(2000);
	  resetFunc();
  }
  else
  {
	  Serial.println(F("Unknown command"));         // Serial.println("Unknown command");
  }
}

void check_blink()
{
	unsigned long current_M = millis();

	wdt_reset();
	
	metering_NETLIGHT = current_M - metering_temp;                            // 
	metering_temp = current_M;
	if (metering_NETLIGHT > 3055 && metering_NETLIGHT < 3070)
	{
		state_device = 2;                                                     // 2 - Зарегистрировано в сети
		count_blink2++;
		if (count_blink2 > 50)
		{
			state_device = 0;
			MsTimer2::stop();                                                 // Включить таймер прерывания
	    	resetFunc();                                                      // Что то пошло не так с регистрацией на станции
	    }
	}
	else if (metering_NETLIGHT > 855 && metering_NETLIGHT < 870)
	{
		state_device = 1;                                                     // 1 Не зарегистрирован в сети, поиск
		count_blink1++;
		if (count_blink1 > 120) 
		{
			state_device = 0;
			MsTimer2::stop();                                                 // Включить таймер прерывания
			resetFunc();                                                      // Что то пошло не так с регистрацией на станции
		}
	}
	else if (metering_NETLIGHT > 350 && metering_NETLIGHT < 370)
	{
		state_device = 3;                                                     // 3 - к интернету подключен
	}
}

bool check_ping()
{
	con.print(F("Ping -> "));
	con.print(url_ping);
	if (ping(url_ping))
	{
		con.println(F(".. Ok!"));
		return true;
	}
	con.println(F(".. false!"));
	return false;
}
void ping()
{
	int count_ping = 0;
	while (1)
	{
		if (check_ping())
		{
			return;
		}
		else
		{
			count_ping++;
			if (count_ping > 7) resetFunc(); // 7 попыток. Что то пошло не так с интернетом
		}
		delay(1000);
	}
}

void start_init()
{
	bool setup_ok = false;
	uint8_t count_init = 0;
	MsTimer2::start();                                                 // Включить таймер прерывания
	do
	{
		con.println(F("Initializing....(May take 5-10 seconds)"));

		digitalWrite(SIM800_RESET_PIN, LOW);                      // Сигнал сброс в исходное состояние
		digitalWrite(LED13, LOW);
		digitalWrite(PWR_On, HIGH);                               // Кратковременно отключаем питание модуля GPRS
		delay(2000);
		digitalWrite(LED13, HIGH);
		digitalWrite(PWR_On, LOW);
		delay(1500);
		digitalWrite(SIM800_RESET_PIN, HIGH);                     // Производим сброс модема после включения питания
		delay(1200);
		digitalWrite(SIM800_RESET_PIN, LOW);
		int count_status = 0;
		while (digitalRead(STATUS) == LOW)
		{
			count_status++;
			if(count_status > 150) resetFunc(); // 100 попыток. Что то пошло не так программа перезапуска  если модуль не включился
			delay(100);
		}
		delay(2000);
		con.println(F("Power SIM800 On"));

	
		if (getIMEI())                       // Получить IMEI
		{
			con.print(F("\nIMEI:"));
			imei = buffer;                 // Отключить на время отладки
			cleanStr(imei);                // Отключить на время отладки
			con.println(imei);
		}
		else
		{
			con.print(F("\nIMEI: NO"));
			//return;  // IMEI не определился
		}

		
		
		if (getSIMCCID())                       // Получить Номер СИМ карты
		{
			con.print(F("\nSIM CCID:"));
			SIMCCID = buffer;                 //  
			cleanStr(SIMCCID);                //  
			con.println(SIMCCID);
		}
		else
		{
			return;  // SIMCCID не определился
		}
		

		while (state_device != 2)  // Ожидание регистрации в сети
		{
			delay(1000);
			// Уточнить программу перезапуска  если модуль не зарегистрировался через 70 секунд
		}
		delay(1000);
		Serial.print(F("\nSetting up network..."));

		char n = getNetworkStatus();
		
		Serial.print(F("\nNetwork status "));
		//Serial.print(n);
		Serial.print(F(": "));
		if (n == '0') Serial.println(F("\nNot registered"));                      // 0 – не зарегистрирован, поиска сети нет
		if (n == '1') Serial.println(F("\nRegistered (home)"));                   // 1 – зарегистрирован, домашняя сеть
		if (n == '2') Serial.println(F("\nNot registered (searching)"));          // 2 – не зарегистрирован, идёт поиск новой сети
		if (n == '3') Serial.println(F("\nDenied"));                              // 3 – регистрация отклонена
		if (n == '4') Serial.println(F("\nUnknown"));                             // 4 – неизвестно
		if (n == '5') Serial.println(F("\nRegistered roaming"));                  // 5 – роуминг

		if (n == '1' || n == '5')                                                 // Если домашняя сеть или роуминг
		{
			if (state_device == 2)                                                // Проверить аппаратно подключения модема к оператору
			{
				delay(2000);
				/*	do
					{*/
				int signal = getSignalQuality();
				Serial.print(F("rssi ..")); Serial.println(signal);

				Serial.print(F("Init GPRS.. "));// Serial.println(n);

												//strcpy_P(bufcom, (char*)pgm_read_word(&(table_message[48])));
												//sendCommand(bufcom, 20000);                  // sendCommand("AT+CIPSHUT", 20000);

												//if (!sendCommand("AT+CGATT=1")) return 2;    // Регистрация в GPRS

												//strcpy_P(bufcom, (char*)pgm_read_word(&(table_message[9])));
				//if (!sendCommand("AT+CGATT?")) return 2;   // if (!sendCommand("AT+CGATT?"))     // Регистрация в GPRS

														   //strcpy_P(bufcom, (char*)pgm_read_word(&(table_message[10])));
				//if (!sendCommand("AT+SAPBR=3,1,\"Contype\",\"GPRS\"")) return 3;   // if (!sendCommand("AT+SAPBR=3,1,\"Contype\",\"GPRS\""))     return 3;// 

				getOperatorName();

				delay(1000);
				Serial.println(F("GPRS connect .."));

				//ret = connect_GPRS();                                              // Подключение к GPRS
				//Serial.print(F("ret - ")); Serial.print(ret);

				//if (check_connect(ret))
				//{
					setup_ok = true;
				//}
				//else
				//{
				//   count_init++;             // Увеличить счетчик количества попыток 
			 //   }
			  //} while (!setup_ok);             // 
			}
		}
	} while (count_init > 30 || setup_ok == false);    // 30 попыток зарегистрироваться в сети
}


bool check_connect(byte ret)
{
	if (ret == 0)
	{
		while (state_device != 3)  // Ожидание регистрации в сети
		{
			delay(50);
			// Уточнить программу перезапуска  если модуль не зарегистрировался не зарегистрировался через 60 секунд
		}
		Serial.println(F("\nGPRS connect OK!+"));
		return  true;
	}
	else           // Модуль не подключиля к интернету
	{
		//count_init++;             // Увеличить счетчик количества попыток 
		Serial.println(F("Failed init GPRS"));
		delay(5000);
		if (state_device == 3)      // Модуль одумался и все таки подключиля к интернету
		{
			Serial.println(F("GPRS connect OK!-"));
			return true;
		}
	}
	return false;
}






void setup()
{
	wdt_disable(); // бесполезна¤ строка до которой не доходит выполнение при bootloop Не уверен!!
	con.begin(115200);
	con.println(F("\nSIM800 setup start"));   

	SIM_SERIAL.begin(19200);

	pinMode(SIM800_RESET_PIN, OUTPUT);
	pinMode(LED13, OUTPUT);
	pinMode(PWR_On, OUTPUT);

	pinMode(LED_RED,  OUTPUT);
	pinMode(LED_BLUE, OUTPUT);
	pinMode(LED_GREEN,OUTPUT);
	pinMode(NETLIGHT ,INPUT);                      // Индикация NETLIGHT
	pinMode(STATUS ,INPUT);                        // Индикация STATUS

	setColor(COLOR_RED);
	delay(300);
	setColor(COLOR_GREEN); 
	delay(300);
	setColor(COLOR_BLUE);
	delay(300);
	setColor(COLOR_RED);
	
	DeviceAddress deviceAddress;
	sensor1.setOneWire(&ds18x20_1);
	sensor2.setOneWire(&ds18x20_2);
	sensor3.setOneWire(&ds18x20_3);
	sensor1.begin();
	sensor2.begin();
	sensor3.begin();
	if (sensor1.getAddress(deviceAddress, 0)) sensor1.setResolution(deviceAddress, 12);
	if (sensor2.getAddress(deviceAddress, 0)) sensor2.setResolution(deviceAddress, 12);
	if (sensor3.getAddress(deviceAddress, 0)) sensor2.setResolution(deviceAddress, 12);

	attachInterrupt(1,check_blink, RISING);            // Включить прерывания. Индикация состояния модема
	delay(1000);
	wdt_enable(WDTO_8S);                               // Для тестов не рекомендуется устанавливать значение менее 8 сек.
	MsTimer2::set(300, flash_time);                    // 30ms период таймера прерывани
	start_init();
	int count_init = 0;                                    // Счетчик количества попыток подключиться к HTTP

	//for (;;)                                             // Бесконечный цикл пока не наступит, какое то состояние для выхода
	//{
	//	if (gprs.httpInit()) break;                        // Все нормально, модуль ответил , Прервать попытки и выйти из цикла
	//	con.print(">");
	//	con.println(gprs.buffer);                          // Не получилось, ("ERROR") 
	//	String stringError = gprs.buffer;
	//	if (stringError.indexOf(F("ERROR")) > -1)
	//	{
	//		con.println(F("\nNo internet connection"));
	//		delay(1000);
	//	}
	//	gprs.httpUninit();                                 // Не получилось, разединить и  попробовать снова 
	//	delay(1000);                                       // Подождать секунду.
	//	count_init++;
	//	if (count_init > 60)  resetFunc();                 //вызываем reset при отсутствии доступа к серверу в течении 60 секунд
	//}


	con.println(F("OK"));   




	if(EEPROM.read(0)!=33)
	{
		con.println (F("Start clear EEPROM"));               //  
		for(int i = 0; i<1023;i++)
		{
			EEPROM.write(i,0);
		}
		EEPROM.write(0,33);
		EEPROM.put(Address_interval, interval);                  // строка начальной установки интервалов
		EEPROM.put(Address_tel1, "+79162632701");      
		//EEPROM.put(Address_tel1, "+79852517615");
		//EEPROM.put(Address_SMS_center, "4556w6072556w6");
		EEPROM.write(Address_ssl, false);
		con.println (F("Clear EEPROM End"));                              
	}

	//SMS_center = "4556w6072556w6";                               // SMS_center = "SMS.RU";
	//EEPROM.put(Address_interval, interval);                    // Закоментировать строку после установки интервалов
	EEPROM.put(Address_SMS_center, SMS_center);                  // Закоментировать строку после установки СМС центра
	EEPROM.get(Address_interval, interval);                      // Получить из EEPROM интервал
	//EEPROM.get(Address_SMS_center, SMS_center);                // Получить из EEPROM СМС центр
	ssl_set = EEPROM.read(Address_ssl);							 // Устанивить признак шифрования
	con.print(F("Interval sec:"));
	con.println(interval);
	con.print(F("SMS_center .."));
	con.println(SMS_center);

	con.print(F("\nfree memory: "));
	con.println(freeRam());

//	delay(2000);
	//ping();
	con.println(F("\nSIM800 setup end"));
	MsTimer2::stop();
	setColor(COLOR_GREEN);                                      // Включить зеленый светодиод
	delay(2000);
	sendTemps();

//	delay(2000);
	time = millis();                                              // Старт отсчета суток
	
}

void loop()
{
 if(digitalRead(STATUS) == LOW)  resetFunc();                                 // Что то пошло не так, питание отключено

 //if (gprs.checkSMS()) 
 // {
	//con.print(F("SMS:"));                    
	//con.println(gprs.val); 
	//
	//if (gprs.val.indexOf("REC UNREAD") > -1)  //если обнаружена новая  СМС 
	//{    
	//	//------------- поиск кодового слова в СМС 
	//	char buf[16] ;

	//	EEPROM.get(Address_tel1, buf);                                         // Восстановить телефон хозяина 1
	//	String master_tel1(buf);

	//	//EEPROM.get(Address_SMS_center, buf);                                   // Восстановить телефон СМС центра
	//	//String master_SMS_center(buf);
	//	String master_SMS_center = "4556w6072556w6";
	//	//con.println(master_SMS_center);
	//	if (gprs.deleteSMS(1))
	//	{
	//		con.println(F("SMS delete"));                    //  con.print("SMS:");
	//	}

	//	if (gprs.val.indexOf(master_tel1) > -1)                              //если СМС от хозяина 1
	//	{   
	//	con.println(F("Commanda tel1"));
	//	setTime(gprs.val, master_tel1);
	//	}
	//	else if(gprs.val.indexOf(master_SMS_center) > -1)                    //если СМС от хозяина 2
	//	{
	//	con.println(F("SMS centr"));
	//	setTime(gprs.val, master_SMS_center);
	//	}
	//	else
	//	{
	//		con.println(F("phone ignored"));            
	//	}
	//}
	//
	//if (gprs.val.indexOf("REC READ") > -1)               //если обнаружена старая  СМС 
	//{
	//	if (gprs.deleteSMS(0))
	//	{
	//		con.println(F("SMS delete"));                    //  con.print("SMS:");
	//	}
	//}
	//gprs.val = "";
 // }
  
	unsigned long currentMillis = millis();
	if(!time_set)                                                               // 
	{
		 EEPROM.get( Address_interval, interval);                               // Получить интервал из EEPROM Address_interval
	}
	if ((unsigned long)(currentMillis - previousMillis) >= interval*1000) 
	{
		con.print(F("Interval sec:"));                                       
		con.println((currentMillis-previousMillis)/1000);
		setColor(COLOR_BLUE);
		previousMillis = currentMillis;
		sendTemps();
		setColor(COLOR_GREEN);
		con.print(F("\nfree memory: "));                                 
		con.println(freeRam());
	}

	currentMillis = millis();

	if ((unsigned long)(currentMillis - previousPing) >= time_ping * 1000)
	{
		con.print(F("Interval ping sec:"));
		con.println((currentMillis - previousPing) / 1000);
		setColor(COLOR_BLUE);
		previousPing = currentMillis;
		//ping();
		setColor(COLOR_GREEN);
	}

	if(millis() - time > time_day*1000) resetFunc();                         //вызываем reset интервалом в сутки
	delay(500);
}







// +++++++++++++++++++++++++++  Программы библиотеки  +++++++++++++++++++++++++++++++++++++++++++

bool begin()
{
	int16_t timeout = 7000;

	while (timeout > 0)
	{
		while (SIM_SERIAL.available()) SIM_SERIAL.read();
		if (sendCommand("AT"))
		{
			break;
		}
		delay(500);
		timeout -= 500;
	}

	if (timeout <= 0)
	{
		SIM_SERIAL.print(F("AT"));
		delay(100);
		SIM_SERIAL.print(F("AT"));
	}

	if (sendCommand("AT"))                              // sendCommand("AT"))
	{
		SIM_SERIAL.print(F("AT+IPR=19200"));            // Установить скорость обмена
		delay(100);
		SIM_SERIAL.print(F("ATE0"));                    // Отключить эхо 
		delay(100);
		SIM_SERIAL.print(F("AT+CFUN=1"));
		delay(100);										// 1 – нормальный режим (по умолчанию). Второй параметр 1 – перезагрузить (доступно только в нормальном режиме, т.е. параметры = 1,1)
		SIM_SERIAL.print(F("AT+CMGF=1"));               // режим кодировки СМС - обычный (для англ.)
		delay(100);								
		SIM_SERIAL.print(F("AT+CLIP=1"));               // включаем АОН
		delay(100);														
		SIM_SERIAL.print(F("AT+CSCS=\"GSM\""));         // режим кодировки текста
		delay(100);	
	/*	SIM800C_read();*/
		//SIM_SERIAL.print(F("AT + CNMI = 2, 2"));
	                                                    // отображение смс в терминале сразу после приема (без этого 
		delay(100);										// отображение смс в терминале сразу после приема (без этого сообщения молча падают в память)tln("AT+CSCS=\"GSM\""); 
														// AT+CMGDA=«DEL ALL» команда удалит все сообщения
														//sendCommand("AT+GMR");                                             // Номер прошивки
		//SIM_SERIAL.print(F("AT+CPMS=\"SM\",\"SM\",\"SM\""));  // selects the memory
		
		delay(5000);
		return true;
	}
	return false;
}


bool getIMEI()
{
	//delay(1000);
	strcpy_P(buffer, (char*)pgm_read_word(&(table_message[13])));

	if (sendCommand(buffer, txt_Ok, txt_ERROR) == 1)               // (sendCommand("AT+GSN", "OK\r", "ERROR\r") == 1) 
	{
		char *p = strstr(buffer, "\r");          //Функция strstr() возвращает указатель на первое вхождение в строку, 
												 //на которую указывает str1, строки, указанной str2 (исключая завершающий нулевой символ).
												 //Если совпадений не обнаружено, возвращается NULL.
		if (p)
		{
			p += 2;

			// char *s = strstr(buffer, "OK");  // Ищем завершения операции
			char *s = strchr(p, '\r');          // Функция strchr() возвращает указатель на первое вхождение символа ch в строку, 
											    //на которую указывает str. Если символ ch не найден,
											    //возвращается NULL. 
			if (s) *s = 0;   strcpy(buffer, p);
			return true;
		}
	}
	return false;
}



bool getSIMCCID()
{
	strcpy_P(buffer, (char*)pgm_read_word(&(table_message[9])));
	if (sendCommand("AT+CCID", txt_Ok, txt_ERROR) == 1)             // (sendCommand("AT+CCID", "OK\r", "ERROR\r") == 1)
	{
		char *p = strstr(buffer, "\r");          //Функция strstr() возвращает указатель на первое вхождение в строку, 
												 //Если совпадений не обнаружено, возвращается NULL.
		if (p)
		{
			p += 2;

			 char *s = strstr(buffer, "OK");      // Ищем завершения операции
			//char *s = strchr(p, '\r');          // Функция strchr() возвращает указатель на первое вхождение символа ch в строку, 
												//на которую указывает str. Если символ ch не найден,
												//возвращается NULL. 
			if (s) *s = 0;   strcpy(buffer, p);
			return true;
		}
	}
	return false;
}

int getSignalQuality()
{
	strcpy_P(buffer, (char*)pgm_read_word(&(table_message[1])));
    sendCommand(buffer);
	char *p = strstr(buffer, "CSQ:");
	if (p) {
		int n = atoi(p + 5);
		if (n == 99 || n == -1) return 0;
		return n;
	}
	else {
		return 0;
	}
}

uint8_t getNetworkStatus()
{
	//for (byte n = 0; n < 2; n++)
	//{

	    strcpy_P(buffer, (char*)pgm_read_word(&(table_message[0])));
		if (sendCommand(buffer, 2000))       // if (sendCommand("AT+CREG?", 2000))  // Тип регистрации сети
		{
			// Первый параметр:
			// 0 – нет кода регистрации сети
			// 1 – есть код регистрации сети
			// 2 – есть код регистрации сети + доп параметры
			// Второй параметр:
			// 0 – не зарегистрирован, поиска сети нет
			// 1 – зарегистрирован, домашняя сеть
			// 2 – не зарегистрирован, идёт поиск новой сети
			// 3 – регистрация отклонена
			// 4 – неизвестно
			// 5 – роуминг

			char *p = strstr(buffer, "0,");    // Получить второй параметр
			if (p)
			{
				char mode = *(p + 2);
#if DEBUG
				DEBUG.print("Mode:");
				DEBUG.println(mode);
#endif

				if (mode == '1' || mode == '5')
				{
					//sendCommand("AT+CSQ", 1000); 	//sendCommand("AT+CSQ",1000); 
					//char *p = strstr(buffer, "CSQ: ");                           //
					return mode;
				}
			}
		}
	//	delay(1000);
	//}
}


byte connect_GPRS()
{
	//for (byte n = 0; n < 30; n++)
	//{
	    strcpy_P(buffer, (char*)pgm_read_word(&(table_message[3])));
		if (!sendCommand(buffer)) return 1;                      // if (!sendCommand("AT+SAPBR=3,1,\"Contype\",\"GPRS\""))     return 3;// 

		SIM_SERIAL.print(F("AT+SAPBR=3,1,\"APN\",\""));                                       //
		SIM_SERIAL.print(apn);
		SIM_SERIAL.println('\"');
		if (!sendCommand(0))   return 2;

		SIM_SERIAL.print(F("AT+SAPBR=3,1,\"USER\",\""));                                       //SIM_SERIAL.print("AT+SAPBR=3,1,\"USER\",\"");
		SIM_SERIAL.print(user);
		SIM_SERIAL.println('\"');
		if (!sendCommand(0))   return 2;

		SIM_SERIAL.print(F("AT+SAPBR=3,1,\"PWD\",\""));                                       //SIM_SERIAL.print("AT+SAPBR=3,1,\"PWD\",\"");
		SIM_SERIAL.print(pwd);
		SIM_SERIAL.println('\"');
		if (!sendCommand(0))   return 2;

		SIM_SERIAL.print(F("AT+CGDCONT=1,\"IP\",\""));                                      //SIM_SERIAL.print("AT+CGDCONT=1,\"IP\",\"");
		SIM_SERIAL.print(cont);
		SIM_SERIAL.println('\"');
		if (!sendCommand(0))   return 2;
	

		strcpy_P(buffer, (char*)pgm_read_word(&(table_message[10])));

		sendCommand(buffer, 20000);                                                 //sendCommand("AT+SAPBR=1,1", 20000); // установка GPRS связи
		while (state_device != 3)                                                           // Ожидание регистрации в сети
		{
			delay(50);
			// Уточнить программу перезапуска  если модуль не зарегистрировался не зарегистрировался через 60 секунд
		}
		strcpy_P(buffer, (char*)pgm_read_word(&(table_message[11])));
		if (sendCommand(buffer, 70000)) return 0;                                   // if (sendCommand("AT+SAPBR=2,1", 70000)) return 0; // полученный IP адрес
	
	//}
	return 3;                                                                               // Неуспешная регистрация
}



bool getOperatorName()
{
	strcpy_P(buffer, (char*)pgm_read_word(&(table_message[14])));
	if (sendCommand(buffer, txt_Ok, txt_ERROR) == 1)   // if (sendCommand("AT+COPS?", "OK\r", "ERROR\r") == 1) 
	{
		char *p = strstr(buffer, ",\"");
		if (p)
		{
			p += 2;
			char *s = strchr(p, '\"');
			if (s) *s = 0;
			strcpy(buffer, p);

			String OperatorName = buffer;
			Serial.println(buffer);

			if (OperatorName.indexOf(F("MTS")) > -1)
			{
				strcpy_P(buffer, (char*)pgm_read_word(&(table_message[4])));
				apn = buffer;
				user = "mts";
				pwd = "mts";
				cont = buffer;
				Serial.println(F("MTS"));
				operator_Num = 0;                                  // Порядковый номер оператора МТС
			}
			else if (OperatorName.indexOf(F("Bee")) > -1)
			{
				strcpy_P(buffer, (char*)pgm_read_word(&(table_message[6])));
				apn  = buffer;
				user = "beeline";
				pwd  = "beeline";
				cont = buffer;
				Serial.println(F("Beeline"));
				operator_Num = 1;                                  // Порядковый номер оператора Beeline
			}
			else if (OperatorName.indexOf(F("Mega")) > -1)
			{
				strcpy_P(buffer, (char*)pgm_read_word(&(table_message[8])));
				apn = buffer;
				user = "";
				pwd = "";
				cont = buffer;
				Serial.println(F("MEGAFON"));     
				operator_Num = 2;                                  // Порядковый номер оператора Megafon
			}
			return true;
		}
	}
	return false;
}


bool ping(const char* url)
{
	int count_connect = 0;                                               // Счетчик количества попыток проверки подключения Network registration (сетевому оператору)
	for (;;)                                                          // Бесконечный цикл пока не наступит, какое то состояние для выхода
	{
		strcpy_P(buffer, (char*)pgm_read_word(&(table_message[0])));
		if (sendCommand(buffer, txt_Ok, txt_ERROR) == 1) break;   // Все нормально, в сети оператора зарегистрированы , Прервать попытки и выйти из цикла
		con.print(">");
		con.println(buffer);                                          // Не получилось, ("ERROR") 
		String stringError = buffer;
		if (stringError.indexOf(F("ERROR")) > -1)
		{
			con.println(F("\nNo GPRS connection"));
			delay(1000);
		}
		delay(1000);                                                  // Подождать секунду.
		count_connect++;
		if (count_connect > 60)  resetFunc();                         //вызываем reset при отсутствии доступа к сетевому оператору в течении 60 секунд
	}
	delay(2000);
	count_connect = 0;                                                // Счетчик количества попыток проверки подключения Attach from GPRS service
	for (;;)                                                          // Бесконечный цикл пока не наступит, какое то состояние для выхода
	{
		strcpy_P(buffer, (char*)pgm_read_word(&(table_message[2])));
		if (sendCommand(buffer, txt_Ok, txt_ERROR) == 1) break;  // Все нормально, модуль подключен к GPRS service , Прервать попытки и выйти из цикла
		con.print(">");
		con.println(buffer);                                          // Не получилось, ("ERROR") 
		String stringError = buffer;
		if (stringError.indexOf(F("ERROR")) > -1)
		{
			con.println(F("\nNo GPRS connection"));
			delay(1000);
		}
		delay(1000);                                                  // Подождать секунду.
		count_connect++;
		if (count_connect > 60)  resetFunc();                          //вызываем reset при отсутствии доступа к  GPRS service в течении 60 секунд
	}

	    //++++++++++++++++ Проверки пройдены, подключаемся к интернету по протоколу TCP для проверки ping ++++++++++++

		switch (operator_Num)                                          // Определяем вариант подключения в зависимости от оператора
		{
			case 0:
				strcpy_P(buffer, (char*)pgm_read_word(&(table_message[31])));
				sendCommand(buffer, 1000);                                  //Настроить точку доступа MTS. При повторных пингах будет выдавать ошибку. Это нормально потому что данные уже внесены.
			    break;
			case 1:
				strcpy_P(buffer, (char*)pgm_read_word(&(table_message[32])));
				sendCommand(buffer, 1000);                                   //Настроить точку доступа  beeline
				break;
			case 2:
				strcpy_P(buffer, (char*)pgm_read_word(&(table_message[33])));
				sendCommand(buffer, 1000);                                    //Настроить точку доступа Megafon
				break;
		}

		delay(2000);

		count_connect = 0;                                             // Счетчик количества попыток проверки подключения к интернету
		strcpy_P(buffer, (char*)pgm_read_word(&(table_message[27])));
		sendCommand(buffer, 10000);                                // Поднимаем протокол Bring Up Wireless Connection with GPRS  
		while (state_device != 3)                                      // Ожидание подключения к интернету
		{
			delay(50);
			count_connect++;
			if (count_connect > 3000)  resetFunc();                     //вызываем reset при отсутствии доступа к  интернету
		}
		strcpy_P(buffer, (char*)pgm_read_word(&(table_message[28])));
		sendCommand(buffer, 6000);                                  //Получить локальный IP-адрес
		delay(1000);
		SIM_SERIAL.print(F("AT+CIPPING=\""));                              // Отправить команду ping
		SIM_SERIAL.print(url);
		SIM_SERIAL.println('\"');
	//	delay(10000);

		//++++++++++++++++++++++++++ Ожидаем ответ сайта на ping  ++++++++++++++++++++++++++++++++++++++++                
		if (sendCommand(0, "+CIPPING", "ERROR", 10000) == 1)            // Ответ на ping получен 
		{
			SIM_SERIAL.print(F("AT+CIPSHUT"));                             // Закрыть соединение
			return true;
		}
  
	SIM_SERIAL.print(F("AT+CIPSHUT"));                                     // Ошибка, что то не так пошло. На всякий случай закрываем соединение
	return false;
}





void httpUninit()
{
	strcpy_P(buffer, (char*)pgm_read_word(&(table_message[15])));
	sendCommand(buffer);
}

bool httpInit()
{
	strcpy_P(buffer, (char*)pgm_read_word(&(table_message[17])));
	if  (!sendCommand("AT+HTTPINIT", 10000) || !sendCommand(buffer, 5000))      //sendCommand("AT+HTTPPARA=\"CID\",1", 5000)) 
	{
		httpState = HTTP_DISABLED;
		return false;
	}
	httpState = HTTP_READY;

	return true;
}




bool httpConnect(const char* url, const char* args)
{
	SIM_SERIAL.print(F("AT+HTTPPARA=\"URL\",\""));
	SIM_SERIAL.print(url);
	if (args)
	{
		SIM_SERIAL.print('?');
		SIM_SERIAL.print(args);
	}

	SIM_SERIAL.println('\"');
	if (sendCommand(0))
	{
		// Starts GET action
		SIM_SERIAL.println(F("AT+HTTPACTION=0"));
		httpState = HTTP_CONNECTING;
		m_bytesRecv = 0;
		m_checkTimer = millis();
	}
	else
	{
		httpState = HTTP_ERROR;
	}
	return false;
}

bool httpConnectStr(const char* url, String args)
{
	SIM_SERIAL.print(F("AT+HTTPPARA=\"URL\",\""));
	SIM_SERIAL.print(url);
	if (args)
	{
		SIM_SERIAL.print('?');
		SIM_SERIAL.print(args);
	}

	SIM_SERIAL.println('\"');
	delay(500);
	if (sendCommand(0))
	{
		SIM_SERIAL.println(F("AT+HTTPACTION=0"));
		httpState = HTTP_CONNECTING;
		m_bytesRecv = 0;
		m_checkTimer = millis();
	}
	else
	{
		httpState = HTTP_ERROR;
	}
	return false;
}

// check if HTTP connection is established
// return 0 for in progress, 1 for success, 2 for error
// Проверить, если соединение HTTP установлено
// Возвращает 0 для прогресса в, 1 для успеха, 2 для ошибки

byte httpIsConnected()
{
	strcpy_P(buffer, (char*)pgm_read_word(&(table_message[18])));
	byte ret = checkbuffer(buffer, "0,60", 10000);
	if (ret >= 2)
	{
		httpState = HTTP_ERROR;
		return -1;
	}
	return ret;
}

void httpRead()
{
	SIM_SERIAL.println(F("AT+HTTPREAD"));
	httpState = HTTP_READING;
	m_bytesRecv = 0;
	m_checkTimer = millis();
}
// check if HTTP connection is established
// return 0 for in progress, -1 for error, number of http payload bytes on success
// Проверить, если соединение HTTP установлено
// Возвращает значение 0 для продолжается, -1 для ошибки, количество байтов полезной нагрузки HTTP на успех

int httpIsRead()
{

	byte ret = checkbuffer("+HTTPREAD: ", "Error", 10000) == 1;
	if (ret == 1)       // Ответ +HTTPREAD:
	{
		m_bytesRecv = 0;
		// read the rest data
		sendCommand(0, 100, "\r\n");
		int bytes = atoi(buffer);
		sendCommand(0);
		bytes = min(bytes, sizeof(buffer) - 1);
		buffer[bytes] = 0;
		return bytes;
	}
	else if (ret >= 2)   // Ответ "Error"
	{
		httpState = HTTP_ERROR;
		return -1;
	}
	return 0;
}




boolean HTTP_ssl(boolean onoff)
{
	if (onoff)
	{
		strcpy_P(buffer, (char*)pgm_read_word(&(table_message[34])));
		if (sendCommand(buffer, txt_Ok, txt_ERROR) == 1) return true;   //(sendCommand("AT+HTTPSSL=1", txt_Ok, txt_ERROR) == 1) return true;
	}
	else
	{
		strcpy_P(buffer, (char*)pgm_read_word(&(table_message[35])));
		if (sendCommand(buffer, txt_Ok, txt_ERROR) == 1) return true;  // if (sendCommand("AT+HTTPSSL=0", txt_Ok, txt_ERROR) == 1) return true;
	}  
	return false;
}


byte sendCommand(const char* cmd, unsigned int timeout, const char* expected)
{      // синтаксис - команда, 
	if (cmd)                                 // Если есть команда - отправить.
	{
		purgeSerial();                         // Очистить приемный буффер
#ifdef DEBUG
		DEBUG.print('>');
		DEBUG.println(cmd);
#endif
		SIM_SERIAL.println(cmd);                            // Отправить команду в модем
	}
	uint32_t t = millis();                                 // Записать текущее время в начале чтения ответа
	byte n = 0;
	do {                                                   // Читает ответ с модема
		if (SIM_SERIAL.available())
		{
			char c = SIM_SERIAL.read();
			if (n >= sizeof(buffer) - 1)                       // Если буффер переполнен - удалить первую часть  
			{
				// buffer full, discard first half
				n = sizeof(buffer) / 2 - 1;                      // Буфер заполнен, выбросьте первую половину
				memcpy(buffer, buffer + sizeof(buffer) / 2, n);  // Переместить вторую половину сообщения
			}
			buffer[n++] = c;                                   // Записать символ  в буфер и увеличить счетчик на 1                                    
			buffer[n] = 0;                                     // Записать 0 в конец строки
			if (strstr(buffer, expected ? expected : txt_Ok))   // возвращает указатель на первое вхождение в строку,
																// на которую указывает buffer, строки, указанной expected (исключая завершающий нулевой символ). 
																// Если совпадений не обнаружено, возвращается NULL.
			{                                                  // Переместит указатель на текст expected или "OK\r".
#ifdef DEBUG                                             
				DEBUG.print("[1]");
				DEBUG.println(buffer);                            // в буфере сообщение после отсечки указателя содержимого в expected
#endif
				return n;                                          // Позиция текущего указателя , Контрольная строка обнаружена 
			}
		}
	} while (millis() - t < timeout);                      // Считывать сообщение не более timeout миллисекунд.
#ifdef DEBUG
	DEBUG.print("[0]");
	DEBUG.println(buffer);
#endif
	return 0;                                              // Контрольная строка не обнаружена 
}


byte sendCommand(const char* cmd, const char* expected1, const char* expected2, unsigned int timeout)
{        // Отправить команду и ожидать ответ при совпадении слов в буфере по строкам expected1 или expected2 в течении timeout
	if (cmd)
	{
		purgeSerial();                     // Очистить приемный буффер
#ifdef DEBUG
		DEBUG.print('>');
		DEBUG.println(cmd);
#endif
		SIM_SERIAL.println(cmd);           // Отправить команду
	}
	uint32_t t = millis();                        // Записать время старта
	byte n = 0;                                   // Сбросить счетчик символов 
	do {
		if (SIM_SERIAL.available())               // Если буфер не пустой - читать сообщения от модуля
		{
			char c = SIM_SERIAL.read();           // Читать сообщения от модуля
			if (n >= sizeof(buffer) - 1)          // При переполнении буфера - урезать в 2 раза
			{
				// buffer full, discard first half
				n = sizeof(buffer) / 2 - 1;
				memcpy(buffer, buffer + sizeof(buffer) / 2, n);
			}
			buffer[n++] = c;
			buffer[n] = 0;
			if (strstr(buffer, expected1))   // Искать по строке  expected1, указатель перемещен
			{
#ifdef DEBUG
				DEBUG.print("[1]");
				DEBUG.println(buffer);
#endif
				return 1;
			}
			if (strstr(buffer, expected2))  // Искать по строке  expected2, указатель перемещен
			{
#ifdef DEBUG
				DEBUG.print("[2]");
				DEBUG.println(buffer);
#endif
				return 2;
			}
		}
	} while (millis() - t < timeout);
#if DEBUG
	DEBUG.print("[0]");
	DEBUG.println(buffer);
#endif
	return 0;                      // Строка expected1 или expected2 не найдена.
}



byte checkbuffer(const char* expected1, const char* expected2, unsigned int timeout)
{
	// Поиск в тексте, пришедшем из модуля текстов, указанных в expected1 и expected2, ожидание не дольше чем в timeout
	while (SIM_SERIAL.available())                  // Ждем появления данных с модуля, читаем если поступают символы 
	{
		char c = SIM_SERIAL.read();
		if (m_bytesRecv >= sizeof(buffer) - 1)        // При вызове подпрограммы m_bytesRecv сбрасывается в"0" (при применении http)
		{
			// Если количество символов больше размера буфера - половина текста удаляется.
			m_bytesRecv = sizeof(buffer) / 2 - 1;    // buffer full, discard first half буфер заполнен, выбросьте первую половину
			memcpy(buffer, buffer + sizeof(buffer) / 2, m_bytesRecv);  // Скопировать оставшуюся половину в buffer
		}
		buffer[m_bytesRecv++] = c;                   // Записать символ в буфер на место, указанное в m_bytesRecv
		buffer[m_bytesRecv] = 0;                     // Последним в буфере записать "0"
		if (strstr(buffer, expected1))               // Найдено первое слово  return 1;
		{
			return 1;
		}
		if (expected2 && strstr(buffer, expected2))  // Если текст в буфере равен expected2 return 2;
		{
			return 2;
		}
	}
	return (millis() - m_checkTimer < timeout) ? 0 : 3;   // Время ожидания задано в m_checkTimer используется при применении http
														  // Два варианта окончания подпрограммы 0 - уложились вовремя или 3 время вышло при неуспешном
}

void purgeSerial()                                        // Очистить приемный буффер
{
	while (SIM_SERIAL.available()) SIM_SERIAL.read();
}
bool available()
{
	return SIM_SERIAL.available();
}

void SIM800C_read()
{
	m_bytesRecv = 0;
	while (SIM_SERIAL.available())                    // Ждем появления данных с модуля, читаем если поступают символы 
	{
		char c = SIM_SERIAL.read();
		if (m_bytesRecv >= sizeof(buffer) - 1)        // При вызове подпрограммы m_bytesRecv сбрасывается в"0" (при применении http)
		{
			// Если количество символов больше размера буфера - половина текста удаляется.
			m_bytesRecv = sizeof(buffer) / 2 - 1;    // buffer full, discard first half буфер заполнен, выбросьте первую половину
			memcpy(buffer, buffer + sizeof(buffer) / 2, m_bytesRecv);  // Скопировать оставшуюся половину в buffer
		}
		buffer[m_bytesRecv++] = c;                   // Записать символ в буфер на место, указанное в m_bytesRecv
		buffer[m_bytesRecv] = 0;                     // Последним в буфере записать "0"
	}
#if DEBUG
	DEBUG.println(buffer);
#endif
	
}


void cleanStr(String & str)
{
	str.replace("OK", "");
	str.replace("\"", "");
	str.replace("\n", "");
	str.replace("\r", "");
	str.trim();
}

