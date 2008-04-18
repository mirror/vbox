/** @file
 *
 * tstVBoxAPILinux - sample program to illustrate the VirtualBox
 *                   XPCOM API for machine management on Linux.
 *                   It only uses standard C/C++ and XPCOM semantics,
 *                   no additional VBox classes/macros/helpers.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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

/*
 * PURPOSE OF THIS SAMPLE PROGRAM
 * ------------------------------
 *
 * This sample program is intended to demonstrate the minimal code necessary
 * to use VirtualBox XPCOM API for learning puroses only. The program uses
 * pure XPCOM and doesn't have any extra dependencies to let you better
 * understand what is going on when a client talks to the VirtualBox core
 * using the XPCOM framework.
 *
 * However, if you want to write a real application, it is highly recommended
 * to use our MS COM XPCOM Glue library and helper C++ classes. This way, you
 * will get at least the following benefits:
 *
 * a) better portability: both the MS COM (used on Windows) and XPCOM (used
 *    everywhere else) VirtualBox client application from the same source code
 *    (including common smart C++ templates for automatic interface pointer
 *    reference counter and string data management);
 * b) simpler XPCOM initialization and shutdown (only a signle method call
 *    that does everything right).
 *
 * Currently, there is no separate sample program that uses the VirtualBox MS
 * COM XPCOM Glue library. Please refer to the sources of stock VirtualBox
 * applications such as the VirtualBox GUI frontend or the VBoxManage command
 * line frontend.
 *
 *
 * RUNNING THIS SAMPLE PROGRAM
 * ---------------------------
 *
 * This sample program needs to know where the VirtualBox core files reside
 * and where to search for VirtualBox shared libraries. Therefore, you need to
 * use the following (or similar) command to execute it:
 *
 *   $ env VBOX_XPCOM_HOME=../../.. LD_LIBRARY_PATH=../../.. ./tstVBoxAPILinux
 *
 * The above command assumes that VBoxRT.so, VBoxXPCOM.so and others reside in
 * the directory ../../..
 */


#include <stdio.h>
#include <stdlib.h>
#include <iconv.h>
#include <errno.h>

/*
 * Include the XPCOM headers
 */

#if defined(XPCOM_GLUE)
#include <nsXPCOMGlue.h>
#endif

#include <nsMemory.h>
#include <nsString.h>
#include <nsIServiceManager.h>
#include <nsEventQueueUtils.h>

#include <nsIExceptionService.h>

/*
 * VirtualBox XPCOM interface. This header is generated
 * from IDL which in turn is generated from a custom XML format.
 */
#include "VirtualBox_XPCOM.h"

/*
 * Prototypes
 */

