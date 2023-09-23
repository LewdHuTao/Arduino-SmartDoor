#include <EEPROM.h>
#include <MFRC522.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <Servo.h>
#include <SPI.h>
#include <Wire.h>

MFRC522 mfrc522(10, 9);
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo myServo;

constexpr uint8_t greenLed = 7;
constexpr uint8_t whiteLed = 6;
constexpr uint8_t redLed = 5;
constexpr uint8_t ServoPin = 8;
constexpr uint8_t BuzzerPin = 4;
constexpr uint8_t wipeB = 3;
const float voltageThreshold = 0.1;
const int voltagePin = A0;

boolean match = false;
boolean programMode = false;
boolean replaceMaster = false;

uint8_t successRead;

byte storedCard[4];
byte readCard[4];
byte masterCard[4];

char storedPass[4];
char password[4];
char masterPass[4];
boolean RFIDMode = true;
boolean lockMode = false;
boolean NormalMode = true;
char key_pressed = 0;
uint8_t i = 0;


const byte rows = 4;
const byte columns = 4;


char hexaKeys[rows][columns] = {
  { '1', '2', '3' },
  { '4', '5', '6' },
  { '7', '8', '9' },
  { '*', '0', '#' }
};


byte row_pins[rows] = { A0, A1, A2, A3 };
byte column_pins[columns] = { 2, 1, 0 };


Keypad newKey = Keypad(makeKeymap(hexaKeys), row_pins, column_pins, rows, columns);



void setup() {

  pinMode(redLed, OUTPUT);
  pinMode(greenLed, OUTPUT);
  pinMode(whiteLed, OUTPUT);
  pinMode(BuzzerPin, OUTPUT);
  pinMode(wipeB, INPUT_PULLUP);


  digitalWrite(redLed, LOW);
  digitalWrite(greenLed, LOW);
  digitalWrite(whiteLed, LOW);


  lcd.begin();
  lcd.backlight();
  SPI.begin();
  mfrc522.PCD_Init();
  myServo.attach(ServoPin);
  myServo.write(10);

  int sensorValue = analogRead(voltagePin);
  float batteryVoltage = sensorValue * (1.0 / 1023.0);

  if (batteryVoltage > voltageThreshold) {

    digitalWrite(greenLed, HIGH);
    digitalWrite(redLed, HIGH);
    digitalWrite(whiteLed, HIGH);


    delay(120);
    digitalWrite(BuzzerPin, HIGH);
    delay(120);
    digitalWrite(BuzzerPin, LOW);
    delay(120);
    digitalWrite(BuzzerPin, HIGH);
    delay(120);
    digitalWrite(BuzzerPin, LOW);
    delay(2000);
    lcd.clear();
    lcd.setCursor(3, 0);
    lcd.print("SMART DOOR");
    delay(3000);
    lcd.clear();
    digitalWrite(redLed, LOW);
    digitalWrite(greenLed, LOW);
    digitalWrite(whiteLed, LOW);
  }

  ShowReaderDetails();

  if (digitalRead(wipeB) == LOW) {
    digitalWrite(redLed, HIGH);

    lcd.setCursor(0, 0);
    lcd.print("Button Pressed");
    digitalWrite(BuzzerPin, HIGH);
    delay(500);
    digitalWrite(BuzzerPin, LOW);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("This will remove");
    lcd.setCursor(0, 1);
    lcd.print("all records");
    delay(2000);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("You have 10");
    lcd.setCursor(0, 1);
    lcd.print("secs to cancel");
    delay(2000);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Unpress to cancel");
    lcd.setCursor(0, 1);
    lcd.print("Time left: ");

    bool buttonState = monitorWipeButton(10000);
    if (buttonState == true && digitalRead(wipeB) == LOW) {
      lcd.clear();
      lcd.print("Wiping records..");
      for (uint16_t x = 0; x < EEPROM.length(); x = x + 1) {
        if (EEPROM.read(x) == 0) {

        } else {
          EEPROM.write(x, 0);
        }
      }
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Wiping done");


      cycleLeds();
    } else {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Wiping cancelled");
      digitalWrite(redLed, LOW);
    }
  }





  if (EEPROM.read(1) != 143) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("No master card");
    lcd.setCursor(0, 1);
    lcd.print("defined");
    delay(2000);

    lcd.setCursor(0, 0);
    lcd.print("Scan tag to");
    lcd.setCursor(0, 1);
    lcd.print("define as master");

    do {
      successRead = getID();


      digitalWrite(BuzzerPin, HIGH);
      delay(200);
      digitalWrite(BuzzerPin, LOW);

      delay(200);
    } while (!successRead);
    for (uint8_t j = 0; j < 4; j++) {
      EEPROM.write(2 + j, readCard[j]);
    }
    EEPROM.write(1, 143);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Master defined");
    lcd.setCursor(0, 1);
    lcd.print("Add 1 record");
    delay(2000);
    storePassword(6);
  }


  for (uint8_t i = 0; i < 4; i++) {
    masterCard[i] = EEPROM.read(2 + i);
    masterPass[i] = EEPROM.read(6 + i);
  }

  ShowOnLCD();
  cycleLeds();
}



