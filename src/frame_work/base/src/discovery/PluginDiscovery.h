#ifndef PLUGINDISCOVERY_H
#define PLUGINDISCOVERY_H

#include "build_cfg.h"
#include <QObject>
#include <QList>
#include <QString>
#include <QMap>

namespace Daqster {

class PluginDescription;

/**
 * @brief Handles plugin file discovery and hash computation.
 *
 * Responsible for:
 * - Scanning configured directories for plugin files
 * - Computing file hashes for integrity checking
 * - Determining if a file is a candidate plugin
 */
class FRAME_WORKSHARED_EXPORT PluginDiscovery : public QObject
{
    Q_OBJECT

public:
    explicit PluginDiscovery(QObject* parent = nullptr);
    ~PluginDiscovery();

    /**
     * @brief Add a directory to the plugin search path
     * @param directory Directory path
     */
    void addSearchPath(const QString& directory);

    /**
     * @brief Get the list of search paths
     * @return List of directory paths
     */
    QList<QString> searchPaths() const;

    /**
     * @brief Compute hash of a file
     * @param filePath Path to the file
     * @param hash Result hash string
     * @return true on success
     */
    static bool computeFileHash(const QString& filePath, QString& hash);

    /**
     * @brief Check if a file is a candidate plugin file
     * @param filePath Path to check
     * @return true if file looks like a plugin
     */
    static bool isCandidatePluginFile(const QString& filePath);

    /**
     * @brief Check if a file is in one of the search paths
     * @param filePath Path to check
     * @return true if file is in a search path
     */
    bool isInSearchPath(const QString& filePath) const;

    /**
     * @brief Discover plugins in all search paths
     * @param existingPlugins Map of already known plugins (by hash)
     * @return Map of newly discovered plugins (hash -> file path)
     */
    QMap<QString, QString> discoverPlugins(const QMap<QString, PluginDescription>& existingPlugins);

private:
    QList<QString> m_searchPaths;
};

} // namespace Daqster

#endif // PLUGINDISCOVERY_H
