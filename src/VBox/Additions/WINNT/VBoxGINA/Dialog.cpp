/** @file
 *
 * VBoxGINA -- Windows Logon DLL for VirtualBox Dialog Code
 *
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

#include <windows.h>
#include "Dialog.h"
#include "WinWlx.h"
#include "Helper.h"
#include "VBoxGINA.h"

//
// MSGINA dialog box IDs.
//
#define IDD_WLXDIAPLAYSASNOTICE_DIALOG    1400
#define IDD_WLXLOGGEDOUTSAS_DIALOG        1450
/* the Windows 2000 ID */
#define IDD_WLXLOGGEDOUTSAS_DIALOG2       1500
#define IDD_CHANGE_PASSWORD_DIALOG        1550
#define IDD_WLXLOGGEDONSAS_DIALOG         1650
#define IDD_WLXWKSTALOCKEDSAS_DIALOG      1850

//
// MSGINA control IDs
//
#define IDC_WLXLOGGEDOUTSAS_USERNAME      1453
#define IDC_WLXLOGGEDOUTSAS_USERNAME2     1502
#define IDC_WLXLOGGEDOUTSAS_PASSWORD      1454
#define IDC_WLXLOGGEDOUTSAS_PASSWORD2     1503
#define IDC_WLXLOGGEDOUTSAS_DOMAIN        1455
#define IDC_WLXLOGGEDOUTSAS_DOMAIN2       1504
#define IDC_WLXWKSTALOCKEDSAS_DOMAIN      1856

static DLGPROC pfWlxLoggedOutSASDlgProc   = NULL;

static PWLX_DIALOG_BOX_PARAM pfWlxDialogBoxParam = NULL;

int WINAPI MyWlxDialogBoxParam (HANDLE, HANDLE, LPWSTR, HWND, DLGPROC, LPARAM);

