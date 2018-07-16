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
#include <iprt/formats/codeview.h>

#include "DBGCInternal.h"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * PE dumper instance.
 */
typedef struct DUMPIMAGEPE
{
    /** Pointer to the image base address variable. */
    PCDBGCVAR                   pImageBase;
    /** Pointer to the file header. */
    PCIMAGE_FILE_HEADER         pFileHdr;
    /** Pointer to the NT headers. */
    union
    {
        PCIMAGE_NT_HEADERS32    pNt32;
        PCIMAGE_NT_HEADERS64    pNt64;
        void                   *pv;
    } u;
    /** Pointer to the section headers. */
    PCIMAGE_SECTION_HEADER      paShdrs;
    /** Number of section headers. */
    unsigned                    cShdrs;
    /** Number of RVA and sizes (data directory entries). */
    unsigned                    cDataDir;
    /** Pointer to the data directory. */
    PCIMAGE_DATA_DIRECTORY      paDataDir;

    /** The command descriptor (for failing the command). */
    PCDBGCCMD                   pCmd;
} DUMPIMAGEPE;
/** Pointer to a PE dumper instance. */
typedef DUMPIMAGEPE *PDUMPIMAGEPE;


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


static const char *dbgPeDebugTypeName(uint32_t uType)
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
    return "??";
}


