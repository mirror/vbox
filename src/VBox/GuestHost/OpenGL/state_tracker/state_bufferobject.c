/* Copyright (c) 2001, Stanford University
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#include "state.h"
#include "state/cr_statetypes.h"
#include "state/cr_statefuncs.h"
#include "state_internals.h"
#include "cr_mem.h"
#include "cr_string.h"


static CRBufferObject *AllocBufferObject(GLuint name)
{
    CRBufferObject *b = crCalloc(sizeof(CRBufferObject));
    if (b) {
        b->refCount = 1;
        b->name = name;
        b->usage = GL_STATIC_DRAW_ARB;
        b->access = GL_READ_WRITE_ARB;
    }
    return b;
}


void crStateBufferObjectInit (CRContext *ctx)
{
    CRStateBits *sb          = GetCurrentBits();
    CRBufferObjectBits *bb = &sb->bufferobject;
    CRBufferObjectState *b = &ctx->bufferobject;

    RESET(bb->dirty, ctx->bitid);
    RESET(bb->arrayBinding, ctx->bitid);
    RESET(bb->elementsBinding, ctx->bitid);

#ifdef IN_GUEST
    b->retainBufferData = GL_TRUE;
#else
    b->retainBufferData = GL_FALSE;
#endif

    b->nullBuffer = AllocBufferObject(0);
    b->arrayBuffer = b->nullBuffer;
    b->elementsBuffer = b->nullBuffer;
    b->nullBuffer->refCount = 3;

    b->buffers = crAllocHashtable();
}


static void crStateFreeBufferObject(void *data)
{
    CRBufferObject *pObj = (CRBufferObject *)data;
    if (pObj->data) crFree(pObj->data);

#ifndef IN_GUEST
    if (diff_api.DeleteBuffersARB)
    {
        diff_api.DeleteBuffersARB(1, &pObj->name);
    }
#endif

    crFree(pObj);
}

void crStateBufferObjectDestroy (CRContext *ctx)
{
    CRBufferObjectState *b = &ctx->bufferobject;
    crFreeHashtable(b->buffers, crStateFreeBufferObject);
    crFree(b->nullBuffer);
}


void STATE_APIENTRY
crStateBindBufferARB (GLenum target, GLuint buffer)
{
    CRContext *g = GetCurrentContext();
    CRBufferObjectState *b = &(g->bufferobject);
    CRStateBits *sb = GetCurrentBits();
    CRBufferObjectBits *bb = &(sb->bufferobject);
    CRBufferObject *oldObj, *newObj;

    if (g->current.inBeginEnd) {
        crStateError(__LINE__, __FILE__, GL_INVALID_OPERATION,
                                 "glBindBufferARB called in begin/end");
        return;
    }

    FLUSH();

    if (target == GL_ARRAY_BUFFER_ARB) {
        oldObj = b->arrayBuffer;
    }
    else if (target == GL_ELEMENT_ARRAY_BUFFER_ARB) {
        oldObj = b->elementsBuffer;
    }
    else {
        crStateError(__LINE__, __FILE__, GL_INVALID_ENUM,
                                 "glBindBufferARB(target)");
        return;
    }

    if (buffer == 0) {
        newObj = b->nullBuffer;
    }
    else {
        newObj = (CRBufferObject *) crHashtableSearch(b->buffers, buffer);
        if (!newObj) {
            newObj = AllocBufferObject(buffer);
            if (!newObj) {
                crStateError(__LINE__, __FILE__, GL_OUT_OF_MEMORY, "glBindBuffer");
                return;
            }
            crHashtableAdd( b->buffers, buffer, newObj );
        }
    }

    newObj->refCount++;
    oldObj->refCount--;

    if (target == GL_ARRAY_BUFFER_ARB) {
        b->arrayBuffer = newObj;
        DIRTY(bb->dirty, g->neg_bitid);
        DIRTY(bb->arrayBinding, g->neg_bitid);
    }
    else {
        CRASSERT(target == GL_ELEMENT_ARRAY_BUFFER_ARB);
        b->elementsBuffer = newObj;
        DIRTY(bb->dirty, g->neg_bitid);
        DIRTY(bb->elementsBinding, g->neg_bitid);
    }

    if (oldObj->refCount <= 0) {
        /*we shouldn't reach this point*/
        CRASSERT(false);
        crHashtableDelete(b->buffers, (unsigned long) oldObj->name, crStateFreeBufferObject);
    }
}

