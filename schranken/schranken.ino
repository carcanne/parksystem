// Parksystem - Pico 2 (Schranken)
//
// Steuert Einfahrts- und Ausfahrtsschranke (Servo + optionale LED) und stellt
// die Weboberflaeche bereit, ueber die die Kamera simuliert wird. Dieser Pico
// haelt keine Fahrzeugdaten: jede Einfahrt und Ausfahrt wird bei der Kasse
// angefragt. Antwortet die Kasse nicht, bleiben beide Schranken zu (US-7).

#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <Servo.h>

#include "config.h"
#include "schranken_page.h"

class Barrier {
 public:
  Barrier(int servoPin, int ledPin) : servoPin_(servoPin), ledPin_(ledPin) {}

  void begin() {
    pinMode(ledPin_, OUTPUT);
    servo_.attach(servoPin_);
    close();
  }

  void open() {
    servo_.write(ANGLE_OPEN);
    digitalWrite(ledPin_, HIGH);
    open_ = true;
    openedAt_ = millis();
  }

  void close() {
    servo_.write(ANGLE_CLOSED);
    digitalWrite(ledPin_, LOW);
    open_ = false;
  }

  // Schliesst die Schranke selbststaendig nach BARRIER_OPEN_MS
  void update() {
    if (open_ && millis() - openedAt_ >= BARRIER_OPEN_MS) close();
  }

  bool isOpen() const { return open_; }

 private:
  int servoPin_;
  int ledPin_;
  Servo servo_;
  bool open_ = false;
  unsigned long openedAt_ = 0;
};

Barrier entryBarrier(ENTRY_SERVO_PIN, ENTRY_LED_PIN);
Barrier exitBarrier(EXIT_SERVO_PIN, EXIT_LED_PIN);
WebServer server(80);

const char CONNECTION_ERROR[] =
    "{\"ok\":false,\"code\":\"CONNECTION\",\"message\":\"Verbindungsfehler: Die Kasse "
    "antwortet nicht. Die Schranke bleibt geschlossen, bitte Personal rufen.\"}";

String urlEncode(const String& s) {
  const char* hex = "0123456789ABCDEF";
  String out;
  for (unsigned int i = 0; i < s.length(); i++) {
    unsigned char c = s[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.') {
      out += (char)c;
    } else {
      out += '%';
      out += hex[c >> 4];
      out += hex[c & 0x0F];
    }
  }
  return out;
}

// Fragt bei der Kasse an. Liefert den HTTP-Status oder einen Wert <= 0, wenn
// die Kasse nicht erreichbar war oder keine gueltige Antwort geschickt hat.
int callKasse(const String& path, const String* form, String& body) {
  WiFiClient client;
  HTTPClient http;
  String url = String("http://") + KASSE_HOST + ":" + KASSE_PORT + path;
  if (!http.begin(client, url)) return -1;
  http.setTimeout(KASSE_TIMEOUT_MS);

  int code;
  if (form != nullptr) {
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    code = http.POST(*form);
  } else {
    code = http.GET();
  }
  if (code > 0) body = http.getString();
  http.end();

  if (code > 0 && !body.startsWith("{")) return -1;
  return code;
}

void sendJson(int status, const String& json) {
  server.send(status, "application/json; charset=utf-8", json);
}

// Einfahrt und Ausfahrt: Kasse entscheidet, die Schranke oeffnet nur bei "ok"
void handlePassage(Barrier& barrier, const char* kassePath) {
  String form = "plate=" + urlEncode(server.arg("plate"));
  String body;
  int code = callKasse(kassePath, &form, body);

  if (code <= 0) {
    Serial.printf("%s: Kasse nicht erreichbar (%d)\n", kassePath, code);
    sendJson(503, CONNECTION_ERROR);
    return;
  }
  if (code == 200 && body.indexOf("\"ok\":true") >= 0) {
    barrier.open();
  }
  sendJson(code, body);
}

void handleEntry() { handlePassage(entryBarrier, "/api/entry"); }
void handleExit() { handlePassage(exitBarrier, "/api/exit"); }

// Fahrzeugliste kommt immer von der Kasse
void handleVehicles() {
  String body;
  int code = callKasse("/api/vehicles", nullptr, body);
  if (code != 200) {
    sendJson(503, CONNECTION_ERROR);
    return;
  }
  sendJson(200, body);
}

void handleBarriers() {
  String j = "{\"entry\":";
  j += entryBarrier.isOpen() ? "true" : "false";
  j += ",\"exit\":";
  j += exitBarrier.isOpen() ? "true" : "false";
  j += "}";
  sendJson(200, j);
}

void handlePage() {
  server.send(200, "text/html; charset=utf-8", SCHRANKEN_PAGE);
}

void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Verbinde mit WLAN");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    entryBarrier.update();
    exitBarrier.update();
  }
  Serial.println();
  Serial.print("Schranken erreichbar unter http://");
  Serial.println(WiFi.localIP());
  digitalWrite(LED_BUILTIN, HIGH);
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  entryBarrier.begin();
  exitBarrier.begin();
  connectWifi();

  server.on("/", HTTP_GET, handlePage);
  server.on("/api/entry", HTTP_POST, handleEntry);
  server.on("/api/exit", HTTP_POST, handleExit);
  server.on("/api/vehicles", HTTP_GET, handleVehicles);
  server.on("/api/barriers", HTTP_GET, handleBarriers);
  server.onNotFound([]() { server.send(404, "text/plain; charset=utf-8", "Nicht gefunden"); });
  server.begin();
}

void loop() {
  server.handleClient();
  entryBarrier.update();
  exitBarrier.update();

  static unsigned long lastWifiCheck = 0;
  if (millis() - lastWifiCheck > 10000) {
    lastWifiCheck = millis();
    if (WiFi.status() != WL_CONNECTED) {
      digitalWrite(LED_BUILTIN, LOW);
      connectWifi();
    }
  }
}
