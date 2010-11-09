/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIMachineSettingsNetwork class implementation
 */

/*
 * Copyright (C) 2008-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* VBox Includes */
#ifdef VBOX_WITH_VDE
# include <iprt/ldr.h>
# include <VBox/VDEPlug.h>
#endif

#include "QIWidgetValidator.h"
#include "QIArrowButtonSwitch.h"
#include "VBoxGlobal.h"
#include "UIMachineSettingsNetwork.h"
#include "QITabWidget.h"

/* Qt Includes */
#include <QTimer>
#include <QCompleter>

/* Empty item extra-code */
const char *emptyItemCode = "#empty#";

/* UIMachineSettingsNetwork Stuff */
UIMachineSettingsNetwork::UIMachineSettingsNetwork (UIMachineSettingsNetworkPage *aParent, bool aDisableStaticControls)
    : QIWithRetranslateUI <QWidget> (0)
    , mParent (aParent)
    , mValidator (0)
    , m_iSlot(-1)
    , mPolished (false)
    , mDisableStaticControls (false)
{
    /* Apply UI decorations */
    Ui::UIMachineSettingsNetwork::setupUi (this);

    /* Setup widgets */
    mCbAdapterName->setInsertPolicy (QComboBox::NoInsert);
    mLeMAC->setValidator (new QRegExpValidator (QRegExp (
                          "[0-9A-Fa-f][02468ACEace][0-9A-Fa-f]{10}"), this));
    mLeMAC->setMinimumWidthByText (QString().fill ('0', 12));

    /* Setup connections */
    connect (mAbsAdvanced, SIGNAL (clicked()), this, SLOT (toggleAdvanced()));
    connect (mTbMAC, SIGNAL (clicked()), this, SLOT (generateMac()));
    connect (mPbPortForwarding, SIGNAL (clicked()), this, SLOT (sltOpenPortForwardingDlg()));

#ifdef Q_WS_MAC
    /* Prevent this widgets to go in the Small/Mini size state which is
     * available on Mac OS X. Not sure why this happens but this seems to help
     * against. */
    QList <QWidget*> list = findChildren <QWidget*>();
    foreach (QWidget *w, list)
        if (w->parent() == this)
            w->setFixedHeight (w->sizeHint().height());
#endif /* Q_WS_MAC */

    /* Applying language settings */
    retranslateUi();

    /* If some controls should be disabled or not when the
     * same tab widgets are shown during runtime
     */
    mDisableStaticControls = aDisableStaticControls;
}

void UIMachineSettingsNetwork::fetchAdapterData(const UINetworkAdapterData &data)
{
    /* Load adapter & slot number: */
    m_iSlot = data.m_iSlot;
    m_adapter = data.m_adapter;

    /* Load adapter activity state: */
    mCbEnableAdapter->setChecked(data.m_fAdapterEnabled);

    /* Load adapter type: */
    int adapterPos = mCbAdapterType->findData(data.m_adapterType);
    mCbAdapterType->setCurrentIndex(adapterPos == -1 ? 0 : adapterPos);

    /* Load attachment type: */
    int attachmentPos = mCbAttachmentType->findData(data.m_attachmentType);
    mCbAttachmentType->setCurrentIndex(attachmentPos == -1 ? 0 : attachmentPos);

    /* Load alternative name: */
    switch (attachmentType())
    {
        case KNetworkAttachmentType_Bridged:
            mBrgName = data.m_strBridgedAdapterName;
            if (mBrgName.isEmpty()) mBrgName = QString();
            break;
        case KNetworkAttachmentType_Internal:
            mIntName = data.m_strInternalNetworkName;
            if (mIntName.isEmpty()) mIntName = QString();
            break;
        case KNetworkAttachmentType_HostOnly:
            mHoiName = data.m_strHostInterfaceName;
            if (mHoiName.isEmpty()) mHoiName = QString();
            break;
#ifdef VBOX_WITH_VDE
        case KNetworkAttachmentType_VDE:
            mVDEName = data.m_strVDENetworkName;
            if (mVDEName.isEmpty()) mVDEName = QString();
            break;
#endif
        default:
            break;
    }
    updateAttachmentAlternative();

    /* Other options: */
    mLeMAC->setText(data.m_strMACAddress);
    mCbCableConnected->setChecked(data.m_fCableConnected);

    /* Load port forwarding rules: */
    mPortForwardingRules = data.m_redirects;
}

