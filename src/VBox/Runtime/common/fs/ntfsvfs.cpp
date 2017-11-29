/* $Id$ */
/** @file
 * IPRT - NTFS Virtual Filesystem, currently only for reading allocation bitmap.
 */

/*
 * Copyright (C) 2012-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP RTLOGGROUP_FS
#include <iprt/fsvfs.h>

#include <iprt/asm.h>
#include <iprt/avl.h>
#include <iprt/assert.h>
#include <iprt/file.h>
#include <iprt/log.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/vfs.h>
#include <iprt/vfslowlevel.h>
#include <iprt/formats/ntfs.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** Max clusters in an allocation run.
 * This ASSUMES that the cluster size is at most 64KB. */
#define RTFSNTFS_MAX_CLUSTER_IN_RUN     UINT64_C(0x00007fffffffffff)


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/** Pointer to the instance data for a NTFS volume. */
typedef struct RTFSNTFSVOL *PRTFSNTFSVOL;
/** Pointer to a NTFS MFT record. */
typedef struct RTFSNTFSMFTREC *PRTFSNTFSMFTREC;
/** Poitner to a NTFS core object record.   */
typedef struct RTFSNTFSCORE *PRTFSNTFSCORE;


/**
 * NTFS disk allocation extent (internal representation).
 */
typedef struct RTFSNTFSEXTENT
{
    /** The disk or partition byte offset.
     * This is set to UINT64_MAX for parts of sparse files that aren't recorded. */
    uint64_t            off;
    /** The size of the extent in bytes. */
    uint64_t            cbExtent;
} RTFSNTFSEXTENT;
/** Pointer to an NTFS 9660 extent. */
typedef RTFSNTFSEXTENT *PRTFSNTFSEXTENT;
/** Pointer to a const NTFS 9660 extent. */
typedef RTFSNTFSEXTENT const *PCRTFSNTFSEXTENT;

/**
 * An array of zero or more extents.
 */
typedef struct RTFSNTFSEXTENTS
{
    /** Number of bytes covered by the extents. */
    uint64_t            cbData;
    /** Number of allocation extents. */
    uint32_t            cExtents;
    /** Array of allocation extents. */
    PRTFSNTFSEXTENT     paExtents;
} RTFSNTFSEXTENTS;
/** Pointer to an extent array. */
typedef RTFSNTFSEXTENTS *PRTFSNTFSEXTENTS;
/** Pointer to a const extent array. */
typedef RTFSNTFSEXTENTS const *PCRTFSNTFSEXTENTS;


/**
 * NTFS MFT record.
 *
 * These are kept in a tree to , so
 */
typedef struct RTFSNTFSMFTREC
{
    /** MFT record number (index) as key. */
    AVLU64NODECORE      TreeNode;
    /** Pointer to the next MFT record if chained.  Holds a reference.  */
    PRTFSNTFSMFTREC     pNext;
    union
    {
        /** Generic record pointer.  RTFSNTFSVOL::cbMftRecord in size. */
        uint8_t        *pbRec;
        /** Pointer to the file record. */
        PNTFSRECFILE    pFileRec;
    } RT_UNION_NM(u);
    /** Pointer to the core object with the parsed data.
     * This is a weak reference.  Non-base MFT record all point to the base one. */
    PRTFSNTFSCORE       pCore;
    /** Reference counter. */
    uint32_t volatile   cRefs;
    /** Set if this is a base MFT record. */
    bool                fIsBase;
    /** The disk offset of this MFT entry. */
    uint64_t            offDisk;
} RTFSNTFSMFTREC;


/** Pointer to a attribute subrecord structure. */
typedef struct RTFSNTFSATTRSUBREC *PRTFSNTFSATTRSUBREC;

/**
 * An attribute subrecord.
 *
 * This is for covering non-resident attributes that have had their allocation
 * list split.
 */
typedef struct RTFSNTFSATTRSUBREC
{
    /** Pointer to the next one. */
    PRTFSNTFSATTRSUBREC pNext;
    /** Pointer to the attribute header.
     * The MFT is held down by RTFSNTFSCORE via pMftEntry. */
    PNTFSATTRIBHDR      pAttrHdr;
    /** Disk space allocation if non-resident. */
    RTFSNTFSEXTENTS     Extents;
} RTFSNTFSATTRSUBREC;

/**
 * An attribute.
 */
typedef struct RTFSNTFSATTR
{
    /** List entry (head RTFSNTFSCORE::AttribHead). */
    RTLISTNODE          ListEntry;
    /** Pointer to the core object this attribute belongs to. */
    PRTFSNTFSCORE       pCore;
    /** Pointer to the attribute header.
     * The MFT is held down by RTFSNTFSCORE via pMftEntry. */
    PNTFSATTRIBHDR      pAttrHdr;
    /** Disk space allocation if non-resident. */
    RTFSNTFSEXTENTS     Extents;
    /** Pointer to any subrecords containing further allocation extents. */
    PRTFSNTFSATTRSUBREC pSubRecHead;
} RTFSNTFSATTR;
/** Pointer to a attribute structure. */
typedef RTFSNTFSATTR *PRTFSNTFSATTR;


/**
 * NTFS file system object, shared part.
 */
typedef struct RTFSNTFSCORE
{
    /** Reference counter.   */
    uint32_t volatile   cRefs;
    /** Pointer to the volume. */
    PRTFSNTFSVOL        pVol;
    /** Pointer to the head of the MFT record chain for this object.
     * Holds a reference.  */
    PRTFSNTFSMFTREC     pMftRec;
    /** List of attributes (RTFSNTFSATTR). */
    RTLISTANCHOR        AttribHead;
} RTFSNTFSCORE;


/**
 * Instance data for an NTFS volume.
 */
typedef struct RTFSNTFSVOL
{
    /** Handle to itself. */
    RTVFS           hVfsSelf;
    /** The file, partition, or whatever backing the NTFS volume. */
    RTVFSFILE       hVfsBacking;
    /** The size of the backing thingy. */
    uint64_t        cbBacking;
    /** The formatted size of the volume. */
    uint64_t        cbVolume;
    /** cbVolume expressed as a cluster count. */
    uint64_t        cClusters;

    /** RTVFSMNT_F_XXX. */
    uint32_t        fMntFlags;
    /** RTFSNTVFS_F_XXX (currently none defined). */
    uint32_t        fNtfsFlags;

    /** The (logical) cluster size. */
    uint32_t        cbCluster;
    /** The (logical) sector size. */
    uint32_t        cbSector;

    /** The shift count for converting between bytes and clusters. */
    uint8_t         cClusterShift;
    /** Explicit padding. */
    uint8_t         abReserved[7];

    /** The logical cluster number of the MFT. */
    uint64_t        uLcnMft;
    /** The logical cluster number of the mirror MFT. */
    uint64_t        uLcnMftMirror;

    /** The MFT record size. */
    uint32_t        cbMftRecord;
    /** The index record size. */
    uint32_t        cbIndexRecord;

    /** The volume serial number. */
    uint64_t        uSerialNo;

    /** The MFT data attribute. */
    PRTFSNTFSATTR   pMftData;

    /** Root of the MFT record tree (RTFSNTFSMFTREC). */
    AVLU64TREE      MftRoot;
} RTFSNTFSVOL;


static PRTFSNTFSMFTREC rtFsNtfsMftVol_New(PRTFSNTFSVOL pVol, uint64_t idMft)
{
    PRTFSNTFSMFTREC pRec = (PRTFSNTFSMFTREC)RTMemAllocZ(sizeof(*pRec));
    if (pRec)
    {
        pRec->pbRec = (uint8_t *)RTMemAllocZ(pVol->cbMftRecord);
        if (pRec->pbRec)
        {
            pRec->TreeNode.Key = idMft;
            pRec->pNext        = NULL;
            pRec->offDisk      = UINT64_MAX;
            pRec->cRefs        = 1;
            if (RTAvlU64Insert(&pVol->MftRoot, &pRec->TreeNode))
                return pRec;
            RTMemFree(pRec);
        }
    }
    return NULL;
}


static uint32_t rtFsNtfsMftRec_Destroy(PRTFSNTFSMFTREC pThis, PRTFSNTFSVOL pVol)
{
    RTMemFree(pThis->pbRec);
    pThis->pbRec = NULL;

    PAVLU64NODECORE pRemoved = RTAvlU64Remove(&pVol->MftRoot, pThis->TreeNode.Key);
    Assert(pRemoved == &pThis->TreeNode); NOREF(pRemoved);

    RTMemFree(pThis);

    return 0;
}


static uint32_t rtFsNtfsMftRec_Retain(PRTFSNTFSMFTREC pThis)
{
    uint32_t cRefs = ASMAtomicIncU32(&pThis->cRefs);
    Assert(cRefs < 64);
    return cRefs;
}

static uint32_t rtFsNtfsMftRec_Release(PRTFSNTFSMFTREC pThis, PRTFSNTFSVOL pVol)
{
    uint32_t cRefs = ASMAtomicDecU32(&pThis->cRefs);
    Assert(cRefs < 64);
    if (cRefs != 0)
        return cRefs;
    return rtFsNtfsMftRec_Destroy(pThis, pVol);
}


#ifdef LOG_ENABLED
/**
 * Logs the MFT record
 *
 * @param   pRec            The MFT record to log.
 * @param   cbMftRecord     MFT record size (from RTFSNTFSVOL).
 */
