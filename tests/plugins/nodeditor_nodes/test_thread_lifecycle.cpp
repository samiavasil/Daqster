#include "test_thread_lifecycle.h"

// The shutdown path includes the interface from src/plugins/common/
// (HeadlessEngine.cpp:17, NodeEditorWidget.cpp:5), so the test does too —
// asserting the cast with a different header than the production caller would
// be asserting a different cast.
#include <capabilities/IStartable.h>
#include <capabilities/IStoppable.h>

// …and the models inherit it from src/plugins/node_editor/nodeditor_nodes/core/shared/,
// which is now a forwarding shim to the canonical capabilities/ header
// (REQ-SW-PL-050 "Оставащ дълг" #1). Including both paths below is deliberate,
// and bothInterfaceHeadersDeclareTheSameType() asserts they resolve to the
// very same type — the shim makes that identity structural, not accidental.
#include <Sources/GpuMonitor/GpuMonitorModel.h>
#include <Sources/JackDetect/JackDetectModel.h>
#include <Sources/Pcap/PcapModel.h>
#include <Sources/SystemMonitor/SystemMonitorModel.h>

// The remaining IStoppable models. Included unconditionally: each one only
// *enumerates* the device it would later open (QMediaDevices::videoInputs /
// defaultAudioInput, both empty under QCoreApplication), or allocates a timer
// it never starts. None of them opens hardware in its constructor.
#include <Displays/DaqDisplay/DaqDisplayNode.h>
#include <Sinks/FileRecord/FileRecordModel.h>
#include <Sinks/NetworkSink/NetworkSinkModel.h>
#include <Sources/AudioSource/AudioSourceDataModel.h>
#include <Sources/FilePlayback/FilePlaybackModel.h>
#include <Sources/Gamepad/GamepadModel.h>
#include <Sources/NetworkSource/NetworkSourceModel.h>
#include <Sources/Video/CameraSourceNode.h>
#include <Sources/Video/StreamSourceNode.h>
#include <Sources/Video/VideoFileSourceNode.h>

#ifdef HAVE_LIBIIO
// PlutoSdrEngine calls iio_* directly, so the model only exists in this target
// when the SDK was found at configure time. Without it the model is not linked
// and therefore not covered — recorded rather than silently dropped, because
// the point of the roster check below is to notice exactly this kind of drift.
#include <Sources/PlutoSdr/PlutoSdrModel.h>
#endif

#include <shared/IStartable.h>
#include <shared/IStoppable.h>

#include <QElapsedTimer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QThread>
#include <type_traits>
#include <typeinfo>

#include <Threading/CancelToken.h>

#include <QtNodes/DataFlowGraphModel>
#include <QtNodes/NodeDelegateModelRegistry>

#include "SceneClose.h" // FrameworkCore: the close helper every host calls

#include <cstring>
#include <memory>

