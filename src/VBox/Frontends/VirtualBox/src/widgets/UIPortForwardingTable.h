/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIPortForwardingTable class declaration
 */

/*
 * Copyright (C) 2010-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIPortForwardingTable_h__
#define __UIPortForwardingTable_h__

/* Qt includes: */
#include <QWidget>

/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* COM includes: */
#include "COMEnums.h"

/* Forward declarations: */
class QITableView;
class UIToolBar;
class QIDialogButtonBox;
class UIPortForwardingModel;

/* Name data: */
class NameData : public QString
{
public:

    NameData() : QString() {}
    NameData(const QString &strName) : QString(strName) {}
};
Q_DECLARE_METATYPE(NameData);

/* Ip data: */
class IpData : public QString
{
public:

    IpData() : QString() {}
    IpData(const QString &strIP) : QString(strIP) {}
};
Q_DECLARE_METATYPE(IpData);

/* Port data: */
class PortData
{
public:

    PortData() : m_uValue(0) {}
    PortData(ushort uValue) : m_uValue(uValue) {}
    PortData(const PortData &other) : m_uValue(other.value()) {}
    bool operator==(const PortData &other) { return m_uValue == other.m_uValue; }
    ushort value() const { return m_uValue; }

private:

    ushort m_uValue;
};
Q_DECLARE_METATYPE(PortData);

/* Port forwarding data: */
struct UIPortForwardingData
{
    UIPortForwardingData(const NameData &strName, KNATProtocol protocol,
                         const IpData &strHostIP, PortData uHostPort,
                         const IpData &strGuestIP, PortData uGuestPort)
        : name(strName), protocol(protocol)
        , hostIp(strHostIP), hostPort(uHostPort)
        , guestIp(strGuestIP), guestPort(uGuestPort) {}
    bool operator==(const UIPortForwardingData &other)
    {
        return name == other.name &&
               protocol == other.protocol &&
               hostIp == other.hostIp &&
               hostPort == other.hostPort &&
               guestIp == other.guestIp &&
               guestPort == other.guestPort;
    }
    NameData name;
    KNATProtocol protocol;
    IpData hostIp;
    PortData hostPort;
    IpData guestIp;
    PortData guestPort;
};

/* Port forwarding data list: */
typedef QList<UIPortForwardingData> UIPortForwardingDataList;

/* Port forwarding dialog: */
class UIPortForwardingTable : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIPortForwardingTable(const UIPortForwardingDataList &rules, bool fIPv6);

    /* API: Rules stuff: */
    const UIPortForwardingDataList& rules() const;
    bool validate() const;
    bool discard() const;

private slots:

    /* Handlers: Table operation stuff: */
    void sltAddRule();
    void sltCopyRule();
    void sltDelRule();

    /* Handlers: Table stuff: */
    void sltTableDataChanged();
    void sltCurrentChanged();
    void sltShowTableContexMenu(const QPoint &position);
    void sltAdjustTable();

private:

    /* Handler: Translation stuff: */
    void retranslateUi();

    /* Handlers: Event-processing stuff: */
    bool eventFilter(QObject *pObject, QEvent *pEvent);

    /* Flags: */
    bool m_fIsTableDataChanged;

    /* Widgets: */
    QITableView *m_pTableView;
    UIToolBar *m_pToolBar;

    /* Model: */
    UIPortForwardingModel *m_pModel;

    /* Actions: */
    QAction *m_pAddAction;
    QAction *m_pCopyAction;
    QAction *m_pDelAction;
};

#endif // __UIPortForwardingTable_h__
