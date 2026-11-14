#include "NumberDisplayDataModel.h"

#include "NodeDataTypes/EmbeddingData.h"
#include "NodeDataTypes/NumericType.h"

#include <QStringList>

using QtNodes::PortType;
using QtNodes::PortIndex;
using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::NodeValidationState;

NumberDisplayDataModel::
NumberDisplayDataModel()
{
    // Display nodes must never get a graphics effect (perf): the shadow blur
    // runs per repaint and costs ~46% CPU during video playback.
    QtNodes::NodeStyle s = this->nodeStyle();
    s.ShadowEnabled = false;
    this->setNodeStyle(s);
}

unsigned int
NumberDisplayDataModel::
nPorts(PortType portType) const
{
    switch (portType)
    {
    case PortType::In:  return 1;
    case PortType::Out: return 0;
    default: break;
    }
    return 0;
}

NodeDataType
NumberDisplayDataModel::
dataType(PortType, PortIndex) const
{
    // REQ-SW-PL-090: the port type follows the selector, so an "embedding"
    // display accepts EmbeddingData and the type check refuses a numeric edge.
    if (m_currentType == DataType::Embedding)
        return EmbeddingData().type();
    if (m_currentType == DataType::Int)
        return NumericType<int>().type();
    else
        return NumericType<double>().type();
}

std::shared_ptr<NodeData>
NumberDisplayDataModel::
outData(PortIndex)
{
    if (m_currentType == DataType::Int)
        return m_result_int;
    else
        return m_result_dbl;
}

void
NumberDisplayDataModel::
setInData(std::shared_ptr<NodeData> data, PortIndex const)
{
    if (m_currentType == DataType::Int) {
        auto numberData = std::dynamic_pointer_cast<NumericType<int>>(data);
        if (numberData) {
            NodeValidationState s;
            s._state = NodeValidationState::State::Valid;
            setValidationState(s);
            Q_EMIT displayTextChanged(numberData->numberAsText());
        } else {
            NodeValidationState s;
            s._state = NodeValidationState::State::Warning;
            s._stateMessage = QStringLiteral("Missing or incorrect inputs");
            setValidationState(s);
            Q_EMIT displayTextChanged(QString());
        }
    } else if (m_currentType == DataType::Embedding) {
        // REQ-SW-PL-090: an embedding is shown as its statistics (dimension,
        // min/max/mean) plus the first few components. One pass over the
        // vector (~µs even at 4096 dims, no allocation per component), so it
        // runs inline — unlike the image/window displays there is nothing here
        // worth a 30 Hz worker pass.
        auto embeddingData = std::dynamic_pointer_cast<EmbeddingData>(data);
        NodeValidationState s;
        if (embeddingData && !embeddingData->isEmpty()) {
            s._state = NodeValidationState::State::Valid;
            setValidationState(s);
            Q_EMIT displayTextChanged(embeddingSummary(embeddingData->values()));
        } else {
            s._state = NodeValidationState::State::Warning;
            s._stateMessage = QStringLiteral("Missing or incorrect inputs");
            setValidationState(s);
            Q_EMIT displayTextChanged(QString());
        }
    } else {
        auto numberData = std::dynamic_pointer_cast<NumericType<double>>(data);
        if (numberData) {
            NodeValidationState s;
            s._state = NodeValidationState::State::Valid;
            setValidationState(s);
            Q_EMIT displayTextChanged(numberData->numberAsText());
        } else {
            NodeValidationState s;
            s._state = NodeValidationState::State::Warning;
            s._stateMessage = QStringLiteral("Missing or incorrect inputs");
            setValidationState(s);
            Q_EMIT displayTextChanged(QString());
        }
    }
}

bool
NumberDisplayDataModel::
eventFilter(QObject *object, QEvent *event)
{
    Q_UNUSED(object);
    Q_UNUSED(event);
    return false;
}

QJsonObject NumberDisplayDataModel::save() const
{
    QJsonObject modelJson = NodeDelegateModel::save();
    modelJson["type"] = (m_currentType == DataType::Int)       ? "int"
                        : (m_currentType == DataType::Embedding) ? "embedding"
                                                                 : "double";
    return modelJson;
}

void NumberDisplayDataModel::load(QJsonObject const &p)
{
    QString typeStr = p["type"].toString();
    if (typeStr == "int") {
        m_currentType = DataType::Int;
    } else if (typeStr == "embedding") {
        m_currentType = DataType::Embedding;
    } else {
        m_currentType = DataType::Double;
    }
}

void NumberDisplayDataModel::onTypeChanged(int index)
{
    // 0 = double, 1 = int (unchanged since the node existed), 2 = embedding.
    DataType newType = DataType::Double;
    if (index == 1)
        newType = DataType::Int;
    else if (index == 2)
        newType = DataType::Embedding;
    if (newType == m_currentType) return;
    switchType(newType);
}

void NumberDisplayDataModel::switchType(DataType newType)
{
    Q_EMIT portsAboutToBeDeleted(PortType::In, 0, 0);

    m_result_int.reset();
    m_result_dbl.reset();

    m_currentType = newType;

    Q_EMIT portsDeleted();
    Q_EMIT displayTextChanged(QString());
    Q_EMIT portsAboutToBeInserted(PortType::In, 0, 0);
    Q_EMIT portsInserted();
}

QtNodes::NodeValidationState
NumberDisplayDataModel::
validationState() const
{
    return modelValidationState;
}

QString NumberDisplayDataModel::embeddingSummary(std::vector<float> const &values, int maxValues)
{
    if (values.empty())
        return QStringLiteral("embedding: 0 values");

    // Single pass; n is the model's dimension (typically ≤ 4096).
    double min = double(values.front());
    double max = min;
    double sum = 0.0;
    for (float const value : values) {
        double const v = double(value);
        if (v < min) min = v;
        if (v > max) max = v;
        sum += v;
    }
    double const mean = sum / double(values.size());

    QString const head = QStringLiteral("dim %1 · min %2 · max %3 · mean %4")
                             .arg(int(values.size()))
                             .arg(min, 0, 'g', 4)
                             .arg(max, 0, 'g', 4)
                             .arg(mean, 0, 'g', 4);

    int const shown = qBound(0, maxValues, int(values.size()));
    if (shown == 0)
        return head;

    QStringList parts;
    parts.reserve(shown);
    for (int i = 0; i < shown; ++i) {
        const auto index = static_cast<std::size_t>(i);
        parts.append(QString::number(double(values[index]), 'g', 4));
    }

    // The tail is clipped on the value count, never on characters, so a wide
    // window still shows whole numbers.
    QString tail = QStringLiteral("[0…%1] %2")
                       .arg(shown - 1)
                       .arg(parts.join(QStringLiteral("  ")));
    if (shown < values.size())
        tail += QStringLiteral(" …");
    return head + QLatin1Char('\n') + tail;
}
