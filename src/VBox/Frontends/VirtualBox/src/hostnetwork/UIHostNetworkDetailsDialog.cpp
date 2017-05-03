/* $Id$ */
/** @file
 * VBox Qt GUI - UIHostNetworkDetailsDialog class implementation.
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
# include <QCheckBox>
# include <QLabel>
# include <QStyleOption>
# include <QRegExpValidator>
# include <QVBoxLayout>

/* GUI includes: */
# include "QILineEdit.h"
# include "QITabWidget.h"
# include "UIHostNetworkDetailsDialog.h"
# include "UIIconPool.h"

/* Other VBox includes: */
# include "iprt/assert.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIHostNetworkDetailsDialog::UIHostNetworkDetailsDialog(QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI2<QWidget>(pParent)
    , m_pTabWidget(0)
    , m_pLabelIPv4(0), m_pEditorIPv4(0), m_pLabelNMv4(0), m_pEditorNMv4(0)
    , m_pLabelIPv6(0), m_pEditorIPv6(0), m_pLabelNMv6(0), m_pEditorNMv6(0)
    , m_pCheckBoxDHCP(0) , m_pLabelDHCPAddress(0), m_pEditorDHCPAddress(0), m_pLabelDHCPMask(0), m_pEditorDHCPMask(0)
    , m_pLabelDHCPLowerAddress(0), m_pEditorDHCPLowerAddress(0), m_pLabelDHCPUpperAddress(0), m_pEditorDHCPUpperAddress(0)
{
    /* Prepare: */
    prepare();
}

void UIHostNetworkDetailsDialog::setData(const UIDataHostNetwork &data)
{
    /* Save old data: */
    m_oldData = data;

    /* Invent default old values if current old values are invalid: */
    const quint32 uAddr = ipv4FromQStringToQuint32(m_oldData.m_interface.m_strAddress);
    const quint32 uMask = ipv4FromQStringToQuint32(m_oldData.m_interface.m_strMask);
    const quint32 uProp = uAddr & uMask;
    const QString strMask = ipv4FromQuint32ToQString(uMask);
    const QString strProp = ipv4FromQuint32ToQString(uProp);
//    printf("Proposal is = %s x %s\n",
//           strProp.toUtf8().constData(),
//           strMask.toUtf8().constData());
    if (   m_oldData.m_dhcpserver.m_strAddress.isEmpty()
        || m_oldData.m_dhcpserver.m_strAddress == "0.0.0.0")
        m_oldData.m_dhcpserver.m_strAddress = strProp;
    if (   m_oldData.m_dhcpserver.m_strMask.isEmpty()
        || m_oldData.m_dhcpserver.m_strMask == "0.0.0.0")
        m_oldData.m_dhcpserver.m_strMask = strMask;
    if (   m_oldData.m_dhcpserver.m_strLowerAddress.isEmpty()
        || m_oldData.m_dhcpserver.m_strLowerAddress == "0.0.0.0")
        m_oldData.m_dhcpserver.m_strLowerAddress = strProp;
    if (   m_oldData.m_dhcpserver.m_strUpperAddress.isEmpty()
        || m_oldData.m_dhcpserver.m_strUpperAddress == "0.0.0.0")
        m_oldData.m_dhcpserver.m_strUpperAddress = strProp;

    /* Copy old data to new one: */
    m_newData = m_oldData;

    /* Load 'Interface' data: */
    loadDataForInterface();
    /* Load 'DHCP server' data: */
    loadDataForDHCPServer();
}

void UIHostNetworkDetailsDialog::clearData()
{
    /* Reset old/new data: */
    m_oldData = UIDataHostNetwork();
    m_newData = m_oldData;

    /* Load 'Interface' data: */
    loadDataForInterface();
    /* Load 'DHCP server' data: */
    loadDataForDHCPServer();
}

