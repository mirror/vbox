/** @file
 *
 * tstVBoxAPILinux - sample program to illustrate the VirtualBox
 *                   XPCOM API for machine management on Linux.
 *                   It only uses standard C/C++ and XPCOM semantics,
 *                   no additional VBox classes/macros/helpers.
 *
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#include <stdio.h>
#include <stdlib.h>
#include <iconv.h>
#include <errno.h>


/*
 * Include the XPCOM headers
 */
#include <nsIServiceManager.h>
#include <nsIComponentRegistrar.h>
#include <nsXPCOMGlue.h>
#include <nsMemory.h>
#include <nsString.h>
#include <nsIProgrammingLanguage.h>
#include <ipcIService.h>
#include <ipcCID.h>
#include <ipcIDConnectService.h>
#include <nsIEventQueueService.h>

/*
 * Some XPCOM declarations that haven't made it
 * into the official headers yet
 */
#define IPC_DCONNECTSERVICE_CONTRACTID \
    "@mozilla.org/ipc/dconnect-service;1"
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static nsIEventQueue* gEventQ = nsnull;


/*
 * VirtualBox XPCOM interface. This header is generated
 * from IDL which in turn is generated from a custom XML format.
 */
#include "VirtualBox_XPCOM.h"

/*
 * Prototypes
 */
char *nsIDToString(nsID *guid);



/**
 * Display all registered VMs on the screen with some information about each
 *
 * @param virtualBox VirtualBox instance object.
 */
void listVMs(IVirtualBox *virtualBox)
{
    nsresult rc;

    printf("----------------------------------------------------\n");
    printf("VM List:\n\n");

    /*
     * Get the list of all registered VMs
     */
    IMachineCollection *collection = nsnull;
    IMachineEnumerator *enumerator = nsnull;
    rc = virtualBox->GetMachines(&collection);
    if (NS_SUCCEEDED(rc))
        rc = collection->Enumerate(&enumerator);
    if (NS_SUCCEEDED(rc))
    {
        /*
         * Iterate through the collection
         */
        PRBool hasMore = false;
        while (enumerator->HasMore(&hasMore), hasMore)
        {
            IMachine *machine = nsnull;
            rc = enumerator->GetNext(&machine);
            if ((NS_SUCCEEDED(rc)) && machine)
            {
                nsXPIDLString machineName;
                machine->GetName(getter_Copies(machineName));
                char *machineNameAscii = ToNewCString(machineName);
                printf("\tName:        %s\n", machineNameAscii);
                free(machineNameAscii);

                nsID *iid = nsnull;
                machine->GetId(&iid);
                const char *uuidString = nsIDToString(iid);
                printf("\tUUID:        %s\n", uuidString);
                free((void*)uuidString);
                nsMemory::Free(iid);

                nsXPIDLString configFile;
                machine->GetSettingsFilePath(getter_Copies(configFile));
                char *configFileAscii = ToNewCString(configFile);
                printf("\tConfig file: %s\n", configFileAscii);
                free(configFileAscii);

                PRUint32 memorySize;
                machine->GetMemorySize(&memorySize);
                printf("\tMemory size: %uMB\n", memorySize);

                IGuestOSType *osType = nsnull;
                machine->GetOSType(&osType);
                nsXPIDLString osName;
                osType->GetDescription(getter_Copies(osName));
                char *osNameAscii = ToNewCString(osName);
                printf("\tGuest OS:    %s\n\n", osNameAscii);
                free(osNameAscii);
                osType->Release();

                machine->Release();
            }
        }
    }
    printf("----------------------------------------------------\n\n");
    /* don't forget to release the objects... */
    if (enumerator)
        enumerator->Release();
    if (collection)
        collection->Release();
}

/**
 * Create a sample VM
 *
 * @param virtualBox VirtualBox instance object.
 */
