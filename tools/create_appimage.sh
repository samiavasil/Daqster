#!/bin/bash

# Daqster AppImage Creator Script (Unified)
# This script creates an AppImage for both local and CI environments

set -e

# Default values
MODE="local"
QT_DIR=""
SOURCE_BUILD_DIR=""
BUILD_DIR=""
APPIMAGE_NAME="Daqster-x86_64.AppImage"

# Function to show usage
show_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --mode MODE          Mode: 'local' or 'ci' (default: local)"
    echo "  --qt-dir DIR         Qt installation directory"
    echo "  --source-dir DIR     Source build directory"
    echo "  --build-dir DIR      Build output directory"
    echo "  --name NAME          AppImage name (default: Daqster-x86_64.AppImage)"
    echo "  --help               Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0                                    # Local mode with defaults"
    echo "  $0 --mode ci                         # CI mode with defaults"
    echo "  $0 --qt-dir /custom/qt --source-dir /custom/build"
    echo ""
    echo "Default paths:"
    echo "  Local mode:"
    echo "    Qt: /mnt/Builder/bin/Linux/Qt/5.15.2/gcc_64"
    echo "    Source: PROJECT_ROOT/build/Desktop-Debug"
    echo "    Build: PROJECT_ROOT/tools/Build_AppImage"
    echo "  CI mode:"
    echo "    Qt: /usr/lib/x86_64-linux-gnu"
    echo "    Source: PROJECT_ROOT/stage"
    echo "    Build: PROJECT_ROOT"
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --mode)
            MODE="$2"
            shift 2
            ;;
        --qt-dir)
            QT_DIR="$2"
            shift 2
            ;;
        --source-dir)
            SOURCE_BUILD_DIR="$2"
            shift 2
            ;;
        --build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        --name)
            APPIMAGE_NAME="$2"
            shift 2
            ;;
        --help)
            show_usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            show_usage
            exit 1
            ;;
    esac
done

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Set default values based on mode
if [ "$MODE" = "ci" ]; then
    QT_DIR="${QT_DIR:-/usr/lib/x86_64-linux-gnu}"
    SOURCE_BUILD_DIR="${SOURCE_BUILD_DIR:-$PROJECT_ROOT/stage}"
    BUILD_DIR="${BUILD_DIR:-$PROJECT_ROOT}"
    echo "=== Daqster AppImage Creator (CI Mode) ==="
else
    QT_DIR="${QT_DIR:-/mnt/Builder/bin/Linux/Qt/5.15.2/gcc_64}"
    SOURCE_BUILD_DIR="${SOURCE_BUILD_DIR:-$PROJECT_ROOT/build/Desktop-Debug}"
    BUILD_DIR="${BUILD_DIR:-$PROJECT_ROOT/tools/Build_AppImage}"
    echo "=== Daqster AppImage Creator (Local Mode) ==="
fi

echo "Project root: $PROJECT_ROOT"
echo "Build directory: $BUILD_DIR"
echo "Source build: $SOURCE_BUILD_DIR"
echo "Qt directory: $QT_DIR"
echo "AppImage name: $APPIMAGE_NAME"

# Check if source build exists
if [ ! -d "$SOURCE_BUILD_DIR" ]; then
    echo "Error: Source build directory $SOURCE_BUILD_DIR not found!"
    if [ "$MODE" = "ci" ]; then
        echo "Please run 'cmake --install build --prefix stage' first"
    else
        echo "Please build the project first:"
        echo "  cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug"
        echo "  cmake --build build -j"
    fi
    exit 1
fi

# Check if Qt directory exists
if [ ! -d "$QT_DIR" ]; then
    echo "Error: Qt directory $QT_DIR not found!"
    if [ "$MODE" = "ci" ]; then
        echo "Please install Qt development packages"
    else
        echo "Please update QT_DIR in this script to point to your Qt installation"
    fi
    exit 1
fi

# Clean previous AppImage
echo "Cleaning previous AppImage..."
rm -rf "$BUILD_DIR/Daqster.AppDir"
rm -f "$BUILD_DIR/$APPIMAGE_NAME"

# Download appimagetool if not exists
if [ ! -f "$BUILD_DIR/appimagetool-x86_64.AppImage" ]; then
    echo "Downloading appimagetool..."
    cd "$BUILD_DIR"
    wget -c "https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage"
    chmod +x appimagetool-x86_64.AppImage
fi

# Create AppImage structure
echo "Creating AppImage structure..."
mkdir -p "$BUILD_DIR/Daqster.AppDir"/{usr/{bin,lib,share},usr/share/applications,usr/share/icons/hicolor/256x256/apps}
mkdir -p "$BUILD_DIR/Daqster.AppDir/usr/lib"/{plugins,qml,daqster/plugins}

# Copy executable
echo "Copying executable..."
cp "$SOURCE_BUILD_DIR/bin/Daqster" "$BUILD_DIR/Daqster.AppDir/usr/bin/"

