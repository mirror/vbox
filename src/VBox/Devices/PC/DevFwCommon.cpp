/* $Id$ */
/** @file
 * FwCommon - Shared firmware code (used by DevPcBios & DevEFI).
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
#define LOG_GROUP LOG_GROUP_DEV
#include <VBox/pdmdev.h>

#include <VBox/log.h>
#include <VBox/err.h>
#include <VBox/param.h>

#include <iprt/assert.h>
#include <iprt/buildconfig.h>
#include <iprt/file.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/uuid.h>

#include "../Builtins.h"
#include "../Builtins2.h"
#include "DevFwCommon.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/

/*
 * Default DMI data (legacy).
 * Don't change this information otherwise Windows guests will demand re-activation!
 */
static const int32_t s_iDefDmiBIOSReleaseMajor  = 0;
static const int32_t s_iDefDmiBIOSReleaseMinor  = 0;
static const int32_t s_iDefDmiBIOSFirmwareMajor = 0;
static const int32_t s_iDefDmiBIOSFirmwareMinor = 0;
static const char   *s_szDefDmiBIOSVendor       = "innotek GmbH";
static const char   *s_szDefDmiBIOSVersion      = "VirtualBox";
static const char   *s_szDefDmiBIOSReleaseDate  = "12/01/2006";
static const char   *s_szDefDmiSystemVendor     = "innotek GmbH";
static const char   *s_szDefDmiSystemProduct    = "VirtualBox";
static const char   *s_szDefDmiSystemVersion    = "1.2";
static const char   *s_szDefDmiSystemSerial     = "0";
static const char   *s_szDefDmiSystemFamily     = "Virtual Machine";
static const char   *s_szDefDmiChassisVendor    = "Sun Microsystems, Inc.";
static const char   *s_szDefDmiChassisVersion   = "";
static const char   *s_szDefDmiChassisSerial    = "";
static const char   *s_szDefDmiChassisAssetTag  = "";


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
#pragma pack(1)

typedef struct SMBIOSHDR
{
    uint8_t         au8Signature[4];
    uint8_t         u8Checksum;
    uint8_t         u8Eps;
    uint8_t         u8VersionMajor;
    uint8_t         u8VersionMinor;
    uint16_t        u16MaxStructureSize;
    uint8_t         u8EntryPointRevision;
    uint8_t         u8Pad[5];
} *SMBIOSHDRPTR;
AssertCompileSize(SMBIOSHDR, 16);

typedef struct DMIMAINHDR
{
    uint8_t         au8Signature[5];
    uint8_t         u8Checksum;
    uint16_t        u16TablesLength;
    uint32_t        u32TableBase;
    uint16_t        u16TableEntries;
    uint8_t         u8TableVersion;
} *DMIMAINHDRPTR;
AssertCompileSize(DMIMAINHDR, 15);

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
static uint8_t fwCommonChecksum(const uint8_t * const au8Data, uint32_t u32Length)
{
    uint8_t u8Sum = 0;
    for (size_t i = 0; i < u32Length; ++i)
        u8Sum += au8Data[i];
    return -u8Sum;
}

static bool sharedfwChecksumOk(const uint8_t * const au8Data, uint32_t u32Length)
{
    uint8_t u8Sum = 0;
    for (size_t i = 0; i < u32Length; i++)
        u8Sum += au8Data[i];
    return (u8Sum == 0);
}


/**
 * Construct the DMI table.
 *
 * @returns VBox status code.
 * @param   pDevIns             The device instance.
 * @param   pTable              Where to create the DMI table.
 * @param   cbMax               The max size of the DMI table.
 * @param   pUuid               Pointer to the UUID to use if the DmiUuid
 *                              configuration string isn't present.
 * @param   pCfgHandle          The handle to our config node.
 * @param   fPutSmbiosHeaders   Plant SMBIOS headers if true.
 */
