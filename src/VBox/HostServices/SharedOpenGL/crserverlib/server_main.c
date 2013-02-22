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
#include "cr_vreg.h"
#include "cr_environment.h"
#include "cr_pixeldata.h"
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

static void deleteMuralInfoCallback( void *data )
{
    CRMuralInfo *m = (CRMuralInfo *) data;
    if (m->spuWindow) /* <- do not do term for default mural as it does not contain any info to be freed,
                       * and renderspu will destroy it up itself*/
    {
        crServerMuralTerm(m);
    }
    crFree(m);
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

    /* needed to make sure window dummy mural not get created on mural destruction
     * and generally this should be zeroed up */
    cr_server.currentCtxInfo = NULL;
    cr_server.currentWindow = 0;
    cr_server.currentNativeWindow = 0;
    cr_server.currentMural = NULL;

    /* sync our state with renderspu,
     * do it before mural & context deletion to avoid deleting currently set murals/contexts*/
    cr_server.head_spu->dispatch_table.MakeCurrent(0, 0, 0);

    /* Deallocate all semaphores */
    crFreeHashtable(cr_server.semaphores, crFree);
    cr_server.semaphores = NULL;

    /* Deallocate all barriers */
    crFreeHashtable(cr_server.barriers, DeleteBarrierCallback);
    cr_server.barriers = NULL;

    /* Free all context info */
    crFreeHashtable(cr_server.contextTable, deleteContextInfoCallback);

    /* Free vertex programs */
    crFreeHashtable(cr_server.programTable, crFree);

    /* Free dummy murals */
    crFreeHashtable(cr_server.dummyMuralTable, deleteMuralInfoCallback);

    /* Free murals */
    crFreeHashtable(cr_server.muralTable, deleteMuralInfoCallback);

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

    VBoxVrTerm();
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
    int rc = VBoxVrInit();
    if (!RT_SUCCESS(rc))
    {
        crWarning("VBoxVrInit failed, rc %d", rc);
        return;
    }

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

    cr_server.bUseMultipleContexts = (crGetenv( "CR_SERVER_ENABLE_MULTIPLE_CONTEXTS" ) != NULL);

    if (cr_server.bUseMultipleContexts)
    {
        crInfo("Info: using multiple contexts!");
        crDebug("Debug: using multiple contexts!");
    }

    cr_server.firstCallCreateContext = GL_TRUE;
    cr_server.firstCallMakeCurrent = GL_TRUE;
    cr_server.bForceMakeCurrentOnClientSwitch = GL_FALSE;

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
    cr_server.curClient->currentCtxInfo = &cr_server.MainContextInfo;

    cr_server.dummyMuralTable = crAllocHashtable();

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

    int rc = VBoxVrInit();
    if (!RT_SUCCESS(rc))
    {
        crWarning("VBoxVrInit failed, rc %d", rc);
        return GL_FALSE;
    }

#if DEBUG_FP_EXCEPTIONS
    {
        fpu_control_t mask;
        _FPU_GETCW(mask);
        mask &= ~(_FPU_MASK_IM | _FPU_MASK_DM | _FPU_MASK_ZM
                            | _FPU_MASK_OM | _FPU_MASK_UM);
        _FPU_SETCW(mask);
    }
#endif

    cr_server.bUseMultipleContexts = (crGetenv( "CR_SERVER_ENABLE_MULTIPLE_CONTEXTS" ) != NULL);

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
    cr_server.bForceMakeCurrentOnClientSwitch = GL_FALSE;

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

    cr_server.dummyMuralTable = crAllocHashtable();

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

    rc = SSMR3PutMem(pSSM, pMI, RT_OFFSETOF(CRMuralInfo, CreateInfo));
    CRASSERT(rc == VINF_SUCCESS);

    if (pMI->pVisibleRects)
    {
        rc = SSMR3PutMem(pSSM, pMI->pVisibleRects, 4*sizeof(GLint)*pMI->cVisibleRects);
    }

    rc = SSMR3PutMem(pSSM, pMI->ctxUsage, sizeof (pMI->ctxUsage));
    CRASSERT(rc == VINF_SUCCESS);
}

