/**
 * Word clock.
 *
 * A word clock implementation for English language.
 * This clock lights up LEDs (WS2812B) to visualize words using the overlaying template.
 * LEDs are laid out as a strip where the id counting between rows alternates.
 * So the pattern for counting LEDs is:
 *
 * row1::rtl, row2::ltr, row3::rtl, row4::ltr, ...
 *
 * Further a RTC module is used to keep track of time.
 * And an IR module allows to set/correct display animations, etc. via remote control.
 *
 * Author: Christian Hansen
 * Date: 27.01.2019
 * Version: 0.3
 *
 *  Hardware:
 *  - Arduino Nano
 *  - WS2812B strip
 *  - DS3231
 *  - 
 * 
 * Libraries in use:
 * - WS2812B: https://github.com/FastLED/FastLED
 * - DS3231:  https://github.com/JChristensen/DS3232RTC
 * - IR:      https://github.com/z3t0/Arduino-IRremote
 * - Enerlib: http://playground.arduino.cc/Code/Enerlib
 * 
 *  Version  Description
 *  =======  ===========
 *  0.3      * Switch library for DS3231
 *           * Introduce low power library: Enerlib
 *           * Refactor schedule handling to use alarm of RTC module and set Arduino to low power mode
 *           * Refactor code organization/documentation
 *           * Switched pin defintions to be able to use wake-up through RTC interrupt
 * 
 *  0.2      * Added interrupt timer to remove calls for current time from main loop
 *           * The timer interrupt is also used to check whether IR received a signal and needs to be handled
 *           * Refactored data type associations
 *           * Added animation derived from the Matrix movie (has still bugs though which need to be fixed)
 *
 *  0.1      * first (feature-complete) working solution
*/

// ===================================
// INCLUDES
// ===================================

#include <math.h>
#include <IRremote.h>
#include <DS3232RTC.h>  // Analog 4, Analog 5 for Arduino Nano, Digital 2 to react on interrupt
#include <FastLED.h>
#include <Enerlib.h>

// ===================================
// NAMESPACES
// ===================================

FASTLED_USING_NAMESPACE

// ===================================
// DEBUG
// ===================================

#define DEBUG  0
#if DEBUG
# define DBG_PRINT(...)    Serial.print(__VA_ARGS__)
# define DBG_PRINTLN(...)  Serial.println(__VA_ARGS__)
#else
# define DBG_PRINT(...)
# define DBG_PRINTLN(...)
#endif

// ===================================
// DEFINITIONS
// ===================================

#define TIMER_PRESCALE 256
#define BOARD_FREQ 16000000 // 16 MHz

#define LED_DATA_PIN 4
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
#define RTC_ALARM_PIN 2
#define RTC_WAKE_UP_HRS 6
#define RTC_WAKE_UP_MINS 0
#define MIN_STEP 5
#define MIN_PARTS 6

#define IR_RECEIVE_PIN 6
#define IR_PAUSE 3 // timer interrupts

// ===================================
// STRUCTURES
// ===================================

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

// ===================================
// MACROS
// ===================================

#define WORD(s,a) { s, a, sizeof(a)/sizeof(a[0]) }

// ===================================
// VARIABLES
// ===================================

// ===================================
// ... Board ...
// ===================================

uint8_t interruptDeltaT = 2; // ~seconds at which timer1 should interrupt

// ===================================
// ... Power ...
// ===================================

Energy energy;

// ===================================
// ... Words ...
// ===================================

const uint8_t A_IT[] = {0, 1};
const Word IT = WORD("IT", A_IT);

const uint8_t A_IS[] = {3, 4};
const Word IS = WORD("IS", A_IS);

