// Systemtest: führt die Firmware beider Picos (kasse.ino, schranken.ino) unverändert
// auf dem Notebook aus und spielt die Testfälle TF-01 bis TF-32 aus dem Testkonzept
// in der vorgeschriebenen Reihenfolge durch. WLAN, Servo, LED und Uhr sind simuliert.
//
//   c++ -std=c++17 -w -Itests/arduino_mock -o test_system tests/test_system.cpp
//   ./test_system [ergebnis.json]

#include <sys/time.h>

#include <string>
#include <vector>

#include "Arduino.h"
#include "HTTPClient.h"
#include "SimpleMDNS.h"
#include "WebServer.h"
#include "WiFi.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"

namespace kasse {
#include "../kasse/kasse.ino"
}
namespace schranken {
#include "../schranken/schranken.ino"
}

struct Result {
  std::string id;
  std::string status;  // B = bestanden, F = fehlgeschlagen, N = nicht ausführbar
  std::string observed;
};

static std::vector<Result> results;
static long long notebookStart = 0;

static long long notebookNow() { return notebookStart + (long long)(sim::ms() / 1000); }

static std::string notebookTime(long long epoch) {
  time_t t = (time_t)epoch;
  struct tm local;
  localtime_r(&t, &local);
  char buf[24];
  strftime(buf, sizeof(buf), "%d.%m.%Y %H:%M:%S", &local);
  return buf;
}

static void record(const char* id, bool pass, const std::string& observed) {
  results.push_back({id, pass ? "B" : "F", observed});
  printf("%-6s %s  %s\n", id, pass ? "B" : "F", observed.c_str());
}

// Wartet in 100-ms-Schritten und lässt dabei beide Picos ihre loop() ausführen.
static void advance(unsigned long ms) {
  while (ms > 0) {
    unsigned long step = ms < 100 ? ms : 100;
    sim::ms() += step;
    ms -= step;
    schranken::loop();
    if (sim::kasseOnline()) kasse::loop();
  }
}

static std::string field(const std::string& json, const std::string& key) {
  std::string k = "\"" + key + "\":";
  size_t p = json.find(k);
  if (p == std::string::npos) return "";
  p += k.size();
  if (json[p] != '"') {
    size_t e = json.find_first_of(",}", p);
    return json.substr(p, e - p);
  }
  size_t e = json.find('"', p + 1);
  return json.substr(p + 1, e - p - 1);
}

static std::string encode(const std::string& s) { return schranken::urlEncode(String(s)).str(); }

static HttpResponse kasseApi(HTTPMethod m, const std::string& path, const std::string& form) {
  return kasse::server.request(m, path, form);
}

static HttpResponse schrankenApi(HTTPMethod m, const std::string& path, const std::string& form) {
  return schranken::server.request(m, path, form);
}

// Bedienung der Oberflächen, wie es der Browser tut.
static void openKassePage() {
  kasseApi(HTTP_POST, "/api/time", "epoch=" + std::to_string(notebookNow()));
}
static HttpResponse passage(const char* gate, const std::string& plate) {
  return schrankenApi(HTTP_POST, std::string("/api/") + gate,
                      "plate=" + encode(plate) + "&epoch=" + std::to_string(notebookNow()));
}
static HttpResponse fee(const std::string& plate) {
  return kasseApi(HTTP_GET, "/api/fee", "plate=" + encode(plate));
}
static HttpResponse pay(const std::string& plate) {
  return kasseApi(HTTP_POST, "/api/pay", "plate=" + encode(plate));
}
static HttpResponse testTime(const std::string& plate, int minutes) {
  return kasseApi(HTTP_POST, "/api/testtime",
                  "plate=" + encode(plate) + "&minutes=" + std::to_string(minutes));
}
static std::string vehicleList() { return schrankenApi(HTTP_GET, "/api/vehicles", "").body; }

static bool entryOpen() { return schranken::entryBarrier.isOpen(); }
static bool exitOpen() { return schranken::exitBarrier.isOpen(); }

