/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIGlobalSettingsNetwork class declaration
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

#ifndef __UIGlobalSettingsNetwork_h__
#define __UIGlobalSettingsNetwork_h__

/* Local includes */
#include "UISettingsPage.h"
#include "UIGlobalSettingsNetwork.gen.h"

/* Global settings / Network page / Host interface data: */
struct UIHostInterfaceData
{
    /* Host-only Interface: */
    QString m_strName;
    bool m_fDhcpClientEnabled;
    QString m_strInterfaceAddress;
    QString m_strInterfaceMask;
    bool m_fIpv6Supported;
    QString m_strInterfaceAddress6;
    QString m_strInterfaceMaskLength6;
    bool operator==(const UIHostInterfaceData &other) const
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

/* Global settings / Network page / DHCP server data: */
struct UIDHCPServerData
{
    /* DHCP Server */
    bool m_fDhcpServerEnabled;
    QString m_strDhcpServerAddress;
    QString m_strDhcpServerMask;
    QString m_strDhcpLowerAddress;
    QString m_strDhcpUpperAddress;
    bool operator==(const UIDHCPServerData &other) const
    {
        return m_fDhcpServerEnabled == other.m_fDhcpServerEnabled &&
               m_strDhcpServerAddress == other.m_strDhcpServerAddress &&
               m_strDhcpServerMask == other.m_strDhcpServerMask &&
               m_strDhcpLowerAddress == other.m_strDhcpLowerAddress &&
               m_strDhcpUpperAddress == other.m_strDhcpUpperAddress;
    }
};

/* Global settings / Network page / Full network data: */
struct UIHostNetworkData
{
    UIHostInterfaceData m_interface;
    UIDHCPServerData m_dhcpserver;
    bool operator==(const UIHostNetworkData &other) const
    {
        return m_interface == other.m_interface &&
               m_dhcpserver == other.m_dhcpserver;
    }
};

/* Global settings / Network page / Cache: */
struct UISettingsCacheGlobalNetwork
{
    QList<UIHostNetworkData> m_items;
};

/* Global settings / Network page / Host interface item: */
class UIHostInterfaceItem : public QTreeWidgetItem
{
public:

    /* Constructor: */
    UIHostInterfaceItem();

    /* Get/return data to/form items: */
    void fetchNetworkData(const UIHostNetworkData &data);
    void uploadNetworkData(UIHostNetworkData &data);

    /* Validation stuff: */
    bool revalidate(QString &strWarning, QString &strTitle);

    /* Helpers: */
    QString updateInfo();

    /* Network item getters: */
    QString name() const { return m_data.m_interface.m_strName; }
    bool isDhcpClientEnabled() const { return m_data.m_interface.m_fDhcpClientEnabled; }
    QString interfaceAddress() const { return m_data.m_interface.m_strInterfaceAddress; }
    QString interfaceMask() const { return m_data.m_interface.m_strInterfaceMask; }
    bool isIpv6Supported() const { return m_data.m_interface.m_fIpv6Supported; }
    QString interfaceAddress6() const { return m_data.m_interface.m_strInterfaceAddress6; }
    QString interfaceMaskLength6() const { return m_data.m_interface.m_strInterfaceMaskLength6; }

    bool isDhcpServerEnabled() const { return m_data.m_dhcpserver.m_fDhcpServerEnabled; }
    QString dhcpServerAddress() const { return m_data.m_dhcpserver.m_strDhcpServerAddress; }
    QString dhcpServerMask() const { return m_data.m_dhcpserver.m_strDhcpServerMask; }
    QString dhcpLowerAddress() const { return m_data.m_dhcpserver.m_strDhcpLowerAddress; }
    QString dhcpUpperAddress() const { return m_data.m_dhcpserver.m_strDhcpUpperAddress; }

    /* Network item setters */
    void setDhcpClientEnabled(bool fEnabled) { m_data.m_interface.m_fDhcpClientEnabled = fEnabled; }
    void setInterfaceAddress (const QString &strValue) { m_data.m_interface.m_strInterfaceAddress = strValue; }
    void setInterfaceMask (const QString &strValue) { m_data.m_interface.m_strInterfaceMask = strValue; }
    void setIp6Supported (bool fSupported) { m_data.m_interface.m_fIpv6Supported = fSupported; }
    void setInterfaceAddress6 (const QString &strValue) { m_data.m_interface.m_strInterfaceAddress6 = strValue; }
    void setInterfaceMaskLength6 (const QString &strValue) { m_data.m_interface.m_strInterfaceMaskLength6 = strValue; }

    void setDhcpServerEnabled (bool fEnabled) { m_data.m_dhcpserver.m_fDhcpServerEnabled = fEnabled; }
    void setDhcpServerAddress (const QString &sttValue) { m_data.m_dhcpserver.m_strDhcpServerAddress = sttValue; }
    void setDhcpServerMask (const QString &strValue) { m_data.m_dhcpserver.m_strDhcpServerMask = strValue; }
    void setDhcpLowerAddress (const QString &strValue) { m_data.m_dhcpserver.m_strDhcpLowerAddress = strValue; }
    void setDhcpUpperAddress (const QString &strValue) { m_data.m_dhcpserver.m_strDhcpUpperAddress = strValue; }

private:

    /* Network data: */
    UIHostNetworkData m_data;
};

/* Global settings / Network page: */
class UIGlobalSettingsNetwork : public UISettingsPageGlobal, public Ui::UIGlobalSettingsNetwork
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGlobalSettingsNetwork();

protected:

    /* Load data to cache from corresponding external object(s),
     * this task COULD be performed in other than GUI thread: */
    void loadToCacheFrom(QVariant &data);
    /* Load data to corresponding widgets from cache,
     * this task SHOULD be performed in GUI thread only: */
    void getFromCache();

    /* Save data from corresponding widgets to cache,
     * this task SHOULD be performed in GUI thread only: */
    void putToCache();
    /* Save data from cache to corresponding external object(s),
     * this task COULD be performed in other than GUI thread: */
    void saveFromCacheTo(QVariant &data);

    /* Validation stuff: */
    void setValidator(QIWidgetValidator *pValidator);
    bool revalidate(QString &strWarning, QString &strTitle);

    /* Navigation stuff: */
    void setOrderAfter (QWidget *aWidget);

    /* Translation stuff: */
    void retranslateUi();

private slots:

    /* Helper slots: */
    void sltAddInterface();
    void sltDelInterface();
    void sltEditInterface();
    void sltUpdateCurrentItem();
    void sltChowContextMenu(const QPoint &pos);

private:

    /* Helper members: */
    void appendCacheItem(const CHostNetworkInterface &iface);
    void removeCacheItem(const QString &strInterfaceName);
    void appendListItem(const UIHostNetworkData &data, bool fChooseItem = false);
    void removeListItem(UIHostInterfaceItem *pItem);

    /* Validator: */
    QIWidgetValidator *m_pValidator;

    /* Helper actions: */
    QAction *m_pAddAction;
    QAction *m_pDelAction;
    QAction *m_pEditAction;

    /* Editness flag: */
    bool m_fChanged;

    /* Cache: */
    UISettingsCacheGlobalNetwork m_cache;
};

#endif // __UIGlobalSettingsNetwork_h__

