/* $Id$ */
/** @file
 * DevIommuIntel - I/O Memory Management Unit (Intel), header shared with the IOMMU, ACPI, chipset/firmware code.
 */

/*
 * Copyright (C) 2021 Oracle Corporation
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

#include <VBox/iommu-intel.h>

/** Feature/capability flags exposed to the guest (x2APIC Opt Out until we get
 *  regular APIC setup working). */
#define VTD_ACPI_DMAR_FLAGS                         (ACPI_DMAR_F_INTR_REMAP | ACPI_DMAR_F_X2APIC_OPT_OUT)
/** The MMIO base address of the DMAR unit (taken from real hardware). */
#define VTD_MMIO_BASE_ADDR                          0xfed90000


#endif /* !VBOX_INCLUDED_SRC_Bus_DevIommuIntel_h */
