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
 * The Original Code is the Python XPCOM language bindings.
 *
 * The Initial Developer of the Original Code is
 * ActiveState Tool Corp.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mark Hammond <mhammond@skippinet.com.au> (original author)
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

//
// This code is part of the XPCOM extensions for Python.
//
// Written May 2000 by Mark Hammond.
//
// Based heavily on the Python COM support, which is
// (c) Mark Hammond and Greg Stein.
//
// (c) 2000, ActiveState corp.

#include "PyXPCOM.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsIFile.h"
#include "nsIComponentRegistrar.h"
#include "nsIComponentManagerObsolete.h"
#include "nsIConsoleService.h"
#include "nspr.h" // PR_fprintf
#ifdef VBOX
#include "nsEventQueueUtils.h"
#endif

#ifdef XP_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "windows.h"
#endif

#include "nsIEventQueue.h"
#include "nsIProxyObjectManager.h"

#define LOADER_LINKS_WITH_PYTHON

#ifndef PYXPCOM_USE_PYGILSTATE
extern PYXPCOM_EXPORT void PyXPCOM_InterpreterState_Ensure();
#endif

#ifdef VBOX_PYXPCOM
# ifdef VBOX_PYXPCOM_VERSIONED
#  if   PY_VERSION_HEX >= 0x02080000
#   define MODULE_NAME "VBoxPython2_8"
#  elif PY_VERSION_HEX >= 0x02070000
#   define MODULE_NAME "VBoxPython2_7"
#  elif PY_VERSION_HEX >= 0x02060000
#   define MODULE_NAME "VBoxPython2_6"
#  elif PY_VERSION_HEX >= 0x02050000
#   define MODULE_NAME "VBoxPython2_5"
#  elif PY_VERSION_HEX >= 0x02040000
#   define MODULE_NAME "VBoxPython2_4"
#  elif PY_VERSION_HEX >= 0x02030000
#   define MODULE_NAME "VBoxPython2_3"
#  else
#   error "Fix module versioning."
#  endif
# else
#  define MODULE_NAME "VBoxPython"
# endif
#else
#define MODULE_NAME "_xpcom"
#endif

// "boot-strap" methods - interfaces we need to get the base
// interface support!

/* deprecated, included for backward compatibility */
static PyObject *
PyXPCOMMethod_NS_GetGlobalComponentManager(PyObject *self, PyObject *args)
{
	if (PyErr_Warn(PyExc_DeprecationWarning, "Use GetComponentManager instead") < 0)
	       return NULL;
	if (!PyArg_ParseTuple(args, ""))
		return NULL;
	nsCOMPtr<nsIComponentManager> cm;
	nsresult rv;
	Py_BEGIN_ALLOW_THREADS;
	rv = NS_GetComponentManager(getter_AddRefs(cm));
	Py_END_ALLOW_THREADS;
	if ( NS_FAILED(rv) )
		return PyXPCOM_BuildPyException(rv);

	nsCOMPtr<nsIComponentManagerObsolete> ocm(do_QueryInterface(cm, &rv));
	if ( NS_FAILED(rv) )
		return PyXPCOM_BuildPyException(rv);

	return Py_nsISupports::PyObjectFromInterface(ocm, NS_GET_IID(nsIComponentManagerObsolete), PR_FALSE);
}

static PyObject *
PyXPCOMMethod_GetComponentManager(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ""))
		return NULL;
	nsCOMPtr<nsIComponentManager> cm;
	nsresult rv;
	Py_BEGIN_ALLOW_THREADS;
	rv = NS_GetComponentManager(getter_AddRefs(cm));
	Py_END_ALLOW_THREADS;
	if ( NS_FAILED(rv) )
		return PyXPCOM_BuildPyException(rv);

	return Py_nsISupports::PyObjectFromInterface(cm, NS_GET_IID(nsIComponentManager), PR_FALSE);
}

// No xpcom callable way to get at the registrar, even though the interface
// is scriptable.
static PyObject *
PyXPCOMMethod_GetComponentRegistrar(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ""))
		return NULL;
	nsCOMPtr<nsIComponentRegistrar> cm;
	nsresult rv;
	Py_BEGIN_ALLOW_THREADS;
	rv = NS_GetComponentRegistrar(getter_AddRefs(cm));
	Py_END_ALLOW_THREADS;
	if ( NS_FAILED(rv) )
		return PyXPCOM_BuildPyException(rv);

	return Py_nsISupports::PyObjectFromInterface(cm, NS_GET_IID(nsISupports), PR_FALSE);
}