/* @todo add hashtable walker with result info and intermediate abort */
static void crVBoxServerSaveCreateInfoCB(unsigned long key, void *data1, void *data2)
{
    CRCreateInfo_t *pCreateInfo = (CRCreateInfo_t *)data1;
    PSSMHANDLE pSSM = (PSSMHANDLE) data2;
    int32_t rc;

    CRASSERT(pCreateInfo && pSSM);

    /* Don't store default mural create info */
    if (!key) return;

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

static void crVBoxServerSaveCreateInfoFromMuralInfoCB(unsigned long key, void *data1, void *data2)
{
    CRMuralInfo *pMural = (CRMuralInfo *)data1;
    CRCreateInfo_t *pCreateInfo = &pMural->CreateInfo;
    crVBoxServerSaveCreateInfoCB(key, pCreateInfo, data2);
}

static void crVBoxServerSaveCreateInfoFromCtxInfoCB(unsigned long key, void *data1, void *data2)
{
    CRContextInfo *pContextInfo = (CRContextInfo *)data1;
    CRCreateInfo_t CreateInfo = pContextInfo->CreateInfo;
    /* saved state contains internal id */
    CreateInfo.externalID = pContextInfo->pContext->id;
    crVBoxServerSaveCreateInfoCB(key, &CreateInfo, data2);
}

static void crVBoxServerSyncTextureCB(unsigned long key, void *data1, void *data2)
{
    CRTextureObj *pTexture = (CRTextureObj *) data1;
    CRContext *pContext = (CRContext *) data2;

    CRASSERT(pTexture && pContext);
    crStateTextureObjectDiff(pContext, NULL, NULL, pTexture, GL_TRUE);
}

typedef struct CRVBOX_SAVE_STATE_GLOBAL
{
    /* context id -> mural association
     * on context data save, each context will be made current with the corresponding mural from this table
     * thus saving the mural front & back buffer data */
    CRHashTable *contextMuralTable;
    /* mural id -> context info
     * for murals that do not have associated context in contextMuralTable
     * we still need to save*/
    CRHashTable *additionalMuralContextTable;

    PSSMHANDLE pSSM;

    int rc;
} CRVBOX_SAVE_STATE_GLOBAL, *PCRVBOX_SAVE_STATE_GLOBAL;


typedef struct CRVBOX_CTXWND_CTXWALKER_CB
{
    PCRVBOX_SAVE_STATE_GLOBAL pGlobal;
    CRHashTable *usedMuralTable;
    GLuint cAdditionalMurals;
} CRVBOX_CTXWND_CTXWALKER_CB, *PCRVBOX_CTXWND_CTXWALKER_CB;

static void crVBoxServerBuildAdditionalWindowContextMapCB(unsigned long key, void *data1, void *data2)
{
    CRMuralInfo * pMural = (CRMuralInfo *) data1;
    PCRVBOX_CTXWND_CTXWALKER_CB pData = (PCRVBOX_CTXWND_CTXWALKER_CB)data2;
    CRContextInfo *pContextInfo = NULL;

    if (!pMural->CreateInfo.externalID)
        return;

    if (crHashtableSearch(pData->usedMuralTable, pMural->CreateInfo.externalID))
    {
        Assert(crHashtableGetDataKey(pData->pGlobal->contextMuralTable, pMural, NULL));
        return;
    }

    Assert(!crHashtableGetDataKey(pData->pGlobal->contextMuralTable, pMural, NULL));

    if (cr_server.MainContextInfo.CreateInfo.visualBits == pMural->CreateInfo.visualBits)
    {
        pContextInfo = &cr_server.MainContextInfo;
    }
    else
    {
        crWarning("different visual bits not implemented!");
        pContextInfo = &cr_server.MainContextInfo;
    }

    crHashtableAdd(pData->pGlobal->additionalMuralContextTable, pMural->CreateInfo.externalID, pContextInfo);
}


typedef struct CRVBOX_CTXWND_WNDWALKER_CB
{
    PCRVBOX_SAVE_STATE_GLOBAL pGlobal;
    CRHashTable *usedMuralTable;
    CRContextInfo *pContextInfo;
    CRMuralInfo * pMural;
} CRVBOX_CTXWND_WNDWALKER_CB, *PCRVBOX_CTXWND_WNDWALKER_CB;

static void crVBoxServerBuildContextWindowMapWindowWalkerCB(unsigned long key, void *data1, void *data2)
{
    CRMuralInfo * pMural = (CRMuralInfo *) data1;
    PCRVBOX_CTXWND_WNDWALKER_CB pData = (PCRVBOX_CTXWND_WNDWALKER_CB)data2;

    Assert(pData->pMural != pMural);
    Assert(pData->pContextInfo);

    if (pData->pMural)
        return;

    if (!pMural->CreateInfo.externalID)
        return;

    if (!CR_STATE_SHAREDOBJ_USAGE_IS_SET(pMural, pData->pContextInfo->pContext))
        return;

    if (crHashtableSearch(pData->usedMuralTable, pMural->CreateInfo.externalID))
        return;

    CRASSERT(pMural->CreateInfo.visualBits == pData->pContextInfo->CreateInfo.visualBits);
    pData->pMural = pMural;
}

static void crVBoxServerBuildContextUsedWindowMapCB(unsigned long key, void *data1, void *data2)
{
    CRContextInfo *pContextInfo = (CRContextInfo *)data1;
    PCRVBOX_CTXWND_CTXWALKER_CB pData = (PCRVBOX_CTXWND_CTXWALKER_CB)data2;

    if (!pContextInfo->currentMural)
        return;

    crHashtableAdd(pData->pGlobal->contextMuralTable, pContextInfo->CreateInfo.externalID, pContextInfo->currentMural);
    crHashtableAdd(pData->usedMuralTable, pContextInfo->currentMural->CreateInfo.externalID, pContextInfo->currentMural);
}

CRMuralInfo * crServerGetDummyMural(GLint visualBits)
{
    CRMuralInfo * pMural = (CRMuralInfo *)crHashtableSearch(cr_server.dummyMuralTable, visualBits);
    if (!pMural)
    {
        GLint id;
        pMural = (CRMuralInfo *) crCalloc(sizeof(CRMuralInfo));
        if (!pMural)
        {
            crWarning("crCalloc failed!");
            return NULL;
        }
        id = crServerMuralInit(pMural, "", visualBits, -1);
        if (id < 0)
        {
            crWarning("crServerMuralInit failed!");
            crFree(pMural);
            return NULL;
        }

        crHashtableAdd(cr_server.dummyMuralTable, visualBits, pMural);
    }

    return pMural;
}

static void crVBoxServerBuildContextUnusedWindowMapCB(unsigned long key, void *data1, void *data2)
{
    CRContextInfo *pContextInfo = (CRContextInfo *)data1;
    PCRVBOX_CTXWND_CTXWALKER_CB pData = (PCRVBOX_CTXWND_CTXWALKER_CB)data2;
    CRMuralInfo * pMural = NULL;

    if (pContextInfo->currentMural)
        return;

    Assert(crHashtableNumElements(pData->pGlobal->contextMuralTable) <= crHashtableNumElements(cr_server.muralTable) - 1);
    if (crHashtableNumElements(pData->pGlobal->contextMuralTable) < crHashtableNumElements(cr_server.muralTable) - 1)
    {
        CRVBOX_CTXWND_WNDWALKER_CB MuralData;
        MuralData.pGlobal = pData->pGlobal;
        MuralData.usedMuralTable = pData->usedMuralTable;
        MuralData.pContextInfo = pContextInfo;
        MuralData.pMural = NULL;

        crHashtableWalk(cr_server.muralTable, crVBoxServerBuildContextWindowMapWindowWalkerCB, &MuralData);

        pMural = MuralData.pMural;

    }

    if (!pMural)
    {
        pMural = crServerGetDummyMural(pContextInfo->CreateInfo.visualBits);
        if (!pMural)
        {
            crWarning("crServerGetDummyMural failed");
            return;
        }
    }
    else
    {
        crHashtableAdd(pData->usedMuralTable, pMural->CreateInfo.externalID, pMural);
        ++pData->cAdditionalMurals;
    }

    crHashtableAdd(pData->pGlobal->contextMuralTable, pContextInfo->CreateInfo.externalID, pMural);
}

static void crVBoxServerBuildSaveStateGlobal(PCRVBOX_SAVE_STATE_GLOBAL pGlobal)
{
    CRVBOX_CTXWND_CTXWALKER_CB Data;
    GLuint cMurals;
    pGlobal->contextMuralTable = crAllocHashtable();
    pGlobal->additionalMuralContextTable = crAllocHashtable();
    /* 1. go through all contexts and match all having currentMural set */
    Data.pGlobal = pGlobal;
    Data.usedMuralTable = crAllocHashtable();
    Data.cAdditionalMurals = 0;
    crHashtableWalk(cr_server.contextTable, crVBoxServerBuildContextUsedWindowMapCB, &Data);

    cMurals = crHashtableNumElements(pGlobal->contextMuralTable);
    CRASSERT(cMurals <= crHashtableNumElements(cr_server.contextTable));
    CRASSERT(cMurals <= crHashtableNumElements(cr_server.muralTable) - 1);
    CRASSERT(cMurals == crHashtableNumElements(Data.usedMuralTable));
    if (cMurals < crHashtableNumElements(cr_server.contextTable))
    {
        Data.cAdditionalMurals = 0;
        crHashtableWalk(cr_server.contextTable, crVBoxServerBuildContextUnusedWindowMapCB, &Data);
    }

    CRASSERT(crHashtableNumElements(pGlobal->contextMuralTable) == crHashtableNumElements(cr_server.contextTable));
    CRASSERT(cMurals + Data.cAdditionalMurals <= crHashtableNumElements(cr_server.muralTable) - 1);
    if (cMurals + Data.cAdditionalMurals < crHashtableNumElements(cr_server.muralTable) - 1)
    {
        crHashtableWalk(cr_server.muralTable, crVBoxServerBuildAdditionalWindowContextMapCB, &Data);
        CRASSERT(cMurals + Data.cAdditionalMurals + crHashtableNumElements(pGlobal->additionalMuralContextTable) == crHashtableNumElements(cr_server.muralTable) - 1);
    }

    crFreeHashtable(Data.usedMuralTable, NULL);
}

static void crVBoxServerFBImageDataTerm(CRFBData *pData)
{
    GLuint i;
    for (i = 0; i < pData->cElements; ++i)
    {
        CRFBDataElement * pEl = &pData->aElements[i];
        if (pEl->pvData)
        {
            crFree(pEl->pvData);
            /* sanity */
            pEl->pvData = NULL;
        }
    }
    pData->cElements = 0;
}

static int crVBoxServerFBImageDataInitEx(CRFBData *pData, CRContextInfo *pCtxInfo, CRMuralInfo *pMural, GLboolean fWrite, GLboolean fForceBackFrontOnly, GLuint overrideWidth, GLuint overrideHeight)
{
    CRContext *pContext;
    GLuint i;
    GLfloat *pF;
    CRFBDataElement *pEl;
    GLuint width;
    GLuint height;

    CRASSERT(pCtxInfo->currentMural);

    crMemset(pData, 0, sizeof (*pData));

    pContext = pCtxInfo->pContext;

    width = overrideWidth ? overrideWidth : pMural->width;
    height = overrideHeight ? overrideHeight : pMural->height;

    if (!width || !height)
        return VINF_SUCCESS;

    pData->idFBO = pMural->fUseFBO ? pMural->aidColorTexs[fWrite ? pMural->iCurDrawBuffer : pMural->iCurReadBuffer] : 0;
    pData->cElements = 0;

    pEl = &pData->aElements[pData->cElements];
    pEl->idFBO = pMural->fUseFBO ? pMural->aidFBOs[CR_SERVER_FBO_FB_IDX(pMural)] : 0;
    pEl->enmBuffer = pData->aElements[1].idFBO ? GL_COLOR_ATTACHMENT0 : GL_FRONT;
    pEl->posX = 0;
    pEl->posY = 0;
    pEl->width = width;
    pEl->height = height;
    pEl->enmFormat = GL_RGBA;
    pEl->enmType = GL_UNSIGNED_BYTE;
    pEl->cbData = width * height * 4;
    pEl->pvData = crCalloc(pEl->cbData);
    if (!pEl->pvData)
    {
        crVBoxServerFBImageDataTerm(pData);
        crWarning("crVBoxServerFBImageDataInit: crCalloc failed");
        return VERR_NO_MEMORY;
    }
    ++pData->cElements;

    /* there is a lot of code that assumes we have double buffering, just assert here to print a warning in the log
     * so that we know that something irregular is going on */
    CRASSERT(pCtxInfo->CreateInfo.visualBits && CR_DOUBLE_BIT);
    if ((pCtxInfo->CreateInfo.visualBits && CR_DOUBLE_BIT) || fForceBackFrontOnly)
    {
        pEl = &pData->aElements[pData->cElements];
        pEl->idFBO = pMural->fUseFBO ? pMural->aidFBOs[CR_SERVER_FBO_BB_IDX(pMural)] : 0;
        pEl->enmBuffer = pData->aElements[1].idFBO ? GL_COLOR_ATTACHMENT0 : GL_BACK;
        pEl->posX = 0;
        pEl->posY = 0;
        pEl->width = width;
        pEl->height = height;
        pEl->enmFormat = GL_RGBA;
        pEl->enmType = GL_UNSIGNED_BYTE;
        pEl->cbData = width * height * 4;
        pEl->pvData = crCalloc(pEl->cbData);
        if (!pEl->pvData)
        {
            crVBoxServerFBImageDataTerm(pData);
            crWarning("crVBoxServerFBImageDataInit: crCalloc failed");
            return VERR_NO_MEMORY;
        }
        ++pData->cElements;
    }

    if (!fForceBackFrontOnly)
    {
        if (pCtxInfo->CreateInfo.visualBits && CR_DEPTH_BIT)
        {
            AssertCompile(sizeof (GLfloat) == 4);
            pEl = &pData->aElements[pData->cElements];
            pEl->idFBO = pMural->fUseFBO ? pMural->aidFBOs[CR_SERVER_FBO_FB_IDX(pMural)] : 0;
            pEl->enmBuffer = 0; /* we do not care */
            pEl->posX = 0;
            pEl->posY = 0;
            pEl->width = width;
            pEl->height = height;
            pEl->enmFormat = GL_DEPTH_COMPONENT;
            pEl->enmType = GL_FLOAT;
            pEl->cbData = width * height * 4;
            pEl->pvData = crCalloc(pEl->cbData);
            if (!pEl->pvData)
            {
                crVBoxServerFBImageDataTerm(pData);
                crWarning("crVBoxServerFBImageDataInit: crCalloc failed");
                return VERR_NO_MEMORY;
            }

            /* init to default depth value, just in case */
            pF = (GLfloat*)pEl->pvData;
            for (i = 0; i < width * height; ++i)
            {
                pF[i] = 1.;
            }
            ++pData->cElements;
        }

        if (pCtxInfo->CreateInfo.visualBits && CR_STENCIL_BIT)
        {
            AssertCompile(sizeof (GLuint) == 4);
            pEl = &pData->aElements[pData->cElements];
            pEl->idFBO = pMural->fUseFBO ? pMural->aidFBOs[CR_SERVER_FBO_FB_IDX(pMural)] : 0;
            pEl->enmBuffer = 0; /* we do not care */
            pEl->posX = 0;
            pEl->posY = 0;
            pEl->width = width;
            pEl->height = height;
            pEl->enmFormat = GL_STENCIL_INDEX;
            pEl->enmType = GL_UNSIGNED_INT;
            pEl->cbData = width * height * 4;
            pEl->pvData = crCalloc(pEl->cbData);
            if (!pEl->pvData)
            {
                crVBoxServerFBImageDataTerm(pData);
                crWarning("crVBoxServerFBImageDataInit: crCalloc failed");
                return VERR_NO_MEMORY;
            }
            ++pData->cElements;
        }
    }
    return VINF_SUCCESS;
}

static int crVBoxServerFBImageDataInit(CRFBData *pData, CRContextInfo *pCtxInfo, CRMuralInfo *pMural, GLboolean fWrite)
{
    return crVBoxServerFBImageDataInitEx(pData, pCtxInfo, pMural, fWrite, GL_FALSE, 0, 0);
}

static int crVBoxServerSaveFBImage(PSSMHANDLE pSSM)
{
    CRContextInfo *pCtxInfo;
    CRContext *pContext;
    CRMuralInfo *pMural;
    int32_t rc;
    GLuint i;
    struct
    {
        CRFBData data;
        CRFBDataElement buffer[3]; /* CRFBData::aElements[1] + buffer[3] gives 4: back, front, depth and stencil  */
    } Data;

    Assert(sizeof (Data) >= RT_OFFSETOF(CRFBData, aElements[4]));

    pCtxInfo = cr_server.currentCtxInfo;
    pContext = pCtxInfo->pContext;
    pMural = pCtxInfo->currentMural;

    rc = crVBoxServerFBImageDataInit(&Data.data, pCtxInfo, pMural, GL_FALSE);
    if (!RT_SUCCESS(rc))
    {
        crWarning("crVBoxServerFBImageDataInit failed rc %d", rc);
        return rc;
    }

    rc = crStateAcquireFBImage(pContext, &Data.data);
    AssertRCReturn(rc, rc);

    for (i = 0; i < Data.data.cElements; ++i)
    {
        CRFBDataElement * pEl = &Data.data.aElements[i];
        rc = SSMR3PutMem(pSSM, pEl->pvData, pEl->cbData);
        AssertRCReturn(rc, rc);
    }

    crVBoxServerFBImageDataTerm(&Data.data);

    return VINF_SUCCESS;
}

#define CRSERVER_ASSERTRC_RETURN_VOID(_rc) do { \
        if(!RT_SUCCESS((_rc))) { \
            AssertFailed(); \
            return; \
        } \
    } while (0)

