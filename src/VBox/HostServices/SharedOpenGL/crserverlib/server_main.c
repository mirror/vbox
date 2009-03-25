/* Copyright (c) 2001, Stanford University
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#include "server.h"
#include "cr_net.h"
#include "cr_unpack.h"
#include "cr_error.h"
#include "cr_glstate.h"
#include "cr_string.h"
#include "cr_mem.h"
#include "cr_hash.h"
#include "server_dispatch.h"
#include "state/cr_texture.h"
#include <signal.h>
#include <stdlib.h>
#define DEBUG_FP_EXCEPTIONS 0
#if DEBUG_FP_EXCEPTIONS
#include <fpu_control.h>
#include <math.h>
#endif
#include <iprt/assert.h>

/**
 * \mainpage CrServerLib 
 *
 * \section CrServerLibIntroduction Introduction
 *
 * Chromium consists of all the top-level files in the cr
 * directory.  The core module basically takes care of API dispatch,
 * and OpenGL state management.
 */


/**
 * CRServer global data
 */
CRServer cr_server;

int tearingdown = 0; /* can't be static */


/**
 * Return pointer to server's first SPU.
 */
SPU*
crServerHeadSPU(void)
{
     return cr_server.head_spu;
}



static void DeleteBarrierCallback( void *data )
{
    CRServerBarrier *barrier = (CRServerBarrier *) data;
    crFree(barrier->waiting);
    crFree(barrier);
}


static void deleteContextCallback( void *data )
{
    CRContext *c = (CRContext *) data;
    crStateDestroyContext(c);
}


static void crServerTearDown( void )
{
    GLint i;

    /* avoid a race condition */
    if (tearingdown)
        return;

    tearingdown = 1;

    crStateSetCurrent( NULL );

    cr_server.curClient = NULL;
    cr_server.run_queue = NULL;

    crFree( cr_server.overlap_intens );
    cr_server.overlap_intens = NULL;

    /* Deallocate all semaphores */
    crFreeHashtable(cr_server.semaphores, crFree);
    cr_server.semaphores = NULL;
 
    /* Deallocate all barriers */
    crFreeHashtable(cr_server.barriers, DeleteBarrierCallback);
    cr_server.barriers = NULL;

    /* Free all context info */
    crFreeHashtable(cr_server.contextTable, deleteContextCallback);

    /* Free context/window creation info */
    crFreeHashtable(cr_server.pContextCreateInfoTable, crServerCreateInfoDeleteCB);
    crFreeHashtable(cr_server.pWindowCreateInfoTable, crServerCreateInfoDeleteCB);

    /* Free vertex programs */
    crFreeHashtable(cr_server.programTable, crFree);

    for (i = 0; i < cr_server.numClients; i++) {
        if (cr_server.clients[i]) {
            CRConnection *conn = cr_server.clients[i]->conn;
            crNetFreeConnection(conn);
            crFree(cr_server.clients[i]);
        }
    }
    cr_server.numClients = 0;

#if 1
    /* disable these two lines if trying to get stack traces with valgrind */
    crSPUUnloadChain(cr_server.head_spu);
    cr_server.head_spu = NULL;
#endif

    crUnloadOpenGL();
}

static void crServerClose( unsigned int id )
{
    crError( "Client disconnected!" );
    (void) id;
}

static void crServerCleanup( int sigio )
{
    crServerTearDown();

    tearingdown = 0;
}


void
crServerSetPort(int port)
{
    cr_server.tcpip_port = port;
}



static void
crPrintHelp(void)
{
    printf("Usage: crserver [OPTIONS]\n");
    printf("Options:\n");
    printf("  -mothership URL  Specifies URL for contacting the mothership.\n");
    printf("                   URL is of the form [protocol://]hostname[:port]\n");
    printf("  -port N          Specifies the port number this server will listen to.\n");
    printf("  -help            Prints this information.\n");
}


/**
 * Do CRServer initializations.  After this, we can begin servicing clients.
 */
