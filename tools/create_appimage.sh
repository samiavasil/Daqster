#!/bin/bash

# Daqster AppImage Creator Script (Unified)
# This script creates an AppImage for both local and CI environments
#
# Packaging notes:
# - Qt libraries are bundled with a blanket copy of the Qt tree (libs +
#   plugins + QML). This is intentionally NOT ldd-based resolution: Qt
#   plugins and QML modules carry their own transitive dependencies and a
#   hand-picked subset is fragile. The blanket copy is larger but reliable.
# - Test plugins (*test*plugin*.so / *plugin*test*.so) and private plugins
#   (*AiStudio*) are always filtered out of the packaged plugin directory.
# - GStreamer backends (libgstreamer*, libgst*, gstreamer-1.0 plugins) are
#   bundled so Qt Multimedia can play audio/video inside the AppImage.

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

# Copy libraries (CI mode: includes lib/daqster/plugins/ from cmake --install)
echo "Copying libraries..."
cp -r "$SOURCE_BUILD_DIR/lib"/* "$BUILD_DIR/Daqster.AppDir/usr/lib/" 2>/dev/null || true

# Copy plugins from build directory (local mode only)
if [ "$MODE" = "local" ]; then
    echo "Copying plugins from build directory..."
    for f in "$SOURCE_BUILD_DIR"/bin/*plugin*.so; do
        [ -e "$f" ] || continue
        name="$(basename "$f")"
        case "$name" in
            *AiStudio*|*test*plugin*.so|*plugin*test*.so)
                echo "  Skipping (filtered): $name"
                continue
                ;;
        esac
        cp "$f" "$BUILD_DIR/Daqster.AppDir/usr/lib/daqster/plugins/"
    done
fi

# Filter test/private plugins out of the packaged plugin directory
# (CI mode: plugins arrive via the lib/* copy above; local mode: copied above)
PLUGIN_DIR="$BUILD_DIR/Daqster.AppDir/usr/lib/daqster/plugins"
if [ -d "$PLUGIN_DIR" ]; then
    echo "Filtering plugins (test/private)..."
    for f in "$PLUGIN_DIR"/*; do
        [ -e "$f" ] || continue
        name="$(basename "$f")"
        case "$name" in
            *AiStudio*|*test*plugin*.so|*plugin*test*.so)
                echo "  Skipping (filtered): $name"
                rm -f "$f"
                ;;
        esac
    done
fi

# Verify the packaged plugin set
echo "Packaged plugins:"
if [ -d "$PLUGIN_DIR" ] && ls "$PLUGIN_DIR"/* >/dev/null 2>&1; then
    ls -la "$PLUGIN_DIR"
    if ls "$PLUGIN_DIR"/*RequirementsManager* >/dev/null 2>&1; then
        echo "requirements_manager plugin: INCLUDED"
    else
        echo "WARNING: requirements_manager plugin not found in staging tree"
    fi
else
    echo "  (no plugins packaged)"
fi

# Detect Qt major version and layout (Qt6 preferred, Qt5 fallback)
QT_LIB_PREFIX=""
QT_PLUGIN_SRC=""
QT_QML_SRC=""
if ls "$QT_DIR"/libQt6Core.so* >/dev/null 2>&1; then
    QT_LIB_PREFIX="libQt6"
    if [ -d "$QT_DIR/qt6" ]; then
        QT_PLUGIN_SRC="$QT_DIR/qt6/plugins"
        QT_QML_SRC="$QT_DIR/qt6/qml"
    else
        QT_PLUGIN_SRC="$QT_DIR/plugins"
        QT_QML_SRC="$QT_DIR/qml"
    fi
elif ls "$QT_DIR"/libQt5Core.so* >/dev/null 2>&1; then
    QT_LIB_PREFIX="libQt5"
    if [ -d "$QT_DIR/qt5" ]; then
        QT_PLUGIN_SRC="$QT_DIR/qt5/plugins"
        QT_QML_SRC="$QT_DIR/qt5/qml"
    else
        QT_PLUGIN_SRC="$QT_DIR/plugins"
        QT_QML_SRC="$QT_DIR/qml"
    fi
fi

# Copy Qt libraries (blanket copy — see header note on why not ldd-based)
if [ -n "$QT_LIB_PREFIX" ]; then
    echo "Copying Qt libraries ($QT_LIB_PREFIX)..."
    cp -r "$QT_DIR"/${QT_LIB_PREFIX}* "$BUILD_DIR/Daqster.AppDir/usr/lib/" 2>/dev/null || true
else
    echo "Warning: no Qt libraries detected in $QT_DIR"
fi

# Copy Qt plugins
if [ -n "$QT_PLUGIN_SRC" ] && [ -d "$QT_PLUGIN_SRC" ]; then
    echo "Copying Qt plugins from $QT_PLUGIN_SRC..."
    cp -r "$QT_PLUGIN_SRC"/* "$BUILD_DIR/Daqster.AppDir/usr/lib/plugins/" 2>/dev/null || true
fi

# Copy QML modules
if [ -n "$QT_QML_SRC" ] && [ -d "$QT_QML_SRC" ]; then
    echo "Copying QML modules from $QT_QML_SRC..."
    cp -r "$QT_QML_SRC"/* "$BUILD_DIR/Daqster.AppDir/usr/lib/qml/" 2>/dev/null || true
fi

# Copy ICU libraries (if available)
echo "Copying ICU libraries..."
cp /usr/lib/x86_64-linux-gnu/libicu*.so.* "$BUILD_DIR/Daqster.AppDir/usr/lib/" 2>/dev/null || true

# Copy GStreamer backends (needed by Qt Multimedia for audio/video)
# Core libs + element plugins are bundled; the AppRun sets GST_PLUGIN_PATH.
echo "Copying GStreamer backends..."
cp -rL /usr/lib/x86_64-linux-gnu/libgst*.so* "$BUILD_DIR/Daqster.AppDir/usr/lib/" 2>/dev/null || true
if [ -d /usr/lib/x86_64-linux-gnu/gstreamer-1.0 ]; then
    mkdir -p "$BUILD_DIR/Daqster.AppDir/usr/lib/gstreamer-1.0"
    cp -rL /usr/lib/x86_64-linux-gnu/gstreamer-1.0/*.so "$BUILD_DIR/Daqster.AppDir/usr/lib/gstreamer-1.0/" 2>/dev/null || true
fi
if [ -d /usr/lib/x86_64-linux-gnu/gstreamer1.0 ]; then
    cp -rL /usr/lib/x86_64-linux-gnu/gstreamer1.0/gstreamer-1.0/* "$BUILD_DIR/Daqster.AppDir/usr/lib/gstreamer-1.0/" 2>/dev/null || true
fi

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

# Set GStreamer paths (Qt Multimedia backends)
export GST_PLUGIN_PATH="${HERE}/usr/lib/gstreamer-1.0:${GST_PLUGIN_PATH}"
export GST_PLUGIN_SYSTEM_PATH="${HERE}/usr/lib/gstreamer-1.0"

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