/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxGLSettingsNetwork class declaration
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

#ifndef __VBoxGLSettingsNetwork_h__
#define __VBoxGLSettingsNetwork_h__

#include "VBoxSettingsPage.h"
#include "VBoxGLSettingsNetwork.gen.h"

class NetworkItem : public QTreeWidgetItem
{
public:

    NetworkItem();

    void getFromInterface (const CHostNetworkInterface &aInterface);
    void putBackToInterface();

    bool revalidate (QString &aWarning, QString &aTitle);

    QString updateInfo();

    /* Common getters */
    bool isChanged() const { return mChanged; }

    /* Common setters */
    void setChanged (bool aChanged) { mChanged = aChanged; }

    /* Page getters */
    bool isDhcpClientEnabled() const { return mDhcpClientEnabled; }
    QString interfaceAddress() const { return mInterfaceAddress; }
    QString interfaceMask() const { return mInterfaceMask; }
    bool isIpv6Supported() const { return mIpv6Supported; }
    QString interfaceAddress6() const { return mInterfaceAddress6; }
    QString interfaceMaskLength6() const { return mInterfaceMaskLength6; }

    bool isDhcpServerEnabled() const { return mDhcpServerEnabled; }
    QString dhcpServerAddress() const { return mDhcpServerAddress; }
    QString dhcpServerMask() const { return mDhcpServerMask; }
    QString dhcpLowerAddress() const { return mDhcpLowerAddress; }
    QString dhcpUpperAddress() const { return mDhcpUpperAddress; }

    /* Page setters */
    void setDhcpClientEnabled (bool aEnabled) { mDhcpClientEnabled = aEnabled; }
    void setInterfaceAddress (const QString &aValue) { mInterfaceAddress = aValue; }
    void setInterfaceMask (const QString &aValue) { mInterfaceMask = aValue; }
    void setIp6Supported (bool aSupported) { mIpv6Supported = aSupported; }
    void setInterfaceAddress6 (const QString &aValue) { mInterfaceAddress6 = aValue; }
    void setInterfaceMaskLength6 (const QString &aValue) { mInterfaceMaskLength6 = aValue; }

    void setDhcpServerEnabled (bool aEnabled) { mDhcpServerEnabled = aEnabled; }
    void setDhcpServerAddress (const QString &aValue) { mDhcpServerAddress = aValue; }
    void setDhcpServerMask (const QString &aValue) { mDhcpServerMask = aValue; }
    void setDhcpLowerAddress (const QString &aValue) { mDhcpLowerAddress = aValue; }
    void setDhcpUpperAddress (const QString &aValue) { mDhcpUpperAddress = aValue; }

private:

    /* Common */
    CHostNetworkInterface mInterface;
    bool mChanged;

    /* Host-only Interface */
    bool mDhcpClientEnabled;
    QString mInterfaceAddress;
    QString mInterfaceMask;
    bool mIpv6Supported;
    QString mInterfaceAddress6;
    QString mInterfaceMaskLength6;

    /* DHCP Server */
    bool mDhcpServerEnabled;
    QString mDhcpServerAddress;
    QString mDhcpServerMask;
    QString mDhcpLowerAddress;
    QString mDhcpUpperAddress;
};

class VBoxGLSettingsNetwork : public VBoxSettingsPage,
                              public Ui::VBoxGLSettingsNetwork
{
    Q_OBJECT;

public:

    VBoxGLSettingsNetwork();

protected:

    void getFrom (const CSystemProperties &aProps,
                  const VBoxGlobalSettings &aGs);
    void putBackTo (CSystemProperties &aProps,
                    VBoxGlobalSettings &aGs);

    void setValidator (QIWidgetValidator *aVal);
    bool revalidate (QString &aWarning, QString &aTitle);

    void setOrderAfter (QWidget *aWidget);

    void retranslateUi();

private slots:

    void addInterface();
    void remInterface();
    void editInterface();
    void updateCurrentItem();
    void showContextMenu (const QPoint &aPos);

private:

    QAction *mAddInterface;
    QAction *mRemInterface;
    QAction *mEditInterface;

    QIWidgetValidator *mValidator;
};

#endif // __VBoxGLSettingsNetwork_h__

