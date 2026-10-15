// Parksystem - Pico 1 (Kasse)
//
// Verwaltet Kennzeichen, Einfahrtszeiten und Bezahlstatus im Arbeitsspeicher
// und stellt die Weboberflaeche fuer Kasse (/) und Automat (/automat) bereit.
// Der Schranken-Pico fragt hier ueber /api/entry und /api/exit an.

#include <WiFi.h>
#include <WebServer.h>
#include <time.h>

#include "config.h"
#include "parking_logic.h"
#include "kasse_page.h"

struct Vehicle {
  bool used;
  char plate[PLATE_LEN];
  time_t entry;
  bool paid;
  long paidRappen;
};

Vehicle vehicles[MAX_VEHICLES];
WebServer server(80);

// ---------------------------------------------------------------------------
// Fahrzeugdaten
// ---------------------------------------------------------------------------

Vehicle* findVehicle(const char* plate) {
  for (int i = 0; i < MAX_VEHICLES; i++) {
    if (vehicles[i].used && strcmp(vehicles[i].plate, plate) == 0) return &vehicles[i];
  }
  return nullptr;
}

Vehicle* freeSlot() {
  for (int i = 0; i < MAX_VEHICLES; i++) {
    if (!vehicles[i].used) return &vehicles[i];
  }
  return nullptr;
}

bool clockValid() {
  return time(nullptr) > 1700000000;  // NTP hat die Uhr gestellt
}

String formatTime(time_t t) {
  struct tm local;
  localtime_r(&t, &local);
  char buf[24];
  strftime(buf, sizeof(buf), "%d.%m.%Y %H:%M:%S", &local);
  return String(buf);
}

String chf(long rappen) {
  char buf[20];
  formatChf(rappen, buf, sizeof(buf));
  return String(buf);
}

long minutesOf(const Vehicle& v) {
  return parkedMinutes(v.entry, time(nullptr));
}

long openRappen(const Vehicle& v) {
  return v.paid ? 0 : feeRappen(minutesOf(v));
}

String vehicleJson(const Vehicle& v) {
  long minutes = minutesOf(v);
  String j = "{\"plate\":\"";
  j += v.plate;
  j += "\",\"entry\":\"";
  j += formatTime(v.entry);
  j += "\",\"minutes\":";
  j += minutes;
  j += ",\"fee\":\"";
  j += chf(feeRappen(minutes));
  j += "\",\"paid\":";
  j += v.paid ? "true" : "false";
  j += ",\"paidAmount\":\"";
  j += chf(v.paidRappen);
  j += "\",\"open\":\"";
  j += chf(openRappen(v));
  j += "\"}";
  return j;
}

// ---------------------------------------------------------------------------
// HTTP-Hilfsfunktionen
// ---------------------------------------------------------------------------

// Alle Antworten haben die Form {"ok":..,"code":"..","message":"..", ...}.
// message ist immer ein ganzer Satz fuer die Anzeige (US-6).
void sendResult(int status, bool ok, const char* code, const String& message,
                const String& extra = "") {
  String j = "{\"ok\":";
  j += ok ? "true" : "false";
  j += ",\"code\":\"";
  j += code;
  j += "\",\"message\":\"";
  j += message;
  j += "\"";
  if (extra.length() > 0) {
    j += ",";
    j += extra;
  }
  j += "}";
  server.send(status, "application/json; charset=utf-8", j);
}

// Liest das Argument "plate" und vereinheitlicht es. Bei einem ungueltigen
// Kennzeichen wird direkt die Fehlerantwort gesendet und false geliefert.
bool readPlate(char* plate) {
  String raw = server.arg("plate");
  switch (normalizePlate(raw.c_str(), plate, PLATE_LEN)) {
    case PLATE_OK:
      return true;
    case PLATE_EMPTY:
      sendResult(400, false, "EMPTY", "Das Kennzeichen fehlt. Bitte ein Kennzeichen eingeben.");
      return false;
    case PLATE_NO_LETTERS:
      sendResult(400, false, "NO_LETTERS",
                 "Das Kennzeichen enthält keine Buchstaben. Erwartet werden zwei Buchstaben "
                 "und ein bis sechs Ziffern, z.B. AG 123456.");
      return false;
    default:
      sendResult(400, false, "FORMAT",
                 "Das Kennzeichen hat ein ungültiges Format. Erwartet werden zwei Buchstaben "
                 "und ein bis sechs Ziffern, z.B. AG 123456.");
      return false;
  }
}

