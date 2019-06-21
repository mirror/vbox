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

#include <map>

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

/** No clipboard area open flags specified. */
#define SHAREDCLIPBOARDAREA_OPEN_FLAGS_NONE               0
/** The clipboard area must not exist yet. */
#define SHAREDCLIPBOARDAREA_OPEN_FLAGS_MUST_NOT_EXIST     RT_BIT(0)
/** Mask of all valid clipboard area open flags.  */
#define SHAREDCLIPBOARDAREA_OPEN_FLAGS_VALID_MASK         0x1

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

int SharedClipboardURIDataHdrAlloc(PVBOXCLIPBOARDDATAHDR *ppDataHdr);
void SharedClipboardURIDataHdrFree(PVBOXCLIPBOARDDATAHDR pDataHdr);
PVBOXCLIPBOARDDATAHDR SharedClipboardURIDataHdrDup(PVBOXCLIPBOARDDATAHDR pDataHdr);
uint32_t SharedClipboardURIDataHdrGetMetaDataSize(PVBOXCLIPBOARDDATAHDR pDataHdr);
int SharedClipboardURIDataHdrInit(PVBOXCLIPBOARDDATAHDR pDataHdr);
void SharedClipboardURIDataHdrDestroy(PVBOXCLIPBOARDDATAHDR pDataHdr);
void SharedClipboardURIDataHdrFree(PVBOXCLIPBOARDDATAHDR pDataHdr);
void SharedClipboardURIDataHdrReset(PVBOXCLIPBOARDDATAHDR pDataHdr);
bool SharedClipboardURIDataHdrIsValid(PVBOXCLIPBOARDDATAHDR pDataHdr);

int SharedClipboardURIDataChunkAlloc(PVBOXCLIPBOARDDATACHUNK *ppDataChunk);
void SharedClipboardURIDataChunkFree(PVBOXCLIPBOARDDATACHUNK pDataChunk);
PVBOXCLIPBOARDDATACHUNK SharedClipboardURIDataChunkDup(PVBOXCLIPBOARDDATACHUNK pDataChunk);
int SharedClipboardURIDataChunkInit(PVBOXCLIPBOARDDATACHUNK pDataChunk);
void SharedClipboardURIDataChunkDestroy(PVBOXCLIPBOARDDATACHUNK pDataChunk);
bool SharedClipboardURIDataChunkIsValid(PVBOXCLIPBOARDDATACHUNK pDataChunk);

int SharedClipboardURIDirDataAlloc(PVBOXCLIPBOARDDIRDATA *ppDirData);
void SharedClipboardURIDirDataFree(PVBOXCLIPBOARDDIRDATA pDirData);
int SharedClipboardURIDirDataInit(PVBOXCLIPBOARDDIRDATA pDirData);
void SharedClipboardURIDirDataDestroy(PVBOXCLIPBOARDDIRDATA pDirData);
PVBOXCLIPBOARDDIRDATA SharedClipboardURIDirDataDup(PVBOXCLIPBOARDDIRDATA pDirData);
bool SharedClipboardURIDirDataIsValid(PVBOXCLIPBOARDDIRDATA pDirData);

int SharedClipboardURIFileHdrInit(PVBOXCLIPBOARDFILEHDR pFileHdr);
void SharedClipboardURIFileHdrDestroy(PVBOXCLIPBOARDFILEHDR pFileHdr);
PVBOXCLIPBOARDFILEHDR SharedClipboardURIFileHdrDup(PVBOXCLIPBOARDFILEHDR pFileHdr);
bool SharedClipboardURIFileHdrIsValid(PVBOXCLIPBOARDFILEHDR pFileHdr, PVBOXCLIPBOARDDATAHDR pDataHdr);

void SharedClipboardURIFileDataDestroy(PVBOXCLIPBOARDFILEDATA pFileData);
PVBOXCLIPBOARDFILEDATA SharedClipboardURIFileDataDup(PVBOXCLIPBOARDFILEDATA pFileData);
bool SharedClipboardURIFileDataIsValid(PVBOXCLIPBOARDFILEDATA pFileData, PVBOXCLIPBOARDDATAHDR pDataHdr);

/** Specifies a meta data format. */
typedef uint32_t SHAREDCLIPBOARDMETADATAFMT;

