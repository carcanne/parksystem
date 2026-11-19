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
make test       # alle Unit-Tests mit Test Doubles + Systemtest (ohne Picos)
make unit       # nur Unit-Tests
make firmware   # beide Sketches für den Pico W kompilieren (arduino-cli)
```

| Testprogramm | Was wird getestet | Abhängigkeiten |
|--------------|-------------------|----------------|
| `tests/test_logic.cpp` | Kennzeichenprüfung, Gebühr, Nachzahlung (reine Funktionen) | keine |
| `tests/test_kasse.cpp` | Logik der Kasse (`kasse/parkhaus.h`) | Fake, Dummy, Spy |
| `tests/test_schranken.cpp` | Logik der Schranken (`schranken/durchfahrt.h`) | Mock, Stub, Fake |
| `tests/test_system.cpp` | Firmware beider Picos zusammen, TF-01 bis TF-32 in der Reihenfolge des Testkonzepts | Arduino-Attrappe in `tests/arduino_mock/` |

Der Systemtest stammt aus der Aufgabe Testprotokoll. Er läuft nach dem Umbau unverändert
mit 33 von 33 bestandenen Prüfungen und dient damit als Regressionstest für den Umbau.
Die Resultate und das Testprotokoll liegen in `docs/testprotokoll/`.

## Test Doubles

### Aufbau: Logik von der Hardware getrennt

Die Logik ruft keine Hardware-, Zeit- oder Netzwerkfunktionen mehr direkt auf. Sie bekommt
ihre Abhängigkeiten als Schnittstellen über den Konstruktor (Dependency Injection). Die
`.ino`-Dateien setzen nur noch die produktiven Klassen zusammen und übersetzen zwischen
HTTP und Logik.

```
Schranken-Pico                                    Kassen-Pico

Durchfahrt (durchfahrt.h)                         Parkhaus (parkhaus.h)
 + fahrzeugErkannt(kennzeichen, browserZeit)       + einfahrt / ausfahrt / gebuehr / bezahlen
 + update()                                        + testzeit / uhrStellen / fahrzeugliste
   |           |            |                        |                |            |
 «Schranke»  «Kasse»      «Uhr»                   «Fahrzeugspeicher» «Uhr»        «Log»
 oeffnen()   einfahrt()   jetzt(): ms             finden()           jetzt()      info()
 schliessen()ausfahrt()                           hinzufuegen()      stellen()
                                                  entfernen(), alle()

produktiv (src):                                  produktiv (src):
 ServoSchranke  HttpKasse  ArduinoUhr              ArraySpeicher  SystemUhr  SerialLog
Test (tests/doubles/schranken_doubles.h):         Test (tests/doubles/kasse_doubles.h):
 SchrankeMock   KasseStub  FakeUhr                 FakeSpeicher   FakeUhr    DummyLog
                                                   SpeicherSpy (umschliesst ArraySpeicher)