Vehicle* readKnownVehicle(char* plate) {
  if (!readPlate(plate)) return nullptr;
  Vehicle* v = findVehicle(plate);
  if (v == nullptr) {
    sendResult(404, false, "UNKNOWN",
               String("Das Fahrzeug ") + plate + " ist unbekannt, es steht nicht im Parkhaus.");
  }
  return v;
}

// ---------------------------------------------------------------------------
// API fuer den Schranken-Pico
// ---------------------------------------------------------------------------

// US-1, US-2: Einfahrt registrieren oder abweisen
void handleEntry() {
  char plate[PLATE_LEN];
  if (!readPlate(plate)) return;

  if (!clockValid()) {
    sendResult(503, false, "NO_TIME",
               "Die Uhrzeit der Kasse ist nicht synchronisiert, eine Einfahrt ist nicht möglich.");
    return;
  }
  if (findVehicle(plate) != nullptr) {
    sendResult(409, false, "DUPLICATE",
               String("Das Fahrzeug ") + plate + " steht bereits im Parkhaus.");
    return;
  }
  Vehicle* v = freeSlot();
  if (v == nullptr) {
    sendResult(409, false, "FULL", "Das Parkhaus ist voll, bitte später wiederkommen.");
    return;
  }

  // Neuer Aufenthalt: ein frueherer Bezahlstatus gilt nicht mehr (TF-25)
  v->used = true;
  strncpy(v->plate, plate, PLATE_LEN);
  v->entry = time(nullptr);
  v->paid = false;
  v->paidRappen = 0;

  Serial.printf("Einfahrt %s um %s\n", v->plate, formatTime(v->entry).c_str());
  sendResult(200, true, "OK",
             String("Willkommen! ") + plate + " ist um " + formatTime(v->entry) + " eingefahren.",
             "\"vehicle\":" + vehicleJson(*v));
}

// US-5: Ausfahrt nur bei bezahlter Gebuehr
void handleExit() {
  char plate[PLATE_LEN];
  Vehicle* v = readKnownVehicle(plate);
  if (v == nullptr) return;

  if (!v->paid) {
    String open = chf(openRappen(*v));
    sendResult(402, false, "UNPAID",
               "Die Parkgebühr von " + open + " für " + plate +
                   " ist noch offen. Bitte an der Kasse oder am Automaten bezahlen.",
               "\"open\":\"" + open + "\"");
    return;
  }

  Serial.printf("Ausfahrt %s\n", v->plate);
  v->used = false;
  sendResult(200, true, "OK", String("Gute Fahrt! ") + plate + " hat das Parkhaus verlassen.");
}

// Fahrzeugliste fuer Kasse und Schranken-Oberflaeche
void handleVehicles() {
  String j = "{\"ok\":true,\"now\":\"";
  j += clockValid() ? formatTime(time(nullptr)) : String("nicht synchronisiert");
  j += "\",\"vehicles\":[";
  bool first = true;
  for (int i = 0; i < MAX_VEHICLES; i++) {
    if (!vehicles[i].used) continue;
    if (!first) j += ",";
    j += vehicleJson(vehicles[i]);
    first = false;
  }
  j += "]}";
  server.send(200, "application/json; charset=utf-8", j);
}

// ---------------------------------------------------------------------------
// API fuer Kasse und Automat
// ---------------------------------------------------------------------------

// US-3: Gebuehr abfragen
void handleFee() {
  char plate[PLATE_LEN];
  Vehicle* v = readKnownVehicle(plate);
  if (v == nullptr) return;

  String msg = v->paid
                   ? String("Die Gebühr für ") + plate + " ist bezahlt (" + chf(v->paidRappen) +
                         "), offener Betrag CHF 0.00."
                   : String("Offener Betrag für ") + plate + ": " + chf(openRappen(*v)) + ".";
  sendResult(200, true, "OK", msg, "\"vehicle\":" + vehicleJson(*v));
}