void STATE_APIENTRY
crStateDeleteBuffersARB(GLsizei n, const GLuint *buffers)
{
    CRContext *g = GetCurrentContext();
    CRBufferObjectState *b = &(g->bufferobject);
    CRStateBits *sb = GetCurrentBits();
    CRBufferObjectBits *bb = &(sb->bufferobject);
    int i;

    FLUSH();

    if (g->current.inBeginEnd) {
        crStateError(__LINE__, __FILE__, GL_INVALID_OPERATION,
                                 "glDeleteBuffersARB called in Begin/End");
        return;
    }

    if (n < 0) {
        crStateError(__LINE__, __FILE__, GL_INVALID_VALUE,
                                 "glDeleteBuffersARB(n < 0)");
        return;
    }

    for (i = 0; i < n; i++) {
        if (buffers[i]) {
            CRBufferObject *obj = (CRBufferObject *)
                crHashtableSearch(b->buffers, buffers[i]);
            if (obj) {
                if (obj == b->arrayBuffer) {
                    b->arrayBuffer = b->nullBuffer;
                    b->arrayBuffer->refCount++;
                    DIRTY(bb->dirty, g->neg_bitid);
                    DIRTY(bb->arrayBinding, g->neg_bitid);
                }
                if (obj == b->elementsBuffer) {
                    b->elementsBuffer = b->nullBuffer;
                    b->elementsBuffer->refCount++;
                    DIRTY(bb->dirty, g->neg_bitid);
                    DIRTY(bb->elementsBinding, g->neg_bitid);
                }
                /* @todo check bindings with the vertex arrays */

                crHashtableDelete(b->buffers, buffers[i], crStateFreeBufferObject);
            }
        }
    }
}


void STATE_APIENTRY
crStateGenBuffersARB(GLsizei n, GLuint * buffers)
{
    CRContext *g = GetCurrentContext();
    CRBufferObjectState *b = &(g->bufferobject);
    GLint start;

    FLUSH();

    if (g->current.inBeginEnd) {
        crStateError(__LINE__, __FILE__, GL_INVALID_OPERATION,
                                 "glGenBuffersARB called in Begin/End");
        return;
    }

    if (n < 0) {
        crStateError(__LINE__, __FILE__, GL_INVALID_VALUE,
                                 "glGenBuffersARB(n < 0)");
        return;
    }

    start = crHashtableAllocKeys(b->buffers, n);
    if (start) {
        GLint i;
        for (i = 0; i < n; i++)
            buffers[i] = (GLuint) (start + i);
    }
    else {
        crStateError(__LINE__, __FILE__, GL_OUT_OF_MEMORY, "glGenBuffersARB");
    }
}


GLboolean STATE_APIENTRY
crStateIsBufferARB(GLuint buffer)
{
    CRContext *g = GetCurrentContext();
    CRBufferObjectState *b = &g->bufferobject;

    FLUSH();

    if (g->current.inBeginEnd) {
        crStateError(__LINE__, __FILE__, GL_INVALID_OPERATION,
                                 "glIsBufferARB called in begin/end");
        return GL_FALSE;
    }

    if (buffer && crHashtableSearch(b->buffers, buffer))
        return GL_TRUE;
    else
        return GL_FALSE;
}