void loop() {

  if (RFIDMode == true) {
    do {
      successRead = getID();

      if (programMode) {
        cycleLeds();
      } else {
        normalModeOn();
      }
    } while (!successRead);
    if (programMode) {
      if (isMaster(readCard)) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Exit setup mode");
        digitalWrite(BuzzerPin, HIGH);
        delay(700);
        digitalWrite(BuzzerPin, LOW);
        ShowOnLCD();
        programMode = false;
        return;
      }

      else {
        if (findID(readCard)) {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Already there");
          deleteID(readCard);

          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Tag to add/del");
          lcd.setCursor(0, 1);
          lcd.print("Master to exit");
        }

        else {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("New tag found");
          lcd.setCursor(0, 1);
          lcd.print("Add to records");
          writeID(readCard);

          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Scan to add or del");
          lcd.setCursor(0, 1);
          lcd.print("Master to exit");
        }
      }
    }

    else {
      if (isMaster(readCard)) {
        programMode = true;
        matchpass();
        if (programMode == true) {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Program Mode");

          uint8_t count = EEPROM.read(0);
          lcd.setCursor(0, 1);
          lcd.print("Found");
          lcd.print(count);
          lcd.print(" records");

          digitalWrite(BuzzerPin, HIGH);
          delay(100);
          digitalWrite(BuzzerPin, LOW);

          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Scan a tag to");
          lcd.setCursor(0, 1);
          lcd.print("add or remove");
        }
      }

      else {
        if (findID(readCard)) {
          granted();
          RFIDMode = false;
          ShowOnLCD();
        }

        else {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Access denied");
          denied();
          ShowOnLCD();
        }
      }
    }
  }

  else if (RFIDMode == false) {
    key_pressed = newKey.getKey();

    if (key_pressed) {
      password[i++] = key_pressed;
      lcd.print("*");
    }
    if (i == 4) {
      delay(200);

      if (lockMode == true) {
        if (!(strncmp(password, storedPass, 4))) {
          lcd.clear();
          lcd.print("Door closed");
          grantedLock();
          lockMode = false;
          RFIDMode = true;
          ShowOnLCD();
          i = 0;
        }
      }

      else if (!(strncmp(password, storedPass, 4))) {
        lcd.clear();
        lcd.print("Door opened");
        granted();
        lockMode = true;
        RFIDMode = true;
        ShowOnLCD();
        i = 0;
      } else {
        lcd.clear();
        lcd.print("Wrong password");
        denied();
        RFIDMode = true;
        ShowOnLCD();
        i = 0;
      }
    }
  }
}


void granted() {

  digitalWrite(redLed, LOW);
  digitalWrite(greenLed, HIGH);
  if (RFIDMode == false) {
    myServo.write(90);
  }
  delay(1000);
  digitalWrite(greenLed, HIGH);
}

