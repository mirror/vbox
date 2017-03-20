/* $Id$ */
/** @file
 * VBox Qt GUI - UIGlobalSettingsNetwork class implementation.
 */

/*
 * Copyright (C) 2009-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QHeaderView>

/* GUI includes: */
# include "VBoxGlobal.h"
# include "UIIconPool.h"
# include "UIConverter.h"
# include "UIMessageCenter.h"
# include "UIGlobalSettingsNetwork.h"
# include "UIGlobalSettingsNetworkDetailsNAT.h"
# include "UIGlobalSettingsNetworkDetailsHost.h"

/* COM includes: */
# include "CNATNetwork.h"
# include "CHostNetworkInterface.h"

/* Other VBox includes: */
# include <iprt/cidr.h>

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* COM includes: */
#include "CDHCPServer.h"


/** Global settings: Network page data structure. */
struct UIDataSettingsGlobalNetwork
{
    /** Constructs data. */
    UIDataSettingsGlobalNetwork()
        : m_networksNAT(QList<UIDataSettingsGlobalNetworkNAT>())
        , m_networksHost(QList<UIDataSettingsGlobalNetworkHost>())
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool equal(const UIDataSettingsGlobalNetwork &other) const
    {
        return true
               && (m_networksNAT == other.m_networksNAT)
               && (m_networksHost == other.m_networksHost)
               ;
    }

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataSettingsGlobalNetwork &other) const { return equal(other); }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataSettingsGlobalNetwork &other) const { return !equal(other); }

    /** Holds the NAT network data. */
    QList<UIDataSettingsGlobalNetworkNAT> m_networksNAT;
    /** Holds the host network data. */
    QList<UIDataSettingsGlobalNetworkHost> m_networksHost;
};


/* Global settings / Network page / NAT network item: */
class UIItemNetworkNAT : public QITreeWidgetItem
{
public:

    /* Constructor: */
    UIItemNetworkNAT();

    /* API: Get/return data to/form items: */
    void fetchNetworkData(const UIDataSettingsGlobalNetworkNAT &data);
    void uploadNetworkData(UIDataSettingsGlobalNetworkNAT &data);

    /** Performs validation, updates @a messages list if something is wrong. */
    bool validate(UIValidationMessage &messages);

    /* API: Update stuff: */
    void updateInfo();
    void updateData();

    /* API: Network item getters: */
    QString name() const { return m_data.m_strName; }
    QString newName() const { return m_data.m_strNewName; }

    /** Returns default text. */
    virtual QString defaultText() const /* override */;

private:

    /* Variable: Network data: */
    UIDataSettingsGlobalNetworkNAT m_data;
};


/* Global settings / Network page / Host network item: */
class UIItemNetworkHost : public QITreeWidgetItem
{
public:

    /* Constructor: */
    UIItemNetworkHost();

    /* API: Get/return data to/form items: */
    void fetchNetworkData(const UIDataSettingsGlobalNetworkHost &data);
    void uploadNetworkData(UIDataSettingsGlobalNetworkHost &data);

    /** Performs validation, updates @a messages list if something is wrong. */
    bool validate(UIValidationMessage &messages);

    /* API: Update stuff: */
    void updateInfo();

    /* API: Network item getters: */
    QString name() const { return m_data.m_interface.m_strName; }

private:

    /* Variable: Network data: */
    UIDataSettingsGlobalNetworkHost m_data;
};


UIItemNetworkNAT::UIItemNetworkNAT()
    : QITreeWidgetItem()
{
}

void UIItemNetworkNAT::fetchNetworkData(const UIDataSettingsGlobalNetworkNAT &data)
{
    /* Get from cache: */
    m_data = data;

    /* Fetch info: */
    updateInfo();
}

void UIItemNetworkNAT::uploadNetworkData(UIDataSettingsGlobalNetworkNAT &data)
{
    /* Put to cache: */
    data = m_data;
}

bool UIItemNetworkNAT::validate(UIValidationMessage &message)
{
    /* Pass by default: */
    bool fPass = true;

    /* NAT network name validation: */
    bool fNameValid = true;
    if (m_data.m_strNewName.isEmpty())
    {
        /* Emptiness validation: */
        message.second << UIGlobalSettingsNetwork::tr("No new name specified for the NAT network previously called <b>%1</b>.").arg(m_data.m_strName);
        fNameValid = false;
        fPass = false;
    }

    /* NAT network CIDR validation: */
    if (m_data.m_strCIDR.isEmpty())
    {
        /* Emptiness validation: */
        if (fNameValid)
            message.second << UIGlobalSettingsNetwork::tr("No CIDR specified for the NAT network <b>%1</b>.").arg(m_data.m_strNewName);
        else
            message.second << UIGlobalSettingsNetwork::tr("No CIDR specified for the NAT network previously called <b>%1</b>.").arg(m_data.m_strName);
        fPass = false;
    }
    else
    {
        /* Correctness validation: */
        RTNETADDRIPV4 network, mask;
        int rc = RTCidrStrToIPv4(m_data.m_strCIDR.toUtf8().constData(), &network, &mask);
        if (RT_FAILURE(rc))
        {
            if (fNameValid)
                message.second << UIGlobalSettingsNetwork::tr("Invalid CIDR specified (<i>%1</i>) for the NAT network <b>%2</b>.")
                                                              .arg(m_data.m_strCIDR, m_data.m_strNewName);
            else
                message.second << UIGlobalSettingsNetwork::tr("Invalid CIDR specified (<i>%1</i>) for the NAT network previously called <b>%2</b>.")
                                                              .arg(m_data.m_strCIDR, m_data.m_strName);
            fPass = false;
        }
    }

    /* Return result: */
    return fPass;
}

void UIItemNetworkNAT::updateInfo()
{
    /* Compose item name/tool-tip: */
    QString strHeader("<tr><td><nobr>%1:&nbsp;</nobr></td><td><nobr>%2</nobr></td></tr>");
    QString strSubHeader("<tr><td><nobr>&nbsp;&nbsp;%1:&nbsp;</nobr></td><td><nobr>%2</nobr></td></tr>");
    QString strToolTip;

    /* Item name was not changed: */
    setCheckState(0, m_data.m_fEnabled ? Qt::Checked : Qt::Unchecked);
    if (m_data.m_strNewName == m_data.m_strName)
    {
        /* Just use the old one: */
        setText(1, m_data.m_strName);
        strToolTip += strHeader.arg(UIGlobalSettingsNetwork::tr("Network Name"), m_data.m_strName);
    }
    /* If name was changed: */
    else
    {
        /* We should explain that: */
        const QString oldName = m_data.m_strName;
        const QString newName = m_data.m_strNewName.isEmpty() ? UIGlobalSettingsNetwork::tr("[empty]") : m_data.m_strNewName;
        setText(1, UIGlobalSettingsNetwork::tr("%1 (renamed from %2)").arg(newName, oldName));
        strToolTip += strHeader.arg(UIGlobalSettingsNetwork::tr("Old Network Name"), m_data.m_strName);
        strToolTip += strHeader.arg(UIGlobalSettingsNetwork::tr("New Network Name"), m_data.m_strNewName);
    }

    /* Other tool-tip information: */
    strToolTip += strHeader.arg(UIGlobalSettingsNetwork::tr("Network CIDR"), m_data.m_strCIDR);
    strToolTip += strHeader.arg(UIGlobalSettingsNetwork::tr("Supports DHCP"),
                                m_data.m_fSupportsDHCP ? UIGlobalSettingsNetwork::tr("yes") : UIGlobalSettingsNetwork::tr("no"));
    strToolTip += strHeader.arg(UIGlobalSettingsNetwork::tr("Supports IPv6"),
                                m_data.m_fSupportsIPv6 ? UIGlobalSettingsNetwork::tr("yes") : UIGlobalSettingsNetwork::tr("no"));
    if (m_data.m_fSupportsIPv6 && m_data.m_fAdvertiseDefaultIPv6Route)
        strToolTip += strSubHeader.arg(UIGlobalSettingsNetwork::tr("Default IPv6 route"), UIGlobalSettingsNetwork::tr("yes"));

    /* Assign tool-tip finally: */
    setToolTip(1, strToolTip);
}

