/** @file
 *
 * Guest client: seamless mode
 * Abstract class for interacting with the guest system
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __Additions_client_seamless_guest_h
# define __Additions_client_seamless_guest_h

#include <iprt/types.h>      /* for RTRECT */

#include "seamless-glue.h"

/**
 * Observable to monitor the state of the guest windows.  This abstract class definition
 * serves as a template (in the linguistic sense, not the C++ sense) for creating
 * platform-specific child classes.
 */
class VBoxGuestSeamlessGuest
{
public:
    /** Events which can be reported by this class */
    enum meEvent
    {
        /** Empty event */
        NONE,
        /** Seamless mode is now supported */
        CAPABLE,
        /** Seamless mode is no longer supported */
        INCAPABLE
    };
};

#if defined(RT_OS_LINUX) || defined(RT_OS_SOLARIS) || defined(RT_OS_FREEBSD)
# include "seamless-x11.h"  /* for VBoxGuestSeamlessGuestImpl */
#else
# error Port me
#endif

#endif /* __Additions_client_seamless_guest_h not defined */
