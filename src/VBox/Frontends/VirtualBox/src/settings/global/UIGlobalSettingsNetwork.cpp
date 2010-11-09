/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIGlobalSettingsNetwork class implementation
 */

/*
 * Copyright (C) 2009-2010 Oracle Corporation
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
#include "UIGlobalSettingsNetwork.h"
#include "UIGlobalSettingsNetworkDetails.h"
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"

/* Qt includes */
#include <QHeaderView>
#include <QHostAddress>

NetworkItem::NetworkItem()
    : QTreeWidgetItem()
{
}

void NetworkItem::fetchNetworkData(const UIHostNetworkData &data)
{
    /* Fetch from cache: */
    m_data = data;

    /* Update tool-tip: */
    updateInfo();
}

void NetworkItem::uploadNetworkData(UIHostNetworkData &data)
{
    /* Upload to cache: */
    data = m_data;
}

bool NetworkItem::revalidate (QString &aWarning, QString & /* aTitle */)
{
    /* Host-only Interface validation */
    if (!m_data.m_interface.m_fDhcpClientEnabled)
    {
        if (m_data.m_interface.m_strInterfaceAddress.isEmpty() &&
            (QHostAddress (m_data.m_interface.m_strInterfaceAddress) == QHostAddress::Any ||
             QHostAddress (m_data.m_interface.m_strInterfaceAddress).protocol() != QAbstractSocket::IPv4Protocol))
        {
            aWarning = UIGlobalSettingsNetwork::tr ("host IPv4 address of <b>%1</b> is wrong").arg (text (0));
            return false;
        }
        if (!m_data.m_interface.m_strInterfaceMask.isEmpty() &&
            (QHostAddress (m_data.m_interface.m_strInterfaceMask) == QHostAddress::Any ||
             QHostAddress (m_data.m_interface.m_strInterfaceMask).protocol() != QAbstractSocket::IPv4Protocol))
        {
            aWarning = UIGlobalSettingsNetwork::tr ("host IPv4 network mask of <b>%1</b> is wrong").arg (text (0));
            return false;
        }
        if (m_data.m_interface.m_fIpv6Supported)
        {
            if (!m_data.m_interface.m_strInterfaceAddress6.isEmpty() &&
                (QHostAddress (m_data.m_interface.m_strInterfaceAddress6) == QHostAddress::AnyIPv6 ||
                 QHostAddress (m_data.m_interface.m_strInterfaceAddress6).protocol() != QAbstractSocket::IPv6Protocol))
            {
                aWarning = UIGlobalSettingsNetwork::tr ("host IPv6 address of <b>%1</b> is wrong").arg (text (0));
                return false;
            }
        }
    }

    /* DHCP Server settings */
    if (m_data.m_dhcpserver.m_fDhcpServerEnabled)
    {
        if (QHostAddress (m_data.m_dhcpserver.m_strDhcpServerAddress) == QHostAddress::Any ||
            QHostAddress (m_data.m_dhcpserver.m_strDhcpServerAddress).protocol() != QAbstractSocket::IPv4Protocol)
        {
            aWarning = UIGlobalSettingsNetwork::tr ("DHCP server address of <b>%1</b> is wrong").arg (text (0));
            return false;
        }
        if (QHostAddress (m_data.m_dhcpserver.m_strDhcpServerMask) == QHostAddress::Any ||
            QHostAddress (m_data.m_dhcpserver.m_strDhcpServerMask).protocol() != QAbstractSocket::IPv4Protocol)
        {
            aWarning = UIGlobalSettingsNetwork::tr ("DHCP server network mask of <b>%1</b> is wrong").arg (text (0));
            return false;
        }
        if (QHostAddress (m_data.m_dhcpserver.m_strDhcpLowerAddress) == QHostAddress::Any ||
            QHostAddress (m_data.m_dhcpserver.m_strDhcpLowerAddress).protocol() != QAbstractSocket::IPv4Protocol)
        {
            aWarning = UIGlobalSettingsNetwork::tr ("DHCP lower address bound of <b>%1</b> is wrong").arg (text (0));
            return false;
        }
        if (QHostAddress (m_data.m_dhcpserver.m_strDhcpUpperAddress) == QHostAddress::Any ||
            QHostAddress (m_data.m_dhcpserver.m_strDhcpUpperAddress).protocol() != QAbstractSocket::IPv4Protocol)
        {
            aWarning = UIGlobalSettingsNetwork::tr ("DHCP upper address bound of <b>%1</b> is wrong").arg (text (0));
            return false;
        }
    }
    return true;
}

