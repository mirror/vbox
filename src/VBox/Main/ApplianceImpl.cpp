/* $Id$ */
/** @file
 *
 * IAppliance and IVirtualSystem COM class implementations
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

#include "Logging.h"

#include "VBox/xml.h"

#include <iostream>
#include <sstream>

using namespace std;

// defines
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

struct Network
{
    Utf8Str strNetworkName;         // value from NetworkSection/Network/@name
            // unfortunately the OVF spec is unspecific about how networks should be specified further
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
                                        // listed in the NetworkSection at the outermost envelope level."
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
typedef map<Utf8Str, Network> NetworksMap;

struct VirtualSystem;

// opaque private instance data of Appliance class
struct Appliance::Data
{
    Bstr bstrPath;

    DiskImagesMap           mapDisks;           // map of DiskImage structs, sorted by DiskImage.strDiskId

    NetworksMap             mapNetworks;        // map of Network structs, sorted by Network.strNetworkName

    list<VirtualSystem>     llVirtualSystems;

    list< ComObjPtr<VirtualSystemDescription> > virtualSystemDescriptions;
};

typedef map<uint32_t, VirtualHardwareItem> HardwareItemsMap;

enum ControllerSystemType { IDE, SATA, SCSI };
struct HardDiskController
{
    uint32_t             idController;           // instance ID (Item/InstanceId); this gets referenced from HardDisk
    ControllerSystemType controllerSystem;       // one of IDE, SATA, SCSI
    Utf8Str              strControllerType;      // controllertype (Item/ResourceSubType); e.g. "LsiLogic"; can be empty (esp. for IDE)
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

// globals
////////////////////////////////////////////////////////////////////////////////

template <class T>
inline
com::Utf8Str toString(const T& val)
{
    // @todo optimize
    std::ostringstream ss;
    ss << val;
    return Utf8Str(ss.str().c_str());
}

// IVirtualBox public methods
////////////////////////////////////////////////////////////////////////////////

/**
 * Implementation for IVirtualBox::openAppliance. Loads the given appliance (see API reference).
 *
 * @param bstrPath Appliance to open (either .ovf or .ova file, see API reference)
 * @param anAppliance IAppliance object created if S_OK is returned.
 * @return S_OK or error.
 */
STDMETHODIMP VirtualBox::OpenAppliance (IN_BSTR bstrPath, IAppliance** anAppliance)
{
    HRESULT rc;

    ComObjPtr<Appliance> appliance;
    appliance.createObject();
    rc = appliance->init(this, bstrPath);
//     ComAssertComRCThrowRC(rc);

    if (SUCCEEDED(rc))
        appliance.queryInterfaceTo(anAppliance);

    return rc;
}

// IAppliance constructor / destructor
////////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR(Appliance)
struct shutup {};

