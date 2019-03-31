# A Wordclock Implementation

## Hardware

Electronic parts:

- Arduino Nano
- WS2812B strip
- DS3231 RTC
- IR module

Image parts:

- Image frame from IKEA (50cm x 50cm)
- Laser-cut card board template (40cm x 40 cm)


**Arduino Nano**

The Arduino Nano is used for processing. The main reason to choose it was because it is small and does not consume much power. As the goal was to place all electronic parts within the image frame.

As a small tweak to save a bit more power the power LED can be removed from the Arduino. Also, the power LED then does not disturb the light from the LED strip depending on how everything is mounted.

If another board should be used check the pin assignments within the header file. Especially, for the interrupt pins.

**WS2812B**

Widely-used LED strip for DIY projects. A nice feature is that each pixel can be accessed by its index.

**DS3231**

RTC module which has the posibility of setting alarms and then to wake up the Arduino using interrupts.

**IR Module**

Used to communicate via remote control with the Arduino.

_Note_: When the data of the LED strip is updated, interrupts are disabled because of the precise timing needed for this process. Thus, IR interrupts will only be checked before a new update routine of the pixels is started.

---

## Features

- Set time via function call
    - Usually not used very often, therefore only as setup function
- Show time
    - In words
        - Hours
        - Five-minute steps
    - In Digits
        - Minutes (currently the only option)
- Define alarms
    - Define alarm start and stop
    - Activate via remote control
    - Deep sleep of Arduino board
- LED animations
    - Pixel animations
    - Cycle through color wheel
    - Cycle through brightness levels
- Change modes via remote control

---

## Resources

### About the clock

**LEDs**

The LED strip counts 121 pixels which are placed as 11 x 11 kind-of-matrix. The LED strip is layed out to have alternating indexes for each row. Simplified indexing works like this:

| x\*  | C1 | C2 | C3 | C4 | C5 |
| -- | -- | -- | -- | -- | -- | 
| R1 |  1 |  2 |  3 |  4 |  5 |
| R2 | 10 |  9 |  8 |  7 |  6 |
| R3 | 11 | 12 | 13 | 14 | 15 |

\* Within sources the indexes start at 0, of course.

**Word indexes**

The table shows the indexes of minutes first then hours at the end follow the digits.
Check out the template svg file to see the relations.


| Word       | Index(es) on LED strip |
| ---------- | ---------------------- |
| IT         | 0, 1                   |
| IS         | 3, 4                   |
| --         | --                     |
| FIVE       | 29, 30, 31, 32         |
| TEN        | 21, 20, 19             |
| QUARTER    | 7, 17, 16, 15, 14, 13, 12, 11 |
| TWENTY     | 23, 24, 25, 26, 27, 28 |
| TWENTYFIVE | 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 |
| HALF       | 6, 7, 8, 9             |
| --         | --                     |
| TO         | 43, 42                 |
| PAST       | 41, 40, 39, 38         |
| --         | --                     |
| TWELVE     | 60, 59, 58, 57, 56, 55 |
| ONE        | 73, 74, 75             |
| TWO        | 48, 49, 50             |
| THREE      | 65, 64, 63, 62, 61     |
| FOUR       | 36, 35, 34, 33         |
| FIVE       | 44, 45, 46, 47         |
| SIX        | 92, 93, 94             |
| SEVEN      | 87, 86, 85, 84, 83     |
| EIGHT      | 81, 80, 79, 78, 77     |
| NINE       | 51, 52, 53, 54         |
| TEN        | 89, 90, 91             |
| ELEVEN     | 67, 68, 69, 70, 71, 72 |
| --         | --                     |
| AM         | 107. 106               |
| PM         | 102, 101               |
| --         | --                     |
| X\*        | 110                    |
| --         | --                     |
| 0          | 111                    |
| 1          | 112                    |
| 2          | 113                    |
| 3          | 114                    | 
| 4          | 115                    | 
| 5          | 116                    |
| 6          | 117                    |
| 7          | 118                    |
| 8          | 119                    |
| 9          | 120                    |
| --         | --                     |
| CHK\*\*    | 64, 68, 84, 92, 82, 72, 58, 52, 34 |


\* Inidcate that the schedule/alarm is active.

\*\* Lights-up a check mark to visualize confirmation.


**IR handling**

As already mentioned above interrupts are disabled while the LED strip is updated. That means it is not possible to directly interrupt this process, and when the update process is run at 60 FPS one needs to be very quick to send a signal so that it is detected between two update cycles.

To circumvent that there is a work-around implemnted.
About every two seconds the Arduino's ISR is triggered. And within that it is checked if the IR module received a signal. 

Because it is not guaranteed that this signal can be used directly (it could be disturbed by any noise in the envirnment), this signal is used to stop the LED update cycles for a few seconds.

Within that timespan it is possible to send a second command (or some more if it is not detected directly). And when the command can be processed, modes or settings will be changed accordingly and the main routine is continued.

### Libraries used

 - WS2812B: 
    - https://github.com/FastLED/FastLED
 - DS3231:  
    - https://github.com/JChristensen/DS3232RTC
 - IR:      
    - https://github.com/z3t0/Arduino-IRremote
 - Enerlib: 
    - http://playground.arduino.cc/Code/Enerlib

---

## License