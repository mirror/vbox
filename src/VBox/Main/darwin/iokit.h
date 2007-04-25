/** $Id: $ */
/** @file
 * Main - Darwin IOKit Routines.
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#ifndef ___darwin_iokit_h___
#define ___darwin_iokit_h___

#include <iprt/cdefs.h>
#include <iprt/types.h>

/**
 * Darwin DVD descriptor as returned by DarwinGetDVDDrives().
 */
typedef struct DARWINDVD
{
    /** Pointer to the next DVD. */
    struct DARWINDVD *pNext;
    /** Variable length name / identifier. */
    char szName[1];
} DARWINDVD, *PDARWINDVD;


__BEGIN_DECLS
PDARWINDVD DarwinGetDVDDrives(void);
__END_DECLS


#endif

