// Define Pin numbers
#define DISPLAY_BUTTON0 0
#define DISPLAY_BUTTON1 1
#define DISPLAY_BUTTON2 2
#define LIMIT_SW 5
#define SERVO_CTRL_PIN 18
#define MOTOR_DIR_PIN 6
#define MOTOR_STEP_PIN 9
#define MOTOR_EN_PIN 10
#define MOTOR_ENC_I 11
#define MOTOR_ENC_B 12
#define MOTOR_ENC_A 13

// Include libraries
//#include <SoftwareSerial.h>
#include <ESP32Servo.h>
#include <AccelStepper.h>
#include <Adafruit_GFX.h>  // Core graphics library
#include <Fonts/FreeMonoBold24pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Adafruit_ST7789.h>  // Hardware-specific library for ST7789
#include <SPI.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>


// Display variables
Adafruit_ST7789 display = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);  // Use dedicated hardware SPI pins
GFXcanvas1 canvas(240, 135);
int16_t textBoundX1, textBoundY1;
uint16_t textBoundWidth, textBoundHeight;
String speedMenu[] = { "Slow", "Medium", "Fast" };
int speedMenuElementCount = 3;

// MOTOR variables
AccelStepper stepper(AccelStepper::DRIVER, MOTOR_STEP_PIN, MOTOR_DIR_PIN);  // Define a stepper and the pins it will use
//int stepsPerRevolution = 200 * 16; // 200 steps per revolution motor * 1/16 microstep setting
long motorEncoderPos = 0;
float stepsPerTickMark = 32;                        //stepsPerRevolution / 100;
unsigned int motorSpeeds[] = {2000, 6000, 10000};  //max of 8000
unsigned int motorAccels[] = {100000, 100000, 100000};
unsigned int motorSpeed = motorSpeeds[0];
unsigned int motorAccel = motorAccels[0];

// Servo variables
Servo handleServo;
const int handleStartingPosition = 95;
const int handleLockedPosition = 90;
const int handleCheckPosition = 105;
int servoMovementDurations[] = {500, 250, 150};
int servoMovementDuration = servoMovementDurations[0];

// Combination variables
//int firstNumber = 400, secondNumber = firstNumber - 202, thirdNumber = secondNumber + 100; // four wheels
int firstNumber = 300, secondNumber, thirdNumber; // three wheels
int thirdNumberArray[] = {12,21,34,47,59,72,84,97}; // list of narrowed down possible combinations for third number
int thirdNumberElements = sizeof(thirdNumberArray)/sizeof(thirdNumberArray[0]);
//const int hardStop = 296;

// menu button variables
long menuPosition = 0;
unsigned long lastButtonPress0 = 0;
unsigned long lastButtonPress1 = 0;
unsigned long lastButtonPress2 = 0;
bool menuPositionChanged = false;
bool buttonWasPressed0 = false;
bool buttonWasPressed1 = false;
bool buttonWasPressed2 = false;


// Interrupt Service Routines
void IRAM_ATTR buttonPress0() {
  unsigned long currentTime = millis();
  if (currentTime - lastButtonPress0 > 250) {
    lastButtonPress0 = currentTime;
    menuPosition++;
    menuPositionChanged = true;
    buttonWasPressed0 = true;
  }
}

void IRAM_ATTR buttonPress1() {
  unsigned long currentTime = millis();
  if (currentTime - lastButtonPress0 > 500) {
    lastButtonPress0 = currentTime;
    buttonWasPressed1 = true;
  }
}

void IRAM_ATTR buttonPress2() {
  unsigned long currentTime = millis();
  if (currentTime - lastButtonPress2 > 250) {
    lastButtonPress2 = currentTime;
    menuPosition--;
    menuPositionChanged = true;
    buttonWasPressed2 = true;
  }
}

void IRAM_ATTR updateMotorEncoderPos(){
  if(digitalRead(MOTOR_ENC_B)){
    motorEncoderPos+=2;
  }
  else{
    motorEncoderPos-=2;
  }
}

