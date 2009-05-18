/* $Revision$ */
/** @file tstXPCOMCGlue.c
 * Demonstrator program to illustrate use of C bindings of Main API.
 *
 * Linux only at the moment due to shared library magic in the Makefile.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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
*   Internal Functions                                                         *
*******************************************************************************/
static void listVMs(IVirtualBox *virtualBox, ISession *session, nsIEventQueue *queue);
static void registerCallBack(IVirtualBox *virtualBox, ISession *session, PRUnichar *machineId, nsIEventQueue *queue);
static void startVM(IVirtualBox *virtualBox, ISession *session, PRUnichar *id, nsIEventQueue *queue);

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

/**
 * Callback functions
 */
static const char *GetStateName(PRUint32 machineState)
{
    switch (machineState)
    {
        case MachineState_Null:                return "<null>";
        case MachineState_PoweredOff:          return "PoweredOff";
        case MachineState_Saved:               return "Saved";
        case MachineState_Aborted:             return "Aborted";
        case MachineState_Running:             return "Running";
        case MachineState_Paused:              return "Paused";
        case MachineState_Stuck:               return "Stuck";
        case MachineState_Starting:            return "Starting";
        case MachineState_Stopping:            return "Stopping";
        case MachineState_Saving:              return "Saving";
        case MachineState_Restoring:           return "Restoring";
        case MachineState_Discarding:          return "Discarding";
        case MachineState_SettingUp:           return "SettingUp";
        default:                               return "no idea";
    }
}

static nsresult OnMousePointerShapeChange(
    IConsoleCallback *pThis,
    PRBool visible,
    PRBool alpha,
    PRUint32 xHot,
    PRUint32 yHot,
    PRUint32 width,
    PRUint32 height,
    PRUint8 * shape
) {
    printf("OnMousePointerShapeChange\n");
    return 0;
}

static nsresult OnMouseCapabilityChange(
    IConsoleCallback *pThis,
    PRBool supportsAbsolute,
    PRBool needsHostCursor
) {
    printf("OnMouseCapabilityChange\n");
    return 0;
}

static nsresult OnKeyboardLedsChange(
    IConsoleCallback *pThis,
    PRBool numLock,
    PRBool capsLock,
    PRBool scrollLock
) {
    printf("OnMouseCapabilityChange\n");
    return 0;
}

static nsresult OnStateChange(
    IConsoleCallback *pThis,
    PRUint32 state
) {
    printf("OnStateChange: %s\n", GetStateName(state));
    fflush(stdout);
    if (state == MachineState_PoweredOff)
        g_fStop = 1;
    return 0;
}

static nsresult OnAdditionsStateChange(IConsoleCallback *pThis )
{
    printf("OnAdditionsStateChange\n");
    return 0;
}

static nsresult OnDVDDriveChange(IConsoleCallback *pThis )
{
    printf("OnDVDDriveChange \n");
    return 0;
}

static nsresult OnFloppyDriveChange(IConsoleCallback *pThis )
{
    printf("OnFloppyDriveChange \n");
    return 0;
}

static nsresult OnNetworkAdapterChange(
    IConsoleCallback *pThis,
    INetworkAdapter * networkAdapter
) {
    printf("OnNetworkAdapterChange\n");
    return 0;
}

static nsresult OnSerialPortChange(
    IConsoleCallback *pThis,
    ISerialPort * serialPort
) {
    printf("OnSerialPortChange\n");
    return 0;
}

static nsresult OnParallelPortChange(
    IConsoleCallback *pThis,
    IParallelPort * parallelPort
) {
    printf("OnParallelPortChange\n");
    return 0;
}

static nsresult OnStorageControllerChange(IConsoleCallback *pThis )
{
    printf("OnStorageControllerChange\n");
    return 0;
}

static nsresult OnVRDPServerChange(IConsoleCallback *pThis )
{
    printf("OnVRDPServerChange\n");
    return 0;
}

static nsresult OnUSBControllerChange(IConsoleCallback *pThis )
{
    printf("OnUSBControllerChange\n");
    return 0;
}

static nsresult OnUSBDeviceStateChange(
    IConsoleCallback *pThis,
    IUSBDevice * device,
    PRBool attached,
    IVirtualBoxErrorInfo * error
) {
    printf("OnUSBDeviceStateChange\n");
    return 0;
}

static nsresult OnSharedFolderChange(
    IConsoleCallback *pThis,
    PRUint32 scope
) {
    printf("OnSharedFolderChange\n");
    return 0;
}

static nsresult OnRuntimeError(
    IConsoleCallback *pThis,
    PRBool fatal,
    PRUnichar * id,
    PRUnichar * message
) {
    printf("OnRuntimeError\n");
    return 0;
}

static nsresult OnCanShowWindow(
    IConsoleCallback *pThis,
    PRBool * canShow
) {
    printf("OnCanShowWindow\n");
    return 0;
}

