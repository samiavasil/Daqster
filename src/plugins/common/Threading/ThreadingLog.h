#pragma once

// SPDX-License-Identifier: MIT
//
// Logging category for the lifetime primitives in this directory
// (REQ-SW-PL-050): ThreadOwner, ProcessOwner and the nodes that use them.
// `Servers/LocalServerService` reports its process/port/health lifecycle here
// too — it is the same question ("who owns this child, and when does it go
// away"), only asked about a server (REQ-SW-PL-081).
//
// Declared in a header and defined in exactly one translation unit
// (Threading/ThreadingLog.cpp) instead of a Q_LOGGING_CATEGORY per file: a
// header-only primitive (ThreadOwner) would otherwise either emit into the
// default category or define the category once per including translation unit.

#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(lcThreading)
