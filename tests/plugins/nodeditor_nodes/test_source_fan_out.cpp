#include "test_source_fan_out.h"

#include "Sources/AudioSource/AudioSourceDataModel.h"
#include "Sources/GpuMonitor/GpuMonitorModel.h"
#include "Sources/JackDetect/JackDetectModel.h"
#include "Sources/Pcap/PcapModel.h"

#include <QtNodes/DataFlowGraphModel>
#include <QtNodes/NodeDelegateModelRegistry>

#include <QMetaMethod>

#include <cstddef>
#include <memory>

using QtNodes::ConnectionId;
using QtNodes::ConnectionPolicy;
using QtNodes::NodeData;
using QtNodes::NodeDelegateModelRegistry;
using QtNodes::NodeId;
using QtNodes::PortIndex;
using QtNodes::PortType;

namespace {

/// One 10 ms block of silence: 480 frames @ 48 kHz, mono int16.
std::shared_ptr<SampledData> makeBlock()
{
    SampledStreamDescriptor descriptor;
    descriptor.sampleRate = 48000.0;
    descriptor.domain = QStringLiteral("audio");
    StreamChannelDescriptor channel;
    channel.name = QStringLiteral("Mono");
    channel.sampleType = SampleType::INT16;
    descriptor.channels.append(channel);

    QByteArray pcm(480 * 2, '\0');
    return std::make_shared<SampledData>(pcm, descriptor);
}

/// Hand the model one block the way its capture worker does, without a
/// microphone: `onSamplesReady` is a private slot, and a slot is reachable
/// through the meta-object. The argument type name is taken from the method
/// itself, so this keeps working if the signature is ever spelled differently —
/// the metatype the constructor registered is `std::shared_ptr<SampledData>`.
bool pushBlock(AudioSourceDataModel &source, std::shared_ptr<SampledData> block)
{
    const int index = source.metaObject()->indexOfSlot(
        "onSamplesReady(std::shared_ptr<SampledData>)");
    if (index < 0)
        return false;

    const QMetaMethod method = source.metaObject()->method(index);
    const QByteArray typeName = method.parameterTypes().value(0);
    return method.invoke(&source, Qt::DirectConnection,
                         QGenericArgument(typeName.constData(), &block));
}

/// A registry + graph for one test, with the model instances kept reachable:
/// `DataFlowGraphModel` owns its models as `unique_ptr` and exposes no accessor.
struct FanOutGraph
{
    FanOutGraph()
        : registry(std::make_shared<NodeDelegateModelRegistry>())
    {
        registry->registerModel(
            [this] {
                auto model = std::make_unique<AudioSourceDataModel>();
                m_sources.append(model.get());
                return model;
            },
            QStringLiteral("Sources"));

        registry->registerModel(
            [this] {
                auto model = std::make_unique<RecordingSinkModel>();
                m_sinks.append(model.get());
                return model;
            },
            QStringLiteral("Test"));

        // registerModel probes the creator once to learn the model name
        // (`creator()->name()`); that probe instance is destroyed immediately,
        // so the recorded pointer to it must go before it dangles.
        m_sources.clear();
        m_sinks.clear();

        graph = std::make_unique<QtNodes::DataFlowGraphModel>(registry);
    }

    NodeId addSource()
    {
        const NodeId id = graph->addNode(QStringLiteral("AudioSource"));
        source = m_sources.isEmpty() ? nullptr : m_sources.last();
        return id;
    }

    NodeId addSink()
    {
        const NodeId id = graph->addNode(QStringLiteral("FanOutRecordingSink"));
        sinks.append(m_sinks.last());
        return id;
    }

    static ConnectionId link(NodeId outNode, PortIndex outPort, NodeId inNode, PortIndex inPort)
    {
        return ConnectionId{outNode, outPort, inNode, inPort};
    }

    std::shared_ptr<NodeDelegateModelRegistry> registry;
    std::unique_ptr<QtNodes::DataFlowGraphModel> graph;
    AudioSourceDataModel *source = nullptr;
    QList<RecordingSinkModel *> sinks;

private:
    QList<AudioSourceDataModel *> m_sources;
    QList<RecordingSinkModel *> m_sinks;
};

/// Every capture source under test: the microphone and the three pollers.
QList<QtNodes::NodeDelegateModel *> captureSources()
{
    return {new AudioSourceDataModel(),
            new PcapModel(),
            new JackDetectModel(),
            new GpuMonitorModel()};
}

} // namespace

