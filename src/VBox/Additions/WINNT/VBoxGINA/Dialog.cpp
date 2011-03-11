/* $Id$ */
/** @file
 * VBoxGINA - Windows Logon DLL for VirtualBox, Dialog Code.
 */

/*
 *
 * Copyright (C) 2006-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <windows.h>
#include <stdio.h>      /* Needed for swprintf() */

#include "Dialog.h"
#include "WinWlx.h"
#include "Helper.h"
#include "VBoxGINA.h"


/*
 * Dialog IDs for legacy Windows OSes (e.g. NT 4.0).
 */
#define IDD_WLXDIAPLAYSASNOTICE_DIALOG    1400
#define IDD_WLXLOGGEDOUTSAS_DIALOG        1450
/** Change password dialog: To change the current
 *  account password. */
#define IDD_CHANGE_PASSWORD_DIALOG        1550
#define IDD_WLXLOGGEDONSAS_DIALOG         1650
/** Security dialog: To lock the workstation, log off
 *  change password, ... */
#define IDD_SECURITY_DIALOG               1800
/** Locked dialog: To unlock the currently lockted
 *  workstation. */
#define IDD_WLXWKSTALOCKEDSAS_DIALOG      1850
/** Shutdown dialog: To either restart, logoff current
 *  user or shutdown the workstation. */
#define IDD_SHUTDOWN_DIALOG               2200
/** Logoff dialog: "Do you really want to logoff?". */
#define IDD_LOGOFF_DIALOG                 2250


/*
 * Dialog IDs for Windows 2000 and up.
 */
#define IDD_WLXLOGGEDOUTSAS_DIALOG2       1500
/** Change password dialog: To change the current
 *  account password. */
#define IDD_CHANGE_PASSWORD_DIALOG2       1700
/** Locked dialog: To unlock the currently lockted
 *  workstation. */
#define IDD_WLXWKSTALOCKEDSAS_DIALOG2     1950


/*
 * Control IDs.
 */
#define IDC_WLXLOGGEDOUTSAS_USERNAME      1453
#define IDC_WLXLOGGEDOUTSAS_USERNAME2     1502
#define IDC_WLXLOGGEDOUTSAS_PASSWORD      1454
#define IDC_WLXLOGGEDOUTSAS_PASSWORD2     1503
#define IDC_WLXLOGGEDOUTSAS_DOMAIN        1455
#define IDC_WLXLOGGEDOUTSAS_DOMAIN2       1504

#define IDC_WKSTALOCKED_USERNAME          1953
#define IDC_WKSTALOCKED_PASSWORD          1954
#define IDC_WKSTALOCKEd_DOMAIN            1856
#define IDC_WKSTALOCKED_DOMAIN2           1956


/*
 * Own IDs.
 */
#define IDT_BASE                          WM_USER  + 1100 /* Timer ID base. */
#define IDT_LOGGEDONDLG_POLL              IDT_BASE + 1
#define IDT_LOCKEDDLG_POLL                IDT_BASE + 2

static DLGPROC g_pfnWlxLoggedOutSASDlgProc = NULL;
static DLGPROC g_pfnWlxLockedSASDlgProc = NULL;

static PWLX_DIALOG_BOX_PARAM g_pfnWlxDialogBoxParam = NULL;

int WINAPI MyWlxDialogBoxParam (HANDLE, HANDLE, LPWSTR, HWND, DLGPROC, LPARAM);

void hookDialogBoxes(PVOID pWinlogonFunctions, DWORD dwWlxVersion)
{
    Log(("VBoxGINA::hookDialogBoxes\n"));

    /* this is version dependent */
    switch (dwWlxVersion)
    {
        case WLX_VERSION_1_0:
        {
            g_pfnWlxDialogBoxParam = ((PWLX_DISPATCH_VERSION_1_0)pWinlogonFunctions)->WlxDialogBoxParam;
            ((PWLX_DISPATCH_VERSION_1_0)pWinlogonFunctions)->WlxDialogBoxParam = MyWlxDialogBoxParam;
            break;
        }

        case WLX_VERSION_1_1:
        {
            g_pfnWlxDialogBoxParam = ((PWLX_DISPATCH_VERSION_1_1)pWinlogonFunctions)->WlxDialogBoxParam;
            ((PWLX_DISPATCH_VERSION_1_1)pWinlogonFunctions)->WlxDialogBoxParam = MyWlxDialogBoxParam;
            break;
        }

        case WLX_VERSION_1_2:
        {
            g_pfnWlxDialogBoxParam = ((PWLX_DISPATCH_VERSION_1_2)pWinlogonFunctions)->WlxDialogBoxParam;
            ((PWLX_DISPATCH_VERSION_1_2)pWinlogonFunctions)->WlxDialogBoxParam = MyWlxDialogBoxParam;
            break;
        }

        case WLX_VERSION_1_3:
        {
            g_pfnWlxDialogBoxParam = ((PWLX_DISPATCH_VERSION_1_3)pWinlogonFunctions)->WlxDialogBoxParam;
            ((PWLX_DISPATCH_VERSION_1_3)pWinlogonFunctions)->WlxDialogBoxParam = MyWlxDialogBoxParam;
            break;
        }

        case WLX_VERSION_1_4:
        {
            g_pfnWlxDialogBoxParam = ((PWLX_DISPATCH_VERSION_1_4)pWinlogonFunctions)->WlxDialogBoxParam;
            ((PWLX_DISPATCH_VERSION_1_4)pWinlogonFunctions)->WlxDialogBoxParam = MyWlxDialogBoxParam;
            break;
        }

        default:
        {
            Log(("VBoxGINA::hookDialogBoxes: unrecognized version '%d', nothing hooked!\n", dwWlxVersion));
            /* not good, don't do anything */
            break;
        }
    }
}

