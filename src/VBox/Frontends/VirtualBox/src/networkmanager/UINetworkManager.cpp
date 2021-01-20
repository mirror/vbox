/* $Id$ */
/** @file
 * VBox Qt GUI - UINetworkManager class implementation.
 */

/*
 * Copyright (C) 2009-2021 Oracle Corporation
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
#include <QMenuBar>
#include <QPushButton>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIDialogButtonBox.h"
#include "QITabWidget.h"
#include "QITreeWidget.h"
#include "UIActionPoolManager.h"
#include "UIConverter.h"
#include "UIDetailsWidgetHostNetwork.h"
#include "UIDetailsWidgetNATNetwork.h"
#include "UIExtraDataManager.h"
#include "UIIconPool.h"
#include "UIMessageCenter.h"
#include "UINetworkManager.h"
#include "UINetworkManagerUtils.h"
#include "QIToolBar.h"
#ifdef VBOX_WS_MAC
# include "UIWindowMenuManager.h"
#endif /* VBOX_WS_MAC */
#include "UICommon.h"

/* COM includes: */
#include "CDHCPServer.h"
#include "CHost.h"
#include "CHostNetworkInterface.h"
#include "CNATNetwork.h"

/* Other VBox includes: */
#include <iprt/cidr.h>


/** Tab-widget indexes. */
enum TabWidgetIndex
{
    TabWidgetIndex_HostNetwork,
    TabWidgetIndex_NATNetwork,
};


/** Host network tree-widget column indexes. */
enum HostNetworkColumn
{
    HostNetworkColumn_Name,
    HostNetworkColumn_IPv4,
    HostNetworkColumn_IPv6,
    HostNetworkColumn_DHCP,
    HostNetworkColumn_Max,
};


/** NAT network tree-widget column indexes. */
enum NATNetworkColumn
{
    NATNetworkColumn_Availability,
    NATNetworkColumn_Name,
    NATNetworkColumn_Max,
};


/** Network Manager: Host Network tree-widget item. */
class UIItemHostNetwork : public QITreeWidgetItem, public UIDataHostNetwork
{
    Q_OBJECT;

public:

    /** Updates item fields from data. */
    void updateFields();

    /** Returns item name. */
    QString name() const { return m_interface.m_strName; }

private:

    /** Returns CIDR for a passed @a strMask. */
    static int maskToCidr(const QString &strMask);
};


/** Network Manager: NAT Network tree-widget item. */
class UIItemNATNetwork : public QITreeWidgetItem, public UIDataNATNetwork
{
    Q_OBJECT;

public:

    /** Updates item fields from data. */
    void updateFields();

    /** Returns item name. */
    QString name() const { return m_strName; }
};


/*********************************************************************************************************************************
*   Class UIItemHostNetwork implementation.                                                                                      *
*********************************************************************************************************************************/

void UIItemHostNetwork::updateFields()
{
    /* Compose item fields: */
    setText(HostNetworkColumn_Name, m_interface.m_strName);
    setText(HostNetworkColumn_IPv4, m_interface.m_strAddress.isEmpty() ? QString() :
                                    QString("%1/%2").arg(m_interface.m_strAddress).arg(maskToCidr(m_interface.m_strMask)));
    setText(HostNetworkColumn_IPv6, m_interface.m_strAddress6.isEmpty() || !m_interface.m_fSupportedIPv6 ? QString() :
                                    QString("%1/%2").arg(m_interface.m_strAddress6).arg(m_interface.m_strPrefixLength6.toInt()));
    setText(HostNetworkColumn_DHCP, tr("Enable", "DHCP Server"));
    setCheckState(HostNetworkColumn_DHCP, m_dhcpserver.m_fEnabled ? Qt::Checked : Qt::Unchecked);

    /* Compose item tool-tip: */
    const QString strTable("<table cellspacing=5>%1</table>");
    const QString strHeader("<tr><td><nobr>%1:&nbsp;</nobr></td><td><nobr>%2</nobr></td></tr>");
    const QString strSubHeader("<tr><td><nobr>&nbsp;&nbsp;%1:&nbsp;</nobr></td><td><nobr>%2</nobr></td></tr>");
    QString strToolTip;

    /* Interface information: */
    strToolTip += strHeader.arg(tr("Adapter"))
                           .arg(m_interface.m_fDHCPEnabled ?
                                tr("Automatically configured", "interface") :
                                tr("Manually configured", "interface"));
    strToolTip += strSubHeader.arg(tr("IPv4 Address"))
                              .arg(m_interface.m_strAddress.isEmpty() ?
                                   tr ("Not set", "address") :
                                   m_interface.m_strAddress) +
                  strSubHeader.arg(tr("IPv4 Network Mask"))
                              .arg(m_interface.m_strMask.isEmpty() ?
                                   tr ("Not set", "mask") :
                                   m_interface.m_strMask);
    if (m_interface.m_fSupportedIPv6)
    {
        strToolTip += strSubHeader.arg(tr("IPv6 Address"))
                                  .arg(m_interface.m_strAddress6.isEmpty() ?
                                       tr("Not set", "address") :
                                       m_interface.m_strAddress6) +
                      strSubHeader.arg(tr("IPv6 Prefix Length"))
                                  .arg(m_interface.m_strPrefixLength6.isEmpty() ?
                                       tr("Not set", "length") :
                                       m_interface.m_strPrefixLength6);
    }

    /* DHCP server information: */
    strToolTip += strHeader.arg(tr("DHCP Server"))
                           .arg(m_dhcpserver.m_fEnabled ?
                                tr("Enabled", "server") :
                                tr("Disabled", "server"));
    if (m_dhcpserver.m_fEnabled)
    {
        strToolTip += strSubHeader.arg(tr("Address"))
                                  .arg(m_dhcpserver.m_strAddress.isEmpty() ?
                                       tr("Not set", "address") :
                                       m_dhcpserver.m_strAddress) +
                      strSubHeader.arg(tr("Network Mask"))
                                  .arg(m_dhcpserver.m_strMask.isEmpty() ?
                                       tr("Not set", "mask") :
                                       m_dhcpserver.m_strMask) +
                      strSubHeader.arg(tr("Lower Bound"))
                                  .arg(m_dhcpserver.m_strLowerAddress.isEmpty() ?
                                       tr("Not set", "bound") :
                                       m_dhcpserver.m_strLowerAddress) +
                      strSubHeader.arg(tr("Upper Bound"))
                                  .arg(m_dhcpserver.m_strUpperAddress.isEmpty() ?
                                       tr("Not set", "bound") :
                                       m_dhcpserver.m_strUpperAddress);
    }

    /* Assign tool-tip finally: */
    setToolTip(HostNetworkColumn_Name, strTable.arg(strToolTip));
}

/* static */
int UIItemHostNetwork::maskToCidr(const QString &strMask)
{
    /* Parse passed mask: */
    QList<int> address;
    foreach (const QString &strValue, strMask.split('.'))
        address << strValue.toInt();

    /* Calculate CIDR: */
    int iCidr = 0;
    for (int i = 0; i < 4 || i < address.size(); ++i)
    {
        switch(address.at(i))
        {
            case 0x80: iCidr += 1; break;
            case 0xC0: iCidr += 2; break;
            case 0xE0: iCidr += 3; break;
            case 0xF0: iCidr += 4; break;
            case 0xF8: iCidr += 5; break;
            case 0xFC: iCidr += 6; break;
            case 0xFE: iCidr += 7; break;
            case 0xFF: iCidr += 8; break;
            /* Return CIDR prematurelly: */
            default: return iCidr;
        }
    }

    /* Return CIDR: */
    return iCidr;
}


/*********************************************************************************************************************************
*   Class UIItemNATNetwork implementation.                                                                                       *
*********************************************************************************************************************************/

void UIItemNATNetwork::updateFields()
{
    /* Compose item fields: */
    setCheckState(NATNetworkColumn_Availability, m_fEnabled ? Qt::Checked : Qt::Unchecked);
    setText(NATNetworkColumn_Name, m_strName);

    /* Compose tool-tip: */
    const QString strTable("<table cellspacing=5>%1</table>");
    const QString strHeader("<tr><td><nobr>%1:&nbsp;</nobr></td><td><nobr>%2</nobr></td></tr>");
    const QString strSubHeader("<tr><td><nobr>&nbsp;&nbsp;%1:&nbsp;</nobr></td><td><nobr>%2</nobr></td></tr>");
    QString strToolTip;

    /* Network information: */
    strToolTip += strHeader.arg(tr("Network Name"), m_strName);
    strToolTip += strHeader.arg(tr("Network CIDR"), m_strCIDR);
    strToolTip += strHeader.arg(tr("Supports DHCP"), m_fSupportsDHCP ? tr("yes") : tr("no"));
    strToolTip += strHeader.arg(tr("Supports IPv6"), m_fSupportsIPv6 ? tr("yes") : tr("no"));
    if (m_fSupportsIPv6 && m_fAdvertiseDefaultIPv6Route)
        strToolTip += strSubHeader.arg(tr("Default IPv6 route"), tr("yes"));

    /* Assign tool-tip finally: */
    setToolTip(NATNetworkColumn_Name, strTable.arg(strToolTip));
}


/*********************************************************************************************************************************
*   Class UINetworkManagerWidget implementation.                                                                                 *
*********************************************************************************************************************************/

