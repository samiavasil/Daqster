#include "RuntimeShell.h"
#include "NodeEditorWidget.h"
#include "QPluginManager.h"
#include "capabilities/INodeProvider.h"
#include "capabilities/IWidgetProvider.h"
#include "debug.h"
#include "LogCategories.h"
#include "shared/IStartable.h"
#include "shared/IStoppable.h"
#include "capabilities/INodeConfigSchema.h"

// REQ-SW-PL-054: the control plane is a mode-agnostic seam. ControlPlaneServer
// lives in FrameworkCore; the GUI host plugs itself in as the RuntimeController.
#include "ControlPlaneServer.h"
#include "NodeSchemaBuilder.h"

// REQ-SW-PL-051: no built-in node model headers here — the models are owned by
// nodeditor_nodes/core and registered through its INodeProvider below.

#include <QMainWindow>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QVBoxLayout>
#include <QWidget>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>
#include <QPointF>
#include <QSet>
#include <QDir>
#include <QCloseEvent>

#include <algorithm>

#include <QtNodes/NodeDelegateModel>
#include <QtNodes/NodeDelegateModelRegistry>
#include <QtNodes/DataFlowGraphModel>
#include <QtNodes/DataFlowGraphicsScene>
#include <QtNodes/internal/NodeGraphicsObject.hpp>

#include <exception>

// Custom QMainWindow that stops nodes before closing
class RuntimeMainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit RuntimeMainWindow(RuntimeShell* shell, QWidget* parent = nullptr)
        : QMainWindow(parent), m_shell(shell) {}

protected:
    void closeEvent(QCloseEvent* event) override
    {
        if (m_shell) {
            m_shell->stopAllNodes();
        }
        QMainWindow::closeEvent(event);
    }

private:
    RuntimeShell* m_shell;
};

RuntimeShell::RuntimeShell(QObject* parent)
    : Daqster::QBasePluginObject(parent)
    , m_mainWindow(nullptr)
    , m_editorWidget(nullptr)
{
}

RuntimeShell::~RuntimeShell()
{
    // Nodes are already stopped in closeEvent before window destruction.
    // DeInitialize only cleans up pointers, does NOT call stopAllNodes().
    stopControlPlane();
    if (m_mainWindow) {
        m_mainWindow->deleteLater();
        m_mainWindow = nullptr;
    }
    m_editorWidget = nullptr;
    m_mdiAreas.clear();
    m_workspaceIdToMdiArea.clear();
    m_autoStartNodes.clear();
    m_workspaces.clear();
}

void RuntimeShell::DeInitialize()
{
    // Virtual override for QBasePluginObject - cleanup only, no stopAllNodes()
    // (nodes already stopped in closeEvent before window destruction)
    stopControlPlane();
    if (m_mainWindow) {
        m_mainWindow->deleteLater();
        m_mainWindow = nullptr;
    }
    m_editorWidget = nullptr;
    m_mdiAreas.clear();
    m_workspaceIdToMdiArea.clear();
    m_autoStartNodes.clear();
    m_workspaces.clear();
}

void RuntimeShell::onMainWindowDestroyed(QObject* obj)
{
    Q_UNUSED(obj);
    m_mainWindow = nullptr;
    m_editorWidget = nullptr;
    // Do NOT deleteLater() - parent owns us
}

bool RuntimeShell::RunRuntime(const QString& flowPath)
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

    // 1. Create QMainWindow
    m_mainWindow = new RuntimeMainWindow(this);
    m_mainWindow->setAttribute(Qt::WA_DeleteOnClose, true);
    connect(m_mainWindow, &QObject::destroyed, this, &RuntimeShell::onMainWindowDestroyed);

    // Central widget will be set up by arrangeWorkspaces()
    QWidget* centralWidget = new QWidget(m_mainWindow);
    m_mainWindow->setCentralWidget(centralWidget);

    // 2. Register nodes (built-in + external plugins)
    if (!registerNodes()) {
        QMessageBox::critical(m_mainWindow, tr("Runtime Mode"),
                              tr("Failed to register node types."));
        return false;
    }

    // 3. Build canvas (hidden — only needed for graph model + scene)
    if (!buildCanvas()) {
        QMessageBox::critical(m_mainWindow, tr("Runtime Mode"),
                              tr("Failed to build editor canvas."));
        return false;
    }

    // 4. Load flow file (parses and stores the UI section; shows its own error)
    if (!loadFlow(flowPath)) {
        // Error message already shown by loadFlow
        return false;
    }

    // 5. Arrange deembedded widgets in MDI workspaces
    arrangeWorkspaces(m_uiSection);

    // 6. Auto-start nodes
    autoStartNodes(m_uiSection);

    // Show the runtime window
    m_mainWindow->setWindowTitle(tr("Daqster Runtime — %1").arg(QFileInfo(flowPath).fileName()));
    m_mainWindow->resize(1280, 720);
    m_mainWindow->show();

    qCInfo(lcNodeEditor) << "Runtime mode: loaded" << flowPath;
    return true;
}