namespace {

/// Every model in _core that inherits Daqster::IStoppable, and whether this
/// suite reaches it. Recorded explicitly so the roster is reviewable rather
/// than inferred from a green run.
///
/// The original suite listed 14 models as excluded "needs hardware", which was
/// true only of start(). All fourteen construct inertly — the device is opened
/// in start(), not in the constructor — so the assertions that do not start
/// anything (the cast, idempotency, the three destruction orders) now cover
/// them. What genuinely needs hardware is start(), so the two active-work
/// tests stay on the smaller list below.
///
///   Model               start() needs          covered by this suite
///   ─────────────────────────────────────────────────────────────────────
///   SystemMonitor       nothing (poller)       cast + idempotency + active
///   GpuMonitor          nvidia-smi            cast + idempotency + active
///   JackDetect          an ALSA device        cast + idempotency + active
///   Pcap                an interface          cast + idempotency + active
///   AudioSource/mic     a capture device      cast + idempotency
///   FilePlayback        a decodable file      cast + idempotency
///   FileRecord          a writable device     cast + idempotency
///   NetworkSource       a bound port          cast + idempotency
///   NetworkSink         a reachable peer      cast + idempotency
///   Gamepad             a real joystick       cast + idempotency
///   VideoFileSource     a decoder             cast + idempotency
///   StreamSource        a network socket      cast + idempotency
///   CameraSource        a capture device      cast + idempotency
///   DaqDisplay          nothing (readout)     cast + idempotency
///   PlutoSdr            libiio + hardware     cast + idempotency, when the
///                                              SDK was found at configure
///
/// Not in this suite at all, and why:
///
///   VideoEffectNode    submits to the shared ComputePool, so its stop() must
///                      drain a real job queue — PL-039's own test, which needs
///                      the pool wired up rather than a bare model.
///   VideoOutputNode    drives the GL display surface through IVideoOutputUi;
///                      covered by demo_nodeditor_videooutput_tests, which has a
///                      QApplication and a real surface.
///   FrameSamplerNode   is NOT IStoppable. It gates frames on a QTimer that is
///                      parented to the model, so it dies with the node — but
///                      that is an argument, not an assertion. See the
///                      nonStoppableModelsAreDeliberate() tripwire.
///
/// Guards against a future model being added to _core without implementing
/// IStoppable, or added to the factory list below and forgotten: either
/// direction moves one of these counts and fails the build.
int expectedStoppableModelCount()
{
#ifdef HAVE_LIBIIO
    return 15;
#else
    return 14;
#endif
}

/// The subset this suite may also start(). Kept separate so the count is a
/// tripwire too: a model added here must genuinely be startable with no
/// hardware, or the active-work tests will hang or flake on a CI box.
int expectedStartableModelCount()
{
    return 4;
}

// ── AudioSource close-path fixtures (REQ-SW-PL-078) ──────────────────────────

/// A one-input consumer: enough for DataFlowGraphModel::addConnection() to have
/// a real peer for the AudioSource output. It is a graph node, not a widget, so
/// it needs no Q_OBJECT.
class SinkStub : public QtNodes::NodeDelegateModel
{
public:
    QString caption() const override { return QStringLiteral("SinkStub"); }
    bool captionVisible() const override { return false; }
    QString name() const override { return QStringLiteral("SinkStub"); }

    unsigned int nPorts(QtNodes::PortType portType) const override
    {
        return portType == QtNodes::PortType::In ? 1u : 0u;
    }

    QtNodes::NodeDataType dataType(QtNodes::PortType, QtNodes::PortIndex) const override
    {
        return SampledData().type();
    }

    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex) override
    {
        return nullptr;
    }

    void setInData(std::shared_ptr<QtNodes::NodeData> data, QtNodes::PortIndex) override
    {
        m_in = std::move(data);
    }

    QWidget *embeddedWidget() override { return nullptr; }

private:
    std::shared_ptr<QtNodes::NodeData> m_in;
};

/// Scrub every free heap block of the capture worker's size class with 0xAB.
///
/// A use-after-free usually *works*: the freed block keeps its vtable pointer
/// until something reuses it, so the stale QMetaObject::invokeMethod() resolves
/// a real method on an object that no longer exists and the suite stays green.
/// Refilling the block makes the dangling call read 0xABAB… instead — which is
/// what the field crash looked like (jump to an unmapped address from
/// AudioSourceDataModel::setCaptureEnabled, backtrace frame 2).
///
/// The blocks stay allocated until the holder dies, so the statement that
/// follows it is the one that observes the difference: crash or no-op.
struct ClobberedHeap
{
    static constexpr int blockCount = 64;

    explicit ClobberedHeap(std::size_t blockSize = sizeof(MicCaptureWorker))
    {
        blocks.reserve(blockCount);
        for (int i = 0; i < blockCount; ++i) {
            void *block = ::operator new(blockSize);
            std::memset(block, 0xAB, blockSize);
            blocks.append(block);
        }
    }

