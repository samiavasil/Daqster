#ifndef GAMEPADMODEL_H
#define GAMEPADMODEL_H

#include "NodeDataTypes/SampledData.h"
#include "GamepadEngine.h"
#include "shared/IStoppable.h"
#include "shared/IStartable.h"

#include <QtNodes/NodeDelegateModel>

#include <memory>

/**
 * @brief Gamepad input source node model (REQ-SW-PL-042).
 *
 * Thin NodeDelegateModel controller: 1 output port (SampledData "sample"),
 * owns the GamepadEngine (Linux joystick API). Each poll wraps the current
 * axis/button state in a shared_ptr<SampledData> with a SampledStreamDescriptor
 * (domain="gamepad", 12 FLOAT32 channels: 4 axes + 8 buttons) and emits
 * dataUpdated(0).
 *
 * The config UI (GamepadWidget) lives in the GUI plugin and is created through
 * NodeWidgetFactory (REQ-SW-PL-051). The configuration therefore lives here as
 * plain fields, and every user edit arrives through the on*Changed slots while
 * the engine reports back through the signals below.
 *
 * Connection-count gating (model of SystemMonitorModel/PlutoSdrModel): the
 * engine polls only while the user pressed Start AND at least one output
 * connection exists; removing the last connection auto-stops the polling
 * (clean teardown).
 */
class GamepadModel : public QtNodes::NodeDelegateModel, public Daqster::IStoppable, public Daqster::IStartable
{
    Q_OBJECT

public:
    GamepadModel();
    ~GamepadModel() override;

    QString caption() const override
    { return QStringLiteral("Gamepad Input"); }

    bool captionVisible() const override
    { return false; }

    QString name() const override
    { return QStringLiteral("GamepadInput"); }

    QJsonObject save() const override;
    void load(QJsonObject const &p) override;

    unsigned int nPorts(QtNodes::PortType portType) const override;

    QtNodes::NodeDataType dataType(QtNodes::PortType portType,
                                   QtNodes::PortIndex portIndex) const override;

    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex port) override;

    void setInData(std::shared_ptr<QtNodes::NodeData> data,
                   QtNodes::PortIndex port) override;

    /// Core model has no QtWidgets dependency — the config UI is created by
    /// the GUI plugin and wired through NodeWidgetFactory (REQ-SW-PL-051).
    QWidget *embeddedWidget() override { return nullptr; }

    /// Configuration state, for the GUI plugin to render the widget from.
    QString devicePath() const { return m_devicePath; }
    int pollRateHz() const { return m_pollRateHz; }

    /// Stop the polling timer + close the fd. Idempotent — safe to call
    /// multiple times (REQ-SW-PL-050).
    void stop() override;

    /// Start polling programmatically (runtime autoStart, REQ-SW-PL-048).
    void start() override;

    void outputConnectionCreated(QtNodes::ConnectionId const &) override;
    void outputConnectionDeleted(QtNodes::ConnectionId const &) override;

public slots:
    /// Controls of GamepadWidget (called by NodeWidgetFactory).
    void onStartRequested();
    void onStopRequested();
    void onDevicePathChanged(const QString &path);
    void onPollRateChanged(int hz);

signals:
    /// Engine feedback rendered by GamepadWidget.
    void statusChanged(const QString &status);
    void axisValuesChanged(float x, float y, float z, float rz);
    void buttonStatesChanged(float a, float b, float x, float y,
                            float lb, float rb, float back, float start);

private slots:
    void onStateReady(const GamepadState &s);
    void onStatusChanged(const QString &status);
    void onErrorOccurred(const QString &message);

private:
    void updateEngineConfig();
    void setPollingEnabled(bool enabled);

    GamepadEngine *m_engine = nullptr;
    std::shared_ptr<SampledData> m_output;
    QString m_devicePath = QStringLiteral("/dev/input/js0");
    int m_pollRateHz = 60;
    int m_connectionCount = 0;
    bool m_userStarted = false;
};

#endif // GAMEPADMODEL_H