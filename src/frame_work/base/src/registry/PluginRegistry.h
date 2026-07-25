#ifndef PLUGINREGISTRY_H
#define PLUGINREGISTRY_H

#include "build_cfg.h"
#include "PluginDescription.h"
#include <QObject>
#include <QMap>
#include <QString>
#include <QList>

namespace Daqster {

class QPluginInterface;
class QBasePluginObject;

/**
 * @brief Handles runtime plugin registration and instance management.
 *
 * Responsible for:
 * - Maintaining the map of loaded plugin interfaces
 * - Creating plugin objects on demand
 * - Managing plugin lifecycle (enable/disable, shutdown)
 * - Providing capability discovery via instances()
 */
class FRAME_WORKSHARED_EXPORT PluginRegistry : public QObject
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
     * @brief Create a plugin object
     * @param hash Plugin hash
     * @param parent Parent QObject
     * @return Created plugin object, or nullptr on failure
     */
    QBasePluginObject* createPluginObject(const QString& hash, QObject* parent = nullptr);

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
     * @brief Find all instances implementing a given interface
     * @param iid Interface ID string
     * @return List of QObject pointers implementing the interface
     */
    QList<QObject*> instances(const char* iid);

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
    QMap<QString, QPluginInterface*> m_pluginMap;
    QMap<QString, PluginDescription> m_descriptions;
};

} // namespace Daqster

#endif // PLUGINREGISTRY_H
