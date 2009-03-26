/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxGLSettingsNetwork class implementation
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

#include "QIWidgetValidator.h"
#include "VBoxGlobal.h"
#include "VBoxGLSettingsNetwork.h"
#include "VBoxGLSettingsNetworkDetails.h"
#include "VBoxProblemReporter.h"

/* Qt includes */
#include <QHeaderView>
#include <QHostAddress>

NetworkItem::NetworkItem()
    : QTreeWidgetItem()
    , mChanged (false)
    , mDhcpClientEnabled (false)
    , mInterfaceAddress (QString::null)
    , mInterfaceMask (QString::null)
    , mIpv6Supported (false)
    , mInterfaceAddress6 (QString::null)
    , mInterfaceMaskLength6 (QString::null)
    , mDhcpServerEnabled (false)
    , mDhcpServerAddress (QString::null)
    , mDhcpServerMask (QString::null)
    , mDhcpLowerAddress (QString::null)
    , mDhcpUpperAddress (QString::null)
{
}

void NetworkItem::getFromInterface (const CHostNetworkInterface &aInterface)
{
    /* Initialization */
    mInterface = aInterface;
    CDHCPServer dhcp = vboxGlobal().virtualBox().FindDHCPServerByNetworkName (mInterface.GetNetworkName());
    if (dhcp.isNull()) vboxGlobal().virtualBox().CreateDHCPServer (mInterface.GetNetworkName());
    dhcp = vboxGlobal().virtualBox().FindDHCPServerByNetworkName (mInterface.GetNetworkName());
    AssertMsg (!dhcp.isNull(), ("DHCP Server creation failed!\n"));
    setText (0, mInterface.GetName());

    /* Host-only Interface settings */
    mDhcpClientEnabled = mInterface.GetDhcpEnabled();
    mInterfaceAddress = mInterface.GetIPAddress();
    mInterfaceMask = mInterface.GetNetworkMask();
    mIpv6Supported = mInterface.GetIPV6Supported();
    mInterfaceAddress6 = mInterface.GetIPV6Address();
    mInterfaceMaskLength6 = QString ("%1").arg (mInterface.GetIPV6NetworkMaskPrefixLength());

    /* DHCP Server settings */
    mDhcpServerEnabled = dhcp.GetEnabled();
    mDhcpServerAddress = dhcp.GetIPAddress();
    mDhcpServerMask = dhcp.GetNetworkMask();
    mDhcpLowerAddress = dhcp.GetLowerIP();
    mDhcpUpperAddress = dhcp.GetUpperIP();

    /* Update tool-tip */
    updateInfo();
}

void NetworkItem::putBackToInterface()
{
    /* Host-only Interface settings */
    if (mDhcpClientEnabled)
    {
        mInterface.EnableDynamicIpConfig();
    }
    else
    {
        AssertMsg (mInterfaceAddress.isEmpty() ||
                   QHostAddress (mInterfaceAddress).protocol() == QAbstractSocket::IPv4Protocol,
                   ("Interface IPv4 address must be empty or IPv4-valid!\n"));
        AssertMsg (mInterfaceMask.isEmpty() ||
                   QHostAddress (mInterfaceMask).protocol() == QAbstractSocket::IPv4Protocol,
                   ("Interface IPv4 network mask must be empty or IPv4-valid!\n"));
        mInterface.EnableStaticIpConfig (mInterfaceAddress, mInterfaceMask);
        if (mInterface.GetIPV6Supported())
        {
            AssertMsg (mInterfaceAddress6.isEmpty() ||
                       QHostAddress (mInterfaceAddress6).protocol() == QAbstractSocket::IPv6Protocol,
                       ("Interface IPv6 address must be empty or IPv6-valid!\n"));
            mInterface.EnableStaticIpConfigV6 (mInterfaceAddress6, mInterfaceMaskLength6.toULong());
        }
    }

    /* DHCP Server settings */
    CDHCPServer dhcp = vboxGlobal().virtualBox().FindDHCPServerByNetworkName (mInterface.GetNetworkName());
    AssertMsg (!dhcp.isNull(), ("DHCP Server should be already created!\n"));
    dhcp.SetEnabled (mDhcpServerEnabled);
    AssertMsg (QHostAddress (mDhcpServerAddress).protocol() == QAbstractSocket::IPv4Protocol,
               ("DHCP Server IPv4 address must be IPv4-valid!\n"));
    AssertMsg (QHostAddress (mDhcpServerMask).protocol() == QAbstractSocket::IPv4Protocol,
               ("DHCP Server IPv4 network mask must be IPv4-valid!\n"));
    AssertMsg (QHostAddress (mDhcpLowerAddress).protocol() == QAbstractSocket::IPv4Protocol,
               ("DHCP Server IPv4 lower bound must be IPv4-valid!\n"));
    AssertMsg (QHostAddress (mDhcpUpperAddress).protocol() == QAbstractSocket::IPv4Protocol,
               ("DHCP Server IPv4 upper bound must be IPv4-valid!\n"));
    dhcp.SetConfiguration (mDhcpServerAddress, mDhcpServerMask, mDhcpLowerAddress, mDhcpUpperAddress);
}

