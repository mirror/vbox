/* $Id$ */
/** @file
 * DBGC - Debugger Console, Native Commands.
 */

/*
 * Copyright (C) 2006-2023 Oracle and/or its affiliates.
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
 * SPDX-License-Identifier: GPL-3.0-only
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DBGC
#include <VBox/dbg.h>
#include <VBox/vmm/dbgf.h>
#include <VBox/param.h>
#include <iprt/errcore.h>
#include <VBox/log.h>

#include <iprt/assert.h>
#include <iprt/ctype.h>
#include <iprt/dir.h>
#include <iprt/env.h>
#include <iprt/ldr.h>
#include <iprt/mem.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/time.h>
#ifdef DBGC_DUMP_IMAGE_TOOL
# include <iprt/buildconfig.h>
# include <iprt/message.h>
# include <iprt/file.h>
# include <iprt/getopt.h>
# include <iprt/initterm.h>
# include <iprt/process.h>
# include <iprt/stream.h>
# include <iprt/vfs.h>
#endif
#include <iprt/formats/mz.h>
#include <iprt/formats/pecoff.h>
#include <iprt/formats/elf32.h>
#include <iprt/formats/elf64.h>
#include <iprt/formats/codeview.h>
#include <iprt/formats/mach-o.h>

#include "DBGCInternal.h"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
#ifdef DBGC_DUMP_IMAGE_TOOL
/** Command helper state for the image dumper tool. */
typedef struct CMDHLPSTATE
{
    DBGCCMDHLP  Core;
    /** The exit code for the tool. */
    RTEXITCODE  rcExit;
    /** The current input file. */
    RTVFSFILE   hVfsFile;
} CMDHLPSTATE;
typedef CMDHLPSTATE *PCMDHLPSTATE;
#endif


/** Helper for translating flags. */
typedef struct
{
    uint32_t        fFlag;
    const char     *pszNm;
} DBGCDUMPFLAGENTRY;
#define FLENT(a_Define) { a_Define, #a_Define }


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
#ifndef DBGC_DUMP_IMAGE_TOOL
extern FNDBGCCMD dbgcCmdDumpImage; /* See DBGCCommands.cpp. */
#endif


/** Stringifies a 32-bit flag value. */
static void dbgcDumpImageFlags32(PDBGCCMDHLP pCmdHlp, uint32_t fFlags, DBGCDUMPFLAGENTRY const *paEntries, size_t cEntries)
{
    for (size_t i = 0; i < cEntries; i++)
        if (fFlags & paEntries[i].fFlag)
            DBGCCmdHlpPrintf(pCmdHlp, " %s", paEntries[i].pszNm);
}


/**
 * Early read function.
 *
 * @todo refactor/abstract this somehow...
 */
static int dumpReadAt(PDBGCCMDHLP pCmdHlp, PCDBGCCMD pCmd, PCDBGCVAR pImageBase, const char *pszName,
                      size_t off, void *pvDst, size_t cbToRead, size_t *pcbRead)
{
    RT_BZERO(pvDst, cbToRead);
    if (pcbRead)
        *pcbRead = 0;

#ifndef DBGC_DUMP_IMAGE_TOOL
    DBGCVAR AddrToReadAt;
    int rc = DBGCCmdHlpEval(pCmdHlp, &AddrToReadAt, "%DV + %#zx", pImageBase, off);
    if (RT_SUCCESS(rc))
    {
        rc = DBGCCmdHlpMemRead(pCmdHlp, pvDst, cbToRead, &AddrToReadAt, pcbRead);
        if (RT_SUCCESS(rc))
            return VINF_SUCCESS;
        return DBGCCmdHlpFailRc(pCmdHlp, pCmd, rc, "%s: Failed to read %zu bytes at offset %Dv", pszName, cbToRead, &AddrToReadAt);
    }
    return DBGCCmdHlpFailRc(pCmdHlp, pCmd, rc, "%s: Failed to calculate address %Dv + #%zx for %#zx byte read",
                            pszName, pImageBase, off, cbToRead);

#else  /* DBGC_DUMP_IMAGE_TOOL */
    CMDHLPSTATE * const pToolState = RT_FROM_MEMBER(pCmdHlp, CMDHLPSTATE, Core);
    int rc = RTVfsFileReadAt(pToolState->hVfsFile, off, pvDst, cbToRead, pcbRead);
    if (RT_SUCCESS(rc))
        return VINF_SUCCESS;
    RT_NOREF(pImageBase);
    return DBGCCmdHlpFailRc(pCmdHlp, pCmd, rc, "%s: Failed to read %zu bytes at offset %#zx", pszName, cbToRead, off);
#endif /* DBGC_DUMP_IMAGE_TOOL */
}


/*********************************************************************************************************************************
*   DumpImageBase                                                                                                                *
*********************************************************************************************************************************/
class DumpImageBase
{
public:
    /** The name of what's being dumped (for error messages). */
    const char * const  m_pszName;
    /** Pointer to the image base address variable. */
    PCDBGCVAR const     m_pImageBase;
    /** Pointer to the command helpers. */
    PDBGCCMDHLP const   m_pCmdHlp;
    /** The command descriptor (for failing the command). */
    PCDBGCCMD const     m_pCmd;

private:
    /** The Image base address. */
    uint64_t            m_uImageBaseAddr;
protected:
    /** The full formatted address width. */
    uint8_t             m_cchAddr;
private:
    /** The formatted address value width. */
    uint8_t             m_cchAddrValue;
    /** The address prefix length.   */
    uint8_t             m_cchAddrPfx;
    /** The address prefix.   */
    char                m_szAddrPfx[16 - 3];

private:
    DumpImageBase();

    void setupAddrFormatting(const char *a_pszImageBaseAddr)
    {
        /*
         * Expected inputs: %%12345678, %123456789abcdef, 0x12345678, 0008:12345678
         *
         * So, work backwards till be find the start of the address/offset value
         * component, and treat what comes first as a prefix.
         */
        size_t const cch        = strlen(a_pszImageBaseAddr);
        size_t       cchAddrPfx = cch;
        while (cchAddrPfx > 0 && RT_C_IS_XDIGIT(a_pszImageBaseAddr[cchAddrPfx - 1]))
            cchAddrPfx--;

        size_t cchLeadingZeros = 0;
        while (a_pszImageBaseAddr[cchAddrPfx + cchLeadingZeros] == '0')
            cchLeadingZeros++;

        int rc = RTStrToUInt64Full(&a_pszImageBaseAddr[cchAddrPfx], 16, &m_uImageBaseAddr);
        AssertRCSuccess(rc);
        m_cchAddrValue = (uint8_t)(cch - cchAddrPfx);
        Assert(m_cchAddrValue == cch - cchAddrPfx);
        if (m_cchAddrValue > 8 && cchLeadingZeros > 1)
            m_cchAddrValue = RT_ALIGN_T(m_cchAddrValue - (uint8_t)(cchLeadingZeros - 1), 2, uint8_t);

        AssertStmt(cchAddrPfx < sizeof(m_szAddrPfx), cchAddrPfx = sizeof(m_szAddrPfx) - 1);
        memcpy(m_szAddrPfx, a_pszImageBaseAddr, cchAddrPfx);
        m_szAddrPfx[cchAddrPfx] = '\0';
        m_cchAddrPfx = (uint8_t)cchAddrPfx;

        m_cchAddr = m_cchAddrPfx + m_cchAddrValue;
    }

public:
    DumpImageBase(PDBGCCMDHLP a_pCmdHlp, PCDBGCCMD a_pCmd, PCDBGCVAR a_pImageBase,
                  const char *a_pszImageBaseAddr, const char *a_pszName)
        : m_pszName(a_pszName)
        , m_pImageBase(a_pImageBase)
        , m_pCmdHlp(a_pCmdHlp)
        , m_pCmd(a_pCmd)
        , m_uImageBaseAddr(0)
        , m_cchAddr(0)
        , m_cchAddrValue(12)
        , m_cchAddrPfx(2)
        , m_szAddrPfx("0x")
    {
        setupAddrFormatting(a_pszImageBaseAddr);
    }

    virtual ~DumpImageBase() { }

    virtual size_t rvaToFileOffset(size_t uRva) const = 0;
    virtual size_t getEndRva(bool a_fAligned = true) const = 0;

    char *rvaToStringWithAddr(size_t uRva, char *pszDst, size_t cbDst, bool fWide = false)
    {
        if (!fWide)
            RTStrPrintf(pszDst, cbDst, "%#09zx/%s%0*RX64", uRva, m_szAddrPfx, m_cchAddrValue, m_uImageBaseAddr + uRva);
        else
            RTStrPrintf(pszDst, cbDst, "%#09zx / %s%0*RX64", uRva, m_szAddrPfx, m_cchAddrValue, m_uImageBaseAddr + uRva);
        return pszDst;
    }

    virtual void myPrintf(const char *pszFormat, ...)
    {
        va_list va;
        va_start(va, pszFormat);
        m_pCmdHlp->pfnPrintfV(m_pCmdHlp, NULL, pszFormat, va);
        va_end(va);
    }

    void myPrintHeader(size_t uRva, const char *pszFormat, ...)
    {
        char    szTmp[64];
        char    szLine[128];
        va_list va;
        va_start(va, pszFormat);
        size_t const cchLine = RTStrPrintf(szLine, sizeof(szLine), "%s - %N",
                                           rvaToStringWithAddr(uRva, szTmp, sizeof(szTmp), true), pszFormat, &va);
        va_end(va);
        myPrintf("\n"
                 "%s\n"
                 "%.*s====\n",
                 szLine,
                 cchLine, "===============================================================================");
    }

    virtual int myError(const char *pszFormat, ...)
    {
        va_list va;
        va_start(va, pszFormat);
        int rc = DBGCCmdHlpFail(m_pCmdHlp, m_pCmd, "%s: %N", m_pszName, pszFormat, &va);
        va_end(va);
        return rc;
    }

    virtual int myError(int rc, const char *pszFormat, ...)
    {
        va_list va;
        va_start(va, pszFormat);
        rc = DBGCCmdHlpFailRc(m_pCmdHlp, m_pCmd, rc, "%s: %N", m_pszName, pszFormat, &va);
        va_end(va);
        return rc;
    }

    int readBytesAtRva(size_t uRva, void *pvBuf, size_t cbToRead, size_t *pcbRead = NULL)
    {
        /* Ensure buffer and return size is zero before we do anything. */
        if (pcbRead)
            *pcbRead = 0;
        RT_BZERO(pvBuf, cbToRead);

#ifndef DBGC_DUMP_IMAGE_TOOL
        /* Calc the read address. */
        DBGCVAR AddrToReadAt;
        int rc = DBGCCmdHlpEval(m_pCmdHlp, &AddrToReadAt, "%DV + %#zx", m_pImageBase, uRva);
        if (RT_SUCCESS(rc))
        {
            rc = DBGCCmdHlpMemRead(m_pCmdHlp, pvBuf, cbToRead, &AddrToReadAt, pcbRead);
            if (RT_FAILURE(rc))
                rc = myError(rc, "Failed to read %zu bytes at %Dv", cbToRead, &AddrToReadAt);
        }
#else  /* DBGC_DUMP_IMAGE_TOOL */
        int rc;
        size_t const offFile = rvaToFileOffset(uRva);
        if (offFile != ~(size_t)0)
        {
            CMDHLPSTATE * const pToolState = RT_FROM_MEMBER(m_pCmdHlp, CMDHLPSTATE, Core);
            rc = RTVfsFileReadAt(pToolState->hVfsFile, offFile, pvBuf, cbToRead, pcbRead);
            if (RT_FAILURE(rc))
                rc = myError(rc, "Failed to read %zu bytes at %#zx (RVA %#zx)", cbToRead, offFile, uRva);
        }
        else
            rc = myError(VERR_READ_ERROR, "Failed to convert RVA %#zx to file offset for %zu byte read!", uRva, cbToRead);
#endif /* DBGC_DUMP_IMAGE_TOOL */
        return rc;
    }
};