//
// Redirected WlxLoggedOutSASDlgProc().
//
BOOL credentialsToUI(HWND hwndUserId, HWND hwndPassword, HWND hwndDomain)
{
    BOOL bIsFQDN = FALSE;
    wchar_t szUserFQDN[512]; /* VMMDEV_CREDENTIALS_STRLEN + 255 bytes max. for FQDN */
    if (hwndDomain)
    {
        /* search the domain combo box for our required domain and select it */
        Log(("VBoxGINA::MyWlxLoggedOutSASDlgProc: Trying to find domain entry in combo box ...\n"));
        DWORD dwIndex = (DWORD) SendMessage(hwndDomain, CB_FINDSTRING,
                                            0, (LPARAM)g_Domain);
        if (dwIndex != CB_ERR)
        {
            Log(("VBoxGINA::MyWlxLoggedOutSASDlgProc: Found domain at combo box pos %ld\n", dwIndex));
            SendMessage(hwndDomain, CB_SETCURSEL, (WPARAM) dwIndex, 0);
            EnableWindow(hwndDomain, FALSE);
        }
        else
        {
            Log(("VBoxGINA::MyWlxLoggedOutSASDlgProc: Domain not found in combo box ...\n"));

            /* If the domain value has a dot (.) in it, it is a FQDN (Fully Qualified Domain Name)
             * which will not work with the combo box selection because Windows only keeps the
             * NETBIOS names to the left most part of the domain name there. Of course a FQDN
             * then will not be found by the search in the block above.
             *
             * To solve this problem the FQDN domain value will be appended at the user name value
             * (Kerberos style) using an "@", e.g. "<user-name>@full.qualified.domain".
             *
             */
            size_t l = wcslen(g_Domain);
            if (l > 255)
                Log(("VBoxGINA::MyWlxLoggedOutSASDlgProc: Warning! FQDN is too long (max 255 bytes), will be truncated!\n"));

            if (wcslen(g_Username) > 0) /* We need a user name that we can use in caes of a FQDN */
            {
                if (l > 16) /* Domain name is longer than 16 chars, cannot be a NetBIOS name anymore */
                {
                    Log(("VBoxGINA::MyWlxLoggedOutSASDlgProc: Domain seems to be a FQDN (length)!\n"));
                    bIsFQDN = TRUE;
                }
                else if (   l > 0
                         && wcsstr(g_Domain, L".") != NULL) /* if we found a dot (.) in the domain name, this has to be a FQDN */
                {
                    Log(("VBoxGINA::MyWlxLoggedOutSASDlgProc: Domain seems to be a FQDN (dot)!\n"));
                    bIsFQDN = TRUE;
                }

                if (bIsFQDN)
                {
                    swprintf(szUserFQDN, sizeof(szUserFQDN) / sizeof(wchar_t), L"%s@%s", g_Username, g_Domain);
                    Log(("VBoxGINA::MyWlxLoggedOutSASDlgProc: FQDN user name is now: %s!\n", szUserFQDN));
                }
            }
        }
    }
    if (hwndUserId)
    {
        if (!bIsFQDN)
            SendMessage(hwndUserId, WM_SETTEXT, 0, (LPARAM)g_Username);
        else
            SendMessage(hwndUserId, WM_SETTEXT, 0, (LPARAM)szUserFQDN);
    }
    if (hwndPassword)
        SendMessage(hwndPassword, WM_SETTEXT, 0, (LPARAM)g_Password);

    return TRUE;
}