UINetworkManagerWidget::UINetworkManagerWidget(EmbedTo enmEmbedding, UIActionPool *pActionPool,
                                               bool fShowToolbar /* = true */, QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_enmEmbedding(enmEmbedding)
    , m_pActionPool(pActionPool)
    , m_fShowToolbar(fShowToolbar)
    , m_pToolBar(0)
    , m_pTabWidget(0)
    , m_pTabHostNetwork(0)
    , m_pLayoutHostNetwork(0)
    , m_pTreeWidgetHostNetwork(0)
    , m_pDetailsWidgetHostNetwork(0)
    , m_pTabNATNetwork(0)
    , m_pLayoutNATNetwork(0)
    , m_pTreeWidgetNATNetwork(0)
    , m_pDetailsWidgetNATNetwork(0)
{
    prepare();
}

QMenu *UINetworkManagerWidget::menu() const
{
    AssertPtrReturn(m_pActionPool, 0);
    return m_pActionPool->action(UIActionIndexMN_M_NetworkWindow)->menu();
}

void UINetworkManagerWidget::retranslateUi()
{
    /* Adjust toolbar: */
#ifdef VBOX_WS_MAC
    // WORKAROUND:
    // There is a bug in Qt Cocoa which result in showing a "more arrow" when
    // the necessary size of the toolbar is increased. Also for some languages
    // the with doesn't match if the text increase. So manually adjust the size
    // after changing the text.
    if (m_pToolBar)
        m_pToolBar->updateLayout();
#endif

    /* Translate tab-widget: */
    if (m_pTabWidget)
    {
        m_pTabWidget->setTabText(0, UINetworkManager::tr("Host-only Networks"));
        m_pTabWidget->setTabText(1, UINetworkManager::tr("NAT Networks"));
    }

    /* Translate host network tree-widget: */
    if (m_pTreeWidgetHostNetwork)
    {
        const QStringList fields = QStringList()
                                   << UINetworkManager::tr("Name")
                                   << UINetworkManager::tr("IPv4 Address/Mask")
                                   << UINetworkManager::tr("IPv6 Address/Mask")
                                   << UINetworkManager::tr("DHCP Server");
        m_pTreeWidgetHostNetwork->setHeaderLabels(fields);
    }

    /* Translate NAT network tree-widget: */
    if (m_pTreeWidgetNATNetwork)
    {
        const QStringList fields = QStringList()
                                   << UINetworkManager::tr("Active")
                                   << UINetworkManager::tr("Name");
        m_pTreeWidgetNATNetwork->setHeaderLabels(fields);
    }
}

void UINetworkManagerWidget::resizeEvent(QResizeEvent *pEvent)
{
    /* Call to base-class: */
    QIWithRetranslateUI<QWidget>::resizeEvent(pEvent);

    /* Adjust tree-widgets: */
    sltAdjustTreeWidgets();
}

void UINetworkManagerWidget::showEvent(QShowEvent *pEvent)
{
    /* Call to base-class: */
    QIWithRetranslateUI<QWidget>::showEvent(pEvent);

    /* Adjust tree-widgets: */
    sltAdjustTreeWidgets();
}

void UINetworkManagerWidget::sltResetDetailsChanges()
{
    /* Check tab-widget: */
    AssertMsgReturnVoid(m_pTabWidget, ("This action should not be allowed!\n"));

    /* Just push current item data to details-widget again: */
    switch (m_pTabWidget->currentIndex())
    {
        case TabWidgetIndex_HostNetwork: sltHandleCurrentItemChangeHostNetwork(); break;
        case TabWidgetIndex_NATNetwork: sltHandleCurrentItemChangeNATNetwork(); break;
        default: break;
    }
}

void UINetworkManagerWidget::sltApplyDetailsChanges()
{
    /* Check tab-widget: */
    AssertMsgReturnVoid(m_pTabWidget, ("This action should not be allowed!\n"));

    /* Apply details-widget data changes: */
    switch (m_pTabWidget->currentIndex())
    {
        case TabWidgetIndex_HostNetwork: sltApplyDetailsChangesHostNetwork(); break;
        case TabWidgetIndex_NATNetwork: sltApplyDetailsChangesNATNetwork(); break;
        default: break;
    }
}

void UINetworkManagerWidget::sltCreateHostNetwork()
{
    /* For host networks only: */
    if (m_pTabWidget->currentIndex() != TabWidgetIndex_HostNetwork)
        return;

    /* Get host for further activities: */
    CHost comHost = uiCommon().host();

    /* Create interface: */
    CHostNetworkInterface comInterface;
    CProgress comProgress = comHost.CreateHostOnlyNetworkInterface(comInterface);

    /* Show error message if necessary: */
    if (!comHost.isOk() || comProgress.isNull())
        msgCenter().cannotCreateHostNetworkInterface(comHost, this);
    else
    {
        /* Show interface creation progress: */
        msgCenter().showModalProgressDialog(comProgress, UINetworkManager::tr("Adding network ..."), ":/progress_network_interface_90px.png", this, 0);

        /* Show error message if necessary: */
        if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
            msgCenter().cannotCreateHostNetworkInterface(comProgress, this);
        else
        {
            /* Get network name for further activities: */
            const QString strNetworkName = comInterface.GetNetworkName();

            /* Show error message if necessary: */
            if (!comInterface.isOk())
                msgCenter().cannotAcquireHostNetworkInterfaceParameter(comInterface, this);
            else
            {
                /* Get VBox for further activities: */
                CVirtualBox comVBox = uiCommon().virtualBox();

                /* Find corresponding DHCP server (create if necessary): */
                CDHCPServer comServer = comVBox.FindDHCPServerByNetworkName(strNetworkName);
                if (!comVBox.isOk() || comServer.isNull())
                    comServer = comVBox.CreateDHCPServer(strNetworkName);

                /* Show error message if necessary: */
                if (!comVBox.isOk() || comServer.isNull())
                    msgCenter().cannotCreateDHCPServer(comVBox, strNetworkName, this);
            }

            /* Add interface to the tree: */
            UIDataHostNetwork data;
            loadHostNetwork(comInterface, data);
            createItemForHostNetwork(data, true);

            /* Adjust tree-widgets: */
            sltAdjustTreeWidgets();
        }
    }
}

void UINetworkManagerWidget::sltRemoveHostNetwork()
{
    /* For host networks only: */
    if (m_pTabWidget->currentIndex() != TabWidgetIndex_HostNetwork)
        return;

    /* Check host network tree-widget: */
    AssertMsgReturnVoid(m_pTreeWidgetHostNetwork, ("Host network tree-widget isn't created!\n"));

    /* Get network item: */
    UIItemHostNetwork *pItem = static_cast<UIItemHostNetwork*>(m_pTreeWidgetHostNetwork->currentItem());
    AssertMsgReturnVoid(pItem, ("Current item must not be null!\n"));

    /* Get interface name: */
    const QString strInterfaceName(pItem->name());

    /* Confirm host network removal: */
    if (!msgCenter().confirmHostOnlyInterfaceRemoval(strInterfaceName, this))
        return;

    /* Get host for further activities: */
    CHost comHost = uiCommon().host();

    /* Find corresponding interface: */
    const CHostNetworkInterface &comInterface = comHost.FindHostNetworkInterfaceByName(strInterfaceName);

    /* Show error message if necessary: */
    if (!comHost.isOk() || comInterface.isNull())
        msgCenter().cannotFindHostNetworkInterface(comHost, strInterfaceName, this);
    else
    {
        /* Get network name for further activities: */
        QString strNetworkName;
        if (comInterface.isOk())
            strNetworkName = comInterface.GetNetworkName();
        /* Get interface id for further activities: */
        QUuid uInterfaceId;
        if (comInterface.isOk())
            uInterfaceId = comInterface.GetId();

        /* Show error message if necessary: */
        if (!comInterface.isOk())
            msgCenter().cannotAcquireHostNetworkInterfaceParameter(comInterface, this);
        else
        {
            /* Get VBox for further activities: */
            CVirtualBox comVBox = uiCommon().virtualBox();

            /* Find corresponding DHCP server: */
            const CDHCPServer &comServer = comVBox.FindDHCPServerByNetworkName(strNetworkName);
            if (comVBox.isOk() && comServer.isNotNull())
            {
                /* Remove server if any: */
                comVBox.RemoveDHCPServer(comServer);

                /* Show error message if necessary: */
                if (!comVBox.isOk())
                    msgCenter().cannotRemoveDHCPServer(comVBox, strInterfaceName, this);
            }

            /* Remove interface finally: */
            CProgress comProgress = comHost.RemoveHostOnlyNetworkInterface(uInterfaceId);

            /* Show error message if necessary: */
            if (!comHost.isOk() || comProgress.isNull())
                msgCenter().cannotRemoveHostNetworkInterface(comHost, strInterfaceName, this);
            else
            {
                /* Show interface removal progress: */
                msgCenter().showModalProgressDialog(comProgress, UINetworkManager::tr("Removing network ..."), ":/progress_network_interface_90px.png", this, 0);

                /* Show error message if necessary: */
                if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
                    return msgCenter().cannotRemoveHostNetworkInterface(comProgress, strInterfaceName, this);
                else
                {
                    /* Remove interface from the tree: */
                    delete pItem;

                    /* Adjust tree-widgets: */
                    sltAdjustTreeWidgets();
                }
            }
        }
    }
}

void UINetworkManagerWidget::sltCreateNATNetwork()
{
    /* For NAT networks only: */
    if (m_pTabWidget->currentIndex() != TabWidgetIndex_NATNetwork)
        return;

    /* Compose a set of busy names: */
    QSet<QString> names;
    for (int i = 0; i < m_pTreeWidgetNATNetwork->topLevelItemCount(); ++i)
        names << qobject_cast<UIItemNATNetwork*>(m_pTreeWidgetNATNetwork->childItem(i))->name();
    /* Compose a map of busy indexes: */
    QMap<int, bool> presence;
    const QString strNameTemplate("NatNetwork%1");
    const QRegExp regExp(strNameTemplate.arg("([\\d]*)"));
    foreach (const QString &strName, names)
        if (regExp.indexIn(strName) != -1)
            presence[regExp.cap(1).toInt()] = true;
    /* Search for a minimum index: */
    int iMinimumIndex = 0;
    for (int i = 0; !presence.isEmpty() && i <= presence.lastKey() + 1; ++i)
        if (!presence.contains(i))
        {
            iMinimumIndex = i;
            break;
        }
    /* Compose resulting index and name: */
    const QString strNetworkName = strNameTemplate.arg(iMinimumIndex == 0 ? QString() : QString::number(iMinimumIndex));

    /* Compose new item data: */
    UIDataNATNetwork oldData;
    oldData.m_fExists = true;
    oldData.m_fEnabled = true;
    oldData.m_strName = strNetworkName;
    oldData.m_strCIDR = "10.0.2.0/24";
    oldData.m_fSupportsDHCP = true;
    oldData.m_fSupportsIPv6 = false;
    oldData.m_fAdvertiseDefaultIPv6Route = false;

    /* Get VirtualBox for further activities: */
    CVirtualBox comVBox = uiCommon().virtualBox();

    /* Create network: */
    CNATNetwork comNetwork = comVBox.CreateNATNetwork(oldData.m_strName);

    /* Show error message if necessary: */
    if (!comVBox.isOk())
        msgCenter().cannotCreateNATNetwork(comVBox, this);
    else
    {
        /* Save whether NAT network is enabled: */
        if (comNetwork.isOk())
            comNetwork.SetEnabled(oldData.m_fEnabled);
        /* Save NAT network name: */
        if (comNetwork.isOk())
            comNetwork.SetNetworkName(oldData.m_strName);
        /* Save NAT network CIDR: */
        if (comNetwork.isOk())
            comNetwork.SetNetwork(oldData.m_strCIDR);
        /* Save whether NAT network needs DHCP server: */
        if (comNetwork.isOk())
            comNetwork.SetNeedDhcpServer(oldData.m_fSupportsDHCP);
        /* Save whether NAT network supports IPv6: */
        if (comNetwork.isOk())
            comNetwork.SetIPv6Enabled(oldData.m_fSupportsIPv6);
        /* Save whether NAT network should advertise default IPv6 route: */
        if (comNetwork.isOk())
            comNetwork.SetAdvertiseDefaultIPv6RouteEnabled(oldData.m_fAdvertiseDefaultIPv6Route);

        /* Show error message if necessary: */
        if (!comNetwork.isOk())
            msgCenter().cannotSaveNATNetworkParameter(comNetwork, this);

        /* Add network to the tree: */
        UIDataNATNetwork newData;
        loadNATNetwork(comNetwork, newData);
        createItemForNATNetwork(newData, true);

        /* Adjust tree-widgets: */
        sltAdjustTreeWidgets();
    }
}

void UINetworkManagerWidget::sltRemoveNATNetwork()
{
    /* For NAT networks only: */
    if (m_pTabWidget->currentIndex() != TabWidgetIndex_NATNetwork)
        return;

    /* Check NAT network tree-widget: */
    AssertMsgReturnVoid(m_pTreeWidgetNATNetwork, ("NAT network tree-widget isn't created!\n"));

    /* Get network item: */
    UIItemNATNetwork *pItem = static_cast<UIItemNATNetwork*>(m_pTreeWidgetNATNetwork->currentItem());
    AssertMsgReturnVoid(pItem, ("Current item must not be null!\n"));

    /* Get network name: */
    const QString strNetworkName(pItem->name());

    /* Confirm host network removal: */
    if (!msgCenter().confirmNATNetworkRemoval(strNetworkName, this))
        return;

    /* Get VirtualBox for further activities: */
    CVirtualBox comVBox = uiCommon().virtualBox();

    /* Find corresponding network: */
    const CNATNetwork &comNetwork = comVBox.FindNATNetworkByName(strNetworkName);

    /* Show error message if necessary: */
    if (!comVBox.isOk() || comNetwork.isNull())
        msgCenter().cannotFindNATNetwork(comVBox, strNetworkName, this);
    else
    {
        /* Remove network finally: */
        comVBox.RemoveNATNetwork(comNetwork);

        /* Show error message if necessary: */
        if (!comVBox.isOk())
            msgCenter().cannotRemoveNATNetwork(comVBox, strNetworkName, this);
        else
        {
            /* Remove interface from the tree: */
            delete pItem;

            /* Adjust tree-widgets: */
            sltAdjustTreeWidgets();
        }
    }
}

void UINetworkManagerWidget::sltToggleDetailsVisibility(bool fVisible)
{
    /* Save the setting: */
    gEDataManager->setHostNetworkManagerDetailsExpanded(fVisible);
    /* Show/hide details area and Apply/Reset buttons: */
    switch (m_pTabWidget->currentIndex())
    {
        case TabWidgetIndex_HostNetwork:
        {
            if (m_pDetailsWidgetNATNetwork)
                m_pDetailsWidgetNATNetwork->setVisible(false);
            if (m_pDetailsWidgetHostNetwork)
                m_pDetailsWidgetHostNetwork->setVisible(fVisible);
            break;
        }
        case TabWidgetIndex_NATNetwork:
        {
            if (m_pDetailsWidgetHostNetwork)
                m_pDetailsWidgetHostNetwork->setVisible(false);
            if (m_pDetailsWidgetNATNetwork)
                m_pDetailsWidgetNATNetwork->setVisible(fVisible);
            break;
        }
    }
    /* Notify external listeners: */
    emit sigDetailsVisibilityChanged(fVisible);
}

void UINetworkManagerWidget::sltHandleCurrentTabWidgetIndexChange()
{
    /* Show/hide details area and Apply/Reset buttons: */
    const bool fVisible = m_pActionPool->action(UIActionIndexMN_M_Network_T_Details)->isChecked();
    switch (m_pTabWidget->currentIndex())
    {
        case TabWidgetIndex_HostNetwork:
        {
            if (m_pDetailsWidgetNATNetwork)
                m_pDetailsWidgetNATNetwork->setVisible(false);
            if (m_pDetailsWidgetHostNetwork)
                m_pDetailsWidgetHostNetwork->setVisible(fVisible);
            break;
        }
        case TabWidgetIndex_NATNetwork:
        {
            if (m_pDetailsWidgetHostNetwork)
                m_pDetailsWidgetHostNetwork->setVisible(false);
            if (m_pDetailsWidgetNATNetwork)
                m_pDetailsWidgetNATNetwork->setVisible(fVisible);
            break;
        }
    }
}

void UINetworkManagerWidget::sltAdjustTreeWidgets()
{
    /* Check host network tree-widget: */
    if (m_pTreeWidgetHostNetwork)
    {
        /* Get the tree-widget abstract interface: */
        QAbstractItemView *pItemView = m_pTreeWidgetHostNetwork;
        /* Get the tree-widget header-view: */
        QHeaderView *pItemHeader = m_pTreeWidgetHostNetwork->header();

        /* Calculate the total tree-widget width: */
        const int iTotal = m_pTreeWidgetHostNetwork->viewport()->width();
        /* Look for a minimum width hints for non-important columns: */
        const int iMinWidth1 = qMax(pItemView->sizeHintForColumn(HostNetworkColumn_IPv4), pItemHeader->sectionSizeHint(HostNetworkColumn_IPv4));
        const int iMinWidth2 = qMax(pItemView->sizeHintForColumn(HostNetworkColumn_IPv6), pItemHeader->sectionSizeHint(HostNetworkColumn_IPv6));
        const int iMinWidth3 = qMax(pItemView->sizeHintForColumn(HostNetworkColumn_DHCP), pItemHeader->sectionSizeHint(HostNetworkColumn_DHCP));
        /* Propose suitable width hints for non-important columns: */
        const int iWidth1 = iMinWidth1 < iTotal / HostNetworkColumn_Max ? iMinWidth1 : iTotal / HostNetworkColumn_Max;
        const int iWidth2 = iMinWidth2 < iTotal / HostNetworkColumn_Max ? iMinWidth2 : iTotal / HostNetworkColumn_Max;
        const int iWidth3 = iMinWidth3 < iTotal / HostNetworkColumn_Max ? iMinWidth3 : iTotal / HostNetworkColumn_Max;
        /* Apply the proposal: */
        m_pTreeWidgetHostNetwork->setColumnWidth(HostNetworkColumn_IPv4, iWidth1);
        m_pTreeWidgetHostNetwork->setColumnWidth(HostNetworkColumn_IPv6, iWidth2);
        m_pTreeWidgetHostNetwork->setColumnWidth(HostNetworkColumn_DHCP, iWidth3);
        m_pTreeWidgetHostNetwork->setColumnWidth(HostNetworkColumn_Name, iTotal - iWidth1 - iWidth2 - iWidth3);
    }

    /* Check NAT network tree-widget: */
    if (m_pTreeWidgetNATNetwork)
    {
        /* Get the tree-widget abstract interface: */
        QAbstractItemView *pItemView = m_pTreeWidgetNATNetwork;
        /* Get the tree-widget header-view: */
        QHeaderView *pItemHeader = m_pTreeWidgetNATNetwork->header();

        /* Calculate the total tree-widget width: */
        const int iTotal = m_pTreeWidgetNATNetwork->viewport()->width();
        /* Look for a minimum width hints for non-important columns: */
        const int iMinWidth1 = qMax(pItemView->sizeHintForColumn(NATNetworkColumn_Availability), pItemHeader->sectionSizeHint(NATNetworkColumn_Availability));
        /* Propose suitable width hints for non-important columns: */
        const int iWidth1 = iMinWidth1 < iTotal / NATNetworkColumn_Max ? iMinWidth1 : iTotal / NATNetworkColumn_Max;
        /* Apply the proposal: */
        m_pTreeWidgetNATNetwork->setColumnWidth(NATNetworkColumn_Availability, iWidth1);
        m_pTreeWidgetNATNetwork->setColumnWidth(NATNetworkColumn_Name, iTotal - iWidth1);
    }
}

void UINetworkManagerWidget::sltHandleItemChangeHostNetwork(QTreeWidgetItem *pItem)
{
    /* Get network item: */
    UIItemHostNetwork *pChangedItem = static_cast<UIItemHostNetwork*>(pItem);
    AssertMsgReturnVoid(pChangedItem, ("Changed item must not be null!\n"));

    /* Get item data: */
    UIDataHostNetwork oldData = *pChangedItem;

    /* Make sure dhcp server status changed: */
    if (   (   oldData.m_dhcpserver.m_fEnabled
            && pChangedItem->checkState(HostNetworkColumn_DHCP) == Qt::Checked)
        || (   !oldData.m_dhcpserver.m_fEnabled
            && pChangedItem->checkState(HostNetworkColumn_DHCP) == Qt::Unchecked))
        return;

    /* Get host for further activities: */
    CHost comHost = uiCommon().host();

    /* Find corresponding interface: */
    CHostNetworkInterface comInterface = comHost.FindHostNetworkInterfaceByName(oldData.m_interface.m_strName);

    /* Show error message if necessary: */
    if (!comHost.isOk() || comInterface.isNull())
        msgCenter().cannotFindHostNetworkInterface(comHost, oldData.m_interface.m_strName, this);
    else
    {
        /* Get network name for further activities: */
        const QString strNetworkName = comInterface.GetNetworkName();

        /* Show error message if necessary: */
        if (!comInterface.isOk())
            msgCenter().cannotAcquireHostNetworkInterfaceParameter(comInterface, this);
        else
        {
            /* Get VBox for further activities: */
            CVirtualBox comVBox = uiCommon().virtualBox();

            /* Find corresponding DHCP server (create if necessary): */
            CDHCPServer comServer = comVBox.FindDHCPServerByNetworkName(strNetworkName);
            if (!comVBox.isOk() || comServer.isNull())
                comServer = comVBox.CreateDHCPServer(strNetworkName);

            /* Show error message if necessary: */
            if (!comVBox.isOk() || comServer.isNull())
                msgCenter().cannotCreateDHCPServer(comVBox, strNetworkName, this);
            else
            {
                /* Save whether DHCP server is enabled: */
                if (comServer.isOk())
                    comServer.SetEnabled(!oldData.m_dhcpserver.m_fEnabled);
                /* Save default DHCP server configuration if current is invalid: */
                if (   comServer.isOk()
                    && !oldData.m_dhcpserver.m_fEnabled
                    && (   oldData.m_dhcpserver.m_strAddress == "0.0.0.0"
                        || oldData.m_dhcpserver.m_strMask == "0.0.0.0"
                        || oldData.m_dhcpserver.m_strLowerAddress == "0.0.0.0"
                        || oldData.m_dhcpserver.m_strUpperAddress == "0.0.0.0"))
                {
                    const QStringList &proposal = makeDhcpServerProposal(oldData.m_interface.m_strAddress,
                                                                         oldData.m_interface.m_strMask);
                    comServer.SetConfiguration(proposal.at(0), proposal.at(1), proposal.at(2), proposal.at(3));
                }

                /* Show error message if necessary: */
                if (!comServer.isOk())
                    msgCenter().cannotSaveDHCPServerParameter(comServer, this);
                else
                {
                    /* Update interface in the tree: */
                    UIDataHostNetwork data;
                    loadHostNetwork(comInterface, data);
                    updateItemForHostNetwork(data, true, pChangedItem);

                    /* Make sure current item fetched: */
                    sltHandleCurrentItemChangeHostNetwork();

                    /* Adjust tree-widgets: */
                    sltAdjustTreeWidgets();
                }
            }
        }
    }
}

void UINetworkManagerWidget::sltHandleCurrentItemChangeHostNetwork()
{
    /* Check host network tree-widget: */
    AssertMsgReturnVoid(m_pTreeWidgetHostNetwork, ("Host network tree-widget isn't created!\n"));

    /* Get network item: */
    UIItemHostNetwork *pItem = static_cast<UIItemHostNetwork*>(m_pTreeWidgetHostNetwork->currentItem());

    /* Update actions availability: */
    m_pActionPool->action(UIActionIndexMN_M_Network_S_Remove)->setEnabled(pItem);

    /* Check host network details-widget: */
    AssertMsgReturnVoid(m_pDetailsWidgetHostNetwork, ("Host network details-widget isn't created!\n"));

    /* If there is an item => update details data: */
    if (pItem)
        m_pDetailsWidgetHostNetwork->setData(*pItem);
    /* Otherwise => clear details: */
    else
        m_pDetailsWidgetHostNetwork->setData(UIDataHostNetwork());
}

void UINetworkManagerWidget::sltHandleContextMenuRequestHostNetwork(const QPoint &position)
{
    /* Check host network tree-widget: */
    AssertMsgReturnVoid(m_pTreeWidgetHostNetwork, ("Host network tree-widget isn't created!\n"));

    /* Compose temporary context-menu: */
    QMenu menu;
    if (m_pTreeWidgetHostNetwork->itemAt(position))
    {
        menu.addAction(m_pActionPool->action(UIActionIndexMN_M_Network_S_Remove));
        menu.addAction(m_pActionPool->action(UIActionIndexMN_M_Network_T_Details));
    }
    else
    {
        menu.addAction(m_pActionPool->action(UIActionIndexMN_M_Network_S_Create));
//        menu.addAction(m_pActionPool->action(UIActionIndexMN_M_Network_S_Refresh));
    }
    /* And show it: */
    menu.exec(m_pTreeWidgetHostNetwork->mapToGlobal(position));
}

void UINetworkManagerWidget::sltApplyDetailsChangesHostNetwork()
{
    /* Check host network tree-widget: */
    AssertMsgReturnVoid(m_pTreeWidgetHostNetwork, ("Host network tree-widget isn't created!\n"));

    /* Get host network item: */
    UIItemHostNetwork *pItem = static_cast<UIItemHostNetwork*>(m_pTreeWidgetHostNetwork->currentItem());
    AssertMsgReturnVoid(pItem, ("Current item must not be null!\n"));

    /* Check host network details-widget: */
    AssertMsgReturnVoid(m_pDetailsWidgetHostNetwork, ("Host network details-widget isn't created!\n"));

    /* Revalidate host network details: */
    if (m_pDetailsWidgetHostNetwork->revalidate())
    {
        /* Get item data: */
        UIDataHostNetwork oldData = *pItem;
        UIDataHostNetwork newData = m_pDetailsWidgetHostNetwork->data();

        /* Get host for further activities: */
        CHost comHost = uiCommon().host();

        /* Find corresponding interface: */
        CHostNetworkInterface comInterface = comHost.FindHostNetworkInterfaceByName(oldData.m_interface.m_strName);

        /* Show error message if necessary: */
        if (!comHost.isOk() || comInterface.isNull())
            msgCenter().cannotFindHostNetworkInterface(comHost, oldData.m_interface.m_strName, this);
        else
        {
            /* Save automatic interface configuration: */
            if (newData.m_interface.m_fDHCPEnabled)
            {
                if (   comInterface.isOk()
                    && !oldData.m_interface.m_fDHCPEnabled)
                    comInterface.EnableDynamicIPConfig();
            }
            /* Save manual interface configuration: */
            else
            {
                /* Save IPv4 interface configuration: */
                if (   comInterface.isOk()
                    && (   oldData.m_interface.m_fDHCPEnabled
                        || newData.m_interface.m_strAddress != oldData.m_interface.m_strAddress
                        || newData.m_interface.m_strMask != oldData.m_interface.m_strMask))
                    comInterface.EnableStaticIPConfig(newData.m_interface.m_strAddress, newData.m_interface.m_strMask);
                /* Save IPv6 interface configuration: */
                if (   comInterface.isOk()
                    && newData.m_interface.m_fSupportedIPv6
                    && (   oldData.m_interface.m_fDHCPEnabled
                        || newData.m_interface.m_strAddress6 != oldData.m_interface.m_strAddress6
                        || newData.m_interface.m_strPrefixLength6 != oldData.m_interface.m_strPrefixLength6))
                    comInterface.EnableStaticIPConfigV6(newData.m_interface.m_strAddress6, newData.m_interface.m_strPrefixLength6.toULong());
            }

            /* Show error message if necessary: */
            if (!comInterface.isOk())
                msgCenter().cannotSaveHostNetworkInterfaceParameter(comInterface, this);
            else
            {
                /* Get network name for further activities: */
                const QString strNetworkName = comInterface.GetNetworkName();

                /* Show error message if necessary: */
                if (!comInterface.isOk())
                    msgCenter().cannotAcquireHostNetworkInterfaceParameter(comInterface, this);
                else
                {
                    /* Get VBox for further activities: */
                    CVirtualBox comVBox = uiCommon().virtualBox();

                    /* Find corresponding DHCP server (create if necessary): */
                    CDHCPServer comServer = comVBox.FindDHCPServerByNetworkName(strNetworkName);
                    if (!comVBox.isOk() || comServer.isNull())
                        comServer = comVBox.CreateDHCPServer(strNetworkName);

                    /* Show error message if necessary: */
                    if (!comVBox.isOk() || comServer.isNull())
                        msgCenter().cannotCreateDHCPServer(comVBox, strNetworkName, this);
                    else
                    {
                        /* Save whether DHCP server is enabled: */
                        if (   comServer.isOk()
                            && newData.m_dhcpserver.m_fEnabled != oldData.m_dhcpserver.m_fEnabled)
                            comServer.SetEnabled(newData.m_dhcpserver.m_fEnabled);
                        /* Save DHCP server configuration: */
                        if (   comServer.isOk()
                            && newData.m_dhcpserver.m_fEnabled
                            && (   newData.m_dhcpserver.m_strAddress != oldData.m_dhcpserver.m_strAddress
                                || newData.m_dhcpserver.m_strMask != oldData.m_dhcpserver.m_strMask
                                || newData.m_dhcpserver.m_strLowerAddress != oldData.m_dhcpserver.m_strLowerAddress
                                || newData.m_dhcpserver.m_strUpperAddress != oldData.m_dhcpserver.m_strUpperAddress))
                            comServer.SetConfiguration(newData.m_dhcpserver.m_strAddress, newData.m_dhcpserver.m_strMask,
                                                       newData.m_dhcpserver.m_strLowerAddress, newData.m_dhcpserver.m_strUpperAddress);

                        /* Show error message if necessary: */
                        if (!comServer.isOk())
                            msgCenter().cannotSaveDHCPServerParameter(comServer, this);
                    }
                }
            }

            /* Find corresponding interface again (if necessary): */
            if (!comInterface.isOk())
            {
                comInterface = comHost.FindHostNetworkInterfaceByName(oldData.m_interface.m_strName);

                /* Show error message if necessary: */
                if (!comHost.isOk() || comInterface.isNull())
                    msgCenter().cannotFindHostNetworkInterface(comHost, oldData.m_interface.m_strName, this);
            }

            /* If interface is Ok now: */
            if (comInterface.isNotNull() && comInterface.isOk())
            {
                /* Update interface in the tree: */
                UIDataHostNetwork data;
                loadHostNetwork(comInterface, data);
                updateItemForHostNetwork(data, true, pItem);

                /* Make sure current item fetched: */
                sltHandleCurrentItemChangeHostNetwork();

                /* Adjust tree-widgets: */
                sltAdjustTreeWidgets();
            }
        }
    }

    /* Make sure button states updated: */
    m_pDetailsWidgetHostNetwork->updateButtonStates();
}

void UINetworkManagerWidget::sltHandleItemChangeNATNetwork(QTreeWidgetItem *pItem)
{
    /* Get network item: */
    UIItemNATNetwork *pChangedItem = static_cast<UIItemNATNetwork*>(pItem);
    AssertMsgReturnVoid(pChangedItem, ("Changed item must not be null!\n"));

    /* Get item data: */
    UIDataNATNetwork oldData = *pChangedItem;

    /* Make sure network availability status changed: */
    if (   (   oldData.m_fEnabled
            && pChangedItem->checkState(NATNetworkColumn_Availability) == Qt::Checked)
        || (   !oldData.m_fEnabled
            && pChangedItem->checkState(NATNetworkColumn_Availability) == Qt::Unchecked))
        return;

    /* Get VirtualBox for further activities: */
    CVirtualBox comVBox = uiCommon().virtualBox();

    /* Find corresponding network: */
    CNATNetwork comNetwork = comVBox.FindNATNetworkByName(oldData.m_strName);

    /* Show error message if necessary: */
    if (!comVBox.isOk() || comNetwork.isNull())
        msgCenter().cannotFindNATNetwork(comVBox, oldData.m_strName, this);
    else
    {
        /* Save whether NAT network is enabled: */
        if (comNetwork.isOk())
            comNetwork.SetEnabled(!oldData.m_fEnabled);

        /* Show error message if necessary: */
        if (!comNetwork.isOk())
            msgCenter().cannotSaveNATNetworkParameter(comNetwork, this);
        else
        {
            /* Update network in the tree: */
            UIDataNATNetwork data;
            loadNATNetwork(comNetwork, data);
            updateItemForNATNetwork(data, true, pChangedItem);

            /* Make sure current item fetched, trying to hold chosen position: */
            sltHandleCurrentItemChangeNATNetworkHoldingPosition(true /* hold position? */);

            /* Adjust tree-widgets: */
            sltAdjustTreeWidgets();
        }
    }
}

void UINetworkManagerWidget::sltHandleCurrentItemChangeNATNetworkHoldingPosition(bool fHoldPosition)
{
    /* Check NAT network tree-widget: */
    AssertMsgReturnVoid(m_pTreeWidgetNATNetwork, ("NAT network tree-widget isn't created!\n"));

    /* Get network item: */
    UIItemNATNetwork *pItem = static_cast<UIItemNATNetwork*>(m_pTreeWidgetNATNetwork->currentItem());

    /* Update actions availability: */
    m_pActionPool->action(UIActionIndexMN_M_Network_S_Remove)->setEnabled(pItem);

    /* Check NAT network details-widget: */
    AssertMsgReturnVoid(m_pDetailsWidgetNATNetwork, ("NAT network details-widget isn't created!\n"));

    /* If there is an item => update details data: */
    if (pItem)
    {
        QStringList busyNamesForItem = busyNames();
        busyNamesForItem.removeAll(pItem->name());
        m_pDetailsWidgetNATNetwork->setData(*pItem, busyNamesForItem, fHoldPosition);
    }
    /* Otherwise => clear details: */
    else
        m_pDetailsWidgetNATNetwork->setData(UIDataNATNetwork());
}

void UINetworkManagerWidget::sltHandleCurrentItemChangeNATNetwork()
{
    sltHandleCurrentItemChangeNATNetworkHoldingPosition(false /* hold position? */);
}

void UINetworkManagerWidget::sltHandleContextMenuRequestNATNetwork(const QPoint &position)
{
    /* Check NAT network tree-widget: */
    AssertMsgReturnVoid(m_pTreeWidgetNATNetwork, ("NAT network tree-widget isn't created!\n"));

    /* Compose temporary context-menu: */
    QMenu menu;
    if (m_pTreeWidgetNATNetwork->itemAt(position))
    {
        menu.addAction(m_pActionPool->action(UIActionIndexMN_M_Network_S_Remove));
        menu.addAction(m_pActionPool->action(UIActionIndexMN_M_Network_T_Details));
    }
    else
    {
        menu.addAction(m_pActionPool->action(UIActionIndexMN_M_Network_S_Create));
//        menu.addAction(m_pActionPool->action(UIActionIndexMN_M_Network_S_Refresh));
    }
    /* And show it: */
    menu.exec(m_pTreeWidgetNATNetwork->mapToGlobal(position));
}

void UINetworkManagerWidget::sltApplyDetailsChangesNATNetwork()
{
    /* Check NAT network tree-widget: */
    AssertMsgReturnVoid(m_pTreeWidgetNATNetwork, ("NAT network tree-widget isn't created!\n"));

    /* Get NAT network item: */
    UIItemNATNetwork *pItem = static_cast<UIItemNATNetwork*>(m_pTreeWidgetNATNetwork->currentItem());
    AssertMsgReturnVoid(pItem, ("Current item must not be null!\n"));

    /* Check NAT network details-widget: */
    AssertMsgReturnVoid(m_pDetailsWidgetNATNetwork, ("NAT network details-widget isn't created!\n"));

    /* Revalidate NAT network details: */
    if (m_pDetailsWidgetNATNetwork->revalidate())
    {
        /* Get item data: */
        UIDataNATNetwork oldData = *pItem;
        UIDataNATNetwork newData = m_pDetailsWidgetNATNetwork->data();

        /* Get VirtualBox for further activities: */
        CVirtualBox comVBox = uiCommon().virtualBox();

        /* Find corresponding network: */
        CNATNetwork comNetwork = comVBox.FindNATNetworkByName(oldData.m_strName);

        /* Show error message if necessary: */
        if (!comVBox.isOk() || comNetwork.isNull())
            msgCenter().cannotFindNATNetwork(comVBox, oldData.m_strName, this);
        else
        {
            /* Save whether NAT network is enabled: */
            if (comNetwork.isOk() && newData.m_fEnabled != oldData.m_fEnabled)
                comNetwork.SetEnabled(newData.m_fEnabled);
            /* Save NAT network name: */
            if (comNetwork.isOk() && newData.m_strName != oldData.m_strName)
                comNetwork.SetNetworkName(newData.m_strName);
            /* Save NAT network CIDR: */
            if (comNetwork.isOk() && newData.m_strCIDR != oldData.m_strCIDR)
                comNetwork.SetNetwork(newData.m_strCIDR);
            /* Save whether NAT network needs DHCP server: */
            if (comNetwork.isOk() && newData.m_fSupportsDHCP != oldData.m_fSupportsDHCP)
                comNetwork.SetNeedDhcpServer(newData.m_fSupportsDHCP);
            /* Save whether NAT network supports IPv6: */
            if (comNetwork.isOk() && newData.m_fSupportsIPv6 != oldData.m_fSupportsIPv6)
                comNetwork.SetIPv6Enabled(newData.m_fSupportsIPv6);
            /* Save whether NAT network should advertise default IPv6 route: */
            if (comNetwork.isOk() && newData.m_fAdvertiseDefaultIPv6Route != oldData.m_fAdvertiseDefaultIPv6Route)
                comNetwork.SetAdvertiseDefaultIPv6RouteEnabled(newData.m_fAdvertiseDefaultIPv6Route);

            /* Save IPv4 forwarding rules: */
            if (comNetwork.isOk() && newData.m_rules4 != oldData.m_rules4)
            {
                UIPortForwardingDataList oldRules = oldData.m_rules4;

                /* Remove rules to be removed: */
                foreach (const UIDataPortForwardingRule &oldRule, oldData.m_rules4)
                    if (comNetwork.isOk() && !newData.m_rules4.contains(oldRule))
                    {
                        comNetwork.RemovePortForwardRule(false /* IPv6? */, oldRule.name);
                        oldRules.removeAll(oldRule);
                    }
                /* Add rules to be added: */
                foreach (const UIDataPortForwardingRule &newRule, newData.m_rules4)
                    if (comNetwork.isOk() && !oldRules.contains(newRule))
                    {
                        comNetwork.AddPortForwardRule(false /* IPv6? */, newRule.name, newRule.protocol,
                                                      newRule.hostIp, newRule.hostPort.value(),
                                                      newRule.guestIp, newRule.guestPort.value());
                        oldRules.append(newRule);
                    }
            }
            /* Save IPv6 forwarding rules: */
            if (comNetwork.isOk() && newData.m_rules6 != oldData.m_rules6)
            {
                UIPortForwardingDataList oldRules = oldData.m_rules6;

                /* Remove rules to be removed: */
                foreach (const UIDataPortForwardingRule &oldRule, oldData.m_rules6)
                    if (comNetwork.isOk() && !newData.m_rules6.contains(oldRule))
                    {
                        comNetwork.RemovePortForwardRule(true /* IPv6? */, oldRule.name);
                        oldRules.removeAll(oldRule);
                    }
                /* Add rules to be added: */
                foreach (const UIDataPortForwardingRule &newRule, newData.m_rules6)
                    if (comNetwork.isOk() && !oldRules.contains(newRule))
                    {
                        comNetwork.AddPortForwardRule(true /* IPv6? */, newRule.name, newRule.protocol,
                                                      newRule.hostIp, newRule.hostPort.value(),
                                                      newRule.guestIp, newRule.guestPort.value());
                        oldRules.append(newRule);
                    }
            }

            /* Show error message if necessary: */
            if (!comNetwork.isOk())
                msgCenter().cannotSaveNATNetworkParameter(comNetwork, this);

            /* Update network in the tree: */
            UIDataNATNetwork data;
            loadNATNetwork(comNetwork, data);
            updateItemForNATNetwork(data, true, pItem);

            /* Make sure current item fetched, trying to hold chosen position: */
            sltHandleCurrentItemChangeNATNetworkHoldingPosition(true /* hold position? */);

            /* Adjust tree-widgets: */
            sltAdjustTreeWidgets();
        }
    }

    /* Make sure button states updated: */
    m_pDetailsWidgetNATNetwork->updateButtonStates();
}

void UINetworkManagerWidget::prepare()
{
    /* Prepare self: */
    uiCommon().setHelpKeyword(this, "networkingdetails");

    /* Prepare stuff: */
    prepareActions();
    prepareWidgets();

    /* Load settings: */
    loadSettings();

    /* Apply language settings: */
    retranslateUi();

    /* Load networks: */
    loadHostNetworks();
    loadNATNetworks();
}

void UINetworkManagerWidget::prepareActions()
{
    /* First of all, add actions which has smaller shortcut scope: */
    addAction(m_pActionPool->action(UIActionIndexMN_M_Network_S_Create));
    addAction(m_pActionPool->action(UIActionIndexMN_M_Network_S_Remove));
    addAction(m_pActionPool->action(UIActionIndexMN_M_Network_T_Details));
    addAction(m_pActionPool->action(UIActionIndexMN_M_Network_S_Refresh));

    /* Connect actions: */
    connect(m_pActionPool->action(UIActionIndexMN_M_Network_S_Create), &QAction::triggered,
            this, &UINetworkManagerWidget::sltCreateHostNetwork);
    connect(m_pActionPool->action(UIActionIndexMN_M_Network_S_Create), &QAction::triggered,
            this, &UINetworkManagerWidget::sltCreateNATNetwork);
    connect(m_pActionPool->action(UIActionIndexMN_M_Network_S_Remove), &QAction::triggered,
            this, &UINetworkManagerWidget::sltRemoveHostNetwork);
    connect(m_pActionPool->action(UIActionIndexMN_M_Network_S_Remove), &QAction::triggered,
            this, &UINetworkManagerWidget::sltRemoveNATNetwork);
    connect(m_pActionPool->action(UIActionIndexMN_M_Network_T_Details), &QAction::toggled,
            this, &UINetworkManagerWidget::sltToggleDetailsVisibility);
}

void UINetworkManagerWidget::prepareWidgets()
{
    /* Create main-layout: */
    new QVBoxLayout(this);
    if (layout())
    {
        /* Configure layout: */
        layout()->setContentsMargins(0, 0, 0, 0);
#ifdef VBOX_WS_MAC
        layout()->setSpacing(10);
#else
        layout()->setSpacing(qApp->style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing) / 2);
#endif

        /* Prepare toolbar, if requested: */
        if (m_fShowToolbar)
            prepareToolBar();

        /* Prepare tab-widget: */
        prepareTabWidget();

        /* Prepare details widgets: */
        prepareDetailsWidgetHostNetwork();
        prepareDetailsWidgetNATNetwork();
    }
}