/**
 * Buffered reading by relative virtual address (RVA).
 */
class DumpImageBufferedReader
{
private:
    /** Static sized buffer. */
    uint8_t         m_abBufFixed[4096];
    /** Pointer to m_abBufFixed if that's sufficient, otherwise heap buffer. */
    uint8_t        *m_pbBuf;
    /** The size of the buffer m_pbBuf points at. */
    size_t          m_cbBufAlloc;
    /** Number of valid bytes in the buffer. */
    size_t          m_cbBuf;
    /** The RVA of the first buffer byte, maximum value if empty. */
    size_t          m_uRvaBuf;
    /** Pointer to the image dumper. */
    DumpImageBase  *m_pImage;

    int loadBuffer(size_t uRva)
    {
        /* Check that the RVA is within the image. */
        size_t const cbMaxRva = m_pImage->getEndRva();
        if (uRva >= cbMaxRva)
            return VERR_EOF;

        /* Adjust the RVA if we're reading beyond the end of the image. */
        if (uRva + m_cbBufAlloc > RT_ALIGN_Z(cbMaxRva, 8))
            uRva = m_cbBufAlloc > RT_ALIGN_Z(cbMaxRva, 8) ? RT_ALIGN_Z(cbMaxRva, 8) - m_cbBufAlloc : 0;

        /* Do the read.  In case of failure readBytesAtRva will zero the buffer. */
        m_uRvaBuf = uRva;
        m_cbBuf   = 0;
        return m_pImage->readBytesAtRva(uRva, m_pbBuf, RT_MIN(cbMaxRva - uRva, m_cbBufAlloc), &m_cbBuf);
    }

    /** Resizes the buffer if the current one can't hold @a cbNeeded bytes. */
    int ensureBufferSpace(size_t cbNeeded)
    {
        if (cbNeeded > m_cbBufAlloc)
        {
            cbNeeded = RT_ALIGN_Z(cbNeeded, 512);
            void *pvNew = RTMemTmpAllocZ(cbNeeded);
            if (!pvNew)
                return DBGCCmdHlpFailRc(m_pImage->m_pCmdHlp, m_pImage->m_pCmd, VERR_NO_TMP_MEMORY,
                                        "Failed to allocate %zu (%#zx) bytes", cbNeeded, cbNeeded);
            memcpy(pvNew, m_pbBuf, RT_MIN(m_cbBuf, m_cbBufAlloc));

            if (m_pbBuf != &m_abBufFixed[0])
                RTMemTmpFree(m_pbBuf);
            m_pbBuf      = (uint8_t *)pvNew;
            m_cbBufAlloc = cbNeeded;
        }
        return VINF_SUCCESS;
    }

    DumpImageBufferedReader();

public:
    DumpImageBufferedReader(DumpImageBase *a_pImage)
        : m_pbBuf(&m_abBufFixed[0])
        , m_cbBufAlloc(sizeof(m_abBufFixed))
        , m_cbBuf(0)
        , m_uRvaBuf(~(size_t)0)
        , m_pImage(a_pImage)
    {
        RT_ZERO(m_abBufFixed);
    }

    /** Copy constructor. */
    DumpImageBufferedReader(DumpImageBufferedReader const &a_rThat)
        : m_pbBuf(&m_abBufFixed[0])
        , m_cbBufAlloc(sizeof(m_abBufFixed))
        , m_cbBuf(RT_MIN(a_rThat.m_cbBuf, sizeof(m_abBufFixed)))
        , m_uRvaBuf(a_rThat.m_uRvaBuf)
        , m_pImage(a_rThat.m_pImage)
    {
        memcpy(m_abBufFixed, a_rThat.m_pbBuf, m_cbBuf);
        if (m_cbBuf < sizeof(m_abBufFixed))
            RT_BZERO(&m_abBufFixed[m_cbBuf], sizeof(m_abBufFixed) - m_cbBuf);
    }

    ~DumpImageBufferedReader()
    {
        if (m_pbBuf != &m_abBufFixed[0])
            RTMemTmpFree(m_pbBuf);
        m_pbBuf = NULL;
    }

    /**
     * Reads @a cbToRead bytes at @a uRva into @a pvDst.
     *
     * The buffer is entirely zeroed before reading anything, so it's okay to ignore
     * the status code.
     */
    int readBytes(size_t uRva, void *pvDst, size_t cbToRead)
    {
        RT_BZERO(pvDst, cbToRead);

        while (cbToRead)
        {
            /*
             * Is the start of the request overlapping with the buffer?
             */
            if (uRva >= m_uRvaBuf)
            {
                size_t const offBuf = uRva - m_uRvaBuf;
                if (offBuf < m_cbBuf)
                {
                    size_t const cbThisRead = RT_MIN(m_cbBuf - offBuf, cbToRead);
                    memcpy(pvDst, &m_pbBuf[offBuf], cbThisRead);
                    if (cbToRead <= cbThisRead)
                        return VINF_SUCCESS;
                    uRva     += cbThisRead;
                    cbToRead -= cbThisRead;
                    pvDst     = (uint8_t *)pvDst + cbThisRead;
                }
            }

            /*
             * Fill buffer.
             */
            int rc = loadBuffer(uRva);
            if (RT_FAILURE(rc))
                return rc;
        }
        return VINF_SUCCESS;
    }

    /**
     * Ensures @a cbItem at @a uRva is in the buffer and returns a pointer to it.
     *
     * The returned pointer is only valid till the next call to the reader instance.
     *
     * @returns NULL if failed to load the range into the buffer.
     * @note    Extra buffer space will be allocated if @a cbItem is larger than the
     *          internal buffer.
     */
    uint8_t const *bufferedBytes(size_t uRva, size_t cbItem)
    {
        /* Do we need to load the item into the buffer? */
        if (   uRva < m_uRvaBuf
            || uRva + cbItem > m_uRvaBuf + m_cbBuf)
        {
            int rc = ensureBufferSpace(cbItem);
            if (RT_SUCCESS(rc))
                rc = loadBuffer(uRva);
            if (RT_FAILURE(rc))
                return NULL;
        }

        Assert(uRva >= m_uRvaBuf && uRva + cbItem <= m_uRvaBuf + m_cbBuf);
        return &m_pbBuf[uRva - m_uRvaBuf];
    }

    /**
     * Gets a buffered zero terminated string at @a uRva.
     *
     * @note The implied max length is the size of the internal buffer.  No extra
     *       space will be allocated if the string doesn't terminate within the
     *       buffer size.
     */
    const char *bufferedString(size_t uRva)
    {
        /* Do we need to reload the buffer? */
        if (   uRva < m_uRvaBuf
            || uRva >= m_uRvaBuf + m_cbBuf
            || (   uRva != m_uRvaBuf
                && !memchr(&m_pbBuf[uRva - m_uRvaBuf], '\0', m_cbBufAlloc - (uRva - m_uRvaBuf))))
        {
            int rc = loadBuffer(uRva);
            AssertRCReturn(rc, NULL);
        }

        /* The RVA is within the buffer now, just check that the string ends
           before the end of the buffer. */
        Assert(uRva >= m_uRvaBuf && uRva < m_uRvaBuf + m_cbBuf);
        size_t const       offString = uRva - m_uRvaBuf;
        const char * const pszString = (const char *)&m_pbBuf[offString];
        AssertReturn(memchr(pszString, '\0', m_cbBufAlloc - offString), NULL);
        return pszString;
    }

    /**
     * Gets a simple integer value, with default in case of failure.
     */
    template<typename IntType>
    IntType bufferedInt(size_t uRva, IntType Default = 0)
    {
        AssertCompile(sizeof(IntType) <= 8);
        AssertReturn(uRva < uRva + sizeof(IntType), Default);

        /* Do we need to reload the buffer? */
        if (   uRva < m_uRvaBuf
            || uRva + sizeof(IntType) > m_uRvaBuf + m_cbBuf)
        {
            int rc = loadBuffer(uRva);
            AssertRCReturn(rc, Default);
        }

        /* The RVA is within the buffer now. */
        Assert(uRva >= m_uRvaBuf && uRva + sizeof(IntType) <= m_uRvaBuf + m_cbBuf);
        return *(IntType *)&m_pbBuf[uRva - m_uRvaBuf];
    }

};


/*********************************************************************************************************************************
*   PE                                                                                                                           *
*********************************************************************************************************************************/

static const char *dbgcPeMachineName(uint16_t uMachine)
{
    switch (uMachine)
    {
        case IMAGE_FILE_MACHINE_I386         : return "I386";
        case IMAGE_FILE_MACHINE_AMD64        : return "AMD64";
        case IMAGE_FILE_MACHINE_UNKNOWN      : return "UNKNOWN";
        case IMAGE_FILE_MACHINE_BASIC_16     : return "BASIC_16";
        case IMAGE_FILE_MACHINE_BASIC_16_TV  : return "BASIC_16_TV";
        case IMAGE_FILE_MACHINE_IAPX16       : return "IAPX16";
        case IMAGE_FILE_MACHINE_IAPX16_TV    : return "IAPX16_TV";
        //case IMAGE_FILE_MACHINE_IAPX20       : return "IAPX20";
        //case IMAGE_FILE_MACHINE_IAPX20_TV    : return "IAPX20_TV";
        case IMAGE_FILE_MACHINE_I8086        : return "I8086";
        case IMAGE_FILE_MACHINE_I8086_TV     : return "I8086_TV";
        case IMAGE_FILE_MACHINE_I286_SMALL   : return "I286_SMALL";
        case IMAGE_FILE_MACHINE_MC68         : return "MC68";
        //case IMAGE_FILE_MACHINE_MC68_WR      : return "MC68_WR";
        case IMAGE_FILE_MACHINE_MC68_TV      : return "MC68_TV";
        case IMAGE_FILE_MACHINE_MC68_PG      : return "MC68_PG";
        //case IMAGE_FILE_MACHINE_I286_LARGE   : return "I286_LARGE";
        case IMAGE_FILE_MACHINE_U370_WR      : return "U370_WR";
        case IMAGE_FILE_MACHINE_AMDAHL_470_WR: return "AMDAHL_470_WR";
        case IMAGE_FILE_MACHINE_AMDAHL_470_RO: return "AMDAHL_470_RO";
        case IMAGE_FILE_MACHINE_U370_RO      : return "U370_RO";
        case IMAGE_FILE_MACHINE_R4000        : return "R4000";
        case IMAGE_FILE_MACHINE_WCEMIPSV2    : return "WCEMIPSV2";
        case IMAGE_FILE_MACHINE_VAX_WR       : return "VAX_WR";
        case IMAGE_FILE_MACHINE_VAX_RO       : return "VAX_RO";
        case IMAGE_FILE_MACHINE_SH3          : return "SH3";
        case IMAGE_FILE_MACHINE_SH3DSP       : return "SH3DSP";
        case IMAGE_FILE_MACHINE_SH4          : return "SH4";
        case IMAGE_FILE_MACHINE_SH5          : return "SH5";
        case IMAGE_FILE_MACHINE_ARM          : return "ARM";
        case IMAGE_FILE_MACHINE_THUMB        : return "THUMB";
        case IMAGE_FILE_MACHINE_ARMNT        : return "ARMNT";
        case IMAGE_FILE_MACHINE_AM33         : return "AM33";
        case IMAGE_FILE_MACHINE_POWERPC      : return "POWERPC";
        case IMAGE_FILE_MACHINE_POWERPCFP    : return "POWERPCFP";
        case IMAGE_FILE_MACHINE_IA64         : return "IA64";
        case IMAGE_FILE_MACHINE_MIPS16       : return "MIPS16";
        case IMAGE_FILE_MACHINE_MIPSFPU      : return "MIPSFPU";
        case IMAGE_FILE_MACHINE_MIPSFPU16    : return "MIPSFPU16";
        case IMAGE_FILE_MACHINE_EBC          : return "EBC";
        case IMAGE_FILE_MACHINE_M32R         : return "M32R";
        case IMAGE_FILE_MACHINE_ARM64        : return "ARM64";
    }
    return "??";
}