static PyObject *
PyXPCOMMethod_GetServiceManager(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ""))
		return NULL;
	nsCOMPtr<nsIServiceManager> sm;
	nsresult rv;
	Py_BEGIN_ALLOW_THREADS;
	rv = NS_GetServiceManager(getter_AddRefs(sm));
	Py_END_ALLOW_THREADS;
	if ( NS_FAILED(rv) )
		return PyXPCOM_BuildPyException(rv);

	// Return a type based on the IID.
	return Py_nsISupports::PyObjectFromInterface(sm, NS_GET_IID(nsIServiceManager));
}

/* deprecated, included for backward compatibility */
static PyObject *
PyXPCOMMethod_GetGlobalServiceManager(PyObject *self, PyObject *args)
{
	if (PyErr_Warn(PyExc_DeprecationWarning, "Use GetServiceManager instead") < 0)
		return NULL;

	return PyXPCOMMethod_GetComponentManager(self, args);
}

static PyObject *
PyXPCOMMethod_XPTI_GetInterfaceInfoManager(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ""))
		return NULL;
	nsIInterfaceInfoManager* im;
	Py_BEGIN_ALLOW_THREADS;
	im = XPTI_GetInterfaceInfoManager();
	Py_END_ALLOW_THREADS;
	if ( im == nsnull )
		return PyXPCOM_BuildPyException(NS_ERROR_FAILURE);

	/* Return a type based on the IID (with no extra ref) */
	// Can not auto-wrap the interface info manager as it is critical to
	// building the support we need for autowrap.
	PyObject *ret = Py_nsISupports::PyObjectFromInterface(im, NS_GET_IID(nsIInterfaceInfoManager), PR_FALSE);
	NS_IF_RELEASE(im);
	return ret;
}

static PyObject *
PyXPCOMMethod_XPTC_InvokeByIndex(PyObject *self, PyObject *args)
{
	PyObject *obIS, *obParams;
	nsCOMPtr<nsISupports> pis;
	int index;

	// We no longer rely on PyErr_Occurred() for our error state,
	// but keeping this assertion can't hurt - it should still always be true!
	NS_WARN_IF_FALSE(!PyErr_Occurred(), "Should be no pending Python error!");

	if (!PyArg_ParseTuple(args, "OiO", &obIS, &index, &obParams))
		return NULL;

	if (!Py_nsISupports::Check(obIS)) {
		return PyErr_Format(PyExc_TypeError,
		                    "First param must be a native nsISupports wrapper (got %s)",
		                    obIS->ob_type->tp_name);
	}
	// Ack!  We must ask for the "native" interface supported by
	// the object, not specifically nsISupports, else we may not
	// back the same pointer (eg, Python, following identity rules,
	// will return the "original" gateway when QI'd for nsISupports)
	if (!Py_nsISupports::InterfaceFromPyObject(
			obIS,
			Py_nsIID_NULL,
			getter_AddRefs(pis),
			PR_FALSE))
		return NULL;

	PyXPCOM_InterfaceVariantHelper arg_helper((Py_nsISupports *)obIS, index);
	if (!arg_helper.Init(obParams))
		return NULL;

	if (!arg_helper.FillArray())
		return NULL;

	nsresult r;
	Py_BEGIN_ALLOW_THREADS;
	r = XPTC_InvokeByIndex(pis, index, arg_helper.m_num_array, arg_helper.m_var_array);
	Py_END_ALLOW_THREADS;
	if ( NS_FAILED(r) )
		return PyXPCOM_BuildPyException(r);

	return arg_helper.MakePythonResult();
}

static PyObject *
PyXPCOMMethod_WrapObject(PyObject *self, PyObject *args)
{
	PyObject *ob, *obIID;
	int bWrapClient = 1;
	if (!PyArg_ParseTuple(args, "OO|i", &ob, &obIID, &bWrapClient))
		return NULL;

	nsIID	iid;
	if (!Py_nsIID::IIDFromPyObject(obIID, &iid))
		return NULL;

	nsCOMPtr<nsISupports> ret;
	nsresult r = PyXPCOM_XPTStub::CreateNew(ob, iid, getter_AddRefs(ret));
	if ( NS_FAILED(r) )
		return PyXPCOM_BuildPyException(r);

	// _ALL_ wrapped objects are associated with a weak-ref
	// to their "main" instance.
	AddDefaultGateway(ob, ret); // inject a weak reference to myself into the instance.

	// Now wrap it in an interface.
	return Py_nsISupports::PyObjectFromInterface(ret, iid, bWrapClient);
}

