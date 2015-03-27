/* $Id$ */
/** @file
 * IAppliance and IVirtualSystem COM class implementations.
 */

/*
 * Copyright (C) 2008-2015 Oracle Corporation
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
#include <iprt/dir.h>
#include <iprt/file.h>
#include <iprt/s3.h>
#include <iprt/sha.h>
#include <iprt/manifest.h>
#include <iprt/tar.h>
#include <iprt/stream.h>

#include <VBox/vd.h>
#include <VBox/com/array.h>

#include "ApplianceImpl.h"
#include "VirtualBoxImpl.h"
#include "GuestOSTypeImpl.h"
#include "ProgressImpl.h"
#include "MachineImpl.h"
#include "MediumImpl.h"
#include "MediumFormatImpl.h"
#include "SystemPropertiesImpl.h"
#include "HostImpl.h"

#include "AutoCaller.h"
#include "Logging.h"

#include "ApplianceImplPrivate.h"

#include <VBox/param.h>
#include <VBox/version.h>
#include <VBox/settings.h>

#include <iprt/x509-branch-collision.h>
#include <set>

using namespace std;

////////////////////////////////////////////////////////////////////////////////
//
// IAppliance public methods
//
////////////////////////////////////////////////////////////////////////////////

/**
 * Public method implementation. This opens the OVF with ovfreader.cpp.
 * Thread implementation is in Appliance::readImpl().
 *
 * @param aFile
 * @return
 */
HRESULT Appliance::read(const com::Utf8Str &aFile,
                        ComPtr<IProgress> &aProgress)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (!i_isApplianceIdle())
        return E_ACCESSDENIED;

    if (m->pReader)
    {
        delete m->pReader;
        m->pReader = NULL;
    }

    // see if we can handle this file; for now we insist it has an ovf/ova extension
    if (!(   aFile.endsWith(".ovf", Utf8Str::CaseInsensitive)
          || aFile.endsWith(".ova", Utf8Str::CaseInsensitive)))
        return setError(VBOX_E_FILE_ERROR,
                        tr("Appliance file must have .ovf extension"));

    ComObjPtr<Progress> progress;
    HRESULT rc = S_OK;
    try
    {
        /* Parse all necessary info out of the URI */
        i_parseURI(aFile, m->locInfo);
        rc = i_readImpl(m->locInfo, progress);
    }
    catch (HRESULT aRC)
    {
        rc = aRC;
    }

    if (SUCCEEDED(rc))
        /* Return progress to the caller */
        progress.queryInterfaceTo(aProgress.asOutParam());

    return S_OK;
}

/**
 * Public method implementation. This looks at the output of ovfreader.cpp and creates
 * VirtualSystemDescription instances.
 * @return
 */
