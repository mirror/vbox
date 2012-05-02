/* $Id$ */
/** @file
 *
 * IAppliance and IVirtualSystem COM class implementations.
 */

/*
 * Copyright (C) 2008-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <iprt/path.h>
#include <iprt/cpp/utils.h>

#include <VBox/com/array.h>

#include "ApplianceImpl.h"
#include "VFSExplorerImpl.h"
#include "VirtualBoxImpl.h"
#include "GuestOSTypeImpl.h"
#include "Global.h"
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
    ovf::CIMOSType_T    cim;
    VBOXOSTYPE          osType;
}
g_osTypes[] =
{
    { ovf::CIMOSType_CIMOS_Unknown,                              VBOXOSTYPE_Unknown },
    { ovf::CIMOSType_CIMOS_OS2,                                  VBOXOSTYPE_OS2 },
    { ovf::CIMOSType_CIMOS_OS2,                                  VBOXOSTYPE_OS2Warp3 },
    { ovf::CIMOSType_CIMOS_OS2,                                  VBOXOSTYPE_OS2Warp4 },
    { ovf::CIMOSType_CIMOS_OS2,                                  VBOXOSTYPE_OS2Warp45 },
    { ovf::CIMOSType_CIMOS_MSDOS,                                VBOXOSTYPE_DOS },
    { ovf::CIMOSType_CIMOS_WIN3x,                                VBOXOSTYPE_Win31 },
    { ovf::CIMOSType_CIMOS_WIN95,                                VBOXOSTYPE_Win95 },
    { ovf::CIMOSType_CIMOS_WIN98,                                VBOXOSTYPE_Win98 },
    { ovf::CIMOSType_CIMOS_WINNT,                                VBOXOSTYPE_WinNT },
    { ovf::CIMOSType_CIMOS_WINNT,                                VBOXOSTYPE_WinNT4 },
    { ovf::CIMOSType_CIMOS_NetWare,                              VBOXOSTYPE_Netware },
    { ovf::CIMOSType_CIMOS_NovellOES,                            VBOXOSTYPE_Netware },
    { ovf::CIMOSType_CIMOS_Solaris,                              VBOXOSTYPE_Solaris },
    { ovf::CIMOSType_CIMOS_SunOS,                                VBOXOSTYPE_Solaris },
    { ovf::CIMOSType_CIMOS_FreeBSD,                              VBOXOSTYPE_FreeBSD },
    { ovf::CIMOSType_CIMOS_NetBSD,                               VBOXOSTYPE_NetBSD },
    { ovf::CIMOSType_CIMOS_QNX,                                  VBOXOSTYPE_QNX },
    { ovf::CIMOSType_CIMOS_Windows2000,                          VBOXOSTYPE_Win2k },
    { ovf::CIMOSType_CIMOS_WindowsMe,                            VBOXOSTYPE_WinMe },
    { ovf::CIMOSType_CIMOS_OpenBSD,                              VBOXOSTYPE_OpenBSD },
    { ovf::CIMOSType_CIMOS_WindowsXP,                            VBOXOSTYPE_WinXP },
    { ovf::CIMOSType_CIMOS_WindowsXPEmbedded,                    VBOXOSTYPE_WinXP },
    { ovf::CIMOSType_CIMOS_WindowsEmbeddedforPointofService,     VBOXOSTYPE_WinXP },
    { ovf::CIMOSType_CIMOS_MicrosoftWindowsServer2003,           VBOXOSTYPE_Win2k3 },
    { ovf::CIMOSType_CIMOS_MicrosoftWindowsServer2003_64,        VBOXOSTYPE_Win2k3_x64 },
    { ovf::CIMOSType_CIMOS_WindowsXP_64,                         VBOXOSTYPE_WinXP_x64 },
    { ovf::CIMOSType_CIMOS_WindowsVista,                         VBOXOSTYPE_WinVista },
    { ovf::CIMOSType_CIMOS_WindowsVista_64,                      VBOXOSTYPE_WinVista_x64 },
    { ovf::CIMOSType_CIMOS_MicrosoftWindowsServer2008,           VBOXOSTYPE_Win2k8 },
    { ovf::CIMOSType_CIMOS_MicrosoftWindowsServer2008_64,        VBOXOSTYPE_Win2k8_x64 },
    { ovf::CIMOSType_CIMOS_FreeBSD_64,                           VBOXOSTYPE_FreeBSD_x64 },
    { ovf::CIMOSType_CIMOS_MACOS,                                VBOXOSTYPE_MacOS },
    { ovf::CIMOSType_CIMOS_MACOS,                                VBOXOSTYPE_MacOS_x64 },            // there is no CIM 64-bit type for this

    // Linuxes
    { ovf::CIMOSType_CIMOS_RedHatEnterpriseLinux,                VBOXOSTYPE_RedHat },
    { ovf::CIMOSType_CIMOS_RedHatEnterpriseLinux_64,             VBOXOSTYPE_RedHat_x64 },
    { ovf::CIMOSType_CIMOS_Solaris_64,                           VBOXOSTYPE_Solaris_x64 },
    { ovf::CIMOSType_CIMOS_SUSE,                                 VBOXOSTYPE_OpenSUSE },
    { ovf::CIMOSType_CIMOS_SLES,                                 VBOXOSTYPE_OpenSUSE },
    { ovf::CIMOSType_CIMOS_NovellLinuxDesktop,                   VBOXOSTYPE_OpenSUSE },
    { ovf::CIMOSType_CIMOS_SUSE_64,                              VBOXOSTYPE_OpenSUSE_x64 },
    { ovf::CIMOSType_CIMOS_SLES_64,                              VBOXOSTYPE_OpenSUSE_x64 },
    { ovf::CIMOSType_CIMOS_LINUX,                                VBOXOSTYPE_Linux },
    { ovf::CIMOSType_CIMOS_LINUX,                                VBOXOSTYPE_Linux22 },
    { ovf::CIMOSType_CIMOS_SunJavaDesktopSystem,                 VBOXOSTYPE_Linux },
    { ovf::CIMOSType_CIMOS_TurboLinux,                           VBOXOSTYPE_Turbolinux },
    { ovf::CIMOSType_CIMOS_TurboLinux_64,                        VBOXOSTYPE_Turbolinux_x64 },
    { ovf::CIMOSType_CIMOS_Mandriva,                             VBOXOSTYPE_Mandriva },
    { ovf::CIMOSType_CIMOS_Mandriva_64,                          VBOXOSTYPE_Mandriva_x64 },
    { ovf::CIMOSType_CIMOS_Ubuntu,                               VBOXOSTYPE_Ubuntu },
    { ovf::CIMOSType_CIMOS_Ubuntu_64,                            VBOXOSTYPE_Ubuntu_x64 },
    { ovf::CIMOSType_CIMOS_Debian,                               VBOXOSTYPE_Debian },
    { ovf::CIMOSType_CIMOS_Debian_64,                            VBOXOSTYPE_Debian_x64 },
    { ovf::CIMOSType_CIMOS_Linux_2_4_x,                          VBOXOSTYPE_Linux24 },
    { ovf::CIMOSType_CIMOS_Linux_2_4_x_64,                       VBOXOSTYPE_Linux24_x64 },
    { ovf::CIMOSType_CIMOS_Linux_2_6_x,                          VBOXOSTYPE_Linux26 },
    { ovf::CIMOSType_CIMOS_Linux_2_6_x_64,                       VBOXOSTYPE_Linux26_x64 },
    { ovf::CIMOSType_CIMOS_Linux_64,                             VBOXOSTYPE_Linux26_x64 },

    // types that we have support for but CIM doesn't
    { ovf::CIMOSType_CIMOS_Linux_2_6_x,                          VBOXOSTYPE_ArchLinux },
    { ovf::CIMOSType_CIMOS_Linux_2_6_x_64,                       VBOXOSTYPE_ArchLinux_x64 },
    { ovf::CIMOSType_CIMOS_Linux_2_6_x,                          VBOXOSTYPE_FedoraCore },
    { ovf::CIMOSType_CIMOS_Linux_2_6_x_64,                       VBOXOSTYPE_FedoraCore_x64 },
    { ovf::CIMOSType_CIMOS_Linux_2_6_x,                          VBOXOSTYPE_Gentoo },
    { ovf::CIMOSType_CIMOS_Linux_2_6_x_64,                       VBOXOSTYPE_Gentoo_x64 },
    { ovf::CIMOSType_CIMOS_Linux_2_6_x,                          VBOXOSTYPE_Xandros },
    { ovf::CIMOSType_CIMOS_Linux_2_6_x_64,                       VBOXOSTYPE_Xandros_x64 },
    { ovf::CIMOSType_CIMOS_Solaris,                              VBOXOSTYPE_OpenSolaris },
    { ovf::CIMOSType_CIMOS_Solaris_64,                           VBOXOSTYPE_OpenSolaris_x64 },

    // types added with CIM 2.25.0 follow:
    { ovf::CIMOSType_CIMOS_WindowsServer2008R2,                  VBOXOSTYPE_Win2k8 },           // duplicate, see above
//     { ovf::CIMOSType_CIMOS_VMwareESXi = 104,                                                 // we can't run ESX in a VM
    { ovf::CIMOSType_CIMOS_Windows7,                             VBOXOSTYPE_Win7 },
    { ovf::CIMOSType_CIMOS_Windows7,                             VBOXOSTYPE_Win7_x64 },         // there is no CIM 64-bit type for this
    { ovf::CIMOSType_CIMOS_CentOS,                               VBOXOSTYPE_RedHat },
    { ovf::CIMOSType_CIMOS_CentOS_64,                            VBOXOSTYPE_RedHat_x64 },
    { ovf::CIMOSType_CIMOS_OracleEnterpriseLinux,                VBOXOSTYPE_Oracle },
    { ovf::CIMOSType_CIMOS_OracleEnterpriseLinux_64,             VBOXOSTYPE_Oracle_x64 },
    { ovf::CIMOSType_CIMOS_eComStation,                          VBOXOSTYPE_ECS }

    // there are no CIM types for these, so these turn to "other" on export:
    //      VBOXOSTYPE_OpenBSD
    //      VBOXOSTYPE_OpenBSD_x64
    //      VBOXOSTYPE_NetBSD
    //      VBOXOSTYPE_NetBSD_x64

};

/* Pattern structure for matching the OS type description field */
struct osTypePattern
{
    const char *pcszPattern;
    VBOXOSTYPE osType;
};

