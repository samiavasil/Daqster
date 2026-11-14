#include "NodeEditorIdeObject.h"
#include "NodeEditorWidget.h"
#include "LayoutDesignerPanel.h"
#include "SceneProblemsPanel.h"
#include "NodeProblems.h"
#include "StatusBarController.h"
#include "PluginMenuController.h"
#include "QPluginManager.h"
#include "QPluginManagerGui.h"
#include "capabilities/INodeProvider.h"
#include "capabilities/IStatusBarProvider.h"
#include "capabilities/IMenuProvider.h"
#include "capabilities/IProblemFixer.h"
#include "capabilities/IWidgetProvider.h"
#include "debug.h"
#include "LogCategories.h"
#include "Scene/SceneVariables.h"

#include <QMainWindow>
#include <QLabel>
#include <QLayout>
#include <QPushButton>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QAction>
#include <QCheckBox>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include "SceneClose.h"
#include "SceneOpen.h"
#include <QMessageBox>
#include <QSet>
#include <QDir>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QToolBar>
#include <QTimer>

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

// REQ-SW-PL-051: no built-in node model headers here — they are owned by
// nodeditor_nodes/core and reach the editor through INodeProvider.

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

    // Phase 1+2: Register built-in nodes + external INodeProvider plugins
    registerNodes();

    // REQ-SW-PL-051: plugins may load in any order. When a new INodeProvider
    // plugin appears (or is reloaded), re-discover and register its nodes.
    Daqster::QPluginManager* pm = Daqster::QPluginManager::instance();
    if (pm) {
        connect(pm, &Daqster::QPluginManager::PluginsListChangeDetected,
                this, &NodeEditorIdeObject::discoverAndRegisterExternalNodes);
    }

    // REQ-SW-PL-051: registerNodes() above already picked up both the node
    // providers and the widget providers (see adoptWidgetProviders).

    // Build canvas AFTER all nodes are registered
    m_Widget->buildCanvas();

    // REQ-SW-PL-082: live node validation errors reach the panel through the
    // graph's own signals. The timer only coalesces a burst (a display node
    // emits nodeUpdated per frame) into one refresh per event-loop turn — it is
    // not a poll and it is stopped on close.
    m_problemRefreshTimer = new QTimer(this);
    m_problemRefreshTimer->setSingleShot(true);
    m_problemRefreshTimer->setInterval(0);
    connect(m_problemRefreshTimer, &QTimer::timeout,
            this, &NodeEditorIdeObject::refreshNodeProblems);

    // "Unsaved changes" has to be an observation, not a guess (REQ-SW-PL-078
    // AC1): the close prompt is only useful if it is right.
    trackSceneDirtyState();

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

    // ── Close Scene (REQ-SW-PL-078) ────────────────────────────────────────
    // The missing half of "Load Scene…": the IDE could open a scene but never
    // let go of one. Ctrl+W on Linux/Windows (QKeySequence::Close), and the
    // same action on a toolbar so the button is visible without a menu.
    fileMenu->addSeparator();
    m_closeSceneAction = fileMenu->addAction(tr("Close Scene"));
    m_closeSceneAction->setShortcut(QKeySequence::Close);
    m_closeSceneAction->setToolTip(
        tr("Stop every node, release what the plugins hold and empty the canvas"));
    connect(m_closeSceneAction, &QAction::triggered, this, [this]() {
        closeScene();
    });

    QToolBar* sceneToolBar = m_Win->addToolBar(tr("Scene"));
    sceneToolBar->setObjectName(QStringLiteral("sceneToolBar"));
    sceneToolBar->setMovable(false);
    sceneToolBar->addAction(saveAction);
    sceneToolBar->addAction(loadAction);
    sceneToolBar->addAction(m_closeSceneAction);

    // ── Layout Designer (REQ-SW-PL-059): F6 visual layout editor ───────────
    m_designerAction = fileMenu->addAction(tr("Layout Designer"));
    m_designerAction->setShortcut(Qt::Key_F6);
    m_designerAction->setCheckable(true);
    connect(m_designerAction, &QAction::toggled, this, [this](bool enabled) {
        setLayoutDesignerMode(enabled);
    });

    l->addWidget(m_Widget);

    // ── Scene problems (REQ-SW-PL-082) ─────────────────────────────────────
    // Under the canvas: the verdict of `ISceneOpening` for the scene that was
    // just loaded. Collapsed until there is something to say, so a clean scene
    // does not lose canvas height to an empty list.
    m_problemsPanel = new SceneProblemsPanel(mainWidget);
    m_problemsPanel->setVisible(false);
    connect(m_problemsPanel, &SceneProblemsPanel::problemActivated,
            this, &NodeEditorIdeObject::selectSceneNode);
    // REQ-SW-PL-085: a fixer's success must re-read the scene, not trust it.
    connect(m_problemsPanel, &SceneProblemsPanel::fixRequested,
            this, &NodeEditorIdeObject::onProblemFixed);
    l->addWidget(m_problemsPanel);

    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(0);

    // ── Status bar + plugin menu (REQ-SW-PL-079) ───────────────────────────
    // A plugin's global state (warm model weights, VRAM held, a background
    // job) belongs to no node, so no node widget can show it — until now the
    // only window into it was nvidia-smi. Each IStatusBarProvider gets one
    // permanent label here (permanent = a transient message never hides it)
    // and its actions in Tools → Plugins. The controller owns that submenu.
    QMenu* toolsMenu = m_Win->menuBar()->addMenu(tr("&Tools"));
    QMenu* pluginsMenu = toolsMenu->addMenu(tr("&Plugins"));
    pluginsMenu->menuAction()->setVisible(false);   // until a provider offers an action

    if (m_statusBar)
        delete m_statusBar.data();
    m_statusBar = new StatusBarController(m_Win->statusBar(), pluginsMenu, this);
    // A provider object went away (plugin unload/reload): re-discover so its
    // row comes back if the plugin is still there.
    connect(m_statusBar.data(), &StatusBarController::providerDied,
            this, &NodeEditorIdeObject::discoverAndRegisterExternalNodes);
    m_statusBar->startAutoRefresh();
    adoptStatusBarProviders();

    const QStringList statusRowIds =
        m_statusBar ? m_statusBar->statusIds() : QStringList();
    qCInfo(lcNodeEditor) << "Status bar ready:" << statusRowIds.size()
                         << "IStatusBarProvider row(s):"
                         << statusRowIds.join(QStringLiteral(", "));

    // ── Plugin menu actions (REQ-SW-PL-084) ────────────────────────────────
    // The menus above exist, so a plugin can put its entry where it belongs
    // (Tools → AI Setup) without the IDE knowing the plugin: it implements
    // IMenuProvider, the controller adds the QAction and calls back by id.
    installPluginMenuActions();

    // REQ-SW-PL-085: the plugin that reported a problem may also fix it.
    adoptProblemFixers();

    // The designer panel lives next to the canvas; only one of the two is
    // visible at a time (see setLayoutDesignerMode).
    m_designer = new LayoutDesignerPanel(mainWidget);
    m_designer->setVisible(false);
    m_designer->setProvider([this](QtNodes::NodeId nodeId) -> QWidget* {
        if (!m_Widget)
            return nullptr;
        // Handed over still deembedded (a hidden top-level window): the engine
        // reparents it into its container and LayoutEngine drops the leftover
        // window flags afterwards. Stripping them here - while the widget is
        // still parentless - would leave a stale QWindow behind and break
        // mapToGlobal (popups would open far from the mouse).
        return m_Widget->nodeWidget(nodeId);
    });
    connect(m_designer, &LayoutDesignerPanel::saveRequested,
            this, &NodeEditorIdeObject::saveLayoutFromDesigner);
    connect(m_designer, &LayoutDesignerPanel::closed, this, [this]() {
        if (m_designerAction)
            m_designerAction->setChecked(false);
        else
            setLayoutDesignerMode(false);
    });
    l->addWidget(m_designer);

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

