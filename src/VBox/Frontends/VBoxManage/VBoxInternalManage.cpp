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
#include <VBox/VBoxHDD-new.h>
#include <VBox/sup.h>

#include "VBoxManage.h"

/* Includes for the raw disk stuff. */
#ifdef RT_OS_WINDOWS
#include <windows.h>
#include <winioctl.h>
#elif RT_OS_LINUX
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <linux/hdreg.h>
#include <linux/fs.h>
#endif /* !RT_OS_WINDOWS && !RT_OS_LINUX */

using namespace com;


/** Macro for checking whether a partition is of extended type or not. */
#define PARTTYPE_IS_EXTENDED(x) ((x) == 0x05 || (x) == 0x0f || (x) == 0x85)

/* Maximum number of partitions we can deal with. Ridiculously large number,
 * but the memory consumption is rather low so who cares about never using
 * most entries. */
#define HOSTPARTITION_MAX 100


typedef struct HOSTPARTITION
{
    unsigned        uIndex;
    unsigned        uType;
    unsigned        uStartCylinder;
    unsigned        uStartHead;
    unsigned        uStartSector;
    unsigned        uEndCylinder;
    unsigned        uEndHead;
    unsigned        uEndSector;
    uint64_t        uStart;
    uint64_t        uSize;
    uint64_t        uPartDataStart;
    uint64_t        cPartDataSectors;
} HOSTPARTITION, *PHOSTPARTITION;

typedef struct HOSTPARTITIONS
{
    unsigned        cPartitions;
    HOSTPARTITION   aPartitions[HOSTPARTITION_MAX];
} HOSTPARTITIONS, *PHOSTPARTITIONS;

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
             "%s%s%s%s%s%s%s"
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
                : "",
#ifdef RT_OS_WINDOWS
            (u64Cmd & USAGE_MODINSTALL) ?
                "  modinstall\n"
                "       Installs the neccessary driver for the host OS\n"
                "\n"
                : "",
            (u64Cmd & USAGE_MODUNINSTALL) ?
                "  moduninstall\n"
                "       Deinstalls the driver\n"
                "\n"
                : ""
#else
                "",
                ""
#endif
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
    int rc = RTUtf16ToUtf8(Keys.raw(), &pszKeys);
    if (RT_SUCCESS(rc))
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


static DECLCALLBACK(void) handleVDError(void *pvUser, int rc, RT_SRC_POS_DECL, const char *pszFormat, va_list va)
{
    RTPrintf("ERROR: ");
    RTPrintfV(pszFormat, va);
    RTPrintf("\n");
    RTPrintf("Error code %Vrc at %s(%u) in function %s\n", rc, RT_SRC_POS_ARGS);
}

