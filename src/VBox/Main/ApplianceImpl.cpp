/* $Id$ */
/** @file
 *
 * IAppliance and IVirtualSystem COM class implementations.
 */

/*
 * Copyright (C) 2008-2009 Sun Microsystems, Inc.
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

#include <iprt/stream.h>
#include <iprt/path.h>
#include <iprt/dir.h>
#include <iprt/file.h>
#include <iprt/s3.h>
#include <iprt/sha.h>
#include <iprt/manifest.h>

#include "ovfreader.h"

#include <VBox/param.h>
#include <VBox/version.h>

#include "ApplianceImpl.h"
#include "VFSExplorerImpl.h"
#include "VirtualBoxImpl.h"
#include "GuestOSTypeImpl.h"
#include "ProgressImpl.h"
#include "MachineImpl.h"
#include "MediumImpl.h"

#include "HostNetworkInterfaceImpl.h"

#include "Logging.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////
//
// Appliance data definition
//
////////////////////////////////////////////////////////////////////////////////

/* Describe a location for the import/export. The location could be a file on a
 * local hard disk or a remote target based on the supported inet protocols. */
struct Appliance::LocationInfo
{
    LocationInfo()
      : storageType(VFSType_File) {}
    VFSType_T storageType; /* Which type of storage should be handled */
    Utf8Str strPath;       /* File path for the import/export */
    Utf8Str strHostname;   /* Hostname on remote storage locations (could be empty) */
    Utf8Str strUsername;   /* Username on remote storage locations (could be empty) */
    Utf8Str strPassword;   /* Password on remote storage locations (could be empty) */
};

// opaque private instance data of Appliance class
struct Appliance::Data
{
 	Data()
 	  : pReader(NULL) {}

 	~Data()
 	{
 	    if (pReader)
 	    {
 	        delete pReader;
 	        pReader = NULL;
 	    }
 	}

    LocationInfo locInfo; /* The location info for the currently processed OVF */

    OVFReader *pReader;

    list< ComObjPtr<VirtualSystemDescription> > virtualSystemDescriptions;

    list<Utf8Str> llWarnings;

    ULONG ulWeightPerOperation;
    Utf8Str strOVFSHA1Digest;
};

struct VirtualSystemDescription::Data
{
    list<VirtualSystemDescriptionEntry> llDescriptions;
};

////////////////////////////////////////////////////////////////////////////////
//
// internal helpers
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
static void convertCIMOSType2VBoxOSType(Utf8Str &strType, CIMOSType_T c, const Utf8Str &cStr)
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
static CIMOSType_T convertVBoxOSType2CIMOSType(const char *pcszVbox)
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

DEFINE_EMPTY_CTOR_DTOR(Appliance)

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
// Appliance private methods
//
////////////////////////////////////////////////////////////////////////////////

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
    while (RTPathExists(tmpName) ||
           mVirtualBox->FindHardDisk(Bstr(tmpName), &harddisk) != VBOX_E_OBJECT_NOT_FOUND)
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

struct Appliance::TaskOVF
{
    TaskOVF(Appliance *aThat)
      : pAppliance(aThat)
      , rc(S_OK) {}

    static int updateProgress(unsigned uPercent, void *pvUser);

    LocationInfo locInfo;
    Appliance *pAppliance;
    ComObjPtr<Progress> progress;
    HRESULT rc;
};

struct Appliance::TaskImportOVF: Appliance::TaskOVF
{
    enum TaskType
    {
        Read,
        Import
    };

    TaskImportOVF(Appliance *aThat)
        : TaskOVF(aThat)
        , taskType(Read) {}

    int startThread();

    TaskType taskType;
};

struct Appliance::TaskExportOVF: Appliance::TaskOVF
{
    enum OVFFormat
    {
        unspecified,
        OVF_0_9,
        OVF_1_0
    };
    enum TaskType
    {
        Write
    };

    TaskExportOVF(Appliance *aThat)
        : TaskOVF(aThat)
        , taskType(Write) {}

    int startThread();

    TaskType taskType;
    OVFFormat enFormat;
};

struct MyHardDiskAttachment
{
    Guid    uuid;
    ComPtr<IMachine> pMachine;
    Bstr    controllerType;
    int32_t lChannel;
    int32_t lDevice;
};

/* static */
int Appliance::TaskOVF::updateProgress(unsigned uPercent, void *pvUser)
{
    Appliance::TaskOVF* pTask = *(Appliance::TaskOVF**)pvUser;

    if (pTask &&
        !pTask->progress.isNull())
    {
        BOOL fCanceled;
        pTask->progress->COMGETTER(Canceled)(&fCanceled);
        if (fCanceled)
            return -1;
        pTask->progress->SetCurrentOperationProgress(uPercent);
    }
    return VINF_SUCCESS;
}

HRESULT Appliance::readImpl(const LocationInfo &aLocInfo, ComObjPtr<Progress> &aProgress)
{
    /* Initialize our worker task */
    std::auto_ptr<TaskImportOVF> task(new TaskImportOVF(this));
    /* What should the task do */
    task->taskType = TaskImportOVF::Read;
    /* Copy the current location info to the task */
    task->locInfo = aLocInfo;

    BstrFmt bstrDesc = BstrFmt(tr("Read appliance '%s'"),
                               aLocInfo.strPath.c_str());
    HRESULT rc;
    /* Create the progress object */
    aProgress.createObject();
    if (task->locInfo.storageType == VFSType_File)
    {
        /* 1 operation only */
        rc = aProgress->init(mVirtualBox, static_cast<IAppliance*>(this),
                             bstrDesc,
                             TRUE /* aCancelable */);
    }
    else
    {
        /* 4/5 is downloading, 1/5 is reading */
        rc = aProgress->init(mVirtualBox, static_cast<IAppliance*>(this),
                             bstrDesc,
                             TRUE /* aCancelable */,
                             2, // ULONG cOperations,
                             5, // ULONG ulTotalOperationsWeight,
                             BstrFmt(tr("Download appliance '%s'"),
                                     aLocInfo.strPath.c_str()), // CBSTR bstrFirstOperationDescription,
                             4); // ULONG ulFirstOperationWeight,
    }
    if (FAILED(rc)) throw rc;

    task->progress = aProgress;

    rc = task->startThread();
    CheckComRCThrowRC(rc);

    /* Don't destruct on success */
    task.release();

    return rc;
}

HRESULT Appliance::importImpl(const LocationInfo &aLocInfo, ComObjPtr<Progress> &aProgress)
{
    /* Initialize our worker task */
    std::auto_ptr<TaskImportOVF> task(new TaskImportOVF(this));
    /* What should the task do */
    task->taskType = TaskImportOVF::Import;
    /* Copy the current location info to the task */
    task->locInfo = aLocInfo;

    Bstr progressDesc = BstrFmt(tr("Import appliance '%s'"),
                                aLocInfo.strPath.c_str());

    HRESULT rc = S_OK;

    /* todo: This progress init stuff should be done a little bit more generic */
    if (task->locInfo.storageType == VFSType_File)
        rc = setUpProgressFS(aProgress, progressDesc);
    else
        rc = setUpProgressImportS3(aProgress, progressDesc);
    if (FAILED(rc)) throw rc;

    task->progress = aProgress;

    rc = task->startThread();
    CheckComRCThrowRC(rc);

    /* Don't destruct on success */
    task.release();

    return rc;
}

/**
 * Worker thread implementation for Read() (ovf reader).
 * @param aThread
 * @param pvUser
 */
/* static */
DECLCALLBACK(int) Appliance::taskThreadImportOVF(RTTHREAD /* aThread */, void *pvUser)
{
    std::auto_ptr<TaskImportOVF> task(static_cast<TaskImportOVF*>(pvUser));
    AssertReturn(task.get(), VERR_GENERAL_FAILURE);

    Appliance *pAppliance = task->pAppliance;

    LogFlowFuncEnter();
    LogFlowFunc(("Appliance %p\n", pAppliance));

    HRESULT rc = S_OK;

    switch(task->taskType)
    {
        case TaskImportOVF::Read:
        {
            if (task->locInfo.storageType == VFSType_File)
                rc = pAppliance->readFS(task.get());
            else if (task->locInfo.storageType == VFSType_S3)
                rc = pAppliance->readS3(task.get());
            break;
        }
        case TaskImportOVF::Import:
        {
            if (task->locInfo.storageType == VFSType_File)
                rc = pAppliance->importFS(task.get());
            else if (task->locInfo.storageType == VFSType_S3)
                rc = pAppliance->importS3(task.get());
            break;
        }
    }

    LogFlowFunc(("rc=%Rhrc\n", rc));
    LogFlowFuncLeave();

    return VINF_SUCCESS;
}

int Appliance::TaskImportOVF::startThread()
{
    int vrc = RTThreadCreate(NULL, Appliance::taskThreadImportOVF, this,
                             0, RTTHREADTYPE_MAIN_HEAVY_WORKER, 0,
                             "Appliance::Task");

    ComAssertMsgRCRet(vrc,
                      ("Could not create taskThreadImportOVF (%Rrc)\n", vrc), E_FAIL);

    return S_OK;
}

int Appliance::readFS(TaskImportOVF *pTask)
{
    LogFlowFuncEnter();
    LogFlowFunc(("Appliance %p\n", this));

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock appLock(this);

    HRESULT rc = S_OK;

    try
    {
        /* Read & parse the XML structure of the OVF file */
        m->pReader = new OVFReader(pTask->locInfo.strPath);
        /* Create the SHA1 sum of the OVF file for later validation */
        char *pszDigest;
        int vrc = RTSha1Digest(pTask->locInfo.strPath.c_str(), &pszDigest);
        if (RT_FAILURE(vrc))
            throw setError(VBOX_E_FILE_ERROR,
                           tr("Couldn't calculate SHA1 digest for file '%s' (%Rrc)"),
                           RTPathFilename(pTask->locInfo.strPath.c_str()), vrc);
        m->strOVFSHA1Digest = pszDigest;
        RTStrFree(pszDigest);
    }
    catch(xml::Error &x)
    {
        rc = setError(VBOX_E_FILE_ERROR,
                      x.what());
    }

    pTask->rc = rc;

    if (!pTask->progress.isNull())
        pTask->progress->notifyComplete(rc);

    LogFlowFunc(("rc=%Rhrc\n", rc));
    LogFlowFuncLeave();

    return VINF_SUCCESS;
}

int Appliance::readS3(TaskImportOVF *pTask)
{
    LogFlowFuncEnter();
    LogFlowFunc(("Appliance %p\n", this));

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock appLock(this);

    HRESULT rc = S_OK;
    int vrc = VINF_SUCCESS;
    RTS3 hS3 = NIL_RTS3;
    char szOSTmpDir[RTPATH_MAX];
    RTPathTemp(szOSTmpDir, sizeof(szOSTmpDir));
    /* The template for the temporary directory created below */
    char *pszTmpDir;
    RTStrAPrintf(&pszTmpDir, "%s"RTPATH_SLASH_STR"vbox-ovf-XXXXXX", szOSTmpDir);
    list< pair<Utf8Str, ULONG> > filesList;
    Utf8Str strTmpOvf;

    try
    {
        /* Extract the bucket */
        Utf8Str tmpPath = pTask->locInfo.strPath;
        Utf8Str bucket;
        parseBucket(tmpPath, bucket);

        /* We need a temporary directory which we can put the OVF file & all
         * disk images in */
        vrc = RTDirCreateTemp(pszTmpDir);
        if (RT_FAILURE(vrc))
            throw setError(VBOX_E_FILE_ERROR,
                           tr("Cannot create temporary directory '%s'"), pszTmpDir);

        /* The temporary name of the target OVF file */
        strTmpOvf = Utf8StrFmt("%s/%s", pszTmpDir, RTPathFilename(tmpPath.c_str()));

        /* Next we have to download the OVF */
        vrc = RTS3Create(&hS3, pTask->locInfo.strUsername.c_str(), pTask->locInfo.strPassword.c_str(), pTask->locInfo.strHostname.c_str(), "virtualbox-agent/"VBOX_VERSION_STRING);
        if(RT_FAILURE(vrc))
            throw setError(VBOX_E_IPRT_ERROR,
                           tr("Cannot create S3 service handler"));
        RTS3SetProgressCallback(hS3, pTask->updateProgress, &pTask);

        /* Get it */
        char *pszFilename = RTPathFilename(strTmpOvf.c_str());
        vrc = RTS3GetKey(hS3, bucket.c_str(), pszFilename, strTmpOvf.c_str());
        if (RT_FAILURE(vrc))
        {
            if(vrc == VERR_S3_CANCELED)
                throw S_OK; /* todo: !!!!!!!!!!!!! */
            else if(vrc == VERR_S3_ACCESS_DENIED)
                throw setError(E_ACCESSDENIED,
                               tr("Cannot download file '%s' from S3 storage server (Access denied). Make sure that your credentials are right. Also check that your host clock is properly synced"), pszFilename);
            else if(vrc == VERR_S3_NOT_FOUND)
                throw setError(VBOX_E_FILE_ERROR,
                               tr("Cannot download file '%s' from S3 storage server (File not found)"), pszFilename);
            else
                throw setError(VBOX_E_IPRT_ERROR,
                               tr("Cannot download file '%s' from S3 storage server (%Rrc)"), pszFilename, vrc);
        }

        /* Close the connection early */
        RTS3Destroy(hS3);
        hS3 = NIL_RTS3;

        if (!pTask->progress.isNull())
            pTask->progress->SetNextOperation(Bstr(tr("Reading")), 1);

        /* Prepare the temporary reading of the OVF */
        ComObjPtr<Progress> progress;
        LocationInfo li;
        li.strPath = strTmpOvf;
        /* Start the reading from the fs */
        rc = readImpl(li, progress);
        if (FAILED(rc)) throw rc;

        /* Unlock the appliance for the reading thread */
        appLock.unlock();
        /* Wait until the reading is done, but report the progress back to the
           caller */
        ComPtr<IProgress> progressInt(progress);
        waitForAsyncProgress(pTask->progress, progressInt); /* Any errors will be thrown */

        /* Again lock the appliance for the next steps */
        appLock.lock();
    }
    catch(HRESULT aRC)
    {
        rc = aRC;
    }
    /* Cleanup */
    RTS3Destroy(hS3);
    /* Delete all files which where temporary created */
    if (RTPathExists(strTmpOvf.c_str()))
    {
        vrc = RTFileDelete(strTmpOvf.c_str());
        if(RT_FAILURE(vrc))
            rc = setError(VBOX_E_FILE_ERROR,
                          tr("Cannot delete file '%s' (%Rrc)"), strTmpOvf.c_str(), vrc);
    }
    /* Delete the temporary directory */
    if (RTPathExists(pszTmpDir))
    {
        vrc = RTDirRemove(pszTmpDir);
        if(RT_FAILURE(vrc))
            rc = setError(VBOX_E_FILE_ERROR,
                          tr("Cannot delete temporary directory '%s' (%Rrc)"), pszTmpDir, vrc);
    }
    if (pszTmpDir)
        RTStrFree(pszTmpDir);

    pTask->rc = rc;

    if (!pTask->progress.isNull())
        pTask->progress->notifyComplete(rc);

    LogFlowFunc(("rc=%Rhrc\n", rc));
    LogFlowFuncLeave();

    return VINF_SUCCESS;
}

