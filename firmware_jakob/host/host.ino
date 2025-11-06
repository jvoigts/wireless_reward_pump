// battery powered commutator host  side
// for outdoor arena
// 
// sends  integer turn inputs, need to pre-process that from bonsai
// sends at 1k offset to encode negative turns
//
// 2023 jakob voigts 
// voigtsj@janelia.hhmi.org

#include <SPI.h>
#include <RH_NRF24.h>

// Singleton instance of the radio driver
RH_NRF24 nrf24(14, 15);  //


uint8_t cmd;
uint8_t arg_hi;
uint8_t arg_lo;

int led_on;

int count = 0;

elapsedMillis main_timer;

int bytes2int(unsigned int hi, unsigned int lo) {
  unsigned int x;
  x = hi;      // fill high byte
  x = x << 8;  // move to high bits
  x |= lo;     // OR combine the lower half
  return x;
}

void setup() {
  Serial.begin(9600);
  delay(500);
  Serial.println("host startup");
  pinMode(13, OUTPUT);  // blinking LED
  // while (!Serial)
  //   ; // wait for serial port to connect. Needed for Leonardo only
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


  if (Serial.available()) {
    int turns = Serial.parseInt();
    Serial.println(turns);
    turns = turns + 1000;  // negative tunrs are sent as values <1k, 1000 means no turn
    uint8_t data[] = { highByte(turns), lowByte(turns) };
    nrf24.send(data, sizeof(data));
  }
}