void UIMachineSettingsNetwork::uploadAdapterData(UINetworkAdapterData &data)
{
    /* Save adapter activity state: */
    data.m_fAdapterEnabled = mCbEnableAdapter->isChecked();

    /* Save adapter type: */
    data.m_adapterType = (KNetworkAdapterType)mCbAdapterType->itemData(mCbAdapterType->currentIndex()).toInt();

    /* Save attachment type & alternative name: */
    data.m_attachmentType = attachmentType();
    switch (data.m_attachmentType)
    {
        case KNetworkAttachmentType_Null:
            break;
        case KNetworkAttachmentType_NAT:
            break;
        case KNetworkAttachmentType_Bridged:
            data.m_strBridgedAdapterName = alternativeName();
            break;
        case KNetworkAttachmentType_Internal:
            data.m_strInternalNetworkName = alternativeName();
            break;
        case KNetworkAttachmentType_HostOnly:
            data.m_strHostInterfaceName = alternativeName();
            break;
#ifdef VBOX_WITH_VDE
        case KNetworkAttachmentType_VDE:
            data.m_strVDENetworkName = alternativeName();
            break;
#endif
        default:
            break;
    }

    /* Other options: */
    data.m_strMACAddress = mLeMAC->text().isEmpty() ? QString() : mLeMAC->text();
    data.m_fCableConnected = mCbCableConnected->isChecked();

    /* Save port forwarding rules: */
    data.m_redirects = mPortForwardingRules;
}

void UIMachineSettingsNetwork::setValidator (QIWidgetValidator *aValidator)
{
    mValidator = aValidator;

    if (!mDisableStaticControls)
        connect (mCbEnableAdapter, SIGNAL (toggled (bool)),
                 mValidator, SLOT (revalidate()));
    connect (mCbAttachmentType, SIGNAL (activated (const QString&)),
             this, SLOT (updateAttachmentAlternative()));
    connect (mCbAdapterName, SIGNAL (activated (const QString&)),
             this, SLOT (updateAlternativeName()));
    connect (mCbAdapterName, SIGNAL (editTextChanged (const QString&)),
             this, SLOT (updateAlternativeName()));

    if (!mDisableStaticControls)
        mValidator->revalidate();
}

bool UIMachineSettingsNetwork::revalidate (QString &aWarning, QString &aTitle)
{
    /* 'True' for disabled adapter */
    if (!mCbEnableAdapter->isChecked())
        return true;

    /* Validate alternatives */
    bool valid = true;
    switch (attachmentType())
    {
        case KNetworkAttachmentType_Bridged:
            if (alternativeName().isNull())
            {
                aWarning = tr ("no bridged network adapter is selected");
                valid = false;
            }
            break;
        case KNetworkAttachmentType_Internal:
            if (alternativeName().isNull())
            {
                aWarning = tr ("no internal network name is specified");
                valid = false;
            }
            break;
        case KNetworkAttachmentType_HostOnly:
            if (alternativeName().isNull())
            {
                aWarning = tr ("no host-only network adapter is selected");
                valid = false;
            }
            break;
        default:
            break;
    }

    if (!valid)
        aTitle += ": " + vboxGlobal().removeAccelMark (pageTitle());

    return valid;
}

QWidget* UIMachineSettingsNetwork::setOrderAfter (QWidget *aAfter)
{
    setTabOrder (aAfter, mCbEnableAdapter);
    setTabOrder (mCbEnableAdapter, mCbAttachmentType);
    setTabOrder (mCbAttachmentType, mCbAdapterName);
    setTabOrder (mCbAdapterName, mAbsAdvanced);
    setTabOrder (mAbsAdvanced, mCbAdapterType);
    setTabOrder (mCbAdapterType, mLeMAC);
    setTabOrder (mLeMAC, mTbMAC);
    setTabOrder (mTbMAC, mCbCableConnected);
    setTabOrder (mCbCableConnected, mPbPortForwarding);
    return mPbPortForwarding;
}

QString UIMachineSettingsNetwork::pageTitle() const
{
    return VBoxGlobal::tr("Adapter %1", "network").arg(QString("&%1").arg(m_iSlot + 1));;
}

KNetworkAttachmentType UIMachineSettingsNetwork::attachmentType() const
{
    return (KNetworkAttachmentType) mCbAttachmentType->itemData (
           mCbAttachmentType->currentIndex()).toInt();
}

