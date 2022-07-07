#include <SD.h>
#include <SPI.h>
#include <Adafruit_GPS.h>
#include <SoftwareSerial.h>

#include <avr/sleep.h>
#include <math.h>
SoftwareSerial mySerial(8, 7);
Adafruit_GPS GPS(&mySerial);

// Set GPSECHO to 'false' to turn off echoing the GPS data to the Serial console
// Set to 'true' if you want to debug and listen to the raw GPS sentences
#define GPSECHO  false
/* set to true to only log to SD when GPS has a fix, for debugging, keep it false */
#define LOG_FIXONLY false  

// this keeps track of whether we're using the interrupt
// off by default!
boolean usingInterrupt = false;
void useInterrupt(boolean); // Func prototype keeps Arduino 0023 happy

// Set the pins used
#define chipSelect 10
#define ledPin 13

File logfile;

// read a Hex value and return the decimal equivalent
uint8_t parseHex(char c) {
  if (c < '0')
    return 0;
  if (c <= '9')
    return c - '0';
  if (c < 'A')
    return 0;
  if (c <= 'F')
    return (c - 'A')+10;
}

// blink out an error code
void error(uint8_t errno) {
  /*
  if (SD.errorCode()) {
   putstring("SD error: ");
   Serial.print(card.errorCode(), HEX);
   Serial.print(',');
   Serial.println(card.errorData(), HEX);
   }
   */
  while(1) {
    uint8_t i;
    for (i=0; i<errno; i++) {
      digitalWrite(ledPin, HIGH);
      delay(100);
      digitalWrite(ledPin, LOW);
      delay(100);
    }
    for (i=errno; i<10; i++) {
      delay(200);
    }
  }
}

void setup() {
  // for Leonardos, if you want to debug SD issues, uncomment this line
  // to see serial output
  //while (!Serial);

  // connect at 115200 so we can read the GPS fast enough and echo without dropping chars
  // also spit it out
  Serial.begin(9600);
  Serial.println("\r\nUltimate GPSlogger Shield");
  pinMode(ledPin, OUTPUT);

  // make sure that the default chip select pin is set to
  // output, even if you don't use it:
  pinMode(10, OUTPUT);

  // see if the card is present and can be initialized:
  //if (!SD.begin(chipSelect, 11, 12, 13)) {
  if (!SD.begin(chipSelect)) {      // if you're using an UNO, you can use this line instead
    Serial.println("Card init. failed!");
    error(2);
  }
  else{
    Serial.println("Card intitalised");
  }
  char filename[15];
  strcpy(filename, "GPSLOG.txt");
  for (uint8_t i = 0; i < 100; i++) {
    filename[6] = '0' + i/10;
    filename[7] = '0' + i%10;
    // create if does not exist, do not open existing, write, sync after write
    if (! SD.exists(filename)) {
      break;
    }
  }

  logfile = SD.open(filename, FILE_WRITE);
  if( ! logfile ) {
    Serial.print("Couldnt create "); 
    Serial.println(filename);
    error(3);
  }
  Serial.print("Writing to "); 
  Serial.println(filename);

  // connect to the GPS at the desired rate
  GPS.begin(9600);

  // uncomment this line to turn on RMC (recommended minimum) and GGA (fix data) including altitude
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
  // uncomment this line to turn on only the "minimum recommended" data
  //GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY);
  // For logging data, we don't suggest using anything but either RMC only or RMC+GGA
  // to keep the log files at a reasonable size
  // Set the update rate
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);   // 100 millihertz (once every 10 seconds), 1Hz or 5Hz update rate

  // Turn off updates on antenna status, if the firmware permits it
  GPS.sendCommand(PGCMD_NOANTENNA);

  // the nice thing about this code is you can have a timer0 interrupt go off
  // every 1 millisecond, and read data from the GPS for you. that makes the
  // loop code a heck of a lot easier!
  useInterrupt(true);

  // Ask for firmware version
  mySerial.println(PMTK_Q_RELEASE);

  Serial.println("Ready!");
}

// I have no idea why they did this here in the parsing example
uint32_t timer = millis();


// Interrupt is called once a millisecond, looks for any new GPS data, and stores it
SIGNAL(TIMER0_COMPA_vect) {
  char c = GPS.read();
  // if you want to debug, this is a good time to do it!
  #ifdef UDR0
      if (GPSECHO)
        if (c) UDR0 = c;  
      // writing direct to UDR0 is much much faster than Serial.print 
      // but only one character can be written at a time. 
  #endif
}

void useInterrupt(boolean v) {
  if (v) {
    // Timer0 is already used for millis() - we'll just interrupt somewhere
    // in the middle and call the "Compare A" function above
    OCR0A = 0xAF;
    TIMSK0 |= _BV(OCIE0A);
    usingInterrupt = true;
  } 
  else {
    // do not call the interrupt function COMPA anymore
    TIMSK0 &= ~_BV(OCIE0A);
    usingInterrupt = false;
  }
}

//required for fmod()   #include <math.h>;
 
// converts lat/long from Adafruit
// degree-minute format to decimal-degrees
double convertDegMinToDecDeg (float degMin) {
  double min = 0.0;
  double decDeg = 0.0;
 
  //get the minutes, fmod() requires double
  min = fmod((double)degMin, 100.0);
 
  //rebuild coordinates in decimal degrees
  degMin = (int) ( degMin / 100 );
  decDeg = degMin + ( min / 60 );
 
  return decDeg;
}

