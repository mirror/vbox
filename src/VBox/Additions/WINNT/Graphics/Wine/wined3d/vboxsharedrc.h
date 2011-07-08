/* $Id$ */
/** @file
 *
 * VBox extension to Wine D3D
 *
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#ifndef ___vboxsharedrc_h___
#define ___vboxsharedrc_h___

#define VBOXSHRC_F_SHARED              0x00000001 /* shared rc */
#define VBOXSHRC_F_SHARED_OPENED       0x00000002 /* if set shared rc is opened, otherwise it is created */
#define VBOXSHRC_F_INITIALIZED         0x00000004 /* set once shared rc is initialized */

#define VBOXSHRC_GET_SHAREFLAFS(_o) ((_o)->resource.sharerc_flags)
#define VBOXSHRC_GET_SHAREHANDLE(_o) ((_o)->resource.sharerc_handle)
#define VBOXSHRC_SET_SHAREHANDLE(_o, _h) ((_o)->resource.sharerc_handle = (_h))
#define VBOXSHRC_SET_INITIALIZED(_o) (VBOXSHRC_GET_SHAREFLAFS(_o) |= VBOXSHRC_F_INITIALIZED)
#define VBOXSHRC_COPY_SHAREDATA(_oDst, _oSrc) do { \
        VBOXSHRC_GET_SHAREFLAFS(_oDst) = VBOXSHRC_GET_SHAREFLAFS(_oSrc); \
        VBOXSHRC_SET_SHAREHANDLE(_oDst, VBOXSHRC_GET_SHAREFLAFS(_oSrc)); \
    } while (0)
#define VBOXSHRC_SET_SHARED(_o) (VBOXSHRC_GET_SHAREFLAFS(_o) |= VBOXSHRC_F_SHARED)
#define VBOXSHRC_SET_SHARED_OPENED(_o) (VBOXSHRC_GET_SHAREFLAFS(_o) |= VBOXSHRC_F_SHARED_OPENED)
#define VBOXSHRC_IS_SHARED(_o) (!!(VBOXSHRC_GET_SHAREFLAFS(_o) & VBOXSHRC_F_SHARED))
#define VBOXSHRC_IS_SHARED_OPENED(_o) (!!(VBOXSHRC_GET_SHAREFLAFS(_o) & VBOXSHRC_F_SHARED_OPENED))
#define VBOXSHRC_IS_INITIALIZED(_o) (!!(VBOXSHRC_GET_SHAREFLAFS(_o) & VBOXSHRC_F_INITIALIZED))

#ifdef VBOX_WINE_WITH_IPRT
# include <iprt/assert.h>
#else
# define AssertBreakpoint() do { } while (0)
# define Assert(_expr) do { } while (0)
#endif


#endif /* #ifndef ___vboxsharedrc_h___ */