void UIItemNetworkNAT::updateData()
{
    /* Update data: */
    m_data.m_fEnabled = checkState(0) == Qt::Checked;
}

QString UIItemNetworkNAT::defaultText() const
{
    return m_data.m_fEnabled ?
           tr("%1, %2", "col.2 text, col.1 name")
             .arg(text(1))
             .arg(parentTree()->headerItem()->text(0)) :
           tr("%1",     "col.2 text")
             .arg(text(1));
}


UIItemNetworkHost::UIItemNetworkHost()
    : QITreeWidgetItem()
{
}

void UIItemNetworkHost::fetchNetworkData(const UIDataSettingsGlobalNetworkHost &data)
{
    /* Get from cache: */
    m_data = data;

    /* Fetch info: */
    updateInfo();
}

void UIItemNetworkHost::uploadNetworkData(UIDataSettingsGlobalNetworkHost &data)
{
    /* Put to cache: */
    data = m_data;
}

bool UIItemNetworkHost::validate(UIValidationMessage &message)
{
    /* Pass by default: */
    bool fPass = true;

    /* Host interface validation: */
    if (!m_data.m_interface.m_fDhcpClientEnabled)
    {
        if (!m_data.m_interface.m_strInterfaceAddress.trimmed().isEmpty() &&
            (   !RTNetIsIPv4AddrStr(m_data.m_interface.m_strInterfaceAddress.toUtf8().constData())
             || RTNetStrIsIPv4AddrAny(m_data.m_interface.m_strInterfaceAddress.toUtf8().constData())))
        {
            message.second << UIGlobalSettingsNetwork::tr("Host interface <b>%1</b> does not currently have a valid IPv4 address.").arg(text(0));
            fPass = false;
        }
        if (!m_data.m_interface.m_strInterfaceMask.trimmed().isEmpty() &&
            (   !RTNetIsIPv4AddrStr(m_data.m_interface.m_strInterfaceMask.toUtf8().constData())
             || RTNetStrIsIPv4AddrAny(m_data.m_interface.m_strInterfaceMask.toUtf8().constData())))
        {
            message.second << UIGlobalSettingsNetwork::tr("Host interface <b>%1</b> does not currently have a valid IPv4 network mask.").arg(text(0));
            fPass = false;
        }
        if (m_data.m_interface.m_fIpv6Supported)
        {
            if (!m_data.m_interface.m_strInterfaceAddress6.trimmed().isEmpty() &&
                (   !RTNetIsIPv6AddrStr(m_data.m_interface.m_strInterfaceAddress6.toUtf8().constData())
                 || RTNetStrIsIPv6AddrAny(m_data.m_interface.m_strInterfaceAddress6.toUtf8().constData())))
            {
                message.second << UIGlobalSettingsNetwork::tr("Host interface <b>%1</b> does not currently have a valid IPv6 address.").arg(text(0));
                fPass = false;
            }
            bool fIsMaskPrefixLengthNumber = false;
            const int iMaskPrefixLength = m_data.m_interface.m_strInterfaceMaskLength6.trimmed().toInt(&fIsMaskPrefixLengthNumber);
            if (   !fIsMaskPrefixLengthNumber
                || iMaskPrefixLength < 0
                || iMaskPrefixLength > 128)
            {
                message.second << UIGlobalSettingsNetwork::tr("Host interface <b>%1</b> does not currently have a valid IPv6 network mask prefix length.").arg(text(0));
                fPass = false;
            }
        }
    }

    /* DHCP server validation: */
    if (m_data.m_dhcpserver.m_fDhcpServerEnabled)
    {
        if (   !RTNetIsIPv4AddrStr(m_data.m_dhcpserver.m_strDhcpServerAddress.toUtf8().constData())
            || RTNetStrIsIPv4AddrAny(m_data.m_dhcpserver.m_strDhcpServerAddress.toUtf8().constData()))
        {
            message.second << UIGlobalSettingsNetwork::tr("Host interface <b>%1</b> does not currently have a valid DHCP server address.").arg(text(0));
            fPass = false;
        }
        if (   !RTNetIsIPv4AddrStr(m_data.m_dhcpserver.m_strDhcpServerMask.toUtf8().constData())
            || RTNetStrIsIPv4AddrAny(m_data.m_dhcpserver.m_strDhcpServerMask.toUtf8().constData()))
        {
            message.second << UIGlobalSettingsNetwork::tr("Host interface <b>%1</b> does not currently have a valid DHCP server mask.").arg(text(0));
            fPass = false;
        }
        if (   !RTNetIsIPv4AddrStr(m_data.m_dhcpserver.m_strDhcpLowerAddress.toUtf8().constData())
            || RTNetStrIsIPv4AddrAny(m_data.m_dhcpserver.m_strDhcpLowerAddress.toUtf8().constData()))
        {
            message.second << UIGlobalSettingsNetwork::tr("Host interface <b>%1</b> does not currently have a valid DHCP server lower address bound.").arg(text(0));
            fPass = false;
        }
        if (   !RTNetIsIPv4AddrStr(m_data.m_dhcpserver.m_strDhcpUpperAddress.toUtf8().constData())
            || RTNetStrIsIPv4AddrAny(m_data.m_dhcpserver.m_strDhcpUpperAddress.toUtf8().constData()))
        {
            message.second << UIGlobalSettingsNetwork::tr("Host interface <b>%1</b> does not currently have a valid DHCP server upper address bound.").arg(text(0));
            fPass = false;
        }
    }

    /* Return result: */
    return fPass;
}