static int partRead(RTFILE File, PHOSTPARTITIONS pPart)
{
    uint8_t aBuffer[512];
    int rc;

    pPart->cPartitions = 0;
    memset(pPart->aPartitions, '\0', sizeof(pPart->aPartitions));
    rc = RTFileReadAt(File, 0, &aBuffer, sizeof(aBuffer), NULL);
    if (VBOX_FAILURE(rc))
        return rc;
    if (aBuffer[510] != 0x55 || aBuffer[511] != 0xaa)
        return VERR_INVALID_PARAMETER;

    unsigned uExtended = (unsigned)-1;

    for (unsigned i = 0; i < 4; i++)
    {
        uint8_t *p = &aBuffer[0x1be + i * 16];
        if (p[4] == 0)
            continue;
        PHOSTPARTITION pCP = &pPart->aPartitions[pPart->cPartitions++];
        pCP->uIndex = i + 1;
        pCP->uType = p[4];
        pCP->uStartCylinder = (uint32_t)p[3] + ((uint32_t)(p[2] & 0xc0) << 2);
        pCP->uStartHead = p[1];
        pCP->uStartSector = p[2] & 0x3f;
        pCP->uEndCylinder = (uint32_t)p[7] + ((uint32_t)(p[6] & 0xc0) << 2);
        pCP->uEndHead = p[5];
        pCP->uEndSector = p[6] & 0x3f;
        pCP->uStart = RT_MAKE_U32_FROM_U8(p[8], p[9], p[10], p[11]);
        pCP->uSize = RT_MAKE_U32_FROM_U8(p[12], p[13], p[14], p[15]);
        pCP->uPartDataStart = 0;    /* will be filled out later properly. */
        pCP->cPartDataSectors = 0;

        if (PARTTYPE_IS_EXTENDED(p[4]))
        {
            if (uExtended == (unsigned)-1)
                uExtended = pCP - pPart->aPartitions;
            else
            {
                RTPrintf("More than one extended partition. Aborting\n");
                return VERR_INVALID_PARAMETER;
            }
        }
    }

    if (uExtended != (unsigned)-1)
    {
        unsigned uIndex = 5;
        uint64_t uStart = pPart->aPartitions[uExtended].uStart;
        uint64_t uOffset = 0;
        if (!uStart)
        {
            RTPrintf("Inconsistency for logical partition start. Aborting\n");
            return VERR_INVALID_PARAMETER;
        }

        do
        {
            rc = RTFileReadAt(File, (uStart + uOffset) * 512, &aBuffer, sizeof(aBuffer), NULL);
            if (VBOX_FAILURE(rc))
                return rc;

            if (aBuffer[510] != 0x55 || aBuffer[511] != 0xaa)
            {
                RTPrintf("Logical partition without magic. Aborting\n");
                return VERR_INVALID_PARAMETER;
            }
            uint8_t *p = &aBuffer[0x1be];

            if (p[4] == 0)
            {
                RTPrintf("Logical partition with type 0 encountered. Aborting\n");
                return VERR_INVALID_PARAMETER;
            }

            PHOSTPARTITION pCP = &pPart->aPartitions[pPart->cPartitions++];
            pCP->uIndex = uIndex;
            pCP->uType = p[4];
            pCP->uStartCylinder = (uint32_t)p[3] + ((uint32_t)(p[2] & 0xc0) << 2);
            pCP->uStartHead = p[1];
            pCP->uStartSector = p[2] & 0x3f;
            pCP->uEndCylinder = (uint32_t)p[7] + ((uint32_t)(p[6] & 0xc0) << 2);
            pCP->uEndHead = p[5];
            pCP->uEndSector = p[6] & 0x3f;
            uint32_t uStartOffset = RT_MAKE_U32_FROM_U8(p[8], p[9], p[10], p[11]);
            pCP->uStart = uStart + uOffset + uStartOffset;
            pCP->uSize = RT_MAKE_U32_FROM_U8(p[12], p[13], p[14], p[15]);
            /* Fill out partitioning location info for EBR. */
            pCP->uPartDataStart = uStart + uOffset;
            pCP->cPartDataSectors = RT_MIN(uStartOffset, 63);
            p += 16;
            if (p[4] == 0)
                uExtended = (unsigned)-1;
            else if (PARTTYPE_IS_EXTENDED(p[4]))
            {
                uExtended = uIndex++;
                uOffset = RT_MAKE_U32_FROM_U8(p[8], p[9], p[10], p[11]);
            }
            else
            {
                RTPrintf("Logical partition chain broken. Aborting\n");
                return VERR_INVALID_PARAMETER;
            }
        } while (uExtended != (unsigned)-1);
    }

    /* Sort partitions in ascending order of start sector. Also do a lot of
     * consistency checking. */
    uint64_t uPrevEnd = 0;
    for (unsigned i = 0; i < pPart->cPartitions-1; i++)
    {
        unsigned uMinIdx = i;
        uint64_t uMinVal = pPart->aPartitions[i].uStart;
        for (unsigned j = i + 1; j < pPart->cPartitions; j++)
        {
            if (pPart->aPartitions[j].uStart < uMinVal)
            {
                uMinIdx = j;
                uMinVal = pPart->aPartitions[j].uStart;
            }
            else if (pPart->aPartitions[j].uStart == uMinVal)
            {
                RTPrintf("Two partitions start at the same place. Aborting\n");
                return VERR_INVALID_PARAMETER;
            } else if (pPart->aPartitions[j].uStart == 0)
            {
                RTPrintf("Partition starts at sector 0. Aborting\n");
                return VERR_INVALID_PARAMETER;
            }
        }
        if (uMinIdx != i)
        {
            /* Swap entries at index i and uMinIdx. */
            memcpy(&pPart->aPartitions[pPart->cPartitions],
                   &pPart->aPartitions[i], sizeof(HOSTPARTITION));
            memcpy(&pPart->aPartitions[i],
                   &pPart->aPartitions[uMinIdx], sizeof(HOSTPARTITION));
            memcpy(&pPart->aPartitions[uMinIdx],
                   &pPart->aPartitions[pPart->cPartitions], sizeof(HOSTPARTITION));
        }
        if (pPart->aPartitions[i].cPartDataSectors)
        {
            if (pPart->aPartitions[i].uPartDataStart < uPrevEnd)
            {
                RTPrintf("Overlapping partition description areas. Aborting\n");
                return VERR_INVALID_PARAMETER;
            }
            uPrevEnd = pPart->aPartitions[i].uPartDataStart + pPart->aPartitions[i].cPartDataSectors;
        }
        if (pPart->aPartitions[i].uStart < uPrevEnd)
        {
            RTPrintf("Overlapping partitions. Aborting\n");
            return VERR_INVALID_PARAMETER;
        }
        if (!PARTTYPE_IS_EXTENDED(pPart->aPartitions[i].uType))
            uPrevEnd = pPart->aPartitions[i].uStart + pPart->aPartitions[i].uSize;
    }

    /* Fill out partitioning location info for MBR. */
    pPart->aPartitions[0].uPartDataStart = 0;
    pPart->aPartitions[0].cPartDataSectors = RT_MIN(pPart->aPartitions[0].uStart, 63);

    return VINF_SUCCESS;
}

