/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIGlobalSettingsNetworkDetails class implementation
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

/* Global includes */
#include <QHostAddress>
#include <QRegExpValidator>

/* Local includes */
#include "UIGlobalSettingsNetwork.h"
#include "UIGlobalSettingsNetworkDetails.h"

/* Network page details constructor: */
UIGlobalSettingsNetworkDetails::UIGlobalSettingsNetworkDetails(QWidget *pParent)
    : QIWithRetranslateUI2<QIDialog>(pParent
#ifdef Q_WS_MAC
    ,Qt::Sheet
#endif /* Q_WS_MAC */
    )
    , m_pItem(0)
{
    /* Apply UI decorations: */
    Ui::UIGlobalSettingsNetworkDetails::setupUi(this);

    /* Setup dialog: */
    setWindowIcon(QIcon(":/guesttools_16px.png"));

    /* Setup validators: */
    QString strTemplateIPv4("([0-9]|[1-9][0-9]|1[0-9][0-9]|2[0-4][0-9]|25[0-5])\\."
                            "([0-9]|[1-9][0-9]|1[0-9][0-9]|2[0-4][0-9]|25[0-5])\\."
                            "([0-9]|[1-9][0-9]|1[0-9][0-9]|2[0-4][0-9]|25[0-5])\\."
                            "([0-9]|[1-9][0-9]|1[0-9][0-9]|2[0-4][0-9]|25[0-5])");
    QString strTemplateIPv6("[0-9a-fA-Z]{1,4}:{1,2}[0-9a-fA-Z]{1,4}:{1,2}"
                            "[0-9a-fA-Z]{1,4}:{1,2}[0-9a-fA-Z]{1,4}:{1,2}"
                            "[0-9a-fA-Z]{1,4}:{1,2}[0-9a-fA-Z]{1,4}:{1,2}"
                            "[0-9a-fA-Z]{1,4}:{1,2}[0-9a-fA-Z]{1,4}");

    m_pIPv4Editor->setValidator(new QRegExpValidator(QRegExp(strTemplateIPv4), this));
    m_pNMv4Editor->setValidator(new QRegExpValidator(QRegExp(strTemplateIPv4), this));
    m_pIPv6Editor->setValidator(new QRegExpValidator(QRegExp(strTemplateIPv6), this));
    m_pNMv6Editor->setValidator(new QRegExpValidator(QRegExp("[1-9][0-9]|1[0-1][0-9]|12[0-8]"), this));
    m_pDhcpAddressEditor->setValidator(new QRegExpValidator(QRegExp(strTemplateIPv4), this));
    m_pDhcpMaskEditor->setValidator(new QRegExpValidator(QRegExp(strTemplateIPv4), this));
    m_pDhcpLowerAddressEditor->setValidator(new QRegExpValidator(QRegExp(strTemplateIPv4), this));
    m_pDhcpUpperAddressEditor->setValidator(new QRegExpValidator(QRegExp(strTemplateIPv4), this));

    /* Setup widgets */
    m_pIPv6Editor->setFixedWidthByText(QString().fill('X', 32) + QString().fill(':', 7));

#if 0 /* defined (Q_WS_WIN) */
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

    /* Fix minimum possible size: */
    resize(minimumSizeHint());
    qApp->processEvents();
    setFixedSize(minimumSizeHint());
}

/* Get data to details sub-dialog: */
void UIGlobalSettingsNetworkDetails::getFromItem(UIHostInterfaceItem *pItem)
{
    m_pItem = pItem;

    /* Host-only Interface: */
    m_pEnableManualCheckbox->setChecked(!m_pItem->isDhcpClientEnabled());
#if !0 /* !defined (Q_WS_WIN) */
    /* Disable automatic for all hosts for now: */
    m_pEnableManualCheckbox->setChecked(true);
    m_pEnableManualCheckbox->setEnabled(false);
#endif
    sltDhcpClientStatusChanged();

    /* DHCP Server: */
    m_pEnabledDhcpServerCheckbox->setChecked(m_pItem->isDhcpServerEnabled());
    sltDhcpServerStatusChanged();
}

/* Return data from details sub-dialog: */
void UIGlobalSettingsNetworkDetails::putBackToItem()
{
    /* Host-only Interface: */
    m_pItem->setDhcpClientEnabled(!m_pEnableManualCheckbox->isChecked());
    if (m_pEnableManualCheckbox->isChecked())
    {
        m_pItem->setInterfaceAddress(m_pIPv4Editor->text());
        m_pItem->setInterfaceMask(m_pNMv4Editor->text());
        if (m_pItem->isIpv6Supported())
        {
            m_pItem->setInterfaceAddress6(m_pIPv6Editor->text());
            m_pItem->setInterfaceMaskLength6(m_pNMv6Editor->text());
        }
    }

    /* DHCP Server: */
    m_pItem->setDhcpServerEnabled(m_pEnabledDhcpServerCheckbox->isChecked());
    if (m_pEnabledDhcpServerCheckbox->isChecked())
    {
        m_pItem->setDhcpServerAddress(m_pDhcpAddressEditor->text());
        m_pItem->setDhcpServerMask(m_pDhcpMaskEditor->text());
        m_pItem->setDhcpLowerAddress(m_pDhcpLowerAddressEditor->text());
        m_pItem->setDhcpUpperAddress(m_pDhcpUpperAddressEditor->text());
    }
}

/* Validation stuff: */
void UIGlobalSettingsNetworkDetails::retranslateUi()
{
    /* Translate uic generated strings: */
    Ui::UIGlobalSettingsNetworkDetails::retranslateUi(this);
}

/* Handler for DHCP client settings change: */
void UIGlobalSettingsNetworkDetails::sltDhcpClientStatusChanged()
{
    bool fIsManual = m_pEnableManualCheckbox->isChecked();
    bool fIsIpv6Supported = fIsManual && m_pItem->isIpv6Supported();

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
        m_pIPv4Editor->setText(m_pItem->interfaceAddress());
        m_pNMv4Editor->setText(m_pItem->interfaceMask());
        if (fIsIpv6Supported)
        {
            m_pIPv6Editor->setText(m_pItem->interfaceAddress6());
            m_pNMv6Editor->setText(m_pItem->interfaceMaskLength6());
        }
    }
}

/* Handler for DHCP server settings change: */
void UIGlobalSettingsNetworkDetails::sltDhcpServerStatusChanged()
{
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
        m_pDhcpAddressEditor->setText(m_pItem->dhcpServerAddress());
        m_pDhcpMaskEditor->setText(m_pItem->dhcpServerMask());
        m_pDhcpLowerAddressEditor->setText(m_pItem->dhcpLowerAddress());
        m_pDhcpUpperAddressEditor->setText(m_pItem->dhcpUpperAddress());
    }
}

