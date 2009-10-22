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
#include "VBoxVMSettingsUSBFilterDetails.h"
#include "VBoxSettingsUtils.h"
#include "QIWidgetValidator.h"
#include "VBoxToolBar.h"
#include "VBoxGlobal.h"

#include <QHeaderView>

inline static QString emptyToNull (const QString &str)
{
    return str.isEmpty() ? QString::null : str;
}

VBoxVMSettingsUSB::VBoxVMSettingsUSB (FilterType aType)
    : mValidator (0)
    , mType (aType)
    , mUSBFilterListModified (false)
{
    /* Apply UI decorations */
    Ui::VBoxVMSettingsUSB::setupUi (this);

    /* Prepare actions */
    mNewAction = new QAction (mTwFilters);
    mAddAction = new QAction (mTwFilters);
    mEdtAction = new QAction (mTwFilters);
    mDelAction = new QAction (mTwFilters);
    mMupAction = new QAction (mTwFilters);
    mMdnAction = new QAction (mTwFilters);

    mNewAction->setShortcut (QKeySequence ("Ins"));
    mAddAction->setShortcut (QKeySequence ("Alt+Ins"));
    mEdtAction->setShortcut (QKeySequence ("Ctrl+Return"));
    mDelAction->setShortcut (QKeySequence ("Del"));
    mMupAction->setShortcut (QKeySequence ("Ctrl+Up"));
    mMdnAction->setShortcut (QKeySequence ("Ctrl+Down"));

    mNewAction->setIcon (VBoxGlobal::iconSet (":/usb_new_16px.png",
                                              ":/usb_new_disabled_16px.png"));
    mAddAction->setIcon (VBoxGlobal::iconSet (":/usb_add_16px.png",
                                              ":/usb_add_disabled_16px.png"));
    mEdtAction->setIcon (VBoxGlobal::iconSet (":/usb_filter_edit_16px.png",
                                              ":/usb_filter_edit_disabled_16px.png"));
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
    mMenu->addAction (mEdtAction);
    mMenu->addSeparator();
    mMenu->addAction (mDelAction);
    mMenu->addSeparator();
    mMenu->addAction (mMupAction);
    mMenu->addAction (mMdnAction);

    /* Prepare toolbar */
    VBoxToolBar *toolBar = new VBoxToolBar (mWtFilterHandler);
    toolBar->setUsesTextLabel (false);
    toolBar->setIconSize (QSize (16, 16));
    toolBar->setOrientation (Qt::Vertical);
    toolBar->addAction (mNewAction);
    toolBar->addAction (mAddAction);
    toolBar->addAction (mEdtAction);
    toolBar->addAction (mDelAction);
    toolBar->addAction (mMupAction);
    toolBar->addAction (mMdnAction);
    toolBar->setSizePolicy (QSizePolicy::Fixed, QSizePolicy::MinimumExpanding);
    toolBar->updateGeometry();
    toolBar->setMinimumHeight (toolBar->sizeHint().height());
    mWtFilterHandler->layout()->addWidget (toolBar);

    /* Setup connections */
    connect (mGbUSB, SIGNAL (toggled (bool)),
             this, SLOT (usbAdapterToggled (bool)));
    connect (mTwFilters, SIGNAL (currentItemChanged (QTreeWidgetItem*, QTreeWidgetItem*)),
             this, SLOT (currentChanged (QTreeWidgetItem*, QTreeWidgetItem*)));
    connect (mTwFilters, SIGNAL (customContextMenuRequested (const QPoint &)),
             this, SLOT (showContextMenu (const QPoint &)));
    connect (mTwFilters, SIGNAL (itemDoubleClicked (QTreeWidgetItem *, int)),
             this, SLOT (edtClicked ()));

    mUSBDevicesMenu = new VBoxUSBMenu (this);
    connect (mUSBDevicesMenu, SIGNAL (triggered (QAction*)),
             this, SLOT (addConfirmed (QAction *)));
    connect (mNewAction, SIGNAL (triggered (bool)),
             this, SLOT (newClicked()));
    connect (mAddAction, SIGNAL (triggered (bool)),
             this, SLOT (addClicked()));
    connect (mEdtAction, SIGNAL (triggered (bool)),
             this, SLOT (edtClicked()));
    connect (mDelAction, SIGNAL (triggered (bool)),
             this, SLOT (delClicked()));
    connect (mMupAction, SIGNAL (triggered (bool)),
             this, SLOT (mupClicked()));
    connect (mMdnAction, SIGNAL (triggered (bool)),
             this, SLOT (mdnClicked()));

    /* Setup dialog */
    mTwFilters->header()->hide();

    /* Applying language settings */
    retranslateUi();
}

void VBoxVMSettingsUSB::getFrom (const CSystemProperties &, const VBoxGlobalSettings &)
{
    mGbUSB->setVisible (false);

    CHostUSBDeviceFilterVector filtvec = vboxGlobal().virtualBox().GetHost()
                                        .GetUSBDeviceFilters();
    for (int i = 0; i < filtvec.size(); ++i)
    {
        CHostUSBDeviceFilter hostFilter = filtvec[i];
        CUSBDeviceFilter filter (hostFilter);
        addUSBFilter (filter, false /* isNew */);
    }

    mTwFilters->setCurrentItem (mTwFilters->topLevelItem (0));
    currentChanged (mTwFilters->currentItem());
}

