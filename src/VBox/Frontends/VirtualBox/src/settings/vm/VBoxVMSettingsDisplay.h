/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMSettingsDisplay class declaration
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

#ifndef __VBoxVMSettingsDisplay_h__
#define __VBoxVMSettingsDisplay_h__

#include "COMDefs.h"
#include "UISettingsPage.h"
#include "VBoxVMSettingsDisplay.gen.h"

class VBoxVMSettingsDisplay : public UISettingsPage,
                              public Ui::VBoxVMSettingsDisplay
{
    Q_OBJECT;

public:

    VBoxVMSettingsDisplay();

#ifdef VBOX_WITH_VIDEOHWACCEL
    bool isAcceleration2DVideoSelected() const;
#endif

#ifdef VBOX_WITH_CRHGSMI
    bool isAcceleration3DSelected() const;
    int getMinVramSizeMBForWddm3D() const;
    int getVramSizeMB() const;
    void setWddmMode(bool bWddm);
#endif

protected:

    void getFrom (const CMachine &aMachine);
    void putBackTo();

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

    void checkMultiMonitorReqs();

    CMachine mMachine;
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
    /* specifies whether the guest os is wddm-capable */
    bool m_bWddmMode;
#endif
};

#endif // __VBoxVMSettingsDisplay_h__

