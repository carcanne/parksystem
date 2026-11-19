#pragma once

// Uhr der Kasse (Unix-Zeit in Sekunden). Der Kassen-Pico hat keine batteriegepufferte
// Uhr und kein NTP, darum wird sie von aussen gestellt (Browser-Zeit).
class Uhr {
 public:
  virtual ~Uhr() {}
  virtual long long jetzt() const = 0;
  virtual void stellen(long long epoch) = 0;
};
