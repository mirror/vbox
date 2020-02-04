
/* $Id$ $Revision$ $Date$ $Author$ */

/** @file
 * vboximgOpts.h
 */

/*
 * Copyright (C) 2008-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOX_INCLUDED_SRC_vboximg_mount_vboximgOpts_h
#define VBOX_INCLUDED_SRC_vboximg_mount_vboximgOpts_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif


typedef struct vboximgOpts {
     char         *pszVm;                   /** optional VM UUID */
     char         *pszImageUuidOrPath;      /** Virtual Disk image UUID or path */
     uint32_t      fListMediaLong;          /** Flag to list virtual disks of all known VMs */
     uint32_t      fVerboseList;            /** FUSE parsing doesn't understand combined flags (-lv, -vl), so we kludge it */
     uint32_t      fWideList;               /** FUSE parsing doesn't understand combined flags,(-lw, -wl) so we kludge it */
     uint32_t      fList;                   /** Flag to list virtual disks of all known VMs */
     uint32_t      fListParts;              /** Flag to summarily list partitions associated with pszImage */
     uint32_t      fGstFs;                  /** Flag to try to exposes supported filesystems directly in the mountpoint inside a subdirectory */
     uint32_t      fAllowRoot;              /** Flag to allow root to access this FUSE FS */
     uint32_t      fRW;                     /** Flag to allow changes to FUSE-mounted Virtual Disk image */
     uint32_t      fWide;                   /** Flag to use wide-format list mode */
     uint32_t      fBriefUsage;             /** Flag to display only FS-specific program usage options */
     uint32_t      fVerbose;                /** Add more info to lists and operations */
} VBOXIMGOPTS;


#endif /* !VBOX_INCLUDED_SRC_vboximg_mount_vboximgOpts_h */
