# ── Plugin JSON metadata (generated from template) ──────────────────
# REQ-SW-PL-035: the plugin metadata JSON is generated from a .json.in
# template with @DAQSTER_VERSION@ substituted (single source of truth:
# root VERSION file). Must run BEFORE create_plugin() so the JSON exists
# when moc resolves Q_PLUGIN_METADATA FILE.
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/PluginFancyTest.json.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/PluginFancyTest.json"
    @ONLY
)

# Use template system for plugin
create_plugin(plugin_fancy_test
    SOURCES
        PluginFancyInterface.cpp
        PluginFancyObject.cpp
        PluginFancyInterface.h
        PluginFancyObject.h
        fancy_test.qrc
    REQUIRES_LIBRARIES
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        frame_work
)
