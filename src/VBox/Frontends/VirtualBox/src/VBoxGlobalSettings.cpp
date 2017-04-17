/* $Id$ */
/** @file
 * VBox Qt GUI - VBoxGlobalSettingsData, VBoxGlobalSettings class implementation.
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QString>
# include <QRegExp>
# include <QVariant>

/* GUI includes: */
# include "UIDefs.h"
# include "UIExtraDataDefs.h"
# include "VBoxGlobalSettings.h"

/* COM includes: */
# include "COMEnums.h"
# include "CVirtualBox.h"

# ifdef VBOX_WS_MAC
/* Namespaces: */
using namespace UIExtraDataDefs;
# endif /* VBOX_WS_MAC */

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/** @class VBoxGlobalSettingsData
 *
 *  The VBoxGlobalSettingsData class encapsulates the global settings
 *  of the VirtualBox.
 */

VBoxGlobalSettingsData::VBoxGlobalSettingsData()
{
    /* default settings */
    maxGuestRes = QString::null;
    remapScancodes = QString::null;
    proxySettings = QString::null;
    hostScreenSaverDisabled = false;
}

VBoxGlobalSettingsData::VBoxGlobalSettingsData (const VBoxGlobalSettingsData &that)
{
    maxGuestRes = that.maxGuestRes;
    remapScancodes = that.remapScancodes;
    proxySettings = that.proxySettings;
    hostScreenSaverDisabled = that.hostScreenSaverDisabled;
}

VBoxGlobalSettingsData::~VBoxGlobalSettingsData()
{
}

bool VBoxGlobalSettingsData::operator== (const VBoxGlobalSettingsData &that) const
{
    return this == &that ||
        (maxGuestRes == that.maxGuestRes &&
         remapScancodes == that.remapScancodes &&
         proxySettings == that.proxySettings &&
         hostScreenSaverDisabled == that.hostScreenSaverDisabled
        );
}

/** @class VBoxGlobalSettings
 *
 *  The VBoxGlobalSettings class is a wrapper class for VBoxGlobalSettingsData
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
    { "GUI/MaxGuestResolution",                    "maxGuestRes",             "\\d*[1-9]\\d*,\\d*[1-9]\\d*|any|auto", true },
    { "GUI/RemapScancodes",                        "remapScancodes",          "(\\d+=\\d+,)*\\d+=\\d+", true },
    { "GUI/ProxySettings",                         "proxySettings",           "[\\s\\S]*", true },
    { "GUI/HostScreenSaverDisabled",               "hostScreenSaverDisabled", "true|false", true }
};

/**
 *  Loads the settings from the (global) extra data area of VirtualBox.
 *
 *  If an error occurs while accessing extra data area, the method immediately
 *  returns and the vbox argument will hold all error info (and therefore
 *  vbox.isOk() will be false to indicate this).
 *
 *  If an error occurs while setting the value of some property, the method
 *  also returns immediately. #operator !() will return false in this case
 *  and #lastError() will contain the error message.
 *
 *  @note   This method emits the #propertyChanged() signal.
 */
void VBoxGlobalSettings::load (CVirtualBox &vbox)
{
    for (size_t i = 0; i < SIZEOF_ARRAY(gPropertyMap); i++)
    {
        QString value = vbox.GetExtraData(gPropertyMap[i].publicName);
        if (!vbox.isOk())
            return;
        /* Check for the host key upgrade path. */
        if (   value.isEmpty()
            && QString(gPropertyMap[i].publicName) == "GUI/Input/HostKeyCombination")
            value = vbox.GetExtraData("GUI/Input/HostKey");
        /* Empty value means the key is absent. It is OK, the default will
         * apply. */
        if (value.isEmpty())
            continue;
        /* Try to set the property validating it against rx. */
        setPropertyPrivate(i, value);
        if (!(*this))
            break;
    }
}

/**
 *  Saves the settings to the (global) extra data area of VirtualBox.
 *
 *  If an error occurs while accessing extra data area, the method immediately
 *  returns and the vbox argument will hold all error info (and therefore
 *  vbox.isOk() will be false to indicate this).
 */
void VBoxGlobalSettings::save (CVirtualBox &vbox) const
{
    for (size_t i = 0; i < SIZEOF_ARRAY (gPropertyMap); i++)
    {
        QVariant value = property (gPropertyMap [i].name);
        Assert (value.isValid() && value.canConvert (QVariant::String));

        vbox.SetExtraData (gPropertyMap [i].publicName, value.toString());
        if (!vbox.isOk())
            return;
    }
}

/**
 *  Returns a value of the property with the given public name
 *  or QString::null if there is no such public property.
 */
QString VBoxGlobalSettings::publicProperty (const QString &publicName) const
{
    for (size_t i = 0; i < SIZEOF_ARRAY (gPropertyMap); i++)
    {
        if (gPropertyMap [i].publicName == publicName)
        {
            QVariant value = property (gPropertyMap [i].name);
            Assert (value.isValid() && value.canConvert (QVariant::String));

            if (value.isValid() && value.canConvert (QVariant::String))
                return value.toString();
            else
                break;
        }
    }

    return QString::null;
}

/**
 *  Sets a value of a property with the given public name.
 *  Returns false if the specified property is not found, and true otherwise.
 *
 *  This method (as opposed to #setProperty (const char *name, const QVariant& value))
 *  validates the value against the property's regexp.
 *
 *  If an error occurs while setting the value of the property, #operator !()
 *  will return false after this method returns true, and #lastError() will contain
 *  the error message.
 *
 *  @note   This method emits the #propertyChanged() signal.
 */
bool VBoxGlobalSettings::setPublicProperty (const QString &publicName, const QString &value)
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

void VBoxGlobalSettings::setPropertyPrivate (size_t index, const QString &value)
{
    if (value.isEmpty())
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
    Assert (oldVal.isValid() && oldVal.canConvert (QVariant::String));

    if (oldVal.isValid() && oldVal.canConvert (QVariant::String) &&
        oldVal.toString() != value)
    {
        bool ok = setProperty (gPropertyMap [index].name, value);
        Assert (ok);
        if (ok)
        {
            /* The individual setter may have set a specific error */
            if (!last_err.isNull())
                return;

            last_err = QString::null;
            emit propertyChanged (gPropertyMap [index].publicName,
                                  gPropertyMap [index].name);
        }
    }
}

