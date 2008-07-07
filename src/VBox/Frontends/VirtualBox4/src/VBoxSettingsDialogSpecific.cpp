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

    /* Applying language settings */
    retranslateUi();

    /* Creating settings pages */
    attachPage ((VBoxSettingsPage*) new VBoxGLSettingsGeneral());

    attachPage ((VBoxSettingsPage*) new VBoxGLSettingsInput());

    attachPage ((VBoxSettingsPage*) new VBoxGLSettingsLanguage());

#ifdef ENABLE_GLOBAL_USB
    attachPage ((VBoxSettingsPage*) new VBoxVMSettingsUSB (VBoxVMSettingsUSB::HostType));
#endif

    /* Update QTreeWidget with available items */
    updateAvailability();
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
    setWindowTitle (tr ("VirtualBox Preferences"));

    /* Remember old index */
    int ci = mTwSelector->indexOfTopLevelItem (mTwSelector->currentItem());

    mTwSelector->clear();

    /* Populate selector list */
    QTreeWidgetItem *item = 0;

    /* General page */
    item = new QTreeWidgetItem (mTwSelector, QStringList() << " General "
                                             << "00" << "#general");
    item->setIcon (treeWidget_Category,
                   VBoxGlobal::iconSet (":/machine_16px.png"));

    /* Input page */
    item = new QTreeWidgetItem (mTwSelector, QStringList() << " Input "
                                             << "01" << "#input");
    item->setIcon (treeWidget_Category,
                   VBoxGlobal::iconSet (":/hostkey_16px.png"));

    /* Language page */
    item = new QTreeWidgetItem (mTwSelector, QStringList() << " Language "
                                             << "02" << "#language");
    item->setIcon (treeWidget_Category,
                   VBoxGlobal::iconSet (":/site_16px.png"));

#ifdef ENABLE_GLOBAL_USB
    /* USB page */
    item = new QTreeWidgetItem (mTwSelector, QStringList() << " USB "
                                             << "03" << "#usb");
    item->setIcon (treeWidget_Category,
                   VBoxGlobal::iconSet (":/usb_16px.png"));
#endif

    VBoxSettingsDialog::retranslateUi();

    /* Set the old index */
    mTwSelector->setCurrentItem (mTwSelector->topLevelItem (ci == -1 ? 0 : ci));

    /* Update QTreeWidget with available items */
    updateAvailability();
}

