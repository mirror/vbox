/* Copyright (c) 2001, Stanford University
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#include "unpacker.h"

void crUnpackPolygonStipple( void  )
{
	GLubyte *mask = DATA_POINTER( 0, GLubyte );

	cr_unpackDispatch.PolygonStipple( mask );
	INCR_DATA_PTR( 32*32/8 );
}
