/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMSettingsUSB class implementation
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
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

#include "VBoxVMSettingsUSB.h"
#include "VBoxGlobalSettingsDlg.h"
#include "VBoxVMSettingsDlg.h"
#include "VBoxVMSettingsUtils.h"
#include "QIWidgetValidator.h"
#include "VBoxToolBar.h"
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"

inline static QString emptyToNull (const QString &str)
{
    return str.isEmpty() ? QString::null : str;
}

VBoxVMSettingsUSB* VBoxVMSettingsUSB::mSettings = 0;

void VBoxVMSettingsUSB::getFrom (QWidget *aPage,
                                 VBoxGlobalSettingsDlg *aDlg,
                                 const QString &aPath)
{
    mSettings = new VBoxVMSettingsUSB (aPage, HostType, aDlg, aPath);
    QVBoxLayout *layout = new QVBoxLayout (aPage);
    layout->setContentsMargins (0, 0, 0, 0);
    layout->addWidget (mSettings);

    mSettings->getFromHost();

    /* Fixing tab-order */
    setTabOrder (aDlg->mTwSelector, mSettings->mTwFilters);
    setTabOrder (mSettings->mTwFilters, mSettings->mLeName);
    setTabOrder (mSettings->mLeName, mSettings->mLeVendorID);
    setTabOrder (mSettings->mLeVendorID, mSettings->mLeManufacturer);
    setTabOrder (mSettings->mLeManufacturer, mSettings->mLeProductID);
    setTabOrder (mSettings->mLeProductID, mSettings->mLeProduct);
    setTabOrder (mSettings->mLeProduct, mSettings->mLeRevision);
    setTabOrder (mSettings->mLeRevision, mSettings->mLeSerialNo);
    setTabOrder (mSettings->mLeSerialNo, mSettings->mLePort);
    setTabOrder (mSettings->mLePort, mSettings->mCbAction);
}

void VBoxVMSettingsUSB::getFrom (const CMachine &aMachine,
                                 QWidget *aPage,
                                 VBoxVMSettingsDlg *aDlg,
                                 const QString &aPath)
{
    mSettings = new VBoxVMSettingsUSB (aPage, MachineType, aDlg, aPath);
    QVBoxLayout *layout = new QVBoxLayout (aPage);
    layout->setContentsMargins (0, 0, 0, 0);
    layout->addWidget (mSettings);

    mSettings->getFromMachine (aMachine);

    /* Fixing tab-order */
    setTabOrder (aDlg->mTwSelector, mSettings->mGbUSB);
    setTabOrder (mSettings->mGbUSB, mSettings->mCbUSB2);
    setTabOrder (mSettings->mCbUSB2, mSettings->mTwFilters);
    setTabOrder (mSettings->mTwFilters, mSettings->mLeName);
    setTabOrder (mSettings->mLeName, mSettings->mLeVendorID);
    setTabOrder (mSettings->mLeVendorID, mSettings->mLeManufacturer);
    setTabOrder (mSettings->mLeManufacturer, mSettings->mLeProductID);
    setTabOrder (mSettings->mLeProductID, mSettings->mLeProduct);
    setTabOrder (mSettings->mLeProduct, mSettings->mLeRevision);
    setTabOrder (mSettings->mLeRevision, mSettings->mLeSerialNo);
    setTabOrder (mSettings->mLeSerialNo, mSettings->mLePort);
    setTabOrder (mSettings->mLePort, mSettings->mCbRemote);
}

void VBoxVMSettingsUSB::putBackTo()
{
    if (mSettings)
        mSettings->mType == MachineType ? mSettings->putBackToMachine() :
                                          mSettings->putBackToHost();
}


