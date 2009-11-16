/* $Id$ */
/** @file
 * Shared firmware code.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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
/** @todo: what should it be? */
#define LOG_GROUP LOG_GROUP_DEV_PC_BIOS
#include <VBox/pdmdev.h>

#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/alloc.h>
#include <iprt/buildconfig.h>
#include <iprt/file.h>
#include <iprt/string.h>
#include <iprt/uuid.h>
#include <VBox/err.h>
#include <VBox/param.h>

#include "../Builtins.h"
#include "../Builtins2.h"
#include "DevFwCommon.h"

#pragma pack(1)

/** DMI header */
typedef struct DMIHDR
{
    uint8_t         u8Type;
    uint8_t         u8Length;
    uint16_t        u16Handle;
} *PDMIHDR;
AssertCompileSize(DMIHDR, 4);

/** DMI BIOS information (Type 0) */
typedef struct DMIBIOSINF
{
    DMIHDR          header;
    uint8_t         u8Vendor;
    uint8_t         u8Version;
    uint16_t        u16Start;
    uint8_t         u8Release;
    uint8_t         u8ROMSize;
    uint64_t        u64Characteristics;
    uint8_t         u8CharacteristicsByte1;
    uint8_t         u8CharacteristicsByte2;
    uint8_t         u8ReleaseMajor;
    uint8_t         u8ReleaseMinor;
    uint8_t         u8FirmwareMajor;
    uint8_t         u8FirmwareMinor;
} *PDMIBIOSINF;
AssertCompileSize(DMIBIOSINF, 0x18);

/** DMI system information (Type 1) */
typedef struct DMISYSTEMINF
{
    DMIHDR          header;
    uint8_t         u8Manufacturer;
    uint8_t         u8ProductName;
    uint8_t         u8Version;
    uint8_t         u8SerialNumber;
    uint8_t         au8Uuid[16];
    uint8_t         u8WakeupType;
    uint8_t         u8SKUNumber;
    uint8_t         u8Family;
} *PDMISYSTEMINF;
AssertCompileSize(DMISYSTEMINF, 0x1b);

/** DMI system enclosure or chassis type (Type 3) */
typedef struct DMICHASSIS
{
    DMIHDR          header;
    uint8_t         u8Manufacturer;
    uint8_t         u8Type;
    uint8_t         u8Version;
    uint8_t         u8SerialNumber;
    uint8_t         u8AssetTag;
    uint8_t         u8BootupState;
    uint8_t         u8PowerSupplyState;
    uint8_t         u8ThermalState;
    uint8_t         u8SecurityStatus;
    /* v2.3+, currently not supported */
    uint32_t        u32OEMdefined;
    uint8_t         u8Height;
    uint8_t         u8NumPowerChords;
    uint8_t         u8ContElems;
    uint8_t         u8ContElemRecLen;
} *PDMICHASSIS;
AssertCompileSize(DMICHASSIS, 0x15);

/** DMI processor information (Type 4) */
typedef struct DMIPROCESSORINF
{
    DMIHDR          header;
    uint8_t         u8SocketDesignation;
    uint8_t         u8ProcessorType;
    uint8_t         u8ProcessorFamily;
    uint8_t         u8ProcessorManufacturer;
    uint64_t        u64ProcessorIdentification;
    uint8_t         u8ProcessorVersion;
    uint8_t         u8Voltage;
    uint16_t        u16ExternalClock;
    uint16_t        u16MaxSpeed;
    uint16_t        u16CurrentSpeed;
    uint8_t         u8Status;
    uint8_t         u8ProcessorUpgrade;
    uint16_t        u16L1CacheHandle;
    uint16_t        u16L2CacheHandle;
    uint16_t        u16L3CacheHandle;
    uint8_t         u8SerialNumber;
    uint8_t         u8AssetTag;
    uint8_t         u8PartNumber;
    uint8_t         u8CoreCount;
    uint8_t         u8CoreEnabled;
    uint8_t         u8ThreadCount;
    uint16_t        u16ProcessorCharacteristics;
    uint16_t        u16ProcessorFamily2;
} *PDMIPROCESSORINF;
AssertCompileSize(DMIPROCESSORINF, 0x2a);

