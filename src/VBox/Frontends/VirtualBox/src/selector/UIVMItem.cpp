/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIVMItem class implementation
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
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
# include "precomp.h"
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
#include <QFileInfo>
#include <QIcon>

/* GUI includes: */
#include "UIVMItem.h"
#include "VBoxGlobal.h"
#include "COMEnumsWrapper.h"
#ifdef Q_WS_MAC
# include <ApplicationServices/ApplicationServices.h>
#endif /* Q_WS_MAC */

/* COM includes: */
#include "CSnapshot.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

// Helpers
////////////////////////////////////////////////////////////////////////////////

/// @todo Remove. See @c todo in #switchTo() below.
#if 0

#if defined (Q_WS_WIN32)

struct EnumWindowsProcData
{
    ULONG pid;
    WId wid;
};

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
    EnumWindowsProcData *d = (EnumWindowsProcData *) lParam;

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);

    if (d->pid == pid)
    {
        WINDOWINFO info;
        if (!GetWindowInfo(hwnd, &info))
            return TRUE;

#if 0
        LogFlowFunc(("pid=%d, wid=%08X\n", pid, hwnd));
        LogFlowFunc(("  parent=%08X\n", GetParent(hwnd)));
        LogFlowFunc(("  owner=%08X\n", GetWindow(hwnd, GW_OWNER)));
        TCHAR buf [256];
        LogFlowFunc(("  rcWindow=%d,%d;%d,%d\n",
                      info.rcWindow.left, info.rcWindow.top,
                      info.rcWindow.right, info.rcWindow.bottom));
        LogFlowFunc(("  dwStyle=%08X\n", info.dwStyle));
        LogFlowFunc(("  dwExStyle=%08X\n", info.dwExStyle));
        GetClassName(hwnd, buf, 256);
        LogFlowFunc(("  class=%ls\n", buf));
        GetWindowText(hwnd, buf, 256);
        LogFlowFunc(("  text=%ls\n", buf));
#endif

        /* we are interested in unowned top-level windows only */
        if (!(info.dwStyle & (WS_CHILD | WS_POPUP)) &&
            info.rcWindow.left < info.rcWindow.right &&
            info.rcWindow.top < info.rcWindow.bottom &&
            GetParent(hwnd) == NULL &&
            GetWindow(hwnd, GW_OWNER) == NULL)
        {
            d->wid = hwnd;
            /* if visible, stop the search immediately */
            if (info.dwStyle & WS_VISIBLE)
                return FALSE;
            /* otherwise, give other top-level windows a chance
             * (the last one wins) */
        }
    }

    return TRUE;
}

#endif

/**
 * Searches for a main window of the given process.
 *
 * @param aPid process ID to search for
 *
 * @return window ID on success or <tt>(WId) ~0</tt> otherwise.
 */
static WId FindWindowIdFromPid(ULONG aPid)
{
#if defined (Q_WS_WIN32)

    EnumWindowsProcData d = { aPid, (WId) ~0 };
    EnumWindows(EnumWindowsProc, (LPARAM) &d);
    LogFlowFunc(("SELECTED wid=%08X\n", d.wid));
    return d.wid;

#elif defined (Q_WS_X11)

    NOREF(aPid);
    return (WId) ~0;

#elif defined (Q_WS_MAC)

    /** @todo Figure out how to get access to another windows of another process...
     * Or at least check that it's not a VBoxVRDP process. */
    NOREF (aPid);
    return (WId) 0;

#else

    return (WId) ~0;

#endif
}

#endif

UIVMItem::UIVMItem(const CMachine &aMachine)
    : m_machine(aMachine)
{
    recache();
}

UIVMItem::~UIVMItem()
{
}

// public members
////////////////////////////////////////////////////////////////////////////////

QIcon UIVMItem::osIcon() const
{
    return m_fAccessible ? vboxGlobal().vmGuestOSTypeIcon(m_strOSTypeId) :
                           QPixmap(":/os_other.png");
}

