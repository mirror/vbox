/* $Id$ */
/** @file
 * VBoxManage - The 'internalcommands' command.
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
#include <VBox/com/errorprint2.h>

#include <VBox/com/VirtualBox.h>

#include <VBox/VBoxHDD.h>
#include <VBox/sup.h>
#include <VBox/err.h>
#include <VBox/log.h>

#include <iprt/file.h>
#include <iprt/initterm.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/uuid.h>


#include "VBoxManage.h"

/* Includes for the raw disk stuff. */
#ifdef RT_OS_WINDOWS
# include <windows.h>
# include <winioctl.h>
#elif defined(RT_OS_LINUX) || defined(RT_OS_DARWIN) || defined(RT_OS_SOLARIS)
# include <errno.h>
# include <sys/ioctl.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <fcntl.h>
# include <unistd.h>
#endif
#ifdef RT_OS_LINUX
# include <sys/utsname.h>
# include <linux/hdreg.h>
# include <linux/fs.h>
# include <stdlib.h> /* atoi() */
#endif /* RT_OS_LINUX */
#ifdef RT_OS_DARWIN
# include <sys/disk.h>
#endif /* RT_OS_DARWIN */
#ifdef RT_OS_SOLARIS
# include <stropts.h>
# include <sys/dkio.h>
# include <sys/vtoc.h>
#endif /* RT_OS_SOLARIS */

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
    /** partition type */
    unsigned        uType;
    /** CHS/cylinder of the first sector */
    unsigned        uStartCylinder;
    /** CHS/head of the first sector */
    unsigned        uStartHead;
    /** CHS/head of the first sector */
    unsigned        uStartSector;
    /** CHS/cylinder of the last sector */
    unsigned        uEndCylinder;
    /** CHS/head of the last sector */
    unsigned        uEndHead;
    /** CHS/sector of the last sector */
    unsigned        uEndSector;
    /** start sector of this partition relative to the beginning of the hard
     * disk or relative to the beginning of the extended partition table */
    uint64_t        uStart;
    /** numer of sectors of the partition */
    uint64_t        uSize;
    /** start sector of this partition _table_ */
    uint64_t        uPartDataStart;
    /** numer of sectors of this partition _table_ */
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
             "%s%s%s%s%s%s%s%s%s"
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
            (u64Cmd & USAGE_SETHDUUID) ?
                "  sethduuid <filepath>\n"
                "       Assigns a new UUID to the given image file. This way, multiple copies\n"
                "       of a container can be registered.\n"
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
                "       \\\\.\\PhysicalDrive0).\n"
                "       On Linux host the parameter -relative causes a VMDK file to be created\n"
                "       which refers to individual partitions instead to the entire disk.\n"
                "       Optionally the created image can be immediately registered.\n"
                "       The necessary partition numbers can be queried with\n"
                "         VBoxManage internalcommands listpartitions\n"
                "\n"
                : "",
             (u64Cmd & USAGE_RENAMEVMDK) ?
                 "  renamevmdk -from <filename> -to <filename>\n"
                 "       Renames an existing VMDK image, including the base file and all its extents.\n"
                 "\n"
                 : "",
             (u64Cmd & USAGE_CONVERTTORAW) ?
                 "  converttoraw [-format <fileformat>] <filename> <outputfile>"
#ifdef ENABLE_CONVERT_RAW_TO_STDOUT
                 "|stdout"
#endif /* ENABLE_CONVERT_RAW_TO_STDOUT */
                 "\n"
                 "       Convert image to raw, writing to file"
#ifdef ENABLE_CONVERT_RAW_TO_STDOUT
                 " or stdout"
