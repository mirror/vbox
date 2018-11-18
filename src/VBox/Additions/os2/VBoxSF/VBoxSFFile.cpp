/** $Id$ */
/** @file
 * VBoxSF - OS/2 Shared Folders, the file level IFS EPs.
 */

/*
 * Copyright (c) 2007-2018 knut st. osmundsen <bird-src-spam@anduin.net>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DEFAULT
#include "VBoxSFInternal.h"

#include <VBox/log.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/mem.h>



DECLASM(APIRET)
FS32_OPENCREATE(PCDFSI pCdFsi, PVBOXSFCD pCdFsd, PCSZ pszName, LONG offCurDirEnd,
                PSFFSI pSfFsi, PVBOXSFSYFI pSfFsd, ULONG fOpenMode, USHORT fOpenFlags,
                PUSHORT puAction, ULONG fAttribs, BYTE const *pbEaBuf, PUSHORT pfGenFlag)
{
    LogFlow(("FS32_OPENCREATE: pCdFsi=%p pCdFsd=%p pszName=%p:{%s} offCurDirEnd=%d pSfFsi=%p pSfFsd=%p fOpenMode=%#x fOpenFlags=%#x puAction=%p fAttribs=%#x pbEaBuf=%p pfGenFlag=%p\n",
             pCdFsi, pCdFsd, pszName, pszName, offCurDirEnd, pSfFsi, pSfFsd, fOpenMode, fOpenFlags, puAction, fAttribs, pbEaBuf, pfGenFlag));
    RT_NOREF(pfGenFlag);

    /*
     * Validate and convert parameters.
     */
    /* No EAs. */
    if (!pbEaBuf)
    { /* likely */ }
    else
    {
        LogRel(("FS32_OPENCREATE: Returns ERROR_EAS_NOT_SUPPORTED [%p];\n", pbEaBuf));
        return ERROR_EAS_NOT_SUPPORTED;
    }

    /* No direct access. */
    if (!(fOpenMode & OPEN_FLAGS_DASD))
    { /* likely */ }
    else
    {
        LogRel(("FS32_OPENCREATE: Returns ERROR_ACCESS_DENIED [DASD];\n"));
        return ERROR_ACCESS_DENIED;
    }

    SHFLCREATEPARMS *pParams = (SHFLCREATEPARMS *)VbglR0PhysHeapAlloc(sizeof(*pParams));
    if (!pParams)
        return ERROR_NOT_ENOUGH_MEMORY;
    RT_ZERO(*pParams);

    /* access: */
    if (fOpenMode & OPEN_ACCESS_READWRITE)
        pParams->CreateFlags = SHFL_CF_ACCESS_READWRITE | SHFL_CF_ACCESS_ATTR_READWRITE;
    else if (fOpenMode & OPEN_ACCESS_WRITEONLY)
        pParams->CreateFlags = SHFL_CF_ACCESS_WRITE     | SHFL_CF_ACCESS_ATTR_WRITE;
    else
        pParams->CreateFlags = SHFL_CF_ACCESS_READ      | SHFL_CF_ACCESS_ATTR_READ; /* read or/and exec */

    /* Sharing: */
    switch (fOpenMode & (OPEN_SHARE_DENYNONE | OPEN_SHARE_DENYREADWRITE | OPEN_SHARE_DENYREAD | OPEN_SHARE_DENYWRITE))
    {
        case OPEN_SHARE_DENYNONE:       pParams->CreateFlags |= SHFL_CF_ACCESS_DENYNONE; break;
        case OPEN_SHARE_DENYWRITE:      pParams->CreateFlags |= SHFL_CF_ACCESS_DENYWRITE; break;
        case OPEN_SHARE_DENYREAD:       pParams->CreateFlags |= SHFL_CF_ACCESS_DENYREAD; break;
        case OPEN_SHARE_DENYREADWRITE:  pParams->CreateFlags |= SHFL_CF_ACCESS_DENYALL; break;
        case 0:                         pParams->CreateFlags |= SHFL_CF_ACCESS_DENYWRITE; break; /* compatibility */
        default:
            LogRel(("FS32_OPENCREATE: Invalid file sharing mode: %#x\n", fOpenMode));
            VbglR0PhysHeapFree(pParams);
            return VERR_INVALID_PARAMETER;

    }

    /* How to open the file: */
    switch (fOpenFlags & 0x13)
    {
        case                        OPEN_ACTION_FAIL_IF_EXISTS | OPEN_ACTION_FAIL_IF_NEW:      /* 0x00 */
            pParams->CreateFlags |= SHFL_CF_ACT_FAIL_IF_EXISTS | SHFL_CF_ACT_FAIL_IF_NEW;
            break;
        case                        OPEN_ACTION_FAIL_IF_EXISTS | OPEN_ACTION_CREATE_IF_NEW:    /* 0x10 */
            pParams->CreateFlags |= SHFL_CF_ACT_FAIL_IF_EXISTS | SHFL_CF_ACT_CREATE_IF_NEW;
            break;
        case                        OPEN_ACTION_OPEN_IF_EXISTS | OPEN_ACTION_FAIL_IF_NEW:      /* 0x01 */
            pParams->CreateFlags |= SHFL_CF_ACT_OPEN_IF_EXISTS | SHFL_CF_ACT_FAIL_IF_NEW;
            break;
        case                        OPEN_ACTION_OPEN_IF_EXISTS | OPEN_ACTION_CREATE_IF_NEW:    /* 0x11 */
            pParams->CreateFlags |= SHFL_CF_ACT_OPEN_IF_EXISTS | SHFL_CF_ACT_CREATE_IF_NEW;
            break;
        case                        OPEN_ACTION_REPLACE_IF_EXISTS | OPEN_ACTION_FAIL_IF_NEW:   /* 0x02 */
            pParams->CreateFlags |= SHFL_CF_ACT_REPLACE_IF_EXISTS | SHFL_CF_ACT_FAIL_IF_NEW;
            break;
        case                        OPEN_ACTION_REPLACE_IF_EXISTS | OPEN_ACTION_CREATE_IF_NEW: /* 0x12 */
            pParams->CreateFlags |= SHFL_CF_ACT_REPLACE_IF_EXISTS | SHFL_CF_ACT_CREATE_IF_NEW;
            break;
        default:
            LogRel(("FS32_OPENCREATE: Invalid file open flags: %#x\n", fOpenFlags));
            VbglR0PhysHeapFree(pParams);
            return VERR_INVALID_PARAMETER;
    }

    /* Misc: cache, etc? There seems to be no API for that. */

    /* Attributes: */
    pParams->Info.Attr.fMode = ((uint32_t)fAttribs << RTFS_DOS_SHIFT) & RTFS_DOS_MASK_OS2;

    /* Initial size: */
    if (pSfFsi->sfi_sizel > 0)
        pParams->Info.cbObject = pSfFsi->sfi_sizel;

    /*
     * Resolve path to a folder and folder relative path.
     */
    PVBOXSFFOLDER pFolder;
    PSHFLSTRING   pStrFolderPath;
    RT_NOREF(pCdFsi);
    APIRET rc = vboxSfOs2ResolvePath(pszName, pCdFsd, offCurDirEnd, &pFolder, &pStrFolderPath);
    LogFlow(("FS32_OPENCREATE: vboxSfOs2ResolvePath: -> %u pFolder=%p\n", rc, pFolder));
    if (rc == NO_ERROR)
    {
        /*
         * Try open the file.
         */
        int vrc = VbglR0SfCreate(&g_SfClient, &pFolder->hHostFolder, pStrFolderPath, pParams);
        LogFlow(("FS32_OPENCREATE: VbglR0SfCreate -> %Rrc Result=%d fMode=%#x\n", vrc, pParams->Result, pParams->Info.Attr.fMode));
        if (RT_SUCCESS(vrc))
        {
            switch (pParams->Result)
            {
                case SHFL_FILE_EXISTS:
                    if (pParams->Handle == SHFL_HANDLE_NIL)
                    {
                        rc = ERROR_FILE_EXISTS;
                        break;
                    }
                    RT_FALL_THRU();
                case SHFL_FILE_CREATED:
                case SHFL_FILE_REPLACED:
                    if (   pParams->Info.cbObject < _2G
                        || (fOpenMode & OPEN_FLAGS_LARGEFILE))
                    {
                        pSfFsd->u32Magic    = VBOXSFSYFI_MAGIC;
                        pSfFsd->pSelf       = pSfFsd;
                        pSfFsd->hHostFile   = pParams->Handle;
                        pSfFsd->pFolder     = pFolder;

                        uint32_t cOpenFiles = ASMAtomicIncU32(&pFolder->cOpenFiles);
                        Assert(cOpenFiles < _32K);
                        pFolder = NULL; /* Reference now taken by pSfFsd->pFolder. */

                        pSfFsi->sfi_sizel   = pParams->Info.cbObject;
                        pSfFsi->sfi_type    = STYPE_FILE;
                        pSfFsi->sfi_DOSattr = (uint8_t)((pParams->Info.Attr.fMode & RTFS_DOS_MASK_OS2) >> RTFS_DOS_SHIFT);
                        int16_t cMinLocalTimeDelta = vboxSfOs2GetLocalTimeDelta();
                        vboxSfOs2DateTimeFromTimeSpec(&pSfFsi->sfi_cdate, &pSfFsi->sfi_ctime, pParams->Info.BirthTime,        cMinLocalTimeDelta);
                        vboxSfOs2DateTimeFromTimeSpec(&pSfFsi->sfi_adate, &pSfFsi->sfi_atime, pParams->Info.AccessTime,       cMinLocalTimeDelta);
                        vboxSfOs2DateTimeFromTimeSpec(&pSfFsi->sfi_mdate, &pSfFsi->sfi_mtime, pParams->Info.ModificationTime, cMinLocalTimeDelta);
                        if (pParams->Result == SHFL_FILE_CREATED)
                            pSfFsi->sfi_tstamp |= ST_PCREAT | ST_SCREAT | ST_PWRITE | ST_SWRITE | ST_PREAD | ST_SREAD;

                        *puAction = pParams->Result == SHFL_FILE_CREATED ? FILE_CREATED
                                  : pParams->Result == SHFL_FILE_EXISTS  ? FILE_EXISTED
                                  :                                        FILE_TRUNCATED;

                        Log(("FS32_OPENCREATE: hHandle=%#RX64 for '%s'\n", pSfFsd->hHostFile, pszName));
                        rc = NO_ERROR;
                    }
                    else
                    {
                        LogRel(("FS32_OPENCREATE: cbObject=%#RX64 no OPEN_FLAGS_LARGEFILE (%s)\n", pParams->Info.cbObject, pszName));
                        VbglR0SfClose(&g_SfClient, &pFolder->hHostFolder, pParams->Handle);
                        rc = ERROR_ACCESS_DENIED;
                    }
                    break;

                case SHFL_PATH_NOT_FOUND:
                    rc = ERROR_PATH_NOT_FOUND;
                    break;

                default:
                case SHFL_FILE_NOT_FOUND:
                    rc = ERROR_FILE_NOT_FOUND;
                    break;
            }
        }
        else if (vrc == VERR_ALREADY_EXISTS)
            rc = ERROR_ACCESS_DENIED;
        else
            rc = vboxSfOs2ConvertStatusToOs2(vrc, ERROR_PATH_NOT_FOUND);
        vboxSfOs2ReleasePathAndFolder(pStrFolderPath, pFolder);
    }
    VbglR0PhysHeapFree(pParams);
    LogFlow(("FS32_OPENCREATE: returns %u\n", rc));
    return rc;
}


