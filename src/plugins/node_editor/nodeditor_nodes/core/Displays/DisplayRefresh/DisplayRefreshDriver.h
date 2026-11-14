#pragma once

#include "nodeditor_nodes_core_export.h"

#include "Threading/CancelToken.h"

#include <QByteArray>
#include <QObject>

#include <functional>
#include <memory>

class QTimer;

/**
 * @brief 30 Hz "dirty → compute" throttle shared by the display nodes
 * (REQ-SW-PL-090).
 *
 * The display nodes (TextDisplay / ImageDisplay / FrameWindowDisplay) all have
 * the same shape: `setInData()` on the graph thread may only store the newest
 * payload and raise a flag, and the actual work (truncation, downscaling,
 * statistics) runs on the shared ComputePool (REQ-SW-PL-039) at most 30 times
 * a second. That timing choreography — QTimer, per-node pool key, CancelToken —
 * is identical for all three, so it lives here once instead of three times.
 *
 * Shutdown contract (REQ-SW-PL-050 D1): `stop()` stops the timer and cancels
 * the token BEFORE draining the pool, so a task that outlives its node returns
 * without touching it. `stop()` is idempotent; the destructor calls it.
 *
 * GUI-thread only for markDirty()/submit()/stop() — the timer and the pool's
 * submission API both live there.
 */
class NODEDITOR_NODES_CORE_EXPORT DisplayRefreshDriver : public QObject
{
    Q_OBJECT

public:
    /// `parent` is the display node: the driver is destroyed with it.
    explicit DisplayRefreshDriver(QObject *parent = nullptr);
    ~DisplayRefreshDriver() override;

    /// The display is stale — render it on the next tick. Cheap; safe to call
    /// on every incoming block (that is the point).
    void markDirty() { m_dirty = true; }

    /// True while a render is pending.
    bool isDirty() const { return m_dirty; }

    /// Submit `task` to the shared pool under this node's key. The task runs
    /// on a worker thread and is skipped entirely once the token is cancelled.
    /// Latest-wins: a queued task for the same node is replaced.
    bool submit(std::function<void()> task);

    /// The token every task captures BY VALUE (REQ-SW-PL-050 D1). A task that
    /// must not post a result after shutdown checks it a second time, right
    /// before the queued delivery.
    Daqster::CancelTokenPtr token() const { return m_token; }

    /// Stop the timer and cancel pending/running work. Idempotent.
    void stop();

signals:
    /// Emitted on the GUI thread at most ~30 Hz, only while dirty. The owner
    /// submits its compute pass here — and may re-arm with markDirty() when
    /// nobody consumes the result yet (no widget attached).
    void refreshRequested();

private slots:
    void onTick();

private:
    QTimer *m_timer = nullptr;
    QByteArray m_poolKey;
    /// Cancelled by stop() before ComputePool::cancel(); every task captures a
    /// copy BY VALUE, so the check survives this driver's destruction.
    Daqster::CancelTokenPtr m_token = std::make_shared<Daqster::CancelToken>();
    bool m_dirty = false;
    bool m_stopped = false;
};
