/* $Id$ */
/** @file
 * VBoxPkg.h - Common header, must be include before IPRT and VBox headers.
 */

/*
 * Copyright (C) 2009-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VBoxPkg_h
#define ___VBoxPkg_h

/*
 * IPRT configuration.
 */
#define IN_RING0
/** @todo detect this */
#if !defined(ARCH_BITS) || !defined(HC_ARCH_BITS)
# error "please add right bitness"
#endif

/*
 * VBox and IPRT headers.
 */
#include <VBox/version.h>
#include <iprt/types.h>
#ifdef _MSC_VER
# pragma warning ( disable : 4389)
# pragma warning ( disable : 4245)
# pragma warning ( disable : 4244)
#endif
#include <iprt/asm.h>
#include <iprt/asm-amd64-x86.h>
#ifdef _MSC_VER
# pragma warning ( default : 4244)
# pragma warning ( default : 4245)
# pragma warning ( default : 4389)
#endif

#endif