/* These are the 32-Bit ones. They are sorted by priority. */
static const osTypePattern g_osTypesPattern[] =
{
    {"Windows NT",    VBOXOSTYPE_WinNT4},
    {"Windows XP",    VBOXOSTYPE_WinXP},
    {"Windows 2000",  VBOXOSTYPE_Win2k},
    {"Windows 2003",  VBOXOSTYPE_Win2k3},
    {"Windows Vista", VBOXOSTYPE_WinVista},
    {"Windows 2008",  VBOXOSTYPE_Win2k8},
    {"SUSE",          VBOXOSTYPE_OpenSUSE},
    {"Novell",        VBOXOSTYPE_OpenSUSE},
    {"Red Hat",       VBOXOSTYPE_RedHat},
    {"Mandriva",      VBOXOSTYPE_Mandriva},
    {"Ubuntu",        VBOXOSTYPE_Ubuntu},
    {"Debian",        VBOXOSTYPE_Debian},
    {"QNX",           VBOXOSTYPE_QNX},
    {"Linux 2.4",     VBOXOSTYPE_Linux24},
    {"Linux 2.6",     VBOXOSTYPE_Linux26},
    {"Linux",         VBOXOSTYPE_Linux},
    {"OpenSolaris",   VBOXOSTYPE_OpenSolaris},
    {"Solaris",       VBOXOSTYPE_OpenSolaris},
    {"FreeBSD",       VBOXOSTYPE_FreeBSD},
    {"NetBSD",        VBOXOSTYPE_NetBSD},
    {"Windows 95",    VBOXOSTYPE_Win95},
    {"Windows 98",    VBOXOSTYPE_Win98},
    {"Windows Me",    VBOXOSTYPE_WinMe},
    {"Windows 3.",    VBOXOSTYPE_Win31},
    {"DOS",           VBOXOSTYPE_DOS},
    {"OS2",           VBOXOSTYPE_OS2}
};

