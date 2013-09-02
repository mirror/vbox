/* $Revision$ */
/** @file tstXPCOMCEvent.c
 * Demonstrator program to illustrate use of C bindings of Main API,
 * and in particular how to handle active event listeners (event delivery
 * through callbacks). The code is derived from tstXPCOMCGlue.c, so keep
 * the diffs to the original code as small as possible.
 *
 * Linux only at the moment due to shared library magic in the Makefile.
 */

/*
 * Copyright (C) 2009-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "VBoxXPCOMCGlue.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/poll.h>

/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Set by signal handler. */
static volatile int g_fStop = 0;

int volatile g_refcount = 0;

/* #define for printing nsID type UUID's */

#define printUUID(iid) \
{\
    printf(#iid ": {%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}\n",\
           (unsigned)(iid)->m0,\
           (unsigned)(iid)->m1,\
           (unsigned)(iid)->m2,\
           (unsigned)(iid)->m3[0],\
           (unsigned)(iid)->m3[1],\
           (unsigned)(iid)->m3[2],\
           (unsigned)(iid)->m3[3],\
           (unsigned)(iid)->m3[4],\
           (unsigned)(iid)->m3[5],\
           (unsigned)(iid)->m3[6],\
           (unsigned)(iid)->m3[7]);\
}\

static const char *GetStateName(PRUint32 machineState)
{
    switch (machineState)
    {
        case MachineState_Null:                return "<null>";
        case MachineState_PoweredOff:          return "PoweredOff";
        case MachineState_Saved:               return "Saved";
        case MachineState_Teleported:          return "Teleported";
        case MachineState_Aborted:             return "Aborted";
        case MachineState_Running:             return "Running";
        case MachineState_Paused:              return "Paused";
        case MachineState_Stuck:               return "Stuck";
        case MachineState_Teleporting:         return "Teleporting";
        case MachineState_LiveSnapshotting:    return "LiveSnapshotting";
        case MachineState_Starting:            return "Starting";
        case MachineState_Stopping:            return "Stopping";
        case MachineState_Saving:              return "Saving";
        case MachineState_Restoring:           return "Restoring";
        case MachineState_TeleportingPausedVM: return "TeleportingPausedVM";
        case MachineState_TeleportingIn:       return "TeleportingIn";
        case MachineState_FaultTolerantSyncing: return "FaultTolerantSyncing";
        case MachineState_DeletingSnapshotOnline: return "DeletingSnapshotOnline";
        case MachineState_DeletingSnapshotPaused: return "DeletingSnapshotPaused";
        case MachineState_RestoringSnapshot:   return "RestoringSnapshot";
        case MachineState_DeletingSnapshot:    return "DeletingSnapshot";
        case MachineState_SettingUp:           return "SettingUp";
        default:                               return "no idea";
    }
}

/**
 * Event handler function
 */
