/* $Id$ */
/** @file
 * VBox Qt GUI - UIGlobalSettingsNetworkDetailsNAT class implementation.
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

/* Qt includes: */
#include <QRegExpValidator>

/* GUI includes: */
#include "UIGlobalSettingsNetwork.h"
#include "UIGlobalSettingsNetworkDetailsNAT.h"
#include "UIGlobalSettingsPortForwardingDlg.h"

UIGlobalSettingsNetworkDetailsNAT::UIGlobalSettingsNetworkDetailsNAT(QWidget *pParent, UIDataNetworkNAT &data)
    : QIWithRetranslateUI2<QIDialog>(pParent)
    , m_data(data)
{
    /* Apply UI decorations: */
    Ui::UIGlobalSettingsNetworkDetailsNAT::setupUi(this);

    /* Setup dialog: */
    setWindowIcon(QIcon(":/guesttools_16px.png"));

    /* Setup validators: */
    const QString strTemplateName("\\S+");
    const QString strTemplateCIDRPrefix1("([1-9]|[1-9][0-9]|1[0-9][0-9]|2[0-4][0-9]|25[0-5])");
    const QString strTemplateCIDRPrefix2("([0-9]|[1-9][0-9]|1[0-9][0-9]|2[0-4][0-9]|25[0-5])");
    const QString strTemplateCIDRPostfix("([1-9]|1[0-9]|2[0-9]|3[0-2])");
    const QString strTemplateCIDR("%1(\\.%2(\\.%2(\\.%2)?)?)?/%3");
    const QString strTemplateCIDRFull = strTemplateCIDR.arg(strTemplateCIDRPrefix1, strTemplateCIDRPrefix2, strTemplateCIDRPostfix);
    m_pEditorNetworkName->setValidator(new QRegExpValidator(QRegExp(strTemplateName), this));
    m_pEditorNetworkCIDR->setValidator(new QRegExpValidator(QRegExp(strTemplateCIDRFull), this));

    /* Apply language settings: */
    retranslateUi();

    /* Load: */
    load();

    /* Fix minimum possible size: */
    resize(minimumSizeHint());
    setFixedSize(minimumSizeHint());
}

void UIGlobalSettingsNetworkDetailsNAT::retranslateUi()
{
    /* Translate uic generated strings: */
    Ui::UIGlobalSettingsNetworkDetailsNAT::retranslateUi(this);
}

void UIGlobalSettingsNetworkDetailsNAT::polishEvent(QShowEvent*)
{
    /* Update availability: */
    m_pCheckboxAdvertiseDefaultIPv6Route->setEnabled(m_pCheckboxSupportsIPv6->isChecked());
    m_pContainerOptions->setEnabled(m_pCheckboxNetwork->isChecked());
}

void UIGlobalSettingsNetworkDetailsNAT::sltEditPortForwarding()
{
    /* Open dialog to edit port-forwarding rules: */
    UIGlobalSettingsPortForwardingDlg dlg(this, m_data.m_ipv4rules, m_data.m_ipv6rules);
    if (dlg.exec() == QDialog::Accepted)
    {
        m_data.m_ipv4rules = dlg.ipv4rules();
        m_data.m_ipv6rules = dlg.ipv6rules();
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
    m_data.m_strNewName = m_pEditorNetworkName->text();
    m_data.m_strCIDR = m_pEditorNetworkCIDR->text();
    m_data.m_fSupportsDHCP = m_pCheckboxSupportsDHCP->isChecked();
    m_data.m_fSupportsIPv6 = m_pCheckboxSupportsIPv6->isChecked();
    m_data.m_fAdvertiseDefaultIPv6Route = m_pCheckboxAdvertiseDefaultIPv6Route->isChecked();
}

