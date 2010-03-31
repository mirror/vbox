/* $Id$ */
/** @file
 *
 * IAppliance and IVirtualSystem COM class implementations.
 */

/*
 * Copyright (C) 2008-2010 Sun Microsystems, Inc.
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

#include <iprt/path.h>

#include <VBox/com/array.h>

#include "ApplianceImpl.h"
#include "VFSExplorerImpl.h"
#include "VirtualBoxImpl.h"
#include "GuestOSTypeImpl.h"
#include "ProgressImpl.h"
#include "MachineImpl.h"

#include "AutoCaller.h"
#include "Logging.h"

#include "ApplianceImplPrivate.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////
//
// Internal helpers
//
////////////////////////////////////////////////////////////////////////////////

static const struct
{
    CIMOSType_T     cim;
    const char      *pcszVbox;
}
g_osTypes[] =
{
    { CIMOSType_CIMOS_Unknown,                              SchemaDefs_OSTypeId_Other },
    { CIMOSType_CIMOS_OS2,                                  SchemaDefs_OSTypeId_OS2 },
    { CIMOSType_CIMOS_MSDOS,                                SchemaDefs_OSTypeId_DOS },
    { CIMOSType_CIMOS_WIN3x,                                SchemaDefs_OSTypeId_Windows31 },
    { CIMOSType_CIMOS_WIN95,                                SchemaDefs_OSTypeId_Windows95 },
    { CIMOSType_CIMOS_WIN98,                                SchemaDefs_OSTypeId_Windows98 },
    { CIMOSType_CIMOS_WINNT,                                SchemaDefs_OSTypeId_WindowsNT4 },
    { CIMOSType_CIMOS_NetWare,                              SchemaDefs_OSTypeId_Netware },
    { CIMOSType_CIMOS_NovellOES,                            SchemaDefs_OSTypeId_Netware },
    { CIMOSType_CIMOS_Solaris,                              SchemaDefs_OSTypeId_OpenSolaris },
    { CIMOSType_CIMOS_SunOS,                                SchemaDefs_OSTypeId_OpenSolaris },
    { CIMOSType_CIMOS_FreeBSD,                              SchemaDefs_OSTypeId_FreeBSD },
    { CIMOSType_CIMOS_NetBSD,                               SchemaDefs_OSTypeId_NetBSD },
    { CIMOSType_CIMOS_QNX,                                  SchemaDefs_OSTypeId_QNX },
    { CIMOSType_CIMOS_Windows2000,                          SchemaDefs_OSTypeId_Windows2000 },
    { CIMOSType_CIMOS_WindowsMe,                            SchemaDefs_OSTypeId_WindowsMe },
    { CIMOSType_CIMOS_OpenBSD,                              SchemaDefs_OSTypeId_OpenBSD },
    { CIMOSType_CIMOS_WindowsXP,                            SchemaDefs_OSTypeId_WindowsXP },
    { CIMOSType_CIMOS_WindowsXPEmbedded,                    SchemaDefs_OSTypeId_WindowsXP },
    { CIMOSType_CIMOS_WindowsEmbeddedforPointofService,     SchemaDefs_OSTypeId_WindowsXP },
    { CIMOSType_CIMOS_MicrosoftWindowsServer2003,           SchemaDefs_OSTypeId_Windows2003 },
    { CIMOSType_CIMOS_MicrosoftWindowsServer2003_64,        SchemaDefs_OSTypeId_Windows2003_64 },
    { CIMOSType_CIMOS_WindowsXP_64,                         SchemaDefs_OSTypeId_WindowsXP_64 },
    { CIMOSType_CIMOS_WindowsVista,                         SchemaDefs_OSTypeId_WindowsVista },
    { CIMOSType_CIMOS_WindowsVista_64,                      SchemaDefs_OSTypeId_WindowsVista_64 },
    { CIMOSType_CIMOS_MicrosoftWindowsServer2008,           SchemaDefs_OSTypeId_Windows2008 },
    { CIMOSType_CIMOS_MicrosoftWindowsServer2008_64,        SchemaDefs_OSTypeId_Windows2008_64 },
    { CIMOSType_CIMOS_FreeBSD_64,                           SchemaDefs_OSTypeId_FreeBSD_64 },
    { CIMOSType_CIMOS_RedHatEnterpriseLinux,                SchemaDefs_OSTypeId_RedHat },
    { CIMOSType_CIMOS_RedHatEnterpriseLinux_64,             SchemaDefs_OSTypeId_RedHat_64 },
    { CIMOSType_CIMOS_Solaris_64,                           SchemaDefs_OSTypeId_OpenSolaris_64 },
    { CIMOSType_CIMOS_SUSE,                                 SchemaDefs_OSTypeId_OpenSUSE },
    { CIMOSType_CIMOS_SLES,                                 SchemaDefs_OSTypeId_OpenSUSE },
    { CIMOSType_CIMOS_NovellLinuxDesktop,                   SchemaDefs_OSTypeId_OpenSUSE },
    { CIMOSType_CIMOS_SUSE_64,                              SchemaDefs_OSTypeId_OpenSUSE_64 },
    { CIMOSType_CIMOS_SLES_64,                              SchemaDefs_OSTypeId_OpenSUSE_64 },
    { CIMOSType_CIMOS_LINUX,                                SchemaDefs_OSTypeId_Linux },
    { CIMOSType_CIMOS_SunJavaDesktopSystem,                 SchemaDefs_OSTypeId_Linux },
    { CIMOSType_CIMOS_TurboLinux,                           SchemaDefs_OSTypeId_Linux},

    //                { CIMOSType_CIMOS_TurboLinux_64, },

    { CIMOSType_CIMOS_Mandriva,                             SchemaDefs_OSTypeId_Mandriva },
    { CIMOSType_CIMOS_Mandriva_64,                          SchemaDefs_OSTypeId_Mandriva_64 },
    { CIMOSType_CIMOS_Ubuntu,                               SchemaDefs_OSTypeId_Ubuntu },
    { CIMOSType_CIMOS_Ubuntu_64,                            SchemaDefs_OSTypeId_Ubuntu_64 },
    { CIMOSType_CIMOS_Debian,                               SchemaDefs_OSTypeId_Debian },
    { CIMOSType_CIMOS_Debian_64,                            SchemaDefs_OSTypeId_Debian_64 },
    { CIMOSType_CIMOS_Linux_2_4_x,                          SchemaDefs_OSTypeId_Linux24 },
    { CIMOSType_CIMOS_Linux_2_4_x_64,                       SchemaDefs_OSTypeId_Linux24_64 },
    { CIMOSType_CIMOS_Linux_2_6_x,                          SchemaDefs_OSTypeId_Linux26 },
    { CIMOSType_CIMOS_Linux_2_6_x_64,                       SchemaDefs_OSTypeId_Linux26_64 },
    { CIMOSType_CIMOS_Linux_64,                             SchemaDefs_OSTypeId_Linux26_64 }
};

/* Pattern structure for matching the OS type description field */
struct osTypePattern
{
    const char *pcszPattern;
    const char *pcszVbox;
};

