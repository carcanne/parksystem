// Unit-Tests der Schranken-Logik (schranken/durchfahrt.h) ohne Hardware und ohne Netzwerk.
// Servo -> SchrankeMock, Kasse -> KasseStub, millis() -> FakeUhr.
//
//   make unit   (oder: c++ -std=c++17 -Wall -o build/test_schranken tests/test_schranken.cpp)

#include "../schranken/durchfahrt.h"
#include "doubles/schranken_doubles.h"
#include "testing.h"

using testing::enthaelt;
using testing::feld;
using testing::testfall;

static const char OK_EINFAHRT[] =
    "{\"ok\":true,\"code\":\"OK\",\"message\":\"Willkommen! AG 123456 ist um 12.11.2026 10:00:00 "
    "eingefahren.\"}";
static const char OK_AUSFAHRT[] =
    "{\"ok\":true,\"code\":\"OK\",\"message\":\"Gute Fahrt! AG 123456 hat das Parkhaus verlassen.\"}";

static std::string abgewiesen(const char* code, const char* meldung) {
  return std::string("{\"ok\":false,\"code\":\"") + code + "\",\"message\":\"" + meldung + "\"}";
}

// Abweisung durch die Kasse: Schranke darf nicht öffnen, Antwort wird unverändert
// an die Weboberfläche weitergegeben.
static void pruefeBleibtZu(Richtung richtung, int status, const std::string& body, const char* code) {
  SchrankeMock schranke;
  KasseStub kasse(status, body);
  FakeUhr uhr;
  Durchfahrt d(richtung, schranke, kasse, uhr);

  Antwort a = d.fahrzeugErkannt("egal", "1793000000");

  CHECK_EQ(schranke.oeffnenAufrufe, 0);
  CHECK(!d.istOffen());
  CHECK_EQ(a.status, status);
  CHECK_EQ(feld(a.json, "code"), std::string(code));
}

