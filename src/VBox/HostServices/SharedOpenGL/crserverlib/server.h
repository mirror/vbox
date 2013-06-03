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
GLint crServerMuralInit(CRMuralInfo *mural, const char *dpyName, GLint visBits, GLint preloadWinID);
void crServerMuralTerm(CRMuralInfo *mural);
void crServerMuralSize(CRMuralInfo *mural, GLint width, GLint height);
int crServerMuralSynchRootVr(CRMuralInfo *mural, uint32_t *pcRects, const RTRECT **ppRects);

GLint crServerGenerateID(GLint *pCounter);

GLint crServerSPUWindowID(GLint serverWindow);

GLuint crServerTranslateProgramID(GLuint id);

CRMuralInfo * crServerGetDummyMural(GLint visualBits);

void crServerSetupOutputRedirect(CRMuralInfo *mural);
void crServerCheckMuralGeometry(CRMuralInfo *mural);
GLboolean crServerSupportRedirMuralFBO(void);

void crVBoxServerNotifyEvent(int32_t idScreen);

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
                && !mural->fRootVrOn ? CrVrScrCompositorEntryIsInList(&mural->CEntry) : CrVrScrCompositorEntryIsInList(&mural->RootVrCEntry);
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

//#define VBOX_WITH_CRSERVER_DUMPER
#ifdef VBOX_WITH_CRSERVER_DUMPER
void crServerDumpCheckTerm();
int crServerDumpCheckInit();
void crServerDumpBuffer(int idx);
void crServerDumpTextures();
void crServerDumpShader(GLint id);
void crServerDumpProgram(GLint id);
void crServerDumpCurrentProgram();
void crServerDumpFramesCheck();

extern unsigned long g_CrDbgDumpDraw;
extern unsigned long g_CrDbgDumpDrawFramesSettings;
extern unsigned long g_CrDbgDumpDrawFramesAppliedSettings;
extern unsigned long g_CrDbgDumpDrawFramesCount;
bool crServerDumpFilter(unsigned long event);

#define CR_SERVER_DUMP_F_DRAW_BUFF_ENTER        0x00000001
#define CR_SERVER_DUMP_F_DRAW_BUFF_LEAVE        0x00000002
#define CR_SERVER_DUMP_F_DRAW_TEX_ENTER         0x00000010
#define CR_SERVER_DUMP_F_DRAW_TEX_LEAVE         0x00000020
#define CR_SERVER_DUMP_F_DRAW_PROGRAM_ENTER     0x00000100
#define CR_SERVER_DUMP_F_DRAW_PROGRAM_LEAVE     0x00000200

#define CR_SERVER_DUMP_F_DRAW_ALL (CR_SERVER_DUMP_F_DRAW_BUFF_ENTER | CR_SERVER_DUMP_F_DRAW_BUFF_LEAVE \
        | CR_SERVER_DUMP_F_DRAW_TEX_ENTER | CR_SERVER_DUMP_F_DRAW_TEX_LEAVE \
        | CR_SERVER_DUMP_F_DRAW_PROGRAM_ENTER | CR_SERVER_DUMP_F_DRAW_PROGRAM_LEAVE)

#define CR_SERVER_DUMP_F_COMPILE_SHADER         0x00001000
#define CR_SERVER_DUMP_F_LINK_PROGRAM           0x00002000
#define CR_SERVER_DUMP_F_SWAPBUFFERS_ENTER      0x00010000
#define CR_SERVER_DUMP_F_SWAPBUFFERS_LEAVE      0x00020000

#define CR_SERVER_DUMP_IF_ANY(_ev) ((g_CrDbgDumpDraw & (_ev)) && crServerDumpFilter((_ev)))

