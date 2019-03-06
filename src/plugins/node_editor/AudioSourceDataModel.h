#ifndef SOURCEDATAMODEL_H
#define SOURCEDATAMODEL_H

#include <QtCore/QObject>
#include <nodes/NodeDataModel>
#include "ComplexType.h"

using QtNodes::PortType;
using QtNodes::PortIndex;
using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::NodeDataModel;
using QtNodes::NodeValidationState;


class AudioSourceDataModel : public NodeDataModel
{
Q_OBJECT

public:
AudioSourceDataModel();
#if 0
virtual
~AudioSourceDataModel();

public:

QString
caption() const override
{ return QStringLiteral("AudioSource Source"); }

bool
captionVisible() const override
{ return false; }

QString
name() const override
{ return QStringLiteral("AudioSource"); }

public:

QJsonObject
save() const override;

void
restore(QJsonObject const &p) override;

public:

unsigned int
nPorts(PortType portType) const override;

NodeDataType
dataType(PortType portType, PortIndex portIndex) const override;

std::shared_ptr<NodeData>
outData(PortIndex port) override;

void
setInData(std::shared_ptr<NodeData> data, PortIndex port) override;

QWidget *
embeddedWidget() override;


protected slots:
void ChangeTime(int t);
private Q_SLOTS:

void
onTextEdited(QString const &string);

private:

std::shared_ptr<ComplexType<double>> _number;

//NumberSourceDataUi * m_ui;
int m_time;
#endif
};

#endif // SOURCEDATAMODEL_H
