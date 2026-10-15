#include "test_widget_embed.h"

#include <QLabel>
#include <QMainWindow>
#include <QWidget>

#include <memory>

#include "NodeSupport/types/ChatGraphModel.h" // code under test (compiled in directly)
#include "LayoutEngine.h"                     // designer preview build
#include "capabilities/IWidgetProvider.h"

#include <QtNodes/DataFlowGraphicsScene>
#include <QtNodes/NodeDelegateModel>
#include <QtNodes/NodeDelegateModelRegistry>

// F6 designer round-trip regression. See test_widget_embed.h for the full story.

namespace {

/// Minimal leaf model — the scene only needs it to create a node.
class DummyModel : public QtNodes::NodeDelegateModel
{
public:
    QString name() const override { return QStringLiteral("Dummy"); }
    QString caption() const override { return name(); }

    unsigned int nPorts(QtNodes::PortType) const override { return 0; }
    QtNodes::NodeDataType dataType(QtNodes::PortType, QtNodes::PortIndex) const override
    {
        return {};
    }
    void setInData(std::shared_ptr<QtNodes::NodeData>, QtNodes::PortIndex) override {}
    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex) override { return nullptr; }

    // Widgets come from the IWidgetProvider below, not from the model.
    QWidget* embeddedWidget() override { return nullptr; }
};

/// Serves a QLabel for the node. The widget is intentionally *not* owned by the
/// model: once embedded, QGraphicsProxyWidget owns it, so the model must not
/// delete it again (the test only leaks it, which is fine).
class LabelProvider : public Daqster::IWidgetProvider
{
public:
    QWidget* createWidget(QtNodes::NodeDelegateModel* model) const override
    {
        if (!model || model->name() != QStringLiteral("Dummy"))
            return nullptr;
        return new QLabel(QStringLiteral("E"));
    }
};

std::shared_ptr<QtNodes::NodeDelegateModelRegistry> makeRegistry()
{
    auto registry = std::make_shared<QtNodes::NodeDelegateModelRegistry>();
    registry->registerModel<DummyModel>([]() { return std::make_unique<DummyModel>(); });
    return registry;
}

} // namespace

void TestWidgetEmbed::reembedAfterDesignerPreviewRoundTrip()
{
    LabelProvider provider;

    ChatGraphModel model(makeRegistry());
    model.setWidgetProviders({&provider});

    QtNodes::DataFlowGraphicsScene scene(model);
    const QtNodes::NodeId id = model.addNode(QStringLiteral("Dummy"));

    auto* ngo = scene.nodeGraphicsObject(id);
    QVERIFY(ngo != nullptr);
    QWidget* w = model.nodeWidget(id);
    QVERIFY(w != nullptr);
    QVERIFY2(ngo->isWidgetEmbedded(), "a freshly created node widget is embedded");

    // Snapshot the healthy embedded state; the round-trip must restore exactly
    // this. NOTE: an embedded node widget is its own *offscreen* toplevel window
    // (isWindow() == true, windowHandle() != nullptr) — that is the healthy
    // state, so assert equality with the snapshot, not "no window".
    const bool wasWindow = w->isWindow();
    QWidget* const wasParent = w->parentWidget();
    const bool hadWindowHandle = w->windowHandle() != nullptr;
    const bool wasVisible = w->isVisible();

    // F6 ON: NodeEditorIdeObject::deembedAllWidgets().
    ngo->setWidgetEmbedded(false);
    w->hide();
    QVERIFY(!ngo->isWidgetEmbedded());

    // LayoutDesignerPanel::rebuildPreview() reparents the widget into the preview.
    FlowUi::WorkspaceLayout ws;
    ws.id = 0;
    FlowUi::LayoutNode panel;
    panel.type = FlowUi::LayoutNode::Type::Panel;
    panel.nodeId = id;
    ws.root = panel;

    QMainWindow* preview = FlowUi::LayoutEngine::buildWorkspace(
        ws, [&model](QtNodes::NodeId nid) -> QWidget* { return model.nodeWidget(nid); });
    QVERIFY(preview != nullptr);

    QWidget host;
    preview->setParent(&host);
    host.resize(400, 300);
    host.show();
    preview->show();
    QCoreApplication::processEvents();

    // Leaving the preview hands the widget back as a plain child of the preview
    // QMainWindow — precisely the state that used to break re-embedding.
    QVERIFY2(w->parentWidget() != nullptr, "the preview reparents the node widget");

    // F6 OFF: NodeEditorIdeObject::reembedAllWidgets().
    host.hide();
    w->hide();
    ngo->setWidgetEmbedded(true);
    QCoreApplication::processEvents();

    QVERIFY2(ngo->isWidgetEmbedded(), "widget must be re-embedded after the F6 round-trip");
    QCOMPARE(w->isWindow(), wasWindow);
    QVERIFY(w->parentWidget() == wasParent);
    QCOMPARE(w->windowHandle() != nullptr, hadWindowHandle);
    QCOMPARE(w->isVisible(), wasVisible);

    // Detach + drop the preview; the scene (destroyed before the model) owns the
    // re-embedded widget through its proxy.
    preview->setParent(nullptr);
    delete preview;
}
