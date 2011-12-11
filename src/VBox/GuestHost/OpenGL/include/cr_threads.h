/* Copyright (c) 2001, Stanford University
 * All rights reserved.
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */
	
#ifndef CR_THREADS_H
#define CR_THREADS_H

#include <iprt/cdefs.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "chromium.h"
#include "cr_bits.h"

#ifdef WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <pthread.h>
#include <semaphore.h>
#endif

#include <iprt/asm.h>
/*
 * Handle for Thread-Specific Data
 */
typedef struct {
#ifdef WINDOWS
	DWORD key;
#else
	pthread_key_t key;
#endif
	int initMagic;
} CRtsd;


extern DECLEXPORT(void) crInitTSD(CRtsd *tsd);
extern DECLEXPORT(void) crInitTSDF(CRtsd *tsd, void (*destructor)(void *));
extern DECLEXPORT(void) crFreeTSD(CRtsd *tsd);
extern DECLEXPORT(void) crSetTSD(CRtsd *tsd, void *ptr);
extern DECLEXPORT(void *) crGetTSD(CRtsd *tsd);
extern DECLEXPORT(unsigned long) crThreadID(void);


/* Mutex datatype */
#ifdef WINDOWS
typedef CRITICAL_SECTION CRmutex;
#else
typedef pthread_mutex_t CRmutex;
#endif

extern DECLEXPORT(void) crInitMutex(CRmutex *mutex);
extern DECLEXPORT(void) crFreeMutex(CRmutex *mutex);
extern DECLEXPORT(void) crLockMutex(CRmutex *mutex);
extern DECLEXPORT(void) crUnlockMutex(CRmutex *mutex);


/* Condition variable datatype */
#ifdef WINDOWS
typedef int CRcondition;
#else
typedef pthread_cond_t CRcondition;
#endif

extern DECLEXPORT(void) crInitCondition(CRcondition *cond);
extern DECLEXPORT(void) crFreeCondition(CRcondition *cond);
extern DECLEXPORT(void) crWaitCondition(CRcondition *cond, CRmutex *mutex);
extern DECLEXPORT(void) crSignalCondition(CRcondition *cond);


/* Barrier datatype */
typedef struct {
	unsigned int count;
#ifdef WINDOWS
	HANDLE hEvents[CR_MAX_CONTEXTS];
#else
	unsigned int waiting;
	pthread_cond_t cond;
	pthread_mutex_t mutex;
#endif
} CRbarrier;

extern DECLEXPORT(void) crInitBarrier(CRbarrier *b, unsigned int count);
extern DECLEXPORT(void) crFreeBarrier(CRbarrier *b);
extern DECLEXPORT(void) crWaitBarrier(CRbarrier *b);


/* Semaphores */
#ifdef WINDOWS
	typedef int CRsemaphore;
#else
	typedef sem_t CRsemaphore;
#endif

extern DECLEXPORT(void) crInitSemaphore(CRsemaphore *s, unsigned int count);
extern DECLEXPORT(void) crWaitSemaphore(CRsemaphore *s);
extern DECLEXPORT(void) crSignalSemaphore(CRsemaphore *s);

typedef DECLCALLBACK(void) FNCRTSDREFDTOR(void*);
typedef FNCRTSDREFDTOR *PFNCRTSDREFDTOR;

typedef enum {
    CRTSDREFDATA_STATE_UNDEFINED = 0,
    CRTSDREFDATA_STATE_INITIALIZED,
    CRTSDREFDATA_STATE_TOBE_DESTROYED,
    CRTSDREFDATA_STATE_DESTROYING,
    CRTSDREFDATA_STATE_32BIT_HACK = 0x7fffffff
} CRTSDREFDATA_STATE;

#define CRTSDREFDATA \
    volatile int32_t cTsdRefs; \
    uint32_t enmTsdRefState; \
    PFNCRTSDREFDTOR pfnTsdRefDtor; \

#define crTSDRefInit(_p, _pfnDtor) do { \
        (_p)->cTsdRefs = 1; \
        (_p)->enmTsdRefState = CRTSDREFDATA_STATE_INITIALIZED; \
        (_p)->pfnTsdRefDtor = (_pfnDtor); \
    } while (0)

#define crTSDRefIsFunctional(_p) (!!((_p)->enmTsdRefState == CRTSDREFDATA_STATE_INITIALIZED))

#define crTSDRefAddRef(_p) do { \
        int cRefs = ASMAtomicIncS32(&(_p)->cTsdRefs); \
        CRASSERT(cRefs > 1 || (_p)->enmTsdRefState == CRTSDREFDATA_STATE_DESTROYING); \
    } while (0)

#define crTSDRefRelease(_p) do { \
        int cRefs = ASMAtomicDecS32(&(_p)->cTsdRefs); \
        CRASSERT(cRefs >= 0); \
        if (!cRefs && (_p)->enmTsdRefState != CRTSDREFDATA_STATE_DESTROYING /* <- avoid recursion if crTSDRefAddRef/Release is called from dtor */) { \
            (_p)->enmTsdRefState = CRTSDREFDATA_STATE_DESTROYING; \
            (_p)->pfnTsdRefDtor((_p)); \
        } \
    } while (0)

#define crTSDRefReleaseMarkDestroy(_p) do { \
        (_p)->enmTsdRefState = CRTSDREFDATA_STATE_TOBE_DESTROYED; \
    } while (0)

#define crTSDRefGetCurrent(_t, _pTsd) ((_t*) crGetTSD((_pTsd)))

#define crTSDRefSetCurrent(_t, _pTsd, _p) do { \
        _t * oldCur = crTSDRefGetCurrent(_t, _pTsd); \
        if (oldCur != (_p)) { \
            crSetTSD((_pTsd), (_p)); \
            if (oldCur) { \
                crTSDRefRelease(oldCur); \
            } \
            if ((_p)) { \
                crTSDRefAddRef((_t*)(_p)); \
            } \
        } \
    } while (0)
#ifdef __cplusplus
}
#endif

#endif /* CR_THREADS_H */
