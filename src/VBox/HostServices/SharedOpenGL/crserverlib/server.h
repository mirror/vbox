/* Copyright (c) 2001, Stanford University
 * All rights reserved.
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#ifndef CR_SERVER_H
#define CR_SERVER_H

#include "cr_protocol.h"
#include "cr_glstate.h"
#include "spu_dispatch_table.h"

#include "state/cr_currentpointers.h"

#include "cr_server.h"

#ifdef VBOX_WITH_CRHGSMI
# include <VBox/VBoxVideo.h>

#include <iprt/cdefs.h>

RT_C_DECLS_BEGIN

extern uint8_t* g_pvVRamBase;
extern uint32_t g_cbVRam;
extern HCRHGSMICMDCOMPLETION g_hCrHgsmiCompletion;
extern PFNCRHGSMICMDCOMPLETION g_pfnCrHgsmiCompletion;

#define VBOXCRHGSMI_PTR(_off, _t) ((_t*)(g_pvVRamBase + (_off)))
#define VBOXCRHGSMI_PTR_SAFE(_off, _cb, _t) ((_t*)crServerCrHgsmiPtrGet(_off, _cb))

DECLINLINE(void*) crServerCrHgsmiPtrGet(VBOXVIDEOOFFSET offBuffer, uint32_t cbBuffer)
{
    return ((offBuffer) + (cbBuffer) <= g_cbVRam ? VBOXCRHGSMI_PTR(offBuffer, void) : NULL);
}

DECLINLINE(void) crServerCrHgsmiCmdComplete(struct VBOXVDMACMD_CHROMIUM_CMD *pCmd, int cmdProcessingRc)
{
    g_pfnCrHgsmiCompletion(g_hCrHgsmiCompletion, pCmd, cmdProcessingRc);
}

#define VBOXCRHGSMI_CMD_COMPLETE(_pData, _rc) do { \
        CRVBOXHGSMI_CMDDATA_ASSERT_ISSET(_pData); \
        CRVBOXHGSMI_CMDDATA_RC(_pData, _rc); \
        crServerCrHgsmiCmdComplete((_pData)->pCmd, VINF_SUCCESS); \
    } while (0)

#define VBOXCRHGSMI_CMD_CHECK_COMPLETE(_pData, _rc) do { \
        if (CRVBOXHGSMI_CMDDATA_IS_SET(_pData)) { \
            VBOXCRHGSMI_CMD_COMPLETE(_pData, _rc); \
        } \
    } while (0)

#endif

/*
 * This is the base number for window and context IDs
 */
#define MAGIC_OFFSET 5000

extern CRServer cr_server;

/* Semaphore wait queue node */
typedef struct _wqnode {
    RunQueue *q;
    struct _wqnode *next;
} wqnode;

typedef struct {
    GLuint count;
    GLuint num_waiting;
    RunQueue **waiting;
} CRServerBarrier;

typedef struct {
    GLuint count;
    wqnode *waiting, *tail;
} CRServerSemaphore;

typedef struct {
    GLuint id;
    GLint projParamStart;
    GLfloat projMat[16];  /* projection matrix, accumulated via calls to */
                        /* glProgramLocalParameterARB, glProgramParameterNV */
} CRServerProgram;

void crServerSetVBoxConfiguration();
void crServerSetVBoxConfigurationHGCM();
void crServerInitDispatch(void);
void crServerReturnValue( const void *payload, unsigned int payload_len );
void crServerWriteback(void);
int crServerRecv( CRConnection *conn, CRMessage *msg, unsigned int len );
void crServerSerializeRemoteStreams(void);
void crServerAddToRunQueue( CRClient *client );
void crServerDeleteClient( CRClient *client );


void crServerApplyBaseProjection( const CRmatrix *baseProj );
void crServerApplyViewMatrix( const CRmatrix *view );
void crServerSetOutputBounds( const CRMuralInfo *mural, int extNum );
void crServerComputeViewportBounds( const CRViewportState *v, CRMuralInfo *mural );

GLboolean crServerInitializeBucketing(CRMuralInfo *mural);

void crComputeOverlapGeom(double *quads, int nquad, CRPoly ***res);
void crComputeKnockoutGeom(double *quads, int nquad, int my_quad_idx, CRPoly **res);

int crServerGetCurrentEye(void);

GLboolean crServerClientInBeginEnd(const CRClient *client);

GLint crServerDispatchCreateContextEx(const char *dpyName, GLint visualBits, GLint shareCtx, GLint preloadCtxID, int32_t internalID);
GLint crServerDispatchWindowCreateEx(const char *dpyName, GLint visBits, GLint preloadWinID);
GLint crServerMuralInit(CRMuralInfo *mural, const char *dpyName, GLint visBits, GLint preloadWinID, GLboolean fUseDefaultDEntry);
void crServerMuralTerm(CRMuralInfo *mural);
GLboolean crServerMuralSize(CRMuralInfo *mural, GLint width, GLint height);
void crServerMuralPosition(CRMuralInfo *mural, GLint x, GLint y, GLboolean fSkipCheckGeometry);
void crServerMuralVisibleRegion( CRMuralInfo *mural, GLint cRects, const GLint *pRects );
void crServerMuralShow( CRMuralInfo *mural, GLint state );
int crServerMuralSynchRootVr(CRMuralInfo *mural, bool *pfChanged);

GLint crServerGenerateID(GLint *pCounter);

GLint crServerSPUWindowID(GLint serverWindow);

GLuint crServerTranslateProgramID(GLuint id);

CRMuralInfo * crServerGetDummyMural(GLint visualBits);

void crServerPresentOutputRedirect(CRMuralInfo *pMural);
void crServerOutputRedirectCheckEnableDisable(CRMuralInfo *pMural);

void crServerCheckMuralGeometry(CRMuralInfo *mural);
GLboolean crServerSupportRedirMuralFBO(void);

void crVBoxServerNotifyEvent(int32_t idScreen, uint32_t uEvent, void*pvData);
void crVBoxServerCheckVisibilityEvent(int32_t idScreen);

void crServerDisplayTermAll();

void crServerWindowSize(CRMuralInfo *pMural);
void crServerWindowShow(CRMuralInfo *pMural);
void crServerWindowVisibleRegion(CRMuralInfo *pMural);
void crServerWindowReparent(CRMuralInfo *pMural);