void STATE_APIENTRY
crStateBufferDataARB(GLenum target, GLsizeiptrARB size, const GLvoid * data, GLenum usage)
{
    CRContext *g = GetCurrentContext();
    CRBufferObjectState *b = &g->bufferobject;
    CRBufferObject *obj;
    CRStateBits *sb = GetCurrentBits();
    CRBufferObjectBits *bb = &sb->bufferobject;

    FLUSH();

    if (g->current.inBeginEnd) {
        crStateError(__LINE__, __FILE__, GL_INVALID_OPERATION,
                                 "glBufferDataARB called in begin/end");
        return;
    }

    if (size < 0) {
        crStateError(__LINE__, __FILE__, GL_INVALID_VALUE,
                                 "glBufferDataARB(size < 0)");
        return;
    }

    switch (usage) {
        case GL_STREAM_DRAW_ARB:
        case GL_STREAM_READ_ARB:
        case GL_STREAM_COPY_ARB:
        case GL_STATIC_DRAW_ARB:
        case GL_STATIC_READ_ARB:
        case GL_STATIC_COPY_ARB:
        case GL_DYNAMIC_DRAW_ARB:
        case GL_DYNAMIC_READ_ARB:
        case GL_DYNAMIC_COPY_ARB:
            /* OK */
            break;
        default:
            crStateError(__LINE__, __FILE__, GL_INVALID_ENUM,
                                     "glBufferDataARB(usage)");
            return;
    }

    if (target == GL_ARRAY_BUFFER_ARB) {
        obj = b->arrayBuffer;
    }
    else if (target == GL_ELEMENT_ARRAY_BUFFER_ARB) {
        obj = b->elementsBuffer;
    }
    else {
        crStateError(__LINE__, __FILE__, GL_INVALID_ENUM,
                                 "glBufferDataARB(target)");
        return;
    }

    if (obj->name == 0) {
        crStateError(__LINE__, __FILE__, GL_INVALID_OPERATION, "glBufferDataARB");
        return;
    }

    if (obj->pointer) {
        crStateError(__LINE__, __FILE__, GL_INVALID_OPERATION,
                                 "glBufferDataARB(buffer is mapped)");
        return;
    }

    obj->usage = usage;
    obj->size = size;

    /* The user of the state tracker should set the retainBufferData field
     * during context initialization, if needed.
     */
    if (b->retainBufferData) {
        if (obj->data) {
            crFree(obj->data);
        }

        obj->data = crAlloc(size);
        if (!obj->data) {
            crStateError(__LINE__, __FILE__, GL_OUT_OF_MEMORY, "glBufferDataARB");
            return;
        }
        if (data)
            crMemcpy(obj->data, data, size);
    }

    DIRTY(bb->dirty, g->neg_bitid);
    DIRTY(obj->dirty, g->neg_bitid);
    obj->dirtyStart = 0;
    obj->dirtyLength = size;
}


void STATE_APIENTRY
crStateBufferSubDataARB(GLenum target, GLintptrARB offset, GLsizeiptrARB size, const GLvoid * data)
{
    CRContext *g = GetCurrentContext();
    CRBufferObjectState *b = &g->bufferobject;
    CRBufferObject *obj;
    CRStateBits *sb = GetCurrentBits();
    CRBufferObjectBits *bb = &sb->bufferobject;

    FLUSH();

    if (g->current.inBeginEnd) {
        crStateError(__LINE__, __FILE__, GL_INVALID_OPERATION,
                                 "glBufferSubDataARB called in begin/end");
        return;
    }

    if (target == GL_ARRAY_BUFFER_ARB) {
        obj = b->arrayBuffer;
    }
    else if (target == GL_ELEMENT_ARRAY_BUFFER_ARB) {
        obj = b->elementsBuffer;
    }
    else {
        crStateError(__LINE__, __FILE__, GL_INVALID_ENUM,
                                 "glBufferSubDataARB(target)");
        return;
    }

    if (obj->name == 0) {
        crStateError(__LINE__, __FILE__, GL_INVALID_OPERATION,
                                 "glBufferSubDataARB");
        return;
    }

    if (obj->pointer) {
        crStateError(__LINE__, __FILE__, GL_INVALID_OPERATION,
                                 "glBufferSubDataARB(buffer is mapped)");
        return;
    }

    if (size < 0 || offset < 0 || (unsigned int)offset + size > obj->size) {
        crStateError(__LINE__, __FILE__, GL_INVALID_OPERATION,
                                 "glBufferSubDataARB(bad offset and/or size)");
        return;
    }

    if (b->retainBufferData && obj->data) {
        crMemcpy((char *) obj->data + offset, data, size);
    }

    DIRTY(bb->dirty, g->neg_bitid);
    DIRTY(obj->dirty, g->neg_bitid);
    /* grow dirty region */
    if (offset + size > obj->dirtyStart + obj->dirtyLength)
        obj->dirtyLength = offset + size;
    if (offset < obj->dirtyStart)
        obj->dirtyStart = offset;
}


