/* $Id$ */
/** @file
 * PGM - Page Manager and Monitor, Physical Memory Access Template.
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/**
 * Read physical memory. (one byte/word/dword)
 *
 * This API respects access handlers and MMIO. Use PGMPhysSimpleReadGCPhys() if you
 * want to ignore those.
 *
 * @param   pVM             Pointer to the VM.
 * @param   GCPhys          Physical address start reading from.
 */
VMMDECL(PGMPHYS_DATATYPE) PGMPHYSFN_READNAME(PVM pVM, RTGCPHYS GCPhys)
{
    Assert(VM_IS_EMT(pVM));
    PGMPHYS_DATATYPE val;
    PGMPhysRead(pVM, GCPhys, &val, sizeof(val));
    return val;
}


/**
 * Write to physical memory. (one byte/word/dword)
 *
 * This API respects access handlers and MMIO. Use PGMPhysSimpleReadGCPhys() if you
 * want to ignore those.
 *
 * @param   pVM             Pointer to the VM.
 * @param   GCPhys          Physical address to write to.
 * @param   val             What to write.
 */
VMMDECL(void) PGMPHYSFN_WRITENAME(PVM pVM, RTGCPHYS GCPhys, PGMPHYS_DATATYPE val)
{
    Assert(VM_IS_EMT(pVM));
    PGMPhysWrite(pVM, GCPhys, &val, sizeof(val));
}

#undef PGMPHYSFN_READNAME
#undef PGMPHYSFN_WRITENAME
#undef PGMPHYS_DATATYPE
#undef PGMPHYS_DATASIZE