static void rtfsNtfsMftRec_Log(PRTFSNTFSMFTREC pRec, uint32_t cbMftRecord)
{
    if (LogIs2Enabled())
    {
        PCNTFSRECFILE  pFileRec = pRec->pFileRec;
        Log2(("NTFS: MFT #%#RX64 at %#RX64\n", pRec->TreeNode.Key, pRec->offDisk));
        if (pFileRec->Hdr.uMagic == NTFSREC_MAGIC_FILE)
        {
            size_t const          cbRec = cbMftRecord;
            uint8_t const * const pbRec = pRec->pbRec;

            Log2(("NTFS: FILE record: \n"));
            Log2(("NTFS:   UpdateSeqArray  %#x L %#x\n", RT_LE2H_U16(pFileRec->Hdr.offUpdateSeqArray), RT_LE2H_U16(pFileRec->Hdr.cUpdateSeqEntries) ));
            Log2(("NTFS:   uLsn            %#RX64\n", RT_LE2H_U64(pFileRec->uLsn)));
            Log2(("NTFS:   uRecReuseSeqNo  %#RX16\n", RT_LE2H_U16(pFileRec->uRecReuseSeqNo)));
            Log2(("NTFS:   cLinks          %#RX16\n", RT_LE2H_U16(pFileRec->cLinks)));
            Log2(("NTFS:   offFirstAttrib  %#RX16\n", RT_LE2H_U16(pFileRec->offFirstAttrib)));
            Log2(("NTFS:   fFlags          %#RX16%s%s\n", RT_LE2H_U16(pFileRec->fFlags),
                  RT_LE2H_U16(pFileRec->fFlags) & NTFSRECFILE_F_IN_USE    ? " in-use"    : "",
                  RT_LE2H_U16(pFileRec->fFlags) & NTFSRECFILE_F_DIRECTORY ? " directory" : ""));
            Log2(("NTFS:   cbRecUsed       %#RX32\n", RT_LE2H_U32(pFileRec->cbRecUsed)));
            Log2(("NTFS:   BaseMftRec      %#RX64, sqn %#x\n",
                  NTFSMFTREF_GET_IDX(&pFileRec->BaseMftRec), NTFSMFTREF_GET_SEQ(&pFileRec->BaseMftRec)));
            Log2(("NTFS:   idNextAttrib    %#RX16\n", RT_LE2H_U16(pFileRec->idNextAttrib)));
            if (   RT_LE2H_U16(pFileRec->offFirstAttrib)         >= sizeof(*pFileRec)
                && (   RT_LE2H_U16(pFileRec->Hdr.offUpdateSeqArray) >= sizeof(*pFileRec)
                    || pFileRec->Hdr.offUpdateSeqArray == 0))
            {
                Log2(("NTFS:   uPaddingOrUsa   %#RX16\n", pFileRec->uPaddingOrUsa));
                Log2(("NTFS:   idxMftSelf      %#RX32\n", RT_LE2H_U32(pFileRec->idxMftSelf)));
            }

            uint32_t offRec = pFileRec->offFirstAttrib;
            size_t   cbRecUsed = RT_MIN(cbRec, pFileRec->cbRecUsed);
            while (offRec + NTFSATTRIBHDR_SIZE_RESIDENT <= cbRecUsed)
            {
                PCNTFSATTRIBHDR pHdr     = (PCNTFSATTRIBHDR)&pbRec[offRec];
                uint32_t const  cbAttrib = RT_LE2H_U32(pHdr->cbAttrib);
                Log2(("NTFS:   @%#05x: Attrib record: %#x LB %#x, instance #%#x, fFlags=%#RX16, %s\n", offRec,
                      RT_LE2H_U32(pHdr->uAttrType), cbAttrib, RT_LE2H_U16(pHdr->idAttrib), RT_LE2H_U16(pHdr->fFlags),
                      pHdr->fNonResident == 0 ? "resident" : pHdr->fNonResident == 1 ? "non-resident" : "bad-resident-flag"));
                if (pHdr->offName && pHdr->cwcName)
                {
                    if (offRec + RT_LE2H_U16(pHdr->offName) + pHdr->cwcName * sizeof(RTUTF16) <= cbRec)
                        Log2(("NTFS:     Name %.*ls\n", pHdr->cwcName,&pbRec[offRec + RT_LE2H_U16(pHdr->offName)]));
                    else
                        Log2(("NTFS:     Name <!out of bounds!> %#x L %#x\n", RT_LE2H_U16(pHdr->offName), pHdr->cwcName));
                }
                switch (pHdr->uAttrType)
                {
                    case NTFS_AT_UNUSED:                    Log2(("NTFS:     Type: UNUSED\n")); break;
                    case NTFS_AT_STANDARD_INFORMATION:      Log2(("NTFS:     Type: STANDARD_INFORMATION\n")); break;
                    case NTFS_AT_ATTRIBUTE_LIST:            Log2(("NTFS:     Type: ATTRIBUTE_LIST\n")); break;
                    case NTFS_AT_FILENAME:                  Log2(("NTFS:     Type: FILENAME\n")); break;
                    case NTFS_AT_OBJECT_ID:                 Log2(("NTFS:     Type: OBJECT_ID\n")); break;
                    case NTFS_AT_SECURITY_DESCRIPTOR:       Log2(("NTFS:     Type: SECURITY_DESCRIPTOR\n")); break;
                    case NTFS_AT_VOLUME_NAME:               Log2(("NTFS:     Type: VOLUME_NAME\n")); break;
                    case NTFS_AT_VOLUME_INFORMATION:        Log2(("NTFS:     Type: VOLUME_INFORMATION\n")); break;
                    case NTFS_AT_DATA:                      Log2(("NTFS:     Type: DATA\n")); break;
                    case NTFS_AT_INDEX_ROOT:                Log2(("NTFS:     Type: INDEX_ROOT\n")); break;
                    case NTFS_AT_INDEX_ALLOCATION:          Log2(("NTFS:     Type: INDEX_ALLOCATION\n")); break;
                    case NTFS_AT_BITMAP:                    Log2(("NTFS:     Type: BITMAP\n")); break;
                    case NTFS_AT_REPARSE_POINT:             Log2(("NTFS:     Type: REPARSE_POINT\n")); break;
                    case NTFS_AT_EA_INFORMATION:            Log2(("NTFS:     Type: EA_INFORMATION\n")); break;
                    case NTFS_AT_EA:                        Log2(("NTFS:     Type: EA\n")); break;
                    case NTFS_AT_PROPERTY_SET:              Log2(("NTFS:     Type: PROPERTY_SET\n")); break;
                    case NTFS_AT_LOGGED_UTILITY_STREAM:     Log2(("NTFS:     Type: LOGGED_UTILITY_STREAM\n")); break;
                    default:
                        if (RT_LE2H_U32(pHdr->uAttrType) >= RT_LE2H_U32_C(NTFS_AT_FIRST_USER_DEFINED))
                            Log2(("NTFS:     Type: unknown user defined - %#x!\n", RT_LE2H_U32(pHdr->uAttrType)));
                        else
                            Log2(("NTFS:     Type: unknown - %#x!\n", RT_LE2H_U32(pHdr->uAttrType)));
                        break;
                }

                size_t const cbMaxAttrib = cbRec - offRec;
                if (!pHdr->fNonResident)
                {
                    uint16_t const offValue = RT_LE2H_U16(pHdr->u.Res.offValue);
                    uint32_t const cbValue  = RT_LE2H_U32(pHdr->u.Res.cbValue);
                    Log2(("NTFS:     Value: %#x LB %#x, fFlags=%#x bReserved=%#x\n",
                          offValue, cbValue, pHdr->u.Res.fFlags, pHdr->u.Res.bReserved));
                    if (   offValue < cbMaxAttrib
                        && cbValue  < cbMaxAttrib
                        && offValue + cbValue <= cbMaxAttrib)
                    {
                        uint8_t const *pbValue = &pbRec[offRec + offValue];
                        RTTIMESPEC     Spec;
                        char           sz[80];
                        switch (pHdr->uAttrType)
                        {
                            case NTFS_AT_STANDARD_INFORMATION:
                            {
                                PCNTFSATSTDINFO pInfo = (PCNTFSATSTDINFO)pbValue;
                                if (cbValue >= NTFSATSTDINFO_SIZE_NTFS_V12)
                                {
                                    Log2(("NTFS:     iCreationTime      %#RX64 %s\n", RT_LE2H_U64(pInfo->iCreationTime),
                                          RTTimeSpecToString(RTTimeSpecSetNtTime(&Spec, RT_LE2H_U64(pInfo->iCreationTime)), sz, sizeof(sz)) ));
                                    Log2(("NTFS:     iLastDataModTime   %#RX64 %s\n", RT_LE2H_U64(pInfo->iLastDataModTime),
                                          RTTimeSpecToString(RTTimeSpecSetNtTime(&Spec, RT_LE2H_U64(pInfo->iLastDataModTime)), sz, sizeof(sz)) ));
                                    Log2(("NTFS:     iLastMftModTime    %#RX64 %s\n", RT_LE2H_U64(pInfo->iLastMftModTime),
                                          RTTimeSpecToString(RTTimeSpecSetNtTime(&Spec, RT_LE2H_U64(pInfo->iLastMftModTime)), sz, sizeof(sz)) ));
                                    Log2(("NTFS:     iLastAccessTime    %#RX64 %s\n", RT_LE2H_U64(pInfo->iLastAccessTime),
                                          RTTimeSpecToString(RTTimeSpecSetNtTime(&Spec, RT_LE2H_U64(pInfo->iLastAccessTime)), sz, sizeof(sz)) ));
                                    Log2(("NTFS:     fFileAttribs       %#RX32\n", RT_LE2H_U32(pInfo->fFileAttribs) ));
                                    Log2(("NTFS:     cMaxFileVersions   %#RX32\n", RT_LE2H_U32(pInfo->cMaxFileVersions) ));
                                    Log2(("NTFS:     uFileVersion       %#RX32\n", RT_LE2H_U32(pInfo->uFileVersion) ));
                                }
                                else
                                    Log2(("NTFS:     Error! cbValue=%#x is smaller than expected (%#x) for NTFSATSTDINFO!\n",
                                          cbValue, NTFSATSTDINFO_SIZE_NTFS_V12));
                                if (cbValue >= sizeof(*pInfo))
                                {
                                    Log2(("NTFS:     idClass            %#RX32\n", RT_LE2H_U32(pInfo->idClass) ));
                                    Log2(("NTFS:     idOwner            %#RX32\n", RT_LE2H_U32(pInfo->idOwner) ));
                                    Log2(("NTFS:     idSecurity         %#RX32\n", RT_LE2H_U32(pInfo->idSecurity) ));
                                    Log2(("NTFS:     cbQuotaChared      %#RX64\n", RT_LE2H_U64(pInfo->cbQuotaChared) ));
                                    Log2(("NTFS:     idxUpdateSequence  %#RX64\n", RT_LE2H_U64(pInfo->idxUpdateSequence) ));
                                }
                                if (cbValue > sizeof(*pInfo))
                                    Log2(("NTFS:     Undefined data: %.*Rhxs\n", cbValue - sizeof(*pInfo), &pbValue[sizeof(*pInfo)]));
                                break;
                            }

                            case NTFS_AT_ATTRIBUTE_LIST:
                            {
                                uint32_t iEntry   = 0;
                                uint32_t offEntry = 0;
                                while (offEntry + NTFSATLISTENTRY_SIZE_MINIMAL < cbValue)
                                {
                                    PCNTFSATLISTENTRY pInfo = (PCNTFSATLISTENTRY)&pbValue[offEntry];
                                    Log2(("NTFS:     attr[%u]: %#x in %#RX64 (sqn %#x), instance %#x, VNC=%#RX64-, name %#x L %#x\n",
                                          iEntry, RT_LE2H_U32(pInfo->uAttrType),  NTFSMFTREF_GET_IDX(&pInfo->InMftRec),
                                          NTFSMFTREF_GET_SEQ(&pInfo->InMftRec), RT_LE2H_U16(pInfo->idAttrib),
                                          RT_LE2H_U64(pInfo->iVcnFirst), pInfo->offName, pInfo->cwcName));
                                    if (   pInfo->cwcName > 0
                                        && pInfo->offName < pInfo->cbEntry)
                                        Log2(("NTFS:               name '%.*ls'\n", pInfo->cwcName, (uint8_t *)pInfo + pInfo->offName));

                                    /* next */
                                    if (pInfo->cbEntry < NTFSATLISTENTRY_SIZE_MINIMAL)
                                    {
                                        Log2(("NTFS:     cbEntry is too small! cbEntry=%#x, min %#x\n",
                                              pInfo->cbEntry, NTFSATLISTENTRY_SIZE_MINIMAL));
                                        break;
                                    }
                                    iEntry++;
                                    offEntry += RT_ALIGN_32(pInfo->cbEntry, 8);
                                }
                                break;
                            }

                            case NTFS_AT_FILENAME:
                            {
                                PCNTFSATFILENAME pInfo = (PCNTFSATFILENAME)pbValue;
                                if (cbValue >= RT_UOFFSETOF(NTFSATFILENAME, wszFilename))
                                {
                                    Log2(("NTFS:     ParentDirMftRec    %#RX64, sqn %#x\n",
                                          NTFSMFTREF_GET_IDX(&pInfo->ParentDirMftRec), NTFSMFTREF_GET_SEQ(&pInfo->ParentDirMftRec) ));
                                    Log2(("NTFS:     iCreationTime      %#RX64 %s\n", RT_LE2H_U64(pInfo->iCreationTime),
                                          RTTimeSpecToString(RTTimeSpecSetNtTime(&Spec, RT_LE2H_U64(pInfo->iCreationTime)), sz, sizeof(sz)) ));
                                    Log2(("NTFS:     iLastDataModTime   %#RX64 %s\n", RT_LE2H_U64(pInfo->iLastDataModTime),
                                          RTTimeSpecToString(RTTimeSpecSetNtTime(&Spec, RT_LE2H_U64(pInfo->iLastDataModTime)), sz, sizeof(sz)) ));
                                    Log2(("NTFS:     iLastMftModTime    %#RX64 %s\n", RT_LE2H_U64(pInfo->iLastMftModTime),
                                          RTTimeSpecToString(RTTimeSpecSetNtTime(&Spec, RT_LE2H_U64(pInfo->iLastMftModTime)), sz, sizeof(sz)) ));
                                    Log2(("NTFS:     iLastAccessTime    %#RX64 %s\n", RT_LE2H_U64(pInfo->iLastAccessTime),
                                          RTTimeSpecToString(RTTimeSpecSetNtTime(&Spec, RT_LE2H_U64(pInfo->iLastAccessTime)), sz, sizeof(sz)) ));
                                    Log2(("NTFS:     cbAllocated        %#RX64 (%Rhcb)\n",
                                          RT_LE2H_U64(pInfo->cbAllocated), RT_LE2H_U64(pInfo->cbAllocated)));
                                    Log2(("NTFS:     cbData             %#RX64 (%Rhcb)\n",
                                          RT_LE2H_U64(pInfo->cbData), RT_LE2H_U64(pInfo->cbData)));
                                    Log2(("NTFS:     fFileAttribs       %#RX32\n", RT_LE2H_U32(pInfo->fFileAttribs) ));
                                    if (RT_LE2H_U32(pInfo->fFileAttribs) & NTFS_FA_REPARSE_POINT)
                                        Log2(("NTFS:     uReparseTag        %#RX32\n", RT_LE2H_U32(pInfo->u.uReparseTag) ));
                                    else
                                        Log2(("NTFS:     cbPackedEas        %#RX16\n", RT_LE2H_U16(pInfo->u.cbPackedEas) ));
                                    Log2(("NTFS:     cwcFilename        %#x\n", pInfo->cwcFilename));
                                    Log2(("NTFS:     fFilenameType      %#x\n", pInfo->fFilenameType));
                                    if (cbValue >= RT_UOFFSETOF(NTFSATFILENAME, wszFilename))
                                        Log2(("NTFS:     wszFilename       '%.*ls'\n", pInfo->cwcFilename, pInfo->wszFilename ));
                                    else
                                        Log2(("NTFS:     Error! Truncated filename!!\n"));
                                }
                                else
                                    Log2(("NTFS:     Error! cbValue=%#x is smaller than expected (%#x) for NTFSATFILENAME!\n",
                                          cbValue, RT_OFFSETOF(NTFSATFILENAME, wszFilename) ));
                                break;
                            }

                            //case NTFS_AT_OBJECT_ID:
                            //case NTFS_AT_SECURITY_DESCRIPTOR:
                            //case NTFS_AT_VOLUME_NAME:
                            //case NTFS_AT_VOLUME_INFORMATION:
                            //case NTFS_AT_DATA:
                            //case NTFS_AT_INDEX_ROOT:
                            //case NTFS_AT_INDEX_ALLOCATION:
                            //case NTFS_AT_BITMAP:
                            //case NTFS_AT_REPARSE_POINT:
                            //case NTFS_AT_EA_INFORMATION:
                            //case NTFS_AT_EA:
                            //case NTFS_AT_PROPERTY_SET:
                            //case NTFS_AT_LOGGED_UTILITY_STREAM:

                            default:
                                if (cbValue <= 24)
                                    Log2(("NTFS:     %.*Rhxs\n", cbValue, pbValue));
                                else
                                    Log2(("%.*Rhxd\n", cbValue, pbValue));
                                break;
                        }

                    }
                    else
                        Log2(("NTFS:     !Value is out of bounds!\n"));
                }
                else if (RT_MAX(cbAttrib, NTFSATTRIBHDR_SIZE_NONRES_UNCOMPRESSED) <= cbMaxAttrib)
                {
                    Log2(("NTFS:     VNC range          %#RX64 .. %#RX64 (%#RX64 clusters)\n",
                          RT_LE2H_U64(pHdr->u.NonRes.iVcnFirst), RT_LE2H_U64(pHdr->u.NonRes.iVcnLast),
                          RT_LE2H_U64(pHdr->u.NonRes.iVcnLast) - RT_LE2H_U64(pHdr->u.NonRes.iVcnFirst) + 1));
                    Log2(("NTFS:     cbAllocated        %#RX64 (%Rhcb)\n",
                          RT_LE2H_U64(pHdr->u.NonRes.cbAllocated), RT_LE2H_U64(pHdr->u.NonRes.cbAllocated)));
                    Log2(("NTFS:     cbInitialized      %#RX64 (%Rhcb)\n",
                          RT_LE2H_U64(pHdr->u.NonRes.cbInitialized), RT_LE2H_U64(pHdr->u.NonRes.cbInitialized)));
                    uint16_t const offMappingPairs = RT_LE2H_U16(pHdr->u.NonRes.offMappingPairs);
                    Log2(("NTFS:     offMappingPairs    %#RX16\n", offMappingPairs));
                    if (   pHdr->u.NonRes.abReserved[0] || pHdr->u.NonRes.abReserved[1]
                        || pHdr->u.NonRes.abReserved[2] || pHdr->u.NonRes.abReserved[3] || pHdr->u.NonRes.abReserved[4] )
                        Log2(("NTFS:     abReserved         %.7Rhxs\n", pHdr->u.NonRes.abReserved));
                    if (pHdr->u.NonRes.uCompressionUnit != 0)
                        Log2(("NTFS:     Compression unit   2^%u clusters\n", pHdr->u.NonRes.uCompressionUnit));

                    if (   NTFSATTRIBHDR_SIZE_NONRES_COMPRESSED <= cbMaxAttrib
                        && NTFSATTRIBHDR_SIZE_NONRES_COMPRESSED <= cbAttrib
                        && (   offMappingPairs >= NTFSATTRIBHDR_SIZE_NONRES_COMPRESSED
                            || offMappingPairs <  NTFSATTRIBHDR_SIZE_NONRES_UNCOMPRESSED))
                        Log2(("NTFS:     cbCompressed       %#RX64 (%Rhcb)\n",
                              RT_LE2H_U64(pHdr->u.NonRes.cbCompressed), RT_LE2H_U64(pHdr->u.NonRes.cbCompressed)));
                    else if (pHdr->u.NonRes.uCompressionUnit != 0 && pHdr->u.NonRes.uCompressionUnit != 64)
                        Log2(("NTFS:     !Error! Compressed attrib fields are out of bound!\n"));

                    if (   offMappingPairs < cbAttrib
                        && offMappingPairs >= NTFSATTRIBHDR_SIZE_NONRES_UNCOMPRESSED)
                    {
                        uint8_t const *pbPairs    = &pbRec[offRec + offMappingPairs];
                        uint32_t const cbMaxPairs = cbAttrib - offMappingPairs;
                        int64_t iVnc = pHdr->u.NonRes.iVcnFirst;
                        Log2(("NTFS:     Mapping Pairs: %.*Rhxsd\n", cbMaxPairs, pbPairs));
                        if (!iVnc && !*pbPairs)
                            Log2(("NTFS:         [0]: Empty\n"));
                        else
                        {
                            if (iVnc != 0)
                                Log2(("NTFS:         [0]: VCN=%#012RX64 L %#012RX64 - not mapped\n", 0, iVnc));
                            int64_t  iLnc     = 0;
                            uint32_t iPair    = 0;
                            uint32_t offPairs = 0;
                            while (offPairs < cbMaxPairs)
                            {
                                /* First byte: 4-bit length of each of the pair values */
                                uint8_t const bLengths = pbPairs[offPairs];
                                if (!bLengths)
                                    break;
                                uint8_t const cbRun    = (bLengths & 0x0f) + (bLengths >> 4);
                                if (offPairs + cbRun > cbMaxPairs)
                                {
                                    Log2(("NTFS:         [%d]: run overrun! cbRun=%#x bLengths=%#x offPairs=%#x cbMaxPairs=%#x\n",
                                          iPair, cbRun, bLengths, offPairs, cbMaxPairs));
                                    break;
                                }

                                /* Value 1: Number of (virtual) clusters in this run. */
                                int64_t cClustersInRun;
                                uint8_t cbNum = (bLengths & 0xf);
                                if (cbNum)
                                {
                                    uint8_t const *pbNum = &pbPairs[offPairs + cbNum]; /* last byte */
                                    cClustersInRun = (int8_t)*pbNum--;
                                    while (cbNum-- > 1)
                                        cClustersInRun = (cClustersInRun << 8) + *pbNum--;
                                }
                                else
                                    cClustersInRun = -1;

                                /* Value 2: The logical cluster delta to get to the first cluster in the run. */
                                cbNum = bLengths >> 4;
                                if (cbNum)
                                {
                                    uint8_t const *pbNum = &pbPairs[offPairs + cbNum + (bLengths & 0xf)]; /* last byte */
                                    int64_t cLcnDelta = (int8_t)*pbNum--;
                                    while (cbNum-- > 1)
                                        cLcnDelta = (cLcnDelta << 8) + *pbNum--;
                                    iLnc += cLcnDelta;
                                    Log2(("NTFS:         [%d]: VNC=%#012RX64 L %#012RX64 => LNC=%#012RX64\n",
                                          iPair, iVnc, cClustersInRun, iLnc));
                                }
                                else
                                    Log2(("NTFS:         [%d]: VNC=%#012RX64 L %#012RX64 => HOLE\n",
                                          iPair, iVnc, cClustersInRun));

                                /* Advance. */
                                iVnc     += cClustersInRun;
                                offPairs += 1 + cbRun;
                                iPair++;
                            }
                        }
                    }
                    else if (   cbAttrib != NTFSATTRIBHDR_SIZE_NONRES_COMPRESSED
                             && cbAttrib != NTFSATTRIBHDR_SIZE_NONRES_UNCOMPRESSED)
                    {
                        Log2(("NTFS:     Warning! Odd non-resident attribute size: %#x!\n", cbAttrib));
                        if (cbAttrib >= NTFSATTRIBHDR_SIZE_NONRES_UNCOMPRESSED)
                            Log2(("NTFS:     @%05x: %.*Rhxs!\n", offRec + NTFSATTRIBHDR_SIZE_NONRES_UNCOMPRESSED,
                                  cbAttrib - NTFSATTRIBHDR_SIZE_NONRES_UNCOMPRESSED,
                                  &pbRec[offRec + NTFSATTRIBHDR_SIZE_NONRES_UNCOMPRESSED]));
                    }
                }
                else
                    Log2(("NTFS:     !Attrib header is out of bound!\n"));

                /* Advance. */
                offRec += RT_MAX(cbAttrib, NTFSATTRIBHDR_SIZE_RESIDENT);
            }

            /* Anything left? */
            if (offRec < cbRecUsed)
                Log2(("NTFS:   @%#05x: Tail: %.*Rhxs\n", offRec, cbRecUsed - offRec, &pbRec[offRec]));
        }
        else
            Log2(("NTFS:   Unknown record type: %.4Rhxs\n", pFileRec));
    }
}
#endif /* LOG_ENABLED */


