/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 *
 * Contributor(s): Steve Streeter (Hewlett-Packard Company)
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable
 * instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#include "primpl.h"

#ifdef VBOX_WITH_XPCOM_NAMESPACE_CLEANUP
#define PR_FindLibrary VBoxNsprPR_FindLibrary
#endif /* VBOX_WITH_XPCOM_NAMESPACE_CLEANUP */

#include <string.h>

# include <iprt/ldr.h>
# include <iprt/path.h>
# include <iprt/err.h>

# if defined(XP_MACOSX) /** @todo Add some equivalent to PR_GetLibraryFilePathname. */
#  include <mach-o/dyld.h>
# endif

# ifdef USE_DLFCN
#  include <dlfcn.h>
/* Define these on systems that don't have them. */
#  ifndef RTLD_NOW
#   define RTLD_NOW 0
#  endif
#  ifndef RTLD_LAZY
#   define RTLD_LAZY RTLD_NOW
#  endif
#  ifndef RTLD_GLOBAL
#   define RTLD_GLOBAL 0
#  endif
#  ifndef RTLD_LOCAL
#   define RTLD_LOCAL 0
#  endif
# endif

#ifdef VBOX_USE_IPRT_IN_NSPR
# include <iprt/mem.h>
# include <iprt/string.h>
# undef  strdup
# define strdup(str) RTStrDup(str)
#endif

#define _PR_DEFAULT_LD_FLAGS PR_LD_LAZY

/************************************************************************/

struct PRLibrary {
    char*                       name;  /* Our own copy of the name string */
    PRLibrary*                  next;
    int                         refCount;

    RTLDRMOD                    dlh;
};

static PRLibrary *pr_loadmap;
static PRLibrary *pr_exe_loadmap;
static PRMonitor *pr_linker_lock;
static char* _pr_currentLibPath = NULL;

static PRLibrary *pr_LoadLibraryByPathname(const char *name, PRIntn flags);

/************************************************************************/

static void DLLErrorInternal(PRIntn oserr)
/*
** This whole function, and most of the code in this file, are run
** with a big hairy lock wrapped around it. Not the best of situations,
** but will eventually come up with the right answer.
*/
{
    const char *error = NULL;
#ifdef USE_DLFCN
    error = dlerror();  /* $$$ That'll be wrong some of the time - AOF */
#elif defined(HAVE_STRERROR)
    error = strerror(oserr);  /* this should be okay */
#else
    error = errno_string(oserr);
#endif
    if (NULL != error)
        PR_SetErrorText(strlen(error), error);
}  /* DLLErrorInternal */

void _PR_InitLinker(void)
{
    PRLibrary *lm;

    if (!pr_linker_lock) {
        pr_linker_lock = PR_NewNamedMonitor("linker-lock");
    }
    PR_EnterMonitor(pr_linker_lock);

    lm = PR_NEWZAP(PRLibrary);
    lm->name = strdup("Executable");
    lm->dlh = NIL_RTLDRMOD;
    lm->refCount    = 1;
    pr_exe_loadmap  = lm;
    pr_loadmap      = lm;

    PR_LOG(_pr_linker_lm, PR_LOG_MIN, ("Loaded library %s (init)", lm?lm->name:"NULL"));
    PR_ExitMonitor(pr_linker_lock);
}

/*
 * _PR_ShutdownLinker was originally only used on WIN16 (see above),
 * but I think it should also be used on other platforms.  However,
 * I disagree with the original implementation's unloading the dlls
 * for the application.  Any dlls that still remain on the pr_loadmap
 * list when NSPR shuts down are application programming errors.  The
 * only exception is pr_exe_loadmap, which was added to the list by
 * NSPR and hence should be cleaned up by NSPR.
 */
void _PR_ShutdownLinker(void)
{
    /* FIXME: pr_exe_loadmap should be destroyed. */

    PR_DestroyMonitor(pr_linker_lock);
    pr_linker_lock = NULL;

    if (_pr_currentLibPath) {
        RTStrFree(_pr_currentLibPath);
        _pr_currentLibPath = NULL;
    }
}

