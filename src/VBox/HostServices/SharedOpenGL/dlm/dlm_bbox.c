/* $Id$ */
/**
 * Code to compute the bounding box of a DLM display list.
 * Matrix commands in the display list are observed to properly
 * transform the vertices we find.
 */

#include <float.h>
#include "cr_dlm.h"
#include "cr_mem.h"
#include "cr_spu.h"
#include "cr_glstate.h"
#include "dlm.h"


static CRMatrixStack ModelViewStack;
static CRMatrixStack DummyStack;
static CRMatrixStack *CurrentStack;


static GLfloat Xmin, Xmax, Ymin, Ymax, Zmin, Zmax;


static void
MatrixMode(GLenum matrix)
{
	switch (matrix) {
	case GL_MODELVIEW:
		CurrentStack = &ModelViewStack;
		break;
	default:
		CurrentStack = &DummyStack;
	}
}


static void
LoadMatrixf(const GLfloat m[16])
{
	crMatrixInitFromFloats(CurrentStack->top, m);
}


static void
LoadMatrixd(const GLdouble m[16])
{
	crMatrixInitFromDoubles(CurrentStack->top, m);
}


static void
LoadIdentity(void)
{
	crMatrixInit(CurrentStack->top);
}


static void
PushMatrix(void)
{
	if (CurrentStack->depth < CurrentStack->maxDepth) {
		/* copy matrix */
		*(CurrentStack->top + 1) = *(CurrentStack->top);
		/* Move the stack pointer */
		CurrentStack->depth++;
		CurrentStack->top = CurrentStack->stack + CurrentStack->depth;
	}
	else {
		crWarning("Stack overflow in dlm_bbox.c");
	}
}


static void
PopMatrix(void)
{
	if (CurrentStack->depth > 0) {
		CurrentStack->depth--;
		CurrentStack->top = CurrentStack->stack + CurrentStack->depth;
	}
	else {
		crWarning("Stack underflow in dlm_bbox.c");
	}
}


static void
MultMatrixf(const GLfloat m1[16])
{
	CRmatrix *m = CurrentStack->top;
	const GLdefault lm00 = m->m00;  
	const GLdefault lm01 = m->m01;  
	const GLdefault lm02 = m->m02;	
	const GLdefault lm03 = m->m03;	
	const GLdefault lm10 = m->m10;	
	const GLdefault lm11 = m->m11;	
	const GLdefault lm12 = m->m12;	
	const GLdefault lm13 = m->m13;	
	const GLdefault lm20 = m->m20;	
	const GLdefault lm21 = m->m21;	
	const GLdefault lm22 = m->m22;	
	const GLdefault lm23 = m->m23;	
	const GLdefault lm30 = m->m30;	
	const GLdefault lm31 = m->m31;	
	const GLdefault lm32 = m->m32;		
	const GLdefault lm33 = m->m33;		
	const GLdefault rm00 = (GLdefault) m1[0];	
	const GLdefault rm01 = (GLdefault) m1[1];	
	const GLdefault rm02 = (GLdefault) m1[2];	
	const GLdefault rm03 = (GLdefault) m1[3];	
	const GLdefault rm10 = (GLdefault) m1[4];	
	const GLdefault rm11 = (GLdefault) m1[5];		
	const GLdefault rm12 = (GLdefault) m1[6];	
	const GLdefault rm13 = (GLdefault) m1[7];	
	const GLdefault rm20 = (GLdefault) m1[8];	
	const GLdefault rm21 = (GLdefault) m1[9];	
	const GLdefault rm22 = (GLdefault) m1[10];	
	const GLdefault rm23 = (GLdefault) m1[11];	
	const GLdefault rm30 = (GLdefault) m1[12];	
	const GLdefault rm31 = (GLdefault) m1[13];	
	const GLdefault rm32 = (GLdefault) m1[14];	
	const GLdefault rm33 = (GLdefault) m1[15];	

	m->m00 = lm00*rm00 + lm10*rm01 + lm20*rm02 + lm30*rm03;	
	m->m01 = lm01*rm00 + lm11*rm01 + lm21*rm02 + lm31*rm03;	
	m->m02 = lm02*rm00 + lm12*rm01 + lm22*rm02 + lm32*rm03;	
	m->m03 = lm03*rm00 + lm13*rm01 + lm23*rm02 + lm33*rm03;	
	m->m10 = lm00*rm10 + lm10*rm11 + lm20*rm12 + lm30*rm13;	
	m->m11 = lm01*rm10 + lm11*rm11 + lm21*rm12 + lm31*rm13;	
	m->m12 = lm02*rm10 + lm12*rm11 + lm22*rm12 + lm32*rm13;	
	m->m13 = lm03*rm10 + lm13*rm11 + lm23*rm12 + lm33*rm13;	
	m->m20 = lm00*rm20 + lm10*rm21 + lm20*rm22 + lm30*rm23;	
	m->m21 = lm01*rm20 + lm11*rm21 + lm21*rm22 + lm31*rm23;	
	m->m22 = lm02*rm20 + lm12*rm21 + lm22*rm22 + lm32*rm23;	
	m->m23 = lm03*rm20 + lm13*rm21 + lm23*rm22 + lm33*rm23;	
	m->m30 = lm00*rm30 + lm10*rm31 + lm20*rm32 + lm30*rm33;	
	m->m31 = lm01*rm30 + lm11*rm31 + lm21*rm32 + lm31*rm33;	
	m->m32 = lm02*rm30 + lm12*rm31 + lm22*rm32 + lm32*rm33;	
	m->m33 = lm03*rm30 + lm13*rm31 + lm23*rm32 + lm33*rm33;
}