static nsresult OnShowWindow(
    IConsoleCallback *pThis,
    PRUint64 * winId
) {
    printf("OnShowWindow\n");
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
    static const nsID ivirtualboxCallbackUUID = IVIRTUALBOXCALLBACK_IID;
    static const nsID isupportIID = NS_ISUPPORTS_IID;

    /* match iid */
    if (    memcmp(iid, &ivirtualboxCallbackUUID, sizeof(nsID)) == 0
        ||  memcmp(iid, &isupportIID, sizeof(nsID)) == 0)
    {
        ++g_refcount;
        printf("QueryInterface: %d\n", g_refcount);
        *resultp = pThis;
        return NS_OK;
    }

    /* printf("vboxCallback QueryInterface didn't find a matching interface\n"); */
    printUUID(iid);
    printUUID(&ivirtualboxCallbackUUID);
    return NS_NOINTERFACE;
}

/**
 * Signal callback.
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
 * Register callback functions for the selected VM.
 *
 * @param   virtualBox ptr to IVirtualBox object
 * @param   session    ptr to ISession object
 * @param   id         identifies the machine to start
 * @param   queue      handle to the event queue
 */
static void registerCallBack(IVirtualBox *virtualBox, ISession *session, PRUnichar *machineId, nsIEventQueue *queue)
{
    IConsole *console = NULL;
    nsresult rc;

    rc = session->vtbl->GetConsole(session, &console);
    if ((NS_SUCCEEDED(rc)) && console)
    {
        IConsoleCallback *consoleCallback = NULL;

        consoleCallback = calloc(1, sizeof(IConsoleCallback));
        consoleCallback->vtbl = calloc(1, sizeof(struct IConsoleCallback_vtbl));

        if (consoleCallback && consoleCallback->vtbl)
        {
            consoleCallback->vtbl->nsisupports.AddRef = &AddRef;
            consoleCallback->vtbl->nsisupports.Release = &Release;
            consoleCallback->vtbl->nsisupports.QueryInterface = &QueryInterface;
            consoleCallback->vtbl->OnMousePointerShapeChange = &OnMousePointerShapeChange;
            consoleCallback->vtbl->OnMouseCapabilityChange = &OnMouseCapabilityChange;
            consoleCallback->vtbl->OnKeyboardLedsChange =&OnKeyboardLedsChange;
            consoleCallback->vtbl->OnStateChange = &OnStateChange;
            consoleCallback->vtbl->OnAdditionsStateChange = &OnAdditionsStateChange;
            consoleCallback->vtbl->OnDVDDriveChange = &OnDVDDriveChange;
            consoleCallback->vtbl->OnFloppyDriveChange = &OnFloppyDriveChange;
            consoleCallback->vtbl->OnNetworkAdapterChange = &OnNetworkAdapterChange;
            consoleCallback->vtbl->OnSerialPortChange = &OnSerialPortChange;
            consoleCallback->vtbl->OnParallelPortChange = &OnParallelPortChange;
            consoleCallback->vtbl->OnStorageControllerChange = &OnStorageControllerChange;
            consoleCallback->vtbl->OnVRDPServerChange = &OnVRDPServerChange;
            consoleCallback->vtbl->OnUSBControllerChange = &OnUSBControllerChange;
            consoleCallback->vtbl->OnUSBDeviceStateChange = &OnUSBDeviceStateChange;
            consoleCallback->vtbl->OnSharedFolderChange = &OnSharedFolderChange;
            consoleCallback->vtbl->OnRuntimeError = &OnRuntimeError;
            consoleCallback->vtbl->OnCanShowWindow = &OnCanShowWindow;
            consoleCallback->vtbl->OnShowWindow = &OnShowWindow;
            g_refcount = 1;

            rc = console->vtbl->RegisterCallback(console, consoleCallback);
            if (NS_SUCCEEDED(rc))
            {
                /* crude way to show how it works, but any
                 * great ideas anyone?
                 */
                PRInt32 fd;

                printf("Entering event loop, PowerOff the machine to exit or press Ctrl-C to terminate\n");
                fflush(stdout);
                signal(SIGINT, sigIntHandler);

                fd = queue->vtbl->GetEventQueueSelectFD(queue);
                if (fd >= 0)
                {
                    while (!g_fStop)
                    {
                        struct pollfd   pfd;
                        pfd.fd = fd;
                        pfd.events = POLLIN | POLLERR | POLLHUP;
                        pfd.revents = 0;
                        poll(&pfd, 1, 250);
                        rc = queue->vtbl->ProcessPendingEvents(queue);
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
            console->vtbl->UnregisterCallback(console, consoleCallback);
            consoleCallback->vtbl->nsisupports.Release((nsISupports *)consoleCallback);
        }
        else
        {
            printf("Failed while allocating memory for console Callback.\n");
        }
    }
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

                osType->vtbl->nsisupports.Release((void *)osType);
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

    rc = virtualBox->vtbl->GetMachine(virtualBox, id, &machine);

    if (NS_FAILED(rc) || !machine)
    {
        fprintf(stderr, "Error: Couldn't get the machine handle.\n");
        return;
    }

    g_pVBoxFuncs->pfnUtf8ToUtf16("gui", &sessionType);

    rc = virtualBox->vtbl->OpenRemoteSession(
        virtualBox,
        session,
        id,
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
            registerCallBack(virtualBox, session, id, queue);
        }
        progress->vtbl->nsisupports.Release((void *)progress);
    }

    /* It's important to always release resources. */
    machine->vtbl->nsisupports.Release((void *)machine);
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
    session->vtbl->Close(session);

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
