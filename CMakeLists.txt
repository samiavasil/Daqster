cmake_minimum_required(VERSION 3.1.0)

project(Daqster VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY  ${CMAKE_BINARY_DIR}/bin)


#set_target_properties(Daqster PROPERTIES LIBRARY_OUTPUT_DIRECTORY $<1:${PROJECT_SOURCE_DIR}/lib>)
#set_target_properties(Daqster PROPERTIES RUNTIME_OUTPUT_DIRECTORY $<1:${PROJECT_SOURCE_DIR}/bin>)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(CMAKE_VERSION VERSION_LESS "3.7.0")
    set(CMAKE_INCLUDE_CURRENT_DIR ON)
endif()

add_subdirectory(src/apps)
add_subdirectory(src/frame_work)
add_subdirectory(src/plugins)
add_subdirectory(src/external_libs/nodeeditor)
add_subdirectory(src/external_libs/qtrest_lib)

