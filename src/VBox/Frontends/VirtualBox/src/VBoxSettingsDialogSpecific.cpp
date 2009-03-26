/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxSettingsDialog class implementation
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

// #define ENABLE_GLOBAL_USB

#include "VBoxSettingsDialogSpecific.h"
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"
#include "QIWidgetValidator.h"
#include "VBoxSettingsSelector.h"

#include "VBoxGLSettingsGeneral.h"
#include "VBoxGLSettingsInput.h"
#include "VBoxGLSettingsUpdate.h"
#include "VBoxGLSettingsLanguage.h"
#include "VBoxGLSettingsNetwork.h"

#include "VBoxVMSettingsGeneral.h"
#include "VBoxVMSettingsHD.h"
#include "VBoxVMSettingsCD.h"
#include "VBoxVMSettingsFD.h"
#include "VBoxVMSettingsAudio.h"
#include "VBoxVMSettingsNetwork.h"
#include "VBoxVMSettingsSerial.h"
#include "VBoxVMSettingsParallel.h"
#include "VBoxVMSettingsUSB.h"
#include "VBoxVMSettingsSF.h"
#include "VBoxVMSettingsVRDP.h"

/* Qt includes */
#include <QStackedWidget>

VBoxGLSettingsDlg::VBoxGLSettingsDlg (QWidget *aParent)
    : VBoxSettingsDialog (aParent)
{
#ifndef Q_WS_MAC
    setWindowIcon (QIcon (":/global_settings_16px.png"));
#endif /* Q_WS_MAC */

    /* Creating settings pages */
    VBoxSettingsPage *prefPage = NULL;

    /* General page */
    prefPage = new VBoxGLSettingsGeneral();
    addItem (":/machine_32px.png", ":/machine_disabled_32px.png", ":/machine_16px.png", ":/machine_disabled_16px.png",
             GeneralId, "#general",
             prefPage);

    /* Input page */
    prefPage = new VBoxGLSettingsInput();
    addItem (":/hostkey_32px.png", ":/hostkey_disabled_32px.png", ":/hostkey_16px.png", ":/hostkey_disabled_16px.png",
             InputId, "#input",
             prefPage);

    /* Update page */
    prefPage = new VBoxGLSettingsUpdate();
    addItem (":/refresh_32px.png", ":/refresh_disabled_32px.png", ":/refresh_16px.png", ":/refresh_disabled_16px.png",
             UpdateId, "#update",
             prefPage);

    /* Language page */
    prefPage = new VBoxGLSettingsLanguage();
    addItem (":/site_32px.png", ":/site_disabled_32px.png", ":/site_16px.png", ":/site_disabled_16px.png",
             LanguageId, "#language",
             prefPage);

#ifdef ENABLE_GLOBAL_USB
    /* USB page */
    prefPage = VBoxVMSettingsUSB (VBoxVMSettingsUSB::HostType);
    addItem (":/usb_32px.png", ":/usb_disabled_32px.png", ":/usb_16px.png", ":/usb_disabled_16px.png"
             USBId, "#usb",
             prefPage);
#endif

#ifdef VBOX_WITH_NETFLT
    /* Network page */
    prefPage = new VBoxGLSettingsNetwork();
    addItem (":/nw_32px.png", ":/nw_disabled_32px.png", ":/nw_16px.png", ":/nw_disabled_16px.png",
             NetworkId, "#language",
             prefPage);
#endif

    /* Update Selector with available items */
    updateAvailability();

    /* Applying language settings */
    retranslateUi();

    /* First item as default */
    mSelector->selectById (0);
}

void VBoxGLSettingsDlg::getFrom()
{
    CSystemProperties prop = vboxGlobal().virtualBox().GetSystemProperties();
    VBoxGlobalSettings sett = vboxGlobal().settings();

    QList <VBoxSettingsPage*> pages = mSelector->settingPages();
    foreach (VBoxSettingsPage *page, pages)
        if (page->isEnabled())
            page->getFrom (prop, sett);
}

