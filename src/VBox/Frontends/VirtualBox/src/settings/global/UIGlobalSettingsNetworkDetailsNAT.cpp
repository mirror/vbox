/* $Id$ */
/** @file
 * VBox Qt GUI - UIGlobalSettingsNetworkDetailsNAT class implementation.
 */

/*
 * Copyright (C) 2009-2020 Oracle Corporation
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
#include <QCheckBox>
#include <QRegExpValidator>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

/* GUI includes: */
#include "QIDialogButtonBox.h"
#include "UIGlobalSettingsNetwork.h"
#include "UIGlobalSettingsNetworkDetailsNAT.h"
#include "UIGlobalSettingsPortForwardingDlg.h"


UIGlobalSettingsNetworkDetailsNAT::UIGlobalSettingsNetworkDetailsNAT(QWidget *pParent,
                                                                     UIDataSettingsGlobalNetworkNAT &data,
                                                                     UIPortForwardingDataList &ipv4rules,
                                                                     UIPortForwardingDataList &ipv6rules)
    : QIWithRetranslateUI2<QIDialog>(pParent)
    , m_data(data)
    , m_ipv4rules(ipv4rules)
    , m_ipv6rules(ipv6rules)
    , m_pCheckboxNetwork(0)
    , m_pWidgetSettings(0)
    , m_pLabelNetworkName(0)
    , m_pEditorNetworkName(0)
    , m_pLabelNetworkCIDR(0)
    , m_pEditorNetworkCIDR(0)
    , m_pLabelExtended(0)
    , m_pCheckboxSupportsDHCP(0)
    , m_pCheckboxSupportsIPv6(0)
    , m_pCheckboxAdvertiseDefaultIPv6Route(0)
    , m_pButtonPortForwarding(0)
    , m_pButtonBox(0)
{
    prepare();
}

void UIGlobalSettingsNetworkDetailsNAT::prepare()
{
    /* Setup dialog: */
    setWindowIcon(QIcon(":/guesttools_16px.png"));

    /* Prepare everything: */
    prepareWidgets();
    prepareConnections();

    /* Apply language settings: */
    retranslateUi();

    /* Load: */
    load();

    /* Adjust dialog size: */
    adjustSize();

#ifdef VBOX_WS_MAC
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setFixedSize(minimumSize());
#endif /* VBOX_WS_MAC */
}

