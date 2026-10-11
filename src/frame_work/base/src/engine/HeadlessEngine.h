#pragma once

#include <QPluginManager.h>
#include <capabilities/INodeProvider.h>
#include <capabilities/IStoppable.h>
#include <capabilities/IStartable.h>
#include <capabilities/IRuntimeHost.h>   // ControlPlaneOptions (REQ-SW-PL-054)
#include "RuntimeController.h"

#include <QtNodes/NodeDelegateModel>
#include <QtNodes/NodeDelegateModelRegistry>
#include <QtNodes/DataFlowGraphModel>

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QHash>
#include <QSet>
#include <memory>
#include <vector>

namespace Daqster {

class ControlPlaneServer;

/**
 * @brief Headless engine for running flows without GUI.
 * 
 * Uses QCoreApplication event loop, loads .flow files, instantiates node models
 * via the plugin registry, connects them, and manages their lifecycle via
 * IStartable/IStoppable interfaces. Integrates with ShutdownHandler for
 * graceful shutdown.
 */
// FRAMEWORK_CORE_EXPORT: RuntimeHostHeadlessPluginObject constructs a HeadlessEngine
// (cross-DLL). A QObject subclass carries a vtable and a staticMetaObject, both
// DATA symbols; WINDOWS_EXPORT_ALL_SYMBOLS emits only the __imp_ form of those
// in the import library, so a consumer without explicit markup fails to link on
// Windows. Linux cannot surface this (undefined symbols resolve at dlopen).
class FRAMEWORK_CORE_EXPORT HeadlessEngine : public QObject, public RuntimeController
{
    Q_OBJECT
public:
    explicit HeadlessEngine(QObject* parent = nullptr);
    ~HeadlessEngine() override;

    /**
     * @brief Load and run a flow file.
     * @param flowPath Path to the .flow file.
     * @return true on success, false on error.
     */
    bool loadFlow(const QString& flowPath);

    /**
     * @brief Stop all running nodes (calls IStoppable::stop()).
     */
    void stopAllNodes();

    /**
     * @brief Get the graph model for inspection.
     */
    QtNodes::DataFlowGraphModel* graphModel() const { return m_graphModel.get(); }

    /**
     * @brief Get the node registry.
     */
    QtNodes::NodeDelegateModelRegistry* registry() const { return m_registry.get(); }

    // ── Remote control plane (REQ-SW-PL-054) ─────────────────────────────
    /**
     * @brief Start the HTTP control plane described by @p options.
     *
     * Returns false (without listening) when the options are unsafe or the
     * bind fails, so the caller can fail loudly.
     */
    bool startControlPlane(const ControlPlaneOptions& options, QString* errorMessage = nullptr);

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

Q_SIGNALS:
    void flowLoaded(bool success);
    void nodeStarted(QtNodes::NodeId nodeId);
    void nodeStopped(QtNodes::NodeId nodeId);
    void nodeConfigChanged(QtNodes::NodeId nodeId);
    void errorOccurred(const QString& message);

private:
    // Plugin manager and registry
    Daqster::QPluginManager* m_pluginManager = nullptr;
    std::shared_ptr<QtNodes::NodeDelegateModelRegistry> m_registry;
    std::unique_ptr<QtNodes::DataFlowGraphModel> m_graphModel;

    // Flow UI section (for autoStart, etc.)
    struct NodeUi {
        bool deembedded = false;
        int workspace = 0;
        struct Geometry { int x=0, y=0, w=0, h=0; bool maximized=false; } geometry;
        bool autoStart = false;
    };
    struct WorkspaceUi {
        int id = 0;
        NodeUi::Geometry geometry;
        bool tabbed = true;
    };
    struct UiSection {
        int version = 1;
        std::vector<WorkspaceUi> workspaces;
        QHash<QtNodes::NodeId, NodeUi> nodes;
    };
    UiSection m_uiSection;

    // Node lifecycle
    QHash<QtNodes::NodeId, bool> m_autoStartNodes;
    QHash<QtNodes::NodeId, bool> m_runningNodes;

    // Remote control plane (REQ-SW-PL-054)
    std::unique_ptr<ControlPlaneServer> m_controlPlane;

    // Helpers
    bool registerNodes();
    bool validateFlow();
    bool buildGraphModel();
    bool parseFlowFile(const QString& flowPath);
    bool parseUiSection(const QJsonObject& sceneJson);
    void autoStartNodes();
    void connectNodes(const QJsonArray& connectionsJson);
    QtNodes::NodeDelegateModel* createNodeModel(const QString& modelName, const QJsonObject& nodeJson);
    QJsonObject serializeUiSection() const;
};

} // namespace Daqster