#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Chariot.h>
#include "bitmaps.h"

Adafruit_SSD1306 display = Adafruit_SSD1306(128, 32, &Wire);

#define BUTTON_B  6
#define BUTTON_C  5
#define MAP_HEIGHT     (sizeof(bm_map) / 16)  // pixels
#define SCREEN_HEIGHT  32  // pixels
#define SCROLL_SPEED   4   // How many pixels we scroll the map on each button press
#define MAX_DIST       10000  // (feet) Maximum distance to draw the target.
#define SECONDS        1000
#define MINUTES        60000

// Radius of the Earth at the latitude of the Man, and 3,904 feet elevation - https://rechneronline.de/earth-radius/
#define EARTH_RADIUS  20895853  // in feet (6,369,056 meters)

// Distance in feet between the Man and the center of each road
// Source: https://bm-innovate.s3.amazonaws.com/2023/2023%20BRC%20Measurements.pdf
int16_t roads[]   = {2500, 2940, 3230, 3520, 3810, 4100, 4590, 4880, 5170, 5460, 5650, 5845};
char road_names[] = {'E',  'A',  'B',  'C',  'D',  'E',  'F',  'G',  'H',  'I',  'J',  'K'};
#define NUM_ROADS   (sizeof(roads) / sizeof(roads[0]))
#define ROAD_WIDTH  40

int16_t y_pos = MAN_Y - SCREEN_HEIGHT/2;  // (pixels) Current scrolling position. Initialize it at the center.
int32_t fix_time = 0;  // Timestamp when we got the GPS fix. 0 if no valid fix received. Note: this might be negative.
uint32_t receive_time = 0; // when we received the last message
int32_t char_lat = 0;  // latitude of the chariot, in millionths of degrees
int32_t char_lon = 0;  // longitude of the chariot, in millionths of degrees
float char_dist = 0;    // (feet) distance of the chariot from the man
float char_angle = 0;   // (radians) angle of the chariot relative to the man. 0 is at 3 o'clock, and it goes counter-clockwise

void setup() {

  Serial.begin(9600);

  pinMode(BUTTON_B, INPUT_PULLUP);
  pinMode(BUTTON_C, INPUT_PULLUP);

  // Initialize the display
  delay(1000); // needed most fo the time for the display to init
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(40, 15);
  display.println("BRC 2023!");
  display.display();

  delay(500); // Short delay to see the text

  initRadio(RECEIVER_ID);
}

void loop() {
  if(!digitalRead(BUTTON_B)) {
    y_pos = max(y_pos - SCROLL_SPEED, 0);
  }
  if(!digitalRead(BUTTON_C)) {
    y_pos = min(y_pos + SCROLL_SPEED, MAP_HEIGHT);
  }

  receive();
  updateDisplay();
}

void receive()
{
  if (!rf95.available()) {
    return;
  }

  Payload payload;
  uint8_t len = sizeof(payload);

  // Note: if the received message is larger than `len` (for example because we received someone else's message),
  // recv() will simply discard the unknown message and return true, without changing the buffer or len.
  // This is good: we don't have to worry about getting stuck because we received a random message that we can't read.
  if (rf95.recv((uint8_t*) &payload, &len)) {
    // Make sure this is the right kind of message
    if (len != sizeof(Payload)
    || rf95.headerFrom() != SENDER_ID
    || rf95.headerTo() != RECEIVER_ID
    || rf95.headerId() != MESSAGE_ID) {
      return;
    }

    char_lat = payload.lat;
    char_lon = payload.lon;

    fix_time = (payload.fix_age_minutes != INVALID_FIX_AGE)
        ? millis() - (int32_t)payload.fix_age_minutes * MINUTES
        : 0;

    receive_time = millis();

    updateDistanceAndAngle();
  }
}

// Converts the chariot GPS coordinates into a distance and angle relative to the Man
void updateDistanceAndAngle() {
# define RAD(x) ((x)*PI/180000000.0)  // Converts from millionths of degrees to radians
  float x = RAD(char_lon - MAN_LON) * cos(RAD((char_lat + MAN_LAT) / 2));
  float y = RAD(char_lat - MAN_LAT);
# undef TO_RAD

  char_dist = sqrt(x*x + y*y) * EARTH_RADIUS;
  char_angle = atan2(y, x) + MAP_ANGLE;
}