VBoxSettingsPage* VBoxGLSettingsDlg::attachPage (VBoxSettingsPage *aPage)
{
    mStack->addWidget (aPage);

    aPage->setOrderAfter (mTwSelector);

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
        QTreeWidgetItem *usbItem = findItem (mTwSelector, "#usb",
                                             treeWidget_Link);
        if (usbItem)
        {
            usbItem->setHidden (true);
            int index = mTwSelector->indexOfTopLevelItem (usbItem);
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

    /* Applying language settings */
    retranslateUi();

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

    /* Setup Settings Dialog */
    if (!aCategory.isNull())
    {
        /* Search for a list view item corresponding to the category */
        if (QTreeWidgetItem *item = findItem (mTwSelector, aCategory, treeWidget_Link))
        {
            mTwSelector->setCurrentItem (item);
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
}

void VBoxVMSettingsDlg::revalidate (QIWidgetValidator *aWval)
{
    /* Do individual validations for pages */
    QWidget *pg = aWval->widget();
    bool valid = aWval->isOtherValid();

    QString warningText;
    QString pageTitle = pagePath (pg);

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

    /* Remember old index */
    int ci = mTwSelector->indexOfTopLevelItem (mTwSelector->currentItem());

    mTwSelector->clear();

    /* We have to make sure that the Serial & Network subpages are retranslated
     * before they are revalidated. Cause: They do string comparing within
     * vboxGlobal which is retranslated at that point already. */
    QEvent event (QEvent::LanguageChange);

    /* Populate selector list */
    QTreeWidgetItem *item = 0;
    QWidget *curpage = 0;

    /* General page */
    item = new QTreeWidgetItem (mTwSelector, QStringList() << " General "
                                             << "00" << "#general");
    item->setIcon (treeWidget_Category,
                   VBoxGlobal::iconSet (":/machine_16px.png"));

    /* HD page */
    item = new QTreeWidgetItem (mTwSelector, QStringList() << " Hard Disks "
                                             << "01" << "#hdds");
    item->setIcon (treeWidget_Category,
                   VBoxGlobal::iconSet (":/hd_16px.png"));

    /* CD page */
    item = new QTreeWidgetItem (mTwSelector, QStringList() << " CD/DVD-ROM "
                                             << "02" << "#dvd");
    item->setIcon (treeWidget_Category,
                   VBoxGlobal::iconSet (":/cd_16px.png"));

    /* FD page */
    item = new QTreeWidgetItem (mTwSelector, QStringList() << " Floppy "
                                             << "03" << "#floppy");
    item->setIcon (treeWidget_Category,
                   VBoxGlobal::iconSet (":/fd_16px.png"));

    /* Audio page */
    item = new QTreeWidgetItem (mTwSelector, QStringList() << " Audio "
                                             << "04" << "#audio");
    item->setIcon (treeWidget_Category,
                   VBoxGlobal::iconSet (":/sound_16px.png"));

    /* Network page */
    item = new QTreeWidgetItem (mTwSelector, QStringList() << " Network "
                                             << "05" << "#network");
    item->setIcon (treeWidget_Category,
                   VBoxGlobal::iconSet (":/nw_16px.png"));
    curpage = mStack->widget (mTwSelector->indexOfTopLevelItem (item));
    if (curpage)
        qApp->sendEvent (curpage, &event);

    /* Serial page */
    item = new QTreeWidgetItem (mTwSelector, QStringList() << " Serial Ports "
                                             << "06" << "#serialPorts");
    item->setIcon (treeWidget_Category,
                   VBoxGlobal::iconSet (":/serial_port_16px.png"));
    curpage = mStack->widget (mTwSelector->indexOfTopLevelItem (item));
    if (curpage)
        qApp->sendEvent (curpage, &event);

    /* Parallel page */
    item = new QTreeWidgetItem (mTwSelector, QStringList() << " Parallel Ports "
                                             << "07" << "#parallelPorts");
    item->setIcon (treeWidget_Category,
                   VBoxGlobal::iconSet (":/parallel_port_16px.png"));
    curpage = mStack->widget (mTwSelector->indexOfTopLevelItem (item));
    if (curpage)
        qApp->sendEvent (curpage, &event);

    /* USB page */
    item = new QTreeWidgetItem (mTwSelector, QStringList() << " USB "
                                             << "08" << "#usb");
    item->setIcon (treeWidget_Category,
                   VBoxGlobal::iconSet (":/usb_16px.png"));

    /* SFolders page */
    item = new QTreeWidgetItem (mTwSelector, QStringList() << " Shared Folders "
                                             << "09" << "#sfolders");
    item->setIcon (treeWidget_Category,
                   VBoxGlobal::iconSet (":/shared_folder_16px.png"));

    /* VRDP page */
    item = new QTreeWidgetItem (mTwSelector, QStringList() << " Remote Display "
                                             << "10" << "#vrdp");
    item->setIcon (treeWidget_Category,
                   VBoxGlobal::iconSet (":/vrdp_16px.png"));

    VBoxSettingsDialog::retranslateUi();

    /* Set the old index */
    mTwSelector->setCurrentItem (mTwSelector->topLevelItem (ci == -1 ? 0 : ci));

    /* Update QTreeWidget with available items */
    updateAvailability();

    /* Revalidate all pages to retranslate the warning messages also. */
    QList<QIWidgetValidator*> l = this->findChildren<QIWidgetValidator*>();
    foreach (QIWidgetValidator *wval, l)
        if (!wval->isValid())
            revalidate (wval);
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

    QIWidgetValidator *wval = new QIWidgetValidator (pagePath (aPage),
                                                     aPage, this);
    connect (wval, SIGNAL (validityChanged (const QIWidgetValidator*)),
             this, SLOT (enableOk (const QIWidgetValidator*)));
    connect (wval, SIGNAL (isValidRequested (QIWidgetValidator*)),
             this, SLOT (revalidate (QIWidgetValidator*)));

    aPage->setValidator (wval);
    aPage->setOrderAfter (mTwSelector);

    return aPage;
}

QString VBoxVMSettingsDlg::dialogTitle() const
{
    QString dialogTitle;
    if (!mMachine.isNull())
        dialogTitle = tr ("%1 - Settings").arg (mMachine.GetName());
    return dialogTitle;
}

void VBoxVMSettingsDlg::updateAvailability()
{
    if (mMachine.isNull())
        return;

    /* Parallel Port Page (currently disabled) */
    QTreeWidgetItem *parallelItem =
        findItem (mTwSelector, "#parallelPorts", treeWidget_Link);
    Assert (parallelItem);
    if (parallelItem)
    {
        parallelItem->setHidden (true);
        int index = mTwSelector->indexOfTopLevelItem (parallelItem);
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
        QTreeWidgetItem *usbItem =
            findItem (mTwSelector, "#usb", treeWidget_Link);
        Assert (usbItem);
        if (usbItem)
        {
            usbItem->setHidden (true);
            int index = mTwSelector->indexOfTopLevelItem (usbItem);
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
        QTreeWidgetItem *vrdpItem =
            findItem (mTwSelector, "#vrdp", treeWidget_Link);
        Assert (vrdpItem);
        if (vrdpItem)
        {
            vrdpItem->setHidden (true);
            int index = mTwSelector->indexOfTopLevelItem (vrdpItem);
            if (mStack->widget (index))
                mStack->widget (index)->setEnabled (false);
        }
        /* If mMachine has something to say, show the message */
        vboxProblem().cannotLoadMachineSettings (mMachine, false /* strict */);
    }
}

