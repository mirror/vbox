/* $Id$ */
/** @file
 * USB device vendor and product ID database - stub.
 */

/*
 * Copyright (C) 2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "USBIdDatabase.h"

Product const   AliasDictionary::aProducts[] = {0};
const size_t    AliasDictionary::cProducts   = 1; /* std::lower_bound cannot deal with empty array */
Vendor const    AliasDictionary::aVendors[]  = {0};
const size_t    AliasDictionary::cVendors    = 1; /* std::lower_bound cannot deal with empty array */

