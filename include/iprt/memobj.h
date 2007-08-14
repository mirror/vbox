/** @file
 * innotek Portable Runtime - Memory Objects (Ring-0).
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___iprt_memobj_h
#define ___iprt_memobj_h

#include <iprt/cdefs.h>
#include <iprt/types.h>

__BEGIN_DECLS

/** @defgroup grp_rt_memobj     RTMemObj - Memory Object Manipulation (Ring-0)
 * @ingroup grp_rt
 * @{
 */

#ifdef IN_RING0

/**
 * Checks if this is mapping or not.
 *
 * @returns true if it's a mapping, otherwise false.
 * @param   MemObj  The ring-0 memory object handle.
 */
RTR0DECL(bool) RTR0MemObjIsMapping(RTR0MEMOBJ MemObj);

/**
 * Gets the address of a ring-0 memory object.
 *
 * @returns The address of the memory object.
 * @returns NULL if the handle is invalid (asserts in strict builds) or if there isn't any mapping.
 * @param   MemObj  The ring-0 memory object handle.
 */
RTR0DECL(void *) RTR0MemObjAddress(RTR0MEMOBJ MemObj);

/**
 * Gets the size of a ring-0 memory object.
 *
 * @returns The address of the memory object.
 * @returns NULL if the handle is invalid (asserts in strict builds) or if there isn't any mapping.
 * @param   MemObj  The ring-0 memory object handle.
 */
RTR0DECL(size_t) RTR0MemObjSize(RTR0MEMOBJ MemObj);

/**
 * Get the physical address of an page in the memory object.
 *
 * @returns The physical address.
 * @returns NIL_RTHCPHYS if the object doesn't contain fixed physical pages.
 * @returns NIL_RTHCPHYS if the iPage is out of range.
 * @returns NIL_RTHCPHYS if the object handle isn't valid.
 * @param   MemObj  The ring-0 memory object handle.
 * @param   iPage   The page number within the object.
 */
RTR0DECL(RTHCPHYS) RTR0MemObjGetPagePhysAddr(RTR0MEMOBJ MemObj, size_t iPage);

/**
 * Frees a ring-0 memory object.
 *
 * @returns IPRT status code.
 * @retval  VERR_INVALID_HANDLE if
 * @param   MemObj          The ring-0 memory object to be freed. NULL is accepted.
 * @param   fFreeMappings   Whether or not to free mappings of the object.
 */
RTR0DECL(int) RTR0MemObjFree(RTR0MEMOBJ MemObj, bool fFreeMappings);

/**
 * Allocates page aligned virtual kernel memory.
 *
 * The memory is taken from a non paged (= fixed physical memory backing) pool.
 *
 * @returns IPRT status code.
 * @param   pMemObj         Where to store the ring-0 memory object handle.
 * @param   cb              Number of bytes to allocate. This is rounded up to nearest page.
 * @param   fExecutable     Flag indicating whether it should be permitted to executed code in the memory object.
 */
RTR0DECL(int) RTR0MemObjAllocPage(PRTR0MEMOBJ pMemObj, size_t cb, bool fExecutable);

/**
 * Allocates page aligned virtual kernel memory with physical backing below 4GB.
 *
 * The physical memory backing the allocation is fixed.
 *
 * @returns IPRT status code.
 * @param   pMemObj         Where to store the ring-0 memory object handle.
 * @param   cb              Number of bytes to allocate. This is rounded up to nearest page.
 * @param   fExecutable     Flag indicating whether it should be permitted to executed code in the memory object.
 */
RTR0DECL(int) RTR0MemObjAllocLow(PRTR0MEMOBJ pMemObj, size_t cb, bool fExecutable);

/**
 * Allocates page aligned virtual kernel memory with contiguous physical backing below 4GB.
 *
 * The physical memory backing the allocation is fixed.
 *
 * @returns IPRT status code.
 * @param   pMemObj         Where to store the ring-0 memory object handle.
 * @param   cb              Number of bytes to allocate. This is rounded up to nearest page.
 * @param   fExecutable     Flag indicating whether it should be permitted to executed code in the memory object.
 */
RTR0DECL(int) RTR0MemObjAllocCont(PRTR0MEMOBJ pMemObj, size_t cb, bool fExecutable);

/**
 * Locks a range of user virtual memory.
 *
 * @returns IPRT status code.
 * @param   pMemObj         Where to store the ring-0 memory object handle.
 * @param   pv              User virtual address. This is rounded down to a page boundrary.
 * @param   cb              Number of bytes to lock. This is rounded up to nearest page boundrary.
 * @param   R0Process       The process to lock pages in. NIL_R0PROCESS is an alias for the current one.
 *
 * @remark  RTR0MemGetAddressR3() and RTR0MemGetAddress() will return the rounded down address.
 */
RTR0DECL(int) RTR0MemObjLockUser(PRTR0MEMOBJ pMemObj, void *pv, size_t cb, RTR0PROCESS R0Process);

/**
 * Locks a range of kernel virtual memory.
 *
 * @returns IPRT status code.
 * @param   pMemObj         Where to store the ring-0 memory object handle.
 * @param   pv              Kernel virtual address. This is rounded down to a page boundrary.
 * @param   cb              Number of bytes to lock. This is rounded up to nearest page boundrary.
 *
 * @remark  RTR0MemGetAddress() will return the rounded down address.
 */
