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

Initial version of this file was created on 12.03.2017 at 20:54:50
**************************************************************************/

#include "QPluginObjectsInterface.h"
#include "QBasePluginObject.h"
#include "debug.h"
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
 * @brief Set plugin location. This function should be called just from PluginManager
 * when succesfully load pugin from some configured directory.
 * @param Plugin dirctory Location
 */
void QPluginObjectsInterface::SetLocation( const QString & Location )
{
    m_PluginDescryptor.SetProperty( PLUGIN_LOCATION, Location );
}

/**
 * @brief Set File Hash. Used by plugin manager.
 * @return
 */
void QPluginObjectsInterface::SetHash(const QString &Hash)
{
    m_PluginDescryptor.SetProperty( PLUGIN_HASH, Hash );
}

/**
 * @brief Set plugin Healthy State. Defined states are:
 * TODO:Not readdy at all TBD
 *           FOUNDED   -  Founded in plugin search procedure
 *           IF_LOADED -  Interface plugin object (object factory) successfully loaded
 *           HEALTHY   -  Founded, loaded and one or more plugins objects are successfully created
 *           ILL       -  Founded but exception occured when tryed to load
 *           UNDEFINED -  Not defined state
 * @param State
 */
void QPluginObjectsInterface::SetHealthyState( const PluginDescription::PluginHealtyState_t& State)
{
    m_PluginDescryptor.SetProperty( PLUGIN_HELTHY_STATE, State );
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

PluginDescription::PluginHealtyState_t QPluginObjectsInterface::GetHealthyState( )
{
    return (PluginDescription::PluginHealtyState_t)m_PluginDescryptor.GetProperty( PLUGIN_HELTHY_STATE ).toUInt();
}


/**
 * @brief Return is plugin enabled
 * @return true/false
 */
bool QPluginObjectsInterface::IsEnabled() const
{
   m_PluginDescryptor.IsEnabled();
}

/**
 * @brief Enable plugin
 * @param En - true/false
 */
void QPluginObjectsInterface::Enable(bool En)
{
    m_PluginDescryptor.Enable( En );
}

/**
 * @brief Store Plugin Parameters to persistent settings store.
 * The main idea is when some plugin is loaded one time information for plugin is saved
 * on store and in feature this plugin information is used without loading of plugin.
 * Plugin will be loaded just if it is explicitly used esle just the persistent information is used.
 * @param Store
 * @return
 */
bool QPluginObjectsInterface::StorePluginParamsToPersistency( QSettings &Store )
{
    return m_PluginDescryptor.StorePluginParamsToPersistency( Store );
}

/**
 * @brief Get plugin Location
 * @return plugin location
 */
QString QPluginObjectsInterface::GetLocation() const
{
    return  m_PluginDescryptor.GetProperty( PLUGIN_LOCATION ).toString();
}

/**
 * @brief Return Plugin file hash
 * @return
 */
QString QPluginObjectsInterface::GetHash() const
{
    return m_PluginDescryptor.GetProperty( PLUGIN_HASH ).toString();
}

/**
 * Return plugin basic type. If this isn't set to some type you can check typeName
 * string and try to detect type from name.
 * @return Daqster::PluginType_t
 */
PluginDescription::PluginType_t QPluginObjectsInterface::GetType () const
{
    return (PluginDescription::PluginType_t)m_PluginDescryptor.GetProperty( PLUGIN_TYPE ).toUInt();
}

/**
 * Return plugin embeded icon.
 * @return QIcon
 */
QIcon QPluginObjectsInterface::GetIcon () const
{
    return m_PluginDescryptor.GetIcon();//m_PluginDescryptor.GetProperty( PLUGIN_ICON ).value<QIcon>();
}


/**
 * Return plugin name
 * @return Plugin name
 */
QString QPluginObjectsInterface::GetName () const
{
    return m_PluginDescryptor.GetProperty( PLUGIN_NAME ).toString();
}

/**
 * Get plugin type name
 * @return Plugin type name
 */
QString QPluginObjectsInterface::GetTypeName () const
{
    return m_PluginDescryptor.GetProperty( PLUGIN_TYPE_NAME ).toString();
}

/**
 * Get plugin version
 * @return Plugin Version
 */
QString QPluginObjectsInterface::GetVersion () const
{
    return m_PluginDescryptor.GetProperty( PLUGIN_VERSION ).toString();
}


/**
 * Get plugin description
 * @return Plugin Description
 */
QString QPluginObjectsInterface::GetDescription () const
{
    return m_PluginDescryptor.GetProperty( PLUGIN_DESCRIPTION ).toString();
}


/**
 * Get plugin detail description.
 * @return Plugin Detail Description
 */
QString QPluginObjectsInterface::GetDetailDescription () const
{
    return m_PluginDescryptor.GetProperty( PLUGIN_DETAIL_DESCRIPTION ).toString();
}


/**
 * Get plugin license
 * @return Plugin License
 */
QString QPluginObjectsInterface::GetLicense () const
{
    return m_PluginDescryptor.GetProperty( PLUGIN_LICENSE ).toString();
}

/**
 * Return plugin author
 * @return Plugin Author
 */
QString QPluginObjectsInterface::GetAuthor () const
{
    return m_PluginDescryptor.GetProperty( PLUGIN_AUTHOR ).toString();
}

const PluginDescription &QPluginObjectsInterface::GetPluginDescriptor() const
{
    return m_PluginDescryptor;
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