static const char *dbgcPeDataDirName(unsigned iDir)
{
    switch (iDir)
    {
        case IMAGE_DIRECTORY_ENTRY_EXPORT:          return "EXPORT";
        case IMAGE_DIRECTORY_ENTRY_IMPORT:          return "IMPORT";
        case IMAGE_DIRECTORY_ENTRY_RESOURCE:        return "RESOURCE";
        case IMAGE_DIRECTORY_ENTRY_EXCEPTION:       return "EXCEPTION";
        case IMAGE_DIRECTORY_ENTRY_SECURITY:        return "SECURITY";
        case IMAGE_DIRECTORY_ENTRY_BASERELOC:       return "BASERELOC";
        case IMAGE_DIRECTORY_ENTRY_DEBUG:           return "DEBUG";
        case IMAGE_DIRECTORY_ENTRY_ARCHITECTURE:    return "ARCHITECTURE";
        case IMAGE_DIRECTORY_ENTRY_GLOBALPTR:       return "GLOBALPTR";
        case IMAGE_DIRECTORY_ENTRY_TLS:             return "TLS";
        case IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG:     return "LOAD_CONFIG";
        case IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT:    return "BOUND_IMPORT";
        case IMAGE_DIRECTORY_ENTRY_IAT:             return "IAT";
        case IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT:    return "DELAY_IMPORT";
        case IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR:  return "COM_DESCRIPTOR";
    }
    return "??";
}


static const char *dbgPeDebugTypeName(uint32_t uType, char *pszTmp, size_t cchTmp)
{
    switch (uType)
    {
        case IMAGE_DEBUG_TYPE_UNKNOWN:       return "UNKNOWN";
        case IMAGE_DEBUG_TYPE_COFF:          return "COFF";
        case IMAGE_DEBUG_TYPE_CODEVIEW:      return "CODEVIEW";
        case IMAGE_DEBUG_TYPE_FPO:           return "FPO";
        case IMAGE_DEBUG_TYPE_MISC:          return "MISC";
        case IMAGE_DEBUG_TYPE_EXCEPTION:     return "EXCEPTION";
        case IMAGE_DEBUG_TYPE_FIXUP:         return "FIXUP";
        case IMAGE_DEBUG_TYPE_OMAP_TO_SRC:   return "OMAP_TO_SRC";
        case IMAGE_DEBUG_TYPE_OMAP_FROM_SRC: return "OMAP_FROM_SRC";
        case IMAGE_DEBUG_TYPE_BORLAND:       return "BORLAND";
        case IMAGE_DEBUG_TYPE_RESERVED10:    return "RESERVED10";
        case IMAGE_DEBUG_TYPE_CLSID:         return "CLSID";
        case IMAGE_DEBUG_TYPE_VC_FEATURE:    return "VC_FEATURE";
        case IMAGE_DEBUG_TYPE_POGO:          return "POGO";
        case IMAGE_DEBUG_TYPE_ILTCG:         return "ILTCG";
        case IMAGE_DEBUG_TYPE_MPX:           return "MPX";
        case IMAGE_DEBUG_TYPE_REPRO:         return "REPRO";
    }
    RTStrPrintf(pszTmp, cchTmp, "%#RX32", uType);
    return pszTmp;
}


/**
 * PE dumper class.
 */
class DumpImagePe : public DumpImageBase
{
public:
    /** Pointer to the file header. */
    PCIMAGE_FILE_HEADER         m_pFileHdr;
    /** Pointer to the NT headers. */
    union
    {
        PCIMAGE_NT_HEADERS32    pNt32;
        PCIMAGE_NT_HEADERS64    pNt64;
        void                   *pv;
    } u;
    /** The PE header RVA / file offset. */
    uint32_t                    m_offPeHdr;
    /** Section table RVA / file offset. */
    uint32_t                    m_offShdrs;
    /** Pointer to the section headers. */
    PCIMAGE_SECTION_HEADER      m_paShdrs;
    /** Number of section headers. */
    unsigned                    m_cShdrs;
    /** Number of RVA and sizes (data directory entries). */
    unsigned                    cDataDir;
    /** Pointer to the data directory. */
    PCIMAGE_DATA_DIRECTORY      paDataDir;

public:
    DumpImagePe(PDBGCCMDHLP a_pCmdHlp, PCDBGCCMD a_pCmd, PCDBGCVAR a_pImageBase,
                const char *a_pszImageBaseAddr, const char *a_pszName,
                uint32_t a_offPeHdr, PCIMAGE_FILE_HEADER a_pFileHdr, void *a_pvNtHdrs,
                uint32_t a_offShdrs, unsigned a_cShdrs, PCIMAGE_SECTION_HEADER a_paShdrs)
        : DumpImageBase(a_pCmdHlp, a_pCmd, a_pImageBase, a_pszImageBaseAddr, a_pszName)
        , m_pFileHdr(a_pFileHdr)
        , m_offPeHdr(a_offPeHdr)
        , m_offShdrs(a_offShdrs)
        , m_paShdrs(a_paShdrs)
        , m_cShdrs(a_cShdrs)
        , cDataDir(0)
        , paDataDir(NULL)
    {
        u.pv = a_pvNtHdrs;
        if (a_pFileHdr->SizeOfOptionalHeader == sizeof(IMAGE_OPTIONAL_HEADER32))
        {
            paDataDir = u.pNt32->OptionalHeader.DataDirectory;
            cDataDir  = u.pNt32->OptionalHeader.NumberOfRvaAndSizes;
        }
        else
        {
            paDataDir = u.pNt64->OptionalHeader.DataDirectory;
            cDataDir  = u.pNt64->OptionalHeader.NumberOfRvaAndSizes;
        }
    }

    virtual size_t rvaToFileOffset(size_t uRva) const RT_OVERRIDE
    {
        AssertReturn(m_paShdrs, uRva);
        AssertReturn(u.pv, uRva);
        if (uRva < m_paShdrs[0].VirtualAddress)
            return uRva;
        unsigned iSh = m_cShdrs;
        while (iSh-- > 0)
        {
            if (uRva >= m_paShdrs[iSh].VirtualAddress)
            {
                size_t offSection = uRva - m_paShdrs[iSh].VirtualAddress;
                if (offSection < m_paShdrs[iSh].SizeOfRawData)
                    return m_paShdrs[iSh].PointerToRawData + offSection;
                return ~(size_t)0;
            }
        }
        return ~(size_t)0;
    }

    virtual size_t getEndRva(bool a_fAligned = true) const RT_OVERRIDE
    {
        AssertCompileMembersAtSameOffset(IMAGE_NT_HEADERS32, OptionalHeader.SizeOfImage,
                                         IMAGE_NT_HEADERS64, OptionalHeader.SizeOfImage);
        if (a_fAligned)
        {
            uint32_t const cbAlignment = u.pNt32->OptionalHeader.SectionAlignment;
            if (RT_IS_POWER_OF_TWO(cbAlignment))
                return RT_ALIGN_Z((size_t)u.pNt32->OptionalHeader.SizeOfImage, cbAlignment);
        }
        return u.pNt32->OptionalHeader.SizeOfImage;
    }


    /** @name Helpers
     * @{
     */

    char *timestampToString(uint32_t uTimestamp, char *pszDst, size_t cbDst) RT_NOEXCEPT
    {
        /** @todo detect random numbers and skip formatting them.   */
        RTTIMESPEC TimeSpec;
        RTTIME     Time;
        RTTimeToStringEx(RTTimeExplode(&Time, RTTimeSpecSetDosSeconds(&TimeSpec, uTimestamp)),
                         pszDst, cbDst, 0 /*cFractionDigits*/);
        return pszDst;
    }

    /** @} */

    /** @name Dumpers
     * @{
     */

    int dumpPeHdr(void)
    {
        char szTmp[64];
        myPrintHeader(m_offPeHdr, "PE & File Header - %s", m_pszName);
        myPrintf("Signature:                    %#010RX32\n", u.pNt32->Signature);
        PCIMAGE_FILE_HEADER const pFileHdr = &u.pNt32->FileHeader;
        myPrintf("Machine:                      %s (%#06RX16)\n", dbgcPeMachineName(pFileHdr->Machine), pFileHdr->Machine);
        myPrintf("Number of sections:           %#06RX16\n", pFileHdr->NumberOfSections);
        myPrintf("Timestamp:                    %#010RX32\n",
                 pFileHdr->TimeDateStamp, timestampToString(pFileHdr->TimeDateStamp, szTmp, sizeof(szTmp)));
        if (pFileHdr->PointerToSymbolTable || pFileHdr->NumberOfSymbols)
            myPrintf("Symbol table:                 %#010RX32 L %#06RX16\n",
                     pFileHdr->PointerToSymbolTable, pFileHdr->NumberOfSymbols);
        myPrintf("Size of optional header:      %#06RX16\n", pFileHdr->SizeOfOptionalHeader);

        myPrintf("Characteristics:              %#06RX16", pFileHdr->Characteristics);
        if (pFileHdr->Characteristics & IMAGE_FILE_RELOCS_STRIPPED)         myPrintf(" RELOCS_STRIPPED");
        if (pFileHdr->Characteristics & IMAGE_FILE_EXECUTABLE_IMAGE)        myPrintf(" EXECUTABLE_IMAGE");
        if (pFileHdr->Characteristics & IMAGE_FILE_LINE_NUMS_STRIPPED)      myPrintf(" LINE_NUMS_STRIPPED");
        if (pFileHdr->Characteristics & IMAGE_FILE_LOCAL_SYMS_STRIPPED)     myPrintf(" LOCAL_SYMS_STRIPPED");
        if (pFileHdr->Characteristics & IMAGE_FILE_AGGRESIVE_WS_TRIM)       myPrintf(" AGGRESIVE_WS_TRIM");
        if (pFileHdr->Characteristics & IMAGE_FILE_LARGE_ADDRESS_AWARE)     myPrintf(" LARGE_ADDRESS_AWARE");
        if (pFileHdr->Characteristics & IMAGE_FILE_16BIT_MACHINE)           myPrintf(" 16BIT_MACHINE");
        if (pFileHdr->Characteristics & IMAGE_FILE_BYTES_REVERSED_LO)       myPrintf(" BYTES_REVERSED_LO");
        if (pFileHdr->Characteristics & IMAGE_FILE_32BIT_MACHINE)           myPrintf(" 32BIT_MACHINE");
        if (pFileHdr->Characteristics & IMAGE_FILE_DEBUG_STRIPPED)          myPrintf(" DEBUG_STRIPPED");
        if (pFileHdr->Characteristics & IMAGE_FILE_REMOVABLE_RUN_FROM_SWAP) myPrintf(" REMOVABLE_RUN_FROM_SWAP");
        if (pFileHdr->Characteristics & IMAGE_FILE_NET_RUN_FROM_SWAP)       myPrintf(" NET_RUN_FROM_SWAP");
        if (pFileHdr->Characteristics & IMAGE_FILE_SYSTEM)                  myPrintf(" SYSTEM");
        if (pFileHdr->Characteristics & IMAGE_FILE_DLL)                     myPrintf(" DLL");
        if (pFileHdr->Characteristics & IMAGE_FILE_UP_SYSTEM_ONLY)          myPrintf(" UP_SYSTEM_ONLY");
        if (pFileHdr->Characteristics & IMAGE_FILE_BYTES_REVERSED_HI)       myPrintf(" BYTES_REVERSED_HI");
        myPrintf("\n");
        return VINF_SUCCESS;
    }

