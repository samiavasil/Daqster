# Shared library for common plugin infrastructure (REQ-SW-PL-032).
#
# Extracted so nodeditor_nodes/core, its gui sibling, private plugins
# (ai_studio_plugin) and their test binaries can share ONE definition of
# TexturePool — the same pattern as NodeEditorLibrary
# (node_editor_ide/CMakeLists.txt).
#
# Why a library and not a header: TexturePool is a process-wide singleton of GL
# texture names. Every copy would own a separate pool, so textures handed back
# to one copy while another still believes them live is exactly the class of
# use-after-free that crashed the pre-split build at startup. On Linux the
# inline function-local statics happened to be coalesced by STB_GNU_UNIQUE; on
# Windows they are not. The library makes the single-definition property
# explicit instead of linker-dependent.
#
# VideoGLContextManager.h stays header-only on purpose — it is small, and it
# must be visible (not linked) so the lazy-creation logic is inlined into every
# consumer and the QOpenGLContext type does not leak into the library's ABI.

cmake_minimum_required(VERSION 3.16)

project(daqster_common LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

register_component(DaqsterCommonLibrary
    REQUIRES_LIBRARIES
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::OpenGL
)

get_property(DAQSTER_COMMON_LIB_ENABLED GLOBAL PROPERTY COMPONENT_DaqsterCommonLibrary_ENABLED)

if(DAQSTER_COMMON_LIB_ENABLED)
    add_library(DaqsterCommonLibrary SHARED
        GL/TexturePool.cpp
    )

    # Windows: WINDOWS_EXPORT_ALL_SYMBOLS makes the linker export all symbols
    # and emit the import library (.lib) — without it the DLL exports nothing
    # and consumers fail with LNK1181. No-op on Linux/macOS (.so/.dylib export
    # everything by default).
    set_target_properties(DaqsterCommonLibrary PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS ON
    )

    # Export macro for Windows: define DAQSTERCOMMON_LIBRARY when building the
    # library so DaqsterCommonExport.h expands DAQSTERCOMMON_EXPORT to
    # __declspec(dllexport) (consumers do NOT define this macro, so it expands
    # to __declspec(dllimport) there). Global data symbols (e.g. the
    # TexturePool singleton) are NOT linkable from consumers through
    # WINDOWS_EXPORT_ALL_SYMBOLS alone — they must be marked explicitly.
    target_compile_definitions(DaqsterCommonLibrary PRIVATE DAQSTERCOMMON_LIBRARY)

    target_include_directories(DaqsterCommonLibrary PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    link_component_dependencies(DaqsterCommonLibrary)

    if(TARGET Qt${QT_VERSION_MAJOR}::OpenGL)
        target_link_libraries(DaqsterCommonLibrary PUBLIC Qt${QT_VERSION_MAJOR}::OpenGL)
    endif()

    set_target_properties(DaqsterCommonLibrary PROPERTIES
        OUTPUT_NAME "DaqsterCommonLibrary"
    )

    install(TARGETS DaqsterCommonLibrary
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )

    message(STATUS "Created internal library: DaqsterCommonLibrary")
else()
    get_property(REASONS GLOBAL PROPERTY COMPONENT_DaqsterCommonLibrary_REASONS)
    message(STATUS "Skipping library DaqsterCommonLibrary - dependencies not met:")
    foreach(REASON ${REASONS})
        message(STATUS "  - ${REASON}")
    endforeach()
endif()