/* These are the 32-Bit ones. They are sorted by priority. */
static const osTypePattern g_osTypesPattern[] =
{
    {"Windows NT",    SchemaDefs_OSTypeId_WindowsNT4},
    {"Windows XP",    SchemaDefs_OSTypeId_WindowsXP},
    {"Windows 2000",  SchemaDefs_OSTypeId_Windows2000},
    {"Windows 2003",  SchemaDefs_OSTypeId_Windows2003},
    {"Windows Vista", SchemaDefs_OSTypeId_WindowsVista},
    {"Windows 2008",  SchemaDefs_OSTypeId_Windows2008},
    {"SUSE",          SchemaDefs_OSTypeId_OpenSUSE},
    {"Novell",        SchemaDefs_OSTypeId_OpenSUSE},
    {"Red Hat",       SchemaDefs_OSTypeId_RedHat},
    {"Mandriva",      SchemaDefs_OSTypeId_Mandriva},
    {"Ubuntu",        SchemaDefs_OSTypeId_Ubuntu},
    {"Debian",        SchemaDefs_OSTypeId_Debian},
    {"QNX",           SchemaDefs_OSTypeId_QNX},
    {"Linux 2.4",     SchemaDefs_OSTypeId_Linux24},
    {"Linux 2.6",     SchemaDefs_OSTypeId_Linux26},
    {"Linux",         SchemaDefs_OSTypeId_Linux},
    {"OpenSolaris",   SchemaDefs_OSTypeId_OpenSolaris},
    {"Solaris",       SchemaDefs_OSTypeId_OpenSolaris},
    {"FreeBSD",       SchemaDefs_OSTypeId_FreeBSD},
    {"NetBSD",        SchemaDefs_OSTypeId_NetBSD},
    {"Windows 95",    SchemaDefs_OSTypeId_Windows95},
    {"Windows 98",    SchemaDefs_OSTypeId_Windows98},
    {"Windows Me",    SchemaDefs_OSTypeId_WindowsMe},
    {"Windows 3.",    SchemaDefs_OSTypeId_Windows31},
    {"DOS",           SchemaDefs_OSTypeId_DOS},
    {"OS2",           SchemaDefs_OSTypeId_OS2}
};

/* These are the 64-Bit ones. They are sorted by priority. */
static const osTypePattern g_osTypesPattern64[] =
{
    {"Windows XP",    SchemaDefs_OSTypeId_WindowsXP_64},
    {"Windows 2003",  SchemaDefs_OSTypeId_Windows2003_64},
    {"Windows Vista", SchemaDefs_OSTypeId_WindowsVista_64},
    {"Windows 2008",  SchemaDefs_OSTypeId_Windows2008_64},
    {"SUSE",          SchemaDefs_OSTypeId_OpenSUSE_64},
    {"Novell",        SchemaDefs_OSTypeId_OpenSUSE_64},
    {"Red Hat",       SchemaDefs_OSTypeId_RedHat_64},
    {"Mandriva",      SchemaDefs_OSTypeId_Mandriva_64},
    {"Ubuntu",        SchemaDefs_OSTypeId_Ubuntu_64},
    {"Debian",        SchemaDefs_OSTypeId_Debian_64},
    {"Linux 2.4",     SchemaDefs_OSTypeId_Linux24_64},
    {"Linux 2.6",     SchemaDefs_OSTypeId_Linux26_64},
    {"Linux",         SchemaDefs_OSTypeId_Linux26_64},
    {"OpenSolaris",   SchemaDefs_OSTypeId_OpenSolaris_64},
    {"Solaris",       SchemaDefs_OSTypeId_OpenSolaris_64},
    {"FreeBSD",       SchemaDefs_OSTypeId_FreeBSD_64},
};

/**
 * Private helper func that suggests a VirtualBox guest OS type
 * for the given OVF operating system type.
 * @param osTypeVBox
 * @param c
 * @param cStr
 */
void convertCIMOSType2VBoxOSType(Utf8Str &strType, CIMOSType_T c, const Utf8Str &cStr)
{
    /* First check if the type is other/other_64 */
    if (c == CIMOSType_CIMOS_Other)
    {
        for (size_t i=0; i < RT_ELEMENTS(g_osTypesPattern); ++i)
            if (cStr.contains (g_osTypesPattern[i].pcszPattern, Utf8Str::CaseInsensitive))
            {
                strType = g_osTypesPattern[i].pcszVbox;
                return;
            }
    }
    else if (c == CIMOSType_CIMOS_Other_64)
    {
        for (size_t i=0; i < RT_ELEMENTS(g_osTypesPattern64); ++i)
            if (cStr.contains (g_osTypesPattern64[i].pcszPattern, Utf8Str::CaseInsensitive))
            {
                strType = g_osTypesPattern64[i].pcszVbox;
                return;
            }
    }

    for (size_t i = 0; i < RT_ELEMENTS(g_osTypes); ++i)
    {
        if (c == g_osTypes[i].cim)
        {
            strType = g_osTypes[i].pcszVbox;
            return;
        }
    }

    strType = SchemaDefs_OSTypeId_Other;
}

