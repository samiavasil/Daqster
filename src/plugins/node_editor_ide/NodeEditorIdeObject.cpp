#include "NodeEditorIdeObject.h"
#include "NodeEditorWidget.h"
#include "QPluginManager.h"
#include "capabilities/INodeProvider.h"
#include "debug.h"
#include "LogCategories.h"
#include "RuntimeShell.h"

#include <QMainWindow>
#include <QLabel>
#include <QLayout>
#include <QPushButton>
#include <QMenu>
#include <QMenuBar>
#include <QAction>
#include <QCheckBox>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QSet>
#include <QDir>
#include <QKeyEvent>

#include <exception>

#include <QtNodes/NodeDelegateModel>
#include <QtNodes/NodeDelegateModelRegistry>
#include <QtNodes/DataFlowGraphModel>
#include <QtNodes/DataFlowGraphicsScene>
#include <QtNodes/ConnectionStyle>
#include <QtNodes/internal/NodeGraphicsObject.hpp>
#include <QtNodes/internal/NodeGroup.hpp>
#include <QtNodes/internal/GroupGraphicsObject.hpp>

#include <QtWidgets/QVBoxLayout>

#include "NumberSourceDataModel.h"
#include "NumberDisplayDataModel.h"
#include "ModuloModel.h"
#include "ArithmeticLogicModel.h"

static void setStyle()
{
    QtNodes::ConnectionStyle::setConnectionStyle(
        R"(
        {
        "ConnectionStyle": {
        "ConstructionColor": "gray",
        "NormalColor": "black",
        "SelectedColor": "gray",
        "SelectedHaloColor": "deepskyblue",
        "HoveredColor": "deepskyblue",

        "LineWidth": 3.0,
        "ConstructionLineWidth": 2.0,
        "PointDiameter": 10.0,

        "UseDataDefinedColors": true
        }
        }
        )");
}

NodeEditorIdeObject::NodeEditorIdeObject(QObject* Parent)
    : Daqster::QBasePluginObject(Parent)
    , m_Win(nullptr)
    , m_Widget(nullptr)
{
}

NodeEditorIdeObject::~NodeEditorIdeObject()
{
    DeInitialize();
    if (m_runtimeShell) {
        m_runtimeShell->deleteLater();
        m_runtimeShell = nullptr;
    }
}

void NodeEditorIdeObject::SetName(const QString& name)
{
    if (nullptr != m_Win) {
        m_Win->setWindowTitle(name);
    }
}

bool NodeEditorIdeObject::Initialize()
{
    m_Win = new QMainWindow();
    QWidget* mainWidget = new QWidget(m_Win);
    m_Win->setCentralWidget(mainWidget);
    QVBoxLayout* l = new QVBoxLayout(mainWidget);

    QLabel* label = new QLabel();
    label->setText("Node Editor IDE");
    QPushButton* button = new QPushButton(m_Win);
    l->addWidget(label);
    l->addWidget(button);

    setStyle();

    m_Widget = new NodeEditorWidget(mainWidget);

    // Phase 1+2: Register built-in nodes + external INodeProvider plugins
    registerNodes();

    // Build canvas AFTER all nodes are registered
    m_Widget->buildCanvas();

    // ── File menu (REQ-SW-PL-037): Save/Load scene ─────────────────────────
    // Save uses saveSceneToFile() (REQ-SW-PL-049): graph model JSON + groups +
    // the "ui" section (runtime layout). Load uses the tolerant path that
    // skips unregistered node types instead of crashing.
    QMenu* fileMenu = m_Win->menuBar()->addMenu(tr("&File"));

    QAction* saveAction = fileMenu->addAction(tr("Save Scene…"));
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, [this]() {
        saveSceneToFile();
    });

    QAction* loadAction = fileMenu->addAction(tr("Load Scene…"));
    loadAction->setShortcut(QKeySequence::Open);
    connect(loadAction, &QAction::triggered, this, [this]() {
        loadSceneTolerant();
    });

    l->addWidget(m_Widget);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(0);

    m_Win->resize(1024, 768);
    m_Win->show();
    m_Win->setAttribute(Qt::WA_DeleteOnClose, true);

    // Dev driver (DAQSTER_AUTOSTART_VIDEO=1): builds a video source -> output
    // graph and starts playback without GUI interaction. Used by the PERF
    // measurement harness (tests/performance/performance-video-display-2026-08-13.md).
    if (qEnvironmentVariableIsSet("DAQSTER_AUTOSTART_VIDEO"))
        autoStartVideo();

    // Dev driver (DAQSTER_AUTOSTART_FLOW=<path>, REQ-SW-PL-038): loads a .flow
    // scene headlessly at startup (tolerant path — unregistered nodes are
    // skipped instead of crashing). If DAQSTER_VIDEO_FILE is also set, the
    // loaded VideoFileSource/VideoOutput nodes are configured and playback +
    // Perf are started (same logic as autoStartVideo).
    if (qEnvironmentVariableIsSet("DAQSTER_AUTOSTART_FLOW")) {
        if (loadSceneTolerant(qEnvironmentVariable("DAQSTER_AUTOSTART_FLOW"))
            && qEnvironmentVariableIsSet("DAQSTER_VIDEO_FILE")) {
            startVideoPlayback();
        }
    }

    connect(m_Widget, &NodeEditorWidget::nodeDoubleClicked,
            this, &NodeEditorIdeObject::nodeDoubleClicked);
    connect(m_Win, SIGNAL(destroyed(QObject*)), this, SLOT(MainWinDestroyed(QObject*)));
    connect(button, SIGNAL(clicked(bool)), this, SLOT(ShowPlugins()));

    // Install event filter for F11 presentation mode toggle (REQ-SW-PL-048)
    m_Win->installEventFilter(this);

    return true;
}