void crServerWindowSetIsVisible(CRMuralInfo *pMural, GLboolean fIsVisible);
void crServerWindowCheckIsVisible(CRMuralInfo *pMural);

int crVBoxServerUpdateMuralRootVisibleRegion(CRMuralInfo *pMI);

#define CR_SERVER_REDIR_F_NONE     0x00
/* the data should be displayed on host (unset when is on or when CR_SERVER_REDIR_F_FBO_RAM_VMFB is set) */
#define CR_SERVER_REDIR_F_DISPLAY       0x01
/* guest window data get redirected to FBO on host */
#define CR_SERVER_REDIR_F_FBO           0x02
/* used with CR_SERVER_REDIR_F_FBO only
 * indicates that FBO data should be copied to RAM for further processing */
#define CR_SERVER_REDIR_F_FBO_RAM       0x04
/* used with CR_SERVER_REDIR_F_FBO_RAM only
 * indicates that FBO data should be passed to VRDP backend */
#define CR_SERVER_REDIR_F_FBO_RAM_VRDP  0x08
/* used with CR_SERVER_REDIR_F_FBO_RAM only
 * indicates that FBO data should be passed to VM Framebuffer */
#define CR_SERVER_REDIR_F_FBO_RAM_VMFB  0x10
/* used with CR_SERVER_REDIR_F_FBO_RAM only
 * makes the RPW (Read Pixels Worker) mechanism to be used for GPU memory aquisition */
#define CR_SERVER_REDIR_F_FBO_RPW       0x20


#define CR_SERVER_REDIR_F_ALL           0x3f

#define CR_SERVER_REDIR_FGROUP_REQUIRE_FBO     (CR_SERVER_REDIR_F_ALL & ~CR_SERVER_REDIR_F_DISPLAY)
#define CR_SERVER_REDIR_FGROUP_REQUIRE_FBO_RAM (CR_SERVER_REDIR_F_FBO_RAM_VRDP | CR_SERVER_REDIR_F_FBO_RAM_VMFB | CR_SERVER_REDIR_F_FBO_RPW)

DECLINLINE(GLuint) crServerRedirModeAdjust(GLuint value)
{
    /* sanitize values */
    value &= CR_SERVER_REDIR_F_ALL;

    if (value & CR_SERVER_REDIR_FGROUP_REQUIRE_FBO)
        value |=  CR_SERVER_REDIR_F_FBO;
    if (value & CR_SERVER_REDIR_FGROUP_REQUIRE_FBO_RAM)
        value |=  CR_SERVER_REDIR_F_FBO_RAM;

    return value;
}

int32_t crServerSetOffscreenRenderingMode(GLuint value);
void crServerRedirMuralFBO(CRMuralInfo *mural, GLuint redir);
void crServerDeleteMuralFBO(CRMuralInfo *mural);
void crServerPresentFBO(CRMuralInfo *mural);
GLboolean crServerIsRedirectedToFBO();
GLint crServerMuralFBOIdxFromBufferName(CRMuralInfo *mural, GLenum buffer);
void crServerMuralFBOSwapBuffers(CRMuralInfo *mural);

void crServerVBoxCompositionDisableEnter(CRMuralInfo *mural);
void crServerVBoxCompositionDisableLeave(CRMuralInfo *mural, GLboolean fForcePresentOnEnabled);
void crServerVBoxCompositionPresent(CRMuralInfo *mural);
DECLINLINE(GLboolean) crServerVBoxCompositionPresentNeeded(CRMuralInfo *mural)
{
    return mural->bVisible
                && mural->width
                && mural->height
                && !mural->fRootVrOn ? !CrVrScrCompositorIsEmpty(&mural->Compositor) : !CrVrScrCompositorIsEmpty(&mural->RootVrCompositor);
}

#define CR_SERVER_FBO_BB_IDX(_mural) ((_mural)->iBbBuffer)
#define CR_SERVER_FBO_FB_IDX(_mural) (((_mural)->iBbBuffer + 1) % ((_mural)->cBuffers))
/* returns a valid index to be used for negative _idx, i.e. for GL_NONE cases */
//#define CR_SERVER_FBO_ADJUST_IDX(_mural, _idx) ((_idx) >= 0 ? (_idx) : CR_SERVER_FBO_BB_IDX(_mural))
/* just a helper that uses CR_SERVER_FBO_ADJUST_IDX for getting mural's FBO id for buffer index*/
//#define CR_SERVER_FBO_FOR_IDX(_mural, _idx) ((_mural)->aidFBOs[CR_SERVER_FBO_ADJUST_IDX((_mural), (_idx))])
//#define CR_SERVER_FBO_TEX_FOR_IDX(_mural, _idx) ((_mural)->aidColorTexs[CR_SERVER_FBO_ADJUST_IDX((_mural), (_idx))])
#define CR_SERVER_FBO_FOR_IDX(_mural, _idx) ((_idx) >= 0 ? (_mural)->aidFBOs[(_idx)] : 0)
#define CR_SERVER_FBO_TEX_FOR_IDX(_mural, _idx) ((_idx) >= 0 ? (_mural)->aidColorTexs[(_idx)] : 0)

int32_t crVBoxServerInternalClientRead(CRClient *pClient, uint8_t *pBuffer, uint32_t *pcbBuffer);

PCR_DISPLAY crServerDisplayGetInitialized(uint32_t idScreen);

void crServerPerformMakeCurrent( CRMuralInfo *mural, CRContextInfo *ctxInfo );

PCR_BLITTER crServerVBoxBlitterGet();

DECLINLINE(void) crServerVBoxBlitterWinInit(CR_BLITTER_WINDOW *win, CRMuralInfo *mural)
{
    win->Base.id = mural->spuWindow;
    win->Base.visualBits = mural->CreateInfo.visualBits;
    win->width = mural->width;
    win->height = mural->height;
}

DECLINLINE(void) crServerVBoxBlitterCtxInit(CR_BLITTER_CONTEXT *ctx, CRContextInfo *ctxInfo)
{
    ctx->Base.id = ctxInfo->SpuContext;
    if (ctx->Base.id < 0)
        ctx->Base.id = cr_server.MainContextInfo.SpuContext;
    ctx->Base.visualBits = cr_server.curClient->currentCtxInfo->CreateInfo.visualBits;
}

