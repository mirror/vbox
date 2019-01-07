/** $Id$ */
/** @file
 * VBoxSF - OS/2 Shared Folder IFS, Internal Header.
 */

/*
 * Copyright (c) 2007 knut st. osmundsen <bird-src-spam@anduin.net>
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

#ifndef GA_INCLUDED_SRC_os2_VBoxSF_VBoxSFInternal_h
#define GA_INCLUDED_SRC_os2_VBoxSF_VBoxSFInternal_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif


#define INCL_BASE
#define INCL_ERROR
#define INCL_LONGLONG
#define OS2EMX_PLAIN_CHAR
#include <os2ddk/bsekee.h>
#include <os2ddk/devhlp.h>
#include <os2ddk/unikern.h>
#include <os2ddk/fsd.h>
#undef RT_MAX

#include <iprt/types.h>
#include <iprt/assert.h>
#include <iprt/list.h>
#include <VBox/VBoxGuestLibSharedFolders.h>
#include <VBox/VBoxGuest.h>


/** Allocation header used by RTMemAlloc.
 * This should be subtracted from round numbers. */
#define ALLOC_HDR_SIZE  (0x10 + 4)


/**
 * A shared folder
 */
typedef struct VBOXSFFOLDER
{
    /** For the shared folder list. */
    RTLISTNODE          ListEntry;
    /** Magic number (VBOXSFFOLDER_MAGIC). */
    uint32_t            u32Magic;
    /** Number of active references to this folder. */
    uint32_t volatile   cRefs;
    /** Number of open files referencing this folder.   */
    uint32_t volatile   cOpenFiles;
    /** Number of open searches referencing this folder.   */
    uint32_t volatile   cOpenSearches;
    /** Number of drives this is attached to. */
    uint8_t volatile    cDrives;

    /** The host folder handle. */
    VBGLSFMAP           hHostFolder;

    /** OS/2 volume handle. */
    USHORT              hVpb;

    /** The length of the name and tag, including zero terminators and such. */
    uint16_t            cbNameAndTag;
    /** The length of the folder name. */
    uint8_t             cchName;
    /** The shared folder name.  If there is a tag it follows as a second string. */
    char                szName[RT_FLEXIBLE_ARRAY];
} VBOXSFFOLDER;
/** Pointer to a shared folder. */
typedef VBOXSFFOLDER *PVBOXSFFOLDER;
/** Magic value for VBOXSFVP (Neal Town Stephenson). */
#define VBOXSFFOLDER_MAGIC      UINT32_C(0x19591031)

/** The shared mutex protecting folders list, drives and the connection. */
extern MutexLock_t      g_MtxFolders;
/** List of active folder (PVBOXSFFOLDER). */
extern RTLISTANCHOR     g_FolderHead;


/**
 * VBoxSF Volume Parameter Structure.
 *
 * @remarks Overlays the 36 byte VPFSD structure (fsd.h).
 * @note    No self pointer as the kernel may reallocate these.
 */
typedef struct VBOXSFVP
{
    /** Magic value (VBOXSFVP_MAGIC). */
    uint32_t         u32Magic;
    /** The folder. */
    PVBOXSFFOLDER    pFolder;
} VBOXSFVP;
AssertCompile(sizeof(VBOXSFVP) <= sizeof(VPFSD));
/** Pointer to a VBOXSFVP struct. */
typedef VBOXSFVP *PVBOXSFVP;
/** Magic value for VBOXSFVP (Laurence van Cott Niven). */
#define VBOXSFVP_MAGIC          UINT32_C(0x19380430)


/**
 * VBoxSF Current Directory Structure.
 *
 * @remark  Overlays the 8 byte CDFSD structure (fsd.h).
 */
typedef struct VBOXSFCD
{
    uint32_t u32Dummy;
} VBOXSFCD;
AssertCompile(sizeof(VBOXSFCD) <= sizeof(CDFSD));
/** Pointer to a VBOXSFCD struct. */
typedef VBOXSFCD *PVBOXSFCD;


/**
 * VBoxSF System File Structure.
 *
 * @remark  Overlays the 30 byte SFFSD structure (fsd.h).
 */
typedef struct VBOXSFSYFI
{
    /** Magic value (VBOXSFSYFI_MAGIC). */
    uint32_t            u32Magic;
    /** Self pointer for quick 16:16 to flat translation. */
    struct VBOXSFSYFI  *pSelf;
    /** The host file handle. */
    SHFLHANDLE          hHostFile;
    /** The shared folder (referenced). */
    PVBOXSFFOLDER       pFolder;
} VBOXSFSYFI;
AssertCompile(sizeof(VBOXSFSYFI) <= sizeof(SFFSD));
/** Pointer to a VBOXSFSYFI struct. */
typedef VBOXSFSYFI *PVBOXSFSYFI;
/** Magic value for VBOXSFSYFI (Jon Ellis Meacham). */
#define VBOXSFSYFI_MAGIC         UINT32_C(0x19690520)


/** Request structure for vboxSfOs2HostReqListDir. */
typedef struct VBOXSFLISTDIRREQ
{
    VBGLIOCIDCHGCMFASTCALL  Hdr;
    VMMDevHGCMCall          Call;
    VBoxSFParmList          Parms;
    HGCMPageListInfo        StrPgLst;
    HGCMPageListInfo        BufPgLst;
} VBOXSFLISTDIRREQ;

/**
 * More file search data (on physical heap).
 */
typedef struct VBOXSFFSBUF /**< @todo rename as is no longer buffer. */
{
    /** The request (must be first). */
    VBOXSFLISTDIRREQ    Req;
    /** A magic number (VBOXSFFSBUF_MAGIC). */
    uint32_t            u32Magic;
    /** The filter string (full path), NULL if all files are request. */
    PSHFLSTRING         pFilter;
    /** Size of the buffer for directory entries. */
    uint32_t            cbBuf;
    /** Buffer for directory entries on the physical heap. */
    PSHFLDIRINFO        pBuf;
    /** Must have attributes (shifted down DOS attributes).  */
    uint8_t             fMustHaveAttribs;
    /** Non-matching attributes (shifted down DOS attributes).  */
    uint8_t             fExcludedAttribs;
    /** Set if FF_ATTR_LONG_FILENAME. */
    bool                fLongFilenames : 1;
    uint8_t             bPadding1;
    /** The local time offset to use for this search. */
    int16_t             cMinLocalTimeDelta;
    uint8_t             abPadding2[2];
    /** Number of valid bytes in the buffer. */
    uint32_t            cbValid;
    /** Number of entries left in the buffer.   */
    uint32_t            cEntriesLeft;
    /** The next entry. */
    PSHFLDIRINFO        pEntry;
    //uint32_t            uPadding3;
    /** Staging area for staging a full FILEFINDBUF4L (+ 32 safe bytes). */
    uint8_t             abStaging[RT_ALIGN_32(sizeof(FILEFINDBUF4L) + 32, 8)];
} VBOXSFFSBUF;
/** Pointer to a file search buffer. */
typedef VBOXSFFSBUF *PVBOXSFFSBUF;
/** Magic number for VBOXSFFSBUF (Robert Anson Heinlein). */
#define VBOXSFFSBUF_MAGIC       UINT32_C(0x19070707)


/**
 * VBoxSF File Search Structure.
 *
 * @remark  Overlays the 24 byte FSFSD structure (fsd.h).
 * @note    No self pointer as the kernel may reallocate these.
 */