void NodeEditorIdeObject::registerBuiltInNodes()
{
    auto* registry = m_Widget->getInjectedRegistry();

    registry->registerModel<NumberSourceDataModel>("General/Sources");
    registry->registerModel<NumberDisplayDataModel>("General/Display");
    registry->registerModel<ModuloModel>("General/Processing");
    registry->registerModel<ArithmeticLogicModel>("General/Processing");
}

void NodeEditorIdeObject::registerNodes()
{
    registerBuiltInNodes();
    discoverAndRegisterExternalNodes();
}

// ── Runtime mode entry point (REQ-SW-PL-048) ────────────────────────────
// Loads a .flow file and shows deembedded node widgets as the application UI
// with the editor canvas hidden. Delegates to RuntimeShell which handles MDI
// layout, autoStart via IStartable, and shutdown via IStoppable.
bool NodeEditorIdeObject::RunRuntime(const QString& flowPath)
{
    if (flowPath.isEmpty()) {
        QMessageBox::critical(nullptr, tr("Runtime Mode"), tr("No flow file specified."));
        return false;
    }

    if (!QFile::exists(flowPath)) {
        QMessageBox::critical(nullptr, tr("Runtime Mode"),
                              tr("Flow file not found: %1").arg(flowPath));
        return false;
    }

    // Create RuntimeShell instance (owned by this object, deleted in destructor)
    m_runtimeShell = new RuntimeShell(this);
    return m_runtimeShell->RunRuntime(flowPath);
}

void NodeEditorIdeObject::discoverAndRegisterExternalNodes()
{
    Daqster::QPluginManager* pm = Daqster::QPluginManager::instance();
    if (!pm) return;

    QObjectList providers = pm->instances(INodeProvider_IID);
    auto* registry = m_Widget->getInjectedRegistry();

    for (QObject* obj : providers) {
        auto* provider = qobject_cast<Daqster::INodeProvider*>(obj);
        if (!provider) continue;

        QString name = obj->property("name").toString();
        DEBUG << "Discovered INodeProvider plugin:" << name;

        provider->registerNodes(*registry);
    }
}

void NodeEditorIdeObject::nodeDoubleClicked(QtNodes::NodeId nodeId)
{
    Q_UNUSED(nodeId);
    QMenu menu;
    QAction* removeAction = menu.addAction("Laa");
    QAction* markAction = menu.addAction("Daa");

    QAction* selectedAction = menu.exec();
    if (selectedAction == markAction) {
        qCDebug(lcNodeEditor) << "Laa";
    } else if (selectedAction == removeAction) {
        qCDebug(lcNodeEditor) << "Daa";
    }
}

void NodeEditorIdeObject::DeInitialize()
{
    if (nullptr != m_Win) {
        m_Win->deleteLater();
    }
    if (m_runtimeShell) {
        m_runtimeShell->deleteLater();
        m_runtimeShell = nullptr;
    }
    DEBUG_V << "NodeEditorIdeObject destroyed";
}

