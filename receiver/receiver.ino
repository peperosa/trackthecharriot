#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_SleepyDog.h>
#include <Chariot.h>
#include "bitmaps.h"

Adafruit_SSD1306 display = Adafruit_SSD1306(128, 32, &Wire);

#define AUTO_OFF_DELAY  5*60*1000 // (milliseconds) - how long to wait before turning the screen off
#define MAP_HEIGHT     (sizeof(bm_map) / 16)  // pixels
#define SCREEN_HEIGHT  32  // pixels
#define SCROLL_SPEED   4   // How many pixels we scroll the map on each button press
#define MAN_X          64  // Coordinates of the Man in the map, in pixels
#define MAN_Y          76

int y_pos = 76;  // Current scrolling position

uint32_t last_interaction_time = 0; // (milliseconds) When we last pressed any button. Used for auto-off.
bool is_on = true;

// Flags to check if a button was pressed
#define BTN_UP_PRESS         1 << 0
#define BTN_UP_LONG_PRESS    1 << 1
#define BTN_DN_PRESS         1 << 2
#define BTN_DN_LONG_PRESS    1 << 3

#define NUM_BUTTONS   2
#define LONG_PRESS_DURATION  3000

uint32_t btn_start_press_time[NUM_BUTTONS] = {0, 0};  // Keeps track of when we started pressing each button
uint8_t btn_pins[NUM_BUTTONS] = {9, 5};  // Pin for each button


void setup() {
  Serial.begin(9600);
  delay(100);
  Serial.println("Track The Chariot!");

  // Initialize the display
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.display();

  initRadio(RECEIVER_ID);

  pinMode(btn_pins[0], INPUT_PULLUP);
  pinMode(btn_pins[1], INPUT_PULLUP);
}

void loop() {
  is_on ? onLoop() : offLoop();
}

// Main loop when we're powered off
void offLoop() {
  uint8_t btn_status = checkButtons();
  if (btn_status & BTN_UP_LONG_PRESS) {
    last_interaction_time = millis();
    turnOn();
    return;
  }

  delay(300);  // TODO: Need use proper low power sleep
  //Watchdog.sleep(300); // low power, deep sleep NOTE: BROKEN, never wakes? Or maybe it does but display stays blank
}

// Main loop when we're powered on
void onLoop() {
  uint8_t btn_status = checkButtons();

  // If any button is pressed, update the last interaction time to now
  if (btn_status) {
    last_interaction_time = millis();
  }
  if (btn_status & BTN_UP_PRESS) {
    y_pos = max(y_pos - SCROLL_SPEED, 0);
  }
  if (btn_status & BTN_DN_PRESS) {
    y_pos = min(y_pos + SCROLL_SPEED, MAP_HEIGHT - 32);
  }
  if (btn_status & BTN_UP_LONG_PRESS || millis() > last_interaction_time + AUTO_OFF_DELAY) {
    turnOff();
    return;
  }

  receive();
  updateDisplay();
}

uint8_t checkButtons() {
  uint8_t flags = 0;
  flags |= checkOneButton(0) << 0;
  flags |= checkOneButton(1) << 2;
  return flags;
}

// Returns 00b if the button isn't pressed at all, 01b if it's pressed and 11b (ie: 3) if long pressed
uint8_t checkOneButton(uint8_t index) {
  bool pressed = !digitalRead(btn_pins[index]); // "not" because they're pulled up, so the pin is high when not pressed
  if (pressed) {
    // If we just started pressing, record the start time
    if (btn_start_press_time[index] == 0) {
      btn_start_press_time[index] = millis();
    }
    if  (millis() - btn_start_press_time[index] > LONG_PRESS_DURATION) {
      btn_start_press_time[index] = 0;
      return B00000011;
    } else {
      return B00000001;
    }
  } else {
    // Button not pressed, reset start time to 0
    btn_start_press_time[index] = 0;
    return 0;
  }
}

bool receive()
{
  if (!rf95.available()) {
    return false;
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
      Serial.println("Discarding unknown message");
      return false;
    }

    Serial.print("RECEIVED ");
    Serial.print(payload.lat);
    Serial.print(", ");
    Serial.print(payload.lon);
    Serial.print(" - age: ");
    Serial.print(payload.fix_age_minutes);
    Serial.print(" - rssi: ");
    Serial.println(rf95.lastRssi(), DEC);
  }
}

void updateDisplay() {
  display.clearDisplay();
  display.drawBitmap(0, 0, bm_map + y_pos * 16, 128, 32, WHITE);
  drawTarget(64, 50);
  display.display();
}

// Wake up all peripherals
void turnOn() {
  if (is_on) {
    return;
  }
  rf95.setModeRx();
  display.ssd1306_command(SSD1306_DISPLAYON);
  is_on = true;
}

// Put all peripherals to sleep
void turnOff() {
  if (!is_on) {
    return;
  }
  Serial.println("Turning off. Serial might break");
  rf95.sleep();
  display.ssd1306_command(SSD1306_DISPLAYOFF);
  is_on = false;
  delay(1000); // Give it some time to debounce the buttons and make sure everything settles down
}


/**
 * Draws an animated circle at pixel coordinates (x, y)
 * If the circle would not be visible (because the screen is scrolled too far up or down)
 * draw a blinking arrow instead
 */
void drawTarget(int x, int y) {
  // Take the time in milliseconds, modulo 512. This is used for the animation.
  int t = 512 - (millis() % 512);

  if (y_pos > y + 7) {
    // Target is too high, draw a blinking up arrow
    if (t > 256) { display.drawBitmap(61, 0, up_arrow, 8, 8, WHITE); }
  } else if (y_pos + SCREEN_HEIGHT < y - 7) {
    // Target is too low, draw a blinking down arrow
    if (t > 256) { display.drawBitmap(61, SCREEN_HEIGHT - 8, dn_arrow, 8, 8, WHITE); }
  } else {
    // Draw 2 animated circles converging to the target
    display.drawCircle(x, y - y_pos, t / 32, WHITE);
    display.drawCircle(x, y - y_pos, t / 64, WHITE);
  }
}