int Appliance::importFS(TaskImportOVF *pTask)
{
    LogFlowFuncEnter();
    LogFlowFunc(("Appliance %p\n", this));

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock appLock(this);

    HRESULT rc = S_OK;

    // rollback for errors:
    // a list of images that we created/imported
    list<MyHardDiskAttachment> llHardDiskAttachments;
    list< ComPtr<IMedium> > llHardDisksCreated;
    list<Guid> llMachinesRegistered;

    ComPtr<ISession> session;
    bool fSessionOpen = false;
    rc = session.createInprocObject(CLSID_Session);
    CheckComRCReturnRC(rc);

    const OVFReader reader = *m->pReader;
 	// this is safe to access because this thread only gets started
 	// if pReader != NULL

    /* If an manifest file exists, verify the content. Therefor we need all
     * files which are referenced by the OVF & the OVF itself */
    Utf8Str strMfFile = manifestFileName(pTask->locInfo.strPath);
    list<Utf8Str> filesList;
    if (RTPathExists(strMfFile.c_str()))
    {
        Utf8Str strSrcDir(pTask->locInfo.strPath);
        strSrcDir.stripFilename();
        /* Add every disks of every virtual system to an internal list */
        list< ComObjPtr<VirtualSystemDescription> >::const_iterator it;
        for (it = m->virtualSystemDescriptions.begin();
             it != m->virtualSystemDescriptions.end();
             ++it)
        {
            ComObjPtr<VirtualSystemDescription> vsdescThis = (*it);
            std::list<VirtualSystemDescriptionEntry*> avsdeHDs = vsdescThis->findByType(VirtualSystemDescriptionType_HardDiskImage);
            std::list<VirtualSystemDescriptionEntry*>::const_iterator itH;
            for (itH = avsdeHDs.begin();
                 itH != avsdeHDs.end();
                 ++itH)
            {
                VirtualSystemDescriptionEntry *vsdeHD = *itH;
                /* Find the disk from the OVF's disk list */
                DiskImagesMap::const_iterator itDiskImage = reader.m_mapDisks.find(vsdeHD->strRef);
                const DiskImage &di = itDiskImage->second;
                Utf8StrFmt strSrcFilePath("%s%c%s", strSrcDir.c_str(), RTPATH_DELIMITER, di.strHref.c_str());
                filesList.push_back(strSrcFilePath);
            }
        }
        /* Create the test list */
        PRTMANIFESTTEST pTestList = (PRTMANIFESTTEST)RTMemAllocZ(sizeof(RTMANIFESTTEST)*(filesList.size()+1));
        pTestList[0].pszTestFile = (char*)pTask->locInfo.strPath.c_str();
        pTestList[0].pszTestDigest = (char*)m->strOVFSHA1Digest.c_str();
        int vrc = VINF_SUCCESS;
        size_t i = 1;
        list<Utf8Str>::const_iterator it1;
        for (it1 = filesList.begin();
             it1 != filesList.end();
             ++it1, ++i)
        {
            char* pszDigest;
            vrc = RTSha1Digest((*it1).c_str(), &pszDigest);
            pTestList[i].pszTestFile = (char*)(*it1).c_str();
            pTestList[i].pszTestDigest = pszDigest;
        }
        size_t cIndexOnError;
        vrc = RTManifestVerify(strMfFile.c_str(), pTestList, filesList.size() + 1, &cIndexOnError);
        if (vrc == VERR_MANIFEST_DIGEST_MISMATCH)
            rc = setError(VBOX_E_FILE_ERROR,
                          tr("The SHA1 digest of '%s' doesn't match to the one in '%s'"),
                          RTPathFilename(pTestList[cIndexOnError].pszTestFile),
                          RTPathFilename(strMfFile.c_str()));
        else if (RT_FAILURE(vrc))
            rc = setError(VBOX_E_FILE_ERROR,
                          tr("Couldn't verify the content of '%s' against the available files (%Rrc)"),
                          RTPathFilename(strMfFile.c_str()),
                          vrc);
        /* Cleanup */
        for (size_t i=1; i < filesList.size(); ++i)
            RTStrFree(pTestList[i].pszTestDigest);
        RTMemFree(pTestList);
        if (FAILED(rc))
        {
            /* Return on error */
            pTask->rc = rc;

            if (!pTask->progress.isNull())
                pTask->progress->notifyComplete(rc);
            return rc;
        }
    }

    list<VirtualSystem>::const_iterator it;
    list< ComObjPtr<VirtualSystemDescription> >::const_iterator it1;
    /* Iterate through all virtual systems of that appliance */
    size_t i = 0;
    for (it = reader.m_llVirtualSystems.begin(),
         it1 = m->virtualSystemDescriptions.begin();
         it != reader.m_llVirtualSystems.end();
         ++it, ++it1, ++i)
    {
        const VirtualSystem &vsysThis = *it;
        ComObjPtr<VirtualSystemDescription> vsdescThis = (*it1);

        ComPtr<IMachine> pNewMachine;

        /* Catch possible errors */
        try
        {
            /* Guest OS type */
            std::list<VirtualSystemDescriptionEntry*> vsdeOS;
            vsdeOS = vsdescThis->findByType(VirtualSystemDescriptionType_OS);
            if (vsdeOS.size() < 1)
                throw setError(VBOX_E_FILE_ERROR,
                               tr("Missing guest OS type"));
            const Utf8Str &strOsTypeVBox = vsdeOS.front()->strVbox;

            /* Now that we know the base system get our internal defaults based on that. */
            ComPtr<IGuestOSType> osType;
            rc = mVirtualBox->GetGuestOSType(Bstr(strOsTypeVBox), osType.asOutParam());
            if (FAILED(rc)) throw rc;

            /* Create the machine */
            /* First get the name */
            std::list<VirtualSystemDescriptionEntry*> vsdeName = vsdescThis->findByType(VirtualSystemDescriptionType_Name);
            if (vsdeName.size() < 1)
                throw setError(VBOX_E_FILE_ERROR,
                               tr("Missing VM name"));
            const Utf8Str &strNameVBox = vsdeName.front()->strVbox;
            rc = mVirtualBox->CreateMachine(Bstr(strNameVBox), Bstr(strOsTypeVBox),
                                                 Bstr(), Bstr(),
                                                 pNewMachine.asOutParam());
            if (FAILED(rc)) throw rc;

            // and the description
            std::list<VirtualSystemDescriptionEntry*> vsdeDescription = vsdescThis->findByType(VirtualSystemDescriptionType_Description);
            if (vsdeDescription.size())
            {
                const Utf8Str &strDescription = vsdeDescription.front()->strVbox;
                rc = pNewMachine->COMSETTER(Description)(Bstr(strDescription));
                if (FAILED(rc)) throw rc;
            }

            /* CPU count */
            std::list<VirtualSystemDescriptionEntry*> vsdeCPU = vsdescThis->findByType (VirtualSystemDescriptionType_CPU);
            ComAssertMsgThrow(vsdeCPU.size() == 1, ("CPU count missing"), E_FAIL);
            const Utf8Str &cpuVBox = vsdeCPU.front()->strVbox;
            ULONG tmpCount = (ULONG)RTStrToUInt64(cpuVBox.c_str());
            rc = pNewMachine->COMSETTER(CPUCount)(tmpCount);
            if (FAILED(rc)) throw rc;
            bool fEnableIOApic = false;
            /* We need HWVirt & IO-APIC if more than one CPU is requested */
            if (tmpCount > 1)
            {
                rc = pNewMachine->SetHWVirtExProperty(HWVirtExPropertyType_Enabled, TRUE);
                if (FAILED(rc)) throw rc;

                fEnableIOApic = true;
            }

            /* RAM */
            std::list<VirtualSystemDescriptionEntry*> vsdeRAM = vsdescThis->findByType(VirtualSystemDescriptionType_Memory);
            ComAssertMsgThrow(vsdeRAM.size() == 1, ("RAM size missing"), E_FAIL);
            const Utf8Str &memoryVBox = vsdeRAM.front()->strVbox;
            ULONG tt = (ULONG)RTStrToUInt64(memoryVBox.c_str());
            rc = pNewMachine->COMSETTER(MemorySize)(tt);
            if (FAILED(rc)) throw rc;

            /* VRAM */
            /* Get the recommended VRAM for this guest OS type */
            ULONG vramVBox;
            rc = osType->COMGETTER(RecommendedVRAM)(&vramVBox);
            if (FAILED(rc)) throw rc;

            /* Set the VRAM */
            rc = pNewMachine->COMSETTER(VRAMSize)(vramVBox);
            if (FAILED(rc)) throw rc;

            /* I/O APIC: so far we have no setting for this. Enable it if we
              import a Windows VM because if if Windows was installed without IOAPIC,
              it will not mind finding an one later on, but if Windows was installed
              _with_ an IOAPIC, it will bluescreen if it's not found */
            Bstr bstrFamilyId;
            rc = osType->COMGETTER(FamilyId)(bstrFamilyId.asOutParam());
            if (FAILED(rc)) throw rc;

            Utf8Str strFamilyId(bstrFamilyId);
            if (strFamilyId == "Windows")
                fEnableIOApic = true;

            /* If IP-APIC should be enabled could be have different reasons.
               See CPU count & the Win test above. Here we enable it if it was
               previously requested. */
            if (fEnableIOApic)
            {
                ComPtr<IBIOSSettings> pBIOSSettings;
                rc = pNewMachine->COMGETTER(BIOSSettings)(pBIOSSettings.asOutParam());
                if (FAILED(rc)) throw rc;

                rc = pBIOSSettings->COMSETTER(IOAPICEnabled)(TRUE);
                if (FAILED(rc)) throw rc;
            }

            /* Audio Adapter */
            std::list<VirtualSystemDescriptionEntry*> vsdeAudioAdapter = vsdescThis->findByType(VirtualSystemDescriptionType_SoundCard);
            /* @todo: we support one audio adapter only */
            if (vsdeAudioAdapter.size() > 0)
            {
                const Utf8Str& audioAdapterVBox = vsdeAudioAdapter.front()->strVbox;
                if (audioAdapterVBox.compare("null", Utf8Str::CaseInsensitive) != 0)
                {
                    uint32_t audio = RTStrToUInt32(audioAdapterVBox.c_str());
                    ComPtr<IAudioAdapter> audioAdapter;
                    rc = pNewMachine->COMGETTER(AudioAdapter)(audioAdapter.asOutParam());
                    if (FAILED(rc)) throw rc;
                    rc = audioAdapter->COMSETTER(Enabled)(true);
                    if (FAILED(rc)) throw rc;
                    rc = audioAdapter->COMSETTER(AudioController)(static_cast<AudioControllerType_T>(audio));
                    if (FAILED(rc)) throw rc;
                }
            }

#ifdef VBOX_WITH_USB
            /* USB Controller */
            std::list<VirtualSystemDescriptionEntry*> vsdeUSBController = vsdescThis->findByType(VirtualSystemDescriptionType_USBController);
            // USB support is enabled if there's at least one such entry; to disable USB support,
            // the type of the USB item would have been changed to "ignore"
            bool fUSBEnabled = vsdeUSBController.size() > 0;

            ComPtr<IUSBController> usbController;
            rc = pNewMachine->COMGETTER(USBController)(usbController.asOutParam());
            if (FAILED(rc)) throw rc;
            rc = usbController->COMSETTER(Enabled)(fUSBEnabled);
            if (FAILED(rc)) throw rc;
#endif /* VBOX_WITH_USB */

            /* Change the network adapters */
            std::list<VirtualSystemDescriptionEntry*> vsdeNW = vsdescThis->findByType(VirtualSystemDescriptionType_NetworkAdapter);
            if (vsdeNW.size() == 0)
            {
                /* No network adapters, so we have to disable our default one */
                ComPtr<INetworkAdapter> nwVBox;
                rc = pNewMachine->GetNetworkAdapter(0, nwVBox.asOutParam());
                if (FAILED(rc)) throw rc;
                rc = nwVBox->COMSETTER(Enabled)(false);
                if (FAILED(rc)) throw rc;
            }
            else
            {
                list<VirtualSystemDescriptionEntry*>::const_iterator nwIt;
                /* Iterate through all network cards. We support 8 network adapters
                 * at the maximum. (@todo: warn if there are more!) */
                size_t a = 0;
                for (nwIt = vsdeNW.begin();
                     (nwIt != vsdeNW.end() && a < SchemaDefs::NetworkAdapterCount);
                     ++nwIt, ++a)
                {
                    const VirtualSystemDescriptionEntry* pvsys = *nwIt;

                    const Utf8Str &nwTypeVBox = pvsys->strVbox;
                    uint32_t tt1 = RTStrToUInt32(nwTypeVBox.c_str());
                    ComPtr<INetworkAdapter> pNetworkAdapter;
                    rc = pNewMachine->GetNetworkAdapter((ULONG)a, pNetworkAdapter.asOutParam());
                    if (FAILED(rc)) throw rc;
                    /* Enable the network card & set the adapter type */
                    rc = pNetworkAdapter->COMSETTER(Enabled)(true);
                    if (FAILED(rc)) throw rc;
                    rc = pNetworkAdapter->COMSETTER(AdapterType)(static_cast<NetworkAdapterType_T>(tt1));
                    if (FAILED(rc)) throw rc;

                    // default is NAT; change to "bridged" if extra conf says so
                    if (!pvsys->strExtraConfig.compare("type=Bridged", Utf8Str::CaseInsensitive))
                    {
                        /* Attach to the right interface */
                        rc = pNetworkAdapter->AttachToBridgedInterface();
                        if (FAILED(rc)) throw rc;
                        ComPtr<IHost> host;
                        rc = mVirtualBox->COMGETTER(Host)(host.asOutParam());
                        if (FAILED(rc)) throw rc;
                        com::SafeIfaceArray<IHostNetworkInterface> nwInterfaces;
                        rc = host->COMGETTER(NetworkInterfaces)(ComSafeArrayAsOutParam(nwInterfaces));
                        if (FAILED(rc)) throw rc;
                        /* We search for the first host network interface which
                         * is usable for bridged networking */
                        for (size_t i=0; i < nwInterfaces.size(); ++i)
                        {
                            HostNetworkInterfaceType_T itype;
                            rc = nwInterfaces[i]->COMGETTER(InterfaceType)(&itype);
                            if (FAILED(rc)) throw rc;
                            if (itype == HostNetworkInterfaceType_Bridged)
                            {
                                Bstr name;
                                rc = nwInterfaces[i]->COMGETTER(Name)(name.asOutParam());
                                if (FAILED(rc)) throw rc;
                                /* Set the interface name to attach to */
                                pNetworkAdapter->COMSETTER(HostInterface)(name);
                                if (FAILED(rc)) throw rc;
                                break;
                            }
                        }
                    }
                    /* Next test for host only interfaces */
                    else if (!pvsys->strExtraConfig.compare("type=HostOnly", Utf8Str::CaseInsensitive))
                    {
                        /* Attach to the right interface */
                        rc = pNetworkAdapter->AttachToHostOnlyInterface();
                        if (FAILED(rc)) throw rc;
                        ComPtr<IHost> host;
                        rc = mVirtualBox->COMGETTER(Host)(host.asOutParam());
                        if (FAILED(rc)) throw rc;
                        com::SafeIfaceArray<IHostNetworkInterface> nwInterfaces;
                        rc = host->COMGETTER(NetworkInterfaces)(ComSafeArrayAsOutParam(nwInterfaces));
                        if (FAILED(rc)) throw rc;
                        /* We search for the first host network interface which
                         * is usable for host only networking */
                        for (size_t i=0; i < nwInterfaces.size(); ++i)
                        {
                            HostNetworkInterfaceType_T itype;
                            rc = nwInterfaces[i]->COMGETTER(InterfaceType)(&itype);
                            if (FAILED(rc)) throw rc;
                            if (itype == HostNetworkInterfaceType_HostOnly)
                            {
                                Bstr name;
                                rc = nwInterfaces[i]->COMGETTER(Name)(name.asOutParam());
                                if (FAILED(rc)) throw rc;
                                /* Set the interface name to attach to */
                                pNetworkAdapter->COMSETTER(HostInterface)(name);
                                if (FAILED(rc)) throw rc;
                                break;
                            }
                        }
                    }
                }
            }

/// @todo FIXME
#if 0
            /* Floppy drive */
            std::list<VirtualSystemDescriptionEntry*> vsdeFloppy = vsdescThis->findByType(VirtualSystemDescriptionType_Floppy);
            // Floppy support is enabled if there's at least one such entry; to disable floppy support,
            // the type of the floppy item would have been changed to "ignore"
            bool fFloppyEnabled = vsdeFloppy.size() > 0;
            ComPtr<IMedium> floppyDrive;
            rc = pNewMachine->COMGETTER(FloppyDrive)(floppyDrive.asOutParam());
            if (FAILED(rc)) throw rc;
            rc = floppyDrive->COMSETTER(Enabled)(fFloppyEnabled);
            if (FAILED(rc)) throw rc;

            /* CDROM drive */
            /* @todo: I can't disable the CDROM. So nothing to do for now */
            // std::list<VirtualSystemDescriptionEntry*> vsdeFloppy = vsd->findByType(VirtualSystemDescriptionType_CDROM);
#endif

            /* Hard disk controller IDE */
            std::list<VirtualSystemDescriptionEntry*> vsdeHDCIDE = vsdescThis->findByType(VirtualSystemDescriptionType_HardDiskControllerIDE);
            if (vsdeHDCIDE.size() > 1)
                throw setError(VBOX_E_FILE_ERROR,
                               tr("Too many IDE controllers in OVF; VirtualBox only supports one"));
            if (vsdeHDCIDE.size() == 1)
            {
                ComPtr<IStorageController> pController;
                rc = pNewMachine->AddStorageController(Bstr("IDE Controller"), StorageBus_IDE, pController.asOutParam());
                if (FAILED(rc)) throw rc;

                const char *pcszIDEType = vsdeHDCIDE.front()->strVbox.c_str();
                if (!strcmp(pcszIDEType, "PIIX3"))
                    rc = pController->COMSETTER(ControllerType)(StorageControllerType_PIIX3);
                else if (!strcmp(pcszIDEType, "PIIX4"))
                    rc = pController->COMSETTER(ControllerType)(StorageControllerType_PIIX4);
                else if (!strcmp(pcszIDEType, "ICH6"))
                    rc = pController->COMSETTER(ControllerType)(StorageControllerType_ICH6);
                else
                    throw setError(VBOX_E_FILE_ERROR,
                                   tr("Invalid IDE controller type \"%s\""),
                                   pcszIDEType);
                if (FAILED(rc)) throw rc;
            }
#ifdef VBOX_WITH_AHCI
            /* Hard disk controller SATA */
            std::list<VirtualSystemDescriptionEntry*> vsdeHDCSATA = vsdescThis->findByType(VirtualSystemDescriptionType_HardDiskControllerSATA);
            if (vsdeHDCSATA.size() > 1)
                throw setError(VBOX_E_FILE_ERROR,
                               tr("Too many SATA controllers in OVF; VirtualBox only supports one"));
            if (vsdeHDCSATA.size() > 0)
            {
                ComPtr<IStorageController> pController;
                const Utf8Str &hdcVBox = vsdeHDCSATA.front()->strVbox;
                if (hdcVBox == "AHCI")
                {
                    rc = pNewMachine->AddStorageController(Bstr("SATA Controller"), StorageBus_SATA, pController.asOutParam());
                    if (FAILED(rc)) throw rc;
                }
                else
                    throw setError(VBOX_E_FILE_ERROR,
                                   tr("Invalid SATA controller type \"%s\""),
                                   hdcVBox.c_str());
            }
#endif /* VBOX_WITH_AHCI */

#ifdef VBOX_WITH_LSILOGIC
            /* Hard disk controller SCSI */
            std::list<VirtualSystemDescriptionEntry*> vsdeHDCSCSI = vsdescThis->findByType(VirtualSystemDescriptionType_HardDiskControllerSCSI);
            if (vsdeHDCSCSI.size() > 1)
                throw setError(VBOX_E_FILE_ERROR,
                               tr("Too many SCSI controllers in OVF; VirtualBox only supports one"));
            if (vsdeHDCSCSI.size() > 0)
            {
                ComPtr<IStorageController> pController;
                StorageControllerType_T controllerType;
                const Utf8Str &hdcVBox = vsdeHDCSCSI.front()->strVbox;
                if (hdcVBox == "LsiLogic")
                    controllerType = StorageControllerType_LsiLogic;
                else if (hdcVBox == "BusLogic")
                    controllerType = StorageControllerType_BusLogic;
                else
                    throw setError(VBOX_E_FILE_ERROR,
                                   tr("Invalid SCSI controller type \"%s\""),
                                   hdcVBox.c_str());

                rc = pNewMachine->AddStorageController(Bstr("SCSI Controller"), StorageBus_SCSI, pController.asOutParam());
                if (FAILED(rc)) throw rc;
                rc = pController->COMSETTER(ControllerType)(controllerType);
                if (FAILED(rc)) throw rc;
            }
#endif /* VBOX_WITH_LSILOGIC */

            /* Now its time to register the machine before we add any hard disks */
            rc = mVirtualBox->RegisterMachine(pNewMachine);
            if (FAILED(rc)) throw rc;

            Bstr newMachineId_;
            rc = pNewMachine->COMGETTER(Id)(newMachineId_.asOutParam());
            if (FAILED(rc)) throw rc;
            Guid newMachineId(newMachineId_);

            // store new machine for roll-back in case of errors
            llMachinesRegistered.push_back(newMachineId);

            /* Create the hard disks & connect them to the appropriate controllers. */
            std::list<VirtualSystemDescriptionEntry*> avsdeHDs = vsdescThis->findByType(VirtualSystemDescriptionType_HardDiskImage);
            if (avsdeHDs.size() > 0)
            {
                /* If in the next block an error occur we have to deregister
                   the machine, so make an extra try/catch block. */
                ComPtr<IMedium> srcHdVBox;
                bool fSourceHdNeedsClosing = false;

                try
                {
                    /* In order to attach hard disks we need to open a session
                     * for the new machine */
                    rc = mVirtualBox->OpenSession(session, newMachineId_);
                    if (FAILED(rc)) throw rc;
                    fSessionOpen = true;

                    /* The disk image has to be on the same place as the OVF file. So
                     * strip the filename out of the full file path. */
                    Utf8Str strSrcDir(pTask->locInfo.strPath);
                    strSrcDir.stripFilename();

                    /* Iterate over all given disk images */
                    list<VirtualSystemDescriptionEntry*>::const_iterator itHD;
                    for (itHD = avsdeHDs.begin();
                         itHD != avsdeHDs.end();
                         ++itHD)
                    {
                        VirtualSystemDescriptionEntry *vsdeHD = *itHD;

                        /* Check if the destination file exists already or the
                         * destination path is empty. */
                        if (    vsdeHD->strVbox.isEmpty()
                             || RTPathExists(vsdeHD->strVbox.c_str())
                           )
                            /* This isn't allowed */
                            throw setError(VBOX_E_FILE_ERROR,
                                           tr("Destination file '%s' exists",
                                              vsdeHD->strVbox.c_str()));

                        /* Find the disk from the OVF's disk list */
                        DiskImagesMap::const_iterator itDiskImage = reader.m_mapDisks.find(vsdeHD->strRef);
                        /* vsdeHD->strRef contains the disk identifier (e.g. "vmdisk1"), which should exist
                           in the virtual system's disks map under that ID and also in the global images map. */
                        VirtualDisksMap::const_iterator itVirtualDisk = vsysThis.mapVirtualDisks.find(vsdeHD->strRef);

                        if (    itDiskImage == reader.m_mapDisks.end()
                             || itVirtualDisk == vsysThis.mapVirtualDisks.end()
                           )
                            throw setError(E_FAIL,
                                           tr("Internal inconsistency looking up disk images."));

                        const DiskImage &di = itDiskImage->second;
                        const VirtualDisk &vd = itVirtualDisk->second;

                        /* Make sure all target directories exists */
                        rc = VirtualBox::ensureFilePathExists(vsdeHD->strVbox.c_str());
                        if (FAILED(rc))
                            throw rc;

                        // subprogress object for hard disk
                        ComPtr<IProgress> pProgress2;

                        ComPtr<IMedium> dstHdVBox;
                        /* If strHref is empty we have to create a new file */
                        if (di.strHref.isEmpty())
                        {
                            /* Which format to use? */
                            Bstr srcFormat = L"VDI";
                            if (   di.strFormat.compare("http://www.vmware.com/specifications/vmdk.html#sparse", Utf8Str::CaseInsensitive)
                                || di.strFormat.compare("http://www.vmware.com/specifications/vmdk.html#compressed", Utf8Str::CaseInsensitive))
                                srcFormat = L"VMDK";
                            /* Create an empty hard disk */
                            rc = mVirtualBox->CreateHardDisk(srcFormat, Bstr(vsdeHD->strVbox), dstHdVBox.asOutParam());
                            if (FAILED(rc)) throw rc;

                            /* Create a dynamic growing disk image with the given capacity */
                            rc = dstHdVBox->CreateBaseStorage(di.iCapacity / _1M, MediumVariant_Standard, pProgress2.asOutParam());
                            if (FAILED(rc)) throw rc;

                            /* Advance to the next operation */
                            if (!pTask->progress.isNull())
                                pTask->progress->SetNextOperation(BstrFmt(tr("Creating virtual disk image '%s'"), vsdeHD->strVbox.c_str()),
                                                                 vsdeHD->ulSizeMB);     // operation's weight, as set up with the IProgress originally
                        }
                        else
                        {
                            /* Construct the source file path */
                            Utf8StrFmt strSrcFilePath("%s%c%s", strSrcDir.c_str(), RTPATH_DELIMITER, di.strHref.c_str());
                            /* Check if the source file exists */
                            if (!RTPathExists(strSrcFilePath.c_str()))
                                /* This isn't allowed */
                                throw setError(VBOX_E_FILE_ERROR,
                                               tr("Source virtual disk image file '%s' doesn't exist"),
                                                  strSrcFilePath.c_str());

                            /* Clone the disk image (this is necessary cause the id has
                             * to be recreated for the case the same hard disk is
                             * attached already from a previous import) */

                            /* First open the existing disk image */
                            rc = mVirtualBox->OpenHardDisk(Bstr(strSrcFilePath),
                                                           AccessMode_ReadOnly,
                                                           false, Bstr(""), false, Bstr(""),
                                                           srcHdVBox.asOutParam());
                            if (FAILED(rc)) throw rc;
                            fSourceHdNeedsClosing = true;

                            /* We need the format description of the source disk image */
                            Bstr srcFormat;
                            rc = srcHdVBox->COMGETTER(Format)(srcFormat.asOutParam());
                            if (FAILED(rc)) throw rc;
                            /* Create a new hard disk interface for the destination disk image */
                            rc = mVirtualBox->CreateHardDisk(srcFormat, Bstr(vsdeHD->strVbox), dstHdVBox.asOutParam());
                            if (FAILED(rc)) throw rc;
                            /* Clone the source disk image */
                            rc = srcHdVBox->CloneTo(dstHdVBox, MediumVariant_Standard, NULL, pProgress2.asOutParam());
                            if (FAILED(rc)) throw rc;

                            /* Advance to the next operation */
                            if (!pTask->progress.isNull())
                                pTask->progress->SetNextOperation(BstrFmt(tr("Importing virtual disk image '%s'"), strSrcFilePath.c_str()),
                                                                 vsdeHD->ulSizeMB);     // operation's weight, as set up with the IProgress originally);
                        }

                        // now wait for the background disk operation to complete; this throws HRESULTs on error
                        waitForAsyncProgress(pTask->progress, pProgress2);

                        if (fSourceHdNeedsClosing)
                        {
                            rc = srcHdVBox->Close();
                            if (FAILED(rc)) throw rc;
                            fSourceHdNeedsClosing = false;
                        }

                        llHardDisksCreated.push_back(dstHdVBox);
                        /* Now use the new uuid to attach the disk image to our new machine */
                        ComPtr<IMachine> sMachine;
                        rc = session->COMGETTER(Machine)(sMachine.asOutParam());
                        if (FAILED(rc)) throw rc;
                        Bstr hdId;
                        rc = dstHdVBox->COMGETTER(Id)(hdId.asOutParam());
                        if (FAILED(rc)) throw rc;

                        /* For now we assume we have one controller of every type only */
                        HardDiskController hdc = (*vsysThis.mapControllers.find(vd.idController)).second;

                        // this is for rollback later
                        MyHardDiskAttachment mhda;
                        mhda.uuid = newMachineId;
                        mhda.pMachine = pNewMachine;

                        switch (hdc.system)
                        {
                            case HardDiskController::IDE:
                                // For the IDE bus, the channel parameter can be either 0 or 1, to specify the primary
                                // or secondary IDE controller, respectively. For the primary controller of the IDE bus,
                                // the device number can be either 0 or 1, to specify the master or the slave device,
                                // respectively. For the secondary IDE controller, the device number is always 1 because
                                // the master device is reserved for the CD-ROM drive.
                                mhda.controllerType = Bstr("IDE Controller");
                                switch (vd.ulAddressOnParent)
                                {
                                    case 0:     // interpret this as primary master
                                        mhda.lChannel = (long)0;
                                        mhda.lDevice = (long)0;
                                    break;

                                    case 1:     // interpret this as primary slave
                                        mhda.lChannel = (long)0;
                                        mhda.lDevice = (long)1;
                                    break;

                                    case 2:     // interpret this as secondary slave
                                        mhda.lChannel = (long)1;
                                        mhda.lDevice = (long)1;
                                    break;

                                    default:
                                        throw setError(VBOX_E_NOT_SUPPORTED,
                                                       tr("Invalid channel %RI16 specified; IDE controllers support only 0, 1 or 2"), vd.ulAddressOnParent);
                                    break;
                                }
                            break;

                            case HardDiskController::SATA:
                                mhda.controllerType = Bstr("SATA Controller");
                                mhda.lChannel = (long)vd.ulAddressOnParent;
                                mhda.lDevice = (long)0;
                            break;

                            case HardDiskController::SCSI:
                                mhda.controllerType = Bstr("SCSI Controller");
                                mhda.lChannel = (long)vd.ulAddressOnParent;
                                mhda.lDevice = (long)0;
                            break;

                            default: break;
                        }

                        Log(("Attaching disk %s to channel %d on device %d\n", vsdeHD->strVbox.c_str(), mhda.lChannel, mhda.lDevice));

                        rc = sMachine->AttachDevice(mhda.controllerType,
                                                    mhda.lChannel,
                                                    mhda.lDevice,
                                                    DeviceType_HardDisk,
                                                    hdId);
                        if (FAILED(rc)) throw rc;

                        llHardDiskAttachments.push_back(mhda);

                        rc = sMachine->SaveSettings();
                        if (FAILED(rc)) throw rc;
                    } // end for (itHD = avsdeHDs.begin();

                    // only now that we're done with all disks, close the session
                    rc = session->Close();
                    if (FAILED(rc)) throw rc;
                    fSessionOpen = false;
                }
                catch(HRESULT /* aRC */)
                {
                    if (fSourceHdNeedsClosing)
                        srcHdVBox->Close();

                    if (fSessionOpen)
                        session->Close();

                    throw;
                }
            }
        }
        catch(HRESULT aRC)
        {
            rc = aRC;
        }

        if (FAILED(rc))
            break;

    } // for (it = pAppliance->m->llVirtualSystems.begin(),

    if (FAILED(rc))
    {
        // with _whatever_ error we've had, do a complete roll-back of
        // machines and disks we've created; unfortunately this is
        // not so trivially done...

        HRESULT rc2;
        // detach all hard disks from all machines we created
        list<MyHardDiskAttachment>::iterator itM;
        for (itM = llHardDiskAttachments.begin();
             itM != llHardDiskAttachments.end();
             ++itM)
        {
            const MyHardDiskAttachment &mhda = *itM;
            rc2 = mVirtualBox->OpenSession(session, Bstr(mhda.uuid));
            if (SUCCEEDED(rc2))
            {
                ComPtr<IMachine> sMachine;
                rc2 = session->COMGETTER(Machine)(sMachine.asOutParam());
                if (SUCCEEDED(rc2))
                {
                    rc2 = sMachine->DetachDevice(Bstr(mhda.controllerType), mhda.lChannel, mhda.lDevice);
                    rc2 = sMachine->SaveSettings();
                }
                session->Close();
            }
        }

        // now clean up all hard disks we created
        list< ComPtr<IMedium> >::iterator itHD;
        for (itHD = llHardDisksCreated.begin();
             itHD != llHardDisksCreated.end();
             ++itHD)
        {
            ComPtr<IMedium> pDisk = *itHD;
            ComPtr<IProgress> pProgress;
            rc2 = pDisk->DeleteStorage(pProgress.asOutParam());
            rc2 = pProgress->WaitForCompletion(-1);
        }

        // finally, deregister and remove all machines
        list<Guid>::iterator itID;
        for (itID = llMachinesRegistered.begin();
             itID != llMachinesRegistered.end();
             ++itID)
        {
            const Guid &guid = *itID;
            ComPtr<IMachine> failedMachine;
            rc2 = mVirtualBox->UnregisterMachine(guid.toUtf16(), failedMachine.asOutParam());
            if (SUCCEEDED(rc2))
                rc2 = failedMachine->DeleteSettings();
        }
    }

    pTask->rc = rc;

    if (!pTask->progress.isNull())
        pTask->progress->notifyComplete(rc);

    LogFlowFunc(("rc=%Rhrc\n", rc));
    LogFlowFuncLeave();

    return VINF_SUCCESS;
}