INT_PTR CALLBACK MyWlxLoggedOutSASDlgProc(HWND   hwndDlg,  // handle to dialog box
                                          UINT   uMsg,     // message
                                          WPARAM wParam,   // first message parameter
                                          LPARAM lParam)   // second message parameter
{
    BOOL bResult;
    static HWND s_hwndUserId, s_hwndPassword, s_hwndDomain = 0;

    /*Log(("VBoxGINA::MyWlxLoggedOutSASDlgProc\n"));*/

    //
    // Pass on to MSGINA first.
    //
    bResult = g_pfnWlxLoggedOutSASDlgProc(hwndDlg, uMsg, wParam, lParam);

    //
    // We are only interested in the WM_INITDIALOG message.
    //
    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            Log(("VBoxGINA::MyWlxLoggedOutSASDlgProc: got WM_INITDIALOG\n"));

            /* get the entry fields */
            s_hwndUserId = GetDlgItem(hwndDlg, IDC_WLXLOGGEDOUTSAS_USERNAME);
            if (!s_hwndUserId)
                s_hwndUserId = GetDlgItem(hwndDlg, IDC_WLXLOGGEDOUTSAS_USERNAME2);
            s_hwndPassword = GetDlgItem(hwndDlg, IDC_WLXLOGGEDOUTSAS_PASSWORD);
            if (!s_hwndPassword)
                s_hwndPassword = GetDlgItem(hwndDlg, IDC_WLXLOGGEDOUTSAS_PASSWORD2);
            s_hwndDomain = GetDlgItem(hwndDlg, IDC_WLXLOGGEDOUTSAS_DOMAIN);
            if (!s_hwndDomain)
                s_hwndDomain = GetDlgItem(hwndDlg, IDC_WLXLOGGEDOUTSAS_DOMAIN2);

            Log(("VBoxGINA::MyWlxLoggedOutSASDlgProc: hwndUserId: %x, hwndPassword: %d, hwndDomain: %d\n",
                 s_hwndUserId, s_hwndPassword, s_hwndDomain));

            /* terminate the credentials poller thread, it's done is job */
            credentialsPollerTerminate();

            if (credentialsAvailable())
            {
                /* query the credentials from VBox */
                if (credentialsRetrieve())
                {
                    /* fill in credentials to appropriate UI elements */
                    credentialsToUI(s_hwndUserId, s_hwndPassword, s_hwndDomain);

                    /* we got the credentials, null them out */
                    credentialsReset();

                    /* confirm the logon dialog, simulating the user pressing "OK" */
                    WPARAM wParam = MAKEWPARAM(IDOK, BN_CLICKED);
                    PostMessage(hwndDlg, WM_COMMAND, wParam, 0);
                }
            }
            else
            {
                /*
                 * The dialog is there but we don't have any credentials.
                 * Create a timer and poll for them.
                 */
                UINT_PTR uTimer = SetTimer(hwndDlg, IDT_LOGGEDONDLG_POLL, 200, NULL);
                if (!uTimer)
                    Log(("VBoxGINA::MyWlxLoggedOutSASDlgProc: failed creating timer! Last error: %ld\n",
                         GetLastError()));
            }
            break;
        }

        case WM_TIMER:
        {
            /* is it our credentials poller timer? */
            if (wParam == IDT_LOGGEDONDLG_POLL)
            {
                if (credentialsAvailable())
                {
                    if (credentialsRetrieve())
                    {
                        /* fill in credentials to appropriate UI elements */
                        credentialsToUI(s_hwndUserId, s_hwndPassword, s_hwndDomain);

                        /* we got the credentials, null them out */
                        credentialsReset();

                        /* confirm the logon dialog, simulating the user pressing "OK" */
                        WPARAM wParam = MAKEWPARAM(IDOK, BN_CLICKED);
                        PostMessage(hwndDlg, WM_COMMAND, wParam, 0);

                        /* we don't need the timer any longer */
                        KillTimer(hwndDlg, IDT_LOGGEDONDLG_POLL);
                    }
                }
            }
            break;
        }

        case WM_DESTROY:
            KillTimer(hwndDlg, IDT_LOGGEDONDLG_POLL);
            break;
    }
    return bResult;
}