void NodeEditorIdeObject::MainWinDestroyed(QObject* obj)
{
    m_Win = nullptr;
    m_Widget = nullptr;
    deleteLater();
    if (nullptr == obj)
        DEBUG << "Strange::!!!";
}

void NodeEditorIdeObject::ShowPlugins()
{
    Daqster::QPluginManager* pm = Daqster::QPluginManager::instance();
    if (nullptr != pm) {
        DEBUG << "Plugin Manager: " << pm;
        pm->ShowPluginManagerGui(m_Win);
    }
}

// ── Presentation mode toggle (REQ-SW-PL-048) ──────────────────────────────────
// F11 key handler: hides GraphicsView + shows deembedded widgets (or arranges
// in MDI); toggles back: shows GraphicsView, hides deembedded widgets.
// Reversible — Pure Data style presentation mode.
bool NodeEditorIdeObject::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_Win && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_F11 && !keyEvent->isAutoRepeat()) {
            togglePresentationMode();
            return true; // Event handled
        }
    }
    return QObject::eventFilter(watched, event);
}

void NodeEditorIdeObject::togglePresentationMode()
{
    if (!m_Widget || !m_Widget->scene() || !m_Win)
        return;

    m_presentationMode = !m_presentationMode;

    if (m_presentationMode) {
        // Enter presentation mode: hide canvas, show deembedded widgets
        m_Widget->hide();

        // Deembed all nodes that have widgets and are not already deembedded
        for (const QtNodes::NodeId nodeId : m_Widget->graphModel()->allNodeIds()) {
            QtNodes::NodeGraphicsObject* node = m_Widget->scene()->nodeGraphicsObject(nodeId);
            if (node == nullptr || !node->hasWidget() || !node->isWidgetEmbedded())
                continue;

            auto* model = m_Widget->graphModel()->delegateModel<QtNodes::NodeDelegateModel>(nodeId);
            QWidget* w = model != nullptr ? model->embeddedWidget() : nullptr;
            if (w == nullptr)
                continue;

            // Two-step deembed: setWidgetEmbedded(false) FIRST
            node->setWidgetEmbedded(false);

            // Show as top-level window
            w->setWindowFlags(Qt::Window);
            w->setWindowTitle(model->caption());
            w->show();
        }

        qCInfo(lcNodeEditor) << "Presentation mode: ON (canvas hidden, deembedded widgets shown)";
    } else {
        // Exit presentation mode: show canvas, re-embed widgets
        m_Widget->show();

        // Re-embed all deembedded widgets
        for (const QtNodes::NodeId nodeId : m_Widget->graphModel()->allNodeIds()) {
            QtNodes::NodeGraphicsObject* node = m_Widget->scene()->nodeGraphicsObject(nodeId);
            if (node == nullptr || !node->hasWidget() || node->isWidgetEmbedded())
                continue;

            auto* model = m_Widget->graphModel()->delegateModel<QtNodes::NodeDelegateModel>(nodeId);
            QWidget* w = model != nullptr ? model->embeddedWidget() : nullptr;
            if (w == nullptr)
                continue;

            // Hide the top-level window first
            w->hide();

            // Re-embed: setWidgetEmbedded(true) will re-parent to the proxy widget
            node->setWidgetEmbedded(true);

            // Clear window flags
            w->setWindowFlags(Qt::Widget);
        }

        qCInfo(lcNodeEditor) << "Presentation mode: OFF (canvas shown, widgets re-embedded)";
    }
}

// ── Tolerant scene load (REQ-SW-PL-037) ─────────────────────────────────────
// QtNodes' DataFlowGraphModel::loadNode() throws std::logic_error when a saved
// scene references a model type that is not registered in the current
// environment (e.g. the providing plugin is not loaded). Instead of crashing,
// this method:
//   1. opens a .flow file dialog (or uses the path from DAQSTER_AUTOSTART_FLOW,
//      REQ-SW-PL-038),
//   2. parses the JSON scene,
//   3. drops nodes whose "internal-data"."model-name" is not registered,
//   4. drops connections referencing any dropped node id (no dangling edges),
//   5. loads the cleaned scene and warns the user about the skipped types.
bool NodeEditorIdeObject::loadSceneTolerant(const QString& fileName)
{
    QString path = fileName;
    if (path.isEmpty()) {
        path = QFileDialog::getOpenFileName(
            m_Win, tr("Open Flow Scene"), QDir::homePath(), tr("Flow Scene Files (*.flow)"));
        if (path.isEmpty())
            return false;
    }

    return loadSceneFromFile(path);
}

