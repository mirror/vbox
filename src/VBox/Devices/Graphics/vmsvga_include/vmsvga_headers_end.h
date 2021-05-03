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

#if defined(__clang__)
# pragma clang diagnostic pop
#endif

#if RT_GNUC_PREREQ(4, 6)
# pragma GCC diagnostic pop
#endif

#ifndef VBOX_VMSVGA_HEADERS_BEING_INCLUDED
# error vmsvga_headers_begin was not included.
#else
# undef VBOX_VMSVGA_HEADERS_BEING_INCLUDED
#endif