HRESULT Appliance::interpret()
{
    // @todo:
    //  - don't use COM methods but the methods directly (faster, but needs appropriate
    // locking of that objects itself (s. HardDisk))
    //  - Appropriate handle errors like not supported file formats
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (!i_isApplianceIdle())
        return E_ACCESSDENIED;

    HRESULT rc = S_OK;

    /* Clear any previous virtual system descriptions */
    m->virtualSystemDescriptions.clear();

    if (!m->pReader)
        return setError(E_FAIL,
                        tr("Cannot interpret appliance without reading it first (call read() before interpret())"));

    // Change the appliance state so we can safely leave the lock while doing time-consuming
    // disk imports; also the below method calls do all kinds of locking which conflicts with
    // the appliance object lock
    m->state = Data::ApplianceImporting;
    alock.release();

    /* Try/catch so we can clean up on error */
    try
    {
        list<ovf::VirtualSystem>::const_iterator it;
        /* Iterate through all virtual systems */
        for (it = m->pReader->m_llVirtualSystems.begin();
             it != m->pReader->m_llVirtualSystems.end();
             ++it)
        {
            const ovf::VirtualSystem &vsysThis = *it;

            ComObjPtr<VirtualSystemDescription> pNewDesc;
            rc = pNewDesc.createObject();
            if (FAILED(rc)) throw rc;
            rc = pNewDesc->init();
            if (FAILED(rc)) throw rc;

            // if the virtual system in OVF had a <vbox:Machine> element, have the
            // VirtualBox settings code parse that XML now
            if (vsysThis.pelmVBoxMachine)
                pNewDesc->i_importVBoxMachineXML(*vsysThis.pelmVBoxMachine);

            // Guest OS type
            // This is taken from one of three places, in this order:
            Utf8Str strOsTypeVBox;
            Utf8StrFmt strCIMOSType("%RU32", (uint32_t)vsysThis.cimos);
            // 1) If there is a <vbox:Machine>, then use the type from there.
            if (   vsysThis.pelmVBoxMachine
                && pNewDesc->m->pConfig->machineUserData.strOsType.isNotEmpty()
               )
                strOsTypeVBox = pNewDesc->m->pConfig->machineUserData.strOsType;
            // 2) Otherwise, if there is OperatingSystemSection/vbox:OSType, use that one.
            else if (vsysThis.strTypeVBox.isNotEmpty())      // OVFReader has found vbox:OSType
                strOsTypeVBox = vsysThis.strTypeVBox;
            // 3) Otherwise, make a best guess what the vbox type is from the OVF (CIM) OS type.
            else
                convertCIMOSType2VBoxOSType(strOsTypeVBox, vsysThis.cimos, vsysThis.strCimosDesc);
            pNewDesc->i_addEntry(VirtualSystemDescriptionType_OS,
                                 "",
                                 strCIMOSType,
                                 strOsTypeVBox);

            /* VM name */
            Utf8Str nameVBox;
            /* If there is a <vbox:Machine>, we always prefer the setting from there. */
            if (   vsysThis.pelmVBoxMachine
                && pNewDesc->m->pConfig->machineUserData.strName.isNotEmpty())
                nameVBox = pNewDesc->m->pConfig->machineUserData.strName;
            else
                nameVBox = vsysThis.strName;
            /* If there isn't any name specified create a default one out
             * of the OS type */
            if (nameVBox.isEmpty())
                nameVBox = strOsTypeVBox;
            i_searchUniqueVMName(nameVBox);
            pNewDesc->i_addEntry(VirtualSystemDescriptionType_Name,
                                 "",
                                 vsysThis.strName,
                                 nameVBox);

            /* Based on the VM name, create a target machine path. */
            Bstr bstrMachineFilename;
            rc = mVirtualBox->ComposeMachineFilename(Bstr(nameVBox).raw(),
                                                     NULL /* aGroup */,
                                                     NULL /* aCreateFlags */,
                                                     NULL /* aBaseFolder */,
                                                     bstrMachineFilename.asOutParam());
            if (FAILED(rc)) throw rc;
            /* Determine the machine folder from that */
            Utf8Str strMachineFolder = Utf8Str(bstrMachineFilename).stripFilename();

            /* VM Product */
            if (!vsysThis.strProduct.isEmpty())
                pNewDesc->i_addEntry(VirtualSystemDescriptionType_Product,
                                      "",
                                      vsysThis.strProduct,
                                      vsysThis.strProduct);

            /* VM Vendor */
            if (!vsysThis.strVendor.isEmpty())
                pNewDesc->i_addEntry(VirtualSystemDescriptionType_Vendor,
                                     "",
                                     vsysThis.strVendor,
                                     vsysThis.strVendor);

            /* VM Version */
            if (!vsysThis.strVersion.isEmpty())
                pNewDesc->i_addEntry(VirtualSystemDescriptionType_Version,
                                     "",
                                     vsysThis.strVersion,
                                     vsysThis.strVersion);

            /* VM ProductUrl */
            if (!vsysThis.strProductUrl.isEmpty())
                pNewDesc->i_addEntry(VirtualSystemDescriptionType_ProductUrl,
                                     "",
                                     vsysThis.strProductUrl,
                                     vsysThis.strProductUrl);

            /* VM VendorUrl */
            if (!vsysThis.strVendorUrl.isEmpty())
                pNewDesc->i_addEntry(VirtualSystemDescriptionType_VendorUrl,
                                     "",
                                     vsysThis.strVendorUrl,
                                     vsysThis.strVendorUrl);

            /* VM description */
            if (!vsysThis.strDescription.isEmpty())
                pNewDesc->i_addEntry(VirtualSystemDescriptionType_Description,
                                     "",
                                     vsysThis.strDescription,
                                     vsysThis.strDescription);

            /* VM license */
            if (!vsysThis.strLicenseText.isEmpty())
                pNewDesc->i_addEntry(VirtualSystemDescriptionType_License,
                                     "",
                                     vsysThis.strLicenseText,
                                     vsysThis.strLicenseText);

            /* Now that we know the OS type, get our internal defaults based on that. */
            ComPtr<IGuestOSType> pGuestOSType;
            rc = mVirtualBox->GetGuestOSType(Bstr(strOsTypeVBox).raw(), pGuestOSType.asOutParam());
            if (FAILED(rc)) throw rc;

            /* CPU count */
            ULONG cpuCountVBox;
            /* If there is a <vbox:Machine>, we always prefer the setting from there. */
            if (   vsysThis.pelmVBoxMachine
                && pNewDesc->m->pConfig->hardwareMachine.cCPUs)
                cpuCountVBox = pNewDesc->m->pConfig->hardwareMachine.cCPUs;
            else
                cpuCountVBox = vsysThis.cCPUs;
            /* Check for the constraints */
            if (cpuCountVBox > SchemaDefs::MaxCPUCount)
            {
                i_addWarning(tr("The virtual system \"%s\" claims support for %u CPU's, but VirtualBox has support for "
                                "max %u CPU's only."),
                                vsysThis.strName.c_str(), cpuCountVBox, SchemaDefs::MaxCPUCount);
                cpuCountVBox = SchemaDefs::MaxCPUCount;
            }
            if (vsysThis.cCPUs == 0)
                cpuCountVBox = 1;
            pNewDesc->i_addEntry(VirtualSystemDescriptionType_CPU,
                                "",
                                Utf8StrFmt("%RU32", (uint32_t)vsysThis.cCPUs),
                                Utf8StrFmt("%RU32", (uint32_t)cpuCountVBox));

            /* RAM */
            uint64_t ullMemSizeVBox;
            /* If there is a <vbox:Machine>, we always prefer the setting from there. */
            if (   vsysThis.pelmVBoxMachine
                && pNewDesc->m->pConfig->hardwareMachine.ulMemorySizeMB)
                ullMemSizeVBox = pNewDesc->m->pConfig->hardwareMachine.ulMemorySizeMB;
            else
                ullMemSizeVBox = vsysThis.ullMemorySize / _1M;
            /* Check for the constraints */
            if (    ullMemSizeVBox != 0
                 && (    ullMemSizeVBox < MM_RAM_MIN_IN_MB
                      || ullMemSizeVBox > MM_RAM_MAX_IN_MB
                    )
               )
            {
                i_addWarning(tr("The virtual system \"%s\" claims support for %llu MB RAM size, but VirtualBox has "
                                "support for min %u & max %u MB RAM size only."),
                                vsysThis.strName.c_str(), ullMemSizeVBox, MM_RAM_MIN_IN_MB, MM_RAM_MAX_IN_MB);
                ullMemSizeVBox = RT_MIN(RT_MAX(ullMemSizeVBox, MM_RAM_MIN_IN_MB), MM_RAM_MAX_IN_MB);
            }
            if (vsysThis.ullMemorySize == 0)
            {
                /* If the RAM of the OVF is zero, use our predefined values */
                ULONG memSizeVBox2;
                rc = pGuestOSType->COMGETTER(RecommendedRAM)(&memSizeVBox2);
                if (FAILED(rc)) throw rc;
                /* VBox stores that in MByte */
                ullMemSizeVBox = (uint64_t)memSizeVBox2;
            }
            pNewDesc->i_addEntry(VirtualSystemDescriptionType_Memory,
                                 "",
                                 Utf8StrFmt("%RU64", (uint64_t)vsysThis.ullMemorySize),
                                 Utf8StrFmt("%RU64", (uint64_t)ullMemSizeVBox));

            /* Audio */
            Utf8Str strSoundCard;
            Utf8Str strSoundCardOrig;
            /* If there is a <vbox:Machine>, we always prefer the setting from there. */
            if (   vsysThis.pelmVBoxMachine
                && pNewDesc->m->pConfig->hardwareMachine.audioAdapter.fEnabled)
            {
                strSoundCard = Utf8StrFmt("%RU32",
                                          (uint32_t)pNewDesc->m->pConfig->hardwareMachine.audioAdapter.controllerType);
            }
            else if (vsysThis.strSoundCardType.isNotEmpty())
            {
                /* Set the AC97 always for the simple OVF case.
                 * @todo: figure out the hardware which could be possible */
                strSoundCard = Utf8StrFmt("%RU32", (uint32_t)AudioControllerType_AC97);
                strSoundCardOrig = vsysThis.strSoundCardType;
            }
            if (strSoundCard.isNotEmpty())
                pNewDesc->i_addEntry(VirtualSystemDescriptionType_SoundCard,
                                     "",
                                     strSoundCardOrig,
                                     strSoundCard);

#ifdef VBOX_WITH_USB
            /* USB Controller */
            /* If there is a <vbox:Machine>, we always prefer the setting from there. */
            if (   (   vsysThis.pelmVBoxMachine
                    && pNewDesc->m->pConfig->hardwareMachine.usbSettings.llUSBControllers.size() > 0)
                || vsysThis.fHasUsbController)
                pNewDesc->i_addEntry(VirtualSystemDescriptionType_USBController, "", "", "");
#endif /* VBOX_WITH_USB */

            /* Network Controller */
            /* If there is a <vbox:Machine>, we always prefer the setting from there. */
            if (vsysThis.pelmVBoxMachine)
            {
                uint32_t maxNetworkAdapters = Global::getMaxNetworkAdapters(pNewDesc->m->pConfig->hardwareMachine.chipsetType);

                const settings::NetworkAdaptersList &llNetworkAdapters = pNewDesc->m->pConfig->hardwareMachine.llNetworkAdapters;
                /* Check for the constrains */
                if (llNetworkAdapters.size() > maxNetworkAdapters)
                    i_addWarning(tr("The virtual system \"%s\" claims support for %zu network adapters, but VirtualBox "
                                    "has support for max %u network adapter only."),
                                    vsysThis.strName.c_str(), llNetworkAdapters.size(), maxNetworkAdapters);
                /* Iterate through all network adapters. */
                settings::NetworkAdaptersList::const_iterator it1;
                size_t a = 0;
                for (it1 = llNetworkAdapters.begin();
                     it1 != llNetworkAdapters.end() && a < maxNetworkAdapters;
                     ++it1, ++a)
                {
                    if (it1->fEnabled)
                    {
                        Utf8Str strMode = convertNetworkAttachmentTypeToString(it1->mode);
                        pNewDesc->i_addEntry(VirtualSystemDescriptionType_NetworkAdapter,
                                             "", // ref
                                             strMode, // orig
                                             Utf8StrFmt("%RU32", (uint32_t)it1->type), // conf
                                             0,
                                             Utf8StrFmt("slot=%RU32;type=%s", it1->ulSlot, strMode.c_str())); // extra conf
                    }
                }
            }
            /* else we use the ovf configuration. */
            else if (vsysThis.llEthernetAdapters.size() >  0)
            {
                size_t cEthernetAdapters = vsysThis.llEthernetAdapters.size();
                uint32_t maxNetworkAdapters = Global::getMaxNetworkAdapters(ChipsetType_PIIX3);

                /* Check for the constrains */
                if (cEthernetAdapters > maxNetworkAdapters)
                    i_addWarning(tr("The virtual system \"%s\" claims support for %zu network adapters, but VirtualBox "
                                    "has support for max %u network adapter only."),
                                    vsysThis.strName.c_str(), cEthernetAdapters, maxNetworkAdapters);

                /* Get the default network adapter type for the selected guest OS */
                NetworkAdapterType_T defaultAdapterVBox = NetworkAdapterType_Am79C970A;
                rc = pGuestOSType->COMGETTER(AdapterType)(&defaultAdapterVBox);
                if (FAILED(rc)) throw rc;

                ovf::EthernetAdaptersList::const_iterator itEA;
                /* Iterate through all abstract networks. Ignore network cards
                 * which exceed the limit of VirtualBox. */
                size_t a = 0;
                for (itEA = vsysThis.llEthernetAdapters.begin();
                     itEA != vsysThis.llEthernetAdapters.end() && a < maxNetworkAdapters;
                     ++itEA, ++a)
                {
                    const ovf::EthernetAdapter &ea = *itEA; // logical network to connect to
                    Utf8Str strNetwork = ea.strNetworkName;
                    // make sure it's one of these two
                    if (    (strNetwork.compare("Null", Utf8Str::CaseInsensitive))
                         && (strNetwork.compare("NAT", Utf8Str::CaseInsensitive))
                         && (strNetwork.compare("Bridged", Utf8Str::CaseInsensitive))
                         && (strNetwork.compare("Internal", Utf8Str::CaseInsensitive))
                         && (strNetwork.compare("HostOnly", Utf8Str::CaseInsensitive))
                         && (strNetwork.compare("Generic", Utf8Str::CaseInsensitive))
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

                    pNewDesc->i_addEntry(VirtualSystemDescriptionType_NetworkAdapter,
                                         "",      // ref
                                         ea.strNetworkName,      // orig
                                         Utf8StrFmt("%RU32", (uint32_t)nwAdapterVBox),   // conf
                                         0,
                                         Utf8StrFmt("type=%s", strNetwork.c_str()));       // extra conf
                }
            }

            /* If there is a <vbox:Machine>, we always prefer the setting from there. */
            bool fFloppy = false;
            bool fDVD = false;
            if (vsysThis.pelmVBoxMachine)
            {
                settings::StorageControllersList &llControllers = pNewDesc->m->pConfig->storageMachine.llStorageControllers;
                settings::StorageControllersList::iterator it3;
                for (it3 = llControllers.begin();
                     it3 != llControllers.end();
                     ++it3)
                {
                    settings::AttachedDevicesList &llAttachments = it3->llAttachedDevices;
                    settings::AttachedDevicesList::iterator it4;
                    for (it4 = llAttachments.begin();
                         it4 != llAttachments.end();
                         ++it4)
                    {
                        fDVD |= it4->deviceType == DeviceType_DVD;
                        fFloppy |= it4->deviceType == DeviceType_Floppy;
                        if (fFloppy && fDVD)
                            break;
                    }
                    if (fFloppy && fDVD)
                        break;
                }
            }
            else
            {
                fFloppy = vsysThis.fHasFloppyDrive;
                fDVD = vsysThis.fHasCdromDrive;
            }
            /* Floppy Drive */
            if (fFloppy)
                pNewDesc->i_addEntry(VirtualSystemDescriptionType_Floppy, "", "", "");
            /* CD Drive */
            if (fDVD)
                pNewDesc->i_addEntry(VirtualSystemDescriptionType_CDROM, "", "", "");

            /* Hard disk Controller */
            uint16_t cIDEused = 0;
            uint16_t cSATAused = 0; NOREF(cSATAused);
            uint16_t cSCSIused = 0; NOREF(cSCSIused);
            ovf::ControllersMap::const_iterator hdcIt;
            /* Iterate through all hard disk controllers */
            for (hdcIt = vsysThis.mapControllers.begin();
                 hdcIt != vsysThis.mapControllers.end();
                 ++hdcIt)
            {
                const ovf::HardDiskController &hdc = hdcIt->second;
                Utf8Str strControllerID = Utf8StrFmt("%RI32", (uint32_t)hdc.idController);

                switch (hdc.system)
                {
                    case ovf::HardDiskController::IDE:
                        /* Check for the constrains */
                        if (cIDEused < 4)
                        {
                            // @todo: figure out the IDE types
                            /* Use PIIX4 as default */
                            Utf8Str strType = "PIIX4";
                            if (!hdc.strControllerType.compare("PIIX3", Utf8Str::CaseInsensitive))
                                strType = "PIIX3";
                            else if (!hdc.strControllerType.compare("ICH6", Utf8Str::CaseInsensitive))
                                strType = "ICH6";
                            pNewDesc->i_addEntry(VirtualSystemDescriptionType_HardDiskControllerIDE,
                                                 strControllerID,         // strRef
                                                 hdc.strControllerType,   // aOvfValue
                                                 strType);                // aVBoxValue
                        }
                        else
                            /* Warn only once */
                            if (cIDEused == 2)
                                i_addWarning(tr("The virtual \"%s\" system requests support for more than two "
                                              "IDE controller channels, but VirtualBox supports only two."),
                                              vsysThis.strName.c_str());

                        ++cIDEused;
                    break;

                    case ovf::HardDiskController::SATA:
                        /* Check for the constrains */
                        if (cSATAused < 1)
                        {
                            // @todo: figure out the SATA types
                            /* We only support a plain AHCI controller, so use them always */
                            pNewDesc->i_addEntry(VirtualSystemDescriptionType_HardDiskControllerSATA,
                                                 strControllerID,
                                                 hdc.strControllerType,
                                                 "AHCI");
                        }
                        else
                        {
                            /* Warn only once */
                            if (cSATAused == 1)
                                i_addWarning(tr("The virtual system \"%s\" requests support for more than one "
                                                "SATA controller, but VirtualBox has support for only one"),
                                                vsysThis.strName.c_str());

                        }
                        ++cSATAused;
                    break;

                    case ovf::HardDiskController::SCSI:
                        /* Check for the constrains */
                        if (cSCSIused < 1)
                        {
                            VirtualSystemDescriptionType_T vsdet = VirtualSystemDescriptionType_HardDiskControllerSCSI;
                            Utf8Str hdcController = "LsiLogic";
                            if (!hdc.strControllerType.compare("lsilogicsas", Utf8Str::CaseInsensitive))
                            {
                                // OVF considers SAS a variant of SCSI but VirtualBox considers it a class of its own
                                vsdet = VirtualSystemDescriptionType_HardDiskControllerSAS;
                                hdcController = "LsiLogicSas";
                            }
                            else if (!hdc.strControllerType.compare("BusLogic", Utf8Str::CaseInsensitive))
                                hdcController = "BusLogic";
                            pNewDesc->i_addEntry(vsdet,
                                                 strControllerID,
                                                 hdc.strControllerType,
                                                 hdcController);
                        }
                        else
                            i_addWarning(tr("The virtual system \"%s\" requests support for an additional "
                                            "SCSI controller of type \"%s\" with ID %s, but VirtualBox presently "
                                            "supports only one SCSI controller."),
                                            vsysThis.strName.c_str(),
                                            hdc.strControllerType.c_str(),
                                            strControllerID.c_str());
                        ++cSCSIused;
                    break;
                }
            }

            /* Hard disks */
            if (vsysThis.mapVirtualDisks.size() > 0)
            {
                ovf::VirtualDisksMap::const_iterator itVD;
                /* Iterate through all hard disks ()*/
                for (itVD = vsysThis.mapVirtualDisks.begin();
                     itVD != vsysThis.mapVirtualDisks.end();
                     ++itVD)
                {
                    const ovf::VirtualDisk &hd = itVD->second;
                    /* Get the associated disk image */
                    ovf::DiskImage di;
                    std::map<RTCString, ovf::DiskImage>::iterator foundDisk;

                    foundDisk = m->pReader->m_mapDisks.find(hd.strDiskId);
                    if (foundDisk == m->pReader->m_mapDisks.end())
                        continue;
                    else
                    {
                        di = foundDisk->second;
                    }

                    /*
                     * Figure out from URI which format the image of disk has.
                     * URI must have inside section <Disk>                   .
                     * But there aren't strong requirements about correspondence one URI for one disk virtual format.
                     * So possibly, we aren't able to recognize some URIs.
                     */

                    ComObjPtr<MediumFormat> mediumFormat;
                    rc = i_findMediumFormatFromDiskImage(di, mediumFormat);
                    if (FAILED(rc))
                        throw rc;

                    Bstr bstrFormatName;
                    rc = mediumFormat->COMGETTER(Name)(bstrFormatName.asOutParam());
                    if (FAILED(rc))
                        throw rc;
                    Utf8Str vdf = Utf8Str(bstrFormatName);

                    // @todo:
                    //  - figure out all possible vmdk formats we also support
                    //  - figure out if there is a url specifier for vhd already
                    //  - we need a url specifier for the vdi format

                    if (vdf.compare("VMDK", Utf8Str::CaseInsensitive) == 0)
                    {
                        /* If the href is empty use the VM name as filename */
                        Utf8Str strFilename = di.strHref;
                        if (!strFilename.length())
                            strFilename = Utf8StrFmt("%s.vmdk", hd.strDiskId.c_str());

                        Utf8Str strTargetPath = Utf8Str(strMachineFolder);
                        strTargetPath.append(RTPATH_DELIMITER).append(di.strHref);
                        /*
                         * Remove last extension from the file name if the file is compressed
                        */
                        if (di.strCompression.compare("gzip", Utf8Str::CaseInsensitive)==0)
                        {
                            strTargetPath.stripSuffix();
                        }

                        i_searchUniqueDiskImageFilePath(strTargetPath);

                        /* find the description for the hard disk controller
                         * that has the same ID as hd.idController */
                        const VirtualSystemDescriptionEntry *pController;
                        if (!(pController = pNewDesc->i_findControllerFromID(hd.idController)))
                            throw setError(E_FAIL,
                                           tr("Cannot find hard disk controller with OVF instance ID %RI32 "
                                              "to which disk \"%s\" should be attached"),
                                           hd.idController,
                                           di.strHref.c_str());

                        /* controller to attach to, and the bus within that controller */
                        Utf8StrFmt strExtraConfig("controller=%RI16;channel=%RI16",
                                                  pController->ulIndex,
                                                  hd.ulAddressOnParent);
                        pNewDesc->i_addEntry(VirtualSystemDescriptionType_HardDiskImage,
                                             hd.strDiskId,
                                             di.strHref,
                                             strTargetPath,
                                             di.ulSuggestedSizeMB,
                                             strExtraConfig);
                    }
                    else if (vdf.compare("RAW", Utf8Str::CaseInsensitive) == 0)
                    {
                        /* If the href is empty use the VM name as filename */
                        Utf8Str strFilename = di.strHref;
                        if (!strFilename.length())
                            strFilename = Utf8StrFmt("%s.iso", hd.strDiskId.c_str());

                        Utf8Str strTargetPath = Utf8Str(strMachineFolder)
                            .append(RTPATH_DELIMITER)
                            .append(di.strHref);
                        /*
                         * Remove last extension from the file name if the file is compressed
                        */
                        if (di.strCompression.compare("gzip", Utf8Str::CaseInsensitive)==0)
                        {
                            strTargetPath.stripSuffix();
                        }

                        i_searchUniqueDiskImageFilePath(strTargetPath);

                        /* find the description for the hard disk controller
                         * that has the same ID as hd.idController */
                        const VirtualSystemDescriptionEntry *pController;
                        if (!(pController = pNewDesc->i_findControllerFromID(hd.idController)))
                            throw setError(E_FAIL,
                                           tr("Cannot find disk controller with OVF instance ID %RI32 "
                                              "to which disk \"%s\" should be attached"),
                                           hd.idController,
                                           di.strHref.c_str());

                        /* controller to attach to, and the bus within that controller */
                        Utf8StrFmt strExtraConfig("controller=%RI16;channel=%RI16",
                                                  pController->ulIndex,
                                                  hd.ulAddressOnParent);
                        pNewDesc->i_addEntry(VirtualSystemDescriptionType_HardDiskImage,
                                             hd.strDiskId,
                                             di.strHref,
                                             strTargetPath,
                                             di.ulSuggestedSizeMB,
                                             strExtraConfig);
                    }
                    else
                        throw setError(VBOX_E_FILE_ERROR,
                                       tr("Unsupported format for virtual disk image %s in OVF: \"%s\""),
                                          di.strHref.c_str(),
                                          di.strFormat.c_str());
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

    // reset the appliance state
    alock.acquire();
    m->state = Data::ApplianceIdle;

    return rc;
}

/**
 * Public method implementation. This creates one or more new machines according to the
 * VirtualSystemScription instances created by Appliance::Interpret().
 * Thread implementation is in Appliance::i_importImpl().
 * @param aProgress
 * @return
 */
HRESULT Appliance::importMachines(const std::vector<ImportOptions_T> &aOptions,
                                  ComPtr<IProgress> &aProgress)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (aOptions.size())
    {
        m->optListImport.setCapacity(aOptions.size());
        for (size_t i = 0; i < aOptions.size(); ++i)
        {
            m->optListImport.insert(i, aOptions[i]);
        }
    }

    AssertReturn(!(m->optListImport.contains
                   (ImportOptions_KeepAllMACs)
                   && m->optListImport.contains(ImportOptions_KeepNATMACs)
                  ), E_INVALIDARG);

    // do not allow entering this method if the appliance is busy reading or writing
    if (!i_isApplianceIdle())
        return E_ACCESSDENIED;

    if (!m->pReader)
        return setError(E_FAIL,
                        tr("Cannot import machines without reading it first (call read() before i_importMachines())"));

    ComObjPtr<Progress> progress;
    HRESULT rc = S_OK;
    try
    {
        rc = i_importImpl(m->locInfo, progress);
    }
    catch (HRESULT aRC)
    {
        rc = aRC;
    }

    if (SUCCEEDED(rc))
        /* Return progress to the caller */
        progress.queryInterfaceTo(aProgress.asOutParam());

    return rc;
}

////////////////////////////////////////////////////////////////////////////////
//
// Appliance private methods
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Appliance::i_preCheckImageAvailability(PSHASTORAGE pSHAStorage,
                                               RTCString &availableImage)
{
    PFSSRDONLYINTERFACEIO pTarIo = (PFSSRDONLYINTERFACEIO)pSHAStorage->pVDImageIfaces->pvUser;
    const char *pszFilename;
    int vrc = fssRdOnlyGetCurrentName(pTarIo, &pszFilename);
    if (RT_SUCCESS(vrc))
    {
        if (!fssRdOnlyIsCurrentDirectory(pTarIo))
        {
            availableImage = pszFilename;
            return S_OK;
        }

        throw setError(VBOX_E_FILE_ERROR, tr("Empty directory folder (%s) isn't allowed in the OVA package (%Rrc)"),
                       pszFilename, VERR_IS_A_DIRECTORY);
    }

    throw setError(VBOX_E_FILE_ERROR, tr("Could not open the current file in the OVA package (%Rrc)"), vrc);
}

/*******************************************************************************
 * Read stuff
 ******************************************************************************/

/**
 * Implementation for reading an OVF (via task).
 *
 * This starts a new thread which will call
 * Appliance::taskThreadImportOrExport() which will then call readFS() or
 * readS3(). This will then open the OVF with ovfreader.cpp.
 *
 * This is in a separate private method because it is used from three locations:
 *
 * 1) from the public Appliance::Read().
 *
 * 2) in a second worker thread; in that case, Appliance::ImportMachines() called Appliance::i_importImpl(), which
 *    called Appliance::readFSOVA(), which called Appliance::i_importImpl(), which then called this again.
 *
 * 3) from Appliance::readS3(), which got called from a previous instance of Appliance::taskThreadImportOrExport().
 *
 * @param   aLocInfo    The OVF location.
 * @param   aProgress   Where to return the progress object.
 * @return  COM success status code. COM error codes will be thrown.
 */
HRESULT Appliance::i_readImpl(const LocationInfo &aLocInfo, ComObjPtr<Progress> &aProgress)
{
    BstrFmt bstrDesc = BstrFmt(tr("Reading appliance '%s'"),
                               aLocInfo.strPath.c_str());
    HRESULT rc;
    /* Create the progress object */
    aProgress.createObject();
    if (aLocInfo.storageType == VFSType_File)
        /* 1 operation only */
        rc = aProgress->init(mVirtualBox, static_cast<IAppliance*>(this),
                             bstrDesc.raw(),
                             TRUE /* aCancelable */);
    else
        /* 4/5 is downloading, 1/5 is reading */
        rc = aProgress->init(mVirtualBox, static_cast<IAppliance*>(this),
                             bstrDesc.raw(),
                             TRUE /* aCancelable */,
                             2, // ULONG cOperations,
                             5, // ULONG ulTotalOperationsWeight,
                             BstrFmt(tr("Download appliance '%s'"),
                                     aLocInfo.strPath.c_str()).raw(), // CBSTR bstrFirstOperationDescription,
                             4); // ULONG ulFirstOperationWeight,
    if (FAILED(rc)) throw rc;

    /* Initialize our worker task */
    std::auto_ptr<TaskOVF> task(new TaskOVF(this, TaskOVF::Read, aLocInfo, aProgress));

    rc = task->startThread();
    if (FAILED(rc)) throw rc;

    /* Don't destruct on success */
    task.release();

    return rc;
}

/**
 * Actual worker code for reading an OVF from disk. This is called from Appliance::taskThreadImportOrExport()
 * and therefore runs on the OVF read worker thread. This opens the OVF with ovfreader.cpp.
 *
 * This runs in two contexts:
 *
 * 1) in a first worker thread; in that case, Appliance::Read() called Appliance::readImpl();
 *
 * 2) in a second worker thread; in that case, Appliance::Read() called Appliance::readImpl(), which
 *    called Appliance::readS3(), which called Appliance::readImpl(), which then called this.
 *
 * @param pTask
 * @return
 */
HRESULT Appliance::i_readFS(TaskOVF *pTask)
{
    LogFlowFuncEnter();
    LogFlowFunc(("Appliance %p\n", this));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock appLock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;

    if (pTask->locInfo.strPath.endsWith(".ovf", Utf8Str::CaseInsensitive))
        rc = i_readFSOVF(pTask);
    else
        rc = i_readFSOVA(pTask);

    LogFlowFunc(("rc=%Rhrc\n", rc));
    LogFlowFuncLeave();

    return rc;
}

HRESULT Appliance::i_readFSOVF(TaskOVF *pTask)
{
    LogFlowFuncEnter();

    HRESULT rc = S_OK;
    int vrc = VINF_SUCCESS;

    PVDINTERFACEIO pShaIo = 0;
    PVDINTERFACEIO pFileIo = 0;
    do
    {
        try
        {
            /* Create the necessary file access interfaces. */
            pFileIo = FileCreateInterface();
            if (!pFileIo)
            {
                rc = E_OUTOFMEMORY;
                break;
            }

            Utf8Str strMfFile = Utf8Str(pTask->locInfo.strPath).stripSuffix().append(".mf");

            SHASTORAGE storage;
            RT_ZERO(storage);

            if (RTFileExists(strMfFile.c_str()))
            {
                pShaIo = ShaCreateInterface();
                if (!pShaIo)
                {
                    rc = E_OUTOFMEMORY;
                    break;
                }

                //read the manifest file and find a type of used digest
                RTFILE pFile = NULL;
                vrc = RTFileOpen(&pFile, strMfFile.c_str(), RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_NONE);
                if (RT_SUCCESS(vrc) && pFile != NULL)
                {
                    uint64_t cbFile = 0;
                    uint64_t maxFileSize = _1M;
                    size_t cbRead = 0;
                    void  *pBuf; /** @todo r=bird: You leak this buffer! throwing stuff is evil. */

                    vrc = RTFileGetSize(pFile, &cbFile);
                    if (cbFile > maxFileSize)
                        throw setError(VBOX_E_FILE_ERROR,
                                tr("Size of the manifest file '%s' is bigger than 1Mb. Check it, please."),
                                RTPathFilename(strMfFile.c_str()));

                    if (RT_SUCCESS(vrc))
                       pBuf = RTMemAllocZ(cbFile);
                    else
                        throw setError(VBOX_E_FILE_ERROR,
                                tr("Could not get size of the manifest file '%s' "),
                                RTPathFilename(strMfFile.c_str()));

                    vrc = RTFileRead(pFile, pBuf, cbFile, &cbRead);

                    if (RT_FAILURE(vrc))
                    {
                        if (pBuf)
                            RTMemFree(pBuf);
                        throw setError(VBOX_E_FILE_ERROR,
                               tr("Could not read the manifest file '%s' (%Rrc)"),
                               RTPathFilename(strMfFile.c_str()), vrc);
                    }

                    RTFileClose(pFile);

                    RTDIGESTTYPE digestType;
                    vrc = RTManifestVerifyDigestType(pBuf, cbRead, &digestType);

                    if (pBuf)
                        RTMemFree(pBuf);

                    if (RT_FAILURE(vrc))
                    {
                        throw setError(VBOX_E_FILE_ERROR,
                               tr("Could not verify supported digest types in the manifest file '%s' (%Rrc)"),
                               RTPathFilename(strMfFile.c_str()), vrc);
                    }

                    storage.fCreateDigest = true;

                    if (digestType == RTDIGESTTYPE_SHA256)
                    {
                        storage.fSha256 = true;
                    }

                    Utf8Str name = i_applianceIOName(applianceIOFile);

                    vrc = VDInterfaceAdd(&pFileIo->Core, name.c_str(),
                                             VDINTERFACETYPE_IO, 0, sizeof(VDINTERFACEIO),
                                             &storage.pVDImageIfaces);
                    if (RT_FAILURE(vrc))
                        throw setError(VBOX_E_IPRT_ERROR, "Creation of the VD interface failed (%Rrc)", vrc);

                    rc = i_readFSImpl(pTask, pTask->locInfo.strPath, pShaIo, &storage);
                    if (FAILED(rc))
                        break;
                }
                else
                {
                    throw setError(VBOX_E_FILE_ERROR,
                               tr("Could not open the manifest file '%s' (%Rrc)"),
                               RTPathFilename(strMfFile.c_str()), vrc);
                }
            }
            else
            {
                storage.fCreateDigest = false;
                rc = i_readFSImpl(pTask, pTask->locInfo.strPath, pFileIo, &storage);
                if (FAILED(rc))
                    break;
            }
        }
        catch (HRESULT rc2)
        {
            rc = rc2;
        }

    }while (0);

    /* Cleanup */
    if (pShaIo)
        RTMemFree(pShaIo);
    if (pFileIo)
        RTMemFree(pFileIo);

    LogFlowFunc(("rc=%Rhrc\n", rc));
    LogFlowFuncLeave();

    return rc;
}

HRESULT Appliance::i_readFSOVA(TaskOVF *pTask)
{
    LogFlowFuncEnter();

    /*
     * Open the tar file and get a VD I/O interface for it.
     */
    HRESULT hrc;
    PFSSRDONLYINTERFACEIO pTarIo;
    int vrc = fssRdOnlyCreateInterfaceForTarFile(pTask->locInfo.strPath.c_str(), &pTarIo);
    if (RT_SUCCESS(vrc))
    {
        /*
         * Check that the first file is has an .ovf suffix.
         */
        const char *pszName;
        vrc = fssRdOnlyGetCurrentName(pTarIo, &pszName);
        if (RT_SUCCESS(vrc))
        {
            size_t cchName = strlen(pszName);
            if (   cchName >= sizeof(".ovf")
                && RTStrICmp(&pszName[cchName - sizeof(".ovf") + 1], ".ovf") == 0)
            {
                /*
                 * Stack the rest of the expected VD I/O stuff.
                 */
                PVDINTERFACEIO pShaIo = ShaCreateInterface();
                if (pShaIo)
                {
                    Utf8Str     IoName = i_applianceIOName(applianceIOTar);
                    SHASTORAGE  ShaStorage;
                    RT_ZERO(ShaStorage);
                    vrc = VDInterfaceAdd((PVDINTERFACE)pTarIo, IoName.c_str(),
                                         VDINTERFACETYPE_IO, pTarIo, sizeof(VDINTERFACEIO),
                                         &ShaStorage.pVDImageIfaces);
                    if (RT_SUCCESS(vrc))
                        /*
                         * Read and parse the OVF.
                         */
                        hrc = i_readFSImpl(pTask, pszName, pShaIo, &ShaStorage);
                    else
                        hrc = setError(VBOX_E_IPRT_ERROR, "Creation of the VD interface failed (%Rrc)", vrc);
                    RTMemFree(pShaIo);
                }
                else
                    hrc = E_OUTOFMEMORY;
            }
            else
                hrc = setError(VBOX_E_FILE_ERROR,
                               tr("First file in the OVA package must have the extension 'ovf'. But the file '%s' has a different extension."),
                               pszName);
        }
        else
            hrc = setError(VBOX_E_FILE_ERROR, tr("Error reading OVA file '%s' (%Rrc)"), pTask->locInfo.strPath.c_str(), vrc);
        fssRdOnlyDestroyInterface(pTarIo);
    }
    else
        hrc = setError(VBOX_E_FILE_ERROR, tr("Could not open the OVA file '%s' (%Rrc)"), pTask->locInfo.strPath.c_str(), vrc);

    LogFlowFunc(("rc=%Rhrc\n", hrc));
    LogFlowFuncLeave();
    return hrc;
}

HRESULT Appliance::i_readFSImpl(TaskOVF *pTask, const RTCString &strFilename, PVDINTERFACEIO pIfIo, PSHASTORAGE pStorage)
{
    LogFlowFuncEnter();

    HRESULT rc = S_OK;

    pStorage->fCreateDigest = true;

    void *pvTmpBuf = 0;
    try
    {
        /* Read the OVF into a memory buffer */
        size_t cbSize = 0;
        int vrc = readFileIntoBuffer(strFilename.c_str(), &pvTmpBuf, &cbSize, pIfIo, pStorage);
        if (RT_FAILURE(vrc)
            || !pvTmpBuf)
            throw setError(VBOX_E_FILE_ERROR,
                           tr("Could not read OVF file '%s' (%Rrc)"),
                           RTPathFilename(strFilename.c_str()), vrc);

        /* Read & parse the XML structure of the OVF file */
        m->pReader = new ovf::OVFReader(pvTmpBuf, cbSize, pTask->locInfo.strPath);

        if (m->pReader->m_envelopeData.getOVFVersion() == ovf::OVFVersion_2_0)
        {
            m->fSha256 = true;

            uint8_t digest[RTSHA256_HASH_SIZE];
            size_t cchDigest = RTSHA256_DIGEST_LEN;
            char *pszDigest;

            RTSha256(pvTmpBuf, cbSize, &digest[0]);

            vrc = RTStrAllocEx(&pszDigest, cchDigest + 1);
            if (RT_FAILURE(vrc))
                throw setError(E_OUTOFMEMORY, tr("Could not allocate string for SHA256 digest (%Rrc)"), vrc);

            vrc = RTSha256ToString(digest, pszDigest, cchDigest + 1);
            if (RT_SUCCESS(vrc))
                /* Copy the SHA256 sum of the OVF file for later validation */
                m->strOVFSHADigest = pszDigest;
            else
                throw setError(VBOX_E_FILE_ERROR, tr("Converting SHA256 digest to a string was failed (%Rrc)"), vrc);

            RTStrFree(pszDigest);

        }
        else
        {
            m->fSha256 = false;
            /* Copy the SHA1 sum of the OVF file for later validation */
            m->strOVFSHADigest = pStorage->strDigest;
        }

    }
    catch (RTCError &x)      // includes all XML exceptions
    {
        rc = setError(VBOX_E_FILE_ERROR,
                      x.what());
    }
    catch (HRESULT aRC)
    {
        rc = aRC;
    }

    /* Cleanup */
    if (pvTmpBuf)
        RTMemFree(pvTmpBuf);

    LogFlowFunc(("rc=%Rhrc\n", rc));
    LogFlowFuncLeave();

    return rc;
}

#ifdef VBOX_WITH_S3
/**
 * Worker code for reading OVF from the cloud. This is called from Appliance::taskThreadImportOrExport()
 * in S3 mode and therefore runs on the OVF read worker thread. This then starts a second worker
 * thread to create temporary files (see Appliance::readFS()).
 *
 * @param pTask
 * @return
 */
HRESULT Appliance::i_readS3(TaskOVF *pTask)
{
    LogFlowFuncEnter();
    LogFlowFunc(("Appliance %p\n", this));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock appLock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;
    int vrc = VINF_SUCCESS;
    RTS3 hS3 = NIL_RTS3;
    char szOSTmpDir[RTPATH_MAX];
    RTPathTemp(szOSTmpDir, sizeof(szOSTmpDir));
    /* The template for the temporary directory created below */
    char *pszTmpDir = RTPathJoinA(szOSTmpDir, "vbox-ovf-XXXXXX");
    list< pair<Utf8Str, ULONG> > filesList;
    Utf8Str strTmpOvf;

    try
    {
        /* Extract the bucket */
        Utf8Str tmpPath = pTask->locInfo.strPath;
        Utf8Str bucket;
        i_parseBucket(tmpPath, bucket);

        /* We need a temporary directory which we can put the OVF file & all
         * disk images in */
        vrc = RTDirCreateTemp(pszTmpDir, 0700);
        if (RT_FAILURE(vrc))
            throw setError(VBOX_E_FILE_ERROR,
                           tr("Cannot create temporary directory '%s'"), pszTmpDir);

        /* The temporary name of the target OVF file */
        strTmpOvf = Utf8StrFmt("%s/%s", pszTmpDir, RTPathFilename(tmpPath.c_str()));

        /* Next we have to download the OVF */
        vrc = RTS3Create(&hS3,
                         pTask->locInfo.strUsername.c_str(),
                         pTask->locInfo.strPassword.c_str(),
                         pTask->locInfo.strHostname.c_str(),
                         "virtualbox-agent/" VBOX_VERSION_STRING);
        if (RT_FAILURE(vrc))
            throw setError(VBOX_E_IPRT_ERROR,
                           tr("Cannot create S3 service handler"));
        RTS3SetProgressCallback(hS3, pTask->updateProgress, &pTask);

        /* Get it */
        char *pszFilename = RTPathFilename(strTmpOvf.c_str());
        vrc = RTS3GetKey(hS3, bucket.c_str(), pszFilename, strTmpOvf.c_str());
        if (RT_FAILURE(vrc))
        {
            if (vrc == VERR_S3_CANCELED)
                throw S_OK; /* todo: !!!!!!!!!!!!! */
            else if (vrc == VERR_S3_ACCESS_DENIED)
                throw setError(E_ACCESSDENIED,
                               tr("Cannot download file '%s' from S3 storage server (Access denied). Make sure that "
                                  "your credentials are right. "
                                  "Also check that your host clock is properly synced"),
                               pszFilename);
            else if (vrc == VERR_S3_NOT_FOUND)
                throw setError(VBOX_E_FILE_ERROR,
                               tr("Cannot download file '%s' from S3 storage server (File not found)"), pszFilename);
            else
                throw setError(VBOX_E_IPRT_ERROR,
                               tr("Cannot download file '%s' from S3 storage server (%Rrc)"), pszFilename, vrc);
        }

        /* Close the connection early */
        RTS3Destroy(hS3);
        hS3 = NIL_RTS3;

        pTask->pProgress->SetNextOperation(Bstr(tr("Reading")).raw(), 1);

        /* Prepare the temporary reading of the OVF */
        ComObjPtr<Progress> progress;
        LocationInfo li;
        li.strPath = strTmpOvf;
        /* Start the reading from the fs */
        rc = i_readImpl(li, progress);
        if (FAILED(rc)) throw rc;

        /* Unlock the appliance for the reading thread */
        appLock.release();
        /* Wait until the reading is done, but report the progress back to the
           caller */
        ComPtr<IProgress> progressInt(progress);
        i_waitForAsyncProgress(pTask->pProgress, progressInt); /* Any errors will be thrown */

        /* Again lock the appliance for the next steps */
        appLock.acquire();
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
        if (RT_FAILURE(vrc))
            rc = setError(VBOX_E_FILE_ERROR,
                          tr("Cannot delete file '%s' (%Rrc)"), strTmpOvf.c_str(), vrc);
    }
    /* Delete the temporary directory */
    if (RTPathExists(pszTmpDir))
    {
        vrc = RTDirRemove(pszTmpDir);
        if (RT_FAILURE(vrc))
            rc = setError(VBOX_E_FILE_ERROR,
                          tr("Cannot delete temporary directory '%s' (%Rrc)"), pszTmpDir, vrc);
    }
    if (pszTmpDir)
        RTStrFree(pszTmpDir);

    LogFlowFunc(("rc=%Rhrc\n", rc));
    LogFlowFuncLeave();

    return rc;
}
#endif /* VBOX_WITH_S3 */

/*******************************************************************************
 * Import stuff
 ******************************************************************************/

/**
 * Implementation for importing OVF data into VirtualBox. This starts a new thread which will call
 * Appliance::taskThreadImportOrExport().
 *
 * This creates one or more new machines according to the VirtualSystemScription instances created by
 * Appliance::Interpret().
 *
 * This is in a separate private method because it is used from two locations:
 *
 * 1) from the public Appliance::ImportMachines().
 * 2) from Appliance::i_importS3(), which got called from a previous instance of Appliance::taskThreadImportOrExport().
 *
 * @param aLocInfo
 * @param aProgress
 * @return
 */
HRESULT Appliance::i_importImpl(const LocationInfo &locInfo,
                                ComObjPtr<Progress> &progress)
{
    HRESULT rc = S_OK;

    SetUpProgressMode mode;
    if (locInfo.storageType == VFSType_File)
        mode = ImportFile;
    else
        mode = ImportS3;

    rc = i_setUpProgress(progress,
                         BstrFmt(tr("Importing appliance '%s'"), locInfo.strPath.c_str()),
                         mode);
    if (FAILED(rc)) throw rc;

    /* Initialize our worker task */
    std::auto_ptr<TaskOVF> task(new TaskOVF(this, TaskOVF::Import, locInfo, progress));

    rc = task->startThread();
    if (FAILED(rc)) throw rc;

    /* Don't destruct on success */
    task.release();

    return rc;
}

/**
 * Actual worker code for importing OVF data into VirtualBox.
 *
 * This is called from Appliance::taskThreadImportOrExport() and therefore runs
 * on the OVF import worker thread. This creates one or more new machines
 * according to the VirtualSystemScription instances created by
 * Appliance::Interpret().
 *
 * This runs in three contexts:
 *
 * 1) in a first worker thread; in that case, Appliance::ImportMachines() called Appliance::i_importImpl();
 *
 * 2) in a second worker thread; in that case, Appliance::ImportMachines() called Appliance::i_importImpl(), which
 *    called Appliance::i_i_importFSOVA(), which called Appliance::i_importImpl(), which then called this again.
 *
 * 3) in a second worker thread; in that case, Appliance::ImportMachines() called Appliance::i_importImpl(), which
 *    called Appliance::i_importS3(), which called Appliance::i_importImpl(), which then called this again.
 *
 * @param   pTask       The OVF task data.
 * @return  COM status code.
 */
HRESULT Appliance::i_importFS(TaskOVF *pTask)
{

    LogFlowFuncEnter();
    LogFlowFunc(("Appliance %p\n", this));

    /* Change the appliance state so we can safely leave the lock while doing
     * time-consuming disk imports; also the below method calls do all kinds of
     * locking which conflicts with the appliance object lock. */
    AutoWriteLock writeLock(this COMMA_LOCKVAL_SRC_POS);
    /* Check if the appliance is currently busy. */
    if (!i_isApplianceIdle())
        return E_ACCESSDENIED;
    /* Set the internal state to importing. */
    m->state = Data::ApplianceImporting;

    HRESULT rc = S_OK;

    /* Clear the list of imported machines, if any */
    m->llGuidsMachinesCreated.clear();

    if (pTask->locInfo.strPath.endsWith(".ovf", Utf8Str::CaseInsensitive))
        rc = i_importFSOVF(pTask, writeLock);
    else
        rc = i_importFSOVA(pTask, writeLock);

    if (FAILED(rc))
    {
        /* With _whatever_ error we've had, do a complete roll-back of
         * machines and disks we've created */
        writeLock.release();
        ErrorInfoKeeper eik;
        for (list<Guid>::iterator itID = m->llGuidsMachinesCreated.begin();
             itID != m->llGuidsMachinesCreated.end();
             ++itID)
        {
            Guid guid = *itID;
            Bstr bstrGuid = guid.toUtf16();
            ComPtr<IMachine> failedMachine;
            HRESULT rc2 = mVirtualBox->FindMachine(bstrGuid.raw(), failedMachine.asOutParam());
            if (SUCCEEDED(rc2))
            {
                SafeIfaceArray<IMedium> aMedia;
                rc2 = failedMachine->Unregister(CleanupMode_DetachAllReturnHardDisksOnly, ComSafeArrayAsOutParam(aMedia));
                ComPtr<IProgress> pProgress2;
                rc2 = failedMachine->DeleteConfig(ComSafeArrayAsInParam(aMedia), pProgress2.asOutParam());
                pProgress2->WaitForCompletion(-1);
            }
        }
        writeLock.acquire();
    }

    /* Reset the state so others can call methods again */
    m->state = Data::ApplianceIdle;

    LogFlowFunc(("rc=%Rhrc\n", rc));
    LogFlowFuncLeave();

    return rc;
}

HRESULT Appliance::i_importFSOVF(TaskOVF *pTask, AutoWriteLockBase& writeLock)
{
    LogFlowFuncEnter();

    HRESULT rc = S_OK;

    PVDINTERFACEIO pShaIo = NULL;
    PVDINTERFACEIO pFileIo = NULL;
    void *pvMfBuf = NULL;
    void *pvCertBuf = NULL;
    writeLock.release();

    /* Create the import stack for the rollback on errors. */
    ImportStack stack(pTask->locInfo, m->pReader->m_mapDisks, pTask->pProgress);

    try
    {
        /* Create the necessary file access interfaces. */
        pFileIo = FileCreateInterface();
        if (!pFileIo)
            throw setError(E_OUTOFMEMORY);

        Utf8Str strMfFile = Utf8Str(pTask->locInfo.strPath).stripSuffix().append(".mf");

        SHASTORAGE storage;
        RT_ZERO(storage);

        Utf8Str name = i_applianceIOName(applianceIOFile);

        int vrc = VDInterfaceAdd(&pFileIo->Core, name.c_str(),
                                 VDINTERFACETYPE_IO, 0, sizeof(VDINTERFACEIO),
                                 &storage.pVDImageIfaces);
        if (RT_FAILURE(vrc))
            throw setError(VBOX_E_IPRT_ERROR, "Creation of the VD interface failed (%Rrc)", vrc);

        if (RTFileExists(strMfFile.c_str()))
        {
            pShaIo = ShaCreateInterface();
            if (!pShaIo)
                throw setError(E_OUTOFMEMORY);

            Utf8Str nameSha = i_applianceIOName(applianceIOSha);
            /* Fill out interface descriptor. */
            pShaIo->Core.u32Magic         = VDINTERFACE_MAGIC;
            pShaIo->Core.cbSize           = sizeof(VDINTERFACEIO);
            pShaIo->Core.pszInterfaceName = nameSha.c_str();
            pShaIo->Core.enmInterface     = VDINTERFACETYPE_IO;
            pShaIo->Core.pvUser           = &storage;
            pShaIo->Core.pNext            = NULL;

            storage.fCreateDigest = true;

            size_t cbMfFile = 0;

            /* Now import the appliance. */
            i_importMachines(stack, pShaIo, &storage);
            /* Read & verify the manifest file. */
            /* Add the ovf file to the digest list. */
            stack.llSrcDisksDigest.push_front(STRPAIR(pTask->locInfo.strPath, m->strOVFSHADigest));
            rc = i_readFileToBuf(strMfFile, &pvMfBuf, &cbMfFile, true, pShaIo, &storage);
            if (FAILED(rc)) throw rc;
            rc = i_verifyManifestFile(strMfFile, stack, pvMfBuf, cbMfFile);
            if (FAILED(rc)) throw rc;

            size_t cbCertFile = 0;

            /* Save the SHA digest of the manifest file for the next validation */
            Utf8Str manifestShaDigest = storage.strDigest;

            Utf8Str strCertFile = Utf8Str(pTask->locInfo.strPath).stripSuffix().append(".cert");
            if (RTFileExists(strCertFile.c_str()))
            {
                rc = i_readFileToBuf(strCertFile, &pvCertBuf, &cbCertFile, false, pShaIo, &storage);
                if (FAILED(rc)) throw rc;

                /* verify Certificate */
                rc = i_verifyCertificateFile(pvCertBuf, cbCertFile, &storage);
                if (FAILED(rc)) throw rc;
            }
        }
        else
        {
            storage.fCreateDigest = false;
            i_importMachines(stack, pFileIo, &storage);
        }
    }
    catch (HRESULT rc2)
    {
        rc = rc2;
        /*
         * Restoring original UUID from OVF description file.
         * During import VBox creates new UUIDs for imported images and
         * assigns them to the images. In case of failure we have to restore
         * the original UUIDs because those new UUIDs are obsolete now and
         * won't be used anymore.
         */
        {
            ErrorInfoKeeper eik; /* paranoia */
            list< ComObjPtr<VirtualSystemDescription> >::const_iterator itvsd;
            /* Iterate through all virtual systems of that appliance */
            for (itvsd = m->virtualSystemDescriptions.begin();
                 itvsd != m->virtualSystemDescriptions.end();
                 ++itvsd)
            {
                ComObjPtr<VirtualSystemDescription> vsdescThis = (*itvsd);
                settings::MachineConfigFile *pConfig = vsdescThis->m->pConfig;
                if(vsdescThis->m->pConfig!=NULL)
                    stack.restoreOriginalUUIDOfAttachedDevice(pConfig);
            }
        }
    }
    writeLock.acquire();

    /* Cleanup */
    if (pvMfBuf)
        RTMemFree(pvMfBuf);
    if (pvCertBuf)
        RTMemFree(pvCertBuf);
    if (pShaIo)
        RTMemFree(pShaIo);
    if (pFileIo)
        RTMemFree(pFileIo);

    LogFlowFunc(("rc=%Rhrc\n", rc));
    LogFlowFuncLeave();

    return rc;
}

HRESULT Appliance::i_importFSOVA(TaskOVF *pTask, AutoWriteLockBase& writeLock)
{
    LogFlowFuncEnter();
    HRESULT rc = S_OK;

    /*
     * Open the OVA (TAR) file.
     */
    PFSSRDONLYINTERFACEIO pTarIo;
    int vrc = fssRdOnlyCreateInterfaceForTarFile(pTask->locInfo.strPath.c_str(), &pTarIo);
    if (RT_FAILURE(vrc))
        return setError(VBOX_E_FILE_ERROR,
                        tr("Could not open OVA file '%s' (%Rrc)"),
                        pTask->locInfo.strPath.c_str(), vrc);


    PVDINTERFACEIO pShaIo = 0;
    void *pvMfBuf = NULL;
    void *pvCertBuf = NULL;
    Utf8Str OVFfilename;

    writeLock.release();

    /* Create the import stack for the rollback on errors. */
    ImportStack stack(pTask->locInfo, m->pReader->m_mapDisks, pTask->pProgress);

    try
    {
        /* Create the necessary file access interfaces. */
        pShaIo = ShaCreateInterface();
        if (!pShaIo)
            throw setError(E_OUTOFMEMORY);

        Utf8Str nameTar = i_applianceIOName(applianceIOTar);
        SHASTORAGE storage;
        RT_ZERO(storage);
        vrc = VDInterfaceAdd((PVDINTERFACE)pTarIo, nameTar.c_str(),
                             VDINTERFACETYPE_IO, pTarIo, sizeof(VDINTERFACEIO),
                             &storage.pVDImageIfaces);
        if (RT_FAILURE(vrc))
            throw setError(VBOX_E_IPRT_ERROR,
                           tr("Creation of the VD interface failed (%Rrc)"), vrc);

        /* Fill out interface descriptor. */
        Utf8Str nameSha = i_applianceIOName(applianceIOSha);
        pShaIo->Core.u32Magic         = VDINTERFACE_MAGIC;
        pShaIo->Core.cbSize           = sizeof(VDINTERFACEIO);
        pShaIo->Core.pszInterfaceName = nameSha.c_str();
        pShaIo->Core.enmInterface     = VDINTERFACETYPE_IO;
        pShaIo->Core.pvUser           = &storage;
        pShaIo->Core.pNext            = NULL;

        /*
         * File #1 - the .ova file.
         *
         * Read the name of the first file. This is how all internal files
         * are named.
         */
        const char *pszFilename;
        vrc = fssRdOnlyGetCurrentName(pTarIo, &pszFilename);
        if (RT_FAILURE(vrc))
            throw setError(VBOX_E_IPRT_ERROR,
                           tr("Getting the OVF file within the archive failed (%Rrc)"), vrc);
        if (vrc == VINF_TAR_DIR_PATH)
            throw setError(VBOX_E_FILE_ERROR,
                           tr("Empty directory folder (%s) isn't allowed in the OVA package (%Rrc)"),
                           pszFilename, vrc);

        /* save original OVF filename */
        OVFfilename = pszFilename;
        Utf8Str strMfFile = (Utf8Str(pszFilename)).stripSuffix().append(".mf");
        Utf8Str strCertFile = (Utf8Str(pszFilename)).stripSuffix().append(".cert");

        /* Skip the OVF file, cause this was read in IAppliance::Read already. */
        vrc = fssRdOnlySkipCurrent(pTarIo);
        if (RT_SUCCESS(vrc))
            vrc = fssRdOnlyGetCurrentName(pTarIo, &pszFilename);
        if (   RT_FAILURE(vrc)
            && vrc != VERR_EOF)
            throw setError(VBOX_E_IPRT_ERROR, tr("Seeking within the archive failed (%Rrc)"), vrc);

        PVDINTERFACEIO pCallbacks = pShaIo;
        PSHASTORAGE pStorage = &storage;

        /* We always need to create the digest, cause we don't know if there
         * is a manifest file in the stream. */
        pStorage->fCreateDigest = true;

        /*
         * File #2 - the manifest file (.mf), optional.
         *
         * Note: This isn't fatal if the file is not found. The standard
         * defines 3 cases:
         *  1. no manifest file
         *  2. manifest file after the OVF file
         *  3. manifest file after all disk files
         *
         * If we want streaming capabilities, we can't check if it is there by
         * searching for it. We have to try to open it on all possible places.
         * If it fails here, we will try it again after all disks where read.
         */
        size_t cbMfFile = 0;
        rc = i_readTarFileToBuf(pTarIo, strMfFile, &pvMfBuf, &cbMfFile, true, pCallbacks, pStorage);
        if (FAILED(rc))
            throw rc;

        /*
         * File #3 - certificate file (.cer), optional.
         *
         * Logic is the same as with manifest file.  This only makes sense if
         * there is a manifest file.
         */
        size_t cbCertFile = 0;
        vrc = fssRdOnlyGetCurrentName(pTarIo, &pszFilename);
        if (RT_SUCCESS(vrc))
        {
            if (pvMfBuf)
            {
                if (strCertFile.compare(pszFilename) == 0)
                {
                    rc = i_readTarFileToBuf(pTarIo, strCertFile, &pvCertBuf, &cbCertFile, false, pCallbacks, pStorage);
                    if (FAILED(rc)) throw rc;

                    if (pvCertBuf)
                    {
                        /* verify the certificate */
                        rc = i_verifyCertificateFile(pvCertBuf, cbCertFile, pStorage);
                        if (FAILED(rc)) throw rc;
                    }
                }
            }
        }

        /*
         * Now import the appliance.
         */
        i_importMachines(stack, pCallbacks, pStorage);

        /*
         * The certificate and mainifest files may alternatively be stored
         * after the disk files, so look again if we didn't find them already.
         */
        if (!pvMfBuf)
        {
            /*
             * File #N-1 - The manifest file, optional.
             */
            rc = i_readTarFileToBuf(pTarIo, strMfFile, &pvMfBuf, &cbMfFile, true, pCallbacks, pStorage);
            if (FAILED(rc)) throw rc;

            /* If we were able to read a manifest file we can check it now. */
            if (pvMfBuf)
            {
                /* Add the ovf file to the digest list. */
                stack.llSrcDisksDigest.push_front(STRPAIR(OVFfilename, m->strOVFSHADigest));
                rc = i_verifyManifestFile(strMfFile, stack, pvMfBuf, cbMfFile);
                if (FAILED(rc)) throw rc;

                /*
                 * File #N - The certificate file, optional.
                 * (Requires mainfest, as mention before.)
                 */
                vrc = fssRdOnlyGetCurrentName(pTarIo, &pszFilename);
                if (RT_SUCCESS(vrc))
                {
                    if (strCertFile.compare(pszFilename) == 0)
                    {
                        rc = i_readTarFileToBuf(pTarIo, strCertFile, &pvCertBuf, &cbCertFile, false, pCallbacks, pStorage);
                        if (FAILED(rc)) throw rc;

                        if (pvCertBuf)
                        {
                            /* verify the certificate */
                            rc = i_verifyCertificateFile(pvCertBuf, cbCertFile, pStorage);
                            if (FAILED(rc)) throw rc;
                        }
                    }
                }
            }
        }
        /** @todo else: Verify the manifest! */
    }
    catch (HRESULT rc2)
    {
        rc = rc2;

        /*
         * Restoring original UUID from OVF description file.
         * During import VBox creates new UUIDs for imported images and
         * assigns them to the images. In case of failure we have to restore
         * the original UUIDs because those new UUIDs are obsolete now and
         * won't be used anymore.
         */
        ErrorInfoKeeper eik; /* paranoia */
        list< ComObjPtr<VirtualSystemDescription> >::const_iterator itvsd;
        /* Iterate through all virtual systems of that appliance */
        for (itvsd = m->virtualSystemDescriptions.begin();
             itvsd != m->virtualSystemDescriptions.end();
             ++itvsd)
        {
            ComObjPtr<VirtualSystemDescription> vsdescThis = (*itvsd);
            settings::MachineConfigFile *pConfig = vsdescThis->m->pConfig;
            if(vsdescThis->m->pConfig!=NULL)
              stack.restoreOriginalUUIDOfAttachedDevice(pConfig);
        }
    }
    writeLock.acquire();

    /* Cleanup */
    fssRdOnlyDestroyInterface(pTarIo);
    if (pvMfBuf)
        RTMemFree(pvMfBuf);
    if (pShaIo)
        RTMemFree(pShaIo);
    if (pvCertBuf)
        RTMemFree(pvCertBuf);

    LogFlowFunc(("rc=%Rhrc\n", rc));
    LogFlowFuncLeave();

    return rc;
}

#ifdef VBOX_WITH_S3
/**
 * Worker code for importing OVF from the cloud. This is called from Appliance::taskThreadImportOrExport()
 * in S3 mode and therefore runs on the OVF import worker thread. This then starts a second worker
 * thread to import from temporary files (see Appliance::i_importFS()).
 * @param pTask
 * @return
 */
HRESULT Appliance::i_importS3(TaskOVF *pTask)
{
    LogFlowFuncEnter();
    LogFlowFunc(("Appliance %p\n", this));

    AutoWriteLock appLock(this COMMA_LOCKVAL_SRC_POS);

    int vrc = VINF_SUCCESS;
    RTS3 hS3 = NIL_RTS3;
    char szOSTmpDir[RTPATH_MAX];
    RTPathTemp(szOSTmpDir, sizeof(szOSTmpDir));
    /* The template for the temporary directory created below */
    char *pszTmpDir = RTPathJoinA(szOSTmpDir, "vbox-ovf-XXXXXX");
    list< pair<Utf8Str, ULONG> > filesList;

    HRESULT rc = S_OK;
    try
    {
        /* Extract the bucket */
        Utf8Str tmpPath = pTask->locInfo.strPath;
        Utf8Str bucket;
        i_parseBucket(tmpPath, bucket);

        /* We need a temporary directory which we can put the all disk images
         * in */
        vrc = RTDirCreateTemp(pszTmpDir, 0700);
        if (RT_FAILURE(vrc))
            throw setError(VBOX_E_FILE_ERROR,
                           tr("Cannot create temporary directory '%s' (%Rrc)"), pszTmpDir, vrc);

        /* Add every disks of every virtual system to an internal list */
        list< ComObjPtr<VirtualSystemDescription> >::const_iterator it;
        for (it = m->virtualSystemDescriptions.begin();
             it != m->virtualSystemDescriptions.end();
             ++it)
        {
            ComObjPtr<VirtualSystemDescription> vsdescThis = (*it);
            std::list<VirtualSystemDescriptionEntry*> avsdeHDs =
                vsdescThis->i_findByType(VirtualSystemDescriptionType_HardDiskImage);
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
        vrc = RTS3Create(&hS3,
                         pTask->locInfo.strUsername.c_str(),
                         pTask->locInfo.strPassword.c_str(),
                         pTask->locInfo.strHostname.c_str(),
                         "virtualbox-agent/" VBOX_VERSION_STRING);
        if (RT_FAILURE(vrc))
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
            if (!pTask->pProgress.isNull())
                pTask->pProgress->SetNextOperation(BstrFmt(tr("Downloading file '%s'"), pszFilename).raw(), s.second);

            vrc = RTS3GetKey(hS3, bucket.c_str(), pszFilename, strSrcFile.c_str());
            if (RT_FAILURE(vrc))
            {
                if (vrc == VERR_S3_CANCELED)
                    throw S_OK; /* todo: !!!!!!!!!!!!! */
                else if (vrc == VERR_S3_ACCESS_DENIED)
                    throw setError(E_ACCESSDENIED,
                                   tr("Cannot download file '%s' from S3 storage server (Access denied). "
                                      "Make sure that your credentials are right. Also check that your host clock is "
                                      "properly synced"),
                                   pszFilename);
                else if (vrc == VERR_S3_NOT_FOUND)
                    throw setError(VBOX_E_FILE_ERROR,
                                   tr("Cannot download file '%s' from S3 storage server (File not found)"),
                                   pszFilename);
                else
                    throw setError(VBOX_E_IPRT_ERROR,
                                   tr("Cannot download file '%s' from S3 storage server (%Rrc)"),
                                   pszFilename, vrc);
            }
        }

        /* Provide a OVF file (haven't to exist) so the import routine can
         * figure out where the disk images/manifest file are located. */
        Utf8StrFmt strTmpOvf("%s/%s", pszTmpDir, RTPathFilename(tmpPath.c_str()));
        /* Now check if there is an manifest file. This is optional. */
        Utf8Str strManifestFile; //= queryManifestFileName(strTmpOvf);
//        Utf8Str strManifestFile = queryManifestFileName(strTmpOvf);
        char *pszFilename = RTPathFilename(strManifestFile.c_str());
        if (!pTask->pProgress.isNull())
            pTask->pProgress->SetNextOperation(BstrFmt(tr("Downloading file '%s'"), pszFilename).raw(), 1);

        /* Try to download it. If the error is VERR_S3_NOT_FOUND, it isn't fatal. */
        vrc = RTS3GetKey(hS3, bucket.c_str(), pszFilename, strManifestFile.c_str());
        if (RT_SUCCESS(vrc))
            filesList.push_back(pair<Utf8Str, ULONG>(strManifestFile, 0));
        else if (RT_FAILURE(vrc))
        {
            if (vrc == VERR_S3_CANCELED)
                throw S_OK; /* todo: !!!!!!!!!!!!! */
            else if (vrc == VERR_S3_NOT_FOUND)
                vrc = VINF_SUCCESS; /* Not found is ok */
            else if (vrc == VERR_S3_ACCESS_DENIED)
                throw setError(E_ACCESSDENIED,
                               tr("Cannot download file '%s' from S3 storage server (Access denied)."
                                  "Make sure that your credentials are right. "
                                  "Also check that your host clock is properly synced"),
                               pszFilename);
            else
                throw setError(VBOX_E_IPRT_ERROR,
                               tr("Cannot download file '%s' from S3 storage server (%Rrc)"),
                               pszFilename, vrc);
        }

        /* Close the connection early */
        RTS3Destroy(hS3);
        hS3 = NIL_RTS3;

        pTask->pProgress->SetNextOperation(BstrFmt(tr("Importing appliance")).raw(), m->ulWeightForXmlOperation);

        ComObjPtr<Progress> progress;
        /* Import the whole temporary OVF & the disk images */
        LocationInfo li;
        li.strPath = strTmpOvf;
        rc = i_importImpl(li, progress);
        if (FAILED(rc)) throw rc;

        /* Unlock the appliance for the fs import thread */
        appLock.release();
        /* Wait until the import is done, but report the progress back to the
           caller */
        ComPtr<IProgress> progressInt(progress);
        i_waitForAsyncProgress(pTask->pProgress, progressInt); /* Any errors will be thrown */

        /* Again lock the appliance for the next steps */
        appLock.acquire();
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
            if (RT_FAILURE(vrc))
                rc = setError(VBOX_E_FILE_ERROR,
                              tr("Cannot delete file '%s' (%Rrc)"), pszFilePath, vrc);
        }
    }
    /* Delete the temporary directory */
    if (RTPathExists(pszTmpDir))
    {
        vrc = RTDirRemove(pszTmpDir);
        if (RT_FAILURE(vrc))
            rc = setError(VBOX_E_FILE_ERROR,
                          tr("Cannot delete temporary directory '%s' (%Rrc)"), pszTmpDir, vrc);
    }
    if (pszTmpDir)
        RTStrFree(pszTmpDir);

    LogFlowFunc(("rc=%Rhrc\n", rc));
    LogFlowFuncLeave();

    return rc;
}
#endif /* VBOX_WITH_S3 */

HRESULT Appliance::i_readFileToBuf(const Utf8Str &strFile,
                                   void **ppvBuf,
                                   size_t *pcbSize,
                                   bool fCreateDigest,
                                   PVDINTERFACEIO pCallbacks,
                                   PSHASTORAGE pStorage)
{
    HRESULT rc = S_OK;

    bool fOldDigest = pStorage->fCreateDigest;/* Save the old digest property */
    pStorage->fCreateDigest = fCreateDigest;
    int vrc = readFileIntoBuffer(strFile.c_str(), ppvBuf, pcbSize, pCallbacks, pStorage);
    if (   RT_FAILURE(vrc)
        && vrc != VERR_FILE_NOT_FOUND)
        rc = setError(VBOX_E_FILE_ERROR,
                      tr("Could not read file '%s' (%Rrc)"),
                      RTPathFilename(strFile.c_str()), vrc);
    pStorage->fCreateDigest = fOldDigest; /* Restore the old digest creation behavior again. */

    return rc;
}

HRESULT Appliance::i_readTarFileToBuf(PFSSRDONLYINTERFACEIO pTarIo,
                                      const Utf8Str &strFile,
                                      void **ppvBuf,
                                      size_t *pcbSize,
                                      bool fCreateDigest,
                                      PVDINTERFACEIO pCallbacks,
                                      PSHASTORAGE pStorage)
{
    HRESULT rc = S_OK;

    const char *pszCurFile;
    int vrc = fssRdOnlyGetCurrentName(pTarIo, &pszCurFile);
    if (RT_SUCCESS(vrc))
    {
        if (vrc != VINF_TAR_DIR_PATH)
        {
            if (!strcmp(pszCurFile, RTPathFilename(strFile.c_str())))
                rc = i_readFileToBuf(strFile, ppvBuf, pcbSize, fCreateDigest, pCallbacks, pStorage);
        }
        else
            rc = setError(VBOX_E_FILE_ERROR,
                          tr("Empty directory folder (%s) isn't allowed in the OVA package (%Rrc)"),
                          pszCurFile, vrc);
    }
    else if (vrc != VERR_EOF)
        rc = setError(VBOX_E_IPRT_ERROR, "Seeking within the archive failed (%Rrc)", vrc);

    return rc;
}

HRESULT Appliance::i_verifyManifestFile(const Utf8Str &strFile, ImportStack &stack, void *pvBuf, size_t cbSize)
{
    LogFlowFuncEnter();
    LogFlowFunc(("Appliance %p\n", this));
    HRESULT rc = S_OK;

    PRTMANIFESTTEST paTests = (PRTMANIFESTTEST)RTMemAlloc(sizeof(RTMANIFESTTEST) * stack.llSrcDisksDigest.size());
    if (!paTests)
        return E_OUTOFMEMORY;

    size_t i = 0;
    list<STRPAIR>::const_iterator it1;
    for (it1 = stack.llSrcDisksDigest.begin();
         it1 != stack.llSrcDisksDigest.end();
         ++it1, ++i)
    {
        paTests[i].pszTestFile = (*it1).first.c_str();
        paTests[i].pszTestDigest = (*it1).second.c_str();
    }
    size_t iFailed;
    int vrc = RTManifestVerifyFilesBuf(pvBuf, cbSize, paTests, stack.llSrcDisksDigest.size(), &iFailed);
    if (RT_UNLIKELY(vrc == VERR_MANIFEST_DIGEST_MISMATCH))
        rc = setError(VBOX_E_FILE_ERROR,
                      tr("The SHA digest of '%s' does not match the one in '%s' (%Rrc)"),
                      RTPathFilename(paTests[iFailed].pszTestFile), RTPathFilename(strFile.c_str()), vrc);
    else if (RT_FAILURE(vrc))
        rc = setError(VBOX_E_FILE_ERROR,
                      tr("Could not verify the content of '%s' against the available files (%Rrc)"),
                      RTPathFilename(strFile.c_str()), vrc);

    RTMemFree(paTests);
    LogFlowFuncLeave();

    return rc;
}

HRESULT Appliance::i_verifyCertificateFile(void *pvBuf, size_t cbSize, PSHASTORAGE pStorage)
{
    LogFlowFuncEnter();
    LogFlowFunc(("Appliance %p\n", this));
    HRESULT rc = S_OK;

    int vrc = 0;
    RTDIGESTTYPE digestType;
    void * pvCertBuf = pvBuf;
    size_t cbCertSize = cbSize;
    Utf8Str manifestDigest = pStorage->strDigest;

    vrc = RTManifestVerifyDigestType(pvCertBuf, cbCertSize, &digestType);
    if (RT_FAILURE(vrc))
    {
        rc = setError(VBOX_E_FILE_ERROR, tr("Digest type of certificate is unknown"));
    }
    else
    {
        RTX509PrepareOpenSSL();

        vrc = RTRSAVerify(pvCertBuf, (unsigned int)cbCertSize, manifestDigest.c_str(), digestType);
        if (RT_SUCCESS(vrc))
        {
            /*
             * possible step in the future. Not obligatory due to OVF2.0 standard
             * OVF2.0:"A consumer of the OVF package shall verify the signature and should validate the certificate"
             */
            vrc = RTX509CertificateVerify(pvCertBuf, (unsigned int)cbCertSize);
        }

        /* After first unsuccessful operation */
        if (RT_FAILURE(vrc))
        {
            {
                /* first stage for getting possible error code and it's description using native openssl method */
                char* errStrDesc = NULL;
                unsigned long errValue = RTX509GetErrorDescription(&errStrDesc);

                if(errValue != 0)
                {
                    rc = setError(VBOX_E_FILE_ERROR, tr(errStrDesc));
                    LogFlowFunc(("Error during verifying X509 certificate(internal openssl description): %s\n", errStrDesc));
                }

                RTMemFree(errStrDesc);
            }

            {
                /* second stage for getting possible error code using our defined errors codes. The original error description
                   will be replaced by our description */

                Utf8Str errStrDesc;
                switch(vrc)
                {
                    case VERR_X509_READING_CERT_FROM_BIO:
                        errStrDesc = "Error during reading a certificate in PEM format from BIO ";
                        break;
                    case VERR_X509_EXTRACT_PUBKEY_FROM_CERT:
                        errStrDesc = "Error during extraction a public key from the certificate ";
                        break;
                    case VERR_X509_EXTRACT_RSA_FROM_PUBLIC_KEY:
                        errStrDesc = "Error during extraction RSA from the public key ";
                        break;
                    case VERR_X509_RSA_VERIFICATION_FUILURE:
                        errStrDesc = "RSA verification failure ";
                        break;
                    case VERR_X509_NO_BASIC_CONSTARAINTS:
                        errStrDesc = "Basic constraints were not found ";
                        break;
                    case VERR_X509_GETTING_EXTENSION_FROM_CERT:
                        errStrDesc = "Error during getting extensions from the certificate ";
                        break;
                    case VERR_X509_GETTING_DATA_FROM_EXTENSION:
                        errStrDesc = "Error during extraction data from the extension ";
                        break;
                    case VERR_X509_PRINT_EXTENSION_TO_BIO:
                        errStrDesc = "Error during print out an extension to BIO ";
                        break;
                    case VERR_X509_CERTIFICATE_VERIFICATION_FAILURE:
                        errStrDesc = "X509 certificate verification failure ";
                        break;
                    default:
                        errStrDesc = "Unknown error during X509 certificate verification";
                }
                rc = setError(VBOX_E_FILE_ERROR, tr(errStrDesc.c_str()));
            }
        }
        else
        {
            if(vrc == VINF_X509_NOT_SELFSIGNED_CERTIFICATE)
            {
                setWarning(VBOX_E_FILE_ERROR,
                           tr("Signature from the X509 certificate has been verified. "
                              "But VirtualBox can't validate the given X509 certificate. "
                              "Only self signed X509 certificates are supported at moment. \n"));
            }
        }
    }

    LogFlowFuncLeave();
    return rc;
}

/**
 * Helper that converts VirtualSystem attachment values into VirtualBox attachment values.
 * Throws HRESULT values on errors!
 *
 * @param hdc in: the HardDiskController structure to attach to.
 * @param ulAddressOnParent in: the AddressOnParent parameter from OVF.
 * @param controllerType out: the name of the hard disk controller to attach to (e.g. "IDE Controller").
 * @param lControllerPort out: the channel (controller port) of the controller to attach to.
 * @param lDevice out: the device number to attach to.
 */
void Appliance::i_convertDiskAttachmentValues(const ovf::HardDiskController &hdc,
                                            uint32_t ulAddressOnParent,
                                            Bstr &controllerType,
                                            int32_t &lControllerPort,
                                            int32_t &lDevice)
{
    Log(("Appliance::i_convertDiskAttachmentValues: hdc.system=%d, hdc.fPrimary=%d, ulAddressOnParent=%d\n",
         hdc.system,
         hdc.fPrimary,
         ulAddressOnParent));

    switch (hdc.system)
    {
        case ovf::HardDiskController::IDE:
            // For the IDE bus, the port parameter can be either 0 or 1, to specify the primary
            // or secondary IDE controller, respectively. For the primary controller of the IDE bus,
            // the device number can be either 0 or 1, to specify the master or the slave device,
            // respectively. For the secondary IDE controller, the device number is always 1 because
            // the master device is reserved for the CD-ROM drive.
            controllerType = Bstr("IDE Controller");
            switch (ulAddressOnParent)
            {
                case 0: // master
                    if (!hdc.fPrimary)
                    {
                        // secondary master
                        lControllerPort = (long)1;
                        lDevice = (long)0;
                    }
                    else // primary master
                    {
                        lControllerPort = (long)0;
                        lDevice = (long)0;
                    }
                break;

                case 1: // slave
                    if (!hdc.fPrimary)
                    {
                        // secondary slave
                        lControllerPort = (long)1;
                        lDevice = (long)1;
                    }
                    else // primary slave
                    {
                        lControllerPort = (long)0;
                        lDevice = (long)1;
                    }
                break;

                // used by older VBox exports
                case 2:     // interpret this as secondary master
                    lControllerPort = (long)1;
                    lDevice = (long)0;
                break;

                // used by older VBox exports
                case 3:     // interpret this as secondary slave
                    lControllerPort = (long)1;
                    lDevice = (long)1;
                break;

                default:
                    throw setError(VBOX_E_NOT_SUPPORTED,
                                   tr("Invalid channel %RI16 specified; IDE controllers support only 0, 1 or 2"),
                                   ulAddressOnParent);
                break;
            }
        break;

        case ovf::HardDiskController::SATA:
            controllerType = Bstr("SATA Controller");
            lControllerPort = (long)ulAddressOnParent;
            lDevice = (long)0;
        break;

        case ovf::HardDiskController::SCSI:
        {
            if(hdc.strControllerType.compare("lsilogicsas")==0)
                controllerType = Bstr("SAS Controller");
            else
                controllerType = Bstr("SCSI Controller");
            lControllerPort = (long)ulAddressOnParent;
            lDevice = (long)0;
        }
        break;

        default: break;
    }

    Log(("=> lControllerPort=%d, lDevice=%d\n", lControllerPort, lDevice));
}

/**
 * Imports one disk image. This is common code shared between
 *  --  i_importMachineGeneric() for the OVF case; in that case the information comes from
 *      the OVF virtual systems;
 *  --  i_importVBoxMachine(); in that case, the information comes from the <vbox:Machine>
 *      tag.
 *
 * Both ways of describing machines use the OVF disk references section, so in both cases
 * the caller needs to pass in the ovf::DiskImage structure from ovfreader.cpp.
 *
 * As a result, in both cases, if di.strHref is empty, we create a new disk as per the OVF
 * spec, even though this cannot really happen in the vbox:Machine case since such data
 * would never have been exported.
 *
 * This advances stack.pProgress by one operation with the disk's weight.
 *
 * @param di ovfreader.cpp structure describing the disk image from the OVF that is to be imported
 * @param strTargetPath Where to create the target image.
 * @param pTargetHD out: The newly created target disk. This also gets pushed on stack.llHardDisksCreated for cleanup.
 * @param stack
 */
void Appliance::i_importOneDiskImage(const ovf::DiskImage &di,
                                     Utf8Str *strTargetPath,
                                     ComObjPtr<Medium> &pTargetHD,
                                     ImportStack &stack,
                                     PVDINTERFACEIO pCallbacks,
                                     PSHASTORAGE pStorage)
{
    SHASTORAGE finalStorage;
    PSHASTORAGE pRealUsedStorage = pStorage;/* may be changed later to finalStorage */
    PVDINTERFACEIO pFileIo = NULL;/* used in GZIP case*/
    ComObjPtr<Progress> pProgress;
    pProgress.createObject();
    HRESULT rc = pProgress->init(mVirtualBox,
                                 static_cast<IAppliance*>(this),
                                 BstrFmt(tr("Creating medium '%s'"),
                                 strTargetPath->c_str()).raw(),
                                 TRUE);
    if (FAILED(rc)) throw rc;

    /* Get the system properties. */
    SystemProperties *pSysProps = mVirtualBox->i_getSystemProperties();

    /*
     * we put strSourceOVF into the stack.llSrcDisksDigest in the end of this
     * function like a key for a later validation of the SHA digests
     */
    const Utf8Str &strSourceOVF = di.strHref;

    Utf8Str strSrcFilePath(stack.strSourceDir);
    Utf8Str strTargetDir(*strTargetPath);

    /* Construct source file path */
    Utf8Str name = i_applianceIOName(applianceIOTar);

    if (RTStrNICmp(pStorage->pVDImageIfaces->pszInterfaceName, name.c_str(), name.length()) == 0)
        strSrcFilePath = strSourceOVF;
    else
    {
        strSrcFilePath.append(RTPATH_SLASH_STR);
        strSrcFilePath.append(strSourceOVF);
    }

    /* First of all check if the path is an UUID. If so, the user like to
     * import the disk into an existing path. This is useful for iSCSI for
     * example. */
    RTUUID uuid;
    int vrc = RTUuidFromStr(&uuid, strTargetPath->c_str());
    if (vrc == VINF_SUCCESS)
    {
        rc = mVirtualBox->i_findHardDiskById(Guid(uuid), true, &pTargetHD);
        if (FAILED(rc)) throw rc;
    }
    else
    {
        bool fGzipUsed = !(di.strCompression.compare("gzip",Utf8Str::CaseInsensitive));
        /* check read file to GZIP compression */
        try
        {
            if (fGzipUsed == true)
            {
                /*
                 * Create the necessary file access interfaces.
                 * For the next step:
                 * We need to replace the previously created chain of SHA-TAR or SHA-FILE interfaces
                 * with simple FILE interface because we don't need SHA or TAR interfaces here anymore.
                 * But we mustn't delete the chain of SHA-TAR or SHA-FILE interfaces.
                 */

                /* Decompress the GZIP file and save a new file in the target path */
                strTargetDir = strTargetDir.stripFilename();
                strTargetDir.append(RTPATH_SLASH_STR);
                strTargetDir.append("temp_");

                Utf8Str strTempTargetFilename(strSrcFilePath);
                strTempTargetFilename = strTempTargetFilename.stripPath();

                strTargetDir.append(strTempTargetFilename);

                vrc = decompressImageAndSave(strSrcFilePath.c_str(), strTargetDir.c_str(), pCallbacks, pStorage);

                if (RT_FAILURE(vrc))
                    throw setError(VBOX_E_FILE_ERROR,
                                   tr("Could not read the file '%s' (%Rrc)"),
                                   RTPathFilename(strSrcFilePath.c_str()), vrc);

                /* Create the necessary file access interfaces. */
                pFileIo = FileCreateInterface();
                if (!pFileIo)
                    throw setError(E_OUTOFMEMORY);

                name = i_applianceIOName(applianceIOFile);

                vrc = VDInterfaceAdd(&pFileIo->Core, name.c_str(),
                                     VDINTERFACETYPE_IO, NULL, sizeof(VDINTERFACEIO),
                                     &finalStorage.pVDImageIfaces);
                if (RT_FAILURE(vrc))
                    throw setError(VBOX_E_IPRT_ERROR,
                                   tr("Creation of the VD interface failed (%Rrc)"), vrc);

                /* Correct the source and the target with the actual values */
                strSrcFilePath = strTargetDir;

                pRealUsedStorage = &finalStorage;
            }

            Utf8Str strTrgFormat = "VMDK";
            ComObjPtr<MediumFormat> trgFormat;
            Bstr bstrFormatName;
            ULONG lCabs = 0;

            //check existence of option "ImportToVDI", in this case all imported disks will be converted to VDI images
            bool chExt = m->optListImport.contains(ImportOptions_ImportToVDI);

            char *pszSuff = NULL;

            if ((pszSuff = RTPathSuffix(strTargetPath->c_str()))!=NULL)
            {
                /* 
                 * Figure out which format the user like to have. Default is VMDK
                 * or it can be VDI if according command-line option is set
                 */

                /*
                 * We need a proper target format
                 * if target format has been changed by user via GUI import wizard
                 * or via VBoxManage import command (option --importtovdi)
                 * then we need properly process such format like ISO
                 * Because there is no conversion ISO to VDI
                 */

                pszSuff++;
                trgFormat = pSysProps->i_mediumFormatFromExtension(pszSuff);
                if (trgFormat.isNull())
                {
                    rc = setError(E_FAIL,
                           tr("Internal inconsistency looking up medium format for the disk image '%s'"),
                           di.strHref.c_str());
                }

                rc = trgFormat->COMGETTER(Name)(bstrFormatName.asOutParam());
                if (FAILED(rc)) throw rc;

                strTrgFormat = Utf8Str(bstrFormatName);

                if(chExt && strTrgFormat.compare("RAW", Utf8Str::CaseInsensitive) != 0)
                {
                    /* change the target extension */
                    strTrgFormat = "vdi";
                    trgFormat = pSysProps->i_mediumFormatFromExtension(strTrgFormat);
                    *strTargetPath = strTargetPath->stripSuffix();
                    *strTargetPath = strTargetPath->append(".");
                    *strTargetPath = strTargetPath->append(strTrgFormat.c_str());
                }

                /* Check the capabilities. We need create capabilities. */
                lCabs = 0;
                com::SafeArray <MediumFormatCapabilities_T> mediumFormatCap;
                rc = trgFormat->COMGETTER(Capabilities)(ComSafeArrayAsOutParam(mediumFormatCap));

                if (FAILED(rc))
                    throw rc;
                else
                {
                    for (ULONG j = 0; j < mediumFormatCap.size(); j++)
                        lCabs |= mediumFormatCap[j];
                }

                if (!(   ((lCabs & MediumFormatCapabilities_CreateFixed) == MediumFormatCapabilities_CreateFixed)
                      || ((lCabs & MediumFormatCapabilities_CreateDynamic) == MediumFormatCapabilities_CreateDynamic)))
                    throw setError(VBOX_E_NOT_SUPPORTED,
                                   tr("Could not find a valid medium format for the target disk '%s'"),
                                   strTargetPath->c_str());
            }
            else
            {
                throw setError(VBOX_E_FILE_ERROR,
                               tr("The target disk '%s' has no extension "),
                               strTargetPath->c_str(), VERR_INVALID_NAME);
            }

            /* Create an IMedium object. */
            pTargetHD.createObject();

            /*CD/DVD case*/
            if (strTrgFormat.compare("RAW", Utf8Str::CaseInsensitive) == 0)
            {
                try
                {
                    if (fGzipUsed == true)
                    {
                        /*
                         * The source and target pathes are the same.
                         * It means that we have the needed file already.
                         * For example, in GZIP case, we decompress the file and save it in the target path,
                         * but with some prefix like "temp_". See part "check read file to GZIP compression" earlier
                         * in this function.
                         * Just rename the file by deleting "temp_" from it's name
                         */
                        vrc = RTFileRename(strSrcFilePath.c_str(), strTargetPath->c_str(), RTPATHRENAME_FLAGS_NO_REPLACE);
                        if (RT_FAILURE(vrc))
                            throw setError(VBOX_E_FILE_ERROR,
                                           tr("Could not rename the file '%s' (%Rrc)"),
                                           RTPathFilename(strSourceOVF.c_str()), vrc);
                    }
                    else
                    {
                        /* Calculating SHA digest for ISO file while copying one */
                        vrc = copyFileAndCalcShaDigest(strSrcFilePath.c_str(),
                                                       strTargetPath->c_str(),
                                                       pCallbacks,
                                                       pRealUsedStorage);

                        if (RT_FAILURE(vrc))
                            throw setError(VBOX_E_FILE_ERROR,
                                           tr("Could not copy ISO file '%s' listed in the OVF file (%Rrc)"),
                                           RTPathFilename(strSourceOVF.c_str()), vrc);
                    }
                }
                catch (HRESULT /*arc*/)
                {
                    throw;
                }

                /* Advance to the next operation. */
                /* operation's weight, as set up with the IProgress originally */
                stack.pProgress->SetNextOperation(BstrFmt(tr("Importing virtual disk image '%s'"),
                                                  RTPathFilename(strSourceOVF.c_str())).raw(),
                                                  di.ulSuggestedSizeMB);
            }
            else/* HDD case*/
            {
                rc = pTargetHD->init(mVirtualBox,
                                     strTrgFormat,
                                     *strTargetPath,
                                     Guid::Empty /* media registry: none yet */,
                                     DeviceType_HardDisk);
                if (FAILED(rc)) throw rc;

                /* Now create an empty hard disk. */
                rc = mVirtualBox->CreateMedium(Bstr(strTrgFormat).raw(),
                                               Bstr(*strTargetPath).raw(),
                                               AccessMode_ReadWrite, DeviceType_HardDisk,
                                               ComPtr<IMedium>(pTargetHD).asOutParam());
                if (FAILED(rc)) throw rc;

                /* If strHref is empty we have to create a new file. */
                if (strSourceOVF.isEmpty())
                {
                    com::SafeArray<MediumVariant_T>  mediumVariant;
                    mediumVariant.push_back(MediumVariant_Standard);
                    /* Create a dynamic growing disk image with the given capacity. */
                    rc = pTargetHD->CreateBaseStorage(di.iCapacity / _1M,
                                                      ComSafeArrayAsInParam(mediumVariant),
                                                      ComPtr<IProgress>(pProgress).asOutParam());
                    if (FAILED(rc)) throw rc;

                    /* Advance to the next operation. */
                    /* operation's weight, as set up with the IProgress originally */
                    stack.pProgress->SetNextOperation(BstrFmt(tr("Creating disk image '%s'"),
                                                      strTargetPath->c_str()).raw(),
                                                      di.ulSuggestedSizeMB);
                }
                else
                {
                    /* We need a proper source format description */
                    /* Which format to use? */
                    ComObjPtr<MediumFormat> srcFormat;
                    rc = i_findMediumFormatFromDiskImage(di, srcFormat);
                    if (FAILED(rc))
                        throw setError(VBOX_E_NOT_SUPPORTED,
                                       tr("Could not find a valid medium format for the source disk '%s' "
                                          "Check correctness of the image format URL in the OVF description file "
                                          "or extension of the image"),
                                       RTPathFilename(strSourceOVF.c_str()));

                    /* Clone the source disk image */
                    ComObjPtr<Medium> nullParent;
                    rc = pTargetHD->i_importFile(strSrcFilePath.c_str(),
                                                 srcFormat,
                                                 MediumVariant_Standard,
                                                 pCallbacks, pRealUsedStorage,
                                                 nullParent,
                                                 pProgress);
                    if (FAILED(rc)) throw rc;



                    /* Advance to the next operation. */
                    /* operation's weight, as set up with the IProgress originally */
                    stack.pProgress->SetNextOperation(BstrFmt(tr("Importing virtual disk image '%s'"),
                                                      RTPathFilename(strSourceOVF.c_str())).raw(),
                                                      di.ulSuggestedSizeMB);
                }

                /* Now wait for the background disk operation to complete; this throws
                 * HRESULTs on error. */
                ComPtr<IProgress> pp(pProgress);
                i_waitForAsyncProgress(stack.pProgress, pp);

                if (fGzipUsed == true)
                {
                    /*
                     * Just delete the temporary file
                     */
                    vrc = RTFileDelete(strSrcFilePath.c_str());
                    if (RT_FAILURE(vrc))
                        setWarning(VBOX_E_FILE_ERROR,
                                   tr("Could not delete the file '%s' (%Rrc)"),
                                   RTPathFilename(strSrcFilePath.c_str()), vrc);
                }
            }
        }
        catch (...)
        {
            if (pFileIo)
                RTMemFree(pFileIo);

            throw;
        }
    }

    if (pFileIo)
        RTMemFree(pFileIo);

    /* Add the newly create disk path + a corresponding digest the our list for
     * later manifest verification. */
    stack.llSrcDisksDigest.push_back(STRPAIR(strSourceOVF, pStorage ? pStorage->strDigest : ""));
}

/**
 * Imports one OVF virtual system (described by the given ovf::VirtualSystem and VirtualSystemDescription)
 * into VirtualBox by creating an IMachine instance, which is returned.
 *
 * This throws HRESULT error codes for anything that goes wrong, in which case the caller must clean
 * up any leftovers from this function. For this, the given ImportStack instance has received information
 * about what needs cleaning up (to support rollback).
 *
 * @param vsysThis OVF virtual system (machine) to import.
 * @param vsdescThis  Matching virtual system description (machine) to import.
 * @param pNewMachine out: Newly created machine.
 * @param stack Cleanup stack for when this throws.
 */
void Appliance::i_importMachineGeneric(const ovf::VirtualSystem &vsysThis,
                                       ComObjPtr<VirtualSystemDescription> &vsdescThis,
                                       ComPtr<IMachine> &pNewMachine,
                                       ImportStack &stack,
                                       PVDINTERFACEIO pCallbacks,
                                       PSHASTORAGE pStorage)
{
    LogFlowFuncEnter();
    HRESULT rc;

    // Get the instance of IGuestOSType which matches our string guest OS type so we
    // can use recommended defaults for the new machine where OVF doesn't provide any
    ComPtr<IGuestOSType> osType;
    rc = mVirtualBox->GetGuestOSType(Bstr(stack.strOsTypeVBox).raw(), osType.asOutParam());
    if (FAILED(rc)) throw rc;

    /* Create the machine */
    SafeArray<BSTR> groups; /* no groups */
    rc = mVirtualBox->CreateMachine(NULL, /* machine name: use default */
                                    Bstr(stack.strNameVBox).raw(),
                                    ComSafeArrayAsInParam(groups),
                                    Bstr(stack.strOsTypeVBox).raw(),
                                    NULL, /* aCreateFlags */
                                    pNewMachine.asOutParam());
    if (FAILED(rc)) throw rc;

    // set the description
    if (!stack.strDescription.isEmpty())
    {
        rc = pNewMachine->COMSETTER(Description)(Bstr(stack.strDescription).raw());
        if (FAILED(rc)) throw rc;
    }

    // CPU count
    rc = pNewMachine->COMSETTER(CPUCount)(stack.cCPUs);
    if (FAILED(rc)) throw rc;

    if (stack.fForceHWVirt)
    {
        rc = pNewMachine->SetHWVirtExProperty(HWVirtExPropertyType_Enabled, TRUE);
        if (FAILED(rc)) throw rc;
    }

    // RAM
    rc = pNewMachine->COMSETTER(MemorySize)(stack.ulMemorySizeMB);
    if (FAILED(rc)) throw rc;

    /* VRAM */
    /* Get the recommended VRAM for this guest OS type */
    ULONG vramVBox;
    rc = osType->COMGETTER(RecommendedVRAM)(&vramVBox);
    if (FAILED(rc)) throw rc;

    /* Set the VRAM */
    rc = pNewMachine->COMSETTER(VRAMSize)(vramVBox);
    if (FAILED(rc)) throw rc;

    // I/O APIC: Generic OVF has no setting for this. Enable it if we
    // import a Windows VM because if if Windows was installed without IOAPIC,
    // it will not mind finding an one later on, but if Windows was installed
    // _with_ an IOAPIC, it will bluescreen if it's not found
    if (!stack.fForceIOAPIC)
    {
        Bstr bstrFamilyId;
        rc = osType->COMGETTER(FamilyId)(bstrFamilyId.asOutParam());
        if (FAILED(rc)) throw rc;
        if (bstrFamilyId == "Windows")
            stack.fForceIOAPIC = true;
    }

    if (stack.fForceIOAPIC)
    {
        ComPtr<IBIOSSettings> pBIOSSettings;
        rc = pNewMachine->COMGETTER(BIOSSettings)(pBIOSSettings.asOutParam());
        if (FAILED(rc)) throw rc;

        rc = pBIOSSettings->COMSETTER(IOAPICEnabled)(TRUE);
        if (FAILED(rc)) throw rc;
    }

    if (!stack.strAudioAdapter.isEmpty())
        if (stack.strAudioAdapter.compare("null", Utf8Str::CaseInsensitive) != 0)
        {
            uint32_t audio = RTStrToUInt32(stack.strAudioAdapter.c_str());       // should be 0 for AC97
            ComPtr<IAudioAdapter> audioAdapter;
            rc = pNewMachine->COMGETTER(AudioAdapter)(audioAdapter.asOutParam());
            if (FAILED(rc)) throw rc;
            rc = audioAdapter->COMSETTER(Enabled)(true);
            if (FAILED(rc)) throw rc;
            rc = audioAdapter->COMSETTER(AudioController)(static_cast<AudioControllerType_T>(audio));
            if (FAILED(rc)) throw rc;
        }

#ifdef VBOX_WITH_USB
    /* USB Controller */
    if (stack.fUSBEnabled)
    {
        ComPtr<IUSBController> usbController;
        rc = pNewMachine->AddUSBController(Bstr("OHCI").raw(), USBControllerType_OHCI, usbController.asOutParam());
        if (FAILED(rc)) throw rc;
    }
#endif /* VBOX_WITH_USB */

    /* Change the network adapters */
    uint32_t maxNetworkAdapters = Global::getMaxNetworkAdapters(ChipsetType_PIIX3);

    std::list<VirtualSystemDescriptionEntry*> vsdeNW = vsdescThis->i_findByType(VirtualSystemDescriptionType_NetworkAdapter);
    if (vsdeNW.size() == 0)
    {
        /* No network adapters, so we have to disable our default one */
        ComPtr<INetworkAdapter> nwVBox;
        rc = pNewMachine->GetNetworkAdapter(0, nwVBox.asOutParam());
        if (FAILED(rc)) throw rc;
        rc = nwVBox->COMSETTER(Enabled)(false);
        if (FAILED(rc)) throw rc;
    }
    else if (vsdeNW.size() > maxNetworkAdapters)
        throw setError(VBOX_E_FILE_ERROR,
                       tr("Too many network adapters: OVF requests %d network adapters, "
                          "but VirtualBox only supports %d"),
                       vsdeNW.size(), maxNetworkAdapters);
    else
    {
        list<VirtualSystemDescriptionEntry*>::const_iterator nwIt;
        size_t a = 0;
        for (nwIt = vsdeNW.begin();
             nwIt != vsdeNW.end();
             ++nwIt, ++a)
        {
            const VirtualSystemDescriptionEntry* pvsys = *nwIt;

            const Utf8Str &nwTypeVBox = pvsys->strVBoxCurrent;
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
            if (pvsys->strExtraConfigCurrent.endsWith("type=Bridged", Utf8Str::CaseInsensitive))
            {
                /* Attach to the right interface */
                rc = pNetworkAdapter->COMSETTER(AttachmentType)(NetworkAttachmentType_Bridged);
                if (FAILED(rc)) throw rc;
                ComPtr<IHost> host;
                rc = mVirtualBox->COMGETTER(Host)(host.asOutParam());
                if (FAILED(rc)) throw rc;
                com::SafeIfaceArray<IHostNetworkInterface> nwInterfaces;
                rc = host->COMGETTER(NetworkInterfaces)(ComSafeArrayAsOutParam(nwInterfaces));
                if (FAILED(rc)) throw rc;
                // We search for the first host network interface which
                // is usable for bridged networking
                for (size_t j = 0;
                     j < nwInterfaces.size();
                     ++j)
                {
                    HostNetworkInterfaceType_T itype;
                    rc = nwInterfaces[j]->COMGETTER(InterfaceType)(&itype);
                    if (FAILED(rc)) throw rc;
                    if (itype == HostNetworkInterfaceType_Bridged)
                    {
                        Bstr name;
                        rc = nwInterfaces[j]->COMGETTER(Name)(name.asOutParam());
                        if (FAILED(rc)) throw rc;
                        /* Set the interface name to attach to */
                        rc = pNetworkAdapter->COMSETTER(BridgedInterface)(name.raw());
                        if (FAILED(rc)) throw rc;
                        break;
                    }
                }
            }
            /* Next test for host only interfaces */
            else if (pvsys->strExtraConfigCurrent.endsWith("type=HostOnly", Utf8Str::CaseInsensitive))
            {
                /* Attach to the right interface */
                rc = pNetworkAdapter->COMSETTER(AttachmentType)(NetworkAttachmentType_HostOnly);
                if (FAILED(rc)) throw rc;
                ComPtr<IHost> host;
                rc = mVirtualBox->COMGETTER(Host)(host.asOutParam());
                if (FAILED(rc)) throw rc;
                com::SafeIfaceArray<IHostNetworkInterface> nwInterfaces;
                rc = host->COMGETTER(NetworkInterfaces)(ComSafeArrayAsOutParam(nwInterfaces));
                if (FAILED(rc)) throw rc;
                // We search for the first host network interface which
                // is usable for host only networking
                for (size_t j = 0;
                     j < nwInterfaces.size();
                     ++j)
                {
                    HostNetworkInterfaceType_T itype;
                    rc = nwInterfaces[j]->COMGETTER(InterfaceType)(&itype);
                    if (FAILED(rc)) throw rc;
                    if (itype == HostNetworkInterfaceType_HostOnly)
                    {
                        Bstr name;
                        rc = nwInterfaces[j]->COMGETTER(Name)(name.asOutParam());
                        if (FAILED(rc)) throw rc;
                        /* Set the interface name to attach to */
                        rc = pNetworkAdapter->COMSETTER(HostOnlyInterface)(name.raw());
                        if (FAILED(rc)) throw rc;
                        break;
                    }
                }
            }
            /* Next test for internal interfaces */
            else if (pvsys->strExtraConfigCurrent.endsWith("type=Internal", Utf8Str::CaseInsensitive))
            {
                /* Attach to the right interface */
                rc = pNetworkAdapter->COMSETTER(AttachmentType)(NetworkAttachmentType_Internal);
                if (FAILED(rc)) throw rc;
            }
            /* Next test for Generic interfaces */
            else if (pvsys->strExtraConfigCurrent.endsWith("type=Generic", Utf8Str::CaseInsensitive))
            {
                /* Attach to the right interface */
                rc = pNetworkAdapter->COMSETTER(AttachmentType)(NetworkAttachmentType_Generic);
                if (FAILED(rc)) throw rc;
            }

            /* Next test for NAT network interfaces */
            else if (pvsys->strExtraConfigCurrent.endsWith("type=NATNetwork", Utf8Str::CaseInsensitive))
            {
                /* Attach to the right interface */
                rc = pNetworkAdapter->COMSETTER(AttachmentType)(NetworkAttachmentType_NATNetwork);
                if (FAILED(rc)) throw rc;
                com::SafeIfaceArray<INATNetwork> nwNATNetworks;
                rc = mVirtualBox->COMGETTER(NATNetworks)(ComSafeArrayAsOutParam(nwNATNetworks));
                if (FAILED(rc)) throw rc;
                // Pick the first NAT network (if there is any)
                if (nwNATNetworks.size())
                {
                    Bstr name;
                    rc = nwNATNetworks[0]->COMGETTER(NetworkName)(name.asOutParam());
                    if (FAILED(rc)) throw rc;
                    /* Set the NAT network name to attach to */
                    rc = pNetworkAdapter->COMSETTER(NATNetwork)(name.raw());
                    if (FAILED(rc)) throw rc;
                    break;
                }
            }
        }
    }

    // IDE Hard disk controller
    std::list<VirtualSystemDescriptionEntry*> vsdeHDCIDE =
        vsdescThis->i_findByType(VirtualSystemDescriptionType_HardDiskControllerIDE);
    /*
     * In OVF (at least VMware's version of it), an IDE controller has two ports,
     * so VirtualBox's single IDE controller with two channels and two ports each counts as
     * two OVF IDE controllers -- so we accept one or two such IDE controllers
     */
    size_t cIDEControllers = vsdeHDCIDE.size();
    if (cIDEControllers > 2)
        throw setError(VBOX_E_FILE_ERROR,
                       tr("Too many IDE controllers in OVF; import facility only supports two"));
    if (vsdeHDCIDE.size() > 0)
    {
        // one or two IDE controllers present in OVF: add one VirtualBox controller
        ComPtr<IStorageController> pController;
        rc = pNewMachine->AddStorageController(Bstr("IDE Controller").raw(), StorageBus_IDE, pController.asOutParam());
        if (FAILED(rc)) throw rc;

        const char *pcszIDEType = vsdeHDCIDE.front()->strVBoxCurrent.c_str();
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

    /* Hard disk controller SATA */
    std::list<VirtualSystemDescriptionEntry*> vsdeHDCSATA =
        vsdescThis->i_findByType(VirtualSystemDescriptionType_HardDiskControllerSATA);
    if (vsdeHDCSATA.size() > 1)
        throw setError(VBOX_E_FILE_ERROR,
                       tr("Too many SATA controllers in OVF; import facility only supports one"));
    if (vsdeHDCSATA.size() > 0)
    {
        ComPtr<IStorageController> pController;
        const Utf8Str &hdcVBox = vsdeHDCSATA.front()->strVBoxCurrent;
        if (hdcVBox == "AHCI")
        {
            rc = pNewMachine->AddStorageController(Bstr("SATA Controller").raw(),
                                                   StorageBus_SATA,
                                                   pController.asOutParam());
            if (FAILED(rc)) throw rc;
        }
        else
            throw setError(VBOX_E_FILE_ERROR,
                           tr("Invalid SATA controller type \"%s\""),
                           hdcVBox.c_str());
    }

    /* Hard disk controller SCSI */
    std::list<VirtualSystemDescriptionEntry*> vsdeHDCSCSI =
        vsdescThis->i_findByType(VirtualSystemDescriptionType_HardDiskControllerSCSI);
    if (vsdeHDCSCSI.size() > 1)
        throw setError(VBOX_E_FILE_ERROR,
                       tr("Too many SCSI controllers in OVF; import facility only supports one"));
    if (vsdeHDCSCSI.size() > 0)
    {
        ComPtr<IStorageController> pController;
        Bstr bstrName(L"SCSI Controller");
        StorageBus_T busType = StorageBus_SCSI;
        StorageControllerType_T controllerType;
        const Utf8Str &hdcVBox = vsdeHDCSCSI.front()->strVBoxCurrent;
        if (hdcVBox == "LsiLogic")
            controllerType = StorageControllerType_LsiLogic;
        else if (hdcVBox == "LsiLogicSas")
        {
            // OVF treats LsiLogicSas as a SCSI controller but VBox considers it a class of its own
            bstrName = L"SAS Controller";
            busType = StorageBus_SAS;
            controllerType = StorageControllerType_LsiLogicSas;
        }
        else if (hdcVBox == "BusLogic")
            controllerType = StorageControllerType_BusLogic;
        else
            throw setError(VBOX_E_FILE_ERROR,
                           tr("Invalid SCSI controller type \"%s\""),
                           hdcVBox.c_str());

        rc = pNewMachine->AddStorageController(bstrName.raw(), busType, pController.asOutParam());
        if (FAILED(rc)) throw rc;
        rc = pController->COMSETTER(ControllerType)(controllerType);
        if (FAILED(rc)) throw rc;
    }

    /* Hard disk controller SAS */
    std::list<VirtualSystemDescriptionEntry*> vsdeHDCSAS =
        vsdescThis->i_findByType(VirtualSystemDescriptionType_HardDiskControllerSAS);
    if (vsdeHDCSAS.size() > 1)
        throw setError(VBOX_E_FILE_ERROR,
                       tr("Too many SAS controllers in OVF; import facility only supports one"));
    if (vsdeHDCSAS.size() > 0)
    {
        ComPtr<IStorageController> pController;
        rc = pNewMachine->AddStorageController(Bstr(L"SAS Controller").raw(),
                                               StorageBus_SAS,
                                               pController.asOutParam());
        if (FAILED(rc)) throw rc;
        rc = pController->COMSETTER(ControllerType)(StorageControllerType_LsiLogicSas);
        if (FAILED(rc)) throw rc;
    }

    /* Now its time to register the machine before we add any hard disks */
    rc = mVirtualBox->RegisterMachine(pNewMachine);
    if (FAILED(rc)) throw rc;

    // store new machine for roll-back in case of errors
    Bstr bstrNewMachineId;
    rc = pNewMachine->COMGETTER(Id)(bstrNewMachineId.asOutParam());
    if (FAILED(rc)) throw rc;
    Guid uuidNewMachine(bstrNewMachineId);
    m->llGuidsMachinesCreated.push_back(uuidNewMachine);

    // Add floppies and CD-ROMs to the appropriate controllers.
    std::list<VirtualSystemDescriptionEntry*> vsdeFloppy = vsdescThis->i_findByType(VirtualSystemDescriptionType_Floppy);
    if (vsdeFloppy.size() > 1)
        throw setError(VBOX_E_FILE_ERROR,
                       tr("Too many floppy controllers in OVF; import facility only supports one"));
    std::list<VirtualSystemDescriptionEntry*> vsdeCDROM = vsdescThis->i_findByType(VirtualSystemDescriptionType_CDROM);
    if (    (vsdeFloppy.size() > 0)
         || (vsdeCDROM.size() > 0)
       )
    {
        // If there's an error here we need to close the session, so
        // we need another try/catch block.

        try
        {
            // to attach things we need to open a session for the new machine
            rc = pNewMachine->LockMachine(stack.pSession, LockType_Write);
            if (FAILED(rc)) throw rc;
            stack.fSessionOpen = true;

            ComPtr<IMachine> sMachine;
            rc = stack.pSession->COMGETTER(Machine)(sMachine.asOutParam());
            if (FAILED(rc)) throw rc;

            // floppy first
            if (vsdeFloppy.size() == 1)
            {
                ComPtr<IStorageController> pController;
                rc = sMachine->AddStorageController(Bstr("Floppy Controller").raw(),
                                                    StorageBus_Floppy,
                                                    pController.asOutParam());
                if (FAILED(rc)) throw rc;

                Bstr bstrName;
                rc = pController->COMGETTER(Name)(bstrName.asOutParam());
                if (FAILED(rc)) throw rc;

                // this is for rollback later
                MyHardDiskAttachment mhda;
                mhda.pMachine = pNewMachine;
                mhda.controllerType = bstrName;
                mhda.lControllerPort = 0;
                mhda.lDevice = 0;

                Log(("Attaching floppy\n"));

                rc = sMachine->AttachDevice(mhda.controllerType.raw(),
                                            mhda.lControllerPort,
                                            mhda.lDevice,
                                            DeviceType_Floppy,
                                            NULL);
                if (FAILED(rc)) throw rc;

                stack.llHardDiskAttachments.push_back(mhda);
            }

            rc = sMachine->SaveSettings();
            if (FAILED(rc)) throw rc;

            // only now that we're done with all disks, close the session
            rc = stack.pSession->UnlockMachine();
            if (FAILED(rc)) throw rc;
            stack.fSessionOpen = false;
        }
        catch(HRESULT aRC)
        {
            com::ErrorInfo info;

            if (stack.fSessionOpen)
                stack.pSession->UnlockMachine();

            if (info.isFullAvailable())
                throw setError(aRC, Utf8Str(info.getText()).c_str());
            else
                throw setError(aRC, "Unknown error during OVF import");
        }
    }

    // create the hard disks & connect them to the appropriate controllers
    std::list<VirtualSystemDescriptionEntry*> avsdeHDs = vsdescThis->i_findByType(VirtualSystemDescriptionType_HardDiskImage);
    if (avsdeHDs.size() > 0)
    {
        // If there's an error here we need to close the session, so
        // we need another try/catch block.
        try
        {
#ifdef LOG_ENABLED
            if (LogIsEnabled())
            {
                size_t i = 0;
                for (list<VirtualSystemDescriptionEntry*>::const_iterator itHD = avsdeHDs.begin();
                     itHD != avsdeHDs.end(); ++itHD, i++)
                     Log(("avsdeHDs[%zu]: strRef=%s strOvf=%s\n", i, (*itHD)->strRef.c_str(), (*itHD)->strOvf.c_str()));
                i = 0;
                for (ovf::DiskImagesMap::const_iterator itDisk = stack.mapDisks.begin(); itDisk != stack.mapDisks.end(); ++itDisk)
                    Log(("mapDisks[%zu]: strDiskId=%s strHref=%s\n",
                         i, itDisk->second.strDiskId.c_str(), itDisk->second.strHref.c_str()));

            }
#endif

            // to attach things we need to open a session for the new machine
            rc = pNewMachine->LockMachine(stack.pSession, LockType_Write);
            if (FAILED(rc)) throw rc;
            stack.fSessionOpen = true;

            /* get VM name from virtual system description. Only one record is possible (size of list is equal 1). */
            std::list<VirtualSystemDescriptionEntry*> vmName = vsdescThis->i_findByType(VirtualSystemDescriptionType_Name);
            std::list<VirtualSystemDescriptionEntry*>::iterator vmNameIt = vmName.begin();
            VirtualSystemDescriptionEntry* vmNameEntry = *vmNameIt;


            ovf::DiskImagesMap::const_iterator oit = stack.mapDisks.begin();
            std::set<RTCString>  disksResolvedNames;

            uint32_t cImportedDisks = 0;

            while (oit != stack.mapDisks.end() && cImportedDisks != avsdeHDs.size())
            {
                ovf::DiskImage diCurrent = oit->second;
                ovf::VirtualDisksMap::const_iterator itVDisk = vsysThis.mapVirtualDisks.begin();

                VirtualSystemDescriptionEntry *vsdeTargetHD = 0;
                Log(("diCurrent.strDiskId=%s diCurrent.strHref=%s\n", diCurrent.strDiskId.c_str(), diCurrent.strHref.c_str()));

                /*
                 *
                 * Iterate over all given disk images of the virtual system
                 * disks description. We need to find the target disk path,
                 * which could be changed by the user.
                 *
                 */
                {
                    list<VirtualSystemDescriptionEntry*>::const_iterator itHD;
                    for (itHD = avsdeHDs.begin();
                         itHD != avsdeHDs.end();
                         ++itHD)
                    {
                        VirtualSystemDescriptionEntry *vsdeHD = *itHD;
                        if (vsdeHD->strRef == diCurrent.strDiskId)
                        {
                            vsdeTargetHD = vsdeHD;
                            break;
                        }
                    }
                    if (!vsdeTargetHD)
                    {
                        /* possible case if a disk image belongs to other virtual system (OVF package with multiple VMs inside) */
                        LogWarning(("OVA/OVF import: Disk image %s was missed during import of VM %s\n",
                                    oit->first.c_str(), vmNameEntry->strOvf.c_str()));
                        NOREF(vmNameEntry);
                        ++oit;
                        continue;
                    }

                    //diCurrent.strDiskId contains the disk identifier (e.g. "vmdisk1"), which should exist
                    //in the virtual system's disks map under that ID and also in the global images map
                    itVDisk = vsysThis.mapVirtualDisks.find(diCurrent.strDiskId);
                    if (itVDisk == vsysThis.mapVirtualDisks.end())
                        throw setError(E_FAIL,
                                       tr("Internal inconsistency looking up disk image '%s'"),
                                       diCurrent.strHref.c_str());
                }

                /*
                 * preliminary check availability of the image
                 * This step is useful if image is placed in the OVA (TAR) package
                 */

                Utf8Str name = i_applianceIOName(applianceIOTar);

                if (strncmp(pStorage->pVDImageIfaces->pszInterfaceName, name.c_str(), name.length()) == 0)
                {
                    /* It means that we possibly have imported the storage earlier on the previous loop steps*/
                    std::set<RTCString>::const_iterator h = disksResolvedNames.find(diCurrent.strHref);
                    if (h != disksResolvedNames.end())
                    {
                        /* Yes, disk name was found, we can skip it*/
                        ++oit;
                        continue;
                    }

                    RTCString availableImage(diCurrent.strHref);

                    rc = i_preCheckImageAvailability(pStorage, availableImage);

                    if (SUCCEEDED(rc))
                    {
                        /* current opened file isn't the same as passed one */
                        if (availableImage.compare(diCurrent.strHref, Utf8Str::CaseInsensitive) != 0)
                        {
                            /*
                             * availableImage contains the disk file reference (e.g. "disk1.vmdk"), which should exist
                             * in the global images map.
                             * And find the disk from the OVF's disk list
                             *
                             */
                            {
                                ovf::DiskImagesMap::const_iterator itDiskImage = stack.mapDisks.begin();
                                while (++itDiskImage != stack.mapDisks.end())
                                {
                                    if (itDiskImage->second.strHref.compare(availableImage, Utf8Str::CaseInsensitive) == 0)
                                        break;
                                }
                                if (itDiskImage == stack.mapDisks.end())
                                {
                                    throw setError(E_FAIL,
                                                   tr("Internal inconsistency looking up disk image '%s'. "
                                                      "Check compliance OVA package structure and file names "
                                                      "references in the section <References> in the OVF file."),
                                                   availableImage.c_str());
                                }

                                /* replace with a new found disk image */
                                diCurrent = *(&itDiskImage->second);
                            }

                            /*
                             * Again iterate over all given disk images of the virtual system
                             * disks description using the found disk image
                             */
                            {
                                list<VirtualSystemDescriptionEntry*>::const_iterator itHD;
                                for (itHD = avsdeHDs.begin();
                                     itHD != avsdeHDs.end();
                                     ++itHD)
                                {
                                    VirtualSystemDescriptionEntry *vsdeHD = *itHD;
                                    if (vsdeHD->strRef == diCurrent.strDiskId)
                                    {
                                        vsdeTargetHD = vsdeHD;
                                        break;
                                    }
                                }
                                if (!vsdeTargetHD)
                                {
                                    /*
                                     * in this case it's an error because something wrong with OVF description file.
                                     * May be VBox imports OVA package with wrong file sequence inside the archive.
                                     */
                                    throw setError(E_FAIL,
                                                   tr("Internal inconsistency looking up disk image '%s'"),
                                                   diCurrent.strHref.c_str());
                                }

                                itVDisk = vsysThis.mapVirtualDisks.find(diCurrent.strDiskId);
                                if (itVDisk == vsysThis.mapVirtualDisks.end())
                                    throw setError(E_FAIL,
                                                   tr("Internal inconsistency looking up disk image '%s'"),
                                                   diCurrent.strHref.c_str());
                            }
                        }
                        else
                        {
                            ++oit;
                        }
                    }
                    else
                    {
                        ++oit;
                        continue;
                    }
                }
                else
                {
                    /* just continue with normal files*/
                    ++oit;
                }

                const ovf::VirtualDisk &ovfVdisk = itVDisk->second;

                /* very important to store disk name for the next checks */
                disksResolvedNames.insert(diCurrent.strHref);

                ComObjPtr<Medium> pTargetHD;

                Utf8Str savedVBoxCurrent = vsdeTargetHD->strVBoxCurrent;

                i_importOneDiskImage(diCurrent,
                                     &vsdeTargetHD->strVBoxCurrent,
                                     pTargetHD,
                                     stack,
                                     pCallbacks,
                                     pStorage);

                // now use the new uuid to attach the disk image to our new machine
                ComPtr<IMachine> sMachine;
                rc = stack.pSession->COMGETTER(Machine)(sMachine.asOutParam());
                if (FAILED(rc))
                    throw rc;

                // find the hard disk controller to which we should attach
                ovf::HardDiskController hdc = (*vsysThis.mapControllers.find(ovfVdisk.idController)).second;

                // this is for rollback later
                MyHardDiskAttachment mhda;
                mhda.pMachine = pNewMachine;

                i_convertDiskAttachmentValues(hdc,
                                              ovfVdisk.ulAddressOnParent,
                                              mhda.controllerType,        // Bstr
                                              mhda.lControllerPort,
                                              mhda.lDevice);

                Log(("Attaching disk %s to port %d on device %d\n",
                vsdeTargetHD->strVBoxCurrent.c_str(), mhda.lControllerPort, mhda.lDevice));

                ComObjPtr<MediumFormat> mediumFormat;
                rc = i_findMediumFormatFromDiskImage(diCurrent, mediumFormat);
                if (FAILED(rc))
                    throw rc;

                Bstr bstrFormatName;
                rc = mediumFormat->COMGETTER(Name)(bstrFormatName.asOutParam());
                if (FAILED(rc))
                    throw rc;

                Utf8Str vdf = Utf8Str(bstrFormatName);

                if (vdf.compare("RAW", Utf8Str::CaseInsensitive) == 0)
                {
                    ComPtr<IMedium> dvdImage(pTargetHD);

                    rc = mVirtualBox->OpenMedium(Bstr(vsdeTargetHD->strVBoxCurrent).raw(),
                                                 DeviceType_DVD,
                                                 AccessMode_ReadWrite,
                                                 false,
                                                 dvdImage.asOutParam());

                    if (FAILED(rc))
                        throw rc;

                    rc = sMachine->AttachDevice(mhda.controllerType.raw(),// wstring name
                                                mhda.lControllerPort,     // long controllerPort
                                                mhda.lDevice,             // long device
                                                DeviceType_DVD,           // DeviceType_T type
                                                dvdImage);
                    if (FAILED(rc))
                        throw rc;
                }
                else
                {
                    rc = sMachine->AttachDevice(mhda.controllerType.raw(),// wstring name
                                                mhda.lControllerPort,     // long controllerPort
                                                mhda.lDevice,             // long device
                                                DeviceType_HardDisk,      // DeviceType_T type
                                                pTargetHD);

                    if (FAILED(rc))
                        throw rc;
                }

                stack.llHardDiskAttachments.push_back(mhda);

                rc = sMachine->SaveSettings();
                if (FAILED(rc))
                    throw rc;

                /* restore */
                vsdeTargetHD->strVBoxCurrent = savedVBoxCurrent;

                ++cImportedDisks;

            } // end while(oit != stack.mapDisks.end())

            /*
             * quantity of the imported disks isn't equal to the size of the avsdeHDs list.
             */
            if(cImportedDisks < avsdeHDs.size())
            {
                LogWarning(("Not all disk images were imported for VM %s. Check OVF description file.",
                            vmNameEntry->strOvf.c_str()));
            }

            // only now that we're done with all disks, close the session
            rc = stack.pSession->UnlockMachine();
            if (FAILED(rc))
                throw rc;
            stack.fSessionOpen = false;
        }
        catch(HRESULT aRC)
        {
            com::ErrorInfo info;
            if (stack.fSessionOpen)
                stack.pSession->UnlockMachine();

            if (info.isFullAvailable())
                throw setError(aRC, Utf8Str(info.getText()).c_str());
            else
                throw setError(aRC, "Unknown error during OVF import");
        }
    }
    LogFlowFuncLeave();
}

/**
 * Imports one OVF virtual system (described by a vbox:Machine tag represented by the given config
 * structure) into VirtualBox by creating an IMachine instance, which is returned.
 *
 * This throws HRESULT error codes for anything that goes wrong, in which case the caller must clean
 * up any leftovers from this function. For this, the given ImportStack instance has received information
 * about what needs cleaning up (to support rollback).
 *
 * The machine config stored in the settings::MachineConfigFile structure contains the UUIDs of
 * the disk attachments used by the machine when it was exported. We also add vbox:uuid attributes
 * to the OVF disks sections so we can look them up. While importing these UUIDs into a second host
 * will most probably work, reimporting them into the same host will cause conflicts, so we always
 * generate new ones on import. This involves the following:
 *
 *  1)  Scan the machine config for disk attachments.
 *
 *  2)  For each disk attachment found, look up the OVF disk image from the disk references section
 *      and import the disk into VirtualBox, which creates a new UUID for it. In the machine config,
 *      replace the old UUID with the new one.
 *
 *  3)  Change the machine config according to the OVF virtual system descriptions, in case the
 *      caller has modified them using setFinalValues().
 *
 *  4)  Create the VirtualBox machine with the modfified machine config.
 *
 * @param config
 * @param pNewMachine
 * @param stack
 */
void Appliance::i_importVBoxMachine(ComObjPtr<VirtualSystemDescription> &vsdescThis,
                                    ComPtr<IMachine> &pReturnNewMachine,
                                    ImportStack &stack,
                                    PVDINTERFACEIO pCallbacks,
                                    PSHASTORAGE pStorage)
{
    LogFlowFuncEnter();
    Assert(vsdescThis->m->pConfig);

    HRESULT rc = S_OK;

    settings::MachineConfigFile &config = *vsdescThis->m->pConfig;

    /*
     * step 1): modify machine config according to OVF config, in case the user
     * has modified them using setFinalValues()
     */

    /* OS Type */
    config.machineUserData.strOsType = stack.strOsTypeVBox;
    /* Description */
    config.machineUserData.strDescription = stack.strDescription;
    /* CPU count & extented attributes */
    config.hardwareMachine.cCPUs = stack.cCPUs;
    if (stack.fForceIOAPIC)
        config.hardwareMachine.fHardwareVirt = true;
    if (stack.fForceIOAPIC)
        config.hardwareMachine.biosSettings.fIOAPICEnabled = true;
    /* RAM size */
    config.hardwareMachine.ulMemorySizeMB = stack.ulMemorySizeMB;

/*
    <const name="HardDiskControllerIDE" value="14" />
    <const name="HardDiskControllerSATA" value="15" />
    <const name="HardDiskControllerSCSI" value="16" />
    <const name="HardDiskControllerSAS" value="17" />
*/

#ifdef VBOX_WITH_USB
    /* USB controller */
    if (stack.fUSBEnabled)
    {
        /** @todo r=klaus add support for arbitrary USB controller types, this can't handle
         *  multiple controllers due to its design anyway */
        /* usually the OHCI controller is enabled already, need to check */
        bool fOHCIEnabled = false;
        settings::USBControllerList &llUSBControllers = config.hardwareMachine.usbSettings.llUSBControllers;
        settings::USBControllerList::iterator it;
        for (it = llUSBControllers.begin(); it != llUSBControllers.end(); ++it)
        {
            if (it->enmType == USBControllerType_OHCI)
            {
                fOHCIEnabled = true;
                break;
            }
        }

        if (!fOHCIEnabled)
        {
            settings::USBController ctrl;
            ctrl.strName = "OHCI";
            ctrl.enmType = USBControllerType_OHCI;

            llUSBControllers.push_back(ctrl);
        }
    }
    else
        config.hardwareMachine.usbSettings.llUSBControllers.clear();
#endif
    /* Audio adapter */
    if (stack.strAudioAdapter.isNotEmpty())
    {
        config.hardwareMachine.audioAdapter.fEnabled = true;
        config.hardwareMachine.audioAdapter.controllerType = (AudioControllerType_T)stack.strAudioAdapter.toUInt32();
    }
    else
        config.hardwareMachine.audioAdapter.fEnabled = false;
    /* Network adapter */
    settings::NetworkAdaptersList &llNetworkAdapters = config.hardwareMachine.llNetworkAdapters;
    /* First disable all network cards, they will be enabled below again. */
    settings::NetworkAdaptersList::iterator it1;
    bool fKeepAllMACs = m->optListImport.contains(ImportOptions_KeepAllMACs);
    bool fKeepNATMACs = m->optListImport.contains(ImportOptions_KeepNATMACs);
    for (it1 = llNetworkAdapters.begin(); it1 != llNetworkAdapters.end(); ++it1)
    {
        it1->fEnabled = false;
        if (!(   fKeepAllMACs
              || (fKeepNATMACs && it1->mode == NetworkAttachmentType_NAT)
              || (fKeepNATMACs && it1->mode == NetworkAttachmentType_NATNetwork)))
            Host::i_generateMACAddress(it1->strMACAddress);
    }
    /* Now iterate over all network entries. */
    std::list<VirtualSystemDescriptionEntry*> avsdeNWs = vsdescThis->i_findByType(VirtualSystemDescriptionType_NetworkAdapter);
    if (avsdeNWs.size() > 0)
    {
        /* Iterate through all network adapter entries and search for the
         * corresponding one in the machine config. If one is found, configure
         * it based on the user settings. */
        list<VirtualSystemDescriptionEntry*>::const_iterator itNW;
        for (itNW = avsdeNWs.begin();
             itNW != avsdeNWs.end();
             ++itNW)
        {
            VirtualSystemDescriptionEntry *vsdeNW = *itNW;
            if (   vsdeNW->strExtraConfigCurrent.startsWith("slot=", Utf8Str::CaseInsensitive)
                && vsdeNW->strExtraConfigCurrent.length() > 6)
            {
                uint32_t iSlot = vsdeNW->strExtraConfigCurrent.substr(5, 1).toUInt32();
                /* Iterate through all network adapters in the machine config. */
                for (it1 = llNetworkAdapters.begin();
                     it1 != llNetworkAdapters.end();
                     ++it1)
                {
                    /* Compare the slots. */
                    if (it1->ulSlot == iSlot)
                    {
                        it1->fEnabled = true;
                        it1->type = (NetworkAdapterType_T)vsdeNW->strVBoxCurrent.toUInt32();
                        break;
                    }
                }
            }
        }
    }

    /* Floppy controller */
    bool fFloppy = vsdescThis->i_findByType(VirtualSystemDescriptionType_Floppy).size() > 0;
    /* DVD controller */
    bool fDVD = vsdescThis->i_findByType(VirtualSystemDescriptionType_CDROM).size() > 0;
    /* Iterate over all storage controller check the attachments and remove
     * them when necessary. Also detect broken configs with more than one
     * attachment. Old VirtualBox versions (prior to 3.2.10) had all disk
     * attachments pointing to the last hard disk image, which causes import
     * failures. A long fixed bug, however the OVF files are long lived. */
    settings::StorageControllersList &llControllers = config.storageMachine.llStorageControllers;
    Guid hdUuid;
    uint32_t cDisks = 0;
    bool fInconsistent = false;
    bool fRepairDuplicate = false;
    settings::StorageControllersList::iterator it3;
    for (it3 = llControllers.begin();
         it3 != llControllers.end();
         ++it3)
    {
        settings::AttachedDevicesList &llAttachments = it3->llAttachedDevices;
        settings::AttachedDevicesList::iterator it4 = llAttachments.begin();
        while (it4 != llAttachments.end())
        {
            if (  (   !fDVD
                   && it4->deviceType == DeviceType_DVD)
                ||
                  (   !fFloppy
                   && it4->deviceType == DeviceType_Floppy))
            {
                it4 = llAttachments.erase(it4);
                continue;
            }
            else if (it4->deviceType == DeviceType_HardDisk)
            {
                const Guid &thisUuid = it4->uuid;
                cDisks++;
                if (cDisks == 1)
                {
                    if (hdUuid.isZero())
                        hdUuid = thisUuid;
                    else
                        fInconsistent = true;
                }
                else
                {
                   if (thisUuid.isZero())
                        fInconsistent = true;
                    else if (thisUuid == hdUuid)
                        fRepairDuplicate = true;
                }
            }
            ++it4;
        }
    }
    /* paranoia... */
    if (fInconsistent || cDisks == 1)
        fRepairDuplicate = false;

    /*
     * step 2: scan the machine config for media attachments
     */
    /* get VM name from virtual system description. Only one record is possible (size of list is equal 1). */
    std::list<VirtualSystemDescriptionEntry*> vmName = vsdescThis->i_findByType(VirtualSystemDescriptionType_Name);
    std::list<VirtualSystemDescriptionEntry*>::iterator vmNameIt = vmName.begin();
    VirtualSystemDescriptionEntry* vmNameEntry = *vmNameIt;

    /* Get all hard disk descriptions. */
    std::list<VirtualSystemDescriptionEntry*> avsdeHDs = vsdescThis->i_findByType(VirtualSystemDescriptionType_HardDiskImage);
    std::list<VirtualSystemDescriptionEntry*>::iterator avsdeHDsIt = avsdeHDs.begin();
    /* paranoia - if there is no 1:1 match do not try to repair. */
    if (cDisks != avsdeHDs.size())
        fRepairDuplicate = false;

    // there must be an image in the OVF disk structs with the same UUID

    ovf::DiskImagesMap::const_iterator oit = stack.mapDisks.begin();
    std::set<RTCString>  disksResolvedNames;

    uint32_t cImportedDisks = 0;

    while(oit != stack.mapDisks.end() && cImportedDisks != avsdeHDs.size())
    {
        ovf::DiskImage diCurrent = oit->second;

        VirtualSystemDescriptionEntry *vsdeTargetHD = 0;

        {
            /* Iterate over all given disk images of the virtual system
             * disks description. We need to find the target disk path,
             * which could be changed by the user. */
            list<VirtualSystemDescriptionEntry*>::const_iterator itHD;
            for (itHD = avsdeHDs.begin();
                 itHD != avsdeHDs.end();
                 ++itHD)
            {
                VirtualSystemDescriptionEntry *vsdeHD = *itHD;
                if (vsdeHD->strRef == oit->first)
                {
                    vsdeTargetHD = vsdeHD;
                    break;
                }
            }
            if (!vsdeTargetHD)
            {
                /* possible case if a disk image belongs to other virtual system (OVF package with multiple VMs inside) */
                LogWarning(("OVA/OVF import: Disk image %s was missed during import of VM %s\n",
                            oit->first.c_str(), vmNameEntry->strOvf.c_str()));
                NOREF(vmNameEntry);
                ++oit;
                continue;
            }
        }

        /*
         * preliminary check availability of the image
         * This step is useful if image is placed in the OVA (TAR) package
         */

        Utf8Str name = i_applianceIOName(applianceIOTar);

        if (strncmp(pStorage->pVDImageIfaces->pszInterfaceName, name.c_str(), name.length()) == 0)
        {
            /* It means that we possibly have imported the storage earlier on the previous loop steps*/
            std::set<RTCString>::const_iterator h = disksResolvedNames.find(diCurrent.strHref);
            if (h != disksResolvedNames.end())
            {
                /* Yes, disk name was found, we can skip it*/
                ++oit;
                continue;
            }

            RTCString availableImage(diCurrent.strHref);

            rc = i_preCheckImageAvailability(pStorage, availableImage);

            if (SUCCEEDED(rc))
            {
                /* current opened file isn't the same as passed one */
                if(availableImage.compare(diCurrent.strHref, Utf8Str::CaseInsensitive) != 0)
                {
                    // availableImage contains the disk identifier (e.g. "vmdisk1"), which should exist
                    // in the virtual system's disks map under that ID and also in the global images map
                    // and find the disk from the OVF's disk list
                    ovf::DiskImagesMap::const_iterator itDiskImage = stack.mapDisks.begin();
                    while (++itDiskImage != stack.mapDisks.end())
                    {
                        if(itDiskImage->second.strHref.compare(availableImage, Utf8Str::CaseInsensitive) == 0 )
                            break;
                    }
                    if (itDiskImage == stack.mapDisks.end())
                    {
                        throw setError(E_FAIL,
                                       tr("Internal inconsistency looking up disk image '%s'. "
                                          "Check compliance OVA package structure and file names "
                                          "references in the section <References> in the OVF file."),
                                       availableImage.c_str());
                    }

                    /* replace with a new found disk image */
                    diCurrent = *(&itDiskImage->second);

                    /*
                     * Again iterate over all given disk images of the virtual system
                     * disks description using the found disk image
                     */
                    list<VirtualSystemDescriptionEntry*>::const_iterator itHD;
                    for (itHD = avsdeHDs.begin();
                         itHD != avsdeHDs.end();
                         ++itHD)
                    {
                        VirtualSystemDescriptionEntry *vsdeHD = *itHD;
                        if (vsdeHD->strRef == diCurrent.strDiskId)
                        {
                            vsdeTargetHD = vsdeHD;
                            break;
                        }
                    }
                    if (!vsdeTargetHD)
                        /*
                         * in this case it's an error because something wrong with OVF description file.
                         * May be VBox imports OVA package with wrong file sequence inside the archive.
                         */
                        throw setError(E_FAIL,
                                       tr("Internal inconsistency looking up disk image '%s'"),
                                       diCurrent.strHref.c_str());
                }
                else
                {
                    ++oit;
                }
            }
            else
            {
                ++oit;
                continue;
            }
        }
        else
        {
            /* just continue with normal files*/
            ++oit;
        }

        /* Important! to store disk name for the next checks */
        disksResolvedNames.insert(diCurrent.strHref);

        // there must be an image in the OVF disk structs with the same UUID
        bool fFound = false;
        Utf8Str strUuid;

        // for each storage controller...
        for (settings::StorageControllersList::iterator sit = config.storageMachine.llStorageControllers.begin();
             sit != config.storageMachine.llStorageControllers.end();
             ++sit)
        {
            settings::StorageController &sc = *sit;

            // find the OVF virtual system description entry for this storage controller
            switch (sc.storageBus)
            {
                case StorageBus_SATA:
                    break;
                case StorageBus_SCSI:
                    break;
                case StorageBus_IDE:
                    break;
                case StorageBus_SAS:
                    break;
            }

            // for each medium attachment to this controller...
            for (settings::AttachedDevicesList::iterator dit = sc.llAttachedDevices.begin();
                 dit != sc.llAttachedDevices.end();
                 ++dit)
            {
                settings::AttachedDevice &d = *dit;

                if (d.uuid.isZero())
                    // empty DVD and floppy media
                    continue;

                // When repairing a broken VirtualBox xml config section (written
                // by VirtualBox versions earlier than 3.2.10) assume the disks
                // show up in the same order as in the OVF description.
                if (fRepairDuplicate)
                {
                    VirtualSystemDescriptionEntry *vsdeHD = *avsdeHDsIt;
                    ovf::DiskImagesMap::const_iterator itDiskImage = stack.mapDisks.find(vsdeHD->strRef);
                    if (itDiskImage != stack.mapDisks.end())
                    {
                        const ovf::DiskImage &di = itDiskImage->second;
                        d.uuid = Guid(di.uuidVBox);
                    }
                    ++avsdeHDsIt;
                }

                // convert the Guid to string
                strUuid = d.uuid.toString();

                if (diCurrent.uuidVBox != strUuid)
                {
                    continue;
                }

                /*
                 * step 3: import disk
                 */
                Utf8Str savedVBoxCurrent = vsdeTargetHD->strVBoxCurrent;
                ComObjPtr<Medium> pTargetHD;

                i_importOneDiskImage(diCurrent,
                                     &vsdeTargetHD->strVBoxCurrent,
                                     pTargetHD,
                                     stack,
                                     pCallbacks,
                                     pStorage);

                Bstr hdId;

                ComObjPtr<MediumFormat> mediumFormat;
                rc = i_findMediumFormatFromDiskImage(diCurrent, mediumFormat);
                if (FAILED(rc))
                    throw rc;

                Bstr bstrFormatName;
                rc = mediumFormat->COMGETTER(Name)(bstrFormatName.asOutParam());
                if (FAILED(rc))
                    throw rc;

                Utf8Str vdf = Utf8Str(bstrFormatName);

                if (vdf.compare("RAW", Utf8Str::CaseInsensitive) == 0)
                {
                    ComPtr<IMedium> dvdImage(pTargetHD);

                    rc = mVirtualBox->OpenMedium(Bstr(vsdeTargetHD->strVBoxCurrent).raw(),
                                                 DeviceType_DVD,
                                                 AccessMode_ReadWrite,
                                                 false,
                                                 dvdImage.asOutParam());

                    if (FAILED(rc)) throw rc;

                    // ... and replace the old UUID in the machine config with the one of
                    // the imported disk that was just created
                    rc = dvdImage->COMGETTER(Id)(hdId.asOutParam());
                    if (FAILED(rc)) throw rc;
                }
                else
                {
                    // ... and replace the old UUID in the machine config with the one of
                    // the imported disk that was just created
                    rc = pTargetHD->COMGETTER(Id)(hdId.asOutParam());
                    if (FAILED(rc)) throw rc;
                }

                /* restore */
                vsdeTargetHD->strVBoxCurrent = savedVBoxCurrent;

                /*
                 * 1. saving original UUID for restoring in case of failure.
                 * 2. replacement of original UUID by new UUID in the current VM config (settings::MachineConfigFile).
                 */
                {
                    rc = stack.saveOriginalUUIDOfAttachedDevice(d, Utf8Str(hdId));
                    d.uuid = hdId;
                }

                fFound = true;
                break;
            } // for (settings::AttachedDevicesList::const_iterator dit = sc.llAttachedDevices.begin();
        } // for (settings::StorageControllersList::const_iterator sit = config.storageMachine.llStorageControllers.begin();

            // no disk with such a UUID found:
        if (!fFound)
            throw setError(E_FAIL,
                           tr("<vbox:Machine> element in OVF contains a medium attachment for the disk image %s "
                              "but the OVF describes no such image"),
                           strUuid.c_str());

        ++cImportedDisks;

    }// while(oit != stack.mapDisks.end())


    /*
     * quantity of the imported disks isn't equal to the size of the avsdeHDs list.
     */
    if(cImportedDisks < avsdeHDs.size())
    {
        LogWarning(("Not all disk images were imported for VM %s. Check OVF description file.",
                    vmNameEntry->strOvf.c_str()));
    }

    /*
     * step 4): create the machine and have it import the config
     */

    ComObjPtr<Machine> pNewMachine;
    rc = pNewMachine.createObject();
    if (FAILED(rc)) throw rc;

    // this magic constructor fills the new machine object with the MachineConfig
    // instance that we created from the vbox:Machine
    rc = pNewMachine->init(mVirtualBox,
                           stack.strNameVBox,// name from OVF preparations; can be suffixed to avoid duplicates
                           config);          // the whole machine config
    if (FAILED(rc)) throw rc;

    pReturnNewMachine = ComPtr<IMachine>(pNewMachine);

    // and register it
    rc = mVirtualBox->RegisterMachine(pNewMachine);
    if (FAILED(rc)) throw rc;

    // store new machine for roll-back in case of errors
    Bstr bstrNewMachineId;
    rc = pNewMachine->COMGETTER(Id)(bstrNewMachineId.asOutParam());
    if (FAILED(rc)) throw rc;
    m->llGuidsMachinesCreated.push_back(Guid(bstrNewMachineId));

    LogFlowFuncLeave();
}

void Appliance::i_importMachines(ImportStack &stack,
                                 PVDINTERFACEIO pCallbacks,
                                 PSHASTORAGE pStorage)
{
    HRESULT rc = S_OK;

    // this is safe to access because this thread only gets started
    const ovf::OVFReader &reader = *m->pReader;

    /*
     * get the SHA digest version that was set in accordance with the value of attribute "xmlns:ovf"
     * of the element <Envelope> in the OVF file during reading operation. See readFSImpl().
     */
    pStorage->fSha256 = m->fSha256;

    // create a session for the machine + disks we manipulate below
    rc = stack.pSession.createInprocObject(CLSID_Session);
    if (FAILED(rc)) throw rc;

    list<ovf::VirtualSystem>::const_iterator it;
    list< ComObjPtr<VirtualSystemDescription> >::const_iterator it1;
    /* Iterate through all virtual systems of that appliance */
    size_t i = 0;
    for (it  = reader.m_llVirtualSystems.begin(), it1  = m->virtualSystemDescriptions.begin();
         it != reader.m_llVirtualSystems.end() && it1 != m->virtualSystemDescriptions.end();
         ++it, ++it1, ++i)
    {
        const ovf::VirtualSystem &vsysThis = *it;
        ComObjPtr<VirtualSystemDescription> vsdescThis = (*it1);

        ComPtr<IMachine> pNewMachine;

        // there are two ways in which we can create a vbox machine from OVF:
        // -- either this OVF was written by vbox 3.2 or later, in which case there is a <vbox:Machine> element
        //    in the <VirtualSystem>; then the VirtualSystemDescription::Data has a settings::MachineConfigFile
        //    with all the machine config pretty-parsed;
        // -- or this is an OVF from an older vbox or an external source, and then we need to translate the
        //    VirtualSystemDescriptionEntry and do import work

        // Even for the vbox:Machine case, there are a number of configuration items that will be taken from
        // the OVF because otherwise the "override import parameters" mechanism in the GUI won't work.

        // VM name
        std::list<VirtualSystemDescriptionEntry*> vsdeName = vsdescThis->i_findByType(VirtualSystemDescriptionType_Name);
        if (vsdeName.size() < 1)
            throw setError(VBOX_E_FILE_ERROR,
                           tr("Missing VM name"));
        stack.strNameVBox = vsdeName.front()->strVBoxCurrent;

        // have VirtualBox suggest where the filename would be placed so we can
        // put the disk images in the same directory
        Bstr bstrMachineFilename;
        rc = mVirtualBox->ComposeMachineFilename(Bstr(stack.strNameVBox).raw(),
                                                 NULL /* aGroup */,
                                                 NULL /* aCreateFlags */,
                                                 NULL /* aBaseFolder */,
                                                 bstrMachineFilename.asOutParam());
        if (FAILED(rc)) throw rc;
        // and determine the machine folder from that
        stack.strMachineFolder = bstrMachineFilename;
        stack.strMachineFolder.stripFilename();
        LogFunc(("i=%zu strName=%s bstrMachineFilename=%ls\n", i, stack.strNameVBox.c_str(), bstrMachineFilename.raw()));

        // guest OS type
        std::list<VirtualSystemDescriptionEntry*> vsdeOS;
        vsdeOS = vsdescThis->i_findByType(VirtualSystemDescriptionType_OS);
        if (vsdeOS.size() < 1)
            throw setError(VBOX_E_FILE_ERROR,
                           tr("Missing guest OS type"));
        stack.strOsTypeVBox = vsdeOS.front()->strVBoxCurrent;

        // CPU count
        std::list<VirtualSystemDescriptionEntry*> vsdeCPU = vsdescThis->i_findByType(VirtualSystemDescriptionType_CPU);
        if (vsdeCPU.size() != 1)
            throw setError(VBOX_E_FILE_ERROR, tr("CPU count missing"));

        stack.cCPUs = vsdeCPU.front()->strVBoxCurrent.toUInt32();
        // We need HWVirt & IO-APIC if more than one CPU is requested
        if (stack.cCPUs > 1)
        {
            stack.fForceHWVirt = true;
            stack.fForceIOAPIC = true;
        }

        // RAM
        std::list<VirtualSystemDescriptionEntry*> vsdeRAM = vsdescThis->i_findByType(VirtualSystemDescriptionType_Memory);
        if (vsdeRAM.size() != 1)
            throw setError(VBOX_E_FILE_ERROR, tr("RAM size missing"));
        stack.ulMemorySizeMB = (ULONG)vsdeRAM.front()->strVBoxCurrent.toUInt64();

#ifdef VBOX_WITH_USB
        // USB controller
        std::list<VirtualSystemDescriptionEntry*> vsdeUSBController =
            vsdescThis->i_findByType(VirtualSystemDescriptionType_USBController);
        // USB support is enabled if there's at least one such entry; to disable USB support,
        // the type of the USB item would have been changed to "ignore"
        stack.fUSBEnabled = vsdeUSBController.size() > 0;
#endif
        // audio adapter
        std::list<VirtualSystemDescriptionEntry*> vsdeAudioAdapter =
            vsdescThis->i_findByType(VirtualSystemDescriptionType_SoundCard);
        /* @todo: we support one audio adapter only */
        if (vsdeAudioAdapter.size() > 0)
            stack.strAudioAdapter = vsdeAudioAdapter.front()->strVBoxCurrent;

        // for the description of the new machine, always use the OVF entry, the user may have changed it in the import config
        std::list<VirtualSystemDescriptionEntry*> vsdeDescription =
            vsdescThis->i_findByType(VirtualSystemDescriptionType_Description);
        if (vsdeDescription.size())
            stack.strDescription = vsdeDescription.front()->strVBoxCurrent;

        // import vbox:machine or OVF now
        if (vsdescThis->m->pConfig)
            // vbox:Machine config
            i_importVBoxMachine(vsdescThis, pNewMachine, stack, pCallbacks, pStorage);
        else
            // generic OVF config
            i_importMachineGeneric(vsysThis, vsdescThis, pNewMachine, stack, pCallbacks, pStorage);

    } // for (it = pAppliance->m->llVirtualSystems.begin() ...
}

HRESULT Appliance::ImportStack::saveOriginalUUIDOfAttachedDevice(settings::AttachedDevice &device,
                                                     const Utf8Str &newlyUuid)
{
    HRESULT rc = S_OK;

    /* save for restoring */
    mapNewUUIDsToOriginalUUIDs.insert(std::make_pair(newlyUuid, device.uuid.toString()));

    return rc;
}

HRESULT Appliance::ImportStack::restoreOriginalUUIDOfAttachedDevice(settings::MachineConfigFile *config)
{
    HRESULT rc = S_OK;

    settings::StorageControllersList &llControllers = config->storageMachine.llStorageControllers;
    settings::StorageControllersList::iterator itscl;
    for (itscl = llControllers.begin();
         itscl != llControllers.end();
         ++itscl)
    {
        settings::AttachedDevicesList &llAttachments = itscl->llAttachedDevices;
        settings::AttachedDevicesList::iterator itadl = llAttachments.begin();
        while (itadl != llAttachments.end())
        {
            std::map<Utf8Str , Utf8Str>::iterator it =
                mapNewUUIDsToOriginalUUIDs.find(itadl->uuid.toString());
            if(it!=mapNewUUIDsToOriginalUUIDs.end())
            {
                Utf8Str uuidOriginal = it->second;
                itadl->uuid = Guid(uuidOriginal);
                mapNewUUIDsToOriginalUUIDs.erase(it->first);
            }
            ++itadl;
        }
    }

    return rc;
}

