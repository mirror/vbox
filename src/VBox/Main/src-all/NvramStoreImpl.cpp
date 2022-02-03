/* $Id$ */
/** @file
 * VirtualBox COM NVRAM store class implementation
 */

/*
 * Copyright (C) 2021-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#define LOG_GROUP LOG_GROUP_MAIN_NVRAMSTORE
#include "LoggingNew.h"

#include "NvramStoreImpl.h"
#ifdef VBOX_COM_INPROC
# include "ConsoleImpl.h"
#else
# include "MachineImpl.h"
# include "GuestOSTypeImpl.h"
# include "AutoStateDep.h"
#endif
#include "UefiVariableStoreImpl.h"

#include "AutoCaller.h"

#include <VBox/com/array.h>
#include <VBox/vmm/pdmdrv.h>
#include <VBox/err.h>

#include <iprt/cpp/utils.h>
#include <iprt/efi.h>
#include <iprt/file.h>
#include <iprt/vfs.h>
#include <iprt/zip.h>


// defines
////////////////////////////////////////////////////////////////////////////////

// globals
////////////////////////////////////////////////////////////////////////////////

/**
 * NVRAM store driver instance data.
 */
typedef struct DRVMAINNVRAMSTORE
{
    /** Pointer to the keyboard object. */
    NvramStore                  *pNvramStore;
    /** Pointer to the driver instance structure. */
    PPDMDRVINS                  pDrvIns;
    /** Our VFS connector interface. */
    PDMIVFSCONNECTOR            IVfs;
} DRVMAINNVRAMSTORE, *PDRVMAINNVRAMSTORE;

/** The NVRAM store map keyed by namespace/entity. */
typedef std::map<Utf8Str, RTVFSFILE> NvramStoreMap;
/** The NVRAM store map iterator. */
typedef std::map<Utf8Str, RTVFSFILE>::iterator NvramStoreIter;

struct BackupableNvramStoreData
{
    BackupableNvramStoreData()
    { }

    /** The NVRAM file path. */
    com::Utf8Str            strNvramPath;
    /** The NVRAM store. */
    NvramStoreMap           mapNvram;
};

/////////////////////////////////////////////////////////////////////////////
// NvramStore::Data structure
/////////////////////////////////////////////////////////////////////////////

struct NvramStore::Data
{
    Data()
        : pParent(NULL)
#ifdef VBOX_COM_INPROC
          , cRefs(0)
#endif
    { }

#ifdef VBOX_COM_INPROC
    /** The Console owning this NVRAM store. */
    Console * const         pParent;
    /** Number of references held to this NVRAM store from the various devices/drivers. */
    volatile uint32_t       cRefs;
#else
    /** The Machine object owning this NVRAM store. */
    Machine * const                    pParent;
    /** The peer NVRAM store object. */
    ComObjPtr<NvramStore>              pPeer;
    /** The UEFI variable store. */
    const ComObjPtr<UefiVariableStore> pUefiVarStore;
#endif

    Backupable<BackupableNvramStoreData> bd;
};

// constructor / destructor
////////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR(NvramStore)

HRESULT NvramStore::FinalConstruct()
{
    return BaseFinalConstruct();
}

void NvramStore::FinalRelease()
{
    uninit();
    BaseFinalRelease();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

#if !defined(VBOX_COM_INPROC)
/**
 * Initializes the NVRAM store object.
 *
 * @returns COM result indicator
 */
HRESULT NvramStore::init(Machine *aParent)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("aParent: %p\n", aParent));

    ComAssertRet(aParent, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m = new Data();

    /* share the parent weakly */
    unconst(m->pParent) = aParent;

    m->bd.allocate();

    autoInitSpan.setSucceeded();

    LogFlowThisFuncLeave();
    return S_OK;
}

/**
 *  Initializes the NVRAM store object given another NVRAM store object
 *  (a kind of copy constructor). This object shares data with
 *  the object passed as an argument.
 *
 *  @note This object must be destroyed before the original object
 *  it shares data with is destroyed.
 */
HRESULT NvramStore::init(Machine *aParent, NvramStore *that)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("aParent: %p, that: %p\n", aParent, that));

    ComAssertRet(aParent && that, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m = new Data();

    unconst(m->pParent) = aParent;
    m->pPeer = that;

    AutoWriteLock thatlock(that COMMA_LOCKVAL_SRC_POS);
    m->bd.share(that->m->bd);

    autoInitSpan.setSucceeded();

    LogFlowThisFuncLeave();
    return S_OK;
}

/**
 *  Initializes the guest object given another guest object
 *  (a kind of copy constructor). This object makes a private copy of data
 *  of the original object passed as an argument.
 */