void
crServerInit(int argc, char *argv[])
{
    int i;
    char *mothership = NULL;
    CRMuralInfo *defaultMural;

    for (i = 1 ; i < argc ; i++)
    {
        if (!crStrcmp( argv[i], "-mothership" ))
        {
            if (i == argc - 1)
            {
                crError( "-mothership requires an argument" );
            }
            mothership = argv[i+1];
            i++;
        }
        else if (!crStrcmp( argv[i], "-port" ))
        {
            /* This is the port on which we'll accept client connections */
            if (i == argc - 1)
            {
                crError( "-port requires an argument" );
            }
            cr_server.tcpip_port = crStrToInt(argv[i+1]);
            i++;
        }
        else if (!crStrcmp( argv[i], "-vncmode" ))
        {
            cr_server.vncMode = 1;
        }
        else if (!crStrcmp( argv[i], "-help" ))
        {
            crPrintHelp();
            exit(0);
        }
    }

    signal( SIGTERM, crServerCleanup );
    signal( SIGINT, crServerCleanup );
#ifndef WINDOWS
    signal( SIGPIPE, SIG_IGN );
#endif

#if DEBUG_FP_EXCEPTIONS
    {
        fpu_control_t mask;
        _FPU_GETCW(mask);
        mask &= ~(_FPU_MASK_IM | _FPU_MASK_DM | _FPU_MASK_ZM
                            | _FPU_MASK_OM | _FPU_MASK_UM);
        _FPU_SETCW(mask);
    }
#endif

    cr_server.firstCallCreateContext = GL_TRUE;
    cr_server.firstCallMakeCurrent = GL_TRUE;

    /*
     * Create default mural info and hash table.
     */
    cr_server.muralTable = crAllocHashtable();
    defaultMural = (CRMuralInfo *) crCalloc(sizeof(CRMuralInfo));
    crHashtableAdd(cr_server.muralTable, 0, defaultMural);

    cr_server.programTable = crAllocHashtable();

    crNetInit(crServerRecv, crServerClose);
    crStateInit();

    crServerSetVBoxConfiguration();

    crStateLimitsInit( &(cr_server.limits) );

    /*
     * Default context
     */
    cr_server.contextTable = crAllocHashtable();
    cr_server.DummyContext = crStateCreateContext( &cr_server.limits,
                                                   CR_RGB_BIT | CR_DEPTH_BIT, NULL );
    cr_server.curClient->currentCtx = cr_server.DummyContext;

    crServerInitDispatch();
    crStateDiffAPI( &(cr_server.head_spu->dispatch_table) );

    crUnpackSetReturnPointer( &(cr_server.return_ptr) );
    crUnpackSetWritebackPointer( &(cr_server.writeback_ptr) );

    cr_server.barriers = crAllocHashtable();
    cr_server.semaphores = crAllocHashtable();
}

void crVBoxServerTearDown(void)
{
    crServerTearDown();
}

/**
 * Do CRServer initializations.  After this, we can begin servicing clients.
 */
GLboolean crVBoxServerInit(void)
{
    CRMuralInfo *defaultMural;

#if DEBUG_FP_EXCEPTIONS
    {
        fpu_control_t mask;
        _FPU_GETCW(mask);
        mask &= ~(_FPU_MASK_IM | _FPU_MASK_DM | _FPU_MASK_ZM
                            | _FPU_MASK_OM | _FPU_MASK_UM);
        _FPU_SETCW(mask);
    }
#endif

    crNetInit(crServerRecv, crServerClose);

    cr_server.firstCallCreateContext = GL_TRUE;
    cr_server.firstCallMakeCurrent = GL_TRUE;

    cr_server.bIsInLoadingState = GL_FALSE;
    cr_server.bIsInSavingState  = GL_FALSE;

    /*
     * Create default mural info and hash table.
     */
    cr_server.muralTable = crAllocHashtable();
    defaultMural = (CRMuralInfo *) crCalloc(sizeof(CRMuralInfo));
    crHashtableAdd(cr_server.muralTable, 0, defaultMural);

    cr_server.programTable = crAllocHashtable();

    crStateInit();

    crStateLimitsInit( &(cr_server.limits) );

    cr_server.barriers = crAllocHashtable();
    cr_server.semaphores = crAllocHashtable();

    crUnpackSetReturnPointer( &(cr_server.return_ptr) );
    crUnpackSetWritebackPointer( &(cr_server.writeback_ptr) );

    /*
     * Default context
     */
    cr_server.contextTable = crAllocHashtable();
    cr_server.DummyContext = crStateCreateContext( &cr_server.limits,
                                                   CR_RGB_BIT | CR_DEPTH_BIT, NULL );
    cr_server.pContextCreateInfoTable = crAllocHashtable();
    cr_server.pWindowCreateInfoTable = crAllocHashtable();

    crServerSetVBoxConfigurationHGCM();

    if (!cr_server.head_spu)
        return GL_FALSE;

    crServerInitDispatch();
    crStateDiffAPI( &(cr_server.head_spu->dispatch_table) );

    return GL_TRUE;
}

