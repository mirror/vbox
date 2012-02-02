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

/* Global includes */
#include <QHeaderView>
#include <QHostAddress>

/* Local includes */
#include "QIWidgetValidator.h"
#include "UIIconPool.h"
#include "UIGlobalSettingsNetwork.h"
#include "UIGlobalSettingsNetworkDetails.h"
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"

/* Host-network item constructor: */
UIHostInterfaceItem::UIHostInterfaceItem()
    : QTreeWidgetItem()
{
}

/* Get data to item: */
void UIHostInterfaceItem::fetchNetworkData(const UIHostNetworkData &data)
{
    /* Fetch from cache: */
    m_data = data;

    /* Update tool-tip: */
    updateInfo();
}

/* Return data from item: */
void UIHostInterfaceItem::uploadNetworkData(UIHostNetworkData &data)
{
    /* Upload to cache: */
    data = m_data;
}

/* Validation stuff: */
bool UIHostInterfaceItem::revalidate(QString &strWarning, QString & /* strTitle */)
{
    /* Host-only interface validation: */
    if (!m_data.m_interface.m_fDhcpClientEnabled)
    {
        if (m_data.m_interface.m_strInterfaceAddress.isEmpty() &&
            (QHostAddress(m_data.m_interface.m_strInterfaceAddress) == QHostAddress::Any ||
             QHostAddress(m_data.m_interface.m_strInterfaceAddress).protocol() != QAbstractSocket::IPv4Protocol))
        {
            strWarning = UIGlobalSettingsNetwork::tr("host IPv4 address of <b>%1</b> is wrong").arg(text(0));
            return false;
        }
        if (!m_data.m_interface.m_strInterfaceMask.isEmpty() &&
            (QHostAddress(m_data.m_interface.m_strInterfaceMask) == QHostAddress::Any ||
             QHostAddress(m_data.m_interface.m_strInterfaceMask).protocol() != QAbstractSocket::IPv4Protocol))
        {
            strWarning = UIGlobalSettingsNetwork::tr("host IPv4 network mask of <b>%1</b> is wrong").arg(text(0));
            return false;
        }
        if (m_data.m_interface.m_fIpv6Supported)
        {
            if (!m_data.m_interface.m_strInterfaceAddress6.isEmpty() &&
                (QHostAddress(m_data.m_interface.m_strInterfaceAddress6) == QHostAddress::AnyIPv6 ||
                 QHostAddress(m_data.m_interface.m_strInterfaceAddress6).protocol() != QAbstractSocket::IPv6Protocol))
            {
                strWarning = UIGlobalSettingsNetwork::tr("host IPv6 address of <b>%1</b> is wrong").arg(text(0));
                return false;
            }
        }
    }

    /* DHCP server validation: */
    if (m_data.m_dhcpserver.m_fDhcpServerEnabled)
    {
        if (QHostAddress(m_data.m_dhcpserver.m_strDhcpServerAddress) == QHostAddress::Any ||
            QHostAddress(m_data.m_dhcpserver.m_strDhcpServerAddress).protocol() != QAbstractSocket::IPv4Protocol)
        {
            strWarning = UIGlobalSettingsNetwork::tr("DHCP server address of <b>%1</b> is wrong").arg(text(0));
            return false;
        }
        if (QHostAddress(m_data.m_dhcpserver.m_strDhcpServerMask) == QHostAddress::Any ||
            QHostAddress(m_data.m_dhcpserver.m_strDhcpServerMask).protocol() != QAbstractSocket::IPv4Protocol)
        {
            strWarning = UIGlobalSettingsNetwork::tr("DHCP server network mask of <b>%1</b> is wrong").arg(text(0));
            return false;
        }
        if (QHostAddress(m_data.m_dhcpserver.m_strDhcpLowerAddress) == QHostAddress::Any ||
            QHostAddress(m_data.m_dhcpserver.m_strDhcpLowerAddress).protocol() != QAbstractSocket::IPv4Protocol)
        {
            strWarning = UIGlobalSettingsNetwork::tr("DHCP lower address bound of <b>%1</b> is wrong").arg(text(0));
            return false;
        }
        if (QHostAddress(m_data.m_dhcpserver.m_strDhcpUpperAddress) == QHostAddress::Any ||
            QHostAddress(m_data.m_dhcpserver.m_strDhcpUpperAddress).protocol() != QAbstractSocket::IPv4Protocol)
        {
            strWarning = UIGlobalSettingsNetwork::tr("DHCP upper address bound of <b>%1</b> is wrong").arg(text(0));
            return false;
        }
    }
    return true;
}

