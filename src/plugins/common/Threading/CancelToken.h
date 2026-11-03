#pragma once

// SPDX-License-Identifier: MIT
//
// A one-way cancellation flag that a task can share with the object it works
// for (REQ-SW-PL-050).
//
// The problem it solves: a task submitted to a thread pool captures data by
// value, but the *guard* it used to check ("am I still wanted?") lived on the
// node — so checking the guard was itself a dereference of a pointer that may
// already be dangling. Half of the shutdown race is not "the task reads a
// member after the destructor", it is "the task reads the flag that tells it
// whether it may read a member".
//
// A CancelToken is owned by std::shared_ptr. The owner keeps one, every task
// takes a copy **by value**, and the owner cancels it before it tears anything
// down. After that the token (and therefore the check) is still valid even
// though the owner is gone.
//
// Usage:
//   // once, in the owner's constructor / start():
//   m_token = std::make_shared<Daqster::CancelToken>();
//
//   // per task — capture by value, never `this` for the guard:
//   pool.submitLatest(key, [token = m_token, this]() {
//       if (Daqster::isCancelled(token))
//           return;                 // the owner may already be destroyed
//       ...
//   });
//
//   // shutdown, BEFORE draining the pool:
//   m_token->cancel();
//   pool.cancel(key);

#include <atomic>
#include <memory>

namespace Daqster {

/// One-way, thread-safe "stop" flag shared with tasks that may outlive the
/// object that submitted them.
///
/// `cancel()` is idempotent and may be called from any thread; `cancelled()`
/// uses acquire/release ordering, so a task that observes `true` also observes
/// everything the owner wrote before cancelling.
class CancelToken
{
public:
    CancelToken() = default;

    CancelToken(const CancelToken &) = delete;
    CancelToken &operator=(const CancelToken &) = delete;

    /// Raise the flag. Idempotent; safe from any thread.
    void cancel() noexcept { m_cancelled.store(true, std::memory_order_release); }

    /// True once cancel() has been called.
    bool cancelled() const noexcept
    {
        return m_cancelled.load(std::memory_order_acquire);
    }

private:
    std::atomic<bool> m_cancelled{false};
};

using CancelTokenPtr = std::shared_ptr<CancelToken>;

/// The prologue of every task that captured a token by value.
///
/// Returns true when the task must return **without touching the owner** — the
/// token was cancelled, or (defensively) no token was captured at all. Callers
/// must treat this as the first statement of the task body, before any `this`
/// dereference, so a cancelled task never reaches freed memory.
inline bool isCancelled(const CancelTokenPtr &token) noexcept
{
    return !token || token->cancelled();
}

} // namespace Daqster