const uint8_t  A_M5[] = {29, 30, 31, 32};
const uint8_t A_M10[] = {21, 20, 19};
const uint8_t A_M15[] = {7, 17, 16, 15, 14, 13, 12, 11}; // >> "a quarter"
const uint8_t A_M20[] = {23, 24, 25, 26, 27, 28};
const uint8_t A_M25[] = {23, 24, 25, 26, 27, 28, 29, 30, 31, 32}; // only for simplicity
const uint8_t A_M30[] = {6, 7, 8, 9};
const Word W_MINS[] = { 
  WORD("M5" , A_M5),  WORD("M10", A_M10), WORD("M15", A_M15), 
  WORD("M20", A_M20), WORD("M25", A_M25), WORD("M30", A_M30)
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
  WORD("H12", A_H12), WORD("H1",  A_H1),  WORD("H2",  A_H2),
  WORD("H3",  A_H3),  WORD("H4",  A_H4),  WORD("H5",  A_H5),
  WORD("H6",  A_H6),  WORD("H7",  A_H7),  WORD("H8",  A_H8),
  WORD("H9",  A_H9),  WORD("H10", A_H10), WORD("H11", A_H11)
};

const uint8_t A_AM[] = {107, 106};
const Word AM = WORD("AM", A_AM);

const uint8_t A_PM[] = {102, 101};
const Word PM = WORD("PM", A_PM);

const Digit SCHEDULE = { "S", 110 }; // pseudo-digit
const Digit DIGITS[] = {
  {"D0", 111}, {"D1", 112}, {"D2", 113},
  {"D3", 114}, {"D4", 115}, {"D5", 116},
  {"D6", 117}, {"D7", 118}, {"D8", 119},
  {"D9", 120}
};

const uint8_t A_CHK[] = {64, 68, 84, 92, 82, 72, 58, 52, 34};
const Word CHK = WORD("CHK", A_CHK);

// ===================================
// ... RTC ...
// ===================================

volatile bool updateTime = false;
volatile bool isrAlarmWasCalled = false;

bool isTimeUpdateRunning = false;
bool isScheduleActive = false;
bool isPowerOffInitialized = false;

tmElements_t t;

// ===================================
// ... IR ...
// ===================================

decode_results irResults;

volatile bool shouldEvaluateIRresults = false;
volatile byte irCtr = 0;

bool evaluatingIRresults = false;
bool pauseAnimations = false;
bool autoCycleHue = false;
bool autoCycleBrightness = false;

/*
  All remote control values we want to check are
  32-bit values. As all start with 0xFA00XXXX, we 
  will just use the first 16-bit when evaluating 
  the results the IR module received.
*/
const uint16_t IR_ZERO  = 0x2FEC;
const uint16_t IR_ONE   = 0x2FC8; 
const uint16_t IR_TWO   = 0x2FE8; 
const uint16_t IR_THREE = 0x2FD8; 
const uint16_t IR_FOUR  = 0x2FF8; 
const uint16_t IR_FIVE  = 0x2FC4; 
const uint16_t IR_SIX   = 0x2FE4; 
const uint16_t IR_SEVEN = 0x2FD4;
const uint16_t IR_VOL_UP   = 0x2FFC;
const uint16_t IR_VOL_DOWN = 0x2FDC;
const uint16_t IR_UP       = 0x2FCE;
const uint16_t IR_DOWN     = 0x2FF6;
const uint16_t IR_POWER    = 0x2FD0;
const uint16_t IR_AUTO_HUE = 0x2FD6;
const uint16_t IR_AUTO_BRIGHTNESS = 0x2FEA;

// ===================================
// ... LED ...
// ===================================

CRGB leds[LED_PIXELS];
bool blinkToConfirm = false;
uint8_t fps = 60;
uint8_t ledMode = LED_MODE_NORMAL;
uint8_t hue = 0;                      // FastLED's HSV range is from [0...255], instead of common [0...359]
uint8_t oldBrightness = 20;           // in %, to avoid division will be multiplied by 0.01 before application, used for value of HSV color
uint8_t newBrightness = 20;
bool incBrightness = true;

// ===================================
// INITIALIZATIONS
// ===================================

IRrecv irrecv(IR_RECEIVE_PIN);

// ===================================
// RTC FUNCTIONS
// ===================================

/**
 * Init RTC module.
 */
