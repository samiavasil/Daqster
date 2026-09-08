#include "RuntimeShell.h"
#include "NodeEditorWidget.h"
#include "QPluginManager.h"
#include "capabilities/INodeProvider.h"
#include "debug.h"
#include "LogCategories.h"
#include "shared/IStartable.h"
#include "shared/IStoppable.h"

// Built-in node models
#include "BuiltInNodes/Sources/NumberSource/NumberSourceDataModel.h"
#include "BuiltInNodes/Displays/NumberDisplay/NumberDisplayDataModel.h"
#include "BuiltInNodes/Operators/Modulo/ModuloModel.h"
#include "BuiltInNodes/Operators/ArithmeticLogic/ArithmeticLogicModel.h"

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
#include <QSet>
#include <QDir>

#include <QtNodes/NodeDelegateModel>
#include <QtNodes/NodeDelegateModelRegistry>
#include <QtNodes/DataFlowGraphModel>
#include <QtNodes/DataFlowGraphicsScene>
#include <QtNodes/internal/NodeGraphicsObject.hpp>

#include <exception>

RuntimeShell::RuntimeShell(QObject* parent)
    : Daqster::QBasePluginObject(parent)
    , m_mainWindow(nullptr)
    , m_editorWidget(nullptr)
{
}

RuntimeShell::~RuntimeShell()
{
    DeInitialize();
}

void RuntimeShell::DeInitialize()
{
    // Stop all nodes before cleanup (REQ-SW-PL-050)
    stopAllNodes();

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
    deleteLater();
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
    m_mainWindow = new QMainWindow();
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

    // 4. Load flow file
    if (!loadFlow(flowPath)) {
        // Error message already shown by loadFlow
        return false;
    }

    // 5. Parse UI section from the flow file (extract BEFORE load, like NodeEditorIdeObject does)
    QFile file(flowPath);
    FlowUi::UiSection uiSection;
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        QJsonParseError parseError{};
        QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
        if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
            QJsonObject sceneJson = doc.object();
            uiSection = FlowUi::UiSection::fromJson(sceneJson["ui"].toObject());
        }
    }

    // 6. Arrange deembedded widgets in MDI workspaces
    arrangeWorkspaces(uiSection);

    // 7. Auto-start nodes
    autoStartNodes(uiSection);

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

    // Register built-in nodes (same as NodeEditorIdeObject::registerBuiltInNodes)
    registry->registerModel<NumberSourceDataModel>("General/Sources");
    registry->registerModel<NumberDisplayDataModel>("General/Display");
    registry->registerModel<ModuloModel>("General/Processing");
    registry->registerModel<ArithmeticLogicModel>("General/Processing");

    // Discover and register external nodes (same as NodeEditorIdeObject::discoverAndRegisterExternalNodes)
    Daqster::QPluginManager* pm = Daqster::QPluginManager::instance();
    if (pm) {
        QObjectList providers = pm->instances(INodeProvider_IID);
        for (QObject* obj : providers) {
            auto* provider = qobject_cast<Daqster::INodeProvider*>(obj);
            if (!provider) continue;

            QString name = obj->property("name").toString();
            DEBUG << "RuntimeShell: Discovered INodeProvider plugin:" << name;
            provider->registerNodes(*registry);
        }
    }

    return true;
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
        return false;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(lcNodeEditor) << "loadFlow: cannot open" << fileName;
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
        QMessageBox::critical(m_mainWindow, tr("Runtime Mode"),
                              tr("Invalid JSON in flow file: %1").arg(parseError.errorString()));
        return false;
    }

    QJsonObject sceneJson = sceneDocument.object();

    // Extract the "ui" section (REQ-SW-PL-049) BEFORE the node-cleaning loop:
    // it is not part of the graph model JSON and must not be passed to load().
    const FlowUi::UiSection uiSection = FlowUi::UiSection::fromJson(sceneJson["ui"].toObject());
    sceneJson.remove("ui");

    const QJsonArray nodesJsonArray = sceneJson["nodes"].toArray();

    auto* registry = m_editorWidget->getInjectedRegistry();
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
        m_editorWidget->scene()->clearScene();
        m_editorWidget->graphModel()->load(sceneJson);
    } catch (const std::exception& e) {
        qCWarning(lcNodeEditor) << "loadFlow: load failed:" << e.what();
        QMessageBox::critical(m_mainWindow, tr("Runtime Mode"),
                              tr("Failed to load flow: %1").arg(e.what()));
        return false;
    }

    // Store workspaces and autoStart flags for later use
    m_workspaces = uiSection.workspaces;
    m_autoStartNodes.clear();
    for (auto it = uiSection.nodes.constBegin(); it != uiSection.nodes.constEnd(); ++it) {
        m_autoStartNodes.insert(it.key(), it.value().autoStart);
    }

    const int loadedNodeCount = static_cast<int>(m_editorWidget->graphModel()->allNodeIds().size());
    const int loadedConnCount = static_cast<int>(sceneJson["connections"].toArray().size());
    qCInfo(lcNodeEditor) << "loadFlow: loaded" << fileName
                         << "nodes=" << loadedNodeCount
                         << "connections=" << loadedConnCount;

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

        // Get the widget from the model
        auto* model = m_editorWidget->graphModel()->delegateModel<QtNodes::NodeDelegateModel>(nodeId);
        QWidget* widget = model != nullptr ? model->embeddedWidget() : nullptr;
        if (widget == nullptr)
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
        }
    }
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