#pragma once

#include <string>

// Protokollausgabe der Kasse (auf dem Pico: serielle Schnittstelle).
class Log {
 public:
  virtual ~Log() {}
  virtual void info(const std::string& text) = 0;
};