DECLASM(APIRET)
FS32_CLOSE(ULONG uType, ULONG fIoFlags, PSFFSI pSfFsi, PVBOXSFSYFI pSfFsd)
{
    LogFlow(("FS32_CLOSE: uType=%#x fIoFlags=%#x pSfFsi=%p pSfFsd=%p:{%#x}\n", uType, fIoFlags, pSfFsi, pSfFsd, pSfFsd->u32Magic));

    /*
     * Validate input.
     */
    AssertReturn(pSfFsd->u32Magic == VBOXSFSYFI_MAGIC, ERROR_SYS_INTERNAL);
    AssertReturn(pSfFsd->pSelf    == pSfFsd, ERROR_SYS_INTERNAL);
    PVBOXSFFOLDER pFolder = pSfFsd->pFolder;
    AssertReturn(pFolder != NULL, ERROR_SYS_INTERNAL);
    Assert(pFolder->u32Magic == VBOXSFFOLDER_MAGIC);
    Assert(pFolder->cOpenFiles > 0);

    /*
     * We only care for when the system is done truly with the file
     * and we can close it.
     */
    if (uType != FS_CL_FORSYS)
        return NO_ERROR;

    /** @todo flush file if fIoFlags says so? */
    RT_NOREF(fIoFlags);

    int vrc = VbglR0SfClose(&g_SfClient, &pFolder->hHostFolder, pSfFsd->hHostFile);
    AssertRC(vrc);

    pSfFsd->hHostFile = SHFL_HANDLE_NIL;
    pSfFsd->pSelf     = NULL;
    pSfFsd->u32Magic  = ~VBOXSFSYFI_MAGIC;
    pSfFsd->pFolder   = NULL;

    ASMAtomicDecU32(&pFolder->cOpenFiles);
    vboxSfOs2ReleaseFolder(pFolder);

    RT_NOREF(pSfFsi);
    LogFlow(("FS32_CLOSE: returns NO_ERROR\n"));
    return NO_ERROR;
}


DECLASM(APIRET)
FS32_COMMIT(ULONG uType, ULONG fIoFlags, PSFFSI pSfFsi, PVBOXSFSYFI pSfFsd)
{
    LogFlow(("FS32_COMMIT: uType=%#x fIoFlags=%#x pSfFsi=%p pSfFsd=%p:{%#x}\n", uType, fIoFlags, pSfFsi, pSfFsd, pSfFsd->u32Magic));

    /*
     * Validate input.
     */
    AssertReturn(pSfFsd->u32Magic == VBOXSFSYFI_MAGIC, ERROR_SYS_INTERNAL);
    AssertReturn(pSfFsd->pSelf    == pSfFsd, ERROR_SYS_INTERNAL);
    PVBOXSFFOLDER pFolder = pSfFsd->pFolder;
    AssertReturn(pFolder != NULL, ERROR_SYS_INTERNAL);
    Assert(pFolder->u32Magic == VBOXSFFOLDER_MAGIC);
    Assert(pFolder->cOpenFiles > 0);
    RT_NOREF(pFolder);

    /*
     * We only need to flush writable files.
     */
    if (   (pSfFsi->sfi_mode & SFMODE_OPEN_ACCESS) == SFMODE_OPEN_WRITEONLY
        || (pSfFsi->sfi_mode & SFMODE_OPEN_ACCESS) == SFMODE_OPEN_READWRITE)
    {
        int vrc = VbglR0SfFlush(&g_SfClient, &pFolder->hHostFolder, pSfFsd->hHostFile);
        if (RT_FAILURE(vrc))
        {
            LogRel(("FS32_COMMIT: VbglR0SfFlush failed: %Rrc\n", vrc));
            return ERROR_FLUSHBUF_FAILED;
        }
    }

    NOREF(uType); NOREF(fIoFlags); NOREF(pSfFsi);
    LogFlow(("FS32_COMMIT: returns NO_ERROR\n"));
    return NO_ERROR;
}


