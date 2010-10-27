/* $Id$ */
/** @file
 * VirtualBox Main - interface for Extension Packs, VBoxSVC & VBoxC.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "ExtPackManagerImpl.h"

#include <iprt/ctype.h>
#include <iprt/dir.h>
#include <iprt/env.h>
#include <iprt/file.h>
#include <iprt/ldr.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/pipe.h>
#include <iprt/process.h>
#include <iprt/string.h>

#include <VBox/com/array.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/log.h>
#include "AutoCaller.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** @name VBOX_EXTPACK_HELPER_NAME
 * The name of the utility program we employ to install and uninstall the
 * extension packs.  This is a set-uid-to-root binary on unixy platforms, which
 * is why it has to be a separate program.
 */
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
# define VBOX_EXTPACK_HELPER_NAME   "VBoxExtPackHelper.exe"
#else
# define VBOX_EXTPACK_HELPER_NAME   "VBoxExtPackHelper"
#endif


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Private extension pack data.
 */
struct ExtPack::Data
{
    /** @name IExtPack Attributes.
     * @{ */
    /** The name. */
    Utf8Str     strName;
    /** The version string. */
    Utf8Str     strVersion;
    /** The revision. */
    uint32_t    uRevision;
    /** Whether it's usable or not. */
    bool        fUsable;
    /** Why it is unusable. */
    Utf8Str     strWhyUnusable;
    /** @}  */

    /** Where the extension pack is located. */
    Utf8Str     strPath;
    /** The module handle of the main extension pack module. */
    RTLDRMOD    hMainMod;
};

/** List of extension packs. */
typedef std::list< ComObjPtr<ExtPack> > ExtPackList;

/**
 * Private extension pack manager data.
 */
struct ExtPackManager::Data
{
    /** Where the directory where the extension packs are installed. */
    Utf8Str     strBasePath;
    /** The list of installed extension packs. */
    ExtPackList llInstalledExtPacks;
};


DEFINE_EMPTY_CTOR_DTOR(ExtPack)

/**
 * Called by ComObjPtr::createObject when creating the object.
 *
 * Just initialize the basic object state, do the rest in init().
 *
 * @returns S_OK.
 */
HRESULT ExtPack::FinalConstruct()
{
    m = NULL;
    return S_OK;
}

/**
 * Initializes the extension pack by reading its file.
 *
 * @returns COM status code.
 * @param   a_pszName       The name of the extension pack.  This is also the
 *                          name of the subdirector under @a a_pszParentDir
 *                          where the extension pack is installed.
 * @param   a_pszParentDir  The parent directory.
 */
HRESULT ExtPack::init(const char *a_pszName, const char *a_pszParentDir)
{
    /*
     * Figure out where we live and allocate+initialize our private data.
     */
    char szDir[RTPATH_MAX];
    int vrc = RTPathJoin(szDir, sizeof(szDir), a_pszParentDir, a_pszName);
    AssertLogRelRCReturn(vrc, E_FAIL);

    m = new Data;
    m->strName          = a_pszName;
    m->strVersion       = "";
    m->uRevision        = 0;
    m->fUsable          = false;
    m->strWhyUnusable   = tr("ExtPack::init failed");

    m->strPath          = szDir;
    m->hMainMod         = NIL_RTLDRMOD;

    /*
     * Read the description file.
     */


    return S_OK;
}

/**
 * COM cruft.
 */
void ExtPack::FinalRelease()
{
    uninit();
}

/**
 * Do the actual cleanup.
 */
void ExtPack::uninit()
{
    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (!autoUninitSpan.uninitDone() && m != NULL)
    {
        if (m->hMainMod != NIL_RTLDRMOD)
        {
            RTLdrClose(m->hMainMod);
            m->hMainMod = NIL_RTLDRMOD;
        }

        delete m;
        m = NULL;
    }
}




STDMETHODIMP ExtPack::COMGETTER(Name)(BSTR *a_pbstrName)
{
    CheckComArgOutPointerValid(a_pbstrName);

    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
        m->strName.cloneTo(a_pbstrName);
    return hrc;
}

STDMETHODIMP ExtPack::COMGETTER(Version)(BSTR *a_pbstrVersion)
{
    CheckComArgOutPointerValid(a_pbstrVersion);

    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
        m->strVersion.cloneTo(a_pbstrVersion);
    return hrc;
}

