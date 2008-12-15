/* Copyright (c) 2001, Stanford University
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#include "packer.h"
#include "cr_opcodes.h"
#include "cr_version.h"

/*
 * Expand glArrayElement into crPackVertex/Color/Normal/etc.
 */
void
crPackExpandArrayElement(GLint index, CRClientState *c)
{
	unsigned char *p;
	int unit;

	if (c->array.e.enabled)
	{
		crPackEdgeFlagv(c->array.e.p + index * c->array.e.stride);
	}
	for (unit = 0; unit < CR_MAX_TEXTURE_UNITS; unit++)
	{
		if (c->array.t[unit].enabled)
		{
			p = c->array.t[unit].p + index * c->array.t[unit].stride;
			switch (c->array.t[unit].type)
			{
				case GL_SHORT:
					switch (c->array.t[c->curClientTextureUnit].size)
					{
						case 1: crPackMultiTexCoord1svARB(GL_TEXTURE0_ARB + unit, (GLshort *)p); break;
						case 2: crPackMultiTexCoord2svARB(GL_TEXTURE0_ARB + unit, (GLshort *)p); break;
						case 3: crPackMultiTexCoord3svARB(GL_TEXTURE0_ARB + unit, (GLshort *)p); break;
						case 4: crPackMultiTexCoord4svARB(GL_TEXTURE0_ARB + unit, (GLshort *)p); break;
					}
					break;
				case GL_INT:
					switch (c->array.t[c->curClientTextureUnit].size)
					{
						case 1: crPackMultiTexCoord1ivARB(GL_TEXTURE0_ARB + unit, (GLint *)p); break;
						case 2: crPackMultiTexCoord2ivARB(GL_TEXTURE0_ARB + unit, (GLint *)p); break;
						case 3: crPackMultiTexCoord3ivARB(GL_TEXTURE0_ARB + unit, (GLint *)p); break;
						case 4: crPackMultiTexCoord4ivARB(GL_TEXTURE0_ARB + unit, (GLint *)p); break;
					}
					break;
				case GL_FLOAT:
					switch (c->array.t[c->curClientTextureUnit].size)
					{
						case 1: crPackMultiTexCoord1fvARB(GL_TEXTURE0_ARB + unit, (GLfloat *)p); break;
						case 2: crPackMultiTexCoord2fvARB(GL_TEXTURE0_ARB + unit, (GLfloat *)p); break;
						case 3: crPackMultiTexCoord3fvARB(GL_TEXTURE0_ARB + unit, (GLfloat *)p); break;
						case 4: crPackMultiTexCoord4fvARB(GL_TEXTURE0_ARB + unit, (GLfloat *)p); break;
					}
					break;
				case GL_DOUBLE:
					switch (c->array.t[c->curClientTextureUnit].size)
					{
						case 1: crPackMultiTexCoord1dvARB(GL_TEXTURE0_ARB + unit, (GLdouble *)p); break;
						case 2: crPackMultiTexCoord2dvARB(GL_TEXTURE0_ARB + unit, (GLdouble *)p); break;
						case 3: crPackMultiTexCoord3dvARB(GL_TEXTURE0_ARB + unit, (GLdouble *)p); break;
						case 4: crPackMultiTexCoord4dvARB(GL_TEXTURE0_ARB + unit, (GLdouble *)p); break;
					}
					break;
			}
		}
	} /* loop over texture units */

	if (c->array.i.enabled)
	{
		p = c->array.i.p + index * c->array.i.stride;
		switch (c->array.i.type)
		{
			case GL_SHORT: crPackIndexsv((GLshort *)p); break;
			case GL_INT: crPackIndexiv((GLint *)p); break;
			case GL_FLOAT: crPackIndexfv((GLfloat *)p); break;
			case GL_DOUBLE: crPackIndexdv((GLdouble *)p); break;
		}
	}
	if (c->array.c.enabled)
	{
		p = c->array.c.p + index * c->array.c.stride;
		switch (c->array.c.type)
		{
			case GL_BYTE:
				switch (c->array.c.size)
				{
					case 3: crPackColor3bv((GLbyte *)p); break;
					case 4: crPackColor4bv((GLbyte *)p); break;
				}
				break;
			case GL_UNSIGNED_BYTE:
				switch (c->array.c.size)
				{
					case 3: crPackColor3ubv((GLubyte *)p); break;
					case 4: crPackColor4ubv((GLubyte *)p); break;
				}
				break;
			case GL_SHORT:
				switch (c->array.c.size)
				{
					case 3: crPackColor3sv((GLshort *)p); break;
					case 4: crPackColor4sv((GLshort *)p); break;
				}
				break;
			case GL_UNSIGNED_SHORT:
				switch (c->array.c.size)
				{
					case 3: crPackColor3usv((GLushort *)p); break;
					case 4: crPackColor4usv((GLushort *)p); break;
				}
				break;
			case GL_INT:
				switch (c->array.c.size)
				{
					case 3: crPackColor3iv((GLint *)p); break;
					case 4: crPackColor4iv((GLint *)p); break;
				}
				break;
			case GL_UNSIGNED_INT:
				switch (c->array.c.size)
				{
					case 3: crPackColor3uiv((GLuint *)p); break;
					case 4: crPackColor4uiv((GLuint *)p); break;
				}
				break;
			case GL_FLOAT:
				switch (c->array.c.size)
				{
					case 3: crPackColor3fv((GLfloat *)p); break;
					case 4: crPackColor4fv((GLfloat *)p); break;
				}
				break;
			case GL_DOUBLE:
				switch (c->array.c.size)
				{
					case 3: crPackColor3dv((GLdouble *)p); break;
					case 4: crPackColor4dv((GLdouble *)p); break;
				}
				break;
		}
	}
	if (c->array.n.enabled)
	{
		p = c->array.n.p + index * c->array.n.stride;
		switch (c->array.n.type)
		{
			case GL_BYTE: crPackNormal3bv((GLbyte *)p); break;
			case GL_SHORT: crPackNormal3sv((GLshort *)p); break;
			case GL_INT: crPackNormal3iv((GLint *)p); break;
			case GL_FLOAT: crPackNormal3fv((GLfloat *)p); break;
			case GL_DOUBLE: crPackNormal3dv((GLdouble *)p); break;
		}
	}
#ifdef CR_EXT_secondary_color
	if (c->array.s.enabled)
	{
		p = c->array.s.p + index * c->array.s.stride;
		switch (c->array.s.type)
		{
			case GL_BYTE:
				crPackSecondaryColor3bvEXT((GLbyte *)p); break;
			case GL_UNSIGNED_BYTE:
				crPackSecondaryColor3ubvEXT((GLubyte *)p); break;
			case GL_SHORT:
				crPackSecondaryColor3svEXT((GLshort *)p); break;
			case GL_UNSIGNED_SHORT:
				crPackSecondaryColor3usvEXT((GLushort *)p); break;
			case GL_INT:
				crPackSecondaryColor3ivEXT((GLint *)p); break;
			case GL_UNSIGNED_INT:
				crPackSecondaryColor3uivEXT((GLuint *)p); break;
			case GL_FLOAT:
				crPackSecondaryColor3fvEXT((GLfloat *)p); break;
			case GL_DOUBLE:
				crPackSecondaryColor3dvEXT((GLdouble *)p); break;
		}
	}
#endif
	if (c->array.v.enabled)
	{
		p = c->array.v.p + (index * c->array.v.stride);
		switch (c->array.v.type)
		{
			case GL_SHORT:
				switch (c->array.v.size)
				{
					case 2: crPackVertex2svBBOX_COUNT((GLshort *)p); break;
					case 3: crPackVertex3svBBOX_COUNT((GLshort *)p); break;
					case 4: crPackVertex4svBBOX_COUNT((GLshort *)p); break;
				}
				break;
			case GL_INT:
				switch (c->array.v.size)
				{
					case 2: crPackVertex2ivBBOX_COUNT((GLint *)p); break;
					case 3: crPackVertex3ivBBOX_COUNT((GLint *)p); break;
					case 4: crPackVertex4ivBBOX_COUNT((GLint *)p); break;
				}
				break;
			case GL_FLOAT:
				switch (c->array.v.size)
				{
					case 2: crPackVertex2fvBBOX_COUNT((GLfloat *)p); break;
					case 3: crPackVertex3fvBBOX_COUNT((GLfloat *)p); break;
					case 4: crPackVertex4fvBBOX_COUNT((GLfloat *)p); break;
				}
				break;
			case GL_DOUBLE:
				switch (c->array.v.size)
				{
					case 2: crPackVertex2dvBBOX_COUNT((GLdouble *)p); break;
					case 3: crPackVertex3dvBBOX_COUNT((GLdouble *)p); break;
					case 4: crPackVertex4dvBBOX_COUNT((GLdouble *)p); break;
				}
				break;
		}
	}
}


