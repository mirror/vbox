/* $Id$ */
/** @file
 * VBoxManage - The 'showvminfo' command and helper routines.
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOX_ONLY_DOCS


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <VBox/com/com.h>
#include <VBox/com/string.h>
#include <VBox/com/Guid.h>
#include <VBox/com/array.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint.h>

#include <VBox/com/VirtualBox.h>

#ifdef VBOX_WITH_PCI_PASSTHROUGH
#include <VBox/pci.h>
#endif

#include <VBox/log.h>
#include <VBox/version.h>
#include <iprt/stream.h>
#include <iprt/time.h>
#include <iprt/string.h>
#include <iprt/getopt.h>
#include <iprt/ctype.h>

#include "VBoxManage.h"
using namespace com;


// funcs
///////////////////////////////////////////////////////////////////////////////

HRESULT showSnapshots(ComPtr<ISnapshot> &rootSnapshot,
                      ComPtr<ISnapshot> &currentSnapshot,
                      VMINFO_DETAILS details,
                      const Utf8Str &prefix /* = ""*/,
                      int level /*= 0*/)
{
    /* start with the root */
    Bstr name;
    Bstr uuid;
    Bstr description;
    CHECK_ERROR2I_RET(rootSnapshot, COMGETTER(Name)(name.asOutParam()), hrcCheck);
    CHECK_ERROR2I_RET(rootSnapshot, COMGETTER(Id)(uuid.asOutParam()), hrcCheck);
    CHECK_ERROR2I_RET(rootSnapshot, COMGETTER(Description)(description.asOutParam()), hrcCheck);
    bool fCurrent = (rootSnapshot == currentSnapshot);
    if (details == VMINFO_MACHINEREADABLE)
    {
        /* print with hierarchical numbering */
        RTPrintf("SnapshotName%s=\"%ls\"\n", prefix.c_str(), name.raw());
        RTPrintf("SnapshotUUID%s=\"%s\"\n", prefix.c_str(), Utf8Str(uuid).c_str());
        if (!description.isEmpty())
            RTPrintf("SnapshotDescription%s=\"%ls\"\n", prefix.c_str(), description.raw());
        if (fCurrent)
        {
            RTPrintf("CurrentSnapshotName=\"%ls\"\n", name.raw());
            RTPrintf("CurrentSnapshotUUID=\"%s\"\n", Utf8Str(uuid).c_str());
            RTPrintf("CurrentSnapshotNode=\"SnapshotName%s\"\n", prefix.c_str());
        }
    }
    else
    {
        /* print with indentation */
        RTPrintf("   %sName: %ls (UUID: %s)%s\n",
                 prefix.c_str(),
                 name.raw(),
                 Utf8Str(uuid).c_str(),
                 (fCurrent) ? " *" : "");
        if (!description.isEmpty())
            RTPrintf("   %sDescription:\n%ls\n", prefix.c_str(), description.raw());
    }

    /* get the children */
    HRESULT hrc = S_OK;
    SafeIfaceArray <ISnapshot> coll;
    CHECK_ERROR2I_RET(rootSnapshot,COMGETTER(Children)(ComSafeArrayAsOutParam(coll)), hrcCheck);
    if (!coll.isNull())
    {
        for (size_t index = 0; index < coll.size(); ++index)
        {
            ComPtr<ISnapshot> snapshot = coll[index];
            if (snapshot)
            {
                Utf8Str newPrefix;
                if (details == VMINFO_MACHINEREADABLE)
                    newPrefix = Utf8StrFmt("%s-%d", prefix.c_str(), index + 1);
                else
                {
                    newPrefix = Utf8StrFmt("%s   ", prefix.c_str());
                }

                /* recursive call */
                HRESULT hrc2 = showSnapshots(snapshot, currentSnapshot, details, newPrefix, level + 1);
                if (FAILED(hrc2))
                    hrc = hrc2;
            }
        }
    }
    return hrc;
}

static void makeTimeStr(char *s, int cb, int64_t millies)
{
    RTTIME t;
    RTTIMESPEC ts;

    RTTimeSpecSetMilli(&ts, millies);

    RTTimeExplode(&t, &ts);

    RTStrPrintf(s, cb, "%04d/%02d/%02d %02d:%02d:%02d UTC",
                        t.i32Year, t.u8Month, t.u8MonthDay,
                        t.u8Hour, t.u8Minute, t.u8Second);
}

const char *machineStateToName(MachineState_T machineState, bool fShort)
{
    switch (machineState)
    {
        case MachineState_PoweredOff:
            return fShort ? "poweroff"             : "powered off";
        case MachineState_Saved:
            return "saved";
        case MachineState_Teleported:
            return "teleported";
        case MachineState_Aborted:
            return "aborted";
        case MachineState_Running:
            return "running";
        case MachineState_Paused:
            return "paused";
        case MachineState_Stuck:
            return fShort ? "gurumeditation"       : "guru meditation";
        case MachineState_Teleporting:
            return "teleporting";
        case MachineState_LiveSnapshotting:
            return fShort ? "livesnapshotting"     : "live snapshotting";
        case MachineState_Starting:
            return "starting";
        case MachineState_Stopping:
            return "stopping";
        case MachineState_Saving:
            return "saving";
        case MachineState_Restoring:
            return "restoring";
        case MachineState_TeleportingPausedVM:
            return fShort ? "teleportingpausedvm"  : "teleporting paused vm";
        case MachineState_TeleportingIn:
            return fShort ? "teleportingin"        : "teleporting (incoming)";
        case MachineState_FaultTolerantSyncing:
            return fShort ? "faulttolerantsyncing" : "fault tolerant syncing";
        case MachineState_DeletingSnapshotOnline:
            return fShort ? "deletingsnapshotlive" : "deleting snapshot live";
        case MachineState_DeletingSnapshotPaused:
            return fShort ? "deletingsnapshotlivepaused" : "deleting snapshot live paused";
        case MachineState_OnlineSnapshotting:
            return fShort ? "onlinesnapshotting"   : "online snapshotting";
        case MachineState_RestoringSnapshot:
            return fShort ? "restoringsnapshot"    : "restoring snapshot";
        case MachineState_DeletingSnapshot:
            return fShort ? "deletingsnapshot"     : "deleting snapshot";
        case MachineState_SettingUp:
            return fShort ? "settingup"            : "setting up";
        case MachineState_Snapshotting:
            return fShort ? "snapshotting"         : "offline snapshotting";
        default:
            break;
    }
    return "unknown";
}

const char *facilityStateToName(AdditionsFacilityStatus_T faStatus, bool fShort)
{
    switch (faStatus)
    {
        case AdditionsFacilityStatus_Inactive:
            return fShort ? "inactive" : "not active";
        case AdditionsFacilityStatus_Paused:
            return "paused";
        case AdditionsFacilityStatus_PreInit:
            return fShort ? "preinit" : "pre-initializing";
        case AdditionsFacilityStatus_Init:
            return fShort ? "init"    : "initializing";
        case AdditionsFacilityStatus_Active:
            return fShort ? "active"  : "active/running";
        case AdditionsFacilityStatus_Terminating:
            return "terminating";
        case AdditionsFacilityStatus_Terminated:
            return "terminated";
        case AdditionsFacilityStatus_Failed:
            return "failed";
        case AdditionsFacilityStatus_Unknown:
        default:
            break;
    }
    return "unknown";
}

/**
 * This takes care of escaping double quotes and slashes that the string might
 * contain.
 *
 * @param   pszName             The variable name.
 * @param   pszValue            The value.
 */
static void outputMachineReadableString(const char *pszName, const char *pszValue)
{
    Assert(strpbrk(pszName, "\"\\") == NULL);

    if (   !pszValue
        || !*pszValue
        || (   strchr(pszValue, '"') == NULL
            && strchr(pszValue, '\\') == NULL) )
        RTPrintf("%s=\"%s\"\n", pszName, pszValue);
    else
    {
        /* The value needs escaping. */
        RTPrintf("%s=\"", pszName);
        const char *psz = pszValue;
        for (;;)
        {
            const char *pszNext = strpbrk(psz, "\"\\");
            if (!pszNext)
            {
                RTPrintf("%s", psz);
                break;
            }
            RTPrintf("%.*s\\%c", pszNext - psz, psz, *pszNext);
            psz = pszNext + 1;
        }
        RTPrintf("\"\n");
    }
}


/**
 * This takes care of escaping double quotes and slashes that the string might
 * contain.
 *
 * @param   pszName             The variable name.
 * @param   pbstrValue          The value.
 */
static void outputMachineReadableString(const char *pszName, Bstr const *pbstrValue)
{
    com::Utf8Str strValue(*pbstrValue);
    outputMachineReadableString(pszName, strValue.c_str());
}

/**
 * Converts bandwidth group type to a string.
 * @returns String representation.
 * @param   enmType         Bandwidth control group type.
 */
inline const char * bwGroupTypeToString(BandwidthGroupType_T enmType)
{
    switch (enmType)
    {
        case BandwidthGroupType_Null:    return "Null";
        case BandwidthGroupType_Disk:    return "Disk";
        case BandwidthGroupType_Network: return "Network";
    }
    return "unknown";
}

HRESULT showBandwidthGroups(ComPtr<IBandwidthControl> &bwCtrl,
                            VMINFO_DETAILS details)
{
    int rc = S_OK;
    SafeIfaceArray<IBandwidthGroup> bwGroups;

    CHECK_ERROR_RET(bwCtrl, GetAllBandwidthGroups(ComSafeArrayAsOutParam(bwGroups)), rc);

    if (bwGroups.size() && details != VMINFO_MACHINEREADABLE)
        RTPrintf("\n\n");
    for (size_t i = 0; i < bwGroups.size(); i++)
    {
        Bstr strName;
        LONG64 cMaxBytesPerSec;
        BandwidthGroupType_T enmType;

        CHECK_ERROR_RET(bwGroups[i], COMGETTER(Name)(strName.asOutParam()), rc);
        CHECK_ERROR_RET(bwGroups[i], COMGETTER(Type)(&enmType), rc);
        CHECK_ERROR_RET(bwGroups[i], COMGETTER(MaxBytesPerSec)(&cMaxBytesPerSec), rc);

        const char *pszType = bwGroupTypeToString(enmType);
        if (details == VMINFO_MACHINEREADABLE)
            RTPrintf("BandwidthGroup%zu=%ls,%s,%lld\n", i, strName.raw(), pszType, cMaxBytesPerSec);
        else
        {
            const char *pszUnits = "";
            LONG64 cBytes = cMaxBytesPerSec;
            if (cBytes == 0)
            {
                RTPrintf("Name: '%ls', Type: %s, Limit: none (disabled)\n", strName.raw(), pszType);
                continue;
            }
            else if (!(cBytes % _1G))
            {
                pszUnits = "G";
                cBytes /= _1G;
            }
            else if (!(cBytes % _1M))
            {
                pszUnits = "M";
                cBytes /= _1M;
            }
            else if (!(cBytes % _1K))
            {
                pszUnits = "K";
                cBytes /= _1K;
            }
            const char *pszNetUnits = NULL;
            if (enmType == BandwidthGroupType_Network)
            {
                /*
                 * We want to report network rate limit in bits/s, not bytes.
                 * Only if it cannot be express it in kilobits we will fall
                 * back to reporting it in bytes.
                 */
                LONG64 cBits = cMaxBytesPerSec;
                if (!(cBits % 125))
                {
                    cBits /= 125;
                    pszNetUnits = "k";
                    if (!(cBits % 1000000))
                    {
                        cBits /= 1000000;
                        pszNetUnits = "g";
                    }
                    else if (!(cBits % 1000))
                    {
                        cBits /= 1000;
                        pszNetUnits = "m";
                    }
                    RTPrintf("Name: '%ls', Type: %s, Limit: %lld %sbits/sec (%lld %sbytes/sec)\n", strName.raw(), pszType, cBits, pszNetUnits, cBytes, pszUnits);
                }
            }
            if (!pszNetUnits)
                RTPrintf("Name: '%ls', Type: %s, Limit: %lld %sbytes/sec\n", strName.raw(), pszType, cBytes, pszUnits);
        }
    }
    if (details != VMINFO_MACHINEREADABLE)
        RTPrintf(bwGroups.size() != 0 ? "\n" : "<none>\n\n");

    return rc;
}

static const char *paravirtProviderToString(ParavirtProvider_T provider, VMINFO_DETAILS details)
{
    switch (provider)
    {
        case ParavirtProvider_None:
            if (details == VMINFO_MACHINEREADABLE)
                return "none";
            return "None";

        case ParavirtProvider_Default:
            if (details == VMINFO_MACHINEREADABLE)
                return "default";
            return "Default";

        case ParavirtProvider_Legacy:
            if (details == VMINFO_MACHINEREADABLE)
                return "legacy";
            return "Legacy";

        case ParavirtProvider_Minimal:
            if (details == VMINFO_MACHINEREADABLE)
                return "minimal";
            return "Minimal";

        case ParavirtProvider_HyperV:
            if (details == VMINFO_MACHINEREADABLE)
                return "hyperv";
            return "HyperV";

        case ParavirtProvider_KVM:
            if (details == VMINFO_MACHINEREADABLE)
                return "kvm";
            return "KVM";

        default:
            if (details == VMINFO_MACHINEREADABLE)
                return "unknown";
            return "Unknown";
    }
}

/**
 * Helper for formatting an indexed name or some such thing.
 */
static const char *FmtNm(char psz[80], const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    RTStrPrintfV(psz, 80, pszFormat, va);
    va_end(va);
    return psz;
}


/* Disable global optimizations for MSC 8.0/64 to make it compile in reasonable
   time. MSC 7.1/32 doesn't have quite as much trouble with it, but still
   sufficient to qualify for this hack as well since this code isn't performance
   critical and probably won't gain much from the extra optimizing in real life. */
#if defined(_MSC_VER)
# pragma optimize("g", off)
# pragma warning(push)
# if _MSC_VER < RT_MSC_VER_VC120
#  pragma warning(disable: 4748)
# endif
#endif

