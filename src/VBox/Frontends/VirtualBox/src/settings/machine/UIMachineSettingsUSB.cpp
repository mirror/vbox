/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIMachineSettingsUSB class implementation
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Qt includes: */
#include <QHeaderView>
#include <QHelpEvent>
#include <QToolTip>

/* GUI includes: */
#include "QIWidgetValidator.h"
#include "UIIconPool.h"
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"
#include "UIToolBar.h"
#include "UIMachineSettingsUSB.h"
#include "UIMachineSettingsUSBFilterDetails.h"
#include "COMEnumsWrapper.h"

/* COM includes: */
#include "CConsole.h"
#include "CUSBController.h"
#include "CUSBDevice.h"
#include "CUSBDeviceFilter.h"
#include "CHostUSBDevice.h"
#include "CHostUSBDeviceFilter.h"
#include "CExtPackManager.h"
#include "CExtPack.h"

/**
 *  USB popup menu class.
 *  This class provides the list of USB devices attached to the host.
 */
class VBoxUSBMenu : public QMenu
{
    Q_OBJECT;

public:

    /* Constructor: */
    VBoxUSBMenu(QWidget *)
    {
        connect(this, SIGNAL(aboutToShow()), this, SLOT(processAboutToShow()));
    }

    /* Returns USB device related to passed action: */
    const CUSBDevice& getUSB(QAction *pAction)
    {
        return m_usbDeviceMap[pAction];
    }

    /* Console setter: */
    void setConsole(const CConsole &console)
    {
        m_console = console;
    }

private slots:

    /* Prepare menu appearance: */
    void processAboutToShow()
    {
        clear();
        m_usbDeviceMap.clear();

        CHost host = vboxGlobal().host();

        bool fIsUSBEmpty = host.GetUSBDevices().size() == 0;
        if (fIsUSBEmpty)
        {
            QAction *pAction = addAction(tr("<no devices available>", "USB devices"));
            pAction->setEnabled(false);
            pAction->setToolTip(tr("No supported devices connected to the host PC", "USB device tooltip"));
        }
        else
        {
            CHostUSBDeviceVector devvec = host.GetUSBDevices();
            for (int i = 0; i < devvec.size(); ++i)
            {
                CHostUSBDevice dev = devvec[i];
                CUSBDevice usb(dev);
                QAction *pAction = addAction(vboxGlobal().details(usb));
                pAction->setCheckable(true);
                m_usbDeviceMap[pAction] = usb;
                /* Check if created item was already attached to this session: */
                if (!m_console.isNull())
                {
                    CUSBDevice attachedUSB = m_console.FindUSBDeviceById(usb.GetId());
                    pAction->setChecked(!attachedUSB.isNull());
                    pAction->setEnabled(dev.GetState() != KUSBDeviceState_Unavailable);
                }
            }
        }
    }

private:

    /* Event handler: */
    bool event(QEvent *pEvent)
    {
        /* We provide dynamic tooltips for the usb devices: */
        if (pEvent->type() == QEvent::ToolTip)
        {
            QHelpEvent *pHelpEvent = static_cast<QHelpEvent*>(pEvent);
            QAction *pAction = actionAt(pHelpEvent->pos());
            if (pAction)
            {
                CUSBDevice usb = m_usbDeviceMap[pAction];
                if (!usb.isNull())
                {
                    QToolTip::showText(pHelpEvent->globalPos(), vboxGlobal().toolTip(usb));
                    return true;
                }
            }
        }
        /* Call to base-class: */
        return QMenu::event(pEvent);
    }

    QMap<QAction*, CUSBDevice> m_usbDeviceMap;
    CConsole m_console;
};

