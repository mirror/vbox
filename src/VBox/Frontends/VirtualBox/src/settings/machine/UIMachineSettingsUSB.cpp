/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineSettingsUSB class implementation.
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QHeaderView>
# include <QHelpEvent>
# include <QToolTip>

/* GUI includes: */
# include "QIWidgetValidator.h"
# include "UIConverter.h"
# include "UIIconPool.h"
# include "UIMachineSettingsUSB.h"
# include "UIMachineSettingsUSBFilterDetails.h"
# include "UIMessageCenter.h"
# include "UIToolBar.h"
# include "VBoxGlobal.h"

/* COM includes: */
# include "CConsole.h"
# include "CExtPack.h"
# include "CExtPackManager.h"
# include "CHostUSBDevice.h"
# include "CHostUSBDeviceFilter.h"
# include "CUSBController.h"
# include "CUSBDevice.h"
# include "CUSBDeviceFilter.h"
# include "CUSBDeviceFilters.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* VirtualBox interface declarations: */
#ifndef VBOX_WITH_XPCOM
# include "VirtualBox.h"
#else /* !VBOX_WITH_XPCOM */
# include "VirtualBox_XPCOM.h"
#endif /* VBOX_WITH_XPCOM */


/** Machine settings: USB filter data structure. */
struct UIDataSettingsMachineUSBFilter
{
    /** Constructs data. */
    UIDataSettingsMachineUSBFilter()
        : m_fActive(false)
        , m_strName(QString())
        , m_strVendorId(QString())
        , m_strProductId(QString())
        , m_strRevision(QString())
        , m_strManufacturer(QString())
        , m_strProduct(QString())
        , m_strSerialNumber(QString())
        , m_strPort(QString())
        , m_strRemote(QString())
        , m_enmAction(KUSBDeviceFilterAction_Null)
        , m_enmHostUSBDeviceState(KUSBDeviceState_NotSupported)
        , m_fHostUSBDevice(false)
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool equal(const UIDataSettingsMachineUSBFilter &other) const
    {
        return true
               && (m_fActive == other.m_fActive)
               && (m_strName == other.m_strName)
               && (m_strVendorId == other.m_strVendorId)
               && (m_strProductId == other.m_strProductId)
               && (m_strRevision == other.m_strRevision)
               && (m_strManufacturer == other.m_strManufacturer)
               && (m_strProduct == other.m_strProduct)
               && (m_strSerialNumber == other.m_strSerialNumber)
               && (m_strPort == other.m_strPort)
               && (m_strRemote == other.m_strRemote)
               && (m_enmAction == other.m_enmAction)
               && (m_enmHostUSBDeviceState == other.m_enmHostUSBDeviceState)
               ;
    }

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataSettingsMachineUSBFilter &other) const { return equal(other); }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataSettingsMachineUSBFilter &other) const { return !equal(other); }

    /** Holds whether the USB filter is enabled. */
    bool     m_fActive;
    /** Holds the USB filter name. */
    QString  m_strName;
    /** Holds the USB filter vendor ID. */
    QString  m_strVendorId;
    /** Holds the USB filter product ID. */
    QString  m_strProductId;
    /** Holds the USB filter revision. */
    QString  m_strRevision;
    /** Holds the USB filter manufacturer. */
    QString  m_strManufacturer;
    /** Holds the USB filter product. */
    QString  m_strProduct;
    /** Holds the USB filter serial number. */
    QString  m_strSerialNumber;
    /** Holds the USB filter port. */
    QString  m_strPort;
    /** Holds the USB filter remote. */
    QString  m_strRemote;

    /** Holds the USB filter action. */
    KUSBDeviceFilterAction  m_enmAction;
    /** Holds the USB device state. */
    KUSBDeviceState         m_enmHostUSBDeviceState;
    /** Holds whether the USB filter is host USB device. */
    bool                    m_fHostUSBDevice;
};


/** Machine settings: USB page data structure. */
struct UIDataSettingsMachineUSB
{
    /** Constructs data. */
    UIDataSettingsMachineUSB()
        : m_fUSBEnabled(false)
        , m_USBControllerType(KUSBControllerType_Null)
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool equal(const UIDataSettingsMachineUSB &other) const
    {
        return true
               && (m_fUSBEnabled == other.m_fUSBEnabled)
               && (m_USBControllerType == other.m_USBControllerType)
               ;
    }

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataSettingsMachineUSB &other) const { return equal(other); }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataSettingsMachineUSB &other) const { return !equal(other); }

    /** Holds whether the USB is enabled. */
    bool m_fUSBEnabled;
    /** Holds the USB controller type. */
    KUSBControllerType m_USBControllerType;
};


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


/** QITreeWidgetItem extension representing USB filter item. */
class UIUSBFilterItem : public QITreeWidgetItem, public UIDataSettingsMachineUSBFilter
{
public:

    /** Constructs USB filter item. */
    UIUSBFilterItem() {}