/* display worker thread.
 * see comments for CR_SERVER_RPW struct definition in cr_server.h */
DECLINLINE(void) crServerXchgI8(int8_t *pu8Val1, int8_t *pu8Val2)
{
    int8_t tmp;
    tmp = *pu8Val1;
    *pu8Val1 = *pu8Val2;
    *pu8Val2 = tmp;
}

#ifdef DEBUG_misha
# define CR_SERVER_RPW_DEBUG
#endif
/* *
 * _name : Draw, Submitted, Worker, Gpu
 */

#ifdef CR_SERVER_RPW_DEBUG
# define crServerRpwEntryDbgVerify(_pE) crServerRpwEntryDbgDoVerify(_pE)
#else
# define crServerRpwEntryDbgVerify(_pE) do {} while (0)
#endif


#define CR_SERVER_RPW_ENTRY_TEX_IS_VALID(_pEntry, _name) ((_pEntry)->iTex##_name > 0)

#define CR_SERVER_RPW_ENTRY_TEX_INVALIDATE(_pEntry, _name) do { \
        crServerRpwEntryDbgVerify(_pEntry); \
        Assert(CR_SERVER_RPW_ENTRY_TEX_IS_VALID(_pEntry, _name)); \
        (_pEntry)->iTex##_name = -(_pEntry)->iTex##_name; \
        crServerRpwEntryDbgVerify(_pEntry); \
    } while (0)

#define CR_SERVER_RPW_ENTRY_TEX_PROMOTE(_pEntry, _fromName, _toName) do { \
        crServerRpwEntryDbgVerify(_pEntry); \
        Assert(CR_SERVER_RPW_ENTRY_TEX_IS_VALID(_pEntry, _fromName)); \
        Assert(!CR_SERVER_RPW_ENTRY_TEX_IS_VALID(_pEntry, _toName)); \
        crServerXchgI8(&(_pEntry)->iTex##_fromName, &(_pEntry)->iTex##_toName); \
        crServerRpwEntryDbgVerify(_pEntry); \
    } while (0)

#define CR_SERVER_RPW_ENTRY_TEX_XCHG_VALID(_pEntry, _fromName, _toName) do { \
        crServerRpwEntryDbgVerify(_pEntry); \
        Assert(CR_SERVER_RPW_ENTRY_TEX_IS_VALID(_pEntry, _fromName)); \
        Assert(CR_SERVER_RPW_ENTRY_TEX_IS_VALID(_pEntry, _toName)); \
        crServerXchgI8(&(_pEntry)->iTex##_fromName, &(_pEntry)->iTex##_toName); \
        Assert(CR_SERVER_RPW_ENTRY_TEX_IS_VALID(_pEntry, _fromName)); \
        Assert(CR_SERVER_RPW_ENTRY_TEX_IS_VALID(_pEntry, _toName)); \
        crServerRpwEntryDbgVerify(_pEntry); \
    } while (0)


#define CR_SERVER_RPW_ENTRY_TEX_PROMOTE_KEEPVALID(_pEntry, _fromName, _toName) do { \
        crServerRpwEntryDbgVerify(_pEntry); \
        Assert(CR_SERVER_RPW_ENTRY_TEX_IS_VALID(_pEntry, _fromName)); \
        Assert(!CR_SERVER_RPW_ENTRY_TEX_IS_VALID(_pEntry, _toName)); \
        crServerXchgI8(&(_pEntry)->iTex##_fromName, &(_pEntry)->iTex##_toName); \
        (_pEntry)->iTex##_fromName = -(_pEntry)->iTex##_fromName; \
        Assert(CR_SERVER_RPW_ENTRY_TEX_IS_VALID(_pEntry, _fromName)); \
        Assert(CR_SERVER_RPW_ENTRY_TEX_IS_VALID(_pEntry, _toName)); \
        crServerRpwEntryDbgVerify(_pEntry); \
    } while (0)

#define CR_SERVER_RPW_ENTRY_TEX(_pEntry, _name) ((_pEntry)->aidWorkerTexs[(_pEntry)->iTex##_name - 1])

#define CR_SERVER_RPW_ENTRY_PBO_NEXT_ID(_i) (((_i) + 1) % 2)
#define CR_SERVER_RPW_ENTRY_PBO_IS_ACTIVE(_pEntry) ((_pEntry)->iCurPBO >= 0)
#define CR_SERVER_RPW_ENTRY_PBO_CUR(_pEntry) ((_pEntry)->aidPBOs[(_pEntry)->iCurPBO])
#define CR_SERVER_RPW_ENTRY_PBO_COMPLETED(_pEntry) ((_pEntry)->aidPBOs[CR_SERVER_RPW_ENTRY_PBO_NEXT_ID((_pEntry)->iCurPBO)])
#define CR_SERVER_RPW_ENTRY_PBO_FLIP(_pEntry) do { \
        (_pEntry)->iCurPBO = CR_SERVER_RPW_ENTRY_PBO_NEXT_ID((_pEntry)->iCurPBO); \
    } while (0)

#ifdef CR_SERVER_RPW_DEBUG
DECLINLINE(void) crServerRpwEntryDbgDoVerify(CR_SERVER_RPW_ENTRY *pEntry)
{
    int tstMask = 0;
    int8_t iVal;
    Assert(CR_SERVER_RPW_ENTRY_TEX_IS_VALID(pEntry, Draw));

#define CR_VERVER_RPW_ENTRY_DBG_CHECKVAL(_v) do { \
        iVal = RT_ABS(_v); \
        Assert(iVal > 0); \
        Assert(iVal < 5); \
        Assert(!(tstMask & (1 << iVal))); \
        tstMask |= (1 << iVal); \
    } while (0)

    CR_VERVER_RPW_ENTRY_DBG_CHECKVAL(pEntry->iTexDraw);
    CR_VERVER_RPW_ENTRY_DBG_CHECKVAL(pEntry->iTexSubmitted);
    CR_VERVER_RPW_ENTRY_DBG_CHECKVAL(pEntry->iTexWorker);
    CR_VERVER_RPW_ENTRY_DBG_CHECKVAL(pEntry->iTexGpu);
    Assert(tstMask == 0x1E);
}
#endif