void UINetworkManagerWidget::prepareToolBar()
{
    /* Prepare toolbar: */
    m_pToolBar = new QIToolBar(parentWidget());
    if (m_pToolBar)
    {
        const int iIconMetric = (int)(QApplication::style()->pixelMetric(QStyle::PM_LargeIconSize));
        m_pToolBar->setIconSize(QSize(iIconMetric, iIconMetric));
        m_pToolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndexMN_M_Network_S_Create));
        m_pToolBar->addSeparator();
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndexMN_M_Network_S_Remove));
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndexMN_M_Network_T_Details));

#ifdef VBOX_WS_MAC
        /* Check whether we are embedded into a stack: */
        if (m_enmEmbedding == EmbedTo_Stack)
        {
            /* Add into layout: */
            layout()->addWidget(m_pToolBar);
        }
#else
        /* Add into layout: */
        layout()->addWidget(m_pToolBar);
#endif
    }
}

void UINetworkManagerWidget::prepareTabWidget()
{
    /* Create tab-widget: */
    m_pTabWidget = new QITabWidget(this);
    if (m_pTabWidget)
    {
        connect(m_pTabWidget, &QITabWidget::currentChanged,
                this, &UINetworkManagerWidget::sltHandleCurrentTabWidgetIndexChange);

        prepareTabHostNetwork();
        prepareTabNATNetwork();

        /* Add into layout: */
        layout()->addWidget(m_pTabWidget);
    }
}