HRESULT CmdListPartitions(int argc, char **argv, ComPtr<IVirtualBox> aVirtualBox, ComPtr<ISession> aSession)
{
    Utf8Str rawdisk;

    /* let's have a closer look at the arguments */
    for (int i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "-rawdisk") == 0)
        {
            if (argc <= i + 1)
            {
                return errorArgument("Missing argument to '%s'", argv[i]);
            }
            i++;
            rawdisk = argv[i];
        }
        else
        {
            return errorSyntax(USAGE_LISTPARTITIONS, "Invalid parameter '%s'", Utf8Str(argv[i]).raw());
        }
    }

    if (rawdisk.isEmpty())
        return errorSyntax(USAGE_LISTPARTITIONS, "Mandatory parameter -rawdisk missing");

    RTFILE RawFile;
    int vrc = RTFileOpen(&RawFile, rawdisk.raw(), RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_WRITE);
    if (VBOX_FAILURE(vrc))
    {
        RTPrintf("Error opening the raw disk: %Vrc\n", vrc);
        return vrc;
    }

    HOSTPARTITIONS partitions;
    vrc = partRead(RawFile, &partitions);
    if (VBOX_FAILURE(vrc))
        return vrc;

    RTPrintf("Number  Type   StartCHS       EndCHS      Size (MiB)  Start (Sect)\n");
    for (unsigned i = 0; i < partitions.cPartitions; i++)
    {
        /* Suppress printing the extended partition. Otherwise people
         * might add it to the list of partitions for raw partition
         * access (which is not good). */
        if (PARTTYPE_IS_EXTENDED(partitions.aPartitions[i].uType))
            continue;

        RTPrintf("%-7u %#04x  %-4u/%-3u/%-2u  %-4u/%-3u/%-2u    %10llu   %10llu\n",
                 partitions.aPartitions[i].uIndex,
                 partitions.aPartitions[i].uType,
                 partitions.aPartitions[i].uStartCylinder,
                 partitions.aPartitions[i].uStartHead,
                 partitions.aPartitions[i].uStartSector,
                 partitions.aPartitions[i].uEndCylinder,
                 partitions.aPartitions[i].uEndHead,
                 partitions.aPartitions[i].uEndSector,
                 partitions.aPartitions[i].uSize / 2048,
                 partitions.aPartitions[i].uStart);
    }

    return 0;
}