void initRTC();

/**
 * Set RTC to a predefined time.
 */
// void setRTCTime();

/**
 * Determine if minutes should be displayed.
 * This is not necessary if minutes are 0.
 * Or, in range of 55 to 60, as the clock shows "future" 5-minute-steps.
 */
bool showMinutes(int mins);

/**
 * Check if schedule should be applied.
 */
bool shouldGoToSleep();

/**
 * (Re)set the alarm schedule.
 */
void setAlarmSchedule();

/**
 * Set alarm schedule and immediately go 
 * into power down state.
 */
void setAlarmScheduleAndEnterLowPower();

// ===================================
// LOW POWER FUNCTIONS
// ===================================

/**
 * Enter low poer mode.
 * Arduino will only wake up again through
 * interrupt from external module.
 */
void enterLowPower();

// ===================================
// LED FUNCTIONS
// ===================================

/**
 * Increase pixel brightness.
 * Value range is [0 ... 80].
 * 100 is not an option to save LED lifetime.
 * For decreasing use negative step size.
 */
void increaseBrightness(int stepSize);

/**
 * Increase base hue value.
 * For decreasing use negative step size.
 */
void increaseHue(int stepSize);

// ===================================
// LED WORD FUNCTIONS
// ===================================

/**
 * Color pixels for given word.
 */
void setColorForWord(Word _word);

/**
 * Color pixels for given word.
 */
void setColorForWord(Word _word, const struct CRGB &color);

/**
 * Color pixels for given digit.
 */
void setColorForDigit(Digit digit);

// ===================================
// LED ANIMATION FUNCTIONS
// ===================================

/**
 * Fill strip with rainbow colors.
 */
void rainbow();

/**
 * Add white spots.
 */
void addGlitter(fract8 chanceOfGlitter);

/**
 * Rainbow pattern with white spots.
 */
void rainbowWithGlitter();

/**
 * Random colored speckles that blink in and fade smoothly.
 */
void confetti();

/**
 * A colored dot sweeping back and forth, with fading trails
 */
void sinelon();

/**
 * Colored stripes pulsing at a defined Beats-Per-Minute (BPM)
 */
void bpm();

/**
 * Eight colored dots, weaving in and out of sync with each other.
 */
void juggle();

/**
 * Let letters blink like in the in the movie 'Matrix'
 * FIXME: timing issues and new letter spawning
 */
void matrix();

// ===================================
// INTERRUPTS
// ===================================

/**
 * Setup up Timer1 to be called every second.
 */
void setupTimer1();

/**
 * Interrupt service routine for Timer1.
 * Is triggered when TCNT1 equals OCR1A.
 */
ISR(TIMER1_COMPA_vect);

/**
 * Interrupt service routine for RTC.
 * Used to wake up the Arduino from power down state.
 * Will be called when RTC detects a 
 * matching for the activated schedule.
 */
void isrAlarm();

// ===================================
// MAIN HANDLER FUNCTIONS
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

// ===================================
// MAIN
// ===================================

/**
 * Main setup function.
 */