QString UIMachineSettingsNetwork::alternativeName (int aType) const
{
    if (aType == -1) aType = attachmentType();
    QString result;
    switch (aType)
    {
        case KNetworkAttachmentType_Bridged:
            result = mBrgName;
            break;
        case KNetworkAttachmentType_Internal:
            result = mIntName;
            break;
        case KNetworkAttachmentType_HostOnly:
            result = mHoiName;
            break;
#ifdef VBOX_WITH_VDE
        case KNetworkAttachmentType_VDE:
            result = mVDEName;
            break;
#endif
        default:
            break;
    }
    Assert (result.isNull() || !result.isEmpty());
    return result;
}

void UIMachineSettingsNetwork::showEvent (QShowEvent *aEvent)
{
    if (!mPolished)
    {
        mPolished = true;

        /* Give the minimum size hint to the first layout column */
        mNetworkChildGridLayout->setColumnMinimumWidth (0, mLbAttachmentType->width());

        if (mDisableStaticControls)
        {
            /* Disable controls for dynamically displayed page */
            mCbEnableAdapter->setEnabled (false);
            mCbAdapterType->setEnabled (false);
            mLeMAC->setEnabled (false);
            mTbMAC->setEnabled (false);
            mLbAdapterType->setEnabled (false);
            mLbMAC->setEnabled (false);
            mAbsAdvanced->animateClick();
        }
        else
        {
            /* Hide advanced items initially */
            toggleAdvanced();
        }
    }
    QWidget::showEvent (aEvent);
}

void UIMachineSettingsNetwork::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::UIMachineSettingsNetwork::retranslateUi (this);

    /* Translate combo-boxes content */
    populateComboboxes();

    /* Translate attachment info */
    updateAttachmentAlternative();
}

void UIMachineSettingsNetwork::updateAttachmentAlternative()
{
    /* Blocking signals to change content manually */
    mCbAdapterName->blockSignals (true);

    /* Update alternative-name combo-box availability */
    mLbAdapterName->setEnabled (attachmentType() != KNetworkAttachmentType_Null &&
                                attachmentType() != KNetworkAttachmentType_NAT);
    mCbAdapterName->setEnabled (attachmentType() != KNetworkAttachmentType_Null &&
                                attachmentType() != KNetworkAttachmentType_NAT);

    /* Refresh list */
    mCbAdapterName->clear();
    switch (attachmentType())
    {
        case KNetworkAttachmentType_Bridged:
            mCbAdapterName->insertItems (0, mParent->brgList());
            mCbAdapterName->setEditable (false);
            break;
        case KNetworkAttachmentType_Internal:
            mCbAdapterName->insertItems (0, mParent->fullIntList());
            mCbAdapterName->setEditable (true);
            mCbAdapterName->setCompleter (0);
            break;
        case KNetworkAttachmentType_HostOnly:
            mCbAdapterName->insertItems (0, mParent->hoiList());
            mCbAdapterName->setEditable (false);
            break;
#ifdef VBOX_WITH_VDE
        case KNetworkAttachmentType_VDE:
            mCbAdapterName->insertItem(0, alternativeName());
            mCbAdapterName->setEditable (true);
            mCbAdapterName->setCompleter (0);
            break;
#endif
        default:
            break;
    }

    /* Prepend 'empty' or 'default' item */
    if (mCbAdapterName->count() == 0)
    {
        switch (attachmentType())
        {
            case KNetworkAttachmentType_Bridged:
            case KNetworkAttachmentType_HostOnly:
            {
                /* Adapters list 'empty' */
                int pos = mCbAdapterName->findData (emptyItemCode);
                if (pos == -1)
                    mCbAdapterName->insertItem (0, tr ("Not selected", "network adapter name"), emptyItemCode);
                else
                    mCbAdapterName->setItemText (pos, tr ("Not selected", "network adapter name"));
                break;
            }
            case KNetworkAttachmentType_Internal:
            {
                /* Internal network 'default' name */
                if (mCbAdapterName->findText ("intnet") == -1)
                    mCbAdapterName->insertItem (0, "intnet");
                break;
            }
            default:
                break;
        }
    }

    /* Select previous or default item */
    switch (attachmentType())
    {
        case KNetworkAttachmentType_Bridged:
        case KNetworkAttachmentType_HostOnly:
        {
            int pos = mCbAdapterName->findText (alternativeName());
            mCbAdapterName->setCurrentIndex (pos == -1 ? 0 : pos);
            break;
        }
        case KNetworkAttachmentType_Internal:
        {
            int pos = mCbAdapterName->findText (alternativeName());
            mCbAdapterName->setCurrentIndex (pos == -1 ? 0 : pos);
            break;
        }
        default:
            break;
    }

    /* Remember selected item */
    updateAlternativeName();

    /* Update Forwarding rules button availability: */
    mPbPortForwarding->setEnabled(attachmentType() == KNetworkAttachmentType_NAT);

    /* Unblocking signals as content is changed already */
    mCbAdapterName->blockSignals (false);
}

