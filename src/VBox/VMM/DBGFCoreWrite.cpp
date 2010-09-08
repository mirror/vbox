/* $Id$ */
/** @file
 * DBGF - Debugger Facility, Guest Core Dump.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*
 * VBox VMCore Format:
 * [ ELF 64 Header]  -- Only 1
 *
 * [ PT_NOTE ]       -- Only 1
 *    - Offset into CoreDescriptor followed by list of Notes (Note Hdr + data) of VBox CPUs.
 *    - (Any Additional custom Note sections)
 *
 * [ PT_LOAD ]       -- One for each contiguous memory chunk
 *    - Memory offset
 *    - File offset
 *
 * CoreDescriptor
 *    - Magic, VBox version
 *    - Number of CPus
 *
 * Per-CPU register dump
 *    - CPU 1 Note Hdr + Data
 *    - CPU 2 Note Hdr + Data
 *    ...
 * (Additional custom notes Hdr+data)
 *    - VBox 1 Note Hdr + Data
 *    - VBox 2 Note Hdr + Data
 *    ...
 * Memory dump
 *
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DBGF
#include <iprt/param.h>
#include <iprt/file.h>

#include "DBGFInternal.h"

#include <VBox/cpum.h>
#include "CPUMInternal.h"
#include <VBox/dbgf.h>
#include <VBox/vm.h>
#include <VBox/pgm.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <VBox/mm.h>
#include <VBox/version.h>

#include "../Runtime/include/internal/ldrELF64.h"

/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#ifdef DEBUG_ramshankar
# undef Log
# define Log LogRel
#endif
#define DBGFLOG_NAME           "DBGFCoreWrite"

/*
 * For now use Solaris-specific padding and namesz length (i.e. includes NULL terminator)
 */
static const int s_NoteAlign  = 4;      /* @todo see #5211 comment 3 */
static const int s_cbNoteName = 16;
static const char *s_pcszCoreVBoxCore = "VBOXCORE";
static const char *s_pcszCoreVBoxCpu  = "VBOXCPU";

/**
 * DBGFCOREDATA: Core data.
 */
typedef struct
{
    const char *pszDumpPath;    /* File path to dump the core into. */
} DBGFCOREDATA, *PDBGFCOREDATA;

/**
 * ELF function to write 64-bit ELF header.
 *
 * @param hFile             The file to write to.
 * @param cProgHdrs         Number of program headers.
 * @param cSecHdrs          Number of section headers.
 * @param pcbElfHdr         Where to store the size of written header to file,
 *                          can be NULL.
 *
 * @return IPRT status code.
 */
static int Elf64WriteElfHdr(RTFILE hFile, uint16_t cProgHdrs, uint16_t cSecHdrs, uint64_t *pcbElfHdr)
{
    Elf64_Ehdr ElfHdr;
    RT_ZERO(ElfHdr);
    ElfHdr.e_ident[EI_MAG0]  = ELFMAG0;
    ElfHdr.e_ident[EI_MAG1]  = ELFMAG1;
    ElfHdr.e_ident[EI_MAG2]  = ELFMAG2;
    ElfHdr.e_ident[EI_MAG3]  = ELFMAG3;
    ElfHdr.e_ident[EI_DATA]  = ELFDATA2LSB;
    ElfHdr.e_type            = ET_CORE;
    ElfHdr.e_version         = EV_CURRENT;
    ElfHdr.e_ident[EI_CLASS] = ELFCLASS64;
    /* 32-bit VMs will produce cores with e_machine EM_386. */
#ifdef RT_ARCH_AMD64
    ElfHdr.e_machine         = EM_X86_64;
#else
    ElfHdr.e_machine         = EM_386;
#endif
    ElfHdr.e_phnum           = cProgHdrs;
    ElfHdr.e_shnum           = cSecHdrs;
    ElfHdr.e_ehsize          = sizeof(ElfHdr);
    ElfHdr.e_phoff           = sizeof(ElfHdr);
    ElfHdr.e_phentsize       = sizeof(Elf64_Phdr);
    ElfHdr.e_shentsize       = sizeof(Elf64_Shdr);

    int rc = RTFileWrite(hFile, &ElfHdr, sizeof(ElfHdr), NULL /* all */);
    if (RT_SUCCESS(rc) && pcbElfHdr)
        *pcbElfHdr = sizeof(ElfHdr);
    return rc;
}