int Appliance::importS3(TaskImportOVF *pTask)
{
    LogFlowFuncEnter();
    LogFlowFunc(("Appliance %p\n", this));

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock appLock(this);

    int vrc = VINF_SUCCESS;
    RTS3 hS3 = NIL_RTS3;
    char szOSTmpDir[RTPATH_MAX];
    RTPathTemp(szOSTmpDir, sizeof(szOSTmpDir));
    /* The template for the temporary directory created below */
    char *pszTmpDir;
    RTStrAPrintf(&pszTmpDir, "%s"RTPATH_SLASH_STR"vbox-ovf-XXXXXX", szOSTmpDir);
    list< pair<Utf8Str, ULONG> > filesList;

    HRESULT rc = S_OK;
    try
    {
        /* Extract the bucket */
        Utf8Str tmpPath = pTask->locInfo.strPath;
        Utf8Str bucket;
        parseBucket(tmpPath, bucket);

        /* We need a temporary directory which we can put the all disk images
         * in */
        vrc = RTDirCreateTemp(pszTmpDir);
        if (RT_FAILURE(vrc))
            throw setError(VBOX_E_FILE_ERROR,
                           tr("Cannot create temporary directory '%s'"), pszTmpDir);

        /* Add every disks of every virtual system to an internal list */
        list< ComObjPtr<VirtualSystemDescription> >::const_iterator it;
        for (it = m->virtualSystemDescriptions.begin();
             it != m->virtualSystemDescriptions.end();
             ++it)
        {
            ComObjPtr<VirtualSystemDescription> vsdescThis = (*it);
            std::list<VirtualSystemDescriptionEntry*> avsdeHDs = vsdescThis->findByType(VirtualSystemDescriptionType_HardDiskImage);
            std::list<VirtualSystemDescriptionEntry*>::const_iterator itH;
            for (itH = avsdeHDs.begin();
                 itH != avsdeHDs.end();
                 ++itH)
            {
                const Utf8Str &strTargetFile = (*itH)->strOvf;
                if (!strTargetFile.isEmpty())
                {
                    /* The temporary name of the target disk file */
                    Utf8StrFmt strTmpDisk("%s/%s", pszTmpDir, RTPathFilename(strTargetFile.c_str()));
                    filesList.push_back(pair<Utf8Str, ULONG>(strTmpDisk, (*itH)->ulSizeMB));
                }
            }
        }

        /* Next we have to download the disk images */
        vrc = RTS3Create(&hS3, pTask->locInfo.strUsername.c_str(), pTask->locInfo.strPassword.c_str(), pTask->locInfo.strHostname.c_str(), "virtualbox-agent/"VBOX_VERSION_STRING);
        if(RT_FAILURE(vrc))
            throw setError(VBOX_E_IPRT_ERROR,
                           tr("Cannot create S3 service handler"));
        RTS3SetProgressCallback(hS3, pTask->updateProgress, &pTask);

        /* Download all files */
        for (list< pair<Utf8Str, ULONG> >::const_iterator it1 = filesList.begin(); it1 != filesList.end(); ++it1)
        {
            const pair<Utf8Str, ULONG> &s = (*it1);
            const Utf8Str &strSrcFile = s.first;
            /* Construct the source file name */
            char *pszFilename = RTPathFilename(strSrcFile.c_str());
            /* Advance to the next operation */
            if (!pTask->progress.isNull())
                pTask->progress->SetNextOperation(BstrFmt(tr("Downloading file '%s'"), pszFilename), s.second);

            vrc = RTS3GetKey(hS3, bucket.c_str(), pszFilename, strSrcFile.c_str());
            if (RT_FAILURE(vrc))
            {
                if(vrc == VERR_S3_CANCELED)
                    throw S_OK; /* todo: !!!!!!!!!!!!! */
                else if(vrc == VERR_S3_ACCESS_DENIED)
                    throw setError(E_ACCESSDENIED,
                                   tr("Cannot download file '%s' from S3 storage server (Access denied). Make sure that your credentials are right. Also check that your host clock is properly synced"), pszFilename);
                else if(vrc == VERR_S3_NOT_FOUND)
                    throw setError(VBOX_E_FILE_ERROR,
                                   tr("Cannot download file '%s' from S3 storage server (File not found)"), pszFilename);
                else
                    throw setError(VBOX_E_IPRT_ERROR,
                                   tr("Cannot download file '%s' from S3 storage server (%Rrc)"), pszFilename, vrc);
            }
        }

        /* Provide a OVF file (haven't to exist) so the import routine can
         * figure out where the disk images/manifest file are located. */
        Utf8StrFmt strTmpOvf("%s/%s", pszTmpDir, RTPathFilename(tmpPath.c_str()));
        /* Now check if there is an manifest file. This is optional. */
        Utf8Str strManifestFile = manifestFileName(strTmpOvf);
        char *pszFilename = RTPathFilename(strManifestFile.c_str());
        if (!pTask->progress.isNull())
            pTask->progress->SetNextOperation(BstrFmt(tr("Downloading file '%s'"), pszFilename), 1);

        /* Try to download it. If the error is VERR_S3_NOT_FOUND, it isn't fatal. */
        vrc = RTS3GetKey(hS3, bucket.c_str(), pszFilename, strManifestFile.c_str());
        if (RT_SUCCESS(vrc))
            filesList.push_back(pair<Utf8Str, ULONG>(strManifestFile, 0));
        else if (RT_FAILURE(vrc))
        {
            if(vrc == VERR_S3_CANCELED)
                throw S_OK; /* todo: !!!!!!!!!!!!! */
            else if(vrc == VERR_S3_NOT_FOUND)
                vrc = VINF_SUCCESS; /* Not found is ok */
            else if(vrc == VERR_S3_ACCESS_DENIED)
                throw setError(E_ACCESSDENIED,
                               tr("Cannot download file '%s' from S3 storage server (Access denied). Make sure that your credentials are right. Also check that your host clock is properly synced"), pszFilename);
            else
                throw setError(VBOX_E_IPRT_ERROR,
                               tr("Cannot download file '%s' from S3 storage server (%Rrc)"), pszFilename, vrc);
        }

        /* Close the connection early */
        RTS3Destroy(hS3);
        hS3 = NIL_RTS3;

        if (!pTask->progress.isNull())
            pTask->progress->SetNextOperation(BstrFmt(tr("Importing appliance")), m->ulWeightPerOperation);

        ComObjPtr<Progress> progress;
        /* Import the whole temporary OVF & the disk images */
        LocationInfo li;
        li.strPath = strTmpOvf;
        rc = importImpl(li, progress);
        if (FAILED(rc)) throw rc;

        /* Unlock the appliance for the fs import thread */
        appLock.unlock();
        /* Wait until the import is done, but report the progress back to the
           caller */
        ComPtr<IProgress> progressInt(progress);
        waitForAsyncProgress(pTask->progress, progressInt); /* Any errors will be thrown */

        /* Again lock the appliance for the next steps */
        appLock.lock();
    }
    catch(HRESULT aRC)
    {
        rc = aRC;
    }
    /* Cleanup */
    RTS3Destroy(hS3);
    /* Delete all files which where temporary created */
    for (list< pair<Utf8Str, ULONG> >::const_iterator it1 = filesList.begin(); it1 != filesList.end(); ++it1)
    {
        const char *pszFilePath = (*it1).first.c_str();
        if (RTPathExists(pszFilePath))
        {
            vrc = RTFileDelete(pszFilePath);
            if(RT_FAILURE(vrc))
                rc = setError(VBOX_E_FILE_ERROR,
                              tr("Cannot delete file '%s' (%Rrc)"), pszFilePath, vrc);
        }
    }
    /* Delete the temporary directory */
    if (RTPathExists(pszTmpDir))
    {
        vrc = RTDirRemove(pszTmpDir);
        if(RT_FAILURE(vrc))
            rc = setError(VBOX_E_FILE_ERROR,
                          tr("Cannot delete temporary directory '%s' (%Rrc)"), pszTmpDir, vrc);
    }
    if (pszTmpDir)
        RTStrFree(pszTmpDir);

    pTask->rc = rc;

    if (!pTask->progress.isNull())
        pTask->progress->notifyComplete(rc);

    LogFlowFunc(("rc=%Rhrc\n", rc));
    LogFlowFuncLeave();

    return VINF_SUCCESS;
}