DECLINLINE(bool) crServerRpwIsInitialized(const CR_SERVER_RPW *pWorker)
{
    return !!pWorker->ctxId;
}
int crServerRpwInit(CR_SERVER_RPW *pWorker);
int crServerRpwTerm(CR_SERVER_RPW *pWorker);
DECLINLINE(bool) crServerRpwEntryIsInitialized(const CR_SERVER_RPW_ENTRY *pEntry)
{
    return !!pEntry->pfnData;
}
int crServerRpwEntryInit(CR_SERVER_RPW *pWorker, CR_SERVER_RPW_ENTRY *pEntry, uint32_t width, uint32_t height, PFNCR_SERVER_RPW_DATA pfnData);
int crServerRpwEntryCleanup(CR_SERVER_RPW *pWorker, CR_SERVER_RPW_ENTRY *pEntry);
int crServerRpwEntryResize(CR_SERVER_RPW *pWorker, CR_SERVER_RPW_ENTRY *pEntry, uint32_t width, uint32_t height);
int crServerRpwEntrySubmit(CR_SERVER_RPW *pWorker, CR_SERVER_RPW_ENTRY *pEntry);
int crServerRpwEntryWaitComplete(CR_SERVER_RPW *pWorker, CR_SERVER_RPW_ENTRY *pEntry);
int crServerRpwEntryCancel(CR_SERVER_RPW *pWorker, CR_SERVER_RPW_ENTRY *pEntry);
DECLINLINE(void) crServerRpwEntryDrawSettingsToTex(const CR_SERVER_RPW_ENTRY *pEntry, VBOXVR_TEXTURE *pTex)
{
    pTex->width = pEntry->Size.cx;
    pTex->height = pEntry->Size.cy;
    pTex->target = GL_TEXTURE_2D;
    Assert(CR_SERVER_RPW_ENTRY_TEX_IS_VALID(pEntry, Draw));
    pTex->hwid = CR_SERVER_RPW_ENTRY_TEX(pEntry, Draw);
}
/**/

typedef struct CR_SERVER_CTX_SWITCH
{
    GLuint idDrawFBO, idReadFBO;
    CRContext *pNewCtx;
    CRContext *pOldCtx;
} CR_SERVER_CTX_SWITCH;

DECLINLINE(void) crServerCtxSwitchPrepare(CR_SERVER_CTX_SWITCH *pData, CRContext *pNewCtx)
{
    CRMuralInfo *pCurrentMural = cr_server.currentMural;
    CRContextInfo *pCurCtxInfo = cr_server.currentCtxInfo;
    GLuint idDrawFBO, idReadFBO;
    CRContext *pCurCtx = pCurCtxInfo ? pCurCtxInfo->pContext : NULL;

    CRASSERT(pCurCtx == crStateGetCurrent());

    if (pCurrentMural)
    {
        idDrawFBO = CR_SERVER_FBO_FOR_IDX(pCurrentMural, pCurrentMural->iCurDrawBuffer);
        idReadFBO = CR_SERVER_FBO_FOR_IDX(pCurrentMural, pCurrentMural->iCurReadBuffer);
    }
    else
    {
        idDrawFBO = 0;
        idReadFBO = 0;
    }

    crStateSwitchPrepare(pNewCtx, pCurCtx, idDrawFBO, idReadFBO);

    pData->idDrawFBO = idDrawFBO;
    pData->idReadFBO = idReadFBO;
    pData->pNewCtx = pNewCtx;
    pData->pOldCtx = pCurCtx;
}

DECLINLINE(void) crServerCtxSwitchPostprocess(CR_SERVER_CTX_SWITCH *pData)
{
    crStateSwitchPostprocess(pData->pOldCtx, pData->pNewCtx, pData->idDrawFBO, pData->idReadFBO);
}

void crServerInitTmpCtxDispatch();

int crServerVBoxParseNumerics(const char *pszStr, const int defaultVal);

void CrDpRootUpdate(PCR_DISPLAY pDisplay);
void CrDpEnter(PCR_DISPLAY pDisplay);
void CrDpLeave(PCR_DISPLAY pDisplay);
int CrDpInit(PCR_DISPLAY pDisplay);
void CrDpTerm(PCR_DISPLAY pDisplay);

DECLINLINE(bool) CrDpIsEmpty(PCR_DISPLAY pDisplay)
{
    return CrVrScrCompositorIsEmpty(&pDisplay->Mural.Compositor);
}

int CrDpSaveState(PCR_DISPLAY pDisplay, PSSMHANDLE pSSM);
int CrDpLoadState(PCR_DISPLAY pDisplay, PSSMHANDLE pSSM, uint32_t version);

void CrDpReparent(PCR_DISPLAY pDisplay, CRScreenInfo *pScreen);

void CrDpResize(PCR_DISPLAY pDisplay, int32_t xPos, int32_t yPos, uint32_t width, uint32_t height);
void CrDpEntryInit(PCR_DISPLAY_ENTRY pEntry, const VBOXVR_TEXTURE *pTextureData, uint32_t fFlags, PFNVBOXVRSCRCOMPOSITOR_ENTRY_RELEASED pfnEntryReleased);
void CrDpEntryCleanup(PCR_DISPLAY_ENTRY pEntry);
int CrDpEntryRegionsSet(PCR_DISPLAY pDisplay, PCR_DISPLAY_ENTRY pEntry, const RTPOINT *pPos, uint32_t cRegions, const RTRECT *paRegions);
int CrDpEntryRegionsAdd(PCR_DISPLAY pDisplay, PCR_DISPLAY_ENTRY pEntry, const RTPOINT *pPos, uint32_t cRegions, const RTRECT *paRegions, CR_DISPLAY_ENTRY_MAP *pMap);
void CrDpRegionsClear(PCR_DISPLAY pDisplay);
DECLINLINE(bool) CrDpEntryIsUsed(PCR_DISPLAY_ENTRY pEntry)
{
    return CrVrScrCompositorEntryIsInList(&pEntry->CEntry);
}

DECLINLINE(CRMuralInfo*) CrDpGetMural(PCR_DISPLAY pDisplay)
{
    return &pDisplay->Mural;
}

