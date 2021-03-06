/* ardu.SIM800L.connect
 * Control an arduino with a SIM800L module through phone calls and SMS.
 * Initially this sketch was built to react to the trigering of an alarm by sending SMS
 *
 * by Vincent Debierre
 * License: Open source.
 * https://github.com/thilab/ardu.SIM800L.connect
 */

#include <SoftwareSerial.h>

// DEBUG & TEST - Comment these two for production
#define TEST // For the phone numbers to be used
#define DEBUG // For debuging through Serial

#ifdef DEBUG
#define DEBUG_PRINT(x)    Serial.print (x)
#define DEBUG_PRINTDEC(x) Serial.print (x, DEC)
#define DEBUG_PRINTLN(x)  Serial.println (x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTDEC(x)
#define DEBUG_PRINTLN(x)
#endif


//def SIM800L
#define RXPIN 2
#define TXPIN 3
#define LEDPIN 13
#define OUTPUTSIZE 150

//Input pins alarm
#define actifPIN 6
#define autoprotPIN 7
#define batPIN 8
#define sirenePIN 9

#define OK 1
#define ERR 2
#define SENT 3
#define INPUTREADY 4
#define ACKN 0
#define NORM 1
#define NOTAPPROVED 255

//Macros
#define sireneSTATUS (digitalRead (sirenePIN))
#define actifSTATUS (digitalRead (actifPIN))
#define batSTATUS ((digitalRead (batPIN))) // CHANGER SUR SITE
#define autoprotSTATUS (digitalRead (autoprotPIN))

#ifdef TEST
#define batSTATUS ((digitalRead (batPIN))) // CHANGER SUR SITE
#endif

//SMS text
#define sireneTXT "ALARME !"
#define batTXT "Batterie faible."
#define autoprotTXT "Autoprotection!"

// Phone number list
char* phoneNUMBERList[] = {"33603000000", "33695000000", "33661000000", "3364000000", "3368000000", "336000000"};

//Receivers list
#ifdef TEST
byte sireneDEST[] = {0, 0, 0};
byte maintDEST[] = {0};
byte autoprotDEST[] = {0, 1};
byte approvedLIST[] = {0, 1, 2, 3, 4, 5};
#define sireneNUM 3
#define maintNUM 1
#define autoprotNUM 2
#define approvedNUM 6
#else
byte sireneDEST[] = {0, 1, 2, 3, 4, 5};
byte maintDEST[] = {0};
byte autoprotDEST[] = {0, 1, 2, 3, 4, 5};
byte approvedLIST[] = {0, 1, 2, 3, 4, 5};
#define sireneNUM 6
#define maintNUM 1
#define autoprotNUM 6
#define approvedNUM 6
#endif

char SIM800output[OUTPUTSIZE];  // Parsing string
unsigned int bufferPOS = 0;     // bufferPOS for buffer array
unsigned long dwellstart = 0;

#ifdef DEBUG
const unsigned double actionDwell = 60000;   // Limit action to one every 1min
#else
const unsigned double actionDwell = 300000;   // Limit action to one every 5 min
#endif


static char callID[20];

boolean SMSreceived = false;
boolean CALLreceived = false;
boolean stopSMS = false;
boolean outputRDY = false;
boolean SMSduringLOOP[approvedNUM];

SoftwareSerial SIM800(RXPIN, TXPIN);

void setup()
{
  pinMode(LEDPIN, OUTPUT);
  digitalWrite(LEDPIN, LOW);

  pinMode(sirenePIN, INPUT);
  pinMode(actifPIN, INPUT);
  pinMode(batPIN, INPUT);
  pinMode(autoprotPIN, INPUT);

  //Setup liaison série
#ifdef DEBUG
  Serial.begin(19200);             // Arduino Serial port baud rate.
#endif

  //Setup SIM800L
  pinMode(RXPIN, INPUT);
  pinMode(TXPIN, OUTPUT);
  SIM800.begin(19200);
  delay(20000); // give time to log on to network.
  DEBUG_PRINTLN("SETUP");
  SIM800.print(F("AT\r"));                // connection to allow auto-bauding setup
  delay(200);
  SIM800.print(F("ATE0\r"));              // Echo off
  delay(200);
  SIM800.print(F("AT+CLIP=1;+CMGF=1;+CNMI=2,2,0,0,0\r")); // turn on caller ID notification ; turn SMS system into text mode ; Send SMS data directly to the serial connexion
  delay(200);

  memset(SIM800output, '\0', OUTPUTSIZE);   // Empty string

  //Empty buffer
  while (SIM800.available() != 0) SIM800.read() ;

  // Send status to maintenance group
  char * a = getSTAT();
  sendGroupSMS (maintDEST, maintNUM, a);
  free(a);
}

