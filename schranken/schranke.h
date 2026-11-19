#pragma once

// Eine Schranke (Ein- oder Ausfahrt).
class Schranke {
 public:
  virtual ~Schranke() {}
  virtual void oeffnen() = 0;
  virtual void schliessen() = 0;
};