/** DMI OEM strings (Type 11) */
typedef struct DMIOEMSTRINGS
{
    DMIHDR          header;
    uint8_t         u8Count;
    uint8_t         u8VBoxVersion;
    uint8_t         u8VBoxRevision;
} *PDMIOEMSTRINGS;
AssertCompileSize(DMIOEMSTRINGS, 0x7);

/** MPS floating pointer structure */
typedef struct MPSFLOATPTR
{
    uint8_t         au8Signature[4];
    uint32_t        u32MPSAddr;
    uint8_t         u8Length;
    uint8_t         u8SpecRev;
    uint8_t         u8Checksum;
    uint8_t         au8Feature[5];
} *PMPSFLOATPTR;
AssertCompileSize(MPSFLOATPTR, 16);

/** MPS config table header */
typedef struct MPSCFGTBLHEADER
{
    uint8_t         au8Signature[4];
    uint16_t        u16Length;
    uint8_t         u8SpecRev;
    uint8_t         u8Checksum;
    uint8_t         au8OemId[8];
    uint8_t         au8ProductId[12];
    uint32_t        u32OemTablePtr;
    uint16_t        u16OemTableSize;
    uint16_t        u16EntryCount;
    uint32_t        u32AddrLocalApic;
    uint16_t        u16ExtTableLength;
    uint8_t         u8ExtTableChecksxum;
    uint8_t         u8Reserved;
} *PMPSCFGTBLHEADER;
AssertCompileSize(MPSCFGTBLHEADER, 0x2c);

/** MPS processor entry */
typedef struct MPSPROCENTRY
{
    uint8_t         u8EntryType;
    uint8_t         u8LocalApicId;
    uint8_t         u8LocalApicVersion;
    uint8_t         u8CPUFlags;
    uint32_t        u32CPUSignature;
    uint32_t        u32CPUFeatureFlags;
    uint32_t        u32Reserved[2];
} *PMPSPROCENTRY;
AssertCompileSize(MPSPROCENTRY, 20);

/** MPS bus entry */
typedef struct MPSBUSENTRY
{
    uint8_t         u8EntryType;
    uint8_t         u8BusId;
    uint8_t         au8BusTypeStr[6];
} *PMPSBUSENTRY;
AssertCompileSize(MPSBUSENTRY, 8);

/** MPS I/O-APIC entry */
typedef struct MPSIOAPICENTRY
{
    uint8_t         u8EntryType;
    uint8_t         u8Id;
    uint8_t         u8Version;
    uint8_t         u8Flags;
    uint32_t        u32Addr;
} *PMPSIOAPICENTRY;
AssertCompileSize(MPSIOAPICENTRY, 8);

/** MPS I/O-Interrupt entry */
typedef struct MPSIOINTERRUPTENTRY
{
    uint8_t         u8EntryType;
    uint8_t         u8Type;
    uint16_t        u16Flags;
    uint8_t         u8SrcBusId;
    uint8_t         u8SrcBusIrq;
    uint8_t         u8DstIOAPICId;
    uint8_t         u8DstIOAPICInt;
} *PMPSIOIRQENTRY;
AssertCompileSize(MPSIOINTERRUPTENTRY, 8);

#pragma pack()


/**
 * Calculate a simple checksum for the MPS table.
 *
 * @param   data            data
 * @param   len             size of data
 */
static uint8_t sharedfwChecksum(const uint8_t * const au8Data, uint32_t u32Length)
{
    uint8_t u8Sum = 0;
    for (size_t i = 0; i < u32Length; ++i)
        u8Sum += au8Data[i];
    return -u8Sum;
}

/**
 * Construct the DMI table.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pTable      Where to create the DMI table.
 * @param   cbMax       The max size of the DMI table.
 * @param   pUuid       Pointer to the UUID to use if the DmiUuid
 *                      configuration string isn't present.
 * @param   pCfgHandle  The handle to our config node.
 */
