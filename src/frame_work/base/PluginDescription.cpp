/************************************************************************
                        Daqster/PluginDescription.cpp.cpp - Copyright 
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

#include "PluginDescription.h"

namespace Daqster {
// Constructors/Destructors
//  

PluginDescription::PluginDescription () {

}

PluginDescription::~PluginDescription () {

}

/**
 * Return plugin author
 * @return const QString&
 */
const QString& PluginDescription::GetAuthor ()
{
    return m_Author;
}

void PluginDescription::SetAuthor(const QString &Author){
    m_Author = Author;
}

/**
 * Get plugin description
 * @return const QString&
 */
const QString& PluginDescription::GetDescription ()
{
    return m_Description;
}

/**
 * Get plugin detail description.
 * @return const QString&
 */
const QString& PluginDescription::GetDetailDescription ()
{
    return m_DetailDescription;
}

/**
 * Return plugin embeded icon.
 * @return const QIcon&
 */
const QIcon& PluginDescription::GetIcon ()
{
    return m_Icon;
}

/**
 * Get plugin license
 * @return const QString&
 */
const QString& PluginDescription::GetLicense ()
{
    return m_License;
}

/**
 * Return plugin name
 * @return const QString&
 */
const QString& PluginDescription::GetName ()
{
    return m_Name;
}

/**
 * Return plugin basic type. If this isn't set to some type you can check typeName
 * string and try to detect type from name.
 * @return const Daqster::PluginType_t&
 */
const Daqster::PluginType_t& PluginDescription::GetType ()
{
    return m_PluginType;
}

/**
 * Get plugin type name
 * @return const QString&
 */

const QString& PluginDescription::GetTypeName ()
{
    return m_PluginTypeName;
}


/**
 * Get plugin version
 * @return const QString&
 */
const QString& PluginDescription::GetVersion ()
{
    return m_Version;
}

/**
 * @brief PluginDescription::SetDescription
 * @param Description
 */
void PluginDescription::SetDescription(const QString &Description)
{
    m_Description = Description;
}

/**
 * @brief PluginDescription::SetDetailDescription
 * @param DetailDescription
 */
void PluginDescription::SetDetailDescription(const QString &DetailDescription)
{
    m_DetailDescription = DetailDescription;
}

/**
 * @brief PluginDescription::SetIcon
 * @param Icon
 */
void PluginDescription::SetIcon(const QIcon &Icon)
{
    m_Icon = Icon;
}

/**
 * @brief PluginDescription::SetLicense
 * @param License
 */
void PluginDescription::SetLicense(const QString &License)
{
    m_License = License;
}

/**
 * @brief PluginDescription::SetName
 * @param Name
 */
void PluginDescription::SetName(const QString &Name)
{
    m_Name = Name;
}

/**
 * @brief PluginDescription::SetPluginType
 * @param PluginType
 */
void PluginDescription::SetPluginType(const Daqster::PluginType_t &PluginType)
{
    m_PluginType = PluginType;
}

/**
 * @brief PluginDescription::SetPluginTypeName
 * @param PluginTypeName
 */
void PluginDescription::SetPluginTypeName(const QString &PluginTypeName)
{
    m_PluginTypeName = PluginTypeName;
}

/**
 * @brief PluginDescription::SetVersion
 * @param Version
 */
void PluginDescription::SetVersion(const QString &Version)
{
    m_Version = Version;
}


}
