/* $Id$ */
/** @file
 * DnD - Path list class.
 */

/*
 * Copyright (C) 2014-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_GUEST_DND
#include <VBox/GuestHost/DragAndDrop.h>

#include <iprt/string.h>


/**
 * Returns whether a given MIME format contains file URLs we can work with or not.
 *
 * @returns \c true if the format contains file URLs, \c false if not.
 * @param   pcszFormat          MIME format to check.
 * @param   cchFormatMax        Maximum characters of MIME format to check.
 */
bool DnDMIMEHasFileURLs(const char *pcszFormat, size_t cchFormatMax)
{
    /** @todo "text/uri" also an official variant? */
    return (   RTStrNICmp(pcszFormat, "text/uri-list", cchFormatMax)             == 0
            || RTStrNICmp(pcszFormat, "x-special/gnome-icon-list", cchFormatMax) == 0);
}

/**
 * Returns whether a given MIME format needs an own "Dropped Files" directory or not.
 *
 * @returns \c true if the format needs an own "Dropped Files" directory, \c false if not.
 * @param   pcszFormat          MIME format to check.
 * @param   cchFormatMax        Maximum characters of MIME format to check.
 */
bool DnDMIMENeedsDropDir(const char *pcszFormat, size_t cchFormatMax)
{
    return DnDMIMEHasFileURLs(pcszFormat, cchFormatMax);
}