static void
MultMatrixd(const GLdouble m1[16])
{
	GLfloat m2[16];
	int i;
	for (i = 0; i < 16; i++)
		m2[i] = (GLfloat) m1[i];
	MultMatrixf(m2);
}


static void
Rotatef(GLfloat ang, GLfloat x, GLfloat y, GLfloat z) 
{
	crMatrixRotate(CurrentStack->top, ang, x, y, z);
}


static void
Rotated(GLdouble ang, GLdouble x, GLdouble y, GLdouble z) 
{
	crMatrixRotate(CurrentStack->top, (GLfloat) ang,
								 (GLfloat) x, (GLfloat) y, (GLfloat) z);
}


static void
Translatef(GLfloat x, GLfloat y, GLfloat z) 
{
	crMatrixTranslate(CurrentStack->top, x, y, z);
}


static void
Translated(GLdouble x, GLdouble y, GLdouble z) 
{
	crMatrixTranslate(CurrentStack->top, (GLfloat) x, (GLfloat) y, (GLfloat) z);
}


static void
Scalef(GLfloat x, GLfloat y, GLfloat z) 
{
	crMatrixScale(CurrentStack->top, x, y, z);
}


static void
Scaled(GLdouble x, GLdouble y, GLdouble z) 
{
	crMatrixScale(CurrentStack->top, (GLfloat) x, (GLfloat) y, (GLfloat) z);
}


/**
 * Transform given (x,y,z,w) by current matrix and update the bounding box.
 */
static void
DoVertex(GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
	const CRmatrix *m = CurrentStack->top;
	const GLfloat x2 = m->m00 * x + m->m10 * y + m->m20 * z + m->m30 * w;
	const GLfloat y2 = m->m01 * x + m->m11 * y + m->m21 * z + m->m31 * w;
	const GLfloat z2 = m->m02 * x + m->m12 * y + m->m22 * z + m->m32 * w;
	/*const GLfloat w2 = m->m03 * x + m->m13 * y + m->m23 * z + m->m33 * w;*/

	if (x2 < Xmin) Xmin = x2;
	if (x2 > Xmax) Xmax = x2;
	if (y2 < Ymin) Ymin = y2;
	if (y2 > Ymax) Ymax = y2;
	if (z2 < Zmin) Zmin = z2;
	if (z2 > Zmax) Zmax = z2;
}


