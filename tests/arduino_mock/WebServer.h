#pragma once

#include "WiFi.h"

enum HTTPMethod { HTTP_GET, HTTP_POST };

struct HttpResponse {
  int code = 0;
  std::string type;
  std::string body;
};

// WebServer, dessen Handler im Test direkt über request() aufgerufen werden.
class WebServer {
 public:
  explicit WebServer(int) {}

  void on(const char* path, HTTPMethod method, std::function<void()> handler) {
    routes_[key(method, path)] = handler;
  }
  void onNotFound(std::function<void()> handler) { notFound_ = handler; }
  void begin() {}
  void handleClient() {}

  String arg(const char* name) {
    auto it = args_.find(name);
    return it == args_.end() ? String() : String(it->second);
  }
  void send(int code, const char* type, const String& content) {
    response_.code = code;
    response_.type = type;
    response_.body = content.str();
  }
  WiFiClient& client() { return client_; }

  HttpResponse request(HTTPMethod method, const std::string& path, const std::string& form) {
    args_ = parseForm(form);
    response_ = HttpResponse();
    auto it = routes_.find(key(method, path));
    if (it != routes_.end()) {
      it->second();
    } else if (notFound_) {
      notFound_();
    }
    return response_;
  }

 private:
  static std::string key(HTTPMethod m, const std::string& path) {
    return (m == HTTP_GET ? "GET " : "POST ") + path;
  }

  static std::map<std::string, std::string> parseForm(const std::string& form) {
    std::map<std::string, std::string> out;
    size_t start = 0;
    while (start < form.size()) {
      size_t end = form.find('&', start);
      if (end == std::string::npos) end = form.size();
      std::string pair = form.substr(start, end - start);
      size_t eq = pair.find('=');
      if (eq != std::string::npos) out[decode(pair.substr(0, eq))] = decode(pair.substr(eq + 1));
      start = end + 1;
    }
    return out;
  }

  static std::string decode(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
      if (s[i] == '+') {
        out += ' ';
      } else if (s[i] == '%' && i + 2 < s.size()) {
        out += (char)strtol(s.substr(i + 1, 2).c_str(), nullptr, 16);
        i += 2;
      } else {
        out += s[i];
      }
    }
    return out;
  }

  std::map<std::string, std::function<void()>> routes_;
  std::function<void()> notFound_;
  std::map<std::string, std::string> args_;
  HttpResponse response_;
  WiFiClient client_;
};
