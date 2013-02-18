/* Copyright (c) 2001, Stanford University
 * All rights reserved.
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#ifndef INCLUDE_CR_SERVER_H
#define INCLUDE_CR_SERVER_H

#include "cr_spu.h"
#include "cr_net.h"
#include "cr_hash.h"
#include "cr_protocol.h"
#include "cr_glstate.h"
#include "cr_vreg.h"
#include "spu_dispatch_table.h"

#include "state/cr_currentpointers.h"

#include <iprt/types.h>
#include <iprt/err.h>
#include <iprt/string.h>

#include <VBox/vmm/ssm.h>

#ifdef VBOX_WITH_CRHGSMI
# include <VBox/VBoxVideo.h>
#endif
#include <VBox/Hardware/VBoxVideoVBE.h>

#include "cr_vreg.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CR_MAX_WINDOWS 100
#define CR_MAX_CLIENTS 64

/*@todo must match MaxGuestMonitors from SchemaDefs.h*/
#define CR_MAX_GUEST_MONITORS VBOX_VIDEO_MAX_SCREENS

typedef DECLCALLBACKPTR(void, PFNCRSERVERPRESENTFBO) (void *data, int32_t screenId, int32_t x, int32_t y, uint32_t w, uint32_t h);

/* Callbacks for output of the rendered frames.
 *
 * This allows to pass rendered frames to an external component rather than draw them on screen.
 *
 * An external component registers the redirection callbacks using crVBoxServerOutputRedirectSet.
 *
 * The list of formats supported by the caller is obtained using CRORContextProperty.
 * The actual format choosed by the service is passed as a CRORBegin parameter.
 */
typedef struct {
    const void *pvContext; /* Supplied by crVBoxServerOutputRedirectSet. */
    DECLR3CALLBACKMEMBER(void, CRORBegin,           (const void *pvContext, void **ppvInstance,
                                                     const char *pszFormat));
    DECLR3CALLBACKMEMBER(void, CRORGeometry,        (void *pvInstance,
                                                     int32_t x, int32_t y, uint32_t w, uint32_t h));
    DECLR3CALLBACKMEMBER(void, CRORVisibleRegion,   (void *pvInstance,
                                                     uint32_t cRects, const RTRECT *paRects));
    DECLR3CALLBACKMEMBER(void, CRORFrame,           (void *pvInstance,
                                                     void *pvData, uint32_t cbData));
    DECLR3CALLBACKMEMBER(void, CROREnd,             (void *pvInstance));
    DECLR3CALLBACKMEMBER(int,  CRORContextProperty, (const void *pvContext, uint32_t index,
                                                      void *pvBuffer, uint32_t cbBuffer, uint32_t *pcbOut));
} CROutputRedirect;

typedef struct {
    CRrecti imagewindow;    /**< coordinates in mural space */
    CRrectf bounds;         /**< normalized coordinates in [-1,-1] x [1,1] */
    CRrecti outputwindow;   /**< coordinates in server's rendering window */
    CRrecti clippedImagewindow;  /**< imagewindow clipped to current viewport */
    CRmatrix baseProjection;  /**< pre-multiplied onto projection matrix */
    CRrecti scissorBox;     /**< passed to back-end OpenGL */
    CRrecti viewport;       /**< passed to back-end OpenGL */
    GLuint serialNo;        /**< an optimization */
} CRExtent;

struct BucketingInfo;

typedef struct {
    char   *pszDpyName;
    GLint   visualBits;
    int32_t externalID;
} CRCreateInfo_t;

/**
 * Mural info
 */