bool NetworkItem::revalidate (QString &aWarning, QString & /* aTitle */)
{
    /* Host-only Interface validation */
    if (!mDhcpClientEnabled)
    {
        if (!mInterfaceAddress.isEmpty() &&
            (QHostAddress (mInterfaceAddress) == QHostAddress::Any ||
             QHostAddress (mInterfaceAddress).protocol() != QAbstractSocket::IPv4Protocol))
        {
            aWarning = QTreeWidget::tr ("host IPv4 address of <b>%1</b> is wrong").arg (text (0));
            return false;
        }
        if (!mInterfaceMask.isEmpty() &&
            (QHostAddress (mInterfaceMask) == QHostAddress::Any ||
             QHostAddress (mInterfaceMask).protocol() != QAbstractSocket::IPv4Protocol))
        {
            aWarning = QTreeWidget::tr ("host IPv4 network mask of <b>%1</b> is wrong").arg (text (0));
            return false;
        }
        if (mIpv6Supported)
        {
            if (!mInterfaceAddress6.isEmpty() &&
                (QHostAddress (mInterfaceAddress6) == QHostAddress::AnyIPv6 ||
                 QHostAddress (mInterfaceAddress6).protocol() != QAbstractSocket::IPv6Protocol))
            {
                aWarning = QTreeWidget::tr ("host IPv6 address of <b>%1</b> is wrong").arg (text (0));
                return false;
            }
        }
    }

    /* DHCP Server settings */
    if (mDhcpServerEnabled)
    {
        if (QHostAddress (mDhcpServerAddress) == QHostAddress::Any ||
            QHostAddress (mDhcpServerAddress).protocol() != QAbstractSocket::IPv4Protocol)
        {
            aWarning = QTreeWidget::tr ("DHCP server address of <b>%1</b> is wrong").arg (text (0));
            return false;
        }
        if (QHostAddress (mDhcpServerMask) == QHostAddress::Any ||
            QHostAddress (mDhcpServerMask).protocol() != QAbstractSocket::IPv4Protocol)
        {
            aWarning = QTreeWidget::tr ("DHCP server mask of <b>%1</b> is wrong").arg (text (0));
            return false;
        }
        if (QHostAddress (mDhcpLowerAddress) == QHostAddress::Any ||
            QHostAddress (mDhcpLowerAddress).protocol() != QAbstractSocket::IPv4Protocol)
        {
            aWarning = QTreeWidget::tr ("DHCP lower address bound of <b>%1</b> is wrong").arg (text (0));
            return false;
        }
        if (QHostAddress (mDhcpUpperAddress) == QHostAddress::Any ||
            QHostAddress (mDhcpUpperAddress).protocol() != QAbstractSocket::IPv4Protocol)
        {
            aWarning = QTreeWidget::tr ("DHCP upper address bound of <b>%1</b> is wrong").arg (text (0));
            return false;
        }
    }
    return true;
}