bool NodeEditorIdeObject::loadSceneFromFile(const QString& fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(lcNodeEditor) << "loadSceneFromFile: cannot open" << fileName;
        return false;
    }

    const QByteArray wholeFile = file.readAll();
    QJsonParseError parseError{};
    const QJsonDocument sceneDocument = QJsonDocument::fromJson(wholeFile, &parseError);
    if (parseError.error != QJsonParseError::NoError || !sceneDocument.isObject()) {
        qCWarning(lcNodeEditor) << "loadSceneFromFile: invalid JSON in" << fileName
                                << ":" << parseError.errorString();
        return false;
    }

    QJsonObject sceneJson = sceneDocument.object();

    // Extract the "ui" section (REQ-SW-PL-049) BEFORE the node-cleaning loop:
    // it is not part of the graph model JSON and must not be passed to load().
    // Missing "ui" (old flow) → empty section → current behavior.
    const FlowUi::UiSection uiSection = FlowUi::UiSection::fromJson(sceneJson["ui"].toObject());
    sceneJson.remove("ui");

    const QJsonArray nodesJsonArray = sceneJson["nodes"].toArray();

    auto* registry = m_Widget->getInjectedRegistry();
    const auto& creators = registry->registeredModelCreators();

    QStringList skippedTypes;
    QSet<QtNodes::NodeId> skippedNodeIds;

    QJsonArray cleanedNodes;
    for (const auto& nodeValue : nodesJsonArray) {
        const QJsonObject nodeJson = nodeValue.toObject();
        const QString modelName = nodeJson["internal-data"].toObject()["model-name"].toString();
        if (modelName.isEmpty() || creators.count(modelName) == 0) {
            skippedTypes << (modelName.isEmpty() ? tr("<unnamed>") : modelName);
            skippedNodeIds.insert(static_cast<QtNodes::NodeId>(nodeJson["id"].toInt()));
            continue;
        }
        cleanedNodes.append(nodeJson);
    }
    sceneJson["nodes"] = cleanedNodes;

    if (!skippedNodeIds.isEmpty()) {
        QJsonArray cleanedConnections;
        const QJsonArray connJsonArray = sceneJson["connections"].toArray();
        for (const auto& connValue : connJsonArray) {
            const QJsonObject connJson = connValue.toObject();
            const QtNodes::NodeId outNodeId =
                static_cast<QtNodes::NodeId>(connJson["outNodeId"].toInt());
            const QtNodes::NodeId inNodeId =
                static_cast<QtNodes::NodeId>(connJson["inNodeId"].toInt());
            if (skippedNodeIds.contains(outNodeId) || skippedNodeIds.contains(inNodeId))
                continue;
            cleanedConnections.append(connJson);
        }
        sceneJson["connections"] = cleanedConnections;
    }

    try {
        m_Widget->scene()->clearScene();
        m_Widget->graphModel()->load(sceneJson);
    } catch (const std::exception& e) {
        qCWarning(lcNodeEditor) << "loadSceneFromFile: load failed:" << e.what();
        return false;
    }

    // Restore the runtime UI layout captured in the "ui" section (REQ-SW-PL-049).
    applyUiSection(uiSection);

    const int loadedNodeCount = static_cast<int>(m_Widget->graphModel()->allNodeIds().size());
    const int loadedConnCount = static_cast<int>(
        sceneJson["connections"].toArray().size());
    qCInfo(lcNodeEditor) << "loadSceneFromFile: loaded" << fileName
                         << "nodes=" << loadedNodeCount
                         << "connections=" << loadedConnCount;

    if (!skippedTypes.isEmpty()) {
        qCWarning(lcNodeEditor) << "loadSceneFromFile: skipped unregistered node types:"
                                << skippedTypes.join(QStringLiteral(", "));
        QMessageBox::warning(m_Win,
                             tr("Load Scene"),
                             tr("The following node types are not registered in this "
                                "environment and were skipped:\n%1")
                                 .arg(skippedTypes.join(QLatin1Char('\n'))));
    }

    return true;
}

