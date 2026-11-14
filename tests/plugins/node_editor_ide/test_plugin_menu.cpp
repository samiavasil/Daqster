#include "test_plugin_menu.h"

#include "PluginMenuController.h"
#include "capabilities/IMenuProvider.h"

#include <QAction>
#include <QMenu>
#include <QMenuBar>
#include <QStringList>

#include <stdexcept>

// REQ-SW-PL-084. The controller is the boundary between the IDE and foreign
// plugin code, so the tests are about the contract: the entry lands where the
// provider asked, the callback carries its id, a second install does not
// duplicate, and a broken provider cannot leave a half-built menu.

namespace {

/// Test provider — a plain object (not a plugin, not even a QObject), which is
/// the point: the capability must not require the host's plugin machinery.
class FakeMenuProvider : public Daqster::IMenuProvider
{
public:
    QVector<Daqster::PluginMenuAction> menuActions() const override
    {
        if (throwOnActions)
            throw std::runtime_error("menuActions() boom");
        return actions;
    }

    void triggerMenuAction(const QString& id) override
    {
        triggered << id;
        if (throwOnTrigger)
            throw std::runtime_error("triggerMenuAction() boom");
    }

    QVector<Daqster::PluginMenuAction> actions;
    QStringList triggered;
    bool throwOnActions = false;
    bool throwOnTrigger = false;
};

/// A host menu bar with the framework's own Tools → Plugins entries in place.
QMenuBar* makeHostMenuBar(QMenu*& toolsMenu, QMenu*& pluginsMenu)
{
    auto* bar = new QMenuBar();
    toolsMenu = bar->addMenu(QStringLiteral("&Tools"));
    pluginsMenu = toolsMenu->addMenu(QStringLiteral("&Plugins"));
    return bar;
}

/// Menu with this exact title, ignoring the mnemonic marker.
QMenu* menuTitled(QObject* parent, const QString& title)
{
    QString wanted = title;
    wanted.remove(QLatin1Char('&'));

    const auto menus = parent->findChildren<QMenu*>();
    for (QMenu* menu : menus) {
        QString plain = menu->title();
        plain.remove(QLatin1Char('&'));
        if (plain == wanted)
            return menu;
    }
    return nullptr;
}

/// Number of menus with this title (a duplicate install would make it 2).
int countMenusTitled(QObject* parent, const QString& title)
{
    QString wanted = title;
    wanted.remove(QLatin1Char('&'));

    int count = 0;
    const auto menus = parent->findChildren<QMenu*>();
    for (QMenu* menu : menus) {
        QString plain = menu->title();
        plain.remove(QLatin1Char('&'));
        if (plain == wanted)
            ++count;
    }
    return count;
}

QAction* actionTitled(QMenu* menu, const QString& text)
{
    if (!menu)
        return nullptr;
    const auto actions = menu->actions();
    for (QAction* action : actions) {
        if (action && action->text() == text)
            return action;
    }
    return nullptr;
}

} // namespace

void TestPluginMenu::actionsAppearUnderRequestedPath()
{
    QMenu* toolsMenu = nullptr;
    QMenu* pluginsMenu = nullptr;
    QMenuBar* bar = makeHostMenuBar(toolsMenu, pluginsMenu);
    PluginMenuController controller(bar);

    FakeMenuProvider provider;
    provider.actions = {
        {QStringLiteral("Tools"), QStringLiteral("AI Setup"),
         QStringLiteral("ai.setup"), true},
        {QStringLiteral("Tools/Plugins"), QStringLiteral("Warm models"),
         QStringLiteral("ai.warm"), true},
    };

    controller.setProviders({&provider});

    QCOMPARE(controller.actionCount(), 2);
    QVERIFY(actionTitled(toolsMenu, QStringLiteral("AI Setup")) != nullptr);
    QVERIFY(actionTitled(pluginsMenu, QStringLiteral("Warm models")) != nullptr);

    // The host's own menus were reused, not replaced.
    QCOMPARE(menuTitled(bar, QStringLiteral("Tools")), toolsMenu);
    QCOMPARE(menuTitled(bar, QStringLiteral("Plugins")), pluginsMenu);
}

void TestPluginMenu::submenuIsCreatedOnlyOnce()
{
    QMenu* toolsMenu = nullptr;
    QMenu* pluginsMenu = nullptr;
    QMenuBar* bar = makeHostMenuBar(toolsMenu, pluginsMenu);
    PluginMenuController controller(bar);

    FakeMenuProvider provider;
    provider.actions = {
        {QStringLiteral("Tools/AI Setup"), QStringLiteral("Install"),
         QStringLiteral("ai.install"), true},
        {QStringLiteral("Tools/AI Setup"), QStringLiteral("Diagnose"),
         QStringLiteral("ai.diagnose"), true},
    };

    controller.setProviders({&provider});

    QCOMPARE(countMenusTitled(bar, QStringLiteral("AI Setup")), 1);
    QMenu* setupMenu = menuTitled(bar, QStringLiteral("AI Setup"));
    QVERIFY(setupMenu != nullptr);
    QVERIFY(actionTitled(setupMenu, QStringLiteral("Install")) != nullptr);
    QVERIFY(actionTitled(setupMenu, QStringLiteral("Diagnose")) != nullptr);
}