/**
 * Enumeration of meta data formats.
 */
typedef enum _SHAREDCLIPBOARDMETADATAFMTENUM
{
    /** Unknown meta data format; do not use. */
    SHAREDCLIPBOARDMETADATAFMT_UNKNOWN  = 0,
    /** Meta data is an URI list.
     *  UTF-8 format with Unix path separators. Each URI entry is separated by "\r\n". */
    SHAREDCLIPBOARDMETADATAFMT_URI_LIST = 1
} SHAREDCLIPBOARDMETADATAFMTENUM;

/**
 * Structure for maintaining meta data format data.
 */
typedef struct _SHAREDCLIPBOARDMETADATAFMTDATA
{
    /** Meta data format version. */
    uint32_t uVer;
    /** Actual meta data format data. */
    void    *pvFmt;
    /** Size of meta data format data (in bytes). */
    uint32_t cbFmt;
} SHAREDCLIPBOARDMETADATAFMTDATA, *PSHAREDCLIPBOARDMETADATAFMTDATA;

/**
 * Structure for keeping Shared Clipboard meta data.
 * The actual format of the meta data depends on the format set in enmFmt.
 */
typedef struct _SHAREDCLIPBOARDMETADATA
{
    /** Format of the data. */
    SHAREDCLIPBOARDMETADATAFMT  enmFmt;
    /** Actual meta data block. */
    void                       *pvMeta;
    /** Total size (in bytes) of the allocated meta data block .*/
    uint32_t                    cbMeta;
    /** How many bytes are being used in the meta data block. */
    uint32_t                    cbUsed;
} SHAREDCLIPBOARDMETADATA, *PSHAREDCLIPBOARDMETADATA;

int SharedClipboardMetaDataInit(PSHAREDCLIPBOARDMETADATA pMeta, SHAREDCLIPBOARDMETADATAFMT enmFmt);
void SharedClipboardMetaDataDestroy(PSHAREDCLIPBOARDMETADATA pMeta);
int SharedClipboardMetaDataAdd(PSHAREDCLIPBOARDMETADATA pMeta, const void *pvDataAdd, uint32_t cbDataAdd);
int SharedClipboardMetaDataConvertToFormat(const void *pvData, size_t cbData, SHAREDCLIPBOARDMETADATAFMT enmFmt,
                                           void **ppvData, uint32_t *pcbData);
int SharedClipboardMetaDataResize(PSHAREDCLIPBOARDMETADATA pMeta, uint32_t cbNewSize);
size_t SharedClipboardMetaDataGetFree(PSHAREDCLIPBOARDMETADATA pMeta);
size_t SharedClipboardMetaDataGetUsed(PSHAREDCLIPBOARDMETADATA pMeta);
size_t SharedClipboardMetaDataGetSize(PSHAREDCLIPBOARDMETADATA pMeta);
void *SharedClipboardMetaDataMutableRaw(PSHAREDCLIPBOARDMETADATA pMeta);
const void *SharedClipboardMetaDataRaw(PSHAREDCLIPBOARDMETADATA pMeta);

/**
 * Enumeration specifying an URI transfer direction.
 */
typedef enum _SHAREDCLIPBOARDURITRANSFERDIR
{
    /** Unknown transfer directory. */
    SHAREDCLIPBOARDURITRANSFERDIR_UNKNOWN = 0,
    /** Read transfer (from source). */
    SHAREDCLIPBOARDURITRANSFERDIR_READ,
    /** Write transfer (to target). */
    SHAREDCLIPBOARDURITRANSFERDIR_WRITE,
    /** The usual 32-bit hack. */
    SHAREDCLIPBOARDURITRANSFERDIR__32BIT_HACK = 0x7fffffff
} SHAREDCLIPBOARDURITRANSFERDIR;

/**
 * Structure for maintaining an URI transfer state.
 * Everything in here will be part of a saved state (later).
 */
typedef struct _SHAREDCLIPBOARDURITRANSFERSTATE
{
    /** The transfer's direction. */
    SHAREDCLIPBOARDURITRANSFERDIR enmDir;
    /** The transfer's cached data header.
     *  Can be NULL if no header has been received yet. */
    PVBOXCLIPBOARDDATAHDR         pHeader;
    /** The transfer's cached meta data.
     *  Can be NULL if no meta data has been received yet. */
    PSHAREDCLIPBOARDMETADATA      pMeta;
} SHAREDCLIPBOARDURITRANSFERSTATE, *PSHAREDCLIPBOARDURITRANSFERSTATE;

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