int sharedfwPlantDMITable(PPDMDEVINS pDevIns, uint8_t *pTable, unsigned cbMax, PRTUUID pUuid, PCFGMNODE pCfgHandle)
{
    char *pszStr = (char *)pTable;
    int iStrNr;
    int rc;
    char *pszDmiBIOSVendor, *pszDmiBIOSVersion, *pszDmiBIOSReleaseDate;
    int  iDmiBIOSReleaseMajor, iDmiBIOSReleaseMinor, iDmiBIOSFirmwareMajor, iDmiBIOSFirmwareMinor;
    char *pszDmiSystemVendor, *pszDmiSystemProduct, *pszDmiSystemVersion, *pszDmiSystemSerial, *pszDmiSystemUuid, *pszDmiSystemFamily;
    char *pszDmiChassisVendor, *pszDmiChassisVersion, *pszDmiChassisSerial, *pszDmiChassisAssetTag;
    char *pszDmiOEMVBoxVer, *pszDmiOEMVBoxRev;

#define CHECKSIZE(want) \
    do { \
        size_t _max = (size_t)(pszStr + want - (char *)pTable) + 5; /* +1 for strtab terminator +4 for end-of-table entry */ \
        if (_max > cbMax) \
        { \
            return PDMDevHlpVMSetError(pDevIns, VERR_TOO_MUCH_DATA, RT_SRC_POS, \
                   N_("One of the DMI strings is too long. Check all bios/Dmi* configuration entries. At least %zu bytes are needed but there is no space for more than %d bytes"), _max, cbMax); \
        } \
    } while (0)
#define SETSTRING(memb, str) \
    do { \
        if (!str[0]) \
            memb = 0; /* empty string */ \
        else \
        { \
            memb = iStrNr++; \
            size_t _len = strlen(str) + 1; \
            CHECKSIZE(_len); \
            memcpy(pszStr, str, _len); \
            pszStr += _len; \
        } \
    } while (0)
#define READCFGSTR(name, variable, default_value) \
    do { \
        rc = CFGMR3QueryStringAlloc(pCfgHandle, name, & variable); \
        if (rc == VERR_CFGM_VALUE_NOT_FOUND) \
            variable = MMR3HeapStrDup(PDMDevHlpGetVM(pDevIns), MM_TAG_CFGM, default_value); \
        else if (RT_FAILURE(rc)) \
            return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS, \
                    N_("Configuration error: Querying \"" name "\" as a string failed")); \
        else if (!strcmp(variable, "<EMPTY>")) \
            variable[0] = '\0'; \
    } while (0)
