/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VMGlobalSettingsData, VMGlobalSettings class implementation
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#include <qglobal.h>
#include <qstring.h>
#include <qregexp.h>

#include "VMGlobalSettings.h"
#include "QIHotKeyEdit.h"
#include "COMDefs.h"

#include <qvariant.h>

/** @class VMGlobalSettingsData
 *
 *  The VMGlobalSettingsData class incapsulates the global settings
 *  of the VirtualBox.
 */

VMGlobalSettingsData::VMGlobalSettingsData()
{
    /* default settings */
#if defined (Q_WS_WIN32)
    hostkey = 0xA3; // VK_RCONTROL
//    hostkey = 165; // VK_RMENU
#elif defined (Q_WS_X11)
    hostkey = 0xffe4; // XK_Control_R
//    hostkey = 65514; // XK_Alt_R
#elif defined (Q_WS_MAC)
    hostkey = 0x36; // QZ_RMETA
//    hostkey = 0x3e; // QZ_RCTRL
//    hostkey = 0x3d; // QZ_RALT
#else
# warning "port me!"
    hostkey = 0;
#endif
    autoCapture = true;
    guiFeatures = QString::null;
    languageId  = QString::null;
}

VMGlobalSettingsData::VMGlobalSettingsData( const VMGlobalSettingsData &that ) {
    hostkey = that.hostkey;
    autoCapture = that.autoCapture;
    guiFeatures = that.guiFeatures;
    languageId  = that.languageId;
}

VMGlobalSettingsData::~VMGlobalSettingsData()
{
}

bool VMGlobalSettingsData::operator==( const VMGlobalSettingsData &that ) const
{
    return this == &that || (
        hostkey == that.hostkey &&
        autoCapture == that.autoCapture &&
        guiFeatures == that.guiFeatures &&
        languageId  == that.languageId
    );
}

/** @class VMGlobalSettings
 *
 *  The VMGlobalSettings class is a wrapper class for VMGlobalSettingsData
 *  to implement implicit sharing of VirtualBox global data.
 */

static struct
{
    const char *publicName;
    const char *name;
    const char *rx;
    bool canDelete;
}
gPropertyMap[] =
{
    { "GUI/Input/HostKey",      "hostKey",      "\\d*[1-9]\\d*", true },
    { "GUI/Input/AutoCapture",  "autoCapture",  "true|false", true },
    { "GUI/Customizations",     "guiFeatures",  "\\S+", true },
    /* LanguageID regexp must correlate with gVBoxLangIDRegExp in
     * VBoxGlobal.cpp */
    { "GUI/LanguageID",         "languageId",   "(([a-z]{2})(_([A-Z]{2}))?)|(built_in)", true },
};

void VMGlobalSettings::setHostKey (int key)
{
    if (!QIHotKeyEdit::isValidKey (key))
    {
        last_err = tr ("'%1 (0x%2)' is an invalid host key code.")
                       .arg (key).arg (key, 0, 16);
        return;
    }

    mData()->hostkey = key;
    resetError();
}

bool VMGlobalSettings::isFeatureActive (const char *aFeature) const
{
    QStringList featureList = QStringList::split (',', data()->guiFeatures);
    return featureList.contains (aFeature);
}

/**
 *  Loads the settings from the (global) extra data area of VirtualBox.
 *
 *  If an error occures while accessing extra data area, the method immediately
 *  returns and the vbox argument will hold all error info (and therefore
 *  vbox.isOk() will be false to indicate this).
 *
 *  If an error occures while setting the value of some property, the method
 *  also returns immediately. #operator !() will return false in this case
 *  and #lastError() will contain the error message.
 *
 *  @note   This method emits the #propertyChanged() signal.
 */
void VMGlobalSettings::load (CVirtualBox &vbox)
{
    for (size_t i = 0; i < SIZEOF_ARRAY (gPropertyMap); i++)
    {
        QString value = vbox.GetExtraData (gPropertyMap [i].publicName);
        if (!vbox.isOk())
            return;
        // null value means the key is absent. it is ok, the default will apply
        if (value.isNull())
            continue;
        // try to set the property validating it against rx
        setPropertyPrivate (i, value);
        if (!(*this))
            break;
    }
}

/**
 *  Saves the settings to the (global) extra data area of VirtualBox.
 *
 *  If an error occures while accessing extra data area, the method immediately
 *  returns and the vbox argument will hold all error info (and therefore
 *  vbox.isOk() will be false to indicate this).
 */
void VMGlobalSettings::save (CVirtualBox &vbox) const
{
    for (size_t i = 0; i < SIZEOF_ARRAY (gPropertyMap); i++)
    {
        QVariant value = property (gPropertyMap [i].name);
        Assert (value.isValid() && value.canCast (QVariant::String));

        vbox.SetExtraData (gPropertyMap [i].publicName, value.toString());
        if (!vbox.isOk())
            return;
    }
}

/**
 *  Returns a value of the property with the given public name
 *  or QString::null if there is no such public property.
 */
QString VMGlobalSettings::publicProperty (const QString &publicName) const
{
    for (size_t i = 0; i < SIZEOF_ARRAY (gPropertyMap); i++)
    {
        if (gPropertyMap [i].publicName == publicName)
        {
            QVariant value = property (gPropertyMap [i].name);
            Assert (value.isValid() && value.canCast (QVariant::String));

            if (value.isValid() && value.canCast (QVariant::String))
                return value.toString();
            else
                break;
        }
    }

    return QString::null;
}

/**
 *  Sets a value of the property with the given public name.
 *  Returns false if such property is not found, and true otherwise.
 *
 *  This method (as opposed to #setProperty (const char *name, const QVariant& value))
 *  validates the value against the property's regexp.
 *
 *  If an error occures while setting the value of the property, #operator !()
 *  will return false after this method returns, and #lastError() will contain
 *  the error message.
 *
 *  @note   This method emits the #propertyChanged() signal.
 */
bool VMGlobalSettings::setPublicProperty (const QString &publicName, const QString &value)
{
    for (size_t i = 0; i < SIZEOF_ARRAY (gPropertyMap); i++)
    {
        if (gPropertyMap [i].publicName == publicName)
        {
            setPropertyPrivate (i, value);
            return true;
        }
    }

    return false;
}

void VMGlobalSettings::setPropertyPrivate (int index, const QString &value)
{
    if (value.isNull())
    {
        if (!gPropertyMap [index].canDelete)
        {
            last_err = tr ("Cannot delete the key '%1'.")
                .arg (gPropertyMap [index].publicName);
            return;
        }
    }
    else
    {
        if (!QRegExp (gPropertyMap [index].rx).exactMatch (value))
        {
            last_err = tr ("The value '%1' of the key '%2' doesn't match the "
                           "regexp constraint '%3'.")
                .arg (value, gPropertyMap [index].publicName,
                      gPropertyMap [index].rx);
            return;
        }
    }

    QVariant oldVal = property (gPropertyMap [index].name);
    Assert (oldVal.isValid() && oldVal.canCast (QVariant::String));

    if (oldVal.isValid() && oldVal.canCast (QVariant::String) &&
        oldVal.toString() != value)
    {
        bool ok = setProperty (gPropertyMap [index].name, value);
        Assert (ok);
        if (ok)
        {
            last_err = QString::null;
            emit propertyChanged (gPropertyMap [index].publicName,
                                  gPropertyMap [index].name);
        }
    }
}

