
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

//#include <splash.h>
//#include <Adafruit_SPITFT.h>
//#include <Adafruit_SPITFT_Macros.h>
//#include <gfxfont.h>

Adafruit_SSD1306 display = Adafruit_SSD1306(128, 32, &Wire);

// OLED FeatherWing buttons map to different pins depending on board:
// 32u4, M0, M4, nrf52840 and 328p
#define BUTTON_A  9
#define BUTTON_B  6
#define BUTTON_C  5

#include <RH_RF95.h>

// for Feather32u4 RFM9x
#define RFM95_CS 8
#define RFM95_RST 4
#define RFM95_INT 7

// Change to 434.0 or other frequency, must match RX's freq!
#define RF95_FREQ 433.0

// Singleton instance of the radio driver
RH_RF95 rf95(RFM95_CS, RFM95_INT);

// Blinky on receipt
#define LED 13

unsigned long lastDisplay, lastRecv;

void setup() {

  pinMode(LED, OUTPUT);
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);

  Serial.begin(9600);

  Serial.println("Feather LoRa RX Test!");
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

  Serial.println("OLED FeatherWing test");
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // Address 0x3C for 128x32

  Serial.println("OLED begun");

  // Show image buffer on the display hardware.
  // Since the buffer is intialized with an Adafruit splashscreen
  // internally, this will display the splashscreen.
  display.display();
  delay(1000);

  // Clear the buffer.
  display.clearDisplay();
  display.display();

  Serial.println("IO test");

  pinMode(BUTTON_A, INPUT_PULLUP);
  pinMode(BUTTON_B, INPUT_PULLUP);
  pinMode(BUTTON_C, INPUT_PULLUP);

  // text display tests
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.print("Connecting to SSID\n'adafruit':");
  display.print("connected!");
  display.println("IP: 10.0.1.23");
  display.println("CIAO! wait until RX");
  display.setCursor(0,0);
  display.display(); // actually display all of the above
}

// is a header in the message to verify the message is received correctly
#define MAGIC_NUMBER_LEN 2
uint8_t MAGIC_NUMBER[MAGIC_NUMBER_LEN] = {0x02, 0xcb};

uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
int lastRSSI;
uint8_t count_alive;

// lat/lon are stored as signed 32-bit ints as millionths of a degree (-123.45678 => -123,456,780)
int32_t theirLat;
int32_t theirLon;
bool areTheyAccurate;

// timeout for RX --> if not receiving a signal in more than 20s --> error message
#define MAX_RX_INTERVAL 20000

void loop() {

  if (rf95.available())
  {
    // Should be a message for us now
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);

    long sinceLastRX = millis() - lastRecv;
    if (sinceLastRX > MAX_RX_INTERVAL) {
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(WHITE);      
        display.setCursor(0, 0);      
        display.println("OOPS noRX");                
        display.setCursor(0, 8);      
        display.println("Explore");       
        display.setCursor(0, 16);      
        display.println("and enjoy )'(");                      
        display.setCursor(0, 24);      
        display.println("the charriot will find u");                
        display.display();      
    } 
      
    if (rf95.recv(buf, &len))
    { 
      count_alive = count_alive + 1; 
      digitalWrite(LED, HIGH);
      RH_RF95::printBuffer("Received: ", buf, len);
      Serial.print("RSSI: ");
      Serial.println(rf95.lastRssi(), DEC);
      lastRSSI = rf95.lastRssi();
      
      // decode message
      for (int i = 0; i < MAGIC_NUMBER_LEN; i++) {
        if (MAGIC_NUMBER[i] != buf[i]) {       
          Serial.println("MAGIC NUMBER doesn't match!");
        }
      }
      void* p = buf + MAGIC_NUMBER_LEN;
      theirLat = *(int32_t*)p;
      p = (int32_t*)p + 1;
      theirLon = *(int32_t*)p;
      p = (int32_t*)p + 1;
      areTheyAccurate = *(uint8_t*)p;
      lastRecv = millis();
      
      // write display
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor(0, 0);
      display.println(fmtPlayaStr(theirLat, theirLon, areTheyAccurate));
      display.println();
      display.setCursor(0, 8);      
      display.println("count:" + String(count_alive) + "  " + String(lastRSSI) + "db");
      display.setCursor(0, 16);
      display.println("Lat:" + String(theirLat));           
      display.setCursor(0, 24);
      display.println("Lon:" + String(theirLon));                
      display.setCursor(0, 32);
      display.display();
               
      // Send a reply
      uint8_t data[] = "And hello back to you";
      rf95.send(data, sizeof(data));
      rf95.waitPacketSent();
      Serial.println("Sent a reply");
      digitalWrite(LED, LOW);

    }
    else
    {
      Serial.println("Receive failed");
    
      if (sinceLastRX > MAX_RX_INTERVAL) {
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(WHITE);      
        display.setCursor(0, 0);      
        display.println("OOPS noRX");                
        display.setCursor(0, 8);      
        display.println("Explore");       
        display.setCursor(0, 16);      
        display.println("and enjoy )'(");                      
        display.setCursor(0, 24);      
        display.println("the charriot will find u");                
        display.display();      
      } 
    }
  }  
}

