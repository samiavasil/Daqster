#include "PluginDiscovery.h"
#include "PluginDescription.h"
#include "LogCategories.h"

#include <QDir>
#include <QFileInfo>
#include <QLibrary>
#include <QCryptographicHash>
#include <QFile>
#include <QPluginLoader>
#include <QJsonObject>
#include <QJsonValue>

namespace Daqster {

PluginDiscovery::PluginDiscovery(QObject* parent)
    : QObject(parent)
{
}

PluginDiscovery::~PluginDiscovery()
{
}

void PluginDiscovery::addSearchPath(const QString& directory)
{
    QDir dir(directory);
    QString absPath = dir.absolutePath();
    if (m_searchPaths.contains(absPath))
        return;

    if (!dir.exists()) {
        qCDebug(lcFramework) << "PluginDiscovery: skipping non-existent search path:" << directory;
        return;
    }

    m_searchPaths.append(absPath);
}

QList<QString> PluginDiscovery::searchPaths() const
{
    return m_searchPaths;
}

bool PluginDiscovery::computeFileHash(const QString& filePath, QString& hash)
{
    bool ret = false;
    QCryptographicHash hashMaster(QCryptographicHash::Md5);
    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly)) {
        if (hashMaster.addData(&file)) {
            hash = QString(hashMaster.result().toHex().data());
            ret = true;
        }
    } else {
        hash = QString();
    }
    return ret;
}

bool PluginDiscovery::isCandidatePluginFile(const QString& filePath)
{
    const QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) {
        return false;
    }

    if (!QLibrary::isLibrary(filePath)) {
        return false;
    }

    const QString baseName = info.fileName().toLower();
    if (baseName.contains("plugin")) {
        return true;
    }

    const QString jsonPath = info.absolutePath() + "/" + info.completeBaseName() + ".json";
    if (QFile::exists(jsonPath)) {
        return true;
    }

    QPluginLoader loader(filePath);
    if (!loader.metaData().isEmpty()) {
        return true;
    }

    return false;
}

QString PluginDiscovery::pluginType(const QString& filePath)
{
    // metaData() reads the JSON Qt embeds in the binary; it does NOT load the
    // library, so a plugin can be classified before it (and its dependencies)
    // ever enter the process (REQ-SW-PL-073).
    const QJsonObject metaData = QPluginLoader(filePath).metaData();
    if (metaData.isEmpty()) {
        return QString();
    }

    // Walk down through "MetaData" keys. Qt always wraps the file content once;
    // some plugins (runtime hosts, application plugins) nest their own
    // "MetaData" again, node plugins keep it flat.
    QJsonObject node = metaData;
    for (int depth = 0; depth < 4; ++depth) {
        const QString type = node.value(QStringLiteral("PluginType")).toString();
        if (!type.isEmpty()) {
            return type;
        }
        const QJsonObject next = node.value(QStringLiteral("MetaData")).toObject();
        if (next.isEmpty()) {
            break;
        }
        node = next;
    }
    return QString();
}

bool PluginDiscovery::isInSearchPath(const QString& filePath) const
{
    const QString absFile = QFileInfo(filePath).absoluteFilePath();
    for (const QString& dirPath : m_searchPaths) {
        const QDir dir(dirPath);
        const QString absDir = dir.absolutePath();
        if (absFile.startsWith(absDir + QDir::separator()) || absFile == absDir) {
            return true;
        }
    }
    return false;
}

QMap<QString, QString> PluginDiscovery::discoverPlugins(
    const QSet<QString>& loadedHashes)
{
    QMap<QString, QString> newPlugins;
    QDir pluginsDir;

    for (const QString& path : m_searchPaths) {
        if (pluginsDir.cd(path)) {
            for (QString fileName : pluginsDir.entryList(QDir::Files)) {
                fileName = pluginsDir.absoluteFilePath(fileName);
                if (!isCandidatePluginFile(fileName)) {
                    continue;
                }

                QString hash;
                computeFileHash(fileName, hash);

                if (!hash.isEmpty() && !loadedHashes.contains(hash)) {
                    newPlugins[hash] = fileName;
                }
            }
            pluginsDir.cdUp();
        }
    }

    return newPlugins;
}

} // namespace Daqster