```

### Identifizierte Abhängigkeiten

| Abhängigkeit | Warum nicht im automatisierten Test | Was die Logik wirklich braucht | Test Double | Begründung |
|--------------|-------------------------------------|--------------------------------|-------------|------------|
| **Schranke** (Servo über PWM an GP8/GP9, LED) | Braucht den Pico und die Verdrahtung; ob der Servo dreht, sieht nur ein Mensch (aufwändig) | `oeffnen()`, `schliessen()` | **Mock** `SchrankeMock` | Die Schranke liefert nichts zurück. Geprüft wird nur, *ob* und *wie oft* die Logik sie öffnet oder schliesst, z.B. «öffnet genau einmal», «schliesst nicht vor 5 s». Dafür muss sich das Double die Aufrufe merken. |
| **Kasse aus Sicht der Schranken** (HTTP über WLAN an 192.168.42.2) | Braucht zwei Picos und das WLAN (langsam). Ein Verbindungsfehler lässt sich nur durch Ausstecken erzwingen (schwierig), Antwortzeiten schwanken (nicht deterministisch) | Antwort auf «darf Fahrzeug X ein-/ausfahren?»: HTTP-Status und JSON, oder Verbindungsfehler | **Stub** `KasseStub` | Die Schranken-Logik entscheidet nur anhand der Antwort. Der Stub liefert fest eingestellte Antworten (OK, UNPAID, UNKNOWN, Timeout …) und ignoriert die Parameter. So ist jeder Fall, auch der Verbindungsfehler, mit einer Zeile herstellbar. |
| **Zeit auf dem Schranken-Pico** (`millis()`) | Für «Schranke schliesst nach 5 s» müsste der Test echt 5 s warten (langsam); `millis()` läuft weiter, während der Test prüft (nicht deterministisch) | aktuelle Laufzeit in ms | **Fake** `FakeUhr` | Eine funktionierende, aber vereinfachte Uhr, die nur weiterläuft, wenn der Test sie vorstellt. Grenzwerte wie 4999 ms / 5000 ms sind damit exakt prüfbar. |
| **Uhrzeit der Kasse** (`time()`, `settimeofday()`) | Für 61 Minuten Parkzeit müsste man 61 Minuten warten oder das Testfeld benutzen (langsam). Der Zustand «Uhr nach Neustart nicht gestellt» lässt sich auf dem Notebook nicht herstellen (schwierig) | aktuelle Zeit lesen, Uhr stellen | **Fake** `FakeUhr` | Die Logik liest *und stellt* die Uhr (K-2). Ein Stub mit fester Zeit reicht deshalb nicht. Der Fake verhält sich wie eine echte Uhr, startet wie der Pico ungestellt (0) und lässt sich vorstellen. |
| **Speicherung der Fahrzeuge** (Array mit 50 Plätzen im RAM) | «Parkhaus voll» braucht 50 Einfahrten (aufwändig); der Zustand ist von aussen nicht einsehbar | finden, hinzufügen, entfernen, alle auflisten | **Fake** `FakeSpeicher`, zusätzlich **Spy** `SpeicherSpy` | Fake: ein `std::vector` mit frei wählbarer Kapazität ist eine vereinfachte, aber echte Implementierung; der Test kann den Inhalt direkt prüfen. Spy: umschliesst den *produktiven* `ArraySpeicher` und zeichnet die Aufrufe auf. Damit prüfen wir das Zusammenspiel Parkhaus + echte Speicherung (Integrationstest), z.B. «abgewiesene Einfahrt schreibt nichts», «voll nach genau 50». |
| **Serielle Ausgabe** (`Serial.printf`) | Braucht eine USB-Verbindung; für das Ergebnis eines Testfalls irrelevant | `info(text)` | **Dummy** `DummyLog` | Der Konstruktor verlangt ein Log, kein Testfall prüft es. Ein Platzhalter, der nichts tut, genügt. |
| **Kamera** bzw. deren Simulation über die Weboberfläche | Browser-Bedienung lässt sich in C++ nicht automatisieren | nur das erkannte Kennzeichen | **keines nötig** | Die Weboberfläche bleibt im `.ino`-Handler. Die Logik bekommt das Kennzeichen als Parameter `fahrzeugErkannt(kennzeichen, …)`, der Test übergibt es direkt. |

### Automatisierte Testfälle mit Bezug zum Testkonzept

Spalten: **K** = `test_kasse`, **S** = `test_schranken` (beide mit Test Doubles),
**Sys** = Systemtest. Ein Stern (*) markiert die Sternfälle aus dem Testkonzept.

| TF | Inhalt | K | S | Sys | Doubles im Unit-Test |
|----|--------|:-:|:-:|:---:|----------------------|
| TF-01* | AG 123456 einfahren, registriert, Schranke öffnet | ✔ | ✔ | ✔ | FakeUhr, FakeSpeicher / SchrankeMock, KasseStub |
| TF-02 | Einfahrtsschranke schliesst nach 5 s | | ✔ | ✔ | FakeUhr, SchrankeMock |
| TF-03* | Einfahrtszeit = Uhr der Kasse | ✔ | | ✔ | FakeUhr |
| TF-04* | Zweites Fahrzeug mit eigener Zeit | ✔ | ✔ | ✔ | FakeUhr, FakeSpeicher |
| TF-05*–TF-08* | Ungültige Kennzeichen abgewiesen, Meldung | ✔ | ✔ | ✔ | FakeSpeicher, SpeicherSpy / KasseStub, SchrankeMock |
| TF-09* | ZG 7 (Grenzwert) einfahren | ✔ | ✔ | ✔ | FakeSpeicher / KasseStub, SchrankeMock |
| TF-10 | «␣␣ag␣␣␣123456␣» → bereits im Parkhaus | ✔ | ✔ | ✔ | FakeSpeicher, SpeicherSpy |
| TF-11 | Meldung bleibt 10 s stehen | | | (✔) | – (nur Quelltext der Seite, siehe Fazit) |
| TF-12*–TF-16* | Gebühr 0/40/60/61/121 Min. | ✔ | | ✔ | FakeUhr (vorstellen statt warten), zusätzlich über das Testfeld |
| TF-17 | Unbekanntes Fahrzeug an der Kasse | ✔ | | ✔ | FakeSpeicher |
| TF-18* | Bezahlen CHF 6.00 | ✔ | | ✔ | FakeUhr, FakeSpeicher |
| TF-19, TF-20 | Bezahlt, keine zweite Zahlung (Kasse/Automat) | ✔ | | ✔ | FakeUhr, FakeSpeicher |
| TF-21* | Am Automaten bezahlen | ✔ | | ✔ | FakeSpeicher |
| TF-22* | Bezahlt ausfahren, Schranke öffnet | ✔ | ✔ | ✔ | FakeSpeicher / KasseStub, SchrankeMock |
| TF-23 | Ausfahrtsschranke schliesst nach 5 s | | ✔ | ✔ | FakeUhr, SchrankeMock |
| TF-24*, TF-27* | Unbekanntes Fahrzeug an der Ausfahrt | ✔ | ✔ | ✔ | FakeSpeicher / KasseStub, SchrankeMock |
| TF-25* | Erneute Einfahrt: neuer Aufenthalt | ✔ | | ✔ | FakeUhr, FakeSpeicher |
| TF-26* | Ohne Zahlung ausfahren: abgewiesen, CHF 2.00 | ✔ | ✔ | ✔ | FakeSpeicher, SpeicherSpy / KasseStub |
| TF-28* | Kasse weg (Ausfahrt): Verbindungsfehler, unterscheidbar | | ✔ | ✔ | KasseStub (Timeout), SchrankeMock |
| TF-29* | Kasse weg (Einfahrt): Verbindungsfehler | | ✔ | ✔ | KasseStub (Verbindung abgelehnt), SchrankeMock |
| TF-30 | Neustart der Kasse: Daten verloren | ✔ | | ✔ | neuer FakeSpeicher |
| TF-31* | Nachzahlung nach weiterer Stunde | ✔ | ✔ | ✔ | FakeUhr, FakeSpeicher |
| TF-32* | Einfahrt nach Neustart stellt die Uhr | ✔ | ✔ | ✔ | FakeUhr (ungestellt) |
| Ergänzung | Parkhaus voll (Fake mit 2 Plätzen, Spy mit echtem Array) | ✔ | ✔ | | FakeSpeicher, SpeicherSpy |
| Ergänzung | Laufende Uhr wird nicht verstellt (K-2), ungültige Zeit | ✔ | | | FakeUhr |
| Ergänzung | Antwort ohne JSON / ohne Zustimmung öffnet nicht | | ✔ | | KasseStub |

Total: 34 Testfälle in `test_kasse`, 17 in `test_schranken`, alle bestanden. Als
Gegenprobe haben wir absichtlich Fehler eingebaut (Schranke schliesst bei `>` statt `>=`
5000 ms, Ausfahrt ohne Bezahlprüfung). Die Tests schlagen dann wie erwartet fehl.

### Fazit: Was sich nicht automatisieren lässt

Die **Logik** aller 32 Testfälle ist automatisiert und läuft in unter einer Sekunde ohne
Picos. Durch die Test Doubles hängt kein Testfall mehr von Wartezeiten, vom WLAN oder vom
Ausstecken eines Picos ab. Nicht automatisieren lassen sich die Teile, die genau das
prüfen, was wir durch Doubles ersetzt haben, also die echte Hardware und die Oberfläche:

- **TF-01, TF-04, TF-09, TF-22: Schranke öffnet sichtbar.** Der Mock beweist nur, dass die
  Logik `oeffnen()` aufruft. Ob der Servo richtig verdrahtet ist, in die richtige Richtung
  dreht und die LED leuchtet, sieht man nur am Gerät.
- **TF-02, TF-23: Schliessen innerhalb 10 s mit der Stoppuhr.** Die FakeUhr prüft die 5 s
  der Logik exakt, nicht aber die echte `millis()`-Uhr und die Bewegungszeit des Servos.
- **TF-11: Meldung bleibt am Bildschirm stehen.** Das ist JavaScript im Browser, nicht
  C++-Logik. Der Systemtest prüft nur ersatzweise den Quelltext der Seite.
- **TF-20: Automat sperrt den Bezahlknopf.** Die Logik (keine zweite Zahlung) ist
  getestet, die Darstellung im Browser nicht.
- **TF-28, TF-29: Kassen-Pico ausstecken.** Der Stub liefert den Fehlercode eines
  Timeouts. Ob `HTTPClient` im echten WLAN nach 2 s abbricht und die gelbe Meldung
  innerhalb 10 s erscheint, zeigt nur der Versuch an der Hardware.
- **TF-30, TF-32: echter Neustart.** Im Test entspricht ein neuer, leerer Speicher bzw.
  eine ungestellte FakeUhr dem Neustart. Dass der Pico wirklich alles verliert und die
  Browser-Zeit über die Schranken-Seite ankommt, prüft der Durchlauf auf den Picos.
- **Eingangskriterien und TF-03 gegen die Notebook-Uhr:** Die Übernahme der Uhrzeit aus
  dem Browser (JavaScript `Date.now()`) ist nicht Teil der C++-Logik.

Auch die produktiven Adapter `ServoSchranke`, `HttpKasse`, `SystemUhr` und `SerialLog`
sind in den Unit-Tests bewusst nicht enthalten. Sie werden im Systemtest gegen die
Arduino-Attrappe ausgeführt und am Ende auf den Picos geprüft. Ein kurzer Durchlauf der
oben genannten Punkte auf der Hardware (Anhang B des Testprotokolls) bleibt deshalb nötig.

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
