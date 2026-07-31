/************************************************************************
                        Daqster/QPluginManager.cpp - Copyright
Daqster software
Copyright (C) 2016, Vasil Vasilev,  Bulgaria

This file is part of Daqster and its software development toolkit.

Daqster is a free software; you can redistribute it and/or modify it
under the terms of the GNU Library General Public Licence as published by
the Free Software Foundation; either version 2 of the Licence, or (at
your option) any later version.

Daqster is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library
General Public Licence for more details.

Initial version of this file was created on 16.03.2017 at 11:40:20
**************************************************************************/
#include "LogCategories.h"
#include"QBasePluginObject.h"
#include "QPluginManager.h"
#include "PluginFilter.h"
#include "QPluginInterface.h"
#include "QPluginLoaderExt.h"
#include "PluginDiscovery.h"
#include "PluginRegistry.h"
#include "PluginPersistence.h"
#include "gui/QPluginManagerGui.h"

#include <QDir>
#include <QApplication>
#include <QSharedPointer>
#include <QStandardPaths>
#include<QFile>
#include <QFileInfo>
#include <QLibrary>
#include <QSet>
#include<QThread>
#include <QWidget>


namespace Daqster {

QPluginManager::QPluginManager (const QString &ConfigFile ) {
    // Config file setup — use writable location for AppImage compatibility.
    // Qt5 and Qt6 must use separate config files to avoid corruption when
    // both run simultaneously on the same machine.
    QString resolvedConfig;
    if (ConfigFile.isEmpty() || ConfigFile == "daqster.ini") {
        QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        QDir().mkpath(configDir);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        resolvedConfig = configDir + "/daqster_qt6.ini";
#else
        resolvedConfig = configDir + "/daqster_qt5.ini";
#endif
    } else {
        resolvedConfig = ConfigFile;
    }
    
    // Initialize split classes
    m_discovery = std::make_unique<PluginDiscovery>();
    m_persistence = std::make_unique<PluginPersistence>(resolvedConfig);
    m_registry = std::make_unique<PluginRegistry>();

    // Wire persistence callback — PluginRegistry uses this to save state
    m_registry->setPersistenceCallback([this](const PluginDescription& desc) {
        m_persistence->savePluginState(desc);
    });
    
    // Plugin directories — ordered by priority
    
    // 1. Build directory (highest priority for debug)
    m_discovery->addSearchPath(QDir(qApp->applicationDirPath()).absolutePath());
    m_discovery->addSearchPath(QDir(qApp->applicationDirPath()+"/plugins").absolutePath());
    m_discovery->addSearchPath(QDir(qApp->applicationDirPath()+"/../lib/daqster/plugins").absolutePath());
    
    // 2. Environment variable override
    const QString envDir = qgetenv("DAQSTER_PLUGIN_DIR");
    if (!envDir.isEmpty()) {
        m_discovery->addSearchPath(QDir(envDir).absolutePath());
    }
    
    // 2.1. Additional environment variable for multiple directories
    const QString additionalDirs = qgetenv("DAQSTER_PLUGIN_PATH");
    if (!additionalDirs.isEmpty()) {
        QStringList dirs = additionalDirs.split(QDir::listSeparator(), Qt::SkipEmptyParts);
        for(const QString& dir : dirs) {
            m_discovery->addSearchPath(QDir(dir).absolutePath());
        }
    }
    
    // 3. User plugins directory (platform-specific)
#ifdef Q_OS_WIN
    QString userPluginDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                            + "/Daqster/plugins";
    m_discovery->addSearchPath(QDir(userPluginDir).absolutePath());
    // 4. System plugins directory (Windows)
    m_discovery->addSearchPath(QDir("C:/Program Files/Daqster/plugins").absolutePath());
#else
    QString userPluginDir = QDir::homePath() + "/.local/share/daqster/plugins";
    m_discovery->addSearchPath(QDir(userPluginDir).absolutePath());
    // 4. System plugins directories (Unix, lowest priority)
    m_discovery->addSearchPath(QDir("/usr/lib/daqster/plugins").absolutePath());
    m_discovery->addSearchPath(QDir("/usr/local/lib/daqster/plugins").absolutePath());
#endif
    
    // Debug: Print all plugin search paths
    QList<QString> searchPaths = m_discovery->searchPaths();
    qCDebug(lcFramework) << "=== Plugin Search Paths ===";
    for (int i = 0; i < searchPaths.size(); ++i) {
        qCDebug(lcFramework) << QString("Path %1: %2").arg(i + 1).arg(searchPaths[i]);
    }
    qCDebug(lcFramework) << "=== End Plugin Search Paths ===";
    
    LoadPluginsInfoFromPersistency();
}

QPluginManager::~QPluginManager () {

}

/**
 * @brief QPluginManager::instance
 * @return
 */
QPluginManager *QPluginManager::instance()
{
    Q_ASSERT( QApplication::instance()->thread() == QThread::currentThread() );
    static QPluginManager instance;
    return &instance;
}

bool QPluginManager::Initialize()
{
    connect( QApplication::instance() ,SIGNAL(aboutToQuit()), QPluginManager::instance(),SLOT(ShutdownPluginManager()) );
    return true;
}

QBasePluginObject* QPluginManager::CreatePluginObject( const QString& KeyHash, QObject* Parent  )
{
    QPluginInterface* ObjInterface = m_registry->plugin(KeyHash);

    // If plugin not loaded yet, try to load from file
    if( nullptr == ObjInterface ){
        if( m_registry->containsDescription(KeyHash) && m_registry->pluginDescription(KeyHash).IsEnabled() ){
            PluginDescription::PluginHealthyState_t PersistentHealthy =
                static_cast<PluginDescription::PluginHealthyState_t>( m_registry->pluginDescription(KeyHash).GetProperty(PLUGIN_HELTHY_STATE).toUInt() );
            if( PluginDescription::ILL != PersistentHealthy ){
                if( !LoadPluginInterfaceObject( m_registry->pluginDescription(KeyHash).GetProperty(PLUGIN_LOCATION).toString(), KeyHash ) ){
                    qCDebug(lcFramework) << "Can't load plugin from file" << m_registry->pluginDescription(KeyHash).GetProperty(PLUGIN_LOCATION).toString();
                }
                ObjInterface = m_registry->plugin(KeyHash);
            }
        }
    }

    // Delegate to PluginRegistry (single source of truth for creation)
    QBasePluginObject* Object = m_registry->createPluginObject(KeyHash, Parent);

    // Connect signal for lifecycle management
    if( Object && ObjInterface ){
        connect( ObjInterface, SIGNAL(AllPluginObjectsDestroyed(QString)),
                 this, SLOT(AllPluginObjectsDestroyed(QString)) );
    }

    return Object;
}

void QPluginManager::EnableDisablePlugin( const QString &Hash, bool Enable )
{
    Daqster::PluginDescription Desc = m_registry->pluginDescription(Hash);
    if( !Desc.IsEmpty() ){
       if( Enable != Desc.IsEnabled() ){
          Desc.Enable( Enable );
          m_registry->setPluginDescription(Hash, Desc);

          Daqster::QPluginInterface* object = m_registry->plugin(Hash);
          if( nullptr != object ){
              object->Enable( Enable );
          }
          m_persistence->savePluginState( Desc );
          if( !Enable )
          {
            ShutdownPlugin( Hash );
          }
          emit PluginsListChangeDetected();
       }
    }
}

void QPluginManager::EnableDisablePluginList(const QList<QString> &HashList, bool Enable)
{
    for (const QString& hash : HashList) {
        EnableDisablePlugin(hash, Enable);
    }
}


/**
 * Return list with founded plugins. Return list can be filtered by criteria
 * described in input filter parameter.
 * @param  Filter Plugin filtration object
 */
QList<Daqster::PluginDescription> QPluginManager::GetPluginList( const Daqster::PluginFilter& Filter )
{
    QList<PluginDescription> List  = m_registry->allDescriptions().values();
    
    // Deduplicate by file location (PLUGIN_LOCATION) to prevent double entries of the same physical file (persistency vs discovery hash mismatch)
    QMap<QString, PluginDescription> uniqueMap;
    for (const PluginDescription& desc : List) {
        QString location = desc.GetProperty(PLUGIN_LOCATION).toString();
        QString hash = desc.GetProperty(PLUGIN_HASH).toString();
        QString key = !location.isEmpty() ? location : hash;
        
        if (uniqueMap.contains(key)) {
            QString existingLoc = uniqueMap[key].GetProperty(PLUGIN_LOCATION).toString();
            if (existingLoc.contains("/tmp/") && !location.contains("/tmp/")) {
                uniqueMap[key] = desc;
            }
        } else {
            uniqueMap[key] = desc;
        }
    }

    QList<PluginDescription> dedupedList = uniqueMap.values();
    auto it = dedupedList.begin();
    while( it != dedupedList.end() ){
        if( !Filter.IsFiltered( *it ) ){
            it = dedupedList.erase(it);
        }
        else{
            it++;
        }
    }
    return dedupedList;
}

PluginDescription QPluginManager::GetPluginDescriptionByHash(const QString &Hash)
{
    return m_registry->pluginDescription(Hash);
}


/**
 * Search for plugins in configured directories.
 */
void QPluginManager::SearchForPlugins ()
{
    bool Changed = false;

    // 1. Remove stale plugins (hash mismatch)
    QList<QString> hashes = m_registry->registeredHashes();
    for (const QString& Hash : hashes) {
        QPluginInterface* ObjInterface = m_registry->plugin(Hash);
        if (nullptr != ObjInterface) {
            QString cHash;
            PluginDiscovery::computeFileHash(ObjInterface->GetLocation(), cHash);
            if (0 != cHash.compare(Hash)) {
                qCDebug(lcFramework) << "Plugin " << ObjInterface->GetName() << "was removed from location: " << ObjInterface->GetLocation();
                ObjInterface = m_registry->takePlugin(Hash);
                m_registry->removeDescription(Hash);
                ObjInterface->deleteLater();
                m_persistence->removePlugin(Hash);
                Changed = true;
            }
        }
    }

    // 2. Discover new plugins
    QMap<QString, QString> discovered = m_discovery->discoverPlugins(m_registry->allDescriptions());
    for (auto it = discovered.constBegin(); it != discovered.constEnd(); ++it) {
        if (LoadPluginInterfaceObject(it.value(), it.key())) {
            Changed = true;
        } else {
            qCDebug(lcFramework) << "Can't Load plugin from file" << it.value();
        }
    }

    // Debug dump
    qCDebug(lcFramework) << "Begin registered hashes";
    for (const QString& Hash : m_registry->registeredHashes()) {
        qCDebug(lcFramework) << Hash;
    }
    qCDebug(lcFramework) << "End registered hashes";

    if (Changed) {
        emit PluginsListChangeDetected();
    }
}


/**
 * Add directory to plugin search path
 * @param  Directory Directory path.
 */
void QPluginManager::AddPluginsDirectory (const QString& Directory)
{
    m_discovery->addSearchPath(Directory);
}


// ShowPluginManagerGui — direct instantiation (GUI is part of frame_work)
void QPluginManager::ShowPluginManagerGui(QWidget *Parent)
{
    auto* dlg = new QPluginManagerGui(Parent);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}

/**
 * @brief QPluginManager::LoadPluginsInfoFromPersistency Load plugins information from persistency
 */
void QPluginManager::LoadPluginsInfoFromPersistency()
{
    QMap<QString, PluginDescription> loaded = m_persistence->loadPlugins();
    QMap<QString, PluginDescription> validDescriptions;
    QSet<QString> seenLocations;

    for (auto it = loaded.constBegin(); it != loaded.constEnd(); ++it) {
        const QString& hash = it.key();
        const PluginDescription& desc = it.value();
        QString location = desc.GetProperty(PLUGIN_LOCATION).toString();

        if (location.isEmpty() || !QFileInfo::exists(location)) {
            qCDebug(lcFramework) << "Removing stale plugin from persistency (file missing):" << location << "hash:" << hash;
            m_persistence->removePlugin(hash);
            continue;
        }

        if (seenLocations.contains(location)) {
            bool isTmp = location.contains("/tmp/");
            if (isTmp) {
                m_persistence->removePlugin(hash);
                continue;
            }
        }

        seenLocations.insert(location);
        validDescriptions[hash] = desc;
    }

    m_registry->setPluginDescriptions(validDescriptions);
}


/**
 * @brief This slot can be connected to QPluginInterface signal AllPluginObjectDestroyed in order
 * to automaticaly unload plugin.
 * @param Hash
 */
void QPluginManager::AllPluginObjectsDestroyed(const QString &Hash)
{
    QPluginInterface* OIface = m_registry->plugin(Hash);
    if( nullptr != OIface )
    {
        OIface = m_registry->takePlugin(Hash);
        OIface->deleteLater();
    }
}

void QPluginManager::ShutdownPlugin( const QString &Hash )
{
    m_registry->shutdownPlugin(Hash);
}

void QPluginManager::ShutdownPluginManager()
{
    static bool s_shutdownDone = false;
    if (s_shutdownDone) return;
    s_shutdownDone = true;
    QPluginLoaderExt::setShuttingDown(true);
    m_registry->shutdownAll();
}


bool QPluginManager::LoadPluginInterfaceObject( const QString& PluginFileName, const QString& Hash  )
{

    QPluginInterface* ObjInterface = nullptr;
    bool ret = false;
    QSharedPointer<QPluginLoaderExt> pluginLoader( new QPluginLoaderExt(PluginFileName), &QObject::deleteLater );
    /*All symbols are resolved in load time*/
    pluginLoader->setLoadHints( QLibrary::ResolveAllSymbolsHint );
    QObject* Inst = pluginLoader->instance();
    qCDebug(lcFramework) << "PLUGIN METADATA: \n\t" << pluginLoader->metaData();
    if( nullptr != Inst )
    {
        ObjInterface = dynamic_cast<Daqster::QPluginInterface*>(Inst);
        if( nullptr != ObjInterface )
        {
            ObjInterface->SetPluginLoader( pluginLoader );
            ObjInterface->SetLocation( PluginFileName );
            ObjInterface->SetHash( Hash );
            ObjInterface->SetHealthyState(PluginDescription::IF_LOADED);

            // Register in registry
            m_registry->registerPlugin(Hash, ObjInterface);

            // Preserve the persisted enabled state
            bool wasEnabled = m_registry->containsDescription(Hash)
                ? m_registry->pluginDescription(Hash).IsEnabled()
                : true;
            PluginDescription desc = ObjInterface->GetPluginDescriptor();
            desc.Enable(wasEnabled);
            m_registry->setPluginDescription(Hash, desc);
            ObjInterface->Enable(wasEnabled);

            connect( ObjInterface, SIGNAL(AllPluginObjectsDestroyed(QString)), this, SLOT(AllPluginObjectsDestroyed(QString)) );
            ret = true;
        }
        else{
            qCDebug(lcFramework) << "Bad Plugin '" << PluginFileName << "'Try to unload resources";
            if( pluginLoader->unload() ){
                qCDebug(lcFramework) << "Bad Plugin '" << PluginFileName << "' Unloaded successfully";
            }
            else{
                qCDebug(lcFramework) << "Bad Plugin '" << PluginFileName << "' Can't unload !!??";
            }
        }
    }
    else{
        qCDebug(lcFramework) << "Bad Plugin '" << PluginFileName << "' Can't be loaded ";
        qCDebug(lcFramework) << pluginLoader->errorString();
    }
    return ret;
}

/**
 * Return all plugin instances that implement a given interface (by IID).
 * Creates instances lazily for enabled plugins that have no instances yet.
 * Each returned QObject has a dynamic property "_daqster_hash" with its plugin hash.
 */
QObjectList QPluginManager::instances(const char* iid)
{
    return m_registry->instances(iid);
}

}//End of Daqster namespace