typedef struct VBOXSFFS
{
    /** Magic value (VBOXSFFS_MAGIC). */
    uint32_t            u32Magic;
    /** The last file position position. */
    uint32_t            offLastFile;
    /** The host directory handle. */
    SHFLHANDLE          hHostDir;
    /** The shared folder (referenced). */
    PVBOXSFFOLDER       pFolder;
    /** Search data buffer. */
    PVBOXSFFSBUF        pBuf;
} VBOXSFFS;
AssertCompile(sizeof(VBOXSFFS) <= sizeof(FSFSD));
/** Pointer to a VBOXSFFS struct. */
typedef VBOXSFFS *PVBOXSFFS;
/** Magic number for VBOXSFFS (Isaak Azimov). */
#define VBOXSFFS_MAGIC          UINT32_C(0x19200102)


extern VBGLSFCLIENT g_SfClient;

void        vboxSfOs2InitFileBuffers(void);
PSHFLSTRING vboxSfOs2StrAlloc(size_t cwcLength);
PSHFLSTRING vboxSfOs2StrDup(PCSHFLSTRING pSrc);
void        vboxSfOs2StrFree(PSHFLSTRING pStr);

APIRET      vboxSfOs2ResolvePath(const char *pszPath, PVBOXSFCD pCdFsd, LONG offCurDirEnd,
                                 PVBOXSFFOLDER *ppFolder, PSHFLSTRING *ppStrFolderPath);
APIRET      vboxSfOs2ResolvePathEx(const char *pszPath, PVBOXSFCD pCdFsd, LONG offCurDirEnd, uint32_t offStrInBuf,
                                   PVBOXSFFOLDER *ppFolder, void **ppvBuf);
void        vboxSfOs2ReleasePathAndFolder(PSHFLSTRING pStrPath, PVBOXSFFOLDER pFolder);
void        vboxSfOs2ReleaseFolder(PVBOXSFFOLDER pFolder);
APIRET      vboxSfOs2ConvertStatusToOs2(int vrc, APIRET rcDefault);
int16_t     vboxSfOs2GetLocalTimeDelta(void);
void        vboxSfOs2DateTimeFromTimeSpec(FDATE *pDosDate, FTIME *pDosTime, RTTIMESPEC SrcTimeSpec, int16_t cMinLocalTimeDelta);
PRTTIMESPEC vboxSfOs2DateTimeToTimeSpec(FDATE DosDate, FTIME DosTime, int16_t cMinLocalTimeDelta, PRTTIMESPEC pDstTimeSpec);
APIRET      vboxSfOs2FileStatusFromObjInfo(PBYTE pbDst, ULONG cbDst, ULONG uLevel, SHFLFSOBJINFO const *pSrc);
APIRET      vboxSfOs2SetInfoCommonWorker(PVBOXSFFOLDER pFolder, SHFLHANDLE hHostFile, ULONG fAttribs,
                                         PFILESTATUS pTimestamps, PSHFLFSOBJINFO pObjInfoBuf, uint32_t offObjInfoInAlloc);
APIRET      vboxSfOs2MakeEmptyEaList(PEAOP pEaOp, ULONG uLevel);
APIRET      vboxSfOs2MakeEmptyEaListEx(PEAOP pEaOp, ULONG uLevel, uint32_t *pcbWritten, ULONG *poffError);

DECLASM(PVBOXSFVP) Fsh32GetVolParams(USHORT hVbp, PVPFSI *ppVpFsi /*optional*/);


/** @name Host request helpers
 *
 * @todo generalize these and put back into VbglR0Sf.
 *
 * @{  */

#include <iprt/err.h>

/** Request structure for vboxSfOs2HostReqMapFolderWithBuf.  */
typedef struct VBOXSFMAPFOLDERWITHBUFREQ
{
    VBGLIOCIDCHGCMFASTCALL  Hdr;
    VMMDevHGCMCall          Call;
    VBoxSFParmMapFolder     Parms;
    union
    {
        HGCMPageListInfo    PgLst;
        uint8_t             abPadding[8 + sizeof(RTGCPHYS64) * 2 /*RT_UOFFSETOF(HGCMPageListInfo, aPages[2])*/];
    } u;
} VBOXSFMAPFOLDERWITHBUFREQ;

/**
 * SHFL_FN_MAP_FOLDER request.
 */
DECLINLINE(int) vboxSfOs2HostReqMapFolderWithBuf(VBOXSFMAPFOLDERWITHBUFREQ *pReq, PSHFLSTRING pStrName,
                                                 RTUTF16 wcDelimiter, bool fCaseSensitive)
{
    pReq->Parms.id32Root.type               = VMMDevHGCMParmType_32bit;
    pReq->Parms.id32Root.u.value32          = SHFL_ROOT_NIL;

    pReq->Parms.uc32Delimiter.type          = VMMDevHGCMParmType_32bit;
    pReq->Parms.uc32Delimiter.u.value32     = wcDelimiter;

    pReq->Parms.fCaseSensitive.type         = VMMDevHGCMParmType_32bit;
    pReq->Parms.fCaseSensitive.u.value32    = fCaseSensitive;

    AssertReturn(pStrName->u16Size <= PAGE_SIZE - SHFLSTRING_HEADER_SIZE, VERR_FILENAME_TOO_LONG);
    pReq->Parms.pStrName.type               = VMMDevHGCMParmType_PageList;
    pReq->Parms.pStrName.u.PageList.size    = SHFLSTRING_HEADER_SIZE + pStrName->u16Size;
    pReq->Parms.pStrName.u.PageList.offset  = RT_UOFFSETOF(VBOXSFMAPFOLDERWITHBUFREQ, u.PgLst) - sizeof(VBGLIOCIDCHGCMFASTCALL);
    pReq->u.PgLst.flags                     = VBOX_HGCM_F_PARM_DIRECTION_BOTH;
    pReq->u.PgLst.aPages[0]                 = VbglR0PhysHeapGetPhysAddr(pStrName);
    pReq->u.PgLst.offFirstPage              = (uint16_t)(pReq->u.PgLst.aPages[0] & PAGE_OFFSET_MASK);
    pReq->u.PgLst.aPages[0]                &= ~(RTGCPHYS)PAGE_OFFSET_MASK;
    uint32_t cbReq;
    if (PAGE_SIZE - pReq->u.PgLst.offFirstPage <= pStrName->u16Size + SHFLSTRING_HEADER_SIZE)
    {
        pReq->u.PgLst.cPages            = 1;
        cbReq = RT_UOFFSETOF(VBOXSFMAPFOLDERWITHBUFREQ, u.PgLst.aPages[1]);
    }
    else
    {
        pReq->u.PgLst.aPages[1]         = pReq->u.PgLst.aPages[0] + PAGE_SIZE;
        pReq->u.PgLst.cPages            = 2;
        cbReq = RT_UOFFSETOF(VBOXSFMAPFOLDERWITHBUFREQ, u.PgLst.aPages[2]);
    }

    VBGLIOCIDCHGCMFASTCALL_INIT(&pReq->Hdr, VbglR0PhysHeapGetPhysAddr(pReq), &pReq->Call, g_SfClient.idClient,
                                SHFL_FN_MAP_FOLDER, SHFL_CPARMS_MAP_FOLDER, cbReq);

    int vrc = VbglR0HGCMFastCall(g_SfClient.handle, &pReq->Hdr, cbReq);
    if (RT_SUCCESS(vrc))
        vrc = pReq->Call.header.result;
    return vrc;
}