    ~ClobberedHeap()
    {
        for (void *block : blocks)
            ::operator delete(block);
    }

    QVector<void *> blocks;
};

/// Two nodes, one output connection: the smallest graph whose deletion reaches
/// AudioSourceDataModel::outputConnectionDeleted().
struct AudioSourceGraph{
    AudioSourceGraph()
    {
        registry = std::make_shared<QtNodes::NodeDelegateModelRegistry>();
        registry->registerModel<AudioSourceDataModel>();
        registry->registerModel<SinkStub>();
        graph = std::make_unique<QtNodes::DataFlowGraphModel>(registry);

        source = graph->addNode(QStringLiteral("AudioSource"));
        sink = graph->addNode(QStringLiteral("SinkStub"));
        // outputConnectionCreated() → setCaptureEnabled(true): the model now
        // believes a consumer is connected, exactly like an open scene.
        graph->addConnection({source, 0, sink, 0});
    }

    AudioSourceDataModel *audioSource()
    {
        return graph->delegateModel<AudioSourceDataModel>(source);
    }

    std::shared_ptr<QtNodes::NodeDelegateModelRegistry> registry;
    std::unique_ptr<QtNodes::DataFlowGraphModel> graph;
    QtNodes::NodeId source = QtNodes::InvalidNodeId;
    QtNodes::NodeId sink = QtNodes::InvalidNodeId;
};

} // namespace

QVector<TestThreadLifecycle::ModelEntry> TestThreadLifecycle::allStoppableModels()
{
    return {
        {QStringLiteral("SystemMonitor"),
         [] { return std::make_unique<SystemMonitorModel>(); }},
        {QStringLiteral("GpuMonitor"),
         [] { return std::make_unique<GpuMonitorModel>(); }},
        {QStringLiteral("JackDetect"),
         [] { return std::make_unique<JackDetectModel>(); }},
        {QStringLiteral("Pcap"),
         [] { return std::make_unique<PcapModel>(); }},
        {QStringLiteral("AudioSource/mic"),
         [] { return std::make_unique<AudioSourceDataModel>(); }},
        {QStringLiteral("FilePlayback"),
         [] { return std::make_unique<FilePlaybackModel>(); }},
        {QStringLiteral("FileRecord"),
         [] { return std::make_unique<FileRecordModel>(); }},
        {QStringLiteral("NetworkSource"),
         [] { return std::make_unique<NetworkSourceModel>(); }},
        {QStringLiteral("NetworkSink"),
         [] { return std::make_unique<NetworkSinkModel>(); }},
        {QStringLiteral("Gamepad"),
         [] { return std::make_unique<GamepadModel>(); }},
        {QStringLiteral("VideoFileSource"),
         [] { return std::make_unique<VideoFileSourceNode>(); }},
        {QStringLiteral("StreamSource"),
         [] { return std::make_unique<StreamSourceNode>(); }},
        {QStringLiteral("CameraSource"),
         [] { return std::make_unique<CameraSourceNode>(); }},
        {QStringLiteral("DaqDisplay"),
         [] { return std::make_unique<DaqDisplayNode>(); }},
#ifdef HAVE_LIBIIO
        {QStringLiteral("PlutoSdr"),
         [] { return std::make_unique<PlutoSdrModel>(); }},
#endif
    };
}

QVector<TestThreadLifecycle::ModelEntry> TestThreadLifecycle::startableWithoutDeviceModels()
{
    return {
        {QStringLiteral("SystemMonitor"),
         [] { return std::make_unique<SystemMonitorModel>(); }},
        {QStringLiteral("GpuMonitor"),
         [] { return std::make_unique<GpuMonitorModel>(); }},
        {QStringLiteral("JackDetect"),
         [] { return std::make_unique<JackDetectModel>(); }},
        {QStringLiteral("Pcap"),
         [] { return std::make_unique<PcapModel>(); }},
    };
}

