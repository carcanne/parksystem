#pragma once

#include <Arduino.h>
#include <hardware/clocks.h>
#include <hardware/pwm.h>

#include "config.h"
#include "schranke.h"

// Produktive Schranke: Servo an einem PWM-Pin und LED als Anzeige.
class ServoSchranke : public Schranke {
 public:
  static const int SERVO_MIN_US = 544;
  static const int SERVO_MAX_US = 2400;

  ServoSchranke(int servoPin, int ledPin) : servoPin_(servoPin), ledPin_(ledPin) {}

  void begin() {
    pinMode(ledPin_, OUTPUT);
    gpio_set_function(servoPin_, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(servoPin_);
    pwm_set_clkdiv(slice, clock_get_hz(clk_sys) / 1000000.0f);
    pwm_set_wrap(slice, 20000 - 1);
    pwm_set_enabled(slice, true);
  }

  void oeffnen() override {
    Serial.printf("Schranke GP%d auf\n", servoPin_);
    writeServo(ANGLE_OPEN);
    digitalWrite(ledPin_, HIGH);
  }

  void schliessen() override {
    Serial.printf("Schranke GP%d zu\n", servoPin_);
    writeServo(ANGLE_CLOSED);
    digitalWrite(ledPin_, LOW);
  }

 private:
  void writeServo(int angle) {
    pwm_set_gpio_level(servoPin_, map(angle, 0, 180, SERVO_MIN_US, SERVO_MAX_US));
  }

  int servoPin_;
  int ledPin_;
};