VBoxVMSettingsUSB::VBoxVMSettingsUSB (QWidget *aParent,
                                      FilterType aType,
                                      QWidget *aDlg,
                                      const QString &aPath)
    : QIWithRetranslateUI<QWidget> (aParent)
    , mType (aType)
{
    /* Apply UI decorations */
    Ui::VBoxVMSettingsUSB::setupUi (this);

    /* Setup validation */
    mValidator = new QIWidgetValidator (aPath, aParent, aDlg);
    connect (mValidator, SIGNAL (validityChanged (const QIWidgetValidator *)),
             aDlg, SLOT (enableOk (const QIWidgetValidator *)));

    /* Prepare actions */
    mNewAction = new QAction (mTwFilters);
    mAddAction = new QAction (mTwFilters);
    mDelAction = new QAction (mTwFilters);
    mMupAction = new QAction (mTwFilters);
    mMdnAction = new QAction (mTwFilters);

    mNewAction->setShortcut (QKeySequence ("Ins"));
    mAddAction->setShortcut (QKeySequence ("Alt+Ins"));
    mDelAction->setShortcut (QKeySequence ("Del"));
    mMupAction->setShortcut (QKeySequence ("Ctrl+Up"));
    mMdnAction->setShortcut (QKeySequence ("Ctrl+Down"));

    mNewAction->setIcon (VBoxGlobal::iconSet (":/usb_new_16px.png",
                                              ":/usb_new_disabled_16px.png"));
    mAddAction->setIcon (VBoxGlobal::iconSet (":/usb_add_16px.png",
                                              ":/usb_add_disabled_16px.png"));
    mDelAction->setIcon (VBoxGlobal::iconSet (":/usb_remove_16px.png",
                                              ":/usb_remove_disabled_16px.png"));
    mMupAction->setIcon (VBoxGlobal::iconSet (":/usb_moveup_16px.png",
                                              ":/usb_moveup_disabled_16px.png"));
    mMdnAction->setIcon (VBoxGlobal::iconSet (":/usb_movedown_16px.png",
                                              ":/usb_movedown_disabled_16px.png"));

    /* Prepare menu and toolbar */
    mMenu = new QMenu (mTwFilters);
    mMenu->addAction (mNewAction);
    mMenu->addAction (mAddAction);
    mMenu->addSeparator();
    mMenu->addAction (mDelAction);
    mMenu->addSeparator();
    mMenu->addAction (mMupAction);
    mMenu->addAction (mMdnAction);

    /* Prepare toolbar */
    VBoxToolBar *toolBar = new VBoxToolBar (mGbUSBFilters);
    toolBar->setUsesTextLabel (false);
    toolBar->setUsesBigPixmaps (false);
    toolBar->setOrientation (Qt::Vertical);
    toolBar->addAction (mNewAction);
    toolBar->addAction (mAddAction);
    toolBar->addAction (mDelAction);
    toolBar->addAction (mMupAction);
    toolBar->addAction (mMdnAction);
    toolBar->setSizePolicy (QSizePolicy::Fixed, QSizePolicy::MinimumExpanding);
    mGbUSBFilters->layout()->addWidget (toolBar);

    /* Setup connections */
    connect (mGbUSB, SIGNAL (toggled (bool)),
             this, SLOT (usbAdapterToggled (bool)));
    connect (mTwFilters, SIGNAL (currentItemChanged (QTreeWidgetItem*, QTreeWidgetItem*)),
             this, SLOT (currentChanged (QTreeWidgetItem*, QTreeWidgetItem*)));
    connect (mTwFilters, SIGNAL (customContextMenuRequested (const QPoint &)),
             this, SLOT (showContextMenu (const QPoint &)));

    mUSBDevicesMenu = new VBoxUSBMenu (aParent);
    connect (mUSBDevicesMenu, SIGNAL (triggered (QAction*)),
             this, SLOT (addConfirmed (QAction *)));
    connect (mNewAction, SIGNAL (triggered (bool)),
             this, SLOT (newClicked()));
    connect (mAddAction, SIGNAL (triggered (bool)),
             this, SLOT (addClicked()));
    connect (mDelAction, SIGNAL (triggered (bool)),
             this, SLOT (delClicked()));
    connect (mMupAction, SIGNAL (triggered (bool)),
             this, SLOT (mupClicked()));
    connect (mMdnAction, SIGNAL (triggered (bool)),
             this, SLOT (mdnClicked()));

    connect (mLeName, SIGNAL (textEdited (const QString &)),
             this, SLOT (setCurrentText (const QString &)));
    connect (mLeVendorID, SIGNAL (textEdited (const QString &)),
             this, SLOT (setCurrentText (const QString &)));
    connect (mLeProductID, SIGNAL (textEdited (const QString &)),
             this, SLOT (setCurrentText (const QString &)));
    connect (mLeRevision, SIGNAL (textEdited (const QString &)),
             this, SLOT (setCurrentText (const QString &)));
    connect (mLePort, SIGNAL (textEdited (const QString &)),
             this, SLOT (setCurrentText (const QString &)));
    connect (mLeManufacturer, SIGNAL (textEdited (const QString &)),
             this, SLOT (setCurrentText (const QString &)));
    connect (mLeProduct, SIGNAL (textEdited (const QString &)),
             this, SLOT (setCurrentText (const QString &)));
    connect (mLeSerialNo, SIGNAL (textEdited (const QString &)),
             this, SLOT (setCurrentText (const QString &)));
    connect (mCbRemote, SIGNAL (activated (const QString &)),
             this, SLOT (setCurrentText (const QString &)));
    connect (mCbAction, SIGNAL (activated (const QString &)),
             this, SLOT (setCurrentText (const QString &)));

    /* Setup dialog */
    mTwFilters->header()->hide();

    mCbRemote->addItem (""); /* Any */
    mCbRemote->addItem (""); /* Yes */
    mCbRemote->addItem (""); /* No */
    mLbRemote->setHidden (mType != MachineType);
    mCbRemote->setHidden (mType != MachineType);

    mCbAction->insertItem (0, ""); /* KUSBDeviceFilterAction_Ignore */
    mCbAction->insertItem (1, ""); /* KUSBDeviceFilterAction_Hold */
    mLbAction->setHidden (mType != HostType);
    mCbAction->setHidden (mType != HostType);

    /* Applying language settings */
    retranslateUi();
}