static PyObject *
PyXPCOMMethod_UnwrapObject(PyObject *self, PyObject *args)
{
	PyObject *ob;
	if (!PyArg_ParseTuple(args, "O", &ob))
		return NULL;

	nsISupports *uob = NULL;
	nsIInternalPython *iob = NULL;
	PyObject *ret = NULL;
	if (!Py_nsISupports::InterfaceFromPyObject(ob,
				NS_GET_IID(nsISupports),
				&uob,
				PR_FALSE))
		goto done;
	if (NS_FAILED(uob->QueryInterface(NS_GET_IID(nsIInternalPython), reinterpret_cast<void **>(&iob)))) {
		PyErr_SetString(PyExc_ValueError, "This XPCOM object is not implemented by Python");
		goto done;
	}
	ret = iob->UnwrapPythonObject();
done:
	Py_BEGIN_ALLOW_THREADS;
	NS_IF_RELEASE(uob);
	NS_IF_RELEASE(iob);
	Py_END_ALLOW_THREADS;
	return ret;
}

// @pymethod int|pythoncom|_GetInterfaceCount|Retrieves the number of interface objects currently in existance
static PyObject *
PyXPCOMMethod_GetInterfaceCount(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":_GetInterfaceCount"))
		return NULL;
	return PyInt_FromLong(_PyXPCOM_GetInterfaceCount());
	// @comm If is occasionally a good idea to call this function before your Python program
	// terminates.  If this function returns non-zero, then you still have PythonCOM objects
	// alive in your program (possibly in global variables).
}

// @pymethod int|pythoncom|_GetGatewayCount|Retrieves the number of gateway objects currently in existance
static PyObject *
PyXPCOMMethod_GetGatewayCount(PyObject *self, PyObject *args)
{
	// @comm This is the number of Python object that implement COM servers which
	// are still alive (ie, serving a client).  The only way to reduce this count
	// is to have the process which uses these PythonCOM servers release its references.
	if (!PyArg_ParseTuple(args, ":_GetGatewayCount"))
		return NULL;
	return PyInt_FromLong(_PyXPCOM_GetGatewayCount());
}

static PyObject *
PyXPCOMMethod_NS_ShutdownXPCOM(PyObject *self, PyObject *args)
{
	// @comm This is the number of Python object that implement COM servers which
	// are still alive (ie, serving a client).  The only way to reduce this count
	// is to have the process which uses these PythonCOM servers release its references.
	if (!PyArg_ParseTuple(args, ":NS_ShutdownXPCOM"))
		return NULL;
	nsresult nr;
	Py_BEGIN_ALLOW_THREADS;
	nr = NS_ShutdownXPCOM(nsnull);
	Py_END_ALLOW_THREADS;

	// Dont raise an exception - as we are probably shutting down
	// and dont really case - just return the status
	return PyInt_FromLong(nr);
}

static NS_DEFINE_CID(kProxyObjectManagerCID, NS_PROXYEVENT_MANAGER_CID);

// A hack to work around their magic constants!
static PyObject *
PyXPCOMMethod_GetProxyForObject(PyObject *self, PyObject *args)
{
	PyObject *obQueue, *obIID, *obOb;
	int flags;
	if (!PyArg_ParseTuple(args, "OOOi", &obQueue, &obIID, &obOb, &flags))
		return NULL;
	nsIID iid;
	if (!Py_nsIID::IIDFromPyObject(obIID, &iid))
		return NULL;
	nsCOMPtr<nsISupports> pob;
	if (!Py_nsISupports::InterfaceFromPyObject(obOb, iid, getter_AddRefs(pob), PR_FALSE))
		return NULL;
	nsIEventQueue *pQueue = NULL;
	nsIEventQueue *pQueueRelease = NULL;

	if (PyInt_Check(obQueue)) {
		pQueue = (nsIEventQueue *)PyInt_AsLong(obQueue);
	} else {
		if (!Py_nsISupports::InterfaceFromPyObject(obQueue, NS_GET_IID(nsIEventQueue), (nsISupports **)&pQueue, PR_TRUE))
			return NULL;
		pQueueRelease = pQueue;
	}

	nsresult rv_proxy;
	nsCOMPtr<nsISupports> presult;
	Py_BEGIN_ALLOW_THREADS;
	nsCOMPtr<nsIProxyObjectManager> proxyMgr =
	         do_GetService(kProxyObjectManagerCID, &rv_proxy);

	if ( NS_SUCCEEDED(rv_proxy) ) {
		rv_proxy = proxyMgr->GetProxyForObject(pQueue,
				iid,
				pob,
				flags,
				getter_AddRefs(presult));
	}
	if (pQueueRelease)
		pQueueRelease->Release();
	Py_END_ALLOW_THREADS;

	PyObject *result;
	if (NS_SUCCEEDED(rv_proxy) ) {
		result = Py_nsISupports::PyObjectFromInterface(presult, iid);
	} else {
		result = PyXPCOM_BuildPyException(rv_proxy);
	}
	return result;
}