void STATE_APIENTRY
crStateGetBufferSubDataARB(GLenum target, GLintptrARB offset, GLsizeiptrARB size, void * data)
{
    CRContext *g = GetCurrentContext();
    CRBufferObjectState *b = &g->bufferobject;
    CRBufferObject *obj;

    FLUSH();

    if (g->current.inBeginEnd) {
        crStateError(__LINE__, __FILE__, GL_INVALID_OPERATION,
                                 "glGetBufferSubDataARB called in begin/end");
        return;
    }

    if (target == GL_ARRAY_BUFFER_ARB) {
        obj = b->arrayBuffer;
    }
    else if (target == GL_ELEMENT_ARRAY_BUFFER_ARB) {
        obj = b->elementsBuffer;
    }
    else {
        crStateError(__LINE__, __FILE__, GL_INVALID_ENUM,
                                 "glGetBufferSubDataARB(target)");
        return;
    }

    if (obj->name == 0) {
        crStateError(__LINE__, __FILE__, GL_INVALID_OPERATION,
                                 "glGetBufferSubDataARB");
        return;
    }

    if (obj->pointer) {
        crStateError(__LINE__, __FILE__, GL_INVALID_OPERATION,
                                 "glGetBufferSubDataARB(buffer is mapped)");
        return;
    }

    if (size < 0 || offset < 0 || (unsigned int)offset + size > obj->size) {
        crStateError(__LINE__, __FILE__, GL_INVALID_OPERATION,
                                 "glGetBufferSubDataARB(bad offset and/or size)");
        return;
    }

    if (b->retainBufferData && obj->data) {
        crMemcpy(data, (char *) obj->data + offset, size);
    }
}


void * STATE_APIENTRY
crStateMapBufferARB(GLenum target, GLenum access)
{
    CRContext *g = GetCurrentContext();
    CRBufferObjectState *b = &g->bufferobject;
    CRBufferObject *obj;

    FLUSH();

    if (g->current.inBeginEnd) {
        crStateError(__LINE__, __FILE__, GL_INVALID_OPERATION,
                                 "glMapBufferARB called in begin/end");
        return NULL;
    }

    if (target == GL_ARRAY_BUFFER_ARB) {
        obj = b->arrayBuffer;
    }
    else if (target == GL_ELEMENT_ARRAY_BUFFER_ARB) {
        obj = b->elementsBuffer;
    }
    else {
        crStateError(__LINE__, __FILE__, GL_INVALID_ENUM,
                                 "glMapBufferARB(target)");
        return NULL;
    }

    if (obj->name == 0) {
        crStateError(__LINE__, __FILE__, GL_INVALID_OPERATION, "glMapBufferARB");
        return GL_FALSE;
    }

    switch (access) {
        case GL_READ_ONLY_ARB:
        case GL_WRITE_ONLY_ARB:
        case GL_READ_WRITE_ARB:
            obj->access = access;
            break;
        default:
            crStateError(__LINE__, __FILE__, GL_INVALID_ENUM,
                                     "glMapBufferARB(access)");
            return NULL;
    }

    if (b->retainBufferData && obj->data)
        obj->pointer = obj->data;

    return obj->pointer;
}


