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

#include "UISettingsPage.h"
#include "UIGlobalSettingsNetwork.gen.h"

/* Host interface data: */
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
};

/* DHCP server data: */
struct UIDHCPServerData
{
    /* DHCP Server */
    bool m_fDhcpServerEnabled;
    QString m_strDhcpServerAddress;
    QString m_strDhcpServerMask;
    QString m_strDhcpLowerAddress;
    QString m_strDhcpUpperAddress;
};

/* Global network data: */
struct UIHostNetworkData
{
    UIHostInterfaceData m_interface;
    UIDHCPServerData m_dhcpserver;
};

/* Global settings / Network page / Cache: */
struct UISettingsCacheGlobalNetwork
{
    QList<UIHostNetworkData> m_items;
};

class NetworkItem : public QTreeWidgetItem
{
public:

    NetworkItem();

    void fetchNetworkData(const UIHostNetworkData &data);
    void uploadNetworkData(UIHostNetworkData &data);

    bool revalidate (QString &aWarning, QString &aTitle);

    QString updateInfo();

    /* Page getters */
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

    /* Page setters */
    void setDhcpClientEnabled (bool aEnabled) { m_data.m_interface.m_fDhcpClientEnabled = aEnabled; }
    void setInterfaceAddress (const QString &aValue) { m_data.m_interface.m_strInterfaceAddress = aValue; }
    void setInterfaceMask (const QString &aValue) { m_data.m_interface.m_strInterfaceMask = aValue; }
    void setIp6Supported (bool aSupported) { m_data.m_interface.m_fIpv6Supported = aSupported; }
    void setInterfaceAddress6 (const QString &aValue) { m_data.m_interface.m_strInterfaceAddress6 = aValue; }
    void setInterfaceMaskLength6 (const QString &aValue) { m_data.m_interface.m_strInterfaceMaskLength6 = aValue; }

    void setDhcpServerEnabled (bool aEnabled) { m_data.m_dhcpserver.m_fDhcpServerEnabled = aEnabled; }
    void setDhcpServerAddress (const QString &aValue) { m_data.m_dhcpserver.m_strDhcpServerAddress = aValue; }
    void setDhcpServerMask (const QString &aValue) { m_data.m_dhcpserver.m_strDhcpServerMask = aValue; }
    void setDhcpLowerAddress (const QString &aValue) { m_data.m_dhcpserver.m_strDhcpLowerAddress = aValue; }
    void setDhcpUpperAddress (const QString &aValue) { m_data.m_dhcpserver.m_strDhcpUpperAddress = aValue; }

private:

    /* Network data: */
    UIHostNetworkData m_data;
};

/* Global settings / Network page: */
class UIGlobalSettingsNetwork : public UISettingsPageGlobal,
                              public Ui::UIGlobalSettingsNetwork
{
    Q_OBJECT;

public:

    UIGlobalSettingsNetwork();

protected:

    /* Load data to cashe from corresponding external object(s),
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

    void setValidator (QIWidgetValidator *aVal);
    bool revalidate (QString &aWarning, QString &aTitle);

    void setOrderAfter (QWidget *aWidget);

    void retranslateUi();

private slots:

    void addInterface();
    void remInterface();
    void editInterface();
    void updateCurrentItem();
    void showContextMenu (const QPoint &aPos);

private:

    QIWidgetValidator *mValidator;

    QAction *mAddInterface;
    QAction *mRemInterface;
    QAction *mEditInterface;

    bool m_fChanged;

    /* Cache: */
    UISettingsCacheGlobalNetwork m_cache;
};

#endif // __UIGlobalSettingsNetwork_h__

