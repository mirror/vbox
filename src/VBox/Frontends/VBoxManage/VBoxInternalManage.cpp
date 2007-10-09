/** @file
 *
 * VBox frontends: VBoxManage (command-line interface):
 * VBoxInternalManage
 *
 * VBoxInternalManage used to be a second CLI for doing special tricks,
 * not intended for general usage, only for assisting VBox developers.
 * It is now integrated into VBoxManage.
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



/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/com/com.h>
#include <VBox/com/string.h>
#include <VBox/com/Guid.h>
#include <VBox/com/ErrorInfo.h>

#include <VBox/com/VirtualBox.h>

#include <iprt/runtime.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/uuid.h>
#include <VBox/err.h>

#include <VBox/VBoxHDD.h>

#include "VBoxManage.h"

#ifndef VBOX_OSE
HRESULT CmdListPartitions(int argc, char **argv, ComPtr<IVirtualBox> aVirtualBox, ComPtr<ISession> aSession);
HRESULT CmdCreateRawVMDK(int argc, char **argv, ComPtr<IVirtualBox> aVirtualBox, ComPtr<ISession> aSession);
#endif /* !VBOX_OSE */

using namespace com;

/** flag whether we're in internal mode */
bool g_fInternalMode;

/**
 * Print the usage info.
 */
void printUsageInternal(USAGECATEGORY u64Cmd)
{
    RTPrintf("Usage: VBoxManage internalcommands <command> [command arguments]\n"
             "\n"
             "Commands:\n"
             "\n"
             "%s%s%s%s%s"
             "WARNING: This is a development tool and shall only be used to analyse\n"
             "         problems. It is completely unsupported and will change in\n"
             "         incompatible ways without warning.\n",
            (u64Cmd & USAGE_LOADSYMS) ?
                "  loadsyms <vmname>|<uuid> <symfile> [delta] [module] [module address]\n"
                "      This will instruct DBGF to load the given symbolfile\n"
                "      during initialization.\n"
                "\n"
                : "",
            (u64Cmd & USAGE_UNLOADSYMS) ?
                "  unloadsyms <vmname>|<uuid> <symfile>\n"
                "      Removes <symfile> from the list of symbol files that\n"
                "      should be loaded during DBF initialization.\n"
                "\n"
                : "",
            (u64Cmd & USAGE_SETVDIUUID) ?
                "  setvdiuuid <filepath>\n"
                "       Assigns a new UUID to the given VDI file. This way, multiple copies\n"
                "       of VDI containers can be registered.\n"
                "\n"
                : "",
            (u64Cmd & USAGE_LISTPARTITIONS) ?
                "  listpartitions -rawdisk <diskname>\n"
                "       Lists all partitions on <diskname>.\n"
                "\n"
                : "",
            (u64Cmd & USAGE_CREATERAWVMDK) ?
                "  createrawvmdk -filename <filename> -rawdisk <diskname>\n"
                "                [-partitions <list of partition numbers> [-mbr <filename>] ]\n"
                "                [-register] [-relative]\n"
                "       Creates a new VMDK image which gives access to an entite host disk (if\n"
                "       the parameter -partitions is not specified) or some partitions of a\n"
                "       host disk. If access to individual partitions is granted, then the\n"
                "       parameter -mbr can be used to specify an alternative MBR to be used\n"
                "       (the partitioning information in the MBR file is ignored).\n"
                "       The diskname is on Linux e.g. /dev/sda, and on Windows e.g.\n"
                "       \\\\.\\PhysicalDisk0).\n"
                "       On Linux host the parameter -relative causes a VMDK file to be created\n"
                "       which refers to individual partitions instead to the entire disk.\n"
                "       Optionally the created image can be immediately registered.\n"
                "       The necessary partition numbers can be queried with\n"
                "         VBoxManage internalcommands listpartitions\n"
                "\n"
                : ""
             );
}