static void crVBoxServerSaveAdditionalMuralsCB(unsigned long key, void *data1, void *data2)
{
    CRContextInfo *pContextInfo = (CRContextInfo *) data1;
    PCRVBOX_SAVE_STATE_GLOBAL pData = (PCRVBOX_SAVE_STATE_GLOBAL)data2;
    CRMuralInfo *pMural = (CRMuralInfo*)crHashtableSearch(cr_server.muralTable, key);
    PSSMHANDLE pSSM = pData->pSSM;
    CRbitvalue initialCtxUsage[CR_MAX_BITARRAY];
    CRMuralInfo *pInitialCurMural = pContextInfo->currentMural;

    crMemcpy(initialCtxUsage, pMural->ctxUsage, sizeof (initialCtxUsage));

    CRSERVER_ASSERTRC_RETURN_VOID(pData->rc);

    pData->rc = SSMR3PutMem(pSSM, &key, sizeof(key));
    CRSERVER_ASSERTRC_RETURN_VOID(pData->rc);

    pData->rc = SSMR3PutMem(pSSM, &pContextInfo->CreateInfo.externalID, sizeof(pContextInfo->CreateInfo.externalID));
    CRSERVER_ASSERTRC_RETURN_VOID(pData->rc);

    crServerPerformMakeCurrent(pMural, pContextInfo);

    pData->rc = crVBoxServerSaveFBImage(pSSM);

    /* restore the reference data, we synchronize it with the HW state in a later crServerPerformMakeCurrent call */
    crMemcpy(pMural->ctxUsage, initialCtxUsage, sizeof (initialCtxUsage));
    pContextInfo->currentMural = pInitialCurMural;

    CRSERVER_ASSERTRC_RETURN_VOID(pData->rc);
}