/******************************************************************************/

PR_IMPLEMENT(PRStatus) PR_SetLibraryPath(const char *path)
{
    PRStatus rv = PR_SUCCESS;

    if (!_pr_initialized) _PR_ImplicitInitialization();
    PR_EnterMonitor(pr_linker_lock);
    if (_pr_currentLibPath) {
        RTStrFree(_pr_currentLibPath);
    }
    if (path) {
        _pr_currentLibPath = strdup(path);
        if (!_pr_currentLibPath) {
            PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
        rv = PR_FAILURE;
        }
    } else {
        _pr_currentLibPath = 0;
    }
    PR_ExitMonitor(pr_linker_lock);
    return rv;
}

/*
** Return the library path for finding shared libraries.
*/
PR_IMPLEMENT(char *)
PR_GetLibraryPath(void)
{
    char *ev;
    char *copy = NULL;  /* a copy of _pr_currentLibPath */

    if (!_pr_initialized) _PR_ImplicitInitialization();
    PR_EnterMonitor(pr_linker_lock);
    if (_pr_currentLibPath != NULL) {
        goto exit;
    }

    /* initialize pr_currentLibPath */

#if defined(XP_UNIX) || defined(XP_BEOS)
#if defined(USE_DLFCN) || defined(USE_MACH_DYLD) || defined(XP_BEOS)
    {
    char *p=NULL;
    int len;

# if defined(VBOX) && defined(XP_MACOSX)
    ev = getenv("DYLD_LIBRARY_PATH");
# else
    ev = getenv("LD_LIBRARY_PATH");
# endif
    if (!ev) {
        ev = "/usr/lib:/lib";
    }

    len = strlen(ev) + 1;        /* +1 for the null */

    p = (char*) RTStrAlloc(len);
    if (p) {
        strcpy(p, ev);
    }   /* if (p)  */
    ev = p;
    PR_LOG(_pr_io_lm, PR_LOG_NOTICE, ("linker path '%s'", ev));

    }
#else
    /* AFAIK there isn't a library path with the HP SHL interface --Rob */
    ev = strdup("");
# endif
#endif

    /*
     * If ev is NULL, we have run out of memory
     */
    _pr_currentLibPath = ev;

  exit:
    if (_pr_currentLibPath) {
        copy = RTMemDup(_pr_currentLibPath, strlen(_pr_currentLibPath) + 1);
    }
    PR_ExitMonitor(pr_linker_lock);
    if (!copy) {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
    }
    return copy;
}

/*
** Build library name from path, lib and extensions
*/
PR_IMPLEMENT(char*)
PR_GetLibraryName(const char *path, const char *lib)
{
    char *fullname;

#if defined(XP_UNIX) || defined(XP_BEOS)
    if (strstr(lib, PR_DLL_SUFFIX) == NULL)
    {
        if (path) {
            fullname = PR_smprintf("%s/lib%s%s", path, lib, PR_DLL_SUFFIX);
        } else {
            fullname = PR_smprintf("lib%s%s", lib, PR_DLL_SUFFIX);
        }
    } else {
        if (path) {
            fullname = PR_smprintf("%s/%s", path, lib);
        } else {
            fullname = PR_smprintf("%s", lib);
        }
    }
#endif /* XP_UNIX || XP_BEOS */
    return fullname;
}

/*
** Free the memory allocated, for the caller, by PR_GetLibraryName
*/
PR_IMPLEMENT(void)
PR_FreeLibraryName(char *mem)
{
    PR_smprintf_free(mem);
}

static PRLibrary*
pr_UnlockedFindLibrary(const char *name)
{
    PRLibrary* lm = pr_loadmap;
    const char* np = strrchr(name, PR_DIRECTORY_SEPARATOR);
    np = np ? np + 1 : name;
    while (lm) {
    const char* cp = strrchr(lm->name, PR_DIRECTORY_SEPARATOR);
    cp = cp ? cp + 1 : lm->name;

    if (strcmp(np, cp)  == 0)
    {
        /* found */
        lm->refCount++;
        PR_LOG(_pr_linker_lm, PR_LOG_MIN,
           ("%s incr => %d (find lib)",
            lm->name, lm->refCount));
        return lm;
    }
    lm = lm->next;
    }
    return NULL;
}

