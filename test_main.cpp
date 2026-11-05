// Laptop test - no board needed.   g++ -std=c++11 -Wall -Wextra test_main.cpp SensorReading.cpp -o test && ./test
#include "SensorReading.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

int main() {
    // Temperature conversions
    Temperature t(25.0f);
    assert(t.toFahrenheit() > 76.9f && t.toFahrenheit() < 77.1f);
    assert(t.toKelvin() > 298.1f && t.toKelvin() < 298.2f);

    StatusMonitor mon(60.0f, 55.0f);
    char line[96];

    struct Step { int temp, pot, btn; uint32_t ts; const char* note; };
    Step steps[] = {
        {1000, 2048, 0, 100, "normal"},
        {2600, 3000, 0, 200, "overheat (>=60C)"},
        {2300, 3000, 0, 300, "still overheat (hysteresis)"},
        {1500, 3000, 0, 400, "recovered"},
        {1500, 3000, 1, 500, "e-stop pressed"},
        {1500, 3000, 0, 600, "button released, still latched"},
        {5000, 100,  0, 700, "bad temp raw -> fault"},
    };

    for (size_t i = 0; i < sizeof(steps)/sizeof(steps[0]); ++i) {
        if (i == 6) mon.resetEstop();
        SensorReading r(steps[i].temp, steps[i].pot, steps[i].btn, steps[i].ts);
        mon.apply(r);
        r.toLine(line, sizeof(line));
        printf("%-34s -> %s  (throttle out %.0f%%)\n", steps[i].note, line, r.effectiveThrottlePercent());
    }

    // Matches the agreed base format when checksum is off
    SensorReading r(1000, 2048, 0, 184320);
    StatusMonitor m; m.apply(r);
    r.toLine(line, sizeof(line), false);
    printf("\nno-checksum: %s\n", line);

    // Checksum check
    r.toLine(line, sizeof(line), true);
    char* c = strstr(line, ",CHK:");
    assert(c);
    unsigned got; sscanf(c + 5, "%x", &got);
    assert(got == telemetryChecksum(line, (size_t)(c - line)));
    printf("All tests passed\n");
    return 0;
}
