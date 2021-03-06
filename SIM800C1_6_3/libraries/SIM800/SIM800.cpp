/*************************************************************************
* SIM800 GPRS/HTTP Library
* Distributed under GPL v2.0
* Written by Stanley Huang <stanleyhuangyc@gmail.com>
* For more information, please visit http://arduinodev.com
*************************************************************************/

#include "SIM800.h"
#include <SoftwareSerial.h>

//void(*resetFunc) (void) = 0;                           // ��������� ������� reset

bool CGPRS_SIM800::begin(Stream &port)
{
	SIM_SERIAL = &port;
	int16_t timeout = 7000;

	while (timeout > 0)
	{
		while (SIM_SERIAL->available()) SIM_SERIAL->read();
		if (sendCommandS(F("AT")))
		{
			break;
		}
		delay(500);
		timeout -= 500;
	}

	if (timeout <= 0)
	{
		sendCommandS(F("AT"));   
		delay(100);
		sendCommandS(F("AT"));              
		delay(100);
		sendCommandS(F("AT"));               
		delay(100);
	}
	
	if (sendCommandS(F("AT")))               
	{
		sendCommandS(F("AT+IPR=19200"));                	 // ���������� �������� ������
		delay(100);
		sendCommandS(F("ATE0"));                             // ��������� ��� 
		delay(100);
		sendCommandS(F("AT+CFUN=1"));                        // 1 � ���������� ����� (�� ���������). ������ �������� 1 � ������������� (�������� ������ � ���������� ������, �.�. ��������� = 1,1)
		delay(100);																
		sendCommandS(F("AT+CMGF=1"));                        // ����� ��������� ��� - ������� (��� ����.)
		delay(100);														
		sendCommandS(F("AT+CLIP=1"));                        // �������� ���
		delay(100);													
		sendCommandS(F("AT+CSCS=\"GSM\""));                  // ����� ��������� ������
		delay(100);															
		//sendCommandS(F("AT+CNMI=2,2"));                    // ����������� ��� � ��������� ����� ����� ������ (��� ����� ��������� ����� ������ � ������)tln("AT+CSCS=\"GSM\""); 
		delay(100);									 
		//sendCommandS(F("AT+CMGDA=\"DEL ALL\""));           // AT+CMGDA=�DEL ALL� ������� ������ ��� ���������
		//delay(100);
		//sendCommandS(F("AT+GMR"));                         // ����� ��������
		//delay(100);
		sendCommandS(F("AT + CMGF = 1"));                    // sets the SMS mode to text
		//sendCommandS(F("AT+CPMS=\"SM\",\"SM\",\"SM\""));   // selects the memory
		return true;
	}
	return false;
}


byte CGPRS_SIM800::connect_GPRS()
{
	for (byte n = 0; n < 10; n++)
	{
		if (!sendCommandS(F("AT+SAPBR=3,1,\"Contype\",\"GPRS\""))) return 1;           

		SIM_SERIAL->print(F("AT+SAPBR=3,1,\"APN\",\""));
		SIM_SERIAL->print(apn);
		SIM_SERIAL->println('\"');
		if (!sendCommandS(no))   return 2;   

		SIM_SERIAL->print(F("AT+SAPBR=3,1,\"USER\",\""));
		SIM_SERIAL->print(user);
		SIM_SERIAL->println('\"');
		if (!sendCommandS(no))   return 2;

		SIM_SERIAL->print(F("AT+SAPBR=3,1,\"PWD\",\""));
		SIM_SERIAL->print(pwd);
		SIM_SERIAL->println('\"');
		if (!sendCommandS(no))   return 2;

		SIM_SERIAL->print(F("AT+CGDCONT=1,\"IP\",\""));
		SIM_SERIAL->print(cont);
		SIM_SERIAL->println('\"');
		if (!sendCommandS(no))   return 2;

		timeout = 15000;
		Serial.print(F("Connect GPRS..")); Serial.println(n+1);
		sendCommandS(F("AT+SAPBR=1,1"));  timeout = 2000;   return 0;                 // ��������� GPRS �����
	}
	return 3;                                                                         // ���������� �����������
}