QString NetworkItem::updateInfo()
{
    /* Update text: */
    setText(0, m_data.m_interface.m_strName);

    /* Update information label */
    QString hdr ("<tr><td><nobr>%1:&nbsp;</nobr></td>"
                 "<td><nobr>%2</nobr></td></tr>");
    QString sub ("<tr><td><nobr>&nbsp;&nbsp;%1:&nbsp;</nobr></td>"
                 "<td><nobr>%2</nobr></td></tr>");
    QString data, tip, buffer;

    /* Host-only Interface information */
    buffer = hdr.arg (UIGlobalSettingsNetwork::tr ("Adapter"))
                .arg (m_data.m_interface.m_fDhcpClientEnabled ? UIGlobalSettingsNetwork::tr ("Automatically configured", "interface")
                                                              : UIGlobalSettingsNetwork::tr ("Manually configured", "interface"));
    data += buffer;
    tip += buffer;

    if (!m_data.m_interface.m_fDhcpClientEnabled)
    {
        buffer = sub.arg (UIGlobalSettingsNetwork::tr ("IPv4 Address"))
                    .arg (m_data.m_interface.m_strInterfaceAddress.isEmpty() ? UIGlobalSettingsNetwork::tr ("Not set", "address")
                                                                             : m_data.m_interface.m_strInterfaceAddress) +
                 sub.arg (UIGlobalSettingsNetwork::tr ("IPv4 Network Mask"))
                    .arg (m_data.m_interface.m_strInterfaceMask.isEmpty() ? UIGlobalSettingsNetwork::tr ("Not set", "mask")
                                                                          : m_data.m_interface.m_strInterfaceMask);
        tip += buffer;

        if (m_data.m_interface.m_fIpv6Supported)
        {
            buffer = sub.arg (UIGlobalSettingsNetwork::tr ("IPv6 Address"))
                        .arg (m_data.m_interface.m_strInterfaceAddress6.isEmpty() ? UIGlobalSettingsNetwork::tr ("Not set", "address")
                                                                                  : m_data.m_interface.m_strInterfaceAddress6) +
                     sub.arg (UIGlobalSettingsNetwork::tr ("IPv6 Network Mask Length"))
                        .arg (m_data.m_interface.m_strInterfaceMaskLength6.isEmpty() ? UIGlobalSettingsNetwork::tr ("Not set", "length")
                                                              : m_data.m_interface.m_strInterfaceMaskLength6);
            tip += buffer;
        }
    }

    /* DHCP Server information */
    buffer = hdr.arg (UIGlobalSettingsNetwork::tr ("DHCP Server"))
                .arg (m_data.m_dhcpserver.m_fDhcpServerEnabled ? UIGlobalSettingsNetwork::tr ("Enabled", "server")
                                                               : UIGlobalSettingsNetwork::tr ("Disabled", "server"));
    data += buffer;
    tip += buffer;

    if (m_data.m_dhcpserver.m_fDhcpServerEnabled)
    {
        buffer = sub.arg (UIGlobalSettingsNetwork::tr ("Address"))
                    .arg (m_data.m_dhcpserver.m_strDhcpServerAddress.isEmpty() ? UIGlobalSettingsNetwork::tr ("Not set", "address")
                                                                               : m_data.m_dhcpserver.m_strDhcpServerAddress) +
                 sub.arg (UIGlobalSettingsNetwork::tr ("Network Mask"))
                    .arg (m_data.m_dhcpserver.m_strDhcpServerMask.isEmpty() ? UIGlobalSettingsNetwork::tr ("Not set", "mask")
                                                                            : m_data.m_dhcpserver.m_strDhcpServerMask) +
                 sub.arg (UIGlobalSettingsNetwork::tr ("Lower Bound"))
                    .arg (m_data.m_dhcpserver.m_strDhcpLowerAddress.isEmpty() ? UIGlobalSettingsNetwork::tr ("Not set", "bound")
                                                                              : m_data.m_dhcpserver.m_strDhcpLowerAddress) +
                 sub.arg (UIGlobalSettingsNetwork::tr ("Upper Bound"))
                    .arg (m_data.m_dhcpserver.m_strDhcpUpperAddress.isEmpty() ? UIGlobalSettingsNetwork::tr ("Not set", "bound")
                                                                              : m_data.m_dhcpserver.m_strDhcpUpperAddress);
        tip += buffer;
    }

    setToolTip (0, tip);

    return QString ("<table>") + data + QString ("</table>");
}

