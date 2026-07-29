cmake_minimum_required(VERSION 3.12)

project(QtRestLib CXX)

set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_AUTOMOC ON)

# Bring in standard install directory variables (CMAKE_INSTALL_BINDIR, etc.)
# early so they are available when we set QTREST_INSTALL_* below.
include(GNUInstallDirs)

# qtrest supports both Qt5 and Qt6 using a unified target prefix.
if(NOT DEFINED QT_VERSION_MAJOR)
  find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Network Qml)
endif()

find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core Network Qml REQUIRED)
set(QT_PREFIX Qt${QT_VERSION_MAJOR})
message(STATUS "QtRest library: Qt${QT_VERSION_MAJOR} detected - using Qt${QT_VERSION_MAJOR} components")

# Override upstream qtrest install destination variables with relative paths so
# that cmake_install.cmake evaluates ${CMAKE_INSTALL_PREFIX} at install time
# instead of hardcoding the configure-time default (/usr/local or
# C:/Program Files/...).
set(QTREST_INSTALL_BIN_DIR     "${CMAKE_INSTALL_BINDIR}")
set(QTREST_INSTALL_LIB_DIR     "${CMAKE_INSTALL_LIBDIR}")
set(QTREST_INSTALL_INCLUDE_DIR "${CMAKE_INSTALL_INCLUDEDIR}/qtrest")

# Configure upstream qtrest build options.
set(BUILD_TYPE SHARED CACHE STRING "qtrest build type" FORCE)
set(WITH_QML_SUPPORT 1 CACHE STRING "qtrest qml support" FORCE)
set(BUILD_EXAMPLE 0 CACHE STRING "qtrest example build" FORCE)

# Temporarily clear the project-wide output directory overrides so that the
# upstream qtrest target places its shared library in its own CMake binary
# directory (build/.../qtrest/core/). The upstream cmake_install.cmake
# references exactly that path for qtrest.dll / libqtrest.so.
set(_qtrest_saved_runtime "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
set(_qtrest_saved_library "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
unset(CMAKE_RUNTIME_OUTPUT_DIRECTORY)
unset(CMAKE_LIBRARY_OUTPUT_DIRECTORY)

add_subdirectory(qtrest)

# Restore project-wide output directory settings for subsequent targets.
# Use DEFINED check (not truthiness) so an empty-string saved value is
# restored as "" rather than incorrectly triggering unset().
if(DEFINED _qtrest_saved_runtime)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${_qtrest_saved_runtime}")
else()
  unset(CMAKE_RUNTIME_OUTPUT_DIRECTORY)
endif()
if(DEFINED _qtrest_saved_library)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${_qtrest_saved_library}")
else()
  unset(CMAKE_LIBRARY_OUTPUT_DIRECTORY)
endif()

# On Windows with MinGW the shared library is created as libqtrest.dll (MinGW
# prefix convention) but the upstream cmake_install.cmake expects qtrest.dll.
# Remove the prefix so the build artifact name matches the install rule.
if(WIN32)
  set_target_properties(qtrest PROPERTIES PREFIX "")
endif()

# Create empty placeholder files for the Windows-only artifacts referenced by
# the upstream cmake_install.cmake install(FILES) rules. The upstream adds these
# rules unconditionally (no platform guard), so they run on Linux/macOS too.
# Without the placeholders, cmake --install aborts with "file INSTALL cannot find".
#   - qtrest.dll: overwritten by the real DLL on Windows; remains empty on Linux/macOS
#   - qtrest.lib: overwritten on MSVC; MinGW uses .dll.a so it stays empty there too
# Use if(NOT EXISTS) to avoid zeroing out files already produced by a prior build.
if(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/qtrest/core/qtrest.dll")
  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/qtrest/core/qtrest.dll" "")
endif()
if(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/qtrest/core/qtrest.lib")
  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/qtrest/core/qtrest.lib" "")
endif()

# Install the qtrest shared library. On Windows the upstream cmake_install.cmake
# already handles qtrest.dll via install(FILES). On non-Windows we add an
# explicit install(TARGETS) because the upstream only has Windows-specific rules.
if(NOT WIN32)
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