typedef struct {
    GLuint width, height;
    GLint gX, gY;            /*guest coordinates*/
    GLint hX, hY;            /*host coordinates, screenID related*/

    int spuWindow;           /*the SPU's corresponding window ID */

    int screenId;

    GLboolean bVisible;      /*guest window is visible*/
    GLubyte   fUseFBO;       /*redirect to FBO instead of real host window*/
    GLboolean bFbDraw;       /*GL_FRONT buffer is drawn to directly*/

    GLint       cVisibleRects;    /*count of visible rects*/
    GLint      *pVisibleRects;    /*visible rects left, top, right, bottom*/
    GLboolean   bReceivedRects;   /*indicates if guest did any updates for visible regions*/

    GLuint cBuffers;
    GLuint iBbBuffer;
    GLuint aidFBOs[2];
    GLuint aidColorTexs[2];

    void *pvOutputRedirectInstance;

    CRCreateInfo_t CreateInfo;

    /* to avoid saved state breakage we need to keep RT_OFFSETOF(CRMuralInfo, CreateInfo) intact
     * this is why we place some FBO stuff to the tail
     * @todo: once we need to increment a saved state version, we could refactor this structure */
    GLuint iCurDrawBuffer;
    GLuint iCurReadBuffer;

    GLuint idDepthStencilRB;
    GLuint fboWidth, fboHeight;
    GLuint idPBO;

    VBOXVR_SCR_COMPOSITOR_ENTRY CEntry;
    VBOXVR_SCR_COMPOSITOR Compositor;

    /* bitfield representing contexts the mural has been ever current with
     * we just reuse CR_STATE_SHAREDOBJ_USAGE_XXX API here for simplicity */
    CRbitvalue             ctxUsage[CR_MAX_BITARRAY];
} CRMuralInfo;

typedef struct {
    CRContext *pContext;
    int SpuContext;
    CRCreateInfo_t CreateInfo;
    CRMuralInfo * currentMural;
} CRContextInfo;

/**
 * A client is basically an upstream Cr Node (connected via mothership)
 */
typedef struct _crclient {
    int spu_id;        /**< id of the last SPU in the client's SPU chain */
    CRConnection *conn;       /**< network connection from the client */
    int number;        /**< a unique number for each client */
    uint64_t pid;      /*guest pid*/
    GLint currentContextNumber;
    CRContextInfo *currentCtxInfo;
    GLint currentWindow;
    CRMuralInfo *currentMural;
    GLint windowList[CR_MAX_WINDOWS];
    GLint contextList[CR_MAX_CONTEXTS];
#ifdef VBOXCR_LOGFPS
    uint64_t timeUsed;
#endif
} CRClient;

typedef struct _crclientnode {
    CRClient *pClient;
    struct _crclientnode *prev, *next;
} CRClientNode;

typedef struct CRPoly_t {
    int npoints;
    double *points;
    struct CRPoly_t *next;
} CRPoly;

/**
 * There's one of these run queue entries per client
 * The run queue is a circular, doubly-linked list of these objects.
 */
typedef struct RunQueue_t {
    CRClient *client;
    int blocked;
    struct RunQueue_t *next;
    struct RunQueue_t *prev;
} RunQueue;

typedef struct {
    GLint freeWindowID;
    GLint freeContextID;
    GLint freeClientID;
} CRServerFreeIDsPool_t;

typedef struct {
    int32_t    x, y;
    uint32_t   w, h;
    uint64_t   winID;
} CRScreenInfo;

typedef struct {
    int32_t    x, y;
    uint32_t   w, h;
} CRScreenViewportInfo;


/* DISPLAY */

typedef struct CR_DISPLAY_ENTRY
{
    VBOXVR_SCR_COMPOSITOR_ENTRY CEntry;
} CR_DISPLAY_ENTRY, *PCR_DISPLAY_ENTRY;

typedef struct CR_DISPLAY
{
    VBOXVR_SCR_COMPOSITOR Compositor;
    CRMuralInfo Mural;
} CR_DISPLAY, *PCR_DISPLAY;

int CrDpInit(PCR_DISPLAY pDisplay);
void CrDpTerm(PCR_DISPLAY pDisplay);
void CrDpResize(PCR_DISPLAY pDisplay, uint32_t width, uint32_t height,
        uint32_t stretchedWidth, uint32_t stretchedHeight);
void CrDpEntryInit(PCR_DISPLAY_ENTRY pEntry, const PVBOXVR_TEXTURE pTextureData);
void CrDpEntryCleanup(PCR_DISPLAY pDisplay, PCR_DISPLAY_ENTRY pEntry);
int CrDpEntryRegionsSet(PCR_DISPLAY pDisplay, PCR_DISPLAY_ENTRY pEntry, const RTPOINT *pPos, uint32_t cRegions, const RTRECT *paRegions);
int CrDpEntryRegionsAdd(PCR_DISPLAY pDisplay, PCR_DISPLAY_ENTRY pEntry, const RTPOINT *pPos, uint32_t cRegions, const RTRECT *paRegions);
int CrDpPresentEntry(PCR_DISPLAY pDisplay, PCR_DISPLAY_ENTRY pEntry);
DECLINLINE(bool) CrDpEntryIsUsed(PCR_DISPLAY_ENTRY pEntry)
{
    return CrVrScrCompositorEntryIsInList(&pEntry->CEntry);
}

