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
#include <iprt/dir.h>
#include <iprt/param.h>
#include <iprt/s3.h>
#include <iprt/manifest.h>
#include <iprt/tar.h>
#include <iprt/stream.h>

#include <VBox/version.h>

#include "ApplianceImpl.h"
#include "VirtualBoxImpl.h"

#include "ProgressImpl.h"
#include "MachineImpl.h"
#include "MediumImpl.h"
#include "MediumFormatImpl.h"
#include "Global.h"
#include "SystemPropertiesImpl.h"

#include "AutoCaller.h"
#include "Logging.h"

#include "ApplianceImplPrivate.h"

using namespace std;

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
STDMETHODIMP Machine::Export(IAppliance *aAppliance, IN_BSTR location, IVirtualSystemDescription **aDescription)
{
    HRESULT rc = S_OK;

    if (!aAppliance)
        return E_POINTER;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ComObjPtr<VirtualSystemDescription> pNewDesc;

    try
    {
        Appliance *pAppliance = static_cast<Appliance*>(aAppliance);
        AutoCaller autoCaller1(pAppliance);
        if (FAILED(autoCaller1.rc())) return autoCaller1.rc();

        LocationInfo locInfo;
        parseURI(location, locInfo);
        // create a new virtual system to store in the appliance
        rc = pNewDesc.createObject();
        if (FAILED(rc)) throw rc;
        rc = pNewDesc->init();
        if (FAILED(rc)) throw rc;

        // store the machine object so we can dump the XML in Appliance::Write()
        pNewDesc->m->pMachine = this;

        // now fill it with description items
        Bstr bstrName1;
        Bstr bstrDescription;
        Bstr bstrGuestOSType;
        uint32_t cCPUs;
        uint32_t ulMemSizeMB;
        BOOL fUSBEnabled;
        BOOL fAudioEnabled;
        AudioControllerType_T audioController;

        ComPtr<IUSBController> pUsbController;
        ComPtr<IAudioAdapter> pAudioAdapter;

        // first, call the COM methods, as they request locks
        rc = COMGETTER(USBController)(pUsbController.asOutParam());
        if (FAILED(rc))
            fUSBEnabled = false;
        else
            rc = pUsbController->COMGETTER(Enabled)(&fUSBEnabled);

        // request the machine lock while accessing internal members
        AutoReadLock alock1(this COMMA_LOCKVAL_SRC_POS);

        pAudioAdapter = mAudioAdapter;
        rc = pAudioAdapter->COMGETTER(Enabled)(&fAudioEnabled);
        if (FAILED(rc)) throw rc;
        rc = pAudioAdapter->COMGETTER(AudioController)(&audioController);
        if (FAILED(rc)) throw rc;

        // get name
        Utf8Str strVMName = mUserData->s.strName;
        // get description
        Utf8Str strDescription = mUserData->s.strDescription;
        // get guest OS
        Utf8Str strOsTypeVBox = mUserData->s.strOsType;
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

        /* Guest OS type */
        ovf::CIMOSType_T cim = convertVBoxOSType2CIMOSType(strOsTypeVBox.c_str());
        pNewDesc->addEntry(VirtualSystemDescriptionType_OS,
                           "",
                           Utf8StrFmt("%RI32", cim),
                           strOsTypeVBox);

        /* VM name */
        pNewDesc->addEntry(VirtualSystemDescriptionType_Name,
                           "",
                           strVMName,
                           strVMName);

        // description
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
        Utf8Str strMemory = Utf8StrFmt("%RI64", (uint64_t)ulMemSizeMB * _1M);
        pNewDesc->addEntry(VirtualSystemDescriptionType_Memory,
                           "",
                           strMemory,
                           strMemory);

        // the one VirtualBox IDE controller has two channels with two ports each, which is
        // considered two IDE controllers with two ports each by OVF, so export it as two
        int32_t lIDEControllerPrimaryIndex = 0;
        int32_t lIDEControllerSecondaryIndex = 0;
        int32_t lSATAControllerIndex = 0;
        int32_t lSCSIControllerIndex = 0;

        /* Fetch all available storage controllers */
        com::SafeIfaceArray<IStorageController> nwControllers;
        rc = COMGETTER(StorageControllers)(ComSafeArrayAsOutParam(nwControllers));
        if (FAILED(rc)) throw rc;

        ComPtr<IStorageController> pIDEController;
        ComPtr<IStorageController> pSATAController;
        ComPtr<IStorageController> pSCSIController;
        ComPtr<IStorageController> pSASController;
        for (size_t j = 0; j < nwControllers.size(); ++j)
        {
            StorageBus_T eType;
            rc = nwControllers[j]->COMGETTER(Bus)(&eType);
            if (FAILED(rc)) throw rc;
            if (   eType == StorageBus_IDE
                && pIDEController.isNull())
                pIDEController = nwControllers[j];
            else if (   eType == StorageBus_SATA
                     && pSATAController.isNull())
                pSATAController = nwControllers[j];
            else if (   eType == StorageBus_SCSI
                     && pSATAController.isNull())
                pSCSIController = nwControllers[j];
            else if (   eType == StorageBus_SAS
                     && pSASController.isNull())
                pSASController = nwControllers[j];
        }

//     <const name="HardDiskControllerIDE" value="6" />
        if (!pIDEController.isNull())
        {
            Utf8Str strVbox;
            StorageControllerType_T ctlr;
            rc = pIDEController->COMGETTER(ControllerType)(&ctlr);
            if (FAILED(rc)) throw rc;
            switch(ctlr)
            {
                case StorageControllerType_PIIX3: strVbox = "PIIX3"; break;
                case StorageControllerType_PIIX4: strVbox = "PIIX4"; break;
                case StorageControllerType_ICH6: strVbox = "ICH6"; break;
            }

            if (strVbox.length())
            {
                lIDEControllerPrimaryIndex = (int32_t)pNewDesc->m->llDescriptions.size();
                pNewDesc->addEntry(VirtualSystemDescriptionType_HardDiskControllerIDE,
                                   Utf8StrFmt("%d", lIDEControllerPrimaryIndex),        // strRef
                                   strVbox,     // aOvfValue
                                   strVbox);    // aVboxValue
                lIDEControllerSecondaryIndex = lIDEControllerPrimaryIndex + 1;
                pNewDesc->addEntry(VirtualSystemDescriptionType_HardDiskControllerIDE,
                                   Utf8StrFmt("%d", lIDEControllerSecondaryIndex),
                                   strVbox,
                                   strVbox);
            }
        }

//     <const name="HardDiskControllerSATA" value="7" />
        if (!pSATAController.isNull())
        {
            Utf8Str strVbox = "AHCI";
            lSATAControllerIndex = (int32_t)pNewDesc->m->llDescriptions.size();
            pNewDesc->addEntry(VirtualSystemDescriptionType_HardDiskControllerSATA,
                               Utf8StrFmt("%d", lSATAControllerIndex),
                               strVbox,
                               strVbox);
        }

//     <const name="HardDiskControllerSCSI" value="8" />
        if (!pSCSIController.isNull())
        {
            StorageControllerType_T ctlr;
            rc = pSCSIController->COMGETTER(ControllerType)(&ctlr);
            if (SUCCEEDED(rc))
            {
                Utf8Str strVbox = "LsiLogic";       // the default in VBox
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

        if (!pSASController.isNull())
        {
            // VirtualBox considers the SAS controller a class of its own but in OVF
            // it should be a SCSI controller
            Utf8Str strVbox = "LsiLogicSas";
            lSCSIControllerIndex = (int32_t)pNewDesc->m->llDescriptions.size();
            pNewDesc->addEntry(VirtualSystemDescriptionType_HardDiskControllerSAS,
                               Utf8StrFmt("%d", lSCSIControllerIndex),
                               strVbox,
                               strVbox);
        }

//     <const name="HardDiskImage" value="9" />
//     <const name="Floppy" value="18" />
//     <const name="CDROM" value="19" />

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

            rc = GetStorageControllerByName(controllerName.raw(), ctl.asOutParam());
            if (FAILED(rc)) throw rc;

            StorageBus_T storageBus;
            DeviceType_T deviceType;
            LONG lChannel;
            LONG lDevice;

            rc = ctl->COMGETTER(Bus)(&storageBus);
            if (FAILED(rc)) throw rc;

            rc = pHDA->COMGETTER(Type)(&deviceType);
            if (FAILED(rc)) throw rc;

            rc = pHDA->COMGETTER(Medium)(pMedium.asOutParam());
            if (FAILED(rc)) throw rc;

            rc = pHDA->COMGETTER(Port)(&lChannel);
            if (FAILED(rc)) throw rc;

            rc = pHDA->COMGETTER(Device)(&lDevice);
            if (FAILED(rc)) throw rc;

            Utf8Str strTargetVmdkName;
            Utf8Str strLocation;
            LONG64  llSize = 0;

            if (    deviceType == DeviceType_HardDisk
                 && pMedium
               )
            {
                Bstr bstrLocation;
                rc = pMedium->COMGETTER(Location)(bstrLocation.asOutParam());
                if (FAILED(rc)) throw rc;
                strLocation = bstrLocation;

                // find the source's base medium for two things:
                // 1) we'll use its name to determine the name of the target disk, which is readable,
                //    as opposed to the UUID filename of a differencing image, if pMedium is one
                // 2) we need the size of the base image so we can give it to addEntry(), and later
                //    on export, the progress will be based on that (and not the diff image)
                ComPtr<IMedium> pBaseMedium;
                rc = pMedium->COMGETTER(Base)(pBaseMedium.asOutParam());
                        // returns pMedium if there are no diff images
                if (FAILED(rc)) throw rc;

                Bstr bstrBaseName;
                rc = pBaseMedium->COMGETTER(Name)(bstrBaseName.asOutParam());
                if (FAILED(rc)) throw rc;

                Utf8Str strTargetName = Utf8Str(locInfo.strPath).stripPath().stripExt();
                strTargetVmdkName = Utf8StrFmt("%s-disk%d.vmdk", strTargetName.c_str(), ++pAppliance->m->cDisks);

                // force reading state, or else size will be returned as 0
                MediumState_T ms;
                rc = pBaseMedium->RefreshState(&ms);
                if (FAILED(rc)) throw rc;

                rc = pBaseMedium->COMGETTER(Size)(&llSize);
                if (FAILED(rc)) throw rc;
            }

            // and how this translates to the virtual system
            int32_t lControllerVsys = 0;
            LONG lChannelVsys;

            switch (storageBus)
            {
                case StorageBus_IDE:
                    // this is the exact reverse to what we're doing in Appliance::taskThreadImportMachines,
                    // and it must be updated when that is changed!
                    // Before 3.2 we exported one IDE controller with channel 0-3, but we now maintain
                    // compatibility with what VMware does and export two IDE controllers with two channels each

                    if (lChannel == 0 && lDevice == 0)      // primary master
                    {
                        lControllerVsys = lIDEControllerPrimaryIndex;
                        lChannelVsys = 0;
                    }
                    else if (lChannel == 0 && lDevice == 1) // primary slave
                    {
                        lControllerVsys = lIDEControllerPrimaryIndex;
                        lChannelVsys = 1;
                    }
                    else if (lChannel == 1 && lDevice == 0) // secondary master; by default this is the CD-ROM but as of VirtualBox 3.1 that can change
                    {
                        lControllerVsys = lIDEControllerSecondaryIndex;
                        lChannelVsys = 0;
                    }
                    else if (lChannel == 1 && lDevice == 1) // secondary slave
                    {
                        lControllerVsys = lIDEControllerSecondaryIndex;
                        lChannelVsys = 1;
                    }
                    else
                        throw setError(VBOX_E_NOT_SUPPORTED,
                                    tr("Cannot handle medium attachment: channel is %d, device is %d"), lChannel, lDevice);
                break;

                case StorageBus_SATA:
                    lChannelVsys = lChannel;        // should be between 0 and 29
                    lControllerVsys = lSATAControllerIndex;
                break;

                case StorageBus_SCSI:
                case StorageBus_SAS:
                    lChannelVsys = lChannel;        // should be between 0 and 15
                    lControllerVsys = lSCSIControllerIndex;
                break;

                case StorageBus_Floppy:
                    lChannelVsys = 0;
                    lControllerVsys = 0;
                break;

                default:
                    throw setError(VBOX_E_NOT_SUPPORTED,
                                tr("Cannot handle medium attachment: storageBus is %d, channel is %d, device is %d"), storageBus, lChannel, lDevice);
                break;
            }

            Utf8StrFmt strExtra("controller=%RI32;channel=%RI32", lControllerVsys, lChannelVsys);
            Utf8Str strEmpty;

            switch (deviceType)
            {
                case DeviceType_HardDisk:
                    Log(("Adding VirtualSystemDescriptionType_HardDiskImage, disk size: %RI64\n", llSize));
                    pNewDesc->addEntry(VirtualSystemDescriptionType_HardDiskImage,
                                       strTargetVmdkName,   // disk ID: let's use the name
                                       strTargetVmdkName,   // OVF value:
                                       strLocation, // vbox value: media path
                                       (uint32_t)(llSize / _1M),
                                       strExtra);
                break;

                case DeviceType_DVD:
                    pNewDesc->addEntry(VirtualSystemDescriptionType_CDROM,
                                       strEmpty,   // disk ID
                                       strEmpty,   // OVF value
                                       strEmpty, // vbox value
                                       1,           // ulSize
                                       strExtra);
                break;

                case DeviceType_Floppy:
                    pNewDesc->addEntry(VirtualSystemDescriptionType_Floppy,
                                       strEmpty,      // disk ID
                                       strEmpty,      // OVF value
                                       strEmpty,      // vbox value
                                       1,       // ulSize
                                       strExtra);
                break;
            }
        }

//     <const name="NetworkAdapter" />
        uint32_t maxNetworkAdapters = Global::getMaxNetworkAdapters(getChipsetType());
        size_t a;
        for (a = 0; a < maxNetworkAdapters; ++a)
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
                rc = pNetworkAdapter->COMGETTER(AdapterType)(&adapterType);
                if (FAILED(rc)) throw rc;

                rc = pNetworkAdapter->COMGETTER(AttachmentType)(&attachmentType);
                if (FAILED(rc)) throw rc;

                Utf8Str strAttachmentType = convertNetworkAttachmentTypeToString(attachmentType);
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
            pNewDesc->addEntry(VirtualSystemDescriptionType_SoundCard,
                               "",
                               "ensoniq1371",       // this is what OVFTool writes and VMware supports
                               Utf8StrFmt("%RI32", audioController));

        /* We return the new description to the caller */
        ComPtr<IVirtualSystemDescription> copy(pNewDesc);
        copy.queryInterfaceTo(aDescription);

        AutoWriteLock alock(pAppliance COMMA_LOCKVAL_SRC_POS);
        // finally, add the virtual system to the appliance
        pAppliance->m->virtualSystemDescriptions.push_back(pNewDesc);
    }
    catch(HRESULT arc)
    {
        rc = arc;
    }

    return rc;
}

////////////////////////////////////////////////////////////////////////////////
//
// IAppliance public methods
//
////////////////////////////////////////////////////////////////////////////////

/**
 * Public method implementation.
 * @param format
 * @param path
 * @param aProgress
 * @return
 */
STDMETHODIMP Appliance::Write(IN_BSTR format, BOOL fManifest, IN_BSTR path, IProgress **aProgress)
{
    if (!path) return E_POINTER;
    CheckComArgOutPointerValid(aProgress);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    // do not allow entering this method if the appliance is busy reading or writing
    if (!isApplianceIdle())
        return E_ACCESSDENIED;

    // see if we can handle this file; for now we insist it has an ".ovf" extension
    Utf8Str strPath = path;
    if (!(   strPath.endsWith(".ovf", Utf8Str::CaseInsensitive)
          || strPath.endsWith(".ova", Utf8Str::CaseInsensitive)))
        return setError(VBOX_E_FILE_ERROR,
                        tr("Appliance file must have .ovf or .ova extension"));

    m->fManifest = !!fManifest;
    Utf8Str strFormat(format);
    OVFFormat ovfF;
    if (strFormat == "ovf-0.9")
        ovfF = OVF_0_9;
    else if (strFormat == "ovf-1.0")
        ovfF = OVF_1_0;
    else if (strFormat == "ovf-2.0")
        ovfF = OVF_2_0;
    else
        return setError(VBOX_E_FILE_ERROR,
                        tr("Invalid format \"%s\" specified"), strFormat.c_str());

    /* as of OVF 2.0 we have to use SHA256 */
    m->fSha256 = ovfF >= OVF_2_0;

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

////////////////////////////////////////////////////////////////////////////////
//
// Appliance private methods
//
////////////////////////////////////////////////////////////////////////////////

/*******************************************************************************
 * Export stuff
 ******************************************************************************/

/**
 * Implementation for writing out the OVF to disk. This starts a new thread which will call
 * Appliance::taskThreadWriteOVF().
 *
 * This is in a separate private method because it is used from two locations:
 *
 * 1) from the public Appliance::Write().
 *
 * 2) in a second worker thread; in that case, Appliance::Write() called Appliance::writeImpl(), which
 *    called Appliance::writeFSOVA(), which called Appliance::writeImpl(), which then called this again.
 *
 * 3) from Appliance::writeS3(), which got called from a previous instance of Appliance::taskThreadWriteOVF().
 *
 * @param aFormat
 * @param aLocInfo
 * @param aProgress
 * @return
 */
HRESULT Appliance::writeImpl(OVFFormat aFormat, const LocationInfo &aLocInfo, ComObjPtr<Progress> &aProgress)
{
    HRESULT rc = S_OK;
    try
    {
        rc = setUpProgress(aProgress,
                           BstrFmt(tr("Export appliance '%s'"), aLocInfo.strPath.c_str()),
                           (aLocInfo.storageType == VFSType_File) ? WriteFile : WriteS3);

        /* Initialize our worker task */
        std::auto_ptr<TaskOVF> task(new TaskOVF(this, TaskOVF::Write, aLocInfo, aProgress));
        /* The OVF version to write */
        task->enFormat = aFormat;

        rc = task->startThread();
        if (FAILED(rc)) throw rc;

        /* Don't destruct on success */
        task.release();
    }
    catch (HRESULT aRC)
    {
        rc = aRC;
    }

    return rc;
}

/**
 * Called from Appliance::writeFS() for creating a XML document for this
 * Appliance.
 *
 * @param writeLock                          The current write lock.
 * @param doc                                The xml document to fill.
 * @param stack                              Structure for temporary private
 *                                           data shared with caller.
 * @param strPath                            Path to the target OVF.
 *                                           instance for which to write XML.
 * @param enFormat                           OVF format (0.9 or 1.0).
 */
void Appliance::buildXML(AutoWriteLockBase& writeLock,
                         xml::Document &doc,
                         XMLStack &stack,
                         const Utf8Str &strPath,
                         OVFFormat enFormat)
{
    xml::ElementNode *pelmRoot = doc.createRootElement("Envelope");

    pelmRoot->setAttribute("ovf:version", enFormat == OVF_2_0 ? "2.0"
                                        : enFormat == OVF_1_0 ? "1.0"
                                        :                       "0.9");
    pelmRoot->setAttribute("xml:lang", "en-US");

    Utf8Str strNamespace = (enFormat == OVF_0_9)
        ? "http://www.vmware.com/schema/ovf/1/envelope"     // 0.9
        : "http://schemas.dmtf.org/ovf/envelope/1";         // 1.0
    pelmRoot->setAttribute("xmlns", strNamespace);
    pelmRoot->setAttribute("xmlns:ovf", strNamespace);

    //         pelmRoot->setAttribute("xmlns:ovfstr", "http://schema.dmtf.org/ovf/strings/1");
    pelmRoot->setAttribute("xmlns:rasd", "http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_ResourceAllocationSettingData");
    pelmRoot->setAttribute("xmlns:vssd", "http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_VirtualSystemSettingData");
    pelmRoot->setAttribute("xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance");
    pelmRoot->setAttribute("xmlns:vbox", "http://www.virtualbox.org/ovf/machine");
    //         pelmRoot->setAttribute("xsi:schemaLocation", "http://schemas.dmtf.org/ovf/envelope/1 ../ovf-envelope.xsd");

    // <Envelope>/<References>
    xml::ElementNode *pelmReferences = pelmRoot->createChild("References");     // 0.9 and 1.0

    /* <Envelope>/<DiskSection>:
       <DiskSection>
       <Info>List of the virtual disks used in the package</Info>
       <Disk ovf:capacity="4294967296" ovf:diskId="lamp" ovf:format="..." ovf:populatedSize="1924967692"/>
       </DiskSection> */
    xml::ElementNode *pelmDiskSection;
    if (enFormat == OVF_0_9)
    {
        // <Section xsi:type="ovf:DiskSection_Type">
        pelmDiskSection = pelmRoot->createChild("Section");
        pelmDiskSection->setAttribute("xsi:type", "ovf:DiskSection_Type");
    }
    else
        pelmDiskSection = pelmRoot->createChild("DiskSection");

    xml::ElementNode *pelmDiskSectionInfo = pelmDiskSection->createChild("Info");
    pelmDiskSectionInfo->addContent("List of the virtual disks used in the package");

    /* <Envelope>/<NetworkSection>:
       <NetworkSection>
       <Info>Logical networks used in the package</Info>
       <Network ovf:name="VM Network">
       <Description>The network that the LAMP Service will be available on</Description>
       </Network>
       </NetworkSection> */
    xml::ElementNode *pelmNetworkSection;
    if (enFormat == OVF_0_9)
    {
        // <Section xsi:type="ovf:NetworkSection_Type">
        pelmNetworkSection = pelmRoot->createChild("Section");
        pelmNetworkSection->setAttribute("xsi:type", "ovf:NetworkSection_Type");
    }
    else
        pelmNetworkSection = pelmRoot->createChild("NetworkSection");

    xml::ElementNode *pelmNetworkSectionInfo = pelmNetworkSection->createChild("Info");
    pelmNetworkSectionInfo->addContent("Logical networks used in the package");

    // and here come the virtual systems:

    // write a collection if we have more than one virtual system _and_ we're
    // writing OVF 1.0; otherwise fail since ovftool can't import more than
    // one machine, it seems
    xml::ElementNode *pelmToAddVirtualSystemsTo;
    if (m->virtualSystemDescriptions.size() > 1)
    {
        if (enFormat == OVF_0_9)
            throw setError(VBOX_E_FILE_ERROR,
                           tr("Cannot export more than one virtual system with OVF 0.9, use OVF 1.0"));

        pelmToAddVirtualSystemsTo = pelmRoot->createChild("VirtualSystemCollection");
        pelmToAddVirtualSystemsTo->setAttribute("ovf:name", "ExportedVirtualBoxMachines");      // whatever
    }
    else
        pelmToAddVirtualSystemsTo = pelmRoot;       // add virtual system directly under root element

    // this list receives pointers to the XML elements in the machine XML which
    // might have UUIDs that need fixing after we know the UUIDs of the exported images
    std::list<xml::ElementNode*> llElementsWithUuidAttributes;

    list< ComObjPtr<VirtualSystemDescription> >::const_iterator it;
    /* Iterate through all virtual systems of that appliance */
    for (it = m->virtualSystemDescriptions.begin();
         it != m->virtualSystemDescriptions.end();
         ++it)
    {
        ComObjPtr<VirtualSystemDescription> vsdescThis = *it;
        buildXMLForOneVirtualSystem(writeLock,
                                    *pelmToAddVirtualSystemsTo,
                                    &llElementsWithUuidAttributes,
                                    vsdescThis,
                                    enFormat,
                                    stack);         // disks and networks stack
    }

    // now, fill in the network section we set up empty above according
    // to the networks we found with the hardware items
    map<Utf8Str, bool>::const_iterator itN;
    for (itN = stack.mapNetworks.begin();
         itN != stack.mapNetworks.end();
         ++itN)
    {
        const Utf8Str &strNetwork = itN->first;
        xml::ElementNode *pelmNetwork = pelmNetworkSection->createChild("Network");
        pelmNetwork->setAttribute("ovf:name", strNetwork.c_str());
        pelmNetwork->createChild("Description")->addContent("Logical network used by this appliance.");
    }

    // Finally, write out the disk info
    list<Utf8Str> diskList;
    map<Utf8Str, const VirtualSystemDescriptionEntry*>::const_iterator itS;
    uint32_t ulFile = 1;
    for (itS = stack.mapDisks.begin();
         itS != stack.mapDisks.end();
         ++itS)
    {
        const Utf8Str &strDiskID = itS->first;
        const VirtualSystemDescriptionEntry *pDiskEntry = itS->second;

        // source path: where the VBox image is
        const Utf8Str &strSrcFilePath = pDiskEntry->strVboxCurrent;
        Bstr bstrSrcFilePath(strSrcFilePath);

        // Do NOT check here whether the file exists. FindMedium will figure
        // that out, and filesystem-based tests are simply wrong in the
        // general case (think of iSCSI).

        // We need some info from the source disks
        ComPtr<IMedium> pSourceDisk;

        Log(("Finding source disk \"%ls\"\n", bstrSrcFilePath.raw()));
        HRESULT rc = mVirtualBox->OpenMedium(bstrSrcFilePath.raw(), DeviceType_HardDisk, AccessMode_ReadWrite, FALSE /* fForceNewUuid */,  pSourceDisk.asOutParam());
        if (FAILED(rc)) throw rc;

        Bstr uuidSource;
        rc = pSourceDisk->COMGETTER(Id)(uuidSource.asOutParam());
        if (FAILED(rc)) throw rc;
        Guid guidSource(uuidSource);

        // output filename
        const Utf8Str &strTargetFileNameOnly = pDiskEntry->strOvf;
        // target path needs to be composed from where the output OVF is
        Utf8Str strTargetFilePath(strPath);
        strTargetFilePath.stripFilename();
        strTargetFilePath.append("/");
        strTargetFilePath.append(strTargetFileNameOnly);

        // We are always exporting to VMDK stream optimized for now
        Bstr bstrSrcFormat = L"VMDK";

        diskList.push_back(strTargetFilePath);

        LONG64 cbCapacity = 0;     // size reported to guest
        rc = pSourceDisk->COMGETTER(LogicalSize)(&cbCapacity);
        if (FAILED(rc)) throw rc;
        // Todo r=poetzsch: wrong it is reported in bytes ...
        // capacity is reported in megabytes, so...
        //cbCapacity *= _1M;

        Guid guidTarget; /* Creates a new uniq number for the target disk. */
        guidTarget.create();

        // now handle the XML for the disk:
        Utf8StrFmt strFileRef("file%RI32", ulFile++);
        // <File ovf:href="WindowsXpProfessional-disk1.vmdk" ovf:id="file1" ovf:size="1710381056"/>
        xml::ElementNode *pelmFile = pelmReferences->createChild("File");
        pelmFile->setAttribute("ovf:href", strTargetFileNameOnly);
        pelmFile->setAttribute("ovf:id", strFileRef);
        // Todo: the actual size is not available at this point of time,
        // cause the disk will be compressed. The 1.0 standard says this is
        // optional! 1.1 isn't fully clear if the "gzip" format is used.
        // Need to be checked. */
        //            pelmFile->setAttribute("ovf:size", Utf8StrFmt("%RI64", cbFile).c_str());

        // add disk to XML Disks section
        // <Disk ovf:capacity="8589934592" ovf:diskId="vmdisk1" ovf:fileRef="file1" ovf:format="..."/>
        xml::ElementNode *pelmDisk = pelmDiskSection->createChild("Disk");
        pelmDisk->setAttribute("ovf:capacity", Utf8StrFmt("%RI64", cbCapacity).c_str());
        pelmDisk->setAttribute("ovf:diskId", strDiskID);
        pelmDisk->setAttribute("ovf:fileRef", strFileRef);
        pelmDisk->setAttribute("ovf:format",
                               (enFormat == OVF_0_9)
                               ?  "http://www.vmware.com/specifications/vmdk.html#sparse"      // must be sparse or ovftool chokes
                               :  "http://www.vmware.com/interfaces/specifications/vmdk.html#streamOptimized"
                               // correct string as communicated to us by VMware (public bug #6612)
                              );

        // add the UUID of the newly target image to the OVF disk element, but in the
        // vbox: namespace since it's not part of the standard
        pelmDisk->setAttribute("vbox:uuid", Utf8StrFmt("%RTuuid", guidTarget.raw()).c_str());

        // now, we might have other XML elements from vbox:Machine pointing to this image,
        // but those would refer to the UUID of the _source_ image (which we created the
        // export image from); those UUIDs need to be fixed to the export image
        Utf8Str strGuidSourceCurly = guidSource.toStringCurly();
        for (std::list<xml::ElementNode*>::iterator eit = llElementsWithUuidAttributes.begin();
             eit != llElementsWithUuidAttributes.end();
             ++eit)
        {
            xml::ElementNode *pelmImage = *eit;
            Utf8Str strUUID;
            pelmImage->getAttributeValue("uuid", strUUID);
            if (strUUID == strGuidSourceCurly)
                // overwrite existing uuid attribute
                pelmImage->setAttribute("uuid", guidTarget.toStringCurly());
        }
    }
}

/**
 * Called from Appliance::buildXML() for each virtual system (machine) that
 * needs XML written out.
 *
 * @param writeLock                          The current write lock.
 * @param elmToAddVirtualSystemsTo           XML element to append elements to.
 * @param pllElementsWithUuidAttributes out: list of XML elements produced here
 *                                           with UUID attributes for quick
 *                                           fixing by caller later
 * @param vsdescThis                         The IVirtualSystemDescription
 *                                           instance for which to write XML.
 * @param enFormat                           OVF format (0.9 or 1.0).
 * @param stack                              Structure for temporary private
 *                                           data shared with caller.
 */
void Appliance::buildXMLForOneVirtualSystem(AutoWriteLockBase& writeLock,
                                            xml::ElementNode &elmToAddVirtualSystemsTo,
                                            std::list<xml::ElementNode*> *pllElementsWithUuidAttributes,
                                            ComObjPtr<VirtualSystemDescription> &vsdescThis,
                                            OVFFormat enFormat,
                                            XMLStack &stack)
{
    LogFlowFunc(("ENTER appliance %p\n", this));

    xml::ElementNode *pelmVirtualSystem;
    if (enFormat == OVF_0_9)
    {
        // <Section xsi:type="ovf:NetworkSection_Type">
        pelmVirtualSystem = elmToAddVirtualSystemsTo.createChild("Content");
        pelmVirtualSystem->setAttribute("xsi:type", "ovf:VirtualSystem_Type");
    }
    else
        pelmVirtualSystem = elmToAddVirtualSystemsTo.createChild("VirtualSystem");

    /*xml::ElementNode *pelmVirtualSystemInfo =*/ pelmVirtualSystem->createChild("Info")->addContent("A virtual machine");

    std::list<VirtualSystemDescriptionEntry*> llName = vsdescThis->findByType(VirtualSystemDescriptionType_Name);
    if (llName.size() != 1)
        throw setError(VBOX_E_NOT_SUPPORTED,
                        tr("Missing VM name"));
    Utf8Str &strVMName = llName.front()->strVboxCurrent;
    pelmVirtualSystem->setAttribute("ovf:id", strVMName);

    // product info
    std::list<VirtualSystemDescriptionEntry*> llProduct = vsdescThis->findByType(VirtualSystemDescriptionType_Product);
    std::list<VirtualSystemDescriptionEntry*> llProductUrl = vsdescThis->findByType(VirtualSystemDescriptionType_ProductUrl);
    std::list<VirtualSystemDescriptionEntry*> llVendor = vsdescThis->findByType(VirtualSystemDescriptionType_Vendor);
    std::list<VirtualSystemDescriptionEntry*> llVendorUrl = vsdescThis->findByType(VirtualSystemDescriptionType_VendorUrl);
    std::list<VirtualSystemDescriptionEntry*> llVersion = vsdescThis->findByType(VirtualSystemDescriptionType_Version);
    bool fProduct = llProduct.size() && !llProduct.front()->strVboxCurrent.isEmpty();
    bool fProductUrl = llProductUrl.size() && !llProductUrl.front()->strVboxCurrent.isEmpty();
    bool fVendor = llVendor.size() && !llVendor.front()->strVboxCurrent.isEmpty();
    bool fVendorUrl = llVendorUrl.size() && !llVendorUrl.front()->strVboxCurrent.isEmpty();
    bool fVersion = llVersion.size() && !llVersion.front()->strVboxCurrent.isEmpty();
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
        if (enFormat == OVF_0_9)
        {
            // <Section ovf:required="false" xsi:type="ovf:ProductSection_Type">
            pelmAnnotationSection = pelmVirtualSystem->createChild("Section");
            pelmAnnotationSection->setAttribute("xsi:type", "ovf:ProductSection_Type");
        }
        else
            pelmAnnotationSection = pelmVirtualSystem->createChild("ProductSection");

        pelmAnnotationSection->createChild("Info")->addContent("Meta-information about the installed software");
        if (fProduct)
            pelmAnnotationSection->createChild("Product")->addContent(llProduct.front()->strVboxCurrent);
        if (fVendor)
            pelmAnnotationSection->createChild("Vendor")->addContent(llVendor.front()->strVboxCurrent);
        if (fVersion)
            pelmAnnotationSection->createChild("Version")->addContent(llVersion.front()->strVboxCurrent);
        if (fProductUrl)
            pelmAnnotationSection->createChild("ProductUrl")->addContent(llProductUrl.front()->strVboxCurrent);
        if (fVendorUrl)
            pelmAnnotationSection->createChild("VendorUrl")->addContent(llVendorUrl.front()->strVboxCurrent);
    }

    // description
    std::list<VirtualSystemDescriptionEntry*> llDescription = vsdescThis->findByType(VirtualSystemDescriptionType_Description);
    if (llDescription.size() &&
        !llDescription.front()->strVboxCurrent.isEmpty())
    {
        /*  <Section ovf:required="false" xsi:type="ovf:AnnotationSection_Type">
                <Info>A human-readable annotation</Info>
                <Annotation>Plan 9</Annotation>
            </Section> */
        xml::ElementNode *pelmAnnotationSection;
        if (enFormat == OVF_0_9)
        {
            // <Section ovf:required="false" xsi:type="ovf:AnnotationSection_Type">
            pelmAnnotationSection = pelmVirtualSystem->createChild("Section");
            pelmAnnotationSection->setAttribute("xsi:type", "ovf:AnnotationSection_Type");
        }
        else
            pelmAnnotationSection = pelmVirtualSystem->createChild("AnnotationSection");

        pelmAnnotationSection->createChild("Info")->addContent("A human-readable annotation");
        pelmAnnotationSection->createChild("Annotation")->addContent(llDescription.front()->strVboxCurrent);
    }

    // license
    std::list<VirtualSystemDescriptionEntry*> llLicense = vsdescThis->findByType(VirtualSystemDescriptionType_License);
    if (llLicense.size() &&
        !llLicense.front()->strVboxCurrent.isEmpty())
    {
        /* <EulaSection>
            <Info ovf:msgid="6">License agreement for the Virtual System.</Info>
            <License ovf:msgid="1">License terms can go in here.</License>
            </EulaSection> */
        xml::ElementNode *pelmEulaSection;
        if (enFormat == OVF_0_9)
        {
            pelmEulaSection = pelmVirtualSystem->createChild("Section");
            pelmEulaSection->setAttribute("xsi:type", "ovf:EulaSection_Type");
        }
        else
            pelmEulaSection = pelmVirtualSystem->createChild("EulaSection");

        pelmEulaSection->createChild("Info")->addContent("License agreement for the virtual system");
        pelmEulaSection->createChild("License")->addContent(llLicense.front()->strVboxCurrent);
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
    VirtualSystemDescriptionEntry *pvsdeOS = llOS.front();
    xml::ElementNode *pelmOperatingSystemSection;
    if (enFormat == OVF_0_9)
    {
        pelmOperatingSystemSection = pelmVirtualSystem->createChild("Section");
        pelmOperatingSystemSection->setAttribute("xsi:type", "ovf:OperatingSystemSection_Type");
    }
    else
        pelmOperatingSystemSection = pelmVirtualSystem->createChild("OperatingSystemSection");

    pelmOperatingSystemSection->setAttribute("ovf:id", pvsdeOS->strOvf);
    pelmOperatingSystemSection->createChild("Info")->addContent("The kind of installed guest operating system");
    Utf8Str strOSDesc;
    convertCIMOSType2VBoxOSType(strOSDesc, (ovf::CIMOSType_T)pvsdeOS->strOvf.toInt32(), "");
    pelmOperatingSystemSection->createChild("Description")->addContent(strOSDesc);
    // add the VirtualBox ostype in a custom tag in a different namespace
    xml::ElementNode *pelmVBoxOSType = pelmOperatingSystemSection->createChild("vbox:OSType");
    pelmVBoxOSType->setAttribute("ovf:required", "false");
    pelmVBoxOSType->addContent(pvsdeOS->strVboxCurrent);

    // <VirtualHardwareSection ovf:id="hw1" ovf:transport="iso">
    xml::ElementNode *pelmVirtualHardwareSection;
    if (enFormat == OVF_0_9)
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
    if (enFormat == OVF_0_9)
        pelmSystem->createChild("vssd:InstanceId")->addContent("0");
    else // capitalization changed...
        pelmSystem->createChild("vssd:InstanceID")->addContent("0");

    // <vssd:VirtualSystemIdentifier>VAtest</vssd:VirtualSystemIdentifier>
    pelmSystem->createChild("vssd:VirtualSystemIdentifier")->addContent(strVMName);
    // <vssd:VirtualSystemType>vmx-4</vssd:VirtualSystemType>
    const char *pcszHardware = "virtualbox-2.2";
    if (enFormat == OVF_0_9)
        // pretend to be vmware compatible then
        pcszHardware = "vmx-6";
    pelmSystem->createChild("vssd:VirtualSystemType")->addContent(pcszHardware);

    // loop thru all description entries twice; once to write out all
    // devices _except_ disk images, and a second time to assign the
    // disk images; this is because disk images need to reference
    // IDE controllers, and we can't know their instance IDs without
    // assigning them first

    uint32_t idIDEPrimaryController = 0;
    int32_t lIDEPrimaryControllerIndex = 0;
    uint32_t idIDESecondaryController = 0;
    int32_t lIDESecondaryControllerIndex = 0;
    uint32_t idSATAController = 0;
    int32_t lSATAControllerIndex = 0;
    uint32_t idSCSIController = 0;
    int32_t lSCSIControllerIndex = 0;

    uint32_t ulInstanceID = 1;

    uint32_t cDVDs = 0;

    for (size_t uLoop = 1; uLoop <= 2; ++uLoop)
    {
        int32_t lIndexThis = 0;
        list<VirtualSystemDescriptionEntry>::const_iterator itD;
        for (itD = vsdescThis->m->llDescriptions.begin();
            itD != vsdescThis->m->llDescriptions.end();
            ++itD, ++lIndexThis)
        {
            const VirtualSystemDescriptionEntry &desc = *itD;

            LogFlowFunc(("Loop %u: handling description entry ulIndex=%u, type=%s, strRef=%s, strOvf=%s, strVbox=%s, strExtraConfig=%s\n",
                         uLoop,
                         desc.ulIndex,
                         (  desc.type == VirtualSystemDescriptionType_HardDiskControllerIDE ? "HardDiskControllerIDE"
                          : desc.type == VirtualSystemDescriptionType_HardDiskControllerSATA ? "HardDiskControllerSATA"
                          : desc.type == VirtualSystemDescriptionType_HardDiskControllerSCSI ? "HardDiskControllerSCSI"
                          : desc.type == VirtualSystemDescriptionType_HardDiskControllerSAS ? "HardDiskControllerSAS"
                          : desc.type == VirtualSystemDescriptionType_HardDiskImage ? "HardDiskImage"
                          : Utf8StrFmt("%d", desc.type).c_str()),
                         desc.strRef.c_str(),
                         desc.strOvf.c_str(),
                         desc.strVboxCurrent.c_str(),
                         desc.strExtraConfigCurrent.c_str()));

            ovf::ResourceType_T type = (ovf::ResourceType_T)0;      // if this becomes != 0 then we do stuff
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
                        type = ovf::ResourceType_Processor; // 3
                        desc.strVboxCurrent.toInt(uTemp);
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
                        type = ovf::ResourceType_Memory; // 4
                        desc.strVboxCurrent.toInt(uTemp);
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
                        type = ovf::ResourceType_IDEController; // 5
                        strResourceSubType = desc.strVboxCurrent;

                        if (!lIDEPrimaryControllerIndex)
                        {
                            // first IDE controller:
                            strCaption = "ideController0";
                            lAddress = 0;
                            lBusNumber = 0;
                            // remember this ID
                            idIDEPrimaryController = ulInstanceID;
                            lIDEPrimaryControllerIndex = lIndexThis;
                        }
                        else
                        {
                            // second IDE controller:
                            strCaption = "ideController1";
                            lAddress = 1;
                            lBusNumber = 1;
                            // remember this ID
                            idIDESecondaryController = ulInstanceID;
                            lIDESecondaryControllerIndex = lIndexThis;
                        }
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
                        type = ovf::ResourceType_OtherStorageDevice; // 20
                        // it seems that OVFTool always writes these two, and since we can only
                        // have one SATA controller, we'll use this as well
                        lAddress = 0;
                        lBusNumber = 0;

                        if (    desc.strVboxCurrent.isEmpty()      // AHCI is the default in VirtualBox
                             || (!desc.strVboxCurrent.compare("ahci", Utf8Str::CaseInsensitive))
                           )
                            strResourceSubType = "AHCI";
                        else
                            throw setError(VBOX_E_NOT_SUPPORTED,
                                            tr("Invalid config string \"%s\" in SATA controller"), desc.strVboxCurrent.c_str());

                        // remember this ID
                        idSATAController = ulInstanceID;
                        lSATAControllerIndex = lIndexThis;
                    }
                break;

                case VirtualSystemDescriptionType_HardDiskControllerSCSI:
                case VirtualSystemDescriptionType_HardDiskControllerSAS:
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
                        type = ovf::ResourceType_ParallelSCSIHBA; // 6
                        // it seems that OVFTool always writes these two, and since we can only
                        // have one SATA controller, we'll use this as well
                        lAddress = 0;
                        lBusNumber = 0;

                        if (    desc.strVboxCurrent.isEmpty()      // LsiLogic is the default in VirtualBox
                             || (!desc.strVboxCurrent.compare("lsilogic", Utf8Str::CaseInsensitive))
                            )
                            strResourceSubType = "lsilogic";
                        else if (!desc.strVboxCurrent.compare("buslogic", Utf8Str::CaseInsensitive))
                            strResourceSubType = "buslogic";
                        else if (!desc.strVboxCurrent.compare("lsilogicsas", Utf8Str::CaseInsensitive))
                            strResourceSubType = "lsilogicsas";
                        else
                            throw setError(VBOX_E_NOT_SUPPORTED,
                                            tr("Invalid config string \"%s\" in SCSI/SAS controller"), desc.strVboxCurrent.c_str());

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
                        uint32_t cDisks = stack.mapDisks.size();
                        Utf8Str strDiskID = Utf8StrFmt("vmdisk%RI32", ++cDisks);

                        strDescription = "Disk Image";
                        strCaption = Utf8StrFmt("disk%RI32", cDisks);        // this is not used for anything else
                        type = ovf::ResourceType_HardDisk; // 17

                        // the following references the "<Disks>" XML block
                        strHostResource = Utf8StrFmt("/disk/%s", strDiskID.c_str());

                        // controller=<index>;channel=<c>
                        size_t pos1 = desc.strExtraConfigCurrent.find("controller=");
                        size_t pos2 = desc.strExtraConfigCurrent.find("channel=");
                        int32_t lControllerIndex = -1;
                        if (pos1 != Utf8Str::npos)
                        {
                            RTStrToInt32Ex(desc.strExtraConfigCurrent.c_str() + pos1 + 11, NULL, 0, &lControllerIndex);
                            if (lControllerIndex == lIDEPrimaryControllerIndex)
                                ulParent = idIDEPrimaryController;
                            else if (lControllerIndex == lIDESecondaryControllerIndex)
                                ulParent = idIDESecondaryController;
                            else if (lControllerIndex == lSCSIControllerIndex)
                                ulParent = idSCSIController;
                            else if (lControllerIndex == lSATAControllerIndex)
                                ulParent = idSATAController;
                        }
                        if (pos2 != Utf8Str::npos)
                            RTStrToInt32Ex(desc.strExtraConfigCurrent.c_str() + pos2 + 8, NULL, 0, &lAddressOnParent);

                        LogFlowFunc(("HardDiskImage details: pos1=%d, pos2=%d, lControllerIndex=%d, lIDEPrimaryControllerIndex=%d, lIDESecondaryControllerIndex=%d, ulParent=%d, lAddressOnParent=%d\n",
                                     pos1, pos2, lControllerIndex, lIDEPrimaryControllerIndex, lIDESecondaryControllerIndex, ulParent, lAddressOnParent));

                        if (    !ulParent
                             || lAddressOnParent == -1
                           )
                            throw setError(VBOX_E_NOT_SUPPORTED,
                                            tr("Missing or bad extra config string in hard disk image: \"%s\""), desc.strExtraConfigCurrent.c_str());

                        stack.mapDisks[strDiskID] = &desc;
                    }
                break;

                case VirtualSystemDescriptionType_Floppy:
                    if (uLoop == 1)
                    {
                        strDescription = "Floppy Drive";
                        strCaption = "floppy0";         // this is what OVFTool writes
                        type = ovf::ResourceType_FloppyDrive; // 14
                        lAutomaticAllocation = 0;
                        lAddressOnParent = 0;           // this is what OVFTool writes
                    }
                break;

                case VirtualSystemDescriptionType_CDROM:
                    if (uLoop == 2)
                    {
                        strDescription = "CD-ROM Drive";
                        strCaption = Utf8StrFmt("cdrom%RI32", ++cDVDs);     // OVFTool starts with 1
                        type = ovf::ResourceType_CDDrive; // 15
                        lAutomaticAllocation = 1;

                        // controller=<index>;channel=<c>
                        size_t pos1 = desc.strExtraConfigCurrent.find("controller=");
                        size_t pos2 = desc.strExtraConfigCurrent.find("channel=");
                        int32_t lControllerIndex = -1;
                        if (pos1 != Utf8Str::npos)
                        {
                            RTStrToInt32Ex(desc.strExtraConfigCurrent.c_str() + pos1 + 11, NULL, 0, &lControllerIndex);
                            if (lControllerIndex == lIDEPrimaryControllerIndex)
                                ulParent = idIDEPrimaryController;
                            else if (lControllerIndex == lIDESecondaryControllerIndex)
                                ulParent = idIDESecondaryController;
                            else if (lControllerIndex == lSCSIControllerIndex)
                                ulParent = idSCSIController;
                            else if (lControllerIndex == lSATAControllerIndex)
                                ulParent = idSATAController;
                        }
                        if (pos2 != Utf8Str::npos)
                            RTStrToInt32Ex(desc.strExtraConfigCurrent.c_str() + pos2 + 8, NULL, 0, &lAddressOnParent);

                        LogFlowFunc(("DVD drive details: pos1=%d, pos2=%d, lControllerIndex=%d, lIDEPrimaryControllerIndex=%d, lIDESecondaryControllerIndex=%d, ulParent=%d, lAddressOnParent=%d\n",
                                     pos1, pos2, lControllerIndex, lIDEPrimaryControllerIndex, lIDESecondaryControllerIndex, ulParent, lAddressOnParent));

                        if (    !ulParent
                             || lAddressOnParent == -1
                           )
                            throw setError(VBOX_E_NOT_SUPPORTED,
                                            tr("Missing or bad extra config string in DVD drive medium: \"%s\""), desc.strExtraConfigCurrent.c_str());

                        // there is no DVD drive map to update because it is
                        // handled completely with this entry.
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
                        type = ovf::ResourceType_EthernetAdapter; // 10
                        /* Set the hardware type to something useful.
                            * To be compatible with vmware & others we set
                            * PCNet32 for our PCNet types & E1000 for the
                            * E1000 cards. */
                        switch (desc.strVboxCurrent.toInt32())
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

                        stack.mapNetworks[desc.strOvf] = true;
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
                        type = ovf::ResourceType_USBController; // 23
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
                        type = ovf::ResourceType_SoundCard; // 35
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

                // NOTE: DO NOT CHANGE THE ORDER of these items! The OVF standards prescribes that
                // the elements from the rasd: namespace must be sorted by letter, and VMware
                // actually requires this as well (see public bug #6612)

                if (lAddress != -1)
                    pItem->createChild("rasd:Address")->addContent(Utf8StrFmt("%d", lAddress));

                if (lAddressOnParent != -1)
                    pItem->createChild("rasd:AddressOnParent")->addContent(Utf8StrFmt("%d", lAddressOnParent));

                if (!strAllocationUnits.isEmpty())
                    pItem->createChild("rasd:AllocationUnits")->addContent(strAllocationUnits);

                if (lAutomaticAllocation != -1)
                    pItem->createChild("rasd:AutomaticAllocation")->addContent( (lAutomaticAllocation) ? "true" : "false" );

                if (lBusNumber != -1)
                    if (enFormat == OVF_0_9) // BusNumber is invalid OVF 1.0 so only write it in 0.9 mode for OVFTool compatibility
                        pItem->createChild("rasd:BusNumber")->addContent(Utf8StrFmt("%d", lBusNumber));

                if (!strCaption.isEmpty())
                    pItem->createChild("rasd:Caption")->addContent(strCaption);

                if (!strConnection.isEmpty())
                    pItem->createChild("rasd:Connection")->addContent(strConnection);

                if (!strDescription.isEmpty())
                    pItem->createChild("rasd:Description")->addContent(strDescription);

                if (!strCaption.isEmpty())
                    if (enFormat == OVF_1_0)
                        pItem->createChild("rasd:ElementName")->addContent(strCaption);

                if (!strHostResource.isEmpty())
                    pItem->createChild("rasd:HostResource")->addContent(strHostResource);

                // <rasd:InstanceID>1</rasd:InstanceID>
                xml::ElementNode *pelmInstanceID;
                if (enFormat == OVF_0_9)
                    pelmInstanceID = pItem->createChild("rasd:InstanceId");
                else
                    pelmInstanceID = pItem->createChild("rasd:InstanceID");      // capitalization changed...
                pelmInstanceID->addContent(Utf8StrFmt("%d", ulInstanceID++));

                if (ulParent)
                    pItem->createChild("rasd:Parent")->addContent(Utf8StrFmt("%d", ulParent));

                if (!strResourceSubType.isEmpty())
                    pItem->createChild("rasd:ResourceSubType")->addContent(strResourceSubType);

                // <rasd:ResourceType>3</rasd:ResourceType>
                pItem->createChild("rasd:ResourceType")->addContent(Utf8StrFmt("%d", type));

                // <rasd:VirtualQuantity>1</rasd:VirtualQuantity>
                if (lVirtualQuantity != -1)
                    pItem->createChild("rasd:VirtualQuantity")->addContent(Utf8StrFmt("%d", lVirtualQuantity));
            }
        }
    } // for (size_t uLoop = 1; uLoop <= 2; ++uLoop)

    // now that we're done with the official OVF <Item> tags under <VirtualSystem>, write out VirtualBox XML
    // under the vbox: namespace
    xml::ElementNode *pelmVBoxMachine = pelmVirtualSystem->createChild("vbox:Machine");
    // ovf:required="false" tells other OVF parsers that they can ignore this thing
    pelmVBoxMachine->setAttribute("ovf:required", "false");
    // ovf:Info element is required or VMware will bail out on the vbox:Machine element
    pelmVBoxMachine->createChild("ovf:Info")->addContent("Complete VirtualBox machine configuration in VirtualBox format");

    // create an empty machine config
    settings::MachineConfigFile *pConfig = new settings::MachineConfigFile(NULL);

    writeLock.release();
    try
    {
        AutoWriteLock machineLock(vsdescThis->m->pMachine COMMA_LOCKVAL_SRC_POS);
        // fill the machine config
        vsdescThis->m->pMachine->copyMachineDataToSettings(*pConfig);
        // write the machine config to the vbox:Machine element
        pConfig->buildMachineXML(*pelmVBoxMachine,
                                   settings::MachineConfigFile::BuildMachineXML_WriteVboxVersionAttribute
                                 | settings::MachineConfigFile::BuildMachineXML_SkipRemovableMedia
                                 | settings::MachineConfigFile::BuildMachineXML_SuppressSavedState,
                                        // but not BuildMachineXML_IncludeSnapshots nor BuildMachineXML_MediaRegistry
                                 pllElementsWithUuidAttributes);
        delete pConfig;
    }
    catch (...)
    {
        writeLock.acquire();
        delete pConfig;
        throw;
    }
    writeLock.acquire();
}

