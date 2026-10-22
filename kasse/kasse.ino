#include <WiFi.h>
#include <WebServer.h>
#include <SimpleMDNS.h>
#include <sys/time.h>
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

void reply(int status, const char* type, const String& content) {
  server.send(status, type, content);
  WiFiClient& client = server.client();
  while (client.available()) client.read();
  client.stop();
}

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
  return time(nullptr) > 1700000000;
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
  reply(status, "application/json; charset=utf-8", j);
}

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

void handleEntry() {
  char plate[PLATE_LEN];
  if (!readPlate(plate)) return;

  if (!clockValid()) {
    sendResult(503, false, "NO_TIME",
               "Die Uhrzeit der Kasse ist nicht gestellt, eine Einfahrt ist nicht möglich. "
               "Bitte die Kassen-Seite einmal öffnen.");
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
  reply(200, "application/json; charset=utf-8", j);
}

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

void handleTime() {
  long long epoch = atoll(server.arg("epoch").c_str());
  if (epoch < 1700000000LL) {
    sendResult(400, false, "TIME", "Die übermittelte Uhrzeit ist ungültig.");
    return;
  }
  struct timeval tv = {(time_t)epoch, 0};
  settimeofday(&tv, nullptr);
  sendResult(200, true, "OK", "Uhrzeit gestellt auf " + formatTime(time(nullptr)) + ".");
}

void handlePage() {
  reply(200, "text/html; charset=utf-8", KASSE_PAGE);
}

void printAddress() {
  Serial.print("Kasse erreichbar unter http://");
  Serial.println(WiFi.localIP());
}

void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);
  IPAddress gateway(GATEWAY_IP);
  WiFi.config(IPAddress(KASSE_IP), gateway, gateway, IPAddress(255, 255, 255, 0));
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Verbinde mit WLAN " WIFI_SSID);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  printAddress();
  digitalWrite(LED_BUILTIN, HIGH);
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  connectWifi();
  MDNS.begin(HOSTNAME);

  setenv("TZ", TIMEZONE, 1);
  tzset();

  server.on("/", HTTP_GET, handlePage);
  server.on("/automat", HTTP_GET, handlePage);
  server.on("/api/vehicles", HTTP_GET, handleVehicles);
  server.on("/api/fee", HTTP_GET, handleFee);
  server.on("/api/pay", HTTP_POST, handlePay);
  server.on("/api/testtime", HTTP_POST, handleTestTime);
  server.on("/api/time", HTTP_POST, handleTime);
  server.on("/api/entry", HTTP_POST, handleEntry);
  server.on("/api/exit", HTTP_POST, handleExit);
  server.onNotFound([]() { sendResult(404, false, "NOT_FOUND", "Diese Seite gibt es nicht."); });
  server.begin();
}

void loop() {
  server.handleClient();
  MDNS.update();

  static bool serialWasOpen = false;
  if (Serial && !serialWasOpen) printAddress();
  serialWasOpen = Serial;

  static unsigned long lastWifiCheck = 0;
  static unsigned long offlineSince = 0;
  if (millis() - lastWifiCheck > 5000) {
    lastWifiCheck = millis();
    if (WiFi.status() == WL_CONNECTED) {
      offlineSince = 0;
    } else if (offlineSince == 0) {
      offlineSince = millis();
      Serial.printf("WLAN getrennt (Status %d)\n", WiFi.status());
    } else if (millis() - offlineSince > 30000) {
      Serial.println("WLAN seit 30 s weg, verbinde neu");
      digitalWrite(LED_BUILTIN, LOW);
      connectWifi();
      offlineSince = 0;
    }
  }
}