#endif /* ENABLE_CONVERT_RAW_TO_STDOUT */
                 ".\n"
                 "\n"
                 : "",
             (u64Cmd & USAGE_CONVERTHD) ?
                 "  converthd [-srcformat VDI|VMDK|VHD|RAW]\n"
                 "            [-dstformat VDI|VMDK|VHD|RAW]\n"
                 "            <inputfile> <outputfile>\n"
                 "       converts hard disk images between formats\n"
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
        RTPrintf("error: failed to delete key '%s' from '%s',  string conversion error %Rrc!\n",
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
        if (RT_FAILURE(rc))
            return errorArgument(argv[0], "Failed to read delta '%s', rc=%Rrc\n", argv[2], rc);
    }

    /* pszModule */
    if (argc >= 4)
        pszModule = argv[3];

    /* ModuleAddress */
    if (argc >= 5)
    {
        int rc = RTStrToUInt64Ex(argv[4], NULL, 0, &ModuleAddress);
        if (RT_FAILURE(rc))
            return errorArgument(argv[0], "Failed to read module address '%s', rc=%Rrc\n", argv[4], rc);
    }

    /* ModuleSize */
    if (argc >= 6)
    {
        int rc = RTStrToUInt64Ex(argv[5], NULL, 0, &ModuleSize);
        if (RT_FAILURE(rc))
            return errorArgument(argv[0], "Failed to read module size '%s', rc=%Rrc\n", argv[5], rc);
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


static DECLCALLBACK(void) handleVDError(void *pvUser, int rc, RT_SRC_POS_DECL, const char *pszFormat, va_list va)
{
    RTPrintf("ERROR: ");
    RTPrintfV(pszFormat, va);
    RTPrintf("\n");
    RTPrintf("Error code %Rrc at %s(%u) in function %s\n", rc, RT_SRC_POS_ARGS);
}

static int handleSetHDUUID(int argc, char **argv, ComPtr<IVirtualBox> aVirtualBox, ComPtr<ISession> aSession)
{
    /* we need exactly one parameter: the image file */
    if (argc != 1)
    {
        return errorSyntax(USAGE_SETHDUUID, "Not enough parameters");
    }

    /* generate a new UUID */
    Guid uuid;
    uuid.create();

    /* just try it */
    char *pszFormat = NULL;
    int rc = VDGetFormat(argv[0], &pszFormat);
    if (RT_FAILURE(rc))
    {
        RTPrintf("Format autodetect failed: %Rrc\n", rc);
        return 1;
    }

    PVBOXHDD pDisk = NULL;

    PVDINTERFACE     pVDIfs = NULL;
    VDINTERFACE      vdInterfaceError;
    VDINTERFACEERROR vdInterfaceErrorCallbacks;
    vdInterfaceErrorCallbacks.cbSize       = sizeof(VDINTERFACEERROR);
    vdInterfaceErrorCallbacks.enmInterface = VDINTERFACETYPE_ERROR;
    vdInterfaceErrorCallbacks.pfnError     = handleVDError;

    rc = VDInterfaceAdd(&vdInterfaceError, "VBoxManage_IError", VDINTERFACETYPE_ERROR,
                        &vdInterfaceErrorCallbacks, NULL, &pVDIfs);
    AssertRC(rc);

    rc = VDCreate(pVDIfs, &pDisk);
    if (RT_FAILURE(rc))
    {
        RTPrintf("Error while creating the virtual disk container: %Rrc\n", rc);
        return 1;
    }

    /* Open the image */
    rc = VDOpen(pDisk, pszFormat, argv[0], VD_OPEN_FLAGS_NORMAL, NULL);
    if (RT_FAILURE(rc))
    {
        RTPrintf("Error while opening the image: %Rrc\n", rc);
        return 1;
    }

    rc = VDSetUuid(pDisk, VD_LAST_IMAGE, uuid.raw());
    if (RT_FAILURE(rc))
        RTPrintf("Error while setting a new UUID: %Rrc\n", rc);
    else
        RTPrintf("UUID changed to: %s\n", uuid.toString().raw());

    VDCloseAll(pDisk);

    return RT_FAILURE(rc);
}

static int partRead(RTFILE File, PHOSTPARTITIONS pPart)
{
    uint8_t aBuffer[512];
    int rc;

    pPart->cPartitions = 0;
    memset(pPart->aPartitions, '\0', sizeof(pPart->aPartitions));
    rc = RTFileReadAt(File, 0, &aBuffer, sizeof(aBuffer), NULL);
    if (RT_FAILURE(rc))
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
                uExtended = (unsigned)(pCP - pPart->aPartitions);
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
            if (RT_FAILURE(rc))
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
            if (!uStartOffset)
            {
                RTPrintf("Invalid partition start offset. Aborting\n");
                return VERR_INVALID_PARAMETER;
            }
            pCP->uStart = uStart + uOffset + uStartOffset;
            pCP->uSize = RT_MAKE_U32_FROM_U8(p[12], p[13], p[14], p[15]);
            /* Fill out partitioning location info for EBR. */
            pCP->uPartDataStart = uStart + uOffset;
            pCP->cPartDataSectors = uStartOffset;
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

    /* Sort partitions in ascending order of start sector, plus a trivial
     * bit of consistency checking. */
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
            }
            else if (pPart->aPartitions[j].uStart == 0)
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
    }

    /* Now do a lot of consistency checking. */
    uint64_t uPrevEnd = 0;
    for (unsigned i = 0; i < pPart->cPartitions-1; i++)
    {
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
    pPart->aPartitions[0].cPartDataSectors = pPart->aPartitions[0].uStart;

    return VINF_SUCCESS;
}

