/** @file
 *
 * HGCMObjects - Host-Guest Communication Manager objects header.
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#ifndef __HGCMOBJECTS__H
#define __HGCMOBJECTS__H

#include <iprt/assert.h>
#include <iprt/avl.h>
#include <iprt/critsect.h>
#include <iprt/asm.h>

class HGCMObject;

typedef struct _ObjectAVLCore
{
    AVLULNODECORE AvlCore;
    HGCMObject *pSelf;
} ObjectAVLCore;

class HGCMObject
{
    private:
    friend uint32_t hgcmObjGenerateHandle (HGCMObject *pObject);

        int32_t volatile cRef;

        ObjectAVLCore Core;

        virtual bool Reuse (void) { return false; };

    protected:
        virtual ~HGCMObject (void) {};

    public:
        HGCMObject () : cRef (0) {};

        void Reference (void)
        {
            ASMAtomicIncS32 (&cRef);
        }

        void Dereference (void)
        {
            int32_t refCnt = ASMAtomicDecS32 (&cRef);

            AssertRelease(refCnt >= 0);

            if (refCnt)
            {
                return;
            }

            if (!Reuse ())
            {
                delete this;
            }
        }

        uint32_t Handle (void)
        {
            return Core.AvlCore.Key;
        };
};

int hgcmObjInit (void);

void hgcmObjUninit (void);

uint32_t hgcmObjGenerateHandle (HGCMObject *pObject);

void hgcmObjDeleteHandle (uint32_t handle);

HGCMObject *hgcmObjReference (uint32_t handle);

void hgcmObjDereference (HGCMObject *pObject);

#endif /* __HGCMOBJECTS__H */
