# Use Qt detection from parent scope
find_package(OpenSSL REQUIRED)
#add_subdirectory(external)

add_library(QtCoinTraderPlugin
SHARED
    QtCoinTraderInterface.cpp
    QtCoinTraderPluginObject.cpp
    utils/RandData.cpp
    QtCoinTraderInterface.h
    QtCoinTraderPluginObject.h
    utils/RandData.h
    QtCoinTrader.qrc
    qml/qml_example/qml.qrc
    assets/assets.qrc
    QtCoinTraderInterface.json
)

add_definitions(-DBUILD_AVAILABLE_PLUGIN)

target_include_directories(QtCoinTraderPlugin
  PRIVATE

  )
target_link_libraries(QtCoinTraderPlugin
    OpenSSL::SSL
    OpenSSL::Crypto
    frame_work
    qtrest_lib
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Qml
    Qt${QT_VERSION_MAJOR}::Quick
    Qt${QT_VERSION_MAJOR}::QuickControls2
    Qt${QT_VERSION_MAJOR}::Charts
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::OpenGL
    )

# Install plugin to dedicated directory
install(TARGETS QtCoinTraderPlugin
    RUNTIME DESTINATION bin/plugins
    LIBRARY DESTINATION lib/daqster/plugins
    ARCHIVE DESTINATION lib
)

# Ensure plugin can locate shared libs in ../.. /lib relative to its location
set_target_properties(QtCoinTraderPlugin PROPERTIES
    INSTALL_RPATH "$ORIGIN/../../lib"
)
