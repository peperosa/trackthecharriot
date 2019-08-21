// GPS receiver + Feather9x_TX

#include <SPI.h>
#include <RH_RF95.h>

// for feather32u4 
#define RFM95_CS 8
#define RFM95_RST 4
#define RFM95_INT 7

// Change to 434.0 or other frequency, must match RX's freq!
#define RF95_FREQ 433.0

// Singleton instance of the radio driver
RH_RF95 rf95(RFM95_CS, RFM95_INT);

// timing
#define TRANSMIT_INTERVAL 5000      // interval between sending updates
#define MAX_FIX_AGE 5000   // Ignore data from GPS if older than this
unsigned long lastSend, lastFix, lastGPSserial;
bool sending = false;

// 95% error radius at HDOP=1
#define GPS_BASE_ACCURACY 6.2  // m

#define ACCURACY_THRESHOLD 30  // m


void setup() 
{
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);

  Serial.begin(9600);
  while (!Serial) {
    delay(1);
  }

  delay(100);

  Serial.println("Feather LoRa TX Test!");

  // manual reset
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);

  while (!rf95.init()) {
    Serial.println("LoRa radio init failed");
    Serial.println("Uncomment '#define SERIAL_DEBUG' in RH_RF95.cpp for detailed debug info");
    while (1);
  }
  Serial.println("LoRa radio init OK!");

  // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM
  if (!rf95.setFrequency(RF95_FREQ)) {
    Serial.println("setFrequency failed");
    while (1);
  }
  Serial.print("Set Freq to: "); Serial.println(RF95_FREQ);
  
  // Defaults after init are 434.0MHz, 13dBm, Bw = 125 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on

  // The default transmitter power is 13dBm, using PA_BOOST.
  // If you are using RFM95/96/97/98 modules which uses the PA_BOOST transmitter pin, then 
  // you can set transmitter powers from 5 to 23 dBm:
  rf95.setTxPower(23, false);
  
}

#define MAGIC_NUMBER_LEN 2
uint8_t MAGIC_NUMBER[MAGIC_NUMBER_LEN] = {0x02, 0xcb};

//String timeStr = "";
uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
int lastRSSI;

// lat/lon are stored as signed 32-bit ints as millionths of a degree (-123.45678 => -123,456,780)
int32_t myLat;
int32_t myLon;
float myElev;  // unused
float myHAcc;
bool amIAccurate;
int32_t theirLat;
int32_t theirLon;
float theirElev;  // unused
bool areTheyAccurate;

//int16_t packetnum = 0;  // packet counter, we increment per xmission

void transmitData() {
  long sinceLastFix = millis() - lastFix;
  if (sinceLastFix > MAX_FIX_AGE) {
    // GPS data is stale
    return;
  }

  uint8_t len = 2 * sizeof(int32_t) + sizeof(uint8_t) + MAGIC_NUMBER_LEN + 1;
  uint8_t radiopacket[len];
  for (int i = 0; i < MAGIC_NUMBER_LEN; i++) {
    radiopacket[i] = MAGIC_NUMBER[i];
  }
  void* p = radiopacket + MAGIC_NUMBER_LEN;
  *(int32_t*)p = myLat;
  p = (int32_t*)p + 1;
  *(int32_t*)p = myLon;
  p = (int32_t*)p + 1;
  *(uint8_t*)p = amIAccurate;
  radiopacket[len - 1] = '\0';

  sending = true;  
  rf95.send((uint8_t *)radiopacket, len);
  RH_RF95::printBuffer("Sending: ", radiopacket, len);
  rf95.waitPacketSent();
  
  Serial.println("Waiting for reply...");
  if (rf95.waitAvailableTimeout(1000))
  { 
    // Should be a reply message for us now   
    if (rf95.recv(buf, &len))
     {
        RH_RF95::printBuffer("Got reply: ", buf, len);
        //Serial.print("Got reply: ");
        //Serial.println((char*)buf);
        Serial.print("RSSI: ");
        Serial.println(rf95.lastRssi(), DEC);    
      }
      else
      {
        Serial.println("Receive failed");
      }
    }
    else
    {
      Serial.println("No reply, is there a listener around?");
    }
  sending = false;
  lastSend = millis();
}

// production - burning man
#define MAN_LAT 40786400
#define MAN_LON -119206500
#define PLAYA_ELEV 1190.  // m
#define SCALE 1.

void setFix () {
  
  // TEST MODE
  myLat = MAN_LAT; // - 200*(1e-3*millis());
  myLon = MAN_LON; // - 200*(1e-3*millis());
  //Serial.println(String(myLat, 6) + " " + String(myLon, 6));
  // this is what I get 4014105532 1530205013524
  // sent over RADIO --> 2 CB E0 59 6E 2 9C D E5 F8 1 0 

  amIAccurate = true;
  lastFix = millis();
  return;
  
  /*
  //  REAL MODE
  int32_t lat, lon;
  unsigned long age;
  gps.get_position(&lat, &lon, &age);
  if (age == TinyGPS::GPS_INVALID_AGE) {
    return;
  }
  lastFix = millis() - age;

  if (lat == TinyGPS::GPS_INVALID_ANGLE || lon == TinyGPS::GPS_INVALID_ANGLE) {
    lat = 0;
    lon = 0;
  }
  //Serial.println(String(lat, 6) + " " + String(lon, 6));
  myLat = lat;
  myLon = lon;

  if (gps.hdop() == TinyGPS::GPS_INVALID_HDOP) {
    myHAcc = -1;
  } else {
    myHAcc = 1e-2 * gps.hdop() * GPS_BASE_ACCURACY;
  }
  amIAccurate = (myHAcc > 0 && myHAcc <= ACCURACY_THRESHOLD);
  */
}

void loop() {
  if (Serial1.available()) {
    char c = Serial1.read();
    long sinceLastGPSserial = millis() - lastGPSserial;
    Serial.write(c);
    if (gps.encode(c)) { // Did a new valid sentence come in?      
      setFix();
    }
  }

  long sinceLastTransmit = millis() - lastSend;
  if (sinceLastTransmit < 0 || sinceLastTransmit > TRANSMIT_INTERVAL) {    
    transmitData();
  }

}