/**
 * Actual worker code for writing out OVF/OVA to disk. This is called from Appliance::taskThreadWriteOVF()
 * and therefore runs on the OVF/OVA write worker thread. This runs in two contexts:
 *
 * 1) in a first worker thread; in that case, Appliance::Write() called Appliance::writeImpl();
 *
 * 2) in a second worker thread; in that case, Appliance::Write() called Appliance::writeImpl(), which
 *    called Appliance::writeS3(), which called Appliance::writeImpl(), which then called this. In other
 *    words, to write to the cloud, the first worker thread first starts a second worker thread to create
 *    temporary files and then uploads them to the S3 cloud server.
 *
 * @param pTask
 * @return
 */
HRESULT Appliance::writeFS(TaskOVF *pTask)
{
    LogFlowFuncEnter();
    LogFlowFunc(("ENTER appliance %p\n", this));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT rc = S_OK;

    // Lock the media tree early to make sure nobody else tries to make changes
    // to the tree. Also lock the IAppliance object for writing.
    AutoMultiWriteLock2 multiLock(&mVirtualBox->getMediaTreeLockHandle(), this->lockHandle() COMMA_LOCKVAL_SRC_POS);
    // Additional protect the IAppliance object, cause we leave the lock
    // when starting the disk export and we don't won't block other
    // callers on this lengthy operations.
    m->state = Data::ApplianceExporting;

    if (pTask->locInfo.strPath.endsWith(".ovf", Utf8Str::CaseInsensitive))
        rc = writeFSOVF(pTask, multiLock);
    else
        rc = writeFSOVA(pTask, multiLock);

    // reset the state so others can call methods again
    m->state = Data::ApplianceIdle;

    LogFlowFunc(("rc=%Rhrc\n", rc));
    LogFlowFuncLeave();
    return rc;
}

