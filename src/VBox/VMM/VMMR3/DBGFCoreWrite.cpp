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

/** @page pg_dbgf_vmcore    VMCore Format
 *
 * The VirtualBox VMCore Format:
 * [ ELF 64 Header]  -- Only 1
 *
 * [ PT_NOTE ]       -- Only 1
 *    - Offset into CoreDescriptor followed by list of Notes (Note Hdr + data) of VBox CPUs.
 *    - (Any Additional custom Note sections).
 *
 * [ PT_LOAD ]       -- One for each contiguous memory chunk
 *    - Memory offset (physical).
 *    - File offset.
 *
 * CoreDescriptor
 *    - Magic, VBox version.
 *    - Number of CPus.
 *
 * Per-CPU register dump
 *    - CPU 1 Note Hdr + Data.
 *    - CPU 2 Note Hdr + Data.
 *    ...
 * (Additional custom notes Hdr+data)
 *    - VBox 1 Note Hdr + Data.
 *    - VBox 2 Note Hdr + Data.
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

#include <VBox/vmm/cpum.h>
#include "CPUMInternal.h"
#include <VBox/vmm/dbgf.h>
#include <VBox/vmm/dbgfcorefmt.h>
#include <VBox/vmm/vm.h>
#include <VBox/vmm/pgm.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <VBox/vmm/mm.h>
#include <VBox/version.h>

#include "../../Runtime/include/internal/ldrELF64.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#define DBGFLOG_NAME           "DBGFCoreWrite"


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static const int s_NoteAlign  = 8;
static const int s_cbNoteName = 16;

/* These strings *HAVE* to be 8-byte aligned */
static const char *s_pcszCoreVBoxCore = "VBCORE";
static const char *s_pcszCoreVBoxCpu  = "VBCPU";


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Guest core writer data.
 *
 * Used to pass parameters from DBGFR3CoreWrite to dbgfR3CoreWriteRendezvous.
 */
typedef struct DBGFCOREDATA
{
    /** The name of the file to write the file to. */
    const char *pszFilename;
    /** Whether to replace (/overwrite) any existing file. */
    bool        fReplaceFile;
} DBGFCOREDATA;
/** Pointer to the guest core writer data.  */
typedef DBGFCOREDATA *PDBGFCOREDATA;



/**
 * ELF function to write 64-bit ELF header.
 *
 * @param hFile             The file to write to.
 * @param cProgHdrs         Number of program headers.
 * @param cSecHdrs          Number of section headers.
 *
 * @return IPRT status code.
 */
static int Elf64WriteElfHdr(RTFILE hFile, uint16_t cProgHdrs, uint16_t cSecHdrs)
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
    /* 32-bit builds will produce cores with e_machine EM_386. */
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

    return RTFileWrite(hFile, &ElfHdr, sizeof(ElfHdr), NULL /* all */);
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
 *
 * @return IPRT status code.
 */
static int Elf64WriteProgHdr(RTFILE hFile, uint32_t Type, uint32_t fFlags, uint64_t offFileData, uint64_t cbFileData,
                             uint64_t cbMemData, RTGCPHYS Phys)
{
    Elf64_Phdr ProgHdr;
    RT_ZERO(ProgHdr);
    ProgHdr.p_type          = Type;
    ProgHdr.p_flags         = fFlags;
    ProgHdr.p_offset        = offFileData;
    ProgHdr.p_filesz        = cbFileData;
    ProgHdr.p_memsz         = cbMemData;
    ProgHdr.p_paddr         = Phys;

    return RTFileWrite(hFile, &ProgHdr, sizeof(ProgHdr), NULL /* all */);
}


/**
 * Returns the size of the NOTE section given the name and size of the data.
 *
 * @param pszName           Name of the note section.
 * @param cb                Size of the data portion of the note section.
 *
 * @return The size of the NOTE section as rounded to the file alignment.
 */
