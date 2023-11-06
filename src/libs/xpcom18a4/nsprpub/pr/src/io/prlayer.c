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
** File:        prlayer.c
** Description: Routines for handling pushable protocol modules on sockets.
*/

#include "primpl.h"
#include "prerror.h"
#include "prmem.h"
#include "prlock.h"
#include "prlog.h"
#include "prio.h"

#include <string.h> /* for memset() */

void PR_CALLBACK pl_FDDestructor(PRFileDesc *fd)
{
    PR_ASSERT(fd != NULL);
    if (NULL != fd->lower) fd->lower->higher = fd->higher;
    if (NULL != fd->higher) fd->higher->lower = fd->lower;
    PR_DELETE(fd);
}

PR_IMPLEMENT(PRFileDesc*) PR_CreateIOLayerStub(
    PRDescIdentity ident, const PRIOMethods *methods)
{
    PRFileDesc *fd = NULL;
    PR_ASSERT((PR_NSPR_IO_LAYER != ident) && (PR_TOP_IO_LAYER != ident));
    if ((PR_NSPR_IO_LAYER == ident) || (PR_TOP_IO_LAYER == ident))
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
    else
    {
        fd = PR_NEWZAP(PRFileDesc);
        if (NULL == fd)
            PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
        else
        {
            fd->methods = methods;
            fd->dtor = pl_FDDestructor;
            fd->identity = ident;
        }
    }
    return fd;
}  /* PR_CreateIOLayerStub */

/*
 * _PR_DestroyIOLayer
 *		Delete the stack head of a new style stack.
 */

static PRStatus _PR_DestroyIOLayer(PRFileDesc *stack)
{
    if (NULL == stack)
        return PR_FAILURE;
    else {
        PR_DELETE(stack);
    	return PR_SUCCESS;
    }
}  /* _PR_DestroyIOLayer */

PR_IMPLEMENT(PRStatus) PR_PushIOLayer(
    PRFileDesc *stack, PRDescIdentity id, PRFileDesc *fd)
{
    PRFileDesc *insert = PR_GetIdentitiesLayer(stack, id);

    PR_ASSERT(fd != NULL);
    PR_ASSERT(stack != NULL);
    PR_ASSERT(insert != NULL);
    PR_ASSERT(PR_IO_LAYER_HEAD != id);
    if ((NULL == stack) || (NULL == fd) || (NULL == insert))
    {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return PR_FAILURE;
    }

    if (stack == insert)
    {
		/* going on top of the stack */
		/* old-style stack */	
		PRFileDesc copy = *stack;
		*stack = *fd;
		*fd = copy;
		fd->higher = stack;
		stack->lower = fd;
		stack->higher = NULL;
	} else {
        /*
		 * going somewhere in the middle of the stack for both old and new
		 * style stacks, or going on top of stack for new style stack
		 */
        fd->lower = insert;
        fd->higher = insert->higher;

        insert->higher->lower = fd;
        insert->higher = fd;
    }

    return PR_SUCCESS;
}

PR_IMPLEMENT(PRFileDesc*) PR_PopIOLayer(PRFileDesc *stack, PRDescIdentity id)
{
    PRFileDesc *extract = PR_GetIdentitiesLayer(stack, id);

    PR_ASSERT(0 != id);
    PR_ASSERT(NULL != stack);
    PR_ASSERT(NULL != extract);
    if ((NULL == stack) || (0 == id) || (NULL == extract))
    {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return NULL;
    }

    if (extract == stack) {
        /* popping top layer of the stack */
		/* old style stack */
        PRFileDesc copy = *stack;
        extract = stack->lower;
        *stack = *extract;
        *extract = copy;
        stack->higher = NULL;
	} else if ((PR_IO_LAYER_HEAD == stack->identity) &&
					(extract == stack->lower) && (extract->lower == NULL)) {
			/*
			 * new style stack
			 * popping the only layer in the stack; delete the stack too
			 */
			stack->lower = NULL;
			_PR_DestroyIOLayer(stack);
	} else {
		/* for both kinds of stacks */
        extract->lower->higher = extract->higher;
        extract->higher->lower = extract->lower;
    }
    extract->higher = extract->lower = NULL;
    return extract;
}  /* PR_PopIOLayer */

#define ID_CACHE_INCREMENT 16
typedef struct _PRIdentity_cache
{
    PRLock *ml;
    char **name;
    PRIntn length;
    PRDescIdentity ident;
} _PRIdentity_cache;

static _PRIdentity_cache identity_cache;

