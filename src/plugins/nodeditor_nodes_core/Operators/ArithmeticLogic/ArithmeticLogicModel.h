#ifndef ARITHMETICLOGICMODEL_H
#define ARITHMETICLOGICMODEL_H

#include <QtCore/QObject>
#include <QtNodes/NodeDelegateModel>
#include <memory>
#include "ExprParser.h"
#include "NodeDataTypes/NumericType.h"

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
    /// Core model has no QtWidgets dependency — the controls are created by the
    /// GUI plugin and wired through NodeWidgetFactory (REQ-SW-PL-051).
    QWidget *embeddedWidget() override { return nullptr; }

    /// Current state for the GUI plugin to render the widget from.
    int typeIndex() const { return (m_currentType == DataType::Int) ? 0 : 1; }
    int inputCount() const { return m_inputCount; }
    QString expression() const { return m_expression; }
    bool strobeEnabled() const { return m_strobeEnabled; }

    /// The node BODY (boundary, caption, ports) does not depend on data —
    /// widget content self-repaints via Qt. The validation border self-repaints
    /// via setValidationState(). Opts out of the per-frame body repaint.
    bool dataArrivalChangesWidget() const override { return false; }

    QtNodes::NodeValidationState validationState() const override
    { return m_validationState; }

public slots:
    /// Controls of the GUI widget (called by NodeWidgetFactory).
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
    QString m_expression;

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
