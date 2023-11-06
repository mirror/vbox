/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 et cindent: */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsXPComPrivate_h__
#define nsXPComPrivate_h__

#include "nscore.h"
#include "nsXPCOM.h"

class nsStringContainer;
class nsCStringContainer;

#ifdef VBOX_WITH_XPCOM_NAMESPACE_CLEANUP
#define NS_RegisterXPCOMExitRoutine VBoxNsxpNS_RegisterXPCOMExitRoutine
#define NS_UnregisterXPCOMExitRoutine VBoxNsxpNS_UnregisterXPCOMExitRoutine
#endif /* VBOX_WITH_XPCOM_NAMESPACE_CLEANUP */

/**
 * Private Method to register an exit routine.  This method
 * allows you to setup a callback that will be called from 
 * the NS_ShutdownXPCOM function after all services and 
 * components have gone away.
 *
 * This API is for the exclusive use of the xpcom glue library.
 * 
 * Note that these APIs are NOT threadsafe and must be called on the
 * main thread.
 * 
 * @status FROZEN
 * @param exitRoutine pointer to user defined callback function
 *                    of type XPCOMExitRoutine. 
 * @param priority    higher priorities are called before lower  
 *                    priorities.
 *
 * @return NS_OK for success;
 *         other error codes indicate a failure.
 *
 */
typedef NS_CALLBACK(XPCOMExitRoutine)(void);

extern "C" NS_COM nsresult
NS_RegisterXPCOMExitRoutine(XPCOMExitRoutine exitRoutine, PRUint32 priority);

extern "C" NS_COM nsresult
NS_UnregisterXPCOMExitRoutine(XPCOMExitRoutine exitRoutine);


// think hard before changing this
#define XPCOM_GLUE_VERSION 1


/* XPCOM Specific Defines
 *
 * XPCOM_DLL              - name of the loadable xpcom library on disk. 
 * XPCOM_SEARCH_KEY       - name of the environment variable that can be 
 *                          modified to include additional search paths.
 * GRE_CONF_NAME          - Name of the GRE Configuration file
 */

#ifdef XPCOM_DLL_BASE
#define XPCOM_DLL         XPCOM_DLL_BASE MOZ_DLL_SUFFIX
#endif

#if defined(XP_WIN32) || defined(XP_OS2)

#define XPCOM_SEARCH_KEY  "PATH"
#define GRE_CONF_NAME     "gre.config"
#define GRE_WIN_REG_LOC   "Software\\mozilla.org\\GRE\\"
#ifndef XPCOM_DLL
#define XPCOM_DLL         "xpcom"MOZ_DLL_SUFFIX
#endif

#elif defined(XP_BEOS)

#define XPCOM_SEARCH_KEY  "ADDON_PATH"
#define GRE_CONF_NAME ".gre.config"
#define GRE_CONF_PATH "/boot/home/config/settings/GRE/gre.conf"
#ifndef XPCOM_DLL
#define XPCOM_DLL "libxpcom"MOZ_DLL_SUFFIX
#endif

#else // Unix

#ifndef XPCOM_DLL
#define XPCOM_DLL "libxpcom"MOZ_DLL_SUFFIX
#endif

// you have to love apple..
#ifdef XP_MACOSX  
#define XPCOM_SEARCH_KEY  "DYLD_LIBRARY_PATH"
#else
#define XPCOM_SEARCH_KEY  "LD_LIBRARY_PATH"
#endif

#define GRE_CONF_NAME ".gre.config"
#define GRE_CONF_PATH "/etc/gre.conf"
#define GRE_CONF_DIR  "/etc/gre.d/"
#endif

#if defined(XP_WIN) || defined(XP_OS2)
  #define XPCOM_FILE_PATH_SEPARATOR       "\\"
  #define XPCOM_ENV_PATH_SEPARATOR        ";"
#elif defined(XP_UNIX) || defined(XP_BEOS)
  #define XPCOM_FILE_PATH_SEPARATOR       "/"
  #define XPCOM_ENV_PATH_SEPARATOR        ":"
#else
  #error need_to_define_your_file_path_separator_and_illegal_characters
#endif

#endif


