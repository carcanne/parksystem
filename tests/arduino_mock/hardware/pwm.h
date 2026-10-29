#pragma once

#include "../Arduino.h"

#define GPIO_FUNC_PWM 4
inline void gpio_set_function(int, int) {}
inline uint pwm_gpio_to_slice_num(int pin) { return (uint)(pin / 2); }
inline void pwm_set_clkdiv(uint, float) {}
inline void pwm_set_wrap(uint, unsigned) {}
inline void pwm_set_enabled(uint, bool) {}
inline void pwm_set_gpio_level(int pin, unsigned level) { sim::pwm()[pin] = level; }
