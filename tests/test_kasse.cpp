// Unit-Tests der Kassen-Logik (kasse/parkhaus.h) ohne Hardware.
// Systemuhr -> FakeUhr, Fahrzeugspeicher -> FakeSpeicher, serielle Ausgabe -> DummyLog.
// Am Schluss ein Integrationstest mit der echten Speicherung (ArraySpeicher) hinter einem Spy.
//
//   make unit   (oder: c++ -std=c++17 -Wall -o build/test_kasse tests/test_kasse.cpp)

#include <stdlib.h>
#include <time.h>

#include "../kasse/array_speicher.h"
#include "../kasse/parkhaus.h"
#include "doubles/kasse_doubles.h"
#include "testing.h"

using testing::enthaelt;
using testing::feld;
using testing::testfall;

static const long long START = 1794474000LL;  // 12.11.2026 10:00:00 (CET)
static const std::string START_TEXT = std::to_string(START);
static const long long MINUTE = 60;

// Testumgebung: Kasse mit gestellter Uhr und leerem Parkhaus.
struct Testkasse {
  FakeUhr uhr;
  FakeSpeicher speicher;
  DummyLog log;
  Parkhaus parkhaus{speicher, uhr, log};

  explicit Testkasse(size_t kapazitaet = 50) : speicher(kapazitaet) { uhr.stellen(START); }

  Ergebnis einfahrt(const std::string& kz) { return parkhaus.einfahrt(kz, ""); }
  std::string offen(const std::string& kz) {
    return feld(parkhaus.gebuehr(kz).json(), "open");
  }
};