bool CGPRS_SIM800::connect_IP_GPRS()
{
	for (byte n = 0; n < 30; n++)
	{
		timeout = 15000;
		if (sendCommandS(F("AT+SAPBR=2,1"))) timeout = 2000;  return true;                             // �������� IP �����
		delay(500);
	}
	return false;
}


void CGPRS_SIM800::cleanStr(String & str) 
{
  str.replace("OK", "");
  str.replace("\"", "");
  str.replace("\n", "");
  str.replace("\r", "");
  str.trim();
}

uint8_t CGPRS_SIM800::getNetworkStatus()
{
	for (byte n = 0; n < 30; n++)
	{
		if (sendCommandS(F("AT+CREG?")))                       // ��� ����������� ����
		{
			// ������ ��������:
			// 0 � ��� ���� ����������� ����
			// 1 � ���� ��� ����������� ����
			// 2 � ���� ��� ����������� ���� + ��� ���������
			// ������ ��������:
			// 0 � �� ���������������, ������ ���� ���
			// 1 � ���������������, �������� ����
			// 2 � �� ���������������, ��� ����� ����� ����
			// 3 � ����������� ���������
			// 4 � ����������
			// 5 � �������
		
			char *p = strstr(buffer, "0,");                        // �������� ������ ��������
			if (p)
			{
				char mode = *(p + 2);
#if DEBUG
				DEBUG.print("Mode:");
				DEBUG.println(mode);
#endif
				return mode;
			}
		}
		delay(1000);
	}
}


bool CGPRS_SIM800::getIMEI()
{
  if (sendCommandS(F("AT+GSN")) == 1)               // (sendCommand("AT+GSN", "OK\r", "ERROR\r") == 1) 
  {
	char *p = strstr(buffer, "\r");          //������� strstr() ���������� ��������� �� ������ ��������� � ������, 
											 //�� ������� ��������� str1, ������, ��������� str2 (�������� ����������� ������� ������).
											 //���� ���������� �� ����������, ������������ NULL.
	  if (p) 
	  {
		p += 2;
		 char *s = strchr(p, '\r');         // ������� strchr() ���������� ��������� �� ������ ��������� ������� ch � ������, 
											//�� ������� ��������� str. ���� ������ ch �� ������,
											//������������ NULL. 
		 if (s) *s = 0;   strcpy(buffer, p);
		 return true;
	  }
  }
  return false;
}
bool CGPRS_SIM800::getSIMCCID()
{
	if (sendCommandS(F("AT+CCID")) == 1)             // (sendCommand("AT+CCID", "OK\r", "ERROR\r") == 1)
	{      
		char *p = strstr(buffer, "\r");              //������� strstr() ���������� ��������� �� ������ ��������� � ������, 
												     //���� ���������� �� ����������, ������������ NULL.
		if (p)
		{
			p += 2;
			char *s = strchr(p, '\r');          // ������� strchr() ���������� ��������� �� ������ ��������� ������� ch � ������, 
												//�� ������� ��������� str. ���� ������ ch �� ������,
												//������������ NULL. 
			if (s) *s = 0;   strcpy(buffer, p);
			int i=0;

			for (i=0;i<120;i++)
			{
				if (buffer[i] == "f")  break;
				if (buffer[i] >= '0'&&buffer[i] <= '9') // ���� ����� ������� - �� ������� ��
				{
					buffer1[i] = buffer[i];
					//Serial.print(buffer1[i]);
				}
			}
			return true;
		}
	}
	return false;
}