static void crVBoxServerSaveContextStateCB(unsigned long key, void *data1, void *data2)
{
    CRContextInfo *pContextInfo = (CRContextInfo *) data1;
    CRContext *pContext = pContextInfo->pContext;
    PCRVBOX_SAVE_STATE_GLOBAL pData = (PCRVBOX_SAVE_STATE_GLOBAL)data2;
    PSSMHANDLE pSSM = pData->pSSM;
    CRMuralInfo *pMural = (CRMuralInfo*)crHashtableSearch(pData->contextMuralTable, key);
    CRMuralInfo *pContextCurrentMural = pContextInfo->currentMural;
    const int32_t i32Dummy = 0;

    AssertCompile(sizeof (i32Dummy) == sizeof (pMural->CreateInfo.externalID));
    CRSERVER_ASSERTRC_RETURN_VOID(pData->rc);

    CRASSERT(pContext && pSSM);
    CRASSERT(pMural);
    CRASSERT(pMural->CreateInfo.externalID);

    /* We could have skipped saving the key and use similar callback to load context states back,
     * but there's no guarantee we'd traverse hashtable in same order after loading.
     */
    pData->rc = SSMR3PutMem(pSSM, &key, sizeof(key));
    CRSERVER_ASSERTRC_RETURN_VOID(pData->rc);

#ifdef DEBUG_misha
    {
            unsigned long id;
            if (!crHashtableGetDataKey(cr_server.contextTable, pContextInfo, &id))
                crWarning("No client id for server ctx %d", pContextInfo->CreateInfo.externalID);
            else
                CRASSERT(id == key);
    }
#endif

#ifdef CR_STATE_NO_TEXTURE_IMAGE_STORE
    if (pContextInfo->currentMural
            || crHashtableSearch(cr_server.muralTable, pMural->CreateInfo.externalID) /* <- this is not a dummy mural */
            )
    {
        CRASSERT(pMural->CreateInfo.externalID);
        CRASSERT(!crHashtableSearch(cr_server.dummyMuralTable, pMural->CreateInfo.externalID));
        pData->rc = SSMR3PutMem(pSSM, &pMural->CreateInfo.externalID, sizeof(pMural->CreateInfo.externalID));
    }
    else
    {
        /* this is a dummy mural */
        CRASSERT(!pMural->width);
        CRASSERT(!pMural->height);
        CRASSERT(crHashtableSearch(cr_server.dummyMuralTable, pMural->CreateInfo.externalID));
        pData->rc = SSMR3PutMem(pSSM, &i32Dummy, sizeof(pMural->CreateInfo.externalID));
    }
    CRSERVER_ASSERTRC_RETURN_VOID(pData->rc);

    CRASSERT(CR_STATE_SHAREDOBJ_USAGE_IS_SET(pMural, pContext));
    CRASSERT(pContextInfo->currentMural == pMural || !pContextInfo->currentMural);
    CRASSERT(cr_server.curClient);

    crServerPerformMakeCurrent(pMural, pContextInfo);
#endif

    pData->rc = crStateSaveContext(pContext, pSSM);
    CRSERVER_ASSERTRC_RETURN_VOID(pData->rc);

    pData->rc = crVBoxServerSaveFBImage(pSSM);
    CRSERVER_ASSERTRC_RETURN_VOID(pData->rc);

    /* restore the initial current mural */
    pContextInfo->currentMural = pContextCurrentMural;
}

