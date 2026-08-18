#pragma once

#include <QtNodes/DataFlowGraphicsScene>

/**
 * Custom scene that overrides onNodeUpdated() to suppress
 * QGraphicsItem::update() for video nodes, eliminating the
 * scene repaint cascade triggered by data propagation.
 *
 * When a video node receives data, it updates its display
 * directly in setInData() (GL blit, QVideoWidget, QGraphicsVideoItem).
 * The node->update() call in onNodeUpdated() only triggers
 * scene repaint of NodeGraphicsObject (frame, caption, ports)
 * which is unnecessary for video nodes and causes 49% CPU
 * overhead from QBezier bezier path rasterization.
 */
class CustomDataFlowScene : public QtNodes::DataFlowGraphicsScene
{
    Q_OBJECT
public:
    using DataFlowGraphicsScene::DataFlowGraphicsScene;

protected slots:
    void onNodeUpdated(QtNodes::NodeId const nodeId) override;
};