extern "C" APIRET APIENTRY
FS32_CHGFILEPTRL(PSFFSI pSfFsi, PVBOXSFSYFI pSfFsd, LONGLONG off, ULONG uMethod, ULONG fIoFlags)
{
    LogFlow(("FS32_CHGFILEPTRL: pSfFsi=%p pSfFsd=%p off=%RI64 (%#RX64) uMethod=%u fIoFlags=%#x\n",
             pSfFsi, pSfFsd, off, off, uMethod, fIoFlags));

    /*
     * Validate input.
     */
    AssertReturn(pSfFsd->u32Magic == VBOXSFSYFI_MAGIC, ERROR_SYS_INTERNAL);
    AssertReturn(pSfFsd->pSelf    == pSfFsd, ERROR_SYS_INTERNAL);
    PVBOXSFFOLDER pFolder = pSfFsd->pFolder;
    AssertReturn(pFolder != NULL, ERROR_SYS_INTERNAL);
    Assert(pFolder->u32Magic == VBOXSFFOLDER_MAGIC);
    Assert(pFolder->cOpenFiles > 0);

    /*
     * Calc absolute offset.
     */
    int64_t offNew;
    switch (uMethod)
    {
        case CFP_RELBEGIN:
            if (off >= 0)
            {
                offNew = off;
                break;
            }
            Log(("FS32_CHGFILEPTRL: Negative seek (BEGIN): %RI64\n", off));
            return ERROR_NEGATIVE_SEEK;

        case CFP_RELCUR:
            offNew = pSfFsi->sfi_positionl + off;
            if (offNew >= 0)
                break;
            Log(("FS32_CHGFILEPTRL: Negative seek (RELCUR): %RU64 + %RI64\n", pSfFsi->sfi_positionl, off));
            return ERROR_NEGATIVE_SEEK;

        case CFP_RELEND:
        {
            /* Have to consult the host to get the current file size. */

            PSHFLFSOBJINFO pObjInfo = (PSHFLFSOBJINFO)VbglR0PhysHeapAlloc(sizeof(*pObjInfo));
            if (!pObjInfo)
                return ERROR_NOT_ENOUGH_MEMORY;
            RT_ZERO(*pObjInfo);
            uint32_t cbObjInfo = sizeof(*pObjInfo);

            int vrc = VbglR0SfFsInfo(&g_SfClient, &pFolder->hHostFolder, pSfFsd->hHostFile,
                                     SHFL_INFO_FILE | SHFL_INFO_GET, &cbObjInfo, (PSHFLDIRINFO)pObjInfo);
            if (RT_SUCCESS(vrc))
            {
                if (pSfFsi->sfi_mode & SFMODE_LARGE_FILE)
                    pSfFsi->sfi_sizel = pObjInfo->cbObject;
                else
                    pSfFsi->sfi_sizel = RT_MIN(pObjInfo->cbObject, _2G - 1);
            }
            else
                LogRel(("FS32_CHGFILEPTRL/CFP_RELEND: VbglR0SfFsInfo failed: %Rrc\n", vrc));
            VbglR0PhysHeapFree(pObjInfo);

            offNew = pSfFsi->sfi_sizel + off;
            if (offNew >= 0)
                break;
            Log(("FS32_CHGFILEPTRL: Negative seek (CFP_RELEND): %RI64 + %RI64\n", pSfFsi->sfi_sizel, off));
            return ERROR_NEGATIVE_SEEK;
        }


        default:
            LogRel(("FS32_CHGFILEPTRL: Unknown seek method: %#x\n", uMethod));
            return ERROR_INVALID_FUNCTION;
    }

    /*
     * Commit the seek.
     */
    pSfFsi->sfi_positionl = offNew;
    LogFlow(("FS32_CHGFILEPTRL: returns; sfi_positionl=%RI64\n", offNew));
    RT_NOREF_PV(fIoFlags);
    return NO_ERROR;
}


/** Forwards the call to FS32_CHGFILEPTRL. */
DECLASM(APIRET)
FS32_CHGFILEPTR(PSFFSI pSfFsi, PVBOXSFSYFI pSfFsd, LONG off, ULONG uMethod, ULONG fIoFlags)
{
    return FS32_CHGFILEPTRL(pSfFsi, pSfFsd, off, uMethod, fIoFlags);
}


/**
 * Worker for FS32_PATHINFO that handles file stat setting.
 *
 * @returns OS/2 status code
 * @param   pFolder         The folder.
 * @param   pSfFsi          The file system independent file structure.  We'll
 *                          update the timestamps and size here.
 * @param   pSfFsd          Out file data.
 * @param   uLevel          The information level.
 * @param   pbData          The stat data to set.
 * @param   cbData          The uLevel specific input size.
 */
static APIRET
vboxSfOs2SetFileInfo(PVBOXSFFOLDER pFolder, PSFFSI pSfFsi, PVBOXSFSYFI pSfFsd, ULONG uLevel, PBYTE pbData, ULONG cbData)
{
    APIRET rc;

    /*
     * Data buffer both for caching user data and for issuing the
     * change request to the host.
     */
    struct SetFileInfoBuf
    {
        union
        {
            FILESTATUS      Lvl1;
            FILESTATUS3L    Lvl1L;
        };
        SHFLFSOBJINFO ObjInfo;

    } *pBuf = (struct SetFileInfoBuf *)VbglR0PhysHeapAlloc(sizeof(*pBuf));
    if (pBuf)
    {
        /* Copy in the data. */
        rc = KernCopyIn(&pBuf->Lvl1, pbData, cbData);
        if (rc == NO_ERROR)
        {
            /*
             * Join paths with FS32_PATHINFO and FS32_FILEATTRIBUTE.
             */
            rc = vboxSfOs2SetInfoCommonWorker(pFolder, pSfFsd->hHostFile,
                                              uLevel == FI_LVL_STANDARD ? pBuf->Lvl1.attrFile : pBuf->Lvl1L.attrFile,
                                              &pBuf->Lvl1, &pBuf->ObjInfo);
            if (rc == NO_ERROR)
            {
                /*
                 * Update the timestamps in the independent file data with what
                 * the host returned:
                 */
                pSfFsi->sfi_tstamp |= ST_PCREAT | ST_PWRITE | ST_PREAD;
                pSfFsi->sfi_tstamp &= ~(ST_SCREAT | ST_SWRITE| ST_SREAD);
                uint16_t cDelta = vboxSfOs2GetLocalTimeDelta();
                vboxSfOs2DateTimeFromTimeSpec(&pSfFsi->sfi_cdate, &pSfFsi->sfi_ctime, pBuf->ObjInfo.BirthTime,        cDelta);
                vboxSfOs2DateTimeFromTimeSpec(&pSfFsi->sfi_adate, &pSfFsi->sfi_atime, pBuf->ObjInfo.AccessTime,       cDelta);
                vboxSfOs2DateTimeFromTimeSpec(&pSfFsi->sfi_mdate, &pSfFsi->sfi_mtime, pBuf->ObjInfo.ModificationTime, cDelta);

                /* And the size field as we're at it: */
                pSfFsi->sfi_sizel = pBuf->ObjInfo.cbObject;
            }
            else
                rc = ERROR_INVALID_PARAMETER;
        }

        VbglR0PhysHeapFree(pBuf);
    }
    else
        rc = ERROR_NOT_ENOUGH_MEMORY;
    return rc;
}

#include <VBox/VBoxGuest.h>




