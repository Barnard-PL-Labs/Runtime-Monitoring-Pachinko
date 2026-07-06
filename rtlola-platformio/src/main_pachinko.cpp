/**
 * RTLola ESP32 Monitor - Pachinko Ball Detection
 * 
 * OVERVIEW:
 * =========
 * This dual-core implementation monitors 5 pachinko ball sensors using RTLola
 * for runtime verification. Ball detection uses non-blocking debouncing with
 * rising-edge detection.
 * 
 * ARCHITECTURE:
 * =============
 * 
 *   ┌─────────────────────────────┐              ┌─────────────────────────────┐
 *   │  CORE 0: Sensor System      │              │  CORE 1: RTLola Monitor     │
 *   │                             │              │                             │
 *   │  • Read 5 analog sensors    │   Event      │  • Receive ball events      │
 *   │  • Debounce (200ms)         │   Queue      │  • Run RTLola cycle()       │
 *   │  • Detect rising edges ─────────────────►  │  • Check triggers           │
 *   │  • Send ball events         │              │  • Detect patterns:         │
 *   │                             │              │    - Simultaneous balls     │
 *   │  • Receive flags ◄──────────────────────── │    - Repeated triggers      │
 *   │  • React to violations      │   Flag       │    - Unusual sequences      │
 *   │                             │   Queue      │                             │
 *   └─────────────────────────────┘              └─────────────────────────────┘
 * 
 * HARDWARE:
 * =========
 * - 5 ball sensors on ADC1 pins: GPIO 36, 39, 34, 35, 32
 * - Each sensor bridges contacts when a ball passes
 * - Analog threshold detection with debouncing
 * 
 * USAGE:
 * ======
 * 1. Update specs/pachinko.lola with your monitoring rules
 * 2. Build: pio run -e esp32dev-pachinko -t upload
 * 3. Monitor: pio device monitor
 */

#include <Arduino.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

// Include the generated RTLola monitor for pachinko spec
// The monitor.h is in lib/rtlola_pachinko/ (generated from specs/pachinko.lola)
// We use a direct path since PlatformIO's LDF may not find it on first build
#include "../lib/rtlola_pachinko/monitor.h"

// ============================================================================
// Hardware Configuration
// ============================================================================

#define SERIAL_BAUD 115200

// Sensor pins (ADC1-capable GPIOs on ESP32)
static const int SENSOR_COUNT = 5;
static const uint8_t SENSOR_PINS[SENSOR_COUNT] = { 36, 39, 34, 35, 32 };

// Detection parameters
static const int THRESHOLD = 1000;              // Analog threshold for ball detection
static const unsigned long DEBOUNCE_MS = 200;   // Debounce time per sensor

// ============================================================================
// Task Configuration
// ============================================================================

#define SYSTEM_STACK_SIZE 4096
#define MONITOR_STACK_SIZE 4096
#define EVENT_QUEUE_SIZE 20     // Buffer for ball events
#define FLAG_QUEUE_SIZE 10      // Buffer for monitor flags

// System runs at high frequency for responsive ball detection
#define SYSTEM_LOOP_MS 5        // 200Hz sensor polling

// ============================================================================
// Flag Types - Messages from Monitor to System
// ============================================================================
// 
// These are generic flag types that students can assign meaning to.
// For example:
//   FLAG_TYPE_1 = "send to display ESP32"
//   FLAG_TYPE_2 = "send to sound ESP32"
//   FLAG_TYPE_3 = "log to SD card"
//   FLAG_TYPE_4 = "broadcast to all"
//

typedef enum {
    FLAG_TYPE_0 = 0,
    FLAG_TYPE_1,
    FLAG_TYPE_2,
    FLAG_TYPE_3,
    FLAG_TYPE_4
} FlagType;

typedef struct {
    FlagType type;
    uint8_t trigger_id;     // Which trigger fired (0, 1, 2, ...)
    double timestamp;       // When the violation occurred
    char message[64];       // Human-readable message
} MonitorFlag;

// ============================================================================
// Global Variables
// ============================================================================

// Queues for inter-core communication
QueueHandle_t eventQueue;       // System → Monitor (ball events)
QueueHandle_t flagQueue;        // Monitor → System (alerts)

// Task handles
TaskHandle_t systemTaskHandle;
TaskHandle_t monitorTaskHandle;

// Statistics
volatile uint32_t total_balls_detected = 0;
volatile uint32_t balls_per_channel[SENSOR_COUNT] = {0};
volatile uint32_t events_sent = 0;
volatile uint32_t events_processed = 0;
volatile uint32_t flags_received = 0;