UIMachineSettingsUSB::UIMachineSettingsUSB(UISettingsPageType type)
    : UISettingsPage(type)
    , mValidator(0)
    , m_pToolBar(0)
    , mNewAction(0), mAddAction(0), mEdtAction(0), mDelAction(0)
    , mMupAction(0), mMdnAction(0)
    , mUSBDevicesMenu(0)
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

    /* Prepare toolbar */
    m_pToolBar = new UIToolBar (mWtFilterHandler);
    m_pToolBar->setUsesTextLabel (false);
    m_pToolBar->setIconSize (QSize (16, 16));
    m_pToolBar->setOrientation (Qt::Vertical);
    m_pToolBar->addAction (mNewAction);
    m_pToolBar->addAction (mAddAction);
    m_pToolBar->addAction (mEdtAction);
    m_pToolBar->addAction (mDelAction);
    m_pToolBar->addAction (mMupAction);
    m_pToolBar->addAction (mMdnAction);
    m_pToolBar->setSizePolicy (QSizePolicy::Fixed, QSizePolicy::MinimumExpanding);
    m_pToolBar->updateGeometry();
#ifdef Q_WS_MAC
    /* On the Mac this has to be slightly higher, than what sizeHint returned.
     * No idea why. */
    m_pToolBar->setMinimumHeight(m_pToolBar->sizeHint().height() + 4);
#else
    m_pToolBar->setMinimumHeight(m_pToolBar->sizeHint().height());