/** Request structure used by vboxSfOs2HostReqUnmapFolder.  */
typedef struct VBOXSFUNMAPFOLDERREQ
{
    VBGLIOCIDCHGCMFASTCALL  Hdr;
    VMMDevHGCMCall          Call;
    VBoxSFParmUnmapFolder   Parms;
} VBOXSFUNMAPFOLDERREQ;


/**
 * SHFL_FN_UNMAP_FOLDER request.
 */
DECLINLINE(int) vboxSfOs2HostReqUnmapFolderSimple(uint32_t idRoot)
{
    VBOXSFUNMAPFOLDERREQ *pReq = (VBOXSFUNMAPFOLDERREQ *)VbglR0PhysHeapAlloc(sizeof(*pReq));
    if (pReq)
    {
        pReq->Parms.id32Root.type      = VMMDevHGCMParmType_32bit;
        pReq->Parms.id32Root.u.value32 = idRoot;

        VBGLIOCIDCHGCMFASTCALL_INIT(&pReq->Hdr, VbglR0PhysHeapGetPhysAddr(pReq), &pReq->Call, g_SfClient.idClient,
                                    SHFL_FN_UNMAP_FOLDER, SHFL_CPARMS_UNMAP_FOLDER, sizeof(*pReq));

        int vrc = VbglR0HGCMFastCall(g_SfClient.handle, &pReq->Hdr, sizeof(*pReq));
        if (RT_SUCCESS(vrc))
            vrc = pReq->Call.header.result;

        VbglR0PhysHeapFree(pReq);
        return vrc;
    }
    return VERR_NO_MEMORY;
}


/** Request structure for vboxSfOs2HostReqCreate.  */
typedef struct VBOXSFCREATEREQ
{
    VBGLIOCIDCHGCMFASTCALL  Hdr;
    VMMDevHGCMCall          Call;
    VBoxSFParmCreate        Parms;
    SHFLCREATEPARMS         CreateParms;
    SHFLSTRING              StrPath;
} VBOXSFCREATEREQ;

/**
 * SHFL_FN_CREATE request.
 */
DECLINLINE(int) vboxSfOs2HostReqCreate(PVBOXSFFOLDER pFolder, VBOXSFCREATEREQ *pReq)
{
    VBGLIOCIDCHGCMFASTCALL_INIT(&pReq->Hdr, VbglR0PhysHeapGetPhysAddr(pReq), &pReq->Call, g_SfClient.idClient,
                                SHFL_FN_CREATE, SHFL_CPARMS_CREATE,
                                RT_UOFFSETOF(VBOXSFCREATEREQ, StrPath.String) + pReq->StrPath.u16Size);

    pReq->Parms.id32Root.type                   = VMMDevHGCMParmType_32bit;
    pReq->Parms.id32Root.u.value32              = pFolder->hHostFolder.root;

    pReq->Parms.pStrPath.type                   = VMMDevHGCMParmType_Embedded;
    pReq->Parms.pStrPath.u.Embedded.cbData      = SHFLSTRING_HEADER_SIZE + pReq->StrPath.u16Size;
    pReq->Parms.pStrPath.u.Embedded.offData     = RT_UOFFSETOF(VBOXSFCREATEREQ, StrPath) - sizeof(VBGLIOCIDCHGCMFASTCALL);
    pReq->Parms.pStrPath.u.Embedded.fFlags      = VBOX_HGCM_F_PARM_DIRECTION_TO_HOST;

    pReq->Parms.pCreateParms.type               = VMMDevHGCMParmType_Embedded;
    pReq->Parms.pCreateParms.u.Embedded.cbData  = sizeof(pReq->CreateParms);
    pReq->Parms.pCreateParms.u.Embedded.offData = RT_UOFFSETOF(VBOXSFCREATEREQ, CreateParms) - sizeof(VBGLIOCIDCHGCMFASTCALL);
    pReq->Parms.pCreateParms.u.Embedded.fFlags  = VBOX_HGCM_F_PARM_DIRECTION_BOTH;

    int vrc = VbglR0HGCMFastCall(g_SfClient.handle, &pReq->Hdr,
                                 RT_UOFFSETOF(VBOXSFCREATEREQ, StrPath.String) + pReq->StrPath.u16Size);
    if (RT_SUCCESS(vrc))
        vrc = pReq->Call.header.result;
    return vrc;
}


/** Request structure for vboxSfOs2HostReqClose.  */
typedef struct VBOXSFCLOSEREQ
{
    VBGLIOCIDCHGCMFASTCALL  Hdr;
    VMMDevHGCMCall          Call;
    VBoxSFParmClose         Parms;
} VBOXSFCLOSEREQ;

/**
 * SHFL_FN_CLOSE request.
 */
DECLINLINE(int) vboxSfOs2HostReqClose(PVBOXSFFOLDER pFolder, VBOXSFCLOSEREQ *pReq, uint64_t hHostFile)
{
    VBGLIOCIDCHGCMFASTCALL_INIT(&pReq->Hdr, VbglR0PhysHeapGetPhysAddr(pReq), &pReq->Call, g_SfClient.idClient,
                                SHFL_FN_CLOSE, SHFL_CPARMS_CLOSE, sizeof(*pReq));

    pReq->Parms.id32Root.type                   = VMMDevHGCMParmType_32bit;
    pReq->Parms.id32Root.u.value32              = pFolder->hHostFolder.root;

    pReq->Parms.u64Handle.type                  = VMMDevHGCMParmType_64bit;
    pReq->Parms.u64Handle.u.value64             = hHostFile;

    int vrc = VbglR0HGCMFastCall(g_SfClient.handle, &pReq->Hdr, sizeof(*pReq));
    if (RT_SUCCESS(vrc))
        vrc = pReq->Call.header.result;
    return vrc;
}

/**
 * SHFL_FN_CLOSE request, allocate request buffer.
 */
DECLINLINE(int) vboxSfOs2HostReqCloseSimple(PVBOXSFFOLDER pFolder, uint64_t hHostFile)
{
    VBOXSFCLOSEREQ *pReq = (VBOXSFCLOSEREQ *)VbglR0PhysHeapAlloc(sizeof(*pReq));
    if (pReq)
    {
        int vrc = vboxSfOs2HostReqClose(pFolder, pReq, hHostFile);
        VbglR0PhysHeapFree(pReq);
        return vrc;
    }
    return VERR_NO_MEMORY;
}


/** Request structure for vboxSfOs2HostReqQueryVolInfo.  */
typedef struct VBOXSFVOLINFOREQ
{
    VBGLIOCIDCHGCMFASTCALL  Hdr;
    VMMDevHGCMCall          Call;
    VBoxSFParmInformation   Parms;
    SHFLVOLINFO             VolInfo;
} VBOXSFVOLINFOREQ;

/**
 * SHFL_FN_INFORMATION[SHFL_INFO_VOLUME | SHFL_INFO_GET] request.
 */