RTR0DECL(int) RTR0MemObjLockKernel(PRTR0MEMOBJ pMemObj, void *pv, size_t cb);

/**
 * Allocates contiguous page aligned physical memory without (necessarily) any kernel mapping.
 *
 * @returns IPRT status code.
 * @param   pMemObj         Where to store the ring-0 memory object handle.
 * @param   cb              Number of bytes to allocate. This is rounded up to nearest page.
 * @param   PhysHighest     The highest permittable address (inclusive).
 *                          Pass NIL_RTHCPHYS if any address is acceptable.
 */
RTR0DECL(int) RTR0MemObjAllocPhys(PRTR0MEMOBJ pMemObj, size_t cb, RTHCPHYS PhysHighest);

/**
 * Allocates non-contiguous page aligned physical memory without (necessarily) any kernel mapping.
 *
 * @returns IPRT status code.
 * @param   pMemObj         Where to store the ring-0 memory object handle.
 * @param   cb              Number of bytes to allocate. This is rounded up to nearest page.
 * @param   PhysHighest     The highest permittable address (inclusive).
 *                          Pass NIL_RTHCPHYS if any address is acceptable.
 */
RTR0DECL(int) RTR0MemObjAllocPhysNC(PRTR0MEMOBJ pMemObj, size_t cb, RTHCPHYS PhysHighest);

/**
 * Creates a page aligned, contiguous, physical memory object.
 *
 * No physical memory is allocated, we trust you do know what you're doing.
 *
 * @returns IPRT status code.
 * @param   pMemObj         Where to store the ring-0 memory object handle.
 * @param   Phys            The physical address to start at. This is rounded down to the
 *                          nearest page boundrary.
 * @param   cb              The size of the object in bytes. This is rounded up to nearest page boundrary.
 */
RTR0DECL(int) RTR0MemObjEnterPhys(PRTR0MEMOBJ pMemObj, RTHCPHYS Phys, size_t cb);

/**
 * Reserves kernel virtual address space.
 *
 * @returns IPRT status code.
 * @param   pMemObj         Where to store the ring-0 memory object handle.
 * @param   pvFixed         Requested address. (void *)-1 means any address. This must match the alignment.
 * @param   cb              The number of bytes to reserve. This is rounded up to nearest page.
 * @param   uAlignment      The alignment of the reserved memory.
 *                          Supported values are 0 (alias for PAGE_SIZE), PAGE_SIZE, _2M and _4M.
 */
RTR0DECL(int) RTR0MemObjReserveKernel(PRTR0MEMOBJ pMemObj, void *pvFixed, size_t cb, size_t uAlignment);

/**
 * Reserves user virtual address space in the current process.
 *
 * @returns IPRT status code.
 * @param   pMemObj         Where to store the ring-0 memory object handle.
 * @param   R3PtrFixed      Requested address. (RTR3PTR)-1 means any address. This must match the alignment.
 * @param   cb              The number of bytes to reserve. This is rounded up to nearest PAGE_SIZE.
 * @param   uAlignment      The alignment of the reserved memory.
 *                          Supported values are 0 (alias for PAGE_SIZE), PAGE_SIZE, _2M and _4M.
 * @param   R0Process       The process to reserve the memory in. NIL_R0PROCESS is an alias for the current one.
 */
RTR0DECL(int) RTR0MemObjReserveUser(PRTR0MEMOBJ pMemObj, RTR3PTR R3PtrFixed, size_t cb, size_t uAlignment, RTR0PROCESS R0Process);

/**
 * Maps a memory object into kernel virtual address space.
 *
 * @returns IPRT status code.
 * @param   pMemObj         Where to store the ring-0 memory object handle of the mapping object.
 * @param   MemObjToMap     The object to be map.
 * @param   pvFixed         Requested address. (void *)-1 means any address. This must match the alignment.
 * @param   uAlignment      The alignment of the reserved memory.
 *                          Supported values are 0 (alias for PAGE_SIZE), PAGE_SIZE, _2M and _4M.
 * @param   fProt           Combination of RTMEM_PROT_* flags (except RTMEM_PROT_NONE).
 */
RTR0DECL(int) RTR0MemObjMapKernel(PRTR0MEMOBJ pMemObj, RTR0MEMOBJ MemObjToMap, void *pvFixed, size_t uAlignment, unsigned fProt);

/**
 * Maps a memory object into user virtual address space in the current process.
 *
 * @returns IPRT status code.
 * @param   pMemObj         Where to store the ring-0 memory object handle of the mapping object.
 * @param   MemObjToMap     The object to be map.
 * @param   R3PtrFixed      Requested address. (RTR3PTR)-1 means any address. This must match the alignment.
 * @param   uAlignment      The alignment of the reserved memory.
 *                          Supported values are 0 (alias for PAGE_SIZE), PAGE_SIZE, _2M and _4M.
 * @param   fProt           Combination of RTMEM_PROT_* flags (except RTMEM_PROT_NONE).
 * @param   R0Process       The process to map the memory into. NIL_R0PROCESS is an alias for the current one.
 */
RTR0DECL(int) RTR0MemObjMapUser(PRTR0MEMOBJ pMemObj, RTR0MEMOBJ MemObjToMap, RTR3PTR R3PtrFixed, size_t uAlignment, unsigned fProt, RTR0PROCESS R0Process);

#endif /* IN_RING0 */

/** @} */

__END_DECLS

#endif