static int CmdListPartitions(int argc, char **argv, ComPtr<IVirtualBox> aVirtualBox, ComPtr<ISession> aSession)
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
    if (RT_FAILURE(vrc))
    {
        RTPrintf("Error opening the raw disk: %Rrc\n", vrc);
        return vrc;
    }

    HOSTPARTITIONS partitions;
    vrc = partRead(RawFile, &partitions);
    if (RT_FAILURE(vrc))
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

static int CmdCreateRawVMDK(int argc, char **argv, ComPtr<IVirtualBox> aVirtualBox, ComPtr<ISession> aSession)
{
    HRESULT rc = S_OK;
    Bstr filename;
    const char *pszMBRFilename = NULL;
    Utf8Str rawdisk;
    const char *pszPartitions = NULL;
    bool fRegister = false;
    bool fRelative = false;

    uint64_t cbSize = 0;
    PVBOXHDD pDisk = NULL;
    VBOXHDDRAW RawDescriptor;
    HOSTPARTITIONS partitions;
    uint32_t uPartitions = 0;
    PVDINTERFACE pVDIfs = NULL;

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
    if (RT_FAILURE(vrc))
    {
        RTPrintf("Error opening the raw disk '%s': %Rrc\n", rawdisk.raw(), vrc);
        goto out;
    }

#ifdef RT_OS_WINDOWS
    /* Windows NT has no IOCTL_DISK_GET_LENGTH_INFORMATION ioctl. This was
     * added to Windows XP, so we have to use the available info from DriveGeo.
     * Note that we cannot simply use IOCTL_DISK_GET_DRIVE_GEOMETRY as it
     * yields a slightly different result than IOCTL_DISK_GET_LENGTH_INFO.
     * We call IOCTL_DISK_GET_DRIVE_GEOMETRY first as we need to check the media
     * type anyway, and if IOCTL_DISK_GET_LENGTH_INFORMATION is supported
     * we will later override cbSize.
     */
    DISK_GEOMETRY DriveGeo;
    DWORD cbDriveGeo;
    if (DeviceIoControl((HANDLE)RawFile,
                        IOCTL_DISK_GET_DRIVE_GEOMETRY, NULL, 0,
                        &DriveGeo, sizeof(DriveGeo), &cbDriveGeo, NULL))
    {
        if (   DriveGeo.MediaType == FixedMedia
            || DriveGeo.MediaType == RemovableMedia)
        {
            cbSize =     DriveGeo.Cylinders.QuadPart
                     *   DriveGeo.TracksPerCylinder
                     *   DriveGeo.SectorsPerTrack
                     *   DriveGeo.BytesPerSector;
        }
        else
        {
            RTPrintf("File '%s' is no fixed/removable medium device\n", rawdisk.raw());
            vrc = VERR_INVALID_PARAMETER;
            goto out;
        }

        GET_LENGTH_INFORMATION DiskLenInfo;
        DWORD junk;
        if (DeviceIoControl((HANDLE)RawFile,
                            IOCTL_DISK_GET_LENGTH_INFO, NULL, 0,
                            &DiskLenInfo, sizeof(DiskLenInfo), &junk, (LPOVERLAPPED)NULL))
        {
            /* IOCTL_DISK_GET_LENGTH_INFO is supported -- override cbSize. */
            cbSize = DiskLenInfo.Length.QuadPart;
        }
    }
    else
    {
        vrc = RTErrConvertFromWin32(GetLastError());
        RTPrintf("Error getting the geometry of the raw disk '%s': %Rrc\n", rawdisk.raw(), vrc);
        goto out;
    }
#elif defined(RT_OS_LINUX)
    struct stat DevStat;
    if (!fstat(RawFile, &DevStat) && S_ISBLK(DevStat.st_mode))
    {
#ifdef BLKGETSIZE64
        /* BLKGETSIZE64 is broken up to 2.4.17 and in many 2.5.x. In 2.6.0
         * it works without problems. */
        struct utsname utsname;
        if (    uname(&utsname) == 0
            &&  (   (strncmp(utsname.release, "2.5.", 4) == 0 && atoi(&utsname.release[4]) >= 18)
                 || (strncmp(utsname.release, "2.", 2) == 0 && atoi(&utsname.release[2]) >= 6)))
        {
            uint64_t cbBlk;
            if (!ioctl(RawFile, BLKGETSIZE64, &cbBlk))
                cbSize = cbBlk;
        }
#endif /* BLKGETSIZE64 */
        if (!cbSize)
        {
            long cBlocks;
            if (!ioctl(RawFile, BLKGETSIZE, &cBlocks))
                cbSize = (uint64_t)cBlocks << 9;
            else
            {
                vrc = RTErrConvertFromErrno(errno);
                RTPrintf("Error getting the size of the raw disk '%s': %Rrc\n", rawdisk.raw(), vrc);
                goto out;
            }
        }
    }
    else
    {
        RTPrintf("File '%s' is no block device\n", rawdisk.raw());
        vrc = VERR_INVALID_PARAMETER;
        goto out;
    }
#elif defined(RT_OS_DARWIN)
    struct stat DevStat;
    if (!fstat(RawFile, &DevStat) && S_ISBLK(DevStat.st_mode))
    {
        uint64_t cBlocks;
        uint32_t cbBlock;
        if (!ioctl(RawFile, DKIOCGETBLOCKCOUNT, &cBlocks))
        {
            if (!ioctl(RawFile, DKIOCGETBLOCKSIZE, &cbBlock))
                cbSize = cBlocks * cbBlock;
            else
            {
                RTPrintf("Cannot get the block size for file '%s': %Rrc", rawdisk.raw(), vrc);
                vrc = RTErrConvertFromErrno(errno);
                goto out;
            }
        }
        else
        {
            vrc = RTErrConvertFromErrno(errno);
            RTPrintf("Cannot get the block count for file '%s': %Rrc", rawdisk.raw(), vrc);
            goto out;
        }
    }
    else
    {
        RTPrintf("File '%s' is no block device\n", rawdisk.raw());
        vrc = VERR_INVALID_PARAMETER;
        goto out;
    }
#elif defined(RT_OS_SOLARIS)
    struct stat DevStat;
    if (!fstat(RawFile, &DevStat) && (   S_ISBLK(DevStat.st_mode)
                                      || S_ISCHR(DevStat.st_mode)))
    {
        struct dk_minfo mediainfo;
        if (!ioctl(RawFile, DKIOCGMEDIAINFO, &mediainfo))
            cbSize = mediainfo.dki_capacity * mediainfo.dki_lbsize;
        else
        {
            vrc = RTErrConvertFromErrno(errno);
            RTPrintf("Error getting the size of the raw disk '%s': %Rrc\n", rawdisk.raw(), vrc);
            goto out;
        }
    }
    else
    {
        RTPrintf("File '%s' is no block or char device\n", rawdisk.raw());
        vrc = VERR_INVALID_PARAMETER;
        goto out;
    }
#else /* all unrecognized OSes */
    /* Hopefully this works on all other hosts. If it doesn't, it'll just fail
     * creating the VMDK, so no real harm done. */
    vrc = RTFileGetSize(RawFile, &cbSize);
    if (RT_FAILURE(vrc))
    {
        RTPrintf("Error getting the size of the raw disk '%s': %Rrc\n", rawdisk.raw(), vrc);
        goto out;
    }
#endif

    /* Check whether cbSize is actually sensible. */
    if (!cbSize || cbSize % 512)
    {
        RTPrintf("Detected size of raw disk '%s' is %s, an invalid value\n", rawdisk.raw(), cbSize);
        vrc = VERR_INVALID_PARAMETER;
        goto out;
    }

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
            if (RT_FAILURE(vrc))
            {
                RTPrintf("Incorrect value in partitions parameter\n");
                goto out;
            }
            uPartitions |= RT_BIT(u32);
            p = pszNext;
            if (*p == ',')
                p++;
            else if (*p != '\0')
            {
                RTPrintf("Incorrect separator in partitions parameter\n");
                vrc = VERR_INVALID_PARAMETER;
                goto out;
            }
        }

        vrc = partRead(RawFile, &partitions);
        if (RT_FAILURE(vrc))
        {
            RTPrintf("Error reading the partition information from '%s'\n", rawdisk.raw());
            goto out;
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
        {
            RTPrintf("Out of memory allocating the partition list for '%s'\n", rawdisk.raw());
            vrc = VERR_NO_MEMORY;
            goto out;
        }
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
                    if (RT_FAILURE(vrc))
                    {
                        RTPrintf("Error creating reference to individual partition %u, rc=%Rrc\n",
                                 partitions.aPartitions[i].uIndex, vrc);
                        goto out;
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
                {
                    RTPrintf("Out of memory allocating the partition descriptor for '%s'\n", rawdisk.raw());
                    vrc = VERR_NO_MEMORY;
                    goto out;
                }
                vrc = RTFileReadAt(RawFile, partitions.aPartitions[i].uPartDataStart * 512, pPartData, (size_t)RawDescriptor.pPartitions[i].cbPartitionData, NULL);
                if (RT_FAILURE(vrc))
                {
                    RTPrintf("Cannot read partition data from raw device '%s': %Rrc\n", rawdisk.raw(), vrc);
                    goto out;
                }
                /* Splice in the replacement MBR code if specified. */
                if (    partitions.aPartitions[i].uPartDataStart == 0
                    &&  pszMBRFilename)
                {
                    RTFILE MBRFile;
                    vrc = RTFileOpen(&MBRFile, pszMBRFilename, RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_WRITE);
                    if (RT_FAILURE(vrc))
                    {
                        RTPrintf("Cannot open replacement MBR file '%s' specified with -mbr: %Rrc\n", pszMBRFilename, vrc);
                        goto out;
                    }
                    vrc = RTFileReadAt(MBRFile, 0, pPartData, 0x1be, NULL);
                    RTFileClose(MBRFile);
                    if (RT_FAILURE(vrc))
                    {
                        RTPrintf("Cannot read replacement MBR file '%s': %Rrc\n", pszMBRFilename, vrc);
                        goto out;
                    }
                }
                RawDescriptor.pPartitions[i].pvPartitionData = pPartData;
            }
        }
    }

    RTFileClose(RawFile);

    VDINTERFACE      vdInterfaceError;
    VDINTERFACEERROR vdInterfaceErrorCallbacks;
    vdInterfaceErrorCallbacks.cbSize       = sizeof(VDINTERFACEERROR);
    vdInterfaceErrorCallbacks.enmInterface = VDINTERFACETYPE_ERROR;
    vdInterfaceErrorCallbacks.pfnError     = handleVDError;

    vrc = VDInterfaceAdd(&vdInterfaceError, "VBoxManage_IError", VDINTERFACETYPE_ERROR,
                         &vdInterfaceErrorCallbacks, NULL, &pVDIfs);
    AssertRC(vrc);

    vrc = VDCreate(pVDIfs, &pDisk);
    if (RT_FAILURE(vrc))
    {
        RTPrintf("Error while creating the virtual disk container: %Rrc\n", vrc);
        goto out;
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
    vrc = VDCreateBase(pDisk, "VMDK", Utf8Str(filename).raw(), cbSize,
                       VD_IMAGE_FLAGS_FIXED | VD_VMDK_IMAGE_FLAGS_RAWDISK,
                       (char *)&RawDescriptor, &PCHS, &LCHS, NULL,
                       VD_OPEN_FLAGS_NORMAL, NULL, NULL);
    if (RT_FAILURE(vrc))
    {
        RTPrintf("Error while creating the raw disk VMDK: %Rrc\n", vrc);
        goto out;
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
        CHECK_ERROR(aVirtualBox, OpenHardDisk(filename, AccessMode_ReadWrite, hardDisk.asOutParam()));
    }

    return SUCCEEDED(rc) ? 0 : 1;

out:
    RTPrintf("The raw disk vmdk file was not created\n");
    return RT_SUCCESS(vrc) ? 0 : 1;
}

