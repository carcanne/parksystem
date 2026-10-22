#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <SimpleMDNS.h>
#include <hardware/clocks.h>
#include <hardware/pwm.h>

#include "config.h"
#include "schranken_page.h"

const int SERVO_MIN_US = 544;
const int SERVO_MAX_US = 2400;

class Barrier {
 public:
  Barrier(int servoPin, int ledPin) : servoPin_(servoPin), ledPin_(ledPin) {}

  void begin() {
    pinMode(ledPin_, OUTPUT);
    gpio_set_function(servoPin_, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(servoPin_);
    pwm_set_clkdiv(slice, clock_get_hz(clk_sys) / 1000000.0f);
    pwm_set_wrap(slice, 20000 - 1);
    pwm_set_enabled(slice, true);
    close();
  }

  void open() {
    Serial.printf("Schranke GP%d auf\n", servoPin_);
    writeServo(servoPin_, ANGLE_OPEN);
    digitalWrite(ledPin_, HIGH);
    open_ = true;
    openedAt_ = millis();
  }

  void close() {
    Serial.printf("Schranke GP%d zu\n", servoPin_);
    writeServo(servoPin_, ANGLE_CLOSED);
    digitalWrite(ledPin_, LOW);
    open_ = false;
  }

  void update() {
    if (open_ && millis() - openedAt_ >= BARRIER_OPEN_MS) close();
  }

  bool isOpen() const { return open_; }

 private:
  static void writeServo(int pin, int angle) {
    pwm_set_gpio_level(pin, map(angle, 0, 180, SERVO_MIN_US, SERVO_MAX_US));
  }

  int servoPin_;
  int ledPin_;
  bool open_ = false;
  unsigned long openedAt_ = 0;
};

Barrier entryBarrier(ENTRY_SERVO_PIN, ENTRY_LED_PIN);
Barrier exitBarrier(EXIT_SERVO_PIN, EXIT_LED_PIN);
WebServer server(80);

void reply(int status, const char* type, const String& content) {
  server.send(status, type, content);
  WiFiClient& client = server.client();
  while (client.available()) client.read();
  client.stop();
}

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

int callKasse(const String& path, const String* form, String& body) {
  WiFiClient client;
  client.setTimeout(KASSE_TIMEOUT_MS);
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

  if (code > 0 && !body.startsWith("{")) return -1000 - code;
  return code;
}

void sendJson(int status, const String& json) {
  reply(status, "application/json; charset=utf-8", json);
}

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

void handleVehicles() {
  static unsigned long lastFailure = 0;
  static bool failed = false;
  if (failed && millis() - lastFailure < 10000) {
    sendJson(503, CONNECTION_ERROR);
    return;
  }
  String body;
  int code = callKasse("/api/vehicles", nullptr, body);
  failed = code != 200;
  if (failed) {
    lastFailure = millis();
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
  for (Barrier* b : {&entryBarrier, &exitBarrier}) {
    delay(1000);
    b->open();
    delay(1000);
    b->close();
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  entryBarrier.begin();
  exitBarrier.begin();
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