DECLVBGL(int) VbglR0SfFastPhysFsInfo(PVBGLSFCLIENT pClient, PVBGLSFMAP pMap, SHFLHANDLE hFile,
                                     uint32_t flags, uint32_t *pcbBuffer, PSHFLDIRINFO pBuffer)
{
    struct FsInfoReq
    {
        VBGLIOCIDCHGCMFASTCALL  Hdr;
        VMMDevHGCMCall          Call;
        VBoxSFParmInformation   Parms;
        HGCMPageListInfo        PgLst;
        RTGCPHYS64              PageTwo;
    } *pReq;
    AssertCompileMemberOffset(struct FsInfoReq, Call, 52);
    AssertCompileMemberOffset(struct FsInfoReq, Parms, 0x60);

    pReq = (struct FsInfoReq *)VbglR0PhysHeapAlloc(sizeof(*pReq));
    if (!pReq)
        return VERR_NO_MEMORY;

    VBGLIOCIDCHGCMFASTCALL_INIT(&pReq->Hdr, VbglR0PhysHeapGetPhysAddr(pReq), &pReq->Call, pClient->idClient,
                                SHFL_FN_INFORMATION, SHFL_CPARMS_INFORMATION, sizeof(*pReq));
#if 0
    VBGLREQHDR_INIT_EX(&pReq->Hdr.Hdr, sizeof(*pReq), sizeof(*pReq));
    pReq->Hdr.GCPhysReq      = VbglR0PhysHeapGetPhysAddr(pReq) + sizeof(pReq->Hdr);
    pReq->Hdr.fInterruptible = false;

    pReq->Call.header.header.size       = sizeof(*pReq) - sizeof(pReq->Hdr);
    pReq->Call.header.header.version    = VBGLREQHDR_VERSION;
    pReq->Call.header.header.requestType= VMMDevReq_HGCMCall32;
    pReq->Call.header.header.rc         = VERR_INTERNAL_ERROR;
    pReq->Call.header.header.reserved1  = 0;
    pReq->Call.header.header.fRequestor = VMMDEV_REQUESTOR_KERNEL        | VMMDEV_REQUESTOR_USR_DRV_OTHER
                                        | VMMDEV_REQUESTOR_CON_DONT_KNOW | VMMDEV_REQUESTOR_TRUST_NOT_GIVEN;
    pReq->Call.header.fu32Flags         = 0;
    pReq->Call.header.result            = VERR_INTERNAL_ERROR;
    pReq->Call.u32ClientID              = pClient->idClient;
    pReq->Call.u32Function              = SHFL_FN_INFORMATION;
    pReq->Call.cParms                   = SHFL_CPARMS_INFORMATION;
#endif
    uint32_t const cbBuffer = *pcbBuffer;
    pReq->Parms.id32Root.type           = VMMDevHGCMParmType_32bit;
    pReq->Parms.id32Root.u.value32      = pMap->root;
    pReq->Parms.u64Handle.type          = VMMDevHGCMParmType_64bit;
    pReq->Parms.u64Handle.u.value64     = hFile;
    pReq->Parms.f32Flags.type           = VMMDevHGCMParmType_32bit;
    pReq->Parms.f32Flags.u.value32      = flags;
    pReq->Parms.cb32.type               = VMMDevHGCMParmType_32bit;
    pReq->Parms.cb32.u.value32          = cbBuffer;
    pReq->Parms.pInfo.type              = VMMDevHGCMParmType_PageList;
    pReq->Parms.pInfo.u.PageList.size   = cbBuffer;
    pReq->Parms.pInfo.u.PageList.offset = RT_UOFFSETOF(struct FsInfoReq, PgLst) - RT_UOFFSETOF(struct FsInfoReq, Call);

    Assert(cbBuffer < _1K);
    pReq->PgLst.flags                   = VBOX_HGCM_F_PARM_DIRECTION_FROM_HOST;
    pReq->PgLst.cPages                  = cbBuffer <= (PAGE_SIZE - ((uintptr_t)pBuffer & PAGE_OFFSET_MASK)) ? 1 : 2;
    pReq->PgLst.offFirstPage            = (uint16_t)((uintptr_t)pBuffer & PAGE_OFFSET_MASK);
    pReq->PgLst.aPages[0]               = VbglR0PhysHeapGetPhysAddr(pBuffer) & ~(RTGCPHYS64)PAGE_OFFSET_MASK;
    if (pReq->PgLst.cPages == 1)
        pReq->PageTwo                   = NIL_RTGCPHYS64;
    else
        pReq->PageTwo                   = pReq->PgLst.aPages[0] + PAGE_SIZE;

    int rc = VbglR0HGCMFastCall(pClient->handle, &pReq->Hdr, sizeof(*pReq));
    if (RT_SUCCESS(rc))
    {
        rc = pReq->Call.header.result;
        *pcbBuffer = pReq->Parms.cb32.u.value32;
    }
    VbglR0PhysHeapFree(pReq);
    return rc;
}


DECLVBGL(int) VbglR0SfPhysFsInfo(PVBGLSFCLIENT pClient, PVBGLSFMAP pMap, SHFLHANDLE hFile,
                                 uint32_t flags, uint32_t *pcbBuffer, PSHFLDIRINFO pBuffer)
{
    uint32_t const cbBuffer = *pcbBuffer;

    struct
    {
        VBoxSFInformation   Core;
        HGCMPageListInfo    PgLst;
        RTGCPHYS64          PageTwo;
    } Req;

    VBGL_HGCM_HDR_INIT_EX(&Req.Core.callInfo, pClient->idClient, SHFL_FN_INFORMATION, SHFL_CPARMS_INFORMATION, sizeof(Req));
    Req.Core.callInfo.fInterruptible = false;

    Req.Core.root.type                      = VMMDevHGCMParmType_32bit;
    Req.Core.root.u.value32                 = pMap->root;

    Req.Core.handle.type                    = VMMDevHGCMParmType_64bit;
    Req.Core.handle.u.value64               = hFile;
    Req.Core.flags.type                     = VMMDevHGCMParmType_32bit;
    Req.Core.flags.u.value32                = flags;
    Req.Core.cb.type                        = VMMDevHGCMParmType_32bit;
    Req.Core.cb.u.value32                   = cbBuffer;
    Req.Core.info.type                      = VMMDevHGCMParmType_PageList;
    Req.Core.info.u.PageList.size           = cbBuffer;
    Req.Core.info.u.PageList.offset         = sizeof(Req.Core);

    Assert(cbBuffer < _1K);
    Req.PgLst.flags                         = VBOX_HGCM_F_PARM_DIRECTION_FROM_HOST;
    Req.PgLst.cPages                        = cbBuffer <= (PAGE_SIZE - ((uintptr_t)pBuffer & PAGE_OFFSET_MASK)) ? 1 : 2;
    Req.PgLst.offFirstPage                  = (uint16_t)((uintptr_t)pBuffer & PAGE_OFFSET_MASK);
    Req.PgLst.aPages[0]                     = VbglR0PhysHeapGetPhysAddr(pBuffer) & ~(RTGCPHYS64)PAGE_OFFSET_MASK;
    if (Req.PgLst.cPages == 1)
        Req.PageTwo                         = NIL_RTGCPHYS64;
    else
        Req.PageTwo                         = Req.PgLst.aPages[0] + PAGE_SIZE;

    int rc = VbglR0HGCMCallRaw(pClient->handle, &Req.Core.callInfo, sizeof(Req));
    //Log(("VBOXSF: VbglR0SfFsInfo: VbglR0HGCMCall rc = %#x, result = %#x\n", rc, data.callInfo.Hdr.rc));
    if (RT_SUCCESS(rc))
    {
        rc = Req.Core.callInfo.Hdr.rc;
        *pcbBuffer = Req.Core.cb.u.value32;
    }
    return rc;
}


/**
 * Worker for FS32_PATHINFO that handles file stat queries.
 *
 * @returns OS/2 status code
 * @param   pFolder         The folder.
 * @param   pSfFsi          The file system independent file structure.  We'll
 *                          update the timestamps and size here.
 * @param   pSfFsd          Out file data.
 * @param   uLevel          The information level.
 * @param   pbData          Where to return the data (user address).
 * @param   cbData          The amount of data to produce.
 */