VBoxVMSettingsUSB::~VBoxVMSettingsUSB()
{
    mSettings = 0;
}

void VBoxVMSettingsUSB::getFromHost()
{
    mGbUSB->setVisible (false);

    CHostUSBDeviceFilterEnumerator en = vboxGlobal().virtualBox().GetHost()
                                        .GetUSBDeviceFilters().Enumerate();
    while (en.HasMore())
    {
        CHostUSBDeviceFilter hostFilter = en.GetNext();
        CUSBDeviceFilter filter = CUnknown (hostFilter);
        addUSBFilter (filter, false /* isNew */);
    }

    mTwFilters->setCurrentItem (mTwFilters->topLevelItem (0));
    currentChanged (mTwFilters->currentItem());
}

void VBoxVMSettingsUSB::putBackToHost()
{
    CHost host = vboxGlobal().virtualBox().GetHost();

    if (mUSBFilterListModified)
    {
        /* First, remove all old filters */
        for (ulong count = host.GetUSBDeviceFilters().GetCount(); count; -- count)
            host.RemoveUSBDeviceFilter (0);

        /* Then add all new filters */
        for (int i = 0; i < mFilters.size(); ++ i)
        {
            CUSBDeviceFilter filter = mFilters [i];
            filter.SetActive (mTwFilters->topLevelItem (i)->
                checkState (0) == Qt::Checked);
            CHostUSBDeviceFilter insertedFilter = CUnknown (filter);
            host.InsertUSBDeviceFilter (host.GetUSBDeviceFilters().GetCount(),
                                        insertedFilter);
        }
    }

    mUSBFilterListModified = false;
}

