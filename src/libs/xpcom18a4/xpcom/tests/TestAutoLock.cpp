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

/*

  Some tests for nsAutoLock.

 */

#include "nsAutoLock.h"

#include <iprt/assert.h>
#include <iprt/errcore.h>
#include <iprt/initterm.h>
#include <iprt/message.h>
#include <iprt/thread.h>

PRLock* gLock;
int gCount;

static DECLCALLBACK(int) run(RTTHREAD hSelf, void* arg)
{
    RT_NOREF(hSelf, arg);

    for (int i = 0; i < 1000000; ++i) {
        nsAutoLock guard(gLock);
        ++gCount;
        AssertRelease(gCount == 1);
        --gCount;
    }

    return VINF_SUCCESS;
}


int main(int argc, char** argv)
{
    gLock = PR_NewLock();
    gCount = 0;

    int vrc = RTR3InitExe(argc, &argv, 0);
    if (RT_FAILURE(vrc))
        return RTMsgInitFailure(vrc);

    // This shouldn't compile
    //nsAutoLock* l1 = new nsAutoLock(theLock);
    //delete l1;

    // Create a block-scoped lock. This should compile.
    {
        nsAutoLock l2(gLock);
    }

    // Fork a thread to access the shared variable in a tight loop
    RTTHREAD hThread = NIL_RTTHREAD;
    vrc = RTThreadCreate(&hThread, run, NULL, 0 /*cbStack*/,
                         RTTHREADTYPE_IO, RTTHREADFLAGS_WAITABLE, "AutoLockTest");

    // ...and now do the same thing ourselves
    run(NIL_RTTHREAD, nsnull);

    // Wait for the background thread to finish, if necessary.
    int rcThread;
    RTThreadWait(hThread, RT_INDEFINITE_WAIT, &rcThread);
    AssertRC(rcThread);

    return 0;
}
