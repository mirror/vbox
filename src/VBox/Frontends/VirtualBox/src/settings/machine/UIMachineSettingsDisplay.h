/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIMachineSettingsDisplay class declaration
 */

/*
 * Copyright (C) 2008-2010 Oracle Corporation
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

#include "COMDefs.h"
#include "UISettingsPage.h"
#include "UIMachineSettingsDisplay.gen.h"

/* Machine settings / Display page / Cache: */
struct UISettingsCacheMachineDisplay
{
    int m_iCurrentVRAM;
    int m_cMonitorCount;
    bool m_f3dAccelerationEnabled;
    bool m_f2dAccelerationEnabled;
    bool m_fVRDEServerSupported;
    bool m_fVRDEServerEnabled;
    QString m_strVRDEPort;
    KAuthType m_iVRDEAuthType;
    ulong m_uVRDETimeout;
    bool m_fMultipleConnectionsAllowed;
};

/* Machine settings / Display page: */
class UIMachineSettingsDisplay : public UISettingsPageMachine,
                              public Ui::UIMachineSettingsDisplay
{
    Q_OBJECT;

public:

    UIMachineSettingsDisplay();

#ifdef VBOX_WITH_VIDEOHWACCEL
    bool isAcceleration2DVideoSelected() const;
#endif

#ifdef VBOX_WITH_CRHGSMI
    void setWddmMode(bool bWddm);
#endif

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

    QIWidgetValidator *mValidator;

    /* System minimum lower limit of VRAM (MiB). */
    int m_minVRAM;
    /* System maximum limit of VRAM (MiB). */
    int m_maxVRAM;
    /* Upper limit of VRAM in MiB for this dialog. This value is lower than
     * m_maxVRAM to save careless users from setting useless big values. */
    int m_maxVRAMVisible;
    /* Initial VRAM value when the dialog is opened. */
    int m_initialVRAM;
#ifdef VBOX_WITH_CRHGSMI
    /* Specifies whether the guest os is wddm-capable: */
    bool m_bWddmMode;
#endif

    /* Cache: */
    UISettingsCacheMachineDisplay m_cache;
};

#endif // __UIMachineSettingsDisplay_h__

