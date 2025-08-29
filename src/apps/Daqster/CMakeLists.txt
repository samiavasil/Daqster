find_package(Qt5 COMPONENTS Widgets  REQUIRED)
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
    Qt5::Widgets
    frame_work
    )

# Install executable
install(TARGETS Daqster
    RUNTIME DESTINATION bin
)
