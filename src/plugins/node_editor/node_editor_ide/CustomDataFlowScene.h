#pragma once

#include <QtNodes/DataFlowGraphicsScene>

/**
 * Custom scene that overrides onNodeDataArrived() to use a repaint-only fast
 * path for nodes whose geometry does not change on data arrival (video
 * pipeline nodes), eliminating the scene repaint cascade triggered by data
 * propagation.
 *
 * When a video node receives data, it updates its display directly in
 * setInData() (GL blit, QVideoWidget, QGraphicsVideoItem). The full
 * onNodeUpdated() path (recomputeSize + updateQWidgetEmbedPos +
 * moveConnections) is unnecessary for video nodes and causes CPU overhead
 * from QBezier bezier path rasterization.
 *
 * Models that DO resize on data arrival (NumberDisplay, DaqDisplay, ...)
 * keep the full path via onNodeUpdated().
 */
class CustomDataFlowScene : public QtNodes::DataFlowGraphicsScene
{
    Q_OBJECT
public:
    using DataFlowGraphicsScene::DataFlowGraphicsScene;

public Q_SLOTS:
    void onNodeDataArrived(QtNodes::NodeId const nodeId) override;
};
