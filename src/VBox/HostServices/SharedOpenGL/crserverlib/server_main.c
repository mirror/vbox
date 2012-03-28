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
#include "render/renderspu.h"
#include <signal.h>
#include <stdlib.h>
#define DEBUG_FP_EXCEPTIONS 0
#if DEBUG_FP_EXCEPTIONS
#include <fpu_control.h>
#include <math.h>
#endif
#include <iprt/assert.h>
#include <VBox/err.h>

#ifdef VBOXCR_LOGFPS
#include <iprt/timer.h>
#endif

#ifdef VBOX_WITH_CRHGSMI
# include <VBox/HostServices/VBoxCrOpenGLSvc.h>
uint8_t* g_pvVRamBase = NULL;
uint32_t g_cbVRam = 0;
HCRHGSMICMDCOMPLETION g_hCrHgsmiCompletion = NULL;
PFNCRHGSMICMDCOMPLETION g_pfnCrHgsmiCompletion = NULL;
#endif

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

DECLINLINE(int32_t) crVBoxServerClientGet(uint32_t u32ClientID, CRClient **ppClient)
{
    CRClient *pClient = NULL;
    int32_t i;

    *ppClient = NULL;

    for (i = 0; i < cr_server.numClients; i++)
    {
        if (cr_server.clients[i] && cr_server.clients[i]->conn
            && cr_server.clients[i]->conn->u32ClientID==u32ClientID)
        {
            pClient = cr_server.clients[i];
            break;
        }
    }
    if (!pClient)
    {
        crWarning("client not found!");
        return VERR_INVALID_PARAMETER;
    }

    if (!pClient->conn->vMajor)
    {
        crWarning("no major version specified for client!");
        return VERR_NOT_SUPPORTED;
    }

    *ppClient = pClient;

    return VINF_SUCCESS;
}


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


static void deleteContextInfoCallback( void *data )
{
    CRContextInfo *c = (CRContextInfo *) data;
    crStateDestroyContext(c->pContext);
    if (c->CreateInfo.pszDpyName)
        crFree(c->CreateInfo.pszDpyName);
    crFree(c);
}


static void crServerTearDown( void )
{
    GLint i;
    CRClientNode *pNode, *pNext;

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
    crFreeHashtable(cr_server.contextTable, deleteContextInfoCallback);

    /* Free context/window creation info */
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

    pNode = cr_server.pCleanupClient;
    while (pNode)
    {
        pNext=pNode->next;
        crFree(pNode->pClient);
        crFree(pNode);
        pNode=pNext;
    }
    cr_server.pCleanupClient = NULL;

#if 1
    /* disable these two lines if trying to get stack traces with valgrind */
    crSPUUnloadChain(cr_server.head_spu);
    cr_server.head_spu = NULL;
#endif

    crStateDestroy();

    crNetTearDown();
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

#ifndef DEBUG_misha
    cr_server.bUseMultipleContexts = GL_FALSE;
#else
    cr_server.bUseMultipleContexts = GL_FALSE;
#endif

    if (cr_server.bUseMultipleContexts)
    {
        crInfo("Info: using multiple contexts!");
        crDebug("Debug: using multiple contexts!");
    }

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
    cr_server.MainContextInfo.pContext = crStateCreateContext( &cr_server.limits,
                                                   CR_RGB_BIT | CR_DEPTH_BIT, NULL );
    cr_server.curClient->currentCtxInfo = &cr_server.MainContextInfo;

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

#ifndef DEBUG_misha
    cr_server.bUseMultipleContexts = GL_FALSE;
#else
    cr_server.bUseMultipleContexts = GL_FALSE;
#endif

    if (cr_server.bUseMultipleContexts)
    {
        crInfo("Info: using multiple contexts!");
        crDebug("Debug: using multiple contexts!");
    }

    crNetInit(crServerRecv, crServerClose);

    cr_server.firstCallCreateContext = GL_TRUE;
    cr_server.firstCallMakeCurrent = GL_TRUE;

    cr_server.bIsInLoadingState = GL_FALSE;
    cr_server.bIsInSavingState  = GL_FALSE;


    cr_server.pCleanupClient = NULL;

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
    cr_server.MainContextInfo.pContext = crStateCreateContext( &cr_server.limits,
                                                   CR_RGB_BIT | CR_DEPTH_BIT, NULL );
//    cr_server.pContextCreateInfoTable = crAllocHashtable();
    cr_server.pWindowCreateInfoTable = crAllocHashtable();

    crServerSetVBoxConfigurationHGCM();

    if (!cr_server.head_spu)
        return GL_FALSE;

    crServerInitDispatch();
    crStateDiffAPI( &(cr_server.head_spu->dispatch_table) );

    /*Check for PBO support*/
    if (crStateGetCurrent()->extensions.ARB_pixel_buffer_object)
    {
        cr_server.bUsePBOForReadback=GL_TRUE;
    }

    return GL_TRUE;
}

