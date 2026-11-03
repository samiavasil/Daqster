#include "StatusBarController.h"

#include "capabilities/IStatusBarProvider.h"

#include <QAction>
#include <QLabel>
#include <QLoggingCategory>
#include <QMenu>
#include <QMouseEvent>
#include <QPoint>
#include <QStatusBar>
#include <QStringList>
#include <QTimer>
#include <QToolTip>

#include <algorithm>
#include <exception>

// This class is compiled into the node_editor_ide tests as well as into the
// plugin, and the test binary deliberately does not link FrameworkCore — so the
// category is defined here instead of coming from LogCategories (same reasoning
// as Threading/ThreadingLog.cpp).
Q_LOGGING_CATEGORY(lcStatusBar, "daqster.statusbar")

namespace {

/// Read text and tooltip from a foreign plugin without letting it take the IDE
/// down. A provider that throws is a provider bug; the row is hidden and the
/// reason is logged.
void readStatus(Daqster::IStatusBarProvider* provider,
                const QString& statusId,
                QString* text,
                QString* tooltip)
{
    try {
        *text = provider->statusText();
        *tooltip = provider->statusTooltip();
        return;
    } catch (const std::exception& error) {
        qCWarning(lcStatusBar) << "provider" << statusId
                               << "threw from statusText()/statusTooltip():" << error.what()
                               << "— row hidden";
    } catch (...) {
        qCWarning(lcStatusBar) << "provider" << statusId
                               << "threw a non-std exception from statusText()/statusTooltip()"
                                  " — row hidden";
    }

    text->clear();
    tooltip->clear();
}

/// statusActions() of a foreign plugin, empty on failure.
QList<Daqster::IStatusBarProvider::Action>
readActions(Daqster::IStatusBarProvider* provider, const QString& statusId)
{
    try {
        return provider->statusActions();
    } catch (const std::exception& error) {
        qCWarning(lcStatusBar) << "provider" << statusId
                               << "threw from statusActions():" << error.what();
    } catch (...) {
        qCWarning(lcStatusBar) << "provider" << statusId
                               << "threw a non-std exception from statusActions()";
    }
    return {};
}

/// statusOrder() of a foreign plugin, default order on failure.
int readOrder(Daqster::IStatusBarProvider* provider, const QString& statusId)
{
    try {
        return provider->statusOrder();
    } catch (...) {
        qCWarning(lcStatusBar) << "provider" << statusId
                               << "threw from statusOrder() — using the default order";
    }
    return 100;
}

} // namespace

StatusBarController::StatusBarController(QStatusBar* statusBar, QMenu* actionsMenu,
                                         QObject* parent)
    : QObject(parent)
    , m_statusBar(statusBar)
    , m_menu(actionsMenu)
    , m_timer(new QTimer(this))
{
    m_timer->setInterval(kDefaultRefreshMs);
    connect(m_timer, &QTimer::timeout, this, &StatusBarController::refresh);
}

StatusBarController::~StatusBarController()
{
    clearEntries();
}

void StatusBarController::setProviders(
    const QList<Daqster::IStatusBarProvider*>& providers)
{
    clearEntries();

    // Collect first: the sort has to happen before any label exists, because
    // the status bar shows permanent widgets in insertion order.
    struct Candidate
    {
        Daqster::IStatusBarProvider* provider = nullptr;
        QString statusId;
        int order = 100;
    };

    QList<Candidate> candidates;
    for (Daqster::IStatusBarProvider* provider : providers) {
        if (!provider) {
            qCWarning(lcStatusBar) << "ignoring a null IStatusBarProvider";
            continue;
        }

        QString statusId;
        try {
            statusId = provider->statusId();
        } catch (const std::exception& error) {
            qCWarning(lcStatusBar) << "statusId() threw:" << error.what()
                                   << "— provider skipped";
            continue;
        } catch (...) {
            qCWarning(lcStatusBar) << "statusId() threw a non-std exception"
                                   << "— provider skipped";
            continue;
        }

        if (statusId.isEmpty()) {
            qCWarning(lcStatusBar) << "provider with an empty statusId() skipped";
            continue;
        }

        const bool duplicate = std::any_of(
            candidates.cbegin(), candidates.cend(),
            [&statusId](const Candidate& existing) { return existing.statusId == statusId; });
        if (duplicate) {
            qCWarning(lcStatusBar) << "two providers claim statusId" << statusId
                                   << "— keeping the first one";
            continue;
        }

        candidates.append(
            Candidate{provider, statusId, readOrder(provider, statusId)});
    }

    std::stable_sort(candidates.begin(), candidates.end(),
                     [](const Candidate& left, const Candidate& right) {
                         if (left.order != right.order)
                             return left.order < right.order;
                         return left.statusId < right.statusId;
                     });

    for (const Candidate& candidate : candidates) {
        Entry entry;
        entry.provider = candidate.provider;
        entry.statusId = candidate.statusId;
        entry.owner = dynamic_cast<QObject*>(candidate.provider);
        entry.hasObjectIdentity = !entry.owner.isNull();

        if (entry.owner) {
            // Plugin objects die on unload/reload; the row must go with them.
            // Direct, but it only flips a flag on OUR entry: re-discovering
            // plugins from inside another plugin's destructor would hand back
            // the very object that is being destroyed. The row is dropped (and
            // `providerDied` emitted) by the next refresh, after the plugin
            // interface has cleaned its instance pool.
            const QString statusId = candidate.statusId;
            entry.hook = connect(entry.owner, &QObject::destroyed, this,
                                 [this, statusId]() { markProviderDead(statusId); });
        }

        if (m_statusBar) {
            auto* label = new QLabel(m_statusBar);
            label->setObjectName(QStringLiteral("statusBarProvider_") + candidate.statusId);
            label->setVisible(false);   // until the first refresh says otherwise
            label->installEventFilter(this);
            m_statusBar->addPermanentWidget(label, 0);
            entry.label = label;
        }

        m_entries.append(entry);
    }

    rebuildMenu();
    refresh();
}