HRESULT Appliance::writeFSOVF(TaskOVF *pTask, AutoWriteLockBase& writeLock)
{
    LogFlowFuncEnter();

    HRESULT rc = S_OK;

    PVDINTERFACEIO pShaIo = 0;
    PVDINTERFACEIO pFileIo = 0;
    do
    {
        pShaIo = ShaCreateInterface();
        if (!pShaIo)
        {
            rc = E_OUTOFMEMORY;
            break;
        }
        pFileIo = FileCreateInterface();
        if (!pFileIo)
        {
            rc = E_OUTOFMEMORY;
            break;
        }

        SHASTORAGE storage;
        RT_ZERO(storage);
        storage.fCreateDigest = m->fManifest;
        storage.fSha256 = m->fSha256;
        int vrc = VDInterfaceAdd(&pFileIo->Core, "Appliance::IOFile",
                                 VDINTERFACETYPE_IO, 0, sizeof(VDINTERFACEIO),
                                 &storage.pVDImageIfaces);
        if (RT_FAILURE(vrc))
        {
            rc = E_FAIL;
            break;
        }
        rc = writeFSImpl(pTask, writeLock, pShaIo, &storage);
    }while(0);

    /* Cleanup */
    if (pShaIo)
        RTMemFree(pShaIo);
    if (pFileIo)
        RTMemFree(pFileIo);

    LogFlowFuncLeave();
    return rc;
}