void UIHostNetworkDetailsDialog::retranslateUi()
{
    /* Translate tab-widget: */
    m_pTabWidget->setTabText(0, tr("&Adapter"));
    m_pTabWidget->setTabText(1, tr("&DHCP Server"));

    /* Translate 'Interface' tab content: */
    m_pLabelIPv4->setText(tr("&IPv4 Address:"));
    m_pEditorIPv4->setToolTip(tr("Holds the host IPv4 address for this adapter."));
    m_pLabelNMv4->setText(tr("IPv4 Network &Mask:"));
    m_pEditorNMv4->setToolTip(tr("Holds the host IPv4 network mask for this adapter."));
    m_pLabelIPv6->setText(tr("I&Pv6 Address:"));
    m_pEditorIPv6->setToolTip(tr("Holds the host IPv6 address for this adapter if IPv6 is supported."));
    m_pLabelNMv6->setText(tr("IPv6 Network Mask &Length:"));
    m_pEditorNMv6->setToolTip(tr("Holds the host IPv6 network mask prefix length for this adapter if IPv6 is supported."));

    /* Translate 'DHCP server' tab content: */
    m_pCheckBoxDHCP->setText(tr("&Enable Server"));
    m_pCheckBoxDHCP->setToolTip(tr("When checked, the DHCP Server will be enabled for this network on machine start-up."));
    m_pLabelDHCPAddress->setText(tr("Server Add&ress:"));
    m_pEditorDHCPAddress->setToolTip(tr("Holds the address of the DHCP server servicing the network associated with this host-only adapter."));
    m_pLabelDHCPMask->setText(tr("Server &Mask:"));
    m_pEditorDHCPMask->setToolTip(tr("Holds the network mask of the DHCP server servicing the network associated with this host-only adapter."));
    m_pLabelDHCPLowerAddress->setText(tr("&Lower Address Bound:"));
    m_pEditorDHCPLowerAddress->setToolTip(tr("Holds the lower address bound offered by the DHCP server servicing the network associated with this host-only adapter."));
    m_pLabelDHCPUpperAddress->setText(tr("&Upper Address Bound:"));
    m_pEditorDHCPUpperAddress->setToolTip(tr("Holds the upper address bound offered by the DHCP server servicing the network associated with this host-only adapter."));
}

void UIHostNetworkDetailsDialog::prepare()
{
    /* Prepare this: */
    prepareThis();

    /* Apply language settings: */
    retranslateUi();
}

void UIHostNetworkDetailsDialog::prepareThis()
{
    /* Create layout: */
    QVBoxLayout *pLayout = new QVBoxLayout(this);
    AssertPtrReturnVoid(pLayout);
    {
        /* Configure layout: */
        pLayout->setContentsMargins(0, 0, 0, 0);
        /* Prepare tab-widget: */
        prepareTabWidget();
    }
}

void UIHostNetworkDetailsDialog::prepareTabWidget()
{
    /* Create tab-widget: */
    m_pTabWidget = new QITabWidget;
    AssertPtrReturnVoid(m_pTabWidget);
    {
        /* Prepare 'Interface' tab: */
        prepareTabInterface();
        /* Prepare 'DHCP server' tab: */
        prepareTabDHCPServer();
        /* Add into layout: */
        layout()->addWidget(m_pTabWidget);
    }
}