// ── Save with "ui" section (REQ-SW-PL-049) ──────────────────────────────────
// Saves the scene as graph model JSON + groups (byte-identical to
// DataFlowGraphicsScene::save()) + the "ui" section describing the runtime
// layout of deembedded node widgets. The "ui" section is written indented so
// it is human-inspectable in the .flow file.
bool NodeEditorIdeObject::saveSceneToFile()
{
    if (m_Widget == nullptr || m_Widget->scene() == nullptr) {
        qCWarning(lcNodeEditor) << "saveSceneToFile: no scene to save";
        return false;
    }

    QString fileName = QFileDialog::getSaveFileName(
        m_Win, tr("Save Flow Scene"), QDir::homePath(), tr("Flow Scene Files (*.flow)"));
    if (fileName.isEmpty())
        return false;
    if (!fileName.endsWith("flow", Qt::CaseInsensitive))
        fileName += ".flow";

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        qCWarning(lcNodeEditor) << "saveSceneToFile: cannot open" << fileName;
        return false;
    }

    QJsonObject sceneJson = m_Widget->graphModel()->save();

    // Replicate DataFlowGraphicsScene::save() groups serialization
    // (byte-identical): groups()/name()/nodeIDs()/groupGraphicsObject().locked()
    // are all public API.
    QJsonArray groupsJsonArray;
    for (const auto& [groupId, groupPtr] : m_Widget->scene()->groups()) {
        if (!groupPtr)
            continue;

        QJsonObject groupJson;
        groupJson["id"] = static_cast<qint64>(groupId);
        groupJson["name"] = groupPtr->name();

        QJsonArray nodeIdsJson;
        for (const QtNodes::NodeId nodeId : groupPtr->nodeIDs()) {
            nodeIdsJson.append(static_cast<qint64>(nodeId));
        }
        groupJson["nodes"] = nodeIdsJson;
        groupJson["locked"] = groupPtr->groupGraphicsObject().locked();

        groupsJsonArray.append(groupJson);
    }
    if (!groupsJsonArray.isEmpty()) {
        sceneJson["groups"] = groupsJsonArray;
    }

    sceneJson["ui"] = captureUiSection().toJson();

    file.write(QJsonDocument(sceneJson).toJson(QJsonDocument::Indented));
    qCInfo(lcNodeEditor) << "saveSceneToFile: saved" << fileName;
    return true;
}

FlowUi::UiSection NodeEditorIdeObject::captureUiSection() const
{
    FlowUi::UiSection ui;
    ui.version = 1;

    // Workspaces: stored layout (from a loaded "ui" section) or the main
    // window default {id:0, tabbed:true, geometry: m_Win->geometry()+maximized}.
    if (m_workspaces.empty()) {
        FlowUi::WorkspaceUi ws;
        ws.id = 0;
        ws.tabbed = true;
        if (m_Win != nullptr) {
            const QRect geo = m_Win->geometry();
            ws.geometry.x = geo.x();
            ws.geometry.y = geo.y();
            ws.geometry.w = geo.width();
            ws.geometry.h = geo.height();
            ws.geometry.maximized = m_Win->isMaximized();
        }
        ui.workspaces.push_back(ws);
    } else {
        ui.workspaces = m_workspaces;
    }

    if (m_Widget == nullptr || m_Widget->scene() == nullptr)
        return ui;

    // Per-node layout: ALL nodes get an entry; geometry only for deembedded.
    for (const QtNodes::NodeId nodeId : m_Widget->graphModel()->allNodeIds()) {
        QtNodes::NodeGraphicsObject* node = m_Widget->scene()->nodeGraphicsObject(nodeId);
        if (node == nullptr || !node->hasWidget())
            continue;

        FlowUi::NodeUi nui;
        nui.deembedded = !node->isWidgetEmbedded();
        nui.workspace = 0;
        nui.autoStart = m_autoStartNodes.value(nodeId, false);

        if (nui.deembedded) {
            auto* model =
                m_Widget->graphModel()->delegateModel<QtNodes::NodeDelegateModel>(nodeId);
            QWidget* w = model != nullptr ? model->embeddedWidget() : nullptr;
            if (w != nullptr) {
                const QRect geo = w->geometry();
                nui.geometry.x = geo.x();
                nui.geometry.y = geo.y();
                nui.geometry.w = geo.width();
                nui.geometry.h = geo.height();
                nui.geometry.maximized = w->isMaximized();
            }
        }

        ui.nodes.insert(nodeId, nui);
    }

    return ui;
}