static nsresult HandleEvent(IEventListener *pThis, IEvent *event)
{
    enum VBoxEventType evType;
    nsresult rc;

    if (!event)
    {
        printf("event null\n");
        return 0;
    }

    evType = VBoxEventType_Invalid;
    rc = event->vtbl->GetType(event, &evType);
    if (NS_FAILED(rc))
    {
        printf("cannot get event type, rc=%#x\n", rc);
        return 0;
    }

    switch (evType)
    {
        case VBoxEventType_OnMousePointerShapeChanged:
            printf("OnMousePointerShapeChanged\n");
            break;

        case VBoxEventType_OnMouseCapabilityChanged:
            printf("OnMouseCapabilityChanged\n");
            break;

        case VBoxEventType_OnKeyboardLedsChanged:
            printf("OnMouseCapabilityChanged\n");
            break;

        case VBoxEventType_OnStateChanged:
        {
            static const nsID istateChangedEventUUID = ISTATECHANGEDEVENT_IID;
            IStateChangedEvent *ev = NULL;
            enum MachineState state;
            rc = event->vtbl->nsisupports.QueryInterface((nsISupports *)event, &istateChangedEventUUID, (void **)&ev);
            if (NS_FAILED(rc))
            {
                printf("cannot get StateChangedEvent interface, rc=%#x\n", rc);
                return 0;
            }
            if (!ev)
            {
                printf("StateChangedEvent reference null\n");
                return 0;
            }
            rc = ev->vtbl->GetState(ev, &state);
            if (NS_FAILED(rc))
                printf("warning: cannot get state, rc=%#x\n", rc);
            ev->vtbl->ievent.nsisupports.Release((nsISupports *)ev);
            printf("OnStateChanged: %s\n", GetStateName(state));

            fflush(stdout);
            if (   state == MachineState_PoweredOff
                || state == MachineState_Saved
                || state == MachineState_Teleported
                || state == MachineState_Aborted
               )
                g_fStop = 1;
            break;
        }

        case VBoxEventType_OnAdditionsStateChanged:
            printf("OnAdditionsStateChanged\n");
            break;

        case VBoxEventType_OnNetworkAdapterChanged:
            printf("OnNetworkAdapterChanged\n");
            break;

        case VBoxEventType_OnSerialPortChanged:
            printf("OnSerialPortChanged\n");
            break;

        case VBoxEventType_OnParallelPortChanged:
            printf("OnParallelPortChanged\n");
            break;

        case VBoxEventType_OnStorageControllerChanged:
            printf("OnStorageControllerChanged\n");
            break;

        case VBoxEventType_OnMediumChanged:
            printf("OnMediumChanged\n");
            break;

        case VBoxEventType_OnVRDEServerChanged:
            printf("OnVRDEServerChanged\n");
            break;

        case VBoxEventType_OnUSBControllerChanged:
            printf("OnUSBControllerChanged\n");
            break;

        case VBoxEventType_OnUSBDeviceStateChanged:
            printf("OnUSBDeviceStateChanged\n");
            break;

        case VBoxEventType_OnSharedFolderChanged:
            printf("OnSharedFolderChanged\n");
            break;

        case VBoxEventType_OnRuntimeError:
            printf("OnRuntimeError\n");
            break;

        case VBoxEventType_OnCanShowWindow:
            printf("OnCanShowWindow\n");
            break;

        case VBoxEventType_OnShowWindow:
            printf("OnShowWindow\n");
            break;

        default:
            printf("unknown event: %d\n", evType);
    }

    return 0;
}

static nsresult AddRef(nsISupports *pThis)
{
    nsresult c;

    c = ++g_refcount;
    printf("AddRef: %d\n", c);
    return c;
}

static nsresult Release(nsISupports *pThis)
{
    nsresult c;

    c = --g_refcount;
    printf("Release: %d\n", c);
    if (c == 0)
    {
        /* delete object */
        free(pThis->vtbl);
        free(pThis);
    }
    return c;
}

static nsresult QueryInterface(nsISupports *pThis, const nsID *iid, void **resultp)
{
    static const nsID ieventListenerUUID = IEVENTLISTENER_IID;
    static const nsID isupportIID = NS_ISUPPORTS_IID;

    /* match iid */
    if (    memcmp(iid, &ieventListenerUUID, sizeof(nsID)) == 0
        ||  memcmp(iid, &isupportIID, sizeof(nsID)) == 0)
    {
        ++g_refcount;
        printf("QueryInterface: %d\n", g_refcount);
        *resultp = pThis;
        return NS_OK;
    }

    /* printf("event listener QueryInterface didn't find a matching interface\n"); */
    printUUID(iid);
    printUUID(&ieventListenerUUID);
    return NS_NOINTERFACE;
}

/**
 * Signal handler, terminate event listener.
 *
 * @param  iSig     The signal number (ignored).
 */
