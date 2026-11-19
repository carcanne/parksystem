#pragma once

// Minimales Test-Gerüst: testfall("Name", [] { CHECK(...); CHECK_EQ(...); });
// return ergebnis();  -> Exit-Code 0, wenn alle Prüfungen bestanden sind.

#include <stdio.h>

#include <sstream>
#include <string>

namespace testing {

struct Stand {
  int faelle = 0;
  int fehlgeschlagen = 0;
  bool aktuellOk = true;
};
inline Stand& stand() {
  static Stand s;
  return s;
}

inline void fehler(const std::string& text, const char* datei, int zeile) {
  printf("    FEHLER %s:%d: %s\n", datei, zeile, text.c_str());
  stand().aktuellOk = false;
}

inline void check(bool ok, const char* ausdruck, const char* datei, int zeile) {
  if (!ok) fehler(ausdruck, datei, zeile);
}

template <typename A, typename B>
void checkEq(const A& ist, const B& soll, const char* ausdruck, const char* datei, int zeile) {
  if (ist == soll) return;
  std::ostringstream o;
  o << ausdruck << ": erwartet \"" << soll << "\", erhalten \"" << ist << "\"";
  fehler(o.str(), datei, zeile);
}

template <typename F>
void testfall(const char* name, F f) {
  stand().faelle++;
  stand().aktuellOk = true;
  f();
  if (!stand().aktuellOk) stand().fehlgeschlagen++;
  printf("%s  %s\n", stand().aktuellOk ? "OK  " : "FAIL", name);
}

inline int ergebnis() {
  printf("\n%d Testfälle, %d fehlgeschlagen\n", stand().faelle, stand().fehlgeschlagen);
  return stand().fehlgeschlagen == 0 ? 0 : 1;
}

// Liest ein Feld aus einer JSON-Antwort, z.B. feld(json, "code").
inline std::string feld(const std::string& json, const std::string& key) {
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

inline bool enthaelt(const std::string& text, const std::string& teil) {
  return text.find(teil) != std::string::npos;
}

}  // namespace testing

#define CHECK(c) testing::check((c), #c, __FILE__, __LINE__)
#define CHECK_EQ(ist, soll) testing::checkEq((ist), (soll), #ist, __FILE__, __LINE__)