void NodeEditorIdeObject::applyUiSection(const FlowUi::UiSection& ui)
{
    // Workspaces are stored (not applied) — the MDI workspace shell is a
    // runtime-mode concern (REQ-SW-PL-048); the IDE keeps the layout for the
    // next save.
    m_workspaces = ui.workspaces;

    if (m_Widget == nullptr || m_Widget->scene() == nullptr)
        return;

    for (auto it = ui.nodes.constBegin(); it != ui.nodes.constEnd(); ++it) {
        const QtNodes::NodeId nodeId = it.key();
        const FlowUi::NodeUi& nui = it.value();

        // Tolerant load guard: nodes skipped by loadSceneFromFile() (missing
        // model type) are not in the graph — ignore their ui entry.
        if (!m_Widget->graphModel()->nodeExists(nodeId))
            continue;

        m_autoStartNodes.insert(nodeId, nui.autoStart);

        if (!nui.deembedded)
            continue;

        QtNodes::NodeGraphicsObject* node = m_Widget->scene()->nodeGraphicsObject(nodeId);
        if (node == nullptr || !node->isWidgetEmbedded())
            continue;

        // Direct call is safe here — no context-menu loop is open during load.
        node->setWidgetEmbedded(false);

        auto* model =
            m_Widget->graphModel()->delegateModel<QtNodes::NodeDelegateModel>(nodeId);
        QWidget* w = model != nullptr ? model->embeddedWidget() : nullptr;
        if (w == nullptr)
            continue;

        w->setWindowState(Qt::WindowNoState);
        w->setGeometry(QRect(nui.geometry.x, nui.geometry.y,
                             nui.geometry.w, nui.geometry.h));
        if (nui.geometry.maximized)
            w->setWindowState(Qt::WindowMaximized);
    }
}

