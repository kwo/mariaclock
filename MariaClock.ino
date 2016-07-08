/*
 * MariaClock
 * v1.0 25.06.2016
 */

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <TimeLib.h>   // https://github.com/PaulStoffregen/Time
#include <Timezone.h>  // https://github.com/tauonteilchen/Timezone https://github.com/JChristensen/Timezone
#include "Adafruit_LEDBackpack.h"
#include "Adafruit_GFX.h"

const char ssid[] = "*****";  // your network SSID (name)
const char pass[] = "*****";  // your network password

static const char ntpServerName[] = "de.pool.ntp.org";

// Central European Time (Berlin, Madrid, Paris, Rome, Warsaw)
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};  // Central European Summer Time
TimeChangeRule CET = {"CET ", Last, Sun, Oct, 3, 60};    // Central European Standard Time
Timezone timezone(CEST, CET);

WiFiUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets

int photocellPin = 0;
int photocellReading;
int ledBrightness = 0;

Adafruit_7segment matrix = Adafruit_7segment();

void setup() {

  Serial.begin(9600);
  delay(250);

  Serial.println("NTPClock");
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");

  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  Serial.println("Starting UDP");
  Udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(Udp.localPort());
  Serial.println("waiting for sync");

  setSyncProvider(getNtpTime);
  setSyncInterval(600);

  matrix.begin(0x70);

}

int lastMinute = -1; // last recorded minute

void loop() {

  photocellReading = analogRead(photocellPin); 
  ledBrightness = map(photocellReading, 0, 1023, 0, 15);
  matrix.setBrightness(ledBrightness);
  Serial.print("brightness ");
  Serial.print(photocellReading);     // the raw analog reading
  Serial.print(" ");
  Serial.println(ledBrightness);
  
  if (timeStatus() != timeNotSet) {
    int mm = minute();
    if (mm != lastMinute) { //update the display only if time has changed
      lastMinute = mm;
      updateDisplay();
      delay(100);
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

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime() {

  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL;
    }
  }

  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time

}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address) {
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
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
