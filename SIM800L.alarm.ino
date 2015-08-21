/* ardu.SIM800L.connect
 * Control an arduino with a SIM800L module through phone calls and SMS.
 * Initially this sketch was built to react to the trigering of an alarm by sending SMS
 * 
 * by Vincent Debierre
 * License: Open source.
 * https://github.com/thilab/ardu.SIM800L.connect 
 */


#include <SoftwareSerial.h>

// DEBUG
#define DEBUG
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
#define BUFFERSIZE 64
#define OUTPUTSIZE 170

//Input pins alarm
#define sirenePIN 10
#define actifPIN 11
#define batPIN 12
#define autoprotPIN 13

//Macros
#define sireneSTATUS (digitalRead (sirenePIN))
#define actifSTATUS (digitalRead (actifPIN))
#define batSTATUS (digitalRead (batPIN))
#define autoprotSTATUS (digitalRead (autoprotPIN))


//SMS text
#define sireneTXT "ALARME !"
#define batTXT "Batterie de l'alarme faible."
#define autoprotTXT "Autoprotection activée."

// Phone number list
char* phoneNUMBERList[] = "3360300000", "3375300000", "3360560000", "33382500000"};

//Receivers list
byte sireneDEST[] = {1, 2};
byte maintDEST[] = {0};
byte autoprotDEST[] = {2};
byte approvedLIST[] = {0, 1, 3};

char buffer[BUFFERSIZE];        // buffer array for data received from SIM800
char SIM800output[OUTPUTSIZE];  // Parsing string
unsigned int bufferPOS = 0;     // bufferPOS for buffer array
byte nCHAR = 0;
char c;

boolean SMSreceived = false;
boolean CALLreceived = false;

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

  SIM800.print(F("AT\r"));                // connection to allow auto-bauding setup
  delay(100);
  SIM800.print(F("ATE0;AT+CLIP=1;+CMGF=1;+CNMI=2,2,0,0,0\r"));
  delay(100);

  /* This code need to be fine tuned to replace previous lines
   while (sendATcommand("AT", "OK", 2000));                              // connection to allow auto-bauding setup
   while (sendATcommand("ATE0", "OK", 2000));                            // Echo off
   delay(500);
   while (sendATcommand("AT+CLIP=1;+CMGF=1;+CNMI=2,2,0,0,0", "OK", 2000));// turn on caller ID notification ; turn SMS system into text mode ; Send SMS data directly to the serial connexion
  */

  memset(buffer, '\0', BUFFERSIZE);       // Empty strings
  memset(SIM800output, '\0', OUTPUTSIZE);
}

void loop()
{
  // Verify alarm pins status
  if (sireneSTATUS && !(autoprotSTATUS)) SMS(sirenePIN);
  if autoprotSTATUS SMS(autoprotPIN);
  if batSTATUS SMS(batPIN);

  // Read data received from SIM800
  readSIM800();

  // Act according to msg received
  SIM800do();

#ifdef DEBUG
  // Send serial commands to SIM800
  if (Serial.available()) SIM800.write(Serial.read());
#endif
}

void SMS (int msgSMS) {
  switch (msgSMS) {
    case sirenePIN :
      sendGroupSMS(sireneDEST, sireneTXT);
      break;
    case autoprotPIN :
      sendGroupSMS(autoprotDEST, autoprotTXT);
      break;
    case batPIN :
      sendGroupSMS(maintDEST, batTXT);
      break;
  }
}

void sendGroupSMS (byte receivers[], char * smsTXT) {
  int i;
  int DESTnumb = sizeof(receivers) / sizeof(byte);
  for (i = 0; i < DESTnumb; i++) {
    sendSMS (phoneNUMBERList[i], smsTXT);
  }
}

void sendSMS (char * receiver, char * smsTXT) {

  char buff[30];
  memset(buff, '\0', 30);
  sprintf(buff, "AT+CMGS=\"%s\"\r", receiver);
  SIM800.println(buff);

  delay(200);
  SIM800.print(smsTXT);
  delay(200);
  SIM800.write(0x1A);

  /*  SIM800.write("AT + CMGS = \"");
  SIM800.write(receiver);
  SIM800.write("\"\r");
  SIM800.write(smsTXT);
  SIM800.write(((char)26));
  delay(100);
  //char ctrlz[1] = {(char)26};
  //sendATcommand(ctrlz, "OK", 2000); //the ASCII code of the ctrl+z is 26
  delay(500);
  */
}

