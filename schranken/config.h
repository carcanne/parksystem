// Konfiguration Schranken-Pico (Pico 2)
#pragma once

#define WIFI_SSID     "DEIN_WLAN"
#define WIFI_PASSWORD "DEIN_PASSWORT"

// IP-Adresse des Kassen-Picos (wird beim Start im Serial Monitor der Kasse ausgegeben)
#define KASSE_HOST "192.168.1.50"
#define KASSE_PORT 80

// Wie lange auf die Kasse gewartet wird, bevor ein Verbindungsfehler gemeldet wird
#define KASSE_TIMEOUT_MS 3000

// Pins (siehe README). Servo-Signal an GP8 / GP9, LEDs optional an GP14 / GP15
#define ENTRY_SERVO_PIN 8
#define EXIT_SERVO_PIN  9
#define ENTRY_LED_PIN   14
#define EXIT_LED_PIN    15

// Servo-Winkel fuer geschlossene und offene Schranke
#define ANGLE_CLOSED 0
#define ANGLE_OPEN   90

// Schranke schliesst nach dieser Zeit selbststaendig (Anforderung: < 10 s)
#define BARRIER_OPEN_MS 5000
