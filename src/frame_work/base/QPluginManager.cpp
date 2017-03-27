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

namespace Daqster {
// Constructors/Destructors
//  

QPluginManager::QPluginManager (const QString &ConfigFile ):m_Mutex(QMutex::Recursive) {
    m_ConfigFile = ConfigFile;
    m_DirList.append( qApp->applicationDirPath()+QString("/plugins") );
    LoadPluginsInfoFromPersistency();
}

QPluginManager::~QPluginManager () { }


/**
 * Return list with founded plugins. Return list can be filtered by criteria
 * described in input filter parameter.
 * @param  Filter Plugin filtration object
 */
QList<PluginDescription> QPluginManager::GetPluginList( const Daqster::PluginFilter& Filter )
{
    QList<PluginDescription> List;
    m_Mutex.lock();
    //TODO
    m_Mutex.unlock();
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
    m_Mutex.lock();
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
            QString Hash;
            foreach (QString fileName, PluginsDir.entryList(QDir::Files)) {
                ObjInterface = NULL;
                fileName = PluginsDir.absoluteFilePath( fileName );
                FileHash( fileName,  Hash  );
                if( !m_PluginsHashDescMap.contains(Hash) )
                {

                    if(  false == m_PluginMap.contains( Hash ) )
                    {
                        QSharedPointer<QPluginLoader> pluginLoader( new QPluginLoader(fileName));
                        QObject* Inst = pluginLoader->instance();
                        if( NULL != Inst )
                        {
                            ObjInterface = dynamic_cast<Daqster::QPluginObjectsInterface*>(Inst);
                            if( NULL != ObjInterface )
                            {
                                DEBUG << "<" + Hash + ">";
                                ObjInterface->SetPluginLoader( pluginLoader );
                                ObjInterface->SetLocation( fileName );
                                ObjInterface->SetHash( Hash );
                                ObjInterface->StorePluginParamsToPersistency( settings );
                                m_PluginMap[ Hash ] = ObjInterface;
                            }
                            else if( NULL != Inst )
                            {
                                Inst->deleteLater();
                            }
                        }
                    }
                    ObjInterface = m_PluginMap.value( Hash, NULL );
                    if( NULL != ObjInterface )
                    {
                        m_PluginsHashDescMap[Hash] = ObjInterface->GetPluginDescriptor();
                        ObjInterface->CreatePlugin();
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
    m_Mutex.unlock();
}


/**
 * Add directory to plugin search path
 * @param  Directory Directory path.
 */
void QPluginManager::AddPluginsDirectory (const QString& Directory)
{
    m_Mutex.lock();
    if( !m_DirList.contains(Directory) ){
        m_DirList.append( Directory );
    }
    m_Mutex.unlock();
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
    m_Mutex.lock();
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
    m_Mutex.unlock();
}

//typedef Singleton<QPluginManager>  PluginManager;
QPluginManager& GetApplicationPluginManager()
{
    static QPluginManager man;
    return man;
    //return PluginManager::instance();
}


}//End of Daqster namespace

