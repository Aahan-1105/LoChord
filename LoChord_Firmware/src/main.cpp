/**
 * @file main.cpp
 * @brief LoChord - Standalone USB-MIDI & Chord Controller Firmware
 * Target: Waveshare ESP32-S3-Zero / Clone
 */

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <FS.h>
#include "USB.h"
#include "USBMIDI.h"

USBMIDI MIDI;

// -------------------------------------------------------------
// Pin Definitions (Matched to your Schematic)
// -------------------------------------------------------------
#define LED_PIN       13
#define NUM_LEDS      6

#define ENCODER_A     4
#define ENCODER_B     5

const uint8_t ROW_PINS[2] = {44, 43}; // ROW1 (RX/GPIO44), ROW2 (TX/GPIO43)
const uint8_t COL_PINS[3] = {1, 2, 3};   // COL1 (GP1), COL2 (GP2), COL3 (GP3)

// 7 Touch Pads mapped to GP6 through GP12
const uint8_t TOUCH_PINS[7] = {6, 7, 8, 9, 10, 11, 12}; 
const int TOUCH_THRESHOLD = 30000; 

// -------------------------------------------------------------
// Global States & Mappings
// -------------------------------------------------------------
Adafruit_NeoPixel leds(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

bool matrixState[2][3] = {{false, false, false}, {false, false, false}};
bool touchState[7] = {false, false, false, false, false, false, false};

int lastEncoderA = HIGH;
uint8_t masterVolume = 100; // Default volume (0-127)

// Base diatonic notes for touch pads (C4 major scale)
const uint8_t TOUCH_MIDI_NOTES[7] = {60, 62, 64, 65, 67, 69, 71}; 

// Active octave transpose offset controlled by modifiers
int octaveOffset = 0; 

// -------------------------------------------------------------
// Function Prototypes
// -------------------------------------------------------------
void scanTouchPads();
void scanModifiers();
void scanEncoder();

// -------------------------------------------------------------
// Setup
// -------------------------------------------------------------
void setup() {
  Serial.begin(115200);

  MIDI.begin();
  USB.begin();

  leds.begin();
  leds.setBrightness(35);
  leds.clear();
  leds.show();

  pinMode(ENCODER_A, INPUT_PULLUP);
  pinMode(ENCODER_B, INPUT_PULLUP);
  lastEncoderA = digitalRead(ENCODER_A);

  for (int r = 0; r < 2; r++) {
    pinMode(ROW_PINS[r], OUTPUT);
    digitalWrite(ROW_PINS[r], HIGH);
  }
  for (int c = 0; c < 3; c++) {
    pinMode(COL_PINS[c], INPUT_PULLUP);
  }
}

// -------------------------------------------------------------
// Main Loop
// -------------------------------------------------------------
void loop() {
  scanTouchPads();
  scanModifiers();
  scanEncoder();
  
  delay(4);
}

// -------------------------------------------------------------
// Input Scanners
// -------------------------------------------------------------

void scanTouchPads() {
  for (int i = 0; i < 7; i++) {
    int touchVal = touchRead(TOUCH_PINS[i]);
    bool pressed = (touchVal > TOUCH_THRESHOLD);

    uint8_t finalNote = TOUCH_MIDI_NOTES[i] + (octaveOffset * 12);

    if (pressed && !touchState[i]) {
      touchState[i] = true;
      MIDI.noteOn(0, finalNote, 127);
      
      if (i < NUM_LEDS) {
        leds.setPixelColor(i, leds.Color(0, 200, 255));
        leds.show();
      }
    } else if (!pressed && touchState[i]) {
      touchState[i] = false;
      MIDI.noteOff(0, finalNote, 0);
      
      if (i < NUM_LEDS) {
        leds.setPixelColor(i, leds.Color(0, 0, 0));
        leds.show();
      }
    }
  }
}

void scanModifiers() {
  for (int r = 0; r < 2; r++) {
    digitalWrite(ROW_PINS[r], LOW);
    
    for (int c = 0; c < 3; c++) {
      bool pressed = (digitalRead(COL_PINS[c]) == LOW);
      
      if (pressed && !matrixState[r][c]) {
        matrixState[r][c] = true;

        if (r == 0 && c == 0) {
          octaveOffset = -1; 
        } else if (r == 0 && c == 1) {
          octaveOffset = 0;  
        } else if (r == 0 && c == 2) {
          octaveOffset = 1;  
        }
        
      } else if (!pressed && matrixState[r][c]) {
        matrixState[r][c] = false;
      }
    }
    digitalWrite(ROW_PINS[r], HIGH);
  }
}

void scanEncoder() {
  int currentA = digitalRead(ENCODER_A);
  
  if (currentA != lastEncoderA && currentA == LOW) {
    if (digitalRead(ENCODER_B) != currentA) {
      if (masterVolume < 127) masterVolume += 4;
      if (masterVolume > 127) masterVolume = 127;
    } else {
      if (masterVolume > 4) masterVolume -= 4;
      else masterVolume = 0;
    }
    MIDI.controlChange(0, 7, masterVolume);
  }
  lastEncoderA = currentA;
}