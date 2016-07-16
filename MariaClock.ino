#define FW_NAME "mariaclock"
#define FW_VERSION "2.0.12"
#include <Homie.h>
#include <TimeLib.h>   // https://github.com/PaulStoffregen/Time
#include <Timezone.h>  // https://github.com/tauonteilchen/Timezone https://github.com/JChristensen/Timezone
#include <Adafruit_LEDBackpack.h>
#include <Adafruit_GFX.h>

/* Magic sequence for Autodetectable Binary Upload */
const char *__FLAGGED_FW_NAME = "\xbf\x84\xe4\x13\x54" FW_NAME "\x93\x44\x6b\xa7\x75";
const char *__FLAGGED_FW_VERSION = "\x6a\x3f\x3e\x0e\xe1" FW_VERSION "\xb0\x30\x48\xd4\x1a";
/* End of magic sequence for Autodetectable Binary Upload */

// Central European Time (Berlin, Madrid, Paris, Rome, Warsaw)
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};  // Central European Summer Time
TimeChangeRule CET = {"CET", Last, Sun, Oct, 3, 60};    // Central European Standard Time
Timezone timezone(CEST, CET);
int timeLastMinute = -1; // last recorded minute

const int LUX_PIN = 0;
const int LUX_POLL_INTERVAL_MS = 100;   // 100 ms
const int LUX_SEND_INTERVAL_MS = 10000;  // 10 seconds
unsigned long luxLastPoll = 0;
unsigned long luxLastSent = 0;
int luxLastReading = 0;

// id, type, handler
HomieNode nLux("lux", "lux");
HomieNode nTime("time", "time");

Adafruit_7segment matrix = Adafruit_7segment();

void setup() {
  Homie.setBrand("kwo");
  Homie.setFirmware(FW_NAME, FW_VERSION);
  Homie.disableResetTrigger();
  Homie.enableBuiltInLedIndicator(false);
  Homie.registerNode(nLux);
  Homie.registerNode(nTime);
  Homie.setSetupFunction(hSetup);
  Homie.setLoopFunction(hLoop);
  Homie.setup();
}

void loop() {
  Homie.loop();
}

void hSetup() {
  matrix.begin(0x70);
  nTime.subscribe("value", hTime);
}

bool hTime(String value) {
  if (isNumeric(value)) {
    time_t t = value.toInt();
    setTime(t);
    if (Homie.setNodeProperty(nTime, "value", value, true)) {
      Serial.print("time set ");
      Serial.println(value);
    } else {
      Serial.println("set time value failed " + value);
    }
    return true;
  }
  Serial.println("set time failed " + value);
  return false;
}

void hLoop() {

  if (millis() - luxLastPoll >= LUX_POLL_INTERVAL_MS) {

    luxLastPoll = millis();

    int luxRaw = analogRead(LUX_PIN);
    int luxReading = map(constrain(luxRaw, 0, 511), 0, 511, 0, 15);
    
    matrix.setBrightness(luxReading);

    if (millis() - luxLastSent >= LUX_SEND_INTERVAL_MS) {
      if (luxReading != luxLastReading) {
        luxLastReading = luxReading;
        if (Homie.setNodeProperty(nLux, "value", String(luxLastReading), true)) {
          luxLastSent = millis();
        }
        Homie.setNodeProperty(nLux, "raw", String(luxRaw), true);
      }
    }

  }

  if (timeStatus() != timeNotSet) {
    int mm = minute();
    if (mm != timeLastMinute) { //update the display only if time has changed
      timeLastMinute = mm;
      updateDisplay();
    }
  }

}

void updateDisplay() {

  bool dots = true;
  matrix.drawColon(dots);

  time_t local = timezone.toLocal(now());
  int hh = hour(local);
  int mm = minute(local);

  if (hh < 10) {
    matrix.writeDigitNum(0, 0, dots);
    matrix.writeDigitNum(1, hh, dots);
  } else if (hh < 20) {
    matrix.writeDigitNum(0, 1, dots);
    matrix.writeDigitNum(1, hh - 10, dots);
  } else {
    matrix.writeDigitNum(0, 2, dots);
    matrix.writeDigitNum(1, hh - 20, dots);
  }

  if (mm < 10) {
    matrix.writeDigitNum(3, 0, dots);
    matrix.writeDigitNum(4, mm, dots);
  } else if (mm < 20) {
    matrix.writeDigitNum(3, 1, dots);
    matrix.writeDigitNum(4, mm - 10, dots);
  } else if (mm < 30) {
    matrix.writeDigitNum(3, 2, dots);
    matrix.writeDigitNum(4, mm - 20, dots);
  } else if (mm < 40) {
    matrix.writeDigitNum(3, 3, dots);
    matrix.writeDigitNum(4, mm - 30, dots);
  } else if (mm < 50) {
    matrix.writeDigitNum(3, 4, dots);
    matrix.writeDigitNum(4, mm - 40, dots);
  } else {
    matrix.writeDigitNum(3, 5, dots);
    matrix.writeDigitNum(4, mm - 50, dots);
  }

  matrix.writeDisplay();

}

bool isNumeric(String str) {
  for (char i = 0; i < str.length(); i++) {
    if ( !isDigit(str.charAt(i)) ) {
      return false;
    }
  }
  return true;
}