/** @todo this is no longer necessary, we can enumerate extra data */
/**
 * Finds a new unique key name.
 *
 * I don't think this is 100% race condition proof, but we assumes
 * the user is not trying to push this point.
 *
 * @returns Result from the insert.
 * @param   pMachine        The Machine object.
 * @param   pszKeyBase      The base key.
 * @param   rKey            Reference to the string object in which we will return the key.
 */
static HRESULT NewUniqueKey(ComPtr<IMachine> pMachine, const char *pszKeyBase, Utf8Str &rKey)
{
    Bstr Keys;
    HRESULT hrc = pMachine->GetExtraData(Bstr(pszKeyBase), Keys.asOutParam());
    if (FAILED(hrc))
        return hrc;

    /* if there are no keys, it's simple. */
    if (Keys.isEmpty())
    {
        rKey = "1";
        return pMachine->SetExtraData(Bstr(pszKeyBase), Bstr("1"));
    }

    /* find a unique number - brute force rulez. */
    Utf8Str KeysUtf8(Keys);
    const char *pszKeys = RTStrStripL(KeysUtf8.raw());
    for (unsigned i = 1; i < 1000000; i++)
    {
        char szKey[32];
        size_t cchKey = RTStrPrintf(szKey, sizeof(szKey), "%#x", i);
        const char *psz = strstr(pszKeys, szKey);
        while (psz)
        {
            if (    (   psz == pszKeys
                     || psz[-1] == ' ')
                &&  (   psz[cchKey] == ' '
                     || !psz[cchKey])
               )
                break;
            psz = strstr(psz + cchKey, szKey);
        }
        if (!psz)
        {
            rKey = szKey;
            Utf8StrFmt NewKeysUtf8("%s %s", pszKeys, szKey);
            return pMachine->SetExtraData(Bstr(pszKeyBase), Bstr(NewKeysUtf8));
        }
    }
    RTPrintf("Error: Cannot find unique key for '%s'!\n", pszKeyBase);
    return E_FAIL;
}


#if 0
/**
 * Remove a key.
 *
 * I don't think this isn't 100% race condition proof, but we assumes
 * the user is not trying to push this point.
 *
 * @returns Result from the insert.
 * @param   pMachine    The machine object.
 * @param   pszKeyBase  The base key.
 * @param   pszKey      The key to remove.
 */
static HRESULT RemoveKey(ComPtr<IMachine> pMachine, const char *pszKeyBase, const char *pszKey)
{
    Bstr Keys;
    HRESULT hrc = pMachine->GetExtraData(Bstr(pszKeyBase), Keys.asOutParam());
    if (FAILED(hrc))
        return hrc;

    /* if there are no keys, it's simple. */
    if (Keys.isEmpty())
        return S_OK;

    char *pszKeys;
    int rc = RTStrUcs2ToUtf8(&pszKeys, Keys.raw());
    if (VBOX_SUCCESS(rc))
    {
        /* locate it */
        size_t cchKey = strlen(pszKey);
        char *psz = strstr(pszKeys, pszKey);
        while (psz)
        {
            if (    (   psz == pszKeys
                     || psz[-1] == ' ')
                &&  (   psz[cchKey] == ' '
                     || !psz[cchKey])
               )
                break;
            psz = strstr(psz + cchKey, pszKey);
        }
        if (psz)
        {
            /* remove it */
            char *pszNext = RTStrStripL(psz + cchKey);
            if (*pszNext)
                memmove(psz, pszNext, strlen(pszNext) + 1);
            else
                *psz = '\0';
            psz = RTStrStrip(pszKeys);

            /* update */
            hrc = pMachine->SetExtraData(Bstr(pszKeyBase), Bstr(psz));
        }

        RTStrFree(pszKeys);
        return hrc;
    }
    else
        RTPrintf("error: failed to delete key '%s' from '%s',  string conversion error %Vrc!\n",
                 pszKey,  pszKeyBase, rc);

    return E_FAIL;
}
#endif


