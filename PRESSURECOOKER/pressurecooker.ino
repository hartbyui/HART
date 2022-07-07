/*
Date: 6/16/2022
Auther: Caleb Malcarne
Contact: caleb.malcarne@gmail.com
Pressure Cooker

This sensor gathers:
Pressure
Humidity
Temprature
approx. altitude

uploads to SD card
*/
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <SD.h>
#include <DS3232RTC.h>  
#define SEALEVELPRESSURE_HPA (1013.25)
#define DATA_LED 9
File DataFile;
DS3232RTC myRTC;

Adafruit_BME280 bme;

unsigned long delayTime;

void setup() {
    Serial.begin(9600);
    while(!Serial);    
   //------Initalize Real Time Clock------// 
    myRTC.begin();
    setSyncProvider(myRTC.get);   
    if(timeStatus() != timeSet)
        Serial.println("Unable to sync with the RTC");
    else
        Serial.println("RTC has set the system time");
   //----------Initalize SD Card----------//
    Serial.print("Initializing SD card...");
    if (!SD.begin(10)) {
    Serial.println("initialization failed!");
    while (1);
    }
    Serial.println("initialization done.");
    //----------Initalize Sensor----------//
    bme.begin(); 
    //------------------------------------//
    
    
    //delayTime = 1000;
    pinMode(DATA_LED, OUTPUT);
    digitalWrite(DATA_LED, LOW);



    DataFile = SD.open("BME280.csv", FILE_WRITE);
      if(DataFile){
        DataFile.print('/');
        DataFile.print(month());
        DataFile.print('/');
        DataFile.print(day());
        DataFile.print('/');
        DataFile.print(year());
        DataFile.println();

        DataFile.print(hour());
        DataFile.print(printDigits(minute()));
        DataFile.print(printDigits(second()));
        DataFile.println();
        DataFile.println(F("Time, Temprature, Pressure, Altitude, Humidity"));
    }
    DataFile.close();

}

void loop() { 
    digitalClockDisplay();
    printValues();
    recordData();
    delay(delayTime);
}

void recordData(){
  float temp = bme.readTemperature();
  float pres = (bme.readPressure() / 100.0F);
  float alta = bme.readAltitude(SEALEVELPRESSURE_HPA);
  float hum =  bme.readHumidity();

  DataFile = SD.open("BME280.csv", FILE_WRITE);

  if(DataFile){ 
    DataFile.print(hour());
    DataFile.print(':');
    DataFile.print(printDigits(minute()));
    DataFile.print(':');
    DataFile.print(printDigits(second()));
    DataFile.print(',');
    DataFile.print(temp);
    DataFile.print(',');
    DataFile.print(pres);
    DataFile.print(',');
    DataFile.print(alta);
    DataFile.print(',');
    DataFile.print(hum);
    DataFile.println();
    
  }
  DataFile.close();
  digitalWrite(DATA_LED, HIGH);
  delay(100);
  digitalWrite(DATA_LED, LOW);

}

void digitalClockDisplay()
{
    // digital clock display of the time
    Serial.print(hour());
    printDigits(minute());
    printDigits(second());
    Serial.print(' ');
    Serial.print(day());
    Serial.print(' ');
    Serial.print(month());
    Serial.print(' ');
    Serial.print(year());
    Serial.println();
}

int printDigits(int digits)
{
    Serial.print(':');
    if(digits < 10)
        Serial.print('0');
    //Serial.print(digits);
    return(digits);
}

void printValues() {
    Serial.print(F("Temperature = "));
    Serial.print(bme.readTemperature());
    Serial.println(F( "°C"));

    Serial.print(F("Pressure = "));

    Serial.print(bme.readPressure() / 100.0F);
    Serial.println(F(" hPa"));

    Serial.print(F("Approx. Altitude = "));
    Serial.print(bme.readAltitude(SEALEVELPRESSURE_HPA));
    Serial.println(F(" m"));

    Serial.print(F("Humidity = "));
    Serial.print(bme.readHumidity());
    Serial.println(F(" %"));

    Serial.println();
}
