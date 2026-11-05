// SensorReading.h - Safa's telemetry classes
// (first time properly using C++ so bear with me lol)
// No Arduino.h in here so I can compile it on my laptop without the board
#ifndef SENSOR_READING_H   // include guard - stops the file being pasted in twice, apparently this matters
#define SENSOR_READING_H

#include <stdint.h>   // for uint8_t / uint32_t (numbers with a fixed size)
#include <stddef.h>   // for size_t

// ---------- Temperature (TCRT5000 pretends to be a temp sensor) ----------
// enum class = a list of named options, so I don't have to remember 0/1/2
enum class TempUnit { Celsius, Fahrenheit, Kelvin };

class Temperature {
public:
    // constructor: sets up the object. "explicit" stops C++ doing sneaky conversions
    explicit Temperature(float celsius = 0.0f) : celsius_(celsius) {}

    // turns the raw ADC number into a fake temperature (just a straight line from min to max)
    static Temperature fromRaw(int raw, int adcMax, float minC = 0.0f, float maxC = 100.0f);

    // I only store Celsius and work out the others when asked, so there's one source of truth
    float toCelsius()    const { return celsius_; }
    float toFahrenheit() const { return celsius_ * 9.0f / 5.0f + 32.0f; }   // the classic C*9/5+32
    float toKelvin()     const { return celsius_ + 273.15f; }
    float in(TempUnit u) const;   // pick the unit with the enum

private:
    float celsius_;   // trailing underscore = private variable (a habit I picked up)
};

// ---------- Throttle (the potentiometer) ----------
class Throttle {
public:
    Throttle(int raw = 0, int adcMax = 4095) : raw_(raw), adcMax_(adcMax) {}
    int   raw()     const { return raw_; }
    float percent() const;   // 0 to 100, won't go past either end
private:
    int raw_, adcMax_;
};

// ---------- Status flags ----------
// Each flag is one bit (1, 2, 4...) so more than one can be on at the same time.
// 1 << 0 means "shift the 1 over by 0 places", 1 << 1 is 2, 1 << 2 is 4 etc.
enum StatusFlag : uint8_t {
    STATUS_OK           = 0,
    STATUS_OVERHEAT     = 1 << 0,
    STATUS_ESTOP        = 1 << 1,
    STATUS_SENSOR_FAULT = 1 << 2
};

// ---------- Error codes ----------
// Just ONE number, and the more serious it is the higher the number
enum ErrorCode : uint8_t {
    ERR_NONE                  = 0,
    ERR_TEMP_OUT_OF_RANGE     = 1,
    ERR_THROTTLE_OUT_OF_RANGE = 2,
    ERR_OVERHEAT              = 3,
    ERR_EMERGENCY_STOP        = 4
};

// ---------- One snapshot of all the sensors at one moment ----------
class SensorReading {
public:
    // give it the raw numbers from analogRead / digitalRead plus millis() and it sorts the rest
    SensorReading(int tempRaw, int potRaw, bool buttonPressed,
                  uint32_t timestampMs, int adcMax = 4095);

    // getters (const = promise not to change anything, the compiler holds me to it)
    const Temperature& temperature() const { return temp_; }
    const Throttle&    throttle()    const { return throttle_; }
    bool     buttonPressed() const { return button_; }
    uint32_t timestampMs()   const { return timestampMs_; }
    int      tempRaw()       const { return tempRaw_; }
    int      adcMax()        const { return adcMax_; }

    uint8_t  statusFlags() const { return flags_; }
    uint8_t  errorCode()   const { return error_; }
    void     setStatus(uint8_t flags, uint8_t errorCode) { flags_ = flags; error_ = errorCode; }
    bool     isOk() const { return flags_ == STATUS_OK; }

    // if e-stop is on the throttle gets forced to 0 (obviously we don't want the car speeding up in an emergency)
    float effectiveThrottlePercent() const;

    // builds the line to send, e.g.  TEMP:24.5,POT:512,BTN:0,STATUS:OK,ERR:0,TS:184320,CHK:7A
    // returns how many characters it wrote, or 0 if the buffer was too small
    size_t toLine(char* buf, size_t len, bool withChecksum = true) const;

    // turns the flags into words like "OVERHEAT+ESTOP"
    size_t statusString(char* buf, size_t len) const;

private:
    Temperature temp_;
    Throttle    throttle_;
    bool        button_;
    uint32_t    timestampMs_;
    int         tempRaw_, adcMax_;
    uint8_t     flags_ = STATUS_OK;   // starts off as "all fine"
    uint8_t     error_ = ERR_NONE;
};

// ---------- StatusMonitor: the bit that decides if something is wrong ----------
class StatusMonitor {
public:
    // 60 = overheat turns ON, 55 = overheat turns OFF (gap stops it flickering on and off)
    StatusMonitor(float overheatOnC = 60.0f, float overheatOffC = 55.0f)
        : onC_(overheatOnC), offC_(overheatOffC) {}

    // call this on every reading BEFORE toLine()
    void apply(SensorReading& r);

    // un-sticks the e-stop (letting go of the button isn't enough, it has to be reset on purpose)
    void resetEstop() { estopLatched_ = false; }

    bool overheating() const { return overheating_; }
    bool estopLatched() const { return estopLatched_; }

private:
    float onC_, offC_;
    bool  overheating_ = false;
    bool  estopLatched_ = false;   // "latched" = stays on by itself once triggered
};

// checksum = XOR all the characters together. Python redoes it and if the answers differ the line got corrupted
uint8_t telemetryChecksum(const char* s, size_t n);

#endif
