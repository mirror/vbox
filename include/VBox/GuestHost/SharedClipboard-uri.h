/* $Id$ */
/** @file
 * Shared Clipboard - Shared URI functions between host and guest.
 */

/*
 * Copyright (C) 2019 Oracle Corporation
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

#ifndef VBOX_INCLUDED_GuestHost_SharedClipboard_uri_h
#define VBOX_INCLUDED_GuestHost_SharedClipboard_uri_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/assert.h>
#include <iprt/critsect.h>
#include <iprt/fs.h>
#include <iprt/list.h>

#include <iprt/cpp/list.h>
#include <iprt/cpp/ministring.h>

#include <VBox/HostServices/VBoxClipboardSvc.h>

/** Clipboard area ID. A valid area is >= 1.
 *  If 0 is specified, the last (most recent) area is meant.
 *  Set to UINT32_MAX if not initialized. */
typedef uint32_t SHAREDCLIPBOARDAREAID;

/** Defines a non-initialized (nil) clipboard area. */
#define NIL_SHAREDCLIPBOARDAREAID       UINT32_MAX

/** SharedClipboardArea open flags. */
typedef uint32_t SHAREDCLIPBOARDAREAOPENFLAGS;

/** No open flags specified. */
#define SHAREDCLIPBOARDAREA_OPEN_FLAGS_NONE               0
/** The clipboard area must not exist yet. */
#define SHAREDCLIPBOARDAREA_OPEN_FLAGS_MUST_NOT_EXIST     RT_BIT(0)

/**
 * Class for maintaining a Shared Clipboard area
 * on the host or guest. This will contain all received files & directories
 * for a single Shared Clipboard operation.
 *
 * In case of a failed Shared Clipboard operation this class can also
 * perform a gentle rollback if required.
 */
class SharedClipboardArea
{
public:

    SharedClipboardArea(void);
    SharedClipboardArea(const char *pszPath, SHAREDCLIPBOARDAREAID uID = NIL_SHAREDCLIPBOARDAREAID,
                        SHAREDCLIPBOARDAREAOPENFLAGS fFlags = SHAREDCLIPBOARDAREA_OPEN_FLAGS_NONE);
    virtual ~SharedClipboardArea(void);

public:

    uint32_t AddRef(void);
    uint32_t Release(void);

    int Lock(void);
    int Unlock(void);

    int AddFile(const char *pszFile);
    int AddDir(const char *pszDir);
    int Close(void);
    bool IsOpen(void) const;
    int OpenEx(const char *pszPath, SHAREDCLIPBOARDAREAID uID = NIL_SHAREDCLIPBOARDAREAID,
               SHAREDCLIPBOARDAREAOPENFLAGS fFlags = SHAREDCLIPBOARDAREA_OPEN_FLAGS_NONE);
    int OpenTemp(SHAREDCLIPBOARDAREAID uID = NIL_SHAREDCLIPBOARDAREAID,
                 SHAREDCLIPBOARDAREAOPENFLAGS fFlags = SHAREDCLIPBOARDAREA_OPEN_FLAGS_NONE);
    SHAREDCLIPBOARDAREAID GetID(void) const;
    const char *GetDirAbs(void) const;
    uint32_t GetRefCount(void);
    int Reopen(void);
    int Reset(bool fDeleteContent);
    int Rollback(void);

public:

    static int PathConstruct(const char *pszBase, SHAREDCLIPBOARDAREAID uID, char *pszPath, size_t cbPath);

protected:

    int initInternal(void);
    int destroyInternal(void);
    int closeInternal(void);

protected:

    /** Creation timestamp (in ms). */
    uint64_t                     m_tsCreatedMs;
    /** Number of references to this instance. */
    volatile uint32_t            m_cRefs;
    /** Critical section for serializing access. */
    RTCRITSECT                   m_CritSect;
    /** Open flags. */
    uint32_t                     m_fOpen;
    /** Directory handle for drop directory. */
    RTDIR                        m_hDir;
    /** Absolute path to drop directory. */
    RTCString                    m_strPathAbs;
    /** List for holding created directories in the case of a rollback. */
    RTCList<RTCString>           m_lstDirs;
    /** List for holding created files in the case of a rollback. */
    RTCList<RTCString>           m_lstFiles;
    /** Associated clipboard area ID. */
    SHAREDCLIPBOARDAREAID        m_uID;
};

