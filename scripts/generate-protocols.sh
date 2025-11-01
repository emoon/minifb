#!/bin/bash

# Script to generate Wayland protocols

# On Ubuntu/Debian execute:
# sudo apt update
# sudo apt install libwayland-dev wayland-protocols

# CentOS/RHEL/Fedora execute:
# sudo dnf install wayland-devel wayland-protocols-devel
# old versions:
# sudo yum install wayland-devel wayland-protocols-devel

# Arch Linux execute:
# sudo pacman -S wayland wayland-protocols

# openSUSE execute:
# sudo zypper install wayland-devel wayland-protocols-devel

set -e

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Project root is one level up from scripts/
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

PROTOCOL_DIR="/usr/share/wayland-protocols"
OUTPUT_DIR="$PROJECT_ROOT/src/wayland/generated"

# Verify we're in the correct project (check for CMakeLists.txt or other project files)
if [ ! -f "$PROJECT_ROOT/CMakeLists.txt" ]; then
    echo "Error: No se encontró CMakeLists.txt en $PROJECT_ROOT"
    echo "Asegúrate de que el script esté en la carpeta scripts/ del proyecto minifb"
    exit 1
fi

echo "Directorio del proyecto: $PROJECT_ROOT"
echo "Directorio de salida: $OUTPUT_DIR"

# Check wayland-scanner version
if command -v wayland-scanner >/dev/null 2>&1; then
    echo "wayland-scanner version:"
    wayland-scanner --version 2>/dev/null || echo "  (version info not available)"
    echo ""
else
    echo "Error: wayland-scanner not found. Please install wayland development packages."
    exit 1
fi

# Check if wayland-protocols directory exists
if [ ! -d "$PROTOCOL_DIR" ]; then
    echo "Error: wayland-protocols directory not found at $PROTOCOL_DIR"
    echo "Please install wayland-protocols package:"
    echo " - Ubuntu/Debian: sudo apt install wayland-protocols"
    echo " - Fedora: sudo dnf install wayland-protocols-devel"
    echo " - Arch: sudo pacman -S wayland-protocols"
    echo " - openSUSE: sudo zypper install wayland-protocols-devel"
    exit 1
fi

# Create directory if it doesn't exist
mkdir -p "$OUTPUT_DIR"

# XDG Shell
echo "Generando xdg-shell..."
wayland-scanner client-header \
  "$PROTOCOL_DIR/stable/xdg-shell/xdg-shell.xml" \
  "$OUTPUT_DIR/xdg-shell-client-protocol.h"

wayland-scanner private-code \
  "$PROTOCOL_DIR/stable/xdg-shell/xdg-shell.xml" \
  "$OUTPUT_DIR/xdg-shell-protocol.c"

# Other optional protocols
if [ -f "$PROTOCOL_DIR/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml" ]; then
    echo "Generando xdg-decoration..."
    wayland-scanner client-header \
      "$PROTOCOL_DIR/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml" \
      "$OUTPUT_DIR/xdg-decoration-client-protocol.h"

    wayland-scanner private-code \
      "$PROTOCOL_DIR/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml" \
      "$OUTPUT_DIR/xdg-decoration-protocol.c"
fi

echo "Protocolos generados exitosamente en $OUTPUT_DIR"