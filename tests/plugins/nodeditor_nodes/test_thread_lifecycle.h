#pragma once

#include <QtNodes/NodeDelegateModel>

#include <QtTest>

#include <functional>
#include <memory>

/// Lifecycle tests for the IStoppable / IStartable contract
/// (REQ-SW-PL-050, AC 7).
///
/// Every node that owns a thread, a timer or a subprocess implements
/// Daqster::IStoppable. The shutdown path is entirely duck-typed:
/// HeadlessEngine::stopAllNodes(), NodeEditorWidget::stopAllNodes() and
/// RuntimeShell::stopAllNodes() each walk the graph and
/// `dynamic_cast<Daqster::IStoppable*>` every model. Nothing in the type
/// system says a new source node must implement it, nothing enforces that
/// stop() is idempotent, and nothing notices a node whose stop() leaves a
/// QTimer or a QThread running — the process just hangs or crashes on exit,
/// which is the worst possible time to find out.
///
/// So the contract is asserted directly, on the real models, with the same
/// cast the shutdown path uses. Any model that is unreachable through that
/// cast is a node whose background work outlives the app.
///
/// QTEST_GUILESS_MAIN provides QCoreApplication. The models under test own
/// no widget (REQ-SW-PL-051) and are idle until start().
class TestThreadLifecycle : public QObject
{
    Q_OBJECT

private slots:
    // The cast the entire shutdown path depends on.
    void stoppableCastReachesEveryModel();

    // IStoppable's documented idempotency requirement.
    void stopIsIdempotent();
    void stopWithoutStartIsSafe();
    void destroyWithoutStopIsSafe();
    void destroyAfterStopIsSafe();
    void stopWithActiveWorkIsSafe();
    void stopReleasesTheTimer();

    // If the cast fails, the whole suite above is decoration — a node whose
    // stop() is never called cannot pass a "stop is safe" test.
    void everyModelReportsItselfStoppable();

    void startThenStopIsClean();

    // The cast the app uses (HeadlessEngine.cpp:17 / NodeEditorWidget.cpp:5)
    // and the interface the models satisfy (…/shared/IStoppable.h) are two
    // separate definitions of the same class name. Asserted so the duplication
    // cannot drift silently. See the .cpp for the full story.
    void bothInterfaceHeadersDeclareTheSameType();
    void shutdownPathCastMatchesTheModelsInterface();

private:
    /// One entry per node that owns background work: a label plus a factory.
    struct ModelEntry
    {
        QString label;
        std::function<std::unique_ptr<QtNodes::NodeDelegateModel>()> create;
    };

    /// Every model in _core that inherits Daqster::IStoppable and constructs
    /// inertly — pollers, timers and threads are created but never started, and
    /// no device is opened. This is the list the cast / idempotency /
    /// destruction assertions run over, because none of those need a device.
    static QVector<ModelEntry> allStoppableModels();

    /// The subset whose start() is also safe with no hardware attached. Only
    /// the two active-work assertions need this: joining a running poller is
    /// the failure that hangs at shutdown, and it is unreachable without
    /// actually starting the thread.
    static QVector<ModelEntry> startableWithoutDeviceModels();
};