void VBoxVMSettingsUSB::getFromMachine (const CMachine &aMachine)
{
    mMachine = aMachine;

    CUSBController ctl = aMachine.GetUSBController();
    mGbUSB->setChecked (ctl.GetEnabled());
    mCbUSB2->setChecked (ctl.GetEnabledEhci());
    usbAdapterToggled (mGbUSB->isChecked());

    CUSBDeviceFilterEnumerator en = ctl.GetDeviceFilters().Enumerate();
    while (en.HasMore())
        addUSBFilter (en.GetNext(), false /* isNew */);

    mTwFilters->setCurrentItem (mTwFilters->topLevelItem (0));
    currentChanged (mTwFilters->currentItem());
}

void VBoxVMSettingsUSB::putBackToMachine()
{
    CUSBController ctl = mMachine.GetUSBController();

    ctl.SetEnabled (mGbUSB->isChecked());
    ctl.SetEnabledEhci (mCbUSB2->isChecked());

    if (mUSBFilterListModified)
    {
        /* First, remove all old filters */
        for (ulong count = ctl.GetDeviceFilters().GetCount(); count; -- count)
            ctl.RemoveDeviceFilter (0);

        /* Then add all new filters */
        for (int i = 0; i < mFilters.size(); ++ i)
        {
            CUSBDeviceFilter filter = mFilters [i];
            filter.SetActive (mTwFilters->topLevelItem (i)->
                checkState (0) == Qt::Checked);
            ctl.InsertDeviceFilter (~0, filter);
        }
    }

    mUSBFilterListModified = false;
}


void VBoxVMSettingsUSB::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxVMSettingsUSB::retranslateUi (this);

    mNewAction->setText (tr ("&Add Empty Filter"));
    mAddAction->setText (tr ("A&dd Filter From Device"));
    mDelAction->setText (tr ("&Remove Filter"));
    mMupAction->setText (tr ("&Move Filter Up"));
    mMdnAction->setText (tr ("M&ove Filter Down"));

    mNewAction->setToolTip (mNewAction->text().remove ('&') +
        QString (" (%1)").arg (mNewAction->shortcut().toString()));
    mAddAction->setToolTip (mAddAction->text().remove ('&') +
        QString (" (%1)").arg (mAddAction->shortcut().toString()));
    mDelAction->setToolTip (mDelAction->text().remove ('&') +
        QString (" (%1)").arg (mDelAction->shortcut().toString()));
    mMupAction->setToolTip (mMupAction->text().remove ('&') +
        QString (" (%1)").arg (mMupAction->shortcut().toString()));
    mMdnAction->setToolTip (mMdnAction->text().remove ('&') +
        QString (" (%1)").arg (mMdnAction->shortcut().toString()));

    mNewAction->setWhatsThis (tr ("Adds a new USB filter with all fields "
                                  "initially set to empty strings. Note "
                                  "that such a filter will match any "
                                  "attached USB device."));
    mAddAction->setWhatsThis (tr ("Adds a new USB filter with all fields "
                                  "set to the values of the selected USB "
                                  "device attached to the host PC."));
    mDelAction->setWhatsThis (tr ("Removes the selected USB filter."));
    mMupAction->setWhatsThis (tr ("Moves the selected USB filter up."));
    mMdnAction->setWhatsThis (tr ("Moves the selected USB filter down."));

    mCbRemote->setItemText (0, tr ("Any", "remote"));
    mCbRemote->setItemText (1, tr ("Yes", "remote"));
    mCbRemote->setItemText (2, tr ("No", "remote"));

    mCbAction->setItemText (0,
        vboxGlobal().toString (KUSBDeviceFilterAction_Ignore));
    mCbAction->setItemText (1,
        vboxGlobal().toString (KUSBDeviceFilterAction_Hold));

    mUSBFilterName = tr ("New Filter %1", "usb");
}


void VBoxVMSettingsUSB::usbAdapterToggled (bool aOn)
{
    mGbUSBFilters->setEnabled (aOn);
    mWtProperties->setEnabled (aOn && mTwFilters->topLevelItemCount());
}