static int rtFsNtfsAttr_ParseExtents(PRTFSNTFSATTR pAttrib, PRTFSNTFSEXTENTS pExtents, uint8_t cClusterShift, int64_t iVcnFirst,
                                     PRTERRINFO pErrInfo, uint64_t idxMft, uint32_t offAttrib)
{
    PCNTFSATTRIBHDR pAttrHdr = pAttrib->pAttrHdr;
    Assert(pAttrHdr->fNonResident);
    Assert(pExtents->cExtents  == 0);
    Assert(pExtents->paExtents == NULL);

    /** @todo Not entirely sure how to best detect empty mapping pair program.
     *        Not sure if this is a real problem as zero length stuff can be
     *        resident.  */
    uint16_t const offMappingPairs = RT_LE2H_U16(pAttrHdr->u.NonRes.offMappingPairs);
    uint32_t const cbAttrib        = RT_LE2H_U32(pAttrHdr->cbAttrib);
    if (   offMappingPairs != cbAttrib
        && offMappingPairs != 0)
    {
        if (pAttrHdr->u.NonRes.iVcnFirst < iVcnFirst)
            return RTERRINFO_LOG_REL_SET_F(pErrInfo, VERR_VFS_BOGUS_FORMAT,
                                           "Bad MFT record %#RX64: Attribute (@%#x) has a lower starting VNC than expected: %#RX64, %#RX64",
                                           idxMft, offAttrib, pAttrHdr->u.NonRes.iVcnFirst, iVcnFirst);

        if (   offMappingPairs >= cbAttrib
            || offMappingPairs < NTFSATTRIBHDR_SIZE_NONRES_UNCOMPRESSED)
            return RTERRINFO_LOG_REL_SET_F(pErrInfo, VERR_VFS_BOGUS_FORMAT,
                                           "Bad MFT record %#RX64: Mapping pair program for attribute (@%#x) is out of bounds: %#x, cbAttrib=%#x",
                                           idxMft, offAttrib, offMappingPairs, cbAttrib);

        /*
         * Count the pairs.
         */
        uint8_t const  * const  pbPairs  = (const uint8_t *)pAttrHdr + pAttrHdr->u.NonRes.offMappingPairs;
        uint32_t const          cbPairs  = cbAttrib - offMappingPairs;
        uint32_t                offPairs = 0;
        uint32_t                cPairs   = 0;
        while (offPairs < cbPairs)
        {
            uint8_t const bLengths = pbPairs[offPairs];
            if (bLengths)
            {
                uint8_t const cbRunField = bLengths & 0x0f;
                uint8_t const cbLcnField = bLengths >> 4;
                if (   cbRunField > 0
                    && cbRunField <= 8)
                {
                    if (cbLcnField <= 8)
                    {
                        cPairs++;

                        /* Advance and check for overflow/end. */
                        offPairs += 1 + cbRunField + cbLcnField;
                        if (offPairs <= cbAttrib)
                            continue;
                        return RTERRINFO_LOG_REL_SET_F(pErrInfo, VERR_VFS_BOGUS_FORMAT,
                                                       "Bad MFT record %#RX64: Mapping pair #%#x for attribute (@%#x) is out of bounds",
                                                       idxMft, cPairs - 1, offAttrib);
                    }
                    return RTERRINFO_LOG_REL_SET_F(pErrInfo, VERR_VFS_BOGUS_FORMAT,
                                                   "Bad MFT record %#RX64: Mapping pair #%#x for attribute (@%#x): cbLcnField is out of bound: %u",
                                                   idxMft, cPairs - 1, offAttrib, cbLcnField);

                }
                return RTERRINFO_LOG_REL_SET_F(pErrInfo, VERR_VFS_BOGUS_FORMAT,
                                               "Bad MFT record %#RX64: Mapping pair #%#x for attribute (@%#x): cbRunField is out of bound: %u",
                                               idxMft, cPairs - 1, offAttrib, cbRunField);
            }
            break;
        }

        /*
         * Allocate an the extent table for them.
         */
        uint32_t const cExtents  = cPairs + (pAttrHdr->u.NonRes.iVcnFirst != iVcnFirst);
        if (cExtents)
        {
            PRTFSNTFSEXTENT paExtents = (PRTFSNTFSEXTENT)RTMemAllocZ(sizeof(paExtents[0]) * cExtents);
            AssertReturn(paExtents, VERR_NO_MEMORY);

            /*
             * Fill the table.
             */
            uint32_t iExtent = 0;

            /* A sparse hole between this and the previous extent table? */
            if (pAttrHdr->u.NonRes.iVcnFirst != iVcnFirst)
            {
                paExtents[iExtent].off      = UINT64_MAX;
                paExtents[iExtent].cbExtent = (pAttrHdr->u.NonRes.iVcnFirst - iVcnFirst) << cClusterShift;
                Log3(("   paExtent[%#04x]: %#018RX64 LB %#010RX64\n", iExtent, paExtents[iExtent].off, paExtents[iExtent].cbExtent));
                iExtent++;
            }

            /* Run the program again, now with values and without verbose error checking. */
            uint64_t cMaxClustersInRun = (INT64_MAX >> cClusterShift) - pAttrHdr->u.NonRes.iVcnFirst;
            uint64_t cbData            = 0;
            int64_t  iLcn              = 0;
            int      rc                = VINF_SUCCESS;
            offPairs = 0;
            for (; iExtent < cExtents; iExtent++)
            {
                uint8_t const bLengths = pbPairs[offPairs++];
                uint8_t const cbRunField = bLengths & 0x0f;
                uint8_t const cbLcnField = bLengths >> 4;
                AssertBreakStmt((unsigned)cbRunField - 1U <= 7U, rc = VERR_VFS_BOGUS_FORMAT);
                AssertBreakStmt((unsigned)cbLcnField      <= 8U, rc = VERR_VFS_BOGUS_FORMAT);

                AssertBreakStmt(!(pbPairs[offPairs + cbRunField - 1] & 0x80),
                                rc = RTERRINFO_LOG_REL_SET_F(pErrInfo, VERR_VFS_BOGUS_FORMAT,
                                                             "Bad MFT record %#RX64: Extent #%#x for attribute (@%#x): Negative runlength value",
                                                             idxMft, iExtent, offAttrib));
                uint64_t cClustersInRun = 0;
                switch (cbRunField)
                {
                    case 8: cClustersInRun |= (uint64_t)pbPairs[offPairs + 7] << 56; RT_FALL_THRU();
                    case 7: cClustersInRun |= (uint64_t)pbPairs[offPairs + 6] << 48; RT_FALL_THRU();
                    case 6: cClustersInRun |= (uint64_t)pbPairs[offPairs + 5] << 40; RT_FALL_THRU();
                    case 5: cClustersInRun |= (uint64_t)pbPairs[offPairs + 4] << 32; RT_FALL_THRU();
                    case 4: cClustersInRun |= (uint32_t)pbPairs[offPairs + 3] << 24; RT_FALL_THRU();
                    case 3: cClustersInRun |= (uint32_t)pbPairs[offPairs + 2] << 16; RT_FALL_THRU();
                    case 2: cClustersInRun |= (uint16_t)pbPairs[offPairs + 1] <<  8; RT_FALL_THRU();
                    case 1: cClustersInRun |= (uint16_t)pbPairs[offPairs + 0] <<  0; RT_FALL_THRU();
                }
                offPairs += cbRunField;
                AssertBreakStmt(cClustersInRun <= cMaxClustersInRun,
                                rc = RTERRINFO_LOG_REL_SET_F(pErrInfo, VERR_VFS_BOGUS_FORMAT,
                                                             "Bad MFT record %#RX64: Extent #%#x for attribute (@%#x): too many clusters %#RX64, max %#RX64",
                                                             idxMft, iExtent, offAttrib, cClustersInRun, cMaxClustersInRun));
                cMaxClustersInRun          -= cClustersInRun;
                paExtents[iExtent].cbExtent = cClustersInRun << cClusterShift;
                cbData                     += cClustersInRun << cClusterShift;

                if (cbLcnField)
                {
                    unsigned offVncDelta = cbLcnField;
                    int64_t  cLncDelta   = (int8_t)pbPairs[--offVncDelta + offPairs];
                    while (offVncDelta-- > 0)
                        cLncDelta = (cLncDelta << 8) | pbPairs[offVncDelta + offPairs];
                    offPairs += cbLcnField;

                    iLcn += cLncDelta;
                    if (iLcn >= 0)
                    {
                        paExtents[iExtent].off = (uint64_t)iLcn << cClusterShift;
                        AssertBreakStmt((paExtents[iExtent].off >> cClusterShift) == (uint64_t)iLcn,
                                        rc = RTERRINFO_LOG_REL_SET_F(pErrInfo, VERR_VFS_BOGUS_FORMAT,
                                                                     "Bad MFT record %#RX64: Extent #%#x for attribute (@%#x): iLcn %RX64 overflows when shifted by %u",
                                                                     idxMft, iExtent, offAttrib, iLcn, cClusterShift));
                    }
                    else
                        paExtents[iExtent].off = UINT64_MAX;
                }
                else
                    paExtents[iExtent].off = UINT64_MAX;
                Log3(("   paExtent[%#04x]: %#018RX64 LB %#010RX64\n", iExtent, paExtents[iExtent].off, paExtents[iExtent].cbExtent));
            }

            /* Commit if everything went fine? */
            if (RT_SUCCESS(rc))
            {
                pExtents->cbData    = cbData;
                pExtents->cExtents  = cExtents;
                pExtents->paExtents = paExtents;
            }
            else
            {
                RTMemFree(paExtents);
                return rc;
            }
        }
    }
    return VINF_SUCCESS;
}


