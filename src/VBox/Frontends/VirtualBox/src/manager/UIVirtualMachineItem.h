/* $Id$ */
/** @file
 * VBox Qt GUI - UIVirtualMachineItem class declarations.
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIVirtualMachineItem_h__
#define __UIVirtualMachineItem_h__

/* Qt includes: */
#include <QDateTime>
#include <QMimeData>
#include <QPixmap>

/* GUI includes: */
#include "UISettingsDefs.h"

/* COM includes: */
#include "COMEnums.h"
#include "CVirtualBoxErrorInfo.h"
#include "CMachine.h"

/* Using declarations: */
using namespace UISettingsDefs;

class UIVirtualMachineItem
{
public:

    UIVirtualMachineItem(const CMachine &aMachine);
    virtual ~UIVirtualMachineItem();

    CMachine machine() const { return m_machine; }

    QString name() const { return m_strName; }
    QPixmap osPixmap(QSize *pLogicalSize = 0) const;
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
    const QStringList &groups();
    bool recache();
    /** Recaches item pixmap. */
    void recachePixmap();

    bool canSwitchTo() const;
    bool switchTo();

    bool hasDetails() const { return m_fHasDetails; }

    /** Returns configuration access level. */
    ConfigurationAccessLevel configurationAccessLevel() const { return m_configurationAccessLevel; }

    static bool isItemEditable(UIVirtualMachineItem *pItem);
    static bool isItemSaved(UIVirtualMachineItem *pItem);
    static bool isItemPoweredOff(UIVirtualMachineItem *pItem);
    static bool isItemStarted(UIVirtualMachineItem *pItem);
    static bool isItemRunning(UIVirtualMachineItem *pItem);
    static bool isItemRunningHeadless(UIVirtualMachineItem *pItem);
    static bool isItemPaused(UIVirtualMachineItem *pItem);
    static bool isItemStuck(UIVirtualMachineItem *pItem);

private:

    /* Private member vars */
    CMachine m_machine;

    /* Cached machine data (to minimize server requests) */
    QString m_strId;
    QString m_strSettingsFile;

    bool m_fAccessible;
    CVirtualBoxErrorInfo m_accessError;

    QString m_strName;
    QPixmap m_pixmap;
    QSize m_logicalPixmapSize;
    QString m_strSnapshotName;
    QDateTime m_lastStateChange;
    KMachineState m_machineState;
    KSessionState m_sessionState;
    QString m_strOSTypeId;
    ULONG m_cSnaphot;

    ULONG m_pid;

    bool m_fHasDetails;

    QStringList m_groups;
    /** Holds configuration access level. */
    ConfigurationAccessLevel m_configurationAccessLevel;
};

/* Make the pointer of this class public to the QVariant framework */
Q_DECLARE_METATYPE(UIVirtualMachineItem *);

class UIVirtualMachineItemMimeData: public QMimeData
{
    Q_OBJECT;

public:

    UIVirtualMachineItemMimeData(UIVirtualMachineItem *pItem);

    UIVirtualMachineItem *item() const;
    QStringList formats() const;

    static QString type();

private:

    /* Private member vars */
    UIVirtualMachineItem *m_pItem;

    static QString m_type;
};

#endif /* __UIVirtualMachineItem_h__ */
