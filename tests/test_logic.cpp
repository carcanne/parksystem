#include <stdio.h>
#include <string.h>

#include "../kasse/parking_logic.h"

static int failures = 0;

static void expectPlate(const char* input, PlateCheck expected, const char* normalized) {
  char out[PLATE_LEN];
  PlateCheck got = normalizePlate(input, out, sizeof(out));
  bool ok = got == expected && (expected != PLATE_OK || strcmp(out, normalized) == 0);
  if (!ok) {
    printf("FAIL Kennzeichen \"%s\": erwartet %d \"%s\", erhalten %d \"%s\"\n",
           input, expected, normalized ? normalized : "", got, out);
    failures++;
  }
}

static void expectFee(long minutes, const char* expected) {
  char out[16];
  formatChf(feeRappen(minutes), out, sizeof(out));
  if (strcmp(out, expected) != 0) {
    printf("FAIL Gebuehr %ld Min: erwartet %s, erhalten %s\n", minutes, expected, out);
    failures++;
  }
}

static void expectMinutes(long long entry, long long now, long expected) {
  long got = parkedMinutes(entry, now);
  if (got != expected) {
    printf("FAIL Dauer %lld s: erwartet %ld, erhalten %ld\n", now - entry, expected, got);
    failures++;
  }
}

int main() {
  expectPlate("AG 123456", PLATE_OK, "AG 123456");
  expectPlate("LU 4711", PLATE_OK, "LU 4711");
  expectPlate("ZG 7", PLATE_OK, "ZG 7");
  expectPlate("ZH 999999", PLATE_OK, "ZH 999999");
  expectPlate("  ag   123456 ", PLATE_OK, "AG 123456");
  expectPlate("ag123456", PLATE_OK, "AG 123456");

  expectPlate("123456", PLATE_NO_LETTERS, nullptr);
  expectPlate("", PLATE_EMPTY, nullptr);
  expectPlate("   ", PLATE_EMPTY, nullptr);
  expectPlate("AG 1234567", PLATE_BAD_FORMAT, nullptr);
  expectPlate("A 123456", PLATE_BAD_FORMAT, nullptr);
  expectPlate("AGB 123", PLATE_BAD_FORMAT, nullptr);
  expectPlate("AG", PLATE_BAD_FORMAT, nullptr);
  expectPlate("AG 12A4", PLATE_BAD_FORMAT, nullptr);
  expectPlate("AG-123", PLATE_BAD_FORMAT, nullptr);
  expectPlate("ÄG 123", PLATE_BAD_FORMAT, nullptr);

  expectFee(0, "CHF 2.00");
  expectFee(40, "CHF 2.00");
  expectFee(60, "CHF 2.00");
  expectFee(61, "CHF 4.00");
  expectFee(120, "CHF 4.00");
  expectFee(121, "CHF 6.00");
  expectFee(180, "CHF 6.00");
  expectFee(181, "CHF 8.00");

  expectMinutes(1000, 1000, 0);
  expectMinutes(1000, 1000 + 59, 0);
  expectMinutes(1000, 1000 + 60 * 60 + 59, 60);
  expectMinutes(1000, 1000 + 61 * 60, 61);
  expectMinutes(1000, 500, 0);

  if (failures == 0) {
    printf("Alle Tests bestanden\n");
    return 0;
  }
  printf("%d Test(s) fehlgeschlagen\n", failures);
  return 1;
}
