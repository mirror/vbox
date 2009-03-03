/* $Revision$ */
/** @file tstdlOpen.c
 * Demonstrator program to illustrate use of C bindings of Main API
 * using dynamic linking (dlopen and friends).
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include "VirtualBox_CXPCOM.h"

static char *nsIDToString(nsID *guid);
static void listVMs(IVirtualBox *virtualBox, ISession *session);
static void startVM(IVirtualBox *virtualBox, ISession *session, nsID *id);

void (*VBoxComInitializePtr)(IVirtualBox **virtualBox, ISession **session);
void (*VBoxComUninitializePtr)(void);
void (*VBoxComUnallocMemPtr)(void *ptr);
void (*VBoxUtf16FreePtr)(PRUnichar *pwszString);
void (*VBoxUtf8FreePtr)(char *pszString);
int (*VBoxUtf16ToUtf8Ptr)(const PRUnichar *pwszString, char **ppszString);
int (*VBoxUtf8ToUtf16Ptr)(const char *pszString, PRUnichar **ppwszString);
const char * (*VBoxGetEnvPtr)(const char *pszVar);
int (*VBoxSetEnvPtr)(const char *pszVar, const char *pszValue);

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
            VBoxUtf16ToUtf8Ptr(machineNameUtf16,&machineName);
            printf("\tName:        %s\n", machineName);

            VBoxUtf8FreePtr(machineName);
            VBoxComUnallocMemPtr(machineNameUtf16);
        }
        else
        {
            printf("\tName:        <inaccessible>\n");
        }


        {
            nsID  *iid = NULL;
            char  *uuidString;

            machine->vtbl->GetId(machine, &iid);
            uuidString = nsIDToString(iid);
            printf("\tUUID:        %s\n", uuidString);

            free(uuidString);
            VBoxComUnallocMemPtr(iid);
        }

        if (isAccessible)
        {
            {
                PRUnichar *configFile;
                char      *configFile1 = calloc((size_t)64, (size_t)1);

                machine->vtbl->GetSettingsFilePath(machine, &configFile);
                VBoxUtf16ToUtf8Ptr(configFile, &configFile1);
                printf("\tConfig file: %s\n", configFile1);

                free(configFile1);
                VBoxComUnallocMemPtr(configFile);
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
                VBoxUtf16ToUtf8Ptr(osNameUtf16,&osName);
                printf("\tGuest OS:    %s\n\n", osName);

                osType->vtbl->nsisupports.Release((void *)osType);
                VBoxUtf8FreePtr(osName);
                VBoxComUnallocMemPtr(osNameUtf16);
                VBoxComUnallocMemPtr(typeId);
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
            nsID  *iid = NULL;

            machine->vtbl->GetId(machine, &iid);
            startVM(virtualBox, session, iid);

            VBoxComUnallocMemPtr(iid);
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
        fprintf(stderr, "Error: Couldn't get the machine handle.\n");
        return;
    }

    VBoxUtf8ToUtf16Ptr("gui", &sessionType);

    rc = virtualBox->vtbl->OpenRemoteSession(
        virtualBox,
        session,
        id,
        sessionType,
        env,
        &progress
    );

    VBoxUtf16FreePtr(sessionType);

    if (NS_FAILED(rc))
    {
        fprintf(stderr, "Error: OpenRemoteSession failed.\n");
    }
    else
    {
        PRBool completed;
        nsresult resultCode;

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
            VBoxUtf16ToUtf8Ptr(textUtf16, &text);
            printf("Error: %s\n", text);

            VBoxComUnallocMemPtr(textUtf16);
            VBoxUtf8FreePtr(text);
        }
        else
        {
            fprintf(stderr, "Remote session has been successfully opened.\n");
        }
        progress->vtbl->nsisupports.Release((void *)progress);
    }

    /* It's important to always release resources. */
    machine->vtbl->nsisupports.Release((void *)machine);
}

#ifdef USE_DYNAMIC_GLUE
/** The dlopen handle for VBoxXPCOM. */
void *g_hVBoxXPCOMC = NULL;
/** The last load error. */
char g_szVBoxXPCOMErrMsg[256];

#if defined(__linux__) || defined(__linux_gnu__) || defined(__sun__)
# define SYM_PREFIX     ""
# define DYNLIB_NAME    "VBoxXPCOMC.so"
#elif defined(__APPLE__)
# define SYM_PREFIX     "_"
# define DYNLIB_NAME    "VBoxXPCOMC.dylib"
#else
# error "Port me"
#endif