char *nsIDToString(nsID *guid);
void printErrorInfo();


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
                PRBool isAccessible = PR_FALSE;
                machine->GetAccessible (&isAccessible);

                if (isAccessible)
                {
                    nsXPIDLString machineName;
                    machine->GetName(getter_Copies(machineName));
                    char *machineNameAscii = ToNewCString(machineName);
                    printf("\tName:        %s\n", machineNameAscii);
                    free(machineNameAscii);
                }
                else
                {
                    printf("\tName:        <inaccessible>\n");
                }

                nsID *iid = nsnull;
                machine->GetId(&iid);
                const char *uuidString = nsIDToString(iid);
                printf("\tUUID:        %s\n", uuidString);
                free((void*)uuidString);
                nsMemory::Free(iid);

                if (isAccessible)
                {
                    nsXPIDLString configFile;
                    machine->GetSettingsFilePath(getter_Copies(configFile));
                    char *configFileAscii = ToNewCString(configFile);
                    printf("\tConfig file: %s\n", configFileAscii);
                    free(configFileAscii);

                    PRUint32 memorySize;
                    machine->GetMemorySize(&memorySize);
                    printf("\tMemory size: %uMB\n", memorySize);

                    nsXPIDLString typeId;
                    machine->GetOSTypeId(getter_Copies(typeId));
                    IGuestOSType *osType = nsnull;
                    virtualBox->GetGuestOSType (typeId.get(), &osType);
                    nsXPIDLString osName;
                    osType->GetDescription(getter_Copies(osName));
                    char *osNameAscii = ToNewCString(osName);
                    printf("\tGuest OS:    %s\n\n", osNameAscii);
                    free(osNameAscii);
                    osType->Release();
                }

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
     * First create a unnamed new VM. It will be unconfigured and not be saved
     * in the configuration until we explicitely choose to do so.
     */
    nsID VMuuid = {0};
    nsCOMPtr <IMachine> machine;
    rc = virtualBox->CreateMachine(nsnull, NS_LITERAL_STRING("A brand new name").get(),
                                   VMuuid, getter_AddRefs(machine));
    if (NS_FAILED(rc))
    {
        printf("Error: could not create machine! rc=%08X\n", rc);
        return;
    }

    /*
     * Set some properties
     */
    /* alternative to illustrate the use of string classes */
    rc = machine->SetName(NS_ConvertUTF8toUTF16("A new name").get());
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
    nsCOMPtr <IGuestOSType> osType;
    rc = virtualBox->GetGuestOSType(NS_LITERAL_STRING("win2k").get(),
                                    getter_AddRefs(osType));
    if (NS_FAILED(rc))
    {
        printf("Error: could not find guest OS type! rc=%08X\n", rc);
    }
    else
    {
        machine->SetOSTypeId (NS_LITERAL_STRING("win2k").get());
    }

    /*
     * Register the VM. Note that this call also saves the VM config
     * to disk. It is also possible to save the VM settings but not
     * register the VM.
     *
     * Also note that due to current VirtualBox limitations, the machine
     * must be registered *before* we can attach hard disks to it.
     */
    rc = virtualBox->RegisterMachine(machine);
    if (NS_FAILED(rc))
    {
        printf("Error: could not register machine! rc=%08X\n", rc);
        printErrorInfo();
        return;
    }

    /*
     * In order to manipulate the registered machine, we must open a session
     * for that machine. Do it now.
     */
    nsCOMPtr<ISession> session;
    {
        nsCOMPtr<nsIComponentManager> manager;
        rc = NS_GetComponentManager (getter_AddRefs (manager));
        if (NS_FAILED(rc))
        {
            printf("Error: could not get component manager! rc=%08X\n", rc);
            return;
        }
        rc = manager->CreateInstanceByContractID (NS_SESSION_CONTRACTID,
                                                  nsnull,
                                                  NS_GET_IID(ISession),
                                                  getter_AddRefs(session));
        if (NS_FAILED(rc))
        {
            printf("Error, could not instantiate Session object! rc=0x%x\n", rc);
            return;
        }

        nsID *machineUUID = nsnull;
        machine->GetId(&machineUUID);
        rc = virtualBox->OpenSession(session, *machineUUID);
        nsMemory::Free(machineUUID);
        if (NS_FAILED(rc))
        {
            printf("Error, could not open session! rc=0x%x\n", rc);
            return;
        }

        /*
         * After the machine is registered, the initial machine object becomes
         * immutable. In order to get a mutable machine object, we must query
         * it from the opened session object.
         */
        rc = session->GetMachine(getter_AddRefs(machine));
        if (NS_FAILED(rc))
        {
            printf("Error, could not get sessioned machine! rc=0x%x\n", rc);
            return;
        }
    }

    /*
     * Create a virtual harddisk
     */
    nsCOMPtr<IHardDisk> hardDisk = 0;
    nsCOMPtr<IVirtualDiskImage> vdi = 0;
    rc = virtualBox->CreateHardDisk(HardDiskStorageType::VirtualDiskImage,
                                    getter_AddRefs(hardDisk));
    if (NS_SUCCEEDED (rc))
    {
        rc = hardDisk->QueryInterface(NS_GET_IID(IVirtualDiskImage),
                                      (void **)(getter_AddRefs(vdi)));
        if (NS_SUCCEEDED (rc))
            rc = vdi->SetFilePath(NS_LITERAL_STRING("TestHardDisk.vdi").get());
    }

    if (NS_FAILED(rc))
    {
        printf("Failed creating a hard disk object! rc=%08X\n", rc);
    }
    else
    {
        /*
         * We have only created an object so far. No on disk representation exists
         * because none of its properties has been set so far. Let's continue creating
         * a dynamically expanding image.
         */
        nsCOMPtr <IProgress> progress;
        rc = vdi->CreateDynamicImage(100,                                             // size in megabytes
                                     getter_AddRefs(progress));                       // optional progress object
        if (NS_FAILED(rc))
        {
            printf("Failed creating hard disk image! rc=%08X\n", rc);
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
            if (NS_FAILED(rc) || NS_FAILED(resultCode))
            {
                printf("Error: could not create hard disk! rc=%08X\n",
                       NS_FAILED(rc) ? rc : resultCode);
            }
            else
            {
                /*
                 * Now we have to register the new hard disk with VirtualBox.
                 */
                rc = virtualBox->RegisterHardDisk(hardDisk);
                if (NS_FAILED(rc))
                {
                    printf("Error: could not register hard disk! rc=%08X\n", rc);
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
                                                StorageBus::IDE, // controler identifier
                                                0,               // channel number on the controller
                                                0);              // device number on the controller
                    nsMemory::Free(vdiUUID);
                    if (NS_FAILED(rc))
                    {
                        printf("Error: could not attach hard disk! rc=%08X\n", rc);
                    }
                }
            }
        }
    }

    /*
     * It's got a hard disk but that one is new and thus not bootable. Make it
     * boot from an ISO file. This requires some processing. First the ISO file
     * has to be registered and then mounted to the VM's DVD drive and selected
     * as the boot device.
     */
    nsID uuid = {0};
    nsCOMPtr<IDVDImage> dvdImage;

    rc = virtualBox->OpenDVDImage(NS_LITERAL_STRING("/home/achimha/isoimages/winnt4ger.iso").get(),
                                  uuid, /* NULL UUID, i.e. a new one will be created */
                                  getter_AddRefs(dvdImage));
    if (NS_FAILED(rc))
    {
        printf("Error: could not open CD image! rc=%08X\n", rc);
    }
    else
    {
        /*
         * Register it with VBox
         */
        rc = virtualBox->RegisterDVDImage(dvdImage);
        if (NS_FAILED(rc))
        {
            printf("Error: could not register CD image! rc=%08X\n", rc);
        }
        else
        {
            /*
             * Now assign it to our VM
             */
            nsID *isoUUID = nsnull;
            dvdImage->GetId(&isoUUID);
            nsCOMPtr<IDVDDrive> dvdDrive;
            machine->GetDVDDrive(getter_AddRefs(dvdDrive));
            rc = dvdDrive->MountImage(*isoUUID);
            nsMemory::Free(isoUUID);
            if (NS_FAILED(rc))
            {
                printf("Error: could not mount ISO image! rc=%08X\n", rc);
            }
            else
            {
                /*
                 * Last step: tell the VM to boot from the CD.
                 */
                rc = machine->SetBootOrder (1, DeviceType::DVD);
                if (NS_FAILED(rc))
                {
                    printf("Could not set boot device! rc=%08X\n", rc);
                }
            }
        }
    }

    /*
     * Save all changes we've just made.
     */
    rc = machine->SaveSettings();
    if (NS_FAILED(rc))
    {
        printf("Could not save machine settings! rc=%08X\n", rc);
    }

    /*
     * It is always important to close the open session when it becomes not
     * necessary any more.
     */
    session->Close();
}

