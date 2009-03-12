/** @file
 *
 * tstAPI - test program for our COM/XPCOM interface
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

#include <stdio.h>
#include <stdlib.h>

#include <VBox/com/com.h>
#include <VBox/com/string.h>
#include <VBox/com/array.h>
#include <VBox/com/Guid.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint_legacy.h>
#include <VBox/com/EventQueue.h>

#include <VBox/com/VirtualBox.h>

using namespace com;

#define LOG_ENABLED
#define LOG_GROUP LOG_GROUP_MAIN
#define LOG_INSTANCE NULL
#include <VBox/log.h>

#include <iprt/initterm.h>
#include <iprt/path.h>
#include <iprt/param.h>
#include <iprt/stream.h>
#include <iprt/thread.h>

#define printf RTPrintf


// forward declarations
///////////////////////////////////////////////////////////////////////////////

static Bstr getObjectName(ComPtr<IVirtualBox> aVirtualBox,
                                  ComPtr<IUnknown> aObject);
static void queryMetrics (ComPtr<IVirtualBox> aVirtualBox,
                          ComPtr <IPerformanceCollector> collector,
                          ComSafeArrayIn (IUnknown *, objects));
static void listAffectedMetrics(ComPtr<IVirtualBox> aVirtualBox,
                                ComSafeArrayIn(IPerformanceMetric*, aMetrics));

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
    RTR3Init();

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
    // Testing VirtualBox::COMGETTER(ProgressOperations).
    // This is designed to be tested while running
    // "./VBoxManage clonehd src.vdi clone.vdi" in parallel.
    // It will then display the progress every 2 seconds.
    ////////////////////////////////////////////////////////////////////////////
    {
        printf ("Testing VirtualBox::COMGETTER(ProgressOperations)...\n");

        for (;;) {
            com::SafeIfaceArray <IProgress> operations;

            CHECK_ERROR_BREAK (virtualBox,
                COMGETTER(ProgressOperations)(ComSafeArrayAsOutParam(operations)));

            printf ("operations: %d\n", operations.size());
            if (operations.size() == 0)
                break; // No more operations left.

            for (size_t i = 0; i < operations.size(); ++i) {
                PRInt32 percent;

                operations[i]->COMGETTER(Percent)(&percent);
                printf ("operations[%u]: %ld\n", (unsigned)i, (long)percent);
            }
            RTThreadSleep (2000); // msec
        }
    }
#endif

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

            printf ("IVirtualBox(virtualBox)=%p IVirtualBox(virtualBox2)=%p\n",
                    (IVirtualBox *) virtualBox, (IVirtualBox *) virtualBox2);
            Assert ((IVirtualBox *) virtualBox == (IVirtualBox *) virtualBox2);

            ComPtr <IUnknown> unk (virtualBox);
            ComPtr <IUnknown> unk2;
            unk2 = virtualBox2;

            printf ("IUnknown(virtualBox)=%p IUnknown(virtualBox2)=%p\n",
                    (IUnknown *) unk, (IUnknown *) unk2);
            Assert ((IUnknown *) unk == (IUnknown *) unk2);

            ComPtr <IVirtualBox> vb = unk;
            ComPtr <IVirtualBox> vb2 = unk;

            printf ("IVirtualBox(IUnknown(virtualBox))=%p IVirtualBox(IUnknown(virtualBox2))=%p\n",
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

#if 0
    // Array test
    ////////////////////////////////////////////////////////////////////////////
    {
        printf ("Calling IVirtualBox::Machines...\n");

        com::SafeIfaceArray <IMachine> machines;
        CHECK_ERROR_BREAK (virtualBox,
                           COMGETTER(Machines) (ComSafeArrayAsOutParam (machines)));

        printf ("%u machines registered (machines.isNull()=%d).\n",
                machines.size(), machines.isNull());

        for (size_t i = 0; i < machines.size(); ++ i)
        {
            Bstr name;
            CHECK_ERROR_BREAK (machines [i], COMGETTER(Name) (name.asOutParam()));
            printf ("machines[%u]='%s'\n", i, Utf8Str (name).raw());
        }

#if 0
        {
            printf ("Testing [out] arrays...\n");
            com::SafeGUIDArray uuids;
            CHECK_ERROR_BREAK (virtualBox,
                               COMGETTER(Uuids) (ComSafeArrayAsOutParam (uuids)));

            for (size_t i = 0; i < uuids.size(); ++ i)
                printf ("uuids[%u]=%Vuuid\n", i, &uuids [i]);
        }

        {
            printf ("Testing [in] arrays...\n");
            com::SafeGUIDArray uuids (5);
            for (size_t i = 0; i < uuids.size(); ++ i)
            {
                Guid id;
                id.create();
                uuids [i] = id;
                printf ("uuids[%u]=%Vuuid\n", i, &uuids [i]);
            }

            CHECK_ERROR_BREAK (virtualBox,
                               SetUuids (ComSafeArrayAsInParam (uuids)));
        }
#endif

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
                RTUtf16ToUtf8((PCRTUTF16)driveName, &driveNameUtf8);
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
                RTUtf16ToUtf8((PCRTUTF16)driveName, &driveNameUtf8);
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
    // find a registered hard disk by location and get properties
    ///////////////////////////////////////////////////////////////////////////
    do
    {
        ComPtr <IHardDisk> hd;
        static const wchar_t *Names[] =
        {
#ifndef RT_OS_LINUX
            L"freedos.vdi",
            L"MS-DOS.vmdk",
            L"iscsi",
            L"some/path/and/disk.vdi",
#else
            L"xp.vdi",
            L"Xp.vDI",
#endif
        };

        printf ("\n");

        for (size_t i = 0; i < RT_ELEMENTS (Names); ++ i)
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

                com::SafeArray <BSTR> names;
                com::SafeArray <BSTR> values;

                CHECK_ERROR_BREAK (hd, GetProperties (NULL,
                                                      ComSafeArrayAsOutParam (names),
                                                      ComSafeArrayAsOutParam (values)));

                printf ("Properties:\n");
                for (size_t i = 0; i < names.size(); ++ i)
                    printf (" %ls = %ls\n", names [i], values [i]);

                if (names.size() == 0)
                    printf (" <none>\n");

#if 0
                Bstr name ("TargetAddress");
                Bstr value = Utf8StrFmt ("lalala (%llu)", RTTimeMilliTS());

                printf ("Settings property %ls to %ls...\n", name.raw(), value.raw());
                CHECK_ERROR (hd, SetProperty (name, value));
#endif
            }
            else
            {
                com::ErrorInfo info (virtualBox);
                PRINT_ERROR_INFO (info);
            }
            printf ("\n");
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
    // check for available hd backends
    ///////////////////////////////////////////////////////////////////////////
    {
        RTPrintf("Supported hard disk backends: --------------------------\n");
        ComPtr<ISystemProperties> systemProperties;
        CHECK_ERROR_BREAK (virtualBox,
                           COMGETTER(SystemProperties) (systemProperties.asOutParam()));
        com::SafeIfaceArray <IHardDiskFormat> hardDiskFormats;
        CHECK_ERROR_BREAK (systemProperties,
                           COMGETTER(HardDiskFormats) (ComSafeArrayAsOutParam (hardDiskFormats)));

        for (size_t i = 0; i < hardDiskFormats.size(); ++ i)
        {
            /* General information */
            Bstr id;
            CHECK_ERROR_BREAK (hardDiskFormats [i],
                               COMGETTER(Id) (id.asOutParam()));

            Bstr description;
            CHECK_ERROR_BREAK (hardDiskFormats [i],
                               COMGETTER(Id) (description.asOutParam()));

            ULONG caps;
            CHECK_ERROR_BREAK (hardDiskFormats [i],
                               COMGETTER(Capabilities) (&caps));

            RTPrintf("Backend %u: id='%ls' description='%ls' capabilities=%#06x extensions='",
                     i, id.raw(), description.raw(), caps);

            /* File extensions */
            com::SafeArray <BSTR> fileExtensions;
            CHECK_ERROR_BREAK (hardDiskFormats [i],
                               COMGETTER(FileExtensions) (ComSafeArrayAsOutParam (fileExtensions)));
            for (size_t a = 0; a < fileExtensions.size(); ++ a)
            {
                RTPrintf ("%ls", Bstr (fileExtensions [a]).raw());
                if (a != fileExtensions.size()-1)
                    RTPrintf (",");
            }
            RTPrintf ("'");

            /* Configuration keys */
            com::SafeArray <BSTR> propertyNames;
            com::SafeArray <BSTR> propertyDescriptions;
            com::SafeArray <ULONG> propertyTypes;
            com::SafeArray <ULONG> propertyFlags;
            com::SafeArray <BSTR> propertyDefaults;
            CHECK_ERROR_BREAK (hardDiskFormats [i],
                               DescribeProperties (ComSafeArrayAsOutParam (propertyNames),
                                                   ComSafeArrayAsOutParam (propertyDescriptions),
                                                   ComSafeArrayAsOutParam (propertyTypes),
                                                   ComSafeArrayAsOutParam (propertyFlags),
                                                   ComSafeArrayAsOutParam (propertyDefaults)));

            RTPrintf (" config=(");
            if (propertyNames.size() > 0)
            {
                for (size_t a = 0; a < propertyNames.size(); ++ a)
                {
                    RTPrintf ("key='%ls' desc='%ls' type=", Bstr (propertyNames [a]).raw(), Bstr (propertyDescriptions [a]).raw());
                    switch (propertyTypes [a])
                    {
                        case DataType_Int32Type: RTPrintf ("int"); break;
                        case DataType_Int8Type: RTPrintf ("byte"); break;
                        case DataType_StringType: RTPrintf ("string"); break;
                    }
                    RTPrintf (" flags=%#04x", propertyFlags [a]);
                    RTPrintf (" default='%ls'", Bstr (propertyDefaults [a]).raw());
                    if (a != propertyNames.size()-1)
                        RTPrintf (",");
                }
            }
            RTPrintf (")\n");
        }
        RTPrintf("-------------------------------------------------------\n");
    }