QString UIVMItem::machineStateName() const
{
    return m_fAccessible ? gCOMenum->toString(m_machineState) :
           QApplication::translate("UIVMListView", "Inaccessible");
}

QIcon UIVMItem::machineStateIcon() const
{
    return m_fAccessible ? gCOMenum->toIcon(m_machineState) :
                           QPixmap(":/state_aborted_16px.png");
}

QString UIVMItem::sessionStateName() const
{
    return m_fAccessible ? gCOMenum->toString(m_sessionState) :
           QApplication::translate("UIVMListView", "Inaccessible");
}

QString UIVMItem::toolTipText() const
{
    QString dateTime = (m_lastStateChange.date() == QDate::currentDate()) ?
                        m_lastStateChange.time().toString(Qt::LocalDate) :
                        m_lastStateChange.toString(Qt::LocalDate);

    QString toolTip;

    if (m_fAccessible)
    {
        toolTip = QString("<b>%1</b>").arg(m_strName);
        if (!m_strSnapshotName.isNull())
            toolTip += QString(" (%1)").arg(m_strSnapshotName);
        toolTip = QApplication::translate("UIVMListView",
            "<nobr>%1<br></nobr>"
            "<nobr>%2 since %3</nobr><br>"
            "<nobr>Session %4</nobr>",
            "VM tooltip (name, last state change, session state)")
            .arg(toolTip)
            .arg(gCOMenum->toString(m_machineState))
            .arg(dateTime)
            .arg(gCOMenum->toString(m_sessionState));
    }
    else
    {
        toolTip = QApplication::translate("UIVMListView",
            "<nobr><b>%1</b><br></nobr>"
            "<nobr>Inaccessible since %2</nobr>",
            "Inaccessible VM tooltip (name, last state change)")
            .arg(m_strSettingsFile)
            .arg(dateTime);
    }

    return toolTip;
}

bool UIVMItem::recache()
{
    bool needsResort = true;

    m_strId = m_machine.GetId();
    m_strSettingsFile = m_machine.GetSettingsFilePath();

    m_fAccessible = m_machine.GetAccessible();
    if (m_fAccessible)
    {
        QString name = m_machine.GetName();

        CSnapshot snp = m_machine.GetCurrentSnapshot();
        m_strSnapshotName = snp.isNull() ? QString::null : snp.GetName();
        needsResort = name != m_strName;
        m_strName = name;

        m_machineState = m_machine.GetState();
        m_lastStateChange.setTime_t(m_machine.GetLastStateChange() / 1000);
        m_sessionState = m_machine.GetSessionState();
        m_strOSTypeId = m_machine.GetOSTypeId();
        m_cSnaphot = m_machine.GetSnapshotCount();

        if (   m_machineState == KMachineState_PoweredOff
            || m_machineState == KMachineState_Saved
            || m_machineState == KMachineState_Teleported
            || m_machineState == KMachineState_Aborted
           )
        {
            m_pid = (ULONG) ~0;
    /// @todo Remove. See @c todo in #switchTo() below.
#if 0
            mWinId = (WId) ~0;
#endif
        }
        else
        {
            m_pid = m_machine.GetSessionPid();
    /// @todo Remove. See @c todo in #switchTo() below.
#if 0
            mWinId = FindWindowIdFromPid(m_pid);
#endif
        }
    }
    else
    {
        m_accessError = m_machine.GetAccessError();

        /* this should be in sync with
         * UIMessageCenter::confirm_machineDeletion() */
        QFileInfo fi(m_strSettingsFile);
        QString name = VBoxGlobal::hasAllowedExtension(fi.completeSuffix(), VBoxFileExts) ?
                       fi.completeBaseName() : fi.fileName();
        needsResort = name != m_strName;
        m_strName = name;
        m_machineState = KMachineState_Null;
        m_sessionState = KSessionState_Null;
        m_lastStateChange = QDateTime::currentDateTime();
        m_strOSTypeId = QString::null;
        m_cSnaphot = 0;

        m_pid = (ULONG) ~0;
    /// @todo Remove. See @c todo in #switchTo() below.
#if 0
        mWinId = (WId) ~0;
#endif
    }

    return needsResort;
}