// ============================================================================
// CORE 0: Sensor System
// ============================================================================

// Per-sensor state for debouncing
struct SensorState {
    unsigned long lastTriggerMs;
    bool wasAbove;
};

static SensorState sensorStates[SENSOR_COUNT];

// Current cycle's ball detection (true = ball detected this cycle)
static bool ballDetected[SENSOR_COUNT];

/**
 * Initialize sensor states
 */
void initSensors() {
    for (int i = 0; i < SENSOR_COUNT; i++) {
        sensorStates[i].lastTriggerMs = 0;
        sensorStates[i].wasAbove = false;
        ballDetected[i] = false;
    }
}

/**
 * Read all sensors with non-blocking debounce.
 * Uses rising-edge detection so held contacts don't repeat.
 * 
 * After this function, ballDetected[i] is true if sensor i
 * just detected a ball this cycle (and wasn't in debounce).
 */
void readAllSensors() {
    const unsigned long now = millis();
    
    for (int i = 0; i < SENSOR_COUNT; i++) {
        const int raw = analogRead(SENSOR_PINS[i]);
        const bool above = (raw > THRESHOLD);
        
        // Reset detection flag each cycle
        ballDetected[i] = false;
        
        // Rising edge: crossed threshold upward
        if (above && !sensorStates[i].wasAbove) {
            // Check debounce
            if (now - sensorStates[i].lastTriggerMs >= DEBOUNCE_MS) {
                sensorStates[i].lastTriggerMs = now;
                ballDetected[i] = true;
                
                // Update statistics
                total_balls_detected++;
                balls_per_channel[i]++;
                
                // Log detection
                Serial.printf("[SENSOR] Ball on channel %d (GPIO %d) raw=%d\n", 
                              i, SENSOR_PINS[i], raw);
            }
        }
        
        sensorStates[i].wasAbove = above;
    }
}

/**
 * Handle a flag received from the monitor.
 * 
 * Students can customize what happens for each flag type.
 * For example, different types could send to different ESP32s via ESP-NOW.
 */
void handleMonitorFlag(MonitorFlag& flag) {
    flags_received++;

    Serial.printf("[SYSTEM] Flag received: type=%d, trigger=%d, msg=%s\n",
                  flag.type, flag.trigger_id, flag.message);

    // ════════════════════════════════════════════════════════════════════
    // THIS IS WHERE YOU PUT ESP-NOW CODE TO RESPOND TO FLAGS FROM THE MONITOR
    // 
    // Example: esp_now_send(peer_mac, (uint8_t*)&flag, sizeof(flag));
    // ════════════════════════════════════════════════════════════════════
    
    switch (flag.type) {
        case FLAG_TYPE_0:
            Serial.printf("[SYSTEM] Type 0: %s\n", flag.message);
            break;
            
        case FLAG_TYPE_1:
            Serial.printf("[SYSTEM] Type 1: %s\n", flag.message);
            break;
            
        case FLAG_TYPE_2:
            Serial.printf("[SYSTEM] Type 2: %s\n", flag.message);
            break;
            
        case FLAG_TYPE_3:
            Serial.printf("[SYSTEM] Type 3: %s\n", flag.message);
            break;
            
        case FLAG_TYPE_4:
            Serial.printf("[SYSTEM] Type 4: %s\n", flag.message);
            break;
            
        default:
            break;
    }
}

/**
 * System Task - runs on Core 0
 * 
 * Reads all 5 sensors with non-blocking debounce and sends
 * ball detection events to the RTLola monitor on Core 1.
 */
