/** @file tstLinuxC.c
 *
 * Demonstrator program to illustrate use of C bindings of Main API.
 * Linux only at the moment due to shared library magic in the Makefile.
 *
 * $Id$
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <iconv.h>
#include "tstLinuxC.h"

static char *nsIDToString(nsID *guid);
static void listVMs(IVirtualBox *virtualBox, ISession *session);
static void startVM(IVirtualBox *virtualBox, ISession *session, nsID *id);

/**
 * Helper function to convert an nsID into a human readable string.
 *
 * @returns result string, allocated. Has to be freed using free()
 * @param   guid Pointer to nsID that will be converted.
 */
static char *nsIDToString(nsID *guid)
{
    /* Avoid magic number 39. Yes, sizeof "literal" includes the NUL byte. */
    char *res = malloc(sizeof "{12345678-1234-1234-1234-123456789012}");

    if (res != NULL)
    {
        sprintf(res, "{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
                (unsigned)guid->m0, (unsigned)guid->m1, (unsigned)guid->m2,
                (unsigned)guid->m3[0], (unsigned)guid->m3[1],
                (unsigned)guid->m3[2], (unsigned)guid->m3[3],
                (unsigned)guid->m3[4], (unsigned)guid->m3[5],
                (unsigned)guid->m3[6], (unsigned)guid->m3[7]);
    }
    return res;
}

/**
 * List the registered VMs.
 *
 * @param   virtualBox ptr to IVirtualBox object
 * @param   session    ptr to ISession object
 */
static void listVMs(IVirtualBox *virtualBox, ISession *session)
{
    nsresult rc;
    IMachine **machines = NULL;
    PRUint32 machineCnt = 0;
    PRUint32 i;
    unsigned start_id;

    /*
     * Get the list of all registered VMs.
     */

    rc = virtualBox->vtbl->GetMachines2(virtualBox, &machineCnt, &machines);
    if (NS_FAILED(rc))
    {
        fprintf (stderr, "could not get list of machines, rc=%08x\n",
            (unsigned)rc);
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

        if (!machine) {
            printf("\t(skipped, NULL)\n");
            continue;
        }

        machine->vtbl->GetAccessible (machine, &isAccessible);

        if (isAccessible)
        {
            PRUnichar *machineName;

            machine->vtbl->GetName(machine, &machineName);
            printf("\tName:        %s\n", VBoxConvertPRUnichartoAscii(machineName));

            VBoxComUnallocStr(machineName);
        }
        else
        {
            printf("\tName:        <inaccessible>\n");
        }


        {
            nsID  *iid          = nsnull;
            char  *uuidString;

            machine->vtbl->GetId(machine, &iid);
            uuidString = nsIDToString(iid);
            printf("\tUUID:        %s\n", uuidString);

            free(uuidString);
            VBoxComUnallocIID(iid);
        }

        if (isAccessible)
        {
            {
                PRUnichar *configFile;
                char      *configFile1 = calloc((size_t)64, (size_t)1);

                machine->vtbl->GetSettingsFilePath(machine, &configFile);
                VBoxUtf16ToUtf8(configFile, &configFile1);
                printf("\tConfig file: %s\n", configFile1);

                free(configFile1);
                VBoxComUnallocStr(configFile);
            }

            {
                PRUint32 memorySize;

                machine->vtbl->GetMemorySize(machine, &memorySize);
                printf("\tMemory size: %uMB\n", memorySize);
            }

            {
                PRUnichar *typeId;
                PRUnichar *osName;
                IGuestOSType *osType = nsnull;

                machine->vtbl->GetOSTypeId(machine, &typeId);
                virtualBox->vtbl->GetGuestOSType(virtualBox, typeId, &osType);
                osType->vtbl->GetDescription(osType, &osName);
                printf("\tGuest OS:    %s\n\n", VBoxConvertPRUnichartoAscii(osName));

                osType->vtbl->nsisupports.Release((void *)osType);
                VBoxComUnallocStr(osName);
                VBoxComUnallocStr(typeId);
            }
        }
    }

    /*
     * Let the user chose a machine to start.
     */

    printf ("Type Machine# to start (0 - %u) or 'quit' to do nothing: ",
        (unsigned)(machineCnt - 1));
    fflush (stdout);

    if (scanf ("%u", &start_id) == 1 && start_id < machineCnt) {
        IMachine *machine = machines[start_id];

        if (machine) {
            nsID  *iid = nsnull;

            machine->vtbl->GetId(machine, &iid);
            startVM(virtualBox, session, iid);

            VBoxComUnallocIID(iid);
        }
    }

    /*
     * Don't forget to release the objects in the array.
     */

    for (i = 0; i < machineCnt; ++i) {
        IMachine *machine = machines[i];

        if (machine) {
            machine->vtbl->nsisupports.Release((void *)machine);
        }
    }
}