int SharedClipboardPathSanitizeFilename(char *pszPath, size_t cbPath);
int SharedClipboardPathSanitize(char *pszPath, size_t cbPath);

/** SharedClipboardURIObject flags. */
typedef uint32_t SHAREDCLIPBOARDURIOBJECTFLAGS;

/** No flags specified. */
#define SHAREDCLIPBOARDURIOBJECT_FLAGS_NONE                   0

/** Mask of all valid Shared Clipboard URI object flags. */
#define SHAREDCLIPBOARDURIOBJECT_FLAGS_VALID_MASK             UINT32_C(0x0)

/**
 * Class for handling Shared Clipboard URI objects.
 * This class abstracts the access and handling objects when performing Shared Clipboard actions.
 */
class SharedClipboardURIObject
{
public:

    /**
     * Enumeration for specifying an URI object type.
     */
    enum Type
    {
        /** Unknown type, do not use. */
        Type_Unknown = 0,
        /** Object is a file. */
        Type_File,
        /** Object is a directory. */
        Type_Directory,
        /** The usual 32-bit hack. */
        Type_32Bit_Hack = 0x7fffffff
    };

    /**
     * Enumeration for specifying an URI object view
     * for representing its data accordingly.
     */
    enum View
    {
        /** Unknown view, do not use. */
        View_Unknown = 0,
        /** Handle data from the source point of view. */
        View_Source,
        /** Handle data from the destination point of view. */
        View_Target,
        /** The usual 32-bit hack. */
        View_Dest_32Bit_Hack = 0x7fffffff
    };

    SharedClipboardURIObject(void);
    SharedClipboardURIObject(Type type, const RTCString &strSrcPathAbs = "", const RTCString &strDstPathAbs = "");
    virtual ~SharedClipboardURIObject(void);

public:

    /**
     * Returns the given absolute source path of the object.
     *
     * @return  Absolute source path of the object.
     */
    const RTCString &GetSourcePathAbs(void) const { return m_strSrcPathAbs; }

    /**
     * Returns the given, absolute destination path of the object.
     *
     * @return  Absolute destination path of the object.
     */
    const RTCString &GetDestPathAbs(void) const { return m_strTgtPathAbs; }

    RTFMODE GetMode(void) const;

    uint64_t GetProcessed(void) const;

    uint64_t GetSize(void) const;

    /**
     * Returns the object's type.
     *
     * @return  The object's type.
     */
    Type GetType(void) const { return m_enmType; }

    /**
     * Returns the object's view.
     *
     * @return  The object's view.
     */
    View GetView(void) const { return m_enmView; }

public:

    int SetSize(uint64_t cbSize);

public:

    void Close(void);
    bool IsComplete(void) const;
    bool IsOpen(void) const;
    int OpenDirectory(View enmView, uint32_t fCreate = 0, RTFMODE fMode = 0);
    int OpenDirectoryEx(const RTCString &strPathAbs, View enmView,
                        uint32_t fCreate = 0, RTFMODE fMode = 0,
                        SHAREDCLIPBOARDURIOBJECTFLAGS fFlags = SHAREDCLIPBOARDURIOBJECT_FLAGS_NONE);
    int OpenFile(View enmView, uint64_t fOpen = 0, RTFMODE fMode = 0);
    int OpenFileEx(const RTCString &strPathAbs, View enmView,
                   uint64_t fOpen = 0, RTFMODE fMode = 0,
                   SHAREDCLIPBOARDURIOBJECTFLAGS fFlags  = SHAREDCLIPBOARDURIOBJECT_FLAGS_NONE);
    int QueryInfo(View enmView);
    int Read(void *pvBuf, size_t cbBuf, uint32_t *pcbRead);
    void Reset(void);
    int SetDirectoryData(const RTCString &strPathAbs, View enmView, uint32_t fOpen = 0, RTFMODE fMode = 0,
                         SHAREDCLIPBOARDURIOBJECTFLAGS fFlags = SHAREDCLIPBOARDURIOBJECT_FLAGS_NONE);
    int SetFileData(const RTCString &strPathAbs, View enmView, uint64_t fOpen = 0, RTFMODE fMode = 0,
                    SHAREDCLIPBOARDURIOBJECTFLAGS fFlags = SHAREDCLIPBOARDURIOBJECT_FLAGS_NONE);
    int Write(const void *pvBuf, size_t cbBuf, uint32_t *pcbWritten);

public:

