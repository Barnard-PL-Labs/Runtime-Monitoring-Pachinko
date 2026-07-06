#!/bin/bash
#
# Create distribution archives for sharing the RTLola ESP32 project
#
# Usage:
#   ./scripts/create_distribution.sh
#
# Creates two zip files:
#   1. rtlola-esp32-binary.zip    - Includes pre-built compiler (same platform only)
#   2. rtlola-esp32-source.zip    - Includes source to build on any platform
#

set -e

# Get directories
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
RTLOLA_DIR="$(cd "$PROJECT_DIR/.." && pwd)"
OUTPUT_DIR="$PROJECT_DIR/dist"

echo "Creating distribution archives..."
echo ""

# Create output directory
mkdir -p "$OUTPUT_DIR"

# ============================================================================
# Create binary distribution (same platform)
# ============================================================================
echo "Creating binary distribution (for same platform)..."

BINARY_ZIP="$OUTPUT_DIR/rtlola-esp32-binary.zip"
rm -f "$BINARY_ZIP"

cd "$RTLOLA_DIR"
zip -r "$BINARY_ZIP" \
    rtlola-platformio/platformio.ini \
    rtlola-platformio/src/ \
    rtlola-platformio/include/ \
    rtlola-platformio/specs/ \
    rtlola-platformio/scripts/ \
    rtlola-platformio/lib/.gitkeep \
    rtlola-platformio/SETUP.md \
    rtlola-platformio/README.md \
    rtlola-platformio/.gitignore \
    target/release/rtlola2c \
    -x "*.DS_Store" \
    -x "*__pycache__*" \
    -x "*.pyc"

echo "  Created: $BINARY_ZIP"
echo "  Size: $(du -h "$BINARY_ZIP" | cut -f1)"

# ============================================================================
# Create source distribution (any platform)
# ============================================================================
echo ""
echo "Creating source distribution (for any platform)..."

SOURCE_ZIP="$OUTPUT_DIR/rtlola-esp32-source.zip"
rm -f "$SOURCE_ZIP"

cd "$RTLOLA_DIR"
zip -r "$SOURCE_ZIP" \
    rtlola-platformio/platformio.ini \
    rtlola-platformio/src/ \
    rtlola-platformio/include/ \
    rtlola-platformio/specs/ \
    rtlola-platformio/scripts/ \
    rtlola-platformio/lib/.gitkeep \
    rtlola-platformio/SETUP.md \
    rtlola-platformio/README.md \
    rtlola-platformio/.gitignore \
    rtlola2c/ \
    rtlola-streamir/ \
    Cargo.toml \
    Cargo.lock \
    -x "*.DS_Store" \
    -x "*__pycache__*" \
    -x "*.pyc" \
    -x "*/target/*"

echo "  Created: $SOURCE_ZIP"
echo "  Size: $(du -h "$SOURCE_ZIP" | cut -f1)"

# ============================================================================
# Summary
# ============================================================================
echo ""
echo "════════════════════════════════════════════════════════════"
echo "Distribution archives created in: $OUTPUT_DIR"
echo ""
echo "  rtlola-esp32-binary.zip"
echo "    - Use when sharing with same OS/architecture"
echo "    - Includes pre-built rtlola2c compiler"
echo "    - No Rust required on target machine"
echo ""
echo "  rtlola-esp32-source.zip"
echo "    - Use when sharing with different OS/architecture"
echo "    - Includes compiler source code"
echo "    - Requires Rust on target machine to build compiler"
echo ""
echo "To use on another machine:"
echo "  1. unzip rtlola-esp32-*.zip"
echo "  2. cd rtlola/rtlola-platformio"
echo "  3. ./scripts/setup.sh"
echo "════════════════════════════════════════════════════════════"