void UIItemNetworkHost::updateInfo()
{
    /* Compose item name/tool-tip: */
    setText(0, m_data.m_interface.m_strName);
    QString strHeader("<tr><td><nobr>%1:&nbsp;</nobr></td><td><nobr>%2</nobr></td></tr>");
    QString strSubHeader("<tr><td><nobr>&nbsp;&nbsp;%1:&nbsp;</nobr></td><td><nobr>%2</nobr></td></tr>");
    QString strToolTip;

    /* Host interface information: */
    strToolTip += strHeader.arg(UIGlobalSettingsNetwork::tr("Adapter"))
                           .arg(m_data.m_interface.m_fDhcpClientEnabled ?
                                UIGlobalSettingsNetwork::tr("Automatically configured", "interface") :
                                UIGlobalSettingsNetwork::tr("Manually configured", "interface"));
    if (!m_data.m_interface.m_fDhcpClientEnabled)
    {
        strToolTip += strSubHeader.arg(UIGlobalSettingsNetwork::tr("IPv4 Address"))
                                  .arg(m_data.m_interface.m_strInterfaceAddress.isEmpty() ?
                                       UIGlobalSettingsNetwork::tr ("Not set", "address") :
                                       m_data.m_interface.m_strInterfaceAddress) +
                      strSubHeader.arg(UIGlobalSettingsNetwork::tr("IPv4 Network Mask"))
                                  .arg(m_data.m_interface.m_strInterfaceMask.isEmpty() ?
                                       UIGlobalSettingsNetwork::tr ("Not set", "mask") :
                                       m_data.m_interface.m_strInterfaceMask);
        if (m_data.m_interface.m_fIpv6Supported)
        {
            strToolTip += strSubHeader.arg(UIGlobalSettingsNetwork::tr("IPv6 Address"))
                                      .arg(m_data.m_interface.m_strInterfaceAddress6.isEmpty() ?
                                           UIGlobalSettingsNetwork::tr("Not set", "address") :
                                           m_data.m_interface.m_strInterfaceAddress6) +
                          strSubHeader.arg(UIGlobalSettingsNetwork::tr("IPv6 Network Mask Length"))
                                      .arg(m_data.m_interface.m_strInterfaceMaskLength6.isEmpty() ?
                                           UIGlobalSettingsNetwork::tr("Not set", "length") :
                                           m_data.m_interface.m_strInterfaceMaskLength6);
        }
    }

    /* DHCP server information: */
    strToolTip += strHeader.arg(UIGlobalSettingsNetwork::tr("DHCP Server"))
                           .arg(m_data.m_dhcpserver.m_fDhcpServerEnabled ?
                                UIGlobalSettingsNetwork::tr("Enabled", "server") :
                                UIGlobalSettingsNetwork::tr("Disabled", "server"));
    if (m_data.m_dhcpserver.m_fDhcpServerEnabled)
    {
        strToolTip += strSubHeader.arg(UIGlobalSettingsNetwork::tr("Address"))
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
    }

    /* Assign tool-tip finally: */
    setToolTip(0, strToolTip);
}


UIGlobalSettingsNetwork::UIGlobalSettingsNetwork()
    : m_pActionAddNetworkNAT(0), m_pActionDelNetworkNAT(0), m_pActionEditNetworkNAT(0)
    , m_pActionAddNetworkHost(0), m_pActionDelNetworkHost(0), m_pActionEditNetworkHost(0)
    , m_pCache(new UISettingsCacheGlobalNetwork)
{
    /* Apply UI decorations: */
    Ui::UIGlobalSettingsNetwork::setupUi(this);

    /* Prepare NAT network tree-widget: */
    {
        m_pTreeNetworkNAT->setColumnCount(2);
        m_pTreeNetworkNAT->header()->setStretchLastSection(false);
#if QT_VERSION >= 0x050000
        m_pTreeNetworkNAT->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        m_pTreeNetworkNAT->header()->setSectionResizeMode(1, QHeaderView::Stretch);
#else /* QT_VERSION < 0x050000 */
        m_pTreeNetworkNAT->header()->setResizeMode(0, QHeaderView::ResizeToContents);
        m_pTreeNetworkNAT->header()->setResizeMode(1, QHeaderView::Stretch);
#endif /* QT_VERSION < 0x050000 */
        m_pTreeNetworkNAT->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_pTreeNetworkNAT, SIGNAL(currentItemChanged(QTreeWidgetItem*, QTreeWidgetItem*)),
                this, SLOT(sltHandleCurrentItemChangeNetworkNAT()));
        connect(m_pTreeNetworkNAT, SIGNAL(customContextMenuRequested(const QPoint&)),
                this, SLOT(sltShowContextMenuNetworkNAT(const QPoint&)));
        connect(m_pTreeNetworkNAT, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)),
                this, SLOT(sltEditNetworkNAT()));
        connect(m_pTreeNetworkNAT, SIGNAL(itemChanged(QTreeWidgetItem*, int)),
                this, SLOT(sltHandleItemChangeNetworkNAT(QTreeWidgetItem*)));
    }
    /* Prepare Host network tree-widget: */
    {
        m_pTreeNetworkHost->header()->hide();
        m_pTreeNetworkHost->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_pTreeNetworkHost, SIGNAL(currentItemChanged(QTreeWidgetItem*, QTreeWidgetItem*)),
                this, SLOT(sltHandleCurrentItemChangeNetworkHost()));
        connect(m_pTreeNetworkHost, SIGNAL(customContextMenuRequested(const QPoint&)),
                this, SLOT(sltShowContextMenuNetworkHost(const QPoint&)));
        connect(m_pTreeNetworkHost, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)),
                this, SLOT(sltEditNetworkHost()));
    }

    /* Prepare actions: */
    m_pActionAddNetworkNAT = new QAction(m_pTreeNetworkNAT);
    {
        m_pActionAddNetworkNAT->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        m_pActionAddNetworkNAT->setShortcuts(QList<QKeySequence>() << QKeySequence("Ins") << QKeySequence("Ctrl+N"));
        m_pActionAddNetworkNAT->setIcon(UIIconPool::iconSet(":/add_host_iface_16px.png",
                                                            ":/add_host_iface_disabled_16px.png"));
        connect(m_pActionAddNetworkNAT, SIGNAL(triggered(bool)), this, SLOT(sltAddNetworkNAT()));
    }
    m_pActionDelNetworkNAT = new QAction(m_pTreeNetworkNAT);
    {
        m_pActionDelNetworkNAT->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        m_pActionDelNetworkNAT->setShortcuts(QList<QKeySequence>() << QKeySequence("Del") << QKeySequence("Ctrl+R"));
        m_pActionDelNetworkNAT->setIcon(UIIconPool::iconSet(":/remove_host_iface_16px.png",
                                                            ":/remove_host_iface_disabled_16px.png"));
        connect(m_pActionDelNetworkNAT, SIGNAL(triggered(bool)), this, SLOT(sltDelNetworkNAT()));
    }
    m_pActionEditNetworkNAT = new QAction(m_pTreeNetworkNAT);
    {
        m_pActionEditNetworkNAT->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        m_pActionEditNetworkNAT->setShortcuts(QList<QKeySequence>() << QKeySequence("Space") << QKeySequence("F2"));
        m_pActionEditNetworkNAT->setIcon(UIIconPool::iconSet(":/guesttools_16px.png",
                                                             ":/guesttools_disabled_16px.png"));
        connect(m_pActionEditNetworkNAT, SIGNAL(triggered(bool)), this, SLOT(sltEditNetworkNAT()));
    }
    m_pActionAddNetworkHost = new QAction(m_pTreeNetworkHost);
    {
        m_pActionAddNetworkHost->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        m_pActionAddNetworkHost->setShortcuts(QList<QKeySequence>() << QKeySequence("Ins") << QKeySequence("Ctrl+N"));
        m_pActionAddNetworkHost->setIcon(UIIconPool::iconSet(":/add_host_iface_16px.png",
                                                             ":/add_host_iface_disabled_16px.png"));
        connect(m_pActionAddNetworkHost, SIGNAL(triggered(bool)), this, SLOT(sltAddNetworkHost()));
    }
    m_pActionDelNetworkHost = new QAction(m_pTreeNetworkHost);
    {
        m_pActionDelNetworkHost->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        m_pActionDelNetworkHost->setShortcuts(QList<QKeySequence>() << QKeySequence("Del") << QKeySequence("Ctrl+R"));
        m_pActionDelNetworkHost->setIcon(UIIconPool::iconSet(":/remove_host_iface_16px.png",
                                                             ":/remove_host_iface_disabled_16px.png"));
        connect(m_pActionDelNetworkHost, SIGNAL(triggered(bool)), this, SLOT(sltDelNetworkHost()));
    }
    m_pActionEditNetworkHost = new QAction(m_pTreeNetworkHost);
    {
        m_pActionEditNetworkHost->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        m_pActionEditNetworkHost->setShortcuts(QList<QKeySequence>() << QKeySequence("Space") << QKeySequence("F2"));
        m_pActionEditNetworkHost->setIcon(UIIconPool::iconSet(":/guesttools_16px.png",
                                                              ":/guesttools_disabled_16px.png"));
        connect(m_pActionEditNetworkHost, SIGNAL(triggered(bool)), this, SLOT(sltEditNetworkHost()));
    }

    /* Determine icon metric: */
    const QStyle *pStyle = QApplication::style();
    const int iIconMetric = pStyle->pixelMetric(QStyle::PM_SmallIconSize);

    /* Prepare NAT network toolbar: */
    {
        m_pToolbarNetworkNAT->setIconSize(QSize(iIconMetric, iIconMetric));
        m_pToolbarNetworkNAT->setOrientation(Qt::Vertical);
        m_pToolbarNetworkNAT->addAction(m_pActionAddNetworkNAT);
        m_pToolbarNetworkNAT->addAction(m_pActionDelNetworkNAT);
        m_pToolbarNetworkNAT->addAction(m_pActionEditNetworkNAT);
    }
    /* Prepare Host network toolbar: */
    {
        m_pToolbarNetworkHost->setIconSize(QSize(iIconMetric, iIconMetric));
        m_pToolbarNetworkHost->setOrientation(Qt::Vertical);
        m_pToolbarNetworkHost->addAction(m_pActionAddNetworkHost);
        m_pToolbarNetworkHost->addAction(m_pActionDelNetworkHost);
        m_pToolbarNetworkHost->addAction(m_pActionEditNetworkHost);
    }

