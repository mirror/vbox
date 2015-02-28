/* $Id$ */
/** @file
 * dump-vmwgfx.c - Dumps parameters and capabilities of vmwgfx.ko.
 */

/*
 * Copyright (C) 2013-2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>

/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#define DRM_IOCTL_BASE              'd'
#define DRM_COMMAND_BASE            0x40
#define DRM_VMW_GET_PARAM           0
#define DRM_VMW_GET_3D_CAP          13
#define DRM_IOCTL_VMW_GET_PARAM     _IOWR(DRM_IOCTL_BASE, DRM_COMMAND_BASE + DRM_VMW_GET_PARAM, struct drm_vmw_getparam_arg)
#define DRM_IOCTL_VMW_GET_3D_CAP    _IOW(DRM_IOCTL_BASE, DRM_COMMAND_BASE + DRM_VMW_GET_3D_CAP, struct drm_vmw_get_3d_cap_arg)

#define SVGA3DCAPS_RECORD_DEVCAPS   0x100

#define DRM_VMW_PARAM_NUM_STREAMS       0
#define DRM_VMW_PARAM_FREE_STREAMS      1
#define DRM_VMW_PARAM_3D                2
#define DRM_VMW_PARAM_HW_CAPS           3
#define DRM_VMW_PARAM_FIFO_CAPS         4
#define DRM_VMW_PARAM_MAX_FB_SIZE       5
#define DRM_VMW_PARAM_FIFO_HW_VERSION   6
#define DRM_VMW_PARAM_MAX_SURF_MEMORY   7
#define DRM_VMW_PARAM_3D_CAP_SIZE       8
#define DRM_VMW_PARAM_MAX_MOB_MEMORY    9
#define DRM_VMW_PARAM_MAX_MOB_SIZE     10

/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
struct drm_vmw_get_3d_cap_arg
{
    uint64_t buffer;
    uint32_t max_size;
    uint32_t pad64;
};


struct SVGA3dCapsRecordHeader
{
    uint32_t length;
    uint32_t type;
};

struct SVGA3dCapsRecord
{
    /* Skipped if DRM_VMW_PARAM_MAX_MOB_MEMORY is read. */
    struct SVGA3dCapsRecordHeader header;
    uint32_t data[1];    
};

struct drm_vmw_getparam_arg
{
    uint64_t value;
    uint32_t param;
    uint32_t pad64;
};


static int QueryParam(int fd, uint32_t uParam, const char *pszParam)
{
    struct drm_vmw_getparam_arg Arg = { 0, uParam, 0 };
    int rc = ioctl(fd, DRM_IOCTL_VMW_GET_PARAM, &Arg);
    if (rc >= 0)
        printf("%32s: %#llx (%lld)\n", pszParam, Arg.value, Arg.value);
    else
        printf("%32s: failed: rc=%d errno=%d (%s)\n", pszParam, rc, errno, strerror(errno));    
    return rc;
}


int main(int argc, char **argv)
{
    int rcExit = 1;
    const char *pszDev = "/dev/dri/card0";
    if (argc == 2)
        pszDev = argv[1];
    
    int fd = open(pszDev, O_RDWR);
    if (fd != -1)
    {
        /*
         * Parameters.
         */
#define QUERY_PARAM(nm) QueryParam(fd, nm, #nm)
        QUERY_PARAM(DRM_VMW_PARAM_NUM_STREAMS);
        QUERY_PARAM(DRM_VMW_PARAM_FREE_STREAMS);
        QUERY_PARAM(DRM_VMW_PARAM_3D);
        QUERY_PARAM(DRM_VMW_PARAM_HW_CAPS);
        QUERY_PARAM(DRM_VMW_PARAM_FIFO_CAPS);
        QUERY_PARAM(DRM_VMW_PARAM_MAX_FB_SIZE);
        QUERY_PARAM(DRM_VMW_PARAM_FIFO_HW_VERSION); 
        QUERY_PARAM(DRM_VMW_PARAM_MAX_SURF_MEMORY);
        QUERY_PARAM(DRM_VMW_PARAM_3D_CAP_SIZE);
        /*QUERY_PARAM(DRM_VMW_PARAM_MAX_MOB_MEMORY); -- changes 3d cap on success. */
        QUERY_PARAM(DRM_VMW_PARAM_MAX_MOB_SIZE);
    
        /*
         * 3D capabilities.
         */
        struct SVGA3dCapsRecord *pBuf = (struct SVGA3dCapsRecord *)calloc(1024, sizeof(uint32_t));
        struct drm_vmw_get_3d_cap_arg Caps3D = { (uintptr_t)pBuf, 1024 * sizeof(uint32_t), 0 }; 
        errno = 0;
        int rc = ioctl(fd, DRM_IOCTL_VMW_GET_3D_CAP, &Caps3D);
        if (rc >= 0)
        {
            struct SVGA3dCapsRecord *pCur = pBuf;
            printf("DRM_IOCTL_VMW_GET_3D_CAP: rc=%d\n", rc);
            for (;;)
            {
                printf("SVGA3dCapsRecordHeader: length=%#x (%d) type=%d\n", 
                       pCur->header.length, pCur->header.length, pCur->header.type);
                if (pCur->header.length == 0)
                    break;
                uint32_t i;
                for (i = 0; i < pCur->header.length - 2; i += 2)
                {
                    printf(" cap %#04x = %#010x (%d)\n", pCur->data[i], pCur->data[i + 1], pCur->data[i + 1]);
                }
                pCur = (struct SVGA3dCapsRecord *)((uint32_t *)pCur + pCur->header.length);
            }
            rcExit = 0;
        }
        else
            fprintf(stderr, "DRM_IOCTL_VMW_GET_3D_CAP failed: %d - %s\n", errno, strerror(errno));
    }
    else 
        fprintf(stderr, "error opening '%s': %d\n", pszDev, errno);
    
    
    return rcExit;
}
