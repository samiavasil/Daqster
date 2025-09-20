# FindQtVersion.cmake
# Detects available Qt version and sets appropriate variables

# Debug information
message(STATUS "=== Qt Version Detection Debug ===")
message(STATUS "CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")
if(DEFINED USE_QT6)
    message(STATUS "USE_QT6 defined: TRUE, value: ${USE_QT6}")
else()
    message(STATUS "USE_QT6 defined: FALSE")
endif()

# Check for explicit Qt version preference from command line
if(DEFINED USE_QT6)
    if(USE_QT6)
        set(QT_PREFER_QT6 TRUE)
        message(STATUS "Qt6 preference set via USE_QT6=ON")
    else()
        set(QT_PREFER_QT5 TRUE)
        message(STATUS "Qt5 preference set via USE_QT6=OFF")
    endif()
elseif(CMAKE_PREFIX_PATH)
    # Use simple pattern matching for Qt version detection from CMAKE_PREFIX_PATH
    if(CMAKE_PREFIX_PATH MATCHES "qt/5\\.|qt5|Qt/5\\.|Qt5|5\\.15|5\\.14|5\\.13|5\\.12")
        set(QT_PREFER_QT5 TRUE)
        message(STATUS "Qt5 preference detected from CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")
    elseif(CMAKE_PREFIX_PATH MATCHES "qt/6\\.|qt6|Qt/6\\.|Qt6|6\\.|6\\.0|6\\.1|6\\.2|6\\.3|6\\.4|6\\.5|6\\.6|6\\.7|6\\.8|6\\.9")
        set(QT_PREFER_QT6 TRUE)
        message(STATUS "Qt6 preference detected from CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")
    else()
        message(STATUS "No Qt version preference detected from CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")
    endif()
endif()

# Try to find Qt5 first if preference is set, otherwise try Qt6 first
if(QT_PREFER_QT5)
    find_package(Qt5 QUIET COMPONENTS Core Widgets Gui)
    if(Qt5_FOUND)
        set(QT_VERSION_MAJOR 5)
        set(QT_PREFIX Qt5)
        set(QT_FOUND TRUE)
        message(STATUS "Using Qt5 (${Qt5_VERSION}) - preferred")
        
        # Set Qt5 specific variables
        set(QT_CORE_LIB Qt5::Core)
        set(QT_WIDGETS_LIB Qt5::Widgets)
        set(QT_GUI_LIB Qt5::Gui)
        
        # Try to find QML and Quick modules separately
        find_package(Qt5 QUIET COMPONENTS Qml)
        if(Qt5Qml_FOUND)
            set(QT_QML_LIB Qt5::Qml)
        else()
            set(QT_QML_LIB "")
        endif()
        
        find_package(Qt5 QUIET COMPONENTS Quick)
        if(Qt5Quick_FOUND)
            set(QT_QUICK_LIB Qt5::Quick)
        else()
            set(QT_QUICK_LIB "")
        endif()
        
        find_package(Qt5 QUIET COMPONENTS QuickControls2)
        if(Qt5QuickControls2_FOUND)
            set(QT_QUICKCONTROLS2_LIB Qt5::QuickControls2)
        else()
            set(QT_QUICKCONTROLS2_LIB "")
            message(WARNING "Qt5QuickControls2 not found, QuickControls2 support will be limited")
        endif()
        
        set(QT_CHARTS_LIB Qt5::Charts)
        set(QT_NETWORK_LIB Qt5::Network)
        find_package(Qt5 QUIET COMPONENTS Multimedia)
        if(Qt5Multimedia_FOUND)
            set(QT_MULTIMEDIA_LIB Qt5::Multimedia)
        else()
            set(QT_MULTIMEDIA_LIB "")
            message(WARNING "Qt5Multimedia not found, Multimedia support will be limited")
        endif()
        set(QT_OPENGL_LIB Qt5::OpenGL)
        
        # Qt5 specific settings
        set(QT_QML_IMPORT_PATH "qml")
        set(QT_PLUGIN_PATH "plugins")
    else()
        # Fallback to Qt6
        find_package(Qt6 QUIET COMPONENTS Core Widgets Gui Qml Quick)
        if(Qt6_FOUND)
            set(QT_VERSION_MAJOR 6)
            set(QT_PREFIX Qt6)
            set(QT_FOUND TRUE)
            message(STATUS "Using Qt6 (${Qt6_VERSION}) - fallback")
            
            # Set Qt6 specific variables
            set(QT_CORE_LIB Qt6::Core)
            set(QT_WIDGETS_LIB Qt6::Widgets)
            set(QT_GUI_LIB Qt6::Gui)
            set(QT_QML_LIB Qt6::Qml)
            set(QT_QUICK_LIB Qt6::Quick)
            set(QT_CHARTS_LIB Qt6::Charts)
            set(QT_NETWORK_LIB Qt6::Network)
            set(QT_MULTIMEDIA_LIB Qt6::Multimedia)
            set(QT_OPENGL_LIB Qt6::OpenGL)
            
            # Qt6 specific settings
            set(QT_QML_IMPORT_PATH "qml")
            set(QT_PLUGIN_PATH "plugins")
        endif()
    endif()