void VBoxGLSettingsDlg::putBackTo()
{
    CSystemProperties prop = vboxGlobal().virtualBox().GetSystemProperties();
    VBoxGlobalSettings sett = vboxGlobal().settings();
    VBoxGlobalSettings newsett = sett;

    QList <VBoxSettingsPage*> pages = mSelector->settingPages();
    foreach (VBoxSettingsPage *page, pages)
        if (page->isEnabled())
            page->putBackTo (prop, newsett);

    if (!prop.isOk())
        vboxProblem().cannotSetSystemProperties (prop);
    else
    {
        if (!(newsett == sett))
            vboxGlobal().setSettings (newsett);
    }
}

void VBoxGLSettingsDlg::retranslateUi()
{
    /* Set dialog's name */
    setWindowTitle (dialogTitle());

    /* General page */
    mSelector->setItemText (GeneralId, tr ("General"));

    /* Input page */
    mSelector->setItemText (InputId, tr ("Input"));

    /* Update page */
    mSelector->setItemText (UpdateId, tr ("Update"));

    /* Language page */
    mSelector->setItemText (LanguageId, tr ("Language"));

#ifdef ENABLE_GLOBAL_USB
    /* USB page */
    mSelector->setItemText (USBId, tr ("USB"));
#endif

    /* Network page */
    mSelector->setItemText (NetworkId, tr ("Network"));

    /* Translate the selector */
    mSelector->polish();

    VBoxSettingsDialog::retranslateUi();

    /* Update Selector with available items */
    updateAvailability();
}

QString VBoxGLSettingsDlg::dialogTitle() const
{
    return tr ("VirtualBox - %1").arg (titleExtension());
}

void VBoxGLSettingsDlg::updateAvailability()
{
    CHost host = vboxGlobal().virtualBox().GetHost();

    /* Show an error message (if there is any).
     * This message box may be suppressed if the user wishes so. */
    if (!host.isReallyOk())
        vboxProblem().cannotAccessUSB (host);

    CHostUSBDeviceFilterVector coll = host.GetUSBDeviceFilters();
    if (host.lastRC() == E_NOTIMPL) {
        /* Disable the USB controller category if the USB controller is
         * not available (i.e. in VirtualBox OSE). */
        mSelector->setVisibleById (USBId, false);
    }
}

