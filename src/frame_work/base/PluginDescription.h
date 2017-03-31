/************************************************************************
                        Daqster/PluginDescription.h.h - Copyright 
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

Initial version of this file was created on 16.03.2017 at 12:33:53
**************************************************************************/
#ifndef PLUGINDESCRIPTION_H
#define PLUGINDESCRIPTION_H

#include "base/global.h"
#include <QString>
#include <QIcon>

#define PLUGIN_AUTHOR_PROPERTY        "Author"
#define PLUGIN_DESCRIPTION_PROPERTY   "Description"
#define PLUGIN_DETAIL_DESCRIPTION     "DetailDescription"
#define PLUGIN_ICON                   "Icon"
#define PLUGIN_LICENSE                "License"
#define PLUGIN_LOCATION               "Location"
#define PLUGIN_NAME                   "Name"
#define PLUGIN_TYPE                   "Type"
#define PLUGIN_TYPE_NAME              "TypeName"
#define PLUGIN_VERSION                "Version"
#define PLUGIN_HASH                   "Hash"
#define PLUGIN_HELTHY_STATE           "HealthyState"

class QSettings;

namespace Daqster {

class PrivateDescription;


/**
  * class PluginDescription
  * This class is a container with all plugin description - bear all information
  * abouth plugin. PluginManager can create new plugin by description provided by
  * this class.
  */

class FRAME_WORKSHARED_EXPORT PluginDescription
{
    friend class QPluginObjectsInterface;
public:
    typedef enum{
        SOME_TYPE           = 0x1,
        DETECT_BY_TYPE_NAME = 0x80000000,
        UNDEFINED_TYPE      = 0xffffffff
    } PluginType_t;

    typedef enum{
        FOUNDED ,  /*Founded in plugin search procedure                */
        IF_LOADED, /*Interface plugin object (object factory) successfully loaded */
        OBJECT_CREATION, /*Trying to create Plugin object*/
        HEALTHY ,  /*Founded, loaded and one or more plugins objects are successfully created */
        ILL ,      /*Founded but exception occured when tryed to load  */
        UNDEFINED  /*Not defined state                                 */
    } PluginHealtyState_t;

    typedef enum{
      NOTHING_OPT              = 0,
      LOCATION_OPT             = 1 << 1,
      ENABLE_OPT               = 1 << 2,
      NAME_OPT                 = 1 << 3,
      TYPE_OPT                 = 1 << 4,
      PLUG_TYPE_NAME_OPT       = 1 << 5,
      AUTHOR_OPT               = 1 << 6,
      DESCRIPTION_OPT          = 1 << 7,
      DETAIL_DESCRIPTION_OPT   = 1 << 8,
      LICENSE_OPT              = 1 << 9,
      VERSION_OPT              = 1 << 10,
      ICON_OPT                 = 1 << 11,

    }PlugDiff;
  // Constructors/Destructors
  /**
   * Empty Constructor
   */
  explicit PluginDescription ();

   /**
   * @brief Copy constructor
   * @param b
   */
   PluginDescription(const PluginDescription &b);
  /**
   * Empty Destructor
   */
  virtual ~PluginDescription ();

  void SetProperty(const char *name, const QVariant &value);

  QVariant GetProperty(const char *name) const;

  QList<QByteArray> GetPropertiesNames() const;
   /**
    * @brief IsEmpty
    * @return
    */
   bool IsEmpty() const;

   /**
   * @brief PluginDescription::Compare - Return bitmask with difference betwen two PluginDescription objects
   * @param Object for compare
   * @return Return 0 if object Pairs are the same
   *                > 0 if this have some equal properties as b
   *                < 0 if this haven't some equal properties as b
   */
   int Compare(const PluginDescription &b) const;

   /**
    * @brief Compare objects valid fields
    * @param Object for compare
    * @return true  - if object have the same valid valid fields. Doesn't check  invalid fields.
    *         false - otherwise
    */
   bool CompareByValidFields(const PluginDescription &b) const;

   /**
    * @brief Overoading operator ==
    * @param PluginDescription object
    * @return true if objects are equal
    */
   bool operator ==(const PluginDescription &b);

   /**
    * @brief Overloading Equal operator
    * @param PluginDescription object
    * @return PluginDescription
    */
   PluginDescription &operator =(const PluginDescription &b);

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
   * @brief Store Plugin Parammeters to Qsetting store
   * @param Store
   * @return
   */
  bool StorePluginParamsToPersistency( QSettings &Store );

  /**
   * @brief Get Plugin Parammeters from Qsetting store
   * @param Store
   * @return
   */
  bool GetPluginParamsFromPersistency(QSettings &Store);


protected:
    void CopyDinamycProperties(const PluginDescription &b);
protected:
  // Is plugin enabled for usage
  bool m_Enabled;
  PrivateDescription *m_PrivateDescription;
};
} // end of package namespace

#endif // PLUGINDESCRIPTION_H
