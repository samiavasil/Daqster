#include "DisplayRefreshDriver.h"

#include "Threading/ComputePool.h"

#include <QTimer>

namespace {

/// ~30 Hz: fast enough that a text/image block looks live, slow enough that a
/// 60 fps source cannot turn display work into per-frame work (same budget as
/// DaqDisplayNode, REQ-SW-PL-023 §5).
constexpr int kRefreshIntervalMs = 1000 / 30;

} // namespace

DisplayRefreshDriver::DisplayRefreshDriver(QObject *parent)
    : QObject(parent)
{
    // Per-node key into the shared ComputePool (REQ-SW-PL-039): per-key
    // latest-wins submission + per-key serialization.
    m_poolKey = QByteArray::number(reinterpret_cast<quintptr>(this));

    m_timer = new QTimer(this);
    m_timer->setInterval(kRefreshIntervalMs);
    connect(m_timer, &QTimer::timeout, this, &DisplayRefreshDriver::onTick);
    m_timer->start();
}

DisplayRefreshDriver::~DisplayRefreshDriver()
{
    stop();
}

bool DisplayRefreshDriver::submit(std::function<void()> task)
{
    if (m_stopped || !task)
        return false;

    // The token copy is captured BY VALUE: the node (and this driver) may be
    // destroyed while the task is still queued or running.
    auto token = m_token;
    return Daqster::ComputePool::instance().submitLatest(
        m_poolKey,
        [token, task = std::move(task)]() {
            if (Daqster::isCancelled(token))
                return; // the owner is stopping/stopped — do not touch it
            task();
        });
}

void DisplayRefreshDriver::stop()
{
    // Idempotent: stopping a stopped timer / cancelling a cancelled token and
    // an empty pool key are all no-ops.
    m_stopped = true;
    if (m_timer != nullptr)
        m_timer->stop();

    // D1 (REQ-SW-PL-050): cancel the token BEFORE draining, because cancel()
    // only waits (≤ 500 ms) for the running task — it cannot kill it.
    m_token->cancel();
    Daqster::ComputePool::instance().cancel(m_poolKey);
}

void DisplayRefreshDriver::onTick()
{
    if (m_stopped || !m_dirty)
        return;

    m_dirty = false;
    Q_EMIT refreshRequested();
}
