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

#include "PyXPCOM_std.h"
#include "nsILocalFile.h"

#include <iprt/asm.h>
#include <iprt/errcore.h>
#include <iprt/semaphore.h>

#ifdef XP_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "windows.h"
#endif

static volatile uint32_t g_cLockCount = 0;
static RTSEMFASTMUTEX    g_lockMain = NULL;

PYXPCOM_EXPORT PyObject *PyXPCOM_Error = NULL;

PyXPCOM_INTERFACE_DEFINE(Py_nsIComponentManager, nsIComponentManager, PyMethods_IComponentManager)
PyXPCOM_INTERFACE_DEFINE(Py_nsIInterfaceInfoManager, nsIInterfaceInfoManager, PyMethods_IInterfaceInfoManager)
PyXPCOM_INTERFACE_DEFINE(Py_nsIEnumerator, nsIEnumerator, PyMethods_IEnumerator)
PyXPCOM_INTERFACE_DEFINE(Py_nsISimpleEnumerator, nsISimpleEnumerator, PyMethods_ISimpleEnumerator)
PyXPCOM_INTERFACE_DEFINE(Py_nsIInterfaceInfo, nsIInterfaceInfo, PyMethods_IInterfaceInfo)
PyXPCOM_INTERFACE_DEFINE(Py_nsIInputStream, nsIInputStream, PyMethods_IInputStream)
PyXPCOM_INTERFACE_DEFINE(Py_nsIClassInfo, nsIClassInfo, PyMethods_IClassInfo)
PyXPCOM_INTERFACE_DEFINE(Py_nsIVariant, nsIVariant, PyMethods_IVariant)
// deprecated, but retained for backward compatibility:
PyXPCOM_INTERFACE_DEFINE(Py_nsIComponentManagerObsolete, nsIComponentManagerObsolete, PyMethods_IComponentManagerObsolete)

////////////////////////////////////////////////////////////
// Lock/exclusion global functions.
//
void PyXPCOM_AcquireGlobalLock(void)
{
	NS_PRECONDITION(g_lockMain != NIL_RTSEMFASTMUTEX, "Cant acquire a NULL lock!");
	RTSemFastMutexRequest(g_lockMain);
}
void PyXPCOM_ReleaseGlobalLock(void)
{
	NS_PRECONDITION(g_lockMain != NIL_RTSEMFASTMUTEX, "Cant release a NULL lock!");
	RTSemFastMutexRelease(g_lockMain);
}

void PyXPCOM_DLLAddRef(void)
{
	// Must be thread-safe, although cant have the Python lock!
	CEnterLeaveXPCOMFramework _celf;
	uint32_t cnt = ASMAtomicIncU32(&g_cLockCount);
	if (cnt==1) { // First call
		if (!Py_IsInitialized()) {
			Py_Initialize();
			// Make sure our Windows framework is all setup.
			PyXPCOM_Globals_Ensure();
			// Make sure we have _something_ as sys.argv.
			if (PySys_GetObject((char*)"argv")==NULL) {
				PyObject *path = PyList_New(0);
#if PY_MAJOR_VERSION <= 2
				PyObject *str = PyString_FromString("");
#else
				PyObject *str = PyUnicode_FromString("");
#endif
				PyList_Append(path, str);
				PySys_SetObject((char*)"argv", path);
				Py_XDECREF(path);
				Py_XDECREF(str);
			}

			/* Done automatically since python 3.7 and deprecated since python 3.9. */
#if PY_VERSION_HEX < 0x03090000
			// Must force Python to start using thread locks, as
			// we are free-threaded (maybe, I think, sometimes :-)
			PyEval_InitThreads();
#endif
			// NOTE: We never finalize Python!!
		}
	}
}
void PyXPCOM_DLLRelease(void)
{
	ASMAtomicDecU32(&g_cLockCount);
}

void pyxpcom_construct(void)
{
    int vrc = RTSemFastMutexCreate(&g_lockMain);
    if (RT_FAILURE(vrc))
        return;

    return; // PR_TRUE;
}

void pyxpcom_destruct(void)
{
    RTSemFastMutexDestroy(g_lockMain);
}

// Yet another attempt at cross-platform library initialization and finalization.
struct DllInitializer {
	DllInitializer() {
		pyxpcom_construct();
	}
	~DllInitializer() {
		pyxpcom_destruct();
	}
} dll_initializer;

////////////////////////////////////////////////////////////
// Other helpers/global functions.
//
PRBool PyXPCOM_Globals_Ensure()
{
	PRBool rc = PR_TRUE;

	// The exception object - we load it from .py code!
	if (PyXPCOM_Error == NULL) {
		rc = PR_FALSE;
		PyObject *mod = NULL;

		mod = PyImport_ImportModule("xpcom");
		if (mod!=NULL) {
			PyXPCOM_Error = PyObject_GetAttrString(mod, "Exception");
			Py_DECREF(mod);
		}
		rc = (PyXPCOM_Error != NULL);
	}
	if (!rc)
		return rc;

	static PRBool bHaveInitXPCOM = PR_FALSE;
	if (!bHaveInitXPCOM) {

		if (!NS_IsXPCOMInitialized()) {
			// not already initialized.
			// Elsewhere, Mozilla can find it itself (we hope!)
			nsresult rv = NS_InitXPCOM2(nsnull, nsnull, nsnull);
			if (NS_FAILED(rv)) {
				PyErr_SetString(PyExc_RuntimeError, "The XPCOM subsystem could not be initialized");
				return PR_FALSE;
			}
		}
		// Even if xpcom was already init, we want to flag it as init!
		bHaveInitXPCOM = PR_TRUE;
		// Register our custom interfaces.

		Py_nsISupports::InitType();
		Py_nsIComponentManager::InitType();
		Py_nsIInterfaceInfoManager::InitType();
		Py_nsIEnumerator::InitType();
		Py_nsISimpleEnumerator::InitType();
		Py_nsIInterfaceInfo::InitType();
		Py_nsIInputStream::InitType();
		Py_nsIClassInfo::InitType();
		Py_nsIVariant::InitType();
		// for backward compatibility:
		Py_nsIComponentManagerObsolete::InitType();

	}
	return rc;
}

