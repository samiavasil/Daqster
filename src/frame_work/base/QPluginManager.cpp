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
#include "base/debug.h"
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

QPluginManager::QPluginManager (const QString &ConfigFile ) {
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
void QPluginManager::GetPluginList (Daqster::PluginFilter Filter)
{
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
    QDir pluginsDir;
    QSettings settings( m_ConfigFile, QSettings::IniFormat );
    settings.setIniCodec("UTF-8");
    settings.beginGroup("Plugins");
    foreach ( QString path, m_DirList ) {
        if( pluginsDir.cd( path ) )
        {
        //    settings.beginGroup(path);
            QString Hash;
            foreach (QString fileName, pluginsDir.entryList(QDir::Files)) {

                fileName = pluginsDir.absoluteFilePath( fileName );
                if(  false == m_PluginMap.contains( fileName ) ){
                    QSharedPointer<QPluginLoader> pluginLoader( new QPluginLoader(fileName));
                    QObject* Inst = pluginLoader->instance();
                    if( NULL != Inst ){

                        Daqster::QPluginObjectsInterface* ObjInterface = dynamic_cast<Daqster::QPluginObjectsInterface*>(Inst);
                        if( NULL != ObjInterface ){
                            FileHash( fileName,  Hash  );
                            DEBUG << "<"+Hash+">";
                            ObjInterface->SetPluginLoader( pluginLoader );
                            ObjInterface->SetLocation( fileName );
                            ObjInterface->SetHash( Hash );
                            ObjInterface->StorePluginParamsToPersistency( settings );

                            m_PluginMap[fileName] = ObjInterface;
                            Daqster::QBasePluginObject* Object = ObjInterface->CreatePlugin();
                            if( NULL != Object ){
                                DEBUG << "Plugin with name " << ObjInterface->GetName() << "created successfully";
                            }
                        }
                        else if( NULL != Inst ){
                            Inst->deleteLater();
                        }
                    }
                }
            }
        //    settings.endGroup();
        }
    }
    settings.endGroup();
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

}


}//End of Daqster namespace