static int CmdRenameVMDK(int argc, char **argv, ComPtr<IVirtualBox> aVirtualBox, ComPtr<ISession> aSession)
{
    Bstr src;
    Bstr dst;
    /* Parse the arguments. */
    for (int i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "-from") == 0)
        {
            if (argc <= i + 1)
            {
                return errorArgument("Missing argument to '%s'", argv[i]);
            }
            i++;
            src = argv[i];
        }
        else if (strcmp(argv[i], "-to") == 0)
        {
            if (argc <= i + 1)
            {
                return errorArgument("Missing argument to '%s'", argv[i]);
            }
            i++;
            dst = argv[i];
        }
        else
        {
            return errorSyntax(USAGE_RENAMEVMDK, "Invalid parameter '%s'", Utf8Str(argv[i]).raw());
        }
    }

    if (src.isEmpty())
        return errorSyntax(USAGE_RENAMEVMDK, "Mandatory parameter -from missing");
    if (dst.isEmpty())
        return errorSyntax(USAGE_RENAMEVMDK, "Mandatory parameter -to missing");

    PVBOXHDD pDisk = NULL;

    PVDINTERFACE     pVDIfs = NULL;
    VDINTERFACE      vdInterfaceError;
    VDINTERFACEERROR vdInterfaceErrorCallbacks;
    vdInterfaceErrorCallbacks.cbSize       = sizeof(VDINTERFACEERROR);
    vdInterfaceErrorCallbacks.enmInterface = VDINTERFACETYPE_ERROR;
    vdInterfaceErrorCallbacks.pfnError     = handleVDError;

    int vrc = VDInterfaceAdd(&vdInterfaceError, "VBoxManage_IError", VDINTERFACETYPE_ERROR,
                             &vdInterfaceErrorCallbacks, NULL, &pVDIfs);
    AssertRC(vrc);

    vrc = VDCreate(pVDIfs, &pDisk);
    if (RT_FAILURE(vrc))
    {
        RTPrintf("Error while creating the virtual disk container: %Rrc\n", vrc);
        return vrc;
    }
    else
    {
        vrc = VDOpen(pDisk, "VMDK", Utf8Str(src).raw(), VD_OPEN_FLAGS_NORMAL, NULL);
        if (RT_FAILURE(vrc))
        {
            RTPrintf("Error while opening the source image: %Rrc\n", vrc);
        }
        else
        {
            vrc = VDCopy(pDisk, 0, pDisk, "VMDK", Utf8Str(dst).raw(), true, 0, VD_IMAGE_FLAGS_NONE, NULL, NULL, NULL, NULL);
            if (RT_FAILURE(vrc))
            {
                RTPrintf("Error while renaming the image: %Rrc\n", vrc);
            }
        }
    }
    VDCloseAll(pDisk);
    return vrc;
}

