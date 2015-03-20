/* $Id$ */
/** @file
 * VBoxManage - VirtualBox command-line interface, internal header file.
 */

/*
 * Copyright (C) 2006-2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___H_VBOXMANAGE
#define ___H_VBOXMANAGE

#ifndef VBOX_ONLY_DOCS
#include <VBox/com/com.h>
#include <VBox/com/ptr.h>
#include <VBox/com/VirtualBox.h>
#include <VBox/com/string.h>
#include <VBox/com/array.h>
#endif /* !VBOX_ONLY_DOCS */

#include <iprt/types.h>
#include <iprt/message.h>
#include <iprt/stream.h>
#include <iprt/getopt.h>

////////////////////////////////////////////////////////////////////////////////
//
// definitions
//
////////////////////////////////////////////////////////////////////////////////

/** @name Syntax diagram category.
 * @{ */
#define USAGE_DUMPOPTS              0
#define USAGE_LIST                  RT_BIT_64(0)
#define USAGE_SHOWVMINFO            RT_BIT_64(1)
#define USAGE_REGISTERVM            RT_BIT_64(2)
#define USAGE_UNREGISTERVM          RT_BIT_64(3)
#define USAGE_CREATEVM              RT_BIT_64(4)
#define USAGE_MODIFYVM              RT_BIT_64(5)
#define USAGE_CLONEVM               RT_BIT_64(6)
#define USAGE_STARTVM               RT_BIT_64(7)
#define USAGE_CONTROLVM             RT_BIT_64(8)
#define USAGE_DISCARDSTATE          RT_BIT_64(9)
#define USAGE_SNAPSHOT              RT_BIT_64(10)
#define USAGE_CLOSEMEDIUM           RT_BIT_64(11)
#define USAGE_SHOWMEDIUMINFO        RT_BIT_64(12)
#define USAGE_CREATEMEDIUM          RT_BIT_64(13)
#define USAGE_MODIFYMEDIUM          RT_BIT_64(14)
#define USAGE_CLONEMEDIUM           RT_BIT_64(15)
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
#define USAGE_LOADMAP               RT_BIT_64(30)
#define USAGE_SETHDUUID             RT_BIT_64(31)
#define USAGE_CONVERTFROMRAW        RT_BIT_64(32)
#define USAGE_LISTPARTITIONS        RT_BIT_64(33)
#define USAGE_CREATERAWVMDK         RT_BIT_64(34)
#define USAGE_DEBUGVM               RT_BIT_64(35)
#define USAGE_ADOPTSTATE            RT_BIT_64(36)
#define USAGE_MODINSTALL            RT_BIT_64(37)
#define USAGE_MODUNINSTALL          RT_BIT_64(38)
#define USAGE_RENAMEVMDK            RT_BIT_64(39)
#ifdef VBOX_WITH_GUEST_PROPS
# define USAGE_GUESTPROPERTY        RT_BIT_64(40)
#endif  /* VBOX_WITH_GUEST_PROPS defined */
#define USAGE_CONVERTTORAW          RT_BIT_64(41)
#define USAGE_METRICS               RT_BIT_64(42)
#define USAGE_CONVERTHD             RT_BIT_64(43)
#define USAGE_IMPORTAPPLIANCE       RT_BIT_64(44)
#define USAGE_EXPORTAPPLIANCE       RT_BIT_64(45)
#define USAGE_HOSTONLYIFS           RT_BIT_64(46)
#define USAGE_DHCPSERVER            RT_BIT_64(47)
#define USAGE_DUMPHDINFO            RT_BIT_64(48)
#define USAGE_STORAGEATTACH         RT_BIT_64(49)
#define USAGE_STORAGECONTROLLER     RT_BIT_64(50)
#ifdef VBOX_WITH_GUEST_CONTROL
# define USAGE_GUESTCONTROL         RT_BIT_64(51)
#endif  /* VBOX_WITH_GUEST_CONTROL defined */
#define USAGE_DEBUGLOG              RT_BIT_64(52)
#define USAGE_SETHDPARENTUUID       RT_BIT_64(53)
#define USAGE_PASSWORDHASH          RT_BIT_64(54)
#define USAGE_EXTPACK               RT_BIT_64(55)
#define USAGE_BANDWIDTHCONTROL      RT_BIT_64(56)
#define USAGE_GUESTSTATS            RT_BIT_64(57)
#define USAGE_REPAIRHD              RT_BIT_64(58)
#define USAGE_NATNETWORK            RT_BIT_64(59)
#define USAGE_MEDIUMPROPERTY        RT_BIT_64(60)
#define USAGE_ENCRYPTMEDIUM         RT_BIT_64(61)
#define USAGE_MEDIUMENCCHKPWD       RT_BIT_64(62)
#define USAGE_ALL                   (~(uint64_t)0)
/** @} */

