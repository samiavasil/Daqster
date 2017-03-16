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
    DETECT_BY_TYPE_NAME = 0x80000000
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

  // Constructors/Destructors
  /**
   * Empty Constructor
   */
  PluginDescription ();

  /**
   * Empty Destructor
   */
  virtual ~PluginDescription ();

  /**
   * @brief Return plugin author
   * @return const QString&
   */
  const QString& GetAuthor ();

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

protected:
  //properties
  // Plugin Author
  QString m_Author;
  // Plugin Description
  QString m_Description;
  // Plugin detailed description
  // 
  QString m_DetailDescription;
  // Plugin Embeded Icon
  QIcon m_Icon;
  // Plugin License
  QString m_License;
  // Plugin name
  QString m_Name;
  // Plugin type
  Daqster::PluginType_t m_PluginType;
  // Plugin Type Name
  QString m_PluginTypeName;
  // Plugin Version
  QString m_Version;
};
} // end of package namespace

#endif // PLUGINDESCRIPTION_H
