#ifndef BUILD_CFG_H
#define BUILD_CFG_H

#define QT_FW_ENABLED

#if defined( QT_FW_ENABLED )

#include <QtCore/qglobal.h>

// When building/using frame_work as a static library (e.g. on
// Windows MinGW in CI), we must not use dllimport/dllexport.
// FRAME_WORK_STATIC is defined via CMake in that case.
#if defined(FRAME_WORK_STATIC)
  #define FRAME_WORKSHARED_EXPORT
#elif defined(FRAME_WORK_LIBRARY)
  #define FRAME_WORKSHARED_EXPORT Q_DECL_EXPORT
#else
  #define FRAME_WORKSHARED_EXPORT Q_DECL_IMPORT
#endif

#else /*To configure different - Not QT based build Just for base part. Please define types and macro */
#error "Missing Build configuration. Please configure build configuration correctly."
#endif

#endif // BUILD_CFG_H
