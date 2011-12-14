/** @file
 *
 * VirtualBox 3D common tooling
 */

/*
 * Copyright (C) 2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef ___VBox_VBoxVideo3D_h
#define ___VBox_VBoxVideo3D_h

#include <iprt/cdefs.h>
#include <iprt/asm.h>
#ifndef VBoxTlsRefGetImpl
# ifdef VBoxTlsRefSetImpl
#  error "VBoxTlsRefSetImpl is defined, unexpected!"
# endif
# include <iprt/thread.h>
# define VBoxTlsRefGetImpl(_tls) (RTTlsGet((RTTLS)(_tls)))
# define VBoxTlsRefSetImpl(_tls, _val) (RTTlsSet((RTTLS)(_tls), (_val)))
#else
# ifndef VBoxTlsRefSetImpl
#  error "VBoxTlsRefSetImpl is NOT defined, unexpected!"
# endif
#endif

#ifndef VBoxTlsRefAssertImpl
# define VBoxTlsRefAssertImpl(_a) do {} while (0)
#endif

typedef DECLCALLBACK(void) FNVBOXTLSREFDTOR(void*);
typedef FNVBOXTLSREFDTOR *PFNVBOXTLSREFDTOR;

typedef enum {
    VBOXTLSREFDATA_STATE_UNDEFINED = 0,
    VBOXTLSREFDATA_STATE_INITIALIZED,
    VBOXTLSREFDATA_STATE_TOBE_DESTROYED,
    VBOXTLSREFDATA_STATE_DESTROYING,
    VBOXTLSREFDATA_STATE_32BIT_HACK = 0x7fffffff
} VBOXTLSREFDATA_STATE;

#define VBOXTLSREFDATA \
    volatile int32_t cTlsRefs; \
    uint32_t enmTlsRefState; \
    PFNVBOXTLSREFDTOR pfnTlsRefDtor; \

#define VBoxTlsRefInit(_p, _pfnDtor) do { \
        (_p)->cTlsRefs = 1; \
        (_p)->enmTlsRefState = VBOXTLSREFDATA_STATE_INITIALIZED; \
        (_p)->pfnTlsRefDtor = (_pfnDtor); \
    } while (0)

#define VBoxTlsRefIsFunctional(_p) (!!((_p)->enmTlsRefState == VBOXTLSREFDATA_STATE_INITIALIZED))

#define VBoxTlsRefAddRef(_p) do { \
        int cRefs = ASMAtomicIncS32(&(_p)->cTlsRefs); \
        VBoxTlsRefAssertImpl(cRefs > 1 || (_p)->enmTlsRefState == VBOXTLSREFDATA_STATE_DESTROYING); \
    } while (0)

#define VBoxTlsRefRelease(_p) do { \
        int cRefs = ASMAtomicDecS32(&(_p)->cTlsRefs); \
        VBoxTlsRefAssertImpl(cRefs >= 0); \
        if (!cRefs && (_p)->enmTlsRefState != VBOXTLSREFDATA_STATE_DESTROYING /* <- avoid recursion if VBoxTlsRefAddRef/Release is called from dtor */) { \
            (_p)->enmTlsRefState = VBOXTLSREFDATA_STATE_DESTROYING; \
            (_p)->pfnTlsRefDtor((_p)); \
        } \
    } while (0)

#define VBoxTlsRefReleaseMarkDestroy(_p) do { \
        (_p)->enmTlsRefState = VBOXTLSREFDATA_STATE_TOBE_DESTROYED; \
    } while (0)

#define VBoxTlsRefGetCurrent(_t, _Tsd) ((_t*) VBoxTlsRefGetImpl((_Tsd)))

#define VBoxTlsRefSetCurrent(_t, _Tsd, _p) do { \
        _t * oldCur = VBoxTlsRefGetCurrent(_t, _Tsd); \
        if (oldCur != (_p)) { \
            VBoxTlsRefSetImpl((_Tsd), (_p)); \
            if (oldCur) { \
                VBoxTlsRefRelease(oldCur); \
            } \
            if ((_p)) { \
                VBoxTlsRefAddRef((_t*)(_p)); \
            } \
        } \
    } while (0)

#endif /* #ifndef ___VBox_VBoxVideo3D_h */
