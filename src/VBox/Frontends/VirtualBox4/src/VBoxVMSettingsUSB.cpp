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

VBoxVMSettingsUSB::VBoxVMSettingsUSB (QWidget *aParent,
                                      FilterType aType,
                                      VBoxVMSettingsDlg *aDlg,
                                      const QString &aPath)
    : QWidget (aParent)
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

    mNewAction->setText (tr ("&Add Empty Filter"));
    mAddAction->setText (tr ("A&dd Filter From Device"));
    mDelAction->setText (tr ("&Remove Filter"));
    mMupAction->setText (tr ("&Move Filter Up"));
    mMdnAction->setText (tr ("M&ove Filter Down"));

    mNewAction->setShortcut (QKeySequence ("Ins"));
    mAddAction->setShortcut (QKeySequence ("Alt+Ins"));
    mDelAction->setShortcut (QKeySequence ("Del"));
    mMupAction->setShortcut (QKeySequence ("Ctrl+Up"));
    mMdnAction->setShortcut (QKeySequence ("Ctrl+Down"));

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

    mCbRemote->insertItem (0, tr ("Any", "remote"));
    mCbRemote->insertItem (1, tr ("Yes", "remote"));
    mCbRemote->insertItem (2, tr ("No", "remote"));
    mLbRemote->setHidden (mType != MachineType);
    mCbRemote->setHidden (mType != MachineType);

    mCbAction->insertItem (0,
        vboxGlobal().toString (KUSBDeviceFilterAction_Ignore));
    mCbAction->insertItem (1,
        vboxGlobal().toString (KUSBDeviceFilterAction_Hold));
    mLbAction->setHidden (mType != HostType);
    mCbAction->setHidden (mType != HostType);

    /* Fixing tab-order */
    setTabOrder (aDlg->mTwSelector, mGbUSB);
    setTabOrder (mGbUSB, mCbUSB2);
    setTabOrder (mCbUSB2, mTwFilters);
    setTabOrder (mTwFilters, mLeName);
    setTabOrder (mLeName, mLeVendorID);
    setTabOrder (mLeVendorID, mLeManufacturer);
    setTabOrder (mLeManufacturer, mLeProductID);
    setTabOrder (mLeProductID, mLeProduct);
    setTabOrder (mLeProduct, mLeRevision);
    setTabOrder (mLeRevision, mLeSerialNo);
    setTabOrder (mLeSerialNo, mLePort);
    setTabOrder (mLePort, mCbRemote);
    setTabOrder (mCbRemote, mCbAction);
}

void VBoxVMSettingsUSB::getFromMachine (const CMachine &aMachine,
                                        QWidget *aPage,
                                        VBoxVMSettingsDlg *aDlg,
                                        const QString &aPath)
{
    CUSBController ctl = aMachine.GetUSBController();

    /* Show an error message (if there is any).
     * Note that we don't use the generic cannotLoadMachineSettings()
     * call here because we want this message to be suppressable. */
    if (!aMachine.isReallyOk())
        vboxProblem().cannotAccessUSB (aMachine);

    if (ctl.isNull())
    {
        /* Disable the USB controller category if the USB controller is
         * not available (i.e. in VirtualBox OSE) */
        QList<QTreeWidgetItem*> items = aDlg->mTwSelector->findItems (
            "#usb", Qt::MatchExactly, listView_Link);
        QTreeWidgetItem *usbItem = items.count() ? items [0] : 0;
        Assert (usbItem);
        if (usbItem)
            usbItem->setHidden (true);
        return;
    }

    mSettings = new VBoxVMSettingsUSB (aPage, MachineType, aDlg, aPath);
    QVBoxLayout *layout = new QVBoxLayout (aPage);
    layout->setContentsMargins (0, 0, 0, 0);
    layout->addWidget (mSettings);

    mSettings->getFrom (aMachine);
}

void VBoxVMSettingsUSB::putBackToMachine()
{
    if (mSettings)
        mSettings->putBackTo();
}

void VBoxVMSettingsUSB::getFrom (const CMachine &aMachine)
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

void VBoxVMSettingsUSB::putBackTo()
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
    QString usbFilterName = tr ("New Filter %1", "usb");
    QRegExp regExp (QString ("^") + usbFilterName.arg ("([0-9]+)") + QString ("$"));
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
    CUSBDeviceFilter filter = mMachine.GetUSBController()
        .CreateDeviceFilter (usbFilterName.arg (maxFilterIndex + 1));

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

    CUSBDeviceFilter filter = mMachine.GetUSBController()
        .CreateDeviceFilter (vboxGlobal().details (usb));

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