void NodeEditorIdeObject::registerNodes()
{
    // REQ-SW-PL-051: the built-in models live in nodeditor_nodes/core and
    // are registered by its INodeProvider, together with every other node.
    discoverAndRegisterExternalNodes();
}

// ── Runtime mode (REQ-SW-PL-053) ─────────────────────────────────────────
// Running a .flow with deembedded widgets as the UI is no longer an editor
// responsibility: the visible runtime lives in RuntimeHostGuiPlugin's
// RuntimeShell, reached by the runner through QPluginManager::runtimeHosts().
// The editor keeps only its own F11 presentation mode (see setPresentationMode).

void NodeEditorIdeObject::discoverAndRegisterExternalNodes()
{
    Daqster::QPluginManager* pm = Daqster::QPluginManager::instance();
    if (!pm) return;

    // The editor window can already be gone when this runs: the
    // IWidgetProvider 'destroyed' re-discovery below fires while
    // QPluginManager::ShutdownPluginManager destroys the plugin objects, and
    // MainWinDestroyed() has nulled m_Widget by then. Registering nodes into a
    // destroyed editor segfaulted on quit.
    if (!m_Widget) return;

    auto* registry = m_Widget->getInjectedRegistry();

    for (Daqster::INodeProvider* provider : pm->nodeProviders()) {
        if (!provider) continue;

        DEBUG << "Discovered INodeProvider plugin";

        provider->registerNodes(*registry);
    }

    // REQ-SW-PL-051: also discover IWidgetProvider plugins (they may load
    // in any order). Pass all of them to the editor widget so split models
    // get their widgets.
    adoptWidgetProviders(pm->widgetProviders());

    // REQ-SW-PL-079: plugins may load after the window is up; their status row
    // appears with them (and disappears when they are unloaded).
    adoptStatusBarProviders();

    // REQ-SW-PL-084: the same for menu actions — a plugin loaded after the
    // menus were built still gets its entry (setProviders() is idempotent).
    installPluginMenuActions();

    // REQ-SW-PL-085: and for problem fixers ("Поправи" on a reported row).
    adoptProblemFixers();
}

