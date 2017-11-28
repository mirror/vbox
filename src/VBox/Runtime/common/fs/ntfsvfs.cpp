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
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/** Pointer to the instance data for a NTFS volume. */
typedef struct RTFSNTFSVOL *PRTFSNTFSVOL;
/** Pointer to a NTFS MFT record. */
typedef struct RTFSNTFSMFTREC *PRTFSNTFSMFTREC;

/**
 * NTFS MFT record.
 */
typedef struct RTFSNTFSMFTREC
{
    /** MFT record number as key. */
    AVLU64NODECORE      Core;
    /** Pointer to the next MFT record if chained. */
    PRTFSNTFSMFTREC     pNext;
    /** Pointer back to the volume. */
    PRTFSNTFSVOL        pVol;
    /** The disk offset of this MFT entry. */
    uint64_t            offDisk;
    union
    {
        /** Generic pointer. */
        uint8_t        *pbRec;
        /** Pointer to the file record. */
        PNTFSRECFILE    pFileRec;
    } RT_UNION_NM(u);

    /** Reference counter. */
    uint32_t volatile   cRefs;

    // ....
} RTFSNTFSMFTREC;

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

    /** Pointer to the MFT record for the MFT. */
    PRTFSNTFSMFTREC pMft;

    /** Root of the MFT record tree (RTFSNTFSMFTREC). */
    AVLU64TREE      MftRoot;
} RTFSNTFSVOL;


static PRTFSNTFSMFTREC rtFsNtfsMftRec_New(PRTFSNTFSVOL pVol, uint64_t idMft)
{
    PRTFSNTFSMFTREC pRec = (PRTFSNTFSMFTREC)RTMemAllocZ(sizeof(*pRec));
    if (pRec)
    {
        pRec->pbRec = (uint8_t *)RTMemAllocZ(pVol->cbMftRecord);
        if (pRec->pbRec)
        {
            pRec->Core.Key = idMft;
            pRec->pNext    = NULL;
            pRec->offDisk  = UINT64_MAX / 2;
            pRec->pVol     = pVol;
            pRec->cRefs    = 1;
            return pRec;
        }
    }
    return NULL;
}


#if 0 /* currently unused */
static uint32_t rtFsNtfsMftRec_Destroy(PRTFSNTFSMFTREC pThis)
{
    RTMemFree(pThis->pbRec);
    pThis->pbRec = NULL;

    PAVLU64NODECORE pRemoved = RTAvlU64Remove(&pThis->pVol->MftRoot, pThis->Core.Key);
    Assert(pRemoved == &pThis->Core); NOREF(pRemoved);

    pThis->pVol = NULL;
    RTMemFree(pThis);

    return 0;
}


static uint32_t rtFsNtfsMftRec_Retain(PRTFSNTFSMFTREC pThis)
{
    uint32_t cRefs = ASMAtomicIncU32(&pThis->cRefs);
    Assert(cRefs < 64);
    return cRefs;
}


static uint32_t rtFsNtfsMftRec_Release(PRTFSNTFSMFTREC pThis)
{
    uint32_t cRefs = ASMAtomicDecU32(&pThis->cRefs);
    Assert(cRefs < 64);
    if (cRefs != 0)
        return cRefs;
    return rtFsNtfsMftRec_Destroy(pThis);
}
#endif


#ifdef LOG_ENABLED
/**
 * Logs the MFT record
 *
 * @param   pRec        The MFT record to log.
 */