QString UIHostInterfaceItem::updateInfo()
{
    /* Update text: */
    setText(0, m_data.m_interface.m_strName);

    /* Update information label: */
    QString strHeader("<tr><td><nobr>%1:&nbsp;</nobr></td><td><nobr>%2</nobr></td></tr>");
    QString strSubHeader("<tr><td><nobr>&nbsp;&nbsp;%1:&nbsp;</nobr></td><td><nobr>%2</nobr></td></tr>");
    QString strData, strToolTip, strBuffer;

    /* Host-only interface information: */
    strBuffer = strHeader.arg(UIGlobalSettingsNetwork::tr("Adapter"))
                .arg(m_data.m_interface.m_fDhcpClientEnabled ? UIGlobalSettingsNetwork::tr("Automatically configured", "interface")
                                                             : UIGlobalSettingsNetwork::tr("Manually configured", "interface"));
    strData += strBuffer;
    strToolTip += strBuffer;
    if (!m_data.m_interface.m_fDhcpClientEnabled)
    {
        strBuffer = strSubHeader.arg(UIGlobalSettingsNetwork::tr("IPv4 Address"))
                                .arg(m_data.m_interface.m_strInterfaceAddress.isEmpty() ?
                                     UIGlobalSettingsNetwork::tr ("Not set", "address") :
                                     m_data.m_interface.m_strInterfaceAddress) +
                    strSubHeader.arg(UIGlobalSettingsNetwork::tr("IPv4 Network Mask"))
                                .arg(m_data.m_interface.m_strInterfaceMask.isEmpty() ?
                                     UIGlobalSettingsNetwork::tr ("Not set", "mask") :
                                     m_data.m_interface.m_strInterfaceMask);
        strToolTip += strBuffer;
        if (m_data.m_interface.m_fIpv6Supported)
        {
            strBuffer = strSubHeader.arg(UIGlobalSettingsNetwork::tr("IPv6 Address"))
                                    .arg(m_data.m_interface.m_strInterfaceAddress6.isEmpty() ?
                                         UIGlobalSettingsNetwork::tr("Not set", "address") :
                                         m_data.m_interface.m_strInterfaceAddress6) +
                        strSubHeader.arg(UIGlobalSettingsNetwork::tr("IPv6 Network Mask Length"))
                                    .arg(m_data.m_interface.m_strInterfaceMaskLength6.isEmpty() ?
                                         UIGlobalSettingsNetwork::tr("Not set", "length") :
                                         m_data.m_interface.m_strInterfaceMaskLength6);
            strToolTip += strBuffer;
        }
    }

    /* DHCP server information: */
    strBuffer = strHeader.arg(UIGlobalSettingsNetwork::tr("DHCP Server"))
                         .arg(m_data.m_dhcpserver.m_fDhcpServerEnabled ?
                              UIGlobalSettingsNetwork::tr("Enabled", "server") :
                              UIGlobalSettingsNetwork::tr("Disabled", "server"));
    strData += strBuffer;
    strToolTip += strBuffer;
    if (m_data.m_dhcpserver.m_fDhcpServerEnabled)
    {
        strBuffer = strSubHeader.arg(UIGlobalSettingsNetwork::tr("Address"))
                                .arg(m_data.m_dhcpserver.m_strDhcpServerAddress.isEmpty() ?
                                     UIGlobalSettingsNetwork::tr("Not set", "address") :
                                     m_data.m_dhcpserver.m_strDhcpServerAddress) +
                    strSubHeader.arg(UIGlobalSettingsNetwork::tr("Network Mask"))
                                .arg(m_data.m_dhcpserver.m_strDhcpServerMask.isEmpty() ?
                                     UIGlobalSettingsNetwork::tr("Not set", "mask") :
                                     m_data.m_dhcpserver.m_strDhcpServerMask) +
                    strSubHeader.arg(UIGlobalSettingsNetwork::tr("Lower Bound"))
                                .arg(m_data.m_dhcpserver.m_strDhcpLowerAddress.isEmpty() ?
                                     UIGlobalSettingsNetwork::tr("Not set", "bound") :
                                     m_data.m_dhcpserver.m_strDhcpLowerAddress) +
                    strSubHeader.arg(UIGlobalSettingsNetwork::tr("Upper Bound"))
                                .arg(m_data.m_dhcpserver.m_strDhcpUpperAddress.isEmpty() ?
                                     UIGlobalSettingsNetwork::tr("Not set", "bound") :
                                     m_data.m_dhcpserver.m_strDhcpUpperAddress);
        strToolTip += strBuffer;
    }

    setToolTip(0, strToolTip);

    return QString("<table>") + strData + QString("</table>");
}

