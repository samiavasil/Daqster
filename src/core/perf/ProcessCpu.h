#pragma once

#include <chrono>
#include <cstdint>

namespace Daqster::Perf {

// Self-CPU sampler for the current process.
//
// sample() returns the CPU time consumed by the process over the interval
// since the previous sample, as a percentage of wall-clock time. On multi-core
// systems the value can exceed 100%. The first call only establishes the
// baseline and returns 0.0.
//
// Linux reads utime + stime from /proc/self/stat (clock ticks, scaled by
// sysconf(_SC_CLK_TCK)); Windows uses GetProcessTimes() (KernelTime +
// UserTime, 100-ns FILETIME units). The platform-specific implementation lives
// in ProcessCpu.cpp behind #ifdef Q_OS_WIN.
class ProcessCpu {
public:
    double sample();

private:
    // Total CPU time consumed so far, in platform units (clock ticks on
    // Linux, 100-ns intervals on Windows).
    static std::int64_t totalCpuUnits();
    // Platform CPU-time units per second (CLK_TCK on Linux, 1e7 on Windows).
    static double unitsPerSecond();

    std::int64_t m_lastCpuUnits = 0;
    std::chrono::steady_clock::time_point m_lastWall{};
    bool m_hasSample = false;
};

} // namespace Daqster::Perf