static int dbgcDumpImagePeDebugDir(PDUMPIMAGEPE pThis, PDBGCCMDHLP pCmdHlp, PCDBGCVAR pDataAddr, uint32_t cbData)
{
    uint32_t cEntries = cbData / sizeof(IMAGE_DEBUG_DIRECTORY);
    for (uint32_t i = 0; i < cEntries; i++)
    {
        /*
         * Read the entry into memory.
         */
        DBGCVAR DbgDirAddr;
        int rc = DBGCCmdHlpEval(pCmdHlp, &DbgDirAddr, "%DV + %#RX32", pDataAddr, i * sizeof(IMAGE_DEBUG_DIRECTORY));
        if (RT_FAILURE(rc))
            return DBGCCmdHlpFailRc(pCmdHlp, pThis->pCmd, rc, "DBGCCmdHlpEval failed on debug entry %u", i);

        IMAGE_DEBUG_DIRECTORY DbgDir;
        rc = DBGCCmdHlpMemRead(pCmdHlp, &DbgDir, sizeof(DbgDir), &DbgDirAddr, NULL);
        if (RT_FAILURE(rc))
            return DBGCCmdHlpFailRc(pCmdHlp, pThis->pCmd, rc, "Failed to read %zu at %Dv", sizeof(DbgDir), &DbgDirAddr);

        /*
         * Dump it.
         */
        DBGCVAR DebugDataAddr = *pThis->pImageBase;
        rc = DBGCCmdHlpEval(pCmdHlp, &DebugDataAddr, "%DV + %#RX32", pThis->pImageBase, DbgDir.AddressOfRawData);
        DBGCCmdHlpPrintf(pCmdHlp, "  Debug[%u]: %Dv/%08RX32 LB %06RX32 %u (%s) v%u.%u file=%RX32 ts=%08RX32 fl=%RX32\n",
                         i, &DebugDataAddr, DbgDir.AddressOfRawData, DbgDir.SizeOfData, DbgDir.Type,
                         dbgPeDebugTypeName(DbgDir.Type), DbgDir.MajorVersion, DbgDir.MinorVersion, DbgDir.PointerToRawData,
                         DbgDir.TimeDateStamp, DbgDir.Characteristics);
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
                rc = DBGCCmdHlpMemRead(pCmdHlp, &uBuf, DbgDir.SizeOfData, &DebugDataAddr, NULL);
                if (RT_FAILURE(rc))
                    return DBGCCmdHlpFailRc(pCmdHlp, pThis->pCmd, rc, "Failed to read %zu at %Dv",
                                            DbgDir.SizeOfData, &DebugDataAddr);

                if (   uBuf.Pdb20.u32Magic   == CVPDB20INFO_MAGIC
                    && uBuf.Pdb20.offDbgInfo == 0
                    && DbgDir.SizeOfData > RT_UOFFSETOF(CVPDB20INFO, szPdbFilename) )
                    DBGCCmdHlpPrintf(pCmdHlp, "    PDB2.0: ts=%08RX32 age=%RX32 %s\n",
                                     uBuf.Pdb20.uTimestamp, uBuf.Pdb20.uAge, uBuf.Pdb20.szPdbFilename);
                else if (   uBuf.Pdb20.u32Magic == CVPDB70INFO_MAGIC
                         && DbgDir.SizeOfData > RT_UOFFSETOF(CVPDB70INFO, szPdbFilename) )
                    DBGCCmdHlpPrintf(pCmdHlp, "    PDB7.0: %RTuuid age=%u %s\n",
                                     &uBuf.Pdb70.PdbUuid, uBuf.Pdb70.uAge, uBuf.Pdb70.szPdbFilename);
                else
                    DBGCCmdHlpPrintf(pCmdHlp, "    Unknown PDB/codeview magic: %.8Rhxs\n", uBuf.abPage);
            }
        }
        else if (DbgDir.Type == IMAGE_DEBUG_TYPE_MISC)
        {
            if (   DbgDir.SizeOfData < sizeof(uBuf)
                && DbgDir.SizeOfData > RT_UOFFSETOF(IMAGE_DEBUG_MISC, Data)
                && DbgDir.AddressOfRawData > 0
                && RT_SUCCESS(rc) )
            {
                rc = DBGCCmdHlpMemRead(pCmdHlp, &uBuf, DbgDir.SizeOfData, &DebugDataAddr, NULL);
                if (RT_FAILURE(rc))
                    return DBGCCmdHlpFailRc(pCmdHlp, pThis->pCmd, rc, "Failed to read %zu at %Dv",
                                            DbgDir.SizeOfData, &DebugDataAddr);

                if (   uBuf.Misc.DataType == IMAGE_DEBUG_MISC_EXENAME
                    && uBuf.Misc.Length   == DbgDir.SizeOfData)
                {
                    if (!uBuf.Misc.Unicode)
                        DBGCCmdHlpPrintf(pCmdHlp, "    Misc DBG: ts=%RX32 %s\n",
                                         DbgDir.TimeDateStamp, (const char *)&uBuf.Misc.Data[0]);
                    else
                        DBGCCmdHlpPrintf(pCmdHlp, "    Misc DBG: ts=%RX32 %ls\n",
                                         DbgDir.TimeDateStamp, (PCRTUTF16)&uBuf.Misc.Data[0]);
                }
            }
        }
    }
    return VINF_SUCCESS;
}


static int dbgcDumpImagePeDataDirs(PDUMPIMAGEPE pThis, PDBGCCMDHLP pCmdHlp, unsigned cDataDirs, PCIMAGE_DATA_DIRECTORY paDataDirs)
{
    int rcRet = VINF_SUCCESS;
    for (unsigned i = 0; i < cDataDirs; i++)
    {
        if (paDataDirs[i].Size || paDataDirs[i].VirtualAddress)
        {
            DBGCVAR DataAddr = *pThis->pImageBase;
            DBGCCmdHlpEval(pCmdHlp, &DataAddr, "%DV + %#RX32", pThis->pImageBase, paDataDirs[i].VirtualAddress);
            DBGCCmdHlpPrintf(pCmdHlp, "DataDir[%02u]: %Dv/%08RX32 LB %08RX32 %s\n",
                             i, &DataAddr, paDataDirs[i].VirtualAddress, paDataDirs[i].Size, dbgcPeDataDirName(i));
            int rc = VINF_SUCCESS;
            if (   i == IMAGE_DIRECTORY_ENTRY_DEBUG
                && paDataDirs[i].Size >= sizeof(IMAGE_DEBUG_DIRECTORY))
                rc = dbgcDumpImagePeDebugDir(pThis, pCmdHlp, &DataAddr, paDataDirs[i].Size);
            if (RT_FAILURE(rc) && RT_SUCCESS(rcRet))
                rcRet = rc;
        }
    }
    return rcRet;
}


