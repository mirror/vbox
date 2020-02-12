/* $Id$ */
/** @file
 * VBox Qt GUI - UIVirtualMachineItemCloud class declarations.
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

#ifndef FEQT_INCLUDED_SRC_manager_UIVirtualMachineItemCloud_h
#define FEQT_INCLUDED_SRC_manager_UIVirtualMachineItemCloud_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UIVirtualMachineItem.h"

/* Forward declarations: */
class UICloudMachine;

/** UIVirtualMachineItem sub-class used as cloud Virtual Machine item interface. */
class UIVirtualMachineItemCloud : public UIVirtualMachineItem
{
    Q_OBJECT;

signals:

    /** Notifies listeners about state change. */
    void sigStateChange();

public:

    /** Fake cloud item states. */
    enum FakeCloudItemState
    {
        FakeCloudItemState_NotApplicable,
        FakeCloudItemState_Loading,
        FakeCloudItemState_Done
    };

    /** Constructs fake cloud VM item. */
    UIVirtualMachineItemCloud();
    /** Constructs real cloud VM item on the basis of taken @a guiCloudMachine. */
    UIVirtualMachineItemCloud(const UICloudMachine &guiCloudMachine);
    /** Destructs cloud VM item. */
    virtual ~UIVirtualMachineItemCloud();

    /** @name State attributes.
      * @{ */
        /** Defines fake cloud item @a enmState. */
        void setFakeCloudItemState(FakeCloudItemState enmState) { m_enmFakeCloudItemState = enmState; }
        /** Returns fake cloud item state. */
        FakeCloudItemState fakeCloudItemState() const { return m_enmFakeCloudItemState; }

        /** Updates cloud VM state.
          * @param  pWidget  Brings parent widget to show messages according to. */
        void updateState(QWidget *pParent);
        /** Acquires instance info of certain @a enmType.
          * @param  pWidget  Brings parent widget to show messages according to. */
        QString acquireInstanceInfo(KVirtualSystemDescriptionType enmType, QWidget *pParent);

        /** Puts cloud VM on pause.
          * @param  pWidget  Brings parent widget to show messages according to. */
        void pause(QWidget *pParent);
        /** Resumes cloud VM execution.
          * @param  pWidget  Brings parent widget to show messages according to. */
        void resume(QWidget *pParent);
        /** Wrapper to handle two tasks above.
          * @param  fPause   Brings whether cloud VM should be paused or resumed otherwise.
          * @param  pWidget  Brings parent widget to show messages according to. */
        void pauseOrResume(bool fPause, QWidget *pParent);
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
        /** Holds cached cloud machine object reference. */
        UICloudMachine *m_pCloudMachine;
    /** @} */

    /** @name State attributes.
      * @{ */
        /** Holds fake cloud item state. */
        FakeCloudItemState  m_enmFakeCloudItemState;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_manager_UIVirtualMachineItemCloud_h */