UIGlobalSettingsNetwork::UIGlobalSettingsNetwork()
    : mValidator(0)
    , mAddInterface(0), mRemInterface(0), mEditInterface(0)
    , m_fChanged(false)
{
    /* Apply UI decorations */
    Ui::UIGlobalSettingsNetwork::setupUi (this);

#ifdef Q_WS_MAC
    /* Make shifting spacer for MAC as we have fixed-size networks list */
    QSpacerItem *shiftSpacer =
        new QSpacerItem (0, 1, QSizePolicy::Expanding, QSizePolicy::Preferred);
    QGridLayout *mainLayout = static_cast <QGridLayout*> (layout());
    mainLayout->addItem (shiftSpacer, 1, 4);
    //static_cast <QHBoxLayout*> (mWtActions->layout())->addStretch();
#endif

    /* Setup tree-widget */
    mTwInterfaces->header()->hide();
    mTwInterfaces->setContextMenuPolicy (Qt::CustomContextMenu);

    /* Prepare toolbar */
    mAddInterface = new QAction (mTwInterfaces);
    mRemInterface = new QAction (mTwInterfaces);
    mEditInterface = new QAction (mTwInterfaces);

    mAddInterface->setShortcuts (QList <QKeySequence> ()
                                 << QKeySequence ("Ins")
                                 << QKeySequence ("Ctrl+N"));
    mRemInterface->setShortcuts (QList <QKeySequence> ()
                                 << QKeySequence ("Del")
                                 << QKeySequence ("Ctrl+R"));
    mEditInterface->setShortcuts (QList <QKeySequence> ()
                                  << QKeySequence ("Space")
                                  << QKeySequence ("F2"));

    mAddInterface->setIcon(UIIconPool::iconSet(":/add_host_iface_16px.png",
                                               ":/add_host_iface_disabled_16px.png"));
    mRemInterface->setIcon(UIIconPool::iconSet(":/remove_host_iface_16px.png",
                                               ":/remove_host_iface_disabled_16px.png"));
    mEditInterface->setIcon(UIIconPool::iconSet(":/guesttools_16px.png",
                                                ":/guesttools_disabled_16px.png"));

    mTbActions->setUsesTextLabel (false);
    mTbActions->setIconSize (QSize (16, 16));
    mTbActions->setOrientation (Qt::Vertical);
    mTbActions->addAction (mAddInterface);
    mTbActions->addAction (mRemInterface);
    mTbActions->addAction (mEditInterface);
    mTbActions->setSizePolicy (QSizePolicy::Fixed, QSizePolicy::MinimumExpanding);
    mTbActions->updateGeometry();
    mTbActions->setMinimumHeight (mTbActions->sizeHint().height());

    /* Setup connections */
    connect (mAddInterface, SIGNAL (triggered (bool)), this, SLOT (addInterface()));
    connect (mRemInterface, SIGNAL (triggered (bool)), this, SLOT (remInterface()));
    connect (mEditInterface, SIGNAL (triggered (bool)), this, SLOT (editInterface()));
    connect (mTwInterfaces, SIGNAL (currentItemChanged (QTreeWidgetItem *, QTreeWidgetItem *)),
             this, SLOT (updateCurrentItem()));
    connect (mTwInterfaces, SIGNAL (customContextMenuRequested (const QPoint &)),
             this, SLOT (showContextMenu (const QPoint &)));
    connect (mTwInterfaces, SIGNAL (itemDoubleClicked (QTreeWidgetItem*, int)),
             this, SLOT (editInterface()));

    /* Applying language settings */
    retranslateUi();
}