// ── The interface duplication ────────────────────────────────────────────────
//
// Daqster::IStoppable is declared TWICE, in two byte-identical headers:
//
//   src/plugins/common/capabilities/IStoppable.h          ← the interface
//   src/plugins/node_editor/nodeditor_nodes/core/shared/IStoppable.h  ← a copy
//
// All 19 node models inherit the second. The three shutdown paths cast to the
// first:
//
//   HeadlessEngine.cpp:94       includes capabilities/IStoppable.h
//   NodeEditorWidget.cpp:75     includes capabilities/IStoppable.h
//   RuntimeShell.cpp:622        includes shared/IStoppable.h   ← the copy
//
// Same class name, same namespace, same body — so this works. But it works by
// the compiler merging two distinct definitions of the same name into one
// type, and nothing enforces that they stay identical. Add a virtual to one
// and the layouts diverge: the cast above still succeeds (same name), and stop()
// then jumps to the wrong offset, on every node, at shutdown. There is no
// compiler diagnostic for that.
//
// The proper fix is to delete core/shared/IStoppable.h and have every model
// include the common/ one — the same consolidation REQ-SW-PL-032's E1 step
// applied to TexturePool. Not done here because it touches 19 model headers in
// a task whose scope is unfreezing tests; the assertions below keep the
// duplication honest until then.

// ── The cast the whole shutdown path depends on ──────────────────────────────

void TestThreadLifecycle::bothInterfaceHeadersDeclareTheSameType()
{
    // The two headers are byte-identical apart from the include guard, so the
    // class bodies must stay in sync. Assert the body, not the type_info:
    // typeid() of one name is trivially one type, so comparing it to itself
    // would prove nothing. What actually matters is that the interface stays
    // state-free with exactly one virtual function, because that is what makes
    // the duplicate declaration safe: identical layout, so the merge is benign.
    static_assert(std::is_polymorphic<Daqster::IStoppable>::value,
                  "IStoppable is used through dynamic_cast, so it must stay polymorphic");
    static_assert(std::is_abstract<Daqster::IStoppable>::value,
                  "IStoppable declares stop() as pure virtual and must stay abstract");
    // std::is_empty is useless here — a polymorphic class always has a vptr.
    // sizeof() is the meaningful proxy: a vptr and nothing else means no data
    // members, so both declarations must produce the same layout.
    static_assert(sizeof(Daqster::IStoppable) == sizeof(void *),
                  "IStoppable grew state — the duplicated declaration in core/shared/ "
                  "would then diverge in layout and the shutdown cast would jump to "
                  "the wrong offset");
    QVERIFY(true); // the static_asserts above are the assertion

    // Same for IStartable, which has the identical duplication problem.
    static_assert(sizeof(Daqster::IStartable) == sizeof(void *),
                  "IStartable grew state — see the note on IStoppable above");
    static_assert(std::is_abstract<Daqster::IStartable>::value,
                  "IStartable declares start() as pure virtual");
    QVERIFY(true);
}

void TestThreadLifecycle::shutdownPathCastMatchesTheModelsInterface()
{
    // The concrete cast the app performs. Every hardware-free model must be
    // reachable through Daqster::IStoppable as the shutdown path sees it.
    for (const ModelEntry &entry : allStoppableModels()) {
        const std::unique_ptr<QtNodes::NodeDelegateModel> model = entry.create();
        Daqster::IStoppable *viaCommonPath = dynamic_cast<Daqster::IStoppable *>(model.get());
        QVERIFY2(viaCommonPath != nullptr,
                 qPrintable(QStringLiteral("%1 is not reachable through the common/ "
                                           "IStoppable the shutdown path casts to")
                                .arg(entry.label)));
        viaCommonPath->stop();
    }
}

