/** @file
 * innotek Portable Runtime - CRC32.
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
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#ifndef __iprt_crc32_h__
#define __iprt_crc32_h__


#include <iprt/cdefs.h>
#include <iprt/types.h>


__BEGIN_DECLS

/** @defgroup grp_rt_crc32      RTCrc32 - CRC32 Calculation
 * @ingroup grp_rt
 * @{
 */

/**
 * Calculate CRC32 for a memory block.
 *
 * @returns CRC32 for the memory block.
 * @param   pv      Pointer to the memory block.
 * @param   cb      Size of the memory block in bytes.
 */
RTDECL(uint32_t) RTCrc32(const void *pv, size_t cb);

/**
 * Start a multiblock CRC32 calculation.
 *
 * @returns Start CRC32.
 */
RTDECL(uint32_t) RTCrc32Start(void);

/**
 * Processes a multiblock of a CRC32 calculation.
 *
 * @returns Intermediate CRC32 value.
 * @param   uCRC32  Current CRC32 intermediate value.
 * @param   pv      The data block to process.
 * @param   cb      The size of the data block in bytes.
 */
RTDECL(uint32_t) RTCrc32Process(uint32_t uCRC32, const void *pv, size_t cb);

/**
 * Complete a multiblock CRC32 calculation.
 *
 * @returns CRC32 value.
 * @param   uCRC32  Current CRC32 intermediate value.
 */
RTDECL(uint32_t) RTCrc32Finish(uint32_t uCRC32);



/** @} */

__END_DECLS

#endif

