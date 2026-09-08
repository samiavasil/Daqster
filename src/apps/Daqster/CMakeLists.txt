# Use template system for application
create_application(Daqster
    SOURCES
        main.cpp
        main.h
        ApplicationsManager.cpp
        ApplicationsManager.h
        AppToolbar.cpp
        AppToolbar.h
        AppSelectionDialog.cpp
        AppSelectionDialog.h
        mainwindow.ui
        icons.qrc
        QConsoleListener.cpp
        QConsoleListener.h
        StyleFile
    REQUIRES_LIBRARIES
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Gui
        DaqsterGui
)

#add_definitions(-D_STDSTREAM_DEFINED)

target_include_directories(Daqster
PRIVATE
    ./
    ${DAQSTER_SOURCE_DIR}/src/core
    ${DAQSTER_SOURCE_DIR}/src/gui
)