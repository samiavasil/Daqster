#pragma once

#include <QtCore/QObject>
#include <QtNodes/NodeDelegateModel>
#include <memory>
#include "NodeDataTypes/NumericType.h"

class QTimer;

class NumberSourceDataModel
    : public QtNodes::NodeDelegateModel
{
    Q_OBJECT

public:
    enum class DataType { Int, Double };

    NumberSourceDataModel();
    ~NumberSourceDataModel() override;

    QString caption() const override
    { return QStringLiteral("Number Source"); }

    bool captionVisible() const override
    { return false; }

    QString name() const override
    { return QStringLiteral("NumberSource"); }

    QJsonObject save() const override;
    void load(QJsonObject const &p) override;

    unsigned int nPorts(QtNodes::PortType portType) const override;
    QtNodes::NodeDataType dataType(QtNodes::PortType portType, QtNodes::PortIndex portIndex) const override;
    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex const port) override;
    void setInData(std::shared_ptr<QtNodes::NodeData> data, QtNodes::PortIndex const port) override;

    /// Core model has no QtWidgets dependency — the controls are created by the
    /// GUI plugin and wired through NodeWidgetFactory (REQ-SW-PL-051).
    QWidget *embeddedWidget() override { return nullptr; }

    /// Current state for the GUI plugin to render the widget from.
    int typeIndex() const { return (m_currentType == DataType::Int) ? 1 : 0; }
    QString text() const { return m_text; }
    bool randomEnabled() const { return m_randomEnabled; }
    int interval() const { return m_interval; }

public slots:
    /// Controls of the GUI widget (called by NodeWidgetFactory).
    void onTypeChanged(int index);
    void onTextEdited(QString const &string);
    void onRandomToggled(bool checked);
    void onIntervalChanged(int value);

signals:
    /// Random mode generated a new value — the GUI line edit must follow.
    void textChanged(const QString& text);
    /// The value line edit is read-only while random mode is on.
    void textEditableChanged(bool editable);

private slots:
    void onTimerTick();

private:
    void switchType(DataType newType);
    void generateRandom();
    void updateTimer();

    DataType m_currentType = DataType::Double;
    QString m_text = QStringLiteral("0.0");
    bool m_randomEnabled = false;
    int m_interval = 500;

    std::shared_ptr<NumericType<int>> m_number_int;
    std::shared_ptr<NumericType<double>> m_number_dbl;

    QTimer* m_timer = nullptr;
};
