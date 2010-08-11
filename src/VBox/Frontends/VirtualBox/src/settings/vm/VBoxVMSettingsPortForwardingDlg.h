/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMSettingsPortForwardingDlg class declaration
 */

/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __VBoxVMSettingsPortForwardingDlg_h__
#define __VBoxVMSettingsPortForwardingDlg_h__

/* Local includes */
#include "QIWithRetranslateUI.h"
#include "QIDialog.h"
#include "COMDefs.h"

/* Forward declarations: */
class QITableView;
class VBoxToolBar;
class QIDialogButtonBox;
class UIPortForwardingModel;

/* Name data: */
class NameData : public QString
{
public:

    NameData() : QString() {}
    NameData(const QString &ruleName) : QString(ruleName) {}
};
Q_DECLARE_METATYPE(NameData);

/* Ip data: */
class IpData : public QString
{
public:

    IpData() : QString() {}
    IpData(const QString &ipAddress) : QString(ipAddress) {}
};
Q_DECLARE_METATYPE(IpData);

/* Port data: */
class PortData
{
public:

    PortData() : m_uValue(0) {}
    PortData(ushort uValue) : m_uValue(uValue) {}
    PortData(const PortData &other) : m_uValue(other.value()) {}
    ushort value() const { return m_uValue; }

private:

    ushort m_uValue;
};
Q_DECLARE_METATYPE(PortData);

/* Port forwarding data: */
struct UIPortForwardingData
{
    UIPortForwardingData(const NameData &strName, KNATProtocol eProtocol,
                         const IpData &strHostIp, PortData uHostPort,
                         const IpData &strGuestIp, PortData uGuestPort)
        : name(strName), protocol(eProtocol)
        , hostIp(strHostIp), hostPort(uHostPort)
        , guestIp(strGuestIp), guestPort(uGuestPort) {}
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
class VBoxVMSettingsPortForwardingDlg : public QIWithRetranslateUI<QIDialog>
{
    Q_OBJECT;

public:

    /* Port forwarding dialog constructor: */
    VBoxVMSettingsPortForwardingDlg(QWidget *pParent = 0,
                                    const UIPortForwardingDataList &rules = UIPortForwardingDataList());
    /* Port forwarding dialog destructor: */
    ~VBoxVMSettingsPortForwardingDlg();

    /* The list of chosen rules: */
    const UIPortForwardingDataList& rules() const;

private slots:

    /* Action's slots: */
    void sltAddRule();
    void sltCopyRule();
    void sltDelRule();

    /* Table slots: */
    void sltTableDataChanged();
    void sltCurrentChanged();
    void sltShowTableContexMenu(const QPoint &position);
    void sltAdjustTable();

    /* Dialog slots: */
    void accept();
    void reject();

private:

    /* UI Translator: */
    void retranslateUi();

    /* Event filter: */
    bool eventFilter(QObject *pObj, QEvent *pEvent);

    /* Flags: */
    bool fIsTableDataChanged;

    /* Widgets: */
    QITableView *m_pTableView;
    VBoxToolBar *m_pToolBar;
    QIDialogButtonBox *m_pButtonBox;

    /* Model: */
    UIPortForwardingModel *m_pModel;

    /* Actions: */
    QAction *m_pAddAction;
    QAction *m_pCopyAction;
    QAction *m_pDelAction;
};

#endif // __VBoxVMSettingsPortForwardingDlg_h__

