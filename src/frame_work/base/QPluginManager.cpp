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
    QBasePluginObject* Object = NULL;
    QPluginObjectsInterface* ObjInterface = m_PluginMap.value( KeyHash, NULL );
    if( NULL == ObjInterface ){
        if( m_PluginsHashDescMap.contains( KeyHash ) ){
            if( !LoadPluginInterfaceObject( m_PluginsHashDescMap[KeyHash].GetLocation(), KeyHash ) ){
                DEBUG << "Can't load plugin from file" << m_PluginsHashDescMap[KeyHash].GetLocation();
            }
        }
    }
    ObjInterface = m_PluginMap.value( KeyHash, NULL );
    if( NULL != ObjInterface ){
        Object = ObjInterface->CreatePlugin();
    }
    return Object;
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
                    if(  false == m_PluginMap.contains( Hash ) )
                    {
                        if( !LoadPluginInterfaceObject( fileName, Hash ) ){
                            DEBUG << "Can' Load plugin from file" << fileName;
                        }
                    }
                    ObjInterface = m_PluginMap.value( Hash, NULL );
                    if( NULL != ObjInterface )
                    {
                        ObjInterface->StorePluginParamsToPersistency( settings );
                        m_PluginsHashDescMap[Hash] = ObjInterface->GetPluginDescriptor();
                    }
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
    DEBUG << "End m_PluginsHashDescMap Hashes";
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
            settings.beginGroup( Name );
            Desk.GetPluginParamsFromPersistency( settings );
            if( !m_PluginsHashDescMap.contains( Desk.GetHash() ) )
            {
                /*Check is this plugin file still exist*/
                if( (!Desk.GetHash().isEmpty()) && true == FileHash( Desk.GetLocation(),  Hash  ) )
                {
                    if( 0 == Hash.compare(Desk.GetHash()) )
                    {
                         m_PluginsHashDescMap[Desk.GetHash()] = Desk;
                         added = true;
                    }
                }
            }
            if( true != added )
            {
                DEBUG << "Hm..Remove PluginDescription record: Hash '" << Desk.GetHash() << "'.";
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
            QObject* Inst = pluginLoader->instance();
            if( NULL != Inst )
            {
                ObjInterface = dynamic_cast<Daqster::QPluginObjectsInterface*>(Inst);
                if( NULL != ObjInterface )
                {
                    ObjInterface->SetPluginLoader( pluginLoader );
                    ObjInterface->SetLocation( PluginFileName );
                    ObjInterface->SetHash( Hash );                    
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