int main() {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();

  // ---------- Einfahrt ----------

  testfall("TF-01 AG 123456 einfahren: registriert mit Kennzeichen und Einfahrtszeit", [] {
    Testkasse k;
    Ergebnis e = k.einfahrt("AG 123456");
    CHECK_EQ(e.status, 200);
    CHECK(e.ok);
    CHECK_EQ(e.meldung, std::string("Willkommen! AG 123456 ist um 12.11.2026 10:00:00 eingefahren."));
    CHECK_EQ(k.speicher.fahrzeuge.size(), (size_t)1);
    CHECK(enthaelt(k.parkhaus.fahrzeugliste(), "\"plate\":\"AG 123456\""));
  });

  testfall("TF-03 Einfahrtszeit entspricht der Uhr der Kasse (Abweichung 0 s)", [] {
    Testkasse k;
    k.uhr.vorstellen(42);
    k.einfahrt("AG 123456");
    CHECK_EQ(k.speicher.finden("AG 123456")->einfahrt, START + 42);
  });

  testfall("TF-04 LU 4711 einfahren: beide Fahrzeuge mit eigener Einfahrtszeit", [] {
    Testkasse k;
    k.einfahrt("AG 123456");
    k.uhr.vorstellen(20);
    CHECK_EQ(k.einfahrt("LU 4711").status, 200);
    CHECK_EQ(k.speicher.fahrzeuge.size(), (size_t)2);
    CHECK_EQ(k.speicher.finden("AG 123456")->einfahrt, START);
    CHECK_EQ(k.speicher.finden("LU 4711")->einfahrt, START + 20);
  });

  struct Ungueltig {
    const char* tf;
    const char* eingabe;
    const char* code;
    const char* meldungsteil;
  };
  static const Ungueltig ungueltige[] = {
      {"TF-05 123456: Meldung «keine Buchstaben»", "123456", "NO_LETTERS", "keine Buchstaben"},
      {"TF-06 Leere Eingabe: Meldung «Kennzeichen fehlt»", "", "EMPTY", "Kennzeichen fehlt"},
      {"TF-07 AG 1234567: Meldung «ungültiges Format»", "AG 1234567", "FORMAT", "ungültiges Format"},
      {"TF-08 A 123456: Meldung «ungültiges Format»", "A 123456", "FORMAT", "ungültiges Format"},
  };
  for (const Ungueltig& u : ungueltige) {
    testfall(u.tf, [&u] {
      Testkasse k;
      Ergebnis e = k.einfahrt(u.eingabe);
      CHECK_EQ(e.status, 400);
      CHECK(!e.ok);
      CHECK_EQ(e.code, std::string(u.code));
      CHECK(enthaelt(e.meldung, u.meldungsteil));
      CHECK(k.speicher.fahrzeuge.empty());
    });
  }

  testfall("TF-09 ZG 7 (unterer Grenzwert) einfahren: registriert", [] {
    Testkasse k;
    CHECK_EQ(k.einfahrt("ZG 7").status, 200);
    CHECK(k.speicher.finden("ZG 7") != nullptr);
  });

  testfall("TF-10 «  ag   123456 » wird als AG 123456 erkannt: «bereits im Parkhaus»", [] {
    Testkasse k;
    k.einfahrt("AG 123456");
    Ergebnis e = k.einfahrt("  ag   123456 ");
    CHECK_EQ(e.status, 409);
    CHECK_EQ(e.code, std::string("DUPLICATE"));
    CHECK_EQ(e.meldung, std::string("Das Fahrzeug AG 123456 steht bereits im Parkhaus."));
    CHECK_EQ(k.speicher.fahrzeuge.size(), (size_t)1);
  });

  testfall("Ergänzung: Parkhaus voll: Einfahrt abgewiesen", [] {
    Testkasse k(2);
    k.einfahrt("AG 1");
    k.einfahrt("AG 2");
    Ergebnis e = k.einfahrt("AG 3");
    CHECK_EQ(e.status, 409);
    CHECK_EQ(e.code, std::string("FULL"));
    CHECK(k.speicher.finden("AG 3") == nullptr);
  });

  // ---------- Gebühr und Zahlung ----------

  struct GebuehrFall {
    const char* tf;
    const char* kz;
    long minuten;
    const char* soll;
  };
  static const GebuehrFall gebuehren[] = {
      {"TF-12 40 Min.: CHF 2.00", "AG 123456", 40, "CHF 2.00"},
      {"TF-13 60 Min.: CHF 2.00", "AG 123456", 60, "CHF 2.00"},
      {"TF-14 61 Min.: CHF 4.00", "AG 123456", 61, "CHF 4.00"},
      {"TF-15 121 Min.: CHF 6.00", "AG 123456", 121, "CHF 6.00"},
      {"TF-16 ZG 7, 0 Min.: CHF 2.00", "ZG 7", 0, "CHF 2.00"},
  };
  for (const GebuehrFall& g : gebuehren) {
    testfall(g.tf, [&g] {
      Testkasse k;
      k.einfahrt(g.kz);
      k.uhr.vorstellen(g.minuten * MINUTE);  // statt Testfeld und Warten: Fake-Uhr vorstellen
      Ergebnis e = k.parkhaus.gebuehr(g.kz);
      CHECK_EQ(e.status, 200);
      CHECK_EQ(feld(e.json(), "minutes"), std::to_string(g.minuten));
      CHECK_EQ(feld(e.json(), "open"), std::string(g.soll));
    });
  }

  testfall("TF-12..16 über das Testfeld der Kassen-Seite (Einfahrtszeit zurücksetzen)", [] {
    Testkasse k;
    k.einfahrt("AG 123456");
    CHECK_EQ(feld(k.parkhaus.testzeit("AG 123456", "61").json(), "open"), std::string("CHF 4.00"));
    CHECK_EQ(k.offen("AG 123456"), std::string("CHF 4.00"));
    CHECK_EQ(k.parkhaus.testzeit("AG 123456", "abc").code, std::string("MINUTES"));
    CHECK_EQ(k.parkhaus.testzeit("AG 123456", "1234567").code, std::string("MINUTES"));
    CHECK_EQ(k.parkhaus.testzeit("SG 55555", "10").code, std::string("UNKNOWN"));
  });

  testfall("TF-17 SG 55555 an der Kasse: «unbekannt», kein Betrag, keine Zahlung", [] {
    Testkasse k;
    Ergebnis e = k.parkhaus.gebuehr("SG 55555");
    CHECK_EQ(e.status, 404);
    CHECK_EQ(e.code, std::string("UNKNOWN"));
    CHECK(!enthaelt(e.json(), "\"vehicle\""));
    CHECK_EQ(k.parkhaus.bezahlen("SG 55555").status, 404);
  });

  testfall("TF-18 AG 123456 nach 121 Min. bezahlen: Bestätigung über CHF 6.00, bezahlt", [] {
    Testkasse k;
    k.einfahrt("AG 123456");
    k.uhr.vorstellen(121 * MINUTE);
    Ergebnis e = k.parkhaus.bezahlen("AG 123456");
    CHECK_EQ(e.status, 200);
    CHECK_EQ(e.meldung, std::string("Zahlung über CHF 6.00 für AG 123456 bestätigt, Status bezahlt."));
    CHECK_EQ(feld(e.json(), "paid"), std::string("true"));
    CHECK_EQ(k.speicher.finden("AG 123456")->bezahltRappen, 600L);
  });

  testfall("TF-19/20 Erneute Abfrage: bezahlt, offen CHF 0.00, keine zweite Zahlung", [] {
    Testkasse k;
    k.einfahrt("AG 123456");
    k.uhr.vorstellen(121 * MINUTE);
    k.parkhaus.bezahlen("AG 123456");
    k.uhr.vorstellen(10);
    Ergebnis f = k.parkhaus.gebuehr("AG 123456");
    CHECK_EQ(feld(f.json(), "paid"), std::string("true"));
    CHECK_EQ(feld(f.json(), "open"), std::string("CHF 0.00"));
    Ergebnis zweite = k.parkhaus.bezahlen("AG 123456");
    CHECK_EQ(zweite.status, 409);
    CHECK_EQ(zweite.code, std::string("ALREADY_PAID"));
    CHECK_EQ(k.speicher.finden("AG 123456")->bezahltRappen, 600L);
  });

  testfall("TF-21 LU 4711 am Automaten bezahlen: Kasse zeigt bezahlt", [] {
    Testkasse k;
    k.einfahrt("LU 4711");
    CHECK_EQ(k.parkhaus.bezahlen("LU 4711").status, 200);  // Automat und Kasse: dieselbe Logik
    CHECK_EQ(feld(k.parkhaus.gebuehr("LU 4711").json(), "paid"), std::string("true"));
  });

  // ---------- Ausfahrt ----------

  testfall("TF-22 AG 123456 bezahlt ausfahren: erlaubt, verschwindet aus der Liste", [] {
    Testkasse k;
    k.einfahrt("AG 123456");
    k.parkhaus.bezahlen("AG 123456");
    Ergebnis e = k.parkhaus.ausfahrt("AG 123456");
    CHECK_EQ(e.status, 200);
    CHECK(e.ok);
    CHECK_EQ(e.meldung, std::string("Gute Fahrt! AG 123456 hat das Parkhaus verlassen."));
    CHECK(k.speicher.finden("AG 123456") == nullptr);
  });

  testfall("TF-24 AG 123456 zweites Mal ausfahren: «unbekannt»", [] {
    Testkasse k;
    k.einfahrt("AG 123456");
    k.parkhaus.bezahlen("AG 123456");
    k.parkhaus.ausfahrt("AG 123456");
    Ergebnis e = k.parkhaus.ausfahrt("AG 123456");
    CHECK_EQ(e.status, 404);
    CHECK_EQ(e.code, std::string("UNKNOWN"));
  });

  testfall("TF-25 AG 123456 erneut einfahren: neuer Aufenthalt, offen CHF 2.00", [] {
    Testkasse k;
    k.einfahrt("AG 123456");
    k.uhr.vorstellen(121 * MINUTE);
    k.parkhaus.bezahlen("AG 123456");
    k.parkhaus.ausfahrt("AG 123456");
    k.uhr.vorstellen(3);
    CHECK_EQ(k.einfahrt("AG 123456").status, 200);
    Ergebnis f = k.parkhaus.gebuehr("AG 123456");
    CHECK_EQ(feld(f.json(), "open"), std::string("CHF 2.00"));
    CHECK_EQ(feld(f.json(), "paid"), std::string("false"));
  });

  testfall("TF-26 ZH 999999 ohne Zahlung ausfahren: abgewiesen, Meldung nennt CHF 2.00", [] {
    Testkasse k;
    k.einfahrt("ZH 999999");
    k.uhr.vorstellen(6);
    Ergebnis e = k.parkhaus.ausfahrt("ZH 999999");
    CHECK_EQ(e.status, 402);
    CHECK_EQ(e.code, std::string("UNPAID"));
    CHECK_EQ(e.meldung, std::string("Die Parkgebühr von CHF 2.00 für ZH 999999 ist noch offen. "
                                    "Bitte an der Kasse oder am Automaten bezahlen."));
    CHECK(k.speicher.finden("ZH 999999") != nullptr);
  });

  testfall("TF-27 SG 55555 ausfahren: «unbekannt»", [] {
    Testkasse k;
    Ergebnis e = k.parkhaus.ausfahrt("SG 55555");
    CHECK_EQ(e.status, 404);
    CHECK_EQ(e.meldung, std::string("Das Fahrzeug SG 55555 ist unbekannt, es steht nicht im Parkhaus."));
  });

  testfall("TF-31 Zahlung bei 0 Min., nach 61 Min. Nachzahlung CHF 2.00, danach Ausfahrt", [] {
    Testkasse k;
    k.einfahrt("ZG 7");
    CHECK_EQ(k.parkhaus.bezahlen("ZG 7").status, 200);
    k.uhr.vorstellen(61 * MINUTE);
    CHECK_EQ(k.offen("ZG 7"), std::string("CHF 2.00"));

    Ergebnis x1 = k.parkhaus.ausfahrt("ZG 7");
    CHECK_EQ(x1.status, 402);
    CHECK(enthaelt(x1.meldung, "Nachzahlung von CHF 2.00"));

    Ergebnis p2 = k.parkhaus.bezahlen("ZG 7");
    CHECK_EQ(p2.meldung, std::string("Zahlung über CHF 2.00 für ZG 7 bestätigt, Status bezahlt."));
    CHECK_EQ(feld(p2.json(), "paidAmount"), std::string("CHF 4.00"));
    CHECK_EQ(k.parkhaus.ausfahrt("ZG 7").status, 200);
  });

  // ---------- Uhr und Neustart der Kasse ----------

  testfall("TF-30 Neustart der Kasse: Daten verloren, LU 4711 unbekannt", [] {
    Testkasse vorher;
    vorher.einfahrt("LU 4711");
    vorher.parkhaus.bezahlen("LU 4711");

    Testkasse nachher;  // neuer Pico-Start: leerer Speicher
    CHECK_EQ(nachher.parkhaus.gebuehr("LU 4711").code, std::string("UNKNOWN"));
    CHECK(enthaelt(nachher.parkhaus.fahrzeugliste(), "\"vehicles\":[]"));
  });

  testfall("TF-32 Uhr nach Neustart nicht gestellt: Einfahrt stellt sie mit der Browser-Zeit", [] {
    FakeUhr uhr;  // 0 = nicht gestellt
    FakeSpeicher speicher;
    DummyLog log;
    Parkhaus p(speicher, uhr, log);
    CHECK(!p.uhrGestellt());

    Ergebnis e = p.einfahrt("AG 123456", START_TEXT);
    CHECK_EQ(e.status, 200);
    CHECK(p.uhrGestellt());
    CHECK_EQ(speicher.finden("AG 123456")->einfahrt, START);
  });

  testfall("TF-32 Ohne gestellte Uhr und ohne Browser-Zeit: Einfahrt abgewiesen, kein Eintrag", [] {
    FakeUhr uhr;
    FakeSpeicher speicher;
    DummyLog log;
    Parkhaus p(speicher, uhr, log);
    Ergebnis e = p.einfahrt("AG 123456", "");
    CHECK_EQ(e.status, 503);
    CHECK_EQ(e.code, std::string("NO_TIME"));
    CHECK(speicher.fahrzeuge.empty());
    CHECK(enthaelt(p.fahrzeugliste(), "nicht synchronisiert"));
  });

  testfall("K-2 Laufende Uhr wird durch die Browser-Zeit nicht verstellt", [] {
    Testkasse k;
    k.parkhaus.einfahrt("AG 123456", std::to_string(START + 3600));
    CHECK_EQ(k.uhr.jetzt(), START);
  });

  testfall("Kassen-Seite stellt die Uhr, ungültige Zeit wird abgewiesen", [] {
    Testkasse k;
    CHECK_EQ(k.parkhaus.uhrStellen("123").code, std::string("TIME"));
    CHECK_EQ(k.uhr.jetzt(), START);
    CHECK_EQ(k.parkhaus.uhrStellen(std::to_string(START + 60)).status, 200);
    CHECK_EQ(k.uhr.jetzt(), START + 60);
  });

  // ---------- Integration mit echter Speicherung (Spy) ----------

  testfall("Spy: Abgewiesene Einfahrten (TF-05..08, TF-10) schreiben nichts in den Speicher", [] {
    ArraySpeicher echt;
    SpeicherSpy spy(echt);
    FakeUhr uhr;
    uhr.stellen(START);
    DummyLog log;
    Parkhaus p(spy, uhr, log);

    p.einfahrt("AG 123456", "");
    for (const char* kz : {"123456", "", "AG 1234567", "A 123456", "  ag   123456 "}) p.einfahrt(kz, "");

    CHECK_EQ(spy.hinzugefuegt.size(), (size_t)1);
    CHECK_EQ(echt.alle().size(), (size_t)1);
  });

  testfall("Spy: Ausfahrt entfernt genau einmal, unbezahlte Ausfahrt (TF-26) gar nicht", [] {
    ArraySpeicher echt;
    SpeicherSpy spy(echt);
    FakeUhr uhr;
    uhr.stellen(START);
    DummyLog log;
    Parkhaus p(spy, uhr, log);

    p.einfahrt("ZH 999999", "");
    p.ausfahrt("ZH 999999");
    CHECK(spy.entfernt.empty());

    p.bezahlen("ZH 999999");
    p.ausfahrt("ZH 999999");
    p.ausfahrt("ZH 999999");
    CHECK_EQ(spy.entfernt.size(), (size_t)1);
    CHECK(echt.finden("ZH 999999") == nullptr);
  });

  testfall("Spy: Echter ArraySpeicher ist nach MAX_VEHICLES Fahrzeugen voll", [] {
    ArraySpeicher echt;
    SpeicherSpy spy(echt);
    FakeUhr uhr;
    uhr.stellen(START);
    DummyLog log;
    Parkhaus p(spy, uhr, log);

    for (int i = 1; i <= MAX_VEHICLES; i++) CHECK_EQ(p.einfahrt("AG " + std::to_string(i), "").status, 200);
    CHECK_EQ(p.einfahrt("AG 999", "").code, std::string("FULL"));
    CHECK_EQ(spy.hinzugefuegt.size(), (size_t)MAX_VEHICLES + 1);  // Versuch wurde gemacht ...
    CHECK_EQ(echt.alle().size(), (size_t)MAX_VEHICLES);           // ... aber nicht gespeichert
  });

  return testing::ergebnis();
}
