#define FW_NAME "mariaclock"
#define FW_VERSION "3.0.2"
#include <ESP8266WiFi.h>
#include <TimeLib.h>   // https://github.com/PaulStoffregen/Time
#include <Timezone.h>  // https://github.com/tauonteilchen/Timezone https://github.com/JChristensen/Timezone

const char ssid[] = "****";  // your network SSID (name)
const char pass[] = "****";  // your network password

const int LUX_PIN = 0;
const int LUX_POLL_INTERVAL_SEC = 1;   // 1 second
time_t luxLastPoll = 0;
int luxRaw = 0;
int luxReading = 0;

// Central European Time (Berlin, Madrid, Paris, Rome, Warsaw)
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};  // Central European Summer Time
TimeChangeRule CET = {"CET", Last, Sun, Oct, 3, 60};    // Central European Standard Time
Timezone timezone(CEST, CET);

const int NTP_POLL_INTERVAL_SEC = 300;  // 5 minutes
const int NTP_PACKET_TIMEOUT_SEC = 2;  // 2 seconds
time_t ntpLastSent = 0;
time_t ntpNextPoll = 0;

int timeLastMinute = -1; // last recorded minute

void setup() {

  Serial.begin(9600);
  delay(250);

  Serial.println("");
  Serial.print(FW_NAME);
  Serial.print(" ");
  Serial.println(FW_VERSION);

  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");

  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  ntpInit();
  matrixInit();
}

void loop() {

  int nn = minute();
  if (nn != timeLastMinute) { //update the display only if time has changed
    timeLastMinute = nn;
    time_t local = timezone.toLocal(now());
    matrixDisplay(hour(local), minute(local));
  }

  if (now() - luxLastPoll >= LUX_POLL_INTERVAL_SEC) {

    luxLastPoll = now();

    luxRaw = analogRead(LUX_PIN);
    // typical readings do not often exceed 512, so limit range to 512 and then map
    luxReading = map(constrain(luxRaw, 0, 511), 0, 511, 0, 15);

    matrixBrightness(luxReading);

  }

  if (ntpLastSent == 0 && now() > ntpNextPoll) {
    ntpSendPacket();
    ntpLastSent = now();
    Serial.println("NTP send packet.");
  }

  if (ntpLastSent != 0) {
    time_t t = ntpCheckPacket();
    if (t != 0) {
      Serial.println("NTP got packet.");
      ntpNextPoll = now() + NTP_POLL_INTERVAL_SEC;
      ntpLastSent = 0;
      setClockTime(t);
    } else if (now() - ntpLastSent >= NTP_PACKET_TIMEOUT_SEC) {
      Serial.println("NTP packet expired.");
      ntpNextPoll = now() + 60; // wait a minute
      ntpLastSent = 0;
    }
  }

}

void setClockTime(time_t t) {
  setTime(t);
  Serial.print("NTP set time: ");
  Serial.println(formatTime(t));
}

String formatTime(time_t t) {
  return padDigit(hour(t)) + ":" + padDigit(minute(t)) + ":" + padDigit(second(t));
}

String padDigit(int i) {
  if (i < 10) {
    return "0" + String(i);
  }
  return String(i);
}