#if 0
typedef struct CR_SERVER_CHECK_BUFFERS
{
    CRBufferObject *obj;
    CRContext *ctx;
}CR_SERVER_CHECK_BUFFERS, *PCR_SERVER_CHECK_BUFFERS;

static void crVBoxServerCheckConsistencyContextBuffersCB(unsigned long key, void *data1, void *data2)
{
    CRContextInfo* pContextInfo = (CRContextInfo*)data1;
    CRContext *ctx = pContextInfo->pContext;
    PCR_SERVER_CHECK_BUFFERS pBuffers = (PCR_SERVER_CHECK_BUFFERS)data2;
    CRBufferObject *obj = pBuffers->obj;
    CRBufferObjectState *b = &(ctx->bufferobject);
    int j, k;

    if (obj == b->arrayBuffer)
    {
        Assert(!pBuffers->ctx || pBuffers->ctx == ctx);
        pBuffers->ctx = ctx;
    }
    if (obj == b->elementsBuffer)
    {
        Assert(!pBuffers->ctx || pBuffers->ctx == ctx);
        pBuffers->ctx = ctx;
    }
#ifdef CR_ARB_pixel_buffer_object
    if (obj == b->packBuffer)
    {
        Assert(!pBuffers->ctx || pBuffers->ctx == ctx);
        pBuffers->ctx = ctx;
    }
    if (obj == b->unpackBuffer)
    {
        Assert(!pBuffers->ctx || pBuffers->ctx == ctx);
        pBuffers->ctx = ctx;
    }
#endif

#ifdef CR_ARB_vertex_buffer_object
    for (j=0; j<CRSTATECLIENT_MAX_VERTEXARRAYS; ++j)
    {
        CRClientPointer *cp = crStateGetClientPointerByIndex(j, &ctx->client.array);
        if (obj == cp->buffer)
        {
            Assert(!pBuffers->ctx || pBuffers->ctx == ctx);
            pBuffers->ctx = ctx;
        }
    }

    for (k=0; k<ctx->client.vertexArrayStackDepth; ++k)
    {
        CRVertexArrays *pArray = &ctx->client.vertexArrayStack[k];
        for (j=0; j<CRSTATECLIENT_MAX_VERTEXARRAYS; ++j)
        {
            CRClientPointer *cp = crStateGetClientPointerByIndex(j, pArray);
            if (obj == cp->buffer)
            {
                Assert(!pBuffers->ctx || pBuffers->ctx == ctx);
                pBuffers->ctx = ctx;
            }
        }
    }
#endif
}

static void crVBoxServerCheckConsistencyBuffersCB(unsigned long key, void *data1, void *data2)
{
    CRBufferObject *obj = (CRBufferObject *)data1;
    CR_SERVER_CHECK_BUFFERS Buffers = {0};
    Buffers.obj = obj;
    crHashtableWalk(cr_server.contextTable, crVBoxServerCheckConsistencyContextBuffersCB, (void*)&Buffers);
}

//static void crVBoxServerCheckConsistency2CB(unsigned long key, void *data1, void *data2)
//{
//    CRContextInfo* pContextInfo1 = (CRContextInfo*)data1;
//    CRContextInfo* pContextInfo2 = (CRContextInfo*)data2;
//
//    CRASSERT(pContextInfo1->pContext);
//    CRASSERT(pContextInfo2->pContext);
//
//    if (pContextInfo1 == pContextInfo2)
//    {
//        CRASSERT(pContextInfo1->pContext == pContextInfo2->pContext);
//        return;
//    }
//
//    CRASSERT(pContextInfo1->pContext != pContextInfo2->pContext);
//    CRASSERT(pContextInfo1->pContext->shared);
//    CRASSERT(pContextInfo2->pContext->shared);
//    CRASSERT(pContextInfo1->pContext->shared == pContextInfo2->pContext->shared);
//    if (pContextInfo1->pContext->shared != pContextInfo2->pContext->shared)
//        return;
//
//    crHashtableWalk(pContextInfo1->pContext->shared->buffersTable, crVBoxServerCheckConsistencyBuffersCB, pContextInfo2);
//}
static void crVBoxServerCheckSharedCB(unsigned long key, void *data1, void *data2)
{
    CRContextInfo* pContextInfo = (CRContextInfo*)data1;
    void **ppShared = (void**)data2;
    if (!*ppShared)
        *ppShared = pContextInfo->pContext->shared;
    else
        Assert(pContextInfo->pContext->shared == *ppShared);
}

static void crVBoxServerCheckConsistency()
{
    CRSharedState *pShared = NULL;
    crHashtableWalk(cr_server.contextTable, crVBoxServerCheckSharedCB, (void*)&pShared);
    Assert(pShared);
    if (pShared)
    {
        crHashtableWalk(pShared->buffersTable, crVBoxServerCheckConsistencyBuffersCB, NULL);
    }
}
#endif