STDMETHODIMP ExtPack::COMGETTER(Revision)(ULONG *a_puRevision)
{
    CheckComArgOutPointerValid(a_puRevision);

    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
        *a_puRevision = m->uRevision;
    return hrc;
}

STDMETHODIMP ExtPack::COMGETTER(Usable)(BOOL *a_pfUsable)
{
    CheckComArgOutPointerValid(a_pfUsable);

    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
        *a_pfUsable = m->fUsable;
    return hrc;
}

STDMETHODIMP ExtPack::COMGETTER(WhyUnusable)(BSTR *a_pbstrWhy)
{
    CheckComArgOutPointerValid(a_pbstrWhy);

    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
        m->strWhyUnusable.cloneTo(a_pbstrWhy);
    return hrc;
}



void *ExtPack::getCallbackTable()
{
    return NULL;
}

/**
 * Refreshes the extension pack state.
 *
 * This is called by the manager so that the on disk changes are picked up.
 *
 * @returns S_OK or COM error status with error information.
 * @param   pfCanDelete     Optional can-delete-this-object output indicator.
 */
HRESULT ExtPack::refresh(bool *pfCanDelete)
{
    if (pfCanDelete)
        *pfCanDelete = false;
    return S_OK;
}




DEFINE_EMPTY_CTOR_DTOR(ExtPackManager)

/**
 * Called by ComObjPtr::createObject when creating the object.
 *
 * Just initialize the basic object state, do the rest in init().
 *
 * @returns S_OK.
 */
HRESULT ExtPackManager::FinalConstruct()
{
    m = NULL;
    return S_OK;
}

/**
 * Initializes the extension pack manager.
 *
 * @returns COM status code.
 */
HRESULT ExtPackManager::init()
{
    /*
     * Figure some stuff out before creating the instance data.
     */
    char szBasePath[RTPATH_MAX];
    int rc = RTPathAppPrivateArch(szBasePath, sizeof(szBasePath));
    AssertLogRelRCReturn(rc, E_FAIL);
    rc = RTPathAppend(szBasePath, sizeof(szBasePath), "ExtensionPacks");
    AssertLogRelRCReturn(rc, E_FAIL);

    /*
     * Allocate and initialize the instance data.
     */
    m = new Data;
    m->strBasePath = szBasePath;

    /*
     * Go looking for extensions.  The RTDirOpen may fail if nothing has been
     * installed yet, or if root is paranoid and has revoked our access to them.
     *
     * We ASSUME that there are no files, directories or stuff in the directory
     * that exceed the max name length in RTDIRENTRYEX.
     */
    PRTDIR pDir;
    int vrc = RTDirOpen(&pDir, szBasePath);
    if (RT_FAILURE(vrc))
        return S_OK;
    HRESULT hrc = S_OK;
    for (;;)
    {
        RTDIRENTRYEX Entry;
        vrc = RTDirReadEx(pDir, &Entry, NULL /*pcbDirEntry*/, RTFSOBJATTRADD_NOTHING, RTPATH_F_ON_LINK);
        if (RT_FAILURE(vrc))
        {
            AssertLogRelMsg(vrc == VERR_NO_MORE_FILES, ("%Rrc\n", vrc));
            break;
        }
        if (   RTFS_IS_DIRECTORY(Entry.Info.Attr.fMode)
            && strcmp(Entry.szName, ".")  != 0
            && strcmp(Entry.szName, "..") != 0 )
        {
            /*
             * All directories are extensions, the shall be nothing but
             * extensions in this subdirectory.
             */
            ComObjPtr<ExtPack> NewExtPack;
            HRESULT hrc2 = NewExtPack.createObject();
            if (SUCCEEDED(hrc2))
                hrc2 = NewExtPack->init(Entry.szName, szBasePath);
            if (SUCCEEDED(hrc2))
                m->llInstalledExtPacks.push_back(NewExtPack);
            else if (SUCCEEDED(rc))
                hrc = hrc2;
        }
    }
    RTDirClose(pDir);

    return hrc;
}

/**
 * COM cruft.
 */
void ExtPackManager::FinalRelease()
{
    uninit();
}

/**
 * Do the actual cleanup.
 */
void ExtPackManager::uninit()
{
    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (!autoUninitSpan.uninitDone() && m != NULL)
    {
        /** @todo do unload notifications */

        delete m;
        m = NULL;
    }
}


