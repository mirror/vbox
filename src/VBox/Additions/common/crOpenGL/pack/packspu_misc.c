/* Copyright (c) 2001, Stanford University
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#include "cr_packfunctions.h"
#include "packspu.h"
#include "packspu_proto.h"
#include "cr_mem.h"

void PACKSPU_APIENTRY packspu_ChromiumParametervCR(GLenum target, GLenum type, GLsizei count, const GLvoid *values)
{

    CRMessage msg;
    int len;
    
    GET_THREAD(thread);

    
    switch(target)
    {
        case GL_GATHER_PACK_CR:
            /* flush the current pack buffer */
            packspuFlush( (void *) thread );

            /* the connection is thread->server.conn */
            msg.header.type = CR_MESSAGE_GATHER;
            msg.gather.offset = 69;
            len = sizeof(CRMessageGather);
            crNetSend(thread->netServer.conn, NULL, &msg, len);
            break;
            
        default:
            if (pack_spu.swap)
                crPackChromiumParametervCRSWAP(target, type, count, values);
            else
                crPackChromiumParametervCR(target, type, count, values);
    }


}

GLboolean packspuSyncOnFlushes()
{
    GLint buffer;

    /*Seems to still cause issues, always sync for now*/
    return 1;

    crStateGetIntegerv(GL_DRAW_BUFFER, &buffer);
    /*Usually buffer==GL_BACK, so put this extra check to simplify boolean eval on runtime*/
    return  (buffer != GL_BACK)
            && (buffer == GL_FRONT_LEFT 
                || buffer == GL_FRONT_RIGHT
                || buffer == GL_FRONT
                || buffer == GL_FRONT_AND_BACK
                || buffer == GL_LEFT
                || buffer == GL_RIGHT);
}

void PACKSPU_APIENTRY packspu_DrawBuffer(GLenum mode)
{
    GLboolean hadtoflush;

    hadtoflush = packspuSyncOnFlushes();

    crStateDrawBuffer(mode);
    crPackDrawBuffer(mode);

    if (hadtoflush && !packspuSyncOnFlushes())
        packspu_Flush();
}

void PACKSPU_APIENTRY packspu_Finish( void )
{
    GET_THREAD(thread);
    GLint writeback = pack_spu.thread[pack_spu.idxThreadInUse].netServer.conn->actual_network;

    if (pack_spu.swap)
    {
        crPackFinishSWAP();
    }
    else
    {
        crPackFinish();
    }

    if (packspuSyncOnFlushes())
    {
        if (writeback)
        {
            if (pack_spu.swap)
                crPackWritebackSWAP(&writeback);
            else
                crPackWriteback(&writeback);

            packspuFlush( (void *) thread );

            while (writeback)
                crNetRecv();
        }
    }
}

#define PACK_FORCED_SYNC

void PACKSPU_APIENTRY packspu_Flush( void )
{
    GET_THREAD(thread);
    int writeback = 1;
    int found=0;

    if (!thread->bInjectThread)
    {
        crPackFlush();
        if (packspuSyncOnFlushes())
        {
            crPackWriteback(&writeback);
            packspuFlush( (void *) thread );
            while (writeback)
                crNetRecv();
        }
    }
    else
    {
        int i;

        crLockMutex(&_PackMutex);

        /*Make sure we process commands in order they should appear, so flush other threads first*/
        for (i=0; i<MAX_THREADS; ++i)
        {
            if (pack_spu.thread[i].inUse
                && (thread != &pack_spu.thread[i]) && pack_spu.thread[i].netServer.conn
                && pack_spu.thread[i].packer && pack_spu.thread[i].packer->currentBuffer)
            {
#ifdef PACK_FORCED_SYNC
            	CRPackContext *pc = pack_spu.thread[i].packer;
            	unsigned char *data_ptr;

            	CR_GET_BUFFERED_POINTER( pc, 16 );
            	WRITE_DATA( 0, GLint, 16 );
            	WRITE_DATA( 4, GLenum, CR_WRITEBACK_EXTEND_OPCODE );
            	WRITE_NETWORK_POINTER( 8, (void *) &writeback );
            	WRITE_OPCODE( pc, CR_EXTEND_OPCODE );
            	CR_UNLOCK_PACKER_CONTEXT(pc);
#endif
                packspuFlush((void *) &pack_spu.thread[i]);

                if (pack_spu.thread[i].netServer.conn->u32ClientID == thread->netServer.conn->u32InjectClientID)
                {
                    found=1;
                }

#ifdef PACK_FORCED_SYNC
                while (writeback)
                    crNetRecv();
#endif
            }
        }

        if (!found)
        {
            /*Thread we're supposed to inject commands for has been detached,
              so there's nothing to sync with and we should just pass commands through our own connection.
             */
            thread->netServer.conn->u32InjectClientID=0;
        }

#ifdef PACK_FORCED_SYNC
        writeback = 1;
        crPackWriteback(&writeback);
#endif
        packspuFlush((void *) thread);

#ifdef PACK_FORCED_SYNC
        while (writeback)
            crNetRecv();
#endif
        crUnlockMutex(&_PackMutex);
    }
}

