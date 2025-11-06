#include <SPI.h>
#include <RF24.h>
#include <AccelStepper.h>
#include <Wire.h>
#include "SparkFun_TMAG5273_Arduino_Library.h"
#include <EEPROM.h>

// ================= PARAMETERS =================
#define CE_PIN               14
#define CSN_PIN              15
#define STEP_PIN             2
#define DIR_PIN              3
#define SLP_PIN              1
#define BUTTON_FWD           13
#define BUTTON_REV           14

const int STEPPER_MAX_SPEED    = 10000;   // steps/sec
const int STEPPER_ACCEL        = 5000;    // steps/sec^2
const int STEP_SIZE_BUTTON     = 5000;    // steps per button press
const float HOMING_THRESHOLD_mT = 0.0;    // Hall sensor threshold
const long MAX_VOLUME_uL        = 7000;   // Maximum volume in μL
const float STEPS_PER_UL = 15.1;          // steps per μL

// ================= RADIO ======================
RF24 radio(CE_PIN, CSN_PIN);
const uint64_t pipePCtoPump = 0xF0F0F0F0E1LL;
const uint64_t pipePumpToPC = 0xF0F0F0F0D2LL;

// ================= STEPPER ====================
AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);
long remainingSteps = 0;
bool pumpActive = true; // ON by default

// ================= HALL SENSOR =================
TMAG5273 hallSensor;

// ================= DEBOUNCE ===================
unsigned long lastButtonTimeFWD = 0;
unsigned long lastButtonTimeREV = 0;
const unsigned long debounceDelay = 50; // ms

// ================= FUNCTIONS ===================
long volumeToSteps(float uL) { return round(uL * STEPS_PER_UL); }

void sendRemainingVolume() {
  float remaining_uL = remainingSteps / STEPS_PER_UL;
  String msg = "REM:" + String(remaining_uL, 2);
  char buf[32];
  msg.toCharArray(buf, 32);
  radio.stopListening();
  radio.write(&buf, sizeof(buf));
  radio.startListening();
}

void homePump() {
  // Set full speed, no acceleration
  stepper.setMaxSpeed(STEPPER_MAX_SPEED);
  stepper.setAcceleration(0);           // temporarily disable acceleration
  stepper.setSpeed(-STEPPER_MAX_SPEED); // move backwards to home

  unsigned long lastSend = 0;

  while (true) {
    stepper.runSpeed();

    float z = hallSensor.getZData();

    // Send hall sensor reading over radio every 1 second
    if (millis() - lastSend >= 1000) {
      String msg = "HALL:" + String(z, 2);
      char buf[32];
      msg.toCharArray(buf, 32);
      radio.stopListening();
      radio.write(&buf, sizeof(buf));
      radio.startListening();
      lastSend = millis();
    }

    if (z <= HOMING_THRESHOLD_mT) {
      // Reached home
      stepper.setCurrentPosition(volumeToSteps(MAX_VOLUME_uL));
      remainingSteps = volumeToSteps(MAX_VOLUME_uL);
      EEPROM.put(0, remainingSteps);
      sendRemainingVolume();
      break;
    }
  }

  // Restore normal stepper acceleration
  stepper.setMaxSpeed(STEPPER_MAX_SPEED);
  stepper.setAcceleration(STEPPER_ACCEL);
}


void pumpOFF() {
  pumpActive = false;
  digitalWrite(SLP_PIN, LOW); // disable motor
  // radio remains listening to receive 'ON'
}

void pumpON() {
  pumpActive = true;
  digitalWrite(SLP_PIN, HIGH); // enable motor
}

// ================= SETUP ======================
void setup() {
  pinMode(SLP_PIN, OUTPUT);
  digitalWrite(SLP_PIN, HIGH); // enable driver

  pinMode(BUTTON_FWD, INPUT_PULLDOWN);
  pinMode(BUTTON_REV, INPUT_PULLDOWN);

  // Stepper setup
  stepper.setMaxSpeed(STEPPER_MAX_SPEED);
  stepper.setAcceleration(STEPPER_ACCEL);

  // Radio setup
  radio.begin();
  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_1MBPS);
  radio.openReadingPipe(1, pipePCtoPump);
  radio.openWritingPipe(pipePumpToPC);
  radio.startListening();

  // Hall sensor
  Wire.begin();
  if (!hallSensor.begin()) {
    // if sensor not found, just continue; homing won't work
  }

  // Load remaining steps from EEPROM
  EEPROM.get(0, remainingSteps);
}

// ================= LOOP =======================
void loop() {
  // --- RADIO COMMANDS ---
  if (radio.available()) {
    char msg[32] = {0};
    radio.read(&msg, sizeof(msg));
    String command = String(msg);

    if (command == "FWD" && pumpActive) {
      stepper.move(STEP_SIZE_BUTTON);
      remainingSteps += STEP_SIZE_BUTTON;
      sendRemainingVolume();
    }
    else if (command == "REV" && pumpActive) {
      stepper.move(-STEP_SIZE_BUTTON);
      remainingSteps -= STEP_SIZE_BUTTON;
      if (remainingSteps < 0) remainingSteps = 0;
      sendRemainingVolume();
    }
    else if (command == "HOME" && pumpActive) homePump();
    else if (command == "OFF") pumpOFF();
    else if (command == "ON") pumpON();
    else if (command.toFloat() > 0 && pumpActive) { // Volume in μL
      float uL = command.toFloat();
      long stepsToMove = volumeToSteps(uL);
      if (stepsToMove > remainingSteps) stepsToMove = remainingSteps;
      stepper.move(stepsToMove); // FWD = dispense
      remainingSteps -= stepsToMove;
      sendRemainingVolume();
      EEPROM.put(0, remainingSteps);
    }

    // Send ACK back
    char reply[] = "ACK";
    radio.stopListening();
    radio.write(&reply, sizeof(reply));
    radio.startListening();
  }

  // --- BUTTONS ---
  unsigned long now = millis();
  if (digitalRead(BUTTON_FWD) && pumpActive && now - lastButtonTimeFWD > debounceDelay) {
    stepper.move(STEP_SIZE_BUTTON);
    remainingSteps += STEP_SIZE_BUTTON;
    sendRemainingVolume();
    lastButtonTimeFWD = now;
  }
  if (digitalRead(BUTTON_REV) && pumpActive && now - lastButtonTimeREV > debounceDelay) {
    stepper.move(-STEP_SIZE_BUTTON);
    remainingSteps -= STEP_SIZE_BUTTON;
    if (remainingSteps < 0) remainingSteps = 0;
    sendRemainingVolume();
    lastButtonTimeREV = now;
  }

  // --- RUN STEPPER ---
  stepper.run();
}