/* Load data to cashe from corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void UIGlobalSettingsNetwork::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to properties & settings: */
    UISettingsPageGlobal::fetchData(data);

    /* Load to cache: */
    const CHostNetworkInterfaceVector &interfaces = vboxGlobal().virtualBox().GetHost().GetNetworkInterfaces();
    for (int iNetworkIndex = 0; iNetworkIndex < interfaces.size(); ++iNetworkIndex)
    {
        const CHostNetworkInterface &iface = interfaces[iNetworkIndex];
        if (iface.GetInterfaceType() == KHostNetworkInterfaceType_HostOnly)
        {
            /* Initialization: */
            CDHCPServer dhcp = vboxGlobal().virtualBox().FindDHCPServerByNetworkName(iface.GetNetworkName());
            if (dhcp.isNull()) vboxGlobal().virtualBox().CreateDHCPServer(iface.GetNetworkName());
            dhcp = vboxGlobal().virtualBox().FindDHCPServerByNetworkName(iface.GetNetworkName());
            AssertMsg(!dhcp.isNull(), ("DHCP Server creation failed!\n"));
            UIHostNetworkData data;
            /* Host-only interface settings */
            data.m_interface.m_strName = iface.GetName();
            data.m_interface.m_fDhcpClientEnabled = iface.GetDhcpEnabled();
            data.m_interface.m_strInterfaceAddress = iface.GetIPAddress();
            data.m_interface.m_strInterfaceMask = iface.GetNetworkMask();
            data.m_interface.m_fIpv6Supported = iface.GetIPV6Supported();
            data.m_interface.m_strInterfaceAddress6 = iface.GetIPV6Address();
            data.m_interface.m_strInterfaceMaskLength6 = QString::number(iface.GetIPV6NetworkMaskPrefixLength());
            /* DHCP server settings: */
            data.m_dhcpserver.m_fDhcpServerEnabled = dhcp.GetEnabled();
            data.m_dhcpserver.m_strDhcpServerAddress = dhcp.GetIPAddress();
            data.m_dhcpserver.m_strDhcpServerMask = dhcp.GetNetworkMask();
            data.m_dhcpserver.m_strDhcpLowerAddress = dhcp.GetLowerIP();
            data.m_dhcpserver.m_strDhcpUpperAddress = dhcp.GetUpperIP();
            /* Cache: */
            m_cache.m_items << data;
        }
    }

    /* Upload properties & settings to data: */
    UISettingsPageGlobal::uploadData(data);
}

/* Load data to corresponding widgets from cache,
 * this task SHOULD be performed in GUI thread only: */
void UIGlobalSettingsNetwork::getFromCache()
{
    /* Fetch from cache: */
    for (int iNetworkIndex = 0; iNetworkIndex < m_cache.m_items.size(); ++iNetworkIndex)
    {
        const UIHostNetworkData &data = m_cache.m_items[iNetworkIndex];
        NetworkItem *pItem = new NetworkItem;
        pItem->fetchNetworkData(data);
        mTwInterfaces->addTopLevelItem(pItem);
    }
    mTwInterfaces->setCurrentItem(mTwInterfaces->topLevelItem(0));
    updateCurrentItem();
#ifdef Q_WS_MAC
    int width = qMax(static_cast<QAbstractItemView*>(mTwInterfaces)->sizeHintForColumn(0) +
                     2 * mTwInterfaces->frameWidth() + QApplication::style()->pixelMetric(QStyle::PM_ScrollBarExtent),
                     220);
    mTwInterfaces->setFixedWidth(width);
    mTwInterfaces->resizeColumnToContents(0);
#endif /* Q_WS_MAC */
}