HRESULT Appliance::writeFSOVA(TaskOVF *pTask, AutoWriteLockBase& writeLock)
{
    LogFlowFuncEnter();

    RTTAR tar;
    int vrc = RTTarOpen(&tar, pTask->locInfo.strPath.c_str(), RTFILE_O_CREATE | RTFILE_O_WRITE | RTFILE_O_DENY_ALL, false);
    if (RT_FAILURE(vrc))
        return setError(VBOX_E_FILE_ERROR,
                        tr("Could not create OVA file '%s' (%Rrc)"),
                        pTask->locInfo.strPath.c_str(), vrc);

    HRESULT rc = S_OK;

    PVDINTERFACEIO pShaIo = 0;
    PVDINTERFACEIO pTarIo = 0;
    do
    {
        pShaIo = ShaCreateInterface();
        if (!pShaIo)
        {
            rc = E_OUTOFMEMORY;
            break;
        }
        pTarIo = TarCreateInterface();
        if (!pTarIo)
        {
            rc = E_OUTOFMEMORY;
            break;
        }
        SHASTORAGE storage;
        RT_ZERO(storage);
        storage.fCreateDigest = m->fManifest;
        storage.fSha256 = m->fSha256;
        vrc = VDInterfaceAdd(&pTarIo->Core, "Appliance::IOTar",
                             VDINTERFACETYPE_IO, tar, sizeof(VDINTERFACEIO),
                             &storage.pVDImageIfaces);
        if (RT_FAILURE(vrc))
        {
            rc = E_FAIL;
            break;
        }
        rc = writeFSImpl(pTask, writeLock, pShaIo, &storage);
    }while(0);

    RTTarClose(tar);

    /* Cleanup */
    if (pShaIo)
        RTMemFree(pShaIo);
    if (pTarIo)
        RTMemFree(pTarIo);

    /* Delete ova file on error */
    if(FAILED(rc))
        RTFileDelete(pTask->locInfo.strPath.c_str());

    LogFlowFuncLeave();
    return rc;
}