GLboolean STATE_APIENTRY
crStateUnmapBufferARB(GLenum target)
{
    CRContext *g = GetCurrentContext();
    CRBufferObjectState *b = &g->bufferobject;
    CRBufferObject *obj;
    CRStateBits *sb = GetCurrentBits();
    CRBufferObjectBits *bb = &sb->bufferobject;

    FLUSH();

    if (g->current.inBeginEnd) {
        crStateError(__LINE__, __FILE__, GL_INVALID_OPERATION,
                                 "glUnmapBufferARB called in begin/end");
        return GL_FALSE;
    }

    if (target == GL_ARRAY_BUFFER_ARB) {
        obj = b->arrayBuffer;
    }
    else if (target == GL_ELEMENT_ARRAY_BUFFER_ARB) {
        obj = b->elementsBuffer;
    }
    else {
        crStateError(__LINE__, __FILE__, GL_INVALID_ENUM,
                                 "glUnmapBufferARB(target)");
        return GL_FALSE;
    }

    if (obj->name == 0) {
        crStateError(__LINE__, __FILE__, GL_INVALID_OPERATION, "glUnmapBufferARB");
        return GL_FALSE;
    }

    if (!obj->pointer) {
        crStateError(__LINE__, __FILE__, GL_INVALID_OPERATION, "glUnmapBufferARB");
        return GL_FALSE;
    }

    obj->pointer = NULL;

    if (obj->access != GL_READ_ONLY_ARB) {
        /* the data was most likely modified */
        DIRTY(bb->dirty, g->neg_bitid);
        DIRTY(obj->dirty, g->neg_bitid);
        obj->dirtyStart = 0;
        obj->dirtyLength = obj->size;
    }

    return GL_TRUE;
}


void STATE_APIENTRY
crStateGetBufferParameterivARB(GLenum target, GLenum pname, GLint *params)
{
    CRContext *g = GetCurrentContext();
    CRBufferObjectState *b = &g->bufferobject;
    CRBufferObject *obj;

    FLUSH();

    if (g->current.inBeginEnd) {
        crStateError(__LINE__, __FILE__, GL_INVALID_OPERATION,
                                 "glGetBufferParameterivARB called in begin/end");
        return;
    }

    if (target == GL_ARRAY_BUFFER_ARB) {
        obj = b->arrayBuffer;
    }
    else if (target == GL_ELEMENT_ARRAY_BUFFER_ARB) {
        obj = b->elementsBuffer;
    }
    else {
        crStateError(__LINE__, __FILE__, GL_INVALID_ENUM,
                                 "glGetBufferParameterivARB(target)");
        return;
    }

    switch (pname) {
        case GL_BUFFER_SIZE_ARB:
            *params = obj->size;
            break;
        case GL_BUFFER_USAGE_ARB:
            *params = obj->usage;
            break;
        case GL_BUFFER_ACCESS_ARB:
            *params = obj->access;
            break;
        case GL_BUFFER_MAPPED_ARB:
            *params = (obj->pointer != NULL);
            break;
        default:
            crStateError(__LINE__, __FILE__, GL_INVALID_ENUM,
                                     "glGetBufferParameterivARB(pname)");
            return;
    }
}


void STATE_APIENTRY
crStateGetBufferPointervARB(GLenum target, GLenum pname, GLvoid **params)
{
    CRContext *g = GetCurrentContext();
    CRBufferObjectState *b = &g->bufferobject;
    CRBufferObject *obj;

    FLUSH();

    if (g->current.inBeginEnd) {
        crStateError(__LINE__, __FILE__, GL_INVALID_OPERATION,
                                 "glGetBufferPointervARB called in begin/end");
        return;
    }

    if (target == GL_ARRAY_BUFFER_ARB) {
        obj = b->arrayBuffer;
    }
    else if (target == GL_ELEMENT_ARRAY_BUFFER_ARB) {
        obj = b->elementsBuffer;
    }
    else {
        crStateError(__LINE__, __FILE__, GL_INVALID_ENUM,
                                 "glGetBufferPointervARB(target)");
        return;
    }

    if (pname != GL_BUFFER_MAP_POINTER_ARB) {
        crStateError(__LINE__, __FILE__, GL_INVALID_ENUM,
                                 "glGetBufferPointervARB(pname)");
        return;
    }

    *params = obj->pointer;
}


