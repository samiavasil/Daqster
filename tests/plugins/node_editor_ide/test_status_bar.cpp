#include "test_status_bar.h"

#include "StatusBarController.h"
#include "capabilities/IStatusBarProvider.h"

#include <QAction>
#include <QLabel>
#include <QMenu>
#include <QSignalSpy>
#include <QStatusBar>
#include <QStringList>

#include <stdexcept>
#include <utility>

// REQ-SW-PL-079. The controller is the boundary between the IDE and foreign
// plugin code, so the tests are mostly about what happens when the plugin
// side misbehaves: empty text, a null entry, a provider that throws, a provider
// object that is deleted while its row is on screen.

namespace {

/// Test provider. A plain QObject (not a plugin object) with the same lifetime
/// semantics, so the tests can destroy it and watch the row disappear.
class FakeProvider : public QObject, public Daqster::IStatusBarProvider
{
    Q_OBJECT
public:
    explicit FakeProvider(QString id, QObject* parent = nullptr)
        : QObject(parent)
        , m_id(std::move(id))
    {
    }

    QString statusId() const override { return m_id; }

    QString statusText() const override
    {
        if (throwOnText)
            throw std::runtime_error("statusText() boom");
        return text;
    }

    QString statusTooltip() const override { return tooltip; }

    int statusOrder() const override { return order; }

    QList<Action> statusActions() const override { return actions; }

    void triggerStatusAction(const QString& id) override
    {
        triggered << id;
        if (throwOnTrigger)
            throw std::runtime_error("triggerStatusAction() boom");
    }

    QString text;
    QString tooltip;
    int order = 100;
    bool throwOnText = false;
    bool throwOnTrigger = false;
    QList<Action> actions;
    QStringList triggered;

private:
    QString m_id;
};

/// Providers actually visible in the status bar, left to right (skips the
/// status bar's own message label).
QStringList statusBarRowOrder(const QStatusBar& bar)
{
    QStringList order;
    const auto labels = bar.findChildren<QLabel*>();
    for (QLabel* label : labels) {
        if (label->objectName().startsWith(QStringLiteral("statusBarProvider_")))
            order << label->objectName();
    }
    return order;
}

Daqster::IStatusBarProvider::Action makeAction(const QString& id,
                                               const QString& text,
                                               const QString& tooltip = QString())
{
    Daqster::IStatusBarProvider::Action action;
    action.id = id;
    action.text = text;
    action.tooltip = tooltip;
    return action;
}

} // namespace

void TestStatusBar::statusBarShowsProviderText()
{
    QStatusBar bar;
    QMenu menu;
    StatusBarController controller(&bar, &menu);

    FakeProvider provider(QStringLiteral("test.row"));
    provider.text = QStringLiteral("Models: 1 warm, 4.2 GB");
    provider.tooltip = QStringLiteral("qwen3-8b: 4.2 GB");

    controller.setProviders(QList<Daqster::IStatusBarProvider*>{&provider});

    QLabel* label = controller.labelFor(QStringLiteral("test.row"));
    QVERIFY2(label != nullptr, "every provider must get a row");
    QCOMPARE(label->text(), provider.text);
    QCOMPARE(label->toolTip(), provider.tooltip);
    QVERIFY2(!label->isHidden(), "a provider with text must be visible");
    QCOMPARE(controller.labelCount(), 1);

    // A typing/transient status message must not hide the rows.
    bar.showMessage(QStringLiteral("Loading scene..."));
    QVERIFY(!label->isHidden());
}

void TestStatusBar::statusActionIsTriggered()
{
    QStatusBar bar;
    QMenu menu;
    StatusBarController controller(&bar, &menu);

    FakeProvider provider(QStringLiteral("test.actions"));
    provider.actions = {makeAction(QStringLiteral("free"), QStringLiteral("Free warm models"),
                                   QStringLiteral("Unload every model now"))};

    QSignalSpy triggeredSpy(&controller, &StatusBarController::actionTriggered);

    controller.setProviders(QList<Daqster::IStatusBarProvider*>{&provider});

    QAction* target = nullptr;
    for (QAction* action : controller.menuActions()) {
        if (action->text() == QStringLiteral("Free warm models"))
            target = action;
    }
    QVERIFY2(target != nullptr, "the provider action must be listed in the menu");

    target->trigger();

    QCOMPARE(provider.triggered, QStringList{QStringLiteral("free")});
    QCOMPARE(triggeredSpy.count(), 1);
    QCOMPARE(triggeredSpy.at(0).at(0).toString(), QStringLiteral("test.actions"));
    QCOMPARE(triggeredSpy.at(0).at(1).toString(), QStringLiteral("free"));
}

