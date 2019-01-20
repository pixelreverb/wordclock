/**
   Word clock.

   A word clock implementation for English language.
   This clock lights up LEDs (ws2812b) to visualize words using the overlaying template.
   LEDs are laid out as a strip where the id counting between rows alternates.
   So the pattern for counting LEDs is:

   row1::rtl, row2::ltr, row3::rtl, row4::ltr, ...

   Further a RTC module (ds3231) is used to keep track of time.
   And an IR module allows to set/correct the time, display animations, etc. via remote control.

   Author: Christian Hansen
   Date: 13.01.2019
   Version: 0.2

   Libraries in use:
   - ws2812b: https://github.com/FastLED/FastLED
   - ds3231:  http://www.rinkydinkelectronics.com/library.php?id=73
   - IR:      https://github.com/z3t0/Arduino-IRremote

   TODOs:
   - Changing brightness has no effect at the moment, needs to be changed
   - Immidiate power off (not only with schedule)
   - Make use of sleep mode to save more energy compsumption in power-off-state
*/

// INCLUDES
#include <math.h>
#include <IRremote.h>
#include <DS3231.h>
#include <FastLED.h>

FASTLED_USING_NAMESPACE

// DEBUG
#define DEBUG  0
#if DEBUG
# define DBG_PRINT(...)    Serial.print(__VA_ARGS__)
# define DBG_PRINTLN(...)  Serial.println(__VA_ARGS__)
#else
# define DBG_PRINT(...)
# define DBG_PRINTLN(...)
#endif

// DEFINITIONS
#define TIMER_PRESCALE 256
#define BOARD_FREQ 16000000 // 16 MHz

#define LED_DATA_PIN 2
#define LED_PIXELS 121
#define LED_ROWS 11 // needed for matrix-depended effects
#define LED_COLUMNS 11 // needed for matrix-depended effects
#define LED_TYPE WS2812B
#define LED_COLOR_ORDER GRB
//#define FRAMES_PER_SECOND 60

#define LED_MODE_NORMAL 0
#define LED_MODE_RAINBOW 1
#define LED_MODE_RAINBOW_GLITTER 2
#define LED_MODE_CONFETTI 3
#define LED_MODE_SINELON 4
#define LED_MODE_BPM 5
#define LED_MODE_JUGGLE 6
#define LED_MODE_MATRIX 7
#define LED_BRIGHTNESS 20
#define LED_BRIGHTNESS_STEP 10
#define LED_HUE_STEP 10

#define RTC_HRS 0
#define RTC_MINS 1
#define RTC_SECS 2
#define RTC_ALIVE_FROM 6 // hour of day
#define RTC_ALIVE_TO  23 // hour of day
#define MIN_STEP 5
#define MIN_PARTS 6

#define IR_RECEIVE_PIN 3
#define IR_PAUSE 3 // timer interrupts

// STRUCTURES
struct wording
{
  const char * text;
  const uint8_t * leds;
  size_t size;
};
typedef struct wording Word;

struct digit
{
  const char * text;
  uint16_t led;
};
typedef struct digit Digit;

// MACROS
#define WORD(s,a) { s, a, sizeof(a)/sizeof(a[0]) }

// VARIABLES
// ... Board ...
uint8_t interruptDeltaT = 2;// seconds at which timer1 should interrupt

// ... Words ...
const uint8_t A_IT[] = {0, 1};
const Word IT = WORD("IT", A_IT);

const uint8_t A_IS[] = {3, 4};
const Word IS = WORD("IS", A_IS);

const uint8_t  A_M5[] = {29, 30, 31, 32};
const uint8_t A_M10[] = {21, 20, 19};
const uint8_t A_M15[] = {17, 16, 15, 14, 13, 12, 11};
const uint8_t A_M20[] = {23, 24, 25, 26, 27, 28};
const uint8_t A_M25[] = {23, 24, 25, 26, 27, 28, 29, 30, 31, 32};
const uint8_t A_M30[] = {6, 7, 8, 9};
const Word W_MINS[] = {
  WORD("M5" , A_M5),
  WORD("M10", A_M10),
  WORD("M15", A_M15),
  WORD("M20", A_M20),
  WORD("M25", A_M25), // only for simplicity
  WORD("M30", A_M30)
};

const uint8_t A_TO[] = {43, 42};
const Word TO = WORD("TO", A_TO);

const uint8_t A_PAST[] = {41, 40, 39, 38};
const Word PAST = WORD("PAST", A_PAST);