bool CGPRS_SIM800::getOperatorName()
{
  // display operator name
  if (sendCommandS(F("AT+COPS?")) == 1)   
  {
	  char *p = strstr(buffer, ",\"");
	  if (p)
	  {
		  p += 2;
		  char *s = strchr(p, '\"');
		  if (s) *s = 0;
		  strcpy(buffer, p);

		  String OperatorName = buffer;
		  if (OperatorName.indexOf(F("MTS")) > -1)
		  {
			  apn  = F("internet.mts.ru");
			  user = F("mts");
			  pwd  = F("mts");
			  cont = F("internet.mts.ru");
			  Serial.println(F("MTS"));
			  operator_Num = 0;                                  // ���������� ����� ��������� ���
		  }
		  else if (OperatorName.indexOf(F("Bee")) > -1)
		  {
			  apn  = F("internet.beeline.ru");
			  user = F("beeline");
			  pwd  = F("beeline");
			  cont = F("internet.beeline.ru");
			  Serial.println(F("Beeline"));
			  operator_Num = 1;                                  // ���������� ����� ��������� Beeline
		  }
		  else if (OperatorName.indexOf(F("Mega")) > -1)
		  {
			  apn  = F("internet");
			  user = "";
			  pwd  = "";
			  cont = F("internet");
			  Serial.println(F("MEGAFON"));
			  operator_Num = 2;                                  // ���������� ����� ��������� Megafon
		  }
		  Serial.println(apn);
		  return true;
	  }
  }
  return false;
}


byte CGPRS_SIM800::ping_connect_internet()
{
	int count_connect = 0;                                                // ������� ���������� ������� �������� ����������� Network registration (�������� ���������)
	for (;;)                                                              // ����������� ���� ���� �� ��������, ����� �� ��������� ��� ������
	{
		if (sendCommandS(F("AT+CREG?")) == 1) break;                      // ��� ���������, � ���� ��������� ���������������� , �������� ������� � ����� �� �����
		Serial.print(">");
		Serial.println(buffer);                                           // �� ����������, ("ERROR") 
		String stringError = buffer;
		if (stringError.indexOf(F("ERROR")) > -1)
		{
			Serial.println(F("\nNo GPRS connection"));
			delay(1000);
		}
		delay(1000);                                                     // ��������� �������.
		count_connect++;
		if (count_connect > 60)  resetFunc(); //break;                                   //�������� reset ��� ���������� ������� � �������� ��������� � ������� 60 ������
	}
	delay(1000);
	count_connect = 0;                                                 // ������� ���������� ������� �������� ����������� Attach from GPRS service
	for (;;)                                                           // ����������� ���� ���� �� ��������, ����� �� ��������� ��� ������
	{
		if (sendCommandS(F("AT+CGATT?")) == 1) break;                  // ��� ���������, ������ ��������� � GPRS service , �������� ������� � ����� �� �����
		Serial.print(F(">"));
		Serial.println(buffer);                                          // �� ����������, ("ERROR") 
		String stringError = buffer;
		if (stringError.indexOf(F("ERROR")) > -1)
		{
			Serial.println(F("\nNo GPRS connection"));
			delay(1000);
		}
		delay(1000);                                                     // ��������� �������.
		count_connect++;
		if (count_connect > 60)  resetFunc(); //break;  // resetFunc();                  //�������� reset ��� ���������� ������� �  GPRS service � ������� 60 ������
	}

	//++++++++++++++++ �������� ��������, ������������ � ��������� �� ��������� TCP ��� �������� ping ++++++++++++

	switch (operator_Num)                                                  // ���������� ������� ����������� � ����������� �� ���������
	{
	case 0:
		sendCommandS(F("AT+CSTT=\"internet.mts.ru\""));                    //��������� ����� ������� MTS. ��� ��������� ������ ����� �������� ������. ��� ��������� ������ ��� ������ ��� �������.
		break;
	case 1:
		sendCommandS(F("AT+CSTT=\"internet.beeline.ru\""));                //��������� ����� �������  beeline
		break;
	case 2:
		sendCommandS(F("AT+CSTT=\"internet\""));                           //��������� ����� ������� Megafon
		break;
	}
	delay(1000);
	timeout = 10000;
	sendCommandS(F("AT+CIICR"));                                           // ��������� �������� Bring Up Wireless Connection with GPRS  
	timeout = 2000;
	return 0;
}


