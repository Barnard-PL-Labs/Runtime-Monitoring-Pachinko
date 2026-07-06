# RTLola ESP32 Project - Setup Guide

This guide explains how to set up the RTLola ESP32 monitoring project on a new machine.

## Prerequisites

### Required on All Platforms

1. **Python 3.8+**
   ```bash
   # Check if installed
   python3 --version
   ```

2. **PlatformIO CLI**
   ```bash
   # Install via pip
   pip install platformio
   
   # Or use the VS Code extension (recommended):
   # https://platformio.org/install/ide?install=vscode
   ```

3. **Git** (optional, for cloning)
   ```bash
   git --version
   ```

### Required Only If Building Compiler from Source

4. **Rust** (only needed if `rtlola2c` binary not provided)
   ```bash
   # Install Rust
   curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
   
   # Restart terminal, then verify
   rustc --version
   cargo --version
   ```

---

## Project Structure

```
rtlola/
├── Cargo.toml                    # Rust workspace config
├── Cargo.lock                    # Dependency lock file
├── rtlola2c/                     # Compiler source code
├── rtlola-streamir/              # Compiler dependency
├── target/
│   └── release/
│       └── rtlola2c              # Compiled binary (may need to build)
└── rtlola-platformio/            # ← YOU ARE HERE
    ├── platformio.ini            # PlatformIO config
    ├── src/
    │   ├── main.cpp              # Single-core version
    │   └── main_dual_core.cpp    # Dual-core version
    ├── specs/
    │   └── *.lola                # Your RTLola specifications
    ├── scripts/
    │   └── prebuild_rtlola.py    # Auto-compiles .lola files
    └── lib/                      # Auto-generated monitor code
```

---

## Setup Instructions

### Step 1: Get the Files

Either clone/copy the project or extract the provided archive:

```bash
# If you have a zip file
unzip rtlola-esp32-project.zip
cd rtlola

# If cloning from git
git clone <repository-url>
cd rtlola
```

### Step 2: Build the RTLola Compiler (If Needed)

Check if the compiler binary exists:

```bash
ls -la target/release/rtlola2c
```

**If the binary exists and you're on the same platform (OS/architecture):**
Skip to Step 3.

**If the binary doesn't exist or you're on a different platform:**

```bash
# Make sure you're in the rtlola directory (parent of rtlola-platformio)
cd rtlola

# Build the compiler (requires Rust)
cargo build --release

# Verify it built successfully
./target/release/rtlola2c --help
```

You should see help output like:
```
RTLola to C compiler
Usage: rtlola2c [OPTIONS] <SPEC>
...
```

### Step 3: Install PlatformIO Dependencies

```bash
cd rtlola-platformio

# PlatformIO will auto-download ESP32 toolchain on first build
# Just run a build to trigger this:
pio run
```

This will:
1. Download the ESP32 Arduino framework
2. Run the pre-build script (compiles your `.lola` specs)
3. Build the firmware

### Step 4: Connect Your ESP32 and Upload

```bash
# List available serial ports
pio device list

# Upload to ESP32
pio run -t upload

# Open serial monitor to see output
pio device monitor
```

---

## Quick Commands Reference

| Action | Command |
|--------|---------|
| Build firmware | `pio run` |
| Build + Upload | `pio run -t upload` |
| Serial monitor | `pio device monitor` |
| Build dual-core version | `pio run -e esp32dev-dualcore` |
| Upload dual-core version | `pio run -e esp32dev-dualcore -t upload` |
| Clean build | `pio run -t clean` |
| List serial ports | `pio device list` |

---

## Adding Your Own Specifications

1. Create a `.lola` file in `specs/`:
   ```bash
   nano specs/my_monitor.lola
   ```

2. Write your specification:
   ```lola
   input temperature: Float64
   input pressure: Float64
   
   trigger temperature > 100.0 "Too hot!"
   trigger pressure < 50.0 "Pressure low!"
   ```

3. Update `src/main.cpp` to include the generated header:
   ```cpp
   #include "rtlola_my_monitor/monitor.h"
   ```

4. Update the `InternalEvent` struct to match your inputs:
   ```cpp
   InternalEvent event = {
       .temperature = read_temp(),
       .temperature_is_present = true,
       .pressure = read_pressure(),
       .pressure_is_present = true,
       .time = millis() / 1000.0
   };
   ```

5. Build and upload:
   ```bash
   pio run -t upload
   ```

---

## Troubleshooting

### "rtlola2c compiler not found"

Build the compiler:
```bash
cd ..  # Go to rtlola directory
cargo build --release
```

### "cargo: command not found"

Install Rust:
```bash
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
source ~/.cargo/env
```

### "pio: command not found"

Install PlatformIO:
```bash
pip install platformio
```

### Serial port permission denied (Linux)

Add yourself to the dialout group:
```bash
sudo usermod -a -G dialout $USER
# Log out and back in
```

### Upload fails / ESP32 not detected

1. Check the USB cable (some are charge-only)
2. Install drivers if needed:
   - CP210x: https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers
   - CH340: https://www.wch-ic.com/downloads/CH341SER_ZIP.html
3. Try holding BOOT button while uploading

### Build errors in generated monitor code

1. Check your `.lola` spec syntax
2. Try with optimization: `RTLOLA_OPTIMIZE=1 pio run`
3. Look at the generated `lib/rtlola_<name>/monitor.h` for correct struct names

---

## Platform-Specific Notes

### macOS

- If using Apple Silicon (M1/M2/M3), the pre-built binary must also be ARM64
- Serial ports appear as `/dev/cu.usbserial-*` or `/dev/cu.SLAB_USBtoUART`

### Windows

- Serial ports appear as `COM3`, `COM4`, etc.
- May need to install CH340 or CP210x drivers
- Use PowerShell or Git Bash for commands

### Linux

- Serial ports appear as `/dev/ttyUSB0` or `/dev/ttyACM0`
- Add user to `dialout` group for serial access
- May need to install `python3-venv` for PlatformIO

---

## Version Information

- PlatformIO Core: 6.x
- ESP32 Arduino Core: 2.x
- Rust: 1.70+ (if building compiler)
- Python: 3.8+

---

## Support

For issues with:
- **RTLola language**: https://rtlola.org/
- **PlatformIO**: https://docs.platformio.org/
- **ESP32 Arduino**: https://docs.espressif.com/projects/arduino-esp32/
