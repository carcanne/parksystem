#pragma once

#include "config.h"
#include "fahrzeugspeicher.h"

// Produktive Speicherung: festes Array mit MAX_VEHICLES Plätzen im Arbeitsspeicher.
class ArraySpeicher : public Fahrzeugspeicher {
 public:
  Fahrzeug* finden(const std::string& kennzeichen) override {
    for (Platz& p : plaetze_) {
      if (p.belegt && p.fahrzeug.kennzeichen == kennzeichen) return &p.fahrzeug;
    }
    return nullptr;
  }

  bool hinzufuegen(const Fahrzeug& fahrzeug) override {
    for (Platz& p : plaetze_) {
      if (!p.belegt) {
        p.belegt = true;
        p.fahrzeug = fahrzeug;
        return true;
      }
    }
    return false;
  }

  void entfernen(const std::string& kennzeichen) override {
    for (Platz& p : plaetze_) {
      if (p.belegt && p.fahrzeug.kennzeichen == kennzeichen) p.belegt = false;
    }
  }

  std::vector<Fahrzeug> alle() const override {
    std::vector<Fahrzeug> liste;
    for (const Platz& p : plaetze_) {
      if (p.belegt) liste.push_back(p.fahrzeug);
    }
    return liste;
  }

  // Stromausfall: Inhalt des Arbeitsspeichers geht verloren (nur für den Systemtest).
  void leeren() {
    for (Platz& p : plaetze_) p.belegt = false;
  }

 private:
  struct Platz {
    bool belegt = false;
    Fahrzeug fahrzeug;
  };
  Platz plaetze_[MAX_VEHICLES];
};
