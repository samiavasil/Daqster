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
/// sources) and nothing else.
class IdeAutoStartSourceModel : public QtNodes::NodeDelegateModel,
                                public Daqster::IStartable
{
    Q_OBJECT

public:
    IdeAutoStartSourceModel() = default;

    QString name() const override { return QStringLiteral("IdeAutoStartSource"); }

    QString caption() const override { return QStringLiteral("AutoStartSource"); }

    unsigned int nPorts(QtNodes::PortType) const override { return 0u; }

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

/// A display-like node: flagged `autoStart` in a scene, but it has nothing to
/// start, so the IDE must ignore it instead of failing the load.
class IdeAutoStartSinkModel : public QtNodes::NodeDelegateModel
{
    Q_OBJECT

public:
    IdeAutoStartSinkModel() = default;

    QString name() const override { return QStringLiteral("IdeAutoStartSink"); }

    QString caption() const override { return QStringLiteral("AutoStartSink"); }

    unsigned int nPorts(QtNodes::PortType) const override { return 0u; }

    QtNodes::NodeDataType dataType(QtNodes::PortType, QtNodes::PortIndex) const override
    {
        return {};
    }

    void setInData(std::shared_ptr<QtNodes::NodeData>, QtNodes::PortIndex) override {}

    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex) override { return nullptr; }

    QWidget* embeddedWidget() override { return nullptr; }
};

/// REQ-SW-PL-082: the IDE starts the nodes a scene declared with `autoStart`.
///
/// The loop under test is the one `NodeEditorIdeObject::loadSceneFromFile()`
/// calls, so these tests go through a real `DataFlowGraphModel` and the real
/// `ui` section parser — not through a mock of either. No IDE window is needed:
/// the loop owns no widget, and starting a node is the model's business.
class IdeAutoStartTest : public QObject
{
    Q_OBJECT

private slots:
    /// The acceptance case: two nodes, one flagged → exactly one started.
    void onlyTheFlaggedNodeStarts();

    /// A scene with no `autoStart` anywhere keeps the old behaviour: nothing runs.
    void absentFlagStartsNothing();

    /// `autoStart: false` is the same as absent.
    void falseFlagStartsNothing();

    /// A node the `ISceneOpening` verdict named is not started, and says why.
    void verdictSkipsTheOffendingNode();

    /// A node whose own validation state is not Valid is not started either.
    void invalidNodeIsSkipped();

    /// A flagged node that does not implement `IStartable` is ignored quietly.
    void nonStartableFlaggedNodeIsIgnored();

    /// The double-start guard: an id the host already started is not started again.
    void alreadyRunningNodeIsNotStartedTwice();
};