else()
    # Try to find Qt6 first (default behavior)
    find_package(Qt6 QUIET COMPONENTS Core Widgets Gui Qml Quick)

    if(Qt6_FOUND)
        set(QT_VERSION_MAJOR 6)
        set(QT_PREFIX Qt6)
        set(QT_FOUND TRUE)
        message(STATUS "Using Qt6 (${Qt6_VERSION})")
        
        # Set Qt6 specific variables
        set(QT_CORE_LIB Qt6::Core)
        set(QT_WIDGETS_LIB Qt6::Widgets)
        set(QT_GUI_LIB Qt6::Gui)
        set(QT_QML_LIB Qt6::Qml)
        set(QT_QUICK_LIB Qt6::Quick)
        set(QT_QUICKCONTROLS2_LIB Qt6::QuickControls2)
        set(QT_CHARTS_LIB Qt6::Charts)
        set(QT_NETWORK_LIB Qt6::Network)
        set(QT_MULTIMEDIA_LIB Qt6::Multimedia)
        set(QT_OPENGL_LIB Qt6::OpenGL)
        
        # Qt6 specific settings
        set(QT_QML_IMPORT_PATH "qml")
        set(QT_PLUGIN_PATH "plugins")
        
    else()
        # Fallback to Qt5 - try with correct module names
        find_package(Qt5 REQUIRED COMPONENTS Core Widgets Gui)
        
        # Try to find QML and Quick modules separately
        find_package(Qt5 QUIET COMPONENTS Qml)
        if(NOT Qt5Qml_FOUND)
            message(WARNING "Qt5Qml not found, QML support will be limited")
        endif()
        
        find_package(Qt5 QUIET COMPONENTS Quick)
        if(NOT Qt5Quick_FOUND)
            message(WARNING "Qt5Quick not found, Quick support will be limited")
        endif()
        
        set(QT_VERSION_MAJOR 5)
        set(QT_PREFIX Qt5)
        set(QT_FOUND TRUE)
        message(STATUS "Using Qt5 (${Qt5_VERSION}) - fallback")
        
        # Set Qt5 specific variables
        set(QT_CORE_LIB Qt5::Core)
        set(QT_WIDGETS_LIB Qt5::Widgets)
        set(QT_GUI_LIB Qt5::Gui)
        
        # QML and Quick are optional for Qt5
        if(Qt5Qml_FOUND)
            set(QT_QML_LIB Qt5::Qml)
        else()
            set(QT_QML_LIB "")
        endif()
        
        if(Qt5Quick_FOUND)
            set(QT_QUICK_LIB Qt5::Quick)
        else()
            set(QT_QUICK_LIB "")
        endif()
        
        find_package(Qt5 QUIET COMPONENTS QuickControls2)
        if(Qt5QuickControls2_FOUND)
            set(QT_QUICKCONTROLS2_LIB Qt5::QuickControls2)
        else()
            set(QT_QUICKCONTROLS2_LIB "")
            message(WARNING "Qt5QuickControls2 not found, QuickControls2 support will be limited")
        endif()
        
        set(QT_CHARTS_LIB Qt5::Charts)
        set(QT_NETWORK_LIB Qt5::Network)
        find_package(Qt5 QUIET COMPONENTS Multimedia)
        if(Qt5Multimedia_FOUND)
            set(QT_MULTIMEDIA_LIB Qt5::Multimedia)
        else()
            set(QT_MULTIMEDIA_LIB "")
            message(WARNING "Qt5Multimedia not found, Multimedia support will be limited")
        endif()
        set(QT_OPENGL_LIB Qt5::OpenGL)
        
        # Qt5 specific settings
        set(QT_QML_IMPORT_PATH "qml")
        set(QT_PLUGIN_PATH "plugins")
    endif()