void updateDisplay() {
  display.clearDisplay();

  // Draw the map, if we're not scrolled past it
  if (y_pos < MAP_HEIGHT) {
    display.drawBitmap(0, 0, bm_map + y_pos * 16, 128, min(32, MAP_HEIGHT - y_pos), WHITE);
  }

  bool valid_fix = isValidFix();

  // Draw the text, if visible
  if (y_pos > MAP_HEIGHT - SCREEN_HEIGHT) {
    display.setCursor(0, MAP_HEIGHT - y_pos + 5);
    printBatteryAndSignal();
    display.println();
    if (valid_fix) {
      printClockAddress();
      printFixAge();
      display.println();
      display.print(char_lat);
      display.print(", ");
      display.print(char_lon);
    } else {
      display.print("Waiting for GPS");
    }
  }

  // Draw the target if it's not too far away
  if (char_dist < MAX_DIST && valid_fix) {
    int16_t x = MAN_X + round(cos(char_angle) * char_dist / FEET_PER_PIXEL);
    int16_t y = MAN_Y - round(sin(char_angle) * char_dist / FEET_PER_PIXEL);
    drawTarget(x, y);
  }

  display.display();
}

// If the fix is older than 30 minutes, consider it invalid
bool isValidFix() {
  return fix_time != 0 && (int32_t)millis() - fix_time < 30 * MINUTES;
}

/**
 * Draws an animated circle at pixel coordinates (x, y)
 * If the circle would not be visible (because the screen is scrolled too far up or down)
 * draw a blinking arrow instead
 */
void drawTarget(int16_t x, int16_t y) {
  // Take the time in milliseconds, modulo 512. This is used for the animation.
  int16_t t = 512 - (millis() % 512);

  if (y_pos > y + 7) {
    // Target is too high, draw a blinking up arrow
    if (t > 256) { display.drawBitmap(61, 0, up_arrow, 8, 8, WHITE); }
  } else if (y_pos + SCREEN_HEIGHT < y - 7) {
    // Target is too low, draw a blinking down arrow
    if (t > 256) { display.drawBitmap(61, SCREEN_HEIGHT - 8, dn_arrow, 8, 8, WHITE); }
  } else {
    // Draw 2 animated circles converging to the target
    display.drawCircle(x, y - y_pos, t / 20, WHITE);
    display.drawCircle(x, y - y_pos, t / 60, WHITE);
  }
}

/**
 * Prints the chariot angle as a clock address (like 4:31)
 */
void printClockAddress() {
  // There's 12*60 = 720 minutes around the clock, so convert the angle from radians to [0-720]
  int16_t a = (int16_t)round((720 + 180 - char_angle * 360 / PI)) % 720;
  int8_t hour = a / 60;
  int8_t min = a % 60;

  // Print the radial street
  hour == 0 ? display.print(12) : display.print(hour);
  display.print(':');
  if (min < 10) {  // add leading 0
    display.print(0);
  }
  display.print(min);

  // Print the annular, if we're between 1:50 and 10:10
  if (a > 110 && a < 610) {
    printRoad();
  } else {
    if (char_dist > roads[NUM_ROADS - 1]) {
      display.print(" deep playa");
    }
  }
}

// Prints the name of the annular road we're on
// Note: we have max 10 chars for this
void printRoad() {
  int16_t dist = (int16_t) char_dist;

  // If we're in the inner playa, don't print anything
  if (dist < roads[0] - ROAD_WIDTH ) {
    return;
  }

  for (uint8_t i = 0; i < NUM_ROADS; i++) {
    // On the road
    if (abs(dist - roads[i]) < ROAD_WIDTH) {
      display.print(" & ");
      printRoadName(i);
      return;
    }
    // between two roads
    if (dist < roads[i]) {
      display.print(" btw ");
      printRoadName(i - 1);
      display.print(" & ");
      display.print(road_names[i]);  // no need to call printRoadName since we know it cannot be Esplanade
      return;
    }
  }
  display.print(" beyond ");
  display.print(road_names[NUM_ROADS - 1]);
}

void printRoadName(uint8_t index) {
  if (index == 0) {
    display.print("Esp");
  } else {
    display.print(road_names[index]);
  }
}

void printFixAge() {
  int16_t age_minutes = (millis() - fix_time) / MINUTES;
  display.print(' ');
  display.print(age_minutes);
  display.print('m');
}

void printBatteryAndSignal() {
  // Signal level
  if (receive_time != 0 && millis() - receive_time < 20 * SECONDS) {
    display.print("Signal: ");
    display.print(100 + rf95.lastRssi()); // add 100 to convert from negative to positive range, which is more intuitive
  } else {
    display.print("No signal.");
  }

  // Battery level
  display.print(" Bat: ");
  float vbat = analogRead(A9) * 2 * 3.3 / 1024;
  // print battery as a percentage. Note: it's probably not super accurate
  int8_t percent = (int8_t)((vbat - 3.2) * 100);
  display.print(min(max(1, percent) , 100));
  display.print('%');
  // print volts:
  //display.print(vbat, 1);
  //display.print('V');
}
