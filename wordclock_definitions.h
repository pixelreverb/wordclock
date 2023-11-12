#define TIMER_PRESCALE 256
#define BOARD_FREQ 16000000 // 16 MHz

#define LED_DATA_PIN 4
#define LED_PIXELS 121 // amount of pixels
#define LED_ROWS 11    // needed for matrix-depended effects
#define LED_COLUMNS 11 // needed for matrix-depended effects
#define LED_TYPE WS2812B
#define LED_COLOR_ORDER GRB
// #define FRAMES_PER_SECOND 60

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
#define RTC_ALIVE_FROM 6     // hour of day
#define RTC_ALIVE_FROM_MIN 0 // minutes
#define RTC_ALIVE_TO 22      // hour of day
#define RTC_ALIVE_TO_MIN 30  // minutes
#define RTC_ALARM_PIN 2
#define RTC_WAKE_UP_HRS 6
#define RTC_WAKE_UP_MINS 0
#define MIN_STEP 5
#define MIN_PARTS 6

#define IR_RECEIVE_PIN 6
#define IR_PAUSE 3 // timer interrupts