int CrDemGlobalInit();
void CrDemTeGlobalTerm();
void CrDemEnter(PCR_DISPLAY_ENTRY_MAP pMap);
void CrDemLeave(PCR_DISPLAY_ENTRY_MAP pMap, PCR_DISPLAY_ENTRY pNewEntry, PCR_DISPLAY_ENTRY pReplacedEntry);
int CrDemInit(PCR_DISPLAY_ENTRY_MAP pMap);
void CrDemTerm(PCR_DISPLAY_ENTRY_MAP pMap);
PCR_DISPLAY_ENTRY CrDemEntryAcquire(PCR_DISPLAY_ENTRY_MAP pMap, GLuint idTexture, uint32_t fFlags);
void CrDemEntryRelease(PCR_DISPLAY_ENTRY pEntry);
int CrDemEntrySaveState(PCR_DISPLAY_ENTRY pEntry, PSSMHANDLE pSSM);
int CrDemEntryLoadState(PCR_DISPLAY_ENTRY_MAP pMap, PCR_DISPLAY_ENTRY *ppEntry, PSSMHANDLE pSSM);

int crServerDisplaySaveState(PSSMHANDLE pSSM);
int crServerDisplayLoadState(PSSMHANDLE pSSM, uint32_t u32Version);


void crServerDEntryResized(CRMuralInfo *pMural, CR_DISPLAY_ENTRY *pDEntry);
void crServerDEntryMoved(CRMuralInfo *pMural, CR_DISPLAY_ENTRY *pDEntry);
void crServerDEntryVibleRegions(CRMuralInfo *pMural, CR_DISPLAY_ENTRY *pDEntry);
void crServerDEntryCheckFBO(CRMuralInfo *pMural, CR_DISPLAY_ENTRY *pDEntry, CRContext *ctx);

void crServerDEntryAllResized(CRMuralInfo *pMural);
void crServerDEntryAllMoved(CRMuralInfo *pMural);
void crServerDEntryAllVibleRegions(CRMuralInfo *pMural);

//#define VBOX_WITH_CRSERVER_DUMPER
#ifdef VBOX_WITH_CRSERVER_DUMPER
void crServerDumpCheckTerm();
int crServerDumpCheckInit();
void crServerDumpBuffer(int idx);
void crServerDumpTextures();
void crServerDumpTexture(const VBOXVR_TEXTURE *pTex);
void crServerDumpShader(GLint id);
void crServerDumpProgram(GLint id);
void crServerDumpCurrentProgram();
void crServerDumpRecompileDumpCurrentProgram();
void crServerRecompileCurrentProgram();
void crServerDumpCurrentProgramUniforms();
void crServerDumpCurrentProgramAttribs();
void crServerDumpFramesCheck();
void crServerDumpState();
void crServerDumpDrawel(const char*pszFormat, ...);
void crServerDumpDrawelv(GLuint idx, const char*pszElFormat, uint32_t cbEl, const void *pvVal, uint32_t cVal);

extern int64_t g_CrDbgDumpPid;
extern unsigned long g_CrDbgDumpEnabled;
extern unsigned long g_CrDbgDumpDraw;
extern unsigned long g_CrDbgDumpDrawFramesSettings;
extern unsigned long g_CrDbgDumpDrawFramesAppliedSettings;
extern unsigned long g_CrDbgDumpDrawFramesCount;

extern uint32_t g_CrDbgDumpVertattrFixupOn;

bool crServerDumpFilterDmp(unsigned long event, CR_DUMPER *pDumper);
bool crServerDumpFilterOpEnter(unsigned long event, CR_DUMPER *pDumper);
void crServerDumpFilterOpLeave(unsigned long event, CR_DUMPER *pDumper);

//#define CR_SERVER_DUMP_MASK_OP                  0x0000fffc
//#define CR_SERVER_DUMP_OFF_OP                   2
//
//#define CR_SERVER_DUMP_MASK_DIR                 0x00000003
//#define CR_SERVER_DUMP_OFF_DIR                  0
//
//#define CR_SERVER_DUMP_MASK_DMP                 0xffff0000
//#define CR_SERVER_DUMP_OFF_DMP                  16
//
//#define CR_SERVER_DUMP_MAKE_OP(_v)              (1 << ((_v) + CR_SERVER_DUMP_OFF_OP))
//#define CR_SERVER_DUMP_MAKE_DIR(_v)             (1 << ((_v) + CR_SERVER_DUMP_OFF_DIR))
//#define CR_SERVER_DUMP_MAKE_DMP(_v)             (1 << ((_v) + CR_SERVER_DUMP_OFF_DMP))
//
//#define CR_SERVER_DUMP_GET_OP(_v)               ((_v) & CR_SERVER_DUMP_MASK_OP)
//#define CR_SERVER_DUMP_GET_DMP(_v)              ((_v) & CR_SERVER_DUMP_MASK_DMP)
//#define CR_SERVER_DUMP_GET_DIR(_v)              ((_v) & CR_SERVER_DUMP_MASK_DIR)
//
//#define CR_SERVER_DUMP_ISANY_OP(_v1, _v2)       (!!(CR_SERVER_DUMP_GET_OP(_v1) & CR_SERVER_DUMP_GET_OP(_v2)))
//#define CR_SERVER_DUMP_ISANY_DIR(_v1, _v2)      (!!(CR_SERVER_DUMP_GET_DIR(_v1) & CR_SERVER_DUMP_GET_DIR(_v2)))
//#define CR_SERVER_DUMP_ISANY_DMP(_v1, _v2)      (!!(CR_SERVER_DUMP_GET_DMP(_v1) & CR_SERVER_DUMP_GET_DMP(_v2)))
//
//#define CR_SERVER_DUMP_ISANY_OP(_v1, _v2)       ((CR_SERVER_DUMP_GET_OP(_v1) & CR_SERVER_DUMP_GET_OP(_v2)) == CR_SERVER_DUMP_GET_OP(_v2))
//#define CR_SERVER_DUMP_ISANY_DIR(_v1, _v2)      ((CR_SERVER_DUMP_GET_DIR(_v1) & CR_SERVER_DUMP_GET_DIR(_v2)) == CR_SERVER_DUMP_GET_DIR(_v2))
//#define CR_SERVER_DUMP_ISANY_DMP(_v1, _v2)      ((CR_SERVER_DUMP_GET_DMP(_v1) & CR_SERVER_DUMP_GET_DMP(_v2)) == CR_SERVER_DUMP_GET_DMP(_v2))
//
//#define CR_SERVER_DUMP_F_DIR_ENTER              CR_SERVER_DUMP_MAKE_DIR(0)
//#define CR_SERVER_DUMP_F_DIR_LEAVE              CR_SERVER_DUMP_MAKE_DIR(1)
//
//#define CR_SERVER_DUMP_F_OP_DRAW                CR_SERVER_DUMP_MAKE_OP(0)
//#define CR_SERVER_DUMP_F_OP_SWAPBUFFERS         CR_SERVER_DUMP_MAKE_OP(1)
//#define CR_SERVER_DUMP_F_OP_LINK_PROGRAM        CR_SERVER_DUMP_MAKE_OP(2)
//#define CR_SERVER_DUMP_F_OP_COMPILE_PROGRAM     CR_SERVER_DUMP_MAKE_OP(3)
//
//#define CR_SERVER_DUMP_F_DMP_BUFF               CR_SERVER_DUMP_MAKE_DMP(0)
//#define CR_SERVER_DUMP_F_DMP_TEX                CR_SERVER_DUMP_MAKE_DMP(0)
//#define CR_SERVER_DUMP_F_DMP_PROGRAM            CR_SERVER_DUMP_MAKE_DMP(0)
//#define CR_SERVER_DUMP_F_DMP_PROGRAM_UNIFORMS   CR_SERVER_DUMP_MAKE_DMP(0)
//#define CR_SERVER_DUMP_F_DMP_STATE              CR_SERVER_DUMP_MAKE_DMP(0)
//
//#define CR_SERVER_DUMP_GET_OP(_v)               ((_v) & CR_SERVER_DUMP_MASK_OP)
//#define CR_SERVER_DUMP_GET_DMP(_v)               ((_v) & CR_SERVER_DUMP_MASK_DMP)
//#define CR_SERVER_DUMP_GET_DIR(_v)               ((_v) & CR_SERVER_DUMP_MASK_DIR)

