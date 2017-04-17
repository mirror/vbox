/* $Id$ */
/** @file
 * VBox Qt GUI - VBoxGlobalSettingsData, VBoxGlobalSettings class declarations.
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

#ifndef __VBoxGlobalSettings_h__
#define __VBoxGlobalSettings_h__

#include "CIShared.h"

/* Qt includes */
#include <QObject>

class CVirtualBox;

class VBoxGlobalSettingsData
{
public:

    VBoxGlobalSettingsData();
    VBoxGlobalSettingsData( const VBoxGlobalSettingsData &that );
    virtual ~VBoxGlobalSettingsData();
    bool operator==( const VBoxGlobalSettingsData &that ) const;

private:

    bool hostScreenSaverDisabled;

    friend class VBoxGlobalSettings;
};

/////////////////////////////////////////////////////////////////////////////

class VBoxGlobalSettings : public QObject, public CIShared <VBoxGlobalSettingsData>
{
    Q_OBJECT
    Q_PROPERTY (bool hostScreenSaverDisabled READ hostScreenSaverDisabled WRITE setHostScreenSaverDisabled)

public:

    VBoxGlobalSettings (bool null = true)
        : CIShared <VBoxGlobalSettingsData> (null) {}
    VBoxGlobalSettings (const VBoxGlobalSettings &that)
        : QObject(), CIShared <VBoxGlobalSettingsData> (that) {}
    VBoxGlobalSettings &operator= (const VBoxGlobalSettings &that) {
        CIShared <VBoxGlobalSettingsData>::operator= (that);
        return *this;
    }

    // Properties

    bool hostScreenSaverDisabled() const { return data()->hostScreenSaverDisabled; }
    void setHostScreenSaverDisabled (bool disabled)
    {
        mData()->hostScreenSaverDisabled = disabled;
    }

    void load (CVirtualBox &vbox);
    void save (CVirtualBox &vbox) const;

    /**
     *  Returns true if the last setter or #load() operation has been failed
     *  and false otherwise.
     */
    bool operator !() const { return !last_err.isEmpty(); }

    /**
     *  Returns the description of the error set by the last setter or #load()
     *  operation, or an empty (or null) string if the last operation was
     *  successful.
     */
    QString lastError() const { return last_err; }

    QString publicProperty (const QString &publicName) const;
    bool setPublicProperty (const QString &publicName, const QString &value);

signals:

    /**
     *  This sighal is emitted only when #setPublicProperty() or #load() is called.
     *  Direct modification of properties through individual setters or through
     *  QObject::setProperty() currently cannot be tracked.
     */
    void propertyChanged (const char *publicName, const char *name);

private:

    void setPropertyPrivate (size_t index, const QString &value);
    void resetError() { last_err = QString::null; }

    QString last_err;
};

#endif // __VBoxGlobalSettings_h__

