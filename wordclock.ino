
/**
 * Word clock.
 * 
 * A word clock implementation for English language.
 * This clock lights up LEDs (ws2812b) to visualize words using the overlaying template.
 * LEDs are laid out as a strip where the id counting between rows alternates.
 * So the pattern for counting LEDs is:
 * 
 * row1::rtl, row2::ltr, row3::rtl, row4::ltr, ...
 * 
 * Further a RTC module (ds3231) is used to keep track of time.
 * And an IR module allows to set/correct the time, display animations, etc. via remote control.
 * 
 * Author: Christian Hansen
 * Date: 02.01.2019
 * Version: 0.1
 */
 
// INCLUDES
#include <math.h>
#include <Adafruit_NeoPixel.h>
#include <DS3231.h>
#include <IRremote.h>

// DEFINITIONS
#define LED_DATA_PIN 2
#define LED_PIXELS 121
#define LED_MODE_NORMAL 0
#define LED_MODE_RAINBOW 1
#define LED_MODE_CONFETTI 2
#define LED_MODE_CONFETTI_RAINBOW 3
#define LED_MODE_RUNNER 4
#define LED_MODE_RUNNER_RAINBOW 5
#define LED_BRIGHTNESS_STEP 10
#define LED_HUE_STEP 10

#define RTC_HRS 0
#define RTC_MINS 1
#define RTC_SECS 2

#define IR_RECEIVE_PIN 3

#if !defined(ARRAY_SIZE)
    #define ARRAY_SIZE(x) (sizeof((x)) / sizeof((x)[0]))
#endif

// STRUCTURES
//template <int N>
struct wording
{
  char * text;
  int * leds; 
};
typedef struct wording Word;

Word* Word_new(const char * text, int pins[])
{
  int len = sizeof(pins)/sizeof(pins[0]);
  Serial.println(len);
  Word* w = malloc(sizeof(Word) + len * sizeof(int));
  if (w) {
    //w->text = text;
    w->leds = pins;  
  }
  return w;
}

// VARIABLES
// ... Words in order of appearence ...
// ... Word::begin ...
const uint8_t IT[2] = {0,1};
const uint8_t IS[2] = {3,4};

// ... Word::5 minute steps ...
const uint8_t HALF_M[4]    = {6,7,8,9};
const uint8_t TEN_M[3]     = {21,20,19};
const uint8_t QUARTER_M[7] = {17,16,15,14,13,12,11};
const uint8_t TWENTY_M[6]  = {23,24,25,26,27,28};
const uint8_t FIVE_M[4]    = {29,30,31,32};
const uint8_t TWENTYFIVE_M[10] = {23,24,25,26,27,28,29,30,31,32}; // only for simplicity

// ... Word::relation ...
const uint8_t TO[2]   = {43,42};
const uint8_t PAST[4] = {41,40,39,38};

// ... Word::hours ...
const uint8_t FOUR_H[4]   = {36,35,34,33};
const uint8_t FIVE_H[4]   = {44,45,46,47};
const uint8_t TWO_H[3]    = {48,49,50};
const uint8_t NINE_H[4]   = {51,52,53,54};
const uint8_t THREE_H[5]  = {65,64,63,62,61};
const uint8_t TWELVE_H[6] = {60,59,58,57,56,55};
const uint8_t ELEVEN_H[6] = {67,68,69,70,71,72};
const uint8_t ONE_H[3]    = {73,74,75};
const uint8_t SEVEN_H[5]  = {87,86,85,84,83};
const uint8_t EIGHT_H[5]  = {81,80,79,78,77};
const uint8_t TEN_H[3]    = {89,90,91};
const uint8_t SIX_H[3]    = {92,93,94};

// ... Word::part of day ...
const uint8_t AM[2] = {107,106};
const uint8_t PM[2] = {102,101};

