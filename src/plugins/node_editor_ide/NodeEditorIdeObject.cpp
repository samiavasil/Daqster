#include "NodeEditorIdeObject.h"
#include "NodeEditorWidget.h"
#include "QPluginManager.h"
#include "capabilities/INodeProvider.h"
#include "debug.h"
#include "LogCategories.h"

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
#include <QMessageBox>
#include <QSet>
#include <QDir>

#include <exception>

#include <QtNodes/NodeDelegateModel>
#include <QtNodes/NodeDelegateModelRegistry>
#include <QtNodes/DataFlowGraphModel>
#include <QtNodes/DataFlowGraphicsScene>
#include <QtNodes/ConnectionStyle>

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

    // Phase 1: Register built-in nodes (from former node_editor_app)
    registerBuiltInNodes();

    // Phase 2: Discover and register external INodeProvider plugins
    discoverAndRegisterExternalNodes();

    // Build canvas AFTER all nodes are registered
    m_Widget->buildCanvas();

    // ── File menu (REQ-SW-PL-037): Save/Load scene ─────────────────────────
    // Save uses QtNodes' native DataFlowGraphicsScene::save() (opens its own
    // file dialog, writes .flow JSON). Load uses the tolerant path that skips
    // unregistered node types instead of crashing.
    QMenu* fileMenu = m_Win->menuBar()->addMenu(tr("&File"));

    QAction* saveAction = fileMenu->addAction(tr("Save Scene…"));
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, [this]() {
        if (m_Widget->scene() != nullptr)
            m_Widget->scene()->save();
    });

    QAction* loadAction = fileMenu->addAction(tr("Load Scene…"));
    loadAction->setShortcut(QKeySequence::Open);
    connect(loadAction, &QAction::triggered, this, &NodeEditorIdeObject::loadSceneTolerant);

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

    connect(m_Widget, &NodeEditorWidget::nodeDoubleClicked,
            this, &NodeEditorIdeObject::nodeDoubleClicked);
    connect(m_Win, SIGNAL(destroyed(QObject*)), this, SLOT(MainWinDestroyed(QObject*)));
    connect(button, SIGNAL(clicked(bool)), this, SLOT(ShowPlugins()));
    return true;
}

void NodeEditorIdeObject::registerBuiltInNodes()
{
    auto* registry = m_Widget->getInjectedRegistry();

    registry->registerModel<NumberSourceDataModel>("Sources");
    registry->registerModel<NumberDisplayDataModel>("Displays");
    registry->registerModel<ModuloModel>("Operators");
    registry->registerModel<ArithmeticLogicModel>("Operators");
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

// ── Tolerant scene load (REQ-SW-PL-037) ─────────────────────────────────────
// QtNodes' DataFlowGraphModel::loadNode() throws std::logic_error when a saved
// scene references a model type that is not registered in the current
// environment (e.g. the providing plugin is not loaded). Instead of crashing,
// this method:
//   1. opens a .flow file dialog,
//   2. parses the JSON scene,
//   3. drops nodes whose "internal-data"."model-name" is not registered,
//   4. drops connections referencing any dropped node id (no dangling edges),
//   5. loads the cleaned scene and warns the user about the skipped types.
bool NodeEditorIdeObject::loadSceneTolerant()
{
    const QString fileName = QFileDialog::getOpenFileName(
        m_Win, tr("Open Flow Scene"), QDir::homePath(), tr("Flow Scene Files (*.flow)"));
    if (fileName.isEmpty())
        return false;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(lcNodeEditor) << "loadSceneTolerant: cannot open" << fileName;
        return false;
    }

    const QByteArray wholeFile = file.readAll();
    QJsonParseError parseError{};
    const QJsonDocument sceneDocument = QJsonDocument::fromJson(wholeFile, &parseError);
    if (parseError.error != QJsonParseError::NoError || !sceneDocument.isObject()) {
        qCWarning(lcNodeEditor) << "loadSceneTolerant: invalid JSON in" << fileName
                                << ":" << parseError.errorString();
        return false;
    }

    QJsonObject sceneJson = sceneDocument.object();
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
        qCWarning(lcNodeEditor) << "loadSceneTolerant: load failed:" << e.what();
        return false;
    }

    if (!skippedTypes.isEmpty()) {
        qCWarning(lcNodeEditor) << "loadSceneTolerant: skipped unregistered node types:"
                                << skippedTypes.join(QStringLiteral(", "));
        QMessageBox::warning(m_Win,
                             tr("Load Scene"),
                             tr("The following node types are not registered in this "
                                "environment and were skipped:\n%1")
                                 .arg(skippedTypes.join(QLatin1Char('\n'))));
    }

    return true;
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
                    DEBUG << "autoStartVideo: pressing " << buttonText;
                    b->click();
                    break;
                }
            }
        }
    }

    // Enable the Perf checkbox on the output node (drives the [PERF] console
    // line + badge).
    auto* outModel = gm->delegateModel<QtNodes::NodeDelegateModel>(outId);
    if (outModel != nullptr) {
        QWidget* w = outModel->embeddedWidget();
        if (w != nullptr) {
            const auto checks = w->findChildren<QCheckBox*>();
            for (QCheckBox* c : checks) {
                if (c->text() == tr("Perf")) {
                    DEBUG << "autoStartVideo: enabling Perf";
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
                        DEBUG << "autoStartVideo: enabling in-scene video (DAQSTER_SCENE_VIDEO=1)";
                        c->setChecked(false);
                        break;
                    }
                }
            }
        }
    }
}
