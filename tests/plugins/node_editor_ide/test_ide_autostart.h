#pragma once

#include "capabilities/IStartable.h"

#include <QtNodes/NodeDelegateModel>
#include <QtTest>

#include <QJsonObject>

/// A source that records how often it was started and does nothing else.
///
/// The whole point of the tests is "was `start()` called, exactly once, on
/// exactly the right node", so the fake must be observable and free: no timer,
/// no thread, no device. It implements `IStartable` (like the real capture
/// sources), has one output and no input — an entry point by structure.
class IdeAutoStartSourceModel : public QtNodes::NodeDelegateModel,
                                public Daqster::IStartable
{
    Q_OBJECT

public:
    IdeAutoStartSourceModel() = default;

    QString name() const override { return QStringLiteral("IdeAutoStartSource"); }

    QString caption() const override { return QStringLiteral("AutoStartSource"); }

    unsigned int nPorts(QtNodes::PortType type) const override
    {
        return type == QtNodes::PortType::Out ? 1u : 0u;
    }

    QtNodes::NodeDataType dataType(QtNodes::PortType, QtNodes::PortIndex) const override
    {
        return {};
    }

    void setInData(std::shared_ptr<QtNodes::NodeData>, QtNodes::PortIndex) override {}

    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex) override { return nullptr; }

    QWidget* embeddedWidget() override { return nullptr; }

    void start() override { ++m_starts; }

    int startCount() const { return m_starts; }

    /// Make this node report "I cannot run" without touching the graph: the
    /// scenario is a node that loaded with a missing backend.
    void setInvalid(const QString& message)
    {
        m_state._state = QtNodes::NodeValidationState::State::Error;
        m_state._stateMessage = message;
    }

    QtNodes::NodeValidationState validationState() const override { return m_state; }

private:
    int m_starts = 0;
    QtNodes::NodeValidationState m_state;
};

/// A startable node with one input and one output: **not** an entry point (its
/// input is wired), so only `autoStart: true` puts it in the plan.
class IdeAutoStartMiddleModel : public QtNodes::NodeDelegateModel,
                                public Daqster::IStartable
{
    Q_OBJECT

public:
    IdeAutoStartMiddleModel() = default;

    QString name() const override { return QStringLiteral("IdeAutoStartMiddle"); }

    QString caption() const override { return QStringLiteral("AutoStartMiddle"); }

    unsigned int nPorts(QtNodes::PortType) const override { return 1u; }

    QtNodes::NodeDataType dataType(QtNodes::PortType, QtNodes::PortIndex) const override
    {
        return {};
    }

    void setInData(std::shared_ptr<QtNodes::NodeData>, QtNodes::PortIndex) override {}

    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex) override { return nullptr; }

    QWidget* embeddedWidget() override { return nullptr; }

    void start() override { ++m_starts; }

    int startCount() const { return m_starts; }

private:
    int m_starts = 0;
};

/// A display-like node: flagged `autoStart` in a scene, but it has nothing to
/// start, so the IDE must ignore it instead of failing the load.
class IdeAutoStartSinkModel : public QtNodes::NodeDelegateModel
{
    Q_OBJECT

public:
    IdeAutoStartSinkModel() = default;

    QString name() const override { return QStringLiteral("IdeAutoStartSink"); }

    QString caption() const override { return QStringLiteral("AutoStartSink"); }

    unsigned int nPorts(QtNodes::PortType type) const override
    {
        return type == QtNodes::PortType::In ? 1u : 0u;
    }
    QtNodes::NodeDataType dataType(QtNodes::PortType, QtNodes::PortIndex) const override
    {
        return {};
    }

    void setInData(std::shared_ptr<QtNodes::NodeData>, QtNodes::PortIndex) override {}

    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex) override { return nullptr; }

    QWidget* embeddedWidget() override { return nullptr; }
};

/// REQ-SW-PL-082: the start the IDE runs on „Пусни сцената" (Ctrl+R).
///
/// *Which* nodes is `Daqster::plannedStarts()` — the entry points of the graph,
/// with `autoStart` as the override — tested once in `scene_autostart_tests`.
/// What is tested here is the IDE's half: the loop `NodeEditorIdeObject` calls,
/// its skip rules and its double-start guard, against a real
/// `DataFlowGraphModel` and the real `ui` section parser. No IDE window needed.
class IdeAutoStartTest : public QObject
{
    Q_OBJECT

private slots:
    /// Computing the plan is not a start: opening a scene runs this and must
    /// leave every node stopped.
    void computingThePlanStartsNothing();

    /// No flags at all: the entry point starts, the wired node does not — the
    /// structure decides, and that is the change (an unmarked scene used to do
    /// nothing).
    void noFlagStartsOnlyTheEntryPoint();

    /// `autoStart: false` on the only entry point of the scene: nothing runs.
    void falseFlagStartsNothing();

    /// `autoStart: true` on the wired node forces it in, next to the entry point.
    void trueFlagForcesTheWiredNode();

    /// A node the `ISceneOpening` verdict named is not started, and says why.
    void verdictSkipsTheOffendingNode();

    /// A node whose own validation state is not Valid is not started either.
    void invalidNodeIsSkipped();

    /// A flagged node that does not implement `IStartable` is ignored quietly.
    void nonStartableFlaggedNodeIsIgnored();

    /// The double-start guard: an id the host already started is not started again.
    void alreadyRunningNodeIsNotStartedTwice();
};