// ... Word::numbers ...
const uint8_t ZERO_N[1]  = {111};
const uint8_t ONE_N[1]   = {112};
const uint8_t TWO_N[1]   = {113};
const uint8_t THREE_N[1] = {114};
const uint8_t FOUR_N[1]  = {115};
const uint8_t FIVE_N[1]  = {116};
const uint8_t SIX_N[1]   = {117};
const uint8_t SEVEN_N[1] = {118};
const uint8_t EIGHT_N[1] = {119};
const uint8_t NINE_N[1]  = {120};

//const uint8_t *minutes[6] = { FIVE_M, TEN_M, QUARTER_M, TWENTY_M, TWENTYFIVE_M, HALF_M };
//const uint8_t hours[12]  = { ONE_H, TWO_H, THREE_H, FOUR_H, FIVE_H, SIX_H, SEVEN_H, EIGHT_H, NINE_H, TEN_H, ELEVEN_H, TWELVE_H };
//const uint8_t digits[10] = { ZERO_N, ONE_N, TWO_N, THREE_N, FOUR_N, FIVE_N, SIX_N, SEVEN_N, EIGHT_N, NINE_N };

// ... LED ...
const double hsvSpacing = 360.0 / 16.0;
uint8_t ledMode = LED_MODE_NORMAL;
uint16_t hue = 0;
uint8_t ledBrightness = 20; // in %, to avoid division will be multiplied by 0.01 before application, used for value of HSV color
uint8_t pixelFades[LED_PIXELS] = {}; // used for animations

// ... RTC ...
int hrs = 0;
int mins = 0;
int secs = 0;

// ... IR ...
decode_results irResults;
const int IR_ZERO  = 0xFF6897;
const int IR_ONE   = 0xFF30CF;
const int IR_TWO   = 0xFF18E7;
const int IR_THREE = 0xFF7A85;
const int IR_FOUR  = 0xFF10EF;
const int IR_FIVE  = 0xFF38C7;
const int IR_VOL_UP   = 0xFF629D;
const int IR_VOL_DOWN = 0xFFA857;
const int IR_UP       = 0xFF906F;
const int IR_DOWN     = 0xFFE01F;
const int IR_POWER    = 0xFFA25D;

// INITIALIZATIONS
Adafruit_NeoPixel ledStrip = Adafruit_NeoPixel(LED_PIXELS, LED_DATA_PIN, NEO_GRB + NEO_KHZ800);
DS3231 rtc(SDA, SCL); // Analog 4, Analog 5 for Arduino Nano, lib only uses 24h format, no alarms
IRrecv irrecv(IR_RECEIVE_PIN);

// MACROS


// FUNCTIONS
// ... Utility functions ...
/**
 * Calculate RGB color values from HSV values.
 */
void HSVtoRGB(double& r, double& g, double& b, double h, double s, double v) {
  if (s == 0.0) {
    r = v;
    g = v;
    b = v;
  }
  int i = int(h * 6.0); // assume int() truncates!
  double f = (h * 6.0) - i;
  double p = v * (1.0 - s);
  double q = v * (1.0 - s * f);
  double t = v * (1.0 - s * (1.0 - f));
  i = i % 6;
  if (i == 0) {
    r = v; g = t; b = p; return; // v, t, p  
  }
  if (i == 1) {
    r = q; g = v; b = p; return; // q, v, p
  }
  if (i == 2) {
    r = p; g = v; b = t; return; // p, v, t
  }
  if (i == 3) {
    r = p; g = q; b = v; return; // p, q, v
  }
  if (i == 4) {
    r = t; g = p; b = v; return; // t, p, v
  }
  if (i == 5) {
    r = v; g = p; b = q; return; // v, p, q
  }
}

/**
 * Return substring from string by index and separator.
 * https://stackoverflow.com/questions/9072320/split-string-into-string-array
 */
String getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length()-1;

  for(int i=0; i<=maxIndex && found<=index; i++){
    if(data.charAt(i)==separator || i==maxIndex){
        found++;
        strIndex[0] = strIndex[1]+1;
        strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  }

  return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
}

// ... RTC functions ...
/**
 * Get int value of certain part of time string returned from TRC module.
 */