    static int RebaseURIPath(RTCString &strPath, const RTCString &strBaseOld = "", const RTCString &strBaseNew = "");

protected:

    void closeInternal(void);
    int setDirectoryDataInternal(const RTCString &strPathAbs, View enmView, uint32_t fCreate = 0, RTFMODE fMode = 0,
                                 SHAREDCLIPBOARDURIOBJECTFLAGS fFlags = SHAREDCLIPBOARDURIOBJECT_FLAGS_NONE);
    int setFileDataInternal(const RTCString &strPathAbs, View enmView, uint64_t fOpen = 0, RTFMODE fMode = 0,
                            SHAREDCLIPBOARDURIOBJECTFLAGS fFlags = SHAREDCLIPBOARDURIOBJECT_FLAGS_NONE);
    int queryInfoInternal(View enmView);

protected:

    /** The object's type. */
    Type      m_enmType;
    /** The object's view. */
    View      m_enmView;
    /** Absolute path (base) for the source. */
    RTCString m_strSrcPathAbs;
    /** Absolute path (base) for the target. */
    RTCString m_strTgtPathAbs;
    /** Saved SHAREDCLIPBOARDURIOBJECT_FLAGS. */
    uint32_t  m_fFlags;
    /** Requested file mode.
     *  Note: The actual file mode of an opened file will be in objInfo. */
    RTFMODE   m_fModeRequested;

    /** Union containing data depending on the object's type. */
    union
    {
        /** Structure containing members for objects that
         *  are files. */
        struct
        {
            /** File handle. */
            RTFILE      hFile;
            /** File system object information of this file. */
            RTFSOBJINFO objInfo;
            /** Requested file open flags. */
            uint32_t    fOpenRequested;
            /** Bytes to proces for reading/writing. */
            uint64_t    cbToProcess;
            /** Bytes processed reading/writing. */
            uint64_t    cbProcessed;
        } File;
        struct
        {
            /** Directory handle. */
            RTDIR       hDir;
            /** File system object information of this directory. */
            RTFSOBJINFO objInfo;
            /** Requested directory creation flags. */
            uint32_t    fCreateRequested;
        } Dir;
    } u;
};

/** SharedClipboardURIList flags. */
typedef uint32_t SHAREDCLIPBOARDURILISTFLAGS;

/** No flags specified. */
#define SHAREDCLIPBOARDURILIST_FLAGS_NONE                   0
/** Keep the original paths, don't convert paths to relative ones. */
#define SHAREDCLIPBOARDURILIST_FLAGS_ABSOLUTE_PATHS         RT_BIT(0)
/** Resolve all symlinks. */
#define SHAREDCLIPBOARDURILIST_FLAGS_RESOLVE_SYMLINKS       RT_BIT(1)
/** Keep the files + directory entries open while
 *  being in this list. */
#define SHAREDCLIPBOARDURILIST_FLAGS_KEEP_OPEN              RT_BIT(2)
/** Lazy loading: Only enumerate sub directories when needed.
 ** @todo Implement lazy loading.  */
#define SHAREDCLIPBOARDURILIST_FLAGS_LAZY                   RT_BIT(3)

