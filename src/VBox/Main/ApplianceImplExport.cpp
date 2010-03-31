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
#include <iprt/dir.h>
#include <iprt/param.h>
#include <iprt/s3.h>
#include <iprt/manifest.h>

#include <VBox/version.h>

#include "ApplianceImpl.h"
#include "VirtualBoxImpl.h"

#include "ProgressImpl.h"
#include "MachineImpl.h"

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

STDMETHODIMP Machine::Export(IAppliance *aAppliance, IVirtualSystemDescription **aDescription)
{
    HRESULT rc = S_OK;

    if (!aAppliance)
        return E_POINTER;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ComObjPtr<VirtualSystemDescription> pNewDesc;

    try
    {
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

        // request the machine lock while acessing internal members
        AutoReadLock alock1(this COMMA_LOCKVAL_SRC_POS);

        pAudioAdapter = mAudioAdapter;
        rc = pAudioAdapter->COMGETTER(Enabled)(&fAudioEnabled);
        if (FAILED(rc)) throw rc;
        rc = pAudioAdapter->COMGETTER(AudioController)(&audioController);
        if (FAILED(rc)) throw rc;

        // get name
        bstrName1 = mUserData->mName;
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

        /* Guest OS type */
        Utf8Str strOsTypeVBox(bstrGuestOSType);
        CIMOSType_T cim = convertVBoxOSType2CIMOSType(strOsTypeVBox.c_str());
        pNewDesc->addEntry(VirtualSystemDescriptionType_OS,
                           "",
                           Utf8StrFmt("%RI32", cim),
                           strOsTypeVBox);

        /* VM name */
        Utf8Str strVMName(bstrName1);
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
        Utf8Str strMemory = Utf8StrFmt("%RI64", (uint64_t)ulMemSizeMB * _1M);
        pNewDesc->addEntry(VirtualSystemDescriptionType_Memory,
                           "",
                           strMemory,
                           strMemory);

        int32_t lIDEControllerIndex = 0;
        int32_t lSATAControllerIndex = 0;
        int32_t lSCSIControllerIndex = 0;

        /* Fetch all available storage controllers */
        com::SafeIfaceArray<IStorageController> nwControllers;
        rc = COMGETTER(StorageControllers)(ComSafeArrayAsOutParam(nwControllers));
        if (FAILED(rc)) throw rc;

        ComPtr<IStorageController> pIDEController;
#ifdef VBOX_WITH_AHCI
        ComPtr<IStorageController> pSATAController;
#endif /* VBOX_WITH_AHCI */
#ifdef VBOX_WITH_LSILOGIC
        ComPtr<IStorageController> pSCSIController;
#endif /* VBOX_WITH_LSILOGIC */
        for (size_t j = 0; j < nwControllers.size(); ++j)
        {
            StorageBus_T eType;
            rc = nwControllers[j]->COMGETTER(Bus)(&eType);
            if (FAILED(rc)) throw rc;
            if (   eType == StorageBus_IDE
                && pIDEController.isNull())
                pIDEController = nwControllers[j];
#ifdef VBOX_WITH_AHCI
            else if (   eType == StorageBus_SATA
                     && pSATAController.isNull())
                pSATAController = nwControllers[j];
#endif /* VBOX_WITH_AHCI */
#ifdef VBOX_WITH_LSILOGIC
            else if (   eType == StorageBus_SCSI
                     && pSATAController.isNull())
                pSCSIController = nwControllers[j];
#endif /* VBOX_WITH_LSILOGIC */
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
                lIDEControllerIndex = (int32_t)pNewDesc->m->llDescriptions.size();
                pNewDesc->addEntry(VirtualSystemDescriptionType_HardDiskControllerIDE,
                                   Utf8StrFmt("%d", lIDEControllerIndex),
                                   strVbox,
                                   strVbox);
            }
        }

#ifdef VBOX_WITH_AHCI
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
#endif // VBOX_WITH_AHCI

#ifdef VBOX_WITH_LSILOGIC
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
#endif // VBOX_WITH_LSILOGIC

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

            rc = GetStorageControllerByName(controllerName, ctl.asOutParam());
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
            ULONG64 ullSize = 0;

            if (    deviceType == DeviceType_HardDisk
                 && pMedium
               )
            {
                Bstr bstrLocation;
                rc = pMedium->COMGETTER(Location)(bstrLocation.asOutParam());
                if (FAILED(rc)) throw rc;
                strLocation = bstrLocation;

                Bstr bstrName;
                rc = pMedium->COMGETTER(Name)(bstrName.asOutParam());
                if (FAILED(rc)) throw rc;

                strTargetVmdkName = bstrName;
                strTargetVmdkName.stripExt();
                strTargetVmdkName.append(".vmdk");

                // we need the size of the image so we can give it to addEntry();
                // later, on export, the progress weight will be based on this.
                // pMedium can be a differencing image though; in that case, we
                // need to use the size of the base instead.
                ComPtr<IMedium> pBaseMedium;
                rc = pMedium->COMGETTER(Base)(pBaseMedium.asOutParam());
                        // returns pMedium if there are no diff images
                if (FAILED(rc)) throw rc;

                // force reading state, or else size will be returned as 0
                MediumState_T ms;
                rc = pBaseMedium->RefreshState(&ms);
                if (FAILED(rc)) throw rc;

                rc = pBaseMedium->COMGETTER(Size)(&ullSize);
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

                    if (lChannel == 0 && lDevice == 0)      // primary master
                        lChannelVsys = 0;
                    else if (lChannel == 0 && lDevice == 1) // primary slave
                        lChannelVsys = 1;
                    else if (lChannel == 1 && lDevice == 0) // secondary master; by default this is the CD-ROM but as of VirtualBox 3.1 that can change
                        lChannelVsys = 2;
                    else if (lChannel == 1 && lDevice == 1) // secondary slave
                        lChannelVsys = 3;
                    else
                        throw setError(VBOX_E_NOT_SUPPORTED,
                                    tr("Cannot handle medium attachment: channel is %d, device is %d"), lChannel, lDevice);

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
                    Log(("Adding VirtualSystemDescriptionType_HardDiskImage, disk size: %RI64\n", ullSize));
                    pNewDesc->addEntry(VirtualSystemDescriptionType_HardDiskImage,
                                       strTargetVmdkName,   // disk ID: let's use the name
                                       strTargetVmdkName,   // OVF value:
                                       strLocation, // vbox value: media path
                                       (uint32_t)(ullSize / _1M),
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
            pNewDesc->addEntry(VirtualSystemDescriptionType_SoundCard,
                               "",
                               "ensoniq1371",       // this is what OVFTool writes and VMware supports
                               Utf8StrFmt("%RI32", audioController));

        // finally, add the virtual system to the appliance
        Appliance *pAppliance = static_cast<Appliance*>(aAppliance);
        AutoCaller autoCaller1(pAppliance);
        if (FAILED(autoCaller1.rc())) return autoCaller1.rc();

        /* We return the new description to the caller */
        ComPtr<IVirtualSystemDescription> copy(pNewDesc);
        copy.queryInterfaceTo(aDescription);

        AutoWriteLock alock(pAppliance COMMA_LOCKVAL_SRC_POS);

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
STDMETHODIMP Appliance::Write(IN_BSTR format, IN_BSTR path, IProgress **aProgress)
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
    if (!strPath.endsWith(".ovf", Utf8Str::CaseInsensitive))
        return setError(VBOX_E_FILE_ERROR,
                        tr("Appliance file must have .ovf extension"));

    Utf8Str strFormat(format);
    OVFFormat ovfF;
    if (strFormat == "ovf-0.9")
        ovfF = OVF_0_9;
    else if (strFormat == "ovf-1.0")
        ovfF = OVF_1_0;
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

////////////////////////////////////////////////////////////////////////////////
//
// Appliance private methods
//
////////////////////////////////////////////////////////////////////////////////

/**
 * Implementation for writing out the OVF to disk. This starts a new thread which will call
 * Appliance::taskThreadWriteOVF().
 *
 * This is in a separate private method because it is used from two locations:
 *
 * 1) from the public Appliance::Write().
 * 2) from Appliance::writeS3(), which got called from a previous instance of Appliance::taskThreadWriteOVF().
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
        Bstr progressDesc = BstrFmt(tr("Export appliance '%s'"),
                                    aLocInfo.strPath.c_str());

        /* todo: This progress init stuff should be done a little bit more generic */
        if (aLocInfo.storageType == VFSType_File)
            rc = setUpProgressFS(aProgress, progressDesc);
        else
            rc = setUpProgressWriteS3(aProgress, progressDesc);

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
 *
 * @param vsdescThis
 */
void Appliance::buildXMLForOneVirtualSystem(xml::ElementNode &elmToAddVirtualSystemsTo,
                                            ComObjPtr<VirtualSystemDescription> &vsdescThis,
                                            OVFFormat enFormat,
                                            XMLStack &stack)
{
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
        if (enFormat == OVF_0_9)
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
        if (enFormat == OVF_0_9)
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
    if (enFormat == OVF_0_9)
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

    uint32_t idIDEController = 0;
    int32_t lIDEControllerIndex = 0;
    uint32_t idSATAController = 0;
    int32_t lSATAControllerIndex = 0;
    uint32_t idSCSIController = 0;
    int32_t lSCSIControllerIndex = 0;

    uint32_t ulInstanceID = 1;

    for (size_t uLoop = 1; uLoop <= 2; ++uLoop)
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
                        uint32_t cDisks = stack.mapDisks.size();
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

                        stack.mapDisks[strDiskID] = &desc;
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
                    if (enFormat == OVF_1_0)
                        pItem->createChild("rasd:ElementName")->addContent(strCaption);
                }

                if (!strDescription.isEmpty())
                    pItem->createChild("rasd:Description")->addContent(strDescription);

                // <rasd:InstanceID>1</rasd:InstanceID>
                xml::ElementNode *pelmInstanceID;
                if (enFormat == OVF_0_9)
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
                    if (enFormat == OVF_0_9) // BusNumber is invalid OVF 1.0 so only write it in 0.9 mode for OVFTool compatibility
                        pItem->createChild("rasd:BusNumber")->addContent(Utf8StrFmt("%d", lBusNumber));

                if (ulParent)
                    pItem->createChild("rasd:Parent")->addContent(Utf8StrFmt("%d", ulParent));
                if (lAddressOnParent != -1)
                    pItem->createChild("rasd:AddressOnParent")->addContent(Utf8StrFmt("%d", lAddressOnParent));
            }
        }
    } // for (size_t uLoop = 1; uLoop <= 2; ++uLoop)

    // now that we're done with the official OVF <Item> tags under <VirtualSystem>, write out VirtualBox XML
    // under the vbox: namespace
    xml::ElementNode *pelmVBoxMachine = pelmVirtualSystem->createChild("vbox:Machine");
    settings::MachineConfigFile *pConfig = new settings::MachineConfigFile(NULL);

    try
    {
        AutoWriteLock machineLock(vsdescThis->m->pMachine COMMA_LOCKVAL_SRC_POS);
        vsdescThis->m->pMachine->copyMachineDataToSettings(*pConfig);
        pConfig->buildMachineXML(*pelmVBoxMachine);
        delete pConfig;
    }
    catch (...)
    {
        delete pConfig;
        throw;
    }
}

