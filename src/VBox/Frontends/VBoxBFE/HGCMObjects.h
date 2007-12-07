/** @file
 *
 * HGCMObjects - Host-Guest Communication Manager objects header.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * innotek GmbH confidential
 * All rights reserved
 */

#ifndef __HGCMOBJECTS__H
#define __HGCMOBJECTS__H

#define LOG_GROUP_MAIN_OVERRIDE LOG_GROUP_HGCM
#include "Logging.h"

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

typedef enum
{
    HGCMOBJ_CLIENT,
    HGCMOBJ_THREAD,
    HGCMOBJ_MSG,
    HGCMOBJ_SizeHack   = 0x7fffffff
} HGCMOBJ_TYPE;

class HGCMObject
{
    private:
    friend uint32_t hgcmObjMake (HGCMObject *pObject, uint32_t u32HandleIn);

        int32_t volatile cRef;
        HGCMOBJ_TYPE     enmObjType;

        ObjectAVLCore Core;

    protected:
        virtual ~HGCMObject (void) {};

    public:
        HGCMObject (HGCMOBJ_TYPE enmObjType) : cRef (0)
        {
            this->enmObjType  = enmObjType;
        };

        void Reference (void)
        {
            int32_t refCnt = ASMAtomicIncS32 (&cRef);
            NOREF(refCnt);
            Log(("Reference: refCnt = %d\n", refCnt));
        }

        void Dereference (void)
        {
            int32_t refCnt = ASMAtomicDecS32 (&cRef);

            Log(("Dereference: refCnt = %d\n", refCnt));
            
            AssertRelease(refCnt >= 0);

            if (refCnt)
            {
                return;
            }

            delete this;
        }

        uint32_t Handle (void)
        {
            return Core.AvlCore.Key;
        };

        HGCMOBJ_TYPE Type (void)
        {
            return enmObjType;
        };
};

int hgcmObjInit (void);

void hgcmObjUninit (void);

uint32_t hgcmObjGenerateHandle (HGCMObject *pObject);
uint32_t hgcmObjAssignHandle (HGCMObject *pObject, uint32_t u32Handle);

void hgcmObjDeleteHandle (uint32_t handle);

HGCMObject *hgcmObjReference (uint32_t handle, HGCMOBJ_TYPE enmObjType);

void hgcmObjDereference (HGCMObject *pObject);

uint32_t hgcmObjQueryHandleCount ();
void     hgcmObjSetHandleCount (uint32_t u32HandleCount);

#endif /* __HGCMOBJECTS__H */
