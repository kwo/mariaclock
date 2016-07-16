#define FW_NAME "mariaclock"
#define FW_VERSION "2.1.7"
#include <Homie.h>
#include <TimeLib.h>   // https://github.com/PaulStoffregen/Time
#include <Timezone.h>  // https://github.com/tauonteilchen/Timezone https://github.com/JChristensen/Timezone
#include <Adafruit_LEDBackpack.h>
#include <Adafruit_GFX.h>
#include <WiFiUdp.h>

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
Adafruit_7segment matrix = Adafruit_7segment();

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
  matrix.begin(0x70);
}

void hLoop() {

  int mm = minute();
  if (mm != timeLastMinute) { //update the display only if time has changed
    timeLastMinute = mm;
    updateDisplay();
  }

  if (now() - luxLastPoll >= LUX_POLL_INTERVAL_SEC) {

    luxLastPoll = now();

    int luxRaw = analogRead(LUX_PIN);
    // typical readings do not often exceed 512, so limit range to 512 and then map
    int luxReading = map(constrain(luxRaw, 0, 511), 0, 511, 0, 15);

    matrix.setBrightness(luxReading);

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

/*-------- NTP code ----------*/

WiFiUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets
static const char ntpServerName[] = "raspberrypi";
IPAddress ntpServerIP;

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

void ntpInit() {

  Udp.begin(localPort);
  WiFi.hostByName(ntpServerName, ntpServerIP);

  ntpSendPacket();

  time_t t = 0;
  while (t == 0) {
    t = ntpCheckPacket();
  }
  
  setTime(t);
  ntpLastReceived = now();
  Homie.setNodeProperty(nTime, "value", String(t), true);

}

time_t ntpCheckPacket() {

  int size = Udp.parsePacket();
  if (size >= NTP_PACKET_SIZE) {
    Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
    unsigned long secsSince1900;
    // convert four bytes starting at location 40 to a long integer
    secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
    secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
    secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
    secsSince1900 |= (unsigned long)packetBuffer[43];
    return secsSince1900 - 2208988800UL;
  }

  return 0;

}

// send an NTP request to the time server at the given address
void ntpSendPacket() {

  while (Udp.parsePacket() > 0) ; // discard any previously received packets

  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(ntpServerIP, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();

}