QString NetworkItem::updateInfo()
{
    /* Update information label */
    QString hdr ("<tr><td><nobr>%1:&nbsp;</nobr></td>"
                 "<td><nobr>%2</nobr></td></tr>");
    QString sub ("<tr><td><nobr>&nbsp;&nbsp;%1:&nbsp;</nobr></td>"
                 "<td><nobr>%2</nobr></td></tr>");
    QString data, tip, buffer;

    /* Host-only Interface information */
    buffer = hdr.arg (QTreeWidget::tr ("Host Interface"))
                .arg (mDhcpClientEnabled ? QTreeWidget::tr ("Automatically configured", "interface")
                                         : QTreeWidget::tr ("Manually configured", "interface"));
    data += buffer;
    tip += buffer;

    if (!mDhcpClientEnabled)
    {
        buffer = sub.arg (QTreeWidget::tr ("IPv4 Address"))
                    .arg (mInterfaceAddress.isEmpty() ? QTreeWidget::tr ("Not set", "address")
                                                      : mInterfaceAddress) +
                 sub.arg (QTreeWidget::tr ("IPv4 Mask"))
                    .arg (mInterfaceMask.isEmpty() ? QTreeWidget::tr ("Not set", "mask")
                                                   : mInterfaceMask);
        tip += buffer;

        if (mIpv6Supported)
        {
            buffer = sub.arg (QTreeWidget::tr ("IPv6 Address"))
                        .arg (mInterfaceAddress6.isEmpty() ? QTreeWidget::tr ("Not set", "address")
                                                           : mInterfaceAddress6) +
                     sub.arg (QTreeWidget::tr ("IPv6 Mask Length"))
                        .arg (mInterfaceMaskLength6.isEmpty() ? QTreeWidget::tr ("Not set", "length")
                                                              : mInterfaceMaskLength6);
            tip += buffer;
        }
    }

    /* DHCP Server information */
    buffer = hdr.arg (QTreeWidget::tr ("DHCP Server"))
                .arg (mDhcpServerEnabled ? QTreeWidget::tr ("Enabled", "server")
                                         : QTreeWidget::tr ("Disabled", "server"));
    data += buffer;
    tip += buffer;

    if (mDhcpServerEnabled)
    {
        buffer = sub.arg (QTreeWidget::tr ("Address"))
                    .arg (mDhcpServerAddress.isEmpty() ? QTreeWidget::tr ("Not set", "address")
                                                       : mDhcpServerAddress) +
                 sub.arg (QTreeWidget::tr ("Mask"))
                    .arg (mDhcpServerMask.isEmpty() ? QTreeWidget::tr ("Not set", "mask")
                                                    : mDhcpServerMask) +
                 sub.arg (QTreeWidget::tr ("Lower Bound"))
                    .arg (mDhcpLowerAddress.isEmpty() ? QTreeWidget::tr ("Not set", "bound")
                                                      : mDhcpLowerAddress) +
                 sub.arg (QTreeWidget::tr ("Upper Bound"))
                    .arg (mDhcpUpperAddress.isEmpty() ? QTreeWidget::tr ("Not set", "bound")
                                                      : mDhcpUpperAddress);
        tip += buffer;
    }

    setToolTip (0, tip);

    return QString ("<table>") + data + QString ("</table>");
}

VBoxGLSettingsNetwork::VBoxGLSettingsNetwork()
{
    /* Apply UI decorations */
    Ui::VBoxGLSettingsNetwork::setupUi (this);

#ifdef Q_WS_MAC
    /* Make shifting spacer for MAC as we have fixed-size networks list */
    QSpacerItem *shiftSpacer =
        new QSpacerItem (0, 1, QSizePolicy::Expanding, QSizePolicy::Preferred);
    QGridLayout *mainLayout = static_cast <QGridLayout*> (layout());
    mainLayout->addItem (shiftSpacer, 1, 4, 2);
#endif

    /* Setup tree-widget */
    mTwInterfaces->header()->hide();
    mTwInterfaces->setContextMenuPolicy (Qt::CustomContextMenu);

    /* Prepare toolbar */
    mAddInterface = new QAction (mTwInterfaces);
    mRemInterface = new QAction (mTwInterfaces);
    mEditInterface = new QAction (mTwInterfaces);

    mAddInterface->setShortcuts (QList <QKeySequence> ()
                                 << QKeySequence ("Ins")
                                 << QKeySequence ("Ctrl+N"));
    mRemInterface->setShortcuts (QList <QKeySequence> ()
                                 << QKeySequence ("Del")
                                 << QKeySequence ("Ctrl+R"));
    mEditInterface->setShortcuts (QList <QKeySequence> ()
                                  << QKeySequence ("Space")
                                  << QKeySequence ("F2"));

    mAddInterface->setIcon (VBoxGlobal::iconSet (":/add_host_iface_16px.png",
                                                 ":/add_host_iface_disabled_16px.png"));
    mRemInterface->setIcon (VBoxGlobal::iconSet (":/remove_host_iface_16px.png",
                                                 ":/remove_host_iface_disabled_16px.png"));
    mEditInterface->setIcon (VBoxGlobal::iconSet (":/guesttools_16px.png",
                                                  ":/guesttools_disabled_16px.png"));

    mTbActions->setUsesTextLabel (false);
    mTbActions->setIconSize (QSize (16, 16));
    mTbActions->setOrientation (Qt::Vertical);
    mTbActions->addAction (mAddInterface);
    mTbActions->addAction (mRemInterface);
    mTbActions->addAction (mEditInterface);
    mTbActions->setSizePolicy (QSizePolicy::Fixed, QSizePolicy::MinimumExpanding);
    mTbActions->updateGeometry();
    mTbActions->setMinimumHeight (mTbActions->sizeHint().height());

    /* Setup connections */
    connect (mAddInterface, SIGNAL (triggered (bool)), this, SLOT (addInterface()));
    connect (mRemInterface, SIGNAL (triggered (bool)), this, SLOT (remInterface()));
    connect (mEditInterface, SIGNAL (triggered (bool)), this, SLOT (editInterface()));
    connect (mTwInterfaces, SIGNAL (currentItemChanged (QTreeWidgetItem *, QTreeWidgetItem *)),
             this, SLOT (updateCurrentItem()));
    connect (mTwInterfaces, SIGNAL (customContextMenuRequested (const QPoint &)),
             this, SLOT (showContextMenu (const QPoint &)));
    connect (mTwInterfaces, SIGNAL (itemDoubleClicked (QTreeWidgetItem*, int)),
             this, SLOT (editInterface()));

    /* Applying language settings */
    retranslateUi();
}