static uint32_t g_hackVBoxServerSaveLoadCallsLeft = 0;

DECLEXPORT(int32_t) crVBoxServerSaveState(PSSMHANDLE pSSM)
{
    int32_t  rc, i;
    uint32_t ui32;
    GLboolean b;
    unsigned long key;
    GLenum err;
#ifdef CR_STATE_NO_TEXTURE_IMAGE_STORE
    CRClient *curClient;
    CRMuralInfo *curMural = NULL;
    CRContextInfo *curCtxInfo = NULL;
#endif
    CRVBOX_SAVE_STATE_GLOBAL Data;

    crMemset(&Data, 0, sizeof (Data));

#if 0
    crVBoxServerCheckConsistency();
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
    curClient = cr_server.curClient;
    /* Save current win and ctx IDs, as we'd rebind contexts when saving textures */
    if (curClient)
    {
        curCtxInfo = cr_server.curClient->currentCtxInfo;
        curMural = cr_server.curClient->currentMural;
    }
    else if (cr_server.numClients)
    {
        cr_server.curClient = cr_server.clients[0];
    }
#endif

    /* first save windows info */
    /* Save windows creation info */
    ui32 = crHashtableNumElements(cr_server.muralTable);
    /* There should be default mural always */
    CRASSERT(ui32>=1);
    rc = SSMR3PutU32(pSSM, (uint32_t) ui32-1);
    AssertRCReturn(rc, rc);
    crHashtableWalk(cr_server.muralTable, crVBoxServerSaveCreateInfoFromMuralInfoCB, pSSM);

    /* Save cr_server.muralTable
     * @todo we don't need it all, just geometry info actually
     */
    rc = SSMR3PutU32(pSSM, (uint32_t) ui32-1);
    AssertRCReturn(rc, rc);
    crHashtableWalk(cr_server.muralTable, crVBoxServerSaveMuralCB, pSSM);

    /* we need to save front & backbuffer data for each mural first create a context -> mural association */
    crVBoxServerBuildSaveStateGlobal(&Data);

    rc = crStateSaveGlobals(pSSM);
    AssertRCReturn(rc, rc);

    Data.pSSM = pSSM;
    /* Save contexts state tracker data */
    /* @todo For now just some blind data dumps,
     * but I've a feeling those should be saved/restored in a very strict sequence to
     * allow diff_api to work correctly.
     * Should be tested more with multiply guest opengl apps working when saving VM snapshot.
     */
    crHashtableWalk(cr_server.contextTable, crVBoxServerSaveContextStateCB, &Data);
    AssertRCReturn(Data.rc, Data.rc);

    ui32 = crHashtableNumElements(Data.additionalMuralContextTable);
    rc = SSMR3PutU32(pSSM, (uint32_t) ui32);
    AssertRCReturn(rc, rc);

    crHashtableWalk(Data.additionalMuralContextTable, crVBoxServerSaveAdditionalMuralsCB, &Data);
    AssertRCReturn(Data.rc, Data.rc);

#ifdef CR_STATE_NO_TEXTURE_IMAGE_STORE
    cr_server.curClient = curClient;
    /* Restore original win and ctx IDs*/
    if (curClient && curMural && curCtxInfo)
    {
        crServerPerformMakeCurrent(curMural, curCtxInfo);
    }
    else
    {
        cr_server.bForceMakeCurrentOnClientSwitch = GL_TRUE;
    }
#endif

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

    /* all context gl error states should have now be synced with chromium erro states,
     * reset the error if any */
    while ((err = cr_server.head_spu->dispatch_table.GetError()) != GL_NO_ERROR)
        crWarning("crServer: glGetError %d after saving snapshot", err);

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

static int32_t crVBoxServerLoadMurals(PSSMHANDLE pSSM, uint32_t version)
{
    unsigned long key;
    uint32_t ui, uiNumElems;
    /* Load windows */
    int32_t rc = SSMR3GetU32(pSSM, &uiNumElems);
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
        rc = SSMR3GetMem(pSSM, &muralInfo, RT_OFFSETOF(CRMuralInfo, CreateInfo));
        AssertRCReturn(rc, rc);

        if (version <= SHCROGL_SSM_VERSION_BEFORE_FRONT_DRAW_TRACKING)
            muralInfo.bFbDraw = GL_TRUE;

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

        if (version >= SHCROGL_SSM_VERSION_WITH_WINDOW_CTX_USAGE)
        {
            CRMuralInfo *pActualMural = (CRMuralInfo *)crHashtableSearch(cr_server.muralTable, key);;
            CRASSERT(pActualMural);
            rc = SSMR3GetMem(pSSM, pActualMural->ctxUsage, sizeof (pActualMural->ctxUsage));
            CRASSERT(rc == VINF_SUCCESS);
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

    CRASSERT(RT_SUCCESS(rc));
    return VINF_SUCCESS;
}

static int crVBoxServerLoadFBImage(PSSMHANDLE pSSM, uint32_t version,
        CRContextInfo* pContextInfo, CRMuralInfo *pMural)
{
    CRContext *pContext = pContextInfo->pContext;
    int32_t rc = VINF_SUCCESS;
    GLuint i;
    /* can apply the data right away */
    struct
    {
        CRFBData data;
        CRFBDataElement buffer[3]; /* CRFBData::aElements[1] + buffer[3] gives 4: back, front, depth and stencil  */
    } Data;

    Assert(sizeof (Data) >= RT_OFFSETOF(CRFBData, aElements[4]));

    if (version >= SHCROGL_SSM_VERSION_WITH_SAVED_DEPTH_STENCIL_BUFFER)
    {
        CRASSERT(cr_server.currentCtxInfo == pContextInfo);
        CRASSERT(cr_server.currentMural = pMural);

        if (!pMural->width || !pMural->height)
            return VINF_SUCCESS;

        rc = crVBoxServerFBImageDataInit(&Data.data, pContextInfo, pMural, GL_TRUE);
        if (!RT_SUCCESS(rc))
        {
            crWarning("crVBoxServerFBImageDataInit failed rc %d", rc);
            return rc;
        }
    }
    else
    {
        GLint storedWidth, storedHeight;

        if (version > SHCROGL_SSM_VERSION_WITH_BUGGY_FB_IMAGE_DATA)
        {
            CRASSERT(cr_server.currentCtxInfo == pContextInfo);
            CRASSERT(cr_server.currentMural = pMural);
            storedWidth = pMural->width;
            storedHeight = pMural->height;
        }
        else
        {
            storedWidth = pContext->buffer.storedWidth;
            storedHeight = pContext->buffer.storedHeight;
        }

        if (!storedWidth || !storedHeight)
            return VINF_SUCCESS;

        rc = crVBoxServerFBImageDataInitEx(&Data.data, pContextInfo, pMural, GL_TRUE, GL_TRUE, storedWidth, storedHeight);
        if (!RT_SUCCESS(rc))
        {
            crWarning("crVBoxServerFBImageDataInit failed rc %d", rc);
            return rc;
        }
    }

    CRASSERT(Data.data.cElements);

    for (i = 0; i < Data.data.cElements; ++i)
    {
        CRFBDataElement * pEl = &Data.data.aElements[i];
        rc = SSMR3GetMem(pSSM, pEl->pvData, pEl->cbData);
        AssertRCReturn(rc, rc);
    }

    if (version > SHCROGL_SSM_VERSION_WITH_BUGGY_FB_IMAGE_DATA)
    {
        CRBufferState *pBuf = &pContext->buffer;
        /* can apply the data right away */
        CRASSERT(cr_server.currentCtxInfo == pContextInfo);
        CRASSERT(cr_server.currentMural = pMural);

        crStateApplyFBImage(pContext, &Data.data);
        AssertRCReturn(rc, rc);
        CRASSERT(!pBuf->pFrontImg);
        CRASSERT(!pBuf->pBackImg);
        crVBoxServerFBImageDataTerm(&Data.data);

        if (pMural->fUseFBO && pMural->bVisible)
        {
            crServerPresentFBO(pMural);
        }
    }
    else
    {
        CRBufferState *pBuf = &pContext->buffer;
        CRASSERT(!pBuf->pFrontImg);
        CRASSERT(!pBuf->pBackImg);
        CRASSERT(Data.data.cElements); /* <- older versions always saved front and back, and we filtered out the null-sized buffers above */

        if (Data.data.cElements)
        {
            CRFBData *pLazyData = crAlloc(RT_OFFSETOF(CRFBData, aElements[Data.data.cElements]));
            if (!RT_SUCCESS(rc))
            {
                crVBoxServerFBImageDataTerm(&Data.data);
                crWarning("crAlloc failed");
                return VERR_NO_MEMORY;
            }

            crMemcpy(pLazyData, &Data.data, RT_OFFSETOF(CRFBData, aElements[Data.data.cElements]));
            pBuf->pFrontImg = pLazyData;
        }
    }

    CRASSERT(RT_SUCCESS(rc));
    return VINF_SUCCESS;
}

DECLEXPORT(int32_t) crVBoxServerLoadState(PSSMHANDLE pSSM, uint32_t version)
{
    int32_t  rc, i;
    uint32_t ui, uiNumElems;
    unsigned long key;
    GLenum err;

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

    if (version < SHCROGL_SSM_VERSION_BEFORE_CTXUSAGE_BITS)
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

        ctxID = crServerDispatchCreateContextEx(createInfo.pszDpyName, createInfo.visualBits, 0, key, createInfo.externalID /* <-saved state stores internal id here*/);
        CRASSERT((int64_t)ctxID == (int64_t)key);

        pContextInfo = (CRContextInfo*) crHashtableSearch(cr_server.contextTable, key);
        CRASSERT(pContextInfo);
        CRASSERT(pContextInfo->pContext);
        pContext = pContextInfo->pContext;
        pContext->shared->id=-1;
    }

    if (version > SHCROGL_SSM_VERSION_WITH_BUGGY_FB_IMAGE_DATA)
    {
        /* we have a mural data here */
        rc = crVBoxServerLoadMurals(pSSM, version);
        AssertRCReturn(rc, rc);
    }

    if (version > SHCROGL_SSM_VERSION_WITH_BUGGY_FB_IMAGE_DATA && uiNumElems)
    {
        /* set the current client to allow doing crServerPerformMakeCurrent later */
        CRASSERT(cr_server.numClients);
        cr_server.curClient = cr_server.clients[0];
    }

    rc = crStateLoadGlobals(pSSM, version);
    AssertRCReturn(rc, rc);

    /* Restore context state data */
    for (ui=0; ui<uiNumElems; ++ui)
    {
        CRContextInfo* pContextInfo;
        CRContext *pContext;
        CRMuralInfo *pMural = NULL;
        int32_t winId = 0;

        rc = SSMR3GetMem(pSSM, &key, sizeof(key));
        AssertRCReturn(rc, rc);

        pContextInfo = (CRContextInfo*) crHashtableSearch(cr_server.contextTable, key);
        CRASSERT(pContextInfo);
        CRASSERT(pContextInfo->pContext);
        pContext = pContextInfo->pContext;

        if (version > SHCROGL_SSM_VERSION_WITH_BUGGY_FB_IMAGE_DATA)
        {
            rc = SSMR3GetMem(pSSM, &winId, sizeof(winId));
            AssertRCReturn(rc, rc);

            if (winId)
            {
                pMural = (CRMuralInfo*)crHashtableSearch(cr_server.muralTable, winId);
                CRASSERT(pMural);
            }
            else
            {
                /* null winId means a dummy mural, get it */
                pMural = crServerGetDummyMural(pContextInfo->CreateInfo.visualBits);
                CRASSERT(pMural);
            }

            crServerPerformMakeCurrent(pMural, pContextInfo);
        }

        rc = crStateLoadContext(pContext, cr_server.contextTable, crVBoxServerGetContextCB, pSSM, version);
        AssertRCReturn(rc, rc);

        /*Restore front/back buffer images*/
        rc = crVBoxServerLoadFBImage(pSSM, version, pContextInfo, pMural);
        AssertRCReturn(rc, rc);
    }

    if (version > SHCROGL_SSM_VERSION_WITH_BUGGY_FB_IMAGE_DATA)
    {
        CRContextInfo *pContextInfo;
        CRMuralInfo *pMural;
        GLint ctxId;

        rc = SSMR3GetU32(pSSM, &uiNumElems);
        AssertRCReturn(rc, rc);
        for (ui=0; ui<uiNumElems; ++ui)
        {
            CRbitvalue initialCtxUsage[CR_MAX_BITARRAY];
            CRMuralInfo *pInitialCurMural;

            rc = SSMR3GetMem(pSSM, &key, sizeof(key));
            AssertRCReturn(rc, rc);

            rc = SSMR3GetMem(pSSM, &ctxId, sizeof(ctxId));
            AssertRCReturn(rc, rc);

            pMural = (CRMuralInfo*)crHashtableSearch(cr_server.muralTable, key);
            CRASSERT(pMural);
            if (ctxId)
            {
                pContextInfo = (CRContextInfo *)crHashtableSearch(cr_server.contextTable, ctxId);
                CRASSERT(pContextInfo);
            }
            else
                pContextInfo =  &cr_server.MainContextInfo;

            crMemcpy(initialCtxUsage, pMural->ctxUsage, sizeof (initialCtxUsage));
            pInitialCurMural = pContextInfo->currentMural;

            crServerPerformMakeCurrent(pMural, pContextInfo);

            rc = crVBoxServerLoadFBImage(pSSM, version, pContextInfo, pMural);
            AssertRCReturn(rc, rc);

            /* restore the reference data, we synchronize it with the HW state in a later crServerPerformMakeCurrent call */
            crMemcpy(pMural->ctxUsage, initialCtxUsage, sizeof (initialCtxUsage));
            pContextInfo->currentMural = pInitialCurMural;
        }

        if (cr_server.currentCtxInfo != &cr_server.MainContextInfo)
        {
            /* most ogl data gets loaded to hw on chromium 3D state switch, i.e. inside crStateMakeCurrent -> crStateSwitchContext
             * to force the crStateSwitchContext being called later, we need to  set the current context to our dummy one */
            pMural = crServerGetDummyMural(cr_server.MainContextInfo.CreateInfo.visualBits);
            CRASSERT(pMural);
            crServerPerformMakeCurrent(pMural, &cr_server.MainContextInfo);
        }

        cr_server.curClient = NULL;
        cr_server.bForceMakeCurrentOnClientSwitch = GL_TRUE;
    }
    else
    {
        CRServerFreeIDsPool_t dummyIdsPool;

        /* we have a mural data here */
        rc = crVBoxServerLoadMurals(pSSM, version);
        AssertRCReturn(rc, rc);

        /* not used any more, just read it out and ignore */
        rc = SSMR3GetMem(pSSM, &dummyIdsPool, sizeof(dummyIdsPool));
        CRASSERT(rc == VINF_SUCCESS);
    }

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

    while ((err = cr_server.head_spu->dispatch_table.GetError()) != GL_NO_ERROR)
        crWarning("crServer: glGetError %d after loading snapshot", err);

    cr_server.bIsInLoadingState = GL_FALSE;

#if 0
    crVBoxServerCheckConsistency();
#endif

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
                && (cr_server.curClient->currentCtxInfo->pContext->buffer.pFrontImg)
                && cr_server.curClient->currentMural
                && cr_server.curClient->currentMural->screenId == sIndex
                && cr_server.curClient->currentCtxInfo->pContext->buffer.storedHeight == h
                && cr_server.curClient->currentCtxInfo->pContext->buffer.storedWidth == w)
            {
                int clientWindow = cr_server.curClient->currentWindow;
                int clientContext = cr_server.curClient->currentContextNumber;
                CRFBData *pLazyData = (CRFBData *)cr_server.curClient->currentCtxInfo->pContext->buffer.pFrontImg;

                if (clientWindow && clientWindow != cr_server.currentWindow)
                {
                    crServerDispatchMakeCurrent(clientWindow, 0, clientContext);
                }

                crStateApplyFBImage(cr_server.curClient->currentCtxInfo->pContext, pLazyData);
                crStateFreeFBImageLegacy(cr_server.curClient->currentCtxInfo->pContext);
            }
        }
        cr_server.curClient = NULL;
    }
