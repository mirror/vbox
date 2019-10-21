/* $Id$ */
/** @file
 * PC BIOS - SCSI definitions.
 */

/*
 * Copyright (C) 2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOX_INCLUDED_SRC_PC_BIOS_scsi_h
#define VBOX_INCLUDED_SRC_PC_BIOS_scsi_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Command opcodes. */
#define SCSI_SERVICE_ACT   0x9e
#define SCSI_INQUIRY       0x12
#define SCSI_READ_CAP_10   0x25
#define SCSI_READ_10       0x28
#define SCSI_WRITE_10      0x2a
#define SCSI_READ_CAP_16   0x10    /* Not an opcode by itself, sub-action for the "Service Action" */
#define SCSI_READ_16       0x88
#define SCSI_WRITE_16      0x8a

#endif /* !VBOX_INCLUDED_SRC_PC_BIOS_scsi_h */

