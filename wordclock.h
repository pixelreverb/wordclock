#ifndef WORDCLOCK_HEADER
#define WORDCLOCK_HEADER

/* External */
#include <math.h>
#include <IRremote.h>
#include <DS3232RTC.h> // Analog 4, Analog 5 for Arduino Nano, Digital 2 to react on interrupt
#include <FastLED.h>
#include <Enerlib.h>

/* Internal */
// Content:
// Definitions
// Pin and value definitions for:
//      - the Board
//      - LED pixels
//      - LED animation modes
//      - RTC
//      - IR
#include "wordclock_definitions.h"

// Content:
// Structures
//      - Word
//      - Digit
// Macros
#include "wordclock_constants.h"

/* RTC */
volatile bool updateTime = false;
volatile bool isrAlarmWasCalled = false;

bool isTimeUpdateRunning = false;
bool isScheduleActive = true;
bool isPowerOffInitialized = false;

tmElements_t t;

/* IR remote */
decode_results irResults;

volatile bool shouldEvaluateIRresults = false;
volatile byte irCtr = 0;

bool evaluatingIRresults = false;
bool pauseAnimations = false;
bool autoCycleHue = true;
bool autoCycleBrightness = false;

/*
  All remote control values we want to check are
  32-bit values. As all start with 0xFA00XXXX, we
  will just use the first 16-bit when evaluating
  the results the IR module received.

  But, also keep in mind that remote control protocols
  vary and that the definitions below may only match
  the remote control which is used with this project.

  So, you are advised to revisit your remote controls'
  values.
*/
const uint16_t IR_ZERO = 0x2FEC;
const uint16_t IR_ONE = 0x2FC8;
const uint16_t IR_TWO = 0x2FE8;
const uint16_t IR_THREE = 0x2FD8;
const uint16_t IR_FOUR = 0x2FF8;
const uint16_t IR_FIVE = 0x2FC4;
const uint16_t IR_SIX = 0x2FE4;
const uint16_t IR_SEVEN = 0x2FD4;
const uint16_t IR_VOL_UP = 0x2FFC;
const uint16_t IR_VOL_DOWN = 0x2FDC;
const uint16_t IR_UP = 0x2FCE;
const uint16_t IR_DOWN = 0x2FF6;
const uint16_t IR_POWER = 0x2FD0;
const uint16_t IR_AUTO_HUE = 0x2FD6;
const uint16_t IR_AUTO_BRIGHTNESS = 0x2FEA;

/* LED */
bool blinkToConfirm = false;
uint8_t fps = 60;
uint8_t ledMode = LED_MODE_NORMAL;
uint8_t hue = 0;            // FastLED's HSV range is from [0...255], instead of common [0...359]
uint8_t oldBrightness = 20; // in %, to avoid division will be multiplied by 0.01 before application, used for value of HSV color
uint8_t newBrightness = 20;
bool incBrightness = true;

// ===================================
class Wordclock
{
public:
  Wordclock(){};
  ~Wordclock(){};

  /* Time and power management */
  void initRTC(DS3232RTC &theClock);
  void setRtcTime(DS3232RTC &theClock, int hours, int min);
  void setAlarmSchedule(DS3232RTC &theClock);
  void setAlarmScheduleAndEnterLowPower(DS3232RTC &theClock, Energy &energy);
  void enterLowPower(Energy &energy);
  bool shouldShowMinutes(int mins);
  bool shouldGoToSleep(tmElements_t &tm);

  /* LED managament */
  void increaseBrightness(int stepSize);
  void increaseHue(int stepSize);
  void setColorForWord(CRGB *leds, Word _word);
  void setColorForWord(CRGB *leds, const struct CRGB color, Word _word);
  void setColorForDigit(CRGB *leds, Digit digit);

  /* LED animation management*/
  void rainbow(CRGB *leds);
  void rainbowWithGlitter(CRGB *leds);
  void confetti(CRGB *leds);
  void sinelon(CRGB *leds);
  void bpm(CRGB *leds);
  void juggle(CRGB *leds);
  void matrix(CRGB *leds);

private:
  addGlitter(CRGB *leds, fract8 chanceOfGlitter) 
  {
    if (random8() < chanceOfGlitter)
      leds[random16(LED_PIXELS)] += CRGB::White;
  }
};

// ===================================
// ... MAIN HANDLER FUNCTIONS
// ===================================

/**
 * Main function to determine which words to highlight to show time.
 */
void handleDisplayTime();

/**
 * Main function to evaluate IR results.
 */
void handleIRresults();

/**
 * Evaluate the IR result and set the proper led mode.
 */
void evaluateIRResult(uint32_t result);

/**
 * Set values for selected LED mode.
 */
void setLEDModeState(String debugMsg, uint8_t mode, uint8_t _fps);

/**
 * Main function to decide what should be displayed.
 */
void handleLeds();

#endif