/* Network page constructor: */
UIGlobalSettingsNetwork::UIGlobalSettingsNetwork()
    : m_pValidator(0)
    , m_pAddAction(0), m_pDelAction(0), m_pEditAction(0)
    , m_fChanged(false)
{
    /* Apply UI decorations: */
    Ui::UIGlobalSettingsNetwork::setupUi (this);

    /* Setup tree-widget: */
    m_pInterfacesTree->header()->hide();
    m_pInterfacesTree->setContextMenuPolicy(Qt::CustomContextMenu);

    /* Prepare toolbar: */
    m_pAddAction = new QAction(m_pInterfacesTree);
    m_pDelAction = new QAction(m_pInterfacesTree);
    m_pEditAction = new QAction(m_pInterfacesTree);

    m_pAddAction->setShortcuts(QList<QKeySequence>() << QKeySequence("Ins") << QKeySequence("Ctrl+N"));
    m_pDelAction->setShortcuts(QList<QKeySequence>() << QKeySequence("Del") << QKeySequence("Ctrl+R"));
    m_pEditAction->setShortcuts(QList<QKeySequence>() << QKeySequence("Space") << QKeySequence("F2"));

    m_pAddAction->setIcon(UIIconPool::iconSet(":/add_host_iface_16px.png",
                                              ":/add_host_iface_disabled_16px.png"));
    m_pDelAction->setIcon(UIIconPool::iconSet(":/remove_host_iface_16px.png",
                                              ":/remove_host_iface_disabled_16px.png"));
    m_pEditAction->setIcon(UIIconPool::iconSet(":/guesttools_16px.png",
                                               ":/guesttools_disabled_16px.png"));

    m_pActionsToolbar->setUsesTextLabel(false);
    m_pActionsToolbar->setIconSize(QSize(16, 16));
    m_pActionsToolbar->setOrientation(Qt::Vertical);
    m_pActionsToolbar->addAction(m_pAddAction);
    m_pActionsToolbar->addAction(m_pDelAction);
    m_pActionsToolbar->addAction(m_pEditAction);
    m_pActionsToolbar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::MinimumExpanding);
    m_pActionsToolbar->updateGeometry();
    m_pActionsToolbar->setMinimumHeight(m_pActionsToolbar->sizeHint().height());

    /* Setup connections: */
    connect(m_pAddAction, SIGNAL(triggered(bool)), this, SLOT(sltAddInterface()));
    connect(m_pDelAction, SIGNAL(triggered(bool)), this, SLOT(sltDelInterface()));
    connect(m_pEditAction, SIGNAL(triggered(bool)), this, SLOT(sltEditInterface()));
    connect(m_pInterfacesTree, SIGNAL(currentItemChanged(QTreeWidgetItem*, QTreeWidgetItem*)),
            this, SLOT(sltUpdateCurrentItem()));
    connect(m_pInterfacesTree, SIGNAL(customContextMenuRequested(const QPoint&)),
            this, SLOT(sltChowContextMenu(const QPoint&)));
    connect(m_pInterfacesTree, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)),
            this, SLOT(sltEditInterface()));

    /* Apply language settings: */
    retranslateUi();
}

