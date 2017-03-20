/* $Id: $ */
/** @file
 * VBox Qt GUI - UIGlobalSettingsNetworkDetailsHost class declaration.
 */

/*
 * Copyright (C) 2009-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIGlobalSettingsNetworkDetailsHost_h__
#define __UIGlobalSettingsNetworkDetailsHost_h__

/* GUI includes: */
#include "QIDialog.h"
#include "QIWithRetranslateUI.h"
#include "UIGlobalSettingsNetworkDetailsHost.gen.h"


/** Global settings: Network page: Host interface data structure. */
struct UIDataSettingsGlobalNetworkHostInterface
{
    /** Constructs data. */
    UIDataSettingsGlobalNetworkHostInterface()
        : m_strName(QString())
        , m_fDhcpClientEnabled(false)
        , m_strInterfaceAddress(QString())
        , m_strInterfaceMask(QString())
        , m_fIpv6Supported(false)
        , m_strInterfaceAddress6(QString())
        , m_strInterfaceMaskLength6(QString())
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool equal(const UIDataSettingsGlobalNetworkHostInterface &other) const
    {
        return true
               && (m_strName == other.m_strName)
               && (m_fDhcpClientEnabled == other.m_fDhcpClientEnabled)
               && (m_strInterfaceAddress == other.m_strInterfaceAddress)
               && (m_strInterfaceMask == other.m_strInterfaceMask)
               && (m_fIpv6Supported == other.m_fIpv6Supported)
               && (m_strInterfaceAddress6 == other.m_strInterfaceAddress6)
               && (m_strInterfaceMaskLength6 == other.m_strInterfaceMaskLength6)
               ;
    }

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataSettingsGlobalNetworkHostInterface &other) const { return equal(other); }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataSettingsGlobalNetworkHostInterface &other) const { return !equal(other); }

    /** Holds host interface name. */
    QString m_strName;
    /** Holds whether DHCP client enabled. */
    bool m_fDhcpClientEnabled;
    /** Holds IPv4 interface address. */
    QString m_strInterfaceAddress;
    /** Holds IPv4 interface mask. */
    QString m_strInterfaceMask;
    /** Holds whether IPv6 protocol supported. */
    bool m_fIpv6Supported;
    /** Holds IPv6 interface address. */
    QString m_strInterfaceAddress6;
    /** Holds IPv6 interface mask length. */
    QString m_strInterfaceMaskLength6;
};


/** Global settings: Network page: DHCP server data structure. */
struct UIDataSettingsGlobalNetworkDHCPServer
{
    /** Constructs data. */
    UIDataSettingsGlobalNetworkDHCPServer()
        : m_fDhcpServerEnabled(false)
        , m_strDhcpServerAddress(QString())
        , m_strDhcpServerMask(QString())
        , m_strDhcpLowerAddress(QString())
        , m_strDhcpUpperAddress(QString())
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool equal(const UIDataSettingsGlobalNetworkDHCPServer &other) const
    {
        return true
               && (m_fDhcpServerEnabled == other.m_fDhcpServerEnabled)
               && (m_strDhcpServerAddress == other.m_strDhcpServerAddress)
               && (m_strDhcpServerMask == other.m_strDhcpServerMask)
               && (m_strDhcpLowerAddress == other.m_strDhcpLowerAddress)
               && (m_strDhcpUpperAddress == other.m_strDhcpUpperAddress)
               ;
    }

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataSettingsGlobalNetworkDHCPServer &other) const { return equal(other); }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataSettingsGlobalNetworkDHCPServer &other) const { return !equal(other); }

    /** Holds whether DHCP server enabled. */
    bool m_fDhcpServerEnabled;
    /** Holds DHCP server address. */
    QString m_strDhcpServerAddress;
    /** Holds DHCP server mask. */
    QString m_strDhcpServerMask;
    /** Holds DHCP server lower address. */
    QString m_strDhcpLowerAddress;
    /** Holds DHCP server upper address. */
    QString m_strDhcpUpperAddress;
};


/** Global settings: Network page: Host network data structure. */
struct UIDataSettingsGlobalNetworkHost
{
    /** Constructs data. */
    UIDataSettingsGlobalNetworkHost()
        : m_interface(UIDataSettingsGlobalNetworkHostInterface())
        , m_dhcpserver(UIDataSettingsGlobalNetworkDHCPServer())
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool equal(const UIDataSettingsGlobalNetworkHost &other) const
    {
        return true
               && (m_interface == other.m_interface)
               && (m_dhcpserver == other.m_dhcpserver)
               ;
    }

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataSettingsGlobalNetworkHost &other) const { return equal(other); }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataSettingsGlobalNetworkHost &other) const { return !equal(other); }

    /** Holds the host interface data. */
    UIDataSettingsGlobalNetworkHostInterface m_interface;
    /** Holds the DHCP server data. */
    UIDataSettingsGlobalNetworkDHCPServer m_dhcpserver;
};


/* Global settings / Network page / Details sub-dialog: */
class UIGlobalSettingsNetworkDetailsHost : public QIWithRetranslateUI2<QIDialog>, public Ui::UIGlobalSettingsNetworkDetailsHost
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGlobalSettingsNetworkDetailsHost(QWidget *pParent, UIDataSettingsGlobalNetworkHost &data);

protected:

    /* Handler: Translation stuff: */
    void retranslateUi();

private slots:

    /* Handlers: [Re]load stuff: */
    void sltDhcpClientStatusChanged() { loadClientStuff(); }
    void sltDhcpServerStatusChanged() { loadServerStuff(); }

    /* Handler: Dialog stuff: */
    void accept();

private:

    /* Helpers: Load/Save stuff: */
    void load();
    void loadClientStuff();
    void loadServerStuff();
    void save();

    /** Converts IPv4 address from QString to quint32. */
    static quint32 ipv4FromQStringToQuint32(const QString &strAddress);
    /** Converts IPv4 address from quint32 to QString. */
    static QString ipv4FromQuint32ToQString(quint32 uAddress);

    /* Variable: External data reference: */
    UIDataSettingsGlobalNetworkHost &m_data;
};

#endif /* __UIGlobalSettingsNetworkDetailsHost_h__ */
