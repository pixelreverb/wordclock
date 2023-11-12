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
 * Date: 10.2023
 * Version: 0.5
 *
 *  Hardware:
 *  - Arduino Nano
 *  - WS2812B strip
 *  - DS3231
 *  - IR module
 *
 * Libraries in use:
 * - WS2812B: https://github.com/FastLED/FastLED
 * - DS3231:  https://github.com/JChristensen/DS3232RTC
 * - IR:      https://github.com/z3t0/Arduino-IRremote      requires version 2.0.x
 * - Enerlib: http://playground.arduino.cc/Code/Enerlib
 *
 *  Version  Description
 *  =======  ===========
 *  0.5      * Better organisation: introduced a "Wordclock" class, split defintions of header file into seperate files
 *
 *  0.4      * Move defnitions into separate header file
 *           * Fix bug for timestamps of XX:30 not showing 'HALF'
 *
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

/* Includes */
#include "wordclock.h" // DS3232RTC: Analog 4, Analog 5 for Arduino Nano, Digital 2 to react on interrupt

/* Namespaces */
FASTLED_USING_NAMESPACE

/* Control flags */
#define DEBUG 0
#define SET_TIMER 0 // Set to 1 if only time should be set

/* Debug macro */
#if DEBUG
#define DBG_PRINT(...) Serial.print(__VA_ARGS__)
#define DBG_PRINTLN(...) Serial.println(__VA_ARGS__)
#else
#define DBG_PRINT(...)
#define DBG_PRINTLN(...)
#endif

/* Global variables */
DS3232RTC theClock;
Energy energy;
CRGB leds[LED_PIXELS];
Wordclock wordclock;
IRrecv irrecv(IR_RECEIVE_PIN);

/* Interrupt handling */
uint8_t interruptDeltaT = 2; // ~seconds at which timer1 should interrupt
void setupTimer1();
ISR(TIMER1_COMPA_vect);
void isrAlarm();

// ===================================
// MAIN
// ===================================

/**
 * Main setup function.
 */
void setup()
{
  Serial.begin(9600);
  while (!Serial)
  {
  }
  DBG_PRINTLN("Setup...");

  // RTC
  wordclock.initRTC(theClock);
  if (SET_TIMER)
  {
    wordclock.setRtcTime(theClock, 14, 6);
    DBG_PRINTLN("Time set...");
    return; // early exit
  }

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
}

/**
 * Main loop function.
 */
