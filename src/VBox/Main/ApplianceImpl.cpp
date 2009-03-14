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

#include "ApplianceImpl.h"
#include "VirtualBoxImpl.h"
#include "GuestOSTypeImpl.h"
#include "ProgressImpl.h"
#include "MachineImpl.h"

#include "Logging.h"

#include "VBox/xml.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////
//
// hardware definitions
//
////////////////////////////////////////////////////////////////////////////////

struct DiskImage
{
    Utf8Str strDiskId;              // value from DiskSection/Disk/@diskId
    int64_t iCapacity;              // value from DiskSection/Disk/@capacity;
                                    // (maximum size for dynamic images, I guess; we always translate this to bytes)
    int64_t iPopulatedSize;         // value from DiskSection/Disk/@populatedSize
                                    // (actual used size of disk, always in bytes; can be an estimate of used disk
                                    // space, but cannot be larger than iCapacity)
    Utf8Str strFormat;              // value from DiskSection/Disk/@format
                // typically http://www.vmware.com/specifications/vmdk.html#sparse

    // fields from /References/File; the spec says the file reference from disk can be empty,
    // so in that case, strFilename will be empty, then a new disk should be created
    Utf8Str strHref;                // value from /References/File/@href (filename); if empty, then the remaining fields are ignored
    int64_t iSize;                  // value from /References/File/@size (optional according to spec; then we set -1 here)
    int64_t iChunkSize;             // value from /References/File/@chunkSize (optional, unsupported)
    Utf8Str strCompression;         // value from /References/File/@compression (optional, can be "gzip" according to spec)
};

struct VirtualHardwareItem
{
    Utf8Str strDescription;
    Utf8Str strCaption;
    Utf8Str strElementName;

    uint32_t ulInstanceID;
    uint32_t ulParent;

    OVFResourceType_T resourceType;
    Utf8Str strOtherResourceType;
    Utf8Str strResourceSubType;

    Utf8Str strHostResource;            // "Abstractly specifies how a device shall connect to a resource on the deployment platform.
                                        // Not all devices need a backing." Used with disk items, for which this references a virtual
                                        // disk from the Disks section.
    bool fAutomaticAllocation;
    bool fAutomaticDeallocation;
    Utf8Str strConnection;              // "All Ethernet adapters that specify the same abstract network connection name within an OVF
                                        // package shall be deployed on the same network. The abstract network connection name shall be
                                        // listed in the NetworkSection at the outermost envelope level." We ignore this and only set up
                                        // a network adapter depending on the network name.
    Utf8Str strAddress;                 // "Device-specific. For an Ethernet adapter, this specifies the MAC address."
    Utf8Str strAddressOnParent;         // "For a device, this specifies its location on the controller."
    Utf8Str strAllocationUnits;         // "Specifies the units of allocation used. For example, “byte * 2^20”."
    uint64_t ullVirtualQuantity;        // "Specifies the quantity of resources presented. For example, “256”."
    uint64_t ullReservation;            // "Specifies the minimum quantity of resources guaranteed to be available."
    uint64_t ullLimit;                  // "Specifies the maximum quantity of resources that will be granted."
    uint64_t ullWeight;                 // "Specifies a relative priority for this allocation in relation to other allocations."

    Utf8Str strConsumerVisibility;
    Utf8Str strMappingBehavior;
    Utf8Str strPoolID;
    uint32_t ulBusNumber;               // seen with IDE controllers, but not listed in OVF spec

    uint32_t ulLineNumber;              // line number of <Item> element in XML source; cached for error messages

    VirtualHardwareItem()
        : ulInstanceID(0), fAutomaticAllocation(false), fAutomaticDeallocation(false), ullVirtualQuantity(0), ullReservation(0), ullLimit(0), ullWeight(0), ulBusNumber(0), ulLineNumber(0)
    {};
};

typedef map<Utf8Str, DiskImage> DiskImagesMap;

struct VirtualSystem;

typedef map<uint32_t, VirtualHardwareItem> HardwareItemsMap;

struct HardDiskController
{
    uint32_t             idController;           // instance ID (Item/InstanceId); this gets referenced from HardDisk
    enum ControllerSystemType { IDE, SATA, SCSI };
    ControllerSystemType system;                 // one of IDE, SATA, SCSI
    Utf8Str              strControllerType;      // controller subtype (Item/ResourceSubType); e.g. "LsiLogic"; can be empty (esp. for IDE)
    Utf8Str              strAddress;             // for IDE
    uint32_t             ulBusNumber;            // for IDE

    HardDiskController()
        : idController(0),
          ulBusNumber(0)
    {
    }
};

typedef map<uint32_t, HardDiskController> ControllersMap;

struct VirtualDisk
{
    uint32_t            idController;           // SCSI (or IDE) controller this disk is connected to;
                                                // points into VirtualSystem.mapControllers
    uint32_t            ulAddressOnParent;      // parsed strAddressOnParent of hardware item; will be 0 or 1 for IDE
                                                // and possibly higher for disks attached to SCSI controllers (untested)
    Utf8Str             strDiskId;              // if the hard disk has an ovf:/disk/<id> reference,
                                                // this receives the <id> component; points to one of the
                                                // references in Appliance::Data.mapDisks
};

typedef map<Utf8Str, VirtualDisk> VirtualDisksMap;

struct VirtualSystem
{
    Utf8Str             strName;                // copy of VirtualSystem/@id

    CIMOSType_T         cimos;
    Utf8Str             strVirtualSystemType;   // generic hardware description; OVF says this can be something like "vmx-4" or "xen";
                                                // VMware Workstation 6.5 is "vmx-07"

    HardwareItemsMap    mapHardwareItems;       // map of virtual hardware items, sorted by unique instance ID

    uint64_t            ullMemorySize;          // always in bytes, copied from llHardwareItems; default = 0 (unspecified)
    uint16_t            cCPUs;                  // no. of CPUs, copied from llHardwareItems; default = 1

    list<Utf8Str>       llNetworkNames;
            // list of strings referring to network names
            // (one for each VirtualSystem/Item[@ResourceType=10]/Connection element)

    ControllersMap      mapControllers;
            // list of hard disk controllers
            // (one for each VirtualSystem/Item[@ResourceType=6] element with accumulated data from children)

    VirtualDisksMap     mapVirtualDisks;
            // (one for each VirtualSystem/Item[@ResourceType=17] element with accumulated data from children)

    bool                fHasFloppyDrive;        // true if there's a floppy item in mapHardwareItems
    bool                fHasCdromDrive;         // true if there's a CD-ROM item in mapHardwareItems; ISO images are not yet supported by OVFtool
    bool                fHasUsbController;      // true if there's a USB controller item in mapHardwareItems

    Utf8Str             strSoundCardType;       // if not empty, then the system wants a soundcard; this then specifies the hardware;
                                                // VMware Workstation 6.5 uses "ensoniq1371" for example

    Utf8Str             strLicenceInfo;         // license info if any; receives contents of VirtualSystem/EulaSection/Info
    Utf8Str             strLicenceText;         // license info if any; receives contents of VirtualSystem/EulaSection/License

    VirtualSystem()
        : ullMemorySize(0), cCPUs(1), fHasFloppyDrive(false), fHasCdromDrive(false), fHasUsbController(false)
    {
    }
};

////////////////////////////////////////////////////////////////////////////////
//
// Appliance data definition
//
////////////////////////////////////////////////////////////////////////////////

// opaque private instance data of Appliance class
struct Appliance::Data
{
    Utf8Str                 strPath;            // file name last given to either read() or write()

    DiskImagesMap           mapDisks;           // map of DiskImage structs, sorted by DiskImage.strDiskId

    list<VirtualSystem>     llVirtualSystems;   // list of virtual systems, created by and valid after read()

    list< ComObjPtr<VirtualSystemDescription> > virtualSystemDescriptions; //

    list<Utf8Str> llWarnings;
};

struct VirtualSystemDescription::Data
{
    list<VirtualSystemDescriptionEntry> llDescriptions;
};

////////////////////////////////////////////////////////////////////////////////
//
// Threads
//
////////////////////////////////////////////////////////////////////////////////

struct Appliance::TaskImportMachines
{
    TaskImportMachines(Appliance *aThat, Progress *aProgress)
        : pAppliance(aThat)
        , progress(aProgress)
        , rc(S_OK)
    {}
    ~TaskImportMachines() {}

    HRESULT startThread();

    Appliance *pAppliance;
    ComObjPtr<Progress> progress;
    HRESULT rc;
};

struct Appliance::TaskWriteOVF
{
    TaskWriteOVF(Appliance *aThat, Progress *aProgress)
        : pAppliance(aThat)
        , progress(aProgress)
        , rc(S_OK)
    {}
    ~TaskWriteOVF() {}

    HRESULT startThread();

    Appliance *pAppliance;
    ComObjPtr<Progress> progress;
    HRESULT rc;
};

////////////////////////////////////////////////////////////////////////////////
//
// internal helpers
//
////////////////////////////////////////////////////////////////////////////////

static Utf8Str stripFilename(const Utf8Str &strFile)
{
    Utf8Str str2(strFile);
    RTPathStripFilename(str2.mutableRaw());
    return str2;
}

