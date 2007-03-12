/** @file
 *
 * VBox frontends: VBoxManage (command-line interface):
 * VBoxManage header.
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
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

#ifndef __H_VBOXMANAGE
#define __H_VBOXMANAGE

/** Syntax diagram category. */
typedef enum {
    USAGE_DUMPOPTS              = 0,
    USAGE_LIST                  = BIT64(0),
    USAGE_SHOWVMINFO            = BIT64(1),
    USAGE_REGISTERVM            = BIT64(2),
    USAGE_UNREGISTERVM          = BIT64(3),
    USAGE_CREATEVM              = BIT64(4),
    USAGE_MODIFYVM              = BIT64(5),
    USAGE_STARTVM               = BIT64(6),
    USAGE_CONTROLVM             = BIT64(7),
    USAGE_DISCARDSTATE          = BIT64(8),
    USAGE_SNAPSHOT              = BIT64(9),
    USAGE_REGISTERIMAGE         = BIT64(10),
    USAGE_UNREGISTERIMAGE       = BIT64(11),
    USAGE_SHOWVDIINFO           = BIT64(12),
    USAGE_CREATEVDI             = BIT64(13),
    USAGE_MODIFYVDI             = BIT64(14),
    USAGE_CLONEVDI              = BIT64(15),
    USAGE_ADDISCSIDISK          = BIT64(16),
    USAGE_CREATEHOSTIF          = BIT64(17),
    USAGE_REMOVEHOSTIF          = BIT64(18),
    USAGE_GETEXTRADATA          = BIT64(19),
    USAGE_SETEXTRADATA          = BIT64(20),
    USAGE_SETPROPERTY           = BIT64(21),
    USAGE_USBFILTER             = BIT64(22) | BIT64(23) | BIT64(24),
    USAGE_USBFILTER_ADD         = BIT64(22),
    USAGE_USBFILTER_MODIFY      = BIT64(23),
    USAGE_USBFILTER_REMOVE      = BIT64(24),
    USAGE_SHAREDFOLDER          = BIT64(25) | BIT64(26),
    USAGE_SHAREDFOLDER_ADD      = BIT64(25),
    USAGE_SHAREDFOLDER_REMOVE   = BIT64(26),
    USAGE_UPDATESETTINGS        = BIT64(27),
    USAGE_LOADSYMS              = BIT64(29),
    USAGE_UNLOADSYMS            = BIT64(30),
    USAGE_SETVDIUUID            = BIT64(31),
    USAGE_CONVERTDD             = BIT64(32),
    USAGE_ALL                   = (uint64_t)~0,
} USAGECATEGORY;

/** flag whether we're in internal mode */
extern bool fInternalMode;

/*
 * Prototypes
 */
int errorSyntax(USAGECATEGORY enmCmd, const char *pszFormat, ...);
int errorArgument(const char *pszFormat, ...);

void printUsageInternal(USAGECATEGORY enmCmd);
int handleInternalCommands(int argc, char *argv[],
                           ComPtr <IVirtualBox> aVirtualBox, ComPtr<ISession> aSession);


#endif /* !__H_VBOXMANAGE */
