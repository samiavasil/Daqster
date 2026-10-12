#ifndef DAQSTER_HEADLESS_APP_H
#define DAQSTER_HEADLESS_APP_H

#include "framework_core_export.h"

namespace Daqster {

/**
 * @brief Prepare the process for headless flow execution on the "offscreen"
 *        platform plugin.
 *
 * The platform plugin is selected during QGuiApplication/QApplication
 * construction, so a caller that wants a display-less headless process must
 * arrange QT_QPA_PLATFORM before building the app object. This helper sets it
 * to "offscreen" when the environment has no explicit value.
 *
 * REQ-SW-PL-051: node models used to construct their editor widgets inside
 * their constructors, which forced headless runs onto QApplication. That is
 * gone — the per-node core/gui split (finished by REQ-SW-PL-056) makes model
 * construction widget-free. NodeRunner's --headless path uses QApplication with
 * the offscreen platform (via this helper or the test environment's
 * QT_QPA_PLATFORM=offscreen). QGuiApplication was attempted but has
 * argument-parsing issues on offscreen. This helper remains exported for
 * embedders that run a widget-capable headless process.
 *
 * An explicit QT_QPA_PLATFORM in the environment is honoured, so a caller
 * can still pick a different platform plugin.
 *
 * Must be called BEFORE the QApplication instance is constructed, since the
 * platform plugin is selected during construction.
 */
FRAMEWORK_CORE_EXPORT void configureHeadlessPlatform();

} // namespace Daqster

#endif // DAQSTER_HEADLESS_APP_H
