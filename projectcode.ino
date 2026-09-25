#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN 10
#define RST_PIN 9
MFRC522 rfid(SS_PIN, RST_PIN);

const int TRIG = 7, ECHO = 6, IR_ENTRY = 2, MQ_AO = A0, BUZZER = 8;

// ---- CALIBRATE THESE ----
const int EXIT_DIST_CM = 1;              // only trigger for extremely close exit detection
const int GAS_ON = 200, GAS_OFF = 150;  // smoke level to raise / clear alarm (watch "G" values)
const unsigned long WARMUP_MS = 20000;  // MQ-2 needs warm-up before alarms count
const byte NUM_STATIONS = 3;
const bool RFID_DEBUG = true;            // set to false after you confirm the card IDs
byte tags[NUM_STATIONS][4] = {          // paste your key-tag UIDs (see "UID:" in Serial Monitor)
  {0xDE, 0xAD, 0xBE, 0xEF},
  {0x11, 0x22, 0x33, 0x44},
  {0xAA, 0xBB, 0xCC, 0xDD}
};
// -------------------------

int lastStation = 0;
unsigned long lastTag = 0, lastExit = 0, lastIn = 0, lastGas = 0, lastF = 0;
int exitHits = 0;
bool exitPresent = false, irPrev = false, alarmOn = false;

long distanceCm() {
  digitalWrite(TRIG, LOW);  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  long us = pulseIn(ECHO, HIGH, 25000);
  return us == 0 ? -1 : us / 58;
}

void checkExit() {                       // ultrasonic: person leaving
  long d = distanceCm();
  bool nearby = d > 0 && d < EXIT_DIST_CM;
  if (nearby) {
    if (++exitHits >= 2 && !exitPresent && millis() - lastExit > 700) {
      exitPresent = true; lastExit = millis();
      Serial.println("O");
    }
  } else { exitHits = 0; exitPresent = false; }
}

void checkEntry() {                      // IR: person boarding
  bool ir = digitalRead(IR_ENTRY) == LOW;
  if (ir && !irPrev && millis() - lastIn > 700) { Serial.println("I"); lastIn = millis(); }
  irPrev = ir;
}

void checkSmoke() {
  if (millis() - lastGas < 500) return;
  lastGas = millis();
  long sum = 0;
  for (int i = 0; i < 5; i++) sum += analogRead(MQ_AO);
  int v = sum / 5;
  Serial.print('G'); Serial.println(v);
  if (millis() > WARMUP_MS) {
    if (!alarmOn && v >= GAS_ON) { alarmOn = true; Serial.println("F1"); lastF = millis(); }
    else if (alarmOn && v <= GAS_OFF) { alarmOn = false; Serial.println("F0"); }
  }
  if (alarmOn && millis() - lastF > 2000) { Serial.println("F1"); lastF = millis(); }
  digitalWrite(BUZZER, alarmOn);
}

void checkRfid() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;

  if (RFID_DEBUG) {
    Serial.print("UID:");
    for (byte i = 0; i < rfid.uid.size; i++) {
      Serial.print(rfid.uid.uidByte[i] < 0x10 ? " 0" : " ");
      Serial.print(rfid.uid.uidByte[i], HEX);
    }
    Serial.println();
  }

  bool matched = false;
  for (byte s = 0; s < NUM_STATIONS; s++) {
    if (rfid.uid.size == 4 && memcmp(rfid.uid.uidByte, tags[s], 4) == 0) {
      matched = true;
      int n = s + 1;
      if (n != lastStation && millis() - lastTag > 3000) {
        Serial.print('S'); Serial.println(n);
        lastStation = n; lastTag = millis();
      }
      if (RFID_DEBUG) Serial.print("MATCHED station "); Serial.println(n);
      break;
    }
  }

  if (RFID_DEBUG && !matched) Serial.println("UNKNOWN tag: not in tags[]");

  rfid.PICC_HaltA(); rfid.PCD_StopCrypto1();
}

void setup() {
  Serial.begin(9600);

  pinMode(SS_PIN, OUTPUT);
  digitalWrite(SS_PIN, HIGH);
  pinMode(RST_PIN, OUTPUT);
  digitalWrite(RST_PIN, HIGH);

  SPI.begin();
  rfid.PCD_Init();

  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  pinMode(IR_ENTRY, INPUT_PULLUP);  // common IR obstacle module is LOW when triggered
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);
}

void loop() {
  checkRfid();
  checkExit();
  checkEntry();
  checkSmoke();
  delay(20);
}