int32_t crVBoxServerAddClient(uint32_t u32ClientID)
{
    CRClient *newClient;

    if (cr_server.numClients>=CR_MAX_CLIENTS)
    {
        return VERR_MAX_THRDS_REACHED;
    }

    newClient = (CRClient *) crCalloc(sizeof(CRClient));
    crDebug("crServer: AddClient u32ClientID=%d", u32ClientID);

    newClient->spu_id = 0;
    newClient->currentCtxInfo = &cr_server.MainContextInfo;
    newClient->currentContextNumber = -1;
    newClient->conn = crNetAcceptClient(cr_server.protocol, NULL,
                                        cr_server.tcpip_port,
                                        cr_server.mtu, 0);
    newClient->conn->u32ClientID = u32ClientID;

    cr_server.clients[cr_server.numClients++] = newClient;

    crServerAddToRunQueue(newClient);

    return VINF_SUCCESS;
}

void crVBoxServerRemoveClient(uint32_t u32ClientID)
{
    CRClient *pClient=NULL;
    int32_t i;

    crDebug("crServer: RemoveClient u32ClientID=%d", u32ClientID);

    for (i = 0; i < cr_server.numClients; i++)
    {
        if (cr_server.clients[i] && cr_server.clients[i]->conn
            && cr_server.clients[i]->conn->u32ClientID==u32ClientID)
        {
            pClient = cr_server.clients[i];
            break;
        }
    }
    //if (!pClient) return VERR_INVALID_PARAMETER;
    if (!pClient)
    {
        crWarning("Invalid client id %u passed to crVBoxServerRemoveClient", u32ClientID);
        return;
    }

#ifdef VBOX_WITH_CRHGSMI
    CRVBOXHGSMI_CMDDATA_ASSERT_CLEANED(&pClient->conn->CmdData);
#endif

    /* Disconnect the client */
    pClient->conn->Disconnect(pClient->conn);

    /* Let server clear client from the queue */
    crServerDeleteClient(pClient);
}

static int32_t crVBoxServerInternalClientWriteRead(CRClient *pClient)
{
#ifdef VBOXCR_LOGFPS
    uint64_t tstart, tend;
#endif

    /*crDebug("=>crServer: ClientWrite u32ClientID=%d", u32ClientID);*/


#ifdef VBOXCR_LOGFPS
    tstart = RTTimeNanoTS();
#endif

    /* This should be setup already */
    CRASSERT(pClient->conn->pBuffer);
    CRASSERT(pClient->conn->cbBuffer);
#ifdef VBOX_WITH_CRHGSMI
    CRVBOXHGSMI_CMDDATA_ASSERT_CONSISTENT(&pClient->conn->CmdData);
#endif

    if (
#ifdef VBOX_WITH_CRHGSMI
         !CRVBOXHGSMI_CMDDATA_IS_SET(&pClient->conn->CmdData) &&
#endif
         cr_server.run_queue->client != pClient
         && crServerClientInBeginEnd(cr_server.run_queue->client))
    {
        crDebug("crServer: client %d blocked, allow_redir_ptr = 0", pClient->conn->u32ClientID);
        pClient->conn->allow_redir_ptr = 0;
    }
    else
    {
        pClient->conn->allow_redir_ptr = 1;
    }

    crNetRecv();
    CRASSERT(pClient->conn->pBuffer==NULL && pClient->conn->cbBuffer==0);
    CRVBOXHGSMI_CMDDATA_ASSERT_CLEANED(&pClient->conn->CmdData);

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

#ifndef VBOX_WITH_CRHGSMI
    CRASSERT(!pClient->conn->allow_redir_ptr || crNetNumMessages(pClient->conn)==0);
#endif

#ifdef VBOXCR_LOGFPS
    tend = RTTimeNanoTS();
    pClient->timeUsed += tend-tstart;
#endif
    /*crDebug("<=crServer: ClientWrite u32ClientID=%d", u32ClientID);*/

    return VINF_SUCCESS;
}


int32_t crVBoxServerClientWrite(uint32_t u32ClientID, uint8_t *pBuffer, uint32_t cbBuffer)
{
    CRClient *pClient=NULL;
    int32_t rc = crVBoxServerClientGet(u32ClientID, &pClient);

    if (RT_FAILURE(rc))
        return rc;


    CRASSERT(pBuffer);

    /* This should never fire unless we start to multithread */
    CRASSERT(pClient->conn->pBuffer==NULL && pClient->conn->cbBuffer==0);

    pClient->conn->pBuffer = pBuffer;
    pClient->conn->cbBuffer = cbBuffer;
#ifdef VBOX_WITH_CRHGSMI
    CRVBOXHGSMI_CMDDATA_ASSERT_CLEANED(&pClient->conn->CmdData);
#endif

    return crVBoxServerInternalClientWriteRead(pClient);
}

