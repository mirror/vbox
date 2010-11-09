/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIMachineSettingsUSB class implementation
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Local includes */
#include "QIWidgetValidator.h"
#include "UIIconPool.h"
#include "VBoxGlobal.h"
#include "UIToolBar.h"
#include "UIMachineSettingsUSB.h"
#include "UIMachineSettingsUSBFilterDetails.h"

/* Global includes */
#include <QHeaderView>

UIMachineSettingsUSB::UIMachineSettingsUSB(UISettingsPageType type)
    : UISettingsPage(type)
    , mValidator(0)
    , mNewAction(0), mAddAction(0), mEdtAction(0), mDelAction(0)
    , mMupAction(0), mMdnAction(0)
    , mMenu(0), mUSBDevicesMenu(0)
    , mUSBFilterListModified(false)
{
    /* Apply UI decorations */
    Ui::UIMachineSettingsUSB::setupUi (this);

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

    mNewAction->setIcon(UIIconPool::iconSet(":/usb_new_16px.png",
                                            ":/usb_new_disabled_16px.png"));
    mAddAction->setIcon(UIIconPool::iconSet(":/usb_add_16px.png",
                                            ":/usb_add_disabled_16px.png"));
    mEdtAction->setIcon(UIIconPool::iconSet(":/usb_filter_edit_16px.png",
                                            ":/usb_filter_edit_disabled_16px.png"));
    mDelAction->setIcon(UIIconPool::iconSet(":/usb_remove_16px.png",
                                            ":/usb_remove_disabled_16px.png"));
    mMupAction->setIcon(UIIconPool::iconSet(":/usb_moveup_16px.png",
                                            ":/usb_moveup_disabled_16px.png"));
    mMdnAction->setIcon(UIIconPool::iconSet(":/usb_movedown_16px.png",
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
    UIToolBar *toolBar = new UIToolBar (mWtFilterHandler);
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
             this, SLOT (edtClicked()));
    connect (mTwFilters, SIGNAL (itemChanged (QTreeWidgetItem *, int)),
             this, SLOT (sltUpdateActivityState(QTreeWidgetItem *)));

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

#ifndef VBOX_WITH_EHCI
    mCbUSB2->setHidden(true);
#endif
}

bool UIMachineSettingsUSB::isOHCIEnabled() const
{
    return mGbUSB->isChecked();
}

/* Load data to cashe from corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void UIMachineSettingsUSB::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to properties & settings or machine: */
    fetchData(data);

    /* Depending on page type: */
    switch (type())
    {
        case UISettingsPageType_Global:
        {
            /* Fill internal variables with corresponding values: */
            m_cache.m_fUSBEnabled = false;
            m_cache.m_fEHCIEnabled = false;
            const CHostUSBDeviceFilterVector &filters = vboxGlobal().virtualBox().GetHost().GetUSBDeviceFilters();
            for (int iFilterIndex = 0; iFilterIndex < filters.size(); ++iFilterIndex)
            {
                const CHostUSBDeviceFilter &filter = filters[iFilterIndex];
                UIUSBFilterData data;
                data.m_fActive = filter.GetActive();
                data.m_strName = filter.GetName();
                data.m_strVendorId = filter.GetVendorId();
                data.m_strProductId = filter.GetProductId();
                data.m_strRevision = filter.GetRevision();
                data.m_strManufacturer = filter.GetManufacturer();
                data.m_strProduct = filter.GetProduct();
                data.m_strSerialNumber = filter.GetSerialNumber();
                data.m_strPort = filter.GetPort();
                data.m_strRemote = filter.GetRemote();
                data.m_action = filter.GetAction();
                CHostUSBDevice hostUSBDevice(filter);
                if (!hostUSBDevice.isNull())
                {
                    data.m_fHostUSBDevice = true;
                    data.m_hostUSBDeviceState = hostUSBDevice.GetState();
                }
                else
                {
                    data.m_fHostUSBDevice = false;
                    data.m_hostUSBDeviceState = KUSBDeviceState_NotSupported;
                }
                m_cache.m_items << data;
            }
            break;
        }
        case UISettingsPageType_Machine:
        {
            /* Initialize machine COM storage: */
            m_machine = data.value<UISettingsDataMachine>().m_machine;
            /* Fill internal variables with corresponding values: */
            const CUSBController &ctl = m_machine.GetUSBController();
            bool fIsControllerAvailable = !ctl.isNull();
            m_cache.m_fUSBEnabled = fIsControllerAvailable && ctl.GetEnabled();
            m_cache.m_fEHCIEnabled = fIsControllerAvailable && ctl.GetEnabledEhci();
            if (fIsControllerAvailable)
            {
                const CUSBDeviceFilterVector &filters = ctl.GetDeviceFilters();
                for (int iFilterIndex = 0; iFilterIndex < filters.size(); ++iFilterIndex)
                {
                    const CUSBDeviceFilter &filter = filters[iFilterIndex];
                    UIUSBFilterData data;
                    data.m_fActive = filter.GetActive();
                    data.m_strName = filter.GetName();
                    data.m_strVendorId = filter.GetVendorId();
                    data.m_strProductId = filter.GetProductId();
                    data.m_strRevision = filter.GetRevision();
                    data.m_strManufacturer = filter.GetManufacturer();
                    data.m_strProduct = filter.GetProduct();
                    data.m_strSerialNumber = filter.GetSerialNumber();
                    data.m_strPort = filter.GetPort();
                    data.m_strRemote = filter.GetRemote();
                    data.m_fHostUSBDevice = false;
                    m_cache.m_items << data;
                }
            }
            break;
        }
        default:
            break;
    }

    /* Upload properties & settings or machine to data: */
    uploadData(data);
}