static PyObject *
PyXPCOMMethod_MakeVariant(PyObject *self, PyObject *args)
{
	PyObject *ob;
	if (!PyArg_ParseTuple(args, "O:MakeVariant", &ob))
		return NULL;
	nsCOMPtr<nsIVariant> pVar;
	nsresult nr = PyObject_AsVariant(ob, getter_AddRefs(pVar));
	if (NS_FAILED(nr))
		return PyXPCOM_BuildPyException(nr);
	if (pVar == nsnull) {
		NS_ERROR("PyObject_AsVariant worked but returned a NULL ptr!");
		return PyXPCOM_BuildPyException(NS_ERROR_UNEXPECTED);
	}
	return Py_nsISupports::PyObjectFromInterface(pVar, NS_GET_IID(nsIVariant));
}

static PyObject *
PyXPCOMMethod_GetVariantValue(PyObject *self, PyObject *args)
{
	PyObject *ob, *obParent = NULL;
	if (!PyArg_ParseTuple(args, "O|O:GetVariantValue", &ob, &obParent))
		return NULL;

	nsCOMPtr<nsIVariant> var;
	if (!Py_nsISupports::InterfaceFromPyObject(ob,
				NS_GET_IID(nsISupports),
				getter_AddRefs(var),
				PR_FALSE))
		return PyErr_Format(PyExc_ValueError,
				    "Object is not an nsIVariant (got %s)",
				    ob->ob_type->tp_name);

	Py_nsISupports *parent = nsnull;
	if (obParent && obParent != Py_None) {
		if (!Py_nsISupports::Check(obParent)) {
			PyErr_SetString(PyExc_ValueError,
					"Object not an nsISupports wrapper");
			return NULL;
		}
		parent = (Py_nsISupports *)obParent;
	}
	return PyObject_FromVariant(parent, var);
}

PyObject *PyGetSpecialDirectory(PyObject *self, PyObject *args)
{
	char *dirname;
	if (!PyArg_ParseTuple(args, "s:GetSpecialDirectory", &dirname))
		return NULL;
	nsCOMPtr<nsIFile> file;
	nsresult r = NS_GetSpecialDirectory(dirname, getter_AddRefs(file));
	if ( NS_FAILED(r) )
		return PyXPCOM_BuildPyException(r);
	// returned object swallows our reference.
	return Py_nsISupports::PyObjectFromInterface(file, NS_GET_IID(nsIFile));
}

PyObject *AllocateBuffer(PyObject *self, PyObject *args)
{
	int bufSize;
	if (!PyArg_ParseTuple(args, "i", &bufSize))
		return NULL;
	return PyBuffer_New(bufSize);
}

// Writes a message to the console service.  This could be done via pure
// Python code, but is useful when the logging code is actually the
// xpcom .py framework itself (ie, we don't want our logging framework to
// call back into the very code generating the log messages!
PyObject *LogConsoleMessage(PyObject *self, PyObject *args)
{
	char *msg;
	if (!PyArg_ParseTuple(args, "s", &msg))
		return NULL;

	nsCOMPtr<nsIConsoleService> consoleService = do_GetService(NS_CONSOLESERVICE_CONTRACTID);
	if (consoleService)
		consoleService->LogStringMessage(NS_ConvertASCIItoUCS2(msg).get());
	else {
	// This either means no such service, or in shutdown - hardly worth
	// the warning, and not worth reporting an error to Python about - its
	// log handler would just need to catch and ignore it.
	// And as this is only called by this logging setup, any messages should
	// still go to stderr or a logfile.
		NS_WARNING("pyxpcom can't log console message.");
	}

	Py_INCREF(Py_None);
	return Py_None;
}

#ifdef VBOX
//#define USE_EVENTQUEUE  1

# ifdef USE_EVENTQUEUE
#  include <VBox/com/EventQueue.h>
#  include <iprt/err.h>
# else
#  include <iprt/cdefs.h>
# endif

static nsIEventQueue* g_mainEventQ = nsnull;

# ifndef USE_EVENTQUEUE /** @todo Make USE_EVENTQUEUE default. */
// Wrapper that checks if the queue has pending events.
DECLINLINE(bool)
hasEventQueuePendingEvents(nsIEventQueue *pQueue)
{
	PRBool fHasEvents = PR_FALSE;
	nsresult rc = pQueue->PendingEvents(&fHasEvents);
	return NS_SUCCEEDED(rc) && fHasEvents ? true : false;
}