void
crPackExpandDrawArrays(GLenum mode, GLint first, GLsizei count, CRClientState *c)
{
	int i;

	if (count < 0)
	{
		__PackError(__LINE__, __FILE__, GL_INVALID_VALUE, "crPackDrawArrays(negative count)");
		return;
	}

	if (mode > GL_POLYGON)
	{
		__PackError(__LINE__, __FILE__, GL_INVALID_ENUM, "crPackDrawArrays(bad mode)");
		return;
	}

	crPackBegin(mode);
	for (i=0; i<count; i++)
	{
		crPackExpandArrayElement(first + i, c);
	}
	crPackEnd();
}


/*
 * Really pack glDrawElements (for server-side vertex arrays)
 * Note: we pack the pointer (which is actually an index into the server-side
 * vertex buffer) and not the actual indices.
 */
void PACK_APIENTRY
crPackDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices)
{
	unsigned char *data_ptr;
	int packet_length = sizeof(int) + sizeof(mode) + sizeof(count) + sizeof(type) + sizeof(GLintptrARB);
	data_ptr = (unsigned char *) crPackAlloc(packet_length);
	WRITE_DATA( 0, GLenum, CR_DRAWELEMENTS_EXTEND_OPCODE );
	WRITE_DATA( 4, GLenum, mode );
	WRITE_DATA( 8, GLsizei, count);
	WRITE_DATA( 12, GLenum, type);
	WRITE_DATA( 16, GLsizeiptrARB, (GLsizeiptrARB) indices );
	crHugePacket( CR_EXTEND_OPCODE, data_ptr );
	crPackFree( data_ptr );
}