/**
 * ELF function to write 64-bit program header.
 *
 * @param hFile             The file to write to.
 * @param Type              Type of program header (PT_*).
 * @param fFlags            Flags (access permissions, PF_*).
 * @param offFileData       File offset of contents.
 * @param cbFileData        Size of contents in the file.
 * @param cbMemData         Size of contents in memory.
 * @param Phys              Physical address, pass zero if not applicable.
 * @param pcbProgHdr        Where to store the size of written header to file,
 *                          can be NULL.
 *
 * @return IPRT status code.
 */
static int Elf64WriteProgHdr(RTFILE hFile, uint32_t Type, uint32_t fFlags, uint64_t offFileData, uint64_t cbFileData, uint64_t cbMemData,
                             RTGCPHYS Phys, uint64_t *pcbProgHdr)
{
    Elf64_Phdr ProgHdr;
    RT_ZERO(ProgHdr);
    ProgHdr.p_type          = Type;
    ProgHdr.p_flags         = fFlags;
    ProgHdr.p_offset        = offFileData;
    ProgHdr.p_filesz        = cbFileData;
    ProgHdr.p_memsz         = cbMemData;
    ProgHdr.p_paddr         = Phys;

    int rc = RTFileWrite(hFile, &ProgHdr, sizeof(ProgHdr), NULL /* all */);
    if (RT_SUCCESS(rc) && pcbProgHdr)
        *pcbProgHdr = sizeof(ProgHdr);
    return rc;
}


/**
 * Returns the size of the NOTE section given the name and size of the data.
 *
 * @param pszName           Name of the note section.
 * @param cb                Size of the data portion of the note section.
 *
 * @return The size of the NOTE section as rounded to the file alignment.
 */
static inline uint64_t Elf64NoteSectionSize(const char *pszName, uint64_t cbData)
{
    uint64_t cbNote = sizeof(Elf64_Nhdr);

    size_t cbName = strlen(pszName) + 1;
    size_t cbNameAlign = RT_ALIGN_Z(cbName, s_NoteAlign);

    cbNote += cbNameAlign;
    cbNote += RT_ALIGN_64(cbData, s_NoteAlign);
    return cbNote;
}


/**
 * Elf function to write 64-bit note header.
 *
 * @param hFile             The file to write to.
 * @param Type              Type of this section.
 * @param pszName           Name of this section.
 * @param pcv               Opaque pointer to the data, if NULL only computes size.
 * @param cbData            Size of the data.
 * @param pcbNoteHdr        Where to store the size of written header to file,
 *                          can be NULL.
 *
 * @return IPRT status code.
 */
static int Elf64WriteNoteHdr(RTFILE hFile, uint16_t Type, const char *pszName, const void *pcvData, uint64_t cbData, uint64_t *pcbNoteHdr)
{
    AssertReturn(pcvData, VERR_INVALID_POINTER);
    AssertReturn(cbData > 0, VERR_NO_DATA);

    char szNoteName[s_cbNoteName];
    RT_ZERO(szNoteName);
    RTStrCopy(szNoteName, sizeof(szNoteName), pszName);

    size_t cbName        = strlen(szNoteName) + 1;
    size_t cbNameAlign   = RT_ALIGN_Z(cbName, s_NoteAlign);
    uint64_t cbDataAlign = RT_ALIGN_64(cbData, s_NoteAlign);

    static const char s_achPad[7] = { 0, 0, 0, 0, 0, 0, 0 };
    AssertCompile(sizeof(s_achPad) >= s_NoteAlign - 1);

    Elf64_Nhdr ElfNoteHdr;
    RT_ZERO(ElfNoteHdr);
    ElfNoteHdr.n_namesz = (Elf64_Word)cbName;   /* @todo fix this later to NOT include NULL terminator */
    ElfNoteHdr.n_type   = Type;
    ElfNoteHdr.n_descsz = (Elf64_Word)cbDataAlign;

    /*
     * Write note header.
     */
    int rc = RTFileWrite(hFile, &ElfNoteHdr, sizeof(ElfNoteHdr), NULL /* all */);
    if (RT_SUCCESS(rc))
    {
        /*
         * Write note name.
         */
        rc = RTFileWrite(hFile, szNoteName, cbName, NULL /* all */);
        if (RT_SUCCESS(rc))
        {
            /*
             * Write note name padding if required.
             */
            if (cbNameAlign > cbName)
                rc = RTFileWrite(hFile, s_achPad, cbNameAlign - cbName, NULL);

            if (RT_SUCCESS(rc))
            {
                /*
                 * Write note data.
                 */
                rc = RTFileWrite(hFile, pcvData, cbData, NULL /* all */);
                if (RT_SUCCESS(rc))
                {
                    /*
                     * Write note data padding if required.
                     */
                    if (cbDataAlign > cbData)
                        rc = RTFileWrite(hFile, s_achPad, cbDataAlign - cbData, NULL /* all*/);
                }
            }
        }
    }

    if (RT_FAILURE(rc))
        LogRel((DBGFLOG_NAME ":RTFileWrite failed. rc=%Rrc pszName=%s cbData=%u cbDataAlign=%u\n", rc, pszName, cbData, cbDataAlign));

    return rc;
}