const uint8_t A_H12[] = {60, 59, 58, 57, 56, 55};
const uint8_t A_H1[] = {73, 74, 75};
const uint8_t A_H2[] = {48, 49, 50};
const uint8_t A_H3[] = {65, 64, 63, 62, 61};
const uint8_t A_H4[] = {36, 35, 34, 33};
const uint8_t A_H5[] = {44, 45, 46, 47};
const uint8_t A_H6[] = {92, 93, 94};
const uint8_t A_H7[] = {87, 86, 85, 84, 83};
const uint8_t A_H8[] = {81, 80, 79, 78, 77};
const uint8_t A_H9[] = {51, 52, 53, 54};
const uint8_t A_H10[] = {89, 90, 91};
const uint8_t A_H11[] = {67, 68, 69, 70, 71, 72};
const Word W_HOURS[] = {
  WORD("H12", A_H12),
  WORD("H1",  A_H1),
  WORD("H2",  A_H2),
  WORD("H3",  A_H3),
  WORD("H4",  A_H4),
  WORD("H5",  A_H5),
  WORD("H6",  A_H6),
  WORD("H7",  A_H7),
  WORD("H8",  A_H8),
  WORD("H9",  A_H9),
  WORD("H10", A_H10),
  WORD("H11", A_H11)
};

const uint8_t A_AM[] = {107, 106};
const Word AM = WORD("AM", A_AM);

const uint8_t A_PM[] = {102, 101};
const Word PM = WORD("PM", A_PM);

const Digit SCHEDULE = { "S", 110 }; // pseudo-digit
const Digit DIGITS[] = {
  {"D0", 111},
  {"D1", 112},
  {"D2", 113},
  {"D3", 114},
  {"D4", 115},
  {"D5", 116},
  {"D6", 117},
  {"D7", 118},
  {"D8", 119},
  {"D9", 120}
};

const uint8_t A_CHK[] = {64, 68, 84, 92, 82, 72, 58, 52, 34};
const Word CHK = WORD("CHK", A_CHK);

// ... RTC ...
bool updateTime = false;
bool isTimeUpdateRunning = false;
bool isScheduleActive = false;
bool isPowerOffInitialized = false;
struct Time t;

// ... IR ...
decode_results irResults;
bool shouldEvaluateIRresults = false;
bool evaluatingIRresults = false;
bool pauseAnimations = false;
bool autoCycleHue = false;
byte irCtr = 0;
const uint32_t IR_ZERO  = 0xFA002FEC; //0xFF6897;
const uint32_t IR_ONE   = 0xFA002FC8;//0xFF30CF;
const uint32_t IR_TWO   = 0xFA002FE8;//0xFF18E7;
const uint32_t IR_THREE = 0xFA002FD8;//0xFF7A85;
const uint32_t IR_FOUR  = 0xFA002FF8;//0xFF10EF;
const uint32_t IR_FIVE  = 0xFA002FC4;//0xFF38C7;
const uint32_t IR_SIX   = 0xFA002FE4;//0xFF38C7;
const uint32_t IR_SEVEN = 0xFA002FD4;
const uint32_t IR_VOL_UP   = 0xFA002FFC;//0xFF629D;
const uint32_t IR_VOL_DOWN = 0xFA002FDC;//0xFFA857;
const uint32_t IR_UP       = 0xFA002FCE;//0xFF906F;
const uint32_t IR_DOWN     = 0xFA002FF6;//0xFFE01F;
const uint32_t IR_POWER    = 0xFA002FD0;//0xFFA25D;
const uint32_t IR_AUTO_HUE = 0xFA002FD6;

// ... LED ...
CRGB leds[LED_PIXELS];
bool blinkToConfirm = false;
uint8_t fps = 60;
uint8_t ledMode = LED_MODE_NORMAL;
uint8_t hue = 0; // FastLED's HSV range is from [0...255], instead of common [0...359]
uint8_t oldBrightness = 20; // in %, to avoid division will be multiplied by 0.01 before application, used for value of HSV color
uint8_t newBrightness = 20;

// INITIALIZATIONS
DS3231 rtc(SDA, SCL); // Analog 4, Analog 5 for Arduino Nano, lib only uses 24h format, no alarms
IRrecv irrecv(IR_RECEIVE_PIN);

// RTC FUNCTIONS
/**
   Determine if minutes should be displayed.
   This is not necessary if minutes are 0.
   Or, in range of 55 to 60, as the clock shows "future" 5-minute-steps.
*/
bool showMinutes(int mins)
{
  return ((mins < 56) || (mins == 0));
}

