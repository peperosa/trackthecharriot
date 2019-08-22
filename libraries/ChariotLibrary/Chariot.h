// This file contains common code to both the transmitter and the receiver

#ifndef COMMON_H
#define COMMON_H

#include <RH_RF95.h>

#define SENDER_ID      0x3f
#define RECEIVER_ID    0x58
#define MESSAGE_ID     0xad


// The data that we are going to send over the radio
struct Payload {
  // Latitude and longitude of the GPS fix in millionths of a degree (-123.45678 => -123,456,780)
  int32_t lat = 0;
  int32_t lon = 0;

  // How old the GPS fix is, in minutes. 1 byte, so max age is 255 minutes (~4h)
  // This is set to 255 if we haven't gotten a fix yet or if it's more than 255 minutes.
  uint8_t fix_age_minutes = 0xff;
};

// Radio settings
#define RFM95_CS 8
#define RFM95_RST 4
#define RFM95_INT 7
#define RADIO_FREQUENCY 433.0

// Singleton instance of the radio driver
RH_RF95 rf95(RFM95_CS, RFM95_INT);

// Enters an infinite loop, blinking the LED quickly to indicate an error
void die() {
  pinMode(13, OUTPUT);
  while (1) {
    digitalWrite(13, HIGH); delay(100);
    digitalWrite(13, LOW);  delay(100);
  }
}

// Initializes the radio
void initRadio(uint8_t address) {
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);

  // manual reset
  digitalWrite(RFM95_RST, LOW);  delay(10);
  digitalWrite(RFM95_RST, HIGH); delay(10);

  if (!rf95.init()) {
    Serial.println("LoRa radio init failed");
    Serial.println("Uncomment '#define SERIAL_DEBUG' in RH_RF95.cpp for detailed debug info");
    die();
  }
  Serial.println("LoRa radio init OK!");

  // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, 13dBm, Bw = 125 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on

  if (!rf95.setFrequency(RADIO_FREQUENCY)) {
    Serial.println("setFrequency failed");
    die();
  }
  Serial.print("Set Freq to: ");
  Serial.println(RADIO_FREQUENCY);

  // The default transmitter power is 13dBm, using PA_BOOST.
  // If you are using RFM95/96/97/98 modules which uses the PA_BOOST transmitter pin, then
  // you can set transmitter powers from 5 to 23 dBm:
  //rf95.setTxPower(23, false);

  rf95.setThisAddress(address);
  rf95.setPromiscuous(false);
}

#endif //COMMON_H