static uint64_t Elf64NoteSectionSize(const char *pszName, uint64_t cbData)
{
    uint64_t cbNote = sizeof(Elf64_Nhdr);

    size_t cchName      = strlen(pszName) + 1;
    size_t cchNameAlign = RT_ALIGN_Z(cchName, s_NoteAlign);

    cbNote += cchNameAlign;
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
 *
 * @return IPRT status code.
 */
static int Elf64WriteNoteHdr(RTFILE hFile, uint16_t Type, const char *pszName, const void *pcvData, uint64_t cbData)
{
    AssertReturn(pcvData, VERR_INVALID_POINTER);
    AssertReturn(cbData > 0, VERR_NO_DATA);

    char szNoteName[s_cbNoteName];
    RT_ZERO(szNoteName);
    RTStrCopy(szNoteName, sizeof(szNoteName), pszName);

    size_t cchName       = strlen(szNoteName) + 1;
    size_t cchNameAlign  = RT_ALIGN_Z(cchName, s_NoteAlign);
    uint64_t cbDataAlign = RT_ALIGN_64(cbData, s_NoteAlign);

    /*
     * Yell loudly and bail if we are going to be writing a core file that is not compatible with
     * both Solaris and the 64-bit ELF spec. which dictates 8-byte alignment. See @bugref{5211} comment 3.
     */
    if (cchNameAlign - cchName > 3)
    {
        LogRel((DBGFLOG_NAME ": Elf64WriteNoteHdr pszName=%s cchName=%u cchNameAlign=%u, cchName aligns to 4 not 8-bytes!\n", pszName, cchName,
                cchNameAlign));
        return VERR_INVALID_PARAMETER;
    }

    if (cbDataAlign - cbData > 3)
    {
        LogRel((DBGFLOG_NAME ": Elf64WriteNoteHdr pszName=%s cbData=%u cbDataAlign=%u, cbData aligns to 4 not 8-bytes!\n", pszName, cbData,
                cbDataAlign));
        return VERR_INVALID_PARAMETER;
    }

    static const char s_achPad[7] = { 0, 0, 0, 0, 0, 0, 0 };
    AssertCompile(sizeof(s_achPad) >= s_NoteAlign - 1);

    Elf64_Nhdr ElfNoteHdr;
    RT_ZERO(ElfNoteHdr);
    ElfNoteHdr.n_namesz = (Elf64_Word)cchName - 1; /* Again a discrepancy between ELF-64 and Solaris (@bugref{5211} comment 3), we will follow ELF-64 */
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
        rc = RTFileWrite(hFile, szNoteName, cchName, NULL /* all */);
        if (RT_SUCCESS(rc))
        {
            /*
             * Write note name padding if required.
             */
            if (cchNameAlign > cchName)
                rc = RTFileWrite(hFile, s_achPad, cchNameAlign - cchName, NULL);

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
        LogRel((DBGFLOG_NAME ": RTFileWrite failed. rc=%Rrc pszName=%s cchName=%u cchNameAlign=%u cbData=%u cbDataAlign=%u\n",
                rc, pszName, cchName, cchNameAlign, cbData, cbDataAlign));

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
 * Worker function for dbgfR3CoreWrite which does the writing.
 *
 * @returns VBox status code
 * @param   pVM                 The VM handle.
 * @param   hFile               The file to write to.  Caller closes this.
 */
static int dbgfR3CoreWriteWorker(PVM pVM, RTFILE hFile)
{
    /*
     * Collect core information.
     */
    uint32_t const cu32MemRanges = dbgfR3GetRamRangeCount(pVM);
    uint16_t const cMemRanges    = cu32MemRanges < UINT16_MAX - 1 ? cu32MemRanges : UINT16_MAX - 1; /* One PT_NOTE Program header */
    uint16_t const cProgHdrs     = cMemRanges + 1;

    DBGFCOREDESCRIPTOR CoreDescriptor;
    RT_ZERO(CoreDescriptor);
    CoreDescriptor.u32Magic         = DBGFCORE_MAGIC;
    CoreDescriptor.u32FmtVersion    = DBGFCORE_FMT_VERSION;
    CoreDescriptor.cbSelf           = sizeof(CoreDescriptor);
    CoreDescriptor.u32VBoxVersion   = VBOX_FULL_VERSION;
    CoreDescriptor.u32VBoxRevision  = VMMGetSvnRev();
    CoreDescriptor.cCpus            = pVM->cCpus;

    Log((DBGFLOG_NAME ": CoreDescriptor Version=%u Revision=%u\n", CoreDescriptor.u32VBoxVersion, CoreDescriptor.u32VBoxRevision));

    /*
     * Compute the file layout (see pg_dbgf_vmcore).
     */
    uint64_t const offElfHdr        = RTFileTell(hFile);
    uint64_t const offNoteSection   = offElfHdr         + sizeof(Elf64_Ehdr);
    uint64_t const offLoadSections  = offNoteSection    + sizeof(Elf64_Phdr);
    uint64_t const cbLoadSections   = cMemRanges * sizeof(Elf64_Phdr);
    uint64_t const offCoreDescriptor= offLoadSections   + cbLoadSections;
    uint64_t const cbCoreDescriptor = Elf64NoteSectionSize(s_pcszCoreVBoxCore, sizeof(CoreDescriptor));
    uint64_t const offCpuDumps      = offCoreDescriptor + cbCoreDescriptor;
    uint64_t const cbCpuDumps       = pVM->cCpus * Elf64NoteSectionSize(s_pcszCoreVBoxCpu, sizeof(CPUMCTX));
    uint64_t const offMemory        = offCpuDumps       + cbCpuDumps;

    uint64_t const offNoteSectionData = offCoreDescriptor;
    uint64_t const cbNoteSectionData  = cbCoreDescriptor + cbCpuDumps;

    /*
     * Write ELF header.
     */
    int rc = Elf64WriteElfHdr(hFile, cProgHdrs, 0 /* cSecHdrs */);
    if (RT_FAILURE(rc))
    {
        LogRel((DBGFLOG_NAME ": Elf64WriteElfHdr failed. rc=%Rrc\n", rc));
        return rc;
    }

    /*
     * Write PT_NOTE program header.
     */
    Assert(RTFileTell(hFile) == offNoteSection);
    rc = Elf64WriteProgHdr(hFile, PT_NOTE, PF_R,
                           offNoteSectionData,  /* file offset to contents */
                           cbNoteSectionData,   /* size in core file */
                           cbNoteSectionData,   /* size in memory */
                           0);                  /* physical address */
    if (RT_FAILURE(rc))
    {
        LogRel((DBGFLOG_NAME ": Elf64WritreProgHdr failed for PT_NOTE. rc=%Rrc\n", rc));
        return rc;
    }

    /*
     * Write PT_LOAD program header for each memory range.
     */
    Assert(RTFileTell(hFile) == offLoadSections);
    uint64_t offMemRange = offMemory;
    for (uint16_t iRange = 0; iRange < cMemRanges; iRange++)
    {
        RTGCPHYS    GCPhysStart;
        RTGCPHYS    GCPhysEnd;
        bool        fIsMmio;
        rc = PGMR3PhysGetRange(pVM, iRange, &GCPhysStart, &GCPhysEnd, NULL /* pszDesc */, &fIsMmio);
        if (RT_FAILURE(rc))
        {
            LogRel((DBGFLOG_NAME ": PGMR3PhysGetRange failed for iRange(%u) rc=%Rrc\n", iRange, rc));
            return rc;
        }

        uint64_t cbMemRange  = GCPhysEnd - GCPhysStart + 1;
        uint64_t cbFileRange = fIsMmio ? 0 : cbMemRange;

        Log((DBGFLOG_NAME ": PGMR3PhysGetRange iRange=%u GCPhysStart=%#x GCPhysEnd=%#x cbMemRange=%u\n",
             iRange, GCPhysStart, GCPhysEnd, cbMemRange));

        rc = Elf64WriteProgHdr(hFile, PT_LOAD, PF_R,
                               offMemRange,                         /* file offset to contents */
                               cbFileRange,                         /* size in core file */
                               cbMemRange,                          /* size in memory */
                               GCPhysStart);                        /* physical address */
        if (RT_FAILURE(rc))
        {
            LogRel((DBGFLOG_NAME ": Elf64WriteProgHdr failed for memory range(%u) cbFileRange=%u cbMemRange=%u rc=%Rrc\n",
                    iRange, cbFileRange, cbMemRange, rc));
            return rc;
        }

        offMemRange += cbFileRange;
    }

    /*
     * Write the Core descriptor note header and data.
     */
    Assert(RTFileTell(hFile) == offCoreDescriptor);
    rc = Elf64WriteNoteHdr(hFile, NT_VBOXCORE, s_pcszCoreVBoxCore, &CoreDescriptor, sizeof(CoreDescriptor));
    if (RT_FAILURE(rc))
    {
        LogRel((DBGFLOG_NAME ": Elf64WriteNoteHdr failed for Note '%s' rc=%Rrc\n", s_pcszCoreVBoxCore, rc));
        return rc;
    }

    /*
     * Write the CPU context note headers and data.
     */
    Assert(RTFileTell(hFile) == offCpuDumps);
    for (uint32_t iCpu = 0; iCpu < pVM->cCpus; iCpu++)
    {
        PCPUMCTX pCpuCtx = &pVM->aCpus[iCpu].cpum.s.Guest;
        rc = Elf64WriteNoteHdr(hFile, NT_VBOXCPU, s_pcszCoreVBoxCpu, pCpuCtx, sizeof(CPUMCTX));
        if (RT_FAILURE(rc))
        {
            LogRel((DBGFLOG_NAME ": Elf64WriteNoteHdr failed for vCPU[%u] rc=%Rrc\n", iCpu, rc));
            return rc;
        }
    }

    /*
     * Write memory ranges.
     */
    Assert(RTFileTell(hFile) == offMemory);
    for (uint16_t iRange = 0; iRange < cMemRanges; iRange++)
    {
        RTGCPHYS GCPhysStart;
        RTGCPHYS GCPhysEnd;
        bool     fIsMmio;
        rc = PGMR3PhysGetRange(pVM, iRange, &GCPhysStart, &GCPhysEnd, NULL /* pszDesc */, &fIsMmio);
        if (RT_FAILURE(rc))
        {
            LogRel((DBGFLOG_NAME ": PGMR3PhysGetRange(2) failed for iRange(%u) rc=%Rrc\n", iRange, rc));
            return rc;
        }

        if (fIsMmio)
            continue;

        /*
         * Write page-by-page of this memory range.
         *
         * The read function may fail on MMIO ranges, we write these as zero
         * pages for now (would be nice to have the VGA bits there though).
         */
        uint64_t cbMemRange  = GCPhysEnd - GCPhysStart + 1;
        uint64_t cPages      = cbMemRange >> PAGE_SHIFT;
        for (uint64_t iPage = 0; iPage < cPages; iPage++)
        {
            uint8_t abPage[PAGE_SIZE];
            rc = PGMPhysSimpleReadGCPhys(pVM, abPage, GCPhysStart + (iPage << PAGE_SHIFT),  sizeof(abPage));
            if (RT_FAILURE(rc))
            {
                if (rc != VERR_PGM_PHYS_PAGE_RESERVED)
                    LogRel((DBGFLOG_NAME ": PGMPhysRead failed for iRange=%u iPage=%u. rc=%Rrc. Ignoring...\n", iRange, iPage, rc));
                RT_ZERO(abPage);
            }

            rc = RTFileWrite(hFile, abPage, sizeof(abPage), NULL /* all */);
            if (RT_FAILURE(rc))
            {
                LogRel((DBGFLOG_NAME ": RTFileWrite failed. iRange=%u iPage=%u rc=%Rrc\n", iRange, iPage, rc));
                return rc;
            }
        }
    }

    return rc;
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
static DECLCALLBACK(VBOXSTRICTRC) dbgfR3CoreWriteRendezvous(PVM pVM, PVMCPU pVCpu, void *pvData)
{
    /*
     * Validate input.
     */
    AssertReturn(pVM, VERR_INVALID_VM_HANDLE);
    AssertReturn(pVCpu, VERR_INVALID_VMCPU_HANDLE);
    AssertReturn(pvData, VERR_INVALID_POINTER);

    PDBGFCOREDATA pDbgfData = (PDBGFCOREDATA)pvData;

    /*
     * Create the core file.
     */
    uint32_t fFlags = (pDbgfData->fReplaceFile ? RTFILE_O_CREATE_REPLACE : RTFILE_O_CREATE)
                    | RTFILE_O_WRITE
                    | RTFILE_O_DENY_ALL
                    | (0600 << RTFILE_O_CREATE_MODE_SHIFT);
    RTFILE   hFile;
    int rc = RTFileOpen(&hFile, pDbgfData->pszFilename, fFlags);
    if (RT_SUCCESS(rc))
    {
        rc = dbgfR3CoreWriteWorker(pVM, hFile);
        RTFileClose(hFile);
    }
    else
        LogRel((DBGFLOG_NAME ": RTFileOpen failed for '%s' rc=%Rrc\n", pDbgfData->pszFilename, rc));
    return rc;
}


/**
 * Write core dump of the guest.
 *
 * @returns VBox status code.
 * @param   pVM                 The VM handle.
 * @param   pszFilename         The name of the file to which the guest core
 *                              dump should be written.
 * @param   fReplaceFile        Whether to replace the file or not.
 *
 * @remarks The VM should be suspended before calling this function or DMA may
 *          interfer with the state.
 */
VMMR3DECL(int) DBGFR3CoreWrite(PVM pVM, const char *pszFilename, bool fReplaceFile)
{
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);
    AssertReturn(pszFilename, VERR_INVALID_HANDLE);

    /*
     * Pass the core write request down to EMT rendezvous which makes sure
     * other EMTs, if any, are not running. IO threads could still be running
     * but we don't care about them.
     */
    DBGFCOREDATA CoreData;
    RT_ZERO(CoreData);
    CoreData.pszFilename  = pszFilename;
    CoreData.fReplaceFile = fReplaceFile;

    int rc = VMMR3EmtRendezvous(pVM, VMMEMTRENDEZVOUS_FLAGS_TYPE_ONCE, dbgfR3CoreWriteRendezvous, &CoreData);
    if (RT_SUCCESS(rc))
        LogRel((DBGFLOG_NAME ": Successfully wrote guest core dump '%s'\n", pszFilename));
    else
        LogRel((DBGFLOG_NAME ": Failed to write guest core dump '%s'. rc=%Rrc\n", pszFilename, rc));
    return rc;
}