HRESULT CmdCreateRawVMDK(int argc, char **argv, ComPtr<IVirtualBox> aVirtualBox, ComPtr<ISession> aSession)
{
    HRESULT rc = S_OK;
    Bstr filename;
    const char *pszMBRFilename = NULL;
    Utf8Str rawdisk;
    const char *pszPartitions = NULL;
    bool fRegister = false;
    bool fRelative = false;

    /* let's have a closer look at the arguments */
    for (int i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "-filename") == 0)
        {
            if (argc <= i + 1)
            {
                return errorArgument("Missing argument to '%s'", argv[i]);
            }
            i++;
            filename = argv[i];
        }
        else if (strcmp(argv[i], "-mbr") == 0)
        {
            if (argc <= i + 1)
            {
                return errorArgument("Missing argument to '%s'", argv[i]);
            }
            i++;
            pszMBRFilename = argv[i];
        }
        else if (strcmp(argv[i], "-rawdisk") == 0)
        {
            if (argc <= i + 1)
            {
                return errorArgument("Missing argument to '%s'", argv[i]);
            }
            i++;
            rawdisk = argv[i];
        }
        else if (strcmp(argv[i], "-partitions") == 0)
        {
            if (argc <= i + 1)
            {
                return errorArgument("Missing argument to '%s'", argv[i]);
            }
            i++;
            pszPartitions = argv[i];
        }
        else if (strcmp(argv[i], "-register") == 0)
        {
            fRegister = true;
        }
#ifdef RT_OS_LINUX
        else if (strcmp(argv[i], "-relative") == 0)
        {
            fRelative = true;
        }
#endif /* RT_OS_LINUX */
        else
        {
            return errorSyntax(USAGE_CREATERAWVMDK, "Invalid parameter '%s'", Utf8Str(argv[i]).raw());
        }
    }

    if (filename.isEmpty())
        return errorSyntax(USAGE_CREATERAWVMDK, "Mandatory parameter -filename missing");
    if (rawdisk.isEmpty())
        return errorSyntax(USAGE_CREATERAWVMDK, "Mandatory parameter -rawdisk missing");
    if (!pszPartitions && pszMBRFilename)
        return errorSyntax(USAGE_CREATERAWVMDK, "The parameter -mbr is only valid when the parameter -partitions is also present");

    RTFILE RawFile;
    int vrc = RTFileOpen(&RawFile, rawdisk.raw(), RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_WRITE);
    if (VBOX_FAILURE(vrc))
    {
        RTPrintf("Error opening the raw disk: %Vrc\n", vrc);
        return vrc;
    }

    uint64_t cbSize = 0;
#ifdef RT_OS_WINDOWS
    DISK_GEOMETRY DriveGeo;
    DWORD cbDriveGeo;
    /* Windows NT has no IOCTL_DISK_GET_LENGTH_INFORMATION ioctl. This was
     * added to Windows XP, so use the available info from DriveGeo. */
    if (DeviceIoControl((HANDLE)RawFile,
                        IOCTL_DISK_GET_DRIVE_GEOMETRY, NULL, 0,
                        &DriveGeo, sizeof(DriveGeo), &cbDriveGeo, NULL))
    {
        if (DriveGeo.MediaType == FixedMedia)
        {
            cbSize =     DriveGeo.Cylinders.QuadPart
                     *   DriveGeo.TracksPerCylinder
                     *   DriveGeo.SectorsPerTrack
                     *   DriveGeo.BytesPerSector;
        }
        else
            return VERR_MEDIA_NOT_RECOGNIZED;
    }
    else
        rc = RTErrConvertFromWin32(GetLastError());
#elif defined(RT_OS_LINUX)
    struct stat DevStat;
    if (!fstat(RawFile, &DevStat) && S_ISBLK(DevStat.st_mode))
    {
        long cBlocks;
        if (!ioctl(RawFile, BLKGETSIZE, &cBlocks))
            cbSize = (uint64_t)cBlocks * 512;
        else
            return RTErrConvertFromErrno(errno);
    }
    else
    {
        RTPrintf("File '%s' is no disk\n", rawdisk.raw());
        return VERR_INVALID_PARAMETER;
    }