HRESULT Appliance::writeImpl(int aFormat, const LocationInfo &aLocInfo, ComObjPtr<Progress> &aProgress)
{
    HRESULT rc = S_OK;
    try
    {
        /* Initialize our worker task */
        std::auto_ptr<TaskExportOVF> task(new TaskExportOVF(this));
        /* What should the task do */
        task->taskType = TaskExportOVF::Write;
        /* The OVF version to write */
        task->enFormat = (TaskExportOVF::OVFFormat)aFormat;
        /* Copy the current location info to the task */
        task->locInfo = aLocInfo;

        Bstr progressDesc = BstrFmt(tr("Export appliance '%s'"),
                                    task->locInfo.strPath.c_str());

        /* todo: This progress init stuff should be done a little bit more generic */
        if (task->locInfo.storageType == VFSType_File)
            rc = setUpProgressFS(aProgress, progressDesc);
        else
            rc = setUpProgressWriteS3(aProgress, progressDesc);

        task->progress = aProgress;

        rc = task->startThread();
        CheckComRCThrowRC(rc);

        /* Don't destruct on success */
        task.release();
    }
    catch (HRESULT aRC)
    {
        rc = aRC;
    }

    return rc;
}

DECLCALLBACK(int) Appliance::taskThreadWriteOVF(RTTHREAD /* aThread */, void *pvUser)
{
    std::auto_ptr<TaskExportOVF> task(static_cast<TaskExportOVF*>(pvUser));
    AssertReturn(task.get(), VERR_GENERAL_FAILURE);

    Appliance *pAppliance = task->pAppliance;

    LogFlowFuncEnter();
    LogFlowFunc(("Appliance %p\n", pAppliance));

    HRESULT rc = S_OK;

    switch(task->taskType)
    {
        case TaskExportOVF::Write:
        {
            if (task->locInfo.storageType == VFSType_File)
                rc = pAppliance->writeFS(task.get());
            else if (task->locInfo.storageType == VFSType_S3)
                rc = pAppliance->writeS3(task.get());
            break;
        }
    }

    LogFlowFunc(("rc=%Rhrc\n", rc));
    LogFlowFuncLeave();

    return VINF_SUCCESS;
}

int Appliance::TaskExportOVF::startThread()
{
    int vrc = RTThreadCreate(NULL, Appliance::taskThreadWriteOVF, this,
                             0, RTTHREADTYPE_MAIN_HEAVY_WORKER, 0,
                             "Appliance::Task");

    ComAssertMsgRCRet(vrc,
                      ("Could not create taskThreadWriteOVF (%Rrc)\n", vrc), E_FAIL);

    return S_OK;
}