void UIMachineSettingsNetwork::updateAlternativeName()
{
    switch (attachmentType())
    {
        case KNetworkAttachmentType_Bridged:
        {
            QString newName (mCbAdapterName->itemData (mCbAdapterName->currentIndex()).toString() ==
                             QString (emptyItemCode) ||
                             mCbAdapterName->currentText().isEmpty() ?
                             QString::null : mCbAdapterName->currentText());
            if (mBrgName != newName)
                mBrgName = newName;
            break;
        }
        case KNetworkAttachmentType_Internal:
        {
            QString newName ((mCbAdapterName->itemData (mCbAdapterName->currentIndex()).toString() ==
                              QString (emptyItemCode) &&
                              mCbAdapterName->currentText() ==
                              mCbAdapterName->itemText (mCbAdapterName->currentIndex())) ||
                              mCbAdapterName->currentText().isEmpty() ?
                              QString::null : mCbAdapterName->currentText());
            if (mIntName != newName)
            {
                mIntName = newName;
                if (!mIntName.isNull())
                    QTimer::singleShot (0, mParent, SLOT (updatePages()));
            }
            break;
        }
        case KNetworkAttachmentType_HostOnly:
        {
            QString newName (mCbAdapterName->itemData (mCbAdapterName->currentIndex()).toString() ==
                             QString (emptyItemCode) ||
                             mCbAdapterName->currentText().isEmpty() ?
                             QString::null : mCbAdapterName->currentText());
            if (mHoiName != newName)
                mHoiName = newName;
            break;
        }
#ifdef VBOX_WITH_VDE
        case KNetworkAttachmentType_VDE:
        {
            QString newName ((mCbAdapterName->itemData (mCbAdapterName->currentIndex()).toString() ==
                              QString (emptyItemCode) &&
                              mCbAdapterName->currentText() ==
                              mCbAdapterName->itemText (mCbAdapterName->currentIndex())) ||
                              mCbAdapterName->currentText().isEmpty() ?
                              QString::null : mCbAdapterName->currentText());
            if (mVDEName != newName)
                mVDEName = newName;
            break;
        }
#endif
        default:
            break;
    }

    if (mValidator)
        mValidator->revalidate();
}

void UIMachineSettingsNetwork::toggleAdvanced()
{
    mLbAdapterType->setVisible (mAbsAdvanced->isExpanded());
    mCbAdapterType->setVisible (mAbsAdvanced->isExpanded());
    mLbMAC->setVisible (mAbsAdvanced->isExpanded());
    mLeMAC->setVisible (mAbsAdvanced->isExpanded());
    mTbMAC->setVisible (mAbsAdvanced->isExpanded());
    mCbCableConnected->setVisible (mAbsAdvanced->isExpanded());
    mPbPortForwarding->setVisible (mAbsAdvanced->isExpanded());
}

void UIMachineSettingsNetwork::generateMac()
{
    m_adapter.SetMACAddress(QString::null);
    mLeMAC->setText(m_adapter.GetMACAddress());
}

void UIMachineSettingsNetwork::sltOpenPortForwardingDlg()
{
    UIMachineSettingsPortForwardingDlg dlg(this, mPortForwardingRules);
    if (dlg.exec() == QDialog::Accepted)
        mPortForwardingRules = dlg.rules();
}