STDMETHODIMP ExtPackManager::COMGETTER(InstalledExtPacks)(ComSafeArrayOut(IExtPack *, a_paExtPacks))
{
    CheckComArgSafeArrayNotNull(a_paExtPacks);

    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
    {
        AutoReadLock autoLock(this COMMA_LOCKVAL_SRC_POS);

        SafeIfaceArray<IExtPack> SaExtPacks(m->llInstalledExtPacks);
        SaExtPacks.detachTo(ComSafeArrayOutArg(a_paExtPacks));
    }

    return hrc;
}

STDMETHODIMP ExtPackManager::Find(IN_BSTR a_bstrName, IExtPack **a_pExtPack)
{
    CheckComArgNotNull(a_bstrName);
    CheckComArgOutPointerValid(a_pExtPack);
    Utf8Str strName(a_bstrName);

    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
    {
        AutoReadLock autoLock(this COMMA_LOCKVAL_SRC_POS);

        ComPtr<ExtPack> ptrExtPack = findExtPack(strName.c_str());
        if (!ptrExtPack.isNull())
            ptrExtPack.queryInterfaceTo(a_pExtPack);
        else
            hrc = VBOX_E_OBJECT_NOT_FOUND;
    }

    return hrc;
}

STDMETHODIMP ExtPackManager::Install(IN_BSTR a_bstrTarball, BSTR *a_pbstrName)
{
    CheckComArgNotNull(a_bstrTarball);
    CheckComArgOutPointerValid(a_pbstrName);
    Utf8Str strTarball(a_bstrTarball);

    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
    {
        AutoWriteLock autoLock(this COMMA_LOCKVAL_SRC_POS);

        /*
         * Check that the file exists and that we can access it.
         */
        if (RTFileExists(strTarball.c_str()))
        {
            RTFILE hFile;
            int vrc = RTFileOpen(&hFile, strTarball.c_str(), RTFILE_O_READ | RTFILE_O_DENY_WRITE | RTFILE_O_OPEN);
            if (RT_SUCCESS(vrc))
            {
                RTFSOBJINFO ObjInfo;
                vrc = RTFileQueryInfo(hFile, &ObjInfo, RTFSOBJATTRADD_NOTHING);
                if (   RT_SUCCESS(vrc)
                    && RTFS_IS_FILE(ObjInfo.Attr.fMode))
                {
                    /*
                     * Derive the name of the extension pack from the file
                     * name.
                     *
                     * RESTRICTION: The name can only contain english alphabet
                     *              charactes, decimal digits and space.
                     *              Impose a max length of 64 chars.
                     */
                    char *pszName = RTStrDup(RTPathFilename(strTarball.c_str()));
                    if (pszName)
                    {
                        char *pszEnd = pszName;
                        while (RT_C_IS_ALNUM(*pszEnd) || *pszEnd == ' ')
                            pszEnd++;
                        if (   pszEnd == pszName
                            || pszEnd - pszName <= 64)
                        {
                            *pszEnd = '\0';

                            /*
                             * Refresh the data we have on the extension pack
                             * as it may be made stale by direct meddling or
                             * some other user.
                             */
                            ExtPack *pExtPack;
                            hrc = refreshExtPack(pszName, false /*a_fUnsuableIsError*/, &pExtPack);
                            if (SUCCEEDED(hrc) && !pExtPack)
                            {
                                /*
                                 * Run the set-uid-to-root binary that performs the actual
                                 * installation.  Then create an object for the packet (we
                                 * do this even on failure, to be on the safe side).
                                 */
                                char szTarballFd[64];
                                RTStrPrintf(szTarballFd, sizeof(szTarballFd), "0x%RX64",
                                            (uint64_t)RTFileToNative(hFile));

                                hrc = runSetUidToRootHelper("install",
                                                            "--basepath",   m->strBasePath.c_str(),
                                                            "--name",       pszName,
                                                            "--tarball",    strTarball.c_str(),
                                                            "--tarball-fd", &szTarballFd[0],
                                                            NULL);
                                if (SUCCEEDED(hrc))
                                {
                                    hrc = refreshExtPack(pszName, true /*a_fUnsuableIsError*/, &pExtPack);
                                    if (SUCCEEDED(hrc))
                                        LogRel(("ExtPackManager: Successfully installed extension pack '%s'.\n", pszName));
                                }
                                else
                                {
                                    ErrorInfoKeeper Eik;
                                    refreshExtPack(pszName, false /*a_fUnsuableIsError*/, NULL);
                                }
                            }
                            else if (SUCCEEDED(hrc))
                                hrc = setError(E_FAIL,
                                               tr("Extension pack '%s' is already installed."
                                                  " In case of a reinstallation, please uninstall it first"),
                                               pszName);
                        }
                        else
                            hrc = setError(E_FAIL, tr("Malformed '%s' file name"), strTarball.c_str());
                    }
                    else
                        hrc = E_OUTOFMEMORY;
                }
                else if (RT_SUCCESS(vrc))
                    hrc = setError(E_FAIL, tr("'%s' is not a regular file"), strTarball.c_str());
                else
                    hrc = setError(E_FAIL, tr("Failed to query info on '%s' (%Rrc)"), strTarball.c_str(), vrc);
                RTFileClose(hFile);
            }
            else
                hrc = setError(E_FAIL, tr("Failed to open '%s' (%Rrc)"), strTarball.c_str(), vrc);
        }
        else if (RTPathExists(strTarball.c_str()))
            hrc = setError(E_FAIL, tr("'%s' is not a regular file"), strTarball.c_str());
        else
            hrc = setError(E_FAIL, tr("File '%s' was inaccessible or not found"), strTarball.c_str());
    }

    return hrc;
}

