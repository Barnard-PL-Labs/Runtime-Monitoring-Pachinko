/**
 * RTLola Arduino/ESP32 Helper Functions
 * 
 * Common utilities for working with RTLola monitors on embedded systems.
 */

#ifndef RTLOLA_HELPERS_H
#define RTLOLA_HELPERS_H

#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Get the current time in seconds (for RTLola timestamps)
 */
static inline double rtlola_current_time() {
    return millis() / 1000.0;
}

/**
 * Get the current time in milliseconds
 */
static inline unsigned long rtlola_current_time_ms() {
    return millis();
}

/**
 * Initialize serial for RTLola output
 * @param baud Baud rate (default 115200)
 */
static inline void rtlola_init_serial(unsigned long baud = 115200) {
    Serial.begin(baud);
    while (!Serial && millis() < 3000) {
        delay(10);  // Wait up to 3 seconds for serial
    }
}

#ifdef ESP32
/**
 * Get free heap memory (ESP32 only)
 */
static inline uint32_t rtlola_free_heap() {
    return ESP.getFreeHeap();
}

/**
 * Get minimum free heap since boot (ESP32 only)
 */
static inline uint32_t rtlola_min_free_heap() {
    return ESP.getMinFreeHeap();
}

/**
 * Print memory status (ESP32 only)
 */
static inline void rtlola_print_memory_status() {
    Serial.printf("Heap: %u free, %u min\n", 
                  ESP.getFreeHeap(), 
                  ESP.getMinFreeHeap());
}
#endif

#ifdef __cplusplus
}
#endif

#endif // RTLOLA_HELPERS_H

