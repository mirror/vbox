/* $Id$ */
/** @file
 * HGCMThread - Host-Guest Communication Manager worker threads header.
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___HGCMThread_h
#define ___HGCMThread_h

#include <VBox/types.h>

#include "HGCMObjects.h"

/* Forward declaration of the worker thread class. */
class HGCMThread;

/** A handle for HGCM message. */
typedef uint32_t HGCMMSGHANDLE;

/* Forward declaration of message core class. */
class HGCMMsgCore;

/** @todo comment */

typedef HGCMMsgCore *FNHGCMNEWMSGALLOC(uint32_t u32MsgId);
typedef FNHGCMNEWMSGALLOC *PFNHGCMNEWMSGALLOC;

/** Function that is called after message processing by worker thread,
 *  or if an error occurred during message handling after successfully
 *  posting (hgcmMsgPost) the message to worker thread.
 *
 * @param result    Return code either from the service which actually processed the message
 *                  or from HGCM.
 * @param pMsgCore  Pointer to just processed message.
 */
typedef DECLCALLBACK(void) HGCMMSGCALLBACK(int32_t result, HGCMMsgCore *pMsgCore);
typedef HGCMMSGCALLBACK *PHGCMMSGCALLBACK;


/** HGCM core message. */
class HGCMMsgCore : public HGCMReferencedObject
{
    private:
        friend class HGCMThread;

        /** Version of message header. */
        uint32_t m_u32Version;

        /** Message number/identifier. */
        uint32_t m_u32Msg;

        /** Thread the message belongs to, referenced by the message. */
        HGCMThread *m_pThread;

        /** Callback function pointer. */
        PHGCMMSGCALLBACK m_pfnCallback;

        /** Next element in a message queue. */
        HGCMMsgCore *m_pNext;
        /** Previous element in a message queue.
         *  @todo seems not necessary. */
        HGCMMsgCore *m_pPrev;

        /** Various internal flags. */
        uint32_t m_fu32Flags;

        /** Result code for a Send */
        int32_t m_rcSend;

    protected:
        void InitializeCore(uint32_t u32MsgId, HGCMThread *pThread);

        virtual ~HGCMMsgCore();

    public:
        HGCMMsgCore() : HGCMReferencedObject(HGCMOBJ_MSG) {};

        uint32_t MsgId(void) { return m_u32Msg; };

        HGCMThread *Thread(void) { return m_pThread; };

        /** Initialize message after it was allocated. */
        virtual void Initialize(void) {};

        /** Uninitialize message. */
        virtual void Uninitialize(void) {};
};


/** HGCM worker thread function.
 *
 *  @param pThread       The HGCM thread instance.
 *  @param pvUser        User specified thread parameter.
 */
typedef DECLCALLBACK(void) FNHGCMTHREAD(HGCMThread *pThread, void *pvUser);
typedef FNHGCMTHREAD *PFNHGCMTHREAD;


/**
 * Thread API.
 * Based on thread handles. Internals of a thread are not exposed to users.
 */

/** Initialize threads.
 *
 * @return VBox error code
 */
int hgcmThreadInit(void);
void hgcmThreadUninit(void);


/** Create a HGCM worker thread.
 *
 * @param ppThread          Where to return the pointer to the worker thread.
 * @param pszThreadName     Name of the thread, needed by runtime.
 * @param pfnThread         The worker thread function.
 * @param pvUser            A pointer passed to worker thread.
 * @param pszStatsSubDir    The "sub-directory" under "/HGCM/" where thread
 *                          statistics should be registered.  The caller,
 *                          HGCMService, will deregister them.  NULL if no stats.
 * @param pUVM              The user mode VM handle to register statistics with.
 *                          NULL if no stats.
 *
 * @return VBox error code
 */
int hgcmThreadCreate(HGCMThread **ppThread, const char *pszThreadName, PFNHGCMTHREAD pfnThread, void *pvUser,
                     const char *pszStatsSubDir, PUVM pUVM);

/** Wait for termination of a HGCM worker thread.
 *
 * @param pThread       The HGCM thread.  The passed in reference is always
 *                      consumed.
 *
 * @return VBox error code
 */
int hgcmThreadWait(HGCMThread *pThread);

/** Allocate a message to be posted to HGCM worker thread.
 *
 * @param pThread       The HGCM worker thread.
 * @param ppHandle      Where to store the pointer to the new message.
 * @param u32MsgId      Message identifier.
 * @param pfnNewMessage New message allocation callback.
 *
 * @return VBox error code
 */
int hgcmMsgAlloc(HGCMThread *pThread, HGCMMsgCore **ppHandle, uint32_t u32MsgId, PFNHGCMNEWMSGALLOC pfnNewMessage);

/** Post a message to HGCM worker thread.
 *
 * @param pMsg          The message.  Reference will be consumed!
 * @param pfnCallback   Message completion callback.
 *
 * @return VBox error code
 * @retval VINF_HGCM_ASYNC_EXECUTE on success.
 *
 * @thread any
 */
int hgcmMsgPost(HGCMMsgCore *pMsg, PHGCMMSGCALLBACK pfnCallback);

/** Send a message to HGCM worker thread.
 *
 *  The function will return after message is processed by thread.
 *
 * @param pMsg          The message.  Reference will be consumed!
 *
 * @return VBox error code
 *
 * @thread any
 */
int hgcmMsgSend(HGCMMsgCore *pMsg);


/* Wait for and get a message.
 *
 * @param pThread       The HGCM worker thread.
 * @param ppMsg         Where to store returned message pointer.
 *
 * @return VBox error code
 *
 * @thread worker thread
 */
int hgcmMsgGet(HGCMThread *pThread, HGCMMsgCore **ppMsg);


/** Worker thread has processed a message previously obtained with hgcmMsgGet.
 *
 * @param pMsg          Processed message pointer.
 * @param result        Result code, VBox erro code.
 *
 * @return VBox error code
 *
 * @thread worker thread
 */
void hgcmMsgComplete(HGCMMsgCore *pMsg, int32_t result);


#endif /* !___HGCMThread_h */