#endif

#if 0
    // enumerate hard disks & dvd images
    ///////////////////////////////////////////////////////////////////////////
    do
    {
        {
            com::SafeIfaceArray <IHardDisk> disks;
            CHECK_ERROR_BREAK (virtualBox,
                               COMGETTER(HardDisks)(ComSafeArrayAsOutParam (disks)));

            printf ("%u base hard disks registered (disks.isNull()=%d).\n",
                    disks.size(), disks.isNull());

            for (size_t i = 0; i < disks.size(); ++ i)
            {
                Bstr loc;
                CHECK_ERROR_BREAK (disks [i], COMGETTER(Location) (loc.asOutParam()));
                Guid id;
                CHECK_ERROR_BREAK (disks [i], COMGETTER(Id) (id.asOutParam()));
                MediaState_T state;
                CHECK_ERROR_BREAK (disks [i], COMGETTER(State) (&state));
                Bstr format;
                CHECK_ERROR_BREAK (disks [i], COMGETTER(Format) (format.asOutParam()));

                printf (" disks[%u]: '%ls'\n"
                        "  UUID:         {%Vuuid}\n"
                        "  State:        %s\n"
                        "  Format:       %ls\n",
                        i, loc.raw(), id.raw(),
                        state == MediaState_NotCreated ? "Not Created" :
                        state == MediaState_Created ? "Created" :
                        state == MediaState_Inaccessible ? "Inaccessible" :
                        state == MediaState_LockedRead ? "Locked Read" :
                        state == MediaState_LockedWrite ? "Locked Write" :
                        "???",
                        format.raw());

                if (state == MediaState_Inaccessible)
                {
                    Bstr error;
                    CHECK_ERROR_BREAK (disks [i],
                                       COMGETTER(LastAccessError)(error.asOutParam()));
                    printf ("  Access Error: %ls\n", error.raw());
                }

                /* get usage */

                printf ("  Used by VMs:\n");

                com::SafeGUIDArray ids;
                CHECK_ERROR_BREAK (disks [i],
                                   COMGETTER(MachineIds) (ComSafeArrayAsOutParam (ids)));
                if (ids.size() == 0)
                {
                    printf ("   <not used>\n");
                }
                else
                {
                    for (size_t j = 0; j < ids.size(); ++ j)
                    {
                        printf ("   {%Vuuid}\n", &ids [j]);
                    }
                }
            }
        }
        {
            com::SafeIfaceArray <IDVDImage> images;
            CHECK_ERROR_BREAK (virtualBox,
                               COMGETTER(DVDImages) (ComSafeArrayAsOutParam (images)));

            printf ("%u DVD images registered (images.isNull()=%d).\n",
                    images.size(), images.isNull());

            for (size_t i = 0; i < images.size(); ++ i)
            {
                Bstr loc;
                CHECK_ERROR_BREAK (images [i], COMGETTER(Location) (loc.asOutParam()));
                Guid id;
                CHECK_ERROR_BREAK (images [i], COMGETTER(Id) (id.asOutParam()));
                MediaState_T state;
                CHECK_ERROR_BREAK (images [i], COMGETTER(State) (&state));

                printf (" images[%u]: '%ls'\n"
                        "  UUID:         {%Vuuid}\n"
                        "  State:        %s\n",
                        i, loc.raw(), id.raw(),
                        state == MediaState_NotCreated ? "Not Created" :
                        state == MediaState_Created ? "Created" :
                        state == MediaState_Inaccessible ? "Inaccessible" :
                        state == MediaState_LockedRead ? "Locked Read" :
                        state == MediaState_LockedWrite ? "Locked Write" :
                        "???");

                if (state == MediaState_Inaccessible)
                {
                    Bstr error;
                    CHECK_ERROR_BREAK (images [i],
                                       COMGETTER(LastAccessError)(error.asOutParam()));
                    printf ("  Access Error: %ls\n", error.raw());
                }

                /* get usage */

                printf ("  Used by VMs:\n");

                com::SafeGUIDArray ids;
                CHECK_ERROR_BREAK (images [i],
                                   COMGETTER(MachineIds) (ComSafeArrayAsOutParam (ids)));
                if (ids.size() == 0)
                {
                    printf ("   <not used>\n");
                }
                else
                {
                    for (size_t j = 0; j < ids.size(); ++ j)
                    {
                        printf ("   {%Vuuid}\n", &ids [j]);
                    }
                }
            }
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

#if 0
    do {
        // Get host
        ComPtr <IHost> host;
        CHECK_ERROR_BREAK (virtualBox, COMGETTER(Host) (host.asOutParam()));

        ULONG uMemSize, uMemAvail;
        CHECK_ERROR_BREAK (host, COMGETTER(MemorySize) (&uMemSize));
        printf("Total memory (MB): %u\n", uMemSize);
        CHECK_ERROR_BREAK (host, COMGETTER(MemoryAvailable) (&uMemAvail));
        printf("Free memory (MB): %u\n", uMemAvail);
    } while (0);
#endif

#if 0
    do {
        // Get host
        ComPtr <IHost> host;
        CHECK_ERROR_BREAK (virtualBox, COMGETTER(Host) (host.asOutParam()));

        com::SafeIfaceArray <IHostNetworkInterface> hostNetworkInterfaces;
        CHECK_ERROR_BREAK(host,
                          COMGETTER(NetworkInterfaces) (ComSafeArrayAsOutParam (hostNetworkInterfaces)));
        if (hostNetworkInterfaces.size() > 0)
        {
            ComPtr<IHostNetworkInterface> networkInterface = hostNetworkInterfaces[0];
            Bstr interfaceName;
            networkInterface->COMGETTER(Name)(interfaceName.asOutParam());
            printf("Found %d network interfaces, testing with %lS...\n", hostNetworkInterfaces.size(), interfaceName.raw());
            Guid interfaceGuid;
            networkInterface->COMGETTER(Id)(interfaceGuid.asOutParam());
            // Find the interface by its name
            networkInterface.setNull();
            CHECK_ERROR_BREAK(host,
                              FindHostNetworkInterfaceByName (interfaceName, networkInterface.asOutParam()));
            Guid interfaceGuid2;
            networkInterface->COMGETTER(Id)(interfaceGuid2.asOutParam());
            if (interfaceGuid2 != interfaceGuid)
                printf("Failed to retrieve an interface by name %lS.\n", interfaceName.raw());
            // Find the interface by its guid
            networkInterface.setNull();
            CHECK_ERROR_BREAK(host,
                              FindHostNetworkInterfaceById (interfaceGuid, networkInterface.asOutParam()));
            Bstr interfaceName2;
            networkInterface->COMGETTER(Name)(interfaceName2.asOutParam());
            if (interfaceName != interfaceName2)
                printf("Failed to retrieve an interface by GUID %lS.\n", Bstr(interfaceGuid.toString()).raw());
        }
        else
        {
            printf("No network interfaces found!\n");
        }
    } while (0);
#endif

#if 0 && defined (VBOX_WITH_RESOURCE_USAGE_API)
    do {
        // Get collector
        ComPtr <IPerformanceCollector> collector;
        CHECK_ERROR_BREAK (virtualBox,
                           COMGETTER(PerformanceCollector) (collector.asOutParam()));


        // Fill base metrics array
        Bstr baseMetricNames[] = { L"CPU/Load,RAM/Usage" };
        com::SafeArray<BSTR> baseMetrics (1);
        baseMetricNames[0].cloneTo (&baseMetrics [0]);

        // Get host
        ComPtr <IHost> host;
        CHECK_ERROR_BREAK (virtualBox, COMGETTER(Host) (host.asOutParam()));

        // Get machine
        ComPtr <IMachine> machine;
        Bstr name = argc > 1 ? argv [1] : "dsl";
        Bstr sessionType = argc > 2 ? argv [2] : "vrdp";
        printf ("Getting a machine object named '%ls'...\n", name.raw());
        CHECK_RC_BREAK (virtualBox->FindMachine (name, machine.asOutParam()));

        // Open session
        Guid guid;
        CHECK_RC_BREAK (machine->COMGETTER(Id) (guid.asOutParam()));
        printf ("Opening a remote session for this machine...\n");
        ComPtr <IProgress> progress;
        CHECK_RC_BREAK (virtualBox->OpenRemoteSession (session, guid, sessionType,
                                                       NULL, progress.asOutParam()));
        printf ("Waiting for the session to open...\n");
        CHECK_RC_BREAK (progress->WaitForCompletion (-1));
        ComPtr <IMachine> sessionMachine;
        printf ("Getting sessioned machine object...\n");
        CHECK_RC_BREAK (session->COMGETTER(Machine) (sessionMachine.asOutParam()));

        // Setup base metrics
        // Note that one needs to set up metrics after a session is open for a machine.
        com::SafeIfaceArray<IPerformanceMetric> affectedMetrics;
        com::SafeIfaceArray<IUnknown> objects(2);
        host.queryInterfaceTo(&objects[0]);
        machine.queryInterfaceTo(&objects[1]);
        CHECK_ERROR_BREAK (collector, SetupMetrics(ComSafeArrayAsInParam(baseMetrics),
                                                   ComSafeArrayAsInParam(objects), 1u, 10u,
                                                   ComSafeArrayAsOutParam(affectedMetrics)) );
        listAffectedMetrics(virtualBox,
                            ComSafeArrayAsInParam(affectedMetrics));
        affectedMetrics.setNull();

        // Get console
        ComPtr <IConsole> console;
        printf ("Getting console object...\n");
        CHECK_RC_BREAK (session->COMGETTER(Console) (console.asOutParam()));

        RTThreadSleep(5000); // Sleep for 5 seconds

        printf("\nMetrics collected with VM running: --------------------\n");
        queryMetrics(virtualBox, collector, ComSafeArrayAsInParam(objects));

        // Pause
        //printf ("Press enter to pause the VM execution in the remote session...");
        //getchar();
        CHECK_RC (console->Pause());

        RTThreadSleep(5000); // Sleep for 5 seconds

        printf("\nMetrics collected with VM paused: ---------------------\n");
        queryMetrics(virtualBox, collector, ComSafeArrayAsInParam(objects));

        printf("\nDrop collected metrics: ----------------------------------------\n");
        CHECK_ERROR_BREAK (collector,
            SetupMetrics(ComSafeArrayAsInParam(baseMetrics),
                         ComSafeArrayAsInParam(objects),
                         1u, 5u, ComSafeArrayAsOutParam(affectedMetrics)) );
        listAffectedMetrics(virtualBox,
                            ComSafeArrayAsInParam(affectedMetrics));
        affectedMetrics.setNull();
        queryMetrics(virtualBox, collector, ComSafeArrayAsInParam(objects));

        com::SafeIfaceArray<IUnknown> vmObject(1);
        machine.queryInterfaceTo(&vmObject[0]);

        printf("\nDisable collection of VM metrics: ------------------------------\n");
        CHECK_ERROR_BREAK (collector,
            DisableMetrics(ComSafeArrayAsInParam(baseMetrics),
                           ComSafeArrayAsInParam(vmObject),
                           ComSafeArrayAsOutParam(affectedMetrics)) );
        listAffectedMetrics(virtualBox,
                            ComSafeArrayAsInParam(affectedMetrics));
        affectedMetrics.setNull();
        RTThreadSleep(5000); // Sleep for 5 seconds
        queryMetrics(virtualBox, collector, ComSafeArrayAsInParam(objects));

        printf("\nRe-enable collection of all metrics: ---------------------------\n");
        CHECK_ERROR_BREAK (collector,
            EnableMetrics(ComSafeArrayAsInParam(baseMetrics),
                          ComSafeArrayAsInParam(objects),
                          ComSafeArrayAsOutParam(affectedMetrics)) );
        listAffectedMetrics(virtualBox,
                            ComSafeArrayAsInParam(affectedMetrics));
        affectedMetrics.setNull();
        RTThreadSleep(5000); // Sleep for 5 seconds
        queryMetrics(virtualBox, collector, ComSafeArrayAsInParam(objects));

        // Power off
        printf ("Press enter to power off VM...");
        getchar();
        CHECK_RC (console->PowerDown());
        printf ("Press enter to close this session...");
        getchar();
        session->Close();
    } while (false);
#endif /* VBOX_WITH_RESOURCE_USAGE_API */
#if 0
    // check of OVF appliance handling
    ///////////////////////////////////////////////////////////////////////////
    do
    {
        Bstr ovf = argc > 1 ? argv [1] : "someOVF.ovf";
        RTPrintf ("Try to open %ls ...\n", ovf.raw());

        ComPtr <IAppliance> appliance;
        CHECK_ERROR_BREAK (virtualBox,
                           CreateAppliance (appliance.asOutParam()));
        CHECK_ERROR_BREAK (appliance,
                           Read (ovf));
        Bstr path;
        CHECK_ERROR_BREAK (appliance, COMGETTER (Path)(path.asOutParam()));
        RTPrintf ("Successfully opened %ls.\n", path.raw());
        CHECK_ERROR_BREAK (appliance,
                           Interpret());
        RTPrintf ("Successfully interpreted %ls.\n", path.raw());
        RTPrintf ("Appliance:\n");
        // Fetch all disks
        com::SafeArray<BSTR> retDisks;
        CHECK_ERROR_BREAK (appliance,
                           COMGETTER (Disks)(ComSafeArrayAsOutParam  (retDisks)));
        if (retDisks.size() > 0)
        {
            RTPrintf ("Disks:");
            for (unsigned i = 0; i < retDisks.size(); i++)
                printf (" %ls", Bstr (retDisks [i]).raw());
            RTPrintf ("\n");
        }
        /* Fetch all virtual system descriptions */
        com::SafeIfaceArray<IVirtualSystemDescription> retVSD;
        CHECK_ERROR_BREAK (appliance,
                           COMGETTER (VirtualSystemDescriptions) (ComSafeArrayAsOutParam (retVSD)));
        if (retVSD.size() > 0)
        {
            for (unsigned i = 0; i < retVSD.size(); ++i)
            {
                com::SafeArray<VirtualSystemDescriptionType_T> retTypes;
                com::SafeArray<BSTR> retRefValues;
                com::SafeArray<BSTR> retOrigValues;
                com::SafeArray<BSTR> retAutoValues;
                com::SafeArray<BSTR> retConfiguration;
                CHECK_ERROR_BREAK (retVSD [i],
                                   GetDescription (ComSafeArrayAsOutParam (retTypes),
                                                   ComSafeArrayAsOutParam (retRefValues),
                                                   ComSafeArrayAsOutParam (retOrigValues),
                                                   ComSafeArrayAsOutParam (retAutoValues),
                                                   ComSafeArrayAsOutParam (retConfiguration)));

                RTPrintf ("VirtualSystemDescription:\n");
                for (unsigned a = 0; a < retTypes.size(); ++a)
                {
                    printf (" %d %ls %ls %ls\n",
                            retTypes [a],
                            Bstr (retOrigValues [a]).raw(),
                            Bstr (retAutoValues [a]).raw(),
                            Bstr (retConfiguration [a]).raw());
                }
                /* Show warnings from interpret */
                com::SafeArray<BSTR> retWarnings;
                CHECK_ERROR_BREAK (retVSD [i],
                                   GetWarnings(ComSafeArrayAsOutParam (retWarnings)));
                if (retWarnings.size() > 0)
                {
                    RTPrintf ("The following warnings occurs on interpret:\n");
                    for (unsigned r = 0; r < retWarnings.size(); ++r)
                        RTPrintf ("%ls\n", Bstr (retWarnings [r]).raw());
                    RTPrintf ("\n");
                }
            }
            RTPrintf ("\n");
        }
        RTPrintf ("Try to import the appliance ...\n");
        ComPtr<IProgress> progress;
        CHECK_ERROR_BREAK (appliance,
                           ImportMachines (progress.asOutParam()));
        CHECK_ERROR (progress, WaitForCompletion (-1));
        if (SUCCEEDED (rc))
        {
            /* Check if the import was successfully */
            progress->COMGETTER (ResultCode)(&rc);
            if (FAILED (rc))
            {
                com::ProgressErrorInfo info (progress);
                if (info.isBasicAvailable())
                    RTPrintf ("Error: failed to import appliance. Error message: %lS\n", info.getText().raw());
                else
                    RTPrintf ("Error: failed to import appliance. No error message available!\n");
            }else
                RTPrintf ("Successfully imported the appliance.\n");
        }

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

#ifdef VBOX_WITH_RESOURCE_USAGE_API
static void queryMetrics (ComPtr<IVirtualBox> aVirtualBox,
                          ComPtr <IPerformanceCollector> collector,
                          ComSafeArrayIn (IUnknown *, objects))
{
    HRESULT rc;

    //Bstr metricNames[] = { L"CPU/Load/User:avg,CPU/Load/System:avg,CPU/Load/Idle:avg,RAM/Usage/Total,RAM/Usage/Used:avg" };
    Bstr metricNames[] = { L"*" };
    com::SafeArray<BSTR> metrics (1);
    metricNames[0].cloneTo (&metrics [0]);
    com::SafeArray<BSTR>          retNames;
    com::SafeIfaceArray<IUnknown> retObjects;
    com::SafeArray<BSTR>          retUnits;
    com::SafeArray<ULONG>         retScales;
    com::SafeArray<ULONG>         retSequenceNumbers;
    com::SafeArray<ULONG>         retIndices;
    com::SafeArray<ULONG>         retLengths;
    com::SafeArray<LONG>          retData;
    CHECK_ERROR (collector, QueryMetricsData(ComSafeArrayAsInParam(metrics),
                                             ComSafeArrayInArg(objects),
                                             ComSafeArrayAsOutParam(retNames),
                                             ComSafeArrayAsOutParam(retObjects),
                                             ComSafeArrayAsOutParam(retUnits),
                                             ComSafeArrayAsOutParam(retScales),
                                             ComSafeArrayAsOutParam(retSequenceNumbers),
                                             ComSafeArrayAsOutParam(retIndices),
                                             ComSafeArrayAsOutParam(retLengths),
                                             ComSafeArrayAsOutParam(retData)) );
    RTPrintf("Object     Metric               Values\n"
             "---------- -------------------- --------------------------------------------\n");
    for (unsigned i = 0; i < retNames.size(); i++)
    {
        Bstr metricUnit(retUnits[i]);
        Bstr metricName(retNames[i]);
        RTPrintf("%-10ls %-20ls ", getObjectName(aVirtualBox, retObjects[i]).raw(), metricName.raw());
        const char *separator = "";
        for (unsigned j = 0; j < retLengths[i]; j++)
        {
            if (retScales[i] == 1)
                RTPrintf("%s%d %ls", separator, retData[retIndices[i] + j], metricUnit.raw());
            else
                RTPrintf("%s%d.%02d%ls", separator, retData[retIndices[i] + j] / retScales[i],
                         (retData[retIndices[i] + j] * 100 / retScales[i]) % 100, metricUnit.raw());
            separator = ", ";
        }
        RTPrintf("\n");
    }
}

static Bstr getObjectName(ComPtr<IVirtualBox> aVirtualBox,
                                  ComPtr<IUnknown> aObject)
{
    HRESULT rc;

    ComPtr<IHost> host = aObject;
    if (!host.isNull())
        return Bstr("host");

    ComPtr<IMachine> machine = aObject;
    if (!machine.isNull())
    {
        Bstr name;
        CHECK_ERROR(machine, COMGETTER(Name)(name.asOutParam()));
        if (SUCCEEDED(rc))
            return name;
    }
    return Bstr("unknown");
}

static void listAffectedMetrics(ComPtr<IVirtualBox> aVirtualBox,
                                ComSafeArrayIn(IPerformanceMetric*, aMetrics))
{
    HRESULT rc;
    com::SafeIfaceArray<IPerformanceMetric> metrics(ComSafeArrayInArg(aMetrics));
    if (metrics.size())
    {
        ComPtr<IUnknown> object;
        Bstr metricName;
        RTPrintf("The following metrics were modified:\n\n"
                 "Object     Metric\n"
                 "---------- --------------------\n");
        for (size_t i = 0; i < metrics.size(); i++)
        {
            CHECK_ERROR(metrics[i], COMGETTER(Object)(object.asOutParam()));
            CHECK_ERROR(metrics[i], COMGETTER(MetricName)(metricName.asOutParam()));
            RTPrintf("%-10ls %-20ls\n",
                getObjectName(aVirtualBox, object).raw(), metricName.raw());
        }
        RTPrintf("\n");
    }
    else
    {
        RTPrintf("No metrics match the specified filter!\n");
    }
}

#endif /* VBOX_WITH_RESOURCE_USAGE_API */
/* vim: set shiftwidth=4 tabstop=4 expandtab: */
