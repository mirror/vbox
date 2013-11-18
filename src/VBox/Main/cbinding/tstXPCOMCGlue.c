/* $Revision$ */
/** @file tstXPCOMCGlue.c
 * Demonstrator program to illustrate use of C bindings of Main API, including
 * how to retrieve all available error information.
 *
 * This includes code which shows how to handle active event listeners
 * (event delivery through callbacks), which is not so frequently seen in
 * other samples which mostly use passive event listeners.
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

struct IEventListenerDemo_vtbl
{
    struct IEventListener_vtbl ieventlistener;
};

typedef struct IEventListenerDemo
{
    struct IEventListenerDemo_vtbl *vtbl;

    int refcount;
} IEventListenerDemo;

/**
 * Event handler function
 */
static nsresult IEventListenerDemo_HandleEvent(IEventListener *pThis, IEvent *event)
{
    enum VBoxEventType evType;
    nsresult rc;

    if (!event)
    {
        printf("event null\n");
        return NS_OK;
    }

    evType = VBoxEventType_Invalid;
    rc = event->vtbl->GetType(event, &evType);
    if (NS_FAILED(rc))
    {
        printf("cannot get event type, rc=%#x\n", rc);
        return NS_OK;
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
                return NS_OK;
            }
            if (!ev)
            {
                printf("StateChangedEvent reference null\n");
                return NS_OK;
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

    return NS_OK;
}

static nsresult IEventListenerDemo_QueryInterface(nsISupports *pThis, const nsID *iid, void **resultp)
{
    static const nsID ieventListenerUUID = IEVENTLISTENER_IID;
    static const nsID isupportIID = NS_ISUPPORTS_IID;

    /* match iid */
    if (    memcmp(iid, &ieventListenerUUID, sizeof(nsID)) == 0
        ||  memcmp(iid, &isupportIID, sizeof(nsID)) == 0)
    {
        pThis->vtbl->AddRef(pThis);
        *resultp = pThis;
        return NS_OK;
    }

    return NS_NOINTERFACE;
}

static nsresult IEventListenerDemo_AddRef(nsISupports *pThis)
{
    return ++(((IEventListenerDemo *)pThis)->refcount);
}

static nsresult IEventListenerDemo_Release(nsISupports *pThis)
{
    nsresult c;

    c = --(((IEventListenerDemo *)pThis)->refcount);
    if (c == 0)
        free(pThis);
    return c;
}

struct IEventListenerDemo_vtbl_int_gcc
{
    ptrdiff_t offset_to_top;
    void *typeinfo;
    struct IEventListenerDemo_vtbl vtbl;
};

static struct IEventListenerDemo_vtbl_int_gcc g_IEventListenerDemo_vtbl_int_gcc =
{
    0,      /* offset_to_top */
    NULL,   /* typeinfo, not vital */
    {
        {
            {
                IEventListenerDemo_QueryInterface,
                IEventListenerDemo_AddRef,
                IEventListenerDemo_Release
            },
            IEventListenerDemo_HandleEvent
        }
    }
};

/**
 * Signal handler, terminate event listener.
 *
 * @param  iSig     The signal number (ignored).
 */
static void sigIntHandler(int iSig)
{
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
            IEventListenerDemo *consoleListener = NULL;
            consoleListener = calloc(1, sizeof(IEventListenerDemo));
            if (consoleListener)
            {
                consoleListener->vtbl = &(g_IEventListenerDemo_vtbl_int_gcc.vtbl);
                consoleListener->vtbl->ieventlistener.nsisupports.AddRef((nsISupports *)consoleListener);

                rc = es->vtbl->RegisterListener(es, (IEventListener *)consoleListener,
                                                sizeof(interestingEvents) / sizeof(interestingEvents[0]),
                                                interestingEvents, 1 /* active */);
                if (NS_SUCCEEDED(rc))
                {
                    /* Just wait here for events, no easy way to do this better
                     * as there's not much to do after this completes. */
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
                            if (NS_SUCCEEDED(rc))
                                queue->vtbl->HandleEvent(queue, pEvent);
                        }
                    }
                    signal(SIGINT, SIG_DFL);
                }
                es->vtbl->UnregisterListener(es, (IEventListener *)consoleListener);
                consoleListener->vtbl->ieventlistener.nsisupports.Release((nsISupports *)consoleListener);
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
 * Print detailed error information if available.
 * @param   pszExecutable   string with the executable name
 * @param   pszErrorMsg     string containing the code location specific error message
 * @param   rc              XPCOM result code
 */
static void PrintErrorInfo(const char *pszExecutable, const char *pszErrorMsg, nsresult rc)
{
    nsIException *ex;
    nsresult rc2 = NS_OK;
    fprintf(stderr, "%s: %s (rc=%#010x)\n", pszExecutable, pszErrorMsg, (unsigned)rc);
    rc2 = g_pVBoxFuncs->pfnGetException(&ex);
    if (NS_SUCCEEDED(rc2))
    {
        static const nsID vbei = IVIRTUALBOXERRORINFO_IID;
        IVirtualBoxErrorInfo *ei;
        rc2 = ex->vtbl->nsisupports.QueryInterface((nsISupports *)ex, &vbei, (void **)&ei);
        if (NS_FAILED(rc2))
            ei = NULL;
        if (ei)
        {
            /* got extended error info, maybe multiple infos */
            do
            {
                PRInt32 resultCode = NS_OK;
                PRUnichar *componentUtf16 = NULL;
                char *component = NULL;
                PRUnichar *textUtf16 = NULL;
                char *text = NULL;
                IVirtualBoxErrorInfo *ei_next = NULL;
                fprintf(stderr, "Extended error info (IVirtualBoxErrorInfo):\n");

                ei->vtbl->GetResultCode(ei, &resultCode);
                fprintf(stderr, "  resultCode=%#010x\n", (unsigned)resultCode);

                ei->vtbl->GetComponent(ei, &componentUtf16);
                g_pVBoxFuncs->pfnUtf16ToUtf8(componentUtf16, &component);
                g_pVBoxFuncs->pfnComUnallocMem(componentUtf16);
                fprintf(stderr, "  component=%s\n", component);
                g_pVBoxFuncs->pfnUtf8Free(component);

                ei->vtbl->GetText(ei, &textUtf16);
                g_pVBoxFuncs->pfnUtf16ToUtf8(textUtf16, &text);
                g_pVBoxFuncs->pfnComUnallocMem(textUtf16);
                fprintf(stderr, "  text=%s\n", text);
                g_pVBoxFuncs->pfnUtf8Free(text);

                rc2 = ei->vtbl->GetNext(ei, &ei_next);
                if (NS_FAILED(rc2))
                    ei_next = NULL;
                ei->vtbl->nsiexception.nsisupports.Release((nsISupports *)ei);
                ei = ei_next;
            }
            while (ei);
        }
        else
        {
            /* got basic error info */
            nsresult resultCode = NS_OK;
            PRUnichar *messageUtf16 = NULL;
            char *message = NULL;
            fprintf(stderr, "Basic error info (nsIException):\n");

            ex->vtbl->GetResult(ex, &resultCode);
            fprintf(stderr, "  resultCode=%#010x\n", resultCode);

            ex->vtbl->GetMessage(ex, &messageUtf16);
            g_pVBoxFuncs->pfnUtf16ToUtf8(messageUtf16, &message);
            g_pVBoxFuncs->pfnComUnallocMem(messageUtf16);
            fprintf(stderr, "  message=%s\n", message);
            g_pVBoxFuncs->pfnUtf8Free(message);
        }

        ex->vtbl->nsisupports.Release((nsISupports *)ex);
        g_pVBoxFuncs->pfnClearException();
    }
}

/**
 * Start a VM.
 *
 * @param   argv0       executable name
 * @param   virtualBox  ptr to IVirtualBox object
 * @param   session     ptr to ISession object
 * @param   id          identifies the machine to start
 * @param   queue       ptr to event queue
 */
static void startVM(const char *argv0, IVirtualBox *virtualBox, ISession *session, PRUnichar *id, nsIEventQueue *queue)
{
    nsresult rc;
    IMachine  *machine    = NULL;
    IProgress *progress   = NULL;
    PRUnichar *env        = NULL;
    PRUnichar *sessionType;

    rc = virtualBox->vtbl->FindMachine(virtualBox, id, &machine);
    if (NS_FAILED(rc) || !machine)
    {
        PrintErrorInfo(argv0, "Error: Couldn't get the Machine reference", rc);
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
    if (NS_SUCCEEDED(rc))
    {
        PRBool completed;
        PRInt32 resultCode;

        printf("Waiting for the remote session to open...\n");
        progress->vtbl->WaitForCompletion(progress, -1);

        rc = progress->vtbl->GetCompleted(progress, &completed);
        if (NS_FAILED(rc))
            fprintf(stderr, "Error: GetCompleted status failed\n");

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
            fprintf(stderr, "VM process has been successfully started\n");

            /* Kick off the event listener demo part, which is quite separate.
             * Ignore it if you need a more basic sample. */
            registerEventListener(virtualBox, session, id, queue);
        }
        progress->vtbl->nsisupports.Release((nsISupports *)progress);
    }
    else
        PrintErrorInfo(argv0, "Error: LaunchVMProcess failed", rc);

    /* It's important to always release resources. */
    machine->vtbl->nsisupports.Release((nsISupports *)machine);
}

/**
 * List the registered VMs.
 *
 * @param   argv0       executable name
 * @param   virtualBox  ptr to IVirtualBox object
 * @param   session     ptr to ISession object
 * @param   queue       ptr to event queue
 */
static void listVMs(const char *argv0, IVirtualBox *virtualBox, ISession *session, nsIEventQueue *queue)
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
        PrintErrorInfo(argv0, "could not get list of machines", rc);
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
            g_pVBoxFuncs->pfnComUnallocMem(machineNameUtf16);
            printf("\tName:        %s\n", machineName);
            g_pVBoxFuncs->pfnUtf8Free(machineName);
        }
        else
        {
            printf("\tName:        <inaccessible>\n");
        }

        {
            PRUnichar *uuidUtf16;
            char      *uuidUtf8;

            machine->vtbl->GetId(machine, &uuidUtf16);
            g_pVBoxFuncs->pfnUtf16ToUtf8(uuidUtf16, &uuidUtf8);
            g_pVBoxFuncs->pfnComUnallocMem(uuidUtf16);
            printf("\tUUID:        %s\n", uuidUtf8);
            g_pVBoxFuncs->pfnUtf8Free(uuidUtf8);
        }

        if (isAccessible)
        {
            {
                PRUnichar *configFileUtf16;
                char      *configFileUtf8;

                machine->vtbl->GetSettingsFilePath(machine, &configFileUtf16);
                g_pVBoxFuncs->pfnUtf16ToUtf8(configFileUtf16, &configFileUtf8);
                g_pVBoxFuncs->pfnComUnallocMem(configFileUtf16);
                printf("\tConfig file: %s\n", configFileUtf8);
                g_pVBoxFuncs->pfnUtf8Free(configFileUtf8);
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
                g_pVBoxFuncs->pfnComUnallocMem(typeId);
                osType->vtbl->GetDescription(osType, &osNameUtf16);
                g_pVBoxFuncs->pfnUtf16ToUtf8(osNameUtf16,&osName);
                g_pVBoxFuncs->pfnComUnallocMem(osNameUtf16);
                printf("\tGuest OS:    %s\n\n", osName);
                g_pVBoxFuncs->pfnUtf8Free(osName);

                osType->vtbl->nsisupports.Release((nsISupports *)osType);
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
            startVM(argv0, virtualBox, session, uuidUtf16, queue);
            g_pVBoxFuncs->pfnComUnallocMem(uuidUtf16);
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
    if (machines)
        g_pVBoxFuncs->pfnComUnallocMem(machines);
}

/* Main - Start the ball rolling. */

int main(int argc, char **argv)
{
    IVirtualBoxClient *vboxclient = NULL;
    IVirtualBox *vbox            = NULL;
    ISession   *session          = NULL;
    nsIEventQueue *queue         = NULL;
    PRUint32    revision         = 0;
    PRUnichar  *versionUtf16     = NULL;
    PRUnichar  *homefolderUtf16  = NULL;
    nsresult    rc;     /* Result code of various function (method) calls. */

    printf("Starting main()\n");

    if (VBoxCGlueInit() != 0)
    {
        fprintf(stderr, "%s: FATAL: VBoxCGlueInit failed: %s\n",
                argv[0], g_szVBoxErrMsg);
        return EXIT_FAILURE;
    }

    {
        unsigned ver = g_pVBoxFuncs->pfnGetVersion();
        printf("VirtualBox version: %u.%u.%u\n", ver / 1000000, ver / 1000 % 1000, ver % 1000);
        ver = g_pVBoxFuncs->pfnGetAPIVersion();
        printf("VirtualBox API version: %u.%u\n", ver / 1000, ver % 1000);
    }

    g_pVBoxFuncs->pfnClientInitialize(IVIRTUALBOXCLIENT_IID_STR, &vboxclient);
    if (vboxclient == NULL)
    {
        fprintf(stderr, "%s: FATAL: could not get VirtualBoxClient reference\n", argv[0]);
        return EXIT_FAILURE;
    }

    printf("----------------------------------------------------\n");

    rc = vboxclient->vtbl->GetVirtualBox(vboxclient, &vbox);
    if (NS_FAILED(rc) || !vbox)
    {
        PrintErrorInfo(argv[0], "FATAL: could not get VirtualBox reference", rc);
        return EXIT_FAILURE;
    }
    rc = vboxclient->vtbl->GetSession(vboxclient, &session);
    if (NS_FAILED(rc) || !session)
    {
        PrintErrorInfo(argv[0], "FATAL: could not get Session reference", rc);
        return EXIT_FAILURE;
    }

    g_pVBoxFuncs->pfnGetEventQueue(&queue);

    /*
     * Now ask for revision, version and home folder information of
     * this vbox. Were not using fancy macros here so it
     * remains easy to see how we access C++'s vtable.
     */

    /* 1. Revision */

    rc = vbox->vtbl->GetRevision(vbox, &revision);
    if (NS_SUCCEEDED(rc))
        printf("\tRevision: %u\n", revision);
    else
        PrintErrorInfo(argv[0], "GetRevision() failed", rc);

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
        PrintErrorInfo(argv[0], "GetVersion() failed", rc);

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
        PrintErrorInfo(argv[0], "GetHomeFolder() failed", rc);

    listVMs(argv[0], vbox, session, queue);
    session->vtbl->UnlockMachine(session);

    printf("----------------------------------------------------\n");

    /*
     * Do as mom told us: always clean up after yourself.
     */

    if (session)
    {
        session->vtbl->nsisupports.Release((nsISupports *)session);
        session = NULL;
    }
    if (vbox)
    {
        vbox->vtbl->nsisupports.Release((nsISupports *)vbox);
        vbox = NULL;
    }
    if (vboxclient)
    {
        vboxclient->vtbl->nsisupports.Release((nsISupports *)vboxclient);
        vboxclient = NULL;
    }
    g_pVBoxFuncs->pfnClientUninitialize();
    VBoxCGlueTerm();
    printf("Finished main()\n");

    return 0;
}
/* vim: set ts=4 sw=4 et: */