#define READCFGINT(name, variable, default_value) \
    do { \
        rc = CFGMR3QueryS32(pCfgHandle, name, & variable); \
        if (rc == VERR_CFGM_VALUE_NOT_FOUND) \
            variable = default_value; \
        else if (RT_FAILURE(rc)) \
            return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS, \
                    N_("Configuration error: Querying \"" name "\" as a Int failed")); \
    } while (0)


    /*
     * Don't change this information otherwise Windows guests will demand re-activation!
     */
    READCFGSTR("DmiBIOSVendor",        pszDmiBIOSVendor,      "innotek GmbH");
    READCFGSTR("DmiBIOSVersion",       pszDmiBIOSVersion,     "VirtualBox");
    READCFGSTR("DmiBIOSReleaseDate",   pszDmiBIOSReleaseDate, "12/01/2006");
    READCFGINT("DmiBIOSReleaseMajor",  iDmiBIOSReleaseMajor,   0);
    READCFGINT("DmiBIOSReleaseMinor",  iDmiBIOSReleaseMinor,   0);
    READCFGINT("DmiBIOSFirmwareMajor", iDmiBIOSFirmwareMajor,  0);
    READCFGINT("DmiBIOSFirmwareMinor", iDmiBIOSFirmwareMinor,  0);
    READCFGSTR("DmiSystemVendor",      pszDmiSystemVendor,    "innotek GmbH");
    READCFGSTR("DmiSystemProduct",     pszDmiSystemProduct,   "VirtualBox");
    READCFGSTR("DmiSystemVersion",     pszDmiSystemVersion,   "1.2");
    READCFGSTR("DmiSystemSerial",      pszDmiSystemSerial,    "0");
    rc = CFGMR3QueryStringAlloc(pCfgHandle, "DmiSystemUuid", &pszDmiSystemUuid);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        pszDmiSystemUuid = NULL;
    else if (RT_FAILURE(rc))
        return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS,
                                   N_("Configuration error: Querying \"DmiUuid\" as a string failed"));
    READCFGSTR("DmiSystemFamily",      pszDmiSystemFamily,    "Virtual Machine");
    READCFGSTR("DmiChassisVendor",     pszDmiChassisVendor,   "Sun Microsystems, Inc.");
    READCFGSTR("DmiChassisVersion",    pszDmiChassisVersion,  ""); /* default not specified */
    READCFGSTR("DmiChassisSerial",     pszDmiChassisSerial,   ""); /* default not specified */
    READCFGSTR("DmiChassisAssetTag",   pszDmiChassisAssetTag, ""); /* default not specified */

    /* DMI BIOS information (Type 0) */
    PDMIBIOSINF pBIOSInf         = (PDMIBIOSINF)pszStr;
    CHECKSIZE(sizeof(*pBIOSInf));

    pszStr                       = (char *)&pBIOSInf->u8ReleaseMajor;
    pBIOSInf->header.u8Length    = RT_OFFSETOF(DMIBIOSINF, u8ReleaseMajor);

    /* don't set these fields by default for legacy compatibility */
    if (iDmiBIOSReleaseMajor != 0 || iDmiBIOSReleaseMinor != 0)
    {
        pszStr = (char *)&pBIOSInf->u8FirmwareMajor;
        pBIOSInf->header.u8Length = RT_OFFSETOF(DMIBIOSINF, u8FirmwareMajor);
        pBIOSInf->u8ReleaseMajor  = iDmiBIOSReleaseMajor;
        pBIOSInf->u8ReleaseMinor  = iDmiBIOSReleaseMinor;
        if (iDmiBIOSFirmwareMajor != 0 || iDmiBIOSFirmwareMinor != 0)
        {
            pszStr = (char *)(pBIOSInf + 1);
            pBIOSInf->header.u8Length = sizeof(DMIBIOSINF);
            pBIOSInf->u8FirmwareMajor = iDmiBIOSFirmwareMajor;
            pBIOSInf->u8FirmwareMinor = iDmiBIOSFirmwareMinor;
        }
    }

    iStrNr                       = 1;
    pBIOSInf->header.u8Type      = 0; /* BIOS Information */
    pBIOSInf->header.u16Handle   = 0x0000;
    SETSTRING(pBIOSInf->u8Vendor,  pszDmiBIOSVendor);
    SETSTRING(pBIOSInf->u8Version, pszDmiBIOSVersion);
    pBIOSInf->u16Start           = 0xE000;
    SETSTRING(pBIOSInf->u8Release, pszDmiBIOSReleaseDate);
    pBIOSInf->u8ROMSize          = 1; /* 128K */
    pBIOSInf->u64Characteristics = RT_BIT(4)   /* ISA is supported */
                                 | RT_BIT(7)   /* PCI is supported */
                                 | RT_BIT(15)  /* Boot from CD is supported */
                                 | RT_BIT(16)  /* Selectable Boot is supported */
                                 | RT_BIT(27)  /* Int 9h, 8042 Keyboard services supported */
                                 | RT_BIT(30)  /* Int 10h, CGA/Mono Video Services supported */
                                 /* any more?? */
                                 ;
    pBIOSInf->u8CharacteristicsByte1 = RT_BIT(0)   /* ACPI is supported */
                                     /* any more?? */
                                     ;
    pBIOSInf->u8CharacteristicsByte2 = 0
                                     /* any more?? */
                                     ;
    *pszStr++                    = '\0';

    /* DMI system information (Type 1) */
    PDMISYSTEMINF pSystemInf     = (PDMISYSTEMINF)pszStr;
    CHECKSIZE(sizeof(*pSystemInf));
    pszStr                       = (char *)(pSystemInf + 1);
    iStrNr                       = 1;
    pSystemInf->header.u8Type    = 1; /* System Information */
    pSystemInf->header.u8Length  = sizeof(*pSystemInf);
    pSystemInf->header.u16Handle = 0x0001;
    SETSTRING(pSystemInf->u8Manufacturer, pszDmiSystemVendor);
    SETSTRING(pSystemInf->u8ProductName,  pszDmiSystemProduct);
    SETSTRING(pSystemInf->u8Version,      pszDmiSystemVersion);
    SETSTRING(pSystemInf->u8SerialNumber, pszDmiSystemSerial);

    RTUUID uuid;
    if (pszDmiSystemUuid)
    {
        int rc = RTUuidFromStr(&uuid, pszDmiSystemUuid);
        if (RT_FAILURE(rc))
            return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS,
                                       N_("Invalid UUID for DMI tables specified"));
        uuid.Gen.u32TimeLow = RT_H2BE_U32(uuid.Gen.u32TimeLow);
        uuid.Gen.u16TimeMid = RT_H2BE_U16(uuid.Gen.u16TimeMid);
        uuid.Gen.u16TimeHiAndVersion = RT_H2BE_U16(uuid.Gen.u16TimeHiAndVersion);
        pUuid = &uuid;
    }
    memcpy(pSystemInf->au8Uuid, pUuid, sizeof(RTUUID));

    pSystemInf->u8WakeupType     = 6; /* Power Switch */
    pSystemInf->u8SKUNumber      = 0;
    SETSTRING(pSystemInf->u8Family, pszDmiSystemFamily);
    *pszStr++                    = '\0';

    /* DMI System Enclosure or Chassis (Type 3) */
    PDMICHASSIS pChassis         = (PDMICHASSIS)pszStr;
    CHECKSIZE(sizeof(*pChassis));
    pszStr                       = (char*)&pChassis->u32OEMdefined;
    iStrNr                       = 1;
