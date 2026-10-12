#ifndef NODEDATAMODELTOQIODEVICECONNECTOROBSOLETE_H
#define NODEDATAMODELTOQIODEVICECONNECTOROBSOLETE_H

#include <memory>
#include <QtNodes/NodeData>
#include <QtNodes/NodeDelegateModel>

class QIODevice;

/**
 * @brief Base connector bridging a source node model and a QIODevice.
 *
 * Renamed to *_obsolete (REQ-SW-PL-023 §7) — implementation unchanged.
 * Base of GenericQDevIoConnectorObsolete and AudioNodeQdevIoConnector.
 * Kept working until the QDevIO display world is deleted at the very end.
 */
class NodeDataModelToQIODeviceConnectorObsolete : public QtNodes::NodeData
{
public:
    NodeDataModelToQIODeviceConnectorObsolete( QtNodes::NodeDelegateModel* model);
    virtual ~NodeDataModelToQIODeviceConnectorObsolete() = default;
    virtual void ConnectModels(QtNodes::NodeDelegateModel* dst_model) = 0;
    virtual QtNodes::NodeDataType type() const = 0;

protected:
     QtNodes::NodeDelegateModel* m_src_model;
};

#endif // NODEDATAMODELTOQIODEVICECONNECTOROBSOLETE_H