void loop()
{
  // Verify alarm pins status -
#ifdef DEBUG
  if sireneSTATUS DEBUG_PRINTLN("sirene");
  if autoprotSTATUS DEBUG_PRINTLN("autoprotection");
  if batSTATUS DEBUG_PRINTLN("batterie");
#endif

  if (!stopSMS) {
    if (sireneSTATUS && !(autoprotSTATUS)) action(sirenePIN);
    if autoprotSTATUS action(autoprotPIN);
    if batSTATUS action(batPIN);
  }

  // Read data received from SIM800
  readSIM800();

  // Act according to msg received
  SIM800do(NORM);

#ifdef DEBUG
  // Send serial commands to SIM800
  if (Serial.available()) SIM800.write(Serial.read());
#endif
}

void action (int PIN) {

  DEBUG_PRINT("Action: "); DEBUG_PRINTLN(PIN);
  DEBUG_PRINTLN(millis());
  DEBUG_PRINTLN(dwellstart);

  if ((dwellstart == 0) || ((millis() - dwellstart) > actionDwell)) {
    dwellstart = millis();
    switch (PIN) {
      case sirenePIN :
        sendGroupSMS(sireneDEST, sireneNUM, sireneTXT);
        /*
        char o[approvedNUM];
        boolean ToSEND;
        ToSEND = false;
        for (int i = 0; i < approvedNUM; i++) {
          o[i]=(SMSduringLOOP[i] ? '1' : '0');
          if (SMSduringLOOP[i]) ToSEND = true;
        }
        if (ToSEND) sendGroupSMS(sireneDEST, sireneNUM, o);
        memset(SMSduringLOOP, 0, approvedNUM);
        */
        break;
      case autoprotPIN :
        sendGroupSMS(autoprotDEST, autoprotNUM, autoprotTXT);
        break;
      case batPIN :
        sendGroupSMS(maintDEST, maintNUM, batTXT);
        break;
    }
  }
}

void sendGroupSMS (byte receivers[], byte NUM, char smsTXT[]) { //char *TXT
  int i;
  char txt[OUTPUTSIZE];
  memmove(txt, smsTXT, OUTPUTSIZE);
  for (i = 0; i < NUM; i++) {
    sendSMS (phoneNUMBERList[receivers[i]], txt);
    DEBUG_PRINT(txt); DEBUG_PRINT(" sent to :"); DEBUG_PRINTLN(phoneNUMBERList[receivers[i]]);
  }
}

void wait4ackn (byte ret) {
  byte ackn;
  memset(SIM800output, '\0', OUTPUTSIZE);
  outputRDY = false;

  do {
    readSIM800();
    ackn = SIM800do(ACKN);
  } while ((ackn != ret) || (ackn == ERR));
  if (ackn == ERR) {
    // ERROR - SMS NOT SENT
    DEBUG_PRINTLN("ERROR");
  } else {
    DEBUG_PRINTLN("ACKNOWLEDGED");
  }
}

void sendSMS (char * receiver, char * smsTXT) {
  if (!stopSMS) {
    DEBUG_PRINT(smsTXT); DEBUG_PRINT(" /SENT TO/ "); DEBUG_PRINTLN(receiver);
    char ATcmd[30];
    memset(ATcmd, '\0', 30);
    sprintf(ATcmd, "AT+CMGS=\"%s\"\r", receiver);
    SIM800.println(ATcmd);
    delay(200);
    //    wait4ackn (INPUTREADY);
    SIM800.print(smsTXT);
    delay(200);
    SIM800.write(0x1A);

    wait4ackn (SENT);
    // wait4ackn (OK);
  }
}

void readSIM800 () {

  char c;
  byte nCHAR ;
  nCHAR = SIM800.available();

  if (nCHAR > 0) {

    for (int i = 0; i < nCHAR ; i++) {

      if (bufferPOS == OUTPUTSIZE - 1) {                // Deal with buffer overflow
        outputRDY = true;
        bufferPOS = 0;
        DEBUG_PRINTLN("OVERFLOW");
        while (SIM800.available() != 0)
        { //Empty buffer
          if (SIM800.read() == '\n') return;
        }
        return;
      }

      c = SIM800.read();
      // DEBUG_PRINT(i);DEBUG_PRINTLN(c);

      if ((c == '\n') && (bufferPOS > 0)) {

        if (SIM800output[bufferPOS - 1] == '\r') {
          SIM800output[bufferPOS - 1] = '\0'; // Clear <CR>
          if (bufferPOS != 1) {
            // DEBUG_PRINT("bufferPOS: "); DEBUG_PRINTLN(bufferPOS);
            outputRDY = true;
          }
          bufferPOS = 0;
          return;
        }
      }
      SIM800output[bufferPOS] = c;
      bufferPOS++;
    }
  }
}



void findCALLid () {                                     //char* findCALLid (){
  memset(callID, '\0', 20);
  char * p1 = strchr(SIM800output, '"' ) + sizeof(char);
  if (*p1 == '+') p1++;
  char * p2 = strchr(p1, '"' );
  int strsize = p2 - p1;
  //static char callID[20];
  memmove(&callID, p1, strsize);
  callID[strsize] = '\0';
}

byte approvedCALLER (char * callID2) {
  DEBUG_PRINT("CALLID :"); DEBUG_PRINTLN(callID2);

  for (int i = 0; i < approvedNUM; i++) {
    if (strcmp(callID2, phoneNUMBERList[approvedLIST[i]]) == 0) {
      DEBUG_PRINTLN("CALLID APPROVED");
      return i;
    }
  }
  return NOTAPPROVED;
}

