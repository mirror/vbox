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
 *----------------------------------------------------------------------
 *
 * prtime.h --
 *
 *     NSPR date and time functions
 *
 *-----------------------------------------------------------------------
 */

#ifndef prtime_h___
#define prtime_h___

#include "prlong.h"

PR_BEGIN_EXTERN_C

/**********************************************************************/
/************************* TYPES AND CONSTANTS ************************/
/**********************************************************************/

#define PR_MSEC_PER_SEC		1000UL
#define PR_USEC_PER_SEC		1000000UL
#define PR_USEC_PER_MSEC	1000UL

/*
 * PRTime --
 *
 *     NSPR represents basic time as 64-bit signed integers relative
 *     to midnight (00:00:00), January 1, 1970 Greenwich Mean Time (GMT).
 *     (GMT is also known as Coordinated Universal Time, UTC.)
 *     The units of time are in microseconds. Negative times are allowed
 *     to represent times prior to the January 1970 epoch. Such values are
 *     intended to be exported to other systems or converted to human
 *     readable form.
 *
 *     Notes on porting: PRTime corresponds to time_t in ANSI C.  NSPR 1.0
 *     simply uses PRInt64.
 */

typedef PRInt64 PRTime;

PR_END_EXTERN_C

#endif /* prtime_h___ */