int FwCommonPlantDMITable(PPDMDEVINS pDevIns, uint8_t *pTable, unsigned cbMax, PCRTUUID pUuid, PCFGMNODE pCfgHandle, bool fPutSmbiosHeaders)
{
#define CHECKSIZE(cbWant) \
    { \
        size_t cbNeed = (size_t)(pszStr + cbWant - (char *)pTable) + 5; /* +1 for strtab terminator +4 for end-of-table entry */ \
        if (cbNeed > cbMax) \
        { \
            if (fHideErrors) \
            { \
                LogRel(("One of the DMI strings is too long -- using default DMI data!\n")); \
                continue; \
            } \
            return PDMDevHlpVMSetError(pDevIns, VERR_TOO_MUCH_DATA, RT_SRC_POS, \
                                       N_("One of the DMI strings is too long. Check all bios/Dmi* configuration entries. At least %zu bytes are needed but there is no space for more than %d bytes"), cbNeed, cbMax); \
        } \
    }

#define READCFGSTRDEF(variable, name, default_value) \
    { \
        if (fForceDefault) \
            pszTmp = default_value; \
        else \
        { \
            rc = CFGMR3QueryStringDef(pCfgHandle, name, szBuf, sizeof(szBuf), default_value); \
            if (RT_FAILURE(rc)) \
            { \
                if (fHideErrors) \
                { \
                    LogRel(("Configuration error: Querying \"" name "\" as a string failed -- using default DMI data!\n")); \
                    continue; \
                } \
                return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS, \
                                           N_("Configuration error: Querying \"" name "\" as a string failed")); \
            } \
            else if (!strcmp(szBuf, "<EMPTY>")) \
                pszTmp = ""; \
            else \
                pszTmp = szBuf; \
        } \
        if (!pszTmp[0]) \
            variable = 0; /* empty string */ \
        else \
        { \
            variable = iStrNr++; \
            size_t cStr = strlen(pszTmp) + 1; \
            CHECKSIZE(cStr); \
            memcpy(pszStr, pszTmp, cStr); \
            pszStr += cStr ; \
        } \
    }

