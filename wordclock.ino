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
#define LED_TYPE WS2812B
#define LED_COLOR_ORDER GRB
#define FRAMES_PER_SECOND 60

#define LED_MODE_NORMAL 0
#define LED_MODE_RAINBOW 1
#define LED_MODE_RAINBOW_GLITTER 2
#define LED_MODE_CONFETTI 3
#define LED_MODE_SINELON 4
#define LED_MODE_BPM 5
#define LED_MODE_JUGGLE 6
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
  uint8_t * leds;
  size_t size;
};
typedef struct wording Word;

struct digit
{
  const char * text;
  uint16_t led;
};
typedef struct digit Digit;


// VARIABLES
// ... Board ...
uint8_t interruptDeltaT = 2;// seconds at which timer1 should interrupt

// ... Words ...
const Word IT = {"IT", new uint8_t[2]{0, 1}, 2};
const Word IS = {"IS", new uint8_t[2]{3, 4}, 2};

const Word W_MINS[] = {
  {"M5",  new uint8_t[4]{29, 30, 31, 32}, 4},
  {"M10", new uint8_t[3]{21, 20, 19}, 3},
  {"M15", new uint8_t[7]{17, 16, 15, 14, 13, 12, 11}, 7},
  {"M20", new uint8_t[6]{23, 24, 25, 26, 27, 28}, 6},
  {"M25", new uint8_t[10]{23, 24, 25, 26, 27, 28, 29, 30, 31, 32}, 10}, // only for simplicity
  {"M30", new uint8_t[4]{6, 7, 8, 9}, 4}
};

const Word TO   = {"TO",   new uint8_t[2]{43, 42}, 2};
const Word PAST = {"PAST", new uint8_t[4]{41, 40, 39, 38}, 4};

const Word W_HOURS[] = {
  {"H12", new uint8_t[6]{60, 59, 58, 57, 56, 55}, 6},
  {"H1",  new uint8_t[3]{73, 74, 75}, 3},
  {"H2",  new uint8_t[3]{48, 49, 50}, 3},
  {"H3",  new uint8_t[5]{65, 64, 63, 62, 61}, 5},
  {"H4",  new uint8_t[4]{36, 35, 34, 33}, 4},
  {"H5",  new uint8_t[4]{44, 45, 46, 47}, 4},
  {"H6",  new uint8_t[3]{92, 93, 94}, 3},
  {"H7",  new uint8_t[5]{87, 86, 85, 84, 83}, 5},
  {"H8",  new uint8_t[5]{81, 80, 79, 78, 77}, 5},
  {"H9",  new uint8_t[4]{51, 52, 53, 54}, 4},
  {"H10", new uint8_t[3]{89, 90, 91}, 3},
  {"H11", new uint8_t[6]{67, 68, 69, 70, 71, 72}, 6}
};

const Word AM = {"AM", new uint8_t[2]{107, 106}, 2};
const Word PM = {"PM", new uint8_t[2]{102, 101}, 2};

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

// ... RTC ...
bool updateTime = false;
bool isTimeUpdateRunning = false;
bool isScheduleActive = false;
bool isPowerOffInitialized = false;
Time t;

// ... IR ...
decode_results irResults;
bool shouldEvaluateIRresults = false;
bool evaluatingIRresults = false;
bool pauseAnimations = false;
int irCtr = 0;
const int IR_ZERO  = 0xFA002FEC; //0xFF6897;
const int IR_ONE   = 0xFA002FC8;//0xFF30CF;
const int IR_TWO   = 0xFA002FE8;//0xFF18E7;
const int IR_THREE = 0xFA002FD8;//0xFF7A85;
const int IR_FOUR  = 0xFA002FF8;//0xFF10EF;
const int IR_FIVE  = 0xFA002FC4;//0xFF38C7;
const int IR_SIX   = 0xFA002FE4;//0xFF38C7;
const int IR_VOL_UP   = 0xFA002FFC;//0xFF629D;
const int IR_VOL_DOWN = 0xFA002FDC;//0xFFA857;
const int IR_UP       = 0xFA002FCE;//0xFF906F;
const int IR_DOWN     = 0xFA002FF6;//0xFFE01F;
const int IR_POWER    = 0xFA002FD0;//0xFFA25D;

