find_package(Qt5 COMPONENTS core gui qml QuickControls2 charts OpenGL REQUIRED)
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
    frame_work
    qtrest_lib
    Qt5::Core
    Qt5::Qml
    Qt5::QuickControls2
    Qt5::Charts
    Qt5::OpenGL
    )
