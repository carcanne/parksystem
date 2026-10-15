// Reine Logik ohne Arduino-Abhaengigkeiten, damit sie auch auf dem PC
// getestet werden kann (siehe tests/test_logic.cpp).
#pragma once

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>

enum PlateCheck {
  PLATE_OK,
  PLATE_EMPTY,       // leere Eingabe (nur Leerzeichen zaehlt auch als leer)
  PLATE_NO_LETTERS,  // z.B. "123456"
  PLATE_BAD_FORMAT   // z.B. "A 123456", "AG 1234567"
};

const size_t PLATE_LEN = 16;  // Platz fuer "AG 123456" inkl. Nullterminator

// Vereinheitlicht ein Kennzeichen (Grossbuchstaben, Leerzeichen entfernt) und
// prueft das Format: zwei Buchstaben, danach ein bis sechs Ziffern.
// Bei PLATE_OK steht das Kennzeichen im Format "AG 123456" in out.
inline PlateCheck normalizePlate(const char* in, char* out, size_t outSize) {
  char compact[32];
  size_t len = 0;
  bool hasLetter = false;

  if (outSize > 0) out[0] = '\0';
  if (in == nullptr) return PLATE_EMPTY;

  for (const char* p = in; *p != '\0'; p++) {
    unsigned char c = (unsigned char)*p;
    if (isspace(c)) continue;
    if (len >= sizeof(compact) - 1) return PLATE_BAD_FORMAT;
    if (isalpha(c)) hasLetter = true;
    compact[len++] = (char)toupper(c);
  }
  compact[len] = '\0';

  if (len == 0) return PLATE_EMPTY;
  if (!hasLetter) return PLATE_NO_LETTERS;
  if (len < 3 || len > 8) return PLATE_BAD_FORMAT;
  if (!isupper((unsigned char)compact[0]) || !isupper((unsigned char)compact[1])) {
    return PLATE_BAD_FORMAT;
  }
  for (size_t i = 2; i < len; i++) {
    if (!isdigit((unsigned char)compact[i])) return PLATE_BAD_FORMAT;
  }

  snprintf(out, outSize, "%c%c %s", compact[0], compact[1], compact + 2);
  return PLATE_OK;
}

// Aufenthaltsdauer in ganzen Minuten, Sekunden werden abgeschnitten.
inline long parkedMinutes(long long entryEpoch, long long nowEpoch) {
  if (nowEpoch <= entryEpoch) return 0;
  return (long)((nowEpoch - entryEpoch) / 60);
}

// Tarif: jede angefangene Stunde kostet CHF 2.00, auch die erste.
// 0-60 Min = 2.00, 61-120 Min = 4.00, 121-180 Min = 6.00 ...
const long RAPPEN_PER_HOUR = 200;

inline long feeRappen(long minutes) {
  long hours = minutes <= 0 ? 1 : (minutes + 59) / 60;
  return hours * RAPPEN_PER_HOUR;
}

// Formatiert einen Betrag in Rappen als "CHF 6.00".
inline void formatChf(long rappen, char* out, size_t outSize) {
  snprintf(out, outSize, "CHF %ld.%02ld", rappen / 100, rappen % 100);
}