int getIntTimePart(int format) 
{
  return getValue(rtc.getTimeStr(), ':', format).toInt();
}

/**
 * Determine if minutes should be displayed.
 * This is not necessary if minutes are 0. 
 * Or, in range of 55 to 60, as the clock shows "future" 5-minute-steps.
 */
bool showMinutes(int mins)
{
  return (mins < 56 || mins == 0);
}

// ... LED functions ...
/**
 * Increase pixel brightness.
 * Value range is [0 ... 80].
 * 100 is not an option to save LED lifetime.
 * For decreasing use negative step size.
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
 * Increase base hue value.
 * Value range is [0 ... 360].
 * For decreasing use negative step size.
 */
void increaseHue(int stepSize)
{
  hue += stepSize;
  if (hue > 360) hue -= 360;
  if (hue < 0) hue += 360;
}

/**
 * Color pixels for given word.
 */
void setColorForWord(uint8_t *_word, int len)
{
  double h = hue % 360 / 360.0; // modulo is expensive
  double r, g, b;
  HSVtoRGB(r, g, b, h, 1.0, ledBrightness * 0.01);
  for (int i = 0; i < len; i++)
  {  
    if (_word[i] < LED_PIXELS)
      ledStrip.setPixelColor(_word[i], int(255.0 * r), int(255.0 * g), int(255.0 * b));
  }
}

// ... LED::animation patterns ...
/**
 * Set all pixels to black/off state.
 */
void setAllBlack()
{
  for(int i = 0; i < LED_PIXELS; i++) {
    ledStrip.setPixelColor(i, 0, 0, 0);
    pixelFades[i] = 0;
  } 

  //ledStrip.show();
}

/**
 * Fade pixels to black off state by given step size.
 */
void fadeToBlack(uint8_t stepSize)
{
  for (int i = 0; i < LED_PIXELS; i++) 
  {
    if (pixelFades[i] - stepSize < 0)
      pixelFades[i] = 0;
    else
      pixelFades[i] -= stepSize;
  }
}

/**
 * Show pixels in rainbow colors.
 */
void rainbow() 
{
  hue++;
  if (hue > 360) hue -= 360;
  
  for (int i = 0; i < LED_PIXELS; i++) 
  {
    double offset = i * hsvSpacing;
    double h = int(hue + offset) % 360 / 360.0; // modulo is expensive    
    double r, g, b;
    HSVtoRGB(r, g, b, h, 1.0, ledBrightness * 0.01);
    ledStrip.setPixelColor(i, int(255.0 * r), int(255.0 * g), int(255.0 * b));
  }

  // finally trigger LED strip
  ledStrip.show();
}

/**
 * Let pixels sparkle.
 */
void confetti()
{
  fadeToBlack(10);
  int pos = random(LED_PIXELS);
  pixelFades[pos] = 80;
  for (int i = 0; i < LED_PIXELS; i++)
  {
    if (i == pos) {
      ledStrip.setPixelColor(i, 255, 255, 255);
    } else {
      double h = hue % 360 / 360.0; // modulo is expensive
      double r, g, b;
      HSVtoRGB(r, g, b, h, 1.0, pixelFades[i] * 0.01);
      ledStrip.setPixelColor(i, int(255.0 * r), int(255.0 * g), int(255.0 * b));  
    }
  }

  // finally trigger LED strip
  ledStrip.show();
}

/**
 * Let pixels sparkle in different colors.
 */
void rainbowConfetti()
{
  fadeToBlack(10);
  int pos = random(LED_PIXELS);
  pixelFades[pos] = 80;
  for (int i = 0; i < LED_PIXELS; i++)
  {
    double h = random(361) % 360 / 360.0; // modulo is expensive
    double r, g, b;
    HSVtoRGB(r, g, b, h, 1.0, pixelFades[i] * 0.01);
    ledStrip.setPixelColor(i, int(255.0 * r), int(255.0 * g), int(255.0 * b));  
  }

  // finally trigger LED strip
  ledStrip.show();
}

