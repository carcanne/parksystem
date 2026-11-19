#pragma once

// Logik einer Schranke: fragt bei einem erkannten Fahrzeug die Kasse und öffnet nur bei
// Zustimmung; schliesst nach BARRIER_OPEN_MS selbstständig. Ruft keine Hardware- oder
// Netzwerkfunktionen auf; Schranke, Kasse und Uhr werden über den Konstruktor übergeben.

#include <string>

#include "config.h"
#include "kasse.h"
#include "schranke.h"
#include "uhr.h"

enum class Richtung { Einfahrt, Ausfahrt };

struct Antwort {
  int status;
  std::string json;
};

const char VERBINDUNGSFEHLER[] =
    "{\"ok\":false,\"code\":\"CONNECTION\",\"message\":\"Verbindungsfehler: Die Kasse "
    "antwortet nicht. Die Schranke bleibt geschlossen, bitte Personal rufen.\"}";

class Durchfahrt {
 public:
  Durchfahrt(Richtung richtung, Schranke& schranke, Kasse& kasse, Uhr& uhr)
      : richtung_(richtung), schranke_(schranke), kasse_(kasse), uhr_(uhr) {}

  // Kamera bzw. Weboberfläche hat ein Kennzeichen erkannt.
  Antwort fahrzeugErkannt(const std::string& kennzeichen, const std::string& browserZeit) {
    KassenAntwort a = richtung_ == Richtung::Einfahrt ? kasse_.einfahrt(kennzeichen, browserZeit)
                                                       : kasse_.ausfahrt(kennzeichen, browserZeit);
    if (a.status <= 0) return {503, VERBINDUNGSFEHLER};
    if (a.status == 200 && a.body.find("\"ok\":true") != std::string::npos) oeffnen();
    return {a.status, a.body};
  }

  // Regelmässig aus loop() aufrufen.
  void update() {
    if (offen_ && uhr_.jetzt() - geoeffnetUm_ >= BARRIER_OPEN_MS) schliessen();
  }

  void oeffnen() {
    schranke_.oeffnen();
    offen_ = true;
    geoeffnetUm_ = uhr_.jetzt();
  }

  void schliessen() {
    schranke_.schliessen();
    offen_ = false;
  }

  bool istOffen() const { return offen_; }

 private:
  Richtung richtung_;
  Schranke& schranke_;
  Kasse& kasse_;
  Uhr& uhr_;
  bool offen_ = false;
  unsigned long geoeffnetUm_ = 0;
};
