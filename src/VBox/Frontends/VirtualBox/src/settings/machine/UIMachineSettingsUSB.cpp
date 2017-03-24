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
        , m_action(KUSBDeviceFilterAction_Null)
        , m_fHostUSBDevice(false)
        , m_hostUSBDeviceState(KUSBDeviceState_NotSupported)
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
               && (m_action == other.m_action)
               && (m_hostUSBDeviceState == other.m_hostUSBDeviceState)
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
    KUSBDeviceFilterAction  m_action;
    /** Holds whether the USB filter is host USB device. */
    bool                    m_fHostUSBDevice;
    /** Holds the USB device state. */
    KUSBDeviceState         m_hostUSBDeviceState;
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
class UIUSBFilterItem : public QITreeWidgetItem
{
public:

    /** Constructs USB filter item. */
    UIUSBFilterItem() {}

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
    , mNewAction(0), mAddAction(0), mEdtAction(0), mDelAction(0)
    , mMupAction(0), mMdnAction(0)
    , mUSBDevicesMenu(0)
    , m_pCache(new UISettingsCacheMachineUSB)
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

    /* Determine icon metric: */
    const QStyle *pStyle = QApplication::style();
    const int iIconMetric = pStyle->pixelMetric(QStyle::PM_SmallIconSize);

    /* Prepare tool-bar: */
    m_pFiltersToolBar->setIconSize(QSize(iIconMetric, iIconMetric));
    m_pFiltersToolBar->setOrientation(Qt::Vertical);
    m_pFiltersToolBar->addAction(mNewAction);
    m_pFiltersToolBar->addAction(mAddAction);
    m_pFiltersToolBar->addAction(mEdtAction);
    m_pFiltersToolBar->addAction(mDelAction);
    m_pFiltersToolBar->addAction(mMupAction);
    m_pFiltersToolBar->addAction(mMdnAction);

    /* Setup connections */
    connect(mGbUSB, SIGNAL(stateChanged(int)), this, SLOT(revalidate()));
    connect(mRbUSB1, SIGNAL(toggled(bool)), this, SLOT(revalidate()));
    connect(mRbUSB2, SIGNAL(toggled(bool)), this, SLOT(revalidate()));
    connect(mRbUSB3, SIGNAL(toggled(bool)), this, SLOT(revalidate()));

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

UIMachineSettingsUSB::~UIMachineSettingsUSB()
{
    delete mUSBDevicesMenu;

    /* Cleanup cache: */
    delete m_pCache;
    m_pCache = 0;
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
    /* Fetch data to properties & settings or machine: */
    fetchData(data);

    /* Clear cache initially: */
    m_pCache->clear();

    /* Prepare USB data: */
    UIDataSettingsMachineUSB usbData;

    /* Gather USB values: */
    usbData.m_fUSBEnabled = !m_machine.GetUSBControllers().isEmpty();
    usbData.m_USBControllerType = m_machine.GetUSBControllerCountByType(KUSBControllerType_XHCI) > 0 ? KUSBControllerType_XHCI :
                                  m_machine.GetUSBControllerCountByType(KUSBControllerType_EHCI) > 0 ? KUSBControllerType_EHCI :
                                  m_machine.GetUSBControllerCountByType(KUSBControllerType_OHCI) > 0 ? KUSBControllerType_OHCI :
                                  KUSBControllerType_Null;

    /* Check if controller is valid: */
    const CUSBDeviceFilters &filters = m_machine.GetUSBDeviceFilters();
    if (!filters.isNull())
    {
        /* For each USB filter: */
        const CUSBDeviceFilterVector &coll = filters.GetDeviceFilters();
        for (int iFilterIndex = 0; iFilterIndex < coll.size(); ++iFilterIndex)
        {
            /* Prepare USB filter data: */
            UIDataSettingsMachineUSBFilter usbFilterData;

            /* Check if filter is valid: */
            const CUSBDeviceFilter &filter = coll[iFilterIndex];
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
            m_pCache->child(iFilterIndex).cacheInitialData(usbFilterData);
        }
    }

    /* Cache USB data: */
    m_pCache->cacheInitialData(usbData);

    /* Upload properties & settings or machine to data: */
    uploadData(data);
}

void UIMachineSettingsUSB::getFromCache()
{
    /* Clear list initially: */
    mTwFilters->clear();
    m_filters.clear();

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
    usbAdapterToggled(mGbUSB->isChecked());

    /* Polish page finally: */
    polishPage();

    /* Revalidate: */
    revalidate();
}

void UIMachineSettingsUSB::putToCache()
{
    /* Prepare USB data: */
    UIDataSettingsMachineUSB usbData = m_pCache->base();

    /* Is USB controller enabled? */
    usbData.m_fUSBEnabled = mGbUSB->isChecked();
    /* Of which type? */
    if (!usbData.m_fUSBEnabled)
        usbData.m_USBControllerType = KUSBControllerType_Null;
    else
    {
        if (mRbUSB1->isChecked())
            usbData.m_USBControllerType = KUSBControllerType_OHCI;
        else if (mRbUSB2->isChecked())
            usbData.m_USBControllerType = KUSBControllerType_EHCI;
        else if (mRbUSB3->isChecked())
            usbData.m_USBControllerType = KUSBControllerType_XHCI;
    }

    /* Update USB cache: */
    m_pCache->cacheCurrentData(usbData);

    /* For each USB filter => recache USB filter data: */
    for (int iFilterIndex = 0; iFilterIndex < m_filters.size(); ++iFilterIndex)
        m_pCache->child(iFilterIndex).cacheCurrentData(m_filters[iFilterIndex]);
}

void UIMachineSettingsUSB::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to properties & settings or machine: */
    fetchData(data);

