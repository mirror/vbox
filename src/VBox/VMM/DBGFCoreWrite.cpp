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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DBGF
#include <iprt/param.h>
#include <iprt/file.h>
#include <VBox/dbgf.h>
#include "DBGFInternal.h"
#include <VBox/vm.h>
#include <VBox/pgm.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <VBox/mm.h>

#include "../Runtime/include/internal/ldrELF64.h"

/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#ifdef DEBUG_ramshankar
# undef Log
# define Log LogRel
#endif
#define DBGFLOG_NAME           "DGBFCoreWrite"

/**
 * DBGFCOREDATA: Core data.
 */
typedef struct
{
    const char *pszDumpPath;    /* File path to dump the core into. */
} DBGFCOREDATA, *PDBGFCOREDATA;


/*
 * VBox VMCore Format:
 * [ ELF 64 Header]  -- Only 1
 *
 * [ PT_NOTE ]       -- Only 1
 *    - Offset into list of Notes (Note Hdr + data) of VBox CPUs.
 *    - (Any Additional custom Note sections)
 *
 * [ PT_LOAD ]       -- One for each contiguous memory chunk
 *    - Memory offset
 *    - File offset
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
AssertCompile(sizeof(uint32_t) == 4);

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

    int rc = RTFileWrite(hFile, &ElfHdr, sizeof(ElfHdr), NULL /* full write */);
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

    int rc = RTFileWrite(hFile, &ProgHdr, sizeof(ProgHdr), NULL /* full write */);
    if (RT_SUCCESS(rc) && pcbProgHdr)
        *pcbProgHdr = sizeof(ProgHdr);
    return rc;
}


/**
 * Elf function to write 64-bit note header.
 *
 * @param hFile             The file to write to.
 * @param Type              Type of this section.
 * @param pszName           Name of this section, will be limited to 8 bytes.
 * @param pcv               Opaque pointer to the data, if NULL only computes size.
 * @param cb                Size of the data.
 * @param pcbNoteHdr        Where to store the size of written header to file,
 *                          can be NULL.
 *
 * @return IPRT status code.
 */
static int Elf64WriteNoteHeader(RTFILE hFile, uint16_t Type, const char *pszName, const void *pcv, uint64_t cb, uint64_t *pcbNoteHdr)
{
    AssertReturn(pcv, VERR_INVALID_POINTER);
    AssertReturn(cb > 0, VERR_NO_DATA);

    typedef struct
    {
        Elf64_Nhdr  Hdr;            /* 64-bit NOTE Header */
        char        achName[8];     /* Name of NOTE section */
    } ELFNOTEHDR;

    ELFNOTEHDR ElfNoteHdr;
    RT_ZERO(ElfNoteHdr);
    RTStrCopy(ElfNoteHdr.achName, sizeof(ElfNoteHdr.achName) - 1, pszName);
    ElfNoteHdr.Hdr.n_namesz = strlen(ElfNoteHdr.achName) + 1;
    ElfNoteHdr.Hdr.n_type   = Type;

    static const char s_achPad[3] = { 0, 0, 0 };
    uint64_t cbAlign = RT_ALIGN_64(cb, 4);
    ElfNoteHdr.Hdr.n_descsz = cbAlign;

    /*
     * Write note header and description.
     */
    int rc = RTFileWrite(hFile, &ElfNoteHdr, sizeof(ElfNoteHdr), NULL /* full write */);
    if (RT_SUCCESS(rc))
    {
        rc = RTFileWrite(hFile, pcv, cb, NULL /* full write */);
        if (RT_SUCCESS(rc))
        {
            if (cbAlign > cb)
                rc = RTFileWrite(hFile, s_achPad, cbAlign - cb, NULL /* full write*/);
        }
    }

    if (RT_FAILURE(rc))
        LogRel((DBGFLOG_NAME ":RTFileWrite failed. rc=%Rrc pszName=%s cb=%u cbAlign=%u\n", rc, pszName, cb, cbAlign));

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
 * @remarks The VM must be suspended before calling this function.
 */
static DECLCALLBACK(int) dbgfR3CoreWrite(PVM pVM, PVMCPU pVCpu, void *pvData)
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

    /*
     * Compute size of the note section.
     */
    uint64_t cbNoteSection = pVM->cCpus * sizeof(CPUMCTX);
    uint64_t off = 0;

    /*
     * Create the core file.
     */
    RTFILE hFile = NIL_RTFILE;
    int rc = RTFileOpen(&hFile, pDbgfData->pszDumpPath, RTFILE_O_CREATE_REPLACE | RTFILE_O_READWRITE);
    if (RT_SUCCESS(rc))
    {
        /*
         * Write ELF header.
         */
        uint64_t cbElfHdr = 0;
        rc = Elf64WriteElfHdr(hFile, cProgHdrs, 0 /* cSecHdrs */, &cbElfHdr);
        off += cbElfHdr;
        if (RT_SUCCESS(rc))
        {
            /*
             * Write PT_NOTE program header.
             */
            uint64_t cbProgHdr = 0;
            rc = Elf64WriteProgHdr(hFile, PT_NOTE, PF_R,
                                   cbElfHdr + cProgHdrs * sizeof(Elf64_Phdr),   /* file offset to contents */
                                   cbNoteSection,                               /* size in core file */
                                   cbNoteSection,                               /* size in memory */
                                   0,                                           /* physical address */
                                   &cbProgHdr);
            Assert(cbProgHdr == sizeof(Elf64_Phdr));
            off += cbProgHdr;
            if (RT_SUCCESS(rc))
            {
                /*
                 * Write PT_LOAD program header for each memory range.
                 */
                uint64_t offMemRange = off + cbNoteSection;
                for (uint16_t iRange = 0; iRange < cMemRanges; iRange++)
                {
                    RTGCPHYS GCPhysStart;
                    RTGCPHYS GCPhysEnd;

                    bool fIsMmio;
                    rc = PGMR3PhysGetRange(pVM, iRange, &GCPhysStart, &GCPhysEnd, NULL /* pszDesc */, &fIsMmio);
                    if (RT_FAILURE(rc))
                    {
                        LogRel((DBGFLOG_NAME ": PGMR3PhysGetRange failed for iRange(%u) rc=%Rrc\n", iRange, rc));
                        break;
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
                        break;
                    }

                    offMemRange += cbFileRange;
                }

                /*
                 * Write the CPU context note headers and data.
                 */
                if (RT_SUCCESS(rc))
                {
                    for (uint32_t iCpu = 0; iCpu < pVM->cCpus; iCpu++)
                    {
                        /** @todo -XXX- cpus */
                    }
                }
            }

        }

        RTFileClose(hFile);
    }

    return rc;
}


/**
 * Write core dump of the guest.
 *
 * @return VBox status code.
 * @param   pVM                 The VM handle.
 * @param   idCpu               The target CPU ID.
 * @param   pszDumpPath         The path of the file to dump into, cannot be
 *                              NULL.
 *
 * @remarks The VM must be suspended before calling this function.
 */
VMMR3DECL(int) DBGFR3CoreWrite(PVM pVM, VMCPUID idCpu, const char *pszDumpPath)
{
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);
    AssertReturn(idCpu < pVM->cCpus, VERR_INVALID_CPU_ID);
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

