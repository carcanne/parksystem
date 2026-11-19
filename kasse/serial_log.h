#pragma once

#include <Arduino.h>

#include "log.h"

// Produktives Log: Ausgabe auf der seriellen Schnittstelle.
class SerialLog : public Log {
 public:
  void info(const std::string& text) override { Serial.printf("%s\n", text.c_str()); }
};
