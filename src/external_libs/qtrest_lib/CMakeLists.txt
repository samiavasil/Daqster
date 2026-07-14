cmake_minimum_required(VERSION 3.12)

project(QtRestLib CXX)

set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_AUTOMOC ON)

# qtrest supports both Qt5 and Qt6 using a unified target prefix.
if(NOT DEFINED QT_VERSION_MAJOR)
  find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Network Qml)
endif()

find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core Network Qml REQUIRED)
set(QT_PREFIX Qt${QT_VERSION_MAJOR})
message(STATUS "QtRest library: Qt${QT_VERSION_MAJOR} detected - using Qt${QT_VERSION_MAJOR} components")

# Configure and include upstream qtrest build.
set(BUILD_TYPE SHARED CACHE STRING "qtrest build type" FORCE)
set(WITH_QML_SUPPORT 1 CACHE STRING "qtrest qml support" FORCE)
set(BUILD_EXAMPLE 0 CACHE STRING "qtrest example build" FORCE)
add_subdirectory(qtrest)

# Cross-platform install fix: upstream qtrest only has Windows .dll/.lib rules
if(NOT WIN32 AND "${BUILD_TYPE}" STREQUAL "SHARED")
  # Create empty files so upstream file(INSTALL) commands don't abort
  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/qtrest/core/qtrest.dll" "")
  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/qtrest/core/qtrest.lib" "")
  # Install the actual Linux shared library
  install(TARGETS qtrest LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()

set(CPP_LIB_WRAPPER_SOURCE_FILES
  qtrest_lib.cpp
  api/Exchange/ExchangeApi.cpp
  api/Exchange/model/ExchangeModel.cpp
)

set(HPP_LIB_WRAPPER_SOURCE_FILES
  qtrest_lib.h
  qtrest_lib_global.h
  api/Exchange/ExchangeApi.h
  api/Exchange/model/ExchangeModel.h
)

add_library(qtrest_lib SHARED
  ${CPP_LIB_WRAPPER_SOURCE_FILES}
  ${HPP_LIB_WRAPPER_SOURCE_FILES}
)

target_compile_definitions(qtrest_lib PRIVATE QTREST_LIB_LIBRARY WITH_QML_SUPPORT)

target_include_directories(qtrest_lib
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/qtrest/core>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/qtrest/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/qtrest/src/models>
)

target_link_libraries(qtrest_lib
  PUBLIC
    qtrest
    ${QT_PREFIX}::Qml
)

#target_compile_definitions(nodes
#  PUBLIC
#    ${Qt5Widgets_DEFINITIONS}
#    NODE_EDITOR_SHARED
#  PRIVATE
#    NODE_EDITOR_EXPORTS
#    #NODE_DEBUG_DRAWING
#    QT_NO_KEYWORDS
#)

#target_compile_options(nodes
#  PRIVATE
#    $<$<CXX_COMPILER_ID:MSVC>:/W4 /wd4127 /EHsc>
#    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra>
#    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra>
#)

#target_compile_features(nodes
#  PUBLIC
#    cxx_generic_lambdas # Require C++14
#)

set_target_properties(qtrest_lib
  PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

include(GNUInstallDirs)

install(TARGETS qtrest_lib
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

