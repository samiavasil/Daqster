#ifndef ARITHMETICLOGICMODEL_H
#define ARITHMETICLOGICMODEL_H

#include <QtCore/QObject>
#include <QtNodes/NodeDelegateModel>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QLabel>
#include <memory>
#include "ExprParser.h"
#include "NumericType.h"

class ArithmeticLogicModel : public QtNodes::NodeDelegateModel
{
    Q_OBJECT

public:
    enum class DataType { Int, Double };

    ArithmeticLogicModel();
    ~ArithmeticLogicModel() override;

    QString caption() const override
    { return QStringLiteral("Arithmetic/Logic"); }

    bool captionVisible() const override
    { return true; }

    bool portCaptionVisible(QtNodes::PortType, QtNodes::PortIndex) const override
    { return true; }

    QString portCaption(QtNodes::PortType portType, QtNodes::PortIndex portIndex) const override;

    QString name() const override
    { return QStringLiteral("Arithmetic/Logic"); }

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
    void onInputsChanged(int count);
    void onExpressionChanged(const QString& expr);
    void onStrobeToggled(bool checked);

private:
    void switchType(DataType newType);
    void switchInputCount(int newCount);
    void recompute();
    void updateInputPorts();
    QString defaultExpression() const;

    DataType m_currentType = DataType::Int;
    int m_inputCount = 2;
    bool m_strobeEnabled = false;

    QComboBox* m_typeCombo = nullptr;
    QSpinBox* m_inputSpin = nullptr;
    QLineEdit* m_exprEdit = nullptr;
    QCheckBox* m_strobeCheck = nullptr;
    QWidget* m_container = nullptr;

    ExprParser m_parser;

    std::weak_ptr<NumericType<int>> m_inputs_int[8];
    std::shared_ptr<NumericType<int>> m_result_int;
    std::weak_ptr<NumericType<double>> m_inputs_dbl[8];
    std::shared_ptr<NumericType<double>> m_result_dbl;

    bool m_strobeFired = false;
    std::weak_ptr<NumericType<int>> m_strobe_int;
    std::weak_ptr<NumericType<double>> m_strobe_dbl;

    QtNodes::NodeValidationState m_validationState;
    QString m_validationError = QStringLiteral("Missing or incorrect inputs");
};

#endif // ARITHMETICLOGICMODEL_H
