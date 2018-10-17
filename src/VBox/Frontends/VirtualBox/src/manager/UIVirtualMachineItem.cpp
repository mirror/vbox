/* $Id$ */
/** @file
 * VBox Qt GUI - UIVirtualMachineItem class implementation.
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

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QApplication>
# include <QFileInfo>
# include <QIcon>

/* GUI includes: */
# include "UIVirtualMachineItem.h"
# include "VBoxGlobal.h"
# include "UIConverter.h"
# include "UIExtraDataManager.h"
# ifdef VBOX_WS_MAC
#  include <ApplicationServices/ApplicationServices.h>
# endif /* VBOX_WS_MAC */

/* COM includes: */
# include "CSnapshot.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIVirtualMachineItem::UIVirtualMachineItem(const CMachine &aMachine)
    : m_machine(aMachine)
{
    recache();
}

UIVirtualMachineItem::~UIVirtualMachineItem()
{
}

QPixmap UIVirtualMachineItem::osPixmap(QSize *pLogicalSize /* = 0 */) const
{
    if (pLogicalSize)
        *pLogicalSize = m_logicalPixmapSize;
    return m_pixmap;
}

QString UIVirtualMachineItem::machineStateName() const
{
    return m_fAccessible ? gpConverter->toString(m_machineState) :
           QApplication::translate("UIVMListView", "Inaccessible");
}

QIcon UIVirtualMachineItem::machineStateIcon() const
{
    return m_fAccessible ? gpConverter->toIcon(m_machineState) :
                           gpConverter->toIcon(KMachineState_Aborted);
}

QString UIVirtualMachineItem::sessionStateName() const
{
    return m_fAccessible ? gpConverter->toString(m_sessionState) :
           QApplication::translate("UIVMListView", "Inaccessible");
}

QString UIVirtualMachineItem::toolTipText() const
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
            .arg(gpConverter->toString(m_machineState))
            .arg(dateTime)
            .arg(gpConverter->toString(m_sessionState).toLower());
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

const QStringList& UIVirtualMachineItem::groups()
{
    return m_groups;
}

bool UIVirtualMachineItem::recache()
{
    bool needsResort = true;

    m_uId = m_machine.GetId();
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

        m_groups = m_machine.GetGroups().toList();

        if (   m_machineState == KMachineState_PoweredOff
            || m_machineState == KMachineState_Saved
            || m_machineState == KMachineState_Teleported
            || m_machineState == KMachineState_Aborted
           )
        {
            m_pid = (ULONG) ~0;
        }
        else
        {
            m_pid = m_machine.GetSessionPID();
        }

        /* Determine configuration access level: */
        m_configurationAccessLevel = ::configurationAccessLevel(m_sessionState, m_machineState);
        /* Also take restrictions into account: */
        if (   m_configurationAccessLevel != ConfigurationAccessLevel_Null
            && !gEDataManager->machineReconfigurationEnabled(m_uId))
            m_configurationAccessLevel = ConfigurationAccessLevel_Null;

        /* Should we show details for this item? */
        m_fHasDetails = gEDataManager->showMachineInSelectorDetails(m_uId);
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

        m_groups.clear();
        m_pid = (ULONG) ~0;

        /* Set configuration access level to NULL: */
        m_configurationAccessLevel = ConfigurationAccessLevel_Null;

        /* Should we show details for this item? */
        m_fHasDetails = true;
    }

    /* Recache item pixmap: */
    recachePixmap();

    return needsResort;
}

void UIVirtualMachineItem::recachePixmap()
{
    /* If machine is accessible: */
    if (m_fAccessible)
    {
        /* First, we are trying to acquire personal machine guest OS type icon: */
        m_pixmap = vboxGlobal().vmUserPixmapDefault(m_machine, &m_logicalPixmapSize);
        /* If there is nothing, we are using icon corresponding to cached guest OS type: */
        if (m_pixmap.isNull())
            m_pixmap = vboxGlobal().vmGuestOSTypePixmapDefault(m_strOSTypeId, &m_logicalPixmapSize);
    }
    /* Otherwise: */
    else
    {
        /* We are using "Other" guest OS type icon: */
        m_pixmap = vboxGlobal().vmGuestOSTypePixmapDefault("Other", &m_logicalPixmapSize);
    }
}