static int rtFsNtfsVol_ParseMft(PRTFSNTFSVOL pThis, PRTFSNTFSMFTREC pRec, PRTERRINFO pErrInfo)
{
    AssertReturn(!pRec->pCore, VERR_INTERNAL_ERROR_4);

    /*
     * Check that it is a file record and that its base MFT record number is zero.
     * Caller should do the base record resolving.
     */
    PNTFSRECFILE pFileRec = pRec->pFileRec;
    if (pFileRec->Hdr.uMagic != NTFSREC_MAGIC_FILE)
        return RTERRINFO_LOG_REL_SET_F(pErrInfo, VERR_VFS_BOGUS_FORMAT,
                                       "Bad MFT record %#RX64: Not a FILE entry (%.4Rhxs)",
                                       pRec->TreeNode.Key, &pFileRec->Hdr);
    if (pFileRec->BaseMftRec.u64 != 0)
        return RTERRINFO_LOG_REL_SET_F(pErrInfo, VERR_VFS_BOGUS_FORMAT,
                                       "Bad MFT record %#RX64: Not a base record (%#RX64, sqn %#x)",
                                       pRec->TreeNode.Key, NTFSMFTREF_GET_IDX(&pFileRec->BaseMftRec),
                                       NTFSMFTREF_GET_SEQ(&pFileRec->BaseMftRec) );

     /*
      * Create a core node (1 reference, returned even on error).
      */
    PRTFSNTFSCORE pCore = (PRTFSNTFSCORE)RTMemAllocZ(sizeof(*pCore));
    AssertReturn(pCore, VERR_NO_MEMORY);

    pCore->cRefs    = 1;
    pCore->pVol     = pThis;
    RTListInit(&pCore->AttribHead);
    pCore->pMftRec  = pRec;
    rtFsNtfsMftRec_Retain(pRec);
    pRec->pCore     = pCore;

    /*
     * Parse attributes.
     * We process any attribute list afterwards, skipping attributes in this MFT record.
     */
    PRTFSNTFSATTR       pAttrList = NULL;
    uint8_t * const     pbRec     = pRec->pbRec;
    uint32_t            offRec    = pFileRec->offFirstAttrib;
    uint32_t const      cbRecUsed = RT_MIN(pThis->cbMftRecord, pFileRec->cbRecUsed);
    while (offRec + NTFSATTRIBHDR_SIZE_RESIDENT <= cbRecUsed)
    {
        PNTFSATTRIBHDR  pAttrHdr  = (PNTFSATTRIBHDR)&pbRec[offRec];
        uint32_t const  cbAttrib  = RT_LE2H_U32(pAttrHdr->cbAttrib);
        uint32_t const  cbMin     = !pAttrHdr->fNonResident                   ? NTFSATTRIBHDR_SIZE_RESIDENT
                                  : !pAttrHdr->u.NonRes.uCompressionUnit == 0 ? NTFSATTRIBHDR_SIZE_NONRES_UNCOMPRESSED
                                  :                                             NTFSATTRIBHDR_SIZE_NONRES_COMPRESSED;
        if (cbAttrib < cbMin)
            return RTERRINFO_LOG_REL_SET_F(pErrInfo, VERR_VFS_BOGUS_FORMAT,
                                           "Bad MFT record %#RX64: Attribute (@%#x) is too small (%#x, cbMin=%#x)",
                                           pRec->TreeNode.Key, offRec, cbAttrib, cbMin);
        if (offRec + cbAttrib > cbRecUsed)
            return RTERRINFO_LOG_REL_SET_F(pErrInfo, VERR_VFS_BOGUS_FORMAT,
                                           "Bad MFT record %#RX64: Attribute (@%#x) is too long (%#x, cbRecUsed=%#x)",
                                           pRec->TreeNode.Key, offRec, cbAttrib, cbRecUsed);
        PRTFSNTFSATTR pAttrib = (PRTFSNTFSATTR)RTMemAllocZ(sizeof(*pAttrib));
        AssertReturn(pAttrib, VERR_NO_MEMORY);
        pAttrib->pAttrHdr           = pAttrHdr;
        pAttrib->pCore              = pCore;
        //pAttrib->Extents.cExtents   = 0;
        //pAttrib->Extents.paExtents  = NULL;
        //pAttrib->pSubRecHead        = NULL;
        if (pAttrHdr->fNonResident)
        {
            int rc = rtFsNtfsAttr_ParseExtents(pAttrib, &pAttrib->Extents, pThis->cClusterShift, 0 /*iVncFirst*/,
                                               pErrInfo, pRec->TreeNode.Key, offRec);
            if (RT_FAILURE(rc))
            {
                RTMemFree(pAttrib);
                return rc;
            }
        }
        RTListAppend(&pCore->AttribHead, &pAttrib->ListEntry);

        if (pAttrHdr->uAttrType == NTFS_AT_ATTRIBUTE_LIST)
            pAttrList = pAttrib;

        /* Advance. */
        offRec += cbAttrib;
    }

    /*
     * Process any attribute list.
     */
    if (pAttrList)
    {
        /** @todo    */
    }

    return VINF_SUCCESS;
}


