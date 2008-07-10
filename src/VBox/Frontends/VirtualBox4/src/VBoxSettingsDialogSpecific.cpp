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
#include "VBoxGLSettingsLanguage.h"

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
    attachPage (new VBoxGLSettingsGeneral());

    attachPage (new VBoxGLSettingsInput());

    attachPage (new VBoxGLSettingsLanguage());

#ifdef ENABLE_GLOBAL_USB
    attachPage (new VBoxVMSettingsUSB (VBoxVMSettingsUSB::HostType));
#endif

    /* Update Selector with available items */
    updateAvailability();

    /* Applying language settings */
    retranslateUi();
}

void VBoxGLSettingsDlg::getFrom()
{
    CSystemProperties prop = vboxGlobal().virtualBox().GetSystemProperties();
    VBoxGlobalSettings sett = vboxGlobal().settings();

    QList<VBoxSettingsPage*> pages = mStack->findChildren<VBoxSettingsPage*>();
    foreach (VBoxSettingsPage *page, pages)
        if (page->isEnabled())
            page->getFrom (prop, sett);
}

void VBoxGLSettingsDlg::putBackTo()
{
    CSystemProperties prop = vboxGlobal().virtualBox().GetSystemProperties();
    VBoxGlobalSettings sett = vboxGlobal().settings();
    VBoxGlobalSettings newsett = sett;

    QList<VBoxSettingsPage*> pages = mStack->findChildren<VBoxSettingsPage*>();
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

    /* Remember old index */
    int cid = mSelector->currentId();
    /* Remove all items */
    mSelector->clear();

    /* General page */
    mSelector->addItem (VBoxGlobal::iconSet (":/machine_16px.png"),
                        tr ("General"), GeneralId, "#general");

    /* Input page */
    mSelector->addItem (VBoxGlobal::iconSet (":/hostkey_16px.png"),
                        tr ("Input"), InputId, "#input");

    /* Language page */
    mSelector->addItem (VBoxGlobal::iconSet (":/site_16px.png"),
                        tr ("Language"), LanguageId, "#language");

#ifdef ENABLE_GLOBAL_USB
    /* USB page */
    mSelector->addItem (VBoxGlobal::iconSet (":/usb_16px.png"),
                        tr ("USB"), USBId, "#usb");
#endif

    /* Translate the selector */
    mSelector->polish();
    
    VBoxSettingsDialog::retranslateUi();

    /* Select old remembered category */
    mSelector->selectById (cid == -1 ? 0 : cid);

    /* Update Selector with available items */
    updateAvailability();
}

QString VBoxGLSettingsDlg::dialogTitle() const
{
    return tr ("VirtualBox - %1").arg (titleExtension());
}

VBoxSettingsPage* VBoxGLSettingsDlg::attachPage (VBoxSettingsPage *aPage)
{
    mStack->addWidget (aPage);

    aPage->setOrderAfter (mSelector->widget());

    return aPage;
}