void UINetworkManagerWidget::prepareTabHostNetwork()
{
    /* Prepare host network tab: */
    m_pTabHostNetwork = new QWidget(m_pTabWidget);
    if (m_pTabHostNetwork)
    {
        /* Prepare host network layout: */
        m_pLayoutHostNetwork = new QVBoxLayout(m_pTabHostNetwork);
        if (m_pLayoutHostNetwork)
            prepareTreeWidgetHostNetwork();

        /* Add into tab-widget: */
        m_pTabWidget->insertTab(TabWidgetIndex_HostNetwork, m_pTabHostNetwork, QString());
    }
}

void UINetworkManagerWidget::prepareTreeWidgetHostNetwork()
{
    /* Prepare host network tree-widget: */
    m_pTreeWidgetHostNetwork = new QITreeWidget(m_pTabHostNetwork);
    if (m_pTreeWidgetHostNetwork)
    {
        m_pTreeWidgetHostNetwork->setRootIsDecorated(false);
        m_pTreeWidgetHostNetwork->setAlternatingRowColors(true);
        m_pTreeWidgetHostNetwork->setContextMenuPolicy(Qt::CustomContextMenu);
        m_pTreeWidgetHostNetwork->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_pTreeWidgetHostNetwork->setColumnCount(HostNetworkColumn_Max);
        m_pTreeWidgetHostNetwork->setSortingEnabled(true);
        m_pTreeWidgetHostNetwork->sortByColumn(HostNetworkColumn_Name, Qt::AscendingOrder);
        m_pTreeWidgetHostNetwork->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Expanding);
        connect(m_pTreeWidgetHostNetwork, &QITreeWidget::currentItemChanged,
                this, &UINetworkManagerWidget::sltHandleCurrentItemChangeHostNetwork);
        connect(m_pTreeWidgetHostNetwork, &QITreeWidget::customContextMenuRequested,
                this, &UINetworkManagerWidget::sltHandleContextMenuRequestHostNetwork);
        connect(m_pTreeWidgetHostNetwork, &QITreeWidget::itemChanged,
                this, &UINetworkManagerWidget::sltHandleItemChangeHostNetwork);
        connect(m_pTreeWidgetHostNetwork, &QITreeWidget::itemDoubleClicked,
                m_pActionPool->action(UIActionIndexMN_M_Network_T_Details), &QAction::setChecked);

        /* Add into layout: */
        m_pLayoutHostNetwork->addWidget(m_pTreeWidgetHostNetwork);
    }
}

