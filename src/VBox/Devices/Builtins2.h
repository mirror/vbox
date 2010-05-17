/* $Id$ */
/** @file
 * Built-in drivers & devices part 2 header.
 *
 * These drivers and devices are in separate modules because of LGPL.
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___Builtins2_h
#define ___Builtins2_h

#include <VBox/pdm.h>

RT_C_DECLS_BEGIN

#ifdef IN_VBOXDD2
extern DECLEXPORT(const unsigned char)  g_abPcBiosBinary[];
extern DECLEXPORT(const unsigned)       g_cbPcBiosBinary;
extern DECLEXPORT(const unsigned char)  g_abVgaBiosBinary[];
extern DECLEXPORT(const unsigned)       g_cbVgaBiosBinary;
extern DECLEXPORT(const unsigned char)  g_abNetBiosBinary[];
extern DECLEXPORT(const unsigned)       g_cbNetBiosBinary;
extern DECLEXPORT(const unsigned char)  g_abVmiBiosBinary[];
extern DECLEXPORT(const unsigned)       g_cbVmiBiosBinary;
#else  /* !IN_VBOXDD2 */
extern DECLIMPORT(const unsigned char)  g_abPcBiosBinary[];
extern DECLIMPORT(const unsigned)       g_cbPcBiosBinary;
extern DECLIMPORT(const unsigned char)  g_abVgaBiosBinary[];
extern DECLIMPORT(const unsigned)       g_cbVgaBiosBinary;
extern DECLIMPORT(const unsigned char)  g_abNetBiosBinary[];
extern DECLIMPORT(const unsigned)       g_cbNetBiosBinary;
extern DECLIMPORT(const unsigned char)  g_abVmiBiosBinary[];
extern DECLIMPORT(const unsigned)       g_cbVmiBiosBinary;
#endif /* !IN_VBOXDD2 */
extern const PDMDEVREG g_DeviceAPIC;
extern const PDMDEVREG g_DeviceIOAPIC;
#ifdef VBOX_WITH_SMC
extern const PDMDEVREG g_DeviceSMC;
#endif
#ifdef VBOX_WITH_LPC
extern const PDMDEVREG g_DeviceLPC;
#endif

RT_C_DECLS_END

#endif
