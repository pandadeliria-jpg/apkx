#!/bin/bash
set -e

REPO_URL="https://github.com/pandadeliria-jpg/apkx.git"
INSTALL_DIR="$HOME/.local/bin"
REPO_DIR="$HOME/.apkx"

echo "Installing apkx..."

mkdir -p "$INSTALL_DIR"

if [ -d "$REPO_DIR" ]; then
    echo "Updating apkx from GitHub..."
    cd "$REPO_DIR"
    git pull origin main 2>/dev/null || echo "Update failed, using existing installation"
else
    echo "Cloning apkx from GitHub..."
    git clone "$REPO_URL" "$REPO_DIR"
    cd "$REPO_DIR"
fi

echo "Building apkx..."
cd "$REPO_DIR/android_compat_cpp"
mkdir -p build
cd build
cmake .. >/dev/null 2>&1
make -j$(nproc) >/dev/null 2>&1

cp build/apkx "$INSTALL_DIR/apkx"
chmod +x "$INSTALL_DIR/apkx"

echo ""
echo "apkx installed to $INSTALL_DIR/apkx"
echo "Add to your PATH:"
echo "  export PATH=\"\$HOME/.local/bin:\$PATH\""
echo ""
echo "Run 'apkx --help' to get started"