/**
 * Try load VBoxXPCOMC.so/dylib/dll from the specified location and resolve all
 * the symbols we need.
 *
 * @returns 0 on success, -1 on failure.
 * @param   pszHome         The director where to try load VBoxXPCOMC from. Can be NULL.
 * @param   pszMsgPrefix    Error message prefix. NULL means no error messages.
 */
static int tryLoadOne(const char *pszHome, const char *pszMsgPrefix)
{
    static struct
    {
        const char *pszSymbol;
        void **ppvSym;
    } const s_aSyms[] =
    {
        { SYM_PREFIX "VBoxComInitialize",           (void **)&VBoxComInitializePtr            },
        { SYM_PREFIX "VBoxComUninitialize",         (void **)&VBoxComUninitializePtr          },
        { SYM_PREFIX "VBoxComUnallocMem",           (void **)&VBoxComUnallocMemPtr            },
        { SYM_PREFIX "VBoxUtf16Free",               (void **)&VBoxUtf16FreePtr                },
        { SYM_PREFIX "VBoxUtf8Free",                (void **)&VBoxUtf8FreePtr                 },
        { SYM_PREFIX "VBoxUtf16ToUtf8",             (void **)&VBoxUtf16ToUtf8Ptr              },
        { SYM_PREFIX "VBoxUtf8ToUtf16",             (void **)&VBoxUtf8ToUtf16Ptr              },
        { SYM_PREFIX "VBoxGetEnv",                  (void **)&VBoxGetEnvPtr                   },
        { SYM_PREFIX "VBoxSetEnv",                  (void **)&VBoxSetEnvPtr                   }
    };
    size_t      cchHome = pszHome ? strlen(pszHome) : 0;
    size_t      cbBuf;
    char *      pszBuf;
    int         rc = 0;

    /*
     * Construct the full name.
     */

    cbBuf = cchHome + sizeof("/" DYNLIB_NAME);
    pszBuf = (char *)malloc(cbBuf);
    if (!pszBuf)
    {
        sprintf(g_szVBoxXPCOMErrMsg, "malloc(%u) failed", (unsigned)cbBuf);
        if (pszMsgPrefix)
            fprintf(stderr, "%s%s\n", pszMsgPrefix, g_szVBoxXPCOMErrMsg);
        return -1;
    }
    if (pszHome)
    {
        memcpy(pszBuf, pszHome, cchHome);
        pszBuf[cchHome] = '/';
        cchHome++;
    }
    memcpy(&pszBuf[cchHome], DYNLIB_NAME, sizeof(DYNLIB_NAME));

    /*
     * Try load it by that name, setting the VBOX_APP_HOME first (for now).
     */

    setenv("VBOX_APP_HOME", pszHome, 0 /* no need to overwrite */);
    g_hVBoxXPCOMC = dlopen(pszBuf, RTLD_NOW | RTLD_LOCAL);
    if (g_hVBoxXPCOMC)
    {
        unsigned i = sizeof(s_aSyms) / sizeof(s_aSyms[0]);
        while (i-- > 0)
        {
            void *pv = dlsym(g_hVBoxXPCOMC, s_aSyms[i].pszSymbol);
            if (!pv)
            {
                sprintf(g_szVBoxXPCOMErrMsg, "dlsym(%.80s/%.32s): %128s",
                        pszBuf, s_aSyms[i].pszSymbol, dlerror());
                if (pszMsgPrefix)
                    fprintf(stderr, "%s%s\n", pszMsgPrefix, g_szVBoxXPCOMErrMsg);
                rc = -1;
                break;
            }
            *s_aSyms[i].ppvSym = pv;
        }
    }
    else
    {
        sprintf(g_szVBoxXPCOMErrMsg, "dlopen(%.80s): %128s", pszBuf, dlerror());
        rc = -1;
    }
    free(pszBuf);
    return rc;
}

/**
 * Tries to locate and load VBoxXPCOMC.so/dylib/dll, resolving all the related
 * function pointers.
 *
 * @returns 0 on success, -1 on failure.
 * @param   pszMsgPrefix    Error message prefix. NULL means no error messages.
 *
 * @remark  This should be considered moved into a separate glue library since
 *          its its going to be pretty much the same for any user of VBoxXPCOMC
 *          and it will just cause trouble to have duplicate versions of this
 *          source code all around the place.
 */