void SourceFanOutTest::outPortAcceptsASecondConsumer()
{
    FanOutGraph h;
    const NodeId source = h.addSource();
    const NodeId first = h.addSink();
    const NodeId second = h.addSink();
    QVERIFY(h.source != nullptr);
    QCOMPARE(h.sinks.size(), 2);

    const ConnectionId toFirst = FanOutGraph::link(source, 0, first, 0);
    const ConnectionId toSecond = FanOutGraph::link(source, 0, second, 0);

    QVERIFY(h.graph->connectionPossible(toFirst));
    h.graph->addConnection(toFirst);

    // The defect: `ConnectionPolicy::One` on the out port made this false, so
    // the scene silently refused the second edge.
    QVERIFY(h.graph->connectionPossible(toSecond));
    h.graph->addConnection(toSecond);

    QCOMPARE(int(h.graph->connections(source, PortType::Out, 0).size()), 2);
    QVERIFY(h.graph->connectionExists(toFirst));
    QVERIFY(h.graph->connectionExists(toSecond));
}

void SourceFanOutTest::bothConsumersReceiveTheSameBlock()
{
    FanOutGraph h;
    const NodeId source = h.addSource();
    const NodeId first = h.addSink();
    const NodeId second = h.addSink();
    QVERIFY(h.source != nullptr);

    h.graph->addConnection(FanOutGraph::link(source, 0, first, 0));
    h.graph->addConnection(FanOutGraph::link(source, 0, second, 0));

    QVERIFY(pushBlock(*h.source, makeBlock()));

    QCOMPARE(h.sinks.at(0)->blockCount(), 1);
    QCOMPARE(h.sinks.at(1)->blockCount(), 1);

    // Broadcast, not copy: the graph forwards the very shared_ptr `outData()`
    // returned, so both readers see one block.
    QVERIFY(h.sinks.at(0)->lastBlock() != nullptr);
    QCOMPARE(h.sinks.at(1)->lastBlock().get(), h.sinks.at(0)->lastBlock().get());
    QCOMPARE(h.sinks.at(0)->lastBlock().get(), h.source->outData(0).get());
}

void SourceFanOutTest::removingOneLinkLeavesTheOtherDelivering()
{
    FanOutGraph h;
    const NodeId source = h.addSource();
    const NodeId first = h.addSink();
    const NodeId second = h.addSink();
    QVERIFY(h.source != nullptr);

    const ConnectionId toFirst = FanOutGraph::link(source, 0, first, 0);
    const ConnectionId toSecond = FanOutGraph::link(source, 0, second, 0);
    h.graph->addConnection(toFirst);
    h.graph->addConnection(toSecond);

    h.graph->deleteConnection(toFirst);
    QVERIFY(h.graph->connectionExists(toSecond));
    QCOMPARE(int(h.graph->connections(source, PortType::Out, 0).size()), 1);

    // The surviving link keeps receiving after the partial disconnect.
    QVERIFY(pushBlock(*h.source, makeBlock()));
    QCOMPARE(h.sinks.at(0)->blockCount(), 0);
    QCOMPARE(h.sinks.at(1)->blockCount(), 1);

    // …and the freed out port still accepts a new consumer. It immediately
    // receives the source's latest block (addConnection propagates the current
    // out data), so the next push is its second.
    const NodeId third = h.addSink();
    const ConnectionId toThird = FanOutGraph::link(source, 0, third, 0);
    QVERIFY(h.graph->connectionPossible(toThird));
    h.graph->addConnection(toThird);
    QCOMPARE(h.sinks.at(2)->blockCount(), 1);
    QVERIFY(pushBlock(*h.source, makeBlock()));
    QCOMPARE(h.sinks.at(2)->blockCount(), 2);
}

void SourceFanOutTest::inputAndNonePortsStillAllowOneLink()
{
    AudioSourceDataModel source;

    // Only the OUT side is a broadcast; the base policy for everything else
    // stays `One` (QtNodes' own default).
    QVERIFY(source.portConnectionPolicy(PortType::In, 0) == ConnectionPolicy::One);
    QVERIFY(source.portConnectionPolicy(PortType::None, 0) == ConnectionPolicy::One);
    QVERIFY(source.portConnectionPolicy(PortType::Out, 0) == ConnectionPolicy::Many);
}

void SourceFanOutTest::otherCaptureSourcesAllowFanOut()
{
    // Pcap/JackDetect/GpuMonitor build a NEW SampledData on every poll and let
    // the graph forward that one shared_ptr to every input; no consumer mutates
    // a block (they decode/copy it), so `Out = Many` is safe for them too.
    const QList<QtNodes::NodeDelegateModel *> models = captureSources();
    for (QtNodes::NodeDelegateModel *model : models) {
        QVERIFY(model->portConnectionPolicy(PortType::Out, 0) == ConnectionPolicy::Many);
        QVERIFY(model->portConnectionPolicy(PortType::In, 0) == ConnectionPolicy::One);
        delete model;
    }
}
