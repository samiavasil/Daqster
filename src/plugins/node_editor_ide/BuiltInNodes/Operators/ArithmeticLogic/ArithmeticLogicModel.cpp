#include "ArithmeticLogicModel.h"
#include <cmath>

using QtNodes::PortType;
using QtNodes::PortIndex;
using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::NodeValidationState;

ArithmeticLogicModel::ArithmeticLogicModel()
{
    m_container = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(m_container);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    // Type combo
    QHBoxLayout* typeRow = new QHBoxLayout();
    QLabel* typeLabel = new QLabel("Type:");
    m_typeCombo = new QComboBox();
    m_typeCombo->addItem("int");
    m_typeCombo->addItem("double");
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ArithmeticLogicModel::onTypeChanged);
    typeRow->addWidget(typeLabel);
    typeRow->addWidget(m_typeCombo);
    layout->addLayout(typeRow);

    // Input count spin
    QHBoxLayout* inputRow = new QHBoxLayout();
    QLabel* inputLabel = new QLabel("Inputs:");
    m_inputSpin = new QSpinBox();
    m_inputSpin->setRange(2, 8);
    m_inputSpin->setValue(2);
    connect(m_inputSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ArithmeticLogicModel::onInputsChanged);
    inputRow->addWidget(inputLabel);
    inputRow->addWidget(m_inputSpin);
    layout->addLayout(inputRow);

    // Expression line edit
    QLabel* exprLabel = new QLabel("Expression:");
    m_exprEdit = new QLineEdit();
    m_exprEdit->setPlaceholderText("e.g. a+b");
    m_exprEdit->setText(defaultExpression());
    connect(m_exprEdit, &QLineEdit::textChanged,
            this, &ArithmeticLogicModel::onExpressionChanged);
    layout->addWidget(exprLabel);
    layout->addWidget(m_exprEdit);

    // Strobe checkbox
    m_strobeCheck = new QCheckBox("Strobe");
    connect(m_strobeCheck, &QCheckBox::toggled,
            this, &ArithmeticLogicModel::onStrobeToggled);
    layout->addWidget(m_strobeCheck);

    // Initialize parser
    m_parser.parse(m_exprEdit->text());
}

ArithmeticLogicModel::~ArithmeticLogicModel() {}

QString ArithmeticLogicModel::portCaption(PortType portType, PortIndex portIndex) const
{
    if (portType == PortType::In) {
        if (m_strobeEnabled && portIndex == m_inputCount) {
            return QStringLiteral("Strobe");
        }
        if (portIndex < m_inputCount) {
            char var = 'a' + portIndex;
            return QString(var);
        }
    } else if (portType == PortType::Out) {
        return QStringLiteral("Result");
    }
    return QString();
}

QString ArithmeticLogicModel::defaultExpression() const
{
    if (m_inputCount >= 2) return QStringLiteral("a+b");
    return QStringLiteral("a");
}

QJsonObject ArithmeticLogicModel::save() const
{
    QJsonObject modelJson;
    modelJson["name"] = name();
    modelJson["type"] = (m_currentType == DataType::Int) ? "int" : "double";
    modelJson["inputs"] = m_inputCount;
    modelJson["expression"] = m_exprEdit->text();
    modelJson["strobe"] = m_strobeEnabled;
    return modelJson;
}

void ArithmeticLogicModel::load(QJsonObject const &p)
{
    QString typeStr = p["type"].toString();
    m_typeCombo->blockSignals(true);
    m_typeCombo->setCurrentIndex(typeStr == "double" ? 1 : 0);
    m_typeCombo->blockSignals(false);

    int inputs = p["inputs"].toInt(2);
    m_inputSpin->blockSignals(true);
    m_inputSpin->setValue(inputs);
    m_inputSpin->blockSignals(false);

    m_inputCount = inputs;
    m_exprEdit->blockSignals(true);
    m_exprEdit->setText(p["expression"].toString(defaultExpression()));
    m_exprEdit->blockSignals(false);

    m_parser.parse(m_exprEdit->text());

    m_strobeEnabled = p["strobe"].toBool(false);
    m_strobeCheck->blockSignals(true);
    m_strobeCheck->setChecked(m_strobeEnabled);
    m_strobeCheck->blockSignals(false);
}

unsigned int ArithmeticLogicModel::nPorts(PortType portType) const
{
    switch (portType) {
    case PortType::In: {
        int count = m_inputCount;
        if (m_strobeEnabled) count++;
        return count;
    }
    case PortType::Out: return 1;
    default: break;
    }
    return 0;
}

NodeDataType ArithmeticLogicModel::dataType(PortType, PortIndex) const
{
    if (m_currentType == DataType::Int)
        return NumericType<int>().type();
    else
        return NumericType<double>().type();
}

std::shared_ptr<NodeData> ArithmeticLogicModel::outData(PortIndex)
{
    if (m_currentType == DataType::Int)
        return m_result_int;
    else
        return m_result_dbl;
}

QWidget* ArithmeticLogicModel::embeddedWidget()
{
    return m_container;
}

void ArithmeticLogicModel::onTypeChanged(int index)
{
    DataType newType = (index == 0) ? DataType::Int : DataType::Double;
    if (newType == m_currentType) return;
    switchType(newType);
}

void ArithmeticLogicModel::onInputsChanged(int count)
{
    if (count == m_inputCount) return;
    switchInputCount(count);
}

void ArithmeticLogicModel::onExpressionChanged(const QString& expr)
{
    m_parser.parse(expr);
    recompute();
}

