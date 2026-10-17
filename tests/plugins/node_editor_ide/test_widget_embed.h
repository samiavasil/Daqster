#pragma once

#include <QtTest>

// Regression for the F6 Layout Designer round-trip (REQ-SW-PL-059).
//
// Entering designer mode deembeds every node widget; the designer preview then
// reparents those widgets into its own QMainWindow (LayoutDesignerPanel::
// rebuildPreview -> LayoutEngine::buildWorkspace). Leaving designer mode
// re-embeds them. QGraphicsProxyWidget::setWidget() only accepts a *toplevel*
// widget, so re-embedding a widget that came back as a child of the preview
// silently failed ("cannot embed widget ... which is not a toplevel widget"),
// leaving the node body empty and offering a stale "Embed" context-menu entry
// whose action re-ran the same failing path.
//
// Declared in a header so it can share the single node_editor_ide_tests binary
// (see test_main.cpp). Runs under QApplication with the offscreen platform.
class TestWidgetEmbed : public QObject
{
    Q_OBJECT

private slots:
    void reembedAfterDesignerPreviewRoundTrip();
};
