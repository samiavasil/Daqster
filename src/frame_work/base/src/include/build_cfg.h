#ifndef BUILD_CFG_H
#define BUILD_CFG_H

#define QT_FW_ENABLED/*TOODO:Move this hard coded configuration in some appropriatary place*/

#if defined( QT_FW_ENABLED )

/*Define basic types*/
#include <cstdint>
using int8  = int8_t;
using int16 = int16_t;
using int32 = int32_t;
using int64 = int64_t;
using u8    = uint8_t;
using u16   = uint16_t;
using u32   = uint32_t;
using u64   = uint64_t;

/*Configure dynamic library export macro*/
/*Include Qt headers - try Qt6 first, fallback to Qt5*/
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
