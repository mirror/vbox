/* $Id$ */
/** @file
 * DBGC - Debugger Console, Native Commands.
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_DBGC
#include <VBox/dbg.h>
#include <VBox/vmm/dbgf.h>
#include <VBox/param.h>
#include <VBox/err.h>
#include <VBox/log.h>

#include <iprt/assert.h>
#include <iprt/ctype.h>
#include <iprt/dir.h>
#include <iprt/env.h>
#include <iprt/ldr.h>
#include <iprt/mem.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/formats/mz.h>
#include <iprt/formats/pecoff.h>
#include <iprt/formats/elf32.h>
#include <iprt/formats/elf64.h>

#include "DBGCInternal.h"


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
extern FNDBGCCMD dbgcCmdDumpImage; /* See DBGCCommands.cpp. */


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
    return "";
}

static int dbgcDumpImagePeSectionHdrs(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PCDBGCVAR pImageBase,
                                      unsigned cShdrs, IMAGE_SECTION_HEADER const *paShdrs)
{
    for (unsigned i = 0; i < cShdrs; i++)
    {
        DBGCVAR SectAddr = *pImageBase;
        DBGCCmdHlpEval(pCmdHlp, &SectAddr, "%DV + %#RX32", pImageBase, paShdrs[i].VirtualAddress);
        DBGCCmdHlpPrintf(pCmdHlp, "Section #%u: %Dv/%08RX32 LB %08RX32 %.8s\n",
                         i, &SectAddr, paShdrs[i].VirtualAddress, paShdrs[i].Misc.VirtualSize,  paShdrs[i].Name);
    }

    RT_NOREF(pCmd);
    return VINF_SUCCESS;
}


static int dbgcDumpImagePeOptHdr32(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, IMAGE_NT_HEADERS32 const *pNtHdrs)
{
    RT_NOREF(pCmd, pCmdHlp, pNtHdrs);
    return VINF_SUCCESS;
}

static int dbgcDumpImagePeOptHdr64(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, IMAGE_NT_HEADERS64 const *pNtHdrs)
{
    RT_NOREF(pCmd, pCmdHlp, pNtHdrs);
    return VINF_SUCCESS;
}


