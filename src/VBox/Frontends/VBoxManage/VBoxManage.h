/** @file
 * VBox frontends: VBoxManage (command-line interface):
 * VBoxManage header.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___H_VBOXMANAGE
#define ___H_VBOXMANAGE

#include <iprt/types.h>
#include <VBox/com/ptr.h>
#include <VBox/com/VirtualBox.h>

/** @name Syntax diagram category.
 * @{ */
#define USAGE_DUMPOPTS              0
#define USAGE_LIST                  RT_BIT_64(0)
#define USAGE_SHOWVMINFO            RT_BIT_64(1)
#define USAGE_REGISTERVM            RT_BIT_64(2)
#define USAGE_UNREGISTERVM          RT_BIT_64(3)
#define USAGE_CREATEVM              RT_BIT_64(4)
#define USAGE_MODIFYVM              RT_BIT_64(5)
#define USAGE_STARTVM               RT_BIT_64(6)
#define USAGE_CONTROLVM             RT_BIT_64(7)
#define USAGE_DISCARDSTATE          RT_BIT_64(8)
#define USAGE_SNAPSHOT              RT_BIT_64(9)
#define USAGE_REGISTERIMAGE         RT_BIT_64(10)
#define USAGE_UNREGISTERIMAGE       RT_BIT_64(11)
#define USAGE_SHOWVDIINFO           RT_BIT_64(12)
#define USAGE_CREATEVDI             RT_BIT_64(13)
#define USAGE_MODIFYVDI             RT_BIT_64(14)
#define USAGE_CLONEVDI              RT_BIT_64(15)
#define USAGE_ADDISCSIDISK          RT_BIT_64(16)
#define USAGE_CREATEHOSTIF          RT_BIT_64(17)
#define USAGE_REMOVEHOSTIF          RT_BIT_64(18)
#define USAGE_GETEXTRADATA          RT_BIT_64(19)
#define USAGE_SETEXTRADATA          RT_BIT_64(20)
#define USAGE_SETPROPERTY           RT_BIT_64(21)
#define USAGE_USBFILTER             (RT_BIT_64(22) | RT_BIT_64(23) | RT_BIT_64(24))
#define USAGE_USBFILTER_ADD         RT_BIT_64(22)
#define USAGE_USBFILTER_MODIFY      RT_BIT_64(23)
#define USAGE_USBFILTER_REMOVE      RT_BIT_64(24)
#define USAGE_SHAREDFOLDER          (RT_BIT_64(25) | RT_BIT_64(26))
#define USAGE_SHAREDFOLDER_ADD      RT_BIT_64(25)
#define USAGE_SHAREDFOLDER_REMOVE   RT_BIT_64(26)
#define USAGE_LOADSYMS              RT_BIT_64(29)
#define USAGE_UNLOADSYMS            RT_BIT_64(30)
#define USAGE_SETVDIUUID            RT_BIT_64(31)
#define USAGE_CONVERTDD             RT_BIT_64(32)
#ifdef VBOX_OSE
# define USAGE_LISTPARTITIONS       (0)
# define USAGE_CREATERAWVMDK        (0)
#else /* !VBOX_OSE */
# define USAGE_LISTPARTITIONS       RT_BIT_64(33)
# define USAGE_CREATERAWVMDK        RT_BIT_64(34)
#endif /* !VBOX_OSE */
#define USAGE_VM_STATISTICS         RT_BIT_64(35)
#define USAGE_ADOPTSTATE            RT_BIT_64(36)
#define USAGE_MODINSTALL            RT_BIT_64(37)
#define USAGE_MODUNINSTALL          RT_BIT_64(38)
#ifdef VBOX_OSE
# define USAGE_RENAMEVMDK           (0)
#else
# define USAGE_RENAMEVMDK           RT_BIT_64(39)
#endif /* !VBOX_OSE */
#define USAGE_ALL                   (~(uint64_t)0)
/** @} */

typedef uint64_t USAGECATEGORY;

/** flag whether we're in internal mode */
extern bool g_fInternalMode;

/** showVMInfo details */
typedef enum
{
    VMINFO_NONE             = 0,
    VMINFO_STANDARD         = 1,    /**< standard details */
    VMINFO_STATISTICS       = 2,    /**< guest statistics */
    VMINFO_FULL             = 3,    /**< both */
    VMINFO_MACHINEREADABLE  = 4     /**< both, and make it machine readable */
} VMINFO_DETAILS;

/*
 * Prototypes
 */
int errorSyntax(USAGECATEGORY u64Cmd, const char *pszFormat, ...);
int errorArgument(const char *pszFormat, ...);

void printUsageInternal(USAGECATEGORY u64Cmd);
int handleInternalCommands(int argc, char *argv[],
                           ComPtr <IVirtualBox> aVirtualBox, ComPtr<ISession> aSession);
unsigned long VBoxSVNRev();

#endif /* !___H_VBOXMANAGE */
