#pragma once

#include "WebServer.h"

// Leitet die Anfragen des Schranken-Picos direkt an den WebServer der Kasse weiter.
// Ist die Kasse "vom Strom getrennt", läuft die Anfrage in den Timeout.
namespace sim {
inline WebServer*& kasseServer() { static WebServer* s = nullptr; return s; }
inline bool& kasseOnline() { static bool v = true; return v; }
}  // namespace sim

#define HTTPC_ERROR_CONNECTION_REFUSED -1
#define HTTPC_ERROR_READ_TIMEOUT -11

class HTTPClient {
 public:
  bool begin(WiFiClient& client, const String& url) {
    client_ = &client;
    std::string u = url.str();
    size_t p = u.find('/', strlen("http://"));
    path_ = p == std::string::npos ? "/" : u.substr(p);
    return true;
  }
  void setTimeout(unsigned long ms) { timeout_ = ms; }
  void addHeader(const char*, const char*) {}
  int POST(const String& form) { return call(HTTP_POST, form.str()); }
  int GET() { return call(HTTP_GET, ""); }
  String getString() { return String(body_); }
  void end() {}

 private:
  int call(HTTPMethod method, const std::string& form) {
    if (!sim::kasseOnline() || sim::kasseServer() == nullptr) {
      unsigned long wait = client_ ? client_->timeout() : timeout_;
      if (timeout_ > wait) wait = timeout_;
      delay(wait);
      return HTTPC_ERROR_READ_TIMEOUT;
    }
    std::string path = path_;
    std::string query;
    size_t q = path.find('?');
    if (q != std::string::npos) {
      query = path.substr(q + 1);
      path = path.substr(0, q);
    }
    HttpResponse r = sim::kasseServer()->request(method, path, method == HTTP_GET ? query : form);
    body_ = r.body;
    return r.code;
  }

  WiFiClient* client_ = nullptr;
  unsigned long timeout_ = 5000;
  std::string path_;
  std::string body_;
};