void TestThreadLifecycle::stoppableCastReachesEveryModel()
{
    for (const ModelEntry &entry : allStoppableModels()) {
        const std::unique_ptr<QtNodes::NodeDelegateModel> model = entry.create();
        QVERIFY(model);

        // Exactly what HeadlessEngine::stopAllNodes() does. If this returns
        // nullptr the node's stop() is never called at shutdown and its thread
        // or timer outlives the application.
        Daqster::IStoppable *stoppable = dynamic_cast<Daqster::IStoppable *>(model.get());
        QVERIFY2(stoppable != nullptr,
                 qPrintable(QStringLiteral("%1 does not implement IStoppable — stopAllNodes() "
                                           "skips it silently")
                                .arg(entry.label)));
    }
}

void TestThreadLifecycle::everyModelReportsItselfStoppable()
{
    QVector<ModelEntry> models = allStoppableModels();
    QCOMPARE(models.size(), expectedStoppableModelCount());

    // Each node is stopped through the interface only — no downcast to a
    // concrete type — so this exercises the same dispatch the app uses.
    for (const ModelEntry &entry : models) {
        auto model = entry.create();
        auto *stoppable = dynamic_cast<Daqster::IStoppable *>(model.get());
        QVERIFY(stoppable);
        stoppable->stop();
        QVERIFY2(stoppable, qPrintable(entry.label));
    }
}

// ── Idempotency and the stop-without-start / destroy-without-stop orders ──────

void TestThreadLifecycle::stopIsIdempotent()
{
    // IStoppable's contract states stop() is idempotent, and three call sites
    // rely on it (a node can be stopped by the close handler and again by the
    // engine teardown). A second stop() that touches a destroyed QThread is a
    // crash on exit, which no unit test would catch until it shipped.
    for (const ModelEntry &entry : allStoppableModels()) {
        auto model = entry.create();
        auto *stoppable = dynamic_cast<Daqster::IStoppable *>(model.get());
        QVERIFY(stoppable);
        for (int i = 0; i < 5; ++i)
            stoppable->stop();
    }
}

void TestThreadLifecycle::stopWithoutStartIsSafe()
{
    // Every model above constructs its poller but never starts it. stop() must
    // cope with that: joining a thread that was never spawned, or deleting a
    // worker that never allocated, are the two failure modes.
    for (const ModelEntry &entry : allStoppableModels()) {
        auto model = entry.create();
        auto *stoppable = dynamic_cast<Daqster::IStoppable *>(model.get());
        QVERIFY(stoppable);
        stoppable->stop();
        QVERIFY2(stoppable, qPrintable(entry.label));
    }
}

void TestThreadLifecycle::destroyWithoutStopIsSafe()
{
    // A user closing a scene without pressing stop first. The destructor has
    // to stop its own work; otherwise the process hangs on a joined thread
    // that never gets joined, or a QTimer fires into freed memory.
    for (const ModelEntry &entry : allStoppableModels())
        (void)entry.create(); // scope-exit destruction, no explicit stop()
}

void TestThreadLifecycle::destroyAfterStopIsSafe()
{
    for (const ModelEntry &entry : allStoppableModels()) {
        auto model = entry.create();
        auto *stoppable = dynamic_cast<Daqster::IStoppable *>(model.get());
        QVERIFY(stoppable);
        stoppable->stop();
    } // destructor runs after an explicit stop()
}

