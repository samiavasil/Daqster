#ifndef BUILD_CFG_H
#define BUILD_CFG_H

#define QT_FW_ENABLED/*TOODO:Move this hard coded configuration in some appropriatary place*/

#if defined( QT_FW_ENABLED )

/*Define basic types*/
#include <stdint.h>
typedef int8_t    int8;
typedef int16_t   int16;
typedef int32_t   int32;
typedef int64_t   int64;
typedef uint8_t   u8;
typedef uint16_t  u16;
typedef uint32_t  u32;
typedef uint64_t  u64;

/*Configure dynamic library export macro*/
#include <qglobal.h>
#if defined(FRAME_WORK_LIBRARY)
  #define FRAME_WORKSHARED_EXPORT Q_DECL_EXPORT
#else
  #define FRAME_WORKSHARED_EXPORT Q_DECL_IMPORT
#endif

#else /*To configure different - Not QT based build Just for base part. Please define types and macro */
#error "Missing Build configuration. Please configure build configuration correctly."
#endif

#endif // BUILD_CFG_H