HRESULT Appliance::writeFSImpl(TaskOVF *pTask, AutoWriteLockBase& writeLock, PVDINTERFACEIO pIfIo, PSHASTORAGE pStorage)
{
    LogFlowFuncEnter();

    HRESULT rc = S_OK;

    list<STRPAIR> fileList;
    try
    {
        int vrc;
        // the XML stack contains two maps for disks and networks, which allows us to
        // a) have a list of unique disk names (to make sure the same disk name is only added once)
        // and b) keep a list of all networks
        XMLStack stack;
        // Scope this to free the memory as soon as this is finished
        {
            // Create a xml document
            xml::Document doc;
            // Now fully build a valid ovf document in memory
            buildXML(writeLock, doc, stack, pTask->locInfo.strPath, pTask->enFormat);
            /* Extract the path */
            Utf8Str strOvfFile = Utf8Str(pTask->locInfo.strPath).stripExt().append(".ovf");
            // Create a memory buffer containing the XML. */
            void *pvBuf = 0;
            size_t cbSize;
            xml::XmlMemWriter writer;
            writer.write(doc, &pvBuf, &cbSize);
            if (RT_UNLIKELY(!pvBuf))
                throw setError(VBOX_E_FILE_ERROR,
                               tr("Could not create OVF file '%s'"),
                               strOvfFile.c_str());
            /* Write the ovf file to disk. */
            vrc = ShaWriteBuf(strOvfFile.c_str(), pvBuf, cbSize, pIfIo, pStorage);
            if (RT_FAILURE(vrc))
                throw setError(VBOX_E_FILE_ERROR,
                               tr("Could not create OVF file '%s' (%Rrc)"),
                               strOvfFile.c_str(), vrc);
            fileList.push_back(STRPAIR(strOvfFile, pStorage->strDigest));
        }

        // We need a proper format description
        ComObjPtr<MediumFormat> format;
        // Scope for the AutoReadLock
        {
            SystemProperties *pSysProps = mVirtualBox->getSystemProperties();
            AutoReadLock propsLock(pSysProps COMMA_LOCKVAL_SRC_POS);
            // We are always exporting to VMDK stream optimized for now
            format = pSysProps->mediumFormat("VMDK");
            if (format.isNull())
                throw setError(VBOX_E_NOT_SUPPORTED,
                               tr("Invalid medium storage format"));
        }

        // Finally, write out the disks!
        map<Utf8Str, const VirtualSystemDescriptionEntry*>::const_iterator itS;
        for (itS = stack.mapDisks.begin();
             itS != stack.mapDisks.end();
             ++itS)
        {
            const VirtualSystemDescriptionEntry *pDiskEntry = itS->second;

            // source path: where the VBox image is
            const Utf8Str &strSrcFilePath = pDiskEntry->strVboxCurrent;

            // Do NOT check here whether the file exists. findHardDisk will
            // figure that out, and filesystem-based tests are simply wrong
            // in the general case (think of iSCSI).

            // clone the disk:
            ComObjPtr<Medium> pSourceDisk;

            Log(("Finding source disk \"%s\"\n", strSrcFilePath.c_str()));
            rc = mVirtualBox->findHardDiskByLocation(strSrcFilePath, true, &pSourceDisk);
            if (FAILED(rc)) throw rc;

            Bstr uuidSource;
            rc = pSourceDisk->COMGETTER(Id)(uuidSource.asOutParam());
            if (FAILED(rc)) throw rc;
            Guid guidSource(uuidSource);

            // output filename
            const Utf8Str &strTargetFileNameOnly = pDiskEntry->strOvf;
            // target path needs to be composed from where the output OVF is
            Utf8Str strTargetFilePath(pTask->locInfo.strPath);
            strTargetFilePath.stripFilename()
                .append("/")
                .append(strTargetFileNameOnly);

            // The exporting requests a lock on the media tree. So leave our lock temporary.
            writeLock.release();
            try
            {
                ComObjPtr<Progress> pProgress2;
                pProgress2.createObject();
                rc = pProgress2->init(mVirtualBox, static_cast<IAppliance*>(this), BstrFmt(tr("Creating medium '%s'"), strTargetFilePath.c_str()).raw(), TRUE);
                if (FAILED(rc)) throw rc;

                // advance to the next operation
                pTask->pProgress->SetNextOperation(BstrFmt(tr("Exporting to disk image '%s'"), RTPathFilename(strTargetFilePath.c_str())).raw(),
                                                   pDiskEntry->ulSizeMB);     // operation's weight, as set up with the IProgress originally

                // create a flat copy of the source disk image
                rc = pSourceDisk->exportFile(strTargetFilePath.c_str(), format, MediumVariant_VmdkStreamOptimized, pIfIo, pStorage, pProgress2);
                if (FAILED(rc)) throw rc;

                ComPtr<IProgress> pProgress3(pProgress2);
                // now wait for the background disk operation to complete; this throws HRESULTs on error
                waitForAsyncProgress(pTask->pProgress, pProgress3);
            }
            catch (HRESULT rc3)
            {
                writeLock.acquire();
                // Todo: file deletion on error? If not, we can remove that whole try/catch block.
                throw rc3;
            }
            // Finished, lock again (so nobody mess around with the medium tree
            // in the meantime)
            writeLock.acquire();
            fileList.push_back(STRPAIR(strTargetFilePath, pStorage->strDigest));
        }

        if (m->fManifest)
        {
            // Create & write the manifest file
            Utf8Str strMfFilePath = Utf8Str(pTask->locInfo.strPath).stripExt().append(".mf");
            Utf8Str strMfFileName = Utf8Str(strMfFilePath).stripPath();
            pTask->pProgress->SetNextOperation(BstrFmt(tr("Creating manifest file '%s'"), strMfFileName.c_str()).raw(),
                                               m->ulWeightForManifestOperation);     // operation's weight, as set up with the IProgress originally);
            PRTMANIFESTTEST paManifestFiles = (PRTMANIFESTTEST)RTMemAlloc(sizeof(RTMANIFESTTEST) * fileList.size());
            size_t i = 0;
            list<STRPAIR>::const_iterator it1;
            for (it1 = fileList.begin();
                 it1 != fileList.end();
                 ++it1, ++i)
            {
                paManifestFiles[i].pszTestFile   = (*it1).first.c_str();
                paManifestFiles[i].pszTestDigest = (*it1).second.c_str();
            }
            void *pvBuf;
            size_t cbSize;
            vrc = RTManifestWriteFilesBuf(&pvBuf, &cbSize, m->fSha256 ? RTDIGESTTYPE_SHA256 : RTDIGESTTYPE_SHA1,
                                          paManifestFiles, fileList.size());
            RTMemFree(paManifestFiles);
            if (RT_FAILURE(vrc))
                throw setError(VBOX_E_FILE_ERROR,
                               tr("Could not create manifest file '%s' (%Rrc)"),
                               strMfFileName.c_str(), vrc);
            /* Disable digest creation for the manifest file. */
            pStorage->fCreateDigest = false;
            /* Write the manifest file to disk. */
            vrc = ShaWriteBuf(strMfFilePath.c_str(), pvBuf, cbSize, pIfIo, pStorage);
            RTMemFree(pvBuf);
            if (RT_FAILURE(vrc))
                throw setError(VBOX_E_FILE_ERROR,
                               tr("Could not create manifest file '%s' (%Rrc)"),
                               strMfFilePath.c_str(), vrc);
        }
    }
    catch (RTCError &x)  // includes all XML exceptions
    {
        rc = setError(VBOX_E_FILE_ERROR,
                      x.what());
    }
    catch (HRESULT aRC)
    {
        rc = aRC;
    }

    /* Cleanup on error */
    if (FAILED(rc))
    {
        list<STRPAIR>::const_iterator it1;
        for (it1 = fileList.begin();
             it1 != fileList.end();
             ++it1)
             pIfIo->pfnDelete(pStorage, (*it1).first.c_str());
    }

    LogFlowFunc(("rc=%Rhrc\n", rc));
    LogFlowFuncLeave();

    return rc;
}

