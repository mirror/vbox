/* Copyright (c) 2001, Stanford University
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#include "packspu.h"
#include "cr_packfunctions.h"
#include "state/cr_statefuncs.h"
#include "cr_string.h"
#include "packspu_proto.h"

static const GLubyte *
GetExtensions(void)
{
	GLubyte return_value[10*1000];
	const GLubyte *extensions, *ext;
	GET_THREAD(thread);
	int writeback = 1;

	if (pack_spu.swap)
	{
		crPackGetStringSWAP( GL_EXTENSIONS, return_value, &writeback );
	}
	else
	{
		crPackGetString( GL_EXTENSIONS, return_value, &writeback );
	}
	packspuFlush( (void *) thread );

	while (writeback)
		crNetRecv();

	CRASSERT(crStrlen((char *)return_value) < 10*1000);

	/* OK, we got the result from the server.  Now we have to
	 * intersect is with the set of extensions that Chromium understands
	 * and tack on the Chromium-specific extensions.
	 */
	extensions = return_value;
	ext = crStateMergeExtensions(1, &extensions);

	return ext;  /* XXX we should return a static string here! */
}


static GLfloat
GetVersionString(void)
{
	GLubyte return_value[100];
	GET_THREAD(thread);
	int writeback = 1;
	GLfloat version;

	if (pack_spu.swap)
		crPackGetStringSWAP( GL_VERSION, return_value, &writeback );
	else
		crPackGetString( GL_VERSION, return_value, &writeback );
	packspuFlush( (void *) thread );

	while (writeback)
		crNetRecv();

	CRASSERT(crStrlen((char *)return_value) < 100);

	version = crStrToFloat((char *) return_value);
	version = crStateComputeVersion(version);

	return version;
}


const GLubyte * PACKSPU_APIENTRY packspu_GetString( GLenum name )
{
	GET_CONTEXT(ctx);
	if (name == GL_EXTENSIONS)
	{
		return GetExtensions();
	}
	else if (name == GL_VERSION)
	{
		float version = GetVersionString();
		sprintf(ctx->glVersion, "%g Chromium %s", version, CR_VERSION_STRING);
		return (const GLubyte *) ctx->glVersion;
	}
	else
	{
		return crStateGetString(name);
	}
}
