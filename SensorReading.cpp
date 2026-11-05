// SensorReading.cpp - the actual code for the classes in the .h file
#include "SensorReading.h"
#include <stdio.h>    // snprintf lives here
#include <string.h>   // strlen, strcpy etc

// ---- Temperature ----
Temperature Temperature::fromRaw(int raw, int adcMax, float minC, float maxC) {
    if (adcMax <= 0) return Temperature(minC);   // avoid dividing by zero (that would be bad)
    if (raw < 0) raw = 0;                        // clamp so weird numbers can't break it
    if (raw > adcMax) raw = adcMax;
    // the (float) bits are so it does decimal maths, not integer maths (learned that the hard way)
    return Temperature(minC + (maxC - minC) * (float)raw / (float)adcMax);
}

float Temperature::in(TempUnit u) const {
    switch (u) {   // switch = fancy if/else for picking between options
        case TempUnit::Fahrenheit: return toFahrenheit();
        case TempUnit::Kelvin:     return toKelvin();
        default:                   return toCelsius();   // Celsius or anything else
    }
}

// ---- Throttle ----
float Throttle::percent() const {
    if (adcMax_ <= 0) return 0.0f;
    float p = 100.0f * (float)raw_ / (float)adcMax_;
    if (p < 0.0f) p = 0.0f;       // keep it between 0 and 100
    if (p > 100.0f) p = 100.0f;
    return p;
}

// ---- SensorReading ----
// the bit after the colon is an "initialiser list" - sets up the member variables
SensorReading::SensorReading(int tempRaw, int potRaw, bool buttonPressed,
                             uint32_t timestampMs, int adcMax)
    : temp_(Temperature::fromRaw(tempRaw, adcMax)),
      throttle_(potRaw, adcMax),
      button_(buttonPressed),
      timestampMs_(timestampMs),
      tempRaw_(tempRaw),
      adcMax_(adcMax) {}

float SensorReading::effectiveThrottlePercent() const {
    // & is "bitwise AND" - checks if the ESTOP bit is switched on in flags_
    return (flags_ & STATUS_ESTOP) ? 0.0f : throttle_.percent();
}

size_t SensorReading::statusString(char* buf, size_t len) const {
    if (len == 0) return 0;
    buf[0] = '\0';   // '\0' = end of string, so the buffer starts off empty
    if (flags_ == STATUS_OK) { strncpy(buf, "OK", len - 1); buf[len - 1] = '\0'; return strlen(buf); }

    size_t n = 0;   // how much of the buffer is used so far
    // a little helper function inside a function (called a lambda). It adds a word, with a + in front if it's not the first
    auto add = [&](const char* name) {
        size_t need = strlen(name) + (n ? 1 : 0);
        if (n + need >= len) return;   // no space left, so skip it rather than overflow
        if (n) buf[n++] = '+';
        strcpy(buf + n, name);
        n += strlen(name);
    };
    if (flags_ & STATUS_OVERHEAT)     add("OVERHEAT");
    if (flags_ & STATUS_ESTOP)        add("ESTOP");
    if (flags_ & STATUS_SENSOR_FAULT) add("FAULT");
    return n;
}

uint8_t telemetryChecksum(const char* s, size_t n) {
    uint8_t c = 0;
    for (size_t i = 0; i < n; ++i) c ^= (uint8_t)s[i];   // ^= is XOR, mixes each character into c
    return c;
}

size_t SensorReading::toLine(char* buf, size_t len, bool withChecksum) const {
    char status[32];
    statusString(status, sizeof(status));

    // snprintf = printf but into a char array, and it won't write past the end (safe!)
    int n = snprintf(buf, len, "TEMP:%.1f,POT:%d,BTN:%d,STATUS:%s,ERR:%u,TS:%lu",
                     temp_.toCelsius(), throttle_.raw(), button_ ? 1 : 0,
                     status, (unsigned)error_, (unsigned long)timestampMs_);
    if (n < 0 || (size_t)n >= len) { if (len) buf[0] = '\0'; return 0; }   // didn't fit, give up

    if (withChecksum) {
        uint8_t c = telemetryChecksum(buf, (size_t)n);   // checksum of everything written so far
        int m = snprintf(buf + n, len - n, ",CHK:%02X", c);   // %02X = 2 digit hex, e.g. 0A
        if (m < 0 || (size_t)(n + m) >= len) { buf[0] = '\0'; return 0; }
        n += m;
    }
    return (size_t)n;
}

// ---- StatusMonitor ----
void StatusMonitor::apply(SensorReading& r) {
    uint8_t flags = STATUS_OK;
    uint8_t err   = ERR_NONE;

    // 1. is the raw data even sensible? (outside the ADC range means a wire fell out or something)
    bool tempBad = (r.tempRaw() < 0 || r.tempRaw() > r.adcMax());
    bool potBad  = (r.throttle().raw() < 0 || r.throttle().raw() > r.adcMax());
    if (tempBad || potBad) {
        flags |= STATUS_SENSOR_FAULT;   // |= switches a bit ON without touching the others
        err = tempBad ? ERR_TEMP_OUT_OF_RANGE : ERR_THROTTLE_OUT_OF_RANGE;
    }

    // 2. overheat, with the gap between 60 and 55 so it doesn't flicker
    float t = r.temperature().toCelsius();
    if (!tempBad) {
        if (!overheating_ && t >= onC_)      overheating_ = true;
        else if (overheating_ && t < offC_)  overheating_ = false;
    }
    if (overheating_) { flags |= STATUS_OVERHEAT; err = ERR_OVERHEAT; }

    // 3. emergency stop - once pressed it stays on until resetEstop() is called.
    // This goes last so it overrides the other error codes (most serious wins)
    if (r.buttonPressed()) estopLatched_ = true;
    if (estopLatched_) { flags |= STATUS_ESTOP; err = ERR_EMERGENCY_STOP; }

    r.setStatus(flags, err);   // save the results back into the reading
}
