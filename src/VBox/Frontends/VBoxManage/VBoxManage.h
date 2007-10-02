/** @file
 *
 * VBox frontends: VBoxManage (command-line interface):
 * VBoxManage header.
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
 */

#ifndef __H_VBOXMANAGE
#define __H_VBOXMANAGE

#include <iprt/types.h>
#include <VBox/com/ptr.h>
#include <VBox/com/VirtualBox.h>

/** Syntax diagram category. */
#define USAGE_DUMPOPTS              0
#define USAGE_LIST                  BIT64(0)
#define USAGE_SHOWVMINFO            BIT64(1)
#define USAGE_REGISTERVM            BIT64(2)
#define USAGE_UNREGISTERVM          BIT64(3)
#define USAGE_CREATEVM              BIT64(4)
#define USAGE_MODIFYVM              BIT64(5)
#define USAGE_STARTVM               BIT64(6)
#define USAGE_CONTROLVM             BIT64(7)
#define USAGE_DISCARDSTATE          BIT64(8)
#define USAGE_SNAPSHOT              BIT64(9)
#define USAGE_REGISTERIMAGE         BIT64(10)
#define USAGE_UNREGISTERIMAGE       BIT64(11)
#define USAGE_SHOWVDIINFO           BIT64(12)
#define USAGE_CREATEVDI             BIT64(13)
#define USAGE_MODIFYVDI             BIT64(14)
#define USAGE_CLONEVDI              BIT64(15)
#define USAGE_ADDISCSIDISK          BIT64(16)
#define USAGE_CREATEHOSTIF          BIT64(17)
#define USAGE_REMOVEHOSTIF          BIT64(18)
#define USAGE_GETEXTRADATA          BIT64(19)
#define USAGE_SETEXTRADATA          BIT64(20)
#define USAGE_SETPROPERTY           BIT64(21)
#define USAGE_USBFILTER             (BIT64(22) | BIT64(23) | BIT64(24))
#define USAGE_USBFILTER_ADD         BIT64(22)
#define USAGE_USBFILTER_MODIFY      BIT64(23)
#define USAGE_USBFILTER_REMOVE      BIT64(24)
#define USAGE_SHAREDFOLDER          (BIT64(25) | BIT64(26))
#define USAGE_SHAREDFOLDER_ADD      BIT64(25)
#define USAGE_SHAREDFOLDER_REMOVE   BIT64(26)
#define USAGE_UPDATESETTINGS        BIT64(27)
#define USAGE_LOADSYMS              BIT64(29)
#define USAGE_UNLOADSYMS            BIT64(30)
#define USAGE_SETVDIUUID            BIT64(31)
#define USAGE_CONVERTDD             BIT64(32)
#ifdef VBOX_OSE
#define USAGE_LISTPARTITIONS        (0)
#define USAGE_CREATERAWVMDK         (0)
#else /* !VBOX_OSE */
#define USAGE_LISTPARTITIONS        BIT64(33)
#define USAGE_CREATERAWVMDK         BIT64(34)
#endif /* !VBOX_OSE */
#define USAGE_ALL                   (~(uint64_t)0)

typedef uint64_t USAGECATEGORY;

/** flag whether we're in internal mode */
extern bool fInternalMode;

/** showVMInfo details */
typedef enum
{
    VMINFO_NONE             = 0,
    VMINFO_STANDARD         = 1,    /* standard details */
    VMINFO_STATISTICS       = 2,    /* guest statistics */
    VMINFO_FULL             = 3,    /* both */
    VMINFO_MACHINEREADABLE  = 4,    /* both, and make it machine readable */
} VMINFO_DETAILS;

/*
 * Prototypes
 */
int errorSyntax(USAGECATEGORY u64Cmd, const char *pszFormat, ...);
int errorArgument(const char *pszFormat, ...);

void printUsageInternal(USAGECATEGORY u64Cmd);
int handleInternalCommands(int argc, char *argv[],
                           ComPtr <IVirtualBox> aVirtualBox, ComPtr<ISession> aSession);
unsigned long VBoxSVNRev ();

#endif /* !__H_VBOXMANAGE */
