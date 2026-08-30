# ── Plugin JSON metadata (generated from template) ──────────────────
# REQ-SW-PL-035: the plugin metadata JSON is generated from a .json.in
# template with @DAQSTER_VERSION@ substituted (single source of truth:
# root VERSION file). Must run BEFORE create_plugin() so the JSON exists
# when moc resolves Q_PLUGIN_METADATA FILE.
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/QtCoinTraderInterface.json.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/QtCoinTraderInterface.json"
    @ONLY
)

# Use template system for plugin
create_plugin(QtCoinTraderPlugin
    SOURCES
        QtCoinTraderInterface.cpp
        QtCoinTraderPluginObject.cpp
        utils/RandData.cpp
        QtCoinTraderInterface.h
        QtCoinTraderPluginObject.h
        utils/RandData.h
        QtCoinTrader.qrc
        qml/qml_example/qml.qrc
        assets/assets.qrc
        QtCoinTraderInterface.json
    REQUIRES_LIBRARIES
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Qml
        Qt${QT_VERSION_MAJOR}::QuickControls2
        Qt${QT_VERSION_MAJOR}::Charts
        Qt${QT_VERSION_MAJOR}::Network
        Qt${QT_VERSION_MAJOR}::OpenGL
        Qt${QT_VERSION_MAJOR}::WebSockets
        qtrest_lib
        OpenSSL::SSL
        OpenSSL::Crypto
        frame_work
)
