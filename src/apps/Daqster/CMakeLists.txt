# Use Qt detection from parent scope
add_executable(Daqster
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
)

#add_definitions(-D_STDSTREAM_DEFINED)

target_include_directories(Daqster
PRIVATE
    ./
)

target_link_libraries(Daqster
PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Gui
    frame_work
    )

# Install executable
install(TARGETS Daqster
    RUNTIME DESTINATION bin
)