void crVBoxServerAddClient(uint32_t u32ClientID)
{
    CRClient *newClient = (CRClient *) crCalloc(sizeof(CRClient));
    
    crDebug("crServer: AddClient u32ClientID=%d", u32ClientID);

    newClient->spu_id = 0;
    newClient->currentCtx = cr_server.DummyContext;
    newClient->currentContextNumber = -1;
    newClient->conn = crNetAcceptClient(cr_server.protocol, NULL,
                                        cr_server.tcpip_port,
                                        cr_server.mtu, 0);
    newClient->conn->u32ClientID = u32ClientID;

    cr_server.clients[cr_server.numClients++] = newClient;

    crServerAddToRunQueue(newClient);
}

void crVBoxServerRemoveClient(uint32_t u32ClientID)
{
    CRClient *pClient;
    int32_t i;

    crDebug("crServer: RemoveClient u32ClientID=%d", u32ClientID);

    for (i = 0; i < cr_server.numClients; i++)
    {
        if (cr_server.clients[i] && cr_server.clients[i]->conn 
            && cr_server.clients[i]->conn->u32ClientID==u32ClientID) 
        {
            break;
        }
    }
    pClient = cr_server.clients[i];
    CRASSERT(pClient);

    /* Disconnect the client */
    pClient->conn->Disconnect(pClient->conn);

    /* Let server clear client from the queue */
    crServerDeleteClient(pClient);
}

void crVBoxServerClientWrite(uint32_t u32ClientID, uint8_t *pBuffer, uint32_t cbBuffer)
{
    CRClient *pClient;
    int32_t i;

    //crDebug("crServer: [%x] ClientWrite u32ClientID=%d", crThreadID(), u32ClientID);

    for (i = 0; i < cr_server.numClients; i++)
    {
        if (cr_server.clients[i] && cr_server.clients[i]->conn 
            && cr_server.clients[i]->conn->u32ClientID==u32ClientID) 
        {
            break;
        }
    }
    pClient = cr_server.clients[i];
    CRASSERT(pClient);

    CRASSERT(pBuffer);

    /* This should never fire unless we start to multithread */
    CRASSERT(pClient->conn->pBuffer==NULL && pClient->conn->cbBuffer==0);

    /* Check if there's a blocker in queue and it's not this client */
    if (cr_server.run_queue->client != pClient
        && crServerClientInBeginEnd(cr_server.run_queue->client))
    {
        crDebug("crServer: client %d blocked, allow_redir_ptr = 0", u32ClientID);
        pClient->conn->allow_redir_ptr = 0;
    }
    else    
    {
        pClient->conn->allow_redir_ptr = 1;
    }
    
    pClient->conn->pBuffer = pBuffer;
    pClient->conn->cbBuffer = cbBuffer;

    crNetRecv();
    CRASSERT(pClient->conn->pBuffer==NULL && pClient->conn->cbBuffer==0);

    crServerServiceClients();

#if 0
        if (pClient->currentMural) {
            crStateViewport( 0, 0, 500, 500 );
            pClient->currentMural->viewportValidated = GL_FALSE;
            cr_server.head_spu->dispatch_table.Viewport( 0, 0, 500, 500 );
            crStateViewport( 0, 0, 600, 600 );
            pClient->currentMural->viewportValidated = GL_FALSE;
            cr_server.head_spu->dispatch_table.Viewport( 0, 0, 600, 600 );

            crStateMatrixMode(GL_PROJECTION);
            cr_server.head_spu->dispatch_table.MatrixMode(GL_PROJECTION);
            crServerDispatchLoadIdentity();
            crStateFrustum(-0.6, 0.6, -0.5, 0.5, 1.5, 150.0);
            cr_server.head_spu->dispatch_table.Frustum(-0.6, 0.6, -0.5, 0.5, 1.5, 150.0);
            crServerDispatchLoadIdentity();
            crStateFrustum(-0.5, 0.5, -0.5, 0.5, 1.5, 150.0);
            cr_server.head_spu->dispatch_table.Frustum(-0.5, 0.5, -0.5, 0.5, 1.5, 150.0);

            crStateMatrixMode(GL_MODELVIEW);
            cr_server.head_spu->dispatch_table.MatrixMode(GL_MODELVIEW);
            crServerDispatchLoadIdentity();
            crStateFrustum(-0.5, 0.5, -0.5, 0.5, 1.5, 150.0);
            cr_server.head_spu->dispatch_table.Frustum(-0.5, 0.5, -0.5, 0.5, 1.5, 150.0);
            crServerDispatchLoadIdentity();
        }
#endif

    crStateResetCurrentPointers(&cr_server.current);

    CRASSERT(!pClient->conn->allow_redir_ptr || crNetNumMessages(pClient->conn)==0);
}