void StatusBarController::startAutoRefresh(int intervalMs)
{
    if (!m_timer)
        return;

    if (intervalMs <= 0) {
        m_timer->stop();
        return;
    }

    m_timer->setInterval(intervalMs);
    m_timer->start();
}

QLabel* StatusBarController::labelFor(const QString& statusId) const
{
    for (const Entry& entry : m_entries) {
        if (entry.statusId == statusId)
            return entry.label;
    }
    return nullptr;
}

int StatusBarController::labelCount() const
{
    int count = 0;
    for (const Entry& entry : m_entries) {
        if (!entry.label.isNull())
            ++count;
    }
    return count;
}

QList<QAction*> StatusBarController::menuActions() const
{
    if (!m_menu)
        return {};
    return m_menu->actions();
}

QStringList StatusBarController::statusIds() const
{
    QStringList ids;
    ids.reserve(m_entries.size());
    for (const Entry& entry : m_entries)
        ids << entry.statusId;
    return ids;
}

void StatusBarController::refresh()
{
    if (!m_statusBar && !m_menu)
        return;   // the host window is gone

    if (dropDeadEntries())
        rebuildMenu();

    for (Entry& entry : m_entries) {
        if (!entry.provider || entry.dead)
            continue;

        // A provider whose QObject is gone (or is being destroyed right now)
        // must not be called: the `destroyed` hook flips `dead`, and until this
        // refresh drops the row the pointer is not valid any more.
        if (entry.hasObjectIdentity && entry.owner.isNull())
            continue;

        QString text;
        QString tooltip;
        readStatus(entry.provider, entry.statusId, &text, &tooltip);

        entry.text = text;
        entry.tooltip = tooltip;

        if (!entry.label)
            continue;

        // Empty text is a normal state ("nothing to report"), not an error:
        // the row hides instead of showing an empty hole in the status bar.
        entry.label->setText(text);
        entry.label->setToolTip(tooltip);
        entry.label->setVisible(!text.isEmpty());
    }

    if (m_menu && menuSignature() != m_menuSignature)
        rebuildMenu();
}

void StatusBarController::triggerAction(const QString& statusId,
                                        const QString& actionId)
{
    for (const Entry& entry : m_entries) {
        if (entry.statusId != statusId || !entry.provider || entry.dead)
            continue;
        if (entry.hasObjectIdentity && entry.owner.isNull())
            continue;

        emit actionTriggered(statusId, actionId);

        try {
            entry.provider->triggerStatusAction(actionId);
        } catch (const std::exception& error) {
            // The contract says "no-op on unknown id, never throw"; a plugin
            // that breaks it must not take the IDE down.
            qCWarning(lcStatusBar) << "provider" << statusId << "action" << actionId
                                   << "threw:" << error.what();
        } catch (...) {
            qCWarning(lcStatusBar) << "provider" << statusId << "action" << actionId
                                   << "threw a non-std exception";
        }

        // The action may have freed resources — show the result immediately
        // instead of waiting up to kDefaultRefreshMs for the next tick.
        refresh();
        return;
    }

    qCDebug(lcStatusBar) << "no provider for status action" << statusId << actionId;
}

