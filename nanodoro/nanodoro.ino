/**************************************************************************
 This is an example for our Monochrome OLEDs based on SSD1306 drivers

 Pick one up today in the adafruit shop!
 ------> http://www.adafruit.com/category/63_98

 This example is for a 128x64 pixel display using I2C to communicate
 3 pins are required to interface (two I2C and one reset).

 Adafruit invests time and resources providing this open
 source code, please support Adafruit and open-source
 hardware by purchasing products from Adafruit!

 Written by Limor Fried/Ladyada for Adafruit Industries,
 with contributions from the open source community.
 BSD license, check license.txt for more information
 All text above, and the splash screen below must be
 included in any redistribution.
 **************************************************************************/

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library.
// On an arduino UNO:       A4(SDA), A5(SCL)
// On an arduino MEGA 2560: 20(SDA), 21(SCL)
// On an arduino LEONARDO:   2(SDA),  3(SCL), ...
#define OLED_RESET -1        // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C  ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define NUMFLAKES 10  // Number of snowflakes in the animation example

#define LOGO_HEIGHT 16
#define LOGO_WIDTH 16
static const unsigned char PROGMEM logo_bmp[] = {
  0b00000000, 0b11000000,
  0b00000001, 0b11000000,
  0b00000001, 0b11000000,
  0b00000011, 0b11100000,
  0b11110011, 0b11100000,
  0b11111110, 0b11111000,
  0b01111110, 0b11111111,
  0b00110011, 0b10011111,
  0b00011111, 0b11111100,
  0b00001101, 0b01110000,
  0b00011011, 0b10100000,
  0b00111111, 0b11100000,
  0b00111111, 0b11110000,
  0b01111100, 0b11110000,
  0b01110000, 0b01110000,
  0b00000000, 0b00110000
};

#define PIN_SPEAKER 2
#define PIN_PB_0_0 7
#define PIN_PB_0_1 9
#define PIN_PB_1_0 10
#define PIN_PB_1_1 12

#define TIME_FOCUS 25 * 60
#define TIME_SHORTBREAK 5 * 60
#define TIME_LONGBREAK 15 * 60

typedef enum {
  POMO_STATE_INIT,
  POMO_STATE_RUNNING,
  POMO_STATE_PAUSED,
  POMO_STATE_STOPPED
} pomo_state_t;

pomo_state_t currentState = POMO_STATE_INIT;

int play_pause_button_pressed = 0;
int stop_button_pressed = 0;

int currentTimer;
int currentSession;
int should_beep = 0;
int is_beeping = 0;

char timeString[5];

unsigned long prevTime = 0;
unsigned long prevTimeSpeaker = 0;
unsigned long currentTime = 0;
unsigned long currentTimeSpeaker = 0;

void setup() {
  Serial.begin(9600);

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;  // Don't proceed, loop forever
  }

  //init speaker
  pinMode(PIN_SPEAKER, OUTPUT);
  digitalWrite(PIN_SPEAKER, 0);  //off


  //init buttons
  pinMode(PIN_PB_0_0, OUTPUT);
  digitalWrite(PIN_PB_0_0, 0);  //button down -> low
  pinMode(PIN_PB_0_1, INPUT_PULLUP);

  pinMode(PIN_PB_1_0, OUTPUT);
  digitalWrite(PIN_PB_1_0, 0);  //button down -> low
  pinMode(PIN_PB_1_1, INPUT_PULLUP);

  // Clear the buffer
  display.clearDisplay();

  currentTimer = TIME_FOCUS;
  currentSession = 0;
  play_pause_button_pressed = 0;
  stop_button_pressed = 0;

  updateDisplay();

  prevTimeSpeaker = prevTime = millis();

  for (;;) {  //timer logic
    if (!digitalRead(PIN_PB_0_1)) play_pause_button_pressed = 1;
    if (!digitalRead(PIN_PB_1_1)) stop_button_pressed = 1;
    currentTimeSpeaker = currentTime = millis();
    if (currentTime - prevTime >= 1000) {  //1s timer time up
      prevTime = currentTime;
      pomodoro_system_timer_callback();
    }
    if (currentTimeSpeaker - prevTimeSpeaker >= 250) {
      prevTimeSpeaker = currentTimeSpeaker;
      if (should_beep) pomodoro_speaker_timer_callback();
      else digitalWrite(PIN_SPEAKER, 0);  //off
    }
  }
}

void loop() {
}


void updateDisplay(void) {
  display.clearDisplay();
  display.setTextSize(4);               // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE);  // Draw white text
  display.setCursor(6, 15);             // Start at top-left corner
  display.cp437(true);                  // Use full 256 char 'Code Page 437' font

  // Not all the characters will fit on the display. This is normal.
  // Library will draw what it can and the rest will be clipped.
  sprintf(timeString,
          "%d%d:%d%d",
          (currentTimer / 60) / 10,
          (currentTimer / 60) % 10,
          (currentTimer % 60) / 10,
          (currentTimer % 60) % 10);
  display.write(timeString);

  display.display();
}

int getCurrentSessionTime() {
  switch (currentSession) {
    case 0:
    case 2:
    case 4:
    case 6:
      return TIME_FOCUS;
      break;
    case 1:
    case 3:
    case 5:
      return TIME_SHORTBREAK;
      break;
    case 7:
      return TIME_LONGBREAK;
      break;
    default:
      return 0;
      break;
  }
  return 0;
}

void pomodoro_system_timer_callback() {
  should_beep = 1;
  switch (currentState) {
    case POMO_STATE_INIT:
      should_beep = 0;
      currentSession = 0;
      currentTimer = getCurrentSessionTime();
      if (play_pause_button_pressed) {
        currentState = POMO_STATE_RUNNING;
        play_pause_button_pressed = 0;
      }
      if (stop_button_pressed) {
        stop_button_pressed = 0;
      }
      break;
    case POMO_STATE_RUNNING:
      should_beep = 0;
      if (--currentTimer == 0) {
        currentState = POMO_STATE_STOPPED;
        currentSession = (currentSession + 1) % 8;
        currentTimer = getCurrentSessionTime();
      } else {
        if (play_pause_button_pressed) {
          currentState = POMO_STATE_PAUSED;
          play_pause_button_pressed = 0;
        }
        if (stop_button_pressed) {
          currentState = POMO_STATE_STOPPED;
          currentTimer = getCurrentSessionTime();
          stop_button_pressed = 0;
        }
      }
      break;
    case POMO_STATE_PAUSED:
      if (play_pause_button_pressed) {
        currentState = POMO_STATE_RUNNING;
        play_pause_button_pressed = 0;
      }
      if (stop_button_pressed) {
        currentState = POMO_STATE_STOPPED;
        currentTimer = getCurrentSessionTime();
        stop_button_pressed = 0;
      }
      break;
    case POMO_STATE_STOPPED:
      if (play_pause_button_pressed) {
        currentState = POMO_STATE_RUNNING;
        play_pause_button_pressed = 0;
      }
      break;
    default:
      break;
  }
  play_pause_button_pressed = stop_button_pressed = 0;
  updateDisplay();
}

void pomodoro_speaker_timer_callback() {
  is_beeping ^= 1;
  digitalWrite(PIN_SPEAKER, is_beeping);
}
