/** @file
 *
 * vrdpvd.sys - VirtualBox Windows NT/2000/XP guest mirror video driver
 *
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* dprintf */
#if (defined(DEBUG) && !defined(NO_LOGGING)) || defined(LOG_ENABLED)
# ifdef LOG_TO_BACKDOOR
#  include <VBox/log.h>
#  define dprintf(a) RTLogBackdoorPrintf a
# else
#  define dprintf(a) do {} while (0)
# endif
#else
# define dprintf(a) do {} while (0)
#endif

/* Windows version identifier. */
typedef enum
{
    UNKNOWN_WINVERSION = 0,
    WINNT4 = 1,
    WIN2K  = 2,
    WINXP  = 3
} winVersion_t;

winVersion_t vboxQueryWinVersion (void);
