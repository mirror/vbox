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

extern uint8_t* g_pvVRamBase;
extern HCRHGSMICMDCOMPLETION g_hCrHgsmiCompletion;
extern PFNCRHGSMICMDCOMPLETION g_pfnCrHgsmiCompletion;

#define VBOXCRHGSMI_PTR(_off, _t) ((_t*)(g_pvVRamBase + (_off)))

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

typedef struct {
    char   *pszDpyName;
    GLint   visualBits; 
    int32_t internalID;
} CRCreateInfo_t;

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

void crServerCreateInfoDeleteCB(void *data);

GLint crServerGenerateID(GLint *pCounter);

GLint crServerSPUWindowID(GLint serverWindow);

GLuint crServerTranslateProgramID(GLuint id);

void crServerSetupOutputRedirect(CRMuralInfo *mural);
void crServerCheckMuralGeometry(CRMuralInfo *mural);
GLboolean crServerSupportRedirMuralFBO(void);
void crServerRedirMuralFBO(CRMuralInfo *mural, GLboolean redir);
void crServerCreateMuralFBO(CRMuralInfo *mural);
void crServerDeleteMuralFBO(CRMuralInfo *mural);
void crServerPresentFBO(CRMuralInfo *mural);
GLboolean crServerIsRedirectedToFBO();

int32_t crVBoxServerInternalClientRead(CRClient *pClient, uint8_t *pBuffer, uint32_t *pcbBuffer);

#endif /* CR_SERVER_H */