// Wrapper that checks if the queue is native or not.
DECLINLINE(bool)
isEventQueueNative(nsIEventQueue *pQueue)
{
	PRBool fIsNative = PR_FALSE;
	nsresult rc = pQueue->IsQueueNative(&fIsNative);
	return NS_SUCCEEDED(rc) && fIsNative ? true : false;
}

# ifdef RT_OS_DARWIN
#  include <iprt/time.h>
#  include <iprt/thread.h>
#  include <iprt/err.h>
#  include <CoreFoundation/CFRunLoop.h>
#  if MAC_OS_X_VERSION_MAX_ALLOWED == 1040 /* ASSUMES this means we're using the 10.4 SDK. */
#   include <CarbonEvents.h>
#  endif

// Common fallback for waiting (and maybe processing) events. Caller process
// any pending events when 0 is returned.
static nsresult
waitForEventsCommonFallback(nsIEventQueue *pQueue, PRInt32 cMsTimeout)
{
	if (cMsTimeout < 0) {
		// WaitForEvent probably does the trick here.
		PLEvent *pEvent = NULL;
		nsresult rc = -1;
		Py_BEGIN_ALLOW_THREADS;
		rc = pQueue->WaitForEvent(&pEvent);
		Py_END_ALLOW_THREADS;
		if (NS_SUCCEEDED(rc)) {
			pQueue->HandleEvent(pEvent);
			return 0;
		}
	} else {
		// Poll until time out or event is encountered. We have
		// no other choice here.
		uint64_t const StartMillTS = RTTimeMilliTS();
		for (;;)
		{
			// Any pending events?
			if (hasEventQueuePendingEvents(pQueue))
				return 0;

    			// Work any native per-thread event loops for good measure.
#  ifdef RT_OS_DARWIN
			CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.0, false /*returnAfterSourceHandled*/);
#  endif

			// Any pending events?
			if (hasEventQueuePendingEvents(pQueue))
				return 0;

			// Timed out?
			uint64_t cMsElapsed = RTTimeMilliTS() - StartMillTS;
			if (cMsElapsed > (uint32_t)cMsTimeout)
				break;

			// Sleep a bit; return 0 if interrupt (see the select code edition).
			uint32_t cMsLeft = (uint32_t)cMsTimeout - (uint32_t)cMsElapsed;
			int rc = RTThreadSleep(RT_MIN(cMsLeft, 250));
			if (rc == VERR_INTERRUPTED)
				return 0;
		}
	}

	return 1;
}


// Takes care of the waiting on darwin. Caller process any pending events when
// 0 is returned.
static nsresult
waitForEventsOnDarwin(nsIEventQueue *pQueue, PRInt32 cMsTimeout)
{
	// This deals with the common case where the caller is the main
	// application thread and the queue is a native one.
    	if (    isEventQueueNative(pQueue)
#  if MAC_OS_X_VERSION_MAX_ALLOWED == 1040 /* ASSUMES this means we're using the 10.4 SDK. */
	    &&  GetCurrentEventLoop() == GetMainEventLoop()
#  else
	    &&  CFRunLoopGetMain() == CFRunLoopGetCurrent()
#  endif
	) {
    		OSStatus       orc       = -1;
		CFTimeInterval rdTimeout = cMsTimeout < 0
		                         ? 1.0e10
		                         : (double)cMsTimeout / 1000;
		Py_BEGIN_ALLOW_THREADS;
		orc = CFRunLoopRunInMode(kCFRunLoopDefaultMode, rdTimeout, true /*returnAfterSourceHandled*/);
		if (orc == kCFRunLoopRunHandledSource)
		    orc = CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.0, false /*returnAfterSourceHandled*/);
		Py_END_ALLOW_THREADS;
		if (!orc || orc == kCFRunLoopRunHandledSource)
			return 0;

		if (orc != kCFRunLoopRunTimedOut) {
			NS_WARNING("Unexpected status code from CFRunLoopRunInMode");
			RTThreadSleep(1); // throttle
		}

		return hasEventQueuePendingEvents(pQueue) ? 0 : 1;
	}

	// All native queus are driven by the main application loop... Use the
	// fallback for now.
	return waitForEventsCommonFallback(pQueue, cMsTimeout);
}

# endif /* RT_OS_DARWIN */
# endif /* !USE_EVENTQUEUE */