bool CGPRS_SIM800::ping(const char* url)
{
	sendCommandS(F("AT+CIFSR"));                                          //�������� ��������� IP-�����
	delay(1000);
	SIM_SERIAL->print(F("AT+CIPPING=\""));                                // ��������� ������� ping
	SIM_SERIAL->print(url);
	SIM_SERIAL->println('\"');
	delay(5000);

	//++++++++++++++++++++++++++ ������� ����� ����� �� ping  ++++++++++++++++++++++++++++++++++++++++   
	 expected1 = "+CIPPING";
	 expected2 = ERROR_r;
	 timeout   = 10000;
	if (sendCommandS(no) == 1)            // ����� �� ping ������� 
	{
		SIM_SERIAL->print(F("AT+CIPSHUT"));                           // ������� ����������
		expected1 = OK_r;
		expected2 = ERROR_r;
		timeout = 2000;
		return true;
	}
	expected1 = OK_r;
	expected2 = ERROR_r;
	timeout = 2000;
	SIM_SERIAL->print(F("AT+CIPSHUT"));                             // ������, ��� �� �� ��� �����. �� ������ ������ ��������� ����������
	return false;
}

bool CGPRS_SIM800::checkSMS()
{
	expected1 = "+CMGR:";
	expected2 = ERROR_r;

	if (sendCommandS(F("AT+CMGR=1")) == 1)                            //  ���������� ������� "AT+CMGR=1", ����� ��������� ��������� +CMGR:
	{ 
		while (SIM_SERIAL->available())                                //���� ������ �� GSM ������
		{
			ch = SIM_SERIAL->read();
			val += char(ch);                                           //��������� ������� ������ � ���������� val
			delay(10);
		}
		expected1 = OK_r;
		expected2 = ERROR_r;
		return true;
	}
	 expected1 = OK_r;
	 expected2 = ERROR_r;
	 return false; 
}

bool CGPRS_SIM800::deleteSMS(int n_sms)
{

	if (n_sms > 0)
	{
		if(sendCommandS(F("AT+CMGD=1")) == 1)                      //  remove the SMS
		return true;
	}
	else
	{
		if(sendCommandS(F("AT+CMGDA=\"DEL ALL\"")) == 1)          // remove the SMS
		return true;
	}
	return false;
}

int CGPRS_SIM800::getSignalQuality()
{
  sendCommandS(F("AT+CSQ"));                                      // ������� �������
  char *p = strstr(buffer, "CSQ:");
  if (p) {
	int n = atoi(p+5);
	if (n == 99 || n == -1) return 0;
	return n ;
  } else {
   return 0; 
  }
}


void CGPRS_SIM800::httpUninit()
{
  sendCommandS(F("AT+HTTPTERM"));          
}

bool CGPRS_SIM800::httpInit()
{
	timeout = 10000;
	if  (!sendCommandS(F("AT+HTTPINIT")) || !sendCommandS(F("AT+HTTPPARA=\"CID\",1")))  //if  (!sendCommand("AT+HTTPINIT", 10000) || !sendCommand("AT+HTTPPARA=\"CID\",1", 5000)) 
	{
		httpState = HTTP_DISABLED;
		timeout = 2000;
		return false;
	}
	httpState = HTTP_READY;
	timeout = 2000;
	return true;
}

