# Parksystem mit Ein- und Ausfahrtsschranke

Modul 450, BBZW Sursee

## Gruppenmitglieder

- Leonardo Ciccone
- Nevio Carcangiu

## Beschreibung

Zwei Raspberry Pi Pico W mit Arduino ([arduino-pico](https://github.com/earlephilhower/arduino-pico)).
Der Schranken-Pico öffnet das WLAN `Parksystem` (Passwort `parksystem`), Kasse und
Notebook verbinden sich damit.

- **Kasse (`kasse/`)**: speichert Fahrzeuge, prüft Kennzeichen, berechnet die Gebühr
  (jede angefangene Stunde CHF 2.00) und nimmt Zahlungen an. Beginnt nach der Zahlung
  eine weitere Stunde, wird die Differenz als Nachzahlung verlangt. Die Uhrzeit wird
  beim Öffnen der Kassen-Seite vom Notebook übernommen, nach einem Neustart der Kasse
  auch mit der ersten Ein- oder Ausfahrt über die Schranken-Seite.
- **Schranken (`schranken/`)**: simuliert die Kamera per Weboberfläche, fragt bei jeder
  Ein- und Ausfahrt die Kasse und öffnet nur bei Zustimmung. Die Schranke schliesst
  nach 5 s. Antwortet die Kasse nicht, bleiben beide Schranken zu.

| Oberfläche | Adresse |
|------------|---------|
| Schranken | `http://192.168.42.1/` |
| Kasse | `http://192.168.42.2/` |
| Automat | `http://192.168.42.2/automat` |

## Verdrahtung Schranken-Pico

| Bauteil | Pico-Pin |
|---------|----------|
| Servo Einfahrt Signal | GP8 (Pin 11) |
| Servo Ausfahrt Signal | GP9 (Pin 12) |
| Servo VCC, beide | VBUS (Pin 40, 5 V) |
| Servo GND, beide | GND (z.B. Pin 8) |
| LED Einfahrt (optional) | GP14 (Pin 19) über 220 Ω an GND |
| LED Ausfahrt (optional) | GP15 (Pin 20) über 220 Ω an GND |

Die Servo-Arme so aufstecken, dass sie nach dem Start (90°) waagrecht stehen.

## Inbetriebnahme

1. In der Arduino IDE das Board-Paket „Raspberry Pi Pico/RP2040/RP2350“ installieren
   (Boardverwalter-URL:
   `https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json`).
2. `schranken/schranken.ino` mit Board „Raspberry Pi Pico W“ auf den Schranken-Pico laden.
3. `kasse/kasse.ino` auf den Kassen-Pico laden.
4. Notebook mit dem WLAN `Parksystem` verbinden.
5. Zuerst die Kasse öffnen (stellt die Uhr), dann Automat und Schranken.

## Tests

```sh
c++ -std=c++17 -Wall -o test_logic tests/test_logic.cpp && ./test_logic
```

Testet Kennzeichenprüfung und Gebührenberechnung.

```sh
c++ -std=c++17 -w -Itests/arduino_mock -o test_system tests/test_system.cpp && ./test_system
```

Systemtest: führt die Firmware beider Picos unverändert auf dem Notebook aus (WLAN,
Servo, LED und Uhr simuliert) und spielt TF-01 bis TF-32 aus dem Testkonzept in der
vorgeschriebenen Reihenfolge durch. Die Resultate und das Testprotokoll liegen in
`docs/testprotokoll/`.

## User Stories

| Story | Inhalt | Testfälle |
|-------|--------|-----------|
| US-1 | Einfahrt registrieren | TF-01–TF-04, TF-25, TF-30, TF-32 |
| US-2 | Fehlerhafte Einfahrt abweisen | TF-05–TF-10 |
| US-3 | Gebühr berechnen | TF-12–TF-17 |
| US-4 | Gebühr bezahlen an Kasse oder Automat | TF-18–TF-21 |
| US-5 | Ausfahrt nur bei bezahlter Gebühr | TF-22–TF-27, TF-31 |
| US-6 | Grund einer Abweisung anzeigen | TF-05–TF-08, TF-10, TF-11, TF-17, TF-24, TF-26, TF-27 |
| US-7 | Schranken bleiben zu ohne Kasse | TF-28, TF-29 |

Alle User Stories sind umgesetzt.

## Einschränkungen

- Daten liegen im Arbeitsspeicher. Nach einem Neustart der Kasse sind alle Fahrzeuge
  weg und die Kassen-Seite muss neu geöffnet werden, damit die Uhr wieder stimmt.
- Kein Zugriffsschutz im WLAN.
- Im WLAN `Parksystem` hat das Notebook kein Internet.
