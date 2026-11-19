#pragma once

#include <string>

// Antwort der Kasse. status <= 0 heisst: Kasse nicht erreichbar oder Antwort unbrauchbar.
struct KassenAntwort {
  int status;
  std::string body;  // JSON der Kasse
};

// Kassen-Pico, aus Sicht der Schranken. browserZeit wird mitgeschickt, damit die Kasse
// nach einem Neustart ihre Uhr stellen kann.
class Kasse {
 public:
  virtual ~Kasse() {}
  virtual KassenAntwort einfahrt(const std::string& kennzeichen, const std::string& browserZeit) = 0;
  virtual KassenAntwort ausfahrt(const std::string& kennzeichen, const std::string& browserZeit) = 0;
};
