#ifndef MODULOMODEL_H
#define MODULOMODEL_H

#include <QtCore/QObject>
#include <QtNodes/NodeDelegateModel>
#include <QComboBox>
#include <memory>
#include "NodeDataTypes/NumericType.h"

class ModuloModel : public QtNodes::NodeDelegateModel
{
    Q_OBJECT

public:
    enum class DataType { Int, Double };

    ModuloModel();
    ~ModuloModel() override;

    QString caption() const override
    { return QStringLiteral("Modulo"); }

    bool captionVisible() const override
    { return true; }

    bool portCaptionVisible(QtNodes::PortType, QtNodes::PortIndex) const override
    { return true; }

    QString portCaption(QtNodes::PortType portType, QtNodes::PortIndex portIndex) const override
    {
        if (portType == QtNodes::PortType::In) {
            if (portIndex == 0) return QStringLiteral("Dividend");
            if (portIndex == 1) return QStringLiteral("Divisor");
        } else if (portType == QtNodes::PortType::Out) {
            return QStringLiteral("Result");
        }
        return QString();
    }

    QString name() const override
    { return QStringLiteral("Modulo"); }

    QJsonObject save() const override;
    void load(QJsonObject const &p) override;

    unsigned int nPorts(QtNodes::PortType portType) const override;
    QtNodes::NodeDataType dataType(QtNodes::PortType portType, QtNodes::PortIndex portIndex) const override;
    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex const port) override;
    void setInData(std::shared_ptr<QtNodes::NodeData> nodeData, QtNodes::PortIndex const portIndex) override;
    QWidget *embeddedWidget() override;

    QtNodes::NodeValidationState validationState() const override
    { return m_validationState; }

private slots:
    void onTypeChanged(int index);

private:
    void switchType(DataType newType);
    void recompute();

    DataType m_currentType = DataType::Int;
    QComboBox* m_typeCombo = nullptr;

    std::weak_ptr<NumericType<int>> m_num1_int;
    std::weak_ptr<NumericType<int>> m_num2_int;
    std::shared_ptr<NumericType<int>> m_result_int;

    std::weak_ptr<NumericType<double>> m_num1_dbl;
    std::weak_ptr<NumericType<double>> m_num2_dbl;
    std::shared_ptr<NumericType<double>> m_result_dbl;

    QtNodes::NodeValidationState m_validationState;
    QString m_validationError = QStringLiteral("Missing or incorrect inputs");
};

#endif // MODULOMODEL_H