DECLINLINE(int) vboxSfOs2HostReqQueryVolInfo(PVBOXSFFOLDER pFolder, VBOXSFVOLINFOREQ *pReq, uint64_t hHostFile)
{
    VBGLIOCIDCHGCMFASTCALL_INIT(&pReq->Hdr, VbglR0PhysHeapGetPhysAddr(pReq), &pReq->Call, g_SfClient.idClient,
                                SHFL_FN_INFORMATION, SHFL_CPARMS_INFORMATION, sizeof(*pReq));

    pReq->Parms.id32Root.type                   = VMMDevHGCMParmType_32bit;
    pReq->Parms.id32Root.u.value32              = pFolder->hHostFolder.root;

    pReq->Parms.u64Handle.type                  = VMMDevHGCMParmType_64bit;
    pReq->Parms.u64Handle.u.value64             = hHostFile;

    pReq->Parms.f32Flags.type                   = VMMDevHGCMParmType_32bit;
    pReq->Parms.f32Flags.u.value32              = SHFL_INFO_VOLUME | SHFL_INFO_GET;

    pReq->Parms.cb32.type                       = VMMDevHGCMParmType_32bit;
    pReq->Parms.cb32.u.value32                  = sizeof(pReq->VolInfo);

    pReq->Parms.pInfo.type                      = VMMDevHGCMParmType_Embedded;
    pReq->Parms.pInfo.u.Embedded.cbData         = sizeof(pReq->VolInfo);
    pReq->Parms.pInfo.u.Embedded.offData        = RT_UOFFSETOF(VBOXSFVOLINFOREQ, VolInfo) - sizeof(VBGLIOCIDCHGCMFASTCALL);
    pReq->Parms.pInfo.u.Embedded.fFlags         = VBOX_HGCM_F_PARM_DIRECTION_FROM_HOST;

    int vrc = VbglR0HGCMFastCall(g_SfClient.handle, &pReq->Hdr, sizeof(*pReq));
    if (RT_SUCCESS(vrc))
        vrc = pReq->Call.header.result;
    return vrc;
}


/** Request structure for vboxSfOs2HostReqSetObjInfo & vboxSfOs2HostReqQueryObjInfo. */
typedef struct VBOXSFOBJINFOREQ
{
    VBGLIOCIDCHGCMFASTCALL  Hdr;
    VMMDevHGCMCall          Call;
    VBoxSFParmInformation   Parms;
    SHFLFSOBJINFO           ObjInfo;
} VBOXSFOBJINFOREQ;

/**
 * SHFL_FN_INFORMATION[SHFL_INFO_GET | SHFL_INFO_FILE] request.
 */
DECLINLINE(int) vboxSfOs2HostReqQueryObjInfo(PVBOXSFFOLDER pFolder, VBOXSFOBJINFOREQ *pReq, uint64_t hHostFile)
{
    VBGLIOCIDCHGCMFASTCALL_INIT(&pReq->Hdr, VbglR0PhysHeapGetPhysAddr(pReq), &pReq->Call, g_SfClient.idClient,
                                SHFL_FN_INFORMATION, SHFL_CPARMS_INFORMATION, sizeof(*pReq));

    pReq->Parms.id32Root.type               = VMMDevHGCMParmType_32bit;
    pReq->Parms.id32Root.u.value32          = pFolder->hHostFolder.root;

    pReq->Parms.u64Handle.type              = VMMDevHGCMParmType_64bit;
    pReq->Parms.u64Handle.u.value64         = hHostFile;

    pReq->Parms.f32Flags.type               = VMMDevHGCMParmType_32bit;
    pReq->Parms.f32Flags.u.value32          = SHFL_INFO_GET | SHFL_INFO_FILE;

    pReq->Parms.cb32.type                   = VMMDevHGCMParmType_32bit;
    pReq->Parms.cb32.u.value32              = sizeof(pReq->ObjInfo);

    pReq->Parms.pInfo.type                  = VMMDevHGCMParmType_Embedded;
    pReq->Parms.pInfo.u.Embedded.cbData     = sizeof(pReq->ObjInfo);
    pReq->Parms.pInfo.u.Embedded.offData    = RT_UOFFSETOF(VBOXSFOBJINFOREQ, ObjInfo) - sizeof(VBGLIOCIDCHGCMFASTCALL);
    pReq->Parms.pInfo.u.Embedded.fFlags     = VBOX_HGCM_F_PARM_DIRECTION_FROM_HOST;

    int vrc = VbglR0HGCMFastCall(g_SfClient.handle, &pReq->Hdr, sizeof(*pReq));
    if (RT_SUCCESS(vrc))
        vrc = pReq->Call.header.result;
    return vrc;
}


/**
 * SHFL_FN_INFORMATION[SHFL_INFO_SET | SHFL_INFO_FILE] request.
 */
DECLINLINE(int) vboxSfOs2HostReqSetObjInfo(PVBOXSFFOLDER pFolder, VBOXSFOBJINFOREQ *pReq, uint64_t hHostFile)
{
    VBGLIOCIDCHGCMFASTCALL_INIT(&pReq->Hdr, VbglR0PhysHeapGetPhysAddr(pReq), &pReq->Call, g_SfClient.idClient,
                                SHFL_FN_INFORMATION, SHFL_CPARMS_INFORMATION, sizeof(*pReq));

    pReq->Parms.id32Root.type               = VMMDevHGCMParmType_32bit;
    pReq->Parms.id32Root.u.value32          = pFolder->hHostFolder.root;

    pReq->Parms.u64Handle.type              = VMMDevHGCMParmType_64bit;
    pReq->Parms.u64Handle.u.value64         = hHostFile;

    pReq->Parms.f32Flags.type               = VMMDevHGCMParmType_32bit;
    pReq->Parms.f32Flags.u.value32          = SHFL_INFO_SET | SHFL_INFO_FILE;

    pReq->Parms.cb32.type                   = VMMDevHGCMParmType_32bit;
    pReq->Parms.cb32.u.value32              = sizeof(pReq->ObjInfo);

    pReq->Parms.pInfo.type                  = VMMDevHGCMParmType_Embedded;
    pReq->Parms.pInfo.u.Embedded.cbData     = sizeof(pReq->ObjInfo);
    pReq->Parms.pInfo.u.Embedded.offData    = RT_UOFFSETOF(VBOXSFOBJINFOREQ, ObjInfo) - sizeof(VBGLIOCIDCHGCMFASTCALL);
    pReq->Parms.pInfo.u.Embedded.fFlags     = VBOX_HGCM_F_PARM_DIRECTION_BOTH;

    int vrc = VbglR0HGCMFastCall(g_SfClient.handle, &pReq->Hdr, sizeof(*pReq));
    if (RT_SUCCESS(vrc))
        vrc = pReq->Call.header.result;
    return vrc;
}


/** Request structure for vboxSfOs2HostReqSetObjInfo.  */
typedef struct VBOXSFOBJINFOWITHBUFREQ
{
    VBGLIOCIDCHGCMFASTCALL  Hdr;
    VMMDevHGCMCall          Call;
    VBoxSFParmInformation   Parms;
    union
    {
        HGCMPageListInfo    PgLst;
        uint8_t             abPadding[8 + sizeof(RTGCPHYS64) * 2 /*RT_UOFFSETOF(HGCMPageListInfo, aPages[2])*/];
    } u;
} VBOXSFOBJINFOWITHBUFREQ;

/**
 * SHFL_FN_INFORMATION[SHFL_INFO_SET | SHFL_INFO_FILE] request, with separate
 * buffer (on the physical heap).
 */