int32_t crVBoxServerClientRead(uint32_t u32ClientID, uint8_t *pBuffer, uint32_t *pcbBuffer)
{
    CRClient *pClient;
    int32_t i;

    //crDebug("crServer: [%x] ClientRead u32ClientID=%d", crThreadID(), u32ClientID);

    for (i = 0; i < cr_server.numClients; i++)
    {
        if (cr_server.clients[i] && cr_server.clients[i]->conn 
            && cr_server.clients[i]->conn->u32ClientID==u32ClientID) 
        {
            break;
        }
    }
    pClient = cr_server.clients[i];
    CRASSERT(pClient);

    if (pClient->conn->cbHostBuffer > *pcbBuffer)
    {
        crWarning("crServer: [%lx] ClientRead u32ClientID=%d FAIL, host buffer too small %d of %d",
                  crThreadID(), u32ClientID, *pcbBuffer, pClient->conn->cbHostBuffer);

        /* Return the size of needed buffer */
        *pcbBuffer = pClient->conn->cbHostBuffer;

        return VERR_BUFFER_OVERFLOW;
    }

    *pcbBuffer = pClient->conn->cbHostBuffer;

    if (*pcbBuffer)
    {
        CRASSERT(pClient->conn->pHostBuffer);

        crMemcpy(pBuffer, pClient->conn->pHostBuffer, *pcbBuffer);
        pClient->conn->cbHostBuffer = 0;
    }
    
    return VINF_SUCCESS;
}

int
CRServerMain(int argc, char *argv[])
{
    crServerInit(argc, argv);

    crServerSerializeRemoteStreams();

    crServerTearDown();

    tearingdown = 0;

    return 0;
}

static void crVBoxServerSaveMuralCB(unsigned long key, void *data1, void *data2)
{
    CRMuralInfo *pMI = (CRMuralInfo*) data1;
    PSSMHANDLE pSSM = (PSSMHANDLE) data2;
    int32_t rc;

    CRASSERT(pMI && pSSM);

    /* Don't store default mural */
    if (!key) return;

    rc = SSMR3PutMem(pSSM, &key, sizeof(key));
    CRASSERT(rc == VINF_SUCCESS);

    rc = SSMR3PutMem(pSSM, pMI, sizeof(*pMI));
    CRASSERT(rc == VINF_SUCCESS);
}

/* @todo add hashtable walker with result info and intermediate abort */
static void crVBoxServerSaveCreateInfoCB(unsigned long key, void *data1, void *data2)
{
    CRCreateInfo_t *pCreateInfo = (CRCreateInfo_t *) data1;
    PSSMHANDLE pSSM = (PSSMHANDLE) data2;
    int32_t rc;

    CRASSERT(pCreateInfo && pSSM);

    rc = SSMR3PutMem(pSSM, &key, sizeof(key));
    CRASSERT(rc == VINF_SUCCESS);

    rc = SSMR3PutMem(pSSM, pCreateInfo, sizeof(*pCreateInfo));
    CRASSERT(rc == VINF_SUCCESS);

    if (pCreateInfo->pszDpyName)
    {
        rc = SSMR3PutStrZ(pSSM, pCreateInfo->pszDpyName);
        CRASSERT(rc == VINF_SUCCESS);
    }
}