class SharedClipboardProvider;

/**
 * Structure for storing clipboard provider callback data.
 */
typedef struct _SHAREDCLIPBOARDPROVIDERCALLBACKDATA
{
    /** Pointer to related clipboard provider ("this"). */
    SharedClipboardProvider    *pProvider;
    /** Saved user pointer. */
    void                       *pvUser;
} SHAREDCLIPBOARDPROVIDERCALLBACKDATA, *PSHAREDCLIPBOARDPROVIDERCALLBACKDATA;

/** Callback functopn for the Shared Clipboard provider. */
typedef DECLCALLBACK(int) FNSSHAREDCLIPBOARDPROVIDERCALLBACK(PSHAREDCLIPBOARDPROVIDERCALLBACKDATA pData);
/** Pointer to a FNSSHAREDCLIPBOARDPROVIDERCALLBACK function. */
typedef FNSSHAREDCLIPBOARDPROVIDERCALLBACK *PFNSSHAREDCLIPBOARDPROVIDERCALLBACK;

/**
 * Structure acting as a function callback table for clipboard providers.
 * All callbacks are optional and therefore can be NULL.
 */
typedef struct _SHAREDCLIPBOARDPROVIDERCALLBACKS
{
    /** Saved user pointer. Will be passed to the callback (if needed). */
    void                                    *pvUser;
    /** Function pointer, called when reading the (meta) data header. */
    PFNSSHAREDCLIPBOARDPROVIDERCALLBACK      pfnReadDataHdr;
    /** Function pointer, called when reading a (meta) data chunk. */
    PFNSSHAREDCLIPBOARDPROVIDERCALLBACK      pfnReadDataChunk;
} SHAREDCLIPBOARDPROVIDERCALLBACKS, *PSHAREDCLIPBOARDPROVIDERCALLBACKS;

/**
 * Structure for the Shared Clipboard provider creation context.
 */
typedef struct _SHAREDCLIPBOARDPROVIDERCREATIONCTX
{
    /** Specifies what the source of the provider is. */
    SHAREDCLIPBOARDPROVIDERSOURCE     enmSource;
    /** Optional callback table; can be NULL if not needed. */
    PSHAREDCLIPBOARDPROVIDERCALLBACKS pCallbacks;
    union
    {
        struct
        {
            /** HGCM client ID to use. */
            uint32_t uClientID;
        } VbglR3;
        struct
        {
            SharedClipboardArea *pArea;
        } HostService;
    } u;
} SHAREDCLIPBOARDPROVIDERCREATIONCTX, *PSHAREDCLIPBOARDPROVIDERCREATIONCTX;

/**
 * Structure for read parameters.
 */
typedef struct _SHAREDCLIPBOARDPROVIDERREADPARMS
{
    union
    {
        struct
        {
        } HostService;
    } u;
} SHAREDCLIPBOARDPROVIDERREADPARMS, *PSHAREDCLIPBOARDPROVIDERREADPARMS;

/**
 * Structure for write parameters.
 */
typedef struct _SHAREDCLIPBOARDPROVIDERWRITEPARMS
{
    union
    {
        struct
        {
            uint32_t         uMsg;
            uint32_t         cParms;
            VBOXHGCMSVCPARM *paParms;
        } HostService;
    } u;
} SHAREDCLIPBOARDPROVIDERWRITEPARMS, *PSHAREDCLIPBOARDPROVIDERWRITEPARMS;

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

public:

    void SetCallbacks(PSHAREDCLIPBOARDPROVIDERCALLBACKS pCallbacks);

