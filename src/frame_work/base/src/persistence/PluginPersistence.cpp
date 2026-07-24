#include "PluginPersistence.h"
#include "PluginDescription.h"
#include "debug.h"

#include <QSettings>
#include <QDir>

namespace Daqster {

PluginPersistence::PluginPersistence(const QString& configFile, QObject* parent)
    : QObject(parent)
    , m_configFile(configFile)
{
}

PluginPersistence::~PluginPersistence()
{
}

QMap<QString, PluginDescription> PluginPersistence::loadPlugins()
{
    QMap<QString, PluginDescription> plugins;
    QSettings settings(m_configFile, QSettings::IniFormat);

    if (!settings.childGroups().contains("Plugins")) {
        return plugins;
    }

    settings.beginGroup("Plugins");
    foreach (const QString& name, settings.childGroups()) {
        PluginDescription desc;
        settings.beginGroup(name);
        desc.GetPluginParamsFromPersistency(settings);
        settings.endGroup();

        QString hash = desc.GetProperty(PLUGIN_HASH).toString();
        if (!hash.isEmpty()) {
            plugins[hash] = desc;
        }
    }
    settings.endGroup();

    return plugins;
}

void PluginPersistence::savePluginState(const PluginDescription& desc)
{
    QSettings settings(m_configFile, QSettings::IniFormat);
    settings.beginGroup("Plugins");
    settings.beginGroup(desc.GetProperty(PLUGIN_HASH).toString());
    desc.StorePluginParamsToPersistency(settings);
    settings.endGroup();
    settings.endGroup();
}

void PluginPersistence::removePlugin(const QString& hash)
{
    QSettings settings(m_configFile, QSettings::IniFormat);
    settings.beginGroup("Plugins");
    settings.beginGroup(hash);
    settings.remove("");
    settings.endGroup();
    settings.endGroup();
}

QString PluginPersistence::configFile() const
{
    return m_configFile;
}

} // namespace Daqster