static void sigIntHandler(int iSig)
{
    printf("sigIntHandler\n");
    (void)iSig;
    g_fStop = 1;
}

/**
 * Register event listener for the selected VM.
 *
 * @param   virtualBox ptr to IVirtualBox object
 * @param   session    ptr to ISession object
 * @param   id         identifies the machine to start
 * @param   queue      handle to the event queue
 */
static void registerEventListener(IVirtualBox *virtualBox, ISession *session, PRUnichar *machineId, nsIEventQueue *queue)
{
    IConsole *console = NULL;
    nsresult rc;

    rc = session->vtbl->GetConsole(session, &console);
    if ((NS_SUCCEEDED(rc)) && console)
    {
        IEventSource *es = NULL;
        rc = console->vtbl->GetEventSource(console, &es);
        if (NS_SUCCEEDED(rc) && es)
        {
            PRUint32 interestingEvents[] =
                {
                    VBoxEventType_OnMousePointerShapeChanged,
                    VBoxEventType_OnMouseCapabilityChanged,
                    VBoxEventType_OnKeyboardLedsChanged,
                    VBoxEventType_OnStateChanged,
                    VBoxEventType_OnAdditionsStateChanged,
                    VBoxEventType_OnNetworkAdapterChanged,
                    VBoxEventType_OnSerialPortChanged,
                    VBoxEventType_OnParallelPortChanged,
                    VBoxEventType_OnStorageControllerChanged,
                    VBoxEventType_OnMediumChanged,
                    VBoxEventType_OnVRDEServerChanged,
                    VBoxEventType_OnUSBControllerChanged,
                    VBoxEventType_OnUSBDeviceStateChanged,
                    VBoxEventType_OnSharedFolderChanged,
                    VBoxEventType_OnRuntimeError,
                    VBoxEventType_OnCanShowWindow,
                    VBoxEventType_OnShowWindow
                };
            IEventListener *consoleListener = NULL;
            consoleListener = calloc(1, sizeof(IEventListener));
            consoleListener->vtbl = calloc(1, sizeof(struct IEventListener_vtbl));

            if (consoleListener && consoleListener->vtbl)
            {
                consoleListener->vtbl->nsisupports.AddRef = &AddRef;
                consoleListener->vtbl->nsisupports.Release = &Release;
                consoleListener->vtbl->nsisupports.QueryInterface = &QueryInterface;
                consoleListener->vtbl->HandleEvent = &HandleEvent;
                g_refcount = 1;

                rc = es->vtbl->RegisterListener(es, consoleListener,
                                                sizeof(interestingEvents) / sizeof(interestingEvents[0]),
                                                interestingEvents, 1 /* active */);
                if (NS_SUCCEEDED(rc))
                {
                    /* crude way to show how it works, but any
                     * great ideas anyone?
                     */
                    PRInt32 fd;
                    int ret;

                    printf("Entering event loop, PowerOff the machine to exit or press Ctrl-C to terminate\n");
                    fflush(stdout);
                    signal(SIGINT, sigIntHandler);

                    fd = queue->vtbl->GetEventQueueSelectFD(queue);
                    if (fd >= 0)
                    {
                        while (!g_fStop)
                        {
                            struct pollfd pfd;

                            pfd.fd = fd;
                            pfd.events = POLLIN | POLLERR | POLLHUP;
                            pfd.revents = 0;

                            ret = poll(&pfd, 1, 250);

                            if (ret <= 0)
                                continue;

                            if (pfd.revents & POLLHUP)
                                g_fStop = 1;

                            queue->vtbl->ProcessPendingEvents(queue);
                        }
                    }
                    else
                    {
                        while (!g_fStop)
                        {
                            PLEvent *pEvent = NULL;
                            rc = queue->vtbl->WaitForEvent(queue, &pEvent);
                            /*printf("event: %p rc=%x\n", (void *)pEvent, rc);*/
                            if (NS_SUCCEEDED(rc))
                                queue->vtbl->HandleEvent(queue, pEvent);
                        }
                    }
                    signal(SIGINT, SIG_DFL);
                }
                es->vtbl->UnregisterListener(es, consoleListener);
                consoleListener->vtbl->nsisupports.Release((nsISupports *)consoleListener);
            }
            else
            {
                printf("Failed while allocating memory for console event listener.\n");
            }
            es->vtbl->nsisupports.Release((nsISupports *)es);
        }
        console->vtbl->nsisupports.Release((nsISupports *)console);
    }
}

