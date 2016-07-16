#define FW_NAME "mariaclock"
#define FW_VERSION "2.1.8"
#include <Homie.h>
#include <TimeLib.h>   // https://github.com/PaulStoffregen/Time
#include <Timezone.h>  // https://github.com/tauonteilchen/Timezone https://github.com/JChristensen/Timezone

/* Magic sequence for Autodetectable Binary Upload */
const char *__FLAGGED_FW_NAME = "\xbf\x84\xe4\x13\x54" FW_NAME "\x93\x44\x6b\xa7\x75";
const char *__FLAGGED_FW_VERSION = "\x6a\x3f\x3e\x0e\xe1" FW_VERSION "\xb0\x30\x48\xd4\x1a";
/* End of magic sequence for Autodetectable Binary Upload */

const int LUX_PIN = 0;
const int LUX_POLL_INTERVAL_SEC = 1;   // 1 second
const int LUX_SEND_INTERVAL_SEC = 10;  // 10 seconds
time_t luxLastPoll = 0;
time_t luxLastSent = 0;
int luxLastReading = 0;

// Central European Time (Berlin, Madrid, Paris, Rome, Warsaw)
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};  // Central European Summer Time
TimeChangeRule CET = {"CET", Last, Sun, Oct, 3, 60};    // Central European Standard Time
Timezone timezone(CEST, CET);

const int NTP_POLL_INTERVAL_SEC = 300;  // 5 minutes
const int NTP_PACKET_EXPIRED_SEC = 2;  // 2 seconds
time_t ntpLastSent = 0;
time_t ntpLastReceived = 0;

int timeLastMinute = -1; // last recorded minute

// id, type, handler
HomieNode nLux("lux", "lux");
HomieNode nTime("time", "time");

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
  ntpInit();
  matrixInit();
}

void hLoop() {

  int nn = minute();
  if (nn != timeLastMinute) { //update the display only if time has changed
    timeLastMinute = nn;
    time_t local = timezone.toLocal(now());
    int hh = hour(local);
    int mm = minute(local);
    matrixDisplay(hh, mm);
  }

  if (now() - luxLastPoll >= LUX_POLL_INTERVAL_SEC) {

    luxLastPoll = now();

    int luxRaw = analogRead(LUX_PIN);
    // typical readings do not often exceed 512, so limit range to 512 and then map
    int luxReading = map(constrain(luxRaw, 0, 511), 0, 511, 0, 15);

    matrixBrightness(luxReading);

    if (now() - luxLastSent >= LUX_SEND_INTERVAL_SEC) {
      if (luxReading != luxLastReading) {
        luxLastReading = luxReading;
        if (Homie.setNodeProperty(nLux, "value", String(luxLastReading), true)) {
          luxLastSent = now();
        }
        Homie.setNodeProperty(nLux, "raw", String(luxRaw), true);
      }
    }

  }

  if (ntpLastSent == 0 && (now() - ntpLastReceived >= NTP_POLL_INTERVAL_SEC)) {
    ntpSendPacket();
    ntpLastSent = now();
  }

  if (ntpLastSent != 0) {
    time_t t = ntpCheckPacket();
    if (t != 0) {
      ntpLastReceived = now();
      ntpLastSent = 0;
      setTime(t);
      Homie.setNodeProperty(nTime, "value", String(t), true);
    } else if (now() - ntpLastSent >= NTP_PACKET_EXPIRED_SEC) {
      ntpLastSent = 0;
    }
  }

}


