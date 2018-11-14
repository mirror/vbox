/* $Id$ */
/** @file
 * CPUM - SVM.
 */

/*
 * Copyright (C) 2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_CPUM
#include <VBox/vmm/cpum.h>
#include <VBox/log.h>


/**
 * Gets the MSR permission bitmap byte and bit offset for the specified MSR.
 *
 * @returns VBox status code.
 * @param   idMsr       The MSR being requested.
 * @param   pbOffMsrpm  Where to store the byte offset in the MSR permission
 *                      bitmap for @a idMsr.
 * @param   puMsrpmBit  Where to store the bit offset starting at the byte
 *                      returned in @a pbOffMsrpm.
 */
VMM_INT_DECL(int) CPUMSvmGetMsrpmOffsetAndBit(uint32_t idMsr, uint16_t *pbOffMsrpm, uint8_t *puMsrpmBit)
{
    Assert(pbOffMsrpm);
    Assert(puMsrpmBit);

    /*
     * MSRPM Layout:
     * Byte offset          MSR range
     * 0x000  - 0x7ff       0x00000000 - 0x00001fff
     * 0x800  - 0xfff       0xc0000000 - 0xc0001fff
     * 0x1000 - 0x17ff      0xc0010000 - 0xc0011fff
     * 0x1800 - 0x1fff              Reserved
     *
     * Each MSR is represented by 2 permission bits (read and write).
     */
    if (idMsr <= 0x00001fff)
    {
        /* Pentium-compatible MSRs. */
        uint32_t const bitoffMsr = idMsr << 1;
        *pbOffMsrpm = bitoffMsr >> 3;
        *puMsrpmBit = bitoffMsr & 7;
        return VINF_SUCCESS;
    }

    if (   idMsr >= 0xc0000000
        && idMsr <= 0xc0001fff)
    {
        /* AMD Sixth Generation x86 Processor MSRs. */
        uint32_t const bitoffMsr = (idMsr - 0xc0000000) << 1;
        *pbOffMsrpm = 0x800 + (bitoffMsr >> 3);
        *puMsrpmBit = bitoffMsr & 7;
        return VINF_SUCCESS;
    }

    if (   idMsr >= 0xc0010000
        && idMsr <= 0xc0011fff)
    {
        /* AMD Seventh and Eighth Generation Processor MSRs. */
        uint32_t const bitoffMsr = (idMsr - 0xc0010000) << 1;
        *pbOffMsrpm = 0x1000 + (bitoffMsr >> 3);
        *puMsrpmBit = bitoffMsr & 7;
        return VINF_SUCCESS;
    }

    *pbOffMsrpm = 0;
    *puMsrpmBit = 0;
    return VERR_OUT_OF_RANGE;
}


/**
 * Determines whether an IOIO intercept is active for the nested-guest or not.
 *
 * @param   pvIoBitmap      Pointer to the nested-guest IO bitmap.
 * @param   u16Port         The IO port being accessed.
 * @param   enmIoType       The type of IO access.
 * @param   cbReg           The IO operand size in bytes.
 * @param   cAddrSizeBits   The address size bits (for 16, 32 or 64).
 * @param   iEffSeg         The effective segment number.
 * @param   fRep            Whether this is a repeating IO instruction (REP prefix).
 * @param   fStrIo          Whether this is a string IO instruction.
 * @param   pIoExitInfo     Pointer to the SVMIOIOEXITINFO struct to be filled.
 *                          Optional, can be NULL.
 */
VMM_INT_DECL(bool) CPUMSvmIsIOInterceptActive(void *pvIoBitmap, uint16_t u16Port, SVMIOIOTYPE enmIoType, uint8_t cbReg,
                                              uint8_t cAddrSizeBits, uint8_t iEffSeg, bool fRep, bool fStrIo,
                                              PSVMIOIOEXITINFO pIoExitInfo)
{
    Assert(cAddrSizeBits == 16 || cAddrSizeBits == 32 || cAddrSizeBits == 64);
    Assert(cbReg == 1 || cbReg == 2 || cbReg == 4 || cbReg == 8);

    /*
     * The IOPM layout:
     * Each bit represents one 8-bit port. That makes a total of 0..65535 bits or
     * two 4K pages.
     *
     * For IO instructions that access more than a single byte, the permission bits
     * for all bytes are checked; if any bit is set to 1, the IO access is intercepted.
     *
     * Since it's possible to do a 32-bit IO access at port 65534 (accessing 4 bytes),
     * we need 3 extra bits beyond the second 4K page.
     */
    static const uint16_t s_auSizeMasks[] = { 0, 1, 3, 0, 0xf, 0, 0, 0 };

    uint16_t const offIopm   = u16Port >> 3;
    uint16_t const fSizeMask = s_auSizeMasks[(cAddrSizeBits >> SVM_IOIO_OP_SIZE_SHIFT) & 7];
    uint8_t  const cShift    = u16Port - (offIopm << 3);
    uint16_t const fIopmMask = (1 << cShift) | (fSizeMask << cShift);

    uint8_t const *pbIopm = (uint8_t *)pvIoBitmap;
    Assert(pbIopm);
    pbIopm += offIopm;
    uint16_t const u16Iopm = *(uint16_t *)pbIopm;
    if (u16Iopm & fIopmMask)
    {
        if (pIoExitInfo)
        {
            static const uint32_t s_auIoOpSize[] =
            { SVM_IOIO_32_BIT_OP, SVM_IOIO_8_BIT_OP, SVM_IOIO_16_BIT_OP, 0, SVM_IOIO_32_BIT_OP, 0, 0, 0 };

            static const uint32_t s_auIoAddrSize[] =
            { 0, SVM_IOIO_16_BIT_ADDR, SVM_IOIO_32_BIT_ADDR, 0, SVM_IOIO_64_BIT_ADDR, 0, 0, 0 };

            pIoExitInfo->u         = s_auIoOpSize[cbReg & 7];
            pIoExitInfo->u        |= s_auIoAddrSize[(cAddrSizeBits >> 4) & 7];
            pIoExitInfo->n.u1Str   = fStrIo;
            pIoExitInfo->n.u1Rep   = fRep;
            pIoExitInfo->n.u3Seg   = iEffSeg & 7;
            pIoExitInfo->n.u1Type  = enmIoType;
            pIoExitInfo->n.u16Port = u16Port;
        }
        return true;
    }

    /** @todo remove later (for debugging as VirtualBox always traps all IO
     *        intercepts). */
    AssertMsgFailed(("CPUMSvmIsIOInterceptActive: We expect an IO intercept here!\n"));
    return false;
}