void UIMachineSettingsNetwork::populateComboboxes()
{
    /* Save the current selected adapter */
    int currentAdapter = mCbAdapterType->currentIndex();

    /* Clear the adapters combo-box */
    mCbAdapterType->clear();

    /* Populate adapters */
    mCbAdapterType->insertItem (0,
        vboxGlobal().toString (KNetworkAdapterType_Am79C970A));
    mCbAdapterType->setItemData (0,
        KNetworkAdapterType_Am79C970A);
    mCbAdapterType->setItemData (0,
        mCbAdapterType->itemText (0), Qt::ToolTipRole);
    mCbAdapterType->insertItem (1,
        vboxGlobal().toString (KNetworkAdapterType_Am79C973));
    mCbAdapterType->setItemData (1,
        KNetworkAdapterType_Am79C973);
    mCbAdapterType->setItemData (1,
        mCbAdapterType->itemText (1), Qt::ToolTipRole);
#ifdef VBOX_WITH_E1000
    mCbAdapterType->insertItem (2,
        vboxGlobal().toString (KNetworkAdapterType_I82540EM));
    mCbAdapterType->setItemData (2,
        KNetworkAdapterType_I82540EM);
    mCbAdapterType->setItemData (2,
        mCbAdapterType->itemText (2), Qt::ToolTipRole);
    mCbAdapterType->insertItem (3,
        vboxGlobal().toString (KNetworkAdapterType_I82543GC));
    mCbAdapterType->setItemData (3,
        KNetworkAdapterType_I82543GC);
    mCbAdapterType->setItemData (3,
        mCbAdapterType->itemText (3), Qt::ToolTipRole);
    mCbAdapterType->insertItem (4,
        vboxGlobal().toString (KNetworkAdapterType_I82545EM));
    mCbAdapterType->setItemData (4,
        KNetworkAdapterType_I82545EM);
    mCbAdapterType->setItemData (4,
        mCbAdapterType->itemText (4), Qt::ToolTipRole);
#endif /* VBOX_WITH_E1000 */
#ifdef VBOX_WITH_VIRTIO
    mCbAdapterType->insertItem (5,
        vboxGlobal().toString (KNetworkAdapterType_Virtio));
    mCbAdapterType->setItemData (5,
        KNetworkAdapterType_Virtio);
    mCbAdapterType->setItemData (5,
        mCbAdapterType->itemText (5), Qt::ToolTipRole);
#endif /* VBOX_WITH_VIRTIO */

    /* Set the old value */
    mCbAdapterType->setCurrentIndex (currentAdapter == -1 ?
                                     0 : currentAdapter);


    /* Save the current selected attachment type */
    int currentAttachment = mCbAttachmentType->currentIndex();

    /* Clear the attachments combo-box */
    mCbAttachmentType->clear();

    /* Populate attachments */
    mCbAttachmentType->insertItem (0,
        vboxGlobal().toString (KNetworkAttachmentType_Null));
    mCbAttachmentType->setItemData (0,
        KNetworkAttachmentType_Null);
    mCbAttachmentType->setItemData (0,
        mCbAttachmentType->itemText (0), Qt::ToolTipRole);
    mCbAttachmentType->insertItem (1,
        vboxGlobal().toString (KNetworkAttachmentType_NAT));
    mCbAttachmentType->setItemData (1,
        KNetworkAttachmentType_NAT);
    mCbAttachmentType->setItemData (1,
        mCbAttachmentType->itemText (1), Qt::ToolTipRole);
    mCbAttachmentType->insertItem (2,
        vboxGlobal().toString (KNetworkAttachmentType_Bridged));
    mCbAttachmentType->setItemData (2,
        KNetworkAttachmentType_Bridged);
    mCbAttachmentType->setItemData (2,
        mCbAttachmentType->itemText (2), Qt::ToolTipRole);
    mCbAttachmentType->insertItem (3,
        vboxGlobal().toString (KNetworkAttachmentType_Internal));
    mCbAttachmentType->setItemData (3,
        KNetworkAttachmentType_Internal);
    mCbAttachmentType->setItemData (3,
        mCbAttachmentType->itemText (3), Qt::ToolTipRole);
    mCbAttachmentType->insertItem (4,
        vboxGlobal().toString (KNetworkAttachmentType_HostOnly));
    mCbAttachmentType->setItemData (4,
        KNetworkAttachmentType_HostOnly);
    mCbAttachmentType->setItemData (4,
        mCbAttachmentType->itemText (4), Qt::ToolTipRole);
#ifdef VBOX_WITH_VDE
    RTLDRMOD hLdrDummy;
    if (RT_SUCCESS(RTLdrLoad(VBOX_LIB_VDE_PLUG_NAME, &hLdrDummy)))
    {
        mCbAttachmentType->insertItem (5,
            vboxGlobal().toString (KNetworkAttachmentType_VDE));
        mCbAttachmentType->setItemData (5,
            KNetworkAttachmentType_VDE);
        mCbAttachmentType->setItemData (5,
            mCbAttachmentType->itemText (5), Qt::ToolTipRole);
    }
#endif

    /* Set the old value */
    mCbAttachmentType->setCurrentIndex (currentAttachment);
}

