/** @file
 * innotek Portable Runtime - CRC64.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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

#ifndef ___iprt_crc64_h
#define ___iprt_crc64_h


#include <iprt/cdefs.h>
#include <iprt/types.h>


__BEGIN_DECLS

/** @defgroup grp_rt_crc64      RTCrc64 - CRC64 Calculation
 * @ingroup grp_rt
 * @{
 */

/**
 * Calculate CRC64 for a memory block.
 *
 * @returns CRC64 for the memory block.
 * @param   pv      Pointer to the memory block.
 * @param   cb      Size of the memory block in bytes.
 */
RTDECL(uint64_t) RTCrc64(const void *pv, size_t cb);

/**
 * Start a multiblock CRC64 calculation.
 *
 * @returns Start CRC64.
 */
RTDECL(uint64_t) RTCrc64Start(void);

/**
 * Processes a multiblock of a CRC64 calculation.
 *
 * @returns Intermediate CRC64 value.
 * @param   uCRC64  Current CRC64 intermediate value.
 * @param   pv      The data block to process.
 * @param   cb      The size of the data block in bytes.
 */
RTDECL(uint64_t) RTCrc64Process(uint64_t uCRC64, const void *pv, size_t cb);

/**
 * Complete a multiblock CRC64 calculation.
 *
 * @returns CRC64 value.
 * @param   uCRC64  Current CRC64 intermediate value.
 */
RTDECL(uint64_t) RTCrc64Finish(uint64_t uCRC64);



/** @} */

__END_DECLS

#endif


