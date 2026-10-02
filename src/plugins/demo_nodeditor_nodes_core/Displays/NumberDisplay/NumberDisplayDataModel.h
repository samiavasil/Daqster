#pragma once

#include "demo_nodeditor_nodes_core_export.h"

#include <QtCore/QObject>
#include <QtCore/QEvent>

#include <QtNodes/NodeDelegateModel>

#include <iostream>
#include <memory>
#include "NodeDataTypes/NumericType.h"

class DEMO_NODEDITOR_NODES_CORE_EXPORT NumberDisplayDataModel : public QtNodes::NodeDelegateModel
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

    /// Core model has no QtWidgets dependency — the widget is created by the
    /// GUI plugin and wired through NodeWidgetFactory (REQ-SW-PL-051).
    QWidget *
    embeddedWidget() override { return nullptr; }

    /// The node BODY (boundary, caption, ports) does not depend on data —
    /// widget content self-repaints via Qt. The validation border self-repaints
    /// via setValidationState(). Opts out of the per-frame body repaint.
    bool dataArrivalChangesWidget() const override { return false; }

    /// The node geometry only changes on a REAL widget resize (handled by the
    /// event filter below) — not on data arrival. Opts out of the per-frame
    /// geometry recompute + connection move cascade.
    bool dataArrivalChangesGeometry() const override { return false; }

    /// Re-emits requestNodeUpdate() when the embedded widget is actually
    /// resized, so the scene recomputes the node size + moves connections.
    bool eventFilter(QObject *object, QEvent *event) override;

    QJsonObject save() const override;
    void load(QJsonObject const &p) override;

    QtNodes::NodeValidationState
    validationState() const override;

public slots:
    /// Type selector of the GUI widget (called by NodeWidgetFactory).
    void onTypeChanged(int index);

signals:
    /// The number to display. The core model owns the data; the GUI widget owns
    /// the QLabel it is rendered into (REQ-SW-PL-051 core/gui split).
    void displayTextChanged(QString const& text);

private:
    void switchType(DataType newType);

    DataType m_currentType = DataType::Double;

    std::shared_ptr<NumericType<int>> m_result_int;
    std::shared_ptr<NumericType<double>> m_result_dbl;

    QtNodes::NodeValidationState modelValidationState;
    QString modelValidationError = QStringLiteral("Missing or incorrect inputs");

};