#ifdef VBOX_WITH_S3
/**
 * Worker code for writing out OVF to the cloud. This is called from Appliance::taskThreadWriteOVF()
 * in S3 mode and therefore runs on the OVF write worker thread. This then starts a second worker
 * thread to create temporary files (see Appliance::writeFS()).
 *
 * @param pTask
 * @return
 */
HRESULT Appliance::writeS3(TaskOVF *pTask)
{
    LogFlowFuncEnter();
    LogFlowFunc(("Appliance %p\n", this));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT rc = S_OK;

    AutoWriteLock appLock(this COMMA_LOCKVAL_SRC_POS);

    int vrc = VINF_SUCCESS;
    RTS3 hS3 = NIL_RTS3;
    char szOSTmpDir[RTPATH_MAX];
    RTPathTemp(szOSTmpDir, sizeof(szOSTmpDir));
    /* The template for the temporary directory created below */
    char *pszTmpDir = RTPathJoinA(szOSTmpDir, "vbox-ovf-XXXXXX");
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
        vrc = RTDirCreateTemp(pszTmpDir, 0700);
        if (RT_FAILURE(vrc))
            throw setError(VBOX_E_FILE_ERROR,
                           tr("Cannot create temporary directory '%s' (%Rrc)"), pszTmpDir, vrc);

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
        appLock.release();
        /* Wait until the writing is done, but report the progress back to the
           caller */
        ComPtr<IProgress> progressInt(progress);
        waitForAsyncProgress(pTask->pProgress, progressInt); /* Any errors will be thrown */

        /* Again lock the appliance for the next steps */
        appLock.acquire();

        vrc = RTPathExists(strTmpOvf.c_str()); /* Paranoid check */
        if (RT_FAILURE(vrc))
            throw setError(VBOX_E_FILE_ERROR,
                           tr("Cannot find source file '%s' (%Rrc)"), strTmpOvf.c_str(), vrc);
        /* Add the OVF file */
        filesList.push_back(pair<Utf8Str, ULONG>(strTmpOvf, m->ulWeightForXmlOperation)); /* Use 1% of the total for the OVF file upload */
        /* Add the manifest file */
        if (m->fManifest)
        {
            Utf8Str strMfFile = Utf8Str(strTmpOvf).stripExt().append(".mf");
            filesList.push_back(pair<Utf8Str, ULONG>(strMfFile , m->ulWeightForXmlOperation)); /* Use 1% of the total for the manifest file upload */
        }

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
                if (RT_FAILURE(vrc))
                    throw setError(VBOX_E_FILE_ERROR,
                                   tr("Cannot find source file '%s' (%Rrc)"), strTargetFilePath.c_str(), vrc);
                filesList.push_back(pair<Utf8Str, ULONG>(strTargetFilePath, (*itH)->ulSizeMB));
            }
        }
        /* Next we have to upload the OVF & all disk images */
        vrc = RTS3Create(&hS3, pTask->locInfo.strUsername.c_str(), pTask->locInfo.strPassword.c_str(), pTask->locInfo.strHostname.c_str(), "virtualbox-agent/"VBOX_VERSION_STRING);
        if (RT_FAILURE(vrc))
            throw setError(VBOX_E_IPRT_ERROR,
                           tr("Cannot create S3 service handler"));
        RTS3SetProgressCallback(hS3, pTask->updateProgress, &pTask);

        /* Upload all files */
        for (list< pair<Utf8Str, ULONG> >::const_iterator it1 = filesList.begin(); it1 != filesList.end(); ++it1)
        {
            const pair<Utf8Str, ULONG> &s = (*it1);
            char *pszFilename = RTPathFilename(s.first.c_str());
            /* Advance to the next operation */
            pTask->pProgress->SetNextOperation(BstrFmt(tr("Uploading file '%s'"), pszFilename).raw(), s.second);
            vrc = RTS3PutKey(hS3, bucket.c_str(), pszFilename, s.first.c_str());
            if (RT_FAILURE(vrc))
            {
                if (vrc == VERR_S3_CANCELED)
                    break;
                else if (vrc == VERR_S3_ACCESS_DENIED)
                    throw setError(E_ACCESSDENIED,
                                   tr("Cannot upload file '%s' to S3 storage server (Access denied). Make sure that your credentials are right. Also check that your host clock is properly synced"), pszFilename);
                else if (vrc == VERR_S3_NOT_FOUND)
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