void ArithmeticLogicModel::onStrobeToggled(bool checked)
{
    if (checked == m_strobeEnabled) return;
    m_strobeEnabled = checked;

    Q_EMIT portsAboutToBeDeleted(PortType::In, 0, nPorts(PortType::In) - 1);
    m_strobe_int.reset();
    m_strobe_dbl.reset();
    Q_EMIT portsDeleted();
    Q_EMIT portsAboutToBeInserted(PortType::In, 0, nPorts(PortType::In) - 1);
    Q_EMIT portsInserted();

    recompute();
}

void ArithmeticLogicModel::switchType(DataType newType)
{
    Q_EMIT portsAboutToBeDeleted(PortType::In, 0, nPorts(PortType::In) - 1);
    Q_EMIT portsAboutToBeDeleted(PortType::Out, 0, nPorts(PortType::Out) - 1);

    for (int i = 0; i < 8; i++) {
        m_inputs_int[i].reset();
        m_inputs_dbl[i].reset();
    }
    m_result_int.reset();
    m_result_dbl.reset();
    m_strobe_int.reset();
    m_strobe_dbl.reset();

    m_currentType = newType;

    Q_EMIT portsDeleted();
    Q_EMIT portsAboutToBeInserted(PortType::In, 0, nPorts(PortType::In) - 1);
    Q_EMIT portsAboutToBeInserted(PortType::Out, 0, nPorts(PortType::Out) - 1);
    Q_EMIT portsInserted();

    recompute();
}

void ArithmeticLogicModel::switchInputCount(int newCount)
{
    Q_EMIT portsAboutToBeDeleted(PortType::In, 0, nPorts(PortType::In) - 1);

    for (int i = 0; i < 8; i++) {
        m_inputs_int[i].reset();
        m_inputs_dbl[i].reset();
    }
    m_result_int.reset();
    m_result_dbl.reset();

    m_inputCount = newCount;

    Q_EMIT portsDeleted();
    Q_EMIT portsAboutToBeInserted(PortType::In, 0, nPorts(PortType::In) - 1);
    Q_EMIT portsInserted();

    recompute();
}

void ArithmeticLogicModel::recompute()
{
    PortIndex const outPortIndex = 0;

    m_parser.parse(m_exprEdit->text());

    if (m_currentType == DataType::Int) {
        // Set variables
        for (int i = 0; i < m_inputCount; i++) {
            auto val = m_inputs_int[i].lock();
            char var = 'a' + i;
            m_parser.setVariable(QString(var), val ? val->number() : 0);
        }

        // Check strobe
        if (m_strobeEnabled) {
            auto strobe = m_strobe_int.lock();
            if (!strobe || strobe->number() == 0) {
                NodeValidationState s;
                s._state = NodeValidationState::State::Warning;
                s._stateMessage = QStringLiteral("Waiting for strobe");
                setValidationState(s);
                m_result_int.reset();
                Q_EMIT dataUpdated(outPortIndex);
                return;
            }
        }

        double result = m_parser.evaluate();
        if (!m_parser.errorString().isEmpty()) {
            NodeValidationState s;
            s._state = NodeValidationState::State::Error;
            s._stateMessage = m_parser.errorString();
            setValidationState(s);
            m_result_int.reset();
        } else {
            NodeValidationState s;
            s._state = NodeValidationState::State::Valid;
            setValidationState(s);
            m_result_int = std::make_shared<NumericType<int>>(static_cast<int>(result));
        }
    } else {
        // Set variables
        for (int i = 0; i < m_inputCount; i++) {
            auto val = m_inputs_dbl[i].lock();
            char var = 'a' + i;
            m_parser.setVariable(QString(var), val ? val->number() : 0);
        }

        // Check strobe
        if (m_strobeEnabled) {
            auto strobe = m_inputs_dbl[m_inputCount].lock();
            if (!strobe || strobe->number() == 0.0) {
                NodeValidationState s;
                s._state = NodeValidationState::State::Warning;
                s._stateMessage = QStringLiteral("Waiting for strobe");
                setValidationState(s);
                m_result_dbl.reset();
                Q_EMIT dataUpdated(outPortIndex);
                return;
            }
        }

        double result = m_parser.evaluate();
        if (!m_parser.errorString().isEmpty()) {
            NodeValidationState s;
            s._state = NodeValidationState::State::Error;
            s._stateMessage = m_parser.errorString();
            setValidationState(s);
            m_result_dbl.reset();
        } else {
            NodeValidationState s;
            s._state = NodeValidationState::State::Valid;
            setValidationState(s);
            m_result_dbl = std::make_shared<NumericType<double>>(result);
        }
    }

    Q_EMIT dataUpdated(outPortIndex);
}

void ArithmeticLogicModel::setInData(std::shared_ptr<NodeData> data, PortIndex portIndex)
{
    if (m_currentType == DataType::Int) {
        auto numData = std::dynamic_pointer_cast<NumericType<int>>(data);
        if (m_strobeEnabled && portIndex == m_inputCount) {
            m_strobe_int = numData;
        } else if (portIndex < m_inputCount) {
            m_inputs_int[portIndex] = numData;
        }
    } else {
        auto numData = std::dynamic_pointer_cast<NumericType<double>>(data);
        if (m_strobeEnabled && portIndex == m_inputCount) {
            m_strobe_dbl = numData;
        } else if (portIndex < m_inputCount) {
            m_inputs_dbl[portIndex] = numData;
        }
    }
    recompute();
}
