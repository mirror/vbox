/* $Id$ */
/** @file
 * DevIommuIntel - I/O Memory Management Unit (Intel), header shared with the IOMMU, ACPI, chipset/firmware code.
 */

/*
 * Copyright (C) 2021-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOX_INCLUDED_SRC_Bus_DevIommuIntel_h
#define VBOX_INCLUDED_SRC_Bus_DevIommuIntel_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/** Intel vendor ID for the DMAR unit. */
#define DMAR_PCI_VENDOR_ID                          0x8086
/** VirtualBox DMAR unit's device ID. */
#define DMAR_PCI_DEVICE_ID                          0xc0de
/** VirtualBox DMAR unit's device revision ID. */
#define DMAR_PCI_REVISION_ID                        0x01

/** Feature/capability flags exposed to the guest. */
#define DMAR_ACPI_DMAR_FLAGS                        ACPI_DMAR_F_INTR_REMAP

/** The MMIO base address of the DMAR unit (taken from real hardware). */
#define DMAR_MMIO_BASE_PHYSADDR                     UINT64_C(0xfed90000)
/** The size of the MMIO region (in bytes). */
#define DMAR_MMIO_SIZE                              4096

#endif /* !VBOX_INCLUDED_SRC_Bus_DevIommuIntel_h */