void loop()
{
  if (SET_TIMER)
  {
    return;
  }

  if (isrAlarmWasCalled)
  {
    // ah, just woke up ...
    theClock.alarm(DS3232RTC::ALARM_2); // reset alarm flag
    isrAlarmWasCalled = false;
  }

  handleIRresults();

  if (!pauseAnimations)
  {
    // simple schedule
    theClock.read(t);
    if (wordclock.shouldGoToSleep(t))
    { // leds should not be active
      if (!isPowerOffInitialized)
      {

        // set all leds to black/off
        fill_solid(leds, LED_PIXELS, CRGB::Black);
        isPowerOffInitialized = true;
        FastLED.show();

        // activate schedule
        wordclock.setAlarmScheduleAndEnterLowPower(theClock, energy);
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
 * Setup up Timer1 to be called every second.
 */
void setupTimer1()
{
  noInterrupts(); // stop all interrupts
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 0;                                               // initialize register with 0
  OCR1A = (interruptDeltaT * BOARD_FREQ / TIMER_PRESCALE); // initialize Output Compare Register
  TCCR1B |= (1 << WGM12);                                  // turn on CTC mode
  TCCR1B |= (1 << CS12);                                   // 256 prescale value
  TIMSK1 |= (1 << OCIE1A);                                 // activate Timer Compare Interrupt
  interrupts();
}

/**
 * Interrupt service routine for Timer1.
 * Is triggered when TCNT1 equals OCR1A.
 * Meaning it is called when Timer1 reaches the value defined during intialization.
 */
ISR(TIMER1_COMPA_vect)
{
  TCNT1 = 0;                                           // (re-)initialize register with 0
  updateTime = true;                                   // trigger time update
  shouldEvaluateIRresults = irrecv.decode(&irResults); // check IR receiver
  irCtr++;                                             // increase work-around counter
}

/**
 * Interrupt service routine for RTC.
 * Used to wake up the Arduino from power down state.
 * Will be called when RTC detects a matching for the activated schedule.
 */
void isrAlarm()
{
  if (energy.WasSleeping())
  {
    /* Will only be true when Arduino was in deep sleep before calling the routine.
       Do some re-init things when needed. */
  }
  isrAlarmWasCalled = true;
}

/**
 * MAIN HANDLER FUNCTIONS
 */

/**
 * Reset color array to black.
 */
void clearLedColors(CRGB *leds)
{
  fill_solid(leds, LED_PIXELS, CRGB::Black);
}

/**
 * Set start of sentence
 */
void setColorForStartOfSentence(CRGB *leds)
{
  wordclock.setColorForWord(leds, IT);
  wordclock.setColorForWord(leds, IS);
}

/**
 * Calculate the index for the five minute step.
 */
int getIndexForFiveMinuteStep(int minutes)
{
  if (minutes < 30)
    return floor(t.Minute / MIN_STEP);

  if (minutes > 30)
    // -2 because: -1 in general because its an array length, -1 again because we cannot use 'half'
    return floor((MIN_PARTS - 2) - ((t.Minute - 31) / MIN_STEP));

  return 5; // index for 'half'
}

/**
 * Sets the color for the 5-Minute step.
 */
void setColorForFiveMinuteStep(CRGB *leds, int minutes)
{
  if (!wordclock.shouldShowMinutes(minutes))
    return;

  // WARNING: will only be valid for mins in range of [0...55],
  //          will cause an out-of-bounds exception for mins > 55
  int idx = getIndexForFiveMinuteStep(minutes);
  wordclock.setColorForWord(leds, W_MINS[idx]);
}

/**
 * Set the color for the relation, if 'to' or 'past'.
 */
void setColorForRelation(CRGB *leds, int minutes)
{
  if (!wordclock.shouldShowMinutes(minutes))
    return;

  if (minutes > 30)
    return wordclock.setColorForWord(leds, TO); // to

  return wordclock.setColorForWord(leds, PAST); // past
}

/**
 * Set the color for the current hour.
 */
void setColorForHour(CRGB *leds, int hours, int minutes)
{
  int index = (minutes > 30)
                  ? (int)ceil((t.Hour + 1) % 12) // to
                  : (int)ceil(t.Hour % 12);      // past

  wordclock.setColorForWord(leds, W_HOURS[index]);
}

/**
 * Set the color for daytime, AM or PM.
 */
void setColorForDaytime(CRGB *leds, int hours)
{
  if (hours > 12)
    wordclock.setColorForWord(leds, PM);
  else
    wordclock.setColorForWord(leds, AM);
}

/**
 * Set color for the digits representing the actual minutes.
 */
void setColorForMinuteDigits(CRGB *leds, int minutes)
{
  int m1 = int(minutes / 10);        // only "first digit" of minute value, e.g. 34 => 3
  int m2 = int(minutes - (m1 * 10)); // only "second digit" of minute value, e.g. 34 => 4

  // could have an additional check if m1 == m2, but would only prevent one call
  wordclock.setColorForDigit(leds, DIGITS[m1]);
  wordclock.setColorForDigit(leds, DIGITS[m2]);
}

/**
 * Print a time string.
 */
void debugTime(tmElements_t &tm)
{
  DBG_PRINT(tm.Hour);
  DBG_PRINT(":");
  DBG_PRINT(tm.Minute);
  DBG_PRINT(":");
  DBG_PRINT(tm.Second);
  DBG_PRINTLN();
}

void handleDisplayTime()
{
  if (!updateTime)
    return;

  if (isTimeUpdateRunning)
    return;

  updateTime = false;         // reset flag
  isTimeUpdateRunning = true; // prevent unecessary execution

  // get time values
  theClock.read(t);
  debugTime(t);

  // set colors
  clearLedColors(leds);
  setColorForStartOfSentence(leds);
  setColorForFiveMinuteStep(leds, t.Minute);
  setColorForRelation(leds, t.Minute);
  setColorForHour(leds, t.Hour, t.Minute);
  setColorForDaytime(leds, t.Hour);
  setColorForMinuteDigits(leds, t.Minute);

  isTimeUpdateRunning = false; // reset flag
}

void handleIRresults()
{
  if (!shouldEvaluateIRresults)
  {
    DBG_PRINTLN("no ir result to check");
    return;
  }
  if (evaluatingIRresults)
  {
    DBG_PRINTLN("still evaluating ir result");
    return;
  }

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
  if (irResults.decode_type != NEC)
    return;

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

  uint16_t valueToCheck = (result & 0xffff); // for the 'why?' see init section for IR_*

  switch (valueToCheck)
  {
  // animations
  case IR_ZERO:
    setLEDModeState(">>NORMAL", LED_MODE_NORMAL, 25);
    break;
  case IR_ONE:
    setLEDModeState(">>RAINBOW", LED_MODE_RAINBOW, 60);
    break;
  case IR_TWO:
    setLEDModeState(">>RAINBOW GLITTER", LED_MODE_RAINBOW_GLITTER, 60);
    break;
  case IR_THREE:
    setLEDModeState(">>CONFETTI", LED_MODE_CONFETTI, 60);
    break;
  case IR_FOUR:
    setLEDModeState(">>SINELON", LED_MODE_SINELON, 60);
    break;
  case IR_FIVE:
    setLEDModeState(">>BPM", LED_MODE_BPM, 60);
    break;
  case IR_SIX:
    setLEDModeState(">>JUGGLE", LED_MODE_JUGGLE, 60);
    break;
  case IR_SEVEN:
    setLEDModeState(">>MATRIX", LED_MODE_MATRIX, 25);
    break;
  // brightness
  case IR_VOL_UP:
    DBG_PRINTLN("BRIGHTNESS++");
    wordclock.increaseBrightness(LED_BRIGHTNESS_STEP);
    break;
  case IR_VOL_DOWN:
    DBG_PRINTLN("BRIGHTNESS--");
    wordclock.increaseBrightness(LED_BRIGHTNESS_STEP * -1);
    break;
  // hue
  case IR_UP:
    DBG_PRINTLN("HUE++");
    wordclock.increaseHue(LED_HUE_STEP);
    break;
  case IR_DOWN:
    DBG_PRINTLN("HUE--");
    wordclock.increaseHue(LED_HUE_STEP * -1);
    break;
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
  case LED_MODE_NORMAL:
    handleDisplayTime();
    break;
  case LED_MODE_RAINBOW:
    wordclock.rainbow(leds);
    break;
  case LED_MODE_RAINBOW_GLITTER:
    wordclock.rainbowWithGlitter(leds);
    break;
  case LED_MODE_CONFETTI:
    wordclock.confetti(leds);
    break;
  case LED_MODE_SINELON:
    wordclock.sinelon(leds);
    break;
  case LED_MODE_BPM:
    wordclock.bpm(leds);
    break;
  case LED_MODE_JUGGLE:
    wordclock.juggle(leds);
    break;
  case LED_MODE_MATRIX:
    wordclock.matrix(leds);
    break;
  }

  if (isScheduleActive)
    wordclock.setColorForDigit(leds, SCHEDULE);

  if (blinkToConfirm)
  {
    blinkToConfirm = false;
    wordclock.setColorForWord(leds, CRGB::White, CHK);
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

  if (newBrightness != oldBrightness)
  {
    oldBrightness = newBrightness;
    FastLED.setBrightness(newBrightness);
  }

  FastLED.show();            // send the 'leds' array out to the actual LED strip
  FastLED.delay(1000 / fps); // insert a delay to keep the framerate modest

  // cycle through hue for some animations
  if ((ledMode == LED_MODE_RAINBOW) || (ledMode == LED_MODE_RAINBOW_GLITTER) || (ledMode == LED_MODE_SINELON) || (ledMode == LED_MODE_JUGGLE) || autoCycleHue)
    if (millis() % 20 == 0)
    {
      hue++;
    }
}

/**
 * Init RTC module.
 */
void Wordclock::initRTC(DS3232RTC &theClock)
{
  theClock.begin();
  theClock.setAlarm(DS3232RTC::ALM1_MATCH_DATE, 0, 0, 0, 1);
  theClock.setAlarm(DS3232RTC::ALM2_MATCH_DATE, 0, 0, 0, 1);
  theClock.alarm(DS3232RTC::ALARM_1);
  theClock.alarm(DS3232RTC::ALARM_2);
  theClock.alarmInterrupt(DS3232RTC::ALARM_1, false);
  theClock.alarmInterrupt(DS3232RTC::ALARM_2, false);
  theClock.squareWave(DS3232RTC::SQWAVE_NONE);
}

/**
 * Set RTC to a predefined time.
 */
void Wordclock::setRtcTime(DS3232RTC &theClock, int hours, int min)
{
  tmElements_t tm;

  // set the RTC time
  tm.Hour = hours;
  tm.Minute = min;

  // set some defaults
  tm.Second = 0;
  tm.Day = 1;
  tm.Month = 1;
  tm.Year = 0; // tmElements_t.Year is the offset from 1970

  theClock.write(tm);
}

/**
 * Determine if minutes should be displayed.
 * This is not necessary if minutes are 0.
 * Or, in range of 55 to 60, as the clock shows "future" 5-minute-steps.
 */
bool Wordclock::shouldShowMinutes(int mins)
{
  return ((mins < 56) || (mins == 0));
}

/**
 * Check if schedule should be applied.
 */
bool Wordclock::shouldGoToSleep(tmElements_t &tm)
{
  if (!isScheduleActive)
  {
    return false;
  }

  return ((RTC_ALIVE_FROM > tm.Hour && RTC_ALIVE_FROM_MIN > tm.Minute) ||
          (tm.Hour >= RTC_ALIVE_TO && tm.Minute >= RTC_ALIVE_TO_MIN));
}

/**
 * (Re)set the alarm schedule.
 */
void Wordclock::setAlarmSchedule(DS3232RTC &theClock)
{
  // set Alarm 2
  theClock.setAlarm(DS3232RTC::ALM2_MATCH_HOURS, 0, RTC_WAKE_UP_MINS, RTC_WAKE_UP_HRS, 0);
  // clear the alarm flags
  theClock.alarm(DS3232RTC::ALARM_1);
  theClock.alarm(DS3232RTC::ALARM_2);
  // configure the INT/SQW pin for "interrupt" operation (disable square wave output)
  theClock.squareWave(DS3232RTC::SQWAVE_NONE);
  // enable interrupt output for Alarm 2 only
  theClock.alarmInterrupt(DS3232RTC::ALARM_1, false);
  theClock.alarmInterrupt(DS3232RTC::ALARM_2, true);
}

/**
 * Set alarm schedule and immediately go
 * into power down state.
 */
void Wordclock::setAlarmScheduleAndEnterLowPower(DS3232RTC &theClock, Energy &energy)
{
  this->setAlarmSchedule(theClock);
  this->enterLowPower(energy);
}

/**
 * Enter low power mode.
 * Arduino will only wake up again through
 * interrupt from external module.
 */
void Wordclock::enterLowPower(Energy &energy)
{
  energy.PowerDown();
}

/**
 * Increase pixel brightness.
 * Value range is [0 ... 80].
 * 100 is not an option to save LED lifetime.
 * For decreasing use negative step size.
 */
void Wordclock::increaseBrightness(int stepSize)
{
  if (newBrightness + stepSize > 200)
    newBrightness = 200;
  else if (newBrightness + stepSize < 0)
    newBrightness = 0;
  else
    newBrightness += stepSize;
}

/**
 * Increase base hue value.
 * For decreasing use negative step size.
 */
void Wordclock::increaseHue(int stepSize)
{
  /*
  FastLED uses range [0...255] to represent
  hue values. Hence, we do not need to check
  borders and just let uint8_t overflow.
  */
  hue += stepSize;
}

/**
 * Color pixels for given word.
 */
void Wordclock::setColorForWord(CRGB *leds, Word _word)
{
  for (int i = 0; i < _word.size; i++)
  {
    int ledNo = _word.leds[i];
    if (ledNo < LED_PIXELS)
      leds[ledNo].setHue(hue);
  }
}

/**
 * Color pixels for given word.
 */
void Wordclock::setColorForWord(CRGB *leds, const struct CRGB color, Word _word)
{
  for (int i = 0; i < _word.size; i++)
  {
    int ledNo = _word.leds[i];
    if (ledNo < LED_PIXELS)
      leds[ledNo] = color;
  }
}

/**
 * Color pixels for given digit.
 */
void Wordclock::setColorForDigit(CRGB *leds, Digit digit)
{
  int ledNo = digit.led;
  if (ledNo < LED_PIXELS)
    leds[ledNo].setHue(hue);
}

/**
 * Fill strip with rainbow colors.
 */
void Wordclock::rainbow(CRGB *leds)
{
  fill_rainbow(leds, LED_PIXELS, hue, 7); // FastLED's built-in rainbow generator
}

// void Wordclock::addGlitter(CRGB *leds, fract8 chanceOfGlitter)
// {
//   if (random8() < chanceOfGlitter)
//     leds[random16(LED_PIXELS)] += CRGB::White;
// }

/**
 * Rainbow pattern with white spots.
 */
void Wordclock::rainbowWithGlitter(CRGB *leds)
{
  this->rainbow(leds);
  this->addGlitter(leds, 80);
}

/**
 * Random colored speckles that blink in and fade smoothly.
 */
void Wordclock::confetti(CRGB *leds)
{
  fadeToBlackBy(leds, LED_PIXELS, 10);
  int pos = random16(LED_PIXELS);
  leds[pos] += CHSV(hue + random8(64), 200, 255);
}

/**
 * A colored dot sweeping back and forth, with fading trails
 */
void Wordclock::sinelon(CRGB *leds)
{
  fadeToBlackBy(leds, LED_PIXELS, 20);
  int pos = beatsin16(13, 0, LED_PIXELS - 1);
  leds[pos] += CHSV(hue, 255, 192);
}

/**
 * Colored stripes pulsing at a defined Beats-Per-Minute (BPM)
 */
void Wordclock::bpm(CRGB *leds)
{
  uint8_t BeatsPerMinute = 62;
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8(BeatsPerMinute, 64, 255);
  for (int i = 0; i < LED_PIXELS; i++) // 9948
  {
    leds[i] = ColorFromPalette(palette, hue + (i * 2), beat - hue + (i * 10));
  }
}

/**
 * Eight colored dots, weaving in and out of sync with each other.
 */
void Wordclock::juggle(CRGB *leds)
{
  fadeToBlackBy(leds, LED_PIXELS, 20);
  byte dothue = 0;
  for (int i = 0; i < 8; i++)
  {
    leds[beatsin16(i + 7, 0, LED_PIXELS - 1)] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
}

/**
 * Let letters blink like in the in the movie 'Matrix'
 * FIXME: timing issues and new letter spawning
 */
void Wordclock::matrix(CRGB *leds)
{
  /*
  We are using a strip here
  and to make even more confusing
  the index count alternates, like:
     1  2  3  4
     8  7  6  5
     9 10 11 12 ...
*/

  fadeToBlackBy(leds, LED_PIXELS, 20);

  // copy existing rows to following rows to have a flow effect
  for (int i = LED_ROWS - 2; i > 0; i--)
  {
    for (int j = LED_COLUMNS - 1; j > 0; j--)
    {
      if (i % 2 == 0)
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
  leds[pos] = CHSV(hue, 255, 192);
}