/**
 * We need to check if the GL_EXT_vertex/pixel_buffer_object extensions
 * are supported before calling any of the diff_api functions.
 * This flag indidcates if the extensions is available (1), not available (0)
 * or needs to be tested for (-1).
 * If we don't do this, we can segfault inside OpenGL.
 * Ideally, the render SPU should no-op unsupported GL functions, but
 * that's a bit complicated.
 */
static GLboolean
HaveBufferObjectExtension(void)
{
    static GLint haveBufferObjectExt = -1;

    if (haveBufferObjectExt == -1) {
        const char *ext;
        /* XXX this check is temporary.  We need to make the tilesort SPU plug
         * GetString into the diff'ing table in order for this to really work.
         */
        if (!diff_api.GetString) {
            haveBufferObjectExt = 0;
            return 0;
        }
        CRASSERT(diff_api.GetString);
        ext = (const char *) diff_api.GetString(GL_EXTENSIONS);
        if (crStrstr(ext, "GL_ARB_vertex_buffer_object") ||
                crStrstr(ext, "GL_ARB_pixel_buffer_object")) {
            haveBufferObjectExt = 1;
        }
        else {
            haveBufferObjectExt = 0;
        }
    }
    return haveBufferObjectExt;
}


void crStateBufferObjectDiff(CRBufferObjectBits *bb, CRbitvalue *bitID,
                                                         CRContext *fromCtx, CRContext *toCtx)
{
    CRBufferObjectState *from = &(fromCtx->bufferobject);
    const CRBufferObjectState *to = &(toCtx->bufferobject);

    if (!HaveBufferObjectExtension())
        return;

    /* ARRAY_BUFFER binding */
    if (CHECKDIRTY(bb->arrayBinding, bitID)) {
        if (from->arrayBuffer != to->arrayBuffer) {
            GLuint bufferID = to->arrayBuffer ? to->arrayBuffer->name : 0;
            diff_api.BindBufferARB(GL_ARRAY_BUFFER_ARB, bufferID);
            CLEARDIRTY2(bb->arrayBinding, bitID);
            from->arrayBuffer = to->arrayBuffer;
        }
    }

    if (to->arrayBuffer && CHECKDIRTY(to->arrayBuffer->dirty, bitID)) {
        /* update array buffer data */
        CRBufferObject *bufObj = to->arrayBuffer;
        CRASSERT(bufObj);
        if (bufObj->dirtyStart == 0 && bufObj->dirtyLength == (int) bufObj->size) {
            /* update whole buffer */
            diff_api.BufferDataARB(GL_ARRAY_BUFFER_ARB, bufObj->size,
                                                         bufObj->data, bufObj->usage);
        }
        else {
            /* update sub buffer */
            diff_api.BufferSubDataARB(GL_ARRAY_BUFFER_ARB,
                                                                bufObj->dirtyStart,
                                                                bufObj->dirtyLength,
                                                                (char *) bufObj->data
                                                                + bufObj->dirtyStart);
        }
        CLEARDIRTY2(bufObj->dirty, bitID);
#if 0
        bufObj->dirtyStart = 0;
        bufObj->dirtyLength = 0;
#endif
    }

    /* ELEMENTS_BUFFER binding */
    if (CHECKDIRTY(bb->elementsBinding, bitID)) {
        if (from->elementsBuffer != to->elementsBuffer) {
            GLuint bufferID = to->elementsBuffer ? to->elementsBuffer->name : 0;
            diff_api.BindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, bufferID);
            CLEARDIRTY2(bb->elementsBinding, bitID);
            from->elementsBuffer = to->elementsBuffer;
        }
    }

    if (to->elementsBuffer && CHECKDIRTY(to->elementsBuffer->dirty, bitID)) {
        /* update element buffer data */
        CRBufferObject *bufObj = to->elementsBuffer;
        CRASSERT(bufObj);
        if (bufObj->dirtyStart == 0 && bufObj->dirtyLength == (int) bufObj->size) {
            /* update whole buffer */
            diff_api.BufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, bufObj->size,
                                                         bufObj->data, bufObj->usage);
        }
        else {
            /* update sub buffer */
            diff_api.BufferSubDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB,
                                                                bufObj->dirtyStart,
                                                                bufObj->dirtyLength,
                                                                (char *) bufObj->data
                                                                + bufObj->dirtyStart);
        }
        CLEARDIRTY2(bufObj->dirty, bitID);