void systemTask(void* parameter) {
    Serial.println("[SYSTEM] Task started on Core 0");
    Serial.printf("[SYSTEM] Monitoring %d sensors at %dHz\n", 
                  SENSOR_COUNT, 1000/SYSTEM_LOOP_MS);
    Serial.printf("[SYSTEM] Pins: ");
    for (int i = 0; i < SENSOR_COUNT; i++) {
        Serial.printf("%d ", SENSOR_PINS[i]);
    }
    Serial.println();
    
    // Initialize sensor states
    initSensors();
    
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(SYSTEM_LOOP_MS);
    
    while (true) {
        // ─────────────────────────────────────────────────────────────
        // Step 1: Read all sensors (non-blocking with debounce)
        // ─────────────────────────────────────────────────────────────
        readAllSensors();
        
        // ─────────────────────────────────────────────────────────────
        // Step 2: Create event for RTLola monitor
        // Send 1.0 if ball detected, 0.0 otherwise
        // ─────────────────────────────────────────────────────────────
        InternalEvent event = {
            .sensor_1 = ballDetected[0] ? 1.0 : 0.0,
            .sensor_1_is_present = true,
            .sensor_2 = ballDetected[1] ? 1.0 : 0.0,
            .sensor_2_is_present = true,
            .sensor_3 = ballDetected[2] ? 1.0 : 0.0,
            .sensor_3_is_present = true,
            .sensor_4 = ballDetected[3] ? 1.0 : 0.0,
            .sensor_4_is_present = true,
            .sensor_5 = ballDetected[4] ? 1.0 : 0.0,
            .sensor_5_is_present = true,
            .time = millis() / 1000.0
        };
        
        // Only send if at least one ball was detected (reduces queue traffic)
        // Remove this check if you want continuous monitoring
        bool anyBall = ballDetected[0] || ballDetected[1] || ballDetected[2] || 
                       ballDetected[3] || ballDetected[4];
        
        if (anyBall) {
            if (xQueueSend(eventQueue, &event, 0) == pdTRUE) {
                events_sent++;
            }
        }
        
        // ─────────────────────────────────────────────────────────────
        // Step 3: Check for flags from monitor (non-blocking)
        // ─────────────────────────────────────────────────────────────
        MonitorFlag flag;
        while (xQueueReceive(flagQueue, &flag, 0) == pdTRUE) {
            handleMonitorFlag(flag);
        }
        
        // Run at fixed frequency
        vTaskDelayUntil(&lastWakeTime, period);
    }
}

// ============================================================================
// CORE 1: RTLola Monitor
// ============================================================================

Memory rtlola_memory;

/**
 * Send a flag to the system.
 * 
 * @param type       The flag type (FLAG_TYPE_0 through FLAG_TYPE_4)
 * @param trigger_id Which RTLola trigger fired (0, 1, 2, ...)
 * @param timestamp  When the trigger fired
 * @param message    Human-readable message from the trigger
 */
void sendFlag(FlagType type, uint8_t trigger_id, 
              double timestamp, const char* message) {
    MonitorFlag flag;
    flag.type = type;
    flag.trigger_id = trigger_id;
    flag.timestamp = timestamp;
    strncpy(flag.message, message, sizeof(flag.message) - 1);
    flag.message[sizeof(flag.message) - 1] = '\0';
    
    xQueueSend(flagQueue, &flag, 0);
}

/**
 * Handle RTLola verdict - check triggers and send flags.
 * 
 * Each trigger maps to a flag type. Students can customize this mapping
 * based on what action they want each trigger to produce.
 */
void handleVerdict(Verdict& verdict, double event_time) {
    // Trigger 0: Multiple balls simultaneously → Flag Type 0
    if (verdict.trigger_0_is_present) {
        Serial.printf("[MONITOR] Trigger 0 at t=%.2f\n", event_time);
        sendFlag(FLAG_TYPE_0, 0, event_time, 
                 verdict.trigger_0 ? verdict.trigger_0 : "Multiple simultaneous balls!");
    }
    
    // Trigger 1: Same channel twice in a row → Flag Type 1
    if (verdict.trigger_1_is_present) {
        Serial.printf("[MONITOR] Trigger 1 at t=%.2f\n", event_time);
        sendFlag(FLAG_TYPE_1, 1, event_time,
                 verdict.trigger_1 ? verdict.trigger_1 : "Repeated channel trigger");
    }
    
    // Trigger 2: Edge channels (1 and 5) at same time → Flag Type 2
    if (verdict.trigger_2_is_present) {
        Serial.printf("[MONITOR] Trigger 2 at t=%.2f\n", event_time);
        sendFlag(FLAG_TYPE_2, 2, event_time,
                 verdict.trigger_2 ? verdict.trigger_2 : "Edge channels triggered");
    }
    
    // Trigger 3: Three or more balls at once → Flag Type 3
    if (verdict.trigger_3_is_present) {
        Serial.printf("[MONITOR] Trigger 3 at t=%.2f\n", event_time);
        sendFlag(FLAG_TYPE_3, 3, event_time,
                 verdict.trigger_3 ? verdict.trigger_3 : "Too many simultaneous balls!");
    }
    
    // Trigger 4: Center channel (3) triggered → Flag Type 4
    if (verdict.trigger_4_is_present) {
        Serial.printf("[MONITOR] Trigger 4 at t=%.2f\n", event_time);
        sendFlag(FLAG_TYPE_4, 4, event_time,
                 verdict.trigger_4 ? verdict.trigger_4 : "Center channel hit!");
    }
    
    // Add more triggers as defined in your pachinko.lola spec
}