/**
 * Expand glDrawElements into crPackBegin/Vertex/End, etc commands.
 * Note: if mode==999, don't call glBegin/glEnd.
 */
void
crPackExpandDrawElements(GLenum mode, GLsizei count, GLenum type,
												 const GLvoid *indices, CRClientState *c)
{
	int i;

	if (count < 0)
	{
		__PackError(__LINE__, __FILE__, GL_INVALID_VALUE,
								"crPackDrawElements(negative count)");
		return;
	}

	if (mode > GL_POLYGON && mode != 999)
	{
		__PackError(__LINE__, __FILE__, GL_INVALID_ENUM,
								"crPackDrawElements(bad mode)");
		return;
	}

	if (type != GL_UNSIGNED_BYTE &&
			type != GL_UNSIGNED_SHORT &&
			type != GL_UNSIGNED_INT)
	{
		__PackError(__LINE__, __FILE__, GL_INVALID_ENUM,
								"crPackDrawElements(bad type)");
		return;
	}

	if (mode != 999)
		crPackBegin(mode);

	switch (type)	{
	case GL_UNSIGNED_BYTE:
		{
			const GLubyte *p = (const GLubyte *) indices;
			for (i = 0; i < count; i++)
				crPackExpandArrayElement(p[i], c);
		}
		break;
	case GL_UNSIGNED_SHORT:
		{
			const GLushort *p = (const GLushort *) indices;
			for (i=0; i<count; i++)
				crPackExpandArrayElement(p[i], c);
		}
		break;
	case GL_UNSIGNED_INT:
		{
			const GLuint *p = (const GLuint *) indices;
			for (i = 0; i < count; i++)
				crPackExpandArrayElement(p[i], c);
		}
		break;
	default:
		__PackError( __LINE__, __FILE__, GL_INVALID_ENUM,
								 "crPackDrawElements(bad type)");
		break;
	}

	if (mode != 999)
		crPackEnd();
}


/**
 * Convert a glDrawElements command into a sequence of ArrayElement() calls.
 * NOTE: Caller must issue the glBegin/glEnd.
 */
void
crPackUnrollDrawElements(GLsizei count, GLenum type,
												 const GLvoid *indices)
{
	int i;

	switch (type) {
	case GL_UNSIGNED_BYTE:
		{
			const GLubyte *p = (const GLubyte *) indices;
			for (i = 0; i < count; i++)
				crPackArrayElement(p[i]);
		}
		break;
	case GL_UNSIGNED_SHORT:
		{
			const GLushort *p = (const GLushort *) indices;
			for (i = 0; i < count; i++) 
				crPackArrayElement(p[i]);
		}
		break;
	case GL_UNSIGNED_INT:
		{
			const GLuint *p = (const GLuint *) indices;
			for (i = 0; i < count; i++) 
				crPackArrayElement(p[i]);
		}
		break;
	default:
		__PackError(__LINE__, __FILE__, GL_INVALID_ENUM,
								 "crPackUnrollDrawElements(bad type)");
	}
}



/*
 * Really pack glDrawRangeElements (for server-side vertex arrays)
 * Note: we pack the pointer (which is actually an index into the server-side
 * vertex buffer) and not the actual indices.
 */
