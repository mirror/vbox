/* $Id$ */
/** @file
 * VBox Qt GUI - UIGlobalSettingsNetworkDetailsHost class implementation.
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

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QRegExpValidator>

/* GUI includes: */
# include "UIGlobalSettingsNetwork.h"
# include "UIGlobalSettingsNetworkDetailsHost.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIGlobalSettingsNetworkDetailsHost::UIGlobalSettingsNetworkDetailsHost(QWidget *pParent, UIDataNetworkHost &data)
    : QIWithRetranslateUI2<QIDialog>(pParent)
    , m_data(data)
{
    /* Apply UI decorations: */
    Ui::UIGlobalSettingsNetworkDetailsHost::setupUi(this);

    /* Setup dialog: */
    setWindowIcon(QIcon(":/guesttools_16px.png"));

    /* Setup widgets */
    m_pIPv6Editor->setFixedWidthByText(QString().fill('X', 32) + QString().fill(':', 7));

#if 0 /* defined (VBOX_WS_WIN) */
    QStyleOption options1;
    options1.initFrom(m_pEnableManualCheckbox);
    QGridLayout *playout1 = qobject_cast<QGridLayout*>(m_pDetailsTabWidget->widget(0)->layout());
    int iWid1 = m_pEnableManualCheckbox->style()->pixelMetric(QStyle::PM_IndicatorWidth, &options1, m_pEnableManualCheckbox) +
                m_pEnableManualCheckbox->style()->pixelMetric(QStyle::PM_CheckBoxLabelSpacing, &options1, m_pEnableManualCheckbox) -
                playout1->spacing() - 1;
    QSpacerItem *spacer1 = new QSpacerItem(iWid1, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
    playout1->addItem(spacer1, 1, 0, 4);
#else
    m_pEnableManualCheckbox->setVisible(false);
#endif

    QStyleOption options2;
    options2.initFrom(m_pEnabledDhcpServerCheckbox);
    QGridLayout *pLayout2 = qobject_cast<QGridLayout*>(m_pDetailsTabWidget->widget(1)->layout());
    int wid2 = m_pEnabledDhcpServerCheckbox->style()->pixelMetric(QStyle::PM_IndicatorWidth, &options2, m_pEnabledDhcpServerCheckbox) +
               m_pEnabledDhcpServerCheckbox->style()->pixelMetric(QStyle::PM_CheckBoxLabelSpacing, &options2, m_pEnabledDhcpServerCheckbox) -
               pLayout2->spacing() - 1;
    QSpacerItem *pSpacer2 = new QSpacerItem(wid2, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
    pLayout2->addItem(pSpacer2, 1, 0, 4);

    /* Setup connections: */
    connect(m_pEnableManualCheckbox, SIGNAL(stateChanged(int)), this, SLOT (sltDhcpClientStatusChanged()));
    connect(m_pEnabledDhcpServerCheckbox, SIGNAL(stateChanged (int)), this, SLOT(sltDhcpServerStatusChanged()));

    /* Apply language settings: */
    retranslateUi();

    /* Load: */
    load();

    /* Fix minimum possible size: */
    resize(minimumSizeHint());
    setFixedSize(minimumSizeHint());
}

void UIGlobalSettingsNetworkDetailsHost::retranslateUi()
{
    /* Translate uic generated strings: */
    Ui::UIGlobalSettingsNetworkDetailsHost::retranslateUi(this);
}

void UIGlobalSettingsNetworkDetailsHost::accept()
{
    /* Save before accept: */
    save();
    /* Call to base-class: */
    QIDialog::accept();
}

void UIGlobalSettingsNetworkDetailsHost::load()
{
    /* Host Interface: */
    m_pEnableManualCheckbox->setChecked(!m_data.m_interface.m_fDhcpClientEnabled);
#if !0 /* !defined (VBOX_WS_WIN) */
    /* Disable automatic for all hosts for now: */
    m_pEnableManualCheckbox->setChecked(true);
    m_pEnableManualCheckbox->setEnabled(false);
#endif
    loadClientStuff();

    /* DHCP Server: */
    m_pEnabledDhcpServerCheckbox->setChecked(m_data.m_dhcpserver.m_fDhcpServerEnabled);
    loadServerStuff();
}

void UIGlobalSettingsNetworkDetailsHost::loadClientStuff()
{
    /* Adjust network interface fields: */
    bool fIsManual = m_pEnableManualCheckbox->isChecked();
    bool fIsIpv6Supported = fIsManual && m_data.m_interface.m_fIpv6Supported;

    m_pIPv4Editor->clear();
    m_pNMv4Editor->clear();
    m_pIPv6Editor->clear();
    m_pNMv6Editor->clear();

    m_pIPv4Label->setEnabled(fIsManual);
    m_pNMv4Label->setEnabled(fIsManual);
    m_pIPv4Editor->setEnabled(fIsManual);
    m_pNMv4Editor->setEnabled(fIsManual);
    m_pIPv6Label->setEnabled(fIsIpv6Supported);
    m_pNMv6Label->setEnabled(fIsIpv6Supported);
    m_pIPv6Editor->setEnabled(fIsIpv6Supported);
    m_pNMv6Editor->setEnabled(fIsIpv6Supported);

    if (fIsManual)
    {
        m_pIPv4Editor->setText(m_data.m_interface.m_strInterfaceAddress);
        m_pNMv4Editor->setText(m_data.m_interface.m_strInterfaceMask);
        if (fIsIpv6Supported)
        {
            m_pIPv6Editor->setText(m_data.m_interface.m_strInterfaceAddress6);
            m_pNMv6Editor->setText(m_data.m_interface.m_strInterfaceMaskLength6);
        }
    }
}

void UIGlobalSettingsNetworkDetailsHost::loadServerStuff()
{
    /* Adjust dhcp server fields: */
    bool fIsManual = m_pEnabledDhcpServerCheckbox->isChecked();

    m_pDhcpAddressEditor->clear();
    m_pDhcpMaskEditor->clear();
    m_pDhcpLowerAddressEditor->clear();
    m_pDhcpUpperAddressEditor->clear();

    m_pDhcpAddressLabel->setEnabled(fIsManual);
    m_pDhcpMaskLabel->setEnabled(fIsManual);
    m_pDhcpLowerAddressLabel->setEnabled(fIsManual);
    m_pDhcpUpperAddressLabel->setEnabled(fIsManual);
    m_pDhcpAddressEditor->setEnabled(fIsManual);
    m_pDhcpMaskEditor->setEnabled(fIsManual);
    m_pDhcpLowerAddressEditor->setEnabled(fIsManual);
    m_pDhcpUpperAddressEditor->setEnabled(fIsManual);

    if (fIsManual)
    {
        m_pDhcpAddressEditor->setText(m_data.m_dhcpserver.m_strDhcpServerAddress);
        m_pDhcpMaskEditor->setText(m_data.m_dhcpserver.m_strDhcpServerMask);
        m_pDhcpLowerAddressEditor->setText(m_data.m_dhcpserver.m_strDhcpLowerAddress);
        m_pDhcpUpperAddressEditor->setText(m_data.m_dhcpserver.m_strDhcpUpperAddress);
    }
}

void UIGlobalSettingsNetworkDetailsHost::save()
{
    /* Host Interface: */
    m_data.m_interface.m_fDhcpClientEnabled = !m_pEnableManualCheckbox->isChecked();
    if (m_pEnableManualCheckbox->isChecked())
    {
        m_data.m_interface.m_strInterfaceAddress = m_pIPv4Editor->text();
        m_data.m_interface.m_strInterfaceMask = m_pNMv4Editor->text();
        if (m_data.m_interface.m_fIpv6Supported)
        {
            m_data.m_interface.m_strInterfaceAddress6 = m_pIPv6Editor->text();
            m_data.m_interface.m_strInterfaceMaskLength6 = m_pNMv6Editor->text();
        }
    }

    /* DHCP Server: */
    m_data.m_dhcpserver.m_fDhcpServerEnabled = m_pEnabledDhcpServerCheckbox->isChecked();
    if (m_pEnabledDhcpServerCheckbox->isChecked())
    {
        m_data.m_dhcpserver.m_strDhcpServerAddress = m_pDhcpAddressEditor->text();
        m_data.m_dhcpserver.m_strDhcpServerMask = m_pDhcpMaskEditor->text();
        m_data.m_dhcpserver.m_strDhcpLowerAddress = m_pDhcpLowerAddressEditor->text();
        m_data.m_dhcpserver.m_strDhcpUpperAddress = m_pDhcpUpperAddressEditor->text();
    }
}

