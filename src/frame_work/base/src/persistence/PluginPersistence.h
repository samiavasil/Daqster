#ifndef PLUGINPERSISTENCE_H
#define PLUGINPERSISTENCE_H

#include "build_cfg.h"
#include "PluginDescription.h"
#include <QObject>
#include <QMap>
#include <QString>

namespace Daqster {

/**
 * @brief Handles plugin state persistence to QSettings.
 *
 * Responsible for:
 * - Loading plugin descriptions from INI file
 * - Saving plugin state changes
 * - Managing plugin enabled/disabled state
 */
class FRAME_WORKSHARED_EXPORT PluginPersistence : public QObject
{
    Q_OBJECT

public:
    explicit PluginPersistence(const QString& configFile, QObject* parent = nullptr);
    ~PluginPersistence();

    /**
     * @brief Load all plugin descriptions from persistence
     * @return Map of hash -> PluginDescription
     */
    QMap<QString, PluginDescription> loadPlugins();

    /**
     * @brief Save a single plugin's state
     * @param desc Plugin description to save
     */
    void savePluginState(const PluginDescription& desc);

    /**
     * @brief Remove a plugin from persistence
     * @param hash Plugin hash
     */
    void removePlugin(const QString& hash);

    /**
     * @brief Get the config file path
     * @return Config file path
     */
    QString configFile() const;

private:
    QString m_configFile;
};

} // namespace Daqster

#endif // PLUGINPERSISTENCE_H