STDMETHODIMP ExtPackManager::Uninstall(IN_BSTR a_bstrName, BOOL a_fForcedRemoval)
{
    CheckComArgNotNull(a_bstrName);
    Utf8Str strName(a_bstrName);

    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
    {
        AutoWriteLock autoLock(this COMMA_LOCKVAL_SRC_POS);

        /*
         * Refresh the data we have on the extension pack as it may be made
         * stale by direct meddling or some other user.
         */
        ExtPack *pExtPack;
        hrc = refreshExtPack(strName.c_str(), false /*a_fUnsuableIsError*/, &pExtPack);
        if (SUCCEEDED(hrc))
        {
            if (!pExtPack)
            {
                LogRel(("ExtPackManager: Extension pack '%s' is not installed, so nothing to uninstall.\n", strName.c_str()));
                hrc = S_OK;             /* nothing to uninstall */
            }
            else
            {
                /*
                 * Run the set-uid-to-root binary that performs the
                 * uninstallation.  Then refresh the object.
                 *
                 * This refresh is theorically subject to races, but it's of
                 * the don't-do-that variety.
                 */
                hrc = runSetUidToRootHelper("uninstall",
                                            "--basepath", m->strBasePath.c_str(),
                                            "--name",     strName.c_str(),
                                            NULL);
                if (SUCCEEDED(hrc))
                {
                    hrc = refreshExtPack(strName.c_str(), false /*a_fUnsuableIsError*/, &pExtPack);
                    if (SUCCEEDED(hrc))
                    {
                        if (!pExtPack)
                            LogRel(("ExtPackManager: Successfully uninstalled extension pack '%s'.\n", strName.c_str()));
                        else
                            hrc = setError(E_UNEXPECTED,
                                           tr("Uninstall extension pack '%s' failed under mysterious circumstances"),
                                           strName.c_str());
                    }
                }
                else
                {
                    ErrorInfoKeeper Eik;
                    refreshExtPack(strName.c_str(), false /*a_fUnsuableIsError*/, NULL);
                }

            }
        }
    }

    return hrc;
}


/**
 * Runs the helper program that does the privileged operations.
 *
 * @returns S_OK or a failure status with error information set.
 * @param   a_pszCommand        The command to execute.
 * @param   ...                 The argument strings that goes along with the
 *                              command. Maximum is about 16.  Terminated by a
 *                              NULL.
 */
