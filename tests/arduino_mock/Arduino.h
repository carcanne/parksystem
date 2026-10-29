#pragma once

// Minimaler Ersatz für die Arduino-API, damit die Firmware beider Picos
// unverändert auf dem Notebook kompiliert und im Systemtest ausgeführt werden kann.

#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include <functional>
#include <map>
#include <string>

typedef unsigned int uint;

class String {
 public:
  String() {}
  String(const char* c) : s_(c ? c : "") {}
  String(const std::string& s) : s_(s) {}
  String(char c) : s_(1, c) {}
  String(int v) : s_(std::to_string(v)) {}
  String(long v) : s_(std::to_string(v)) {}
  String(unsigned int v) : s_(std::to_string(v)) {}
  String(unsigned long v) : s_(std::to_string(v)) {}

  const char* c_str() const { return s_.c_str(); }
  const std::string& str() const { return s_; }
  unsigned int length() const { return (unsigned int)s_.size(); }
  char operator[](unsigned int i) const { return i < s_.size() ? s_[i] : '\0'; }

  String& operator+=(const String& o) { s_ += o.s_; return *this; }
  String& operator+=(const char* o) { s_ += o ? o : ""; return *this; }
  String& operator+=(char c) { s_ += c; return *this; }
  String& operator+=(int v) { s_ += std::to_string(v); return *this; }
  String& operator+=(long v) { s_ += std::to_string(v); return *this; }

  int indexOf(const String& o) const {
    size_t p = s_.find(o.s_);
    return p == std::string::npos ? -1 : (int)p;
  }
  bool startsWith(const String& o) const { return s_.compare(0, o.s_.size(), o.s_) == 0; }
  void trim() {
    size_t b = s_.find_first_not_of(" \t\r\n");
    size_t e = s_.find_last_not_of(" \t\r\n");
    s_ = b == std::string::npos ? "" : s_.substr(b, e - b + 1);
  }
  long toInt() const { return atol(s_.c_str()); }
  bool operator==(const String& o) const { return s_ == o.s_; }

 private:
  std::string s_;
};

inline String operator+(const String& a, const String& b) { String r(a); r += b; return r; }
inline String operator+(const String& a, const char* b) { String r(a); r += b; return r; }
inline String operator+(const char* a, const String& b) { String r(a); r += b; return r; }
inline String operator+(const String& a, char b) { String r(a); r += b; return r; }
inline String operator+(const String& a, int b) { String r(a); r += b; return r; }
inline String operator+(const String& a, long b) { String r(a); r += b; return r; }

// Simulierte Zeit: millis() läuft für beide Picos gemeinsam, die Uhr der Kasse
// ist nach dem Start nicht gestellt (Epoch nahe 0), bis settimeofday() aufgerufen wird.
namespace sim {
inline unsigned long long& ms() { static unsigned long long v = 0; return v; }
inline long long& clockOffset() { static long long v = 0; return v; }
inline bool& verbose() { static bool v = false; return v; }
}  // namespace sim

inline unsigned long millis() { return (unsigned long)sim::ms(); }
inline void delay(unsigned long d) { sim::ms() += d; }

inline time_t mock_time(time_t* out) {
  time_t t = (time_t)(sim::ms() / 1000 + sim::clockOffset());
  if (out) *out = t;
  return t;
}
inline int mock_settimeofday(const struct timeval* tv, const void*) {
  sim::clockOffset() = (long long)tv->tv_sec - (long long)(sim::ms() / 1000);
  return 0;
}
#define time(x) mock_time(x)
#define settimeofday mock_settimeofday

#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define LED_BUILTIN 25

namespace sim {
inline std::map<int, int>& pins() { static std::map<int, int> p; return p; }
inline std::map<int, unsigned>& pwm() { static std::map<int, unsigned> p; return p; }
}  // namespace sim

inline void pinMode(int, int) {}
inline void digitalWrite(int pin, int v) { sim::pins()[pin] = v; }
inline bool isDigit(char c) { return isdigit((unsigned char)c) != 0; }
inline long map(long x, long inMin, long inMax, long outMin, long outMax) {
  return (x - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
}

class IPAddress {
 public:
  IPAddress() {}
  IPAddress(int a, int b, int c, int d) { snprintf(text_, sizeof(text_), "%d.%d.%d.%d", a, b, c, d); }
  const char* c_str() const { return text_; }

 private:
  char text_[16] = "0.0.0.0";
};

class SerialPort {
 public:
  void begin(long) {}
  void printf(const char* fmt, ...) {
    if (!sim::verbose()) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
  }
  void print(const char* s) { if (sim::verbose()) fputs(s, stderr); }
  void print(const String& s) { print(s.c_str()); }
  void print(const IPAddress& ip) { print(ip.c_str()); }
  template <typename T>
  void println(const T& v) { print(v); print("\n"); }
  void println() { print("\n"); }
  operator bool() const { return true; }
};
inline SerialPort Serial;
