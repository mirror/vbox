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
 * The per-VM GMM data.
 */
typedef struct GMMPERVM
{
    /** The ram size, in pages. */
    uint64_t        cRAMPages;
    /** The number of private pages. */
    uint64_t        cPrivatePages;
    /** The number of shared pages. */
    uint64_t        cSharedPages;
    /** The current over-comitment policy. */
    GMMOCPOLICY     enmPolicy;
    /** The VM priority for arbitrating VMs in an out-of-memory situation. */
    GMMPRIORITY     enmPriority;
} GMMPERVM;
/** Pointer to the per-VM GMM data. */
typedef GMMPERVM *PGMMPERVM;

#endif

