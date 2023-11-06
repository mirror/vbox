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

#include "nsMemoryImpl.h"
#include "prmem.h"
#include "nsAlgorithm.h"
#include "nsIServiceManager.h"
#include "nsIObserverService.h"
#include "nsAutoLock.h"
#include "nsIEventQueueService.h"
#include "nsString.h"

nsMemoryImpl* gMemory = nsnull;

NS_IMPL_THREADSAFE_ISUPPORTS1(nsMemoryImpl, nsIMemory)

NS_METHOD
nsMemoryImpl::Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr)
{
    NS_ENSURE_ARG_POINTER(aInstancePtr);
    NS_ENSURE_PROPER_AGGREGATION(outer, aIID);
    if (gMemory && NS_SUCCEEDED(gMemory->QueryInterface(aIID, aInstancePtr)))
        return NS_OK;

    nsMemoryImpl* mm = new nsMemoryImpl();
    if (mm == NULL)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv;

    do {
        rv = mm->QueryInterface(aIID, aInstancePtr);
        if (NS_FAILED(rv))
            break;

        rv = NS_OK;
    } while (0);

    if (NS_FAILED(rv))
        delete mm;

    return rv;
}


nsMemoryImpl::nsMemoryImpl()
{
}

nsMemoryImpl::~nsMemoryImpl()
{
}

////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP_(void *) 
nsMemoryImpl::Alloc(PRSize size)
{
    NS_ASSERTION(size, "nsMemoryImpl::Alloc of 0");
    void* result = PR_Malloc(size);
    return result;
}

NS_IMETHODIMP_(void *)
nsMemoryImpl::Realloc(void * ptr, PRSize size)
{
    void* result = PR_Realloc(ptr, size);
    return result;
}

NS_IMETHODIMP_(void)
nsMemoryImpl::Free(void * ptr)
{
    PR_Free(ptr);
}

NS_IMETHODIMP
nsMemoryImpl::HeapMinimize(PRBool aImmediate)
{
    return NS_OK;
}

NS_IMETHODIMP
nsMemoryImpl::IsLowMemory(PRBool *result)
{
    *result = PR_FALSE;
    return NS_OK;
}

static void
EnsureGlobalMemoryService()
{
    if (gMemory) return;
    nsresult rv = nsMemoryImpl::Create(nsnull, NS_GET_IID(nsIMemory), (void**)&gMemory);
    NS_ASSERTION(NS_SUCCEEDED(rv), "nsMemoryImpl::Create failed");
    NS_ASSERTION(gMemory, "improper xpcom initialization");
}

nsresult 
nsMemoryImpl::Startup()
{
    EnsureGlobalMemoryService();
    if (! gMemory)
        return NS_ERROR_FAILURE;

    return NS_OK;
}

nsresult 
nsMemoryImpl::Shutdown()
{
    if (gMemory) {
        NS_RELEASE(gMemory);
        gMemory = nsnull;
    }

    return NS_OK;
}
