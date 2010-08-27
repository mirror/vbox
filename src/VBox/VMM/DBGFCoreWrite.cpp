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
#include <VBox/err.h>
#include <VBox/log.h>

#include "../Runtime/include/internal/ldrELF64.h"

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
static int Elf64WriteElfHdr(RTFILE hFile, uint16_t cProgHdrs, uint16_t cSecHdrs, size_t *pcbElfHdr)
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
 * @param fFlags            Flags (access permissions).
 * @param offFileData       File offset of contents.
 * @param cbFileData        Size of contents in the file.
 * @param cbMemData         Size of contents in memory.
 * @param pcbProgHdr        Where to store the size of written header to file,
 *                          can be NULL.
 *
 * @return IPRT status code.
 */
static int Elf64WriteProgHdr(RTFILE hFile, uint32_t Type, uint32_t fFlags, RTFOFF offFileData, size_t cbFileData, size_t cbMemData,
                                size_t *pcbProgHdr)
{
    Elf64_Phdr ProgHdr;
    RT_ZERO(ProgHdr);
    ProgHdr.p_type          = Type;
    ProgHdr.p_flags         = fFlags;
    ProgHdr.p_offset        = offFileData;
    ProgHdr.p_filesz        = cbFileData;
    ProgHdr.p_memsz         = cbMemData;

    int rc = RTFileWrite(hFile, &ProgHdr, sizeof(ProgHdr), NULL /* full write */);
    if (RT_SUCCESS(rc) && pcbProgHdr)
        *pcbProgHdr = sizeof(ProgHdr);
    return rc;
}


/**
 * Count the number of memory blobs that go into the core file.
 *
 * We cannot do a page-by-page dump of the entire guest memory as there will be
 * way too many entries. Also we don't want to dump MMIO regions which means we
 * cannot have a 1:1 mapping between core file offset and memory offset. Instead
 * we dump the memory in blobs. A memory blob is a contiguous memory area
 * suitable for dumping to a core file.
 *
 * @param pVM               The VM handle.
 * @oaram idCpu             The target CPU ID.
 *
 * @return Number of memory blobs.
 */
static int dbgfR3CountMemoryBlobs(PVM pVM, VMCPUID idCpu)
{
    /* @todo */
    return 0;
}


/**
 * EMT worker function for DBGFR3CoreWrite.
 *
 * @param   pVM              The VM handle.
 * @param   idCpu            The target CPU ID.
 * @param   pszDumpPath      The full path of the file to dump into.
 *
 * @return VBox status code.
 */
static DECLCALLBACK(int) dbgfR3CoreWrite(PVM pVM, VMCPUID idCpu, const char *pszDumpPath)
{
    /*
     * Validate input.
     */
    Assert(idCpu == VMMGetCpuId(pVM));
    AssertReturn(pszDumpPath, VERR_INVALID_POINTER);

    /*
     * Halt the VM if it is not already halted.
     */
    int rc = VINF_SUCCESS;
    if (!DBGFR3IsHalted(pVM))
    {
        rc = DBGFR3Halt(pVM);
        if (RT_FAILURE(rc))
            return rc;
    }

    /*
     * Collect core information.
     */
    uint16_t cMemBlobs = dbgfR3CountMemoryBlobs(pVM, idCpu);
    uint16_t cProgHdrs = cMemBlobs + 1;   /* One PT_NOTE Program header */

    /*
     * Write the core file.
     */
    RTFILE hFile = NIL_RTFILE;
    rc = RTFileOpen(&hFile, pszDumpPath, RTFILE_O_CREATE | RTFILE_O_READWRITE);
    if (RT_SUCCESS(rc))
    {
        size_t cbElfHdr = 0;
        rc = Elf64WriteElfHdr(hFile, 0, 0 /* cSecHdrs */, &cbElfHdr);
        if (RT_SUCCESS(rc))
        {

        }

        RTFileClose(hFile);
    }

    /*
     * Resume the VM.
     */
    DBGFR3Resume(pVM);

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
 */
VMMR3DECL(int) DBGFR3CoreWrite(PVM pVM, VMCPUID idCpu, const char *pszDumpPath)
{
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);
    AssertReturn(idCpu < pVM->cCpus, VERR_INVALID_CPU_ID);
    AssertReturn(pszDumpPath, VERR_INVALID_HANDLE);

    /*
     * Pass the core write request down to EMT.
     */
    return VMR3ReqCallWaitU(pVM->pUVM, idCpu, (PFNRT)dbgfR3CoreWrite, 3, pVM, idCpu, pszDumpPath);
}