#define CR_SERVER_DUMP_F_DRAW_BUFF_ENTER        0x00000001
#define CR_SERVER_DUMP_F_DRAW_BUFF_LEAVE        0x00000002
#define CR_SERVER_DUMP_F_DRAW_STATE_ENTER       0x00000004
#define CR_SERVER_DUMP_F_DRAW_STATE_LEAVE       0x00000008
#define CR_SERVER_DUMP_F_DRAW_TEX_ENTER         0x00000010
#define CR_SERVER_DUMP_F_DRAW_TEX_LEAVE         0x00000020
#define CR_SERVER_DUMP_F_DRAW_PROGRAM_ENTER     0x00000040
#define CR_SERVER_DUMP_F_DRAW_PROGRAM_LEAVE     0x00000080
#define CR_SERVER_DUMP_F_DRAW_PROGRAM_UNIFORMS_ENTER     0x00000100
#define CR_SERVER_DUMP_F_DRAW_PROGRAM_UNIFORMS_LEAVE     0x00000200
#define CR_SERVER_DUMP_F_DRAW_PROGRAM_ATTRIBS_ENTER      0x00000400
#define CR_SERVER_DUMP_F_DRAW_PROGRAM_ATTRIBS_LEAVE      0x00000800

#define CR_SERVER_DUMP_F_DRAW_ENTER_ALL (CR_SERVER_DUMP_F_DRAW_BUFF_ENTER \
        | CR_SERVER_DUMP_F_DRAW_TEX_ENTER \
        | CR_SERVER_DUMP_F_DRAW_PROGRAM_ENTER \
        | CR_SERVER_DUMP_F_DRAW_PROGRAM_UNIFORMS_ENTER \
        | CR_SERVER_DUMP_F_DRAW_STATE_ENTER \
        | CR_SERVER_DUMP_F_DRAW_PROGRAM_ATTRIBS_ENTER)

#define CR_SERVER_DUMP_F_DRAW_LEAVE_ALL (CR_SERVER_DUMP_F_DRAW_BUFF_LEAVE \
        | CR_SERVER_DUMP_F_DRAW_TEX_LEAVE \
        | CR_SERVER_DUMP_F_DRAW_PROGRAM_LEAVE \
        | CR_SERVER_DUMP_F_DRAW_PROGRAM_UNIFORMS_LEAVE \
        | CR_SERVER_DUMP_F_DRAW_STATE_LEAVE \
        | CR_SERVER_DUMP_F_DRAW_PROGRAM_ATTRIBS_LEAVE)

#define CR_SERVER_DUMP_F_DRAW_ALL (CR_SERVER_DUMP_F_DRAW_ENTER_ALL | CR_SERVER_DUMP_F_DRAW_LEAVE_ALL)

#define CR_SERVER_DUMP_F_SWAPBUFFERS_ENTER      0x00010000
#define CR_SERVER_DUMP_F_SWAPBUFFERS_LEAVE      0x00020000
#define CR_SERVER_DUMP_F_TEXPRESENT             0x00040000
#define CR_SERVER_DUMP_F_DRAWEL                 0x00100000
#define CR_SERVER_DUMP_F_COMPILE_SHADER         0x01000000
#define CR_SERVER_DUMP_F_SHADER_SOURCE          0x02000000
#define CR_SERVER_DUMP_F_LINK_PROGRAM           0x04000000


#define CR_SERVER_DUMP_DEFAULT_FILTER_OP(_ev) ((((_ev) & g_CrDbgDumpDraw) != 0) \
        || ((_ev) == CR_SERVER_DUMP_F_SWAPBUFFERS_ENTER && g_CrDbgDumpDrawFramesCount))

#define CR_SERVER_DUMP_DEFAULT_FILTER_DMP(_ev) (((_ev) & g_CrDbgDumpDraw) != 0)

#define CR_SERVER_DUMP_FILTER_OP(_ev, _pDumper) (g_CrDbgDumpEnabled \
                && (!g_CrDbgDumpPid \
                        || (g_CrDbgDumpPid > 0 && ((uint64_t)g_CrDbgDumpPid) == cr_server.curClient->pid) \
                        || (g_CrDbgDumpPid < 0 && ((uint64_t)(-g_CrDbgDumpPid)) != cr_server.curClient->pid)) \
                && crServerDumpFilterOpEnter((_ev), (_pDumper)))