#ifdef VBOX_WITH_GUEST_CONTROL
# define USAGE_GSTCTRL_EXEC         RT_BIT(0)
# define USAGE_GSTCTRL_COPYFROM     RT_BIT(1)
# define USAGE_GSTCTRL_COPYTO       RT_BIT(2)
# define USAGE_GSTCTRL_CREATEDIR    RT_BIT(3)
# define USAGE_GSTCTRL_REMOVEDIR    RT_BIT(4)
# define USAGE_GSTCTRL_REMOVEFILE   RT_BIT(5)
# define USAGE_GSTCTRL_RENAME       RT_BIT(6)
# define USAGE_GSTCTRL_CREATETEMP   RT_BIT(7)
# define USAGE_GSTCTRL_LIST         RT_BIT(8)
# define USAGE_GSTCTRL_PROCESS      RT_BIT(9)
# define USAGE_GSTCTRL_KILL         RT_BIT(10)
# define USAGE_GSTCTRL_SESSION      RT_BIT(11)
# define USAGE_GSTCTRL_STAT         RT_BIT(12)
# define USAGE_GSTCTRL_UPDATEADDS   RT_BIT(13)
# define USAGE_GSTCTRL_WATCH        RT_BIT(14)
#endif

typedef uint64_t USAGECATEGORY;

/** command handler argument */
struct HandlerArg
{
    int argc;
    char **argv;

#ifndef VBOX_ONLY_DOCS
    ComPtr<IVirtualBox> virtualBox;
    ComPtr<ISession> session;
#endif
};

/** flag whether we're in internal mode */
extern bool g_fInternalMode;

/** showVMInfo details */
typedef enum
{
    VMINFO_NONE             = 0,
    VMINFO_STANDARD         = 1,    /**< standard details */
    VMINFO_FULL             = 2,    /**< both */
    VMINFO_MACHINEREADABLE  = 3,    /**< both, and make it machine readable */
    VMINFO_COMPACT          = 4
} VMINFO_DETAILS;

////////////////////////////////////////////////////////////////////////////////
//
// global variables
//
////////////////////////////////////////////////////////////////////////////////

extern bool g_fDetailedProgress;        // in VBoxManage.cpp

////////////////////////////////////////////////////////////////////////////////
//
// prototypes
//
////////////////////////////////////////////////////////////////////////////////

/* VBoxManageHelp.cpp */
void printUsage(USAGECATEGORY fCategory, uint32_t fSubCategory, PRTSTREAM pStrm);
RTEXITCODE errorSyntax(USAGECATEGORY fCategory, const char *pszFormat, ...);
RTEXITCODE errorSyntaxEx(USAGECATEGORY fCategory, uint32_t fSubCategory, const char *pszFormat, ...);
RTEXITCODE errorGetOpt(USAGECATEGORY fCategory, int rc, union RTGETOPTUNION const *pValueUnion);
RTEXITCODE errorGetOptEx(USAGECATEGORY fCategory, uint32_t fSubCategory, int rc, union RTGETOPTUNION const *pValueUnion);
RTEXITCODE errorArgument(const char *pszFormat, ...);

void printUsageInternal(USAGECATEGORY fCategory, PRTSTREAM pStrm);

#ifndef VBOX_ONLY_DOCS
HRESULT showProgress(ComPtr<IProgress> progress);
#endif

/* VBoxManage.cpp */
void showLogo(PRTSTREAM pStrm);

#ifndef VBOX_ONLY_DOCS
RTEXITCODE readPasswordFile(const char *pszFilename, com::Utf8Str *pPasswd);

int handleInternalCommands(HandlerArg *a);
#endif /* !VBOX_ONLY_DOCS */

/* VBoxManageControlVM.cpp */
int handleControlVM(HandlerArg *a);
#ifndef VBOX_ONLY_DOCS
unsigned int getMaxNics(IVirtualBox* vbox, IMachine* mach);
#endif

/* VBoxManageModifyVM.cpp */
#ifndef VBOX_ONLY_DOCS
void parseGroups(const char *pcszGroups, com::SafeArray<BSTR> *pGroups);
#endif
int handleModifyVM(HandlerArg *a);