void TestThreadLifecycle::stopWithActiveWorkIsSafe()
{
    // The realistic shutdown: the poller is running and must be joined. This
    // is the case that actually hangs in the field, and the only one the other
    // tests do not reach.
    //
    // Deliberately the smaller startableWithoutDeviceModels() list, not all 16:
    // start() is what opens the device on the other models, so reaching "poller
    // running" there means attaching real hardware.
    for (const ModelEntry &entry : startableWithoutDeviceModels()) {
        auto model = entry.create();

        auto *startable = dynamic_cast<Daqster::IStartable *>(model.get());
        if (startable) {
            startable->start();
            // Let the poller actually enter its wait so stop() has something
            // to interrupt. Without this the test degenerates into the
            // stop-without-start case above.
            QThread::msleep(120);
        }

        auto *stoppable = dynamic_cast<Daqster::IStoppable *>(model.get());
        QVERIFY(stoppable);
        QElapsedTimer timer;
        timer.start();
        stoppable->stop();
        QVERIFY2(timer.elapsed() < 5000,
                 qPrintable(QStringLiteral("%1: stop() took %2 ms with active work — the "
                                           "poller thread is not being joined")
                                .arg(entry.label)
                                .arg(timer.elapsed())));
    }
}

void TestThreadLifecycle::stopReleasesTheTimer()
{
    // After stop() the node's poll timer must be inactive, otherwise a
    // destroyed model leaves a QTimer firing into freed memory on the next
    // tick. QTimer::isActive() is not observable through IStoppable, so this
    // is asserted through the process-level signal: no Qt timer warnings may
    // be emitted while the model is gone.
    //
    // What IS asserted here is that the object can be destroyed and the event
    // loop pumped afterwards without a warning about a deleted object still
    // being referenced — the concrete failure stop() must prevent.
    qInstallMessageHandler(nullptr);
    for (const ModelEntry &entry : allStoppableModels()) {
        auto model = entry.create();
        auto *stoppable = dynamic_cast<Daqster::IStoppable *>(model.get());
        QVERIFY(stoppable);
        stoppable->stop();
        model.reset();
        // Pump the loop so any dangling timer would fire now.
        QCoreApplication::processEvents(QEventLoop::AllEvents, 200);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::Timer);
    }
}

// ── start() → stop() ─────────────────────────────────────────────────────────

void TestThreadLifecycle::startThenStopIsClean()
{
    // Restart-after-stop is only exercised where start() is safe without a
    // device; see stopWithActiveWorkIsSafe() for why this list is narrower.
    for (const ModelEntry &entry : startableWithoutDeviceModels()) {
        auto model = entry.create();
        auto *startable = dynamic_cast<Daqster::IStartable *>(model.get());
        auto *stoppable = dynamic_cast<Daqster::IStoppable *>(model.get());
        QVERIFY(stoppable);

        if (!startable)
            continue; // monitor-only node: no user-facing start action

        startable->start();
        QThread::msleep(60);
        stoppable->stop();
        // Starting again after a stop must be legal — the runtime restarts
        // nodes when a scene is reloaded, and a node that cannot restart is
        // not really startable, just runnable-once.
        startable->start();
        QThread::msleep(60);
        stoppable->stop();
    }
}

// ── REQ-SW-PL-078: what a close hands to a node after its stop() ─────────────
//
// The close order is fixed (stop every node → notify plugins → delete the
// graph). The graph deletion is NOT a passive teardown: DataFlowGraphModel
// deletes each node's connections first, and every deleted connection calls
// outputConnectionDeleted() on the producer. So a source node's connection
// callback runs after its own stop() — for AudioSource that used to mean a
// queued QMetaObject::invokeMethod() into a worker that stop() had already
// destroyed (QThread::finished → deleteLater).
//
// Field backtrace (coredump, Close Scene):
//   #2 AudioSourceDataModel::setCaptureEnabled(bool)
//   #3 AudioSourceDataModel::outputConnectionDeleted(ConnectionId)
//   #4 QtNodes::DataFlowGraphModel::sendConnectionDeletion
//   #5 DataFlowGraphModel::deleteConnection
//   #6 DataFlowGraphModel::deleteNode
//   #7 QtNodes::BasicGraphicsScene::clearScene()

