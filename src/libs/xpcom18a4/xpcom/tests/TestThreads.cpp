/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

#include "nsIThread.h"
#include "nsIRunnable.h"
#include <stdio.h>
#include <stdlib.h>
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"

class nsRunner : public nsIRunnable {
public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD Run() {
        nsCOMPtr<nsIThread> thread;
        nsresult rv = nsIThread::GetCurrent(getter_AddRefs(thread));
        if (NS_FAILED(rv)) {
            printf("failed to get current thread\n");
            return rv;
        }
        printf("running %d on thread %p\n", mNum, (void *)thread.get());

        // if we don't do something slow, we'll never see the other
        // worker threads run
        PR_Sleep(PR_MillisecondsToInterval(100));

        return rv;
    }

    nsRunner(int num) : mNum(num) {
    }

protected:
    int mNum;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(nsRunner, nsIRunnable)

nsresult
TestThreads()
{
    nsresult rv;

    nsCOMPtr<nsIThread> runner;
    rv = NS_NewThread(getter_AddRefs(runner), new nsRunner(0), 0, PR_JOINABLE_THREAD);
    if (NS_FAILED(rv)) {
        printf("failed to create thread\n");
        return rv;
    }

    nsCOMPtr<nsIThread> thread;
    rv = nsIThread::GetCurrent(getter_AddRefs(thread));
    if (NS_FAILED(rv)) {
        printf("failed to get current thread\n");
        return rv;
    }

    PRThreadScope scope;
    rv = runner->GetScope(&scope);
    if (NS_FAILED(rv)) {
        printf("runner already exited\n");        
    }

    rv = runner->Join();     // wait for the runner to die before quitting
    if (NS_FAILED(rv)) {
        printf("join failed\n");        
    }

    rv = runner->GetScope(&scope);      // this should fail after Join
    if (NS_SUCCEEDED(rv)) {
        printf("get scope failed\n");        
    }

    rv = runner->Interrupt();   // this should fail after Join
    if (NS_SUCCEEDED(rv)) {
        printf("interrupt failed\n");        
    }

    ////////////////////////////////////////////////////////////////////////////
    // try an unjoinable thread 
    rv = NS_NewThread(getter_AddRefs(runner), new nsRunner(1));
    if (NS_FAILED(rv)) {
        printf("failed to create thread\n");
        return rv;
    }

    rv = runner->Join();     // wait for the runner to die before quitting
    if (NS_SUCCEEDED(rv)) {
        printf("shouldn't have been able to join an unjoinable thread\n");        
    }

    PR_Sleep(PR_MillisecondsToInterval(100));       // hopefully the runner will quit here

    return NS_OK;
}

int
main(int argc, char** argv)
{
    int retval = 0;
    nsresult rv;
    
    rv = NS_InitXPCOM2(nsnull, nsnull, nsnull);
    if (NS_FAILED(rv)) return -1;

    rv = TestThreads();
    if (NS_FAILED(rv)) return -1;

    rv = NS_ShutdownXPCOM(nsnull);
    if (NS_FAILED(rv)) return -1;
    return retval;
}