byte SIM800do (boolean mode) {
  byte ret = 0;

  if (outputRDY == true) {
    DEBUG_PRINT(mode); DEBUG_PRINT("SIM800: "); DEBUG_PRINTLN(SIM800output);

    char cmd[5];
    strncpy (cmd, SIM800output, 4);
    cmd[4] = '\0';

    if (mode == NORM) {

      if (SMSreceived ) {
        if (strcmp(cmd, "Stop") == 0) {
          sendSMS(callID, "STOP");
          stopSMS = true;                                 //stop sending SMS
          DEBUG_PRINTLN("STOP SENDING SMS");

        } else if (strcmp(cmd, "Star") == 0) {
          stopSMS = false;                                //(re)start sending SMS
          dwellstart = 0;
          sendSMS(callID, "START");
          DEBUG_PRINTLN("START SENDING SMS");

        } else if (strcmp(cmd, "Ok") == 0) {             // The receiver will deal with the alarm.
          //char a[12];
          sendGroupSMS(sireneDEST, sireneNUM, callID);   // Advise others.

        } else if (strcmp(cmd, "Stat") == 0) {
          sendSTAT();

        } else if (cmd[0] == '/') {

          switch (cmd[1]) {
            case 'a':
              if (SIM800output[3] != '\0') {
                memmove(SIM800output + 1, SIM800output + 3, OUTPUTSIZE - 4);
                SIM800output[0] = '>';
                sendGroupSMS(sireneDEST, sireneNUM, SIM800output);
              }
              break;
          }

        }

        SMSreceived = false;

      } else if (strcmp(cmd, "RING") == 0) {             // APPEL RECU
        CALLreceived = true;

      } else if (strcmp(cmd, "+CLI") == 0) {             // CALL ID

        SIM800.print(F("ATH\r"));                        // HANG UP!
        delay(500); //important ! To fine tune.
        findCALLid();
        if (CALLreceived && (approvedCALLER(callID) != NOTAPPROVED)) {
          // Send Alarm Status
          delay(1000);
          sendSTAT();
        }
        CALLreceived = false;

      } else if (strcmp(cmd, "+CMT") == 0) {             // SMS RECEIVED
        findCALLid();
        if (approvedCALLER(callID) != NOTAPPROVED) {              // compare CALLID with authorized numbers
          SMSreceived = true;
          DEBUG_PRINTLN("SMS received from approved #");
        }

      } else {                                           // Other MSG. Send to DEBUG/maintenance group
        ret = acknSIM800do(cmd);
      }
    } else {
      ret = acknSIM800do(cmd);
    }
    memset(SIM800output, '\0', OUTPUTSIZE);            // Empty SIM800output for next parse.
    outputRDY = false;
    return ret;
  }
}

byte acknSIM800do(char cmd[]) {
  byte ret = 0;
  if (strcmp(cmd, "+CMG") == 0) {             // +CMGS - SMS SENT
    ret = SENT;

  } else if (strcmp(cmd, "> ") == 0) {               // Prompt for SMS text
    ret = INPUTREADY;

  } else if ((strcmp(cmd, "NO C") == 0) || (strcmp(cmd, "AT") == 0) || (strcmp(cmd, "ATE0") == 0)) {         // NO CARRIER, SETUP OUTPUT


  } else if (strcmp(cmd, "OK") == 0) {               // OK. Ready.
    ret = OK;

  } else if (strcmp(cmd, "ERRO") == 0) {             // ERROR
    ret = ERR;

  } else if (strcmp(cmd, "+CMT") == 0) {             // SMS RECEIVED
    findCALLid();
    byte OKcaller = approvedCALLER(callID);
    if (OKcaller != NOTAPPROVED) {              // compare CALLID with authorized numbers
      DEBUG_PRINTLN("SMS received from approved #");
      SMSduringLOOP[OKcaller] = true;
    }
  } else {
    DEBUG_PRINT("UNKNOWN OUTPUT: "); DEBUG_PRINTLN(cmd);
  }
  return ret;
}

char* getSTAT() {
  //char sSTAT[11];
  char *sSTAT = (char*)malloc(11);
  sSTAT[10] = '\0';

  sSTAT[0] = 'e';
  sSTAT[1] = (actifSTATUS ? '1' : '0');
  sSTAT[2] = 's';
  sSTAT[3] = (sireneSTATUS ? '1' : '0');
  sSTAT[4] = 'b';
  sSTAT[5] = (batSTATUS ? '1' : '0');
  sSTAT[6] = 'a';
  sSTAT[7] = (autoprotSTATUS ? '1' : '0');
  sSTAT[8] = 's';
  sSTAT[9] = (stopSMS ? '1' : '0');
  DEBUG_PRINT("STATE: "); DEBUG_PRINTLN(sSTAT);
  return sSTAT;
}

void sendSTAT() {
  char * a = getSTAT();
  boolean b = stopSMS;

  stopSMS = false;
  sendSMS(callID, a);
  stopSMS = b;

  free(a);
}
