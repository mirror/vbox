/** @file
 *
 * VBox host drivers - Ring-0 support drivers - Testcases:
 * Test the memory locking interface
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/sup.h>
#include <VBox/param.h>
#include <iprt/runtime.h>
#include <iprt/stream.h>
#include <stdlib.h>
#include <string.h>


int main(int argc, char **argv)
{
    int         rc;
    int         rcRet = 0;
    RTHCPHYS    HCPhys;
    void       *pv;

    RTR3Init(true, ~0);
    rc = SUPInit(NULL, ~0);
    RTPrintf("SUPInit -> rc=%d\n", rc);
    rcRet += rc != 0;
    if (!rc)
    {
        static struct
        {
            void       *pv;
            void       *pvAligned;
            SUPPAGE     aPages[16];
        } aPinnings[500];
        for (unsigned i = 0; i < sizeof(aPinnings) / sizeof(aPinnings[0]); i++)
        {
#ifdef __OS2__
            aPinnings[i].pv = NULL;
            SUPPageAlloc(0x10000 >> PAGE_SHIFT, &aPinnings[i].pv);
#else
            aPinnings[i].pv = malloc(0x10000);
#endif
            aPinnings[i].pvAligned = ALIGNP(aPinnings[i].pv, PAGE_SIZE);
            rc = SUPPageLock(aPinnings[i].pvAligned, 0xf000 >> PAGE_SHIFT, &aPinnings[i].aPages[0]);
            if (!rc)
            {
                RTPrintf("i=%d: pvAligned=%p pv=%p:\n", i, aPinnings[i].pvAligned, aPinnings[i].pv);
                memset(aPinnings[i].pv, 0xfa, 0x10000);
                unsigned c4GPluss = 0;
                for (unsigned j = 0; j < (0xf000 >> PAGE_SHIFT); j++)
                    if (aPinnings[i].aPages[j].Phys >= _4G)
                    {
                        RTPrintf("%2d: vrt=%p phys=%VHp\n", j, (char *)aPinnings[i].pvAligned + (j << PAGE_SHIFT), aPinnings[i].aPages[j].Phys);
                        c4GPluss++;
                    }
                RTPrintf("i=%d: c4GPluss=%d\n", i, c4GPluss);
            }
            else
            {
                RTPrintf("SUPPageLock -> rc=%d\n", rc);
                rcRet++;
#ifdef __OS2__
                SUPPageFree(aPinnings[i].pv, 0x10000 >> PAGE_SHIFT);
#else
                free(aPinnings[i].pv);
#endif
                aPinnings[i].pv = aPinnings[i].pvAligned = NULL;
                break;
            }
        }

        for (unsigned i = 0; i < sizeof(aPinnings) / sizeof(aPinnings[0]); i += 2)
        {
            if (aPinnings[i].pvAligned)
            {
                rc = SUPPageUnlock(aPinnings[i].pvAligned);
                if (rc)
                {
                    RTPrintf("SUPPageUnlock(%p) -> rc=%d\n", aPinnings[i].pvAligned, rc);
                    rcRet++;
                }
                memset(aPinnings[i].pv, 0xaf, 0x10000);
            }
        }

        for (unsigned i = 0; i < sizeof(aPinnings) / sizeof(aPinnings[0]); i += 2)
        {
            if (aPinnings[i].pv)
            {
                memset(aPinnings[i].pv, 0xcc, 0x10000);
#ifdef __OS2__
                SUPPageFree(aPinnings[i].pv, 0x10000 >> PAGE_SHIFT);
#else
                free(aPinnings[i].pv);
#endif
                aPinnings[i].pv = NULL;
            }
        }


        /*
         * Allocate a bit of contiguous memory.
         */
        pv = SUPContAlloc(RT_ALIGN_Z(15003, PAGE_SIZE) >> PAGE_SHIFT, &HCPhys);
        rcRet += pv == NULL || HCPhys == 0;
        if (pv && HCPhys)
        {
            RTPrintf("SUPContAlloc(15003) -> HCPhys=%llx pv=%p\n", HCPhys, pv);
            void *pv0 = pv;
            memset(pv0, 0xaf, 15003);
            pv = SUPContAlloc(RT_ALIGN_Z(12999, PAGE_SIZE) >> PAGE_SHIFT, &HCPhys);
            rcRet += pv == NULL || HCPhys == 0;
            if (pv && HCPhys)
            {
                RTPrintf("SUPContAlloc(12999) -> HCPhys=%llx pv=%p\n", HCPhys, pv);
                memset(pv, 0xbf, 12999);
                rc = SUPContFree(pv, RT_ALIGN_Z(12999, PAGE_SIZE) >> PAGE_SHIFT);
                rcRet += rc != 0;
                if (rc)
                    RTPrintf("SUPContFree failed! rc=%d\n", rc);
            }
            else
                RTPrintf("SUPContAlloc (2nd) failed!\n");
            memset(pv0, 0xaf, 15003);
            /* pv0 is intentionally not freed! */
        }
        else
            RTPrintf("SUPContAlloc failed!\n");

        /*
         * Allocate a big chunk of virtual memory and then lock it.
         */
        #define BIG_SIZE    72*1024*1024
        #define BIG_SIZEPP  (BIG_SIZE + PAGE_SIZE)
#ifdef __OS2__
        pv = NULL;
        SUPPageAlloc(BIG_SIZEPP >> PAGE_SHIFT, &pv);
#else
        pv = malloc(BIG_SIZEPP);
#endif
        if (pv)
        {
            static SUPPAGE      aPages[BIG_SIZE >> PAGE_SHIFT];
            void *pvAligned = RT_ALIGN_P(pv, PAGE_SIZE);
            rc = SUPPageLock(pvAligned, BIG_SIZE >> PAGE_SHIFT, &aPages[0]);
            if (!rc)
            {
                /* dump */
                RTPrintf("SUPPageLock(%p,%d,) succeeded!\n", pvAligned, BIG_SIZE);
                memset(pv, 0x42, BIG_SIZEPP);
                #if 0
                for (unsigned j = 0; j < (BIG_SIZE >> PAGE_SHIFT); j++)
                    RTPrintf("%2d: vrt=%p phys=%08x\n", j, (char *)pvAligned + (j << PAGE_SHIFT), (uintptr_t)aPages[j].pvPhys);
                #endif

                /* unlock */
                rc = SUPPageUnlock(pvAligned);
                if (rc)
                {
                    RTPrintf("SUPPageUnlock(%p) -> rc=%d\n", pvAligned, rc);
                    rcRet++;
                }
                memset(pv, 0xcc, BIG_SIZEPP);
            }
            else
            {
                RTPrintf("SUPPageLock(%p) -> rc=%d\n", pvAligned, rc);
                rcRet++;
            }
#ifdef __OS2__
            SUPPageFree(pv, BIG_SIZEPP >> PAGE_SHIFT);
#else
            free(pv);
#endif
        }

        rc = SUPTerm();
        RTPrintf("SUPTerm -> rc=%d\n", rc);
        rcRet += rc != 0;
    }

    return rcRet;
}