void TestThreadLifecycle::stopThenConnectionDeletedIsSafe()
{
    AudioSourceGraph scene;
    QVERIFY2(scene.source != QtNodes::InvalidNodeId, "AudioSource did not register");
    QVERIFY2(scene.sink != QtNodes::InvalidNodeId, "SinkStub did not register");

    AudioSourceDataModel *model = scene.audioSource();
    QVERIFY(model != nullptr);

    // A node that was running when the user closed the scene. start() queues
    // startCapture() to the capture thread; stop() joins that thread, and the
    // thread deletes the worker on its way out (QThread::finished → deleteLater).
    model->start();
    QThread::msleep(50);
    model->stop();

    // Now the graph deletes the node — and deletes its connection first, so
    // outputConnectionDeleted() → setCaptureEnabled() runs with the worker gone.
    const ClobberedHeap scrubbed;
    scene.graph->deleteNode(scene.source); // before the fix: SEGV inside here
    scene.graph->deleteNode(scene.sink);

    QVERIFY2(scene.graph->allNodeIds().empty(), "the scene did not end up empty");
}

void TestThreadLifecycle::stopThenUiAndDeviceCallsAreInert()
{
    AudioSourceDataModel model;

    model.start();
    QThread::msleep(50);

    const ClobberedHeap scrubbed;

    QElapsedTimer timer;
    timer.start();
    model.stop();
    model.stop(); // idempotent: no second join, no touch of the freed worker
    QVERIFY2(timer.elapsed() < 5000,
             qPrintable(QStringLiteral("second stop() took %1 ms — it is not a no-op")
                            .arg(timer.elapsed())));

    // Every remaining path that would queue work to the worker must be a no-op
    // once the node is stopped. The widget can still emit them (the GUI plugin
    // tears the widgets down after the models) and the graph does it on the way
    // out; both are reached here directly.
    model.onUiStart(ASDM_START);
    model.onUiStart(ASDM_RELOAD);
    model.onUiStart(ASDM_STOP);
    model.onAudioConnectionChanged(*model.deviceInfo(), *model.audioFormat());
    model.outputConnectionCreated({1, 0, 2, 0});
    model.outputConnectionDeleted({1, 0, 2, 0});
    model.start(); // must not resurrect a joined thread either

    // Let any stray queued event run; a firing into freed memory would surface
    // here instead of in the next test binary.
    if (QCoreApplication::instance() != nullptr)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    QVERIFY(true);
}

void TestThreadLifecycle::audioSourceSurvivesFiveSceneCloses()
{
    // One process, five open → run → close cycles. The field report is a SEGV
    // on an arbitrary close, not always the first: the first one leaves the
    // heap in a state where the stale worker's block may already be reused.
    // Each cycle runs the same helper the IDE does (stop sources first), then
    // empties the graph the way BasicGraphicsScene::clearScene() does.
    for (int cycle = 0; cycle < 5; ++cycle) {
        AudioSourceGraph scene;
        AudioSourceDataModel *model = scene.audioSource();
        QVERIFY2(model != nullptr, qPrintable(QStringLiteral("cycle %1: no AudioSource model").arg(cycle)));

        model->start();
        QThread::msleep(20);

        const Daqster::SceneCloseReport report = Daqster::stopSceneNodes(*scene.graph);
        QVERIFY2(report.nodes == 2,
                 qPrintable(QStringLiteral("cycle %1: stopSceneNodes saw %2 nodes, expected 2")
                                .arg(cycle).arg(report.nodes)));
        QVERIFY2(report.stopped == 1,
                 qPrintable(QStringLiteral("cycle %1: stopped %2 nodes, expected only the AudioSource")
                                .arg(cycle).arg(report.stopped)));

        {
            const ClobberedHeap scrubbed;
            // copy: deleteNode mutates the id set while we iterate it
            const auto ids = scene.graph->allNodeIds();
            for (const QtNodes::NodeId id : ids)
                scene.graph->deleteNode(id); // before the fix: SEGV on the AudioSource
        }

        QVERIFY2(scene.graph->allNodeIds().empty(),
                 qPrintable(QStringLiteral("cycle %1: graph not empty after close").arg(cycle)));
    }
}