#else /* !RT_OS_WINDOWS && !RT_OS_LINUX */
    /* Hopefully this works on all other hosts. If it doesn't, it'll just fail
     * creating the VMDK, so no real harm done. */
    vrc = RTFileGetSize(RawFile, &cbSize);
    if (VBOX_FAILURE(vrc))
    {
        RTPrintf("Error getting the size of the raw disk: %Vrc\n", vrc);
        return vrc;
    }
#endif /* !RT_OS_WINDOWS && !RT_OS_LINUX */

    PVBOXHDD pDisk = NULL;
    VBOXHDDRAW RawDescriptor;
    HOSTPARTITIONS partitions;
    uint32_t uPartitions = 0;

    RawDescriptor.szSignature[0] = 'R';
    RawDescriptor.szSignature[1] = 'A';
    RawDescriptor.szSignature[2] = 'W';
    RawDescriptor.szSignature[3] = '\0';
    if (!pszPartitions)
    {
        RawDescriptor.fRawDisk = true;
        RawDescriptor.pszRawDisk = rawdisk.raw();
    }
    else
    {
        RawDescriptor.fRawDisk = false;
        RawDescriptor.pszRawDisk = NULL;
        RawDescriptor.cPartitions = 0;

        const char *p = pszPartitions;
        char *pszNext;
        uint32_t u32;
        while (*p != '\0')
        {
            vrc = RTStrToUInt32Ex(p, &pszNext, 0, &u32);
            if (VBOX_FAILURE(vrc))
            {
                RTPrintf("Incorrect value in partitions parameter\n");
                return vrc;
            }
            uPartitions |= RT_BIT(u32);
            p = pszNext;
            if (*p == ',')
                p++;
            else if (*p != '\0')
            {
                RTPrintf("Incorrect separator in partitions parameter\n");
                return VERR_INVALID_PARAMETER;
            }
        }

        vrc = partRead(RawFile, &partitions);
        if (VBOX_FAILURE(vrc))
        {
            RTPrintf("Error reading the partition information from '%s'\n", rawdisk.raw());
            return vrc;
        }

        for (unsigned i = 0; i < partitions.cPartitions; i++)
        {
            if (    uPartitions & RT_BIT(partitions.aPartitions[i].uIndex)
                &&  PARTTYPE_IS_EXTENDED(partitions.aPartitions[i].uType))
            {
                /* Some ignorant user specified an extended partition.
                 * Bad idea, as this would trigger an overlapping
                 * partitions error later during VMDK creation. So warn
                 * here and ignore what the user requested. */
                RTPrintf("Warning: it is not possible (and necessary) to explicitly give access to the\n"
                         "         extended partition %u. If required, enable access to all logical\n"
                         "         partitions inside this extended partition.\n", partitions.aPartitions[i].uIndex);
                uPartitions &= ~RT_BIT(partitions.aPartitions[i].uIndex);
            }
        }

        RawDescriptor.cPartitions = partitions.cPartitions;
        RawDescriptor.pPartitions = (PVBOXHDDRAWPART)RTMemAllocZ(partitions.cPartitions * sizeof(VBOXHDDRAWPART));
        if (!RawDescriptor.pPartitions)
            return VERR_NO_MEMORY;
        for (unsigned i = 0; i < partitions.cPartitions; i++)
        {
            if (uPartitions & RT_BIT(partitions.aPartitions[i].uIndex))
            {
                if (fRelative)
                {
#ifdef RT_OS_LINUX
                    /* Refer to the correct partition and use offset 0. */
                    char *pszRawName;
                    vrc = RTStrAPrintf(&pszRawName, "%s%u", rawdisk.raw(),
                                       partitions.aPartitions[i].uIndex);
                    if (VBOX_FAILURE(vrc))
                    {
                        RTPrintf("Error creating reference to individual partition %u, rc=%Vrc\n",
                                 partitions.aPartitions[i].uIndex, vrc);
                        return vrc;
                    }
                    RawDescriptor.pPartitions[i].pszRawDevice = pszRawName;
                    RawDescriptor.pPartitions[i].uPartitionStartOffset = 0;
                    RawDescriptor.pPartitions[i].uPartitionStart = partitions.aPartitions[i].uStart * 512;
#else
                    /** @todo not implemented yet for Windows host. Treat just
                     * like not specified (this code is actually never reached). */
                    RawDescriptor.pPartitions[i].pszRawDevice = rawdisk.raw();
                    RawDescriptor.pPartitions[i].uPartitionStartOffset = partitions.aPartitions[i].uStart * 512;
                    RawDescriptor.pPartitions[i].uPartitionStart = partitions.aPartitions[i].uStart * 512;
#endif
                }
                else
                {
                    /* This is the "everything refers to the base raw device"
                     * variant. This requires opening the base device in RW
                     * mode even for creation. */
                    RawDescriptor.pPartitions[i].pszRawDevice = rawdisk.raw();
                    RawDescriptor.pPartitions[i].uPartitionStartOffset = partitions.aPartitions[i].uStart * 512;
                    RawDescriptor.pPartitions[i].uPartitionStart = partitions.aPartitions[i].uStart * 512;
                }
            }
            else
            {
                /* Suppress access to this partition. */
                RawDescriptor.pPartitions[i].pszRawDevice = NULL;
                RawDescriptor.pPartitions[i].uPartitionStartOffset = 0;
                /* This is used in the plausibility check in the creation
                 * code. In theory it's a dummy, but I don't want to make
                 * the VMDK creatiion any more complicated than what it needs
                 * to be. */
                RawDescriptor.pPartitions[i].uPartitionStart = partitions.aPartitions[i].uStart * 512;
            }
            if (PARTTYPE_IS_EXTENDED(partitions.aPartitions[i].uType))
            {
                /* Suppress exporting the actual extended partition. Only
                 * logical partitions should be processed. However completely
                 * ignoring it leads to leaving out the MBR data. */
                RawDescriptor.pPartitions[i].cbPartition = 0;
            }
            else
                RawDescriptor.pPartitions[i].cbPartition =  partitions.aPartitions[i].uSize * 512;
            RawDescriptor.pPartitions[i].uPartitionDataStart = partitions.aPartitions[i].uPartDataStart * 512;
            RawDescriptor.pPartitions[i].cbPartitionData = partitions.aPartitions[i].cPartDataSectors * 512;
            if (RawDescriptor.pPartitions[i].cbPartitionData)
            {
                Assert (RawDescriptor.pPartitions[i].cbPartitionData -
                        (size_t)RawDescriptor.pPartitions[i].cbPartitionData == 0);
                void *pPartData = RTMemAlloc((size_t)RawDescriptor.pPartitions[i].cbPartitionData);
                if (!pPartData)
                    return VERR_NO_MEMORY;
                vrc = RTFileReadAt(RawFile, partitions.aPartitions[i].uPartDataStart * 512, pPartData, (size_t)RawDescriptor.pPartitions[i].cbPartitionData, NULL);
                if (VBOX_FAILURE(vrc))
                {
                    RTPrintf("Cannot read partition data from raw device '%s': %Vrc\n", rawdisk.raw(), vrc);
                    return vrc;
                }
                /* Splice in the replacement MBR code if specified. */
                if (    partitions.aPartitions[i].uPartDataStart == 0
                    &&  pszMBRFilename)
                {
                    RTFILE MBRFile;
                    vrc = RTFileOpen(&MBRFile, pszMBRFilename, RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_WRITE);
                    if (VBOX_FAILURE(vrc))
                    {
                        RTPrintf("Cannot open replacement MBR file '%s' specified with -mbr: %Vrc\n", pszMBRFilename, vrc);
                        return vrc;
                    }
                    vrc = RTFileReadAt(MBRFile, 0, pPartData, 0x1be, NULL);
                    RTFileClose(MBRFile);
                    if (VBOX_FAILURE(vrc))
                    {
                        RTPrintf("Cannot read replacement MBR file '%s': %Vrc\n", pszMBRFilename, vrc);
                        return vrc;
                    }
                }
                RawDescriptor.pPartitions[i].pvPartitionData = pPartData;
            }
        }
    }

    RTFileClose(RawFile);

    vrc = VDCreate(handleVDError, NULL, &pDisk);
    if (VBOX_FAILURE(vrc))
    {
        RTPrintf("Error while creating the virtual disk container: %Vrc\n", vrc);
        return vrc;
    }

    Assert(RT_MIN(cbSize / 512 / 16 / 63, 16383) -
           (unsigned int)RT_MIN(cbSize / 512 / 16 / 63, 16383) == 0);
    PDMMEDIAGEOMETRY PCHS, LCHS;
    PCHS.cCylinders = (unsigned int)RT_MIN(cbSize / 512 / 16 / 63, 16383);
    PCHS.cHeads = 16;
    PCHS.cSectors = 63;
    LCHS.cCylinders = 0;
    LCHS.cHeads = 0;
    LCHS.cSectors = 0;
    vrc = VDCreateBase(pDisk, "VMDK", Utf8Str(filename).raw(),
                       VD_IMAGE_TYPE_FIXED, cbSize,
		       VD_VMDK_IMAGE_FLAGS_RAWDISK, (char *)&RawDescriptor,
		       &PCHS, &LCHS, VD_OPEN_FLAGS_NORMAL, NULL, NULL);
    if (VBOX_FAILURE(vrc))
    {
        RTPrintf("Error while creating the raw disk VMDK: %Vrc\n", vrc);
        return vrc;
    }
    RTPrintf("RAW host disk access VMDK file %s created successfully.\n", Utf8Str(filename).raw());

    VDCloseAll(pDisk);

    /* Clean up allocated memory etc. */
    if (pszPartitions)
    {
        for (unsigned i = 0; i < partitions.cPartitions; i++)
        {
            if (uPartitions & RT_BIT(partitions.aPartitions[i].uIndex))
            {
                if (fRelative)
                {
#ifdef RT_OS_LINUX
                    /* Free memory allocated above. */
                    RTStrFree((char *)(void *)RawDescriptor.pPartitions[i].pszRawDevice);
#endif /* RT_OS_LINUX */
                }
            }
        }
    }

    if (fRegister)
    {
        ComPtr<IHardDisk> hardDisk;
        CHECK_ERROR(aVirtualBox, OpenHardDisk(filename, hardDisk.asOutParam()));

        if (SUCCEEDED(rc) && hardDisk)
        {
            CHECK_ERROR(aVirtualBox, RegisterHardDisk(hardDisk));
        }
    }

    return SUCCEEDED(rc) ? 0 : 1;
}

