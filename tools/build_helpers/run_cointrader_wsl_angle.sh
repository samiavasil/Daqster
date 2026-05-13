#!/bin/bash
# Launch QtCoinTrader plugin with WSL2 ANGLE (hardware accelerated)

# ANGLE is an OpenGL-to-Direct3D translator, works well on WSL2
export QT_QPA_GL_GLES2=1
export LD_LIBRARY_PATH="$HOME/bin/openssl11/pkg/usr/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH}"

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DAQSTER_BIN="$SCRIPT_DIR/Daqster"

if [ ! -f "$DAQSTER_BIN" ]; then
    echo "Error: Daqster binary not found at $DAQSTER_BIN"
    exit 1
fi

# Run with QtCoinTrader plugin argument
exec "$DAQSTER_BIN" QtCoinTrader "$@"