HRESULT ExtPackManager::runSetUidToRootHelper(const char *a_pszCommand, ...)
{
    /*
     * Calculate the path to the helper program.
     */
    char szExecName[RTPATH_MAX];
    int vrc = RTPathAppPrivateArch(szExecName, sizeof(szExecName));
    AssertLogRelRCReturn(vrc, E_UNEXPECTED);

    vrc = RTPathAppend(szExecName, sizeof(szExecName), VBOX_EXTPACK_HELPER_NAME);
    AssertLogRelRCReturn(vrc, E_UNEXPECTED);

    /*
     * Convert the variable argument list to a RTProcCreate argument vector.
     */
    const char *apszArgs[20];
    unsigned    cArgs = 0;
    apszArgs[cArgs++] = &szExecName[0];
    apszArgs[cArgs++] = a_pszCommand;

    va_list va;
    va_start(va, a_pszCommand);
    const char *pszLastArg;
    LogRel(("ExtPack: Executing '%s'", szExecName));
    do
    {
        LogRel((" '%s'", apszArgs[cArgs - 1]));
        AssertReturn(cArgs < RT_ELEMENTS(apszArgs) - 1, E_UNEXPECTED);
        pszLastArg = va_arg(va, const char *);
        apszArgs[cArgs++] = pszLastArg;
    } while (pszLastArg != NULL);
    LogRel(("\n"));
    va_end(va);
    apszArgs[cArgs] = NULL;

    /*
     * Create a PIPE which we attach to stderr so that we can read the error
     * message on failure and report it back to the caller.
     */
    RTPIPE      hPipeR;
    RTHANDLE    hStdErrPipe;
    hStdErrPipe.enmType = RTHANDLETYPE_PIPE;
    vrc = RTPipeCreate(&hPipeR, &hStdErrPipe.u.hPipe, RTPIPE_C_INHERIT_WRITE);
    AssertLogRelRCReturn(vrc, E_UNEXPECTED);

    /*
     * Spawn the process.
     */
    HRESULT hrc;
    RTPROCESS hProcess;
    vrc = RTProcCreateEx(szExecName,
                         apszArgs,
                         RTENV_DEFAULT,
                         0 /*fFlags*/,
                         NULL /*phStdIn*/,
                         NULL /*phStdOut*/,
                         &hStdErrPipe,
                         NULL /*pszAsUser*/,
                         NULL /*pszPassword*/,
                         &hProcess);
    if (RT_SUCCESS(vrc))
    {
        vrc = RTPipeClose(hStdErrPipe.u.hPipe);
        hStdErrPipe.u.hPipe = NIL_RTPIPE;

        /*
         * Read the pipe output until the process completes.
         */
        RTPROCSTATUS    ProcStatus   = { -42, RTPROCEXITREASON_ABEND };
        size_t          cbStdErrBuf  = 0;
        size_t          offStdErrBuf = 0;
        char           *pszStdErrBuf = NULL;
        do
        {
            /*
             * Service the pipe. Block waiting for output or the pipe breaking
             * when the process terminates.
             */
            if (hPipeR != NIL_RTPIPE)
            {
                char    achBuf[16]; //1024
                size_t  cbRead;
                vrc = RTPipeReadBlocking(hPipeR, achBuf, sizeof(achBuf), &cbRead);
                if (RT_SUCCESS(vrc))
                {
                    /* grow the buffer? */
                    size_t cbBufReq = offStdErrBuf + cbRead + 1;
                    if (   cbBufReq > cbStdErrBuf
                        && cbBufReq < _256K)
                    {
                        size_t cbNew = RT_ALIGN_Z(cbBufReq, 16); // 1024
                        void  *pvNew = RTMemRealloc(pszStdErrBuf, cbNew);
                        if (pvNew)
                        {
                            pszStdErrBuf = (char *)pvNew;
                            cbStdErrBuf  = cbNew;
                        }
                    }

                    /* append if we've got room. */
                    if (cbBufReq <= cbStdErrBuf)
                    {
                        memcpy(&pszStdErrBuf[offStdErrBuf], achBuf, cbRead);
                        offStdErrBuf = offStdErrBuf + cbRead;
                        pszStdErrBuf[offStdErrBuf] = '\0';
                    }
                }
                else
                {
                    AssertLogRelMsg(vrc == VERR_BROKEN_PIPE, ("%Rrc\n", vrc));
                    RTPipeClose(hPipeR);
                    hPipeR = NIL_RTPIPE;
                }
            }

            /*
             * Service the process.  Block if we have no pipe.
             */
            if (hProcess != NIL_RTPROCESS)
            {
                vrc = RTProcWait(hProcess,
                                 hPipeR == NIL_RTPIPE ? RTPROCWAIT_FLAGS_BLOCK : RTPROCWAIT_FLAGS_NOBLOCK,
                                 &ProcStatus);
                if (RT_SUCCESS(vrc))
                    hProcess = NIL_RTPROCESS;
                else
                    AssertLogRelMsgStmt(vrc != VERR_PROCESS_RUNNING, ("%Rrc\n", vrc), hProcess = NIL_RTPROCESS);
            }
        } while (   hPipeR != NIL_RTPIPE
                 || hProcess != NIL_RTPROCESS);

        /*
         * Compose the status code and, on failure, error message.
         */
        LogRel(("ExtPack: enmReason=%d iStatus=%d stderr='%s'\n",
                ProcStatus.enmReason, ProcStatus.iStatus, offStdErrBuf ? pszStdErrBuf : ""));

        if (   ProcStatus.enmReason == RTPROCEXITREASON_NORMAL
            && ProcStatus.iStatus   == 0
            && offStdErrBuf         == 0)
            hrc = S_OK;
        else if (ProcStatus.enmReason == RTPROCEXITREASON_NORMAL)
        {
            AssertMsg(ProcStatus.iStatus != 0, ("%s\n", pszStdErrBuf));
            hrc = setError(E_UNEXPECTED, tr("The installer failed with exit code %d: %s"),
                           ProcStatus.iStatus, offStdErrBuf ? pszStdErrBuf : "");
        }
        else if (ProcStatus.enmReason == RTPROCEXITREASON_SIGNAL)
            hrc = setError(E_UNEXPECTED, tr("The installer was killed by signal #d (stderr: %s)"),
                           ProcStatus.iStatus, offStdErrBuf ? pszStdErrBuf : "");
        else if (ProcStatus.enmReason == RTPROCEXITREASON_ABEND)
            hrc = setError(E_UNEXPECTED, tr("The installer aborted abnormally (stderr: %s)"),
                           offStdErrBuf ? pszStdErrBuf : "");
        else
            hrc = setError(E_UNEXPECTED, tr("internal error: enmReason=%d iStatus=%d stderr='%s'"),
                           ProcStatus.enmReason, ProcStatus.iStatus, offStdErrBuf ? pszStdErrBuf : "");

        RTMemFree(pszStdErrBuf);
    }
    else
        hrc = setError(VBOX_E_IPRT_ERROR, tr("Failed to launch the helper application '%s' (%Rrc)"), szExecName, vrc);

    RTPipeClose(hPipeR);
    RTPipeClose(hStdErrPipe.u.hPipe);

    return hrc;
}

