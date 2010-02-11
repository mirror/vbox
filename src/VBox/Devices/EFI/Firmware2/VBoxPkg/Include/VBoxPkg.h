/* $Id$ */
/** @file
 * VBoxPkg.h - Common header, must be include before IPRT and VBox headers.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
 *
 * Sun Microsystems, Inc. confidential
 * All rights reserved
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
#ifdef _MSC_VER
# pragma warning ( default : 4244)
# pragma warning ( default : 4245)
# pragma warning ( default : 4389)
#endif

#endif