/* These are the 64-Bit ones. They are sorted by priority. */
static const osTypePattern g_osTypesPattern64[] =
{
    {"Windows XP",    VBOXOSTYPE_WinXP_x64},
    {"Windows 2003",  VBOXOSTYPE_Win2k3_x64},
    {"Windows Vista", VBOXOSTYPE_WinVista_x64},
    {"Windows 2008",  VBOXOSTYPE_Win2k8_x64},
    {"SUSE",          VBOXOSTYPE_OpenSUSE_x64},
    {"Novell",        VBOXOSTYPE_OpenSUSE_x64},
    {"Red Hat",       VBOXOSTYPE_RedHat_x64},
    {"Mandriva",      VBOXOSTYPE_Mandriva_x64},
    {"Ubuntu",        VBOXOSTYPE_Ubuntu_x64},
    {"Debian",        VBOXOSTYPE_Debian_x64},
    {"Linux 2.4",     VBOXOSTYPE_Linux24_x64},
    {"Linux 2.6",     VBOXOSTYPE_Linux26_x64},
    {"Linux",         VBOXOSTYPE_Linux26_x64},
    {"OpenSolaris",   VBOXOSTYPE_OpenSolaris_x64},
    {"Solaris",       VBOXOSTYPE_OpenSolaris_x64},
    {"FreeBSD",       VBOXOSTYPE_FreeBSD_x64},
};

