/* $Id$ */
/** @file
 * GVM - The Global VM Data.
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


#ifndef ___VBox_gvm_h
#define ___VBox_gvm_h

#include <VBox/types.h>
#include <iprt/thread.h>


/** @defgroup grp_gvm   GVM - The Global VM Data
 * @{
 */

/**
 * The Global VM Data.
 *
 * This is a ring-0 only structure where we put items we don't need to
 * share with ring-3 or GC, like for instance various RTR0MEMOBJ handles.
 *
 * Unlike VM, there are no special alignment restrictions here. The
 * paddings are checked by compile time assertions.
 */
typedef struct GVM
{
    /** Magic / eye-catcher (GVM_MAGIC). */
    uint32_t        u32Magic;
    /** The global VM handle for this VM. */
    uint32_t        hSelf;
    /** Handle to the EMT thread. */
    RTNATIVETHREAD  hEMT;
    /** The ring-0 mapping of the VM structure. */
    PVM             pVM;

    /** The GVMM per vm data. */
    struct
    {
#ifdef ___GVMMR0Internal_h
        struct GVMMPERVM    s;
#endif
        uint8_t             padding[64];
    } gvmm;

    /** The GMM per vm data. */
    struct
    {
#ifdef ___GMMR0Internal_h
        struct GMMPERVM     s;
#endif
        uint8_t             padding[256];
    } gmm;

} GVM;

/** The GVM::u32Magic value (Wayne Shorter). */
#define GVM_MAGIC       0x19330825

/** @} */

#endif
