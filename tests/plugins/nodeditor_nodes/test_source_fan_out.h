#pragma once

#include "NodeDataTypes/SampledData.h"

#include <QtNodes/NodeDelegateModel>
#include <QtTest>

#include <memory>

/// A consumer that only records what it was handed.
///
/// The graph hands the model the very shared_ptr the source returned, so the
/// recorded pointer is the cheapest possible proof of "one block, two readers".
/// It lives in this header (not the .cpp) so AUTOMOC sees the Q_OBJECT.
class RecordingSinkModel : public QtNodes::NodeDelegateModel
{
    Q_OBJECT

public:
    RecordingSinkModel() = default;

    QString name() const override { return QStringLiteral("FanOutRecordingSink"); }

    QString caption() const override { return name(); }

    unsigned int nPorts(QtNodes::PortType portType) const override
    {
        return portType == QtNodes::PortType::In ? 1u : 0u;
    }

    QtNodes::NodeDataType dataType(QtNodes::PortType, QtNodes::PortIndex) const override
    {
        return SampledData().type();
    }

    void setInData(std::shared_ptr<QtNodes::NodeData> data, QtNodes::PortIndex) override
    {
        // addConnection() also pushes the out port's current value, which is
        // null before the first block — only real blocks are counted.
        if (!data)
            return;
        ++m_blocks;
        m_last = std::move(data);
    }

    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex) override { return nullptr; }

    QWidget *embeddedWidget() override { return nullptr; }

    int blockCount() const { return m_blocks; }

    const std::shared_ptr<QtNodes::NodeData> &lastBlock() const { return m_last; }

private:
    int m_blocks = 0;
    std::shared_ptr<QtNodes::NodeData> m_last;
};

/// Connection-policy / broadcast tests for the capture sources.
///
/// `AudioSourceDataModel` (and the three pollers beside it) used to return
/// `ConnectionPolicy::One` for EVERY port, overriding the QtNodes default of
/// `In=One, Out=Many`. `DataFlowGraphModel::connectionPossible()` therefore
/// rejected the second edge on an out port, so one microphone could feed
/// exactly one node — although the model counts its links and keeps capturing
/// while `count > 0`, i.e. it was written for several consumers, and the
/// broadcast is copy-free (the same shared_ptr reaches every input).
///
/// These tests go through the real graph model: `connectionPossible()` is the
/// gate the scene calls before it inserts an edge, so a regression to `One`
/// fails here rather than in the IDE.
class SourceFanOutTest : public QObject
{
    Q_OBJECT

private slots:
    void outPortAcceptsASecondConsumer();
    void bothConsumersReceiveTheSameBlock();
    void removingOneLinkLeavesTheOtherDelivering();
    void inputAndNonePortsStillAllowOneLink();
    void otherCaptureSourcesAllowFanOut();
};