/**
 * Private helper func that suggests a VirtualBox guest OS type
 * for the given OVF operating system type.
 * @param osTypeVBox
 * @param c
 * @param cStr
 */
void convertCIMOSType2VBoxOSType(Utf8Str &strType, ovf::CIMOSType_T c, const Utf8Str &cStr)
{
    /* First check if the type is other/other_64 */
    if (c == ovf::CIMOSType_CIMOS_Other)
    {
        for (size_t i=0; i < RT_ELEMENTS(g_osTypesPattern); ++i)
            if (cStr.contains (g_osTypesPattern[i].pcszPattern, Utf8Str::CaseInsensitive))
            {
                strType = Global::OSTypeId(g_osTypesPattern[i].osType);
                return;
            }
    }
    else if (c == ovf::CIMOSType_CIMOS_Other_64)
    {
        for (size_t i=0; i < RT_ELEMENTS(g_osTypesPattern64); ++i)
            if (cStr.contains (g_osTypesPattern64[i].pcszPattern, Utf8Str::CaseInsensitive))
            {
                strType = Global::OSTypeId(g_osTypesPattern64[i].osType);
                return;
            }
    }

    for (size_t i = 0; i < RT_ELEMENTS(g_osTypes); ++i)
    {
        if (c == g_osTypes[i].cim)
        {
            strType = Global::OSTypeId(g_osTypes[i].osType);
            return;
        }
    }

    strType = Global::OSTypeId(VBOXOSTYPE_Unknown);
}

/**
 * Private helper func that suggests a VirtualBox guest OS type
 * for the given OVF operating system type.
 * @param osTypeVBox
 * @param c
 */
ovf::CIMOSType_T convertVBoxOSType2CIMOSType(const char *pcszVbox)
{
    for (size_t i = 0; i < RT_ELEMENTS(g_osTypes); ++i)
    {
        if (!RTStrICmp(pcszVbox, Global::OSTypeId(g_osTypes[i].osType)))
            return g_osTypes[i].cim;
    }

    return ovf::CIMOSType_CIMOS_Other;
}

