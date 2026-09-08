#pragma once

#include "FlowLoader.h"
#include <QPluginManager.h>
#include <INodeProvider.h>
#include <IStoppable.h>
#include <IStartable.h>
#include <VideoFrameData.h>
#include <SampledData.h>
#include <TextData.h>
#include <FloatData.h>
#include <EmbeddingData.h>

#include <QtNodes/NodeDelegateModel>
#include <QtNodes/NodeDelegateModelRegistry>
#include <QtNodes/DataFlowGraphModel>
#include <QtNodes/DataFlowGraphicsScene>

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

/**
 * @brief Headless engine for running flows without GUI.
 * 
 * Uses QCoreApplication event loop, loads .flow files, instantiates node models
 * via the plugin registry, connects them, and manages their lifecycle via
 * IStartable/IStoppable interfaces. Integrates with ShutdownHandler for
 * graceful shutdown.
 */
class HeadlessEngine : public QObject
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

Q_SIGNALS:
    void flowLoaded(bool success);
    void nodeStarted(QtNodes::NodeId nodeId);
    void nodeStopped(QtNodes::NodeId nodeId);
    void errorOccurred(const QString& message);

private:
    // Plugin manager and registry
    Daqster::QPluginManager* m_pluginManager = nullptr;
    std::shared_ptr<QtNodes::NodeDelegateModelRegistry> m_registry;
    std::unique_ptr<QtNodes::DataFlowGraphModel> m_graphModel;
    std::unique_ptr<QtNodes::DataFlowGraphicsScene> m_scene;

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

    // Helpers
    bool registerNodes();
    bool buildGraphModel();
    bool parseFlowFile(const QString& flowPath);
    bool parseUiSection(const QJsonObject& sceneJson);
    void autoStartNodes();
    void connectNodes(const QJsonArray& connectionsJson);
    QtNodes::NodeDelegateModel* createNodeModel(const QString& modelName, const QJsonObject& nodeJson);
};

} // namespace Daqster