void UINetworkManagerWidget::prepareDetailsWidgetHostNetwork()
{
    /* Prepare host network details-widget: */
    m_pDetailsWidgetHostNetwork = new UIDetailsWidgetHostNetwork(m_enmEmbedding, this);
    if (m_pDetailsWidgetHostNetwork)
    {
        m_pDetailsWidgetHostNetwork->setVisible(false);
        m_pDetailsWidgetHostNetwork->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
        connect(m_pDetailsWidgetHostNetwork, &UIDetailsWidgetHostNetwork::sigDataChanged,
                this, &UINetworkManagerWidget::sigDetailsDataChangedHostNetwork);
        connect(m_pDetailsWidgetHostNetwork, &UIDetailsWidgetHostNetwork::sigDataChangeRejected,
                this, &UINetworkManagerWidget::sltHandleCurrentItemChangeHostNetwork);
        connect(m_pDetailsWidgetHostNetwork, &UIDetailsWidgetHostNetwork::sigDataChangeAccepted,
                this, &UINetworkManagerWidget::sltApplyDetailsChangesHostNetwork);

        /* Add into layout: */
        layout()->addWidget(m_pDetailsWidgetHostNetwork);
    }
}

void UINetworkManagerWidget::prepareTabNATNetwork()
{
    /* Prepare NAT network tab: */
    m_pTabNATNetwork = new QWidget(m_pTabWidget);
    if (m_pTabNATNetwork)
    {
        /* Prepare NAT network layout: */
        m_pLayoutNATNetwork = new QVBoxLayout(m_pTabNATNetwork);
        if (m_pLayoutNATNetwork)
            prepareTreeWidgetNATNetwork();

        /* Add into tab-widget: */
        m_pTabWidget->insertTab(TabWidgetIndex_NATNetwork, m_pTabNATNetwork, QString());
    }
}