/**
 * Start a VM.
 *
 * @param   virtualBox ptr to IVirtualBox object
 * @param   session    ptr to ISession object
 * @param   id         identifies the machine to start
 * @param   queue      handle to the event queue
 */

static void startVM(IVirtualBox *virtualBox, ISession *session, PRUnichar *id, nsIEventQueue *queue)
{
    nsresult rc;
    IMachine  *machine    = NULL;
    IProgress *progress   = NULL;
    PRUnichar *env        = NULL;
    PRUnichar *sessionType;

    rc = virtualBox->vtbl->FindMachine(virtualBox, id, &machine);

    if (NS_FAILED(rc) || !machine)
    {
        fprintf(stderr, "Error: Couldn't get the machine handle.\n");
        return;
    }

    g_pVBoxFuncs->pfnUtf8ToUtf16("gui", &sessionType);

    rc = machine->vtbl->LaunchVMProcess(machine,
        session,
        sessionType,
        env,
        &progress
    );

    g_pVBoxFuncs->pfnUtf16Free(sessionType);

    if (NS_FAILED(rc))
    {
        fprintf(stderr, "Error: OpenRemoteSession failed.\n");
    }
    else
    {
        PRBool completed;
        PRInt32 resultCode;

        printf("Waiting for the remote session to open...\n");
        progress->vtbl->WaitForCompletion(progress, -1);

        rc = progress->vtbl->GetCompleted(progress, &completed);
        if (NS_FAILED(rc))
        {
            fprintf (stderr, "Error: GetCompleted status failed.\n");
        }

        progress->vtbl->GetResultCode(progress, &resultCode);
        if (NS_FAILED(resultCode))
        {
            IVirtualBoxErrorInfo *errorInfo;
            PRUnichar *textUtf16;
            char *text;

            progress->vtbl->GetErrorInfo(progress, &errorInfo);
            errorInfo->vtbl->GetText(errorInfo, &textUtf16);
            g_pVBoxFuncs->pfnUtf16ToUtf8(textUtf16, &text);
            printf("Error: %s\n", text);

            g_pVBoxFuncs->pfnComUnallocMem(textUtf16);
            g_pVBoxFuncs->pfnUtf8Free(text);
        }
        else
        {
            fprintf(stderr, "Remote session has been successfully opened.\n");
            registerEventListener(virtualBox, session, id, queue);
        }
        progress->vtbl->nsisupports.Release((nsISupports *)progress);
    }

    /* It's important to always release resources. */
    machine->vtbl->nsisupports.Release((nsISupports *)machine);
}

/**
 * List the registered VMs.
 *
 * @param   virtualBox ptr to IVirtualBox object
 * @param   session    ptr to ISession object
 * @param   queue      handle to the event queue
 */