static APIRET
vboxSfOs2QueryFileInfo(PVBOXSFFOLDER pFolder, PSFFSI pSfFsi, PVBOXSFSYFI pSfFsd, ULONG uLevel, PBYTE pbData, ULONG cbData)
{
    /*
     * Performance notes:
     *
     * This function was used for some performance hacking in an attempt at
     * squeezing more performance out of the HGCM and shared folders code.
     *
     * 0. Skip calling the host and returning zeros:
     *       906 ns / 3653 ticks
     *
     *    This is comparable to JFS (859 ns) and HPFS (1107 ns) and give an
     *    idea what we're up against compared to a "local" file system.
     *
     * 1. Having shortcircuted the host side processing by faking a success when
     *    VMMDevHGCM.cpp is about to do pThis->pHGCMDrv->pfnCall, then measuring
     *    various guest side changes in the request and request submission path:
     *
     *     - Wasted on virtual address vs page list buffer:
     *         4095 ns / 16253 ticks
     *
     *       Suspect this is due to expensive memory locking on the guest side and
     *       the host doing extra virtual address conversion.
     *
     *     - Wasted repackaging the HGCM request:
     *         450 ns / 1941 ticks
     *
     *     - Embedding the SHFLFSOBJINFO into the buffer may save a little as well:
     *         286 ns / 1086 ticks
     *
     *    Raw data:
     *        11843ns / 47469 ticks - VbglR0SfFsInfo
     *         7748ns / 31216 ticks - VbglR0SfPhysFsInfo
     *         7298ns / 29275 ticks - VbglR0SfFastPhysFsInfo
     *         7012ns / 28189 ticks - Embedded buffer.
     *
     * 2. should've done measurements of the host side optimizations too...
     *
     */
#if 0
    APIRET rc;
    PSHFLFSOBJINFO pObjInfo = (PSHFLFSOBJINFO)VbglR0PhysHeapAlloc(sizeof(*pObjInfo));
    if (pObjInfo)
    {
        RT_ZERO(*pObjInfo);
        uint32_t cbObjInfo = sizeof(*pObjInfo);

        int vrc = VbglR0SfFastPhysFsInfo(&g_SfClient, &pFolder->hHostFolder, pSfFsd->hHostFile,
                                         SHFL_INFO_FILE | SHFL_INFO_GET, &cbObjInfo, (PSHFLDIRINFO)pObjInfo);
        if (RT_SUCCESS(vrc))
        {
            rc = vboxSfOs2FileStatusFromObjInfo(pbData, cbData, uLevel, pObjInfo);
            if (rc == NO_ERROR)
            {
                /* Update the timestamps in the independent file data: */
                int16_t cMinLocalTimeDelta = vboxSfOs2GetLocalTimeDelta();
                vboxSfOs2DateTimeFromTimeSpec(&pSfFsi->sfi_cdate, &pSfFsi->sfi_ctime, pObjInfo->BirthTime,        cMinLocalTimeDelta);
                vboxSfOs2DateTimeFromTimeSpec(&pSfFsi->sfi_adate, &pSfFsi->sfi_atime, pObjInfo->AccessTime,       cMinLocalTimeDelta);
                vboxSfOs2DateTimeFromTimeSpec(&pSfFsi->sfi_mdate, &pSfFsi->sfi_mtime, pObjInfo->ModificationTime, cMinLocalTimeDelta);

                /* And the size field as we're at it: */
                pSfFsi->sfi_sizel = pObjInfo->cbObject;
            }
        }
        else
        {
            Log(("vboxSfOs2QueryFileInfo: VbglR0SfFsInfo failed: %Rrc\n", vrc));
            rc = vboxSfOs2ConvertStatusToOs2(vrc, ERROR_GEN_FAILURE);
        }
        VbglR0PhysHeapFree(pObjInfo);
    }
    else
        rc = ERROR_NOT_ENOUGH_MEMORY;
#else
    APIRET rc;
    struct MyEmbReq
    {
        VBGLIOCIDCHGCMFASTCALL  Hdr;
        VMMDevHGCMCall          Call;
        VBoxSFParmInformation   Parms;
        SHFLFSOBJINFO           ObjInfo;
    } *pReq = (struct MyEmbReq *)VbglR0PhysHeapAlloc(sizeof(*pReq));
    if (pReq)
    {
        RT_ZERO(pReq->ObjInfo);

        VBGLIOCIDCHGCMFASTCALL_INIT(&pReq->Hdr, VbglR0PhysHeapGetPhysAddr(pReq), &pReq->Call, g_SfClient.idClient,
                                    SHFL_FN_INFORMATION, SHFL_CPARMS_INFORMATION, sizeof(*pReq));
        pReq->Parms.id32Root.type               = VMMDevHGCMParmType_32bit;
        pReq->Parms.id32Root.u.value32          = pFolder->hHostFolder.root;
        pReq->Parms.u64Handle.type              = VMMDevHGCMParmType_64bit;
        pReq->Parms.u64Handle.u.value64         = pSfFsd->hHostFile;
        pReq->Parms.f32Flags.type               = VMMDevHGCMParmType_32bit;
        pReq->Parms.f32Flags.u.value32          = SHFL_INFO_FILE | SHFL_INFO_GET;
        pReq->Parms.cb32.type                   = VMMDevHGCMParmType_32bit;
        pReq->Parms.cb32.u.value32              = sizeof(pReq->ObjInfo);
        pReq->Parms.pInfo.type                  = VMMDevHGCMParmType_Embedded;
        pReq->Parms.pInfo.u.Embedded.cbData     = sizeof(pReq->ObjInfo);
        pReq->Parms.pInfo.u.Embedded.offData    = RT_UOFFSETOF(struct MyEmbReq, ObjInfo) - sizeof(VBGLIOCIDCHGCMFASTCALL);
        pReq->Parms.pInfo.u.Embedded.fFlags     = VBOX_HGCM_F_PARM_DIRECTION_FROM_HOST;

        int vrc = VbglR0HGCMFastCall(g_SfClient.handle, &pReq->Hdr, sizeof(*pReq));
        if (RT_SUCCESS(vrc))
            vrc = pReq->Call.header.result;
        if (RT_SUCCESS(vrc))
        {
            rc = vboxSfOs2FileStatusFromObjInfo(pbData, cbData, uLevel, &pReq->ObjInfo);
            if (rc == NO_ERROR)
            {
                /* Update the timestamps in the independent file data: */
                int16_t cMinLocalTimeDelta = vboxSfOs2GetLocalTimeDelta();
                vboxSfOs2DateTimeFromTimeSpec(&pSfFsi->sfi_cdate, &pSfFsi->sfi_ctime, pReq->ObjInfo.BirthTime,        cMinLocalTimeDelta);
                vboxSfOs2DateTimeFromTimeSpec(&pSfFsi->sfi_adate, &pSfFsi->sfi_atime, pReq->ObjInfo.AccessTime,       cMinLocalTimeDelta);
                vboxSfOs2DateTimeFromTimeSpec(&pSfFsi->sfi_mdate, &pSfFsi->sfi_mtime, pReq->ObjInfo.ModificationTime, cMinLocalTimeDelta);

                /* And the size field as we're at it: */
                pSfFsi->sfi_sizel = pReq->ObjInfo.cbObject;
            }
        }
        else
        {
            Log(("vboxSfOs2QueryFileInfo: VbglR0SfFsInfo failed: %Rrc\n", vrc));
            rc = vboxSfOs2ConvertStatusToOs2(vrc, ERROR_GEN_FAILURE);
        }

        VbglR0PhysHeapFree(pReq);
    }
    else
        rc = ERROR_NOT_ENOUGH_MEMORY;
#endif
    return rc;
}


