/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMSettingsNetwork class implementation
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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

/* VBox Includes */
#include "QIWidgetValidator.h"
#include "QIArrowButtonSwitch.h"
#include "VBoxGlobal.h"
#include "VBoxVMSettingsNetwork.h"

/* Qt Includes */
#include <QTimer>
#include <QCompleter>

/* Empty item extra-code */
const char *emptyItemCode = "#empty#";

/* VBoxVMSettingsNetwork Stuff */
VBoxVMSettingsNetwork::VBoxVMSettingsNetwork (VBoxVMSettingsNetworkPage *aParent, bool aDisableStaticControls)
    : QIWithRetranslateUI <QWidget> (0)
    , mParent (aParent)
    , mValidator (0)
    , mPolished (false)
    , mDisableStaticControls (false)
{
    /* Apply UI decorations */
    Ui::VBoxVMSettingsNetwork::setupUi (this);

    /* Setup widgets */
    mCbAdapterName->setInsertPolicy (QComboBox::NoInsert);
    mLeMAC->setValidator (new QRegExpValidator (QRegExp (
                          "[0-9A-Fa-f][02468ACEace][0-9A-Fa-f]{10}"), this));
    mLeMAC->setMinimumWidthByText (QString().fill ('0', 12));

    /* Setup connections */
    connect (mAbsAdvanced, SIGNAL (clicked()), this, SLOT (toggleAdvanced()));
    connect (mTbMAC, SIGNAL (clicked()), this, SLOT (generateMac()));

#ifdef Q_WS_MAC
    /* Prevent this widgets to go in the Small/Mini size state which is
     * available on Mac OS X. Not sure why this happens but this seems to help
     * against. */
    QList <QWidget*> list = findChildren <QWidget*>();
    foreach (QWidget *w, list)
        if (w->parent() == this)
            w->setFixedHeight (w->sizeHint().height());

    /* Remove tool-button border at MAC */
    mTbMAC->setStyleSheet ("QToolButton {border: 0px none black;}");
#endif /* Q_WS_MAC */

    /* Applying language settings */
    retranslateUi();

    /* If some controls should be disabled or not when the
     * same tab widgets are shown during runtime
     */
    mDisableStaticControls = aDisableStaticControls;
}

void VBoxVMSettingsNetwork::getFromAdapter (const CNetworkAdapter &aAdapter)
{
    mAdapter = aAdapter;

    /* Load adapter activity state */
    mCbEnableAdapter->setChecked (aAdapter.GetEnabled());

    /* Load adapter type */
    int adapterPos = mCbAdapterType->findData (aAdapter.GetAdapterType());
    mCbAdapterType->setCurrentIndex (adapterPos == -1 ? 0 : adapterPos);

    /* Load attachment type */
    int attachmentPos = mCbAttachmentType->findData (aAdapter.GetAttachmentType());
    mCbAttachmentType->setCurrentIndex (attachmentPos == -1 ? 0 : attachmentPos);

    /* Load alternative name */
    switch (attachmentType())
    {
        case KNetworkAttachmentType_Bridged:
            mBrgName = mAdapter.GetHostInterface();
            if (mBrgName.isEmpty()) mBrgName = QString::null;
            break;
        case KNetworkAttachmentType_Internal:
            mIntName = mAdapter.GetInternalNetwork();
            if (mIntName.isEmpty()) mIntName = QString::null;
            break;
        case KNetworkAttachmentType_HostOnly:
            mHoiName = mAdapter.GetHostInterface();
            if (mHoiName.isEmpty()) mHoiName = QString::null;
            break;
        default:
            break;
    }
    updateAttachmentAlternative();

    mLeMAC->setText (mAdapter.GetMACAddress());
    mCbCableConnected->setChecked (mAdapter.GetCableConnected());
}

void VBoxVMSettingsNetwork::putBackToAdapter()
{
    /* Save adapter activity state */
    mAdapter.SetEnabled (mCbEnableAdapter->isChecked());

    /* Save adapter type */
    KNetworkAdapterType type = (KNetworkAdapterType)
        mCbAdapterType->itemData (mCbAdapterType->currentIndex()).toInt();
    mAdapter.SetAdapterType (type);

    /* Save attachment type & alternative name */
    switch (attachmentType())
    {
        case KNetworkAttachmentType_Null:
            mAdapter.Detach();
            break;
        case KNetworkAttachmentType_NAT:
            mAdapter.AttachToNAT();
            break;
        case KNetworkAttachmentType_Bridged:
            mAdapter.SetHostInterface (alternativeName());
            mAdapter.AttachToBridgedInterface();
            break;
        case KNetworkAttachmentType_Internal:
            mAdapter.SetInternalNetwork (alternativeName());
            mAdapter.AttachToInternalNetwork();
            break;
        case KNetworkAttachmentType_HostOnly:
            mAdapter.SetHostInterface (alternativeName());
            mAdapter.AttachToHostOnlyInterface();
            break;
        default:
            break;
    }

    mAdapter.SetMACAddress (mLeMAC->text().isEmpty() ? QString::null : mLeMAC->text());
    mAdapter.SetCableConnected (mCbCableConnected->isChecked());
}

