#ifndef PLUGINSCOMMON_EXPORT_H
#define PLUGINSCOMMON_EXPORT_H

#include <QtCore/qglobal.h>

// Export/import markup for the PluginsCommonLibrary shared library.
//
// On Windows, symbols must be marked with __declspec(dllexport/dllimport) so
// consumers (demo_nodeditor_nodes_core, its _gui sibling, private plugins such
// as ai_studio_plugin, and their test binaries) can link against the library.
// WINDOWS_EXPORT_ALL_SYMBOLS alone is not enough for a global data symbol —
// the singleton behind TexturePool::instance() has to be exported explicitly.
//
// On Linux/macOS the macro expands to nothing: the .so exports everything by
// default. What the shared library buys there is not visibility but ARITY —
// exactly one definition of the pool in the process, instead of one compiled
// copy per plugin that includes the header.
//
//   PLUGINSCOMMON_LIBRARY - defined when building the PluginsCommonLibrary
//                           shared library (Q_DECL_EXPORT)
//   (not defined)         - consuming the library from another target
//                           (Q_DECL_IMPORT)
#if defined(PLUGINSCOMMON_LIBRARY)
#  define PLUGINSCOMMON_EXPORT Q_DECL_EXPORT
#else
#  define PLUGINSCOMMON_EXPORT Q_DECL_IMPORT
#endif

#endif // PLUGINSCOMMON_EXPORT_H