void VBoxGLSettingsDlg::updateAvailability()
{
    CHost host = vboxGlobal().virtualBox().GetHost();
    CHostUSBDeviceFilterCollection coll = host.GetUSBDeviceFilters();

    /* Show an error message (if there is any).
     * This message box may be suppressed if the user wishes so. */
    if (!host.isReallyOk())
        vboxProblem().cannotAccessUSB (host);

    if (coll.isNull())
    {
        /* Disable the USB controller category if the USB controller is
         * not available (i.e. in VirtualBox OSE) */
        mSelector->setVisibleById (USBId, false);
        int index = mSelector->idToIndex (USBId);
        if (index > -1)
        {
            if (mStack->widget (index))
                mStack->widget (index)->setEnabled (false);
        }
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
    connect (&vboxGlobal(), SIGNAL (mediaEnumFinished (const VBoxMediaList &)),
             this, SLOT (onMediaEnumerationDone()));

    /* Creating settings pages */
    VBoxSettingsPage *page = 0;

    page = attachPage (new VBoxVMSettingsGeneral());
    connect (page, SIGNAL (tableChanged()), this, SLOT (resetFirstRunFlag()));

    page = attachPage (new VBoxVMSettingsHD());
    connect (page, SIGNAL (hdChanged()), this, SLOT (resetFirstRunFlag()));

    page = attachPage (new VBoxVMSettingsCD());
    connect (page, SIGNAL (cdChanged()), this, SLOT (resetFirstRunFlag()));

    page = attachPage (new VBoxVMSettingsFD());
    connect (page, SIGNAL (fdChanged()), this, SLOT (resetFirstRunFlag()));

    attachPage (new VBoxVMSettingsAudio());

    attachPage (new VBoxVMSettingsNetworkPage());

    attachPage (new VBoxVMSettingsSerialPage());

    attachPage (new VBoxVMSettingsParallelPage());

    attachPage (new VBoxVMSettingsUSB (VBoxVMSettingsUSB::MachineType));

    attachPage (new VBoxVMSettingsSF (MachineType));

    attachPage (new VBoxVMSettingsVRDP());

    /* Applying language settings */
    retranslateUi();

    /* Setup Settings Dialog */
    if (!aCategory.isNull())
    {
        mSelector->selectByLink (aCategory);
        /* Search for a widget with the given name */
        if (!aControl.isNull())
        {
            if (QWidget *w = mStack->currentWidget()->findChild<QWidget*> (aControl))
            {
                QList<QWidget*> parents;
                QWidget *p = w;
                while ((p = p->parentWidget()) != NULL)
                {
                    if (QTabWidget *tb = qobject_cast<QTabWidget*> (p))
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
    }
}

void VBoxVMSettingsDlg::revalidate (QIWidgetValidator *aWval)
{
    /* Do individual validations for pages */
    QWidget *pg = aWval->widget();
    bool valid = aWval->isOtherValid();

    QString warningText;

    QString pageTitle = mSelector->itemTextByIndex (mStack->indexOf (pg));

    VBoxSettingsPage *page = static_cast<VBoxSettingsPage*> (pg);
    valid = page->revalidate (warningText, pageTitle);

    if (!valid)
        setWarning (tr ("%1 on the <b>%2</b> page.")
                    .arg (warningText, pageTitle));

    aWval->setOtherValid (valid);
}

void VBoxVMSettingsDlg::getFrom()
{
    QList<VBoxSettingsPage*> pages = mStack->findChildren<VBoxSettingsPage*>();
    foreach (VBoxSettingsPage *page, pages)
        page->getFrom (mMachine);

    /* Finally set the reset First Run Wizard flag to "false" to make sure
     * user will see this dialog if he hasn't change the boot-order
     * and/or mounted images configuration */
    mResetFirstRunFlag = false;
}

void VBoxVMSettingsDlg::putBackTo()
{
    QList<VBoxSettingsPage*> pages = mStack->findChildren<VBoxSettingsPage*>();
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

    /* Populate selector list */
    QWidget *curpage = 0;

    /* Remember old index */
    int cid = mSelector->currentId();
    /* Remove all items */
    mSelector->clear();

    /* General page */
    mSelector->addItem (VBoxGlobal::iconSet (":/machine_16px.png"),
                        tr ("General"), GeneralId, "#general");

    /* HD page */
    mSelector->addItem (VBoxGlobal::iconSet (":/hd_16px.png"),
                        tr ("Hard Disks"), HDId, "#hdds");

    /* CD page */
    mSelector->addItem (VBoxGlobal::iconSet (":/cd_16px.png"),
                        tr ("CD/DVD-ROM"), CDId, "#dvd");

    /* FD page */
    mSelector->addItem (VBoxGlobal::iconSet (":/fd_16px.png"),
                        tr ("Floppy"), FDId, "#floppy");

    /* Audio page */
    mSelector->addItem (VBoxGlobal::iconSet (":/sound_16px.png"),
                        tr ("Audio"), AudioId, "#audio");

    /* Network page */
    mSelector->addItem (VBoxGlobal::iconSet (":/nw_16px.png"),
                        tr ("Network"), NetworkId, "#network");

    curpage = mStack->widget (mSelector->idToIndex (NetworkId));
    if (curpage)
        qApp->sendEvent (curpage, &event);

    /* Serial page */
    mSelector->addItem (VBoxGlobal::iconSet (":/serial_port_16px.png"),
                        tr ("Serial Ports"), SerialId, "#serialPorts");

    curpage = mStack->widget (mSelector->idToIndex (SerialId));
    if (curpage)
        qApp->sendEvent (curpage, &event);

    /* Parallel page */
    mSelector->addItem (VBoxGlobal::iconSet (":/parallel_port_16px.png"),
                        tr ("Parallel Ports"), ParallelId, "#parallelPorts");

    curpage = mStack->widget (mSelector->idToIndex (ParallelId));
    if (curpage)
        qApp->sendEvent (curpage, &event);

    /* USB page */
    mSelector->addItem (VBoxGlobal::iconSet (":/usb_16px.png"),
                        tr ("USB"), USBId, "#usb");

    /* SFolders page */
    mSelector->addItem (VBoxGlobal::iconSet (":/shared_folder_16px.png"),
                        tr ("Shared Folders"), SFId, "#sfolders");

    /* VRDP page */
    mSelector->addItem (VBoxGlobal::iconSet (":/vrdp_16px.png"),
                        tr ("Remote Display"), VRDPId, "#vrdp");

    /* Translate the selector */
    mSelector->polish();
    
    VBoxSettingsDialog::retranslateUi();

    /* Select old remembered category */
    mSelector->selectById (cid == -1 ? 0 : cid);

    /* Update QTreeWidget with available items */
    updateAvailability();

    /* Revalidate all pages to retranslate the warning messages also. */
    QList<QIWidgetValidator*> l = this->findChildren<QIWidgetValidator*>();
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

VBoxSettingsPage* VBoxVMSettingsDlg::attachPage (VBoxSettingsPage *aPage)
{
    mStack->addWidget (aPage);

    QIWidgetValidator *wval = new QIWidgetValidator (mSelector->itemTextByIndex (mStack->indexOf (aPage)),
                                                     aPage, this);
    connect (wval, SIGNAL (validityChanged (const QIWidgetValidator*)),
             this, SLOT (enableOk (const QIWidgetValidator*)));
    connect (wval, SIGNAL (isValidRequested (QIWidgetValidator*)),
             this, SLOT (revalidate (QIWidgetValidator*)));

    aPage->setValidator (wval);
    aPage->setOrderAfter (mSelector->widget());

    return aPage;
}

void VBoxVMSettingsDlg::updateAvailability()
{
    if (mMachine.isNull())
        return;

    /* Parallel Port Page (currently disabled) */
    mSelector->setVisibleById (ParallelId, false);
    int index = mSelector->idToIndex (ParallelId);
    if (index > -1)
    {
        if (mStack->widget (index))
            mStack->widget (index)->setEnabled (false);
    }

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
        int index = mSelector->idToIndex (USBId);
        if (index > -1)
        {
            if (mStack->widget (index))
                mStack->widget (index)->setEnabled (false);
        }
    }

    /* VRDP Stuff */
    CVRDPServer vrdp = mMachine.GetVRDPServer();
    if (vrdp.isNull())
    {
        /* Disable the VRDP category if VRDP is
         * not available (i.e. in VirtualBox OSE) */
        mSelector->setVisibleById (VRDPId, false);
        int index = mSelector->idToIndex (VRDPId);
        if (index > -1)
        {
            if (mStack->widget (index))
                mStack->widget (index)->setEnabled (false);
        }
        /* If mMachine has something to say, show the message */
        vboxProblem().cannotLoadMachineSettings (mMachine, false /* strict */);
    }
}

