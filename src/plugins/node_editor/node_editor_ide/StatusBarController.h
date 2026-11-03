#pragma once

#include <QLabel>
#include <QList>
#include <QMenu>
#include <QObject>
#include <QPointer>
#include <QStatusBar>
#include <QString>
#include <QStringList>
#include <QTimer>

class QAction;
class QEvent;

namespace Daqster {
class IStatusBarProvider;
}

/**
 * @brief Renders IStatusBarProvider plugins into a host status bar and menu
 *        (REQ-SW-PL-079).
 *
 * One `QLabel` per provider inside the status bar (permanent widgets, so a
 * transient message never hides them), ordered by `statusOrder()` and then by
 * `statusId()` for a stable order. All provider actions are listed in one
 * dedicated menu; picking one calls the provider back with its action id.
 *
 * Refresh policy:
 *  - a `QTimer` (2 s, `startAutoRefresh()`) calls `refresh()`, and
 *  - the host calls `refresh()` explicitly after loading or closing a scene,
 *  - `setProviders()` re-reads the registry when plugins come or go.
 *
 * `refresh()` only touches strings and label visibility — the expensive part
 * is the provider's `statusText()`, which is called on every tick and must
 * therefore be cheap (see IStatusBarProvider).
 *
 * Clicking a label shows the provider's tooltip; it never triggers an action.
 * The actions are destructive by nature (free model weights, restart a
 * server), so they live in the menu where choosing one is deliberate.
 *
 * Robustness: the controller is the boundary between the IDE and foreign
 * plugin code, so it assumes nothing. A null provider, a provider whose object
 * was destroyed, an empty `statusText()` (the row hides), or an exception out
 * of `statusText()`/`triggerStatusAction()` (logged, row hidden, no crash) are
 * all normal outcomes.
 *
 * Lifecycle: the controller may outlive its widgets. `QStatusBar`, `QMenu` and
 * every label are held through `QPointer`, so after the host window is gone
 * `refresh()` becomes a no-op instead of a dangling-pointer write.
 */
class StatusBarController : public QObject
{
    Q_OBJECT
public:
    /// Default tick of the auto-refresh timer — one light string call per tick.
    static constexpr int kDefaultRefreshMs = 2000;

    /**
     * @param statusBar The host's status bar. May be null (a test) — then only
     *                  the menu side exists.
     * @param actionsMenu The menu the controller owns: every entry is cleared
     *                  and rebuilt by this class, so the host must hand it a
     *                  dedicated (sub)menu, not a shared one.
     * @param parent Owner; the controller lives as long as it does.
     */
    StatusBarController(QStatusBar* statusBar, QMenu* actionsMenu,
                        QObject* parent = nullptr);
    ~StatusBarController() override;

    /**
     * @brief Replace the provider set: rebuild labels and menu actions.
     *
     * Duplicate `statusId()` values are a provider bug, not a host failure: the
     * first one wins and a warning is logged.
     *
     * @param providers Providers from PluginRegistry::statusBarProviders()
     */
    void setProviders(const QList<Daqster::IStatusBarProvider*>& providers);

    /**
     * @brief Start the periodic refresh.
     *
     * @param intervalMs Tick in milliseconds; <= 0 keeps the timer stopped.
     */
    void startAutoRefresh(int intervalMs = kDefaultRefreshMs);

    /// The label of one provider, or nullptr (unknown id / hidden window).
    QLabel* labelFor(const QString& statusId) const;

    /// Number of rows currently built (one per accepted provider).
    int labelCount() const;

    /// The accepted provider ids, in status-bar order (left to right).
    QStringList statusIds() const;

    /// The actions currently listed in the menu, in menu order.
    QList<QAction*> menuActions() const;

public slots:
    /**
     * @brief Re-read text, tooltip and action set from every provider.
     *
     * Cheap by design: called every `kDefaultRefreshMs`. Never throws.
     */
    void refresh();

    /// Run one provider action by id — the menu's entry point.
    void triggerAction(const QString& statusId, const QString& actionId);

signals:
    /**
     * @brief A provider object was destroyed (plugin unload/reload).
     *
     * The row is dropped and the host is expected to re-discover providers;
     * the pointer it handed to setProviders() is gone by the time this fires.
     */
    void providerDied();

    /// An action was dispatched (emitted before the provider is called).
    void actionTriggered(const QString& statusId, const QString& actionId);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// One provider and everything the controller built for it.
    struct Entry
    {
        Daqster::IStatusBarProvider* provider = nullptr;
        /// True when the provider is a QObject and its lifetime can be watched.
        bool hasObjectIdentity = false;
        QPointer<QObject> owner;   ///< same object as a QObject (lifetime guard)
        QMetaObject::Connection hook;   ///< `destroyed` hook, disconnected on rebuild
        QPointer<QLabel> label;    ///< the row, owned by the status bar
        QString statusId;
        QString text;              ///< last text we set (empty = hidden)
        QString tooltip;
        /// The provider object died: `provider` is not valid any more and must
        /// never be called again. The row is dropped by the next refresh().
        bool dead = false;
    };

    /// Delete labels + actions and forget every provider.
    void clearEntries();

    /// Reconcile the menu with the providers' current action lists.
    void rebuildMenu();

    /// A cheap fingerprint of the action set, to avoid rebuilding the menu on
    /// every 2 s tick (which would close an open menu under the user's mouse).
    QString menuSignature() const;

    /// Mark the row of a destroyed provider object dead (called from the
    /// `destroyed` signal — it must not touch the provider or re-discover).
    void markProviderDead(const QString& statusId);

    /// Remove dead rows; true when anything was dropped.
    bool dropDeadEntries();

    QPointer<QStatusBar> m_statusBar;
    QPointer<QMenu> m_menu;
    QPointer<QTimer> m_timer;

    QList<Entry> m_entries;
    QString m_menuSignature;
};