int Appliance::writeFS(TaskExportOVF *pTask)
{
    LogFlowFuncEnter();
    LogFlowFunc(("Appliance %p\n", this));

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock appLock(this);

    HRESULT rc = S_OK;

    try
    {
        xml::Document doc;
        xml::ElementNode *pelmRoot = doc.createRootElement("Envelope");

        pelmRoot->setAttribute("ovf:version", (pTask->enFormat == TaskExportOVF::OVF_1_0) ? "1.0" : "0.9");
        pelmRoot->setAttribute("xml:lang", "en-US");

        Utf8Str strNamespace = (pTask->enFormat == TaskExportOVF::OVF_0_9)
            ? "http://www.vmware.com/schema/ovf/1/envelope"     // 0.9
            : "http://schemas.dmtf.org/ovf/envelope/1";         // 1.0
        pelmRoot->setAttribute("xmlns", strNamespace);
        pelmRoot->setAttribute("xmlns:ovf", strNamespace);

//         pelmRoot->setAttribute("xmlns:ovfstr", "http://schema.dmtf.org/ovf/strings/1");
        pelmRoot->setAttribute("xmlns:rasd", "http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_ResourceAllocationSettingData");
        pelmRoot->setAttribute("xmlns:vssd", "http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_VirtualSystemSettingData");
        pelmRoot->setAttribute("xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance");
//         pelmRoot->setAttribute("xsi:schemaLocation", "http://schemas.dmtf.org/ovf/envelope/1 ../ovf-envelope.xsd");

        // <Envelope>/<References>
        xml::ElementNode *pelmReferences = pelmRoot->createChild("References");     // 0.9 and 1.0

        /* <Envelope>/<DiskSection>:
            <DiskSection>
                <Info>List of the virtual disks used in the package</Info>
                <Disk ovf:capacity="4294967296" ovf:diskId="lamp" ovf:format="http://www.vmware.com/specifications/vmdk.html#compressed" ovf:populatedSize="1924967692"/>
            </DiskSection> */
        xml::ElementNode *pelmDiskSection;
        if (pTask->enFormat == TaskExportOVF::OVF_0_9)
        {
            // <Section xsi:type="ovf:DiskSection_Type">
            pelmDiskSection = pelmRoot->createChild("Section");
            pelmDiskSection->setAttribute("xsi:type", "ovf:DiskSection_Type");
        }
        else
            pelmDiskSection = pelmRoot->createChild("DiskSection");

        xml::ElementNode *pelmDiskSectionInfo = pelmDiskSection->createChild("Info");
        pelmDiskSectionInfo->addContent("List of the virtual disks used in the package");
        // for now, set up a map so we have a list of unique disk names (to make
        // sure the same disk name is only added once)
        map<Utf8Str, const VirtualSystemDescriptionEntry*> mapDisks;

        /* <Envelope>/<NetworkSection>:
            <NetworkSection>
                <Info>Logical networks used in the package</Info>
                <Network ovf:name="VM Network">
                    <Description>The network that the LAMP Service will be available on</Description>
                </Network>
            </NetworkSection> */
        xml::ElementNode *pelmNetworkSection;
        if (pTask->enFormat == TaskExportOVF::OVF_0_9)
        {
            // <Section xsi:type="ovf:NetworkSection_Type">
            pelmNetworkSection = pelmRoot->createChild("Section");
            pelmNetworkSection->setAttribute("xsi:type", "ovf:NetworkSection_Type");
        }
        else
            pelmNetworkSection = pelmRoot->createChild("NetworkSection");

        xml::ElementNode *pelmNetworkSectionInfo = pelmNetworkSection->createChild("Info");
        pelmNetworkSectionInfo->addContent("Logical networks used in the package");
        // for now, set up a map so we have a list of unique network names (to make
        // sure the same network name is only added once)
        map<Utf8Str, bool> mapNetworks;
                // we fill this later below when we iterate over the networks

        // and here come the virtual systems:

        // write a collection if we have more than one virtual system _and_ we're
        // writing OVF 1.0; otherwise fail since ovftool can't import more than
        // one machine, it seems
        xml::ElementNode *pelmToAddVirtualSystemsTo;
        if (m->virtualSystemDescriptions.size() > 1)
        {
            if (pTask->enFormat == TaskExportOVF::OVF_0_9)
                throw setError(VBOX_E_FILE_ERROR,
                               tr("Cannot export more than one virtual system with OVF 0.9, use OVF 1.0"));

            pelmToAddVirtualSystemsTo = pelmRoot->createChild("VirtualSystemCollection");
            /* xml::AttributeNode *pattrVirtualSystemCollectionId = */ pelmToAddVirtualSystemsTo->setAttribute("ovf:name", "ExportedVirtualBoxMachines");      // whatever
        }
        else
            pelmToAddVirtualSystemsTo = pelmRoot;       // add virtual system directly under root element

        uint32_t cDisks = 0;

        list< ComObjPtr<VirtualSystemDescription> >::const_iterator it;
        /* Iterate through all virtual systems of that appliance */
        for (it = m->virtualSystemDescriptions.begin();
             it != m->virtualSystemDescriptions.end();
             ++it)
        {
            ComObjPtr<VirtualSystemDescription> vsdescThis = (*it);

            xml::ElementNode *pelmVirtualSystem;
            if (pTask->enFormat == TaskExportOVF::OVF_0_9)
            {
                // <Section xsi:type="ovf:NetworkSection_Type">
                pelmVirtualSystem = pelmToAddVirtualSystemsTo->createChild("Content");
                pelmVirtualSystem->setAttribute("xsi:type", "ovf:VirtualSystem_Type");
            }
            else
                pelmVirtualSystem = pelmToAddVirtualSystemsTo->createChild("VirtualSystem");

            /*xml::ElementNode *pelmVirtualSystemInfo =*/ pelmVirtualSystem->createChild("Info")->addContent("A virtual machine");

            std::list<VirtualSystemDescriptionEntry*> llName = vsdescThis->findByType(VirtualSystemDescriptionType_Name);
            if (llName.size() != 1)
                throw setError(VBOX_E_NOT_SUPPORTED,
                               tr("Missing VM name"));
            Utf8Str &strVMName = llName.front()->strVbox;
            pelmVirtualSystem->setAttribute("ovf:id", strVMName);

            // product info
            std::list<VirtualSystemDescriptionEntry*> llProduct = vsdescThis->findByType(VirtualSystemDescriptionType_Product);
            std::list<VirtualSystemDescriptionEntry*> llProductUrl = vsdescThis->findByType(VirtualSystemDescriptionType_ProductUrl);
            std::list<VirtualSystemDescriptionEntry*> llVendor = vsdescThis->findByType(VirtualSystemDescriptionType_Vendor);
            std::list<VirtualSystemDescriptionEntry*> llVendorUrl = vsdescThis->findByType(VirtualSystemDescriptionType_VendorUrl);
            std::list<VirtualSystemDescriptionEntry*> llVersion = vsdescThis->findByType(VirtualSystemDescriptionType_Version);
            bool fProduct = llProduct.size() && !llProduct.front()->strVbox.isEmpty();
            bool fProductUrl = llProductUrl.size() && !llProductUrl.front()->strVbox.isEmpty();
            bool fVendor = llVendor.size() && !llVendor.front()->strVbox.isEmpty();
            bool fVendorUrl = llVendorUrl.size() && !llVendorUrl.front()->strVbox.isEmpty();
            bool fVersion = llVersion.size() && !llVersion.front()->strVbox.isEmpty();
            if (fProduct ||
                fProductUrl ||
                fVersion ||
                fVendorUrl ||
                fVersion)
            {
                /* <Section ovf:required="false" xsi:type="ovf:ProductSection_Type">
                    <Info>Meta-information about the installed software</Info>
                    <Product>VAtest</Product>
                    <Vendor>SUN Microsystems</Vendor>
                    <Version>10.0</Version>
                    <ProductUrl>http://blogs.sun.com/VirtualGuru</ProductUrl>
                    <VendorUrl>http://www.sun.com</VendorUrl>
                </Section> */
                xml::ElementNode *pelmAnnotationSection;
                if (pTask->enFormat == TaskExportOVF::OVF_0_9)
                {
                    // <Section ovf:required="false" xsi:type="ovf:ProductSection_Type">
                    pelmAnnotationSection = pelmVirtualSystem->createChild("Section");
                    pelmAnnotationSection->setAttribute("xsi:type", "ovf:ProductSection_Type");
                }
                else
                    pelmAnnotationSection = pelmVirtualSystem->createChild("ProductSection");

                pelmAnnotationSection->createChild("Info")->addContent("Meta-information about the installed software");
                if (fProduct)
                    pelmAnnotationSection->createChild("Product")->addContent(llProduct.front()->strVbox);
                if (fVendor)
                    pelmAnnotationSection->createChild("Vendor")->addContent(llVendor.front()->strVbox);
                if (fVersion)
                    pelmAnnotationSection->createChild("Version")->addContent(llVersion.front()->strVbox);
                if (fProductUrl)
                    pelmAnnotationSection->createChild("ProductUrl")->addContent(llProductUrl.front()->strVbox);
                if (fVendorUrl)
                    pelmAnnotationSection->createChild("VendorUrl")->addContent(llVendorUrl.front()->strVbox);
            }

            // description
            std::list<VirtualSystemDescriptionEntry*> llDescription = vsdescThis->findByType(VirtualSystemDescriptionType_Description);
            if (llDescription.size() &&
                !llDescription.front()->strVbox.isEmpty())
            {
                /*  <Section ovf:required="false" xsi:type="ovf:AnnotationSection_Type">
                        <Info>A human-readable annotation</Info>
                        <Annotation>Plan 9</Annotation>
                    </Section> */
                xml::ElementNode *pelmAnnotationSection;
                if (pTask->enFormat == TaskExportOVF::OVF_0_9)
                {
                    // <Section ovf:required="false" xsi:type="ovf:AnnotationSection_Type">
                    pelmAnnotationSection = pelmVirtualSystem->createChild("Section");
                    pelmAnnotationSection->setAttribute("xsi:type", "ovf:AnnotationSection_Type");
                }
                else
                    pelmAnnotationSection = pelmVirtualSystem->createChild("AnnotationSection");

                pelmAnnotationSection->createChild("Info")->addContent("A human-readable annotation");
                pelmAnnotationSection->createChild("Annotation")->addContent(llDescription.front()->strVbox);
            }

            // license
            std::list<VirtualSystemDescriptionEntry*> llLicense = vsdescThis->findByType(VirtualSystemDescriptionType_License);
            if (llLicense.size() &&
                !llLicense.front()->strVbox.isEmpty())
            {
                /* <EulaSection>
                   <Info ovf:msgid="6">License agreement for the Virtual System.</Info>
                   <License ovf:msgid="1">License terms can go in here.</License>
                   </EulaSection> */
                xml::ElementNode *pelmEulaSection;
                if (pTask->enFormat == TaskExportOVF::OVF_0_9)
                {
                    pelmEulaSection = pelmVirtualSystem->createChild("Section");
                    pelmEulaSection->setAttribute("xsi:type", "ovf:EulaSection_Type");
                }
                else
                    pelmEulaSection = pelmVirtualSystem->createChild("EulaSection");

                pelmEulaSection->createChild("Info")->addContent("License agreement for the virtual system");
                pelmEulaSection->createChild("License")->addContent(llLicense.front()->strVbox);
            }

            // operating system
            std::list<VirtualSystemDescriptionEntry*> llOS = vsdescThis->findByType(VirtualSystemDescriptionType_OS);
            if (llOS.size() != 1)
                throw setError(VBOX_E_NOT_SUPPORTED,
                               tr("Missing OS type"));
            /*  <OperatingSystemSection ovf:id="82">
                    <Info>Guest Operating System</Info>
                    <Description>Linux 2.6.x</Description>
                </OperatingSystemSection> */
            xml::ElementNode *pelmOperatingSystemSection;
            if (pTask->enFormat == TaskExportOVF::OVF_0_9)
            {
                pelmOperatingSystemSection = pelmVirtualSystem->createChild("Section");
                pelmOperatingSystemSection->setAttribute("xsi:type", "ovf:OperatingSystemSection_Type");
            }
            else
                pelmOperatingSystemSection = pelmVirtualSystem->createChild("OperatingSystemSection");

            pelmOperatingSystemSection->setAttribute("ovf:id", llOS.front()->strOvf);
            pelmOperatingSystemSection->createChild("Info")->addContent("The kind of installed guest operating system");
            Utf8Str strOSDesc;
            convertCIMOSType2VBoxOSType(strOSDesc, (CIMOSType_T)llOS.front()->strOvf.toInt32(), "");
            pelmOperatingSystemSection->createChild("Description")->addContent(strOSDesc);

            // <VirtualHardwareSection ovf:id="hw1" ovf:transport="iso">
            xml::ElementNode *pelmVirtualHardwareSection;
            if (pTask->enFormat == TaskExportOVF::OVF_0_9)
            {
                // <Section xsi:type="ovf:VirtualHardwareSection_Type">
                pelmVirtualHardwareSection = pelmVirtualSystem->createChild("Section");
                pelmVirtualHardwareSection->setAttribute("xsi:type", "ovf:VirtualHardwareSection_Type");
            }
            else
                pelmVirtualHardwareSection = pelmVirtualSystem->createChild("VirtualHardwareSection");

            pelmVirtualHardwareSection->createChild("Info")->addContent("Virtual hardware requirements for a virtual machine");

            /*  <System>
                    <vssd:Description>Description of the virtual hardware section.</vssd:Description>
                    <vssd:ElementName>vmware</vssd:ElementName>
                    <vssd:InstanceID>1</vssd:InstanceID>
                    <vssd:VirtualSystemIdentifier>MyLampService</vssd:VirtualSystemIdentifier>
                    <vssd:VirtualSystemType>vmx-4</vssd:VirtualSystemType>
                </System> */
            xml::ElementNode *pelmSystem = pelmVirtualHardwareSection->createChild("System");

            pelmSystem->createChild("vssd:ElementName")->addContent("Virtual Hardware Family"); // required OVF 1.0

            // <vssd:InstanceId>0</vssd:InstanceId>
            if (pTask->enFormat == TaskExportOVF::OVF_0_9)
                pelmSystem->createChild("vssd:InstanceId")->addContent("0");
            else // capitalization changed...
                pelmSystem->createChild("vssd:InstanceID")->addContent("0");

            // <vssd:VirtualSystemIdentifier>VAtest</vssd:VirtualSystemIdentifier>
            pelmSystem->createChild("vssd:VirtualSystemIdentifier")->addContent(strVMName);
            // <vssd:VirtualSystemType>vmx-4</vssd:VirtualSystemType>
            const char *pcszHardware = "virtualbox-2.2";
            if (pTask->enFormat == TaskExportOVF::OVF_0_9)
                // pretend to be vmware compatible then
                pcszHardware = "vmx-6";
            pelmSystem->createChild("vssd:VirtualSystemType")->addContent(pcszHardware);

            // loop thru all description entries twice; once to write out all
            // devices _except_ disk images, and a second time to assign the
            // disk images; this is because disk images need to reference
            // IDE controllers, and we can't know their instance IDs without
            // assigning them first

            uint32_t idIDEController = 0;
            int32_t lIDEControllerIndex = 0;
            uint32_t idSATAController = 0;
            int32_t lSATAControllerIndex = 0;
            uint32_t idSCSIController = 0;
            int32_t lSCSIControllerIndex = 0;

            uint32_t ulInstanceID = 1;

            for (size_t uLoop = 1;
                 uLoop <= 2;
                 ++uLoop)
            {
                int32_t lIndexThis = 0;
                list<VirtualSystemDescriptionEntry>::const_iterator itD;
                for (itD = vsdescThis->m->llDescriptions.begin();
                    itD != vsdescThis->m->llDescriptions.end();
                    ++itD, ++lIndexThis)
                {
                    const VirtualSystemDescriptionEntry &desc = *itD;

                    OVFResourceType_T type = (OVFResourceType_T)0;      // if this becomes != 0 then we do stuff
                    Utf8Str strResourceSubType;

                    Utf8Str strDescription;                             // results in <rasd:Description>...</rasd:Description> block
                    Utf8Str strCaption;                                 // results in <rasd:Caption>...</rasd:Caption> block

                    uint32_t ulParent = 0;

                    int32_t lVirtualQuantity = -1;
                    Utf8Str strAllocationUnits;

                    int32_t lAddress = -1;
                    int32_t lBusNumber = -1;
                    int32_t lAddressOnParent = -1;

                    int32_t lAutomaticAllocation = -1;                  // 0 means "false", 1 means "true"
                    Utf8Str strConnection;                              // results in <rasd:Connection>...</rasd:Connection> block
                    Utf8Str strHostResource;

                    uint64_t uTemp;

                    switch (desc.type)
                    {
                        case VirtualSystemDescriptionType_CPU:
                            /*  <Item>
                                    <rasd:Caption>1 virtual CPU</rasd:Caption>
                                    <rasd:Description>Number of virtual CPUs</rasd:Description>
                                    <rasd:ElementName>virtual CPU</rasd:ElementName>
                                    <rasd:InstanceID>1</rasd:InstanceID>
                                    <rasd:ResourceType>3</rasd:ResourceType>
                                    <rasd:VirtualQuantity>1</rasd:VirtualQuantity>
                                </Item> */
                            if (uLoop == 1)
                            {
                                strDescription = "Number of virtual CPUs";
                                type = OVFResourceType_Processor; // 3
                                desc.strVbox.toInt(uTemp);
                                lVirtualQuantity = (int32_t)uTemp;
                                strCaption = Utf8StrFmt("%d virtual CPU", lVirtualQuantity);     // without this ovftool won't eat the item
                            }
                        break;

                        case VirtualSystemDescriptionType_Memory:
                            /*  <Item>
                                    <rasd:AllocationUnits>MegaBytes</rasd:AllocationUnits>
                                    <rasd:Caption>256 MB of memory</rasd:Caption>
                                    <rasd:Description>Memory Size</rasd:Description>
                                    <rasd:ElementName>Memory</rasd:ElementName>
                                    <rasd:InstanceID>2</rasd:InstanceID>
                                    <rasd:ResourceType>4</rasd:ResourceType>
                                    <rasd:VirtualQuantity>256</rasd:VirtualQuantity>
                                </Item> */
                            if (uLoop == 1)
                            {
                                strDescription = "Memory Size";
                                type = OVFResourceType_Memory; // 4
                                desc.strVbox.toInt(uTemp);
                                lVirtualQuantity = (int32_t)(uTemp / _1M);
                                strAllocationUnits = "MegaBytes";
                                strCaption = Utf8StrFmt("%d MB of memory", lVirtualQuantity);     // without this ovftool won't eat the item
                            }
                        break;

                        case VirtualSystemDescriptionType_HardDiskControllerIDE:
                            /* <Item>
                                    <rasd:Caption>ideController1</rasd:Caption>
                                    <rasd:Description>IDE Controller</rasd:Description>
                                    <rasd:InstanceId>5</rasd:InstanceId>
                                    <rasd:ResourceType>5</rasd:ResourceType>
                                    <rasd:Address>1</rasd:Address>
                                    <rasd:BusNumber>1</rasd:BusNumber>
                                </Item> */
                            if (uLoop == 1)
                            {
                                strDescription = "IDE Controller";
                                strCaption = "ideController0";
                                type = OVFResourceType_IDEController; // 5
                                strResourceSubType = desc.strVbox;
                                // it seems that OVFTool always writes these two, and since we can only
                                // have one IDE controller, we'll use this as well
                                lAddress = 1;
                                lBusNumber = 1;

                                // remember this ID
                                idIDEController = ulInstanceID;
                                lIDEControllerIndex = lIndexThis;
                            }
                        break;

                        case VirtualSystemDescriptionType_HardDiskControllerSATA:
                            /*  <Item>
                                    <rasd:Caption>sataController0</rasd:Caption>
                                    <rasd:Description>SATA Controller</rasd:Description>
                                    <rasd:InstanceId>4</rasd:InstanceId>
                                    <rasd:ResourceType>20</rasd:ResourceType>
                                    <rasd:ResourceSubType>ahci</rasd:ResourceSubType>
                                    <rasd:Address>0</rasd:Address>
                                    <rasd:BusNumber>0</rasd:BusNumber>
                                </Item>
                            */
                            if (uLoop == 1)
                            {
                                strDescription = "SATA Controller";
                                strCaption = "sataController0";
                                type = OVFResourceType_OtherStorageDevice; // 20
                                // it seems that OVFTool always writes these two, and since we can only
                                // have one SATA controller, we'll use this as well
                                lAddress = 0;
                                lBusNumber = 0;

                                if (    desc.strVbox.isEmpty()      // AHCI is the default in VirtualBox
                                     || (!desc.strVbox.compare("ahci", Utf8Str::CaseInsensitive))
                                   )
                                    strResourceSubType = "AHCI";
                                else
                                    throw setError(VBOX_E_NOT_SUPPORTED,
                                                   tr("Invalid config string \"%s\" in SATA controller"), desc.strVbox.c_str());

                                // remember this ID
                                idSATAController = ulInstanceID;
                                lSATAControllerIndex = lIndexThis;
                            }
                        break;

                        case VirtualSystemDescriptionType_HardDiskControllerSCSI:
                            /*  <Item>
                                    <rasd:Caption>scsiController0</rasd:Caption>
                                    <rasd:Description>SCSI Controller</rasd:Description>
                                    <rasd:InstanceId>4</rasd:InstanceId>
                                    <rasd:ResourceType>6</rasd:ResourceType>
                                    <rasd:ResourceSubType>buslogic</rasd:ResourceSubType>
                                    <rasd:Address>0</rasd:Address>
                                    <rasd:BusNumber>0</rasd:BusNumber>
                                </Item>
                            */
                            if (uLoop == 1)
                            {
                                strDescription = "SCSI Controller";
                                strCaption = "scsiController0";
                                type = OVFResourceType_ParallelSCSIHBA; // 6
                                // it seems that OVFTool always writes these two, and since we can only
                                // have one SATA controller, we'll use this as well
                                lAddress = 0;
                                lBusNumber = 0;

                                if (    desc.strVbox.isEmpty()      // LsiLogic is the default in VirtualBox
                                     || (!desc.strVbox.compare("lsilogic", Utf8Str::CaseInsensitive))
                                   )
                                    strResourceSubType = "lsilogic";
                                else if (!desc.strVbox.compare("buslogic", Utf8Str::CaseInsensitive))
                                    strResourceSubType = "buslogic";
                                else
                                    throw setError(VBOX_E_NOT_SUPPORTED,
                                                   tr("Invalid config string \"%s\" in SCSI controller"), desc.strVbox.c_str());

                                // remember this ID
                                idSCSIController = ulInstanceID;
                                lSCSIControllerIndex = lIndexThis;
                            }
                        break;

                        case VirtualSystemDescriptionType_HardDiskImage:
                            /*  <Item>
                                    <rasd:Caption>disk1</rasd:Caption>
                                    <rasd:InstanceId>8</rasd:InstanceId>
                                    <rasd:ResourceType>17</rasd:ResourceType>
                                    <rasd:HostResource>/disk/vmdisk1</rasd:HostResource>
                                    <rasd:Parent>4</rasd:Parent>
                                    <rasd:AddressOnParent>0</rasd:AddressOnParent>
                                </Item> */
                            if (uLoop == 2)
                            {
                                Utf8Str strDiskID = Utf8StrFmt("vmdisk%RI32", ++cDisks);

                                strDescription = "Disk Image";
                                strCaption = Utf8StrFmt("disk%RI32", cDisks);        // this is not used for anything else
                                type = OVFResourceType_HardDisk; // 17

                                // the following references the "<Disks>" XML block
                                strHostResource = Utf8StrFmt("/disk/%s", strDiskID.c_str());

                                // controller=<index>;channel=<c>
                                size_t pos1 = desc.strExtraConfig.find("controller=");
                                size_t pos2 = desc.strExtraConfig.find("channel=");
                                if (pos1 != Utf8Str::npos)
                                {
                                    int32_t lControllerIndex = -1;
                                    RTStrToInt32Ex(desc.strExtraConfig.c_str() + pos1 + 11, NULL, 0, &lControllerIndex);
                                    if (lControllerIndex == lIDEControllerIndex)
                                        ulParent = idIDEController;
                                    else if (lControllerIndex == lSCSIControllerIndex)
                                        ulParent = idSCSIController;
                                    else if (lControllerIndex == lSATAControllerIndex)
                                        ulParent = idSATAController;
                                }
                                if (pos2 != Utf8Str::npos)
                                    RTStrToInt32Ex(desc.strExtraConfig.c_str() + pos2 + 8, NULL, 0, &lAddressOnParent);

                                if (    !ulParent
                                     || lAddressOnParent == -1
                                   )
                                    throw setError(VBOX_E_NOT_SUPPORTED,
                                                   tr("Missing or bad extra config string in hard disk image: \"%s\""), desc.strExtraConfig.c_str());

                                mapDisks[strDiskID] = &desc;
                            }
                        break;

                        case VirtualSystemDescriptionType_Floppy:
                            if (uLoop == 1)
                            {
                                strDescription = "Floppy Drive";
                                strCaption = "floppy0";         // this is what OVFTool writes
                                type = OVFResourceType_FloppyDrive; // 14
                                lAutomaticAllocation = 0;
                                lAddressOnParent = 0;           // this is what OVFTool writes
                            }
                        break;

                        case VirtualSystemDescriptionType_CDROM:
                            if (uLoop == 2)
                            {
                                // we can't have a CD without an IDE controller
                                if (!idIDEController)
                                    throw setError(VBOX_E_NOT_SUPPORTED,
                                                   tr("Can't have CD-ROM without IDE controller"));

                                strDescription = "CD-ROM Drive";
                                strCaption = "cdrom1";          // this is what OVFTool writes
                                type = OVFResourceType_CDDrive; // 15
                                lAutomaticAllocation = 1;
                                ulParent = idIDEController;
                                lAddressOnParent = 0;           // this is what OVFTool writes
                            }
                        break;

                        case VirtualSystemDescriptionType_NetworkAdapter:
                            /* <Item>
                                    <rasd:AutomaticAllocation>true</rasd:AutomaticAllocation>
                                    <rasd:Caption>Ethernet adapter on 'VM Network'</rasd:Caption>
                                    <rasd:Connection>VM Network</rasd:Connection>
                                    <rasd:ElementName>VM network</rasd:ElementName>
                                    <rasd:InstanceID>3</rasd:InstanceID>
                                    <rasd:ResourceType>10</rasd:ResourceType>
                                </Item> */
                            if (uLoop == 1)
                            {
                                lAutomaticAllocation = 1;
                                strCaption = Utf8StrFmt("Ethernet adapter on '%s'", desc.strOvf.c_str());
                                type = OVFResourceType_EthernetAdapter; // 10
                                /* Set the hardware type to something useful.
                                 * To be compatible with vmware & others we set
                                 * PCNet32 for our PCNet types & E1000 for the
                                 * E1000 cards. */
                                switch (desc.strVbox.toInt32())
                                {
                                    case NetworkAdapterType_Am79C970A:
                                    case NetworkAdapterType_Am79C973: strResourceSubType = "PCNet32"; break;
#ifdef VBOX_WITH_E1000
                                    case NetworkAdapterType_I82540EM:
                                    case NetworkAdapterType_I82545EM:
                                    case NetworkAdapterType_I82543GC: strResourceSubType = "E1000"; break;
#endif /* VBOX_WITH_E1000 */
                                }
                                strConnection = desc.strOvf;

                                mapNetworks[desc.strOvf] = true;
                            }
                        break;

                        case VirtualSystemDescriptionType_USBController:
                            /*  <Item ovf:required="false">
                                    <rasd:Caption>usb</rasd:Caption>
                                    <rasd:Description>USB Controller</rasd:Description>
                                    <rasd:InstanceId>3</rasd:InstanceId>
                                    <rasd:ResourceType>23</rasd:ResourceType>
                                    <rasd:Address>0</rasd:Address>
                                    <rasd:BusNumber>0</rasd:BusNumber>
                                </Item> */
                            if (uLoop == 1)
                            {
                                strDescription = "USB Controller";
                                strCaption = "usb";
                                type = OVFResourceType_USBController; // 23
                                lAddress = 0;                   // this is what OVFTool writes
                                lBusNumber = 0;                 // this is what OVFTool writes
                            }
                        break;

                       case VirtualSystemDescriptionType_SoundCard:
                        /*  <Item ovf:required="false">
                                <rasd:Caption>sound</rasd:Caption>
                                <rasd:Description>Sound Card</rasd:Description>
                                <rasd:InstanceId>10</rasd:InstanceId>
                                <rasd:ResourceType>35</rasd:ResourceType>
                                <rasd:ResourceSubType>ensoniq1371</rasd:ResourceSubType>
                                <rasd:AutomaticAllocation>false</rasd:AutomaticAllocation>
                                <rasd:AddressOnParent>3</rasd:AddressOnParent>
                            </Item> */
                            if (uLoop == 1)
                            {
                                strDescription = "Sound Card";
                                strCaption = "sound";
                                type = OVFResourceType_SoundCard; // 35
                                strResourceSubType = desc.strOvf;       // e.g. ensoniq1371
                                lAutomaticAllocation = 0;
                                lAddressOnParent = 3;               // what gives? this is what OVFTool writes
                            }
                        break;
                    }

                    if (type)
                    {
                        xml::ElementNode *pItem;

                        pItem = pelmVirtualHardwareSection->createChild("Item");

                        // NOTE: do not change the order of these items without good reason! While we don't care
                        // about ordering, VMware's ovftool does and fails if the items are not written in
                        // exactly this order, as stupid as it seems.

                        if (!strCaption.isEmpty())
                        {
                            pItem->createChild("rasd:Caption")->addContent(strCaption);
                            if (pTask->enFormat == TaskExportOVF::OVF_1_0)
                                pItem->createChild("rasd:ElementName")->addContent(strCaption);
                        }

                        if (!strDescription.isEmpty())
                            pItem->createChild("rasd:Description")->addContent(strDescription);

                        // <rasd:InstanceID>1</rasd:InstanceID>
                        xml::ElementNode *pelmInstanceID;
                        if (pTask->enFormat == TaskExportOVF::OVF_0_9)
                            pelmInstanceID = pItem->createChild("rasd:InstanceId");
                        else
                            pelmInstanceID = pItem->createChild("rasd:InstanceID");      // capitalization changed...
                        pelmInstanceID->addContent(Utf8StrFmt("%d", ulInstanceID++));

                        // <rasd:ResourceType>3</rasd:ResourceType>
                        pItem->createChild("rasd:ResourceType")->addContent(Utf8StrFmt("%d", type));
                        if (!strResourceSubType.isEmpty())
                            pItem->createChild("rasd:ResourceSubType")->addContent(strResourceSubType);

                        if (!strHostResource.isEmpty())
                            pItem->createChild("rasd:HostResource")->addContent(strHostResource);

                        if (!strAllocationUnits.isEmpty())
                            pItem->createChild("rasd:AllocationUnits")->addContent(strAllocationUnits);

                        // <rasd:VirtualQuantity>1</rasd:VirtualQuantity>
                        if (lVirtualQuantity != -1)
                            pItem->createChild("rasd:VirtualQuantity")->addContent(Utf8StrFmt("%d", lVirtualQuantity));

                        if (lAutomaticAllocation != -1)
                            pItem->createChild("rasd:AutomaticAllocation")->addContent( (lAutomaticAllocation) ? "true" : "false" );

                        if (!strConnection.isEmpty())
                            pItem->createChild("rasd:Connection")->addContent(strConnection);

                        if (lAddress != -1)
                            pItem->createChild("rasd:Address")->addContent(Utf8StrFmt("%d", lAddress));

                        if (lBusNumber != -1)
                            if (pTask->enFormat == TaskExportOVF::OVF_0_9) // BusNumber is invalid OVF 1.0 so only write it in 0.9 mode for OVFTool compatibility
                                pItem->createChild("rasd:BusNumber")->addContent(Utf8StrFmt("%d", lBusNumber));

                        if (ulParent)
                            pItem->createChild("rasd:Parent")->addContent(Utf8StrFmt("%d", ulParent));
                        if (lAddressOnParent != -1)
                            pItem->createChild("rasd:AddressOnParent")->addContent(Utf8StrFmt("%d", lAddressOnParent));
                    }
                }
            } // for (size_t uLoop = 0; ...
        }

        // finally, fill in the network section we set up empty above according
        // to the networks we found with the hardware items
        map<Utf8Str, bool>::const_iterator itN;
        for (itN = mapNetworks.begin();
             itN != mapNetworks.end();
             ++itN)
        {
            const Utf8Str &strNetwork = itN->first;
            xml::ElementNode *pelmNetwork = pelmNetworkSection->createChild("Network");
            pelmNetwork->setAttribute("ovf:name", strNetwork.c_str());
            pelmNetwork->createChild("Description")->addContent("Logical network used by this appliance.");
        }

        list<Utf8Str> diskList;
        map<Utf8Str, const VirtualSystemDescriptionEntry*>::const_iterator itS;
        uint32_t ulFile = 1;
        for (itS = mapDisks.begin();
             itS != mapDisks.end();
             ++itS)
        {
            const Utf8Str &strDiskID = itS->first;
            const VirtualSystemDescriptionEntry *pDiskEntry = itS->second;

            // source path: where the VBox image is
            const Utf8Str &strSrcFilePath = pDiskEntry->strVbox;
            Bstr bstrSrcFilePath(strSrcFilePath);
            if (!RTPathExists(strSrcFilePath.c_str()))
                /* This isn't allowed */
                throw setError(VBOX_E_FILE_ERROR,
                               tr("Source virtual disk image file '%s' doesn't exist"),
                               strSrcFilePath.c_str());

            // output filename
            const Utf8Str &strTargetFileNameOnly = pDiskEntry->strOvf;
            // target path needs to be composed from where the output OVF is
            Utf8Str strTargetFilePath(pTask->locInfo.strPath);
            strTargetFilePath.stripFilename();
            strTargetFilePath.append("/");
            strTargetFilePath.append(strTargetFileNameOnly);

            // clone the disk:
            ComPtr<IMedium> pSourceDisk;
            ComPtr<IMedium> pTargetDisk;
            ComPtr<IProgress> pProgress2;

            Log(("Finding source disk \"%ls\"\n", bstrSrcFilePath.raw()));
            rc = mVirtualBox->FindHardDisk(bstrSrcFilePath, pSourceDisk.asOutParam());
            if (FAILED(rc)) throw rc;

            /* We are always exporting to vmdfk stream optimized for now */
            Bstr bstrSrcFormat = L"VMDK";

            // create a new hard disk interface for the destination disk image
            Log(("Creating target disk \"%s\"\n", strTargetFilePath.raw()));
            rc = mVirtualBox->CreateHardDisk(bstrSrcFormat, Bstr(strTargetFilePath), pTargetDisk.asOutParam());
            if (FAILED(rc)) throw rc;

            // the target disk is now registered and needs to be removed again,
            // both after successful cloning or if anything goes bad!
            try
            {
                // create a flat copy of the source disk image
                rc = pSourceDisk->CloneTo(pTargetDisk, MediumVariant_VmdkStreamOptimized, NULL, pProgress2.asOutParam());
                if (FAILED(rc)) throw rc;

                // advance to the next operation
                if (!pTask->progress.isNull())
                    pTask->progress->SetNextOperation(BstrFmt(tr("Exporting virtual disk image '%s'"), strSrcFilePath.c_str()),
                                                     pDiskEntry->ulSizeMB);     // operation's weight, as set up with the IProgress originally);

                // now wait for the background disk operation to complete; this throws HRESULTs on error
                waitForAsyncProgress(pTask->progress, pProgress2);
            }
            catch (HRESULT rc3)
            {
                // upon error after registering, close the disk or
                // it'll stick in the registry forever
                pTargetDisk->Close();
                throw rc3;
            }
            diskList.push_back(strTargetFilePath);

            // we need the following for the XML
            uint64_t cbFile = 0;        // actual file size
            rc = pTargetDisk->COMGETTER(Size)(&cbFile);
            if (FAILED(rc)) throw rc;

            ULONG64 cbCapacity = 0;     // size reported to guest
            rc = pTargetDisk->COMGETTER(LogicalSize)(&cbCapacity);
            if (FAILED(rc)) throw rc;
            // capacity is reported in megabytes, so...
            cbCapacity *= _1M;

            // upon success, close the disk as well
            rc = pTargetDisk->Close();
            if (FAILED(rc)) throw rc;

            // now handle the XML for the disk:
            Utf8StrFmt strFileRef("file%RI32", ulFile++);
            // <File ovf:href="WindowsXpProfessional-disk1.vmdk" ovf:id="file1" ovf:size="1710381056"/>
            xml::ElementNode *pelmFile = pelmReferences->createChild("File");
            pelmFile->setAttribute("ovf:href", strTargetFileNameOnly);
            pelmFile->setAttribute("ovf:id", strFileRef);
            pelmFile->setAttribute("ovf:size", Utf8StrFmt("%RI64", cbFile).c_str());

            // add disk to XML Disks section
            // <Disk ovf:capacity="8589934592" ovf:diskId="vmdisk1" ovf:fileRef="file1" ovf:format="http://www.vmware.com/specifications/vmdk.html#sparse"/>
            xml::ElementNode *pelmDisk = pelmDiskSection->createChild("Disk");
            pelmDisk->setAttribute("ovf:capacity", Utf8StrFmt("%RI64", cbCapacity).c_str());
            pelmDisk->setAttribute("ovf:diskId", strDiskID);
            pelmDisk->setAttribute("ovf:fileRef", strFileRef);
            pelmDisk->setAttribute("ovf:format", "http://www.vmware.com/specifications/vmdk.html#sparse");      // must be sparse or ovftool chokes
        }

        // now go write the XML
        xml::XmlFileWriter writer(doc);
        writer.write(pTask->locInfo.strPath.c_str());

        /* Create & write the manifest file */
        const char** ppManifestFiles = (const char**)RTMemAlloc(sizeof(char*)*diskList.size() + 1);
        ppManifestFiles[0] = pTask->locInfo.strPath.c_str();
        list<Utf8Str>::const_iterator it1;
        size_t i = 1;
        for (it1 = diskList.begin();
             it1 != diskList.end();
             ++it1, ++i)
            ppManifestFiles[i] = (*it1).c_str();
        Utf8Str strMfFile = manifestFileName(pTask->locInfo.strPath.c_str());
        int vrc = RTManifestWriteFiles(strMfFile.c_str(), ppManifestFiles, diskList.size()+1);
        if (RT_FAILURE(vrc))
            throw setError(VBOX_E_FILE_ERROR,
                           tr("Couldn't create manifest file '%s' (%Rrc)"),
                           RTPathFilename(strMfFile.c_str()), vrc);
        RTMemFree(ppManifestFiles);
    }
    catch(xml::Error &x)
    {
        rc = setError(VBOX_E_FILE_ERROR,
                      x.what());
    }
    catch(HRESULT aRC)
    {
        rc = aRC;
    }

    pTask->rc = rc;

    if (!pTask->progress.isNull())
        pTask->progress->notifyComplete(rc);

    LogFlowFunc(("rc=%Rhrc\n", rc));
    LogFlowFuncLeave();

    return VINF_SUCCESS;
}

