cmake_minimum_required(VERSION 3.2)
# version 3.4 is required as other do not work with C++14 and clang

project(QtRestLib CXX)

set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
set(CMAKE_DISABLE_SOURCE_CHANGES  ON)

get_directory_property(_has_parent PARENT_DIRECTORY)
if(_has_parent)
  set(is_root_project OFF)
else()
  set(is_root_project ON)
endif()

set(NE_DEVELOPER_DEFAULTS "${is_root_project}" CACHE BOOL "Turns on default settings for development of QtRestLib")

#option(BUILD_TESTING "Build tests" "${NE_DEVELOPER_DEFAULTS}")
#option(BUILD_EXAMPLES "Build Examples" "${NE_DEVELOPER_DEFAULTS}")
#option(BUILD_SHARED_LIBS "Build as shared library" ON)
#option(NE_FORCE_TEST_COLOR "Force colorized unit test output" OFF)

#enable_testing()

if(NE_DEVELOPER_DEFAULTS)
  set(CMAKE_CXX_STANDARD 14)
  set(CMAKE_CXX_EXTENSIONS OFF)

  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")
endif()

## Find the QtWidgets library
find_package(Qt5 5.3 COMPONENTS
             Network
             Core
             Widgets
             Gui
             Qml
#            QuickControls2
         )

add_definitions(-DQTREST_LIB_LIBRARY)
##qt5_add_resources(RESOURCES ./resources/resources.qrc)

## Unfortunately, as we have a split include/src, AUTOMOC doesn't work.
## We'll have to manually specify some files
set(CMAKE_AUTOMOC ON)

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


set(CPP_SOURCE_FILES
    qtrest/src/apibase.cpp
    qtrest/src/models/baserestlistmodel.cpp
    qtrest/src/models/detailsmodel.cpp
    qtrest/src/models/restitem.cpp
    qtrest/src/models/pagination.cpp
    qtrest/src/models/abstractjsonrestlistmodel.cpp
    qtrest/src/models/abstractxmlrestlistmodel.cpp
    qtrest/src/models/jsonrestlistmodel.cpp
    qtrest/src/models/requests.cpp
    qtrest/src/models/xmlrestlistmodel.cpp
)

set(HPP_SOURCE_FILES
    qtrest/src/apibase.h
    qtrest/src/models/baserestlistmodel.h
    qtrest/src/models/detailsmodel.h
    qtrest/src/models/restitem.h
    qtrest/src/usingleton.h
    qtrest/src/models/pagination.h
    qtrest/src/models/abstractjsonrestlistmodel.h
    qtrest/src/models/abstractxmlrestlistmodel.h
    qtrest/src/models/jsonrestlistmodel.h
    qtrest/src/models/requests.h
    qtrest/src/models/xmlrestlistmodel.h
    )

set(RESOURCE_FILES
    qtrest/com_github_kafeg_qtrest.qrc
    )

set(OTHER_FILES
    qtrest/README.md
    qtrest/LICENSE
    qtrest/docs/QtMicroRestFramework.qmodel
    )

add_library(qtrest_lib
SHARED
    ${CPP_LIB_WRAPPER_SOURCE_FILES}
    ${HPP_LIB_WRAPPER_SOURCE_FILES}
    ${CPP_SOURCE_FILES}
    ${HPP_SOURCE_FILES}
    ${RESOURCES}
    ${OTHER_FILES}
    )

target_include_directories(qtrest_lib
PUBLIC
#    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/qtrest/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/qtrest/src/models>

#    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/nodes/internal>
)

target_link_libraries(qtrest_lib
  PUBLIC
    Qt5::Core
    Qt5::Network
    Qt5::Qml
#   Qt5::QuickControls2
#   Qt5::Widgets
#   Qt5::Gui
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

#######
## Moc
###

#file(GLOB_RECURSE HEADERS_TO_MOC include/nodes/internal/*.hpp)

#qt5_wrap_cpp(nodes_moc
#    ${HEADERS_TO_MOC}
#  TARGET nodes
#  OPTIONS --no-notes # Don't display a note for the headers which don't produce a moc_*.cpp
#)

#target_sources(nodes PRIVATE ${nodes_moc})

############
## Examples
###

#if(BUILD_EXAMPLES)
#  add_subdirectory(examples)
#endif()

###################
## Automated Tests
###

#if(BUILD_TESTING)
#  add_subdirectory(test)
#endif()

################
## Installation
###

include(GNUInstallDirs)

install(TARGETS qtrest_lib
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