void UIGlobalSettingsNetworkDetailsNAT::prepareWidgets()
{
    /* Prepare main layout: */
    QGridLayout *pLayoutMain = new QGridLayout(this);
    if (pLayoutMain)
    {
        pLayoutMain->setRowStretch(2, 1);

        /* Prepare network check-box: */
        m_pCheckboxNetwork = new QCheckBox(this);
        pLayoutMain->addWidget(m_pCheckboxNetwork, 0, 0, 1, 2);

        /* Prepare 20-px shifting spacer: */
        QSpacerItem *pSpacerItem = new QSpacerItem(20, 0, QSizePolicy::Fixed, QSizePolicy::Minimum);
        if (pSpacerItem)
            pLayoutMain->addItem(pSpacerItem, 1, 0);

        /* Prepare settings widget: */
        m_pWidgetSettings = new QWidget(this);
        if (m_pWidgetSettings)
        {
            /* Prepare settings widget layout: */
            QGridLayout *pLayoutSettings = new QGridLayout(m_pWidgetSettings);
            if (pLayoutSettings)
            {
                pLayoutSettings->setContentsMargins(0, 0, 0, 0);
                pLayoutSettings->setColumnStretch(3, 1);

                /* Prepare network name label: */
                m_pLabelNetworkName = new QLabel(m_pWidgetSettings);
                if (m_pLabelNetworkName)
                {
                    m_pLabelNetworkName->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    pLayoutSettings->addWidget(m_pLabelNetworkName, 0, 0);
                }
                /* Prepare network name editor: */
                m_pEditorNetworkName = new QLineEdit(m_pWidgetSettings);
                if (m_pEditorNetworkName)
                {
                    if (m_pLabelNetworkName)
                        m_pLabelNetworkName->setBuddy(m_pEditorNetworkName);
                    pLayoutSettings->addWidget(m_pEditorNetworkName, 0, 1, 1, 3);
                }

                /* Prepare network CIDR label: */
                m_pLabelNetworkCIDR = new QLabel(m_pWidgetSettings);
                if (m_pLabelNetworkCIDR)
                {
                    m_pLabelNetworkCIDR->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    pLayoutSettings->addWidget(m_pLabelNetworkCIDR, 1, 0);
                }
                /* Prepare network CIDR editor: */
                m_pEditorNetworkCIDR = new QLineEdit(m_pWidgetSettings);
                if (m_pEditorNetworkCIDR)
                {
                    if (m_pLabelNetworkCIDR)
                        m_pLabelNetworkCIDR->setBuddy(m_pEditorNetworkCIDR);
                    pLayoutSettings->addWidget(m_pEditorNetworkCIDR, 1, 1, 1, 3);
                }

                /* Prepare extended label: */
                m_pLabelExtended = new QLabel(m_pWidgetSettings);
                if (m_pLabelExtended)
                {
                    m_pLabelExtended->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    pLayoutSettings->addWidget(m_pLabelExtended, 2, 0);
                }
                /* Prepare 'supports DHCP' check-box: */
                m_pCheckboxSupportsDHCP = new QCheckBox(m_pWidgetSettings);
                if (m_pCheckboxSupportsDHCP)
                    pLayoutSettings->addWidget(m_pCheckboxSupportsDHCP, 2, 1, 1, 2);
                /* Prepare 'supports IPv6' check-box: */
                m_pCheckboxSupportsIPv6 = new QCheckBox(m_pWidgetSettings);
                if (m_pCheckboxSupportsIPv6)
                    pLayoutSettings->addWidget(m_pCheckboxSupportsIPv6, 3, 1, 1, 2);
                /* Prepare 'advertise default IPv6 route' check-box: */
                m_pCheckboxAdvertiseDefaultIPv6Route = new QCheckBox(m_pWidgetSettings);
                if (m_pCheckboxAdvertiseDefaultIPv6Route)
                    pLayoutSettings->addWidget(m_pCheckboxAdvertiseDefaultIPv6Route, 4, 1, 1, 2);

                /* Prepare port forwarding button: */
                m_pButtonPortForwarding = new QPushButton(m_pWidgetSettings);
                if (m_pButtonPortForwarding)
                    pLayoutSettings->addWidget(m_pButtonPortForwarding, 5, 1);
            }

            pLayoutMain->addWidget(m_pWidgetSettings, 1, 1);
        }

        /* Prepare dialog button-box button: */
        m_pButtonBox = new QIDialogButtonBox(this);
        if (m_pButtonBox)
        {
            m_pButtonBox->setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
            pLayoutMain->addWidget(m_pButtonBox, 3, 0, 1, 2);
        }
    }
}

void UIGlobalSettingsNetworkDetailsNAT::prepareConnections()
{
    connect(m_pCheckboxNetwork, &QCheckBox::toggled, m_pWidgetSettings, &QWidget::setEnabled);
    connect(m_pCheckboxSupportsIPv6, &QCheckBox::toggled, m_pCheckboxAdvertiseDefaultIPv6Route, &QCheckBox::setEnabled);
    connect(m_pButtonPortForwarding, &QPushButton::clicked, this, &UIGlobalSettingsNetworkDetailsNAT::sltEditPortForwarding);
    connect(m_pButtonBox, &QIDialogButtonBox::accepted, this, &UIGlobalSettingsNetworkDetailsNAT::accept);
    connect(m_pButtonBox, &QIDialogButtonBox::rejected, this, &UIGlobalSettingsNetworkDetailsNAT::reject);
}