DECLINLINE(int) vboxSfOs2HostReqSetObjInfoWithBuf(PVBOXSFFOLDER pFolder, VBOXSFOBJINFOWITHBUFREQ *pReq, uint64_t hHostFile,
                                                  PSHFLFSOBJINFO pObjInfo, uint32_t offObjInfoInAlloc)
{
    pReq->Parms.id32Root.type           = VMMDevHGCMParmType_32bit;
    pReq->Parms.id32Root.u.value32      = pFolder->hHostFolder.root;

    pReq->Parms.u64Handle.type          = VMMDevHGCMParmType_64bit;
    pReq->Parms.u64Handle.u.value64     = hHostFile;

    pReq->Parms.f32Flags.type           = VMMDevHGCMParmType_32bit;
    pReq->Parms.f32Flags.u.value32      = SHFL_INFO_SET | SHFL_INFO_FILE;

    pReq->Parms.cb32.type               = VMMDevHGCMParmType_32bit;
    pReq->Parms.cb32.u.value32          = sizeof(*pObjInfo);

    pReq->Parms.pInfo.type              = VMMDevHGCMParmType_PageList;
    pReq->Parms.pInfo.u.PageList.size   = sizeof(*pObjInfo);
    pReq->Parms.pInfo.u.PageList.offset = RT_UOFFSETOF(VBOXSFOBJINFOREQ, ObjInfo) - sizeof(VBGLIOCIDCHGCMFASTCALL);
    pReq->u.PgLst.flags                 = VBOX_HGCM_F_PARM_DIRECTION_BOTH;
    pReq->u.PgLst.aPages[0]             = VbglR0PhysHeapGetPhysAddr((uint8_t *)pObjInfo - offObjInfoInAlloc) + offObjInfoInAlloc;
    pReq->u.PgLst.offFirstPage          = (uint16_t)(pReq->u.PgLst.aPages[0] & PAGE_OFFSET_MASK);
    pReq->u.PgLst.aPages[0]            &= ~(RTGCPHYS)PAGE_OFFSET_MASK;
    uint32_t cbReq;
    if (PAGE_SIZE - pReq->u.PgLst.offFirstPage <= sizeof(*pObjInfo))
    {
        pReq->u.PgLst.cPages            = 1;
        cbReq = RT_UOFFSETOF(VBOXSFOBJINFOWITHBUFREQ, u.PgLst.aPages[1]);
    }
    else
    {
        pReq->u.PgLst.aPages[1]         = pReq->u.PgLst.aPages[0] + PAGE_SIZE;
        pReq->u.PgLst.cPages            = 2;
        cbReq = RT_UOFFSETOF(VBOXSFOBJINFOWITHBUFREQ, u.PgLst.aPages[2]);
    }

    VBGLIOCIDCHGCMFASTCALL_INIT(&pReq->Hdr, VbglR0PhysHeapGetPhysAddr(pReq), &pReq->Call, g_SfClient.idClient,
                                SHFL_FN_INFORMATION, SHFL_CPARMS_INFORMATION, cbReq);

    int vrc = VbglR0HGCMFastCall(g_SfClient.handle, &pReq->Hdr, cbReq);
    if (RT_SUCCESS(vrc))
        vrc = pReq->Call.header.result;
    return vrc;
}


/** Request structure for vboxSfOs2HostReqRemove.  */
typedef struct VBOXSFREMOVEREQ
{
    VBGLIOCIDCHGCMFASTCALL  Hdr;
    VMMDevHGCMCall          Call;
    VBoxSFParmRemove        Parms;
    SHFLSTRING              StrPath;
} VBOXSFREMOVEREQ;

/**
 * SHFL_FN_REMOVE request.
 */
DECLINLINE(int) vboxSfOs2HostReqRemove(PVBOXSFFOLDER pFolder, VBOXSFREMOVEREQ *pReq, uint32_t fFlags)
{
    VBGLIOCIDCHGCMFASTCALL_INIT(&pReq->Hdr, VbglR0PhysHeapGetPhysAddr(pReq), &pReq->Call, g_SfClient.idClient,
                                SHFL_FN_REMOVE, SHFL_CPARMS_REMOVE,
                                RT_UOFFSETOF(VBOXSFREMOVEREQ, StrPath.String) + pReq->StrPath.u16Size);

    pReq->Parms.id32Root.type                   = VMMDevHGCMParmType_32bit;
    pReq->Parms.id32Root.u.value32              = pFolder->hHostFolder.root;

    pReq->Parms.pStrPath.type                   = VMMDevHGCMParmType_Embedded;
    pReq->Parms.pStrPath.u.Embedded.cbData      = SHFLSTRING_HEADER_SIZE + pReq->StrPath.u16Size;
    pReq->Parms.pStrPath.u.Embedded.offData     = RT_UOFFSETOF(VBOXSFREMOVEREQ, StrPath) - sizeof(VBGLIOCIDCHGCMFASTCALL);
    pReq->Parms.pStrPath.u.Embedded.fFlags      = VBOX_HGCM_F_PARM_DIRECTION_TO_HOST;

    pReq->Parms.f32Flags.type                   = VMMDevHGCMParmType_32bit;
    pReq->Parms.f32Flags.u.value32              = fFlags;

    int vrc = VbglR0HGCMFastCall(g_SfClient.handle, &pReq->Hdr,
                                 RT_UOFFSETOF(VBOXSFREMOVEREQ, StrPath.String) + pReq->StrPath.u16Size);
    if (RT_SUCCESS(vrc))
        vrc = pReq->Call.header.result;
    return vrc;
}


/** Request structure for vboxSfOs2HostReqRename.  */
typedef struct VBOXSFRENAMEWITHSRCBUFREQ
{
    VBGLIOCIDCHGCMFASTCALL  Hdr;
    VMMDevHGCMCall          Call;
    VBoxSFParmRename        Parms;
    union
    {
        HGCMPageListInfo    PgLst;
        uint8_t             abPadding[8 + sizeof(RTGCPHYS64) * 2 /*RT_UOFFSETOF(HGCMPageListInfo, aPages[2])*/];
    } u;
    SHFLSTRING              StrDstPath;
} VBOXSFRENAMEWITHSRCBUFREQ;

/**
 * SHFL_FN_REMOVE request.
 */
