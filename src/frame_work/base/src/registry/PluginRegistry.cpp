#include "PluginRegistry.h"
#include "QPluginInterface.h"
#include "QBasePluginObject.h"
#include "debug.h"

namespace Daqster {

PluginRegistry::PluginRegistry(QObject* parent)
    : QObject(parent)
{
}

PluginRegistry::~PluginRegistry()
{
    // shutdownAll() is called explicitly via QPluginManager::ShutdownPluginManager()
    // which is connected to QApplication::aboutToQuit. Do NOT call it here —
    // objects may be in partially-destroyed state during destructor chain.
}

void PluginRegistry::setPersistenceCallback(std::function<void(const PluginDescription&)> callback)
{
    m_persistenceCallback = std::move(callback);
}

void PluginRegistry::registerPlugin(const QString& hash, QPluginInterface* iface)
{
    m_pluginMap[hash] = iface;
}

QPluginInterface* PluginRegistry::unregisterPlugin(const QString& hash)
{
    return m_pluginMap.take(hash);
}

QPluginInterface* PluginRegistry::plugin(const QString& hash) const
{
    return m_pluginMap.value(hash, nullptr);
}

bool PluginRegistry::contains(const QString& hash) const
{
    return m_pluginMap.contains(hash);
}

QList<QString> PluginRegistry::registeredHashes() const
{
    return m_pluginMap.keys();
}

QMap<QString, PluginDescription> PluginRegistry::pluginDescriptions() const
{
    return m_descriptions;
}

void PluginRegistry::setPluginDescriptions(const QMap<QString, PluginDescription>& descriptions)
{
    m_descriptions = descriptions;
}

QBasePluginObject* PluginRegistry::createPluginObject(const QString& hash, QObject* parent)
{
    QPluginInterface* iface = m_pluginMap.value(hash, nullptr);
    if (!iface || !iface->IsEnabled()) {
        return nullptr;
    }

    PluginDescription::PluginHealthyState_t healthy = iface->GetHealthyState();
    if (healthy == PluginDescription::ILL) {
        return nullptr;
    }

    // Crash recovery: if last time we crashed during OBJECT_CREATION, disable
    if (healthy == PluginDescription::OBJECT_CREATION) {
        iface->SetHealthyState(PluginDescription::ILL);
        if (m_descriptions.contains(hash)) {
            m_descriptions[hash] = iface->GetPluginDescriptor();
        }
        if (m_persistenceCallback) {
            m_persistenceCallback(iface->GetPluginDescriptor());
        }
        qWarning() << "Attention: There was application crash on last time loading of plugin"
                   << iface->GetLocation()
                   << ". Now we try second time and if it fail the plugin will be disabled."
                   << "To enable Plugin please change HealthyState state in configuration .ini file.";
        DEBUG << "Second chance for loading of plugin: " << iface->GetLocation() << ". If it fail it will be disabled.";
        return nullptr;
    }

    // Mark as OBJECT_CREATION for crash recovery
    if (healthy == PluginDescription::IF_LOADED) {
        iface->SetHealthyState(PluginDescription::OBJECT_CREATION);
        if (m_descriptions.contains(hash)) {
            m_descriptions[hash] = iface->GetPluginDescriptor();
        }
        if (m_persistenceCallback) {
            m_persistenceCallback(iface->GetPluginDescriptor());
        }
    }

    QBasePluginObject* obj = iface->CreatePlugin(parent);

    // Update health state
    PluginDescription::PluginHealthyState_t newHealthy = obj ? PluginDescription::HEALTHY : PluginDescription::ILL;
    if (iface->GetHealthyState() != newHealthy) {
        iface->SetHealthyState(newHealthy);
        if (m_descriptions.contains(hash)) {
            m_descriptions[hash] = iface->GetPluginDescriptor();
        }
        if (m_persistenceCallback) {
            m_persistenceCallback(iface->GetPluginDescriptor());
        }
    }

    return obj;
}

void PluginRegistry::enablePlugin(const QString& hash, bool enable)
{
    QPluginInterface* iface = m_pluginMap.value(hash, nullptr);
    if (iface) {
        iface->Enable(enable);
        if (m_descriptions.contains(hash)) {
            m_descriptions[hash].Enable(enable);
        }
    }
}

void PluginRegistry::shutdownPlugin(const QString& hash)
{
    QPluginInterface* iface = m_pluginMap.value(hash, nullptr);
    if (iface) {
        iface->ShutdownAllPluginObjects();
    }
}

void PluginRegistry::shutdownAll()
{
    for (auto it = m_pluginMap.begin(); it != m_pluginMap.end(); ++it) {
        if (it.value()) {
            it.value()->ShutdownAllPluginObjects();
        }
    }
}

QList<QObject*> PluginRegistry::instances(const char* iid)
{
    QList<QObject*> result;

    for (auto it = m_pluginMap.constBegin(); it != m_pluginMap.constEnd(); ++it) {
        QPluginInterface* iface = it.value();
        if (!iface || !iface->IsEnabled()) {
            continue;
        }

        // Lazy init — create instance if none exist yet
        if (iface->GetPluginInstances().isEmpty()) {
            createPluginObject(it.key());
        }

        for (QBasePluginObject* obj : iface->GetPluginInstances()) {
            if (obj && obj->qt_metacast(iid)) {
                obj->setProperty("_daqster_hash", it.key());
                result.append(obj);
            }
        }
    }

    return result;
}

QMap<QString, PluginDescription> PluginRegistry::allDescriptions() const
{
    return m_descriptions;
}

PluginDescription PluginRegistry::pluginDescription(const QString& hash) const
{
    return m_descriptions.value(hash, PluginDescription());
}

bool PluginRegistry::containsDescription(const QString& hash) const
{
    return m_descriptions.contains(hash);
}

void PluginRegistry::setPluginDescription(const QString& hash, const PluginDescription& desc)
{
    m_descriptions[hash] = desc;
}

QPluginInterface* PluginRegistry::takePlugin(const QString& hash)
{
    return m_pluginMap.take(hash);
}

void PluginRegistry::removeDescription(const QString& hash)
{
    m_descriptions.remove(hash);
}

} // namespace Daqster