static void
Vertex2f(GLfloat x, GLfloat y)
{
	DoVertex(x, y, 0.0f, 1.0f);
}

static void
Vertex2fv(const GLfloat *v)
{
	DoVertex(v[0], v[1], 0.0f, 1.0f);
}

static void
Vertex3f(GLfloat x, GLfloat y, GLfloat z)
{
	DoVertex(x, y, z, 1.0f);
}

static void
Vertex3fv(const GLfloat *v)
{
	DoVertex(v[0], v[1], v[2], 1.0f);
}

static void
Vertex4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
	DoVertex(x, y, z, w);
}

static void
Vertex4fv(const GLfloat *v)
{
	DoVertex(v[0], v[1], v[2], v[3]);
}


/**
 ** XXX TO DO:
 **
glVertex2d( GLdouble x, GLdouble y );
glVertex2f( GLfloat x, GLfloat y );
glVertex2i( GLint x, GLint y );
glVertex2s( GLshort x, GLshort y );
glVertex3d( GLdouble x, GLdouble y, GLdouble z );
glVertex3f( GLfloat x, GLfloat y, GLfloat z );
glVertex3i( GLint x, GLint y, GLint z );
glVertex3s( GLshort x, GLshort y, GLshort z );
glVertex4d( GLdouble x, GLdouble y, GLdouble z, GLdouble w );
glVertex4f( GLfloat x, GLfloat y, GLfloat z, GLfloat w );
glVertex4i( GLint x, GLint y, GLint z, GLint w );
glVertex4s( GLshort x, GLshort y, GLshort z, GLshort w );
glVertex2dv( const GLdouble *v );
glVertex2fv( const GLfloat *v );
glVertex2iv( const GLint *v );
glVertex2sv( const GLshort *v );
glVertex3dv( const GLdouble *v );
glVertex3fv( const GLfloat *v );
glVertex3iv( const GLint *v );
glVertex3sv( const GLshort *v );
glVertex4dv( const GLdouble *v );
glVertex4fv( const GLfloat *v );
glVertex4iv( const GLint *v );
glVertex4sv( const GLshort *v );

glVertexAttrib1dARB(GLuint, GLdouble);
glVertexAttrib1dvARB(GLuint, const GLdouble *);
glVertexAttrib1fARB(GLuint, GLfloat);
glVertexAttrib1fvARB(GLuint, const GLfloat *);
glVertexAttrib1sARB(GLuint, GLshort);
glVertexAttrib1svARB(GLuint, const GLshort *);
glVertexAttrib2dARB(GLuint, GLdouble, GLdouble);
glVertexAttrib2dvARB(GLuint, const GLdouble *);
glVertexAttrib2fARB(GLuint, GLfloat, GLfloat);
glVertexAttrib2fvARB(GLuint, const GLfloat *);
glVertexAttrib2sARB(GLuint, GLshort, GLshort);
glVertexAttrib2svARB(GLuint, const GLshort *);
glVertexAttrib3dARB(GLuint, GLdouble, GLdouble, GLdouble);
glVertexAttrib3dvARB(GLuint, const GLdouble *);
**/

static void
VertexAttrib3fARB(GLuint index, GLfloat x, GLfloat y, GLfloat z)
{
	if (index == 0)
		DoVertex(x, y, z, 1.0f);
}