void VBoxVMSettingsUSB::currentChanged (QTreeWidgetItem *aItem,
                                        QTreeWidgetItem *)
{
    /* Make sure only the current item if present => selected */
    if (mTwFilters->selectedItems().count() != 1 ||
        mTwFilters->selectedItems() [0] != aItem)
    {
        QList<QTreeWidgetItem*> list = mTwFilters->selectedItems();
        for (int i = 0; i < list.size(); ++ i)
            list [i]->setSelected (false);
        if (aItem)
            aItem->setSelected (true);
    }

    /* Enable/disable operational buttons */
    mDelAction->setEnabled (aItem);
    mMupAction->setEnabled (aItem && mTwFilters->itemAbove (aItem));
    mMdnAction->setEnabled (aItem && mTwFilters->itemBelow (aItem));

    /* Load filter parameters */
    CUSBDeviceFilter filter;
    if (aItem)
        filter = mFilters [mTwFilters->indexOfTopLevelItem (aItem)];

    mLeName->setText (filter.isNull() ? QString::null : filter.GetName());
    mLeVendorID->setText (filter.isNull() ? QString::null : filter.GetVendorId());
    mLeProductID->setText (filter.isNull() ? QString::null : filter.GetProductId());
    mLeRevision->setText (filter.isNull() ? QString::null : filter.GetRevision());
    mLePort->setText (filter.isNull() ? QString::null : filter.GetPort());
    mLeManufacturer->setText (filter.isNull() ? QString::null : filter.GetManufacturer());
    mLeProduct->setText (filter.isNull() ? QString::null : filter.GetProduct());
    mLeSerialNo->setText (filter.isNull() ? QString::null : filter.GetSerialNumber());
    switch (mType)
    {
        case MachineType:
        {
            QString remote = filter.isNull() ? QString::null : filter.GetRemote();
            if (remote == "yes" || remote == "true" || remote == "1")
                mCbRemote->setCurrentIndex (1);
            else if (remote == "no" || remote == "false" || remote == "0")
                mCbRemote->setCurrentIndex (2);
            else
                mCbRemote->setCurrentIndex (0);
            break;
        }
        case HostType:
        {
            CHostUSBDeviceFilter hostFilter = CUnknown (filter);
            KUSBDeviceFilterAction action = hostFilter.isNull() ?
                KUSBDeviceFilterAction_Ignore : hostFilter.GetAction();
            if (action == KUSBDeviceFilterAction_Ignore)
                mCbAction->setCurrentIndex (0);
            else if (action == KUSBDeviceFilterAction_Hold)
                mCbAction->setCurrentIndex (1);
            else
                AssertMsgFailed (("Invalid USBDeviceFilterAction type"));
            break;
        }
        default:
        {
            AssertMsgFailed (("Invalid VBoxVMSettingsUSB type"));
            break;
        }
    }

    mWtProperties->setEnabled (aItem);
}

void VBoxVMSettingsUSB::setCurrentText (const QString &aText)
{
    QObject *sdr = sender();
    QTreeWidgetItem *item = mTwFilters->currentItem();
    Assert (item);

    CUSBDeviceFilter filter =
        mFilters [mTwFilters->indexOfTopLevelItem (item)];

    /* Update filter's attribute on line-edited action */
    if (sdr == mLeName)
    {
        filter.SetName (aText);
        item->setText (lvUSBFilters_Name, aText);
    } else
    if (sdr == mLeVendorID)
    {
        filter.SetVendorId (aText);
    } else
    if (sdr == mLeProductID)
    {
        filter.SetProductId (aText);
    } else
    if (sdr == mLeRevision)
    {
        filter.SetRevision (aText);
    } else
    if (sdr == mLePort)
    {
        filter.SetPort (aText);
    } else
    if (sdr == mLeManufacturer)
    {
        filter.SetManufacturer (aText);
    } else
    if (sdr == mLeProduct)
    {
        filter.SetProduct (aText);
    } else
    if (sdr == mLeSerialNo)
    {
        filter.SetSerialNumber (aText);
    } else
    if (sdr == mCbRemote)
    {
        filter.SetRemote (mCbRemote->currentIndex() > 0 ?
                          aText.toLower() : QString::null);
    } else
    if (sdr == mCbAction)
    {
        CHostUSBDeviceFilter hostFilter = CUnknown (filter);
        hostFilter.SetAction (vboxGlobal().toUSBDevFilterAction (aText));
    }
}