/* Load data to corresponding widgets from cache,
 * this task SHOULD be performed in GUI thread only: */
void UIMachineSettingsUSB::getFromCache()
{
    /* Depending on page type: */
    switch (type())
    {
        case UISettingsPageType_Global:
        {
            /* Apply internal variables data to QWidget(s): */
            mGbUSB->setVisible(false);
            mCbUSB2->setVisible(false);
            break;
        }
        case UISettingsPageType_Machine:
        {
            /* Apply internal variables data to QWidget(s): */
            mGbUSB->setChecked(m_cache.m_fUSBEnabled);
            mCbUSB2->setChecked(m_cache.m_fEHCIEnabled);
            usbAdapterToggled(mGbUSB->isChecked());
            break;
        }
        default:
            break;
    }
    /* Apply internal variables data to QWidget(s): */
    for (int iFilterIndex = 0; iFilterIndex < m_cache.m_items.size(); ++iFilterIndex)
        addUSBFilter(m_cache.m_items[iFilterIndex], false /* its new? */);
    /* Choose first filter as current: */
    mTwFilters->setCurrentItem(mTwFilters->topLevelItem(0));
    currentChanged(mTwFilters->currentItem());
    /* Mark dialog as not edited:  */
    mUSBFilterListModified = false;

    /* Revalidate if possible: */
    if (mValidator) mValidator->revalidate();
}

/* Save data from corresponding widgets to cache,
 * this task SHOULD be performed in GUI thread only: */
void UIMachineSettingsUSB::putToCache()
{
    /* Depending on page type: */
    switch (type())
    {
        case UISettingsPageType_Machine:
        {
            /* Gather internal variables data from QWidget(s): */
            m_cache.m_fUSBEnabled = mGbUSB->isChecked();
            m_cache.m_fEHCIEnabled = mCbUSB2->isChecked();
            break;
        }
        default:
            break;
    }
}