static void listVMs(IVirtualBox *virtualBox, ISession *session, nsIEventQueue *queue)
{
    nsresult rc;
    IMachine **machines = NULL;
    PRUint32 machineCnt = 0;
    PRUint32 i;
    unsigned start_id;

    /*
     * Get the list of all registered VMs.
     */

    rc = virtualBox->vtbl->GetMachines(virtualBox, &machineCnt, &machines);
    if (NS_FAILED(rc))
    {
        fprintf(stderr, "could not get list of machines, rc=%08x\n",
            (unsigned)rc);
        return;
    }

    if (machineCnt == 0)
    {
        printf("\tNo VMs\n");
        return;
    }

    printf("VM List:\n\n");

    /*
     * Iterate through the collection.
     */

    for (i = 0; i < machineCnt; ++i)
    {
        IMachine *machine      = machines[i];
        PRBool    isAccessible = PR_FALSE;

        printf("\tMachine #%u\n", (unsigned)i);

        if (!machine)
        {
            printf("\t(skipped, NULL)\n");
            continue;
        }

        machine->vtbl->GetAccessible(machine, &isAccessible);

        if (isAccessible)
        {
            PRUnichar *machineNameUtf16;
            char *machineName;

            machine->vtbl->GetName(machine, &machineNameUtf16);
            g_pVBoxFuncs->pfnUtf16ToUtf8(machineNameUtf16,&machineName);
            printf("\tName:        %s\n", machineName);

            g_pVBoxFuncs->pfnUtf8Free(machineName);
            g_pVBoxFuncs->pfnComUnallocMem(machineNameUtf16);
        }
        else
        {
            printf("\tName:        <inaccessible>\n");
        }

        {
            PRUnichar *uuidUtf16 = NULL;
            char      *uuidUtf8  = NULL;

            machine->vtbl->GetId(machine, &uuidUtf16);
            g_pVBoxFuncs->pfnUtf16ToUtf8(uuidUtf16, &uuidUtf8);
            printf("\tUUID:        %s\n", uuidUtf8);

            g_pVBoxFuncs->pfnUtf8Free(uuidUtf8);
            g_pVBoxFuncs->pfnUtf16Free(uuidUtf16);
        }

        if (isAccessible)
        {
            {
                PRUnichar *configFile;
                char      *configFile1 = calloc((size_t)64, (size_t)1);

                machine->vtbl->GetSettingsFilePath(machine, &configFile);
                g_pVBoxFuncs->pfnUtf16ToUtf8(configFile, &configFile1);
                printf("\tConfig file: %s\n", configFile1);

                free(configFile1);
                g_pVBoxFuncs->pfnComUnallocMem(configFile);
            }

            {
                PRUint32 memorySize;

                machine->vtbl->GetMemorySize(machine, &memorySize);
                printf("\tMemory size: %uMB\n", memorySize);
            }

            {
                PRUnichar *typeId;
                PRUnichar *osNameUtf16;
                char *osName;
                IGuestOSType *osType = NULL;

                machine->vtbl->GetOSTypeId(machine, &typeId);
                virtualBox->vtbl->GetGuestOSType(virtualBox, typeId, &osType);
                osType->vtbl->GetDescription(osType, &osNameUtf16);
                g_pVBoxFuncs->pfnUtf16ToUtf8(osNameUtf16,&osName);
                printf("\tGuest OS:    %s\n\n", osName);

                osType->vtbl->nsisupports.Release((nsISupports *)osType);
                g_pVBoxFuncs->pfnUtf8Free(osName);
                g_pVBoxFuncs->pfnComUnallocMem(osNameUtf16);
                g_pVBoxFuncs->pfnComUnallocMem(typeId);
            }
        }
    }

    /*
     * Let the user chose a machine to start.
     */

    printf("Type Machine# to start (0 - %u) or 'quit' to do nothing: ",
        (unsigned)(machineCnt - 1));
    fflush(stdout);

    if (scanf("%u", &start_id) == 1 && start_id < machineCnt)
    {
        IMachine *machine = machines[start_id];

        if (machine)
        {
            PRUnichar *uuidUtf16 = NULL;

            machine->vtbl->GetId(machine, &uuidUtf16);
            startVM(virtualBox, session, uuidUtf16, queue);

            g_pVBoxFuncs->pfnUtf16Free(uuidUtf16);
        }
    }

    /*
     * Don't forget to release the objects in the array.
     */

    for (i = 0; i < machineCnt; ++i)
    {
        IMachine *machine = machines[i];

        if (machine)
        {
            machine->vtbl->nsisupports.Release((nsISupports *)machine);
        }
    }
}