static void rtfsNtfsMftRec_Log(PRTFSNTFSMFTREC pRec)
{
    if (LogIs2Enabled())
    {
        PCNTFSRECFILE  pFileRec = pRec->pFileRec;
        Log2(("NTFS: MFT #%#RX64 at %#RX64\n", pRec->Core.Key, pRec->offDisk));
        if (pFileRec->Hdr.uMagic == NTFSREC_MAGIC_FILE)
        {
            size_t const          cbRec = pRec->pVol->cbMftRecord;
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
                    uint16_t const offValue = RT_LE2H_U16(pHdr->Res.offValue);
                    uint32_t const cbValue  = RT_LE2H_U32(pHdr->Res.cbValue);
                    Log2(("NTFS:     Value: %#x LB %#x, fFlags=%#x bReserved=%#x\n",
                          offValue, cbValue, pHdr->Res.fFlags, pHdr->Res.bReserved));
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

                            //case NTFS_AT_ATTRIBUTE_LIST:

                            case NTFS_AT_FILENAME:
                            {
                                PCNTFSATFILENAME pInfo = (PCNTFSATFILENAME)pbValue;
                                if (cbValue >= RT_OFFSETOF(NTFSATFILENAME, wszFilename))
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
                                        Log2(("NTFS:     uReparseTag        %#RX32\n", RT_LE2H_U32(pInfo->uReparseTag) ));
                                    else
                                        Log2(("NTFS:     cbPackedEas        %#RX16\n", RT_LE2H_U16(pInfo->cbPackedEas) ));
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
                          RT_LE2H_U64(pHdr->NonRes.iVcnFirst), RT_LE2H_U64(pHdr->NonRes.iVcnLast),
                          RT_LE2H_U64(pHdr->NonRes.iVcnLast) - RT_LE2H_U64(pHdr->NonRes.iVcnFirst) + 1));
                    Log2(("NTFS:     cbAllocated        %#RX64 (%Rhcb)\n",
                          RT_LE2H_U64(pHdr->NonRes.cbAllocated), RT_LE2H_U64(pHdr->NonRes.cbAllocated)));
                    Log2(("NTFS:     cbInitialized      %#RX64 (%Rhcb)\n",
                          RT_LE2H_U64(pHdr->NonRes.cbInitialized), RT_LE2H_U64(pHdr->NonRes.cbInitialized)));
                    uint16_t const offMappingPairs = RT_LE2H_U16(pHdr->NonRes.offMappingPairs);
                    Log2(("NTFS:     offMappingPairs    %#RX16\n", offMappingPairs));
                    if (   pHdr->NonRes.abReserved[0] || pHdr->NonRes.abReserved[1]
                        || pHdr->NonRes.abReserved[2] || pHdr->NonRes.abReserved[3] || pHdr->NonRes.abReserved[4] )
                        Log2(("NTFS:     abReserved         %.7Rhxs\n", pHdr->NonRes.abReserved));
                    if (pHdr->NonRes.uCompressionUnit != 0)
                        Log2(("NTFS:     Compression unit   2^%u clusters\n", pHdr->NonRes.uCompressionUnit));

                    if (   NTFSATTRIBHDR_SIZE_NONRES_COMPRESSED <= cbMaxAttrib
                        && NTFSATTRIBHDR_SIZE_NONRES_COMPRESSED <= cbAttrib
                        && (   offMappingPairs >= NTFSATTRIBHDR_SIZE_NONRES_COMPRESSED
                            || offMappingPairs <  NTFSATTRIBHDR_SIZE_NONRES_UNCOMPRESSED))
                        Log2(("NTFS:     cbCompressed       %#RX64 (%Rhcb)\n",
                              RT_LE2H_U64(pHdr->NonRes.cbCompressed), RT_LE2H_U64(pHdr->NonRes.cbCompressed)));
                    else if (pHdr->NonRes.uCompressionUnit != 0 && pHdr->NonRes.uCompressionUnit != 64)
                        Log2(("NTFS:     !Error! Compressed attrib fields are out of bound!\n"));

                    if (   offMappingPairs < cbAttrib
                        && offMappingPairs >= NTFSATTRIBHDR_SIZE_NONRES_UNCOMPRESSED)
                    {
                        uint8_t const *pbPairs    = &pbRec[offRec + offMappingPairs];
                        uint32_t const cbMaxPairs = cbAttrib - offMappingPairs;
                        int64_t iVnc = pHdr->NonRes.iVcnFirst;
                        Log2(("NTFS:     Mapping Pairs: %.*Rhxsd\n", cbMaxPairs, pbPairs));
                        if (!iVnc && !*pbPairs)
                            Log2(("NTFS:         [0]: Empty\n", cbMaxPairs, pbPairs));
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
                                    int8_t const *pbNum = (int8_t const *)&pbPairs[offPairs + cbNum]; /* last byte */
                                    cClustersInRun = *pbNum--;
                                    while (cbNum-- > 1)
                                        cClustersInRun = (cClustersInRun << 8) + *pbNum--;
                                }
                                else
                                    cClustersInRun = -1;

                                /* Value 2: The logical cluster delta to get to the first cluster in the run. */
                                cbNum = bLengths >> 4;
                                if (cbNum)
                                {
                                    int8_t const *pbNum  = (int8_t const *)&pbPairs[offPairs + cbNum + (bLengths & 0xf)]; /* last byte */
                                    int64_t cLcnDelta = *pbNum--;
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

    PRTFSNTFSMFTREC pRec = rtFsNtfsMftRec_New(pThis, 0);
    AssertReturn(pRec, VERR_NO_MEMORY);
    pThis->pMft = pRec;

    int rc = RTVfsFileReadAt(pThis->hVfsBacking, pThis->uLcnMft << pThis->cClusterShift, pRec->pbRec, pThis->cbMftRecord, NULL);
    if (RT_FAILURE(rc))
        return RTERRINFO_LOG_SET(pErrInfo, rc, "Error reading MFT record #0");
#ifdef LOG_ENABLED
    rtfsNtfsMftRec_Log(pRec);
#endif

    //Log(("MFT#0:\n%.*Rhxd\n", pThis->cbMftRecord, pRec->pbRec));

    return VINF_SUCCESS;
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

