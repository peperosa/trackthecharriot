#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define BUTTON_A  9
#define BUTTON_B  6
#define BUTTON_C  5

Adafruit_SSD1306 display = Adafruit_SSD1306(128, 32, &Wire);

void setup() {
  Serial.begin(9600);

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // Address 0x3C for 128x32

  pinMode(BUTTON_A, INPUT_PULLUP);
  pinMode(BUTTON_B, INPUT_PULLUP);
  pinMode(BUTTON_C, INPUT_PULLUP);

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("Hello world");
  display.display();
}

void loop() {
  if (!digitalRead(BUTTON_A)) display.print("A");
  if (!digitalRead(BUTTON_B)) display.print("B");
  if (!digitalRead(BUTTON_C)) display.print("C");
  delay(10);
  yield();
  display.display();
}