PR_IMPLEMENT(PRLibrary*)
PR_LoadLibraryWithFlags(PRLibSpec libSpec, PRIntn flags)
{
    if (flags == 0) {
        flags = _PR_DEFAULT_LD_FLAGS;
    }
    switch (libSpec.type) {
        case PR_LibSpec_Pathname:
            return pr_LoadLibraryByPathname(libSpec.value.pathname, flags);
        default:
            PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
            return NULL;
    }
}

PR_IMPLEMENT(PRLibrary*)
PR_LoadLibrary(const char *name)
{
    PRLibSpec libSpec;

    libSpec.type = PR_LibSpec_Pathname;
    libSpec.value.pathname = name;
    return PR_LoadLibraryWithFlags(libSpec, 0);
}

/*
** Dynamically load a library. Only load libraries once, so scan the load
** map first.
*/
static PRLibrary*
pr_LoadLibraryByPathname(const char *name, PRIntn flags)
{
    PRLibrary *lm;
    PRLibrary* result;
    PRInt32 oserr;

    if (!_pr_initialized) _PR_ImplicitInitialization();

    /* See if library is already loaded */
    PR_EnterMonitor(pr_linker_lock);

    result = pr_UnlockedFindLibrary(name);
    if (result != NULL) goto unlock;

    lm = PR_NEWZAP(PRLibrary);
    if (lm == NULL) {
        oserr = _MD_ERRNO();
        goto unlock;
    }

    oserr = RTLdrLoad(name, &lm->dlh);
    if (RT_FAILURE(oserr))
        goto unlock;
    lm->name = strdup(name);
    lm->refCount = 1;
    lm->next = pr_loadmap;
    pr_loadmap = lm;

    result = lm;    /* success */
    PR_LOG(_pr_linker_lm, PR_LOG_MIN, ("Loaded library %s (load lib)", lm->name));

  unlock:
    if (result == NULL) {
        PR_SetError(PR_LOAD_LIBRARY_ERROR, oserr);
        DLLErrorInternal(oserr);  /* sets error text */
    }
    PR_ExitMonitor(pr_linker_lock);
    return result;
}

PR_IMPLEMENT(PRLibrary*)
PR_FindLibrary(const char *name)
{
    PRLibrary* result;

    if (!_pr_initialized) _PR_ImplicitInitialization();
    PR_EnterMonitor(pr_linker_lock);
    result = pr_UnlockedFindLibrary(name);
    PR_ExitMonitor(pr_linker_lock);
    return result;
}


/*
** Unload a shared library which was loaded via PR_LoadLibrary
*/
PR_IMPLEMENT(PRStatus)
PR_UnloadLibrary(PRLibrary *lib)
{
    int result = 0;
    PRStatus status = PR_SUCCESS;

    if ((lib == 0) || (lib->refCount <= 0)) {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return PR_FAILURE;
    }

    PR_EnterMonitor(pr_linker_lock);
    if (--lib->refCount > 0) {
    PR_LOG(_pr_linker_lm, PR_LOG_MIN,
           ("%s decr => %d",
        lib->name, lib->refCount));
    goto done;
    }

    result = RTLdrClose(lib->dlh);
    lib->dlh = NIL_RTLDRMOD;

    /* unlink from library search list */
    if (pr_loadmap == lib)
        pr_loadmap = pr_loadmap->next;
    else if (pr_loadmap != NULL) {
        PRLibrary* prev = pr_loadmap;
        PRLibrary* next = pr_loadmap->next;
        while (next != NULL) {
            if (next == lib) {
                prev->next = next->next;
                goto freeLib;
            }
            prev = next;
            next = next->next;
        }
        /*
         * fail (the library is not on the _pr_loadmap list),
         * but don't wipe out an error from dlclose/shl_unload.
         */
        PR_ASSERT(!"_pr_loadmap and lib->refCount inconsistent");
        if (result == 0) {
            PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
            status = PR_FAILURE;
        }
    }
    /*
     * We free the PRLibrary structure whether dlclose/shl_unload
     * succeeds or not.
     */

  freeLib:
    PR_LOG(_pr_linker_lm, PR_LOG_MIN, ("Unloaded library %s", lib->name));
    RTStrFree(lib->name);

    lib->name = NULL;
    PR_DELETE(lib);
    if (result != 0) {
        PR_SetError(PR_UNLOAD_LIBRARY_ERROR, _MD_ERRNO());
        DLLErrorInternal(_MD_ERRNO());
        status = PR_FAILURE;
    }

done:
    PR_ExitMonitor(pr_linker_lock);
    return status;
}