static int dbgcDumpImagePeSectionHdrs(PDUMPIMAGEPE pThis, PDBGCCMDHLP pCmdHlp, unsigned cShdrs, PCIMAGE_SECTION_HEADER paShdrs)
{
    for (unsigned i = 0; i < cShdrs; i++)
    {
        DBGCVAR SectAddr = *pThis->pImageBase;
        DBGCCmdHlpEval(pCmdHlp, &SectAddr, "%DV + %#RX32", pThis->pImageBase, paShdrs[i].VirtualAddress);
        DBGCCmdHlpPrintf(pCmdHlp, "Section[%02u]: %Dv/%08RX32 LB %08RX32 %.8s\n",
                         i, &SectAddr, paShdrs[i].VirtualAddress, paShdrs[i].Misc.VirtualSize,  paShdrs[i].Name);
    }
    return VINF_SUCCESS;
}


static int dbgcDumpImagePeOptHdr32(PDUMPIMAGEPE pThis, PDBGCCMDHLP pCmdHlp, PCIMAGE_NT_HEADERS32 pNtHdrs)
{
    RT_NOREF(pThis, pCmdHlp, pNtHdrs);
    return VINF_SUCCESS;
}

static int dbgcDumpImagePeOptHdr64(PDUMPIMAGEPE pThis, PDBGCCMDHLP pCmdHlp, PCIMAGE_NT_HEADERS64 pNtHdrs)
{
    RT_NOREF(pThis, pCmdHlp, pNtHdrs);
    return VINF_SUCCESS;
}


static int dbgcDumpImagePe(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PCDBGCVAR pImageBase,
                           PCDBGCVAR pPeHdrAddr, PCIMAGE_FILE_HEADER pFileHdr)
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
        DUMPIMAGEPE This;
        RT_ZERO(This);
        This.pImageBase = pImageBase;
        This.pFileHdr   = pFileHdr;
        This.u.pv       = pvBuf;
        This.cShdrs     = pFileHdr->NumberOfSections;
        This.paShdrs    = (PCIMAGE_SECTION_HEADER)((uintptr_t)pvBuf + offSHdrs);
        This.pCmd       = pCmd;

        if (pFileHdr->SizeOfOptionalHeader == sizeof(IMAGE_OPTIONAL_HEADER32))
        {
            This.paDataDir = This.u.pNt32->OptionalHeader.DataDirectory;
            This.cDataDir  = This.u.pNt32->OptionalHeader.NumberOfRvaAndSizes;
            rc = dbgcDumpImagePeOptHdr32(&This, pCmdHlp, This.u.pNt32);
        }
        else if (pFileHdr->SizeOfOptionalHeader == sizeof(IMAGE_OPTIONAL_HEADER64))
        {
            This.paDataDir = This.u.pNt64->OptionalHeader.DataDirectory;
            This.cDataDir  = This.u.pNt64->OptionalHeader.NumberOfRvaAndSizes;
            rc = dbgcDumpImagePeOptHdr64(&This, pCmdHlp, This.u.pNt64);
        }
        else
            rc = DBGCCmdHlpFail(pCmdHlp, pCmd, "%Dv: Unsupported optional header size: %#x\n",
                                pImageBase, pFileHdr->SizeOfOptionalHeader);

        int rc2 = dbgcDumpImagePeSectionHdrs(&This, pCmdHlp, This.cShdrs, This.paShdrs);
        if (RT_FAILURE(rc2) && RT_SUCCESS(rc))
            rc = rc2;

        rc2 = dbgcDumpImagePeDataDirs(&This, pCmdHlp, This.cDataDir, This.paDataDir);
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