    /** Loads USB filter @a data. */
    void loadUSBFilterData(const UIDataSettingsMachineUSBFilter &data)
    {
        m_fActive = data.m_fActive;
        m_strName = data.m_strName;
        m_strVendorId = data.m_strVendorId;
        m_strProductId = data.m_strProductId;
        m_strRevision = data.m_strRevision;
        m_strManufacturer = data.m_strManufacturer;
        m_strProduct = data.m_strProduct;
        m_strSerialNumber = data.m_strSerialNumber;
        m_strPort = data.m_strPort;
        m_strRemote = data.m_strRemote;
        m_enmAction = data.m_enmAction;
        m_fHostUSBDevice = data.m_fHostUSBDevice;
        m_enmHostUSBDeviceState = data.m_enmHostUSBDeviceState;
    }

protected:

    /** Returns default text. */
    virtual QString defaultText() const /* override */
    {
        return checkState(0) == Qt::Checked ?
               tr("%1, Active", "col.1 text, col.1 state").arg(text(0)) :
               tr("%1",         "col.1 text")             .arg(text(0));
    }
};


UIMachineSettingsUSB::UIMachineSettingsUSB()
    : m_pToolBar(0)
    , m_pActionNew(0), m_pActionAdd(0), m_pActionEdit(0), m_pActionRemove(0)
    , m_pActionMoveUp(0), m_pActionMoveDown(0)
    , m_pMenuUSBDevices(0)
    , m_pCache(0)
{
    /* Prepare: */
    prepare();
}

UIMachineSettingsUSB::~UIMachineSettingsUSB()
{
    /* Cleanup: */
    cleanup();
}

bool UIMachineSettingsUSB::isUSBEnabled() const
{
    return mGbUSB->isChecked();
}

bool UIMachineSettingsUSB::changed() const
{
    return m_pCache->wasChanged();
}

void UIMachineSettingsUSB::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Clear cache initially: */
    m_pCache->clear();

    /* Prepare initial USB data: */
    UIDataSettingsMachineUSB initialUsbData;

    /* Gather USB values: */
    initialUsbData.m_fUSBEnabled = !m_machine.GetUSBControllers().isEmpty();
    initialUsbData.m_USBControllerType = m_machine.GetUSBControllerCountByType(KUSBControllerType_XHCI) > 0 ? KUSBControllerType_XHCI :
                                         m_machine.GetUSBControllerCountByType(KUSBControllerType_EHCI) > 0 ? KUSBControllerType_EHCI :
                                         m_machine.GetUSBControllerCountByType(KUSBControllerType_OHCI) > 0 ? KUSBControllerType_OHCI :
                                         KUSBControllerType_Null;

    /* Check if controller is valid: */
    const CUSBDeviceFilters &filtersObject = m_machine.GetUSBDeviceFilters();
    if (!filtersObject.isNull())
    {
        /* For each USB filter: */
        const CUSBDeviceFilterVector &filters = filtersObject.GetDeviceFilters();
        for (int iFilterIndex = 0; iFilterIndex < filters.size(); ++iFilterIndex)
        {
            /* Prepare USB filter data: */
            UIDataSettingsMachineUSBFilter initialUsbFilterData;

            /* Check if filter is valid: */
            const CUSBDeviceFilter &filter = filters.at(iFilterIndex);
            if (!filter.isNull())
            {
                /* Gather USB filter values: */
                initialUsbFilterData.m_fActive = filter.GetActive();
                initialUsbFilterData.m_strName = filter.GetName();
                initialUsbFilterData.m_strVendorId = filter.GetVendorId();
                initialUsbFilterData.m_strProductId = filter.GetProductId();
                initialUsbFilterData.m_strRevision = filter.GetRevision();
                initialUsbFilterData.m_strManufacturer = filter.GetManufacturer();
                initialUsbFilterData.m_strProduct = filter.GetProduct();
                initialUsbFilterData.m_strSerialNumber = filter.GetSerialNumber();
                initialUsbFilterData.m_strPort = filter.GetPort();
                initialUsbFilterData.m_strRemote = filter.GetRemote();
            }

            /* Cache initial USB filter data: */
            m_pCache->child(iFilterIndex).cacheInitialData(initialUsbFilterData);
        }
    }

    /* Cache initial USB data: */
    m_pCache->cacheInitialData(initialUsbData);

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

void UIMachineSettingsUSB::getFromCache()
{
    /* Clear list initially: */
    mTwFilters->clear();

    /* Get USB data from cache: */
    const UIDataSettingsMachineUSB &usbData = m_pCache->base();

    /* Load USB data to page: */
    mGbUSB->setChecked(usbData.m_fUSBEnabled);
    switch (usbData.m_USBControllerType)
    {
        default:
        case KUSBControllerType_OHCI: mRbUSB1->setChecked(true); break;
        case KUSBControllerType_EHCI: mRbUSB2->setChecked(true); break;
        case KUSBControllerType_XHCI: mRbUSB3->setChecked(true); break;
    }

    /* For each USB filter => load it to the page: */
    for (int iFilterIndex = 0; iFilterIndex < m_pCache->childCount(); ++iFilterIndex)
        addUSBFilter(m_pCache->child(iFilterIndex).base(), false /* its new? */);

    /* Choose first filter as current: */
    mTwFilters->setCurrentItem(mTwFilters->topLevelItem(0));

    /* Update page: */
    sltHandleUsbAdapterToggle(mGbUSB->isChecked());

    /* Polish page finally: */
    polishPage();

    /* Revalidate: */
    revalidate();
}

