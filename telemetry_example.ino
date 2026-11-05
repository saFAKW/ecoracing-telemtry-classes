// Example sketch: how to use the classes on the ESP32
// Put SensorReading.h and SensorReading.cpp in the same folder as this file
#include "SensorReading.h"

const int TEMP_PIN = 34;   // TCRT5000 analog out (change to whatever you actually wired it to!)
const int POT_PIN  = 35;   // potentiometer
const int BTN_PIN  = 32;   // emergency stop button (other leg goes to GND)

StatusMonitor monitor;
unsigned long lastSend = 0;
const unsigned long PERIOD_MS = 200;   // 200 ms between readings = 5 per second (5 Hz)

void setup() {
    Serial.begin(115200);
    pinMode(BTN_PIN, INPUT_PULLUP);   // pullup means the pin reads LOW when the button is pressed
}

void loop() {
    // not using delay() so the board isn't stuck waiting. Just skip until enough time has passed
    if (millis() - lastSend < PERIOD_MS) return;
    lastSend = millis();

    // == LOW because of the pullup, so LOW means "pressed"
    SensorReading r(analogRead(TEMP_PIN), analogRead(POT_PIN),
                    digitalRead(BTN_PIN) == LOW, millis(), 4095);
    monitor.apply(r);   // works out the status flags + error code

    char line[96];
    if (r.toLine(line, sizeof(line))) {
        Serial.println(line);   // swap this for the HM-10 serial once Bluetooth works
    }
}
