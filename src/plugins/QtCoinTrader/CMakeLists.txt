find_package(Qt5 COMPONENTS Core Gui Qml QuickControls2 Charts Network OpenGL REQUIRED)
find_package(ssl)
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
    crypto
    ssl
    frame_work
    qtrest_lib
    Qt5::Core
    Qt5::Qml
    Qt5::QuickControls2
    Qt5::Charts
    Qt5::Network
#   Qt5::OpenGL
    )

# Install plugin to dedicated directory
install(TARGETS QtCoinTraderPlugin
    RUNTIME DESTINATION bin/plugins
    LIBRARY DESTINATION lib/daqster/plugins
    ARCHIVE DESTINATION lib
)