void PACK_APIENTRY
crPackDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count,
												GLenum type, const GLvoid *indices)
{
	unsigned char *data_ptr;
	int packet_length = sizeof(int) + sizeof(mode) + sizeof(start)
		+ sizeof(end) + sizeof(count) + sizeof(type) + sizeof(GLintptrARB);

	data_ptr = (unsigned char *) crPackAlloc(packet_length);
	WRITE_DATA( 0, GLenum, CR_DRAWRANGEELEMENTS_EXTEND_OPCODE );
	WRITE_DATA( 4, GLenum, mode );
	WRITE_DATA( 8, GLuint, start );
	WRITE_DATA( 12, GLuint, end );
	WRITE_DATA( 16, GLsizei, count );
	WRITE_DATA( 20, GLenum, type );
	WRITE_DATA( 24, GLsizeiptrARB, (GLsizeiptr) indices );
	crHugePacket( CR_EXTEND_OPCODE, data_ptr );
	crPackFree( data_ptr );
}


/*
 * glDrawRangeElements, expanded into crPackBegin/Vertex/End/etc.
 */
void
crPackExpandDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices, CRClientState *c)
{
	int i;
	GLubyte *p = (GLubyte *)indices;

	(void) end;

	if (count < 0)
	{
		__PackError(__LINE__, __FILE__, GL_INVALID_VALUE, "crPackDrawRangeElements(negative count)");
		return;
	}

	if (mode > GL_POLYGON)
	{
		__PackError(__LINE__, __FILE__, GL_INVALID_ENUM, "crPackDrawRangeElements(bad mode)");
		return;
	}

	if (type != GL_UNSIGNED_BYTE && type != GL_UNSIGNED_SHORT && type != GL_UNSIGNED_INT)
	{
		__PackError(__LINE__, __FILE__, GL_INVALID_ENUM, "crPackDrawRangeElements(bad type)");
		return;
	}

	crPackBegin(mode);
	switch (type)
	{
	case GL_UNSIGNED_BYTE:
		for (i=start; i<count; i++)
		{
			crPackExpandArrayElement((GLint) *p++, c);
		}
		break;
	case GL_UNSIGNED_SHORT:
		for (i=start; i<count; i++)
		{
			crPackExpandArrayElement((GLint) * (GLushort *) p, c);
			p += sizeof(GLushort);
		}
		break;
	case GL_UNSIGNED_INT:
		for (i=start; i<count; i++)
		{
			crPackExpandArrayElement((GLint) * (GLuint *) p, c);
			p += sizeof(GLuint);
		}
		break;
	default:
		crError( "this can't happen: crPackDrawRangeElements" );
		break;
	}
	crPackEnd();
}


#ifdef CR_EXT_multi_draw_arrays
/*
 * Pack real DrawArrays commands.
 */
void PACK_APIENTRY
crPackMultiDrawArraysEXT( GLenum mode, GLint *first, GLsizei *count,
													GLsizei primcount )
{
   GLint i;
   for (i = 0; i < primcount; i++) {
      if (count[i] > 0) {
         crPackDrawArrays(mode, first[i], count[i]);
      }
   }
}


/*
 * Pack with crPackBegin/Vertex/End/etc.
 */
void
crPackExpandMultiDrawArraysEXT( GLenum mode, GLint *first, GLsizei *count,
																GLsizei primcount, CRClientState *c )
{
   GLint i;
   for (i = 0; i < primcount; i++) {
      if (count[i] > 0) {
         crPackExpandDrawArrays(mode, first[i], count[i], c);
      }
   }
}


/*
 * Pack real DrawElements commands.
 */
void PACK_APIENTRY
crPackMultiDrawElementsEXT( GLenum mode, const GLsizei *count, GLenum type,
                            const GLvoid **indices, GLsizei primcount )
{
   GLint i;
   for (i = 0; i < primcount; i++) {
      if (count[i] > 0) {
         crPackDrawElements(mode, count[i], type, indices[i]);
      }
   }
}


/*
 * Pack with crPackBegin/Vertex/End/etc.
 */
void
crPackExpandMultiDrawElementsEXT( GLenum mode, const GLsizei *count,
																	GLenum type, const GLvoid **indices,
																	GLsizei primcount, CRClientState *c )
{
   GLint i;
   for (i = 0; i < primcount; i++) {
      if (count[i] > 0) {
         crPackExpandDrawElements(mode, count[i], type, indices[i], c);
      }
   }
}
#endif /* CR_EXT_multi_draw_arrays */