public: /* Interface to be implemented. */

    virtual int ReadDataHdr(PVBOXCLIPBOARDDATAHDR *ppDataHdr);
    virtual int WriteDataHdr(const PVBOXCLIPBOARDDATAHDR pDataHdr);

    virtual int ReadDataChunk(const PVBOXCLIPBOARDDATAHDR pDataHdr, void *pvChunk, uint32_t cbChunk, uint32_t fFlags = 0,
                              uint32_t *pcbRead = NULL);
    virtual int WriteDataChunk(const PVBOXCLIPBOARDDATAHDR pDataHdr, const void *pvChunk, uint32_t cbChunk, uint32_t fFlags = 0,
                               uint32_t *pcbWritten = NULL);

    virtual int ReadDirectory(PVBOXCLIPBOARDDIRDATA *ppDirData);
    virtual int WriteDirectory(const PVBOXCLIPBOARDDIRDATA pDirData);

    virtual int ReadFileHdr(PVBOXCLIPBOARDFILEHDR *ppFileHdr);
    virtual int WriteFileHdr(const PVBOXCLIPBOARDFILEHDR pFileHdr);

    virtual int ReadFileData(void *pvData, uint32_t cbData, uint32_t fFlags = 0, uint32_t *pcbRead = NULL);
    virtual int WriteFileData(void *pvData, uint32_t cbData, uint32_t fFlags = 0, uint32_t *pcbWritten = NULL);

    virtual void Reset(void);

public: /* Optional callback handling. */

    /*virtual int SetCallbacks();*/

    virtual int OnRead(PSHAREDCLIPBOARDPROVIDERREADPARMS pParms);
    virtual int OnWrite(PSHAREDCLIPBOARDPROVIDERWRITEPARMS pParms);

protected:

    SharedClipboardProvider(void);

protected:

    /**
     * Structure for maintaining an internal event.
     */
    struct Event
    {
        Event(uint32_t uMsg);
        virtual ~Event();

        void *DataAdopt(void);
        uint32_t DataSize(void);
        void *DataRaw(void);
        void Reset(void);
        int SetData(const void *pvData, uint32_t cbData);
        int Wait(RTMSINTERVAL uTimeoutMs);

        /** The event's associated message ID (guest function number). */
        uint32_t    mMsg;
        /** The event's own event semaphore. */
        RTSEMEVENT  mEvent;
        /** User-provided data buffer associated to this event. Optional. */
        void       *mpvData;
        /** Size (in bytes) of user-provided data buffer associated to this event. */
        uint32_t    mcbData;
    };

    /** Map of events; the key is the guest function number (VBOX_SHARED_CLIPBOARD_GUEST_FN_XXX). */
    typedef std::map<uint32_t, Event *> EventMap;

    int eventRegister(uint32_t uMsg);
    int eventUnregister(uint32_t uMsg);
    int eventUnregisterAll(void);
    int eventSignal(uint32_t uMsg);
    int eventWait(uint32_t uMsg, PFNSSHAREDCLIPBOARDPROVIDERCALLBACK pfnCallback, RTMSINTERVAL uTimeoutMs,
                  void **ppvData, uint32_t *pcbData = NULL);
    SharedClipboardProvider::Event *eventGet(uint32_t uMsg);

protected:

    /** Number of references to this instance. */
    volatile uint32_t                m_cRefs;
    /** The provider's callback table. */
    SHAREDCLIPBOARDPROVIDERCALLBACKS m_Callbacks;
    /** Default timeout (in ms) for waiting for events. */
    RTMSINTERVAL                     m_uTimeoutMs;
    /** Map of (internal) events to provide asynchronous reads / writes. */
    EventMap                         m_mapEvents;
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

    int ReadDataHdr(PVBOXCLIPBOARDDATAHDR *ppDataHdr);
    int WriteDataHdr(const PVBOXCLIPBOARDDATAHDR pDataHdr);

    int ReadDataChunk(const PVBOXCLIPBOARDDATAHDR pDataHdr, void *pvChunk, uint32_t cbChunk, uint32_t fFlags = 0,
                      uint32_t *pcbRead = NULL);
    int WriteDataChunk(const PVBOXCLIPBOARDDATAHDR pDataHdr, const void *pvChunk, uint32_t cbChunk, uint32_t fFlags = 0,
                       uint32_t *pcbWritten = NULL);

    int ReadDirectory(PVBOXCLIPBOARDDIRDATA *ppDirData);
    int WriteDirectory(const PVBOXCLIPBOARDDIRDATA pDirData);

    int ReadFileHdr(PVBOXCLIPBOARDFILEHDR *ppFileHdr);
    int WriteFileHdr(const PVBOXCLIPBOARDFILEHDR pFileHdr);

    int ReadFileData(void *pvData, uint32_t cbData, uint32_t fFlags = 0, uint32_t *pcbRead = NULL);
    int WriteFileData(void *pvData, uint32_t cbData, uint32_t fFlags = 0, uint32_t *pcbWritten = NULL);

    void Reset(void);

