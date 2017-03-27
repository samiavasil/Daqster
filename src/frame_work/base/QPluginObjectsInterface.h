/************************************************************************
                        Daqster/QPluginObjectsInterface.h - Copyright vvasilev
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


#ifndef QPLUGINOBJECTSINTERFACE_H
#define QPLUGINOBJECTSINTERFACE_H
#include "global.h"
#include <QObject>
#include <QIcon>
#include <QString>
#include<QSharedPointer>
#include "PluginDescription.h"
class QPluginLoader;
class QSettings;
namespace Daqster {

class QBasePluginObject;

/**
  * class QPluginObjectsInterface
  * @brief This is a base plugin interface class.
  * All plugins should inherite this plugin interface in oreder to create plugin  objects.
  * QPluginLoader class instantiate objects from this class. Object from this class contains
  * pointer associasion to coresponding QPluginLoader object wich load it.
  * QPluginObjectsInterface objects can save it member parametters in persistent QSettings store.
  */

class FRAME_WORKSHARED_EXPORT QPluginObjectsInterface : public QObject
{
    Q_OBJECT
public:

  /**
   * Empty Constructor
   * @param  parent
   */
   QPluginObjectsInterface (QObject* Parent = NULL );

  /**
   * Empty Destructor
   */
  virtual ~QPluginObjectsInterface ();

   /**
    * @brief Get plugin Location
    * @return plugin location
    */
   const QString&  GetLocation();

  /**
   * Return plugin basic type. If this isn't set to some type you can check typeName
   * string and try to detect type from name.
   * @return Daqster::PluginType_t
   */
  Daqster::PluginType_t GetType ();


  /**
   * Return plugin embeded icon.
   * @return const &QIcon
   */
  const QIcon& GetIcon ();


  /**
   * Return plugin name
   * @return const QString&
   */
  const QString& GetName ();


  /**
   * Get plugin type name
   * @return const QString&
   */
  const QString& GetTypeName ();


  /**
   * Get plugin version
   * @return const QString&
   */
  const QString& GetVersion ();


  /**
   * Get plugin description
   * @return const QString&
   */
  const QString& GetDescription ();


  /**
   * Get plugin detail description.
   * @return const QString&
   */
  const QString& GetDetailDescription ();


  /**
   * Get plugin license
   * @return const QString&
   */
  const QString& GetLicense ();


  /**
   * Return plugin author
   * @return const QString&
   */
  const QString& GetAuthor ();

  /**
   * @brief Return Plugin file hash
   * @return
   */
  const QString& GetHash() const;

  /**
   * @brief Return Plugin Descriptor
   * @return
   */
  const Daqster::PluginDescription& GetPluginDescriptor() const;
  /**
   * Set new plugin loader.
   * When the plugin is loaded on first time we create QPluginLoader and its method
   * instance() returns QPluginObjectInterface*  plugInterface. On this point 
   * plugInterface->setPluginLoader() function is called to set pointer to
   * QPluginLoader.
   * @param  Loader New plugin loader
   */
  void SetPluginLoader (QSharedPointer<QPluginLoader> & Loader);


  /**
   * Create  new plugin object.
   * @return Daqster::QBasePluginObject*
   * @param  Parrent Pointer to parent QObject
   */
  Daqster::QBasePluginObject* CreatePlugin (QObject* Parrent = NULL);

  /**
   * @brief Set plugin location. This function should be called just from PluginManager
   * when succesfully load pugin from some configured directory.
   * @param Plugin dirctory Location
   */
  void SetLocation(const QString &Location);

  /**
   * @brief Store Plugin Parameters to persistent settings store.
   * The main idea is when some plugin is loaded one time information for plugin is saved
   * on store and in feature this plugin information is used without loading of plugin.
   * Plugin will be loaded just if it is explicitly used esle just the persistent information is used.
   * @param Store
   * @return
   */
  bool StorePluginParamsToPersistency( QSettings& Store );

  /**
   * @brief Set File Hash. Used by plugin manager.
   * @return
   */
  void SetHash(const QString &Hash);

protected:
  /**
   * Create  new plugin object. Abstract function should be impleented on inherited
   * calsses
   * @return QBasePluginObject *
   * @param  Parrent Parent object
   */
  virtual Daqster::QBasePluginObject* CreatePluginInternal(QObject* Parrent = NULL) = 0;

protected slots:
  void pluginInstanceDestroyed( QObject* obj );

protected:
  Daqster::PluginDescription m_PluginDescryptor;
  // Plugin loader
  QSharedPointer<QPluginLoader> m_PluginLoader;
  // List  with currently instantiated plugins
  QList<Daqster::QBasePluginObject *> m_PluginInstList;
};
} // end of package namespace



/*Next declarations insired fom itom project :)*/
#define CREATE_PLUGIN_INTERFACE_VERSION_STR(major,minor,patch) "Daqster.PlugIn.BaseInterface/"#major"."#minor"."#patch
#define CREATE_PLUGIN_INTERFACE_VERSION(major,minor,patch) ((major<<16)|(minor<<8)|(patch))

#define DAQSTER_PLUGIN_INTERFACE_MAJOR 0
#define DAQSTER_PLUGIN_INTERFACE_MINOR 0
#define DAQSTER_PLUGIN_INTERFACE_PATCH 0
#define DAQSTER_PLUGIN_INTERFACE_VERSION CREATE_PLUGIN_INTERFACE_VERSION(DAQSTER_PLUGIN_INTERFACE_MAJOR,DAQSTER_PLUGIN_INTERFACE_MINOR,DAQSTER_PLUGIN_INTERFACE_PATCH)
//###########################################################################################################
//   Interface version:
//###########################################################################################################
//
//
// Please change the interface version, if you made any changes to this interface, files located in the common folder or to the dataObject.
//
// To add a new version, do the following steps
//
// 1. append the string behind the variable daqster_PluginInterface_CurrentVersion (e.g. CREATE_PLUGIN_INTERFACE_VERSION_STR(0,0,0)) to the array daqster_PluginInterface_OldVersions
// 2. change the version number in the string daqster_PluginInterface_CurrentVersion (e.g. CREATE_PLUGIN_INTERFACE_VERSION_STR(0,0,1))
// TODO: DELL ME 3. if the QPluginObjectsInterface version number is incremented, the ito.AbstractItomDesignerPlugin number in AbstractItomDesignerPlugin.h must be incremented as well.
//
//
// This helps, that deprecated or "future" plugins, which fit not to the current implementation of the interface will not be loaded
// but a sophisticated error message is shown.

static const char* daqster_PluginObjectInterface_OldVersions[] = {
     CREATE_PLUGIN_INTERFACE_VERSION_STR(-1,-1,-1),//version TODO: DELL ME not real version in moment
     NULL
};
static const char* daqster_PluginInterface_CurrentVersion = DAQSTER_PLUGIN_INTERFACE_VERSION; //results in "Daqster.PlugIn.BaseInterface/x.x.x";
// must be out of namespace
Q_DECLARE_INTERFACE(Daqster::QPluginObjectsInterface , daqster_PluginInterface_CurrentVersion )
#endif // QPLUGINBASESINTERFACE_H