/**
 * Actual worker code for writing out OVF to disk. This is called from Appliance::taskThreadWriteOVF()
 * and therefore runs on the OVF write worker thread. This runs in two contexts:
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
HRESULT Appliance::writeFS(const LocationInfo &locInfo, const OVFFormat enFormat, ComObjPtr<Progress> &pProgress)
{
    LogFlowFuncEnter();
    LogFlowFunc(("Appliance %p\n", this));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT rc = S_OK;

    try
    {
        AutoMultiWriteLock2 multiLock(&mVirtualBox->getMediaTreeLockHandle(), this->lockHandle() COMMA_LOCKVAL_SRC_POS);

        xml::Document doc;
        xml::ElementNode *pelmRoot = doc.createRootElement("Envelope");

        pelmRoot->setAttribute("ovf:version", (enFormat == OVF_1_0) ? "1.0" : "0.9");
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
                <Disk ovf:capacity="4294967296" ovf:diskId="lamp" ovf:format="http://www.vmware.com/specifications/vmdk.html#compressed" ovf:populatedSize="1924967692"/>
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

        // the XML stack contains two maps for disks and networks, which allows us to
        // a) have a list of unique disk names (to make sure the same disk name is only added once)
        // and b) keep a list of all networks
        XMLStack stack;

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

        // This can take a very long time so leave the locks; in particular, we have the media tree
        // lock which Medium::CloneTo() will request, and that would deadlock. Instead, protect
        // the appliance by resetting its state so we can safely leave the lock
        m->state = Data::ApplianceExporting;
        multiLock.release();

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

        list< ComObjPtr<VirtualSystemDescription> >::const_iterator it;
        /* Iterate throughs all virtual systems of that appliance */
        for (it = m->virtualSystemDescriptions.begin();
             it != m->virtualSystemDescriptions.end();
             ++it)
        {
            ComObjPtr<VirtualSystemDescription> vsdescThis = *it;
            buildXMLForOneVirtualSystem(*pelmToAddVirtualSystemsTo,
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

        // Finally, write out the disks!

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
            Utf8Str strTargetFilePath(locInfo.strPath);
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
                if (!pProgress.isNull())
                    pProgress->SetNextOperation(BstrFmt(tr("Exporting virtual disk image '%s'"), strSrcFilePath.c_str()),
                                                pDiskEntry->ulSizeMB);     // operation's weight, as set up with the IProgress originally);

                // now wait for the background disk operation to complete; this throws HRESULTs on error
                waitForAsyncProgress(pProgress, pProgress2);
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
        writer.write(locInfo.strPath.c_str());

        /* Create & write the manifest file */
        const char** ppManifestFiles = (const char**)RTMemAlloc(sizeof(char*)*diskList.size() + 1);
        ppManifestFiles[0] = locInfo.strPath.c_str();
        list<Utf8Str>::const_iterator it1;
        size_t i = 1;
        for (it1 = diskList.begin();
             it1 != diskList.end();
             ++it1, ++i)
            ppManifestFiles[i] = (*it1).c_str();
        Utf8Str strMfFile = manifestFileName(locInfo.strPath.c_str());
        int vrc = RTManifestWriteFiles(strMfFile.c_str(), ppManifestFiles, diskList.size()+1);
        RTMemFree(ppManifestFiles);
        if (RT_FAILURE(vrc))
            throw setError(VBOX_E_FILE_ERROR,
                           tr("Couldn't create manifest file '%s' (%Rrc)"),
                           RTPathFilename(strMfFile.c_str()), vrc);
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

    AutoWriteLock appLock(this COMMA_LOCKVAL_SRC_POS);
    // reset the state so others can call methods again
    m->state = Data::ApplianceIdle;

    LogFlowFunc(("rc=%Rhrc\n", rc));
    LogFlowFuncLeave();

    return rc;
}

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
        appLock.release();
        /* Wait until the writing is done, but report the progress back to the
           caller */
        ComPtr<IProgress> progressInt(progress);
        waitForAsyncProgress(pTask->pProgress, progressInt); /* Any errors will be thrown */

        /* Again lock the appliance for the next steps */
        appLock.acquire();

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
            if (!pTask->pProgress.isNull())
                pTask->pProgress->SetNextOperation(BstrFmt(tr("Uploading file '%s'"), pszFilename), s.second);
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

    LogFlowFunc(("rc=%Rhrc\n", rc));
    LogFlowFuncLeave();

    return rc;
}