/**
 * Finds an installed extension pack.
 *
 * @returns Pointer to the extension pack if found, NULL if not. (No reference
 *          counting problem here since the caller must be holding the lock.)
 * @param   a_pszName       The name of the extension pack.
 */
ExtPack *ExtPackManager::findExtPack(const char *a_pszName)
{
    size_t cchName = strlen(a_pszName);

    for (ExtPackList::iterator it = m->llInstalledExtPacks.begin();
         it != m->llInstalledExtPacks.end();
         it++)
    {
        ExtPack::Data *pExtPackData = (*it)->m;
        if (   pExtPackData
            && pExtPackData->strName.length() == cchName
            && pExtPackData->strName.compare(a_pszName, iprt::MiniString::CaseInsensitive) == 0)
            return (*it);
    }
    return NULL;
}

/**
 * Removes an installed extension pack from the internal list.
 *
 * The package is expected to exist!
 *
 * @param   a_pszName       The name of the extension pack.
 */
void ExtPackManager::removeExtPack(const char *a_pszName)
{
    size_t cchName = strlen(a_pszName);

    for (ExtPackList::iterator it = m->llInstalledExtPacks.begin();
         it != m->llInstalledExtPacks.end();
         it++)
    {
        ExtPack::Data *pExtPackData = (*it)->m;
        if (   pExtPackData
            && pExtPackData->strName.length() == cchName
            && pExtPackData->strName.compare(a_pszName, iprt::MiniString::CaseInsensitive) == 0)
        {
            m->llInstalledExtPacks.erase(it);
            return;
        }
    }
    AssertMsgFailed(("%s\n", a_pszName));
}

/**
 * Refreshes the specified extension pack.
 *
 * This may remove the extension pack from the list, so any non-smart pointers
 * to the extension pack object may become invalid.
 *
 * @returns S_OK and *ppExtPack on success, COM status code and error message
 *          on failure.
 * @param   a_pszName           The extension to update..
 * @param   a_fUnsuableIsError  If @c true, report an unusable extension pack
 *                              as an error.
 * @param   a_ppExtPack         Where to store the pointer to the extension
 *                              pack of it is still around after the refresh.
 *                              This is optional.
 */