void VBoxVMSettingsUSB::newClicked()
{
    /* Search for the max available filter index */
    int maxFilterIndex = 0;
    QRegExp regExp (QString ("^") + mUSBFilterName.arg ("([0-9]+)") + QString ("$"));
    QTreeWidgetItemIterator iterator (mTwFilters);
    while (*iterator)
    {
        QString filterName = (*iterator)->text (lvUSBFilters_Name);
        int pos = regExp.indexIn (filterName);
        if (pos != -1)
            maxFilterIndex = regExp.cap (1).toInt() > maxFilterIndex ?
                             regExp.cap (1).toInt() : maxFilterIndex;
        ++ iterator;
    }

    /* Creating new usb filter */
    CUSBDeviceFilter filter;

    if (mType == HostType)
    {
        CHost host = vboxGlobal().virtualBox().GetHost();
        CHostUSBDeviceFilter hostFilter = host
            .CreateUSBDeviceFilter (mUSBFilterName.arg (maxFilterIndex + 1));
        hostFilter.SetAction (KUSBDeviceFilterAction_Hold);
        filter = CUnknown (hostFilter);
    }
    else if (mType == MachineType)
    {
        filter = mMachine.GetUSBController()
            .CreateDeviceFilter (mUSBFilterName.arg (maxFilterIndex + 1));
    }
    else
    {
        AssertMsgFailed (("Invalid VBoxVMSettingsUSB type"));
    }

    filter.SetActive (true);
    addUSBFilter (filter, true /* isNew */);

    mUSBFilterListModified = true;
}

void VBoxVMSettingsUSB::addClicked()
{
    mUSBDevicesMenu->exec (QCursor::pos());
}

void VBoxVMSettingsUSB::addConfirmed (QAction *aAction)
{
    CUSBDevice usb = mUSBDevicesMenu->getUSB (aAction);
    /* if null then some other item but a USB device is selected */
    if (usb.isNull())
        return;

    /* Creating new usb filter */
    CUSBDeviceFilter filter;

    if (mType == HostType)
    {
        CHost host = vboxGlobal().virtualBox().GetHost();
        CHostUSBDeviceFilter hostFilter = host
            .CreateUSBDeviceFilter (vboxGlobal().details (usb));
        hostFilter.SetAction (KUSBDeviceFilterAction_Hold);
        filter = CUnknown (hostFilter);
    }
    else if (mType == MachineType)
    {
        filter = mMachine.GetUSBController()
            .CreateDeviceFilter (vboxGlobal().details (usb));
    }
    else
    {
        AssertMsgFailed (("Invalid VBoxVMSettingsUSB type"));
    }

    filter.SetVendorId (QString().sprintf ("%04hX", usb.GetVendorId()));
    filter.SetProductId (QString().sprintf ("%04hX", usb.GetProductId()));
    filter.SetRevision (QString().sprintf ("%04hX", usb.GetRevision()));
    /* The port property depends on the host computer rather than on the USB
     * device itself; for this reason only a few people will want to use it
     * in the filter since the same device plugged into a different socket
     * will not match the filter in this case. */
#if 0
    /// @todo set it anyway if Alt is currently pressed
    filter.SetPort (QString().sprintf ("%04hX", usb.GetPort()));
#endif
    filter.SetManufacturer (usb.GetManufacturer());
    filter.SetProduct (usb.GetProduct());
    filter.SetSerialNumber (usb.GetSerialNumber());
    filter.SetRemote (usb.GetRemote() ? "yes" : "no");

    filter.SetActive (true);
    addUSBFilter (filter, true /* isNew */);

    mUSBFilterListModified = true;
}