    template<typename OptHdrType, bool const a_f32Bit>
    int dumpOptHdr(OptHdrType const *pOptHdr, uint32_t uBaseOfData = 0)
    {
        myPrintHeader(m_offPeHdr + RT_UOFFSETOF(IMAGE_NT_HEADERS32, OptionalHeader), "Optional Header");
        char szTmp[64];
        myPrintf("Optional header magic:        %#06RX16\n", pOptHdr->Magic);
        myPrintf("Linker version:               %u.%02u\n", pOptHdr->MajorLinkerVersion, pOptHdr->MinorLinkerVersion);
        if (a_f32Bit)
            myPrintf("Image base:                   %#010RX32\n", pOptHdr->ImageBase);
        else
            myPrintf("Image base:                   %#018RX64\n", pOptHdr->ImageBase);
        myPrintf("Entrypoint:                   %s\n", rvaToStringWithAddr(pOptHdr->AddressOfEntryPoint, szTmp, sizeof(szTmp)));
        myPrintf("Base of code:                 %s\n", rvaToStringWithAddr(pOptHdr->BaseOfCode, szTmp, sizeof(szTmp)));
        if (a_f32Bit)
            myPrintf("Base of data:                 %s\n", rvaToStringWithAddr(uBaseOfData, szTmp, sizeof(szTmp)));
        myPrintf("Size of image:                %#010RX32\n", pOptHdr->SizeOfImage);
        myPrintf("Size of headers:              %#010RX32\n", pOptHdr->SizeOfHeaders);
        myPrintf("Size of code:                 %#010RX32\n", pOptHdr->SizeOfCode);
        myPrintf("Size of initialized data:     %#010RX32\n", pOptHdr->SizeOfInitializedData);
        myPrintf("Size of uninitialized data:   %#010RX32\n", pOptHdr->SizeOfUninitializedData);
        myPrintf("Section alignment:            %#010RX32\n", pOptHdr->SectionAlignment);
        myPrintf("File alignment:               %#010RX32\n", pOptHdr->FileAlignment);
        myPrintf("Image version:                %u.%02u\n", pOptHdr->MajorImageVersion, pOptHdr->MinorImageVersion);
        myPrintf("Operating system version:     %u.%02u\n", pOptHdr->MajorOperatingSystemVersion, pOptHdr->MinorOperatingSystemVersion);
        myPrintf("Windows version value:        %#010RX32\n", pOptHdr->Win32VersionValue);
        const char *pszSubSys;
        switch (pOptHdr->Subsystem)
        {
            case IMAGE_SUBSYSTEM_UNKNOWN:                   pszSubSys = "Unknown"; break;
            case IMAGE_SUBSYSTEM_NATIVE:                    pszSubSys = "Native"; break;
            case IMAGE_SUBSYSTEM_WINDOWS_GUI:               pszSubSys = "Windows GUI"; break;
            case IMAGE_SUBSYSTEM_WINDOWS_CUI:               pszSubSys = "Windows char"; break;
            case IMAGE_SUBSYSTEM_OS2_GUI:                   pszSubSys = "OS/2 GUI"; break;
            case IMAGE_SUBSYSTEM_OS2_CUI:                   pszSubSys = "OS/2 char"; break;
            case IMAGE_SUBSYSTEM_POSIX_CUI:                 pszSubSys = "POSIX"; break;
            case IMAGE_SUBSYSTEM_NATIVE_WINDOWS:            pszSubSys = "Native Windows 9x"; break;
            case IMAGE_SUBSYSTEM_WINDOWS_CE_GUI:            pszSubSys = "Windows CE GUI"; break;
            case IMAGE_SUBSYSTEM_EFI_APPLICATION:           pszSubSys = "EFI Application"; break;
            case IMAGE_SUBSYSTEM_EFI_BOOT_SERVICE_DRIVER:   pszSubSys = "EFI Boot Service Driver"; break;
            case IMAGE_SUBSYSTEM_EFI_RUNTIME_DRIVER:        pszSubSys = "EFI Runtime Driver"; break;
            case IMAGE_SUBSYSTEM_EFI_ROM:                   pszSubSys = "EFI ROM"; break;
            case IMAGE_SUBSYSTEM_XBOX:                      pszSubSys = "XBox"; break;
            case IMAGE_SUBSYSTEM_WINDOWS_BOOT_APPLICATION:  pszSubSys = "Windows Boot Application"; break;
            default:                                        pszSubSys = "dunno"; break;
        }
        myPrintf("Subsystem:                    %s (%#x)\n", pszSubSys, pOptHdr->Subsystem);
        myPrintf("Subsystem version:            %u.%02u\n", pOptHdr->MajorSubsystemVersion, pOptHdr->MinorSubsystemVersion);
        myPrintf("DLL characteristics:          %#06RX16\n", pOptHdr->DllCharacteristics);
        myPrintf("Loader flags:                 %#010RX32\n", pOptHdr->LoaderFlags);

        myPrintf("File checksum:                %#010RX32\n", pOptHdr->CheckSum);
        myPrintf("Size of stack reserve:        %#010RX64\n", (uint64_t)pOptHdr->SizeOfStackReserve);
        myPrintf("Size of stack commit:         %#010RX64\n", (uint64_t)pOptHdr->SizeOfStackReserve);
        myPrintf("Size of heap reserve:         %#010RX64\n", (uint64_t)pOptHdr->SizeOfHeapReserve);
        myPrintf("Size of heap commit:          %#010RX64\n", (uint64_t)pOptHdr->SizeOfHeapReserve);

        myPrintf("Number of data directories:   %#010RX32%s\n", pOptHdr->NumberOfRvaAndSizes,
                 pOptHdr->NumberOfRvaAndSizes <= RT_ELEMENTS(pOptHdr->DataDirectory) ? "" : " - bogus!");

        for (uint32_t i = 0; i < RT_ELEMENTS(pOptHdr->DataDirectory); i++)
            if (pOptHdr->DataDirectory[i].Size || pOptHdr->DataDirectory[i].VirtualAddress)
            {
                const char * const pszName = dbgcPeDataDirName(i);
                rvaToStringWithAddr(pOptHdr->DataDirectory[i].VirtualAddress, szTmp, sizeof(szTmp));
                if (i == IMAGE_DIRECTORY_ENTRY_SECURITY)
                {
                    size_t const cchWidth = strlen(szTmp);
                    size_t        cch     = RTStrPrintf(szTmp, sizeof(szTmp), "%#09RX32 (file off)",
                                                        pOptHdr->DataDirectory[i].VirtualAddress);
                    while (cch < cchWidth)
                        szTmp[cch++] = ' ';
                    szTmp[cch] = '\0';
                }
                myPrintf("DataDirectory[%#x]:           %s LB %#07RX32 %s\n", i, szTmp, pOptHdr->DataDirectory[i].Size, pszName);
            }
        return VINF_SUCCESS;
    }

    int dumpSectionHdrs(void) RT_NOEXCEPT
    {
        myPrintHeader(m_offShdrs, "Section Table");
        for (unsigned i = 0; i < m_cShdrs; i++)
        {
            char szTmp[64];
            myPrintf("Section[%02u]: %s LB %08RX32 %.8s\n",
                     i, rvaToStringWithAddr(m_paShdrs[i].VirtualAddress, szTmp, sizeof(szTmp)),
                     m_paShdrs[i].Misc.VirtualSize, m_paShdrs[i].Name);
        }
        return VINF_SUCCESS;
    }

