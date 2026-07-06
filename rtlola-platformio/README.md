# RTLola PlatformIO Project for ESP32

A streamlined workflow for compiling RTLola specifications and flashing them to ESP32 microcontrollers — all in one command.

## New Machine Setup

Setting up on a new machine? See **[SETUP.md](SETUP.md)** for detailed instructions, or run:

```bash
./scripts/setup.sh
```

## Sharing This Project

To share with others, use the distribution script:

```bash
./scripts/create_distribution.sh
```

This creates two zip files in `dist/`:
- `rtlola-esp32-binary.zip` — For same OS/architecture (includes pre-built compiler)
- `rtlola-esp32-source.zip` — For any platform (requires Rust to build compiler)

## Overview

This project automatically:
1. Detects `.lola` specification files in the `specs/` directory
2. Compiles them to C code using `rtlola2c`
3. Adapts the generated code for Arduino/ESP32
4. Builds the firmware and uploads to your ESP32

**One command does it all:** `pio run -t upload`

## Quick Start

### Prerequisites

1. **PlatformIO CLI** - Install via pip:
   ```bash
   pip install platformio
   ```
   Or use the [VS Code PlatformIO extension](https://platformio.org/install/ide?install=vscode).

2. **RTLola Compiler** - Build it from the parent directory:
   ```bash
   cd ../  # Go to rtlola directory
   cargo build --release
   ```
   This creates `target/release/rtlola2c`.

### Step 1: Add Your Specification

Create or copy your `.lola` file into the `specs/` directory:

```bash
# Example: copy an existing spec
cp ../in_place_drift.lola specs/

# Or create a new one
cat > specs/my_monitor.lola << 'EOF'
input temperature: Float64
input humidity: Float64

trigger temperature > 50.0 "Temperature too high!"
trigger humidity > 90.0 "Humidity too high!"
EOF
```

### Step 2: Update Your Code

Edit `src/main.cpp` to:
1. Include the correct monitor header (based on your spec filename)
2. Create events with the correct input fields
3. Handle the triggers from your spec

```cpp
// If your spec is specs/my_monitor.lola, include:
#include "rtlola_my_monitor/monitor.h"

// Create events matching your spec's inputs:
InternalEvent event = {
    .temperature = read_temp_sensor(),
    .temperature_is_present = true,
    .humidity = read_humidity_sensor(),
    .humidity_is_present = true,
    .time = millis() / 1000.0
};
```

### Step 3: Build and Upload

```bash
# Build only (single-core version)
pio run

# Build and upload
pio run -t upload

# Build dual-core version (for teaching runtime monitoring concepts)
pio run -e esp32dev-dualcore -t upload

# Upload and open serial monitor
pio run -t upload && pio device monitor
```

That's it! The pre-build script handles RTLola compilation automatically.

## Project Structure

```
rtlola-platformio/
├── platformio.ini          # PlatformIO configuration
├── specs/                   # Put your .lola files here
│   └── example_threshold.lola
├── src/
│   ├── main.cpp            # Single-core version
│   └── main_dual_core.cpp  # Dual-core version (pedagogical)
├── include/
│   └── rtlola_helpers.h    # Helper functions
├── lib/                     # Auto-generated monitors go here
│   └── rtlola_<spec_name>/ # Created by pre-build script
│       ├── monitor.cpp
│       ├── monitor.h
│       └── library.json
├── scripts/
│   ├── prebuild_rtlola.py  # Pre-build script (automatic)
│   ├── setup.sh            # Setup script for new machines
│   └── create_distribution.sh  # Create shareable archives
├── SETUP.md                # Detailed setup instructions
└── README.md
```

## Single-Core vs Dual-Core Versions

This project includes two versions:

### Single-Core (`main.cpp`)
- Simple, straightforward implementation
- Everything runs in `loop()` on one core
- Good for basic usage and getting started

### Dual-Core (`main_dual_core.cpp`)
- **Pedagogical version** demonstrating runtime monitoring concepts
- Core 0 = "System Under Test" (reads sensors, sends events)
- Core 1 = "Runtime Monitor" (runs RTLola, checks properties)
- Communication via FreeRTOS queue
- Shows separation between system and monitor

Use the dual-core version for teaching:
```bash
pio run -e esp32dev-dualcore -t upload
```

## Configuration

### Selecting Your ESP32 Board

Edit `platformio.ini` to change the board:

```ini
[env:esp32dev]
board = esp32dev          # Generic ESP32-WROOM

# Or use a specific board:
# board = esp32-s3-devkitc-1  # ESP32-S3
# board = esp32-c3-devkitm-1  # ESP32-C3
# board = adafruit_feather_esp32_v2
```

See all ESP32 boards: `pio boards espressif32`

### Enabling Optimization

For smaller code size, set the environment variable:

```bash
RTLOLA_OPTIMIZE=1 pio run -t upload
```

This passes `--optimize` to `rtlola2c`.

### Serial Port

If PlatformIO doesn't auto-detect your port:

```ini
[env:esp32dev]
upload_port = /dev/cu.usbserial-0001   # macOS
# upload_port = /dev/ttyUSB0           # Linux
# upload_port = COM3                    # Windows
```

## How It Works

### Pre-Build Script

The `scripts/prebuild_rtlola.py` script runs automatically before each build:

1. **Finds** all `.lola` files in `specs/`
2. **Compiles** each using `rtlola2c` from `../target/release/rtlola2c`
3. **Adapts** the generated C code for Arduino:
   - Replaces `stdio.h` with `Arduino.h`
   - Converts `printf()` to `Serial.printf()`
   - Adds `extern "C"` wrappers for C++ compatibility
4. **Outputs** to `lib/rtlola_<spec_name>/`

PlatformIO automatically discovers libraries in `lib/`, so the monitor is included in your build.

### Generated API

For each `.lola` spec, the compiler generates:

**InternalEvent struct** - Input to the monitor:
```c
typedef struct {
    double input_name;           // Your input stream
    bool input_name_is_present;  // Set true when providing this input
    double time;                 // Current timestamp in seconds
} InternalEvent;
```

**Verdict struct** - Output from the monitor:
```c
typedef struct {
    bool trigger_0_is_present;   // True if trigger 0 fired
    char* trigger_0;             // Trigger message (or NULL)
    bool trigger_1_is_present;   // True if trigger 1 fired
    char* trigger_1;             // Trigger message (or NULL)
    // ... more triggers
} Verdict;
```

**cycle() function** - Process an event:
```c
Verdict cycle(Memory* memory, InternalEvent event);
```

### Example Usage Pattern

```cpp
#include "rtlola_my_spec/monitor.h"

Memory memory;

void setup() {
    Serial.begin(115200);
    memset(&memory, 0, sizeof(memory));
}

void loop() {
    // Create event with current sensor values
    InternalEvent event = {
        .sensor_a = analogRead(A0) * 3.3 / 4095.0,
        .sensor_a_is_present = true,
        .sensor_b = analogRead(A1) * 3.3 / 4095.0,
        .sensor_b_is_present = true,
        .time = millis() / 1000.0
    };
    
    // Process through RTLola
    Verdict v = cycle(&memory, event);
    
    // React to triggers
    if (v.trigger_0_is_present) {
        digitalWrite(LED_PIN, HIGH);  // Alert!
        Serial.println(v.trigger_0);
    }
    
    delay(100);
}
```

## Multiple Specifications

You can have multiple `.lola` files in `specs/`. Each generates a separate library:

```
specs/
├── temperature_monitor.lola  → lib/rtlola_temperature_monitor/
├── motion_detector.lola      → lib/rtlola_motion_detector/
└── safety_limits.lola        → lib/rtlola_safety_limits/
```

Include multiple monitors in your code:
```cpp
#include "rtlola_temperature_monitor/monitor.h"
#include "rtlola_safety_limits/monitor.h"

Memory temp_memory, safety_memory;
// ... use each independently
```

## Troubleshooting

### "rtlola2c compiler not found"

Build the compiler:
```bash
cd ..  # Go to rtlola directory
cargo build --release
```

### "No .lola files found"

Ensure your specs are in the `specs/` directory with `.lola` extension.

### Compilation Errors in Generated Code

1. Check the `.lola` spec syntax
2. Try with `--optimize`: `RTLOLA_OPTIMIZE=1 pio run`
3. Check `lib/rtlola_<name>/monitor.h` for the correct struct names

### "undefined reference to cycle/Memory"

Make sure you're including the correct header for your spec:
- Spec: `specs/my_spec.lola`
- Include: `#include "rtlola_my_spec/monitor.h"`

### Serial Monitor Shows Garbage

Check baud rate matches: `pio device monitor -b 115200`

## VS Code Integration

If using the PlatformIO VS Code extension:

1. Open this folder in VS Code
2. PlatformIO will auto-detect the project
3. Use the PlatformIO toolbar buttons:
   - ✓ Build
   - → Upload
   - 🔌 Serial Monitor

The pre-build script runs automatically when you click Build or Upload.

## Supported RTLola Features

| Feature | Supported |
|---------|-----------|
| Input streams | ✅ |
| Output streams | ✅ |
| Triggers | ✅ |
| Offset access `.offset(by:n)` | ✅ |
| Get access `.get()` | ✅ |
| Hold access `.hold()` | ✅ |
| Sync access | ✅ |
| `is_fresh()` checks | ✅ |
| Math functions | ✅ |
| Tuple types | ✅ |
| Conditionals | ✅ |
| Sliding windows | ❌ |
| Discrete windows | ❌ |
| Future access | ❌ |

## License

Same license as the parent RTLola project.