#ifdef VBOX_WS_MAC
    /* On macOS we can do a bit of smoothness: */
    m_pLayoutNAT->setContentsMargins(0, 0, 0, 0);
    m_pLayoutHostOnly->setContentsMargins(0, 0, 0, 0);
#endif

    /* Apply language settings: */
    retranslateUi();
}

UIGlobalSettingsNetwork::~UIGlobalSettingsNetwork()
{
    /* Cleanup cache: */
    delete m_pCache;
    m_pCache = 0;
}

void UIGlobalSettingsNetwork::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to properties & settings: */
    UISettingsPageGlobal::fetchData(data);

    /* Clear cache initially: */
    m_pCache->clear();

    /* Prepare old data: */
    UIDataSettingsGlobalNetwork oldData;

    /* Gather old data: */
    foreach (const CNATNetwork &network, vboxGlobal().virtualBox().GetNATNetworks())
    {
        UIDataSettingsGlobalNetworkNAT data;
        generateDataNetworkNAT(network, data);
        oldData.m_networksNAT << data;
    }
    foreach (const CHostNetworkInterface &iface, vboxGlobal().host().GetNetworkInterfaces())
        if (iface.GetInterfaceType() == KHostNetworkInterfaceType_HostOnly)
        {
            UIDataSettingsGlobalNetworkHost data;
            generateDataNetworkHost(iface, data);
            oldData.m_networksHost << data;
        }

    /* Cache old data: */
    m_pCache->cacheInitialData(oldData);

    /* Upload properties & settings to data: */
    UISettingsPageGlobal::uploadData(data);
}

void UIGlobalSettingsNetwork::getFromCache()
{
    /* Get old data from cache: */
    const UIDataSettingsGlobalNetwork &oldData = m_pCache->base();

    /* Load old data from cache: */
    foreach (const UIDataSettingsGlobalNetworkNAT &network, oldData.m_networksNAT)
        createTreeItemNetworkNAT(network);
    m_pTreeNetworkNAT->sortByColumn(1, Qt::AscendingOrder);
    m_pTreeNetworkNAT->setCurrentItem(m_pTreeNetworkNAT->topLevelItem(0));
    sltHandleCurrentItemChangeNetworkNAT();
    foreach (const UIDataSettingsGlobalNetworkHost &network, oldData.m_networksHost)
        createTreeItemNetworkHost(network);
    m_pTreeNetworkHost->sortByColumn(0, Qt::AscendingOrder);
    m_pTreeNetworkHost->setCurrentItem(m_pTreeNetworkHost->topLevelItem(0));
    sltHandleCurrentItemChangeNetworkHost();

    /* Revalidate: */
    revalidate();
}

void UIGlobalSettingsNetwork::putToCache()
{
    /* Prepare new data: */
    UIDataSettingsGlobalNetwork newData = m_pCache->base();

    /* Gather new data: */
    newData.m_networksNAT.clear();
    for (int iNetworkIndex = 0; iNetworkIndex < m_pTreeNetworkNAT->topLevelItemCount(); ++iNetworkIndex)
    {
        UIDataSettingsGlobalNetworkNAT data;
        UIItemNetworkNAT *pItem = static_cast<UIItemNetworkNAT*>(m_pTreeNetworkNAT->topLevelItem(iNetworkIndex));
        pItem->uploadNetworkData(data);
        newData.m_networksNAT << data;
    }
    newData.m_networksHost.clear();
    for (int iNetworkIndex = 0; iNetworkIndex < m_pTreeNetworkHost->topLevelItemCount(); ++iNetworkIndex)
    {
        UIDataSettingsGlobalNetworkHost data;
        UIItemNetworkHost *pItem = static_cast<UIItemNetworkHost*>(m_pTreeNetworkHost->topLevelItem(iNetworkIndex));
        pItem->uploadNetworkData(data);
        newData.m_networksHost << data;
    }

    /* Cache new data: */
    m_pCache->cacheCurrentData(newData);
}