HRESULT ExtPackManager::refreshExtPack(const char *a_pszName, bool a_fUnsuableIsError, ExtPack **a_ppExtPack)
{
    HRESULT hrc;
    ExtPack *pExtPack = findExtPack(a_pszName);
    if (pExtPack)
    {
        /*
         * Refresh existing object.
         */
        bool fCanDelete;
        hrc = pExtPack->refresh(&fCanDelete);
        if (SUCCEEDED(hrc))
        {
            if (fCanDelete)
            {
                removeExtPack(a_pszName);
                pExtPack = NULL;
            }
        }
    }
    else
    {
        /*
         * Does the dir exist?  Make some special effort to deal with case
         * sensitivie file systems (a_pszName is case insensitive).
         */
        char szDir[RTPATH_MAX];
        int vrc = RTPathJoin(szDir, sizeof(szDir), m->strBasePath.c_str(), a_pszName);
        AssertLogRelRCReturn(vrc, E_FAIL);

        RTDIRENTRYEX    Entry;
        RTFSOBJINFO     ObjInfo;
        vrc = RTPathQueryInfoEx(szDir, &ObjInfo, RTFSOBJATTRADD_NOTHING, RTPATH_F_ON_LINK);
        bool fExists = RT_SUCCESS(vrc) && RTFS_IS_DIRECTORY(ObjInfo.Attr.fMode);
        if (!fExists)
        {
            PRTDIR pDir;
            vrc = RTDirOpen(&pDir, m->strBasePath.c_str());
            if (RT_SUCCESS(vrc))
            {
                for (;;)
                {
                    vrc = RTDirReadEx(pDir, &Entry, NULL /*pcbDirEntry*/, RTFSOBJATTRADD_NOTHING, RTPATH_F_ON_LINK);
                    if (RT_FAILURE(vrc))
                    {
                        AssertLogRelMsg(vrc == VERR_NO_MORE_FILES, ("%Rrc\n", vrc));
                        break;
                    }
                    if (   RTFS_IS_DIRECTORY(Entry.Info.Attr.fMode)
                        && !RTStrICmp(Entry.szName, a_pszName))
                    {
                        /*
                         * The installed extension pack has a uses different case.
                         * Update the name and directory variables.
                         */
                        vrc = RTPathJoin(szDir, sizeof(szDir), m->strBasePath.c_str(), Entry.szName); /* not really necessary */
                        AssertLogRelRCReturnStmt(vrc, E_UNEXPECTED, RTDirClose(pDir));
                        a_pszName = Entry.szName;
                        fExists   = true;
                        break;
                    }
                }
                RTDirClose(pDir);
            }
        }
        if (fExists)
        {
            /*
             * We've got something, create a new extension pack object for it.
             */
            ComObjPtr<ExtPack> NewExtPack;
            hrc = NewExtPack.createObject();
            if (SUCCEEDED(hrc))
                hrc = NewExtPack->init(a_pszName, m->strBasePath.c_str());
            if (SUCCEEDED(hrc))
            {
                m->llInstalledExtPacks.push_back(NewExtPack);
                if (NewExtPack->m->fUsable)
                    LogRel(("ExtPackManager: Found extension pack '%s'.\n", a_pszName));
                else
                    LogRel(("ExtPackManager: Found bad extension pack '%s': %s\n",
                            a_pszName, NewExtPack->m->strWhyUnusable.c_str() ));
                pExtPack = NewExtPack;
            }
        }
        else
            hrc = S_OK;
    }

    /*
     * Report error if not usable, if that is desired.
     */
    if (   SUCCEEDED(hrc)
        && pExtPack
        && a_fUnsuableIsError
        && !pExtPack->m->fUsable)
        hrc = setError(E_FAIL, "%s", pExtPack->m->strWhyUnusable.c_str());

    if (a_ppExtPack)
        *a_ppExtPack = pExtPack;
    return hrc;
}



int ExtPackManager::callAllConfigHooks(IConsole *a_pConsole, PVM a_pVM)
{
    NOREF(a_pConsole); NOREF(a_pVM);
    return VINF_SUCCESS;
}

int ExtPackManager::callAllNewMachineHooks(IMachine *a_pMachine)
{
    NOREF(a_pMachine);
    return VINF_SUCCESS;
}


/* vi: set tabstop=4 shiftwidth=4 expandtab: */