int main() {
  testfall("TF-01/04/09 Einfahrt: Kasse stimmt zu, Einfahrtsschranke öffnet genau einmal", [] {
    SchrankeMock schranke;
    KasseStub kasse(200, OK_EINFAHRT);
    FakeUhr uhr;
    Durchfahrt einfahrt(Richtung::Einfahrt, schranke, kasse, uhr);

    Antwort a = einfahrt.fahrzeugErkannt("AG 123456", "1793000000");

    CHECK_EQ(schranke.oeffnenAufrufe, 1);
    CHECK_EQ(schranke.schliessenAufrufe, 0);
    CHECK(einfahrt.istOffen());
    CHECK_EQ(a.status, 200);
    CHECK_EQ(a.json, std::string(OK_EINFAHRT));
  });

  testfall("TF-02 Einfahrtsschranke schliesst nach 5 s selbstständig (nicht früher)", [] {
    SchrankeMock schranke;
    KasseStub kasse(200, OK_EINFAHRT);
    FakeUhr uhr;
    Durchfahrt einfahrt(Richtung::Einfahrt, schranke, kasse, uhr);
    einfahrt.fahrzeugErkannt("AG 123456", "1793000000");

    uhr.vorstellen(4999);
    einfahrt.update();
    CHECK(einfahrt.istOffen());
    CHECK_EQ(schranke.schliessenAufrufe, 0);

    uhr.vorstellen(1);
    einfahrt.update();
    CHECK(!einfahrt.istOffen());
    CHECK_EQ(schranke.schliessenAufrufe, 1);

    uhr.vorstellen(60000);
    einfahrt.update();
    CHECK_EQ(schranke.protokoll, std::string("auf zu "));
  });

  testfall("TF-02 Zweites Fahrzeug während offener Schranke verlängert die Öffnungszeit", [] {
    SchrankeMock schranke;
    KasseStub kasse(200, OK_EINFAHRT);
    FakeUhr uhr;
    Durchfahrt einfahrt(Richtung::Einfahrt, schranke, kasse, uhr);
    einfahrt.fahrzeugErkannt("AG 123456", "1793000000");
    uhr.vorstellen(3000);
    einfahrt.fahrzeugErkannt("LU 4711", "1793000003");

    uhr.vorstellen(3000);
    einfahrt.update();
    CHECK(einfahrt.istOffen());
    uhr.vorstellen(2000);
    einfahrt.update();
    CHECK(!einfahrt.istOffen());
  });

  testfall("TF-05 Kennzeichen ohne Buchstaben: Schranke bleibt zu", [] {
    pruefeBleibtZu(Richtung::Einfahrt, 400, abgewiesen("NO_LETTERS", "keine Buchstaben"), "NO_LETTERS");
  });
  testfall("TF-06 Leeres Kennzeichen: Schranke bleibt zu", [] {
    pruefeBleibtZu(Richtung::Einfahrt, 400, abgewiesen("EMPTY", "Kennzeichen fehlt"), "EMPTY");
  });
  testfall("TF-07/08 Ungültiges Format: Schranke bleibt zu", [] {
    pruefeBleibtZu(Richtung::Einfahrt, 400, abgewiesen("FORMAT", "ungültiges Format"), "FORMAT");
  });
  testfall("TF-10 Fahrzeug bereits im Parkhaus: Schranke bleibt zu", [] {
    pruefeBleibtZu(Richtung::Einfahrt, 409, abgewiesen("DUPLICATE", "bereits im Parkhaus"), "DUPLICATE");
  });
  testfall("Ergänzung: Parkhaus voll: Schranke bleibt zu", [] {
    pruefeBleibtZu(Richtung::Einfahrt, 409, abgewiesen("FULL", "voll"), "FULL");
  });
  testfall("TF-32 Uhr der Kasse nicht gestellt: Schranke bleibt zu", [] {
    pruefeBleibtZu(Richtung::Einfahrt, 503, abgewiesen("NO_TIME", "Uhrzeit nicht gestellt"), "NO_TIME");
  });

  testfall("TF-22/23 Ausfahrt bezahlt: Ausfahrtsschranke öffnet und schliesst nach 5 s", [] {
    SchrankeMock schranke;
    KasseStub kasse(200, OK_AUSFAHRT);
    FakeUhr uhr;
    Durchfahrt ausfahrt(Richtung::Ausfahrt, schranke, kasse, uhr);

    Antwort a = ausfahrt.fahrzeugErkannt("AG 123456", "1793000000");
    CHECK_EQ(a.status, 200);
    CHECK_EQ(schranke.oeffnenAufrufe, 1);

    uhr.vorstellen(5000);
    ausfahrt.update();
    CHECK(!ausfahrt.istOffen());
    CHECK_EQ(schranke.protokoll, std::string("auf zu "));
  });

  testfall("TF-24/27 Unbekanntes Fahrzeug an der Ausfahrt: Schranke bleibt zu", [] {
    pruefeBleibtZu(Richtung::Ausfahrt, 404, abgewiesen("UNKNOWN", "unbekannt"), "UNKNOWN");
  });

  testfall("TF-26/31 Gebühr offen: Schranke bleibt zu, Meldung mit Betrag wird weitergegeben", [] {
    std::string body =
        "{\"ok\":false,\"code\":\"UNPAID\",\"message\":\"Die Parkgebühr von CHF 2.00 für ZH 999999 "
        "ist noch offen.\",\"open\":\"CHF 2.00\"}";
    pruefeBleibtZu(Richtung::Ausfahrt, 402, body, "UNPAID");

    SchrankeMock schranke;
    KasseStub kasse(402, body);
    FakeUhr uhr;
    Durchfahrt ausfahrt(Richtung::Ausfahrt, schranke, kasse, uhr);
    CHECK(enthaelt(ausfahrt.fahrzeugErkannt("ZH 999999", "").json, "CHF 2.00"));
  });

  testfall("TF-28 Kasse antwortet nicht (Ausfahrt): Schranke bleibt zu, Verbindungsfehler", [] {
    SchrankeMock schranke;
    KasseStub kasse(-11, "");  // HTTPC_ERROR_READ_TIMEOUT
    FakeUhr uhr;
    Durchfahrt ausfahrt(Richtung::Ausfahrt, schranke, kasse, uhr);

    Antwort a = ausfahrt.fahrzeugErkannt("LU 4711", "1793000000");

    CHECK_EQ(a.status, 503);
    CHECK_EQ(feld(a.json, "code"), std::string("CONNECTION"));
    CHECK(enthaelt(a.json, "Verbindungsfehler"));
    CHECK_EQ(schranke.oeffnenAufrufe, 0);
  });

  testfall("TF-29 Kasse antwortet nicht (Einfahrt): Schranke bleibt zu, Verbindungsfehler", [] {
    SchrankeMock schranke;
    KasseStub kasse(-1, "");  // HTTPC_ERROR_CONNECTION_REFUSED
    FakeUhr uhr;
    Durchfahrt einfahrt(Richtung::Einfahrt, schranke, kasse, uhr);

    Antwort a = einfahrt.fahrzeugErkannt("SG 55555", "1793000000");

    CHECK_EQ(a.status, 503);
    CHECK_EQ(feld(a.json, "code"), std::string("CONNECTION"));
    CHECK_EQ(schranke.oeffnenAufrufe, 0);
  });

  testfall("TF-28 Verbindungsfehler ist von offener Gebühr unterscheidbar", [] {
    SchrankeMock s1, s2;
    KasseStub weg(-11, "");
    KasseStub unbezahlt(402, abgewiesen("UNPAID", "offen"));
    FakeUhr uhr;
    Antwort a1 = Durchfahrt(Richtung::Ausfahrt, s1, weg, uhr).fahrzeugErkannt("LU 4711", "");
    Antwort a2 = Durchfahrt(Richtung::Ausfahrt, s2, unbezahlt, uhr).fahrzeugErkannt("LU 4711", "");
    CHECK(feld(a1.json, "code") != feld(a2.json, "code"));
    CHECK(a1.status != a2.status);
  });

  testfall("Ergänzung: Ungültige Antwort der Kasse (kein JSON) gilt als Verbindungsfehler", [] {
    SchrankeMock schranke;
    KasseStub kasse(-1200, "");  // HttpKasse meldet -1000 - HTTP-Status
    FakeUhr uhr;
    Durchfahrt einfahrt(Richtung::Einfahrt, schranke, kasse, uhr);
    CHECK_EQ(einfahrt.fahrzeugErkannt("AG 123456", "").status, 503);
    CHECK_EQ(schranke.oeffnenAufrufe, 0);
  });

  testfall("Ergänzung: HTTP 200 ohne Zustimmung (ok:false) öffnet nicht", [] {
    pruefeBleibtZu(Richtung::Einfahrt, 200, abgewiesen("X", "nein"), "X");
  });

  return testing::ergebnis();
}