void grantedLock() {
  digitalWrite(redLed, LOW);
  digitalWrite(greenLed, HIGH);
  if (RFIDMode == false) {
    myServo.write(0);
    digitalWrite(BuzzerPin, HIGH);
    delay(120);
    digitalWrite(BuzzerPin, LOW);
    delay(120);
    digitalWrite(BuzzerPin, HIGH);
    delay(120);
    digitalWrite(BuzzerPin, LOW);
    digitalWrite(greenLed, HIGH);
  }
}


void denied() {
  digitalWrite(greenLed, LOW);

  digitalWrite(redLed, HIGH);
  digitalWrite(BuzzerPin, HIGH);
  delay(1000);
  digitalWrite(BuzzerPin, LOW);

  digitalWrite(redLed, LOW);
}



uint8_t getID() {

  if (!mfrc522.PICC_IsNewCardPresent()) {
    return 0;
  }
  if (!mfrc522.PICC_ReadCardSerial()) {
    return 0;
  }



  for (uint8_t i = 0; i < 4; i++) {
    readCard[i] = mfrc522.uid.uidByte[i];
  }
  mfrc522.PICC_HaltA();
  return 1;
}


void ShowReaderDetails() {

  byte v = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);


  if ((v == 0x00) || (v == 0xFF)) {
    lcd.setCursor(0, 0);
    lcd.print("Communication failure");
    lcd.setCursor(0, 1);
    lcd.print("Check connections");
    digitalWrite(BuzzerPin, LOW);
    delay(700);

    digitalWrite(greenLed, LOW);

    digitalWrite(redLed, HIGH);
    digitalWrite(BuzzerPin, LOW);
    while (true)
      ;
  }
}


void cycleLeds() {
  digitalWrite(redLed, HIGH);
  digitalWrite(greenLed, HIGH);
}


void normalModeOn() {

  digitalWrite(redLed, LOW);
  digitalWrite(greenLed, LOW);
}


void readID(uint8_t number) {
  uint8_t start = (number * 8) + 2;
  for (uint8_t i = 0; i < 4; i++) {
    storedCard[i] = EEPROM.read(start + i);
    storedPass[i] = EEPROM.read(start + i + 4);
  }
}

void writeID(byte a[]) {
  if (!findID(a)) {
    uint8_t num = EEPROM.read(0);
    uint8_t start = (num * 8) + 10;
    num++;
    EEPROM.write(0, num);
    for (uint8_t j = 0; j < 4; j++) {
      EEPROM.write(start + j, a[j]);
    }
    storePassword(start + 4);
    BlinkLEDS(greenLed);
    lcd.setCursor(0, 1);
    lcd.print("Added to records");
    delay(1000);
  } else {
    BlinkLEDS(redLed);
    lcd.setCursor(0, 0);
    lcd.print("Failed!");
    lcd.setCursor(0, 1);
    lcd.print("wrong ID or bad EEPROM");
    delay(2000);
  }
}


void deleteID(byte a[]) {
  if (!findID(a)) {
    BlinkLEDS(redLed);
    lcd.setCursor(0, 0);
    lcd.print("Failed!");
    lcd.setCursor(0, 1);
    lcd.print("wrong ID or bad EEPROM");
    delay(2000);
  } else {
    uint8_t num = EEPROM.read(0);
    uint8_t slot;
    uint8_t start;
    uint8_t looping;
    uint8_t j;
    uint8_t count = EEPROM.read(0);
    slot = findIDSLOT(a);
    start = (slot * 8) + 10;
    looping = ((num - slot)) * 4;
    num--;
    EEPROM.write(0, num);
    for (j = 0; j < looping; j++) {
      EEPROM.write(start + j, EEPROM.read(start + 4 + j));
    }
    for (uint8_t k = 0; k < 4; k++) {
      EEPROM.write(start + j + k, 0);
    }

    lcd.setCursor(0, 1);
    lcd.print("Removed");
    delay(1000);
  }
}