#endif /* Q_WS_MAC */
    mWtFilterHandler->layout()->addWidget (m_pToolBar);

    /* Setup connections */
    connect (mGbUSB, SIGNAL (toggled (bool)),
             this, SLOT (usbAdapterToggled (bool)));
    connect (mTwFilters, SIGNAL (currentItemChanged (QTreeWidgetItem*, QTreeWidgetItem*)),
             this, SLOT (currentChanged (QTreeWidgetItem*)));
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
#endif /* VBOX_WITH_EHCI */
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

    /* Clear cache initially: */
    m_cache.clear();

    /* Depending on page type: */
    switch (pageType())
    {
        case UISettingsPageType_Global:
        {
            /* For each USB filter: */
            const CHostUSBDeviceFilterVector &filters = vboxGlobal().host().GetUSBDeviceFilters();
            for (int iFilterIndex = 0; iFilterIndex < filters.size(); ++iFilterIndex)
            {
                /* Prepare USB filter data: */
                UIDataSettingsMachineUSBFilter usbFilterData;

                /* Check if filter is valid: */
                const CHostUSBDeviceFilter &filter = filters[iFilterIndex];
                if (!filter.isNull())
                {
                    usbFilterData.m_fActive = filter.GetActive();
                    usbFilterData.m_strName = filter.GetName();
                    usbFilterData.m_strVendorId = filter.GetVendorId();
                    usbFilterData.m_strProductId = filter.GetProductId();
                    usbFilterData.m_strRevision = filter.GetRevision();
                    usbFilterData.m_strManufacturer = filter.GetManufacturer();
                    usbFilterData.m_strProduct = filter.GetProduct();
                    usbFilterData.m_strSerialNumber = filter.GetSerialNumber();
                    usbFilterData.m_strPort = filter.GetPort();
                    usbFilterData.m_strRemote = filter.GetRemote();
                    usbFilterData.m_action = filter.GetAction();
                    CHostUSBDevice hostUSBDevice(filter);
                    if (!hostUSBDevice.isNull())
                    {
                        usbFilterData.m_fHostUSBDevice = true;
                        usbFilterData.m_hostUSBDeviceState = hostUSBDevice.GetState();
                    }
                    else
                    {
                        usbFilterData.m_fHostUSBDevice = false;
                        usbFilterData.m_hostUSBDeviceState = KUSBDeviceState_NotSupported;
                    }
                }

                /* Cache USB filter data: */
                m_cache.child(iFilterIndex).cacheInitialData(usbFilterData);
            }

            break;
        }
        case UISettingsPageType_Machine:
        {
            /* Prepare USB data: */
            UIDataSettingsMachineUSB usbData;

            /* Check if controller is valid: */
            const CUSBController &controller = m_machine.GetUSBController();
            if (!controller.isNull())
            {
                /* Gather USB values: */
                usbData.m_fUSBEnabled = controller.GetEnabled();
                usbData.m_fEHCIEnabled = controller.GetEnabledEhci();

                /* For each USB filter: */
                const CUSBDeviceFilterVector &filters = controller.GetDeviceFilters();
                for (int iFilterIndex = 0; iFilterIndex < filters.size(); ++iFilterIndex)
                {
                    /* Prepare USB filter data: */
                    UIDataSettingsMachineUSBFilter usbFilterData;

                    /* Check if filter is valid: */
                    const CUSBDeviceFilter &filter = filters[iFilterIndex];
                    if (!filter.isNull())
                    {
                        usbFilterData.m_fActive = filter.GetActive();
                        usbFilterData.m_strName = filter.GetName();
                        usbFilterData.m_strVendorId = filter.GetVendorId();
                        usbFilterData.m_strProductId = filter.GetProductId();
                        usbFilterData.m_strRevision = filter.GetRevision();
                        usbFilterData.m_strManufacturer = filter.GetManufacturer();
                        usbFilterData.m_strProduct = filter.GetProduct();
                        usbFilterData.m_strSerialNumber = filter.GetSerialNumber();
                        usbFilterData.m_strPort = filter.GetPort();
                        usbFilterData.m_strRemote = filter.GetRemote();
                    }

                    /* Cache USB filter data: */
                    m_cache.child(iFilterIndex).cacheInitialData(usbFilterData);
                }
            }

            /* Cache USB data: */
            m_cache.cacheInitialData(usbData);

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
    /* Clear list initially: */
    mTwFilters->clear();
    m_filters.clear();

    /* Depending on page type: */
    switch (pageType())
    {
        case UISettingsPageType_Global:
        {
            /* Hide unused widgets: */
            mGbUSB->setVisible(false);
            mCbUSB2->setVisible(false);
            break;
        }
        case UISettingsPageType_Machine:
        {
            /* Get USB data from cache: */
            const UIDataSettingsMachineUSB &usbData = m_cache.base();
            /* Load USB data to page: */
            mGbUSB->setChecked(usbData.m_fUSBEnabled);
            mCbUSB2->setChecked(usbData.m_fEHCIEnabled);
            break;
        }
        default:
            break;
    }

    /* For each USB filter => load it to the page: */
    for (int iFilterIndex = 0; iFilterIndex < m_cache.childCount(); ++iFilterIndex)
        addUSBFilter(m_cache.child(iFilterIndex).base(), false /* its new? */);

    /* Choose first filter as current: */
    mTwFilters->setCurrentItem(mTwFilters->topLevelItem(0));

    /* Update page: */
    usbAdapterToggled(mGbUSB->isChecked());

    /* Polish page finally: */
    polishPage();

    /* Revalidate if possible: */
    if (mValidator)
        mValidator->revalidate();
}

/* Save data from corresponding widgets to cache,
 * this task SHOULD be performed in GUI thread only: */
void UIMachineSettingsUSB::putToCache()
{
    /* Depending on page type: */
    switch (pageType())
    {
        case UISettingsPageType_Machine:
        {
            /* Prepare USB data: */
            UIDataSettingsMachineUSB usbData = m_cache.base();

            /* USB 1.0 (OHCI): */
            usbData.m_fUSBEnabled = mGbUSB->isChecked();
            /* USB 2.0 (EHCI): */
            CExtPack extPack = vboxGlobal().virtualBox().GetExtensionPackManager().Find(GUI_ExtPackName);
            usbData.m_fEHCIEnabled = extPack.isNull() || !extPack.GetUsable() ? false : mCbUSB2->isChecked();

            /* Update USB cache: */
            m_cache.cacheCurrentData(usbData);

            break;
        }
        default:
            break;
    }

    /* For each USB filter => recache USB filter data: */
    for (int iFilterIndex = 0; iFilterIndex < m_filters.size(); ++iFilterIndex)
        m_cache.child(iFilterIndex).cacheCurrentData(m_filters[iFilterIndex]);
}

/* Save data from cache to corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void UIMachineSettingsUSB::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to properties & settings or machine: */
    fetchData(data);

    /* Save settings depending on page type: */
    switch (pageType())
    {
        /* Here come the global USB properties: */
        case UISettingsPageType_Global:
        {
            /* Check if USB data really changed: */
            if (m_cache.wasChanged())
            {
                /* Store USB data: */
                if (isMachineInValidMode())
                {
                    /* Get host: */
                    CHost host = vboxGlobal().host();
                    /* For each USB filter data set: */
                    for (int iFilterIndex = 0; iFilterIndex < m_cache.childCount(); ++iFilterIndex)
                    {
                        /* Check if USB filter data really changed: */
                        const UICacheSettingsMachineUSBFilter &usbFilterCache = m_cache.child(iFilterIndex);
                        if (usbFilterCache.wasChanged())
                        {
                            /* If filter was removed or updated: */
                            if (usbFilterCache.wasRemoved() || usbFilterCache.wasUpdated())
                                host.RemoveUSBDeviceFilter(iFilterIndex);
                            /* If filter was created or updated: */
                            if (usbFilterCache.wasCreated() || usbFilterCache.wasUpdated())
                            {
                                /* Get USB filter data from cache: */
                                const UIDataSettingsMachineUSBFilter &usbFilterData = usbFilterCache.data();

                                /* Store USB filter data: */
                                CHostUSBDeviceFilter hostFilter = host.CreateUSBDeviceFilter(usbFilterData.m_strName);
                                hostFilter.SetActive(usbFilterData.m_fActive);
                                hostFilter.SetVendorId(usbFilterData.m_strVendorId);
                                hostFilter.SetProductId(usbFilterData.m_strProductId);
                                hostFilter.SetRevision(usbFilterData.m_strRevision);
                                hostFilter.SetManufacturer(usbFilterData.m_strManufacturer);
                                hostFilter.SetProduct(usbFilterData.m_strProduct);
                                hostFilter.SetSerialNumber(usbFilterData.m_strSerialNumber);
                                hostFilter.SetPort(usbFilterData.m_strPort);
                                hostFilter.SetRemote(usbFilterData.m_strRemote);
                                hostFilter.SetAction(usbFilterData.m_action);
                                host.InsertUSBDeviceFilter(iFilterIndex, hostFilter);
                            }
                        }
                    }
                }
            }
            break;
        }
        /* Here come VM USB properties: */
        case UISettingsPageType_Machine:
        {
            /* Check if USB data really changed: */
            if (m_cache.wasChanged())
            {
                /* Check if controller is valid: */
                CUSBController controller = m_machine.GetUSBController();
                if (!controller.isNull())
                {
                    /* Get USB data from cache: */
                    const UIDataSettingsMachineUSB &usbData = m_cache.data();
                    /* Store USB data: */
                    if (isMachineOffline())
                    {
                        controller.SetEnabled(usbData.m_fUSBEnabled);
                        controller.SetEnabledEhci(usbData.m_fEHCIEnabled);
                    }
                    /* Store USB filters data: */
                    if (isMachineInValidMode())
                    {
                        /* For each USB filter data set: */
                        int iOperationPosition = 0;
                        for (int iFilterIndex = 0; iFilterIndex < m_cache.childCount(); ++iFilterIndex)
                        {
                            /* Check if USB filter data really changed: */
                            const UICacheSettingsMachineUSBFilter &usbFilterCache = m_cache.child(iFilterIndex);
                            if (usbFilterCache.wasChanged())
                            {
                                /* If filter was removed or updated: */
                                if (usbFilterCache.wasRemoved() || usbFilterCache.wasUpdated())
                                {
                                    controller.RemoveDeviceFilter(iOperationPosition);
                                    if (usbFilterCache.wasRemoved())
                                        --iOperationPosition;
                                }

                                /* If filter was created or updated: */
                                if (usbFilterCache.wasCreated() || usbFilterCache.wasUpdated())
                                {
                                    /* Get USB filter data from cache: */
                                    const UIDataSettingsMachineUSBFilter &usbFilterData = usbFilterCache.data();
                                    /* Store USB filter data: */
                                    CUSBDeviceFilter filter = controller.CreateDeviceFilter(usbFilterData.m_strName);
                                    filter.SetActive(usbFilterData.m_fActive);
                                    filter.SetVendorId(usbFilterData.m_strVendorId);
                                    filter.SetProductId(usbFilterData.m_strProductId);
                                    filter.SetRevision(usbFilterData.m_strRevision);
                                    filter.SetManufacturer(usbFilterData.m_strManufacturer);
                                    filter.SetProduct(usbFilterData.m_strProduct);
                                    filter.SetSerialNumber(usbFilterData.m_strSerialNumber);
                                    filter.SetPort(usbFilterData.m_strPort);
                                    filter.SetRemote(usbFilterData.m_strRemote);
                                    controller.InsertDeviceFilter(iOperationPosition, filter);
                                }
                            }

                            /* Advance operation position: */
                            ++iOperationPosition;
                        }
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
    connect(mCbUSB2, SIGNAL(stateChanged(int)), mValidator, SLOT(revalidate()));
}

bool UIMachineSettingsUSB::revalidate(QString &strWarningText, QString& /* strTitle */)
{
    /* USB 2.0 Extension Pack presence test: */
    NOREF(strWarningText);
#ifdef VBOX_WITH_EXTPACK
    CExtPack extPack = vboxGlobal().virtualBox().GetExtensionPackManager().Find(GUI_ExtPackName);
    if (mGbUSB->isChecked() && mCbUSB2->isChecked() && (extPack.isNull() || !extPack.GetUsable()))
    {
        strWarningText = tr("USB 2.0 is currently enabled for this virtual machine. "
                            "However, this requires the <b>%1</b> to be installed. "
                            "Please install the Extension Pack from the VirtualBox download site. "
                            "After this you will be able to re-enable USB 2.0. "
                            "It will be disabled in the meantime unless you cancel the current settings changes.")
                            .arg(GUI_ExtPackName);
        msgCenter().remindAboutUnsupportedUSB2(GUI_ExtPackName, this);
        return true;
    }
#endif
    return true;
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

void UIMachineSettingsUSB::usbAdapterToggled(bool fEnabled)
{
    /* Enable/disable USB children: */
    mUSBChild->setEnabled(isMachineInValidMode() && fEnabled);
    mCbUSB2->setEnabled(isMachineOffline() && fEnabled);
    if (fEnabled)
    {
        /* If there is no chosen item but there is something to choose => choose it: */
        if (mTwFilters->currentItem() == 0 && mTwFilters->topLevelItemCount() != 0)
            mTwFilters->setCurrentItem(mTwFilters->topLevelItem(0));
    }
    /* Update current item: */
    currentChanged(mTwFilters->currentItem());
}

void UIMachineSettingsUSB::currentChanged(QTreeWidgetItem *aItem)
{
    /* Get selected items: */
    QList<QTreeWidgetItem*> selectedItems = mTwFilters->selectedItems();
    /* Deselect all selected items first: */
    for (int iItemIndex = 0; iItemIndex < selectedItems.size(); ++iItemIndex)
        selectedItems[iItemIndex]->setSelected(false);

    /* If tree-widget is NOT enabled => we should NOT select anything: */
    if (!mTwFilters->isEnabled())
        return;

    /* Select item if requested: */
    if (aItem)
        aItem->setSelected(true);

    /* Update corresponding action states: */
    mEdtAction->setEnabled(aItem);
    mDelAction->setEnabled(aItem);
    mMupAction->setEnabled(aItem && mTwFilters->itemAbove(aItem));
    mMdnAction->setEnabled(aItem && mTwFilters->itemBelow(aItem));
}

void UIMachineSettingsUSB::newClicked()
{
    /* Search for the max available filter index: */
    int iMaxFilterIndex = 0;
    QRegExp regExp(QString("^") + mUSBFilterName.arg("([0-9]+)") + QString("$"));
    QTreeWidgetItemIterator iterator(mTwFilters);
    while (*iterator)
    {
        QString filterName = (*iterator)->text(0);
        int pos = regExp.indexIn(filterName);
        if (pos != -1)
            iMaxFilterIndex = regExp.cap(1).toInt() > iMaxFilterIndex ?
                              regExp.cap(1).toInt() : iMaxFilterIndex;
        ++iterator;
    }

    /* Prepare new USB filter data: */
    UIDataSettingsMachineUSBFilter usbFilterData;
    switch (pageType())
    {
        case UISettingsPageType_Global:
            usbFilterData.m_action = KUSBDeviceFilterAction_Hold;
            break;
        default:
            break;
    }
    usbFilterData.m_fActive = true;
    usbFilterData.m_strName = mUSBFilterName.arg(iMaxFilterIndex + 1);
    usbFilterData.m_fHostUSBDevice = false;

    /* Add new USB filter data: */
    addUSBFilter(usbFilterData, true /* its new? */);

    /* Revalidate if possible: */
    if (mValidator)
        mValidator->revalidate();
}

void UIMachineSettingsUSB::addClicked()
{
    mUSBDevicesMenu->exec(QCursor::pos());
}

void UIMachineSettingsUSB::addConfirmed(QAction *pAction)
{
    /* Get USB device: */
    CUSBDevice usb = mUSBDevicesMenu->getUSB(pAction);
    if (usb.isNull())
        return;

    /* Prepare new USB filter data: */
    UIDataSettingsMachineUSBFilter usbFilterData;
    switch (pageType())
    {
        case UISettingsPageType_Global:
            usbFilterData.m_action = KUSBDeviceFilterAction_Hold;
            break;
        default:
            break;
    }
    usbFilterData.m_fActive = true;
    usbFilterData.m_strName = vboxGlobal().details(usb);
    usbFilterData.m_fHostUSBDevice = false;
    usbFilterData.m_strVendorId = QString().sprintf("%04hX", usb.GetVendorId());
    usbFilterData.m_strProductId = QString().sprintf("%04hX", usb.GetProductId());
    usbFilterData.m_strRevision = QString().sprintf("%04hX", usb.GetRevision());
    /* The port property depends on the host computer rather than on the USB
     * device itself; for this reason only a few people will want to use it
     * in the filter since the same device plugged into a different socket
     * will not match the filter in this case. */
#if 0
    usbFilterData.m_strPort = QString().sprintf("%04hX", usb.GetPort());
#endif
    usbFilterData.m_strManufacturer = usb.GetManufacturer();
    usbFilterData.m_strProduct = usb.GetProduct();
    usbFilterData.m_strSerialNumber = usb.GetSerialNumber();
    usbFilterData.m_strRemote = QString::number(usb.GetRemote());

    /* Add new USB filter data: */
    addUSBFilter(usbFilterData, true /* its new? */);

    /* Revalidate if possible: */
    if (mValidator)
        mValidator->revalidate();
}

void UIMachineSettingsUSB::edtClicked()
{
    /* Get current USB filter item: */
    QTreeWidgetItem *pItem = mTwFilters->currentItem();
    Assert(pItem);
    UIDataSettingsMachineUSBFilter &usbFilterData = m_filters[mTwFilters->indexOfTopLevelItem(pItem)];

    /* Configure USB filter details dialog: */
    UIMachineSettingsUSBFilterDetails dlgFilterDetails(pageType(), this);
    dlgFilterDetails.mLeName->setText(usbFilterData.m_strName);
    dlgFilterDetails.mLeVendorID->setText(usbFilterData.m_strVendorId);
    dlgFilterDetails.mLeProductID->setText(usbFilterData.m_strProductId);
    dlgFilterDetails.mLeRevision->setText(usbFilterData.m_strRevision);
    dlgFilterDetails.mLePort->setText(usbFilterData.m_strPort);
    dlgFilterDetails.mLeManufacturer->setText(usbFilterData.m_strManufacturer);
    dlgFilterDetails.mLeProduct->setText(usbFilterData.m_strProduct);
    dlgFilterDetails.mLeSerialNo->setText(usbFilterData.m_strSerialNumber);
    switch (pageType())
    {
        case UISettingsPageType_Global:
        {
            if (usbFilterData.m_action == KUSBDeviceFilterAction_Ignore)
                dlgFilterDetails.mCbAction->setCurrentIndex(0);
            else if (usbFilterData.m_action == KUSBDeviceFilterAction_Hold)
                dlgFilterDetails.mCbAction->setCurrentIndex(1);
            else
                AssertMsgFailed(("Invalid USBDeviceFilterAction type"));
            break;
        }
        case UISettingsPageType_Machine:
        {
            QString strRemote = usbFilterData.m_strRemote.toLower();
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
        usbFilterData.m_strName = dlgFilterDetails.mLeName->text().isEmpty() ? QString::null : dlgFilterDetails.mLeName->text();
        usbFilterData.m_strVendorId = dlgFilterDetails.mLeVendorID->text().isEmpty() ? QString::null : dlgFilterDetails.mLeVendorID->text();
        usbFilterData.m_strProductId = dlgFilterDetails.mLeProductID->text().isEmpty() ? QString::null : dlgFilterDetails.mLeProductID->text();
        usbFilterData.m_strRevision = dlgFilterDetails.mLeRevision->text().isEmpty() ? QString::null : dlgFilterDetails.mLeRevision->text();
        usbFilterData.m_strManufacturer = dlgFilterDetails.mLeManufacturer->text().isEmpty() ? QString::null : dlgFilterDetails.mLeManufacturer->text();
        usbFilterData.m_strProduct = dlgFilterDetails.mLeProduct->text().isEmpty() ? QString::null : dlgFilterDetails.mLeProduct->text();
        usbFilterData.m_strSerialNumber = dlgFilterDetails.mLeSerialNo->text().isEmpty() ? QString::null : dlgFilterDetails.mLeSerialNo->text();
        usbFilterData.m_strPort = dlgFilterDetails.mLePort->text().isEmpty() ? QString::null : dlgFilterDetails.mLePort->text();
        switch (pageType())
        {
            case UISettingsPageType_Global:
            {
                usbFilterData.m_action = gCOMenum->toUSBDevFilterAction(dlgFilterDetails.mCbAction->currentText());
                break;
            }
            case UISettingsPageType_Machine:
            {
                switch (dlgFilterDetails.mCbRemote->currentIndex())
                {
                    case ModeAny: usbFilterData.m_strRemote = QString(); break;
                    case ModeOn:  usbFilterData.m_strRemote = QString::number(1); break;
                    case ModeOff: usbFilterData.m_strRemote = QString::number(0); break;
                    default: AssertMsgFailed(("Invalid combo box index"));
                }
                break;
            }
            default:
                break;
        }
        pItem->setText(0, usbFilterData.m_strName);
        pItem->setToolTip(0, toolTipFor(usbFilterData));
    }
}

void UIMachineSettingsUSB::delClicked()
{
    /* Get current USB filter item: */
    QTreeWidgetItem *pItem = mTwFilters->currentItem();
    Assert(pItem);

    /* Delete corresponding items: */
    m_filters.removeAt(mTwFilters->indexOfTopLevelItem(pItem));
    delete pItem;

    /* Update current item: */
    currentChanged(mTwFilters->currentItem());
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
    m_filters.swap (index, index - 1);

    mTwFilters->setCurrentItem (takenItem);
}

void UIMachineSettingsUSB::mdnClicked()
{
    QTreeWidgetItem *item = mTwFilters->currentItem();
    Assert (item);

    int index = mTwFilters->indexOfTopLevelItem (item);
    QTreeWidgetItem *takenItem = mTwFilters->takeTopLevelItem (index);
    Assert (item == takenItem);
    mTwFilters->insertTopLevelItem (index + 1, takenItem);
    m_filters.swap (index, index + 1);

    mTwFilters->setCurrentItem (takenItem);
}

void UIMachineSettingsUSB::showContextMenu(const QPoint &pos)
{
    QMenu menu;
    if (mTwFilters->isEnabled())
    {
        menu.addAction(mNewAction);
        menu.addAction(mAddAction);
        menu.addSeparator();
        menu.addAction(mEdtAction);
        menu.addSeparator();
        menu.addAction(mDelAction);
        menu.addSeparator();
        menu.addAction(mMupAction);
        menu.addAction(mMdnAction);
    }
    if (!menu.isEmpty())
        menu.exec(mTwFilters->mapToGlobal(pos));
}

void UIMachineSettingsUSB::sltUpdateActivityState(QTreeWidgetItem *pChangedItem)
{
    /* Check changed USB filter item: */
    Assert(pChangedItem);

    /* Delete corresponding items: */
    UIDataSettingsMachineUSBFilter &data = m_filters[mTwFilters->indexOfTopLevelItem(pChangedItem)];
    data.m_fActive = pChangedItem->checkState(0) == Qt::Checked;
}

void UIMachineSettingsUSB::addUSBFilter(const UIDataSettingsMachineUSBFilter &usbFilterData, bool fIsNew)
{
    /* Append internal list with data: */
    m_filters << usbFilterData;

    /* Append tree-widget with item: */
    QTreeWidgetItem *pItem = new QTreeWidgetItem;
    pItem->setCheckState(0, usbFilterData.m_fActive ? Qt::Checked : Qt::Unchecked);
    pItem->setText(0, usbFilterData.m_strName);
    pItem->setToolTip(0, toolTipFor(usbFilterData));
    mTwFilters->addTopLevelItem(pItem);

    /* Select this item if its new: */
    if (fIsNew)
        mTwFilters->setCurrentItem(pItem);
}

/* Fetch data to m_properties & m_settings or m_machine & m_console: */
void UIMachineSettingsUSB::fetchData(const QVariant &data)
{
    switch (pageType())
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
            m_console = data.value<UISettingsDataMachine>().m_console;
            break;
        }
        default:
            break;
    }
}

/* Upload m_properties & m_settings or m_machine & m_console to data: */
void UIMachineSettingsUSB::uploadData(QVariant &data) const
{
    switch (pageType())
    {
        case UISettingsPageType_Global:
        {
            data = QVariant::fromValue(UISettingsDataGlobal(m_properties, m_settings));
            break;
        }
        case UISettingsPageType_Machine:
        {
            data = QVariant::fromValue(UISettingsDataMachine(m_machine, m_console));
            break;
        }
        default:
            break;
    }
}

/* static */
QString UIMachineSettingsUSB::toolTipFor(const UIDataSettingsMachineUSBFilter &usbFilterData)
{
    /* Prepare tool-tip: */
    QString strToolTip;

    QString strVendorId = usbFilterData.m_strVendorId;
    if (!strVendorId.isEmpty())
        strToolTip += tr("<nobr>Vendor ID: %1</nobr>", "USB filter tooltip").arg(strVendorId);

    QString strProductId = usbFilterData.m_strProductId;
    if (!strProductId.isEmpty())
        strToolTip += strToolTip.isEmpty() ? "":"<br/>" + tr("<nobr>Product ID: %2</nobr>", "USB filter tooltip").arg(strProductId);

    QString strRevision = usbFilterData.m_strRevision;
    if (!strRevision.isEmpty())
        strToolTip += strToolTip.isEmpty() ? "":"<br/>" + tr("<nobr>Revision: %3</nobr>", "USB filter tooltip").arg(strRevision);

    QString strProduct = usbFilterData.m_strProduct;
    if (!strProduct.isEmpty())
        strToolTip += strToolTip.isEmpty() ? "":"<br/>" + tr("<nobr>Product: %4</nobr>", "USB filter tooltip").arg(strProduct);

    QString strManufacturer = usbFilterData.m_strManufacturer;
    if (!strManufacturer.isEmpty())
        strToolTip += strToolTip.isEmpty() ? "":"<br/>" + tr("<nobr>Manufacturer: %5</nobr>", "USB filter tooltip").arg(strManufacturer);

    QString strSerial = usbFilterData.m_strSerialNumber;
    if (!strSerial.isEmpty())
        strToolTip += strToolTip.isEmpty() ? "":"<br/>" + tr("<nobr>Serial No.: %1</nobr>", "USB filter tooltip").arg(strSerial);

    QString strPort = usbFilterData.m_strPort;
    if (!strPort.isEmpty())
        strToolTip += strToolTip.isEmpty() ? "":"<br/>" + tr("<nobr>Port: %1</nobr>", "USB filter tooltip").arg(strPort);

    /* Add the state field if it's a host USB device: */
    if (usbFilterData.m_fHostUSBDevice)
    {
        strToolTip += strToolTip.isEmpty() ? "":"<br/>" + tr("<nobr>State: %1</nobr>", "USB filter tooltip")
                                                          .arg(gCOMenum->toString(usbFilterData.m_hostUSBDeviceState));
    }

    return strToolTip;
}

void UIMachineSettingsUSB::polishPage()
{
    mGbUSB->setEnabled(isMachineOffline());
    mUSBChild->setEnabled(isMachineInValidMode() && mGbUSB->isChecked());
    mCbUSB2->setEnabled(isMachineOffline() && mGbUSB->isChecked());
}

#include "UIMachineSettingsUSB.moc"