void UIMachineSettingsUSB::putToCache()
{
    /* Prepare current USB data: */
    UIDataSettingsMachineUSB currentUsbData;

    /* Gather current USB data: */
    currentUsbData.m_fUSBEnabled = mGbUSB->isChecked();
    if (!currentUsbData.m_fUSBEnabled)
        currentUsbData.m_USBControllerType = KUSBControllerType_Null;
    else
    {
        if (mRbUSB1->isChecked())
            currentUsbData.m_USBControllerType = KUSBControllerType_OHCI;
        else if (mRbUSB2->isChecked())
            currentUsbData.m_USBControllerType = KUSBControllerType_EHCI;
        else if (mRbUSB3->isChecked())
            currentUsbData.m_USBControllerType = KUSBControllerType_XHCI;
    }

    /* For each USB filter => cache current USB filter data: */
    QTreeWidgetItem *pMainRootItem = mTwFilters->invisibleRootItem();
    for (int iFilterIndex = 0; iFilterIndex < pMainRootItem->childCount(); ++iFilterIndex)
        m_pCache->child(iFilterIndex).cacheCurrentData(*static_cast<UIUSBFilterItem*>(pMainRootItem->child(iFilterIndex)));

    /* Cache current USB data: */
    m_pCache->cacheCurrentData(currentUsbData);
}

void UIMachineSettingsUSB::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Check if USB data was changed: */
    if (m_pCache->wasChanged())
    {
        /* Check if controller is valid: */
        CUSBDeviceFilters filtersObject = m_machine.GetUSBDeviceFilters();
        if (!filtersObject.isNull())
        {
            /* Get USB data from cache: */
            const UIDataSettingsMachineUSB &usbData = m_pCache->data();
            /* Store USB data: */
            if (isMachineOffline())
            {
                const ULONG cOhciCtls = m_machine.GetUSBControllerCountByType(KUSBControllerType_OHCI);
                const ULONG cEhciCtls = m_machine.GetUSBControllerCountByType(KUSBControllerType_EHCI);
                const ULONG cXhciCtls = m_machine.GetUSBControllerCountByType(KUSBControllerType_XHCI);

                /* Removing USB controllers: */
                if (!usbData.m_fUSBEnabled)
                {
                    if (cXhciCtls || cEhciCtls || cOhciCtls)
                        foreach (const CUSBController &controller, m_machine.GetUSBControllers())
                            m_machine.RemoveUSBController(controller.GetName());
                }
                /* Creating/replacing USB controllers: */
                else
                {
                    switch (usbData.m_USBControllerType)
                    {
                        case KUSBControllerType_OHCI:
                        {
                            if (cXhciCtls || cEhciCtls)
                            {
                                foreach (const CUSBController &controller, m_machine.GetUSBControllers())
                                {
                                    const KUSBControllerType enmType = controller.GetType();
                                    if (enmType == KUSBControllerType_XHCI || enmType == KUSBControllerType_EHCI)
                                        m_machine.RemoveUSBController(controller.GetName());
                                }
                            }
                            if (!cOhciCtls)
                                m_machine.AddUSBController("OHCI", KUSBControllerType_OHCI);
                            break;
                        }
                        case KUSBControllerType_EHCI:
                        {
                            if (cXhciCtls)
                            {
                                foreach (const CUSBController &controller, m_machine.GetUSBControllers())
                                {
                                    const KUSBControllerType enmType = controller.GetType();
                                    if (enmType == KUSBControllerType_XHCI)
                                        m_machine.RemoveUSBController(controller.GetName());
                                }
                            }
                            if (!cOhciCtls)
                                m_machine.AddUSBController("OHCI", KUSBControllerType_OHCI);
                            if (!cEhciCtls)
                                m_machine.AddUSBController("EHCI", KUSBControllerType_EHCI);
                            break;
                        }
                        case KUSBControllerType_XHCI:
                        {
                            if (cEhciCtls || cOhciCtls)
                            {
                                foreach (const CUSBController &controller, m_machine.GetUSBControllers())
                                {
                                    const KUSBControllerType enmType = controller.GetType();
                                    if (enmType == KUSBControllerType_EHCI || enmType == KUSBControllerType_OHCI)
                                        m_machine.RemoveUSBController(controller.GetName());
                                }
                            }
                            if (!cXhciCtls)
                                m_machine.AddUSBController("xHCI", KUSBControllerType_XHCI);
                            break;
                        }
                        default:
                            break;
                    }
                }
            }
            /* Store USB filters data: */
            if (isMachineInValidMode())
            {
                /* For each USB filter data set: */
                int iOperationPosition = 0;
                for (int iFilterIndex = 0; iFilterIndex < m_pCache->childCount(); ++iFilterIndex)
                {
                    /* Check if USB filter data really changed: */
                    const UISettingsCacheMachineUSBFilter &usbFilterCache = m_pCache->child(iFilterIndex);
                    if (usbFilterCache.wasChanged())
                    {
                        /* If filter was removed or updated: */
                        if (usbFilterCache.wasRemoved() || usbFilterCache.wasUpdated())
                        {
                            filtersObject.RemoveDeviceFilter(iOperationPosition);
                            if (usbFilterCache.wasRemoved())
                                --iOperationPosition;
                        }

                        /* If filter was created or updated: */
                        if (usbFilterCache.wasCreated() || usbFilterCache.wasUpdated())
                        {
                            /* Get USB filter data from the cache: */
                            const UIDataSettingsMachineUSBFilter &usbFilterData = usbFilterCache.data();
                            /* Store USB filter data: */
                            CUSBDeviceFilter filter = filtersObject.CreateDeviceFilter(usbFilterData.m_strName);
                            filter.SetActive(usbFilterData.m_fActive);
                            filter.SetVendorId(usbFilterData.m_strVendorId);
                            filter.SetProductId(usbFilterData.m_strProductId);
                            filter.SetRevision(usbFilterData.m_strRevision);
                            filter.SetManufacturer(usbFilterData.m_strManufacturer);
                            filter.SetProduct(usbFilterData.m_strProduct);
                            filter.SetSerialNumber(usbFilterData.m_strSerialNumber);
                            filter.SetPort(usbFilterData.m_strPort);
                            filter.SetRemote(usbFilterData.m_strRemote);
                            filtersObject.InsertDeviceFilter(iOperationPosition, filter);
                        }
                    }

                    /* Advance operation position: */
                    ++iOperationPosition;
                }
            }
        }
    }

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