void setup() {
  // Set up serial communication
  Serial.begin(9600);
  Serial1.begin(9600, SERIAL_8N1, 2, 1);
  Serial.println("Serial communication has been set up");


  // Set up display
  pinMode(TFT_BACKLITE, OUTPUT);  // turn on backlite
  digitalWrite(TFT_BACKLITE, HIGH);
  pinMode(TFT_I2C_POWER, OUTPUT);  // turn on the TFT / I2C power supply
  digitalWrite(TFT_I2C_POWER, HIGH);
  delay(10);
  display.init(135, 240);  // Init ST7789 240x135
  display.setRotation(1);
  display.fillScreen(ST77XX_BLACK);
  display.setTextColor(ST77XX_WHITE);

  //display.setTextSize(3);
  Serial.println("The display has been set up");

  // Set up menu buttons
  pinMode(DISPLAY_BUTTON0, INPUT_PULLUP); // GPIO 0 is pulled high by default
  pinMode(DISPLAY_BUTTON1, INPUT_PULLDOWN); // GPIO 1 is pulled low by default
  pinMode(DISPLAY_BUTTON2, INPUT_PULLDOWN); // GPIO 2 is pulled low by default
  attachInterrupt(DISPLAY_BUTTON0, buttonPress0, FALLING);
  attachInterrupt(DISPLAY_BUTTON1, buttonPress1, RISING);
  attachInterrupt(DISPLAY_BUTTON2, buttonPress2, RISING);
  Serial.println("The menu buttons have been set up");

  // Set up motor
  //stepper.setPinsInverted(true, false, false);  //directionInvert = true (CCW rotation is positive)
  pinMode(MOTOR_EN_PIN, OUTPUT);
  pinMode(MOTOR_ENC_A, INPUT);
  pinMode(MOTOR_ENC_B, INPUT);
  pinMode(MOTOR_ENC_I, INPUT);
  attachInterrupt(MOTOR_ENC_A, updateMotorEncoderPos, RISING);
  digitalWrite(MOTOR_EN_PIN, HIGH);  //disable motor output
  pinMode(LIMIT_SW, INPUT_PULLUP);
  stepper.setMinPulseWidth(50);
  stepper.setMaxSpeed(motorSpeed);
  stepper.setAcceleration(motorAccel);
  /*stepper.runToNewPosition(-stepsPerRevolution); //delay(1000);
  stepper.runToNewPosition(2 * stepsPerRevolution); //delay(1000);
  stepper.runToNewPosition(stepsPerRevolution); */
  stepper.setCurrentPosition(0);
  Serial.println("The motor has been set up");

  // Set up handle servo
  handleServo.attach(SERVO_CTRL_PIN); handleServo.write(handleStartingPosition);
  //pinMode(SERVO_CTRL_PIN, OUTPUT);

  // Splash screen and speed menu
  display.fillScreen(0x314159);
  display.setFont(&FreeMonoBold18pt7b);
  displayCentered("Auto Dialer", 2);
  displayCentered("V1.0", 4);
  delay(2000);
  display.fillScreen(ST77XX_BLACK);
  display.fillRect(0, 0, 240, 24, ST77XX_BLUE);
  display.setCursor(0, 18);
  display.setFont(&FreeMonoBold12pt7b);
  display.println("Set Motor Speed");
  menuPosition = 0;
  printMenu(speedMenu, speedMenuElementCount, menuPosition);
  bool speedMenuActive = true;
  Serial.println("entering speed menu");
  while (speedMenuActive) {
    if (menuPositionChanged) {
      if (menuPosition < 0) {
        menuPosition = speedMenuElementCount - 1;
      } else if (menuPosition > speedMenuElementCount - 1) {
        menuPosition = 0;
      }
      Serial.print("Menu position changed to: ");
      Serial.println(menuPosition);
      display.setCursor(0, 42);
      printMenu(speedMenu, speedMenuElementCount, menuPosition);
      menuPositionChanged = false;  //reset menuPositionChanged
    }
    if (buttonWasPressed1) {
      Serial.println("button was pressed");
      motorSpeed = motorSpeeds[menuPosition];
      motorAccel = motorAccels[menuPosition];
      servoMovementDuration = servoMovementDurations[menuPosition];
      stepper.setMaxSpeed(motorSpeed);
      stepper.setAcceleration(motorAccel);
      display.fillScreen(ST77XX_BLACK);
      display.setCursor(0, 18);
      display.setFont(&FreeMonoBold12pt7b);
      display.println("Motor speed was");
      display.print("set to ");
      display.print(motorSpeed);
      display.println("pps");
      delay(2000);
      menuPosition = 0;
      speedMenuActive = false;
      buttonWasPressed1 = false;
    }
  }

  // Set zero position
  display.fillScreen(ST77XX_BLACK);
  display.setFont(&FreeMonoBold12pt7b);
  displayCentered("Set", 1);
  displayCentered("Zero Position", 2);
  display.setFont();
  display.println("Use arrow buttons to adjust the zero position, then press enter to start the auto dialing session.");
  digitalWrite(MOTOR_EN_PIN, LOW);  //enable motor output
  while (!buttonWasPressed1) {
    stepper.run();
    if (menuPositionChanged) {
      stepper.moveTo(menuPosition * 4);
      menuPositionChanged = false;  //reset menuPositionChanged
    }
  }
  menuPosition = 0;
  buttonWasPressed1 = false;  //reset button flag
  stepper.setCurrentPosition(0);
  display.setFont(&FreeMonoBold18pt7b); display.fillScreen(ST77XX_BLACK);
  displayCentered("Reseting", 1);displayCentered("Combination", 3);
  //stepper.setMaxSpeed(motorSpeeds[0]); stepper.setAcceleration(motorAccels[0]);
}