/**
 * Private helper func that suggests a VirtualBox guest OS type
 * for the given OVF operating system type.
 * @param osTypeVBox
 * @param c
 */
CIMOSType_T convertVBoxOSType2CIMOSType(const char *pcszVbox)
{
    for (size_t i = 0; i < RT_ELEMENTS(g_osTypes); ++i)
    {
        if (!RTStrICmp(pcszVbox, g_osTypes[i].pcszVbox))
            return g_osTypes[i].cim;
    }

    return CIMOSType_CIMOS_Other;
}

////////////////////////////////////////////////////////////////////////////////
//
// IVirtualBox public methods
//
////////////////////////////////////////////////////////////////////////////////

// This code is here so we won't have to include the appliance headers in the
// IVirtualBox implementation.

/**
 * Implementation for IVirtualBox::createAppliance.
 *
 * @param anAppliance IAppliance object created if S_OK is returned.
 * @return S_OK or error.
 */
STDMETHODIMP VirtualBox::CreateAppliance(IAppliance** anAppliance)
{
    HRESULT rc;

    ComObjPtr<Appliance> appliance;
    appliance.createObject();
    rc = appliance->init(this);

    if (SUCCEEDED(rc))
        appliance.queryInterfaceTo(anAppliance);

    return rc;
}

////////////////////////////////////////////////////////////////////////////////
//
// Appliance constructor / destructor
//
////////////////////////////////////////////////////////////////////////////////

Appliance::Appliance()
    : mVirtualBox(NULL)
{
}

Appliance::~Appliance()
{
}

/**
 * Appliance COM initializer.
 * @param
 * @return
 */