int32_t crVBoxServerInternalClientRead(CRClient *pClient, uint8_t *pBuffer, uint32_t *pcbBuffer)
{
    if (pClient->conn->cbHostBuffer > *pcbBuffer)
    {
        crDebug("crServer: [%lx] ClientRead u32ClientID=%d FAIL, host buffer too small %d of %d",
                  crThreadID(), pClient->conn->u32ClientID, *pcbBuffer, pClient->conn->cbHostBuffer);

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

int32_t crVBoxServerClientRead(uint32_t u32ClientID, uint8_t *pBuffer, uint32_t *pcbBuffer)
{
    CRClient *pClient=NULL;
    int32_t rc = crVBoxServerClientGet(u32ClientID, &pClient);

    if (RT_FAILURE(rc))
        return rc;

#ifdef VBOX_WITH_CRHGSMI
    CRVBOXHGSMI_CMDDATA_ASSERT_CLEANED(&pClient->conn->CmdData);
#endif

    return crVBoxServerInternalClientRead(pClient, pBuffer, pcbBuffer);
}

int32_t crVBoxServerClientSetVersion(uint32_t u32ClientID, uint32_t vMajor, uint32_t vMinor)
{
    CRClient *pClient=NULL;
    int32_t i;

    for (i = 0; i < cr_server.numClients; i++)
    {
        if (cr_server.clients[i] && cr_server.clients[i]->conn
            && cr_server.clients[i]->conn->u32ClientID==u32ClientID)
        {
            pClient = cr_server.clients[i];
            break;
        }
    }
    if (!pClient) return VERR_INVALID_PARAMETER;

    pClient->conn->vMajor = vMajor;
    pClient->conn->vMinor = vMinor;

    if (vMajor != CR_PROTOCOL_VERSION_MAJOR
        || vMinor != CR_PROTOCOL_VERSION_MINOR)
    {
        return VERR_NOT_SUPPORTED;
    }
    else return VINF_SUCCESS;
}

int32_t crVBoxServerClientSetPID(uint32_t u32ClientID, uint64_t pid)
{
    CRClient *pClient=NULL;
    int32_t i;

    for (i = 0; i < cr_server.numClients; i++)
    {
        if (cr_server.clients[i] && cr_server.clients[i]->conn
            && cr_server.clients[i]->conn->u32ClientID==u32ClientID)
        {
            pClient = cr_server.clients[i];
            break;
        }
    }
    if (!pClient) return VERR_INVALID_PARAMETER;

    pClient->pid = pid;

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

    if (pMI->pVisibleRects)
    {
        rc = SSMR3PutMem(pSSM, pMI->pVisibleRects, 4*sizeof(GLint)*pMI->cVisibleRects);
    }
}

/* @todo add hashtable walker with result info and intermediate abort */
static void crVBoxServerSaveCreateInfoCB(unsigned long key, void *data1, void *data2)
{
    CRCreateInfo_t *pCreateInfo = (CRCreateInfo_t *)data1;
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

static void crVBoxServerSaveCreateInfoFromCtxInfoCB(unsigned long key, void *data1, void *data2)
{
    CRContextInfo *pContextInfo = (CRContextInfo *)data1;
    CRCreateInfo_t *pCreateInfo = &pContextInfo->CreateInfo;
    crVBoxServerSaveCreateInfoCB(key, pCreateInfo, data2);
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
    CRContextInfo *pContextInfo = (CRContextInfo *) data1;
    CRContext *pContext = pContextInfo->pContext;
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
        unsigned long id;
        if (!crHashtableGetDataKey(cr_server.contextTable, pContextInfo, &id))
        {
            crWarning("No client id for server ctx %d", pContext->id);
        }
        else
        {
            crServerDispatchMakeCurrent(cr_server.curClient->currentWindow, 0, id);
        }
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

    /* Do nothing until we're being called last time */
    if (g_hackVBoxServerSaveLoadCallsLeft>0)
    {
        return VINF_SUCCESS;
    }

    /* Save rendering contexts creation info */
    ui32 = crHashtableNumElements(cr_server.contextTable);
    rc = SSMR3PutU32(pSSM, (uint32_t) ui32);
    AssertRCReturn(rc, rc);
    crHashtableWalk(cr_server.contextTable, crVBoxServerSaveCreateInfoFromCtxInfoCB, pSSM);

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

            rc = SSMR3PutU32(pSSM, pClient->conn->vMajor);
            AssertRCReturn(rc, rc);

            rc = SSMR3PutU32(pSSM, pClient->conn->vMinor);
            AssertRCReturn(rc, rc);

            rc = SSMR3PutMem(pSSM, pClient, sizeof(*pClient));
            AssertRCReturn(rc, rc);

            if (pClient->currentCtxInfo && pClient->currentCtxInfo->pContext && pClient->currentContextNumber>=0)
            {
                b = crHashtableGetDataKey(cr_server.contextTable, pClient->currentCtxInfo, &key);
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

static DECLCALLBACK(CRContext*) crVBoxServerGetContextCB(void* pvData)
{
    CRContextInfo* pContextInfo = (CRContextInfo*)pvData;
    CRASSERT(pContextInfo);
    CRASSERT(pContextInfo->pContext);
    return pContextInfo->pContext;
}

DECLEXPORT(int32_t) crVBoxServerLoadState(PSSMHANDLE pSSM, uint32_t version)
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

    /* Do nothing until we're being called last time */
    if (g_hackVBoxServerSaveLoadCallsLeft>0)
    {
        return VINF_SUCCESS;
    }

    if (version!=SHCROGL_SSM_VERSION)
    {
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    }

    /* Load and recreate rendering contexts */
    rc = SSMR3GetU32(pSSM, &uiNumElems);
    AssertRCReturn(rc, rc);
    for (ui=0; ui<uiNumElems; ++ui)
    {
        CRCreateInfo_t createInfo;
        char psz[200];
        GLint ctxID;
        CRContextInfo* pContextInfo;
        CRContext* pContext;

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

        pContextInfo = (CRContextInfo*) crHashtableSearch(cr_server.contextTable, key);
        CRASSERT(pContextInfo);
        CRASSERT(pContextInfo->pContext);
        pContext = pContextInfo->pContext;
        pContext->shared->id=-1;
    }

    /* Restore context state data */
    for (ui=0; ui<uiNumElems; ++ui)
    {
        CRContextInfo* pContextInfo;
        CRContext *pContext;

        rc = SSMR3GetMem(pSSM, &key, sizeof(key));
        AssertRCReturn(rc, rc);

        pContextInfo = (CRContextInfo*) crHashtableSearch(cr_server.contextTable, key);
        CRASSERT(pContextInfo);
        CRASSERT(pContextInfo->pContext);
        pContext = pContextInfo->pContext;

        rc = crStateLoadContext(pContext, cr_server.contextTable, crVBoxServerGetContextCB, pSSM);
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

        if (muralInfo.pVisibleRects)
        {
            muralInfo.pVisibleRects = crAlloc(4*sizeof(GLint)*muralInfo.cVisibleRects);
            if (!muralInfo.pVisibleRects)
            {
                return VERR_NO_MEMORY;
            }

            rc = SSMR3GetMem(pSSM, muralInfo.pVisibleRects, 4*sizeof(GLint)*muralInfo.cVisibleRects);
            AssertRCReturn(rc, rc);
        }

        /* Restore windows geometry info */
        crServerDispatchWindowSize(key, muralInfo.width, muralInfo.height);
        crServerDispatchWindowPosition(key, muralInfo.gX, muralInfo.gY);
        /* Same workaround as described in stub.c:stubUpdateWindowVisibileRegions for compiz on a freshly booted VM*/
        if (muralInfo.bReceivedRects)
        {
            crServerDispatchWindowVisibleRegion(key, muralInfo.cVisibleRects, muralInfo.pVisibleRects);
        }
        crServerDispatchWindowShow(key, muralInfo.bVisible);

        if (muralInfo.pVisibleRects)
        {
            crFree(muralInfo.pVisibleRects);
        }
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

            if (version>=4)
            {
                rc = SSMR3GetU32(pSSM, &pClient->conn->vMajor);
                AssertRCReturn(rc, rc);

                rc = SSMR3GetU32(pSSM, &pClient->conn->vMinor);
                AssertRCReturn(rc, rc);
            }

            rc = SSMR3GetMem(pSSM, &client, sizeof(client));
            CRASSERT(rc == VINF_SUCCESS);

            client.conn = pClient->conn;
            /* We can't reassign client number, as we'd get wrong results in TranslateTextureID
             * and fail to bind old textures.
             */
            /*client.number = pClient->number;*/
            *pClient = client;

            pClient->currentContextNumber = -1;
            pClient->currentCtxInfo = &cr_server.MainContextInfo;
            pClient->currentMural = NULL;
            pClient->currentWindow = -1;

            cr_server.curClient = pClient;

            if (client.currentCtxInfo && client.currentContextNumber>=0)
            {
                rc = SSMR3GetMem(pSSM, &ctxID, sizeof(ctxID));
                AssertRCReturn(rc, rc);
                client.currentCtxInfo = (CRContextInfo*) crHashtableSearch(cr_server.contextTable, ctxID);
                CRASSERT(client.currentCtxInfo);
                CRASSERT(client.currentCtxInfo->pContext);
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
//            CRContext *tmpCtx;
//            CRCreateInfo_t *createInfo;
            GLfloat one[4] = { 1, 1, 1, 1 };
            GLfloat amb[4] = { 0.4f, 0.4f, 0.4f, 1.0f };

            crServerDispatchMakeCurrent(winID, 0, ctxID);

            crHashtableWalk(client.currentCtxInfo->pContext->shared->textureTable, crVBoxServerSyncTextureCB, client.currentCtxInfo->pContext);

            crStateTextureObjectDiff(client.currentCtxInfo->pContext, NULL, NULL, &client.currentCtxInfo->pContext->texture.base1D, GL_TRUE);
            crStateTextureObjectDiff(client.currentCtxInfo->pContext, NULL, NULL, &client.currentCtxInfo->pContext->texture.base2D, GL_TRUE);
            crStateTextureObjectDiff(client.currentCtxInfo->pContext, NULL, NULL, &client.currentCtxInfo->pContext->texture.base3D, GL_TRUE);
#ifdef CR_ARB_texture_cube_map
            crStateTextureObjectDiff(client.currentCtxInfo->pContext, NULL, NULL, &client.currentCtxInfo->pContext->texture.baseCubeMap, GL_TRUE);
#endif
#ifdef CR_NV_texture_rectangle
            //@todo this doesn't work as expected
            //crStateTextureObjectDiff(client.currentCtxInfo->pContext, NULL, NULL, &client.currentCtxInfo->pContext->texture.baseRect, GL_TRUE);
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
                crStateDiffContext(tmpCtx, client.currentCtxInfo->pContext);
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

#define SCREEN(i) (cr_server.screen[i])
#define MAPPED(screen) ((screen).winID != 0)

static void crVBoxServerReparentMuralCB(unsigned long key, void *data1, void *data2)
{
    CRMuralInfo *pMI = (CRMuralInfo*) data1;
    int *sIndex = (int*) data2;

    if (pMI->screenId == *sIndex)
    {
        renderspuReparentWindow(pMI->spuWindow);
    }
}

static void crVBoxServerCheckMuralCB(unsigned long key, void *data1, void *data2)
{
    CRMuralInfo *pMI = (CRMuralInfo*) data1;
    (void) data2;

    crServerCheckMuralGeometry(pMI);
}

DECLEXPORT(int32_t) crVBoxServerSetScreenCount(int sCount)
{
    int i;

    if (sCount>CR_MAX_GUEST_MONITORS)
        return VERR_INVALID_PARAMETER;

    /*Shouldn't happen yet, but to be safe in future*/
    for (i=0; i<cr_server.screenCount; ++i)
    {
        if (MAPPED(SCREEN(i)))
            crWarning("Screen count is changing, but screen[%i] is still mapped", i);
        return VERR_NOT_IMPLEMENTED;
    }

    cr_server.screenCount = sCount;

    for (i=0; i<sCount; ++i)
    {
        SCREEN(i).winID = 0;
    }

    return VINF_SUCCESS;
}

DECLEXPORT(int32_t) crVBoxServerUnmapScreen(int sIndex)
{
    crDebug("crVBoxServerUnmapScreen(%i)", sIndex);

    if (sIndex<0 || sIndex>=cr_server.screenCount)
        return VERR_INVALID_PARAMETER;

    if (MAPPED(SCREEN(sIndex)))
    {
        SCREEN(sIndex).winID = 0;
        renderspuSetWindowId(0);

        crHashtableWalk(cr_server.muralTable, crVBoxServerReparentMuralCB, &sIndex);
    }

    renderspuSetWindowId(SCREEN(0).winID);
    return VINF_SUCCESS;
}

DECLEXPORT(int32_t) crVBoxServerMapScreen(int sIndex, int32_t x, int32_t y, uint32_t w, uint32_t h, uint64_t winID)
{
    crDebug("crVBoxServerMapScreen(%i) [%i,%i:%u,%u %x]", sIndex, x, y, w, h, winID);

    if (sIndex<0 || sIndex>=cr_server.screenCount)
        return VERR_INVALID_PARAMETER;

    if (MAPPED(SCREEN(sIndex)) && SCREEN(sIndex).winID!=winID)
    {
        crDebug("Mapped screen[%i] is being remapped.", sIndex);
        crVBoxServerUnmapScreen(sIndex);
    }

    SCREEN(sIndex).winID = winID;
    SCREEN(sIndex).x = x;
    SCREEN(sIndex).y = y;
    SCREEN(sIndex).w = w;
    SCREEN(sIndex).h = h;

    renderspuSetWindowId(SCREEN(sIndex).winID);
    crHashtableWalk(cr_server.muralTable, crVBoxServerReparentMuralCB, &sIndex);
    renderspuSetWindowId(SCREEN(0).winID);

    crHashtableWalk(cr_server.muralTable, crVBoxServerCheckMuralCB, NULL);

#ifndef WINDOWS
    /*Restore FB content for clients, which have current window on a screen being remapped*/
    {
        GLint i;

        for (i = 0; i < cr_server.numClients; i++)
        {
            cr_server.curClient = cr_server.clients[i];
            if (cr_server.curClient->currentCtxInfo
                && cr_server.curClient->currentCtxInfo->pContext
                && (cr_server.curClient->currentCtxInfo->pContext->buffer.pFrontImg || cr_server.curClient->currentCtxInfo->pContext->buffer.pBackImg)
                && cr_server.curClient->currentMural
                && cr_server.curClient->currentMural->screenId == sIndex
                && cr_server.curClient->currentCtxInfo->pContext->buffer.storedHeight == h
                && cr_server.curClient->currentCtxInfo->pContext->buffer.storedWidth == w)
            {
                int clientWindow = cr_server.curClient->currentWindow;
                int clientContext = cr_server.curClient->currentContextNumber;

                if (clientWindow && clientWindow != cr_server.currentWindow)
                {
                    crServerDispatchMakeCurrent(clientWindow, 0, clientContext);
                }

                crStateApplyFBImage(cr_server.curClient->currentCtxInfo->pContext);
            }
        }
        cr_server.curClient = NULL;
    }
#endif

    return VINF_SUCCESS;
}

DECLEXPORT(int32_t) crVBoxServerSetRootVisibleRegion(GLint cRects, GLint *pRects)
{
    renderspuSetRootVisibleRegion(cRects, pRects);

    return VINF_SUCCESS;
}

DECLEXPORT(void) crVBoxServerSetPresentFBOCB(PFNCRSERVERPRESENTFBO pfnPresentFBO)
{
    cr_server.pfnPresentFBO = pfnPresentFBO;
}

DECLEXPORT(int32_t) crVBoxServerSetOffscreenRendering(GLboolean value)
{
    if (cr_server.bForceOffscreenRendering==value)
    {
        return VINF_SUCCESS;
    }

    if (value && !crServerSupportRedirMuralFBO())
    {
        return VERR_NOT_SUPPORTED;
    }

    cr_server.bForceOffscreenRendering=value;

    crHashtableWalk(cr_server.muralTable, crVBoxServerCheckMuralCB, NULL);

    return VINF_SUCCESS;
}

DECLEXPORT(int32_t) crVBoxServerOutputRedirectSet(const CROutputRedirect *pCallbacks)
{
    /* No need for a synchronization as this is single threaded. */
    if (pCallbacks)
    {
        cr_server.outputRedirect = *pCallbacks;
        cr_server.bUseOutputRedirect = true;
    }
    else
    {
        cr_server.bUseOutputRedirect = false;
    }

    // @todo dynamically intercept already existing output:
    // crHashtableWalk(cr_server.muralTable, crVBoxServerOutputRedirectCB, NULL);

    return VINF_SUCCESS;
}


#ifdef VBOX_WITH_CRHGSMI
/* We moved all CrHgsmi command processing to crserverlib to keep the logic of dealing with CrHgsmi commands in one place.
 *
 * For now we need the notion of CrHgdmi commands in the crserver_lib to be able to complete it asynchronously once it is really processed.
 * This help avoiding the "blocked-client" issues. The client is blocked if another client is doing begin-end stuff.
 * For now we eliminated polling that could occur on block, which caused a higher-priority thread (in guest) polling for the blocked command complition
 * to block the lower-priority thread trying to complete the blocking command.
 * And removed extra memcpy done on blocked command arrival.
 *
 * In the future we will extend CrHgsmi functionality to maintain texture data directly in CrHgsmi allocation to avoid extra memcpy-ing with PBO,
 * implement command completion and stuff necessary for GPU scheduling to work properly for WDDM Windows guests, etc.
 *
 * NOTE: it is ALWAYS responsibility of the crVBoxServerCrHgsmiCmd to complete the command!
 * */
int32_t crVBoxServerCrHgsmiCmd(struct VBOXVDMACMD_CHROMIUM_CMD *pCmd, uint32_t cbCmd)
{
    int32_t rc;
    uint32_t cBuffers = pCmd->cBuffers;
    uint32_t cParams;
    uint32_t cbHdr;
    CRVBOXHGSMIHDR *pHdr;
    uint32_t u32Function;
    uint32_t u32ClientID;
    CRClient *pClient;

    if (!g_pvVRamBase)
    {
        crWarning("g_pvVRamBase is not initialized");
        crServerCrHgsmiCmdComplete(pCmd, VERR_INVALID_STATE);
        return VINF_SUCCESS;
    }

    if (!cBuffers)
    {
        crWarning("zero buffers passed in!");
        crServerCrHgsmiCmdComplete(pCmd, VERR_INVALID_PARAMETER);
        return VINF_SUCCESS;
    }

    cParams = cBuffers-1;

    cbHdr = pCmd->aBuffers[0].cbBuffer;
    pHdr = VBOXCRHGSMI_PTR_SAFE(pCmd->aBuffers[0].offBuffer, cbHdr, CRVBOXHGSMIHDR);
    if (!pHdr)
    {
        crWarning("invalid header buffer!");
        crServerCrHgsmiCmdComplete(pCmd, VERR_INVALID_PARAMETER);
        return VINF_SUCCESS;
    }

    if (cbHdr < sizeof (*pHdr))
    {
        crWarning("invalid header buffer size!");
        crServerCrHgsmiCmdComplete(pCmd, VERR_INVALID_PARAMETER);
        return VINF_SUCCESS;
    }

    u32Function = pHdr->u32Function;
    u32ClientID = pHdr->u32ClientID;

    switch (u32Function)
    {
        case SHCRGL_GUEST_FN_WRITE:
        {
            crDebug(("svcCall: SHCRGL_GUEST_FN_WRITE\n"));

            /* @todo: Verify  */
            if (cParams == 1)
            {
                CRVBOXHGSMIWRITE* pFnCmd = (CRVBOXHGSMIWRITE*)pHdr;
                VBOXVDMACMD_CHROMIUM_BUFFER *pBuf = &pCmd->aBuffers[1];
                /* Fetch parameters. */
                uint32_t cbBuffer = pBuf->cbBuffer;
                uint8_t *pBuffer  = VBOXCRHGSMI_PTR_SAFE(pBuf->offBuffer, cbBuffer, uint8_t);

                if (cbHdr < sizeof (*pFnCmd))
                {
                    crWarning("invalid write cmd buffer size!");
                    rc = VERR_INVALID_PARAMETER;
                    break;
                }

                CRASSERT(cbBuffer);
                if (!pBuffer)
                {
                    crWarning("invalid buffer data received from guest!");
                    rc = VERR_INVALID_PARAMETER;
                    break;
                }

                rc = crVBoxServerClientGet(u32ClientID, &pClient);
                if (RT_FAILURE(rc))
                {
                    break;
                }

                /* This should never fire unless we start to multithread */
                CRASSERT(pClient->conn->pBuffer==NULL && pClient->conn->cbBuffer==0);
                CRVBOXHGSMI_CMDDATA_ASSERT_CLEANED(&pClient->conn->CmdData);

                pClient->conn->pBuffer = pBuffer;
                pClient->conn->cbBuffer = cbBuffer;
                CRVBOXHGSMI_CMDDATA_SET(&pClient->conn->CmdData, pCmd, pHdr);
                rc = crVBoxServerInternalClientWriteRead(pClient);
                CRVBOXHGSMI_CMDDATA_ASSERT_CLEANED(&pClient->conn->CmdData);
                return rc;
            }
            else
            {
                crWarning("invalid number of args");
                rc = VERR_INVALID_PARAMETER;
                break;
            }
            break;
        }

        case SHCRGL_GUEST_FN_INJECT:
        {
            crDebug(("svcCall: SHCRGL_GUEST_FN_INJECT\n"));

            /* @todo: Verify  */
            if (cParams == 1)
            {
                CRVBOXHGSMIINJECT *pFnCmd = (CRVBOXHGSMIINJECT*)pHdr;
                /* Fetch parameters. */
                uint32_t u32InjectClientID = pFnCmd->u32ClientID;
                VBOXVDMACMD_CHROMIUM_BUFFER *pBuf = &pCmd->aBuffers[1];
                uint32_t cbBuffer = pBuf->cbBuffer;
                uint8_t *pBuffer  = VBOXCRHGSMI_PTR_SAFE(pBuf->offBuffer, cbBuffer, uint8_t);

                if (cbHdr < sizeof (*pFnCmd))
                {
                    crWarning("invalid inject cmd buffer size!");
                    rc = VERR_INVALID_PARAMETER;
                    break;
                }

                CRASSERT(cbBuffer);
                if (!pBuffer)
                {
                    crWarning("invalid buffer data received from guest!");
                    rc = VERR_INVALID_PARAMETER;
                    break;
                }

                rc = crVBoxServerClientGet(u32InjectClientID, &pClient);
                if (RT_FAILURE(rc))
                {
                    break;
                }

                /* This should never fire unless we start to multithread */
                CRASSERT(pClient->conn->pBuffer==NULL && pClient->conn->cbBuffer==0);
                CRVBOXHGSMI_CMDDATA_ASSERT_CLEANED(&pClient->conn->CmdData);

                pClient->conn->pBuffer = pBuffer;
                pClient->conn->cbBuffer = cbBuffer;
                CRVBOXHGSMI_CMDDATA_SET(&pClient->conn->CmdData, pCmd, pHdr);
                rc = crVBoxServerInternalClientWriteRead(pClient);
                CRVBOXHGSMI_CMDDATA_ASSERT_CLEANED(&pClient->conn->CmdData);
                return rc;
            }

            crWarning("invalid number of args");
            rc = VERR_INVALID_PARAMETER;
            break;
        }

        case SHCRGL_GUEST_FN_READ:
        {
            crDebug(("svcCall: SHCRGL_GUEST_FN_READ\n"));

            /* @todo: Verify  */
            if (cParams == 1)
            {
                CRVBOXHGSMIREAD *pFnCmd = (CRVBOXHGSMIREAD*)pHdr;
                VBOXVDMACMD_CHROMIUM_BUFFER *pBuf = &pCmd->aBuffers[1];
                /* Fetch parameters. */
                uint32_t cbBuffer = pBuf->cbBuffer;
                uint8_t *pBuffer  = VBOXCRHGSMI_PTR_SAFE(pBuf->offBuffer, cbBuffer, uint8_t);

                if (cbHdr < sizeof (*pFnCmd))
                {
                    crWarning("invalid read cmd buffer size!");
                    rc = VERR_INVALID_PARAMETER;
                    break;
                }


                if (!pBuffer)
                {
                    crWarning("invalid buffer data received from guest!");
                    rc = VERR_INVALID_PARAMETER;
                    break;
                }

                rc = crVBoxServerClientGet(u32ClientID, &pClient);
                if (RT_FAILURE(rc))
                {
                    break;
                }

                CRVBOXHGSMI_CMDDATA_ASSERT_CLEANED(&pClient->conn->CmdData);

                rc = crVBoxServerInternalClientRead(pClient, pBuffer, &cbBuffer);

                /* Return the required buffer size always */
                pFnCmd->cbBuffer = cbBuffer;

                CRVBOXHGSMI_CMDDATA_ASSERT_CLEANED(&pClient->conn->CmdData);

                /* the read command is never pended, complete it right away */
                pHdr->result = rc;
                crServerCrHgsmiCmdComplete(pCmd, VINF_SUCCESS);
                return VINF_SUCCESS;
            }

            crWarning("invalid number of args");
            rc = VERR_INVALID_PARAMETER;
            break;
        }

        case SHCRGL_GUEST_FN_WRITE_READ:
        {
            crDebug(("svcCall: SHCRGL_GUEST_FN_WRITE_READ\n"));

            /* @todo: Verify  */
            if (cParams == 2)
            {
                CRVBOXHGSMIWRITEREAD *pFnCmd = (CRVBOXHGSMIWRITEREAD*)pHdr;
                VBOXVDMACMD_CHROMIUM_BUFFER *pBuf = &pCmd->aBuffers[1];
                VBOXVDMACMD_CHROMIUM_BUFFER *pWbBuf = &pCmd->aBuffers[2];

                /* Fetch parameters. */
                uint32_t cbBuffer = pBuf->cbBuffer;
                uint8_t *pBuffer  = VBOXCRHGSMI_PTR_SAFE(pBuf->offBuffer, cbBuffer, uint8_t);

                uint32_t cbWriteback = pWbBuf->cbBuffer;
                char *pWriteback  = VBOXCRHGSMI_PTR_SAFE(pWbBuf->offBuffer, cbWriteback, char);

                if (cbHdr < sizeof (*pFnCmd))
                {
                    crWarning("invalid write_read cmd buffer size!");
                    rc = VERR_INVALID_PARAMETER;
                    break;
                }


                CRASSERT(cbBuffer);
                if (!pBuffer)
                {
                    crWarning("invalid write buffer data received from guest!");
                    rc = VERR_INVALID_PARAMETER;
                    break;
                }

                CRASSERT(cbWriteback);
                if (!pWriteback)
                {
                    crWarning("invalid writeback buffer data received from guest!");
                    rc = VERR_INVALID_PARAMETER;
                    break;
                }
                rc = crVBoxServerClientGet(u32ClientID, &pClient);
                if (RT_FAILURE(rc))
                {
                    pHdr->result = rc;
                    crServerCrHgsmiCmdComplete(pCmd, VINF_SUCCESS);
                    return rc;
                }

                /* This should never fire unless we start to multithread */
                CRASSERT(pClient->conn->pBuffer==NULL && pClient->conn->cbBuffer==0);
                CRVBOXHGSMI_CMDDATA_ASSERT_CLEANED(&pClient->conn->CmdData);

                pClient->conn->pBuffer = pBuffer;
                pClient->conn->cbBuffer = cbBuffer;
                CRVBOXHGSMI_CMDDATA_SETWB(&pClient->conn->CmdData, pCmd, pHdr, pWriteback, cbWriteback, &pFnCmd->cbWriteback);
                rc = crVBoxServerInternalClientWriteRead(pClient);
                CRVBOXHGSMI_CMDDATA_ASSERT_CLEANED(&pClient->conn->CmdData);
                return rc;
            }

            crWarning("invalid number of args");
            rc = VERR_INVALID_PARAMETER;
            break;
        }

        case SHCRGL_GUEST_FN_SET_VERSION:
        {
            crWarning("invalid function");
            rc = VERR_NOT_IMPLEMENTED;
            break;
        }

        case SHCRGL_GUEST_FN_SET_PID:
        {
            crWarning("invalid function");
            rc = VERR_NOT_IMPLEMENTED;
            break;
        }

        default:
        {
            crWarning("invalid function");
            rc = VERR_NOT_IMPLEMENTED;
            break;
        }

    }

    /* we can be on fail only here */
    CRASSERT(RT_FAILURE(rc));
    pHdr->result = rc;
    crServerCrHgsmiCmdComplete(pCmd, VINF_SUCCESS);
    return rc;
}

int32_t crVBoxServerCrHgsmiCtl(struct VBOXVDMACMD_CHROMIUM_CTL *pCtl, uint32_t cbCtl)
{
    int rc = VINF_SUCCESS;

    switch (pCtl->enmType)
    {
        case VBOXVDMACMD_CHROMIUM_CTL_TYPE_CRHGSMI_SETUP:
        {
            PVBOXVDMACMD_CHROMIUM_CTL_CRHGSMI_SETUP pSetup = (PVBOXVDMACMD_CHROMIUM_CTL_CRHGSMI_SETUP)pCtl;
            g_pvVRamBase = (uint8_t*)pSetup->pvVRamBase;
            g_cbVRam = pSetup->cbVRam;
            rc = VINF_SUCCESS;
            break;
        }
        case VBOXVDMACMD_CHROMIUM_CTL_TYPE_SAVESTATE_BEGIN:
        case VBOXVDMACMD_CHROMIUM_CTL_TYPE_SAVESTATE_END:
            rc = VINF_SUCCESS;
            break;
        case VBOXVDMACMD_CHROMIUM_CTL_TYPE_CRHGSMI_SETUP_COMPLETION:
        {
            PVBOXVDMACMD_CHROMIUM_CTL_CRHGSMI_SETUP_COMPLETION pSetup = (PVBOXVDMACMD_CHROMIUM_CTL_CRHGSMI_SETUP_COMPLETION)pCtl;
            g_hCrHgsmiCompletion = pSetup->hCompletion;
            g_pfnCrHgsmiCompletion = pSetup->pfnCompletion;
            rc = VINF_SUCCESS;
            break;
        }
        default:
            AssertMsgFailed(("invalid param %d", pCtl->enmType));
            rc = VERR_INVALID_PARAMETER;
    }

    /* NOTE: Control commands can NEVER be pended here, this is why its a task of a caller (Main)
     * to complete them accordingly.
     * This approach allows using host->host and host->guest commands in the same way here
     * making the command completion to be the responsibility of the command originator.
     * E.g. ctl commands can be both Hgcm Host synchronous commands that do not require completion at all,
     * or Hgcm Host Fast Call commands that do require completion. All this details are hidden here */
    return rc;
}
#endif