/* Save data from corresponding widgets to cache,
 * this task SHOULD be performed in GUI thread only: */
void UIGlobalSettingsNetwork::putToCache()
{
    /* Upload to cache: */
    m_cache.m_items.clear();
    for (int iNetworkIndex = 0; iNetworkIndex < mTwInterfaces->topLevelItemCount(); ++iNetworkIndex)
    {
        UIHostNetworkData data;
        NetworkItem *pItem = static_cast<NetworkItem*>(mTwInterfaces->topLevelItem(iNetworkIndex));
        pItem->uploadNetworkData(data);
        m_cache.m_items << data;
    }
}

/* Save data from cache to corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void UIGlobalSettingsNetwork::saveFromCacheTo(QVariant &data)
{
    /* Ensure settings were changed: */
    if (!m_fChanged)
        return;

    /* Fetch data to properties & settings: */
    UISettingsPageGlobal::fetchData(data);

    /* Save from cache: */
    CVirtualBox vbox = vboxGlobal().virtualBox();
    CHost host = vbox.GetHost();
    const CHostNetworkInterfaceVector &interfaces = host.GetNetworkInterfaces();
    /* Remove all the old interfaces first: */
    for (int iNetworkIndex = 0; iNetworkIndex < interfaces.size(); ++iNetworkIndex)
    {
        /* Get iterated interface: */
        const CHostNetworkInterface &iface = interfaces[iNetworkIndex];
        if (iface.GetInterfaceType() == KHostNetworkInterfaceType_HostOnly)
        {
            /* Search for this interface's dhcp sserver: */
            CDHCPServer dhcp = vboxGlobal().virtualBox().FindDHCPServerByNetworkName(iface.GetNetworkName());
            /* Delete it if its present: */
            if (!dhcp.isNull())
                vbox.RemoveDHCPServer(dhcp);
            /* Delete interface finally: */
            CProgress progress = host.RemoveHostOnlyNetworkInterface(iface.GetId());
            if (host.isOk())
            {
                progress.WaitForCompletion(-1);
                // TODO: Fix problem reporter!
                //vboxProblem().showModalProgressDialog(progress, tr("Performing", "creating/removing host-only network"), this);
                if (progress.GetResultCode() != 0)
                    // TODO: Fix problem reporter!
                    //vboxProblem().cannotRemoveHostInterface(progress, iface, this);
                    AssertMsgFailed(("Failed to remove Host-only Network Adapter, result code is %d!\n", progress.GetResultCode()));
            }
            else
                // TODO: Fix problem reporter!
                //vboxProblem().cannotRemoveHostInterface(host, iface, this);
                AssertMsgFailed(("Failed to remove Host-only Network Adapter!\n"));
        }
    }
    /* Add all the new interfaces finally: */
    for (int iNetworkIndex = 0; iNetworkIndex < m_cache.m_items.size(); ++iNetworkIndex)
    {
        /* Get iterated data: */
        const UIHostNetworkData &data = m_cache.m_items[iNetworkIndex];
        CHostNetworkInterface iface;
        /* Create interface: */
        CProgress progress = host.CreateHostOnlyNetworkInterface(iface);
        if (host.isOk())
        {
            // TODO: Fix problem reporter!
            //vboxProblem().showModalProgressDialog(progress, tr("Performing", "creating/removing host-only network"), this);
            progress.WaitForCompletion(-1);
            if (progress.GetResultCode() == 0)
            {
                /* Create DHCP server: */
                CDHCPServer dhcp = vbox.FindDHCPServerByNetworkName(iface.GetNetworkName());
                if (dhcp.isNull()) vbox.CreateDHCPServer(iface.GetNetworkName());
                dhcp = vbox.FindDHCPServerByNetworkName(iface.GetNetworkName());
                AssertMsg(!dhcp.isNull(), ("DHCP Server creation failed!\n"));
                /* Host-only Interface configuring: */
                if (data.m_interface.m_fDhcpClientEnabled)
                {
                    iface.EnableDynamicIpConfig();
                }
                else
                {
                    AssertMsg(data.m_interface.m_strInterfaceAddress.isEmpty() ||
                              QHostAddress(data.m_interface.m_strInterfaceAddress).protocol() == QAbstractSocket::IPv4Protocol,
                              ("Interface IPv4 address must be empty or IPv4-valid!\n"));
                    AssertMsg(data.m_interface.m_strInterfaceMask.isEmpty() ||
                              QHostAddress(data.m_interface.m_strInterfaceMask).protocol() == QAbstractSocket::IPv4Protocol,
                              ("Interface IPv4 network mask must be empty or IPv4-valid!\n"));
                    iface.EnableStaticIpConfig(data.m_interface.m_strInterfaceAddress, data.m_interface.m_strInterfaceMask);
                    if (iface.GetIPV6Supported())
                    {
                        AssertMsg(data.m_interface.m_strInterfaceAddress6.isEmpty() ||
                                  QHostAddress(data.m_interface.m_strInterfaceAddress6).protocol() == QAbstractSocket::IPv6Protocol,
                                  ("Interface IPv6 address must be empty or IPv6-valid!\n"));
                        iface.EnableStaticIpConfigV6(data.m_interface.m_strInterfaceAddress6, data.m_interface.m_strInterfaceMaskLength6.toULong());
                    }
                }
                /* DHCP Server configuring: */
                dhcp.SetEnabled(data.m_dhcpserver.m_fDhcpServerEnabled);
//                AssertMsg(QHostAddress(data.m_dhcpserver.m_strDhcpServerAddress).protocol() == QAbstractSocket::IPv4Protocol,
//                          ("DHCP Server IPv4 address must be IPv4-valid!\n"));
//                AssertMsg(QHostAddress(data.m_dhcpserver.m_strDhcpServerMask).protocol() == QAbstractSocket::IPv4Protocol,
//                          ("DHCP Server IPv4 network mask must be IPv4-valid!\n"));
//                AssertMsg(QHostAddress(data.m_dhcpserver.m_strDhcpLowerAddress).protocol() == QAbstractSocket::IPv4Protocol,
//                          ("DHCP Server IPv4 lower bound must be IPv4-valid!\n"));
//                AssertMsg(QHostAddress(data.m_dhcpserver.m_strDhcpUpperAddress).protocol() == QAbstractSocket::IPv4Protocol,
//                          ("DHCP Server IPv4 upper bound must be IPv4-valid!\n"));
                if (QHostAddress(data.m_dhcpserver.m_strDhcpServerAddress).protocol() == QAbstractSocket::IPv4Protocol &&
                    QHostAddress(data.m_dhcpserver.m_strDhcpServerMask).protocol() == QAbstractSocket::IPv4Protocol &&
                    QHostAddress(data.m_dhcpserver.m_strDhcpLowerAddress).protocol() == QAbstractSocket::IPv4Protocol &&
                    QHostAddress(data.m_dhcpserver.m_strDhcpUpperAddress).protocol() == QAbstractSocket::IPv4Protocol)
                    dhcp.SetConfiguration(data.m_dhcpserver.m_strDhcpServerAddress, data.m_dhcpserver.m_strDhcpServerMask,
                                          data.m_dhcpserver.m_strDhcpLowerAddress, data.m_dhcpserver.m_strDhcpUpperAddress);
            }
            else
                // TODO: Fix problem reporter!
                //vboxProblem().cannotCreateHostInterface(progress, this);
                AssertMsgFailed(("Failed to create Host-only Network Adapter, result code is %d!\n", progress.GetResultCode()));
        }
        else
            // TODO: Fix problem reporter!
            //vboxProblem().cannotCreateHostInterface(host, this);
            AssertMsgFailed(("Failed to create Host-only Network Adapter!\n"));
    }

    /* Upload properties & settings to data: */
    UISettingsPageGlobal::uploadData(data);
}

