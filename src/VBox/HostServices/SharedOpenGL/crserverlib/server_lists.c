/* Copyright (c) 2001-2003, Stanford University
    All rights reserved.

    See the file LICENSE.txt for information on redistributing this software. */

#include "server_dispatch.h"
#include "server.h"
#include "cr_mem.h"


/*
 * Notes on ID translation:
 *
 * If a server has multiple clients (in the case of parallel applications)
 * and N of the clients all create a display list with ID K, does K name
 * one display list or N different display lists?
 *
 * By default, there is one display list named K.  If the clients put
 * identical commands into list K, then this is fine.  But if the clients
 * each put something different into list K when they created it, then this
 * is a serious problem.
 *
 * By zeroing the 'shared_display_lists' configuration option, we can tell
 * the server to make list K be unique for all N clients.  We do this by
 * translating K into a new, unique ID dependant on which client we're
 * talking to (curClient->number).
 *
 * Same story for texture objects, vertex programs, etc.
 *
 * The application can also dynamically switch between shared and private
 * display lists with:
 *   glChromiumParameteri(GL_SHARED_DISPLAY_LISTS_CR, GL_TRUE)
 * and
 *   glChromiumParameteri(GL_SHARED_DISPLAY_LISTS_CR, GL_FALSE)
 *
 */



static GLuint TranslateListID( GLuint id )
{
    if (!cr_server.sharedDisplayLists) {
        int client = cr_server.curClient->number;
        return id + client * 100000;
    }
    return id;
}


GLuint crServerTranslateTextureID( GLuint id )
{
    if (!cr_server.sharedTextureObjects && id) {
        int client = cr_server.curClient->number;
        return id + client * 100000;
    }
    return id;
}

/* XXXX Note: shared/separate Program ID numbers aren't totally implemented! */
GLuint crServerTranslateProgramID( GLuint id )
{
    if (!cr_server.sharedPrograms && id) {
        int client = cr_server.curClient->number;
        return id + client * 100000;
    }
    return id;
}


void SERVER_DISPATCH_APIENTRY
crServerDispatchNewList( GLuint list, GLenum mode )
{
    if (mode == GL_COMPILE_AND_EXECUTE)
        crWarning("using glNewList(GL_COMPILE_AND_EXECUTE) can confuse the crserver");

    list = TranslateListID( list );
    crStateNewList( list, mode );
    cr_server.head_spu->dispatch_table.NewList( list, mode );
}


void SERVER_DISPATCH_APIENTRY
crServerDispatchCallList( GLuint list )
{
    list = TranslateListID( list );

    if (cr_server.curClient->currentCtx->lists.mode == 0) {
        /* we're not compiling, so execute the list now */
        CRMuralInfo *mural = cr_server.curClient->currentMural;
        int i;

        if (!mural->viewportValidated) {
            crServerComputeViewportBounds(&(cr_server.curClient->currentCtx->viewport), mural);
        }

        if (mural->numExtents == 0) {
            /* Issue the list as-is */
            cr_server.head_spu->dispatch_table.CallList( list );
        }
        else {
            /* Loop over the extents (tiles) calling glCallList() */
            for ( i = 0; i < mural->numExtents; i++ )   {
                if (cr_server.run_queue->client->currentCtx)
                    crServerSetOutputBounds( mural, i );
                cr_server.head_spu->dispatch_table.CallList( list );
            }
        }
    }
    else {
        /* we're compiling glCallList into another list - just pass it through */
        cr_server.head_spu->dispatch_table.CallList( list );
    }
}


/**
 * Translate an array of display list IDs from various datatypes to GLuint
 * IDs while adding the per-client offset.
 */
