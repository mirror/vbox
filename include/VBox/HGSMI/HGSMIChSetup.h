/** @file
 * VBox Host Guest Shared Memory Interface (HGSMI), sHost/Guest shared part.
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
 *
 * Sun Microsystems, Inc. confidential
 * All rights reserved
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