// IAppliance private methods
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
                                    const xml::Node *pReferencesElem,
                                    const xml::Node *pCurElem)
{
    HRESULT rc;

    xml::NodesLoop loopChildren(*pCurElem);
    const xml::Node *pElem;
    while ((pElem = loopChildren.forAllNodes()))
    {
        const char *pcszElemName = pElem->getName();
        const char *pcszTypeAttr = "";
        const xml::Node *pTypeAttr;
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
        else if (    (!strcmp(pcszElemName, "NetworkSection"))
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
 * @param pcszPath Path spec of the XML file, for error messages.
 * @param pReferencesElement "References" element from OVF, for looking up file specifications; can be NULL if no such element is present.
 * @param pSectionElem Section element for which this helper is getting called.
 * @return
 */
HRESULT Appliance::HandleDiskSection(const char *pcszPath,
                                     const xml::Node *pReferencesElem,
                                     const xml::Node *pSectionElem)
{
    // contains "Disk" child elements
    xml::NodesLoop loopDisks(*pSectionElem, "Disk");
    const xml::Node *pelmDisk;
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
                const xml::Node *pFileElem;
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

        // RTPrintf("  found disk: %s\n", d.strDiskId.c_str());
        m->mapDisks[d.strDiskId] = d;
    }

    return S_OK;
}

/**
 * Private helper method that handles network sections in the OVF XML.
 * @param pcszPath Path spec of the XML file, for error messages.
 * @param pSectionElem Section element for which this helper is getting called.
 * @return
 */
HRESULT Appliance::HandleNetworkSection(const char *pcszPath,
                                        const xml::Node *pSectionElem)
{
    // contains "Disk" child elements
    xml::NodesLoop loopNetworks(*pSectionElem, "Network");
    const xml::Node *pelmNetwork;
    while ((pelmNetwork = loopNetworks.forAllNodes()))
    {
        Network n;
        if (!(pelmNetwork->getAttributeValue("name", n.strNetworkName)))
            return setError(VBOX_E_FILE_ERROR,
                            tr("Error reading \"%s\": missing 'name' attribute in 'Network', line %d"),
                            pcszPath,
                            pelmNetwork->getLineNumber());

        m->mapNetworks[n.strNetworkName] = n;
    }

    return S_OK;
}

/**
 * Private helper method that handles a "VirtualSystem" element in the OVF XML.
 *
 * @param pcszPath
 * @param pContentElem
 * @return
 */
HRESULT Appliance::HandleVirtualSystemContent(const char *pcszPath,
                                              const xml::Node *pelmVirtualSystem)
{
    VirtualSystem d;

    const xml::Node *pIdAttr = pelmVirtualSystem->findAttribute("id");
    if (pIdAttr)
        d.strName = pIdAttr->getValue();

    xml::NodesLoop loop(*pelmVirtualSystem);      // all child elements
    const xml::Node *pelmThis;
    while ((pelmThis = loop.forAllNodes()))
    {
        const char *pcszElemName = pelmThis->getName();
        const xml::Node *pTypeAttr = pelmThis->findAttribute("type");
        const char *pcszTypeAttr = (pTypeAttr) ? pTypeAttr->getValue() : "";

        if (!strcmp(pcszElemName, "EulaSection"))
        {
         /* <EulaSection>
                <Info ovf:msgid="6">License agreement for the Virtual System.</Info>
                <License ovf:msgid="1">License terms can go in here.</License>
            </EulaSection> */

            const xml::Node *pelmInfo, *pelmLicense;
            if (    ((pelmInfo = pelmThis->findChildElement("Info")))
                 && ((pelmLicense = pelmThis->findChildElement("License")))
               )
            {
                d.strLicenceInfo = pelmInfo->getValue();
                d.strLicenceText = pelmLicense->getValue();
            }
        }
        else if (    (!strcmp(pcszElemName, "VirtualHardwareSection"))
                  || (!strcmp(pcszTypeAttr, "ovf:VirtualHardwareSection_Type"))
                )
        {
            const xml::Node *pelmSystem, *pelmVirtualSystemType;
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
                    d.strVirtualSystemType = pelmVirtualSystemType->getValue();
            }

            xml::NodesLoop loopVirtualHardwareItems(*pelmThis, "Item");      // all "Item" child elements
            const xml::Node *pelmItem;
            while ((pelmItem = loopVirtualHardwareItems.forAllNodes()))
            {
                VirtualHardwareItem i;

                i.ulLineNumber = pelmItem->getLineNumber();

                xml::NodesLoop loopItemChildren(*pelmItem);      // all child elements
                const xml::Node *pelmItemChild;
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
                        int32_t iType; /** @todo how to fix correctly? (enum fun.) */
                        pelmItemChild->copyValue(iType);
                        i.resourceType = (OVFResourceType_T)iType;
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
                d.mapHardwareItems[i.ulInstanceID] = i;
            }

            HardwareItemsMap::const_iterator itH;

            for (itH = d.mapHardwareItems.begin();
                 itH != d.mapHardwareItems.end();
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
                            d.cCPUs = (uint16_t)i.ullVirtualQuantity;
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
                            d.ullMemorySize = i.ullVirtualQuantity * 1024 * 1024;
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
                        hdc.idController = i.ulInstanceID;
                        hdc.controllerSystem = IDE;
                        hdc.strAddress = i.strAddress;
                        hdc.ulBusNumber = i.ulBusNumber;

                        d.mapControllers[i.ulInstanceID] = hdc;
                    }

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
                        hdc.idController = i.ulInstanceID;
                        hdc.controllerSystem = SCSI;
                        hdc.strControllerType = i.strResourceSubType;

                        d.mapControllers[i.ulInstanceID] = hdc;
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

                        // make sure we have a matching NetworkSection/Network
                        NetworksMap::iterator it = m->mapNetworks.find(i.strConnection);
                        if (it == m->mapNetworks.end())
                            return setError(VBOX_E_FILE_ERROR,
                                            tr("Error reading \"%s\": Invalid connection \"%s\"; cannot find matching NetworkSection/Network element, line %d"),
                                            pcszPath,
                                            i.strConnection.c_str(),
                                            i.ulLineNumber);

                        d.llNetworkNames.push_back(i.strConnection);
                    }
                    break;

                    case OVFResourceType_FloppyDrive: // 14
                        d.fHasFloppyDrive = true;           // we have no additional information
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
                        d.fHasCdromDrive = true;           // we have no additional information
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
                        ControllersMap::const_iterator it = d.mapControllers.find(i.ulParent);
                        if (it == d.mapControllers.end())
                            return setError(VBOX_E_FILE_ERROR,
                                            tr("Error reading \"%s\": Hard disk item with instance ID %d specifies invalid parent %d, line %d"),
                                            pcszPath,
                                            i.ulInstanceID,
                                            i.ulParent,
                                            i.ulLineNumber);
                        const HardDiskController &hdc = it->second;

                        VirtualDisk vd;
                        vd.idController = i.ulParent;
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

                        d.mapVirtualDisks[vd.strDiskId] = vd;
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
                        d.fHasUsbController = true;           // we have no additional information
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
                        d.strSoundCardType = i.strResourceSubType;
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

            d.cimos = (CIMOSType_T)cimos64;
        }
    }

    // now create the virtual system
    m->llVirtualSystems.push_back(d);

    return S_OK;
}

// IAppliance public methods
////////////////////////////////////////////////////////////////////////////////

/**
 * Appliance initializer.
 *
 * This loads the given appliance.
 * @param
 * @return
 */