DECLINLINE(CRMuralInfo*) CrDpGetMural(PCR_DISPLAY pDisplay)
{
    return &pDisplay->Mural;
}

typedef struct CR_DISPLAY_ENTRY_MAP
{
    CRHashTable * pTextureMap;
} CR_DISPLAY_ENTRY_MAP, *PCR_DISPLAY_ENTRY_MAP;

int CrDemInit(PCR_DISPLAY_ENTRY_MAP pMap);
void CrDemTerm(PCR_DISPLAY_ENTRY_MAP pMap);
PCR_DISPLAY_ENTRY CrDemEntryGetCreate(PCR_DISPLAY_ENTRY_MAP pMap, GLuint idTexture, CRContextInfo *pCtxInfo);
void CrDemEntryDestroy(PCR_DISPLAY_ENTRY_MAP pMap, GLuint idTexture);

/* */

/* helpers */

void CrHlpFreeTexImage(CRContext *pCurCtx, GLuint idPBO, void *pvData);
void* CrHlpGetTexImage(CRContext *pCurCtx, PVBOXVR_TEXTURE pTexture, GLuint idPBO);

/* */

typedef struct {
    unsigned short tcpip_port;

    CRScreenInfo screen[CR_MAX_GUEST_MONITORS];
    CRScreenViewportInfo screenVieport[CR_MAX_GUEST_MONITORS];
    int          screenCount;

    int numClients;
    CRClient *clients[CR_MAX_CLIENTS];  /**< array [numClients] */
    CRClient *curClient;
    CRClientNode *pCleanupClient;  /*list of clients with pending clean up*/
    CRCurrentStatePointers current;

    GLboolean firstCallCreateContext;
    GLboolean firstCallMakeCurrent;
    GLboolean bIsInLoadingState; /* Indicates if we're in process of loading VM snapshot */
    GLboolean bIsInSavingState; /* Indicates if we're in process of saving VM snapshot */
    GLboolean bForceMakeCurrentOnClientSwitch;
    CRContextInfo *currentCtxInfo;
    GLint currentWindow;
    GLint currentNativeWindow;
    CRMuralInfo *currentMural;

    CRHashTable *muralTable;  /**< hash table where all murals are stored */

    int client_spu_id;

    int mtu;
    int buffer_size;
    char protocol[1024];

    SPU *head_spu;
    SPUDispatchTable dispatch;

    CRNetworkPointer return_ptr;
    CRNetworkPointer writeback_ptr;

    CRLimitsState limits; /**< GL limits for any contexts we create */

    CRContextInfo MainContextInfo;

    CRHashTable *contextTable;  /**< hash table for rendering contexts */

    CRHashTable *programTable;  /**< for vertex programs */
    GLuint currentProgram;

    /* visBits -> dummy mural association */
    CRHashTable *dummyMuralTable;

    /** configuration options */
    /*@{*/
    int useL2;
    int ignore_papi;
    unsigned int maxBarrierCount;
    unsigned int clearCount;
    int optimizeBucket;
    int only_swap_once;
    int debug_barriers;
    int sharedDisplayLists;
    int sharedTextureObjects;
    int sharedPrograms;
    int sharedWindows;
    int uniqueWindows;
    int localTileSpec;
    int useDMX;
    int overlapBlending;
    int vpProjectionMatrixParameter;
    const char *vpProjectionMatrixVariable;
    int stereoView;
    int vncMode;   /* cmd line option */
    /*@}*/
    /** view_matrix config */
    /*@{*/
    GLboolean viewOverride;
    CRmatrix viewMatrix[2];  /**< left and right eye */
    /*@}*/
    /** projection_matrix config */
    /*@{*/
    GLboolean projectionOverride;
    CRmatrix projectionMatrix[2];  /**< left and right eye */
    int currentEye;
    /*@}*/

    /** for warped tiles */
    /*@{*/
    GLfloat alignment_matrix[16], unnormalized_alignment_matrix[16];
    /*@}*/

    /** tile overlap/blending info - this should probably be per-mural */
    /*@{*/
    CRPoly **overlap_geom;
    CRPoly *overlap_knockout;
    float *overlap_intens;
    int num_overlap_intens;
    int num_overlap_levels;
    /*@}*/

    CRHashTable *barriers, *semaphores;

    RunQueue *run_queue;

    GLuint currentSerialNo;

    PFNCRSERVERPRESENTFBO pfnPresentFBO;
    GLubyte               bForceOffscreenRendering; /*Force server to render 3d data offscreen
                                                     *using callback above to update vbox framebuffers*/
    GLubyte               bOffscreenRenderingDefault; /*can be set with CR_SERVER_DEFAULT_RENDER_TYPE*/
    GLboolean             bUsePBOForReadback;       /*Use PBO's for data readback*/

    GLboolean             bUseOutputRedirect;       /* Whether the output redirect was set. */
    CROutputRedirect      outputRedirect;

    GLboolean             bUseMultipleContexts;

    /* @todo: should we use just one blitter?
     * we use two currently because the drawable attribs can differ*/
    CR_DISPLAY_ENTRY_MAP  PresentTexturepMap;
    uint32_t              DisplaysInitMap[(CR_MAX_GUEST_MONITORS + 31)/32];
    CR_DISPLAY            aDispplays[CR_MAX_GUEST_MONITORS];
} CRServer;


