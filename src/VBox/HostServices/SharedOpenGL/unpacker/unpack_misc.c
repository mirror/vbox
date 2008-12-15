/*
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#include "unpacker.h"

void crUnpackExtendChromiumParametervCR( void  )
{
	GLenum target = READ_DATA( 8, GLenum );
	GLenum type = READ_DATA( 12, GLenum );
	GLsizei count = READ_DATA( 16, GLsizei );
	GLvoid *values = DATA_POINTER( 20, GLvoid );

	cr_unpackDispatch.ChromiumParametervCR(target, type, count, values);


	/*
	INCR_VAR_PTR();
	*/
}
