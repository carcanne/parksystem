#include <WiFi.h>
#include <WebServer.h>
#include <SimpleMDNS.h>

#include <string>

#include "config.h"
#include "kasse_page.h"
#include "parkhaus.h"
#include "array_speicher.h"
#include "system_uhr.h"
#include "serial_log.h"

// Produktive Abhängigkeiten, über den Konstruktor an die Logik übergeben.
ArraySpeicher speicher;
SystemUhr uhr;
SerialLog logger;
Parkhaus parkhaus(speicher, uhr, logger);

WebServer server(80);

void reply(int status, const char* type, const String& content) {
  server.send(status, type, content);
  WiFiClient& client = server.client();
  while (client.available()) client.read();
  client.stop();
}

void sendJson(int status, const std::string& json) {
  reply(status, "application/json; charset=utf-8", String(json.c_str()));
}

void send(const Ergebnis& e) { sendJson(e.status, e.json()); }

std::string param(const char* name) { return std::string(server.arg(name).c_str()); }

// Die Handler übersetzen nur zwischen HTTP und der Logik in parkhaus.h.
void handleEntry() { send(parkhaus.einfahrt(param("plate"), param("epoch"))); }
void handleExit() { send(parkhaus.ausfahrt(param("plate"))); }
void handleFee() { send(parkhaus.gebuehr(param("plate"))); }
void handlePay() { send(parkhaus.bezahlen(param("plate"))); }
void handleTestTime() { send(parkhaus.testzeit(param("plate"), param("minutes"))); }
void handleTime() { send(parkhaus.uhrStellen(param("epoch"))); }
void handleVehicles() { sendJson(200, parkhaus.fahrzeugliste()); }

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
  server.onNotFound([]() { send({404, false, "NOT_FOUND", "Diese Seite gibt es nicht.", ""}); });
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