void UIGlobalSettingsNetworkDetailsNAT::retranslateUi()
{
    setWindowTitle(tr("NAT Network Details"));
    m_pCheckboxNetwork->setText(tr("&Enable Network"));
    m_pCheckboxNetwork->setToolTip(tr("When checked, this network will be enabled."));
    m_pLabelNetworkName->setText(tr("Network &Name:"));
    m_pEditorNetworkName->setToolTip(tr("Holds the name for this network."));
    m_pLabelNetworkCIDR->setText(tr("Network &CIDR:"));
    m_pEditorNetworkCIDR->setToolTip(tr("Holds the CIDR for this network."));
    m_pLabelExtended->setText(tr("Network Options:"));
    m_pCheckboxSupportsDHCP->setText(tr("Supports &DHCP"));
    m_pCheckboxSupportsDHCP->setToolTip(tr("When checked, this network will support DHCP."));
    m_pCheckboxSupportsIPv6->setText(tr("Supports &IPv6"));
    m_pCheckboxSupportsIPv6->setToolTip(tr("When checked, this network will support IPv6."));
    m_pCheckboxAdvertiseDefaultIPv6Route->setText(tr("Advertise Default IPv6 &Route"));
    m_pCheckboxAdvertiseDefaultIPv6Route->setToolTip(tr("When checked, this network will be advertised as the default IPv6 route."));
    m_pButtonPortForwarding->setToolTip(tr("Displays a window to configure port forwarding rules."));
    m_pButtonPortForwarding->setText(tr("&Port Forwarding"));
}

void UIGlobalSettingsNetworkDetailsNAT::polishEvent(QShowEvent *pEvent)
{
    /* Call to base-class: */
    QIWithRetranslateUI2<QIDialog>::polishEvent(pEvent);

    /* Update availability: */
    m_pCheckboxAdvertiseDefaultIPv6Route->setEnabled(m_pCheckboxSupportsIPv6->isChecked());
    m_pWidgetSettings->setEnabled(m_pCheckboxNetwork->isChecked());
}

void UIGlobalSettingsNetworkDetailsNAT::sltEditPortForwarding()
{
    /* Open dialog to edit port-forwarding rules: */
    UIGlobalSettingsPortForwardingDlg dlg(this, m_ipv4rules, m_ipv6rules);
    if (dlg.exec() == QDialog::Accepted)
    {
        m_ipv4rules = dlg.ipv4rules();
        m_ipv6rules = dlg.ipv6rules();
    }
}

void UIGlobalSettingsNetworkDetailsNAT::accept()
{
    /* Save before accept: */
    save();
    /* Call to base-class: */
    QIDialog::accept();
}

void UIGlobalSettingsNetworkDetailsNAT::load()
{
    /* NAT Network: */
    m_pCheckboxNetwork->setChecked(m_data.m_fEnabled);
    m_pEditorNetworkName->setText(m_data.m_strNewName);
    m_pEditorNetworkCIDR->setText(m_data.m_strCIDR);
    m_pCheckboxSupportsDHCP->setChecked(m_data.m_fSupportsDHCP);
    m_pCheckboxSupportsIPv6->setChecked(m_data.m_fSupportsIPv6);
    m_pCheckboxAdvertiseDefaultIPv6Route->setChecked(m_data.m_fAdvertiseDefaultIPv6Route);
}

void UIGlobalSettingsNetworkDetailsNAT::save()
{
    /* NAT Network: */
    m_data.m_fEnabled = m_pCheckboxNetwork->isChecked();
    m_data.m_strNewName = m_pEditorNetworkName->text().trimmed();
    m_data.m_strCIDR = m_pEditorNetworkCIDR->text().trimmed();
    m_data.m_fSupportsDHCP = m_pCheckboxSupportsDHCP->isChecked();
    m_data.m_fSupportsIPv6 = m_pCheckboxSupportsIPv6->isChecked();
    m_data.m_fAdvertiseDefaultIPv6Route = m_pCheckboxAdvertiseDefaultIPv6Route->isChecked();
}