VBoxVMSettingsDlg::VBoxVMSettingsDlg (QWidget *aParent,
                                      const CMachine &aMachine,
                                      const QString &aCategory,
                                      const QString &aControl)
    : VBoxSettingsDialog (aParent)
    , mMachine (aMachine)
    , mAllowResetFirstRunFlag (false)
{
#ifndef Q_WS_MAC
    setWindowIcon (QIcon (":/settings_16px.png"));
#endif /* Q_WS_MAC */

    /* Common */
    connect (&vboxGlobal(), SIGNAL (mediumEnumFinished (const VBoxMediaList &)),
             this, SLOT (onMediaEnumerationDone()));

    /* Creating settings pages */
    VBoxSettingsPage *prefPage = NULL;

    /* General page */
    prefPage = new VBoxVMSettingsGeneral();
    connect (prefPage, SIGNAL (tableChanged()), this, SLOT (resetFirstRunFlag()));
    addItem (":/machine_32px.png", ":/machine_disabled_32px.png", ":/machine_16px.png", ":/machine_disabled_16px.png",
             GeneralId, "#general",
             prefPage);

    /* Storage page */
    addItem (":/hd_32px.png", ":/hd_disabled_32px.png", ":/hd_16px.png", ":/hd_disabled_16px.png",
             StorageId, "#storage");

    /* HD page */
    prefPage = new VBoxVMSettingsHD();
    connect (prefPage, SIGNAL (hdChanged()), this, SLOT (resetFirstRunFlag()));
    addItem (":/hd_32px.png", ":/hd_disabled_32px.png", ":/hd_16px.png", ":/hd_disabled_16px.png",
             HDId, "#hdds",
             prefPage, StorageId);

    /* CD page */
    prefPage = new VBoxVMSettingsCD();
    connect (prefPage, SIGNAL (cdChanged()), this, SLOT (resetFirstRunFlag()));
    addItem (":/cd_32px.png", ":/cd_disabled_32px.png", ":/cd_16px.png", ":/cd_disabled_16px.png",
             CDId, "#dvd",
             prefPage, StorageId);

    /* FD page */
    prefPage = new VBoxVMSettingsFD();
    connect (prefPage, SIGNAL (fdChanged()), this, SLOT (resetFirstRunFlag()));
    addItem (":/fd_32px.png", ":/fd_disabled_32px.png", ":/fd_16px.png", ":/fd_disabled_16px.png",
             FDId, "#floppy",
             prefPage, StorageId);

    /* Audio page */
    prefPage = new VBoxVMSettingsAudio();
    addItem (":/sound_32px.png", ":/sound_disabled_32px.png", ":/sound_16px.png", ":/sound_disabled_16px.png",
             AudioId, "#audio",
             prefPage);

    /* Network page */
    prefPage = new VBoxVMSettingsNetworkPage();
    addItem (":/nw_32px.png", ":/nw_disabled_32px.png", ":/nw_16px.png", ":/nw_disabled_16px.png",
             NetworkId, "#network",
             prefPage);

    /* Ports page */
    addItem (":/serial_port_32px.png", ":/serial_port_disabled_32px.png", ":/serial_port_16px.png", ":/serial_port_disabled_16px.png",
             PortsId, "#ports");

    /* USB page */
    prefPage = new VBoxVMSettingsUSB (VBoxVMSettingsUSB::MachineType);
    addItem (":/usb_32px.png", ":/usb_disabled_32px.png", ":/usb_16px.png", ":/usb_disabled_16px.png",
             USBId, "#usb",
             prefPage, PortsId);

    /* Serial page */
    prefPage = new VBoxVMSettingsSerialPage();
    addItem (":/serial_port_32px.png", ":/serial_port_disabled_32px.png", ":/serial_port_16px.png", ":/serial_port_disabled_16px.png",
             SerialId, "#serialPorts",
             prefPage, PortsId);

    /* Parallel page */
    prefPage = new VBoxVMSettingsParallelPage();
    addItem (":/parallel_port_32px.png", ":/parallel_port_disabled_32px.png", ":/parallel_port_16px.png", ":/parallel_port_disabled_16px.png",
             ParallelId, "#parallelPorts",
             prefPage, PortsId);

    /* SFolders page */
    prefPage = new VBoxVMSettingsSF (MachineType);
    addItem (":/shared_folder_32px.png", ":/shared_folder_disabled_32px.png", ":/shared_folder_16px.png", ":/shared_folder_disabled_16px.png",
             SFId, "#sfolders",
             prefPage);

    /* VRDP page */
    prefPage = new VBoxVMSettingsVRDP();
    addItem (":/vrdp_32px.png", ":/vrdp_disabled_32px.png", ":/vrdp_16px.png", ":/vrdp_disabled_16px.png",
             VRDPId, "#vrdp",
             prefPage);

    /* Applying language settings */
    retranslateUi();

    /* First item as default */

    /* Setup Settings Dialog */
    if (!aCategory.isNull())
    {
        mSelector->selectByLink (aCategory);
        /* Search for a widget with the given name */
        if (!aControl.isNull())
        {
            if (QWidget *w = mStack->currentWidget()->findChild <QWidget*> (aControl))
            {
                QList <QWidget*> parents;
                QWidget *p = w;
                while ((p = p->parentWidget()) != NULL)
                {
                    if (QTabWidget *tb = qobject_cast <QTabWidget*> (p))
                    {
                        /* The tab contents widget is two steps down
                         * (QTabWidget -> QStackedWidget -> QWidget) */
                        QWidget *c = parents [parents.count() - 1];
                        if (c)
                            c = parents [parents.count() - 2];
                        if (c)
                            tb->setCurrentWidget (c);
                    }
                    parents.append (p);
                }

                w->setFocus();
            }
        }
    }else
        mSelector->selectById (0);
}

void VBoxVMSettingsDlg::getFrom()
{
    QList <VBoxSettingsPage*> pages = mSelector->settingPages();
    foreach (VBoxSettingsPage *page, pages)
        page->getFrom (mMachine);

    /* Finally set the reset First Run Wizard flag to "false" to make sure
     * user will see this dialog if he hasn't change the boot-order
     * and/or mounted images configuration */
    mResetFirstRunFlag = false;
}

void VBoxVMSettingsDlg::putBackTo()
{
    QList <VBoxSettingsPage*> pages = mSelector->settingPages();
    foreach (VBoxSettingsPage *page, pages)
        page->putBackTo();

    /* Clear the "GUI_FirstRun" extra data key in case if the boot order
     * and/or disk configuration were changed */
    if (mResetFirstRunFlag)
        mMachine.SetExtraData (VBoxDefs::GUI_FirstRun, QString::null);
}