protected:

    SharedClipboardProviderVbglR3(uint32_t uClientID);

    /** HGCM client ID to use. */
    uint32_t m_uClientID;
};

#ifdef VBOX_WITH_SHARED_CLIPBOARD_HOST
/**
 * Shared Clipboard provider implementation for host service (host side).
 */
class SharedClipboardProviderHostService : protected SharedClipboardProvider
{
    friend class SharedClipboardProvider;

public:

    virtual ~SharedClipboardProviderHostService();

public:

    int ReadDataHdr(PVBOXCLIPBOARDDATAHDR *ppDataHdr);
    int WriteDataHdr(const PVBOXCLIPBOARDDATAHDR pDataHdr);

    int ReadDataChunk(const PVBOXCLIPBOARDDATAHDR pDataHdr, void *pvChunk, uint32_t cbChunk, uint32_t fFlags = 0,
                      uint32_t *pcbRead = NULL);
    int WriteDataChunk(const PVBOXCLIPBOARDDATAHDR pDataHdr, const void *pvChunk, uint32_t cbChunk, uint32_t fFlags = 0,
                       uint32_t *pcbWritten = NULL);

    int ReadDirectory(PVBOXCLIPBOARDDIRDATA *ppDirData);
    int WriteDirectory(const PVBOXCLIPBOARDDIRDATA pDirData);

    int ReadFileHdr(PVBOXCLIPBOARDFILEHDR *ppFileHdr);
    int WriteFileHdr(const PVBOXCLIPBOARDFILEHDR pFileHdr);

    int ReadFileData(void *pvData, uint32_t cbData, uint32_t fFlags = 0, uint32_t *pcbRead = NULL);
    int WriteFileData(void *pvData, uint32_t cbData, uint32_t fFlags = 0, uint32_t *pcbWritten = NULL);

    void Reset(void);

public:

    int OnWrite(PSHAREDCLIPBOARDPROVIDERWRITEPARMS pParms);

protected:

    SharedClipboardProviderHostService(SharedClipboardArea *pArea);

protected:

    /** Pointer to associated clipboard area. */
    SharedClipboardArea            *m_pArea;
};
#endif /* VBOX_WITH_SHARED_CLIPBOARD_HOST */

struct _SHAREDCLIPBOARDURITRANSFER;
typedef _SHAREDCLIPBOARDURITRANSFER *PSHAREDCLIPBOARDURITRANSFER;

/**
 * Structure for storing URI transfer callback data.
 */
typedef struct _SHAREDCLIPBOARDURITRANSFERCALLBACKDATA
{
    /** Pointer to related URI transfer. */
    PSHAREDCLIPBOARDURITRANSFER pTransfer;
    /** Saved user pointer. */
    void                       *pvUser;
} SHAREDCLIPBOARDURITRANSFERCALLBACKDATA, *PSHAREDCLIPBOARDURITRANSFERCALLBACKDATA;

typedef DECLCALLBACK(void) FNSHAREDCLIPBOARDURITRANSFERSTARTED(PSHAREDCLIPBOARDURITRANSFERCALLBACKDATA pData);
/** Pointer to a FNSHAREDCLIPBOARDURITRANSFERSTARTED function. */
typedef FNSHAREDCLIPBOARDURITRANSFERSTARTED *PFNSHAREDCLIPBOARDURITRANSFERSTARTED;

typedef DECLCALLBACK(void) FNSHAREDCLIPBOARDURITRANSFERCANCELED(PSHAREDCLIPBOARDURITRANSFERCALLBACKDATA pData);
/** Pointer to a FNSHAREDCLIPBOARDURITRANSFERCANCELED function. */
typedef FNSHAREDCLIPBOARDURITRANSFERCANCELED *PFNSHAREDCLIPBOARDURITRANSFERCANCELED;

