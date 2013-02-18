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

GLint crServerGenerateID(GLint *pCounter);

GLint crServerSPUWindowID(GLint serverWindow);

GLuint crServerTranslateProgramID(GLuint id);

CRMuralInfo * crServerGetDummyMural(GLint visualBits);

void crServerSetupOutputRedirect(CRMuralInfo *mural);
void crServerCheckMuralGeometry(CRMuralInfo *mural);
GLboolean crServerSupportRedirMuralFBO(void);

#define CR_SERVER_REDIR_NONE     0
#define CR_SERVER_REDIR_FBO_BLT  1
#define CR_SERVER_REDIR_FBO_RAM  2
#define CR_SERVER_REDIR_MAXVAL   CR_SERVER_REDIR_FBO_RAM

int32_t crServerSetOffscreenRenderingMode(GLubyte value);
void crServerRedirMuralFBO(CRMuralInfo *mural, GLubyte redir);
void crServerDeleteMuralFBO(CRMuralInfo *mural);
void crServerPresentFBO(CRMuralInfo *mural);
GLboolean crServerIsRedirectedToFBO();
GLuint crServerMuralFBOIdxFromBufferName(CRMuralInfo *mural, GLenum buffer);
void crServerMuralFBOSwapBuffers(CRMuralInfo *mural);

#define CR_SERVER_FBO_BB_IDX(_mural) ((_mural)->iBbBuffer)
#define CR_SERVER_FBO_FB_IDX(_mural) (((_mural)->iBbBuffer + 1) % ((_mural)->cBuffers))

int32_t crVBoxServerInternalClientRead(CRClient *pClient, uint8_t *pBuffer, uint32_t *pcbBuffer);

PCR_DISPLAY crServerDisplayGetInitialized(uint32_t idScreen);

void crServerPerformMakeCurrent( CRMuralInfo *mural, CRContextInfo *ctxInfo );

RT_C_DECLS_END

#endif /* CR_SERVER_H */
