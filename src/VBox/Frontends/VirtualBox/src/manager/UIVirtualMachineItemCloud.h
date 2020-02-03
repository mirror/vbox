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

/** UIVirtualMachineItem sub-class used as cloud Virtual Machine item interface. */
class UIVirtualMachineItemCloud : public UIVirtualMachineItem
{
    Q_OBJECT;

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
    /** Destructs cloud VM item. */
    virtual ~UIVirtualMachineItemCloud();

    /** @name State attributes.
      * @{ */
        /** Returns fake cloud item state. */
        FakeCloudItemState fakeCloudItemState() const { return m_enmFakeCloudItemState; }
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

    /** @name State attributes.
      * @{ */
        /** Holds fake cloud item state. */
        FakeCloudItemState  m_enmFakeCloudItemState;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_manager_UIVirtualMachineItemCloud_h */
