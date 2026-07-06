#!/bin/bash
#
# RTLola ESP32 Project Setup Script
#
# This script checks prerequisites and sets up the project on a new machine.
#
# Usage:
#   chmod +x scripts/setup.sh
#   ./scripts/setup.sh
#

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
RTLOLA_DIR="$(cd "$PROJECT_DIR/.." && pwd)"

echo ""
echo -e "${BLUE}╔════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║     RTLola ESP32 Project - Setup Script                    ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════════════╝${NC}"
echo ""

# Track what needs to be done
NEEDS_RUST=false
NEEDS_COMPILER=false
NEEDS_PLATFORMIO=false
ALL_GOOD=true

# ============================================================================
# Check Python
# ============================================================================
echo -e "${YELLOW}Checking Python...${NC}"
if command -v python3 &> /dev/null; then
    PYTHON_VERSION=$(python3 --version 2>&1)
    echo -e "  ${GREEN}✓${NC} $PYTHON_VERSION"
else
    echo -e "  ${RED}✗${NC} Python 3 not found"
    echo "    Install Python from https://www.python.org/downloads/"
    ALL_GOOD=false
fi

# ============================================================================
# Check PlatformIO
# ============================================================================
echo -e "${YELLOW}Checking PlatformIO...${NC}"
if command -v pio &> /dev/null; then
    PIO_VERSION=$(pio --version 2>&1)
    echo -e "  ${GREEN}✓${NC} $PIO_VERSION"
elif command -v platformio &> /dev/null; then
    PIO_VERSION=$(platformio --version 2>&1)
    echo -e "  ${GREEN}✓${NC} $PIO_VERSION"
else
    echo -e "  ${RED}✗${NC} PlatformIO not found"
    NEEDS_PLATFORMIO=true
    ALL_GOOD=false
fi

# ============================================================================
# Check RTLola Compiler
# ============================================================================
echo -e "${YELLOW}Checking RTLola compiler...${NC}"
COMPILER_PATH="$RTLOLA_DIR/target/release/rtlola2c"
if [ -f "$COMPILER_PATH" ]; then
    echo -e "  ${GREEN}✓${NC} rtlola2c found at $COMPILER_PATH"
    # Test it works
    if "$COMPILER_PATH" --help &> /dev/null; then
        echo -e "  ${GREEN}✓${NC} rtlola2c is working"
    else
        echo -e "  ${RED}✗${NC} rtlola2c exists but doesn't run (wrong platform?)"
        NEEDS_COMPILER=true
        ALL_GOOD=false
    fi
else
    echo -e "  ${YELLOW}!${NC} rtlola2c not found - will need to build from source"
    NEEDS_COMPILER=true
fi

# ============================================================================
# Check Rust (only if compiler needs building)
# ============================================================================
if [ "$NEEDS_COMPILER" = true ]; then
    echo -e "${YELLOW}Checking Rust (needed to build compiler)...${NC}"
    if command -v cargo &> /dev/null; then
        RUST_VERSION=$(rustc --version 2>&1)
        echo -e "  ${GREEN}✓${NC} $RUST_VERSION"
    else
        echo -e "  ${RED}✗${NC} Rust not found"
        NEEDS_RUST=true
        ALL_GOOD=false
    fi
fi

# ============================================================================
# Check source files for building compiler
# ============================================================================
if [ "$NEEDS_COMPILER" = true ]; then
    echo -e "${YELLOW}Checking compiler source files...${NC}"
    if [ -f "$RTLOLA_DIR/Cargo.toml" ] && [ -d "$RTLOLA_DIR/rtlola2c" ]; then
        echo -e "  ${GREEN}✓${NC} Compiler source found"
    else
        echo -e "  ${RED}✗${NC} Compiler source not found"
        echo "    Need: rtlola/Cargo.toml, rtlola/rtlola2c/, rtlola/rtlola-streamir/"
        ALL_GOOD=false
    fi
fi

# ============================================================================
# Summary and Actions
# ============================================================================
echo ""
echo -e "${BLUE}════════════════════════════════════════════════════════════${NC}"
echo ""

if [ "$ALL_GOOD" = true ] && [ "$NEEDS_COMPILER" = false ]; then
    echo -e "${GREEN}All prerequisites satisfied!${NC}"
    echo ""
    echo "You can now build and upload:"
    echo "  cd $PROJECT_DIR"
    echo "  pio run -t upload"
    echo ""
    exit 0
fi

# Offer to install/build missing components
if [ "$NEEDS_PLATFORMIO" = true ]; then
    echo -e "${YELLOW}PlatformIO needs to be installed.${NC}"
    read -p "Install PlatformIO now? (y/n) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo "Installing PlatformIO..."
        pip3 install platformio
        echo -e "${GREEN}✓${NC} PlatformIO installed"
    fi
fi

if [ "$NEEDS_RUST" = true ]; then
    echo -e "${YELLOW}Rust needs to be installed to build the compiler.${NC}"
    read -p "Install Rust now? (y/n) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo "Installing Rust..."
        curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
        source "$HOME/.cargo/env"
        echo -e "${GREEN}✓${NC} Rust installed"
        NEEDS_RUST=false
    fi
fi

if [ "$NEEDS_COMPILER" = true ] && [ "$NEEDS_RUST" = false ]; then
    echo -e "${YELLOW}RTLola compiler needs to be built.${NC}"
    read -p "Build rtlola2c now? (y/n) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo "Building rtlola2c compiler..."
        cd "$RTLOLA_DIR"
        cargo build --release
        echo -e "${GREEN}✓${NC} rtlola2c built successfully"
        NEEDS_COMPILER=false
    fi
fi

# ============================================================================
# Final Test Build
# ============================================================================
echo ""
if [ "$NEEDS_COMPILER" = false ]; then
    echo -e "${YELLOW}Running test build...${NC}"
    cd "$PROJECT_DIR"
    if pio run; then
        echo ""
        echo -e "${GREEN}════════════════════════════════════════════════════════════${NC}"
        echo -e "${GREEN}  Setup complete! Project built successfully.${NC}"
        echo -e "${GREEN}════════════════════════════════════════════════════════════${NC}"
        echo ""
        echo "Next steps:"
        echo "  1. Connect your ESP32 via USB"
        echo "  2. Upload: pio run -t upload"
        echo "  3. Monitor: pio device monitor"
        echo ""
    else
        echo -e "${RED}Build failed. Check errors above.${NC}"
        exit 1
    fi
else
    echo -e "${YELLOW}Setup incomplete. Please install missing dependencies.${NC}"
    exit 1
fi