/**
 * Destroys a core structure.
 *
 * @returns 0
 * @param   pThis               The core structure.
 */
static uint32_t rtFsNtfsCore_Destroy(PRTFSNTFSCORE pThis)
{
    /*
     * Free attributes.
     */
    PRTFSNTFSATTR pCurAttr;
    PRTFSNTFSATTR pNextAttr;
    RTListForEachSafe(&pThis->AttribHead, pCurAttr, pNextAttr, RTFSNTFSATTR, ListEntry)
    {
        PRTFSNTFSATTRSUBREC pSub = pCurAttr->pSubRecHead;
        while (pSub)
        {
            pCurAttr->pSubRecHead = pSub->pNext;
            RTMemFree(pSub->Extents.paExtents);
            pSub->Extents.paExtents = NULL;
            pSub->pAttrHdr          = NULL;
            pSub->pNext             = NULL;
            RTMemFree(pSub);

            pSub = pCurAttr->pSubRecHead;
        }

        pCurAttr->pCore    = NULL;
        pCurAttr->pAttrHdr = NULL;
        RTMemFree(pCurAttr->Extents.paExtents);
        pCurAttr->Extents.paExtents = NULL;
    }

    /*
     * Release the MFT chain.
     */
    PRTFSNTFSMFTREC pMftRec = pThis->pMftRec;
    while (pMftRec)
    {
        pThis->pMftRec = pMftRec->pNext;
        Assert(pMftRec->pCore == pThis);
        pMftRec->pNext = NULL;
        pMftRec->pCore = NULL;
        rtFsNtfsMftRec_Release(pMftRec, pThis->pVol);

        pMftRec = pThis->pMftRec;
    }

    RTMemFree(pThis);

    return 0;
}


