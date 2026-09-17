#ifndef GAMEPADMODEL_H
#define GAMEPADMODEL_H

#include "NodeDataTypes/SampledData.h"
#include "GamepadEngine.h"
#include "GamepadWidget.h"

#include <QtNodes/NodeDelegateModel>

#include <memory>

/**
 * @brief Gamepad input source node model (REQ-SW-PL-042).
 *
 * Thin NodeDelegateModel controller: 1 output port (SampledData "sample"),
 * owns the GamepadEngine (Linux joystick API) + the GamepadWidget (config UI).
 * Each poll wraps the current axis/button state in a shared_ptr<SampledData>
 * with a SampledStreamDescriptor (domain="gamepad", 12 FLOAT32 channels:
 * 4 axes + 8 buttons) and emits dataUpdated(0).
 *
 * Connection-count gating (model of SystemMonitorModel/PlutoSdrModel): the
 * engine polls only while the user pressed Start AND at least one output
 * connection exists; removing the last connection auto-stops the polling
 * (clean teardown).
 */
class GamepadModel : public QtNodes::NodeDelegateModel
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

    QWidget *embeddedWidget() override;

    void outputConnectionCreated(QtNodes::ConnectionId const &) override;
    void outputConnectionDeleted(QtNodes::ConnectionId const &) override;

private slots:
    void onStartRequested();
    void onStopRequested();
    void onDevicePathChanged(const QString &path);
    void onPollRateChanged(int hz);
    void onStateReady(const GamepadState &s);
    void onStatusChanged(const QString &status);
    void onErrorOccurred(const QString &message);

private:
    void updateEngineConfig();
    void setPollingEnabled(bool enabled);

    GamepadEngine *m_engine = nullptr;
    GamepadWidget *m_widget = nullptr;
    std::shared_ptr<SampledData> m_output;
    int m_connectionCount = 0;
    bool m_userStarted = false;
};

#endif // GAMEPADMODEL_H