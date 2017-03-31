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
#include "base/debug.h"
#include<QObject>
#include<QSettings>

namespace Daqster {

/*This is internal private class to store Plugin description*/
class PrivateDescription : public QObject{
    public:
    explicit PrivateDescription( QObject* Parent = NULL ):QObject(Parent){

    }
    virtual ~PrivateDescription(){

    }
};


// Constructors/Destructors
//  

PluginDescription::PluginDescription()
{
   m_PrivateDescription = new PrivateDescription();
}

/**
* @brief Copy constructor
* @param b
*/
PluginDescription::PluginDescription(const PluginDescription& b)
{
    m_PrivateDescription = new PrivateDescription();
    m_Enabled = b.m_Enabled;
    CopyDinamycProperties( b );
}

PluginDescription::~PluginDescription () {
    if( NULL != m_PrivateDescription ){
        m_PrivateDescription->deleteLater();
    }
}

void PluginDescription::SetProperty( const char *name, const QVariant &value )
{
    m_PrivateDescription->setProperty( name, value );
}

QVariant  PluginDescription::GetProperty( const char *name ) const
{
    return m_PrivateDescription->property( name );
}

QList<QByteArray>  PluginDescription::GetPropertiesNames( ) const
{
    return  m_PrivateDescription->dynamicPropertyNames();
}

bool  PluginDescription::IsEmpty() const
{
    return ( 0 == m_PrivateDescription->dynamicPropertyNames().count() );
}

/**
* @brief PluginDescription::Compare - Return bitmask with difference betwen two PluginDescription objects
* @param Object for compare
* @return Return 0 if object Pairs are the same
*                > 0 if this have some equal properties as b
*                < 0 if this haven't some equal properties as b
*/
int  PluginDescription::Compare( const PluginDescription &b ) const{
    /* TODO: TBD
     * unsigned int diff = NOTHING_OPT;
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
    }*/
    return 0;
}

/**
 * @brief Compare objects valid fields
 * @param Object for compare
 * @return true  - if object have the same valid valid fields. Doesn't check  invalid fields.
 *         false - otherwise
 */
bool  PluginDescription::CompareByValidFields( const PluginDescription &b ) const{
    bool ret = true;
     // TODO: TBD
 #if 0
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
#endif
    return ret;
}

void PluginDescription::CopyDinamycProperties( const PluginDescription &b ){
    QList<QByteArray> names = m_PrivateDescription->dynamicPropertyNames();
    /*Delete old properies and copy new ones*/
    QVariant Invalid;
    foreach( QByteArray name, names ){
        m_PrivateDescription->setProperty( name,Invalid );
    }

    names = b.GetPropertiesNames();
    foreach( QByteArray name, names ){
        if( m_PrivateDescription->setProperty( name, b.GetProperty(name) ) ){
            DEBUG << "Strange - set of this dynamic property should return false here. Chek it - maybe it is defined with Q_PROPERTY  macro";
        }
    }
}

/**
 * @brief Overloading Equal operator
 * @param PluginDescription object
 * @return PluginDescription
 */
PluginDescription & PluginDescription::operator=(const PluginDescription &b){
    CopyDinamycProperties( b );
    /*Copy static properties*/
    m_Enabled = b.m_Enabled;
    return *this;
}

/**
 * @brief Overoading operator ==
 * @param PluginDescription object
 * @return true if objects are equal
 */
bool  PluginDescription::operator==(const PluginDescription &b){
    bool ret = false;
    if( m_Enabled == b.m_Enabled ){
        QList<QByteArray> ThisNames = this->m_PrivateDescription->dynamicPropertyNames();
        QList<QByteArray> BNames    = b.m_PrivateDescription->dynamicPropertyNames();
        if( ThisNames.count() == BNames.count() ){
            ret = true;
            foreach( QByteArray Name, ThisNames ) {
                if( this->GetProperty(Name) !=  b.GetProperty(Name) ){
                    ret = false;
                    break;
                }
            }
        }
    }
   return ret;
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
 * @brief Store Plugin Parammeters to Qsetting store
 * @param Store
 * @return
 */
bool PluginDescription::StorePluginParamsToPersistency( QSettings &Store )
{
    QList<QByteArray> names = m_PrivateDescription->dynamicPropertyNames();
    foreach( QByteArray name, names ){
        Store.setValue( name, m_PrivateDescription->property(name) );
    }
    return true;
}

/**
 * @brief Get Plugin Parammeters from Qsetting store
 * @param Store
 * @return
 */
bool PluginDescription::GetPluginParamsFromPersistency( QSettings &Store )
{
    bool ret = false;
    QList<QByteArray> names = m_PrivateDescription->dynamicPropertyNames();
    /*Delete old properies and copy new ones*/
    QVariant Invalid();
    foreach( QByteArray name, names ){
        m_PrivateDescription->setProperty( name,Invalid );
    }

    QStringList list = Store.childKeys();
    foreach( QString name, list ){
        if( m_PrivateDescription->setProperty( name.toUtf8().data(), Store.value(name, "" ) ) ){
            DEBUG << "Strange - set of this dynamic property should return false here. Chek it - maybe it is defined with Q_PROPERTY  macro";
        }
    }
    if( 1 )/*TODO: TBD Check for somehting*/
    {
        ret = true;
    }

    return ret;
}

}