/**
 * Returns @a true if we can activate and bring the VM console window to
 * foreground, and @a false otherwise.
 */
bool UIVirtualMachineItem::canSwitchTo() const
{
    return const_cast <CMachine &>(m_machine).CanShowConsoleWindow();
}

/**
 * Tries to switch to the main window of the VM process.
 *
 * @return true if successfully switched and false otherwise.
 */
bool UIVirtualMachineItem::switchTo()
{
#ifdef VBOX_WS_MAC
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

#if defined (VBOX_WS_WIN) || defined (VBOX_WS_X11)

    return vboxGlobal().activateWindow(id, true);

#elif defined (VBOX_WS_MAC)
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

#else
    return false;
#endif
}

/* static */
bool UIVirtualMachineItem::isItemEditable(UIVirtualMachineItem *pItem)
{
    return pItem &&
           pItem->accessible() &&
           pItem->sessionState() == KSessionState_Unlocked;
}

/* static */
bool UIVirtualMachineItem::isItemSaved(UIVirtualMachineItem *pItem)
{
    return pItem &&
           pItem->accessible() &&
           pItem->machineState() == KMachineState_Saved;
}

/* static */
bool UIVirtualMachineItem::isItemPoweredOff(UIVirtualMachineItem *pItem)
{
    return pItem &&
           pItem->accessible() &&
           (pItem->machineState() == KMachineState_PoweredOff ||
            pItem->machineState() == KMachineState_Saved ||
            pItem->machineState() == KMachineState_Teleported ||
            pItem->machineState() == KMachineState_Aborted);
}

/* static */
bool UIVirtualMachineItem::isItemStarted(UIVirtualMachineItem *pItem)
{
    return isItemRunning(pItem) || isItemPaused(pItem);
}

/* static */
bool UIVirtualMachineItem::isItemRunning(UIVirtualMachineItem *pItem)
{
    return pItem &&
           pItem->accessible() &&
           (pItem->machineState() == KMachineState_Running ||
            pItem->machineState() == KMachineState_Teleporting ||
            pItem->machineState() == KMachineState_LiveSnapshotting);
}

/* static */
bool UIVirtualMachineItem::isItemRunningHeadless(UIVirtualMachineItem *pItem)
{
    if (isItemRunning(pItem))
    {
        /* Open session to determine which frontend VM is started with: */
        CSession session = vboxGlobal().openExistingSession(pItem->id());
        if (!session.isNull())
        {
            /* Acquire the session name: */
            const QString strSessionName = session.GetMachine().GetSessionName();
            /* Close the session early: */
            session.UnlockMachine();
            /* Check whether we are in 'headless' session: */
            return strSessionName == "headless";
        }
    }
    return false;
}

/* static */
bool UIVirtualMachineItem::isItemPaused(UIVirtualMachineItem *pItem)
{
    return pItem &&
           pItem->accessible() &&
           (pItem->machineState() == KMachineState_Paused ||
            pItem->machineState() == KMachineState_TeleportingPausedVM);
}

/* static */
bool UIVirtualMachineItem::isItemStuck(UIVirtualMachineItem *pItem)
{
    return pItem &&
           pItem->accessible() &&
           pItem->machineState() == KMachineState_Stuck;
}

QString UIVirtualMachineItemMimeData::m_type = "application/org.virtualbox.gui.vmselector.UIVirtualMachineItem";

UIVirtualMachineItemMimeData::UIVirtualMachineItemMimeData(UIVirtualMachineItem *pItem)
  : m_pItem(pItem)
{
}

UIVirtualMachineItem *UIVirtualMachineItemMimeData::item() const
{
    return m_pItem;
}

QStringList UIVirtualMachineItemMimeData::formats() const
{
    QStringList types;
    types << type();
    return types;
}

/* static */
QString UIVirtualMachineItemMimeData::type()
{
    return m_type;
}
