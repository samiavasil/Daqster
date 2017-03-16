/************************************************************************
                        Daqster/QPluginObjectsInterface.cpp - Copyright vvasilev
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

Initial version of this file was created on нд март 12 2017 at 20:54:50
**************************************************************************/

#include "QPluginObjectsInterface.h"
#include "QBasePluginObject.h"
#include "base/debug.h"
#include <QPluginLoader>


namespace Daqster {

/**
 * Empty Constructor
 * @param  parent
 */
QPluginObjectsInterface::QPluginObjectsInterface (  QObject* Parent ):QObject(Parent) {

}

QPluginObjectsInterface::~QPluginObjectsInterface () {

}

/**
 * Return plugin basic type. If this isn't set to some type you can check typeName
 * string and try to detect type from name.
 * @return Daqster::PluginType_t
 */
Daqster::PluginType_t QPluginObjectsInterface::GetType ()
{
    return m_PluginType;
}


/**
 * Return plugin embeded icon.
 * @return const QIcon
 */
const QIcon& QPluginObjectsInterface::GetIcon ()
{
    return m_Icon;
}


/**
 * Return plugin name
 * @return const QString&
 */
const QString& QPluginObjectsInterface::GetName ()
{
    return  m_Name;
}


/**
 * Get plugin type name
 * @return const QString&
 */
const QString& QPluginObjectsInterface::GetTypeName ()
{
    return m_PluginTypeName;
}


/**
 * Get plugin version
 * @return const QString&
 */
const QString& QPluginObjectsInterface::GetVersion ()
{
    return m_Version;
}


/**
 * Get plugin description
 * @return const QString&
 */
const QString& QPluginObjectsInterface::GetDescription ()
{
    return m_Description;
}


/**
 * Get plugin detail description.
 * @return const QString&
 */
const QString& QPluginObjectsInterface::GetDetailDescription ()
{
    return m_DetailDescription;
}


/**
 * Get plugin license
 * @return const QString&
 */
const QString& QPluginObjectsInterface::GetLicense ()
{
    return m_License;
}


/**
 * Return plugin author
 * @return const QString&
 */
const QString& QPluginObjectsInterface::GetAuthor ()
{
    return m_Author;
}


/**
 * Set new plugin loader.
 * When the plugin is loaded on first time we create QPluginLoader and its method
 * instance() returns QPluginObjectInterface*  plugInterface. On this point 
 * plugInterface->setPluginLoader() function is called to set pointer to
 * QPluginLoader.
 * @param  Loader New plugin loader
 */
void QPluginObjectsInterface::SetPluginLoader (QSharedPointer<QPluginLoader> & Loader)
{
    m_PluginLoader = Loader;
}

/**
 * Create  new plugin object.
 * @return Daqster::QBasePluginObject*
 * @param  Parrent Pointer to parent QObject
 */
Daqster::QBasePluginObject* QPluginObjectsInterface::CreatePlugin (QObject* Parrent)
{
    QBasePluginObject * obj = CreatePluginInternal( Parrent );
    if( NULL != obj ){
        m_PluginInstList.append( obj );
        connect( obj, SIGNAL(destroyed(QObject*)), this, SLOT(pluginInstanceDestroyed(QObject*)) );
    }
    return obj;
}

void QPluginObjectsInterface::pluginInstanceDestroyed( QObject *obj )
{
    QBasePluginObject* Plugin = (QBasePluginObject*)(obj);//TODO:?
    if( NULL != Plugin ){
        DEBUG << "Remove Plugins num " << m_PluginInstList.removeAll( Plugin );
        DEBUG << "Not destroyed Plugins count " << m_PluginInstList.count();
    }
}


}
