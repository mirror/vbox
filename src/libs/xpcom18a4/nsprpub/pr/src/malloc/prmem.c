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
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

/*
** Thread safe versions of malloc, free, realloc, calloc and cfree.
*/

#include "primpl.h"
#ifdef VBOX_USE_IPRT_IN_NSPR
# include <iprt/mem.h>
#endif

/*
** The PR_Malloc, PR_Calloc, PR_Realloc, and PR_Free functions simply
** call their libc equivalents now.  This may seem redundant, but it
** ensures that we are calling into the same runtime library.  On
** Win32, it is possible to have multiple runtime libraries (e.g.,
** objects compiled with /MD and /MDd) in the same process, and
** they maintain separate heaps, which cannot be mixed.
*/
PR_IMPLEMENT(void *) PR_Malloc(PRUint32 size)
{
#if defined (WIN16)
    return PR_MD_malloc( (size_t) size);
#else
# ifdef VBOX_USE_IPRT_IN_NSPR
    return RTMemAlloc(RT_MAX(size, 1));
# else
    return malloc(size);
# endif
#endif
}

PR_IMPLEMENT(void *) PR_Calloc(PRUint32 nelem, PRUint32 elsize)
{
#if defined (WIN16)
    return PR_MD_calloc( (size_t)nelem, (size_t)elsize );

#else
# ifdef VBOX_USE_IPRT_IN_NSPR
    return RTMemAllocZ(RT_MAX(nelem * (size_t)elsize, 1));
# else
    return calloc(nelem, elsize);
# endif
#endif
}

PR_IMPLEMENT(void *) PR_Realloc(void *ptr, PRUint32 size)
{
#if defined (WIN16)
    return PR_MD_realloc( ptr, (size_t) size);
#else
# ifdef VBOX_USE_IPRT_IN_NSPR
    return RTMemRealloc(ptr, size);
# else
    return realloc(ptr, size);
# endif
#endif
}

PR_IMPLEMENT(void) PR_Free(void *ptr)
{
#if defined (WIN16)
    PR_MD_free( ptr );
#else
# ifdef VBOX_USE_IPRT_IN_NSPR
    RTMemFree(ptr);
# else
    free(ptr);
# endif
#endif
}

