#include <SPI.h>
#include <TinyGPS.h>
#include "common.h"

#define GPSSerial Serial1

TinyGPS gps;

Payload payload;        // the data that we are going to send over the radio
uint32_t last_fix = 0;  // timestamp in ms when we received the last GPS fix
uint32_t last_sent = 0; // timestamp in ms when we last sent the data

#define TRANSMIT_INTERVAL 5000  // interval in ms between sending data

void setup() 
{
  Serial.begin(9600);
  while (!Serial) {
    delay(1);
  }
  Serial.println("Track The Chariot - transmitter");

  initRadio();

  // 9600 baud is the default rate for the GPS
  GPSSerial.begin(9600);
}

void loop() {
  readGPS();

  if (millis() - last_sent > TRANSMIT_INTERVAL) {
    transmitData();
  }
}

void readGPS() {
  // Read the next character from the GPS serial port and pass it to TinyGPS for decoding
  if (!GPSSerial.available() || !gps.encode(GPSSerial.read())) {
    return;  // We don't have a full sentence yet
  }

  int32_t lat, lon;
  unsigned long age;
  gps.get_position(&lat, &lon, &age);
  if (lat == TinyGPS::GPS_INVALID_ANGLE || lon == TinyGPS::GPS_INVALID_ANGLE || age == TinyGPS::GPS_INVALID_AGE) {
    return;
  }

  // Check the accuracy of the position
  if (gps.hdop() != TinyGPS::GPS_INVALID_HDOP && gps.hdop() > 2000) {
    // According to https://en.wikipedia.org/wiki/Dilution_of_precision_%28navigation%29, a DOP value > 20 is wildly
    // inaccurate and should be discarded. (Note that TinyGPS returns the DOP value in 1/100th).
    return;
  }

  last_fix = millis() - age;
  payload.lat = lat / 10 - MAN_LAT;
  payload.lon = lon / 10 - MAN_LON;
}

void transmitData() {
  // Compute the fix age in minutes, cap it to 0xff if it's greater than that.
  payload.fix_age_minutes = last_fix > 0 ? min(0xff, (millis() - last_fix) / 60000) : 0xff;

  rf95.send((uint8_t*) &payload, sizeof(payload));
  RH_RF95::printBuffer("Sending: ", (uint8_t*) &payload, sizeof(payload));
  rf95.waitPacketSent();
  last_sent = millis();
}
