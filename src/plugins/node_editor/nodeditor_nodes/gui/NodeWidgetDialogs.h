#ifndef NODEWIDGETDIALOGS_H
#define NODEWIDGETDIALOGS_H

#include <QApplication>
#include <QWidget>

namespace Daqster {

/**
 * @brief Parent window a dialog opened from a node widget should use.
 *
 * A node widget embedded in the graph is hosted by QGraphicsProxyWidget, which
 * turns it into its own *offscreen* top-level window (it keeps
 * Qt::WA_DontShowOnScreen). Passing it as a dialog parent makes the dialog a
 * transient child of that offscreen window: the dialog still grabs input
 * modally, but it is never really shown or drivable, so the whole application
 * looks frozen — its buttons stop responding and it cannot be closed. Opening
 * with the widget itself therefore has to be avoided while the widget is
 * embedded.
 *
 * The widget's own window is the right parent when it is a real one (a detached
 * floating node widget, or any ordinary parented widget). When it is the
 * offscreen embedded window we fall back to the currently active application
 * window — the window the user actually clicked in.
 *
 * @param widget the widget that wants to open a dialog (may be nullptr)
 * @return a suitable, real top-level parent for the dialog (may be nullptr)
 */
inline QWidget* nodeWidgetDialogParent(QWidget* widget)
{
    if (!widget)
        return QApplication::activeWindow();

    if (!widget->testAttribute(Qt::WA_DontShowOnScreen)) {
        if (QWidget* top = widget->window()) {
            if (top->windowHandle())
                return top;
        }
    }

    if (QWidget* active = QApplication::activeWindow())
        return active;

    return widget->window();
}

} // namespace Daqster

#endif // NODEWIDGETDIALOGS_H
