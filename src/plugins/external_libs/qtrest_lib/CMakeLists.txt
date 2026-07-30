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

# Include GNUInstallDirs BEFORE the qtrest submodule so install paths
# use CMAKE_INSTALL_LIBDIR/BINDIR (relative) instead of hardcoded prefix.
include(GNUInstallDirs)

# Override qtrest submodule's install variables to use relative paths
# so cmake --install --prefix <dir> works correctly.
set(QTREST_INSTALL_LIB_DIR ${CMAKE_INSTALL_LIBDIR} CACHE STRING "qtrest lib install dir")
set(QTREST_INSTALL_BIN_DIR ${CMAKE_INSTALL_BINDIR} CACHE STRING "qtrest bin install dir")

# Configure and include upstream qtrest build.
set(BUILD_TYPE SHARED CACHE STRING "qtrest build type" FORCE)
set(WITH_QML_SUPPORT 1 CACHE STRING "qtrest qml support" FORCE)
set(BUILD_EXAMPLE 0 CACHE STRING "qtrest example build" FORCE)
add_subdirectory(qtrest)

# Ensure qtrest target outputs to parent-controlled directories
if(TARGET qtrest)
    set_target_properties(qtrest PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
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

set_target_properties(qtrest_lib
  PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

install(TARGETS qtrest_lib
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