/**
glVertexAttrib3fvARB(GLuint, const GLfloat *);
glVertexAttrib3sARB(GLuint, GLshort, GLshort, GLshort);
glVertexAttrib3svARB(GLuint, const GLshort *);
glVertexAttrib4NbvARB(GLuint, const GLbyte *);
glVertexAttrib4NivARB(GLuint, const GLint *);
glVertexAttrib4NsvARB(GLuint, const GLshort *);
glVertexAttrib4NubARB(GLuint, GLubyte, GLubyte, GLubyte, GLubyte);
glVertexAttrib4NubvARB(GLuint, const GLubyte *);
glVertexAttrib4NuivARB(GLuint, const GLuint *);
glVertexAttrib4NusvARB(GLuint, const GLushort *);
glVertexAttrib4bvARB(GLuint, const GLbyte *);
glVertexAttrib4dARB(GLuint, GLdouble, GLdouble, GLdouble, GLdouble);
glVertexAttrib4dvARB(GLuint, const GLdouble *);
glVertexAttrib4fARB(GLuint, GLfloat, GLfloat, GLfloat, GLfloat);
glVertexAttrib4fvARB(GLuint, const GLfloat *);
glVertexAttrib4ivARB(GLuint, const GLint *);
glVertexAttrib4sARB(GLuint, GLshort, GLshort, GLshort, GLshort);
glVertexAttrib4svARB(GLuint, const GLshort *);
glVertexAttrib4ubvARB(GLuint, const GLubyte *);
glVertexAttrib4uivARB(GLuint, const GLuint *);
glVertexAttrib4usvARB(GLuint, const GLushort *);

glVertexAttrib1dNV(GLuint, GLdouble);
glVertexAttrib1dvNV(GLuint, const GLdouble *);
glVertexAttrib1fNV(GLuint, GLfloat);
glVertexAttrib1fvNV(GLuint, const GLfloat *);
glVertexAttrib1sNV(GLuint, GLshort);
glVertexAttrib1svNV(GLuint, const GLshort *);
glVertexAttrib2dNV(GLuint, GLdouble, GLdouble);
glVertexAttrib2dvNV(GLuint, const GLdouble *);
glVertexAttrib2fNV(GLuint, GLfloat, GLfloat);
glVertexAttrib2fvNV(GLuint, const GLfloat *);
glVertexAttrib2sNV(GLuint, GLshort, GLshort);
glVertexAttrib2svNV(GLuint, const GLshort *);
glVertexAttrib3dNV(GLuint, GLdouble, GLdouble, GLdouble);
glVertexAttrib3dvNV(GLuint, const GLdouble *);
glVertexAttrib3fNV(GLuint, GLfloat, GLfloat, GLfloat);
glVertexAttrib3fvNV(GLuint, const GLfloat *);
glVertexAttrib3sNV(GLuint, GLshort, GLshort, GLshort);
glVertexAttrib3svNV(GLuint, const GLshort *);
glVertexAttrib4dNV(GLuint, GLdouble, GLdouble, GLdouble, GLdouble);
glVertexAttrib4dvNV(GLuint, const GLdouble *);
glVertexAttrib4fNV(GLuint, GLfloat, GLfloat, GLfloat, GLfloat);
glVertexAttrib4fvNV(GLuint, const GLfloat *);
glVertexAttrib4sNV(GLuint, GLshort, GLshort, GLshort, GLshort);
glVertexAttrib4svNV(GLuint, const GLshort *);
glVertexAttrib4ubNV(GLuint, GLubyte, GLubyte, GLubyte, GLubyte);
glVertexAttrib4ubvNV(GLuint, const GLubyte *);

glVertexAttribs1dvNV(GLuint, GLsizei, const GLdouble *);
glVertexAttribs1fvNV(GLuint, GLsizei, const GLfloat *);
glVertexAttribs1svNV(GLuint, GLsizei, const GLshort *);
glVertexAttribs2dvNV(GLuint, GLsizei, const GLdouble *);
glVertexAttribs2fvNV(GLuint, GLsizei, const GLfloat *);
glVertexAttribs2svNV(GLuint, GLsizei, const GLshort *);
glVertexAttribs3dvNV(GLuint, GLsizei, const GLdouble *);
glVertexAttribs3fvNV(GLuint, GLsizei, const GLfloat *);
glVertexAttribs3svNV(GLuint, GLsizei, const GLshort *);
glVertexAttribs4dvNV(GLuint, GLsizei, const GLdouble *);
glVertexAttribs4fvNV(GLuint, GLsizei, const GLfloat *);
glVertexAttribs4svNV(GLuint, GLsizei, const GLshort *);
glVertexAttribs4ubvNV(GLuint, GLsizei, const GLubyte *);
**/