HRESULT NvramStore::initCopy(Machine *aParent, NvramStore *that)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("aParent: %p, that: %p\n", aParent, that));

    ComAssertRet(aParent && that, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m = new Data();

    unconst(m->pParent) = aParent;
    // mPeer is left null

    AutoWriteLock thatlock(that COMMA_LOCKVAL_SRC_POS);
    m->bd.attachCopy(that->m->bd);

    autoInitSpan.setSucceeded();

    LogFlowThisFuncLeave();
    return S_OK;
}

#else

/**
 * Initializes the NVRAM store object.
 *
 * @returns COM result indicator
 * @param aParent                       Handle of our parent object
 * @param strNonVolatileStorageFile     The NVRAM file path.
 */
HRESULT NvramStore::init(Console *aParent, const com::Utf8Str &strNonVolatileStorageFile)
{
    LogFlowThisFunc(("aParent=%p\n", aParent));

    ComAssertRet(aParent, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m = new Data();
    unconst(m->pParent) = aParent;

    m->bd.allocate();
    m->bd->strNvramPath = strNonVolatileStorageFile;

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}
#endif /* VBOX_COM_INPROC */


/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void NvramStore::uninit()
{
    LogFlowThisFuncEnter();

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    unconst(m->pParent) = NULL;
#ifndef VBOX_COM_INPROC
    unconst(m->pUefiVarStore) = NULL;
#endif

    /* Delete the NVRAM content. */
    NvramStoreIter it = m->bd->mapNvram.begin();
    while (it != m->bd->mapNvram.end())
    {
        RTVfsFileRelease(it->second);
        it++;
    }

    m->bd->mapNvram.clear();

    delete m;
    m = NULL;

    LogFlowThisFuncLeave();
}


HRESULT NvramStore::getNonVolatileStorageFile(com::Utf8Str &aNonVolatileStorageFile)
{
#ifndef VBOX_COM_INPROC
    Utf8Str strTmp;
    {
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
        strTmp = m->bd->strNvramPath;
    }

    AutoReadLock mlock(m->pParent COMMA_LOCKVAL_SRC_POS);
    if (strTmp.isEmpty())
        strTmp = m->pParent->i_getDefaultNVRAMFilename();
    if (strTmp.isNotEmpty())
        m->pParent->i_calculateFullPath(strTmp, aNonVolatileStorageFile);
#else
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    aNonVolatileStorageFile = m->bd->strNvramPath;
#endif

    return S_OK;
}


HRESULT NvramStore::getUefiVariableStore(ComPtr<IUefiVariableStore> &aUefiVarStore)
{
#ifndef VBOX_COM_INPROC
    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(m->pParent);
    if (FAILED(adep.rc())) return adep.rc();

    Utf8Str strPath;
    NvramStore::getNonVolatileStorageFile(strPath);

    /* We need a write lock because of the lazy initialization. */
    AutoWriteLock wlock(this COMMA_LOCKVAL_SRC_POS);

    /* Check if we have to create the UEFI variable store object */
    HRESULT hrc = S_OK;
    if (!m->pUefiVarStore)
    {
        /* Load the NVRAM file first if it isn't already. */
        if (!m->bd->mapNvram.size())
        {
            int vrc = i_loadStore(strPath.c_str());
            if (RT_FAILURE(vrc))
                hrc = setError(E_FAIL, tr("Loading the NVRAM store failed (%Rrc)\n"), vrc);
        }

        if (SUCCEEDED(hrc))
        {
            NvramStoreIter it = m->bd->mapNvram.find("efi/nvram");
            if (it != m->bd->mapNvram.end())
            {
                unconst(m->pUefiVarStore).createObject();
                m->pUefiVarStore->init(this, m->pParent);
            }
            else
                hrc = setError(VBOX_E_OBJECT_NOT_FOUND, tr("The UEFI NVRAM file is not existing for this machine."));
        }
    }

    if (SUCCEEDED(hrc))
    {
        m->pUefiVarStore.queryInterfaceTo(aUefiVarStore.asOutParam());

        /* Mark the NVRAM store as potentially modified. */
        m->pParent->i_setModified(Machine::IsModified_NvramStore);
    }

    return hrc;
#else
    NOREF(aUefiVarStore);
    return E_NOTIMPL;
#endif
}


HRESULT NvramStore::initUefiVariableStore(ULONG aSize)
{
#ifndef VBOX_COM_INPROC
    if (aSize != 0)
        return setError(E_NOTIMPL, tr("Supporting another NVRAM size apart from the default one is not supported right now"));

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(m->pParent);
    if (FAILED(adep.rc())) return adep.rc();

    Utf8Str strPath;
    NvramStore::getNonVolatileStorageFile(strPath);

    /* We need a write lock because of the lazy initialization. */
    AutoReadLock mlock(m->pParent COMMA_LOCKVAL_SRC_POS);
    AutoWriteLock wlock(this COMMA_LOCKVAL_SRC_POS);

    if (m->pParent->i_getFirmwareType() == FirmwareType_BIOS)
        return setError(VBOX_E_NOT_SUPPORTED, tr("The selected firmware type doesn't support a UEFI variable store"));

    /* Load the NVRAM file first if it isn't already. */
    HRESULT hrc = S_OK;
    if (!m->bd->mapNvram.size())
    {
        int vrc = i_loadStore(strPath.c_str());
        if (RT_FAILURE(vrc))
            hrc = setError(E_FAIL, tr("Loading the NVRAM store failed (%Rrc)\n"), vrc);
    }

    if (SUCCEEDED(hrc))
    {
        int vrc = VINF_SUCCESS;
        RTVFSFILE hVfsUefiVarStore = NIL_RTVFSFILE;
        NvramStoreIter it = m->bd->mapNvram.find("efi/nvram");
        if (it != m->bd->mapNvram.end())
            hVfsUefiVarStore = it->second;
        else
        {
            /* Create a new file. */
            vrc = RTVfsMemFileCreate(NIL_RTVFSIOSTREAM, 0 /*cbEstimate*/, &hVfsUefiVarStore);
            if (RT_SUCCESS(vrc))
            {
                /** @todo The size is hardcoded to match what the firmware image uses right now which is a gross hack... */
                vrc = RTVfsFileSetSize(hVfsUefiVarStore, 540672, RTVFSFILE_SIZE_F_NORMAL);
                if (RT_SUCCESS(vrc))
                    m->bd->mapNvram["efi/nvram"] = hVfsUefiVarStore;
                else
                    RTVfsFileRelease(hVfsUefiVarStore);
            }
        }

        if (RT_SUCCESS(vrc))
        {
            vrc = RTEfiVarStoreCreate(hVfsUefiVarStore, 0 /*offStore*/, 0 /*cbStore*/, RTEFIVARSTORE_CREATE_F_DEFAULT, 0 /*cbBlock*/,
                                      NULL /*pErrInfo*/);
            if (RT_FAILURE(vrc))
                return setError(E_FAIL, tr("Failed to initialize the UEFI variable store (%Rrc)"), vrc);
        }
        else
            return setError(E_FAIL, tr("Failed to initialize the UEFI variable store (%Rrc)"), vrc);

        m->pParent->i_setModified(Machine::IsModified_NvramStore);
    }

    return hrc;
#else
    NOREF(aSize);
    return E_NOTIMPL;
#endif
}


Utf8Str NvramStore::i_getNonVolatileStorageFile()
{
    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), Utf8Str::Empty);

    Utf8Str strTmp;
    NvramStore::getNonVolatileStorageFile(strTmp);
    return strTmp;
}


