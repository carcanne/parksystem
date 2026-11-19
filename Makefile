# Automatisierte Tests auf dem Notebook (ohne Picos) und Build für den Pico W.
#   make test      Unit-Tests mit Test Doubles + Systemtest
#   make unit      nur Unit-Tests
#   make firmware  beide Sketches für den Raspberry Pi Pico W kompilieren (arduino-cli)

CXX      ?= c++
CXXFLAGS := -std=c++17 -Wall -Wextra
BUILD    := build
FQBN     := rp2040:rp2040:rpipicow

UNIT := $(BUILD)/test_logic $(BUILD)/test_kasse $(BUILD)/test_schranken

.PHONY: test unit system firmware clean

test: unit system

unit: $(UNIT)
	@for t in $(UNIT); do echo "== $$t"; ./$$t || exit 1; done

system: $(BUILD)/test_system
	@echo "== $<"; ./$<

$(BUILD)/test_logic: tests/test_logic.cpp kasse/parking_logic.h | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ $<

$(BUILD)/test_kasse: tests/test_kasse.cpp tests/testing.h tests/doubles/kasse_doubles.h kasse/*.h | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ $<

$(BUILD)/test_schranken: tests/test_schranken.cpp tests/testing.h tests/doubles/schranken_doubles.h schranken/*.h | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ $<

# Der Systemtest kompiliert die komplette Firmware gegen die Arduino-Attrappe in tests/arduino_mock.
$(BUILD)/test_system: tests/test_system.cpp tests/arduino_mock/*.h kasse/* schranken/* | $(BUILD)
	$(CXX) -std=c++17 -w -Itests/arduino_mock -o $@ $<

firmware:
	arduino-cli compile --fqbn $(FQBN) kasse
	arduino-cli compile --fqbn $(FQBN) schranken

$(BUILD):
	mkdir -p $@

clean:
	rm -rf $(BUILD)