/**
 * Sets a key value, does necessary error bitching.
 *
 * @returns COM status code.
 * @param   pMachine        The Machine object.
 * @param   pszKeyBase      The key base.
 * @param   pszKey          The key.
 * @param   pszAttribute    The attribute name.
 * @param   pszValue        The string value.
 */
static HRESULT SetString(ComPtr<IMachine> pMachine, const char *pszKeyBase, const char *pszKey, const char *pszAttribute, const char *pszValue)
{
    HRESULT hrc = pMachine->SetExtraData(Bstr(Utf8StrFmt("%s/%s/%s", pszKeyBase, pszKey, pszAttribute)), Bstr(pszValue));
    if (FAILED(hrc))
        RTPrintf("error: Failed to set '%s/%s/%s' to '%s'! hrc=%#x\n",
                 pszKeyBase, pszKey, pszAttribute, pszValue, hrc);
    return hrc;
}


/**
 * Sets a key value, does necessary error bitching.
 *
 * @returns COM status code.
 * @param   pMachine        The Machine object.
 * @param   pszKeyBase      The key base.
 * @param   pszKey          The key.
 * @param   pszAttribute    The attribute name.
 * @param   u64Value        The value.
 */
static HRESULT SetUInt64(ComPtr<IMachine> pMachine, const char *pszKeyBase, const char *pszKey, const char *pszAttribute, uint64_t u64Value)
{
    char szValue[64];
    RTStrPrintf(szValue, sizeof(szValue), "%#RX64", u64Value);
    return SetString(pMachine, pszKeyBase, pszKey, pszAttribute, szValue);
}


/**
 * Sets a key value, does necessary error bitching.
 *
 * @returns COM status code.
 * @param   pMachine        The Machine object.
 * @param   pszKeyBase      The key base.
 * @param   pszKey          The key.
 * @param   pszAttribute    The attribute name.
 * @param   i64Value        The value.
 */
static HRESULT SetInt64(ComPtr<IMachine> pMachine, const char *pszKeyBase, const char *pszKey, const char *pszAttribute, int64_t i64Value)
{
    char szValue[64];
    RTStrPrintf(szValue, sizeof(szValue), "%RI64", i64Value);
    return SetString(pMachine, pszKeyBase, pszKey, pszAttribute, szValue);
}


/**
 * Identical to the 'loadsyms' command.
 */