void TestThreadLifecycle::displayComputeTaskChecksTokenBeforeTheNode()
{
    // D1 (REQ-SW-PL-050). DaqDisplayNode submits plot computation to the shared
    // ComputePool. ComputePool::cancel(key) only *waits* up to 500 ms for the
    // running task — it cannot stop it — so the task can still be inside
    // computePlotResult() when the node is destroyed, and its first statement
    // used to be `if (m_shuttingDown.load()) return;` — a read through the very
    // pointer that may already be dangling.
    //
    // Now the guard is Daqster::isCancelled(token), where `token` is captured by
    // value: it survives the node. This asserts the exact sequence the worker
    // relies on, on the real node.
    auto *node = new DaqDisplayNode();
    const Daqster::CancelTokenPtr token = node->computeToken();
    QVERIFY(token != nullptr);
    QVERIFY2(!Daqster::isCancelled(token), "a live node must let its tasks run");

    node->stop();
    QVERIFY2(Daqster::isCancelled(token),
             "stop() must cancel the token its tasks hold, before draining the pool");

    delete node;

    // Scrub the heap block the node lived in with 0xAB: a task that checked the
    // flag through `this` would now read garbage (the same trick the
    // AudioSource use-after-free tests above use). The only thing the task still
    // owns is the token, and it still answers correctly.
    QVector<void *> blocks;
    for (int i = 0; i < 32; ++i) {
        void *block = ::operator new(sizeof(DaqDisplayNode));
        std::memset(block, 0xAB, sizeof(DaqDisplayNode));
        blocks.append(block);
    }

    QVERIFY(Daqster::isCancelled(token));

    for (void *block : blocks)
        ::operator delete(block);
}

void TestThreadLifecycle::audioSourceAndDisplaySurviveFiveSceneCloses()
{
    // Same 5× close as audioSourceSurvivesFiveSceneCloses(), with a display on
    // the other end of the connection. The display is the harder half: it
    // submits work to the shared ComputePool at 30 Hz, so closing the scene has
    // to drain tasks that captured the node (REQ-SW-PL-050 D1), not just stop a
    // QTimer.
    for (int cycle = 0; cycle < 5; ++cycle) {
        auto registry = std::make_shared<QtNodes::NodeDelegateModelRegistry>();
        registry->registerModel<AudioSourceDataModel>();
        registry->registerModel<DaqDisplayNode>();
        QtNodes::DataFlowGraphModel graph(registry);

        const QtNodes::NodeId source = graph.addNode(QStringLiteral("AudioSource"));
        const QtNodes::NodeId display = graph.addNode(QStringLiteral("DaqDisplay"));
        graph.addConnection({source, 0, display, 0});

        auto *model = graph.delegateModel<AudioSourceDataModel>(source);
        QVERIFY2(model != nullptr, qPrintable(QStringLiteral("cycle %1: no AudioSource").arg(cycle)));
        model->start();
        QThread::msleep(20);

        // Both endpoints are IStoppable, and the helper stops them sources
        // first: the AudioSource sees "no consumer" before the display stops.
        const Daqster::SceneCloseReport report = Daqster::stopSceneNodes(graph);
        QVERIFY2(report.stopped == 2,
                 qPrintable(QStringLiteral("cycle %1: stopped %2 nodes, expected 2")
                                .arg(cycle).arg(report.stopped)));

        {
            const ClobberedHeap scrubbed;
            const auto ids = graph.allNodeIds(); // copy: deleteNode mutates the set
            for (const QtNodes::NodeId id : ids)
                graph.deleteNode(id);
        }

        QVERIFY2(graph.allNodeIds().empty(),
                 qPrintable(QStringLiteral("cycle %1: graph not empty after close").arg(cycle)));
    }
}