DECLINLINE(int) vboxSfOs2HostReqRenameWithSrcBuf(PVBOXSFFOLDER pFolder, VBOXSFRENAMEWITHSRCBUFREQ *pReq,
                                                 PSHFLSTRING pSrcStr, uint32_t fFlags)
{
    VBGLIOCIDCHGCMFASTCALL_INIT(&pReq->Hdr, VbglR0PhysHeapGetPhysAddr(pReq), &pReq->Call, g_SfClient.idClient,
                                SHFL_FN_RENAME, SHFL_CPARMS_RENAME,
                                RT_UOFFSETOF(VBOXSFRENAMEWITHSRCBUFREQ, StrDstPath.String) + pReq->StrDstPath.u16Size);

    pReq->Parms.id32Root.type                   = VMMDevHGCMParmType_32bit;
    pReq->Parms.id32Root.u.value32              = pFolder->hHostFolder.root;

    /** @todo Using page lists for contiguous buffers sucks.   */
    AssertReturn(pSrcStr->u16Size <= PAGE_SIZE - SHFLSTRING_HEADER_SIZE, VERR_FILENAME_TOO_LONG);
    pReq->Parms.pStrSrcPath.type                = VMMDevHGCMParmType_PageList;
    pReq->Parms.pStrSrcPath.u.PageList.size     = SHFLSTRING_HEADER_SIZE + pSrcStr->u16Size;
    pReq->Parms.pStrSrcPath.u.PageList.offset   = RT_UOFFSETOF(VBOXSFRENAMEWITHSRCBUFREQ, u.PgLst) - sizeof(VBGLIOCIDCHGCMFASTCALL);
    pReq->u.PgLst.flags                         = VBOX_HGCM_F_PARM_DIRECTION_TO_HOST;
    pReq->u.PgLst.aPages[0]                     = VbglR0PhysHeapGetPhysAddr(pSrcStr);
    pReq->u.PgLst.offFirstPage                  = (uint16_t)(pReq->u.PgLst.aPages[0] & PAGE_OFFSET_MASK);
    pReq->u.PgLst.aPages[0]                    &= ~(RTGCPHYS)PAGE_OFFSET_MASK;
    if (PAGE_SIZE - pReq->u.PgLst.offFirstPage <= SHFLSTRING_HEADER_SIZE + pSrcStr->u16Size)
    {
        pReq->u.PgLst.aPages[1]                 = NIL_RTGCPHYS64;
        pReq->u.PgLst.cPages                    = 1;
    }
    else
    {
        pReq->u.PgLst.aPages[1]                 = pReq->u.PgLst.aPages[0] + PAGE_SIZE;
        pReq->u.PgLst.cPages                    = 2;
    }

    pReq->Parms.pStrDstPath.type                = VMMDevHGCMParmType_Embedded;
    pReq->Parms.pStrDstPath.u.Embedded.cbData   = SHFLSTRING_HEADER_SIZE + pReq->StrDstPath.u16Size;
    pReq->Parms.pStrDstPath.u.Embedded.offData  = RT_UOFFSETOF(VBOXSFRENAMEWITHSRCBUFREQ, StrDstPath) - sizeof(VBGLIOCIDCHGCMFASTCALL);
    pReq->Parms.pStrDstPath.u.Embedded.fFlags   = VBOX_HGCM_F_PARM_DIRECTION_TO_HOST;

    pReq->Parms.f32Flags.type                   = VMMDevHGCMParmType_32bit;
    pReq->Parms.f32Flags.u.value32              = fFlags;

    int vrc = VbglR0HGCMFastCall(g_SfClient.handle, &pReq->Hdr,
                                 RT_UOFFSETOF(VBOXSFRENAMEWITHSRCBUFREQ, StrDstPath.String) + pReq->StrDstPath.u16Size);
    if (RT_SUCCESS(vrc))
        vrc = pReq->Call.header.result;
    return vrc;
}


/** Request structure for vboxSfOs2HostReqFlush.  */
typedef struct VBOXSFFLUSHREQ
{
    VBGLIOCIDCHGCMFASTCALL  Hdr;
    VMMDevHGCMCall          Call;
    VBoxSFParmFlush         Parms;
} VBOXSFFLUSHREQ;

/**
 * SHFL_FN_FLUSH request.
 */
DECLINLINE(int) vboxSfOs2HostReqFlush(PVBOXSFFOLDER pFolder, VBOXSFFLUSHREQ *pReq, uint64_t hHostFile)
{
    VBGLIOCIDCHGCMFASTCALL_INIT(&pReq->Hdr, VbglR0PhysHeapGetPhysAddr(pReq), &pReq->Call, g_SfClient.idClient,
                                SHFL_FN_FLUSH, SHFL_CPARMS_FLUSH, sizeof(*pReq));

    pReq->Parms.id32Root.type                   = VMMDevHGCMParmType_32bit;
    pReq->Parms.id32Root.u.value32              = pFolder->hHostFolder.root;

    pReq->Parms.u64Handle.type                  = VMMDevHGCMParmType_64bit;
    pReq->Parms.u64Handle.u.value64             = hHostFile;

    int vrc = VbglR0HGCMFastCall(g_SfClient.handle, &pReq->Hdr, sizeof(*pReq));
    if (RT_SUCCESS(vrc))
        vrc = pReq->Call.header.result;
    return vrc;
}

/**
 * SHFL_FN_FLUSH request, allocate request buffer.
 */
DECLINLINE(int) vboxSfOs2HostReqFlushSimple(PVBOXSFFOLDER pFolder, uint64_t hHostFile)
{
    VBOXSFFLUSHREQ *pReq = (VBOXSFFLUSHREQ *)VbglR0PhysHeapAlloc(sizeof(*pReq));
    if (pReq)
    {
        int vrc = vboxSfOs2HostReqFlush(pFolder, pReq, hHostFile);
        VbglR0PhysHeapFree(pReq);
        return vrc;
    }
    return VERR_NO_MEMORY;
}


/** Request structure for vboxSfOs2HostReqSetFileSize.  */
typedef struct VBOXSFSETFILESIZEREQ
{
    VBGLIOCIDCHGCMFASTCALL  Hdr;
    VMMDevHGCMCall          Call;
    VBoxSFParmSetFileSize   Parms;
} VBOXSFSETFILESIZEREQ;

/**
 * SHFL_FN_SET_FILE_SIZE request.
 */
DECLINLINE(int) vboxSfOs2HostReqSetFileSize(PVBOXSFFOLDER pFolder, VBOXSFSETFILESIZEREQ *pReq,
                                            uint64_t hHostFile, uint64_t cbNewSize)
{
    VBGLIOCIDCHGCMFASTCALL_INIT(&pReq->Hdr, VbglR0PhysHeapGetPhysAddr(pReq), &pReq->Call, g_SfClient.idClient,
                                SHFL_FN_SET_FILE_SIZE, SHFL_CPARMS_SET_FILE_SIZE, sizeof(*pReq));

    pReq->Parms.id32Root.type                   = VMMDevHGCMParmType_32bit;
    pReq->Parms.id32Root.u.value32              = pFolder->hHostFolder.root;

    pReq->Parms.u64Handle.type                  = VMMDevHGCMParmType_64bit;
    pReq->Parms.u64Handle.u.value64             = hHostFile;

    pReq->Parms.cb64NewSize.type                = VMMDevHGCMParmType_64bit;
    pReq->Parms.cb64NewSize.u.value64           = cbNewSize;

    int vrc = VbglR0HGCMFastCall(g_SfClient.handle, &pReq->Hdr, sizeof(*pReq));
    if (RT_SUCCESS(vrc))
        vrc = pReq->Call.header.result;
    return vrc;
}

/**
 * SHFL_FN_SET_FILE_SIZE request, allocate request buffer.
 */
DECLINLINE(int) vboxSfOs2HostReqSetFileSizeSimple(PVBOXSFFOLDER pFolder, uint64_t hHostFile, uint64_t cbNewSize)
{
    VBOXSFSETFILESIZEREQ *pReq = (VBOXSFSETFILESIZEREQ *)VbglR0PhysHeapAlloc(sizeof(*pReq));
    if (pReq)
    {
        int vrc = vboxSfOs2HostReqSetFileSize(pFolder, pReq, hHostFile, cbNewSize);
        VbglR0PhysHeapFree(pReq);
        return vrc;
    }
    return VERR_NO_MEMORY;
}


/** Request structure for vboxSfOs2HostReqReadEmbedded. */
typedef struct VBOXSFREADEMBEDDEDREQ
{
    VBGLIOCIDCHGCMFASTCALL  Hdr;
    VMMDevHGCMCall          Call;
    VBoxSFParmRead          Parms;
    uint8_t                 abData[RT_FLEXIBLE_ARRAY];
} VBOXSFREADEMBEDDEDREQ;

/**
 * SHFL_FN_READ request using embedded data buffer.
 */