static const struct
{
    CIMOSType_T     cim;
    const char      *pcszVbox;
}
    g_osTypes[] =
    {
        { CIMOSType_CIMOS_Unknown, SchemaDefs_OSTypeId_Other },
        { CIMOSType_CIMOS_OS2, SchemaDefs_OSTypeId_OS2 },
        { CIMOSType_CIMOS_MSDOS, SchemaDefs_OSTypeId_DOS },
        { CIMOSType_CIMOS_WIN3x, SchemaDefs_OSTypeId_Windows31 },
        { CIMOSType_CIMOS_WIN95, SchemaDefs_OSTypeId_Windows95 },
        { CIMOSType_CIMOS_WIN98, SchemaDefs_OSTypeId_Windows98 },
        { CIMOSType_CIMOS_WINNT, SchemaDefs_OSTypeId_WindowsNT4 },
        { CIMOSType_CIMOS_NetWare, SchemaDefs_OSTypeId_Netware },
        { CIMOSType_CIMOS_NovellOES, SchemaDefs_OSTypeId_Netware },
        { CIMOSType_CIMOS_Solaris, SchemaDefs_OSTypeId_Solaris },
        { CIMOSType_CIMOS_SunOS, SchemaDefs_OSTypeId_Solaris },
        { CIMOSType_CIMOS_FreeBSD, SchemaDefs_OSTypeId_FreeBSD },
        { CIMOSType_CIMOS_NetBSD, SchemaDefs_OSTypeId_NetBSD },
        { CIMOSType_CIMOS_QNX, SchemaDefs_OSTypeId_QNX },
        { CIMOSType_CIMOS_Windows2000, SchemaDefs_OSTypeId_Windows2000 },
        { CIMOSType_CIMOS_WindowsMe, SchemaDefs_OSTypeId_WindowsMe },
        { CIMOSType_CIMOS_OpenBSD, SchemaDefs_OSTypeId_OpenBSD },
        { CIMOSType_CIMOS_WindowsXP, SchemaDefs_OSTypeId_WindowsXP },
        { CIMOSType_CIMOS_WindowsXPEmbedded, SchemaDefs_OSTypeId_WindowsXP },
        { CIMOSType_CIMOS_WindowsEmbeddedforPointofService, SchemaDefs_OSTypeId_WindowsXP },
        { CIMOSType_CIMOS_MicrosoftWindowsServer2003, SchemaDefs_OSTypeId_Windows2003 },
        { CIMOSType_CIMOS_MicrosoftWindowsServer2003_64, SchemaDefs_OSTypeId_Windows2003_64 },
        { CIMOSType_CIMOS_WindowsXP_64, SchemaDefs_OSTypeId_WindowsXP_64 },
        { CIMOSType_CIMOS_WindowsVista, SchemaDefs_OSTypeId_WindowsVista },
        { CIMOSType_CIMOS_WindowsVista_64, SchemaDefs_OSTypeId_WindowsVista_64 },
        { CIMOSType_CIMOS_MicrosoftWindowsServer2008, SchemaDefs_OSTypeId_Windows2008 },
        { CIMOSType_CIMOS_MicrosoftWindowsServer2008_64, SchemaDefs_OSTypeId_Windows2008_64 },
        { CIMOSType_CIMOS_FreeBSD_64, SchemaDefs_OSTypeId_FreeBSD_64 },
        { CIMOSType_CIMOS_RedHatEnterpriseLinux, SchemaDefs_OSTypeId_RedHat },
        { CIMOSType_CIMOS_RedHatEnterpriseLinux_64, SchemaDefs_OSTypeId_RedHat_64 },
        { CIMOSType_CIMOS_Solaris_64, SchemaDefs_OSTypeId_Solaris_64 },
        { CIMOSType_CIMOS_SUSE, SchemaDefs_OSTypeId_OpenSUSE },
        { CIMOSType_CIMOS_SLES, SchemaDefs_OSTypeId_OpenSUSE },
        { CIMOSType_CIMOS_NovellLinuxDesktop, SchemaDefs_OSTypeId_OpenSUSE },
        { CIMOSType_CIMOS_SUSE_64, SchemaDefs_OSTypeId_OpenSUSE_64 },
        { CIMOSType_CIMOS_SLES_64, SchemaDefs_OSTypeId_OpenSUSE_64 },
        { CIMOSType_CIMOS_LINUX, SchemaDefs_OSTypeId_Linux },
        { CIMOSType_CIMOS_SunJavaDesktopSystem, SchemaDefs_OSTypeId_Linux },
        { CIMOSType_CIMOS_TurboLinux, SchemaDefs_OSTypeId_Linux},

            //                { CIMOSType_CIMOS_TurboLinux_64, },
            //                { CIMOSType_CIMOS_Linux_64, },
            //                    osTypeVBox = VBOXOSTYPE_Linux_x64;
            //                    break;

        { CIMOSType_CIMOS_Mandriva, SchemaDefs_OSTypeId_Mandriva },
        { CIMOSType_CIMOS_Mandriva_64, SchemaDefs_OSTypeId_Mandriva_64 },
        { CIMOSType_CIMOS_Ubuntu, SchemaDefs_OSTypeId_Ubuntu },
        { CIMOSType_CIMOS_Ubuntu_64, SchemaDefs_OSTypeId_Ubuntu_64 },
        { CIMOSType_CIMOS_Debian, SchemaDefs_OSTypeId_Debian },
        { CIMOSType_CIMOS_Debian_64, SchemaDefs_OSTypeId_Debian_64 },
        { CIMOSType_CIMOS_Linux_2_4_x, SchemaDefs_OSTypeId_Linux24 },
        { CIMOSType_CIMOS_Linux_2_4_x_64, SchemaDefs_OSTypeId_Linux24_64 },
        { CIMOSType_CIMOS_Linux_2_6_x, SchemaDefs_OSTypeId_Linux26 },
        { CIMOSType_CIMOS_Linux_2_6_x_64, SchemaDefs_OSTypeId_Linux26_64 }
};

/**
 * Private helper func that suggests a VirtualBox guest OS type
 * for the given OVF operating system type.
 * @param osTypeVBox
 * @param c
 */
static void convertCIMOSType2VBoxOSType(Utf8Str &strType, CIMOSType_T c)
{
    const char *osTypeVBox = "";

    for (size_t i = 0;
         i < RT_ELEMENTS(g_osTypes);
         ++i)
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
    const char *osTypeVBox = "";

    for (size_t i = 0;
         i < RT_ELEMENTS(g_osTypes);
         ++i)
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
// Appliance::task methods
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Appliance::TaskImportMachines::startThread()
{
    int vrc = RTThreadCreate(NULL, Appliance::taskThreadImportMachines, this,
                             0, RTTHREADTYPE_MAIN_HEAVY_WORKER, 0,
                             "Appliance::Task");
    ComAssertMsgRCRet(vrc,
                      ("Could not create taskThreadImportMachines (%Rrc)\n", vrc), E_FAIL);

    return S_OK;
}

HRESULT Appliance::TaskWriteOVF::startThread()
{
    int vrc = RTThreadCreate(NULL, Appliance::taskThreadWriteOVF, this,
                             0, RTTHREADTYPE_MAIN_HEAVY_WORKER, 0,
                             "Appliance::Task");
    ComAssertMsgRCRet(vrc,
                      ("Could not create taskThreadExportOVF (%Rrc)\n", vrc), E_FAIL);

    return S_OK;
}

////////////////////////////////////////////////////////////////////////////////
//
// Appliance constructor / destructor
//
////////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR(Appliance)
struct shutup {};

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
    delete m;
    m = NULL;
}

////////////////////////////////////////////////////////////////////////////////
//
// Appliance private methods
//
////////////////////////////////////////////////////////////////////////////////

/**
 * Private helper method that goes thru the elements of the given "current" element in the OVF XML
 * and handles the contained child elements (which can be "Section" or "Content" elements).
 *
 * @param pcszPath Path spec of the XML file, for error messages.
 * @param pReferencesElement "References" element from OVF, for looking up file specifications; can be NULL if no such element is present.
 * @param pCurElem Element whose children are to be analyzed here.
 * @return
 */
HRESULT Appliance::LoopThruSections(const char *pcszPath,
                                    const xml::ElementNode *pReferencesElem,
                                    const xml::ElementNode *pCurElem)
{
    HRESULT rc;

    xml::NodesLoop loopChildren(*pCurElem);
    const xml::ElementNode *pElem;
    while ((pElem = loopChildren.forAllNodes()))
    {
        const char *pcszElemName = pElem->getName();
        const char *pcszTypeAttr = "";
        const xml::AttributeNode *pTypeAttr;
        if ((pTypeAttr = pElem->findAttribute("type")))
            pcszTypeAttr = pTypeAttr->getValue();

        if (    (!strcmp(pcszElemName, "DiskSection"))
             || (    (!strcmp(pcszElemName, "Section"))
                  && (!strcmp(pcszTypeAttr, "ovf:DiskSection_Type"))
                )
           )
        {
            if (!(SUCCEEDED((rc = HandleDiskSection(pcszPath, pReferencesElem, pElem)))))
                return rc;
        }
       else if (    (!strcmp(pcszElemName, "NetworkSection"))            // we ignore NetworkSections for now
                  || (    (!strcmp(pcszElemName, "Section"))
                       && (!strcmp(pcszTypeAttr, "ovf:NetworkSection_Type"))
                     )
                )
        {
            if (!(SUCCEEDED((rc = HandleNetworkSection(pcszPath, pElem)))))
                return rc;
        }
        else if (    (!strcmp(pcszElemName, "DeploymentOptionSection>")))
        {
            // TODO
        }
        else if (    (!strcmp(pcszElemName, "Info")))
        {
            // child of VirtualSystemCollection -- TODO
        }
        else if (    (!strcmp(pcszElemName, "ResourceAllocationSection")))
        {
            // child of VirtualSystemCollection -- TODO
        }
        else if (    (!strcmp(pcszElemName, "StartupSection")))
        {
            // child of VirtualSystemCollection -- TODO
        }
        else if (    (!strcmp(pcszElemName, "VirtualSystem"))
                  || (    (!strcmp(pcszElemName, "Content"))
                       && (!strcmp(pcszTypeAttr, "ovf:VirtualSystem_Type"))
                     )
                )
        {
            if (!(SUCCEEDED((rc = HandleVirtualSystemContent(pcszPath, pElem)))))
                return rc;
        }
        else if (    (!strcmp(pcszElemName, "VirtualSystemCollection"))
                  || (    (!strcmp(pcszElemName, "Content"))
                       && (!strcmp(pcszTypeAttr, "ovf:VirtualSystemCollection_Type"))
                     )
                )
        {
            // TODO ResourceAllocationSection

            // recurse for this, since it has VirtualSystem elements as children
            if (!(SUCCEEDED((rc = LoopThruSections(pcszPath, pReferencesElem, pElem)))))
                return rc;
        }
    }

    return S_OK;
}

/**
 * Private helper method that handles disk sections in the OVF XML.
 * Gets called indirectly from IAppliance::read().
 *
 * @param pcszPath Path spec of the XML file, for error messages.
 * @param pReferencesElement "References" element from OVF, for looking up file specifications; can be NULL if no such element is present.
 * @param pSectionElem Section element for which this helper is getting called.
 * @return
 */
