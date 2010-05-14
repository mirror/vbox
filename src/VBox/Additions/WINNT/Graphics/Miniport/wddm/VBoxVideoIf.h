/** @file
 * Contains base definitions of constants & structures used
 * to control & perform rendering,
 * such as DMA commands types, allocation types, escape codes, etc.
 * used by both miniport & display drivers.
 *
 * The latter uses these and only these defs to communicate with the former
 * by posting appropriate requests via D3D RT Krnl Svc accessing callbacks.
 */
/*
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
#ifndef ___VBoxVideoIf_h___
#define ___VBoxVideoIf_h___

#include <VBox/VBoxVideo.h>

/* create allocation func */
typedef enum
{
    VBOXWDDM_ALLOC_TYPE_UNEFINED = 0,
    VBOXWDDM_ALLOC_TYPE_STD_SHAREDPRIMARYSURFACE,
    VBOXWDDM_ALLOC_TYPE_STD_SHADOWSURFACE,
    VBOXWDDM_ALLOC_TYPE_STD_STAGINGSURFACE,
    /* this one is win 7-specific and hence unused for now */
    VBOXWDDM_ALLOC_TYPE_STD_GDISURFACE
    /* custom allocation types requested from user-mode d3d module will go here */
} VBOXWDDM_ALLOC_TYPE;

typedef struct VBOXWDDM_SURFACE_DESC
{
    UINT width;
    UINT height;
    D3DDDIFORMAT format;
    UINT bpp;
    UINT pitch;
} VBOXWDDM_SURFACE_DESC, *PVBOXWDDM_SURFACE_DESC;

typedef struct VBOXWDDM_ALLOCINFO
{
    VBOXWDDM_ALLOC_TYPE enmType;
    union
    {
        VBOXWDDM_SURFACE_DESC SurfInfo;
    } u;
} VBOXWDDM_ALLOCINFO, *PVBOXWDDM_ALLOCINFO;

#define VBOXWDDM_ALLOCINFO_HEADSIZE() (sizeof (VBOXWDDM_ALLOCINFO))
#define VBOXWDDM_ALLOCINFO_SIZE_FROMBODYSIZE(_s) (VBOXWDDM_ALLOCINFO_HEADSIZE() + (_s))
#define VBOXWDDM_ALLOCINFO_SIZE(_tCmd) (VBOXWDDM_ALLOCINFO_SIZE_FROMBODYSIZE(sizeof(_tCmd)))
#define VBOXWDDM_ALLOCINFO_BODY(_p, _t) ( (_t*)(((uint8_t*)(_p)) + VBOXWDDM_ALLOCINFO_HEADSIZE()) )
#define VBOXWDDM_ALLOCINFO_HEAD(_pb) ((VBOXWDDM_ALLOCINFO*)((uint8_t *)(_pb) - VBOXWDDM_ALLOCATION_HEADSIZE()))

/* query info func */
typedef enum
{
    VBOXWDDM_QI_TYPE_UNEFINED = 0,
#ifdef VBOX_WITH_VIDEOHWACCEL
    /* VBOXVHWACMD_QUERYINFO1 */
    VBOXWDDM_QI_TYPE_2D_1,
    /* VBOXVHWACMD_QUERYINFO2 */
    VBOXWDDM_QI_TYPE_2D_2
#endif
} VBOXWDDM_QI_TYPE;

typedef struct VBOXWDDM_QI
{
    VBOXWDDM_QI_TYPE enmType;
    int rc;
} VBOXWDDM_QI;

typedef struct VBOXWDDM_QI_2D_1
{
    VBOXWDDM_QI hdr;
    VBOXVHWACMD_QUERYINFO1 Info;
} VBOXWDDM_QI_2D_1;

typedef struct VBOXWDDM_QI_2D_2
{
    VBOXWDDM_QI hdr;
    VBOXVHWACMD_QUERYINFO2 Info;
} VBOXWDDM_QI_2D_2;

/* submit cmd func */

#endif /* #ifndef ___VBoxVideoIf_h___ */