GLint PACKSPU_APIENTRY packspu_WindowCreate( const char *dpyName, GLint visBits )
{
    GET_THREAD(thread);
    static int num_calls = 0;
    int writeback = pack_spu.thread[pack_spu.idxThreadInUse].netServer.conn->actual_network;
    GLint return_val = (GLint) 0;

    if (!thread) {
        thread = packspuNewThread( crThreadID() );
    }
    CRASSERT(thread);
    CRASSERT(thread->packer);

    crPackSetContext(thread->packer);

    if (pack_spu.swap)
    {
        crPackWindowCreateSWAP( dpyName, visBits, &return_val, &writeback );
    }
    else
    {
        crPackWindowCreate( dpyName, visBits, &return_val, &writeback );
    }
    packspuFlush(thread);
    if (!(thread->netServer.conn->actual_network))
    {
        return num_calls++;
    }
    else
    {
        while (writeback)
            crNetRecv();
        if (pack_spu.swap)
        {
            return_val = (GLint) SWAP32(return_val);
        }
        return return_val;
    }
}



GLboolean PACKSPU_APIENTRY
packspu_AreTexturesResident( GLsizei n, const GLuint * textures,
                                                         GLboolean * residences )
{
    GET_THREAD(thread);
    int writeback = 1;
    GLboolean return_val = GL_TRUE;
    GLsizei i;

    if (!(pack_spu.thread[pack_spu.idxThreadInUse].netServer.conn->actual_network))
    {
        crError( "packspu_AreTexturesResident doesn't work when there's no actual network involved!\nTry using the simplequery SPU in your chain!" );
    }

    if (pack_spu.swap)
    {
        crPackAreTexturesResidentSWAP( n, textures, residences, &return_val, &writeback );
    }
    else
    {
        crPackAreTexturesResident( n, textures, residences, &return_val, &writeback );
    }
    packspuFlush( (void *) thread );

    while (writeback)
        crNetRecv();

    /* Since the Chromium packer/unpacker can't return both 'residences'
     * and the function's return value, compute the return value here.
     */
    for (i = 0; i < n; i++) {
        if (!residences[i]) {
            return_val = GL_FALSE;
            break;
        }
    }

    return return_val;
}


GLboolean PACKSPU_APIENTRY
packspu_AreProgramsResidentNV( GLsizei n, const GLuint * ids,
                                                             GLboolean * residences )
{
    GET_THREAD(thread);
    int writeback = 1;
    GLboolean return_val = GL_TRUE;
    GLsizei i;

    if (!(pack_spu.thread[pack_spu.idxThreadInUse].netServer.conn->actual_network))
    {
        crError( "packspu_AreProgramsResidentNV doesn't work when there's no actual network involved!\nTry using the simplequery SPU in your chain!" );
    }
    if (pack_spu.swap)
    {
        crPackAreProgramsResidentNVSWAP( n, ids, residences, &return_val, &writeback );
    }
    else
    {
        crPackAreProgramsResidentNV( n, ids, residences, &return_val, &writeback );
    }
    packspuFlush( (void *) thread );

    while (writeback)
        crNetRecv();

    /* Since the Chromium packer/unpacker can't return both 'residences'
     * and the function's return value, compute the return value here.
     */
    for (i = 0; i < n; i++) {
        if (!residences[i]) {
            return_val = GL_FALSE;
            break;
        }
    }

    return return_val;
}

void PACKSPU_APIENTRY packspu_GetPolygonStipple( GLubyte * mask )
{
    GET_THREAD(thread);
    int writeback = 1;

    if (pack_spu.swap)
    {
        crPackGetPolygonStippleSWAP( mask, &writeback );
    }
    else
    {
        crPackGetPolygonStipple( mask, &writeback );
    }

#ifdef CR_ARB_pixel_buffer_object
    if (!crStateIsBufferBound(GL_PIXEL_PACK_BUFFER_ARB))
#endif
    {
        packspuFlush( (void *) thread );
        while (writeback)
            crNetRecv();
    }
}