#if 0
        bufObj->dirtyStart = 0;
        bufObj->dirtyLength = 0;
#endif
    }
}


/*
 * XXX this function might need some testing/fixing.
 */
void crStateBufferObjectSwitch(CRBufferObjectBits *bb, CRbitvalue *bitID, 
                                                             CRContext *fromCtx, CRContext *toCtx)
{
    const CRBufferObjectState *from = &(fromCtx->bufferobject);
    const CRBufferObjectState *to = &(toCtx->bufferobject);

    if (!HaveBufferObjectExtension())
        return;

    /* ARRAY_BUFFER binding */
    if (CHECKDIRTY(bb->arrayBinding, bitID)) {
        if (from->arrayBuffer != to->arrayBuffer) {
            GLuint bufferID = to->arrayBuffer ? to->arrayBuffer->name : 0;
            diff_api.BindBufferARB(GL_ARRAY_BUFFER_ARB, bufferID);
            FILLDIRTY(bb->arrayBinding);
        }
        CLEARDIRTY2(bb->arrayBinding, bitID);
    }

    if (to->arrayBuffer && CHECKDIRTY(to->arrayBuffer->dirty, bitID)) {
        /* update array buffer data */
        CRBufferObject *bufObj = to->arrayBuffer;
        CRASSERT(bufObj);
        if (bufObj->dirtyStart == 0 && bufObj->dirtyLength == (int) bufObj->size) {
            /* update whole buffer */
            diff_api.BufferDataARB(GL_ARRAY_BUFFER_ARB, bufObj->size,
                                                         bufObj->data, bufObj->usage);
        }
        else {
            /* update sub buffer */
            diff_api.BufferSubDataARB(GL_ARRAY_BUFFER_ARB,
                                                                bufObj->dirtyStart,
                                                                bufObj->dirtyLength,
                                                                (char *) bufObj->data
                                                                + bufObj->dirtyStart);
        }
        FILLDIRTY(bufObj->dirty);
        CLEARDIRTY2(bufObj->dirty, bitID);
#if 0
        bufObj->dirtyStart = 0;
        bufObj->dirtyLength = 0;
#endif
    }

    /* ELEMENTS_BUFFER binding */
    if (CHECKDIRTY(bb->elementsBinding, bitID)) {
        if (from->elementsBuffer != to->elementsBuffer) {
            GLuint bufferID = to->elementsBuffer ? to->elementsBuffer->name : 0;
            diff_api.BindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, bufferID);
            FILLDIRTY(bb->elementsBinding);
        }
        CLEARDIRTY2(bb->elementsBinding, bitID);
    }

    if (to->elementsBuffer && CHECKDIRTY(to->elementsBuffer->dirty, bitID)) {
        /* update element buffer data */
        CRBufferObject *bufObj = to->elementsBuffer;
        CRASSERT(bufObj);
        if (bufObj->dirtyStart == 0 && bufObj->dirtyLength == (int) bufObj->size) {
            /* update whole buffer */
            diff_api.BufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, bufObj->size,
                                                         bufObj->data, bufObj->usage);
        }
        else {
            /* update sub buffer */
            diff_api.BufferSubDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB,
                                                                bufObj->dirtyStart,
                                                                bufObj->dirtyLength,
                                                                (char *) bufObj->data
                                                                + bufObj->dirtyStart);
        }
        FILLDIRTY(bufObj->dirty);
        CLEARDIRTY2(bufObj->dirty, bitID);
#if 0
        bufObj->dirtyStart = 0;
        bufObj->dirtyLength = 0;
#endif
    }
}


