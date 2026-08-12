#include "ProcessCpu.h"

#include <QtGlobal>

#ifdef Q_OS_WIN
#  include <windows.h>
#else
#  include <fstream>
#  include <sstream>
#  include <string>
#  include <unistd.h>
#endif

namespace Daqster::Perf {

double ProcessCpu::sample()
{
    const std::int64_t cpuUnits = totalCpuUnits();
    const auto wall = std::chrono::steady_clock::now();

    if (!m_hasSample) {
        // First call: baseline only.
        m_lastCpuUnits = cpuUnits;
        m_lastWall = wall;
        m_hasSample = true;
        return 0.0;
    }

    const double deltaCpu = static_cast<double>(cpuUnits - m_lastCpuUnits);
    const double deltaWallSec =
        std::chrono::duration<double>(wall - m_lastWall).count();

    m_lastCpuUnits = cpuUnits;
    m_lastWall = wall;

    if (deltaWallSec <= 0.0)
        return 0.0;

    return (deltaCpu / unitsPerSecond()) / deltaWallSec * 100.0;
}

#ifdef Q_OS_WIN

std::int64_t ProcessCpu::totalCpuUnits()
{
    FILETIME creationTime;
    FILETIME exitTime;
    FILETIME kernelTime;
    FILETIME userTime;
    if (!GetProcessTimes(GetCurrentProcess(), &creationTime, &exitTime,
                         &kernelTime, &userTime)) {
        return 0;
    }

    ULARGE_INTEGER kernel;
    kernel.LowPart = kernelTime.dwLowDateTime;
    kernel.HighPart = kernelTime.dwHighDateTime;

    ULARGE_INTEGER user;
    user.LowPart = userTime.dwLowDateTime;
    user.HighPart = userTime.dwHighDateTime;

    return static_cast<std::int64_t>(kernel.QuadPart + user.QuadPart);
}

double ProcessCpu::unitsPerSecond()
{
    return 1e7; // 100 ns per FILETIME unit
}

#else

std::int64_t ProcessCpu::totalCpuUnits()
{
    // /proc/self/stat layout (proc_pid_stat(5)):
    //   pid (comm) state ppid pgrp session tty_nr tpgid flags minflt cminflt
    //   majflt cmajflt utime stime ...
    // utime (field 14) and stime (field 15) are in clock ticks. The comm field
    // (2) is parenthesised and may itself contain spaces/parens, so locate the
    // LAST ')' and parse the 13 integers that follow it — utime is the 12th,
    // stime the 13th.
    std::ifstream stat("/proc/self/stat");
    if (!stat.is_open())
        return 0;

    std::string line;
    std::getline(stat, line);

    const std::string::size_type rparen = line.rfind(')');
    if (rparen == std::string::npos)
        return 0;

    std::istringstream fields(line.substr(rparen + 1));

    std::int64_t utime = 0;
    std::int64_t stime = 0;
    std::int64_t value = 0;
    for (int i = 0; i < 13; ++i) {
        if (!(fields >> value))
            return 0;
        if (i == 11)
            utime = value; // field 14
        else if (i == 12)
            stime = value; // field 15
    }

    return utime + stime;
}

double ProcessCpu::unitsPerSecond()
{
    const long clk = sysconf(_SC_CLK_TCK);
    return clk > 0 ? static_cast<double>(clk) : 100.0;
}

#endif

} // namespace Daqster::Perf