void createVM(IVirtualBox *virtualBox)
{
    nsresult rc;
    /*
     * First create a new VM. It will be unconfigured and not be saved
     * in the configuration until we explicitely choose to do so.
     */
    IMachine *machine;
    rc = virtualBox->CreateMachine(nsnull, NS_LITERAL_STRING("A brand new VM").get(), &machine);

    /*
     * Set some properties
     */
    /* alternative to illustrate the use of string classes */
    char vmName[] = "A new name";
    rc = machine->SetName(NS_ConvertUTF8toUTF16((char*)vmName).get());
    rc = machine->SetMemorySize(128);

    /*
     * Now a more advanced property -- the guest OS type. This is
     * an object by itself which has to be found first. Note that we
     * use the ID of the guest OS type here which is an internal
     * representation (you can find that by configuring the OS type of
     * a machine in the GUI and then looking at the <Guest ostype=""/>
     * setting in the XML file. It is also possible to get the OS type from
     * its description (win2k would be "Windows 2000") by getting the
     * guest OS type collection and enumerating it.
     */
    IGuestOSType *osType = nsnull;
    char win2k[] = "win2k";
    rc = virtualBox->FindGuestOSType(NS_ConvertUTF8toUTF16((char*)win2k).get(), &osType);
    if (NS_FAILED(rc) || !osType)
    {
        printf("Error: could not find guest OS type! rc = 0x%x\n", rc);
    }
    else
    {
        machine->SetOSType(osType);
        osType->Release();
    }

    /*
     * Create a virtual harddisk
     */
    IHardDisk *hardDisk = 0;
    IVirtualDiskImage *vdi = 0;
    char vdiName[] = "TestHardDisk.vdi";
    rc = virtualBox->CreateHardDisk(HardDiskStorageType::VirtualDiskImage,
                                    &hardDisk);
    if (NS_SUCCEEDED (rc))
    {
        rc = hardDisk->QueryInterface(NS_GET_IID(IVirtualDiskImage), (void **)&vdi);
        if (NS_SUCCEEDED (rc))
            rc = vdi->SetFilePath(NS_ConvertUTF8toUTF16((char*)vdiName).get());
    }

    if (NS_FAILED(rc) || !hardDisk || !vdi)
    {
        printf("Failed creating a hard disk object!\n");
    }
    else
    {
        /*
         * We have only created an object so far. No on disk representation exists
         * because none of its properties has been set so far. Let's continue creating
         * a dynamically expanding image.
         */
        IProgress *progress;
        rc = vdi->CreateDynamicImage(100,                                             // size in megabytes
                                     &progress);                                      // optional progress object
        if (NS_FAILED(rc) || !progress)
        {
            printf("Failed creating hard disk image!\n");
        }
        else
        {
            /*
             * Creating the image is done in the background because it can take quite
             * some time (at least fixed size images). We have to wait for its completion.
             * Here we wait forever (timeout -1)  which is potentially dangerous.
             */
            rc = progress->WaitForCompletion(-1);
            nsresult resultCode;
            progress->GetResultCode(&resultCode);
            progress->Release();
            if (NS_FAILED(rc) || (resultCode != 0))
            {
                printf("Error: could not create hard disk!\n");
            }
            else
            {
                /*
                 * Now we have to register the new hard disk with VirtualBox.
                 */
                rc = virtualBox->RegisterHardDisk(hardDisk);
                if (NS_FAILED(rc))
                {
                    printf("Error: could not register hard disk!\n");
                }
                else
                {
                    /*
                     * Now that it's registered, we can assign it to the VM. This is done
                     * by UUID, so query that one fist. The UUID has been assigned automatically
                     * when we've created the image.
                     */
                    nsID *vdiUUID = nsnull;
                    hardDisk->GetId(&vdiUUID);
                    rc = machine->AttachHardDisk(*vdiUUID,
                                                DiskControllerType::IDE0Controller, // controler identifier
                                                0);                                 // device number on the controller
                    nsMemory::Free(vdiUUID);
                    if (NS_FAILED(rc))
                    {
                        printf("Error: could not attach hard disk!\n");
                    }
                }
            }
        }
    }
    if (vdi)
        vdi->Release();
    if (hardDisk)
        hardDisk->Release();

    /*
     * It's got a hard disk but that one is new and thus not bootable. Make it
     * boot from an ISO file. This requires some processing. First the ISO file
     * has to be registered and then mounted to the VM's DVD drive and selected
     * as the boot device.
     */
    nsID uuid = {0};
    IDVDImage *dvdImage = nsnull;
    char isoFile[] = "/home/achimha/isoimages/winnt4ger.iso";

    rc = virtualBox->OpenDVDImage(NS_ConvertUTF8toUTF16((char*)isoFile).get(),
                                  uuid, /* NULL UUID, i.e. a new one will be created */
                                  &dvdImage);
    if (NS_FAILED(rc) || !dvdImage)
    {
        printf("Error: could not open CD image!\n");
    }
    else
    {
        /*
         * Register it with VBox
         */
        rc = virtualBox->RegisterDVDImage(dvdImage);
        if (NS_FAILED(rc) || !dvdImage)
        {
            printf("Error: could not register CD image!\n");
        }
        else
        {
            /*
             * Now assign it to our VM
             */
            nsID *isoUUID = nsnull;
            dvdImage->GetId(&isoUUID);
            IDVDDrive *dvdDrive = nsnull;
            machine->GetDVDDrive(&dvdDrive);
            rc = dvdDrive->MountImage(*isoUUID);
            nsMemory::Free(isoUUID);
            if (NS_FAILED(rc))
            {
                printf("Error: could not mount ISO image!\n");
            }
            else
            {
                /*
                 * Last step: tell the VM to boot from the CD.
                 */
                rc = machine->SetBootOrder (1, DeviceType::DVDDevice);
                if (NS_FAILED(rc))
                {
                    printf("Could not set boot device!\n");
                }
            }
            dvdDrive->Release();
            dvdImage->Release();
        }
    }

    /*
     * Register the VM. Note that this call also saves the VM config
     * to disk. It is also possible to save the VM settings but not
     * register the VM.
     */
    virtualBox->RegisterMachine(machine);

    machine->Release();
}