static int dbgcDumpImagePe(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PCDBGCVAR pImageBase, PCDBGCVAR pPeHdrAddr,
                           IMAGE_FILE_HEADER const *pFileHdr)
{
    /*
     * Dump file header fields.
     */
    DBGCCmdHlpPrintf(pCmdHlp, "%Dv: PE image - %#x (%s), %u sections\n", pImageBase, pFileHdr->Machine,
                     dbgcPeMachineName(pFileHdr->Machine), pFileHdr->NumberOfSections);
    DBGCCmdHlpPrintf(pCmdHlp, "Characteristics: %#06x", pFileHdr->Characteristics);
    if (pFileHdr->Characteristics & IMAGE_FILE_RELOCS_STRIPPED)         DBGCCmdHlpPrintf(pCmdHlp, " RELOCS_STRIPPED");
    if (pFileHdr->Characteristics & IMAGE_FILE_EXECUTABLE_IMAGE)        DBGCCmdHlpPrintf(pCmdHlp, " EXECUTABLE_IMAGE");
    if (pFileHdr->Characteristics & IMAGE_FILE_LINE_NUMS_STRIPPED)      DBGCCmdHlpPrintf(pCmdHlp, " LINE_NUMS_STRIPPED");
    if (pFileHdr->Characteristics & IMAGE_FILE_LOCAL_SYMS_STRIPPED)     DBGCCmdHlpPrintf(pCmdHlp, " LOCAL_SYMS_STRIPPED");
    if (pFileHdr->Characteristics & IMAGE_FILE_AGGRESIVE_WS_TRIM)       DBGCCmdHlpPrintf(pCmdHlp, " AGGRESIVE_WS_TRIM");
    if (pFileHdr->Characteristics & IMAGE_FILE_LARGE_ADDRESS_AWARE)     DBGCCmdHlpPrintf(pCmdHlp, " LARGE_ADDRESS_AWARE");
    if (pFileHdr->Characteristics & IMAGE_FILE_16BIT_MACHINE)           DBGCCmdHlpPrintf(pCmdHlp, " 16BIT_MACHINE");
    if (pFileHdr->Characteristics & IMAGE_FILE_BYTES_REVERSED_LO)       DBGCCmdHlpPrintf(pCmdHlp, " BYTES_REVERSED_LO");
    if (pFileHdr->Characteristics & IMAGE_FILE_32BIT_MACHINE)           DBGCCmdHlpPrintf(pCmdHlp, " 32BIT_MACHINE");
    if (pFileHdr->Characteristics & IMAGE_FILE_DEBUG_STRIPPED)          DBGCCmdHlpPrintf(pCmdHlp, " DEBUG_STRIPPED");
    if (pFileHdr->Characteristics & IMAGE_FILE_REMOVABLE_RUN_FROM_SWAP) DBGCCmdHlpPrintf(pCmdHlp, " REMOVABLE_RUN_FROM_SWAP");
    if (pFileHdr->Characteristics & IMAGE_FILE_NET_RUN_FROM_SWAP)       DBGCCmdHlpPrintf(pCmdHlp, " NET_RUN_FROM_SWAP");
    if (pFileHdr->Characteristics & IMAGE_FILE_SYSTEM)                  DBGCCmdHlpPrintf(pCmdHlp, " SYSTEM");
    if (pFileHdr->Characteristics & IMAGE_FILE_DLL)                     DBGCCmdHlpPrintf(pCmdHlp, " DLL");
    if (pFileHdr->Characteristics & IMAGE_FILE_UP_SYSTEM_ONLY)          DBGCCmdHlpPrintf(pCmdHlp, " UP_SYSTEM_ONLY");
    if (pFileHdr->Characteristics & IMAGE_FILE_BYTES_REVERSED_HI)       DBGCCmdHlpPrintf(pCmdHlp, " BYTES_REVERSED_HI");
    DBGCCmdHlpPrintf(pCmdHlp, "\n");

    /*
     * Allocate memory for all the headers, including section headers, and read them into memory.
     */
    size_t offSHdrs = pFileHdr->SizeOfOptionalHeader + sizeof(*pFileHdr) + sizeof(uint32_t);
    size_t cbHdrs = offSHdrs + pFileHdr->NumberOfSections * sizeof(IMAGE_SECTION_HEADER);
    if (cbHdrs > _2M)
        return DBGCCmdHlpFail(pCmdHlp, pCmd, "%Dv: headers too big: %zu.\n", pImageBase, cbHdrs);

    void *pvBuf = RTMemTmpAllocZ(cbHdrs);
    if (!pvBuf)
        return DBGCCmdHlpFail(pCmdHlp, pCmd, "%Dv: failed to allocate %zu bytes.\n", pImageBase, cbHdrs);
    int rc = DBGCCmdHlpMemRead(pCmdHlp, pvBuf, cbHdrs, pPeHdrAddr, NULL);
    if (RT_SUCCESS(rc))
    {
        if (pFileHdr->SizeOfOptionalHeader == sizeof(IMAGE_OPTIONAL_HEADER32))
            rc = dbgcDumpImagePeOptHdr32(pCmd, pCmdHlp, (IMAGE_NT_HEADERS32 const *)pvBuf);
        else if (pFileHdr->SizeOfOptionalHeader == sizeof(IMAGE_OPTIONAL_HEADER64))
            rc = dbgcDumpImagePeOptHdr64(pCmd, pCmdHlp, (IMAGE_NT_HEADERS64 const *)pvBuf);
        else
            rc = DBGCCmdHlpFail(pCmdHlp, pCmd, "%Dv: Unsupported optional header size: %#x\n",
                                pImageBase, pFileHdr->SizeOfOptionalHeader);
        int rc2 = dbgcDumpImagePeSectionHdrs(pCmd, pCmdHlp, pImageBase, pFileHdr->NumberOfSections,
                                             (IMAGE_SECTION_HEADER const *)((uintptr_t)pvBuf + offSHdrs));
        if (RT_FAILURE(rc2) && RT_SUCCESS(rc))
            rc = rc2;
    }
    else
        rc = DBGCCmdHlpFailRc(pCmdHlp, pCmd, rc, "%Dv: Failed to read %zu at %Dv", pImageBase, cbHdrs, pPeHdrAddr);
    RTMemTmpFree(pvBuf);
    return rc;
}