static int CmdConvertToRaw(int argc, char **argv, ComPtr<IVirtualBox> aVirtualBox, ComPtr<ISession> aSession)
{
    Bstr srcformat;
    Bstr src;
    Bstr dst;
    bool fWriteToStdOut = false;

    /* Parse the arguments. */
    for (int i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "-format") == 0)
        {
            if (argc <= i + 1)
            {
                return errorArgument("Missing argument to '%s'", argv[i]);
            }
            i++;
            srcformat = argv[i];
        }
        else if (src.isEmpty())
        {
            src = argv[i];
        }
        else if (dst.isEmpty())
        {
            dst = argv[i];
#ifdef ENABLE_CONVERT_RAW_TO_STDOUT
            if (!strcmp(argv[i], "stdout"))
                fWriteToStdOut = true;
#endif /* ENABLE_CONVERT_RAW_TO_STDOUT */
        }
        else
        {
            return errorSyntax(USAGE_CONVERTTORAW, "Invalid parameter '%s'", Utf8Str(argv[i]).raw());
        }
    }

    if (src.isEmpty())
        return errorSyntax(USAGE_CONVERTTORAW, "Mandatory filename parameter missing");
    if (dst.isEmpty())
        return errorSyntax(USAGE_CONVERTTORAW, "Mandatory outputfile parameter missing");

    PVBOXHDD pDisk = NULL;

    PVDINTERFACE     pVDIfs = NULL;
    VDINTERFACE      vdInterfaceError;
    VDINTERFACEERROR vdInterfaceErrorCallbacks;
    vdInterfaceErrorCallbacks.cbSize       = sizeof(VDINTERFACEERROR);
    vdInterfaceErrorCallbacks.enmInterface = VDINTERFACETYPE_ERROR;
    vdInterfaceErrorCallbacks.pfnError     = handleVDError;

    int vrc = VDInterfaceAdd(&vdInterfaceError, "VBoxManage_IError", VDINTERFACETYPE_ERROR,
                             &vdInterfaceErrorCallbacks, NULL, &pVDIfs);
    AssertRC(vrc);

    vrc = VDCreate(pVDIfs, &pDisk);
    if (RT_FAILURE(vrc))
    {
        RTPrintf("Error while creating the virtual disk container: %Rrc\n", vrc);
        return 1;
    }

    /* Open raw output file. */
    RTFILE outFile;
    vrc = VINF_SUCCESS;
    if (fWriteToStdOut)
        outFile = 1;
    else
        vrc = RTFileOpen(&outFile, Utf8Str(dst).raw(), RTFILE_O_OPEN | RTFILE_O_CREATE | RTFILE_O_WRITE | RTFILE_O_DENY_ALL);
    if (RT_FAILURE(vrc))
    {
        VDCloseAll(pDisk);
        RTPrintf("Error while creating destination file \"%s\": %Rrc\n", Utf8Str(dst).raw(), vrc);
        return 1;
    }

    if (srcformat.isEmpty())
    {
        char *pszFormat = NULL;
        vrc = VDGetFormat(Utf8Str(src).raw(), &pszFormat);
        if (RT_FAILURE(vrc))
        {
            VDCloseAll(pDisk);
            if (!fWriteToStdOut)
            {
                RTFileClose(outFile);
                RTFileDelete(Utf8Str(dst).raw());
            }
            RTPrintf("No file format specified and autodetect failed - please specify format: %Rrc\n", vrc);
            return 1;
        }
        srcformat = pszFormat;
        RTStrFree(pszFormat);
    }
    vrc = VDOpen(pDisk, Utf8Str(srcformat).raw(), Utf8Str(src).raw(), VD_OPEN_FLAGS_READONLY, NULL);
    if (RT_FAILURE(vrc))
    {
        VDCloseAll(pDisk);
        if (!fWriteToStdOut)
        {
            RTFileClose(outFile);
            RTFileDelete(Utf8Str(dst).raw());
        }
        RTPrintf("Error while opening the source image: %Rrc\n", vrc);
        return 1;
    }

    uint64_t cbSize = VDGetSize(pDisk, VD_LAST_IMAGE);
    uint64_t offFile = 0;