/**
 * Loads the NVRAM store from the given TAR filesystem stream.
 *
 * @returns IPRT status code.
 * @param   hVfsFssTar          Handle to the tar filesystem stream.
 */
int NvramStore::i_loadStoreFromTar(RTVFSFSSTREAM hVfsFssTar)
{
    int rc = VINF_SUCCESS;

    /*
     * Process the stream.
     */
    for (;;)
    {
        /*
         * Retrieve the next object.
         */
        char       *pszName;
        RTVFSOBJ    hVfsObj;
        rc = RTVfsFsStrmNext(hVfsFssTar, &pszName, NULL, &hVfsObj);
        if (RT_FAILURE(rc))
        {
            if (rc == VERR_EOF)
                rc = VINF_SUCCESS;
            break;
        }

        RTFSOBJINFO UnixInfo;
        rc = RTVfsObjQueryInfo(hVfsObj, &UnixInfo, RTFSOBJATTRADD_UNIX);
        if (RT_SUCCESS(rc))
        {
            switch (UnixInfo.Attr.fMode & RTFS_TYPE_MASK)
            {
                case RTFS_TYPE_FILE:
                {
                    LogRel(("NvramStore: Loading '%s' from archive\n", pszName));
                    RTVFSIOSTREAM hVfsIosEntry = RTVfsObjToIoStream(hVfsObj);
                    Assert(hVfsIosEntry != NIL_RTVFSIOSTREAM);

                    RTVFSFILE hVfsFileEntry;
                    rc = RTVfsMemorizeIoStreamAsFile(hVfsIosEntry, RTFILE_O_READ | RTFILE_O_WRITE, &hVfsFileEntry);
                    if (RT_FAILURE(rc))
                        break;
                    RTVfsIoStrmRelease(hVfsIosEntry);

                    m->bd->mapNvram[Utf8Str(pszName)] = hVfsFileEntry;
                    break;
                }
                case RTFS_TYPE_DIRECTORY:
                    break;
                default:
                    rc = VERR_NOT_SUPPORTED;
                    break;
            }
        }

        /*
         * Release the current object and string.
         */
        RTVfsObjRelease(hVfsObj);
        RTStrFree(pszName);

        if (RT_FAILURE(rc))
            break;
    }

    return rc;
}