static int CmdLoadSyms(int argc, char **argv, ComPtr<IVirtualBox> aVirtualBox, ComPtr<ISession> aSession)
{
    HRESULT rc;

    /*
     * Get the VM
     */
    ComPtr<IMachine> machine;
    /* assume it's a UUID */
    rc = aVirtualBox->GetMachine(Guid(argv[0]), machine.asOutParam());
    if (FAILED(rc) || !machine)
    {
        /* must be a name */
        CHECK_ERROR_RET(aVirtualBox, FindMachine(Bstr(argv[0]), machine.asOutParam()), 1);
    }

    /*
     * Parse the command.
     */
    const char *pszFilename;
    int64_t     offDelta = 0;
    const char *pszModule = NULL;
    uint64_t    ModuleAddress = ~0;
    uint64_t    ModuleSize = 0;

    /* filename */
    if (argc < 2)
        return errorArgument("Missing the filename argument!\n");
    pszFilename = argv[1];

    /* offDelta */
    if (argc >= 3)
    {
        int rc = RTStrToInt64Ex(argv[2], NULL, 0, &offDelta);
        if (VBOX_FAILURE(rc))
            return errorArgument(argv[0], "Failed to read delta '%s', rc=%Vrc\n", argv[2], rc);
    }

    /* pszModule */
    if (argc >= 4)
        pszModule = argv[3];

    /* ModuleAddress */
    if (argc >= 5)
    {
        int rc = RTStrToUInt64Ex(argv[4], NULL, 0, &ModuleAddress);
        if (VBOX_FAILURE(rc))
            return errorArgument(argv[0], "Failed to read module address '%s', rc=%Vrc\n", argv[4], rc);
    }

    /* ModuleSize */
    if (argc >= 6)
    {
        int rc = RTStrToUInt64Ex(argv[5], NULL, 0, &ModuleSize);
        if (VBOX_FAILURE(rc))
            return errorArgument(argv[0], "Failed to read module size '%s', rc=%Vrc\n", argv[5], rc);
    }

    /*
     * Add extra data.
     */
    Utf8Str KeyStr;
    HRESULT hrc = NewUniqueKey(machine, "VBoxInternal/DBGF/loadsyms", KeyStr);
    if (SUCCEEDED(hrc))
        hrc = SetString(machine, "VBoxInternal/DBGF/loadsyms", KeyStr, "Filename", pszFilename);
    if (SUCCEEDED(hrc) && argc >= 3)
        hrc = SetInt64(machine, "VBoxInternal/DBGF/loadsyms", KeyStr, "Delta", offDelta);
    if (SUCCEEDED(hrc) && argc >= 4)
        hrc = SetString(machine, "VBoxInternal/DBGF/loadsyms", KeyStr, "Module", pszModule);
    if (SUCCEEDED(hrc) && argc >= 5)
        hrc = SetUInt64(machine, "VBoxInternal/DBGF/loadsyms", KeyStr, "ModuleAddress", ModuleAddress);
    if (SUCCEEDED(hrc) && argc >= 6)
        hrc = SetUInt64(machine, "VBoxInternal/DBGF/loadsyms", KeyStr, "ModuleSize", ModuleSize);

    return FAILED(hrc);
}

static int handleSetVDIUUID(int argc, char **argv, ComPtr<IVirtualBox> aVirtualBox, ComPtr<ISession> aSession)
{
    /* we need exactly one parameter: the vdi file */
    if (argc != 1)
    {
        return errorSyntax(USAGE_SETVDIUUID, "Not enough parameters");
    }

    /* generate a new UUID */
    Guid uuid;
    uuid.create();

    /* just try it */
    int rc = VDISetImageUUIDs(argv[0], uuid.raw(), NULL, NULL, NULL);
    if (VBOX_FAILURE(rc))
    {
        RTPrintf("Error while setting a new UUID: %Vrc (%d)\n", rc, rc);
    }
    else
    {
        RTPrintf("UUID changed to: %s\n", uuid.toString().raw());
    }

    return 0;
}

/**
 * Wrapper for handling internal commands
 */
int handleInternalCommands(int argc, char *argv[],
                           ComPtr <IVirtualBox> aVirtualBox, ComPtr<ISession> aSession)
{
    g_fInternalMode = true;

    /* at least a command is required */
    if (argc < 1)
        return errorSyntax(USAGE_ALL, "Command missing");

    /*
     * The 'string switch' on command name.
     */
    const char *pszCmd = argv[0];
    if (!strcmp(pszCmd, "loadsyms"))
        return CmdLoadSyms(argc - 1, &argv[1], aVirtualBox, aSession);
    //if (!strcmp(pszCmd, "unloadsyms"))
    //    return CmdUnloadSyms(argc - 1 , &argv[1]);
    if (!strcmp(pszCmd, "setvdiuuid"))
        return handleSetVDIUUID(argc - 1, &argv[1], aVirtualBox, aSession);
#ifndef VBOX_OSE
    if (!strcmp(pszCmd, "listpartitions"))
        return CmdListPartitions(argc - 1, &argv[1], aVirtualBox, aSession);
    if (!strcmp(pszCmd, "createrawvmdk"))
        return CmdCreateRawVMDK(argc - 1, &argv[1], aVirtualBox, aSession);
#endif /* !VBOX_OSE */

    /* default: */
    return errorSyntax(USAGE_ALL, "Invalid command '%s'", Utf8Str(argv[0]).raw());
}