void VBoxVMSettingsNetwork::setValidator (QIWidgetValidator *aValidator)
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

bool VBoxVMSettingsNetwork::revalidate (QString &aWarning, QString &aTitle)
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

QWidget* VBoxVMSettingsNetwork::setOrderAfter (QWidget *aAfter)
{
    setTabOrder (aAfter, mCbEnableAdapter);
    setTabOrder (mCbEnableAdapter, mCbAttachmentType);
    setTabOrder (mCbAttachmentType, mCbAdapterName);
    setTabOrder (mCbAdapterName, mAbsAdvanced);
    setTabOrder (mAbsAdvanced, mCbAdapterType);
    setTabOrder (mCbAdapterType, mLeMAC);
    setTabOrder (mLeMAC, mTbMAC);
    setTabOrder (mTbMAC, mCbCableConnected);
    return mCbCableConnected;
}

QString VBoxVMSettingsNetwork::pageTitle() const
{
    QString title;
    if (!mAdapter.isNull())
    {
        title = VBoxGlobal::tr ("Adapter %1", "network")
            .arg (QString ("&%1").arg (mAdapter.GetSlot() + 1));
    }
    return title;
}

KNetworkAttachmentType VBoxVMSettingsNetwork::attachmentType() const
{
    return (KNetworkAttachmentType) mCbAttachmentType->itemData (
           mCbAttachmentType->currentIndex()).toInt();
}

QString VBoxVMSettingsNetwork::alternativeName (int aType) const
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
        default:
            break;
    }
    Assert (result.isNull() || !result.isEmpty());
    return result;
}

void VBoxVMSettingsNetwork::showEvent (QShowEvent *aEvent)
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

void VBoxVMSettingsNetwork::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxVMSettingsNetwork::retranslateUi (this);

    /* Translate combo-boxes content */
    populateComboboxes();

    /* Translate attachment info */
    updateAttachmentAlternative();
}

void VBoxVMSettingsNetwork::updateAttachmentAlternative()
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
            mCbAdapterName->insertItems (0, mParent->intList());
            mCbAdapterName->setEditable (true);
            mCbAdapterName->setCompleter (0);
            break;
        case KNetworkAttachmentType_HostOnly:
            mCbAdapterName->insertItems (0, mParent->hoiList());
            mCbAdapterName->setEditable (false);
            break;
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

    /* Unblocking signals as content is changed already */
    mCbAdapterName->blockSignals (false);
}

void VBoxVMSettingsNetwork::updateAlternativeName()
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
        default:
            break;
    }

    if (mValidator)
        mValidator->revalidate();
}

void VBoxVMSettingsNetwork::toggleAdvanced()
{
    mLbAdapterType->setVisible (mAbsAdvanced->isExpanded());
    mCbAdapterType->setVisible (mAbsAdvanced->isExpanded());
    mLbMAC->setVisible (mAbsAdvanced->isExpanded());
    mLeMAC->setVisible (mAbsAdvanced->isExpanded());
    mTbMAC->setVisible (mAbsAdvanced->isExpanded());
    mCbCableConnected->setVisible (mAbsAdvanced->isExpanded());
}

void VBoxVMSettingsNetwork::generateMac()
{
    mAdapter.SetMACAddress (QString::null);
    mLeMAC->setText (mAdapter.GetMACAddress());
}

void VBoxVMSettingsNetwork::populateComboboxes()
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

    /* Set the old value */
    mCbAttachmentType->setCurrentIndex (currentAttachment);
}

/* VBoxVMSettingsNetworkPage Stuff */
VBoxVMSettingsNetworkPage::VBoxVMSettingsNetworkPage(bool aDisableStaticControls)
    : mValidator (0)
    , mDisableStaticControls (false)
{
    /* Setup Main Layout */
    QVBoxLayout *mainLayout = new QVBoxLayout (this);
    mainLayout->setContentsMargins (0, 5, 0, 5);

    /* Creating Tab Widget */
    mTwAdapters = new QTabWidget (this);
    mainLayout->addWidget (mTwAdapters);

    /* If some controls should be disabled or not when the
     * same tab widgets are shown during runtime
     */
    mDisableStaticControls = aDisableStaticControls;
}

