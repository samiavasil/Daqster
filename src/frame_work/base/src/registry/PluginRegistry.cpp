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
    shutdownAll();
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

    PluginDescription::PluginHealtyState_t healthy = iface->GetHealthyState();
    if (healthy == PluginDescription::ILL) {
        return nullptr;
    }

    QBasePluginObject* obj = iface->CreatePlugin(parent);
    if (obj) {
        healthy = PluginDescription::HEALTHY;
    } else {
        healthy = PluginDescription::ILL;
    }

    if (iface->GetHealthyState() != healthy) {
        iface->SetHealthyState(healthy);
        if (m_descriptions.contains(hash)) {
            m_descriptions[hash] = iface->GetPluginDescriptor();
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
            QObject* qobj = dynamic_cast<QObject*>(obj);
            if (qobj && qobj->qt_metacast(iid)) {
                qobj->setProperty("_daqster_hash", it.key());
                result.append(qobj);
            }
        }
    }

    return result;
}

} // namespace Daqster