static PyObject*
PyXPCOMMethod_WaitForEvents(PyObject *self, PyObject *args)
{
  PRInt32 aTimeout;

  if (!PyArg_ParseTuple(args, "i", &aTimeout))
    return NULL; /** @todo throw exception */

  nsIEventQueue* q = g_mainEventQ;
  if (q == nsnull)
    return NULL; /** @todo throw exception */

# ifdef USE_EVENTQUEUE
  int rc;
  
  Py_BEGIN_ALLOW_THREADS;
  rc = com::EventQueue::processThreadEventQueue(aTimeout < 0 ? RT_INDEFINITE_WAIT : (uint32_t)aTimeout);
  Py_END_ALLOW_THREADS;
  
  if (RT_SUCCESS(rc))
      return PyInt_FromLong(0);
  if (   rc == VERR_TIMEOUT
      || rc == VERR_INTERRUPTED)
      return PyInt_FromLong(1);
  return NULL; /** @todo throw exception */

# else  /* !USE_EVENTQUEUE */
  NS_WARN_IF_FALSE(isEventQueueNative(q), "The event queue must be native!");

  PRInt32 result = 0;
#  ifdef RT_OS_DARWIN
  if (aTimeout != 0 && !hasEventQueuePendingEvents(q))
    result = waitForEventsOnDarwin(q, aTimeout);
  if (result == 0)
    q->ProcessPendingEvents();

#  else  /* !RT_OS_DARWIN */

  PRBool hasEvents = PR_FALSE;
  nsresult rc;
  PRInt32 fd;

  if (aTimeout == 0)
    goto ok;

  rc = q->PendingEvents(&hasEvents);
  if (NS_FAILED (rc))
    return NULL;

  if (hasEvents)
    goto ok;

  fd = q->GetEventQueueSelectFD();
  if (fd < 0 && aTimeout < 0)
  {
    /* fallback */
    PLEvent *pEvent = NULL;
    Py_BEGIN_ALLOW_THREADS
    rc = q->WaitForEvent(&pEvent);
    Py_END_ALLOW_THREADS
    if (NS_SUCCEEDED(rc))
      q->HandleEvent(pEvent);
    goto ok;
  }

  /* Cannot perform timed wait otherwise */
  if (fd < 0)
      return NULL;

  {
    fd_set fdsetR, fdsetE;
    struct timeval tv;

    FD_ZERO(&fdsetR);
    FD_SET(fd, &fdsetR);

    fdsetE = fdsetR;
    if (aTimeout > 0)
      {
        tv.tv_sec = (PRInt64)aTimeout / 1000;
        tv.tv_usec = ((PRInt64)aTimeout % 1000) * 1000;
      }

    /** @todo: What to do for XPCOM platforms w/o select() ? */
    Py_BEGIN_ALLOW_THREADS;
    int n = select(fd + 1, &fdsetR, NULL, &fdsetE, aTimeout < 0 ? NULL : &tv);
    result = (n == 0) ?  1 :  0;
    Py_END_ALLOW_THREADS;
  }
 ok:
  q->ProcessPendingEvents();
#  endif /* !RT_OS_DARWIN */
  return PyInt_FromLong(result);
# endif /* !USE_EVENTQUEUE */
}

PR_STATIC_CALLBACK(void *) PyHandleEvent(PLEvent *ev)
{
  return nsnull;
}

PR_STATIC_CALLBACK(void) PyDestroyEvent(PLEvent *ev)
{
  delete ev;
}

static PyObject*
PyXPCOMMethod_InterruptWait(PyObject *self, PyObject *args)
{
  nsIEventQueue* q = g_mainEventQ;
  PRInt32 result = 0;
  nsresult rc;

  PLEvent *ev = new PLEvent();
  if (!ev)
  {
    result = 1;
    goto done;
  }
  q->InitEvent (ev, NULL, PyHandleEvent, PyDestroyEvent);
  rc = q->PostEvent (ev);
  if (NS_FAILED(rc))
  {
    result = 2;
    goto done;
  }

 done:
  return PyInt_FromLong(result);
}

static void deinitVBoxPython();

static PyObject*
PyXPCOMMethod_DeinitCOM(PyObject *self, PyObject *args)
{
    Py_BEGIN_ALLOW_THREADS;
    deinitVBoxPython();
    Py_END_ALLOW_THREADS;
    return PyInt_FromLong(0);
}

static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