bool RuntimeShell::registerNodes()
{
    // Create editor widget (needed for registry access)
    QWidget* centralWidget = m_mainWindow->centralWidget();
    if (!centralWidget) return false;

    m_editorWidget = new NodeEditorWidget(centralWidget);
    // Don't add to layout — canvas stays hidden

    auto* registry = m_editorWidget->getInjectedRegistry();
    if (!registry) return false;

    // REQ-SW-PL-051: every node — including the built-in ones (NumberSource,
    // NumberDisplay, Modulo, Arithmetic/Logic) — is registered by the
    // nodeditor_nodes/core INodeProvider, so there is no local list here.
    // Discover and register external nodes.
    Daqster::QPluginManager* pm = Daqster::QPluginManager::instance();
    if (pm) {
        for (Daqster::INodeProvider* provider : pm->nodeProviders()) {
            if (!provider) continue;

            DEBUG << "RuntimeShell: Discovered INodeProvider plugin";
            provider->registerNodes(*registry);
        }
    }

    // REQ-SW-PL-051: Pass the widget providers from the GUI plugins to the
    // editor widget so nodes with split core/gui models get their widgets.
    // More than one plugin may implement IWidgetProvider (each serves widgets
    // for its own core models); ALL of them must be consulted, in order.
    if (pm) {
        adoptWidgetProviders(pm->widgetProviders());
    }

    // REQ-SW-PL-051: plugins may load in any order. When a new INodeProvider
    // plugin appears (or is reloaded), re-discover and register its nodes.
    if (pm) {
        connect(pm, &Daqster::QPluginManager::PluginsListChangeDetected,
                this, [this]() {
                    discoverAndRegisterExternalNodes();
                });
    }

    return true;
}

void RuntimeShell::discoverAndRegisterExternalNodes()
{
    if (!m_editorWidget)
        return;

    auto* registry = m_editorWidget->getInjectedRegistry();
    if (!registry)
        return;

    Daqster::QPluginManager* pm = Daqster::QPluginManager::instance();
    if (!pm)
        return;

    for (Daqster::INodeProvider* provider : pm->nodeProviders()) {
        if (!provider)
            continue;

        DEBUG << "RuntimeShell: Discovered INodeProvider plugin";
        provider->registerNodes(*registry);
    }

    // REQ-SW-PL-051: also discover IWidgetProvider plugins (they may load
    // in any order). Pass all of them to the editor widget so split models
    // get their widgets.
    adoptWidgetProviders(pm->widgetProviders());
}

void RuntimeShell::adoptWidgetProviders(const QList<Daqster::IWidgetProvider*>& providers)
{
    if (providers.isEmpty() || !m_editorWidget)
        return;

    for (Daqster::IWidgetProvider* provider : providers) {
        if (!provider) continue;
        DEBUG << "RuntimeShell: Discovered IWidgetProvider plugin";
    }

    m_editorWidget->setWidgetProviders(providers);

    // The graph model caches the raw pointers, so re-resolve them when any
    // plugin object dies (plugin reload / unload). PluginRegistry recreates
    // fresh, initialized objects on the next widgetProviders() call.
    for (Daqster::IWidgetProvider* provider : providers) {
        if (auto* owner = dynamic_cast<QObject*>(provider)) {
            connect(owner, &QObject::destroyed, this, [this]() {
                DEBUG << "RuntimeShell: IWidgetProvider plugin object destroyed, re-discovering";
                discoverAndRegisterExternalNodes();
            });
        }
    }
}

bool RuntimeShell::buildCanvas()
{
    if (!m_editorWidget) return false;
    m_editorWidget->buildCanvas();
    // Keep canvas hidden — we only need the graph model and scene
    m_editorWidget->hide();
    return true;
}

bool RuntimeShell::loadFlow(const QString& fileName)
{
    if (!m_editorWidget || !m_editorWidget->scene() || !m_editorWidget->graphModel()) {
        qCWarning(lcNodeEditor) << "loadFlow: editor not initialized";
        emit errorOccurred(tr("editor not initialized"));
        return false;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(lcNodeEditor) << "loadFlow: cannot open" << fileName;
        emit errorOccurred(tr("Cannot open flow file: %1").arg(fileName));
        QMessageBox::critical(m_mainWindow, tr("Runtime Mode"),
                              tr("Cannot open flow file: %1").arg(fileName));
        return false;
    }

    const QByteArray wholeFile = file.readAll();
    QJsonParseError parseError{};
    const QJsonDocument sceneDocument = QJsonDocument::fromJson(wholeFile, &parseError);
    if (parseError.error != QJsonParseError::NoError || !sceneDocument.isObject()) {
        qCWarning(lcNodeEditor) << "loadFlow: invalid JSON in" << fileName
                                << ":" << parseError.errorString();
        emit errorOccurred(tr("Invalid JSON in flow file: %1").arg(parseError.errorString()));
        QMessageBox::critical(m_mainWindow, tr("Runtime Mode"),
                              tr("Invalid JSON in flow file: %1").arg(parseError.errorString()));
        return false;
    }

    QStringList skippedTypes;
    QString error;
    if (!applySceneJson(sceneDocument.object(), &skippedTypes, &error)) {
        QMessageBox::critical(m_mainWindow, tr("Runtime Mode"),
                              tr("Failed to load flow: %1").arg(error));
        return false;
    }

    const int loadedNodeCount = static_cast<int>(m_editorWidget->graphModel()->allNodeIds().size());
    qCInfo(lcNodeEditor) << "loadFlow: loaded" << fileName
                         << "nodes=" << loadedNodeCount;

    if (!skippedTypes.isEmpty()) {
        qCWarning(lcNodeEditor) << "loadFlow: skipped unregistered node types:"
                                << skippedTypes.join(QStringLiteral(", "));
        QMessageBox::warning(m_mainWindow,
                             tr("Load Flow"),
                             tr("The following node types are not registered in this "
                                "environment and were skipped:\n%1")
                                 .arg(skippedTypes.join(QLatin1Char('\n'))));
    }

    return true;
}

