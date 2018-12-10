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

#ifndef ___VBoxSFInternal_h___
#define ___VBoxSFInternal_h___


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


/**
 * VBoxSF File Search Buffer (header).
 */
typedef struct VBOXSFFSBUF
{
    /** A magic number (VBOXSFFSBUF_MAGIC). */
    uint32_t            u32Magic;
    /** Amount of buffer space allocated after this header. */
    uint32_t            cbBuf;
    /** The filter string (full path), NULL if all files are request. */
    PSHFLSTRING         pFilter;
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
    /** Staging area for staging a full FILEFINDBUF4L (+ 32 safe bytes). */
    uint8_t             abStaging[RT_ALIGN_32(sizeof(FILEFINDBUF4L) + 32, 8)];
} VBOXSFFSBUF;
AssertCompileSizeAlignment(VBOXSFFSBUF, 8);
/** Pointer to a file search buffer. */
typedef VBOXSFFSBUF *PVBOXSFFSBUF;
/** Magic number for VBOXSFFSBUF (Robert Anson Heinlein). */
#define VBOXSFFSBUF_MAGIC       UINT32_C(0x19070707)
/** Minimum buffer size. */
#define VBOXSFFSBUF_MIN_SIZE (  RT_ALIGN_32(sizeof(VBOXSFFSBUF) + sizeof(SHFLDIRINFO) + CCHMAXPATHCOMP * 4 + ALLOC_HDR_SIZE, 64) \
                              - ALLOC_HDR_SIZE)


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
                                         PFILESTATUS pTimestamps, PSHFLFSOBJINFO pObjInfoBuf);
APIRET      vboxSfOs2MakeEmptyEaList(PEAOP pEaOp, ULONG uLevel);
APIRET      vboxSfOs2MakeEmptyEaListEx(PEAOP pEaOp, ULONG uLevel, uint32_t *pcbWritten, ULONG *poffError);

DECLASM(PVBOXSFVP) Fsh32GetVolParams(USHORT hVbp, PVPFSI *ppVpFsi /*optional*/);



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


/** Request structure for vboxSfOs2HostReqCreate.  */
typedef struct VBOXSFCLOSEREQ
{
    VBGLIOCIDCHGCMFASTCALL  Hdr;
    VMMDevHGCMCall          Call;
    VBoxSFParmClose         Parms;
} VBOXSFCLOSEREQ;

/**
 * SHFL_FN_CLOSE request.
 */
DECLINLINE(int) vboxSfOs2HostReqClose(PVBOXSFFOLDER pFolder, VBOXSFCLOSEREQ *pReq, uint64_t uHandle)
{
    VBGLIOCIDCHGCMFASTCALL_INIT(&pReq->Hdr, VbglR0PhysHeapGetPhysAddr(pReq), &pReq->Call, g_SfClient.idClient,
                                SHFL_FN_CLOSE, SHFL_CPARMS_CLOSE, sizeof(*pReq));

    pReq->Parms.id32Root.type                   = VMMDevHGCMParmType_32bit;
    pReq->Parms.id32Root.u.value32              = pFolder->hHostFolder.root;

    pReq->Parms.u64Handle.type                  = VMMDevHGCMParmType_64bit;
    pReq->Parms.u64Handle.u.value64             = uHandle;

    int vrc = VbglR0HGCMFastCall(g_SfClient.handle, &pReq->Hdr, sizeof(*pReq));
    if (RT_SUCCESS(vrc))
        vrc = pReq->Call.header.result;
    return vrc;
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
DECLINLINE(int) vboxSfOs2HostReqQueryVolInfo(PVBOXSFFOLDER pFolder, VBOXSFVOLINFOREQ *pReq, uint64_t uHandle)
{
    VBGLIOCIDCHGCMFASTCALL_INIT(&pReq->Hdr, VbglR0PhysHeapGetPhysAddr(pReq), &pReq->Call, g_SfClient.idClient,
                                SHFL_FN_INFORMATION, SHFL_CPARMS_INFORMATION, sizeof(*pReq));

    pReq->Parms.id32Root.type                   = VMMDevHGCMParmType_32bit;
    pReq->Parms.id32Root.u.value32              = pFolder->hHostFolder.root;

    pReq->Parms.u64Handle.type                  = VMMDevHGCMParmType_64bit;
    pReq->Parms.u64Handle.u.value64             = uHandle;

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


#endif

