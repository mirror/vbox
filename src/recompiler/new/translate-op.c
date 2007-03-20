/*
 *  Host code generation
 * 
 *  Copyright (c) 2003 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "config.h"         

#if defined(VBOX) && !defined(REMR3PHYSREADWRITE_DEFINED)
#define REMR3PHYSREADWRITE_DEFINED
/* Header sharing between vbox & qemu is rather ugly. */
void     remR3PhysReadHCPtr(uint8_t *pbSrcPhys, void *pvDst, unsigned cb);
uint8_t  remR3PhysReadHCPtrU8(uint8_t *pbSrcPhys);
int8_t   remR3PhysReadHCPtrS8(uint8_t *pbSrcPhys);
uint16_t remR3PhysReadHCPtrU16(uint8_t *pbSrcPhys);
int16_t  remR3PhysReadHCPtrS16(uint8_t *pbSrcPhys);
uint32_t remR3PhysReadHCPtrU32(uint8_t *pbSrcPhys);
int32_t  remR3PhysReadHCPtrS32(uint8_t *pbSrcPhys);
uint64_t remR3PhysReadHCPtrU64(uint8_t *pbSrcPhys);
int64_t  remR3PhysReadHCPtrS64(uint8_t *pbSrcPhys);
void     remR3PhysWriteHCPtr(uint8_t *pbDstPhys, const void *pvSrc, unsigned cb);
void     remR3PhysWriteHCPtrU8(uint8_t *pbDstPhys, uint8_t val);
void     remR3PhysWriteHCPtrU16(uint8_t *pbDstPhys, uint16_t val);
void     remR3PhysWriteHCPtrU32(uint8_t *pbDstPhys, uint32_t val);
void     remR3PhysWriteHCPtrU64(uint8_t *pbDstPhys, uint64_t val);
#endif /* VBOX */

enum {
#define DEF(s, n, copy_size) INDEX_op_ ## s,
#include "opc.h"
#undef DEF
    NB_OPS,
};

#include "dyngen.h"
#include "op.h"

