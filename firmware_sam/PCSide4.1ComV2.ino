#include <SPI.h>
#include <RF24.h>

// ================= PARAMETERS =================
#define CE_PIN 14
#define CSN_PIN 15

const uint64_t PIPE_PC_TO_PUMP = 0xF0F0F0F0E1LL;
const uint64_t PIPE_PUMP_TO_PC = 0xF0F0F0F0D2LL;

// ================= RADIO ======================
RF24 radio(CE_PIN, CSN_PIN);

// ================= SETUP ======================
void setup() {
  Serial.begin(115200);

  radio.begin();
  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_1MBPS);
  radio.openReadingPipe(1, PIPE_PUMP_TO_PC);
  radio.openWritingPipe(PIPE_PC_TO_PUMP);
  radio.startListening();

  Serial.println("PC RF ready. Commands: FWD, REV, HOME, OFF, ON, or volume in uL.");
}

// ================= LOOP =======================
void loop() {
  // --- Serial input ---
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.length() > 0) {
      radio.stopListening();
      char msg[32];
      input.toCharArray(msg, sizeof(msg));
      if (radio.write(&msg, sizeof(msg))) {
        Serial.print("Sent: ");
        Serial.println(input);
      } else Serial.println("Send failed!");
      radio.startListening();
    }
  }

  // --- Receive pump messages ---
  if (radio.available()) {
    char recvMsg[32] = {0};
    radio.read(&recvMsg, sizeof(recvMsg));
    Serial.print("Pump: ");
    Serial.println(recvMsg);
  }
}
