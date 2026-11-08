#pragma once

#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>

class QAction;
class QMenu;
class QMenuBar;

namespace Daqster {
class IMenuProvider;
}

/**
 * @brief Places IMenuProvider actions into a host menu bar (REQ-SW-PL-084).
 *
 * `setProviders()` is the whole API: it collects every provider's
 * `menuActions()`, resolves `menuPath` ("Tools", "Tools/Plugins/AI") to the
 * real `QMenu` — reusing a menu that already exists, creating the missing
 * levels — and adds a `QAction` per entry that calls the provider back with
 * its id.
 *
 * Path resolution:
 *  - a top-level segment matches an existing menubar menu by title (`&`
 *    ignored) and is reused, never taken over;
 *  - every deeper segment does the same inside its parent, so a framework
 *    submenu such as `Tools → Plugins` is not duplicated;
 *  - menus this controller created are remembered and removed on the next
 *    `setProviders()`, so re-installing never leaves a copy behind and never
 *    empties a menu the host owns.
 *
 * Robustness: the controller is the boundary between the host and foreign
 * plugin code. Null providers, an empty `menuPath`, an empty `id` (nothing to
 * send back), an empty `text` (id is shown), a disabled action and an
 * exception out of `triggerMenuAction()` are all handled — logged, skipped or
 * swallowed, never a crash and never a half-built menu.
 *
 * Lifecycle: the menu bar is held through `QPointer`, so after the host window
 * is gone `setProviders()` is a no-op instead of a dangling-pointer write.
 */
class PluginMenuController : public QObject
{
    Q_OBJECT
public:
    /**
     * @param menuBar Host menu bar. May be null (a test) — then every call is
     *                a no-op.
     * @param parent  QObject parent (normally the host plugin object)
     */
    explicit PluginMenuController(QMenuBar* menuBar, QObject* parent = nullptr);

    ~PluginMenuController() override;

    /**
     * @brief (Re)install the actions of `providers`.
     *
     * Idempotent: the actions and menus installed by the previous call are
     * removed first, so calling it twice with the same providers leaves one
     * entry per action, not two.
     *
     * @param providers Providers from `QPluginManager::menuProviders()`
     */
    void setProviders(const QList<Daqster::IMenuProvider*>& providers);

    /**
     * @brief Number of actions currently installed (test seam / diagnostics).
     *
     * @return Action count, 0 when nothing is installed
     */
    int actionCount() const;

    /**
     * @brief Actions currently installed, in installation order.
     *
     * Ownership stays with the controller (and its menus); the caller must not
     * delete them.
     *
     * @return Installed actions
     */
    QList<QAction*> installedActions() const;

private:
    /// Removes everything installed by the previous setProviders() call.
    void uninstall();

    /// Finds the menu at `segments`, creating the missing levels when asked.
    QMenu* resolveMenu(const QStringList& segments, bool create);

    /// Top-level or nested menu with this title (case-sensitive, `&` ignored).
    static QMenu* findChildMenu(QMenu* parent, const QString& title);

    QPointer<QMenuBar> m_menuBar;
    QList<Daqster::IMenuProvider*> m_providers;
    /// Installed actions; QPointer so a host that deletes the menu first leaves
    /// no dangling handles behind.
    QList<QPointer<QAction>> m_actions;
    /// Menus created by this controller, in creation order (parents first).
    QList<QPointer<QMenu>> m_createdMenus;
};
