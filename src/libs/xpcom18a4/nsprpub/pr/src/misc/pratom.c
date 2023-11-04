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
**     PR Atomic operations
*/


#include "pratom.h"
#include "primpl.h"

void _PR_InitAtomic(void)
{
    _PR_MD_INIT_ATOMIC();
}

PR_IMPLEMENT(PRInt32)
PR_AtomicIncrement(PRInt32 *val)
{
    return _PR_MD_ATOMIC_INCREMENT(val);
}

PR_IMPLEMENT(PRInt32)
PR_AtomicDecrement(PRInt32 *val)
{
    return _PR_MD_ATOMIC_DECREMENT(val);
}

PR_IMPLEMENT(PRInt32)
PR_AtomicSet(PRInt32 *val, PRInt32 newval)
{
    return _PR_MD_ATOMIC_SET(val, newval);
}

PR_IMPLEMENT(PRInt32)
PR_AtomicAdd(PRInt32 *ptr, PRInt32 val)
{
    return _PR_MD_ATOMIC_ADD(ptr, val);
}
/*
 * For platforms, which don't support the CAS (compare-and-swap) instruction
 * (or an equivalent), the stack operations are implemented by use of PRLock
 */

PR_IMPLEMENT(PRStack *)
PR_CreateStack(const char *stack_name)
{
PRStack *stack;

    if (!_pr_initialized) {
        _PR_ImplicitInitialization();
    }

    if ((stack = PR_NEW(PRStack)) == NULL) {
		return NULL;
	}
	if (stack_name) {
		stack->prstk_name = (char *) PR_Malloc(strlen(stack_name) + 1);
		if (stack->prstk_name == NULL) {
			PR_DELETE(stack);
			return NULL;
		}
		strcpy(stack->prstk_name, stack_name);
	} else
		stack->prstk_name = NULL;

#ifndef _PR_HAVE_ATOMIC_CAS
    stack->prstk_lock = PR_NewLock();
	if (stack->prstk_lock == NULL) {
		PR_Free(stack->prstk_name);
		PR_DELETE(stack);
		return NULL;
	}
#endif /* !_PR_HAVE_ATOMIC_CAS */

	stack->prstk_head.prstk_elem_next = NULL;
	
    return stack;
}

PR_IMPLEMENT(PRStatus)
PR_DestroyStack(PRStack *stack)
{
	if (stack->prstk_head.prstk_elem_next != NULL) {
		PR_SetError(PR_INVALID_STATE_ERROR, 0);
		return PR_FAILURE;
	}

	if (stack->prstk_name)
		PR_Free(stack->prstk_name);
#ifndef _PR_HAVE_ATOMIC_CAS
	PR_DestroyLock(stack->prstk_lock);
#endif /* !_PR_HAVE_ATOMIC_CAS */
	PR_DELETE(stack);

	return PR_SUCCESS;
}

#ifndef _PR_HAVE_ATOMIC_CAS

PR_IMPLEMENT(void)
PR_StackPush(PRStack *stack, PRStackElem *stack_elem)
{
    PR_Lock(stack->prstk_lock);
	stack_elem->prstk_elem_next = stack->prstk_head.prstk_elem_next;
	stack->prstk_head.prstk_elem_next = stack_elem;
    PR_Unlock(stack->prstk_lock);
    return;
}

PR_IMPLEMENT(PRStackElem *)
PR_StackPop(PRStack *stack)
{
PRStackElem *element;

    PR_Lock(stack->prstk_lock);
	element = stack->prstk_head.prstk_elem_next;
	if (element != NULL) {
		stack->prstk_head.prstk_elem_next = element->prstk_elem_next;
		element->prstk_elem_next = NULL;	/* debugging aid */
	}
    PR_Unlock(stack->prstk_lock);
    return element;
}
#endif /* !_PR_HAVE_ATOMIC_CAS */