void NodeEditorIdeObject::adoptStatusBarProviders()
{
    if (!m_statusBar)
        return;

    Daqster::QPluginManager* pm = Daqster::QPluginManager::instance();
    m_statusBar->setProviders(pm ? pm->statusBarProviders()
                                 : QList<Daqster::IStatusBarProvider*>());
}

void NodeEditorIdeObject::installPluginMenuActions()
{
    if (!m_Win || !m_Win->menuBar())
        return;

    if (!m_pluginMenu)
        m_pluginMenu = new PluginMenuController(m_Win->menuBar(), this);

    Daqster::QPluginManager* pm = Daqster::QPluginManager::instance();
    m_pluginMenu->setProviders(pm ? pm->menuProviders()
                                  : QList<Daqster::IMenuProvider*>());

    qCInfo(lcNodeEditor) << "Plugin menu ready:" << m_pluginMenu->actionCount()
                         << "action(s) from IMenuProvider plugin(s)";
}

void NodeEditorIdeObject::adoptProblemFixers()
{
    if (m_problemsPanel == nullptr)
        return;

    Daqster::QPluginManager* pm = Daqster::QPluginManager::instance();
    m_problemFixers = pm ? pm->problemFixers()
                         : QList<Daqster::IProblemFixer*>();

    // First fixer that says it handles the row wins; the panel asks for a label
    // only when there is one, so a row without a fixer keeps its plain text.
    m_problemsPanel->setFixHandler(
        [this](const Daqster::SceneOpenProblem& problem) -> QString {
            for (Daqster::IProblemFixer* fixer : m_problemFixers) {
                if (fixer != nullptr && fixer->canFix(problem))
                    return fixer->fixLabel(problem);
            }
            return QString();
        },
        [this](const Daqster::SceneOpenProblem& problem, QString* error) -> bool {
            for (Daqster::IProblemFixer* fixer : m_problemFixers) {
                if (fixer != nullptr && fixer->canFix(problem))
                    return fixer->fix(problem, error);
            }
            if (error != nullptr)
                *error = tr("Няма плъгин, който да поправи този проблем");
            return false;
        });

    qCInfo(lcNodeEditor) << "Problem fixers:" << m_problemFixers.size()
                         << "provider(s) from IProblemFixer plugin(s)";
}

void NodeEditorIdeObject::onProblemFixed(const Daqster::SceneOpenProblem& problem)
{
    // The fixer says the problem is gone; drop the evidence the panel was
    // built from and unmark the node, then let the graph be the judge.
    for (int i = m_sceneOpenProblems.size() - 1; i >= 0; --i) {
        const Daqster::SceneOpenProblem& stored = m_sceneOpenProblems.at(i);
        if (stored.nodeId == problem.nodeId && stored.message == problem.message)
            m_sceneOpenProblems.removeAt(i);
    }

    if (!problem.nodeId.isEmpty() && m_Widget != nullptr
        && m_Widget->graphModel() != nullptr) {
        auto* model = m_Widget->graphModel()->delegateModel<QtNodes::NodeDelegateModel>(
            static_cast<QtNodes::NodeId>(problem.nodeId.toUInt()));
        if (model != nullptr)
            model->setValidationState({QtNodes::NodeValidationState::State::Valid, QString()});
    }

    qCInfo(lcNodeEditor).noquote() << "problem fixed:" << problem.nodeId
                                   << problem.message;
    refreshNodeProblems();
}

