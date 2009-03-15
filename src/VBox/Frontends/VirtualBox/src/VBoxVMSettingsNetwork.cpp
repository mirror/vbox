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
#include "VBoxGlobal.h"
#include "VBoxVMSettingsNetwork.h"
#include "VBoxVMSettingsNetworkDetails.h"

/* VBoxVMSettingsNetwork Stuff */
VBoxVMSettingsNetwork::VBoxVMSettingsNetwork (VBoxVMSettingsNetworkPage *aParent)
    : QIWithRetranslateUI <QWidget> (0)
    , mParent (aParent)
    , mDetails (new VBoxVMSettingsNetworkDetails (this))
    , mValidator (0)
{
    /* Apply UI decorations */
    Ui::VBoxVMSettingsNetwork::setupUi (this);

    /* Setup widgets */
    mTbDetails->setIcon (VBoxGlobal::iconSet (
        ":/global_settings_16px.png", ":/global_settings_disabled_16px.png"));

    /* Applying language settings */
    retranslateUi();

#ifdef Q_WS_MAC
    /* Prevent this widgets to go in the Small/Mini size state which is
     * available on Mac OS X. Not sure why this happens but this seems to help
     * against. */
    QList <QWidget*> list = findChildren <QWidget*>();
    foreach (QWidget *w, list)
        if (w->parent() == this)
            w->setFixedHeight (w->sizeHint().height());

    /* Remove tool-button border at MAC */
    mTbDetails->setStyleSheet ("QToolButton {border: 0px none black;}");
#endif /* Q_WS_MAC */
}

void VBoxVMSettingsNetwork::getFromAdapter (const CNetworkAdapter &aAdapter)
{
    mAdapter = aAdapter;

    /* Load adapter state */
    mGbAdapter->setChecked (aAdapter.GetEnabled());

    /* Load adapter type */
    int adapterPos = mCbAdapterType->findData (aAdapter.GetAdapterType());
    mCbAdapterType->setCurrentIndex (adapterPos == -1 ? 0 : adapterPos);

    /* Load network attachment type */
    int attachmentPos = mCbAttachmentType->findData (aAdapter.GetAttachmentType());
    mCbAttachmentType->setCurrentIndex (attachmentPos == -1 ? 0 : attachmentPos);

    /* Load details page */
    mDetails->getFromAdapter (aAdapter);
    updateAttachmentInfo();
}

void VBoxVMSettingsNetwork::putBackToAdapter()
{
    /* Save adapter state */
    mAdapter.SetEnabled (mGbAdapter->isChecked());

    /* Save adapter type */
    KNetworkAdapterType type = (KNetworkAdapterType)
        mCbAdapterType->itemData (mCbAdapterType->currentIndex()).toInt();
    mAdapter.SetAdapterType (type);

    /* Save network attachment type */
    switch (attachmentType())
    {
        case KNetworkAttachmentType_Null:
            mAdapter.Detach();
            break;
        case KNetworkAttachmentType_NAT:
            mAdapter.AttachToNAT();
            break;
        case KNetworkAttachmentType_Bridged:
            mAdapter.AttachToBridgedInterface();
            break;
        case KNetworkAttachmentType_Internal:
            mAdapter.AttachToInternalNetwork();
            break;
        case KNetworkAttachmentType_HostOnly:
            mAdapter.AttachToHostOnlyInterface();
            break;
        default:
            AssertMsgFailed (("Invalid network attachment type: (%d).", attachmentType()));
    }

    /* Save details page */
    mDetails->putBackToAdapter();
}

void VBoxVMSettingsNetwork::setValidator (QIWidgetValidator *aValidator)
{
    mValidator = aValidator;

    connect (mGbAdapter, SIGNAL (toggled (bool)),
             mValidator, SLOT (revalidate()));
    connect (mCbAttachmentType, SIGNAL (activated (const QString&)),
             this, SLOT (updateAttachmentInfo()));
    connect (mTbDetails, SIGNAL (clicked (bool)),
             this, SLOT (detailsClicked()));

    mValidator->revalidate();
}