/**
 * Start a VM.
 *
 * @param   virtualBox ptr to IVirtualBox object
 * @param   session    ptr to ISession object
 * @param   id         identifies the machine to start
 */
static void startVM(IVirtualBox *virtualBox, ISession *session, nsID *id)
{
    nsresult rc;
    IMachine  *machine    = NULL;
    IProgress *progress   = NULL;
    PRUnichar *env        = NULL;
    PRUnichar *sessionType;

    rc = virtualBox->vtbl->GetMachine(virtualBox, id, &machine);

    if (NS_FAILED(rc) || !machine)
    {
        fprintf (stderr, "Error: Couldn't get the machine handle.\n");
        return;
    }

    VBoxStrToUtf16("gui", &sessionType);

    rc = virtualBox->vtbl->OpenRemoteSession(
        virtualBox,
        session,
        id,
        sessionType,
        env,
        &progress
    );

    VBoxUtf16Free(sessionType);

    if (NS_FAILED (rc))
    {
        fprintf (stderr, "Error: OpenRemoteSession failed.\n");
    }
    else
    {
        PRBool completed;
        nsresult resultCode;

        printf("Waiting for the remote session to open...\n");
        progress->vtbl->WaitForCompletion(progress, -1);

        rc = progress->vtbl->GetCompleted(progress, &completed);
        if (NS_FAILED (rc))
        {
            fprintf (stderr, "Error: GetCompleted status failed.\n");
        }

        progress->vtbl->GetResultCode(progress, &resultCode);
        if (NS_FAILED(resultCode))
        {
            IVirtualBoxErrorInfo *errorInfo;
            PRUnichar            *text;

            progress->vtbl->GetErrorInfo(progress, &errorInfo);
            errorInfo->vtbl->GetText(errorInfo, &text);
            printf ("[!] Text        = %s\n", VBoxConvertPRUnichartoAscii (text));
        }
        else
        {
            fprintf (stderr, "Remote session has been successfully opened.\n");
        }
        progress->vtbl->nsisupports.Release((void *)progress);
    }

    /* It's important to always release resources. */
    machine->vtbl->nsisupports.Release((void *)machine);
}

/* Main - Start the ball rolling. */

int main(int argc, char **argv)
{
    IVirtualBox *vbox     = NULL;
    ISession  *session    = NULL;
    PRUint32   revision   = 0;
    PRUnichar *version    = NULL;
    PRUnichar *homefolder = NULL;
    nsresult rc;     /* Result code of various function (method) calls. */

    printf("Starting Main\n");
    /*
     * VBoxComInitialize does all the necessary startup action and
     * provides us with pointers to vbox and session handles.
     * It should be matched by a call to VBoxComUninitialize(vbox)
     * when done.
     */
    VBoxComInitialize(&vbox, &session);

    if (vbox == NULL) {
        fprintf (stderr, "%s: FATAL: could not get vbox handle\n", argv[0]);
        exit (EXIT_FAILURE);
    }
    if (session == NULL) {
        fprintf (stderr, "%s: FATAL: could not get session handle\n", argv[0]);
        exit (EXIT_FAILURE);
    }

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

    rc = vbox->vtbl->GetVersion(vbox, &version);
    if (NS_SUCCEEDED(rc))
    {
        printf("\tVersion: %s\n", VBoxConvertPRUnichartoAscii(version));
    }
    else
    {
        fprintf(stderr, "%s: GetVersion() returned %08x\n",
            argv[0], (unsigned)rc);
    }
    VBoxComUnallocStr(version);

    /* 3. Home Folder */

    rc = vbox->vtbl->GetHomeFolder(vbox, &homefolder);
    if (NS_SUCCEEDED(rc))
    {
        printf("\tHomeFolder: %s\n", VBoxConvertPRUnichartoAscii(homefolder));
    }
    else
    {
        fprintf(stderr, "%s: GetHomeFolder() returned %08x\n",
            argv[0], (unsigned)rc);
    }
    VBoxComUnallocStr(homefolder);

    listVMs(vbox, session);
    session->vtbl->Close(session);

    printf("----------------------------------------------------\n");

    /*
     * Do as mom told us: always clean up after yourself.
     */
    VBoxComUninitialize();
    printf("Finished Main\n");

    return 0;
}
/* vim: set ts=4 sw=4 et: */
