/** @file
 *
 * Built-in drivers & devices (part 2) headers
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#ifndef __Builtins2_h__
#define __Builtins2_h__

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