/** Mask of all valid Shared Clipboard URI list flags. */
#define SHAREDCLIPBOARDURILIST_FLAGS_VALID_MASK             UINT32_C(0xF)

class SharedClipboardURIList
{
public:

    SharedClipboardURIList(void);
    virtual ~SharedClipboardURIList(void);

public:

    int AppendNativePath(const char *pszPath, SHAREDCLIPBOARDURILISTFLAGS fFlags);
    int AppendNativePathsFromList(const char *pszNativePaths, size_t cbNativePaths, SHAREDCLIPBOARDURILISTFLAGS fFlags);
    int AppendNativePathsFromList(const RTCList<RTCString> &lstNativePaths, SHAREDCLIPBOARDURILISTFLAGS fFlags);
    int AppendURIObject(SharedClipboardURIObject *pObject);
    int AppendURIPath(const char *pszURI, SHAREDCLIPBOARDURILISTFLAGS fFlags);
    int AppendURIPathsFromList(const char *pszURIPaths, size_t cbURIPaths, SHAREDCLIPBOARDURILISTFLAGS fFlags);
    int AppendURIPathsFromList(const RTCList<RTCString> &lstURI, SHAREDCLIPBOARDURILISTFLAGS fFlags);

    void Clear(void);
    SharedClipboardURIObject *At(size_t i) const { return m_lstTree.at(i); }
    SharedClipboardURIObject *First(void) const { return m_lstTree.first(); }
    bool IsEmpty(void) const { return m_lstTree.isEmpty(); }
    void RemoveFirst(void);
    int SetFromURIData(const void *pvData, size_t cbData, SHAREDCLIPBOARDURILISTFLAGS fFlags);

    RTCString GetRootEntries(const RTCString &strPathBase = "", const RTCString &strSeparator = "\r\n") const;
    uint64_t GetRootCount(void) const { return m_lstRoot.size(); }
    uint64_t GetTotalCount(void) const { return m_cTotal; }
    uint64_t GetTotalBytes(void) const { return m_cbTotal; }

protected:

    int appendEntry(const char *pcszSource, const char *pcszTarget, SHAREDCLIPBOARDURILISTFLAGS fFlags);
    int appendObject(SharedClipboardURIObject *pObject);
    int appendPathRecursive(const char *pcszSrcPath, const char *pcszDstPath, const char *pcszDstBase, size_t cchDstBase, SHAREDCLIPBOARDURILISTFLAGS fFlags);

protected:

    /** List of all top-level file/directory entries.
     *  Note: All paths are kept internally as UNIX paths for
     *        easier conversion/handling!  */
    RTCList<RTCString>                  m_lstRoot;
    /** List of all URI objects added. The list's content
     *  might vary depending on how the objects are being
     *  added (lazy or not). */
    RTCList<SharedClipboardURIObject *> m_lstTree;
    /** Total number of all URI objects. */
    uint64_t                            m_cTotal;
    /** Total size of all URI objects, that is, the file
     *  size of all objects (in bytes).
     *  Note: Do *not* size_t here, as we also want to support large files
     *        on 32-bit guests. */
    uint64_t                            m_cbTotal;
};

/**
 * Structure for keeping Shared Clipboard meta data.
 *
 * For URI transfers this represents a string list with the file object root entries in it.
 */
typedef struct _SHAREDCLIPBOARDMETADATA
{
    /** Actual meta data block. */
    void  *pvMeta;
    /** Total size (in bytes) of the allocated meta data block .*/
    size_t cbMeta;
    /** How many bytes are being used in the meta data block. */
    size_t cbUsed;
} SHAREDCLIPBOARDMETADATA, *PSHAREDCLIPBOARDMETADATA;