String fmtPlayaStr(int32_t lat, int32_t lon, bool accurate) {
  if (lat == 0 && lon == 0) {
    return "404 charriot not found";
  } else {
    return playaStr(lat, lon, accurate);
  }
}

///// PLAYA COORDINATES CODE /////

// production - burning man
#define MAN_LAT 40786400
#define MAN_LON -119206500

#define PLAYA_ELEV 1190.  // m
#define SCALE 1.
#define DEG_PER_RAD (180. / 3.1415926535)
#define CLOCK_MINUTES (12 * 60)
#define METERS_PER_DEGREE (40030230. / 360.)
// Direction of north in clock units
//#define NORTH 10.5  // hours
//#define NUM_RINGS 13  // Esplanade through L
#define ESPLANADE_RADIUS (2500 * .3048)  // m
#define FIRST_BLOCK_DEPTH (440 * .3048)  // m
#define BLOCK_DEPTH (240 * .3048)  // m
// How far in from Esplanade to show distance relative to Esplanade rather than the man
#define ESPLANADE_INNER_BUFFER (250 * .3048)  // m
// Radial size on either side of 12 w/ no city streets
#define RADIAL_GAP 2.  // hours
// How far radially from edge of city to show distance relative to city streets
#define RADIAL_BUFFER .25  // hours

//// overrides for afrikaburn
#define NORTH 3.3333  // make 6ish approx line up with bearing 80 deg
#define NUM_RINGS 0  // only give distance relative to clan


// 0=man, 1=espl, 2=A, 3=B, ...
float ringRadius(int n) {
  if (n == 0) {
    return 0;
  } else if (n == 1) {
    return ESPLANADE_RADIUS;
  } else if (n == 2) {
    return ESPLANADE_RADIUS + FIRST_BLOCK_DEPTH;
  } else {
    return ESPLANADE_RADIUS + FIRST_BLOCK_DEPTH + (n - 2) * BLOCK_DEPTH;
  }
}

// Distance inward from ring 'n' to show distance relative to n vs. n-1
float ringInnerBuffer(int n) {
  if (n == 0) {
    return 0;
  } else if (n == 1) {
    return ESPLANADE_INNER_BUFFER;
  } else if (n == 2) {
    return .5 * FIRST_BLOCK_DEPTH;
  } else {
    return .5 * BLOCK_DEPTH;
  }
}

int getReferenceRing(float dist) {
  for (int n = NUM_RINGS; n > 0; n--) {
    Serial.println(n + ":" + String(ringRadius(n)) + " " + String(ringInnerBuffer(n)));
    if (ringRadius(n) - ringInnerBuffer(n) <= dist) {

      return n;
    }
  }
  return 0;
}

String getRefDisp(int n) {
  if (n == 0) {
    return ")(";
  } else if (n == 1) {
    return "Espl";
  } else {
    return String(char(int('A') + n - 2));
  }
}


String playaStr(int32_t lat, int32_t lon, bool accurate) {
  // Safe conversion to float w/o precision loss.
  float dlat = 1e-6 * (lat - MAN_LAT);
  float dlon = 1e-6 * (lon - MAN_LON);

  float m_dx = dlon * METERS_PER_DEGREE * cos(1e-6 * MAN_LAT / DEG_PER_RAD);
  float m_dy = dlat * METERS_PER_DEGREE;

  float dist = SCALE * sqrt(m_dx * m_dx + m_dy * m_dy);
  float bearing = DEG_PER_RAD * atan2(m_dx, m_dy);

  float clock_hours = (bearing / 360. * 12. + NORTH);
  int clock_minutes = (int)(clock_hours * 60 + .5);
  // Force into the range [0, CLOCK_MINUTES)
  clock_minutes = ((clock_minutes % CLOCK_MINUTES) + CLOCK_MINUTES) % CLOCK_MINUTES;

  int hour = clock_minutes / 60;
  int minute = clock_minutes % 60;
  String clock_disp = String(hour) + ":" + (minute < 10 ? "0" : "") + String(minute);

  int refRing;
  if (6 - abs(clock_minutes/60. - 6) < RADIAL_GAP - RADIAL_BUFFER) {
    refRing = 0;
  } else {
    refRing = getReferenceRing(dist);
  }
  float refDelta = dist - ringRadius(refRing);
  long refDeltaRounded = (long)(refDelta + .5);

  return clock_disp + " & " + getRefDisp(refRing) + (refDeltaRounded >= 0 ? "+" : "-") + String(refDeltaRounded < 0 ? -refDeltaRounded : refDeltaRounded) + "m" + (accurate ? "" : "-ish");
}