void UIGlobalSettingsNetwork::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to properties & settings: */
    UISettingsPageGlobal::fetchData(data);

    /* Save new data from cache: */
    if (m_pCache->wasChanged())
    {
        if (m_pCache->data().m_networksNAT != m_pCache->base().m_networksNAT)
            foreach (const UIDataSettingsGlobalNetworkNAT &data, m_pCache->data().m_networksNAT)
                saveCacheItemNetworkNAT(data);
        if (m_pCache->data().m_networksHost != m_pCache->base().m_networksHost)
            foreach (const UIDataSettingsGlobalNetworkHost &data, m_pCache->data().m_networksHost)
                saveCacheItemNetworkHost(data);
    }

    /* Upload properties & settings to data: */
    UISettingsPageGlobal::uploadData(data);
}

bool UIGlobalSettingsNetwork::validate(QList<UIValidationMessage> &messages)
{
    /* Pass by default: */
    bool fPass = true;

    /* Validate NAT network items: */
    {
        /* Prepare message: */
        UIValidationMessage message;
        message.first = VBoxGlobal::removeAccelMark(m_pTabWidget->tabText(0));

        /* Validate items first: */
        for (int iNetworkIndex = 0; iNetworkIndex < m_pTreeNetworkNAT->topLevelItemCount(); ++iNetworkIndex)
        {
            UIItemNetworkNAT *pItem = static_cast<UIItemNetworkNAT*>(m_pTreeNetworkNAT->topLevelItem(iNetworkIndex));
            if (!pItem->validate(message))
                fPass = false;
        }

        /* And make sure item names are unique: */
        QList<QString> names;
        for (int iItemIndex = 0; iItemIndex < m_pTreeNetworkNAT->topLevelItemCount(); ++iItemIndex)
        {
            UIItemNetworkNAT *pItem = static_cast<UIItemNetworkNAT*>(m_pTreeNetworkNAT->topLevelItem(iItemIndex));
            const QString strItemName(pItem->newName());
            if (strItemName.isEmpty())
                continue;
            if (!names.contains(strItemName))
                names << strItemName;
            else
            {
                message.second << UIGlobalSettingsNetwork::tr("The name <b>%1</b> is being used for several NAT networks.")
                                                              .arg(strItemName);
                fPass = false;
            }
        }

        /* Serialize message: */
        if (!message.second.isEmpty())
            messages << message;
    }

    /* Validate Host network items: */
    {
        /* Prepare message: */
        UIValidationMessage message;
        message.first = VBoxGlobal::removeAccelMark(m_pTabWidget->tabText(1));

        /* Validate items only: */
        for (int iNetworkIndex = 0; iNetworkIndex < m_pTreeNetworkHost->topLevelItemCount(); ++iNetworkIndex)
        {
            UIItemNetworkHost *pItem = static_cast<UIItemNetworkHost*>(m_pTreeNetworkHost->topLevelItem(iNetworkIndex));
            if (!pItem->validate(message))
                fPass = false;
        }

        /* Serialize message: */
        if (!message.second.isEmpty())
            messages << message;
    }

    /* Return result: */
    return fPass;
}

void UIGlobalSettingsNetwork::setOrderAfter(QWidget *pWidget)
{
    setTabOrder(pWidget, m_pTabWidget->focusProxy());
    setTabOrder(m_pTabWidget->focusProxy(), m_pTreeNetworkNAT);
    setTabOrder(m_pTreeNetworkNAT, m_pTreeNetworkHost);
}

void UIGlobalSettingsNetwork::retranslateUi()
{
    /* Translate uic generated strings: */
    Ui::UIGlobalSettingsNetwork::retranslateUi(this);

    /* NAT networks: */
    {
        /* Translate tree-widget columns: */
        m_pTreeNetworkNAT->setHeaderLabels(QStringList()
                                           << tr("Active", "NAT network")
                                           << tr("Name"));

        /* Translate action text: */
        m_pActionAddNetworkNAT->setText(tr("Add NAT Network"));
        m_pActionDelNetworkNAT->setText(tr("Remove NAT Network"));
        m_pActionEditNetworkNAT->setText(tr("Edit NAT Network"));

        m_pActionAddNetworkNAT->setWhatsThis(tr("Adds new NAT network."));
        m_pActionDelNetworkNAT->setWhatsThis(tr("Removes selected NAT network."));
        m_pActionEditNetworkNAT->setWhatsThis(tr("Edits selected NAT network."));

        m_pActionAddNetworkNAT->setToolTip(m_pActionAddNetworkNAT->whatsThis());
        m_pActionDelNetworkNAT->setToolTip(m_pActionDelNetworkNAT->whatsThis());
        m_pActionEditNetworkNAT->setToolTip(m_pActionEditNetworkNAT->whatsThis());
    }

    /* Host networks: */
    {
        /* Translate action text: */
        m_pActionAddNetworkHost->setText(tr("Add Host-only Network"));
        m_pActionDelNetworkHost->setText(tr("Remove Host-only Network"));
        m_pActionEditNetworkHost->setText(tr("Edit Host-only Network"));

        m_pActionAddNetworkHost->setWhatsThis(tr("Adds new host-only network."));
        m_pActionDelNetworkHost->setWhatsThis(tr("Removes selected host-only network."));
        m_pActionEditNetworkHost->setWhatsThis(tr("Edits selected host-only network."));

        m_pActionAddNetworkHost->setToolTip(m_pActionAddNetworkHost->whatsThis());
        m_pActionDelNetworkHost->setToolTip(m_pActionDelNetworkHost->whatsThis());
        m_pActionEditNetworkHost->setToolTip(m_pActionEditNetworkHost->whatsThis());
    }
}

void UIGlobalSettingsNetwork::sltAddNetworkNAT()
{
    /* Prepare useful variables: */
    CVirtualBox vbox = vboxGlobal().virtualBox();

    /* Compose a pool of busy names: */
    QList<QString> names;
    for (int iItemIndex = 0; iItemIndex < m_pTreeNetworkNAT->topLevelItemCount(); ++iItemIndex)
    {
        UIItemNetworkNAT *pItem = static_cast<UIItemNetworkNAT*>(m_pTreeNetworkNAT->topLevelItem(iItemIndex));
        if (!names.contains(pItem->name()))
            names << pItem->name();
    }
    /* Search for the name with maximum index: */
    int iMaximumIndex = -1;
    const QString strNameTemplate("NatNetwork%1");
    foreach (const QString &strName, names)
    {
        QRegExp regExp(strNameTemplate.arg("([\\d]*)"));
        if (regExp.indexIn(strName) != -1)
            iMaximumIndex = qMax(iMaximumIndex, regExp.cap(1).toInt());
    }

    /* Create NAT network: */
    QString strIndex(iMaximumIndex == -1 ? QString() : QString::number(iMaximumIndex + 1));
    CNATNetwork network = vbox.CreateNATNetwork(strNameTemplate.arg(strIndex));
    if (!vbox.isOk())
        return msgCenter().cannotCreateNATNetwork(vbox, this);
    AssertReturnVoid(!network.isNull());

    /* Update tree: */
    UIDataSettingsGlobalNetworkNAT data;
    generateDataNetworkNAT(network, data);
    createTreeItemNetworkNAT(data, true);
    m_pTreeNetworkNAT->sortByColumn(1, Qt::AscendingOrder);
}