void UIHostNetworkDetailsDialog::prepareTabInterface()
{
    /* Create 'Interface' tab: */
    QWidget *pTabInterface = new QWidget;
    AssertPtrReturnVoid(pTabInterface);
    {
        /* Create 'Interface' layout: */
        QGridLayout *pLayoutInterface = new QGridLayout(pTabInterface);
        AssertPtrReturnVoid(pLayoutInterface);
        {
#ifdef VBOX_WS_MAC
            /* Configure layout: */
            pLayoutInterface->setContentsMargins(10, 10, 10, 10);
#endif
            /* Create IPv4 address label: */
            m_pLabelIPv4 = new QLabel;
            AssertPtrReturnVoid(m_pLabelIPv4);
            {
                /* Configure label: */
                m_pLabelIPv4->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
                /* Add into layout: */
                pLayoutInterface->addWidget(m_pLabelIPv4, 0, 0);
            }
            /* Create IPv4 address editor: */
            m_pEditorIPv4 = new QILineEdit;
            AssertPtrReturnVoid(m_pEditorIPv4);
            {
                /* Configure editor: */
                m_pLabelIPv4->setBuddy(m_pEditorIPv4);
                connect(m_pEditorIPv4, &QLineEdit::textChanged,
                        this, &UIHostNetworkDetailsDialog::sltTextChangedIPv4);
                /* Add into layout: */
                pLayoutInterface->addWidget(m_pEditorIPv4, 0, 1);
            }
            /* Create IPv4 network mask label: */
            m_pLabelNMv4 = new QLabel;
            AssertPtrReturnVoid(m_pLabelNMv4);
            {
                /* Configure label: */
                m_pLabelNMv4->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
                /* Add into layout: */
                pLayoutInterface->addWidget(m_pLabelNMv4, 1, 0);
            }
            /* Create IPv4 network mask editor: */
            m_pEditorNMv4 = new QILineEdit;
            AssertPtrReturnVoid(m_pEditorNMv4);
            {
                /* Configure editor: */
                m_pLabelNMv4->setBuddy(m_pEditorNMv4);
                connect(m_pEditorNMv4, &QLineEdit::textChanged,
                        this, &UIHostNetworkDetailsDialog::sltTextChangedNMv4);
                /* Add into layout: */
                pLayoutInterface->addWidget(m_pEditorNMv4, 1, 1);
            }
            /* Create IPv6 address label: */
            m_pLabelIPv6 = new QLabel;
            AssertPtrReturnVoid(m_pLabelIPv6);
            {
                /* Configure label: */
                m_pLabelIPv6->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
                /* Add into layout: */
                pLayoutInterface->addWidget(m_pLabelIPv6, 2, 0);
            }
            /* Create IPv6 address editor: */
            m_pEditorIPv6 = new QILineEdit;
            AssertPtrReturnVoid(m_pEditorIPv6);
            {
                /* Configure editor: */
                m_pLabelIPv6->setBuddy(m_pEditorIPv6);
                connect(m_pEditorIPv6, &QLineEdit::textChanged,
                        this, &UIHostNetworkDetailsDialog::sltTextChangedIPv6);
                /* Add into layout: */
                pLayoutInterface->addWidget(m_pEditorIPv6, 2, 1);
            }
            /* Create IPv6 network mask label: */
            m_pLabelNMv6 = new QLabel;
            AssertPtrReturnVoid(m_pLabelNMv6);
            {
                /* Configure label: */
                m_pLabelNMv6->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
                /* Add into layout: */
                pLayoutInterface->addWidget(m_pLabelNMv6, 3, 0);
            }
            /* Create IPv6 network mask editor: */
            m_pEditorNMv6 = new QILineEdit;
            AssertPtrReturnVoid(m_pEditorNMv6);
            {
                /* Configure editor: */
                m_pLabelNMv6->setBuddy(m_pEditorNMv6);
                connect(m_pEditorNMv6, &QLineEdit::textChanged,
                        this, &UIHostNetworkDetailsDialog::sltTextChangedNMv6);
                /* Add into layout: */
                pLayoutInterface->addWidget(m_pEditorNMv6, 3, 1);
            }
            /* Create stretch: */
            QSpacerItem *pSpacer1 = new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
            AssertPtrReturnVoid(pSpacer1);
            {
                /* Add into layout: */
                pLayoutInterface->addItem(pSpacer1, 4, 0, 1, 2);
            }
        }
        /* Add to tab-widget: */
        m_pTabWidget->addTab(pTabInterface, QString());
    }
}