DECLINLINE(int) vboxSfOs2HostReqReadEmbedded(PVBOXSFFOLDER pFolder, VBOXSFREADEMBEDDEDREQ *pReq, uint64_t hHostFile,
                                             uint64_t offRead, uint32_t cbToRead)
{
    VBGLIOCIDCHGCMFASTCALL_INIT(&pReq->Hdr, VbglR0PhysHeapGetPhysAddr(pReq), &pReq->Call, g_SfClient.idClient,
                                SHFL_FN_READ, SHFL_CPARMS_READ, RT_UOFFSETOF(VBOXSFREADEMBEDDEDREQ, abData[0]) + cbToRead);

    pReq->Parms.id32Root.type               = VMMDevHGCMParmType_32bit;
    pReq->Parms.id32Root.u.value32          = pFolder->hHostFolder.root;

    pReq->Parms.u64Handle.type              = VMMDevHGCMParmType_64bit;
    pReq->Parms.u64Handle.u.value64         = hHostFile;

    pReq->Parms.off64Read.type              = VMMDevHGCMParmType_64bit;
    pReq->Parms.off64Read.u.value64         = offRead;

    pReq->Parms.cb32Read.type               = VMMDevHGCMParmType_32bit;
    pReq->Parms.cb32Read.u.value32          = cbToRead;

    pReq->Parms.pBuf.type                   = VMMDevHGCMParmType_Embedded;
    pReq->Parms.pBuf.u.Embedded.cbData      = cbToRead;
    pReq->Parms.pBuf.u.Embedded.offData     = RT_UOFFSETOF(VBOXSFREADEMBEDDEDREQ, abData[0]) - sizeof(VBGLIOCIDCHGCMFASTCALL);
    pReq->Parms.pBuf.u.Embedded.fFlags      = VBOX_HGCM_F_PARM_DIRECTION_FROM_HOST;

    int vrc = VbglR0HGCMFastCall(g_SfClient.handle, &pReq->Hdr, RT_UOFFSETOF(VBOXSFREADEMBEDDEDREQ, abData[0]) + cbToRead);
    if (RT_SUCCESS(vrc))
        vrc = pReq->Call.header.result;
    return vrc;
}


/** Request structure for vboxSfOs2HostReqRead. */
typedef struct VBOXSFREADPGLSTREQ
{
    VBGLIOCIDCHGCMFASTCALL  Hdr;
    VMMDevHGCMCall          Call;
    VBoxSFParmRead          Parms;
    HGCMPageListInfo        PgLst;
} VBOXSFREADPGLSTREQ;

/**
 * SHFL_FN_READ request using page list for data buffer (caller populated).
 */
DECLINLINE(int) vboxSfOs2HostReqReadPgLst(PVBOXSFFOLDER pFolder, VBOXSFREADPGLSTREQ *pReq, uint64_t hHostFile,
                                          uint64_t offRead, uint32_t cbToRead, uint32_t cPages)
{
    VBGLIOCIDCHGCMFASTCALL_INIT(&pReq->Hdr, VbglR0PhysHeapGetPhysAddr(pReq), &pReq->Call, g_SfClient.idClient,
                                SHFL_FN_READ, SHFL_CPARMS_READ,
                                RT_UOFFSETOF_DYN(VBOXSFREADPGLSTREQ, PgLst.aPages[cPages]));

    pReq->Parms.id32Root.type               = VMMDevHGCMParmType_32bit;
    pReq->Parms.id32Root.u.value32          = pFolder->hHostFolder.root;

    pReq->Parms.u64Handle.type              = VMMDevHGCMParmType_64bit;
    pReq->Parms.u64Handle.u.value64         = hHostFile;

    pReq->Parms.off64Read.type              = VMMDevHGCMParmType_64bit;
    pReq->Parms.off64Read.u.value64         = offRead;

    pReq->Parms.cb32Read.type               = VMMDevHGCMParmType_32bit;
    pReq->Parms.cb32Read.u.value32          = cbToRead;

    pReq->Parms.pBuf.type                   = VMMDevHGCMParmType_PageList;
    pReq->Parms.pBuf.u.PageList.size        = cbToRead;
    pReq->Parms.pBuf.u.PageList.offset      = RT_UOFFSETOF(VBOXSFREADPGLSTREQ, PgLst) - sizeof(VBGLIOCIDCHGCMFASTCALL);
    pReq->PgLst.flags                       = VBOX_HGCM_F_PARM_DIRECTION_FROM_HOST;
    pReq->PgLst.cPages                      = (uint16_t)cPages;
    AssertReturn(cPages <= UINT16_MAX, VERR_OUT_OF_RANGE);
    /* caller sets offset */

    int vrc = VbglR0HGCMFastCall(g_SfClient.handle, &pReq->Hdr,
                                 RT_UOFFSETOF_DYN(VBOXSFREADPGLSTREQ, PgLst.aPages[cPages]));
    if (RT_SUCCESS(vrc))
        vrc = pReq->Call.header.result;
    return vrc;
}



/** Request structure for vboxSfOs2HostReqWriteEmbedded. */
typedef struct VBOXSFWRITEEMBEDDEDREQ
{
    VBGLIOCIDCHGCMFASTCALL  Hdr;
    VMMDevHGCMCall          Call;
    VBoxSFParmWrite         Parms;
    uint8_t                 abData[RT_FLEXIBLE_ARRAY];
} VBOXSFWRITEEMBEDDEDREQ;

/**
 * SHFL_FN_WRITE request using embedded data buffer.
 */
DECLINLINE(int) vboxSfOs2HostReqWriteEmbedded(PVBOXSFFOLDER pFolder, VBOXSFWRITEEMBEDDEDREQ *pReq, uint64_t hHostFile,
                                              uint64_t offWrite, uint32_t cbToWrite)
{
    VBGLIOCIDCHGCMFASTCALL_INIT(&pReq->Hdr, VbglR0PhysHeapGetPhysAddr(pReq), &pReq->Call, g_SfClient.idClient,
                                SHFL_FN_WRITE, SHFL_CPARMS_WRITE, RT_UOFFSETOF(VBOXSFWRITEEMBEDDEDREQ, abData[0]) + cbToWrite);

    pReq->Parms.id32Root.type               = VMMDevHGCMParmType_32bit;
    pReq->Parms.id32Root.u.value32          = pFolder->hHostFolder.root;

    pReq->Parms.u64Handle.type              = VMMDevHGCMParmType_64bit;
    pReq->Parms.u64Handle.u.value64         = hHostFile;

    pReq->Parms.off64Write.type             = VMMDevHGCMParmType_64bit;
    pReq->Parms.off64Write.u.value64        = offWrite;

    pReq->Parms.cb32Write.type              = VMMDevHGCMParmType_32bit;
    pReq->Parms.cb32Write.u.value32         = cbToWrite;

    pReq->Parms.pBuf.type                   = VMMDevHGCMParmType_Embedded;
    pReq->Parms.pBuf.u.Embedded.cbData      = cbToWrite;
    pReq->Parms.pBuf.u.Embedded.offData     = RT_UOFFSETOF(VBOXSFWRITEEMBEDDEDREQ, abData[0]) - sizeof(VBGLIOCIDCHGCMFASTCALL);
    pReq->Parms.pBuf.u.Embedded.fFlags      = VBOX_HGCM_F_PARM_DIRECTION_TO_HOST;

    int vrc = VbglR0HGCMFastCall(g_SfClient.handle, &pReq->Hdr, RT_UOFFSETOF(VBOXSFWRITEEMBEDDEDREQ, abData[0]) + cbToWrite);
    if (RT_SUCCESS(vrc))
        vrc = pReq->Call.header.result;
    return vrc;
}