#define RAW_BUFFER_SIZE _128K
    uint64_t cbBuf = RAW_BUFFER_SIZE;
    void *pvBuf = RTMemAlloc(cbBuf);
    if (pvBuf)
    {
        RTPrintf("Converting image \"%s\" with size %RU64 bytes (%RU64MB) to raw...\n", Utf8Str(src).raw(), cbSize, (cbSize + _1M - 1) / _1M);
        while (offFile < cbSize)
        {
            size_t cb = cbSize - offFile >= (uint64_t)cbBuf ? cbBuf : (size_t)(cbSize - offFile);
            vrc = VDRead(pDisk, offFile, pvBuf, cb);
            if (RT_FAILURE(vrc))
                break;
            vrc = RTFileWrite(outFile, pvBuf, cb, NULL);
            if (RT_FAILURE(vrc))
                break;
            offFile += cb;
        }
        if (RT_FAILURE(vrc))
        {
            VDCloseAll(pDisk);
            if (!fWriteToStdOut)
            {
                RTFileClose(outFile);
                RTFileDelete(Utf8Str(dst).raw());
            }
            RTPrintf("Error copying image data: %Rrc\n", vrc);
            return 1;
        }
    }
    else
    {
        vrc = VERR_NO_MEMORY;
        VDCloseAll(pDisk);
        if (!fWriteToStdOut)
        {
            RTFileClose(outFile);
            RTFileDelete(Utf8Str(dst).raw());
        }
        RTPrintf("Error allocating read buffer: %Rrc\n", vrc);
        return 1;
    }

    if (!fWriteToStdOut)
        RTFileClose(outFile);
    VDCloseAll(pDisk);
    return 0;
}