    /* Check if USB data really changed: */
    if (m_pCache->wasChanged())
    {
        /* Check if controller is valid: */
        CUSBDeviceFilters filters = m_machine.GetUSBDeviceFilters();
        if (!filters.isNull())
        {
            /* Get USB data from cache: */
            const UIDataSettingsMachineUSB &usbData = m_pCache->data();
            /* Store USB data: */
            if (isMachineOffline())
            {
                ULONG cOhciCtls = m_machine.GetUSBControllerCountByType(KUSBControllerType_OHCI);
                ULONG cEhciCtls = m_machine.GetUSBControllerCountByType(KUSBControllerType_EHCI);
                ULONG cXhciCtls = m_machine.GetUSBControllerCountByType(KUSBControllerType_XHCI);

                /* Removing USB controllers: */
                if (!usbData.m_fUSBEnabled)
                {
                    if (cXhciCtls || cEhciCtls || cOhciCtls)
                    {
                        CUSBControllerVector ctlvec = m_machine.GetUSBControllers();
                        for (int i = 0; i < ctlvec.size(); ++i)
                        {
                            CUSBController ctl = ctlvec[i];
                            QString strName = ctl.GetName();
                            m_machine.RemoveUSBController(strName);
                        }
                    }
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
                                CUSBControllerVector ctlvec = m_machine.GetUSBControllers();
                                for (int i = 0; i < ctlvec.size(); ++i)
                                {
                                    CUSBController ctl = ctlvec[i];
                                    KUSBControllerType enmType = ctl.GetType();
                                    if (enmType == KUSBControllerType_XHCI || enmType == KUSBControllerType_EHCI)
                                    {
                                        QString strName = ctl.GetName();
                                        m_machine.RemoveUSBController(strName);
                                    }
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
                                CUSBControllerVector ctlvec = m_machine.GetUSBControllers();
                                for (int i = 0; i < ctlvec.size(); ++i)
                                {
                                    CUSBController ctl = ctlvec[i];
                                    KUSBControllerType enmType = ctl.GetType();
                                    if (enmType == KUSBControllerType_XHCI)
                                    {
                                        QString strName = ctl.GetName();
                                        m_machine.RemoveUSBController(strName);
                                    }
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
                                CUSBControllerVector ctlvec = m_machine.GetUSBControllers();
                                for (int i = 0; i < ctlvec.size(); ++i)
                                {
                                    CUSBController ctl = ctlvec[i];
                                    KUSBControllerType enmType = ctl.GetType();
                                    if (enmType == KUSBControllerType_EHCI || enmType == KUSBControllerType_OHCI)
                                    {
                                        QString strName = ctl.GetName();
                                        m_machine.RemoveUSBController(strName);
                                    }
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
                            filters.RemoveDeviceFilter(iOperationPosition);
                            if (usbFilterCache.wasRemoved())
                                --iOperationPosition;
                        }

                        /* If filter was created or updated: */
                        if (usbFilterCache.wasCreated() || usbFilterCache.wasUpdated())
                        {
                            /* Get USB filter data from cache: */
                            const UIDataSettingsMachineUSBFilter &usbFilterData = usbFilterCache.data();
                            /* Store USB filter data: */
                            CUSBDeviceFilter filter = filters.CreateDeviceFilter(usbFilterData.m_strName);
                            filter.SetActive(usbFilterData.m_fActive);
                            filter.SetVendorId(usbFilterData.m_strVendorId);
                            filter.SetProductId(usbFilterData.m_strProductId);
                            filter.SetRevision(usbFilterData.m_strRevision);
                            filter.SetManufacturer(usbFilterData.m_strManufacturer);
                            filter.SetProduct(usbFilterData.m_strProduct);
                            filter.SetSerialNumber(usbFilterData.m_strSerialNumber);
                            filter.SetPort(usbFilterData.m_strPort);
                            filter.SetRemote(usbFilterData.m_strRemote);
                            filters.InsertDeviceFilter(iOperationPosition, filter);
                        }
                    }

                    /* Advance operation position: */
                    ++iOperationPosition;
                }
            }
        }
    }

    /* Upload properties & settings or machine to data: */
    uploadData(data);
}

bool UIMachineSettingsUSB::validate(QList<UIValidationMessage> &messages)
{
    Q_UNUSED(messages);

    /* Pass by default: */
    bool fPass = true;

#ifdef VBOX_WITH_EXTPACK
    /* USB 2.0/3.0 Extension Pack presence test: */
    CExtPack extPack = vboxGlobal().virtualBox().GetExtensionPackManager().Find(GUI_ExtPackName);
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

void UIMachineSettingsUSB::setOrderAfter (QWidget *aWidget)
{
    setTabOrder (aWidget, mGbUSB);
    setTabOrder (mGbUSB, mRbUSB1);
    setTabOrder (mRbUSB1, mRbUSB2);
    setTabOrder (mRbUSB2, mRbUSB3);
    setTabOrder (mRbUSB3, mTwFilters);
}

void UIMachineSettingsUSB::retranslateUi()
{
    /* Translate uic generated strings: */
    Ui::UIMachineSettingsUSB::retranslateUi(this);

    mNewAction->setText(tr("Add Empty Filter"));
    mAddAction->setText(tr("Add Filter From Device"));
    mEdtAction->setText(tr("Edit Filter"));
    mDelAction->setText(tr("Remove Filter"));
    mMupAction->setText(tr("Move Filter Up"));
    mMdnAction->setText(tr("Move Filter Down"));

    mNewAction->setWhatsThis(tr("Adds new USB filter with all fields initially set to empty strings. "
                                "Note that such a filter will match any attached USB device."));
    mAddAction->setWhatsThis(tr("Adds new USB filter with all fields set to the values of the "
                                "selected USB device attached to the host PC."));
    mEdtAction->setWhatsThis(tr("Edits selected USB filter."));
    mDelAction->setWhatsThis(tr("Removes selected USB filter."));
    mMupAction->setWhatsThis(tr("Moves selected USB filter up."));
    mMdnAction->setWhatsThis(tr("Moves selected USB filter down."));

    mNewAction->setToolTip(mNewAction->whatsThis());
    mAddAction->setToolTip(mAddAction->whatsThis());
    mEdtAction->setToolTip(mEdtAction->whatsThis());
    mDelAction->setToolTip(mDelAction->whatsThis());
    mMupAction->setToolTip(mMupAction->whatsThis());
    mMdnAction->setToolTip(mMdnAction->whatsThis());

    mUSBFilterName = tr("New Filter %1", "usb");
}

void UIMachineSettingsUSB::polishPage()
{
    mGbUSB->setEnabled(isMachineOffline());
    mUSBChild->setEnabled(isMachineInValidMode() && mGbUSB->isChecked());
    mRbUSB1->setEnabled(isMachineOffline() && mGbUSB->isChecked());
    mRbUSB2->setEnabled(isMachineOffline() && mGbUSB->isChecked());
    mRbUSB3->setEnabled(isMachineOffline() && mGbUSB->isChecked());
}

void UIMachineSettingsUSB::usbAdapterToggled(bool fEnabled)
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
    usbFilterData.m_fActive = true;
    usbFilterData.m_strName = mUSBFilterName.arg(iMaxFilterIndex + 1);
    usbFilterData.m_fHostUSBDevice = false;

    /* Add new USB filter data: */
    addUSBFilter(usbFilterData, true /* its new? */);

    /* Revalidate: */
    revalidate();
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

void UIMachineSettingsUSB::edtClicked()
{
    /* Get current USB filter item: */
    QTreeWidgetItem *pItem = mTwFilters->currentItem();
    Assert(pItem);
    UIDataSettingsMachineUSBFilter &usbFilterData = m_filters[mTwFilters->indexOfTopLevelItem(pItem)];

    /* Configure USB filter details dialog: */
    UIMachineSettingsUSBFilterDetails dlgFilterDetails(this);
    dlgFilterDetails.mLeName->setText(usbFilterData.m_strName);
    dlgFilterDetails.mLeVendorID->setText(usbFilterData.m_strVendorId);
    dlgFilterDetails.mLeProductID->setText(usbFilterData.m_strProductId);
    dlgFilterDetails.mLeRevision->setText(usbFilterData.m_strRevision);
    dlgFilterDetails.mLePort->setText(usbFilterData.m_strPort);
    dlgFilterDetails.mLeManufacturer->setText(usbFilterData.m_strManufacturer);
    dlgFilterDetails.mLeProduct->setText(usbFilterData.m_strProduct);
    dlgFilterDetails.mLeSerialNo->setText(usbFilterData.m_strSerialNumber);
    QString strRemote = usbFilterData.m_strRemote.toLower();
    if (strRemote == "yes" || strRemote == "true" || strRemote == "1")
        dlgFilterDetails.mCbRemote->setCurrentIndex(ModeOn);
    else if (strRemote == "no" || strRemote == "false" || strRemote == "0")
        dlgFilterDetails.mCbRemote->setCurrentIndex(ModeOff);
    else
        dlgFilterDetails.mCbRemote->setCurrentIndex(ModeAny);

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
        switch (dlgFilterDetails.mCbRemote->currentIndex())
        {
            case ModeAny: usbFilterData.m_strRemote = QString(); break;
            case ModeOn:  usbFilterData.m_strRemote = QString::number(1); break;
            case ModeOff: usbFilterData.m_strRemote = QString::number(0); break;
            default: AssertMsgFailed(("Invalid combo box index"));
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

    /* Revalidate: */
    revalidate();
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

void UIMachineSettingsUSB::addUSBFilter(const UIDataSettingsMachineUSBFilter &usbFilterData, bool fIsNew)
{
    /* Append internal list with data: */
    m_filters << usbFilterData;

    /* Append tree-widget with item: */
    UIUSBFilterItem *pItem = new UIUSBFilterItem;
    pItem->setCheckState(0, usbFilterData.m_fActive ? Qt::Checked : Qt::Unchecked);
    pItem->setText(0, usbFilterData.m_strName);
    pItem->setToolTip(0, toolTipFor(usbFilterData));
    mTwFilters->addTopLevelItem(pItem);

    /* Select this item if its new: */
    if (fIsNew)
        mTwFilters->setCurrentItem(pItem);
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
                                                          .arg(gpConverter->toString(usbFilterData.m_hostUSBDeviceState));
    }

    return strToolTip;
}

#include "UIMachineSettingsUSB.moc"

