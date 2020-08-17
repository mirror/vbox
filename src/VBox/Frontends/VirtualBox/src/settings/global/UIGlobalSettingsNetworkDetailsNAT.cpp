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
    , m_pLabelNetworkName(0)
    , m_pLabelNetworkCIDR(0)
    , m_pLabelOptionsAdvanced(0)
    , m_pEditorNetworkName(0)
    , m_pEditorNetworkCIDR(0)
    , m_pCheckboxSupportsDHCP(0)
    , m_pCheckboxSupportsIPv6(0)
    , m_pCheckboxAdvertiseDefaultIPv6Route(0)
    , m_pButtonPortForwarding(0)
    , m_pContainerOptions(0)
{
    prepareWidgets();

    /* Setup dialog: */
    setWindowIcon(QIcon(":/guesttools_16px.png"));

    /* Apply language settings: */
    retranslateUi();

    /* Load: */
    load();

    /* Fix minimum possible size: */
    resize(minimumSizeHint());
    setFixedSize(minimumSizeHint());
}

void UIGlobalSettingsNetworkDetailsNAT::prepareWidgets()
{
    if (objectName().isEmpty())
        setObjectName(QStringLiteral("UIGlobalSettingsNetworkDetailsNAT"));
    QGridLayout *pGridLayout = new QGridLayout(this);
    pGridLayout->setObjectName(QStringLiteral("pGridLayout"));
    m_pCheckboxNetwork = new QCheckBox();
    m_pCheckboxNetwork->setObjectName(QStringLiteral("m_pCheckboxNetwork"));
    pGridLayout->addWidget(m_pCheckboxNetwork, 0, 0, 1, 1);

    m_pContainerOptions = new QWidget();
    m_pContainerOptions->setObjectName(QStringLiteral("m_pContainerOptions"));
    QGridLayout *pGridLayout1 = new QGridLayout(m_pContainerOptions);
    pGridLayout1->setObjectName(QStringLiteral("pGridLayout1"));
    pGridLayout1->setContentsMargins(0, 0, 0, 0);
    QSpacerItem *pSpacerItem = new QSpacerItem(20, 0, QSizePolicy::Fixed, QSizePolicy::Minimum);
    pGridLayout1->addItem(pSpacerItem, 0, 0, 1, 1);

    m_pLabelNetworkName = new QLabel(m_pContainerOptions);
    m_pLabelNetworkName->setObjectName(QStringLiteral("m_pLabelNetworkName"));
    m_pLabelNetworkName->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
    pGridLayout1->addWidget(m_pLabelNetworkName, 0, 1, 1, 1);

    m_pEditorNetworkName = new QLineEdit(m_pContainerOptions);
    m_pEditorNetworkName->setObjectName(QStringLiteral("m_pEditorNetworkName"));
    pGridLayout1->addWidget(m_pEditorNetworkName, 0, 2, 1, 2);

    m_pLabelNetworkCIDR = new QLabel(m_pContainerOptions);
    m_pLabelNetworkCIDR->setObjectName(QStringLiteral("m_pLabelNetworkCIDR"));
    m_pLabelNetworkCIDR->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
    pGridLayout1->addWidget(m_pLabelNetworkCIDR, 1, 1, 1, 1);

    m_pEditorNetworkCIDR = new QLineEdit(m_pContainerOptions);
    m_pEditorNetworkCIDR->setObjectName(QStringLiteral("m_pEditorNetworkCIDR"));
    pGridLayout1->addWidget(m_pEditorNetworkCIDR, 1, 2, 1, 2);

    m_pLabelOptionsAdvanced = new QLabel(m_pContainerOptions);
    m_pLabelOptionsAdvanced->setObjectName(QStringLiteral("m_pLabelOptionsAdvanced"));
    m_pLabelOptionsAdvanced->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignTop);
    pGridLayout1->addWidget(m_pLabelOptionsAdvanced, 2, 1, 1, 1);

    m_pCheckboxSupportsDHCP = new QCheckBox(m_pContainerOptions);
    m_pCheckboxSupportsDHCP->setObjectName(QStringLiteral("m_pCheckboxSupportsDHCP"));
    pGridLayout1->addWidget(m_pCheckboxSupportsDHCP, 2, 2, 1, 1);

    m_pCheckboxSupportsIPv6 = new QCheckBox(m_pContainerOptions);
    m_pCheckboxSupportsIPv6->setObjectName(QStringLiteral("m_pCheckboxSupportsIPv6"));
    pGridLayout1->addWidget(m_pCheckboxSupportsIPv6, 3, 2, 1, 1);

    m_pCheckboxAdvertiseDefaultIPv6Route = new QCheckBox(m_pContainerOptions);
    m_pCheckboxAdvertiseDefaultIPv6Route->setObjectName(QStringLiteral("m_pCheckboxAdvertiseDefaultIPv6Route"));
    pGridLayout1->addWidget(m_pCheckboxAdvertiseDefaultIPv6Route, 4, 2, 1, 1);

    m_pButtonPortForwarding = new QPushButton(m_pContainerOptions);
    m_pButtonPortForwarding->setObjectName(QStringLiteral("m_pButtonPortForwarding"));
    pGridLayout1->addWidget(m_pButtonPortForwarding, 5, 2, 1, 1);
    pGridLayout->addWidget(m_pContainerOptions, 1, 0, 1, 2);

    QSpacerItem *pSpacerItem1 = new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
    pGridLayout->addItem(pSpacerItem1, 2, 0, 1, 2);

    QIDialogButtonBox *pButtonBox = new QIDialogButtonBox();
    pButtonBox->setObjectName(QStringLiteral("pButtonBox"));
    pButtonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::NoButton|QDialogButtonBox::Ok);
    pGridLayout->addWidget(pButtonBox, 3, 0, 1, 2);

    m_pLabelNetworkName->setBuddy(m_pEditorNetworkName);
    m_pLabelNetworkCIDR->setBuddy(m_pEditorNetworkCIDR);

    QObject::connect(m_pCheckboxNetwork, &QCheckBox::toggled, m_pContainerOptions, &QWidget::setEnabled);
    QObject::connect(m_pCheckboxSupportsIPv6, &QCheckBox::toggled, m_pCheckboxAdvertiseDefaultIPv6Route, &QCheckBox::setEnabled);
    QObject::connect(m_pButtonPortForwarding, &QPushButton::clicked, this, &UIGlobalSettingsNetworkDetailsNAT::sltEditPortForwarding);
    QObject::connect(pButtonBox, &QIDialogButtonBox::accepted, this, &UIGlobalSettingsNetworkDetailsNAT::accept);
    QObject::connect(pButtonBox, &QIDialogButtonBox::rejected, this, &UIGlobalSettingsNetworkDetailsNAT::reject);

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
    m_pLabelOptionsAdvanced->setText(tr("Network Options:"));
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
    m_pContainerOptions->setEnabled(m_pCheckboxNetwork->isChecked());
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