static PyObject*
PyXPCOMMethod_AttachThread(PyObject *self, PyObject *args)
{
    nsresult rv;
    PRInt32  result = 0;
    nsCOMPtr<nsIEventQueueService> eqs;

    // Create the Event Queue for this thread...
    Py_BEGIN_ALLOW_THREADS;
    eqs =
      do_GetService(kEventQueueServiceCID, &rv);
    Py_END_ALLOW_THREADS;
    if (NS_FAILED(rv))
    {
      result = 1;
      goto done;
    }

    Py_BEGIN_ALLOW_THREADS;
    rv = eqs->CreateThreadEventQueue();
    Py_END_ALLOW_THREADS;
    if (NS_FAILED(rv))
    {
      result = 2;
      goto done;
    }

 done:
    /** @todo: better throw an exception on error */
    return PyInt_FromLong(result);
}

static PyObject*
PyXPCOMMethod_DetachThread(PyObject *self, PyObject *args)
{
    nsresult rv;
    PRInt32  result = 0;
    nsCOMPtr<nsIEventQueueService> eqs;

    // Destroy the Event Queue for this thread...
    Py_BEGIN_ALLOW_THREADS;
    eqs =
      do_GetService(kEventQueueServiceCID, &rv);
    Py_END_ALLOW_THREADS;
    if (NS_FAILED(rv))
    {
      result = 1;
      goto done;
    }

    Py_BEGIN_ALLOW_THREADS;
    rv = eqs->DestroyThreadEventQueue();
    Py_END_ALLOW_THREADS;
    if (NS_FAILED(rv))
    {
      result = 2;
      goto done;
    }

 done:
    /** @todo: better throw an exception on error */
    return PyInt_FromLong(result);
}

#endif /* VBOX */

extern PYXPCOM_EXPORT PyObject *PyXPCOMMethod_IID(PyObject *self, PyObject *args);

static struct PyMethodDef xpcom_methods[]=
{
	{"GetComponentManager", PyXPCOMMethod_GetComponentManager, 1},
	{"GetComponentRegistrar", PyXPCOMMethod_GetComponentRegistrar, 1},
	{"NS_GetGlobalComponentManager", PyXPCOMMethod_NS_GetGlobalComponentManager, 1}, // deprecated
	{"XPTI_GetInterfaceInfoManager", PyXPCOMMethod_XPTI_GetInterfaceInfoManager, 1},
	{"XPTC_InvokeByIndex", PyXPCOMMethod_XPTC_InvokeByIndex, 1},
	{"GetServiceManager", PyXPCOMMethod_GetServiceManager, 1},
	{"GetGlobalServiceManager", PyXPCOMMethod_GetGlobalServiceManager, 1}, // deprecated
	{"IID", PyXPCOMMethod_IID, 1}, // IID is wrong - deprecated - not just IID, but CID, etc.
	{"ID", PyXPCOMMethod_IID, 1}, // This is the official name.
	{"NS_ShutdownXPCOM", PyXPCOMMethod_NS_ShutdownXPCOM, 1},
	{"WrapObject", PyXPCOMMethod_WrapObject, 1},
	{"UnwrapObject", PyXPCOMMethod_UnwrapObject, 1},
	{"_GetInterfaceCount", PyXPCOMMethod_GetInterfaceCount, 1},
	{"_GetGatewayCount", PyXPCOMMethod_GetGatewayCount, 1},
	{"getProxyForObject", PyXPCOMMethod_GetProxyForObject, 1},
	{"GetProxyForObject", PyXPCOMMethod_GetProxyForObject, 1},
	{"GetSpecialDirectory", PyGetSpecialDirectory, 1},
	{"AllocateBuffer", AllocateBuffer, 1},
	{"LogConsoleMessage", LogConsoleMessage, 1, "Write a message to the xpcom console service"},
	{"MakeVariant", PyXPCOMMethod_MakeVariant, 1},
	{"GetVariantValue", PyXPCOMMethod_GetVariantValue, 1},
#ifdef VBOX
        {"WaitForEvents", PyXPCOMMethod_WaitForEvents, 1},
        {"InterruptWait", PyXPCOMMethod_InterruptWait, 1},
        {"DeinitCOM",     PyXPCOMMethod_DeinitCOM, 1},
        {"AttachThread",  PyXPCOMMethod_AttachThread, 1},
        {"DetachThread",  PyXPCOMMethod_DetachThread, 1},
#endif
	// These should no longer be used - just use the logging.getLogger('pyxpcom')...
	{ NULL }
};

#define REGISTER_IID(t) { \
	PyObject *iid_ob = Py_nsIID::PyObjectFromIID(NS_GET_IID(t)); \
	PyDict_SetItemString(dict, "IID_"#t, iid_ob); \
	Py_DECREF(iid_ob); \
	}

#define REGISTER_INT(val) { \
	PyObject *ob = PyInt_FromLong(val); \
	PyDict_SetItemString(dict, #val, ob); \
	Py_DECREF(ob); \
	}


