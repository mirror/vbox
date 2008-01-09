/** @file
 * PDM - Pluggable Device Manager, Common Instance Macros.
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
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
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

/** @def PDMINS_2_DATA
 * Converts a PDM Device, USB Device, or Driver instance pointer to a pointer to the instance data.
 */
#define PDMINS_2_DATA(pIns, type)   ( (type)(void *)&(pIns)->achInstanceData[0] )

/** @def PDMINS2DATA
 * Converts a PDM Device, USB Device, or Driver instance pointer to a pointer to the instance data.
 * @deprecated  Use PDMINS_2_DATA.
 */
#define PDMINS2DATA(pIns, type)     PDMINS_2_DATA(pIns, type)

/** @def PDMINS2DATA_GCPTR
 * Converts a PDM Device, USB Device, or Driver instance pointer to a GC pointer to the instance data.
 */
#define PDMINS_2_DATA_GCPTR(pIns)   ( (pIns)->pvInstanceDataGC )

/** @def PDMINS2DATA_GCPTR
 * Converts a PDM Device, USB Device, or Driver instance pointer to a GC pointer to the instance data.
 * @deprecated  Use PDMINS_2_DATA_GCPTR.
 */
#define PDMINS2DATA_GCPTR(pIns)     PDMINS_2_DATA_GCPTR(pIns)

/** @def PDMINS2DATA_R3PTR
 * Converts a PDM Device, USB Device, or Driver instance pointer to a HC pointer to the instance data.
 */
#define PDMINS_2_DATA_R3PTR(pIns)   ( (pIns)->pvInstanceDataR3 )

/** @def PDMINS2DATA_R3PTR
 * Converts a PDM Device, USB Device, or Driver instance pointer to a HC pointer to the instance data.
 * @deprecated  Use PDMINS_2_DATA_R3PTR
 */
#define PDMINS2DATA_R3PTR(pIns)     PDMINS_2_DATA_R3PTR(pIns)

/** @def PDMINS2DATA_R0PTR
 * Converts a PDM Device, USB Device, or Driver instance pointer to a R0 pointer to the instance data.
 */
#define PDMINS_2_DATA_R0PTR(pIns)   ( (pIns)->pvInstanceDataR0 )

/** @def PDMINS2DATA_R0PTR
 * Converts a PDM Device, USB Device, or Driver instance pointer to a R0 pointer to the instance data.
 * @deprecated  Use PDMINS_2_DATA_R0PTR
 */
#define PDMINS2DATA_R0PTR(pIns)     PDMINS_2_DATA_R0PTR(pIns)

/** @} */

#endif