// ── Dev driver: DAQSTER_AUTOSTART_VIDEO=1 ────────────────────────────────────
// Builds a video source -> VideoOutput graph, starts playback from
// DAQSTER_VIDEO_FILE / DAQSTER_STREAM_URL and enables the "Perf" checkbox, so
// the video pipeline can be measured headlessly (see the PERF results doc in
// tests/performance/). No-op unless the env var is set.
void NodeEditorIdeObject::autoStartVideo()
{
    QtNodes::DataFlowGraphModel* gm = m_Widget->graphModel();
    if (gm == nullptr) {
        DEBUG << "autoStartVideo: no graph model";
        return;
    }

    // DAQSTER_VIDEO_FILE (file source) takes precedence over DAQSTER_STREAM_URL
    // (rtsp/http stream source).
    const QString videoFile = qEnvironmentVariable("DAQSTER_VIDEO_FILE");
    const QString streamUrl = qEnvironmentVariable(
        "DAQSTER_STREAM_URL", QStringLiteral("rtsp://192.168.33.233:554/stream1"));
    const bool useFile = !videoFile.isEmpty();
    const QString srcNodeName = useFile
        ? QStringLiteral("VideoFileSource") : QStringLiteral("StreamSource");

    const QtNodes::NodeId srcId = gm->addNode(srcNodeName);
    const QtNodes::NodeId outId = gm->addNode(QStringLiteral("VideoOutput"));
    DEBUG << "autoStartVideo: nodes created src=" << srcId << " out=" << outId
          << " source=" << srcNodeName << (useFile ? videoFile : streamUrl);

    // Connect source port 0 -> output port 0 (video-frame on both Qt versions).
    const QtNodes::ConnectionId conn{srcId, 0, outId, 0};
    if (gm->connectionPossible(conn)) {
        gm->addConnection(conn);
        DEBUG << "autoStartVideo: connected " << srcNodeName << " -> VideoOutput";
    } else {
        DEBUG << "autoStartVideo: connection NOT possible";
    }

    // ── Dev driver: DAQSTER_AUTOSTART_EFFECT=<effectId> ─────────────────────
    // Inserts a VideoEffect node between the source and the output (REQ-SW-PL-028
    // smoke driver). The env value is the effect id ("brightness", "contrast",
    // "grayscale", "invert", "sepia", "channelSwap", "flip"). The node is the
    // single "VideoEffect" node with an effect combo; the effect is selected
    // through load() (same path a saved graph uses).
    // DAQSTER_AUTOSTART_EFFECT2=<effectId> inserts a SECOND effect after the
    // first — the GPU-resident chain smoke (REQ-SW-PL-032 Stage 2B): the second
    // effect consumes the first effect's texture directly (no upload/readback).
    QtNodes::NodeId prevId = srcId;
    const auto insertEffect = [&](const QString &effectId) {
        if (effectId.isEmpty())
            return;
        const QtNodes::NodeId effectNodeId = gm->addNode(QStringLiteral("VideoEffect"));
        DEBUG << "autoStartVideo: effect node created id=" << effectNodeId;

        auto *effectModel = gm->delegateModel<QtNodes::NodeDelegateModel>(effectNodeId);
        if (effectModel) {
            QJsonObject cfg;
            cfg[QStringLiteral("effect")] = effectId;
            effectModel->load(cfg);
            DEBUG << "autoStartVideo: effect set to " << effectId;
        } else {
            DEBUG << "autoStartVideo: effect model NOT available";
        }

        const QtNodes::ConnectionId oldConn{prevId, 0, outId, 0};
        if (gm->connectionExists(oldConn))
            gm->deleteConnection(oldConn);
        const QtNodes::ConnectionId inConn{prevId, 0, effectNodeId, 0};
        if (gm->connectionPossible(inConn)) {
            gm->addConnection(inConn);
            DEBUG << "autoStartVideo: connected prev -> effect";
        } else {
            DEBUG << "autoStartVideo: effect input connection NOT possible";
        }
        const QtNodes::ConnectionId outConn{effectNodeId, 0, outId, 0};
        if (gm->connectionPossible(outConn)) {
            gm->addConnection(outConn);
            DEBUG << "autoStartVideo: connected effect -> output";
        } else {
            DEBUG << "autoStartVideo: effect output connection NOT possible";
        }
        prevId = effectNodeId;
    };
    insertEffect(qEnvironmentVariable("DAQSTER_AUTOSTART_EFFECT"));
    insertEffect(qEnvironmentVariable("DAQSTER_AUTOSTART_EFFECT2"));

    // ── Dev driver: DAQSTER_AUTOSTART_SAMPLER=1 ─────────────────────────────
    // Inserts a FrameSampler node between the previous node and the output
    // (REQ-SW-PL-030 smoke driver).
    if (qEnvironmentVariableIsSet("DAQSTER_AUTOSTART_SAMPLER")
        && qEnvironmentVariableIntValue("DAQSTER_AUTOSTART_SAMPLER") != 0) {
        const QtNodes::NodeId samplerId = gm->addNode(QStringLiteral("FrameSampler"));
        DEBUG << "autoStartVideo: sampler node created id=" << samplerId;

        const QtNodes::ConnectionId oldConn{prevId, 0, outId, 0};
        if (gm->connectionExists(oldConn))
            gm->deleteConnection(oldConn);
        const QtNodes::ConnectionId inConn{prevId, 0, samplerId, 0};
        if (gm->connectionPossible(inConn)) {
            gm->addConnection(inConn);
            DEBUG << "autoStartVideo: connected prev -> sampler";
        } else {
            DEBUG << "autoStartVideo: sampler input connection NOT possible";
        }
        const QtNodes::ConnectionId outConn{samplerId, 0, outId, 0};
        if (gm->connectionPossible(outConn)) {
            gm->addConnection(outConn);
            DEBUG << "autoStartVideo: connected sampler -> output";
        } else {
            DEBUG << "autoStartVideo: sampler output connection NOT possible";
        }
    }

    // Configure the source node, press its start button and enable Perf on the
    // output (shared with the DAQSTER_AUTOSTART_FLOW path, REQ-SW-PL-038).
    startVideoPlayback();
}

