#pragma once

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "config.h"
#include "kasse.h"

inline String urlEncode(const String& s) {
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

// Produktive Kasse: HTTP-Anfrage über WLAN an den Kassen-Pico.
class HttpKasse : public Kasse {
 public:
  KassenAntwort einfahrt(const std::string& kennzeichen, const std::string& browserZeit) override {
    return durchfahrt("/api/entry", kennzeichen, browserZeit);
  }

  KassenAntwort ausfahrt(const std::string& kennzeichen, const std::string& browserZeit) override {
    return durchfahrt("/api/exit", kennzeichen, browserZeit);
  }

  // GET- (form == nullptr) oder POST-Anfrage. Liefert den HTTP-Status oder <= 0 bei
  // Verbindungsfehler bzw. einer Antwort, die kein JSON ist.
  int anfrage(const String& path, const String* form, String& body) {
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

 private:
  KassenAntwort durchfahrt(const char* path, const std::string& kennzeichen,
                           const std::string& browserZeit) {
    String form = "plate=" + urlEncode(String(kennzeichen.c_str())) +
                  "&epoch=" + urlEncode(String(browserZeit.c_str()));
    String body;
    int code = anfrage(path, &form, body);
    if (code <= 0) Serial.printf("%s: Kasse nicht erreichbar (%d)\n", path, code);
    return {code, std::string(body.c_str())};
  }
};
