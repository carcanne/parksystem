# Parksystem mit Ein- und Ausfahrtsschranke

Modul 450, BBZW Sursee

## Gruppenmitglieder

- Leonardo Ciccone
- Nevio Carcangiu

## Kurze Beschreibung der Umsetzung

Das Parksystem läuft auf zwei Raspberry Pi Pico W und ist mit Arduino umgesetzt
(Core [arduino-pico](https://github.com/earlephilhower/arduino-pico)). Die beiden
Picos sprechen über HTTP im selben WLAN miteinander.

```
 Browser (Schranken)          Browser (Kasse / Automat)
        │                              │
        ▼                              ▼
 ┌───────────────┐   HTTP    ┌──────────────────┐
 │ Pico 2        │ ────────► │ Pico 1           │
 │ Schranken     │           │ Kasse            │
 │ Servo GP8/GP9 │ ◄──────── │ Fahrzeugdaten    │
 └───────────────┘   JSON    └──────────────────┘
```

**Pico 1 – Kasse (`kasse/`)**

- Verwaltet Kennzeichen, Einfahrtszeit und Bezahlstatus im Arbeitsspeicher.
- Prüft und vereinheitlicht das Kennzeichen (zwei Buchstaben + ein bis sechs Ziffern,
  Gross-/Kleinschreibung und Leerzeichen egal).
- Berechnet die Gebühr: jede angefangene Stunde CHF 2.00, auch die erste
  (0–60 Min = CHF 2.00, 61–120 Min = CHF 4.00, …). Sekunden werden abgeschnitten.
- Weboberfläche Kasse unter `http://<IP Kasse>/`, Automat unter `http://<IP Kasse>/automat`
  (gleiche Daten, Zahlungen sind sofort in beiden Fenstern sichtbar).
- Testfeld auf der Kasse: setzt die Einfahrtszeit eines Fahrzeugs auf „vor N Minuten“
  (für TF-12 bis TF-16 aus dem Testkonzept).
- Uhrzeit per NTP, Zeitzone Schweiz inkl. Sommerzeit.

**Pico 2 – Schranken (`schranken/`)**

- Weboberfläche unter `http://<IP Schranken>/` simuliert die Kamera: Kennzeichen bei
  Ein- oder Ausfahrt eintippen.
- Jede Ein- und Ausfahrt wird bei der Kasse angefragt. Die Schranke öffnet nur, wenn
  die Kasse zustimmt, und schliesst nach 5 Sekunden selbstständig.
- Hält selbst keine Fahrzeugdaten, die Fahrzeugliste kommt immer von der Kasse.
- Antwortet die Kasse nicht innerhalb von 3 Sekunden, bleiben beide Schranken zu und
  es erscheint ein (orange markierter) Verbindungsfehler.
- Jede Abweisung zeigt den Grund als ganzen Satz, bei offener Gebühr mit Betrag. Die
  Meldung bleibt stehen, bis an der Schranke die nächste Eingabe erfolgt.

### Verdrahtung Schranken-Pico

| Bauteil                   | Pico-Pin                      |
|---------------------------|-------------------------------|
| Servo Einfahrt Signal (orange) | GP8 (Pin 11)             |
| Servo Ausfahrt Signal (orange) | GP9 (Pin 12)             |
| Servo VCC (rot), beide    | VBUS (Pin 40, 5 V)            |
| Servo GND (braun), beide  | GND (z.B. Pin 8)              |
| LED Einfahrt (optional)   | GP14 (Pin 19) über 220 Ω an GND |
| LED Ausfahrt (optional)   | GP15 (Pin 20) über 220 Ω an GND |

Der Kassen-Pico braucht keine zusätzliche Hardware. Die Onboard-LED beider Picos
leuchtet, sobald das WLAN verbunden ist.

### Inbetriebnahme

1. In der Arduino IDE unter *Einstellungen → Zusätzliche Boardverwalter-URLs*
   `https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json`
   eintragen und im Boardverwalter „Raspberry Pi Pico/RP2040/RP2350“ installieren.
2. In `kasse/config.h` und `schranken/config.h` WLAN-Name und Passwort eintragen.
3. `kasse/kasse.ino` mit Board „Raspberry Pi Pico W“ auf Pico 1 laden. Im Serial
   Monitor (115200 Baud) erscheint die IP-Adresse der Kasse.
4. Diese IP in `schranken/config.h` bei `KASSE_HOST` eintragen und
   `schranken/schranken.ino` auf Pico 2 laden.
5. Im Browser drei Fenster öffnen: Schranken, Kasse, Automat.

Viele Schul-WLANs trennen die Geräte voneinander. Dann einen Handy-Hotspot verwenden.

### Tests

Die Kennzeichenprüfung und die Gebührenberechnung liegen in
`kasse/parking_logic.h` ohne Arduino-Abhängigkeit und können auf dem PC getestet
werden (Grenzwerte aus TF-05 bis TF-16):

```sh
c++ -std=c++17 -Wall -o test_logic tests/test_logic.cpp && ./test_logic
```

Das System als Ganzes wird manuell nach dem Testkonzept (TF-01 bis TF-30) getestet.

## Umgesetzte User Stories / Anforderungen

| Story | Inhalt | Umgesetzt | Testfälle |
|-------|--------|-----------|-----------|
| US-1 | Einfahrt registrieren: Schranke öffnet, Kennzeichen und Einfahrtszeit in der Fahrzeugliste, Schranke schliesst nach 5 s, mehrere Fahrzeuge parallel, neue Einfahrt = neuer Aufenthalt | ✅ | TF-01–TF-04, TF-25, TF-30 |
| US-2 | Fehlerhafte Einfahrt abweisen: ungültiges Format, leere Eingabe, bereits im Parkhaus, Vereinheitlichung von Gross-/Kleinschreibung und Leerzeichen | ✅ | TF-05–TF-10 |
| US-3 | Gebühr berechnen nach Tarif, unbekanntes Kennzeichen ohne Betrag | ✅ | TF-12–TF-17 |
| US-4 | Gebühr bezahlen an Kasse oder Automat, keine zweite Zahlung, sofort im anderen Fenster sichtbar | ✅ | TF-18–TF-21 |
| US-5 | Ausfahrt nur bei bezahlter Gebühr, Fahrzeug verschwindet aus der Liste, unbekannte oder bereits ausgefahrene Kennzeichen bleiben draussen | ✅ | TF-22–TF-27 |
| US-6 | Grund einer Abweisung als Satz anzeigen, offener Betrag in CHF, Meldung bleibt stehen | ✅ | TF-05–TF-08, TF-10, TF-11, TF-17, TF-24, TF-26, TF-27 |
| US-7 | Schranken bleiben zu, wenn die Kasse nicht antwortet, eigener Verbindungsfehler, Schranken-Pico bleibt bedienbar | ✅ | TF-28, TF-29 |

Minimaler Funktionsumfang laut Auftrag (Ein- und Ausfahrtsschranke, Berechnung und
Bezahlung der Parkgebühr) ist damit abgedeckt.

### Bekannte Einschränkungen

- Die Daten liegen im Arbeitsspeicher der Kasse. Nach einem Neustart der Kasse sind
  alle Fahrzeuge weg (dokumentiert in TF-30), die Fahrzeugliste der Schranken ist dann leer.
- Kein Zugriffsschutz: Jedes Gerät im WLAN kann die Kasse öffnen und bezahlen.
- Nach der Zahlung wird keine weitere Zeit verrechnet (keine Karenzzeit bis zur Ausfahrt).
- Ohne NTP-Zeit weist die Kasse Einfahrten ab, damit kein Eintrag mit falscher Zeit entsteht.
- Erhält ein Pico nach einem Neustart eine neue IP, muss `KASSE_HOST` angepasst werden.