int SharedClipboardMetaDataInit(PSHAREDCLIPBOARDMETADATA pMeta);
void SharedClipboardMetaDataDestroy(PSHAREDCLIPBOARDMETADATA pMeta);
int SharedClipboardMetaDataAdd(PSHAREDCLIPBOARDMETADATA pMeta, const void *pvDataAdd, uint32_t cbDataAdd);
int SharedClipboardMetaDataResize(PSHAREDCLIPBOARDMETADATA pMeta, size_t cbNewSize);
size_t SharedClipboardMetaDataGetUsed(PSHAREDCLIPBOARDMETADATA pMeta);
size_t SharedClipboardMetaDataGetSize(PSHAREDCLIPBOARDMETADATA pMeta);
void *SharedClipboardMetaDataMutableRaw(PSHAREDCLIPBOARDMETADATA pMeta);
const void *SharedClipboardMetaDataRaw(PSHAREDCLIPBOARDMETADATA pMeta);

/**
 * Enumeration to specify the Shared Clipboard provider source type.
 */
typedef enum SHAREDCLIPBOARDPROVIDERSOURCE
{
    /** Invalid source type. */
    SHAREDCLIPBOARDPROVIDERSOURCE_INVALID = 0,
    /** Source is VbglR3. */
    SHAREDCLIPBOARDPROVIDERSOURCE_VBGLR3,
    /** Source is the host service. */
    SHAREDCLIPBOARDPROVIDERSOURCE_HOSTSERVICE
} SHAREDCLIPBOARDPROVIDERSOURCE;

/**
 * Structure for the Shared Clipboard provider creation context.
 */
typedef struct _SHAREDCLIPBOARDPROVIDERCREATIONCTX
{
    SHAREDCLIPBOARDPROVIDERSOURCE enmSource;
    union
    {
        struct
        {
            /** HGCM client ID to use. */
            uint32_t uClientID;
        } VBGLR3;
    } u;
} SHAREDCLIPBOARDPROVIDERCREATIONCTX, *PSHAREDCLIPBOARDPROVIDERCREATIONCTX;

/**
 * Interface class acting as a lightweight proxy for abstracting reading / writing clipboard data.
 *
 * This is needed because various implementations can run on the host *or* on the guest side,
 * requiring different methods for handling the actual data.
 */
class SharedClipboardProvider
{

public:

    static SharedClipboardProvider *Create(PSHAREDCLIPBOARDPROVIDERCREATIONCTX pCtx);

    virtual ~SharedClipboardProvider(void);

public:

    uint32_t AddRef(void);
    uint32_t Release(void);

public: /* Interface to be implemented. */

    virtual int ReadMetaData(uint32_t fFlags = 0);
    virtual int WriteMetaData(const void *pvBuf, size_t cbBuf, size_t *pcbWritten, uint32_t fFlags = 0);

    virtual int ReadDirectory(PVBOXCLIPBOARDDIRDATA pDirData);
    virtual int ReadDirectoryObj(SharedClipboardURIObject &Obj);

    virtual int WriteDirectory(const PVBOXCLIPBOARDDIRDATA pDirData);
    virtual int WriteDirectoryObj(const SharedClipboardURIObject &Obj);

    virtual int ReadFileHdr(PVBOXCLIPBOARDFILEHDR pFileHdr);
    virtual int ReadFileHdrObj(SharedClipboardURIObject &Obj);

    virtual int WriteFileHdr(const PVBOXCLIPBOARDFILEHDR pFileHdr);
    virtual int WriteFileHdrObj(const SharedClipboardURIObject &Obj);

    virtual int ReadFileData(PVBOXCLIPBOARDFILEDATA pFileData, uint32_t *pcbRead);
    virtual int ReadFileDataObj(SharedClipboardURIObject &Obj, uint32_t *pcbRead);

    virtual int WriteFileData(const PVBOXCLIPBOARDFILEDATA pFileData, uint32_t *pcbWritten);
    virtual int WriteFileDataObj(const SharedClipboardURIObject &Obj, uint32_t *pcbWritten);

    virtual void Reset(void);

public:

          SharedClipboardURIList   &GetURIList(void) { return m_URIList; }
    const SharedClipboardURIObject *GetURIObjectCurrent(void) { return m_URIList.First(); }

protected:

    SharedClipboardProvider(void);

protected:

