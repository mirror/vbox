/** @file
 * InnoTek Portable Runtime - CRC64.
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

#ifndef __iprt_crc64_h__
#define __iprt_crc64_h__


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