// LED FUNCTIONS
/**
   Increase pixel brightness.
   Value range is [0 ... 80].
   100 is not an option to save LED lifetime.
   For decreasing use negative step size.
*/
void increaseBrightness(int stepSize)
{
  if (newBrightness + stepSize > 200)
    newBrightness = 200;
  else if (newBrightness + stepSize < 0)
    newBrightness = 0;
  else
    newBrightness += stepSize;
}

/**
   Increase base hue value.
   For decreasing use negative step size.
*/
void increaseHue(int stepSize)
{
  // FastLED uses range [0...255] to represent hue values
  // hence, we do not need to check borders and just let uint8_t overflow
  hue += stepSize;
  //  if (hue > 360) hue -= 360;
  //  if (hue < 0) hue += 360;
}

// LED ANIMATION FUNCTIONS
/**
   Fill strip with rainbow colors.
*/
void rainbow()
{
  // FastLED's built-in rainbow generator
  fill_rainbow(leds, LED_PIXELS, hue, 7);
}

/**
    Add white spots.
*/
void addGlitter( fract8 chanceOfGlitter)
{
  if ( random8() < chanceOfGlitter) 
    leds[ random16(LED_PIXELS) ] += CRGB::White;
}

/**
   Rainbow pattern with white spots.
*/
void rainbowWithGlitter()
{
  rainbow();
  addGlitter(80);
}

/**
   Random colored speckles that blink in and fade smoothly.
*/
void confetti()
{
  fadeToBlackBy( leds, LED_PIXELS, 10);
  int pos = random16(LED_PIXELS);
  leds[pos] += CHSV( hue + random8(64), 200, 255);
}

/**
   A colored dot sweeping back and forth, with fading trails
*/
void sinelon()
{
  fadeToBlackBy( leds, LED_PIXELS, 20);
  int pos = beatsin16( 13, 0, LED_PIXELS - 1 );
  leds[pos] += CHSV( hue, 255, 192);
}

/**
   Colored stripes pulsing at a defined Beats-Per-Minute (BPM)
*/
void bpm()
{
  uint8_t BeatsPerMinute = 62;
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
  for ( int i = 0; i < LED_PIXELS; i++) //9948
  { 
    leds[i] = ColorFromPalette(palette, hue + (i * 2), beat - hue + (i * 10));
  }
}

