#pragma once

#include <QtCore/QObject>
#include <QtWidgets/QLabel>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QVBoxLayout>

#include <QtNodes/NodeDelegateModel>

#include <iostream>
#include <memory>
#include "NodeDataTypes/NumericType.h"

class NumberDisplayDataModel : public QtNodes::NodeDelegateModel
{
    Q_OBJECT

public:
    enum class DataType { Int, Double };

    NumberDisplayDataModel();

    virtual
    ~NumberDisplayDataModel() {}

public:

    QString
    caption() const override
    { return QStringLiteral("NumberResult"); }

    bool
    captionVisible() const override
    { return false; }

    QString
    name() const override
    { return QStringLiteral("NumberResult"); }

public:

    unsigned int
    nPorts(QtNodes::PortType portType) const override;

    QtNodes::NodeDataType
    dataType(QtNodes::PortType portType,
             QtNodes::PortIndex portIndex) const override;

    std::shared_ptr<QtNodes::NodeData>
    outData(QtNodes::PortIndex const port) override;

    void
    setInData(std::shared_ptr<QtNodes::NodeData> data, QtNodes::PortIndex const portIndex) override;

    QWidget *
    embeddedWidget() override { return m_wrapper; }

    /// The node BODY (boundary, caption, ports) does not depend on data —
    /// widget content self-repaints via Qt. The validation border self-repaints
    /// via setValidationState(). Opts out of the per-frame body repaint.
    bool dataArrivalChangesWidget() const override { return false; }

    QJsonObject save() const override;
    void load(QJsonObject const &p) override;

    QtNodes::NodeValidationState
    validationState() const override { return modelValidationState; }

private slots:
    void onTypeChanged(int index);

private:
    void switchType(DataType newType);

    DataType m_currentType = DataType::Double;

    std::shared_ptr<NumericType<int>> m_result_int;
    std::shared_ptr<NumericType<double>> m_result_dbl;

    QtNodes::NodeValidationState modelValidationState;
    QString modelValidationError = QStringLiteral("Missing or incorrect inputs");

    QWidget* m_wrapper = nullptr;
    QComboBox* m_typeCombo = nullptr;
    QLabel * _label;
};