/**
 * Unloads the neccessary driver.
 *
 * @returns VBox status code
 */
int CmdModUninstall(void)
{
    int rc;

    rc = SUPUninstall();
    if (RT_SUCCESS(rc))
        return 0;
    if (rc == VERR_NOT_IMPLEMENTED)
        return 0;
    return E_FAIL;
}

/**
 * Loads the neccessary driver.
 *
 * @returns VBox status code
 */
int CmdModInstall(void)
{
    int rc;

    rc = SUPInstall();
    if (RT_SUCCESS(rc))
        return 0;
    if (rc == VERR_NOT_IMPLEMENTED)
        return 0;
    return E_FAIL;
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
    if (!strcmp(pszCmd, "listpartitions"))
        return CmdListPartitions(argc - 1, &argv[1], aVirtualBox, aSession);
    if (!strcmp(pszCmd, "createrawvmdk"))
        return CmdCreateRawVMDK(argc - 1, &argv[1], aVirtualBox, aSession);

    if (!strcmp(pszCmd, "modinstall"))
        return CmdModInstall();
    if (!strcmp(pszCmd, "moduninstall"))
        return CmdModUninstall();

    /* default: */
    return errorSyntax(USAGE_ALL, "Invalid command '%s'", Utf8Str(argv[0]).raw());
}