void UIHostNetworkDetailsDialog::prepareTabDHCPServer()
{
    /* Create 'DHCP server' tab: */
    QWidget *pTabDHCPServer = new QWidget;
    AssertPtrReturnVoid(pTabDHCPServer);
    {
        /* Create 'DHCP server' layout: */
        QGridLayout *pLayoutDHCPServer = new QGridLayout(pTabDHCPServer);
        AssertPtrReturnVoid(pLayoutDHCPServer);
        {
#ifdef VBOX_WS_MAC
            /* Configure layout: */
            pLayoutDHCPServer->setContentsMargins(10, 10, 10, 10);
#endif
            /* Create DHCP server status check-box: */
            m_pCheckBoxDHCP = new QCheckBox;
            AssertPtrReturnVoid(m_pCheckBoxDHCP);
            {
                /* Configure check-box: */
                connect(m_pCheckBoxDHCP, &QCheckBox::stateChanged,
                        this, &UIHostNetworkDetailsDialog::sltStatusChangedServer);
                /* Add into layout: */
                pLayoutDHCPServer->addWidget(m_pCheckBoxDHCP, 0, 0, 1, 2);
            }
            /* Create DHCP address label: */
            m_pLabelDHCPAddress = new QLabel;
            AssertPtrReturnVoid(m_pLabelDHCPAddress);
            {
                /* Configure label: */
                m_pLabelDHCPAddress->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
                /* Add into layout: */
                pLayoutDHCPServer->addWidget(m_pLabelDHCPAddress, 1, 1);
            }
            /* Create DHCP address editor: */
            m_pEditorDHCPAddress = new QILineEdit;
            AssertPtrReturnVoid(m_pEditorDHCPAddress);
            {
                /* Configure editor: */
                m_pLabelDHCPAddress->setBuddy(m_pEditorDHCPAddress);
                connect(m_pEditorDHCPAddress, &QLineEdit::textChanged,
                        this, &UIHostNetworkDetailsDialog::sltTextChangedAddress);
                /* Add into layout: */
                pLayoutDHCPServer->addWidget(m_pEditorDHCPAddress, 1, 2);
            }
            /* Create DHCP network mask label: */
            m_pLabelDHCPMask = new QLabel;
            AssertPtrReturnVoid(m_pLabelDHCPMask);
            {
                /* Configure label: */
                m_pLabelDHCPMask->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
                /* Add into layout: */
                pLayoutDHCPServer->addWidget(m_pLabelDHCPMask, 2, 1);
            }
            /* Create DHCP network mask editor: */
            m_pEditorDHCPMask = new QILineEdit;
            AssertPtrReturnVoid(m_pEditorDHCPMask);
            {
                /* Configure editor: */
                m_pLabelDHCPMask->setBuddy(m_pEditorDHCPMask);
                connect(m_pEditorDHCPMask, &QLineEdit::textChanged,
                        this, &UIHostNetworkDetailsDialog::sltTextChangedMask);
                /* Add into layout: */
                pLayoutDHCPServer->addWidget(m_pEditorDHCPMask, 2, 2);
            }
            /* Create DHCP lower address label: */
            m_pLabelDHCPLowerAddress = new QLabel;
            AssertPtrReturnVoid(m_pLabelDHCPLowerAddress);
            {
                /* Configure label: */
                m_pLabelDHCPLowerAddress->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
                /* Add into layout: */
                pLayoutDHCPServer->addWidget(m_pLabelDHCPLowerAddress, 3, 1);
            }
            /* Create DHCP lower address editor: */
            m_pEditorDHCPLowerAddress = new QILineEdit;
            AssertPtrReturnVoid(m_pEditorDHCPLowerAddress);
            {
                /* Configure editor: */
                m_pLabelDHCPLowerAddress->setBuddy(m_pEditorDHCPLowerAddress);
                connect(m_pEditorDHCPLowerAddress, &QLineEdit::textChanged,
                        this, &UIHostNetworkDetailsDialog::sltTextChangedLowerAddress);
                /* Add into layout: */
                pLayoutDHCPServer->addWidget(m_pEditorDHCPLowerAddress, 3, 2);
            }
            /* Create DHCP upper address label: */
            m_pLabelDHCPUpperAddress = new QLabel;
            AssertPtrReturnVoid(m_pLabelDHCPUpperAddress);
            {
                /* Configure label: */
                m_pLabelDHCPUpperAddress->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
                /* Add into layout: */
                pLayoutDHCPServer->addWidget(m_pLabelDHCPUpperAddress, 4, 1);
            }
            /* Create DHCP upper address editor: */
            m_pEditorDHCPUpperAddress = new QILineEdit;
            AssertPtrReturnVoid(m_pEditorDHCPUpperAddress);
            {
                /* Configure editor: */
                m_pLabelDHCPUpperAddress->setBuddy(m_pEditorDHCPUpperAddress);
                connect(m_pEditorDHCPUpperAddress, &QLineEdit::textChanged,
                        this, &UIHostNetworkDetailsDialog::sltTextChangedUpperAddress);
                /* Add into layout: */
                pLayoutDHCPServer->addWidget(m_pEditorDHCPUpperAddress, 4, 2);
            }
            /* Create indent: */
            QStyleOption options;
            options.initFrom(m_pCheckBoxDHCP);
            const int iWidth2 = m_pCheckBoxDHCP->style()->pixelMetric(QStyle::PM_IndicatorWidth, &options, m_pCheckBoxDHCP) +
                                m_pCheckBoxDHCP->style()->pixelMetric(QStyle::PM_CheckBoxLabelSpacing, &options, m_pCheckBoxDHCP) -
                                pLayoutDHCPServer->spacing() - 1;
            QSpacerItem *pSpacer2 = new QSpacerItem(iWidth2, 0, QSizePolicy::Fixed, QSizePolicy::Expanding);
            AssertPtrReturnVoid(pSpacer2);
            {
                /* Add into layout: */
                pLayoutDHCPServer->addItem(pSpacer2, 1, 0, 4);
            }
            /* Create stretch: */
            QSpacerItem *pSpacer3 = new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
            AssertPtrReturnVoid(pSpacer3);
            {
                /* Add into layout: */
                pLayoutDHCPServer->addItem(pSpacer3, 5, 0, 1, 3);
            }
        }
        /* Add to tab-widget: */
        m_pTabWidget->addTab(pTabDHCPServer, QString());
    }
}