    int dumpExportDir(DumpImageBufferedReader *pBufRdr, uint32_t uRvaData, uint32_t cbData)
    {
        myPrintHeader(uRvaData, "Export Table");
        RT_NOREF(cbData);
        char szTmp[64];

        /* Use dedicated readers for each array, but saving one by using pBufRdr
           for function addresses. */
        DumpImageBufferedReader NmAddrRdr(*pBufRdr), OrdRdr(*pBufRdr), NameRdr(*pBufRdr);

        /*
         * Read the entry into memory.
         */
        IMAGE_EXPORT_DIRECTORY ExpDir;
        int rc = pBufRdr->readBytes(uRvaData, &ExpDir, sizeof(ExpDir));
        if (RT_FAILURE(rc))
            return rc;

        /*
         * Dump the directory.
         */
        myPrintf("             Name: %s %s\n",
                 rvaToStringWithAddr(ExpDir.Name, szTmp, sizeof(szTmp)), NmAddrRdr.bufferedString(ExpDir.Name));
        myPrintf("    Address table: %s L %u\n",
                 rvaToStringWithAddr(ExpDir.AddressOfFunctions, szTmp, sizeof(szTmp)), ExpDir.NumberOfFunctions);
        myPrintf("       Name table: %s L %u\n",
                 rvaToStringWithAddr(ExpDir.AddressOfNames, szTmp, sizeof(szTmp)), ExpDir.NumberOfNames);
        myPrintf(" Name index table: %s L ditto\n",
                 rvaToStringWithAddr(ExpDir.AddressOfNameOrdinals, szTmp, sizeof(szTmp)), ExpDir.NumberOfNames);
        myPrintf("     Ordinal base: %u\n", ExpDir.Base);
        if (ExpDir.Characteristics)
            myPrintf("  Characteristics: %#RX32\n", ExpDir.Characteristics);
        if (ExpDir.TimeDateStamp && ExpDir.TimeDateStamp != UINT32_MAX)
            myPrintf("    TimeDateStamp: %#RX32 %s\n",
                     ExpDir.TimeDateStamp, timestampToString(ExpDir.TimeDateStamp, szTmp, sizeof(szTmp)));
        if (ExpDir.MajorVersion || ExpDir.MinorVersion)
            myPrintf("          Version: %u.%u\n", ExpDir.MajorVersion, ExpDir.MinorVersion);

        uint32_t const cExports = ExpDir.NumberOfNames;
        if (cExports > _16K)
        {
            myPrintf("  Exports: Too many addresses! (%#x)\n", cExports);
            return VINF_SUCCESS;
        }
        uint32_t const cNames = ExpDir.NumberOfNames;
        if (cNames > _32K)
        {
            myPrintf("  Exports: Too many names! (%#x)\n", cNames);
            return VINF_SUCCESS;
        }
        if (cExports == 0)
        {
            myPrintf("  Exports: No exports!\n");
            return VINF_SUCCESS;
        }

        /*
         * Read the export addresses and  name tables into memory.
         */
        uint32_t const *pauExportRvas = (uint32_t const *)pBufRdr->bufferedBytes(ExpDir.AddressOfFunctions,
                                                                                 sizeof(pauExportRvas[0])* cExports);
        uint16_t const *pau16Ordinals = NULL;
        uint32_t const *pauNameRvas   = NULL;
        bool            fOrderedOrdinals = true;
        if (cNames)
        {
            pauNameRvas   = (uint32_t const *)NmAddrRdr.bufferedBytes(ExpDir.AddressOfNames, sizeof(pauNameRvas[0]) * cNames);
            if (!pauNameRvas)
                return VINF_SUCCESS;
            pau16Ordinals = (uint16_t const *)OrdRdr.bufferedBytes(ExpDir.AddressOfNameOrdinals,
                                                                   sizeof(pau16Ordinals[0]) * cNames);
            if (!pau16Ordinals)
                return VINF_SUCCESS;

            /* Check if the name ordinals are ordered. */
            uint16_t iPrev = pau16Ordinals[0];
            for (uint32_t iOrd = 1; iOrd < cNames; iOrd++)
            {
                uint16_t const iCur = pau16Ordinals[iOrd];
                if (iCur > iPrev)
                    iPrev = iCur;
                else
                {
                    fOrderedOrdinals = false;
                    break;
                }
            }

        }

        /*
         * Dump the exports by named exports.
         */
        static const char s_szAddr[] = "Export RVA/Address";
        unsigned          cchAddr    = (unsigned)strlen(rvaToStringWithAddr(uRvaData, szTmp, sizeof(szTmp)));
        cchAddr = RT_MAX(cchAddr, sizeof(s_szAddr) - 1);
        myPrintf("\n"
                 "Ordinal %*s%s%*s Name RVA  Name\n"
                 "------- %*.*s --------- --------------------------------\n",
                 (cchAddr - sizeof(s_szAddr) + 1) / 2, "", s_szAddr, (cchAddr - sizeof(s_szAddr) + 1 + 1) / 2, "",
                 cchAddr, cchAddr, "--------------------------------------");

        for (uint32_t iExp = 0, iName = 0; iExp < cExports; iExp++)
        {
            if (cNames > 0)
            {
                if (fOrderedOrdinals)
                {
                    if (iName < cNames && pau16Ordinals[iName] == iExp)
                    {
                        uint32_t     const uRvaName = pauNameRvas[iExp];
                        const char * const pszName = NameRdr.bufferedString(uRvaName);
                        myPrintf("%7u %s %#09RX32 %s\n", iExp + ExpDir.Base,
                                 rvaToStringWithAddr(pauExportRvas[iExp], szTmp, sizeof(szTmp)),
                                 uRvaName, pszName ? pszName : "");
                        iName++;
                        continue;
                    }
                }
                else
                {
                    /* Search the entire name ordinal table, not stopping on a hit
                       as there could in theory be different names for the same entry. */
                    uint32_t cPrinted = 0;
                    for (iName = 0; iName < cNames; iName++)
                        if (pau16Ordinals[iName] == iExp)
                        {
                            uint32_t     const uRvaName = pauNameRvas[iExp];
                            const char * const pszName = NameRdr.bufferedString(uRvaName);
                            myPrintf("%7u %s %#09RX32 %s\n", iExp + ExpDir.Base,
                                     rvaToStringWithAddr(pauExportRvas[iExp], szTmp, sizeof(szTmp)),
                                     uRvaName, pszName ? pszName : "");
                            cPrinted++;
                        }
                    if (cPrinted)
                        continue;
                }
            }
            /* Ordinal only. */
            myPrintf("%7u %s %#09RX32\n", iExp + ExpDir.Base,
                     rvaToStringWithAddr(pauExportRvas[iExp], szTmp, sizeof(szTmp)), UINT32_MAX);
        }
        return VINF_SUCCESS;
    }

    template<typename ThunkType, bool const a_f32Bit, ThunkType const a_fOrdinalConst>
    int dumpImportDir(DumpImageBufferedReader *pBufRdr, uint32_t uRvaData, uint32_t cbData)
    {
        char         szTmp[64];
        char         szTmp2[64];
        size_t const cchRvaWithAddr = strlen(rvaToStringWithAddr(uRvaData, szTmp, sizeof(szTmp)));

        /* Use dedicated readers for each array and names */
        DumpImageBufferedReader NameRdr(*pBufRdr), Thunk1stRdr(*pBufRdr), ThunkOrgRdr(*pBufRdr);

        myPrintHeader(uRvaData, "Import table");
        int            rcRet    = VINF_SUCCESS;
        uint32_t const cEntries = cbData / sizeof(IMAGE_IMPORT_DESCRIPTOR);
        for (uint32_t i = 0; i < cEntries; i += 1, uRvaData += sizeof(IMAGE_IMPORT_DESCRIPTOR))
        {
            /*
             * Read the entry into memory.
             */
            IMAGE_IMPORT_DESCRIPTOR ImpDir;
            int rc = pBufRdr->readBytes(uRvaData, &ImpDir, sizeof(ImpDir));
            if (RT_FAILURE(rc))
                return rc;

            if (ImpDir.Name == 0)
                continue;

            /*
             * Dump it.
             */
            if (i > 0)
                myPrintf("\n");
            myPrintf("         Entry #: %u\n",  i);
            myPrintf("            Name: %s - %s\n", rvaToStringWithAddr(ImpDir.Name, szTmp, sizeof(szTmp)),
                     ImpDir.Name ? NameRdr.bufferedString(ImpDir.Name) : "");
            if (ImpDir.TimeDateStamp && ImpDir.TimeDateStamp != UINT32_MAX)
                myPrintf("       Timestamp: %#010RX32 %s\n",
                         ImpDir.TimeDateStamp, timestampToString(ImpDir.TimeDateStamp, szTmp, sizeof(szTmp)));
            myPrintf("     First thunk: %s\n", rvaToStringWithAddr(ImpDir.FirstThunk, szTmp, sizeof(szTmp)));
            myPrintf("  Original thunk: %s\n", rvaToStringWithAddr(ImpDir.u.OriginalFirstThunk, szTmp, sizeof(szTmp)));
            if (ImpDir.ForwarderChain)
                myPrintf(" Forwarder chain: %s\n", rvaToStringWithAddr(ImpDir.ForwarderChain, szTmp, sizeof(szTmp)));

            /*
             * Try process the arrays.
             */
            static char const s_szDashes[] = "-----------------------------------------------";
            static char const s_szHdr1[]   = "Thunk RVA/Addr";
            uint32_t uRvaNames = ImpDir.u.OriginalFirstThunk;
            uint32_t uRvaThunk = ImpDir.FirstThunk;
            if (uRvaThunk == 0)
                uRvaThunk = uRvaNames;
            if (uRvaNames != 0 && uRvaNames != uRvaThunk)
            {
                static char const s_szHdr2[] = "Thunk";
                static char const s_szHdr4[] = "Hint+Name RVA/Addr";
                size_t const      cchCol1    = RT_MAX(sizeof(s_szHdr1) - 1, cchRvaWithAddr);
                size_t const      cchCol2    = RT_MAX(sizeof(s_szHdr2) - 1, 2 + sizeof(ThunkType) * 2);
                size_t const      cchCol4    = RT_MAX(sizeof(s_szHdr4) - 1, cchRvaWithAddr);

                myPrintf(" No.  %-*s %-*s Ord/Hint %-*s Name\n"
                         "----  %.*s %.*s -------- %.*s ----------------\n",
                         cchCol1, s_szHdr1,   cchCol2, s_szHdr2,   cchCol4, s_szHdr4,
                         cchCol1, s_szDashes, cchCol2, s_szDashes, cchCol4, s_szDashes);
                for (uint32_t iEntry = 0;; iEntry += 1, uRvaThunk += sizeof(ThunkType), uRvaNames += sizeof(ThunkType))
                {
                    ThunkType const uName  = ThunkOrgRdr.bufferedInt<ThunkType>(uRvaNames, 0);
                    ThunkType const uThunk = Thunk1stRdr.bufferedInt<ThunkType>(uRvaThunk, 0);
                    if (!uName || !uThunk)
                        break;

                    if (!(uName & a_fOrdinalConst))
                    {
                        uint16_t const     uHint   = NameRdr.bufferedInt<uint16_t>(uName);
                        const char * const pszName = NameRdr.bufferedString(uName + 2);
                        if (a_f32Bit)
                            myPrintf("%4u: %s %#010RX32 %8RU16 %s %s\n",
                                     iEntry, rvaToStringWithAddr(uRvaThunk, szTmp, sizeof(szTmp)), uThunk, uHint,
                                     rvaToStringWithAddr(uName, szTmp2, sizeof(szTmp2)), pszName);
                        else
                            myPrintf("%4u: %s %#018RX64 %8RU16 %s %s\n",
                                     iEntry, rvaToStringWithAddr(uRvaThunk, szTmp, sizeof(szTmp)), uThunk, uHint,
                                     rvaToStringWithAddr(uName, szTmp2, sizeof(szTmp2)), pszName);
                    }
                    else
                    {
                        if (a_f32Bit)
                            myPrintf("%4u: %s %#010RX32 %8RU32\n", iEntry,
                                     rvaToStringWithAddr(uRvaThunk, szTmp, sizeof(szTmp)), uThunk, uName & ~a_fOrdinalConst);
                        else
                            myPrintf("%4u: %s %#018RX64 %8RU64\n", iEntry,
                                     rvaToStringWithAddr(uRvaThunk, szTmp, sizeof(szTmp)), uThunk, uName & ~a_fOrdinalConst);
                    }
                }
            }
            /** @todo */
            //else if (uRvaThunk)
            //    for (;;)
            //    {
            //        ThunkType const *pThunk = (ThunkType const *)Thunk1stRdr.bufferedBytes(uRvaThunk, sizeof(*pThunk));
            //        if (!pThunk->u1.AddressOfData == 0)
            //            break;
            //    }
        }
        return rcRet;
    }

