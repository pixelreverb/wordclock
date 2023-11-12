/* Macro */
#define WORD(s, a)                     \
    {                                  \
        s, a, sizeof(a) / sizeof(a[0]) \
    }

/* Structs */
struct wording
{
    /**
     * Short description for debugging.
     */
    const char *text;

    /**
     * Index array.
     */
    const uint8_t *leds;

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
    const char *text;

    /**
     * Index.
     */
    uint16_t led;
};
typedef struct digit Digit;

/* Definitions */
const uint8_t A_IT[] = {0, 1};
const Word IT = WORD("IT", A_IT);

const uint8_t A_IS[] = {3, 4};
const Word IS = WORD("IS", A_IS);

const uint8_t A_M5[] = {29, 30, 31, 32};
const uint8_t A_M10[] = {21, 20, 19};
const uint8_t A_M15[] = {7, 17, 16, 15, 14, 13, 12, 11}; // >> "a quarter"
const uint8_t A_M20[] = {23, 24, 25, 26, 27, 28};
const uint8_t A_M25[] = {23, 24, 25, 26, 27, 28, 29, 30, 31, 32}; // only for simplicity
const uint8_t A_M30[] = {6, 7, 8, 9};
const Word W_MINS[] = {
    WORD("M5", A_M5), WORD("M10", A_M10), WORD("M15", A_M15),
    WORD("M20", A_M20), WORD("M25", A_M25), WORD("M30", A_M30)};

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
    WORD("H12", A_H12), WORD("H1", A_H1), WORD("H2", A_H2),
    WORD("H3", A_H3), WORD("H4", A_H4), WORD("H5", A_H5),
    WORD("H6", A_H6), WORD("H7", A_H7), WORD("H8", A_H8),
    WORD("H9", A_H9), WORD("H10", A_H10), WORD("H11", A_H11)};

const uint8_t A_AM[] = {107, 106};
const Word AM = WORD("AM", A_AM);

const uint8_t A_PM[] = {102, 101};
const Word PM = WORD("PM", A_PM);

const Digit SCHEDULE = {"S", 110}; // pseudo-digit
const Digit DIGITS[] = {
    {"D0", 111}, {"D1", 112}, {"D2", 113}, {"D3", 114}, {"D4", 115}, {"D5", 116}, {"D6", 117}, {"D7", 118}, {"D8", 119}, {"D9", 120}};

const uint8_t A_CHK[] = {64, 68, 84, 92, 82, 72, 58, 52, 34};
const Word CHK = WORD("CHK", A_CHK);