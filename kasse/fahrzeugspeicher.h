#pragma once

#include <string>
#include <vector>

struct Fahrzeug {
  std::string kennzeichen;
  long long einfahrt;   // Unix-Zeit
  long bezahltRappen;   // bisher bezahlter Betrag
};

// Speicherung der eingefahrenen Fahrzeuge.
class Fahrzeugspeicher {
 public:
  virtual ~Fahrzeugspeicher() {}
  // nullptr, wenn das Fahrzeug nicht im Parkhaus steht
  virtual Fahrzeug* finden(const std::string& kennzeichen) = 0;
  // false, wenn kein Platz mehr frei ist
  virtual bool hinzufuegen(const Fahrzeug& fahrzeug) = 0;
  virtual void entfernen(const std::string& kennzeichen) = 0;
  virtual std::vector<Fahrzeug> alle() const = 0;
};