int Appliance::writeS3(TaskExportOVF *pTask)
{
    LogFlowFuncEnter();
    LogFlowFunc(("Appliance %p\n", this));

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    HRESULT rc = S_OK;

    AutoWriteLock appLock(this);

    int vrc = VINF_SUCCESS;
    RTS3 hS3 = NIL_RTS3;
    char szOSTmpDir[RTPATH_MAX];
    RTPathTemp(szOSTmpDir, sizeof(szOSTmpDir));
    /* The template for the temporary directory created below */
    char *pszTmpDir;
    RTStrAPrintf(&pszTmpDir, "%s"RTPATH_SLASH_STR"vbox-ovf-XXXXXX", szOSTmpDir);
    list< pair<Utf8Str, ULONG> > filesList;

    // todo:
    // - usable error codes
    // - seems snapshot filenames are problematic {uuid}.vdi
    try
    {
        /* Extract the bucket */
        Utf8Str tmpPath = pTask->locInfo.strPath;
        Utf8Str bucket;
        parseBucket(tmpPath, bucket);

        /* We need a temporary directory which we can put the OVF file & all
         * disk images in */
        vrc = RTDirCreateTemp(pszTmpDir);
        if (RT_FAILURE(vrc))
            throw setError(VBOX_E_FILE_ERROR,
                           tr("Cannot create temporary directory '%s'"), pszTmpDir);

        /* The temporary name of the target OVF file */
        Utf8StrFmt strTmpOvf("%s/%s", pszTmpDir, RTPathFilename(tmpPath.c_str()));

        /* Prepare the temporary writing of the OVF */
        ComObjPtr<Progress> progress;
        /* Create a temporary file based location info for the sub task */
        LocationInfo li;
        li.strPath = strTmpOvf;
        rc = writeImpl(pTask->enFormat, li, progress);
        if (FAILED(rc)) throw rc;

        /* Unlock the appliance for the writing thread */
        appLock.unlock();
        /* Wait until the writing is done, but report the progress back to the
           caller */
        ComPtr<IProgress> progressInt(progress);
        waitForAsyncProgress(pTask->progress, progressInt); /* Any errors will be thrown */

        /* Again lock the appliance for the next steps */
        appLock.lock();

        vrc = RTPathExists(strTmpOvf.c_str()); /* Paranoid check */
        if(RT_FAILURE(vrc))
            throw setError(VBOX_E_FILE_ERROR,
                           tr("Cannot find source file '%s'"), strTmpOvf.c_str());
        /* Add the OVF file */
        filesList.push_back(pair<Utf8Str, ULONG>(strTmpOvf, m->ulWeightPerOperation)); /* Use 1% of the total for the OVF file upload */
        Utf8Str strMfFile = manifestFileName(strTmpOvf);
        filesList.push_back(pair<Utf8Str, ULONG>(strMfFile , m->ulWeightPerOperation)); /* Use 1% of the total for the manifest file upload */

        /* Now add every disks of every virtual system */
        list< ComObjPtr<VirtualSystemDescription> >::const_iterator it;
        for (it = m->virtualSystemDescriptions.begin();
             it != m->virtualSystemDescriptions.end();
             ++it)
        {
            ComObjPtr<VirtualSystemDescription> vsdescThis = (*it);
            std::list<VirtualSystemDescriptionEntry*> avsdeHDs = vsdescThis->findByType(VirtualSystemDescriptionType_HardDiskImage);
            std::list<VirtualSystemDescriptionEntry*>::const_iterator itH;
            for (itH = avsdeHDs.begin();
                 itH != avsdeHDs.end();
                 ++itH)
            {
                const Utf8Str &strTargetFileNameOnly = (*itH)->strOvf;
                /* Target path needs to be composed from where the output OVF is */
                Utf8Str strTargetFilePath(strTmpOvf);
                strTargetFilePath.stripFilename();
                strTargetFilePath.append("/");
                strTargetFilePath.append(strTargetFileNameOnly);
                vrc = RTPathExists(strTargetFilePath.c_str()); /* Paranoid check */
                if(RT_FAILURE(vrc))
                    throw setError(VBOX_E_FILE_ERROR,
                                   tr("Cannot find source file '%s'"), strTargetFilePath.c_str());
                filesList.push_back(pair<Utf8Str, ULONG>(strTargetFilePath, (*itH)->ulSizeMB));
            }
        }
        /* Next we have to upload the OVF & all disk images */
        vrc = RTS3Create(&hS3, pTask->locInfo.strUsername.c_str(), pTask->locInfo.strPassword.c_str(), pTask->locInfo.strHostname.c_str(), "virtualbox-agent/"VBOX_VERSION_STRING);
        if(RT_FAILURE(vrc))
            throw setError(VBOX_E_IPRT_ERROR,
                           tr("Cannot create S3 service handler"));
        RTS3SetProgressCallback(hS3, pTask->updateProgress, &pTask);

        /* Upload all files */
        for (list< pair<Utf8Str, ULONG> >::const_iterator it1 = filesList.begin(); it1 != filesList.end(); ++it1)
        {
            const pair<Utf8Str, ULONG> &s = (*it1);
            char *pszFilename = RTPathFilename(s.first.c_str());
            /* Advance to the next operation */
            if (!pTask->progress.isNull())
                pTask->progress->SetNextOperation(BstrFmt(tr("Uploading file '%s'"), pszFilename), s.second);
            vrc = RTS3PutKey(hS3, bucket.c_str(), pszFilename, s.first.c_str());
            if (RT_FAILURE(vrc))
            {
                if(vrc == VERR_S3_CANCELED)
                    break;
                else if(vrc == VERR_S3_ACCESS_DENIED)
                    throw setError(E_ACCESSDENIED,
                                   tr("Cannot upload file '%s' to S3 storage server (Access denied). Make sure that your credentials are right. Also check that your host clock is properly synced"), pszFilename);
                else if(vrc == VERR_S3_NOT_FOUND)
                    throw setError(VBOX_E_FILE_ERROR,
                                   tr("Cannot upload file '%s' to S3 storage server (File not found)"), pszFilename);
                else
                    throw setError(VBOX_E_IPRT_ERROR,
                                   tr("Cannot upload file '%s' to S3 storage server (%Rrc)"), pszFilename, vrc);
            }
        }
    }
    catch(HRESULT aRC)
    {
        rc = aRC;
    }
    /* Cleanup */
    RTS3Destroy(hS3);
    /* Delete all files which where temporary created */
    for (list< pair<Utf8Str, ULONG> >::const_iterator it1 = filesList.begin(); it1 != filesList.end(); ++it1)
    {
        const char *pszFilePath = (*it1).first.c_str();
        if (RTPathExists(pszFilePath))
        {
            vrc = RTFileDelete(pszFilePath);
            if(RT_FAILURE(vrc))
                rc = setError(VBOX_E_FILE_ERROR,
                              tr("Cannot delete file '%s' (%Rrc)"), pszFilePath, vrc);
        }
    }
    /* Delete the temporary directory */
    if (RTPathExists(pszTmpDir))
    {
        vrc = RTDirRemove(pszTmpDir);
        if(RT_FAILURE(vrc))
            rc = setError(VBOX_E_FILE_ERROR,
                          tr("Cannot delete temporary directory '%s' (%Rrc)"), pszTmpDir, vrc);
    }
    if (pszTmpDir)
        RTStrFree(pszTmpDir);

    pTask->rc = rc;

    if (!pTask->progress.isNull())
        pTask->progress->notifyComplete(rc);

    LogFlowFunc(("rc=%Rhrc\n", rc));
    LogFlowFuncLeave();

    return VINF_SUCCESS;
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
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

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
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

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
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    SafeIfaceArray<IVirtualSystemDescription> sfaVSD(m->virtualSystemDescriptions);
    sfaVSD.detachTo(ComSafeArrayOutArg(aVirtualSystemDescriptions));

    return S_OK;
}

/**
 * Public method implementation.
 * @param path
 * @return
 */
STDMETHODIMP Appliance::Read(IN_BSTR path, IProgress **aProgress)
{
    if (!path) return E_POINTER;
    CheckComArgOutPointerValid(aProgress);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    if (m->pReader)
 	{
 	    delete m->pReader;
 	    m->pReader = NULL;
 	}

    // see if we can handle this file; for now we insist it has an ".ovf" extension
    Utf8Str strPath (path);
    if (!strPath.endsWith(".ovf", Utf8Str::CaseInsensitive))
        return setError(VBOX_E_FILE_ERROR,
                        tr("Appliance file must have .ovf extension"));

    ComObjPtr<Progress> progress;
    HRESULT rc = S_OK;
    try
    {
        /* Parse all necessary info out of the URI */
        parseURI(strPath, m->locInfo);
        rc = readImpl(m->locInfo, progress);
    }
    catch (HRESULT aRC)
    {
        rc = aRC;
    }

    if (SUCCEEDED(rc))
        /* Return progress to the caller */
        progress.queryInterfaceTo(aProgress);

    return S_OK;
}