#define CR_SERVER_DUMP_FILTER_DMP(_ev, _pDumper) (crServerDumpFilterDmp((_ev), (_pDumper)))

#define CR_SERVER_DUMP_DRAW_ENTER() do { \
            if (!CR_SERVER_DUMP_FILTER_OP(CR_SERVER_DUMP_F_DRAW_ENTER_ALL, cr_server.Recorder.pDumper)) break; \
            crServerDumpCheckInit(); \
            crDmpStrF(cr_server.Recorder.pDumper, "==ENTER[%d] %s==", (uint32_t)cr_server.curClient->pid, __FUNCTION__); \
            if (CR_SERVER_DUMP_FILTER_DMP(CR_SERVER_DUMP_F_DRAW_STATE_ENTER, cr_server.Recorder.pDumper)) { crServerDumpState(); } \
            if (CR_SERVER_DUMP_FILTER_DMP(CR_SERVER_DUMP_F_DRAW_PROGRAM_ENTER, cr_server.Recorder.pDumper)) { crServerDumpCurrentProgram(); } \
            if (CR_SERVER_DUMP_FILTER_DMP(CR_SERVER_DUMP_F_DRAW_PROGRAM_UNIFORMS_ENTER, cr_server.Recorder.pDumper)) { crServerDumpCurrentProgramUniforms(); } \
            if (CR_SERVER_DUMP_FILTER_DMP(CR_SERVER_DUMP_F_DRAW_PROGRAM_ATTRIBS_ENTER, cr_server.Recorder.pDumper)) { crServerDumpCurrentProgramAttribs(); } \
            if (CR_SERVER_DUMP_FILTER_DMP(CR_SERVER_DUMP_F_DRAW_TEX_ENTER, cr_server.Recorder.pDumper)) { crServerDumpTextures(); } \
            if (CR_SERVER_DUMP_FILTER_DMP(CR_SERVER_DUMP_F_DRAW_BUFF_ENTER, cr_server.Recorder.pDumper)) { crServerDumpBuffer(-1); } \
            crDmpStrF(cr_server.Recorder.pDumper, "==Done ENTER[%d] %s==", (uint32_t)cr_server.curClient->pid, __FUNCTION__); \
            crServerDumpFilterOpLeave(CR_SERVER_DUMP_F_DRAW_ENTER_ALL, cr_server.Recorder.pDumper); \
        } while (0)

#define CR_SERVER_DUMP_DRAW_LEAVE() do { \
            if (!CR_SERVER_DUMP_FILTER_OP(CR_SERVER_DUMP_F_DRAW_LEAVE_ALL, cr_server.Recorder.pDumper)) break; \
            crServerDumpCheckInit(); \
            crDmpStrF(cr_server.Recorder.pDumper, "==LEAVE[%d] %s==", (uint32_t)cr_server.curClient->pid, __FUNCTION__); \
            if (CR_SERVER_DUMP_FILTER_DMP(CR_SERVER_DUMP_F_DRAW_TEX_LEAVE, cr_server.Recorder.pDumper)) { crServerDumpTextures(); } \
            if (CR_SERVER_DUMP_FILTER_DMP(CR_SERVER_DUMP_F_DRAW_BUFF_LEAVE, cr_server.Recorder.pDumper)) { crServerDumpBuffer(-1); } \
            if (CR_SERVER_DUMP_FILTER_DMP(CR_SERVER_DUMP_F_DRAW_PROGRAM_UNIFORMS_LEAVE, cr_server.Recorder.pDumper)) { crServerDumpCurrentProgramUniforms(); } \
            if (CR_SERVER_DUMP_FILTER_DMP(CR_SERVER_DUMP_F_DRAW_PROGRAM_ATTRIBS_LEAVE, cr_server.Recorder.pDumper)) { crServerDumpCurrentProgramAttribs(); } \
            if (CR_SERVER_DUMP_FILTER_DMP(CR_SERVER_DUMP_F_DRAW_PROGRAM_LEAVE, cr_server.Recorder.pDumper)) { crServerDumpCurrentProgram(); } \
            if (CR_SERVER_DUMP_FILTER_DMP(CR_SERVER_DUMP_F_DRAW_STATE_LEAVE, cr_server.Recorder.pDumper)) { crServerDumpState(); } \
            crDmpStrF(cr_server.Recorder.pDumper, "==Done LEAVE[%d] %s==", (uint32_t)cr_server.curClient->pid, __FUNCTION__); \
            crServerDumpFilterOpLeave(CR_SERVER_DUMP_F_DRAW_LEAVE_ALL, cr_server.Recorder.pDumper); \
        } while (0)

#define CR_SERVER_DUMP_COMPILE_SHADER(_id) do { \
            if (!CR_SERVER_DUMP_FILTER_OP(CR_SERVER_DUMP_F_COMPILE_SHADER, cr_server.Recorder.pDumper)) break; \
            crServerDumpCheckInit(); \
            crDmpStrF(cr_server.Recorder.pDumper, "==[%d] %s", (uint32_t)cr_server.curClient->pid, __FUNCTION__); \
            crServerDumpShader((_id)); \
            crDmpStrF(cr_server.Recorder.pDumper, "==Done[%d] %s==", (uint32_t)cr_server.curClient->pid, __FUNCTION__); \
            crServerDumpFilterOpLeave(CR_SERVER_DUMP_F_COMPILE_SHADER, cr_server.Recorder.pDumper); \
        } while (0)

#define CR_SERVER_DUMP_SHADER_SOURCE(_id) do { \
            if (!CR_SERVER_DUMP_FILTER_OP(CR_SERVER_DUMP_F_SHADER_SOURCE, cr_server.Recorder.pDumper)) break; \
            crServerDumpCheckInit(); \
            crDmpStrF(cr_server.Recorder.pDumper, "==[%d] %s==", (uint32_t)cr_server.curClient->pid, __FUNCTION__); \
            crServerDumpShader((_id)); \
            crDmpStrF(cr_server.Recorder.pDumper, "==Done[%d] %s==", (uint32_t)cr_server.curClient->pid, __FUNCTION__); \
            crServerDumpFilterOpLeave(CR_SERVER_DUMP_F_SHADER_SOURCE, cr_server.Recorder.pDumper); \
        } while (0)