void NodeEditorIdeObject::adoptWidgetProviders(const QList<Daqster::IWidgetProvider*>& providers)
{
    if (providers.isEmpty() || !m_Widget)
        return;

    m_Widget->setWidgetProviders(providers);

    // The graph model caches the raw pointers, so re-resolve them when any
    // plugin object dies (plugin reload / unload). PluginRegistry recreates
    // fresh, initialized objects on the next widgetProviders() call.
    for (Daqster::IWidgetProvider* provider : providers) {
        if (auto* owner = dynamic_cast<QObject*>(provider)) {
            connect(owner, &QObject::destroyed, this, [this]() {
                DEBUG << "IWidgetProvider plugin object destroyed, re-discovering";
                discoverAndRegisterExternalNodes();
            });
        }
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
    // Application/plugin shutdown that does not go through the editor window's
    // close event (e.g. QPluginManager::ShutdownPluginManager on aboutToQuit).
    // IStoppable::stop() is idempotent, so nodes already stopped by the close
    // event are not stopped twice. m_Widget is nulled by MainWinDestroyed as
    // soon as the window takes the graph down with it.
    if (nullptr != m_Widget) {
        m_Widget->stopAllNodes();
    }

    if (nullptr != m_Win) {
        m_Win->deleteLater();
    }
    DEBUG_V << "NodeEditorIdeObject destroyed";
}

void NodeEditorIdeObject::MainWinDestroyed(QObject* obj)
{
    m_Win = nullptr;
    m_Widget = nullptr;
    m_designer = nullptr;
    m_designerAction = nullptr;
    m_problemsPanel = nullptr;
    deleteLater();
    if (nullptr == obj)
        DEBUG << "Strange::!!!";
}

void NodeEditorIdeObject::ShowPlugins()
{
    Daqster::QPluginManager* pm = Daqster::QPluginManager::instance();
    if (nullptr != pm) {
        DEBUG << "Plugin Manager: " << pm;
        auto* managerDialog = new Daqster::QPluginManagerGui(m_Win);
        managerDialog->setAttribute(Qt::WA_DeleteOnClose, true);
        managerDialog->show();
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

    // Closing the editor window (the X button) tears the graph down, so the
    // running nodes must be stopped first — the runner does the same from its
    // close handler. Without this the stop of a loaded flow is never invoked.
    if (watched == m_Win && event->type() == QEvent::Close && m_Widget) {
        m_Widget->stopAllNodes();
    }

    return QObject::eventFilter(watched, event);
}

// ── Close Scene (REQ-SW-PL-078) ──────────────────────────────────────────────
// The IDE could open a scene but never close one: the only teardown was the
// window's X, which took the whole editor with it. "Close Scene" releases the
// scene and leaves the IDE running, ready for the next one.
void NodeEditorIdeObject::trackSceneDirtyState()
{
    if (m_Widget == nullptr || m_Widget->graphModel() == nullptr)
        return;

    auto* graph = m_Widget->graphModel();
    auto markDirty = [this]() { m_sceneDirty = true; };

    // Every mutation that would be lost on close. Signals carry ids we do not
    // need: the question is only "did anything change since the last save?".
    connect(graph, &QtNodes::DataFlowGraphModel::nodeCreated,
            this, [markDirty](QtNodes::NodeId) { markDirty(); });
    connect(graph, &QtNodes::DataFlowGraphModel::nodeDeleted,
            this, [markDirty](QtNodes::NodeId) { markDirty(); });
    connect(graph, &QtNodes::DataFlowGraphModel::nodeUpdated,
            this, [markDirty](QtNodes::NodeId) { markDirty(); });
    connect(graph, &QtNodes::DataFlowGraphModel::nodePositionUpdated,
            this, [markDirty](QtNodes::NodeId) { markDirty(); });
    connect(graph, &QtNodes::DataFlowGraphModel::connectionCreated,
            this, [markDirty](QtNodes::ConnectionId) { markDirty(); });
    connect(graph, &QtNodes::DataFlowGraphModel::connectionDeleted,
            this, [markDirty](QtNodes::ConnectionId) { markDirty(); });

    // REQ-SW-PL-082: a node's validation state is part of what the user must
    // read, and `setValidationState()` emits `requestNodeUpdate()` →
    // `nodeUpdated`. Coalesced, so a node that updates every frame does not
    // rebuild the list every frame.
    const auto scheduleProblemsRefresh = [this](QtNodes::NodeId) {
        if (m_problemRefreshTimer != nullptr && !m_problemRefreshTimer->isActive())
            m_problemRefreshTimer->start();
    };
    connect(graph, &QtNodes::DataFlowGraphModel::nodeUpdated,
            this, scheduleProblemsRefresh);
    connect(graph, &QtNodes::DataFlowGraphModel::nodeCreated,
            this, scheduleProblemsRefresh);
    connect(graph, &QtNodes::DataFlowGraphModel::nodeDeleted,
            this, scheduleProblemsRefresh);
    connect(graph, &QtNodes::DataFlowGraphModel::modelReset, this, [this]() {
        if (m_problemRefreshTimer != nullptr)
            m_problemRefreshTimer->start();
    });
}

bool NodeEditorIdeObject::closeCurrentScene(bool confirmDiscard)
{
    if (m_Widget == nullptr || m_Widget->graphModel() == nullptr
        || m_Widget->scene() == nullptr) {
        qCWarning(lcNodeEditor) << "closeScene: no canvas — nothing to close";
        return false;
    }

    auto* graph = m_Widget->graphModel();
    const int nodeCount = static_cast<int>(graph->allNodeIds().size());
    if (nodeCount == 0) {
        // Closing an empty scene (or closing twice) is a no-op, never an error
        // and never a second plugin notification — a warm model must not be
        // released by a close that changed nothing.
        qCInfo(lcNodeEditor) << "closeScene: scene is already empty — no-op";
        m_sceneDirty = false;
        return true;
    }

    if (confirmDiscard && m_sceneDirty) {
        const auto answer = QMessageBox::question(
            m_Win, tr("Close Scene"), tr("Затвори без запазване?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            qCInfo(lcNodeEditor) << "closeScene: cancelled by the user (unsaved changes)";
            return false;
        }
    }

    // The layout designer holds its own preview of node widgets; leaving it on
    // while the nodes disappear would keep dangling pointers on screen.
    if (m_designerMode)
        setLayoutDesignerMode(false);

    qCInfo(lcNodeEditor) << "closeScene: closing" << nodeCount << "nodes";

    // 1+2. Stop every node (sources first) and notify the plugins while the
    //      models are still alive — the shared contract every host uses.
    const Daqster::SceneCloseReport report = Daqster::stopSceneNodes(*graph);
    // File → Close Scene is the one close the user asked for: plugins are told
    // *why* the scene goes away. What they keep resident is their own policy —
    // the reason is a hint, not a release order (REQ-SW-PL-078).
    const int notified = Daqster::notifySceneClosed(Daqster::SceneCloseReason::ExplicitClose);

    // 3. Free the graph: clearScene() deletes the node models, which is only
    //    safe now that each of them was stopped.
    m_Widget->scene()->clearScene();

    // 4. Forget everything that described the old scene.
    m_autoStartNodes.clear();
    m_workspaces.clear();
    m_hasLayoutV2 = false;
    m_layoutSectionV2 = FlowUi::LayoutSection{};
    m_sceneDirty = false;

    // The problems described nodes that no longer exist (REQ-SW-PL-082).
    if (m_problemRefreshTimer != nullptr)
        m_problemRefreshTimer->stop();
    m_sceneOpenProblems.clear();
    m_lastPanelProblems.clear();
    if (m_problemsPanel != nullptr)
        m_problemsPanel->setProblems({});

    qCInfo(lcNodeEditor) << "closeScene: released" << report.stopped << "of" << report.nodes
                         << "running nodes, plugins notified:" << notified
                         << "— canvas is empty";

    // The closed scene may have held resources the status row reports (warm
    // weights, live contexts). Do not wait up to the 2 s tick to tell the user.
    if (m_statusBar)
        m_statusBar->refresh();

    return true;
}

void NodeEditorIdeObject::closeScene()
{
    closeCurrentScene(/*confirmDiscard=*/true);
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
            // The graph model's cached instance - the very widget the proxy
            // holds. Going to the IWidgetProvider directly would build a SECOND
            // widget for the same model and orphan the first one.
            QWidget* w = m_Widget->nodeWidget(nodeId);
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
            QWidget* w = m_Widget->nodeWidget(nodeId);
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

// ── Layout Designer (REQ-SW-PL-059) ─────────────────────────────────────────
// F6 toggles a visual editor for the schema-v2 "ui" section. It works on a
// FlowUi::LayoutSection (split/grid/tab/panel + docks), renders it live with
// LayoutEngine::buildWorkspace() and serializes it back on save.
void NodeEditorIdeObject::setLayoutDesignerMode(bool enabled)
{
    if (!m_designer || !m_Widget || !m_Win)
        return;
    if (enabled == m_designerMode)
        return;

    if (enabled) {
        // The engine reparents node widgets into the preview, so they must be
        // detached from the canvas first (same two-step deembed as F11).
        deembedAllWidgets();

        if (m_hasLayoutV2)
            m_designer->setLayoutSection(m_layoutSectionV2);
        else {
            FlowUi::LayoutSection section;
            section.version = 2;
            m_designer->setLayoutSection(section);
        }
        m_designer->setAvailableNodes(collectAvailableNodes());

        m_Widget->hide();
        m_designer->show();
        m_designer->refresh();

        qCInfo(lcNodeEditor) << "Layout designer: ON";
    } else {
        // Keep what the user designed so the next save writes version 2.
        m_layoutSectionV2 = m_designer->layoutSection();
        m_layoutSectionV2.version = 2;
        m_hasLayoutV2 = true;

        m_designer->hide();
        m_Widget->show();
        reembedAllWidgets();

        qCInfo(lcNodeEditor) << "Layout designer: OFF (v2 section kept)";
    }

    m_designerMode = enabled;
}

void NodeEditorIdeObject::deembedAllWidgets()
{
    if (!m_Widget || !m_Widget->scene())
        return;

    for (const QtNodes::NodeId nodeId : m_Widget->graphModel()->allNodeIds()) {
        QtNodes::NodeGraphicsObject* node = m_Widget->scene()->nodeGraphicsObject(nodeId);
        if (node == nullptr || !node->hasWidget() || !node->isWidgetEmbedded())
            continue;

        node->setWidgetEmbedded(false);

        QWidget* w = m_Widget->nodeWidget(nodeId);
        if (w != nullptr)
            w->hide(); // keep the top-level flags until the engine reparents it
    }
}

void NodeEditorIdeObject::reembedAllWidgets()
{
    if (!m_Widget || !m_Widget->scene())
        return;

    for (const QtNodes::NodeId nodeId : m_Widget->graphModel()->allNodeIds()) {
        QtNodes::NodeGraphicsObject* node = m_Widget->scene()->nodeGraphicsObject(nodeId);
        if (node == nullptr || !node->hasWidget() || node->isWidgetEmbedded())
            continue;

        QWidget* w = m_Widget->nodeWidget(nodeId);
        if (w != nullptr)
            w->hide();

        node->setWidgetEmbedded(true);
    }
}

QVector<QPair<QtNodes::NodeId, QString>> NodeEditorIdeObject::collectAvailableNodes() const
{
    QVector<QPair<QtNodes::NodeId, QString>> nodes;
    if (m_Widget == nullptr || m_Widget->scene() == nullptr)
        return nodes;

    for (const QtNodes::NodeId nodeId : m_Widget->graphModel()->allNodeIds()) {
        QtNodes::NodeGraphicsObject* node = m_Widget->scene()->nodeGraphicsObject(nodeId);
        if (node == nullptr || !node->hasWidget())
            continue;

        auto* model =
            m_Widget->graphModel()->delegateModel<QtNodes::NodeDelegateModel>(nodeId);
        QString label = model != nullptr ? model->caption() : QString();
        if (label.isEmpty())
            label = tr("Node");
        nodes.append({nodeId, tr("%1 (#%2)").arg(label).arg(nodeId)});
    }
    return nodes;
}

void NodeEditorIdeObject::saveLayoutFromDesigner()
{
    if (m_designer == nullptr)
        return;

    m_layoutSectionV2 = m_designer->layoutSection();
    m_layoutSectionV2.version = 2;
    m_hasLayoutV2 = true;

    if (saveSceneToFile())
        qCInfo(lcNodeEditor) << "Layout designer: layout saved as ui version 2";
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

    // REQ-SW-PL-091: a relative `filePath` in a scene means "next to this file",
    // so the host records the scene's directory before the graph model sees a
    // single node — the nodes resolve their paths while `load()`. Recorded here,
    // not in the node: the node cannot know where the JSON came from.
    Daqster::SceneVariables::instance().adoptSceneFile(fileName);

    // Extract the "ui" section (REQ-SW-PL-049 / v2 REQ-SW-PL-059) BEFORE the
    // node-cleaning loop: it is not part of the graph model JSON and must not be
    // passed to load(). Missing "ui" (old flow) → empty section → current
    // behavior. version>=2 dispatches to the recursive LayoutSection model, v1
    // keeps the flat UiSection path.
    const QJsonObject uiJson = sceneJson["ui"].toObject();
    const int uiVersion = uiJson["version"].toInt(1);

    FlowUi::UiSection uiSection;
    FlowUi::LayoutSection layoutV2;
    bool hasV2 = false;
    if (uiVersion >= 2) {
        layoutV2 = FlowUi::LayoutSection::fromJson(uiJson);
        hasV2 = !layoutV2.workspaces.empty();
    } else {
        uiSection = FlowUi::UiSection::fromJson(uiJson);
    }
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

    // REQ-SW-PL-082: the verdict is applied *after* load() (see below) — "зарежда,
    // маркира, не стартира". The IDE used to refuse here with a QMessageBox,
    // which left the user on an empty canvas: the graph he asked for is exactly
    // where the broken path is fixed. Asking before load() would also ask about
    // nodes the filtering above has already dropped.

    try {
        // Replacing a scene is close-then-open (REQ-SW-PL-078): stop the
        // outgoing nodes while their graph is still valid, let the plugins
        // apply their close policy, and only then let clearScene() destroy the
        // models. Without this a node's stop() was never called on
        // "Load Scene…" — only the destructor ran.
        closeCurrentScene(/*confirmDiscard=*/false);
        m_Widget->graphModel()->load(sceneJson);
    } catch (const std::exception& e) {
        qCWarning(lcNodeEditor) << "loadSceneFromFile: load failed:" << e.what();
        return false;
    }

    // REQ-SW-PL-082: the newly loaded nodes may arrive already non-Valid (a
    // model that failed to prepare while loading). applySceneProblems() below
    // re-merges the open-time verdict on top of this.
    refreshNodeProblems();

    // Restore the runtime UI layout captured in the "ui" section (REQ-SW-PL-049).
    // v2 sections are stored for the Layout Designer (REQ-SW-PL-059); the v1
    // path keeps its deembed/geometry restore.
    if (hasV2) {
        m_layoutSectionV2 = layoutV2;
        m_hasLayoutV2 = true;
        if (m_designer != nullptr)
            m_designer->setLayoutSection(layoutV2);
        qCInfo(lcNodeEditor) << "loadSceneFromFile: ui version 2 section loaded ("
                             << layoutV2.workspaces.size() << "workspaces)";
    } else {
        m_hasLayoutV2 = false;
        applyUiSection(uiSection);
    }

    // REQ-SW-PL-082: the scene is loaded — now ask the plugins whether it can
    // *run* here. The answer never closes what the user opened: offending nodes
    // are marked, the panel lists every problem and nothing is started. The
    // file path travels with the scene so a plugin can find the files next to
    // it (`project.json`, relative assets).
    applySceneProblems(Daqster::checkSceneOpening(sceneJson, fileName).problems);

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

    // The freshly loaded scene is the new "saved" baseline, not a change.
    m_sceneDirty = false;

    // A scene may have touched a plugin's global state (a model loaded, a
    // server started) — the status row has to reflect it now, not in 2 s.
    if (m_statusBar)
        m_statusBar->refresh();

    return true;
}

// ── Scene problems in the IDE (REQ-SW-PL-082) ───────────────────────────────
// "Зарежда, маркира, не стартира": the scene stays open for editing, the nodes
// the verdict names carry a NodeValidationState::Error (the canvas paints it),
// and the panel under the canvas lists every problem with a hint. There is
// deliberately no modal dialog here — the user must be able to click the
// problem, land on the node and fix it.
void NodeEditorIdeObject::applySceneProblems(
    const QVector<Daqster::SceneOpenProblem>& problems)
{
    m_sceneOpenProblems = problems;

    if (problems.isEmpty()) {
        qCInfo(lcNodeEditor) << "loadSceneFromFile: scene opening produced no problems";
        // Live validation errors the nodes carried in may still need a row.
        refreshNodeProblems();
        return;
    }

    for (const Daqster::SceneOpenProblem& problem : problems) {
        const QString text = SceneProblemsPanel::textForProblem(problem);

        if (problem.nodeId.isEmpty() || m_Widget == nullptr
            || m_Widget->graphModel() == nullptr) {
            qCWarning(lcNodeEditor).noquote()
                << "scene problem (scene-wide):" << text;
            continue;
        }

        auto* model = m_Widget->graphModel()->delegateModel<QtNodes::NodeDelegateModel>(
            static_cast<QtNodes::NodeId>(problem.nodeId.toUInt()));
        if (model != nullptr) {
            model->setValidationState(
                {QtNodes::NodeValidationState::State::Error, text});
        } else {
            qCWarning(lcNodeEditor) << "scene problem: no node" << problem.nodeId
                                    << "in the loaded scene";
        }

        qCWarning(lcNodeEditor).noquote()
            << "scene problem:" << problem.nodeId << text;
    }

    // The marked nodes now report the same text through their validation
    // state; merge() keeps one row per node+message.
    refreshNodeProblems();
}

void NodeEditorIdeObject::refreshNodeProblems()
{
    if (m_problemsPanel == nullptr)
        return;

    QVector<NodeProblems::NodeState> nodeStates;
    if (m_Widget != nullptr && m_Widget->graphModel() != nullptr) {
        auto* graph = m_Widget->graphModel();
        const auto ids = graph->allNodeIds();
        nodeStates.reserve(ids.size());
        for (const QtNodes::NodeId nodeId : ids) {
            NodeProblems::NodeState nodeState;
            nodeState.nodeId = nodeId;
            nodeState.nodeName =
                graph->nodeData(nodeId, QtNodes::NodeRole::Caption).toString();
            if (nodeState.nodeName.isEmpty()) {
                if (auto* model =
                        graph->delegateModel<QtNodes::NodeDelegateModel>(nodeId)) {
                    nodeState.nodeName = model->caption();
                }
            }
            nodeState.state = graph->nodeData(nodeId, QtNodes::NodeRole::ValidationState)
                                  .value<QtNodes::NodeValidationState>();
            nodeStates.append(nodeState);
        }
    }

    const QVector<Daqster::SceneOpenProblem> problems =
        NodeProblems::merge(m_sceneOpenProblems, nodeStates);
    if (NodeProblems::sameProblems(problems, m_lastPanelProblems))
        return;

    m_lastPanelProblems = problems;
    m_problemsPanel->setProblems(problems);

    if (!problems.isEmpty()) {
        qCInfo(lcNodeEditor) << "problems panel:" << problems.size()
                             << "problem(s), live node validation included";
    }
}

void NodeEditorIdeObject::selectSceneNode(const QString& nodeId)
{
    if (nodeId.isEmpty() || m_Widget == nullptr || m_Widget->scene() == nullptr)
        return;

    bool ok = false;
    const QtNodes::NodeId id =
        static_cast<QtNodes::NodeId>(nodeId.toUInt(&ok));
    if (!ok) {
        qCWarning(lcNodeEditor) << "selectSceneNode: not a node id:" << nodeId;
        return;
    }

    auto* scene = m_Widget->scene();
    auto* item = scene->nodeGraphicsObject(id);
    if (item == nullptr) {
        qCWarning(lcNodeEditor) << "selectSceneNode: node" << nodeId
                                << "is not on the canvas";
        return;
    }

    scene->clearSelection();
    item->setSelected(true);
    if (!scene->views().isEmpty())
        scene->views().first()->centerOn(item);
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

    if (m_hasLayoutV2) {
        // Layout Designer output (REQ-SW-PL-059): recursive schema-v2 section.
        m_layoutSectionV2.version = 2;
        sceneJson["ui"] = m_layoutSectionV2.toJson();
    } else {
        sceneJson["ui"] = captureUiSection().toJson();
    }

    file.write(QJsonDocument(sceneJson).toJson(QJsonDocument::Indented));
    m_sceneDirty = false;
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
            QWidget* w = m_Widget->nodeWidget(nodeId);
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
        // setWidgetEmbedded(false) detaches the graph model's cached widget and
        // hands its ownership back, so the SAME instance is moved out of the
        // node below — a second widget from the provider would orphan this one.
        node->setWidgetEmbedded(false);

        QWidget* w = m_Widget->nodeWidget(nodeId);
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

        QWidget* w = m_Widget->nodeWidget(srcId);
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
    // line + badge).
    auto* outModel = gm->delegateModel<QtNodes::NodeDelegateModel>(outId);
    if (outModel != nullptr) {
        QWidget* w = m_Widget->nodeWidget(outId);
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
    }
}