bool UIMachineSettingsUSB::validate(QList<UIValidationMessage> &messages)
{
    Q_UNUSED(messages);

    /* Pass by default: */
    bool fPass = true;

#ifdef VBOX_WITH_EXTPACK
    /* USB 2.0/3.0 Extension Pack presence test: */
    const CExtPack extPack = vboxGlobal().virtualBox().GetExtensionPackManager().Find(GUI_ExtPackName);
    if (   mGbUSB->isChecked()
        && (mRbUSB2->isChecked() || mRbUSB3->isChecked())
        && (extPack.isNull() || !extPack.GetUsable()))
    {
        /* Prepare message: */
        UIValidationMessage message;
        message.second << tr("USB 2.0/3.0 is currently enabled for this virtual machine. "
                             "However, this requires the <i>%1</i> to be installed. "
                             "Please install the Extension Pack from the VirtualBox download site "
                             "or disable USB 2.0/3.0 to be able to start the machine.")
                             .arg(GUI_ExtPackName);
        /* Serialize message: */
        if (!message.second.isEmpty())
            messages << message;
    }
#endif /* VBOX_WITH_EXTPACK */

    /* Return result: */
    return fPass;
}

void UIMachineSettingsUSB::setOrderAfter(QWidget *pWidget)
{
    setTabOrder(pWidget, mGbUSB);
    setTabOrder(mGbUSB, mRbUSB1);
    setTabOrder(mRbUSB1, mRbUSB2);
    setTabOrder(mRbUSB2, mRbUSB3);
    setTabOrder(mRbUSB3, mTwFilters);
}

void UIMachineSettingsUSB::retranslateUi()
{
    /* Translate uic generated strings: */
    Ui::UIMachineSettingsUSB::retranslateUi(this);

    m_pActionNew->setText(tr("Add Empty Filter"));
    m_pActionAdd->setText(tr("Add Filter From Device"));
    m_pActionEdit->setText(tr("Edit Filter"));
    m_pActionRemove->setText(tr("Remove Filter"));
    m_pActionMoveUp->setText(tr("Move Filter Up"));
    m_pActionMoveDown->setText(tr("Move Filter Down"));

    m_pActionNew->setWhatsThis(tr("Adds new USB filter with all fields initially set to empty strings. "
                                "Note that such a filter will match any attached USB device."));
    m_pActionAdd->setWhatsThis(tr("Adds new USB filter with all fields set to the values of the "
                                "selected USB device attached to the host PC."));
    m_pActionEdit->setWhatsThis(tr("Edits selected USB filter."));
    m_pActionRemove->setWhatsThis(tr("Removes selected USB filter."));
    m_pActionMoveUp->setWhatsThis(tr("Moves selected USB filter up."));
    m_pActionMoveDown->setWhatsThis(tr("Moves selected USB filter down."));

    m_pActionNew->setToolTip(m_pActionNew->whatsThis());
    m_pActionAdd->setToolTip(m_pActionAdd->whatsThis());
    m_pActionEdit->setToolTip(m_pActionEdit->whatsThis());
    m_pActionRemove->setToolTip(m_pActionRemove->whatsThis());
    m_pActionMoveUp->setToolTip(m_pActionMoveUp->whatsThis());
    m_pActionMoveDown->setToolTip(m_pActionMoveDown->whatsThis());

    m_strTrUSBFilterName = tr("New Filter %1", "usb");
}