static int tryLoad(const char *pszMsgPrefix)
{
    /*
     * If the user specifies the location, try only that.
     */

    const char *pszHome = getenv("VBOX_APP_HOME");
    if (pszHome)
        return tryLoadOne(pszHome, pszMsgPrefix);

    /*
     * Try the known standard locations.
     */

#if defined(__gnu__linux__) || defined(__linux__)
    if (tryLoadOne("/opt/VirtualBox", pszMsgPrefix) == 0)
        return 0;
    if (tryLoadOne("/usr/lib/virtualbox", pszMsgPrefix) == 0)
        return 0;
#elif defined(__sun__)
    if (tryLoadOne("/opt/VirtualBox/amd64", pszMsgPrefix) == 0)
        return 0;
    if (tryLoadOne("/opt/VirtualBox/i386", pszMsgPrefix) == 0)
        return 0;
#elif defined(__APPLE__)
    if (tryLoadOne("/Application/VirtualBox.app/Contents/MacOS", pszMsgPrefix) == 0)
        return 0;
#else
# error "port me"
#endif

    /*
     * Finally try the dynamic linker search path.
     */

    if (tryLoadOne(NULL, pszMsgPrefix) == 0)
        return 0;

    /* No luck, return failure. */
    if (pszMsgPrefix)
        fprintf(stderr, "%sFailed to locate VBoxXPCOMC\n", pszMsgPrefix);
    return -1;
}
#endif


/* Main - Start the ball rolling. */

