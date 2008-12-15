/* Copyright (c) 2001, Stanford University
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#include "cr_packfunctions.h"
#include "packspu.h"
#include "packspu_proto.h"

void PACKSPU_APIENTRY packspu_ChromiumParametervCR(GLenum target, GLenum type, GLsizei count, const GLvoid *values)
{

	CRMessage msg;
	int len;
	
	GET_THREAD(thread);

	
	switch(target)
	{
		case GL_GATHER_PACK_CR:
			/* flush the current pack buffer */
			packspuFlush( (void *) thread );

			/* the connection is thread->server.conn */
			msg.header.type = CR_MESSAGE_GATHER;
			msg.gather.offset = 69;
			len = sizeof(CRMessageGather);
			crNetSend(thread->netServer.conn, NULL, &msg, len);
			break;
			
		default:
			if (pack_spu.swap)
				crPackChromiumParametervCRSWAP(target, type, count, values);
			else
				crPackChromiumParametervCR(target, type, count, values);
	}


}

void PACKSPU_APIENTRY packspu_Finish( void )
{
	GET_THREAD(thread);
	GLint writeback = pack_spu.thread[0].netServer.conn->actual_network;
	if (pack_spu.swap)
	{
		crPackFinishSWAP(  );
		if (writeback)
			crPackWritebackSWAP( &writeback );
	}
	else
	{
		crPackFinish(  );
		if (writeback)
			crPackWriteback( &writeback );
	}
	packspuFlush( (void *) thread );
	while (writeback)
		crNetRecv();
}


GLint PACKSPU_APIENTRY packspu_WindowCreate( const char *dpyName, GLint visBits )
{
	static int num_calls = 0;
	int writeback = pack_spu.thread[0].netServer.conn->actual_network;
	GLint return_val = (GLint) 0;

	/* WindowCreate is special - just like CreateContext.
	 * GET_THREAD(thread) doesn't work as the thread won't have called
	 * MakeCurrent yet, so we've got to use the first thread's packer
	 * buffer.
	 */

	crPackSetContext( pack_spu.thread[0].packer );

	if (pack_spu.swap)
	{
		crPackWindowCreateSWAP( dpyName, visBits, &return_val, &writeback );
	}
	else
	{
		crPackWindowCreate( dpyName, visBits, &return_val, &writeback );
	}
	packspuFlush( &pack_spu.thread[0] );
	if (!(pack_spu.thread[0].netServer.conn->actual_network))
	{
		return num_calls++;
	}
	else
	{
		while (writeback)
			crNetRecv();
		if (pack_spu.swap)
		{
			return_val = (GLint) SWAP32(return_val);
		}
		return return_val;
	}
}



GLboolean PACKSPU_APIENTRY
packspu_AreTexturesResident( GLsizei n, const GLuint * textures,
														 GLboolean * residences )
{
	GET_THREAD(thread);
	int writeback = 1;
	GLboolean return_val = GL_TRUE;
	GLsizei i;

	if (!(pack_spu.thread[0].netServer.conn->actual_network))
	{
		crError( "packspu_AreTexturesResident doesn't work when there's no actual network involved!\nTry using the simplequery SPU in your chain!" );
	}

	if (pack_spu.swap)
	{
		crPackAreTexturesResidentSWAP( n, textures, residences, &return_val, &writeback );
	}
	else
	{
		crPackAreTexturesResident( n, textures, residences, &return_val, &writeback );
	}
	packspuFlush( (void *) thread );

	while (writeback)
		crNetRecv();

	/* Since the Chromium packer/unpacker can't return both 'residences'
	 * and the function's return value, compute the return value here.
	 */
	for (i = 0; i < n; i++) {
		if (!residences[i]) {
			return_val = GL_FALSE;
			break;
		}
	}

	return return_val;
}


GLboolean PACKSPU_APIENTRY
packspu_AreProgramsResidentNV( GLsizei n, const GLuint * ids,
															 GLboolean * residences )
{
	GET_THREAD(thread);
	int writeback = 1;
	GLboolean return_val = GL_TRUE;
	GLsizei i;

	if (!(pack_spu.thread[0].netServer.conn->actual_network))
	{
		crError( "packspu_AreProgramsResidentNV doesn't work when there's no actual network involved!\nTry using the simplequery SPU in your chain!" );
	}
	if (pack_spu.swap)
	{
		crPackAreProgramsResidentNVSWAP( n, ids, residences, &return_val, &writeback );
	}
	else
	{
		crPackAreProgramsResidentNV( n, ids, residences, &return_val, &writeback );
	}
	packspuFlush( (void *) thread );

	while (writeback)
		crNetRecv();

	/* Since the Chromium packer/unpacker can't return both 'residences'
	 * and the function's return value, compute the return value here.
	 */
	for (i = 0; i < n; i++) {
		if (!residences[i]) {
			return_val = GL_FALSE;
			break;
		}
	}

	return return_val;
}
