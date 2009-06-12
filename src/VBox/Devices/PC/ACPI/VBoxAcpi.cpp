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

#else
static int patchAml(PPDMDEVINS pDevIns, uint8_t* pAml, size_t uAmlLen)
{
    uint16_t cNumCpus;
    int rc;
    
    rc = CFGMR3QueryU16Def(pDevIns->pCfgHandle, "NumCPUs", &cNumCpus, 1);
   
    if (RT_FAILURE(rc))
        return rc;
    
    /** 
     * Now search AML for:
     *  AML_PROCESSOR_OP            (UINT16) 0x5b83
     * and replace whole block with
     *  AML_NOOP_OP                 (UINT16) 0xa3 
     * for VCPU not configured
     */
    uint16_t cAcpiCpus = 0;
    for (uint32_t i = 0; i < uAmlLen - 5; i++)
    {
        /*
         * AML_PROCESSOR_OP 
         *
         * DefProcessor := ProcessorOp PkgLength NameString ProcID 
                             PblkAddr PblkLen ObjectList
         * ProcessorOp  := ExtOpPrefix 0x83
         * ProcID       := ByteData
         * PblkAddr     := DwordData
         * PblkLen      := ByteData
         */
        if ((pAml[i] == 0x5b) && (pAml[i+1] == 0x83))
        {
            if ((pAml[i+3] != 'C') || (pAml[i+4] != 'P'))
                /* false alarm, not named starting CP */
                continue;
            
            /* Maybe use ProcID instead? */
            cAcpiCpus++;
            if (cAcpiCpus <= cNumCpus)
                continue;

            /* Will fill unwanted CPU block with NOOPs */
            /* 
             * See 18.2.4 Package Length Encoding in ACPI spec 
             * for full format 
             */
            uint32_t cBytes = pAml[i + 2];
            AssertReleaseMsg((cBytes >> 6) == 0, 
                             ("So far, we only understand simple package length"));
                
            /* including AML_PROCESSOR_OP itself */
            for (uint32_t j = 0; j < cBytes + 2; j++)
                pAml[i+j] = 0xa3;

            /* Can increase i by cBytes + 1, but not really worth it */
        }
    }

    /* now recompute checksum, whole file byte sum must be 0 */
    pAml[9] = 0;
    uint8_t         aSum = 0;
    for (uint32_t i = 0; i < uAmlLen; i++)
      aSum = aSum + (uint8_t)pAml[i];
    pAml[9] = (uint8_t) (0 - aSum);
    
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
