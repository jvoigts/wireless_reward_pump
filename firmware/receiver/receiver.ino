// battery powered pump  reciever side
// 
// expects integer turn inputs
// motor shuts down after moves
//
// 2025 jakob voigts 
// voigtsj@janelia.hhmi.org

#include <AccelStepper.h>
#include <SPI.h>
#include <RH_NRF24.h>

#define GEAR_RATIO 100.0
#define MOT_DIR 3
#define MOT_STEP 2
#define MOT_SLP 1

#define BUTTON_A 22
#define BUTTON_B 21

RH_NRF24 nrf24(14, 15);  //
AccelStepper motor(AccelStepper::DRIVER, MOT_STEP, MOT_DIR);

int rotation_in = 0;
int is_running = 0;

// timers
elapsedMicros mainTimer;    // main loop
elapsedMillis serialTimer;  // serial comm
elapsedMillis running_timer;

int bytes2int(unsigned int hi, unsigned int lo) {
  unsigned int x;
  x = hi;      // fill high byte
  x = x << 8;  // move to high bits
  x |= lo;     // OR combine the lower half
  return x;
}

void setup() {
  Serial.begin(9600);  // USB is always 12 Mbit/sec
  pinMode(1, OUTPUT);

  pinMode(MOT_SLP, OUTPUT);
  pinMode(MOT_DIR, OUTPUT);
  pinMode(MOT_STEP, OUTPUT);

  pinMode(BUTTON_A,INPUT_PULLDOWN);
  pinMode(BUTTON_B,INPUT_PULLDOWN);

  digitalWriteFast(MOT_SLP, LOW);  // deactivate driver

    motor.setMaxSpeed(5000.0);
    motor.setAcceleration(1000.0);

  Serial.println("--- NRF setup");

  if (!nrf24.init())
    Serial.println("init failed");
  // Defaults after init are 2.402 GHz (channel 2), 2Mbps, 0dBm
  if (!nrf24.setChannel(1))
    Serial.println("setChannel failed");
  if (!nrf24.setRF(RH_NRF24::DataRate2Mbps, RH_NRF24::TransmitPower0dBm))
    Serial.println("setRF failed");

  Serial.println("done with setup");
}



void loop() {
  // non-timed logic

    motor.run();
  
  if (mainTimer > 1000 * 10) {  // set frequency in usec --------
    mainTimer = 0;

    if (digitalRead(BUTTON_A)==HIGH) {
        digitalWriteFast(MOT_SLP, HIGH);  // activate driver
             running_timer=0;
        motor.move(-1000);
    }
    if (digitalRead(BUTTON_B)==HIGH) {
        digitalWriteFast(MOT_SLP, HIGH);  // activate driver
             running_timer=0;
        motor.move(1000);
        
    }

    if (motor.isRunning()) {
     running_timer=0;
    }

    if (running_timer>500) { // turn off motor 500ms afetr move
           digitalWriteFast(MOT_SLP, LOW);  // Deactivate driver
    } else {
           digitalWriteFast(MOT_SLP, HIGH);  //keep active
    }

    if (nrf24.available()) {
      uint8_t buf[RH_NRF24_MAX_MESSAGE_LEN];
      uint8_t len = sizeof(buf);
      if (nrf24.recv(buf, &len)) {
        rotation_in = bytes2int(buf[0], buf[1])-1000; // encode negative integer turns as values <1000

        if (rotation_in != 0) { // ignore 0 inputs, dont even power on motor
          //digitalWrite(1, HIGH);
          //delayMicroseconds(5000);
          //digitalWrite(1, LOW);
          Serial.println(rotation_in);
          digitalWriteFast(MOT_SLP, HIGH);  // activate driver
          motor.move(-rotation_in*GEAR_RATIO);
          // 4x microstepping (MS2 is pulled high)
          // gear ratio from he
          // got 2oo steps/rev on motor
        }
      } else {
        Serial.println("recv failed");
      }
    }

    if (serialTimer > 10)  // send data to host PC and, if in off/config state, check for config inputs
    {
      serialTimer = 0;
    }
  }
}