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
#include <QCheckBox>
#include <QJsonObject>

#include <QtNodes/NodeDelegateModel>
#include <QtNodes/NodeDelegateModelRegistry>
#include <QtNodes/DataFlowGraphModel>
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
    // "grayscale", "invert", "sepia", "channelSwap", "flip") — the registered
    // node name is "VideoEffect" + capitalized id.
    QtNodes::NodeId prevId = srcId;
    const QString effectId = qEnvironmentVariable("DAQSTER_AUTOSTART_EFFECT");
    if (!effectId.isEmpty()) {
        const QString effectNodeName = QStringLiteral("VideoEffect")
            + effectId.left(1).toUpper() + effectId.mid(1);
        const QtNodes::NodeId effectNodeId = gm->addNode(effectNodeName);
        DEBUG << "autoStartVideo: effect node created id=" << effectNodeId
              << " name=" << effectNodeName;

        const QtNodes::ConnectionId oldConn{prevId, 0, outId, 0};
        if (gm->connectionExists(oldConn))
            gm->deleteConnection(oldConn);
        const QtNodes::ConnectionId inConn{prevId, 0, effectNodeId, 0};
        if (gm->connectionPossible(inConn)) {
            gm->addConnection(inConn);
            DEBUG << "autoStartVideo: connected source -> effect";
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
    }

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
