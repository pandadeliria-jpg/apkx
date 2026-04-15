#!/bin/bash
set -e

# apkx - Android→macOS Compatibility Layer Installer

INSTALL_DIR="$HOME/.local/bin"
REPO_URL="https://github.com/pandadeliria-jpg/apkx.git" # Placeholder repo
BINARY_NAME="apkx"

echo "==== apkx Installer ===="

# Check for dependencies
echo "[*] Checking dependencies..."
for cmd in cmake make curl cargo git; do
    if ! command -v $cmd &> /dev/null; then
        echo "[!] Error: $cmd is not installed. Please install it first."
        exit 1
    fi
done

# Check for apkeep
if ! command -v apkeep &> /dev/null; then
    echo "[*] apkeep not found. Installing via cargo..."
    cargo install apkeep
else
    echo "[*] apkeep is already installed."
fi

# Build apkx
echo "[*] Building apkx..."
mkdir -p build
cd build
cmake ..
make -j$(sysctl -n hw.ncpu)
cd ..

# Install to ~/.local/bin
echo "[*] Installing to $INSTALL_DIR..."
mkdir -p "$INSTALL_DIR"
cp build/apkx "$INSTALL_DIR/$BINARY_NAME"
chmod +x "$INSTALL_DIR/$BINARY_NAME"

# Check PATH
if [[ ":$PATH:" != *":$INSTALL_DIR:"* ]]; then
    echo "[!] $INSTALL_DIR is not in your PATH."
    SHELL_RC=""
    if [[ "$SHELL" == */zsh ]]; then
        SHELL_RC="$HOME/.zshrc"
    elif [[ "$SHELL" == */bash ]]; then
        SHELL_RC="$HOME/.bash_profile"
    fi

    if [ -n "$SHELL_RC" ]; then
        echo "[*] Adding $INSTALL_DIR to $SHELL_RC..."
        echo "export PATH=\"\$HOME/.local/bin:\$PATH\"" >> "$SHELL_RC"
        echo "[+] Added. Please run 'source $SHELL_RC' or restart your terminal."
    else
        echo "[!] Please add 'export PATH=\"\$HOME/.local/bin:\$PATH\"' to your shell config manually."
    fi
fi

echo "[+] apkx successfully installed!"
echo "[*] Usage: apkx search <query> | apkx add <package>"
echo "[*] Update: apkx update apkx"

# Self-update check logic in script (optional, but requested)
if [ -d ".git" ]; then
    echo "[*] Checking for repo updates..."
    git fetch origin main -q
    LOCAL=$(git rev-parse HEAD)
    REMOTE=$(git rev-parse origin/main)
    if [ "$LOCAL" != "$REMOTE" ]; then
        echo "[!] A new version of apkx is available!"
        echo "[*] Run 'apkx update apkx' to upgrade."
    fi
fi