static void
TranslateListIDs(GLsizei n, GLenum type, const GLvoid *lists, GLuint *newLists)
{
    int offset = cr_server.curClient->number * 100000;
    GLsizei i;
    switch (type) {
    case GL_UNSIGNED_BYTE:
        {
            const GLubyte *src = (const GLubyte *) lists;
            for (i = 0; i < n; i++) {
                newLists[i] = src[i] + offset;
            }
        }
        break;
    case GL_BYTE:
        {
            const GLbyte *src = (const GLbyte *) lists;
            for (i = 0; i < n; i++) {
                newLists[i] = src[i] + offset;
            }
        }
        break;
    case GL_UNSIGNED_SHORT:
        {
            const GLushort *src = (const GLushort *) lists;
            for (i = 0; i < n; i++) {
                newLists[i] = src[i] + offset;
            }
        }
        break;
    case GL_SHORT:
        {
            const GLshort *src = (const GLshort *) lists;
            for (i = 0; i < n; i++) {
                newLists[i] = src[i] + offset;
            }
        }
        break;
    case GL_UNSIGNED_INT:
        {
            const GLuint *src = (const GLuint *) lists;
            for (i = 0; i < n; i++) {
                newLists[i] = src[i] + offset;
            }
        }
        break;
    case GL_INT:
        {
            const GLint *src = (const GLint *) lists;
            for (i = 0; i < n; i++) {
                newLists[i] = src[i] + offset;
            }
        }
        break;
    case GL_FLOAT:
        {
            const GLfloat *src = (const GLfloat *) lists;
            for (i = 0; i < n; i++) {
                newLists[i] = (GLuint) src[i] + offset;
            }
        }
        break;
    case GL_2_BYTES:
        {
            const GLubyte *src = (const GLubyte *) lists;
            for (i = 0; i < n; i++) {
                newLists[i] = (src[i*2+0] * 256 +
                                             src[i*2+1]) + offset;
            }
        }
        break;
    case GL_3_BYTES:
        {
            const GLubyte *src = (const GLubyte *) lists;
            for (i = 0; i < n; i++) {
                newLists[i] = (src[i*3+0] * 256 * 256 +
                                             src[i*3+1] * 256 +
                                             src[i*3+2]) + offset;
            }
        }
        break;
    case GL_4_BYTES:
        {
            const GLubyte *src = (const GLubyte *) lists;
            for (i = 0; i < n; i++) {
                newLists[i] = (src[i*4+0] * 256 * 256 * 256 +
                                             src[i*4+1] * 256 * 256 +
                                             src[i*4+2] * 256 +
                                             src[i*4+3]) + offset;
            }
        }
        break;
    default:
        crWarning("CRServer: invalid display list datatype 0x%x", type);
    }
}


void SERVER_DISPATCH_APIENTRY
crServerDispatchCallLists( GLsizei n, GLenum type, const GLvoid *lists )
{
    if (!cr_server.sharedDisplayLists) {
        /* need to translate IDs */
        GLuint *newLists = (GLuint *) crAlloc(n * sizeof(GLuint));
        if (newLists) {
            TranslateListIDs(n, type, lists, newLists);
        }
        lists = newLists;
        type = GL_UNSIGNED_INT;
    }

    if (cr_server.curClient->currentCtx->lists.mode == 0) {
        /* we're not compiling, so execute the list now */
        CRMuralInfo *mural = cr_server.curClient->currentMural;
        int i;

        if (!mural->viewportValidated) {
            crServerComputeViewportBounds(&(cr_server.curClient->currentCtx->viewport), mural);
        }

        if (mural->numExtents == 0) {
            /* Issue the list as-is */
            cr_server.head_spu->dispatch_table.CallLists( n, type, lists );
        }
        else {
            /* Loop over the extents (tiles) calling glCallList() */
            for ( i = 0; i < mural->numExtents; i++ ) {
                if (cr_server.run_queue->client->currentCtx)
                    crServerSetOutputBounds( mural, i );
                cr_server.head_spu->dispatch_table.CallLists( n, type, lists );
            }
        }
    }
    else {
        /* we're compiling glCallList into another list - just pass it through */
        cr_server.head_spu->dispatch_table.CallLists( n, type, lists );
    }

    if (!cr_server.sharedDisplayLists) {
        crFree((void *) lists);  /* malloc'd above */
    }
}


GLboolean SERVER_DISPATCH_APIENTRY crServerDispatchIsList( GLuint list )
{
    GLboolean retval;
    list = TranslateListID( list );
    retval = cr_server.head_spu->dispatch_table.IsList( list );
    crServerReturnValue( &retval, sizeof(retval) );
    return retval;
}


void SERVER_DISPATCH_APIENTRY crServerDispatchDeleteLists( GLuint list, GLsizei range )
{
    list = TranslateListID( list );
    crStateDeleteLists( list, range );
    cr_server.head_spu->dispatch_table.DeleteLists( list, range );
}


void SERVER_DISPATCH_APIENTRY crServerDispatchBindTexture( GLenum target, GLuint texture )
{
    texture = crServerTranslateTextureID( texture );
    crStateBindTexture( target, texture );
    cr_server.head_spu->dispatch_table.BindTexture( target, texture );
}


void SERVER_DISPATCH_APIENTRY crServerDispatchDeleteTextures( GLsizei n, const GLuint *textures)
{
    if (!cr_server.sharedTextureObjects) {
        GLuint *newTextures = (GLuint *) crAlloc(n * sizeof(GLuint));
        GLint i;
        if (!newTextures) {
            crError("crServerDispatchDeleteTextures: out of memory");
            return;
        }
        for (i = 0; i < n; i++) {
            newTextures[i] = crServerTranslateTextureID( textures[i] );
        }
        crStateDeleteTextures( n, newTextures );
        cr_server.head_spu->dispatch_table.DeleteTextures( n, newTextures );
        crFree(newTextures);
    }
    else {
        crStateDeleteTextures( n, textures );
        cr_server.head_spu->dispatch_table.DeleteTextures( n, textures );
    }
}

