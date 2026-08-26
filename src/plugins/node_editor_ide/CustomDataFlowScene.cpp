#include "CustomDataFlowScene.h"

#include <QtNodes/DataFlowGraphModel>

void CustomDataFlowScene::onNodeDataArrived(QtNodes::NodeId const nodeId)
{
    auto node = nodeGraphicsObject(nodeId);
    if (!node)
        return;

    auto *model = static_cast<QtNodes::DataFlowGraphModel &>(graphModel())
                      .delegateModel<QtNodes::NodeDelegateModel>(nodeId);

    if (model && !model->dataArrivalChangesGeometry()) {
        // Repaint-only fast path (video pipeline): the node updates its
        // display directly in setInData() (GL blit, QVideoWidget,
        // QGraphicsVideoItem). Skipping the full geometry recompute +
        // connection move eliminates the scene repaint cascade (QBezier
        // bezier paths, antialiasing) = CPU savings on data arrival.
        node->update();
    } else {
        // Full path (NumberDisplay, DaqDisplay, ...): geometry may change on
        // data arrival, so recompute size + move connections as before.
        onNodeUpdated(nodeId);
    }
}