bool RuntimeShell::applySceneJson(QJsonObject sceneJson,
                                  QStringList* skippedTypes,
                                  QString* errorMessage)
{
    if (!m_editorWidget || !m_editorWidget->scene() || !m_editorWidget->graphModel()) {
        qCWarning(lcNodeEditor) << "applySceneJson: editor not initialized";
        if (errorMessage) *errorMessage = tr("editor not initialized");
        emit errorOccurred(tr("editor not initialized"));
        return false;
    }

    // Extract the "ui" section (REQ-SW-PL-049) BEFORE the node-cleaning loop:
    // it is not part of the graph model JSON and must not be passed to load().
    const FlowUi::UiSection uiSection = FlowUi::UiSection::fromJson(sceneJson["ui"].toObject());
    sceneJson.remove("ui");

    const QJsonArray nodesJsonArray = sceneJson["nodes"].toArray();

    auto* registry = m_editorWidget->getInjectedRegistry();
    const auto& creators = registry->registeredModelCreators();

    QStringList skipped;
    QSet<QtNodes::NodeId> skippedNodeIds;

    QJsonArray cleanedNodes;
    for (const auto& nodeValue : nodesJsonArray) {
        const QJsonObject nodeJson = nodeValue.toObject();
        const QString modelName = nodeJson["internal-data"].toObject()["model-name"].toString();
        if (modelName.isEmpty() || creators.count(modelName) == 0) {
            skipped << (modelName.isEmpty() ? tr("<unnamed>") : modelName);
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
        m_editorWidget->scene()->clearScene();
        m_editorWidget->graphModel()->load(sceneJson);
    } catch (const std::exception& e) {
        qCWarning(lcNodeEditor) << "applySceneJson: load failed:" << e.what();
        if (errorMessage) *errorMessage = QString::fromUtf8(e.what());
        emit errorOccurred(QString::fromUtf8(e.what()));
        return false;
    }

    // Store the full UI section plus the derived autoStart flags for later use.
    m_uiSection = uiSection;
    m_workspaces = uiSection.workspaces;
    m_autoStartNodes.clear();
    for (auto it = uiSection.nodes.constBegin(); it != uiSection.nodes.constEnd(); ++it) {
        m_autoStartNodes.insert(it.key(), it.value().autoStart);
    }

    // A fresh graph means nothing is running yet.
    m_runningNodes.clear();

    if (skippedTypes)
        *skippedTypes = skipped;

    return true;
}

void RuntimeShell::arrangeWorkspaces(const FlowUi::UiSection& ui)
{
    if (!m_mainWindow || !m_editorWidget || !m_editorWidget->scene())
        return;

    QWidget* centralWidget = m_mainWindow->centralWidget();
    if (!centralWidget) return;

    // Clear any existing layout
    QLayout* existingLayout = centralWidget->layout();
    if (existingLayout) {
        QLayoutItem* item;
        while ((item = existingLayout->takeAt(0)) != nullptr) {
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }
        delete existingLayout;
    }

    // A reload (POST /api/flow) rebuilds the workspaces. The old MDI areas were
    // only scheduled for deletion above, so drop their pointers now to avoid
    // using stale entries while the new workspaces are created.
    m_mdiAreas.clear();
    m_workspaceIdToMdiArea.clear();

    // If no workspaces defined, create a default one
    std::vector<FlowUi::WorkspaceUi> workspaces = ui.workspaces;
    if (workspaces.empty()) {
        FlowUi::WorkspaceUi ws;
        ws.id = 0;
        ws.tabbed = true;
        workspaces.push_back(ws);
    }

    // Create layout for central widget
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // For each workspace, create an MDI area
    for (const auto& ws : workspaces) {
        QMdiArea* mdiArea = new QMdiArea(centralWidget);
        mdiArea->setViewMode(ws.tabbed ? QMdiArea::TabbedView : QMdiArea::SubWindowView);
        mdiArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        mdiArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        mdiArea->setActivationOrder(QMdiArea::CreationOrder);

        // Apply workspace geometry if valid
        if (ws.geometry.w > 0 && ws.geometry.h > 0) {
            mdiArea->setGeometry(QRect(ws.geometry.x, ws.geometry.y,
                                       ws.geometry.w, ws.geometry.h));
        }

        m_mdiAreas.push_back(mdiArea);
        m_workspaceIdToMdiArea[ws.id] = mdiArea;
        mainLayout->addWidget(mdiArea);
    }

    // Now deembed nodes and place them in the appropriate MDI areas
    for (auto it = ui.nodes.constBegin(); it != ui.nodes.constEnd(); ++it) {
        const QtNodes::NodeId nodeId = it.key();
        const FlowUi::NodeUi& nui = it.value();

        if (!nui.deembedded)
            continue;

        // Tolerant load guard: nodes skipped by loadFlow() are not in the graph
        if (!m_editorWidget->graphModel()->nodeExists(nodeId))
            continue;

        QtNodes::NodeGraphicsObject* node = m_editorWidget->scene()->nodeGraphicsObject(nodeId);
        if (node == nullptr || !node->isWidgetEmbedded())
            continue;

        // Get the widget through the graph model: it consults the IWidgetProvider
        // of the GUI plugin (REQ-SW-PL-051), while NodeDelegateModel::
        // embeddedWidget() is nullptr for every core/split model.
        auto* model = m_editorWidget->graphModel()->delegateModel<QtNodes::NodeDelegateModel>(nodeId);
        QWidget* widget = m_editorWidget->nodeWidget(nodeId);
        if (model == nullptr || widget == nullptr)
            continue;

        // Two-step deembed:
        // 1. setWidgetEmbedded(false) FIRST — properly detaches from QGraphicsProxyWidget
        node->setWidgetEmbedded(false);

        // 2. Find the target MDI area for this node's workspace
        QMdiArea* targetMdi = m_workspaceIdToMdiArea.value(nui.workspace);
        if (!targetMdi && !m_mdiAreas.empty())
            targetMdi = m_mdiAreas.front(); // fallback to first workspace

        if (!targetMdi)
            continue;

        // Create MDI sub-window and set the widget
        QMdiSubWindow* subWindow = targetMdi->addSubWindow(widget);
        subWindow->setWindowTitle(model->caption());

        // Restore geometry from NodeUi.geometry
        if (nui.geometry.w > 0 && nui.geometry.h > 0) {
            subWindow->setGeometry(QRect(nui.geometry.x, nui.geometry.y,
                                         nui.geometry.w, nui.geometry.h));
        }

        // Window flags: verify Qt::WindowStaysOnTopHint is cleared after setWidget()
        // Reparenting should reset to Qt::Widget. If NOT cleared on Qt5/Qt6,
        // explicitly set widget->setWindowFlags(Qt::Widget) after setWidget().
        clearWindowFlags(widget);

        if (nui.geometry.maximized)
            subWindow->showMaximized();
        else
            subWindow->show();
    }
}

void RuntimeShell::autoStartNodes(const FlowUi::UiSection& ui)
{
    if (!m_editorWidget || !m_editorWidget->graphModel())
        return;

    for (auto it = ui.nodes.constBegin(); it != ui.nodes.constEnd(); ++it) {
        const QtNodes::NodeId nodeId = it.key();
        const FlowUi::NodeUi& nui = it.value();

        if (!nui.autoStart)
            continue;

        // Tolerant load guard
        if (!m_editorWidget->graphModel()->nodeExists(nodeId))
            continue;

        auto* model = m_editorWidget->graphModel()->delegateModel<QtNodes::NodeDelegateModel>(nodeId);
        if (!model)
            continue;

        // Dynamic cast to IStartable and call start()
        auto* startable = dynamic_cast<Daqster::IStartable*>(model);
        if (startable) {
            qCInfo(lcNodeEditor) << "Auto-starting node:" << model->caption() << "(id=" << nodeId << ")";
            startable->start();
            m_runningNodes[nodeId] = true;
            emit nodeStarted(nodeId);
        }
    }
}

void RuntimeShell::stopAllNodes()
{
    if (!m_editorWidget || !m_editorWidget->graphModel())
        return;

    for (const QtNodes::NodeId nodeId : m_editorWidget->graphModel()->allNodeIds()) {
        auto* model = m_editorWidget->graphModel()->delegateModel<QtNodes::NodeDelegateModel>(nodeId);
        if (!model)
            continue;

        auto* stoppable = dynamic_cast<Daqster::IStoppable*>(model);
        if (stoppable) {
            qCInfo(lcNodeEditor) << "Stopping node:" << model->caption() << "(id=" << nodeId << ")";
            stoppable->stop();
            if (m_runningNodes.value(nodeId, false)) {
                m_runningNodes[nodeId] = false;
                emit nodeStopped(nodeId);
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Remote control plane (REQ-SW-PL-054) — GUI mode
//
// The same ControlPlaneServer that serves the headless engine is reused here;
// it only ever talks to Daqster::RuntimeController, so the HTTP/WebSocket
// surface is identical in both runtime modes.
// ═══════════════════════════════════════════════════════════════════════════

QtNodes::DataFlowGraphModel* RuntimeShell::graphModel() const
{
    // startControlPlane() runs before RunRuntime() builds the canvas, so this
    // must tolerate m_editorWidget being null (an early request sees no graph).
    return m_editorWidget ? m_editorWidget->graphModel() : nullptr;
}

bool RuntimeShell::startControlPlane(const Daqster::ControlPlaneOptions& options,
                                     QString* errorMessage)
{
    if (m_controlPlane && m_controlPlane->isListening())
        return true;

    if (!m_controlPlane)
        m_controlPlane = std::make_unique<Daqster::ControlPlaneServer>(this);

    // Stream lifecycle events to WebSocket subscribers (REQ-SW-PL-054 AC10).
    // Connected once per freshly created server; the lambda target dies with it.
    connect(this, &RuntimeShell::nodeStarted, m_controlPlane.get(),
            [this](QtNodes::NodeId id) {
                m_controlPlane->broadcastEvent(QStringLiteral("nodeStarted"),
                                               {{QStringLiteral("nodeId"), static_cast<qint64>(id)}});
            });
    connect(this, &RuntimeShell::nodeStopped, m_controlPlane.get(),
            [this](QtNodes::NodeId id) {
                m_controlPlane->broadcastEvent(QStringLiteral("nodeStopped"),
                                               {{QStringLiteral("nodeId"), static_cast<qint64>(id)}});
            });
    connect(this, &RuntimeShell::nodeConfigChanged, m_controlPlane.get(),
            [this](QtNodes::NodeId id) {
                m_controlPlane->broadcastEvent(QStringLiteral("nodeConfigChanged"),
                                               {{QStringLiteral("nodeId"), static_cast<qint64>(id)}});
            });
    connect(this, &RuntimeShell::errorOccurred, m_controlPlane.get(),
            [this](const QString& message) {
                m_controlPlane->broadcastEvent(QStringLiteral("nodeError"),
                                               {{QStringLiteral("message"), message}});
            });
    // Graph mutations (REQ-SW-PL-063 AC13)
    connect(this, &RuntimeShell::nodeCreated, m_controlPlane.get(),
            [this](QtNodes::NodeId id) {
                m_controlPlane->broadcastEvent(QStringLiteral("nodeCreated"),
                                               {{QStringLiteral("nodeId"), static_cast<qint64>(id)}});
            });
    connect(this, &RuntimeShell::nodeDeleted, m_controlPlane.get(),
            [this](QtNodes::NodeId id) {
                m_controlPlane->broadcastEvent(QStringLiteral("nodeDeleted"),
                                               {{QStringLiteral("nodeId"), static_cast<qint64>(id)}});
            });
    connect(this, &RuntimeShell::nodeMoved, m_controlPlane.get(),
            [this](QtNodes::NodeId id) {
                m_controlPlane->broadcastEvent(QStringLiteral("nodeMoved"),
                                               {{QStringLiteral("nodeId"), static_cast<qint64>(id)}});
            });
    connect(this, &RuntimeShell::connectionCreated, m_controlPlane.get(),
            [this](quint64 outNodeId, int outPortIndex, quint64 inNodeId, int inPortIndex) {
                m_controlPlane->broadcastEvent(QStringLiteral("connectionCreated"),
                                               {{QStringLiteral("outNodeId"), static_cast<qint64>(outNodeId)},
                                                {QStringLiteral("outPortIndex"), outPortIndex},
                                                {QStringLiteral("inNodeId"), static_cast<qint64>(inNodeId)},
                                                {QStringLiteral("inPortIndex"), inPortIndex}});
            });
    connect(this, &RuntimeShell::connectionDeleted, m_controlPlane.get(),
            [this](quint64 outNodeId, int outPortIndex, quint64 inNodeId, int inPortIndex) {
                m_controlPlane->broadcastEvent(QStringLiteral("connectionDeleted"),
                                               {{QStringLiteral("outNodeId"), static_cast<qint64>(outNodeId)},
                                                {QStringLiteral("outPortIndex"), outPortIndex},
                                                {QStringLiteral("inNodeId"), static_cast<qint64>(inNodeId)},
                                                {QStringLiteral("inPortIndex"), inPortIndex}});
            });

    QString error;
    if (!m_controlPlane->start(options, &error)) {
        if (errorMessage)
            *errorMessage = error;
        qCCritical(lcNodeEditor) << "RuntimeShell: control plane failed to start:" << error;
        m_controlPlane.reset();
        return false;
    }

    qCInfo(lcNodeEditor) << "RuntimeShell: control plane listening on"
                         << options.bindAddress << ":" << options.port;
    return true;
}

void RuntimeShell::stopControlPlane()
{
    if (!m_controlPlane)
        return;
    m_controlPlane->stop();
    m_controlPlane.reset();
}

bool RuntimeShell::controlPlaneEnabled() const
{
    return m_controlPlane && m_controlPlane->isListening();
}

QVector<Daqster::RuntimeController::NodeInfo> RuntimeShell::nodes() const
{
    QVector<NodeInfo> result;
    auto* graph = graphModel();
    if (!graph)
        return result;

    for (const QtNodes::NodeId nodeId : graph->allNodeIds()) {
        auto* model = graph->delegateModel<QtNodes::NodeDelegateModel>(nodeId);
        if (!model)
            continue;

        NodeInfo info;
        info.id = static_cast<quint64>(nodeId);
        info.type = model->name();
        info.name = model->caption();
        info.state = m_runningNodes.value(nodeId, false) ? QStringLiteral("running")
                                                         : QStringLiteral("stopped");
        result.append(info);
    }
    return result;
}

bool RuntimeShell::nodeConfig(quint64 nodeId, QJsonObject& out) const
{
    auto* graph = graphModel();
    if (!graph)
        return false;

    auto* model = graph->delegateModel<QtNodes::NodeDelegateModel>(
        static_cast<QtNodes::NodeId>(nodeId));
    if (!model)
        return false;

    out = model->save();
    return true;
}

bool RuntimeShell::setNodeConfig(quint64 nodeId, const QJsonObject& config)
{
    auto* graph = graphModel();
    if (!graph)
        return false;

    const QtNodes::NodeId id = static_cast<QtNodes::NodeId>(nodeId);
    auto* model = graph->delegateModel<QtNodes::NodeDelegateModel>(id);
    if (!model)
        return false;

    // NodeDelegateModel::load() returns void, so a malformed patch must not take
    // the runtime down: snapshot first and roll back on a thrown error.
    const QJsonObject before = model->save();
    try {
        model->load(config);
    } catch (const std::exception& e) {
        qCWarning(lcNodeEditor) << "RuntimeShell: node config rejected (id=" << id
                                << "):" << e.what();
        model->load(before);
        return false;
    }

    emit nodeConfigChanged(id);
    return true;
}

bool RuntimeShell::startNode(quint64 nodeId)
{
    auto* graph = graphModel();
    if (!graph)
        return false;

    const QtNodes::NodeId id = static_cast<QtNodes::NodeId>(nodeId);
    auto* model = graph->delegateModel<QtNodes::NodeDelegateModel>(id);
    if (!model)
        return false;

    auto* startable = dynamic_cast<Daqster::IStartable*>(model);
    if (!startable)
        return false;

    qCInfo(lcNodeEditor) << "RuntimeShell: starting node via control plane:"
                         << model->caption() << "(id=" << id << ")";
    startable->start();
    m_runningNodes[id] = true;
    emit nodeStarted(id);
    return true;
}

bool RuntimeShell::stopNode(quint64 nodeId)
{
    auto* graph = graphModel();
    if (!graph)
        return false;

    const QtNodes::NodeId id = static_cast<QtNodes::NodeId>(nodeId);
    auto* model = graph->delegateModel<QtNodes::NodeDelegateModel>(id);
    if (!model)
        return false;

    auto* stoppable = dynamic_cast<Daqster::IStoppable*>(model);
    if (!stoppable)
        return false;

    qCInfo(lcNodeEditor) << "RuntimeShell: stopping node via control plane:"
                         << model->caption() << "(id=" << id << ")";
    stoppable->stop();
    m_runningNodes[id] = false;
    emit nodeStopped(id);
    return true;
}

QJsonObject RuntimeShell::flowJson() const
{
    QJsonObject flow;
    if (auto* graph = graphModel())
        flow = graph->save(); // { "nodes": [...], "connections": [...] }

    flow.insert(QStringLiteral("ui"), m_uiSection.toJson());
    return flow;
}

QJsonObject RuntimeShell::nodeSchema(quint64 nodeId) const
{
    auto* graph = graphModel();
    if (!graph)
        return {};

    auto* model = graph->delegateModel<QtNodes::NodeDelegateModel>(
        static_cast<QtNodes::NodeId>(nodeId));
    if (!model)
        return {};

    return typeSchema(model->name());
}

QJsonObject RuntimeShell::typeSchema(const QString& type) const
{
    if (!m_editorWidget)
        return {};

    auto* registry = m_editorWidget->getInjectedRegistry();
    if (!registry)
        return {};

    const auto& creators = registry->registeredModelCreators();
    const auto creatorIt = creators.find(type);
    if (creatorIt == creators.end())
        return {};

    const std::unique_ptr<QtNodes::NodeDelegateModel> fresh = creatorIt->second();
    if (!fresh)
        return {};

    // Enriched schema when the model self-describes (REQ-SW-PL-063 AC10).
    const auto* described = dynamic_cast<const Daqster::INodeConfigSchema*>(fresh.get());
    const QJsonObject provided = described ? described->configSchema() : QJsonObject();
    return Daqster::nodeConfigSchema(fresh->save(), type, provided);
}

QVector<Daqster::RuntimeController::NodeTypeInfo> RuntimeShell::nodeTypes() const
{
    QVector<NodeTypeInfo> result;
    if (!m_editorWidget)
        return result;

    auto* registry = m_editorWidget->getInjectedRegistry();
    if (!registry)
        return result;

    const auto& creators = registry->registeredModelCreators();
    const auto& association = registry->registeredModelsCategoryAssociation();
    result.reserve(static_cast<int>(creators.size()));
    for (auto it = creators.begin(); it != creators.end(); ++it) {
        NodeTypeInfo info;
        info.type = it->first;
        const auto catIt = association.find(it->first);
        if (catIt != association.end())
            info.category = catIt->second;
        result.append(info);
    }
    std::sort(result.begin(), result.end(),
              [](const NodeTypeInfo& a, const NodeTypeInfo& b) {
                  if (a.category != b.category)
                      return a.category < b.category;
                  return a.type < b.type;
              });
    return result;
}

QVector<Daqster::RuntimeController::ConnectionInfo> RuntimeShell::connections() const
{
    QVector<ConnectionInfo> result;
    auto* graph = graphModel();
    if (!graph)
        return result;

    const QJsonArray edges = graph->save().value(QStringLiteral("connections")).toArray();
    result.reserve(edges.size());
    for (const QJsonValue& value : edges) {
        const QJsonObject c = value.toObject();
        ConnectionInfo info;
        info.outNodeId = static_cast<quint64>(c.value(QStringLiteral("outNodeId")).toInt());
        info.outPortIndex = c.value(QStringLiteral("outPortIndex")).toInt();
        info.inNodeId = static_cast<quint64>(c.value(QStringLiteral("inNodeId")).toInt());
        info.inPortIndex = c.value(QStringLiteral("inPortIndex")).toInt();
        result.append(info);
    }
    return result;
}

QJsonObject RuntimeShell::nodeDetail(quint64 nodeId) const
{
    auto* graph = graphModel();
    if (!graph)
        return {};

    const QtNodes::NodeId id = static_cast<QtNodes::NodeId>(nodeId);
    auto* model = graph->delegateModel<QtNodes::NodeDelegateModel>(id);
    if (!model)
        return {};

    QJsonObject ports;
    QJsonArray inPorts;
    for (unsigned int i = 0; i < model->nPorts(QtNodes::PortType::In); ++i) {
        inPorts.append(QJsonObject{{QStringLiteral("index"), static_cast<int>(i)},
                                   {QStringLiteral("caption"),
                                    model->portCaption(QtNodes::PortType::In,
                                                       static_cast<QtNodes::PortIndex>(i))}});
    }
    QJsonArray outPorts;
    for (unsigned int i = 0; i < model->nPorts(QtNodes::PortType::Out); ++i) {
        outPorts.append(QJsonObject{{QStringLiteral("index"), static_cast<int>(i)},
                                    {QStringLiteral("caption"),
                                     model->portCaption(QtNodes::PortType::Out,
                                                        static_cast<QtNodes::PortIndex>(i))}});
    }
    ports.insert(QStringLiteral("in"), inPorts);
    ports.insert(QStringLiteral("out"), outPorts);

    QJsonObject position;
    const QVariant pos = graph->nodeData(id, QtNodes::NodeRole::Position);
    if (pos.canConvert<QPointF>()) {
        const QPointF p = pos.toPointF();
        position.insert(QStringLiteral("x"), p.x());
        position.insert(QStringLiteral("y"), p.y());
    }

    QJsonObject out;
    out.insert(QStringLiteral("id"), static_cast<qint64>(id));
    out.insert(QStringLiteral("type"), model->name());
    out.insert(QStringLiteral("name"), model->caption());
    // The visible label lives on the graph model (NodeRole::Label), not on the
    // delegate model — model->label() is the type's default annotation.
    out.insert(QStringLiteral("label"),
               graph->nodeData(id, QtNodes::NodeRole::Label).toString());
    out.insert(QStringLiteral("labelVisible"),
               graph->nodeData(id, QtNodes::NodeRole::LabelVisible).toBool());
    out.insert(QStringLiteral("state"),
               m_runningNodes.value(id, false) ? QStringLiteral("running")
                                               : QStringLiteral("stopped"));
    out.insert(QStringLiteral("position"), position);
    out.insert(QStringLiteral("ports"), ports);
    out.insert(QStringLiteral("config"), model->save());
    out.insert(QStringLiteral("schema"), typeSchema(model->name()));
    return out;
}

void RuntimeShell::deembedNewNode(QtNodes::NodeId nodeId)
{
    if (!m_editorWidget || !m_editorWidget->scene() || m_mdiAreas.empty())
        return;

    QtNodes::NodeGraphicsObject* node = m_editorWidget->scene()->nodeGraphicsObject(nodeId);
    if (node == nullptr || !node->isWidgetEmbedded())
        return;

    auto* model = m_editorWidget->graphModel()->delegateModel<QtNodes::NodeDelegateModel>(nodeId);
    QWidget* widget = m_editorWidget->nodeWidget(nodeId);
    if (model == nullptr || widget == nullptr)
        return;

    node->setWidgetEmbedded(false);

    QMdiArea* targetMdi = m_mdiAreas.front();
    QMdiSubWindow* subWindow = targetMdi->addSubWindow(widget);
    subWindow->setWindowTitle(model->caption());
    subWindow->resize(480, 320);
    clearWindowFlags(widget);
    subWindow->show();
}

quint64 RuntimeShell::createNode(const QString& type, double x, double y,
                                 const QJsonObject& config, QString* errorMessage)
{
    auto* graph = graphModel();
    if (!graph || !m_editorWidget) {
        if (errorMessage)
            *errorMessage = tr("runtime canvas not ready");
        return 0;
    }

    auto* registry = m_editorWidget->getInjectedRegistry();
    if (!registry || !registry->registeredModelCreators().count(type)) {
        if (errorMessage)
            *errorMessage = tr("unknown node type: %1").arg(type);
        return 0;
    }

    const QtNodes::NodeId id = graph->addNode(type);
    graph->setNodeData(id, QtNodes::NodeRole::Position, QPointF(x, y));

    if (!config.isEmpty()) {
        if (auto* model = graph->delegateModel<QtNodes::NodeDelegateModel>(id))
            model->load(config);
    }

    // Remember the node as a deembedded widget so flowJson()/reloadFlow()
    // round-trip the layout it now has on screen.
    FlowUi::NodeUi nui;
    nui.deembedded = true;
    nui.workspace = 0;
    m_uiSection.nodes.insert(id, nui);

    deembedNewNode(id);
    emit nodeCreated(id);
    return static_cast<quint64>(id);
}

bool RuntimeShell::removeNode(quint64 nodeId, QString* errorMessage)
{
    auto* graph = graphModel();
    if (!graph) {
        if (errorMessage)
            *errorMessage = tr("runtime canvas not ready");
        return false;
    }

    const QtNodes::NodeId id = static_cast<QtNodes::NodeId>(nodeId);
    if (!graph->delegateModel<QtNodes::NodeDelegateModel>(id)) {
        if (errorMessage)
            *errorMessage = tr("node not found");
        return false;
    }

    const QVector<ConnectionInfo> edges = connections();
    graph->deleteNode(id);
    m_runningNodes.remove(id);
    m_uiSection.nodes.remove(id);

    for (const ConnectionInfo& c : edges) {
        if (c.outNodeId == nodeId || c.inNodeId == nodeId)
            emit connectionDeleted(c.outNodeId, c.outPortIndex, c.inNodeId, c.inPortIndex);
    }
    emit nodeDeleted(id);
    return true;
}

bool RuntimeShell::setNodePosition(quint64 nodeId, double x, double y)
{
    auto* graph = graphModel();
    if (!graph)
        return false;
    const QtNodes::NodeId id = static_cast<QtNodes::NodeId>(nodeId);
    if (!graph->delegateModel<QtNodes::NodeDelegateModel>(id))
        return false;
    graph->setNodeData(id, QtNodes::NodeRole::Position, QPointF(x, y));
    emit nodeMoved(id);
    return true;
}

bool RuntimeShell::setNodeLabel(quint64 nodeId, const QString& label, bool visible)
{
    auto* graph = graphModel();
    if (!graph)
        return false;
    const QtNodes::NodeId id = static_cast<QtNodes::NodeId>(nodeId);
    if (!graph->delegateModel<QtNodes::NodeDelegateModel>(id))
        return false;
    graph->setNodeData(id, QtNodes::NodeRole::Label, label);
    graph->setNodeData(id, QtNodes::NodeRole::LabelVisible, visible);
    emit nodeMoved(id);
    return true;
}

bool RuntimeShell::addConnection(const ConnectionInfo& connection, QString* errorMessage)
{
    auto* graph = graphModel();
    if (!graph) {
        if (errorMessage)
            *errorMessage = tr("runtime canvas not ready");
        return false;
    }

    const QtNodes::ConnectionId c{
        static_cast<QtNodes::NodeId>(connection.outNodeId),
        static_cast<QtNodes::PortIndex>(connection.outPortIndex),
        static_cast<QtNodes::NodeId>(connection.inNodeId),
        static_cast<QtNodes::PortIndex>(connection.inPortIndex)};

    if (!graph->connectionPossible(c)) {
        if (errorMessage)
            *errorMessage = tr("connection not possible");
        return false;
    }
    graph->addConnection(c);
    emit connectionCreated(connection.outNodeId, connection.outPortIndex,
                           connection.inNodeId, connection.inPortIndex);
    return true;
}

bool RuntimeShell::removeConnection(const ConnectionInfo& connection)
{
    auto* graph = graphModel();
    if (!graph)
        return false;

    const QtNodes::ConnectionId c{
        static_cast<QtNodes::NodeId>(connection.outNodeId),
        static_cast<QtNodes::PortIndex>(connection.outPortIndex),
        static_cast<QtNodes::NodeId>(connection.inNodeId),
        static_cast<QtNodes::PortIndex>(connection.inPortIndex)};

    if (!graph->deleteConnection(c))
        return false;
    emit connectionDeleted(connection.outNodeId, connection.outPortIndex,
                           connection.inNodeId, connection.inPortIndex);
    return true;
}

bool RuntimeShell::saveFlow(const QString& path, QString* errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage)
            *errorMessage = tr("cannot open %1 for writing").arg(path);
        return false;
    }
    if (file.write(QJsonDocument(flowJson()).toJson(QJsonDocument::Indented)) < 0) {
        if (errorMessage)
            *errorMessage = tr("cannot write %1").arg(path);
        file.close();
        return false;
    }
    file.close();
    return true;
}

bool RuntimeShell::reloadFlow(const QJsonObject& flow, QString* errorMessage)
{
    if (!graphModel()) {
        if (errorMessage) *errorMessage = tr("runtime canvas not ready");
        emit errorOccurred(tr("runtime canvas not ready"));
        return false;
    }

    // Stop the old graph before it is replaced: clearScene() destroys the node
    // models, which must not be left running.
    stopAllNodes();

    QStringList skippedTypes;
    QString error;
    if (!applySceneJson(flow, &skippedTypes, &error)) {
        if (errorMessage) *errorMessage = error;
        return false;
    }

    // Rebuild the MDI layout and re-run autoStart against the new UI section.
    arrangeWorkspaces(m_uiSection);
    autoStartNodes(m_uiSection);

    if (!skippedTypes.isEmpty())
        qCWarning(lcNodeEditor) << "reloadFlow: skipped unregistered node types:"
                                << skippedTypes.join(QStringLiteral(", "));

    qCInfo(lcNodeEditor) << "RuntimeShell: flow reloaded via control plane, nodes="
                         << graphModel()->allNodeIds().size();
    return true;
}

QMdiSubWindow* RuntimeShell::createSubWindow(QWidget* widget, const FlowUi::Geometry& geometry)
{
    // This is a helper but we inline the logic in arrangeWorkspaces for now
    Q_UNUSED(widget);
    Q_UNUSED(geometry);
    return nullptr;
}

void RuntimeShell::clearWindowFlags(QWidget* widget)
{
    if (!widget) return;

    // After reparenting via setWidget(), the widget should have Qt::Widget flags.
    // However, on some Qt versions/platforms, WindowStaysOnTopHint may persist.
    // Explicitly clear it and ensure Qt::Widget flag is set.
    Qt::WindowFlags flags = widget->windowFlags();
    if (flags & Qt::WindowStaysOnTopHint) {
        flags &= ~Qt::WindowStaysOnTopHint;
        widget->setWindowFlags(flags);
    }
    // Ensure it's a widget (not a window) when embedded in MDI
    if (!(flags & Qt::Widget)) {
        widget->setWindowFlags(Qt::Widget);
    }
}
#include "RuntimeShell.moc"
