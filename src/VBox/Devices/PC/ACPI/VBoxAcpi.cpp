/** @file
 * VBoxAcpi - Virtual Box ACPI maniputation functionality.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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

#if !defined(IN_RING3)
#error Pure R3 code
#endif

#include <VBox/pdmdev.h>
#include <VBox/pgm.h>
#include <VBox/log.h>
#include <VBox/param.h>
#include <iprt/assert.h>
#include <iprt/alloc.h>
#include <iprt/string.h>

#ifdef VBOX_WITH_DYNAMIC_DSDT
/* vbox.dsl - input to generate proper DSDT on the fly */
# include <vboxdsl.hex>
#else
/* Statically compiled AML */
# include <vboxaml.hex>
#endif

#ifdef VBOX_WITH_DYNAMIC_DSDT
static int prepareDynamicDsdt(PPDMDEVINS pDevIns,  
                              void*      *ppPtr, 
                              size_t     *puDsdtLen)
{
    //LogRel(("file is %s\n", g_abVboxDslSource));
    *ppPtr = NULL;
    *puDsdtLen = 0;
    return 0;
}

static int cleanupDynamicDsdt(PPDMDEVINS pDevIns, 
                              void*      pPtr)
{
    return 0;
}
#endif

/* Two only public functions */
int acpiPrepareDsdt(PPDMDEVINS pDevIns,  void * *ppPtr, size_t *puDsdtLen)
{
#ifdef VBOX_WITH_DYNAMIC_DSDT
    return prepareDynamicDsdt(pDevIns, ppPtr, puDsdtLen);
#else
    *ppPtr = AmlCode;
    *puDsdtLen = sizeof(AmlCode);
    return 0;
#endif
}

int acpiCleanupDsdt(PPDMDEVINS pDevIns,  void * pPtr)
{
#ifdef VBOX_WITH_DYNAMIC_DSDT
    return cleanupDynamicDsdt(pDevIns, pPtr);
#else
    /* Do nothing */
    return 0;
#endif
}