DECLASM(APIRET)
FS32_FILEINFO(ULONG fFlags, PSFFSI pSfFsi, PVBOXSFSYFI pSfFsd, ULONG uLevel,
              PBYTE pbData, ULONG cbData, ULONG fIoFlags)
{
    LogFlow(("FS32_FILEINFO: fFlags=%#x pSfFsi=%p pSfFsd=%p uLevel=%p pbData=%p cbData=%#x fIoFlags=%#x\n",
             fFlags, pSfFsi, pSfFsd, uLevel, pbData, cbData, fIoFlags));

    /*
     * Validate input.
     */
    AssertReturn(pSfFsd->u32Magic == VBOXSFSYFI_MAGIC, ERROR_SYS_INTERNAL);
    AssertReturn(pSfFsd->pSelf    == pSfFsd, ERROR_SYS_INTERNAL);
    PVBOXSFFOLDER pFolder = pSfFsd->pFolder;
    AssertReturn(pFolder != NULL, ERROR_SYS_INTERNAL);
    Assert(pFolder->u32Magic == VBOXSFFOLDER_MAGIC);
    Assert(pFolder->cOpenFiles > 0);

    /*
     * Check the level.
     * Note! See notes in FS32_PATHINFO.
     */
    ULONG cbMinData;
    switch (uLevel)
    {
        case FI_LVL_STANDARD:
            cbMinData = sizeof(FILESTATUS);
            AssertCompileSize(FILESTATUS,  0x16);
            break;
        case FI_LVL_STANDARD_64:
            cbMinData = sizeof(FILESTATUS3L);
            AssertCompileSize(FILESTATUS3L, 0x20); /* cbFile and cbFileAlloc are misaligned. */
            break;
        case FI_LVL_STANDARD_EASIZE:
            cbMinData = sizeof(FILESTATUS2);
            AssertCompileSize(FILESTATUS2, 0x1a);
            break;
        case FI_LVL_STANDARD_EASIZE_64:
            cbMinData = sizeof(FILESTATUS4L);
            AssertCompileSize(FILESTATUS4L, 0x24); /* cbFile and cbFileAlloc are misaligned. */
            break;
        case FI_LVL_EAS_FROM_LIST:
        case FI_LVL_EAS_FULL:
        case FI_LVL_EAS_FULL_5:
        case FI_LVL_EAS_FULL_8:
            cbMinData = sizeof(EAOP);
            break;
        default:
            LogRel(("FS32_PATHINFO: Unsupported info level %u!\n", uLevel));
            return ERROR_INVALID_LEVEL;
    }
    if (cbData < cbMinData || pbData == NULL)
    {
        Log(("FS32_FILEINFO: ERROR_BUFFER_OVERFLOW (cbMinData=%#x, cbData=%#x)\n", cbMinData, cbData));
        return ERROR_BUFFER_OVERFLOW;
    }

    /*
     * Query information.
     */
    APIRET rc;
    if (fFlags == FI_RETRIEVE)
    {
        switch (uLevel)
        {
            case FI_LVL_STANDARD:
            case FI_LVL_STANDARD_EASIZE:
            case FI_LVL_STANDARD_64:
            case FI_LVL_STANDARD_EASIZE_64:
                rc = vboxSfOs2QueryFileInfo(pFolder, pSfFsi, pSfFsd, uLevel, pbData, cbMinData);
                break;

            /*
             * We don't do EAs and we "just" need to return no-EAs.
             * However, that's not as easy as you might think.
             */
            case FI_LVL_EAS_FROM_LIST:
            case FI_LVL_EAS_FULL:
            case FI_LVL_EAS_FULL_5:
            case FI_LVL_EAS_FULL_8:
                rc = vboxSfOs2MakeEmptyEaList((PEAOP)pbData, uLevel);
                break;

            default:
                AssertFailed();
                rc = ERROR_GEN_FAILURE;
                break;
        }
    }
    /*
     * Update information.
     */
    else if (fFlags == FI_SET)
    {
        switch (uLevel)
        {
            case FI_LVL_STANDARD:
            case FI_LVL_STANDARD_64:
                rc = vboxSfOs2SetFileInfo(pFolder, pSfFsi, pSfFsd, uLevel, pbData, cbMinData);
                break;

            case FI_LVL_STANDARD_EASIZE:
                rc = ERROR_EAS_NOT_SUPPORTED;
                break;

            case FI_LVL_STANDARD_EASIZE_64:
            case FI_LVL_EAS_FROM_LIST:
            case FI_LVL_EAS_FULL:
            case FI_LVL_EAS_FULL_5:
            case FI_LVL_EAS_FULL_8:
                rc = ERROR_INVALID_LEVEL;
                break;

            default:
                AssertFailed();
                rc = ERROR_GEN_FAILURE;
                break;
        }
    }
    else
    {
        LogRel(("FS32_FILEINFO: Unknown flags value: %#x\n", fFlags));
        rc = ERROR_INVALID_PARAMETER;
    }
    RT_NOREF_PV(fIoFlags);
    return rc;
}


DECLASM(APIRET)
FS32_NEWSIZEL(PSFFSI pSfFsi, PVBOXSFSYFI pSfFsd, LONGLONG cbFile, ULONG fIoFlags)
{
    LogFlow(("FS32_NEWSIZEL: pSfFsi=%p pSfFsd=%p cbFile=%RI64 (%#RX64) fIoFlags=%#x\n", pSfFsi, pSfFsd, cbFile, cbFile, fIoFlags));

    /*
     * Validate input.
     */
    AssertReturn(pSfFsd->u32Magic == VBOXSFSYFI_MAGIC, ERROR_SYS_INTERNAL);
    AssertReturn(pSfFsd->pSelf    == pSfFsd, ERROR_SYS_INTERNAL);
    PVBOXSFFOLDER pFolder = pSfFsd->pFolder;
    AssertReturn(pFolder != NULL, ERROR_SYS_INTERNAL);
    Assert(pFolder->u32Magic == VBOXSFFOLDER_MAGIC);
    Assert(pFolder->cOpenFiles > 0);
    if (cbFile < 0)
    {
        LogRel(("FS32_NEWSIZEL: Negative size: %RI64\n", cbFile));
        return ERROR_INVALID_PARAMETER;
    }

    /*
     * This should only be possible on a file that is writable.
     */
    APIRET rc;
    if (   (pSfFsi->sfi_mode & SFMODE_OPEN_ACCESS) == SFMODE_OPEN_WRITEONLY
        || (pSfFsi->sfi_mode & SFMODE_OPEN_ACCESS) == SFMODE_OPEN_READWRITE)
    {
        /*
         * Call the host.  We need a full object info structure here to pass
         * a 64-bit unsigned integer value.  Sigh.
         */
        /** @todo Shared folders: New SET_FILE_SIZE API. */
        PSHFLFSOBJINFO pObjInfo = (PSHFLFSOBJINFO)VbglR0PhysHeapAlloc(sizeof(*pObjInfo));
        if (pObjInfo)
        {
            RT_ZERO(*pObjInfo);
            pObjInfo->cbObject = cbFile;
            uint32_t cbObjInfo = sizeof(*pObjInfo);
            int vrc = VbglR0SfFsInfo(&g_SfClient, &pFolder->hHostFolder, pSfFsd->hHostFile,
                                     SHFL_INFO_SIZE | SHFL_INFO_SET, &cbObjInfo, (PSHFLDIRINFO)pObjInfo);
            if (RT_SUCCESS(vrc))
            {
                pSfFsi->sfi_sizel = cbFile;
                rc = NO_ERROR;
            }
            else
            {
                LogRel(("FS32_NEWSIZEL: VbglR0SfFsInfo failed: %Rrc\n", vrc));
                if (vrc == VERR_DISK_FULL)
                    rc = ERROR_DISK_FULL;
                else
                    rc = ERROR_GEN_FAILURE;
            }
            VbglR0PhysHeapFree(pObjInfo);
        }
        else
            rc = ERROR_NOT_ENOUGH_MEMORY;
    }
    else
        rc = ERROR_ACCESS_DENIED;
    LogFlow(("FS32_NEWSIZEL: returns %u\n", rc));
    return rc;
}