/**
 * Loads the NVRAM store.
 *
 * @returns IPRT status code.
 */
int NvramStore::i_loadStore(const char *pszPath)
{
    uint64_t cbStore = 0;
    int rc = RTFileQuerySizeByPath(pszPath, &cbStore);
    if (RT_SUCCESS(rc))
    {
        if (cbStore <= _1M) /* Arbitrary limit to fend off bogus files because the file will be read into memory completely. */
        {
            /*
             * Old NVRAM files just consist of the EFI variable store whereas starting
             * with VirtualBox 7.0 and the introduction of the TPM the need to handle multiple
             * independent NVRAM files came up. For those scenarios all NVRAM states are collected
             * in a tar archive.
             *
             * Here we detect whether the file is the new tar archive format or whether it is just
             * the plain EFI variable store file.
             */
            RTVFSIOSTREAM hVfsIosNvram;
            rc = RTVfsIoStrmOpenNormal(pszPath, RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_NONE,
                                       &hVfsIosNvram);
            if (RT_SUCCESS(rc))
            {
                /* Read the content. */
                RTVFSFILE hVfsFileNvram;
                rc = RTVfsMemorizeIoStreamAsFile(hVfsIosNvram, RTFILE_O_READ, &hVfsFileNvram);
                if (RT_SUCCESS(rc))
                {
                    /* Try to parse it as an EFI variable store. */
                    RTVFS hVfsEfiVarStore;
                    rc = RTEfiVarStoreOpenAsVfs(hVfsFileNvram, RTVFSMNT_F_READ_ONLY, 0 /*fVarStoreFlags*/, &hVfsEfiVarStore,
                                                NULL /*pErrInfo*/);
                    if (RT_SUCCESS(rc))
                    {
                        rc = RTVfsFileSeek(hVfsFileNvram, 0 /*offSeek*/, RTFILE_SEEK_BEGIN, NULL /*poffActual*/);
                        AssertRC(rc);

                        RTVfsFileRetain(hVfsFileNvram); /* Retain a new reference for the map. */
                        m->bd->mapNvram[Utf8Str("efi/nvram")] = hVfsFileNvram;

                        RTVfsRelease(hVfsEfiVarStore);
                    }
                    else if (rc == VERR_VFS_UNKNOWN_FORMAT)
                    {
                        /* Check for the new style tar archive. */
                        rc = RTVfsFileSeek(hVfsFileNvram, 0 /*offSeek*/, RTFILE_SEEK_BEGIN, NULL /*poffActual*/);
                        AssertRC(rc);

                        RTVFSIOSTREAM hVfsIosTar = RTVfsFileToIoStream(hVfsFileNvram);
                        Assert(hVfsIosTar != NIL_RTVFSIOSTREAM);

                        RTVFSFSSTREAM hVfsFssTar;
                        rc = RTZipTarFsStreamFromIoStream(hVfsIosTar, 0 /*fFlags*/, &hVfsFssTar);
                        RTVfsIoStrmRelease(hVfsIosTar);
                        if (RT_SUCCESS(rc))
                        {
                            rc = i_loadStoreFromTar(hVfsFssTar);
                            RTVfsFsStrmRelease(hVfsFssTar);
                        }
                        else
                            LogRel(("The given NVRAM file is neither a raw UEFI variable store nor a tar archive (opening failed with %Rrc)\n", rc));
                    }
                    else
                        LogRel(("Opening the UEFI variable store '%s' failed with %Rrc\n", pszPath, rc));

                    RTVfsFileRelease(hVfsFileNvram);
                }
                else
                    LogRel(("Failed to memorize NVRAM store '%s' with %Rrc\n", pszPath, rc));

                RTVfsIoStrmRelease(hVfsIosNvram);
            }
            else
                LogRelMax(10, ("NVRAM store '%s' couldn't be opened with %Rrc\n", pszPath, rc));
        }
        else
            LogRelMax(10, ("NVRAM store '%s' exceeds limit of %u bytes, actual size is %u\n",
                           pszPath, _1M, cbStore));
    }
    else if (rc == VERR_FILE_NOT_FOUND) /* Valid for the first run where no NVRAM file is there. */
        rc = VINF_SUCCESS;

    return rc;
}


/**
 * Saves the NVRAM store as a tar archive.
 */
