/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla IPC.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef IPCD_H__
#define IPCD_H__

#include <iprt/sg.h>

#include "ipcClient.h"
#include "ipcMessageNew.h"

//-----------------------------------------------------------------------------
// IPC daemon methods (see struct ipcDaemonMethods)
//
// these functions may only be called directly from within the daemon.  plug-in
// modules must access these through the ipcDaemonMethods structure.
//-----------------------------------------------------------------------------

DECLHIDDEN(int)         IPC_SendMsg(PIPCDCLIENT pIpcClient, const nsID &target, const void *pvData, size_t cbData);
DECLHIDDEN(int)         IPC_SendMsgSg(PIPCDCLIENT pIpcClient, const nsID &target, size_t cbTotal, PCRTSGSEG paSegs, uint32_t cSegs);
DECLHIDDEN(PIPCDCLIENT) IPC_GetClientByID(PIPCDSTATE pIpcd, uint32_t idClient);
DECLHIDDEN(PIPCDCLIENT) IPC_GetClientByName(PIPCDSTATE pIpcd, const char *pszName);

DECLHIDDEN(void)        IPC_PutMsgIntoCache(PIPCDSTATE pIpcd, PIPCMSG pMsg);

//
// dispatch message
//
DECLHIDDEN(int) IPC_DispatchMsg(PIPCDCLIENT pIpcClient, PCIPCMSG pMsg);

//
// send message, takes ownership of |msg|.
//
DECLHIDDEN(int) IPC_SendMsg(PIPCDCLIENT pIpcClient, PIPCMSG pMsg);

//
// dispatch notifications about client connects and disconnects
//
DECLHIDDEN(void) IPC_NotifyClientUp(PIPCDCLIENT pIpcClient);
DECLHIDDEN(void) IPC_NotifyClientDown(PIPCDCLIENT pIpcClient);

#endif // !IPCD_H__
