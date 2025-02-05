#include <ESP32Servo.h> //ESP32-specific library for servomotors
#include <FS.h> //file system library, for managing files on SD
#include <SD.h> //library for inteacting with SD card

//Feather-specific libraries for I2C and other connections
#include <Adafruit_BusIO_Register.h>
#include <Adafruit_I2CDevice.h>
#include <Adafruit_I2CRegister.h>

#include <RTClib.h> //library for RTC
#include <Wire.h> //I2C library
#include <Adafruit_Sensor.h> //Adafruit general sensor library
#include <Adafruit_BMP3XX.h> //Adafruit altimeter-specific library
#include <BluetoothSerial.h> //Bluetooth serial library

/*
  BYUI-HART Mechanical Cutdown v1
  Riley Masters - 17 July 2023

  Runs a mechanical cutdown and data logging device for a high-altitude weather balloon. 
  Begins with a setup loop that allows the user to connect to the device via bluetooth and configure settings using a 
  bluetooth serial mobile app. 

  When the setup loop ends, runs a loop that logs flight time, altitude, pressure, and temperature until the device
  is turned off or reset. When flight time reach or exceed a desired value, triggers a servo motor to pull the trigger of 
  a mechanical archery release, dropping the payload from the balloon for recovery. 
*/

// Initialize Variables
int T = 0; // Flight time (seconds)
int TB = 0; //Flight beginning time (seconds, unixtime)
int TR = 9000; // Release time (seconds) - Time after launch at which the cutdown will trigger. Defaults to 2.5 hours.
bool UseA = true; // whether to use altitude ceiling. Defaults to true.
float A = 0; //  Altitude (feet)
float AR = 100000; // Altitude ceiling (feet) - Height at which the cutdown will trigger. Defaults to 100k feet.
float Tem = 0; //temperature (F)
float P = 0; //pressure (hPa) (Yes, it's metric when everything else is English.)
int sampleInt = 2; //Sampling interval (seconds) - how often the altimeter will log data. defaults to every 2 seconds.
bool LogData = true; //whether to log data to an SD card. Defaults to true.
bool Released = false; // Whether release has been triggered.
bool Reset = false; //whether release has been reset
int resetPos = 5; // default servo position
int releasePos = 175; //open servo position
const char *path = "/DataLogs/FlightData.txt"; //file path for data log


// initialize bluetooth 
BluetoothSerial SerialBT; //create BT serial object

#define BT_DISCOVER_TIME	10000

//initialize RTC
RTC_PCF8523 rtc; //create RTC object

//Initialize altitude sensor pins
#define BMP_SCK 13
#define BMP_MISO 12
#define BMP_MOSI 11
#define BMP_CS 10

#define SEALEVELPRESSURE_HPA (1013.25) //set sea level pressure for altitude calculations

Adafruit_BMP3XX bmp; //create sensor object

//Initialize SD Card
const int SDPin = 33; //For ESP32, the SD pin should be 33.
File MyLog;// create file object

// Initialize servo
const int ServoPin = 25;
Servo trigger; //create servo object