int NvramStore::i_saveStoreAsTar(const char *pszPath)
{
    uint32_t        offError = 0;
    RTERRINFOSTATIC ErrInfo;
    RTVFSIOSTREAM   hVfsIos;

    int rc = RTVfsChainOpenIoStream(pszPath, RTFILE_O_WRITE | RTFILE_O_DENY_WRITE | RTFILE_O_CREATE_REPLACE,
                                    &hVfsIos, &offError, RTErrInfoInitStatic(&ErrInfo));
    if (RT_SUCCESS(rc))
    {
        RTVFSFSSTREAM hVfsFss;
        rc = RTZipTarFsStreamToIoStream(hVfsIos, RTZIPTARFORMAT_GNU, 0 /*fFlags*/, &hVfsFss);
        if (RT_SUCCESS(rc))
        {
            NvramStoreIter it = m->bd->mapNvram.begin();

            while (it != m->bd->mapNvram.end())
            {
                RTVFSFILE hVfsFile = it->second;

                rc = RTVfsFileSeek(hVfsFile, 0 /*offSeek*/, RTFILE_SEEK_BEGIN, NULL /*poffActual*/);
                AssertRC(rc);

                RTVFSOBJ hVfsObj = RTVfsObjFromFile(hVfsFile);
                rc = RTVfsFsStrmAdd(hVfsFss, it->first.c_str(), hVfsObj, 0 /*fFlags*/);
                RTVfsObjRelease(hVfsObj);
                if (RT_FAILURE(rc))
                    break;

                it++;
            }

            RTVfsFsStrmRelease(hVfsFss);
        }

        RTVfsIoStrmRelease(hVfsIos);
    }

    return rc;
}


/**
 * Saves the NVRAM store.
 *
 * @returns IPRT status code.
 */
int NvramStore::i_saveStore(void)
{
    int rc = VINF_SUCCESS;

    Utf8Str strTmp;
    NvramStore::getNonVolatileStorageFile(strTmp);

    /*
     * Only store the NVRAM content if the path is not empty, if it is
     * this means the VM was just created and the store was nnot saved yet,
     * see @bugref{10191}.
     */
    if (strTmp.isNotEmpty())
    {
        /*
         * Skip creating the tar archive if only the UEFI NVRAM content is available in order
         * to maintain backwards compatibility. As soon as there is more than one entry or
         * it doesn't belong to the UEFI the tar archive will be created.
         */
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
        if (   m->bd->mapNvram.size() == 1
            && m->bd->mapNvram.find(Utf8Str("efi/nvram")) != m->bd->mapNvram.end())
        {
            RTVFSFILE hVfsFileNvram = m->bd->mapNvram[Utf8Str("efi/nvram")];

            rc = RTVfsFileSeek(hVfsFileNvram, 0 /*offSeek*/, RTFILE_SEEK_BEGIN, NULL /*poffActual*/);
            AssertRC(rc); RT_NOREF(rc);

            RTVFSIOSTREAM hVfsIosDst;
            rc = RTVfsIoStrmOpenNormal(strTmp.c_str(), RTFILE_O_CREATE_REPLACE | RTFILE_O_WRITE | RTFILE_O_DENY_NONE,
                                       &hVfsIosDst);
            if (RT_SUCCESS(rc))
            {
                RTVFSIOSTREAM hVfsIosSrc = RTVfsFileToIoStream(hVfsFileNvram);
                Assert(hVfsIosSrc != NIL_RTVFSIOSTREAM);

                rc = RTVfsUtilPumpIoStreams(hVfsIosSrc, hVfsIosDst, 0 /*cbBufHint*/);

                RTVfsIoStrmRelease(hVfsIosSrc);
                RTVfsIoStrmRelease(hVfsIosDst);
            }
        }
        else if (m->bd->mapNvram.size())
            rc = i_saveStoreAsTar(strTmp.c_str());
        /* else: No NVRAM content to store so we are done here. */
    }

    return rc;
}


