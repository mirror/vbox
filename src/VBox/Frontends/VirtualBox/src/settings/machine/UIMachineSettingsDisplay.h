/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIMachineSettingsDisplay class declaration
 */

/*
 * Copyright (C) 2008-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIMachineSettingsDisplay_h__
#define __UIMachineSettingsDisplay_h__

/* GUI includes: */
#include "UISettingsPage.h"
#include "UIMachineSettingsDisplay.gen.h"

/* COM includes: */
#include "CGuestOSType.h"

/* Machine settings / Display page / Data: */
struct UIDataSettingsMachineDisplay
{
    /* Default constructor: */
    UIDataSettingsMachineDisplay()
        : m_iCurrentVRAM(0)
        , m_cMonitorCount(0)
        , m_f3dAccelerationEnabled(false)
#ifdef VBOX_WITH_VIDEOHWACCEL
        , m_f2dAccelerationEnabled(false)
#endif /* VBOX_WITH_VIDEOHWACCEL */
        , m_fVRDEServerSupported(false)
        , m_fVRDEServerEnabled(false)
        , m_strVRDEPort(QString())
        , m_VRDEAuthType(KAuthType_Null)
        , m_uVRDETimeout(0)
        , m_fMultipleConnectionsAllowed(false) {}
    /* Functions: */
    bool equal(const UIDataSettingsMachineDisplay &other) const
    {
        return (m_iCurrentVRAM == other.m_iCurrentVRAM) &&
               (m_cMonitorCount == other.m_cMonitorCount) &&
               (m_f3dAccelerationEnabled == other.m_f3dAccelerationEnabled) &&
#ifdef VBOX_WITH_VIDEOHWACCEL
               (m_f2dAccelerationEnabled == other.m_f2dAccelerationEnabled) &&
#endif /* VBOX_WITH_VIDEOHWACCEL */
               (m_fVRDEServerSupported == other.m_fVRDEServerSupported) &&
               (m_fVRDEServerEnabled == other.m_fVRDEServerEnabled) &&
               (m_strVRDEPort == other.m_strVRDEPort) &&
               (m_VRDEAuthType == other.m_VRDEAuthType) &&
               (m_uVRDETimeout == other.m_uVRDETimeout) &&
               (m_fMultipleConnectionsAllowed == other.m_fMultipleConnectionsAllowed);
    }
    /* Operators: */
    bool operator==(const UIDataSettingsMachineDisplay &other) const { return equal(other); }
    bool operator!=(const UIDataSettingsMachineDisplay &other) const { return !equal(other); }
    /* Variables: */
    int m_iCurrentVRAM;
    int m_cMonitorCount;
    bool m_f3dAccelerationEnabled;
#ifdef VBOX_WITH_VIDEOHWACCEL
    bool m_f2dAccelerationEnabled;
#endif /* VBOX_WITH_VIDEOHWACCEL */
    bool m_fVRDEServerSupported;
    bool m_fVRDEServerEnabled;
    QString m_strVRDEPort;
    KAuthType m_VRDEAuthType;
    ulong m_uVRDETimeout;
    bool m_fMultipleConnectionsAllowed;
};
typedef UISettingsCache<UIDataSettingsMachineDisplay> UICacheSettingsMachineDisplay;

/* Machine settings / Display page: */
class UIMachineSettingsDisplay : public UISettingsPageMachine,
                              public Ui::UIMachineSettingsDisplay
{
    Q_OBJECT;

public:

    UIMachineSettingsDisplay();

    void setGuestOSType(CGuestOSType guestOSType);

#ifdef VBOX_WITH_VIDEOHWACCEL
    bool isAcceleration2DVideoSelected() const;
#endif /* VBOX_WITH_VIDEOHWACCEL */

protected:

    /* Load data to cashe from corresponding external object(s),
     * this task COULD be performed in other than GUI thread: */
    void loadToCacheFrom(QVariant &data);
    /* Load data to corresponding widgets from cache,
     * this task SHOULD be performed in GUI thread only: */
    void getFromCache();

    /* Save data from corresponding widgets to cache,
     * this task SHOULD be performed in GUI thread only: */
    void putToCache();
    /* Save data from cache to corresponding external object(s),
     * this task COULD be performed in other than GUI thread: */
    void saveFromCacheTo(QVariant &data);

    /* Page changed: */
    bool changed() const { return m_cache.wasChanged(); }

    void setValidator (QIWidgetValidator *aVal);
    bool revalidate (QString &aWarning, QString &aTitle);

    void setOrderAfter (QWidget *aWidget);

    void retranslateUi();

private slots:

    void valueChangedVRAM (int aVal);
    void textChangedVRAM (const QString &aText);
    void valueChangedMonitors (int aVal);
    void textChangedMonitors (const QString &aText);

private:

    void checkVRAMRequirements();
    bool shouldWeWarnAboutLowVideoMemory();

    void polishPage();

    QIWidgetValidator *mValidator;

    /* Guest OS type id: */
    CGuestOSType m_guestOSType;
    /* System minimum lower limit of VRAM (MiB). */
    int m_minVRAM;
    /* System maximum limit of VRAM (MiB). */
    int m_maxVRAM;
    /* Upper limit of VRAM in MiB for this dialog. This value is lower than
     * m_maxVRAM to save careless users from setting useless big values. */
    int m_maxVRAMVisible;
    /* Initial VRAM value when the dialog is opened. */
    int m_initialVRAM;
#ifdef VBOX_WITH_VIDEOHWACCEL
    /* Specifies whether the guest OS supports 2D video-acceleration: */
    bool m_f2DVideoAccelerationSupported;
#endif /* VBOX_WITH_VIDEOHWACCEL */
#ifdef VBOX_WITH_CRHGSMI
    /* Specifies whether the guest OS supports WDDM: */
    bool m_fWddmModeSupported;
#endif /* VBOX_WITH_CRHGSMI */

    /* Cache: */
    UICacheSettingsMachineDisplay m_cache;
};

#endif // __UIMachineSettingsDisplay_h__

