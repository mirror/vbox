/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Doug Turner <dougt@netscape.com>
 *   IBM Corp.
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

#include "SpecialSystemDirectory.h"
#include "nsString.h"
#include "nsDependentString.h"


#if defined(XP_UNIX)

#include <unistd.h>
#include <stdlib.h>
#include <sys/param.h>
#include "prenv.h"
# if defined(XP_MACOSX) && defined(VBOX_WITH_NEWER_OSX_SDK)
# include <Folders.h>
# endif

#endif

NS_COM void StartupSpecialSystemDirectory()
{
}

NS_COM void ShutdownSpecialSystemDirectory()
{
}


nsresult
GetSpecialSystemDirectory(SystemDirectories aSystemSystemDirectory,
                         nsILocalFile** aFile)
{
    switch (aSystemSystemDirectory)
    {
        case OS_DriveDirectory:
            return NS_NewNativeLocalFile(nsDependentCString("/"), 
                                         PR_TRUE, 
                                         aFile);
        case OS_TemporaryDirectory:
#if defined(XP_MACOSX)
        {
            return GetOSXFolderType(kUserDomain, kTemporaryFolderType, aFile);
        }

#elif defined(XP_UNIX)
        {
            static const char *tPath = nsnull;
            if (!tPath) {
                tPath = PR_GetEnv("TMPDIR");
                if (!tPath || !*tPath) {
                    tPath = PR_GetEnv("TMP");
                    if (!tPath || !*tPath) {
                        tPath = PR_GetEnv("TEMP");
                        if (!tPath || !*tPath) {
                            tPath = "/tmp/";
                        }
                    }
                }
            }
            return NS_NewNativeLocalFile(nsDependentCString(tPath), 
                                         PR_TRUE, 
                                         aFile);
        }
#else
        break;
#endif

#if defined(XP_UNIX)
        case Unix_LocalDirectory:
            return NS_NewNativeLocalFile(nsDependentCString("/usr/local/netscape/"), 
                                         PR_TRUE, 
                                         aFile);
        case Unix_LibDirectory:
            return NS_NewNativeLocalFile(nsDependentCString("/usr/local/lib/netscape/"), 
                                         PR_TRUE, 
                                         aFile);

        case Unix_HomeDirectory:
            return NS_NewNativeLocalFile(nsDependentCString(PR_GetEnv("HOME")), 
                                             PR_TRUE, 
                                             aFile);
#endif
        default:
            break;
    }
    return NS_ERROR_NOT_AVAILABLE;
}

#if defined (XP_MACOSX)
nsresult
GetOSXFolderType(short aDomain, OSType aFolderType, nsILocalFile **localFile)
{
    OSErr err;
    FSRef fsRef;
    nsresult rv = NS_ERROR_FAILURE;

    err = ::FSFindFolder(aDomain, aFolderType, kCreateFolder, &fsRef);
    if (err == noErr)
    {
        NS_NewLocalFile(EmptyString(), PR_TRUE, localFile);
        nsCOMPtr<nsILocalFileMac> localMacFile(do_QueryInterface(*localFile));
        if (localMacFile)
            rv = localMacFile->InitWithFSRef(&fsRef);
    }
    return rv;
}                                                                      
#endif