HRESULT Appliance::init(VirtualBox *aVirtualBox, IN_BSTR &path)
{
    HRESULT rc;

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    /* Weakly reference to a VirtualBox object */
    unconst(mVirtualBox) = aVirtualBox;

    // initialize data
    m = new Data;
    m->bstrPath = path;

    // see if we can handle this file; for now we insist it has an ".ovf" extension
    Utf8Str utf8Path(path);
    const char *pcszLastDot = strrchr(utf8Path, '.');
    if (    (!pcszLastDot)
         || (    strcmp(pcszLastDot, ".ovf")
              && strcmp(pcszLastDot, ".OVF")
            )
       )
        return setError(VBOX_E_FILE_ERROR,
                        tr("Appliance file must have .ovf extension"));

    try
    {
        xml::XmlFileParser parser;
        xml::Document doc;
        parser.read(utf8Path.raw(),
                    doc);

        const xml::Node *pRootElem = doc.getRootElement();
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
        xml::NodesList listFileElements;      // receives all /Envelope/References/File nodes
        const xml::Node *pReferencesElem;
        if ((pReferencesElem = pRootElem->findChildElement("References")))
            pReferencesElem->getChildElements(listFileElements, "File");

        // now go though the sections
        if (!(SUCCEEDED(rc = LoopThruSections(utf8Path.raw(), pReferencesElem, pRootElem))))
            return rc;
    }
    catch(xml::Error &x)
    {
        return setError(VBOX_E_FILE_ERROR,
                        x.what());
    }

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

void Appliance::uninit()
{
    delete m;
    m = NULL;
}

STDMETHODIMP Appliance::COMGETTER(Path)(BSTR *aPath)
{
    if (!aPath)
        return E_POINTER;

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    m->bstrPath.cloneTo(aPath);

    return S_OK;
}

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

STDMETHODIMP Appliance::Interpret()
{
    // @todo:
    //  - Locking
    //  - COM error handling
    //  - don't use COM methods but the methods directly (faster, but needs appropriate locking of that objects itself (s. HardDisk2))
    //  - Appropriate handle errors like not supported file formats
    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    HRESULT rc = S_OK;

    /* Clear any previous virtual system descriptions */
    // @todo: have the entries deleted also?
    m->virtualSystemDescriptions.clear();

    /* We need the default path for storing disk images */
    ComPtr<ISystemProperties> systemProps;
    rc = mVirtualBox->COMGETTER(SystemProperties)(systemProps.asOutParam());
    ComAssertComRCThrowRC(rc);
    BSTR defaultHardDiskLocation;
    rc = systemProps->COMGETTER(DefaultHardDiskFolder)(&defaultHardDiskLocation);
    ComAssertComRCThrowRC(rc);

    list<VirtualSystem>::const_iterator it;
    /* Iterate through all appliances */
    for (it = m->llVirtualSystems.begin();
         it != m->llVirtualSystems.end();
         ++it)
    {
        const VirtualSystem &vs = *it;
        ComObjPtr <VirtualSystemDescription> vsd;
        vsd.createObject();
        rc = vsd->init();
        ComAssertComRCThrowRC(rc);

        Utf8Str osTypeVBox = SchemaDefs_OSTypeId_Other;
        /* Guest OS type */
        switch (vs.cimos)
        {
            case CIMOSType_CIMOS_Unknown: // 0 - Unknown
                osTypeVBox = SchemaDefs_OSTypeId_Other;
                break;

            case CIMOSType_CIMOS_OS2: // 12 - OS/2
                osTypeVBox = SchemaDefs_OSTypeId_OS2;
                break;

            case CIMOSType_CIMOS_MSDOS: // 14 - MSDOS
                osTypeVBox = SchemaDefs_OSTypeId_DOS;
                break;

            case CIMOSType_CIMOS_WIN3x: // 15 - WIN3x
                osTypeVBox = SchemaDefs_OSTypeId_Windows31;
                break;

            case CIMOSType_CIMOS_WIN95: // 16 - WIN95
                osTypeVBox = SchemaDefs_OSTypeId_Windows95;
                break;

            case CIMOSType_CIMOS_WIN98: // 17 - WIN98
                osTypeVBox = SchemaDefs_OSTypeId_Windows98;
                break;

            case CIMOSType_CIMOS_WINNT: // 18 - WINNT
                osTypeVBox = SchemaDefs_OSTypeId_WindowsNT4;
                break;

            case CIMOSType_CIMOS_NetWare: // 21 - NetWare
            case CIMOSType_CIMOS_NovellOES: // 86 - Novell OES
                osTypeVBox = SchemaDefs_OSTypeId_Netware;
                break;

            case CIMOSType_CIMOS_Solaris: // 29 - Solaris
            case CIMOSType_CIMOS_SunOS: // 30 - SunOS
                osTypeVBox = SchemaDefs_OSTypeId_Solaris;
                break;

            case CIMOSType_CIMOS_FreeBSD: // 42 - FreeBSD
                osTypeVBox = SchemaDefs_OSTypeId_FreeBSD;
                break;

            case CIMOSType_CIMOS_NetBSD: // 43 - NetBSD
                osTypeVBox = SchemaDefs_OSTypeId_NetBSD;
                break;

            case CIMOSType_CIMOS_QNX: // 48 - QNX
                osTypeVBox = SchemaDefs_OSTypeId_QNX;
                break;

            case CIMOSType_CIMOS_Windows2000: // 58 - Windows 2000
                osTypeVBox = SchemaDefs_OSTypeId_Windows2000;
                break;

            case CIMOSType_CIMOS_WindowsMe: // 63 - Windows (R) Me
                osTypeVBox = SchemaDefs_OSTypeId_WindowsMe;
                break;

            case CIMOSType_CIMOS_OpenBSD: // 65 - OpenBSD
                osTypeVBox = SchemaDefs_OSTypeId_OpenBSD;
                break;

            case CIMOSType_CIMOS_WindowsXP: // 67 - Windows XP
            case CIMOSType_CIMOS_WindowsXPEmbedded: // 72 - Windows XP Embedded
            case CIMOSType_CIMOS_WindowsEmbeddedforPointofService: // 75 - Windows Embedded for Point of Service
                osTypeVBox = SchemaDefs_OSTypeId_WindowsXP;
                break;

            case CIMOSType_CIMOS_MicrosoftWindowsServer2003: // 69 - Microsoft Windows Server 2003
                osTypeVBox = SchemaDefs_OSTypeId_Windows2003;
                break;

            case CIMOSType_CIMOS_MicrosoftWindowsServer2003_64: // 70 - Microsoft Windows Server 2003 64-Bit
                osTypeVBox = SchemaDefs_OSTypeId_Windows2003_64;
                break;

            case CIMOSType_CIMOS_WindowsXP_64: // 71 - Windows XP 64-Bit
                osTypeVBox = SchemaDefs_OSTypeId_WindowsXP_64;
                break;

            case CIMOSType_CIMOS_WindowsVista: // 73 - Windows Vista
                osTypeVBox = SchemaDefs_OSTypeId_WindowsVista;
                break;

            case CIMOSType_CIMOS_WindowsVista_64: // 74 - Windows Vista 64-Bit
                osTypeVBox = SchemaDefs_OSTypeId_WindowsVista_64;
                break;

            case CIMOSType_CIMOS_MicrosoftWindowsServer2008: // 76 - Microsoft Windows Server 2008
                osTypeVBox = SchemaDefs_OSTypeId_Windows2008;
                break;

            case CIMOSType_CIMOS_MicrosoftWindowsServer2008_64: // 77 - Microsoft Windows Server 2008 64-Bit
                osTypeVBox = SchemaDefs_OSTypeId_Windows2008_64;
                break;

            case CIMOSType_CIMOS_FreeBSD_64: // 78 - FreeBSD 64-Bit
                osTypeVBox = SchemaDefs_OSTypeId_FreeBSD_64;
                break;

            case CIMOSType_CIMOS_RedHatEnterpriseLinux: // 79 - RedHat Enterprise Linux
                osTypeVBox = SchemaDefs_OSTypeId_RedHat;
                break;

            case CIMOSType_CIMOS_RedHatEnterpriseLinux_64: // 80 - RedHat Enterprise Linux 64-Bit
                osTypeVBox = SchemaDefs_OSTypeId_RedHat_64;
                break;

            case CIMOSType_CIMOS_Solaris_64: // 81 - Solaris 64-Bit
                osTypeVBox = SchemaDefs_OSTypeId_Solaris_64;
                break;

            case CIMOSType_CIMOS_SUSE: // 82 - SUSE
            case CIMOSType_CIMOS_SLES: // 84 - SLES
            case CIMOSType_CIMOS_NovellLinuxDesktop: // 87 - Novell Linux Desktop
                osTypeVBox = SchemaDefs_OSTypeId_OpenSUSE;
                break;

            case CIMOSType_CIMOS_SUSE_64: // 83 - SUSE 64-Bit
            case CIMOSType_CIMOS_SLES_64: // 85 - SLES 64-Bit
                osTypeVBox = SchemaDefs_OSTypeId_OpenSUSE_64;
                break;

            case CIMOSType_CIMOS_LINUX: // 36 - LINUX
            case CIMOSType_CIMOS_SunJavaDesktopSystem: // 88 - Sun Java Desktop System
            case CIMOSType_CIMOS_TurboLinux: // 91 - TurboLinux
                osTypeVBox = SchemaDefs_OSTypeId_Linux;
                break;

                //                case CIMOSType_CIMOS_TurboLinux_64: // 92 - TurboLinux 64-Bit
                //                case CIMOSType_CIMOS_Linux_64: // 101 - Linux 64-Bit
                //                    osTypeVBox = VBOXOSTYPE_Linux_x64;
                //                    break;

            case CIMOSType_CIMOS_Mandriva: // 89 - Mandriva
                osTypeVBox = SchemaDefs_OSTypeId_Mandriva;
                break;

            case CIMOSType_CIMOS_Mandriva_64: // 90 - Mandriva 64-Bit
                osTypeVBox = SchemaDefs_OSTypeId_Mandriva_64;
                break;

            case CIMOSType_CIMOS_Ubuntu: // 93 - Ubuntu
                osTypeVBox = SchemaDefs_OSTypeId_Ubuntu;
                break;

            case CIMOSType_CIMOS_Ubuntu_64: // 94 - Ubuntu 64-Bit
                osTypeVBox = SchemaDefs_OSTypeId_Ubuntu_64;
                break;

            case CIMOSType_CIMOS_Debian: // 95 - Debian
                osTypeVBox = SchemaDefs_OSTypeId_Debian;
                break;

            case CIMOSType_CIMOS_Debian_64: // 96 - Debian 64-Bit
                osTypeVBox = SchemaDefs_OSTypeId_Debian_64;
                break;

            case CIMOSType_CIMOS_Linux_2_4_x: // 97 - Linux 2.4.x
                osTypeVBox = SchemaDefs_OSTypeId_Linux24;
                break;

            case CIMOSType_CIMOS_Linux_2_4_x_64: // 98 - Linux 2.4.x 64-Bit
                osTypeVBox = SchemaDefs_OSTypeId_Linux24_64;
                break;

            case CIMOSType_CIMOS_Linux_2_6_x: // 99 - Linux 2.6.x
                osTypeVBox = SchemaDefs_OSTypeId_Linux26;
                break;

            case CIMOSType_CIMOS_Linux_2_6_x_64: // 100 - Linux 2.6.x 64-Bit
                osTypeVBox = SchemaDefs_OSTypeId_Linux26_64;
                break;
            default:
                {
                    /* If we are here we have no clue what OS this should be. Set to
                     * other type as default. */
                    osTypeVBox = SchemaDefs_OSTypeId_Other;
                }
        }
        vsd->addEntry(VirtualSystemDescriptionType_OS, "", toString<ULONG>(vs.cimos), osTypeVBox);

        /* VM name */
        /* If the there isn't any name specified create a default one out of
         * the OS type */
        Utf8Str nameVBox = vs.strName;
        if (nameVBox == "")
            nameVBox = osTypeVBox;
        searchUniqueVMName(nameVBox);
        vsd->addEntry(VirtualSystemDescriptionType_Name, "", vs.strName, nameVBox);

        /* Now that we know the base system get our internal defaults based on that. */
        ComPtr<IGuestOSType> osType;
        rc = mVirtualBox->GetGuestOSType(Bstr(osTypeVBox), osType.asOutParam());
        ComAssertComRCThrowRC(rc);

        /* CPU count */
        /* @todo: check min/max requirements of VBox (SchemaDefs::Min/MaxCPUCount) */
        ULONG cpuCountVBox = vs.cCPUs;
        if (vs.cCPUs == 0)
            cpuCountVBox = 1;
        vsd->addEntry(VirtualSystemDescriptionType_CPU, "", toString<ULONG>(vs.cCPUs), toString<ULONG>(cpuCountVBox));

        /* RAM */
        /* @todo: check min/max requirements of VBox (SchemaDefs::Min/MaxGuestRAM) */
        uint64_t ullMemSizeVBox = vs.ullMemorySize;
        if (vs.ullMemorySize == 0)
        {
            /* If the RAM of the OVF is zero, use our predefined values */
            ULONG memSizeVBox2;
            rc = osType->COMGETTER(RecommendedRAM)(&memSizeVBox2);
            ComAssertComRCThrowRC(rc);
            /* VBox stores that in MByte */
            ullMemSizeVBox = (uint64_t)memSizeVBox2 * _1M;
        }
        vsd->addEntry(VirtualSystemDescriptionType_Memory, "", toString<uint64_t>(vs.ullMemorySize), toString<uint64_t>(ullMemSizeVBox));

        /* Hard disk Controller */
        ControllersMap::const_iterator hdcIt;
        /* Iterate through all hard disk controllers */
        for (hdcIt = vs.mapControllers.begin();
             hdcIt != vs.mapControllers.end();
             ++hdcIt)
        {
            HardDiskController hdc = hdcIt->second;
            switch (hdc.controllerSystem)
            {
                case IDE:
                    {
                        // @todo: figure out the IDE types
                        /* Use PIIX4 as default */
                        IDEControllerType_T hdcController = IDEControllerType_PIIX4;
                        if (!RTStrICmp(hdc.strControllerType.c_str(), "PIIX3"))
                            hdcController = IDEControllerType_PIIX3;
                        else if (!RTStrICmp(hdc.strControllerType.c_str(), "PIIX4"))
                            hdcController = IDEControllerType_PIIX4;
                        vsd->addEntry(VirtualSystemDescriptionType_HardDiskControllerIDE, toString<uint32_t>(hdc.idController), hdc.strControllerType, toString<ULONG>(hdcController));
                        break;
                    }
#ifdef VBOX_WITH_AHCI
                case SATA:
                    {
                        // @todo: figure out the SATA types
                        /* We only support a plain AHCI controller, so use them always */
                        vsd->addEntry(VirtualSystemDescriptionType_HardDiskControllerSATA, toString<uint32_t>(hdc.idController), hdc.strControllerType, "AHCI");
                        break;
                    }
#endif /* VBOX_WITH_AHCI */
#ifdef VBOX_WITH_SCSI
                case SCSI:
                    {
                        // @todo: figure out the SCSI types
# ifdef VBOX_WITH_LSILOGIC
                        Utf8Str hdcController = "LsiLogic";
# elif VBOX_WITH_BUSLOGIC
                        Utf8Str hdcController = "BusLogic";
# else /* !VBOX_WITH_BUSLOGIC */
                        Utf8Str hdcController;
# endif
# ifdef VBOX_WITH_LSILOGIC
                        if (!RTStrICmp(hdc.strControllerType.c_str(), "LsiLogic"))
                            hdcController = "LsiLogic";
# endif /* VBOX_WITH_LSILOGIC */
# ifdef VBOX_WITH_BUSLOGIC
                        if (!RTStrICmp(hdc.strControllerType.c_str(), "BusLogic"))
                            hdcController = "BusLogic";
# endif /* VBOX_WITH_BUSLOGIC */
                        vsd->addEntry(VirtualSystemDescriptionType_HardDiskControllerSCSI, toString<uint32_t>(hdc.idController), hdc.strControllerType, hdcController);
                        break;
                    }
#endif /* VBOX_WITH_SCSI */
                default:
                    {
                    /* @todo: hmm, ok, this needs some explanation to the user,
                     * so set an error! The other possibility is to set IDE
                     * PIIX4 as default & redirect all hard disks to this
                     * controller. */
                    break;
                    }
            }
        }

        /* Hard disks */
        if (vs.mapVirtualDisks.size() > 0)
        {
            // @todo:
            //  - strHref could be empty (construct a new default file name)
            //  - check that the filename is unique to vbox in any case
            VirtualDisksMap::const_iterator hdIt;
            /* Iterate through all hard disks ()*/
            for (hdIt = vs.mapVirtualDisks.begin();
                 hdIt != vs.mapVirtualDisks.end();
                 ++hdIt)
            {
                VirtualDisk hd = hdIt->second;
                /* Get the associated disk image */
                DiskImage di = m->mapDisks [hd.strDiskId];
                /* We have to check if we support this format */
                bool fSupported = false;
                // @todo:
                //  - figure out all possible vmdk formats we also support
                //  - figure out if there is a url specifier for vhd already
                //  - we need a url specifier for the vdi format
                if (!RTStrICmp(di.strFormat.c_str(), "http://www.vmware.com/specifications/vmdk.html#sparse"))
                    fSupported = true;
                /* enable compressed formats for the first tests also */
                else if (!RTStrICmp(di.strFormat.c_str(), "http://www.vmware.com/specifications/vmdk.html#compressed"))
                    fSupported = true;
                if (fSupported)
                {
                    /* Construct the path */
                    Utf8Str path = Utf8StrFmt("%ls%c%s", defaultHardDiskLocation, RTPATH_DELIMITER, di.strHref.c_str());
                    /* Make the path unique to the VBox installation */
                    searchUniqueDiskImageFilePath(path);
                    vsd->addEntry(VirtualSystemDescriptionType_HardDiskImage, hd.strDiskId, di.strHref, path);
                }
            }
        }

        /* Network Controller */
        // @todo: is there no hardware specified in the OVF-Format?
        if (vs.llNetworkNames.size() > 0)
        {
            /* Get the default network adapter type for the selected guest OS */
            NetworkAdapterType_T nwAdapterVBox = NetworkAdapterType_Am79C970A;
            rc = osType->COMGETTER(AdapterType)(&nwAdapterVBox);
            ComAssertComRCThrowRC(rc);
            list<Utf8Str>::const_iterator nwIt;
            /* Iterate through all abstract networks. We support 8 network
             * adapters at the maximum. (@todo: warn if it are more!) */
            size_t a = 0;
            for (nwIt = vs.llNetworkNames.begin();
                 nwIt != vs.llNetworkNames.end() && a < SchemaDefs::NetworkAdapterCount;
                 ++nwIt, ++a)
            {
                // Utf8Str nwController = *nwIt; // @todo: not used yet
                vsd->addEntry(VirtualSystemDescriptionType_NetworkAdapter, "", "", toString<ULONG>(nwAdapterVBox));
            }
        }
        m->virtualSystemDescriptions.push_back(vsd);
    }

    return S_OK;
}

STDMETHODIMP Appliance::ImportAppliance()
{
    // @todo: we need definitely a IProgress object here (disk image copying, ...)
    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    HRESULT rc = S_OK;

    list<VirtualSystem>::const_iterator it;
    list< ComObjPtr<VirtualSystemDescription> >::const_iterator it1;
    /* Iterate through all virtual systems of that appliance */
    size_t i = 0;
    for (it = m->llVirtualSystems.begin(),
         it1 = m->virtualSystemDescriptions.begin();
         it != m->llVirtualSystems.end();
         ++it, ++it1, ++i)
    {
        const VirtualSystem &vs = *it;
        ComObjPtr<VirtualSystemDescription> vsd = (*it1);

        /* Guest OS type */
        std::list<VirtualSystemDescriptionEntry*> vsdeOS = vsd->findByType(VirtualSystemDescriptionType_OS);
        Assert(vsdeOS.size() == 1);
        const Utf8Str &osTypeVBox = vsdeOS.front()->strFinalValue;

        /* Now that we know the base system get our internal defaults based on that. */
        ComPtr<IGuestOSType> osType;
        rc = mVirtualBox->GetGuestOSType(Bstr(osTypeVBox), osType.asOutParam());
        ComAssertComRCThrowRC(rc);

        /* Create the machine */
        /* First get the name */
        std::list<VirtualSystemDescriptionEntry*> vsdeName = vsd->findByType(VirtualSystemDescriptionType_Name);
        Assert(vsdeName.size() == 1);
        const Utf8Str &nameVBox = vsdeName.front()->strFinalValue;
        ComPtr<IMachine> newMachine;
        rc = mVirtualBox->CreateMachine(Bstr(nameVBox.c_str()), Bstr(osTypeVBox.c_str()),
                                        Bstr(), Guid(),
                                        newMachine.asOutParam());
        ComAssertComRCThrowRC(rc);

        /* CPU count (ignored for now) */
        /* @todo: check min/max requirements of VBox (SchemaDefs::Min/MaxCPUCount) */
        // EntriesList vsdeCPU = vsd->findByType (VirtualSystemDescriptionType_CPU);

        /* RAM */
        /* @todo: check min/max requirements of VBox (SchemaDefs::Min/MaxGuestRAM) */
        std::list<VirtualSystemDescriptionEntry*> vsdeRAM = vsd->findByType(VirtualSystemDescriptionType_Memory);
        Assert(vsdeRAM.size() == 1);
        const Utf8Str &memoryVBox = vsdeRAM.front()->strFinalValue;
        uint64_t tt = RTStrToUInt64(memoryVBox.c_str()) / _1M;

        rc = newMachine->COMSETTER(MemorySize)(tt);
        ComAssertComRCThrowRC(rc);

        /* VRAM */
        /* Get the recommended VRAM for this guest OS type */
        /* @todo: check min/max requirements of VBox (SchemaDefs::Min/MaxGuestVRAM) */
        ULONG vramVBox;
        rc = osType->COMGETTER(RecommendedVRAM)(&vramVBox);
        ComAssertComRCThrowRC(rc);
        /* Set the VRAM */
        rc = newMachine->COMSETTER(VRAMSize)(vramVBox);
        ComAssertComRCThrowRC(rc);

        /* Change the network adapters */
        std::list<VirtualSystemDescriptionEntry*> vsdeNW = vsd->findByType(VirtualSystemDescriptionType_NetworkAdapter);
        if (vsdeNW.size() == 0)
        {
            /* No network adapters, so we have to disable our default one */
            ComPtr<INetworkAdapter> nwVBox;
            rc = newMachine->GetNetworkAdapter(0, nwVBox.asOutParam());
            ComAssertComRCThrowRC(rc);
            rc = nwVBox->COMSETTER(Enabled)(false);
            ComAssertComRCThrowRC(rc);
        }
        else
        {
            list<VirtualSystemDescriptionEntry*>::const_iterator nwIt;
            /* Iterate through all network cards. We support 8 network adapters
             * at the maximum. (@todo: warn if it are more!) */
            size_t a = 0;
            for (nwIt = vsdeNW.begin();
                 (nwIt != vsdeNW.end() && a < SchemaDefs::NetworkAdapterCount);
                 ++nwIt, ++a)
            {
                const Utf8Str &nwTypeVBox = (*nwIt)->strFinalValue;
                uint32_t tt1 = RTStrToUInt32(nwTypeVBox.c_str());
                ComPtr<INetworkAdapter> nwVBox;
                rc = newMachine->GetNetworkAdapter((ULONG)a, nwVBox.asOutParam());
                ComAssertComRCThrowRC(rc);
                /* Enable the network card & set the adapter type */
                /* NAT is set as default */
                rc = nwVBox->COMSETTER(Enabled)(true);
                ComAssertComRCThrowRC(rc);
                rc = nwVBox->COMSETTER(AdapterType)(static_cast<NetworkAdapterType_T>(tt1));
                ComAssertComRCThrowRC(rc);
            }
        }

        /* Hard disk controller IDE */
        std::list<VirtualSystemDescriptionEntry*> vsdeHDCIDE = vsd->findByType(VirtualSystemDescriptionType_HardDiskControllerIDE);
        /* @todo: we support one IDE controller only */
        if (vsdeHDCIDE.size() > 0)
        {
             IDEControllerType_T hdcVBox = static_cast<IDEControllerType_T>(RTStrToUInt32(vsdeHDCIDE.front()->strFinalValue.c_str()));
             /* Set the appropriate IDE controller in the virtual BIOS of the
              * VM. */
             ComPtr<IBIOSSettings> biosSettings;
             rc = newMachine->COMGETTER(BIOSSettings)(biosSettings.asOutParam());
             CheckComRCReturnRC(rc);
             rc = biosSettings->COMSETTER(IDEControllerType)(hdcVBox);
             CheckComRCReturnRC(rc);
        }
#ifdef VBOX_WITH_AHCI
        /* Hard disk controller SATA */
        std::list<VirtualSystemDescriptionEntry*> vsdeHDCSATA = vsd->findByType(VirtualSystemDescriptionType_HardDiskControllerSATA);
        /* @todo: we support one SATA controller only */
        if (vsdeHDCSATA.size() > 0)
        {
            const Utf8Str &hdcVBox = vsdeHDCIDE.front()->strFinalValue;
            if (hdcVBox == "AHCI")
            {
                /* For now we have just to enable the AHCI controller. */
                ComPtr<ISATAController> hdcSATAVBox;
                rc = newMachine->COMGETTER(SATAController)(hdcSATAVBox.asOutParam());
                CheckComRCReturnRC(rc);
                rc = hdcSATAVBox->COMSETTER(Enabled)(true);
                CheckComRCReturnRC(rc);
            }
            else
            {
                /* @todo: set an error if this is other than AHCI */
            }
        }
#endif /* VBOX_WITH_AHCI */
#ifdef VBOX_WITH_SCSI
        /* Hard disk controller SCSI */
        EntriesList vsdeHDCSCSI = vsd->findByType(VirtualSystemDescriptionType_HardDiskControllerSCSI);
        /* @todo: do we support more than one SCSI controller? */
        if (vsdeHDCSCSI.size() > 0)
        {
            /* @todo: Currently I have no idea how to enable this. Someone has
             * to write main support for SCSI at all. */
        }
#endif /* VBOX_WITH_SCSI */

        /* Now its time to register the machine before we add any hard disks */
        rc = mVirtualBox->RegisterMachine(newMachine);
        ComAssertComRCThrowRC(rc);

        /* Create the hard disks & connect them to the appropriate controllers. */
        std::list<VirtualSystemDescriptionEntry*> vsdeHD = vsd->findByType(VirtualSystemDescriptionType_HardDiskImage);
        if (vsdeHD.size() > 0)
        {
            /* That we can attach hard disks we need to open a session for the
             * new machine */
            Guid newMachineId;
            rc = newMachine->COMGETTER(Id)(newMachineId.asOutParam());
            CheckComRCReturnRC(rc);
            ComPtr<ISession> session;
            rc = session.createInprocObject(CLSID_Session);
            CheckComRCReturnRC(rc);
            rc = mVirtualBox->OpenSession(session, newMachineId);
            CheckComRCReturnRC(rc);

            int result;
            /* The disk image has to be on the same place as the OVF file. So
             * strip the filename out of the full file path. */
            char *srcDir = RTStrDup(Utf8Str(m->bstrPath).raw());
            RTPathStripFilename(srcDir);
            /* Iterate over all given disk images */
            list<VirtualSystemDescriptionEntry*>::const_iterator hdIt;
            for (hdIt = vsdeHD.begin();
                 hdIt != vsdeHD.end();
                 ++hdIt)
            {
                char *dstFilePath = RTStrDup((*hdIt)->strFinalValue.c_str());
                /* Check if the destination file exists already or the
                 * destination path is empty. */
                if (RTPathExists(dstFilePath) ||
                    !RTStrCmp(dstFilePath, ""))
                {
                    /* @todo: what now? For now we override in no
                     * circumstances. */
//                    continue;
                }
                const Utf8Str &strRef = (*hdIt)->strRef;
                /* Get the associated disk image */
                if (m->mapDisks.find(strRef) == m->mapDisks.end())
                {
                    /* @todo: error: entry doesn't exists */
                }
                DiskImage di = m->mapDisks[strRef];
                /* Construct the source file path */
                char *srcFilePath;
                RTStrAPrintf(&srcFilePath, "%s/%s", srcDir, di.strHref.c_str());
                /* Check if the source file exists */
                if (!RTPathExists(srcFilePath))
                {
                    /* @todo: we have to create a new one */
                }
                else
                {
                    /* Make sure all target directories exists */
                    rc = VirtualBox::ensureFilePathExists(dstFilePath);
                    CheckComRCThrowRC(rc);
                    /* Clone the disk image (this is necessary cause the id has
                     * to be recreated for the case the same hard disk is
                     * attached already from a previous import) */
                    /* First open the existing disk image */
                    ComPtr<IHardDisk2> srcHdVBox;
                    rc = mVirtualBox->OpenHardDisk2(Bstr(srcFilePath), srcHdVBox.asOutParam());
                    CheckComRCReturnRC(rc);
                    /* We need the format description of the source disk image */
                    Bstr srcFormat;
                    rc = srcHdVBox->COMGETTER(Format)(srcFormat.asOutParam());
                    CheckComRCReturnRC(rc);
                    /* Create a new hard disk interface for the destination disk image */
                    ComPtr<IHardDisk2> dstHdVBox;
                    rc = mVirtualBox->CreateHardDisk2(srcFormat, Bstr(dstFilePath), dstHdVBox.asOutParam());
                    CheckComRCReturnRC(rc);
                    /* Clone the source disk image */
                    ComPtr<IProgress> progress;
                    rc = srcHdVBox->CloneTo(dstHdVBox, progress.asOutParam());
                    CheckComRCReturnRC(rc);
                    rc = progress->WaitForCompletion(-1);
                    CheckComRCReturnRC(rc);
                    /* We *must* close the source disk image in order to deregister it */
                    rc = srcHdVBox->Close();
                    CheckComRCReturnRC(rc);
                    /* Now use the new uuid to attach the disk image to our new machine */
                    ComPtr<IMachine> sMachine;
                    rc = session->COMGETTER(Machine)(sMachine.asOutParam());
                    Guid hdId;
                    rc = dstHdVBox->COMGETTER(Id)(hdId.asOutParam());;
                    CheckComRCReturnRC(rc);
                    rc = sMachine->AttachHardDisk2(hdId, StorageBus_IDE, 0, 0); //
                    CheckComRCReturnRC(rc);
                    rc = sMachine->SaveSettings();
                    CheckComRCReturnRC(rc);
                    rc = session->Close();
                    CheckComRCReturnRC(rc);
                }
                RTStrFree(srcFilePath);
                RTStrFree(dstFilePath);
            }
            RTStrFree(srcDir);
        }
        /* @todo: Unregister on failure */
#if 0
        vbox.UnregisterMachine (machineId);
        if (vbox.isOk())
            mMachine.DeleteSettings();
        return false;
#endif
    }

    return S_OK;
}

HRESULT Appliance::searchUniqueVMName(Utf8Str& aName) const
{
    IMachine *machine = NULL;
    char *tmpName = RTStrDup(aName.c_str());
    int i = 1;
    /* @todo: Maybe to cost intensive; try to find a lighter way */
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
    IHardDisk2 *harddisk = NULL;
    char *tmpName = RTStrDup(aName.c_str());
    int i = 1;
    /* Check if the file exists or if a file with this path is registered
     * already */
    /* @todo: Maybe to cost intensive; try to find a lighter way */
    while (RTPathExists(tmpName) ||
           mVirtualBox->FindHardDisk2(Bstr(tmpName), &harddisk) != VBOX_E_OBJECT_NOT_FOUND)
    {
        RTStrFree(tmpName);
        char *tmpDir = RTStrDup(aName.c_str());
        RTPathStripFilename(tmpDir);;
        char *tmpFile = RTStrDup(RTPathFilename(aName.c_str()));
        RTPathStripExt(tmpFile);
        char *tmpExt = RTPathExt(aName.c_str());
        RTStrAPrintf(&tmpName, "%s/%s_%d%s", tmpDir, tmpFile, i, tmpExt);
        RTStrFree(tmpFile);
        RTStrFree(tmpDir);
        ++i;
    }
    aName = tmpName;
    RTStrFree(tmpName);

    return S_OK;
}

// IVirtualSystemDescription constructor / destructor
////////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR(VirtualSystemDescription)
struct shutup3 {};

struct VirtualSystemDescription::Data
{
    list<VirtualSystemDescriptionEntry> descriptions;
};

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

void VirtualSystemDescription::uninit()
{
    delete m;
    m = NULL;
}

STDMETHODIMP VirtualSystemDescription::GetDescription(ComSafeArrayOut(VirtualSystemDescriptionType_T, aTypes),
                                                      ComSafeArrayOut(BSTR, aOrigValues),
                                                      ComSafeArrayOut(BSTR, aAutoValues),
                                                      ComSafeArrayOut(BSTR, aConfigurations))
{
    if (ComSafeArrayOutIsNull(aTypes) ||
        ComSafeArrayOutIsNull(aOrigValues) ||
        ComSafeArrayOutIsNull(aAutoValues) ||
        ComSafeArrayOutIsNull(aConfigurations))
        return E_POINTER;

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    ULONG c = (ULONG)m->descriptions.size();
    com::SafeArray<VirtualSystemDescriptionType_T> sfaTypes(c);
    com::SafeArray<BSTR> sfaOrigValues(c);
    com::SafeArray<BSTR> sfaAutoValues(c);
    com::SafeArray<BSTR> sfaConfigurations(c);

    list<VirtualSystemDescriptionEntry>::const_iterator it;
    size_t i = 0;
    for (it = m->descriptions.begin();
         it != m->descriptions.end();
         ++it, ++i)
    {
        const VirtualSystemDescriptionEntry &vsde = (*it);
        /* Types */
        sfaTypes [i] = vsde.type;
        /* Original value */
        Bstr bstr = vsde.strOriginalValue;
        bstr.cloneTo(&sfaOrigValues[i]);
        /* Auto value */
        bstr = vsde.strAutoValue;
        bstr.cloneTo(&sfaAutoValues[i]);
        /* Configuration */
        bstr = vsde.strConfiguration;
        bstr.cloneTo(&sfaConfigurations[i]);
    }

    sfaTypes.detachTo(ComSafeArrayOutArg(aTypes));
    sfaOrigValues.detachTo(ComSafeArrayOutArg(aOrigValues));
    sfaAutoValues.detachTo(ComSafeArrayOutArg(aAutoValues));
    sfaConfigurations.detachTo(ComSafeArrayOutArg(aConfigurations));

    return S_OK;
}

STDMETHODIMP VirtualSystemDescription::SetFinalValues(ComSafeArrayIn(IN_BSTR, aFinalValues))
{
    CheckComArgSafeArrayNotNull(aFinalValues);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this);

    com::SafeArray <IN_BSTR> values(ComSafeArrayInArg(aFinalValues));
    if (values.size() != m->descriptions.size())
        return E_INVALIDARG;

    list<VirtualSystemDescriptionEntry>::const_iterator it;
    size_t i = 0;
    for (it = m->descriptions.begin();
         it != m->descriptions.end();
         ++it, ++i)
    {
        VirtualSystemDescriptionEntry vsde = (*it);
        vsde.strFinalValue = values[i];
    }

    return S_OK;
}

void VirtualSystemDescription::addEntry(VirtualSystemDescriptionType_T aType,
                                        const Utf8Str &aRef,
                                        const Utf8Str &aOrigValue,
                                        const Utf8Str &aAutoValue,
                                        const Utf8Str &aConfig /* = "" */)
{
    VirtualSystemDescriptionEntry vsde;
    vsde.type = aType;
    vsde.strRef = aRef;
    vsde.strOriginalValue = aOrigValue;
    vsde.strAutoValue = aAutoValue;
    vsde.strConfiguration = aConfig;
    /* For now we add the auto value as final value also */
    vsde.strFinalValue = aAutoValue;

    m->descriptions.push_back(vsde);
}

std::list<VirtualSystemDescriptionEntry*> VirtualSystemDescription::findByType(VirtualSystemDescriptionType_T aType)
{
    std::list<VirtualSystemDescriptionEntry*> vsd;
    list<VirtualSystemDescriptionEntry>::iterator it;
    for (it = m->descriptions.begin();
         it != m->descriptions.end();
         ++it)
        if (it->type == aType)
            vsd.push_back(&(*it));

    return vsd;
}