QStringList VBoxVMSettingsNetworkPage::brgList (bool aRefresh)
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

QStringList VBoxVMSettingsNetworkPage::intList (bool aRefresh)
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

    /* Append network list with names from all the pages */
    QStringList list (mIntList);
    for (int index = 0; index < mTwAdapters->count(); ++ index)
    {
        VBoxVMSettingsNetwork *page =
            qobject_cast <VBoxVMSettingsNetwork*> (mTwAdapters->widget (index));
        if (page)
        {
            QString name = page->alternativeName (KNetworkAttachmentType_Internal);
            if (!name.isEmpty() && !list.contains (name))
                list << name;
        }
    }

    return list;
}

QStringList VBoxVMSettingsNetworkPage::hoiList (bool aRefresh)
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

void VBoxVMSettingsNetworkPage::getFrom (const CMachine &aMachine)
{
    /* Setup tab order */
    Assert (mFirstWidget);
    setTabOrder (mFirstWidget, mTwAdapters->focusProxy());
    QWidget *lastFocusWidget = mTwAdapters->focusProxy();

    /* Cache data */
    brgList (true);
    intList (true);
    hoiList (true);

    /* Creating Tab Pages */
    CVirtualBox vbox = vboxGlobal().virtualBox();
    ulong count = qMin ((ULONG) 4, vbox.GetSystemProperties().GetNetworkAdapterCount());
    for (ulong slot = 0; slot < count; ++ slot)
    {
        /* Get Adapter */
        CNetworkAdapter adapter = aMachine.GetNetworkAdapter (slot);

        /* Creating Adapter's page */
        VBoxVMSettingsNetwork *page = new VBoxVMSettingsNetwork (this, mDisableStaticControls);

        /* Loading Adapter's data into page */
        page->getFromAdapter (adapter);

        /* Attach Adapter's page to Tab Widget */
        mTwAdapters->addTab (page, page->pageTitle());

        /* Disable tab page if adapter is being configured dynamically */
        if (mDisableStaticControls && !adapter.GetEnabled())
            mTwAdapters->setTabEnabled(slot, false);

        /* Setup validation */
        page->setValidator (mValidator);

        /* Setup tab order */
        lastFocusWidget = page->setOrderAfter (lastFocusWidget);
    }

    /* Applying language settings */
    retranslateUi();
}

void VBoxVMSettingsNetworkPage::putBackTo()
{
    for (int i = 0; i < mTwAdapters->count(); ++ i)
    {
        VBoxVMSettingsNetwork *page =
            qobject_cast <VBoxVMSettingsNetwork*> (mTwAdapters->widget (i));
        Assert (page);
        page->putBackToAdapter();
    }
}

void VBoxVMSettingsNetworkPage::setValidator (QIWidgetValidator *aVal)
{
    mValidator = aVal;
}

bool VBoxVMSettingsNetworkPage::revalidate (QString &aWarning, QString &aTitle)
{
    bool valid = true;

    for (int i = 0; i < mTwAdapters->count(); ++ i)
    {
        VBoxVMSettingsNetwork *page =
            qobject_cast <VBoxVMSettingsNetwork*> (mTwAdapters->widget (i));
        Assert (page);
        valid = page->revalidate (aWarning, aTitle);
        if (!valid) break;
    }

    return valid;
}

void VBoxVMSettingsNetworkPage::retranslateUi()
{
    for (int i = 0; i < mTwAdapters->count(); ++ i)
    {
        VBoxVMSettingsNetwork *page =
            qobject_cast <VBoxVMSettingsNetwork*> (mTwAdapters->widget (i));
        Assert (page);
        mTwAdapters->setTabText (i, page->pageTitle());
    }
}

void VBoxVMSettingsNetworkPage::updatePages()
{
    for (int i = 0; i < mTwAdapters->count(); ++ i)
    {
        /* Get the iterated page */
        VBoxVMSettingsNetwork *page =
            qobject_cast <VBoxVMSettingsNetwork*> (mTwAdapters->widget (i));
        Assert (page);

        /* Update the page if the attachment type is 'internal network' */
        if (page->attachmentType() == KNetworkAttachmentType_Internal)
            QTimer::singleShot (0, page, SLOT (updateAttachmentAlternative()));
    }
}