    int dumpDebugDir(DumpImageBufferedReader *pBufRdr, uint32_t uRvaData, uint32_t cbData)
    {
        myPrintHeader(uRvaData, "Debug Directory");
        int            rcRet    = VINF_SUCCESS;
        uint32_t const cEntries = cbData / sizeof(IMAGE_DEBUG_DIRECTORY);
        for (uint32_t i = 0; i < cEntries; i += 1, uRvaData += sizeof(IMAGE_DEBUG_DIRECTORY))
        {
            /*
             * Read the entry into memory.
             */
            IMAGE_DEBUG_DIRECTORY DbgDir;
            int rc = pBufRdr->readBytes(uRvaData, &DbgDir, sizeof(DbgDir));
            if (RT_FAILURE(rc))
                return rc;

            /*
             * Dump it.
             * (longest type is 13 chars:'OMAP_FROM_SRC')
             */
            char szTmp[64];
            char szTmp2[64];
            myPrintf("%u: %s LB %06RX32  %#09RX32  %13s",
                     i, rvaToStringWithAddr(DbgDir.AddressOfRawData, szTmp, sizeof(szTmp)), DbgDir.SizeOfData,
                     DbgDir.PointerToRawData,
                     dbgPeDebugTypeName(DbgDir.Type, szTmp2, sizeof(szTmp2)));
            if (DbgDir.MajorVersion || DbgDir.MinorVersion)
                myPrintf("  v%u.%u", DbgDir.MajorVersion, DbgDir.MinorVersion);
            if (DbgDir.Characteristics)
                myPrintf("  flags=%#RX32", DbgDir.Characteristics);
            myPrintf("  %s (%#010RX32)\n", timestampToString(DbgDir.TimeDateStamp, szTmp, sizeof(szTmp)), DbgDir.TimeDateStamp);

            union
            {
                uint8_t             abPage[0x1000];
                CVPDB20INFO         Pdb20;
                CVPDB70INFO         Pdb70;
                IMAGE_DEBUG_MISC    Misc;
            } uBuf;
            RT_ZERO(uBuf);

            if (DbgDir.Type == IMAGE_DEBUG_TYPE_CODEVIEW)
            {
                if (   DbgDir.SizeOfData < sizeof(uBuf)
                    && DbgDir.SizeOfData > 16
                    && DbgDir.AddressOfRawData > 0
                    && RT_SUCCESS(rc))
                {
                    rc = pBufRdr->readBytes(DbgDir.AddressOfRawData, &uBuf, DbgDir.SizeOfData);
                    if (RT_SUCCESS(rc))
                    {
                        if (   uBuf.Pdb20.u32Magic   == CVPDB20INFO_MAGIC
                            && uBuf.Pdb20.offDbgInfo == 0
                            && DbgDir.SizeOfData > RT_UOFFSETOF(CVPDB20INFO, szPdbFilename) )
                            myPrintf("    PDB2.0: ts=%08RX32 age=%RX32 %s\n",
                                     uBuf.Pdb20.uTimestamp, uBuf.Pdb20.uAge, uBuf.Pdb20.szPdbFilename);
                        else if (   uBuf.Pdb20.u32Magic == CVPDB70INFO_MAGIC
                                 && DbgDir.SizeOfData > RT_UOFFSETOF(CVPDB70INFO, szPdbFilename) )
                            myPrintf("    PDB7.0: %RTuuid age=%u %s\n",
                                     &uBuf.Pdb70.PdbUuid, uBuf.Pdb70.uAge, uBuf.Pdb70.szPdbFilename);
                        else
                            myPrintf("    Unknown PDB/codeview magic: %.8Rhxs\n", uBuf.abPage);
                    }
                    else
                        rcRet = rc;
                }
            }
            else if (DbgDir.Type == IMAGE_DEBUG_TYPE_MISC)
            {
                if (   DbgDir.SizeOfData < sizeof(uBuf)
                    && DbgDir.SizeOfData > RT_UOFFSETOF(IMAGE_DEBUG_MISC, Data)
                    && DbgDir.AddressOfRawData > 0)
                {
                    rc = pBufRdr->readBytes(DbgDir.AddressOfRawData, &uBuf, DbgDir.SizeOfData);
                    if (RT_SUCCESS(rc))
                    {
                        if (   uBuf.Misc.DataType == IMAGE_DEBUG_MISC_EXENAME
                            && uBuf.Misc.Length   == DbgDir.SizeOfData)
                        {
                            if (!uBuf.Misc.Unicode)
                                myPrintf("    Misc DBG: ts=%RX32 %s\n", DbgDir.TimeDateStamp, (const char *)&uBuf.Misc.Data[0]);
                            else
                                myPrintf("    Misc DBG: ts=%RX32 %ls\n", DbgDir.TimeDateStamp, (PCRTUTF16)&uBuf.Misc.Data[0]);
                        }
                    }
                    else
                        rcRet = rc;
                }
            }
        }
        return rcRet;
    }

    int dumpDataDirs(DumpImageBufferedReader *pBufRdr, unsigned cDataDirs, PCIMAGE_DATA_DIRECTORY paDataDirs)
    {
        int rcRet = VINF_SUCCESS;
        for (unsigned i = 0; i < cDataDirs; i++)
            if (paDataDirs[i].Size > 0 && paDataDirs[i].VirtualAddress)
            {
                int rc;
                if (   i == IMAGE_DIRECTORY_ENTRY_EXPORT
                         && paDataDirs[i].Size >= sizeof(IMAGE_EXPORT_DIRECTORY))
                    rc = dumpExportDir(pBufRdr, paDataDirs[i].VirtualAddress, paDataDirs[i].Size);
                else if (   i == IMAGE_DIRECTORY_ENTRY_IMPORT
                         && paDataDirs[i].Size >= sizeof(IMAGE_IMPORT_DESCRIPTOR))
                {
                    if (m_pFileHdr->SizeOfOptionalHeader == sizeof(IMAGE_OPTIONAL_HEADER32))
                        rc = dumpImportDir<uint32_t,  true, IMAGE_ORDINAL_FLAG32>(pBufRdr, paDataDirs[i].VirtualAddress,
                                                                                  paDataDirs[i].Size);
                    else
                        rc = dumpImportDir<uint64_t, false, IMAGE_ORDINAL_FLAG64>(pBufRdr, paDataDirs[i].VirtualAddress,
                                                                                  paDataDirs[i].Size);
                }
                else if (   i == IMAGE_DIRECTORY_ENTRY_DEBUG
                         && paDataDirs[i].Size >= sizeof(IMAGE_DEBUG_DIRECTORY))
                    rc = dumpDebugDir(pBufRdr, paDataDirs[i].VirtualAddress, paDataDirs[i].Size);
                else
                    continue;
                if (RT_FAILURE(rc))
                    rcRet = rc;
            }
        return rcRet;
    }

    /** @} */
};


static int dbgcDumpImagePe(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PCDBGCVAR pImageBase, const char *pszImageBaseAddr,
                           const char *pszName, uint32_t offPeHdr, PCIMAGE_FILE_HEADER pFileHdr)
{
    DBGCCmdHlpPrintf(pCmdHlp, "%s: PE image - %#x (%s), %u sections\n", pszName, pFileHdr->Machine,
                     dbgcPeMachineName(pFileHdr->Machine), pFileHdr->NumberOfSections);

    /* Is it a supported optional header size? */
    uint8_t cBits;
    if (pFileHdr->SizeOfOptionalHeader == sizeof(IMAGE_OPTIONAL_HEADER32))
        cBits = 32;
    else if (pFileHdr->SizeOfOptionalHeader == sizeof(IMAGE_OPTIONAL_HEADER64))
        cBits = 64;
    else
        return DBGCCmdHlpFail(pCmdHlp, pCmd, "%s: Unsupported optional header size: %#x\n",
                              pszName, pFileHdr->SizeOfOptionalHeader);

    /*
     * Allocate memory for all the headers, including section headers, and read them into memory.
     */
    size_t const offShdrs = pFileHdr->SizeOfOptionalHeader + sizeof(*pFileHdr) + sizeof(uint32_t);
    size_t const cbHdrs   = offShdrs + pFileHdr->NumberOfSections * sizeof(IMAGE_SECTION_HEADER);
    if (cbHdrs > _2M)
        return DBGCCmdHlpFail(pCmdHlp, pCmd, "%s: headers too big: %zu.\n", pszName, cbHdrs);

    void *pvBuf = RTMemTmpAllocZ(cbHdrs);
    if (!pvBuf)
        return DBGCCmdHlpFail(pCmdHlp, pCmd, "%s: failed to allocate %zu bytes for headers.\n", pszName, cbHdrs);

    int rc = dumpReadAt(pCmdHlp, pCmd, pImageBase, pszName, offPeHdr, pvBuf, cbHdrs, NULL);
    if (RT_SUCCESS(rc))
    {
        /* Format the image base value from the header if one isn't specified. */
        char szTmp[32];
        if (!pszImageBaseAddr)
        {
            if (cBits == 32)
                RTStrPrintf(szTmp, sizeof(szTmp), "%#010RX32", ((PIMAGE_NT_HEADERS32)pvBuf)->OptionalHeader.ImageBase);
            else
                RTStrPrintf(szTmp, sizeof(szTmp), "%#018RX64", ((PIMAGE_NT_HEADERS64)pvBuf)->OptionalHeader.ImageBase);
            pszImageBaseAddr = szTmp;
        }

        /* Finally, instantiate dumper now that we've got the section table
           loaded, and let it contiue. */
        DumpImagePe This(pCmdHlp, pCmd, pImageBase, pszImageBaseAddr, pszName,
                         offPeHdr, pFileHdr, pvBuf,
                         (uint32_t)offShdrs, pFileHdr->NumberOfSections, (PCIMAGE_SECTION_HEADER)((uintptr_t)pvBuf + offShdrs));

        This.dumpPeHdr();
        if (cBits == 32)
            rc = This.dumpOptHdr<IMAGE_OPTIONAL_HEADER32, true>(&This.u.pNt32->OptionalHeader,
                                                                This.u.pNt32->OptionalHeader.BaseOfData);
        else
            rc = This.dumpOptHdr<IMAGE_OPTIONAL_HEADER64, false>(&This.u.pNt64->OptionalHeader);

        int rc2 = This.dumpSectionHdrs();
        if (RT_FAILURE(rc2) && RT_SUCCESS(rc))
            rc = rc2;

        DumpImageBufferedReader BufRdr(&This);
        rc2 = This.dumpDataDirs(&BufRdr, This.cDataDir, This.paDataDir);
        if (RT_FAILURE(rc2) && RT_SUCCESS(rc))
            rc = rc2;
    }
    RTMemTmpFree(pvBuf);
    return rc;
}


/*********************************************************************************************************************************
*   ELF                                                                                                                          *
*********************************************************************************************************************************/

static int dbgcDumpImageElf(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PCDBGCVAR pImageBase, const char *pszName)
{
    RT_NOREF(pCmd, pImageBase);
    DBGCCmdHlpPrintf(pCmdHlp, "%s: ELF image dumping not implemented yet.\n", pszName);
    return VINF_SUCCESS;
}


/*********************************************************************************************************************************
*   Mach-O                                                                                                                       *
*********************************************************************************************************************************/

static const char *dbgcMachoFileType(uint32_t uType)
{
    switch (uType)
    {
        case MH_OBJECT:      return "MH_OBJECT";
        case MH_EXECUTE:     return "MH_EXECUTE";
        case MH_FVMLIB:      return "MH_FVMLIB";
        case MH_CORE:        return "MH_CORE";
        case MH_PRELOAD:     return "MH_PRELOAD";
        case MH_DYLIB:       return "MH_DYLIB";
        case MH_DYLINKER:    return "MH_DYLINKER";
        case MH_BUNDLE:      return "MH_BUNDLE";
        case MH_DYLIB_STUB:  return "MH_DYLIB_STUB";
        case MH_DSYM:        return "MH_DSYM";
        case MH_KEXT_BUNDLE: return "MH_KEXT_BUNDLE";
    }
    return "??";
}


static const char *dbgcMachoCpuType(int32_t iType, int32_t iSubType)
{
    switch (iType)
    {
        case CPU_TYPE_ANY:          return "CPU_TYPE_ANY";
        case CPU_TYPE_VAX:          return "VAX";
        case CPU_TYPE_MC680x0:      return "MC680x0";
        case CPU_TYPE_X86:          return "X86";
        case CPU_TYPE_X86_64:
            switch (iSubType)
            {
                case CPU_SUBTYPE_X86_64_ALL:    return "X86_64/ALL64";
            }
            return "X86_64";
        case CPU_TYPE_MC98000:      return "MC98000";
        case CPU_TYPE_HPPA:         return "HPPA";
        case CPU_TYPE_MC88000:      return "MC88000";
        case CPU_TYPE_SPARC:        return "SPARC";
        case CPU_TYPE_I860:         return "I860";
        case CPU_TYPE_POWERPC:      return "POWERPC";
        case CPU_TYPE_POWERPC64:    return "POWERPC64";

    }
    return "??";
}


