#pragma once

#include "Arduino.h"

#define WIFI_STA 1
#define WIFI_AP 2
#define WL_CONNECTED 3

class WiFiClass {
 public:
  void mode(int) {}
  void setHostname(const char*) {}
  void config(IPAddress ip, IPAddress, IPAddress, IPAddress) { ip_ = ip; }
  void begin(const char*, const char*) {}
  int status() { return WL_CONNECTED; }
  IPAddress localIP() { return ip_; }
  void softAPConfig(IPAddress ip, IPAddress, IPAddress) { ip_ = ip; }
  bool softAP(const char*, const char*) { return true; }
  IPAddress softAPIP() { return ip_; }

 private:
  IPAddress ip_;
};
inline WiFiClass WiFi;

class WiFiClient {
 public:
  void setTimeout(unsigned long ms) { timeout_ = ms; }
  unsigned long timeout() const { return timeout_; }
  int available() { return 0; }
  int read() { return -1; }
  void stop() {}

 private:
  unsigned long timeout_ = 1000;
};
