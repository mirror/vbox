/* $Id$ */

/** @file
 *
 * VirtualBox COM global declarations
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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

#ifndef ____H_GLOBAL
#define ____H_GLOBAL

/* generated header */
#include "SchemaDefs.h"

/* interface definitions */
#include "VBox/com/VirtualBox.h"

#include <VBox/ostypes.h>

#include <iprt/types.h>

#define VBOXOSHINT_NONE                 0
#define VBOXOSHINT_64BIT                RT_BIT(0)
#define VBOXOSHINT_HWVIRTEX             RT_BIT(1)
#define VBOXOSHINT_IOAPIC               RT_BIT(2)

/**
 * Contains global static definitions that can be referenced by all COM classes
 * regardless of the apartment.
 */
class Global
{
public:

    /** Represents OS Type <-> string mappings. */
    struct OSType
    {
        const char                 *familyId;          /* utf-8 */
        const char                 *familyDescription; /* utf-8 */
        const char                 *id;          /* utf-8 */
        const char                 *description; /* utf-8 */
        const VBOXOSTYPE            osType;
        const uint32_t              osHint;
        const uint32_t              recommendedRAM;
        const uint32_t              recommendedVRAM;
        const uint32_t              recommendedHDD;
        const NetworkAdapterType_T  networkAdapterType;
    };

    static const OSType sOSTypes [SchemaDefs::OSTypeId_COUNT];

    static const char *OSTypeId (VBOXOSTYPE aOSType);

    /**
     * Returns @c true if the given machine state is an online state. This is a
     * recommended way to detect if the VM is online (being executed in a
     * dedicated process) or not. Note that some online states are also
     * transitional states (see #IsTransitional()).
     */
    static bool IsOnline (MachineState_T aState)
    {
        return aState >= MachineState_FirstOnline &&
               aState <= MachineState_LastOnline;
    }

    /**
     * Returns @c true if the given machine state is a transient state. This is
     * a recommended way to detect if the VM is performing some potentially
     * lengthy operation (such as starting, stopping, saving, discarding
     * snapshot, etc.). Note some (but not all) transitional states are also
     * online states (see #IsOnline()).
     */
    static bool IsTransient (MachineState_T aState)
    {
        return aState >= MachineState_FirstTransient &&
               aState <= MachineState_LastTransient;
    }

    /**
     * Shortcut to <tt>IsOnline (aState) || IsTransient (aState)</tt>. When it
     * returns @false, the VM is turned off (no VM process) and not busy with
     * another exclusive operation.
     */
    static bool IsOnlineOrTransient (MachineState_T aState)
    {
        return IsOnline (aState) || IsTransient (aState);
    }

    /**
     * Shortcut to <tt>IsOnline (aState) && !IsTransient (aState)</tt>. This is
     * a recommended way to detect if the VM emulation thread is in action
     * (either running, suspended, or stuck). When this method returns @false,
     * then either the VM is not online or the emulation thread is being started
     * or stopped, etc.
     */
    static bool IsActive (MachineState_T aState)
    {
        return IsOnline (aState) && !IsTransient (aState);
    }
};

#endif /* ____H_GLOBAL */
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
