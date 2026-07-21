#pragma once

#include <QtCore/QObject>
#include <QtWidgets/QWidget>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QVBoxLayout>
#include <QtNodes/NodeDelegateModel>
#include <memory>
#include <NumericType.h>

class NumberSourceDataUi;
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
    QWidget *embeddedWidget() override;

private slots:
    void onTypeChanged(int index);
    void onTextEdited(QString const &string);
    void onRandomToggled(bool checked);
    void onIntervalChanged(int value);
    void onTimerTick();

private:
    void switchType(DataType newType);
    void generateRandom();
    void updateTimer();

    DataType m_currentType = DataType::Double;

    std::shared_ptr<NumericType<int>> m_number_int;
    std::shared_ptr<NumericType<double>> m_number_dbl;

    QWidget* m_wrapper = nullptr;
    QComboBox* m_typeCombo = nullptr;
    NumberSourceDataUi* m_ui = nullptr;
    QTimer* m_timer = nullptr;
};