/* UIMachineSettingsNetworkPage Stuff */
UIMachineSettingsNetworkPage::UIMachineSettingsNetworkPage(bool aDisableStaticControls)
    : mValidator(0)
    , mTwAdapters(0)
    , mDisableStaticControls(false)
{
    /* Setup Main Layout */
    QVBoxLayout *mainLayout = new QVBoxLayout (this);
    mainLayout->setContentsMargins (0, 5, 0, 5);

    /* Creating Tab Widget */
    mTwAdapters = new QITabWidget (this);
    mainLayout->addWidget (mTwAdapters);

    /* If some controls should be disabled or not when the
     * same tab widgets are shown during runtime
     */
    mDisableStaticControls = aDisableStaticControls;

    /* How many adapters to display */
    ulong uCount = qMin((ULONG)4, vboxGlobal().virtualBox().GetSystemProperties().GetNetworkAdapterCount());
    /* Add the tab pages to parent tab widget. Needed for space calculations. */
    for (ulong iSlot = 0; iSlot < uCount; ++iSlot)
    {
        /* Creating adapter's page: */
        UIMachineSettingsNetwork *pPage = new UIMachineSettingsNetwork(this, mDisableStaticControls);

        /* Attach adapter's page to Tab Widget: */
        mTwAdapters->addTab(pPage, pPage->pageTitle());

        /* Disable tab page of disabled adapter if it is being configured dynamically: */
        if (mDisableStaticControls && !m_cache.m_items[iSlot].m_fAdapterEnabled)
            mTwAdapters->setTabEnabled(iSlot, false);
    }
}

void UIMachineSettingsNetworkPage::loadDirectlyFrom(const CMachine &machine)
{
    qRegisterMetaType<UISettingsDataMachine>();
    UISettingsDataMachine data(machine);
    QVariant wrapper = QVariant::fromValue(data);
    loadToCacheFrom(wrapper);
    getFromCache();
}

void UIMachineSettingsNetworkPage::saveDirectlyTo(CMachine &machine)
{
    qRegisterMetaType<UISettingsDataMachine>();
    UISettingsDataMachine data(machine);
    QVariant wrapper = QVariant::fromValue(data);
    putToCache();
    saveFromCacheTo(wrapper);
}

QStringList UIMachineSettingsNetworkPage::brgList (bool aRefresh)
{
    if (aRefresh)
    {
        /* Load & filter interface list */
        mBrgList.clear();
        CHostNetworkInterfaceVector interfaces =
            vboxGlobal().virtualBox().GetHost().GetNetworkInterfaces();
        for (CHostNetworkInterfaceVector::ConstIterator it = interfaces.begin();
             it != interfaces.end(); ++ it)
        {
            if (it->GetInterfaceType() == KHostNetworkInterfaceType_Bridged)
                mBrgList << it->GetName();
        }
    }

    return mBrgList;
}

QStringList UIMachineSettingsNetworkPage::intList (bool aRefresh)
{
    if (aRefresh)
    {
        /* Load total network list of all VMs */
        mIntList.clear();
        CVirtualBox vbox = vboxGlobal().virtualBox();
        ulong count = qMin ((ULONG) 4, vbox.GetSystemProperties().GetNetworkAdapterCount());
        CMachineVector vec = vbox.GetMachines();
        for (CMachineVector::ConstIterator m = vec.begin(); m != vec.end(); ++ m)
        {
            if (m->GetAccessible())
            {
                for (ulong slot = 0; slot < count; ++ slot)
                {
                    QString name = m->GetNetworkAdapter (slot).GetInternalNetwork();
                    if (!name.isEmpty() && !mIntList.contains (name))
                        mIntList << name;
                }
            }
        }
    }

    return mIntList;
}

QStringList UIMachineSettingsNetworkPage::fullIntList (bool aRefresh)
{
    QStringList list (intList (aRefresh));
    /* Append network list with names from all the pages */
    for (int index = 0; index < mTwAdapters->count(); ++ index)
    {
        UIMachineSettingsNetwork *page =
            qobject_cast <UIMachineSettingsNetwork*> (mTwAdapters->widget (index));
        if (page)
        {
            QString name = page->alternativeName (KNetworkAttachmentType_Internal);
            if (!name.isEmpty() && !list.contains (name))
                list << name;
        }
    }
    return list;
}

QStringList UIMachineSettingsNetworkPage::hoiList (bool aRefresh)
{
    if (aRefresh)
    {
        /* Load & filter interface list */
        mHoiList.clear();
        CHostNetworkInterfaceVector interfaces =
            vboxGlobal().virtualBox().GetHost().GetNetworkInterfaces();
        for (CHostNetworkInterfaceVector::ConstIterator it = interfaces.begin();
             it != interfaces.end(); ++ it)
        {
            if (it->GetInterfaceType() == KHostNetworkInterfaceType_HostOnly)
                mHoiList << it->GetName();
        }
    }

    return mHoiList;
}

