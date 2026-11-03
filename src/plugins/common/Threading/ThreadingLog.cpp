#include "Threading/ThreadingLog.h"

// "daqster.threading" — thread/process lifetime primitives (REQ-SW-PL-050).
// Deliberately not registered in the FrameworkCore category registry: this
// library is plugin-side and must not depend on FrameworkCore.
Q_LOGGING_CATEGORY(lcThreading, "daqster.threading")