static void crVBoxServerSyncTextureCB(unsigned long key, void *data1, void *data2)
{
    CRTextureObj *pTexture = (CRTextureObj *) data1;
    CRContext *pContext = (CRContext *) data2;

    CRASSERT(pTexture && pContext);
    crStateTextureObjectDiff(pContext, NULL, NULL, pTexture, GL_TRUE);
}

static void crVBoxServerSaveContextStateCB(unsigned long key, void *data1, void *data2)
{
    CRContext *pContext = (CRContext *) data1;
    PSSMHANDLE pSSM = (PSSMHANDLE) data2;
    int32_t rc;

    CRASSERT(pContext && pSSM);

    /* We could have skipped saving the key and use similar callback to load context states back,
     * but there's no guarantee we'd traverse hashtable in same order after loading.
     */
    rc = SSMR3PutMem(pSSM, &key, sizeof(key));
    CRASSERT(rc == VINF_SUCCESS);
    
#ifdef CR_STATE_NO_TEXTURE_IMAGE_STORE
    if (cr_server.curClient)
    {
        crServerDispatchMakeCurrent(cr_server.curClient->currentWindow, 0, pContext->id);
    }
#endif

    rc = crStateSaveContext(pContext, pSSM);
    CRASSERT(rc == VINF_SUCCESS);
}

static uint32_t g_hackVBoxServerSaveLoadCallsLeft = 0;

DECLEXPORT(int32_t) crVBoxServerSaveState(PSSMHANDLE pSSM)
{
    int32_t  rc, i;
    uint32_t ui32;
    GLboolean b;
    unsigned long key;
#ifdef CR_STATE_NO_TEXTURE_IMAGE_STORE
    unsigned long ctxID=-1, winID=-1;
#endif

    /* We shouldn't be called if there's no clients at all*/
    CRASSERT(cr_server.numClients>0);

    /* @todo it's hack atm */
    /* We want to be called only once to save server state but atm we're being called from svcSaveState
     * for every connected client (e.g. guest opengl application)
     */
    if (!cr_server.bIsInSavingState) /* It's first call */
    {
        cr_server.bIsInSavingState = GL_TRUE;

        /* Store number of clients */
        rc = SSMR3PutU32(pSSM, (uint32_t) cr_server.numClients);
        AssertRCReturn(rc, rc);

        g_hackVBoxServerSaveLoadCallsLeft = cr_server.numClients;
    }

    g_hackVBoxServerSaveLoadCallsLeft--;

    /* Do nothing untill we're being called last time */
    if (g_hackVBoxServerSaveLoadCallsLeft>0)
    {
        return VINF_SUCCESS;
    }

    /* Save rendering contexts creation info */
    ui32 = crHashtableNumElements(cr_server.pContextCreateInfoTable);
    rc = SSMR3PutU32(pSSM, (uint32_t) ui32);
    AssertRCReturn(rc, rc);
    crHashtableWalk(cr_server.pContextCreateInfoTable, crVBoxServerSaveCreateInfoCB, pSSM);

#ifdef CR_STATE_NO_TEXTURE_IMAGE_STORE
    /* Save current win and ctx IDs, as we'd rebind contexts when saving textures */
    if (cr_server.curClient)
    {
        ctxID = cr_server.curClient->currentContextNumber;
        winID = cr_server.curClient->currentWindow;
    }
#endif

    /* Save contexts state tracker data */
    /* @todo For now just some blind data dumps, 
     * but I've a feeling those should be saved/restored in a very strict sequence to
     * allow diff_api to work correctly. 
     * Should be tested more with multiply guest opengl apps working when saving VM snapshot.
     */
    crHashtableWalk(cr_server.contextTable, crVBoxServerSaveContextStateCB, pSSM);

#ifdef CR_STATE_NO_TEXTURE_IMAGE_STORE
    /* Restore original win and ctx IDs*/
    if (cr_server.curClient)
    {
        crServerDispatchMakeCurrent(winID, 0, ctxID);
    }
#endif

    /* Save windows creation info */
    ui32 = crHashtableNumElements(cr_server.pWindowCreateInfoTable);
    rc = SSMR3PutU32(pSSM, (uint32_t) ui32);
    AssertRCReturn(rc, rc);
    crHashtableWalk(cr_server.pWindowCreateInfoTable, crVBoxServerSaveCreateInfoCB, pSSM);

    /* Save cr_server.muralTable
     * @todo we don't need it all, just geometry info actually
     * @todo store visible regions as well 
     */
    ui32 = crHashtableNumElements(cr_server.muralTable);
    /* There should be default mural always */
    CRASSERT(ui32>=1);
    rc = SSMR3PutU32(pSSM, (uint32_t) ui32-1);
    AssertRCReturn(rc, rc);
    crHashtableWalk(cr_server.muralTable, crVBoxServerSaveMuralCB, pSSM);

    /* Save starting free context and window IDs */
    rc = SSMR3PutMem(pSSM, &cr_server.idsPool, sizeof(cr_server.idsPool));
    AssertRCReturn(rc, rc);

    /* Save clients info */
    for (i = 0; i < cr_server.numClients; i++)
    {
        if (cr_server.clients[i] && cr_server.clients[i]->conn) 
        {
            CRClient *pClient = cr_server.clients[i];

            rc = SSMR3PutU32(pSSM, pClient->conn->u32ClientID);
            AssertRCReturn(rc, rc);

            rc = SSMR3PutMem(pSSM, pClient, sizeof(*pClient));
            AssertRCReturn(rc, rc);

            if (pClient->currentCtx && pClient->currentContextNumber>=0)
            {
                b = crHashtableGetDataKey(cr_server.contextTable, pClient->currentCtx, &key);
                CRASSERT(b);
                rc = SSMR3PutMem(pSSM, &key, sizeof(key));
                AssertRCReturn(rc, rc);
            }

            if (pClient->currentMural && pClient->currentWindow>=0)
            {
                b = crHashtableGetDataKey(cr_server.muralTable, pClient->currentMural, &key);
                CRASSERT(b);
                rc = SSMR3PutMem(pSSM, &key, sizeof(key));
                AssertRCReturn(rc, rc);
            }
        }
    }

    cr_server.bIsInSavingState = GL_FALSE;

    return VINF_SUCCESS;
}