#ifndef VBOX_COM_INPROC
HRESULT NvramStore::i_retainUefiVarStore(PRTVFS phVfs, bool fReadonly)
{
    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(m->pParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock wlock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT hrc = S_OK;
    NvramStoreIter it = m->bd->mapNvram.find("efi/nvram");
    if (it != m->bd->mapNvram.end())
    {
        RTVFSFILE hVfsFileNvram = it->second;
        RTVFS hVfsEfiVarStore;
        uint32_t fMntFlags = fReadonly ? RTVFSMNT_F_READ_ONLY : 0;

        int vrc = RTEfiVarStoreOpenAsVfs(hVfsFileNvram, fMntFlags, 0 /*fVarStoreFlags*/, &hVfsEfiVarStore,
                                         NULL /*pErrInfo*/);
        if (RT_SUCCESS(vrc))
        {
            *phVfs = hVfsEfiVarStore;
            if (!fReadonly)
                m->pParent->i_setModified(Machine::IsModified_NvramStore);
        }
        else
            hrc = setError(E_FAIL, tr("Opening the UEFI variable store failed (%Rrc)."), vrc);
    }
    else
        hrc = setError(VBOX_E_OBJECT_NOT_FOUND, tr("The UEFI NVRAM file is not existing for this machine."));

    return hrc;
}


HRESULT NvramStore::i_releaseUefiVarStore(RTVFS hVfs)
{
    RTVfsRelease(hVfs);
    return S_OK;
}


/**
 *  Loads settings from the given machine node.
 *  May be called once right after this object creation.
 *
 *  @param data Configuration settings.
 *
 *  @note Locks this object for writing.
 */
HRESULT NvramStore::i_loadSettings(const settings::NvramSettings &data)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoReadLock mlock(m->pParent COMMA_LOCKVAL_SRC_POS);
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->bd->strNvramPath = data.strNvramPath;

    Utf8Str strTmp(m->bd->strNvramPath);
    if (strTmp.isNotEmpty())
        m->pParent->i_copyPathRelativeToMachine(strTmp, m->bd->strNvramPath);
    if (   m->pParent->i_getFirmwareType() == FirmwareType_BIOS
        || m->bd->strNvramPath == m->pParent->i_getDefaultNVRAMFilename())
        m->bd->strNvramPath.setNull();

    return S_OK;
}

/**
 *  Saves settings to the given machine node.
 *
 *  @param data Configuration settings.
 *
 *  @note Locks this object for writing.
 */
HRESULT NvramStore::i_saveSettings(settings::NvramSettings &data)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoWriteLock wlock(this COMMA_LOCKVAL_SRC_POS);

    data.strNvramPath = m->bd->strNvramPath;

    int vrc = i_saveStore();
    if (RT_FAILURE(vrc))
        return setError(E_FAIL, tr("Failed to save the NVRAM content to disk (%Rrc)"), vrc);

    return S_OK;
}

void NvramStore::i_rollback()
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    m->bd.rollback();
}

void NvramStore::i_commit()
{
    /* sanity */
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    /* sanity too */
    AutoCaller peerCaller(m->pPeer);
    AssertComRCReturnVoid(peerCaller.rc());

    /* lock both for writing since we modify both (mPeer is "master" so locked
     * first) */
    AutoMultiWriteLock2 alock(m->pPeer, this COMMA_LOCKVAL_SRC_POS);

    if (m->bd.isBackedUp())
    {
        m->bd.commit();
        if (m->pPeer)
        {
            /* attach new data to the peer and reshare it */
            AutoWriteLock peerlock(m->pPeer COMMA_LOCKVAL_SRC_POS);
            m->pPeer->m->bd.attach(m->bd);
        }
    }
}

void NvramStore::i_copyFrom(NvramStore *aThat)
{
    AssertReturnVoid(aThat != NULL);

    /* sanity */
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    /* sanity too */
    AutoCaller thatCaller(aThat);
    AssertComRCReturnVoid(thatCaller.rc());

    /* peer is not modified, lock it for reading (aThat is "master" so locked
     * first) */
    AutoReadLock rl(aThat COMMA_LOCKVAL_SRC_POS);
    AutoWriteLock wl(this COMMA_LOCKVAL_SRC_POS);

    /* this will back up current data */
    m->bd.assignCopy(aThat->m->bd);

    // Intentionally "forget" the NVRAM file since it must be unique and set
    // to the correct value before the copy of the settings makes sense.
    m->bd->strNvramPath.setNull();
}

HRESULT NvramStore::i_applyDefaults(GuestOSType *aOSType)
{
    HRESULT hrc = S_OK;

    if (aOSType->i_recommendedEFISecureBoot())
    {
        /* Initialize the UEFI variable store and enroll default keys. */
        hrc = initUefiVariableStore(0 /*aSize*/);
        if (SUCCEEDED(hrc))
        {
            ComPtr<IUefiVariableStore> pVarStore;

            hrc = getUefiVariableStore(pVarStore);
            if (SUCCEEDED(hrc))
            {
                hrc = pVarStore->EnrollOraclePlatformKey();
                if (SUCCEEDED(hrc))
                    hrc = pVarStore->EnrollDefaultMsSignatures();
            }
        }
    }

    return hrc;
}

void NvramStore::i_updateNonVolatileStorageFile(const Utf8Str &aNonVolatileStorageFile)
{
    /* sanity */
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    AutoReadLock mlock(m->pParent COMMA_LOCKVAL_SRC_POS);
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    Utf8Str strTmp(aNonVolatileStorageFile);
    if (strTmp == m->pParent->i_getDefaultNVRAMFilename())
        strTmp.setNull();

    if (strTmp == m->bd->strNvramPath)
        return;

    m->bd.backup();
    m->bd->strNvramPath = strTmp;
}