/**
 * Public method implementation.
 * @return
 */
STDMETHODIMP Appliance::Interpret()
{
    // @todo:
    //  - don't use COM methods but the methods directly (faster, but needs appropriate locking of that objects itself (s. HardDisk))
    //  - Appropriate handle errors like not supported file formats
    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock(this);

    HRESULT rc = S_OK;

    /* Clear any previous virtual system descriptions */
    m->virtualSystemDescriptions.clear();

    /* We need the default path for storing disk images */
    ComPtr<ISystemProperties> systemProps;
    rc = mVirtualBox->COMGETTER(SystemProperties)(systemProps.asOutParam());
    CheckComRCReturnRC(rc);
    Bstr bstrDefaultHardDiskLocation;
    rc = systemProps->COMGETTER(DefaultHardDiskFolder)(bstrDefaultHardDiskLocation.asOutParam());
    CheckComRCReturnRC(rc);

    if (!m->pReader)
        return setError(E_FAIL,
                        tr("Cannot interpret appliance without reading it first (call read() before interpret())"));

    /* Try/catch so we can clean up on error */
    try
    {
        list<VirtualSystem>::const_iterator it;
        /* Iterate through all virtual systems */
        for (it = m->pReader->m_llVirtualSystems.begin();
             it != m->pReader->m_llVirtualSystems.end();
             ++it)
        {
            const VirtualSystem &vsysThis = *it;

            ComObjPtr<VirtualSystemDescription> pNewDesc;
            rc = pNewDesc.createObject();
            CheckComRCThrowRC(rc);
            rc = pNewDesc->init();
            CheckComRCThrowRC(rc);

            /* Guest OS type */
            Utf8Str strOsTypeVBox,
                    strCIMOSType = Utf8StrFmt("%RI32", (uint32_t)vsysThis.cimos);
            convertCIMOSType2VBoxOSType(strOsTypeVBox, vsysThis.cimos, vsysThis.strCimosDesc);
            pNewDesc->addEntry(VirtualSystemDescriptionType_OS,
                               "",
                               strCIMOSType,
                               strOsTypeVBox);

            /* VM name */
            /* If the there isn't any name specified create a default one out of
             * the OS type */
            Utf8Str nameVBox = vsysThis.strName;
            if (nameVBox.isEmpty())
                nameVBox = strOsTypeVBox;
            searchUniqueVMName(nameVBox);
            pNewDesc->addEntry(VirtualSystemDescriptionType_Name,
                               "",
                               vsysThis.strName,
                               nameVBox);

            /* VM Product */
            if (!vsysThis.strProduct.isEmpty())
                pNewDesc->addEntry(VirtualSystemDescriptionType_Product,
                                    "",
                                    vsysThis.strProduct,
                                    vsysThis.strProduct);

            /* VM Vendor */
            if (!vsysThis.strVendor.isEmpty())
                pNewDesc->addEntry(VirtualSystemDescriptionType_Vendor,
                                    "",
                                    vsysThis.strVendor,
                                    vsysThis.strVendor);

            /* VM Version */
            if (!vsysThis.strVersion.isEmpty())
                pNewDesc->addEntry(VirtualSystemDescriptionType_Version,
                                    "",
                                    vsysThis.strVersion,
                                    vsysThis.strVersion);

            /* VM ProductUrl */
            if (!vsysThis.strProductUrl.isEmpty())
                pNewDesc->addEntry(VirtualSystemDescriptionType_ProductUrl,
                                    "",
                                    vsysThis.strProductUrl,
                                    vsysThis.strProductUrl);

            /* VM VendorUrl */
            if (!vsysThis.strVendorUrl.isEmpty())
                pNewDesc->addEntry(VirtualSystemDescriptionType_VendorUrl,
                                    "",
                                    vsysThis.strVendorUrl,
                                    vsysThis.strVendorUrl);

            /* VM description */
            if (!vsysThis.strDescription.isEmpty())
                pNewDesc->addEntry(VirtualSystemDescriptionType_Description,
                                    "",
                                    vsysThis.strDescription,
                                    vsysThis.strDescription);

            /* VM license */
            if (!vsysThis.strLicenseText.isEmpty())
                pNewDesc->addEntry(VirtualSystemDescriptionType_License,
                                    "",
                                    vsysThis.strLicenseText,
                                    vsysThis.strLicenseText);

            /* Now that we know the OS type, get our internal defaults based on that. */
            ComPtr<IGuestOSType> pGuestOSType;
            rc = mVirtualBox->GetGuestOSType(Bstr(strOsTypeVBox), pGuestOSType.asOutParam());
            CheckComRCThrowRC(rc);

            /* CPU count */
            ULONG cpuCountVBox = vsysThis.cCPUs;
            /* Check for the constrains */
            if (cpuCountVBox > SchemaDefs::MaxCPUCount)
            {
                addWarning(tr("The virtual system \"%s\" claims support for %u CPU's, but VirtualBox has support for max %u CPU's only."),
                           vsysThis.strName.c_str(), cpuCountVBox, SchemaDefs::MaxCPUCount);
                cpuCountVBox = SchemaDefs::MaxCPUCount;
            }
            if (vsysThis.cCPUs == 0)
                cpuCountVBox = 1;
            pNewDesc->addEntry(VirtualSystemDescriptionType_CPU,
                               "",
                               Utf8StrFmt("%RI32", (uint32_t)vsysThis.cCPUs),
                               Utf8StrFmt("%RI32", (uint32_t)cpuCountVBox));

            /* RAM */
            uint64_t ullMemSizeVBox = vsysThis.ullMemorySize / _1M;
            /* Check for the constrains */
            if (ullMemSizeVBox != 0 &&
                (ullMemSizeVBox < MM_RAM_MIN_IN_MB ||
                 ullMemSizeVBox > MM_RAM_MAX_IN_MB))
            {
                addWarning(tr("The virtual system \"%s\" claims support for %llu MB RAM size, but VirtualBox has support for min %u & max %u MB RAM size only."),
                              vsysThis.strName.c_str(), ullMemSizeVBox, MM_RAM_MIN_IN_MB, MM_RAM_MAX_IN_MB);
                ullMemSizeVBox = RT_MIN(RT_MAX(ullMemSizeVBox, MM_RAM_MIN_IN_MB), MM_RAM_MAX_IN_MB);
            }
            if (vsysThis.ullMemorySize == 0)
            {
                /* If the RAM of the OVF is zero, use our predefined values */
                ULONG memSizeVBox2;
                rc = pGuestOSType->COMGETTER(RecommendedRAM)(&memSizeVBox2);
                CheckComRCThrowRC(rc);
                /* VBox stores that in MByte */
                ullMemSizeVBox = (uint64_t)memSizeVBox2;
            }
            pNewDesc->addEntry(VirtualSystemDescriptionType_Memory,
                               "",
                               Utf8StrFmt("%RI64", (uint64_t)vsysThis.ullMemorySize),
                               Utf8StrFmt("%RI64", (uint64_t)ullMemSizeVBox));

            /* Audio */
            if (!vsysThis.strSoundCardType.isEmpty())
                /* Currently we set the AC97 always.
                   @todo: figure out the hardware which could be possible */
                pNewDesc->addEntry(VirtualSystemDescriptionType_SoundCard,
                                   "",
                                   vsysThis.strSoundCardType,
                                   Utf8StrFmt("%RI32", (uint32_t)AudioControllerType_AC97));

#ifdef VBOX_WITH_USB
            /* USB Controller */
            if (vsysThis.fHasUsbController)
                pNewDesc->addEntry(VirtualSystemDescriptionType_USBController, "", "", "");
#endif /* VBOX_WITH_USB */

            /* Network Controller */
            size_t cEthernetAdapters = vsysThis.llEthernetAdapters.size();
            if (cEthernetAdapters > 0)
            {
                /* Check for the constrains */
                if (cEthernetAdapters > SchemaDefs::NetworkAdapterCount)
                    addWarning(tr("The virtual system \"%s\" claims support for %zu network adapters, but VirtualBox has support for max %u network adapter only."),
                                  vsysThis.strName.c_str(), cEthernetAdapters, SchemaDefs::NetworkAdapterCount);

                /* Get the default network adapter type for the selected guest OS */
                NetworkAdapterType_T defaultAdapterVBox = NetworkAdapterType_Am79C970A;
                rc = pGuestOSType->COMGETTER(AdapterType)(&defaultAdapterVBox);
                CheckComRCThrowRC(rc);

                EthernetAdaptersList::const_iterator itEA;
                /* Iterate through all abstract networks. We support 8 network
                 * adapters at the maximum, so the first 8 will be added only. */
                size_t a = 0;
                for (itEA = vsysThis.llEthernetAdapters.begin();
                     itEA != vsysThis.llEthernetAdapters.end() && a < SchemaDefs::NetworkAdapterCount;
                     ++itEA, ++a)
                {
                    const EthernetAdapter &ea = *itEA; // logical network to connect to
                    Utf8Str strNetwork = ea.strNetworkName;
                    // make sure it's one of these two
                    if (    (strNetwork.compare("Null", Utf8Str::CaseInsensitive))
                         && (strNetwork.compare("NAT", Utf8Str::CaseInsensitive))
                         && (strNetwork.compare("Bridged", Utf8Str::CaseInsensitive))
                         && (strNetwork.compare("Internal", Utf8Str::CaseInsensitive))
                         && (strNetwork.compare("HostOnly", Utf8Str::CaseInsensitive))
                       )
                        strNetwork = "Bridged";     // VMware assumes this is the default apparently

                    /* Figure out the hardware type */
                    NetworkAdapterType_T nwAdapterVBox = defaultAdapterVBox;
                    if (!ea.strAdapterType.compare("PCNet32", Utf8Str::CaseInsensitive))
                    {
                        /* If the default adapter is already one of the two
                         * PCNet adapters use the default one. If not use the
                         * Am79C970A as fallback. */
                        if (!(defaultAdapterVBox == NetworkAdapterType_Am79C970A ||
                              defaultAdapterVBox == NetworkAdapterType_Am79C973))
                            nwAdapterVBox = NetworkAdapterType_Am79C970A;
                    }
#ifdef VBOX_WITH_E1000
                    /* VMWare accidentally write this with VirtualCenter 3.5,
                       so make sure in this case always to use the VMWare one */
                    else if (!ea.strAdapterType.compare("E10000", Utf8Str::CaseInsensitive))
                        nwAdapterVBox = NetworkAdapterType_I82545EM;
                    else if (!ea.strAdapterType.compare("E1000", Utf8Str::CaseInsensitive))
                    {
                        /* Check if this OVF was written by VirtualBox */
                        if (Utf8Str(vsysThis.strVirtualSystemType).contains("virtualbox", Utf8Str::CaseInsensitive))
                        {
                            /* If the default adapter is already one of the three
                             * E1000 adapters use the default one. If not use the
                             * I82545EM as fallback. */
                            if (!(defaultAdapterVBox == NetworkAdapterType_I82540EM ||
                                  defaultAdapterVBox == NetworkAdapterType_I82543GC ||
                                  defaultAdapterVBox == NetworkAdapterType_I82545EM))
                            nwAdapterVBox = NetworkAdapterType_I82540EM;
                        }
                        else
                            /* Always use this one since it's what VMware uses */
                            nwAdapterVBox = NetworkAdapterType_I82545EM;
                    }
#endif /* VBOX_WITH_E1000 */

                    pNewDesc->addEntry(VirtualSystemDescriptionType_NetworkAdapter,
                                       "",      // ref
                                       ea.strNetworkName,      // orig
                                       Utf8StrFmt("%RI32", (uint32_t)nwAdapterVBox),   // conf
                                       0,
                                       Utf8StrFmt("type=%s", strNetwork.c_str()));       // extra conf
                }
            }

            /* Floppy Drive */
            if (vsysThis.fHasFloppyDrive)
                pNewDesc->addEntry(VirtualSystemDescriptionType_Floppy, "", "", "");

            /* CD Drive */
            /* @todo: I can't disable the CDROM. So nothing to do for now */
            /*
            if (vsysThis.fHasCdromDrive)
                pNewDesc->addEntry(VirtualSystemDescriptionType_CDROM, "", "", "");*/

            /* Hard disk Controller */
            uint16_t cIDEused = 0;
            uint16_t cSATAused = 0; NOREF(cSATAused);
            uint16_t cSCSIused = 0; NOREF(cSCSIused);
            ControllersMap::const_iterator hdcIt;
            /* Iterate through all hard disk controllers */
            for (hdcIt = vsysThis.mapControllers.begin();
                 hdcIt != vsysThis.mapControllers.end();
                 ++hdcIt)
            {
                const HardDiskController &hdc = hdcIt->second;
                Utf8Str strControllerID = Utf8StrFmt("%RI32", (uint32_t)hdc.idController);

                switch (hdc.system)
                {
                    case HardDiskController::IDE:
                        {
                            /* Check for the constrains */
                            /* @todo: I'm very confused! Are these bits *one* controller or
                               is every port/bus declared as an extra controller. */
                            if (cIDEused < 4)
                            {
                                // @todo: figure out the IDE types
                                /* Use PIIX4 as default */
                                Utf8Str strType = "PIIX4";
                                if (!hdc.strControllerType.compare("PIIX3", Utf8Str::CaseInsensitive))
                                    strType = "PIIX3";
                                else if (!hdc.strControllerType.compare("ICH6", Utf8Str::CaseInsensitive))
                                    strType = "ICH6";
                                pNewDesc->addEntry(VirtualSystemDescriptionType_HardDiskControllerIDE,
                                                   strControllerID,
                                                   hdc.strControllerType,
                                                   strType);
                            }
                            else
                            {
                                /* Warn only once */
                                if (cIDEused == 1)
                                    addWarning(tr("The virtual \"%s\" system requests support for more than one IDE controller, but VirtualBox has support for only one."),
                                               vsysThis.strName.c_str());

                            }
                            ++cIDEused;
                            break;
                        }

                    case HardDiskController::SATA:
                        {
#ifdef VBOX_WITH_AHCI
                            /* Check for the constrains */
                            if (cSATAused < 1)
                            {
                                // @todo: figure out the SATA types
                                /* We only support a plain AHCI controller, so use them always */
                                pNewDesc->addEntry(VirtualSystemDescriptionType_HardDiskControllerSATA,
                                                   strControllerID,
                                                   hdc.strControllerType,
                                                   "AHCI");
                            }
                            else
                            {
                                /* Warn only once */
                                if (cSATAused == 1)
                                    addWarning(tr("The virtual system \"%s\" requests support for more than one SATA controller, but VirtualBox has support for only one"),
                                               vsysThis.strName.c_str());

                            }
                            ++cSATAused;
                            break;
#else /* !VBOX_WITH_AHCI */
                            addWarning(tr("The virtual system \"%s\" requests at least one SATA controller but this version of VirtualBox does not provide a SATA controller emulation"),
                                      vsysThis.strName.c_str());
#endif /* !VBOX_WITH_AHCI */
                        }

                    case HardDiskController::SCSI:
                        {
#ifdef VBOX_WITH_LSILOGIC
                            /* Check for the constrains */
                            if (cSCSIused < 1)
                            {
                                Utf8Str hdcController = "LsiLogic";
                                if (!hdc.strControllerType.compare("BusLogic", Utf8Str::CaseInsensitive))
                                    hdcController = "BusLogic";
                                pNewDesc->addEntry(VirtualSystemDescriptionType_HardDiskControllerSCSI,
                                                   strControllerID,
                                                   hdc.strControllerType,
                                                   hdcController);
                            }
                            else
                                addWarning(tr("The virtual system \"%s\" requests support for an additional SCSI controller of type \"%s\" with ID %s, but VirtualBox presently supports only one SCSI controller."),
                                           vsysThis.strName.c_str(),
                                           hdc.strControllerType.c_str(),
                                           strControllerID.c_str());
                            ++cSCSIused;
                            break;
#else /* !VBOX_WITH_LSILOGIC */
                            addWarning(tr("The virtual system \"%s\" requests at least one SATA controller but this version of VirtualBox does not provide a SCSI controller emulation"),
                                       vsysThis.strName.c_str());
#endif /* !VBOX_WITH_LSILOGIC */
                        }
                }
            }

            /* Hard disks */
            if (vsysThis.mapVirtualDisks.size() > 0)
            {
                VirtualDisksMap::const_iterator itVD;
                /* Iterate through all hard disks ()*/
                for (itVD = vsysThis.mapVirtualDisks.begin();
                     itVD != vsysThis.mapVirtualDisks.end();
                     ++itVD)
                {
                    const VirtualDisk &hd = itVD->second;
                    /* Get the associated disk image */
                    const DiskImage &di = m->pReader->m_mapDisks[hd.strDiskId];

                    // @todo:
                    //  - figure out all possible vmdk formats we also support
                    //  - figure out if there is a url specifier for vhd already
                    //  - we need a url specifier for the vdi format
                    if (   di.strFormat.compare("http://www.vmware.com/specifications/vmdk.html#sparse", Utf8Str::CaseInsensitive)
                        || di.strFormat.compare("http://www.vmware.com/specifications/vmdk.html#compressed", Utf8Str::CaseInsensitive))
                    {
                        /* If the href is empty use the VM name as filename */
                        Utf8Str strFilename = di.strHref;
                        if (!strFilename.length())
                            strFilename = Utf8StrFmt("%s.vmdk", nameVBox.c_str());
                        /* Construct a unique target path */
                        Utf8StrFmt strPath("%ls%c%s",
                                           bstrDefaultHardDiskLocation.raw(),
                                           RTPATH_DELIMITER,
                                           strFilename.c_str());
                        searchUniqueDiskImageFilePath(strPath);

                        /* find the description for the hard disk controller
                         * that has the same ID as hd.idController */
                        const VirtualSystemDescriptionEntry *pController;
                        if (!(pController = pNewDesc->findControllerFromID(hd.idController)))
                            throw setError(E_FAIL,
                                           tr("Cannot find hard disk controller with OVF instance ID %RI32 to which disk \"%s\" should be attached"),
                                           hd.idController,
                                           di.strHref.c_str());

                        /* controller to attach to, and the bus within that controller */
                        Utf8StrFmt strExtraConfig("controller=%RI16;channel=%RI16",
                                                  pController->ulIndex,
                                                  hd.ulAddressOnParent);
                        ULONG ulSize = 0;
                        if (di.iCapacity != -1)
                            ulSize = (ULONG)(di.iCapacity / _1M);
                        else if (di.iPopulatedSize != -1)
                            ulSize = (ULONG)(di.iPopulatedSize / _1M);
                        else if (di.iSize != -1)
                            ulSize = (ULONG)(di.iSize / _1M);
                        if (ulSize == 0)
                            ulSize = 10000;         // assume 10 GB, this is for the progress bar only anyway
                        pNewDesc->addEntry(VirtualSystemDescriptionType_HardDiskImage,
                                           hd.strDiskId,
                                           di.strHref,
                                           strPath,
                                           ulSize,
                                           strExtraConfig);
                    }
                    else
                        throw setError(VBOX_E_FILE_ERROR,
                                       tr("Unsupported format for virtual disk image in OVF: \"%s\"", di.strFormat.c_str()));
                }
            }

            m->virtualSystemDescriptions.push_back(pNewDesc);
        }
    }
    catch (HRESULT aRC)
    {
        /* On error we clear the list & return */
        m->virtualSystemDescriptions.clear();
        rc = aRC;
    }

    return rc;
}

