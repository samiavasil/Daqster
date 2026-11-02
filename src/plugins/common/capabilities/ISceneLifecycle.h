#pragma once

namespace Daqster {

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
};

} // namespace Daqster