void VBoxGLSettingsNetwork::getFrom (const CSystemProperties &, const VBoxGlobalSettings &)
{
    NetworkItem *item = 0;
    CHostNetworkInterfaceVector interfaces =
        vboxGlobal().virtualBox().GetHost().GetNetworkInterfaces();
    for (CHostNetworkInterfaceVector::ConstIterator it = interfaces.begin();
         it != interfaces.end(); ++ it)
    {
        if (it->GetInterfaceType() == KHostNetworkInterfaceType_HostOnly)
        {
            item = new NetworkItem();
            item->getFromInterface (*it);
            mTwInterfaces->addTopLevelItem (item);
            mTwInterfaces->sortItems (0, Qt::AscendingOrder);
        }
    }

    mTwInterfaces->setCurrentItem (item);
    updateCurrentItem();

#ifdef Q_WS_MAC
    int width = qMax (static_cast<QAbstractItemView*> (mTwInterfaces)
        ->sizeHintForColumn (0) + 2 * mTwInterfaces->frameWidth() +
        QApplication::style()->pixelMetric (QStyle::PM_ScrollBarExtent),
        160);
    mTwInterfaces->setFixedWidth (width);
    mTwInterfaces->resizeColumnToContents (0);
#endif /* Q_WS_MAC */
}

void VBoxGLSettingsNetwork::putBackTo (CSystemProperties &, VBoxGlobalSettings &)
{
    for (int i = 0; i < mTwInterfaces->topLevelItemCount(); ++ i)
    {
        NetworkItem *item =
            static_cast <NetworkItem*> (mTwInterfaces->topLevelItem (i));
        if (item->isChanged())
            item->putBackToInterface();
    }
}

void VBoxGLSettingsNetwork::setValidator (QIWidgetValidator *aValidator)
{
    mValidator = aValidator;
}

bool VBoxGLSettingsNetwork::revalidate (QString &aWarning, QString &aTitle)
{
    NetworkItem *item = static_cast <NetworkItem*> (mTwInterfaces->currentItem());
    return item ? item->revalidate (aWarning, aTitle) : true;
}

void VBoxGLSettingsNetwork::setOrderAfter (QWidget *aWidget)
{
    setTabOrder (aWidget, mTwInterfaces);
}

void VBoxGLSettingsNetwork::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxGLSettingsNetwork::retranslateUi (this);

    /* Translate action tool-tips */
    mAddInterface->setText (tr ("&Add host-only network"));
    mRemInterface->setText (tr ("&Remove host-only network"));
    mEditInterface->setText (tr ("&Edit host-only network"));

    mAddInterface->setToolTip (mAddInterface->text().remove ('&') +
        QString (" (%1)").arg (mAddInterface->shortcut().toString()));
    mRemInterface->setToolTip (mRemInterface->text().remove ('&') +
        QString (" (%1)").arg (mRemInterface->shortcut().toString()));
    mEditInterface->setToolTip (mEditInterface->text().remove ('&') +
        QString (" (%1)").arg (mEditInterface->shortcut().toString()));
}