void UIMachineSettingsUSB::polishPage()
{
    mGbUSB->setEnabled(isMachineOffline());
    mUSBChild->setEnabled(isMachineInValidMode() && mGbUSB->isChecked());
    mRbUSB1->setEnabled(isMachineOffline() && mGbUSB->isChecked());
    mRbUSB2->setEnabled(isMachineOffline() && mGbUSB->isChecked());
    mRbUSB3->setEnabled(isMachineOffline() && mGbUSB->isChecked());
}

void UIMachineSettingsUSB::sltHandleUsbAdapterToggle(bool fEnabled)
{
    /* Enable/disable USB children: */
    mUSBChild->setEnabled(isMachineInValidMode() && fEnabled);
    mRbUSB1->setEnabled(isMachineOffline() && fEnabled);
    mRbUSB2->setEnabled(isMachineOffline() && fEnabled);
    mRbUSB3->setEnabled(isMachineOffline() && fEnabled);
    if (fEnabled)
    {
        /* If there is no chosen item but there is something to choose => choose it: */
        if (mTwFilters->currentItem() == 0 && mTwFilters->topLevelItemCount() != 0)
            mTwFilters->setCurrentItem(mTwFilters->topLevelItem(0));
    }
    /* Update current item: */
    sltHandleCurrentItemChange(mTwFilters->currentItem());
}

void UIMachineSettingsUSB::sltHandleCurrentItemChange(QTreeWidgetItem *pCurrentItem)
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
    if (pCurrentItem)
        pCurrentItem->setSelected(true);

    /* Update corresponding action states: */
    m_pActionEdit->setEnabled(pCurrentItem);
    m_pActionRemove->setEnabled(pCurrentItem);
    m_pActionMoveUp->setEnabled(pCurrentItem && mTwFilters->itemAbove(pCurrentItem));
    m_pActionMoveDown->setEnabled(pCurrentItem && mTwFilters->itemBelow(pCurrentItem));
}

void UIMachineSettingsUSB::sltHandleContextMenuRequest(const QPoint &pos)
{
    QMenu menu;
    if (mTwFilters->isEnabled())
    {
        menu.addAction(m_pActionNew);
        menu.addAction(m_pActionAdd);
        menu.addSeparator();
        menu.addAction(m_pActionEdit);
        menu.addSeparator();
        menu.addAction(m_pActionRemove);
        menu.addSeparator();
        menu.addAction(m_pActionMoveUp);
        menu.addAction(m_pActionMoveDown);
    }
    if (!menu.isEmpty())
        menu.exec(mTwFilters->mapToGlobal(pos));
}

void UIMachineSettingsUSB::sltHandleActivityStateChange(QTreeWidgetItem *pChangedItem)
{
    /* Check changed USB filter item: */
    UIUSBFilterItem *pItem = static_cast<UIUSBFilterItem*>(pChangedItem);
    AssertPtrReturnVoid(pItem);

    /* Update corresponding item: */
    pItem->m_fActive = pItem->checkState(0) == Qt::Checked;
}

void UIMachineSettingsUSB::sltNewFilter()
{
    /* Search for the max available filter index: */
    int iMaxFilterIndex = 0;
    const QRegExp regExp(QString("^") + m_strTrUSBFilterName.arg("([0-9]+)") + QString("$"));
    QTreeWidgetItemIterator iterator(mTwFilters);
    while (*iterator)
    {
        const QString filterName = (*iterator)->text(0);
        const int pos = regExp.indexIn(filterName);
        if (pos != -1)
            iMaxFilterIndex = regExp.cap(1).toInt() > iMaxFilterIndex ?
                              regExp.cap(1).toInt() : iMaxFilterIndex;
        ++iterator;
    }

    /* Prepare new USB filter data: */
    UIDataSettingsMachineUSBFilter usbFilterData;
    usbFilterData.m_fActive = true;
    usbFilterData.m_strName = m_strTrUSBFilterName.arg(iMaxFilterIndex + 1);
    usbFilterData.m_fHostUSBDevice = false;

    /* Add new USB filter data: */
    addUSBFilter(usbFilterData, true /* its new? */);

    /* Revalidate: */
    revalidate();
}

void UIMachineSettingsUSB::sltAddFilter()
{
    m_pMenuUSBDevices->exec(QCursor::pos());
}

void UIMachineSettingsUSB::sltAddFilterConfirmed(QAction *pAction)
{
    /* Get USB device: */
    const CUSBDevice usb = m_pMenuUSBDevices->getUSB(pAction);
    if (usb.isNull())
        return;

    /* Prepare new USB filter data: */
    UIDataSettingsMachineUSBFilter usbFilterData;
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

    /* Revalidate: */
    revalidate();
}

