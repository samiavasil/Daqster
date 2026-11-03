#pragma once

// SPDX-License-Identifier: MIT
//
// Logging category for the lifetime primitives in this directory
// (REQ-SW-PL-050): ThreadOwner, ProcessOwner and the nodes that use them.
//
// Declared in a header and defined in exactly one translation unit
// (Threading/ThreadingLog.cpp) instead of a Q_LOGGING_CATEGORY per file: a
// header-only primitive (ThreadOwner) would otherwise either emit into the
// default category or define the category once per including translation unit.

#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(lcThreading)