/* Load data to cashe from corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void UIGlobalSettingsNetwork::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to properties & settings: */
    UISettingsPageGlobal::fetchData(data);

    /* Load to cache: */
    const CHostNetworkInterfaceVector &interfaces = vboxGlobal().host().GetNetworkInterfaces();
    for (int iNetworkIndex = 0; iNetworkIndex < interfaces.size(); ++iNetworkIndex)
    {
        const CHostNetworkInterface &iface = interfaces[iNetworkIndex];
        if (iface.GetInterfaceType() == KHostNetworkInterfaceType_HostOnly)
            appendCacheItem(iface);
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
        appendListItem(m_cache.m_items[iNetworkIndex]);
    /* Set first list item as current: */
    m_pInterfacesTree->setCurrentItem(m_pInterfacesTree->topLevelItem(0));
    sltUpdateCurrentItem();
}

/* Save data from corresponding widgets to cache,
 * this task SHOULD be performed in GUI thread only: */
void UIGlobalSettingsNetwork::putToCache()
{
    /* Eraze cache: */
    m_cache.m_items.clear();
    /* Upload to cache: */
    for (int iNetworkIndex = 0; iNetworkIndex < m_pInterfacesTree->topLevelItemCount(); ++iNetworkIndex)
    {
        UIHostNetworkData data;
        UIHostInterfaceItem *pItem = static_cast<UIHostInterfaceItem*>(m_pInterfacesTree->topLevelItem(iNetworkIndex));
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

    /* Prepare useful variables: */
    CVirtualBox vbox = vboxGlobal().virtualBox();
    CHost host = vboxGlobal().host();

    /* Update all the host-only interfaces: */
    for (int iNetworkIndex = 0; iNetworkIndex < m_cache.m_items.size(); ++iNetworkIndex)
    {
        /* Get iterated data: */
        const UIHostNetworkData &data = m_cache.m_items[iNetworkIndex];
        /* Find corresponding interface: */
        CHostNetworkInterface iface = host.FindHostNetworkInterfaceByName(data.m_interface.m_strName);
        if (!iface.isNull())
        {
            /* Host-only interface configuring: */
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

            /* Find corresponding DHCP server: */
            CDHCPServer dhcp = vbox.FindDHCPServerByNetworkName(iface.GetNetworkName());
            if (!dhcp.isNull())
            {
                /* DHCP server configuring: */
                dhcp.SetEnabled(data.m_dhcpserver.m_fDhcpServerEnabled);
                AssertMsg(QHostAddress(data.m_dhcpserver.m_strDhcpServerAddress).protocol() == QAbstractSocket::IPv4Protocol,
                          ("DHCP server IPv4 address must be IPv4-valid!\n"));
                AssertMsg(QHostAddress(data.m_dhcpserver.m_strDhcpServerMask).protocol() == QAbstractSocket::IPv4Protocol,
                          ("DHCP server IPv4 network mask must be IPv4-valid!\n"));
                AssertMsg(QHostAddress(data.m_dhcpserver.m_strDhcpLowerAddress).protocol() == QAbstractSocket::IPv4Protocol,
                          ("DHCP server IPv4 lower bound must be IPv4-valid!\n"));
                AssertMsg(QHostAddress(data.m_dhcpserver.m_strDhcpUpperAddress).protocol() == QAbstractSocket::IPv4Protocol,
                          ("DHCP server IPv4 upper bound must be IPv4-valid!\n"));
                if (QHostAddress(data.m_dhcpserver.m_strDhcpServerAddress).protocol() == QAbstractSocket::IPv4Protocol &&
                    QHostAddress(data.m_dhcpserver.m_strDhcpServerMask).protocol() == QAbstractSocket::IPv4Protocol &&
                    QHostAddress(data.m_dhcpserver.m_strDhcpLowerAddress).protocol() == QAbstractSocket::IPv4Protocol &&
                    QHostAddress(data.m_dhcpserver.m_strDhcpUpperAddress).protocol() == QAbstractSocket::IPv4Protocol)
                    dhcp.SetConfiguration(data.m_dhcpserver.m_strDhcpServerAddress, data.m_dhcpserver.m_strDhcpServerMask,
                                          data.m_dhcpserver.m_strDhcpLowerAddress, data.m_dhcpserver.m_strDhcpUpperAddress);
            }
        }
    }

    /* Upload properties & settings to data: */
    UISettingsPageGlobal::uploadData(data);
}

/* Validation assignments: */
void UIGlobalSettingsNetwork::setValidator(QIWidgetValidator *pValidator)
{
    m_pValidator = pValidator;
}

/* Validation processing: */
bool UIGlobalSettingsNetwork::revalidate(QString &strWarning, QString &strTitle)
{
    UIHostInterfaceItem *pItem = static_cast<UIHostInterfaceItem*>(m_pInterfacesTree->currentItem());
    return pItem ? pItem->revalidate(strWarning, strTitle) : true;
}

/* Navigation stuff: */
void UIGlobalSettingsNetwork::setOrderAfter(QWidget *pWidget)
{
    setTabOrder(pWidget, m_pInterfacesTree);
}

/* Translation stuff: */
void UIGlobalSettingsNetwork::retranslateUi()
{
    /* Translate uic generated strings: */
    Ui::UIGlobalSettingsNetwork::retranslateUi(this);

    /* Translate action tool-tips: */
    m_pAddAction->setText(tr("&Add host-only network"));
    m_pDelAction->setText(tr("&Remove host-only network"));
    m_pEditAction->setText(tr("&Edit host-only network"));

    /* Assign tool-tips: */
    m_pAddAction->setToolTip(m_pAddAction->text().remove('&') +
        QString(" (%1)").arg(m_pAddAction->shortcut().toString()));
    m_pDelAction->setToolTip(m_pDelAction->text().remove('&') +
        QString(" (%1)").arg(m_pDelAction->shortcut().toString()));
    m_pEditAction->setToolTip(m_pEditAction->text().remove('&') +
        QString(" (%1)").arg(m_pEditAction->shortcut().toString()));
}

/* Adds new network interface: */
void UIGlobalSettingsNetwork::sltAddInterface()
{
    /* Prepare useful variables: */
    CVirtualBox vbox = vboxGlobal().virtualBox();
    CHost host = vboxGlobal().host();

    /* Create new host-only interface: */
    CHostNetworkInterface iface;
    CProgress progress = host.CreateHostOnlyNetworkInterface(iface);
    if (host.isOk())
    {
        msgCenter().showModalProgressDialog(progress, tr("Networking"),
                                              ":/nw_32px.png", this, true, 0);
        if (progress.GetResultCode() == 0)
        {
            /* Create DHCP server: */
            CDHCPServer dhcp = vbox.FindDHCPServerByNetworkName(iface.GetNetworkName());
            if (dhcp.isNull())
            {
                vbox.CreateDHCPServer(iface.GetNetworkName());
                dhcp = vbox.FindDHCPServerByNetworkName(iface.GetNetworkName());
            }
            AssertMsg(!dhcp.isNull(), ("DHCP server creation failed!\n"));

            /* Append cache with new item: */
            appendCacheItem(iface);
            /* Append list with new item: */
            appendListItem(m_cache.m_items.last(), true);
        }
        else
            msgCenter().cannotCreateHostInterface(progress, this);
    }
    else
        msgCenter().cannotCreateHostInterface(host, this);
}

/* Removes selected network interface: */
void UIGlobalSettingsNetwork::sltDelInterface()
{
    /* Get interface item: */
    UIHostInterfaceItem *pItem = static_cast<UIHostInterfaceItem*>(m_pInterfacesTree->currentItem());
    AssertMsg(pItem, ("Current item should present!\n"));
    /* Get interface name: */
    QString strInterfaceName(pItem->name());
    /* Asking user about deleting selected network interface: */
    if (msgCenter().confirmDeletingHostInterface(strInterfaceName, this) == QIMessageBox::Cancel)
        return;

    /* Prepare useful variables: */
    CVirtualBox vbox = vboxGlobal().virtualBox();
    CHost host = vboxGlobal().host();

    /* Find corresponding interface: */
    const CHostNetworkInterface &iface = host.FindHostNetworkInterfaceByName(strInterfaceName);

    /* Remove DHCP server first: */
    CDHCPServer dhcp = vboxGlobal().virtualBox().FindDHCPServerByNetworkName(iface.GetNetworkName());
    if (!dhcp.isNull())
        vbox.RemoveDHCPServer(dhcp);

    /* Remove interface finally: */
    CProgress progress = host.RemoveHostOnlyNetworkInterface(iface.GetId());
    if (host.isOk())
    {
        msgCenter().showModalProgressDialog(progress, tr("Networking"),
                                              ":/nw_32px.png", this, true, 0);
        if (progress.GetResultCode() == 0)
        {
            /* Remove list item: */
            removeListItem(pItem);
            /* Remove cache item: */
            removeCacheItem(strInterfaceName);
        }
        else
            msgCenter().cannotRemoveHostInterface(progress, iface, this);
    }
    else
        msgCenter().cannotRemoveHostInterface(host, iface, this);
}

/* Edits selected network interface: */
void UIGlobalSettingsNetwork::sltEditInterface()
{
    /* Check interface presence */
    UIHostInterfaceItem *pItem = static_cast<UIHostInterfaceItem*>(m_pInterfacesTree->currentItem());
    AssertMsg(pItem, ("Current item should be selected!\n"));
    /* Edit current item data */
    UIGlobalSettingsNetworkDetails details(this);
    details.getFromItem(pItem);
    if (details.exec() == QDialog::Accepted)
    {
        details.putBackToItem();
        pItem->updateInfo();
        sltUpdateCurrentItem();
        m_pValidator->revalidate();
        m_fChanged = true;
    }
}

/* Update current network interface data relations: */
void UIGlobalSettingsNetwork::sltUpdateCurrentItem()
{
    /* Get current item: */
    UIHostInterfaceItem *pItem = static_cast<UIHostInterfaceItem*>(m_pInterfacesTree->currentItem());
    /* Set the final label text: */
    m_pInfoLabel->setText(pItem ? pItem->updateInfo() : QString());
    /* Update availability: */
    m_pDelAction->setEnabled(pItem);
    m_pEditAction->setEnabled(pItem);
}

/* Show network interface context-menu: */
void UIGlobalSettingsNetwork::sltChowContextMenu(const QPoint &pos)
{
    QMenu menu;
    if (m_pInterfacesTree->itemAt(pos))
    {
        menu.addAction(m_pEditAction);
        menu.addAction(m_pDelAction);
    }
    else
    {
        menu.addAction(m_pAddAction);
    }
    menu.exec(m_pInterfacesTree->mapToGlobal(pos));
}

void UIGlobalSettingsNetwork::appendCacheItem(const CHostNetworkInterface &iface)
{
    /* Get DHCP server (create if necessary): */
    CDHCPServer dhcp = vboxGlobal().virtualBox().FindDHCPServerByNetworkName(iface.GetNetworkName());
    if (dhcp.isNull())
    {
        vboxGlobal().virtualBox().CreateDHCPServer(iface.GetNetworkName());
        dhcp = vboxGlobal().virtualBox().FindDHCPServerByNetworkName(iface.GetNetworkName());
    }
    AssertMsg(!dhcp.isNull(), ("DHCP server creation failed!\n"));

    /* Prepare cache item: */
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

    /* Append cache item: */
    m_cache.m_items << data;
}

void UIGlobalSettingsNetwork::removeCacheItem(const QString &strInterfaceName)
{
    /* Search for invalidated cache item: */
    for (int iNetworkIndex = 0; iNetworkIndex < m_cache.m_items.size(); ++iNetworkIndex)
    {
        /* Get iterated data: */
        const UIHostNetworkData &data = m_cache.m_items[iNetworkIndex];
        if (data.m_interface.m_strName == strInterfaceName)
        {
            m_cache.m_items.removeAll(data);
            break;
        }
    }
}

void UIGlobalSettingsNetwork::appendListItem(const UIHostNetworkData &data, bool fChooseItem)
{
    /* Add new item to the list: */
    UIHostInterfaceItem *pItem = new UIHostInterfaceItem;
    pItem->fetchNetworkData(data);
    m_pInterfacesTree->addTopLevelItem(pItem);
    /* And choose it as current if necessary: */
    if (fChooseItem)
        m_pInterfacesTree->setCurrentItem(pItem);
}

void UIGlobalSettingsNetwork::removeListItem(UIHostInterfaceItem *pItem)
{
    /* Delete passed item: */
    delete pItem;
}

