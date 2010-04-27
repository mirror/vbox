/** @file
 * VBoxGuestLibR0 - Guest Logging facility.
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef __VBOXGUESTLOG__H
#define __VBOXGUESTLOG__H

#ifndef RT_OS_WINDOWS

/* Since I don't know the background for the stuff below, I prefer not to
   change it.  I don't need it or want it for backdoor logging inside the
   Linux Guest Additions kernel modules though. */
/* Update: bird made this the stance on all non-windows platforms. */
# include <VBox/log.h>

#else  /* RT_OS_LINUX not defined */
/* Save LOG_ENABLED state, because "VBox/rt/log.h"
 * may undefine it for IN_RING0 code.
 */
# if (defined(DEBUG) && !defined(NO_LOGGING)) || defined(LOG_ENABLED)
#  define __LOG_ENABLED_SAVED__
# endif

# if (defined(DEBUG) && !defined(NO_LOGGING)) || defined(LOG_ENABLED)
#  ifdef VBOX_GUEST
#   include <VBox/log.h>
#   undef Log
#   define Log(a)  RTLogBackdoorPrintf a
#  else
#   define Log(a)  DbgPrint a
#  endif
# else
#  define Log(a)
# endif

# ifdef __LOG_ENABLED_SAVED__
#  define LOG_ENABLED
#  undef __LOG_ENABLED_SAVED__
# endif

#endif  /* RT_OS_LINUX not defined */

#endif /* __VBOXGUESTLOG__H */
