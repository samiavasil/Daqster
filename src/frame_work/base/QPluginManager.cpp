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
#include "QPluginManager.h"
#include "QPluginManagerGui.h"
#include "QPluginListView.h"
#include "PluginFilter.h"
#include "QPluginObjectsInterface.h"
#include <QDir>
#include <QApplication>
#include <QSharedPointer>
#include <QPluginLoader>
#include <QSettings>
#include<QCryptographicHash>
#include<QFile>
#include<QMessageBox>

#include<QThread>
#include"QBasePluginObject.h"

namespace Daqster {
// Constructors/Destructors
//  
 QPluginManager* QPluginManager::g_Instance =  NULL;

QPluginManager::QPluginManager (const QString &ConfigFile ) {
    m_ConfigFile = ConfigFile;
    m_DirList.append( qApp->applicationDirPath()+QString("/plugins") );
    LoadPluginsInfoFromPersistency();
}

QPluginManager::~QPluginManager () { }

/**
 * @brief QPluginManager::instance
 * @return
 */
QPluginManager *QPluginManager::instance()
{
    assert( QApplication::instance()->thread() == QThread::currentThread() );
    if(  NULL == g_Instance ){
        g_Instance = new QPluginManager();
    }
    assert( g_Instance != NULL );
    return g_Instance;
}

QBasePluginObject* QPluginManager::CreatePluginObject( const QString& KeyHash  )
{
    PluginDescription::PluginHealtyState_t PersistentHealthy = PluginDescription::UNDEFINED;
    QBasePluginObject* Object = NULL;
    QPluginObjectsInterface* ObjInterface = m_PluginMap.value( KeyHash, NULL );

    if( NULL == ObjInterface ){
        if( m_PluginsHashDescMap.contains( KeyHash ) ){
            PersistentHealthy = (PluginDescription::PluginHealtyState_t) m_PluginsHashDescMap[KeyHash].GetProperty(PLUGIN_HELTHY_STATE).toUInt();
            if( PluginDescription::ILL != PersistentHealthy ){
                if( !LoadPluginInterfaceObject( m_PluginsHashDescMap[KeyHash].GetProperty(PLUGIN_LOCATION).toString(), KeyHash ) ){
                    DEBUG << "Can't load plugin from file" << m_PluginsHashDescMap[KeyHash].GetProperty(PLUGIN_LOCATION).toString();
                }

                ObjInterface = m_PluginMap.value( KeyHash, NULL );
            }
        }
    }

    if( NULL != ObjInterface ){
        PluginDescription::PluginHealtyState_t Healthy = ObjInterface->GetHealthyState();
        if( PersistentHealthy == PluginDescription::UNDEFINED){
            PersistentHealthy = Healthy;
        }
        if( Healthy != PluginDescription::ILL ){

             if( PluginDescription::OBJECT_CREATION == PersistentHealthy ){
                 /*Previous time application is failed on loading of this plugin.
                   Give the second chance. If it fail -> this plugin is marked as ILL and
                   will be disabled.
                 */
                 PersistentHealthy =  PluginDescription::ILL;
                 ObjInterface->SetHealthyState( PersistentHealthy );
                 StorePluginStateToPersistncy(ObjInterface);
                 QMessageBox::warning(NULL, tr("Attention"),
                                                  tr("There was application crach on last time loading of plugin %1 .\n"
                                                     "Now we try second time and if it fail the plugin will be disabled.\n"
                                                     "To enable Plugin please change HealthyState state in configuration .ini file."
                                                     ).arg( ObjInterface->GetLocation() ),
                                                  QMessageBox::Ok);
                 DEBUG << "Second chance for loading of plugin: " << ObjInterface->GetLocation() << ". If it fail it will be disabled.";
             }else if( Healthy == PluginDescription::IF_LOADED ){
                 Healthy = PluginDescription::OBJECT_CREATION;
                 ObjInterface->SetHealthyState( Healthy );
                 /*Store Persistent Plugin status*/
                 StorePluginStateToPersistncy(ObjInterface);
             }

             Object = ObjInterface->CreatePlugin();
             if( NULL != Object ){
                Healthy = PluginDescription::HEALTHY;
             }
             else{
                Healthy = PluginDescription::ILL;
             }
        }

        if( ObjInterface->GetHealthyState() != Healthy ){
            ObjInterface->SetHealthyState( Healthy );
            /*Store Persistent Plugin status*/
            StorePluginStateToPersistncy(ObjInterface);
        }
    }
    return Object;
}


void QPluginManager::StorePluginStateToPersistncy( QPluginObjectsInterface* ObjInterface  ){
    if( NULL != ObjInterface ){
        QSettings settings( m_ConfigFile, QSettings::IniFormat );
        settings.beginGroup( "Plugins" );
        settings.beginGroup( ObjInterface->GetHash() );
        ObjInterface->StorePluginParamsToPersistency( settings );
        settings.endGroup();
        settings.endGroup();
    }
}

/**
 * Return list with founded plugins. Return list can be filtered by criteria
 * described in input filter parameter.
 * @param  Filter Plugin filtration object
 */
QList<Daqster::PluginDescription> QPluginManager::GetPluginList( const Daqster::PluginFilter& Filter )
{
    QList<PluginDescription> List  = m_PluginsHashDescMap.values();
    /* Current implementation fo plugin filtration
       TODO: TBD Maybe something as concurrent filtering will be
             good here -> QtConcurrent::blockingFilter(fooList, bad);
             For now just check IsFiltered.
    */
    auto it = List.begin();
    while( it != List.end() ){
        if( Filter.IsFiltered( *it ) ){
            it = List.erase(it);
        }
        else{
            it++;
        }
    }
    return List;
}

/**
 * This function create PlunListView widget. This function internaly (on PluginView
 * creation) create signal/slot connection betwen PluginManager and PluginViews in
 * order to have dynamic refresh of vies if new plugins are loaded/finded..
 * @return Daqster::QPluginListView*
 * @param  Parrent Pointer to parent QWidget.
 * @param  Filter Optional list filter.
 */
Daqster::QPluginListView*  QPluginManager::CreatePluginListView (QWidget* Parrent, PluginFilter *Filter )
{
}


/**
 * Search for plugins in configured directories.
 */
void QPluginManager::SearchForPlugins ()
{
    QDir PluginsDir;
    QSettings settings( m_ConfigFile, QSettings::IniFormat );
    Daqster::QPluginObjectsInterface* ObjInterface = NULL;
    bool Changed = false;
    //  settings.setIniCodec("UTF-8");
    settings.beginGroup("Plugins");
    foreach (QString Hash, m_PluginMap.keys()) {
        ObjInterface = m_PluginMap.value( Hash, NULL );
        /*Check is this plugin file still exist*/
        if( NULL != ObjInterface )
        {
            QString cHash;
             FileHash( ObjInterface->GetLocation(),  cHash  );
            {
                if( 0 != cHash.compare( Hash ) )
                {
                    DEBUG << "Plugin " << ObjInterface->GetName() << "was removed from location: " << ObjInterface->GetLocation();
                    m_PluginsHashDescMap.remove(Hash);
                    ObjInterface = m_PluginMap.take( Hash );
                    ObjInterface->deleteLater();
                    settings.beginGroup( Hash );
                    settings.remove( "" );
                    settings.endGroup();
                    Changed = true;
                }
            }
        }
    }

    foreach ( QString path, m_DirList ) {
        if( PluginsDir.cd( path ) )
        {
            foreach (QString fileName, PluginsDir.entryList(QDir::Files)) {
                fileName = PluginsDir.absoluteFilePath( fileName );
                QString Hash;
                FileHash( fileName,  Hash  );
                if( !m_PluginsHashDescMap.contains(Hash) )
                {
                    settings.beginGroup( Hash );
                    if(  false == m_PluginMap.contains( Hash ) )
                    {
                        if( !LoadPluginInterfaceObject( fileName, Hash ) ){
                            DEBUG << "Can' Load plugin from file" << fileName;
                        }
                        Changed = true;
                    }
                    ObjInterface = m_PluginMap.value( Hash, NULL );
                    if( NULL != ObjInterface )
                    {
                        ObjInterface->StorePluginParamsToPersistency( settings );
                        m_PluginsHashDescMap[Hash] = ObjInterface->GetPluginDescriptor();
                    }
                    settings.endGroup();
                }
            }
        }
    }
    settings.endGroup();
    //Dump
    DEBUG << "Begin m_PluginMap Hashes";
    foreach (QString Hash, m_PluginMap.keys()) {
        DEBUG << Hash;
    }
    DEBUG << "End m_PluginMap Hashes";
    DEBUG << "Begin m_PluginsHashDescMap Hashes";
    foreach (QString Hash, m_PluginsHashDescMap.keys()) {
        DEBUG << Hash;
    }

    if( true == Changed ){
        emit PluginsListChangeDetected();
    }
}


/**
 * Add directory to plugin search path
 * @param  Directory Directory path.
 */
void QPluginManager::AddPluginsDirectory (const QString& Directory)
{
    if( !m_DirList.contains(Directory) ){
        m_DirList.append( Directory );
    }
}


/**
 * Show plugin manager GUI widget. In this GUI you can see available plugins,
 * rescan for new plugins, dynamic unload , enable/disable plugin loading.
 */
void QPluginManager::ShowPluginManagerGui ()
{
    QPluginManagerGui Dialog;
    Dialog.exec();
}

/**
 * @brief FileHash calculate Hash of some file
 * @param Filename
 * @param Hash result
 * @return true on success
 *         false otherwise
 */
bool QPluginManager::FileHash( const QString& Filename, QString& Hash  )
{
    bool ret = false;
    QCryptographicHash hashMaster( QCryptographicHash::Md5 );
    QFile file(Filename);
    if( file.open(QIODevice::ReadOnly|QIODevice::Text ) )
    {
        if( hashMaster.addData( &file ) )/*File content for hash*/
        {
            Hash = QString(hashMaster.result().toHex().data());
            ret = true;
        }
    }
    else
    {
        Hash = QString();
    }
    return ret;
}

/**
 * @brief QPluginManager::LoadPluginsInfoFromPersistency Load plugins information from persistency
 */
void QPluginManager::LoadPluginsInfoFromPersistency()
{
    QSettings settings( m_ConfigFile, QSettings::IniFormat );
    m_PluginsHashDescMap.clear();
    if( settings.childGroups().contains( "Plugins" ))
    {
        QString Hash;
        settings.beginGroup("Plugins");
        foreach ( QString Name, settings.childGroups() ) {
            PluginDescription Desk;
            bool added = false;
            QString PersHash;
            settings.beginGroup( Name );
            Desk.GetPluginParamsFromPersistency( settings );
            PersHash = Desk.GetProperty( PLUGIN_HASH ).toString();
            if( !m_PluginsHashDescMap.contains( PersHash ) )
            {
                /*Check is this plugin file still exist*/
                if( (!PersHash.isEmpty()) && true == FileHash( Desk.GetProperty( PLUGIN_LOCATION ).toString(),  Hash  ) )
                {
                    if( 0 == Hash.compare(PersHash) )
                    {
                         m_PluginsHashDescMap[PersHash] = Desk;
                         added = true;
                    }
                }
            }
            if( true != added )
            {
                DEBUG << "Hm..Remove PluginDescription record: Hash '" << PersHash << "'.";
                settings.remove( "" );
            }
            settings.endGroup();
        }
        settings.endGroup();
    }
}

bool QPluginManager::LoadPluginInterfaceObject( const QString& PluginFileName, const QString& Hash  )
{

    QPluginObjectsInterface* ObjInterface = NULL;
    bool ret = false;
            QSharedPointer<QPluginLoader> pluginLoader( new QPluginLoader(PluginFileName));
            /*All symbols are resolved in load time*/
            pluginLoader->setLoadHints( QLibrary::ResolveAllSymbolsHint );
            QObject* Inst = pluginLoader->instance();
            if( NULL != Inst )
            {
                ObjInterface = dynamic_cast<Daqster::QPluginObjectsInterface*>(Inst);
                if( NULL != ObjInterface )
                {
                    ObjInterface->SetPluginLoader( pluginLoader );
                    ObjInterface->SetLocation( PluginFileName );
                    ObjInterface->SetHash( Hash );
                    ObjInterface->SetHealthyState(PluginDescription::IF_LOADED);
                    m_PluginMap[ Hash ] = ObjInterface;
                    ret = true;
                }
                else if( NULL != Inst )
                {
                    Inst->deleteLater();
                }
            }
      return ret;
}

}//End of Daqster namespace