/**
 * Count the number of memory ranges that go into the core file.
 *
 * We cannot do a page-by-page dump of the entire guest memory as there will be
 * way too many program header entries. Also we don't want to dump MMIO regions
 * which means we cannot have a 1:1 mapping between core file offset and memory
 * offset. Instead we dump the memory in ranges. A memory range is a contiguous
 * memory area suitable for dumping to a core file.
 *
 * @param pVM               The VM handle.
 *
 * @return Number of memory ranges
 */
static uint32_t dbgfR3GetRamRangeCount(PVM pVM)
{
    return PGMR3PhysGetRamRangeCount(pVM);
}


/**
 * EMT Rendezvous worker function for DBGFR3CoreWrite.
 *
 * @param   pVM              The VM handle.
 * @param   pVCpu            The handle of the calling VCPU.
 * @param   pvData           Opaque data.
 *
 * @return VBox status code.
 */
static DECLCALLBACK(VBOXSTRICTRC) dbgfR3CoreWrite(PVM pVM, PVMCPU pVCpu, void *pvData)
{
    /*
     * Validate input.
     */
    AssertReturn(pVM, VERR_INVALID_VM_HANDLE);
    AssertReturn(pVCpu, VERR_INVALID_VMCPU_HANDLE);
    AssertReturn(pvData, VERR_INVALID_POINTER);

    PDBGFCOREDATA pDbgfData = (PDBGFCOREDATA)pvData;

    /*
     * Collect core information.
     */
    uint32_t u32MemRanges = dbgfR3GetRamRangeCount(pVM);
    uint16_t cMemRanges   = u32MemRanges < UINT16_MAX - 1 ? u32MemRanges : UINT16_MAX - 1;    /* One PT_NOTE Program header */
    uint16_t cProgHdrs    = cMemRanges + 1;

    DBGFCOREDESCRIPTOR CoreDescriptor;
    RT_ZERO(CoreDescriptor);
    CoreDescriptor.u32Magic     = DBGFCORE_MAGIC;
    CoreDescriptor.VBoxVersion  = VBOX_FULL_VERSION;
    CoreDescriptor.VBoxRevision = VBOX_SVN_REV;
    CoreDescriptor.cCpus        = pVM->cCpus;

    LogRel((DBGFLOG_NAME ":CoreDescriptor Version=%u Revision=%u\n", CoreDescriptor.VBoxVersion, CoreDescriptor.VBoxRevision));

    /*
     * Compute total size of the note section.
     */
    uint64_t cbNoteSection =   Elf64NoteSectionSize(s_pcszCoreVBoxCore, sizeof(CoreDescriptor))
                             + pVM->cCpus * Elf64NoteSectionSize(s_pcszCoreVBoxCpu, sizeof(CPUMCTX));
    uint64_t off = 0;

    /*
     * Create the core file.
     */
    RTFILE hFile = NIL_RTFILE;
    int rc = RTFileOpen(&hFile, pDbgfData->pszDumpPath, RTFILE_O_CREATE_REPLACE | RTFILE_O_READWRITE);
    if (RT_FAILURE(rc))
    {
        LogRel((DBGFLOG_NAME ":RTFileOpen failed for '%s' rc=%Rrc\n", pDbgfData->pszDumpPath, rc));
        return rc;
    }

    /*
     * Write ELF header.
     */
    uint64_t cbElfHdr = 0;
    uint64_t cbProgHdr = 0;
    uint64_t offMemRange = 0;
    rc = Elf64WriteElfHdr(hFile, cProgHdrs, 0 /* cSecHdrs */, &cbElfHdr);
    off += cbElfHdr;
    if (RT_FAILURE(rc))
    {
        LogRel((DBGFLOG_NAME ":Elf64WriteElfHdr failed. rc=%Rrc\n", rc));
        goto CoreWriteDone;
    }

    /*
     * Write PT_NOTE program header.
     */
    rc = Elf64WriteProgHdr(hFile, PT_NOTE, PF_R,
                           cbElfHdr + cProgHdrs * sizeof(Elf64_Phdr),   /* file offset to contents */
                           cbNoteSection,                               /* size in core file */
                           cbNoteSection,                               /* size in memory */
                           0,                                           /* physical address */
                           &cbProgHdr);
    Assert(cbProgHdr == sizeof(Elf64_Phdr));
    off += cbProgHdr;

    if (RT_FAILURE(rc))
    {
        LogRel((DBGFLOG_NAME ":Elf64WritreProgHdr failed for PT_NOTE. rc=%Rrc\n", rc));
        goto CoreWriteDone;
    }

    /*
     * Write PT_LOAD program header for each memory range.
     */
    offMemRange = off + cbNoteSection;
    for (uint16_t iRange = 0; iRange < cMemRanges; iRange++)
    {
        RTGCPHYS GCPhysStart;
        RTGCPHYS GCPhysEnd;

        bool fIsMmio;
        rc = PGMR3PhysGetRange(pVM, iRange, &GCPhysStart, &GCPhysEnd, NULL /* pszDesc */, &fIsMmio);
        if (RT_FAILURE(rc))
        {
            LogRel((DBGFLOG_NAME ": PGMR3PhysGetRange failed for iRange(%u) rc=%Rrc\n", iRange, rc));
            goto CoreWriteDone;
        }

        uint64_t cbMemRange  = GCPhysEnd - GCPhysStart + 1;
        uint64_t cbFileRange = fIsMmio ? 0 : cbMemRange;

        LogRel((DBGFLOG_NAME ": PGMR3PhysGetRange iRange=%u GCPhysStart=%#x GCPhysEnd=%#x cbMemRange=%u\n",
                iRange, GCPhysStart, GCPhysEnd, cbMemRange));

        rc = Elf64WriteProgHdr(hFile, PT_LOAD, PF_R,
                               offMemRange,                         /* file offset to contents */
                               cbFileRange,                         /* size in core file */
                               cbMemRange,                          /* size in memory */
                               GCPhysStart,                         /* physical address */
                               &cbProgHdr);
        Assert(cbProgHdr == sizeof(Elf64_Phdr));
        if (RT_FAILURE(rc))
        {
            LogRel((DBGFLOG_NAME ":Elf64WriteProgHdr failed for memory range(%u) cbFileRange=%u cbMemRange=%u rc=%Rrc\n", iRange,
                    cbFileRange, cbMemRange, rc));
            goto CoreWriteDone;
        }

        offMemRange += cbFileRange;
    }

    /*
     * Write the Core descriptor note header and data.
     */
    rc = Elf64WriteNoteHdr(hFile, NT_VBOXCORE, s_pcszCoreVBoxCore, &CoreDescriptor, sizeof(CoreDescriptor),
                           NULL /* pcbNoteHdr */);
    if (RT_FAILURE(rc))
    {
        LogRel((DBGFLOG_NAME ":Elf64WriteNoteHdr failed for Note '%s' rc=%Rrc\n", s_pcszCoreVBoxCore, rc));
        goto CoreWriteDone;
    }

    /*
     * Write the CPU context note headers and data.
     */
    for (uint32_t iCpu = 0; iCpu < pVM->cCpus; iCpu++)
    {
        PCPUMCTX pCpuCtx = &pVM->aCpus[iCpu].cpum.s.Guest;
        rc = Elf64WriteNoteHdr(hFile, NT_VBOXCPU, s_pcszCoreVBoxCpu, pCpuCtx, sizeof(CPUMCTX), NULL /* pcbNoteHdr */);
        if (RT_FAILURE(rc))
        {
            LogRel((DBGFLOG_NAME ":Elf64WriteNoteHdr failed for vCPU[%u] rc=%Rrc\n", iCpu, rc));
            goto CoreWriteDone;
        }
    }

    /*
     * Write memory ranges.
     */
    for (uint16_t iRange = 0; iRange < cMemRanges; iRange++)
    {
        RTGCPHYS GCPhysStart;
        RTGCPHYS GCPhysEnd;
        bool fIsMmio;
        rc = PGMR3PhysGetRange(pVM, iRange, &GCPhysStart, &GCPhysEnd, NULL /* pszDesc */, &fIsMmio);
        if (RT_FAILURE(rc))
        {
            LogRel((DBGFLOG_NAME ":PGMR3PhysGetRange(2) failed for iRange(%u) rc=%Rrc\n", iRange, rc));
            goto CoreWriteDone;
        }

        if (fIsMmio)
            continue;

        /*
         * Write page-by-page of this memory range.
         */
        uint64_t cbMemRange  = GCPhysEnd - GCPhysStart + 1;
        uint64_t cPages = cbMemRange >> PAGE_SHIFT;
        for (uint64_t iPage = 0; iPage < cPages; iPage++)
        {
            const int cbBuf = PAGE_SIZE;
            void *pvBuf     = MMR3HeapAlloc(pVM, MM_TAG_DBGF_CORE_WRITE, cbBuf);
            if (RT_UNLIKELY(!pvBuf))
            {
                LogRel((DBGFLOG_NAME ":MMR3HeapAlloc failed. iRange=%u iPage=%u\n", iRange, iPage));
                goto CoreWriteDone;
            }

            rc = PGMPhysRead(pVM, GCPhysStart, pvBuf, cbBuf);
            if (RT_FAILURE(rc))
            {
                /*
                 * For some reason this failed, write out a zero page instead.
                 */
                LogRel((DBGFLOG_NAME ":PGMPhysRead failed for iRange=%u iPage=%u. rc=%Rrc. Ignoring...\n", iRange,
                        iPage, rc));
                memset(pvBuf, 0, cbBuf);
            }

            rc = RTFileWrite(hFile, pvBuf, cbBuf, NULL /* all */);
            if (RT_FAILURE(rc))
            {
                LogRel((DBGFLOG_NAME ":RTFileWrite failed. iRange=%u iPage=%u rc=%Rrc\n", iRange, iPage, rc));
                MMR3HeapFree(pvBuf);
                goto CoreWriteDone;
            }

            MMR3HeapFree(pvBuf);
        }
    }

CoreWriteDone:
    RTFileClose(hFile);

    return rc;
}


/**
 * Write core dump of the guest.
 *
 * @return VBox status code.
 * @param   pVM                 The VM handle.
 * @param   pszDumpPath         The path of the file to dump into, cannot be
 *                              NULL.
 *
 * @remarks The VM must be suspended before calling this function.
 */
VMMR3DECL(int) DBGFR3CoreWrite(PVM pVM, const char *pszDumpPath)
{
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);
    AssertReturn(pszDumpPath, VERR_INVALID_HANDLE);

    /*
     * Pass the core write request down to EMT rendezvous which makes sure
     * other EMTs, if any, are not running.
     */
    DBGFCOREDATA CoreData;
    RT_ZERO(CoreData);
    CoreData.pszDumpPath = pszDumpPath;

    return VMMR3EmtRendezvous(pVM, VMMEMTRENDEZVOUS_FLAGS_TYPE_ONCE, dbgfR3CoreWrite, &CoreData);
}