HRESULT Appliance::init(VirtualBox *aVirtualBox)
{
    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    /* Weak reference to a VirtualBox object */
    unconst(mVirtualBox) = aVirtualBox;

    // initialize data
    m = new Data;

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 * Appliance COM uninitializer.
 * @return
 */
void Appliance::uninit()
{
    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    delete m;
    m = NULL;
}

////////////////////////////////////////////////////////////////////////////////
//
// IAppliance public methods
//
////////////////////////////////////////////////////////////////////////////////

/**
 * Public method implementation.
 * @param
 * @return
 */
STDMETHODIMP Appliance::COMGETTER(Path)(BSTR *aPath)
{
    if (!aPath)
        return E_POINTER;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (!isApplianceIdle())
        return E_ACCESSDENIED;

    Bstr bstrPath(m->locInfo.strPath);
    bstrPath.cloneTo(aPath);

    return S_OK;
}

/**
 * Public method implementation.
 * @param
 * @return
 */
STDMETHODIMP Appliance::COMGETTER(Disks)(ComSafeArrayOut(BSTR, aDisks))
{
    CheckComArgOutSafeArrayPointerValid(aDisks);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (!isApplianceIdle())
        return E_ACCESSDENIED;

    if (m->pReader) // OVFReader instantiated?
    {
        size_t c = m->pReader->m_mapDisks.size();
        com::SafeArray<BSTR> sfaDisks(c);

        DiskImagesMap::const_iterator it;
        size_t i = 0;
        for (it = m->pReader->m_mapDisks.begin();
             it != m->pReader->m_mapDisks.end();
             ++it, ++i)
        {
            // create a string representing this disk
            const DiskImage &d = it->second;
            char *psz = NULL;
            RTStrAPrintf(&psz,
                         "%s\t"
                         "%RI64\t"
                         "%RI64\t"
                         "%s\t"
                         "%s\t"
                         "%RI64\t"
                         "%RI64\t"
                         "%s",
                         d.strDiskId.c_str(),
                         d.iCapacity,
                         d.iPopulatedSize,
                         d.strFormat.c_str(),
                         d.strHref.c_str(),
                         d.iSize,
                         d.iChunkSize,
                         d.strCompression.c_str());
            Utf8Str utf(psz);
            Bstr bstr(utf);
            // push to safearray
            bstr.cloneTo(&sfaDisks[i]);
            RTStrFree(psz);
        }

        sfaDisks.detachTo(ComSafeArrayOutArg(aDisks));
    }

    return S_OK;
}

/**
 * Public method implementation.
 * @param
 * @return
 */
STDMETHODIMP Appliance::COMGETTER(VirtualSystemDescriptions)(ComSafeArrayOut(IVirtualSystemDescription*, aVirtualSystemDescriptions))
{
    CheckComArgOutSafeArrayPointerValid(aVirtualSystemDescriptions);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (!isApplianceIdle())
        return E_ACCESSDENIED;

    SafeIfaceArray<IVirtualSystemDescription> sfaVSD(m->virtualSystemDescriptions);
    sfaVSD.detachTo(ComSafeArrayOutArg(aVirtualSystemDescriptions));

    return S_OK;
}

STDMETHODIMP Appliance::CreateVFSExplorer(IN_BSTR aURI, IVFSExplorer **aExplorer)
{
    CheckComArgOutPointerValid(aExplorer);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    ComObjPtr<VFSExplorer> explorer;
    HRESULT rc = S_OK;
    try
    {
        Utf8Str uri(aURI);
        /* Check which kind of export the user has requested */
        LocationInfo li;
        parseURI(uri, li);
        /* Create the explorer object */
        explorer.createObject();
        rc = explorer->init(li.storageType, li.strPath, li.strHostname, li.strUsername, li.strPassword, mVirtualBox);
    }
    catch (HRESULT aRC)
    {
        rc = aRC;
    }

    if (SUCCEEDED(rc))
        /* Return explorer to the caller */
        explorer.queryInterfaceTo(aExplorer);

    return rc;
}

/**
* Public method implementation.
 * @return
 */
STDMETHODIMP Appliance::GetWarnings(ComSafeArrayOut(BSTR, aWarnings))
{
    if (ComSafeArrayOutIsNull(aWarnings))
        return E_POINTER;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    com::SafeArray<BSTR> sfaWarnings(m->llWarnings.size());

    list<Utf8Str>::const_iterator it;
    size_t i = 0;
    for (it = m->llWarnings.begin();
         it != m->llWarnings.end();
         ++it, ++i)
    {
        Bstr bstr = *it;
        bstr.cloneTo(&sfaWarnings[i]);
    }

    sfaWarnings.detachTo(ComSafeArrayOutArg(aWarnings));

    return S_OK;
}

////////////////////////////////////////////////////////////////////////////////
//
// Appliance private methods
//
////////////////////////////////////////////////////////////////////////////////

/**
 * Returns true if the appliance is in "idle" state. This should always be the
 * case unless an import or export is currently in progress. Similar to machine
 * states, this permits the Appliance implementation code to let go of the
 * Appliance object lock while a time-consuming disk conversion is in progress
 * without exposing the appliance to conflicting calls.
 *
 * This sets an error on "this" (the appliance) and returns false if the appliance
 * is busy. The caller should then return E_ACCESSDENIED.
 *
 * Must be called from under the object lock!
 *
 * @return
 */
bool Appliance::isApplianceIdle() const
{
    if (m->state == Data::ApplianceImporting)
        setError(VBOX_E_INVALID_OBJECT_STATE, "The appliance is busy importing files");
    else if (m->state == Data::ApplianceExporting)
        setError(VBOX_E_INVALID_OBJECT_STATE, "The appliance is busy exporting files");
    else
        return true;

    return false;
}

HRESULT Appliance::searchUniqueVMName(Utf8Str& aName) const
{
    IMachine *machine = NULL;
    char *tmpName = RTStrDup(aName.c_str());
    int i = 1;
    /* @todo: Maybe too cost-intensive; try to find a lighter way */
    while (mVirtualBox->FindMachine(Bstr(tmpName), &machine) != VBOX_E_OBJECT_NOT_FOUND)
    {
        RTStrFree(tmpName);
        RTStrAPrintf(&tmpName, "%s_%d", aName.c_str(), i);
        ++i;
    }
    aName = tmpName;
    RTStrFree(tmpName);

    return S_OK;
}

HRESULT Appliance::searchUniqueDiskImageFilePath(Utf8Str& aName) const
{
    IMedium *harddisk = NULL;
    char *tmpName = RTStrDup(aName.c_str());
    int i = 1;
    /* Check if the file exists or if a file with this path is registered
     * already */
    /* @todo: Maybe too cost-intensive; try to find a lighter way */
    while (    RTPathExists(tmpName)
            || mVirtualBox->FindHardDisk(Bstr(tmpName), &harddisk) != VBOX_E_OBJECT_NOT_FOUND
          )
    {
        RTStrFree(tmpName);
        char *tmpDir = RTStrDup(aName.c_str());
        RTPathStripFilename(tmpDir);;
        char *tmpFile = RTStrDup(RTPathFilename(aName.c_str()));
        RTPathStripExt(tmpFile);
        const char *tmpExt = RTPathExt(aName.c_str());
        RTStrAPrintf(&tmpName, "%s%c%s_%d%s", tmpDir, RTPATH_DELIMITER, tmpFile, i, tmpExt);
        RTStrFree(tmpFile);
        RTStrFree(tmpDir);
        ++i;
    }
    aName = tmpName;
    RTStrFree(tmpName);

    return S_OK;
}

/**
 * Called from the import and export background threads to synchronize the second
 * background disk thread's progress object with the current progress object so
 * that the user interface sees progress correctly and that cancel signals are
 * passed on to the second thread.
 * @param pProgressThis Progress object of the current thread.
 * @param pProgressAsync Progress object of asynchronous task running in background.
 */
void Appliance::waitForAsyncProgress(ComObjPtr<Progress> &pProgressThis,
                                     ComPtr<IProgress> &pProgressAsync)
{
    HRESULT rc;

    // now loop until the asynchronous operation completes and then report its result
    BOOL fCompleted;
    BOOL fCanceled;
    ULONG currentPercent;
    while (SUCCEEDED(pProgressAsync->COMGETTER(Completed(&fCompleted))))
    {
        rc = pProgressThis->COMGETTER(Canceled)(&fCanceled);
        if (FAILED(rc)) throw rc;
        if (fCanceled)
        {
            pProgressAsync->Cancel();
            break;
        }

        rc = pProgressAsync->COMGETTER(Percent(&currentPercent));
        if (FAILED(rc)) throw rc;
        if (!pProgressThis.isNull())
            pProgressThis->SetCurrentOperationProgress(currentPercent);
        if (fCompleted)
            break;

        /* Make sure the loop is not too tight */
        rc = pProgressAsync->WaitForCompletion(100);
        if (FAILED(rc)) throw rc;
    }
    // report result of asynchronous operation
    LONG iRc;
    rc = pProgressAsync->COMGETTER(ResultCode)(&iRc);
    if (FAILED(rc)) throw rc;


    // if the thread of the progress object has an error, then
    // retrieve the error info from there, or it'll be lost
    if (FAILED(iRc))
    {
        ProgressErrorInfo info(pProgressAsync);
        Utf8Str str(info.getText());
        const char *pcsz = str.c_str();
        HRESULT rc2 = setError(iRc, pcsz);
        throw rc2;
    }
}

void Appliance::addWarning(const char* aWarning, ...)
{
    va_list args;
    va_start(args, aWarning);
    Utf8StrFmtVA str(aWarning, args);
    va_end(args);
    m->llWarnings.push_back(str);
}

void Appliance::disksWeight(uint32_t &ulTotalMB, uint32_t &cDisks) const
{
    ulTotalMB = 0;
    cDisks = 0;
    /* Weigh the disk images according to their sizes */
    list< ComObjPtr<VirtualSystemDescription> >::const_iterator it;
    for (it = m->virtualSystemDescriptions.begin();
         it != m->virtualSystemDescriptions.end();
         ++it)
    {
        ComObjPtr<VirtualSystemDescription> vsdescThis = (*it);
        /* One for every hard disk of the Virtual System */
        std::list<VirtualSystemDescriptionEntry*> avsdeHDs = vsdescThis->findByType(VirtualSystemDescriptionType_HardDiskImage);
        std::list<VirtualSystemDescriptionEntry*>::const_iterator itH;
        for (itH = avsdeHDs.begin();
             itH != avsdeHDs.end();
             ++itH)
        {
            const VirtualSystemDescriptionEntry *pHD = *itH;
            ulTotalMB += pHD->ulSizeMB;
            ++cDisks;
        }
    }

}

HRESULT Appliance::setUpProgressFS(ComObjPtr<Progress> &pProgress, const Bstr &bstrDescription)
{
    HRESULT rc;

    /* Create the progress object */
    pProgress.createObject();

    /* Weigh the disk images according to their sizes */
    uint32_t ulTotalMB;
    uint32_t cDisks;
    disksWeight(ulTotalMB, cDisks);

    ULONG cOperations = 1 + cDisks;     // one op per disk plus 1 for the XML

    ULONG ulTotalOperationsWeight;
    if (ulTotalMB)
    {
        m->ulWeightPerOperation = (ULONG)((double)ulTotalMB * 1  / 100);    // use 1% of the progress for the XML
        ulTotalOperationsWeight = ulTotalMB + m->ulWeightPerOperation;
    }
    else
    {
        // no disks to export:
        ulTotalOperationsWeight = 1;
        m->ulWeightPerOperation = 1;
    }

    Log(("Setting up progress object: ulTotalMB = %d, cDisks = %d, => cOperations = %d, ulTotalOperationsWeight = %d, m->ulWeightPerOperation = %d\n",
         ulTotalMB, cDisks, cOperations, ulTotalOperationsWeight, m->ulWeightPerOperation));

    rc = pProgress->init(mVirtualBox, static_cast<IAppliance*>(this),
                         bstrDescription,
                         TRUE /* aCancelable */,
                         cOperations, // ULONG cOperations,
                         ulTotalOperationsWeight, // ULONG ulTotalOperationsWeight,
                         bstrDescription, // CBSTR bstrFirstOperationDescription,
                         m->ulWeightPerOperation); // ULONG ulFirstOperationWeight,
    return rc;
}

HRESULT Appliance::setUpProgressImportS3(ComObjPtr<Progress> &pProgress, const Bstr &bstrDescription)
{
    HRESULT rc;

    /* Create the progress object */
    pProgress.createObject();

    /* Weigh the disk images according to their sizes */
    uint32_t ulTotalMB;
    uint32_t cDisks;
    disksWeight(ulTotalMB, cDisks);

    ULONG cOperations = 1 + 1 + 1 + cDisks;     // one op per disk plus 1 for init, plus 1 for the manifest file & 1 plus for the import */

    ULONG ulTotalOperationsWeight = ulTotalMB;
    if (!ulTotalOperationsWeight)
        // no disks to export:
        ulTotalOperationsWeight = 1;

    ULONG ulImportWeight = (ULONG)((double)ulTotalOperationsWeight * 50  / 100);  // use 50% for import
    ulTotalOperationsWeight += ulImportWeight;

    m->ulWeightPerOperation = ulImportWeight; /* save for using later */

    ULONG ulInitWeight = (ULONG)((double)ulTotalOperationsWeight * 0.1  / 100);  // use 0.1% for init
    ulTotalOperationsWeight += ulInitWeight;

    Log(("Setting up progress object: ulTotalMB = %d, cDisks = %d, => cOperations = %d, ulTotalOperationsWeight = %d, m->ulWeightPerOperation = %d\n",
         ulTotalMB, cDisks, cOperations, ulTotalOperationsWeight, m->ulWeightPerOperation));

    rc = pProgress->init(mVirtualBox, static_cast<IAppliance*>(this),
                         bstrDescription,
                         TRUE /* aCancelable */,
                         cOperations, // ULONG cOperations,
                         ulTotalOperationsWeight, // ULONG ulTotalOperationsWeight,
                         Bstr(tr("Init")), // CBSTR bstrFirstOperationDescription,
                         ulInitWeight); // ULONG ulFirstOperationWeight,
    return rc;
}

HRESULT Appliance::setUpProgressWriteS3(ComObjPtr<Progress> &pProgress, const Bstr &bstrDescription)
{
    HRESULT rc;

    /* Create the progress object */
    pProgress.createObject();

    /* Weigh the disk images according to their sizes */
    uint32_t ulTotalMB;
    uint32_t cDisks;
    disksWeight(ulTotalMB, cDisks);

    ULONG cOperations = 1 + 1 + 1 + cDisks;     // one op per disk plus 1 for the OVF, plus 1 for the mf & 1 plus to the temporary creation */

    ULONG ulTotalOperationsWeight;
    if (ulTotalMB)
    {
        m->ulWeightPerOperation = (ULONG)((double)ulTotalMB * 1  / 100);    // use 1% of the progress for OVF file upload (we didn't know the size at this point)
        ulTotalOperationsWeight = ulTotalMB + m->ulWeightPerOperation;
    }
    else
    {
        // no disks to export:
        ulTotalOperationsWeight = 1;
        m->ulWeightPerOperation = 1;
    }
    ULONG ulOVFCreationWeight = (ULONG)((double)ulTotalOperationsWeight * 50.0 / 100.0); /* Use 50% for the creation of the OVF & the disks */
    ulTotalOperationsWeight += ulOVFCreationWeight;

    Log(("Setting up progress object: ulTotalMB = %d, cDisks = %d, => cOperations = %d, ulTotalOperationsWeight = %d, m->ulWeightPerOperation = %d\n",
         ulTotalMB, cDisks, cOperations, ulTotalOperationsWeight, m->ulWeightPerOperation));

    rc = pProgress->init(mVirtualBox, static_cast<IAppliance*>(this),
                         bstrDescription,
                         TRUE /* aCancelable */,
                         cOperations, // ULONG cOperations,
                         ulTotalOperationsWeight, // ULONG ulTotalOperationsWeight,
                         bstrDescription, // CBSTR bstrFirstOperationDescription,
                         ulOVFCreationWeight); // ULONG ulFirstOperationWeight,
    return rc;
}

void Appliance::parseURI(Utf8Str strUri, LocationInfo &locInfo) const
{
    /* Check the URI for the protocol */
    if (strUri.startsWith("file://", Utf8Str::CaseInsensitive)) /* File based */
    {
        locInfo.storageType = VFSType_File;
        strUri = strUri.substr(sizeof("file://") - 1);
    }
    else if (strUri.startsWith("SunCloud://", Utf8Str::CaseInsensitive)) /* Sun Cloud service */
    {
        locInfo.storageType = VFSType_S3;
        strUri = strUri.substr(sizeof("SunCloud://") - 1);
    }
    else if (strUri.startsWith("S3://", Utf8Str::CaseInsensitive)) /* S3 service */
    {
        locInfo.storageType = VFSType_S3;
        strUri = strUri.substr(sizeof("S3://") - 1);
    }
    else if (strUri.startsWith("webdav://", Utf8Str::CaseInsensitive)) /* webdav service */
        throw E_NOTIMPL;

    /* Not necessary on a file based URI */
    if (locInfo.storageType != VFSType_File)
    {
        size_t uppos = strUri.find("@"); /* username:password combo */
        if (uppos != Utf8Str::npos)
        {
            locInfo.strUsername = strUri.substr(0, uppos);
            strUri = strUri.substr(uppos + 1);
            size_t upos = locInfo.strUsername.find(":");
            if (upos != Utf8Str::npos)
            {
                locInfo.strPassword = locInfo.strUsername.substr(upos + 1);
                locInfo.strUsername = locInfo.strUsername.substr(0, upos);
            }
        }
        size_t hpos = strUri.find("/"); /* hostname part */
        if (hpos != Utf8Str::npos)
        {
            locInfo.strHostname = strUri.substr(0, hpos);
            strUri = strUri.substr(hpos);
        }
    }

    locInfo.strPath = strUri;
}

void Appliance::parseBucket(Utf8Str &aPath, Utf8Str &aBucket) const
{
    /* Buckets are S3 specific. So parse the bucket out of the file path */
    if (!aPath.startsWith("/"))
        throw setError(E_INVALIDARG,
                       tr("The path '%s' must start with /"), aPath.c_str());
    size_t bpos = aPath.find("/", 1);
    if (bpos != Utf8Str::npos)
    {
        aBucket = aPath.substr(1, bpos - 1); /* The bucket without any slashes */
        aPath = aPath.substr(bpos); /* The rest of the file path */
    }
    /* If there is no bucket name provided reject it */
    if (aBucket.isEmpty())
        throw setError(E_INVALIDARG,
                       tr("You doesn't provide a bucket name in the URI '%s'"), aPath.c_str());
}

Utf8Str Appliance::manifestFileName(Utf8Str aPath) const
{
    /* Get the name part */
    char *pszMfName = RTStrDup(RTPathFilename(aPath.c_str()));
    /* Strip any extensions */
    RTPathStripExt(pszMfName);
    /* Path without the filename */
    aPath.stripFilename();
    /* Format the manifest path */
    Utf8StrFmt strMfFile("%s/%s.mf", aPath.c_str(), pszMfName);
    RTStrFree(pszMfName);
    return strMfFile;
}

/**
 *
 * @return
 */
int Appliance::TaskOVF::startThread()
{
    int vrc = RTThreadCreate(NULL, Appliance::taskThreadImportOrExport, this,
                             0, RTTHREADTYPE_MAIN_HEAVY_WORKER, 0,
                             "Appliance::Task");

    ComAssertMsgRCRet(vrc,
                      ("Could not create OVF task thread (%Rrc)\n", vrc), E_FAIL);

    return S_OK;
}

/**
 * Thread function for the thread started in Appliance::readImpl() and Appliance::importImpl()
 * and Appliance::writeImpl().
 * This will in turn call Appliance::readFS() or Appliance::readS3() or Appliance::importFS()
 * or Appliance::importS3() or Appliance::writeFS() or Appliance::writeS3().
 *
 * @param aThread
 * @param pvUser
 */
/* static */
DECLCALLBACK(int) Appliance::taskThreadImportOrExport(RTTHREAD /* aThread */, void *pvUser)
{
    std::auto_ptr<TaskOVF> task(static_cast<TaskOVF*>(pvUser));
    AssertReturn(task.get(), VERR_GENERAL_FAILURE);

    Appliance *pAppliance = task->pAppliance;

    LogFlowFuncEnter();
    LogFlowFunc(("Appliance %p\n", pAppliance));

    HRESULT taskrc = S_OK;

    switch (task->taskType)
    {
        case TaskOVF::Read:
            if (task->locInfo.storageType == VFSType_File)
                taskrc = pAppliance->readFS(task->locInfo);
            else if (task->locInfo.storageType == VFSType_S3)
                taskrc = pAppliance->readS3(task.get());
        break;

        case TaskOVF::Import:
            if (task->locInfo.storageType == VFSType_File)
                taskrc = pAppliance->importFS(task->locInfo, task->pProgress);
            else if (task->locInfo.storageType == VFSType_S3)
                taskrc = pAppliance->importS3(task.get());
        break;

        case TaskOVF::Write:
            if (task->locInfo.storageType == VFSType_File)
                taskrc = pAppliance->writeFS(task->locInfo, task->enFormat, task->pProgress);
            else if (task->locInfo.storageType == VFSType_S3)
                taskrc = pAppliance->writeS3(task.get());
        break;
    }

    task->rc = taskrc;

    if (!task->pProgress.isNull())
        task->pProgress->notifyComplete(taskrc);

    LogFlowFuncLeave();

    return VINF_SUCCESS;
}

/* static */
int Appliance::TaskOVF::updateProgress(unsigned uPercent, void *pvUser)
{
    Appliance::TaskOVF* pTask = *(Appliance::TaskOVF**)pvUser;

    if (    pTask
         && !pTask->pProgress.isNull())
    {
        BOOL fCanceled;
        pTask->pProgress->COMGETTER(Canceled)(&fCanceled);
        if (fCanceled)
            return -1;
        pTask->pProgress->SetCurrentOperationProgress(uPercent);
    }
    return VINF_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////
//
// IVirtualSystemDescription constructor / destructor
//
////////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR(VirtualSystemDescription)

/**
 * COM initializer.
 * @return
 */
HRESULT VirtualSystemDescription::init()
{
    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    /* Initialize data */
    m = new Data();

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();
    return S_OK;
}

/**
* COM uninitializer.
*/

void VirtualSystemDescription::uninit()
{
    delete m;
    m = NULL;
}

////////////////////////////////////////////////////////////////////////////////
//
// IVirtualSystemDescription public methods
//
////////////////////////////////////////////////////////////////////////////////

/**
 * Public method implementation.
 * @param
 * @return
 */
STDMETHODIMP VirtualSystemDescription::COMGETTER(Count)(ULONG *aCount)
{
    if (!aCount)
        return E_POINTER;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aCount = (ULONG)m->llDescriptions.size();

    return S_OK;
}

/**
 * Public method implementation.
 * @return
 */
STDMETHODIMP VirtualSystemDescription::GetDescription(ComSafeArrayOut(VirtualSystemDescriptionType_T, aTypes),
                                                      ComSafeArrayOut(BSTR, aRefs),
                                                      ComSafeArrayOut(BSTR, aOrigValues),
                                                      ComSafeArrayOut(BSTR, aVboxValues),
                                                      ComSafeArrayOut(BSTR, aExtraConfigValues))
{
    if (ComSafeArrayOutIsNull(aTypes) ||
        ComSafeArrayOutIsNull(aRefs) ||
        ComSafeArrayOutIsNull(aOrigValues) ||
        ComSafeArrayOutIsNull(aVboxValues) ||
        ComSafeArrayOutIsNull(aExtraConfigValues))
        return E_POINTER;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    ULONG c = (ULONG)m->llDescriptions.size();
    com::SafeArray<VirtualSystemDescriptionType_T> sfaTypes(c);
    com::SafeArray<BSTR> sfaRefs(c);
    com::SafeArray<BSTR> sfaOrigValues(c);
    com::SafeArray<BSTR> sfaVboxValues(c);
    com::SafeArray<BSTR> sfaExtraConfigValues(c);

    list<VirtualSystemDescriptionEntry>::const_iterator it;
    size_t i = 0;
    for (it = m->llDescriptions.begin();
         it != m->llDescriptions.end();
         ++it, ++i)
    {
        const VirtualSystemDescriptionEntry &vsde = (*it);

        sfaTypes[i] = vsde.type;

        Bstr bstr = vsde.strRef;
        bstr.cloneTo(&sfaRefs[i]);

        bstr = vsde.strOvf;
        bstr.cloneTo(&sfaOrigValues[i]);

        bstr = vsde.strVbox;
        bstr.cloneTo(&sfaVboxValues[i]);

        bstr = vsde.strExtraConfig;
        bstr.cloneTo(&sfaExtraConfigValues[i]);
    }

    sfaTypes.detachTo(ComSafeArrayOutArg(aTypes));
    sfaRefs.detachTo(ComSafeArrayOutArg(aRefs));
    sfaOrigValues.detachTo(ComSafeArrayOutArg(aOrigValues));
    sfaVboxValues.detachTo(ComSafeArrayOutArg(aVboxValues));
    sfaExtraConfigValues.detachTo(ComSafeArrayOutArg(aExtraConfigValues));

    return S_OK;
}

/**
 * Public method implementation.
 * @return
 */
STDMETHODIMP VirtualSystemDescription::GetDescriptionByType(VirtualSystemDescriptionType_T aType,
                                                            ComSafeArrayOut(VirtualSystemDescriptionType_T, aTypes),
                                                            ComSafeArrayOut(BSTR, aRefs),
                                                            ComSafeArrayOut(BSTR, aOrigValues),
                                                            ComSafeArrayOut(BSTR, aVboxValues),
                                                            ComSafeArrayOut(BSTR, aExtraConfigValues))
{
    if (ComSafeArrayOutIsNull(aTypes) ||
        ComSafeArrayOutIsNull(aRefs) ||
        ComSafeArrayOutIsNull(aOrigValues) ||
        ComSafeArrayOutIsNull(aVboxValues) ||
        ComSafeArrayOutIsNull(aExtraConfigValues))
        return E_POINTER;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    std::list<VirtualSystemDescriptionEntry*> vsd = findByType (aType);
    ULONG c = (ULONG)vsd.size();
    com::SafeArray<VirtualSystemDescriptionType_T> sfaTypes(c);
    com::SafeArray<BSTR> sfaRefs(c);
    com::SafeArray<BSTR> sfaOrigValues(c);
    com::SafeArray<BSTR> sfaVboxValues(c);
    com::SafeArray<BSTR> sfaExtraConfigValues(c);

    list<VirtualSystemDescriptionEntry*>::const_iterator it;
    size_t i = 0;
    for (it = vsd.begin();
         it != vsd.end();
         ++it, ++i)
    {
        const VirtualSystemDescriptionEntry *vsde = (*it);

        sfaTypes[i] = vsde->type;

        Bstr bstr = vsde->strRef;
        bstr.cloneTo(&sfaRefs[i]);

        bstr = vsde->strOvf;
        bstr.cloneTo(&sfaOrigValues[i]);

        bstr = vsde->strVbox;
        bstr.cloneTo(&sfaVboxValues[i]);

        bstr = vsde->strExtraConfig;
        bstr.cloneTo(&sfaExtraConfigValues[i]);
    }

    sfaTypes.detachTo(ComSafeArrayOutArg(aTypes));
    sfaRefs.detachTo(ComSafeArrayOutArg(aRefs));
    sfaOrigValues.detachTo(ComSafeArrayOutArg(aOrigValues));
    sfaVboxValues.detachTo(ComSafeArrayOutArg(aVboxValues));
    sfaExtraConfigValues.detachTo(ComSafeArrayOutArg(aExtraConfigValues));

    return S_OK;
}

/**
 * Public method implementation.
 * @return
 */
STDMETHODIMP VirtualSystemDescription::GetValuesByType(VirtualSystemDescriptionType_T aType,
                                                       VirtualSystemDescriptionValueType_T aWhich,
                                                       ComSafeArrayOut(BSTR, aValues))
{
    if (ComSafeArrayOutIsNull(aValues))
        return E_POINTER;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    std::list<VirtualSystemDescriptionEntry*> vsd = findByType (aType);
    com::SafeArray<BSTR> sfaValues((ULONG)vsd.size());

    list<VirtualSystemDescriptionEntry*>::const_iterator it;
    size_t i = 0;
    for (it = vsd.begin();
         it != vsd.end();
         ++it, ++i)
    {
        const VirtualSystemDescriptionEntry *vsde = (*it);

        Bstr bstr;
        switch (aWhich)
        {
            case VirtualSystemDescriptionValueType_Reference: bstr = vsde->strRef; break;
            case VirtualSystemDescriptionValueType_Original: bstr = vsde->strOvf; break;
            case VirtualSystemDescriptionValueType_Auto: bstr = vsde->strVbox; break;
            case VirtualSystemDescriptionValueType_ExtraConfig: bstr = vsde->strExtraConfig; break;
        }

        bstr.cloneTo(&sfaValues[i]);
    }

    sfaValues.detachTo(ComSafeArrayOutArg(aValues));

    return S_OK;
}

/**
 * Public method implementation.
 * @return
 */
STDMETHODIMP VirtualSystemDescription::SetFinalValues(ComSafeArrayIn(BOOL, aEnabled),
                                                      ComSafeArrayIn(IN_BSTR, argVboxValues),
                                                      ComSafeArrayIn(IN_BSTR, argExtraConfigValues))
{
#ifndef RT_OS_WINDOWS
    NOREF(aEnabledSize);
#endif /* RT_OS_WINDOWS */

    CheckComArgSafeArrayNotNull(aEnabled);
    CheckComArgSafeArrayNotNull(argVboxValues);
    CheckComArgSafeArrayNotNull(argExtraConfigValues);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    com::SafeArray<BOOL> sfaEnabled(ComSafeArrayInArg(aEnabled));
    com::SafeArray<IN_BSTR> sfaVboxValues(ComSafeArrayInArg(argVboxValues));
    com::SafeArray<IN_BSTR> sfaExtraConfigValues(ComSafeArrayInArg(argExtraConfigValues));

    if (    (sfaEnabled.size() != m->llDescriptions.size())
         || (sfaVboxValues.size() != m->llDescriptions.size())
         || (sfaExtraConfigValues.size() != m->llDescriptions.size())
       )
        return E_INVALIDARG;

    list<VirtualSystemDescriptionEntry>::iterator it;
    size_t i = 0;
    for (it = m->llDescriptions.begin();
         it != m->llDescriptions.end();
         ++it, ++i)
    {
        VirtualSystemDescriptionEntry& vsde = *it;

        if (sfaEnabled[i])
        {
            vsde.strVbox = sfaVboxValues[i];
            vsde.strExtraConfig = sfaExtraConfigValues[i];
        }
        else
            vsde.type = VirtualSystemDescriptionType_Ignore;
    }

    return S_OK;
}

/**
 * Public method implementation.
 * @return
 */
STDMETHODIMP VirtualSystemDescription::AddDescription(VirtualSystemDescriptionType_T aType,
                                                      IN_BSTR aVboxValue,
                                                      IN_BSTR aExtraConfigValue)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    addEntry(aType, "", aVboxValue, aVboxValue, 0, aExtraConfigValue);

    return S_OK;
}

/**
 * Internal method; adds a new description item to the member list.
 * @param aType Type of description for the new item.
 * @param strRef Reference item; only used with hard disk controllers.
 * @param aOrigValue Corresponding original value from OVF.
 * @param aAutoValue Initial configuration value (can be overridden by caller with setFinalValues).
 * @param ulSizeMB Weight for IProgress
 * @param strExtraConfig Extra configuration; meaning dependent on type.
 */
void VirtualSystemDescription::addEntry(VirtualSystemDescriptionType_T aType,
                                        const Utf8Str &strRef,
                                        const Utf8Str &aOrigValue,
                                        const Utf8Str &aAutoValue,
                                        uint32_t ulSizeMB,
                                        const Utf8Str &strExtraConfig /*= ""*/)
{
    VirtualSystemDescriptionEntry vsde;
    vsde.ulIndex = (uint32_t)m->llDescriptions.size();      // each entry gets an index so the client side can reference them
    vsde.type = aType;
    vsde.strRef = strRef;
    vsde.strOvf = aOrigValue;
    vsde.strVbox = aAutoValue;
    vsde.strExtraConfig = strExtraConfig;
    vsde.ulSizeMB = ulSizeMB;

    m->llDescriptions.push_back(vsde);
}

/**
 * Private method; returns a list of description items containing all the items from the member
 * description items of this virtual system that match the given type.
 * @param aType
 * @return
 */
std::list<VirtualSystemDescriptionEntry*> VirtualSystemDescription::findByType(VirtualSystemDescriptionType_T aType)
{
    std::list<VirtualSystemDescriptionEntry*> vsd;

    list<VirtualSystemDescriptionEntry>::iterator it;
    for (it = m->llDescriptions.begin();
         it != m->llDescriptions.end();
         ++it)
    {
        if (it->type == aType)
            vsd.push_back(&(*it));
    }

    return vsd;
}

/**
 * Private method; looks thru the member hardware items for the IDE, SATA, or SCSI controller with
 * the given reference ID. Useful when needing the controller for a particular
 * virtual disk.
 * @param id
 * @return
 */
const VirtualSystemDescriptionEntry* VirtualSystemDescription::findControllerFromID(uint32_t id)
{
    Utf8Str strRef = Utf8StrFmt("%RI32", id);
    list<VirtualSystemDescriptionEntry>::const_iterator it;
    for (it = m->llDescriptions.begin();
         it != m->llDescriptions.end();
         ++it)
    {
        const VirtualSystemDescriptionEntry &d = *it;
        switch (d.type)
        {
            case VirtualSystemDescriptionType_HardDiskControllerIDE:
            case VirtualSystemDescriptionType_HardDiskControllerSATA:
            case VirtualSystemDescriptionType_HardDiskControllerSCSI:
                if (d.strRef == strRef)
                    return &d;
            break;
        }
    }

    return NULL;
}

