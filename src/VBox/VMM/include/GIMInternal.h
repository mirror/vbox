/* $Id$ */
/** @file
 * GIM - Internal header file.
 */

/*
 * Copyright (C) 2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___GIMInternal_h
#define ___GIMInternal_h

#include <VBox/vmm/gim.h>

RT_C_DECLS_BEGIN

/** @defgroup grp_gim_int       Internal
 * @ingroup grp_gim
 * @internal
 * @{
 */

/**
 * GIM VM Instance data.
 * Changes to this must checked against the padding of the gim union in VM!
 */
typedef struct GIM
{
    /** Whether GIM is enabled for this VM or not. */
    bool                             fEnabled;
    /** The provider that is active for this VM. */
    GIMPROVIDERID                    enmProviderId;
    /** The interface version. */
    uint32_t                         u32Version;

#if 0
    /** Pointer to the provider's ring-3 hypercall handler. */
    R3PTRTYPE(PFNGIMHYPERCALL)       pfnHypercallR3;
    /** Pointer to the provider's ring-0 hypercall handler. */
    R0PTRTYPE(PFNGIMHYPERCALL)       pfnHypercallR0;
    /** Pointer to the provider's raw-mode context hypercall handler. */
    RCPTRTYPE(PFNGIMHYPERCALL)       pfnHypercallRC;

    /** Pointer to the provider's ring-3 MSR-read handler. */
    R3PTRTYPE(PFNGIMRDMSR)           pfnReadMsrR3;
    /** Pointer to the provider's ring-0 MSR-read handler. */
    R0PTRTYPE(PFNGIMRDMSR)           pfnReadMsrR0;
    /** Pointer to the provider's raw-mode context MSR-read handler. */
    RCPTRTYPE(PFNGIMRDMSR)           pfnReadMsrRC;

    /** Pointer to the provider's ring-3 MSR-read handler. */
    R3PTRTYPE(PFNGIMWDMSR)           pfnWriteMsrR3;
    /** Pointer to the provider's ring-0 MSR-read handler. */
    R0PTRTYPE(PFNGIMWDMSR)           pfnWriteMsrRR0;
    /** Pointer to the provider's raw-mode context MSR-read handler. */
    RCPTRTYPE(PFNGIMWDMSR)           pfnWriteMsrRRC;
#endif

    union
    {
        struct
        {
        } minimal;

        struct
        {
            /** Hypervisor system identity - Minor version number. */
            uint16_t                 u16HyperIdVersionMinor;
            /** Hypervisor system identity - Major version number. */
            uint16_t                 u16HyperIdVersionMajor;
            /** Hypervisor system identify - Build number. */
            uint32_t                 u32HyperIdBuildNumber;

            /** Guest OS identity MSR. */
            uint64_t                 u64GuestOsIdMsr;
            /** Reference TSC page MSR. */
            uint64_t                 u64TscPageMsr;
        } hv;

        /** @todo KVM and others. */
    } u;
} GIM;
/** Pointer to GIM VM instance data. */
typedef GIM *PGIM;


/**
 * GIM VMCPU Instance data.
 */
typedef struct GIMCPU
{
} GIMCPU;
/** Pointer to GIM VMCPU instance data. */
typedef GIMCPU *PGIMCPU;


#ifdef IN_RING0
#endif /* IN_RING0 */

/** @} */

RT_C_DECLS_END

#endif /* ___GIMInternal_h */

