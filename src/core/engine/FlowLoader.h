#pragma once

#include "HeadlessEngine.h"
#include <QPluginManager.h>
#include <INodeProvider.h>

#include <QtNodes/NodeDelegateModelRegistry>
#include <QtNodes/DataFlowGraphModel>

#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <memory>

namespace Daqster {

/**
 * @brief Tolerant flow loader for headless and GUI modes.
 * 
 * Parses .flow JSON files, instantiates node models via the registry,
 * and connects them. Skips unregistered node types with warnings instead
 * of failing. Used by both HeadlessEngine and RuntimeShell.
 */
class FlowLoader
{
public:
    explicit FlowLoader(QtNodes::NodeDelegateModelRegistry* registry);
    ~FlowLoader() = default;

    /**
     * @brief UI section data structures (mirrors FlowUiSection from node_editor_ide).
     */
    struct Geometry {
        int x = 0;
        int y = 0;
        int w = 0;
        int h = 0;
        bool maximized = false;

        QJsonObject toJson() const;
        static Geometry fromJson(const QJsonObject& json);
    };

    struct NodeUi {
        bool deembedded = false;
        int workspace = 0;
        Geometry geometry;
        bool autoStart = false;

        QJsonObject toJson() const;
        static NodeUi fromJson(const QJsonObject& json);
    };

    struct WorkspaceUi {
        int id = 0;
        Geometry geometry;
        bool tabbed = true;

        QJsonObject toJson() const;
        static WorkspaceUi fromJson(const QJsonObject& json);
    };

    struct UiSection {
        int version = 1;
        std::vector<WorkspaceUi> workspaces;
        QHash<QtNodes::NodeId, NodeUi> nodes;

        QJsonObject toJson() const;
        static UiSection fromJson(const QJsonObject& json);
    };

    /**
     * @brief Load a flow from a JSON object into the graph model.
     * @param sceneJson The parsed .flow JSON object.
     * @param graphModel The graph model to populate.
     * @param skippedTypes Output: list of skipped (unregistered) node type names.
     * @return true on success (even with skipped nodes), false on parse/load error.
     */
    bool loadFlow(const QJsonObject& sceneJson,
                  QtNodes::DataFlowGraphModel* graphModel,
                  QStringList* skippedTypes = nullptr);

    /**
     * @brief Load a flow from a file.
     * @param fileName Path to the .flow file.
     * @param graphModel The graph model to populate.
     * @param skippedTypes Output: list of skipped (unregistered) node type names.
     * @param uiSection Output: parsed UI section (optional).
     * @return true on success, false on error.
     */
    bool loadFlowFromFile(const QString& fileName,
                          QtNodes::DataFlowGraphModel* graphModel,
                          QStringList* skippedTypes = nullptr,
                          UiSection* uiSection = nullptr);

private:
    QtNodes::NodeDelegateModelRegistry* m_registry = nullptr;

    bool parseNodes(const QJsonArray& nodesJsonArray,
                    QJsonArray& cleanedNodes,
                    QSet<QtNodes::NodeId>& skippedNodeIds,
                    QStringList* skippedTypes);

    bool parseConnections(const QJsonArray& connJsonArray,
                          const QSet<QtNodes::NodeId>& skippedNodeIds,
                          QJsonArray& cleanedConnections);

    bool parseUiSection(const QJsonObject& sceneJson, UiSection* uiSection);
};

} // namespace Daqster