DECLEXPORT(int32_t) crVBoxServerLoadState(PSSMHANDLE pSSM)
{
    int32_t  rc, i;
    uint32_t ui, uiNumElems;
    unsigned long key;

    if (!cr_server.bIsInLoadingState)
    {
        /* AssertRCReturn(...) will leave us in loading state, but it doesn't matter as we'd be failing anyway */
        cr_server.bIsInLoadingState = GL_TRUE;

        /* Read number of clients */
        rc = SSMR3GetU32(pSSM, &g_hackVBoxServerSaveLoadCallsLeft);
        AssertRCReturn(rc, rc);
    }

    g_hackVBoxServerSaveLoadCallsLeft--;

    /* Do nothing untill we're being called last time */
    if (g_hackVBoxServerSaveLoadCallsLeft>0)
    {
        return VINF_SUCCESS;
    }

    /* Load and recreate rendering contexts */
    rc = SSMR3GetU32(pSSM, &uiNumElems);
    AssertRCReturn(rc, rc);
    for (ui=0; ui<uiNumElems; ++ui)
    {
        CRCreateInfo_t createInfo;
        char psz[200];
        GLint ctxID;

        rc = SSMR3GetMem(pSSM, &key, sizeof(key));
        AssertRCReturn(rc, rc);
        rc = SSMR3GetMem(pSSM, &createInfo, sizeof(createInfo));
        AssertRCReturn(rc, rc);

        if (createInfo.pszDpyName)
        {
            rc = SSMR3GetStrZEx(pSSM, psz, 200, NULL);
            AssertRCReturn(rc, rc);
            createInfo.pszDpyName = psz;
        }

        ctxID = crServerDispatchCreateContextEx(createInfo.pszDpyName, createInfo.visualBits, 0, key, createInfo.internalID);
        CRASSERT((int64_t)ctxID == (int64_t)key);
    }

    /* Restore context state data */
    for (ui=0; ui<uiNumElems; ++ui)
    {
        CRContext *pContext;

        rc = SSMR3GetMem(pSSM, &key, sizeof(key));
        AssertRCReturn(rc, rc);

        pContext = (CRContext*) crHashtableSearch(cr_server.contextTable, key);
        CRASSERT(pContext);

        rc = crStateLoadContext(pContext, pSSM);
        AssertRCReturn(rc, rc);
    }    

    /* Load windows */
    rc = SSMR3GetU32(pSSM, &uiNumElems);
    AssertRCReturn(rc, rc);
    for (ui=0; ui<uiNumElems; ++ui)
    {
        CRCreateInfo_t createInfo;
        char psz[200];
        GLint winID;
        unsigned long key;

        rc = SSMR3GetMem(pSSM, &key, sizeof(key));
        AssertRCReturn(rc, rc);
        rc = SSMR3GetMem(pSSM, &createInfo, sizeof(createInfo));
        AssertRCReturn(rc, rc);

        if (createInfo.pszDpyName)
        {
            rc = SSMR3GetStrZEx(pSSM, psz, 200, NULL);
            AssertRCReturn(rc, rc);
            createInfo.pszDpyName = psz;
        }

        winID = crServerDispatchWindowCreateEx(createInfo.pszDpyName, createInfo.visualBits, key);
        CRASSERT((int64_t)winID == (int64_t)key);
    }

    /* Load cr_server.muralTable */
    rc = SSMR3GetU32(pSSM, &uiNumElems);
    AssertRCReturn(rc, rc);
    for (ui=0; ui<uiNumElems; ++ui)
    {
        CRMuralInfo muralInfo;

        rc = SSMR3GetMem(pSSM, &key, sizeof(key));
        AssertRCReturn(rc, rc);
        rc = SSMR3GetMem(pSSM, &muralInfo, sizeof(muralInfo));
        AssertRCReturn(rc, rc);

        /* Restore windows geometry info */
        crServerDispatchWindowSize(key, muralInfo.underlyingDisplay[2], muralInfo.underlyingDisplay[3]);
        crServerDispatchWindowPosition(key, muralInfo.underlyingDisplay[0], muralInfo.underlyingDisplay[1]);
    }

    /* Load starting free context and window IDs */
    rc = SSMR3GetMem(pSSM, &cr_server.idsPool, sizeof(cr_server.idsPool));
    CRASSERT(rc == VINF_SUCCESS);

    /* Load clients info */
    for (i = 0; i < cr_server.numClients; i++)
    {
        if (cr_server.clients[i] && cr_server.clients[i]->conn) 
        {
            CRClient *pClient = cr_server.clients[i];
            CRClient client;
            unsigned long ctxID=-1, winID=-1;

            rc = SSMR3GetU32(pSSM, &ui);
            AssertRCReturn(rc, rc);
            /* If this assert fires, then we should search correct client in the list first*/
            CRASSERT(ui == pClient->conn->u32ClientID);

            rc = SSMR3GetMem(pSSM, &client, sizeof(client));
            CRASSERT(rc == VINF_SUCCESS);

            client.conn = pClient->conn;
            /* We can't reassign client number, as we'd get wrong results in TranslateTextureID
             * and fail to bind old textures.
             */
            /*client.number = pClient->number;*/
            *pClient = client;

            pClient->currentContextNumber = -1;
            pClient->currentCtx = cr_server.DummyContext;
            pClient->currentMural = NULL;
            pClient->currentWindow = -1;

            cr_server.curClient = pClient;

            if (client.currentCtx && client.currentContextNumber>=0)
            {
                rc = SSMR3GetMem(pSSM, &ctxID, sizeof(ctxID));
                AssertRCReturn(rc, rc);
                client.currentCtx = (CRContext*) crHashtableSearch(cr_server.contextTable, ctxID);
                CRASSERT(client.currentCtx);
                //pClient->currentCtx = client.currentCtx;
                //pClient->currentContextNumber = ctxID;
            }

            if (client.currentMural && client.currentWindow>=0)
            {
                rc = SSMR3GetMem(pSSM, &winID, sizeof(winID));
                AssertRCReturn(rc, rc);
                client.currentMural = (CRMuralInfo*) crHashtableSearch(cr_server.muralTable, winID);
                CRASSERT(client.currentMural);
                //pClient->currentMural = client.currentMural;
                //pClient->currentWindow = winID;
            }

            /* Restore client active context and window */
            crServerDispatchMakeCurrent(winID, 0, ctxID);

            if (0)
            {
            CRContext *tmpCtx;
            CRCreateInfo_t *createInfo;
            GLfloat one[4] = { 1, 1, 1, 1 };
            GLfloat amb[4] = { 0.4f, 0.4f, 0.4f, 1.0f };

            crServerDispatchMakeCurrent(winID, 0, ctxID);

            crHashtableWalk(client.currentCtx->shared->textureTable, crVBoxServerSyncTextureCB, client.currentCtx);

            crStateTextureObjectDiff(client.currentCtx, NULL, NULL, &client.currentCtx->texture.base1D, GL_TRUE);
            crStateTextureObjectDiff(client.currentCtx, NULL, NULL, &client.currentCtx->texture.base2D, GL_TRUE);
            crStateTextureObjectDiff(client.currentCtx, NULL, NULL, &client.currentCtx->texture.base3D, GL_TRUE);
#ifdef CR_ARB_texture_cube_map
            crStateTextureObjectDiff(client.currentCtx, NULL, NULL, &client.currentCtx->texture.baseCubeMap, GL_TRUE);
#endif
#ifdef CR_NV_texture_rectangle
            //@todo this doesn't work as expected
            //crStateTextureObjectDiff(client.currentCtx, NULL, NULL, &client.currentCtx->texture.baseRect, GL_TRUE);
#endif
            /*cr_server.head_spu->dispatch_table.Materialfv(GL_FRONT_AND_BACK, GL_AMBIENT, amb);
            cr_server.head_spu->dispatch_table.LightModelfv(GL_LIGHT_MODEL_AMBIENT, amb);
            cr_server.head_spu->dispatch_table.Lightfv(GL_LIGHT1, GL_DIFFUSE, one);

            cr_server.head_spu->dispatch_table.Enable(GL_LIGHTING);
            cr_server.head_spu->dispatch_table.Enable(GL_LIGHT0);
            cr_server.head_spu->dispatch_table.Enable(GL_LIGHT1);

            cr_server.head_spu->dispatch_table.Enable(GL_CULL_FACE);
            cr_server.head_spu->dispatch_table.Enable(GL_TEXTURE_2D);*/
            
            //crStateViewport( 0, 0, 600, 600 );
            //pClient->currentMural->viewportValidated = GL_FALSE;
            //cr_server.head_spu->dispatch_table.Viewport( 0, 0, 600, 600 );

            //crStateMatrixMode(GL_PROJECTION);
            //cr_server.head_spu->dispatch_table.MatrixMode(GL_PROJECTION);

            //crStateLoadIdentity();
            //cr_server.head_spu->dispatch_table.LoadIdentity();

            //crStateFrustum(-0.5, 0.5, -0.5, 0.5, 1.5, 150.0);
            //cr_server.head_spu->dispatch_table.Frustum(-0.5, 0.5, -0.5, 0.5, 1.5, 150.0);

            //crStateMatrixMode(GL_MODELVIEW);
            //cr_server.head_spu->dispatch_table.MatrixMode(GL_MODELVIEW);
            //crServerDispatchLoadIdentity();
            //crStateFrustum(-0.5, 0.5, -0.5, 0.5, 1.5, 150.0);
            //cr_server.head_spu->dispatch_table.Frustum(-0.5, 0.5, -0.5, 0.5, 1.5, 150.0);
            //crServerDispatchLoadIdentity();

                /*createInfo = (CRCreateInfo_t *) crHashtableSearch(cr_server.pContextCreateInfoTable, ctxID);
                CRASSERT(createInfo);
                tmpCtx = crStateCreateContext(NULL, createInfo->visualBits, NULL);
                CRASSERT(tmpCtx);
                crStateDiffContext(tmpCtx, client.currentCtx);
                crStateDestroyContext(tmpCtx);*/
            }
        }
    }

    //crServerDispatchMakeCurrent(-1, 0, -1);

    cr_server.curClient = NULL;

    {
        GLenum err = crServerDispatchGetError();

        if (err != GL_NO_ERROR)
        {
            crWarning("crServer: glGetError %d after loading snapshot", err);
        }
    }

    cr_server.bIsInLoadingState = GL_FALSE;

    return VINF_SUCCESS;
}

