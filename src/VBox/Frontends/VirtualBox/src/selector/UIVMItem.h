/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIVMItem class declarations
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIVMItem_h__
#define __UIVMItem_h__

/* Qt includes: */
#include <QDateTime>
#include <QMimeData>

/* COM includes: */
#include "COMEnums.h"
#include "CVirtualBoxErrorInfo.h"
#include "CMachine.h"

class UIVMItem
{
public:

    UIVMItem(const CMachine &aMachine);
    virtual ~UIVMItem();

    CMachine machine() const { return m_machine; }

    QString name() const { return m_strName; }
    QIcon osIcon() const;
    QString osTypeId() const { return m_strOSTypeId; }
    QString id() const { return m_strId; }

    QString machineStateName() const;
    QIcon machineStateIcon() const;

    QString sessionStateName() const;

    QString snapshotName() const { return m_strSnapshotName; }
    ULONG snapshotCount() const { return m_cSnaphot; }

    QString toolTipText() const;

    bool accessible() const { return m_fAccessible; }
    const CVirtualBoxErrorInfo &accessError() const { return m_accessError; }
    KMachineState machineState() const { return m_machineState; }
    KSessionState sessionState() const { return m_sessionState; }

    QString settingsFile() const { return m_strSettingsFile; }

    bool recache();

    bool canSwitchTo() const;
    bool switchTo();

    bool reconfigurable() const { return m_fReconfigurable; }
    bool hasDetails() const { return m_fHasDetails; }

    static bool isItemEditable(UIVMItem *pItem);
    static bool isItemSaved(UIVMItem *pItem);
    static bool isItemPoweredOff(UIVMItem *pItem);
    static bool isItemStarted(UIVMItem *pItem);
    static bool isItemRunning(UIVMItem *pItem);
    static bool isItemPaused(UIVMItem *pItem);
    static bool isItemStuck(UIVMItem *pItem);

private:

    /* Private member vars */
    CMachine m_machine;

    /* Cached machine data (to minimize server requests) */
    QString m_strId;
    QString m_strSettingsFile;

    bool m_fAccessible;
    CVirtualBoxErrorInfo m_accessError;

    QString m_strName;
    QString m_strSnapshotName;
    QDateTime m_lastStateChange;
    KMachineState m_machineState;
    KSessionState m_sessionState;
    QString m_strOSTypeId;
    ULONG m_cSnaphot;

    ULONG m_pid;

    bool m_fReconfigurable;
    bool m_fHasDetails;
};

/* Make the pointer of this class public to the QVariant framework */
Q_DECLARE_METATYPE(UIVMItem *);

class UIVMItemMimeData: public QMimeData
{
    Q_OBJECT;

public:

    UIVMItemMimeData(UIVMItem *pItem);

    UIVMItem *item() const;
    QStringList formats() const;

    static QString type();

private:

    /* Private member vars */
    UIVMItem *m_pItem;

    static QString m_type;
};

#endif /* __UIVMItem_h__ */