INT_PTR CALLBACK MyWlxLockedSASDlgProc(HWND   hwndDlg,  // handle to dialog box
                                       UINT   uMsg,     // message
                                       WPARAM wParam,   // first message parameter
                                       LPARAM lParam)   // second message parameter
{
    BOOL bResult;
    static HWND s_hwndPassword = 0;

    /*Log(("VBoxGINA::MyWlxLockedSASDlgProc\n"));*/

    //
    // Pass on to MSGINA first.
    //
    bResult = g_pfnWlxLockedSASDlgProc(hwndDlg, uMsg, wParam, lParam);

    //
    // We are only interested in the WM_INITDIALOG message.
    //
    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            Log(("VBoxGINA::MyWlxLockedSASDlgProc: got WM_INITDIALOG\n"));

            /* get the entry fields */
            s_hwndPassword = GetDlgItem(hwndDlg, IDC_WKSTALOCKED_PASSWORD);
            Log(("VBoxGINA::MyWlxLockedSASDlgProc: hwndPassword: %d\n", s_hwndPassword));

            /* terminate the credentials poller thread, it's done is job */
            credentialsPollerTerminate();

            if (credentialsAvailable())
            {
                /* query the credentials from VBox */
                if (credentialsRetrieve())
                {
                    /* fill in credentials to appropriate UI elements */
                    credentialsToUI(NULL /* User ID */, s_hwndPassword, NULL /* Domain */);

                    /* we got the credentials, null them out */
                    credentialsReset();

                    /* confirm the logon dialog, simulating the user pressing "OK" */
                    WPARAM wParam = MAKEWPARAM(IDOK, BN_CLICKED);
                    PostMessage(hwndDlg, WM_COMMAND, wParam, 0);
                }
            }
            else
            {
                /*
                 * The dialog is there but we don't have any credentials.
                 * Create a timer and poll for them.
                 */
                UINT_PTR uTimer = SetTimer(hwndDlg, IDT_LOCKEDDLG_POLL, 200, NULL);
                if (!uTimer)
                    Log(("VBoxGINA::MyWlxLockedSASDlgProc: failed creating timer! Last error: %ld\n",
                         GetLastError()));
            }
            break;
        }

        case WM_TIMER:
        {
            /* is it our credentials poller timer? */
            if (wParam == IDT_LOCKEDDLG_POLL)
            {
                if (credentialsAvailable())
                {
                    if (credentialsRetrieve())
                    {
                        /* fill in credentials to appropriate UI elements */
                        credentialsToUI(NULL /* User ID */, s_hwndPassword, NULL /* Domain */);

                        /* we got the credentials, null them out */
                        credentialsReset();

                        /* confirm the logon dialog, simulating the user pressing "OK" */
                        WPARAM wParam = MAKEWPARAM(IDOK, BN_CLICKED);
                        PostMessage(hwndDlg, WM_COMMAND, wParam, 0);

                        /* we don't need the timer any longer */
                        KillTimer(hwndDlg, IDT_LOCKEDDLG_POLL);
                    }
                }
            }
            break;
        }

        case WM_DESTROY:
            KillTimer(hwndDlg, IDT_LOCKEDDLG_POLL);
            break;
    }
    return bResult;
}


int WINAPI MyWlxDialogBoxParam(HANDLE  hWlx,
                               HANDLE  hInst,
                               LPWSTR  lpszTemplate,
                               HWND    hwndOwner,
                               DLGPROC dlgprc,
                               LPARAM  dwInitParam)
{
    Log(("VBoxGINA::MyWlxDialogBoxParam: lpszTemplate = %ls\n", lpszTemplate));

    //
    // We only know MSGINA dialogs by identifiers.
    //
    if (!HIWORD((int)(void*)lpszTemplate))
    {
        //
        // Hook appropriate dialog boxes as necessary.
        //
        switch ((DWORD) lpszTemplate)
        {
            case IDD_WLXLOGGEDOUTSAS_DIALOG:     /* Windows NT 4.0. */
            case IDD_WLXLOGGEDOUTSAS_DIALOG2:    /* Windows 2000 and up. */
            {
                Log(("VBoxGINA::MyWlxDialogBoxParam: returning hooked LOGGED OUT dialog\n"));
                g_pfnWlxLoggedOutSASDlgProc = dlgprc;
                return g_pfnWlxDialogBoxParam(hWlx, hInst, lpszTemplate, hwndOwner,
                                              MyWlxLoggedOutSASDlgProc, dwInitParam);
            }

            case IDD_WLXWKSTALOCKEDSAS_DIALOG:   /* Windows NT 4.0. */
            case IDD_WLXWKSTALOCKEDSAS_DIALOG2:  /* Windows 2000 and up. */
            {
                Log(("VBoxGINA::MyWlxDialogBoxParam: returning hooked LOCKED dialog\n"));
                g_pfnWlxLockedSASDlgProc = dlgprc;
                return g_pfnWlxDialogBoxParam(hWlx, hInst, lpszTemplate, hwndOwner,
                                              MyWlxLockedSASDlgProc, dwInitParam);
            }

            /** @todo Add other hooking stuff here. */

            default:
            {
                char szBuf[1024];
                sprintf(szBuf, "VBoxGINA::MyWlxDialogBoxParam: dialog %ld not handled\n", (DWORD)lpszTemplate);
                Log((szBuf));
                break;
            }
        }
    }

    //
    // The rest will not be redirected.
    //
    return g_pfnWlxDialogBoxParam(hWlx, hInst, lpszTemplate,
                                  hwndOwner, dlgprc, dwInitParam);
}