static int CmdConvertHardDisk(int argc, char **argv, ComPtr<IVirtualBox> aVirtualBox, ComPtr<ISession> aSession)
{
    Bstr srcformat;
    Bstr dstformat;
    Bstr src;
    Bstr dst;
    int vrc;
    PVBOXHDD pSrcDisk = NULL;
    PVBOXHDD pDstDisk = NULL;

    /* Parse the arguments. */
    for (int i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "-srcformat") == 0)
        {
            if (argc <= i + 1)
            {
                return errorArgument("Missing argument to '%s'", argv[i]);
            }
            i++;
            srcformat = argv[i];
        }
        else if (strcmp(argv[i], "-dstformat") == 0)
        {
            if (argc <= i + 1)
            {
                return errorArgument("Missing argument to '%s'", argv[i]);
            }
            i++;
            dstformat = argv[i];
        }
        else if (src.isEmpty())
        {
            src = argv[i];
        }
        else if (dst.isEmpty())
        {
            dst = argv[i];
        }
        else
        {
            return errorSyntax(USAGE_CONVERTHD, "Invalid parameter '%s'", Utf8Str(argv[i]).raw());
        }
    }

    if (src.isEmpty())
        return errorSyntax(USAGE_CONVERTHD, "Mandatory input image parameter missing");
    if (dst.isEmpty())
        return errorSyntax(USAGE_CONVERTHD, "Mandatory output image parameter missing");


    PVDINTERFACE     pVDIfs = NULL;
    VDINTERFACE      vdInterfaceError;
    VDINTERFACEERROR vdInterfaceErrorCallbacks;
    vdInterfaceErrorCallbacks.cbSize       = sizeof(VDINTERFACEERROR);
    vdInterfaceErrorCallbacks.enmInterface = VDINTERFACETYPE_ERROR;
    vdInterfaceErrorCallbacks.pfnError     = handleVDError;

    vrc = VDInterfaceAdd(&vdInterfaceError, "VBoxManage_IError", VDINTERFACETYPE_ERROR,
                         &vdInterfaceErrorCallbacks, NULL, &pVDIfs);
    AssertRC(vrc);

    do
    {
        /* Try to determine input image format */
        if (srcformat.isEmpty())
        {
            char *pszFormat = NULL;
            vrc = VDGetFormat(Utf8Str(src).raw(), &pszFormat);
            if (RT_FAILURE(vrc))
            {
                RTPrintf("No file format specified and autodetect failed - please specify format: %Rrc\n", vrc);
                break;
            }
            srcformat = pszFormat;
            RTStrFree(pszFormat);
        }

        vrc = VDCreate(pVDIfs, &pSrcDisk);
        if (RT_FAILURE(vrc))
        {
            RTPrintf("Error while creating the source virtual disk container: %Rrc\n", vrc);
            break;
        }

        /* Open the input image */
        vrc = VDOpen(pSrcDisk, Utf8Str(srcformat).raw(), Utf8Str(src).raw(), VD_OPEN_FLAGS_READONLY, NULL);
        if (RT_FAILURE(vrc))
        {
            RTPrintf("Error while opening the source image: %Rrc\n", vrc);
            break;
        }

        /* Output format defaults to VDI */
        if (dstformat.isEmpty())
            dstformat = "VDI";

        vrc = VDCreate(pVDIfs, &pDstDisk);
        if (RT_FAILURE(vrc))
        {
            RTPrintf("Error while creating the destination virtual disk container: %Rrc\n", vrc);
            break;
        }

        uint64_t cbSize = VDGetSize(pSrcDisk, VD_LAST_IMAGE);
        RTPrintf("Converting image \"%s\" with size %RU64 bytes (%RU64MB)...\n", Utf8Str(src).raw(), cbSize, (cbSize + _1M - 1) / _1M);

        /* Create the output image */
        vrc = VDCopy(pSrcDisk, VD_LAST_IMAGE, pDstDisk, Utf8Str(dstformat).raw(),
                     Utf8Str(dst).raw(), false, 0, VD_VMDK_IMAGE_FLAGS_STREAM_OPTIMIZED, NULL, NULL, NULL, NULL);
        if (RT_FAILURE(vrc))
        {
            RTPrintf("Error while copying the image: %Rrc\n", vrc);
            break;
        }
    }
    while (0);
    if (pDstDisk)
        VDCloseAll(pDstDisk);
    if (pSrcDisk)
        VDCloseAll(pSrcDisk);

    return RT_SUCCESS(vrc) ? 0 : 1;
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
int handleInternalCommands(HandlerArg *a)
{
    g_fInternalMode = true;

    /* at least a command is required */
    if (a->argc < 1)
        return errorSyntax(USAGE_ALL, "Command missing");

    /*
     * The 'string switch' on command name.
     */
    const char *pszCmd = a->argv[0];
    if (!strcmp(pszCmd, "loadsyms"))
        return CmdLoadSyms(a->argc - 1, &a->argv[1], a->virtualBox, a->session);
    //if (!strcmp(pszCmd, "unloadsyms"))
    //    return CmdUnloadSyms(argc - 1 , &a->argv[1]);
    if (!strcmp(pszCmd, "sethduuid") || !strcmp(pszCmd, "setvdiuuid"))
        return handleSetHDUUID(a->argc - 1, &a->argv[1], a->virtualBox, a->session);
    if (!strcmp(pszCmd, "listpartitions"))
        return CmdListPartitions(a->argc - 1, &a->argv[1], a->virtualBox, a->session);
    if (!strcmp(pszCmd, "createrawvmdk"))
        return CmdCreateRawVMDK(a->argc - 1, &a->argv[1], a->virtualBox, a->session);
    if (!strcmp(pszCmd, "renamevmdk"))
        return CmdRenameVMDK(a->argc - 1, &a->argv[1], a->virtualBox, a->session);
    if (!strcmp(pszCmd, "converttoraw"))
        return CmdConvertToRaw(a->argc - 1, &a->argv[1], a->virtualBox, a->session);
    if (!strcmp(pszCmd, "converthd"))
        return CmdConvertHardDisk(a->argc - 1, &a->argv[1], a->virtualBox, a->session);

    if (!strcmp(pszCmd, "modinstall"))
        return CmdModInstall();
    if (!strcmp(pszCmd, "moduninstall"))
        return CmdModUninstall();

    /* default: */
    return errorSyntax(USAGE_ALL, "Invalid command '%s'", Utf8Str(a->argv[0]).raw());
}