void UINetworkManagerWidget::prepareTreeWidgetNATNetwork()
{
    /* Prepare NAT network tree-widget: */
    m_pTreeWidgetNATNetwork = new QITreeWidget(m_pTabNATNetwork);
    if (m_pTreeWidgetNATNetwork)
    {
        m_pTreeWidgetNATNetwork->setRootIsDecorated(false);
        m_pTreeWidgetNATNetwork->setAlternatingRowColors(true);
        m_pTreeWidgetNATNetwork->setContextMenuPolicy(Qt::CustomContextMenu);
        m_pTreeWidgetNATNetwork->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_pTreeWidgetNATNetwork->setColumnCount(NATNetworkColumn_Max);
        m_pTreeWidgetNATNetwork->setSortingEnabled(true);
        m_pTreeWidgetNATNetwork->sortByColumn(NATNetworkColumn_Name, Qt::AscendingOrder);
        m_pTreeWidgetNATNetwork->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Expanding);
        connect(m_pTreeWidgetNATNetwork, &QITreeWidget::currentItemChanged,
                this, &UINetworkManagerWidget::sltHandleCurrentItemChangeNATNetwork);
        connect(m_pTreeWidgetNATNetwork, &QITreeWidget::customContextMenuRequested,
                this, &UINetworkManagerWidget::sltHandleContextMenuRequestNATNetwork);
        connect(m_pTreeWidgetNATNetwork, &QITreeWidget::itemChanged,
                this, &UINetworkManagerWidget::sltHandleItemChangeNATNetwork);
        connect(m_pTreeWidgetNATNetwork, &QITreeWidget::itemDoubleClicked,
                m_pActionPool->action(UIActionIndexMN_M_Network_T_Details), &QAction::setChecked);

        /* Add into layout: */
        m_pLayoutNATNetwork->addWidget(m_pTreeWidgetNATNetwork);
    }
}

