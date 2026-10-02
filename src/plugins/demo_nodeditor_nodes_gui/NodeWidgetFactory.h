#ifndef NODEWIDGETFACTORY_H
#define NODEWIDGETFACTORY_H

#include <QObject>
#include <QHash>
#include <functional>
#include <QtNodes/NodeDelegateModel>

namespace Daqster {

/**
 * @brief Factory for creating node widgets from core models.
 * 
 * This factory connects core node models (which have no GUI dependencies)
 * with their corresponding GUI widgets. The factory is registered as a
 * service and used by the editor/runtime to create embedded widgets.
 */
class NodeWidgetFactory : public QObject
{
    Q_OBJECT

public:
    explicit NodeWidgetFactory(QObject* parent = nullptr);
    ~NodeWidgetFactory() override;

    /**
     * @brief Register a widget creator for a model name.
     * @param modelName The model name (from NodeDelegateModel::name())
     * @param creator Lambda that creates the widget given the model
     */
    void registerWidgetCreator(const QString& modelName,
                               std::function<QWidget*(QtNodes::NodeDelegateModel*)> creator);

    /**
     * @brief Create a widget for the given model.
     * @param model The core model
     * @return The created widget, or nullptr if no creator registered
     */
    QWidget* createWidget(QtNodes::NodeDelegateModel* model) const;

    /**
     * @brief Check if a widget creator is registered for a model name.
     */
    bool hasWidgetCreator(const QString& modelName) const;

private:
    QHash<QString, std::function<QWidget*(QtNodes::NodeDelegateModel*)>> m_creators;
};

/**
 * @brief Populate @p factory with a creator for every node of this plugin.
 *
 * Called once from the plugin object's Initialize(). A model is keyed by its
 * NodeDelegateModel::name(), and the creators only build QWidget instances —
 * all node state stays in the core model (REQ-SW-PL-051).
 */
void registerDefaultWidgetCreators(NodeWidgetFactory* factory);

} // namespace Daqster

#endif // NODEWIDGETFACTORY_H