#ifndef DAQSTER_HEADLESS_APP_H
#define DAQSTER_HEADLESS_APP_H

#include "framework_core_export.h"

namespace Daqster {

/**
 * @brief Prepare the process for headless flow execution.
 *
 * REQ-SW-PL-051: node models construct their editor widgets in their
 * constructors (the core plugin deliberately keeps its widget classes
 * rather than going through a widget factory — see the "headless = no
 * canvas, NOT no widgets" decision). A headless run therefore still needs
 * a QApplication, not a QCoreApplication: the first widget construction
 * otherwise aborts with
 *   "QWidget: Cannot create a QWidget without QApplication".
 *
 * We run QApplication on the "offscreen" platform plugin, so the widgets
 * are real and fully functional while no window is ever mapped. Nothing is
 * shown and no display server is required.
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
