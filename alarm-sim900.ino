#include <SoftwareSerial.h>
SoftwareSerial GPRS(7, 8); // RX, TX

enum _parseState {
  PS_DETECT_MSG_TYPE,

  PS_IGNORING_COMMAND_ECHO,

  PS_READ_CMTI_STORAGE_TYPE,
  PS_READ_CMTI_ID,

  PS_READ_CMGR_STATUS,
  PS_READ_CMGR_NUMBER,
  PS_READ_CMGR_SOMETHING,
  PS_READ_CMGR_DATE,
  PS_READ_CMGR_CONTENT
};

byte state = PS_DETECT_MSG_TYPE;

byte alarmStatus = 0;

char buffer[80];
byte pos = 0;

int lastReceivedSMSId = 0;
boolean validSender = false;

void resetBuffer() {
  memset(buffer, 0, sizeof(buffer));
  pos = 0;
}

void setup()
{
  GPRS.begin(9600);
  Serial.begin(9600);

  pinMode(13, OUTPUT);

  GPRS.println("AT+CMGF=1");
  delay(200);

  GPRS.println("AT+CMGD=1,4");
  delay(200);
  
  // Not really necessary but prevents the serial monitor from dropping any input
  while(GPRS.available()) {
    Serial.write(GPRS.read());
  }
}


void loop() {
  while(GPRS.available()) {
    parseATText(GPRS.read());
  }
}

void parseATText(byte b) {

  buffer[pos++] = b;

  if ( pos >= sizeof(buffer) ) {
    resetBuffer(); // just to be safe
  }
  /*
   // Detailed debugging
   Serial.println();
   Serial.print("state = ");
   Serial.println(state);
   Serial.print("b = ");
   Serial.println(b);
   Serial.print("pos = ");
   Serial.println(pos);
   Serial.print("buffer = ");
   Serial.println(buffer);*/

  switch (state) {
    
  case PS_DETECT_MSG_TYPE: 
    {    
      if ( b == '\n' ) {
        resetBuffer();
      } else {     
           
        if ( pos == 3 && strcmp(buffer, "AT+") == 0 ) {
          state = PS_IGNORING_COMMAND_ECHO;
        } else if ( pos == 6 ) {
          //Serial.print("Checking message type: ");
          //Serial.println(buffer);

          if ( strcmp(buffer, "+CMTI:") == 0 ) {
            Serial.println("Received CMTI");
            state = PS_READ_CMTI_STORAGE_TYPE;
          } else if ( strcmp(buffer, "+CMGR:") == 0 ) {
            Serial.println("Received CMGR");            
            state = PS_READ_CMGR_STATUS;
          }
          resetBuffer();
        }
      }
    }
    break;

  case PS_IGNORING_COMMAND_ECHO:
    {
      if ( b == '\n' ) {
        //Serial.print("Ignoring echo: ");
        //Serial.println(buffer);
        state = PS_DETECT_MSG_TYPE;
        resetBuffer();
      }
    }
    break;

  case PS_READ_CMTI_STORAGE_TYPE:
    {
      if ( b == ',' ) {
        Serial.print("SMS storage is ");
        Serial.println(buffer);
        state = PS_READ_CMTI_ID;
        resetBuffer();
      }
    }
    break;

  case PS_READ_CMTI_ID:
    {
      if ( b == '\n' ) {
        lastReceivedSMSId = atoi(buffer);
        Serial.print("SMS id is ");
        Serial.println(lastReceivedSMSId);

        GPRS.print("AT+CMGR=");
        GPRS.println(lastReceivedSMSId);
        //delay(500); don't do this!

        state = PS_DETECT_MSG_TYPE;
        resetBuffer();
      }
    }
    break;

  case PS_READ_CMGR_STATUS:
    {
      if ( b == ',' ) {
        Serial.print("CMGR status: ");
        Serial.println(buffer);
        state = PS_READ_CMGR_NUMBER;
        resetBuffer();
      }
    }
    break;

  case PS_READ_CMGR_NUMBER:
    {
      if ( b == ',' ) {
        Serial.print("CMGR number: ");
        Serial.println(buffer);

        // Uncomment these two lines to check the sender's cell number
        //validSender = false;
        //if ( strcmp(buffer, "\"+0123456789\",") == 0 )
        validSender = true;

        state = PS_READ_CMGR_SOMETHING;
        resetBuffer();
      }
    }
    break;

  case PS_READ_CMGR_SOMETHING:
    {
      if ( b == ',' ) {
        Serial.print("CMGR something: ");
        Serial.println(buffer);
        state = PS_READ_CMGR_DATE;
        resetBuffer();
      }
    }
    break;

  case PS_READ_CMGR_DATE:
    {
      if ( b == '\n' ) {
        Serial.print("CMGR date: ");
        Serial.println(buffer);
        state = PS_READ_CMGR_CONTENT;
        resetBuffer();
      }
    }
    break;

  case PS_READ_CMGR_CONTENT:
    {
      if ( b == '\n' ) {
        Serial.print("CMGR content: ");
        Serial.print(buffer);

        parseSMSContent();
        GPRS.println("AT+CMGD=1,4");

        //GPRS.print("AT+CMGD=");
        //GPRS.println(lastReceivedSMSId);
        //delay(500); don't do this!

        state = PS_DETECT_MSG_TYPE;
        resetBuffer();
      }
    }
    break;
  }
}

void parseSMSContent() {

  char* ptr = buffer;

  while ( strlen(ptr) >= 2 ) {

    if ( ptr[1] == '|' ) {

      if ( 
        ptr[2] == '0'
        && ptr[3] == '0'
        && ptr[4] == '0'
        && ptr[5] == '0'
      ) {

        if ( ptr[0] == '0' ) {
          alarmStatus = 0;
          Serial.println("Alarm OFF - deactivated");
          digitalWrite(13, LOW);
          sendSMS("Alarm OFF - deactivated");
        } else if ( ptr[0] == '1' ) {
          alarmStatus = 1;
          Serial.println("Alarm ON - activated");
          digitalWrite(13, HIGH);
          sendSMS("Alarm ON - activated");
        } else {
          Serial.println("You have to activate or deactivate the alarm");
          sendSMS("You have to activate or deactivate the alarm");
        }
      
      } else {
        Serial.println("Wrong PIN");
        sendSMS("Wrong PIN");
      }
      
    } else {
      Serial.println(ptr[1]);
      Serial.println("Wrong command format");
    }

    ptr += 2;
  }
}


void sendSMS(String msg) {
  GPRS.println("AT+CMGS=\"+480000000000\"");
  delay(500);
  GPRS.print(msg);
  GPRS.write( 0x1a );  
}

void phoneCall() {
  Serial.println("SIM starts calling...");
  GPRS.println("ATD0000000000000000;");
  delay(1000);
}