void TestStatusBar::emptyTextHidesLabel()
{
    QStatusBar bar;
    QMenu menu;
    StatusBarController controller(&bar, &menu);

    FakeProvider provider(QStringLiteral("test.empty"));
    controller.setProviders(QList<Daqster::IStatusBarProvider*>{&provider});

    QLabel* label = controller.labelFor(QStringLiteral("test.empty"));
    QVERIFY(label != nullptr);
    QVERIFY2(label->isHidden(), "empty statusText() must hide the row, not show an empty hole");

    provider.text = QStringLiteral("1 warm model");
    controller.refresh();

    QVERIFY2(!label->isHidden(), "the row comes back as soon as there is something to say");
    QCOMPARE(label->text(), QStringLiteral("1 warm model"));

    provider.text.clear();
    controller.refresh();
    QVERIFY(label->isHidden());
}

void TestStatusBar::missingProviderDoesNotCrashIde()
{
    QStatusBar bar;
    QMenu menu;
    StatusBarController controller(&bar, &menu);

    auto* provider = new FakeProvider(QStringLiteral("test.broken"));
    provider->throwOnText = true;
    provider->throwOnTrigger = true;
    provider->actions = {makeAction(QStringLiteral("nuke"), QStringLiteral("Nuke"))};

    // A null entry and a provider that throws must both be survivable.
    controller.setProviders(QList<Daqster::IStatusBarProvider*>{nullptr, provider});
    QCOMPARE(controller.labelCount(), 1);
    QVERIFY2(controller.labelFor(QStringLiteral("test.broken"))->isHidden(),
             "a throwing statusText() hides the row instead of taking the IDE down");

    controller.triggerAction(QStringLiteral("test.broken"), QStringLiteral("nuke"));
    QCOMPARE(provider->triggered, QStringList{QStringLiteral("nuke")});

    // Plugin unload: the row must not keep a dangling provider pointer.
    delete provider;
    controller.refresh();   // must be a no-op for the dead provider, not a crash

    QCOMPARE(controller.labelCount(), 0);
    QVERIFY(controller.labelFor(QStringLiteral("test.broken")) == nullptr);
    QVERIFY(controller.menuActions().isEmpty());

    // Triggering the action of a provider that is gone is a no-op.
    controller.triggerAction(QStringLiteral("test.broken"), QStringLiteral("nuke"));
    QVERIFY(controller.menuActions().isEmpty());
}

void TestStatusBar::providersAreOrderedByStatusOrder()
{
    QStatusBar bar;
    QMenu menu;
    StatusBarController controller(&bar, &menu);

    FakeProvider late(QStringLiteral("test.late"));
    late.order = 500;
    late.text = QStringLiteral("late");

    FakeProvider early(QStringLiteral("test.early"));
    early.order = 10;
    early.text = QStringLiteral("early");

    FakeProvider middle(QStringLiteral("test.middle"));
    middle.order = 100;
    middle.text = QStringLiteral("middle");

    controller.setProviders(
        QList<Daqster::IStatusBarProvider*>{&late, &middle, &early});

    const QStringList expected{
        QStringLiteral("test.early"), QStringLiteral("test.middle"),
        QStringLiteral("test.late")};
    QCOMPARE(controller.statusIds(), expected);

    const QStringList expectedRows{
        QStringLiteral("statusBarProvider_test.early"),
        QStringLiteral("statusBarProvider_test.middle"),
        QStringLiteral("statusBarProvider_test.late")};
    QCOMPARE(statusBarRowOrder(bar), expectedRows);
}

void TestStatusBar::duplicateStatusIdKeepsFirstRow()
{
    QStatusBar bar;
    QMenu menu;
    StatusBarController controller(&bar, &menu);

    FakeProvider first(QStringLiteral("test.dup"));
    first.text = QStringLiteral("first");
    FakeProvider second(QStringLiteral("test.dup"));
    second.text = QStringLiteral("second");

    controller.setProviders(QList<Daqster::IStatusBarProvider*>{&first, &second});

    QCOMPARE(controller.labelCount(), 1);
    QCOMPARE(controller.labelFor(QStringLiteral("test.dup"))->text(),
             QStringLiteral("first"));
}

void TestStatusBar::menuIsHiddenWithoutActions()
{
    QStatusBar bar;
    QMenu menu;
    StatusBarController controller(&bar, &menu);

    FakeProvider provider(QStringLiteral("test.noactions"));
    provider.text = QStringLiteral("nothing to do");

    controller.setProviders(QList<Daqster::IStatusBarProvider*>{&provider});
    QVERIFY(controller.menuActions().isEmpty());
    QVERIFY2(!menu.menuAction()->isVisible(),
             "an empty plugin menu is hidden, not left as a dead entry");

    provider.actions = {makeAction(QStringLiteral("do"), QStringLiteral("Do it"))};
    controller.refresh();   // the action set is dynamic

    QCOMPARE(controller.menuActions().size(), 1);
    QVERIFY(menu.menuAction()->isVisible());
}

#include "test_status_bar.moc"