void readSIM800 () {

  nCHAR = SIM800.available();

  if (nCHAR > 0) {
    for (int i = 0; i < nCHAR ; i++) {
      if (bufferPOS == BUFFERSIZE - 1) {
        if (SIM800output[0] != '\0') {
          strcat (SIM800output, buffer);
        }
        memset(buffer, '\0', BUFFERSIZE); // Initialize buffer[]
        bufferPOS = 0;
      }

      c = SIM800.read();
      switch (c) {

        case '\n' :

          if (bufferPOS > 0) {
            if (buffer[bufferPOS - 1] == '\r') {
              buffer[bufferPOS - 1] = '\0'; // Clear <CR>
              strcat (SIM800output, buffer);
              memset(buffer, '\0', BUFFERSIZE); // Initialize buffer[]
              bufferPOS = 0;
            }
          } else {
            buffer[bufferPOS++] = c; // Todo (?): Check if end of previous file = '\r'
          }


          break;
        default:

          buffer[bufferPOS++] = c;
          break;
      }
    }
  }
}


char* findCALLid () {

  char * p1 = strchr(SIM800output, '"' ) + sizeof(char);
  if (*p1 == '+') p1++;
  char * p2 = strchr(p1, '"' );
  int strsize = p2 - p1;
  static char callID[20];
  memmove(&callID, p1, strsize);
  callID[strsize] = '\0';
  return callID;
}

char* approvedCALLER (char * callID) {
  DEBUG_PRINTLN(callID);

  for (int i = 0; i < (sizeof(approvedLIST) / sizeof(byte)); i++) {
    if (strcmp(callID, phoneNUMBERList[approvedLIST[i]]) == 0) return callID;
  }
  return '\0';
}

void SIM800do () {

  if (SIM800output[0] != '\0') {

    char cmd[5];
    strncpy (cmd, SIM800output, 4);
    cmd[4] = '\0';

    if (SMSreceived) {
      // OBEY YOUR MASTER
      SMSreceived = false;

    } else if (strcmp(cmd, "RING") == 0) {             // APPEL RECU
      CALLreceived = true;

    } else if (strcmp(cmd, "+CLI") == 0) {             // CALL ID

      SIM800.print(F("ATH\r"));                        // HANG UP!
      delay(500); //important ! To fine tune.

      if (CALLreceived && (approvedCALLER(findCALLid()) != '\0')) {
        // Send Alarm Status
        sendSMS(phoneNUMBERList[1], sireneTXT);

      }

      CALLreceived = false;

    } else if (strcmp(cmd, "+CMT") == 0) {             // SMS RECEIVED

      if (approvedCALLER(findCALLid())) {              // compare CALLID with authorized numbers
        SMSreceived = true;
        sendSMS(phoneNUMBERList[1], sireneTXT);

        DEBUG_PRINTLN("SMS received from approved #");
      }

    } else if (strcmp(cmd, "NO C") == 0) {             // APPEL RACCROCHE

    } else if (strcmp(cmd, "OK") == 0) {               // OK. Ready.

    } else {                                           // Other MSG. Send to DEBUG/maintenance group

      DEBUG_PRINTLN(SIM800output);

      // sendGroupSMS (maintDEST, SIM800output);
    }

    memset(SIM800output, '\0', OUTPUTSIZE);            // Empty SIM800output for next parse.
  }
}

boolean sendATcommand (char* ATcommand, char* expected_answer, unsigned int timeout) {
  
#define ANSWERSIZE 100

  byte i = 0;
  boolean result = false;
  unsigned long start_timer;
  char answer[ANSWERSIZE];
  delay (100);
  memset(answer, '\0', ANSWERSIZE);
  // while ( SIM800.available() > 0) SIM800.read();
  SIM800.print(ATcommand + '\r');
  DEBUG_PRINTLN(ATcommand);
  start_timer = millis();

  do {
    if (SIM800.available() != 0) {
      answer[i++] = SIM800.read();
      DEBUG_PRINTLN(answer);
      if (strstr(answer, expected_answer) != NULL) result = true;
    }
  }
  while ((result == false) && ((millis() - start_timer) < timeout));
  return result;
}
