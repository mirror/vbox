/* $Id$ */
/** @file
 * DevIommuAmd - I/O Memory Management Unit (AMD), header shared with the IOMMU, ACPI, chipset/firmware code.
 */

/*
 * Copyright (C) 2020-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOX_INCLUDED_SRC_Bus_DevIommuAmd_h
#define VBOX_INCLUDED_SRC_Bus_DevIommuAmd_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/** AMD's vendor ID. */
#define IOMMU_PCI_VENDOR_ID                         0x1022
/** VirtualBox IOMMU device ID. */
#define IOMMU_PCI_DEVICE_ID                         0xc0de
/** VirtualBox IOMMU device revision ID. */
#define IOMMU_PCI_REVISION_ID                       0x01
/** The MMIO base address of the IOMMU (taken from real hardware). */
#define IOMMU_MMIO_BASE_ADDR                        0xfeb80000
/** Size of the MMIO region in bytes. */
#define IOMMU_MMIO_REGION_SIZE                      _16K
/** Number of device table segments supported (power of 2). */
#define IOMMU_MAX_DEV_TAB_SEGMENTS                  3
/** Maximum host address translation level supported (inclusive). NOTE! If you
 *  change this make sure to change the value in ACPI tables (DevACPI.cpp) */
#define IOMMU_MAX_HOST_PT_LEVEL                     6
/** The device-specific feature major revision. */
#define IOMMU_DEVSPEC_FEAT_MAJOR_VERSION            0x1
/** The device-specific feature minor revision. */
#define IOMMU_DEVSPEC_FEAT_MINOR_VERSION            0x0
/** The device-specific control major revision. */
#define IOMMU_DEVSPEC_CTRL_MAJOR_VERSION            0x1
/** The device-specific control minor revision. */
#define IOMMU_DEVSPEC_CTRL_MINOR_VERSION            0x0
/** The device-specific status major revision. */
#define IOMMU_DEVSPEC_STATUS_MAJOR_VERSION          0x1
/** The device-specific status minor revision. */
#define IOMMU_DEVSPEC_STATUS_MINOR_VERSION          0x0

#endif /* !VBOX_INCLUDED_SRC_Bus_DevIommuAmd_h */