/** Request structure for vboxSfOs2HostReqWrite. */
typedef struct VBOXSFWRITEPGLSTREQ
{
    VBGLIOCIDCHGCMFASTCALL  Hdr;
    VMMDevHGCMCall          Call;
    VBoxSFParmWrite         Parms;
    HGCMPageListInfo        PgLst;
} VBOXSFWRITEPGLSTREQ;

/**
 * SHFL_FN_WRITE request using page list for data buffer (caller populated).
 */
DECLINLINE(int) vboxSfOs2HostReqWritePgLst(PVBOXSFFOLDER pFolder, VBOXSFWRITEPGLSTREQ *pReq, uint64_t hHostFile,
                                           uint64_t offWrite, uint32_t cbToWrite, uint32_t cPages)
{
    VBGLIOCIDCHGCMFASTCALL_INIT(&pReq->Hdr, VbglR0PhysHeapGetPhysAddr(pReq), &pReq->Call, g_SfClient.idClient,
                                SHFL_FN_WRITE, SHFL_CPARMS_WRITE,
                                RT_UOFFSETOF_DYN(VBOXSFWRITEPGLSTREQ, PgLst.aPages[cPages]));

    pReq->Parms.id32Root.type               = VMMDevHGCMParmType_32bit;
    pReq->Parms.id32Root.u.value32          = pFolder->hHostFolder.root;

    pReq->Parms.u64Handle.type              = VMMDevHGCMParmType_64bit;
    pReq->Parms.u64Handle.u.value64         = hHostFile;

    pReq->Parms.off64Write.type             = VMMDevHGCMParmType_64bit;
    pReq->Parms.off64Write.u.value64        = offWrite;

    pReq->Parms.cb32Write.type              = VMMDevHGCMParmType_32bit;
    pReq->Parms.cb32Write.u.value32         = cbToWrite;

    pReq->Parms.pBuf.type                   = VMMDevHGCMParmType_PageList;
    pReq->Parms.pBuf.u.PageList.size        = cbToWrite;
    pReq->Parms.pBuf.u.PageList.offset      = RT_UOFFSETOF(VBOXSFWRITEPGLSTREQ, PgLst) - sizeof(VBGLIOCIDCHGCMFASTCALL);
    pReq->PgLst.flags                       = VBOX_HGCM_F_PARM_DIRECTION_TO_HOST;
    pReq->PgLst.cPages                      = (uint16_t)cPages;
    AssertReturn(cPages <= UINT16_MAX, VERR_OUT_OF_RANGE);
    /* caller sets offset */

    int vrc = VbglR0HGCMFastCall(g_SfClient.handle, &pReq->Hdr,
                                 RT_UOFFSETOF_DYN(VBOXSFWRITEPGLSTREQ, PgLst.aPages[cPages]));
    if (RT_SUCCESS(vrc))
        vrc = pReq->Call.header.result;
    return vrc;
}


/**
 * SHFL_FN_LIST request with separate string buffer and buffers for entries,
 * both allocated on the physical heap.
 */
DECLINLINE(int) vboxSfOs2HostReqListDir(PVBOXSFFOLDER pFolder, VBOXSFLISTDIRREQ *pReq, uint64_t hHostDir,
                                        PSHFLSTRING pFilter, uint32_t fFlags, PSHFLDIRINFO pBuffer, uint32_t cbBuffer)
{
    VBGLIOCIDCHGCMFASTCALL_INIT(&pReq->Hdr, VbglR0PhysHeapGetPhysAddr(pReq), &pReq->Call, g_SfClient.idClient,
                                SHFL_FN_LIST, SHFL_CPARMS_LIST, sizeof(*pReq));

    pReq->Parms.id32Root.type                   = VMMDevHGCMParmType_32bit;
    pReq->Parms.id32Root.u.value32              = pFolder->hHostFolder.root;

    pReq->Parms.u64Handle.type                  = VMMDevHGCMParmType_64bit;
    pReq->Parms.u64Handle.u.value64             = hHostDir;

    pReq->Parms.f32Flags.type                   = VMMDevHGCMParmType_32bit;
    pReq->Parms.f32Flags.u.value32              = fFlags;

    pReq->Parms.cb32Buffer.type                 = VMMDevHGCMParmType_32bit;
    pReq->Parms.cb32Buffer.u.value32            = cbBuffer;

    pReq->Parms.pStrFilter.type                 = VMMDevHGCMParmType_ContiguousPageList;
    pReq->Parms.pStrFilter.u.PageList.offset    = RT_UOFFSETOF(VBOXSFLISTDIRREQ, StrPgLst) - sizeof(VBGLIOCIDCHGCMFASTCALL);
    pReq->StrPgLst.flags                        = VBOX_HGCM_F_PARM_DIRECTION_TO_HOST;
    pReq->StrPgLst.cPages                       = 1;
    if (pFilter)
    {
        pReq->Parms.pStrFilter.u.PageList.size  = SHFLSTRING_HEADER_SIZE + pFilter->u16Size;
        RTGCPHYS32 const GCPhys       = VbglR0PhysHeapGetPhysAddr(pFilter);
        uint32_t const   offFirstPage = GCPhys & PAGE_OFFSET_MASK;
        pReq->StrPgLst.offFirstPage             = (uint16_t)offFirstPage;
        pReq->StrPgLst.aPages[0]                = GCPhys - offFirstPage;
    }
    else
    {
        pReq->Parms.pStrFilter.u.PageList.size  = 0;
        pReq->StrPgLst.offFirstPage             = 0;
        pReq->StrPgLst.aPages[0]                = NIL_RTGCPHYS64;
    }

    pReq->Parms.pBuffer.type                    = VMMDevHGCMParmType_ContiguousPageList;
    pReq->Parms.pBuffer.u.PageList.offset       = RT_UOFFSETOF(VBOXSFLISTDIRREQ, BufPgLst) - sizeof(VBGLIOCIDCHGCMFASTCALL);
    pReq->Parms.pBuffer.u.PageList.size         = cbBuffer;
    pReq->BufPgLst.flags                        = VBOX_HGCM_F_PARM_DIRECTION_FROM_HOST;
    pReq->BufPgLst.cPages                       = 1;
    RTGCPHYS32 const GCPhys       = VbglR0PhysHeapGetPhysAddr(pBuffer);
    uint32_t const   offFirstPage = GCPhys & PAGE_OFFSET_MASK;
    pReq->BufPgLst.offFirstPage                 = (uint16_t)offFirstPage;
    pReq->BufPgLst.aPages[0]                    = GCPhys - offFirstPage;

    pReq->Parms.f32Done.type                    = VMMDevHGCMParmType_32bit;
    pReq->Parms.f32Done.u.value32               = 0;

    pReq->Parms.c32Entries.type                 = VMMDevHGCMParmType_32bit;
    pReq->Parms.c32Entries.u.value32            = 0;

    int vrc = VbglR0HGCMFastCall(g_SfClient.handle, &pReq->Hdr, sizeof(*pReq));
    if (RT_SUCCESS(vrc))
        vrc = pReq->Call.header.result;
    return vrc;
}

/** @} */

#endif /* !GA_INCLUDED_SRC_os2_VBoxSF_VBoxSFInternal_h */

