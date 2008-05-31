/* $Id$ */
/** @file
 * VBox Disassembler - Helper for formatting the opcode bytes.
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "DisasmInternal.h"
#include <iprt/string.h>
#include <iprt/assert.h>
#include <iprt/err.h>


/**
 * Helper function for formatting the opcode bytes.
 *
 * @returns The number of output bytes.
 *
 * @param   pCpu    Pointer to the disassembler cpu state.
 * @param   pszDst  The output buffer.
 * @param   cchDst  The size of the output buffer.
 * @param   fFlags  The flags passed to the formatter.
 */
size_t disFormatBytes(PCDISCPUSTATE pCpu, char *pszDst, size_t cchDst, uint32_t fFlags)
{
    /*
     * Read the bytes first.
     */
    uint8_t ab[16];
    size_t  cb = pCpu->opsize;
    Assert(cb <= 16);
    if (cb > 16)
        cb = 16;

    if (pCpu->pfnReadBytes)
    {
        int rc = pCpu->pfnReadBytes(pCpu->opaddr, &ab[0], cb, (void *)pCpu);
        if (RT_FAILURE(rc))
        {
            for (size_t i = 0; i < cb; i++)
            {
                int rc2 = pCpu->pfnReadBytes(pCpu->opaddr + i, &ab[i], 1, (void *)pCpu);
                if (RT_FAILURE(rc2))
                    ab[i] = 0xcc;
            }
        }
    }
    else
    {
        uint8_t const *pabSrc = (uint8_t const *)(uintptr_t)pCpu->opaddr;
        for (size_t i = 0; i < cb; i++)
            ab[i] = pabSrc[i];
    }

    /*
     * Now for the output.
     */
    size_t cchOutput = 0;
#define PUT_C(ch) \
            do { \
                cchOutput++; \
                if (cchDst > 1) \
                { \
                    cchDst--; \
                    *pszDst++ = (ch); \
                } \
            } while (0)
#define PUT_NUM(cch, fmt, num) \
            do { \
                 cchOutput += (cch); \
                 if (cchDst > 1) \
                 { \
                    const size_t cchTmp = RTStrPrintf(pszDst, cchDst, fmt, (num)); \
                    pszDst += cchTmp; \
                    cchDst -= cchTmp; \
                 } \
            } while (0)


    if (fFlags & DIS_FMT_FLAGS_BYTES_BRACKETS)
        PUT_C('[');

    for (size_t i = 0; i < cb; i++)
    {
        if (i != 0 && (fFlags & DIS_FMT_FLAGS_BYTES_SPACED))
            PUT_NUM(3, " %02x", ab[i]);
        else
            PUT_NUM(2, "%02x", ab[i]);
    }

    if (fFlags & DIS_FMT_FLAGS_BYTES_BRACKETS)
        PUT_C(']');

    /* Terminate it just in case. */
    if (cchDst >= 1)
        *pszDst = '\0';

#undef PUT_C
#undef PUT_NUM
    return cchOutput;
}