/**
 * Releases a refernece to a core structure, maybe destroying it.
 *
 * @returns New reference count.
 * @param   pThis               The core structure.
 */
static uint32_t rtFsNtfsCore_Release(PRTFSNTFSCORE pThis)
{
    uint32_t cRefs = ASMAtomicDecU32(&pThis->cRefs);
    Assert(cRefs < 128);
    if (cRefs != 0)
        return cRefs;
    return rtFsNtfsCore_Destroy(pThis);
}


/**
 * Retains a refernece to a core structure.
 *
 * @returns New reference count.
 * @param   pThis               The core structure.
 */
static uint32_t rtFsNtfsCore_Retain(PRTFSNTFSCORE pThis)
{
    uint32_t cRefs = ASMAtomicIncU32(&pThis->cRefs);
    Assert(cRefs < 128);
    return cRefs;
}


/**
 * Finds an unnamed attribute.
 *
 * @returns Pointer to the attribute structure if found, NULL if not.
 * @param   pThis               The core object structure to search.
 * @param   uAttrType           The attribute type to find.
 */
static PRTFSNTFSATTR rtFsNtfsCore_FindUnnamedAttribute(PRTFSNTFSCORE pThis, uint32_t uAttrType)
{
    PRTFSNTFSATTR pCurAttr;
    RTListForEach(&pThis->AttribHead, pCurAttr, RTFSNTFSATTR, ListEntry)
    {
        PNTFSATTRIBHDR pAttrHdr = pCurAttr->pAttrHdr;
        if (   pAttrHdr->uAttrType == uAttrType
            && pAttrHdr->cwcName == 0)
            return pCurAttr;
    }
    return NULL;
}



/**
 * @interface_method_impl{RTVFSOBJOPS::Obj,pfnClose}
 */
static DECLCALLBACK(int) rtFsNtfsVol_Close(void *pvThis)
{
    PRTFSNTFSVOL pThis = (PRTFSNTFSVOL)pvThis;

    RTVfsFileRelease(pThis->hVfsBacking);
    pThis->hVfsBacking = NIL_RTVFSFILE;
    pThis->hVfsSelf    = NIL_RTVFS;

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSOBJOPS::Obj,pfnQueryInfo}
 */
static DECLCALLBACK(int) rtFsNtfsVol_QueryInfo(void *pvThis, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr)
{
    NOREF(pvThis); NOREF(pObjInfo); NOREF(enmAddAttr);
    return VERR_WRONG_TYPE;
}


/**
 * @interface_method_impl{RTVFSOBJOPS::Obj,pfnOpenRoot}
 */
static DECLCALLBACK(int) rtFsNtfsVol_OpenRoot(void *pvThis, PRTVFSDIR phVfsDir)
{
    NOREF(pvThis); NOREF(phVfsDir);
    return VERR_NOT_IMPLEMENTED;
}


/**
 * @interface_method_impl{RTVFSOBJOPS::Obj,pfnQueryRangeState}
 */
static DECLCALLBACK(int) rtFsNtfsVol_QueryRangeState(void *pvThis, uint64_t off, size_t cb, bool *pfUsed)
{
    NOREF(pvThis); NOREF(off); NOREF(cb); NOREF(pfUsed);
    return VERR_NOT_IMPLEMENTED;
}


DECL_HIDDEN_CONST(const RTVFSOPS) g_rtFsNtfsVolOps =
{
    /* .Obj = */
    {
        /* .uVersion = */       RTVFSOBJOPS_VERSION,
        /* .enmType = */        RTVFSOBJTYPE_VFS,
        /* .pszName = */        "NtfsVol",
        /* .pfnClose = */       rtFsNtfsVol_Close,
        /* .pfnQueryInfo = */   rtFsNtfsVol_QueryInfo,
        /* .uEndMarker = */     RTVFSOBJOPS_VERSION
    },
    /* .uVersion = */           RTVFSOPS_VERSION,
    /* .fFeatures = */          0,
    /* .pfnOpenRoot = */        rtFsNtfsVol_OpenRoot,
    /* .pfnQueryRangeState = */ rtFsNtfsVol_QueryRangeState,
    /* .uEndMarker = */         RTVFSOPS_VERSION
};



static int rtFsNtfsVolLoadMft(PRTFSNTFSVOL pThis, void *pvBuf, size_t cbBuf, PRTERRINFO pErrInfo)
{
    AssertReturn(cbBuf >= _64K, VERR_INTERNAL_ERROR_2);
    NOREF(pThis); NOREF(pvBuf); NOREF(cbBuf); NOREF(pErrInfo);
    /** @todo read MFT, find bitmap allocation, implement
     *        rtFsNtfsVol_QueryRangeState. */

    /*
     * Bootstrap the MFT data stream.
     */
    PRTFSNTFSMFTREC pRec = rtFsNtfsMftVol_New(pThis, 0);
    AssertReturn(pRec, VERR_NO_MEMORY);

#if 0 //def LOG_ENABLED
    for (uint32_t i = 0; i < 64; i++)
    {
        RTVfsFileReadAt(pThis->hVfsBacking, (pThis->uLcnMft << pThis->cClusterShift) + i * pThis->cbMftRecord,
                        pRec->pbRec, pThis->cbMftRecord, NULL);
        pRec->TreeNode.Key = i;
        Log(("\n"));
        rtfsNtfsMftRec_Log(pRec, pThis->cbMftRecord);
    }
    pRec->TreeNode.Key = 0;
#endif

    pRec->offDisk = pThis->uLcnMft << pThis->cClusterShift;
    int rc = RTVfsFileReadAt(pThis->hVfsBacking, pRec->offDisk, pRec->pbRec, pThis->cbMftRecord, NULL);
    if (RT_SUCCESS(rc))
    {
#ifdef LOG_ENABLED
        rtfsNtfsMftRec_Log(pRec, pThis->cbMftRecord);
#endif
        rc = rtFsNtfsVol_ParseMft(pThis, pRec, pErrInfo);
        if (RT_SUCCESS(rc))
        {
            pThis->pMftData = rtFsNtfsCore_FindUnnamedAttribute(pRec->pCore, NTFS_AT_DATA);
            if (pThis->pMftData)
            {
                /** @todo sanity check the attribute. */
                rtFsNtfsMftRec_Release(pRec, pThis);
                return rc;
            }
            rc = RTERRINFO_LOG_SET(pErrInfo, VERR_VFS_BOGUS_FORMAT, "MFT record #0 has no unnamed DATA attribute!");
        }
        if (pRec->pCore)
            rtFsNtfsCore_Release(pRec->pCore);
        rtFsNtfsMftRec_Release(pRec, pThis);
    }
    else
        rc = RTERRINFO_LOG_SET(pErrInfo, rc, "Error reading MFT record #0");
    return rc;
}


