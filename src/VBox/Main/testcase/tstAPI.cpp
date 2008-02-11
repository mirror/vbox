/** @file
 *
 * tstAPI - test program for our COM/XPCOM interface
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <stdio.h>
#include <stdlib.h>

#include <VBox/com/com.h>
#include <VBox/com/string.h>
#include <VBox/com/array.h>
#include <VBox/com/Guid.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/EventQueue.h>

#include <VBox/com/VirtualBox.h>

using namespace com;

#define LOG_ENABLED
#define LOG_GROUP LOG_GROUP_MAIN
#define LOG_INSTANCE NULL
#include <VBox/log.h>

#include <iprt/runtime.h>
#include <iprt/stream.h>

#define printf RTPrintf

// funcs
///////////////////////////////////////////////////////////////////////////////

HRESULT readAndChangeMachineSettings (IMachine *machine, IMachine *readonlyMachine = 0)
{
    HRESULT rc = S_OK;

    Bstr name;
    printf ("Getting machine name...\n");
    CHECK_RC_RET (machine->COMGETTER(Name) (name.asOutParam()));
    printf ("Name: {%ls}\n", name.raw());

    printf("Getting machine GUID...\n");
    Guid guid;
    CHECK_RC (machine->COMGETTER(Id) (guid.asOutParam()));
    if (SUCCEEDED (rc) && !guid.isEmpty()) {
        printf ("Guid::toString(): {%s}\n", (const char *) guid.toString());
    } else {
        printf ("WARNING: there's no GUID!");
    }

    ULONG memorySize;
    printf ("Getting memory size...\n");
    CHECK_RC_RET (machine->COMGETTER(MemorySize) (&memorySize));
    printf ("Memory size: %d\n", memorySize);

    MachineState_T machineState;
    printf ("Getting machine state...\n");
    CHECK_RC_RET (machine->COMGETTER(State) (&machineState));
    printf ("Machine state: %d\n", machineState);

    BOOL modified;
    printf ("Are any settings modified?...\n");
    CHECK_RC (machine->COMGETTER(SettingsModified) (&modified));
    if (SUCCEEDED (rc))
        printf ("%s\n", modified ? "yes" : "no");

    ULONG memorySizeBig = memorySize * 10;
    printf("Changing memory size to %d...\n", memorySizeBig);
    CHECK_RC (machine->COMSETTER(MemorySize) (memorySizeBig));

    if (SUCCEEDED (rc))
    {
        printf ("Are any settings modified now?...\n");
        CHECK_RC_RET (machine->COMGETTER(SettingsModified) (&modified));
        printf ("%s\n", modified ? "yes" : "no");
        ASSERT_RET (modified, 0);

        ULONG memorySizeGot;
        printf ("Getting memory size again...\n");
        CHECK_RC_RET (machine->COMGETTER(MemorySize) (&memorySizeGot));
        printf ("Memory size: %d\n", memorySizeGot);
        ASSERT_RET (memorySizeGot == memorySizeBig, 0);

        if (readonlyMachine)
        {
            printf ("Getting memory size of the counterpart readonly machine...\n");
            ULONG memorySizeRO;
            readonlyMachine->COMGETTER(MemorySize) (&memorySizeRO);
            printf ("Memory size: %d\n", memorySizeRO);
            ASSERT_RET (memorySizeRO != memorySizeGot, 0);
        }

        printf ("Discarding recent changes...\n");
        CHECK_RC_RET (machine->DiscardSettings());
        printf ("Are any settings modified after discarding?...\n");
        CHECK_RC_RET (machine->COMGETTER(SettingsModified) (&modified));
        printf ("%s\n", modified ? "yes" : "no");
        ASSERT_RET (!modified, 0);

        printf ("Getting memory size once more...\n");
        CHECK_RC_RET (machine->COMGETTER(MemorySize) (&memorySizeGot));
        printf ("Memory size: %d\n", memorySizeGot);
        ASSERT_RET (memorySizeGot == memorySize, 0);

        memorySize = memorySize > 128 ? memorySize / 2 : memorySize * 2;
        printf("Changing memory size to %d...\n", memorySize);
        CHECK_RC_RET (machine->COMSETTER(MemorySize) (memorySize));
    }

    Bstr desc;
    printf ("Getting description...\n");
    CHECK_ERROR_RET (machine, COMGETTER(Description) (desc.asOutParam()), rc);
    printf ("Description is: \"%ls\"\n", desc.raw());

    desc = L"This is an exemplary description (changed).";
    printf ("Setting description to \"%ls\"...\n", desc.raw());
    CHECK_ERROR_RET (machine, COMSETTER(Description) (desc), rc);

    printf ("Saving machine settings...\n");
    CHECK_RC (machine->SaveSettings());
    if (SUCCEEDED (rc))
    {
        printf ("Are any settings modified after saving?...\n");
        CHECK_RC_RET (machine->COMGETTER(SettingsModified) (&modified));
        printf ("%s\n", modified ? "yes" : "no");
        ASSERT_RET (!modified, 0);

        if (readonlyMachine) {
            printf ("Getting memory size of the counterpart readonly machine...\n");
            ULONG memorySizeRO;
            readonlyMachine->COMGETTER(MemorySize) (&memorySizeRO);
            printf ("Memory size: %d\n", memorySizeRO);
            ASSERT_RET (memorySizeRO == memorySize, 0);
        }
    }

    Bstr extraDataKey = L"Blafasel";
    Bstr extraData;
    printf ("Getting extra data key {%ls}...\n", extraDataKey.raw());
    CHECK_RC_RET (machine->GetExtraData (extraDataKey, extraData.asOutParam()));
    if (!extraData.isEmpty()) {
        printf ("Extra data value: {%ls}\n", extraData.raw());
    } else {
        if (extraData.isNull())
            printf ("No extra data exists\n");
        else
            printf ("Extra data is empty\n");
    }

    if (extraData.isEmpty())
        extraData = L"Das ist die Berliner Luft, Luft, Luft...";
    else
        extraData.setNull();
    printf (
        "Setting extra data key {%ls} to {%ls}...\n",
        extraDataKey.raw(), extraData.raw()
    );
    CHECK_RC (machine->SetExtraData (extraDataKey, extraData));

    if (SUCCEEDED (rc)) {
        printf ("Getting extra data key {%ls} again...\n", extraDataKey.raw());
        CHECK_RC_RET (machine->GetExtraData (extraDataKey, extraData.asOutParam()));
        if (!extraData.isEmpty()) {
            printf ("Extra data value: {%ls}\n", extraData.raw());
        } else {
            if (extraData.isNull())
                printf ("No extra data exists\n");
            else
                printf ("Extra data is empty\n");
        }
    }

    return rc;
}

// main
///////////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[])
{
    /*
     * Initialize the VBox runtime without loading
     * the support driver.
     */
    RTR3Init(false);

    HRESULT rc;

    {
        char homeDir [RTPATH_MAX];
        GetVBoxUserHomeDirectory (homeDir, sizeof (homeDir));
        printf ("VirtualBox Home Directory = '%s'\n", homeDir);
    }

    printf ("Initializing COM...\n");

    CHECK_RC_RET (com::Initialize());

    do
    {
    // scopes all the stuff till shutdown
    ////////////////////////////////////////////////////////////////////////////

    ComPtr <IVirtualBox> virtualBox;
    ComPtr <ISession> session;

#if 0
    // Utf8Str test
    ////////////////////////////////////////////////////////////////////////////

    Utf8Str nullUtf8Str;
    printf ("nullUtf8Str='%s'\n", nullUtf8Str.raw());

    Utf8Str simpleUtf8Str = "simpleUtf8Str";
    printf ("simpleUtf8Str='%s'\n", simpleUtf8Str.raw());

    Utf8Str utf8StrFmt = Utf8StrFmt ("[0=%d]%s[1=%d]",
                                     0, "utf8StrFmt", 1);
    printf ("utf8StrFmt='%s'\n", utf8StrFmt.raw());

#endif

    printf ("Creating VirtualBox object...\n");
    CHECK_RC (virtualBox.createLocalObject (CLSID_VirtualBox));
    if (FAILED (rc))
    {
        CHECK_ERROR_NOCALL();
        break;
    }

    printf ("Creating Session object...\n");
    CHECK_RC (session.createInprocObject (CLSID_Session));
    if (FAILED (rc))
    {
        CHECK_ERROR_NOCALL();
        break;
    }

#if 0
    // IUnknown identity test
    ////////////////////////////////////////////////////////////////////////////
    {
        {
            ComPtr <IVirtualBox> virtualBox2;

            printf ("Creating one more VirtualBox object...\n");
            CHECK_RC (virtualBox2.createLocalObject (CLSID_VirtualBox));
            if (FAILED (rc))
            {
                CHECK_ERROR_NOCALL();
                break;
            }

            printf ("IVirtualBox(virualBox)=%p IVirtualBox(virualBox2)=%p\n",
                    (IVirtualBox *) virtualBox, (IVirtualBox *) virtualBox2);
            Assert ((IVirtualBox *) virtualBox == (IVirtualBox *) virtualBox2);

            ComPtr <IUnknown> unk (virtualBox);
            ComPtr <IUnknown> unk2;
            unk2 = virtualBox2;

            printf ("IUnknown(virualBox)=%p IUnknown(virualBox2)=%p\n",
                    (IUnknown *) unk, (IUnknown *) unk2);
            Assert ((IUnknown *) unk == (IUnknown *) unk2);

            ComPtr <IVirtualBox> vb = unk;
            ComPtr <IVirtualBox> vb2 = unk;

            printf ("IVirtualBox(IUnknown(virualBox))=%p IVirtualBox(IUnknown(virualBox2))=%p\n",
                    (IVirtualBox *) vb, (IVirtualBox *) vb2);
            Assert ((IVirtualBox *) vb == (IVirtualBox *) vb2);
        }

        {
            ComPtr <IHost> host;
            CHECK_ERROR_BREAK (virtualBox, COMGETTER(Host)(host.asOutParam()));
            printf (" IHost(host)=%p\n", (IHost *) host);
            ComPtr <IUnknown> unk = host;
            printf (" IUnknown(host)=%p\n", (IUnknown *) unk);
            ComPtr <IHost> host_copy = unk;
            printf (" IHost(host_copy)=%p\n", (IHost *) host_copy);
            ComPtr <IUnknown> unk_copy = host_copy;
            printf (" IUnknown(host_copy)=%p\n", (IUnknown *) unk_copy);
            Assert ((IUnknown *) unk == (IUnknown *) unk_copy);

            /* query IUnknown on IUnknown */
            ComPtr <IUnknown> unk_copy_copy;
            unk_copy.queryInterfaceTo (unk_copy_copy.asOutParam());
            printf (" IUnknown(unk_copy)=%p\n", (IUnknown *) unk_copy_copy);
            Assert ((IUnknown *) unk_copy == (IUnknown *) unk_copy_copy);
            /* query IUnknown on IUnknown in the opposite direction */
            unk_copy_copy.queryInterfaceTo (unk_copy.asOutParam());
            printf (" IUnknown(unk_copy_copy)=%p\n", (IUnknown *) unk_copy);
            Assert ((IUnknown *) unk_copy == (IUnknown *) unk_copy_copy);

            /* query IUnknown again after releasing all previous IUnknown instances
             * but keeping IHost -- it should remain the same (Identity Rule) */
            IUnknown *oldUnk = unk;
            unk.setNull();
            unk_copy.setNull();
            unk_copy_copy.setNull();
            unk = host;
            printf (" IUnknown(host)=%p\n", (IUnknown *) unk);
            Assert (oldUnk == (IUnknown *) unk);
        }

//        printf ("Will be now released (press Enter)...");
//        getchar();
    }
#endif

    // create the event queue
    // (here it is necessary only to process remaining XPCOM/IPC events
    // after the session is closed)
    EventQueue eventQ;

#if 0
    // the simplest COM API test
    ////////////////////////////////////////////////////////////////////////////
    {
        Bstr version;
        CHECK_ERROR_BREAK (virtualBox, COMGETTER(Version) (version.asOutParam()));
        printf ("VirtualBox version = %ls\n", version.raw());
    }
#endif

#if 1
    // Array test
    ////////////////////////////////////////////////////////////////////////////
    {
        printf ("Calling IVirtualBox::Machines...\n");

        com::SafeArray <ULONG> aaa (10);

        com::SafeIfaceArray <IMachine> machines;
        CHECK_ERROR_BREAK (virtualBox,
                           COMGETTER(Machines2) (ComSafeArrayAsOutParam (machines)));

        printf ("%u machines registered.\n", machines.size());

        for (size_t i = 0; i < machines.size(); ++ i)
        {
            Bstr name;
            CHECK_ERROR_BREAK (machines [i], COMGETTER(Name) (name.asOutParam()));
            printf ("machines[%u]='%s'\n", i, Utf8Str (name).raw());
        }
    }
#endif

#if 0
    // some outdated stuff
    ////////////////////////////////////////////////////////////////////////////

    printf("Getting IHost interface...\n");
    IHost *host;
    rc = virtualBox->GetHost(&host);
    if (SUCCEEDED(rc))
    {
        IHostDVDDriveCollection *dvdColl;
        rc = host->GetHostDVDDrives(&dvdColl);
        if (SUCCEEDED(rc))
        {
            IHostDVDDrive *dvdDrive = NULL;
            dvdColl->GetNextHostDVDDrive(dvdDrive, &dvdDrive);
            while (dvdDrive)
            {
                BSTR driveName;
                char *driveNameUtf8;
                dvdDrive->GetDriveName(&driveName);
                RTStrUcs2ToUtf8(&driveNameUtf8, (PCRTUCS2)driveName);
                printf("Host DVD drive name: %s\n", driveNameUtf8);
                RTStrFree(driveNameUtf8);
                SysFreeString(driveName);
                IHostDVDDrive *dvdDriveTemp = dvdDrive;
                dvdColl->GetNextHostDVDDrive(dvdDriveTemp, &dvdDrive);
                dvdDriveTemp->Release();
            }
            dvdColl->Release();
        } else
        {
            printf("Could not get host DVD drive collection\n");
        }

        IHostFloppyDriveCollection *floppyColl;
        rc = host->GetHostFloppyDrives(&floppyColl);
        if (SUCCEEDED(rc))
        {
            IHostFloppyDrive *floppyDrive = NULL;
            floppyColl->GetNextHostFloppyDrive(floppyDrive, &floppyDrive);
            while (floppyDrive)
            {
                BSTR driveName;
                char *driveNameUtf8;
                floppyDrive->GetDriveName(&driveName);
                RTStrUcs2ToUtf8(&driveNameUtf8, (PCRTUCS2)driveName);
                printf("Host floppy drive name: %s\n", driveNameUtf8);
                RTStrFree(driveNameUtf8);
                SysFreeString(driveName);
                IHostFloppyDrive *floppyDriveTemp = floppyDrive;
                floppyColl->GetNextHostFloppyDrive(floppyDriveTemp, &floppyDrive);
                floppyDriveTemp->Release();
            }
            floppyColl->Release();
        } else
        {
            printf("Could not get host floppy drive collection\n");
        }
        host->Release();
    } else
    {
        printf("Call failed\n");
    }
    printf ("\n");
#endif

#if 0
    // IVirtualBoxErrorInfo test
    ////////////////////////////////////////////////////////////////////////////
    {
        // RPC calls

        // call a method that will definitely fail
        Guid uuid;
        ComPtr <IHardDisk> hardDisk;
        rc = virtualBox->GetHardDisk(uuid, hardDisk.asOutParam());
        printf ("virtualBox->GetHardDisk(null-uuid)=%08X\n", rc);

//        {
//            com::ErrorInfo info (virtualBox);
//            PRINT_ERROR_INFO (info);
//        }

        // call a method that will definitely succeed
        Bstr version;
        rc = virtualBox->COMGETTER(Version) (version.asOutParam());
        printf ("virtualBox->COMGETTER(Version)=%08X\n", rc);

        {
            com::ErrorInfo info (virtualBox);
            PRINT_ERROR_INFO (info);
        }

        // Local calls

        // call a method that will definitely fail
        ComPtr <IMachine> machine;
        rc = session->COMGETTER(Machine)(machine.asOutParam());
        printf ("session->COMGETTER(Machine)=%08X\n", rc);

//        {
//            com::ErrorInfo info (virtualBox);
//            PRINT_ERROR_INFO (info);
//        }

        // call a method that will definitely succeed
        SessionState_T state;
        rc = session->COMGETTER(State) (&state);
        printf ("session->COMGETTER(State)=%08X\n", rc);

        {
            com::ErrorInfo info (virtualBox);
            PRINT_ERROR_INFO (info);
        }
    }
#endif

#if 0
    // register the existing hard disk image
    ///////////////////////////////////////////////////////////////////////////
    do
    {
        ComPtr <IHardDisk> hd;
        Bstr src = L"E:\\develop\\innotek\\images\\NewHardDisk.vdi";
        printf ("Opening the existing hard disk '%ls'...\n", src.raw());
        CHECK_ERROR_BREAK (virtualBox, OpenHardDisk (src, hd.asOutParam()));
        printf ("Enter to continue...\n");
        getchar();
        printf ("Registering the existing hard disk '%ls'...\n", src.raw());
        CHECK_ERROR_BREAK (virtualBox, RegisterHardDisk (hd));
        printf ("Enter to continue...\n");
        getchar();
    }
    while (FALSE);
    printf ("\n");
#endif

#if 0
    // find and unregister the existing hard disk image
    ///////////////////////////////////////////////////////////////////////////
    do
    {
        ComPtr <IVirtualDiskImage> vdi;
        Bstr src = L"CreatorTest.vdi";
        printf ("Unregistering the hard disk '%ls'...\n", src.raw());
        CHECK_ERROR_BREAK (virtualBox, FindVirtualDiskImage (src, vdi.asOutParam()));
        ComPtr <IHardDisk> hd = vdi;
        Guid id;
        CHECK_ERROR_BREAK (hd, COMGETTER(Id) (id.asOutParam()));
        CHECK_ERROR_BREAK (virtualBox, UnregisterHardDisk (id, hd.asOutParam()));
    }
    while (FALSE);
    printf ("\n");
#endif

#if 0
    // clone the registered hard disk
    ///////////////////////////////////////////////////////////////////////////
    do
    {
#if defined RT_OS_LINUX
        Bstr src = L"/mnt/hugaida/common/develop/innotek/images/freedos-linux.vdi";
#else
        Bstr src = L"E:/develop/innotek/images/freedos.vdi";
#endif
        Bstr dst = L"./clone.vdi";
        RTPrintf ("Cloning '%ls' to '%ls'...\n", src.raw(), dst.raw());
        ComPtr <IVirtualDiskImage> vdi;
        CHECK_ERROR_BREAK (virtualBox, FindVirtualDiskImage (src, vdi.asOutParam()));
        ComPtr <IHardDisk> hd = vdi;
        ComPtr <IProgress> progress;
        CHECK_ERROR_BREAK (hd, CloneToImage (dst, vdi.asOutParam(), progress.asOutParam()));
        RTPrintf ("Waiting for completion...\n");
        CHECK_ERROR_BREAK (progress, WaitForCompletion (-1));
        ProgressErrorInfo ei (progress);
        if (FAILED (ei.getResultCode()))
        {
            PRINT_ERROR_INFO (ei);
        }
        else
        {
            vdi->COMGETTER(FilePath) (dst.asOutParam());
            RTPrintf ("Actual clone path is '%ls'\n", dst.raw());
        }
    }
    while (FALSE);
    printf ("\n");
#endif

#if 0
    // find a registered hard disk by location
    ///////////////////////////////////////////////////////////////////////////
    do
    {
        ComPtr <IHardDisk> hd;
        static const wchar_t *Names[] =
        {
#ifndef RT_OS_LINUX
            L"E:/Develop/innotek/images/thinker/freedos.vdi",
            L"E:/Develop/innotek/images/thinker/fReeDoS.vDI",
            L"E:/Develop/innotek/images/vmdk/haiku.vmdk",
#else
            L"/mnt/host/common/Develop/innotek/images/maggot/freedos.vdi",
            L"/mnt/host/common/Develop/innotek/images/maggot/fReeDoS.vDI",
#endif
        };
        for (size_t i = 0; i < ELEMENTS (Names); ++ i)
        {
            Bstr src = Names [i];
            printf ("Searching for hard disk '%ls'...\n", src.raw());
            rc = virtualBox->FindHardDisk (src, hd.asOutParam());
            if (SUCCEEDED (rc))
            {
                Guid id;
                Bstr location;
                CHECK_ERROR_BREAK (hd, COMGETTER(Id) (id.asOutParam()));
                CHECK_ERROR_BREAK (hd, COMGETTER(Location) (location.asOutParam()));
                printf ("Found, UUID={%Vuuid}, location='%ls'.\n",
                        id.raw(), location.raw());
            }
            else
            {
                PRINT_ERROR_INFO (com::ErrorInfo (virtualBox));
            }
        }
    }
    while (FALSE);
    printf ("\n");
#endif

#if 0
    // access the machine in read-only mode
    ///////////////////////////////////////////////////////////////////////////
    do
    {
        ComPtr <IMachine> machine;
        Bstr name = argc > 1 ? argv [1] : "dos";
        printf ("Getting a machine object named '%ls'...\n", name.raw());
        CHECK_ERROR_BREAK (virtualBox, FindMachine (name, machine.asOutParam()));
        printf ("Accessing the machine in read-only mode:\n");
        readAndChangeMachineSettings (machine);
#if 0
        if (argc != 2)
        {
            printf ("Error: a string has to be supplied!\n");
        }
        else
        {
            Bstr secureLabel = argv[1];
            machine->COMSETTER(ExtraData)(L"VBoxSDL/SecureLabel", secureLabel);
        }
#endif
    }
    while (0);
    printf ("\n");
#endif

#if 0
    // create a new machine (w/o registering it)
    ///////////////////////////////////////////////////////////////////////////
    do
    {
        ComPtr <IMachine> machine;
#if defined (RT_OS_LINUX)
        Bstr baseDir = L"/tmp/vbox";
#else
        Bstr baseDir = L"C:\\vbox";
#endif
        Bstr name = L"machina";

        printf ("Creating a new machine object (base dir '%ls', name '%ls')...\n",
                baseDir.raw(), name.raw());
        CHECK_ERROR_BREAK (virtualBox, CreateMachine (baseDir, name,
                                                      machine.asOutParam()));

        printf ("Getting name...\n");
        CHECK_ERROR_BREAK (machine, COMGETTER(Name) (name.asOutParam()));
        printf ("Name: {%ls}\n", name.raw());

        BOOL modified = FALSE;
        printf ("Are any settings modified?...\n");
        CHECK_ERROR_BREAK (machine, COMGETTER(SettingsModified) (&modified));
        printf ("%s\n", modified ? "yes" : "no");

        ASSERT_BREAK (modified == TRUE);

        name = L"Kakaya prekrasnaya virtual'naya mashina!";
        printf ("Setting new name ({%ls})...\n", name.raw());
        CHECK_ERROR_BREAK (machine, COMSETTER(Name) (name));

        printf ("Setting memory size to 111...\n");
        CHECK_ERROR_BREAK (machine, COMSETTER(MemorySize) (111));

        Bstr desc = L"This is an exemplary description.";
        printf ("Setting description to \"%ls\"...\n", desc.raw());
        CHECK_ERROR_BREAK (machine, COMSETTER(Description) (desc));

        ComPtr <IGuestOSType> guestOSType;
        Bstr type = L"os2warp45";
        CHECK_ERROR_BREAK (virtualBox, GetGuestOSType (type, guestOSType.asOutParam()));

        printf ("Saving new machine settings...\n");
        CHECK_ERROR_BREAK (machine, SaveSettings());

        printf ("Accessing the newly created machine:\n");
        readAndChangeMachineSettings (machine);
    }
    while (FALSE);
    printf ("\n");
#endif

#if 0
    // enumerate host DVD drives
    ///////////////////////////////////////////////////////////////////////////
    do
    {
        ComPtr <IHost> host;
        CHECK_RC_BREAK (virtualBox->COMGETTER(Host) (host.asOutParam()));

        {
            ComPtr <IHostDVDDriveCollection> coll;
            CHECK_RC_BREAK (host->COMGETTER(DVDDrives) (coll.asOutParam()));
            ComPtr <IHostDVDDriveEnumerator> enumerator;
            CHECK_RC_BREAK (coll->Enumerate (enumerator.asOutParam()));
            BOOL hasmore;
            while (SUCCEEDED (enumerator->HasMore (&hasmore)) && hasmore)
            {
                ComPtr <IHostDVDDrive> drive;
                CHECK_RC_BREAK (enumerator->GetNext (drive.asOutParam()));
                Bstr name;
                CHECK_RC_BREAK (drive->COMGETTER(Name) (name.asOutParam()));
                printf ("Host DVD drive: name={%ls}\n", name.raw());
            }
            CHECK_RC_BREAK (rc);

            ComPtr <IHostDVDDrive> drive;
            CHECK_ERROR (enumerator, GetNext (drive.asOutParam()));
            CHECK_ERROR (coll, GetItemAt (1000, drive.asOutParam()));
            CHECK_ERROR (coll, FindByName (Bstr ("R:"), drive.asOutParam()));
            if (SUCCEEDED (rc))
            {
                Bstr name;
                CHECK_RC_BREAK (drive->COMGETTER(Name) (name.asOutParam()));
                printf ("Found by name: name={%ls}\n", name.raw());
            }
        }
    }
    while (FALSE);
    printf ("\n");
#endif

#if 0
    // enumerate hard disks & dvd images
    ///////////////////////////////////////////////////////////////////////////
    do
    {
        {
            ComPtr <IHardDiskCollection> coll;
            CHECK_RC_BREAK (virtualBox->COMGETTER(HardDisks) (coll.asOutParam()));
            ComPtr <IHardDiskEnumerator> enumerator;
            CHECK_RC_BREAK (coll->Enumerate (enumerator.asOutParam()));
            BOOL hasmore;
            while (SUCCEEDED (enumerator->HasMore (&hasmore)) && hasmore)
            {
                ComPtr <IHardDisk> disk;
                CHECK_RC_BREAK (enumerator->GetNext (disk.asOutParam()));
                Guid id;
                CHECK_RC_BREAK (disk->COMGETTER(Id) (id.asOutParam()));
                Bstr path;
                CHECK_RC_BREAK (disk->COMGETTER(FilePath) (path.asOutParam()));
                printf ("Hard Disk: id={%s}, path={%ls}\n",
                        id.toString().raw(), path.raw());
                Guid mid;
                CHECK_RC_BREAK (
                    virtualBox->GetHardDiskUsage (id, ResourceUsage_AllUsage,
                                                  mid.asOutParam())
                );
                if (mid.isEmpty())
                    printf ("  not used\n");
                else
                    printf ("  used by VM: {%s}\n", mid.toString().raw());
            }
            CHECK_RC_BREAK (rc);
        }

        {
            ComPtr <IDVDImageCollection> coll;
            CHECK_RC_BREAK (virtualBox->COMGETTER(DVDImages) (coll.asOutParam()));
            ComPtr <IDVDImageEnumerator> enumerator;
            CHECK_RC_BREAK (coll->Enumerate (enumerator.asOutParam()));
            BOOL hasmore;
            while (SUCCEEDED (enumerator->HasMore (&hasmore)) && hasmore)
            {
                ComPtr <IDVDImage> image;
                CHECK_RC_BREAK (enumerator->GetNext (image.asOutParam()));
                Guid id;
                CHECK_RC_BREAK (image->COMGETTER(Id) (id.asOutParam()));
                Bstr path;
                CHECK_RC_BREAK (image->COMGETTER(FilePath) (path.asOutParam()));
                printf ("CD/DVD Image: id={%s}, path={%ls}\n",
                        id.toString().raw(), path.raw());
                Bstr mIDs;
                CHECK_RC_BREAK (
                    virtualBox->GetDVDImageUsage (id, ResourceUsage_AllUsage,
                                                  mIDs.asOutParam())
                );
                if (mIDs.isNull())
                    printf ("  not used\n");
                else
                    printf ("  used by VMs: {%ls}\n", mIDs.raw());
            }
            CHECK_RC_BREAK (rc);
        }
    }
    while (FALSE);
    printf ("\n");
#endif

#if 0
    // open a (direct) session
    ///////////////////////////////////////////////////////////////////////////
    do
    {
        ComPtr <IMachine> machine;
        Bstr name = argc > 1 ? argv [1] : "dos";
        printf ("Getting a machine object named '%ls'...\n", name.raw());
        CHECK_ERROR_BREAK (virtualBox, FindMachine (name, machine.asOutParam()));
        Guid guid;
        CHECK_RC_BREAK (machine->COMGETTER(Id) (guid.asOutParam()));
        printf ("Opening a session for this machine...\n");
        CHECK_RC_BREAK (virtualBox->OpenSession (session, guid));
#if 1
        ComPtr <IMachine> sessionMachine;
        printf ("Getting sessioned machine object...\n");
        CHECK_RC_BREAK (session->COMGETTER(Machine) (sessionMachine.asOutParam()));
        printf ("Accessing the machine within the session:\n");
        readAndChangeMachineSettings (sessionMachine, machine);
#if 0
        printf ("\n");
        printf ("Enabling the VRDP server (must succeed even if the VM is saved):\n");
        ComPtr <IVRDPServer> vrdp;
        CHECK_ERROR_BREAK (sessionMachine, COMGETTER(VRDPServer) (vrdp.asOutParam()));
        if (FAILED (vrdp->COMSETTER(Enabled) (TRUE)))
        {
            PRINT_ERROR_INFO (com::ErrorInfo (vrdp));
        }
        else
        {
            BOOL enabled = FALSE;
            CHECK_ERROR_BREAK (vrdp, COMGETTER(Enabled) (&enabled));
            printf ("VRDP server is %s\n", enabled ? "enabled" : "disabled");
        }
#endif
#endif
#if 0
        ComPtr <IConsole> console;
        printf ("Getting the console object...\n");
        CHECK_RC_BREAK (session->COMGETTER(Console) (console.asOutParam()));
        printf ("Discarding the current machine state...\n");
        ComPtr <IProgress> progress;
        CHECK_ERROR_BREAK (console, DiscardCurrentState (progress.asOutParam()));
        printf ("Waiting for completion...\n");
        CHECK_ERROR_BREAK (progress, WaitForCompletion (-1));
        ProgressErrorInfo ei (progress);
        if (FAILED (ei.getResultCode()))
        {
            PRINT_ERROR_INFO (ei);

            ComPtr <IUnknown> initiator;
            CHECK_ERROR_BREAK (progress, COMGETTER(Initiator) (initiator.asOutParam()));

            printf ("initiator(unk) = %p\n", (IUnknown *) initiator);
            printf ("console(unk) = %p\n", (IUnknown *) ComPtr <IUnknown> ((IConsole *) console));
            printf ("console = %p\n", (IConsole *) console);
        }
#endif
        printf("Press enter to close session...");
        getchar();
        session->Close();
    }
    while (FALSE);
    printf ("\n");
#endif

#if 0
    // open a remote session
    ///////////////////////////////////////////////////////////////////////////
    do
    {
        ComPtr <IMachine> machine;
        Bstr name = L"dos";
        printf ("Getting a machine object named '%ls'...\n", name.raw());
        CHECK_RC_BREAK (virtualBox->FindMachine (name, machine.asOutParam()));
        Guid guid;
        CHECK_RC_BREAK (machine->COMGETTER(Id) (guid.asOutParam()));
        printf ("Opening a remote session for this machine...\n");
        ComPtr <IProgress> progress;
        CHECK_RC_BREAK (virtualBox->OpenRemoteSession (session, guid, Bstr("gui"),
                                                       NULL, progress.asOutParam()));
        printf ("Waiting for the session to open...\n");
        CHECK_RC_BREAK (progress->WaitForCompletion (-1));
        ComPtr <IMachine> sessionMachine;
        printf ("Getting sessioned machine object...\n");
        CHECK_RC_BREAK (session->COMGETTER(Machine) (sessionMachine.asOutParam()));
        ComPtr <IConsole> console;
        printf ("Getting console object...\n");
        CHECK_RC_BREAK (session->COMGETTER(Console) (console.asOutParam()));
        printf ("Press enter to pause the VM execution in the remote session...");
        getchar();
        CHECK_RC (console->Pause());
        printf ("Press enter to close this session...");
        getchar();
        session->Close();
    }
    while (FALSE);
    printf ("\n");
#endif

#if 0
    // open an existing remote session
    ///////////////////////////////////////////////////////////////////////////
    do
    {
        ComPtr <IMachine> machine;
        Bstr name = "dos";
        printf ("Getting a machine object named '%ls'...\n", name.raw());
        CHECK_RC_BREAK (virtualBox->FindMachine (name, machine.asOutParam()));
        Guid guid;
        CHECK_RC_BREAK (machine->COMGETTER(Id) (guid.asOutParam()));
        printf ("Opening an existing remote session for this machine...\n");
        CHECK_RC_BREAK (virtualBox->OpenExistingSession (session, guid));
        ComPtr <IMachine> sessionMachine;
        printf ("Getting sessioned machine object...\n");
        CHECK_RC_BREAK (session->COMGETTER(Machine) (sessionMachine.asOutParam()));

#if 0
        Bstr extraDataKey = "VBoxSDL/SecureLabel";
        Bstr extraData = "Das kommt jetzt noch viel krasser vom total konkreten API!";
        CHECK_RC (sessionMachine->SetExtraData (extraDataKey, extraData));
#endif
#if 0
        ComPtr <IConsole> console;
        printf ("Getting console object...\n");
        CHECK_RC_BREAK (session->COMGETTER(Console) (console.asOutParam()));
        printf ("Press enter to pause the VM execution in the remote session...");
        getchar();
        CHECK_RC (console->Pause());
        printf ("Press enter to close this session...");
        getchar();
#endif
        session->Close();
    }
    while (FALSE);
    printf ("\n");
#endif

    printf ("Press enter to release Session and VirtualBox instances...");
    getchar();

    // end "all-stuff" scope
    ////////////////////////////////////////////////////////////////////////////
    }
    while (0);

    printf("Press enter to shutdown COM...");
    getchar();

    com::Shutdown();

    printf ("tstAPI FINISHED.\n");

    return rc;
}
