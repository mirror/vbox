/** @file
 *
 * VBox - Main Program.
 *
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

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/runtime.h>
#include <VBox/vm.h>
#include <VBox/err.h>
#include <signal.h>
#include <stdio.h>
#ifdef _MSC_VER
# include <stdlib.h>
#else
# include <unistd.h>
#endif


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static void SignalInterrupt(int iSignalNo);


int main(int argc, char **argv)
{
    int rc = -1;

    /* todo: decide whether or not we're gonna have one executable
     *       which contains both GUI and VM backend or separate executables.
     *
     * In any case we're expecting this code to be rewritten as this is mostly
     * a sketch of what we currently think is going to happen when a VM is started.
     */

    /*
     * Init runtime.
     */
    rc = RTR3Init();
    if (!VBOX_SUCCESS(rc))
    {
        printf("fatal error: failed to initialize runtime. (rc=%d)\n", rc);
        return 1;
    }

    /*
     * Parse arguments.
     */
    char *pszVMConfig = NULL;
    for (int argi = 1; argi < argc; argi++)
    {
        if (argv[argi][0] == '-')
        {
            switch (argv[argi][1])
            {
                case '?':
                case 'h':
                case 'H':
                    printf("syntax: %s <VMConfig>\n", argv[0]);
                    return 1;

                default:
                    printf("syntax error: Unknown option %s\n", argv[argi]);
                    return 1;
            }
        }
        else
        {
            if (pszVMConfig)
            {
                printf("syntax error: Multiple VM configuration files specified!\n");
                return 1;
            }
            pszVMConfig = argv[argi];
        }
    }

    /*
     * Create the VM from the given config file.
     */
    PVM pVM = NULL;
    rc = VMR3Create(pszVMConfig, &pVM);
    if (VBOX_SUCCESS(rc))
    {
        /*
         * Run the VM.
         */
        rc = VMR3PowerOn(pVM);
        if (VBOX_SUCCESS(rc))
        {
            /*
             * Wait for user to press Control-C,
             * (or Control-Break if signal is available).
             */
            signal(SIGINT, SignalInterrupt);
            #ifdef SIGBREAK
            signal(SIGBREAK, SignalInterrupt);
            #endif
            #ifdef _MSC_VER
            _sleep(~0);
            #else
            pause();
            #endif
            rc = 0;
        }
        else
        {
            printf("fatal error: Failed to power on the VM! rc=%d\n", rc);
            rc = 1;
        }

        /*
         * Destroy the VM.
         */
        int rc2 = VMR3Destroy(pVM);
        if (!VBOX_SUCCESS(rc2))
        {
            printf("warning: VMR3Destroy() failed with rc=%d\n", rc);
            rc = 1;
        }
    }
    else
    {
        printf("fatal error: failed to create VM from %s, rc=%d\n", pszVMConfig, rc);
        rc = 1;
    }
    return rc;
}


/**
 * Signal handler.
 * @param   iSignalNo   Signal number.
 */
void SignalInterrupt(int iSignalNo)
{
    #ifdef SIGBREAK
    if (iSignalNo == SIGBREAK)
        printf("SIGBREAK!\n");
    else
        printf("SIGINT!\n");
    #else
    printf("SIGINT!\n");
    #endif

    /*
     * assuming BSD style signals here, meaning that execution
     * continues upon return....
     * I've no clue how that works on Win32 and linux though.
     */
    #ifdef SIG_ACK
    signal(iSignalNo, SIG_ACK);
    #endif
}