void VBoxGLSettingsNetwork::addInterface()
{
#if defined (Q_WS_WIN32)
    /* Allow the started helper process to make itself the foreground window */
    AllowSetForegroundWindow (ASFW_ANY);

    /* Creating interface */
    CHostNetworkInterface iface;
    CHost host = vboxGlobal().virtualBox().GetHost();
    CProgress progress = host.CreateHostOnlyNetworkInterface (iface);
    if (host.isOk())
    {
        vboxProblem().showModalProgressDialog (progress,
            tr ("Performing", "creating/removing host-only network"), this);
        if (progress.GetResultCode() == 0)
        {
            NetworkItem *item = new NetworkItem();
            item->getFromInterface (iface);
            mTwInterfaces->addTopLevelItem (item);
            mTwInterfaces->sortItems (0, Qt::AscendingOrder);
            mTwInterfaces->setCurrentItem (item);
        }
        else
            vboxProblem().cannotCreateHostInterface (progress, this);
    }
    else
        vboxProblem().cannotCreateHostInterface (host, this);

    /* Allow the started helper process to make itself the foreground window */
    AllowSetForegroundWindow (ASFW_ANY);
#endif
}

void VBoxGLSettingsNetwork::remInterface()
{
#if defined (Q_WS_WIN32)
    /* Allow the started helper process to make itself the foreground window */
    AllowSetForegroundWindow (ASFW_ANY);

    /* Check interface presence & name */
    NetworkItem *item = static_cast <NetworkItem*> (mTwInterfaces->currentItem());
    AssertMsg (item, ("Current item should be selected!\n"));
    QString name (item->text (0));

    /* Asking user about deleting selected network interface */
    if (vboxProblem().confirmDeletingHostInterface (name, this) ==
        QIMessageBox::Cancel) return;

    /* Removing interface */
    CHost host = vboxGlobal().virtualBox().GetHost();
    CHostNetworkInterface iface = host.FindHostNetworkInterfaceByName (name);
    if (!iface.isNull())
    {
        /* Delete interface */
        CProgress progress = host.RemoveHostOnlyNetworkInterface (iface.GetId(), iface);
        if (host.isOk())
        {
            vboxProblem().showModalProgressDialog (progress,
                tr ("Performing", "creating/removing host-only network"), this);
            if (progress.GetResultCode() == 0)
                delete item;
            else
                vboxProblem().cannotRemoveHostInterface (progress, iface, this);
        }
    }
    if (!host.isOk())
        vboxProblem().cannotRemoveHostInterface (host, iface, this);

    /* Allow the started helper process to make itself the foreground window */
    AllowSetForegroundWindow (ASFW_ANY);
#endif
}

void VBoxGLSettingsNetwork::editInterface()
{
    /* Check interface presence */
    NetworkItem *item = static_cast <NetworkItem*> (mTwInterfaces->currentItem());
    AssertMsg (item, ("Current item should be selected!\n"));

    /* Edit current item data */
    VBoxGLSettingsNetworkDetails details (this);
    details.getFromItem (item);
    if (details.exec() == QDialog::Accepted)
    {
        details.putBackToItem();
        item->setChanged (true);
        item->updateInfo();
    }

    updateCurrentItem();
    mValidator->revalidate();
}

void VBoxGLSettingsNetwork::updateCurrentItem()
{
    /* Get current item */
    NetworkItem *item = static_cast <NetworkItem*> (mTwInterfaces->currentItem());
    /* Set the final label text */
    mLbInfo->setText (item ? item->updateInfo() : QString());
    /* Update availability */
    mRemInterface->setEnabled (item);
    mEditInterface->setEnabled (item);
#if !defined (Q_WS_WIN32)
    /* Disable add/remove for all except win for now */
    mAddInterface->setEnabled (false);
    mRemInterface->setEnabled (false);
#endif
}

void VBoxGLSettingsNetwork::showContextMenu (const QPoint &aPos)
{
    QMenu menu;

    if (mTwInterfaces->itemAt (aPos))
    {
        menu.addAction (mEditInterface);
        menu.addAction (mRemInterface);
    }
    else
    {
        menu.addAction (mAddInterface);
    }

    menu.exec (mTwInterfaces->mapToGlobal (aPos));
}

