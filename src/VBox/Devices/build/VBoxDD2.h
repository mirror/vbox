/* $Id$ */
/** @file
 * Built-in drivers & devices part 2 header.
 *
 * These drivers and devices are in separate modules because of LGPL.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOX_INCLUDED_SRC_build_VBoxDD2_h
#define VBOX_INCLUDED_SRC_build_VBoxDD2_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/vmm/pdm.h>

RT_C_DECLS_BEGIN

#ifdef IN_VBOXDD2
extern DECLEXPORT(const unsigned char)  g_abPcBiosBinary386[];
extern DECLEXPORT(const unsigned)       g_cbPcBiosBinary386;
extern DECLEXPORT(const unsigned char)  g_abPcBiosBinary286[];
extern DECLEXPORT(const unsigned)       g_cbPcBiosBinary286;
extern DECLEXPORT(const unsigned char)  g_abPcBiosBinary8086[];
extern DECLEXPORT(const unsigned)       g_cbPcBiosBinary8086;
extern DECLEXPORT(const unsigned char)  g_abVgaBiosBinary386[];
extern DECLEXPORT(const unsigned)       g_cbVgaBiosBinary386;
extern DECLEXPORT(const unsigned char)  g_abVgaBiosBinary286[];
extern DECLEXPORT(const unsigned)       g_cbVgaBiosBinary286;
extern DECLEXPORT(const unsigned char)  g_abVgaBiosBinary8086[];
extern DECLEXPORT(const unsigned)       g_cbVgaBiosBinary8086;
# ifdef VBOX_WITH_PXE_ROM
extern DECLEXPORT(const unsigned char)  g_abNetBiosBinary[];
extern DECLEXPORT(const unsigned)       g_cbNetBiosBinary;
# endif
# ifdef VBOX_WITH_EFI_IN_DD2
extern DECLEXPORT(const unsigned char)  g_abEfiFirmware32[];
extern DECLEXPORT(const unsigned)       g_cbEfiFirmware32;
extern DECLEXPORT(const unsigned char)  g_abEfiFirmware64[];
extern DECLEXPORT(const unsigned)       g_cbEfiFirmware64;
# endif
#else  /* !IN_VBOXDD2 */
extern DECLIMPORT(const unsigned char)  g_abPcBiosBinary386[];
extern DECLIMPORT(const unsigned)       g_cbPcBiosBinary386;
extern DECLIMPORT(const unsigned char)  g_abPcBiosBinary286[];
extern DECLIMPORT(const unsigned)       g_cbPcBiosBinary286;
extern DECLIMPORT(const unsigned char)  g_abPcBiosBinary8086[];
extern DECLIMPORT(const unsigned)       g_cbPcBiosBinary8086;
extern DECLIMPORT(const unsigned char)  g_abVgaBiosBinary386[];
extern DECLIMPORT(const unsigned)       g_cbVgaBiosBinary386;
extern DECLIMPORT(const unsigned char)  g_abVgaBiosBinary286[];
extern DECLIMPORT(const unsigned)       g_cbVgaBiosBinary286;
extern DECLIMPORT(const unsigned char)  g_abVgaBiosBinary8086[];
extern DECLIMPORT(const unsigned)       g_cbVgaBiosBinary8086;
# ifdef VBOX_WITH_PXE_ROM
extern DECLIMPORT(const unsigned char)  g_abNetBiosBinary[];
extern DECLIMPORT(const unsigned)       g_cbNetBiosBinary;
# endif
# ifdef VBOX_WITH_EFI_IN_DD2
extern DECLIMPORT(const unsigned char)  g_abEfiFirmware32[];
extern DECLIMPORT(const unsigned)       g_cbEfiFirmware32;
extern DECLIMPORT(const unsigned char)  g_abEfiFirmware64[];
extern DECLIMPORT(const unsigned)       g_cbEfiFirmware64;
# endif
#endif /* !IN_VBOXDD2 */

#ifndef VBOX_WITH_NEW_LPC_DEVICE
extern const PDMDEVREG g_DeviceLPC;
#endif

RT_C_DECLS_END

#endif /* !VBOX_INCLUDED_SRC_build_VBoxDD2_h */
