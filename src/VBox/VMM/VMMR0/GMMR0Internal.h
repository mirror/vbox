/* $Id$ */
/** @file
 * GMM - The Global Memory Manager, Internal Header.
 */

/*
 * Copyright (C) 2007 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 */

#ifndef ___GMMR0Internal_h
#define ___GMMR0Internal_h

#include <VBox/gmm.h>

/**
 * The allocation sizes.
 */
typedef struct GMMVMSIZES
{
    /** The number of pages of base memory.
     * This is the sum of RAM, ROMs and handy pages. */
    uint64_t        cBasePages;
    /** The number of pages for the shadow pool. (Can be sequeezed for memory.) */
    uint32_t        cShadowPages;
    /** The number of pages for fixed allocations like MMIO2 and the hyper heap. */
    uint32_t        cFixedPages;
} GMMVMSIZES;
typedef GMMVMSIZES *PGMMVMSIZES;


/**
 * The per-VM GMM data.
 */
typedef struct GMMPERVM
{
    /** The reservations. */
    GMMVMSIZES      Reserved;
    /** The actual allocations.
     * This includes both private and shared page allocations. */
    GMMVMSIZES      Allocated;

    /** The current number of private pages. */
    uint64_t        cPrivatePages;
    /** The current number of shared pages. */
    uint64_t        cSharedPages;
    /** The current over-comitment policy. */
    GMMOCPOLICY     enmPolicy;
    /** The VM priority for arbitrating VMs in low and out of memory situation.
     * Like which VMs to start sequeezing first. */
    GMMPRIORITY     enmPriority;

    /** The current number of ballooned pages. */
    uint64_t        cBalloonedPages;
    /** The max number of pages that can be ballooned. */
    uint64_t        cMaxBalloonedPages;
    /** The number of pages we've currently requested the guest to give us. */
    uint64_t        cReqBalloonedPages;
    /** Whether ballooning is enabled or not. */
    bool            fBallooningEnabled;

    /** Whether the VM is allowed to allocate memory or not.
     * This is used when the reservation update request fails or when the VM has
     * been told to suspend/save/die in an out-of-memory case. */
    bool            fMayAllocate;
} GMMPERVM;
/** Pointer to the per-VM GMM data. */
typedef GMMPERVM *PGMMPERVM;

#endif