# Copy libraries
echo "Copying libraries..."
cp -r "$SOURCE_BUILD_DIR/lib"/* "$BUILD_DIR/Daqster.AppDir/usr/lib/" 2>/dev/null || true

# Copy plugins to correct location
echo "Copying plugins..."
cp -r "$SOURCE_BUILD_DIR/bin/plugins"/* "$BUILD_DIR/Daqster.AppDir/usr/lib/daqster/plugins/" 2>/dev/null || true

# Copy Qt libraries
echo "Copying Qt libraries..."
if [ "$MODE" = "ci" ]; then
    # CI mode - copy from system Qt
    cp -r "$QT_DIR"/libQt5* "$BUILD_DIR/Daqster.AppDir/usr/lib/" 2>/dev/null || true
    cp -r "$QT_DIR"/qt5/plugins/* "$BUILD_DIR/Daqster.AppDir/usr/lib/plugins/" 2>/dev/null || true
    cp -r "$QT_DIR"/qt5/qml/* "$BUILD_DIR/Daqster.AppDir/usr/lib/qml/" 2>/dev/null || true
else
    # Local mode - copy from local Qt installation
    cp -r "$QT_DIR/lib"/* "$BUILD_DIR/Daqster.AppDir/usr/lib/" 2>/dev/null || true
    cp -r "$QT_DIR/plugins"/* "$BUILD_DIR/Daqster.AppDir/usr/lib/plugins/" 2>/dev/null || true
    cp -r "$QT_DIR/qml"/* "$BUILD_DIR/Daqster.AppDir/usr/lib/qml/" 2>/dev/null || true
fi

# Copy ICU libraries (if available)
echo "Copying ICU libraries..."
cp /usr/lib/x86_64-linux-gnu/libicu*.so.* "$BUILD_DIR/Daqster.AppDir/usr/lib/" 2>/dev/null || true

# Create AppRun script
echo "Creating AppRun script..."
cat > "$BUILD_DIR/Daqster.AppDir/AppRun" << 'APPRUN_EOF'
#!/bin/bash
HERE="$(dirname "$(readlink -f "${0}")")"

# Set library paths
export LD_LIBRARY_PATH="${HERE}/usr/lib:${LD_LIBRARY_PATH}"

# Set Qt paths
export QML2_IMPORT_PATH="${HERE}/usr/lib/qml:${QML2_IMPORT_PATH}"
export QT_PLUGIN_PATH="${HERE}/usr/lib/plugins:${QT_PLUGIN_PATH}"
export QT_QPA_PLATFORM_PLUGIN_PATH="${HERE}/usr/lib/plugins/platforms"

# Set plugin paths
export DAQSTER_PLUGIN_DIR="${HERE}/usr/lib/daqster/plugins"
export DAQSTER_PLUGIN_PATH="${HERE}/usr/lib/daqster/plugins:${HOME}/.local/share/daqster/plugins"

# Writable directories
export XDG_CONFIG_HOME="${HOME}/.config/daqster"
export XDG_DATA_HOME="${HOME}/.local/share/daqster"
export XDG_CACHE_HOME="${HOME}/.cache/daqster"

# Create directories
mkdir -p "${XDG_CONFIG_HOME}"
mkdir -p "${XDG_DATA_HOME}"
mkdir -p "${XDG_CACHE_HOME}"

# Start application
exec "${HERE}/usr/bin/Daqster" "$@"
APPRUN_EOF

chmod +x "$BUILD_DIR/Daqster.AppDir/AppRun"

# Create desktop file
echo "Creating desktop file..."
cat > "$BUILD_DIR/Daqster.AppDir/daqster.desktop" << 'DESKTOP_EOF'
[Desktop Entry]
Type=Application
Name=Daqster
Comment=Data Acquisition and Analysis Tool
Exec=daqster
Icon=daqster
Categories=Development;Science;
Terminal=false
StartupNotify=true
DESKTOP_EOF

# Copy desktop file to usr/share/applications
cp "$BUILD_DIR/Daqster.AppDir/daqster.desktop" "$BUILD_DIR/Daqster.AppDir/usr/share/applications/"

# Create icon
echo "Creating icon..."
if command -v convert >/dev/null 2>&1; then
    convert -size 256x256 xc:blue -fill white -pointsize 48 -font DejaVu-Sans-Bold -draw "text 100,150 'D'" "$BUILD_DIR/Daqster.AppDir/daqster.png"
    cp "$BUILD_DIR/Daqster.AppDir/daqster.png" "$BUILD_DIR/Daqster.AppDir/usr/share/icons/hicolor/256x256/apps/"
else
    echo "Warning: ImageMagick not found, creating empty icon file"
    touch "$BUILD_DIR/Daqster.AppDir/daqster.png"
    touch "$BUILD_DIR/Daqster.AppDir/usr/share/icons/hicolor/256x256/apps/daqster.png"
fi

# Extract appimagetool
echo "Extracting appimagetool..."
cd "$BUILD_DIR"
if [ ! -d "squashfs-root" ]; then
    ./appimagetool-x86_64.AppImage --appimage-extract
fi

# Create AppImage
echo "Creating AppImage..."
./squashfs-root/AppRun "$BUILD_DIR/Daqster.AppDir" "$BUILD_DIR/$APPIMAGE_NAME"

# Check AppImage
echo "Checking AppImage..."
file "$BUILD_DIR/$APPIMAGE_NAME"
ls -la "$BUILD_DIR/$APPIMAGE_NAME"

echo "=== AppImage created successfully: $BUILD_DIR/$APPIMAGE_NAME ==="
echo "You can now test it with: $BUILD_DIR/$APPIMAGE_NAME"

# Create symlink in project root for local mode
if [ "$MODE" = "local" ]; then
    ln -sf "$BUILD_DIR/$APPIMAGE_NAME" "$PROJECT_ROOT/$APPIMAGE_NAME"
    echo "Symlink created: $PROJECT_ROOT/$APPIMAGE_NAME"
fi