/**
 * Public method implementation.
 * @param aProgress
 * @return
 */
STDMETHODIMP Appliance::ImportMachines(IProgress **aProgress)
{
    CheckComArgOutPointerValid(aProgress);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock(this);

    if (!m->pReader)
        return setError(E_FAIL,
                        tr("Cannot import machines without reading it first (call read() before importMachines())"));

    ComObjPtr<Progress> progress;
    HRESULT rc = S_OK;
    try
    {
        rc = importImpl(m->locInfo, progress);
    }
    catch (HRESULT aRC)
    {
        rc = aRC;
    }

    if (SUCCEEDED(rc))
        /* Return progress to the caller */
        progress.queryInterfaceTo(aProgress);

    return rc;
}

STDMETHODIMP Appliance::CreateVFSExplorer(IN_BSTR aURI, IVFSExplorer **aExplorer)
{
    CheckComArgOutPointerValid(aExplorer);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock(this);

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

STDMETHODIMP Appliance::Write(IN_BSTR format, IN_BSTR path, IProgress **aProgress)
{
    if (!path) return E_POINTER;
    CheckComArgOutPointerValid(aProgress);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock(this);

    // see if we can handle this file; for now we insist it has an ".ovf" extension
    Utf8Str strPath = path;
    if (!strPath.endsWith(".ovf", Utf8Str::CaseInsensitive))
        return setError(VBOX_E_FILE_ERROR,
                        tr("Appliance file must have .ovf extension"));

    Utf8Str strFormat(format);
    TaskExportOVF::OVFFormat ovfF;
    if (strFormat == "ovf-0.9")
        ovfF = TaskExportOVF::OVF_0_9;
    else if (strFormat == "ovf-1.0")
        ovfF = TaskExportOVF::OVF_1_0;
    else
        return setError(VBOX_E_FILE_ERROR,
                        tr("Invalid format \"%s\" specified"), strFormat.c_str());

    ComObjPtr<Progress> progress;
    HRESULT rc = S_OK;
    try
    {
        /* Parse all necessary info out of the URI */
        parseURI(strPath, m->locInfo);
        rc = writeImpl(ovfF, m->locInfo, progress);
    }
    catch (HRESULT aRC)
    {
        rc = aRC;
    }

    if (SUCCEEDED(rc))
        /* Return progress to the caller */
        progress.queryInterfaceTo(aProgress);

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
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

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
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

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
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

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
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

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
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

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
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

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
    CheckComArgNotNull(aVboxValue);
    CheckComArgNotNull(aExtraConfigValue);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

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

////////////////////////////////////////////////////////////////////////////////
//
// IMachine public methods
//
////////////////////////////////////////////////////////////////////////////////

// This code is here so we won't have to include the appliance headers in the
// IMachine implementation, and we also need to access private appliance data.

/**
* Public method implementation.
* @param appliance
* @return
*/

STDMETHODIMP Machine::Export(IAppliance *aAppliance, IVirtualSystemDescription **aDescription)
{
    HRESULT rc = S_OK;

    if (!aAppliance)
        return E_POINTER;

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    ComObjPtr<VirtualSystemDescription> pNewDesc;

    try
    {
        Bstr bstrName;
        Bstr bstrDescription;
        Bstr bstrGuestOSType;
        uint32_t cCPUs;
        uint32_t ulMemSizeMB;
        BOOL fDVDEnabled = FALSE;
        BOOL fFloppyEnabled = FALSE;
        BOOL fUSBEnabled;
        BOOL fAudioEnabled;
        AudioControllerType_T audioController;

        ComPtr<IUSBController> pUsbController;
        ComPtr<IAudioAdapter> pAudioAdapter;

        // get name
        bstrName = mUserData->mName;
        // get description
        bstrDescription = mUserData->mDescription;
        // get guest OS
        bstrGuestOSType = mUserData->mOSTypeId;
        // CPU count
        cCPUs = mHWData->mCPUCount;
        // memory size in MB
        ulMemSizeMB = mHWData->mMemorySize;
        // VRAM size?
        // BIOS settings?
        // 3D acceleration enabled?
        // hardware virtualization enabled?
        // nested paging enabled?
        // HWVirtExVPIDEnabled?
        // PAEEnabled?
        // snapshotFolder?
        // VRDPServer?

/// @todo FIXME // @todo mediumbranch
#if 0
        // floppy
        rc = mFloppyDrive->COMGETTER(Enabled)(&fFloppyEnabled);
        if (FAILED(rc)) throw rc;

        // CD-ROM ?!?
        // ComPtr<IDVDDrive> pDVDDrive;
        fDVDEnabled = 1;
#endif

        // this is more tricky so use the COM method
        rc = COMGETTER(USBController)(pUsbController.asOutParam());
        if (FAILED(rc))
            fUSBEnabled = false;
        else
            rc = pUsbController->COMGETTER(Enabled)(&fUSBEnabled);

        pAudioAdapter = mAudioAdapter;
        rc = pAudioAdapter->COMGETTER(Enabled)(&fAudioEnabled);
        if (FAILED(rc)) throw rc;
        rc = pAudioAdapter->COMGETTER(AudioController)(&audioController);
        if (FAILED(rc)) throw rc;

        // create a new virtual system
        rc = pNewDesc.createObject();
        CheckComRCThrowRC(rc);
        rc = pNewDesc->init();
        CheckComRCThrowRC(rc);

        /* Guest OS type */
        Utf8Str strOsTypeVBox(bstrGuestOSType);
        CIMOSType_T cim = convertVBoxOSType2CIMOSType(strOsTypeVBox.c_str());
        pNewDesc->addEntry(VirtualSystemDescriptionType_OS,
                           "",
                           Utf8StrFmt("%RI32", cim),
                           strOsTypeVBox);

        /* VM name */
        Utf8Str strVMName(bstrName);
        pNewDesc->addEntry(VirtualSystemDescriptionType_Name,
                           "",
                           strVMName,
                           strVMName);

        // description
        Utf8Str strDescription(bstrDescription);
        pNewDesc->addEntry(VirtualSystemDescriptionType_Description,
                           "",
                           strDescription,
                           strDescription);

        /* CPU count*/
        Utf8Str strCpuCount = Utf8StrFmt("%RI32", cCPUs);
        pNewDesc->addEntry(VirtualSystemDescriptionType_CPU,
                           "",
                           strCpuCount,
                           strCpuCount);

        /* Memory */
        Utf8Str strMemory = Utf8StrFmt("%RI32", (uint64_t)ulMemSizeMB * _1M);
        pNewDesc->addEntry(VirtualSystemDescriptionType_Memory,
                           "",
                           strMemory,
                           strMemory);

        int32_t lIDEControllerIndex = 0;
        int32_t lSATAControllerIndex = 0;
        int32_t lSCSIControllerIndex = 0;

//     <const name="HardDiskControllerIDE" value="6" />
        ComPtr<IStorageController> pController;
        rc = GetStorageControllerByName(Bstr("IDE Controller"), pController.asOutParam());
        if (FAILED(rc)) throw rc;
        Utf8Str strVbox;
        StorageControllerType_T ctlr;
        rc = pController->COMGETTER(ControllerType)(&ctlr);
        if (FAILED(rc)) throw rc;
        switch(ctlr)
        {
            case StorageControllerType_PIIX3: strVbox = "PIIX3"; break;
            case StorageControllerType_PIIX4: strVbox = "PIIX4"; break;
            case StorageControllerType_ICH6: strVbox = "ICH6"; break;
        }

        if (strVbox.length())
        {
            lIDEControllerIndex = (int32_t)pNewDesc->m->llDescriptions.size();
            pNewDesc->addEntry(VirtualSystemDescriptionType_HardDiskControllerIDE,
                               Utf8StrFmt("%d", lIDEControllerIndex),
                               strVbox,
                               strVbox);
        }

#ifdef VBOX_WITH_AHCI
//     <const name="HardDiskControllerSATA" value="7" />
        rc = GetStorageControllerByName(Bstr("SATA Controller"), pController.asOutParam());
        if (SUCCEEDED(rc))
        {
            strVbox = "AHCI";
            lSATAControllerIndex = (int32_t)pNewDesc->m->llDescriptions.size();
            pNewDesc->addEntry(VirtualSystemDescriptionType_HardDiskControllerSATA,
                               Utf8StrFmt("%d", lSATAControllerIndex),
                               strVbox,
                               strVbox);
        }
#endif // VBOX_WITH_AHCI

#ifdef VBOX_WITH_LSILOGIC
//     <const name="HardDiskControllerSCSI" value="8" />
        rc = GetStorageControllerByName(Bstr("SCSI Controller"), pController.asOutParam());
        if (SUCCEEDED(rc))
        {
            rc = pController->COMGETTER(ControllerType)(&ctlr);
            if (SUCCEEDED(rc))
            {
                strVbox = "LsiLogic";       // the default in VBox
                switch(ctlr)
                {
                    case StorageControllerType_LsiLogic: strVbox = "LsiLogic"; break;
                    case StorageControllerType_BusLogic: strVbox = "BusLogic"; break;
                }
                lSCSIControllerIndex = (int32_t)pNewDesc->m->llDescriptions.size();
                pNewDesc->addEntry(VirtualSystemDescriptionType_HardDiskControllerSCSI,
                                   Utf8StrFmt("%d", lSCSIControllerIndex),
                                   strVbox,
                                   strVbox);
            }
            else
                throw rc;
        }
#endif // VBOX_WITH_LSILOGIC

//     <const name="HardDiskImage" value="9" />
        MediaData::AttachmentList::iterator itA;
        for (itA = mMediaData->mAttachments.begin();
             itA != mMediaData->mAttachments.end();
             ++itA)
        {
            ComObjPtr<MediumAttachment> pHDA = *itA;

            // the attachment's data
            ComPtr<IMedium> pMedium;
            ComPtr<IStorageController> ctl;
            Bstr controllerName;

            rc = pHDA->COMGETTER(Controller)(controllerName.asOutParam());
            if (FAILED(rc)) throw rc;

            rc = GetStorageControllerByName(controllerName, ctl.asOutParam());
            if (FAILED(rc)) throw rc;

            StorageBus_T storageBus;
            LONG lChannel;
            LONG lDevice;

            rc = ctl->COMGETTER(Bus)(&storageBus);
            if (FAILED(rc)) throw rc;

            rc = pHDA->COMGETTER(Medium)(pMedium.asOutParam());
            if (FAILED(rc)) throw rc;

            rc = pHDA->COMGETTER(Port)(&lChannel);
            if (FAILED(rc)) throw rc;

            rc = pHDA->COMGETTER(Device)(&lDevice);
            if (FAILED(rc)) throw rc;

            Bstr bstrLocation;
            Bstr bstrName;
            if (pMedium) // @todo mediumbranch only for hard disks
            {
                rc = pMedium->COMGETTER(Location)(bstrLocation.asOutParam());
                if (FAILED(rc)) throw rc;
                rc = pMedium->COMGETTER(Name)(bstrName.asOutParam());
                if (FAILED(rc)) throw rc;

                // force reading state, or else size will be returned as 0
                MediumState_T ms;
                rc = pMedium->RefreshState(&ms);
                if (FAILED(rc)) throw rc;

                ULONG64 ullSize;
                rc = pMedium->COMGETTER(Size)(&ullSize);
                if (FAILED(rc)) throw rc;

                // and how this translates to the virtual system
                int32_t lControllerVsys = 0;
                LONG lChannelVsys;

                switch (storageBus)
                {
                    case StorageBus_IDE:
                        // this is the exact reverse to what we're doing in Appliance::taskThreadImportMachines,
                        // and it must be updated when that is changed!

                        if (lChannel == 0 && lDevice == 0)      // primary master
                            lChannelVsys = 0;
                        else if (lChannel == 0 && lDevice == 1) // primary slave
                            lChannelVsys = 1;
                        else if (lChannel == 1 && lDevice == 1) // secondary slave; secondary master is always CDROM
                            lChannelVsys = 2;
                        else
                            throw setError(VBOX_E_NOT_SUPPORTED,
                                        tr("Cannot handle hard disk attachment: channel is %d, device is %d"), lChannel, lDevice);

                        lControllerVsys = lIDEControllerIndex;
                    break;

                    case StorageBus_SATA:
                        lChannelVsys = lChannel;        // should be between 0 and 29
                        lControllerVsys = lSATAControllerIndex;
                    break;

                    case StorageBus_SCSI:
                        lChannelVsys = lChannel;        // should be between 0 and 15
                        lControllerVsys = lSCSIControllerIndex;
                    break;

                    default:
                        throw setError(VBOX_E_NOT_SUPPORTED,
                                    tr("Cannot handle hard disk attachment: storageBus is %d, channel is %d, device is %d"), storageBus, lChannel, lDevice);
                    break;
                }

                Utf8Str strTargetVmdkName(bstrName);
                strTargetVmdkName.stripExt();
                strTargetVmdkName.append(".vmdk");

                pNewDesc->addEntry(VirtualSystemDescriptionType_HardDiskImage,
                                strTargetVmdkName,   // disk ID: let's use the name
                                strTargetVmdkName,   // OVF value:
                                Utf8Str(bstrLocation), // vbox value: media path
                                (uint32_t)(ullSize / _1M),
                                Utf8StrFmt("controller=%RI32;channel=%RI32", lControllerVsys, lChannelVsys));
            }
        }

        /* Floppy Drive */
        if (fFloppyEnabled)  // @todo mediumbranch
            pNewDesc->addEntry(VirtualSystemDescriptionType_Floppy, "", "", "");

        /* CD Drive */
        if (fDVDEnabled) // @todo mediumbranch
            pNewDesc->addEntry(VirtualSystemDescriptionType_CDROM, "", "", "");

//     <const name="NetworkAdapter" />
        size_t a;
        for (a = 0;
             a < SchemaDefs::NetworkAdapterCount;
             ++a)
        {
            ComPtr<INetworkAdapter> pNetworkAdapter;
            BOOL fEnabled;
            NetworkAdapterType_T adapterType;
            NetworkAttachmentType_T attachmentType;

            rc = GetNetworkAdapter((ULONG)a, pNetworkAdapter.asOutParam());
            if (FAILED(rc)) throw rc;
            /* Enable the network card & set the adapter type */
            rc = pNetworkAdapter->COMGETTER(Enabled)(&fEnabled);
            if (FAILED(rc)) throw rc;

            if (fEnabled)
            {
                Utf8Str strAttachmentType;

                rc = pNetworkAdapter->COMGETTER(AdapterType)(&adapterType);
                if (FAILED(rc)) throw rc;

                rc = pNetworkAdapter->COMGETTER(AttachmentType)(&attachmentType);
                if (FAILED(rc)) throw rc;

                switch (attachmentType)
                {
                    case NetworkAttachmentType_Null:
                        strAttachmentType = "Null";
                    break;

                    case NetworkAttachmentType_NAT:
                        strAttachmentType = "NAT";
                    break;

                    case NetworkAttachmentType_Bridged:
                        strAttachmentType = "Bridged";
                    break;

                    case NetworkAttachmentType_Internal:
                        strAttachmentType = "Internal";
                    break;

                    case NetworkAttachmentType_HostOnly:
                        strAttachmentType = "HostOnly";
                    break;
                }

                pNewDesc->addEntry(VirtualSystemDescriptionType_NetworkAdapter,
                                   "",      // ref
                                   strAttachmentType,      // orig
                                   Utf8StrFmt("%RI32", (uint32_t)adapterType),   // conf
                                   0,
                                   Utf8StrFmt("type=%s", strAttachmentType.c_str()));       // extra conf
            }
        }

//     <const name="USBController"  />
#ifdef VBOX_WITH_USB
        if (fUSBEnabled)
            pNewDesc->addEntry(VirtualSystemDescriptionType_USBController, "", "", "");
#endif /* VBOX_WITH_USB */

//     <const name="SoundCard"  />
        if (fAudioEnabled)
        {
            pNewDesc->addEntry(VirtualSystemDescriptionType_SoundCard,
                               "",
                               "ensoniq1371",       // this is what OVFTool writes and VMware supports
                               Utf8StrFmt("%RI32", audioController));
        }

        // finally, add the virtual system to the appliance
        Appliance *pAppliance = static_cast<Appliance*>(aAppliance);
        AutoCaller autoCaller1(pAppliance);
        CheckComRCReturnRC(autoCaller1.rc());

        /* We return the new description to the caller */
        ComPtr<IVirtualSystemDescription> copy(pNewDesc);
        copy.queryInterfaceTo(aDescription);

        AutoWriteLock alock(pAppliance);

        pAppliance->m->virtualSystemDescriptions.push_back(pNewDesc);
    }
    catch(HRESULT arc)
    {
        rc = arc;
    }

    return rc;
}

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