#else
//
// private methods
//
/*static*/
DECLCALLBACK(int) NvramStore::i_nvramStoreQuerySize(PPDMIVFSCONNECTOR pInterface, const char *pszNamespace, const char *pszPath,
                                                    uint64_t *pcb)
{
    PDRVMAINNVRAMSTORE pThis = RT_FROM_MEMBER(pInterface, DRVMAINNVRAMSTORE, IVfs);

    AutoReadLock rlock(pThis->pNvramStore COMMA_LOCKVAL_SRC_POS);
    NvramStoreIter it = pThis->pNvramStore->m->bd->mapNvram.find(Utf8StrFmt("%s/%s", pszNamespace, pszPath));
    if (it != pThis->pNvramStore->m->bd->mapNvram.end())
    {
        RTVFSFILE hVfsFile = it->second;
        return RTVfsFileQuerySize(hVfsFile, pcb);
    }

    return VERR_NOT_FOUND;
}


/*static*/
DECLCALLBACK(int) NvramStore::i_nvramStoreReadAll(PPDMIVFSCONNECTOR pInterface, const char *pszNamespace, const char *pszPath,
                                                  void *pvBuf, size_t cbRead)
{
    PDRVMAINNVRAMSTORE pThis = RT_FROM_MEMBER(pInterface, DRVMAINNVRAMSTORE, IVfs);

    AutoReadLock rlock(pThis->pNvramStore COMMA_LOCKVAL_SRC_POS);
    NvramStoreIter it = pThis->pNvramStore->m->bd->mapNvram.find(Utf8StrFmt("%s/%s", pszNamespace, pszPath));
    if (it != pThis->pNvramStore->m->bd->mapNvram.end())
    {
        RTVFSFILE hVfsFile = it->second;

        int rc = RTVfsFileSeek(hVfsFile, 0 /*offSeek*/, RTFILE_SEEK_BEGIN, NULL /*poffActual*/);
        AssertRC(rc); RT_NOREF(rc);

        return RTVfsFileRead(hVfsFile, pvBuf, cbRead, NULL /*pcbRead*/);
    }

    return VERR_NOT_FOUND;
}


/*static*/
DECLCALLBACK(int) NvramStore::i_nvramStoreWriteAll(PPDMIVFSCONNECTOR pInterface, const char *pszNamespace, const char *pszPath,
                                                   const void *pvBuf, size_t cbWrite)
{
    PDRVMAINNVRAMSTORE pThis = RT_FROM_MEMBER(pInterface, DRVMAINNVRAMSTORE, IVfs);

    AutoWriteLock wlock(pThis->pNvramStore COMMA_LOCKVAL_SRC_POS);

    int rc = VINF_SUCCESS;
    NvramStoreIter it = pThis->pNvramStore->m->bd->mapNvram.find(Utf8StrFmt("%s/%s", pszNamespace, pszPath));
    if (it != pThis->pNvramStore->m->bd->mapNvram.end())
    {
        RTVFSFILE hVfsFile = it->second;

        rc = RTVfsFileSeek(hVfsFile, 0 /*offSeek*/, RTFILE_SEEK_BEGIN, NULL /*poffActual*/);
        AssertRC(rc);
        rc = RTVfsFileSetSize(hVfsFile, cbWrite, RTVFSFILE_SIZE_F_NORMAL);
        if (RT_SUCCESS(rc))
            rc = RTVfsFileWrite(hVfsFile, pvBuf, cbWrite, NULL /*pcbWritten*/);
    }
    else
    {
        /* Create a new entry. */
        RTVFSFILE hVfsFile = NIL_RTVFSFILE;
        rc = RTVfsFileFromBuffer(RTFILE_O_READ | RTFILE_O_WRITE, pvBuf, cbWrite, &hVfsFile);
        if (RT_SUCCESS(rc))
            pThis->pNvramStore->m->bd->mapNvram[Utf8StrFmt("%s/%s", pszNamespace, pszPath)] = hVfsFile;
    }

    return rc;
}


/*static*/
DECLCALLBACK(int) NvramStore::i_nvramStoreDelete(PPDMIVFSCONNECTOR pInterface, const char *pszNamespace, const char *pszPath)
{
    PDRVMAINNVRAMSTORE pThis = RT_FROM_MEMBER(pInterface, DRVMAINNVRAMSTORE, IVfs);

    AutoWriteLock wlock(pThis->pNvramStore COMMA_LOCKVAL_SRC_POS);
    NvramStoreIter it = pThis->pNvramStore->m->bd->mapNvram.find(Utf8StrFmt("%s/%s", pszNamespace, pszPath));
    if (it != pThis->pNvramStore->m->bd->mapNvram.end())
    {
        RTVFSFILE hVfsFile = it->second;
        pThis->pNvramStore->m->bd->mapNvram.erase(it);
        RTVfsFileRelease(hVfsFile);
        return VINF_SUCCESS;
    }

    return VERR_NOT_FOUND;
}


