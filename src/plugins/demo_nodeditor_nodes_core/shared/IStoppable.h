#pragma once

#include <QObject>

namespace Daqster {

/// Interface for node models that run background work (threads, timers, processes).
/// Implemented ONLY by nodes that need explicit stopping.
/// Idempotent — stop() must be safe to call multiple times.
class IStoppable
{
public:
    virtual ~IStoppable() = default;
    virtual void stop() = 0;
};

} // namespace Daqster