bool VBoxVMSettingsNetwork::revalidate (QString &aWarning, QString &aTitle)
{
    bool valid = mGbAdapter->isChecked() ?
                 mDetails->revalidate (attachmentType(), aWarning) : true;

    if (!valid)
        aTitle += ": " + vboxGlobal().removeAccelMark (pageTitle());

    return valid;
}

QWidget* VBoxVMSettingsNetwork::setOrderAfter (QWidget *aAfter)
{
    setTabOrder (aAfter, mGbAdapter);
    setTabOrder (mGbAdapter, mCbAdapterType);
    setTabOrder (mCbAdapterType, mCbAttachmentType);
    return mCbAttachmentType;
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

QString VBoxVMSettingsNetwork::currentName (KNetworkAttachmentType aType) const
{
    return mDetails->currentName (aType);
}

void VBoxVMSettingsNetwork::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxVMSettingsNetwork::retranslateUi (this);

    /* Translate combo-boxes content */
    populateComboboxes();

    /* Translate attachment info */
    updateAttachmentInfo();
}

void VBoxVMSettingsNetwork::updateAttachmentInfo()
{
    KNetworkAttachmentType type = attachmentType();
    QString line ("<tr><td><i><b><nobr><font color=grey>%1:&nbsp;</font></nobr></b></i></td>"
                  "<td><i><font color=grey>%2</font></i></td></tr>");
    switch (type)
    {
        case KNetworkAttachmentType_Bridged:
        {
            QString info;
            QString name (mDetails->currentName (type));
            info += line.arg (tr ("Adapter"))
                        .arg (name.isEmpty() ? tr ("Not Selected") : name);
            mLbInfo->setText ("<table>" + info + "</table>");
            break;
        }
        case KNetworkAttachmentType_Internal:
        {
            QString info;
            QString name (mDetails->currentName (type));
            info += line.arg (tr ("Name"))
                        .arg (name.isEmpty() ? tr ("Not Selected") : name);
            mLbInfo->setText ("<table>" + info + "</table>");
            break;
        }
        case KNetworkAttachmentType_HostOnly:
        {
            QString info;
            QString name (mDetails->currentName (type));
            info += line.arg (tr ("Interface"))
                        .arg (name.isEmpty() ? tr ("Not Selected") : name);
            if (!name.isEmpty())
            {
                bool dhcp = mDetails->property ("HOI_DhcpEnabled").toBool();
                info += line.arg (tr ("Configuration"))
                            .arg (dhcp ? tr ("Automatic", "configuration")
                                       : tr ("Manual", "configuration"));
                if (!dhcp)
                {
                    QString ipv4addr (mDetails->property ("HOI_IPv4Addr").toString());
                    QString ipv4mask (mDetails->property ("HOI_IPv4Mask").toString());
                    info += line.arg (tr ("IPv4 Address")).arg (ipv4addr) +
                            line.arg (tr ("IPv4 Mask")).arg (ipv4mask);
                    bool ipv6 = mDetails->property ("HOI_IPv6Supported").toBool();
                    if (ipv6)
                    {
                        QString ipv6addr (mDetails->property ("HOI_IPv6Addr").toString());
                        QString ipv6mask (mDetails->property ("HOI_IPv6Mask").toString());
                        info += line.arg (tr ("IPv6 Address")).arg (ipv6addr) +
                                line.arg (tr ("IPv6 Mask")).arg (ipv6mask);
                    }
                }
            }
            mLbInfo->setText ("<table>" + info + "</table>");
            break;
        }
        default:
            mLbInfo->clear();
            break;
    }

    if (mValidator)
        mValidator->revalidate();
}

