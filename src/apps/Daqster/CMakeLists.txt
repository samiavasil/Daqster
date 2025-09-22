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

# Automatic linking for all dependencies
link_component_dependencies(Daqster)

# Install executable
install(TARGETS Daqster
    RUNTIME DESTINATION bin
)
