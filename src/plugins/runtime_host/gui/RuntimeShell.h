#pragma once

#include "FlowUiSection.h"
#include "NodeEditorWidget.h"
#include "QBasePluginObject.h"
#include <QtNodes/Definitions>

#include <capabilities/IRuntimeHost.h>   // ControlPlaneOptions (REQ-SW-PL-054)
#include "RuntimeController.h"           // RuntimeController seam (REQ-SW-PL-054)

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMainWindow>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QStringList>
#include <memory>
#include <vector>

namespace Daqster {
class IWidgetProvider;
class ControlPlaneServer;
}

/**
 * @brief GUI runtime host shell (REQ-SW-PL-048/REQ-SW-PL-053).
 *
 * Owns the deembedded-widget runtime window (MDI workspaces) and implements
 * Daqster::RuntimeController so the REST/WebSocket control plane
 * (REQ-SW-PL-054) serves the same surface in GUI mode as the headless engine
 * does. The control plane is a mode-agnostic seam: `ControlPlaneServer` talks
 * only to RuntimeController.
 *
 * The controller methods resolve the graph model lazily because
 * `startControlPlane()` is invoked by the shared bootstrap *before*
 * `RunRuntime()` builds the canvas — an early request simply sees no nodes.
 */
class RuntimeShell : public Daqster::QBasePluginObject, public Daqster::RuntimeController
{
    Q_OBJECT
public:
    RuntimeShell(QObject* parent = nullptr);
    virtual ~RuntimeShell();

    /// Entry point for runtime mode (REQ-SW-PL-048): loads `flowPath` and shows
    /// deembedded widgets arranged in MDI workspaces as the application UI.
    /// Returns true on success, false on error (error message already shown).
    bool RunRuntime(const QString& flowPath);

    /// No-op implementation for QBasePluginObject interface (runtime mode uses
    /// RunRuntime instead of Initialize).
    virtual bool Initialize() override { return true; }

    // ── Remote control plane (REQ-SW-PL-054) ─────────────────────────────
    /// Start the HTTP/WebSocket control plane described by @p options.
    /// Returns false (without listening) when the options are unsafe or the
    /// bind fails, so the caller can fail loudly.
    bool startControlPlane(const Daqster::ControlPlaneOptions& options,
                           QString* errorMessage = nullptr);

    /// Stop the control plane if it is running. Safe to call more than once.
    void stopControlPlane();

    bool controlPlaneEnabled() const;

    // ── RuntimeController (used by ControlPlaneServer) ───────────────────
    QVector<NodeInfo> nodes() const override;
    bool nodeConfig(quint64 nodeId, QJsonObject& out) const override;
    bool setNodeConfig(quint64 nodeId, const QJsonObject& config) override;
    bool startNode(quint64 nodeId) override;
    bool stopNode(quint64 nodeId) override;
    QJsonObject flowJson() const override;
    QJsonObject nodeSchema(quint64 nodeId) const override;
    bool reloadFlow(const QJsonObject& flow, QString* errorMessage = nullptr) override;

Q_SIGNALS:
    void nodeStarted(QtNodes::NodeId nodeId);
    void nodeStopped(QtNodes::NodeId nodeId);
    void nodeConfigChanged(QtNodes::NodeId nodeId);
    void errorOccurred(const QString& message);

protected:
    virtual void DeInitialize() override;

private slots:
    void onMainWindowDestroyed(QObject* obj);

private:
    // Initialization sequence
    bool registerNodes();
    bool buildCanvas();
    bool loadFlow(const QString& flowPath);

    /// Apply an already-parsed scene object (shared by loadFlow and reloadFlow).
    /// Extracts the "ui" section, drops unregistered node types, then rebuilds
    /// the graph. Never shows a dialog; the caller decides on user feedback.
    bool applySceneJson(QJsonObject sceneJson,
                        QStringList* skippedTypes,
                        QString* errorMessage);

    void arrangeWorkspaces(const FlowUi::UiSection& ui);
    void autoStartNodes(const FlowUi::UiSection& ui);

    /// The active graph model, or nullptr before buildCanvas(). Controller
    /// methods must stay safe to call at any point in the lifecycle.
    QtNodes::DataFlowGraphModel* graphModel() const;

    /// REQ-SW-PL-051: re-discover and register nodes from newly loaded
    /// INodeProvider plugins (connected to QPluginManager::PluginsListChangeDetected).
    void discoverAndRegisterExternalNodes();

    /// REQ-SW-PL-051: hands ALL IWidgetProvider plugins to the editor widget
    /// (each GUI plugin serves widgets for its own core models) and watches
    /// their plugin objects, re-discovering if any of them dies.
    void adoptWidgetProviders(const QList<Daqster::IWidgetProvider*>& providers);

public:
    void stopAllNodes();
private:

    // Helpers
    QMdiSubWindow* createSubWindow(QWidget* widget, const FlowUi::Geometry& geometry);
    void clearWindowFlags(QWidget* widget);

    QMainWindow* m_mainWindow = nullptr;
    NodeEditorWidget* m_editorWidget = nullptr;
    std::vector<QMdiArea*> m_mdiAreas;  // One per workspace
    QHash<int, QMdiArea*> m_workspaceIdToMdiArea;
    QHash<QtNodes::NodeId, bool> m_autoStartNodes;
    std::vector<FlowUi::WorkspaceUi> m_workspaces;

    /// Full UI section of the loaded scene (geometry/workspace/deembed state),
    /// kept so `flowJson()` can round-trip the layout, mirroring HeadlessEngine.
    FlowUi::UiSection m_uiSection;

    // Node lifecycle as seen by the control plane.
    QHash<QtNodes::NodeId, bool> m_runningNodes;

    // Remote control plane (REQ-SW-PL-054)
    std::unique_ptr<Daqster::ControlPlaneServer> m_controlPlane;
};
