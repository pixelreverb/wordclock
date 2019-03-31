#ifndef WORDCLOCK_HEADER
#define WORDCLOCK_HEADER

// ===================================
//
// This header file contains almost all project-relevant
// definitons and declarations. Nontheless, variable definitions
// which need additional libraries are kept within the 
// main implementation.
//
// Content:
// 1. Definitions
//    Pin and value definitions for:
//      - the Board
//      - LED pixels
//      - LED animation modes
//      - RTC
//      - IR
// 2. Structures
//      - Word
//      - Digit
// 3. Macros
// 4. Variables
//      - the Board
//      - Word definitions
//      - RTC
//      - IR
//      - LED
// 5. Method declarations
// ===================================


// ===================================
// DEFINITIONS
// ===================================

#define TIMER_PRESCALE 256
#define BOARD_FREQ 16000000 // 16 MHz

#define LED_DATA_PIN 4
#define LED_PIXELS 121  // amount of pixels
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
    /**
     * Short description for debugging.
     */
    const char * text;

    /**
     * Index array.
     */
    const uint8_t * leds;

    /**
     * Amount of LEDs used.
     */
    size_t size;
};
typedef struct wording Word;

struct digit
{
    /**
     * Short description for debugging.
     */
    const char * text;

    /**
     * Index.
     */
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
  
  But, also keep in mind that remote control protocols 
  vary and that the definitions below may only match 
  the remote control which is used with this project.
  
  So, you are advised to revisit your remote controls' 
  values.
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

bool blinkToConfirm = false;
uint8_t fps = 60;
uint8_t ledMode = LED_MODE_NORMAL;
uint8_t hue = 0;                      // FastLED's HSV range is from [0...255], instead of common [0...359]
uint8_t oldBrightness = 20;           // in %, to avoid division will be multiplied by 0.01 before application, used for value of HSV color
uint8_t newBrightness = 20;
bool incBrightness = true;


// ===================================
// METHODS
// ===================================


// ===================================
// ... RTC FUNCTIONS
// ===================================

/**
 * Init RTC module.
 */
void initRTC();

/**
 * Set RTC to a predefined time.
 */
void setRTCTime();

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
// ... LOW POWER FUNCTIONS
// ===================================

/**
 * Enter low poer mode.
 * Arduino will only wake up again through
 * interrupt from external module.
 */
void enterLowPower();


// ===================================
// .. LED FUNCTIONS
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
// ... LED WORD FUNCTIONS
// ===================================

/**
 * Color pixels for given word.
 */
void setColorForWord(Word _word);

/**
 * Color pixels for given digit.
 */
void setColorForDigit(Digit digit);


// ===================================
// ... LED ANIMATION FUNCTIONS
// ===================================

/**
 * Fill strip with rainbow colors.
 */
void rainbow();

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