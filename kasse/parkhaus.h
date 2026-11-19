#pragma once

// Logik der Kasse: Einfahrt, Gebühr, Zahlung, Ausfahrt. Ruft keine Hardware- oder
// Netzwerkfunktionen auf; Speicher, Uhr und Log werden über den Konstruktor übergeben.

#include <ctype.h>
#include <stdlib.h>
#include <time.h>

#include <string>
#include <vector>

#include "fahrzeugspeicher.h"
#include "log.h"
#include "parking_logic.h"
#include "uhr.h"

// Antwort an den Aufrufer: HTTP-Status und JSON {"ok":..,"code":..,"message":..}
struct Ergebnis {
  int status = 0;
  bool ok = false;
  std::string code;
  std::string meldung;
  std::string zusatz;  // weitere JSON-Felder, z.B. "vehicle":{...}

  std::string json() const {
    std::string j = "{\"ok\":";
    j += ok ? "true" : "false";
    j += ",\"code\":\"" + code + "\",\"message\":\"" + meldung + "\"";
    if (!zusatz.empty()) j += "," + zusatz;
    j += "}";
    return j;
  }
};

class Parkhaus {
 public:
  static constexpr long long UHR_GUELTIG_AB = 1700000000LL;

  Parkhaus(Fahrzeugspeicher& speicher, Uhr& uhr, Log& log)
      : speicher_(speicher), uhr_(uhr), log_(log) {}

  bool uhrGestellt() const { return uhr_.jetzt() > UHR_GUELTIG_AB; }

  Ergebnis uhrStellen(const std::string& epoch) {
    if (!stellen(epoch)) return fehler(400, "TIME", "Die übermittelte Uhrzeit ist ungültig.");
    return erfolg("Uhrzeit gestellt auf " + zeitText(uhr_.jetzt()) + ".");
  }

  // browserZeit: Uhrzeit der Schranken-Oberfläche; wird nur übernommen, wenn die
  // eigene Uhr noch nicht gestellt ist (z.B. nach einem Neustart der Kasse).
  Ergebnis einfahrt(const std::string& eingabe, const std::string& browserZeit) {
    std::string kz;
    Ergebnis e;
    if (!kennzeichenLesen(eingabe, kz, e)) return e;

    if (!uhrGestellt()) stellen(browserZeit);
    if (!uhrGestellt()) {
      return fehler(503, "NO_TIME",
                    "Die Uhrzeit der Kasse ist nicht gestellt, eine Einfahrt ist nicht möglich. "
                    "Bitte die Kassen-Seite einmal öffnen.");
    }
    if (speicher_.finden(kz) != nullptr) {
      return fehler(409, "DUPLICATE", "Das Fahrzeug " + kz + " steht bereits im Parkhaus.");
    }
    if (!speicher_.hinzufuegen({kz, uhr_.jetzt(), 0})) {
      return fehler(409, "FULL", "Das Parkhaus ist voll, bitte später wiederkommen.");
    }

    const Fahrzeug& f = *speicher_.finden(kz);
    log_.info("Einfahrt " + kz + " um " + zeitText(f.einfahrt));
    return erfolg("Willkommen! " + kz + " ist um " + zeitText(f.einfahrt) + " eingefahren.",
                  "\"vehicle\":" + fahrzeugJson(f));
  }

  Ergebnis ausfahrt(const std::string& eingabe) {
    std::string kz;
    Ergebnis e;
    Fahrzeug* f = bekanntesFahrzeug(eingabe, kz, e);
    if (f == nullptr) return e;

    if (!bezahlt(*f)) {
      std::string offen = chf(offenRappen(*f));
      std::string grund = f->bezahltRappen > 0
                              ? "Seit der Zahlung von " + chf(f->bezahltRappen) +
                                    " hat eine weitere Stunde begonnen. Die Nachzahlung von " + offen
                              : "Die Parkgebühr von " + offen;
      return fehler(402, "UNPAID",
                    grund + " für " + kz +
                        " ist noch offen. Bitte an der Kasse oder am Automaten bezahlen.",
                    "\"open\":\"" + offen + "\"");
    }

    log_.info("Ausfahrt " + kz);
    speicher_.entfernen(kz);
    return erfolg("Gute Fahrt! " + kz + " hat das Parkhaus verlassen.");
  }

  Ergebnis gebuehr(const std::string& eingabe) {
    std::string kz;
    Ergebnis e;
    Fahrzeug* f = bekanntesFahrzeug(eingabe, kz, e);
    if (f == nullptr) return e;

    std::string msg = bezahlt(*f)
                          ? "Die Gebühr für " + kz + " ist bezahlt (" + chf(f->bezahltRappen) +
                                "), offener Betrag CHF 0.00."
                          : "Offener Betrag für " + kz + ": " + chf(offenRappen(*f)) + ".";
    if (!bezahlt(*f) && f->bezahltRappen > 0) msg += " Bereits bezahlt: " + chf(f->bezahltRappen) + ".";
    return erfolg(msg, "\"vehicle\":" + fahrzeugJson(*f));
  }

  Ergebnis bezahlen(const std::string& eingabe) {
    std::string kz;
    Ergebnis e;
    Fahrzeug* f = bekanntesFahrzeug(eingabe, kz, e);
    if (f == nullptr) return e;

    if (bezahlt(*f)) {
      return fehler(409, "ALREADY_PAID",
                    "Die Gebühr für " + kz + " wurde bereits bezahlt (" + chf(f->bezahltRappen) +
                        "), eine zweite Zahlung ist nicht möglich.",
                    "\"vehicle\":" + fahrzeugJson(*f));
    }

    long betrag = offenRappen(*f);
    f->bezahltRappen += betrag;
    log_.info("Zahlung " + kz + ": " + chf(betrag));
    return erfolg("Zahlung über " + chf(betrag) + " für " + kz + " bestätigt, Status bezahlt.",
                  "\"vehicle\":" + fahrzeugJson(*f));
  }