void UIGlobalSettingsNetwork::setValidator (QIWidgetValidator *aValidator)
{
    mValidator = aValidator;
}

bool UIGlobalSettingsNetwork::revalidate (QString &aWarning, QString &aTitle)
{
    NetworkItem *item = static_cast <NetworkItem*> (mTwInterfaces->currentItem());
    return item ? item->revalidate (aWarning, aTitle) : true;
}

void UIGlobalSettingsNetwork::setOrderAfter (QWidget *aWidget)
{
    setTabOrder (aWidget, mTwInterfaces);
}

void UIGlobalSettingsNetwork::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::UIGlobalSettingsNetwork::retranslateUi (this);

    /* Translate action tool-tips */
    mAddInterface->setText (tr ("&Add host-only network"));
    mRemInterface->setText (tr ("&Remove host-only network"));
    mEditInterface->setText (tr ("&Edit host-only network"));

    mAddInterface->setToolTip (mAddInterface->text().remove ('&') +
        QString (" (%1)").arg (mAddInterface->shortcut().toString()));
    mRemInterface->setToolTip (mRemInterface->text().remove ('&') +
        QString (" (%1)").arg (mRemInterface->shortcut().toString()));
    mEditInterface->setToolTip (mEditInterface->text().remove ('&') +
        QString (" (%1)").arg (mEditInterface->shortcut().toString()));
}

