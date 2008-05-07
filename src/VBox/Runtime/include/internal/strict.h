/* $Id$ */
/** @file
 * IPRT - Internal Header Defining Strictness Indicators.
 */

/*
 * Copyright (C) 2007 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___internal_strict_h
#define ___internal_strict_h

/** @name Strictness Indicators
 * @{ */

/** @def RTCRITSECT_STRICT
 * Enables strictness checks and lock accounting of the RTCritSect API.
 */
#if defined(DOXYGEN_RUNNING) || (!defined(RTCRITSECT_STRICT) && (defined(RT_STRICT) || defined(RT_LOCK_STRICT)))
# define RTCRITSECT_STRICT
#endif

/** @def RTSEMMUTEX_STRICT
 * Enables strictness checks and lock accounting of the RTSemMutex API.
 */
#if defined(DOXYGEN_RUNNING) || (!defined(RTSEMMUTEX_STRICT) && (defined(RT_STRICT) || defined(RT_LOCK_STRICT) || defined(RTSEM_STRICT)))
# define RTSEMMUTEX_STRICT
#endif

/** @def RTSEMRW_STRICT
 * Enables strictness checks and lock accounting of the RTSemRW API.
 */
#if defined(DOXYGEN_RUNNING) || (!defined(RTSEMRW_STRICT) && (defined(RT_STRICT) || defined(RT_LOCK_STRICT) || defined(RTSEM_STRICT)))
# define RTSEMRW_STRICT
#endif


/** @} */

#endif