void UIHostNetworkDetailsDialog::loadDataForInterface()
{
    /* Load IPv4 interface fields: */
    m_pEditorIPv4->setText(m_newData.m_interface.m_strAddress);
    m_pEditorNMv4->setText(m_newData.m_interface.m_strMask);

    /* Toggle IPv6 interface fields availability: */
    const bool fIsIpv6Supported = m_newData.m_interface.m_fSupportedIPv6;
    m_pLabelIPv6->setEnabled(fIsIpv6Supported);
    m_pLabelNMv6->setEnabled(fIsIpv6Supported);
    m_pEditorIPv6->setEnabled(fIsIpv6Supported);
    m_pEditorNMv6->setEnabled(fIsIpv6Supported);

    /* Load IPv6 interface fields: */
    m_pEditorIPv6->setText(m_newData.m_interface.m_strAddress6);
    m_pEditorNMv6->setText(m_newData.m_interface.m_strMaskLength6);
}

void UIHostNetworkDetailsDialog::loadDataForDHCPServer()
{
    /* Toggle DHCP server fields availability: */
    const bool fIsDHCPServerEnabled = m_newData.m_dhcpserver.m_fEnabled;
    m_pLabelDHCPAddress->setEnabled(fIsDHCPServerEnabled);
    m_pLabelDHCPMask->setEnabled(fIsDHCPServerEnabled);
    m_pLabelDHCPLowerAddress->setEnabled(fIsDHCPServerEnabled);
    m_pLabelDHCPUpperAddress->setEnabled(fIsDHCPServerEnabled);
    m_pEditorDHCPAddress->setEnabled(fIsDHCPServerEnabled);
    m_pEditorDHCPMask->setEnabled(fIsDHCPServerEnabled);
    m_pEditorDHCPLowerAddress->setEnabled(fIsDHCPServerEnabled);
    m_pEditorDHCPUpperAddress->setEnabled(fIsDHCPServerEnabled);

    /* Load DHCP server fields: */
    m_pCheckBoxDHCP->setChecked(fIsDHCPServerEnabled);
    m_pEditorDHCPAddress->setText(m_newData.m_dhcpserver.m_strAddress);
    m_pEditorDHCPMask->setText(m_newData.m_dhcpserver.m_strMask);
    m_pEditorDHCPLowerAddress->setText(m_newData.m_dhcpserver.m_strLowerAddress);
    m_pEditorDHCPUpperAddress->setText(m_newData.m_dhcpserver.m_strUpperAddress);
}

void UIHostNetworkDetailsDialog::notify()
{
//    if (m_oldData != m_newData)
//        printf("Interface: %s, %s, %s, %s;  DHCP server: %d, %s, %s, %s, %s\n",
//               m_newData.m_interface.m_strAddress.toUtf8().constData(),
//               m_newData.m_interface.m_strMask.toUtf8().constData(),
//               m_newData.m_interface.m_strAddress6.toUtf8().constData(),
//               m_newData.m_interface.m_strMaskLength6.toUtf8().constData(),
//               (int)m_newData.m_dhcpserver.m_fEnabled,
//               m_newData.m_dhcpserver.m_strAddress.toUtf8().constData(),
//               m_newData.m_dhcpserver.m_strMask.toUtf8().constData(),
//               m_newData.m_dhcpserver.m_strLowerAddress.toUtf8().constData(),
//               m_newData.m_dhcpserver.m_strUpperAddress.toUtf8().constData());

    emit sigDataChanged(m_oldData != m_newData);
}

/* static */
quint32 UIHostNetworkDetailsDialog::ipv4FromQStringToQuint32(const QString &strAddress)
{
    quint32 uAddress = 0;
    foreach (const QString &strPart, strAddress.split('.'))
    {
        uAddress = uAddress << 8;
        bool fOk = false;
        uint uPart = strPart.toUInt(&fOk);
        if (fOk)
            uAddress += uPart;
    }
    return uAddress;
}

/* static */
QString UIHostNetworkDetailsDialog::ipv4FromQuint32ToQString(quint32 uAddress)
{
    QStringList address;
    while (uAddress)
    {
        uint uPart = uAddress & 0xFF;
        address.prepend(QString::number(uPart));
        uAddress = uAddress >> 8;
    }
    return address.join('.');
}