/**
 ** XXX also need to track evaluator coordinates (glutTeapot)
 **/


static void
InitDispatchTable(SPUDispatchTable *t)
{
	crMemZero(t, sizeof(*t));
	crSPUInitDispatchNops(t);
  /* drm1 */
	t->MatrixMode = (MatrixModeFunc_t)MatrixMode;
	t->LoadIdentity = (LoadIdentityFunc_t)LoadIdentity;
	t->LoadMatrixf = (LoadMatrixfFunc_t)LoadMatrixf;
	t->LoadMatrixd = (LoadMatrixdFunc_t)LoadMatrixd;
	t->PushMatrix = (PushMatrixFunc_t)PushMatrix;
	t->PopMatrix = (PopMatrixFunc_t)PopMatrix;
	t->MultMatrixf = (MultMatrixfFunc_t)MultMatrixf;
	t->MultMatrixd = (MultMatrixdFunc_t)MultMatrixd;
	t->Rotatef = (RotatefFunc_t)Rotatef;
  t->Rotated = (RotatedFunc_t)Rotated;
	t->Translatef = (TranslatefFunc_t)Translatef;
	t->Translated = (TranslatedFunc_t)Translated;
	t->Scalef = (ScalefFunc_t)Scalef;
	t->Scaled = (ScaledFunc_t)Scaled;
	t->Vertex2f = (Vertex2fFunc_t)Vertex2f;
	t->Vertex2fv = (Vertex2fvFunc_t)Vertex2fv;
	t->Vertex3f = (Vertex3fFunc_t)Vertex3f;
	t->Vertex3fv = (Vertex3fvFunc_t)Vertex3fv;
	t->Vertex4f = (Vertex4fFunc_t)Vertex4f;
	t->Vertex4fv = (Vertex4fvFunc_t)Vertex4fv;
	t->VertexAttrib3fARB = (VertexAttrib3fARBFunc_t)VertexAttrib3fARB;
}


void
crDLMComputeBoundingBox(unsigned long listId)
{
	static GLboolean tableInitialized = GL_FALSE;
	static SPUDispatchTable t;
	CRDLMContextState *listState = CURRENT_STATE();
	CRDLM *dlm = listState->dlm;
	DLMListInfo *listInfo
		= (DLMListInfo *) crHashtableSearch(dlm->displayLists, listId);

	if (!tableInitialized) {
		InitDispatchTable(&t);
		crStateInitMatrixStack(&ModelViewStack, CR_MAX_MODELVIEW_STACK_DEPTH);
		crStateInitMatrixStack(&DummyStack, CR_MAX_MODELVIEW_STACK_DEPTH);
		tableInitialized = GL_TRUE;
	}

	CurrentStack = &ModelViewStack;

	Xmin = Ymin = Zmin = FLT_MAX;
	Xmax = Ymax = Zmax = -FLT_MAX;
	
	crDLMReplayDLMList(listState->dlm, listId, &t);

	if (Xmin == FLT_MAX) {
		/* XXX review this choice of default bounds */
		/*
		crDebug("Warning: no bounding box!");
		*/
		Xmin = -100;
		Xmax =  100;
		Ymin = -100;
		Ymax =  100;
		Zmin = -100;
		Zmax =  100;
	}
	/*
	crDebug("List %d bbox: %f, %f, %f .. %f, %f, %f", (int) listId,
					Xmin, Ymin, Zmin, Xmax, Ymax, Zmax);
	*/

	listInfo->bbox.xmin = Xmin;
	listInfo->bbox.ymin = Ymin;
	listInfo->bbox.zmin = Zmin;
	listInfo->bbox.xmax = Xmax;
	listInfo->bbox.ymax = Ymax;
	listInfo->bbox.zmax = Zmax;
}