/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
DECLCALLBACK(void *) NvramStore::i_drvQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS          pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVMAINNVRAMSTORE  pDrv    = PDMINS_2_DATA(pDrvIns, PDRVMAINNVRAMSTORE);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIVFSCONNECTOR, &pDrv->IVfs);
    return NULL;
}


/**
 * Destruct a NVRAM store driver instance.
 *
 * @returns VBox status code.
 * @param   pDrvIns     The driver instance data.
 */
DECLCALLBACK(void) NvramStore::i_drvDestruct(PPDMDRVINS pDrvIns)
{
    PDMDRV_CHECK_VERSIONS_RETURN_VOID(pDrvIns);
    PDRVMAINNVRAMSTORE pThis = PDMINS_2_DATA(pDrvIns, PDRVMAINNVRAMSTORE);
    LogFlow(("NvramStore::drvDestruct: iInstance=%d\n", pDrvIns->iInstance));

    if (pThis->pNvramStore)
    {
        uint32_t cRefs = ASMAtomicDecU32(&pThis->pNvramStore->m->cRefs);
        if (!cRefs)
        {
            int rc = pThis->pNvramStore->i_saveStore();
            AssertRC(rc); /** @todo Disk full error? */
        }
    }
}


/**
 * Construct a NVRAM store driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
DECLCALLBACK(int) NvramStore::i_drvConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);
    RT_NOREF(fFlags, pCfg);
    PDRVMAINNVRAMSTORE pThis = PDMINS_2_DATA(pDrvIns, PDRVMAINNVRAMSTORE);
    LogFlow(("NvramStore::drvConstruct: iInstance=%d\n", pDrvIns->iInstance));

    /*
     * Validate configuration.
     */
    PDMDRV_VALIDATE_CONFIG_RETURN(pDrvIns, "", "");
    AssertMsgReturn(PDMDrvHlpNoAttach(pDrvIns) == VERR_PDM_NO_ATTACHED_DRIVER,
                    ("Configuration error: Not possible to attach anything to this driver!\n"),
                    VERR_PDM_DRVINS_NO_ATTACH);

    /*
     * IBase.
     */
    pDrvIns->IBase.pfnQueryInterface    = NvramStore::i_drvQueryInterface;

    pThis->IVfs.pfnQuerySize            = NvramStore::i_nvramStoreQuerySize;
    pThis->IVfs.pfnReadAll              = NvramStore::i_nvramStoreReadAll;
    pThis->IVfs.pfnWriteAll             = NvramStore::i_nvramStoreWriteAll;
    pThis->IVfs.pfnDelete               = NvramStore::i_nvramStoreDelete;

    /*
     * Get the NVRAM store object pointer.
     */
    com::Guid uuid(COM_IIDOF(INvramStore));
    pThis->pNvramStore = (NvramStore *)PDMDrvHlpQueryGenericUserObject(pDrvIns, uuid.raw());
    if (!pThis->pNvramStore)
    {
        AssertMsgFailed(("Configuration error: No/bad NVRAM store object!\n"));
        return VERR_NOT_FOUND;
    }

    uint32_t cRefs = ASMAtomicIncU32(&pThis->pNvramStore->m->cRefs);
    if (cRefs == 1)
    {
        int rc = pThis->pNvramStore->i_loadStore(pThis->pNvramStore->m->bd->strNvramPath.c_str());
        if (RT_FAILURE(rc))
        {
            ASMAtomicDecU32(&pThis->pNvramStore->m->cRefs);
            return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS,
                                       N_("Failed to load the NVRAM store from the file"));
        }
    }

    return VINF_SUCCESS;
}


/**
 * NVRAM store driver registration record.
 */
const PDMDRVREG NvramStore::DrvReg =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName */
    "NvramStore",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "Main NVRAM store driver (Main as in the API).",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_STATUS,
    /* cMaxInstances */
    ~0U,
    /* cbInstance */
    sizeof(DRVMAINNVRAMSTORE),
    /* pfnConstruct */
    NvramStore::i_drvConstruct,
    /* pfnDestruct */
    NvramStore::i_drvDestruct,
    /* pfnRelocate */
    NULL,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    NULL,
    /* pfnSuspend */
    NULL,
    /* pfnResume */
    NULL,
    /* pfnAttach */
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnPowerOff */
    NULL,
    /* pfnSoftReset */
    NULL,
    /* u32EndVersion */
    PDM_DRVREG_VERSION
};
#endif /* !VBOX_COM_INPROC */

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