/* Save data from cache to corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void UIMachineSettingsUSB::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to properties & settings or machine: */
    fetchData(data);

    /* Depending on page type: */
    switch (type())
    {
        case UISettingsPageType_Global:
        {
            /* Gather corresponding values from internal variables: */
            if (mUSBFilterListModified)
            {
                /* Get host: */
                CHost host = vboxGlobal().virtualBox().GetHost();
                /* First, remove all old filters: */
                for (ulong count = host.GetUSBDeviceFilters().size(); count; --count)
                    host.RemoveUSBDeviceFilter(0);
                /* Then add all new filters: */
                for (int iFilterIndex = 0; iFilterIndex < m_cache.m_items.size(); ++iFilterIndex)
                {
                    UIUSBFilterData data = m_cache.m_items[iFilterIndex];
                    CHostUSBDeviceFilter hostFilter = host.CreateUSBDeviceFilter(data.m_strName);
                    hostFilter.SetActive(data.m_fActive);
                    hostFilter.SetVendorId(data.m_strVendorId);
                    hostFilter.SetProductId(data.m_strProductId);
                    hostFilter.SetRevision(data.m_strRevision);
                    hostFilter.SetManufacturer(data.m_strManufacturer);
                    hostFilter.SetProduct(data.m_strProduct);
                    hostFilter.SetSerialNumber(data.m_strSerialNumber);
                    hostFilter.SetPort(data.m_strPort);
                    hostFilter.SetRemote(data.m_strRemote);
                    hostFilter.SetAction(data.m_action);
                    host.InsertUSBDeviceFilter(host.GetUSBDeviceFilters().size(), hostFilter);
                }
            }
            break;
        }
        case UISettingsPageType_Machine:
        {
            /* Initialize machine COM storage: */
            m_machine = data.value<UISettingsDataMachine>().m_machine;
            /* Get machine USB controller: */
            CUSBController ctl = m_machine.GetUSBController();
            /* Gather corresponding values from internal variables: */
            if (!ctl.isNull())
            {
                ctl.SetEnabled(m_cache.m_fUSBEnabled);
                ctl.SetEnabledEhci(m_cache.m_fEHCIEnabled);
                if (mUSBFilterListModified)
                {
                    /* First, remove all old filters: */
                    for (ulong count = ctl.GetDeviceFilters().size(); count; --count)
                        ctl.RemoveDeviceFilter(0);
                    /* Then add all new filters: */
                    for (int iFilterIndex = 0; iFilterIndex < m_cache.m_items.size(); ++iFilterIndex)
                    {
                        const UIUSBFilterData &data = m_cache.m_items[iFilterIndex];
                        CUSBDeviceFilter filter = ctl.CreateDeviceFilter(data.m_strName);
                        filter.SetActive(data.m_fActive);
                        filter.SetVendorId(data.m_strVendorId);
                        filter.SetProductId(data.m_strProductId);
                        filter.SetRevision(data.m_strRevision);
                        filter.SetManufacturer(data.m_strManufacturer);
                        filter.SetProduct(data.m_strProduct);
                        filter.SetSerialNumber(data.m_strSerialNumber);
                        filter.SetPort(data.m_strPort);
                        filter.SetRemote(data.m_strRemote);
                        ctl.InsertDeviceFilter(~0, filter);
                    }
                }
            }
            break;
        }
        default:
            break;
    }

    /* Upload properties & settings or machine to data: */
    uploadData(data);
}

void UIMachineSettingsUSB::setValidator (QIWidgetValidator *aVal)
{
    mValidator = aVal;
    connect (mGbUSB, SIGNAL (stateChanged (int)), mValidator, SLOT (revalidate()));
}

void UIMachineSettingsUSB::setOrderAfter (QWidget *aWidget)
{
    setTabOrder (aWidget, mGbUSB);
    setTabOrder (mGbUSB, mCbUSB2);
    setTabOrder (mCbUSB2, mTwFilters);
}

void UIMachineSettingsUSB::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::UIMachineSettingsUSB::retranslateUi (this);

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

void UIMachineSettingsUSB::usbAdapterToggled (bool aOn)
{
    mGbUSBFilters->setEnabled (aOn);
}

