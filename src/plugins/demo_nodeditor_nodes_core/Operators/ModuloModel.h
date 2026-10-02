#ifndef MODULOMODEL_H
#define MODULOMODEL_H

#include <QtCore/QObject>
#include <QtNodes/NodeDelegateModel>
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
    /// Core model has no QtWidgets dependency — the type selector is created
    /// by the GUI plugin and wired through NodeWidgetFactory (REQ-SW-PL-051).
    QWidget *embeddedWidget() override { return nullptr; }

    /// Current type as a combo index (0 = int, 1 = double). Used by the GUI
    /// plugin to render the widget in the right state.
    int typeIndex() const { return (m_currentType == DataType::Int) ? 0 : 1; }

    /// The node BODY (boundary, caption, ports) does not depend on data —
    /// widget content self-repaints via Qt. The validation border self-repaints
    /// via setValidationState(). Opts out of the per-frame body repaint.
    bool dataArrivalChangesWidget() const override { return false; }

    QtNodes::NodeValidationState validationState() const override
    { return m_validationState; }

public slots:
    /// Type selector of the GUI widget (called by NodeWidgetFactory).
    void onTypeChanged(int index);

private:
    void switchType(DataType newType);
    void recompute();

    DataType m_currentType = DataType::Int;

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