void VBoxVMSettingsUSB::delClicked()
{
    QTreeWidgetItem *item = mTwFilters->currentItem();
    Assert (item);
    int index = mTwFilters->indexOfTopLevelItem (item);

    delete item;
    mFilters.removeAt (index);

    /* Setup validators */
    if (!mTwFilters->topLevelItemCount() && mLeName->validator())
    {
        mLeName->setValidator (0);
        mLeVendorID->setValidator (0);
        mLeProductID->setValidator (0);
        mLeRevision->setValidator (0);
        mLePort->setValidator (0);
        mValidator->rescan();
        mValidator->revalidate();
    }

    currentChanged (mTwFilters->currentItem());
    mUSBFilterListModified = true;
}

void VBoxVMSettingsUSB::mupClicked()
{
    QTreeWidgetItem *item = mTwFilters->currentItem();
    Assert (item);

    int index = mTwFilters->indexOfTopLevelItem (item);
    QTreeWidgetItem *takenItem = mTwFilters->takeTopLevelItem (index);
    Assert (item == takenItem);
    mTwFilters->insertTopLevelItem (index - 1, takenItem);
    mFilters.swap (index, index - 1);

    mTwFilters->setCurrentItem (takenItem);
    mUSBFilterListModified = true;
}

void VBoxVMSettingsUSB::mdnClicked()
{
    QTreeWidgetItem *item = mTwFilters->currentItem();
    Assert (item);

    int index = mTwFilters->indexOfTopLevelItem (item);
    QTreeWidgetItem *takenItem = mTwFilters->takeTopLevelItem (index);
    Assert (item == takenItem);
    mTwFilters->insertTopLevelItem (index + 1, takenItem);
    mFilters.swap (index, index + 1);

    mTwFilters->setCurrentItem (takenItem);
    mUSBFilterListModified = true;
}

void VBoxVMSettingsUSB::showContextMenu (const QPoint &aPos)
{
    mMenu->exec (mTwFilters->mapToGlobal (aPos));
}

void VBoxVMSettingsUSB::addUSBFilter (const CUSBDeviceFilter &aFilter,
                                      bool isNew)
{
    QTreeWidgetItem *currentItem = isNew ?
        mTwFilters->currentItem() :
        mTwFilters->topLevelItem (mTwFilters->topLevelItemCount() - 1);

    int pos = currentItem ? mTwFilters->indexOfTopLevelItem (currentItem) : -1;
    mFilters.insert (pos + 1, aFilter);

    QTreeWidgetItem *item = pos >= 0 ?
        new QTreeWidgetItem (mTwFilters, mTwFilters->topLevelItem (pos)) :
        new QTreeWidgetItem (mTwFilters);
    item->setCheckState (0, aFilter.GetActive() ? Qt::Checked : Qt::Unchecked);
    item->setText (lvUSBFilters_Name, aFilter.GetName());

    /* Setup validators */
    if (!mLeName->validator())
    {
        mLeName->setValidator (new QRegExpValidator (QRegExp (".+"), this));
        mLeVendorID->setValidator (new QRegExpValidator (QRegExp ("[0-9a-fA-F]{0,4}"), this));
        mLeProductID->setValidator (new QRegExpValidator (QRegExp ("[0-9a-fA-F]{0,4}"), this));
        mLeRevision->setValidator (new QRegExpValidator (QRegExp ("[0-9]{0,4}"), this));
        mLePort->setValidator (new QRegExpValidator (QRegExp ("[0-9]*"), this));
        mValidator->rescan();
    }

    if (isNew)
    {
        mTwFilters->setCurrentItem (item);
        mLeName->setFocus();
    }

    mValidator->revalidate();
}