void SERVER_DISPATCH_APIENTRY crServerDispatchPrioritizeTextures( GLsizei n, const GLuint * textures, const GLclampf * priorities )
{
    if (!cr_server.sharedTextureObjects) 
    {
        GLuint *newTextures = (GLuint *) crAlloc(n * sizeof(GLuint));
        GLint i;
        if (!newTextures) {
            crError("crServerDispatchDeleteTextures: out of memory");
            return;
        }
        for (i = 0; i < n; i++) {
            newTextures[i] = crServerTranslateTextureID( textures[i] );
        }
        crStatePrioritizeTextures(n, newTextures, priorities);
        cr_server.head_spu->dispatch_table.PrioritizeTextures(n, newTextures, priorities);
        crFree(newTextures);
    }
    else
    {
        crStatePrioritizeTextures(n, textures, priorities);
        cr_server.head_spu->dispatch_table.PrioritizeTextures(n, textures, priorities);
    }
}

void SERVER_DISPATCH_APIENTRY crServerDispatchDeleteProgramsARB(GLsizei n, const GLuint * programs)
{
    GLuint *pLocalProgs = (GLuint *) crAlloc(n * sizeof(GLuint));
    GLint i;
    if (!pLocalProgs) {
        crError("crServerDispatchDeleteProgramsARB: out of memory");
        return;
    }
    for (i = 0; i < n; i++) {
        pLocalProgs[i] = crServerTranslateProgramID(programs[i]);
    }
    crStateDeleteProgramsARB(n, pLocalProgs);
    cr_server.head_spu->dispatch_table.DeleteProgramsARB(n, pLocalProgs);
    crFree(pLocalProgs);
}

/*@todo will fail for textures loaded from snapshot */
GLboolean SERVER_DISPATCH_APIENTRY crServerDispatchIsTexture( GLuint texture )
{
    GLboolean retval;
    texture = crServerTranslateTextureID( texture );
    retval = cr_server.head_spu->dispatch_table.IsTexture( texture );
    crServerReturnValue( &retval, sizeof(retval) );
    return retval; /* WILL PROBABLY BE IGNORED */
}

/*@todo will fail for progs loaded from snapshot */
GLboolean SERVER_DISPATCH_APIENTRY crServerDispatchIsProgramARB( GLuint program )
{
    GLboolean retval;
    program = crServerTranslateTextureID(program);
    retval = cr_server.head_spu->dispatch_table.IsProgramARB( program );
    crServerReturnValue( &retval, sizeof(retval) );
    return retval; /* WILL PROBABLY BE IGNORED */
}

GLboolean SERVER_DISPATCH_APIENTRY
crServerDispatchAreTexturesResident(GLsizei n, const GLuint *textures,
                                    GLboolean *residences)
{
    GLboolean retval;
    GLboolean *res = (GLboolean *) crAlloc(n * sizeof(GLboolean));
    GLsizei i;

    (void) residences;

    if (!cr_server.sharedTextureObjects) {
        GLuint *textures2 = (GLuint *) crAlloc(n * sizeof(GLuint));
        for (i = 0; i < n; i++)
            textures2[i] = crServerTranslateTextureID(textures[i]);
        retval = cr_server.head_spu->dispatch_table.AreTexturesResident(n, textures2, res);
        crFree(textures2);
    }
    else {
        retval = cr_server.head_spu->dispatch_table.AreTexturesResident(n, textures, res);
    }
    crServerReturnValue(res, n * sizeof(GLboolean));

    crFree(res);

    return retval; /* WILL PROBABLY BE IGNORED */
}


GLboolean SERVER_DISPATCH_APIENTRY
crServerDispatchAreProgramsResidentNV(GLsizei n, const GLuint *programs,
                                                                            GLboolean *residences)
{
    GLboolean retval;
    GLboolean *res = (GLboolean *) crAlloc(n * sizeof(GLboolean));
    GLsizei i;

    (void) residences;

    if (!cr_server.sharedTextureObjects) {
        GLuint *programs2 = (GLuint *) crAlloc(n * sizeof(GLuint));
        for (i = 0; i < n; i++)
            programs2[i] = crServerTranslateProgramID(programs[i]);
        retval = cr_server.head_spu->dispatch_table.AreProgramsResidentNV(n, programs2, res);
        crFree(programs2);
    }
    else {
        retval = cr_server.head_spu->dispatch_table.AreProgramsResidentNV(n, programs, res);
    }

    crServerReturnValue(res, n * sizeof(GLboolean));
    crFree(res);

    return retval; /* WILL PROBABLY BE IGNORED */
}
