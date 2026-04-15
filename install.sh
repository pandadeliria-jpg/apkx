#!/bin/bash
set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_NAME="apkx"
BUILD_DIR="${SCRIPT_DIR}/build"

echo -e "${GREEN}=== apkx Installer ===${NC}"
echo ""

# Check for dependencies
echo -e "${YELLOW}Checking dependencies...${NC}"

if ! command -v cmake &> /dev/null; then
    echo -e "${RED}Error: cmake is required but not installed.${NC}"
    echo "Install with: brew install cmake"
    exit 1
fi

if ! command -v curl-config &> /dev/null; then
    echo -e "${RED}Error: libcurl is required but not installed.${NC}"
    echo "Install with: brew install curl"
    exit 1
fi

echo -e "${GREEN}✓ Dependencies OK${NC}"
echo ""

# Determine install location
if [ -n "$PREFIX" ]; then
    INSTALL_PREFIX="$PREFIX"
elif [ -w "/usr/local/bin" ]; then
    INSTALL_PREFIX="/usr/local"
elif [ -d "$HOME/.local/bin" ]; then
    INSTALL_PREFIX="$HOME/.local"
else
    INSTALL_PREFIX="$HOME/.local"
    mkdir -p "$HOME/.local/bin"
fi

INSTALL_BIN="${INSTALL_PREFIX}/bin"

echo -e "${YELLOW}Install location: ${INSTALL_BIN}${NC}"
echo ""

# Build
echo -e "${YELLOW}Building ${PROJECT_NAME}...${NC}"

mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.ncpu)

if [ ! -f "./${PROJECT_NAME}" ]; then
    echo -e "${RED}Error: Build failed - binary not found${NC}"
    exit 1
fi

echo -e "${GREEN}✓ Build successful${NC}"
echo ""

# Install
echo -e "${YELLOW}Installing to ${INSTALL_BIN}...${NC}"

mkdir -p "${INSTALL_BIN}"
cp "./${PROJECT_NAME}" "${INSTALL_BIN}/${PROJECT_NAME}"
chmod +x "${INSTALL_BIN}/${PROJECT_NAME}"

echo -e "${GREEN}✓ Installed ${PROJECT_NAME} to ${INSTALL_BIN}${NC}"
echo ""

# Check if in PATH
if [[ ":$PATH:" != *":${INSTALL_BIN}:"* ]]; then
    echo -e "${YELLOW}Warning: ${INSTALL_BIN} is not in your PATH${NC}"
    echo ""
    echo "Add this to your shell profile (~/.zshrc or ~/.bashrc):"
    echo ""
    echo "    export PATH=\"${INSTALL_BIN}:\$PATH\""
    echo ""
    echo "Then reload with: source ~/.zshrc (or ~/.bashrc)"
else
    echo -e "${GREEN}✓ ${INSTALL_BIN} is already in PATH${NC}"
fi

echo ""
echo -e "${GREEN}=== Installation Complete ===${NC}"
echo ""
echo "Usage:"
echo "  apkx install <path.apk>     # Install an APK"
echo "  apkx list                   # List installed apps"
echo "  apkx run <package>          # Run an installed app"
echo "  apkx analyze <file.so>      # Analyze ELF/DEX files"
echo ""