void UIMachineSettingsUSB::sltEditFilter()
{
    /* Check current USB filter item: */
    UIUSBFilterItem *pItem = static_cast<UIUSBFilterItem*>(mTwFilters->currentItem());
    AssertPtrReturnVoid(pItem);

    /* Configure USB filter details dialog: */
    UIMachineSettingsUSBFilterDetails dlgFilterDetails(this);
    dlgFilterDetails.mLeName->setText(pItem->m_strName);
    dlgFilterDetails.mLeVendorID->setText(pItem->m_strVendorId);
    dlgFilterDetails.mLeProductID->setText(pItem->m_strProductId);
    dlgFilterDetails.mLeRevision->setText(pItem->m_strRevision);
    dlgFilterDetails.mLePort->setText(pItem->m_strPort);
    dlgFilterDetails.mLeManufacturer->setText(pItem->m_strManufacturer);
    dlgFilterDetails.mLeProduct->setText(pItem->m_strProduct);
    dlgFilterDetails.mLeSerialNo->setText(pItem->m_strSerialNumber);
    const QString strRemote = pItem->m_strRemote.toLower();
    if (strRemote == "yes" || strRemote == "true" || strRemote == "1")
        dlgFilterDetails.mCbRemote->setCurrentIndex(ModeOn);
    else if (strRemote == "no" || strRemote == "false" || strRemote == "0")
        dlgFilterDetails.mCbRemote->setCurrentIndex(ModeOff);
    else
        dlgFilterDetails.mCbRemote->setCurrentIndex(ModeAny);

    /* Run USB filter details dialog: */
    if (dlgFilterDetails.exec() == QDialog::Accepted)
    {
        pItem->m_strName = dlgFilterDetails.mLeName->text().isEmpty() ? QString::null : dlgFilterDetails.mLeName->text();
        pItem->m_strVendorId = dlgFilterDetails.mLeVendorID->text().isEmpty() ? QString::null : dlgFilterDetails.mLeVendorID->text();
        pItem->m_strProductId = dlgFilterDetails.mLeProductID->text().isEmpty() ? QString::null : dlgFilterDetails.mLeProductID->text();
        pItem->m_strRevision = dlgFilterDetails.mLeRevision->text().isEmpty() ? QString::null : dlgFilterDetails.mLeRevision->text();
        pItem->m_strManufacturer = dlgFilterDetails.mLeManufacturer->text().isEmpty() ? QString::null : dlgFilterDetails.mLeManufacturer->text();
        pItem->m_strProduct = dlgFilterDetails.mLeProduct->text().isEmpty() ? QString::null : dlgFilterDetails.mLeProduct->text();
        pItem->m_strSerialNumber = dlgFilterDetails.mLeSerialNo->text().isEmpty() ? QString::null : dlgFilterDetails.mLeSerialNo->text();
        pItem->m_strPort = dlgFilterDetails.mLePort->text().isEmpty() ? QString::null : dlgFilterDetails.mLePort->text();
        switch (dlgFilterDetails.mCbRemote->currentIndex())
        {
            case ModeAny: pItem->m_strRemote = QString(); break;
            case ModeOn:  pItem->m_strRemote = QString::number(1); break;
            case ModeOff: pItem->m_strRemote = QString::number(0); break;
            default: AssertMsgFailed(("Invalid combo box index"));
        }
        pItem->setText(0, pItem->m_strName);
        pItem->setToolTip(0, toolTipFor(*pItem));
    }
}

void UIMachineSettingsUSB::sltRemoveFilter()
{
    /* Check current USB filter item: */
    QTreeWidgetItem *pItem = mTwFilters->currentItem();
    AssertPtrReturnVoid(pItem);

    /* Delete corresponding items: */
    delete pItem;

    /* Update current item: */
    sltHandleCurrentItemChange(mTwFilters->currentItem());

    /* Revalidate: */
    revalidate();
}

void UIMachineSettingsUSB::sltMoveFilterUp()
{
    /* Check current USB filter item: */
    QTreeWidgetItem *pItem = mTwFilters->currentItem();
    AssertPtrReturnVoid(pItem);

    /* Move the item up: */
    const int iIndex = mTwFilters->indexOfTopLevelItem(pItem);
    QTreeWidgetItem *pTakenItem = mTwFilters->takeTopLevelItem(iIndex);
    Assert(pItem == pTakenItem);
    mTwFilters->insertTopLevelItem(iIndex - 1, pTakenItem);

    /* Make sure moved item still chosen: */
    mTwFilters->setCurrentItem(pTakenItem);
}

void UIMachineSettingsUSB::sltMoveFilterDown()
{
    /* Check current USB filter item: */
    QTreeWidgetItem *pItem = mTwFilters->currentItem();
    AssertPtrReturnVoid(pItem);

    /* Move the item down: */
    const int iIndex = mTwFilters->indexOfTopLevelItem(pItem);
    QTreeWidgetItem *pTakenItem = mTwFilters->takeTopLevelItem(iIndex);
    Assert(pItem == pTakenItem);
    mTwFilters->insertTopLevelItem(iIndex + 1, pTakenItem);

    /* Make sure moved item still chosen: */
    mTwFilters->setCurrentItem(pTakenItem);
}

