/* $Id$ */
/** @file
 * VBox Qt GUI - UIDetailsWidgetNATNetwork class implementation.
 */

/*
 * Copyright (C) 2009-2021 Oracle Corporation
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
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QRegExpValidator>
#include <QStyleOption>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIDialogButtonBox.h"
#include "QILineEdit.h"
#include "QITabWidget.h"
#include "UIIconPool.h"
#include "UIDetailsWidgetNATNetwork.h"
#include "UIMessageCenter.h"
#include "UINetworkManagerUtils.h"

/* Other VBox includes: */
#include "iprt/assert.h"
#include "iprt/cidr.h"


UIDetailsWidgetNATNetwork::UIDetailsWidgetNATNetwork(EmbedTo enmEmbedding, QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_enmEmbedding(enmEmbedding)
    , m_pTabWidget(0)
    , m_pCheckboxNetworkAvailable(0)
    , m_pLabelNetworkName(0)
    , m_pEditorNetworkName(0)
    , m_pLabelNetworkIPv4Prefix(0)
    , m_pEditorNetworkIPv4Prefix(0)
    , m_pLabelNetworkIPv6Prefix(0)
    , m_pEditorNetworkIPv6Prefix(0)
    , m_pLabelExtended(0)
    , m_pCheckboxSupportsDHCP(0)
    , m_pCheckboxSupportsIPv6(0)
    , m_pCheckboxAdvertiseDefaultIPv6Route(0)
    , m_pButtonBoxOptions(0)
    , m_pTabWidgetForwarding(0)
    , m_pForwardingTableIPv4(0)
    , m_pForwardingTableIPv6(0)
    , m_pButtonBoxForwarding(0)
    , m_fHoldPosition(false)
{
    prepare();
}

void UIDetailsWidgetNATNetwork::setData(const UIDataNATNetwork &data,
                                        const QStringList &busyNames /* = QStringList() */,
                                        bool fHoldPosition /* = false */)
{
    /* Cache old/new data: */
    m_oldData = data;
    m_newData = m_oldData;
    m_busyNames = busyNames;
    m_fHoldPosition = fHoldPosition;

    /* Load 'Options' data: */
    loadDataForOptions();
    /* Load 'Forwarding' data: */
    loadDataForForwarding();
}

bool UIDetailsWidgetNATNetwork::revalidate() const
{
    /* Make sure network name isn't empty: */
    if (m_newData.m_strName.isEmpty())
    {
        msgCenter().warnAboutNoNameSpecified(m_oldData.m_strName);
        return false;
    }
    else
    {
        /* Make sure item names are unique: */
        if (m_busyNames.contains(m_newData.m_strName))
        {
            msgCenter().warnAboutNameAlreadyBusy(m_newData.m_strName);
            return false;
        }
    }

    /* Make sure IPv4 prefix isn't empty: */
    if (m_newData.m_strPrefixIPv4.isEmpty())
    {
        msgCenter().warnAboutNoIPv4PrefixSpecified(m_newData.m_strName);
        return false;
    }
    /* Make sure IPv6 prefix isn't empty if IPv6 is supported: */
    if (m_newData.m_fSupportsIPv6 && m_newData.m_strPrefixIPv6.isEmpty())
    {
        msgCenter().warnAboutNoIPv4PrefixSpecified(m_newData.m_strName);
        return false;
    }

    /* Validate 'Forwarding' tab content: */
    return m_pForwardingTableIPv4->validate() && m_pForwardingTableIPv6->validate();
}

void UIDetailsWidgetNATNetwork::updateButtonStates()
{
//    if (m_oldData != m_newData)
//        printf("Network: %d, %s, %s, %s, %d, %d, %d\n",
//               m_newData.m_fEnabled,
//               m_newData.m_strName.toUtf8().constData(),
//               m_newData.m_strPrefixIPv4.toUtf8().constData(),
//               m_newData.m_strPrefixIPv6.toUtf8().constData(),
//               m_newData.m_fSupportsDHCP,
//               m_newData.m_fSupportsIPv6,
//               m_newData.m_fAdvertiseDefaultIPv6Route);

    /* Update 'Apply' / 'Reset' button states: */
    if (m_pButtonBoxOptions)
    {
        m_pButtonBoxOptions->button(QDialogButtonBox::Cancel)->setEnabled(m_oldData != m_newData);
        m_pButtonBoxOptions->button(QDialogButtonBox::Ok)->setEnabled(m_oldData != m_newData);
    }
    if (m_pButtonBoxForwarding)
    {
        m_pButtonBoxForwarding->button(QDialogButtonBox::Cancel)->setEnabled(m_oldData != m_newData);
        m_pButtonBoxForwarding->button(QDialogButtonBox::Ok)->setEnabled(m_oldData != m_newData);
    }

    /* Notify listeners as well: */
    emit sigDataChanged(m_oldData != m_newData);
}

void UIDetailsWidgetNATNetwork::retranslateUi()
{
    /* Translate tab-widget: */
    if (m_pTabWidget)
    {
        m_pTabWidget->setTabText(0, tr("&General Options"));
        m_pTabWidget->setTabText(1, tr("&Port Forwarding"));
    }

    /* Translate 'Options' tab content: */
    if (m_pCheckboxNetworkAvailable)
    {
        m_pCheckboxNetworkAvailable->setText(tr("&Enable Network"));
        m_pCheckboxNetworkAvailable->setToolTip(tr("When checked, this network will be enabled."));
    }
    if (m_pLabelNetworkName)
        m_pLabelNetworkName->setText(tr("&Name:"));
    if (m_pEditorNetworkName)
        m_pEditorNetworkName->setToolTip(tr("Holds the name for this network."));
    if (m_pLabelNetworkIPv4Prefix)
        m_pLabelNetworkIPv4Prefix->setText(tr("IPv&4 Prefix:"));
    if (m_pEditorNetworkIPv4Prefix)
        m_pEditorNetworkIPv4Prefix->setToolTip(tr("Holds the IPv4 prefix for this network."));
    if (m_pLabelNetworkIPv6Prefix)
        m_pLabelNetworkIPv6Prefix->setText(tr("IPv&6 Prefix:"));
    if (m_pEditorNetworkIPv6Prefix)
        m_pEditorNetworkIPv6Prefix->setToolTip(tr("Holds the IPv6 prefix for this network."));
    if (m_pLabelExtended)
        m_pLabelExtended->setText(tr("Extended Features:"));
    if (m_pCheckboxSupportsDHCP)
    {
        m_pCheckboxSupportsDHCP->setText(tr("Supports &DHCP"));
        m_pCheckboxSupportsDHCP->setToolTip(tr("When checked, this network will support DHCP."));
    }
    if (m_pCheckboxSupportsIPv6)
    {
        m_pCheckboxSupportsIPv6->setText(tr("Supports &IPv6"));
        m_pCheckboxSupportsIPv6->setToolTip(tr("When checked, this network will support IPv6."));
    }
    if (m_pCheckboxAdvertiseDefaultIPv6Route)
    {
        m_pCheckboxAdvertiseDefaultIPv6Route->setText(tr("Advertise Default IPv6 &Route"));
        m_pCheckboxAdvertiseDefaultIPv6Route->setToolTip(tr("When checked, this network will be advertised as the default IPv6 route."));
    }
    if (m_pButtonBoxOptions)
    {
        m_pButtonBoxOptions->button(QDialogButtonBox::Cancel)->setText(tr("Reset"));
        m_pButtonBoxOptions->button(QDialogButtonBox::Ok)->setText(tr("Apply"));
        m_pButtonBoxOptions->button(QDialogButtonBox::Cancel)->setShortcut(Qt::Key_Escape);
        m_pButtonBoxOptions->button(QDialogButtonBox::Ok)->setShortcut(QString("Ctrl+Return"));
        m_pButtonBoxOptions->button(QDialogButtonBox::Cancel)->setStatusTip(tr("Reset changes in current interface details"));
        m_pButtonBoxOptions->button(QDialogButtonBox::Ok)->setStatusTip(tr("Apply changes in current interface details"));
        m_pButtonBoxOptions->button(QDialogButtonBox::Cancel)->
            setToolTip(tr("Reset Changes (%1)").arg(m_pButtonBoxOptions->button(QDialogButtonBox::Cancel)->shortcut().toString()));
        m_pButtonBoxOptions->button(QDialogButtonBox::Ok)->
            setToolTip(tr("Apply Changes (%1)").arg(m_pButtonBoxOptions->button(QDialogButtonBox::Ok)->shortcut().toString()));
    }

    /* Translate 'Forwarding' tab content: */
    if (m_pTabWidgetForwarding)
    {
        m_pTabWidgetForwarding->setTabText(0, tr("IPv&4"));
        m_pTabWidgetForwarding->setTabText(1, tr("IPv&6"));
    }
    if (m_pButtonBoxForwarding)
    {
        m_pButtonBoxForwarding->button(QDialogButtonBox::Cancel)->setText(tr("Reset"));
        m_pButtonBoxForwarding->button(QDialogButtonBox::Ok)->setText(tr("Apply"));
        m_pButtonBoxForwarding->button(QDialogButtonBox::Cancel)->setShortcut(Qt::Key_Escape);
        m_pButtonBoxForwarding->button(QDialogButtonBox::Ok)->setShortcut(QString("Ctrl+Return"));
        m_pButtonBoxForwarding->button(QDialogButtonBox::Cancel)->setStatusTip(tr("Reset changes in current interface details"));
        m_pButtonBoxForwarding->button(QDialogButtonBox::Ok)->setStatusTip(tr("Apply changes in current interface details"));
        m_pButtonBoxForwarding->button(QDialogButtonBox::Cancel)->
            setToolTip(tr("Reset Changes (%1)").arg(m_pButtonBoxForwarding->button(QDialogButtonBox::Cancel)->shortcut().toString()));
        m_pButtonBoxForwarding->button(QDialogButtonBox::Ok)->
            setToolTip(tr("Apply Changes (%1)").arg(m_pButtonBoxForwarding->button(QDialogButtonBox::Ok)->shortcut().toString()));
    }
}

void UIDetailsWidgetNATNetwork::sltNetworkAvailabilityChanged(bool fChecked)
{
    m_newData.m_fEnabled = fChecked;
    loadDataForOptions();
    updateButtonStates();
}

void UIDetailsWidgetNATNetwork::sltNetworkNameChanged(const QString &strText)
{
    m_newData.m_strName = strText;
    updateButtonStates();
}

void UIDetailsWidgetNATNetwork::sltNetworkIPv4PrefixChanged(const QString &strText)
{
    m_newData.m_strPrefixIPv4 = strText;
    updateButtonStates();
}

void UIDetailsWidgetNATNetwork::sltNetworkIPv6PrefixChanged(const QString &strText)
{
    m_newData.m_strPrefixIPv6 = strText;
    updateButtonStates();
}

void UIDetailsWidgetNATNetwork::sltSupportsDHCPChanged(bool fChecked)
{
    m_newData.m_fSupportsDHCP = fChecked;
    updateButtonStates();
}

void UIDetailsWidgetNATNetwork::sltSupportsIPv6Changed(bool fChecked)
{
    m_newData.m_fSupportsIPv6 = fChecked;
    updateButtonStates();
}

void UIDetailsWidgetNATNetwork::sltAdvertiseDefaultIPv6RouteChanged(bool fChecked)
{
    m_newData.m_fAdvertiseDefaultIPv6Route = fChecked;
    updateButtonStates();
}

void UIDetailsWidgetNATNetwork::sltForwardingRulesIPv4Changed()
{
    m_newData.m_rules4 = m_pForwardingTableIPv4->rules();
    updateButtonStates();
}

void UIDetailsWidgetNATNetwork::sltForwardingRulesIPv6Changed()
{
    m_newData.m_rules6 = m_pForwardingTableIPv6->rules();
    updateButtonStates();
}

void UIDetailsWidgetNATNetwork::sltHandleButtonBoxClick(QAbstractButton *pButton)
{
    /* Make sure button-boxes exist: */
    if (!m_pButtonBoxOptions || !m_pButtonBoxForwarding)
        return;

    /* Disable buttons first of all: */
    m_pButtonBoxOptions->button(QDialogButtonBox::Cancel)->setEnabled(false);
    m_pButtonBoxOptions->button(QDialogButtonBox::Ok)->setEnabled(false);
    m_pButtonBoxForwarding->button(QDialogButtonBox::Cancel)->setEnabled(false);
    m_pButtonBoxForwarding->button(QDialogButtonBox::Ok)->setEnabled(false);

    /* Compare with known buttons: */
    if (   pButton == m_pButtonBoxOptions->button(QDialogButtonBox::Cancel)
        || pButton == m_pButtonBoxForwarding->button(QDialogButtonBox::Cancel))
        emit sigDataChangeRejected();
    else
    if (   pButton == m_pButtonBoxOptions->button(QDialogButtonBox::Ok)
        || pButton == m_pButtonBoxForwarding->button(QDialogButtonBox::Ok))
        emit sigDataChangeAccepted();
}

void UIDetailsWidgetNATNetwork::prepare()
{
    /* Prepare this: */
    prepareThis();

    /* Apply language settings: */
    retranslateUi();

    /* Update button states finally: */
    updateButtonStates();
}

void UIDetailsWidgetNATNetwork::prepareThis()
{
    /* Create layout: */
    new QVBoxLayout(this);
    if (layout())
    {
        /* Configure layout: */
        layout()->setContentsMargins(0, 0, 0, 0);

        /* Prepare tab-widget: */
        prepareTabWidget();
    }
}

void UIDetailsWidgetNATNetwork::prepareTabWidget()
{
    /* Create tab-widget: */
    m_pTabWidget = new QITabWidget(this);
    if (m_pTabWidget)
    {
        /* Prepare 'Options' tab: */
        prepareTabOptions();
        /* Prepare 'Forwarding' tab: */
        prepareTabForwarding();

        /* Add into layout: */
        layout()->addWidget(m_pTabWidget);
    }
}

void UIDetailsWidgetNATNetwork::prepareTabOptions()
{
    /* Prepare 'Options' tab: */
    QWidget *pTabOptions = new QWidget(m_pTabWidget);
    if (pTabOptions)
    {
        /* Prepare 'Options' layout: */
        QGridLayout *pLayoutOptions = new QGridLayout(pTabOptions);
        if (pLayoutOptions)
        {
#ifdef VBOX_WS_MAC
            pLayoutOptions->setSpacing(10);
            pLayoutOptions->setContentsMargins(10, 10, 10, 10);
#endif

            /* Prepare network availability check-box: */
            m_pCheckboxNetworkAvailable = new QCheckBox(pTabOptions);
            if (m_pCheckboxNetworkAvailable)
            {
                connect(m_pCheckboxNetworkAvailable, &QCheckBox::toggled,
                        this, &UIDetailsWidgetNATNetwork::sltNetworkAvailabilityChanged);
                pLayoutOptions->addWidget(m_pCheckboxNetworkAvailable, 0, 0, 1, 2);
            }

            /* Prepare 20-px shifting spacer: */
            QSpacerItem *pSpacerItem = new QSpacerItem(20, 0, QSizePolicy::Fixed, QSizePolicy::Minimum);
            if (pSpacerItem)
                pLayoutOptions->addItem(pSpacerItem, 1, 0);

            /* Prepare settings widget layout: */
            QGridLayout *pLayoutSettings = new QGridLayout;
            if (pLayoutSettings)
            {
                pLayoutSettings->setContentsMargins(0, 0, 0, 0);
                pLayoutSettings->setColumnStretch(3, 1);

                /* Prepare network name label: */
                m_pLabelNetworkName = new QLabel(pTabOptions);
                if (m_pLabelNetworkName)
                {
                    m_pLabelNetworkName->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    pLayoutSettings->addWidget(m_pLabelNetworkName, 0, 0);
                }
                /* Prepare network name editor: */
                m_pEditorNetworkName = new QLineEdit(pTabOptions);
                if (m_pEditorNetworkName)
                {
                    if (m_pLabelNetworkName)
                        m_pLabelNetworkName->setBuddy(m_pEditorNetworkName);
                    connect(m_pEditorNetworkName, &QLineEdit::textEdited,
                            this, &UIDetailsWidgetNATNetwork::sltNetworkNameChanged);

                    pLayoutSettings->addWidget(m_pEditorNetworkName, 0, 1, 1, 3);
                }

                /* Prepare network IPv4 prefix label: */
                m_pLabelNetworkIPv4Prefix = new QLabel(pTabOptions);
                if (m_pLabelNetworkIPv4Prefix)
                {
                    m_pLabelNetworkIPv4Prefix->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    pLayoutSettings->addWidget(m_pLabelNetworkIPv4Prefix, 1, 0);
                }
                /* Prepare network IPv4 prefix editor: */
                m_pEditorNetworkIPv4Prefix = new QLineEdit(pTabOptions);
                if (m_pEditorNetworkIPv4Prefix)
                {
                    if (m_pLabelNetworkIPv4Prefix)
                        m_pLabelNetworkIPv4Prefix->setBuddy(m_pEditorNetworkIPv4Prefix);
                    connect(m_pEditorNetworkIPv4Prefix, &QLineEdit::textEdited,
                            this, &UIDetailsWidgetNATNetwork::sltNetworkIPv4PrefixChanged);

                    pLayoutSettings->addWidget(m_pEditorNetworkIPv4Prefix, 1, 1, 1, 3);
                }

                /* Prepare network IPv6 prefix label: */
                m_pLabelNetworkIPv6Prefix = new QLabel(pTabOptions);
                if (m_pLabelNetworkIPv6Prefix)
                {
                    m_pLabelNetworkIPv6Prefix->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    pLayoutSettings->addWidget(m_pLabelNetworkIPv6Prefix, 2, 0);
                }
                /* Prepare network IPv6 prefix editor: */
                m_pEditorNetworkIPv6Prefix = new QLineEdit(pTabOptions);
                if (m_pEditorNetworkIPv6Prefix)
                {
                    if (m_pLabelNetworkIPv6Prefix)
                        m_pLabelNetworkIPv6Prefix->setBuddy(m_pEditorNetworkIPv6Prefix);
                    connect(m_pEditorNetworkIPv6Prefix, &QLineEdit::textEdited,
                            this, &UIDetailsWidgetNATNetwork::sltNetworkIPv6PrefixChanged);

                    pLayoutSettings->addWidget(m_pEditorNetworkIPv6Prefix, 2, 1, 1, 3);
                }

                /* Prepare extended label: */
                m_pLabelExtended = new QLabel(pTabOptions);
                if (m_pLabelExtended)
                {
                    m_pLabelExtended->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    pLayoutSettings->addWidget(m_pLabelExtended, 3, 0);
                }
                /* Prepare 'supports DHCP' check-box: */
                m_pCheckboxSupportsDHCP = new QCheckBox(pTabOptions);
                if (m_pCheckboxSupportsDHCP)
                {
                    connect(m_pCheckboxSupportsDHCP, &QCheckBox::toggled,
                            this, &UIDetailsWidgetNATNetwork::sltSupportsDHCPChanged);
                    pLayoutSettings->addWidget(m_pCheckboxSupportsDHCP, 3, 1, 1, 2);
                }
                /* Prepare 'supports IPv6' check-box: */
                m_pCheckboxSupportsIPv6 = new QCheckBox(pTabOptions);
                if (m_pCheckboxSupportsIPv6)
                {
                    connect(m_pCheckboxSupportsIPv6, &QCheckBox::toggled,
                            this, &UIDetailsWidgetNATNetwork::sltSupportsIPv6Changed);
                    pLayoutSettings->addWidget(m_pCheckboxSupportsIPv6, 4, 1, 1, 2);
                }
                /* Prepare 'advertise default IPv6 route' check-box: */
                m_pCheckboxAdvertiseDefaultIPv6Route = new QCheckBox(pTabOptions);
                if (m_pCheckboxAdvertiseDefaultIPv6Route)
                {
                    connect(m_pCheckboxAdvertiseDefaultIPv6Route, &QCheckBox::toggled,
                            this, &UIDetailsWidgetNATNetwork::sltAdvertiseDefaultIPv6RouteChanged);
                    pLayoutSettings->addWidget(m_pCheckboxAdvertiseDefaultIPv6Route, 5, 1, 1, 2);
                }

                pLayoutOptions->addLayout(pLayoutSettings, 1, 1);
            }

            /* If parent embedded into stack: */
            if (m_enmEmbedding == EmbedTo_Stack)
            {
                /* Prepare button-box: */
                m_pButtonBoxOptions = new QIDialogButtonBox(pTabOptions);
                if (m_pButtonBoxOptions)
                {
                    m_pButtonBoxOptions->setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
                    connect(m_pButtonBoxOptions, &QIDialogButtonBox::clicked, this, &UIDetailsWidgetNATNetwork::sltHandleButtonBoxClick);

                    pLayoutOptions->addWidget(m_pButtonBoxOptions, 2, 0, 1, 2);
                }
            }
        }

        m_pTabWidget->addTab(pTabOptions, QString());
    }
}

void UIDetailsWidgetNATNetwork::prepareTabForwarding()
{
    /* Prepare 'Forwarding' tab: */
    QWidget *pTabForwarding = new QWidget(m_pTabWidget);
    if (pTabForwarding)
    {
        /* Prepare 'Forwarding' layout: */
        QGridLayout *pLayoutForwarding = new QGridLayout(pTabForwarding);
        if (pLayoutForwarding)
        {
#ifdef VBOX_WS_MAC
            /* Configure layout: */
            pLayoutForwarding->setSpacing(10);
            pLayoutForwarding->setContentsMargins(10, 10, 10, 10);
#endif

            /* Prepare forwarding tab-widget: */
            m_pTabWidgetForwarding = new QITabWidget(pTabForwarding);
            if (m_pTabWidgetForwarding)
            {
                /* Prepare IPv4 forwarding table: */
                m_pForwardingTableIPv4 = new UIPortForwardingTable(UIPortForwardingDataList(),
                                                                   false /* ip IPv6 protocol */,
                                                                   false /* allow empty guest IPs */);
                if (m_pForwardingTableIPv4)
                {
                    connect(m_pForwardingTableIPv4, &UIPortForwardingTable::sigDataChanged,
                            this, &UIDetailsWidgetNATNetwork::sltForwardingRulesIPv4Changed);
                    m_pTabWidgetForwarding->addTab(m_pForwardingTableIPv4, QString());
                }
                /* Prepare IPv6 forwarding table: */
                m_pForwardingTableIPv6 = new UIPortForwardingTable(UIPortForwardingDataList(),
                                                                   true /* ip IPv6 protocol */,
                                                                   false /* allow empty guest IPs */);
                if (m_pForwardingTableIPv6)
                {
                    connect(m_pForwardingTableIPv4, &UIPortForwardingTable::sigDataChanged,
                            this, &UIDetailsWidgetNATNetwork::sltForwardingRulesIPv6Changed);
                    m_pTabWidgetForwarding->addTab(m_pForwardingTableIPv6, QString());
                }

                pLayoutForwarding->addWidget(m_pTabWidgetForwarding, 0, 0);
            }

            /* If parent embedded into stack: */
            if (m_enmEmbedding == EmbedTo_Stack)
            {
                /* Prepare button-box: */
                m_pButtonBoxForwarding = new QIDialogButtonBox(pTabForwarding);
                if (m_pButtonBoxForwarding)
                {
                    m_pButtonBoxForwarding->setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
                    connect(m_pButtonBoxForwarding, &QIDialogButtonBox::clicked, this, &UIDetailsWidgetNATNetwork::sltHandleButtonBoxClick);

                    pLayoutForwarding->addWidget(m_pButtonBoxForwarding, 1, 0);
                }
            }
        }

        m_pTabWidget->addTab(pTabForwarding, QString());
    }
}

void UIDetailsWidgetNATNetwork::loadDataForOptions()
{
    /* Check whether network exists and enabled: */
    const bool fIsNetworkExists = m_newData.m_fExists;
    const bool fIsNetworkEnabled = m_newData.m_fEnabled;

    /* Update 'Options' field availability: */
    m_pCheckboxNetworkAvailable->setEnabled(fIsNetworkExists);
    m_pLabelNetworkName->setEnabled(fIsNetworkExists && fIsNetworkEnabled);
    m_pEditorNetworkName->setEnabled(fIsNetworkExists && fIsNetworkEnabled);
    m_pLabelNetworkIPv4Prefix->setEnabled(fIsNetworkExists && fIsNetworkEnabled);
    m_pEditorNetworkIPv4Prefix->setEnabled(fIsNetworkExists && fIsNetworkEnabled);
    m_pLabelNetworkIPv6Prefix->setEnabled(fIsNetworkExists && fIsNetworkEnabled);
    m_pEditorNetworkIPv6Prefix->setEnabled(fIsNetworkExists && fIsNetworkEnabled);
    m_pLabelExtended->setEnabled(fIsNetworkExists && fIsNetworkEnabled);
    m_pCheckboxSupportsDHCP->setEnabled(fIsNetworkExists && fIsNetworkEnabled);
    m_pCheckboxSupportsIPv6->setEnabled(fIsNetworkExists && fIsNetworkEnabled);
    m_pCheckboxAdvertiseDefaultIPv6Route->setEnabled(fIsNetworkExists && fIsNetworkEnabled);

    /* Load 'Options' fields: */
    m_pCheckboxNetworkAvailable->setChecked(fIsNetworkEnabled);
    m_pEditorNetworkName->setText(m_newData.m_strName);
    m_pEditorNetworkIPv4Prefix->setText(m_newData.m_strPrefixIPv4);
    m_pEditorNetworkIPv6Prefix->setText(m_newData.m_strPrefixIPv6);
    m_pCheckboxSupportsDHCP->setChecked(m_newData.m_fSupportsDHCP);
    m_pCheckboxSupportsIPv6->setChecked(m_newData.m_fSupportsIPv6);
    m_pCheckboxAdvertiseDefaultIPv6Route->setChecked(m_newData.m_fAdvertiseDefaultIPv6Route);
}

void UIDetailsWidgetNATNetwork::loadDataForForwarding()
{
    /* Check whether network exists and enabled: */
    const bool fIsNetworkExists = m_newData.m_fExists;
    const bool fIsNetworkEnabled = m_newData.m_fEnabled;

    /* Update 'Forwarding' field availability: */
    m_pForwardingTableIPv4->setEnabled(fIsNetworkExists && fIsNetworkEnabled);
    m_pForwardingTableIPv6->setEnabled(fIsNetworkExists && fIsNetworkEnabled);

    /* Calculate/load guest address hints: */
    char szTmpIp[16];
    RTNETADDRIPV4 rtNetwork4;
    int iPrefix4;
    const int rc = RTNetStrToIPv4Cidr(m_newData.m_strPrefixIPv4.toUtf8().constData(), &rtNetwork4, &iPrefix4);
    RTStrPrintf(szTmpIp, sizeof(szTmpIp), "%RTnaipv4", rtNetwork4);
    if (RT_SUCCESS(rc))
        m_pForwardingTableIPv4->setGuestAddressHint(QString(szTmpIp));

    /* Load 'Forwarding' fields: */
    m_pForwardingTableIPv4->setRules(m_newData.m_rules4, m_fHoldPosition);
    m_pForwardingTableIPv6->setRules(m_newData.m_rules6, m_fHoldPosition);
    m_fHoldPosition = false;
}