    /** Number of references to this instance. */
    volatile uint32_t      m_cRefs;
    /** Current URI list. */
    SharedClipboardURIList m_URIList;
};

/**
 * Shared Clipboard provider implementation for VbglR3 (guest side).
 */
class SharedClipboardProviderVbglR3 : protected SharedClipboardProvider
{
    friend class SharedClipboardProvider;

public:

    virtual ~SharedClipboardProviderVbglR3(void);

public:

    int ReadMetaData(uint32_t fFlags = 0);
    int WriteMetaData(const void *pvBuf, size_t cbBuf, size_t *pcbWritten, uint32_t fFlags = 0);

    int ReadDirectory(PVBOXCLIPBOARDDIRDATA pDirData);
    int WriteDirectory(const PVBOXCLIPBOARDDIRDATA pDirData);

    int ReadFileHdr(PVBOXCLIPBOARDFILEHDR pFileHdr);
    int WriteFileHdr(const PVBOXCLIPBOARDFILEHDR pFileHdr);
    int ReadFileData(PVBOXCLIPBOARDFILEDATA pFileData, uint32_t *pcbRead);
    int WriteFileData(const PVBOXCLIPBOARDFILEDATA pFileData, uint32_t *pcbWritten);

    void Reset(void);

protected:

    SharedClipboardProviderVbglR3(uint32_t uClientID);

    /** HGCM client ID to use. */
    uint32_t m_uClientID;
};

/**
 * Shared Clipboard provider implementation for host service (host side).
 */
class SharedClipboardProviderHostService : protected SharedClipboardProvider
{
    friend class SharedClipboardProvider;

public:

    virtual ~SharedClipboardProviderHostService(void);

protected:

    SharedClipboardProviderHostService(void);
};

typedef DECLCALLBACK(int) FNSHAREDCLIPBOARDURITRANSFERSTARTED(void *pvUser);
/** Pointer to a FNSHAREDCLIPBOARDURITRANSFERSTARTED function. */
typedef FNSHAREDCLIPBOARDURITRANSFERSTARTED *PFNSHAREDCLIPBOARDURITRANSFERSTARTED;

typedef DECLCALLBACK(int) FNSHAREDCLIPBOARDURITRANSFERCANCELED(void *pvUser);
/** Pointer to a FNSHAREDCLIPBOARDURITRANSFERCANCELED function. */
typedef FNSHAREDCLIPBOARDURITRANSFERCANCELED *PFNSHAREDCLIPBOARDURITRANSFERCANCELED;

typedef DECLCALLBACK(int) FNSHAREDCLIPBOARDURITRANSFERCOMPLETE(void *pvUser, int rc);
/** Pointer to a FNSHAREDCLIPBOARDURITRANSFERCOMPLETE function. */
typedef FNSHAREDCLIPBOARDURITRANSFERCOMPLETE *PFNSHAREDCLIPBOARDURITRANSFERCOMPLETE;

typedef DECLCALLBACK(int) FNSHAREDCLIPBOARDURITRANSFERERROR(void *pvUser, int rc);
/** Pointer to a FNSHAREDCLIPBOARDURITRANSFERERROR function. */
typedef FNSHAREDCLIPBOARDURITRANSFERERROR *PFNSHAREDCLIPBOARDURITRANSFERERROR;

/**
 * Structure acting as a function callback table for URI transfers.
 * All callbacks are optional and therefore can be NULL.
 */
typedef struct _SHAREDCLIPBOARDURITRANSFERCALLBACKS
{
    /** Function pointer, called when the transfer has been started. */
    PFNSHAREDCLIPBOARDURITRANSFERSTARTED  pfnTransferStarted;
    /** Function pointer, called when the transfer is complete. */
    PFNSHAREDCLIPBOARDURITRANSFERCOMPLETE pfnTransferComplete;
    /** Function pointer, called when the transfer has been canceled. */
    PFNSHAREDCLIPBOARDURITRANSFERCANCELED pfnTransferCanceled;
    /** Function pointer, called when transfer resulted in an unrecoverable error. */
    PFNSHAREDCLIPBOARDURITRANSFERERROR    pfnTransferError;
} SHAREDCLIPBOARDURITRANSFERCALLBACKS, *PSHAREDCLIPBOARDURITRANSFERCALLBACKS;