// main
///////////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[])
{
    /*
     * Check that PRUnichar is equal in size to what compiler composes L""
     * strings from; otherwise NS_LITERAL_STRING macros won't work correctly
     * and we will get a meaningless SIGSEGV. This, of course, must be checked
     * at compile time in xpcom/string/nsTDependentString.h, but XPCOM lacks
     * compile-time assert macros and I'm not going to add them now.
     */
    if (sizeof(PRUnichar) != sizeof(wchar_t))
    {
        printf("Error: sizeof(PRUnichar) {%d} != sizeof(wchar_t) {%d}!\n"
               "Probably, you forgot the -fshort-wchar compiler option.\n",
               sizeof(PRUnichar), sizeof(wchar_t));
        return -1;
    }

    nsresult rc;

    /*
     * This is the standard XPCOM init procedure.
     * What we do is just follow the required steps to get an instance
     * of our main interface, which is IVirtualBox.
     */
#if defined(XPCOM_GLUE)
    XPCOMGlueStartup(nsnull);
#endif

    /*
     * Note that we scope all nsCOMPtr variables in order to have all XPCOM
     * objects automatically released before we call NS_ShutdownXPCOM at the
     * end. This is an XPCOM requirement.
     */
    {
        nsCOMPtr<nsIServiceManager> serviceManager;
        rc = NS_InitXPCOM2(getter_AddRefs(serviceManager), nsnull, nsnull);
        if (NS_FAILED(rc))
        {
            printf("Error: XPCOM could not be initialized! rc=0x%x\n", rc);
            return -1;
        }

#if 0
        /*
         * Register our components. This step is only necessary if this executable
         * implements XPCOM components itself which is not the case for this
         * simple example.
         */
        nsCOMPtr<nsIComponentRegistrar> registrar = do_QueryInterface(serviceManager);
        if (!registrar)
        {
            printf("Error: could not query nsIComponentRegistrar interface!\n");
            return -1;
        }
        registrar->AutoRegister(nsnull);
#endif

        /*
         * Make sure the main event queue is created. This event queue is
         * responsible for dispatching incoming XPCOM IPC messages. The main
         * thread should run this event queue's loop during lengthy non-XPCOM
         * operations to ensure messages from the VirtualBox server and other
         * XPCOM IPC clients are processed. This use case doesn't perform such
         * operations so it doesn't run the event loop.
         */
        nsCOMPtr<nsIEventQueue> eventQ;
        rc = NS_GetMainEventQ(getter_AddRefs (eventQ));
        if (NS_FAILED(rc))
        {
            printf("Error: could not get main event queue! rc=%08X\n", rc);
            return -1;
        }

        /*
         * Now XPCOM is ready and we can start to do real work.
         * IVirtualBox is the root interface of VirtualBox and will be
         * retrieved from the XPCOM component manager. We use the
         * XPCOM provided smart pointer nsCOMPtr for all objects because
         * that's very convenient and removes the need deal with reference
         * counting and freeing.
         */
        nsCOMPtr<nsIComponentManager> manager;
        rc = NS_GetComponentManager (getter_AddRefs (manager));
        if (NS_FAILED(rc))
        {
            printf("Error: could not get component manager! rc=%08X\n", rc);
            return -1;
        }

        nsCOMPtr<IVirtualBox> virtualBox;
        rc = manager->CreateInstanceByContractID (NS_VIRTUALBOX_CONTRACTID,
                                                  nsnull,
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
        eventQ->ProcessPendingEvents();
    }

    /*
     * Perform the standard XPCOM shutdown procedure.
     */
    NS_ShutdownXPCOM(nsnull);
#if defined(XPCOM_GLUE)
    XPCOMGlueShutdown();
#endif
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

/**
 * Helper function to print XPCOM exception information set on the current
 * thread after a failed XPCOM method call. This function will also print
 * extended VirtualBox error info if it is available.
 */
void printErrorInfo()
{
    nsresult rc;

    nsCOMPtr <nsIExceptionService> es;
    es = do_GetService (NS_EXCEPTIONSERVICE_CONTRACTID, &rc);
    if (NS_SUCCEEDED (rc))
    {
        nsCOMPtr <nsIExceptionManager> em;
        rc = es->GetCurrentExceptionManager (getter_AddRefs (em));
        if (NS_SUCCEEDED (rc))
        {
            nsCOMPtr<nsIException> ex;
            rc = em->GetCurrentException (getter_AddRefs (ex));
            if (NS_SUCCEEDED (rc) && ex)
            {
                nsCOMPtr <IVirtualBoxErrorInfo> info;
                info = do_QueryInterface(ex, &rc);
                if (NS_SUCCEEDED(rc) && info)
                {
                    /* got extended error info */
                    printf ("Extended error info (IVirtualBoxErrorInfo):\n");
                    nsresult resultCode = NS_OK;
                    info->GetResultCode (&resultCode);
                    printf ("  resultCode=%08X\n", resultCode);
                    nsXPIDLString component;
                    info->GetComponent (getter_Copies (component));
                    printf ("  component=%s\n", NS_ConvertUTF16toUTF8(component).get());
                    nsXPIDLString text;
                    info->GetText (getter_Copies (text));
                    printf ("  text=%s\n", NS_ConvertUTF16toUTF8(text).get());
                }
                else
                {
                    /* got basic error info */
                    printf ("Basic error info (nsIException):\n");
                    nsresult resultCode = NS_OK;
                    ex->GetResult (&resultCode);
                    printf ("  resultCode=%08X\n", resultCode);
                    nsXPIDLCString message;
                    ex->GetMessage (getter_Copies (message));
                    printf ("  message=%s\n", message.get());
                }

                /* reset the exception to NULL to indicate we've processed it */
                em->SetCurrentException (NULL);

                rc = NS_OK;
            }
        }
    }
}