Utf8Str convertNetworkAttachmentTypeToString(NetworkAttachmentType_T type)
{
    Utf8Str strType;
    switch (type)
    {
        case NetworkAttachmentType_NAT: strType = "NAT"; break;
        case NetworkAttachmentType_Bridged: strType = "Bridged"; break;
        case NetworkAttachmentType_Internal: strType = "Internal"; break;
        case NetworkAttachmentType_HostOnly: strType = "HostOnly"; break;
        case NetworkAttachmentType_Generic: strType = "Generic"; break;
        case NetworkAttachmentType_Null: strType = "Null"; break;
    }
    return strType;
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

        ovf::DiskImagesMap::const_iterator it;
        size_t i = 0;
        for (it = m->pReader->m_mapDisks.begin();
             it != m->pReader->m_mapDisks.end();
             ++it, ++i)
        {
            // create a string representing this disk
            const ovf::DiskImage &d = it->second;
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

/**
 * Public method implementation.
 * @param aDisks
 * @return
 */
STDMETHODIMP Appliance::COMGETTER(Machines)(ComSafeArrayOut(BSTR, aMachines))
{
    CheckComArgOutSafeArrayPointerValid(aMachines);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (!isApplianceIdle())
        return E_ACCESSDENIED;

    com::SafeArray<BSTR> sfaMachines(m->llGuidsMachinesCreated.size());
    size_t u = 0;
    for (std::list<Guid>::const_iterator it = m->llGuidsMachinesCreated.begin();
         it != m->llGuidsMachinesCreated.end();
         ++it)
    {
        const Guid &uuid = *it;
        Bstr bstr(uuid.toUtf16());
        bstr.detachTo(&sfaMachines[u]);
        ++u;
    }

    sfaMachines.detachTo(ComSafeArrayOutArg(aMachines));

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
bool Appliance::isApplianceIdle()
{
    if (m->state == Data::ApplianceImporting)
        setError(VBOX_E_INVALID_OBJECT_STATE, tr("The appliance is busy importing files"));
    else if (m->state == Data::ApplianceExporting)
        setError(VBOX_E_INVALID_OBJECT_STATE, tr("The appliance is busy exporting files"));
    else
        return true;

    return false;
}

HRESULT Appliance::searchUniqueVMName(Utf8Str& aName) const
{
    IMachine *machine = NULL;
    char *tmpName = RTStrDup(aName.c_str());
    int i = 1;
    /** @todo: Maybe too cost-intensive; try to find a lighter way */
    while (mVirtualBox->FindMachine(Bstr(tmpName).raw(), &machine) != VBOX_E_OBJECT_NOT_FOUND)
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
    /** @todo: Maybe too cost-intensive; try to find a lighter way */
    while (    RTPathExists(tmpName)
            || mVirtualBox->OpenMedium(Bstr(tmpName).raw(), DeviceType_HardDisk, AccessMode_ReadWrite, FALSE /* fForceNewUuid */,  &harddisk)
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
 * Called from Appliance::importImpl() and Appliance::writeImpl() to set up a
 * progress object with the proper weights and maximum progress values.
 *
 * @param pProgress
 * @param bstrDescription
 * @param mode
 * @return
 */
HRESULT Appliance::setUpProgress(ComObjPtr<Progress> &pProgress,
                                 const Bstr &bstrDescription,
                                 SetUpProgressMode mode)
{
    HRESULT rc;

    /* Create the progress object */
    pProgress.createObject();

    // compute the disks weight (this sets ulTotalDisksMB and cDisks in the instance data)
    disksWeight();

    m->ulWeightForManifestOperation = 0;

    ULONG cOperations;
    ULONG ulTotalOperationsWeight;

    cOperations =   1               // one for XML setup
                  + m->cDisks;      // plus one per disk
    if (m->ulTotalDisksMB)
    {
        m->ulWeightForXmlOperation = (ULONG)((double)m->ulTotalDisksMB * 1 / 100);    // use 1% of the progress for the XML
        ulTotalOperationsWeight = m->ulTotalDisksMB + m->ulWeightForXmlOperation;
    }
    else
    {
        // no disks to export:
        m->ulWeightForXmlOperation = 1;
        ulTotalOperationsWeight = 1;
    }

    switch (mode)
    {
        case ImportFile:
        {
            break;
        }
        case WriteFile:
        {
            // assume that creating the manifest will take .1% of the time it takes to export the disks
            if (m->fManifest)
            {
                ++cOperations;          // another one for creating the manifest

                m->ulWeightForManifestOperation = (ULONG)((double)m->ulTotalDisksMB * .1 / 100);    // use .5% of the progress for the manifest
                ulTotalOperationsWeight += m->ulWeightForManifestOperation;
            }
            break;
        }
        case ImportS3:
        {
            cOperations += 1 + 1;     // another one for the manifest file & another one for the import
            ulTotalOperationsWeight = m->ulTotalDisksMB;
            if (!m->ulTotalDisksMB)
                // no disks to export:
                ulTotalOperationsWeight = 1;

            ULONG ulImportWeight = (ULONG)((double)ulTotalOperationsWeight * 50  / 100);  // use 50% for import
            ulTotalOperationsWeight += ulImportWeight;

            m->ulWeightForXmlOperation = ulImportWeight; /* save for using later */

            ULONG ulInitWeight = (ULONG)((double)ulTotalOperationsWeight * 0.1  / 100);  // use 0.1% for init
            ulTotalOperationsWeight += ulInitWeight;
            break;
        }
        case WriteS3:
        {
            cOperations += 1 + 1;     // another one for the mf & another one for temporary creation

            if (m->ulTotalDisksMB)
            {
                m->ulWeightForXmlOperation = (ULONG)((double)m->ulTotalDisksMB * 1  / 100);    // use 1% of the progress for OVF file upload (we didn't know the size at this point)
                ulTotalOperationsWeight = m->ulTotalDisksMB + m->ulWeightForXmlOperation;
            }
            else
            {
                // no disks to export:
                ulTotalOperationsWeight = 1;
                m->ulWeightForXmlOperation = 1;
            }
            ULONG ulOVFCreationWeight = (ULONG)((double)ulTotalOperationsWeight * 50.0 / 100.0); /* Use 50% for the creation of the OVF & the disks */
            ulTotalOperationsWeight += ulOVFCreationWeight;
            break;
        }
    }

    Log(("Setting up progress object: ulTotalMB = %d, cDisks = %d, => cOperations = %d, ulTotalOperationsWeight = %d, m->ulWeightForXmlOperation = %d\n",
         m->ulTotalDisksMB, m->cDisks, cOperations, ulTotalOperationsWeight, m->ulWeightForXmlOperation));

    rc = pProgress->init(mVirtualBox, static_cast<IAppliance*>(this),
                         bstrDescription.raw(),
                         TRUE /* aCancelable */,
                         cOperations, // ULONG cOperations,
                         ulTotalOperationsWeight, // ULONG ulTotalOperationsWeight,
                         bstrDescription.raw(), // CBSTR bstrFirstOperationDescription,
                         m->ulWeightForXmlOperation); // ULONG ulFirstOperationWeight,
    return rc;
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
    ULONG cOp = 0;
    while (SUCCEEDED(pProgressAsync->COMGETTER(Completed(&fCompleted))))
    {
        rc = pProgressThis->COMGETTER(Canceled)(&fCanceled);
        if (FAILED(rc)) throw rc;
        if (fCanceled)
            pProgressAsync->Cancel();
        /* Check if the current operation has changed. It is also possible
           that in the meantime more than one async operation was finished. So
           we have to loop as long as we reached the same operation count. */
        ULONG curOp;
        for(;;)
        {
            rc = pProgressAsync->COMGETTER(Operation(&curOp));
            if (FAILED(rc)) throw rc;
            if (cOp != curOp)
            {
                Bstr bstr;
                ULONG currentWeight;
                rc = pProgressAsync->COMGETTER(OperationDescription(bstr.asOutParam()));
                if (FAILED(rc)) throw rc;
                rc = pProgressAsync->COMGETTER(OperationWeight(&currentWeight));
                if (FAILED(rc)) throw rc;
                rc = pProgressThis->SetNextOperation(bstr.raw(), currentWeight);
                if (FAILED(rc)) throw rc;
                ++cOp;
            }else
                break;
        }

        rc = pProgressAsync->COMGETTER(OperationPercent(&currentPercent));
        if (FAILED(rc)) throw rc;
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
    Utf8Str str(aWarning, args);
    va_end(args);
    m->llWarnings.push_back(str);
}

/**
 * Refreshes the cDisks and ulTotalDisksMB members in the instance data.
 * Requires that virtual system descriptions are present.
 */
void Appliance::disksWeight()
{
    m->ulTotalDisksMB = 0;
    m->cDisks = 0;
    // weigh the disk images according to their sizes
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
            m->ulTotalDisksMB += pHD->ulSizeMB;
            ++m->cDisks;
        }
    }

}

void Appliance::parseBucket(Utf8Str &aPath, Utf8Str &aBucket)
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

/**
 *
 * @return
 */
int Appliance::TaskOVF::startThread()
{
    int vrc = RTThreadCreate(NULL, Appliance::taskThreadImportOrExport, this,
                             0, RTTHREADTYPE_MAIN_HEAVY_WORKER, 0,
                             "Appliance::Task");

    if (RT_FAILURE(vrc))
        return Appliance::setErrorStatic(E_FAIL,
                                         Utf8StrFmt("Could not create OVF task thread (%Rrc)\n", vrc));

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
                taskrc = pAppliance->readFS(task.get());
            else if (task->locInfo.storageType == VFSType_S3)
#ifdef VBOX_WITH_S3
                taskrc = pAppliance->readS3(task.get());
#else
                taskrc = VERR_NOT_IMPLEMENTED;
#endif
        break;

        case TaskOVF::Import:
            if (task->locInfo.storageType == VFSType_File)
                taskrc = pAppliance->importFS(task.get());
            else if (task->locInfo.storageType == VFSType_S3)
#ifdef VBOX_WITH_S3
                taskrc = pAppliance->importS3(task.get());
#else
                taskrc = VERR_NOT_IMPLEMENTED;
#endif
        break;

        case TaskOVF::Write:
            if (task->locInfo.storageType == VFSType_File)
                taskrc = pAppliance->writeFS(task.get());
            else if (task->locInfo.storageType == VFSType_S3)
#ifdef VBOX_WITH_S3
                taskrc = pAppliance->writeS3(task.get());
#else
                taskrc = VERR_NOT_IMPLEMENTED;
#endif
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

void parseURI(Utf8Str strUri, LocationInfo &locInfo)
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
    m->pConfig = NULL;

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();
    return S_OK;
}

/**
* COM uninitializer.
*/

void VirtualSystemDescription::uninit()
{
    if (m->pConfig)
        delete m->pConfig;
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

        bstr = vsde.strVboxCurrent;
        bstr.cloneTo(&sfaVboxValues[i]);

        bstr = vsde.strExtraConfigCurrent;
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

        bstr = vsde->strVboxCurrent;
        bstr.cloneTo(&sfaVboxValues[i]);

        bstr = vsde->strExtraConfigCurrent;
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
            case VirtualSystemDescriptionValueType_Auto: bstr = vsde->strVboxCurrent; break;
            case VirtualSystemDescriptionValueType_ExtraConfig: bstr = vsde->strExtraConfigCurrent; break;
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
            vsde.strVboxCurrent = sfaVboxValues[i];
            vsde.strExtraConfigCurrent = sfaExtraConfigValues[i];
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
                                        const Utf8Str &aOvfValue,
                                        const Utf8Str &aVboxValue,
                                        uint32_t ulSizeMB,
                                        const Utf8Str &strExtraConfig /*= ""*/)
{
    VirtualSystemDescriptionEntry vsde;
    vsde.ulIndex = (uint32_t)m->llDescriptions.size();      // each entry gets an index so the client side can reference them
    vsde.type = aType;
    vsde.strRef = strRef;
    vsde.strOvf = aOvfValue;
    vsde.strVboxSuggested           // remember original value
        = vsde.strVboxCurrent       // and set current value which can be overridden by setFinalValues()
        = aVboxValue;
    vsde.strExtraConfigSuggested
        = vsde.strExtraConfigCurrent
        = strExtraConfig;
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
            case VirtualSystemDescriptionType_HardDiskControllerSAS:
                if (d.strRef == strRef)
                    return &d;
            break;
        }
    }

    return NULL;
}

/**
 * Method called from Appliance::Interpret() if the source OVF for a virtual system
 * contains a <vbox:Machine> element. This method then attempts to parse that and
 * create a MachineConfigFile instance from it which is stored in this instance data
 * and can then be used to create a machine.
 *
 * This must only be called once per instance.
 *
 * This rethrows all XML and logic errors from MachineConfigFile.
 *
 * @param elmMachine <vbox:Machine> element with attributes and subelements from some
 *                  DOM tree.
 */
void VirtualSystemDescription::importVboxMachineXML(const xml::ElementNode &elmMachine)
{
    settings::MachineConfigFile *pConfig = NULL;

    Assert(m->pConfig == NULL);

    try
    {
        pConfig = new settings::MachineConfigFile(NULL);
        pConfig->importMachineXML(elmMachine);

        m->pConfig = pConfig;
    }
    catch (...)
    {
        if (pConfig)
            delete pConfig;
        throw;
    }
}

/**
 * Returns the machine config created by importVboxMachineXML() or NULL if there's none.
 * @return
 */
const settings::MachineConfigFile* VirtualSystemDescription::getMachineConfig() const
{
    return m->pConfig;
}