HRESULT Appliance::HandleDiskSection(const char *pcszPath,
                                     const xml::ElementNode *pReferencesElem,
                                     const xml::ElementNode *pSectionElem)
{
    // contains "Disk" child elements
    xml::NodesLoop loopDisks(*pSectionElem, "Disk");
    const xml::ElementNode *pelmDisk;
    while ((pelmDisk = loopDisks.forAllNodes()))
    {
        DiskImage d;
        const char *pcszBad = NULL;
        if (!(pelmDisk->getAttributeValue("diskId", d.strDiskId)))
            pcszBad = "diskId";
        else if (!(pelmDisk->getAttributeValue("format", d.strFormat)))
            pcszBad = "format";
        else if (!(pelmDisk->getAttributeValue("capacity", d.iCapacity)))
            pcszBad = "capacity";
        else
        {
            if (!(pelmDisk->getAttributeValue("populatedSize", d.iPopulatedSize)))
                // optional
                d.iPopulatedSize = -1;

            Utf8Str strFileRef;
            if (pelmDisk->getAttributeValue("fileRef", strFileRef)) // optional
            {
                // look up corresponding /References/File nodes (list built above)
                const xml::ElementNode *pFileElem;
                if (    pReferencesElem
                     && ((pFileElem = pReferencesElem->findChildElementFromId(strFileRef.c_str())))
                   )
                {
                    // copy remaining values from file node then
                    const char *pcszBadInFile = NULL;
                    if (!(pFileElem->getAttributeValue("href", d.strHref)))
                        pcszBadInFile = "href";
                    else if (!(pFileElem->getAttributeValue("size", d.iSize)))
                        d.iSize = -1;       // optional
                    // if (!(pFileElem->getAttributeValue("size", d.iChunkSize))) TODO
                    d.iChunkSize = -1;       // optional
                    pFileElem->getAttributeValue("compression", d.strCompression);

                    if (pcszBadInFile)
                        return setError(VBOX_E_FILE_ERROR,
                                        tr("Error reading \"%s\": missing or invalid attribute '%s' in 'File' element, line %d"),
                                        pcszPath,
                                        pcszBadInFile,
                                        pFileElem->getLineNumber());
                }
                else
                    return setError(VBOX_E_FILE_ERROR,
                                    tr("Error reading \"%s\": cannot find References/File element for ID '%s' referenced by 'Disk' element, line %d"),
                                    pcszPath,
                                    strFileRef.c_str(),
                                    pelmDisk->getLineNumber());
            }
        }

        if (pcszBad)
            return setError(VBOX_E_FILE_ERROR,
                            tr("Error reading \"%s\": missing or invalid attribute '%s' in 'DiskSection' element, line %d"),
                            pcszPath,
                            pcszBad,
                            pelmDisk->getLineNumber());

        m->mapDisks[d.strDiskId] = d;
    }

    return S_OK;
}

/**
 * Private helper method that handles network sections in the OVF XML.
 * Gets called indirectly from IAppliance::read().
 *
 * @param pcszPath Path spec of the XML file, for error messages.
 * @param pSectionElem Section element for which this helper is getting called.
 * @return
 */
HRESULT Appliance::HandleNetworkSection(const char *pcszPath,
                                        const xml::ElementNode *pSectionElem)
{
    // we ignore network sections for now

//     xml::NodesLoop loopNetworks(*pSectionElem, "Network");
//     const xml::Node *pelmNetwork;
//     while ((pelmNetwork = loopNetworks.forAllNodes()))
//     {
//         Network n;
//         if (!(pelmNetwork->getAttributeValue("name", n.strNetworkName)))
//             return setError(VBOX_E_FILE_ERROR,
//                             tr("Error reading \"%s\": missing 'name' attribute in 'Network', line %d"),
//                             pcszPath,
//                             pelmNetwork->getLineNumber());
//
//         m->mapNetworks[n.strNetworkName] = n;
//     }

    return S_OK;
}

/**
 * Private helper method that handles a "VirtualSystem" element in the OVF XML.
 * Gets called indirectly from IAppliance::read().
 *
 * @param pcszPath
 * @param pContentElem
 * @return
 */