/**
 * Let a pixel run along the strip.
 */
void runner()
{
  // find position with brightest value
  uint8_t pos = 0;
  int maxVal = 0;
  for (int i = 0; i < LED_PIXELS; i++)
  {
    if (pixelFades[i] > maxVal) {
      maxVal = pixelFades[i];
      pos = i;
    }
  }

  // fade all
  fadeToBlack(10);

  // set next first
  if (pos + 1 == LED_PIXELS) {
    pixelFades[0] = 80;  
  } else {
    pixelFades[pos + 1] = 80;  
  }

  // color strip
  for (int i = 0; i < LED_PIXELS; i++)
  {
    double h = hue % 360 / 360.0; // modulo is expensive
    double r, g, b;
    HSVtoRGB(r, g, b, h, 1.0, pixelFades[i] * 0.01);
    ledStrip.setPixelColor(i, int(255.0 * r), int(255.0 * g), int(255.0 * b));  
  }

  // finally trigger LED strip
  ledStrip.show();
}

/**
 * Let a pixel run along the strip in rainbow style.
 */
void rainbowRunner()
{
  // find position with brightest value
  uint8_t pos = 0;
  int maxVal = 0;
  for (int i = 0; i < LED_PIXELS; i++)
  {
    if (pixelFades[i] > maxVal) {
      maxVal = pixelFades[i];
      pos = i;
    }
  }

  // fade all
  fadeToBlack(10);

  // set next first
  if (pos + 1 == LED_PIXELS) {
    pixelFades[0] = 80;  
  } else {
    pixelFades[pos + 1] = 80;  
  }

  // color strip
  for (int i = 0; i < LED_PIXELS; i++)
  {
    double h = random(361) % 360 / 360.0; // modulo is expensive
    double r, g, b;
    HSVtoRGB(r, g, b, h, 1.0, pixelFades[i] * 0.01);
    ledStrip.setPixelColor(i, int(255.0 * r), int(255.0 * g), int(255.0 * b));  
  }

  // finally trigger LED strip
  ledStrip.show();
}

void displayMinutes(int _mins)
{
  if (showMinutes(mins)) { // TODO: simplify?
    if (mins > 0 && mins < 6) {

      setColorForWord(FIVE_M, ARRAY_SIZE(FIVE_M)); // five
    } else if (mins > 5 && mins < 11) {

      setColorForWord(TEN_M, ARRAY_SIZE(TEN_M)); // ten
    } else if (mins > 10 && mins < 16) {

      setColorForWord(QUARTER_M, ARRAY_SIZE(QUARTER_M)); // quarter
    } else if (mins > 15 && mins < 21) {

      setColorForWord(TWENTY_M, ARRAY_SIZE(TWENTY_M)); // twenty
    } else if (mins > 20 && mins < 26) {

      setColorForWord(TWENTYFIVE_M, ARRAY_SIZE(TWENTYFIVE_M)); // twentyfive
    } else if (mins > 25 && mins < 31) {

      setColorForWord(HALF_M, ARRAY_SIZE(HALF_M)); // half
    } else if (mins > 30 && mins < 36) { // switch from 'past' to 'to'

      setColorForWord(TWENTYFIVE_M, ARRAY_SIZE(TWENTYFIVE_M)); // twentyfive
    } else if (mins > 35 && mins < 41) {

      setColorForWord(TWENTY_M, ARRAY_SIZE(TWENTY_M)); // twenty
    } else if (mins > 40 && mins < 46) {

      setColorForWord(QUARTER_M, ARRAY_SIZE(QUARTER_M)); // quarter
    } else if (mins > 45 && mins < 51) {

      setColorForWord(TEN_M, ARRAY_SIZE(TEN_M)); // ten
    } else if (mins > 50 && mins < 56) {

      setColorForWord(FIVE_M, ARRAY_SIZE(FIVE_M)); // five
    }
  }
}

