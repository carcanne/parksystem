#pragma once

// Test Doubles für die Abhängigkeiten des Schranken-Picos.

#include <string>

#include "../../schranken/kasse.h"
#include "../../schranken/schranke.h"
#include "../../schranken/uhr.h"

// Mock: ersetzt den Servo. Merkt sich, wie oft und in welcher Reihenfolge die Logik die
// Schranke bewegt hat, damit der Test diese Aufrufe prüfen kann.
class SchrankeMock : public Schranke {
 public:
  void oeffnen() override {
    oeffnenAufrufe++;
    protokoll += "auf ";
  }
  void schliessen() override {
    schliessenAufrufe++;
    protokoll += "zu ";
  }

  int oeffnenAufrufe = 0;
  int schliessenAufrufe = 0;
  std::string protokoll;
};

// Stub: ersetzt die HTTP-Anfrage an den Kassen-Pico. Liefert immer dieselbe, im Test
// festgelegte Antwort und ignoriert Kennzeichen und Uhrzeit.
class KasseStub : public Kasse {
 public:
  KasseStub(int status, const std::string& body) : antwort_{status, body} {}

  KassenAntwort einfahrt(const std::string&, const std::string&) override { return antwort_; }
  KassenAntwort ausfahrt(const std::string&, const std::string&) override { return antwort_; }

 private:
  KassenAntwort antwort_;
};

// Fake: ersetzt millis(). Eine einfache, aber funktionierende Uhr, die der Test von Hand
// vorstellt, statt echte Sekunden zu warten.
class FakeUhr : public Uhr {
 public:
  unsigned long jetzt() const override { return ms_; }
  void vorstellen(unsigned long ms) { ms_ += ms; }

 private:
  unsigned long ms_ = 100000;
};
