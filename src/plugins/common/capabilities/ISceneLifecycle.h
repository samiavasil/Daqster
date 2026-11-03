#pragma once

namespace Daqster {

/**
 * @brief Why a scene is being closed — a **hint** for a plugin's close policy.
 *
 * A plugin that keeps resources alive after its nodes are gone (cached model
 * weights, a shared broker, a warm server) must know *who* closed the scene:
 *
 *  - `Replace` — a new scene is loaded over the old one (hot-reload,
 *    `POST /api/flow`, `loadFlow()`), or the application is quitting;
 *  - `ExplicitClose` — the user (or an explicit `POST /api/flow/close`) asked
 *    for the scene to be gone: `File → Close Scene` (Ctrl+W),
 *    `RuntimeShell::closeFlow()`, `HeadlessEngine::closeFlow()`.
 *
 * The reason is informational, **not** a command: the host says *why* the scene
 * goes away, the plugin decides *what* to keep — and a policy configured by the
 * user (env var, settings, UI) always wins over the reason. Keeping caches warm
 * is the default for both reasons, because an evictable cache costs nothing
 * while the process lives and re-opening pays a cold start; giving the memory
 * back is an explicit action (a "release" button, `DAQSTER_*_CLOSE_POLICY`, or
 * process shutdown), never a side effect of Ctrl+W.
 *
 * What the reason does **not** change: every node still releases its own
 * per-node resources (contexts, child processes, devices) in `IStoppable::stop()`
 * regardless of the reason — that part is the close contract, not a policy.
 */
enum class SceneCloseReason {
    Replace,        ///< нова сцена върху стара (hot-reload) или изход от приложението
    ExplicitClose,  ///< изричен Close Scene (Ctrl+W / `POST /api/flow/close`)
};

/**
 * @brief Optional plugin-level hook for the **close** half of a scene lifecycle.
 *
 * The runtime hosts (HeadlessEngine, RuntimeShell, the NodeEditor) already stop
 * every `IStoppable` node before a scene is replaced or the window is closed.
 * Stopping the nodes is necessary but not sufficient: a plugin may own resources
 * that outlive its nodes by design (cached model weights, a shared broker, a
 * warm server). Those need one explicit "the scene is gone" moment.
 *
 * The host calls `onSceneClosed()` **after** every node of the outgoing scene
 * received `IStoppable::stop()` and **before** the node models are destroyed.
 * The order matters: nodes release their own contexts/processes in `stop()`,
 * then the plugin decides what stays warm and what is dropped.
 *
 * `closeScene()` is the sibling contract on the node side: every node that took
 * a `ContextHandle`, a process or a device releases it in `stop()` **and** in
 * its destructor (the destructor is the last line of defence, not the only
 * line). This interface is the plugin-level complement to that rule.
 *
 * Implemented by plugin objects next to `INodeProvider`; a provider that does
 * not implement it is simply skipped (the hook is optional). Idempotent — a
 * close cannot fail, and hosts may run it more than once.
 */
class ISceneLifecycle
{
public:
    virtual ~ISceneLifecycle() = default;

    /// The outgoing scene has been stopped; apply the plugin's close policy.
    virtual void onSceneClosed() = 0;

    /**
     * @brief The same hook, with the reason the scene is going away.
     *
     * Added after the original hook, so the vtable slot of `onSceneClosed()`
     * (slot 1) is unchanged: an older host that only knows the no-argument
     * call keeps working, and an older plugin that only implements the
     * no-argument hook is still called — this default implementation simply
     * forwards to it, i.e. keeps the pre-reason behaviour.
     *
     * A plugin that wants to log or record the difference overrides **this**
     * method; its own no-argument implementation may then forward here with a
     * default reason, since no host calls the no-argument form any more. What
     * the plugin does with the reason is its own policy — the reason never
     * implies "release everything" (see `SceneCloseReason`).
     *
     * @param reason `Replace` for a hot-reload / quit, `ExplicitClose` for a
     *               close the user asked for.
     */
    virtual void onSceneClosed(SceneCloseReason reason)
    {
        static_cast<void>(reason);
        onSceneClosed();
    }
};

} // namespace Daqster
