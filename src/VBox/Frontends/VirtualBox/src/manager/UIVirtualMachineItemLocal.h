/* $Id$ */
/** @file
 * VBox Qt GUI - UIVirtualMachineItemLocal class declaration.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_manager_UIVirtualMachineItemLocal_h
#define FEQT_INCLUDED_SRC_manager_UIVirtualMachineItemLocal_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QDateTime>

/* GUI includes: */
#include "UIVirtualMachineItem.h"

/* COM includes: */
#include "CMachine.h"

/** UIVirtualMachineItem sub-class used as local Virtual Machine item interface. */
class UIVirtualMachineItemLocal : public UIVirtualMachineItem
{
    Q_OBJECT;

public:

    /** Constructs local VM item on the basis of taken @a comMachine. */
    UIVirtualMachineItemLocal(const CMachine &comMachine);
    /** Destructs local VM item. */
    virtual ~UIVirtualMachineItemLocal();

    /** @name Arguments.
      * @{ */
        /** Returns cached virtual machine object. */
        CMachine machine() const { return m_comMachine; }
    /** @} */

    /** @name Basic attributes.
      * @{ */
        /** Returns cached machine settings file name. */
        QString settingsFile() const { return m_strSettingsFile; }
        /** Returns cached machine group list. */
        const QStringList &groups() { return m_groups; }
    /** @} */

    /** @name Snapshot attributes.
      * @{ */
        /** Returns cached snapshot name. */
        QString snapshotName() const { return m_strSnapshotName; }
        /** Returns cached snapshot children count. */
        ULONG snapshotCount() const { return m_cSnaphot; }
    /** @} */

    /** @name State attributes.
      * @{ */
        /** Returns cached session state. */
        KSessionState sessionState() const { return m_enmSessionState; }
        /** Returns cached session state name. */
        QString sessionStateName() const { return m_strSessionStateName; }
    /** @} */

    /** @name Console attributes.
      * @{ */
        /** Returns whether we can switch to main window of VM process. */
        bool canSwitchTo() const;
        /** Tries to switch to the main window of the VM process.
          * @return true if switched successfully. */
        bool switchTo();
    /** @} */

    /** @name Update stuff.
      * @{ */
        /** Recaches machine data. */
        virtual void recache() /* override */;
        /** Recaches machine item pixmap. */
        virtual void recachePixmap() /* override */;
    /** @} */

    /** @name Validation stuff.
      * @{ */
        /** Returns whether passed machine @a pItem is editable. */
        virtual bool isItemEditable() const /* override */;
        /** Returns whether passed machine @a pItem is saved. */
        virtual bool isItemSaved() const /* override */;
        /** Returns whether passed machine @a pItem is powered off. */
        virtual bool isItemPoweredOff() const /* override */;
        /** Returns whether passed machine @a pItem is started. */
        virtual bool isItemStarted() const /* override */;
        /** Returns whether passed machine @a pItem is running. */
        virtual bool isItemRunning() const /* override */;
        /** Returns whether passed machine @a pItem is running headless. */
        virtual bool isItemRunningHeadless() const /* override */;
        /** Returns whether passed machine @a pItem is paused. */
        virtual bool isItemPaused() const /* override */;
        /** Returns whether passed machine @a pItem is stuck. */
        virtual bool isItemStuck() const /* override */;
    /** @} */

protected:

    /** @name Event handling.
      * @{ */
        /** Handles translation event. */
        virtual void retranslateUi() /* override */;
    /** @} */

private:

    /** @name Arguments.
      * @{ */
        /** Holds cached machine object reference. */
        CMachine  m_comMachine;
    /** @} */

    /** @name Basic attributes.
      * @{ */
        /** Holds cached machine settings file name. */
        QString      m_strSettingsFile;
        /** Holds cached machine group list. */
        QStringList  m_groups;
    /** @} */

    /** @name Snapshot attributes.
      * @{ */
        /** Holds cached snapshot name. */
        QString    m_strSnapshotName;
        /** Holds cached last state change date/time. */
        QDateTime  m_lastStateChange;
        /** Holds cached snapshot children count. */
        ULONG      m_cSnaphot;
    /** @} */

    /** @name State attributes.
      * @{ */
        /** Holds cached session state. */
        KSessionState  m_enmSessionState;
        /** Holds cached session state name. */
        QString        m_strSessionStateName;
    /** @} */

    /** @name Console attributes.
      * @{ */
        /** Holds machine PID. */
        ULONG  m_pid;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_manager_UIVirtualMachineItemLocal_h */
