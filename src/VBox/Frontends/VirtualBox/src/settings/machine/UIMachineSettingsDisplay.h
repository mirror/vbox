/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIMachineSettingsDisplay class declaration
 */

/*
 * Copyright (C) 2008-2013 Oracle Corporation
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
        , m_cGuestScreenCount(0)
        , m_f3dAccelerationEnabled(false)
#ifdef VBOX_WITH_VIDEOHWACCEL
        , m_f2dAccelerationEnabled(false)
#endif /* VBOX_WITH_VIDEOHWACCEL */
        , m_fRemoteDisplayServerSupported(false)
        , m_fRemoteDisplayServerEnabled(false)
        , m_strRemoteDisplayPort(QString())
        , m_remoteDisplayAuthType(KAuthType_Null)
        , m_uRemoteDisplayTimeout(0)
        , m_fRemoteDisplayMultiConnAllowed(false) {}
    /* Functions: */
    bool equal(const UIDataSettingsMachineDisplay &other) const
    {
        return (m_iCurrentVRAM == other.m_iCurrentVRAM) &&
               (m_cGuestScreenCount == other.m_cGuestScreenCount) &&
               (m_f3dAccelerationEnabled == other.m_f3dAccelerationEnabled) &&
#ifdef VBOX_WITH_VIDEOHWACCEL
               (m_f2dAccelerationEnabled == other.m_f2dAccelerationEnabled) &&
#endif /* VBOX_WITH_VIDEOHWACCEL */
               (m_fRemoteDisplayServerSupported == other.m_fRemoteDisplayServerSupported) &&
               (m_fRemoteDisplayServerEnabled == other.m_fRemoteDisplayServerEnabled) &&
               (m_strRemoteDisplayPort == other.m_strRemoteDisplayPort) &&
               (m_remoteDisplayAuthType == other.m_remoteDisplayAuthType) &&
               (m_uRemoteDisplayTimeout == other.m_uRemoteDisplayTimeout) &&
               (m_fRemoteDisplayMultiConnAllowed == other.m_fRemoteDisplayMultiConnAllowed);
    }
    /* Operators: */
    bool operator==(const UIDataSettingsMachineDisplay &other) const { return equal(other); }
    bool operator!=(const UIDataSettingsMachineDisplay &other) const { return !equal(other); }
    /* Variables: */
    int m_iCurrentVRAM;
    int m_cGuestScreenCount;
    bool m_f3dAccelerationEnabled;
#ifdef VBOX_WITH_VIDEOHWACCEL
    bool m_f2dAccelerationEnabled;
#endif /* VBOX_WITH_VIDEOHWACCEL */
    bool m_fRemoteDisplayServerSupported;
    bool m_fRemoteDisplayServerEnabled;
    QString m_strRemoteDisplayPort;
    KAuthType m_remoteDisplayAuthType;
    ulong m_uRemoteDisplayTimeout;
    bool m_fRemoteDisplayMultiConnAllowed;
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

    /* Load data to cache from corresponding external object(s),
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

    void setValidator(QIWidgetValidator *pValidator);
    bool revalidate(QString &strWarning, QString &strTitle);

    void setOrderAfter(QWidget *pWidget);

    void retranslateUi();

private slots:

    void sltValueChangedVRAM(int iValue);
    void sltTextChangedVRAM(const QString &strText);
    void sltValueChangedScreens(int iValue);
    void sltTextChangedScreens(const QString &strText);

private:

    void checkVRAMRequirements();
    bool shouldWeWarnAboutLowVideoMemory();

    void polishPage();

    QIWidgetValidator *m_pValidator;

    /* Guest OS type id: */
    CGuestOSType m_guestOSType;
    /* System minimum lower limit of VRAM (MiB). */
    int m_iMinVRAM;
    /* System maximum limit of VRAM (MiB). */
    int m_iMaxVRAM;
    /* Upper limit of VRAM in MiB for this dialog. This value is lower than
     * m_maxVRAM to save careless users from setting useless big values. */
    int m_iMaxVRAMVisible;
    /* Initial VRAM value when the dialog is opened. */
    int m_iInitialVRAM;
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

