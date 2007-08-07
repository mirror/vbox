/** @file
 * PDM - Pluggable Device Manager, Common Instance Macros.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VBox_pdmins_h
#define ___VBox_pdmins_h

/** @defgroup grp_pdm_ins       Common Instance Macros
 * @ingroup grp_pdm
 * @{
 */

/** @def PDMBOTHCBDECL
 * Macro for declaring a callback which is static in HC and exported in GC.
 */
#if defined(IN_GC) || defined(IN_RING0)
# define PDMBOTHCBDECL(type)    DECLEXPORT(type)
#else
# define PDMBOTHCBDECL(type)    static type
#endif

/** @def PDMINS2DATA
 * Converts a PDM Device, USB Device, or Driver instance pointer to a pointer to the instance data.
 */
#define PDMINS2DATA(pIns, type)     ( (type)(void *)&(pIns)->achInstanceData[0] )

/** @def PDMINS2DATA_GCPTR
 * Converts a PDM Device, USB Device, or Driver instance pointer to a GC pointer to the instance data.
 */
#define PDMINS2DATA_GCPTR(pIns)     ( (pIns)->pvInstanceDataGC )

/** @def PDMINS2DATA_R3PTR
 * Converts a PDM Device, USB Device, or Driver instance pointer to a HC pointer to the instance data.
 */
#define PDMINS2DATA_R3PTR(pIns)     ( (pIns)->pvInstanceDataR3 )

 /** @def PDMINS2DATA_R0PTR
 * Converts a PDM Device, USB Device, or Driver instance pointer to a R0 pointer to the instance data.
 */
#define PDMINS2DATA_R0PTR(pIns)     ( (pIns)->pvInstanceDataR0 )

#endif
