#include "PluginRegistry.h"
#include "QPluginInterface.h"
#include "QBasePluginObject.h"
#include "LogCategories.h"
#include <capabilities/INodeProvider.h>
#include <capabilities/IRuntimeHost.h>
#include <capabilities/IWidgetProvider.h>

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
    QObject::connect(iface, &QObject::destroyed, this, [this, hash]() {
        m_pluginMap.remove(hash);
    });
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
        qCWarning(lcRegistry) << "Attention: There was application crash on last time loading of plugin"
                   << iface->GetLocation()
                   << ". Now we try second time and if it fail the plugin will be disabled."
                   << "To enable Plugin please change HealthyState state in configuration .ini file.";
        qCDebug(lcRegistry) << "Second chance for loading of plugin: " << iface->GetLocation() << ". If it fail it will be disabled.";
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
    auto snapshot = m_pluginMap;
    for (auto it = snapshot.begin(); it != snapshot.end(); ++it) {
        if (it.value()) {
            it.value()->ShutdownAllPluginObjects();
        }
    }
}

QList<QBasePluginObject*> PluginRegistry::allPluginInstances() const
{
    QList<QBasePluginObject*> result;
    for (auto it = m_pluginMap.constBegin(); it != m_pluginMap.constEnd(); ++it) {
        QPluginInterface* iface = it.value();
        if (!iface)
            continue;
        for (QBasePluginObject* obj : iface->GetPluginInstances()) {
            if (obj)
                result.append(obj);
        }
    }
    return result;
}

QList<QBasePluginObject*> PluginRegistry::capabilityInstances(QPluginInterface* iface, const QString& hash)
{
    if (!iface || !iface->IsEnabled())
        return {};

    // Lazy init — create instance if none exist yet.
    //
    // Deliberately parentless (nullptr, not `this`): QPluginManager::
    // ShutdownPluginManager() deletes every instance from
    // allPluginInstances(). Parenting them to the registry would make ~QObject
    // delete them a second time when the registry is destroyed, which is a
    // double-free that only shows up at process teardown (heap corruption
    // reported by glibc while the dynamic loader unloads libGLX).
    if (iface->GetPluginInstances().isEmpty()) {
        if (createPluginObject(hash, nullptr))
            return iface->GetPluginInstances();
    }

    return iface->GetPluginInstances();
}

void PluginRegistry::ensureInitialized(QBasePluginObject* obj)
{
    if (!obj)
        return;

    static const char* kInitialized = "_daqster_capability_initialized";
    if (obj->property(kInitialized).toBool())
        return;

    // Mark first: a plugin object that throws out of Initialize() must not be
    // retried on every probe (which happens on every node placement).
    obj->setProperty(kInitialized, true);
    obj->Initialize();
}

QList<QObject*> PluginRegistry::instances(const char* iid)
{
    QList<QObject*> result;

    for (auto it = m_pluginMap.constBegin(); it != m_pluginMap.constEnd(); ++it) {
        for (QBasePluginObject* obj : capabilityInstances(it.value(), it.key())) {
            // Match BEFORE initializing: ensureInitialized() constructs whatever
            // the object's constructor owns, and this probe may be running in a
            // headless process that must not wake a GUI runtime host.
            if (obj && obj->qt_metacast(iid)) {
                ensureInitialized(obj);
                obj->setProperty("_daqster_hash", it.key());
                result.append(obj);
            }
        }
    }

    return result;
}

QList<Daqster::INodeProvider*> PluginRegistry::nodeProviders()
{
    QList<Daqster::INodeProvider*> result;

    for (auto it = m_pluginMap.constBegin(); it != m_pluginMap.constEnd(); ++it) {
        // INodeProvider is a non-QObject interface, so it cannot be matched by
        // qt_metacast()/Q_INTERFACES. dynamic_cast is the correct probe here.
        //
        // NOTE: deliberately NOT calling ensureInitialized() here. registerNodes()
        // is const and works on a bare object, and calling Initialize() would
        // construct runtime hosts this probe has no business waking up —
        // FrameworkGuiPluginObject builds a whole RuntimeShell (MDI + QtWidgets +
        // OpenGL) in its constructor, so probing for node providers in headless
        // mode would drag the entire GUI stack into a headless process.
        // Initialization is the job of the specific consumer (see
        // widgetProviders() / runtimeHosts()).
        for (QBasePluginObject* obj : capabilityInstances(it.value(), it.key())) {
            if (auto* provider = dynamic_cast<Daqster::INodeProvider*>(obj)) {
                obj->setProperty("_daqster_hash", it.key());
                result.append(provider);
            }
        }
    }

    return result;
}

QList<Daqster::IWidgetProvider*> PluginRegistry::widgetProviders()
{
    QList<Daqster::IWidgetProvider*> result;

    for (auto it = m_pluginMap.constBegin(); it != m_pluginMap.constEnd(); ++it) {
        // IWidgetProvider is a non-QObject interface, so it cannot be matched by
        // qt_metacast()/Q_INTERFACES. dynamic_cast is the correct probe here
        // (same reasoning as nodeProviders()).
        for (QBasePluginObject* obj : capabilityInstances(it.value(), it.key())) {
            if (auto* provider = dynamic_cast<Daqster::IWidgetProvider*>(obj)) {
                // The provider's whole job is createWidget(); without
                // Initialize() its widget factory does not exist yet.
                ensureInitialized(obj);
                obj->setProperty("_daqster_hash", it.key());
                result.append(provider);
            }
        }
    }

    return result;
}

QList<Daqster::IRuntimeHost*> PluginRegistry::runtimeHosts(Daqster::RuntimeMode mode)
{
    QList<Daqster::IRuntimeHost*> result;

    for (auto it = m_pluginMap.constBegin(); it != m_pluginMap.constEnd(); ++it) {
        // IRuntimeHost is a non-QObject interface, so dynamic_cast is the
        // correct probe (same reasoning as nodeProviders()).
        for (QBasePluginObject* obj : capabilityInstances(it.value(), it.key())) {
            auto* host = dynamic_cast<Daqster::IRuntimeHost*>(obj);
            if (host && host->runtimeMode() == mode) {
                ensureInitialized(obj);
                obj->setProperty("_daqster_hash", it.key());
                result.append(host);
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