void UINetworkManagerWidget::prepareDetailsWidgetNATNetwork()
{
    /* Prepare NAT network details-widget: */
    m_pDetailsWidgetNATNetwork = new UIDetailsWidgetNATNetwork(m_enmEmbedding, this);
    if (m_pDetailsWidgetNATNetwork)
    {
        m_pDetailsWidgetNATNetwork->setVisible(false);
        m_pDetailsWidgetNATNetwork->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
        connect(m_pDetailsWidgetNATNetwork, &UIDetailsWidgetNATNetwork::sigDataChanged,
                this, &UINetworkManagerWidget::sigDetailsDataChangedNATNetwork);
        connect(m_pDetailsWidgetNATNetwork, &UIDetailsWidgetNATNetwork::sigDataChangeRejected,
                this, &UINetworkManagerWidget::sltHandleCurrentItemChangeNATNetwork);
        connect(m_pDetailsWidgetNATNetwork, &UIDetailsWidgetNATNetwork::sigDataChangeAccepted,
                this, &UINetworkManagerWidget::sltApplyDetailsChangesNATNetwork);

        /* Add into layout: */
        layout()->addWidget(m_pDetailsWidgetNATNetwork);
    }
}

void UINetworkManagerWidget::loadSettings()
{
    /* Details action/widget: */
    if (m_pActionPool)
    {
        m_pActionPool->action(UIActionIndexMN_M_Network_T_Details)->setChecked(gEDataManager->hostNetworkManagerDetailsExpanded());
        sltToggleDetailsVisibility(m_pActionPool->action(UIActionIndexMN_M_Network_T_Details)->isChecked());
    }
}

void UINetworkManagerWidget::loadHostNetworks()
{
    /* Check host network tree-widget: */
    if (!m_pTreeWidgetHostNetwork)
        return;

    /* Clear tree first of all: */
    m_pTreeWidgetHostNetwork->clear();

    /* Get host for further activities: */
    const CHost comHost = uiCommon().host();

    /* Get interfaces for further activities: */
    const QVector<CHostNetworkInterface> interfaces = comHost.GetNetworkInterfaces();

    /* Show error message if necessary: */
    if (!comHost.isOk())
        msgCenter().cannotAcquireHostNetworkInterfaces(comHost, this);
    else
    {
        /* For each host-only interface => load it to the tree: */
        foreach (const CHostNetworkInterface &comInterface, interfaces)
            if (comInterface.GetInterfaceType() == KHostNetworkInterfaceType_HostOnly)
            {
                UIDataHostNetwork data;
                loadHostNetwork(comInterface, data);
                createItemForHostNetwork(data, false);
            }

        /* Choose the 1st item as current initially: */
        m_pTreeWidgetHostNetwork->setCurrentItem(m_pTreeWidgetHostNetwork->topLevelItem(0));
        sltHandleCurrentItemChangeHostNetwork();

        /* Adjust tree-widgets: */
        sltAdjustTreeWidgets();
    }
}

void UINetworkManagerWidget::loadHostNetwork(const CHostNetworkInterface &comInterface, UIDataHostNetwork &data)
{
    /* Gather interface settings: */
    if (comInterface.isNotNull())
        data.m_interface.m_fExists = true;
    if (comInterface.isOk())
        data.m_interface.m_strName = comInterface.GetName();
    if (comInterface.isOk())
        data.m_interface.m_fDHCPEnabled = comInterface.GetDHCPEnabled();
    if (comInterface.isOk())
        data.m_interface.m_strAddress = comInterface.GetIPAddress();
    if (comInterface.isOk())
        data.m_interface.m_strMask = comInterface.GetNetworkMask();
    if (comInterface.isOk())
        data.m_interface.m_fSupportedIPv6 = comInterface.GetIPV6Supported();
    if (comInterface.isOk())
        data.m_interface.m_strAddress6 = comInterface.GetIPV6Address();
    if (comInterface.isOk())
        data.m_interface.m_strPrefixLength6 = QString::number(comInterface.GetIPV6NetworkMaskPrefixLength());

    /* Get host interface network name for further activities: */
    QString strNetworkName;
    if (comInterface.isOk())
        strNetworkName = comInterface.GetNetworkName();

    /* Show error message if necessary: */
    if (!comInterface.isOk())
        msgCenter().cannotAcquireHostNetworkInterfaceParameter(comInterface, this);

    /* Get VBox for further activities: */
    CVirtualBox comVBox = uiCommon().virtualBox();

    /* Find corresponding DHCP server (create if necessary): */
    CDHCPServer comServer = comVBox.FindDHCPServerByNetworkName(strNetworkName);
    if (!comVBox.isOk() || comServer.isNull())
        comServer = comVBox.CreateDHCPServer(strNetworkName);

    /* Show error message if necessary: */
    if (!comVBox.isOk() || comServer.isNull())
        msgCenter().cannotCreateDHCPServer(comVBox, strNetworkName, this);
    else
    {
        /* Gather DHCP server settings: */
        if (comServer.isOk())
            data.m_dhcpserver.m_fEnabled = comServer.GetEnabled();
        if (comServer.isOk())
            data.m_dhcpserver.m_strAddress = comServer.GetIPAddress();
        if (comServer.isOk())
            data.m_dhcpserver.m_strMask = comServer.GetNetworkMask();
        if (comServer.isOk())
            data.m_dhcpserver.m_strLowerAddress = comServer.GetLowerIP();
        if (comServer.isOk())
            data.m_dhcpserver.m_strUpperAddress = comServer.GetUpperIP();

        /* Show error message if necessary: */
        if (!comServer.isOk())
            return msgCenter().cannotAcquireDHCPServerParameter(comServer, this);
    }
}

void UINetworkManagerWidget::loadNATNetworks()
{
    /* Check NAT network tree-widget: */
    if (!m_pTreeWidgetNATNetwork)
        return;

    /* Clear tree first of all: */
    m_pTreeWidgetNATNetwork->clear();

    /* Get VirtualBox for further activities: */
    const CVirtualBox comVBox = uiCommon().virtualBox();

    /* Get interfaces for further activities: */
    const QVector<CNATNetwork> networks = comVBox.GetNATNetworks();

    /* Show error message if necessary: */
    if (!comVBox.isOk())
        msgCenter().cannotAcquireNATNetworks(comVBox, this);
    else
    {
        /* For each NAT network => load it to the tree: */
        foreach (const CNATNetwork &comNetwork, networks)
        {
            UIDataNATNetwork data;
            loadNATNetwork(comNetwork, data);
            createItemForNATNetwork(data, false);
        }

        /* Choose the 1st item as current initially: */
        m_pTreeWidgetNATNetwork->setCurrentItem(m_pTreeWidgetNATNetwork->topLevelItem(0));
        sltHandleCurrentItemChangeNATNetwork();

        /* Adjust tree-widgets: */
        sltAdjustTreeWidgets();
    }
}

void UINetworkManagerWidget::loadNATNetwork(const CNATNetwork &comNetwork, UIDataNATNetwork &data)
{
    /* Gather network settings: */
    if (comNetwork.isNotNull())
        data.m_fExists = true;
    if (comNetwork.isOk())
        data.m_fEnabled = comNetwork.GetEnabled();
    if (comNetwork.isOk())
        data.m_strName = comNetwork.GetNetworkName();
    if (comNetwork.isOk())
        data.m_strCIDR = comNetwork.GetNetwork();
    if (comNetwork.isOk())
        data.m_fSupportsDHCP = comNetwork.GetNeedDhcpServer();
    if (comNetwork.isOk())
        data.m_fSupportsIPv6 = comNetwork.GetIPv6Enabled();
    if (comNetwork.isOk())
        data.m_fAdvertiseDefaultIPv6Route = comNetwork.GetAdvertiseDefaultIPv6RouteEnabled();

    /* Gather forwarding rules: */
    if (comNetwork.isOk())
    {
        /* Load IPv4 rules: */
        foreach (QString strIPv4Rule, comNetwork.GetPortForwardRules4())
        {
            /* Replace all ':' with ',' first: */
            strIPv4Rule.replace(':', ',');
            /* Parse rules: */
            QStringList rules = strIPv4Rule.split(',');
            Assert(rules.size() == 6);
            if (rules.size() != 6)
                continue;
            data.m_rules4 << UIDataPortForwardingRule(rules.at(0),
                                                      gpConverter->fromInternalString<KNATProtocol>(rules.at(1)),
                                                      QString(rules.at(2)).remove('[').remove(']'),
                                                      rules.at(3).toUInt(),
                                                      QString(rules.at(4)).remove('[').remove(']'),
                                                      rules.at(5).toUInt());
        }

        /* Load IPv6 rules: */
        foreach (QString strIPv6Rule, comNetwork.GetPortForwardRules6())
        {
            /* Replace all ':' with ',' first: */
            strIPv6Rule.replace(':', ',');
            /* But replace ',' back with ':' for addresses: */
            QRegExp re("\\[[0-9a-fA-F,]*,[0-9a-fA-F,]*\\]");
            re.setMinimal(true);
            while (re.indexIn(strIPv6Rule) != -1)
            {
                QString strCapOld = re.cap(0);
                QString strCapNew = strCapOld;
                strCapNew.replace(',', ':');
                strIPv6Rule.replace(strCapOld, strCapNew);
            }
            /* Parse rules: */
            QStringList rules = strIPv6Rule.split(',');
            Assert(rules.size() == 6);
            if (rules.size() != 6)
                continue;
            data.m_rules6 << UIDataPortForwardingRule(rules.at(0),
                                                      gpConverter->fromInternalString<KNATProtocol>(rules.at(1)),
                                                      QString(rules.at(2)).remove('[').remove(']'),
                                                      rules.at(3).toUInt(),
                                                      QString(rules.at(4)).remove('[').remove(']'),
                                                      rules.at(5).toUInt());
        }
    }

    /* Show error message if necessary: */
    if (!comNetwork.isOk())
        msgCenter().cannotAcquireNATNetworkParameter(comNetwork, this);
}

