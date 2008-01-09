/** @file
 *
 * Guest client: seamless mode
 * Proxy between the guest and host components
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
 */

#ifndef __Additions_client_seamless_glue_h
# define __Additions_client_seamless_glue_h

class VBoxGuestSeamlessObserver
{
public:
    virtual void notify(void) = 0;
    virtual ~VBoxGuestSeamlessObserver() {}
};

#endif /* __Additions_client_seamless_glue_h not defined */