endif()

# Check for optional Qt modules
if(QT_VERSION_MAJOR EQUAL 6)
    # Qt6 modules
    find_package(Qt6 QUIET COMPONENTS Charts Network Multimedia OpenGL)
    
    if(Qt6Charts_FOUND)
        set(QT_CHARTS_AVAILABLE TRUE)
        message(STATUS "Qt6 Charts available")
    else()
        set(QT_CHARTS_AVAILABLE FALSE)
        message(STATUS "Qt6 Charts not available")
    endif()
    
    if(Qt6Network_FOUND)
        set(QT_NETWORK_AVAILABLE TRUE)
        message(STATUS "Qt6 Network available")
    else()
        set(QT_NETWORK_AVAILABLE FALSE)
        message(STATUS "Qt6 Network not available")
    endif()
    
    if(Qt6Multimedia_FOUND)
        set(QT_MULTIMEDIA_AVAILABLE TRUE)
        message(STATUS "Qt6 Multimedia available")
    else()
        set(QT_MULTIMEDIA_AVAILABLE FALSE)
        message(STATUS "Qt6 Multimedia not available")
    endif()
    
    if(Qt6OpenGL_FOUND)
        set(QT_OPENGL_AVAILABLE TRUE)
        message(STATUS "Qt6 OpenGL available")
    else()
        set(QT_OPENGL_AVAILABLE FALSE)
        message(STATUS "Qt6 OpenGL not available")
    endif()
    
else()
    # Qt5 modules
    find_package(Qt5 QUIET COMPONENTS Charts Network Multimedia OpenGL)
    
    if(Qt5Charts_FOUND)
        set(QT_CHARTS_AVAILABLE TRUE)
        message(STATUS "Qt5 Charts available")
    else()
        set(QT_CHARTS_AVAILABLE FALSE)
        message(STATUS "Qt5 Charts not available")
    endif()
    
    if(Qt5Network_FOUND)
        set(QT_NETWORK_AVAILABLE TRUE)
        message(STATUS "Qt5 Network available")
    else()
        set(QT_NETWORK_AVAILABLE FALSE)
        message(STATUS "Qt5 Network not available")
    endif()
    
    if(Qt5Multimedia_FOUND)
        set(QT_MULTIMEDIA_AVAILABLE TRUE)
        message(STATUS "Qt5 Multimedia available")
    else()
        set(QT_MULTIMEDIA_AVAILABLE FALSE)
        message(STATUS "Qt5 Multimedia not available")
    endif()
    
    if(Qt5OpenGL_FOUND)
        set(QT_OPENGL_AVAILABLE TRUE)
        message(STATUS "Qt5 OpenGL available")
    else()
        set(QT_OPENGL_AVAILABLE FALSE)
        message(STATUS "Qt5 OpenGL not available")
    endif()
endif()

# Set common variables
set(QT_VERSION ${QT_VERSION_MAJOR})
set(QT_MAJOR_VERSION ${QT_VERSION_MAJOR})