void UIGlobalSettingsNetwork::addInterface()
{
    /* Creating interface item: */
    NetworkItem *pItem = new NetworkItem;
    /* Fill item's data: */
    UIHostNetworkData data;
    /* Interface data: */
    // TODO: Make unique name!
    data.m_interface.m_strName = tr("New Host-Only Interface");
    data.m_interface.m_fDhcpClientEnabled = true;
    data.m_interface.m_fIpv6Supported = false;
    /* DHCP data: */
    data.m_dhcpserver.m_fDhcpServerEnabled = false;
    /* Fetch item with data: */
    pItem->fetchNetworkData(data);
    /* Add new top-level item: */
    mTwInterfaces->addTopLevelItem(pItem);
    mTwInterfaces->sortItems(0, Qt::AscendingOrder);
    mTwInterfaces->setCurrentItem(pItem);
    /* Mark dialog as edited: */
    m_fChanged = true;
}

void UIGlobalSettingsNetwork::remInterface()
{
    /* Get interface item: */
    NetworkItem *pItem = static_cast<NetworkItem*>(mTwInterfaces->currentItem());
    AssertMsg(pItem, ("Current item should present!\n"));
    /* Get interface name: */
    QString strInterfaceName(pItem->name());
    /* Asking user about deleting selected network interface: */
    if (vboxProblem().confirmDeletingHostInterface(strInterfaceName, this) == QIMessageBox::Cancel)
        return;
    /* Removing interface: */
    delete pItem;
    /* Mark dialog as edited: */
    m_fChanged = true;
}

void UIGlobalSettingsNetwork::editInterface()
{
    /* Check interface presence */
    NetworkItem *item = static_cast <NetworkItem*> (mTwInterfaces->currentItem());
    AssertMsg (item, ("Current item should be selected!\n"));
    /* Edit current item data */
    UIGlobalSettingsNetworkDetails details (this);
    details.getFromItem (item);
    if (details.exec() == QDialog::Accepted)
    {
        details.putBackToItem();
        item->updateInfo();
    }
    updateCurrentItem();
    mValidator->revalidate();
    /* Mark dialog as edited: */
    m_fChanged = true;
}

void UIGlobalSettingsNetwork::updateCurrentItem()
{
    /* Get current item */
    NetworkItem *item = static_cast <NetworkItem*> (mTwInterfaces->currentItem());
    /* Set the final label text */
    mLbInfo->setText (item ? item->updateInfo() : QString());
    /* Update availability */
    mRemInterface->setEnabled (item);
    mEditInterface->setEnabled (item);
}

void UIGlobalSettingsNetwork::showContextMenu (const QPoint &aPos)
{
    QMenu menu;

    if (mTwInterfaces->itemAt (aPos))
    {
        menu.addAction (mEditInterface);
        menu.addAction (mRemInterface);
    }
    else
    {
        menu.addAction (mAddInterface);
    }

    menu.exec (mTwInterfaces->mapToGlobal (aPos));
}

