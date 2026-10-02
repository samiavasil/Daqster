#ifndef PLUGINREGISTRY_H
#define PLUGINREGISTRY_H

#include "framework_core_export.h"
#include "PluginDescription.h"
#include <capabilities/IRuntimeHost.h>  // RuntimeMode (by value in the API)
#include <QObject>
#include <QMap>
#include <QString>
#include <QList>
#include <functional>

namespace Daqster {

class QPluginInterface;
class QBasePluginObject;
class INodeProvider;
class IWidgetProvider;

/**
 * @brief Handles runtime plugin registration and instance management.
 *
 * Responsible for:
 * - Maintaining the map of loaded plugin interfaces
 * - Creating plugin objects on demand
 * - Managing plugin lifecycle (enable/disable, shutdown)
 * - Providing capability discovery via instances()
 */
class FRAMEWORK_CORE_EXPORT PluginRegistry : public QObject
{
    Q_OBJECT

public:
    explicit PluginRegistry(QObject* parent = nullptr);
    ~PluginRegistry();

    /**
     * @brief Register a plugin interface
     * @param hash Plugin file hash
     * @param iface Plugin interface object
     */
    void registerPlugin(const QString& hash, QPluginInterface* iface);

    /**
     * @brief Unregister a plugin
     * @param hash Plugin hash
     * @return The removed interface (caller takes ownership)
     */
    QPluginInterface* unregisterPlugin(const QString& hash);

    /**
     * @brief Get a plugin interface by hash
     * @param hash Plugin hash
     * @return Plugin interface, or nullptr if not found
     */
    QPluginInterface* plugin(const QString& hash) const;

    /**
     * @brief Check if a plugin is registered
     * @param hash Plugin hash
     * @return true if registered
     */
    bool contains(const QString& hash) const;

    /**
     * @brief Get all registered plugin hashes
     * @return List of hashes
     */
    QList<QString> registeredHashes() const;

    /**
     * @brief Get plugin descriptions for all registered plugins
     * @return Map of hash -> description
     */
    QMap<QString, PluginDescription> pluginDescriptions() const;

    /**
     * @brief Update plugin descriptions
     * @param descriptions New descriptions map
     */
    void setPluginDescriptions(const QMap<QString, PluginDescription>& descriptions);

    /**
     * @brief Create a plugin object (single source of truth)
     *
     * Handles: enabled check, crash recovery, health state tracking,
     * object creation, persistence via callback.
     *
     * @param hash Plugin hash
     * @param parent Parent QObject
     * @return Created plugin object, or nullptr on failure
     */
    QBasePluginObject* createPluginObject(const QString& hash, QObject* parent = nullptr);

    /**
     * @brief Set callback for persisting plugin state changes
     * @param callback Function to call when plugin state needs to be saved
     */
    void setPersistenceCallback(std::function<void(const PluginDescription&)> callback);

    /**
     * @brief Enable or disable a plugin
     * @param hash Plugin hash
     * @param enable true to enable, false to disable
     */
    void enablePlugin(const QString& hash, bool enable);

    /**
     * @brief Shutdown all plugin objects for a specific plugin
     * @param hash Plugin hash
     */
    void shutdownPlugin(const QString& hash);

    /**
     * @brief Shutdown all plugins
     */
    void shutdownAll();

    /**
     * @brief Collect all live plugin object instances across all interfaces.
     *
     * Used by QPluginManager::ShutdownPluginManager() to synchronously delete
     * plugin objects (and join their threads) while the event loop is still
     * alive — the async deleteLater() chain is not processed once the loop
     * exits (aboutToQuit), leaving threads running during ~QApplication.
     *
     * @return List of all QBasePluginObject instances
     */
    QList<QBasePluginObject*> allPluginInstances() const;

    /**
     * @brief Find all instances implementing a given interface
     * @param iid Interface ID string
     * @return List of QObject pointers implementing the interface
     */
    QList<QObject*> instances(const char* iid);

    /**
     * @brief Find all plugin objects implementing the INodeProvider capability.
     *
     * INodeProvider is a non-QObject interface (implementers already inherit
     * QObject via QBasePluginObject), so it cannot participate in
     * Q_INTERFACES/qt_metacast. Use this instead of instances(INodeProvider_IID).
     *
     * @return List of INodeProvider pointers (lazily instantiating plugins)
     */
    QList<Daqster::INodeProvider*> nodeProviders();

    /**
     * @brief Find all plugin objects implementing the IWidgetProvider capability
     *        (REQ-SW-PL-051 core/gui split).
     *
     * A node model whose widget lives in a separate GUI plugin returns nullptr
     * from embeddedWidget(); the node editor asks these providers for the
     * widget instead. Probed with dynamic_cast for the same reason as
     * nodeProviders() — IWidgetProvider is a non-QObject interface.
     *
     * @return List of IWidgetProvider pointers (lazily instantiating plugins)
     */
    QList<Daqster::IWidgetProvider*> widgetProviders();

    /**
     * @brief Find all plugin objects implementing the IRuntimeHost capability
     *        for a given runtime mode (REQ-SW-PL-053).
     *
     * IRuntimeHost is a non-QObject interface, so like INodeProvider it is
     * probed with dynamic_cast rather than qt_metacast. Hosts of the other
     * mode are filtered out so a runner can ask for exactly the flavour it
     * needs and never accidentally start the wrong engine.
     *
     * @param mode Runtime mode to filter by
     * @return List of IRuntimeHost pointers (lazily instantiating plugins)
     */
    QList<Daqster::IRuntimeHost*> runtimeHosts(Daqster::RuntimeMode mode);

    // ── Additional methods for full QPluginManager delegation ─────

    /**
     * @brief Get all plugin descriptions
     */
    QMap<QString, PluginDescription> allDescriptions() const;

    /**
     * @brief Get a single plugin description by hash
     */
    PluginDescription pluginDescription(const QString& hash) const;

    /**
     * @brief Check if a description exists for a hash
     */
    bool containsDescription(const QString& hash) const;

    /**
     * @brief Set/update a plugin description
     */
    void setPluginDescription(const QString& hash, const PluginDescription& desc);

    /**
     * @brief Remove a plugin and return its interface (caller takes ownership)
     */
    QPluginInterface* takePlugin(const QString& hash);

    /**
     * @brief Remove a plugin description
     */
    void removeDescription(const QString& hash);

signals:
    void pluginListChanged();

private:
    /**
     * @brief Instance list of a plugin interface, creating a registry-owned
     *        object when the plugin has none yet.
     *
     * Capability objects are handed out as raw pointers to callers that keep
     * them for the whole session (the node editor stores IWidgetProvider in
     * its graph model), so anything this method creates is parented to the
     * registry and dies with it instead of floating free.
     */
    QList<QBasePluginObject*> capabilityInstances(QPluginInterface* iface, const QString& hash);

    /**
     * @brief Call Initialize() on a plugin object at most once.
     *
     * A capability object is useless until initialized (e.g.
     * DemoNodeEditorNodesGuiObject builds its NodeWidgetFactory there), and
     * plugins are not guaranteed to have been initialized by the launcher.
     * Objects that failed to initialize are retried on the next discovery.
     */
    static void ensureInitialized(QBasePluginObject* obj);

    QMap<QString, QPluginInterface*> m_pluginMap;
    QMap<QString, PluginDescription> m_descriptions;
    std::function<void(const PluginDescription&)> m_persistenceCallback;
};

} // namespace Daqster

#endif // PLUGINREGISTRY_H