void UINetworkManagerWidget::createItemForHostNetwork(const UIDataHostNetwork &data, bool fChooseItem)
{
    /* Create new item: */
    UIItemHostNetwork *pItem = new UIItemHostNetwork;
    if (pItem)
    {
        /* Configure item: */
        pItem->UIDataHostNetwork::operator=(data);
        pItem->updateFields();
        /* Add item to the tree: */
        m_pTreeWidgetHostNetwork->addTopLevelItem(pItem);
        /* And choose it as current if necessary: */
        if (fChooseItem)
            m_pTreeWidgetHostNetwork->setCurrentItem(pItem);
    }
}

void UINetworkManagerWidget::updateItemForHostNetwork(const UIDataHostNetwork &data, bool fChooseItem, UIItemHostNetwork *pItem)
{
    /* Update passed item: */
    if (pItem)
    {
        /* Configure item: */
        pItem->UIDataHostNetwork::operator=(data);
        pItem->updateFields();
        /* And choose it as current if necessary: */
        if (fChooseItem)
            m_pTreeWidgetHostNetwork->setCurrentItem(pItem);
    }
}

void UINetworkManagerWidget::createItemForNATNetwork(const UIDataNATNetwork &data, bool fChooseItem)
{
    /* Create new item: */
    UIItemNATNetwork *pItem = new UIItemNATNetwork;
    if (pItem)
    {
        /* Configure item: */
        pItem->UIDataNATNetwork::operator=(data);
        pItem->updateFields();
        /* Add item to the tree: */
        m_pTreeWidgetNATNetwork->addTopLevelItem(pItem);
        /* And choose it as current if necessary: */
        if (fChooseItem)
            m_pTreeWidgetNATNetwork->setCurrentItem(pItem);
    }
}

void UINetworkManagerWidget::updateItemForNATNetwork(const UIDataNATNetwork &data, bool fChooseItem, UIItemNATNetwork *pItem)
{
    /* Update passed item: */
    if (pItem)
    {
        /* Configure item: */
        pItem->UIDataNATNetwork::operator=(data);
        pItem->updateFields();
        /* And choose it as current if necessary: */
        if (fChooseItem)
            m_pTreeWidgetNATNetwork->setCurrentItem(pItem);
    }
}

QStringList UINetworkManagerWidget::busyNames() const
{
    QStringList names;
    for (int i = 0; i < m_pTreeWidgetNATNetwork->topLevelItemCount(); ++i)
    {
        UIItemNATNetwork *pItem = qobject_cast<UIItemNATNetwork*>(m_pTreeWidgetNATNetwork->childItem(i));
        const QString strItemName(pItem->name());
        if (!strItemName.isEmpty() && !names.contains(strItemName))
            names << strItemName;
    }
    return names;
}


/*********************************************************************************************************************************
*   Class UINetworkManagerFactory implementation.                                                                                *
*********************************************************************************************************************************/

UINetworkManagerFactory::UINetworkManagerFactory(UIActionPool *pActionPool /* = 0 */)
    : m_pActionPool(pActionPool)
{
}

void UINetworkManagerFactory::create(QIManagerDialog *&pDialog, QWidget *pCenterWidget)
{
    pDialog = new UINetworkManager(pCenterWidget, m_pActionPool);
}


/*********************************************************************************************************************************
*   Class UINetworkManager implementation.                                                                                       *
*********************************************************************************************************************************/

UINetworkManager::UINetworkManager(QWidget *pCenterWidget, UIActionPool *pActionPool)
    : QIWithRetranslateUI<QIManagerDialog>(pCenterWidget)
    , m_pActionPool(pActionPool)
{
}

void UINetworkManager::sltHandleButtonBoxClick(QAbstractButton *pButton)
{
    /* Disable buttons first of all: */
    button(ButtonType_Reset)->setEnabled(false);
    button(ButtonType_Apply)->setEnabled(false);

    /* Compare with known buttons: */
    if (pButton == button(ButtonType_Reset))
        emit sigDataChangeRejected();
    else
    if (pButton == button(ButtonType_Apply))
        emit sigDataChangeAccepted();
}

void UINetworkManager::retranslateUi()
{
    /* Translate window title: */
    setWindowTitle(tr("Network Manager"));

    /* Translate buttons: */
    button(ButtonType_Reset)->setText(tr("Reset"));
    button(ButtonType_Apply)->setText(tr("Apply"));
    button(ButtonType_Close)->setText(tr("Close"));
    button(ButtonType_Help)->setText(tr("Help"));
    button(ButtonType_Reset)->setStatusTip(tr("Reset changes in current network details"));
    button(ButtonType_Apply)->setStatusTip(tr("Apply changes in current network details"));
    button(ButtonType_Close)->setStatusTip(tr("Close dialog without saving"));
    button(ButtonType_Help)->setStatusTip(tr("Show dialog help"));
    button(ButtonType_Reset)->setShortcut(QString("Ctrl+Backspace"));
    button(ButtonType_Apply)->setShortcut(QString("Ctrl+Return"));
    button(ButtonType_Close)->setShortcut(Qt::Key_Escape);
    button(ButtonType_Help)->setShortcut(QKeySequence::HelpContents);
    button(ButtonType_Reset)->setToolTip(tr("Reset Changes (%1)").arg(button(ButtonType_Reset)->shortcut().toString()));
    button(ButtonType_Apply)->setToolTip(tr("Apply Changes (%1)").arg(button(ButtonType_Apply)->shortcut().toString()));
    button(ButtonType_Close)->setToolTip(tr("Close Window (%1)").arg(button(ButtonType_Close)->shortcut().toString()));
    button(ButtonType_Help)->setToolTip(tr("Show Help (%1)").arg(button(ButtonType_Help)->shortcut().toString()));
}

void UINetworkManager::configure()
{
    /* Apply window icons: */
    setWindowIcon(UIIconPool::iconSetFull(":/host_iface_manager_32px.png", ":/host_iface_manager_16px.png"));
}

void UINetworkManager::configureCentralWidget()
{
    /* Prepare widget: */
    UINetworkManagerWidget *pWidget = new UINetworkManagerWidget(EmbedTo_Dialog, m_pActionPool, true, this);
    if (pWidget)
    {
        setWidget(pWidget);
        setWidgetMenu(pWidget->menu());
#ifdef VBOX_WS_MAC
        setWidgetToolbar(pWidget->toolbar());
#endif
        connect(this, &UINetworkManager::sigDataChangeRejected,
                pWidget, &UINetworkManagerWidget::sltResetDetailsChanges);
        connect(this, &UINetworkManager::sigDataChangeAccepted,
                pWidget, &UINetworkManagerWidget::sltApplyDetailsChanges);

        /* Add into layout: */
        centralWidget()->layout()->addWidget(pWidget);
    }
}

void UINetworkManager::configureButtonBox()
{
    /* Configure button-box: */
    connect(widget(), &UINetworkManagerWidget::sigDetailsVisibilityChanged,
            button(ButtonType_Apply), &QPushButton::setVisible);
    connect(widget(), &UINetworkManagerWidget::sigDetailsVisibilityChanged,
            button(ButtonType_Reset), &QPushButton::setVisible);
    connect(widget(), &UINetworkManagerWidget::sigDetailsDataChangedHostNetwork,
            button(ButtonType_Apply), &QPushButton::setEnabled);
    connect(widget(), &UINetworkManagerWidget::sigDetailsDataChangedHostNetwork,
            button(ButtonType_Reset), &QPushButton::setEnabled);
    connect(widget(), &UINetworkManagerWidget::sigDetailsDataChangedNATNetwork,
            button(ButtonType_Apply), &QPushButton::setEnabled);
    connect(widget(), &UINetworkManagerWidget::sigDetailsDataChangedNATNetwork,
            button(ButtonType_Reset), &QPushButton::setEnabled);
    connect(buttonBox(), &QIDialogButtonBox::clicked,
            this, &UINetworkManager::sltHandleButtonBoxClick);
    // WORKAROUND:
    // Since we connected signals later than extra-data loaded
    // for signals above, we should handle that stuff here again:
    button(ButtonType_Apply)->setVisible(gEDataManager->hostNetworkManagerDetailsExpanded());
    button(ButtonType_Reset)->setVisible(gEDataManager->hostNetworkManagerDetailsExpanded());
}

void UINetworkManager::finalize()
{
    /* Apply language settings: */
    retranslateUi();
}

UINetworkManagerWidget *UINetworkManager::widget()
{
    return qobject_cast<UINetworkManagerWidget*>(QIManagerDialog::widget());
}


#include "UINetworkManager.moc"
