#include "CustomDataFlowScene.h"

#include <QtNodes/DataFlowGraphModel>

void CustomDataFlowScene::onNodeDataArrived(QtNodes::NodeId const nodeId)
{
    auto node = nodeGraphicsObject(nodeId);
    if (!node)
        return;

    auto *dfModel = dynamic_cast<QtNodes::DataFlowGraphModel *>(&graphModel());
    if (!dfModel) {
        onNodeUpdated(nodeId);
        return;
    }
    auto *model = dfModel->delegateModel<QtNodes::NodeDelegateModel>(nodeId);

    if (model && !model->dataArrivalChangesGeometry()) {
        // Repaint-only fast path (video pipeline): the node updates its
        // display directly in setInData() (GL blit, QVideoWidget,
        // QGraphicsVideoItem). Skipping the full geometry recompute +
        // connection move eliminates the scene repaint cascade (QBezier
        // bezier paths, antialiasing) = CPU savings on data arrival.
        //
        // Repaint node body ONLY if the model says its appearance depends on
        // data (dataArrivalChangesWidget == true). Video nodes return false —
        // their display widget calls update() on itself, and
        // QGraphicsProxyWidget forwards it to the scene, repainting only the
        // display area.
        if (model->dataArrivalChangesWidget())
            node->update();
    } else {
        // Full path (NumberDisplay, DaqDisplay, ...): geometry may change on
        // data arrival, so recompute size + move connections as before.
        onNodeUpdated(nodeId);
    }
}