PR_IMPLEMENT(PRDescIdentity) PR_GetUniqueIdentity(const char *layer_name)
{
    PRDescIdentity identity, length;
    char **names = NULL, *name = NULL, **old = NULL;

    if (!_pr_initialized) _PR_ImplicitInitialization();

    PR_ASSERT((PRDescIdentity)0x7fff > identity_cache.ident);

    if (NULL != layer_name)
    {
        name = (char*)PR_Malloc(strlen(layer_name) + 1);
        if (NULL == name)
        {
            PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
            return PR_INVALID_IO_LAYER;
        }
        strcpy(name, layer_name);
    }

    /* this initial code runs unsafe */
retry:
    PR_ASSERT(NULL == names);
    length = identity_cache.length;
    if (length < (identity_cache.ident + 1))
    {
        length += ID_CACHE_INCREMENT;
        names = (char**)PR_CALLOC(length * sizeof(char*));
        if (NULL == names)
        {
            if (NULL != name) PR_DELETE(name);
            PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
            return PR_INVALID_IO_LAYER;
        }
    }

    /* now we get serious about thread safety */
    PR_Lock(identity_cache.ml);
    PR_ASSERT(identity_cache.ident <= identity_cache.length);
    identity = identity_cache.ident + 1;
    if (identity > identity_cache.length)  /* there's no room */
    {
        /* we have to do something - hopefully it's already done */
        if ((NULL != names) && (length >= identity))
        {
            /* what we did is still okay */
            if (identity_cache.length)
                memcpy(
                    names, identity_cache.name,
                    identity_cache.length * sizeof(char*));
            old = identity_cache.name;
            identity_cache.name = names;
            identity_cache.length = length;
            names = NULL;
        }
        else
        {
            PR_ASSERT(identity_cache.ident <= identity_cache.length);
            PR_Unlock(identity_cache.ml);
            if (NULL != names) PR_DELETE(names);
            goto retry;
        }
    }
    if (NULL != name) /* there's a name to be stored */
    {
        identity_cache.name[identity] = name;
    }
    identity_cache.ident = identity;
    PR_ASSERT(identity_cache.ident <= identity_cache.length);
    PR_Unlock(identity_cache.ml);

    if (NULL != old) PR_DELETE(old);
    if (NULL != names) PR_DELETE(names);

    return identity;
}  /* PR_GetUniqueIdentity */

PR_IMPLEMENT(const char*) PR_GetNameForIdentity(PRDescIdentity ident)
{
    if (!_pr_initialized) _PR_ImplicitInitialization();

    if (PR_TOP_IO_LAYER == ident) return NULL;

    PR_ASSERT(ident <= identity_cache.ident);
    return (ident > identity_cache.ident) ? NULL : identity_cache.name[ident];
}  /* PR_GetNameForIdentity */

PR_IMPLEMENT(PRDescIdentity) PR_GetLayersIdentity(PRFileDesc* fd)
{
    PR_ASSERT(NULL != fd);
    if (PR_IO_LAYER_HEAD == fd->identity) {
    	PR_ASSERT(NULL != fd->lower);
    	return fd->lower->identity;
	} else
    	return fd->identity;
}  /* PR_GetLayersIdentity */

PR_IMPLEMENT(PRFileDesc*) PR_GetIdentitiesLayer(PRFileDesc* fd, PRDescIdentity id)
{
    PRFileDesc *layer = fd;

    if (PR_TOP_IO_LAYER == id) {
    	if (PR_IO_LAYER_HEAD == fd->identity)
			return fd->lower;
		else 
			return fd;
	}

    for (layer = fd; layer != NULL; layer = layer->lower)
    {
        if (id == layer->identity) return layer;
    }
    for (layer = fd; layer != NULL; layer = layer->higher)
    {
        if (id == layer->identity) return layer;
    }
    return NULL;
}  /* PR_GetIdentitiesLayer */

void _PR_InitLayerCache(void)
{
    memset(&identity_cache, 0, sizeof(identity_cache));
    identity_cache.ml = PR_NewLock();
    PR_ASSERT(NULL != identity_cache.ml);
}  /* _PR_InitLayerCache */

void _PR_CleanupLayerCache(void)
{
    if (identity_cache.ml)
    {
        PR_DestroyLock(identity_cache.ml);
        identity_cache.ml = NULL;
    }

    if (identity_cache.name)
    {
        PRDescIdentity ident;

        for (ident = 0; ident <= identity_cache.ident; ident++)
            PR_DELETE(identity_cache.name[ident]);

        PR_DELETE(identity_cache.name);
    }
}  /* _PR_CleanupLayerCache */

/* prlayer.c */
