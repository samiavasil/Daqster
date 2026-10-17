# Application-object launchers (REQ-SW-PL-072)
#
# The application object is the only thing that genuinely needs QtWidgets in the
# whole runtime. To keep NodeRunner itself widget-free (so the headless process
# never even links the widget stack) that object is created by two tiny
# dlopen()ed libraries exporting the same C entry point, daqster_create_application():
#
#   DaqsterLauncherHeadless - Qt::Gui only            -> QGuiApplication
#   DaqsterLauncherGui      - Qt::Gui + Qt::Widgets   -> QApplication
#
# NodeRunner picks one at runtime (--headless) and loads exactly that one, so the
# Qt modules present in the process are decided at runtime, not at link time.
# Both are QGuiApplication-based on purpose: the video pipeline's hardware-GL
# detection requires QGuiApplication::instance(), so GPU/GL optimisations work
# in headless and GUI mode alike.
#
# Output/rpath: libraries land in ${CMAKE_BINARY_DIR}/bin (see the root
# CMAKE_LIBRARY_OUTPUT_DIRECTORY), which is also on NodeRunner's build RPATH;
# installed they go to lib/ and are found through $ORIGIN/../lib.

add_library(DaqsterLauncherHeadless SHARED
    headless/AppLauncher.cpp
)
target_include_directories(DaqsterLauncherHeadless PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(DaqsterLauncherHeadless PRIVATE
    Qt${QT_VERSION_MAJOR}::Gui
)
set_target_properties(DaqsterLauncherHeadless PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

install(TARGETS DaqsterLauncherHeadless
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

if(DAQSTER_BUILD_GUI)
    add_library(DaqsterLauncherGui SHARED
        gui/AppLauncher.cpp
    )
    target_include_directories(DaqsterLauncherGui PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_link_libraries(DaqsterLauncherGui PRIVATE
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
    )
    set_target_properties(DaqsterLauncherGui PROPERTIES
        POSITION_INDEPENDENT_CODE ON
    )

    # Windows: export the C entry point (explicit DAQSTER_APP_LAUNCHER_EXPORT is
    # already on the declaration; keep the all-symbols safety net as elsewhere).
    if(WIN32)
        set_target_properties(DaqsterLauncherGui PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
    endif()

    install(TARGETS DaqsterLauncherGui
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
endif()

message(STATUS "Created launcher libraries: DaqsterLauncherHeadless${DAQSTER_BUILD_GUI} (+/DaqsterLauncherGui)")
