/* $Id$ */
/** @file
 * BS3Kit - Initialize any high DLLs, real mode.
 */

/*
 * Copyright (C) 2007-2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL), a copy of it is provided in the "COPYING.CDDL" file included
 * in the VirtualBox distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
//#define BS3_USE_RM_TEXT_SEG 1
#include "bs3kit-template-header.h"
#include "bs3kit-linker.h"
#include "bs3-cmn-memory.h"


extern BS3HIGHDLLENTRY BS3_FAR_DATA BS3_DATA_NM(g_aBs3HighDllTable)[];
extern BS3HIGHDLLENTRY BS3_FAR_DATA BS3_DATA_NM(g_Bs3HighDllTable_End);


BS3_DECL_FAR(void) Bs3InitHighDlls_rm_far(void)
{
    unsigned const cHighDlls = (unsigned)(&g_Bs3HighDllTable_End - &g_aBs3HighDllTable[0]);
    Bs3TestPrintf("cHighDlls=%d g_uBs3CpuDetected=%#x Bs3InitHighDlls_rm_far=%p\n", cHighDlls, g_uBs3CpuDetected, &Bs3InitHighDlls_rm_far);

    if (   cHighDlls > 0
        && (g_uBs3CpuDetected & BS3CPU_TYPE_MASK) >= BS3CPU_80386)
    {
        /*
         * Get the drive geometry. ASSUMES the bootsector hasn't been trashed yet!
         */
        uint8_t const bDrive       = ((BS3BOOTSECTOR RT_FAR *)BS3_FP_MAKE(0,0x7c00))->bBootDrv;
        uint16_t      uMaxCylinder = 0;
        uint8_t       uMaxHead     = 0;
        uint8_t       uMaxSector   = 0;
        int rc = Bs3DiskQueryGeometry_rm(bDrive, &uMaxCylinder, &uMaxHead, &uMaxSector);
        Bs3TestPrintf("Bs3DiskQueryGeometry(%#x)-> C=%#x H=%#x S=%#x\n", bDrive, uMaxCylinder, uMaxHead, uMaxSector);
        if (rc == 0)
        {
            uint16_t const cSectorsPerCylinder = uMaxSector * ((uint16_t)uMaxHead + 1);
            unsigned i;

            /*
             * We need a buffer first of all. Using a 4K / PAGE_SIZE buffer let us
             * share calculations and variables with the allocation code.
             */
            uint32_t         uFlatBuf;
            uint8_t          cBufSectors;
            uint16_t         cbBuf = 12*_1K; /* This is typically enough for a track (512*18 = 0x2400 (9216)). */
            uint8_t BS3_FAR *pbBuf = (uint8_t BS3_FAR *)Bs3MemAlloc(BS3MEMKIND_REAL, cbBuf);
            if (!pbBuf)
            {
                cbBuf = _4K;
                pbBuf = (uint8_t BS3_FAR *)Bs3MemAlloc(BS3MEMKIND_REAL, cbBuf);
                if (!pbBuf)
                {
                    Bs3TestPrintf("Failed to allocate 4 KiB memory buffer for loading high DLL(s)!\n");
                    Bs3Shutdown();
                }
            }
            cBufSectors = (uint8_t)(cbBuf / 512);
            uFlatBuf    = BS3_FP_REAL_TO_FLAT(pbBuf);

            /*
             * Iterate the high DLL table and load it all into memory.
             */
            for (i = 0; i < cHighDlls; i++)
            {
                const char RT_FAR * const pszzStrings  = (char RT_FAR *)&g_aBs3HighDllTable[i] + g_aBs3HighDllTable[i].offStrings;
                const char RT_FAR * const pszFilename  = &pszzStrings[g_aBs3HighDllTable[i].offFilename];
                uint16_t const            cPagesToLoad = (uint16_t)(g_aBs3HighDllTable[i].cbLoaded / _4K);
                uint16_t                  iPage;
                Bs3Printf("Loading dll '%s' at %#RX32 ...", pszFilename, g_aBs3HighDllTable[i].uLoadAddr);

                /*
                 * Allocate the memory taken by the DLL.
                 */
                iPage = Bs3SlabAllocFixed(&g_Bs3Mem4KUpperTiled.Core, g_aBs3HighDllTable[i].uLoadAddr, cPagesToLoad);
                if (iPage == 0 || iPage == UINT16_MAX)
                {
                    Bs3TestPrintf("Bs3SlabAllocFixed(,%#RX32, %#RX16) failed: %#RX16 (%s)\n",
                                  g_aBs3HighDllTable[i].uLoadAddr, cPagesToLoad, iPage, pszFilename);
                    Bs3Shutdown();
                }
                /** @todo We don't have any memory management above 16MB... */

                /*
                 * Load the DLL. This is where we ASSUME real-mode, pre-PIC setup,
                 * and interrupts enabled as we need to use int13 for the actual
                 * reading. We switch to 32-bit protected mode and copies the
                 * chunk from pbBuf and to the actual load address.
                 *
                 * Note! When reading we must make sure to not switch head as the
                 *       BIOS code messes up the result if it does the wraparound.
                 */
                {

                    /* Load the image. */
                    {
#ifdef BS3_WITH_LOAD_CHECKSUMS
# if 0 /* for debugging */
                        uint32_t       iCurSector         = 0;
# endif
                        uint32_t       uChecksum          = BS3_CALC_CHECKSUM_INITIAL_VALUE;
#endif
                        uint32_t       cSectorsLeftToLoad = g_aBs3HighDllTable[i].cbInImage  / 512;
                        uint32_t       uCurFlatLoadAddr   = g_aBs3HighDllTable[i].uLoadAddr;
                        /* Calculate the current CHS position: */
                        uint32_t const uCurSectorInImage  = g_aBs3HighDllTable[i].offInImage / 512;
                        uint16_t       uCylinder          = uCurSectorInImage / cSectorsPerCylinder;
                        uint16_t const uRemainder         = uCurSectorInImage % cSectorsPerCylinder;
                        uint8_t        uHead              = uRemainder / uMaxSector;
                        uint8_t        uSector            = (uRemainder % uMaxSector) + 1;
                        while (cSectorsLeftToLoad > 0)
                        {
                            /* Figure out how much we dare read.  Only up to the end of the track. */
                            uint8_t cSectors = uMaxSector + 1 - uSector;
                            if (cSectors > cBufSectors)
                                cSectors = cBufSectors;
                            if (cSectorsLeftToLoad < cSectors)
                                cSectors = (uint8_t)(cSectorsLeftToLoad);

                            Bs3TestPrintf("Calling Bs3DiskRead(%#x,%#x,%#x,%#x,%#x,%p) [uCurFlatLoadAddr=%RX32]\n",
                                          bDrive, uCylinder, uHead, uSector, cSectors, pbBuf, uCurFlatLoadAddr);
                            rc = Bs3DiskRead_rm(bDrive, uCylinder, uHead, uSector, cSectors, pbBuf);
                            if (rc != 0)
                            {
                                Bs3TestPrintf("Bs3DiskRead(%#x,%#x,%#x,%#x,%#x,) failed: %#x\n",
                                              bDrive, uCylinder, uHead, uSector, cBufSectors, rc);
                                Bs3Shutdown();
                            }

#ifdef BS3_WITH_LOAD_CHECKSUMS
                            /* Checksum what we just loaded. */
# if 0 /* For debugging. */
                            {
                                uint16_t j;
                                uint32_t uChecksumTmp = uChecksum;
                                for (j = 0; j < cSectors; j++, iCurSector++)
                                    Bs3TestPrintf("sector #%RU32:  %#RX32 %#010RX32\n", iCurSector,
                                                  uChecksumTmp = Bs3CalcChecksum(uChecksumTmp, &pbBuf[j * 512], 512),
                                                  Bs3CalcChecksum(BS3_CALC_CHECKSUM_INITIAL_VALUE, &pbBuf[j * 512], 512));
                                uChecksum = Bs3CalcChecksum(uChecksum, pbBuf, 512 * cSectors);
                                if (uChecksum != uChecksumTmp)
                                    Bs3TestPrintf("Checksum error: %#RX32, expected %#RX32!\n", uChecksum, uChecksumTmp);
                            }
# else
                            uChecksum = Bs3CalcChecksum(uChecksum, pbBuf, 512 * cSectors);
# endif
#endif

                            /* Copy the page to where the DLL is being loaded.  */
                            Bs3MemCopyFlat_rm_far(uCurFlatLoadAddr, uFlatBuf, 512 * cSectors);
                            Bs3PrintChr('.');

                            /* Advance */
                            uCurFlatLoadAddr   += cSectors * 512;
                            cSectorsLeftToLoad -= cSectors;
                            uSector            += cSectors;
                            if (!uSector || uSector > uMaxSector)
                            {
                                uSector        = 1;
                                uHead++;
                                if (uHead > uMaxHead)
                                {
                                    uHead      = 0;
                                    uCylinder++;
                                }
                            }
                        }

#ifdef BS3_WITH_LOAD_CHECKSUMS
                        /* Verify the checksum. */
                        if (uChecksum != g_aBs3HighDllTable[i].uChecksum)
                        {
                            Bs3TestPrintf("Checksum mismatch for '%s': %#RX32 vs %#RX32\n",
                                          pszFilename, uChecksum, g_aBs3HighDllTable[i].uChecksum);
                            Bs3Shutdown();
                        }
#endif
                    }
                }
            }

            Bs3Printf("\n");
            Bs3MemFree(pbBuf, cbBuf);
        }
        else
        {
            Bs3TestPrintf("Bs3DiskQueryGeometry(%#x) failed: %#x\n", bDrive, rc);
            Bs3Shutdown();
        }
    }
}