extern "C" APIRET APIENTRY
FS32_READ(PSFFSI pSfFsi, PVBOXSFSYFI pSfFsd, PVOID pvData, PULONG pcb, ULONG fIoFlags)
{
    LogFlow(("FS32_READ: pSfFsi=%p pSfFsd=%p pvData=%p pcb=%p:{%#x} fIoFlags=%#x\n", pSfFsi, pSfFsd, pvData, pcb, *pcb, fIoFlags));

    /*
     * Validate input.
     */
    AssertReturn(pSfFsd->u32Magic == VBOXSFSYFI_MAGIC, ERROR_SYS_INTERNAL);
    AssertReturn(pSfFsd->pSelf    == pSfFsd, ERROR_SYS_INTERNAL);
    PVBOXSFFOLDER pFolder = pSfFsd->pFolder;
    AssertReturn(pFolder != NULL, ERROR_SYS_INTERNAL);
    Assert(pFolder->u32Magic == VBOXSFFOLDER_MAGIC);
    Assert(pFolder->cOpenFiles > 0);
    RT_NOREF(pFolder);

    /*
     * If the read request is small enough, go thru a temporary buffer to
     * avoid locking/unlocking user memory.
     */
    uint64_t offRead  = pSfFsi->sfi_positionl;
    uint32_t cbRead   = *pcb;
    uint32_t cbActual = cbRead;
    if (cbRead <= _8K - ALLOC_HDR_SIZE)
    {
        void *pvBuf = VbglR0PhysHeapAlloc(cbRead);
        if (pvBuf != NULL)
        {
            APIRET rc;
            int vrc = VbglR0SfRead(&g_SfClient, &pFolder->hHostFolder, pSfFsd->hHostFile,
                                   offRead, &cbActual, (uint8_t *)pvBuf, true /*fLocked*/);
            if (RT_SUCCESS(vrc))
            {
                AssertStmt(cbActual <= cbRead, cbActual = cbRead);
                rc = KernCopyOut(pvData, pvBuf, cbActual);
                if (rc == NO_ERROR)
                {
                    *pcb = cbActual;
                    pSfFsi->sfi_positionl = offRead + cbActual;
                    if ((uint64_t)pSfFsi->sfi_sizel < offRead + cbActual)
                        pSfFsi->sfi_sizel = offRead + cbActual;
                    pSfFsi->sfi_tstamp   |= ST_SREAD | ST_PREAD;
                    LogFlow(("FS32_READ: returns; cbActual=%#x sfi_positionl=%RI64 [copy]\n", cbActual, pSfFsi->sfi_positionl));
                }
            }
            else
            {
                Log(("FS32_READ: VbglR0SfRead(off=%#x,cb=%#x) -> %Rrc [copy]\n", offRead, cbRead, vrc));
                rc = ERROR_BAD_NET_RESP;
            }
            VbglR0PhysHeapFree(pvBuf);
            return rc;
        }
    }

    /*
     * Do the read directly on the buffer, Vbgl will do the locking for us.
     */
    int vrc = VbglR0SfRead(&g_SfClient, &pFolder->hHostFolder, pSfFsd->hHostFile,
                           offRead, &cbActual, (uint8_t *)pvData, false /*fLocked*/);
    if (RT_SUCCESS(vrc))
    {
        AssertStmt(cbActual <= cbRead, cbActual = cbRead);
        *pcb = cbActual;
        pSfFsi->sfi_positionl = offRead + cbActual;
        if ((uint64_t)pSfFsi->sfi_sizel < offRead + cbActual)
            pSfFsi->sfi_sizel = offRead + cbActual;
        pSfFsi->sfi_tstamp   |= ST_SREAD | ST_PREAD;
        LogFlow(("FS32_READ: returns; cbActual=%#x sfi_positionl=%RI64 [direct]\n", cbActual, pSfFsi->sfi_positionl));
        return NO_ERROR;
    }
    Log(("FS32_READ: VbglR0SfRead(off=%#x,cb=%#x) -> %Rrc [direct]\n", offRead, cbRead, vrc));
    RT_NOREF_PV(fIoFlags);
    return ERROR_BAD_NET_RESP;
}


extern "C" APIRET APIENTRY
FS32_WRITE(PSFFSI pSfFsi, PVBOXSFSYFI pSfFsd, void const *pvData, PULONG pcb, ULONG fIoFlags)
{
    /*
     * Validate input.
     */
    AssertReturn(pSfFsd->u32Magic == VBOXSFSYFI_MAGIC, ERROR_SYS_INTERNAL);
    AssertReturn(pSfFsd->pSelf    == pSfFsd, ERROR_SYS_INTERNAL);
    PVBOXSFFOLDER pFolder = pSfFsd->pFolder;
    AssertReturn(pFolder != NULL, ERROR_SYS_INTERNAL);
    Assert(pFolder->u32Magic == VBOXSFFOLDER_MAGIC);
    Assert(pFolder->cOpenFiles > 0);
    RT_NOREF(pFolder);

    /*
     * If the write request is small enough, go thru a temporary buffer to
     * avoid locking/unlocking user memory.
     */
    uint64_t offWrite = pSfFsi->sfi_positionl;
    uint32_t cbWrite  = *pcb;
    uint32_t cbActual = cbWrite;
    if (cbWrite <= _8K - ALLOC_HDR_SIZE)
    {
        void *pvBuf = VbglR0PhysHeapAlloc(cbWrite);
        if (pvBuf != NULL)
        {
            APIRET rc = KernCopyIn(pvBuf, pvData, cbWrite);
            if (rc == NO_ERROR)
            {
                int vrc = VbglR0SfWrite(&g_SfClient, &pFolder->hHostFolder, pSfFsd->hHostFile,
                                        offWrite, &cbActual, (uint8_t *)pvBuf, true /*fLocked*/);
                if (RT_SUCCESS(vrc))
                {
                    AssertStmt(cbActual <= cbWrite, cbActual = cbWrite);
                    *pcb = cbActual;
                    pSfFsi->sfi_positionl = offWrite + cbActual;
                    if ((uint64_t)pSfFsi->sfi_sizel < offWrite + cbActual)
                        pSfFsi->sfi_sizel = offWrite + cbActual;
                    pSfFsi->sfi_tstamp   |= ST_SWRITE | ST_PWRITE;
                    LogFlow(("FS32_READ: returns; cbActual=%#x sfi_positionl=%RI64 [copy]\n", cbActual, pSfFsi->sfi_positionl));
                }
                else
                {
                    Log(("FS32_READ: VbglR0SfWrite(off=%#x,cb=%#x) -> %Rrc [copy]\n", offWrite, cbWrite, vrc));
                    rc = ERROR_BAD_NET_RESP;
                }
            }
            VbglR0PhysHeapFree(pvBuf);
            return rc;
        }
    }

    /*
     * Do the write directly on the buffer, Vbgl will do the locking for us.
     */
    int vrc = VbglR0SfWrite(&g_SfClient, &pFolder->hHostFolder, pSfFsd->hHostFile,
                            offWrite, &cbActual, (uint8_t *)pvData, false /*fLocked*/);
    if (RT_SUCCESS(vrc))
    {
        AssertStmt(cbActual <= cbWrite, cbActual = cbWrite);
        *pcb = cbActual;
        pSfFsi->sfi_positionl = offWrite + cbActual;
        if ((uint64_t)pSfFsi->sfi_sizel < offWrite + cbActual)
            pSfFsi->sfi_sizel = offWrite + cbActual;
        pSfFsi->sfi_tstamp   |= ST_SWRITE | ST_PWRITE;
        LogFlow(("FS32_READ: returns; cbActual=%#x sfi_positionl=%RI64 [direct]\n", cbActual, pSfFsi->sfi_positionl));
        return NO_ERROR;
    }
    Log(("FS32_READ: VbglR0SfWrite(off=%#x,cb=%#x) -> %Rrc [direct]\n", offWrite, cbWrite, vrc));
    RT_NOREF_PV(fIoFlags);
    return ERROR_BAD_NET_RESP;
}


extern "C" APIRET APIENTRY
FS32_READFILEATCACHE(PSFFSI pSfFsi, PVBOXSFSYFI pSfFsd, ULONG fIoFlags, LONGLONG off, ULONG pcb, KernCacheList_t **ppCacheList)
{
    /*
     * Validate input.
     */
    AssertReturn(pSfFsd->u32Magic == VBOXSFSYFI_MAGIC, ERROR_SYS_INTERNAL);
    AssertReturn(pSfFsd->pSelf    == pSfFsd, ERROR_SYS_INTERNAL);
    PVBOXSFFOLDER pFolder = pSfFsd->pFolder;
    AssertReturn(pFolder != NULL, ERROR_SYS_INTERNAL);
    Assert(pFolder->u32Magic == VBOXSFFOLDER_MAGIC);
    Assert(pFolder->cOpenFiles > 0);
    RT_NOREF(pFolder);

    /* I think this is used for sendfile(). */

    NOREF(pSfFsi); NOREF(pSfFsd); NOREF(fIoFlags); NOREF(off); NOREF(pcb); NOREF(ppCacheList);
    return ERROR_NOT_SUPPORTED;
}


extern "C" APIRET APIENTRY
FS32_RETURNFILECACHE(KernCacheList_t *pCacheList)
{
    NOREF(pCacheList);
    return ERROR_NOT_SUPPORTED;
}


/* oddments */

DECLASM(APIRET)
FS32_CANCELLOCKREQUESTL(PSFFSI pSfFsi, PVBOXSFSYFI pSfFsd, struct filelockl *pLockRange)
{
    /*
     * Validate input.
     */
    AssertReturn(pSfFsd->u32Magic == VBOXSFSYFI_MAGIC, ERROR_SYS_INTERNAL);
    AssertReturn(pSfFsd->pSelf    == pSfFsd, ERROR_SYS_INTERNAL);
    PVBOXSFFOLDER pFolder = pSfFsd->pFolder;
    AssertReturn(pFolder != NULL, ERROR_SYS_INTERNAL);
    Assert(pFolder->u32Magic == VBOXSFFOLDER_MAGIC);
    Assert(pFolder->cOpenFiles > 0);
    RT_NOREF(pFolder);

    NOREF(pSfFsi); NOREF(pSfFsd); NOREF(pLockRange);
    return ERROR_NOT_SUPPORTED;
}