// ── Shared video playback driver (REQ-SW-PL-038) ────────────────────────────
// Finds the VideoFileSource (or StreamSource) and VideoOutput nodes in the
// current graph by model-name, configures the source with DAQSTER_VIDEO_FILE /
// DAQSTER_STREAM_URL, presses its "Play"/"Connect" button and enables the
// "Perf" checkbox on the output (drives the [PERF] console line + badge).
// Used both by autoStartVideo() (nodes created programmatically) and by the
// DAQSTER_AUTOSTART_FLOW path (nodes loaded from a .flow scene).
void NodeEditorIdeObject::startVideoPlayback()
{
    QtNodes::DataFlowGraphModel* gm = m_Widget->graphModel();
    if (gm == nullptr) {
        DEBUG << "startVideoPlayback: no graph model";
        return;
    }

    // DAQSTER_VIDEO_FILE (file source) takes precedence over DAQSTER_STREAM_URL
    // (rtsp/http stream source).
    const QString videoFile = qEnvironmentVariable("DAQSTER_VIDEO_FILE");
    const QString streamUrl = qEnvironmentVariable(
        "DAQSTER_STREAM_URL", QStringLiteral("rtsp://192.168.33.233:554/stream1"));
    const bool useFile = !videoFile.isEmpty();
    const QString srcNodeName = useFile
        ? QStringLiteral("VideoFileSource") : QStringLiteral("StreamSource");

    // Find the source and output nodes by model-name in the current graph.
    QtNodes::NodeId srcId = QtNodes::InvalidNodeId;
    QtNodes::NodeId outId = QtNodes::InvalidNodeId;
    for (const QtNodes::NodeId nodeId : gm->allNodeIds()) {
        const QString type = gm->nodeData(nodeId, QtNodes::NodeRole::Type).toString();
        if (type == srcNodeName)
            srcId = nodeId;
        else if (type == QStringLiteral("VideoOutput"))
            outId = nodeId;
    }

    if (srcId == QtNodes::InvalidNodeId) {
        DEBUG << "startVideoPlayback: no" << srcNodeName << "node in graph";
        return;
    }
    if (outId == QtNodes::InvalidNodeId) {
        DEBUG << "startVideoPlayback: no VideoOutput node in graph";
        return;
    }

    // Configure the source node and press its start button.
    auto* srcModel = gm->delegateModel<QtNodes::NodeDelegateModel>(srcId);
    if (srcModel != nullptr) {
        QJsonObject cfg;
        const QString buttonText = useFile ? QStringLiteral("Play")
                                           : QStringLiteral("Connect");
        if (useFile)
            cfg["filePath"] = videoFile;
        else
            cfg["url"] = streamUrl;
        srcModel->load(cfg);

        QWidget* w = srcModel->embeddedWidget();
        if (w != nullptr) {
            const auto buttons = w->findChildren<QPushButton*>();
            for (QPushButton* b : buttons) {
                if (b->text() == buttonText) {
                    DEBUG << "startVideoPlayback: pressing " << buttonText;
                    b->click();
                    break;
                }
            }
        }
    }

    // Enable the Perf checkbox on the output node (drives the [PERF] console
    // line + badge). REQ-SW-PL-053: the controls (incl. the Perf toggle) live
    // in the DETACHED display window, not in the embedded node widget — the
    // VideoOutputNode exposes setPerfEnabled() for the detached-controls case.
    auto* outModel = gm->delegateModel<QtNodes::NodeDelegateModel>(outId);
    if (outModel != nullptr) {
        QWidget* w = outModel->embeddedWidget();
        if (w != nullptr) {
            const auto checks = w->findChildren<QCheckBox*>();
            for (QCheckBox* c : checks) {
                if (c->text() == tr("Perf")) {
                    DEBUG << "startVideoPlayback: enabling Perf";
                    c->setChecked(true);
                    break;
                }
            }

            // DAQSTER_SCENE_VIDEO=1 (Qt6 dev driver, REQ-SW-PL-021): uncheck
            // the "GPU display" checkbox so video renders IN the scene (in-scene
            // QGraphicsVideoItem) instead of a detached window — headless
            // verification of the in-scene path.
            if (qEnvironmentVariableIsSet("DAQSTER_SCENE_VIDEO")
                && qEnvironmentVariableIntValue("DAQSTER_SCENE_VIDEO") != 0) {
                const auto sceneChecks = w->findChildren<QCheckBox*>();
                for (QCheckBox* c : sceneChecks) {
                    if (c->text() == tr("GPU display")) {
                        DEBUG << "startVideoPlayback: enabling in-scene video (DAQSTER_SCENE_VIDEO=1)";
                        c->setChecked(false);
                        break;
                    }
                }
            }
        }

        // REQ-SW-PL-053: the Perf toggle moved to the detached window's
        // controls pane — enable it through the node's slot (no cross-plugin
        // link dependency; the slot is invoked via the meta-object system).
        if (!QMetaObject::invokeMethod(outModel, "setPerfEnabled", Q_ARG(bool, true))) {
            DEBUG << "startVideoPlayback: output node has no setPerfEnabled slot";
        }
    }
}