// US-4: Gebuehr bezahlen
void handlePay() {
  char plate[PLATE_LEN];
  Vehicle* v = readKnownVehicle(plate);
  if (v == nullptr) return;

  if (v->paid) {
    sendResult(409, false, "ALREADY_PAID",
               String("Die Gebühr für ") + plate + " wurde bereits bezahlt (" +
                   chf(v->paidRappen) + "), eine zweite Zahlung ist nicht möglich.",
               "\"vehicle\":" + vehicleJson(*v));
    return;
  }

  v->paidRappen = openRappen(*v);
  v->paid = true;
  Serial.printf("Zahlung %s: %s\n", v->plate, chf(v->paidRappen).c_str());
  sendResult(200, true, "OK",
             "Zahlung über " + chf(v->paidRappen) + " für " + plate + " bestätigt, Status bezahlt.",
             "\"vehicle\":" + vehicleJson(*v));
}

// Testfeld: Einfahrtszeit auf "vor N Minuten" setzen (TF-12 bis TF-16)
void handleTestTime() {
  char plate[PLATE_LEN];
  Vehicle* v = readKnownVehicle(plate);
  if (v == nullptr) return;

  String raw = server.arg("minutes");
  raw.trim();
  bool digitsOnly = raw.length() > 0 && raw.length() <= 6;
  for (unsigned int i = 0; i < raw.length(); i++) {
    if (!isDigit(raw[i])) digitsOnly = false;
  }
  if (!digitsOnly) {
    sendResult(400, false, "MINUTES",
               "Bitte die Minuten als ganze Zahl zwischen 0 und 999999 eingeben.");
    return;
  }

  long minutes = raw.toInt();
  v->entry = time(nullptr) - (time_t)minutes * 60;
  sendResult(200, true, "OK",
             String("Einfahrtszeit von ") + plate + " auf vor " + minutes +
                 " Minuten gesetzt, Gebühr " + chf(openRappen(*v)) + ".",
             "\"vehicle\":" + vehicleJson(*v));
}

void handlePage() {
  server.send(200, "text/html; charset=utf-8", KASSE_PAGE);
}

// ---------------------------------------------------------------------------

void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Verbinde mit WLAN");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Kasse erreichbar unter http://");
  Serial.println(WiFi.localIP());
  digitalWrite(LED_BUILTIN, HIGH);
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  connectWifi();

  setenv("TZ", TIMEZONE, 1);
  tzset();
  NTP.begin("pool.ntp.org", "time.nist.gov");
  if (NTP.waitSet(15000)) {
    Serial.printf("Uhrzeit: %s\n", formatTime(time(nullptr)).c_str());
  } else {
    Serial.println("Warnung: NTP-Zeit noch nicht gesetzt, Einfahrten werden abgewiesen.");
  }

  server.on("/", HTTP_GET, handlePage);
  server.on("/automat", HTTP_GET, handlePage);
  server.on("/api/vehicles", HTTP_GET, handleVehicles);
  server.on("/api/fee", HTTP_GET, handleFee);
  server.on("/api/pay", HTTP_POST, handlePay);
  server.on("/api/testtime", HTTP_POST, handleTestTime);
  server.on("/api/entry", HTTP_POST, handleEntry);
  server.on("/api/exit", HTTP_POST, handleExit);
  server.onNotFound([]() { sendResult(404, false, "NOT_FOUND", "Diese Seite gibt es nicht."); });
  server.begin();
}

void loop() {
  server.handleClient();

  static unsigned long lastWifiCheck = 0;
  if (millis() - lastWifiCheck > 10000) {
    lastWifiCheck = millis();
    if (WiFi.status() != WL_CONNECTED) {
      digitalWrite(LED_BUILTIN, LOW);
      connectWifi();
    }
  }
}