void setup()
{
  delay(1000); // warm-up delay

  Serial.begin(9600);
  delay(100);
  DBG_PRINTLN("Setup...");

  // RTC
  initRTC();
  pinMode(RTC_ALARM_PIN, INPUT_PULLUP);
  attachInterrupt(INT0, isrAlarm, FALLING);
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

/**
 * Main loop function.
 */
void loop()
{
  if (isrAlarmWasCalled)
  {
    // ah, just woke up ...
    RTC.alarm(ALARM_2); // reset alarm flag  
    isrAlarmWasCalled = false;
  }

  handleIRresults();

  if (!pauseAnimations) 
  {
    // simple schedule
    if (shouldGoToSleep())
    { // leds should not be active
      if (!isPowerOffInitialized) {

        // set all leds to black/off
        fill_solid(leds, LED_PIXELS, CRGB::Black);
        isPowerOffInitialized = true;
        FastLED.show();

        // activate schedule
        setAlarmScheduleAndEnterLowPower();
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

// ===================================
// FUNCTION IMPLEMENTATIONS
// ===================================

/**
 * INTERRUPT FUNCTIONS
 */

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

ISR(TIMER1_COMPA_vect)
{
  /*
    Interrupt service routine is called
    when Timer1 reaches the value defined 
    during intialization.
  */

  TCNT1 = 0;                                            // (re-)initialize register with 0
  updateTime = true;                                    // trigger time update
  shouldEvaluateIRresults = irrecv.decode(&irResults);  // check IR receiver
  irCtr++;                                              // increase work-around counter
}

void isrAlarm()
{
  if (energy.WasSleeping())
  {
    /* 
      Will only be true when Arduino was in deep sleep before calling the routine.
      Do some re-init things when needed.
    */ 
  }
  isrAlarmWasCalled = true;
}

/**
 * MAIN HANDLER FUNCTIONS
 */

void handleDisplayTime()
{
  if (!updateTime) return;
  if (isTimeUpdateRunning) return;
  
  updateTime = false;         // reset flag
  isTimeUpdateRunning = true; // prevent unecessary execution

  // get time values
  RTC.read(t);

  // reset color array to black
  fill_solid(leds, LED_PIXELS, CRGB::Black);

  // set start of sentence
  setColorForWord(IT);
  setColorForWord(IS);

  // set 5-minute-step
  if (showMinutes(t.Minute)) 
  {
    // WARNING: will only be valid for mins in range of [0...55],
    //          will cause an out-of-bounds exception for mins > 55
    int idx = (t.Minute > 30) 
                ? floor((MIN_PARTS - 2) - ((t.Minute - 31) / MIN_STEP)) // -2 because: -1 in general because its an array length, -1 again because we cannot use 'half'
                : floor(t.Minute / MIN_STEP);
    setColorForWord(W_MINS[ idx ]);
  }

  // set relation
  if (showMinutes(t.Minute)) 
  {
    if (t.Minute > 30)
      setColorForWord(TO); // to
    else
      setColorForWord(PAST); // past
  }

  // set hour
  if (t.Minute > 30)
    setColorForWord(W_HOURS[(int)ceil((t.Hour + 1) % 12)]); // to
  else
    setColorForWord(W_HOURS[(int)ceil(t.Hour % 12)]); // past

  // set daytime
  if (t.Hour > 12)
    setColorForWord(PM);
  else
    setColorForWord(AM);

  // set digits for minute and second
  int m1 = int(t.Minute / 10);  // only "first digit" of minute value, e.g. 34 => 3
  int m2 = int(t.Minute - (m1 * 10)); // only "second digit" of minute value, e.g. 34 => 4

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

  // we just check for this protocol: NEC
  if (irResults.decode_type != NEC) return; 

  // we have a result with expected prototcol, we can enable animations again
  pauseAnimations = false;
  shouldEvaluateIRresults = false;
  evaluatingIRresults = true;

  evaluateIRResult(irResults.value);

  delay(50);
  irrecv.resume();
  evaluatingIRresults = false;
}

void evaluateIRResult(uint32_t result)
{
  DBG_PRINTLN(result, HEX);

  uint16_t valueToCheck = (result & 0xffff);  // for the 'why?' see init section for IR_*

  switch (valueToCheck) 
  {
    // animations
    case IR_ZERO:  setLEDModeState(">>NORMAL",   LED_MODE_NORMAL,   25); break;
    case IR_ONE:   setLEDModeState(">>RAINBOW",  LED_MODE_RAINBOW,  60); break;
    case IR_TWO:   setLEDModeState(">>RAINBOW GLITTER", LED_MODE_RAINBOW_GLITTER , 60); break;
    case IR_THREE: setLEDModeState(">>CONFETTI", LED_MODE_CONFETTI, 60); break;
    case IR_FOUR:  setLEDModeState(">>SINELON",  LED_MODE_SINELON,  60); break;
    case IR_FIVE:  setLEDModeState(">>BPM",      LED_MODE_BPM,      60); break;
    case IR_SIX:   setLEDModeState(">>JUGGLE",   LED_MODE_JUGGLE,   60); break;
    case IR_SEVEN: setLEDModeState(">>MATRIX",   LED_MODE_MATRIX,   25); break;
    // brightness
    case IR_VOL_UP:   DBG_PRINTLN("BRIGHTNESS++"); increaseBrightness(LED_BRIGHTNESS_STEP); break;
    case IR_VOL_DOWN: DBG_PRINTLN("BRIGHTNESS--"); increaseBrightness(LED_BRIGHTNESS_STEP * -1); break;
    // hue
    case IR_UP:   DBG_PRINTLN("HUE++"); increaseHue(LED_HUE_STEP); break;
    case IR_DOWN: DBG_PRINTLN("HUE--"); increaseHue(LED_HUE_STEP * -1); break;
    // schedule
    case IR_POWER: 
      DBG_PRINTLN(">>SCHEDULE"); 
      isScheduleActive = !isScheduleActive;
      blinkToConfirm = true;
      break;
    // automatic routines
    case IR_AUTO_HUE:
      DBG_PRINTLN(">>AUTO HUE");
      autoCycleHue = !autoCycleHue;
      blinkToConfirm = true;
      break;
    case IR_AUTO_BRIGHTNESS:
      DBG_PRINTLN(">>AUTO HUE");
      autoCycleBrightness = !autoCycleBrightness;
      blinkToConfirm = true;
      break;
  }
}

void setLEDModeState(String debugMsg, uint8_t mode, uint8_t _fps)
{
  DBG_PRINTLN(debugMsg);
  ledMode = mode;
  fps = _fps;
}

void handleLeds()
{
  switch (ledMode) 
  {
    case LED_MODE_NORMAL:   handleDisplayTime(); break;
    case LED_MODE_RAINBOW:  rainbow(); break;
    case LED_MODE_RAINBOW_GLITTER: rainbowWithGlitter(); break;
    case LED_MODE_CONFETTI: confetti(); break;
    case LED_MODE_SINELON:  sinelon(); break;
    case LED_MODE_BPM:      bpm(); break;
    case LED_MODE_JUGGLE:   juggle(); break;
    case LED_MODE_MATRIX:   matrix(); break;
  }

  if (isScheduleActive)
    setColorForDigit(SCHEDULE);

  if (blinkToConfirm)
  {
    blinkToConfirm = false;
    setColorForWord(CHK, CRGB::White);
  }

  if (autoCycleBrightness) 
  {
    if (millis() % 20 == 0) 
    {
      if (incBrightness) 
      {
        newBrightness++;
        if (newBrightness > 180) 
        {
          newBrightness = 180;
          incBrightness = false;
        }
      } 
      else
      {
        newBrightness--;
        if (newBrightness < 2) // FastLED treats brightness of 0 as if it is 255
        {
          newBrightness = 2;
          incBrightness = true;
        }
      }
    }
  }

  if (newBrightness != oldBrightness) {
    oldBrightness = newBrightness;
    FastLED.setBrightness(newBrightness);  
  }

  FastLED.show();             // send the 'leds' array out to the actual LED strip
  FastLED.delay(1000 / fps);  // insert a delay to keep the framerate modest

  // cycle through hue for some animations
  if ((ledMode == LED_MODE_RAINBOW) 
      || (ledMode == LED_MODE_RAINBOW_GLITTER)
      || (ledMode == LED_MODE_SINELON)
      || (ledMode == LED_MODE_JUGGLE)
      || autoCycleHue)
    if( millis() % 20 == 0 ) { hue++; }
}


/**
 * RTC FUNCTIONS
 */

void initRTC()
{
  RTC.setAlarm(ALM1_MATCH_DATE, 0, 0, 0, 1);
  RTC.setAlarm(ALM2_MATCH_DATE, 0, 0, 0, 1);
  RTC.alarm(ALARM_1);
  RTC.alarm(ALARM_2);
  RTC.alarmInterrupt(ALARM_1, false);
  RTC.alarmInterrupt(ALARM_2, false);
  RTC.squareWave(SQWAVE_NONE);
}

// void setRTCTime()
// {
//   tmElements_t tm;
//   tm.Hour = 06;               // set the RTC time to 06:29:50
//   tm.Minute = 29;
//   tm.Second = 0;
//   tm.Day = 1;
//   tm.Month = 1;
//   tm.Year = 0;      // tmElements_t.Year is the offset from 1970
//   RTC.write(tm); 
// }

bool showMinutes(int mins)
{
  return ((mins < 56) || (mins == 0));
}

bool shouldGoToSleep()
{
  if (!isScheduleActive) return false;
  RTC.read(t);
  return ((RTC_ALIVE_FROM > t.Hour) || (t.Hour >= RTC_ALIVE_TO));
}

void setAlarmSchedule()
{
   // set Alarm 2 
    RTC.setAlarm(ALM2_MATCH_HOURS, 0, RTC_WAKE_UP_MINS, RTC_WAKE_UP_HRS, 0);
    // clear the alarm flags
    RTC.alarm(ALARM_1);
    RTC.alarm(ALARM_2);
    // configure the INT/SQW pin for "interrupt" operation (disable square wave output)
    RTC.squareWave(SQWAVE_NONE);
    // enable interrupt output for Alarm 2 only
    RTC.alarmInterrupt(ALARM_1, false);
    RTC.alarmInterrupt(ALARM_2, true);
}

void setAlarmScheduleAndEnterLowPower()
{
  setAlarmSchedule();
  enterLowPower();
}

/**
 * LOW POWER FUNCTIONS
 */

void enterLowPower()
{
  energy.PowerDown();
}

/**
 * LED FUNCTIONS :: SETTINGS
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

void increaseHue(int stepSize)
{
  /* 
    FastLED uses range [0...255] to represent 
    hue values. Hence, we do not need to check 
    borders and just let uint8_t overflow.
  */
  hue += stepSize;
}

/**
 * LED FUNCTIONS :: WORDS
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

void setColorForWord(Word _word, const struct CRGB &color)
{
  for (int i = 0; i < _word.size; i++) 
  {
    int ledNo = _word.leds[i];
    if (ledNo < LED_PIXELS)
      leds[ledNo] = color;
  }
}

void setColorForDigit(Digit digit)
{
  int ledNo = digit.led;
  if (ledNo < LED_PIXELS)
    leds[ledNo].setHue(hue);
}

/**
 * LED FUNCTIONS :: ANIMATIONS
 */

void rainbow()
{
  /*
    FastLED's built-in rainbow generator
  */
  fill_rainbow(leds, LED_PIXELS, hue, 7);
}

void addGlitter( fract8 chanceOfGlitter)
{
  if ( random8() < chanceOfGlitter) 
    leds[ random16(LED_PIXELS) ] += CRGB::White;
}

void rainbowWithGlitter()
{
  rainbow();
  addGlitter(80);
}

void confetti()
{
  fadeToBlackBy( leds, LED_PIXELS, 10);
  int pos = random16(LED_PIXELS);
  leds[pos] += CHSV( hue + random8(64), 200, 255);
}

void sinelon()
{
  fadeToBlackBy( leds, LED_PIXELS, 20);
  int pos = beatsin16( 13, 0, LED_PIXELS - 1 );
  leds[pos] += CHSV( hue, 255, 192);
}

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
  /* 
    We are using a strip here
    and to make even more confusing
    the index count alternates, like:
       1  2  3  4
       8  7  6  5
       9 10 11 12 ...
  */

  fadeToBlackBy( leds, LED_PIXELS, 20);
 
  // copy existing rows to next following rows to have a flow effect
  for (int i = LED_ROWS - 2; i > 0; i--)
  {
    for(int j = LED_COLUMNS - 1; j > 0; j--)
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