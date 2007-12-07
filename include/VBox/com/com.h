/** @file
 * MS COM / XPCOM Abstraction Layer:
 * COM initialization / shutdown
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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

#ifndef ___VBox_com_com_h
#define ___VBox_com_com_h

#include "VBox/com/defs.h"

namespace com
{

/**
 *  Initializes the COM runtime.
 *  Must be called on every thread that uses COM, before any COM activity.
 *
 *  @return COM result code
 */
HRESULT Initialize();

/**
 *  Shuts down the COM runtime.
 *  Must be called on every thread before termination.
 *  No COM calls may be made after this method returns.
 */
HRESULT Shutdown();

/**
 *  Resolves a given interface ID to a string containing the interface name.
 *  If, for some reason, the given IID cannot be resolved to a name, a NULL
 *  string is returned. A non-NULL string returned by this funciton must be
 *  freed using SysFreeString().
 *
 *  @param aIID     ID of the interface to get a name for
 *  @param aName    Resolved interface name or @c NULL on error
 */
void GetInterfaceNameByIID (const GUID &aIID, BSTR *aName);

/** 
 *  Returns the VirtualBox user home directory.
 *
 *  On failure, this function will return a path that caused a failure (or
 *  NULL if the faiulre is not path-related).
 *
 *  On success, this function will try to create the returned directory if it
 *  doesn't exist yet. This may also fail with the corresponding status code.
 *
 *  If @a aDirLen is smaller than RTPATH_MAX then there is a great chance that
 *  this method will return VERR_BUFFER_OVERFLOW.
 *
 *  @param aDir     Buffer to store the directory string in UTF-8 encoding.
 *  @param aDirLen  Length of the supplied buffer including space for the
 *                  terminating null character, in bytes.
 *  @return         VBox status code.
 */
int GetVBoxUserHomeDirectory (char *aDir, size_t aDirLen);

} /* namespace com */

#endif

