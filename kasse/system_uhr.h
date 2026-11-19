#pragma once

#include <sys/time.h>
#include <time.h>

#include "uhr.h"

// Produktive Uhr: Systemzeit des Picos.
class SystemUhr : public Uhr {
 public:
  long long jetzt() const override { return (long long)time(nullptr); }

  void stellen(long long epoch) override {
    struct timeval tv = {(time_t)epoch, 0};
    settimeofday(&tv, nullptr);
  }
};
