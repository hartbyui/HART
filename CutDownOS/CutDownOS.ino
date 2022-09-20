/*
  cutdownOS
  by: Caleb Malcarne
  data: 7/8/2022

*/

#include <LiquidCrystal.h>
#include <Servo.h>
#define READY 18
#define SW_MODE2 25
#define BTN1 19
#define BTN2 15
#define BTN3 14
#define START 20

//long current_sec = 0;
const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
int mins = 0;
int hours = 0;
bool ready_state = 0;
bool setTime_state = 0;
bool starting = 0;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

Servo myservo;
 

void setup() {
  lcd.begin(16, 2);

  pinMode(BTN1, INPUT_PULLUP);
  pinMode(BTN2, INPUT_PULLUP);
  pinMode(BTN3, INPUT_PULLUP);
  pinMode(READY, INPUT_PULLUP);
  pinMode(START, INPUT_PULLUP);
  pinMode(9, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(READY), readyState, RISING);
  attachInterrupt(digitalPinToInterrupt(BTN1), setTime, RISING);
  attachInterrupt(digitalPinToInterrupt(START), start, RISING);

  lcd.print("CutDownOS Alpha");
  delay(1000);
}

void base(){
  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print("CutDownOS");  
}

void loop(){

  if(digitalRead(START) == 0){
    digitalWrite(9, 1);
  } else {
    digitalWrite(9, 0);
  }
}

void servo_release(){
  int pos = 0;
  for (pos = 0; pos <= 180; pos += 1) { 
    // in steps of 1 degree
    myservo.write(pos);              
    delay(15);                       
  }
  for (pos = 180; pos >= 0; pos -= 1) { 
    myservo.write(pos);              
    delay(15);                       
  }
}

void running(){
  bool run = 1;
  lcd.clear();
  lcd.setCursor(1,0);
  int sec = 0;
  while(run == 1){
    lcd.setCursor(0, 1);
    int rem = (3600*hours+60*mins) - sec;
    lcd.print(rem);
    delay(200000);
    sec++;
    if(rem == 0){
      run = 0;
    }
  }

  lcd.clear();
  lcd.print("timer done!");
  myservo.attach(8);
  servo_release();

}

void start(){
  starting = 1;
  delay(40000);
  lcd.clear();
  lcd.setCursor(1,0);
  lcd.print("press BTN 2 to");
  lcd.setCursor(1,2);
  lcd.print("confirm start");

  while(starting == 1){
    if(digitalRead(BTN2) == 0){
      lcd.clear();
      lcd.setCursor(4,0);
      lcd.print("Starting");
      lcd.setCursor(8,1);
      lcd.print(3);
      delay(250000);
      lcd.setCursor(8,1);
      lcd.print(2);
      delay(250000);
      lcd.setCursor(8,1);
      lcd.print(1);
      delay(250000);
      running();
    } else if (digitalRead(BTN1) == 0 || digitalRead(READY) == 0){
      starting = 0;
    }
  }
}

void printHours(){
  lcd.setCursor(4, 1);
  String b = String("h:" + String(hours));
  lcd.print(b);
}
void printMins(){
  lcd.setCursor(8, 1);
  String d = String("m:" + String(mins));
  lcd.print(d);
}

void setTime(){
  ready_state = 0;
  setTime_state = 1;
  lcd.clear();
  lcd.setCursor(4,0);
  lcd.print("Set Delay");

  printHours();
  printMins();
  while(setTime_state = 1){
    if(digitalRead(READY) == 0){
      readyState();
      setTime_state = 0;
    }
    
    if(digitalRead(BTN2) == 0){
      if(hours == 9){
        hours = 0;
        printHours();
      delay(40000);
      } else {
        hours++;
        printHours();
        delay(40000);
      }
    }

    if(digitalRead(BTN3) == 0){
      if(mins == 60){
        mins = 0;
        printMins();
      delay(40000);
      } else {
        mins++;
        printMins();
        delay(40000);
      }
    }

  }

}

void readyState(){
  myservo.attach(8);
  ready_state = 1;
  lcd.clear();
  lcd.setCursor(5,0);
  lcd.print("Cutdown");
  lcd.setCursor(6,1);
  lcd.print("Ready");

  while(ready_state == 1){
    if(digitalRead(BTN1) == 0){
      ready_state = 0;
      setTime();
    }
    if(digitalRead(START) == 0){
      start();
      ready_state = 1;
    }
  }
}