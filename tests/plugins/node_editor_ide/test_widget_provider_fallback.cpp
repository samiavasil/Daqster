#include <QtTest>

#include <QLabel>
#include <QWidget>

#include <memory>

#include "NodeSupport/types/ChatGraphModel.h" // code under test (compiled in directly)
#include "capabilities/IWidgetProvider.h"
#include "test_widget_provider_fallback.h"

#include <QtNodes/NodeData>
#include <QtNodes/NodeDelegateModel>
#include <QtNodes/NodeDelegateModelRegistry>

// REQ-SW-PL-051 regression test. See test_widget_provider_fallback.h for why.

namespace {

/// Minimal leaf model. embeddedWidget() is either nullptr (the split model
/// contract) or a lazily built QLabel("E") that the model owns — used to prove
/// the embeddedWidget() fallback still works when no provider serves the node.
class DummyModel : public QtNodes::NodeDelegateModel
{
public:
    explicit DummyModel(QString nodeName, bool withEmbeddedWidget = false)
        : m_name(std::move(nodeName))
        , m_withEmbedded(withEmbeddedWidget)
    {
    }

    QString name() const override { return m_name; }
    QString caption() const override { return m_name; }

    unsigned int nPorts(QtNodes::PortType) const override { return 0; }
    QtNodes::NodeDataType dataType(QtNodes::PortType, QtNodes::PortIndex) const override
    {
        return {};
    }
    void setInData(std::shared_ptr<QtNodes::NodeData>, QtNodes::PortIndex) override {}
    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex) override { return nullptr; }

    QWidget* embeddedWidget() override
    {
        if (!m_withEmbedded)
            return nullptr;
        if (!m_embedded)
            m_embedded = std::make_unique<QLabel>(QStringLiteral("E"));
        return m_embedded.get();
    }

private:
    QString m_name;
    bool m_withEmbedded;
    std::unique_ptr<QWidget> m_embedded;
};

/// Test provider. Returns a QLabel tagged with `tag` for exactly `servesName`;
/// nullptr for every other model. `servesName` empty means "never serves".
/// Created widgets are recorded in `created` so the test can delete them.
class TaggedProvider : public Daqster::IWidgetProvider
{
public:
    TaggedProvider(QString servesName, QString tag, QList<QWidget*>* created)
        : m_servesName(std::move(servesName))
        , m_tag(std::move(tag))
        , m_created(created)
    {
    }

    QWidget* createWidget(QtNodes::NodeDelegateModel* model) const override
    {
        if (!model || m_servesName.isEmpty() || model->name() != m_servesName)
            return nullptr;
        auto* label = new QLabel(m_tag);
        if (m_created)
            m_created->append(label);
        return label;
    }

private:
    QString m_servesName;
    QString m_tag;
    QList<QWidget*>* m_created;
};

std::shared_ptr<QtNodes::NodeDelegateModelRegistry> registryWith(QString modelName,
                                                                bool withEmbeddedWidget)
{
    auto registry = std::make_shared<QtNodes::NodeDelegateModelRegistry>();
    registry->registerModel<DummyModel>([modelName, withEmbeddedWidget]() {
        return std::make_unique<DummyModel>(modelName, withEmbeddedWidget);
    });
    return registry;
}

QString labelText(QWidget* widget)
{
    if (auto* label = qobject_cast<QLabel*>(widget))
        return label->text();
    return QString();
}

} // namespace

void TestWidgetProviderFallback::secondProviderServesWhenFirstReturnsNull()
{
    QList<QWidget*> created;
    TaggedProvider first(QStringLiteral("SomeOtherModel"), QStringLiteral("A"), &created);
    TaggedProvider second(QStringLiteral("Dummy"), QStringLiteral("B"), &created);

    ChatGraphModel model(registryWith(QStringLiteral("Dummy"), false));
    model.setWidgetProviders({&first, &second});

    const QtNodes::NodeId id = model.addNode(QStringLiteral("Dummy"));
    QWidget* widget = model.nodeWidget(id);

    QVERIFY2(widget != nullptr, "second provider must be consulted when the first returns null");
    QCOMPARE(labelText(widget), QStringLiteral("B"));

    qDeleteAll(created);
}

void TestWidgetProviderFallback::firstServingProviderWins()
{
    QList<QWidget*> created;
    TaggedProvider first(QStringLiteral("Dummy"), QStringLiteral("A"), &created);
    TaggedProvider second(QStringLiteral("Dummy"), QStringLiteral("B"), &created);

    ChatGraphModel model(registryWith(QStringLiteral("Dummy"), false));
    model.setWidgetProviders({&first, &second});

    const QtNodes::NodeId id = model.addNode(QStringLiteral("Dummy"));
    QWidget* widget = model.nodeWidget(id);

    QVERIFY(widget != nullptr);
    QCOMPARE(labelText(widget), QStringLiteral("A"));

    qDeleteAll(created);
}

void TestWidgetProviderFallback::cacheReturnsSameWidget()
{
    QList<QWidget*> created;
    TaggedProvider provider(QStringLiteral("Dummy"), QStringLiteral("A"), &created);

    ChatGraphModel model(registryWith(QStringLiteral("Dummy"), false));
    model.setWidgetProviders({&provider});

    const QtNodes::NodeId id = model.addNode(QStringLiteral("Dummy"));
    QWidget* first = model.nodeWidget(id);
    QWidget* second = model.nodeWidget(id);

    QVERIFY(first != nullptr);
    QCOMPARE(first, second);

    qDeleteAll(created);
}

void TestWidgetProviderFallback::allProvidersMissFallsBackToEmbeddedWidget()
{
    QList<QWidget*> created;
    TaggedProvider provider(QStringLiteral("SomeOtherModel"), QStringLiteral("A"), &created);

    ChatGraphModel model(registryWith(QStringLiteral("Dummy"), true));
    model.setWidgetProviders({&provider});

    const QtNodes::NodeId id = model.addNode(QStringLiteral("Dummy"));
    QWidget* widget = model.nodeWidget(id);

    QVERIFY2(widget != nullptr, "embeddedWidget() fallback must survive the provider loop");
    QCOMPARE(labelText(widget), QStringLiteral("E"));

    qDeleteAll(created);
}

void TestWidgetProviderFallback::emptyProviderListFallsBackToEmbeddedWidget()
{
    ChatGraphModel model(registryWith(QStringLiteral("Dummy"), true));
    model.setWidgetProviders({});

    const QtNodes::NodeId id = model.addNode(QStringLiteral("Dummy"));
    QWidget* widget = model.nodeWidget(id);

    QVERIFY(widget != nullptr);
    QCOMPARE(labelText(widget), QStringLiteral("E"));
}