#endif

    {
        PCR_DISPLAY pDisplay = crServerDisplayGetInitialized(sIndex);
        if (pDisplay)
            CrDpResize(pDisplay, w, h, w, h);
    }

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

int32_t crServerSetOffscreenRenderingMode(GLubyte value)
{
    if (cr_server.bForceOffscreenRendering==value)
    {
        return VINF_SUCCESS;
    }

    if (value > CR_SERVER_REDIR_MAXVAL)
    {
        crWarning("crServerSetOffscreenRenderingMode: invalid arg: %d", value);
        return VERR_INVALID_PARAMETER;
    }

    if (value && !crServerSupportRedirMuralFBO())
    {
        return VERR_NOT_SUPPORTED;
    }

    cr_server.bForceOffscreenRendering=value;

    crHashtableWalk(cr_server.muralTable, crVBoxServerCheckMuralCB, NULL);

    return VINF_SUCCESS;
}

DECLEXPORT(int32_t) crVBoxServerSetOffscreenRendering(GLboolean value)
{
    return crServerSetOffscreenRenderingMode(value ? CR_SERVER_REDIR_FBO_RAM : cr_server.bOffscreenRenderingDefault);
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

static void crVBoxServerUpdateScreenViewportCB(unsigned long key, void *data1, void *data2)
{
    CRMuralInfo *mural = (CRMuralInfo*) data1;
    int *sIndex = (int*) data2;

    if (mural->screenId != *sIndex)
        return;

    crServerCheckMuralGeometry(mural);
}


DECLEXPORT(int32_t) crVBoxServerSetScreenViewport(int sIndex, int32_t x, int32_t y, uint32_t w, uint32_t h)
{
    CRScreenViewportInfo *pVieport;
    GLboolean fPosChanged, fSizeChanged;

    crDebug("crVBoxServerSetScreenViewport(%i)", sIndex);

    if (sIndex<0 || sIndex>=cr_server.screenCount)
    {
        crWarning("crVBoxServerSetScreenViewport: invalid screen id %d", sIndex);
        return VERR_INVALID_PARAMETER;
    }

    pVieport = &cr_server.screenVieport[sIndex];
    fPosChanged = (pVieport->x != x || pVieport->y != y);
    fSizeChanged = (pVieport->w != w || pVieport->h != h);

    if (!fPosChanged && !fSizeChanged)
    {
        crDebug("crVBoxServerSetScreenViewport: no changes");
        return VINF_SUCCESS;
    }

    if (fPosChanged)
    {
        pVieport->x = x;
        pVieport->y = y;

        crHashtableWalk(cr_server.muralTable, crVBoxServerUpdateScreenViewportCB, &sIndex);
    }

    if (fSizeChanged)
    {
        pVieport->w = w;
        pVieport->h = h;

        /* no need to do anything here actually */
    }
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
