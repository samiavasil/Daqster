#include "NodeWidgetFactory.h"

#include <QtNodes/NodeDelegateModel>

namespace Daqster {

NodeWidgetFactory::NodeWidgetFactory(QObject* parent)
    : QObject(parent)
{
}

NodeWidgetFactory::~NodeWidgetFactory()
{
}

void NodeWidgetFactory::registerWidgetCreator(const QString& modelName,
                                              std::function<QWidget*(QtNodes::NodeDelegateModel*)> creator)
{
    m_creators[modelName] = std::move(creator);
}

QWidget* NodeWidgetFactory::createWidget(QtNodes::NodeDelegateModel* model) const
{
    if (!model)
        return nullptr;

    const QString modelName = model->name();
    auto it = m_creators.constFind(modelName);
    if (it != m_creators.constEnd()) {
        return it.value()(model);
    }
    return nullptr;
}

bool NodeWidgetFactory::hasWidgetCreator(const QString& modelName) const
{
    return m_creators.contains(modelName);
}

} // namespace Daqster