/**
 * Monitor Task - runs on Core 1
 */
void monitorTask(void* parameter) {
    Serial.println("[MONITOR] Task started on Core 1");
    
    // Initialize RTLola monitor
    memset(&rtlola_memory, 0, sizeof(rtlola_memory));
    Serial.println("[MONITOR] RTLola monitor initialized");
    
    InternalEvent event;
    
    while (true) {
        // Wait for ball events from system
        if (xQueueReceive(eventQueue, &event, portMAX_DELAY) == pdTRUE) {
            events_processed++;
            
            // Process through RTLola
            Verdict verdict = cycle(&rtlola_memory, event);
            
            // Check for violations and send flags
            handleVerdict(verdict, event.time);
        }
    }
}

// ============================================================================
// Status Reporting Task
// ============================================================================

void statusTask(void* parameter) {
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(10000));  // Every 10 seconds
        
        Serial.println("\n════════════════════════════════════════════════════════════");
        Serial.println("           PACHINKO MONITOR STATUS");
        Serial.println("════════════════════════════════════════════════════════════");
        Serial.printf("  Total balls detected: %lu\n", total_balls_detected);
        Serial.println("  Per-channel counts:");
        for (int i = 0; i < SENSOR_COUNT; i++) {
            Serial.printf("    Channel %d (GPIO %d): %lu balls\n", 
                          i, SENSOR_PINS[i], balls_per_channel[i]);
        }
        Serial.println("  Communication:");
        Serial.printf("    Events sent:      %lu\n", events_sent);
        Serial.printf("    Events processed: %lu\n", events_processed);
        Serial.printf("    Flags received:   %lu\n", flags_received);
        Serial.printf("  Free heap: %u bytes\n", ESP.getFreeHeap());
        Serial.println("════════════════════════════════════════════════════════════\n");
    }
}

// ============================================================================
// Setup
// ============================================================================

void setup() {
    Serial.begin(SERIAL_BAUD);
    while (!Serial && millis() < 3000) {
        delay(10);
    }
    
    Serial.println("\n");
    Serial.println("╔════════════════════════════════════════════════════════════╗");
    Serial.println("║     PACHINKO BALL MONITOR - RTLola Runtime Verification    ║");
    Serial.println("╠════════════════════════════════════════════════════════════╣");
    Serial.println("║                                                            ║");
    Serial.println("║  Core 0: Sensor System                                     ║");
    Serial.println("║    • Reads 5 ball sensors (non-blocking)                   ║");
    Serial.println("║    • Debounced edge detection                              ║");
    Serial.println("║    • Sends ball events to monitor                          ║");
    Serial.println("║                                                            ║");
    Serial.println("║  Core 1: RTLola Monitor                                    ║");
    Serial.println("║    • Verifies ball patterns                                ║");
    Serial.println("║    • Detects anomalies (jams, multiple balls, etc.)        ║");
    Serial.println("║    • Sends flags back to system                            ║");
    Serial.println("║                                                            ║");
    Serial.println("╚════════════════════════════════════════════════════════════╝");
    Serial.println();
    
    // Create queues
    eventQueue = xQueueCreate(EVENT_QUEUE_SIZE, sizeof(InternalEvent));
    flagQueue = xQueueCreate(FLAG_QUEUE_SIZE, sizeof(MonitorFlag));
    
    if (!eventQueue || !flagQueue) {
        Serial.println("ERROR: Failed to create queues!");
        while (1) { delay(1000); }
    }
    Serial.println("[SETUP] Queues created");
    
    // Create tasks on separate cores
    xTaskCreatePinnedToCore(systemTask, "SensorSystem", SYSTEM_STACK_SIZE,
                            NULL, 1, &systemTaskHandle, 0);  // Core 0
    
    xTaskCreatePinnedToCore(monitorTask, "RTLolaMonitor", MONITOR_STACK_SIZE,
                            NULL, 1, &monitorTaskHandle, 1);  // Core 1
    
    xTaskCreate(statusTask, "Status", 2048, NULL, 0, NULL);
    
    Serial.println("[SETUP] Tasks created");
    Serial.println("\n[SETUP] Pachinko monitoring started!\n");
    Serial.println("Drop some balls to see detection and RTLola verification.\n");
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}
