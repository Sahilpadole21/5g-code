
const int transmitTimeout = 15000;  // it is used to wait for response after transmiting data to NBIoT

char dataStringRAW[] PROGMEM = "AT+RAWPUB=alerts,IIITH,<data>\r";


const int rangeSINR[4] = {20, 13, 5, 0};

// 0 to 4
int signalStatus;
boolean networkStatus = false;
const int modemResetPin = 16;
String transmitString;
String modemResponse;
String networkString;
unsigned long startOffline;
const int infoTimeout = 5000;
unsigned long modemEpochTime;
bool Slash_n = false;
boolean modemStatus = false;
int RSRP;
int RSRQ;
int SINR;

const int modemStartupInterval = 30;
char networkStr[30] = "";
unsigned int startmsg = 0, networkLatch = 0;

void flushReceiveBuffer()
{
  while (Serial.available())
  {
    Serial.read();
    yield();
  }
}


/*
   check modem response
*/
bool checkResponse(unsigned long timeout, String Response) {
  // flushReceiveBuffer();
  unsigned long previousMillis = millis();
  boolean responseStatus = false;
  boolean gotResponse = false;

  String dataString = "";
  modemResponse = "";
  while ((unsigned long)(millis() - previousMillis) <= timeout && !gotResponse)
  {

    while (Serial.available())
    {
      char inChar = (char)Serial.read();
      dataString += inChar;
    }
    dataString.replace("\r", "");
    if (dataString.indexOf(Response) >= 0)
    {
      responseStatus = true;
      gotResponse = true;
      break;
    }
    if (dataString.indexOf("ERROR") >= 0)
    {

      responseStatus = false;
      gotResponse = true;
      break;
    }
  }
  Debug.print("Response : ");
  Debug.println(dataString);
  modemResponse = dataString;
  // Debug.println(F("End Checking Response"));
  Debug.printf_P(PSTR("End Checking, Response: %s\n"), modemResponse.c_str());

  if (!gotResponse)
  {
    Debug.println(F("Response Timeout"));
  }
  Serial.flush();
  return responseStatus;
}


/*
   function to send data to server
*/
boolean sendRAWData(String dataToSend, int timeout)
{
  flushReceiveBuffer();
  String toSend;
  dataToSend.trim();
  toSend = FPSTR(dataStringRAW);
  toSend.replace("<data>", dataToSend);
  Debug.printf_P(PSTR("Sending Raw Data: %s\n"), toSend.c_str());
  Serial.print(toSend);
  // debugA("Sending Raw Data: %s\n", toSend.c_str());
  transmitString = "";
  transmitString = toSend;
  if (checkResponse(timeout, "+RAWPUB: SENT"))
  {
    startOffline = millis();
    return true;
  }
  else
  {
    //    Debug.println(F("Send Failed"));
    return false;
  }
  // Serial.flush();
}



/*
 * function to get network info
 */
boolean updateNetworkInfo()
{
  flushReceiveBuffer();
  Serial.print("AT+NWKINFO?\r");

  unsigned long previousMillis = millis();
  boolean responseStatus = false;
  boolean gotResponse = false;
  String responseString;
  int commaCount = 0;
  //    boolean EC_Status = false;
  while ((unsigned long)(millis() - previousMillis) <= infoTimeout && !gotResponse)
  {
    while (Serial.available())
    {
      char inChar = (char)Serial.read();
      responseString += inChar;
      if (inChar == '\r') {
        Slash_n = true;
      }
      if (inChar == ',')
      {
        commaCount++;
      }
    }
    if (responseString.indexOf("NWKINFO") >= 0 &&  Slash_n == true)//(commaCount == 4 ||
    {
      //          Debug.println(EC20_String);
      Slash_n = false;
      responseStatus = true;
      gotResponse = true;
      //      networkString = responseString;
      Debug.println(responseString);
      responseString.replace("+NWKINFO:", "");
      Debug.println(responseString);

      networkLatch = responseString.substring(0, responseString.indexOf(",")).toInt();
      Debug.println("NWKLATCH:" + String(networkLatch, DEC));

      int startIndex = responseString.indexOf(",") + 1;
      int endIndex = responseString.lastIndexOf(",");
      networkString = responseString.substring(startIndex, endIndex);

      RSRP = networkString.substring(0, networkString.indexOf(",")).toInt();

      RSRQ = networkString.substring(networkString.indexOf(",") + 1, networkString.lastIndexOf(",")).toInt();

      SINR = networkString.substring(networkString.lastIndexOf(",") + 1, networkString.length()).toInt();
      // modemEpochTime = responseString.substring(responseString.lastIndexOf(","), responseString.length());;
      String epochTimeString = responseString.substring(responseString.lastIndexOf(",") + 1, responseString.length());
      Debug.println(epochTimeString);
      // modemEpochTime = atol(responseString.substring(responseString.lastIndexOf(","), responseString.length()).c_str());
      modemEpochTime = atol(epochTimeString.c_str());
      Debug.printf_P(PSTR("RSRP: %ddBm,RSRQ: %ddB,SINR: %ddB,Modem Clock: %ld\n"), RSRP, RSRQ, SINR, modemEpochTime);

      if (SINR >= rangeSINR[0])
      {
        signalStatus = 4;
      }
      else if (SINR >= rangeSINR[1] && SINR < rangeSINR[0])
      {
        signalStatus = 3;
      }
      else if (SINR >= rangeSINR[2] && SINR < rangeSINR[1])
      {
        signalStatus = 2;
      }
      else if (SINR >= rangeSINR[3] && SINR < rangeSINR[2])
      {
        signalStatus = 1;
      }
      else
      {
        signalStatus = 0;
      }
      
      if (networkLatch == 1 && signalStatus >= 1 ) {
        networkStatus = true;        
      }
      else {
        networkStatus = false;
      }
      //    }
      break;
    }
    if (responseString.indexOf("ERROR") >= 0)
    {
      responseStatus = false;
      gotResponse = true;
      break;
    }
  }
  //  Debug.println(commaCount);
  Debug.print(F("Network Info:"));
  Debug.println(responseString);
  //  Debug.println(networkString);

  if (!gotResponse)
  {
    Debug.println(F("Response Timeout"));
    modemEpochTime = 0;
  }
  Serial.flush();

  return responseStatus;
}

/*
 * function to check modem response
 */

void initCallModem()
{
  unsigned long modemStartedAt = millis();
  flushReceiveBuffer();
  Debug.println(F("Starting Modem: "));
  while ((unsigned long)(millis() - modemStartedAt) <= modemStartupInterval * 1000UL)
  {
    Serial.print("AT\r");
    delay(100);
    if (checkResponse(1000, "OK"))
    {
      modemStatus = true;
      Debug.println(F("Modem Available"));
      if (updateNetworkInfo())
      {
        sprintf(networkStr, "%s", networkString.c_str());
      }
      break;
    }
    // if ((unsigned long)(millis() - modemStatus) >= 1000UL)
    // {
    //   Debug.print(F("."));
    // }
  }
}
