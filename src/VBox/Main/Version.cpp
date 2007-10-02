/** @file
 *
 * Some version stuff.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <iprt/string.h>
#include <VBox/version.h>
#include "Version.h"

/**
 * This function is implemented in a separate file to speed up the
 * compilation if only the SVN revision changed.
 */

unsigned long VBoxSVNRev ()
{
    return VBOX_SVN_REV;
}