#define CR_SERVER_DUMP_LINK_PROGRAM(_id) do { \
            if (!CR_SERVER_DUMP_FILTER_OP(CR_SERVER_DUMP_F_LINK_PROGRAM, cr_server.Recorder.pDumper)) break; \
            crServerDumpCheckInit(); \
            crDmpStrF(cr_server.Recorder.pDumper, "==[%d] %s==", (uint32_t)cr_server.curClient->pid, __FUNCTION__); \
            crServerDumpProgram((_id)); \
            crDmpStrF(cr_server.Recorder.pDumper, "==Done[%d] %s==", (uint32_t)cr_server.curClient->pid, __FUNCTION__); \
            crServerDumpFilterOpLeave(CR_SERVER_DUMP_F_LINK_PROGRAM, cr_server.Recorder.pDumper); \
        } while (0)

#define CR_SERVER_DUMP_SWAPBUFFERS_ENTER() do { \
            if (!CR_SERVER_DUMP_FILTER_OP(CR_SERVER_DUMP_F_SWAPBUFFERS_ENTER, cr_server.Recorder.pDumper)) break; \
            crServerDumpCheckInit(); \
            crDmpStrF(cr_server.Recorder.pDumper, "==ENTER[%d] %s==", (uint32_t)cr_server.curClient->pid, __FUNCTION__); \
            if (CR_SERVER_DUMP_FILTER_DMP(CR_SERVER_DUMP_F_SWAPBUFFERS_ENTER, cr_server.Recorder.pDumper)) { crServerDumpBuffer(CR_SERVER_FBO_BB_IDX(cr_server.currentMural)); } \
            if (g_CrDbgDumpDrawFramesCount) { crServerDumpFramesCheck(); } \
            crDmpStrF(cr_server.Recorder.pDumper, "==Done ENTER[%d] %s==", (uint32_t)cr_server.curClient->pid, __FUNCTION__); \
            crServerDumpFilterOpLeave(CR_SERVER_DUMP_F_SWAPBUFFERS_ENTER, cr_server.Recorder.pDumper); \
        } while (0)

#define CR_SERVER_DUMP_TEXPRESENT(_pTex) do { \
            if (!CR_SERVER_DUMP_FILTER_OP(CR_SERVER_DUMP_F_TEXPRESENT, cr_server.Recorder.pDumper)) break; \
            crServerDumpCheckInit(); \
            crDmpStrF(cr_server.Recorder.pDumper, "==[%d] %s==", (uint32_t)cr_server.curClient->pid, __FUNCTION__); \
            crServerDumpTexture((_pTex)); \
            crServerDumpFilterOpLeave(CR_SERVER_DUMP_F_TEXPRESENT, cr_server.Recorder.pDumper); \
        } while (0)

#define CR_SERVER_DUMP_SWAPBUFFERS_LEAVE() do { \
            if (!CR_SERVER_DUMP_FILTER_OP(CR_SERVER_DUMP_F_SWAPBUFFERS_LEAVE, cr_server.Recorder.pDumper)) break; \
            crDmpStrF(cr_server.Recorder.pDumper, "==LEAVE[%d] %s==", (uint32_t)cr_server.curClient->pid, __FUNCTION__); \
            crServerDumpCheckInit(); \
            crDmpStrF(cr_server.Recorder.pDumper, "==Done LEAVE[%d] %s==", (uint32_t)cr_server.curClient->pid, __FUNCTION__); \
            crServerDumpFilterOpLeave(CR_SERVER_DUMP_F_SWAPBUFFERS_LEAVE, cr_server.Recorder.pDumper); \
        } while (0)

#define CR_SERVER_DUMP_DRAWEL_F(_msg) do { \
        if (!CR_SERVER_DUMP_FILTER_OP(CR_SERVER_DUMP_F_DRAWEL, cr_server.Recorder.pDumper)) break; \
        crServerDumpCheckInit(); \
        crDmpStrF(cr_server.Recorder.pDumper, "==[%d] %s==", (uint32_t)cr_server.curClient->pid, __FUNCTION__); \
        crServerDumpDrawel _msg; \
        crServerDumpFilterOpLeave(CR_SERVER_DUMP_F_DRAWEL, cr_server.Recorder.pDumper); \
    } while (0)

#define CR_SERVER_DUMP_DRAWEL_V(_index, _pszElFormat, _cbEl, _pvVal, _cVal) do { \
        if (!CR_SERVER_DUMP_FILTER_OP(CR_SERVER_DUMP_F_DRAWEL, cr_server.Recorder.pDumper)) break; \
        crServerDumpCheckInit(); \
        crDmpStrF(cr_server.Recorder.pDumper, "==[%d] %s==", (uint32_t)cr_server.curClient->pid, __FUNCTION__); \
        crServerDumpDrawelv((_index), (_pszElFormat), (_cbEl), (_pvVal), (_cVal)); \
        crServerDumpFilterOpLeave(CR_SERVER_DUMP_F_DRAWEL, cr_server.Recorder.pDumper); \
    } while (0)
#else /* if !defined VBOX_WITH_CRSERVER_DUMPER */
#define CR_SERVER_DUMP_DRAW_ENTER() do {} while (0)
#define CR_SERVER_DUMP_DRAW_LEAVE() do {} while (0)
#define CR_SERVER_DUMP_COMPILE_SHADER(_id) do {} while (0)
#define CR_SERVER_DUMP_LINK_PROGRAM(_id) do {} while (0)
#define CR_SERVER_DUMP_TEXPRESENT(_pTex) do {} while (0)
#define CR_SERVER_DUMP_SWAPBUFFERS_ENTER() do {} while (0)
#define CR_SERVER_DUMP_SWAPBUFFERS_LEAVE() do {} while (0)
#define CR_SERVER_DUMP_SHADER_SOURCE(_id) do {} while (0)
#define CR_SERVER_DUMP_DRAWEL_F(_msg) do {} while (0)
#define CR_SERVER_DUMP_DRAWEL_V(_index, _pszElFormat, _cbEl, _pvVal, _cVal) do {} while (0)
#endif /* !VBOX_WITH_CRSERVER_DUMPER */

RT_C_DECLS_END

#endif /* CR_SERVER_H */