void VBoxVMSettingsUSB::putBackTo (CSystemProperties &, VBoxGlobalSettings &)
{
    CHost host = vboxGlobal().virtualBox().GetHost();

    if (mUSBFilterListModified)
    {
        /* First, remove all old filters */
        for (ulong count = host.GetUSBDeviceFilters().size(); count; -- count)
            host.RemoveUSBDeviceFilter (0);

        /* Then add all new filters */
        for (int i = 0; i < mFilters.size(); ++ i)
        {
            CUSBDeviceFilter filter = mFilters [i];
            filter.SetActive (mTwFilters->topLevelItem (i)->
                checkState (0) == Qt::Checked);
            CHostUSBDeviceFilter insertedFilter (filter);
            host.InsertUSBDeviceFilter (host.GetUSBDeviceFilters().size(),
                                        insertedFilter);
        }
    }

    mUSBFilterListModified = false;
}

void VBoxVMSettingsUSB::getFrom (const CMachine &aMachine)
{
    mMachine = aMachine;

    CUSBController ctl = aMachine.GetUSBController();
    mGbUSB->setChecked (!ctl.isNull() && ctl.GetEnabled());
    mCbUSB2->setChecked (!ctl.isNull() && ctl.GetEnabledEhci());
    usbAdapterToggled (mGbUSB->isChecked());

    if (!ctl.isNull())
    {
        CUSBDeviceFilterVector filtvec = ctl.GetDeviceFilters();
        for (int i = 0; i < filtvec.size(); ++i)
            addUSBFilter (filtvec[i], false /* isNew */);
    }

    mTwFilters->setCurrentItem (mTwFilters->topLevelItem (0));
    currentChanged (mTwFilters->currentItem());
}

void VBoxVMSettingsUSB::putBackTo()
{
    CUSBController ctl = mMachine.GetUSBController();
    if (!ctl.isNull())
    {
        ctl.SetEnabled (mGbUSB->isChecked());
        ctl.SetEnabledEhci (mCbUSB2->isChecked());

        if (mUSBFilterListModified)
        {
            /* First, remove all old filters */
            for (ulong count = ctl.GetDeviceFilters().size(); count; -- count)
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
    }
    mUSBFilterListModified = false;
}

void VBoxVMSettingsUSB::setValidator (QIWidgetValidator *aVal)
{
    mValidator = aVal;
}

void VBoxVMSettingsUSB::setOrderAfter (QWidget *aWidget)
{
    setTabOrder (aWidget, mGbUSB);
    setTabOrder (mGbUSB, mCbUSB2);
    setTabOrder (mCbUSB2, mTwFilters);
}

void VBoxVMSettingsUSB::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxVMSettingsUSB::retranslateUi (this);

    mNewAction->setText (tr ("&Add Empty Filter"));
    mAddAction->setText (tr ("A&dd Filter From Device"));
    mEdtAction->setText (tr ("&Edit Filter"));
    mDelAction->setText (tr ("&Remove Filter"));
    mMupAction->setText (tr ("&Move Filter Up"));
    mMdnAction->setText (tr ("M&ove Filter Down"));

    mNewAction->setToolTip (mNewAction->text().remove ('&') +
        QString (" (%1)").arg (mNewAction->shortcut().toString()));
    mAddAction->setToolTip (mAddAction->text().remove ('&') +
        QString (" (%1)").arg (mAddAction->shortcut().toString()));
    mEdtAction->setToolTip (mEdtAction->text().remove ('&') +
        QString (" (%1)").arg (mEdtAction->shortcut().toString()));
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
    mEdtAction->setWhatsThis (tr ("Edits the selected USB filter."));
    mDelAction->setWhatsThis (tr ("Removes the selected USB filter."));
    mMupAction->setWhatsThis (tr ("Moves the selected USB filter up."));
    mMdnAction->setWhatsThis (tr ("Moves the selected USB filter down."));

    mUSBFilterName = tr ("New Filter %1", "usb");
}

void VBoxVMSettingsUSB::usbAdapterToggled (bool aOn)
{
    mGbUSBFilters->setEnabled (aOn);
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
    mEdtAction->setEnabled (aItem);
    mDelAction->setEnabled (aItem);
    mMupAction->setEnabled (aItem && mTwFilters->itemAbove (aItem));
    mMdnAction->setEnabled (aItem && mTwFilters->itemBelow (aItem));
}

