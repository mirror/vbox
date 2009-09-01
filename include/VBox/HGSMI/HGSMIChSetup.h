/** @file
 * VBox Host Guest Shared Memory Interface (HGSMI), sHost/Guest shared part.
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___VBox_HGSMI_HGSMIChSetup_h
#define ___VBox_HGSMI_HGSMIChSetup_h

#include <VBox/HGSMI/HGSMI.h>

/* HGSMI setup and configuration channel commands and data structures. */
#define HGSMI_CC_HOST_FLAGS_LOCATION 0 /* Tell the host the location of HGSMIHOSTFLAGS structure,
                                        * where the host can write information about pending
                                        * buffers, etc, and which can be quickly polled by
                                        * the guest without a need to port IO.
                                        */

typedef struct _HGSMIBUFFERLOCATION
{
    HGSMIOFFSET offLocation;
    HGSMISIZE   cbLocation;
} HGSMIBUFFERLOCATION;
AssertCompileSize(HGSMIBUFFERLOCATION, 8);

/* HGSMI setup and configuration data structures. */
#define HGSMIHOSTFLAGS_COMMANDS_PENDING 0x1
#define HGSMIHOSTFLAGS_IRQ              0x2

typedef struct _HGSMIHOSTFLAGS
{
    uint32_t u32HostFlags;
    uint32_t au32Reserved[3];
} HGSMIHOSTFLAGS;
AssertCompileSize(HGSMIHOSTFLAGS, 16);

#endif