#define READCFGSTR(variable, name) \
    READCFGSTRDEF(variable, # name, s_szDef ## name)

#define READCFGINT(variable, name) \
    { \
        if (fForceDefault) \
            variable = s_iDef ## name; \
        else \
        { \
            rc = CFGMR3QueryS32Def(pCfgHandle, # name, & variable, s_iDef ## name); \
            if (RT_FAILURE(rc)) \
            { \
                if (fHideErrors) \
                { \
                    LogRel(("Configuration error: Querying \"" # name "\" as an int failed -- using default DMI data!\n")); \
                    continue; \
                } \
                return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS, \
                                           N_("Configuration error: Querying \"" # name "\" as an int failed")); \
            } \
        } \
    }

    bool fForceDefault = false;
#ifdef VBOX_BIOS_DMI_FALLBACK
    /*
     * There will be two passes. If an error occurs during the first pass, a
     * message will be written to the release log and we fall back to default
     * DMI data and start a second pass.
     */
    bool fHideErrors = true;
#else
    /*
     * There will be one pass, every error is fatal and will prevent the VM
     * from starting.
     */
    bool fHideErrors = false;
#endif

    for  (;; fForceDefault = true, fHideErrors = false)
    {
        int  iStrNr;
        int  rc;
        char szBuf[256];
        char *pszStr = (char *)pTable;
        char szDmiSystemUuid[64];
        char *pszDmiSystemUuid;
        const char *pszTmp;

        if (fForceDefault)
            pszDmiSystemUuid = NULL;
        else
        {
            rc = CFGMR3QueryString(pCfgHandle, "DmiSystemUuid", szDmiSystemUuid, sizeof(szDmiSystemUuid));
            if (rc == VERR_CFGM_VALUE_NOT_FOUND)
                pszDmiSystemUuid = NULL;
            else if (RT_FAILURE(rc))
            {
                if (fHideErrors)
                {
                    LogRel(("Configuration error: Querying \"DmiSystemUuid\" as a string failed, using default DMI data\n"));
                    continue;
                }
                return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS,
                                           N_("Configuration error: Querying \"DmiSystemUuid\" as a string failed"));
            }
            else
                pszDmiSystemUuid = szDmiSystemUuid;
        }

        /*********************************
         * DMI BIOS information (Type 0) *
         *********************************/
        PDMIBIOSINF pBIOSInf         = (PDMIBIOSINF)pszStr;
        CHECKSIZE(sizeof(*pBIOSInf));

        pszStr                       = (char *)&pBIOSInf->u8ReleaseMajor;
        pBIOSInf->header.u8Length    = RT_OFFSETOF(DMIBIOSINF, u8ReleaseMajor);

        /* don't set these fields by default for legacy compatibility */
        int iDmiBIOSReleaseMajor, iDmiBIOSReleaseMinor;
        READCFGINT(iDmiBIOSReleaseMajor, DmiBIOSReleaseMajor);
        READCFGINT(iDmiBIOSReleaseMinor, DmiBIOSReleaseMinor);
        if (iDmiBIOSReleaseMajor != 0 || iDmiBIOSReleaseMinor != 0)
        {
            pszStr = (char *)&pBIOSInf->u8FirmwareMajor;
            pBIOSInf->header.u8Length = RT_OFFSETOF(DMIBIOSINF, u8FirmwareMajor);
            pBIOSInf->u8ReleaseMajor  = iDmiBIOSReleaseMajor;
            pBIOSInf->u8ReleaseMinor  = iDmiBIOSReleaseMinor;

            int iDmiBIOSFirmwareMajor, iDmiBIOSFirmwareMinor;
            READCFGINT(iDmiBIOSFirmwareMajor, DmiBIOSFirmwareMajor);
            READCFGINT(iDmiBIOSFirmwareMinor, DmiBIOSFirmwareMinor);
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
        READCFGSTR(pBIOSInf->u8Vendor,  DmiBIOSVendor);
        READCFGSTR(pBIOSInf->u8Version, DmiBIOSVersion);
        pBIOSInf->u16Start           = 0xE000;
        READCFGSTR(pBIOSInf->u8Release, DmiBIOSReleaseDate);
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

        /***********************************
         * DMI system information (Type 1) *
         ***********************************/
        PDMISYSTEMINF pSystemInf     = (PDMISYSTEMINF)pszStr;
        CHECKSIZE(sizeof(*pSystemInf));
        pszStr                       = (char *)(pSystemInf + 1);
        iStrNr                       = 1;
        pSystemInf->header.u8Type    = 1; /* System Information */
        pSystemInf->header.u8Length  = sizeof(*pSystemInf);
        pSystemInf->header.u16Handle = 0x0001;
        READCFGSTR(pSystemInf->u8Manufacturer, DmiSystemVendor);
        READCFGSTR(pSystemInf->u8ProductName,  DmiSystemProduct);
        READCFGSTR(pSystemInf->u8Version,      DmiSystemVersion);
        READCFGSTR(pSystemInf->u8SerialNumber, DmiSystemSerial);

        RTUUID uuid;
        if (pszDmiSystemUuid)
        {
            rc = RTUuidFromStr(&uuid, pszDmiSystemUuid);
            if (RT_FAILURE(rc))
            {
                if (fHideErrors)
                {
                    LogRel(("Configuration error: Invalid UUID for DMI tables specified, using default DMI data\n"));
                    continue;
                }
                return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS,
                                           N_("Configuration error: Invalid UUID for DMI tables specified"));
            }
            uuid.Gen.u32TimeLow = RT_H2BE_U32(uuid.Gen.u32TimeLow);
            uuid.Gen.u16TimeMid = RT_H2BE_U16(uuid.Gen.u16TimeMid);
            uuid.Gen.u16TimeHiAndVersion = RT_H2BE_U16(uuid.Gen.u16TimeHiAndVersion);
            pUuid = &uuid;
        }
        memcpy(pSystemInf->au8Uuid, pUuid, sizeof(RTUUID));

        pSystemInf->u8WakeupType     = 6; /* Power Switch */
        pSystemInf->u8SKUNumber      = 0;
        READCFGSTR(pSystemInf->u8Family, DmiSystemFamily);
        *pszStr++                    = '\0';

        /********************************************
         * DMI System Enclosure or Chassis (Type 3) *
         ********************************************/
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
        READCFGSTR(pChassis->u8Manufacturer, DmiChassisVendor);
        pChassis->u8Type             = 0x01; /* ''other'', no chassis lock present */
        READCFGSTR(pChassis->u8Version, DmiChassisVersion);
        READCFGSTR(pChassis->u8SerialNumber, DmiChassisSerial);
        READCFGSTR(pChassis->u8AssetTag, DmiChassisAssetTag);
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

        /*****************************
         * DMI OEM strings (Type 11) *
         *****************************/
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
        READCFGSTRDEF(pOEMStrings->u8VBoxVersion, "DmiOEMVBoxVer", szTmp);
        RTStrPrintf(szTmp, sizeof(szTmp), "vboxRev_%u", RTBldCfgRevision());
        READCFGSTRDEF(pOEMStrings->u8VBoxRevision, "DmiOEMVBoxRev", szTmp);
        *pszStr++                    = '\0';

        /* End-of-table marker - includes padding to account for fixed table size. */
        PDMIHDR pEndOfTable          = (PDMIHDR)pszStr;
        pEndOfTable->u8Type          = 0x7f;
        pEndOfTable->u8Length        = cbMax - ((char *)pszStr - (char *)pTable) - 2;
        pEndOfTable->u16Handle       = 0xFEFF;

        /* If more fields are added here, fix the size check in READCFGSTR */

        /* Success! */
        break;
    }

#undef READCFGSTR
#undef READCFGINT
#undef CHECKSIZE

    if (fPutSmbiosHeaders)
    {
        struct {
            struct SMBIOSHDR     smbios;
            struct DMIMAINHDR    dmi;
        } aBiosHeaders
        =
        {
            // The SMBIOS header
            {
                { 0x5f, 0x53, 0x4d, 0x5f},         // "_SM_" signature
                0x00,                              // checksum
                0x1f,                              // EPS length, defined by standard
                VBOX_SMBIOS_MAJOR_VER,             // SMBIOS major version
                VBOX_SMBIOS_MINOR_VER,             // SMBIOS minor version
                VBOX_SMBIOS_MAXSS,                 // Maximum structure size
                0x00,                              // Entry point revision
                { 0x00, 0x00, 0x00, 0x00, 0x00 }   // padding
            },
            // The DMI header
            {
                { 0x5f, 0x44, 0x4d, 0x49, 0x5f },  // "_DMI_" signature
                0x00,                              // checksum
                VBOX_DMI_TABLE_SIZE,               // DMI tables length
                VBOX_DMI_TABLE_BASE,               // DMI tables base
                VBOX_DMI_TABLE_ENTR,               // DMI tables entries
                VBOX_DMI_TABLE_VER,                // DMI version
            }
        };

        aBiosHeaders.smbios.u8Checksum = fwCommonChecksum((uint8_t*)&aBiosHeaders.smbios, sizeof(aBiosHeaders.smbios));
        aBiosHeaders.dmi.u8Checksum = fwCommonChecksum((uint8_t*)&aBiosHeaders.dmi, sizeof(aBiosHeaders.dmi));

        PDMDevHlpPhysWrite (pDevIns, 0xfe300, &aBiosHeaders, sizeof(aBiosHeaders));
    }

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
void FwCommonPlantMpsTable(PPDMDEVINS pDevIns, uint8_t *pTable, uint16_t cCpus)
{
    /* configuration table */
    PMPSCFGTBLHEADER pCfgTab      = (MPSCFGTBLHEADER*)pTable;
    memcpy(pCfgTab->au8Signature, "PCMP", 4);
    pCfgTab->u8SpecRev             =  4;    /* 1.4 */
    memcpy(pCfgTab->au8OemId, "VBOXCPU ", 8);
    memcpy(pCfgTab->au8ProductId, "VirtualBox  ", 12);
    pCfgTab->u32OemTablePtr        =  0;
    pCfgTab->u16OemTableSize       =  0;
    pCfgTab->u16EntryCount         =  cCpus /* Processors */
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
    for (int i = 0; i < cCpus; i++)
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
    PMPSBUSENTRY pBusEntry         = (PMPSBUSENTRY)pProcEntry;
    pBusEntry->u8EntryType         = 1; /* bus entry */
    pBusEntry->u8BusId             = 0; /* this ID is referenced by the interrupt entries */
    memcpy(pBusEntry->au8BusTypeStr, "ISA   ", 6);

    /* PCI bus? */

    /* I/O-APIC.
     * MP spec: "The configuration table contains one or more entries for I/O APICs.
     *           ... At least one I/O APIC must be enabled." */
    PMPSIOAPICENTRY pIOAPICEntry   = (PMPSIOAPICENTRY)(pBusEntry+1);
    uint16_t apicId = cCpus;
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
    pCfgTab->u8Checksum            = fwCommonChecksum(pTable, pCfgTab->u16Length);

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
    floatPtr.u8Checksum            = fwCommonChecksum((uint8_t*)&floatPtr, 16);
    PDMDevHlpPhysWrite (pDevIns, 0x9fff0, &floatPtr, 16);
}