void UIMachineSettingsUSB::currentChanged (QTreeWidgetItem *aItem,
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

void UIMachineSettingsUSB::newClicked()
{
    /* Search for the max available filter index */
    int maxFilterIndex = 0;
    QRegExp regExp (QString ("^") + mUSBFilterName.arg ("([0-9]+)") + QString ("$"));
    QTreeWidgetItemIterator iterator (mTwFilters);
    while (*iterator)
    {
        QString filterName = (*iterator)->text (0);
        int pos = regExp.indexIn (filterName);
        if (pos != -1)
            maxFilterIndex = regExp.cap (1).toInt() > maxFilterIndex ?
                             regExp.cap (1).toInt() : maxFilterIndex;
        ++ iterator;
    }

    /* Add new corresponding list item to the cache: */
    UIUSBFilterData data;
    switch (type())
    {
        case UISettingsPageType_Global:
            data.m_action = KUSBDeviceFilterAction_Hold;
            break;
        default:
            break;
    }
    data.m_fActive = true;
    data.m_strName = mUSBFilterName.arg(maxFilterIndex + 1);
    data.m_fHostUSBDevice = false;
    m_cache.m_items << data;

    /* Add new corresponding tree-widget-item to the tree-widget: */
    addUSBFilter(data, true /* its new? */);
    /* Mark filter's list as edited: */
    markSettingsChanged();
    /* Revalidate if possible: */
    if (mValidator)
        mValidator->revalidate();
}

void UIMachineSettingsUSB::addClicked()
{
    mUSBDevicesMenu->exec (QCursor::pos());
}

void UIMachineSettingsUSB::addConfirmed (QAction *aAction)
{
    /* Get USB device: */
    CUSBDevice usb = mUSBDevicesMenu->getUSB (aAction);
    /* if null then some other item but a USB device is selected */
    if (usb.isNull())
        return;

    /* Add new corresponding list item to the cache: */
    UIUSBFilterData data;
    switch (type())
    {
        case UISettingsPageType_Global:
            data.m_action = KUSBDeviceFilterAction_Hold;
            /* Check that under host (global) USB settings if they will be enabled! */
            data.m_fHostUSBDevice = false;
            break;
        case UISettingsPageType_Machine:
            data.m_fHostUSBDevice = false;
            break;
        default:
            break;
    }
    data.m_fActive = true;
    data.m_strName = vboxGlobal().details(usb);
    data.m_strVendorId = QString().sprintf("%04hX", usb.GetVendorId());
    data.m_strProductId = QString().sprintf("%04hX", usb.GetProductId());
    data.m_strRevision = QString().sprintf("%04hX", usb.GetRevision());
    /* The port property depends on the host computer rather than on the USB
     * device itself; for this reason only a few people will want to use it
     * in the filter since the same device plugged into a different socket
     * will not match the filter in this case. */
#if 0
    data.m_strPort = QString().sprintf("%04hX", usb.GetPort());
#endif
    data.m_strManufacturer = usb.GetManufacturer();
    data.m_strProduct = usb.GetProduct();
    data.m_strSerialNumber = usb.GetSerialNumber();
    data.m_strRemote = QString::number(usb.GetRemote());
    m_cache.m_items << data;

    /* Add new corresponding tree-widget-item to the tree-widget: */
    addUSBFilter(data, true /* its new? */);
    /* Mark filter's list as edited: */
    markSettingsChanged();
    /* Revalidate if possible: */
    if (mValidator)
        mValidator->revalidate();
}

void UIMachineSettingsUSB::edtClicked()
{
    /* Get current USB filter item: */
    QTreeWidgetItem *pItem = mTwFilters->currentItem();
    Assert(pItem);
    UIUSBFilterData &data = m_cache.m_items[mTwFilters->indexOfTopLevelItem(pItem)];

    /* Configure USB filter details dialog: */
    UIMachineSettingsUSBFilterDetails dlgFilterDetails(type(), this);
    dlgFilterDetails.mLeName->setText(data.m_strName);
    dlgFilterDetails.mLeVendorID->setText(data.m_strVendorId);
    dlgFilterDetails.mLeProductID->setText(data.m_strProductId);
    dlgFilterDetails.mLeRevision->setText(data.m_strRevision);
    dlgFilterDetails.mLePort->setText(data.m_strPort);
    dlgFilterDetails.mLeManufacturer->setText(data.m_strManufacturer);
    dlgFilterDetails.mLeProduct->setText(data.m_strProduct);
    dlgFilterDetails.mLeSerialNo->setText(data.m_strSerialNumber);
    switch (type())
    {
        case UISettingsPageType_Global:
        {
            if (data.m_action == KUSBDeviceFilterAction_Ignore)
                dlgFilterDetails.mCbAction->setCurrentIndex(0);
            else if (data.m_action == KUSBDeviceFilterAction_Hold)
                dlgFilterDetails.mCbAction->setCurrentIndex(1);
            else
                AssertMsgFailed(("Invalid USBDeviceFilterAction type"));
            break;
        }
        case UISettingsPageType_Machine:
        {
            QString strRemote = data.m_strRemote.toLower();
            if (strRemote == "yes" || strRemote == "true" || strRemote == "1")
                dlgFilterDetails.mCbRemote->setCurrentIndex(ModeOn);
            else if (strRemote == "no" || strRemote == "false" || strRemote == "0")
                dlgFilterDetails.mCbRemote->setCurrentIndex(ModeOff);
            else
                dlgFilterDetails.mCbRemote->setCurrentIndex(ModeAny);
            break;
        }
        default:
            break;
    }

    /* Run USB filter details dialog: */
    if (dlgFilterDetails.exec() == QDialog::Accepted)
    {
        data.m_strName = dlgFilterDetails.mLeName->text().isEmpty() ? QString::null : dlgFilterDetails.mLeName->text();
        data.m_strVendorId = dlgFilterDetails.mLeVendorID->text().isEmpty() ? QString::null : dlgFilterDetails.mLeVendorID->text();
        data.m_strProductId = dlgFilterDetails.mLeProductID->text().isEmpty() ? QString::null : dlgFilterDetails.mLeProductID->text();
        data.m_strRevision = dlgFilterDetails.mLeRevision->text().isEmpty() ? QString::null : dlgFilterDetails.mLeRevision->text();
        data.m_strManufacturer = dlgFilterDetails.mLeManufacturer->text().isEmpty() ? QString::null : dlgFilterDetails.mLeManufacturer->text();
        data.m_strProduct = dlgFilterDetails.mLeProduct->text().isEmpty() ? QString::null : dlgFilterDetails.mLeProduct->text();
        data.m_strSerialNumber = dlgFilterDetails.mLeSerialNo->text().isEmpty() ? QString::null : dlgFilterDetails.mLeSerialNo->text();
        data.m_strPort = dlgFilterDetails.mLePort->text().isEmpty() ? QString::null : dlgFilterDetails.mLePort->text();
        switch (type())
        {
            case UISettingsPageType_Global:
            {
                data.m_action = vboxGlobal().toUSBDevFilterAction(dlgFilterDetails.mCbAction->currentText());
                break;
            }
            case UISettingsPageType_Machine:
            {
                switch (dlgFilterDetails.mCbRemote->currentIndex())
                {
                    case ModeAny: data.m_strRemote = QString(); break;
                    case ModeOn:  data.m_strRemote = QString::number(1); break;
                    case ModeOff: data.m_strRemote = QString::number(0); break;
                    default: AssertMsgFailed(("Invalid combo box index"));
                }
                break;
            }
            default:
                break;
        }
        pItem->setText(0, data.m_strName);
        pItem->setToolTip(0, toolTipFor(data));
        /* Mark filter's list as edited: */
        markSettingsChanged();
    }
}

void UIMachineSettingsUSB::delClicked()
{
    /* Get current USB filter item: */
    QTreeWidgetItem *pItem = mTwFilters->currentItem();
    Assert(pItem);

    /* Delete corresponding items: */
    m_cache.m_items.removeAt(mTwFilters->indexOfTopLevelItem(pItem));
    delete pItem;

    /* Update current item: */
    currentChanged(mTwFilters->currentItem());
    /* Mark filter's list as edited: */
    markSettingsChanged();
    /* Revalidate if possible: */
    if (!mTwFilters->topLevelItemCount())
    {
        if (mValidator)
        {
            mValidator->rescan();
            mValidator->revalidate();
        }
    }
}

void UIMachineSettingsUSB::mupClicked()
{
    QTreeWidgetItem *item = mTwFilters->currentItem();
    Assert (item);

    int index = mTwFilters->indexOfTopLevelItem (item);
    QTreeWidgetItem *takenItem = mTwFilters->takeTopLevelItem (index);
    Assert (item == takenItem);
    mTwFilters->insertTopLevelItem (index - 1, takenItem);
    m_cache.m_items.swap (index, index - 1);

    mTwFilters->setCurrentItem (takenItem);
    markSettingsChanged();
}

void UIMachineSettingsUSB::mdnClicked()
{
    QTreeWidgetItem *item = mTwFilters->currentItem();
    Assert (item);

    int index = mTwFilters->indexOfTopLevelItem (item);
    QTreeWidgetItem *takenItem = mTwFilters->takeTopLevelItem (index);
    Assert (item == takenItem);
    mTwFilters->insertTopLevelItem (index + 1, takenItem);
    m_cache.m_items.swap (index, index + 1);

    mTwFilters->setCurrentItem (takenItem);
    markSettingsChanged();
}

void UIMachineSettingsUSB::showContextMenu (const QPoint &aPos)
{
    mMenu->exec (mTwFilters->mapToGlobal (aPos));
}

void UIMachineSettingsUSB::sltUpdateActivityState(QTreeWidgetItem *pChangedItem)
{
    /* Check changed USB filter item: */
    Assert(pChangedItem);

    /* Delete corresponding items: */
    UIUSBFilterData &data = m_cache.m_items[mTwFilters->indexOfTopLevelItem(pChangedItem)];
    data.m_fActive = pChangedItem->checkState(0) == Qt::Checked;

    markSettingsChanged();
}

void UIMachineSettingsUSB::markSettingsChanged()
{
    mUSBFilterListModified = true;
}

void UIMachineSettingsUSB::addUSBFilter(const UIUSBFilterData &data, bool fIsNew)
{
    /* Append tree-widget with item: */
    QTreeWidgetItem *pItem = new QTreeWidgetItem;
    pItem->setCheckState(0, data.m_fActive ? Qt::Checked : Qt::Unchecked);
    pItem->setText(0, data.m_strName);
    pItem->setToolTip(0, toolTipFor(data));
    mTwFilters->addTopLevelItem(pItem);

    /* Select this item if its new: */
    if (fIsNew)
        mTwFilters->setCurrentItem(pItem);
}

/* Fetch data to m_properties & m_settings or m_machine: */
void UIMachineSettingsUSB::fetchData(const QVariant &data)
{
    switch (type())
    {
        case UISettingsPageType_Global:
        {
            m_properties = data.value<UISettingsDataGlobal>().m_properties;
            m_settings = data.value<UISettingsDataGlobal>().m_settings;
            break;
        }
        case UISettingsPageType_Machine:
        {
            m_machine = data.value<UISettingsDataMachine>().m_machine;
            break;
        }
        default:
            break;
    }
}

/* Upload m_properties & m_settings or m_machine to data: */
void UIMachineSettingsUSB::uploadData(QVariant &data) const
{
    switch (type())
    {
        case UISettingsPageType_Global:
        {
            data = QVariant::fromValue(UISettingsDataGlobal(m_properties, m_settings));
            break;
        }
        case UISettingsPageType_Machine:
        {
            data = QVariant::fromValue(UISettingsDataMachine(m_machine));
            break;
        }
        default:
            break;
    }
}

/* static */ QString UIMachineSettingsUSB::toolTipFor(const UIUSBFilterData &data)
{
    /* Prepare tool-tip: */
    QString strToolTip;

    QString strVendorId = data.m_strVendorId;
    if (!strVendorId.isEmpty())
        strToolTip += tr("<nobr>Vendor ID: %1</nobr>", "USB filter tooltip").arg(strVendorId);

    QString strProductId = data.m_strProductId;
    if (!strProductId.isEmpty())
        strToolTip += strToolTip.isEmpty() ? "":"<br/>" + tr("<nobr>Product ID: %2</nobr>", "USB filter tooltip").arg(strProductId);

    QString strRevision = data.m_strRevision;
    if (!strRevision.isEmpty())
        strToolTip += strToolTip.isEmpty() ? "":"<br/>" + tr("<nobr>Revision: %3</nobr>", "USB filter tooltip").arg(strRevision);

    QString strProduct = data.m_strProduct;
    if (!strProduct.isEmpty())
        strToolTip += strToolTip.isEmpty() ? "":"<br/>" + tr("<nobr>Product: %4</nobr>", "USB filter tooltip").arg(strProduct);

    QString strManufacturer = data.m_strManufacturer;
    if (!strManufacturer.isEmpty())
        strToolTip += strToolTip.isEmpty() ? "":"<br/>" + tr("<nobr>Manufacturer: %5</nobr>", "USB filter tooltip").arg(strManufacturer);

    QString strSerial = data.m_strSerialNumber;
    if (!strSerial.isEmpty())
        strToolTip += strToolTip.isEmpty() ? "":"<br/>" + tr("<nobr>Serial No.: %1</nobr>", "USB filter tooltip").arg(strSerial);

    QString strPort = data.m_strPort;
    if (!strPort.isEmpty())
        strToolTip += strToolTip.isEmpty() ? "":"<br/>" + tr("<nobr>Port: %1</nobr>", "USB filter tooltip").arg(strPort);

    /* Add the state field if it's a host USB device: */
    if (data.m_fHostUSBDevice)
    {
        strToolTip += strToolTip.isEmpty() ? "":"<br/>" + tr("<nobr>State: %1</nobr>", "USB filter tooltip")
                                                          .arg(vboxGlobal().toString(data.m_hostUSBDeviceState));
    }

    return strToolTip;
}