#ifdef VBOX_WITH_DMI_CHASSIS
    pChassis->header.u8Type      = 3; /* System Enclosure or Chassis */
#else
    pChassis->header.u8Type      = 0x7e; /* inactive */
#endif
    pChassis->header.u8Length    = RT_OFFSETOF(DMICHASSIS, u32OEMdefined);
    pChassis->header.u16Handle   = 0x0003;
    SETSTRING(pChassis->u8Manufacturer, pszDmiChassisVendor);
    pChassis->u8Type             = 0x01; /* ''other'', no chassis lock present */
    SETSTRING(pChassis->u8Version, pszDmiChassisVersion);
    SETSTRING(pChassis->u8SerialNumber, pszDmiChassisSerial);
    SETSTRING(pChassis->u8AssetTag, pszDmiChassisAssetTag);
    pChassis->u8BootupState      = 0x03; /* safe */
    pChassis->u8PowerSupplyState = 0x03; /* safe */
    pChassis->u8ThermalState     = 0x03; /* safe */
    pChassis->u8SecurityStatus   = 0x03; /* none XXX */
# if 0
    /* v2.3+, currently not supported */
    pChassis->u32OEMdefined      = 0;
    pChassis->u8Height           = 0; /* unspecified */
    pChassis->u8NumPowerChords   = 0; /* unspecified */
    pChassis->u8ContElems        = 0; /* no contained elements */
    pChassis->u8ContElemRecLen   = 0; /* no contained elements */
# endif
    *pszStr++                    = '\0';

    /* DMI OEM strings */
    PDMIOEMSTRINGS pOEMStrings    = (PDMIOEMSTRINGS)pszStr;
    CHECKSIZE(sizeof(*pOEMStrings));
    pszStr                        = (char *)(pOEMStrings + 1);
    iStrNr                        = 1;
#ifdef VBOX_WITH_DMI_OEMSTRINGS
    pOEMStrings->header.u8Type    = 0xb; /* OEM Strings */
#else
    pOEMStrings->header.u8Type    = 0x7e; /* inactive */
#endif
    pOEMStrings->header.u8Length  = sizeof(*pOEMStrings);
    pOEMStrings->header.u16Handle = 0x0002;
    pOEMStrings->u8Count          = 2;

    char szTmp[64];
    RTStrPrintf(szTmp, sizeof(szTmp), "vboxVer_%u.%u.%u",
                RTBldCfgVersionMajor(), RTBldCfgVersionMinor(), RTBldCfgVersionBuild());
    READCFGSTR("DmiOEMVBoxVer", pszDmiOEMVBoxVer, szTmp);
    RTStrPrintf(szTmp, sizeof(szTmp), "vboxRev_%u", RTBldCfgRevision());
    READCFGSTR("DmiOEMVBoxRev", pszDmiOEMVBoxRev, szTmp);
    SETSTRING(pOEMStrings->u8VBoxVersion, pszDmiOEMVBoxVer);
    SETSTRING(pOEMStrings->u8VBoxRevision, pszDmiOEMVBoxRev);
    *pszStr++                    = '\0';

    /* End-of-table marker - includes padding to account for fixed table size. */
    PDMIHDR pEndOfTable          = (PDMIHDR)pszStr;
    pEndOfTable->u8Type          = 0x7f;
    pEndOfTable->u8Length        = cbMax - ((char *)pszStr - (char *)pTable) - 2;
    pEndOfTable->u16Handle       = 0xFEFF;

    /* If more fields are added here, fix the size check in SETSTRING */