static const char *dbgcMachoLoadCommand(uint32_t uCmd)
{
    switch (uCmd)
    {
        RT_CASE_RET_STR(LC_SEGMENT_32);
        RT_CASE_RET_STR(LC_SYMTAB);
        RT_CASE_RET_STR(LC_SYMSEG);
        RT_CASE_RET_STR(LC_THREAD);
        RT_CASE_RET_STR(LC_UNIXTHREAD);
        RT_CASE_RET_STR(LC_LOADFVMLIB);
        RT_CASE_RET_STR(LC_IDFVMLIB);
        RT_CASE_RET_STR(LC_IDENT);
        RT_CASE_RET_STR(LC_FVMFILE);
        RT_CASE_RET_STR(LC_PREPAGE);
        RT_CASE_RET_STR(LC_DYSYMTAB);
        RT_CASE_RET_STR(LC_LOAD_DYLIB);
        RT_CASE_RET_STR(LC_ID_DYLIB);
        RT_CASE_RET_STR(LC_LOAD_DYLINKER);
        RT_CASE_RET_STR(LC_ID_DYLINKER);
        RT_CASE_RET_STR(LC_PREBOUND_DYLIB);
        RT_CASE_RET_STR(LC_ROUTINES);
        RT_CASE_RET_STR(LC_SUB_FRAMEWORK);
        RT_CASE_RET_STR(LC_SUB_UMBRELLA);
        RT_CASE_RET_STR(LC_SUB_CLIENT);
        RT_CASE_RET_STR(LC_SUB_LIBRARY);
        RT_CASE_RET_STR(LC_TWOLEVEL_HINTS);
        RT_CASE_RET_STR(LC_PREBIND_CKSUM);
        RT_CASE_RET_STR(LC_LOAD_WEAK_DYLIB);
        RT_CASE_RET_STR(LC_SEGMENT_64);
        RT_CASE_RET_STR(LC_ROUTINES_64);
        RT_CASE_RET_STR(LC_UUID);
        RT_CASE_RET_STR(LC_RPATH);
        RT_CASE_RET_STR(LC_CODE_SIGNATURE);
        RT_CASE_RET_STR(LC_SEGMENT_SPLIT_INFO);
        RT_CASE_RET_STR(LC_REEXPORT_DYLIB);
        RT_CASE_RET_STR(LC_LAZY_LOAD_DYLIB);
        RT_CASE_RET_STR(LC_ENCRYPTION_INFO);
        RT_CASE_RET_STR(LC_DYLD_INFO);
        RT_CASE_RET_STR(LC_DYLD_INFO_ONLY);
        RT_CASE_RET_STR(LC_LOAD_UPWARD_DYLIB);
        RT_CASE_RET_STR(LC_VERSION_MIN_MACOSX);
        RT_CASE_RET_STR(LC_VERSION_MIN_IPHONEOS);
        RT_CASE_RET_STR(LC_FUNCTION_STARTS);
        RT_CASE_RET_STR(LC_DYLD_ENVIRONMENT);
        RT_CASE_RET_STR(LC_MAIN);
        RT_CASE_RET_STR(LC_DATA_IN_CODE);
        RT_CASE_RET_STR(LC_SOURCE_VERSION);
        RT_CASE_RET_STR(LC_DYLIB_CODE_SIGN_DRS);
        RT_CASE_RET_STR(LC_ENCRYPTION_INFO_64);
        RT_CASE_RET_STR(LC_LINKER_OPTION);
        RT_CASE_RET_STR(LC_LINKER_OPTIMIZATION_HINT);
        RT_CASE_RET_STR(LC_VERSION_MIN_TVOS);
        RT_CASE_RET_STR(LC_VERSION_MIN_WATCHOS);
        RT_CASE_RET_STR(LC_NOTE);
        RT_CASE_RET_STR(LC_BUILD_VERSION);
    }
    return "??";
}


static const char *dbgcMachoProt(uint32_t fProt)
{
    switch (fProt)
    {
        case VM_PROT_NONE:                                      return "---";
        case VM_PROT_READ:                                      return "r--";
        case VM_PROT_READ | VM_PROT_WRITE:                      return "rw-";
        case VM_PROT_READ | VM_PROT_EXECUTE:                    return "r-x";
        case VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE:    return "rwx";
        case VM_PROT_WRITE:                                     return "-w-";
        case VM_PROT_WRITE | VM_PROT_EXECUTE:                   return "-wx";
        case VM_PROT_EXECUTE:                                   return "-w-";
    }
    return "???";
}


static int dbgcDumpImageMachO(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PCDBGCVAR pImageBase, const char *pszName,
                              mach_header_64_t const *pHdr)
{
#define ENTRY(a_Define)  { a_Define, #a_Define }
    RT_NOREF_PV(pCmd);

    /*
     * Header:
     */
    DBGCCmdHlpPrintf(pCmdHlp, "%s: Mach-O image (%s bit) - %s (%u) - %s (%#x / %#x)\n",
                     pszName, pHdr->magic == IMAGE_MACHO64_SIGNATURE ? "64" : "32",
                     dbgcMachoFileType(pHdr->filetype), pHdr->filetype,
                     dbgcMachoCpuType(pHdr->cputype, pHdr->cpusubtype), pHdr->cputype, pHdr->cpusubtype);

    DBGCCmdHlpPrintf(pCmdHlp, "%s: Flags: %#x", pszName, pHdr->flags);
    static DBGCDUMPFLAGENTRY const s_aHdrFlags[] =
    {
        FLENT(MH_NOUNDEFS),                     FLENT(MH_INCRLINK),
        FLENT(MH_DYLDLINK),                     FLENT(MH_BINDATLOAD),
        FLENT(MH_PREBOUND),                     FLENT(MH_SPLIT_SEGS),
        FLENT(MH_LAZY_INIT),                    FLENT(MH_TWOLEVEL),
        FLENT(MH_FORCE_FLAT),                   FLENT(MH_NOMULTIDEFS),
        FLENT(MH_NOFIXPREBINDING),              FLENT(MH_PREBINDABLE),
        FLENT(MH_ALLMODSBOUND),                 FLENT(MH_SUBSECTIONS_VIA_SYMBOLS),
        FLENT(MH_CANONICAL),                    FLENT(MH_WEAK_DEFINES),
        FLENT(MH_BINDS_TO_WEAK),                FLENT(MH_ALLOW_STACK_EXECUTION),
        FLENT(MH_ROOT_SAFE),                    FLENT(MH_SETUID_SAFE),
        FLENT(MH_NO_REEXPORTED_DYLIBS),         FLENT(MH_PIE),
        FLENT(MH_DEAD_STRIPPABLE_DYLIB),        FLENT(MH_HAS_TLV_DESCRIPTORS),
        FLENT(MH_NO_HEAP_EXECUTION),
    };
    dbgcDumpImageFlags32(pCmdHlp, pHdr->flags, s_aHdrFlags, RT_ELEMENTS(s_aHdrFlags));
    DBGCCmdHlpPrintf(pCmdHlp, "\n");
    if (pHdr->reserved != 0 && pHdr->magic == IMAGE_MACHO64_SIGNATURE)
        DBGCCmdHlpPrintf(pCmdHlp, "%s: Reserved header field: %#x\n", pszName, pHdr->reserved);

    /*
     * And now the load commands.
     */
    const uint32_t cCmds  = pHdr->ncmds;
    const uint32_t cbCmds = pHdr->sizeofcmds;
    DBGCCmdHlpPrintf(pCmdHlp, "%s: %u load commands covering %#x bytes:\n", pszName, cCmds, cbCmds);
    if (cbCmds > _16M)
        return DBGCCmdHlpFailRc(pCmdHlp, pCmd, VERR_OUT_OF_RANGE,
                                "%s: Commands too big: %#x bytes, max 16MiB\n", pszName, cbCmds);


    /* Read the commands into a temp buffer: */
    const uint32_t cbHdr = pHdr->magic == IMAGE_MACHO64_SIGNATURE ? sizeof(mach_header_64_t) : sizeof(mach_header_32_t);
    uint8_t *pbCmds = (uint8_t *)RTMemTmpAllocZ(cbCmds);
    if (!pbCmds)
        return VERR_NO_TMP_MEMORY;

    int rc = dumpReadAt(pCmdHlp, pCmd, pImageBase, pszName, cbHdr, pbCmds, cbCmds, NULL);
    if (RT_SUCCESS(rc))
    {
        static const DBGCDUMPFLAGENTRY s_aSegFlags[] =
        { FLENT(SG_HIGHVM), FLENT(SG_FVMLIB), FLENT(SG_NORELOC), FLENT(SG_PROTECTED_VERSION_1), };

        /*
         * Iterate the commands.
         */
        uint32_t offCmd = 0;
        for (uint32_t iCmd = 0; iCmd < cCmds; iCmd++)
        {
            load_command_t const *pCurCmd = (load_command_t const *)&pbCmds[offCmd];
            const uint32_t cbCurCmd = offCmd + sizeof(*pCurCmd) <= cbCmds ? pCurCmd->cmdsize : sizeof(*pCurCmd);
            if (offCmd + cbCurCmd > cbCmds)
            {
                rc = DBGCCmdHlpFailRc(pCmdHlp, pCmd, VERR_OUT_OF_RANGE,
                                      "%s: Load command #%u (offset %#x + %#x) is out of bounds! cmdsize=%u (%#x) cmd=%u\n",
                                      pszName, iCmd, offCmd, cbHdr, cbCurCmd, cbCurCmd,
                                      offCmd + RT_UOFFSET_AFTER(load_command_t, cmd) <= cbCmds ? pCurCmd->cmd : UINT32_MAX);
                break;
            }

            DBGCCmdHlpPrintf(pCmdHlp, "%s: Load command #%u (offset %#x + %#x): %s (%u) LB %u\n",
                             pszName, iCmd, offCmd, cbHdr, dbgcMachoLoadCommand(pCurCmd->cmd), pCurCmd->cmd, cbCurCmd);
            switch (pCurCmd->cmd)
            {
                case LC_SEGMENT_64:
                    if (cbCurCmd < sizeof(segment_command_64_t))
                        rc = DBGCCmdHlpFailRc(pCmdHlp, pCmd, VERR_LDRMACHO_BAD_LOAD_COMMAND,
                                              "%s: LC_SEGMENT64 is too short!\n", pszName);
                    else
                    {
                        segment_command_64_t const *pSeg = (segment_command_64_t const *)pCurCmd;
                        DBGCCmdHlpPrintf(pCmdHlp, "%s:   vmaddr: %016RX64 LB %08RX64  prot: %s(%x)  maxprot: %s(%x)  name: %.16s\n",
                                         pszName, pSeg->vmaddr, pSeg->vmsize, dbgcMachoProt(pSeg->initprot), pSeg->initprot,
                                         dbgcMachoProt(pSeg->maxprot), pSeg->maxprot, pSeg->segname);
                        DBGCCmdHlpPrintf(pCmdHlp, "%s:   file:   %016RX64 LB %08RX64  sections: %2u  flags: %#x",
                                         pszName, pSeg->fileoff, pSeg->filesize, pSeg->nsects, pSeg->flags);
                        dbgcDumpImageFlags32(pCmdHlp, pSeg->flags, s_aSegFlags, RT_ELEMENTS(s_aSegFlags));
                        DBGCCmdHlpPrintf(pCmdHlp, "\n");
                        if (   pSeg->nsects > _64K
                            || pSeg->nsects * sizeof(section_64_t) + sizeof(pSeg) > cbCurCmd)
                            rc = DBGCCmdHlpFailRc(pCmdHlp, pCmd, VERR_LDRMACHO_BAD_LOAD_COMMAND,
                                                  "%s: LC_SEGMENT64 is too short for all the sections!\n", pszName);
                        else
                        {
                            section_64_t const *paSec = (section_64_t const *)(pSeg + 1);
                            for (uint32_t iSec = 0; iSec < pSeg->nsects; iSec++)
                            {
                                DBGCCmdHlpPrintf(pCmdHlp,
                                                 "%s:   Section #%u: %016RX64 LB %08RX64  align: 2**%-2u  name: %.16s",
                                                 pszName, iSec, paSec[iSec].addr, paSec[iSec].size, paSec[iSec].align,
                                                 paSec[iSec].sectname);
                                if (strncmp(pSeg->segname, paSec[iSec].segname, sizeof(pSeg->segname)))
                                    DBGCCmdHlpPrintf(pCmdHlp, "(in %.16s)", paSec[iSec].segname);
                                DBGCCmdHlpPrintf(pCmdHlp, "\n");

                                /// @todo Good night!
                                ///    uint32_t            offset;
                                ///    uint32_t            reloff;
                                ///    uint32_t            nreloc;
                                ///    uint32_t            flags;
                                ///    /** For S_LAZY_SYMBOL_POINTERS, S_NON_LAZY_SYMBOL_POINTERS and S_SYMBOL_STUBS
                                ///     * this is the index into the indirect symbol table. */
                                ///    uint32_t            reserved1;
                                ///    uint32_t            reserved2;
                                ///    uint32_t            reserved3;
                                ///
                            }
                        }
                    }
                    break;
            }

            /* Advance: */
            offCmd += cbCurCmd;
        }
    }
    RTMemTmpFree(pbCmds);
    return rc;
#undef ENTRY
}