/**
   Eight colored dots, weaving in and out of sync with each other.
*/
void juggle()
{
  fadeToBlackBy( leds, LED_PIXELS, 20);
  byte dothue = 0;
  for ( int i = 0; i < 8; i++) 
  {
    leds[beatsin16( i + 7, 0, LED_PIXELS - 1 )] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
}

void matrix()
{
  fadeToBlackBy( leds, LED_PIXELS, 20);

  // we are using a strip here
  // and to make even more confusing
  // the index count alternates, like:
  //    1  2  3  4
  //    8  7  6  5
  //    9 10 11 12 ...

  // copy existing rows to next following rows to have a flow effect
  for (int i = LED_ROWS - 2; i > 0; i--)
  {
    for(int j = 0; j < LED_COLUMNS; j++)
    {
      if (i%2 == 0) 
      {
        leds[(((i + 1) * LED_COLUMNS) - 1) - (j % LED_COLUMNS)] = leds[(i * LED_COLUMNS) + j];
      }
      else 
      {
        leds[((i + 1) * LED_COLUMNS) + (j % LED_COLUMNS)] = leds[(i * LED_COLUMNS) - ((j % LED_COLUMNS) + 1)];
      }
    }
  }

  // spawn new pixels in first row
  int pos = random16(LED_COLUMNS);
  leds[pos] = CHSV( hue, 255, 192);
}

/**
   Color pixels for given word.
*/
void setColorForWord(Word _word)
{
  for (int i = 0; i < _word.size; i++) 
  {
    int ledNo = _word.leds[i];
    if (ledNo < LED_PIXELS)
      leds[ledNo].setHue(hue);
  }
}

/**
   Color pixels for given word.
*/
void setColorForWord(Word _word, const struct CRGB &color)
{
  for (int i = 0; i < _word.size; i++) 
  {
    int ledNo = _word.leds[i];
    if (ledNo < LED_PIXELS)
      leds[ledNo] = color;
  }
}

/**
   Color pixels for given digit.
*/
void setColorForDigit(Digit digit)
{
  int ledNo = digit.led;
  if (ledNo < LED_PIXELS)
    leds[ledNo].setHue(hue);
}

// INTERRUPTS
void setupTimer1()
{
  noInterrupts();           // stop all interrupts
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 0;                // initialize register with 0
  OCR1A = (interruptDeltaT * BOARD_FREQ / TIMER_PRESCALE); // initialize Output Compare Register
  TCCR1B |= (1 << WGM12);   // turn on CTC mode
  TCCR1B |= (1 << CS12);    // 256 prescale value
  TIMSK1 |= (1 << OCIE1A);  // activate Timer Compare Interrupt
  interrupts();
}

/**
   Interrupt service routine for timer1.
   Is triggered when TCNT1 equals OCR1A.
*/
ISR(TIMER1_COMPA_vect)
{
  // (re-)initialize register with 0
  TCNT1 = 0;
  // trigger time update
  updateTime = true;
  // check IR receiver
  shouldEvaluateIRresults = irrecv.decode(&irResults);
  // increase work-around counter
  irCtr++;
}

/*
   Check if schedule should be applied.
*/
bool shouldGoToSleep() 
{
  if (!isScheduleActive) return false;
  t = rtc.getTime();
  return ((RTC_ALIVE_FROM > t.hour) || (t.hour >= RTC_ALIVE_TO));
}

/**
   Main function to determine which words to highlight to show time.
*/
void handleDisplayTime()
{
  if (!updateTime) return;
  if (isTimeUpdateRunning) return;
  
  updateTime = false; // reset flag
  isTimeUpdateRunning = true; // prevent unecessary execution

  // get time values
  t = rtc.getTime();
  DBG_PRINTLN(rtc.getTimeStr());
  // reset color array to black
  fill_solid(leds, LED_PIXELS, CRGB::Black);

  // set start of sentence
  setColorForWord(IT);
  setColorForWord(IS);

  // set 5-minute-step
  if (showMinutes(t.min)) 
  {
    // WARNING: will only be valid for mins in range of [0...55],
    //          will cause an out-of-bounds exception for mins > 55
    int idx = (t.min > 30) 
                ? floor((MIN_PARTS - 1) - ((t.min - 30) / MIN_STEP))
                : floor(t.min / MIN_STEP);
    setColorForWord(W_MINS[ idx ]);
  }

  // set relation
  if (showMinutes(t.min)) 
  {
    if (t.min > 30)
      setColorForWord(TO); // to
    else
      setColorForWord(PAST); // past
  }

  // set hour
  if (t.min > 30)
    setColorForWord(W_HOURS[(int)ceil(t.hour % 12) + 1]); // to
  else
    setColorForWord(W_HOURS[(int)ceil(t.hour % 12)]); // past

  // set daytime
  if (t.hour > 12)
    setColorForWord(PM);
  else
    setColorForWord(AM);

  // set digits for minute and second
  int m1 = int(t.min / 10);  // only "first digit" of minute value, e.g. 34 => 3
  int m2 = int(t.min - (m1 * 10)); // only "second digit" of minute value, e.g. 34 => 4

  if (m1 == m2) 
  {
    setColorForDigit(DIGITS[ m1 ]);
  } 
  else 
  {
    setColorForDigit(DIGITS[ m1 ]);
    setColorForDigit(DIGITS[ m2 ]);
  }

  isTimeUpdateRunning = false; // reset flag
}

/**
   Main function to evaluate IR results.
*/
void handleIRresults()
{
  if (!shouldEvaluateIRresults) return;
  if (evaluatingIRresults) return;

  // work-around
  if (irResults.decode_type == UNKNOWN) 
  {
    // we stop animations for X timer interrupts, so we have no blocking
    // through disabled interrupts because of led updates
    irCtr = 0;
    pauseAnimations = true;

    delay(50);
    irrecv.resume();
  }

  if (irResults.decode_type != NEC) return; // we just check for this protocol

  // we have a result with expected prototcol, we can enable animations again
  pauseAnimations = false;
  shouldEvaluateIRresults = false;
  evaluatingIRresults = true;

  DBG_PRINTLN(irResults.value, HEX);

  switch (irResults.value) 
  {
    case IR_ZERO:
      DBG_PRINTLN(">>NORMAL");
      ledMode = LED_MODE_NORMAL;
      fps = 25;
      break;
    case IR_ONE:
      DBG_PRINTLN(">>RAINBOW");
      ledMode = LED_MODE_RAINBOW;
      fps = 60;
      break;
    case IR_TWO:
      DBG_PRINTLN(">>RAINBOW GLITTER");
      ledMode = LED_MODE_RAINBOW_GLITTER;
      fps = 60;
      break;
    case IR_THREE:
      DBG_PRINTLN(">>CONFETTI");
      ledMode = LED_MODE_CONFETTI;
      fps = 60;
      break;
    case IR_FOUR:
      DBG_PRINTLN(">>SINELON");
      ledMode = LED_MODE_SINELON;
      fps = 60;
      break;
    case IR_FIVE:
      DBG_PRINTLN(">>BPM");
      ledMode = LED_MODE_BPM;
      fps = 60;
      break;
    case IR_SIX:
      DBG_PRINTLN(">>JUGGLE");
      ledMode = LED_MODE_JUGGLE;
      fps = 60;
      break;
    case IR_SEVEN:
      DBG_PRINTLN(">>MATRIX");
      ledMode = LED_MODE_MATRIX;
      fps = 25;
      break;
    case IR_VOL_UP:
      DBG_PRINTLN("BRIGHTNESS++");
      increaseBrightness(LED_BRIGHTNESS_STEP);
      break;
    case IR_VOL_DOWN:
      DBG_PRINTLN("BRIGHTNESS--");
      increaseBrightness(LED_BRIGHTNESS_STEP * -1);
      break;
    case IR_UP:
      DBG_PRINTLN("HUE++");
      increaseHue(LED_HUE_STEP);
      break;
    case IR_DOWN:
      DBG_PRINTLN("HUE--");
      increaseHue(LED_HUE_STEP * -1);
      break;
    case IR_POWER:
      DBG_PRINTLN(">>SCHEDULE");
      isScheduleActive = !isScheduleActive;
      blinkToConfirm = true;
      break;
    case IR_AUTO_HUE:
      DBG_PRINTLN(">>AUTO HUE");
      autoCycleHue = !autoCycleHue;
      blinkToConfirm = true;
      break;
  }

  delay(50);
  irrecv.resume();
  evaluatingIRresults = false;
}

/**
   Main function to decide what should be displayed.
*/
void handleLeds()
{
  switch (ledMode) 
  {
    case LED_MODE_NORMAL:
      handleDisplayTime();
      break;

    case LED_MODE_RAINBOW:
      rainbow();
      break;

    case LED_MODE_RAINBOW_GLITTER:
      rainbowWithGlitter();
      break;

    case LED_MODE_CONFETTI:
      confetti();
      break;

    case LED_MODE_SINELON:
      sinelon();
      break;

    case LED_MODE_BPM:
      bpm();
      break;

    case LED_MODE_JUGGLE:
      juggle();
      break;

    case LED_MODE_MATRIX:
      matrix();
      break;
  }

  if (isScheduleActive)
    setColorForDigit(SCHEDULE);

  if (blinkToConfirm)
  {
    blinkToConfirm = false;
    setColorForWord(CHK, CRGB::White);
  }

  if (newBrightness != oldBrightness) {
    oldBrightness = newBrightness;
    FastLED.setBrightness(newBrightness);  
  }

  // send the 'leds' array out to the actual LED strip
  FastLED.show();
  // insert a delay to keep the framerate modest
  FastLED.delay(1000 / fps);

  // cycle through hue for some animations
  if ((ledMode == LED_MODE_RAINBOW) 
      || (ledMode == LED_MODE_RAINBOW_GLITTER)
      || (ledMode == LED_MODE_SINELON)
      || (ledMode == LED_MODE_JUGGLE)
      || autoCycleHue)
    if( millis() % 20 == 0 ) { hue++; }
}

void setup()
{
  delay(1000); // warm-up delay

  Serial.begin(9600);
  delay(100);
  DBG_PRINTLN("Setup...");

  // RTC
  rtc.begin();
  //rtc.setDOW(MONDAY);     // Set Day-of-Week to SUNDAY
  //rtc.setTime(20, 36, 0);     // Set the time to 12:00:00 (24hr format)
  //rtc.setDate(7, 1, 2019);   // Set the date to January 1st, 2014
  DBG_PRINTLN("RTC...");

  // LED
  FastLED.addLeds<LED_TYPE, LED_DATA_PIN, LED_COLOR_ORDER>(leds, LED_PIXELS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(newBrightness);
  DBG_PRINTLN("LED...");

  // IR
  irrecv.enableIRIn();
  DBG_PRINTLN("IR...");

  // TIMER
  setupTimer1();
  DBG_PRINTLN("Timer1...");

  delay(500);
}

void loop()
{
  handleIRresults();

  if (!pauseAnimations) 
  {
    // simple schedule
    if (shouldGoToSleep())
    { // leds should not be active
      if (!isPowerOffInitialized) {
        fill_solid(leds, LED_PIXELS, CRGB::Black);
        isPowerOffInitialized = true;
        FastLED.show();
      }
    }
    else
    { // leds are active 
      isPowerOffInitialized = false;
      handleLeds();
    } 
  } 
  else if (irCtr >= IR_PAUSE) 
  {
    pauseAnimations = false;
  }
}