void VBoxVMSettingsUSB::newClicked()
{
    /* Search for the max available filter index */
    int maxFilterIndex = 0;
    QRegExp regExp (QString ("^") + mUSBFilterName.arg ("([0-9]+)") + QString ("$"));
    QTreeWidgetItemIterator iterator (mTwFilters);
    while (*iterator)
    {
        QString filterName = (*iterator)->text (twUSBFilters_Name);
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
        filter = hostFilter;
    }
    else if (mType == MachineType)
    {
        CUSBController ctl = mMachine.GetUSBController();
        if (ctl.isNull())
            return;
        filter = ctl.CreateDeviceFilter (mUSBFilterName.arg (maxFilterIndex + 1));
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
        filter = hostFilter;
    }
    else if (mType == MachineType)
    {
        CUSBController ctl = mMachine.GetUSBController();
        if (ctl.isNull())
            return;
        filter = ctl.CreateDeviceFilter (vboxGlobal().details (usb));
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

void VBoxVMSettingsUSB::edtClicked()
{
    QTreeWidgetItem *item = mTwFilters->currentItem();
    Assert (item);

    VBoxVMSettingsUSBFilterDetails fd (mType, this);

    CUSBDeviceFilter filter =
        mFilters [mTwFilters->indexOfTopLevelItem (item)];

    fd.mLeName->setText (filter.isNull() ? QString::null : filter.GetName());
    fd.mLeVendorID->setText (filter.isNull() ? QString::null : filter.GetVendorId());
    fd.mLeProductID->setText (filter.isNull() ? QString::null : filter.GetProductId());
    fd.mLeRevision->setText (filter.isNull() ? QString::null : filter.GetRevision());
    fd.mLePort->setText (filter.isNull() ? QString::null : filter.GetPort());
    fd.mLeManufacturer->setText (filter.isNull() ? QString::null : filter.GetManufacturer());
    fd.mLeProduct->setText (filter.isNull() ? QString::null : filter.GetProduct());
    fd.mLeSerialNo->setText (filter.isNull() ? QString::null : filter.GetSerialNumber());

    switch (mType)
    {
        case MachineType:
        {
            QString remote = filter.isNull() ? QString::null : filter.GetRemote().toLower();
            if (remote == "yes" || remote == "true" || remote == "1")
                fd.mCbRemote->setCurrentIndex (ModeOn);
            else if (remote == "no" || remote == "false" || remote == "0")
                fd.mCbRemote->setCurrentIndex (ModeOff);
            else
                fd.mCbRemote->setCurrentIndex (ModeAny);
            break;
        }
        case HostType:
        {
            CHostUSBDeviceFilter hostFilter (filter);
            KUSBDeviceFilterAction action = hostFilter.isNull() ?
                KUSBDeviceFilterAction_Ignore : hostFilter.GetAction();
            if (action == KUSBDeviceFilterAction_Ignore)
                fd.mCbAction->setCurrentIndex (0);
            else if (action == KUSBDeviceFilterAction_Hold)
                fd.mCbAction->setCurrentIndex (1);
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

    if (fd.exec() == QDialog::Accepted)
    {
        filter.SetName (fd.mLeName->text().isEmpty() ? QString::null : fd.mLeName->text());
        item->setText (twUSBFilters_Name, fd.mLeName->text());
        filter.SetVendorId (fd.mLeVendorID->text().isEmpty() ? QString::null : fd.mLeVendorID->text());
        filter.SetProductId (fd.mLeProductID->text().isEmpty() ? QString::null : fd.mLeProductID->text());
        filter.SetRevision (fd.mLeRevision->text().isEmpty() ? QString::null : fd.mLeRevision->text());
        filter.SetManufacturer (fd.mLeManufacturer->text().isEmpty() ? QString::null : fd.mLeManufacturer->text());
        filter.SetProduct (fd.mLeProduct->text().isEmpty() ? QString::null : fd.mLeProduct->text());
        filter.SetSerialNumber (fd.mLeSerialNo->text().isEmpty() ? QString::null : fd.mLeSerialNo->text());
        filter.SetPort (fd.mLePort->text().isEmpty() ? QString::null : fd.mLePort->text());
        if (mType == MachineType)
        {
            switch (fd.mCbRemote->currentIndex())
             {
                 case ModeAny: filter.SetRemote (QString::null); break;
                 case ModeOn:  filter.SetRemote ("yes"); break;
                 case ModeOff: filter.SetRemote ("no"); break;
                 default: AssertMsgFailed (("Invalid combo box index"));
             }
        }
        else
        if (mType == HostType)
        {
            CHostUSBDeviceFilter hostFilter (filter);
            hostFilter.SetAction (vboxGlobal().toUSBDevFilterAction (fd.mCbAction->currentText()));
        }
        item->setToolTip (0, vboxGlobal().toolTip (filter));
    }
}

void VBoxVMSettingsUSB::delClicked()
{
    QTreeWidgetItem *item = mTwFilters->currentItem();
    Assert (item);
    int index = mTwFilters->indexOfTopLevelItem (item);

    delete item;
    mFilters.removeAt (index);

    /* Setup validators */
    if (!mTwFilters->topLevelItemCount())
    {
        if (mValidator)
        {
            mValidator->rescan();
            mValidator->revalidate();
        }
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
    item->setText (twUSBFilters_Name, aFilter.GetName());
    item->setToolTip (0, vboxGlobal().toolTip (aFilter));

    if (isNew)
        mTwFilters->setCurrentItem (item);

    if (mValidator)
        mValidator->revalidate();
}

