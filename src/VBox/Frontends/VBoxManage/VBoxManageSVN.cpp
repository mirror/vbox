/* $Id$ */
/** @file
 * VBox frontends: VBoxManage (command-line interface):
 * SVN revision.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#include <VBox/version.h>
#include "VBoxManage.h"

/**
 * Return the SVN revision number. We put this function into a separate file
 * to speed up compilation if the revision number changes. We don't put this
 * function into VBoxSVC to save the overhead of starting the server if only
 * the version number is requested.
 */
unsigned long VBoxSVNRev()
{
    return VBOX_SVN_REV;
}