/**
 * Structure for thread-related members for a single URI transfer.
 */
typedef struct _SHAREDCLIPBOARDURITRANSFERTHREAD
{
    /** Thread handle for the reading / writing thread.
     *  Can be NIL_RTTHREAD if not being used. */
    RTTHREAD                    hThread;
    /** Thread started indicator. */
    volatile bool               fStarted;
    /** Thread cancelled flag / indicator. */
    volatile bool               fCancelled;
} SHAREDCLIPBOARDURITRANSFERTHREAD, *PSHAREDCLIPBOARDURITRANSFERTHREAD;

/**
 * Structure for maintaining a single URI transfer.
 */
typedef struct _SHAREDCLIPBOARDURITRANSFER
{
    /** The node member for using this struct in a RTList. */
    RTLISTNODE                          Node;
    /** The Shared Clipboard provider in charge for this transfer. */
    SharedClipboardProvider            *pProvider;
    /** Opaque pointer to implementation-specific parameters. */
    void                               *pvUser;
    /** Size (in bytes) of implementation-specific parameters. */
    size_t                              cbUser;
    /** Contains thread-related attributes. */
    SHAREDCLIPBOARDURITRANSFERTHREAD    Thread;
} SHAREDCLIPBOARDURITRANSFER, *PSHAREDCLIPBOARDURITRANSFER;

/**
 * Structure for keeping URI clipboard information around.
 */
typedef struct _SHAREDCLIPBOARDURICTX
{
    /** Critical section for serializing access. */
    RTCRITSECT                  CritSect;
    /** List of active transfers.
     *  Use a list or something lateron. */
    RTLISTANCHOR                List;
    /** Number of concurrent transfers.
     *  At the moment we only support only one transfer at a time. */
    uint32_t                    cTransfers;
} SHAREDCLIPBOARDURICTX, *PSHAREDCLIPBOARDURICTX;

int SharedClipboardURITransferCreate(PSHAREDCLIPBOARDPROVIDERCREATIONCTX pCtx,
                                     PSHAREDCLIPBOARDURITRANSFER *ppTransfer);
void SharedClipboardURITransferDestroy(PSHAREDCLIPBOARDURITRANSFER pTransfer);
void SharedClipboardURITransferReset(PSHAREDCLIPBOARDURITRANSFER pTransfer);
int SharedClipboardURITransferWrite(PSHAREDCLIPBOARDURITRANSFER pTransfer);
int SharedClipboardURITransferWriteThread(RTTHREAD hThread, void *pvUser);

int SharedClipboardURICtxInit(PSHAREDCLIPBOARDURICTX pURI, PSHAREDCLIPBOARDURITRANSFER pTransfer);
void SharedClipboardURICtxDestroy(PSHAREDCLIPBOARDURICTX pURI);
void SharedClipboardURICtxReset(PSHAREDCLIPBOARDURICTX pURI);
PSHAREDCLIPBOARDURITRANSFER SharedClipboardURICtxGetTransfer(PSHAREDCLIPBOARDURICTX pURI, uint32_t uIdx);
uint32_t SharedClipboardURICtxGetActiveTransfers(PSHAREDCLIPBOARDURICTX pURI);
int SharedClipboardURICtxTransferAdd(PSHAREDCLIPBOARDURICTX pURI, PSHAREDCLIPBOARDURITRANSFER pTransfer);

bool SharedClipboardMIMEHasFileURLs(const char *pcszFormat, size_t cchFormatMax);
bool SharedClipboardMIMENeedsCache(const char *pcszFormat, size_t cchFormatMax);

#endif /* !VBOX_INCLUDED_GuestHost_SharedClipboard_uri_h */

