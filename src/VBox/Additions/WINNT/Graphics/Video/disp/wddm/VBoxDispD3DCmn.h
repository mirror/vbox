/* $Id$ */
/** @file
 * VBoxVideo Display D3D User mode dll
 */

/*
 * Copyright (C) 2011-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_wddm_VBoxDispD3DCmn_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_wddm_VBoxDispD3DCmn_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "VBoxDispD3DBase.h"

#include <iprt/asm.h>
#include <iprt/initterm.h>
#include <iprt/log.h>
#include <iprt/mem.h>

#include <VBox/Log.h>

#include <VBox/VBoxGuestLib.h>

#include "VBoxDispDbg.h"
#include "VBoxDispD3DIf.h"
#include "../../common/wddm/VBoxMPIf.h"
#include "VBoxDispMpInternal.h"
#include <VBoxDispKmt.h>
#include "VBoxDispD3D.h"
#include "VBoxD3DIf.h"

# ifdef VBOXWDDMDISP
#  define VBOXWDDMDISP_DECL(_type) DECLEXPORT(_type)
# else
#  define VBOXWDDMDISP_DECL(_type) DECLIMPORT(_type)
# endif

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_wddm_VBoxDispD3DCmn_h */
