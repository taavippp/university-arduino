// Piezo speaker
#include <Tone.h>
// NeoPixel
#include <Adafruit_NeoPixel.h>
// Screen
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// 2-dimensional vector struct for holding and managing data comfortably. The 'x' and 'y' properties can also mark the left and right sides of the sensor.
struct Vec2 {
  int x;
  int y;
};

// Struct that holds information about the different stages of the sensor
// fromDistance - what distance this stage begins from (cm)
// color - what color the new lit lights will be
// note - what tone the pieso will play
// noteDuration - how long the pieso will play/pause for
struct DistanceMapEntry {
  float fromDistance;
  uint32_t color;
  int note;
  long noteDuration;
};


// Constant values
const float IR_SPEED = 0.034; // Speed of sound
const long DELAY = 50; // Time in millis between loop()

// Reversing sensor different stages
const struct DistanceMapEntry DISTANCE_MAP[] = {
  {40, Adafruit_NeoPixel::Color(0, 255, 0), NOTE_A3, 500},
  {30, Adafruit_NeoPixel::Color(127, 255, 0), NOTE_A4, 350},
  {20, Adafruit_NeoPixel::Color(255, 127, 0), NOTE_A5, 250},
  {10, Adafruit_NeoPixel::Color(255, 0, 0), NOTE_A6, 150},
};


// Pins that the sensor uses
const struct Vec2 PIN_TRIGGER = {8, 2}; // HC-SR04
const struct Vec2 PIN_ECHO = {9, 3}; // HC-SR04
const struct Vec2 PIN_LED = {10, 4}; // NeoPixel
const int PIN_PIEZO = 7; // Piezo speaker


// NeoPixel variables
const Adafruit_NeoPixel ledLeft(8, PIN_LED.x, NEO_GRB + NEO_KHZ800);
const Adafruit_NeoPixel ledRight(8, PIN_LED.y, NEO_GRB + NEO_KHZ800);


// Screen variables
const struct Vec2 SCREEN = {128, 32};
const int SCREEN_ADDRESS = 0x3C;
const Adafruit_SSD1306 display(SCREEN.x, SCREEN.y, &Wire, -1);


// Piezo variables
Tone piezo;
int note;
long noteDuration = DISTANCE_MAP[0].noteDuration;
// Time when piezo state changed
long noteMillis;


// Distances measured by the IR sensors
struct Vec2 distance;


void setup() {
  Serial.begin(9600);

  // Turn screen on
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("Connection to display failed.");
  }
  display.display();
  
  pinMode(PIN_TRIGGER.x, OUTPUT);
  pinMode(PIN_TRIGGER.y, OUTPUT);
  pinMode(PIN_ECHO.x, INPUT);
  pinMode(PIN_ECHO.y, INPUT);
  pinMode(PIN_LED.x, OUTPUT);
  pinMode(PIN_LED.y, OUTPUT);

  // turn piezo speaker on
  piezo.begin(PIN_PIEZO);

  // turn lights on
  ledLeft.begin();
  ledRight.begin();
  ledLeft.setBrightness(65);
  ledRight.setBrightness(65);
  ledLeft.show();
  ledRight.show();

  // timing the piezo speaker
  noteMillis = millis();
}


void loop() {
  delay(DELAY);
  // clears the display and lights so they can be changed again
  display.clearDisplay();
  ledLeft.clear();
  ledRight.clear();

  // IR sensor input
  distance.x = durationToDistance(activateTrigger(true));
  distance.y = durationToDistance(activateTrigger(false));

  // smallest of the two distances
  int minDistance = min(distance.x, distance.y);

  for (int i = 0; i < 4; i++) {
    DistanceMapEntry e = DISTANCE_MAP[i];
    // NeoPixel and screen logic
    // Left side
    if (distance.x <= e.fromDistance) {
      ledLeft.setPixelColor(i * 2, e.color);
      ledLeft.setPixelColor((i * 2) + 1, e.color);
      // Draws an obstacle to the left side of the car
      display.drawTriangle(
        0, 0,
        (i + 1) * (SCREEN.x / 8), 0,
        0, (i + 1) * (SCREEN.y / 4),
        SSD1306_WHITE
      );
    }
    // Right side
    if (distance.y <= e.fromDistance) {
      ledRight.setPixelColor(i * 2, e.color);
      ledRight.setPixelColor((i * 2) + 1, e.color);
      // Draws an obstacle to the right side of the car
      display.drawTriangle(
        SCREEN.x, 0,
        SCREEN.x - ((i + 1) * SCREEN.x / 8), 0,
        SCREEN.x, (i + 1) * (SCREEN.y / 4),
        SSD1306_WHITE
      );
    }

    // Piezo speaker logic
    if (minDistance <= e.fromDistance && minDistance >= e.fromDistance - 10) {
      noteDuration = e.noteDuration;
      note = e.note;
    }
  }
  
  // If the smallest distance is above 40cm, piezo speaker will stop too
  if (minDistance > DISTANCE_MAP[0].fromDistance) {
    noteMillis = millis();
    pieso.stop();

  // Change piezo state if enough time has passed
  } else if (millis() - noteMillis > noteDuration) {
    noteMillis = millis();
    if (pieso.isPlaying()) {
      pieso.stop();
    } else {
      pieso.play(note);
    }
  }


  // Draw the car's rear end
  display.fillRoundRect(
    SCREEN.x / 4, SCREEN.y / 2,
    SCREEN.x / 2, SCREEN.y,
    SCREEN.y / 4, SSD1306_WHITE
  );

  display.display();
  ledLeft.show();
  ledRight.show();
}


// HC-SR04 activation
long activateTrigger(bool isLeft) {
  int pinTrigger = (isLeft) ? PIN_TRIGGER.x : PIN_TRIGGER.y;
  int pinEcho = (isLeft) ? PIN_ECHO.x : PIN_ECHO.y;
  digitalWrite(pinTrigger, LOW); // Make sure the sensor stops firing an IR beam
  delayMicroseconds(2);
  digitalWrite(pinTrigger, HIGH); // Activate sensor for 10 microsec
  delayMicroseconds(10);
  digitalWrite(pinTrigger, LOW);
  return pulseIn(pinEcho, HIGH); // Wait until the IR beam bounces back
}

// IR beam bounce time -> distance
int durationToDistance(long duration) {
  // return duration * 0.034 / 2;
  return duration * IR_SPEED / 2;
}