////////////////////////////////////////////////////////////
// The module init code.
//
extern "C" NS_EXPORT
void
init_xpcom() {
	PyObject *oModule;

	// ensure the framework has valid state to work with.
	if (!PyXPCOM_Globals_Ensure())
		return;

	// Must force Python to start using thread locks
	PyEval_InitThreads();

	// Create the module and add the functions
	oModule = Py_InitModule(MODULE_NAME, xpcom_methods);

	PyObject *dict = PyModule_GetDict(oModule);
	PyObject *pycom_Error = PyXPCOM_Error;
	if (pycom_Error == NULL || PyDict_SetItemString(dict, "error", pycom_Error) != 0)
	{
		PyErr_SetString(PyExc_MemoryError, "can't define error");
		return;
	}
	PyDict_SetItemString(dict, "IIDType", (PyObject *)&Py_nsIID::type);

	REGISTER_IID(nsISupports);
	REGISTER_IID(nsISupportsCString);
	REGISTER_IID(nsISupportsString);
	REGISTER_IID(nsIModule);
	REGISTER_IID(nsIFactory);
	REGISTER_IID(nsIWeakReference);
	REGISTER_IID(nsISupportsWeakReference);
	REGISTER_IID(nsIClassInfo);
	REGISTER_IID(nsIServiceManager);
	REGISTER_IID(nsIComponentRegistrar);

	// Register our custom interfaces.
	REGISTER_IID(nsIComponentManager);
	REGISTER_IID(nsIInterfaceInfoManager);
	REGISTER_IID(nsIEnumerator);
	REGISTER_IID(nsISimpleEnumerator);
	REGISTER_IID(nsIInterfaceInfo);
	REGISTER_IID(nsIInputStream);
	REGISTER_IID(nsIClassInfo);
	REGISTER_IID(nsIVariant);
	// for backward compatibility:
	REGISTER_IID(nsIComponentManagerObsolete);

	// No good reason not to expose this impl detail, and tests can use it
	REGISTER_IID(nsIInternalPython);
    // We have special support for proxies - may as well add their constants!
    REGISTER_INT(PROXY_SYNC);
    REGISTER_INT(PROXY_ASYNC);
    REGISTER_INT(PROXY_ALWAYS);
    // Build flags that may be useful.
    PyObject *ob = PyBool_FromLong(
#ifdef NS_DEBUG
                                   1
#else
                                   0
#endif
                                   );
    PyDict_SetItemString(dict, "NS_DEBUG", ob);
    Py_DECREF(ob);
}

#ifdef VBOX_PYXPCOM
#include <VBox/com/com.h>
using namespace com;

#include <iprt/initterm.h>
#include <iprt/string.h>
#include <iprt/alloca.h>
#include <iprt/stream.h>

extern "C" NS_EXPORT
void
# ifdef VBOX_PYXPCOM_VERSIONED
#  if   PY_VERSION_HEX >= 0x02080000
initVBoxPython2_8() {
#  elif PY_VERSION_HEX >= 0x02070000
initVBoxPython2_7() {
#  elif PY_VERSION_HEX >= 0x02060000
initVBoxPython2_6() {
#  elif PY_VERSION_HEX >= 0x02050000
initVBoxPython2_5() {
#  elif PY_VERSION_HEX >= 0x02040000
initVBoxPython2_4() {
#  elif PY_VERSION_HEX >= 0x02030000
initVBoxPython2_3() {
#  else
#   error "Fix module versioning."
#  endif
# else
initVBoxPython() {
# endif
  static bool s_vboxInited = false;
  if (!s_vboxInited) {
    int rc = 0;

#if defined(VBOX_PATH_APP_PRIVATE_ARCH) && defined(VBOX_PATH_SHARED_LIBS)
    rc = RTR3Init();
#else
    const char *home = getenv("VBOX_PROGRAM_PATH");
    if (home) {
      size_t len = strlen(home);
      char *exepath = (char *)alloca(len + 32);
      memcpy(exepath, home, len);
      memcpy(exepath + len, "/pythonfake", sizeof("/pythonfake"));
      rc = RTR3InitWithProgramPath(exepath);
    } else {
      rc = RTR3Init();
    }
#endif

    rc = com::Initialize();

    if (NS_SUCCEEDED(rc))
    {
      NS_GetMainEventQ (&g_mainEventQ);
    }

    init_xpcom();
  }
}

static
void deinitVBoxPython()
{

  if (g_mainEventQ)
    NS_RELEASE(g_mainEventQ);

  com::Shutdown();
}

#endif /* VBOX_PYXPCOM */