void UIGlobalSettingsNetwork::sltDelNetworkNAT()
{
    /* Get network item: */
    UIItemNetworkNAT *pItem = static_cast<UIItemNetworkNAT*>(m_pTreeNetworkNAT->currentItem());
    AssertMsg(pItem, ("Current item should present!\n"));
    /* Get network name: */
    const QString strNetworkName(pItem->name());

    /* Confirm NAT network removal: */
    if (!msgCenter().confirmNATNetworkRemoval(strNetworkName, this))
        return;

    /* Prepare useful variables: */
    CVirtualBox vbox = vboxGlobal().virtualBox();

    /* Find corresponding interface: */
    const CNATNetwork &network = vbox.FindNATNetworkByName(strNetworkName);
    AssertReturnVoid(vbox.isOk() && !network.isNull());

    /* Remove NAT network: */
    vbox.RemoveNATNetwork(network);
    if (!vbox.isOk())
        return msgCenter().cannotRemoveNATNetwork(vbox, strNetworkName, this);

    /* Update tree: */
    removeTreeItemNetworkNAT(pItem);
}

void UIGlobalSettingsNetwork::sltEditNetworkNAT()
{
    /* Get network item: */
    UIItemNetworkNAT *pItem = static_cast<UIItemNetworkNAT*>(m_pTreeNetworkNAT->currentItem());
    AssertMsg(pItem, ("Current item should present!\n"));

    /* Edit current item data: */
    UIDataSettingsGlobalNetworkNAT data;
    pItem->uploadNetworkData(data);
    UIGlobalSettingsNetworkDetailsNAT details(this, data);
    if (details.exec() == QDialog::Accepted)
    {
        /* Put data back: */
        pItem->fetchNetworkData(data);
        sltHandleCurrentItemChangeNetworkNAT();
        /* Revalidate: */
        revalidate();
    }
}

void UIGlobalSettingsNetwork::sltHandleItemChangeNetworkNAT(QTreeWidgetItem *pChangedItem)
{
    /* Get network item: */
    UIItemNetworkNAT *pItem = static_cast<UIItemNetworkNAT*>(pChangedItem);
    AssertMsg(pItem, ("Changed item should present!\n"));

    /* Update item data: */
    pItem->updateData();
}

void UIGlobalSettingsNetwork::sltAddNetworkHost()
{
    /* Prepare useful variables: */
    CVirtualBox vbox = vboxGlobal().virtualBox();
    CHost host = vboxGlobal().host();

    /* Create Host interface: */
    CHostNetworkInterface iface;
    CProgress progress = host.CreateHostOnlyNetworkInterface(iface);
    if (!host.isOk())
        return msgCenter().cannotCreateHostInterface(host, this);
    msgCenter().showModalProgressDialog(progress, tr("Networking"), ":/progress_network_interface_90px.png", this, 0);
    if (!progress.isOk() || progress.GetResultCode() != 0)
        return msgCenter().cannotCreateHostInterface(progress, this);

    /* Make sure DHCP server is created too: */
    CDHCPServer dhcp = vbox.FindDHCPServerByNetworkName(iface.GetNetworkName());
    if (dhcp.isNull())
    {
        vbox.CreateDHCPServer(iface.GetNetworkName());
        if (!vbox.isOk())
            return msgCenter().cannotCreateDHCPServer(vbox, this);
        dhcp = vbox.FindDHCPServerByNetworkName(iface.GetNetworkName());
    }
    AssertReturnVoid(!dhcp.isNull());

    /* Update tree: */
    UIDataSettingsGlobalNetworkHost data;
    generateDataNetworkHost(iface, data);
    createTreeItemNetworkHost(data, true);
    m_pTreeNetworkHost->sortByColumn(0, Qt::AscendingOrder);
}

void UIGlobalSettingsNetwork::sltDelNetworkHost()
{
    /* Get network item: */
    UIItemNetworkHost *pItem = static_cast<UIItemNetworkHost*>(m_pTreeNetworkHost->currentItem());
    AssertMsg(pItem, ("Current item should present!\n"));
    /* Get interface name: */
    const QString strInterfaceName(pItem->name());

    /* Confirm Host network removal: */
    if (!msgCenter().confirmHostOnlyInterfaceRemoval(strInterfaceName, this))
        return;

    /* Prepare useful variables: */
    CVirtualBox vbox = vboxGlobal().virtualBox();
    CHost host = vboxGlobal().host();

    /* Find corresponding interface: */
    const CHostNetworkInterface &iface = host.FindHostNetworkInterfaceByName(strInterfaceName);
    AssertReturnVoid(host.isOk() && !iface.isNull());

    /* Remove DHCP server if any: */
    CDHCPServer dhcp = vbox.FindDHCPServerByNetworkName(iface.GetNetworkName());
    if (vbox.isOk() && !dhcp.isNull())
    {
        vbox.RemoveDHCPServer(dhcp);
        if (!vbox.isOk())
            return msgCenter().cannotRemoveDHCPServer(vbox, strInterfaceName, this);
    }

    /* Remove interface finally: */
    CProgress progress = host.RemoveHostOnlyNetworkInterface(iface.GetId());
    if (!host.isOk())
        return msgCenter().cannotRemoveHostInterface(host, strInterfaceName, this);
    msgCenter().showModalProgressDialog(progress, tr("Networking"), ":/progress_network_interface_90px.png", this, 0);
    if (!progress.isOk() || progress.GetResultCode() != 0)
        return msgCenter().cannotRemoveHostInterface(progress, strInterfaceName, this);

    /* Update tree: */
    removeTreeItemNetworkHost(pItem);
}

void UIGlobalSettingsNetwork::sltEditNetworkHost()
{
    /* Get network item: */
    UIItemNetworkHost *pItem = static_cast<UIItemNetworkHost*>(m_pTreeNetworkHost->currentItem());
    AssertMsg(pItem, ("Current item should present!\n"));

    /* Edit current item data: */
    UIDataSettingsGlobalNetworkHost data;
    pItem->uploadNetworkData(data);
    UIGlobalSettingsNetworkDetailsHost details(this, data);
    if (details.exec() == QDialog::Accepted)
    {
        /* Put data back: */
        pItem->fetchNetworkData(data);
        sltHandleCurrentItemChangeNetworkHost();
        /* Revalidate: */
        revalidate();
    }
}