/* Load data to cashe from corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void UIMachineSettingsNetworkPage::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Cache names lists: */
    brgList(true);
    intList(true);
    hoiList(true);

    /* Load adapters data: */
    ulong uCount = qMin((ULONG)4, vboxGlobal().virtualBox().GetSystemProperties().GetNetworkAdapterCount());
    for (ulong uSlot = 0; uSlot < uCount; ++uSlot)
    {
        /* Get adapter: */
        const CNetworkAdapter &adapter = m_machine.GetNetworkAdapter(uSlot);

        /* Prepare adapter's data container: */
        UINetworkAdapterData data;

        /* Load main options: */
        data.m_iSlot = uSlot;
        data.m_adapter = adapter;
        data.m_fAdapterEnabled = adapter.GetEnabled();
        data.m_adapterType = adapter.GetAdapterType();
        data.m_attachmentType = adapter.GetAttachmentType();
        switch (data.m_attachmentType)
        {
            case KNetworkAttachmentType_Bridged:
                data.m_strBridgedAdapterName = adapter.GetHostInterface();
                if (data.m_strBridgedAdapterName.isEmpty()) data.m_strBridgedAdapterName = QString();
                break;
            case KNetworkAttachmentType_Internal:
                data.m_strInternalNetworkName = adapter.GetInternalNetwork();
                if (data.m_strInternalNetworkName.isEmpty()) data.m_strInternalNetworkName = QString();
                break;
            case KNetworkAttachmentType_HostOnly:
                data.m_strHostInterfaceName = adapter.GetHostInterface();
                if (data.m_strHostInterfaceName.isEmpty()) data.m_strHostInterfaceName = QString();
                break;
#ifdef VBOX_WITH_VDE
            case KNetworkAttachmentType_VDE:
                data.m_strVDENetworkName = adapter.GetVDENetwork();
                if (data.m_strVDENetworkName.isEmpty()) data.m_strVDENetworkName = QString();
                break;
#endif
            default:
                break;
        }

        /* Load advanced options: */
        data.m_strMACAddress = adapter.GetMACAddress();
        data.m_fCableConnected = adapter.GetCableConnected();

        /* Load redirect options: */
        QVector<QString> redirects = adapter.GetNatDriver().GetRedirects();
        for (int i = 0; i < redirects.size(); ++i)
        {
            QStringList redirectData = redirects[i].split(',');
            AssertMsg(redirectData.size() == 6, ("Redirect rule should be composed of 6 parts!\n"));
            data.m_redirects << UIPortForwardingData(redirectData[0],
                                                     (KNATProtocol)redirectData[1].toUInt(),
                                                     redirectData[2],
                                                     redirectData[3].toUInt(),
                                                     redirectData[4],
                                                     redirectData[5].toUInt());
        }

        /* Append adapter's data container: */
        m_cache.m_items << data;
    }

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

/* Load data to corresponding widgets from cache,
 * this task SHOULD be performed in GUI thread only: */
void UIMachineSettingsNetworkPage::getFromCache()
{
    /* Setup tab order: */
    Assert(m_pFirstWidget);
    setTabOrder(m_pFirstWidget, mTwAdapters->focusProxy());
    QWidget *pLastFocusWidget = mTwAdapters->focusProxy();

    int uCount = qMin(mTwAdapters->count(), m_cache.m_items.size());
    for (int iSlot = 0; iSlot < uCount; ++iSlot)
    {
        UIMachineSettingsNetwork *pPage =
            qobject_cast<UIMachineSettingsNetwork *>(mTwAdapters->widget(iSlot));
        Assert(pPage);

        /* Loading adapter's data into page: */
        pPage->fetchAdapterData(m_cache.m_items[iSlot]);

        /* Disable tab page of disabled adapter if it is being configured dynamically: */
        if (mDisableStaticControls && !m_cache.m_items[iSlot].m_fAdapterEnabled)
            mTwAdapters->setTabEnabled(iSlot, false);

        /* Setup page validation: */
        pPage->setValidator(mValidator);

        /* Setup tab order: */
        pLastFocusWidget = pPage->setOrderAfter(pLastFocusWidget);
    }

    /* Applying language settings: */
    retranslateUi();

    /* Revalidate if possible: */
    if (mValidator) mValidator->revalidate();
}

/* Save data from corresponding widgets to cache,
 * this task SHOULD be performed in GUI thread only: */