void VBoxVMSettingsDlg::retranslateUi()
{
    /* Set dialog's name */
    setWindowTitle (dialogTitle());

    /* We have to make sure that the Serial & Network subpages are retranslated
     * before they are revalidated. Cause: They do string comparing within
     * vboxGlobal which is retranslated at that point already. */
    QEvent event (QEvent::LanguageChange);
    QWidget *page = NULL;

    /* General page */
    mSelector->setItemText (GeneralId, tr ("General"));

    /* Storage page */
    mSelector->setItemText (StorageId, tr ("Storage"));

    /* HD page */
    mSelector->setItemText (HDId, tr ("Hard Disks"));

    /* CD page */
    mSelector->setItemText (CDId, tr ("CD/DVD-ROM"));

    /* FD page */
    mSelector->setItemText (FDId, tr ("Floppy"));

    /* Audio page */
    mSelector->setItemText (AudioId, tr ("Audio"));

    /* Network page */
    mSelector->setItemText (NetworkId, tr ("Network"));
    if ((page = mSelector->idToPage (NetworkId)))
        qApp->sendEvent (page, &event);

    /* Ports page */
    mSelector->setItemText (PortsId, tr ("Ports"));

    /* Serial page */
    mSelector->setItemText (SerialId, tr ("Serial Ports"));
    if ((page = mSelector->idToPage (SerialId)))
        qApp->sendEvent (page, &event);

    /* Parallel page */
    mSelector->setItemText (ParallelId, tr ("Parallel Ports"));
    if ((page = mSelector->idToPage (ParallelId)))
        qApp->sendEvent (page, &event);

    /* USB page */
    mSelector->setItemText (USBId, tr ("USB"));

    /* SFolders page */
    mSelector->setItemText (SFId, tr ("Shared Folders"));

    /* VRDP page */
    mSelector->setItemText (VRDPId, tr ("Remote Display"));

    /* Translate the selector */
    mSelector->polish();

    VBoxSettingsDialog::retranslateUi();

    /* Update QTreeWidget with available items */
    updateAvailability();

    /* Revalidate all pages to retranslate the warning messages also. */
    QList <QIWidgetValidator*> l = this->findChildren <QIWidgetValidator*> ();
    foreach (QIWidgetValidator *wval, l)
        if (!wval->isValid())
            revalidate (wval);
}

QString VBoxVMSettingsDlg::dialogTitle() const
{
    QString dialogTitle;
    if (!mMachine.isNull())
        dialogTitle = tr ("%1 - %2").arg (mMachine.GetName())
                                    .arg (titleExtension());
    return dialogTitle;
}

void VBoxVMSettingsDlg::onMediaEnumerationDone()
{
    mAllowResetFirstRunFlag = true;
}

void VBoxVMSettingsDlg::resetFirstRunFlag()
{
    if (mAllowResetFirstRunFlag)
        mResetFirstRunFlag = true;
}

void VBoxVMSettingsDlg::updateAvailability()
{
    if (mMachine.isNull())
        return;

    /* Parallel Port Page (currently disabled) */
    mSelector->setVisibleById (ParallelId, false);

    /* USB Stuff */
    CUSBController ctl = mMachine.GetUSBController();
    /* Show an error message (if there is any).
     * Note that we don't use the generic cannotLoadMachineSettings()
     * call here because we want this message to be suppressable. */
    if (!mMachine.isReallyOk())
        vboxProblem().cannotAccessUSB (mMachine);
    if (ctl.isNull())
    {
        /* Disable the USB controller category if the USB controller is
         * not available (i.e. in VirtualBox OSE) */
        mSelector->setVisibleById (USBId, false);
    }

    /* VRDP Stuff */
    CVRDPServer vrdp = mMachine.GetVRDPServer();
    if (vrdp.isNull())
    {
        /* Disable the VRDP category if VRDP is
         * not available (i.e. in VirtualBox OSE) */
        mSelector->setVisibleById (VRDPId, false);
        /* If mMachine has something to say, show the message */
        vboxProblem().cannotLoadMachineSettings (mMachine, false /* strict */);
    }
}