void PACKSPU_APIENTRY packspu_GetPixelMapfv( GLenum map, GLfloat * values )
{
    GET_THREAD(thread);
    int writeback = 1;

    if (pack_spu.swap)
    {
        crPackGetPixelMapfvSWAP( map, values, &writeback );
    }
    else
    {
        crPackGetPixelMapfv( map, values, &writeback );
    }

#ifdef CR_ARB_pixel_buffer_object
    if (!crStateIsBufferBound(GL_PIXEL_PACK_BUFFER_ARB))
#endif
    {
        packspuFlush( (void *) thread );
        while (writeback)
            crNetRecv();
    }
}

void PACKSPU_APIENTRY packspu_GetPixelMapuiv( GLenum map, GLuint * values )
{
    GET_THREAD(thread);
    int writeback = 1;

    if (pack_spu.swap)
    {
        crPackGetPixelMapuivSWAP( map, values, &writeback );
    }
    else
    {
        crPackGetPixelMapuiv( map, values, &writeback );
    }

#ifdef CR_ARB_pixel_buffer_object
    if (!crStateIsBufferBound(GL_PIXEL_PACK_BUFFER_ARB))
#endif
    {
        packspuFlush( (void *) thread );
        while (writeback)
            crNetRecv();
    }
}

void PACKSPU_APIENTRY packspu_GetPixelMapusv( GLenum map, GLushort * values )
{
    GET_THREAD(thread);
    int writeback = 1;

    if (pack_spu.swap)
    {
        crPackGetPixelMapusvSWAP( map, values, &writeback );
    }
    else
    {
        crPackGetPixelMapusv( map, values, &writeback );
    }

#ifdef CR_ARB_pixel_buffer_object
    if (!crStateIsBufferBound(GL_PIXEL_PACK_BUFFER_ARB))
#endif
    {
        packspuFlush( (void *) thread );
        while (writeback)
            crNetRecv();
    }
}

static void packspuFluchOnThreadSwitch(GLboolean fEnable)
{
    GET_THREAD(thread);
    if (thread->currentContext->fAutoFlush == fEnable)
        return;

    thread->currentContext->fAutoFlush = fEnable;
    thread->currentContext->currentThread = fEnable ? thread : NULL;
}

void PACKSPU_APIENTRY packspu_ChromiumParameteriCR(GLenum target, GLint value)
{
    if (GL_FLUSH_ON_THREAD_SWITCH_CR==target)
    {
        /* this is a pure packspu state, don't propagate it any further */
        packspuFluchOnThreadSwitch(value);
        return;
    }
    if (GL_SHARE_CONTEXT_RESOURCES_CR==target)
    {
        crStateShareContext(value);
    }
    crPackChromiumParameteriCR(target, value);
}

#ifdef CHROMIUM_THREADSAFE
void PACKSPU_APIENTRY packspu_VBoxPackSetInjectThread(void)
{
    crLockMutex(&_PackMutex);
    {
        int i;
        GET_THREAD(thread);
        CRASSERT(!thread);
        CRASSERT((pack_spu.numThreads>0) && (pack_spu.numThreads<MAX_THREADS));

        for (i=0; i<MAX_THREADS; ++i)
        {
            if (!pack_spu.thread[i].inUse)
            {
                thread = &pack_spu.thread[i];
                break;
            }
        }
        CRASSERT(thread);

        thread->inUse = GL_TRUE;
        thread->id = crThreadID();
        thread->currentContext = NULL;
        thread->bInjectThread = GL_TRUE;

        thread->netServer.name = crStrdup(pack_spu.name);
        thread->netServer.buffer_size = 64 * 1024;

        crNetNewClient(pack_spu.thread[pack_spu.idxThreadInUse].netServer.conn, &(thread->netServer));
        CRASSERT(thread->netServer.conn);

        CRASSERT(thread->packer == NULL);
        thread->packer = crPackNewContext( pack_spu.swap );
        CRASSERT(thread->packer);
        crPackInitBuffer(&(thread->buffer), crNetAlloc(thread->netServer.conn),
                         thread->netServer.conn->buffer_size, thread->netServer.conn->mtu);
        thread->buffer.canBarf = thread->netServer.conn->Barf ? GL_TRUE : GL_FALSE;

        crPackSetBuffer( thread->packer, &thread->buffer );
        crPackFlushFunc( thread->packer, packspuFlush );
        crPackFlushArg( thread->packer, (void *) thread );
        crPackSendHugeFunc( thread->packer, packspuHuge );
        crPackSetContext( thread->packer );

        crSetTSD(&_PackTSD, thread);

        pack_spu.numThreads++;
    }
    crUnlockMutex(&_PackMutex);
}