/* VBoxManageDebugVM.cpp */
int handleDebugVM(HandlerArg *a);

/* VBoxManageGuestProp.cpp */
extern void usageGuestProperty(PRTSTREAM pStrm, const char *pcszSep1, const char *pcszSep2);

/* VBoxManageGuestCtrl.cpp */
extern void usageGuestControl(PRTSTREAM pStrm, const char *pcszSep1, const char *pcszSep2, uint32_t fSubCategory);

#ifndef VBOX_ONLY_DOCS
/* VBoxManageGuestProp.cpp */
extern int handleGuestProperty(HandlerArg *a);

/* VBoxManageGuestCtrl.cpp */
extern int handleGuestControl(HandlerArg *a);

/* VBoxManageVMInfo.cpp */
HRESULT showSnapshots(ComPtr<ISnapshot> &rootSnapshot,
                      ComPtr<ISnapshot> &currentSnapshot,
                      VMINFO_DETAILS details,
                      const com::Utf8Str &prefix = "",
                      int level = 0);
int handleShowVMInfo(HandlerArg *a);
HRESULT showVMInfo(ComPtr<IVirtualBox> pVirtualBox,
                   ComPtr<IMachine> pMachine,
                   ComPtr<ISession> pSession,
                   VMINFO_DETAILS details = VMINFO_NONE);
const char *machineStateToName(MachineState_T machineState, bool fShort);
HRESULT showBandwidthGroups(ComPtr<IBandwidthControl> &bwCtrl,
                            VMINFO_DETAILS details);

/* VBoxManageList.cpp */
int handleList(HandlerArg *a);

/* VBoxManageMetrics.cpp */
int handleMetrics(HandlerArg *a);

/* VBoxManageMisc.cpp */
int handleRegisterVM(HandlerArg *a);
int handleUnregisterVM(HandlerArg *a);
int handleCreateVM(HandlerArg *a);
int handleCloneVM(HandlerArg *a);
int handleStartVM(HandlerArg *a);
int handleDiscardState(HandlerArg *a);
int handleAdoptState(HandlerArg *a);
int handleGetExtraData(HandlerArg *a);
int handleSetExtraData(HandlerArg *a);
int handleSetProperty(HandlerArg *a);
int handleSharedFolder(HandlerArg *a);
int handleExtPack(HandlerArg *a);

/* VBoxManageDisk.cpp */
HRESULT openMedium(HandlerArg *a, const char *pszFilenameOrUuid,
                   DeviceType_T enmDevType, AccessMode_T enmAccessMode,
                   ComPtr<IMedium> &pMedium, bool fForceNewUuidOnOpen,
                   bool fSilent);
int handleCreateMedium(HandlerArg *a);
int handleModifyMedium(HandlerArg *a);
int handleCloneMedium(HandlerArg *a);
int handleMediumProperty(HandlerArg *a);
int handleEncryptMedium(HandlerArg *a);
int handleCheckMediumPassword(HandlerArg *a);
RTEXITCODE handleConvertFromRaw(int argc, char *argv[]);
HRESULT showMediumInfo(const ComPtr<IVirtualBox> &pVirtualBox,
                       const ComPtr<IMedium> &pMedium,
                       const char *pszParentUUID,
                       bool fOptLong);
int handleShowMediumInfo(HandlerArg *a);
int handleCloseMedium(HandlerArg *a);
int parseMediumType(const char *psz, MediumType_T *penmMediumType);
int parseBool(const char *psz, bool *pb);

/* VBoxManageStorageController.cpp */
int handleStorageAttach(HandlerArg *a);
int handleStorageController(HandlerArg *a);

// VBoxManageImport.cpp
int handleImportAppliance(HandlerArg *a);
int handleExportAppliance(HandlerArg *a);

// VBoxManageSnapshot.cpp
int handleSnapshot(HandlerArg *a);

/* VBoxManageUSB.cpp */
int handleUSBFilter(HandlerArg *a);

/* VBoxManageHostonly.cpp */
int handleHostonlyIf(HandlerArg *a);

/* VBoxManageDHCPServer.cpp */
int handleDHCPServer(HandlerArg *a);

/* VBoxManageNATNetwork.cpp */
int handleNATNetwork(HandlerArg *a);


/* VBoxManageBandwidthControl.cpp */
int handleBandwidthControl(HandlerArg *a);

#endif /* !VBOX_ONLY_DOCS */

#endif /* !___H_VBOXMANAGE */