typedef DECLCALLBACK(void) FNSHAREDCLIPBOARDURITRANSFERCOMPLETE(PSHAREDCLIPBOARDURITRANSFERCALLBACKDATA pData, int rc);
/** Pointer to a FNSHAREDCLIPBOARDURITRANSFERCOMPLETE function. */
typedef FNSHAREDCLIPBOARDURITRANSFERCOMPLETE *PFNSHAREDCLIPBOARDURITRANSFERCOMPLETE;

typedef DECLCALLBACK(void) FNSHAREDCLIPBOARDURITRANSFERERROR(PSHAREDCLIPBOARDURITRANSFERCALLBACKDATA pData, int rc);
/** Pointer to a FNSHAREDCLIPBOARDURITRANSFERERROR function. */
typedef FNSHAREDCLIPBOARDURITRANSFERERROR *PFNSHAREDCLIPBOARDURITRANSFERERROR;

/**
 * Structure acting as a function callback table for URI transfers.
 * All callbacks are optional and therefore can be NULL.
 */
typedef struct _SHAREDCLIPBOARDURITRANSFERCALLBACKS
{
    /** Saved user pointer. */
    void                                 *pvUser;
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
 * Structure for handling a single URI object context.
 */
typedef struct _SHAREDCLIPBOARDCLIENTURIOBJCTX
{
    /** Pointer to current object being processed. */
    SharedClipboardURIObject      *pObj;
} SHAREDCLIPBOARDCLIENTURIOBJCTX, *PSHAREDCLIPBOARDCLIENTURIOBJCTX;

/**
 * Structure for maintaining a single URI transfer.
 *
 ** @todo Not yet thread safe.
 */
typedef struct _SHAREDCLIPBOARDURITRANSFER
{
    /** The node member for using this struct in a RTList. */
    RTLISTNODE                          Node;
    /** Critical section for serializing access. */
    RTCRITSECT                          CritSect;
    /** The transfer's state (for SSM, later). */
    SHAREDCLIPBOARDURITRANSFERSTATE     State;
    /** The transfer's own (local) area, if any (can be NULL if not needed).
     *  The area itself has a clipboard area ID assigned.
     *  On the host this area ID gets shared (maintained / locked) across all VMs via VBoxSVC. */
    SharedClipboardArea                *pArea;
    /** The URI list for this transfer. */
    SharedClipboardURIList             *pURIList;
    /** Context of current object being handled. */
    SHAREDCLIPBOARDCLIENTURIOBJCTX      ObjCtx;
    /** The Shared Clipboard provider in charge for this transfer. */
    SharedClipboardProvider            *pProvider;
    /** Opaque pointer to implementation-specific parameters. */
    void                               *pvUser;
    /** Size (in bytes) of implementation-specific parameters. */
    size_t                              cbUser;
    /** Contains thread-related attributes. */
    SHAREDCLIPBOARDURITRANSFERTHREAD    Thread;
    /** (Optional) callbacks related to this transfer. */
    SHAREDCLIPBOARDURITRANSFERCALLBACKS Callbacks;
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
     *  At the moment we only support only one transfer per client at a time. */
    uint32_t                    cTransfers;
    /** Maximum Number of concurrent transfers.
     *  At the moment we only support only one transfer per client at a time. */
    uint32_t                    cMaxTransfers;
} SHAREDCLIPBOARDURICTX, *PSHAREDCLIPBOARDURICTX;

int SharedClipboardURIObjCtxInit(PSHAREDCLIPBOARDCLIENTURIOBJCTX pObjCtx);
void SharedClipboardURIObjCtxDestroy(PSHAREDCLIPBOARDCLIENTURIOBJCTX pObjCtx);
SharedClipboardURIObject *SharedClipboardURIObjCtxGetObj(PSHAREDCLIPBOARDCLIENTURIOBJCTX pObjCtx);
bool SharedClipboardURIObjCtxIsValid(PSHAREDCLIPBOARDCLIENTURIOBJCTX pObjCtx);

int SharedClipboardURITransferCreate(SHAREDCLIPBOARDURITRANSFERDIR enmDir, PSHAREDCLIPBOARDURITRANSFER *ppTransfer);
int SharedClipboardURITransferDestroy(PSHAREDCLIPBOARDURITRANSFER pTransfer);
int SharedClipboardURITransferPrepare(PSHAREDCLIPBOARDURITRANSFER pTransfer);
int SharedClipboardURITransferProviderCreate(PSHAREDCLIPBOARDURITRANSFER pTransfer,
                                             PSHAREDCLIPBOARDPROVIDERCREATIONCTX pProviderCtx);
void SharedClipboardURITransferReset(PSHAREDCLIPBOARDURITRANSFER pTransfer);
SharedClipboardArea *SharedClipboardURITransferGetArea(PSHAREDCLIPBOARDURITRANSFER pTransfer);
PSHAREDCLIPBOARDCLIENTURIOBJCTX SharedClipboardURITransferGetCurrentObjCtx(PSHAREDCLIPBOARDURITRANSFER pTransfer);
const SharedClipboardURIObject *SharedClipboardURITransferGetCurrentObject(PSHAREDCLIPBOARDURITRANSFER pTransfer);
SharedClipboardProvider *SharedClipboardURITransferGetProvider(PSHAREDCLIPBOARDURITRANSFER pTransfer);
SharedClipboardURIList *SharedClipboardURITransferGetList(PSHAREDCLIPBOARDURITRANSFER pTransfer);
SharedClipboardURIObject *SharedClipboardURITransferGetObject(PSHAREDCLIPBOARDURITRANSFER pTransfer, uint64_t uIdx);
int SharedClipboardURITransferRun(PSHAREDCLIPBOARDURITRANSFER pTransfer, bool fAsync);
void SharedClipboardURITransferSetCallbacks(PSHAREDCLIPBOARDURITRANSFER pTransfer, PSHAREDCLIPBOARDURITRANSFERCALLBACKS pCallbacks);

int SharedClipboardURITransferMetaDataAdd(PSHAREDCLIPBOARDURITRANSFER pTransfer, const void *pvMeta, uint32_t cbMeta);
bool SharedClipboardURITransferMetaDataIsComplete(PSHAREDCLIPBOARDURITRANSFER pTransfer);
int SharedClipboardURITransferMetaGet(PSHAREDCLIPBOARDURITRANSFER pTransfer, const void *pvMeta, uint32_t cbMeta);
int SharedClipboardURITransferMetaDataRead(PSHAREDCLIPBOARDURITRANSFER pTransfer, uint32_t *pcbRead);
int SharedClipboardURITransferMetaDataWrite(PSHAREDCLIPBOARDURITRANSFER pTransfer, uint32_t *pcbWritten);

int SharedClipboardURITransferRead(PSHAREDCLIPBOARDURITRANSFER pTransfer);
int SharedClipboardURITransferReadObjects(PSHAREDCLIPBOARDURITRANSFER pTransfer);

int SharedClipboardURITransferWrite(PSHAREDCLIPBOARDURITRANSFER pTransfer);
int SharedClipboardURITransferWriteObjects(PSHAREDCLIPBOARDURITRANSFER pTransfer);

int SharedClipboardURICtxInit(PSHAREDCLIPBOARDURICTX pURI);
void SharedClipboardURICtxDestroy(PSHAREDCLIPBOARDURICTX pURI);
void SharedClipboardURICtxReset(PSHAREDCLIPBOARDURICTX pURI);
PSHAREDCLIPBOARDURITRANSFER SharedClipboardURICtxGetTransfer(PSHAREDCLIPBOARDURICTX pURI, uint32_t uIdx);
uint32_t SharedClipboardURICtxGetActiveTransfers(PSHAREDCLIPBOARDURICTX pURI);
bool SharedClipboardURICtxMaximumTransfersReached(PSHAREDCLIPBOARDURICTX pURI);
int SharedClipboardURICtxTransferAdd(PSHAREDCLIPBOARDURICTX pURI, PSHAREDCLIPBOARDURITRANSFER pTransfer);
int SharedClipboardURICtxTransferRemove(PSHAREDCLIPBOARDURICTX pURI, PSHAREDCLIPBOARDURITRANSFER pTransfer);

bool SharedClipboardMIMEHasFileURLs(const char *pcszFormat, size_t cchFormatMax);
bool SharedClipboardMIMENeedsCache(const char *pcszFormat, size_t cchFormatMax);

#endif /* !VBOX_INCLUDED_GuestHost_SharedClipboard_uri_h */