static void*
pr_FindSymbolInLib(PRLibrary *lm, const char *name)
{
    void *f = NULL;

    if (RT_FAILURE(RTLdrGetSymbol(lm->dlh, name, &f)))
        f = NULL;

    if (f == NULL) {
        PR_SetError(PR_FIND_SYMBOL_ERROR, _MD_ERRNO());
        DLLErrorInternal(_MD_ERRNO());
    }
    return f;
}

/*
** Called by class loader to resolve missing native's
*/
PR_IMPLEMENT(void*)
PR_FindSymbol(PRLibrary *lib, const char *raw_name)
{
    void *f = NULL;
    const char *name;

    name = raw_name;

    PR_EnterMonitor(pr_linker_lock);
    PR_ASSERT(lib != NULL);
    f = pr_FindSymbolInLib(lib, name);

    PR_ExitMonitor(pr_linker_lock);
    return f;
}

/*
** Return the address of the function 'raw_name' in the library 'lib'
*/
PR_IMPLEMENT(PRFuncPtr)
PR_FindFunctionSymbol(PRLibrary *lib, const char *raw_name)
{
    return ((PRFuncPtr) PR_FindSymbol(lib, raw_name));
}

PR_IMPLEMENT(void*)
PR_FindSymbolAndLibrary(const char *raw_name, PRLibrary* *lib)
{
    void *f = NULL;
    const char *name;
    PRLibrary* lm;

    if (!_pr_initialized) _PR_ImplicitInitialization();

    name = raw_name;

    PR_EnterMonitor(pr_linker_lock);

    /* search all libraries */
    for (lm = pr_loadmap; lm != NULL; lm = lm->next) {
        f = pr_FindSymbolInLib(lm, name);
        if (f != NULL) {
            *lib = lm;
            lm->refCount++;
            PR_LOG(_pr_linker_lm, PR_LOG_MIN,
                       ("%s incr => %d (for %s)",
                    lm->name, lm->refCount, name));
            break;
        }
    }

    PR_ExitMonitor(pr_linker_lock);
    return f;
}

PR_IMPLEMENT(char *)
PR_GetLibraryFilePathname(const char *name, PRFuncPtr addr)
{
#if defined(SOLARIS) || defined(LINUX) || defined(FREEBSD)
    Dl_info dli;
    char *result;

    if (dladdr((void *)addr, &dli) == 0) {
        PR_SetError(PR_LIBRARY_NOT_LOADED_ERROR, _MD_ERRNO());
        DLLErrorInternal(_MD_ERRNO());
        return NULL;
    }
    result = PR_Malloc(strlen(dli.dli_fname)+1);
    if (result != NULL) {
        strcpy(result, dli.dli_fname);
    }
    return result;
#elif defined(USE_MACH_DYLD)
    char *result;
    char const *image_name;
    int i, count = _dyld_image_count();

    for (i = 0; i < count; i++) {
        image_name = _dyld_get_image_name(i);
        if (strstr(image_name, name) != NULL) {
            result = PR_Malloc(strlen(image_name)+1);
            if (result != NULL) {
                strcpy(result, image_name);
            }
            return result;
        }
    }
    PR_SetError(PR_LIBRARY_NOT_LOADED_ERROR, 0);
    return NULL;
#else
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return NULL;
#endif
}

