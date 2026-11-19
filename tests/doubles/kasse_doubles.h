#pragma once

// Test Doubles für die Abhängigkeiten des Kassen-Picos.

#include <string>
#include <vector>

#include "../../kasse/fahrzeugspeicher.h"
#include "../../kasse/log.h"
#include "../../kasse/uhr.h"

// Fake: ersetzt die Systemuhr. Startet wie der Pico ungestellt (0) und lässt sich im Test
// beliebig vorstellen, z.B. um 61 Minuten Parkzeit ohne Warten.
class FakeUhr : public Uhr {
 public:
  long long jetzt() const override { return epoch_; }
  void stellen(long long epoch) override { epoch_ = epoch; }
  void vorstellen(long long sekunden) { epoch_ += sekunden; }

 private:
  long long epoch_ = 0;
};

// Fake: vereinfachte Speicherung in einem std::vector mit frei wählbarer Kapazität, damit
// "Parkhaus voll" mit wenigen Fahrzeugen getestet werden kann. Der Inhalt ist im Test
// direkt einsehbar.
class FakeSpeicher : public Fahrzeugspeicher {
 public:
  explicit FakeSpeicher(size_t kapazitaet = 50) : kapazitaet_(kapazitaet) {}

  Fahrzeug* finden(const std::string& kennzeichen) override {
    for (Fahrzeug& f : fahrzeuge) {
      if (f.kennzeichen == kennzeichen) return &f;
    }
    return nullptr;
  }
  bool hinzufuegen(const Fahrzeug& f) override {
    if (fahrzeuge.size() >= kapazitaet_) return false;
    fahrzeuge.push_back(f);
    return true;
  }
  void entfernen(const std::string& kennzeichen) override {
    for (size_t i = 0; i < fahrzeuge.size(); i++) {
      if (fahrzeuge[i].kennzeichen == kennzeichen) {
        fahrzeuge.erase(fahrzeuge.begin() + i);
        return;
      }
    }
  }
  std::vector<Fahrzeug> alle() const override { return fahrzeuge; }

  std::vector<Fahrzeug> fahrzeuge;

 private:
  size_t kapazitaet_;
};

// Dummy: ersetzt die serielle Ausgabe. Wird nur übergeben, weil der Konstruktor ein Log
// verlangt; für keinen Testfall relevant.
class DummyLog : public Log {
 public:
  void info(const std::string&) override {}
};

// Spy: umschliesst eine echte (produktive) Speicherung und zeichnet zusätzlich jeden
// Aufruf auf. Der Test bleibt damit ein Integrationstest mit Parkhaus + ArraySpeicher.
class SpeicherSpy : public Fahrzeugspeicher {
 public:
  explicit SpeicherSpy(Fahrzeugspeicher& echt) : echt_(echt) {}

  Fahrzeug* finden(const std::string& kennzeichen) override { return echt_.finden(kennzeichen); }
  bool hinzufuegen(const Fahrzeug& f) override {
    hinzugefuegt.push_back(f.kennzeichen);
    return echt_.hinzufuegen(f);
  }
  void entfernen(const std::string& kennzeichen) override {
    entfernt.push_back(kennzeichen);
    echt_.entfernen(kennzeichen);
  }
  std::vector<Fahrzeug> alle() const override { return echt_.alle(); }

  std::vector<std::string> hinzugefuegt;
  std::vector<std::string> entfernt;

 private:
  Fahrzeugspeicher& echt_;
};
