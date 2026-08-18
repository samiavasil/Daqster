#include "CustomDataFlowScene.h"

void CustomDataFlowScene::onNodeUpdated(QtNodes::NodeId const nodeId)
{
    auto node = nodeGraphicsObject(nodeId);
    if (node) {
        node->setGeometryChanged();
        nodeGeometry().recomputeSize(nodeId);
        node->updateQWidgetEmbedPos();
        // node->update() intentionally omitted:
        // Video display is updated directly in VideoOutputNode::setInData()
        // via presentFrame() / setVideoFrame() / setPixmap().
        // Skipping update() eliminates scene repaint cascade
        // (QBezier bezier paths, antialiasing) = ~49% CPU savings.
        node->moveConnections();
    }
}