#undef SETSTRING
#undef READCFGSTR
#undef READCFGINT
#undef CHECKSIZE

    MMR3HeapFree(pszDmiBIOSVendor);
    MMR3HeapFree(pszDmiBIOSVersion);
    MMR3HeapFree(pszDmiBIOSReleaseDate);
    MMR3HeapFree(pszDmiSystemVendor);
    MMR3HeapFree(pszDmiSystemProduct);
    MMR3HeapFree(pszDmiSystemVersion);
    MMR3HeapFree(pszDmiSystemSerial);
    MMR3HeapFree(pszDmiSystemUuid);
    MMR3HeapFree(pszDmiSystemFamily);
    MMR3HeapFree(pszDmiChassisVendor);
    MMR3HeapFree(pszDmiChassisVersion);
    MMR3HeapFree(pszDmiChassisSerial);
    MMR3HeapFree(pszDmiChassisAssetTag);
    MMR3HeapFree(pszDmiOEMVBoxVer);
    MMR3HeapFree(pszDmiOEMVBoxRev);

    return VINF_SUCCESS;
}
AssertCompile(VBOX_DMI_TABLE_ENTR == 5);

/**
 * Construct the MPS table. Only applicable if IOAPIC is active!
 *
 * See ``MultiProcessor Specificatiton Version 1.4 (May 1997)'':
 *   ``1.3 Scope
 *     ...
 *     The hardware required to implement the MP specification is kept to a
 *     minimum, as follows:
 *     * One or more processors that are Intel architecture instruction set
 *       compatible, such as the CPUs in the Intel486 or Pentium processor
 *       family.
 *     * One or more APICs, such as the Intel 82489DX Advanced Programmable
 *       Interrupt Controller or the integrated APIC, such as that on the
 *       Intel Pentium 735\90 and 815\100 processors, together with a discrete
 *       I/O APIC unit.''
 * and later:
 *   ``4.3.3 I/O APIC Entries
 *     The configuration table contains one or more entries for I/O APICs.
 *     ...
 *     I/O APIC FLAGS: EN 3:0 1 If zero, this I/O APIC is unusable, and the
 *                              operating system should not attempt to access
 *                              this I/O APIC.
 *                              At least one I/O APIC must be enabled.''
 *
 * @param   pDevIns    The device instance data.
 * @param   addr       physical address in guest memory.
 */
