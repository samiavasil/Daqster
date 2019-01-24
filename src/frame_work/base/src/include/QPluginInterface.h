/************************************************************************
                        Daqster/QPluginInterface.h - Copyright vvasilev
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

#ifndef QPLUGININTERFACE_H
#define QPLUGININTERFACE_H
#include "build_cfg.h"
#include <QObject>
#include <QIcon>
#include <QString>
#include<QSharedPointer>
#include "PluginDescription.h"

class QPluginLoaderExt;
class QSettings;

/*
VVV On the future plugin descriptions, types and some adtional information can be provided by plugin Json files,
for now it are a properties on plugin interface.
*/
namespace Daqster {

class QBasePluginObject;

/**
  * class QPluginInterface
  * @brief This is a base plugin interface class.
  * All plugins should inherite this plugin interface in oreder to create plugin  objects.
  * QPluginLoaderExt class instantiate objects from this class. Object from this class contains
  * pointer associasion to coresponding QPluginLoaderExt object wich load it.
  * QPluginInterface objects can save it member parametters in persistent QSettings store.
  */

class FRAME_WORKSHARED_EXPORT QPluginInterface : public QObject
{
    Q_OBJECT
public:

  /**
   * Empty Constructor
   * @param  parent
   */
   QPluginInterface (QObject* Parent = NULL );

  /**
   * Empty Destructor
   */
  virtual ~QPluginInterface ();

   /**
    * @brief Get plugin Location
    * @return plugin location
    */
   QString GetLocation() const;

   /**
    * Set new plugin loader.
    * When the plugin is loaded on first time we create QPluginLoaderExt and its method
    * instance() returns QPluginObjectInterface*  plugInterface. On this point
    * plugInterface->setPluginLoader() function is called to set pointer to
    * QPluginLoaderExt.
    * @param  Loader New plugin loader
    */
   void SetPluginLoader (QSharedPointer<QPluginLoaderExt> & Loader);

   /**
    * @brief Return Plugin Loader object
    * @return
    */
   QSharedPointer<QPluginLoaderExt> & GetPluginLoader ();

   /**
    * @brief Set plugin location. This function should be called just from PluginManager
    * when succesfully load pugin from some configured directory.
    * @param Plugin dirctory Location
    */
   void SetLocation(const QString &Location);

   /**
    * @brief Set File Hash. Used by plugin manager.
    * @return
    */
   void SetHash(const QString &Hash);

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
   void SetHealthyState(const PluginDescription::PluginHealtyState_t& State );

   PluginDescription::PluginHealtyState_t GetHealthyState();

   /**
    * @brief Return is plugin enabled
    * @return true/false
    */
   bool  IsEnabled() const;

   /**
    * @brief Enable plugin
    * @param En - true/false
    */
   void Enable( bool En );

   /**
    * @brief Return Plugin file hash
    * @return Hash
    */
   QString GetHash() const;

  /**
   * Return plugin basic type. If this isn't set to some type you can check typeName
   * string and try to detect type from name.
   * @return Daqster::PluginType_t
   */
  PluginDescription::PluginType_t GetType () const;

  /**
   * Return plugin embeded icon.
   * @return QIcon
   */
  QIcon GetIcon() const;

  /**
   * Return plugin name
   * @return Plugin Name
   */
  QString GetName() const;

  /**
   * Get plugin type name
   * @return Plugin type name
   */
  QString GetTypeName() const;

  /**
   * Get plugin version
   * @return Plugin Version
   */
  QString GetVersion() const;

  /**
   * Get plugin description
   * @return Plugin Description
   */
  QString GetDescription() const;

  /**
   * Get plugin detail description.
   * @return Plugin Detail Description
   */
  QString GetDetailDescription() const;

  /**
   * Get plugin license
   * @return Plugin License
   */
  QString GetLicense() const;

  /**
   * Return plugin author
   * @return Plugin Author
   */
  QString GetAuthor() const;

  /**
   * @brief Return Plugin Descriptor
   * @return
   */
  const Daqster::PluginDescription& GetPluginDescriptor() const;

  /**
   * Create  new plugin object.
   * @return Daqster::QBasePluginObject*
   * @param  Parrent Pointer to parent QObject
   */
  Daqster::QBasePluginObject* CreatePlugin (QObject* Parrent = NULL);

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
   * @brief Destroy all Objects included in Plugin Object Pool
   * @return true on success
   *         false otherwise
   */
  virtual bool ShutdownAllPluginObjects();

signals:
  void AllPluginObjectsDestroyed(const QString &Hash);

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
  QSharedPointer<QPluginLoaderExt> m_PluginLoader;
  // Plugin Object Pool - List  with currently instantiated plugins
  QList<Daqster::QBasePluginObject *> m_PluginInstList;
};
} // end of package namespace



/*Next declarations insired fom itom project :)*/
#define IID_DAQSTER_PLUGIN_INTERFACE   "Daqster.PlugIn.QPluginInterface"
#define CREATE_PLUGIN_INTERFACE_VERSION_STR(major,minor,patch) IID_DAQSTER_PLUGIN_INTERFACE"/"#major"."#minor"."#patch
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
// TODO: DELL ME 3. if the QPluginInterface version number is incremented, the ito.AbstractItomDesignerPlugin number in AbstractItomDesignerPlugin.h must be incremented as well.
//
//
// This helps, that deprecated or "future" plugins, which fit not to the current implementation of the interface will not be loaded
// but a sophisticated error message is shown.
/*TODO: Plugin Version control should be implemented*/
static const char* daqster_PluginObjectInterface_OldVersions[] = {
     CREATE_PLUGIN_INTERFACE_VERSION_STR(-1,-1,-1),//version TODO: DELL ME not real version in moment
     NULL
};
static const char* daqster_PluginInterface_CurrentVersion = DAQSTER_PLUGIN_INTERFACE_VERSION; //results in "Daqster.PlugIn.BaseInterface/x.x.x";
// must be out of namespace
Q_DECLARE_INTERFACE(Daqster::QPluginInterface , daqster_PluginInterface_CurrentVersion )
#endif // QPLUGINBASESINTERFACE_H