GLuint PACKSPU_APIENTRY packspu_VBoxPackGetInjectID(void)
{
    GLuint ret;

    crLockMutex(&_PackMutex);
    {
        GET_THREAD(thread);
        CRASSERT(thread && thread->netServer.conn && thread->netServer.conn->type==CR_VBOXHGCM);
        ret = thread->netServer.conn->u32ClientID;
    }
    crUnlockMutex(&_PackMutex);

    return ret;
}

void PACKSPU_APIENTRY packspu_VBoxPackSetInjectID(GLuint id)
{
    crLockMutex(&_PackMutex);
    {
        GET_THREAD(thread);

        CRASSERT(thread && thread->netServer.conn && thread->netServer.conn->type==CR_VBOXHGCM && thread->bInjectThread);
        thread->netServer.conn->u32InjectClientID = id;
    }
    crUnlockMutex(&_PackMutex);
}

void PACKSPU_APIENTRY packspu_VBoxPackAttachThread()
{
#if 0
    int i;
    GET_THREAD(thread);

    for (i=0; i<MAX_THREADS; ++i)
    {
        if (pack_spu.thread[i].inUse && thread==&pack_spu.thread[i] && thread->id==crThreadID())
        {
            crError("2nd attach to same thread");
        }
    }
#endif

    crSetTSD(&_PackTSD, NULL);
}

void PACKSPU_APIENTRY packspu_VBoxPackDetachThread()
{
    int i;
    GET_THREAD(thread);

    if (thread)
    {
        crLockMutex(&_PackMutex);

        for (i=0; i<MAX_THREADS; ++i)
        {
            if (pack_spu.thread[i].inUse && thread==&pack_spu.thread[i]
                && thread->id==crThreadID() && thread->netServer.conn)
            {
                CRASSERT(pack_spu.numThreads>0);

                packspuFlush((void *) thread);

                if (pack_spu.thread[i].packer)
                {
                    CR_LOCK_PACKER_CONTEXT(thread->packer);
                    crPackSetContext(NULL);
                    CR_UNLOCK_PACKER_CONTEXT(thread->packer);
                    crPackDeleteContext(pack_spu.thread[i].packer);
                }
                crNetFreeConnection(pack_spu.thread[i].netServer.conn);

                pack_spu.numThreads--;
                /*note can't shift the array here, because other threads have TLS references to array elements*/
                crMemZero(&pack_spu.thread[i], sizeof(ThreadInfo));

                crSetTSD(&_PackTSD, NULL);

                if (i==pack_spu.idxThreadInUse)
                {
                    for (i=0; i<MAX_THREADS; ++i)
                    {
                        if (pack_spu.thread[i].inUse)
                        {
                            pack_spu.idxThreadInUse=i;
                            break;
                        }
                    }
                }

                break;
            }
        }

        for (i=0; i<CR_MAX_CONTEXTS; ++i)
        {
            ContextInfo *ctx = &pack_spu.context[i];
            if (ctx->currentThread == thread)
            {
                CRASSERT(ctx->fAutoFlush);
                ctx->currentThread = NULL;
            }
        }

        crUnlockMutex(&_PackMutex);
    }
}

#ifdef WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
BOOL WINAPI DllMain(HINSTANCE hDLLInst, DWORD fdwReason, LPVOID lpvReserved)
{
    (void) lpvReserved;

    switch (fdwReason) 
    {
        case DLL_PROCESS_ATTACH:
        {
            crInitMutex(&_PackMutex);
            break;
        }

        case DLL_PROCESS_DETACH:
        {
            crFreeMutex(&_PackMutex);
            crNetTearDown();
            break;
        }

        case DLL_THREAD_ATTACH:
        {
            packspu_VBoxPackAttachThread();
            break;
        }

        case DLL_THREAD_DETACH:
        {
            packspu_VBoxPackDetachThread();
            break;
        }

        default:
            break;
    }

    return TRUE;
}
#endif

#else  /*ifdef CHROMIUM_THREADSAFE*/
void PACKSPU_APIENTRY packspu_VBoxPackSetInjectThread(void)
{
}

GLuint PACKSPU_APIENTRY packspu_VBoxPackGetInjectID(void)
{
    return 0;
}

void PACKSPU_APIENTRY packspu_VBoxPackSetInjectID(GLuint id)
{
    (void) id;
}

void PACKSPU_APIENTRY packspu_VBoxPackAttachThread()
{
}

void PACKSPU_APIENTRY packspu_VBoxPackDetachThread()
{
}
#endif /*CHROMIUM_THREADSAFE*/