HRESULT showVMInfo(ComPtr<IVirtualBox> pVirtualBox,
                   ComPtr<IMachine> machine,
                   ComPtr<ISession> pSession,
                   VMINFO_DETAILS details /*= VMINFO_NONE*/)
{
    HRESULT rc;
    ComPtr<IConsole> pConsole;
    if (pSession)
        pSession->COMGETTER(Console)(pConsole.asOutParam());

    char szNm[80];
    char szValue[256];

#define SHOW_UTF8_STRING(a_pszMachine, a_pszHuman, a_szValue) \
    do \
    { \
        Assert(a_pszHuman[strlen(a_pszHuman) - 1] == ':'); \
        if (details == VMINFO_MACHINEREADABLE) \
            outputMachineReadableString(a_pszMachine, a_szValue); \
        else \
            RTPrintf("%-28s %s\n", a_pszHuman, a_szValue); \
    } while (0)

#define SHOW_BSTR_STRING(a_pszMachine, a_pszHuman, a_bstrValue) \
    do \
    { \
        Assert(a_pszHuman[strlen(a_pszHuman) - 1] == ':'); \
        if (details == VMINFO_MACHINEREADABLE) \
            outputMachineReadableString(a_pszMachine, &a_bstrValue); \
        else \
            RTPrintf("%-28s %ls\n", a_pszHuman, a_bstrValue.raw()); \
    } while (0)

#define SHOW_BOOL_VALUE_EX(a_pszMachine, a_pszHuman, a_fValue, a_szTrue, a_szFalse) \
    do \
    { \
        if (details == VMINFO_MACHINEREADABLE) \
            outputMachineReadableString(a_pszMachine, a_fValue ? "on" : "off"); \
        else \
            RTPrintf("%-28s %s\n", a_pszHuman, a_fValue ? a_szTrue: a_szFalse); \
    } while (0)

#define SHOW_BOOL_VALUE(a_pszMachine, a_pszHuman, a_fValue) \
    SHOW_BOOL_VALUE_EX(a_pszMachine, a_pszHuman, a_fValue, "enabled", "disabled")

#define SHOW_ULONG_VALUE(a_pszMachine, a_pszHuman, a_uValue, a_pszUnit) \
    do \
    { \
        if (details == VMINFO_MACHINEREADABLE) \
            RTPrintf("%s=%u\n", a_pszMachine, a_uValue); \
        else \
            RTPrintf("%-28s %u%s\n", a_pszHuman, a_uValue, a_pszUnit); \
    } while (0)

#define SHOW_LONG64_VALUE(a_pszMachine, a_pszHuman, a_llValue, a_pszUnit) \
    do \
    { \
        if (details == VMINFO_MACHINEREADABLE) \
            RTPrintf("%s=%lld\n", a_pszMachine, a_llValue); \
        else \
            RTPrintf("%-28s %lld%s\n", a_pszHuman, a_llValue, a_pszUnit); \
    } while (0)

#define SHOW_BOOLEAN_PROP(a_pObj, a_Prop, a_pszMachine, a_pszHuman) \
    SHOW_BOOLEAN_PROP_EX(a_pObj, a_Prop, a_pszMachine, a_pszHuman, "enabled", "disabled")

#define SHOW_BOOLEAN_PROP_EX(a_pObj, a_Prop, a_pszMachine, a_pszHuman, a_szTrue, a_szFalse) \
    do \
    { \
        BOOL f; \
        CHECK_ERROR2I_RET(a_pObj, COMGETTER(a_Prop)(&f), hrcCheck); \
        if (details == VMINFO_MACHINEREADABLE) \
            outputMachineReadableString(a_pszMachine, f ? "on" : "off"); \
        else \
            RTPrintf("%-28s %s\n", a_pszHuman, f ? a_szTrue : a_szFalse); \
    } while (0)

#define SHOW_BOOLEAN_METHOD(a_pObj, a_Invocation, a_pszMachine, a_pszHuman) \
    do \
    { \
        BOOL f; \
        CHECK_ERROR2I_RET(a_pObj, a_Invocation, hrcCheck); \
        if (details == VMINFO_MACHINEREADABLE) \
            outputMachineReadableString(a_pszMachine, f ? "on" : "off"); \
        else \
            RTPrintf("%-28s %s\n", a_pszHuman, f ? "enabled" : "disabled"); \
    } while (0)

#define SHOW_STRING_PROP(a_pObj, a_Prop, a_pszMachine, a_pszHuman) \
    do \
    { \
        Bstr bstr; \
        CHECK_ERROR2I_RET(a_pObj, COMGETTER(a_Prop)(bstr.asOutParam()), hrcCheck); \
        if (details == VMINFO_MACHINEREADABLE) \
            outputMachineReadableString(a_pszMachine, &bstr); \
        else \
            RTPrintf("%-28s %ls\n", a_pszHuman, bstr.raw()); \
    } while (0)

#define SHOW_STRING_PROP_NOT_EMPTY(a_pObj, a_Prop, a_pszMachine, a_pszHuman) \
    do \
    { \
        Bstr bstr; \
        CHECK_ERROR2I_RET(a_pObj, COMGETTER(a_Prop)(bstr.asOutParam()), hrcCheck); \
        if (bstr.isNotEmpty()) \
        { \
            if (details == VMINFO_MACHINEREADABLE) \
                outputMachineReadableString(a_pszMachine, &bstr); \
            else \
                RTPrintf("%-28s %ls\n", a_pszHuman, bstr.raw()); \
        } \
    } while (0)

    /** @def SHOW_STRING_PROP_MAJ
     * For not breaking the output in a dot release we don't show default values. */
#define SHOW_STRING_PROP_MAJ(a_pObj, a_Prop, a_pszMachine, a_pszHuman, a_pszUnless, a_uMajorVer) \
    do \
    { \
        Bstr bstr; \
        CHECK_ERROR2I_RET(a_pObj, COMGETTER(a_Prop)(bstr.asOutParam()), hrcCheck); \
        if ((a_uMajorVer) <= VBOX_VERSION_MAJOR || !bstr.equals(a_pszUnless)) \
        { \
            if (details == VMINFO_MACHINEREADABLE)\
                outputMachineReadableString(a_pszMachine, &bstr); \
            else \
                RTPrintf("%-28s %ls\n", a_pszHuman, bstr.raw()); \
        } \
    } while (0)

#define SHOW_STRINGARRAY_PROP(a_pObj, a_Prop, a_pszMachine, a_pszHuman) \
    do \
    { \
        SafeArray<BSTR> array; \
        CHECK_ERROR2I_RET(a_pObj, COMGETTER(a_Prop)(ComSafeArrayAsOutParam(array)), hrcCheck); \
        Utf8Str str; \
        for (size_t i = 0; i < array.size(); i++) \
        { \
            if (i != 0) \
                str.append(","); \
            str.append(Utf8Str(array[i]).c_str()); \
        } \
        Bstr bstr(str); \
        if (details == VMINFO_MACHINEREADABLE) \
            outputMachineReadableString(a_pszMachine, &bstr); \
        else \
            RTPrintf("%-28s %ls\n", a_pszHuman, bstr.raw()); \
    } while (0)

#define SHOW_UUID_PROP(a_pObj, a_Prop, a_pszMachine, a_pszHuman) \
    SHOW_STRING_PROP(a_pObj, a_Prop, a_pszMachine, a_pszHuman)

#define SHOW_USHORT_PROP_EX2(a_pObj, a_Prop, a_pszMachine, a_pszHuman, a_pszUnit, a_szFmtMachine, a_szFmtHuman) \
    do \
    { \
        USHORT u16 = 0; \
        CHECK_ERROR2I_RET(a_pObj, COMGETTER(a_Prop)(&u16), hrcCheck); \
        if (details == VMINFO_MACHINEREADABLE) \
            RTPrintf("%s=" a_szFmtMachine "\n", a_pszMachine, u16); \
        else \
            RTPrintf("%-28s " a_szFmtHuman "%s\n", a_pszHuman, u16, u16, a_pszUnit); \
    } while (0)

#define SHOW_ULONG_PROP(a_pObj, a_Prop, a_pszMachine, a_pszHuman, a_pszUnit) \
    do \
    { \
        ULONG u32 = 0; \
        CHECK_ERROR2I_RET(a_pObj, COMGETTER(a_Prop)(&u32), hrcCheck); \
        if (details == VMINFO_MACHINEREADABLE) \
            RTPrintf("%s=%u\n", a_pszMachine, u32); \
        else \
            RTPrintf("%-28s %u%s\n", a_pszHuman, u32, a_pszUnit); \
    } while (0)

#define SHOW_LONG64_PROP(a_pObj, a_Prop, a_pszMachine, a_pszHuman, a_pszUnit) \
    do \
    { \
        LONG64 i64 = 0; \
        CHECK_ERROR2I_RET(a_pObj, COMGETTER(a_Prop)(&i64), hrcCheck); \
        if (details == VMINFO_MACHINEREADABLE) \
            RTPrintf("%s=%lld\n", a_pszMachine, i64); \
        else \
            RTPrintf("%-28s %'lld%s\n", a_pszHuman, i64, a_pszUnit); \
    } while (0)

    /*
     * The rules for output in -argdump format:
     * 1) the key part (the [0-9a-zA-Z_\-]+ string before the '=' delimiter)
     *    is all lowercase for "VBoxManage modifyvm" parameters. Any
     *    other values printed are in CamelCase.
     * 2) strings (anything non-decimal) are printed surrounded by
     *    double quotes '"'. If the strings themselves contain double
     *    quotes, these characters are escaped by '\'. Any '\' character
     *    in the original string is also escaped by '\'.
     * 3) numbers (containing just [0-9\-]) are written out unchanged.
     */

    BOOL fAccessible;
    CHECK_ERROR2I_RET(machine, COMGETTER(Accessible)(&fAccessible), hrcCheck);
    if (!fAccessible)
    {
        Bstr uuid;
        machine->COMGETTER(Id)(uuid.asOutParam());
        if (details == VMINFO_COMPACT)
            RTPrintf("\"<inaccessible>\" {%s}\n", Utf8Str(uuid).c_str());
        else
        {
            if (details == VMINFO_MACHINEREADABLE)
                RTPrintf("name=\"<inaccessible>\"\n");
            else
                RTPrintf("Name:            <inaccessible!>\n");
            if (details == VMINFO_MACHINEREADABLE)
                RTPrintf("UUID=\"%s\"\n", Utf8Str(uuid).c_str());
            else
                RTPrintf("UUID:            %s\n", Utf8Str(uuid).c_str());
            if (details != VMINFO_MACHINEREADABLE)
            {
                Bstr settingsFilePath;
                rc = machine->COMGETTER(SettingsFilePath)(settingsFilePath.asOutParam());
                RTPrintf("Config file:     %ls\n", settingsFilePath.raw());
                ComPtr<IVirtualBoxErrorInfo> accessError;
                rc = machine->COMGETTER(AccessError)(accessError.asOutParam());
                RTPrintf("Access error details:\n");
                ErrorInfo ei(accessError);
                GluePrintErrorInfo(ei);
                RTPrintf("\n");
            }
        }
        return S_OK;
    }

    if (details == VMINFO_COMPACT)
    {
        Bstr machineName;
        machine->COMGETTER(Name)(machineName.asOutParam());
        Bstr uuid;
        machine->COMGETTER(Id)(uuid.asOutParam());

        RTPrintf("\"%ls\" {%s}\n", machineName.raw(), Utf8Str(uuid).c_str());
        return S_OK;
    }

    SHOW_STRING_PROP(      machine, Name,                       "name",                 "Name:");
    SHOW_STRINGARRAY_PROP( machine, Groups,                     "groups",               "Groups:");
    Bstr osTypeId;
    CHECK_ERROR2I_RET(machine, COMGETTER(OSTypeId)(osTypeId.asOutParam()), hrcCheck);
    ComPtr<IGuestOSType> osType;
    pVirtualBox->GetGuestOSType(osTypeId.raw(), osType.asOutParam());
    if (!osType.isNull())
        SHOW_STRING_PROP(       osType, Description,                "ostype",               "Guest OS:");
    else
        SHOW_STRING_PROP(      machine, OSTypeId,                   "ostype",               "Guest OS:");
    SHOW_UUID_PROP(        machine, Id,                         "UUID",                 "UUID:");
    SHOW_STRING_PROP(      machine, SettingsFilePath,           "CfgFile",              "Config file:");
    SHOW_STRING_PROP(      machine, SnapshotFolder,             "SnapFldr",             "Snapshot folder:");
    SHOW_STRING_PROP(      machine, LogFolder,                  "LogFldr",              "Log folder:");
    SHOW_UUID_PROP(        machine, HardwareUUID,               "hardwareuuid",         "Hardware UUID:");
    SHOW_ULONG_PROP(       machine, MemorySize,                 "memory",               "Memory size",      "MB");
    SHOW_BOOLEAN_PROP(     machine, PageFusionEnabled,          "pagefusion",           "Page Fusion:");
    SHOW_ULONG_PROP(       machine, VRAMSize,                   "vram",                 "VRAM size:",        "MB");
    SHOW_ULONG_PROP(       machine, CPUExecutionCap,            "cpuexecutioncap",      "CPU exec cap:",     "%");
    SHOW_BOOLEAN_PROP(     machine, HPETEnabled,                "hpet",                 "HPET:");
    SHOW_STRING_PROP_MAJ(  machine, CPUProfile,                 "cpu-profile",          "CPUProfile:",       "host", 6);

    ChipsetType_T chipsetType;
    CHECK_ERROR2I_RET(machine, COMGETTER(ChipsetType)(&chipsetType), hrcCheck);
    const char *pszChipsetType;
    switch (chipsetType)
    {
        case ChipsetType_Null:  pszChipsetType = "invalid"; break;
        case ChipsetType_PIIX3: pszChipsetType = "piix3"; break;
        case ChipsetType_ICH9:  pszChipsetType = "ich9"; break;
        default:                AssertFailed(); pszChipsetType = "unknown"; break;
    }
    SHOW_UTF8_STRING("chipset", "Chipset:", pszChipsetType);

    FirmwareType_T firmwareType;
    CHECK_ERROR2I_RET(machine, COMGETTER(FirmwareType)(&firmwareType), hrcCheck);
    const char *pszFirmwareType;
    switch (firmwareType)
    {
        case FirmwareType_BIOS:     pszFirmwareType = "BIOS"; break;
        case FirmwareType_EFI:      pszFirmwareType = "EFI"; break;
        case FirmwareType_EFI32:    pszFirmwareType = "EFI32"; break;
        case FirmwareType_EFI64:    pszFirmwareType = "EFI64"; break;
        case FirmwareType_EFIDUAL:  pszFirmwareType = "EFIDUAL"; break;
        default:                    AssertFailed(); pszFirmwareType = "unknown"; break;
    }
    SHOW_UTF8_STRING("firmware", "Firmware:", pszFirmwareType);

    SHOW_ULONG_PROP(       machine, CPUCount,                   "cpus",                 "Number of CPUs:", "");
    SHOW_BOOLEAN_METHOD(   machine, GetCPUProperty(CPUPropertyType_PAE, &f), "pae", "PAE:");
    SHOW_BOOLEAN_METHOD(   machine, GetCPUProperty(CPUPropertyType_LongMode, &f), "longmode", "Long Mode:");
    SHOW_BOOLEAN_METHOD(   machine, GetCPUProperty(CPUPropertyType_TripleFaultReset, &f), "triplefaultreset", "Triple Fault Reset:");
    SHOW_BOOLEAN_METHOD(   machine, GetCPUProperty(CPUPropertyType_APIC, &f), "apic", "APIC:");
    SHOW_BOOLEAN_METHOD(   machine, GetCPUProperty(CPUPropertyType_X2APIC, &f), "x2apic", "X2APIC:");
    SHOW_BOOLEAN_METHOD(   machine, GetCPUProperty(CPUPropertyType_HWVirt, &f), "nested-hw-virt", "Nested VT-x/AMD-V:");
    SHOW_ULONG_PROP(       machine, CPUIDPortabilityLevel, "cpuid-portability-level",   "CPUID Portability Level:", "");

    if (details != VMINFO_MACHINEREADABLE)
        RTPrintf("%-28s ", "CPUID overrides:");
    ULONG uOrdinal = 0;
    for (uOrdinal = 0; uOrdinal < _4K; uOrdinal++)
    {
        ULONG uLeaf, uSubLeaf, uEAX, uEBX, uECX, uEDX;
        rc = machine->GetCPUIDLeafByOrdinal(uOrdinal, &uLeaf, &uSubLeaf, &uEAX, &uEBX, &uECX, &uEDX);
        if (SUCCEEDED(rc))
        {
            if (details == VMINFO_MACHINEREADABLE)
                RTPrintf("cpuid=%08x,%08x,%08x,%08x,%08x,%08x", uLeaf, uSubLeaf, uEAX, uEBX, uECX, uEDX);
            else
            {
                if (!uOrdinal)
                    RTPrintf("Leaf no.      EAX      EBX      ECX      EDX\n");
                RTPrintf("%-28s %08x/%03x  %08x %08x %08x %08x\n", "", uLeaf, uSubLeaf, uEAX, uEBX, uECX, uEDX);
            }
        }
        else
        {
            if (rc != E_INVALIDARG)
                com::GlueHandleComError(machine, "GetCPUIDLeaf", rc, __FILE__, __LINE__);
            break;
        }
    }
    if (!uOrdinal && details != VMINFO_MACHINEREADABLE)
        RTPrintf("None\n");

    ComPtr<IBIOSSettings> biosSettings;
    CHECK_ERROR2I_RET(machine, COMGETTER(BIOSSettings)(biosSettings.asOutParam()), hrcCheck);

    BIOSBootMenuMode_T bootMenuMode;
    CHECK_ERROR2I_RET(biosSettings, COMGETTER(BootMenuMode)(&bootMenuMode), hrcCheck);
    const char *pszBootMenu;
    switch (bootMenuMode)
    {
        case BIOSBootMenuMode_Disabled:
            pszBootMenu = "disabled";
            break;
        case BIOSBootMenuMode_MenuOnly:
            if (details == VMINFO_MACHINEREADABLE)
                pszBootMenu = "menuonly";
            else
                pszBootMenu = "menu only";
            break;
        default:
            if (details == VMINFO_MACHINEREADABLE)
                pszBootMenu = "messageandmenu";
            else
                pszBootMenu = "message and menu";
    }
    SHOW_UTF8_STRING("bootmenu", "Boot menu mode:", pszBootMenu);

    ComPtr<ISystemProperties> systemProperties;
    CHECK_ERROR2I_RET(pVirtualBox, COMGETTER(SystemProperties)(systemProperties.asOutParam()), hrcCheck);
    ULONG maxBootPosition = 0;
    CHECK_ERROR2I_RET(systemProperties, COMGETTER(MaxBootPosition)(&maxBootPosition), hrcCheck);
    for (ULONG i = 1; i <= maxBootPosition; i++)
    {
        DeviceType_T bootOrder;
        CHECK_ERROR2I_RET(machine, GetBootOrder(i, &bootOrder), hrcCheck);
        const char *pszDevice;
        if (bootOrder == DeviceType_Floppy)
            pszDevice = details == VMINFO_MACHINEREADABLE ? "floppy"        : "Floppy";
        else if (bootOrder == DeviceType_DVD)
            pszDevice = details == VMINFO_MACHINEREADABLE ? "dvd"           : "DVD";
        else if (bootOrder == DeviceType_HardDisk)
            pszDevice = details == VMINFO_MACHINEREADABLE ? "disk"          : "HardDisk";
        else if (bootOrder == DeviceType_Network)
            pszDevice = details == VMINFO_MACHINEREADABLE ? "net"           : "Network";
        else if (bootOrder == DeviceType_USB)
            pszDevice = details == VMINFO_MACHINEREADABLE ? "usb"           : "USB";
        else if (bootOrder == DeviceType_SharedFolder)
            pszDevice = details == VMINFO_MACHINEREADABLE ? "sharedfolder"  : "Shared Folder";
        else
            pszDevice = details == VMINFO_MACHINEREADABLE ? "none"          : "Not Assigned";
        SHOW_UTF8_STRING(FmtNm(szNm, "boot%u", i), FmtNm(szNm, "Boot Device %u:", i), pszDevice);
    }

    SHOW_BOOLEAN_PROP(biosSettings, ACPIEnabled,                "acpi",                 "ACPI:");
    SHOW_BOOLEAN_PROP(biosSettings, IOAPICEnabled,              "ioapic",               "IOAPIC:");

    APICMode_T apicMode;
    CHECK_ERROR2I_RET(biosSettings, COMGETTER(APICMode)(&apicMode), hrcCheck);
    const char *pszAPIC;
    switch (apicMode)
    {
        case APICMode_Disabled:
            pszAPIC = "disabled";
            break;
        case APICMode_APIC:
        default:
            if (details == VMINFO_MACHINEREADABLE)
                pszAPIC = "apic";
            else
                pszAPIC = "APIC";
            break;
        case APICMode_X2APIC:
            if (details == VMINFO_MACHINEREADABLE)
                pszAPIC = "x2apic";
            else
                pszAPIC = "x2APIC";
            break;
    }
    SHOW_UTF8_STRING("biosapic", "BIOS APIC mode:", pszAPIC);

    SHOW_LONG64_PROP(biosSettings,  TimeOffset,                 "biossystemtimeoffset", "Time offset:",  "ms");
    SHOW_BOOLEAN_PROP_EX(machine,   RTCUseUTC,                  "rtcuseutc",            "RTC:",         "UTC", "local time");
    SHOW_BOOLEAN_METHOD(machine, GetHWVirtExProperty(HWVirtExPropertyType_Enabled,   &f),   "hwvirtex",     "Hardw. virt.ext:");
    SHOW_BOOLEAN_METHOD(machine, GetHWVirtExProperty(HWVirtExPropertyType_NestedPaging, &f),"nestedpaging", "Nested Paging:");
    SHOW_BOOLEAN_METHOD(machine, GetHWVirtExProperty(HWVirtExPropertyType_LargePages, &f),  "largepages",   "Large Pages:");
    SHOW_BOOLEAN_METHOD(machine, GetHWVirtExProperty(HWVirtExPropertyType_VPID, &f),        "vtxvpid",      "VT-x VPID:");
    SHOW_BOOLEAN_METHOD(machine, GetHWVirtExProperty(HWVirtExPropertyType_UnrestrictedExecution, &f), "vtxux", "VT-x unr. exec.:");

    ParavirtProvider_T paravirtProvider;
    CHECK_ERROR2I_RET(machine, COMGETTER(ParavirtProvider)(&paravirtProvider), hrcCheck);
    const char *pszParavirtProvider = paravirtProviderToString(paravirtProvider, details);
    SHOW_UTF8_STRING("paravirtprovider", "Paravirt. Provider:", pszParavirtProvider);

    ParavirtProvider_T effParavirtProvider;
    CHECK_ERROR2I_RET(machine, GetEffectiveParavirtProvider(&effParavirtProvider), hrcCheck);
    const char *pszEffParavirtProvider = paravirtProviderToString(effParavirtProvider, details);
    SHOW_UTF8_STRING("effparavirtprovider", "Effective Paravirt. Prov.:", pszEffParavirtProvider);

    Bstr paravirtDebug;
    CHECK_ERROR2I_RET(machine, COMGETTER(ParavirtDebug)(paravirtDebug.asOutParam()), hrcCheck);
    if (paravirtDebug.isNotEmpty())
        SHOW_BSTR_STRING("paravirtdebug", "Paravirt. Debug:", paravirtDebug);

    MachineState_T machineState;
    CHECK_ERROR2I_RET(machine, COMGETTER(State)(&machineState), hrcCheck);
    const char *pszState = machineStateToName(machineState, details == VMINFO_MACHINEREADABLE /*=fShort*/);

    LONG64 stateSince;
    machine->COMGETTER(LastStateChange)(&stateSince);
    RTTIMESPEC timeSpec;
    RTTimeSpecSetMilli(&timeSpec, stateSince);
    char pszTime[30] = {0};
    RTTimeSpecToString(&timeSpec, pszTime, sizeof(pszTime));
    if (details == VMINFO_MACHINEREADABLE)
    {
        RTPrintf("VMState=\"%s\"\n", pszState);
        RTPrintf("VMStateChangeTime=\"%s\"\n", pszTime);

        Bstr stateFile;
        machine->COMGETTER(StateFilePath)(stateFile.asOutParam());
        if (!stateFile.isEmpty())
            RTPrintf("VMStateFile=\"%ls\"\n", stateFile.raw());
    }
    else
        RTPrintf("%-28s %s (since %s)\n", "State:", pszState, pszTime);

    SHOW_ULONG_PROP(      machine,  MonitorCount,               "monitorcount",             "Monitor count:", "");
    SHOW_BOOLEAN_PROP(    machine,  Accelerate3DEnabled,        "accelerate3d",             "3D Acceleration:");
#ifdef VBOX_WITH_VIDEOHWACCEL
    SHOW_BOOLEAN_PROP(    machine,  Accelerate2DVideoEnabled,   "accelerate2dvideo",        "2D Video Acceleration:");
#endif
    SHOW_BOOLEAN_PROP(    machine,  TeleporterEnabled,          "teleporterenabled",        "Teleporter Enabled:");
    SHOW_ULONG_PROP(      machine,  TeleporterPort,             "teleporterport",           "Teleporter Port:", "");
    SHOW_STRING_PROP(     machine,  TeleporterAddress,          "teleporteraddress",        "Teleporter Address:");
    SHOW_STRING_PROP(     machine,  TeleporterPassword,         "teleporterpassword",       "Teleporter Password:");
    SHOW_BOOLEAN_PROP(    machine,  TracingEnabled,             "tracing-enabled",          "Tracing Enabled:");
    SHOW_BOOLEAN_PROP(    machine,  AllowTracingToAccessVM,     "tracing-allow-vm-access",  "Allow Tracing to Access VM:");
    SHOW_STRING_PROP(     machine,  TracingConfig,              "tracing-config",           "Tracing Configuration:");
    SHOW_BOOLEAN_PROP(    machine,  AutostartEnabled,           "autostart-enabled",        "Autostart Enabled:");
    SHOW_ULONG_PROP(      machine,  AutostartDelay,             "autostart-delay",          "Autostart Delay:", "");
    SHOW_STRING_PROP(     machine,  DefaultFrontend,            "defaultfrontend",          "Default Frontend:");

/** @todo Convert the remainder of the function to SHOW_XXX macros and add error
 *        checking where missing. */
    /*
     * Storage Controllers and their attached Mediums.
     */
    com::SafeIfaceArray<IStorageController> storageCtls;
    CHECK_ERROR(machine, COMGETTER(StorageControllers)(ComSafeArrayAsOutParam(storageCtls)));
    for (size_t i = 0; i < storageCtls.size(); ++ i)
    {
        ComPtr<IStorageController> storageCtl = storageCtls[i];
        StorageControllerType_T    enmCtlType = StorageControllerType_Null;
        const char *pszCtl = NULL;
        ULONG ulValue = 0;
        BOOL  fBootable = FALSE;
        Bstr storageCtlName;

        storageCtl->COMGETTER(Name)(storageCtlName.asOutParam());
        if (details == VMINFO_MACHINEREADABLE)
            RTPrintf("storagecontrollername%u=\"%ls\"\n", i, storageCtlName.raw());
        else
            RTPrintf("Storage Controller Name (%u):            %ls\n", i, storageCtlName.raw());

        storageCtl->COMGETTER(ControllerType)(&enmCtlType);
        switch (enmCtlType)
        {
            case StorageControllerType_LsiLogic:
                pszCtl = "LsiLogic";
                break;
            case StorageControllerType_LsiLogicSas:
                pszCtl = "LsiLogicSas";
                break;
            case StorageControllerType_BusLogic:
                pszCtl = "BusLogic";
                break;
            case StorageControllerType_IntelAhci:
                pszCtl = "IntelAhci";
                break;
            case StorageControllerType_PIIX3:
                pszCtl = "PIIX3";
                break;
            case StorageControllerType_PIIX4:
                pszCtl = "PIIX4";
                break;
            case StorageControllerType_ICH6:
                pszCtl = "ICH6";
                break;
            case StorageControllerType_I82078:
                pszCtl = "I82078";
                break;
            case StorageControllerType_USB:
                pszCtl = "USB";
                break;

            default:
                pszCtl = "unknown";
        }
        if (details == VMINFO_MACHINEREADABLE)
            RTPrintf("storagecontrollertype%u=\"%s\"\n", i, pszCtl);
        else
            RTPrintf("Storage Controller Type (%u):            %s\n", i, pszCtl);

        storageCtl->COMGETTER(Instance)(&ulValue);
        if (details == VMINFO_MACHINEREADABLE)
            RTPrintf("storagecontrollerinstance%u=\"%lu\"\n", i, ulValue);
        else
            RTPrintf("Storage Controller Instance Number (%u): %lu\n", i, ulValue);

        storageCtl->COMGETTER(MaxPortCount)(&ulValue);
        if (details == VMINFO_MACHINEREADABLE)
            RTPrintf("storagecontrollermaxportcount%u=\"%lu\"\n", i, ulValue);
        else
            RTPrintf("Storage Controller Max Port Count (%u):  %lu\n", i, ulValue);

        storageCtl->COMGETTER(PortCount)(&ulValue);
        if (details == VMINFO_MACHINEREADABLE)
            RTPrintf("storagecontrollerportcount%u=\"%lu\"\n", i, ulValue);
        else
            RTPrintf("Storage Controller Port Count (%u):      %lu\n", i, ulValue);

        storageCtl->COMGETTER(Bootable)(&fBootable);
        if (details == VMINFO_MACHINEREADABLE)
            RTPrintf("storagecontrollerbootable%u=\"%s\"\n", i, fBootable ? "on" : "off");
        else
            RTPrintf("Storage Controller Bootable (%u):        %s\n", i, fBootable ? "on" : "off");
    }

    for (size_t j = 0; j < storageCtls.size(); ++ j)
    {
        ComPtr<IStorageController> storageCtl = storageCtls[j];
        ComPtr<IMedium> medium;
        Bstr storageCtlName;
        Bstr filePath;
        ULONG cDevices;
        ULONG cPorts;

        storageCtl->COMGETTER(Name)(storageCtlName.asOutParam());
        storageCtl->COMGETTER(MaxDevicesPerPortCount)(&cDevices);
        storageCtl->COMGETTER(PortCount)(&cPorts);

        for (ULONG i = 0; i < cPorts; ++ i)
        {
            for (ULONG k = 0; k < cDevices; ++ k)
            {
                ComPtr<IMediumAttachment> mediumAttach;
                machine->GetMediumAttachment(storageCtlName.raw(),
                                             i, k,
                                             mediumAttach.asOutParam());
                BOOL fIsEjected = FALSE;
                BOOL fTempEject = FALSE;
                DeviceType_T devType = DeviceType_Null;
                if (mediumAttach)
                {
                    mediumAttach->COMGETTER(TemporaryEject)(&fTempEject);
                    mediumAttach->COMGETTER(IsEjected)(&fIsEjected);
                    mediumAttach->COMGETTER(Type)(&devType);
                }
                rc = machine->GetMedium(storageCtlName.raw(), i, k,
                                        medium.asOutParam());
                if (SUCCEEDED(rc) && medium)
                {
                    BOOL fPassthrough = FALSE;

                    if (mediumAttach)
                        mediumAttach->COMGETTER(Passthrough)(&fPassthrough);

                    medium->COMGETTER(Location)(filePath.asOutParam());
                    Bstr uuid;
                    medium->COMGETTER(Id)(uuid.asOutParam());

                    if (details == VMINFO_MACHINEREADABLE)
                    {
                        RTPrintf("\"%ls-%d-%d\"=\"%ls\"\n", storageCtlName.raw(),
                                 i, k, filePath.raw());
                        RTPrintf("\"%ls-ImageUUID-%d-%d\"=\"%s\"\n",
                                 storageCtlName.raw(), i, k, Utf8Str(uuid).c_str());
                        if (fPassthrough)
                            RTPrintf("\"%ls-dvdpassthrough\"=\"%s\"\n", storageCtlName.raw(),
                                     fPassthrough ? "on" : "off");
                        if (devType == DeviceType_DVD)
                        {
                            RTPrintf("\"%ls-tempeject\"=\"%s\"\n", storageCtlName.raw(),
                                     fTempEject ? "on" : "off");
                            RTPrintf("\"%ls-IsEjected\"=\"%s\"\n", storageCtlName.raw(),
                                     fIsEjected ? "on" : "off");
                        }
                    }
                    else
                    {
                        RTPrintf("%ls (%d, %d): %ls (UUID: %s)",
                                 storageCtlName.raw(), i, k, filePath.raw(),
                                 Utf8Str(uuid).c_str());
                        if (fPassthrough)
                            RTPrintf(" (passthrough enabled)");
                        if (fTempEject)
                            RTPrintf(" (temp eject)");
                        if (fIsEjected)
                            RTPrintf(" (ejected)");
                        RTPrintf("\n");
                    }
                }
                else if (SUCCEEDED(rc))
                {
                    if (details == VMINFO_MACHINEREADABLE)
                    {
                        RTPrintf("\"%ls-%d-%d\"=\"emptydrive\"\n", storageCtlName.raw(), i, k);
                        if (devType == DeviceType_DVD)
                            RTPrintf("\"%ls-IsEjected\"=\"%s\"\n", storageCtlName.raw(),
                                     fIsEjected ? "on" : "off");
                    }
                    else
                    {
                        RTPrintf("%ls (%d, %d): Empty", storageCtlName.raw(), i, k);
                        if (fTempEject)
                            RTPrintf(" (temp eject)");
                        if (fIsEjected)
                            RTPrintf(" (ejected)");
                        RTPrintf("\n");
                    }
                }
                else
                {
                    if (details == VMINFO_MACHINEREADABLE)
                        RTPrintf("\"%ls-%d-%d\"=\"none\"\n", storageCtlName.raw(), i, k);
                }
            }
        }
    }

    /* get the maximum amount of NICS */
    ULONG maxNICs = getMaxNics(pVirtualBox, machine);

    for (ULONG currentNIC = 0; currentNIC < maxNICs; currentNIC++)
    {
        ComPtr<INetworkAdapter> nic;
        rc = machine->GetNetworkAdapter(currentNIC, nic.asOutParam());
        if (SUCCEEDED(rc) && nic)
        {
            FmtNm(szNm, details == VMINFO_MACHINEREADABLE ? "nic%u" : "NIC %u:", currentNIC + 1);

            BOOL fEnabled;
            nic->COMGETTER(Enabled)(&fEnabled);
            if (!fEnabled)
            {
                if (details == VMINFO_MACHINEREADABLE)
                    RTPrintf("%s=\"none\"\n", szNm);
                else
                    RTPrintf("%-28s disabled\n", szNm);
            }
            else
            {
                Bstr strMACAddress;
                nic->COMGETTER(MACAddress)(strMACAddress.asOutParam());
                Utf8Str strAttachment;
                Utf8Str strNatSettings = "";
                Utf8Str strNatForwardings = "";
                NetworkAttachmentType_T attachment;
                nic->COMGETTER(AttachmentType)(&attachment);
                switch (attachment)
                {
                    case NetworkAttachmentType_Null:
                        if (details == VMINFO_MACHINEREADABLE)
                            strAttachment = "null";
                        else
                            strAttachment = "none";
                        break;

                    case NetworkAttachmentType_NAT:
                    {
                        Bstr strNetwork;
                        ComPtr<INATEngine> engine;
                        nic->COMGETTER(NATEngine)(engine.asOutParam());
                        engine->COMGETTER(Network)(strNetwork.asOutParam());
                        com::SafeArray<BSTR> forwardings;
                        engine->COMGETTER(Redirects)(ComSafeArrayAsOutParam(forwardings));
                        strNatForwardings = "";
                        for (size_t i = 0; i < forwardings.size(); ++i)
                        {
                            bool fSkip = false;
                            BSTR r = forwardings[i];
                            Utf8Str utf = Utf8Str(r);
                            Utf8Str strName;
                            Utf8Str strProto;
                            Utf8Str strHostPort;
                            Utf8Str strHostIP;
                            Utf8Str strGuestPort;
                            Utf8Str strGuestIP;
                            size_t pos, ppos;
                            pos = ppos = 0;
                            #define ITERATE_TO_NEXT_TERM(res, str, pos, ppos)   \
                            do {                                                \
                                pos = str.find(",", ppos);                      \
                                if (pos == Utf8Str::npos)                       \
                                {                                               \
                                    Log(( #res " extracting from %s is failed\n", str.c_str())); \
                                    fSkip = true;                               \
                                }                                               \
                                res = str.substr(ppos, pos - ppos);             \
                                Log2((#res " %s pos:%d, ppos:%d\n", res.c_str(), pos, ppos)); \
                                ppos = pos + 1;                                 \
                            } while (0)
                            ITERATE_TO_NEXT_TERM(strName, utf, pos, ppos);
                            if (fSkip) continue;
                            ITERATE_TO_NEXT_TERM(strProto, utf, pos, ppos);
                            if (fSkip) continue;
                            ITERATE_TO_NEXT_TERM(strHostIP, utf, pos, ppos);
                            if (fSkip) continue;
                            ITERATE_TO_NEXT_TERM(strHostPort, utf, pos, ppos);
                            if (fSkip) continue;
                            ITERATE_TO_NEXT_TERM(strGuestIP, utf, pos, ppos);
                            if (fSkip) continue;
                            strGuestPort = utf.substr(ppos, utf.length() - ppos);
                            #undef ITERATE_TO_NEXT_TERM
                            switch (strProto.toUInt32())
                            {
                                case NATProtocol_TCP:
                                    strProto = "tcp";
                                    break;
                                case NATProtocol_UDP:
                                    strProto = "udp";
                                    break;
                                default:
                                    strProto = "unk";
                                    break;
                            }
                            if (details == VMINFO_MACHINEREADABLE)
                            {
                                strNatForwardings = Utf8StrFmt("%sForwarding(%d)=\"%s,%s,%s,%s,%s,%s\"\n",
                                    strNatForwardings.c_str(), i, strName.c_str(), strProto.c_str(),
                                    strHostIP.c_str(), strHostPort.c_str(),
                                    strGuestIP.c_str(), strGuestPort.c_str());
                            }
                            else
                            {
                                strNatForwardings = Utf8StrFmt("%sNIC %d Rule(%d):   name = %s, protocol = %s,"
                                    " host ip = %s, host port = %s, guest ip = %s, guest port = %s\n",
                                    strNatForwardings.c_str(), currentNIC + 1, i, strName.c_str(), strProto.c_str(),
                                    strHostIP.c_str(), strHostPort.c_str(),
                                    strGuestIP.c_str(), strGuestPort.c_str());
                            }
                        }
                        ULONG mtu = 0;
                        ULONG sockSnd = 0;
                        ULONG sockRcv = 0;
                        ULONG tcpSnd = 0;
                        ULONG tcpRcv = 0;
                        engine->GetNetworkSettings(&mtu, &sockSnd, &sockRcv, &tcpSnd, &tcpRcv);

/** @todo r=klaus dnsproxy etc needs to be dumped, too */
                        if (details == VMINFO_MACHINEREADABLE)
                        {
                            RTPrintf("natnet%d=\"%ls\"\n", currentNIC + 1, strNetwork.length() ? strNetwork.raw(): Bstr("nat").raw());
                            strAttachment = "nat";
                            strNatSettings = Utf8StrFmt("mtu=\"%d\"\nsockSnd=\"%d\"\nsockRcv=\"%d\"\ntcpWndSnd=\"%d\"\ntcpWndRcv=\"%d\"\n",
                                mtu, sockSnd ? sockSnd : 64, sockRcv ? sockRcv : 64, tcpSnd ? tcpSnd : 64, tcpRcv ? tcpRcv : 64);
                        }
                        else
                        {
                            strAttachment = "NAT";
                            strNatSettings = Utf8StrFmt("NIC %d Settings:  MTU: %d, Socket (send: %d, receive: %d), TCP Window (send:%d, receive: %d)\n",
                                currentNIC + 1, mtu, sockSnd ? sockSnd : 64, sockRcv ? sockRcv : 64, tcpSnd ? tcpSnd : 64, tcpRcv ? tcpRcv : 64);
                        }
                        break;
                    }

                    case NetworkAttachmentType_Bridged:
                    {
                        Bstr strBridgeAdp;
                        nic->COMGETTER(BridgedInterface)(strBridgeAdp.asOutParam());
                        if (details == VMINFO_MACHINEREADABLE)
                        {
                            RTPrintf("bridgeadapter%d=\"%ls\"\n", currentNIC + 1, strBridgeAdp.raw());
                            strAttachment = "bridged";
                        }
                        else
                            strAttachment = Utf8StrFmt("Bridged Interface '%ls'", strBridgeAdp.raw());
                        break;
                    }

                    case NetworkAttachmentType_Internal:
                    {
                        Bstr strNetwork;
                        nic->COMGETTER(InternalNetwork)(strNetwork.asOutParam());
                        if (details == VMINFO_MACHINEREADABLE)
                        {
                            RTPrintf("intnet%d=\"%ls\"\n", currentNIC + 1, strNetwork.raw());
                            strAttachment = "intnet";
                        }
                        else
                            strAttachment = Utf8StrFmt("Internal Network '%s'", Utf8Str(strNetwork).c_str());
                        break;
                    }

                    case NetworkAttachmentType_HostOnly:
                    {
                        Bstr strHostonlyAdp;
                        nic->COMGETTER(HostOnlyInterface)(strHostonlyAdp.asOutParam());
                        if (details == VMINFO_MACHINEREADABLE)
                        {
                            RTPrintf("hostonlyadapter%d=\"%ls\"\n", currentNIC + 1, strHostonlyAdp.raw());
                            strAttachment = "hostonly";
                        }
                        else
                            strAttachment = Utf8StrFmt("Host-only Interface '%ls'", strHostonlyAdp.raw());
                        break;
                    }

                    case NetworkAttachmentType_Generic:
                    {
                        Bstr strGenericDriver;
                        nic->COMGETTER(GenericDriver)(strGenericDriver.asOutParam());
                        if (details == VMINFO_MACHINEREADABLE)
                        {
                            RTPrintf("generic%d=\"%ls\"\n", currentNIC + 1, strGenericDriver.raw());
                            strAttachment = "Generic";
                        }
                        else
                        {
                            strAttachment = Utf8StrFmt("Generic '%ls'", strGenericDriver.raw());

                            // show the generic properties
                            com::SafeArray<BSTR> aProperties;
                            com::SafeArray<BSTR> aValues;
                            rc = nic->GetProperties(NULL,
                                                    ComSafeArrayAsOutParam(aProperties),
                                                    ComSafeArrayAsOutParam(aValues));
                            if (SUCCEEDED(rc))
                            {
                                strAttachment += " { ";
                                for (unsigned i = 0; i < aProperties.size(); ++i)
                                    strAttachment += Utf8StrFmt(!i ? "%ls='%ls'" : ", %ls='%ls'",
                                                                aProperties[i], aValues[i]);
                                strAttachment += " }";
                            }
                        }
                        break;
                    }

                    case NetworkAttachmentType_NATNetwork:
                    {
                        Bstr strNetwork;
                        nic->COMGETTER(NATNetwork)(strNetwork.asOutParam());
                        if (details == VMINFO_MACHINEREADABLE)
                        {
                            RTPrintf("nat-network%d=\"%ls\"\n", currentNIC + 1, strNetwork.raw());
                            strAttachment = "natnetwork";
                        }
                        else
                            strAttachment = Utf8StrFmt("NAT Network '%s'", Utf8Str(strNetwork).c_str());
                        break;
                    }

                    default:
                        strAttachment = "unknown";
                        break;
                }

                /* cable connected */
                BOOL fConnected;
                nic->COMGETTER(CableConnected)(&fConnected);

                /* promisc policy */
                NetworkAdapterPromiscModePolicy_T enmPromiscModePolicy;
                CHECK_ERROR2I_RET(nic, COMGETTER(PromiscModePolicy)(&enmPromiscModePolicy), hrcCheck);
                const char *pszPromiscuousGuestPolicy;
                switch (enmPromiscModePolicy)
                {
                    case NetworkAdapterPromiscModePolicy_Deny:          pszPromiscuousGuestPolicy = "deny"; break;
                    case NetworkAdapterPromiscModePolicy_AllowNetwork:  pszPromiscuousGuestPolicy = "allow-vms"; break;
                    case NetworkAdapterPromiscModePolicy_AllowAll:      pszPromiscuousGuestPolicy = "allow-all"; break;
                    default: AssertFailedReturn(E_INVALIDARG);
                }

                /* trace stuff */
                BOOL fTraceEnabled;
                nic->COMGETTER(TraceEnabled)(&fTraceEnabled);
                Bstr traceFile;
                nic->COMGETTER(TraceFile)(traceFile.asOutParam());

                /* NIC type */
                NetworkAdapterType_T NICType;
                nic->COMGETTER(AdapterType)(&NICType);
                const char *pszNICType;
                switch (NICType)
                {
                    case NetworkAdapterType_Am79C970A:  pszNICType = "Am79C970A";   break;
                    case NetworkAdapterType_Am79C973:   pszNICType = "Am79C973";    break;
#ifdef VBOX_WITH_E1000
                    case NetworkAdapterType_I82540EM:   pszNICType = "82540EM";     break;
                    case NetworkAdapterType_I82543GC:   pszNICType = "82543GC";     break;
                    case NetworkAdapterType_I82545EM:   pszNICType = "82545EM";     break;
#endif
#ifdef VBOX_WITH_VIRTIO
                    case NetworkAdapterType_Virtio:     pszNICType = "virtio";      break;
#endif
                    default: AssertFailed();            pszNICType = "unknown";     break;
                }

                /* reported line speed */
                ULONG ulLineSpeed;
                nic->COMGETTER(LineSpeed)(&ulLineSpeed);

                /* boot priority of the adapter */
                ULONG ulBootPriority;
                nic->COMGETTER(BootPriority)(&ulBootPriority);

                /* bandwidth group */
                ComObjPtr<IBandwidthGroup> pBwGroup;
                Bstr strBwGroup;
                nic->COMGETTER(BandwidthGroup)(pBwGroup.asOutParam());
                if (!pBwGroup.isNull())
                    pBwGroup->COMGETTER(Name)(strBwGroup.asOutParam());

                if (details == VMINFO_MACHINEREADABLE)
                {
                    RTPrintf("macaddress%d=\"%ls\"\n", currentNIC + 1, strMACAddress.raw());
                    RTPrintf("cableconnected%d=\"%s\"\n", currentNIC + 1, fConnected ? "on" : "off");
                    RTPrintf("nic%d=\"%s\"\n", currentNIC + 1, strAttachment.c_str());
                    RTPrintf("nictype%d=\"%s\"\n", currentNIC + 1, pszNICType);
                    RTPrintf("nicspeed%d=\"%d\"\n", currentNIC + 1, ulLineSpeed);
                }
                else
                    RTPrintf("%-28s MAC: %ls, Attachment: %s, Cable connected: %s, Trace: %s (file: %ls), Type: %s, Reported speed: %d Mbps, Boot priority: %d, Promisc Policy: %s, Bandwidth group: %ls\n",
                             szNm, strMACAddress.raw(), strAttachment.c_str(),
                             fConnected ? "on" : "off",
                             fTraceEnabled ? "on" : "off",
                             traceFile.isEmpty() ? Bstr("none").raw() : traceFile.raw(),
                             pszNICType,
                             ulLineSpeed / 1000,
                             (int)ulBootPriority,
                             pszPromiscuousGuestPolicy,
                             strBwGroup.isEmpty() ? Bstr("none").raw() : strBwGroup.raw());
                if (strNatSettings.length())
                    RTPrintf(strNatSettings.c_str());
                if (strNatForwardings.length())
                    RTPrintf(strNatForwardings.c_str());
            }
        }
    }

    /* Pointing device information */
    PointingHIDType_T aPointingHID;
    const char *pszHID = "Unknown";
    const char *pszMrHID = "unknown";
    machine->COMGETTER(PointingHIDType)(&aPointingHID);
    switch (aPointingHID)
    {
        case PointingHIDType_None:
            pszHID = "None";
            pszMrHID = "none";
            break;
        case PointingHIDType_PS2Mouse:
            pszHID = "PS/2 Mouse";
            pszMrHID = "ps2mouse";
            break;
        case PointingHIDType_USBMouse:
            pszHID = "USB Mouse";
            pszMrHID = "usbmouse";
            break;
        case PointingHIDType_USBTablet:
            pszHID = "USB Tablet";
            pszMrHID = "usbtablet";
            break;
        case PointingHIDType_ComboMouse:
            pszHID = "USB Tablet and PS/2 Mouse";
            pszMrHID = "combomouse";
            break;
        case PointingHIDType_USBMultiTouch:
            pszHID = "USB Multi-Touch";
            pszMrHID = "usbmultitouch";
            break;
        default:
            break;
    }
    SHOW_UTF8_STRING("hidpointing", "Pointing Device:", details == VMINFO_MACHINEREADABLE ? pszMrHID : pszHID);

    /* Keyboard device information */
    KeyboardHIDType_T aKeyboardHID;
    machine->COMGETTER(KeyboardHIDType)(&aKeyboardHID);
    pszHID = "Unknown";
    pszMrHID = "unknown";
    switch (aKeyboardHID)
    {
        case KeyboardHIDType_None:
            pszHID = "None";
            pszMrHID = "none";
            break;
        case KeyboardHIDType_PS2Keyboard:
            pszHID = "PS/2 Keyboard";
            pszMrHID = "ps2kbd";
            break;
        case KeyboardHIDType_USBKeyboard:
            pszHID = "USB Keyboard";
            pszMrHID = "usbkbd";
            break;
        case KeyboardHIDType_ComboKeyboard:
            pszHID = "USB and PS/2 Keyboard";
            pszMrHID = "combokbd";
            break;
        default:
            break;
    }
    SHOW_UTF8_STRING("hidkeyboard", "Keyboard Device:", details == VMINFO_MACHINEREADABLE ? pszMrHID : pszHID);

    ComPtr<ISystemProperties> sysProps;
    pVirtualBox->COMGETTER(SystemProperties)(sysProps.asOutParam());

    /* get the maximum amount of UARTs */
    ULONG maxUARTs = 0;
    sysProps->COMGETTER(SerialPortCount)(&maxUARTs);
    for (ULONG currentUART = 0; currentUART < maxUARTs; currentUART++)
    {
        ComPtr<ISerialPort> uart;
        rc = machine->GetSerialPort(currentUART, uart.asOutParam());
        if (SUCCEEDED(rc) && uart)
        {
            FmtNm(szNm, details == VMINFO_MACHINEREADABLE ? "uart%u" : "UART %u:", currentUART + 1);

            /* show the config of this UART */
            BOOL fEnabled;
            uart->COMGETTER(Enabled)(&fEnabled);
            if (!fEnabled)
            {
                if (details == VMINFO_MACHINEREADABLE)
                    RTPrintf("%s=\"off\"\n", szNm);
                else
                    RTPrintf("%-28s disabled\n", szNm);
            }
            else
            {
                ULONG ulIRQ, ulIOBase;
                PortMode_T HostMode;
                Bstr path;
                BOOL fServer;
                uart->COMGETTER(IRQ)(&ulIRQ);
                uart->COMGETTER(IOBase)(&ulIOBase);
                uart->COMGETTER(Path)(path.asOutParam());
                uart->COMGETTER(Server)(&fServer);
                uart->COMGETTER(HostMode)(&HostMode);

                if (details == VMINFO_MACHINEREADABLE)
                    RTPrintf("%s=\"%#06x,%d\"\n", szNm, ulIOBase, ulIRQ);
                else
                    RTPrintf("%-28s I/O base: %#06x, IRQ: %d", szNm, ulIOBase, ulIRQ);
                switch (HostMode)
                {
                    default:
                    case PortMode_Disconnected:
                        if (details == VMINFO_MACHINEREADABLE)
                            RTPrintf("uartmode%d=\"disconnected\"\n", currentUART + 1);
                        else
                            RTPrintf(", disconnected\n");
                        break;
                    case PortMode_RawFile:
                        if (details == VMINFO_MACHINEREADABLE)
                            RTPrintf("uartmode%d=\"file,%ls\"\n", currentUART + 1,
                                     path.raw());
                        else
                            RTPrintf(", attached to raw file '%ls'\n",
                                     path.raw());
                        break;
                    case PortMode_TCP:
                        if (details == VMINFO_MACHINEREADABLE)
                            RTPrintf("uartmode%d=\"%s,%ls\"\n", currentUART + 1,
                                     fServer ? "tcpserver" : "tcpclient", path.raw());
                        else
                            RTPrintf(", attached to tcp (%s) '%ls'\n",
                                     fServer ? "server" : "client", path.raw());
                        break;
                    case PortMode_HostPipe:
                        if (details == VMINFO_MACHINEREADABLE)
                            RTPrintf("uartmode%d=\"%s,%ls\"\n", currentUART + 1,
                                     fServer ? "server" : "client", path.raw());
                        else
                            RTPrintf(", attached to pipe (%s) '%ls'\n",
                                     fServer ? "server" : "client", path.raw());
                        break;
                    case PortMode_HostDevice:
                        if (details == VMINFO_MACHINEREADABLE)
                            RTPrintf("uartmode%d=\"%ls\"\n", currentUART + 1,
                                     path.raw());
                        else
                            RTPrintf(", attached to device '%ls'\n", path.raw());
                        break;
                }
            }
        }
    }

    /* get the maximum amount of LPTs */
    ULONG maxLPTs = 0;
    sysProps->COMGETTER(ParallelPortCount)(&maxLPTs);
    for (ULONG currentLPT = 0; currentLPT < maxLPTs; currentLPT++)
    {
        ComPtr<IParallelPort> lpt;
        rc = machine->GetParallelPort(currentLPT, lpt.asOutParam());
        if (SUCCEEDED(rc) && lpt)
        {
            FmtNm(szNm, details == VMINFO_MACHINEREADABLE ? "lpt%u" : "LPT %u:", currentLPT + 1);

            /* show the config of this LPT */
            BOOL fEnabled;
            lpt->COMGETTER(Enabled)(&fEnabled);
            if (!fEnabled)
            {
                if (details == VMINFO_MACHINEREADABLE)
                    RTPrintf("%s=\"off\"\n", szNm);
                else
                    RTPrintf("%-28s disabled\n", szNm);
            }
            else
            {
                ULONG ulIRQ, ulIOBase;
                Bstr path;
                lpt->COMGETTER(IRQ)(&ulIRQ);
                lpt->COMGETTER(IOBase)(&ulIOBase);
                lpt->COMGETTER(Path)(path.asOutParam());

                if (details == VMINFO_MACHINEREADABLE)
                    RTPrintf("%s=\"%#06x,%d\"\n", szNm, ulIOBase, ulIRQ);
                else
                    RTPrintf("%-28s I/O base: %#06x, IRQ: %d", szNm, ulIOBase, ulIRQ);
                if (details == VMINFO_MACHINEREADABLE)
                    RTPrintf("lptmode%d=\"%ls\"\n", currentLPT + 1, path.raw());
                else
                    RTPrintf(", attached to device '%ls'\n", path.raw());
            }
        }
    }

    ComPtr<IAudioAdapter> AudioAdapter;
    rc = machine->COMGETTER(AudioAdapter)(AudioAdapter.asOutParam());
    if (SUCCEEDED(rc))
    {
        const char *pszDrv   = "Unknown";
        const char *pszCtrl  = "Unknown";
        const char *pszCodec = "Unknown";
        BOOL fEnabled;
        rc = AudioAdapter->COMGETTER(Enabled)(&fEnabled);
        if (SUCCEEDED(rc) && fEnabled)
        {
            AudioDriverType_T enmDrvType;
            rc = AudioAdapter->COMGETTER(AudioDriver)(&enmDrvType);
            switch (enmDrvType)
            {
                case AudioDriverType_Null:
                    if (details == VMINFO_MACHINEREADABLE)
                        pszDrv = "null";
                    else
                        pszDrv = "Null";
                    break;
                case AudioDriverType_WinMM:
                    if (details == VMINFO_MACHINEREADABLE)
                        pszDrv = "winmm";
                    else
                        pszDrv = "WINMM";
                    break;
                case AudioDriverType_DirectSound:
                    if (details == VMINFO_MACHINEREADABLE)
                        pszDrv = "dsound";
                    else
                        pszDrv = "DSOUND";
                    break;
                case AudioDriverType_OSS:
                    if (details == VMINFO_MACHINEREADABLE)
                        pszDrv = "oss";
                    else
                        pszDrv = "OSS";
                    break;
                case AudioDriverType_ALSA:
                    if (details == VMINFO_MACHINEREADABLE)
                        pszDrv = "alsa";
                    else
                        pszDrv = "ALSA";
                    break;
                case AudioDriverType_Pulse:
                    if (details == VMINFO_MACHINEREADABLE)
                        pszDrv = "pulse";
                    else
                        pszDrv = "PulseAudio";
                    break;
                case AudioDriverType_CoreAudio:
                    if (details == VMINFO_MACHINEREADABLE)
                        pszDrv = "coreaudio";
                    else
                        pszDrv = "CoreAudio";
                    break;
                case AudioDriverType_SolAudio:
                    if (details == VMINFO_MACHINEREADABLE)
                        pszDrv = "solaudio";
                    else
                        pszDrv = "SolAudio";
                    break;
                default:
                    if (details == VMINFO_MACHINEREADABLE)
                        pszDrv = "unknown";
                    break;
            }
            AudioControllerType_T enmCtrlType;
            rc = AudioAdapter->COMGETTER(AudioController)(&enmCtrlType);
            switch (enmCtrlType)
            {
                case AudioControllerType_AC97:
                    if (details == VMINFO_MACHINEREADABLE)
                        pszCtrl = "ac97";
                    else
                        pszCtrl = "AC97";
                    break;
                case AudioControllerType_SB16:
                    if (details == VMINFO_MACHINEREADABLE)
                        pszCtrl = "sb16";
                    else
                        pszCtrl = "SB16";
                    break;
                case AudioControllerType_HDA:
                    if (details == VMINFO_MACHINEREADABLE)
                        pszCtrl = "hda";
                    else
                        pszCtrl = "HDA";
                    break;
                default:
                    break;
            }
            AudioCodecType_T enmCodecType;
            rc = AudioAdapter->COMGETTER(AudioCodec)(&enmCodecType);
            switch (enmCodecType)
            {
                case AudioCodecType_SB16:
                    pszCodec = "SB16";
                    break;
                case AudioCodecType_STAC9700:
                    pszCodec = "STAC9700";
                    break;
                case AudioCodecType_AD1980:
                    pszCodec = "AD1980";
                    break;
                case AudioCodecType_STAC9221:
                    pszCodec = "STAC9221";
                    break;
                case AudioCodecType_Null: break; /* Shut up MSC. */
                default:                  break;
            }
        }
        else
            fEnabled = FALSE;

        if (details == VMINFO_MACHINEREADABLE)
            RTPrintf("audio=\"%s\"\n", fEnabled ? pszDrv : "none");
        else
        {
            RTPrintf("%-28s %s", "Audio:", fEnabled ? "enabled" : "disabled");
            if (fEnabled)
                RTPrintf(" (Driver: %s, Controller: %s, Codec: %s)", pszDrv, pszCtrl, pszCodec);
            RTPrintf("\n");
        }
        SHOW_BOOLEAN_PROP(AudioAdapter, EnabledIn,  "audio_in",  "Audio playback:");
        SHOW_BOOLEAN_PROP(AudioAdapter, EnabledOut, "audio_out", "Audio capture:");
    }

    /* Shared clipboard */
    {
        const char *psz;
        ClipboardMode_T enmMode = (ClipboardMode_T)0;
        rc = machine->COMGETTER(ClipboardMode)(&enmMode);
        switch (enmMode)
        {
            case ClipboardMode_Disabled:
                psz = "disabled";
                break;
            case ClipboardMode_HostToGuest:
                psz = details == VMINFO_MACHINEREADABLE ? "hosttoguest" : "HostToGuest";
                break;
            case ClipboardMode_GuestToHost:
                psz = details == VMINFO_MACHINEREADABLE ? "guesttohost" : "GuestToHost";
                break;
            case ClipboardMode_Bidirectional:
                psz = details == VMINFO_MACHINEREADABLE ? "bidirectional" : "Bidirectional";
                break;
            default:
                psz = details == VMINFO_MACHINEREADABLE ? "unknown" : "Unknown";
                break;
        }
        SHOW_UTF8_STRING("clipboard", "Clipboard Mode:", psz);
    }

    /* Drag and drop */
    {
        const char *psz;
        DnDMode_T enmMode;
        rc = machine->COMGETTER(DnDMode)(&enmMode);
        switch (enmMode)
        {
            case DnDMode_Disabled:
                psz = "disabled";
                break;
            case DnDMode_HostToGuest:
                psz = details == VMINFO_MACHINEREADABLE ? "hosttoguest" : "HostToGuest";
                break;
            case DnDMode_GuestToHost:
                psz = details == VMINFO_MACHINEREADABLE ? "guesttohost" : "GuestToHost";
                break;
            case DnDMode_Bidirectional:
                psz = details == VMINFO_MACHINEREADABLE ? "bidirectional" : "Bidirectional";
                break;
            default:
                psz = details == VMINFO_MACHINEREADABLE ? "unknown" : "Unknown";
                break;
        }
        SHOW_UTF8_STRING("draganddrop", "Drag and drop Mode:", psz);
    }

    {
        SessionState_T sessState;
        rc = machine->COMGETTER(SessionState)(&sessState);
        if (SUCCEEDED(rc) && sessState != SessionState_Unlocked)
        {
            Bstr sessName;
            rc = machine->COMGETTER(SessionName)(sessName.asOutParam());
            if (SUCCEEDED(rc) && !sessName.isEmpty())
                SHOW_BSTR_STRING("SessionName", "Session name:", sessName);
        }
    }

    if (pConsole)
    {
        do
        {
            ComPtr<IDisplay> display;
            rc = pConsole->COMGETTER(Display)(display.asOutParam());
            if (rc == E_ACCESSDENIED || display.isNull())
                break; /* VM not powered up */
            if (FAILED(rc))
            {
                com::GlueHandleComError(pConsole, "COMGETTER(Display)(display.asOutParam())", rc, __FILE__, __LINE__);
                return rc;
            }
            ULONG xRes, yRes, bpp;
            LONG xOrigin, yOrigin;
            GuestMonitorStatus_T monitorStatus;
            rc = display->GetScreenResolution(0, &xRes, &yRes, &bpp, &xOrigin, &yOrigin, &monitorStatus);
            if (rc == E_ACCESSDENIED)
                break; /* VM not powered up */
            if (FAILED(rc))
            {
                com::ErrorInfo info(display, COM_IIDOF(IDisplay));
                GluePrintErrorInfo(info);
                return rc;
            }
            if (details == VMINFO_MACHINEREADABLE)
                RTPrintf("VideoMode=\"%d,%d,%d\"@%d,%d %d\n", xRes, yRes, bpp, xOrigin, yOrigin, monitorStatus);
            else
            {
                const char *pszMonitorStatus = "unknown status";
                switch (monitorStatus)
                {
                    case GuestMonitorStatus_Blank:    pszMonitorStatus = "blank"; break;
                    case GuestMonitorStatus_Enabled:  pszMonitorStatus = "enabled"; break;
                    case GuestMonitorStatus_Disabled: pszMonitorStatus = "disabled"; break;
                    default: break;
                }
                RTPrintf("%-28s %dx%dx%d at %d,%d %s\n", "Video mode:", xRes, yRes, bpp, xOrigin, yOrigin, pszMonitorStatus);
            }
        }
        while (0);
    }

    /*
     * Remote Desktop
     */
    ComPtr<IVRDEServer> vrdeServer;
    rc = machine->COMGETTER(VRDEServer)(vrdeServer.asOutParam());
    if (SUCCEEDED(rc) && vrdeServer)
    {
        BOOL fEnabled = false;
        vrdeServer->COMGETTER(Enabled)(&fEnabled);
        if (fEnabled)
        {
            LONG currentPort = -1;
            Bstr ports;
            vrdeServer->GetVRDEProperty(Bstr("TCP/Ports").raw(), ports.asOutParam());
            Bstr address;
            vrdeServer->GetVRDEProperty(Bstr("TCP/Address").raw(), address.asOutParam());
            BOOL fMultiCon;
            vrdeServer->COMGETTER(AllowMultiConnection)(&fMultiCon);
            BOOL fReuseCon;
            vrdeServer->COMGETTER(ReuseSingleConnection)(&fReuseCon);
            Bstr videoChannel;
            vrdeServer->GetVRDEProperty(Bstr("VideoChannel/Enabled").raw(), videoChannel.asOutParam());
            BOOL fVideoChannel =    (videoChannel.compare(Bstr("true"), Bstr::CaseInsensitive)== 0)
                                 || (videoChannel == "1");
            Bstr videoChannelQuality;
            vrdeServer->GetVRDEProperty(Bstr("VideoChannel/Quality").raw(), videoChannelQuality.asOutParam());
            AuthType_T authType = (AuthType_T)0;
            const char *strAuthType;
            vrdeServer->COMGETTER(AuthType)(&authType);
            switch (authType)
            {
                case AuthType_Null:
                    strAuthType = "null";
                    break;
                case AuthType_External:
                    strAuthType = "external";
                    break;
                case AuthType_Guest:
                    strAuthType = "guest";
                    break;
                default:
                    strAuthType = "unknown";
                    break;
            }
            if (pConsole)
            {
                ComPtr<IVRDEServerInfo> vrdeServerInfo;
                CHECK_ERROR_RET(pConsole, COMGETTER(VRDEServerInfo)(vrdeServerInfo.asOutParam()), rc);
                if (!vrdeServerInfo.isNull())
                {
                    rc = vrdeServerInfo->COMGETTER(Port)(&currentPort);
                    if (rc == E_ACCESSDENIED)
                    {
                        currentPort = -1; /* VM not powered up */
                    }
                    else if (FAILED(rc))
                    {
                        com::ErrorInfo info(vrdeServerInfo, COM_IIDOF(IVRDEServerInfo));
                        GluePrintErrorInfo(info);
                        return rc;
                    }
                }
            }
            if (details == VMINFO_MACHINEREADABLE)
            {
                RTPrintf("vrde=\"on\"\n");
                RTPrintf("vrdeport=%d\n", currentPort);
                RTPrintf("vrdeports=\"%ls\"\n", ports.raw());
                RTPrintf("vrdeaddress=\"%ls\"\n", address.raw());
                RTPrintf("vrdeauthtype=\"%s\"\n", strAuthType);
                RTPrintf("vrdemulticon=\"%s\"\n", fMultiCon ? "on" : "off");
                RTPrintf("vrdereusecon=\"%s\"\n", fReuseCon ? "on" : "off");
                RTPrintf("vrdevideochannel=\"%s\"\n", fVideoChannel ? "on" : "off");
                if (fVideoChannel)
                    RTPrintf("vrdevideochannelquality=\"%ls\"\n", videoChannelQuality.raw());
            }
            else
            {
                if (address.isEmpty())
                    address = "0.0.0.0";
                RTPrintf("%-28s enabled (Address %ls, Ports %ls, MultiConn: %s, ReuseSingleConn: %s, Authentication type: %s)\n",
                         "VRDE:", address.raw(), ports.raw(), fMultiCon ? "on" : "off", fReuseCon ? "on" : "off", strAuthType);
                if (pConsole && currentPort != -1 && currentPort != 0)
                   RTPrintf("%-28s %d\n", "VRDE port:", currentPort);
                if (fVideoChannel)
                    RTPrintf("%-28s enabled (Quality %ls)\n", "Video redirection:", videoChannelQuality.raw());
                else
                    RTPrintf("%-28s disabled\n", "Video redirection:");
            }
            com::SafeArray<BSTR> aProperties;
            if (SUCCEEDED(vrdeServer->COMGETTER(VRDEProperties)(ComSafeArrayAsOutParam(aProperties))))
            {
                unsigned i;
                for (i = 0; i < aProperties.size(); ++i)
                {
                    Bstr value;
                    vrdeServer->GetVRDEProperty(aProperties[i], value.asOutParam());
                    if (details == VMINFO_MACHINEREADABLE)
                    {
                        if (value.isEmpty())
                            RTPrintf("vrdeproperty[%ls]=<not set>\n", aProperties[i]);
                        else
                            RTPrintf("vrdeproperty[%ls]=\"%ls\"\n", aProperties[i], value.raw());
                    }
                    else
                    {
                        if (value.isEmpty())
                            RTPrintf("%-28s: %-10lS = <not set>\n", "VRDE property", aProperties[i]);
                        else
                            RTPrintf("%-28s: %-10lS = \"%ls\"\n", "VRDE property", aProperties[i], value.raw());
                    }
                }
            }
        }
        else
        {
            if (details == VMINFO_MACHINEREADABLE)
                RTPrintf("vrde=\"off\"\n");
            else
                RTPrintf("%-28s disabled\n", "VRDE:");
        }
    }

    /*
     * USB.
     */
    SafeIfaceArray<IUSBController> USBCtlColl;
    rc = machine->COMGETTER(USBControllers)(ComSafeArrayAsOutParam(USBCtlColl));
    if (SUCCEEDED(rc))
    {
        bool fOhciEnabled = false;
        bool fEhciEnabled = false;
        bool fXhciEnabled = false;

        for (unsigned i = 0; i < USBCtlColl.size(); i++)
        {
            USBControllerType_T enmType;

            rc = USBCtlColl[i]->COMGETTER(Type)(&enmType);
            if (SUCCEEDED(rc))
            {
                switch (enmType)
                {
                    case USBControllerType_OHCI:
                        fOhciEnabled = true;
                        break;
                    case USBControllerType_EHCI:
                        fEhciEnabled = true;
                        break;
                    case USBControllerType_XHCI:
                        fXhciEnabled = true;
                        break;
                    default:
                        break;
                }
            }
        }

        SHOW_BOOL_VALUE("usb",  "OHCI USB:", fOhciEnabled);
        SHOW_BOOL_VALUE("ehci", "EHCI USB:", fEhciEnabled);
        SHOW_BOOL_VALUE("xhci", "xHCI USB:", fXhciEnabled);
    }

    ComPtr<IUSBDeviceFilters> USBFlts;
    rc = machine->COMGETTER(USBDeviceFilters)(USBFlts.asOutParam());
    if (SUCCEEDED(rc))
    {
        SafeIfaceArray <IUSBDeviceFilter> Coll;
        rc = USBFlts->COMGETTER(DeviceFilters)(ComSafeArrayAsOutParam(Coll));
        if (SUCCEEDED(rc))
        {
            if (details != VMINFO_MACHINEREADABLE)
                RTPrintf("\nUSB Device Filters:\n\n");

            if (Coll.size() == 0)
            {
                if (details != VMINFO_MACHINEREADABLE)
                    RTPrintf("<none>\n\n");
            }
            else
            {
                for (size_t index = 0; index < Coll.size(); ++index)
                {
                    ComPtr<IUSBDeviceFilter> DevPtr = Coll[index];

                    if (details != VMINFO_MACHINEREADABLE)
                        SHOW_UTF8_STRING("index", "Index:", FmtNm(szNm, "%zu", index));
                    SHOW_BOOLEAN_PROP_EX(DevPtr, Active,   FmtNm(szNm, "USBFilterActive%zu", index + 1),       "Active:", "yes", "no");
                    SHOW_STRING_PROP(DevPtr, Name,         FmtNm(szNm, "USBFilterName%zu", index + 1),         "Name:");
                    SHOW_STRING_PROP(DevPtr, VendorId,     FmtNm(szNm, "USBFilterVendorId%zu", index + 1),     "VendorId:");
                    SHOW_STRING_PROP(DevPtr, ProductId,    FmtNm(szNm, "USBFilterProductId%zu", index + 1),    "ProductId:");
                    SHOW_STRING_PROP(DevPtr, Revision,     FmtNm(szNm, "USBFilterRevision%zu", index + 1),     "Revision:");
                    SHOW_STRING_PROP(DevPtr, Manufacturer, FmtNm(szNm, "USBFilterManufacturer%zu", index + 1), "Manufacturer:");
                    SHOW_STRING_PROP(DevPtr, Product,      FmtNm(szNm, "USBFilterProduct%zu", index + 1),      "Product:");
                    SHOW_STRING_PROP(DevPtr, Remote,       FmtNm(szNm, "USBFilterRemote%zu", index + 1),       "Remote:");
                    SHOW_STRING_PROP(DevPtr, SerialNumber, FmtNm(szNm, "USBFilterSerialNumber%zu", index + 1),       "Serial Number:");
                    if (details != VMINFO_MACHINEREADABLE)
                    {
                        ULONG fMaskedIfs;
                        CHECK_ERROR_RET(DevPtr, COMGETTER(MaskedInterfaces)(&fMaskedIfs), rc);
                        if (fMaskedIfs)
                            RTPrintf("%-28s %#010x\n", "Masked Interfaces:", fMaskedIfs);
                        RTPrintf("\n");
                    }
                }
            }
        }

        if (pConsole)
        {
            /* scope */
            {
                if (details != VMINFO_MACHINEREADABLE)
                    RTPrintf("Available remote USB devices:\n\n");

                SafeIfaceArray <IHostUSBDevice> coll;
                CHECK_ERROR_RET(pConsole, COMGETTER(RemoteUSBDevices)(ComSafeArrayAsOutParam(coll)), rc);

                if (coll.size() == 0)
                {
                    if (details != VMINFO_MACHINEREADABLE)
                        RTPrintf("<none>\n\n");
                }
                else
                {
                    /* This code is duplicated below, with USBAttach as prefix. */
                    const char *pszPfx = "USBRemote";
                    for (size_t i = 0; i < coll.size(); ++i)
                    {
                        ComPtr<IHostUSBDevice> dev = coll[i];

                        SHOW_STRING_PROP(dev, Id, FmtNm(szNm, "%sActive%zu", pszPfx, i + 1), "UUID:");
                        SHOW_USHORT_PROP_EX2(dev, VendorId,  FmtNm(szNm, "%sVendorId%zu", pszPfx, i + 1),  "VendorId:",  "", "%#06x", "%#06x (%04X)");
                        SHOW_USHORT_PROP_EX2(dev, ProductId, FmtNm(szNm, "%sProductId%zu", pszPfx, i + 1), "ProductId:", "", "%#06x", "%#06x (%04X)");

                        USHORT bcdRevision;
                        CHECK_ERROR_RET(dev, COMGETTER(Revision)(&bcdRevision), rc);
                        if (details == VMINFO_MACHINEREADABLE)
                            RTStrPrintf(szValue, sizeof(szValue), "%#04x%02x", bcdRevision >> 8, bcdRevision & 0xff);
                        else
                            RTStrPrintf(szValue, sizeof(szValue), "%u.%u (%02u%02u)\n",
                                        bcdRevision >> 8, bcdRevision & 0xff, bcdRevision >> 8, bcdRevision & 0xff);
                        SHOW_UTF8_STRING(FmtNm(szNm, "%sRevision%zu", pszPfx, i + 1), "Revision:", szValue);

                        SHOW_STRING_PROP_NOT_EMPTY(dev, Manufacturer, FmtNm(szNm, "%sManufacturer%zu", pszPfx, i + 1), "Manufacturer:");
                        SHOW_STRING_PROP_NOT_EMPTY(dev, Product,      FmtNm(szNm, "%sProduct%zu", pszPfx, i + 1),      "Product:");
                        SHOW_STRING_PROP_NOT_EMPTY(dev, SerialNumber, FmtNm(szNm, "%sSerialNumber%zu", pszPfx, i + 1), "SerialNumber:");
                        SHOW_STRING_PROP_NOT_EMPTY(dev, Address,      FmtNm(szNm, "%sAddress%zu", pszPfx, i + 1),      "Address:");

                        if (details != VMINFO_MACHINEREADABLE)
                            RTPrintf("\n");
                    }
                }
            }

            /* scope */
            {
                if (details != VMINFO_MACHINEREADABLE)
                    RTPrintf("Currently Attached USB Devices:\n\n");

                SafeIfaceArray <IUSBDevice> coll;
                CHECK_ERROR_RET(pConsole, COMGETTER(USBDevices)(ComSafeArrayAsOutParam(coll)), rc);

                if (coll.size() == 0)
                {
                    if (details != VMINFO_MACHINEREADABLE)
                        RTPrintf("<none>\n\n");
                }
                else
                {
                    /* This code is duplicated below, with USBAttach as prefix. */
                    const char *pszPfx = "USBAttach";
                    for (size_t i = 0; i < coll.size(); ++i)
                    {
                        ComPtr<IHostUSBDevice> dev = coll[i];

                        SHOW_STRING_PROP(dev, Id, FmtNm(szNm, "%sActive%zu", pszPfx, i + 1), "UUID:");
                        SHOW_USHORT_PROP_EX2(dev, VendorId,  FmtNm(szNm, "%sVendorId%zu", pszPfx, i + 1),  "VendorId:",  "", "%#06x", "%#06x (%04X)");
                        SHOW_USHORT_PROP_EX2(dev, ProductId, FmtNm(szNm, "%sProductId%zu", pszPfx, i + 1), "ProductId:", "", "%#06x", "%#06x (%04X)");

                        USHORT bcdRevision;
                        CHECK_ERROR_RET(dev, COMGETTER(Revision)(&bcdRevision), rc);
                        if (details == VMINFO_MACHINEREADABLE)
                            RTStrPrintf(szValue, sizeof(szValue), "%#04x%02x", bcdRevision >> 8, bcdRevision & 0xff);
                        else
                            RTStrPrintf(szValue, sizeof(szValue), "%u.%u (%02u%02u)\n",
                                        bcdRevision >> 8, bcdRevision & 0xff, bcdRevision >> 8, bcdRevision & 0xff);
                        SHOW_UTF8_STRING(FmtNm(szNm, "%sRevision%zu", pszPfx, i + 1), "Revision:", szValue);

                        SHOW_STRING_PROP_NOT_EMPTY(dev, Manufacturer, FmtNm(szNm, "%sManufacturer%zu", pszPfx, i + 1), "Manufacturer:");
                        SHOW_STRING_PROP_NOT_EMPTY(dev, Product,      FmtNm(szNm, "%sProduct%zu", pszPfx, i + 1),      "Product:");
                        SHOW_STRING_PROP_NOT_EMPTY(dev, SerialNumber, FmtNm(szNm, "%sSerialNumber%zu", pszPfx, i + 1), "SerialNumber:");
                        SHOW_STRING_PROP_NOT_EMPTY(dev, Address,      FmtNm(szNm, "%sAddress%zu", pszPfx, i + 1),      "Address:");

                        if (details != VMINFO_MACHINEREADABLE)
                            RTPrintf("\n");
                    }
                }
            }
        }
    } /* USB */

#ifdef VBOX_WITH_PCI_PASSTHROUGH
    /* Host PCI passthrough devices */
    {
         SafeIfaceArray <IPCIDeviceAttachment> assignments;
         rc = machine->COMGETTER(PCIDeviceAssignments)(ComSafeArrayAsOutParam(assignments));
         if (SUCCEEDED(rc))
         {
             if (assignments.size() > 0 && (details != VMINFO_MACHINEREADABLE))
             {
                 RTPrintf("\nAttached physical PCI devices:\n\n");
             }

             for (size_t index = 0; index < assignments.size(); ++index)
             {
                 ComPtr<IPCIDeviceAttachment> Assignment = assignments[index];
                 char szHostPCIAddress[32], szGuestPCIAddress[32];
                 LONG iHostPCIAddress = -1, iGuestPCIAddress = -1;
                 Bstr DevName;

                 Assignment->COMGETTER(Name)(DevName.asOutParam());
                 Assignment->COMGETTER(HostAddress)(&iHostPCIAddress);
                 Assignment->COMGETTER(GuestAddress)(&iGuestPCIAddress);
                 PCIBusAddress().fromLong(iHostPCIAddress).format(szHostPCIAddress, sizeof(szHostPCIAddress));
                 PCIBusAddress().fromLong(iGuestPCIAddress).format(szGuestPCIAddress, sizeof(szGuestPCIAddress));

                 if (details == VMINFO_MACHINEREADABLE)
                     RTPrintf("AttachedHostPCI=%s,%s\n", szHostPCIAddress, szGuestPCIAddress);
                 else
                     RTPrintf("   Host device %ls at %s attached as %s\n", DevName.raw(), szHostPCIAddress, szGuestPCIAddress);
             }

             if (assignments.size() > 0 && (details != VMINFO_MACHINEREADABLE))
             {
                 RTPrintf("\n");
             }
         }
    }
    /* Host PCI passthrough devices */
#endif

    /*
     * Bandwidth groups
     */
    if (details != VMINFO_MACHINEREADABLE)
        RTPrintf("Bandwidth groups:  ");
    {
        ComPtr<IBandwidthControl> bwCtrl;
        CHECK_ERROR_RET(machine, COMGETTER(BandwidthControl)(bwCtrl.asOutParam()), rc);

        rc = showBandwidthGroups(bwCtrl, details);
    }


    /*
     * Shared folders
     */
    if (details != VMINFO_MACHINEREADABLE)
        RTPrintf("Shared folders:");
    uint32_t numSharedFolders = 0;
#if 0 // not yet implemented
    /* globally shared folders first */
    {
        SafeIfaceArray <ISharedFolder> sfColl;
        CHECK_ERROR_RET(pVirtualBox, COMGETTER(SharedFolders)(ComSafeArrayAsOutParam(sfColl)), rc);
        for (size_t i = 0; i < sfColl.size(); ++i)
        {
            ComPtr<ISharedFolder> sf = sfColl[i];
            Bstr name, hostPath;
            sf->COMGETTER(Name)(name.asOutParam());
            sf->COMGETTER(HostPath)(hostPath.asOutParam());
            RTPrintf("Name: '%ls', Host path: '%ls' (global mapping)\n", name.raw(), hostPath.raw());
            ++numSharedFolders;
        }
    }
#endif
    /* now VM mappings */
    {
        com::SafeIfaceArray <ISharedFolder> folders;

        CHECK_ERROR_RET(machine, COMGETTER(SharedFolders)(ComSafeArrayAsOutParam(folders)), rc);

        for (size_t i = 0; i < folders.size(); ++i)
        {
            ComPtr<ISharedFolder> sf = folders[i];

            Bstr name, hostPath;
            BOOL writable;
            sf->COMGETTER(Name)(name.asOutParam());
            sf->COMGETTER(HostPath)(hostPath.asOutParam());
            sf->COMGETTER(Writable)(&writable);
            if (!numSharedFolders && details != VMINFO_MACHINEREADABLE)
                RTPrintf("\n\n");
            if (details == VMINFO_MACHINEREADABLE)
            {
                outputMachineReadableString(FmtNm(szNm, "SharedFolderNameMachineMapping%zu", i + 1), &name);
                outputMachineReadableString(FmtNm(szNm, "SharedFolderPathMachineMapping%zu", i + 1), &hostPath);
            }
            else
                RTPrintf("Name: '%ls', Host path: '%ls' (machine mapping), %s\n",
                         name.raw(), hostPath.raw(), writable ? "writable" : "readonly");
            ++numSharedFolders;
        }
    }
    /* transient mappings */
    if (pConsole)
    {
        com::SafeIfaceArray <ISharedFolder> folders;

        CHECK_ERROR_RET(pConsole, COMGETTER(SharedFolders)(ComSafeArrayAsOutParam(folders)), rc);

        for (size_t i = 0; i < folders.size(); ++i)
        {
            ComPtr<ISharedFolder> sf = folders[i];

            Bstr name, hostPath;
            sf->COMGETTER(Name)(name.asOutParam());
            sf->COMGETTER(HostPath)(hostPath.asOutParam());
            if (!numSharedFolders && details != VMINFO_MACHINEREADABLE)
                RTPrintf("\n\n");
            if (details == VMINFO_MACHINEREADABLE)
            {
                outputMachineReadableString(FmtNm(szNm, "SharedFolderNameTransientMapping%zu", i + 1), &name);
                outputMachineReadableString(FmtNm(szNm, "SharedFolderPathTransientMapping%zu", i + 1), &hostPath);
            }
            else
                RTPrintf("Name: '%ls', Host path: '%ls' (transient mapping)\n", name.raw(), hostPath.raw());
            ++numSharedFolders;
        }
    }
    if (!numSharedFolders && details != VMINFO_MACHINEREADABLE)
        RTPrintf("<none>\n");
    if (details != VMINFO_MACHINEREADABLE)
        RTPrintf("\n");

    if (pConsole)
    {
        /*
         * Live VRDE info.
         */
        ComPtr<IVRDEServerInfo> vrdeServerInfo;
        CHECK_ERROR_RET(pConsole, COMGETTER(VRDEServerInfo)(vrdeServerInfo.asOutParam()), rc);
        BOOL    fActive = FALSE;
        ULONG   cNumberOfClients = 0;
        LONG64  BeginTime = 0;
        LONG64  EndTime = 0;
        LONG64  BytesSent = 0;
        LONG64  BytesSentTotal = 0;
        LONG64  BytesReceived = 0;
        LONG64  BytesReceivedTotal = 0;
        Bstr    User;
        Bstr    Domain;
        Bstr    ClientName;
        Bstr    ClientIP;
        ULONG   ClientVersion = 0;
        ULONG   EncryptionStyle = 0;

        if (!vrdeServerInfo.isNull())
        {
            CHECK_ERROR_RET(vrdeServerInfo, COMGETTER(Active)(&fActive), rc);
            CHECK_ERROR_RET(vrdeServerInfo, COMGETTER(NumberOfClients)(&cNumberOfClients), rc);
            CHECK_ERROR_RET(vrdeServerInfo, COMGETTER(BeginTime)(&BeginTime), rc);
            CHECK_ERROR_RET(vrdeServerInfo, COMGETTER(EndTime)(&EndTime), rc);
            CHECK_ERROR_RET(vrdeServerInfo, COMGETTER(BytesSent)(&BytesSent), rc);
            CHECK_ERROR_RET(vrdeServerInfo, COMGETTER(BytesSentTotal)(&BytesSentTotal), rc);
            CHECK_ERROR_RET(vrdeServerInfo, COMGETTER(BytesReceived)(&BytesReceived), rc);
            CHECK_ERROR_RET(vrdeServerInfo, COMGETTER(BytesReceivedTotal)(&BytesReceivedTotal), rc);
            CHECK_ERROR_RET(vrdeServerInfo, COMGETTER(User)(User.asOutParam()), rc);
            CHECK_ERROR_RET(vrdeServerInfo, COMGETTER(Domain)(Domain.asOutParam()), rc);
            CHECK_ERROR_RET(vrdeServerInfo, COMGETTER(ClientName)(ClientName.asOutParam()), rc);
            CHECK_ERROR_RET(vrdeServerInfo, COMGETTER(ClientIP)(ClientIP.asOutParam()), rc);
            CHECK_ERROR_RET(vrdeServerInfo, COMGETTER(ClientVersion)(&ClientVersion), rc);
            CHECK_ERROR_RET(vrdeServerInfo, COMGETTER(EncryptionStyle)(&EncryptionStyle), rc);
        }

        SHOW_BOOL_VALUE_EX("VRDEActiveConnection", "VRDE Connection:", fActive, "active", "not active");
        SHOW_ULONG_VALUE("VRDEClients=", "Clients so far:", cNumberOfClients, "");

        if (cNumberOfClients > 0)
        {
            char szTimeValue[128];
            makeTimeStr(szTimeValue, sizeof(szTimeValue), BeginTime);
            if (fActive)
                SHOW_UTF8_STRING("VRDEStartTime", "Start time:", szTimeValue);
            else
            {
                SHOW_UTF8_STRING("VRDELastStartTime", "Last started:", szTimeValue);
                makeTimeStr(szTimeValue, sizeof(szTimeValue), EndTime);
                SHOW_UTF8_STRING("VRDELastEndTime", "Last ended:", szTimeValue);
            }

            int64_t ThroughputSend = 0;
            int64_t ThroughputReceive = 0;
            if (EndTime != BeginTime)
            {
                ThroughputSend = (BytesSent * 1000) / (EndTime - BeginTime);
                ThroughputReceive = (BytesReceived * 1000) / (EndTime - BeginTime);
            }
            SHOW_LONG64_VALUE("VRDEBytesSent", "Sent:", BytesSent, "Bytes");
            SHOW_LONG64_VALUE("VRDEThroughputSend", "Average speed:", ThroughputSend, "B/s");
            SHOW_LONG64_VALUE("VRDEBytesSentTotal", "Sent total:", BytesSentTotal, "Bytes");

            SHOW_LONG64_VALUE("VRDEBytesReceived", "Received:", BytesReceived, "Bytes");
            SHOW_LONG64_VALUE("VRDEThroughputReceive", "Speed:", ThroughputReceive, "B/s");
            SHOW_LONG64_VALUE("VRDEBytesReceivedTotal", "Received total:", BytesReceivedTotal, "Bytes");

            if (fActive)
            {
                SHOW_BSTR_STRING("VRDEUserName", "User name:", User);
                SHOW_BSTR_STRING("VRDEDomain", "Domain:", Domain);
                SHOW_BSTR_STRING("VRDEClientName", "Client name:", ClientName);
                SHOW_BSTR_STRING("VRDEClientIP", "Client IP:", ClientIP);
                SHOW_ULONG_VALUE("VRDEClientVersion", "Client version:", ClientVersion, "");
                SHOW_UTF8_STRING("VRDEEncryption", "Encryption:", EncryptionStyle == 0 ? "RDP4" : "RDP5 (X.509)");
            }
        }

        if (details != VMINFO_MACHINEREADABLE)
            RTPrintf("\n");
    }

#ifdef VBOX_WITH_VIDEOREC
    {
        /* Video capture */
        BOOL fCaptureVideo = FALSE;
# ifdef VBOX_WITH_AUDIO_VIDEOREC
        BOOL fCaptureAudio = FALSE;
# endif

        CHECK_ERROR_RET(machine, COMGETTER(VideoCaptureEnabled)(&fCaptureVideo), rc);
        com::SafeArray<BOOL> screens;
        CHECK_ERROR_RET(machine, COMGETTER(VideoCaptureScreens)(ComSafeArrayAsOutParam(screens)), rc);
        ULONG Width;
        CHECK_ERROR_RET(machine, COMGETTER(VideoCaptureWidth)(&Width), rc);
        ULONG Height;
        CHECK_ERROR_RET(machine, COMGETTER(VideoCaptureHeight)(&Height), rc);
        ULONG Rate;
        CHECK_ERROR_RET(machine, COMGETTER(VideoCaptureRate)(&Rate), rc);
        ULONG Fps;
        CHECK_ERROR_RET(machine, COMGETTER(VideoCaptureFPS)(&Fps), rc);
        Bstr  bstrFile;
        CHECK_ERROR_RET(machine, COMGETTER(VideoCaptureFile)(bstrFile.asOutParam()), rc);
        Bstr  bstrOptions;
        CHECK_ERROR_RET(machine, COMGETTER(VideoCaptureOptions)(bstrOptions.asOutParam()), rc);

        Utf8Str strOptions(bstrOptions);
        size_t pos = 0;
        com::Utf8Str key, value;
        while ((pos = strOptions.parseKeyValue(key, value, pos)) != com::Utf8Str::npos)
        {
            if (key.compare("vc_enabled", Utf8Str::CaseInsensitive) == 0)
            {
                fCaptureVideo = value.compare("true", Utf8Str::CaseInsensitive) == 0;
            }
            else if (key.compare("ac_enabled", Utf8Str::CaseInsensitive) == 0)
            {
# ifdef VBOX_WITH_AUDIO_VIDEOREC
                fCaptureAudio = value.compare("true", Utf8Str::CaseInsensitive) == 0;
# endif
            }
        }

        SHOW_BOOL_VALUE_EX("videocap", "Capturing:", fCaptureVideo, "active", "not active");
# ifdef VBOX_WITH_AUDIO_VIDEOREC
        SHOW_BOOL_VALUE_EX("videocapaudio", "Capture audio:", fCaptureAudio, "active", "not active");
# endif
        szValue[0] = '\0';
        for (size_t i = 0, off = 0; i < screens.size(); i++)
            if (screens[i] && off < sizeof(szValue) - 3)
                off += RTStrPrintf(&szValue[off], sizeof(szValue) - off, off ? ",%zu" : "%zu", i);
        SHOW_UTF8_STRING("videocapscreens", "Capture screens:", szValue);
        SHOW_BSTR_STRING("videocapfile", "Capture file:", bstrFile);
        RTStrPrintf(szValue, sizeof(szValue), "%ux%u", Width, Height);
        SHOW_UTF8_STRING("videocapres", "Capture dimensions:", szValue);
        SHOW_ULONG_VALUE("videocaprate", "Capture rate:", Rate, "kbps");
        SHOW_ULONG_VALUE("videocapfps", "Capture FPS:", Fps, "kbps");
        SHOW_BSTR_STRING("videocapopts", "Capture options:", bstrOptions);

        if (details != VMINFO_MACHINEREADABLE)
            RTPrintf("\n");
        /** @todo Add more audio capturing profile / information here. */
    }
#endif /* VBOX_WITH_VIDEOREC */

    if (    details == VMINFO_STANDARD
        ||  details == VMINFO_FULL
        ||  details == VMINFO_MACHINEREADABLE)
    {
        Bstr description;
        machine->COMGETTER(Description)(description.asOutParam());
        if (!description.isEmpty())
        {
            if (details == VMINFO_MACHINEREADABLE)
                outputMachineReadableString("description", &description);
            else
                RTPrintf("Description:\n%ls\n", description.raw());
        }
    }

    if (details != VMINFO_MACHINEREADABLE)
        RTPrintf("Guest:\n\n");

    SHOW_ULONG_PROP(machine, MemoryBalloonSize, "GuestMemoryBalloon", "Configured memory balloon size:", "MB");

    if (pConsole)
    {
        ComPtr<IGuest> guest;
        rc = pConsole->COMGETTER(Guest)(guest.asOutParam());
        if (SUCCEEDED(rc) && !guest.isNull())
        {
            SHOW_STRING_PROP_NOT_EMPTY(guest, OSTypeId, "GuestOSType", "OS type:");

            AdditionsRunLevelType_T guestRunLevel; /** @todo Add a runlevel-to-string (e.g. 0 = "None") method? */
            rc = guest->COMGETTER(AdditionsRunLevel)(&guestRunLevel);
            if (SUCCEEDED(rc))
                SHOW_ULONG_VALUE("GuestAdditionsRunLevel", "Additions run level:", (ULONG)guestRunLevel, "");

            Bstr guestString;
            rc = guest->COMGETTER(AdditionsVersion)(guestString.asOutParam());
            if (   SUCCEEDED(rc)
                && !guestString.isEmpty())
            {
                ULONG uRevision;
                rc = guest->COMGETTER(AdditionsRevision)(&uRevision);
                if (FAILED(rc))
                    uRevision = 0;
                RTStrPrintf(szValue, sizeof(szValue), "%ls r%u", guestString.raw(), uRevision);
                SHOW_UTF8_STRING("GuestAdditionsVersion", "Additions version", szValue);
            }

            if (details != VMINFO_MACHINEREADABLE)
                RTPrintf("\nGuest Facilities:\n\n");

            /* Print information about known Guest Additions facilities: */
            SafeIfaceArray <IAdditionsFacility> collFac;
            CHECK_ERROR_RET(guest, COMGETTER(Facilities)(ComSafeArrayAsOutParam(collFac)), rc);
            LONG64 lLastUpdatedMS;
            char szLastUpdated[32];
            AdditionsFacilityStatus_T curStatus;
            for (size_t index = 0; index < collFac.size(); ++index)
            {
                ComPtr<IAdditionsFacility> fac = collFac[index];
                if (fac)
                {
                    CHECK_ERROR_RET(fac, COMGETTER(Name)(guestString.asOutParam()), rc);
                    if (!guestString.isEmpty())
                    {
                        CHECK_ERROR_RET(fac, COMGETTER(Status)(&curStatus), rc);
                        CHECK_ERROR_RET(fac, COMGETTER(LastUpdated)(&lLastUpdatedMS), rc);
                        if (details == VMINFO_MACHINEREADABLE)
                            RTPrintf("GuestAdditionsFacility_%ls=%u,%lld\n",
                                     guestString.raw(), curStatus, lLastUpdatedMS);
                        else
                        {
                            makeTimeStr(szLastUpdated, sizeof(szLastUpdated), lLastUpdatedMS);
                            RTPrintf("Facility \"%ls\": %s (last update: %s)\n",
                                     guestString.raw(), facilityStateToName(curStatus, false /* No short naming */), szLastUpdated);
                        }
                    }
                    else
                        AssertMsgFailed(("Facility with undefined name retrieved!\n"));
                }
                else
                    AssertMsgFailed(("Invalid facility returned!\n"));
            }
            if (!collFac.size() && details != VMINFO_MACHINEREADABLE)
                RTPrintf("No active facilities.\n");
        }
    }

    if (details != VMINFO_MACHINEREADABLE)
        RTPrintf("\n");

    /*
     * snapshots
     */
    ComPtr<ISnapshot> snapshot;
    rc = machine->FindSnapshot(Bstr().raw(), snapshot.asOutParam());
    if (SUCCEEDED(rc) && snapshot)
    {
        ComPtr<ISnapshot> currentSnapshot;
        rc = machine->COMGETTER(CurrentSnapshot)(currentSnapshot.asOutParam());
        if (SUCCEEDED(rc))
        {
            if (details != VMINFO_MACHINEREADABLE)
                RTPrintf("Snapshots:\n\n");
            showSnapshots(snapshot, currentSnapshot, details);
        }
    }

    if (details != VMINFO_MACHINEREADABLE)
        RTPrintf("\n");
    return S_OK;
}

#if defined(_MSC_VER)
# pragma optimize("", on)
# pragma warning(pop)
#endif

static const RTGETOPTDEF g_aShowVMInfoOptions[] =
{
    { "--details",          'D', RTGETOPT_REQ_NOTHING },
    { "-details",           'D', RTGETOPT_REQ_NOTHING },    // deprecated
    { "--machinereadable",  'M', RTGETOPT_REQ_NOTHING },
    { "-machinereadable",   'M', RTGETOPT_REQ_NOTHING },    // deprecated
    { "--log",              'l', RTGETOPT_REQ_UINT32 },
};

RTEXITCODE handleShowVMInfo(HandlerArg *a)
{
    HRESULT rc;
    const char *VMNameOrUuid = NULL;
    bool fLog = false;
    uint32_t uLogIdx = 0;
    bool fDetails = false;
    bool fMachinereadable = false;

    int c;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    // start at 0 because main() has hacked both the argc and argv given to us
    RTGetOptInit(&GetState, a->argc, a->argv, g_aShowVMInfoOptions, RT_ELEMENTS(g_aShowVMInfoOptions),
                 0, RTGETOPTINIT_FLAGS_NO_STD_OPTS);
    while ((c = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (c)
        {
            case 'D':   // --details
                fDetails = true;
                break;

            case 'M':   // --machinereadable
                fMachinereadable = true;
                break;

            case 'l':   // --log
                fLog = true;
                uLogIdx = ValueUnion.u32;
                break;

            case VINF_GETOPT_NOT_OPTION:
                if (!VMNameOrUuid)
                    VMNameOrUuid = ValueUnion.psz;
                else
                    return errorSyntax(USAGE_SHOWVMINFO, "Invalid parameter '%s'", ValueUnion.psz);
                break;

            default:
                return errorGetOpt(USAGE_SHOWVMINFO, c, &ValueUnion);
        }
    }

    /* check for required options */
    if (!VMNameOrUuid)
        return errorSyntax(USAGE_SHOWVMINFO, "VM name or UUID required");

    /* try to find the given machine */
    ComPtr<IMachine> machine;
    CHECK_ERROR(a->virtualBox, FindMachine(Bstr(VMNameOrUuid).raw(),
                                           machine.asOutParam()));
    if (FAILED(rc))
        return RTEXITCODE_FAILURE;

    /* Printing the log is exclusive. */
    if (fLog && (fMachinereadable || fDetails))
        return errorSyntax(USAGE_SHOWVMINFO, "Option --log is exclusive");

    if (fLog)
    {
        ULONG64 uOffset = 0;
        SafeArray<BYTE> aLogData;
        size_t cbLogData;
        while (true)
        {
            /* Reset the array */
            aLogData.setNull();
            /* Fetch a chunk of the log file */
            CHECK_ERROR_BREAK(machine, ReadLog(uLogIdx, uOffset, _1M,
                                               ComSafeArrayAsOutParam(aLogData)));
            cbLogData = aLogData.size();
            if (cbLogData == 0)
                break;
            /* aLogData has a platform dependent line ending, standardize on
             * Unix style, as RTStrmWrite does the LF -> CR/LF replacement on
             * Windows. Otherwise we end up with CR/CR/LF on Windows. */
            size_t cbLogDataPrint = cbLogData;
            for (BYTE *s = aLogData.raw(), *d = s;
                 s - aLogData.raw() < (ssize_t)cbLogData;
                 s++, d++)
            {
                if (*s == '\r')
                {
                    /* skip over CR, adjust destination */
                    d--;
                    cbLogDataPrint--;
                }
                else if (s != d)
                    *d = *s;
            }
            RTStrmWrite(g_pStdOut, aLogData.raw(), cbLogDataPrint);
            uOffset += cbLogData;
        }
    }
    else
    {
        /* 2nd option can be -details or -argdump */
        VMINFO_DETAILS details = VMINFO_NONE;
        if (fMachinereadable)
            details = VMINFO_MACHINEREADABLE;
        else if (fDetails)
            details = VMINFO_FULL;
        else
            details = VMINFO_STANDARD;

        /* open an existing session for the VM */
        rc = machine->LockMachine(a->session, LockType_Shared);
        if (SUCCEEDED(rc))
            /* get the session machine */
            rc = a->session->COMGETTER(Machine)(machine.asOutParam());

        rc = showVMInfo(a->virtualBox, machine, a->session, details);

        a->session->UnlockMachine();
    }

    return SUCCEEDED(rc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}

#endif /* !VBOX_ONLY_DOCS */
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