void VBoxVMSettingsNetwork::detailsClicked()
{
    /* Reload alternate list */
    KNetworkAttachmentType type = attachmentType();
    QStringList list;
    switch (type)
    {
        case KNetworkAttachmentType_Bridged:
            list = mParent->intList (KHostNetworkInterfaceType_Bridged);
            break;
        case KNetworkAttachmentType_Internal:
            list = mParent->netList();
            break;
        case KNetworkAttachmentType_HostOnly:
            list = mParent->intList (KHostNetworkInterfaceType_HostOnly);
            break;
        default:
            break;
    }
    mDetails->loadList (type, list);
    mDetails->activateWindow();
    mDetails->exec();
    updateAttachmentInfo();
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

KNetworkAttachmentType VBoxVMSettingsNetwork::attachmentType() const
{
    return (KNetworkAttachmentType) mCbAttachmentType->itemData (
           mCbAttachmentType->currentIndex()).toInt();
}

/* VBoxVMSettingsNetworkPage Stuff */
VBoxVMSettingsNetworkPage::VBoxVMSettingsNetworkPage()
    : mValidator (0)
{
    /* Setup Main Layout */
    QVBoxLayout *mainLayout = new QVBoxLayout (this);
    mainLayout->setContentsMargins (0, 5, 0, 5);

    /* Creating Tab Widget */
    mTwAdapters = new QTabWidget (this);
    mainLayout->addWidget (mTwAdapters);
}

QStringList VBoxVMSettingsNetworkPage::netList() const
{
    QStringList list;

    /* Load total network list of all VMs */
    CVirtualBox vbox = vboxGlobal().virtualBox();
    ulong count = qMin ((ULONG) 4, vbox.GetSystemProperties().GetNetworkAdapterCount());
    CMachineVector vec =  vbox.GetMachines();
    for (CMachineVector::ConstIterator m = vec.begin(); m != vec.end(); ++ m)
    {
        if (m->GetAccessible())
        {
            for (ulong slot = 0; slot < count; ++ slot)
            {
                QString name = m->GetNetworkAdapter (slot).GetInternalNetwork();
                if (!name.isEmpty() && !list.contains (name))
                    list << name;
            }
        }
    }

    /* Append network list with names from all the pages */
    for (int index = 0; index < mTwAdapters->count(); ++ index)
    {
        VBoxVMSettingsNetwork *page =
            qobject_cast <VBoxVMSettingsNetwork*> (mTwAdapters->widget (index));
        if (page)
        {
            QString name = page->currentName (KNetworkAttachmentType_Internal);
            if (!name.isEmpty() && !list.contains (name))
                list << name;
        }
    }

    return list;
}

QStringList VBoxVMSettingsNetworkPage::intList (KHostNetworkInterfaceType aType) const
{
    QStringList list;

    /* Load total interfaces list */
    CHostNetworkInterfaceVector interfaces =
        vboxGlobal().virtualBox().GetHost().GetNetworkInterfaces();
    for (CHostNetworkInterfaceVector::ConstIterator it = interfaces.begin();
         it != interfaces.end(); ++ it)
    {
        if (it->GetInterfaceType() == aType)
            list << it->GetName();
    }

    return list;
}

void VBoxVMSettingsNetworkPage::getFrom (const CMachine &aMachine)
{
    /* Setup tab order */
    Assert (mFirstWidget);
    setTabOrder (mFirstWidget, mTwAdapters->focusProxy());
    QWidget *lastFocusWidget = mTwAdapters->focusProxy();

    /* Creating Tab Pages */
    CVirtualBox vbox = vboxGlobal().virtualBox();
    ulong count = qMin ((ULONG) 4, vbox.GetSystemProperties().GetNetworkAdapterCount());
    for (ulong slot = 0; slot < count; ++ slot)
    {
        /* Get Adapter */
        CNetworkAdapter adapter = aMachine.GetNetworkAdapter (slot);

        /* Creating Adapter's page */
        VBoxVMSettingsNetwork *page = new VBoxVMSettingsNetwork (this);

        /* Loading Adapter's data into page */
        page->getFromAdapter (adapter);

        /* Attach Adapter's page to Tab Widget */
        mTwAdapters->addTab (page, page->pageTitle());

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

