#pragma once

class MDNSClass {
 public:
  bool begin(const char*) { return true; }
  void update() {}
};
inline MDNSClass MDNS;