/**
 * Loads the bootsector and parses it, copying values into the instance data.
 *
 * @returns IRPT status code.
 * @param   pThis           The instance data.
 * @param   pvBuf           The buffer.
 * @param   cbBuf           The buffer size.
 * @param   pErrInfo        Where to return additional error details.
 */
static int rtFsNtfsVolLoadAndParseBootsector(PRTFSNTFSVOL pThis, void *pvBuf, size_t cbBuf, PRTERRINFO pErrInfo)
{
    AssertReturn(cbBuf >= sizeof(FATBOOTSECTOR), VERR_INTERNAL_ERROR_2);

    /*
     * Read the boot sector and check that it makes sense for a NTFS volume.
     *
     * Note! There are two potential backup locations of the boot sector, however we
     *       currently don't implement falling back on these on corruption/read errors.
     */
    PFATBOOTSECTOR pBootSector = (PFATBOOTSECTOR)pvBuf;
    int rc = RTVfsFileReadAt(pThis->hVfsBacking, 0, pBootSector, sizeof(*pBootSector), NULL);
    if (RT_FAILURE(rc))
        return RTERRINFO_LOG_SET(pErrInfo, rc, "Error reading boot sector");

    if (memcmp(pBootSector->achOemName, RT_STR_TUPLE(NTFS_OEM_ID_MAGIC)) != 0)
        return RTERRINFO_LOG_SET_F(pErrInfo, VERR_VFS_UNKNOWN_FORMAT,
                                   "Not NTFS - OEM field mismatch: %.8Rhxs'", pBootSector->achOemName);

    /* Check must-be-zero BPB fields. */
    if (pBootSector->Bpb.Ntfs.Bpb.cReservedSectors != 0)
        return RTERRINFO_LOG_SET_F(pErrInfo, VERR_VFS_UNKNOWN_FORMAT, "Not NTFS - MBZ: BPB.cReservedSectors=%u",
                                   RT_LE2H_U16(pBootSector->Bpb.Ntfs.Bpb.cReservedSectors));
    if (pBootSector->Bpb.Ntfs.Bpb.cFats != 0)
        return RTERRINFO_LOG_SET_F(pErrInfo, VERR_VFS_UNKNOWN_FORMAT, "Not NTFS - MBZ: BPB.cFats=%u",
                                   pBootSector->Bpb.Ntfs.Bpb.cFats);
    if (pBootSector->Bpb.Ntfs.Bpb.cMaxRootDirEntries != 0)
        return RTERRINFO_LOG_SET_F(pErrInfo, VERR_VFS_UNKNOWN_FORMAT, "Not NTFS - MBZ: BPB.cMaxRootDirEntries=%u",
                                   RT_LE2H_U16(pBootSector->Bpb.Ntfs.Bpb.cMaxRootDirEntries));
    if (pBootSector->Bpb.Ntfs.Bpb.cTotalSectors16 != 0)
        return RTERRINFO_LOG_SET_F(pErrInfo, VERR_VFS_UNKNOWN_FORMAT, "Not NTFS - MBZ: BPB.cTotalSectors16=%u",
                                   RT_LE2H_U16(pBootSector->Bpb.Ntfs.Bpb.cTotalSectors16));
    if (pBootSector->Bpb.Ntfs.Bpb.cSectorsPerFat != 0)
        return RTERRINFO_LOG_SET_F(pErrInfo, VERR_VFS_UNKNOWN_FORMAT, "Not NTFS - MBZ: BPB.cSectorsPerFat=%u",
                                   RT_LE2H_U16(pBootSector->Bpb.Ntfs.Bpb.cSectorsPerFat));

    /* Check other relevant BPB fields. */
    uint32_t cbSector = RT_LE2H_U16(pBootSector->Bpb.Ntfs.Bpb.cbSector);
    if (   cbSector != 512
        && cbSector != 1024
        && cbSector != 2048
        && cbSector != 4096)
        return RTERRINFO_LOG_SET_F(pErrInfo, VERR_VFS_UNKNOWN_FORMAT, "Not NTFS - BPB.cbSector is ouf of range: %u", cbSector);
    pThis->cbSector = cbSector;
    Log2(("NTFS BPB: cbSector=%#x\n", cbSector));

    uint32_t cClusterPerSector = RT_LE2H_U16(pBootSector->Bpb.Ntfs.Bpb.cSectorsPerCluster);
    if (   !RT_IS_POWER_OF_TWO(cClusterPerSector)
        || cClusterPerSector == 0)
        return RTERRINFO_LOG_SET_F(pErrInfo, VERR_VFS_UNKNOWN_FORMAT,
                                   "Not NTFS - BPB.cCluster is ouf of range: %u", cClusterPerSector);

    pThis->cbCluster = cClusterPerSector * cbSector;
    if (pThis->cbCluster > _64K)
        return RTERRINFO_LOG_SET_F(pErrInfo, VERR_VFS_UNSUPPORTED_FORMAT,
                                   "cluster size exceeds 64KB: %#x", pThis->cbCluster);
    pThis->cClusterShift = ASMBitFirstSetU32(pThis->cbCluster) - 1;
    Log2(("NTFS BPB: cClusterPerSector=%#x => %#x bytes, %u shift\n", cClusterPerSector, pThis->cbCluster, pThis->cClusterShift));

    /* NTFS BPB: cSectors. */
    uint64_t cSectors = RT_LE2H_U64(pBootSector->Bpb.Ntfs.cSectors);
    if (cSectors > pThis->cbBacking / pThis->cbSector)
        return RTERRINFO_LOG_SET_F(pErrInfo, VERR_VFS_UNSUPPORTED_FORMAT,
                                   "NTFS sector count exceeds volume size: %#RX64 vs %#RX64",
                                   cSectors, pThis->cbBacking / pThis->cbSector);
    if (cSectors < 256)
        return RTERRINFO_LOG_SET_F(pErrInfo, VERR_VFS_UNSUPPORTED_FORMAT, "NTFS sector count too small: %#RX64", cSectors);
    pThis->cbVolume  = cSectors * pThis->cbSector;
    pThis->cClusters = cSectors / cClusterPerSector;
    Log2(("NTFS BPB: cSectors=%#RX64 => %#xu bytes (%Rhcb) => cClusters=%#RX64\n",
          cSectors, pThis->cbVolume, pThis->cbVolume, pThis->cClusters));

    /* NTFS BPB: MFT location. */
    uint64_t uLcn = RT_LE2H_U64(pBootSector->Bpb.Ntfs.uLcnMft);
    if (   uLcn < 1
        || uLcn >= pThis->cClusters)
        return RTERRINFO_LOG_SET_F(pErrInfo, VERR_VFS_UNSUPPORTED_FORMAT,
                                   "NTFS MFT location is out of bounds: %#RX64 (%#RX64 clusters)", uLcn, pThis->cClusters);
    pThis->uLcnMft = uLcn;
    Log2(("NTFS BPB: uLcnMft=%#RX64 (byte offset %#RX64)\n", uLcn, uLcn << pThis->cClusterShift));

    /* NTFS BPB: Mirror MFT location. */
    uLcn = RT_LE2H_U64(pBootSector->Bpb.Ntfs.uLcnMftMirror);
    if (   uLcn < 1
        || uLcn >= pThis->cClusters)
        return RTERRINFO_LOG_SET_F(pErrInfo, VERR_VFS_UNSUPPORTED_FORMAT,
                                   "NTFS mirror MFT location is out of bounds: %#RX64 (%#RX64 clusters)", uLcn, pThis->cClusters);
    pThis->uLcnMftMirror = uLcn;
    Log2(("NTFS BPB: uLcnMftMirror=%#RX64 (byte offset %#RX64)\n", uLcn, uLcn << pThis->cClusterShift));

    /* NTFS BPB: Size of MFT file record. */
    if (pBootSector->Bpb.Ntfs.cClustersPerMftRecord >= 0)
    {
        if (   !RT_IS_POWER_OF_TWO((uint32_t)pBootSector->Bpb.Ntfs.cClustersPerMftRecord)
            || pBootSector->Bpb.Ntfs.cClustersPerMftRecord == 0)
            return RTERRINFO_LOG_SET_F(pErrInfo, VERR_VFS_UNSUPPORTED_FORMAT,
                                       "NTFS clusters-per-mft-record value is zero or not a power of two: %#x",
                                        pBootSector->Bpb.Ntfs.cClustersPerMftRecord);
        pThis->cbMftRecord = (uint32_t)pBootSector->Bpb.Ntfs.cClustersPerMftRecord << pThis->cClusterShift;
        Assert(pThis->cbMftRecord == pBootSector->Bpb.Ntfs.cClustersPerMftRecord * pThis->cbCluster);
    }
    else if (   pBootSector->Bpb.Ntfs.cClustersPerMftRecord < -20
             || pBootSector->Bpb.Ntfs.cClustersPerMftRecord > -9)
        return RTERRINFO_LOG_SET_F(pErrInfo, VERR_VFS_UNSUPPORTED_FORMAT,
                                   "NTFS clusters-per-mft-record is out of shift range: %d",
                                    pBootSector->Bpb.Ntfs.cClustersPerMftRecord);
    else
        pThis->cbMftRecord = UINT32_C(1) << -pBootSector->Bpb.Ntfs.cClustersPerMftRecord;
    Log2(("NTFS BPB: cbMftRecord=%#x\n", pThis->cbMftRecord));
    if (   pThis->cbMftRecord > _32K
        || pThis->cbMftRecord < 256)
        return RTERRINFO_LOG_SET_F(pErrInfo, VERR_VFS_UNSUPPORTED_FORMAT,
                                   "Unsupported NTFS MFT record size: %#x", pThis->cbMftRecord);

    /* NTFS BPB: Index block size */
    if (pBootSector->Bpb.Ntfs.cClusterPerIndexBlock >= 0)
    {
        if (   !RT_IS_POWER_OF_TWO((uint32_t)pBootSector->Bpb.Ntfs.cClusterPerIndexBlock)
            || pBootSector->Bpb.Ntfs.cClusterPerIndexBlock == 0)
            return RTERRINFO_LOG_SET_F(pErrInfo, VERR_VFS_UNSUPPORTED_FORMAT,
                                       "NTFS clusters-per-index-block is zero or not a power of two: %#x",
                                        pBootSector->Bpb.Ntfs.cClusterPerIndexBlock);
        pThis->cbIndexRecord = (uint32_t)pBootSector->Bpb.Ntfs.cClusterPerIndexBlock << pThis->cClusterShift;
        Assert(pThis->cbIndexRecord == pBootSector->Bpb.Ntfs.cClusterPerIndexBlock * pThis->cbCluster);
    }
    else if (   pBootSector->Bpb.Ntfs.cClusterPerIndexBlock < -32
             || pBootSector->Bpb.Ntfs.cClusterPerIndexBlock > -9)
        return RTERRINFO_LOG_SET_F(pErrInfo, VERR_VFS_UNSUPPORTED_FORMAT,
                                   "NTFS clusters-per-index-block is out of shift range: %d",
                                    pBootSector->Bpb.Ntfs.cClusterPerIndexBlock);
    else
        pThis->cbIndexRecord = UINT32_C(1) << -pBootSector->Bpb.Ntfs.cClustersPerMftRecord;
    Log2(("NTFS BPB: cbIndexRecord=%#x\n", pThis->cbIndexRecord));

    pThis->uSerialNo = RT_LE2H_U64(pBootSector->Bpb.Ntfs.uSerialNumber);
    Log2(("NTFS BPB: uSerialNo=%#x\n", pThis->uSerialNo));


    return VINF_SUCCESS;
}


