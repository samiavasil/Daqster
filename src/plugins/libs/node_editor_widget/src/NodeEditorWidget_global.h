#pragma once

#include <QtCore/qglobal.h>

#if defined(NODE_EDITOR_WIDGET_LIBRARY)
  #define NODE_EDITOR_WIDGET_EXPORT Q_DECL_EXPORT
#else
  #define NODE_EDITOR_WIDGET_EXPORT Q_DECL_IMPORT
#endif