void TestPluginMenu::triggerSendsTheProviderId()
{
    QMenu* toolsMenu = nullptr;
    QMenu* pluginsMenu = nullptr;
    QMenuBar* bar = makeHostMenuBar(toolsMenu, pluginsMenu);
    PluginMenuController controller(bar);

    FakeMenuProvider provider;
    provider.actions = {{QStringLiteral("Tools"), QStringLiteral("AI Setup"),
                         QStringLiteral("ai.setup"), true}};

    controller.setProviders({&provider});

    QAction* action = actionTitled(toolsMenu, QStringLiteral("AI Setup"));
    QVERIFY(action != nullptr);
    action->trigger();

    QCOMPARE(provider.triggered, QStringList{QStringLiteral("ai.setup")});
}

void TestPluginMenu::installingTwiceDoesNotDuplicate()
{
    QMenu* toolsMenu = nullptr;
    QMenu* pluginsMenu = nullptr;
    QMenuBar* bar = makeHostMenuBar(toolsMenu, pluginsMenu);
    PluginMenuController controller(bar);

    FakeMenuProvider provider;
    provider.actions = {
        {QStringLiteral("Tools/AI Setup"), QStringLiteral("Install"),
         QStringLiteral("ai.install"), true},
        {QStringLiteral("Tools"), QStringLiteral("AI Setup…"),
         QStringLiteral("ai.setup"), true},
    };

    const int barActionsBefore = bar->actions().size();
    controller.setProviders({&provider});
    const int actionsAfterFirst = controller.actionCount();
    controller.setProviders({&provider});

    QCOMPARE(controller.actionCount(), actionsAfterFirst);
    QCOMPARE(bar->actions().size(), barActionsBefore);
    QCOMPARE(countMenusTitled(bar, QStringLiteral("AI Setup")), 1);
    QCOMPARE(countMenusTitled(bar, QStringLiteral("Tools")), 1);
    QCOMPARE(toolsMenu->actions().size(), 3);   // host "Plugins" + our two
}

void TestPluginMenu::disabledAndInvalidEntries()
{
    QMenu* toolsMenu = nullptr;
    QMenu* pluginsMenu = nullptr;
    QMenuBar* bar = makeHostMenuBar(toolsMenu, pluginsMenu);
    PluginMenuController controller(bar);

    FakeMenuProvider provider;
    provider.actions = {
        {QStringLiteral("Tools"), QStringLiteral("Disabled"),
         QStringLiteral("ai.disabled"), false},
        {QStringLiteral("Tools"), QStringLiteral("No id"), QString(), true},
        {QString(), QStringLiteral("No path"), QStringLiteral("ai.nopath"), true},
        {QStringLiteral("Tools"), QStringLiteral("Valid"),
         QStringLiteral("ai.valid"), true},
    };

    controller.setProviders({&provider});

    // The malformed entries are skipped; the disabled one is installed greyed.
    QCOMPARE(controller.actionCount(), 2);
    QAction* disabled = actionTitled(toolsMenu, QStringLiteral("Disabled"));
    QVERIFY(disabled != nullptr);
    QVERIFY(!disabled->isEnabled());
    QVERIFY(actionTitled(toolsMenu, QStringLiteral("No id")) == nullptr);
    QVERIFY(actionTitled(toolsMenu, QStringLiteral("No path")) == nullptr);
    QVERIFY(actionTitled(toolsMenu, QStringLiteral("Valid")) != nullptr);
}

void TestPluginMenu::providerThatThrowsIsSkipped()
{
    QMenu* toolsMenu = nullptr;
    QMenu* pluginsMenu = nullptr;
    QMenuBar* bar = makeHostMenuBar(toolsMenu, pluginsMenu);
    PluginMenuController controller(bar);

    FakeMenuProvider broken;
    broken.throwOnActions = true;

    FakeMenuProvider working;
    working.actions = {{QStringLiteral("Tools"), QStringLiteral("OK"),
                        QStringLiteral("ai.ok"), true}};

    controller.setProviders({&broken, nullptr, &working});

    QCOMPARE(controller.actionCount(), 1);
    QVERIFY(actionTitled(toolsMenu, QStringLiteral("OK")) != nullptr);

    // A throwing trigger must not escape into the GUI event loop.
    working.throwOnTrigger = true;
    QAction* action = actionTitled(toolsMenu, QStringLiteral("OK"));
    QVERIFY(action != nullptr);
    action->trigger();
    QCOMPARE(working.triggered, QStringList{QStringLiteral("ai.ok")});
}

void TestPluginMenu::noProvidersMeansNoActions()
{
    QMenu* toolsMenu = nullptr;
    QMenu* pluginsMenu = nullptr;
    QMenuBar* bar = makeHostMenuBar(toolsMenu, pluginsMenu);
    PluginMenuController controller(bar);

    controller.setProviders({});

    QCOMPARE(controller.actionCount(), 0);
    QCOMPARE(bar->actions().size(), 1);
    QCOMPARE(toolsMenu->actions().size(), 1);   // the host's "Plugins" submenu
}