/**
 * Common worker for the dumpimage command and the VBoxDumpImage tool.
 */
static int dumpImageCommon(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PCDBGCVAR pImageBase, const char *pszImageBaseAddr,
                           const char *pszName)
{
    union
    {
        uint8_t             ab[0x10];
        IMAGE_DOS_HEADER    DosHdr;
        struct
        {
            uint32_t            u32Magic;
            IMAGE_FILE_HEADER   FileHdr;
        } Nt;
        mach_header_64_t    MachO64;
    } uBuf;
    int rc = dumpReadAt(pCmdHlp, pCmd, pImageBase, pszName, 0, &uBuf.DosHdr, sizeof(uBuf.DosHdr), NULL);
    if (RT_SUCCESS(rc))
    {
        /*
         * MZ.
         */
        if (uBuf.DosHdr.e_magic == IMAGE_DOS_SIGNATURE)
        {
            uint32_t offNewHdr = uBuf.DosHdr.e_lfanew;
            if (offNewHdr < _256K && offNewHdr >= 16)
            {
                /* Look for new header. */
                rc = dumpReadAt(pCmdHlp, pCmd, pImageBase, pszName, offNewHdr, &uBuf.Nt, sizeof(uBuf.Nt), NULL);
                if (RT_SUCCESS(rc))
                {
                    /* PE: */
                    if (uBuf.Nt.u32Magic == IMAGE_NT_SIGNATURE)
                        rc = dbgcDumpImagePe(pCmd, pCmdHlp, pImageBase, pszImageBaseAddr, pszName, offNewHdr, &uBuf.Nt.FileHdr);
                    else
                        rc = DBGCCmdHlpFail(pCmdHlp, pCmd, "%s: Unknown new header magic: %.8Rhxs\n", pszName, uBuf.ab);
                }
            }
            else
                rc = DBGCCmdHlpFail(pCmdHlp, pCmd, "%s: MZ header but e_lfanew=%#RX32 is out of bounds (16..256K).\n",
                                    pszName, offNewHdr);
        }
        /*
         * ELF.
         */
        else if (uBuf.ab[0] == ELFMAG0 && uBuf.ab[1] == ELFMAG1 && uBuf.ab[2] == ELFMAG2 && uBuf.ab[3] == ELFMAG3)
            rc = dbgcDumpImageElf(pCmd, pCmdHlp, pImageBase, pszImageBaseAddr);
        /*
         * Mach-O.
         */
        else if (   uBuf.MachO64.magic == IMAGE_MACHO64_SIGNATURE
                 || uBuf.MachO64.magic == IMAGE_MACHO32_SIGNATURE )
            rc = dbgcDumpImageMachO(pCmd, pCmdHlp, pImageBase, pszImageBaseAddr, &uBuf.MachO64);
        /*
         * Dunno.
         */
        else
            rc = DBGCCmdHlpFail(pCmdHlp, pCmd, "%s: Unknown magic: %.8Rhxs\n", pszName, uBuf.ab);
    }
    else
        rc = DBGCCmdHlpFailRc(pCmdHlp, pCmd, rc, "%s: Failed to read %zu", pszName, sizeof(uBuf.DosHdr));
    return rc;
}


#ifndef DBGC_DUMP_IMAGE_TOOL

/**
 * @callback_method_impl{FNDBGCCMD, The 'dumpimage' command.}
 */
DECLCALLBACK(int) dbgcCmdDumpImage(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PUVM pUVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    int rcRet = VINF_SUCCESS;
    for (unsigned iArg = 0; iArg < cArgs; iArg++)
    {
        DBGCVAR const ImageBase = paArgs[iArg];
        char          szImageBaseAddr[64];
        DBGCCmdHlpStrPrintf(pCmdHlp, szImageBaseAddr, sizeof(szImageBaseAddr), "%Dv", &ImageBase);
        int rc = dumpImageCommon(pCmd, pCmdHlp, &ImageBase, szImageBaseAddr, szImageBaseAddr);
        if (RT_FAILURE(rc) && RT_SUCCESS(rcRet))
            rcRet = rc;
    }
    RT_NOREF(pUVM);
    return rcRet;
}

#else /* DBGC_DUMP_IMAGE_TOOL */


/**
 * @interface_member_impl{DBGCCMDHLP,pfnPrintfV}
 */
static DECLCALLBACK(int) toolPrintfV(PDBGCCMDHLP pCmdHlp, size_t *pcbWritten, const char *pszFormat, va_list args)
{
    int rc = RTPrintfV(pszFormat, args);
    if (rc >= 0)
    {
        if (pcbWritten)
            *pcbWritten = (unsigned)rc;
        return VINF_SUCCESS;
    }
    if (pcbWritten)
        *pcbWritten = 0;
    RT_NOREF(pCmdHlp);
    return VERR_IO_GEN_FAILURE;
}


/**
 * @interface_member_impl{DBGCCMDHLP,pfnFailV}
 */
static DECLCALLBACK(int) toolFailV(PDBGCCMDHLP pCmdHlp, PCDBGCCMD pCmd, const char *pszFormat, va_list va)
{
    CMDHLPSTATE * const pCmdHlpState = RT_FROM_MEMBER(pCmdHlp, CMDHLPSTATE, Core);
    pCmdHlpState->rcExit = RTEXITCODE_FAILURE;
    RTMsgErrorV(pszFormat, va);
    RT_NOREF(pCmd);
    return VERR_GENERAL_FAILURE;
}


/**
 * @interface_member_impl{DBGCCMDHLP,pfnFailRcV}
 */
static DECLCALLBACK(int) toolFailRcV(PDBGCCMDHLP pCmdHlp, PCDBGCCMD pCmd, int rc, const char *pszFormat, va_list va)
{
    CMDHLPSTATE * const pCmdHlpState = RT_FROM_MEMBER(pCmdHlp, CMDHLPSTATE, Core);
    pCmdHlpState->rcExit = RTEXITCODE_FAILURE;

    va_list vaCopy;
    va_copy(vaCopy, va);
    RTMsgError("%N: %Rrc", pszFormat, &vaCopy, rc);
    va_end(vaCopy);

    RT_NOREF(pCmd);
    return rc;
}


int main(int argc, char **argv)
{
    int rc = RTR3InitExe(argc, &argv, 0);
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);

    /*
     * Setup image helper code.
     */
    CMDHLPSTATE ToolState;
    RT_ZERO(ToolState);
    ToolState.Core.pfnPrintfV = toolPrintfV;
    ToolState.Core.pfnFailV   = toolFailV;
    ToolState.Core.pfnFailRcV = toolFailRcV;
    ToolState.hVfsFile        = NIL_RTVFSFILE;
    ToolState.rcExit          = RTEXITCODE_SUCCESS;

    char szImageBaseAddr[32] = {0};

    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--image-base", 'b', RTGETOPT_REQ_UINT64 | RTGETOPT_FLAG_HEX },
    };

    RTGETOPTSTATE GetState;
    rc = RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 1, RTGETOPTINIT_FLAGS_OPTS_FIRST);
    AssertRCReturn(rc, RTEXITCODE_FAILURE);

    RTGETOPTUNION ValueUnion;
    int           chOpt;
    while ((chOpt = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (chOpt)
        {
            case 'b':
                if (ValueUnion.u64 >= UINT32_MAX - _16M)
                    RTStrPrintf(szImageBaseAddr, sizeof(szImageBaseAddr), "%#018RX64", ValueUnion.u64);
                else
                    RTStrPrintf(szImageBaseAddr, sizeof(szImageBaseAddr), "%#010RX64", ValueUnion.u64);
                break;

            case 'V':
                RTPrintf("%s\n", RTBldCfgRevision());
                return RTEXITCODE_SUCCESS;

            case 'h':
                RTPrintf("usage: %s [options] <file> [file2..]\n", RTProcShortName());
                return RTEXITCODE_SUCCESS;

            case VINF_GETOPT_NOT_OPTION:
            {
                RTERRINFOSTATIC ErrInfo;
                uint32_t        offError = 0;
                rc = RTVfsChainOpenFile(ValueUnion.psz, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE,
                                        &ToolState.hVfsFile, &offError, RTErrInfoInitStatic(&ErrInfo));
                if (RT_SUCCESS(rc))
                {
                    rc = dumpImageCommon(NULL, &ToolState.Core, NULL, szImageBaseAddr[0] ? szImageBaseAddr : NULL, ValueUnion.psz);
                    if (RT_FAILURE(rc) && ToolState.rcExit == RTEXITCODE_SUCCESS)
                        ToolState.rcExit = RTEXITCODE_FAILURE;
                    RTVfsFileRelease(ToolState.hVfsFile);
                }
                else
                    ToolState.rcExit = RTVfsChainMsgErrorExitFailure("RTVfsChainOpenFile", ValueUnion.psz,
                                                                     rc, offError, &ErrInfo.Core);
                ToolState.hVfsFile = NIL_RTVFSFILE;
                break;
            }

            default:
                return RTGetOptPrintError(chOpt, &ValueUnion);
        }
    }

    return ToolState.rcExit;

}
#endif /* !DBGC_DUMP_IMAGE_TOOL */

