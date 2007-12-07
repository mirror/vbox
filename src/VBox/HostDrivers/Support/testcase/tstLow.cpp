/** @file
 *
 * VBox host drivers - Ring-0 support drivers - Testcases:
 * Test allocating physical memory below 4G
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/sup.h>
#include <VBox/param.h>
#include <VBox/err.h>
#include <iprt/runtime.h>
#include <iprt/stream.h>

#include <string.h>

int main(int argc, char **argv)
{
    int rc;
    int rcRet = 0;

    RTR3Init(false);
    RTPrintf("tstLow: TESTING...\n");

    rc = SUPInit();
    if (VBOX_SUCCESS(rc))
    {
        /*
         * Allocate a bit of contiguous memory.
         */
        SUPPAGE aPages0[128];
        void *pvPages0 = (void *)0x77777777;
        memset(&aPages0[0], 0x8f, sizeof(aPages0));
        rc = SUPLowAlloc(ELEMENTS(aPages0), &pvPages0, NULL, aPages0);
        if (VBOX_SUCCESS(rc))
        {
            /* check that the pages are below 4GB and valid. */
            for (unsigned iPage = 0; iPage < ELEMENTS(aPages0); iPage++)
            {
                RTPrintf("%-4d: Phys=%VHp Reserved=%p\n", iPage, aPages0[iPage].Phys, aPages0[iPage].uReserved);
                if (aPages0[iPage].uReserved != 0)
                {
                    rcRet++;
                    RTPrintf("tstLow: error: aPages0[%d].uReserved=%#x expected 0!\n", iPage, aPages0[iPage].uReserved);
                }
                if (    aPages0[iPage].Phys >= _4G
                    ||  (aPages0[iPage].Phys & PAGE_OFFSET_MASK))
                {
                    rcRet++;
                    RTPrintf("tstLow: error: aPages0[%d].Phys=%VHp!\n", iPage, aPages0[iPage].Phys);
                }
            }
            if (!rcRet)
            {
                for (unsigned iPage = 0; iPage < ELEMENTS(aPages0); iPage++)
                    memset((char *)pvPages0 + iPage * PAGE_SIZE, iPage, PAGE_SIZE);
                for (unsigned iPage = 0; iPage < ELEMENTS(aPages0); iPage++)
                    for (uint8_t *pu8 = (uint8_t *)pvPages0 + iPage * PAGE_SIZE, *pu8End = pu8 + PAGE_SIZE; pu8 < pu8End; pu8++)
                        if (*pu8 != (uint8_t)iPage)
                        {
                            RTPrintf("tstLow: error: invalid page content %02x != %02x. iPage=%p off=%#x\n",
                                     *pu8, (uint8_t)iPage, iPage, (uintptr_t)pu8 & PAGE_OFFSET_MASK);
                            rcRet++;
                        }
            }
            SUPLowFree(pvPages0, ELEMENTS(aPages0));
        }
        else
        {
            RTPrintf("SUPLowAlloc(%d,,) failed -> rc=%Vrc\n", ELEMENTS(aPages0), rc);
            rcRet++;
        }

        /*
         * Allocate odd amounts in from 1 to 127.
         */
        for (unsigned cPages = 1; cPages <= 127; cPages++)
        {
            SUPPAGE aPages1[128];
            void *pvPages1 = (void *)0x77777777;
            memset(&aPages1[0], 0x8f, sizeof(aPages1));
            rc = SUPLowAlloc(cPages, &pvPages1, NULL, aPages1);
            if (VBOX_SUCCESS(rc))
            {
                /* check that the pages are below 4GB and valid. */
                for (unsigned iPage = 0; iPage < cPages; iPage++)
                {
                    RTPrintf("%-4d::%-4d: Phys=%VHp Reserved=%p\n", cPages, iPage, aPages1[iPage].Phys, aPages1[iPage].uReserved);
                    if (aPages1[iPage].uReserved != 0)
                    {
                        rcRet++;
                        RTPrintf("tstLow: error: aPages1[%d].uReserved=%#x expected 0!\n", iPage, aPages1[iPage].uReserved);
                    }
                    if (    aPages1[iPage].Phys >= _4G
                        ||  (aPages1[iPage].Phys & PAGE_OFFSET_MASK))
                    {
                        rcRet++;
                        RTPrintf("tstLow: error: aPages1[%d].Phys=%VHp!\n", iPage, aPages1[iPage].Phys);
                    }
                }
                if (!rcRet)
                {
                    for (unsigned iPage = 0; iPage < cPages; iPage++)
                        memset((char *)pvPages1 + iPage * PAGE_SIZE, iPage, PAGE_SIZE);
                    for (unsigned iPage = 0; iPage < cPages; iPage++)
                        for (uint8_t *pu8 = (uint8_t *)pvPages1 + iPage * PAGE_SIZE, *pu8End = pu8 + PAGE_SIZE; pu8 < pu8End; pu8++)
                            if (*pu8 != (uint8_t)iPage)
                            {
                                RTPrintf("tstLow: error: invalid page content %02x != %02x. iPage=%p off=%#x\n",
                                         *pu8, (uint8_t)iPage, iPage, (uintptr_t)pu8 & PAGE_OFFSET_MASK);
                                rcRet++;
                            }
                }
                SUPLowFree(pvPages1, cPages);
            }
            else
            {
                RTPrintf("SUPLowAlloc(%d,,) failed -> rc=%Vrc\n", cPages, rc);
                rcRet++;
            }
        }

    }
    else
    {
        RTPrintf("SUPInit -> rc=%Vrc\n", rc);
        rcRet++;
    }


    return rcRet;
}
