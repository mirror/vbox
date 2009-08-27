/* Copyright (c) 2001, Stanford University
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#include "cr_spu.h"
#include "chromium.h"
#include "cr_mem.h"
#include "cr_net.h"
#include "server_dispatch.h"
#include "server.h"

void SERVER_DISPATCH_APIENTRY crServerDispatchGenTextures( GLsizei n, GLuint *textures )
{
    GLsizei i;
    GLuint *local_textures = (GLuint *) crAlloc( n*sizeof( *local_textures) );
    (void) textures;
    cr_server.head_spu->dispatch_table.GenTextures( n, local_textures );

    /* This is somewhat hacky.
     * We have to make sure we're going to generate unique texture IDs.
     * That wasn't the case before snapshot loading, because we could just rely on host video drivers.
     * Now we could have a set of loaded texture IDs which aren't already reserved in the host driver.
     * Note: It seems, that it's easy to reserve ATI/NVidia IDs, by simply calling glGenTextures
     * with n==number of loaded textures. But it's really implementation dependant. So can't rely that it'll not change.
     */
    for (i=0; i<n; ++i)
    {
        /* translate the ID as it'd be done in glBindTexture call */
        GLuint tID = crServerTranslateTextureID(local_textures[i]);
        /* check if we have a texture with same ID loaded from snapshot */
        while (crStateIsTexture(tID))
        {
            /* request new ID */
            cr_server.head_spu->dispatch_table.GenTextures(1, &tID);
            local_textures[i] = tID;
            tID = crServerTranslateTextureID(tID);
        }
    }

    crServerReturnValue( local_textures, n*sizeof( *local_textures ) );
    crFree( local_textures );
}


void SERVER_DISPATCH_APIENTRY crServerDispatchGenProgramsNV( GLsizei n, GLuint * ids )
{
    GLuint *local_progs = (GLuint *) crAlloc( n*sizeof( *local_progs) );
    (void) ids;
    cr_server.head_spu->dispatch_table.GenProgramsNV( n, local_progs );
    crServerReturnValue( local_progs, n*sizeof( *local_progs ) );
    crFree( local_progs );
}


void SERVER_DISPATCH_APIENTRY crServerDispatchGenFencesNV( GLsizei n, GLuint * ids )
{
    GLuint *local_fences = (GLuint *) crAlloc( n*sizeof( *local_fences) );
    (void) ids;
    cr_server.head_spu->dispatch_table.GenFencesNV( n, local_fences );
    crServerReturnValue( local_fences, n*sizeof( *local_fences ) );
    crFree( local_fences );
}

void SERVER_DISPATCH_APIENTRY crServerDispatchGenProgramsARB( GLsizei n, GLuint * ids )
{
    GLuint *local_progs = (GLuint *) crAlloc( n*sizeof( *local_progs) );
    GLsizei i;
    (void) ids;
    cr_server.head_spu->dispatch_table.GenProgramsARB( n, local_progs );

    /* see comments in crServerDispatchGenTextures */
    for (i=0; i<n; ++i)
    {
        GLuint tID = crServerTranslateProgramID(local_progs[i]);
        while (crStateIsProgramARB(tID))
        {
            cr_server.head_spu->dispatch_table.GenProgramsARB(1, &tID);
            local_progs[i] = tID;
            tID = crServerTranslateProgramID(tID);
        }
    }

    crServerReturnValue( local_progs, n*sizeof( *local_progs ) );
    crFree( local_progs );
}

void SERVER_DISPATCH_APIENTRY
crServerDispatchCopyTexImage2D(GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border)
{
    GLsizei tw, th;

    cr_server.head_spu->dispatch_table.GetTexLevelParameteriv(target, level, GL_TEXTURE_WIDTH, &width);
    cr_server.head_spu->dispatch_table.GetTexLevelParameteriv(target, level, GL_TEXTURE_HEIGHT, &height);

    /* Workaround for a wine or ati bug. Host drivers crash unless we first provide texture bounds. */
    /* @todo: r=poetzsch: gcc warns here. tw & th is uninitialized in any case.
     * I guess they should be filled with the above two calls & compared
     * against the method params width & height! */
    if (((tw!=width) || (th!=height)) && (internalFormat==GL_DEPTH_COMPONENT24))
    {
        crServerDispatchTexImage2D(target, level, internalFormat, width, height, border, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL);
    }

    cr_server.head_spu->dispatch_table.CopyTexImage2D(target, level, internalFormat, x, y, width, height, border);
}