void hookDialogBoxes(PVOID pWinlogonFunctions, DWORD dwWlxVersion)
{
    Log(("VBoxGINA::hookDialogBoxes\n"));

    /* this is version dependent */
    switch (dwWlxVersion)
    {
        case WLX_VERSION_1_0:
        {
            pfWlxDialogBoxParam = ((PWLX_DISPATCH_VERSION_1_0)pWinlogonFunctions)->WlxDialogBoxParam;
            ((PWLX_DISPATCH_VERSION_1_0)pWinlogonFunctions)->WlxDialogBoxParam = MyWlxDialogBoxParam;
            break;
        }

        case WLX_VERSION_1_1:
        {
            pfWlxDialogBoxParam = ((PWLX_DISPATCH_VERSION_1_1)pWinlogonFunctions)->WlxDialogBoxParam;
            ((PWLX_DISPATCH_VERSION_1_1)pWinlogonFunctions)->WlxDialogBoxParam = MyWlxDialogBoxParam;
            break;
        }

        case WLX_VERSION_1_2:
        {
            pfWlxDialogBoxParam = ((PWLX_DISPATCH_VERSION_1_2)pWinlogonFunctions)->WlxDialogBoxParam;
            ((PWLX_DISPATCH_VERSION_1_2)pWinlogonFunctions)->WlxDialogBoxParam = MyWlxDialogBoxParam;
            break;
        }

        case WLX_VERSION_1_3:
        {
            pfWlxDialogBoxParam = ((PWLX_DISPATCH_VERSION_1_3)pWinlogonFunctions)->WlxDialogBoxParam;
            ((PWLX_DISPATCH_VERSION_1_3)pWinlogonFunctions)->WlxDialogBoxParam = MyWlxDialogBoxParam;
            break;
        }

        case WLX_VERSION_1_4:
        {
            pfWlxDialogBoxParam = ((PWLX_DISPATCH_VERSION_1_4)pWinlogonFunctions)->WlxDialogBoxParam;
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

#define CREDPOLL_TIMERID 0x1243

BOOL CALLBACK MyWlxLoggedOutSASDlgProc(HWND   hwndDlg,  // handle to dialog box
                                       UINT   uMsg,     // message
                                       WPARAM wParam,   // first message parameter
                                       LPARAM lParam)   // second message parameter
{
    BOOL bResult;
    static HWND hwndUserId, hwndPassword, hwndDomain = 0;
    static UINT_PTR timer = 0;

    Log(("VBoxGINA::MyWlxLoggedOutSASDlgProc\n"));

    //
    // Pass on to MSGINA first.
    //
    bResult = pfWlxLoggedOutSASDlgProc(hwndDlg, uMsg, wParam, lParam);

    //
    // We are only interested in the WM_INITDIALOG message.
    //
    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            Log(("VBoxGINA::MyWlxLoggedOutSASDlgProc: got WM_INITDIALOG\n"));

            /* get the entry fields */
            hwndUserId = GetDlgItem(hwndDlg, IDC_WLXLOGGEDOUTSAS_USERNAME);
            if (!hwndUserId)
                hwndUserId = GetDlgItem(hwndDlg, IDC_WLXLOGGEDOUTSAS_USERNAME2);
            hwndPassword = GetDlgItem(hwndDlg, IDC_WLXLOGGEDOUTSAS_PASSWORD);
            if (!hwndPassword)
                hwndPassword = GetDlgItem(hwndDlg, IDC_WLXLOGGEDOUTSAS_PASSWORD2);
            hwndDomain = GetDlgItem(hwndDlg, IDC_WLXLOGGEDOUTSAS_DOMAIN);
            if (!hwndDomain)
                hwndDomain = GetDlgItem(hwndDlg, IDC_WLXLOGGEDOUTSAS_DOMAIN2);

            Log(("VBoxGINA::MyWlxLoggedOutSASDlgProc: hwndUserId: %x, hwndPassword: %d, hwndDomain: %d\n",
                 hwndUserId, hwndPassword, hwndDomain));

            /* terminate the credentials poller thread, it's done is job */
            credentialsPollerTerminate();

            if (credentialsAvailable())
            {
                /* query the credentials from VBox */
                if (credentialsRetrieve())
                {
                    if (hwndUserId)
                        SendMessage(hwndUserId, WM_SETTEXT, 0, (LPARAM)g_Username);
                    if (hwndPassword)
                        SendMessage(hwndPassword, WM_SETTEXT, 0, (LPARAM)g_Password);
                    if (hwndDomain)
                        SendMessage(hwndDomain, WM_SETTEXT, 0, (LPARAM)g_Domain);

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
                timer = SetTimer(hwndDlg, CREDPOLL_TIMERID, 200, NULL);
                if (!timer)
                {
                    Log(("VBoxGINA::MyWlxLoggedOutSASDlgProc: failed creating timer! last error: %s\n",
                         GetLastError()));
                }
            }
            break;
        }

        case WM_TIMER:
        {
            /* is it our credentials poller timer? */
            if (wParam == CREDPOLL_TIMERID)
            {
                if (credentialsAvailable())
                {
                    if (credentialsRetrieve())
                    {
                        if (hwndUserId)
                            SendMessage(hwndUserId, WM_SETTEXT, 0, (LPARAM)g_Username);
                        if (hwndPassword)
                            SendMessage(hwndPassword, WM_SETTEXT, 0, (LPARAM)g_Password);
                        if (hwndDomain)
                            SendMessage(hwndDomain, WM_SETTEXT, 0, (LPARAM)g_Domain);

                        /* we got the credentials, null them out */
                        credentialsReset();

                        /* confirm the logon dialog, simulating the user pressing "OK" */
                        WPARAM wParam = MAKEWPARAM(IDOK, BN_CLICKED);
                        PostMessage(hwndDlg, WM_COMMAND, wParam, 0);

                        /* we don't need the timer any longer */
                        /** @todo will we leak the timer when logging in manually? Should we kill it on WM_CLOSE? */
                        KillTimer(hwndDlg, CREDPOLL_TIMERID);
                    }
                }
            }
            break;
        }
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
   Log(("VBoxGINA::MyWlxDialogBoxParam: lpszTemplate = %d\n", lpszTemplate));

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
         case IDD_WLXLOGGEDOUTSAS_DIALOG:
         case IDD_WLXLOGGEDOUTSAS_DIALOG2:
         {
            Log(("VBoxGINA::MyWlxDialogBoxParam: returning hooked logged out dialog\n"));
            pfWlxLoggedOutSASDlgProc = dlgprc;
            return pfWlxDialogBoxParam(hWlx, hInst, lpszTemplate, hwndOwner,
                                       MyWlxLoggedOutSASDlgProc, dwInitParam);
         }

      }
   }

   //
   // The rest will not be redirected.
   //
   return pfWlxDialogBoxParam(hWlx, hInst, lpszTemplate,
                              hwndOwner, dlgprc, dwInitParam);
}
