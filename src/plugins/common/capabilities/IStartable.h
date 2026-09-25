#pragma once

namespace Daqster {

/// Interface for node models that can be started programmatically (runtime
/// autoStart, REQ-SW-PL-048). Separate from IStoppable — display/processing
/// nodes stay untouched. Implemented ONLY by source/sink nodes that have a
/// user-facing "start" action.
class IStartable
{
public:
    virtual ~IStartable() = default;
    virtual void start() = 0;
};

} // namespace Daqster