void loop() {
  while(firstNumber < 400){
    stepper.runToNewPosition(firstNumber * stepsPerTickMark);
    display.fillScreen(ST77XX_BLACK); display.fillRect(0, 0, 240, 45, ST77XX_WHITE); display.fillRect(0, 90, 240, 45, ST77XX_WHITE); display.setFont(&FreeMonoBold12pt7b); display.setTextWrap(false);
    displayOnGrid(0, 1, ST77XX_RED, ST77XX_WHITE, firstNumber);
    secondNumber = firstNumber - 100;
    while(secondNumber > firstNumber - 200){
      stepper.runToNewPosition(secondNumber * stepsPerTickMark); displayOnGrid(1, 1, ST77XX_ORANGE, ST77XX_WHITE, secondNumber);
      thirdNumber = secondNumber;
      while(thirdNumber < secondNumber + 100){
        for(int i = 0; i < thirdNumberElements; i++){
          if(thirdNumber%100 == thirdNumberArray[i]){
            String combo = String(firstNumber) + ", " + String(secondNumber) + ", " + String(thirdNumber); Serial.print(combo);
            correctPositionError();
            stepper.runToNewPosition(thirdNumber * stepsPerTickMark); displayOnGrid(2, 1, ST77XX_BLUE, ST77XX_WHITE, thirdNumber);
            checkCombination();
            break;
          }
        }
        if(buttonWasPressed1){
          buttonWasPressed1 = false; // clear flag
          display.fillRect(0, 0, 240, 45, ST77XX_YELLOW); display.fillRect(0, 90, 240, 45, ST77XX_YELLOW); display.setTextColor(ST77XX_BLACK); displayCentered("session paused", 1); Serial.println("session was paused");
          while (!buttonWasPressed1) {
            stepper.run();
            if (menuPositionChanged) {
              stepper.move(menuPosition * 4);
              menuPositionChanged = false;  //reset menuPositionChanged
              menuPosition = 0;
            }
          }
          buttonWasPressed1 = false;
          display.fillRect(0, 0, 240, 45, ST77XX_WHITE); display.fillRect(0, 90, 240, 45, ST77XX_WHITE); display.setTextColor(ST77XX_WHITE); Serial.println("session resumed");
        }
        thirdNumber++;
      }
      secondNumber--;
    }
    firstNumber++;
  }
  Serial.println("All combinations tried");
  while(true){
    delay(100);
  }
}

void correctPositionError(){
  int error = stepper.currentPosition() - motorEncoderPos;
  Serial.print("\tStepper Current Position: "); Serial.print(stepper.currentPosition()); Serial.print("\tMotor Encoder Position: "); Serial.print(motorEncoderPos); Serial.print("\tPosition Error: "); Serial.println(error);
  stepper.setCurrentPosition(motorEncoderPos);
}

void checkCombination(){
  /*
  // For dials with a fourth wheel that opens the deadbolt. Ex. Sargent And Greenleaf
  if(thirdNumber < hardStop)
    stepper.runToNewPosition((hardStop - 100) * stepsPerTickMark);
  else
    stepper.runToNewPosition(hardStop * stepsPerTickMark);
  */
  handleServo.write(handleCheckPosition); delay(servoMovementDuration);
  if(digitalRead(LIMIT_SW)){ // Combination was found
    Serial.write("combination was found");
    display.fillScreen(ST77XX_GREEN);
    displayCentered("Combination found", 1);
    displayOnGrid(0, 1, ST77XX_GREEN, ST77XX_BLACK, firstNumber);
    displayOnGrid(1, 1, ST77XX_GREEN, ST77XX_BLACK, secondNumber);
    displayOnGrid(2, 1, ST77XX_GREEN, ST77XX_BLACK, thirdNumber);
    //handleServo.write(handleUnlockedPosition);
    while(true){
      delay(100);
    }
  }
  handleServo.write(handleLockedPosition); delay(servoMovementDuration);
}

void printMenu(String menu[], int numberOfElements, int selectedIndex) {
  //Serial.print("Selected index ");
  //Serial.println(selectedIndex);
  int i = 0;
  for (int i = 0; i < numberOfElements; i++) {
    if (i == selectedIndex) {
      display.setTextColor(ST77XX_BLUE);
      display.println(menu[i]);
      display.setTextColor(ST77XX_WHITE);
    } else {
      display.println(menu[i]);
    }
  }
}


void displayCentered(String text, int lineNumber) {
  //display.setFont(&FreeMonoBold18pt7b);
  display.getTextBounds(text, 0, 0, &textBoundX1, &textBoundY1, &textBoundWidth, &textBoundHeight);
  display.setCursor((240 - textBoundWidth) / 2, textBoundHeight * lineNumber);
  display.println(text);
}

void displayOnGrid(int gridX, int gridY, uint16_t backgroundColor, uint16_t textColor, int number){
  String text = numberToCombination(number);
  display.fillRect(gridX*80, gridY*45, 80, 45, backgroundColor); display.getTextBounds(text, gridX*80, (gridY + 1)*45, &textBoundX1, &textBoundY1, &textBoundWidth, &textBoundHeight);
  display.setCursor(gridX*80 + (80 - textBoundWidth) / 2, (gridY + 1)*45 - (45 - textBoundHeight)/2); display.setTextColor(textColor); display.print(text);
}

String numberToCombination(int number){
  String text = String(number);
  return String(text[1]) + String(text[2]);
}