/* Main - Start the ball rolling. */

int main(int argc, char **argv)
{
    IVirtualBox *vbox            = NULL;
    ISession   *session          = NULL;
    nsIEventQueue *queue         = NULL;
    PRUint32    revision         = 0;
    PRUnichar  *versionUtf16     = NULL;
    PRUnichar  *homefolderUtf16  = NULL;
    nsresult    rc;     /* Result code of various function (method) calls. */

    printf("Starting Main\n");

    /*
     * VBoxComInitialize does all the necessary startup action and
     * provides us with pointers to vbox and session handles.
     * It should be matched by a call to VBoxComUninitialize(vbox)
     * when done.
     */

    if (VBoxCGlueInit() != 0)
    {
        fprintf(stderr, "%s: FATAL: VBoxCGlueInit failed: %s\n",
                argv[0], g_szVBoxErrMsg);
        return EXIT_FAILURE;
    }

    g_pVBoxFuncs->pfnComInitialize(IVIRTUALBOX_IID_STR, &vbox,
                                   ISESSION_IID_STR, &session);
    if (vbox == NULL)
    {
        fprintf(stderr, "%s: FATAL: could not get vbox handle\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (session == NULL)
    {
        fprintf(stderr, "%s: FATAL: could not get session handle\n", argv[0]);
        return EXIT_FAILURE;
    }
    g_pVBoxFuncs->pfnGetEventQueue(&queue);
    printf("Got the event queue: %p\n", (void *)queue);

    /*
     * Now ask for revision, version and home folder information of
     * this vbox. Were not using fancy macros here so it
     * remains easy to see how we access C++'s vtable.
     */

    printf("----------------------------------------------------\n");

    /* 1. Revision */

    rc = vbox->vtbl->GetRevision(vbox, &revision);
    if (NS_SUCCEEDED(rc))
    {
        printf("\tRevision: %u\n", revision);
    }
    else
    {
        fprintf(stderr, "%s: GetRevision() returned %08x\n",
            argv[0], (unsigned)rc);
    }

    /* 2. Version */

    rc = vbox->vtbl->GetVersion(vbox, &versionUtf16);
    if (NS_SUCCEEDED(rc))
    {
        char *version = NULL;
        g_pVBoxFuncs->pfnUtf16ToUtf8(versionUtf16, &version);
        printf("\tVersion: %s\n", version);
        g_pVBoxFuncs->pfnUtf8Free(version);
        g_pVBoxFuncs->pfnComUnallocMem(versionUtf16);
    }
    else
    {
        fprintf(stderr, "%s: GetVersion() returned %08x\n",
            argv[0], (unsigned)rc);
    }

    /* 3. Home Folder */

    rc = vbox->vtbl->GetHomeFolder(vbox, &homefolderUtf16);
    if (NS_SUCCEEDED(rc))
    {
        char *homefolder = NULL;
        g_pVBoxFuncs->pfnUtf16ToUtf8(homefolderUtf16, &homefolder);
        printf("\tHomeFolder: %s\n", homefolder);
        g_pVBoxFuncs->pfnUtf8Free(homefolder);
        g_pVBoxFuncs->pfnComUnallocMem(homefolderUtf16);
    }
    else
    {
        fprintf(stderr, "%s: GetHomeFolder() returned %08x\n",
            argv[0], (unsigned)rc);
    }

    listVMs(vbox, session, queue);
    session->vtbl->UnlockMachine(session);

    printf("----------------------------------------------------\n");

    /*
     * Do as mom told us: always clean up after yourself.
     */

    g_pVBoxFuncs->pfnComUninitialize();
    VBoxCGlueTerm();
    printf("Finished Main\n");

    return 0;
}
/* vim: set ts=4 sw=4 et: */
