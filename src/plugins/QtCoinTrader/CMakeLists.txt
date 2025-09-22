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
# Automatic linking for all dependencies
link_component_dependencies(QtCoinTraderPlugin)

# Conditionally link Charts if available
if(Qt${QT_VERSION_MAJOR}Charts_FOUND)
    target_link_libraries(QtCoinTraderPlugin Qt${QT_VERSION_MAJOR}::Charts)
    target_compile_definitions(QtCoinTraderPlugin PRIVATE QT_CHARTS_AVAILABLE)
    message(STATUS "QtCoinTraderPlugin: Charts support enabled")
else()
    message(STATUS "QtCoinTraderPlugin: Charts support disabled (Qt${QT_VERSION_MAJOR}Charts not found)")
endif()

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