void displayHours(int _hours)
{
  switch(_hours) {
    case 1:
      setColorForWord(ONE_H, ARRAY_SIZE(ONE_H));
      break;
    case 2:
      setColorForWord(TWO_H , ARRAY_SIZE(TWO_H));
      break;
    case 3:
      setColorForWord(THREE_H , ARRAY_SIZE(THREE_H));
      break;
    case 4:
      setColorForWord(FOUR_H, ARRAY_SIZE(FOUR_H));
      break;
    case 5:
      setColorForWord(FIVE_H, ARRAY_SIZE(FIVE_H));
      break;
    case 6:
      setColorForWord(SIX_H, ARRAY_SIZE(SIX_H));
      break;
    case 7:
      setColorForWord(SEVEN_H , ARRAY_SIZE(SEVEN_H));
      break;
    case 8:
      setColorForWord(EIGHT_H , ARRAY_SIZE(EIGHT_H));
      break;
    case 9:
      setColorForWord(NINE_H , ARRAY_SIZE(NINE_H));
      break;
    case 10:
      setColorForWord(TEN_H , ARRAY_SIZE(TEN_H));
      break;
    case 11:
      setColorForWord(ELEVEN_H , ARRAY_SIZE(ELEVEN_H));
      break;
    case 0:  
    case 12:
      setColorForWord(TWELVE_H , ARRAY_SIZE(TWELVE_H));
      break;
  }
}

void displayDigit(int idx)
{
  switch(idx) {
    case 0:
      setColorForWord(ZERO_N, ARRAY_SIZE(ZERO_N));
      break;
    case 1:
      setColorForWord(ONE_N, ARRAY_SIZE(ONE_N));
      break;
    case 2:
      setColorForWord(TWO_N, ARRAY_SIZE(TWO_N));
      break;
    case 3:
      setColorForWord(THREE_N, ARRAY_SIZE(THREE_N));
      break;
    case 4:
      setColorForWord(FOUR_N, ARRAY_SIZE(FOUR_N));
      break;
    case 5:
      setColorForWord(FIVE_N, ARRAY_SIZE(FIVE_N));
      break;
    case 6:
      setColorForWord(SIX_N, ARRAY_SIZE(SIX_N));
      break;
    case 7:
      setColorForWord(SEVEN_N, ARRAY_SIZE(SEVEN_N));
      break;
    case 8:
      setColorForWord(EIGHT_N, ARRAY_SIZE(EIGHT_N));
      break;
    case 9:
      setColorForWord(NINE_N, ARRAY_SIZE(NINE_N));
      break;
  }
}

// MAIN
/**
 * Main function to determine which words to highlight to show time.
 */
void handleDisplayTime()
{
  // get time values
  hrs = getIntTimePart(RTC_HRS);
  mins = getIntTimePart(RTC_MINS);
  secs = getIntTimePart(RTC_SECS);

  Serial.println("Time is:");
  Serial.println((String)hrs + ":" + mins + ":" + secs);

  // reset all pixel colors to black
  setAllBlack();
  
  // set start of sentence
  setColorForWord(IT, ARRAY_SIZE(IT));
  setColorForWord(IS, ARRAY_SIZE(IS));


  // set 5-minute-step
  displayMinutes(mins);

  // set relation
  if (showMinutes(mins)) {
    if (mins > 30) {
      setColorForWord(TO, ARRAY_SIZE(TO));
    } else {
      setColorForWord(PAST, ARRAY_SIZE(PAST));
    }
  }

  // set hour
  if (mins > 30) {
    int idx = int(ceil(hrs % 12) + 1);
    displayHours(idx); // to
  } else {
    int idx = int(ceil(hrs % 12));
    displayHours(idx); // past
  }
  
  // set daytime
  if (hrs > 12) {
    setColorForWord(PM, ARRAY_SIZE(PM));
  } else {
    setColorForWord(AM, ARRAY_SIZE(AM));
  }

  // set digits for minute and second
  int m = int(mins / 10);  // only "first digit" of minute value, e.g. 34 => 3
  int s = mins - (m * 10); // only "second digit" of minute value, e.g. 34 => 4

  if (m == s) {
    displayDigit(m);
  } else {
    displayDigit(m);
    displayDigit(s);
  }
 
  // finally trigger LED strip
  ledStrip.show();
}