void loop() {
  //Serial.println("GPS");
  if (! usingInterrupt) {
      // read data from the GPS in the 'main loop'
      char c = GPS.read();
      // if you want to debug, this is a good time to do it!
      if (GPSECHO)
         if (c) Serial.print(c);
      } 
  //Serial.println("end GPS read"); 
  
  // if a sentence is received, we can check the checksum, parse it...
  if (GPS.newNMEAreceived()) {
      // a tricky thing here is if we print the NMEA sentence, or data
      // we end up not listening and catching other sentences! 
      // so be very wary if using OUTPUT_ALLDATA and trying to print out data
    
      // Don't call lastNMEA more than once between parse calls!  Calling lastNMEA 
      // will clear the received flag and can cause very subtle race conditions if
      // new data comes in before parse is called again.
      char *stringptr = GPS.lastNMEA();
    
      if (!GPS.parse(stringptr))   // this also sets the newNMEAreceived() flag to false
          return;  // we can fail to parse a sentence in which case we should just wait for another
      //if (!GPS.parse(GPS.lastNMEA()))
      //    return;  // we can fail to parse a sentence in which case we should just wait for another

      // Sentence parsed! 
    
     //6  Serial.println("OK");
      // the next part only logs if there is a GPS fix.  I have this sent to false, so it won't use the
      // next lines I think
      if (LOG_FIXONLY && !GPS.fix) {
           Serial.print("No Fix");
           return;
           }
//
//  Cut in from the parsing code
//   
    // OK now let's try the serial printing from the parsing example
    // if millis() or timer wraps around, we'll just reset it
    if (timer > millis())  timer = millis();
    
    // approximately every 2 seconds or so, print out the current stats
    if (millis() - timer > 2000) { 
         timer = millis(); // reset the timer
         Serial.print(String(GPS.hour, DEC));
         Serial.print(":");
         Serial.print(String(GPS.minute,DEC));
         Serial.print(":");
         Serial.print(String(GPS.seconds, DEC));
         Serial.print('.');
         Serial.print(GPS.milliseconds);
         Serial.print(", ");
         Serial.print("Date: ");
         Serial.print(GPS.day, DEC); 
         Serial.print('/');
         Serial.print(GPS.month, DEC); 
         Serial.print("/");
         Serial.print(GPS.year, DEC);
         Serial.print(", ");
                 
  
         if (GPS.fix) {
             Serial.print("Fix: "); 
             Serial.print((int)GPS.fix);
             Serial.print(", ");
             Serial.print(" quality: "); 
             Serial.print((int)GPS.fixquality);
             Serial.print(", Location: ");
             Serial.print(convertDegMinToDecDeg (GPS.latitude), 4); 
             Serial.print(", "); 
             Serial.print(GPS.lat);
             Serial.print(", "); 
             Serial.print(convertDegMinToDecDeg (GPS.longitude), 4); 
             Serial.print(", "); 
             Serial.print(GPS.lon);
             Serial.print(", Speed (knots): "); 
             Serial.print(GPS.speed);
             Serial.print(", Angle: "); 
             Serial.print(GPS.angle);
             Serial.print(", Altitude: "); 
             Serial.print(GPS.altitude);
             Serial.print(", Satellites: "); 
             Serial.println((int)GPS.satellites);
             }
          else {
              Serial.print("Fix: "); 
              Serial.print((int)GPS.fix);
              Serial.print(", ");
              Serial.print(" quality: "); 
              Serial.println((int)GPS.fixquality);  
             }
       

//
//  Cut in from the parsing code
//   
      // OK now let's write it all to the file
     
         logfile.print(GPS.hour, DEC);
         logfile.print(":");
         logfile.print(GPS.minute, DEC);
         logfile.print(":");
         logfile.print(GPS.seconds, DEC);
         logfile.print(", ");
         logfile.print("Date: ");
         logfile.print(GPS.day, DEC); 
         logfile.print('/');
         logfile.print(GPS.month, DEC); 
         logfile.print("/");
         logfile.print(GPS.year, DEC);
         logfile.print(", ");
         
  
         if (GPS.fix) {
             logfile.print(", Location: ");
             logfile.print(convertDegMinToDecDeg (GPS.latitude), 4);
             logfile.print(GPS.lat);
             logfile.print(", "); 
             logfile.print(convertDegMinToDecDeg (GPS.longitude), 4);
             logfile.print(", "); 
             logfile.print(GPS.lon);
             logfile.print(", Speed : "); 
             logfile.print(", "); 
             logfile.print(GPS.speed * 0.514444);
             logfile.print(", Angle: "); 
             logfile.print(GPS.angle);
             logfile.print(", Altitude: "); 
             logfile.println(GPS.altitude);
             logfile.flush();
             }
         else {    
            logfile.print("Fix: "); 
            logfile.print((int)GPS.fix);
            logfile.print(", ");
            logfile.print(" quality: "); 
            logfile.println((int)GPS.fixquality);
            logfile.flush();
            }
//
//  End addition from the parsing code
//  

// this was their code to write to the SD card.  It is very clever, but hard to understand
/*
    uint8_t stringsize = strlen(stringptr);
    if (stringsize != logfile.write((uint8_t *)stringptr, stringsize))    //write the string to the SD file
        error(4);
    if (strstr(stringptr, "RMC") || strstr(stringptr, "GGA"))   logfile.flush();
    Serial.println();
*/    
       }
  }   
}

/* End code */
