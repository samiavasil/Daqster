#include "PluginMenuController.h"

#include "capabilities/IMenuProvider.h"

#include <QAction>
#include <QLoggingCategory>
#include <QMenu>
#include <QMenuBar>

#include <exception>

Q_LOGGING_CATEGORY(lcPluginMenu, "daqster.pluginmenu")

namespace {

/// Title with the mnemonic marker removed, for comparing "&Tools" with "Tools".
QString plainTitle(const QString& title)
{
    QString plain = title;
    plain.remove(QLatin1Char('&'));
    return plain.trimmed();
}

/**
 * @brief Resolves a `/`-separated menu path into segments.
 *
 * Empty segments ("Tools//Plugins", a leading or trailing "/") are dropped so
 * a sloppy path in a plugin does not create an unnamed menu.
 *
 * @param path Raw menuPath from PluginMenuAction
 * @return Non-empty segments, in menu order
 */
QStringList splitPath(const QString& path)
{
    QStringList segments;

    const QStringList raw = path.split(QLatin1Char('/'));
    for (const QString& segment : raw) {
        const QString trimmed = segment.trimmed();
        if (!trimmed.isEmpty())
            segments << trimmed;
    }

    return segments;
}

} // namespace

PluginMenuController::PluginMenuController(QMenuBar* menuBar, QObject* parent)
    : QObject(parent)
    , m_menuBar(menuBar)
{
}

PluginMenuController::~PluginMenuController()
{
    uninstall();
}

QMenu* PluginMenuController::findChildMenu(QMenu* parent, const QString& title)
{
    if (!parent)
        return nullptr;

    const QString wanted = plainTitle(title);
    const QList<QAction*> actions = parent->actions();
    for (QAction* action : actions) {
        QMenu* menu = action ? action->menu() : nullptr;
        if (menu && plainTitle(menu->title()) == wanted)
            return menu;
    }

    return nullptr;
}

QMenu* PluginMenuController::resolveMenu(const QStringList& segments, bool create)
{
    if (!m_menuBar || segments.isEmpty())
        return nullptr;

    // Top level: a menu bar holds actions, each wrapping its QMenu.
    QMenu* current = nullptr;
    const QList<QAction*> barActions = m_menuBar->actions();
    for (QAction* action : barActions) {
        QMenu* menu = action ? action->menu() : nullptr;
        if (menu && plainTitle(menu->title()) == plainTitle(segments.first())) {
            current = menu;
            break;
        }
    }

    if (!current) {
        if (!create)
            return nullptr;
        current = m_menuBar->addMenu(segments.first());
        m_createdMenus << current;
    }

    for (int index = 1; index < segments.size(); ++index) {
        QMenu* child = findChildMenu(current, segments[index]);
        if (!child) {
            if (!create)
                return nullptr;
            child = current->addMenu(segments[index]);
            m_createdMenus << child;
        }
        current = child;
    }

    return current;
}

void PluginMenuController::uninstall()
{
    // Actions first: a QAction owns its slot in the menu, so deleting it
    // removes the entry. The menus this controller created are emptied by that
    // and then removed below (parents were created first, so reverse order
    // deletes children before their parent).
    for (const QPointer<QAction>& action : m_actions) {
        if (action)
            delete action.data();
    }
    m_actions.clear();

    for (int index = m_createdMenus.size() - 1; index >= 0; --index) {
        // Deleted immediately (not deleteLater): the next setProviders() call in
        // the same event-loop turn must not find this copy and reuse it, or the
        // menu bar ends up with two menus of the same title.
        if (m_createdMenus[index])
            delete m_createdMenus[index].data();
    }
    m_createdMenus.clear();
}

int PluginMenuController::actionCount() const
{
    int count = 0;
    for (const QPointer<QAction>& action : m_actions) {
        if (action)
            ++count;
    }
    return count;
}

QList<QAction*> PluginMenuController::installedActions() const
{
    QList<QAction*> alive;
    for (const QPointer<QAction>& action : m_actions) {
        if (action)
            alive << action.data();
    }
    return alive;
}

void PluginMenuController::setProviders(
    const QList<Daqster::IMenuProvider*>& providers)
{
    m_providers = providers;
    uninstall();

    if (!m_menuBar)
        return;

    for (Daqster::IMenuProvider* provider : m_providers) {
        if (!provider)
            continue;

        QVector<Daqster::PluginMenuAction> entries;
        try {
            entries = provider->menuActions();
        } catch (const std::exception& error) {
            qCWarning(lcPluginMenu) << "menuActions() threw:" << error.what()
                                    << "— provider skipped";
            continue;
        } catch (...) {
            qCWarning(lcPluginMenu) << "menuActions() threw an unknown exception"
                                    << "— provider skipped";
            continue;
        }

        for (const Daqster::PluginMenuAction& entry : entries) {
            if (entry.id.isEmpty()) {
                qCWarning(lcPluginMenu) << "provider offered an action without an id"
                                        << "— skipped";
                continue;
            }
            if (entry.menuPath.trimmed().isEmpty()) {
                qCWarning(lcPluginMenu) << "action" << entry.id
                                        << "has no menuPath — skipped";
                continue;
            }

            const QStringList segments = splitPath(entry.menuPath);
            if (segments.isEmpty()) {
                qCWarning(lcPluginMenu) << "action" << entry.id
                                        << "has an empty menuPath — skipped";
                continue;
            }

            QMenu* menu = resolveMenu(segments, true);
            if (!menu) {
                qCWarning(lcPluginMenu) << "cannot resolve menu"
                                        << entry.menuPath << "for action" << entry.id;
                continue;
            }

            const QString label = entry.text.isEmpty() ? entry.id : entry.text;
            QAction* action = menu->addAction(label);
            action->setEnabled(entry.enabled);

            // The provider may be a QObject that dies while the entry is on
            // screen (plugin unload). Guard with QPointer so the slot becomes a
            // no-op instead of calling into freed memory.
            QPointer<QObject> owner;
            const bool hasOwner = dynamic_cast<QObject*>(provider) != nullptr;
            if (hasOwner)
                owner = dynamic_cast<QObject*>(provider);

            const QString actionId = entry.id;
            connect(action, &QAction::triggered, this,
                    [provider, owner, hasOwner, actionId]() {
                        if (hasOwner && owner.isNull()) {
                            qCWarning(lcPluginMenu) << "provider is gone — action"
                                                    << actionId << "ignored";
                            return;
                        }
                        try {
                            provider->triggerMenuAction(actionId);
                        } catch (const std::exception& error) {
                            qCWarning(lcPluginMenu) << "triggerMenuAction(" << actionId
                                                    << ") threw:" << error.what();
                        } catch (...) {
                            qCWarning(lcPluginMenu) << "triggerMenuAction("
                                                    << actionId
                                                    << ") threw an unknown exception";
                        }
                    });

            m_actions << action;
        }
    }

    if (!m_actions.isEmpty()) {
        qCDebug(lcPluginMenu) << "installed" << m_actions.size()
                              << "plugin menu action(s) from" << m_providers.size()
                              << "provider(s)";
    }
}
