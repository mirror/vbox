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
    USAGE_LIST                  = BIT(0),
    USAGE_SHOWVMINFO            = BIT(1),
    USAGE_REGISTERVM            = BIT(2),
    USAGE_UNREGISTERVM          = BIT(3),
    USAGE_CREATEVM              = BIT(4),
    USAGE_MODIFYVM              = BIT(5),
    USAGE_STARTVM               = BIT(6),
    USAGE_CONTROLVM             = BIT(7),
    USAGE_DISCARDSTATE          = BIT(8),
    USAGE_SNAPSHOT              = BIT(9),
    USAGE_REGISTERIMAGE         = BIT(10),
    USAGE_UNREGISTERIMAGE       = BIT(11),
    USAGE_SHOWVDIINFO           = BIT(12),
    USAGE_CREATEVDI             = BIT(13),
    USAGE_MODIFYVDI             = BIT(14),
    USAGE_CLONEVDI              = BIT(15),
    USAGE_ADDISCSIDISK          = BIT(16),
    USAGE_CREATEHOSTIF          = BIT(17),
    USAGE_REMOVEHOSTIF          = BIT(18),
    USAGE_GETEXTRADATA          = BIT(19),
    USAGE_SETEXTRADATA          = BIT(20),
    USAGE_SETPROPERTY           = BIT(21),
    USAGE_USBFILTER             = BIT(22) | BIT(23) | BIT(24),
    USAGE_USBFILTER_ADD         = BIT(22),
    USAGE_USBFILTER_MODIFY      = BIT(23),
    USAGE_USBFILTER_REMOVE      = BIT(24),
    USAGE_SHAREDFOLDER          = BIT(25) | BIT(26),
    USAGE_SHAREDFOLDER_ADD      = BIT(25),
    USAGE_SHAREDFOLDER_REMOVE   = BIT(26),
    USAGE_UPDATESETTINGS        = BIT(27),
    USAGE_LOADSYMS              = BIT(29),
    USAGE_UNLOADSYMS            = BIT(30),
    USAGE_SETVDIUUID            = BIT(31),
    USAGE_ALL                   = ~0,
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