void UIMachineSettingsUSB::prepare()
{
    /* Apply UI decorations: */
    Ui::UIMachineSettingsUSB::setupUi(this);

    /* Prepare cache: */
    m_pCache = new UISettingsCacheMachineUSB;
    AssertPtrReturnVoid(m_pCache);

    /* Layout created in the .ui file. */
    {
        /* Prepare USB Filters tree: */
        prepareFiltersTree();
        /* Prepare USB Filters toolbar: */
        prepareFiltersToolbar();
        /* Prepare connections: */
        prepareConnections();
    }

    /* Apply language settings: */
    retranslateUi();
}

void UIMachineSettingsUSB::prepareFiltersTree()
{
    /* USB Filters tree-widget created in the .ui file. */
    AssertPtrReturnVoid(mTwFilters);
    {
        /* Configure tree-widget: */
        mTwFilters->header()->hide();
    }
}

void UIMachineSettingsUSB::prepareFiltersToolbar()
{
    /* USB Filters toolbar created in the .ui file. */
    AssertPtrReturnVoid(m_pFiltersToolBar);
    {
        /* Configure toolbar: */
        const int iIconMetric = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize);
        m_pFiltersToolBar->setIconSize(QSize(iIconMetric, iIconMetric));
        m_pFiltersToolBar->setOrientation(Qt::Vertical);

        /* Create USB devices menu: */
        m_pMenuUSBDevices = new VBoxUSBMenu(this);
        AssertPtrReturnVoid(m_pMenuUSBDevices);

        /* Create 'New USB Filter' action: */
        m_pActionNew = m_pFiltersToolBar->addAction(UIIconPool::iconSet(":/usb_new_16px.png",
                                                                        ":/usb_new_disabled_16px.png"),
                                                    QString(), this, SLOT(sltNewFilter()));
        AssertPtrReturnVoid(m_pActionNew);
        {
            /* Configure action: */
            m_pActionNew->setShortcuts(QList<QKeySequence>() << QKeySequence("Ins") << QKeySequence("Ctrl+N"));
        }

        /* Create 'Add USB Filter' action: */
        m_pActionAdd = m_pFiltersToolBar->addAction(UIIconPool::iconSet(":/usb_add_16px.png",
                                                                        ":/usb_add_disabled_16px.png"),
                                                    QString(), this, SLOT(sltAddFilter()));
        AssertPtrReturnVoid(m_pActionAdd);
        {
            /* Configure action: */
            m_pActionAdd->setShortcuts(QList<QKeySequence>() << QKeySequence("Alt+Ins") << QKeySequence("Ctrl+A"));
        }

        /* Create 'Edit USB Filter' action: */
        m_pActionEdit = m_pFiltersToolBar->addAction(UIIconPool::iconSet(":/usb_filter_edit_16px.png",
                                                                         ":/usb_filter_edit_disabled_16px.png"),
                                                     QString(), this, SLOT(sltEditFilter()));
        AssertPtrReturnVoid(m_pActionEdit);
        {
            /* Configure action: */
            m_pActionEdit->setShortcuts(QList<QKeySequence>() << QKeySequence("Alt+Return") << QKeySequence("Ctrl+Return"));
        }

        /* Create 'Remove USB Filter' action: */
        m_pActionRemove = m_pFiltersToolBar->addAction(UIIconPool::iconSet(":/usb_remove_16px.png",
                                                                           ":/usb_remove_disabled_16px.png"),
                                                       QString(), this, SLOT(sltRemoveFilter()));
        AssertPtrReturnVoid(m_pActionRemove);
        {
            /* Configure action: */
            m_pActionRemove->setShortcuts(QList<QKeySequence>() << QKeySequence("Del") << QKeySequence("Ctrl+R"));
        }

        /* Create 'Move USB Filter Up' action: */
        m_pActionMoveUp = m_pFiltersToolBar->addAction(UIIconPool::iconSet(":/usb_moveup_16px.png",
                                                                           ":/usb_moveup_disabled_16px.png"),
                                                       QString(), this, SLOT(sltMoveFilterUp()));
        AssertPtrReturnVoid(m_pActionMoveUp);
        {
            /* Configure action: */
            m_pActionMoveUp->setShortcuts(QList<QKeySequence>() << QKeySequence("Alt+Up") << QKeySequence("Ctrl+Up"));
        }

        /* Create 'Move USB Filter Down' action: */
        m_pActionMoveDown = m_pFiltersToolBar->addAction(UIIconPool::iconSet(":/usb_movedown_16px.png",
                                                                             ":/usb_movedown_disabled_16px.png"),
                                                         QString(), this, SLOT(sltMoveFilterDown()));
        AssertPtrReturnVoid(m_pActionMoveDown);
        {
            /* Configure action: */
            m_pActionMoveDown->setShortcuts(QList<QKeySequence>() << QKeySequence("Alt+Down") << QKeySequence("Ctrl+Down"));
        }
    }
}

