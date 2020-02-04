/*
 * Copyright (C) 2016-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/**********************************************************
 * Copyright 2009-2015 VMware, Inc.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 **********************************************************/


#include "vmw_screen.h"
#include "vmw_fence.h"
#include "vmw_context.h"

#include "util/u_memory.h"
#include "pipe/p_compiler.h"

#include "../wddm_screen.h"

/* Called from vmw_drm_create_screen(), creates and initializes the
 * vmw_winsys_screen structure, which is the main entity in this
 * module.
 * First, check whether a vmw_winsys_screen object already exists for
 * this device, and in that case return that one, making sure that we
 * have our own file descriptor open to DRM.
 */

struct vmw_winsys_screen_wddm *
vmw_winsys_create_wddm(const WDDMGalliumDriverEnv *pEnv)
{
   struct vmw_winsys_screen_wddm *vws;

   if (   pEnv->pHWInfo == NULL
       || pEnv->pHWInfo->u32HwType != VBOX_GA_HW_TYPE_VMSVGA)
       return NULL;

   vws = CALLOC_STRUCT(vmw_winsys_screen_wddm);
   if (!vws)
      goto out_no_vws;

   vws->pEnv = pEnv;
   vws->HwInfo = pEnv->pHWInfo->u.svga;

   vws->base.device = 0; /* not used */
   vws->base.open_count = 1;
   vws->base.ioctl.drm_fd = -1; /* not used */
   vws->base.base.have_gb_dma = TRUE;
   vws->base.base.need_to_rebind_resources = FALSE;

   if (!vmw_ioctl_init(&vws->base))
      goto out_no_ioctl;

   vws->base.fence_ops = vmw_fence_ops_create(&vws->base);
   if (!vws->base.fence_ops)
      goto out_no_fence_ops;

   if(!vmw_pools_init(&vws->base))
      goto out_no_pools;

   if (!vmw_winsys_screen_init_svga(&vws->base))
      goto out_no_svga;

   cnd_init(&vws->base.cs_cond);
   mtx_init(&vws->base.cs_mutex, mtx_plain);

   return vws;
out_no_svga:
   vmw_pools_cleanup(&vws->base);
out_no_pools:
   vws->base.fence_ops->destroy(vws->base.fence_ops);
out_no_fence_ops:
   vmw_ioctl_cleanup(&vws->base);
out_no_ioctl:
   FREE(vws);
out_no_vws:
   return NULL;
}

void
vmw_winsys_destroy(struct vmw_winsys_screen *vws)
{
   struct vmw_winsys_screen_wddm *vws_wddm = (struct vmw_winsys_screen_wddm *)vws;
   RT_NOREF(vws_wddm);
   if (--vws->open_count == 0) {
      vmw_pools_cleanup(vws);
      vws->fence_ops->destroy(vws->fence_ops);
      vmw_ioctl_cleanup(vws);
      mtx_destroy(&vws->cs_mutex);
      cnd_destroy(&vws->cs_cond);
      FREE(vws);
   }
}