/**
 * Main function to check for any IR inputs and change states according to it.
 */
void handleIRresults()
{
  // simply print for now
  if (irrecv.decode(&irResults)) {
    
    Serial.println(irResults.value, HEX);
    
    switch(int(irResults.value)) {
      case IR_ZERO:
        Serial.println("Mode >> NORMAL");
        ledMode = LED_MODE_NORMAL;
        setAllBlack();
        break;
      case IR_ONE:
        Serial.println("Mode >> RAINBOW");
        ledMode = LED_MODE_RAINBOW;
        setAllBlack();
        break;
      case IR_TWO:
        Serial.println("Mode >> CONFETTI");
        ledMode = LED_MODE_CONFETTI;
        setAllBlack();
        break;
      case IR_THREE:
        Serial.println("Mode >> CONFETTI RAINBOW");
        ledMode = LED_MODE_CONFETTI_RAINBOW;
        setAllBlack();
        break;
      case IR_FOUR:
        Serial.println("Mode >> RUNNER");
        ledMode = LED_MODE_RUNNER;
        setAllBlack();
        break;
      case IR_FIVE:
        Serial.println("Mode >> RUNNER RAINBOW");
        ledMode = LED_MODE_RUNNER_RAINBOW;
        setAllBlack();
        break;
      case IR_VOL_UP:
        Serial.println("BRIGHTNESS ++");
        increaseBrightness(LED_BRIGHTNESS_STEP);
        break;
      case IR_VOL_DOWN:
        Serial.println("BRIGHTNESS --");
        increaseBrightness(LED_BRIGHTNESS_STEP * -1);
        break;
      case IR_UP:
        Serial.println("HUE ++");
        increaseHue(LED_HUE_STEP);
        break;
      case IR_DOWN:
        Serial.println("HUE --");
        increaseHue(LED_HUE_STEP * -1);
        break;
      case IR_POWER:
        Serial.println("ALL BLACK");
        setAllBlack();
        break;
    }
    
    delay(500);
    irrecv.resume();  
  }
}

/**
 * Main setup function.
 */
void setup() 
{
  delay(1000); // init delay

  Serial.begin(9600);
  delay(100);
  Serial.println("Setup running ...");
  
  // RTC
  rtc.begin();
  //rtc.setDOW(MONDAY);     // Set Day-of-Week to SUNDAY
  //rtc.setTime(20, 36, 0);     // Set the time to 12:00:00 (24hr format)
  //rtc.setDate(7, 1, 2019);   // Set the date to January 1st, 2014
  Serial.println("RTC started ...");

  // LED
  ledStrip.begin();
  ledStrip.show(); // reset pixel colors
  Serial.println("LED started ...");

  // IR
  irrecv.enableIRIn();
  Serial.println("IR started ...");

  randomSeed(analogRead(0));
  
  delay(500);
}

/**
 * Main loop.
 * As there are different delays used when animations are running. 
 * Each handler function is responsible for triggering the LED strip by itself.
 */
void loop() 
{
  handleIRresults();
  
  switch(ledMode) {
    
    case LED_MODE_NORMAL:
      handleDisplayTime();
      delay(950);
      break;
      
    case LED_MODE_RAINBOW:
      rainbow();
      delay(50);
      break;
   
   case LED_MODE_CONFETTI:
      confetti();
      delay(50);
      break;

   case LED_MODE_CONFETTI_RAINBOW:
      rainbowConfetti();
      delay(50);
      break;

   case LED_MODE_RUNNER:
      runner();
      delay(50);
      break;

   case LED_MODE_RUNNER_RAINBOW:
      rainbowRunner();
      delay(50);
      break;
  }

  delay(20);
}