HRESULT Appliance::HandleVirtualSystemContent(const char *pcszPath,
                                              const xml::ElementNode *pelmVirtualSystem)
{
    VirtualSystem vsys;

    const xml::AttributeNode *pIdAttr = pelmVirtualSystem->findAttribute("id");
    if (pIdAttr)
        vsys.strName = pIdAttr->getValue();

    xml::NodesLoop loop(*pelmVirtualSystem);      // all child elements
    const xml::ElementNode *pelmThis;
    while ((pelmThis = loop.forAllNodes()))
    {
        const char *pcszElemName = pelmThis->getName();
        const xml::AttributeNode *pTypeAttr = pelmThis->findAttribute("type");
        const char *pcszTypeAttr = (pTypeAttr) ? pTypeAttr->getValue() : "";

        if (!strcmp(pcszElemName, "EulaSection"))
        {
         /* <EulaSection>
                <Info ovf:msgid="6">License agreement for the Virtual System.</Info>
                <License ovf:msgid="1">License terms can go in here.</License>
            </EulaSection> */

            const xml::ElementNode *pelmInfo, *pelmLicense;
            if (    ((pelmInfo = pelmThis->findChildElement("Info")))
                 && ((pelmLicense = pelmThis->findChildElement("License")))
               )
            {
                vsys.strLicenceInfo = pelmInfo->getValue();
                vsys.strLicenceText = pelmLicense->getValue();
            }
        }
        else if (    (!strcmp(pcszElemName, "VirtualHardwareSection"))
                  || (!strcmp(pcszTypeAttr, "ovf:VirtualHardwareSection_Type"))
                )
        {
            const xml::ElementNode *pelmSystem, *pelmVirtualSystemType;
            if ((pelmSystem = pelmThis->findChildElement("System")))
            {
             /* <System>
                    <vssd:Description>Description of the virtual hardware section.</vssd:Description>
                    <vssd:ElementName>vmware</vssd:ElementName>
                    <vssd:InstanceID>1</vssd:InstanceID>
                    <vssd:VirtualSystemIdentifier>MyLampService</vssd:VirtualSystemIdentifier>
                    <vssd:VirtualSystemType>vmx-4</vssd:VirtualSystemType>
                </System>*/
                if ((pelmVirtualSystemType = pelmSystem->findChildElement("VirtualSystemType")))
                    vsys.strVirtualSystemType = pelmVirtualSystemType->getValue();
            }

            xml::NodesLoop loopVirtualHardwareItems(*pelmThis, "Item");      // all "Item" child elements
            const xml::ElementNode *pelmItem;
            while ((pelmItem = loopVirtualHardwareItems.forAllNodes()))
            {
                VirtualHardwareItem i;

                i.ulLineNumber = pelmItem->getLineNumber();

                xml::NodesLoop loopItemChildren(*pelmItem);      // all child elements
                const xml::ElementNode *pelmItemChild;
                while ((pelmItemChild = loopItemChildren.forAllNodes()))
                {
                    const char *pcszItemChildName = pelmItemChild->getName();
                    if (!strcmp(pcszItemChildName, "Description"))
                        i.strDescription = pelmItemChild->getValue();
                    else if (!strcmp(pcszItemChildName, "Caption"))
                        i.strCaption = pelmItemChild->getValue();
                    else if (!strcmp(pcszItemChildName, "ElementName"))
                        i.strElementName = pelmItemChild->getValue();
                    else if (    (!strcmp(pcszItemChildName, "InstanceID"))
                              || (!strcmp(pcszItemChildName, "InstanceId"))
                            )
                        pelmItemChild->copyValue(i.ulInstanceID);
                    else if (!strcmp(pcszItemChildName, "HostResource"))
                        i.strHostResource = pelmItemChild->getValue();
                    else if (!strcmp(pcszItemChildName, "ResourceType"))
                    {
                        uint32_t ulType;
                        pelmItemChild->copyValue(ulType);
                        i.resourceType = (OVFResourceType_T)ulType;
                    }
                    else if (!strcmp(pcszItemChildName, "OtherResourceType"))
                        i.strOtherResourceType = pelmItemChild->getValue();
                    else if (!strcmp(pcszItemChildName, "ResourceSubType"))
                        i.strResourceSubType = pelmItemChild->getValue();
                    else if (!strcmp(pcszItemChildName, "AutomaticAllocation"))
                        i.fAutomaticAllocation = (!strcmp(pelmItemChild->getValue(), "true")) ? true : false;
                    else if (!strcmp(pcszItemChildName, "AutomaticDeallocation"))
                        i.fAutomaticDeallocation = (!strcmp(pelmItemChild->getValue(), "true")) ? true : false;
                    else if (!strcmp(pcszItemChildName, "Parent"))
                        pelmItemChild->copyValue(i.ulParent);
                    else if (!strcmp(pcszItemChildName, "Connection"))
                        i.strConnection = pelmItemChild->getValue();
                    else if (!strcmp(pcszItemChildName, "Address"))
                        i.strAddress = pelmItemChild->getValue();
                    else if (!strcmp(pcszItemChildName, "AddressOnParent"))
                        i.strAddressOnParent = pelmItemChild->getValue();
                    else if (!strcmp(pcszItemChildName, "AllocationUnits"))
                        i.strAllocationUnits = pelmItemChild->getValue();
                    else if (!strcmp(pcszItemChildName, "VirtualQuantity"))
                        pelmItemChild->copyValue(i.ullVirtualQuantity);
                    else if (!strcmp(pcszItemChildName, "Reservation"))
                        pelmItemChild->copyValue(i.ullReservation);
                    else if (!strcmp(pcszItemChildName, "Limit"))
                        pelmItemChild->copyValue(i.ullLimit);
                    else if (!strcmp(pcszItemChildName, "Weight"))
                        pelmItemChild->copyValue(i.ullWeight);
                    else if (!strcmp(pcszItemChildName, "ConsumerVisibility"))
                        i.strConsumerVisibility = pelmItemChild->getValue();
                    else if (!strcmp(pcszItemChildName, "MappingBehavior"))
                        i.strMappingBehavior = pelmItemChild->getValue();
                    else if (!strcmp(pcszItemChildName, "PoolID"))
                        i.strPoolID = pelmItemChild->getValue();
                    else if (!strcmp(pcszItemChildName, "BusNumber"))
                        pelmItemChild->copyValue(i.ulBusNumber);
                    else
                        return setError(VBOX_E_FILE_ERROR,
                                        tr("Error reading \"%s\": unknown element \"%s\" under Item element, line %d"),
                                        pcszPath,
                                        pcszItemChildName,
                                        i.ulLineNumber);
                }

                // store!
                vsys.mapHardwareItems[i.ulInstanceID] = i;
            }

            HardwareItemsMap::const_iterator itH;

            for (itH = vsys.mapHardwareItems.begin();
                 itH != vsys.mapHardwareItems.end();
                 ++itH)
            {
                const VirtualHardwareItem &i = itH->second;

                // do some analysis
                switch (i.resourceType)
                {
                    case OVFResourceType_Processor:     // 3
                        /*  <rasd:Caption>1 virtual CPU</rasd:Caption>
                            <rasd:Description>Number of virtual CPUs</rasd:Description>
                            <rasd:ElementName>virtual CPU</rasd:ElementName>
                            <rasd:InstanceID>1</rasd:InstanceID>
                            <rasd:ResourceType>3</rasd:ResourceType>
                            <rasd:VirtualQuantity>1</rasd:VirtualQuantity>*/
                        if (i.ullVirtualQuantity < UINT16_MAX)
                            vsys.cCPUs = (uint16_t)i.ullVirtualQuantity;
                        else
                            return setError(VBOX_E_FILE_ERROR,
                                            tr("Error reading \"%s\": CPU count %RI64 is larger than %d, line %d"),
                                            pcszPath,
                                            i.ullVirtualQuantity,
                                            UINT16_MAX,
                                            i.ulLineNumber);
                    break;

                    case OVFResourceType_Memory:        // 4
                        if (    (i.strAllocationUnits == "MegaBytes")           // found in OVF created by OVF toolkit
                             || (i.strAllocationUnits == "MB")                  // found in MS docs
                             || (i.strAllocationUnits == "byte * 2^20")         // suggested by OVF spec DSP0243 page 21
                           )
                            vsys.ullMemorySize = i.ullVirtualQuantity * 1024 * 1024;
                        else
                            return setError(VBOX_E_FILE_ERROR,
                                            tr("Error reading \"%s\": Invalid allocation unit \"%s\" specified with memory size item, line %d"),
                                            pcszPath,
                                            i.strAllocationUnits.c_str(),
                                            i.ulLineNumber);
                    break;

                    case OVFResourceType_IdeController:          // 5       IdeController
                    {
                        /*  <Item>
                                <rasd:Caption>ideController0</rasd:Caption>
                                <rasd:Description>IDE Controller</rasd:Description>
                                <rasd:InstanceId>5</rasd:InstanceId>
                                <rasd:ResourceType>5</rasd:ResourceType>
                                <rasd:Address>0</rasd:Address>
                                <rasd:BusNumber>0</rasd:BusNumber>
                            </Item> */
                        HardDiskController hdc;
                        hdc.system = HardDiskController::IDE;
                        hdc.idController = i.ulInstanceID;
                        hdc.strAddress = i.strAddress;
                        hdc.ulBusNumber = i.ulBusNumber;

                        vsys.mapControllers[i.ulInstanceID] = hdc;
                    }
                    break;

                    case OVFResourceType_ParallelScsiHba:        // 6       SCSI controller
                    {
                        /*  <Item>
                                <rasd:Caption>SCSI Controller 0 - LSI Logic</rasd:Caption>
                                <rasd:Description>SCI Controller</rasd:Description>
                                <rasd:ElementName>SCSI controller</rasd:ElementName>
                                <rasd:InstanceID>4</rasd:InstanceID>
                                <rasd:ResourceSubType>LsiLogic</rasd:ResourceSubType>
                                <rasd:ResourceType>6</rasd:ResourceType>
                            </Item> */
                        HardDiskController hdc;
                        hdc.system = HardDiskController::SCSI;
                        hdc.idController = i.ulInstanceID;
                        hdc.strControllerType = i.strResourceSubType;

                        vsys.mapControllers[i.ulInstanceID] = hdc;
                    }
                    break;

                    case OVFResourceType_EthernetAdapter: // 10
                    {
                        /*  <Item>
                                <rasd:AutomaticAllocation>true</rasd:AutomaticAllocation>
                                <rasd:Caption>Ethernet adapter on 'VM Network'</rasd:Caption>
                                <rasd:Connection>VM Network</rasd:Connection>
                                <rasd:Description>VM Network?</rasd:Description>
                                <rasd:ElementName>Ethernet adapter</rasd:ElementName>
                                <rasd:InstanceID>3</rasd:InstanceID>
                                <rasd:ResourceType>10</rasd:ResourceType>
                            </Item>

                            OVF spec DSP 0243 page 21:
                           "For an Ethernet adapter, this specifies the abstract network connection name
                            for the virtual machine. All Ethernet adapters that specify the same abstract
                            network connection name within an OVF package shall be deployed on the same
                            network. The abstract network connection name shall be listed in the NetworkSection
                            at the outermost envelope level." */

                        // only store the name
                        vsys.llNetworkNames.push_back(i.strConnection);
                    }
                    break;

                    case OVFResourceType_FloppyDrive: // 14
                        vsys.fHasFloppyDrive = true;           // we have no additional information
                    break;

                    case OVFResourceType_CdDrive:       // 15
                        /*  <Item ovf:required="false">
                                <rasd:Caption>cdrom1</rasd:Caption>
                                <rasd:InstanceId>7</rasd:InstanceId>
                                <rasd:ResourceType>15</rasd:ResourceType>
                                <rasd:AutomaticAllocation>true</rasd:AutomaticAllocation>
                                <rasd:Parent>5</rasd:Parent>
                                <rasd:AddressOnParent>0</rasd:AddressOnParent>
                            </Item> */
                            // I tried to see what happens if I set an ISO for the CD-ROM in VMware Workstation,
                            // but then the ovftool dies with "Device backing not supported". So I guess if
                            // VMware can't export ISOs, then we don't need to be able to import them right now.
                        vsys.fHasCdromDrive = true;           // we have no additional information
                    break;

                    case OVFResourceType_HardDisk: // 17
                    {
                        /*  <Item>
                                <rasd:Caption>Harddisk 1</rasd:Caption>
                                <rasd:Description>HD</rasd:Description>
                                <rasd:ElementName>Hard Disk</rasd:ElementName>
                                <rasd:HostResource>ovf://disk/lamp</rasd:HostResource>
                                <rasd:InstanceID>5</rasd:InstanceID>
                                <rasd:Parent>4</rasd:Parent>
                                <rasd:ResourceType>17</rasd:ResourceType>
                            </Item> */

                        // look up the hard disk controller element whose InstanceID equals our Parent;
                        // this is how the connection is specified in OVF
                        ControllersMap::const_iterator it = vsys.mapControllers.find(i.ulParent);
                        if (it == vsys.mapControllers.end())
                            return setError(VBOX_E_FILE_ERROR,
                                            tr("Error reading \"%s\": Hard disk item with instance ID %d specifies invalid parent %d, line %d"),
                                            pcszPath,
                                            i.ulInstanceID,
                                            i.ulParent,
                                            i.ulLineNumber);
                        const HardDiskController &hdc = it->second;

                        VirtualDisk vd;
                        vd.idController = i.ulParent;
                        i.strAddressOnParent.toInt(vd.ulAddressOnParent);
                        bool fFound = false;
                        // ovf://disk/lamp
                        // 12345678901234
                        if (i.strHostResource.substr(0, 11) == "ovf://disk/")
                            vd.strDiskId = i.strHostResource.substr(11);
                        else if (i.strHostResource.substr(0, 6) == "/disk/")
                            vd.strDiskId = i.strHostResource.substr(6);

                        if (    !(vd.strDiskId.length())
                             || (m->mapDisks.find(vd.strDiskId) == m->mapDisks.end())
                           )
                            return setError(VBOX_E_FILE_ERROR,
                                            tr("Error reading \"%s\": Hard disk item with instance ID %d specifies invalid host resource \"%s\", line %d"),
                                            pcszPath,
                                            i.ulInstanceID,
                                            i.strHostResource.c_str(),
                                            i.ulLineNumber);

                        vsys.mapVirtualDisks[vd.strDiskId] = vd;
                    }
                    break;

                    case OVFResourceType_OtherStorageDevice:        // 20       SATA controller
                    {
                        /* <Item>
                            <rasd:Description>SATA Controller</rasd:Description>
                            <rasd:Caption>sataController0</rasd:Caption>
                            <rasd:InstanceID>4</rasd:InstanceID>
                            <rasd:ResourceType>20</rasd:ResourceType>
                            <rasd:ResourceSubType>AHCI</rasd:ResourceSubType>
                            <rasd:Address>0</rasd:Address>
                            <rasd:BusNumber>0</rasd:BusNumber>
                        </Item> */
                        if (i.strCaption.startsWith ("sataController", Utf8Str::CaseInsensitive) &&
                            !i.strResourceSubType.compare ("AHCI", Utf8Str::CaseInsensitive))
                        {
                            HardDiskController hdc;
                            hdc.system = HardDiskController::SATA;
                            hdc.idController = i.ulInstanceID;
                            hdc.strControllerType = i.strResourceSubType;

                            vsys.mapControllers[i.ulInstanceID] = hdc;
                        }
                        else
                            return setError(VBOX_E_FILE_ERROR,
                                            tr("Error reading \"%s\": Host resource of type \"Other Storage Device (%d)\" is supported with SATA AHCI controllers only, line %d"),
                                            pcszPath,
                                            OVFResourceType_OtherStorageDevice,
                                            i.ulLineNumber);
                    }
                    break;

                    case OVFResourceType_UsbController: // 23
                        /*  <Item ovf:required="false">
                                <rasd:Caption>usb</rasd:Caption>
                                <rasd:Description>USB Controller</rasd:Description>
                                <rasd:InstanceId>3</rasd:InstanceId>
                                <rasd:ResourceType>23</rasd:ResourceType>
                                <rasd:Address>0</rasd:Address>
                                <rasd:BusNumber>0</rasd:BusNumber>
                            </Item> */
                        vsys.fHasUsbController = true;           // we have no additional information
                    break;

                    case OVFResourceType_SoundCard: // 35
                        /*  <Item ovf:required="false">
                                <rasd:Caption>sound</rasd:Caption>
                                <rasd:Description>Sound Card</rasd:Description>
                                <rasd:InstanceId>10</rasd:InstanceId>
                                <rasd:ResourceType>35</rasd:ResourceType>
                                <rasd:ResourceSubType>ensoniq1371</rasd:ResourceSubType>
                                <rasd:AutomaticAllocation>false</rasd:AutomaticAllocation>
                                <rasd:AddressOnParent>3</rasd:AddressOnParent>
                            </Item> */
                        vsys.strSoundCardType = i.strResourceSubType;
                    break;

                    default:
                        return setError(VBOX_E_FILE_ERROR,
                                        tr("Error reading \"%s\": Unknown resource type %d in hardware item, line %d"),
                                        pcszPath,
                                        i.resourceType,
                                        i.ulLineNumber);
                }
            }
        }
        else if (    (!strcmp(pcszElemName, "OperatingSystemSection"))
                  || (!strcmp(pcszTypeAttr, "ovf:OperatingSystemSection_Type"))
                )
        {
            uint64_t cimos64;
            if (!(pelmThis->getAttributeValue("id", cimos64)))
                return setError(VBOX_E_FILE_ERROR,
                                tr("Error reading \"%s\": missing or invalid 'ovf:id' attribute in operating system section element, line %d"),
                                pcszPath,
                                pelmThis->getLineNumber());

            vsys.cimos = (CIMOSType_T)cimos64;
        }
    }

    // now create the virtual system
    m->llVirtualSystems.push_back(vsys);

    return S_OK;
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

    Bstr bstrPath(m->strPath);
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

    size_t c = m->mapDisks.size();
    com::SafeArray<BSTR> sfaDisks(c);

    DiskImagesMap::const_iterator it;
    size_t i = 0;
    for (it = m->mapDisks.begin();
         it != m->mapDisks.end();
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
STDMETHODIMP Appliance::Read(IN_BSTR path)
{
    HRESULT rc = S_OK;

    if (!path)
        return E_POINTER;

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    // see if we can handle this file; for now we insist it has an ".ovf" extension
    m->strPath = path;
    if (!m->strPath.endsWith(".ovf", Utf8Str::CaseInsensitive))
        return setError(VBOX_E_FILE_ERROR,
                        tr("Appliance file must have .ovf extension"));

    try
    {
        xml::XmlFileParser parser;
        xml::Document doc;
        parser.read(m->strPath.raw(),
                    doc);

        const xml::ElementNode *pRootElem = doc.getRootElement();
        if (strcmp(pRootElem->getName(), "Envelope"))
            return setError(VBOX_E_FILE_ERROR,
                            tr("Root element in OVF file must be \"Envelope\"."));

        // OVF has the following rough layout:
        /*
            -- <References> ....  files referenced from other parts of the file, such as VMDK images
            -- Metadata, comprised of several section commands
            -- virtual machines, either a single <VirtualSystem>, or a <VirtualSystemCollection>
            -- optionally <Strings> for localization
        */

        // get all "File" child elements of "References" section so we can look up files easily;
        // first find the "References" sections so we can look up files
        xml::ElementNodesList listFileElements;      // receives all /Envelope/References/File nodes
        const xml::ElementNode *pReferencesElem;
        if ((pReferencesElem = pRootElem->findChildElement("References")))
            pReferencesElem->getChildElements(listFileElements, "File");

        // now go though the sections
        if (!(SUCCEEDED(rc = LoopThruSections(m->strPath.raw(), pReferencesElem, pRootElem))))
            return rc;
    }
    catch(xml::Error &x)
    {
        return setError(VBOX_E_FILE_ERROR,
                        x.what());
    }

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

    /* Try/catch so we can clean up on error */
    try
    {
        list<VirtualSystem>::const_iterator it;
        /* Iterate through all virtual systems */
        for (it = m->llVirtualSystems.begin();
             it != m->llVirtualSystems.end();
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
            convertCIMOSType2VBoxOSType(strOsTypeVBox, vsysThis.cimos);
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

            /* Now that we know the OS type, get our internal defaults based on that. */
            ComPtr<IGuestOSType> pGuestOSType;
            rc = mVirtualBox->GetGuestOSType(Bstr(strOsTypeVBox), pGuestOSType.asOutParam());
            CheckComRCThrowRC(rc);

            /* CPU count */
            ULONG cpuCountVBox = vsysThis.cCPUs;
            /* Check for the constrains */
            if (cpuCountVBox > 1) //SchemaDefs::MaxCPUCount)
            {
                addWarning(tr("The virtual system \"%s\" claims support for %u CPU's, but VirtualBox has support for max %u CPU's only."),
                           vsysThis.strName.c_str(), cpuCountVBox, 1); //SchemaDefs::MaxCPUCount);
                cpuCountVBox = 1; //SchemaDefs::MaxCPUCount;
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
                (ullMemSizeVBox < static_cast<uint64_t>(SchemaDefs::MinGuestRAM) ||
                 ullMemSizeVBox > static_cast<uint64_t>(SchemaDefs::MaxGuestRAM)))
            {
                addWarning(tr("The virtual system \"%s\" claims support for %llu MB RAM size, but VirtualBox has support for min %u & max %u MB RAM size only."),
                              vsysThis.strName.c_str(), ullMemSizeVBox, SchemaDefs::MinGuestRAM, SchemaDefs::MaxGuestRAM);
                ullMemSizeVBox = RT_MIN(RT_MAX(ullMemSizeVBox, static_cast<uint64_t>(SchemaDefs::MinGuestRAM)), static_cast<uint64_t>(SchemaDefs::MaxGuestRAM));
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
            if (!vsysThis.strSoundCardType.isNull())
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
            // @todo: there is no hardware specification in the OVF file; supposedly the
            // hardware will then be determined by the VirtualSystemType element (e.g. "vmx-07")
            if (vsysThis.llNetworkNames.size() > 0)
            {
                /* Check for the constrains */
                if (vsysThis.llNetworkNames.size() > SchemaDefs::NetworkAdapterCount)
                    addWarning(tr("The virtual system \"%s\" claims support for %u network adapters, but VirtualBox has support for max %u network adapter only."),
                                  vsysThis.strName.c_str(), vsysThis.llNetworkNames.size(), SchemaDefs::NetworkAdapterCount);

                /* Get the default network adapter type for the selected guest OS */
                NetworkAdapterType_T nwAdapterVBox = NetworkAdapterType_Am79C970A;
                rc = pGuestOSType->COMGETTER(AdapterType)(&nwAdapterVBox);
                CheckComRCThrowRC(rc);
                list<Utf8Str>::const_iterator nwIt;
                /* Iterate through all abstract networks. We support 8 network
                 * adapters at the maximum, so the first 8 will be added only. */
                size_t a = 0;
                for (nwIt = vsysThis.llNetworkNames.begin();
                     nwIt != vsysThis.llNetworkNames.end() && a < SchemaDefs::NetworkAdapterCount;
                     ++nwIt, ++a)
                {
                    Utf8Str strNetwork = *nwIt; // logical network to connect to
                    // make sure it's one of these two
                    if (    (strNetwork.compare("Null", Utf8Str::CaseInsensitive))
                         && (strNetwork.compare("Bridged", Utf8Str::CaseInsensitive))
                         && (strNetwork.compare("Internal", Utf8Str::CaseInsensitive))
                         && (strNetwork.compare("HostOnly", Utf8Str::CaseInsensitive))
                       )
                        strNetwork = "NAT";

                    pNewDesc->addEntry(VirtualSystemDescriptionType_NetworkAdapter,
                                       "",      // ref
                                       strNetwork,      // orig
                                       Utf8StrFmt("%RI32", (uint32_t)nwAdapterVBox),   // conf
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
            uint16_t cSATAused = 0;
            uint16_t cSCSIused = 0;
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

#ifdef VBOX_WITH_AHCI
                    case HardDiskController::SATA:
                        {
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
                        }
#endif /* VBOX_WITH_AHCI */

                    case HardDiskController::SCSI:
                        {
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
                    const DiskImage &di = m->mapDisks[hd.strDiskId];

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
                        pNewDesc->addEntry(VirtualSystemDescriptionType_HardDiskImage,
                                           hd.strDiskId,
                                           di.strHref,
                                           strPath,
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

    HRESULT rc = S_OK;

    ComObjPtr<Progress> progress;
    try
    {
        uint32_t opCount = calcMaxProgress();
        Bstr progressDesc = BstrFmt(tr("Import appliance '%s'"),
                                    m->strPath.raw());
        /* Create the progress object */
        progress.createObject();
        rc = progress->init(mVirtualBox, static_cast<IAppliance*>(this),
                            progressDesc,
                            FALSE /* aCancelable */,
                            opCount,
                            progressDesc);
        if (FAILED(rc)) throw rc;

        /* Initialize our worker task */
        std::auto_ptr<TaskImportMachines> task(new TaskImportMachines(this, progress));
        //AssertComRCThrowRC (task->autoCaller.rc());

        rc = task->startThread();
        if (FAILED(rc)) throw rc;

        task.release();
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

STDMETHODIMP Appliance::Write(IN_BSTR path, IProgress **aProgress)
{
    HRESULT rc = S_OK;

    CheckComArgOutPointerValid(aProgress);

    AutoCaller autoCaller(this);
    if (FAILED(rc = autoCaller.rc())) return rc;

    AutoWriteLock(this);

    // see if we can handle this file; for now we insist it has an ".ovf" extension
    m->strPath = path;
    if (!m->strPath.endsWith(".ovf", Utf8Str::CaseInsensitive))
        return setError(VBOX_E_FILE_ERROR,
                        tr("Appliance file must have .ovf extension"));

    ComObjPtr<Progress> progress;
    try
    {
        uint32_t opCount = calcMaxProgress();
        Bstr progressDesc = BstrFmt(tr("Write appliance '%s'"),
                                    m->strPath.raw());
        /* Create the progress object */
        progress.createObject();
        rc = progress->init(mVirtualBox, static_cast<IAppliance*>(this),
                            progressDesc,
                            FALSE /* aCancelable */,
                            opCount,
                            progressDesc);
        CheckComRCThrowRC(rc);

        /* Initialize our worker task */
        std::auto_ptr<TaskWriteOVF> task(new TaskWriteOVF(this, progress));
        //AssertComRCThrowRC (task->autoCaller.rc());

        rc = task->startThread();
        CheckComRCThrowRC(rc);

        task.release();
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
    IHardDisk *harddisk = NULL;
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
 * Calculates the maximum progress value for importMachines() and write().
 * @return
 */
uint32_t Appliance::calcMaxProgress()
{
    /* Figure out how many sub operation the import will need */
    /* One for the appliance */
    uint32_t opCount = 1;
    list< ComObjPtr<VirtualSystemDescription> >::const_iterator it;
    for (it = m->virtualSystemDescriptions.begin();
         it != m->virtualSystemDescriptions.end();
         ++it)
    {
        /* One for every Virtual System */
        ++opCount;
        ComObjPtr<VirtualSystemDescription> vsdescThis = (*it);
        /* One for every hard disk of the Virtual System */
        std::list<VirtualSystemDescriptionEntry*> avsdeHDs = vsdescThis->findByType(VirtualSystemDescriptionType_HardDiskImage);
        opCount += (uint32_t)avsdeHDs.size();
    }

    return opCount;
}

void Appliance::addWarning(const char* aWarning, ...)
{
    va_list args;
    va_start(args, aWarning);
    Utf8StrFmtVA str(aWarning, args);
    va_end(args);
    m->llWarnings.push_back(str);
}

struct MyHardDiskAttachment
{
    Guid    uuid;
    ComPtr<IMachine> pMachine;
    Bstr    controllerType;
    int32_t lChannel;
    int32_t lDevice;
};

/**
 * Worker thread implementation for ImportMachines().
 * @param aThread
 * @param pvUser
 */
/* static */
DECLCALLBACK(int) Appliance::taskThreadImportMachines(RTTHREAD aThread, void *pvUser)
{
    std::auto_ptr<TaskImportMachines> task(static_cast<TaskImportMachines*>(pvUser));
    AssertReturn(task.get(), VERR_GENERAL_FAILURE);

    Appliance *pAppliance = task->pAppliance;

    LogFlowFuncEnter();
    LogFlowFunc(("Appliance %p\n", pAppliance));

    AutoCaller autoCaller(pAppliance);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock appLock(pAppliance);

    HRESULT rc = S_OK;

    ComPtr<IVirtualBox> pVirtualBox(pAppliance->mVirtualBox);

    // rollback for errors:
    // 1) a list of images that we created/imported
    list<MyHardDiskAttachment> llHardDiskAttachments;
    list< ComPtr<IHardDisk> > llHardDisksCreated;
    list<Guid> llMachinesRegistered;

    ComPtr<ISession> session;
    bool fSessionOpen = false;
    rc = session.createInprocObject(CLSID_Session);
    CheckComRCReturnRC(rc);

    list<VirtualSystem>::const_iterator it;
    list< ComObjPtr<VirtualSystemDescription> >::const_iterator it1;
    /* Iterate through all virtual systems of that appliance */
    size_t i = 0;
    for (it = pAppliance->m->llVirtualSystems.begin(),
            it1 = pAppliance->m->virtualSystemDescriptions.begin();
         it != pAppliance->m->llVirtualSystems.end();
         ++it, ++it1, ++i)
    {
        const VirtualSystem &vsysThis = *it;
        ComObjPtr<VirtualSystemDescription> vsdescThis = (*it1);

        ComPtr<IMachine> pNewMachine;

        /* Catch possible errors */
        try
        {
            if (!task->progress.isNull())
                task->progress->advanceOperation(BstrFmt(tr("Importing Virtual System %d"), i + 1));

            /* How many sub notifications are necessary? */
            const float opCountMax = 100.0/5;
            uint32_t opCount = 0;

            /* Guest OS type */
            std::list<VirtualSystemDescriptionEntry*> vsdeOS;
            vsdeOS = vsdescThis->findByType(VirtualSystemDescriptionType_OS);
            if (vsdeOS.size() < 1)
                throw setError(VBOX_E_FILE_ERROR,
                               tr("Missing guest OS type"));
            const Utf8Str &strOsTypeVBox = vsdeOS.front()->strVbox;

            /* Now that we know the base system get our internal defaults based on that. */
            ComPtr<IGuestOSType> osType;
            rc = pVirtualBox->GetGuestOSType(Bstr(strOsTypeVBox), osType.asOutParam());
            if (FAILED(rc)) throw rc;

            /* Create the machine */
            /* First get the name */
            std::list<VirtualSystemDescriptionEntry*> vsdeName = vsdescThis->findByType(VirtualSystemDescriptionType_Name);
            if (vsdeName.size() < 1)
                throw setError(VBOX_E_FILE_ERROR,
                               tr("Missing VM name"));
            const Utf8Str &strNameVBox = vsdeName.front()->strVbox;
            rc = pVirtualBox->CreateMachine(Bstr(strNameVBox), Bstr(strOsTypeVBox),
                                                 Bstr(), Guid(),
                                                 pNewMachine.asOutParam());
            if (FAILED(rc)) throw rc;

            if (!task->progress.isNull())
                rc = task->progress->notifyProgress((uint32_t)(opCountMax * opCount++));

            /* CPU count (ignored for now) */
            // EntriesList vsdeCPU = vsd->findByType (VirtualSystemDescriptionType_CPU);

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

            if (!task->progress.isNull())
                task->progress->notifyProgress((uint32_t)(opCountMax * opCount++));

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

            if (!task->progress.isNull())
                task->progress->notifyProgress((uint32_t)(opCountMax * opCount++));

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
                        rc = pNetworkAdapter->AttachToBridgedInterface();
                        if (FAILED(rc)) throw rc;
                    }
                }
            }

            /* Floppy drive */
            std::list<VirtualSystemDescriptionEntry*> vsdeFloppy = vsdescThis->findByType(VirtualSystemDescriptionType_Floppy);
            // Floppy support is enabled if there's at least one such entry; to disable floppy support,
            // the type of the floppy item would have been changed to "ignore"
            bool fFloppyEnabled = vsdeFloppy.size() > 0;
            ComPtr<IFloppyDrive> floppyDrive;
            rc = pNewMachine->COMGETTER(FloppyDrive)(floppyDrive.asOutParam());
            if (FAILED(rc)) throw rc;
            rc = floppyDrive->COMSETTER(Enabled)(fFloppyEnabled);
            if (FAILED(rc)) throw rc;

            if (!task->progress.isNull())
                task->progress->notifyProgress((uint32_t)(opCountMax * opCount++));

            /* CDROM drive */
            /* @todo: I can't disable the CDROM. So nothing to do for now */
            // std::list<VirtualSystemDescriptionEntry*> vsdeFloppy = vsd->findByType(VirtualSystemDescriptionType_CDROM);

            /* Hard disk controller IDE */
            std::list<VirtualSystemDescriptionEntry*> vsdeHDCIDE = vsdescThis->findByType(VirtualSystemDescriptionType_HardDiskControllerIDE);
            if (vsdeHDCIDE.size() > 1)
                throw setError(VBOX_E_FILE_ERROR,
                               tr("Too many IDE controllers in OVF; VirtualBox only supports one"));
            if (vsdeHDCIDE.size() == 1)
            {
                ComPtr<IStorageController> pController;
                rc = pNewMachine->GetStorageControllerByName(Bstr("IDE"), pController.asOutParam());
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
                    rc = pNewMachine->AddStorageController(Bstr("SATA"), StorageBus_SATA, pController.asOutParam());
                    if (FAILED(rc)) throw rc;
                }
                else
                    throw setError(VBOX_E_FILE_ERROR,
                                   tr("Invalid SATA controller type \"%s\""),
                                   hdcVBox.c_str());
            }
#endif /* VBOX_WITH_AHCI */

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

                rc = pNewMachine->AddStorageController(Bstr("SCSI"), StorageBus_SCSI, pController.asOutParam());
                if (FAILED(rc)) throw rc;
                rc = pController->COMSETTER(ControllerType)(controllerType);
                if (FAILED(rc)) throw rc;
            }

            /* Now its time to register the machine before we add any hard disks */
            rc = pVirtualBox->RegisterMachine(pNewMachine);
            if (FAILED(rc)) throw rc;

            Guid newMachineId;
            rc = pNewMachine->COMGETTER(Id)(newMachineId.asOutParam());
            if (FAILED(rc)) throw rc;

            if (!task->progress.isNull())
                task->progress->notifyProgress((uint32_t)(opCountMax * opCount++));

            // store new machine for roll-back in case of errors
            llMachinesRegistered.push_back(newMachineId);

            /* Create the hard disks & connect them to the appropriate controllers. */
            std::list<VirtualSystemDescriptionEntry*> avsdeHDs = vsdescThis->findByType(VirtualSystemDescriptionType_HardDiskImage);
            if (avsdeHDs.size() > 0)
            {
                /* If in the next block an error occur we have to deregister
                   the machine, so make an extra try/catch block. */
                ComPtr<IHardDisk> srcHdVBox;
                bool fSourceHdNeedsClosing = false;

                try
                {
                    /* In order to attach hard disks we need to open a session
                     * for the new machine */
                    rc = pVirtualBox->OpenSession(session, newMachineId);
                    if (FAILED(rc)) throw rc;
                    fSessionOpen = true;

                    /* The disk image has to be on the same place as the OVF file. So
                     * strip the filename out of the full file path. */
                    Utf8Str strSrcDir = stripFilename(pAppliance->m->strPath);

                    /* Iterate over all given disk images */
                    list<VirtualSystemDescriptionEntry*>::const_iterator itHD;
                    for (itHD = avsdeHDs.begin();
                         itHD != avsdeHDs.end();
                         ++itHD)
                    {
                        VirtualSystemDescriptionEntry *vsdeHD = *itHD;

                        const char *pcszDstFilePath = vsdeHD->strVbox.c_str();
                        /* Check if the destination file exists already or the
                         * destination path is empty. */
                        if (    !(*pcszDstFilePath)
                             || RTPathExists(pcszDstFilePath)
                           )
                            /* This isn't allowed */
                            throw setError(VBOX_E_FILE_ERROR,
                                           tr("Destination file '%s' exists",
                                              pcszDstFilePath));

                        /* Find the disk from the OVF's disk list */
                        DiskImagesMap::const_iterator itDiskImage = pAppliance->m->mapDisks.find(vsdeHD->strRef);
                        /* vsdeHD->strRef contains the disk identifier (e.g. "vmdisk1"), which should exist
                           in the virtual system's disks map under that ID and also in the global images map. */
                        VirtualDisksMap::const_iterator itVirtualDisk = vsysThis.mapVirtualDisks.find(vsdeHD->strRef);

                        if (    itDiskImage == pAppliance->m->mapDisks.end()
                             || itVirtualDisk == vsysThis.mapVirtualDisks.end()
                           )
                            throw setError(E_FAIL,
                                           tr("Internal inconsistency looking up disk images."));

                        const DiskImage &di = itDiskImage->second;
                        const VirtualDisk &vd = itVirtualDisk->second;

                        /* Make sure all target directories exists */
                        rc = VirtualBox::ensureFilePathExists(pcszDstFilePath);
                        if (FAILED(rc))
                            throw rc;

                        ComPtr<IProgress> progress;

                        ComPtr<IHardDisk> dstHdVBox;
                        /* If strHref is empty we have to create a new file */
                        if (di.strHref.isEmpty())
                        {
                            /* Which format to use? */
                            Bstr srcFormat = L"VDI";
                            if (   di.strFormat.compare("http://www.vmware.com/specifications/vmdk.html#sparse", Utf8Str::CaseInsensitive)
                                || di.strFormat.compare("http://www.vmware.com/specifications/vmdk.html#compressed", Utf8Str::CaseInsensitive))
                                srcFormat = L"VMDK";
                            /* Create an empty hard disk */
                            rc = pVirtualBox->CreateHardDisk(srcFormat, Bstr(pcszDstFilePath), dstHdVBox.asOutParam());
                            if (FAILED(rc)) throw rc;

                            /* Create a dynamic growing disk image with the given capacity */
                            rc = dstHdVBox->CreateDynamicStorage(di.iCapacity / _1M, HardDiskVariant_Standard, progress.asOutParam());
                            if (FAILED(rc)) throw rc;

                            /* Advance to the next operation */
                            if (!task->progress.isNull())
                                task->progress->advanceOperation (BstrFmt(tr("Creating virtual disk image '%s'"), pcszDstFilePath));
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
                            rc = pVirtualBox->OpenHardDisk(Bstr(strSrcFilePath), srcHdVBox.asOutParam());
                            if (FAILED(rc)) throw rc;
                            fSourceHdNeedsClosing = true;

                            /* We need the format description of the source disk image */
                            Bstr srcFormat;
                            rc = srcHdVBox->COMGETTER(Format)(srcFormat.asOutParam());
                            if (FAILED(rc)) throw rc;
                            /* Create a new hard disk interface for the destination disk image */
                            rc = pVirtualBox->CreateHardDisk(srcFormat, Bstr(pcszDstFilePath), dstHdVBox.asOutParam());
                            if (FAILED(rc)) throw rc;
                            /* Clone the source disk image */
                            rc = srcHdVBox->CloneTo(dstHdVBox, HardDiskVariant_Standard, progress.asOutParam());
                            if (FAILED(rc)) throw rc;

                            /* Advance to the next operation */
                            if (!task->progress.isNull())
                                task->progress->advanceOperation (BstrFmt(tr("Importing virtual disk image '%s'"), strSrcFilePath.c_str()));
                        }

                        // now loop until the asynchronous operation completes and then
                        // report its result
                        BOOL fCompleted;
                        LONG currentPercent;
                        while (SUCCEEDED(progress->COMGETTER(Completed(&fCompleted))))
                        {
                            rc = progress->COMGETTER(Percent(&currentPercent));
                            if (FAILED(rc)) throw rc;
                            if (!task->progress.isNull())
                                task->progress->notifyProgress(currentPercent);
                            if (fCompleted)
                                break;
                            /* Make sure the loop is not too tight */
                            rc = progress->WaitForCompletion(100);
                            if (FAILED(rc)) throw rc;
                        }
                        // report result of asynchronous operation
                        HRESULT vrc;
                        rc = progress->COMGETTER(ResultCode)(&vrc);
                        if (FAILED(rc)) throw rc;

                        // if the thread of the progress object has an error, then
                        // retrieve the error info from there, or it'll be lost
                        if (FAILED(vrc))
                        {
                            ProgressErrorInfo info(progress);
                            Utf8Str str(info.getText());
                            const char *pcsz = str.c_str();
                            HRESULT rc2 = setError(vrc,
                                                   pcsz);
                            throw rc2;
                        }

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
                        Guid hdId;
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
                                mhda.controllerType = Bstr("IDE");
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
                                mhda.controllerType = Bstr("SATA");
                                mhda.lChannel = (long)vd.ulAddressOnParent;
                                mhda.lDevice = (long)0;
                            break;

                            case HardDiskController::SCSI:
                                mhda.controllerType = Bstr("SCSI");
                                mhda.lChannel = (long)vd.ulAddressOnParent;
                                mhda.lDevice = (long)0;
                            break;

                            default: break;
                        }

                        Log(("Attaching disk %s to channel %d on device %d\n", pcszDstFilePath, mhda.lChannel, mhda.lDevice));

                        rc = sMachine->AttachHardDisk(hdId,
                                                      mhda.controllerType,
                                                      mhda.lChannel,
                                                      mhda.lDevice);
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
            rc2 = pVirtualBox->OpenSession(session, mhda.uuid);
            if (SUCCEEDED(rc2))
            {
                ComPtr<IMachine> sMachine;
                rc2 = session->COMGETTER(Machine)(sMachine.asOutParam());
                if (SUCCEEDED(rc2))
                {
                    rc2 = sMachine->DetachHardDisk(Bstr(mhda.controllerType), mhda.lChannel, mhda.lDevice);
                    rc2 = sMachine->SaveSettings();
                }
                session->Close();
            }
        }

        // now clean up all hard disks we created
        list< ComPtr<IHardDisk> >::iterator itHD;
        for (itHD = llHardDisksCreated.begin();
             itHD != llHardDisksCreated.end();
             ++itHD)
        {
            ComPtr<IHardDisk> pDisk = *itHD;
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
            rc2 = pVirtualBox->UnregisterMachine(guid, failedMachine.asOutParam());
            if (SUCCEEDED(rc2))
                rc2 = failedMachine->DeleteSettings();
        }
    }

    task->rc = rc;

    if (!task->progress.isNull())
        task->progress->notifyComplete(rc);

    LogFlowFunc(("rc=%Rhrc\n", rc));
    LogFlowFuncLeave();

    return VINF_SUCCESS;
}

/**
 * Worker thread implementation for Write() (ovf writer).
 * @param aThread
 * @param pvUser
 */
/* static */
DECLCALLBACK(int) Appliance::taskThreadWriteOVF(RTTHREAD aThread, void *pvUser)
{
    std::auto_ptr<TaskWriteOVF> task(static_cast<TaskWriteOVF*>(pvUser));
    AssertReturn(task.get(), VERR_GENERAL_FAILURE);

    Appliance *pAppliance = task->pAppliance;

    LogFlowFuncEnter();
    LogFlowFunc(("Appliance %p\n", pAppliance));

    AutoCaller autoCaller(pAppliance);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock appLock(pAppliance);

    HRESULT rc = S_OK;

    ComPtr<IVirtualBox> pVirtualBox(pAppliance->mVirtualBox);

    try
    {
        xml::Document doc;
        xml::ElementNode *pelmRoot = doc.createRootElement("Envelope");

        pelmRoot->setAttribute("ovf:version", "1.0");
        pelmRoot->setAttribute("xml:lang", "en-US");
        pelmRoot->setAttribute("xmlns", "http://schemas.dmtf.org/ovf/envelope/1");
        pelmRoot->setAttribute("xmlns:ovf", "http://schemas.dmtf.org/ovf/envelope/1");
        pelmRoot->setAttribute("xmlns:ovfstr", "http://schema.dmtf.org/ovf/strings/1");
        pelmRoot->setAttribute("xmlns:rasd", "http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_ResourceAllocationSettingData");
        pelmRoot->setAttribute("xmlns:vssd", "http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_VirtualSystemSettingData");
        pelmRoot->setAttribute("xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance");
        pelmRoot->setAttribute("xsi:schemaLocation", "http://schemas.dmtf.org/ovf/envelope/1 ../ovf-envelope.xsd");


        // <Envelope>/<References>
        xml::ElementNode *pelmReferences = pelmRoot->createChild("References");
                // @ŧodo

        /* <Envelope>/<DiskSection>:
            <DiskSection>
                <Info>List of the virtual disks used in the package</Info>
                <Disk ovf:capacity="4294967296" ovf:diskId="lamp" ovf:format="http://www.vmware.com/specifications/vmdk.html#compressed" ovf:populatedSize="1924967692"/>
            </DiskSection> */
        xml::ElementNode *pelmDiskSection = pelmRoot->createChild("DiskSection");
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
        xml::ElementNode *pelmNetworkSection = pelmRoot->createChild("NetworkSection");
        xml::ElementNode *pelmNetworkSectionInfo = pelmNetworkSection->createChild("Info");
        pelmNetworkSectionInfo->addContent("Logical networks used in the package");
        // for now, set up a map so we have a list of unique network names (to make
        // sure the same network name is only added once)
        map<Utf8Str, bool> mapNetworks;
                // we fill this later below when we iterate over the networks

        // and here come the virtual systems:
        xml::ElementNode *pelmVirtualSystemCollection = pelmRoot->createChild("VirtualSystemCollection");
        xml::AttributeNode *pattrVirtualSystemCollectionId = pelmVirtualSystemCollection->setAttribute("ovf:id", "ExportedVirtualBoxMachines");      // whatever

        list< ComObjPtr<VirtualSystemDescription> >::const_iterator it;
        /* Iterate through all virtual systems of that appliance */
        for (it = pAppliance->m->virtualSystemDescriptions.begin();
             it != pAppliance->m->virtualSystemDescriptions.end();
             ++it)
        {
            ComObjPtr<VirtualSystemDescription> vsdescThis = (*it);

            xml::ElementNode *pelmVirtualSystem = pelmVirtualSystemCollection->createChild("VirtualSystem");
            xml::ElementNode *pelmVirtualSystemInfo = pelmVirtualSystem->createChild("Info");      // @todo put in description here after implementing an entry for it

            std::list<VirtualSystemDescriptionEntry*> llName = vsdescThis->findByType(VirtualSystemDescriptionType_Name);
            if (llName.size() != 1)
                throw setError(VBOX_E_NOT_SUPPORTED,
                               tr("Missing VM name"));
            pelmVirtualSystem->setAttribute("ovf:id", llName.front()->strVbox);

            std::list<VirtualSystemDescriptionEntry*> llOS = vsdescThis->findByType(VirtualSystemDescriptionType_OS);
            if (llOS.size() != 1)
                throw setError(VBOX_E_NOT_SUPPORTED,
                               tr("Missing OS type"));
            /*  <OperatingSystemSection ovf:id="82">
                    <Info>Guest Operating System</Info>
                    <Description>Linux 2.6.x</Description>
                </OperatingSystemSection> */
            xml::ElementNode *pelmOperatingSystemSection = pelmVirtualSystem->createChild("OperatingSystemSection");
            pelmOperatingSystemSection->setAttribute("ovf:id", llOS.front()->strOvf);
            pelmOperatingSystemSection->createChild("Info")->addContent("blah");        // @ŧodo
            pelmOperatingSystemSection->createChild("Description")->addContent("blah");        // @ŧodo

            // <VirtualHardwareSection ovf:id="hw1" ovf:transport="iso">
            xml::ElementNode *pelmVirtualHardwareSection = pelmVirtualSystem->createChild("VirtualHardwareSection");

            /*  <System>
                    <vssd:Description>Description of the virtual hardware section.</vssd:Description>
                    <vssd:ElementName>vmware</vssd:ElementName>
                    <vssd:InstanceID>1</vssd:InstanceID>
                    <vssd:VirtualSystemIdentifier>MyLampService</vssd:VirtualSystemIdentifier>
                    <vssd:VirtualSystemType>vmx-4</vssd:VirtualSystemType>
                </System> */
            xml::ElementNode *pelmSystem = pelmVirtualHardwareSection->createChild("System");

            // <vssd:VirtualSystemType>vmx-4</vssd:VirtualSystemType>
            xml::ElementNode *pelmVirtualSystemType = pelmSystem->createChild("VirtualSystemType");
            pelmVirtualSystemType->addContent("virtualbox-2.2");            // instead of vmx-7?

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
            uint32_t cDisks = 0;

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
                                lVirtualQuantity = 1;
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
                                type = OVFResourceType_IdeController; // 5
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
                                type = OVFResourceType_ParallelScsiHba; // 6
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
                                type = OVFResourceType_CdDrive; // 15
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
                                type = OVFResourceType_UsbController; // 23
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

                        if (!strDescription.isEmpty())
                            pItem->createChild("rasd:Description")->addContent(strDescription);
                        if (!strCaption.isEmpty())
                            pItem->createChild("rasd:Caption")->addContent(strCaption);

                        if (!strAllocationUnits.isEmpty())
                            pItem->createChild("rasd:AllocationUnits")->addContent(strAllocationUnits);

                        if (lAutomaticAllocation != -1)
                            pItem->createChild("rasd:AutomaticAllocation")->addContent( (lAutomaticAllocation) ? "true" : "false" );

                        if (!strConnection.isEmpty())
                            pItem->createChild("rasd:Connection")->addContent(strConnection);

                        // <rasd:InstanceID>1</rasd:InstanceID>
                        pItem->createChild("rasd:InstanceID")->addContent(Utf8StrFmt("%d", ulInstanceID));
                        ++ulInstanceID;

                        // <rasd:ResourceType>3</rasd:ResourceType>
                        pItem->createChild("rasd:ResourceType")->addContent(Utf8StrFmt("%d", type));
                        if (!strResourceSubType.isEmpty())
                            pItem->createChild("rasd:ResourceSubType")->addContent(strResourceSubType);

                        // <rasd:VirtualQuantity>1</rasd:VirtualQuantity>
                        if (lVirtualQuantity != -1)
                            pItem->createChild("rasd:VirtualQuantity")->addContent(Utf8StrFmt("%d", lVirtualQuantity));

                        if (lAddress != -1)
                            pItem->createChild("rasd:Address")->addContent(Utf8StrFmt("%d", lAddress));

                        if (lBusNumber != -1)
                            pItem->createChild("rasd:BusNumber")->addContent(Utf8StrFmt("%d", lBusNumber));

                        if (ulParent)
                            pItem->createChild("rasd:Parent")->addContent(Utf8StrFmt("%d", ulParent));
                        if (lAddressOnParent != -1)
                            pItem->createChild("rasd:AddressOnParent")->addContent(Utf8StrFmt("%d", lAddressOnParent));

                        if (!strHostResource.isEmpty())
                            pItem->createChild("rasd:HostResource")->addContent(strHostResource);
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
            Utf8Str strTargetFilePath = stripFilename(pAppliance->m->strPath);
            strTargetFilePath.append("/");
            strTargetFilePath.append(strTargetFileNameOnly);

            // clone the disk:
            ComPtr<IHardDisk> pSourceDisk;
            ComPtr<IHardDisk> pTargetDisk;
            ComPtr<IProgress> pProgress2;

            Log(("Finding source disk \"%ls\"\n", bstrSrcFilePath.raw()));
            rc = pVirtualBox->FindHardDisk(bstrSrcFilePath, pSourceDisk.asOutParam());
            if (FAILED(rc)) throw rc;

            /* We are always exporting to vmdfk stream optimized for now */
            Bstr bstrSrcFormat = L"VMDK";

            // create a new hard disk interface for the destination disk image
            Log(("Creating target disk \"%s\"\n", strTargetFilePath.raw()));
            rc = pVirtualBox->CreateHardDisk(bstrSrcFormat, Bstr(strTargetFilePath), pTargetDisk.asOutParam());
            if (FAILED(rc)) throw rc;

            // the target disk is now registered and needs to be removed again,
            // both after successful cloning or if anything goes bad!
            try
            {
                // clone the source disk image
                rc = pSourceDisk->CloneTo(pTargetDisk, HardDiskVariant_VmdkStreamOptimized, pProgress2.asOutParam());
                if (FAILED(rc)) throw rc;

                // advance to the next operation
                if (!task->progress.isNull())
                    task->progress->advanceOperation(BstrFmt(tr("Exporting virtual disk image '%s'"), strSrcFilePath.c_str()));

                // now loop until the asynchronous operation completes and then
                // report its result
                BOOL fCompleted;
                LONG currentPercent;
                while (SUCCEEDED(pProgress2->COMGETTER(Completed(&fCompleted))))
                {
                    rc = pProgress2->COMGETTER(Percent(&currentPercent));
                    if (FAILED(rc)) throw rc;
                    if (!task->progress.isNull())
                        task->progress->notifyProgress(currentPercent);
                    if (fCompleted)
                        break;
                    // make sure the loop is not too tight
                    rc = pProgress2->WaitForCompletion(100);
                    if (FAILED(rc)) throw rc;
                }

                // report result of asynchronous operation
                HRESULT vrc;
                rc = pProgress2->COMGETTER(ResultCode)(&vrc);
                if (FAILED(rc)) throw rc;

                // if the thread of the progress object has an error, then
                // retrieve the error info from there, or it'll be lost
                if (FAILED(vrc))
                {
                    ProgressErrorInfo info(pProgress2);
                    Utf8Str str(info.getText());
                    const char *pcsz = str.c_str();
                    HRESULT rc2 = setError(vrc, pcsz);
                    throw rc2;
                }
            }
            catch (HRESULT rc3)
            {
                // upon error after registereing, close the disk or
                // it'll stick in the registry forever
                pTargetDisk->Close();
                throw rc3;
            }

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
            pelmDisk->setAttribute("ovf:format", "http://www.vmware.com/specifications/vmdk.html#sparse");
        }

        // now go write the XML
        xml::XmlFileWriter writer(doc);
        writer.write(pAppliance->m->strPath.c_str());
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

    task->rc = rc;

    if (!task->progress.isNull())
        task->progress->notifyComplete(rc);

    LogFlowFunc(("rc=%Rhrc\n", rc));
    LogFlowFuncLeave();

    return VINF_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////
//
// IVirtualSystemDescription constructor / destructor
//
////////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR(VirtualSystemDescription)
struct shutup3 {};

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
STDMETHODIMP VirtualSystemDescription::SetFinalValues(ComSafeArrayIn(BOOL, aEnabled),
                                                      ComSafeArrayIn(IN_BSTR, argVboxValues),
                                                      ComSafeArrayIn(IN_BSTR, argExtraConfigValues))
{
    CheckComArgSafeArrayNotNull(argVboxValues);
    CheckComArgSafeArrayNotNull(argExtraConfigValues);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    com::SafeArray<IN_BSTR> aVboxValues(ComSafeArrayInArg(argVboxValues));
    com::SafeArray<IN_BSTR> aExtraConfigValues(ComSafeArrayInArg(argExtraConfigValues));

    if (    (aVboxValues.size() != m->llDescriptions.size())
         || (aExtraConfigValues.size() != m->llDescriptions.size())
       )
        return E_INVALIDARG;

    list<VirtualSystemDescriptionEntry>::iterator it;
    size_t i = 0;
    for (it = m->llDescriptions.begin();
         it != m->llDescriptions.end();
         ++it, ++i)
    {
        VirtualSystemDescriptionEntry& vsde = *it;

        if (aEnabled[i])
        {
            vsde.strVbox = aVboxValues[i];
            vsde.strExtraConfig = aExtraConfigValues[i];
        }
        else
            vsde.type = VirtualSystemDescriptionType_Ignore;
    }

    return S_OK;
}

/**
 * Internal method; adds a new description item to the member list.
 * @param aType Type of description for the new item.
 * @param strRef Reference item; only used with hard disk controllers.
 * @param aOrigValue Corresponding original value from OVF.
 * @param aAutoValue Initial configuration value (can be overridden by caller with setFinalValues).
 * @param strExtraConfig Extra configuration; meaning dependent on type.
 */
void VirtualSystemDescription::addEntry(VirtualSystemDescriptionType_T aType,
                                        const Utf8Str &strRef,
                                        const Utf8Str &aOrigValue,
                                        const Utf8Str &aAutoValue,
                                        const Utf8Str &strExtraConfig /*= ""*/)
{
    VirtualSystemDescriptionEntry vsde;
    vsde.ulIndex = (uint32_t)m->llDescriptions.size();      // each entry gets an index so the client side can reference them
    vsde.type = aType;
    vsde.strRef = strRef;
    vsde.strOvf = aOrigValue;
    vsde.strVbox = aAutoValue;
    vsde.strExtraConfig = strExtraConfig;

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

STDMETHODIMP Machine::Export(IAppliance *appliance)
{
    HRESULT rc = S_OK;

    if (!appliance)
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
        BOOL fDVDEnabled;
        BOOL fFloppyEnabled;
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

        // floppy
        rc = mFloppyDrive->COMGETTER(Enabled)(&fFloppyEnabled);
        if (FAILED(rc)) throw rc;

        // CD-ROM ?!?
        // ComPtr<IDVDDrive> pDVDDrive;
        fDVDEnabled = 1;

        // this is more tricky so use the COM method
        rc = COMGETTER(USBController)(pUsbController.asOutParam());
        if (FAILED(rc)) throw rc;
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
        rc = GetStorageControllerByName(Bstr("IDE"), pController.asOutParam());
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
        rc = GetStorageControllerByName(Bstr("SATA"), pController.asOutParam());
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

//     <const name="HardDiskControllerSCSI" value="8" />
        rc = GetStorageControllerByName(Bstr("SCSI"), pController.asOutParam());
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

//     <const name="HardDiskImage" value="9" />
        HDData::AttachmentList::iterator itA;
        for (itA = mHDData->mAttachments.begin();
             itA != mHDData->mAttachments.end();
             ++itA)
        {
            ComObjPtr<HardDiskAttachment> pHDA = *itA;

            // the attachment's data
            ComPtr<IHardDisk> pHardDisk;
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

            rc = pHDA->COMGETTER(HardDisk)(pHardDisk.asOutParam());
            if (FAILED(rc)) throw rc;

            rc = pHDA->COMGETTER(Port)(&lChannel);
            if (FAILED(rc)) throw rc;

            rc = pHDA->COMGETTER(Device)(&lDevice);
            if (FAILED(rc)) throw rc;

            Bstr bstrLocation;
            rc = pHardDisk->COMGETTER(Location)(bstrLocation.asOutParam());
            Bstr bstrName;
            rc = pHardDisk->COMGETTER(Name)(bstrName.asOutParam());

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
            RTPathStripExt(strTargetVmdkName.mutableRaw());
            strTargetVmdkName.append(".vmdk");

            pNewDesc->addEntry(VirtualSystemDescriptionType_HardDiskImage,
                               strTargetVmdkName,   // disk ID: let's use the name
                               strTargetVmdkName,   // OVF value:
                               Utf8Str(bstrLocation), // vbox value: media path
                               Utf8StrFmt("controller=%RI32;channel=%RI32", lControllerVsys, lChannelVsys));
        }

        /* Floppy Drive */
        if (fFloppyEnabled)
            pNewDesc->addEntry(VirtualSystemDescriptionType_Floppy, "", "", "");

        /* CD Drive */
        if (fDVDEnabled)
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
        Appliance *pAppliance = static_cast<Appliance*>(appliance);
        AutoCaller autoCaller(pAppliance);
        if (FAILED(rc)) throw rc;

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
