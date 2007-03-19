/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VMGlobalSettingsData, VMGlobalSettings class declarations
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

#ifndef __VMGlobalSettings_h__
#define __VMGlobalSettings_h__

#include "CIShared.h"

#include <qobject.h>

class CVirtualBox;

class VMGlobalSettingsData
{
public:

    VMGlobalSettingsData();
    VMGlobalSettingsData( const VMGlobalSettingsData &that );
    virtual ~VMGlobalSettingsData();
    bool operator==( const VMGlobalSettingsData &that ) const;

private:

    int hostkey;
    bool autoCapture;
    QString guiFeatures;

    friend class VMGlobalSettings;
};

/////////////////////////////////////////////////////////////////////////////

class VMGlobalSettings : public QObject, public CIShared <VMGlobalSettingsData>
{
    Q_OBJECT
    Q_PROPERTY (int hostKey READ hostKey WRITE setHostKey)
    Q_PROPERTY (bool autoCapture READ autoCapture WRITE setAutoCapture)
    Q_PROPERTY (QString guiFeatures READ guiFeatures WRITE setGuiFeatures)

public:

    VMGlobalSettings (bool null = true)
        : CIShared <VMGlobalSettingsData> (null) {}
    VMGlobalSettings (const VMGlobalSettings &that)
        : QObject(), CIShared <VMGlobalSettingsData> (that) {}
    VMGlobalSettings &operator= (const VMGlobalSettings &that) {
        CIShared <VMGlobalSettingsData>::operator= (that);
        return *this;
    }

    // Properties

    int hostKey() const { return data()->hostkey; }
    void setHostKey (int key);

    bool autoCapture() const { return data()->autoCapture; }
    void setAutoCapture (bool autoCapture) {
        mData()->autoCapture = autoCapture;
        resetError();
    }

    QString guiFeatures() const { return data()->guiFeatures; }
    void setGuiFeatures (const QString &aFeatures)
    {
        mData()->guiFeatures = aFeatures;
    }
    bool isFeatureActive (const char*) const;

    //

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

    void setPropertyPrivate (int index, const QString &value);
    void resetError() { last_err = QString::null; }

    QString last_err;
};

#endif // __VMGlobalSettings_h__
