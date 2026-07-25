/************************************************************************
                        Daqster/QPluginManager.cpp.cpp - Copyright
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
#include "debug.h"
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
#include<QThread>
#include <QWidget>


namespace Daqster {
// Constructors/Destructors
//
 QPluginManager* QPluginManager::g_Instance =  nullptr;

QPluginManager::QPluginManager (const QString &ConfigFile ) {
    // Config file setup - използвай writable location за AppImage compatibility
    QString resolvedConfig;
    if (ConfigFile.isEmpty() || ConfigFile == "daqster.ini") {
        QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        QDir().mkpath(configDir);
        resolvedConfig = configDir + "/daqster.ini";
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
    
    // Plugin directories - подредени по приоритет
    
    // 1. Build директория (най-висок приоритет за дебъг)
    m_discovery->addSearchPath( qApp->applicationDirPath() );
    m_discovery->addSearchPath( qApp->applicationDirPath()+QString("/plugins") );
    m_discovery->addSearchPath(QDir(qApp->applicationDirPath()+"/../lib/daqster/plugins").absolutePath());
    
    // 2. Environment variable override (най-висок приоритет)
    const QString envDir = qgetenv("DAQSTER_PLUGIN_DIR");
    if (!envDir.isEmpty()) {
        m_discovery->addSearchPath(QDir(envDir).absolutePath());
    }
    
    // 2.1. Additional environment variable for multiple directories
    const QString additionalDirs = qgetenv("DAQSTER_PLUGIN_PATH");
    if (!additionalDirs.isEmpty()) {
        QStringList dirs = additionalDirs.split(":", Qt::SkipEmptyParts);
        for(const QString& dir : dirs) {
            m_discovery->addSearchPath(QDir(dir).absolutePath());
        }
    }
    
    // 3. User plugins directory
    QString userPluginDir = QDir::homePath() + "/.local/share/daqster/plugins";
    m_discovery->addSearchPath(QDir(userPluginDir).absolutePath());
    
    // 4. System plugins directories (най-нисък приоритет)
    m_discovery->addSearchPath("/usr/lib/daqster/plugins");
    m_discovery->addSearchPath("/usr/local/lib/daqster/plugins");
    
    // Debug: Print all plugin search paths
    QList<QString> searchPaths = m_discovery->searchPaths();
    qDebug() << "=== Plugin Search Paths ===";
    for (int i = 0; i < searchPaths.size(); ++i) {
        qDebug() << QString("Path %1: %2").arg(i + 1).arg(searchPaths[i]);
    }
    qDebug() << "=== End Plugin Search Paths ===";
    
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
    assert( QApplication::instance()->thread() == QThread::currentThread() );
    if(  nullptr == g_Instance ){
        g_Instance = new QPluginManager();
    }
    assert( g_Instance != nullptr );
    return g_Instance;
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
            PluginDescription::PluginHealtyState_t PersistentHealthy =
                (PluginDescription::PluginHealtyState_t) m_registry->pluginDescription(KeyHash).GetProperty(PLUGIN_HELTHY_STATE).toUInt();
            if( PluginDescription::ILL != PersistentHealthy ){
                if( !LoadPluginInterfaceObject( m_registry->pluginDescription(KeyHash).GetProperty(PLUGIN_LOCATION).toString(), KeyHash ) ){
                    DEBUG << "Can't load plugin from file" << m_registry->pluginDescription(KeyHash).GetProperty(PLUGIN_LOCATION).toString();
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
    auto it = List.begin();
    while( it != List.end() ){
        if( !Filter.IsFiltered( *it ) ){
            it = List.erase(it);
        }
        else{
            it++;
        }
    }
    return List;
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
                DEBUG << "Plugin " << ObjInterface->GetName() << "was removed from location: " << ObjInterface->GetLocation();
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
            DEBUG << "Can't Load plugin from file" << it.value();
        }
    }

    // Debug dump
    DEBUG << "Begin registered hashes";
    for (const QString& Hash : m_registry->registeredHashes()) {
        DEBUG << Hash;
    }
    DEBUG << "End registered hashes";

    if (true == Changed) {
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
    m_registry->setPluginDescriptions(m_persistence->loadPlugins());
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
        delete OIface;
    }
}

void QPluginManager::ShutdownPlugin( const QString &Hash )
{
    m_registry->shutdownPlugin(Hash);
}

void QPluginManager::ShutdownPluginManager()
{
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
    qDebug() << "PLUGIN METADATA: \n\t" << pluginLoader->metaData();
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
            DEBUG << "Bad Plugin '" << PluginFileName << "'Try to unload resources";
            if( pluginLoader->unload() ){
                DEBUG << "Bad Plugin '" << PluginFileName << "' Unloaded successfully";
            }
            else{
                DEBUG << "Bad Plugin '" << PluginFileName << "' Can't unload !!??";
            }
        }
    }
    else{
        DEBUG << "Bad Plugin '" << PluginFileName << "' Can't be loaded ";
        DEBUG << pluginLoader->errorString();
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