/**
 * Returns @a true if we can activate and bring the VM console window to
 * foreground, and @a false otherwise.
 */
bool UIVMItem::canSwitchTo() const
{
    return const_cast <CMachine &>(m_machine).CanShowConsoleWindow();

    /// @todo Remove. See @c todo in #switchTo() below.
#if 0
    return mWinId != (WId) ~0;
#endif
}

/**
 * Tries to switch to the main window of the VM process.
 *
 * @return true if successfully switched and false otherwise.
 */
bool UIVMItem::switchTo()
{
#ifdef Q_WS_MAC
    ULONG64 id = m_machine.ShowConsoleWindow();
#else
    WId id = (WId) m_machine.ShowConsoleWindow();
#endif
    AssertWrapperOk(m_machine);
    if (!m_machine.isOk())
        return false;

    /* winId = 0 it means the console window has already done everything
     * necessary to implement the "show window" semantics. */
    if (id == 0)
        return true;

#if defined (Q_WS_WIN32) || defined (Q_WS_X11)

    return vboxGlobal().activateWindow(id, true);

#elif defined (Q_WS_MAC)
    /*
     * This is just for the case were the other process cannot steal
     * the focus from us. It will send us a PSN so we can try.
     */
    ProcessSerialNumber psn;
    psn.highLongOfPSN = id >> 32;
    psn.lowLongOfPSN = (UInt32)id;
    OSErr rc = ::SetFrontProcess(&psn);
    if (!rc)
        Log(("GUI: %#RX64 couldn't do SetFrontProcess on itself, the selector (we) had to do it...\n", id));
    else
        Log(("GUI: Failed to bring %#RX64 to front. rc=%#x\n", id, rc));
    return !rc;

#endif

    return false;

    /// @todo Below is the old method of switching to the console window
    //  based on the process ID of the console process. It should go away
    //  after the new (callback-based) method is fully tested.
#if 0

    if (!canSwitchTo())
        return false;

#if defined (Q_WS_WIN32)

    HWND hwnd = mWinId;

    /* if there are blockers (modal and modeless dialogs, etc), find the
     * topmost one */
    HWND hwndAbove = NULL;
    do
    {
        hwndAbove = GetNextWindow(hwnd, GW_HWNDPREV);
        HWND hwndOwner;
        if (hwndAbove != NULL &&
            ((hwndOwner = GetWindow(hwndAbove, GW_OWNER)) == hwnd ||
             hwndOwner  == hwndAbove))
            hwnd = hwndAbove;
        else
            break;
    }
    while (1);

    /* first, check that the primary window is visible */
    if (IsIconic(mWinId))
        ShowWindow(mWinId, SW_RESTORE);
    else if (!IsWindowVisible(mWinId))
        ShowWindow(mWinId, SW_SHOW);

#if 0
    LogFlowFunc(("mWinId=%08X hwnd=%08X\n", mWinId, hwnd));
#endif

    /* then, activate the topmost in the group */
    AllowSetForegroundWindow(m_pid);
    SetForegroundWindow(hwnd);

    return true;

#elif defined (Q_WS_X11)

    return false;

#elif defined (Q_WS_MAC)

    ProcessSerialNumber psn;
    OSStatus rc = ::GetProcessForPID(m_pid, &psn);
    if (!rc)
    {
        rc = ::SetFrontProcess(&psn);

        if (!rc)
        {
            ShowHideProcess(&psn, true);
            return true;
        }
    }
    return false;

#else

    return false;

#endif

#endif
}

QString UIVMItemMimeData::m_type = "application/org.virtualbox.gui.vmselector.uivmitem";

UIVMItemMimeData::UIVMItemMimeData(UIVMItem *pItem)
  : m_pItem(pItem)
{
}

UIVMItem *UIVMItemMimeData::item() const
{
    return m_pItem;
}

QStringList UIVMItemMimeData::formats() const
{
    QStringList types;
    types << type();
    return types;
}

/* static */
QString UIVMItemMimeData::type()
{
    return m_type;
}

