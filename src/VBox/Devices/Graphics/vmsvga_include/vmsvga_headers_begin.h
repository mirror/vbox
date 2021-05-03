/*
 * Copyright (C) 2021 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* GCC complains that 'ISO C++ prohibits anonymous structs' when "-Wpedantic" is enabled. */
#if RT_GNUC_PREREQ(4, 6)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wpedantic"
#endif

#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wpedantic"
# pragma clang diagnostic ignored "-Wc11-extensions"
#endif

#define VBOX_VMSVGA_HEADERS_BEING_INCLUDED