void sharedfwPlantMpsTable(PPDMDEVINS pDevIns, uint8_t *pTable, uint16_t numCpus)
{
    /* configuration table */
    PMPSCFGTBLHEADER pCfgTab      = (MPSCFGTBLHEADER*)pTable;
    memcpy(pCfgTab->au8Signature, "PCMP", 4);
    pCfgTab->u8SpecRev             =  4;    /* 1.4 */
    memcpy(pCfgTab->au8OemId, "VBOXCPU ", 8);
    memcpy(pCfgTab->au8ProductId, "VirtualBox  ", 12);
    pCfgTab->u32OemTablePtr        =  0;
    pCfgTab->u16OemTableSize       =  0;
    pCfgTab->u16EntryCount         =  numCpus /* Processors */
                                   +  1 /* ISA Bus */
                                   +  1 /* I/O-APIC */
                                   + 16 /* Interrupts */;
    pCfgTab->u32AddrLocalApic      = 0xfee00000;
    pCfgTab->u16ExtTableLength     =  0;
    pCfgTab->u8ExtTableChecksxum   =  0;
    pCfgTab->u8Reserved            =  0;

    uint32_t u32Eax, u32Ebx, u32Ecx, u32Edx;
    uint32_t u32CPUSignature = 0x0520; /* default: Pentium 100 */
    uint32_t u32FeatureFlags = 0x0001; /* default: FPU */
    PDMDevHlpGetCpuId(pDevIns, 0, &u32Eax, &u32Ebx, &u32Ecx, &u32Edx);
    if (u32Eax >= 1)
    {
        PDMDevHlpGetCpuId(pDevIns, 1, &u32Eax, &u32Ebx, &u32Ecx, &u32Edx);
        u32CPUSignature = u32Eax & 0xfff;
        /* Local APIC will be enabled later so override it here. Since we provide
         * an MP table we have an IOAPIC and therefore a Local APIC. */
        u32FeatureFlags = u32Edx | X86_CPUID_FEATURE_EDX_APIC;
    }
    /* Construct MPS table for each VCPU. */
    PMPSPROCENTRY pProcEntry = (PMPSPROCENTRY)(pCfgTab+1);
    for (int i = 0; i<numCpus; i++)
    {
        pProcEntry->u8EntryType        = 0; /* processor entry */
        pProcEntry->u8LocalApicId      = i;
        pProcEntry->u8LocalApicVersion = 0x11;
        pProcEntry->u8CPUFlags         = (i == 0 ? 2 /* bootstrap processor */ : 0 /* application processor */) | 1 /* enabled */;
        pProcEntry->u32CPUSignature    = u32CPUSignature;
        pProcEntry->u32CPUFeatureFlags = u32FeatureFlags;
        pProcEntry->u32Reserved[0]     =
        pProcEntry->u32Reserved[1]     = 0;
        pProcEntry++;
    }

    /* ISA bus */
    PMPSBUSENTRY pBusEntry         = (PMPSBUSENTRY)(pProcEntry+1);
    pBusEntry->u8EntryType         = 1; /* bus entry */
    pBusEntry->u8BusId             = 0; /* this ID is referenced by the interrupt entries */
    memcpy(pBusEntry->au8BusTypeStr, "ISA   ", 6);

    /* PCI bus? */

    /* I/O-APIC.
     * MP spec: "The configuration table contains one or more entries for I/O APICs.
     *           ... At least one I/O APIC must be enabled." */
    PMPSIOAPICENTRY pIOAPICEntry   = (PMPSIOAPICENTRY)(pBusEntry+1);
    uint16_t apicId = numCpus;
    pIOAPICEntry->u8EntryType      = 2; /* I/O-APIC entry */
    pIOAPICEntry->u8Id             = apicId; /* this ID is referenced by the interrupt entries */
    pIOAPICEntry->u8Version        = 0x11;
    pIOAPICEntry->u8Flags          = 1 /* enable */;
    pIOAPICEntry->u32Addr          = 0xfec00000;

    PMPSIOIRQENTRY pIrqEntry       = (PMPSIOIRQENTRY)(pIOAPICEntry+1);
    for (int i = 0; i < 16; i++, pIrqEntry++)
    {
        pIrqEntry->u8EntryType     = 3; /* I/O interrupt entry */
        pIrqEntry->u8Type          = 0; /* INT, vectored interrupt */
        pIrqEntry->u16Flags        = 0; /* polarity of APIC I/O input signal = conforms to bus,
                                           trigger mode = conforms to bus */
        pIrqEntry->u8SrcBusId      = 0; /* ISA bus */
        pIrqEntry->u8SrcBusIrq     = i;
        pIrqEntry->u8DstIOAPICId   = apicId;
        pIrqEntry->u8DstIOAPICInt  = i;
    }

    pCfgTab->u16Length             = (uint8_t*)pIrqEntry - pTable;
    pCfgTab->u8Checksum            = sharedfwChecksum(pTable, pCfgTab->u16Length);

    AssertMsg(pCfgTab->u16Length < 0x1000 - 0x100,
              ("VBOX_MPS_TABLE_SIZE=%d, maximum allowed size is %d",
              pCfgTab->u16Length, 0x1000-0x100));

    MPSFLOATPTR floatPtr;
    floatPtr.au8Signature[0]       = '_';
    floatPtr.au8Signature[1]       = 'M';
    floatPtr.au8Signature[2]       = 'P';
    floatPtr.au8Signature[3]       = '_';
    floatPtr.u32MPSAddr            = VBOX_MPS_TABLE_BASE;
    floatPtr.u8Length              = 1; /* structure size in paragraphs */
    floatPtr.u8SpecRev             = 4; /* MPS revision 1.4 */
    floatPtr.u8Checksum            = 0;
    floatPtr.au8Feature[0]         = 0;
    floatPtr.au8Feature[1]         = 0;
    floatPtr.au8Feature[2]         = 0;
    floatPtr.au8Feature[3]         = 0;
    floatPtr.au8Feature[4]         = 0;
    floatPtr.u8Checksum            = sharedfwChecksum((uint8_t*)&floatPtr, 16);
    PDMDevHlpPhysWrite (pDevIns, 0x9fff0, &floatPtr, 16);
}