RTDECL(int) RTFsNtfsVolOpen(RTVFSFILE hVfsFileIn, uint32_t fMntFlags, uint32_t fNtfsFlags, PRTVFS phVfs, PRTERRINFO pErrInfo)
{
    AssertPtrReturn(phVfs, VERR_INVALID_POINTER);
    AssertReturn(!(fMntFlags & ~RTVFSMNT_F_VALID_MASK), VERR_INVALID_FLAGS);
    AssertReturn(!fNtfsFlags, VERR_INVALID_FLAGS);

    uint32_t cRefs = RTVfsFileRetain(hVfsFileIn);
    AssertReturn(cRefs != UINT32_MAX, VERR_INVALID_HANDLE);

    /*
     * Create a VFS instance and initialize the data so rtFsNtfsVol_Close works.
     */
    RTVFS        hVfs;
    PRTFSNTFSVOL pThis;
    int rc = RTVfsNew(&g_rtFsNtfsVolOps, sizeof(*pThis), NIL_RTVFS, RTVFSLOCK_CREATE_RW, &hVfs, (void **)&pThis);
    if (RT_SUCCESS(rc))
    {
        pThis->hVfsBacking    = hVfsFileIn;
        pThis->hVfsSelf       = hVfs;
        pThis->fMntFlags      = fMntFlags;
        pThis->fNtfsFlags     = fNtfsFlags;

        rc = RTVfsFileGetSize(pThis->hVfsBacking, &pThis->cbBacking);
        if (RT_SUCCESS(rc))
        {
            void *pvBuf = RTMemTmpAlloc(_64K);
            if (pvBuf)
            {
                rc = rtFsNtfsVolLoadAndParseBootsector(pThis, pvBuf, _64K, pErrInfo);
                if (RT_SUCCESS(rc))
                    rc = rtFsNtfsVolLoadMft(pThis, pvBuf, _64K, pErrInfo);
                RTMemTmpFree(pvBuf);
                if (RT_SUCCESS(rc))
                {
                    *phVfs = hVfs;
                    return VINF_SUCCESS;
                }
            }
            else
                rc = VERR_NO_TMP_MEMORY;
        }

        RTVfsRelease(hVfs);
        *phVfs = NIL_RTVFS;
    }
    else
        RTVfsFileRelease(hVfsFileIn);

    return rc;
}


/**
 * @interface_method_impl{RTVFSCHAINELEMENTREG,pfnValidate}
 */
static DECLCALLBACK(int) rtVfsChainNtfsVol_Validate(PCRTVFSCHAINELEMENTREG pProviderReg, PRTVFSCHAINSPEC pSpec,
                                                    PRTVFSCHAINELEMSPEC pElement, uint32_t *poffError, PRTERRINFO pErrInfo)
{
    RT_NOREF(pProviderReg);

    /*
     * Basic checks.
     */
    if (pElement->enmTypeIn != RTVFSOBJTYPE_FILE)
        return pElement->enmTypeIn == RTVFSOBJTYPE_INVALID ? VERR_VFS_CHAIN_CANNOT_BE_FIRST_ELEMENT : VERR_VFS_CHAIN_TAKES_FILE;
    if (   pElement->enmType != RTVFSOBJTYPE_VFS
        && pElement->enmType != RTVFSOBJTYPE_DIR)
        return VERR_VFS_CHAIN_ONLY_DIR_OR_VFS;
    if (pElement->cArgs > 1)
        return VERR_VFS_CHAIN_AT_MOST_ONE_ARG;

    /*
     * Parse the flag if present, save in pElement->uProvider.
     */
    bool fReadOnly = (pSpec->fOpenFile & RTFILE_O_ACCESS_MASK) == RTFILE_O_READ;
    if (pElement->cArgs > 0)
    {
        const char *psz = pElement->paArgs[0].psz;
        if (*psz)
        {
            if (!strcmp(psz, "ro"))
                fReadOnly = true;
            else if (!strcmp(psz, "rw"))
                fReadOnly = false;
            else
            {
                *poffError = pElement->paArgs[0].offSpec;
                return RTErrInfoSet(pErrInfo, VERR_VFS_CHAIN_INVALID_ARGUMENT, "Expected 'ro' or 'rw' as argument");
            }
        }
    }

    pElement->uProvider = fReadOnly ? RTVFSMNT_F_READ_ONLY : 0;
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSCHAINELEMENTREG,pfnInstantiate}
 */
static DECLCALLBACK(int) rtVfsChainNtfsVol_Instantiate(PCRTVFSCHAINELEMENTREG pProviderReg, PCRTVFSCHAINSPEC pSpec,
                                                       PCRTVFSCHAINELEMSPEC pElement, RTVFSOBJ hPrevVfsObj,
                                                       PRTVFSOBJ phVfsObj, uint32_t *poffError, PRTERRINFO pErrInfo)
{
    RT_NOREF(pProviderReg, pSpec, poffError);

    int         rc;
    RTVFSFILE   hVfsFileIn = RTVfsObjToFile(hPrevVfsObj);
    if (hVfsFileIn != NIL_RTVFSFILE)
    {
        RTVFS hVfs;
        rc = RTFsNtfsVolOpen(hVfsFileIn, (uint32_t)pElement->uProvider, (uint32_t)(pElement->uProvider >> 32), &hVfs, pErrInfo);
        RTVfsFileRelease(hVfsFileIn);
        if (RT_SUCCESS(rc))
        {
            *phVfsObj = RTVfsObjFromVfs(hVfs);
            RTVfsRelease(hVfs);
            if (*phVfsObj != NIL_RTVFSOBJ)
                return VINF_SUCCESS;
            rc = VERR_VFS_CHAIN_CAST_FAILED;
        }
    }
    else
        rc = VERR_VFS_CHAIN_CAST_FAILED;
    return rc;
}


/**
 * @interface_method_impl{RTVFSCHAINELEMENTREG,pfnCanReuseElement}
 */
static DECLCALLBACK(bool) rtVfsChainNtfsVol_CanReuseElement(PCRTVFSCHAINELEMENTREG pProviderReg,
                                                            PCRTVFSCHAINSPEC pSpec, PCRTVFSCHAINELEMSPEC pElement,
                                                            PCRTVFSCHAINSPEC pReuseSpec, PCRTVFSCHAINELEMSPEC pReuseElement)
{
    RT_NOREF(pProviderReg, pSpec, pReuseSpec);
    if (   pElement->paArgs[0].uProvider == pReuseElement->paArgs[0].uProvider
        || !pReuseElement->paArgs[0].uProvider)
        return true;
    return false;
}


/** VFS chain element 'file'. */
static RTVFSCHAINELEMENTREG g_rtVfsChainNtfsVolReg =
{
    /* uVersion = */            RTVFSCHAINELEMENTREG_VERSION,
    /* fReserved = */           0,
    /* pszName = */             "ntfs",
    /* ListEntry = */           { NULL, NULL },
    /* pszHelp = */             "Open a NTFS file system, requires a file object on the left side.\n"
                                "First argument is an optional 'ro' (read-only) or 'rw' (read-write) flag.\n",
    /* pfnValidate = */         rtVfsChainNtfsVol_Validate,
    /* pfnInstantiate = */      rtVfsChainNtfsVol_Instantiate,
    /* pfnCanReuseElement = */  rtVfsChainNtfsVol_CanReuseElement,
    /* uEndMarker = */          RTVFSCHAINELEMENTREG_VERSION
};

RTVFSCHAIN_AUTO_REGISTER_ELEMENT_PROVIDER(&g_rtVfsChainNtfsVolReg, rtVfsChainNtfsVolReg);