void UIMachineSettingsNetworkPage::putToCache()
{
    /* Gather internal variables data from QWidget(s): */
    for (int iSlot = 0; iSlot < m_cache.m_items.size(); ++iSlot)
    {
        /* Getting adapter's page: */
        UIMachineSettingsNetwork *pPage = qobject_cast<UIMachineSettingsNetwork*>(mTwAdapters->widget(iSlot));

        /* Loading Adapter's data from page: */
        pPage->uploadAdapterData(m_cache.m_items[iSlot]);
    }
}

/* Save data from cache to corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void UIMachineSettingsNetworkPage::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Gather corresponding values from internal variables: */
    for (int iSlot = 0; iSlot < m_cache.m_items.size(); ++iSlot)
    {
        /* Get adapter: */
        CNetworkAdapter adapter = m_machine.GetNetworkAdapter(iSlot);

        /* Get cached data for this adapter: */
        const UINetworkAdapterData &data = m_cache.m_items[iSlot];

        /* Save main options: */
        adapter.SetEnabled(data.m_fAdapterEnabled);
        adapter.SetAdapterType(data.m_adapterType);
        switch (data.m_attachmentType)
        {
            case KNetworkAttachmentType_Null:
                adapter.Detach();
                break;
            case KNetworkAttachmentType_NAT:
                adapter.AttachToNAT();
                break;
            case KNetworkAttachmentType_Bridged:
                adapter.SetHostInterface(data.m_strBridgedAdapterName);
                adapter.AttachToBridgedInterface();
                break;
            case KNetworkAttachmentType_Internal:
                adapter.SetInternalNetwork(data.m_strInternalNetworkName);
                adapter.AttachToInternalNetwork();
                break;
            case KNetworkAttachmentType_HostOnly:
                adapter.SetHostInterface(data.m_strHostInterfaceName);
                adapter.AttachToHostOnlyInterface();
                break;
    #ifdef VBOX_WITH_VDE
            case KNetworkAttachmentType_VDE:
                adapter.SetVDENetwork(data.m_strVDENetworkName);
                adapter.AttachToVDE();
                break;
    #endif
            default:
                break;
        }

        /* Save advanced options: */
        adapter.SetMACAddress(data.m_strMACAddress);
        adapter.SetCableConnected(data.m_fCableConnected);

        /* Save redirect options: */
        QVector<QString> oldRedirects = adapter.GetNatDriver().GetRedirects();
        for (int i = 0; i < oldRedirects.size(); ++i)
            adapter.GetNatDriver().RemoveRedirect(oldRedirects[i].section(',', 0, 0));
        UIPortForwardingDataList newRedirects = data.m_redirects;
        for (int i = 0; i < newRedirects.size(); ++i)
        {
            UIPortForwardingData newRedirect = newRedirects[i];
            adapter.GetNatDriver().AddRedirect(newRedirect.name, newRedirect.protocol,
                                               newRedirect.hostIp, newRedirect.hostPort.value(),
                                               newRedirect.guestIp, newRedirect.guestPort.value());
        }
    }

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

void UIMachineSettingsNetworkPage::setValidator (QIWidgetValidator *aVal)
{
    mValidator = aVal;
}

bool UIMachineSettingsNetworkPage::revalidate (QString &aWarning, QString &aTitle)
{
    bool valid = true;

    for (int i = 0; i < mTwAdapters->count(); ++ i)
    {
        UIMachineSettingsNetwork *page =
            qobject_cast <UIMachineSettingsNetwork*> (mTwAdapters->widget (i));
        Assert (page);
        valid = page->revalidate (aWarning, aTitle);
        if (!valid) break;
    }

    return valid;
}

void UIMachineSettingsNetworkPage::retranslateUi()
{
    for (int i = 0; i < mTwAdapters->count(); ++ i)
    {
        UIMachineSettingsNetwork *page =
            qobject_cast <UIMachineSettingsNetwork*> (mTwAdapters->widget (i));
        Assert (page);
        mTwAdapters->setTabText (i, page->pageTitle());
    }
}

void UIMachineSettingsNetworkPage::updatePages()
{
    for (int i = 0; i < mTwAdapters->count(); ++ i)
    {
        /* Get the iterated page */
        UIMachineSettingsNetwork *page =
            qobject_cast <UIMachineSettingsNetwork*> (mTwAdapters->widget (i));
        Assert (page);

        /* Update the page if the attachment type is 'internal network' */
        if (page->attachmentType() == KNetworkAttachmentType_Internal)
            QTimer::singleShot (0, page, SLOT (updateAttachmentAlternative()));
    }
}