DECLASM(APIRET)
FS32_CANCELLOCKREQUEST(PSFFSI pSfFsi, PVBOXSFSYFI pSfFsd, struct filelock *pLockRange)
{
    /*
     * Validate input.
     */
    AssertReturn(pSfFsd->u32Magic == VBOXSFSYFI_MAGIC, ERROR_SYS_INTERNAL);
    AssertReturn(pSfFsd->pSelf    == pSfFsd, ERROR_SYS_INTERNAL);
    PVBOXSFFOLDER pFolder = pSfFsd->pFolder;
    AssertReturn(pFolder != NULL, ERROR_SYS_INTERNAL);
    Assert(pFolder->u32Magic == VBOXSFFOLDER_MAGIC);
    Assert(pFolder->cOpenFiles > 0);
    RT_NOREF(pFolder);

    NOREF(pSfFsi); NOREF(pSfFsd); NOREF(pLockRange);
    return ERROR_NOT_SUPPORTED;
}


DECLASM(APIRET)
FS32_FILELOCKSL(PSFFSI pSfFsi, PVBOXSFSYFI pSfFsd, struct filelockl *pUnLockRange,
                struct filelockl *pLockRange, ULONG cMsTimeout, ULONG fFlags)
{
    /*
     * Validate input.
     */
    AssertReturn(pSfFsd->u32Magic == VBOXSFSYFI_MAGIC, ERROR_SYS_INTERNAL);
    AssertReturn(pSfFsd->pSelf    == pSfFsd, ERROR_SYS_INTERNAL);
    PVBOXSFFOLDER pFolder = pSfFsd->pFolder;
    AssertReturn(pFolder != NULL, ERROR_SYS_INTERNAL);
    Assert(pFolder->u32Magic == VBOXSFFOLDER_MAGIC);
    Assert(pFolder->cOpenFiles > 0);
    RT_NOREF(pFolder);

    NOREF(pSfFsi); NOREF(pSfFsd); NOREF(pUnLockRange); NOREF(pLockRange); NOREF(cMsTimeout); NOREF(fFlags);
    return ERROR_NOT_SUPPORTED;
}


DECLASM(APIRET)
FS32_FILELOCKS(PSFFSI pSfFsi, PVBOXSFSYFI pSfFsd, struct filelock *pUnLockRange,
               struct filelock *pLockRange, ULONG cMsTimeout, ULONG fFlags)
{
    /*
     * Validate input.
     */
    AssertReturn(pSfFsd->u32Magic == VBOXSFSYFI_MAGIC, ERROR_SYS_INTERNAL);
    AssertReturn(pSfFsd->pSelf    == pSfFsd, ERROR_SYS_INTERNAL);
    PVBOXSFFOLDER pFolder = pSfFsd->pFolder;
    AssertReturn(pFolder != NULL, ERROR_SYS_INTERNAL);
    Assert(pFolder->u32Magic == VBOXSFFOLDER_MAGIC);
    Assert(pFolder->cOpenFiles > 0);
    RT_NOREF(pFolder);

    NOREF(pSfFsi); NOREF(pSfFsd); NOREF(pUnLockRange); NOREF(pLockRange); NOREF(cMsTimeout); NOREF(fFlags);
    return ERROR_NOT_SUPPORTED;
}


DECLASM(APIRET)
FS32_IOCTL(PSFFSI pSfFsi, PVBOXSFSYFI pSfFsd, USHORT uCategory, USHORT uFunction,
           PVOID pvParm, USHORT cbParm, PUSHORT pcbParmIO,
           PVOID pvData, USHORT cbData, PUSHORT pcbDataIO)
{
    /*
     * Validate input.
     */
    AssertReturn(pSfFsd->u32Magic == VBOXSFSYFI_MAGIC, ERROR_SYS_INTERNAL);
    AssertReturn(pSfFsd->pSelf    == pSfFsd, ERROR_SYS_INTERNAL);
    PVBOXSFFOLDER pFolder = pSfFsd->pFolder;
    AssertReturn(pFolder != NULL, ERROR_SYS_INTERNAL);
    Assert(pFolder->u32Magic == VBOXSFFOLDER_MAGIC);
    Assert(pFolder->cOpenFiles > 0);
    RT_NOREF(pFolder);

    NOREF(pSfFsi); NOREF(pSfFsd); NOREF(uCategory); NOREF(uFunction); NOREF(pvParm); NOREF(cbParm); NOREF(pcbParmIO);
    NOREF(pvData); NOREF(cbData); NOREF(pcbDataIO);
    return ERROR_NOT_SUPPORTED;
}


DECLASM(APIRET)
FS32_FILEIO(PSFFSI pSfFsi, PVBOXSFSYFI pSfFsd, PBYTE pbCmdList, USHORT cbCmdList,
            PUSHORT poffError, USHORT fIoFlag)
{
    /*
     * Validate input.
     */
    AssertReturn(pSfFsd->u32Magic == VBOXSFSYFI_MAGIC, ERROR_SYS_INTERNAL);
    AssertReturn(pSfFsd->pSelf    == pSfFsd, ERROR_SYS_INTERNAL);
    PVBOXSFFOLDER pFolder = pSfFsd->pFolder;
    AssertReturn(pFolder != NULL, ERROR_SYS_INTERNAL);
    Assert(pFolder->u32Magic == VBOXSFFOLDER_MAGIC);
    Assert(pFolder->cOpenFiles > 0);
    RT_NOREF(pFolder);

    NOREF(pSfFsi); NOREF(pSfFsd); NOREF(pbCmdList); NOREF(cbCmdList); NOREF(poffError); NOREF(fIoFlag);
    return ERROR_NOT_SUPPORTED;
}


DECLASM(APIRET)
FS32_NMPIPE(PSFFSI pSfFsi, PVBOXSFSYFI pSfFsd, USHORT uOpType, union npoper *pOpRec,
            PBYTE pbData, PCSZ pszName)
{
    NOREF(pSfFsi); NOREF(pSfFsd); NOREF(uOpType); NOREF(pOpRec); NOREF(pbData); NOREF(pszName);
    return ERROR_NOT_SUPPORTED;
}


DECLASM(APIRET)
FS32_OPENPAGEFILE(PULONG pfFlags, PULONG pcMaxReq, PCSZ pszName, PSFFSI pSfFsi, PVBOXSFSYFI pSfFsd,
                  USHORT fOpenMode, USHORT fOpenFlags, USHORT fAttr, ULONG uReserved)
{
    NOREF(pfFlags); NOREF(pcMaxReq); NOREF(pszName); NOREF(pSfFsi); NOREF(pSfFsd); NOREF(fOpenMode); NOREF(fOpenFlags);
    NOREF(fAttr); NOREF(uReserved);
    return ERROR_NOT_SUPPORTED;
}


DECLASM(APIRET)
FS32_SETSWAP(PSFFSI pSfFsi, PVBOXSFSYFI pSfFsd)
{
    NOREF(pSfFsi); NOREF(pSfFsd);
    return ERROR_NOT_SUPPORTED;
}


DECLASM(APIRET)
FS32_ALLOCATEPAGESPACE(PSFFSI pSfFsi, PVBOXSFSYFI pSfFsd, ULONG cb, USHORT cbWantContig)
{
    NOREF(pSfFsi); NOREF(pSfFsd); NOREF(cb); NOREF(cbWantContig);
    return ERROR_NOT_SUPPORTED;
}


DECLASM(APIRET)
FS32_DOPAGEIO(PSFFSI pSfFsi, PVBOXSFSYFI pSfFsd, struct PageCmdHeader *pList)
{
    NOREF(pSfFsi); NOREF(pSfFsd); NOREF(pList);
    return ERROR_NOT_SUPPORTED;
}

