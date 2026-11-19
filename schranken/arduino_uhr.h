#pragma once

#include <Arduino.h>

#include "uhr.h"

// Produktive Uhr: millis() des Picos.
class ArduinoUhr : public Uhr {
 public:
  unsigned long jetzt() const override { return millis(); }
};