void UIMachineSettingsUSB::prepareConnections()
{
    /* Configure validation connections: */
    connect(mGbUSB, SIGNAL(stateChanged(int)), this, SLOT(revalidate()));
    connect(mRbUSB1, SIGNAL(toggled(bool)), this, SLOT(revalidate()));
    connect(mRbUSB2, SIGNAL(toggled(bool)), this, SLOT(revalidate()));
    connect(mRbUSB3, SIGNAL(toggled(bool)), this, SLOT(revalidate()));

    /* Configure widget connections: */
    connect(mGbUSB, SIGNAL(toggled(bool)),
            this, SLOT(sltHandleUsbAdapterToggle(bool)));
    connect(mTwFilters, SIGNAL(currentItemChanged(QTreeWidgetItem*, QTreeWidgetItem*)),
            this, SLOT(sltHandleCurrentItemChange(QTreeWidgetItem*)));
    connect(mTwFilters, SIGNAL(customContextMenuRequested(const QPoint &)),
            this, SLOT(sltHandleContextMenuRequest(const QPoint &)));
    connect(mTwFilters, SIGNAL(itemDoubleClicked(QTreeWidgetItem *, int)),
            this, SLOT(sltEditFilter()));
    connect(mTwFilters, SIGNAL(itemChanged(QTreeWidgetItem *, int)),
            this, SLOT(sltHandleActivityStateChange(QTreeWidgetItem *)));

    /* Configure USB device menu connections: */
    connect(m_pMenuUSBDevices, SIGNAL(triggered(QAction*)),
            this, SLOT(sltAddFilterConfirmed(QAction *)));
}

void UIMachineSettingsUSB::cleanup()
{
    /* Cleanup USB devices menu: */
    delete m_pMenuUSBDevices;
    m_pMenuUSBDevices = 0;

    /* Cleanup cache: */
    delete m_pCache;
    m_pCache = 0;
}

void UIMachineSettingsUSB::addUSBFilter(const UIDataSettingsMachineUSBFilter &usbFilterData, bool fChoose)
{
    /* Create USB filter item: */
    UIUSBFilterItem *pItem = new UIUSBFilterItem;
    AssertPtrReturnVoid(pItem);
    {
        /* Configure item: */
        pItem->setCheckState(0, usbFilterData.m_fActive ? Qt::Checked : Qt::Unchecked);
        pItem->setText(0, usbFilterData.m_strName);
        pItem->setToolTip(0, toolTipFor(usbFilterData));
        pItem->loadUSBFilterData(usbFilterData);

        /* Append tree-widget with item: */
        mTwFilters->addTopLevelItem(pItem);

        /* Select this item if it's new: */
        if (fChoose)
            mTwFilters->setCurrentItem(pItem);
    }
}

/* static */
QString UIMachineSettingsUSB::toolTipFor(const UIDataSettingsMachineUSBFilter &usbFilterData)
{
    /* Prepare tool-tip: */
    QString strToolTip;

    const QString strVendorId = usbFilterData.m_strVendorId;
    if (!strVendorId.isEmpty())
        strToolTip += tr("<nobr>Vendor ID: %1</nobr>", "USB filter tooltip").arg(strVendorId);

    const QString strProductId = usbFilterData.m_strProductId;
    if (!strProductId.isEmpty())
        strToolTip += strToolTip.isEmpty() ? "":"<br/>" + tr("<nobr>Product ID: %2</nobr>", "USB filter tooltip").arg(strProductId);

    const QString strRevision = usbFilterData.m_strRevision;
    if (!strRevision.isEmpty())
        strToolTip += strToolTip.isEmpty() ? "":"<br/>" + tr("<nobr>Revision: %3</nobr>", "USB filter tooltip").arg(strRevision);

    const QString strProduct = usbFilterData.m_strProduct;
    if (!strProduct.isEmpty())
        strToolTip += strToolTip.isEmpty() ? "":"<br/>" + tr("<nobr>Product: %4</nobr>", "USB filter tooltip").arg(strProduct);

    const QString strManufacturer = usbFilterData.m_strManufacturer;
    if (!strManufacturer.isEmpty())
        strToolTip += strToolTip.isEmpty() ? "":"<br/>" + tr("<nobr>Manufacturer: %5</nobr>", "USB filter tooltip").arg(strManufacturer);

    const QString strSerial = usbFilterData.m_strSerialNumber;
    if (!strSerial.isEmpty())
        strToolTip += strToolTip.isEmpty() ? "":"<br/>" + tr("<nobr>Serial No.: %1</nobr>", "USB filter tooltip").arg(strSerial);

    const QString strPort = usbFilterData.m_strPort;
    if (!strPort.isEmpty())
        strToolTip += strToolTip.isEmpty() ? "":"<br/>" + tr("<nobr>Port: %1</nobr>", "USB filter tooltip").arg(strPort);

    /* Add the state field if it's a host USB device: */
    if (usbFilterData.m_fHostUSBDevice)
    {
        strToolTip += strToolTip.isEmpty() ? "":"<br/>" + tr("<nobr>State: %1</nobr>", "USB filter tooltip")
                                                          .arg(gpConverter->toString(usbFilterData.m_enmHostUSBDeviceState));
    }

    return strToolTip;
}

#include "UIMachineSettingsUSB.moc"

