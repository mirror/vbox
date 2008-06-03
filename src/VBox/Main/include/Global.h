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

#include <VBox/ostypes.h>

#include <iprt/types.h>

/**
 * Contains global static definitions that can be referenced by all COM classes
 * regardless of the apartment.
 */
class Global
{
public:

    /** Represents OS Type <-> string mappings. */
    const struct OSType
    {
        const char    *id;          /* utf-8 */
        const char    *description; /* utf-8 */
        const VBOXOSTYPE osType;
        const uint32_t recommendedRAM;
        const uint32_t recommendedVRAM;
        const uint32_t recommendedHDD;
    };

    static const OSType sOSTypes [SchemaDefs::OSTypeId_COUNT];

    static const char *OSTypeId (VBOXOSTYPE aOSType);
};

#endif /* ____H_GLOBAL */
