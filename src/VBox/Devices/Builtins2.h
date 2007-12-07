/* $Id$ */
/** @file
 * Built-in drivers & devices (part 2) header.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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

__BEGIN_DECLS

#ifdef IN_VBOXDD2
extern DECLEXPORT(const unsigned char)  g_abPcBiosBinary[];
extern DECLEXPORT(const unsigned)       g_cbPcBiosBinary;
extern DECLEXPORT(const unsigned char)  g_abVgaBiosBinary[];
extern DECLEXPORT(const unsigned)       g_cbVgaBiosBinary;
extern DECLEXPORT(const unsigned char)  g_abNetBiosBinary[];
extern DECLEXPORT(const unsigned)       g_cbNetBiosBinary;
#else
extern DECLIMPORT(const unsigned char)  g_abPcBiosBinary[];
extern DECLIMPORT(const unsigned)       g_cbPcBiosBinary;
extern DECLIMPORT(const unsigned char)  g_abVgaBiosBinary[];
extern DECLIMPORT(const unsigned)       g_cbVgaBiosBinary;
extern DECLIMPORT(const unsigned char)  g_abNetBiosBinary[];
extern DECLIMPORT(const unsigned)       g_cbNetBiosBinary;
#endif
extern const PDMDEVREG g_DeviceAPIC;
extern const PDMDEVREG g_DeviceIOAPIC;

__END_DECLS

#endif