static int dbgcDumpImageElf(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PCDBGCVAR pImageBase)
{
    RT_NOREF_PV(pCmd);
    DBGCCmdHlpPrintf(pCmdHlp, "%Dv: ELF image dumping not implemented yet.\n", pImageBase);
    return VINF_SUCCESS;
}


/**
 * @callback_method_impl{FNDBGCCMD, The 'dumpimage' command.}
 */
DECLCALLBACK(int) dbgcCmdDumpImage(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PUVM pUVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    int rcRet = VINF_SUCCESS;
    for (unsigned iArg = 0; iArg < cArgs; iArg++)
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
        } uBuf;
        DBGCVAR const ImageBase = paArgs[iArg];
        int rc = DBGCCmdHlpMemRead(pCmdHlp, &uBuf.DosHdr, sizeof(uBuf.DosHdr), &ImageBase, NULL);
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
                    DBGCVAR NewHdrAddr;
                    rc = DBGCCmdHlpEval(pCmdHlp, &NewHdrAddr, "%DV + %#RX32", &ImageBase, offNewHdr);
                    if (RT_SUCCESS(rc))
                    {
                        rc = DBGCCmdHlpMemRead(pCmdHlp, &uBuf.Nt, sizeof(uBuf.Nt), &NewHdrAddr, NULL);
                        if (RT_SUCCESS(rc))
                        {
                            /* PE: */
                            if (uBuf.Nt.u32Magic == IMAGE_NT_SIGNATURE)
                                rc = dbgcDumpImagePe(pCmd, pCmdHlp, &ImageBase, &NewHdrAddr, &uBuf.Nt.FileHdr);
                            else
                                rc = DBGCCmdHlpFail(pCmdHlp, pCmd, "%Dv: Unknown new header magic: %.8Rhxs\n",
                                                    &ImageBase, uBuf.ab);
                        }
                        else
                            rc = DBGCCmdHlpFailRc(pCmdHlp, pCmd, rc, "%Dv: Failed to read %zu at %Dv",
                                                  &ImageBase, sizeof(uBuf.Nt), &NewHdrAddr);
                    }
                    else
                        rc = DBGCCmdHlpFailRc(pCmdHlp, pCmd, rc, "%Dv: Failed to calc address of new header", &ImageBase);
                }
                else
                    rc = DBGCCmdHlpFail(pCmdHlp, pCmd, "%Dv: MZ header but e_lfanew=%#RX32 is out of bounds (16..256K).\n",
                                        &ImageBase, offNewHdr);
            }
            /*
             * ELF.
             */
            else if (uBuf.ab[0] == ELFMAG0 && uBuf.ab[1] == ELFMAG1 && uBuf.ab[2] == ELFMAG2 && uBuf.ab[3] == ELFMAG3)
                rc = dbgcDumpImageElf(pCmd, pCmdHlp, &ImageBase);
            /*
             * Dunno.
             */
            else
                rc = DBGCCmdHlpFail(pCmdHlp, pCmd, "%Dv: Unknown magic: %.8Rhxs\n", &ImageBase, uBuf.ab);
        }
        else
            rc = DBGCCmdHlpFailRc(pCmdHlp, pCmd, rc, "%Dv: Failed to read %zu", &ImageBase, sizeof(uBuf.DosHdr));
        if (RT_FAILURE(rc) && RT_SUCCESS(rcRet))
            rcRet = rc;
    }
    RT_NOREF(pUVM);
    return rcRet;
}

