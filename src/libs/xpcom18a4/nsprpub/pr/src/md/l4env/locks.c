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
** File:               l4env/locks.c
** Descritpion:        Implemenation for locks using l4env
** Exports:            none, that I am aware of
*/

/**
 * @note The Windows implementation uses critical sections to implement locks.
 *       Solaris uses mutexes.
 */

#include "prlog.h"
#include "primpl.h"

PR_IMPLEMENT(void)
_MD_init_locks(void) { } /* PR_InitLocks */

PR_IMPLEMENT(PRStatus)
_MD_new_lock(
    struct _MDLock *md_lock)
{
    PR_ASSERT("_MD_new_lock called!");
    return 0;
}  /* _MD_new_lock */

PR_IMPLEMENT(void)
_MD_free_lock(
    struct _MDLock *md_lock)
{
    PR_ASSERT("_MD_free_lock called!");
}  /* _MD_free_lock */

PR_IMPLEMENT(void)
_MD_lock(
    struct _MDLock *md_lock)
{
    PR_ASSERT("_MD_lock called!");
}  /* _MD_lock */

PR_IMPLEMENT(void)
_MD_unlock(
    struct _MDLock *md_lock)
{
    PR_ASSERT("_MD_unlock called!");
}  /* _MD_unlock */

/* locks.c */
