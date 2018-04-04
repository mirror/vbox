/* $Id$ */
/** @file
 * VBox Qt GUI - Declarations of utility functions for handling Darwin Cocoa specific event-handling tasks.
 */

/*
 * Copyright (C) 2010-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___CocoaEventHelper_h
#define ___CocoaEventHelper_h

/* Other VBox includes: */
#include <iprt/cdefs.h>
#include <VBox/VBoxCocoa.h>

/* Cocoa declarations: */
ADD_COCOA_NATIVE_REF(NSEvent);


RT_C_DECLS_BEGIN

/** Calls the -(NSUInteger)modifierFlags method on @a pEvent object and converts the flags to carbon style. */
uint32_t darwinEventModifierFlagsXlated(ConstNativeNSEventRef pEvent);

/** Get the name for a Cocoa @a enmEventType. */
const char *darwinEventTypeName(unsigned long enmEventType);

/** Debug helper function for dumping a Cocoa event to stdout.
  * @param   pszPrefix  Brings the message prefix.
  * @param   pEvent     Brings the Cocoa event. */
void darwinPrintEvent(const char *pszPrefix, ConstNativeNSEventRef pEvent);

/** Posts stripped mouse event based on passed @a pEvent. */
void darwinPostStrippedMouseEvent(ConstNativeNSEventRef pEvent);

RT_C_DECLS_END


#endif /* !___CocoaEventHelper_h */

