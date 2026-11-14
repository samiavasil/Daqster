#include <QtTest>

#include "QBasePluginObject.h"
#include "QPluginInterface.h"
#include "PluginRegistry.h"
#include "capabilities/IStatusBarProvider.h"

#include <QtAlgorithms>

// ═══════════════════════════════════════════════════════════════════════════
//  IStatusBarProvider discovery (REQ-SW-PL-079)
//
//  The capability itself is trivial; the load-bearing part is that the registry
//  finds it on a loaded plugin object at all. IStatusBarProvider is a plain
//  (non-QObject) interface with no IID, so it cannot be found through
//  qt_metacast — it is discovered with dynamic_cast in
//  PluginRegistry::statusBarProviders(). These tests register a fake plugin
//  interface (no .so, no disk) and check what the registry reports.
// ═══════════════════════════════════════════════════════════════════════════

namespace {

/// Plugin object that reports a status row — the thing a real plugin would do
/// (e.g. a plugin reporting its open devices and the memory they use).
class FakeStatusBarProvider : public Daqster::QBasePluginObject,
                              public Daqster::IStatusBarProvider
{
    Q_OBJECT
public:
    explicit FakeStatusBarProvider(QObject* parent = nullptr)
        : Daqster::QBasePluginObject(parent)
    {
    }

    bool Initialize() override
    {
        m_initialized = true;
        return true;
    }

    QString statusId() const override { return QStringLiteral("fake.status"); }
    QString statusText() const override { return QStringLiteral("fake: 1 warm"); }
    QString statusTooltip() const override { return QStringLiteral("fake: 1 warm model"); }
    void triggerStatusAction(const QString&) override { ++m_triggered; }

    bool initialized() const { return m_initialized; }
    int triggered() const { return m_triggered; }

protected:
    void DeInitialize() override {}

private:
    bool m_initialized = false;
    int m_triggered = 0;
};

/// Plugin object with no status-bar capability at all (the common case).
class FakePlainPluginObject : public Daqster::QBasePluginObject
{
    Q_OBJECT
public:
    explicit FakePlainPluginObject(QObject* parent = nullptr)
        : Daqster::QBasePluginObject(parent)
    {
    }

    bool Initialize() override { return true; }

protected:
    void DeInitialize() override {}
};

/// Plugin interface (the factory) — what QPluginManager would hold. The test
/// picks which object each "plugin" creates.
class FakePluginInterface : public Daqster::QPluginInterface
{
    Q_OBJECT
public:
    enum class Kind { StatusBar, NoCapability };

    explicit FakePluginInterface(Kind kind, QObject* parent = nullptr)
        : Daqster::QPluginInterface(parent)
        , m_kind(kind)
    {
    }

    int created() const { return m_created; }

protected:
    Daqster::QBasePluginObject* CreatePluginInternal(QObject* parent) override
    {
        ++m_created;
        if (m_kind == Kind::StatusBar)
            return new FakeStatusBarProvider(parent);
        return new FakePlainPluginObject(parent);
    }

private:
    Kind m_kind;
    int m_created = 0;
};

} // namespace

class TestStatusBarDiscovery : public QObject
{
    Q_OBJECT

private slots:
    void statusProviderIsDiscovered();
    void pluginWithoutTheCapabilityIsNotListed();
    void disabledPluginIsNotListed();
};

void TestStatusBarDiscovery::statusProviderIsDiscovered()
{
    FakePluginInterface iface(FakePluginInterface::Kind::StatusBar);
    Daqster::PluginRegistry registry;
    registry.registerPlugin(QStringLiteral("fake-status"), &iface);

    const QList<Daqster::IStatusBarProvider*> providers = registry.statusBarProviders();

    QCOMPARE(providers.size(), 1);
    QVERIFY(providers.first() != nullptr);
    QCOMPARE(providers.first()->statusId(), QStringLiteral("fake.status"));

    // Discovery initializes the plugin object: the row reports the live state of
    // what Initialize() built (a model runtime, in the real plugin).
    auto* provider = dynamic_cast<FakeStatusBarProvider*>(providers.first());
    QVERIFY(provider != nullptr);
    QVERIFY2(provider->initialized(), "statusBarProviders() must hand out initialized objects");

    // Same bookkeeping as every other capability probe.
    QCOMPARE(provider->property("_daqster_hash").toString(), QStringLiteral("fake-status"));

    // Probing again reuses the instance instead of creating a second plugin
    // object — two rows for one plugin would be a bug.
    QCOMPARE(registry.statusBarProviders().size(), 1);
    QCOMPARE(iface.created(), 1);

    provider->triggerStatusAction(QString());   // must be a benign no-op in the fake
    QCOMPARE(provider->triggered(), 1);

    // The registry hands out raw pointers it does not own (see
    // capabilityInstances); the test cleans up after itself.
    qDeleteAll(registry.allPluginInstances());
}

void TestStatusBarDiscovery::pluginWithoutTheCapabilityIsNotListed()
{
    FakePluginInterface iface(FakePluginInterface::Kind::NoCapability);
    Daqster::PluginRegistry registry;
    registry.registerPlugin(QStringLiteral("fake-plain"), &iface);

    QVERIFY(registry.statusBarProviders().isEmpty());

    qDeleteAll(registry.allPluginInstances());
}

void TestStatusBarDiscovery::disabledPluginIsNotListed()
{
    FakePluginInterface iface(FakePluginInterface::Kind::StatusBar);
    Daqster::PluginRegistry registry;
    registry.registerPlugin(QStringLiteral("fake-disabled"), &iface);
    registry.enablePlugin(QStringLiteral("fake-disabled"), false);

    QVERIFY(registry.statusBarProviders().isEmpty());
}

QTEST_GUILESS_MAIN(TestStatusBarDiscovery)
#include "test_status_bar_discovery.moc"
