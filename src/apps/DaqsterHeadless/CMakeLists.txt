cmake_minimum_required(VERSION 3.16)

project(DaqsterHeadless VERSION 0.3.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

add_executable(DaqsterHeadless
    main.cpp
)

target_link_libraries(DaqsterHeadless PRIVATE
    DaqsterCore
    Qt${QT_VERSION_MAJOR}::Core
)

# Install
install(TARGETS DaqsterHeadless
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

message(STATUS "Created executable: DaqsterHeadless (QtCore only)")