#define CR_SERVER_DUMP_DRAW_ENTER() do { \
            if (!CR_SERVER_DUMP_IF_ANY(CR_SERVER_DUMP_F_DRAW_BUFF_ENTER | CR_SERVER_DUMP_F_DRAW_TEX_ENTER | CR_SERVER_DUMP_F_DRAW_PROGRAM_ENTER)) break; \
            crServerDumpCheckInit(); \
            crDmpStrF(cr_server.Recorder.pDumper, "==> %s", __FUNCTION__); \
            if (CR_SERVER_DUMP_IF_ANY(CR_SERVER_DUMP_F_DRAW_PROGRAM_ENTER)) { crServerDumpCurrentProgram(); } \
            if (CR_SERVER_DUMP_IF_ANY(CR_SERVER_DUMP_F_DRAW_TEX_ENTER)) { crServerDumpTextures(); } \
            if (CR_SERVER_DUMP_IF_ANY(CR_SERVER_DUMP_F_DRAW_BUFF_ENTER)) { crServerDumpBuffer(-1); } \
            crDmpStrF(cr_server.Recorder.pDumper, "=================="); \
        } while (0)

#define CR_SERVER_DUMP_DRAW_LEAVE() do { \
            if (!CR_SERVER_DUMP_IF_ANY(CR_SERVER_DUMP_F_DRAW_BUFF_LEAVE | CR_SERVER_DUMP_F_DRAW_TEX_LEAVE | CR_SERVER_DUMP_F_DRAW_PROGRAM_LEAVE)) break; \
            crServerDumpCheckInit(); \
            crDmpStrF(cr_server.Recorder.pDumper, "<== %s", __FUNCTION__); \
            if (CR_SERVER_DUMP_IF_ANY(CR_SERVER_DUMP_F_DRAW_BUFF_LEAVE)) { crServerDumpBuffer(-1); } \
            if (CR_SERVER_DUMP_IF_ANY(CR_SERVER_DUMP_F_DRAW_TEX_LEAVE)) { crServerDumpTextures(); } \
            if (CR_SERVER_DUMP_IF_ANY(CR_SERVER_DUMP_F_DRAW_PROGRAM_LEAVE)) { crServerDumpCurrentProgram(); } \
            crDmpStrF(cr_server.Recorder.pDumper, "=================="); \
        } while (0)

#define CR_SERVER_DUMP_COMPILE_SHADER(_id) do { \
            if (!CR_SERVER_DUMP_IF_ANY(CR_SERVER_DUMP_F_COMPILE_SHADER)) break; \
            crServerDumpCheckInit(); \
            crServerDumpShader((_id)); \
        } while (0)

#define CR_SERVER_DUMP_LINK_PROGRAM(_id) do { \
            if (!CR_SERVER_DUMP_IF_ANY(CR_SERVER_DUMP_F_LINK_PROGRAM)) break; \
            crServerDumpCheckInit(); \
            crServerDumpProgram((_id)); \
        } while (0)

#define CR_SERVER_DUMP_SWAPBUFFERS_ENTER() do { \
            if (!g_CrDbgDumpDrawFramesCount && !CR_SERVER_DUMP_IF_ANY(CR_SERVER_DUMP_F_SWAPBUFFERS_ENTER)) break; \
            crServerDumpCheckInit(); \
            crDmpStrF(cr_server.Recorder.pDumper, "== %s", __FUNCTION__); \
            if (CR_SERVER_DUMP_IF_ANY(CR_SERVER_DUMP_F_SWAPBUFFERS_ENTER)) { crServerDumpBuffer(CR_SERVER_FBO_BB_IDX(cr_server.currentMural)); } \
            if (g_CrDbgDumpDrawFramesCount) { crServerDumpFramesCheck(); } \
        } while (0)

#define CR_SERVER_DUMP_SWAPBUFFERS_LEAVE() do {} while (0)
#else /* if !defined VBOX_WITH_CRSERVER_DUMPER */
#define CR_SERVER_DUMP_DRAW_ENTER() do {} while (0)
#define CR_SERVER_DUMP_DRAW_LEAVE() do {} while (0)
#define CR_SERVER_DUMP_COMPILE_SHADER(_id) do {} while (0)
#define CR_SERVER_DUMP_LINK_PROGRAM(_id) do {} while (0)
#define CR_SERVER_DUMP_SWAPBUFFERS_ENTER() do {} while (0)
#define CR_SERVER_DUMP_SWAPBUFFERS_LEAVE() do {} while (0)
#endif /* !VBOX_WITH_CRSERVER_DUMPER */

RT_C_DECLS_END

#endif /* CR_SERVER_H */
