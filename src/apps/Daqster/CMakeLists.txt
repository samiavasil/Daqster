# Use template system for application
create_application(Daqster
    SOURCES
        main.cpp
        main.h
        ApplicationsManager.cpp
        ApplicationsManager.h
        AppToolbar.cpp
        AppToolbar.h
        mainwindow.ui
        icons.qrc
        QConsoleListener.cpp
        QConsoleListener.h
        StyleFile
    REQUIRES_LIBRARIES
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Gui
        frame_work
)

#add_definitions(-D_STDSTREAM_DEFINED)

target_include_directories(Daqster
PRIVATE
    ./
)