bool StatusBarController::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        if (auto* label = qobject_cast<QLabel*>(watched)) {
            const QString tooltip = label->toolTip();
            if (!tooltip.isEmpty()) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
                const QPoint global = static_cast<QMouseEvent*>(event)->globalPosition().toPoint();
#else
                const QPoint global = static_cast<QMouseEvent*>(event)->globalPos();
#endif
                QToolTip::showText(global, tooltip, label);
            }
        }
    }

    // Never swallow: the label keeps whatever behaviour Qt gives it.
    return QObject::eventFilter(watched, event);
}

void StatusBarController::clearEntries()
{
    for (Entry& entry : m_entries) {
        if (!entry.label.isNull())
            delete entry.label.data();   // also unregisters from the status bar
        if (entry.hook)
            disconnect(entry.hook);
    }
    m_entries.clear();

    if (m_menu)
        m_menu->clear();

    m_menuSignature.clear();
}

void StatusBarController::rebuildMenu()
{
    if (!m_menu)
        return;

    m_menu->clear();

    bool firstProvider = true;
    bool anyAction = false;

    for (const Entry& entry : m_entries) {
        if (!entry.provider || entry.dead)
            continue;
        if (entry.hasObjectIdentity && entry.owner.isNull())
            continue;

        const QList<Daqster::IStatusBarProvider::Action> actions =
            readActions(entry.provider, entry.statusId);
        if (actions.isEmpty())
            continue;

        if (!firstProvider)
            m_menu->addSeparator();
        firstProvider = false;

        for (const Daqster::IStatusBarProvider::Action& action : actions) {
            if (action.id.isEmpty()) {
                qCWarning(lcStatusBar) << "provider" << entry.statusId
                                       << "offered an action without an id — skipped";
                continue;
            }

            const QString label = action.text.isEmpty() ? action.id : action.text;
            QAction* menuAction = m_menu->addAction(label);
            if (!action.tooltip.isEmpty()) {
                menuAction->setToolTip(action.tooltip);
                menuAction->setStatusTip(action.tooltip);
            }

            const QString statusId = entry.statusId;
            const QString actionId = action.id;
            connect(menuAction, &QAction::triggered, this,
                    [this, statusId, actionId]() { triggerAction(statusId, actionId); });
            anyAction = true;
        }
    }

    // An empty plugin menu is noise: hide its entry until a provider offers an
    // action. (The submenu itself stays in the menu bar.)
    if (QAction* menuEntry = m_menu->menuAction())
        menuEntry->setVisible(anyAction);

    m_menuSignature = menuSignature();
}

QString StatusBarController::menuSignature() const
{
    QStringList parts;

    for (const Entry& entry : m_entries) {
        if (!entry.provider || entry.dead)
            continue;
        if (entry.hasObjectIdentity && entry.owner.isNull())
            continue;

        QStringList actionIds;
        const QList<Daqster::IStatusBarProvider::Action> actions =
            readActions(entry.provider, entry.statusId);
        for (const Daqster::IStatusBarProvider::Action& action : actions)
            actionIds << action.id;

        parts << entry.statusId + QLatin1Char(':') + actionIds.join(QLatin1Char(','));
    }

    return parts.join(QLatin1Char('|'));
}

void StatusBarController::markProviderDead(const QString& statusId)
{
    for (Entry& entry : m_entries) {
        if (entry.statusId == statusId)
            entry.dead = true;
    }
}

bool StatusBarController::dropDeadEntries()
{
    bool removed = false;

    for (int index = m_entries.size() - 1; index >= 0; --index) {
        Entry& entry = m_entries[index];

        const bool gone = entry.dead
            || (entry.hasObjectIdentity && entry.owner.isNull());
        if (!gone)
            continue;

        qCDebug(lcStatusBar) << "IStatusBarProvider" << entry.statusId
                             << "is gone — row dropped";

        if (!entry.label.isNull())
            delete entry.label.data();
        if (entry.hook)
            disconnect(entry.hook);
        m_entries.removeAt(index);
        m_menuSignature.clear();   // the menu no longer matches
        removed = true;
    }

    if (removed) {
        // Deferred: the host re-discovers providers in this slot, which calls
        // setProviders() and rebuilds m_entries — doing that from inside
        // refresh()'s own iteration would invalidate it.
        QMetaObject::invokeMethod(this, [this]() { emit providerDied(); },
                                  Qt::QueuedConnection);
    }

    return removed;
}
