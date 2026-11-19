#pragma once

// Laufzeit des Schranken-Picos in Millisekunden (auf dem Pico: millis()).
class Uhr {
 public:
  virtual ~Uhr() {}
  virtual unsigned long jetzt() const = 0;
};
