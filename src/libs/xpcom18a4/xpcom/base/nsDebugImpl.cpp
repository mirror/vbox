/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   IBM Corp.
 *   Henry Sobotka
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

#include "nsXPCOMPrivate.h"
#include "nsDebugImpl.h"
#include "nsDebug.h"
#include "prprf.h"
#include "prinit.h"
#include "plstr.h"
#include "nsError.h"

#if defined(XP_UNIX) && !defined(UNIX_CRASH_ON_ASSERT)
#include <signal.h>
/* for nsTraceRefcnt::WalkTheStack() */
#include "nsISupportsUtils.h"
#include "nsTraceRefcntImpl.h"
#endif

#include <iprt/cdefs.h>
#include <iprt/env.h>
#include <VBox/log.h>

NS_IMPL_THREADSAFE_ISUPPORTS1(nsDebugImpl, nsIDebug)

nsDebugImpl::nsDebugImpl()
{
}

/**
 * Implementation of the nsDebug methods. Note that this code is
 * always compiled in, in case some other module that uses it is
 * compiled with debugging even if this library is not.
 */

NS_IMETHODIMP
nsDebugImpl::Assertion(const char *aStr, const char *aExpr, const char *aFile, PRInt32 aLine)
{
    RTAssertMsg1(aExpr, aLine, aFile, NULL);
    RTAssertMsg2("%s\n", aStr);
    return NS_OK;
}

NS_IMETHODIMP 
nsDebugImpl::Break(const char *aFile, PRInt32 aLine)
{
#if defined(XP_UNIX) && !defined(UNIX_CRASH_ON_ASSERT)
    fprintf(stderr, "\07");

    const char *assertBehavior = RTEnvGet("XPCOM_DEBUG_BREAK");

    if (!assertBehavior) {

      // the default; nothing else to do
      ;

    } else if ( strcmp(assertBehavior, "suspend")== 0 ) {

      // the suspend case is first because we wanna send the signal before 
      // other threads have had a chance to get too far from the state that
      // caused this assertion (in case they happen to have been involved).
      //
      fprintf(stderr, "Suspending process; attach with the debugger.\n");
      kill(0, SIGSTOP);

    } else if ( strcmp(assertBehavior, "warn")==0 ) {
      
      // same as default; nothing else to do (see "suspend" case comment for
      // why this compare isn't done as part of the default case)
      //
      ;

    } 
    else if ( strcmp(assertBehavior,"stack")==0 ) {

      // walk the stack
      //
      nsTraceRefcntImpl::WalkTheStack(stderr);
    } 
    else if ( strcmp(assertBehavior,"abort")==0 ) {

      // same as UNIX_CRASH_ON_ASSERT
      //
      Abort(aFile, aLine);

    } else if ( strcmp(assertBehavior,"trap")==0 ) {
      RT_BREAKPOINT();
    } else {

      fprintf(stderr, "unrecognized value of XPCOM_DEBUG_BREAK env var!\n");

    }    
    fflush(stderr); // this shouldn't really be necessary, i don't think,
                    // but maybe there's some lame stdio that buffers stderr
#else
  Abort(aFile, aLine);
#endif
  return NS_OK;
}

NS_IMETHODIMP 
nsDebugImpl::Abort(const char *aFile, PRInt32 aLine)
{
    AssertReleaseMsgFailed(("###!!! Abort: at file %s, line %d", aFile, aLine));
    return NS_OK;
}

NS_IMETHODIMP 
nsDebugImpl::Warning(const char* aMessage,
                     const char* aFile, PRIntn aLine)
{
    /* Debug log. */
    Log(("WARNING: %s, file %s, line %d", aMessage, aFile, aLine));

    // And write it out to the stdout
    fprintf(stderr, "WARNING: %s, file %s, line %d", aMessage, aFile, aLine);
    fflush(stderr);
    return NS_OK;
}

NS_METHOD
nsDebugImpl::Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr)
{
  *aInstancePtr = nsnull;
  nsIDebug* debug = new nsDebugImpl();
  if (!debug)
    return NS_ERROR_OUT_OF_MEMORY;
  
  nsresult rv = debug->QueryInterface(aIID, aInstancePtr);
  if (NS_FAILED(rv)) {
    delete debug;
  }
  
  return rv;
}

