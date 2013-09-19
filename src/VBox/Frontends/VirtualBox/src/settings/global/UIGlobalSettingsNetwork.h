/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIGlobalSettingsNetwork class declaration
 */

/*
 * Copyright (C) 2009-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIGlobalSettingsNetwork_h__
#define __UIGlobalSettingsNetwork_h__

/* GUI includes: */
#include "UISettingsPage.h"
#include "UIGlobalSettingsNetwork.gen.h"
#include "UIPortForwardingTable.h"

/* Forward declarations: */
class UIItemNetworkNAT;
class UIItemNetworkHost;


/* Global settings / Network page / NAT network data: */
struct UIDataNetworkNAT
{
    /* NAT Network: */
    bool m_fEnabled;
    QString m_strName;
    QString m_strNewName;
    QString m_strCIDR;
    bool m_fSupportsDHCP;
    bool m_fSupportsIPv6;
    bool m_fAdvertiseDefaultIPv6Route;
    UIPortForwardingDataList m_ipv4rules;
    UIPortForwardingDataList m_ipv6rules;
    bool operator==(const UIDataNetworkNAT &other) const
    {
        return m_fEnabled == other.m_fEnabled &&
               m_strName == other.m_strName &&
               m_strNewName == other.m_strNewName &&
               m_strCIDR == other.m_strCIDR &&
               m_fSupportsDHCP == other.m_fSupportsDHCP &&
               m_fSupportsIPv6 == other.m_fSupportsIPv6 &&
               m_fAdvertiseDefaultIPv6Route == other.m_fAdvertiseDefaultIPv6Route &&
               m_ipv4rules == other.m_ipv4rules &&
               m_ipv6rules == other.m_ipv6rules;
    }
};


/* Global settings / Network page / Host interface data: */
struct UIDataNetworkHostInterface
{
    /* Host Interface: */
    QString m_strName;
    bool m_fDhcpClientEnabled;
    QString m_strInterfaceAddress;
    QString m_strInterfaceMask;
    bool m_fIpv6Supported;
    QString m_strInterfaceAddress6;
    QString m_strInterfaceMaskLength6;
    bool operator==(const UIDataNetworkHostInterface &other) const
    {
        return m_strName == other.m_strName &&
               m_fDhcpClientEnabled == other.m_fDhcpClientEnabled &&
               m_strInterfaceAddress == other.m_strInterfaceAddress &&
               m_strInterfaceMask == other.m_strInterfaceMask &&
               m_fIpv6Supported == other.m_fIpv6Supported &&
               m_strInterfaceAddress6 == other.m_strInterfaceAddress6 &&
               m_strInterfaceMaskLength6 == other.m_strInterfaceMaskLength6;
    }
};

/* Global settings / Network page / Host DHCP server data: */
struct UIDataNetworkDHCPServer
{
    /* DHCP Server: */
    bool m_fDhcpServerEnabled;
    QString m_strDhcpServerAddress;
    QString m_strDhcpServerMask;
    QString m_strDhcpLowerAddress;
    QString m_strDhcpUpperAddress;
    bool operator==(const UIDataNetworkDHCPServer &other) const
    {
        return m_fDhcpServerEnabled == other.m_fDhcpServerEnabled &&
               m_strDhcpServerAddress == other.m_strDhcpServerAddress &&
               m_strDhcpServerMask == other.m_strDhcpServerMask &&
               m_strDhcpLowerAddress == other.m_strDhcpLowerAddress &&
               m_strDhcpUpperAddress == other.m_strDhcpUpperAddress;
    }
};

/* Global settings / Network page / Host network data: */
struct UIDataNetworkHost
{
    UIDataNetworkHostInterface m_interface;
    UIDataNetworkDHCPServer m_dhcpserver;
    bool operator==(const UIDataNetworkHost &other) const
    {
        return m_interface == other.m_interface &&
               m_dhcpserver == other.m_dhcpserver;
    }
};


/* Global settings / Network page / Global network cache: */
struct UISettingsCacheGlobalNetwork
{
    QList<UIDataNetworkNAT> m_networksNAT;
    QList<UIDataNetworkHost> m_networksHost;
};


/* Global settings / Network page: */
class UIGlobalSettingsNetwork : public UISettingsPageGlobal, public Ui::UIGlobalSettingsNetwork
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGlobalSettingsNetwork();

protected:

    /* API:
     * Load data to cache from corresponding external object(s),
     * this task COULD be performed in other than GUI thread: */
    void loadToCacheFrom(QVariant &data);
    /* API:
     * Load data to corresponding widgets from cache,
     * this task SHOULD be performed in GUI thread only: */
    void getFromCache();

    /* API:
     * Save data from corresponding widgets to cache,
     * this task SHOULD be performed in GUI thread only: */
    void putToCache();
    /* API:
     * Save data from cache to corresponding external object(s),
     * this task COULD be performed in other than GUI thread: */
    void saveFromCacheTo(QVariant &data);

    /* API: Validation stuff: */
    bool validate(QList<UIValidationMessage> &messages);

    /* API: Navigation stuff: */
    void setOrderAfter(QWidget *pWidget);

    /* API: Translation stuff: */
    void retranslateUi();

private slots:

    /* Handlers: NAT network stuff: */
    void sltAddNetworkNAT();
    void sltDelNetworkNAT();
    void sltEditNetworkNAT();
    void sltHandleItemChangeNetworkNAT(QTreeWidgetItem *pChangedItem);
    void sltHandleCurrentItemChangeNetworkNAT();
    void sltShowContextMenuNetworkNAT(const QPoint &pos);

    /* Handlers: Host network stuff: */
    void sltAddNetworkHost();
    void sltDelNetworkHost();
    void sltEditNetworkHost();
    void sltHandleCurrentItemChangeNetworkHost();
    void sltShowContextMenuNetworkHost(const QPoint &pos);

private:

    /* Helpers: NAT network cache stuff: */
    UIDataNetworkNAT generateDataNetworkNAT(const CNATNetwork &network);
    void saveCacheItemNetworkNAT(const UIDataNetworkNAT &data);

    /* Helpers: NAT network tree stuff: */
    void createTreeItemNetworkNAT(const UIDataNetworkNAT &data, bool fChooseItem = false);
    void removeTreeItemNetworkNAT(UIItemNetworkNAT *pItem);

    /* Helpers: Host network cache stuff: */
    UIDataNetworkHost generateDataNetworkHost(const CHostNetworkInterface &iface);
    void saveCacheItemNetworkHost(const UIDataNetworkHost &data);

    /* Helpers: Host network tree stuff: */
    void createTreeItemNetworkHost(const UIDataNetworkHost &data, bool fChooseItem = false);
    void removeTreeItemNetworkHost(UIItemNetworkHost *pItem);

    /* Variables: NAT network actions: */
    QAction *m_pActionAddNetworkNAT;
    QAction *m_pActionDelNetworkNAT;
    QAction *m_pActionEditNetworkNAT;

    /* Variables: Host network actions: */
    QAction *m_pActionAddNetworkHost;
    QAction *m_pActionDelNetworkHost;
    QAction *m_pActionEditNetworkHost;

    /* Variable: Editness flag: */
    bool m_fChanged;

    /* Variable: Cache: */
    UISettingsCacheGlobalNetwork m_cache;
};

#endif // __UIGlobalSettingsNetwork_h__