void UIGlobalSettingsNetwork::sltHandleCurrentItemChangeNetworkNAT()
{
    /* Get current item: */
    UIItemNetworkNAT *pItem = static_cast<UIItemNetworkNAT*>(m_pTreeNetworkNAT->currentItem());
    /* Update availability: */
    m_pActionDelNetworkNAT->setEnabled(pItem);
    m_pActionEditNetworkNAT->setEnabled(pItem);
}

void UIGlobalSettingsNetwork::sltHandleCurrentItemChangeNetworkHost()
{
    /* Get current item: */
    UIItemNetworkHost *pItem = static_cast<UIItemNetworkHost*>(m_pTreeNetworkHost->currentItem());
    /* Update availability: */
    m_pActionDelNetworkHost->setEnabled(pItem);
    m_pActionEditNetworkHost->setEnabled(pItem);
}

void UIGlobalSettingsNetwork::sltShowContextMenuNetworkNAT(const QPoint &pos)
{
    /* Compose temporary context-menu: */
    QMenu menu;
    if (m_pTreeNetworkNAT->itemAt(pos))
    {
        menu.addAction(m_pActionEditNetworkNAT);
        menu.addAction(m_pActionDelNetworkNAT);
    }
    else
    {
        menu.addAction(m_pActionAddNetworkNAT);
    }
    /* And show it: */
    menu.exec(m_pTreeNetworkNAT->mapToGlobal(pos));
}

void UIGlobalSettingsNetwork::sltShowContextMenuNetworkHost(const QPoint &pos)
{
    /* Compose temporary context-menu: */
    QMenu menu;
    if (m_pTreeNetworkHost->itemAt(pos))
    {
        menu.addAction(m_pActionEditNetworkHost);
        menu.addAction(m_pActionDelNetworkHost);
    }
    else
    {
        menu.addAction(m_pActionAddNetworkHost);
    }
    /* And show it: */
    menu.exec(m_pTreeNetworkHost->mapToGlobal(pos));
}

void UIGlobalSettingsNetwork::generateDataNetworkNAT(const CNATNetwork &network, UIDataSettingsGlobalNetworkNAT &data)
{
    /* Load NAT network settings: */
    data.m_fEnabled = network.GetEnabled();
    data.m_strName = network.GetNetworkName();
    data.m_strNewName = data.m_strName;
    data.m_strCIDR = network.GetNetwork();
    data.m_fSupportsDHCP = network.GetNeedDhcpServer();
    data.m_fSupportsIPv6 = network.GetIPv6Enabled();
    data.m_fAdvertiseDefaultIPv6Route = network.GetAdvertiseDefaultIPv6RouteEnabled();

    /* Load IPv4 rules: */
    QVector<QString> ipv4rules = network.GetPortForwardRules4();
    foreach (QString strIPv4Rule, ipv4rules)
    {
        /* Replace all ':' with ',' first: */
        strIPv4Rule.replace(':', ',');
        /* Parse rules: */
        QStringList rules = strIPv4Rule.split(',');
        Assert(rules.size() == 6);
        if (rules.size() != 6)
            continue;
        data.m_ipv4rules << UIPortForwardingData(rules[0],
                                                 gpConverter->fromInternalString<KNATProtocol>(rules[1]),
                                                 QString(rules[2]).remove('[').remove(']'),
                                                 rules[3].toUInt(),
                                                 QString(rules[4]).remove('[').remove(']'),
                                                 rules[5].toUInt());
    }

    /* Load IPv6 rules: */
    QVector<QString> ipv6rules = network.GetPortForwardRules6();
    foreach (QString strIPv6Rule, ipv6rules)
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
        data.m_ipv6rules << UIPortForwardingData(rules[0],
                                                 gpConverter->fromInternalString<KNATProtocol>(rules[1]),
                                                 QString(rules[2]).remove('[').remove(']'),
                                                 rules[3].toUInt(),
                                                 QString(rules[4]).remove('[').remove(']'),
                                                 rules[5].toUInt());
    }
}

void UIGlobalSettingsNetwork::saveCacheItemNetworkNAT(const UIDataSettingsGlobalNetworkNAT &data)
{
    /* Make sure corresponding NAT network exists: */
    CVirtualBox vbox = vboxGlobal().virtualBox();
    CNATNetwork network = vbox.FindNATNetworkByName(data.m_strName);
    AssertReturnVoid(vbox.isOk() && !network.isNull());

    /* Save NAT network options: */
    network.SetEnabled(data.m_fEnabled);
    network.SetNetworkName(data.m_strNewName);
    network.SetNetwork(data.m_strCIDR);
    network.SetNeedDhcpServer(data.m_fSupportsDHCP);
    network.SetIPv6Enabled(data.m_fSupportsIPv6);
    network.SetAdvertiseDefaultIPv6RouteEnabled(data.m_fAdvertiseDefaultIPv6Route);

    /* Rewrite IPv4 rules: */
    QVector<QString> oldIPv4Rules = network.GetPortForwardRules4();
    foreach (const QString &strRule, oldIPv4Rules)
        network.RemovePortForwardRule(false, strRule.section(':', 0, 0));
    foreach (const UIPortForwardingData &newRule, data.m_ipv4rules)
        network.AddPortForwardRule(false,
                                   newRule.name, newRule.protocol,
                                   newRule.hostIp, newRule.hostPort.value(),
                                   newRule.guestIp, newRule.guestPort.value());

    /* Rewrite IPv6 rules: */
    QVector<QString> oldIPv6Rules = network.GetPortForwardRules6();
    foreach (const QString &strRule, oldIPv6Rules)
        network.RemovePortForwardRule(true, strRule.section(':', 0, 0));
    foreach (const UIPortForwardingData &newRule, data.m_ipv6rules)
        network.AddPortForwardRule(true,
                                   newRule.name, newRule.protocol,
                                   newRule.hostIp, newRule.hostPort.value(),
                                   newRule.guestIp, newRule.guestPort.value());
}

void UIGlobalSettingsNetwork::createTreeItemNetworkNAT(const UIDataSettingsGlobalNetworkNAT &data, bool fChooseItem)
{
    /* Add new item to the tree: */
    UIItemNetworkNAT *pItem = new UIItemNetworkNAT;
    pItem->fetchNetworkData(data);
    m_pTreeNetworkNAT->addTopLevelItem(pItem);
    /* And choose it as current if necessary: */
    if (fChooseItem)
        m_pTreeNetworkNAT->setCurrentItem(pItem);
}

void UIGlobalSettingsNetwork::removeTreeItemNetworkNAT(UIItemNetworkNAT *pItem)
{
    /* Delete passed item: */
    delete pItem;
}

