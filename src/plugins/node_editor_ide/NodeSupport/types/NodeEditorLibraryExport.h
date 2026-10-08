#ifndef NODEEDITORLIBRARY_EXPORT_H
#define NODEEDITORLIBRARY_EXPORT_H

#include <QtCore/qglobal.h>

// Export/import markup for the NodeEditorLibrary shared library.
//
//   NODE_EDITOR_LIBRARY_BUILD - defined when building the NodeEditorLibrary
//                               shared library (Q_DECL_EXPORT)
//   (not defined)             - consuming the library from another target
//                               (Q_DECL_IMPORT)
//
// NOTE (REQ-SW-PL-051): the four obsolete QDevIO display classes
// (QDevIoDisplayModelObsolete, QDevioDisplayModelUiObsolete,
// XYSeriesIODeviceObsolete, EventThreadPullObsolete) are deliberately NOT
// marked with this macro. Their .cpp files are compiled into BOTH
// NodeEditorLibrary (GUI builds) and DemoNodeEditorNodesCore (all builds —
// headless has no NodeEditorLibrary at all, see the ROUTING_NODES block in
// demo_nodeditor_nodes_core/CMakeLists.txt). Every consumer therefore compiles
// its own copy, so no cross-DLL import is ever needed. Marking them would make
// DemoNodeEditorNodesCore expand this to __declspec(dllimport), and its
// AUTOMOC pass — which runs over those same headers because the .cpp files are
// in its SOURCES — then fails to define staticMetaObject:
//   error C2491: definition of dllimport static data member not allowed
// plus a wall of C4273 'inconsistent dll linkage' warnings.
#if defined(NODE_EDITOR_LIBRARY_BUILD)
#  define NODE_EDITOR_LIBRARY_EXPORT Q_DECL_EXPORT
#else
#  define NODE_EDITOR_LIBRARY_EXPORT Q_DECL_IMPORT
#endif

#endif // NODEEDITORLIBRARY_EXPORT_H