bool CGPRS_SIM800::httpConnect(const char* url, const char* args)
{
	SIM_SERIAL->print(F("AT+HTTPPARA=\"URL\",\""));                    
	SIM_SERIAL->print(url);
	if (args) 
	{
		SIM_SERIAL->print('?');
		SIM_SERIAL->print(args);
	}

	SIM_SERIAL->println('\"');
	if (sendCommandS(no))
	{
		// Starts GET action
		SIM_SERIAL->println(F("AT+HTTPACTION=0"));                         
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

bool CGPRS_SIM800::httpConnectStr(const char* url, String args)
{
	SIM_SERIAL->print(F("AT+HTTPPARA=\"URL\",\""));   
	SIM_SERIAL->print(url);
	if (args) 
	{
		SIM_SERIAL->print('?');
		SIM_SERIAL->print(args);
	}

	SIM_SERIAL->println('\"');
	delay(500);
	if (sendCommandS(no))
	{
		SIM_SERIAL->println(F("AT+HTTPACTION=0"));              
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
// ���������, ���� ���������� HTTP �����������
// ���������� 0 - � ������, 1 ��� �������, 2 ��� ������

byte CGPRS_SIM800::httpIsConnected()
{
	byte ret = checkbuffer("0,200", "0,60", 15000);           
	if (ret >= 2) 
	{
		httpState = HTTP_ERROR;
		return -1;
	}
	return ret;
}

void CGPRS_SIM800::httpRead()
{
	SIM_SERIAL->println(F("AT+HTTPREAD"));     
	httpState = HTTP_READING;
	m_bytesRecv = 0;
	m_checkTimer = millis();
}
// check if HTTP connection is established
// return 0 for in progress, -1 for error, number of http payload bytes on success
// ���������, ���� ���������� HTTP �����������
// ���������� �������� 0 ��� ������������, -1 ��� ������, ���������� ������ �������� �������� HTTP �� �����

int CGPRS_SIM800::httpIsRead()
{
	byte ret = checkbuffer("+HTTPREAD: ", ERROR_r, 10000) == 1;
	if (ret == 1)                  // ����� +HTTPREAD:
	{
		m_bytesRecv = 0;
		sendCommandS(no, 100, "\r\n");
		int bytes = atoi(buffer);
		sendCommandS(no);
		bytes = min(bytes, sizeof(buffer) - 1);
		buffer[bytes] = 0;
		return bytes;
	} else if (ret >= 2)           // ����� "Error"
	{
		httpState = HTTP_ERROR;
		return -1;
	}
	return 0;  
}

boolean CGPRS_SIM800::HTTP_ssl(boolean onoff) 
{
	timeout  = 4000;
	if (onoff)
	{
		if(sendCommandS(F("AT+HTTPSSL=1")) == 1)timeout  = 2000; return true;
	}
	else
	{
		if (sendCommandS(F("AT+HTTPSSL=0")) == 1) timeout  = 2000; return true;
	}
	timeout  = 2000; 
	return false;           
}

byte CGPRS_SIM800::sendCommandS(String cmd, unsigned int timeout, const char* expected)
{   
	if (cmd!= "no")                                              // ���� ���� ������� - ���������.
	{
		purgeSerial();                                           // �������� �������� ������
#ifdef DEBUG
		DEBUG.print('>');
		DEBUG.println(cmd);
#endif
		SIM_SERIAL->println(cmd);                                // ��������� ������� � �����
	}
	uint32_t t = millis();                                       // �������� ������� ����� � ������ ������ ������
	byte n = 0;
	do {                                                         // ������ ����� � ������
		if (SIM_SERIAL->available())
		{
			char c = SIM_SERIAL->read();
			if (n >= sizeof(buffer) - 1)                        // ���� ������ ���������� - ������� ������ �����  
			{ 
				// buffer full, discard first half
				n = sizeof(buffer) / 2 - 1;                      // ����� ��������, ��������� ������ ��������
				memcpy(buffer, buffer + sizeof(buffer) / 2, n);  // ����������� ������ �������� ���������
			}
			buffer[n++] = c;                                   // �������� ������  � ����� � ��������� ������� �� 1                                    
			buffer[n] = 0;                                     // �������� 0 � ����� ������
			if (strstr(buffer, expected ? expected : "OK\r"))   // ���������� ��������� �� ������ ��������� � ������,
																// �� ������� ��������� buffer, ������, ��������� expected (�������� ����������� ������� ������). 
																// ���� ���������� �� ����������, ������������ NULL.
			{                                                  // ���������� ��������� �� ����� expected ��� "OK\r".
#ifdef DEBUG                                             
				DEBUG.print("[1]");
				DEBUG.println(buffer);                            // � ������ ��������� ����� ������� ��������� ����������� � expected
#endif
				return n;                                          // ������� �������� ��������� , ����������� ������ ���������� 
			}
		}
	} while (millis() - t < timeout);                      // ��������� ��������� �� ����� timeout �����������.
#ifdef DEBUG
	DEBUG.print("[0]");
	DEBUG.println(buffer);
#endif
	return 0;                                              // ����������� ������ �� ���������� 
}
byte CGPRS_SIM800::sendCommandS(String cmd)
{     
	if (cmd != "no")                                      // ��������� ������� � ������� ����� ��� ���������� ���� � ������ �� ������� expected1 ��� expected2 � ������� timeout
	{
		purgeSerial();                                    // �������� �������� ������
#ifdef DEBUG
		DEBUG.print('>');
		DEBUG.println(cmd);
#endif
		SIM_SERIAL->println(cmd);                         // ��������� �������
	}
	uint32_t t = millis();                                // �������� ����� ������
	byte n = 0;                                           // �������� ������� �������� 
	do {
		if (SIM_SERIAL->available())                      // ���� ����� �� ������ - ������ ��������� �� ������
		{
			char c = SIM_SERIAL->read();                  // ������ ��������� �� ������
			if (n >= sizeof(buffer) - 1)                  // ��� ������������ ������ - ������� � 2 ����
			{
				n = sizeof(buffer) / 2 - 1;
				memcpy(buffer, buffer + sizeof(buffer) / 2, n);
			}
			buffer[n++] = c;
			buffer[n] = 0;
			if (strstr(buffer, expected1))                // ������ �� ������  expected1, ��������� ���������
			{
#ifdef DEBUG
				DEBUG.print("[1]");
				DEBUG.println(buffer);
#endif
				return 1;
			}
			if (strstr(buffer, expected2))                 // ������ �� ������  expected2, ��������� ���������
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
	return 0;                      // ������ expected1 ��� expected2 �� �������.
}


byte CGPRS_SIM800::checkbuffer(const char* expected1, const char* expected2, unsigned int timeout)
{
	// ����� � ������, ��������� �� ������ �������, ��������� � expected1 � expected2, �������� �� ������ ��� � timeout
	while (SIM_SERIAL->available())                  // ���� ��������� ������ � ������, ������ ���� ��������� ������� 
	{
		char c = SIM_SERIAL->read();
		if (m_bytesRecv >= sizeof(buffer) - 1)        // ��� ������ ������������ m_bytesRecv ������������ �"0" (��� ���������� http)
		{
													  // ���� ���������� �������� ������ ������� ������ - �������� ������ ���������.
			m_bytesRecv = sizeof(buffer) / 2 - 1;    // buffer full, discard first half ����� ��������, ��������� ������ ��������
			memcpy(buffer, buffer + sizeof(buffer) / 2, m_bytesRecv);  // ����������� ���������� �������� � buffer
		}
		buffer[m_bytesRecv++] = c;                   // �������� ������ � ����� �� �����, ��������� � m_bytesRecv
		buffer[m_bytesRecv] = 0;                     // ��������� � ������ �������� "0"
		if (strstr(buffer, expected1))               // ������� ������ �����  return 1;
		{
			return 1;
		}
		if (expected2 && strstr(buffer, expected2))  // ���� ����� � ������ ����� expected2 return 2;
		{
			return 2;
		}
	}
	return (millis() - m_checkTimer < timeout) ? 0 : 3;   // ����� �������� ������ � m_checkTimer ������������ ��� ���������� http
														  // ��� �������� ��������� ������������ 0 - ��������� ������� ��� 3 ����� ����� ��� ����������
}

void CGPRS_SIM800::purgeSerial()    // �������� �������� ������
{
   while (SIM_SERIAL->available()) SIM_SERIAL->read();
}
bool CGPRS_SIM800::available()
{
	return SIM_SERIAL->available(); 
}