// ... LED ...
CRGB leds[LED_PIXELS];
uint8_t ledMode = LED_MODE_NORMAL;
uint8_t hue = 0; // FastLED's HSV range is from [0...255], instead of common [0...359]
uint8_t ledBrightness = 20; // in %, to avoid division will be multiplied by 0.01 before application, used for value of HSV color

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
  if (ledBrightness + stepSize > 80)
    ledBrightness = 80;
  else if (ledBrightness + stepSize < 0)
    ledBrightness = 0;
  else
    ledBrightness += stepSize;
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
  fill_rainbow( leds, LED_PIXELS, hue, 7);
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

  int mins = t.min;

  // set start of sentence
  setColorForWord(IT);
  setColorForWord(IS);

  // set 5-minute-step
  if (showMinutes(mins)) 
  {
    // WARNING: will only be valid for mins in range of [0...55],
    //          will cause an out-of-bounds exception for mins > 55
    int idx = (mins > 30) 
                ? int((MIN_PARTS - 1) - ((mins - 30) / MIN_STEP)) 
                : int(mins / MIN_STEP);
    setColorForWord(W_MINS[ idx ]);
  }

  // set relation
  if (showMinutes(mins)) 
  {
    if (mins > 30)
      setColorForWord(TO); // to
    else
      setColorForWord(PAST); // past
  }

  // set hour
  if (mins > 30)
    setColorForWord(W_HOURS[int(ceil(t.hour % 12)) + 1]);// to
  else
    setColorForWord(W_HOURS[int(ceil(t.hour % 12))]);// past

  // set daytime
  if (t.hour > 12)
    setColorForWord(PM);
  else
    setColorForWord(AM);

  // set digits for minute and second
  int m1 = int(mins / 10);  // only "first digit" of minute value, e.g. 34 => 3
  int m2 = int(mins - (m1 * 10)); // only "second digit" of minute value, e.g. 34 => 4

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

  switch (int(irResults.value)) 
  {
    case IR_ZERO:
      DBG_PRINTLN(">>NORMAL");
      ledMode = LED_MODE_NORMAL;
      break;
    case IR_ONE:
      DBG_PRINTLN(">>RAINBOW");
      ledMode = LED_MODE_RAINBOW;
      break;
    case IR_TWO:
      DBG_PRINTLN(">>RAINBOW GLITTER");
      ledMode = LED_MODE_RAINBOW_GLITTER;
      break;
    case IR_THREE:
      DBG_PRINTLN(">>CONFETTI");
      ledMode = LED_MODE_CONFETTI;
      break;
    case IR_FOUR:
      DBG_PRINTLN(">>SINELON");
      ledMode = LED_MODE_SINELON;
      break;
    case IR_FIVE:
      DBG_PRINTLN(">>BPM");
      ledMode = LED_MODE_BPM;
      break;
    case IR_SIX:
      DBG_PRINTLN(">>JUGGLE");
      ledMode = LED_MODE_JUGGLE;
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
  }

  if (isScheduleActive)
    setColorForDigit(SCHEDULE);

  // send the 'leds' array out to the actual LED strip
  FastLED.show();
  // insert a delay to keep the framerate modest
  FastLED.delay(1000 / FRAMES_PER_SECOND);

  // cycle through hue for some animations
  if ((ledMode == LED_MODE_RAINBOW) 
      || (ledMode == LED_MODE_RAINBOW_GLITTER)
      || (ledMode == LED_MODE_SINELON)
      || (ledMode == LED_MODE_JUGGLE))
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
  FastLED.setBrightness(LED_BRIGHTNESS);
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
    t = rtc.getTime();

    // simple schedule
    if ((t.hour >= RTC_ALIVE_FROM) && (t.hour < RTC_ALIVE_TO)) 
    { // leds are active 
      isPowerOffInitialized = false;
      handleLeds();
    } 
    else 
    { // leds should not be active
      if (!isPowerOffInitialized) {
        fill_solid(leds, LED_PIXELS, CRGB::Black);
        isPowerOffInitialized = true;
        FastLED.show();
      }
    }

  } 
  else if (irCtr >= IR_PAUSE) 
  {
    pauseAnimations = false;
  }
}