  // Testfeld der Kassen-Seite: Einfahrtszeit auf "vor <minuten> Minuten" setzen.
  Ergebnis testzeit(const std::string& eingabe, const std::string& minutenEingabe) {
    std::string kz;
    Ergebnis e;
    Fahrzeug* f = bekanntesFahrzeug(eingabe, kz, e);
    if (f == nullptr) return e;

    std::string roh = trim(minutenEingabe);
    bool nurZiffern = !roh.empty() && roh.size() <= 6;
    for (char c : roh) {
      if (!isdigit((unsigned char)c)) nurZiffern = false;
    }
    if (!nurZiffern) {
      return fehler(400, "MINUTES", "Bitte die Minuten als ganze Zahl zwischen 0 und 999999 eingeben.");
    }

    long minuten = atol(roh.c_str());
    f->einfahrt = uhr_.jetzt() - (long long)minuten * 60;
    return erfolg("Einfahrtszeit von " + kz + " auf vor " + std::to_string(minuten) +
                      " Minuten gesetzt, offener Betrag " + chf(offenRappen(*f)) + ".",
                  "\"vehicle\":" + fahrzeugJson(*f));
  }

  std::string fahrzeugliste() const {
    std::string j = "{\"ok\":true,\"now\":\"";
    j += uhrGestellt() ? zeitText(uhr_.jetzt()) : std::string("nicht synchronisiert");
    j += "\",\"vehicles\":[";
    bool erstes = true;
    for (const Fahrzeug& f : speicher_.alle()) {
      if (!erstes) j += ",";
      j += fahrzeugJson(f);
      erstes = false;
    }
    j += "]}";
    return j;
  }

  static std::string zeitText(long long epoch) {
    time_t t = (time_t)epoch;
    struct tm lokal;
    localtime_r(&t, &lokal);
    char buf[24];
    strftime(buf, sizeof(buf), "%d.%m.%Y %H:%M:%S", &lokal);
    return buf;
  }

  static std::string chf(long rappen) {
    char buf[20];
    formatChf(rappen, buf, sizeof(buf));
    return buf;
  }

 private:
  static Ergebnis erfolg(const std::string& meldung, const std::string& zusatz = "") {
    return {200, true, "OK", meldung, zusatz};
  }
  static Ergebnis fehler(int status, const char* code, const std::string& meldung,
                         const std::string& zusatz = "") {
    return {status, false, code, meldung, zusatz};
  }

  static std::string trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    size_t e = s.find_last_not_of(" \t\r\n");
    return b == std::string::npos ? "" : s.substr(b, e - b + 1);
  }

  bool stellen(const std::string& epochText) {
    long long epoch = atoll(epochText.c_str());
    if (epoch < UHR_GUELTIG_AB) return false;
    uhr_.stellen(epoch);
    return true;
  }

  bool kennzeichenLesen(const std::string& eingabe, std::string& kz, Ergebnis& e) const {
    char buf[PLATE_LEN];
    switch (normalizePlate(eingabe.c_str(), buf, sizeof(buf))) {
      case PLATE_OK:
        kz = buf;
        return true;
      case PLATE_EMPTY:
        e = fehler(400, "EMPTY", "Das Kennzeichen fehlt. Bitte ein Kennzeichen eingeben.");
        return false;
      case PLATE_NO_LETTERS:
        e = fehler(400, "NO_LETTERS",
                   "Das Kennzeichen enthält keine Buchstaben. Erwartet werden zwei Buchstaben "
                   "und ein bis sechs Ziffern, z.B. AG 123456.");
        return false;
      default:
        e = fehler(400, "FORMAT",
                   "Das Kennzeichen hat ein ungültiges Format. Erwartet werden zwei Buchstaben "
                   "und ein bis sechs Ziffern, z.B. AG 123456.");
        return false;
    }
  }

  Fahrzeug* bekanntesFahrzeug(const std::string& eingabe, std::string& kz, Ergebnis& e) {
    if (!kennzeichenLesen(eingabe, kz, e)) return nullptr;
    Fahrzeug* f = speicher_.finden(kz);
    if (f == nullptr) {
      e = fehler(404, "UNKNOWN", "Das Fahrzeug " + kz + " ist unbekannt, es steht nicht im Parkhaus.");
    }
    return f;
  }

  long minuten(const Fahrzeug& f) const { return parkedMinutes(f.einfahrt, uhr_.jetzt()); }
  long offenRappen(const Fahrzeug& f) const { return openRappen(minuten(f), f.bezahltRappen); }
  bool bezahlt(const Fahrzeug& f) const { return offenRappen(f) == 0; }

  std::string fahrzeugJson(const Fahrzeug& f) const {
    long min = minuten(f);
    return "{\"plate\":\"" + f.kennzeichen + "\",\"entry\":\"" + zeitText(f.einfahrt) +
           "\",\"minutes\":" + std::to_string(min) + ",\"fee\":\"" + chf(feeRappen(min)) +
           "\",\"paid\":" + (bezahlt(f) ? "true" : "false") + ",\"paidAmount\":\"" +
           chf(f.bezahltRappen) + "\",\"open\":\"" + chf(offenRappen(f)) + "\"}";
  }

  Fahrzeugspeicher& speicher_;
  Uhr& uhr_;
  Log& log_;
};