// runs before flight
void setup() {
  bool SetupMode = true; // Starts device in setup mode, where timer and altitude values can be changed.
  //initialize all components
  initializeServo(); 
  initializeSensors();
  BTConnect();
  SDcheck();
  bmp.performReading(); // take a sensor reading
  bool uptodate = false; //keeps loop from constantly reprinting an unchanged menu
  while (SetupMode){ //loop through setup menu while cutdown is in setup mode    
      A = bmp.readAltitude(SEALEVELPRESSURE_HPA)*3.28084; // get altitude in meters, convert to feet
      if(uptodate == false){ //only print if values have changed
        //print menu to Bluetooth serial
        SerialBT.print("Current Release Time (s): ");
        SerialBT.println(TR);
        SerialBT.print("Altitude Active?: ");
        SerialBT.println(UseA);
        SerialBT.print("Current Altitude Ceiling (ft): "); 
        SerialBT.println(AR);
        SerialBT.print("Current Altitude (ft): ");
        SerialBT.println(A);
        SerialBT.print("Data Logging Active?: ");
        SerialBT.println(LogData);
        uptodate = true;
      }
      switch (SerialBT.read()){ //setup menu
        case '1' : //set release time
        {
          int TNew = 0;
          //print current timer value, prompt new one.
          SerialBT.print("Current Timer Value (s): ");
          SerialBT.println(TR);
          SerialBT.println("New Timer Value (s): ");
          if(SerialBT.available() > 0){
            TNew = SerialBT.parseInt(); //get new timer value from bluetooth serial
          }
          if (TNew > 0) { //only accept valid times
            TR = TNew;
          } 
          else{
            SerialBT.println("Time cannot be < 0!");
          }
          uptodate = false; //allow menu to print
        break;
        }
        case '2' : //enable/disable altitude ceiling
        {
          switch (UseA){
            case 1: //if altitude ceiling is on, turn it off
              {SerialBT.println("Altitude ceiling disabled.");
              UseA = false;}
            break;

            case 0: // if altitude ceiling is off, turn it on
              {SerialBT.println("Altitude ceiling enabled.");
              UseA = true;}
            break;
          }
          uptodate = false;
        break;
        }
        case '3': //set altitude ceiling
          {
          float ANew = 0;
          // print current altitude ceiling, prompt new one
          SerialBT.print("Current Altitude Ceiling (ft): ");
          SerialBT.println(AR);
          SerialBT.println("New Altitude Ceiling (ft): ");
          if(SerialBT.available() > 0){
            ANew = SerialBT.parseFloat(); //get a float value from the BT serial input
          }
          if (ANew > 0) {
            AR = ANew; // if new altitde is valid, set release altitude to new altitude
          } else{
            SerialBT.println("Altitude cannot be < 0!");
          }
          uptodate = false;}
        break;
        case '4' : //enable/disable data logging
        {
          switch (LogData){
            case 1:
              {SerialBT.println("Data logging disabled.");
              LogData = false;}
            break;

            case 0:
              {switch(SDcheck()){
                case true :
                {
                  SerialBT.println("Data logging enabled.");
                  LogData = true;
                  break;
                }
                case false: 
                {
                  SerialBT.println("Cannot log data without MicroSD card!");
                  break;
                }
              }}
              
            break;
          }
          uptodate = false;
        break;
        case '5' : //end setup mode - remove when physical switch is implemented
        {
          SerialBT.println("Development use only - Setup mode disabled");
          SetupMode = false;
        }
        break;
        case '6' : // manually open release
        {
          trigger.write(releasePos);
          SerialBT.println("Release opened.");
          break;
        }
        case '7' : // manually close release
        {
          trigger.write(resetPos);
          SerialBT.println("Release closed.");
          break;
        }
      }
  } 
  }

  //when setup loop ends, run final preflight setup:  
  //set timer
  DateTime flightBegin = rtc.now(); //read time from RTC
  TB = flightBegin.unixtime(); //set flight beginning time;

  if(LogData) {
    logBegin(SD, flightBegin); //if data logging is on, configure data file and write timestamp
  }

  //Disable bluetooth
  SerialBT.println("Commencing flight. Disabling Bluetooth communication."); // print final message to bluetooth serial
  SerialBT.disconnect(); //disconnect from connected device
  SerialBT.end(); // close bluetooth serial
}

