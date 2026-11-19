#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <SimpleMDNS.h>

#include <string>

#include "config.h"
#include "schranken_page.h"
#include "durchfahrt.h"
#include "servo_schranke.h"
#include "http_kasse.h"
#include "arduino_uhr.h"

// Produktive Abhängigkeiten, über den Konstruktor an die Logik übergeben.
ServoSchranke entryServo(ENTRY_SERVO_PIN, ENTRY_LED_PIN);
ServoSchranke exitServo(EXIT_SERVO_PIN, EXIT_LED_PIN);
HttpKasse httpKasse;
ArduinoUhr uhr;

Durchfahrt entryBarrier(Richtung::Einfahrt, entryServo, httpKasse, uhr);
Durchfahrt exitBarrier(Richtung::Ausfahrt, exitServo, httpKasse, uhr);

WebServer server(80);

void reply(int status, const char* type, const String& content) {
  server.send(status, type, content);
  WiFiClient& client = server.client();
  while (client.available()) client.read();
  client.stop();
}

void sendJson(int status, const String& json) {
  reply(status, "application/json; charset=utf-8", json);
}

// Simulierte Kamera: die Weboberfläche liefert das Kennzeichen, die Logik entscheidet.
void handlePassage(Durchfahrt& durchfahrt) {
  Antwort a = durchfahrt.fahrzeugErkannt(server.arg("plate").c_str(), server.arg("epoch").c_str());
  sendJson(a.status, String(a.json.c_str()));
}

void handleEntry() { handlePassage(entryBarrier); }
void handleExit() { handlePassage(exitBarrier); }

void handleVehicles() {
  static unsigned long lastFailure = 0;
  static bool failed = false;
  if (failed && millis() - lastFailure < 10000) {
    sendJson(503, VERBINDUNGSFEHLER);
    return;
  }
  String body;
  int code = httpKasse.anfrage("/api/vehicles", nullptr, body);
  failed = code != 200;
  if (failed) {
    lastFailure = millis();
    sendJson(503, VERBINDUNGSFEHLER);
    return;
  }
  sendJson(200, body);
}

void handleBarriers() {
  String j = "{\"entry\":";
  j += entryBarrier.istOffen() ? "true" : "false";
  j += ",\"exit\":";
  j += exitBarrier.istOffen() ? "true" : "false";
  j += "}";
  sendJson(200, j);
}

void handlePage() {
  reply(200, "text/html; charset=utf-8", SCHRANKEN_PAGE);
}

void printAddress() {
  Serial.print("WLAN \"" AP_SSID "\", Schranken erreichbar unter http://");
  Serial.println(WiFi.softAPIP());
  Serial.println("Kasse erwartet unter http://" KASSE_HOST);
}

void startAccessPoint() {
  WiFi.mode(WIFI_AP);
  WiFi.setHostname(HOSTNAME);
  IPAddress ip(AP_IP);
  WiFi.softAPConfig(ip, ip, IPAddress(255, 255, 255, 0));
  while (!WiFi.softAP(AP_SSID, AP_PASSWORD)) {
    Serial.println("WLAN konnte nicht geöffnet werden, neuer Versuch ...");
    delay(1000);
  }
  printAddress();
  digitalWrite(LED_BUILTIN, HIGH);
}

void selfTest() {
  for (Durchfahrt* b : {&entryBarrier, &exitBarrier}) {
    delay(1000);
    b->oeffnen();
    delay(1000);
    b->schliessen();
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  entryServo.begin();
  exitServo.begin();
  entryBarrier.schliessen();
  exitBarrier.schliessen();
  selfTest();
  startAccessPoint();
  MDNS.begin(HOSTNAME);

  server.on("/", HTTP_GET, handlePage);
  server.on("/api/entry", HTTP_POST, handleEntry);
  server.on("/api/exit", HTTP_POST, handleExit);
  server.on("/api/vehicles", HTTP_GET, handleVehicles);
  server.on("/api/barriers", HTTP_GET, handleBarriers);
  server.onNotFound([]() { reply(404, "text/plain; charset=utf-8", "Nicht gefunden"); });
  server.begin();
}

void loop() {
  server.handleClient();
  MDNS.update();
  entryBarrier.update();
  exitBarrier.update();

  static bool serialWasOpen = false;
  if (Serial && !serialWasOpen) printAddress();
  serialWasOpen = Serial;
}