// main
///////////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[])
{
    nsresult rc;

    /*
     * This is the standard XPCOM init processing.
     * What we do is just follow the required steps to get an instance
     * of our main interface, which is IVirtualBox
     */
    XPCOMGlueStartup(nsnull);
    nsCOMPtr<nsIServiceManager> serviceManager;
    rc = NS_InitXPCOM2(getter_AddRefs(serviceManager), nsnull, nsnull);
    if (NS_FAILED(rc))
    {
        printf("Error: XPCOM could not be initialized! rc=0x%x\n", rc);
        return -1;
    }

    // register our component
    nsCOMPtr<nsIComponentRegistrar> registrar = do_QueryInterface(serviceManager);
    if (!registrar)
    {
        printf("Error: could not query nsIComponentRegistrar interface!\n");
        return -1;
    }
    registrar->AutoRegister(nsnull);

    // Create the Event Queue for this thread
    nsCOMPtr<nsIEventQueueService> eqs = do_GetService(kEventQueueServiceCID, &rc);
    if (NS_FAILED( rc ))
    {
        printf("Error: could not get event queue service! rc=%08X\n", rc);
        return -1;
    }

    rc = eqs->CreateThreadEventQueue();
    if (NS_FAILED( rc ))
    {
        printf("Error: could not create thread event queue! rc=%08X\n", rc);
        return -1;
    }

    rc = eqs->GetThreadEventQueue(NS_CURRENT_THREAD, &gEventQ);
    if (NS_FAILED( rc ))
    {
        printf("Error: could not get tread event queue! rc=%08X\n", rc);
        return -1;
    }

    // get ipc service
    nsCOMPtr<ipcIService> ipcServ = do_GetService(IPC_SERVICE_CONTRACTID, &rc);
    if (NS_FAILED( rc ))
    {
        printf("Error: could not get ipc service! rc=%08X\n", rc);
        return -1;
    }

    // get dconnect service
    nsCOMPtr<ipcIDConnectService> dcon = do_GetService(IPC_DCONNECTSERVICE_CONTRACTID, &rc);
    if (NS_FAILED( rc ))
    {
        printf("Error: could not get dconnect service! rc=%08X\n", rc);
        return -1;
    }

    PRUint32 serverID = 0;
    rc = ipcServ->ResolveClientName("VirtualBoxServer", &serverID);
    if (NS_FAILED( rc ))
    {
        printf("Error: could not get VirtualBox server ID! rc=%08X\n", rc);
        return -1;
    }

    nsCOMPtr<nsIComponentManager> manager = do_QueryInterface(registrar);
    if (!manager)
    {
        printf("Error: could not query nsIComponentManager interface!\n");
        return -1;
    }

    /*
     * Now XPCOM is ready and we can start to do real work.
     * IVirtualBox is the root interface of VirtualBox and will be
     * retrieved from the XPCOM component registrar. We use the
     * XPCOM provided smart pointer nsCOMPtr for all objects because
     * that's very convenient and removes the need deal with reference
     * counting and freeing.
     */
    nsCOMPtr<IVirtualBox> virtualBox;
    rc = dcon->CreateInstanceByContractID(serverID, NS_VIRTUALBOX_CONTRACTID,
                                          NS_GET_IID(IVirtualBox),
                                          getter_AddRefs(virtualBox));
    if (NS_FAILED(rc))
    {
        printf("Error, could not instantiate VirtualBox object! rc=0x%x\n", rc);
        return -1;
    }
    printf("VirtualBox object created\n");

    ////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////


    listVMs(virtualBox);

    createVM(virtualBox);


    ////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////

    /* this is enough to free the IVirtualBox instance -- smart pointers rule! */
    virtualBox = nsnull;

    /*
     * Process events that might have queued up in the XPCOM event
     * queue. If we don't process them, the server might hang.
     */
    gEventQ->ProcessPendingEvents();

    /*
     * Perform the standard XPCOM shutdown procedure
     */
    NS_ShutdownXPCOM(nsnull);
    XPCOMGlueShutdown();
    printf("Done!\n");
    return 0;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////
//// Helpers
//////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Helper function to convert an nsID into a human readable string
 *
 * @returns result string, allocated. Has to be freed using free()
 * @param   guid Pointer to nsID that will be converted.
 */
char *nsIDToString(nsID *guid)
{
    char *res = (char*)malloc(39);

    if (res != NULL)
    {
        snprintf(res, 39, "{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
                 guid->m0, (PRUint32)guid->m1, (PRUint32)guid->m2,
                 (PRUint32)guid->m3[0], (PRUint32)guid->m3[1], (PRUint32)guid->m3[2],
                 (PRUint32)guid->m3[3], (PRUint32)guid->m3[4], (PRUint32)guid->m3[5],
                 (PRUint32)guid->m3[6], (PRUint32)guid->m3[7]);
    }
    return res;
}