void loop() {
  // runs during flight:
  DateTime logTime = rtc.now(); //read time from RTC
  T = logTime.unixtime() - TB; //Flight time = difference between current time and beginning time
  if(LogData || UseA){ //If altimeter is being used for data logging and/or release, take a data reading
    bmp.performReading(); //read data from altimeter
    A = bmp.readAltitude(SEALEVELPRESSURE_HPA)*3.28084; //Read altitude in meters, convert to feet
    if(LogData && (T%sampleInt == 0)){ //if data logging is on and flight time is a multiple of sampleInt, log a reading
      P = bmp.readPressure()/100; //read pressure in pascals, convert to hectopascals
      Tem = (bmp.readTemperature() * (9/5)) + 32; //read temperature in Celsius, convert to Fahrenheit
      logAppend(SD); //write data to SD card
    }
  }
  //check release
  //open release at proper time or altitude
  if(!Released && (T >= TR || (A >= AR && UseA))){ //if (release has not already been triggered), and (flight time is at or past release time) or ((altitude is at or above release altitude) and (altitude trigger is on))
    trigger.write(releasePos); //open release
    Released = true; //make sure it won't do this again
  }

  if(Released && !Reset && T >= (TR + 15)){ //wait 15 seconds
    trigger.write(resetPos); //close release
    Reset = true; // make sure it won't do this again
  }

  delay(1000); //execute flight loop once per second
}

// Functions

// Bluetooth config and connection. 
void BTConnect() {
  static bool btScanAsync = true;
  static bool btScanSync = true;

  Serial.begin(115200);
  SerialBT.begin("Cutdown"); //Bluetooth device name
  SerialBT.setTimeout(20000); //Serial will wait up to 20 seconds for an input when prompted
  
  if (btScanAsync) {
    if (SerialBT.discoverAsync(btAdvertisedDeviceFound)) {
      delay(10000);
      SerialBT.discoverAsyncStop();
    } 
  }
  
  if (btScanSync) {
    BTScanResults *pResults = SerialBT.discover(BT_DISCOVER_TIME);
    if (pResults) {
      pResults->dump(&Serial);
    }
  }
}

void btAdvertisedDeviceFound(BTAdvertisedDevice* pDevice) {
	Serial.printf("Found a device asynchronously: %s\n", pDevice->toString().c_str());
}

//Sensor setup
void initializeSensors(){
  bmp.begin_I2C(); //initialize I2C connection for altimeter
  //set values for altimeter sampling and calculations
  bmp.setTemperatureOversampling(BMP3_OVERSAMPLING_8X);
  bmp.setPressureOversampling(BMP3_OVERSAMPLING_4X);
  bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_3);
  bmp.setOutputDataRate(BMP3_ODR_50_HZ);

  //Initialize RTC
  rtc.begin();
}

//SD Functions
bool SDcheck() { //checks if an SD card is inserted, returns true if yes, returns false and disables data logging if no
  if(!SD.begin()) {
    SerialBT.println("No MicroSD card found. Data logging disabled.");
    SerialBT.println("Make sure MicroSD card is FAT16 or FAT32 format.");
    LogData = false;
    return false;
  }
  else{
    SerialBT.println("MicroSD card found.");
    return true;
  }
}

void logBegin(fs::FS &fs, DateTime now){ // Creates log file if it doesn't exist, and writes the date and time the flight began to it
  File file = fs.open(path, FILE_APPEND); //write in append mode, so it doesn't overwrite any existing data
  file.print("Flight Data beginning ");
  file.println(now.timestamp());
  file.close();
}

void logAppend(fs::FS &fs){ // Writes data to log file
  File file = fs.open(path, FILE_APPEND); //write in append mode, so it doesn't overwrite any existing data
  file.print("Flight time (s): ");
  file.print(T); //write flight time in seconds
  file.print(", Altitude (ft): ");
  file.print(A); //write altitude in feet
  file.print(", Temperature (F): ");
  file.print(Tem); //write temp in degrees F
  file.print(", Pressure (hPa): ");
  file.println(P); // write pressure in HPa
}

//Servo setup
void initializeServo(){
  ESP32PWM::allocateTimer(0);
	ESP32PWM::allocateTimer(1);
	ESP32PWM::allocateTimer(2);
	ESP32PWM::allocateTimer(3);
	trigger.setPeriodHertz(50);    
	trigger.attach(ServoPin, 1000, 2000);
}