int main(int argc, char **argv)
{
    IVirtualBox *vbox            = NULL;
    ISession   *session          = NULL;
    PRUint32    revision         = 0;
    PRUnichar  *versionUtf16     = NULL;
    PRUnichar  *homefolderUtf16  = NULL;
    nsresult    rc;     /* Result code of various function (method) calls. */
#ifndef USE_DYNAMIC_GLUE
    void       *xpcomHandle      = NULL;
    const char *xpcomdlError     = NULL;
    struct stat stIgnored;

    /*
     * Guess where VirtualBox is installed not mentioned in the environment.
     * (This will be moved to VBoxComInitialize later.)
     */

    if (stat("/opt/VirtualBox/VBoxXPCOMC.so", &stIgnored) == 0)
    {
        xpcomHandle = dlopen ("/opt/VirtualBox/VBoxXPCOMC.so", RTLD_NOW);
        if (!xpcomHandle)
        {
            fprintf(stderr, "%s: FATAL: could not open VBoxXPCOMC.so library: %s\n", argv[0],dlerror());
            return EXIT_FAILURE;
        }
        *(void **) (&VBoxSetEnvPtr) = dlsym(xpcomHandle, "VBoxSetEnv");
        if ((xpcomdlError = dlerror()) != NULL)  {
            fprintf(stderr, "%s: FATAL: could not open VBoxXPCOMC.so library: %s\n", argv[0],xpcomdlError);
            return EXIT_FAILURE;
        }
        VBoxSetEnvPtr("VBOX_APP_HOME","/opt/VirtualBox/");
    }
    else if (stat("/usr/lib/virtualbox/VBoxXPCOMC.so", &stIgnored) == 0)
    {
        xpcomHandle = dlopen ("/usr/lib/virtualbox/VBoxXPCOMC.so", RTLD_NOW);
        if (!xpcomHandle)
        {
            fprintf(stderr, "%s: FATAL: could not open VBoxXPCOMC.so library: %s\n", argv[0],dlerror());
            return EXIT_FAILURE;
        }
        *(void **) (&VBoxSetEnvPtr) = dlsym(xpcomHandle, "VBoxSetEnv");
        if ((xpcomdlError = dlerror()) != NULL)  {
            fprintf(stderr, "%s: FATAL: could not open VBoxXPCOMC.so library: %s\n", argv[0],xpcomdlError);
            return EXIT_FAILURE;
        }
        VBoxSetEnvPtr("VBOX_APP_HOME","/usr/lib/virtualbox/");
    }
#else
    /*
     * Initialize the dynamic linking glue.
     */

    if (tryLoad(NULL) != 0)
    {
        fprintf(stderr, "%s: FATAL: %s\n", argv[0], g_szVBoxXPCOMErrMsg);
        return EXIT_FAILURE;
    }
#endif

    printf("Starting Main\n");

    /*
     * VBoxComInitialize does all the necessary startup action and
     * provides us with pointers to vbox and session handles.
     * It should be matched by a call to VBoxComUninitialize(vbox)
     * when done.
     */

#ifndef USE_DYNAMIC_GLUE
    dlerror();    /* Clear any existing error */
    *(void **) (&VBoxComInitializePtr) = dlsym(xpcomHandle, "VBoxComInitialize");
    if ((xpcomdlError = dlerror()) != NULL)  {
        fprintf(stderr, "%s: FATAL: could not open VBoxXPCOMC.so library: %s\n", argv[0],xpcomdlError);
        return EXIT_FAILURE;
    }
#endif
    VBoxComInitializePtr(&vbox, &session);

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

#ifndef USE_DYNAMIC_GLUE
    *(void **) (&VBoxUtf16ToUtf8Ptr) = dlsym(xpcomHandle, "VBoxUtf16ToUtf8");
    if ((xpcomdlError = dlerror()) != NULL)  {
        fprintf(stderr, "%s: FATAL: could not open VBoxXPCOMC.so library: %s\n", argv[0],xpcomdlError);
        return EXIT_FAILURE;
    }
    *(void **) (&VBoxUtf8ToUtf16Ptr) = dlsym(xpcomHandle, "VBoxUtf8ToUtf16");
    if ((xpcomdlError = dlerror()) != NULL)  {
        fprintf(stderr, "%s: FATAL: could not open VBoxXPCOMC.so library: %s\n", argv[0],xpcomdlError);
        return EXIT_FAILURE;
    }
    *(void **) (&VBoxUtf8FreePtr) = dlsym(xpcomHandle, "VBoxUtf8Free");
    if ((xpcomdlError = dlerror()) != NULL)  {
        fprintf(stderr, "%s: FATAL: could not open VBoxXPCOMC.so library: %s\n", argv[0],xpcomdlError);
        return EXIT_FAILURE;
    }
    *(void **) (&VBoxUtf16FreePtr) = dlsym(xpcomHandle, "VBoxUtf16Free");
    if ((xpcomdlError = dlerror()) != NULL)  {
        fprintf(stderr, "%s: FATAL: could not open VBoxXPCOMC.so library: %s\n", argv[0],xpcomdlError);
        return EXIT_FAILURE;
    }
    *(void **) (&VBoxComUnallocMemPtr) = dlsym(xpcomHandle, "VBoxComUnallocMem");
    if ((xpcomdlError = dlerror()) != NULL)  {
        fprintf(stderr, "%s: FATAL: could not open VBoxXPCOMC.so library: %s\n", argv[0],xpcomdlError);
        return EXIT_FAILURE;
    }
#endif

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
        VBoxUtf16ToUtf8Ptr(versionUtf16, &version);
        printf("\tVersion: %s\n", version);
        VBoxUtf8FreePtr(version);
        VBoxComUnallocMemPtr(versionUtf16);
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
        VBoxUtf16ToUtf8Ptr(homefolderUtf16, &homefolder);
        printf("\tHomeFolder: %s\n", homefolder);
        VBoxUtf8FreePtr(homefolder);
        VBoxComUnallocMemPtr(homefolderUtf16);
    }
    else
    {
        fprintf(stderr, "%s: GetHomeFolder() returned %08x\n",
            argv[0], (unsigned)rc);
    }

    listVMs(vbox, session);
    session->vtbl->Close(session);

    printf("----------------------------------------------------\n");

    /*
     * Do as mom told us: always clean up after yourself.
     */

#ifndef USE_DYNAMIC_GLUE
    *(void **) (&VBoxComUninitializePtr) = dlsym(xpcomHandle, "VBoxComUninitialize");
    if ((xpcomdlError = dlerror()) != NULL)  {
        fprintf(stderr, "%s: FATAL: could not open VBoxXPCOMC.so library: %s\n", argv[0],xpcomdlError);
        return EXIT_FAILURE;
    }
#endif
    VBoxComUninitializePtr();
#ifndef USE_DYNAMIC_GLUE
    dlclose(xpcomHandle);
#else
    dlclose(g_hVBoxXPCOMC);
#endif
    printf("Finished Main\n");

    return 0;
}
/* vim: set ts=4 sw=4 et: */
