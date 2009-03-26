/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxGLSettingsNetworkDetails class implementation
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

/* VBox Includes */
#include "VBoxGLSettingsNetwork.h"
#include "VBoxGLSettingsNetworkDetails.h"

/* Qt Includes */
#include <QHostAddress>
#include <QRegExpValidator>

VBoxGLSettingsNetworkDetails::VBoxGLSettingsNetworkDetails (QWidget *aParent)
    : QIWithRetranslateUI2 <QIDialog> (aParent
#ifdef Q_WS_MAC
    ,Qt::Sheet
#endif /* Q_WS_MAC */
    )
{
    /* Apply UI decorations */
    Ui::VBoxGLSettingsNetworkDetails::setupUi (this);

    /* Setup dialog */
    setWindowIcon (QIcon (":/guesttools_16px.png"));

    /* Setup validators */
    QString templateIPv4 ("([0-9]|[1-9][0-9]|1[0-9][0-9]|2[0-4][0-9]|25[0-5])\\."
                          "([0-9]|[1-9][0-9]|1[0-9][0-9]|2[0-4][0-9]|25[0-5])\\."
                          "([0-9]|[1-9][0-9]|1[0-9][0-9]|2[0-4][0-9]|25[0-5])\\."
                          "([0-9]|[1-9][0-9]|1[0-9][0-9]|2[0-4][0-9]|25[0-5])");
    QString templateIPv6 ("[0-9a-fA-Z]{1,4}:{1,2}[0-9a-fA-Z]{1,4}:{1,2}"
                          "[0-9a-fA-Z]{1,4}:{1,2}[0-9a-fA-Z]{1,4}:{1,2}"
                          "[0-9a-fA-Z]{1,4}:{1,2}[0-9a-fA-Z]{1,4}:{1,2}"
                          "[0-9a-fA-Z]{1,4}:{1,2}[0-9a-fA-Z]{1,4}");

    mLeIPv4->setValidator (new QRegExpValidator (QRegExp (templateIPv4), this));
    mLeNMv4->setValidator (new QRegExpValidator (QRegExp (templateIPv4), this));
    mLeIPv6->setValidator (new QRegExpValidator (QRegExp (templateIPv6), this));
    mLeNMv6->setValidator (new QRegExpValidator (QRegExp ("[1-9][0-9]|1[0-1][0-9]|12[0-8]"), this));
    mLeDhcpAddress->setValidator (new QRegExpValidator (QRegExp (templateIPv4), this));
    mLeDhcpMask->setValidator (new QRegExpValidator (QRegExp (templateIPv4), this));
    mLeDhcpLowerAddress->setValidator (new QRegExpValidator (QRegExp (templateIPv4), this));
    mLeDhcpUpperAddress->setValidator (new QRegExpValidator (QRegExp (templateIPv4), this));

    /* Setup widgets */
    mLeIPv6->setFixedWidthByText (QString().fill ('X', 32) + QString().fill (':', 7));

#if defined (Q_WS_WIN32)
    QStyleOption options1;
    options1.initFrom (mCbManual);
    QGridLayout *layout1 = qobject_cast <QGridLayout*> (mTwDetails->widget (0)->layout());
    int wid1 = mCbManual->style()->pixelMetric (QStyle::PM_IndicatorWidth, &options1, mCbManual) +
               mCbManual->style()->pixelMetric (QStyle::PM_CheckBoxLabelSpacing, &options1, mCbManual) -
               layout1->spacing() - 1;
    QSpacerItem *spacer1 = new QSpacerItem (wid1, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
    layout1->addItem (spacer1, 1, 0, 4);
#else
    mCbManual->setVisible (false);
#endif

    QStyleOption options2;
    options2.initFrom (mCbDhcpServerEnabled);
    QGridLayout *layout2 = qobject_cast <QGridLayout*> (mTwDetails->widget (1)->layout());
    int wid2 = mCbDhcpServerEnabled->style()->pixelMetric (QStyle::PM_IndicatorWidth, &options2, mCbDhcpServerEnabled) +
               mCbDhcpServerEnabled->style()->pixelMetric (QStyle::PM_CheckBoxLabelSpacing, &options2, mCbDhcpServerEnabled) -
               layout2->spacing() - 1;
    QSpacerItem *spacer2 = new QSpacerItem (wid2, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
    layout2->addItem (spacer2, 1, 0, 4);

    /* Setup connections */
    connect (mCbManual, SIGNAL (stateChanged (int)),
             this, SLOT (dhcpClientStatusChanged()));
    connect (mCbDhcpServerEnabled, SIGNAL (stateChanged (int)),
             this, SLOT (dhcpServerStatusChanged()));

    /* Applying language settings */
    retranslateUi();

    /* Fix minimum possible size */
    resize (minimumSizeHint());
    qApp->processEvents();
    setFixedSize (minimumSizeHint());
}

void VBoxGLSettingsNetworkDetails::getFromItem (NetworkItem *aItem)
{
    mItem = aItem;

    /* Host-only Interface */
    mCbManual->setChecked (!mItem->isDhcpClientEnabled());
#if !defined (Q_WS_WIN32)
    /* Disable automatic for all except win for now */
    mCbManual->setChecked (true);
    mCbManual->setEnabled (false);
#endif
    dhcpClientStatusChanged();

    /* DHCP Server */
    mCbDhcpServerEnabled->setChecked (mItem->isDhcpServerEnabled());
    dhcpServerStatusChanged();
}

void VBoxGLSettingsNetworkDetails::putBackToItem()
{
    /* Host-only Interface */
    mItem->setDhcpClientEnabled (!mCbManual->isChecked());
    if (mCbManual->isChecked())
    {
        mItem->setInterfaceAddress (mLeIPv4->text());
        mItem->setInterfaceMask (mLeNMv4->text());
        if (mItem->isIpv6Supported())
        {
            mItem->setInterfaceAddress6 (mLeIPv6->text());
            mItem->setInterfaceMaskLength6 (mLeNMv6->text());
        }
    }

    /* DHCP Server */
    mItem->setDhcpServerEnabled (mCbDhcpServerEnabled->isChecked());
    if (mCbDhcpServerEnabled->isChecked())
    {
        mItem->setDhcpServerAddress (mLeDhcpAddress->text());
        mItem->setDhcpServerMask (mLeDhcpMask->text());
        mItem->setDhcpLowerAddress (mLeDhcpLowerAddress->text());
        mItem->setDhcpUpperAddress (mLeDhcpUpperAddress->text());
    }
}

void VBoxGLSettingsNetworkDetails::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxGLSettingsNetworkDetails::retranslateUi (this);
}

void VBoxGLSettingsNetworkDetails::dhcpClientStatusChanged()
{
    bool isManual = mCbManual->isChecked();
    bool isIpv6Supported = isManual && mItem->isIpv6Supported();

    mLeIPv4->clear();
    mLeNMv4->clear();
    mLeIPv6->clear();
    mLeNMv6->clear();

    mLbIPv4->setEnabled (isManual);
    mLbNMv4->setEnabled (isManual);
    mLeIPv4->setEnabled (isManual);
    mLeNMv4->setEnabled (isManual);
    mLbIPv6->setEnabled (isIpv6Supported);
    mLbNMv6->setEnabled (isIpv6Supported);
    mLeIPv6->setEnabled (isIpv6Supported);
    mLeNMv6->setEnabled (isIpv6Supported);

    if (isManual)
    {
        mLeIPv4->setText (mItem->interfaceAddress());
        mLeNMv4->setText (mItem->interfaceMask());
        if (isIpv6Supported)
        {
            mLeIPv6->setText (mItem->interfaceAddress6());
            mLeNMv6->setText (mItem->interfaceMaskLength6());
        }
    }
}

void VBoxGLSettingsNetworkDetails::dhcpServerStatusChanged()
{
    bool isEnabled = mCbDhcpServerEnabled->isChecked();

    mLeDhcpAddress->clear();
    mLeDhcpMask->clear();
    mLeDhcpLowerAddress->clear();
    mLeDhcpUpperAddress->clear();

    mLbDhcpAddress->setEnabled (isEnabled);
    mLbDhcpMask->setEnabled (isEnabled);
    mLbDhcpLowerAddress->setEnabled (isEnabled);
    mLbDhcpUpperAddress->setEnabled (isEnabled);
    mLeDhcpAddress->setEnabled (isEnabled);
    mLeDhcpMask->setEnabled (isEnabled);
    mLeDhcpLowerAddress->setEnabled (isEnabled);
    mLeDhcpUpperAddress->setEnabled (isEnabled);

    if (isEnabled)
    {
        mLeDhcpAddress->setText (mItem->dhcpServerAddress());
        mLeDhcpMask->setText (mItem->dhcpServerMask());
        mLeDhcpLowerAddress->setText (mItem->dhcpLowerAddress());
        mLeDhcpUpperAddress->setText (mItem->dhcpUpperAddress());
    }
}