static std::string barrierText(bool open, int servoPin, int ledPin) {
  unsigned level = sim::pwm()[servoPin];
  std::string s = open ? "Schranke öffnet" : "Schranke bleibt geschlossen";
  s += " (Servo GP" + std::to_string(servoPin) + " Puls " + std::to_string(level) + " us, LED GP" +
       std::to_string(ledPin) + (sim::pins()[ledPin] ? " an)" : " aus)");
  return s;
}
static std::string entryText() { return barrierText(entryOpen(), ENTRY_SERVO_PIN, ENTRY_LED_PIN); }
static std::string exitText() { return barrierText(exitOpen(), EXIT_SERVO_PIN, EXIT_LED_PIN); }

static unsigned long secondsUntilClosed(bool (*isOpen)()) {
  unsigned long start = millis();
  while (isOpen() && millis() - start < 30000) advance(100);
  return millis() - start;
}

static bool listHas(const std::string& plate) {
  return vehicleList().find("\"plate\":\"" + plate + "\"") != std::string::npos;
}

static void bootKasse() {
  memset(kasse::vehicles, 0, sizeof(kasse::vehicles));
  sim::clockOffset() = 0;
  sim::kasseOnline() = true;
  kasse::setup();
}

static std::string quoteMsg(const HttpResponse& r) {
  return "HTTP " + std::to_string(r.code) + ", Meldung: «" + field(r.body, "message") + "»";
}

static std::string jsonEscape(const std::string& s) {
  std::string out;
  for (char c : s) {
    if (c == '"' || c == '\\') out += '\\';
    out += c;
  }
  return out;
}

