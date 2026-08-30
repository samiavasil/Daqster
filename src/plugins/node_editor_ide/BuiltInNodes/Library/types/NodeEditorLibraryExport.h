#ifndef NODEEDITORLIBRARY_EXPORT_H
#define NODEEDITORLIBRARY_EXPORT_H

#include <QtCore/qglobal.h>

// Export/import markup for the NodeEditorLibrary shared library.
//
// On Windows, moc-generated meta-object data symbols (e.g. the static
// QMetaObject QDevIoDisplayModelObsolete::staticMetaObject) are NOT linkable
// from consumers through CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS alone: global data
// symbols must be marked with __declspec(dllexport/dllimport) so the consumer
// references them via the import thunk (__imp_*). Without this, consumers
// fail to link with LNK2019 (unresolved external symbol ...::staticMetaObject).
//
//   NODE_EDITOR_LIBRARY_BUILD - defined when building the NodeEditorLibrary
//                               shared library (Q_DECL_EXPORT)
//   (not defined)             - consuming the library from another target
//                               (Q_DECL_IMPORT)
#if defined(NODE_EDITOR_LIBRARY_BUILD)
#  define NODE_EDITOR_LIBRARY_EXPORT Q_DECL_EXPORT
#else
#  define NODE_EDITOR_LIBRARY_EXPORT Q_DECL_IMPORT
#endif

#endif // NODEEDITORLIBRARY_EXPORT_H