void UIGlobalSettingsNetwork::generateDataNetworkHost(const CHostNetworkInterface &iface, UIDataSettingsGlobalNetworkHost &data)
{
    /* Get DHCP server (create if necessary): */
    CDHCPServer dhcp = vboxGlobal().virtualBox().FindDHCPServerByNetworkName(iface.GetNetworkName());
    if (dhcp.isNull())
    {
        CVirtualBox vbox = vboxGlobal().virtualBox();
        vbox.CreateDHCPServer(iface.GetNetworkName());
        if (!vbox.isOk())
        {
            msgCenter().cannotCreateDHCPServer(vbox, this);
            return;
        }
        dhcp = vboxGlobal().virtualBox().FindDHCPServerByNetworkName(iface.GetNetworkName());
    }
    Assert(!dhcp.isNull());
    if (dhcp.isNull())
        return;

    /* Host interface settings: */
    data.m_interface.m_strName = iface.GetName();
    data.m_interface.m_fDhcpClientEnabled = iface.GetDHCPEnabled();
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
}

void UIGlobalSettingsNetwork::saveCacheItemNetworkHost(const UIDataSettingsGlobalNetworkHost &data)
{
    /* Make sure corresponding Host interface exists: */
    CHost host = vboxGlobal().host();
    CHostNetworkInterface iface = host.FindHostNetworkInterfaceByName(data.m_interface.m_strName);
    AssertReturnVoid(host.isOk() && !iface.isNull());

    /* Automatic host interface configuration: */
    if (data.m_interface.m_fDhcpClientEnabled)
    {
        iface.EnableDynamicIPConfig();
    }
    /* Manual host interface configuration: */
    else
    {
        AssertMsg(data.m_interface.m_strInterfaceAddress.trimmed().isEmpty() ||
                  RTNetIsIPv4AddrStr(data.m_interface.m_strInterfaceAddress.toUtf8().constData()),
                  ("Interface IPv4 address must be empty or IPv4-valid!\n"));
        AssertMsg(data.m_interface.m_strInterfaceMask.trimmed().isEmpty() ||
                  RTNetIsIPv4AddrStr(data.m_interface.m_strInterfaceMask.toUtf8().constData()),
                  ("Interface IPv4 network mask must be empty or IPv4-valid!\n"));
        if (   (   data.m_interface.m_strInterfaceAddress.trimmed().isEmpty()
                || RTNetIsIPv4AddrStr(data.m_interface.m_strInterfaceAddress.toUtf8().constData()))
            && (   data.m_interface.m_strInterfaceMask.trimmed().isEmpty()
                || RTNetIsIPv4AddrStr(data.m_interface.m_strInterfaceMask.toUtf8().constData())))
            iface.EnableStaticIPConfig(data.m_interface.m_strInterfaceAddress, data.m_interface.m_strInterfaceMask);
        if (iface.GetIPV6Supported())
        {
            AssertMsg(data.m_interface.m_strInterfaceAddress6.trimmed().isEmpty() ||
                      RTNetIsIPv6AddrStr(data.m_interface.m_strInterfaceAddress6.toUtf8().constData()),
                      ("Interface IPv6 address must be empty or IPv6-valid!\n"));
            bool fIsMaskPrefixLengthNumber = false;
            const int iMaskPrefixLength = data.m_interface.m_strInterfaceMaskLength6.trimmed().toInt(&fIsMaskPrefixLengthNumber);
            AssertMsg(fIsMaskPrefixLengthNumber && iMaskPrefixLength >= 0 && iMaskPrefixLength <= 128,
                      ("Interface IPv6 network mask prefix length must be empty or IPv6-valid!\n"));
            if (   (   data.m_interface.m_strInterfaceAddress6.trimmed().isEmpty()
                    || RTNetIsIPv6AddrStr(data.m_interface.m_strInterfaceAddress6.toUtf8().constData()))
                && (   fIsMaskPrefixLengthNumber
                    && iMaskPrefixLength >= 0
                    && iMaskPrefixLength <= 128))
                iface.EnableStaticIPConfigV6(data.m_interface.m_strInterfaceAddress6, data.m_interface.m_strInterfaceMaskLength6.toULong());
        }
    }

    /* Make sure corresponding DHCP server exists: */
    CVirtualBox vbox = vboxGlobal().virtualBox();
    CDHCPServer dhcp = vbox.FindDHCPServerByNetworkName(iface.GetNetworkName());
    AssertReturnVoid(vbox.isOk() && !dhcp.isNull());

    /* Save DHCP server configuration: */
    dhcp.SetEnabled(data.m_dhcpserver.m_fDhcpServerEnabled);
    if (data.m_dhcpserver.m_fDhcpServerEnabled)
    {
        AssertMsg(RTNetIsIPv4AddrStr(data.m_dhcpserver.m_strDhcpServerAddress.toUtf8().constData()),
                  ("DHCP server IPv4 address must be IPv4-valid!\n"));
        AssertMsg(RTNetIsIPv4AddrStr(data.m_dhcpserver.m_strDhcpServerMask.toUtf8().constData()),
                  ("DHCP server IPv4 network mask must be IPv4-valid!\n"));
        AssertMsg(RTNetIsIPv4AddrStr(data.m_dhcpserver.m_strDhcpLowerAddress.toUtf8().constData()),
                  ("DHCP server IPv4 lower bound must be IPv4-valid!\n"));
        AssertMsg(RTNetIsIPv4AddrStr(data.m_dhcpserver.m_strDhcpUpperAddress.toUtf8().constData()),
                  ("DHCP server IPv4 upper bound must be IPv4-valid!\n"));
        if (RTNetIsIPv4AddrStr(data.m_dhcpserver.m_strDhcpServerAddress.toUtf8().constData()) &&
            RTNetIsIPv4AddrStr(data.m_dhcpserver.m_strDhcpServerMask.toUtf8().constData()) &&
            RTNetIsIPv4AddrStr(data.m_dhcpserver.m_strDhcpLowerAddress.toUtf8().constData()) &&
            RTNetIsIPv4AddrStr(data.m_dhcpserver.m_strDhcpUpperAddress.toUtf8().constData()))
            dhcp.SetConfiguration(data.m_dhcpserver.m_strDhcpServerAddress, data.m_dhcpserver.m_strDhcpServerMask,
                                  data.m_dhcpserver.m_strDhcpLowerAddress, data.m_dhcpserver.m_strDhcpUpperAddress);
    }
    if (!dhcp.isOk())
        emit sigOperationProgressError(UIMessageCenter::formatErrorInfo(dhcp));
}

void UIGlobalSettingsNetwork::createTreeItemNetworkHost(const UIDataSettingsGlobalNetworkHost &data, bool fChooseItem)
{
    /* Add new item to the tree: */
    UIItemNetworkHost *pItem = new UIItemNetworkHost;
    pItem->fetchNetworkData(data);
    m_pTreeNetworkHost->addTopLevelItem(pItem);
    /* And choose it as current if necessary: */
    if (fChooseItem)
        m_pTreeNetworkHost->setCurrentItem(pItem);
}

void UIGlobalSettingsNetwork::removeTreeItemNetworkHost(UIItemNetworkHost *pItem)
{
    /* Delete passed item: */
    delete pItem;
}