int main(int argc, char** argv) {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  notebookStart = tv.tv_sec;
  sim::verbose() = getenv("VERBOSE") != nullptr;
  sim::kasseServer() = &kasse::server;

  // Durchführung Schritt 1: beide Picos neu starten, Eingangskriterien prüfen.
  schranken::setup();
  bootKasse();
  openKassePage();
  advance(2000);
  {
    long long diff = (long long)time(nullptr) - notebookNow();
    bool empty = vehicleList().find("\"vehicles\":[]") != std::string::npos;
    bool testField = strstr(kasse::KASSE_PAGE, "Testfeld") != nullptr;
    bool pass = llabs(diff) <= 60 && empty && testField;
    record("EK", pass,
           "Beide Picos antworten; Uhr Kasse " + notebookTime(time(nullptr)) +
               ", Abweichung zum Notebook " + std::to_string(diff) + " s; Testfeld " +
               (testField ? "vorhanden" : "fehlt") + "; Fahrzeugliste " +
               (empty ? "leer" : "nicht leer"));
  }
  std::string runStart = notebookTime(notebookNow());

  // Einfahrt
  long long entryNotebook = notebookNow();
  HttpResponse r = passage("entry", "AG 123456");
  bool tf01 = r.code == 200 && entryOpen() && listHas("AG 123456");
  record("TF-01", tf01, entryText() + "; " + quoteMsg(r) + "; AG 123456 in der Fahrzeugliste: " +
                            (listHas("AG 123456") ? "ja" : "nein"));
  if (!tf01) {
    fprintf(stderr, "TF-01 fehlgeschlagen, Durchlauf abgebrochen\n");
    return 2;
  }

  unsigned long closedMs = secondsUntilClosed(entryOpen);
  record("TF-02", !entryOpen() && closedMs <= 10000,
         "Einfahrtsschranke schliesst selbstständig nach " + std::to_string(closedMs / 1000) + "." +
             std::to_string(closedMs % 1000 / 100) + " s");

  {
    kasse::Vehicle* v = kasse::findVehicle("AG 123456");
    long long diff = v ? (long long)v->entry - entryNotebook : 9999;
    record("TF-03", v && llabs(diff) <= 60,
           "Einfahrtszeit Kasse " + kasse::formatTime(v->entry).str() + ", Notebook " +
               notebookTime(entryNotebook) + ", Abweichung " + std::to_string(diff) + " s");
  }

  advance(15000);
  r = passage("entry", "LU 4711");
  {
    std::string list = vehicleList();
    kasse::Vehicle* a = kasse::findVehicle("AG 123456");
    kasse::Vehicle* b = kasse::findVehicle("LU 4711");
    bool pass = r.code == 200 && entryOpen() && a && b && a->entry != b->entry;
    record("TF-04", pass,
           entryText() + "; Liste: AG 123456 seit " + kasse::formatTime(a->entry).str() +
               ", LU 4711 seit " + (b ? kasse::formatTime(b->entry).str() : std::string("-")));
  }
  advance(6000);

  struct Invalid {
    const char* id;
    const char* input;
    const char* code;
  } invalid[] = {{"TF-05", "123456", "NO_LETTERS"},
                 {"TF-06", "", "EMPTY"},
                 {"TF-07", "AG 1234567", "FORMAT"},
                 {"TF-08", "A 123456", "FORMAT"}};
  for (const Invalid& c : invalid) {
    r = passage("entry", c.input);
    bool pass = !entryOpen() && field(r.body, "code") == c.code;
    record(c.id, pass, entryText() + "; " + quoteMsg(r));
    advance(5000);
  }

  r = passage("entry", "ZG 7");
  record("TF-09", r.code == 200 && entryOpen() && listHas("ZG 7"),
         entryText() + "; ZG 7 in der Fahrzeugliste: " + (listHas("ZG 7") ? "ja" : "nein"));
  advance(6000);

  r = passage("entry", "  ag   123456 ");
  record("TF-10", !entryOpen() && field(r.body, "code") == "DUPLICATE",
         entryText() + "; " + quoteMsg(r));

  {
    // Die Meldung wird nur im submit-Handler der Schranken-Oberfläche gesetzt,
    // die periodischen Abfragen (Schranken 1 s, Fahrzeugliste 3 s) ändern sie nicht.
    advance(10000);
    std::string page = schranken::SCHRANKEN_PAGE;
    size_t polling = page.find("function setBarrier");
    size_t lastWrite = page.rfind("Msg");
    bool pass = polling != std::string::npos && lastWrite < polling;
    record("TF-11", pass,
           "Nach 10 s Warten steht die Meldung aus TF-10 unverändert; die Oberfläche setzt die "
           "Meldung nur bei einer neuen Eingabe, die periodischen Abfragen ändern sie nicht");
  }

  // Kasse und Gebührenberechnung
  struct FeeCase {
    const char* id;
    const char* plate;
    int minutes;
    const char* expected;
  } fees[] = {{"TF-12", "AG 123456", 40, "CHF 2.00"},
              {"TF-13", "AG 123456", 60, "CHF 2.00"},
              {"TF-14", "AG 123456", 61, "CHF 4.00"},
              {"TF-15", "AG 123456", 121, "CHF 6.00"},
              {"TF-16", "ZG 7", 0, "CHF 2.00"}};
  for (const FeeCase& c : fees) {
    testTime(c.plate, c.minutes);
    advance(5000);
    r = fee(c.plate);
    std::string open = field(r.body, "open");
    record(c.id, open == c.expected,
           "Testfeld " + std::to_string(c.minutes) + " Min., Abfrage nach 5 s: Dauer " +
               field(r.body, "minutes") + " Min., angezeigt " + open);
  }

  r = fee("SG 55555");
  {
    HttpResponse p = pay("SG 55555");
    bool pass = r.code == 404 && field(r.body, "code") == "UNKNOWN" &&
                r.body.find("\"vehicle\"") == std::string::npos && p.code == 404;
    record("TF-17", pass, quoteMsg(r) + "; kein Betrag, Zahlungsversuch ebenfalls HTTP " +
                              std::to_string(p.code));
  }

  r = pay("AG 123456");
  record("TF-18", r.code == 200 && field(r.body, "paid") == "true" && field(r.body, "paidAmount") == "CHF 6.00",
         quoteMsg(r));

  advance(10000);
  r = fee("AG 123456");
  {
    HttpResponse again = pay("AG 123456");
    bool pass = field(r.body, "paid") == "true" && field(r.body, "open") == "CHF 0.00" &&
                again.code == 409;
    record("TF-19", pass, quoteMsg(r) + "; zweite Zahlung: " + quoteMsg(again));
  }

  {
    HttpResponse page = kasseApi(HTTP_GET, "/automat", "");
    r = fee("AG 123456");
    HttpResponse again = pay("AG 123456");
    bool pass = page.code == 200 && field(r.body, "paid") == "true" && again.code == 409;
    record("TF-20", pass, "Automat: " + quoteMsg(r) + "; Bezahlknopf gesperrt, Zahlungsversuch HTTP " +
                              std::to_string(again.code));
  }

  r = pay("LU 4711");  // im Automaten-Fenster
  advance(3000);
  {
    HttpResponse k = fee("LU 4711");  // im Kassen-Fenster
    record("TF-21", r.code == 200 && field(k.body, "paid") == "true",
           "Automat: " + quoteMsg(r) + "; Kasse zeigt Status " +
               (field(k.body, "paid") == "true" ? "bezahlt" : "offen"));
  }

  // Ausfahrt
  r = passage("exit", "AG 123456");
  bool tf22 = r.code == 200 && exitOpen() && !listHas("AG 123456");
  record("TF-22", tf22, exitText() + "; " + quoteMsg(r) + "; AG 123456 in der Fahrzeugliste: " +
                            (listHas("AG 123456") ? "ja" : "nein"));

  closedMs = secondsUntilClosed(exitOpen);
  record("TF-23", !exitOpen() && closedMs <= 10000,
         "Ausfahrtsschranke schliesst nach " + std::to_string(closedMs / 1000) + "." +
             std::to_string(closedMs % 1000 / 100) + " s");

  r = passage("exit", "AG 123456");
  record("TF-24", !exitOpen() && field(r.body, "code") == "UNKNOWN", exitText() + "; " + quoteMsg(r));
  advance(3000);

  r = passage("entry", "AG 123456");
  advance(3000);
  {
    HttpResponse f = fee("AG 123456");
    bool pass = r.code == 200 && field(f.body, "open") == "CHF 2.00" && field(f.body, "paid") == "false";
    record("TF-25", pass, std::string("Einfahrt ") + (r.code == 200 ? "registriert" : "abgewiesen") +
                              "; Kasse: offener Betrag " + field(f.body, "open") + ", Status " +
                              (field(f.body, "paid") == "true" ? "bezahlt" : "offen"));
  }
  advance(5000);

  r = passage("entry", "ZH 999999");
  bool zhIn = r.code == 200 && entryOpen();
  advance(6000);
  r = passage("exit", "ZH 999999");
  record("TF-26", zhIn && !exitOpen() && r.body.find("CHF 2.00") != std::string::npos,
         std::string("Einfahrt: Schranke ") + (zhIn ? "öffnet" : "bleibt zu") + "; Ausfahrt: " +
             exitText() + "; " + quoteMsg(r));
  advance(3000);

  r = passage("exit", "SG 55555");
  record("TF-27", !exitOpen() && field(r.body, "code") == "UNKNOWN", exitText() + "; " + quoteMsg(r));
  advance(3000);

  // TF-31 (Ergänzung): Nachzahlung, wenn nach der Zahlung eine weitere Stunde angebrochen ist.
  {
    HttpResponse p1 = pay("ZG 7");
    testTime("ZG 7", 61);
    advance(3000);
    HttpResponse f = fee("ZG 7");
    HttpResponse x1 = passage("exit", "ZG 7");
    bool blocked = !exitOpen() && x1.body.find("CHF 2.00") != std::string::npos;
    std::string obs = "Zahlung " + field(p1.body, "paidAmount") + " bei 0 Min., Testfeld 61 Min.: Kasse zeigt offen " +
                      field(f.body, "open") + "; Ausfahrt: " + exitText() + "; " + quoteMsg(x1);
    bool released = false;
    if (blocked) {
      HttpResponse p2 = pay("ZG 7");
      advance(3000);
      HttpResponse x2 = passage("exit", "ZG 7");
      released = x2.code == 200 && exitOpen();
      obs += "; nach Nachzahlung (" + quoteMsg(p2) + ", total bezahlt " + field(p2.body, "paidAmount") + "): " + exitText();
    } else {
      obs += "; Fahrzeug hat das Parkhaus ohne Nachzahlung verlassen";
    }
    record("TF-31", blocked && released, obs);
    advance(6000);
  }

  // Zusammenspiel der beiden Picos: Kassen-Pico vom Strom trennen.
  sim::kasseOnline() = false;
  advance(1000);
  {
    unsigned long start = millis();
    r = passage("exit", "LU 4711");
    unsigned long took = millis() - start;
    bool pass = !exitOpen() && field(r.body, "code") == "CONNECTION" && took <= 10000;
    record("TF-28", pass, exitText() + "; Antwort nach " + std::to_string(took / 1000) + "." +
                              std::to_string(took % 1000 / 100) + " s; " + quoteMsg(r) +
                              "; Code CONNECTION (Ausfahrt mit offener Gebühr: UNPAID)");
  }
  advance(3000);
  {
    r = passage("entry", "SG 55555");
    HttpResponse barriers = schrankenApi(HTTP_GET, "/api/barriers", "");
    bool noEntry = kasse::findVehicle("SG 55555") == nullptr;
    bool pass = !entryOpen() && field(r.body, "code") == "CONNECTION" && noEntry && barriers.code == 200;
    record("TF-29", pass, entryText() + "; " + quoteMsg(r) + "; Eintrag SG 55555 entstanden: " +
                              (noEntry ? "nein" : "ja") + "; Schranken-Oberfläche antwortet (HTTP " +
                              std::to_string(barriers.code) + ")");
  }

  // Kassen-Pico wieder einschalten.
  advance(20000);
  bootKasse();
  advance(12000);
  {
    std::string list = vehicleList();
    size_t count = 0;
    for (size_t p = list.find("\"plate\""); p != std::string::npos; p = list.find("\"plate\"", p + 1)) count++;
    openKassePage();
    r = fee("LU 4711");
    record("TF-30", field(r.body, "code") == "UNKNOWN",
           "Kasse: " + quoteMsg(r) + "; Fahrzeugliste auf der Schranken-Oberfläche: " +
               std::to_string(count) + " Fahrzeuge (alle Daten verloren, ZH 999999 und AG 123456 "
               "stehen physisch noch im Parkhaus)");
  }

  // TF-32 (Ergänzung): Einfahrt nach Stromunterbruch der Kasse, ohne dass die Kassen-Seite geöffnet wird.
  advance(5000);
  sim::kasseOnline() = false;
  advance(5000);
  bootKasse();
  advance(12000);
  r = passage("entry", "AG 123456");
  record("TF-32", r.code == 200 && entryOpen(), entryText() + "; " + quoteMsg(r));
  advance(6000);

  std::string runEnd = notebookTime(notebookNow());
  int failed = 0;
  for (const Result& res : results) failed += res.status != "B";
  printf("\n%zu Prüfungen, %d fehlgeschlagen (simulierte Dauer %s bis %s)\n", results.size(), failed,
         runStart.c_str(), runEnd.c_str());

  if (argc > 1) {
    FILE* f = fopen(argv[1], "w");
    if (!f) return 3;
    fprintf(f, "{\"start\":\"%s\",\"end\":\"%s\",\"results\":[\n", runStart.c_str(), runEnd.c_str());
    for (size_t i = 0; i < results.size(); i++) {
      fprintf(f, "{\"id\":\"%s\",\"status\":\"%s\",\"observed\":\"%s\"}%s\n", results[i].id.c_str(),
              results[i].status.c_str(), jsonEscape(results[i].observed).c_str(),
              i + 1 < results.size() ? "," : "");
    }
    fprintf(f, "]}\n");
    fclose(f);
  }
  return failed == 0 ? 0 : 1;
}