extern DECLEXPORT(void) crServerInit( int argc, char *argv[] );
extern DECLEXPORT(int)  CRServerMain( int argc, char *argv[] );
extern DECLEXPORT(void) crServerServiceClients(void);
extern DECLEXPORT(void) crServerAddNewClient(void);
extern DECLEXPORT(SPU*) crServerHeadSPU(void);
extern DECLEXPORT(void) crServerSetPort(int port);

extern DECLEXPORT(GLboolean) crVBoxServerInit(void);
extern DECLEXPORT(void) crVBoxServerTearDown(void);
extern DECLEXPORT(int32_t) crVBoxServerAddClient(uint32_t u32ClientID);
extern DECLEXPORT(void) crVBoxServerRemoveClient(uint32_t u32ClientID);
extern DECLEXPORT(int32_t) crVBoxServerClientWrite(uint32_t u32ClientID, uint8_t *pBuffer, uint32_t cbBuffer);
extern DECLEXPORT(int32_t) crVBoxServerClientRead(uint32_t u32ClientID, uint8_t *pBuffer, uint32_t *pcbBuffer);
extern DECLEXPORT(int32_t) crVBoxServerClientSetVersion(uint32_t u32ClientID, uint32_t vMajor, uint32_t vMinor);
extern DECLEXPORT(int32_t) crVBoxServerClientSetPID(uint32_t u32ClientID, uint64_t pid);

extern DECLEXPORT(int32_t) crVBoxServerSaveState(PSSMHANDLE pSSM);
extern DECLEXPORT(int32_t) crVBoxServerLoadState(PSSMHANDLE pSSM, uint32_t version);

extern DECLEXPORT(int32_t) crVBoxServerSetScreenCount(int sCount);
extern DECLEXPORT(int32_t) crVBoxServerUnmapScreen(int sIndex);
extern DECLEXPORT(int32_t) crVBoxServerMapScreen(int sIndex, int32_t x, int32_t y, uint32_t w, uint32_t h, uint64_t winID);

extern DECLEXPORT(int32_t) crVBoxServerSetRootVisibleRegion(GLint cRects, GLint *pRects);

extern DECLEXPORT(void) crVBoxServerSetPresentFBOCB(PFNCRSERVERPRESENTFBO pfnPresentFBO);

extern DECLEXPORT(int32_t) crVBoxServerSetOffscreenRendering(GLboolean value);

extern DECLEXPORT(int32_t) crVBoxServerOutputRedirectSet(const CROutputRedirect *pCallbacks);

extern DECLEXPORT(int32_t) crVBoxServerSetScreenViewport(int sIndex, int32_t x, int32_t y, uint32_t w, uint32_t h);

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
extern DECLEXPORT(int32_t) crVBoxServerCrHgsmiCmd(struct VBOXVDMACMD_CHROMIUM_CMD *pCmd, uint32_t cbCmd);
extern DECLEXPORT(int32_t) crVBoxServerCrHgsmiCtl(struct VBOXVDMACMD_CHROMIUM_CTL *pCtl, uint32_t cbCtl);
#endif

#ifdef __cplusplus
}
#endif

#endif