boolean checkTwo(byte a[], byte b[]) {
  if (a[0] != 0)
    match = true;
  for (uint8_t k = 0; k < 4; k++) {
    if (a[k] != b[k])
      match = false;
  }
  if (match) {
    return true;
  } else {
    return false;
  }
}


uint8_t findIDSLOT(byte find[]) {
  uint8_t count = EEPROM.read(0);
  for (uint8_t i = 1; i <= count; i++) {
    readID(i);
    if (checkTwo(find, storedCard)) {

      return i;
      break;
    }
  }
}


boolean findID(byte find[]) {
  uint8_t count = EEPROM.read(0);
  for (uint8_t i = 1; i <= count; i++) {
    readID(i);
    if (checkTwo(find, storedCard)) {
      return true;
      break;
    } else {
    }
  }
  return false;
}



void BlinkLEDS(int led) {

  digitalWrite(redLed, LOW);
  digitalWrite(greenLed, LOW);
  digitalWrite(BuzzerPin, HIGH);
  delay(100);
  digitalWrite(led, HIGH);
  digitalWrite(BuzzerPin, LOW);
  delay(100);
  digitalWrite(led, LOW);
  digitalWrite(BuzzerPin, HIGH);
  delay(100);
  digitalWrite(led, HIGH);
  digitalWrite(BuzzerPin, LOW);
}



boolean isMaster(byte test[]) {
  if (checkTwo(test, masterCard)) {
    return true;
  } else
    return false;
}


bool monitorWipeButton(uint32_t interval) {
  unsigned long currentMillis = millis();

  while (millis() - currentMillis < interval) {
    int timeSpent = (millis() - currentMillis) / 1000;
    Serial.println(timeSpent);
    lcd.setCursor(10, 1);
    lcd.print(timeSpent);


    if (((uint32_t)millis() % 10) == 0) {
      if (digitalRead(wipeB) != LOW) {
        return false;
      }
    }
  }
  return true;
}


void ShowOnLCD() {
  if (RFIDMode == false) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Enter password:");
    lcd.setCursor(0, 1);
  }

  else if (lockMode == true) {
    if (RFIDMode == true) {
      digitalWrite(BuzzerPin, HIGH);
      delay(120);
      digitalWrite(BuzzerPin, LOW);
      delay(120);
      digitalWrite(BuzzerPin, HIGH);
      delay(120);
      digitalWrite(BuzzerPin, LOW);
      digitalWrite(whiteLed, HIGH);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Door is open");
      lcd.setCursor(0, 1);
      lcd.print("Scan to close");
    }
  }

  else if (RFIDMode == true) {
    digitalWrite(whiteLed, LOW);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Door is close");
    lcd.setCursor(0, 1);
    lcd.print("Scan to open");
  }
}


void storePassword(int j) {
  int k = j + 4;
  lcd.clear();
  lcd.print("New Password:");
  lcd.setCursor(0, 1);
  while (j < k) {
    char key = newKey.getKey();
    if (key) {
      lcd.print("*");
      EEPROM.write(j, key);
      j++;
    }
  }
}

void matchpass() {
  RFIDMode = false;
  ShowOnLCD();
  int n = 0;
  while (n < 1) {
    key_pressed = newKey.getKey();
    if (key_pressed) {
      password[i++] = key_pressed;
      lcd.print("*");
    }
    if (i == 4) {
      delay(200);

      if (!(strncmp(password, masterPass, 4))) {
        RFIDMode = true;
        programMode = true;
        i = 0;
      } else {
        lcd.clear();
        lcd.print("Wrong password");
        digitalWrite(BuzzerPin, HIGH);
        delay(1000);
        digitalWrite(BuzzerPin, LOW);
        programMode = false;
        RFIDMode = true;
        ShowOnLCD();
        i = 0;
      }
      n = 4;
    }
  }
}
