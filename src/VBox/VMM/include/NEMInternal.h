/* $Id$ */
/** @file
 * NEM - Internal header file.
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

#ifndef ___NEMInternal_h
#define ___NEMInternal_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/vmm/nem.h>
#include <VBox/vmm/stam.h>
#include <VBox/vmm/vmapi.h>

RT_C_DECLS_BEGIN


/** @defgroup grp_nem_int      Internal
 * @ingroup grp_nem
 * @internal
 * @{
 */

/**
 * NEM VM Instance data.
 */
typedef struct NEM
{
    /** NEM_MAGIC. */
    uint32_t                    u32Magic;

    /** Set if enabled. */
    bool                        fEnabled;

} NEM;
/** Pointer to NEM VM instance data. */
typedef NEM *PNEM;

/** NEM::u32Magic value. */
#define NEM_MAGIC               UINT32_C(0x004d454e)
/** NEM::u32Magic value after termination. */
#define NEM_MAGIC_DEAD          UINT32_C(0xdead1111)


/**
 * NEM VMCPU Instance data.
 */
typedef struct NEMCPU
{
    /** NEMCPU_MAGIC. */
    uint32_t                    u32Magic;

} NEMCPU;
/** Pointer to NEM VMCPU instance data. */
typedef NEMCPU *PNEMCPU;

/** NEMCPU::u32Magic value. */
#define NEMCPU_MAGIC            UINT32_C(0x4d454e20)
/** NEMCPU::u32Magic value after termination. */
#define NEMCPU_MAGIC_DEAD       UINT32_C(0xdead2222)

#ifdef IN_RING3
int     nemR3NativeInit(PVM pVM, bool fFallback, bool fForced);
int     nemR3NativeInitCompleted(PVM pVM, VMINITCOMPLETED enmWhat);
int     nemR3NativeTerm(PVM pVM);
void    nemR3NativeReset(PVM pVM);
void    nemR3NativeResetCpu(PVMCPU pVCpu);
#endif


/** @} */

RT_C_DECLS_END

#endif

