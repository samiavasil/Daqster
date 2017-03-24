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
#include<QSettings>

namespace Daqster {
// Constructors/Destructors
//  

PluginDescription::PluginDescription(const QString &               Location,
                                      const bool                   Enabled,
                                      const QString &              Name,
                                      const Daqster::PluginType_t  PluginType,
                                      const QString &              PluginTypeName,
                                      const QString &              Author,
                                      const QString &              Description,
                                      const QString &              DetailDescription,
                                      const QString &              License,
                                      const QString &              Version,
                                      const QIcon   &              Icon
                                    )
{
   m_Location          = Location;
   m_Enabled           = Enabled;
   m_Name              = Name;
   m_PluginType        = PluginType;
   m_Author            = Author;
   m_Description       = Description;
   m_DetailDescription = DetailDescription;
   m_License           = License;
   m_PluginTypeName    = PluginTypeName;
   m_Version           = Version;
   m_Icon              = Icon;
}

/**
* @brief Copy constructor
* @param b
*/
PluginDescription::PluginDescription(const PluginDescription& b)
{
    *this = b;
}

PluginDescription::~PluginDescription () {

}

bool  PluginDescription::IsEmpty()
{
    return ( *this == PluginDescription() );
}

/**
 * @brief PluginDescription::Compare - Return bitmask with difference betwen two PluginDescription objects
 * @param Object for compare
 * @return Bitmask with PlugDiff values ( see PlugDiff type)
 */
unsigned int  PluginDescription::Compare( const PluginDescription &b ) const{
    unsigned int diff = NOTHING_OPT;
    if( m_Location.compare(  b.m_Location ) ){
        diff |= LOCATION_OPT;
    }
    if( m_Enabled !=  b.m_Enabled ){
        diff |= ENABLE_OPT;
    }
    if( m_Name.compare(  b.m_Name ) ){
        diff |= NAME_OPT;
    }
    if(  m_PluginType != b.m_PluginType ){
        diff |= TYPE_OPT;
    }
    if( m_Author.compare(  b.m_Author ) ){
        diff |= AUTHOR_OPT;
    }
    if( m_Description.compare(  b.m_Description ) ){
        diff |= DESCRIPTION_OPT;
    }
    if( m_DetailDescription.compare(  b.m_DetailDescription ) ){
        diff |= DETAIL_DESCRIPTION_OPT;
    }
    if( m_License.compare(  b.m_License ) ){
        diff |= LICENSE_OPT;
    }
    if( m_PluginTypeName.compare(  b.m_PluginTypeName ) ){
        diff |= PLUG_TYPE_NAME_OPT;
    }
    if( m_Version.compare(  b.m_Version ) ){
        diff |= VERSION_OPT;
    }
    if( m_Icon.name().compare( b.m_Icon.name() ) ){
        diff |= ICON_OPT;
    }
    return diff;
}

/**
 * @brief Compare objects valid fields
 * @param Object for compare
 * @return true  - if object have the same valid valid fields. Doesn't check  invalid fields.
 *         false - otherwise
 */
bool  PluginDescription::CompareByValidFields( const PluginDescription &b ) const{
    bool ret = true;
    if(  !m_Location.isEmpty() && m_Location.compare(  b.m_Name ) ){
       ret = false;
    }else if( m_Enabled != b.m_Enabled ){
        ret = false;
    }else if( !m_Name.isEmpty() && m_Name.compare(  b.m_Name ) ){
        ret = false;
    } else if(  m_PluginType != UNDEFINED_TYPE && m_PluginType != b.m_PluginType ){
        ret = false;
    }else if( !m_Author.isEmpty() && m_Author.compare(  b.m_Author ) ){
        ret = false;
    }else if( !m_Description.isEmpty() && m_Description.compare(  b.m_Description ) ){
        ret = false;
    }else if( !m_DetailDescription.isEmpty() && m_DetailDescription.compare(  b.m_DetailDescription ) ){
        ret = false;
    }else if( !m_License.isEmpty() && m_License.compare(  b.m_License ) ){
        ret = false;
    }else if( !m_PluginTypeName.isEmpty() && m_PluginTypeName.compare(  b.m_PluginTypeName ) ){
        ret = false;
    }else if( !m_Version.isEmpty() && m_Version.compare(  b.m_Version ) ){
        ret = false;
    }else if( m_Icon.isNull() && m_Icon.name().compare( b.m_Icon.name() ) ){
        ret = false;
    }
    return ret;
}

/**
 * @brief Overloading Equal operator
 * @param PluginDescription object
 * @return PluginDescription
 */
PluginDescription & PluginDescription::operator=(const PluginDescription &b){
    m_Location          = b.m_Location         ;
    m_Enabled           = b.m_Enabled          ;
    m_Name              = b.m_Name             ;
    m_PluginType        = b.m_PluginType       ;
    m_Author            = b.m_Author           ;
    m_Description       = b.m_Description      ;
    m_DetailDescription = b.m_DetailDescription;
    m_License           = b.m_License          ;
    m_PluginTypeName    = b.m_PluginTypeName   ;
    m_Version           = b.m_Version          ;
    m_Icon              = b.m_Icon             ;
    return *this;
}

/**
 * @brief Overoading operator ==
 * @param PluginDescription object
 * @return true if objects are equal
 */
bool  PluginDescription::operator==(const PluginDescription &b){
    return(
                ( m_PluginType == b.m_PluginType )&&
                (!(
                     m_Name.compare(  b.m_Name )||
                     m_Location.compare(  b.m_Location )||
                     m_Author.compare(  b.m_Author )||
                     m_Version.compare(  b.m_Version )||
                     m_PluginTypeName.compare(  b.m_PluginTypeName )||
                     m_Description.compare(  b.m_Description )
                     ))
                );
}


/**
 * Return plugin author
 * @return const QString&
 */
const QString& PluginDescription::GetAuthor () const
{
    return m_Author;
}

/**
 * Get plugin description
 * @return const QString&
 */
const QString& PluginDescription::GetDescription () const
{
    return m_Description;
}

/**
 * Get plugin detail description.
 * @return const QString&
 */
const QString& PluginDescription::GetDetailDescription () const
{
    return m_DetailDescription;
}

/**
 * Return plugin embeded icon.
 * @return const QIcon&
 */
const QIcon& PluginDescription::GetIcon () const
{
    return m_Icon;
}

/**
 * Get plugin license
 * @return const QString&
 */
const QString& PluginDescription::GetLicense () const
{
    return m_License;
}

/**
 * Return plugin name
 * @return const QString&
 */
const QString& PluginDescription::GetName () const
{
    return m_Name;
}

/**
 * Return plugin basic type. If this isn't set to some type you can check typeName
 * string and try to detect type from name.
 * @return const Daqster::PluginType_t&
 */
const Daqster::PluginType_t& PluginDescription::GetType () const
{
    return m_PluginType;
}

/**
 * Get plugin type name
 * @return const QString&
 */

const QString& PluginDescription::GetTypeName ()const
{
    return m_PluginTypeName;
}


/**
 * Get plugin version
 * @return const QString&
 */
const QString& PluginDescription::GetVersion () const
{
    return m_Version;
}

/**
 * @brief Get plugin directory Location
 * @return
 */
const QString &PluginDescription::GetLocation() const
{
    return m_Location;
}

/**
 * @brief Return Plugin file hash
 * @return
 */
const QString& PluginDescription::GetHash() const
{
    return m_Hash;
}

/**
 * @brief Return is plugin enabled
 * @return true/false
 */
bool PluginDescription::IsEnabled() const
{
    return m_Enabled;
}

/**
 * @brief Set Author Name
 * @param Author
 */
void PluginDescription::SetAuthor(const QString &Author) {
    m_Author = Author;
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

/**
 * @brief Enable plugin
 * @param En - true/false
 */
void PluginDescription::Enable( bool En )
{
    m_Enabled = En;
}

/**
 * @brief Set plugin directory Location
 * @param Location
 */
void PluginDescription::SetLocation(const QString &Location)
{
    m_Location = Location;
}

/**
 * @brief Store Plugin Parammeters to Qsetting store
 * @param Store
 * @return
 */
bool PluginDescription::StorePluginParamsToPersistency( QSettings &Store )
{
    Store.beginGroup( m_Hash );
    Store.setValue("Author", m_Author );
    Store.setValue("Description", m_Description);
    Store.setValue("DetailDescription", m_DetailDescription);
    Store.setValue("License", m_License);
    Store.setValue("Location", m_Location);
    Store.setValue("Name", m_Name);
    Store.setValue("Type", m_PluginType);
    Store.setValue("TypeName", m_PluginTypeName);
    Store.setValue("Version", m_Version);
    Store.setValue("Enabled", m_Enabled);
    Store.endGroup();
    return true;
}

/**
 * @brief Set File Hash. Used by plugin manager.
 * @return
 */
void PluginDescription::SetHash(const QString &Hash)
{
    m_Hash = Hash;
}


}
