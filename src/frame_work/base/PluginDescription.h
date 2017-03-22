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

namespace Daqster {

typedef enum{
    SOME_TYPE           = 0x1,
    DETECT_BY_TYPE_NAME = 0x80000000,
    UNDEFINED_TYPE      = 0xffffffff
} PluginType_t;

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
  PluginDescription (const QString &              Location = QString(),
                      const bool                   Enabled  = true,
                      const QString &              Name = QString(),
                      const Daqster::PluginType_t  PluginType = Daqster::DETECT_BY_TYPE_NAME,
                      const QString &              PluginTypeName= QString(),
                      const QString &              Author = QString(),
                      const QString &              Description = QString(),
                      const QString &              DetailDescription = QString(),
                      const QString &              License = QString(),
                      const QString &              Version = QString(),
                      const QIcon   &              Icon = QIcon()
                     );

   /**
   * @brief Copy constructor
   * @param b
   */
   PluginDescription(const PluginDescription &b);
  /**
   * Empty Destructor
   */
  virtual ~PluginDescription ();

   /**
    * @brief IsEmpty
    * @return
    */
   bool IsEmpty();

   /**
   * @brief PluginDescription::Compare - Return bitmask with difference betwen two PluginDescription objects
   * @param Object for compare
   * @return Bitmask with PlugDiff values ( see PlugDiff type)
   */
   unsigned int Compare(const PluginDescription &b) const;

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
   bool operator ==(const PluginDescription &b) const;

   /**
    * @brief Overloading Equal operator
    * @param PluginDescription object
    * @return PluginDescription
    */
   PluginDescription &operator =(const PluginDescription &b);

  /**
   * @brief Return plugin author
   * @return const QString&
   */
  const QString& GetAuthor ();

  /**
   * @brief Set Author Name
   * @param Author
   */
  void SetAuthor(const QString &Author);

  /**
   * @brief Get plugin description
   * @return const QString&
   */
  const QString& GetDescription ();

  /**
   * @brief Get plugin detail description.
   * @return const QString&
   */
  const QString& GetDetailDescription ();

  /**
   * @brief Return plugin embeded icon.
   * @return const QIcon&
   */
  const QIcon& GetIcon ();

  /**
   * @brief Get plugin license
   * @return const QString&
   */
  const QString& GetLicense ();

  /**
   * @brief Return plugin name
   * @return const QString&
   */
  const QString& GetName ();

  /**
   * @brief Return plugin basic type. If this isn't set to some type you can check typeName
   * string and try to detect type from name.
   * @return const Daqster::PluginType_t&
   */
  const Daqster::PluginType_t& GetType ();

  /**
   * @brief Get plugin type name
   * @return const QString&
   */
  const QString& GetTypeName ();

  /**
   * @brief Get plugin version
   * @return const QString&
   */
  const QString& GetVersion ();

  /**
   * @brief Get plugin directory Location
   * @return
   */
  const QString& GetLocation( );

  /**
   * @brief Return is plugin enabled
   * @return true/false
   */
  bool  IsEnabled();

  /**
   * @brief SetDescription
   * @param Description
   */
  void SetDescription(const QString &Description);

  /**
   * @brief SetDetailDescription
   * @param DetailDescription
   */
  void SetDetailDescription(const QString &DetailDescription);

  /**
   * @brief SetIcon
   * @param Icon
   */
  void SetIcon(const QIcon &Icon);

  /**
   * @brief SetLicense
   * @param License
   */
  void SetLicense(const QString &License);

  /**
   * @brief SetName
   * @param Name
   */
  void SetName(const QString &Name);

  /**
   * @brief SetPluginType
   * @param PluginType
   */
  void SetPluginType(const Daqster::PluginType_t &PluginType);

  /**
   * @brief SetPluginTypeName
   * @param PluginTypeName
   */
  void SetPluginTypeName(const QString &PluginTypeName);

  /**
   * @brief SetVersion
   * @param Version
   */
  void SetVersion(const QString &Version);

  /**
   * @brief Enable plugin
   * @param En - true/false
   */
  void Enable( bool En );

  /**
   * @brief Set plugin directory Location
   * @param Location
   */
  void SetLocation( const QString& Location );


protected:
  //properties
  // Plugin Author
  QString m_Author;
  // Plugin Description
  QString m_Description;
  // Plugin detailed description
  QString m_DetailDescription;
  // Plugin Embeded Icon
  QIcon m_Icon;
  // Plugin License
  QString m_License;
  // Plugin Location
  QString m_Location;
  // Plugin name
  QString m_Name;
  // Plugin type
  Daqster::PluginType_t m_PluginType;
  // Plugin Type Name
  QString m_PluginTypeName;
  // Plugin Version
  QString m_Version;
  // Is plugin enabled for usage
  bool m_Enabled;
  /*TODO: TBD on the feature.
   * Cryptographic Hash of plugin file,
   * wich can be used to verify where the file isn't
   * changed on the fly.
   * QString m_Hash; - Returned from QCryptographicHash
   */

};
} // end of package namespace

#endif // PLUGINDESCRIPTION_H
