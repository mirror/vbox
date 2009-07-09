/* Copyright (c) 2001, Stanford University
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#include "packer.h"
#include "cr_opcodes.h"
#include "cr_version.h"
#include "state/cr_limits.h"
#include "cr_glstate.h"

/*Convert from GLint to GLfloat in [-1.f,1.f]*/
#define CRP_I2F_NORM(i) ((2.f*((GLint)(i))+1.f) * (1.f/4294967294.f))
/*Convert from GLshort to GLfloat in [-1.f,1.f]*/
#define CRP_S2F_NORM(s) ((2.f*((GLshort)(s))+1.f) * (1.f/65535.f))


static void crPackVertexAttrib(const CRVertexArrays *array, unsigned int attr, GLint index)
{
    GLint *iPtr;
    GLshort *sPtr;
    unsigned char *p = array->a[attr].p + index * array->a[attr].stride;

#ifdef CR_ARB_vertex_buffer_object
    if (array->a[attr].buffer && array->a[attr].buffer->data)
    {
        p = (unsigned char *)(array->a[attr].buffer->data) + (unsigned long)p;
    }
#endif
    switch (array->a[attr].type)
    {
        case GL_SHORT:
            sPtr = (GLshort*) p;
            switch (array->a[attr].size)
            {
                case 1:
                    if (array->a[attr].normalized)
                        crPackVertexAttrib1fARB(attr, CRP_S2F_NORM(sPtr[0]));
                    else
                        crPackVertexAttrib1svARB(attr, sPtr);
                    break;
                case 2:
                    if (array->a[attr].normalized)
                        crPackVertexAttrib2fARB(attr, CRP_S2F_NORM(sPtr[0]), CRP_S2F_NORM(sPtr[1]));
                    else
                        crPackVertexAttrib2svARB(attr, sPtr);
                    break;
                case 3:
                    if (array->a[attr].normalized)
                        crPackVertexAttrib3fARB(attr, CRP_S2F_NORM(sPtr[0]), CRP_S2F_NORM(sPtr[1]), CRP_S2F_NORM(sPtr[2]));
                    else
                        crPackVertexAttrib3svARB(attr, sPtr);
                    break;
                case 4: 
                    if (array->a[attr].normalized)
                        crPackVertexAttrib4NsvARB(attr, sPtr);
                    else
                        crPackVertexAttrib4svARB(attr, sPtr);
                    break;
            }
            break;
        case GL_INT:
            iPtr = (GLint*) p;
            switch (array->a[attr].size)
            {
                case 1:
                    if (array->a[attr].normalized)
                        crPackVertexAttrib1fARB(attr, CRP_I2F_NORM(iPtr[0]));
                    else
                        crPackVertexAttrib1fARB(attr, iPtr[0]);
                    break;
                case 2:
                    if (array->a[attr].normalized)
                        crPackVertexAttrib2fARB(attr, CRP_I2F_NORM(iPtr[0]), CRP_I2F_NORM(iPtr[1]));
                    else
                        crPackVertexAttrib2fARB(attr, iPtr[0], iPtr[1]);
                    break;
                case 3:
                    if (array->a[attr].normalized)
                        crPackVertexAttrib3fARB(attr, CRP_I2F_NORM(iPtr[0]), CRP_I2F_NORM(iPtr[1]), CRP_I2F_NORM(iPtr[2]));
                    else
                        crPackVertexAttrib3fARB(attr, iPtr[0], iPtr[1], iPtr[2]);
                    break;
                case 4: 
                    if (array->a[attr].normalized)
                        crPackVertexAttrib4NivARB(attr, iPtr);
                    else
                        crPackVertexAttrib4fARB(attr, iPtr[0], iPtr[1], iPtr[2], iPtr[3]);
                    break;
            }
            break;
        case GL_FLOAT:
            switch (array->a[attr].size)
            {
                case 1: crPackVertexAttrib1fvARB(attr, (GLfloat *)p); break;
                case 2: crPackVertexAttrib2fvARB(attr, (GLfloat *)p); break;
                case 3: crPackVertexAttrib3fvARB(attr, (GLfloat *)p); break;
                case 4: crPackVertexAttrib4fvARB(attr, (GLfloat *)p); break;
            }
            break;
        case GL_DOUBLE:
            switch (array->a[attr].size)
            {
                case 1: crPackVertexAttrib1dvARB(attr, (GLdouble *)p); break;
                case 2: crPackVertexAttrib2dvARB(attr, (GLdouble *)p); break;
                case 3: crPackVertexAttrib3dvARB(attr, (GLdouble *)p); break;
                case 4: crPackVertexAttrib4dvARB(attr, (GLdouble *)p); break;
            }
            break;
        default:
            crWarning("Bad datatype for vertex attribute [%d] array: 0x%x\n",
                       attr, array->a[attr].type);
    }
}

/*
 * Expand glArrayElement into crPackVertex/Color/Normal/etc.
 */
void
crPackExpandArrayElement(GLint index, CRClientState *c)
{
    unsigned char *p;
    unsigned int unit, attr;
    const CRVertexArrays *array = &(c->array);
    const GLboolean vpEnabled = crStateGetCurrent()->program.vpEnabled;

    if (array->n.enabled && !(vpEnabled && array->a[VERT_ATTRIB_NORMAL].enabled))
    {
        p = array->n.p + index * array->n.stride;

#ifdef CR_ARB_vertex_buffer_object
        if (array->n.buffer && array->n.buffer->data)
        {
            p = (unsigned char *)(array->n.buffer->data) + (unsigned long)p;
        }
#endif

        switch (array->n.type)
        {
            case GL_BYTE: crPackNormal3bv((GLbyte *)p); break;
            case GL_SHORT: crPackNormal3sv((GLshort *)p); break;
            case GL_INT: crPackNormal3iv((GLint *)p); break;
            case GL_FLOAT: crPackNormal3fv((GLfloat *)p); break;
            case GL_DOUBLE: crPackNormal3dv((GLdouble *)p); break;
        }
    }

    if (array->c.enabled && !(vpEnabled && array->a[VERT_ATTRIB_COLOR0].enabled))
    {
        p = array->c.p + index * array->c.stride;

#ifdef CR_ARB_vertex_buffer_object
        if (array->c.buffer && array->c.buffer->data)
        {
            p = (unsigned char *)(array->c.buffer->data) + (unsigned long)p;
        }
#endif

        switch (array->c.type)
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

#ifdef CR_EXT_secondary_color
    if (array->s.enabled && !(vpEnabled && array->a[VERT_ATTRIB_COLOR1].enabled))
    {
        p = array->s.p + index * array->s.stride;

#ifdef CR_ARB_vertex_buffer_object
        if (array->s.buffer && array->s.buffer->data)
        {
            p = (unsigned char *)(array->s.buffer->data) + (unsigned long)p;
        }
#endif

        switch (array->s.type)
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
#endif // CR_EXT_secondary_color


#ifdef CR_EXT_fog_coord
    if (array->f.enabled && !(vpEnabled && array->a[VERT_ATTRIB_FOG].enabled))
    {
        p = array->f.p + index * array->f.stride;

#ifdef CR_ARB_vertex_buffer_object
        if (array->f.buffer && array->f.buffer->data)
        {
            p = (unsigned char *)(array->f.buffer->data) + (unsigned long)p;
        }
#endif
        crPackFogCoordfEXT( *((GLfloat *) p) );
    }
#endif // CR_EXT_fog_coord

    for (unit = 0 ; unit < CR_MAX_TEXTURE_UNITS ; unit++)
    {
        if (array->t[unit].enabled && !(vpEnabled && array->a[VERT_ATTRIB_TEX0+unit].enabled))
        {
            p = array->t[unit].p + index * array->t[unit].stride;

#ifdef CR_ARB_vertex_buffer_object
            if (array->t[unit].buffer && array->t[unit].buffer->data)
            {
                p = (unsigned char *)(array->t[unit].buffer->data) + (unsigned long)p;
            }
#endif

            switch (array->t[unit].type)
            {
                case GL_SHORT:
                    switch (array->t[unit].size)
                    {
                        case 1: crPackMultiTexCoord1svARB(GL_TEXTURE0_ARB + unit, (GLshort *)p); break;
                        case 2: crPackMultiTexCoord2svARB(GL_TEXTURE0_ARB + unit, (GLshort *)p); break;
                        case 3: crPackMultiTexCoord3svARB(GL_TEXTURE0_ARB + unit, (GLshort *)p); break;
                        case 4: crPackMultiTexCoord4svARB(GL_TEXTURE0_ARB + unit, (GLshort *)p); break;
                    }
                    break;
                case GL_INT:
                    switch (array->t[unit].size)
                    {
                        case 1: crPackMultiTexCoord1ivARB(GL_TEXTURE0_ARB + unit, (GLint *)p); break;
                        case 2: crPackMultiTexCoord2ivARB(GL_TEXTURE0_ARB + unit, (GLint *)p); break;
                        case 3: crPackMultiTexCoord3ivARB(GL_TEXTURE0_ARB + unit, (GLint *)p); break;
                        case 4: crPackMultiTexCoord4ivARB(GL_TEXTURE0_ARB + unit, (GLint *)p); break;
                    }
                    break;
                case GL_FLOAT:
                    switch (array->t[unit].size)
                    {
                        case 1: crPackMultiTexCoord1fvARB(GL_TEXTURE0_ARB + unit, (GLfloat *)p); break;
                        case 2: crPackMultiTexCoord2fvARB(GL_TEXTURE0_ARB + unit, (GLfloat *)p); break;
                        case 3: crPackMultiTexCoord3fvARB(GL_TEXTURE0_ARB + unit, (GLfloat *)p); break;
                        case 4: crPackMultiTexCoord4fvARB(GL_TEXTURE0_ARB + unit, (GLfloat *)p); break;
                    }
                    break;
                case GL_DOUBLE:
                    switch (array->t[unit].size)
                    {
                        case 1: crPackMultiTexCoord1dvARB(GL_TEXTURE0_ARB + unit, (GLdouble *)p); break;
                        case 2: crPackMultiTexCoord2dvARB(GL_TEXTURE0_ARB + unit, (GLdouble *)p); break;
                        case 3: crPackMultiTexCoord3dvARB(GL_TEXTURE0_ARB + unit, (GLdouble *)p); break;
                        case 4: crPackMultiTexCoord4dvARB(GL_TEXTURE0_ARB + unit, (GLdouble *)p); break;
                    }
                    break;
            }
        }
    }

    if (array->i.enabled)
    {
        p = array->i.p + index * array->i.stride;

#ifdef CR_ARB_vertex_buffer_object
        if (array->i.buffer && array->i.buffer->data)
        {
            p = (unsigned char *)(array->i.buffer->data) + (unsigned long)p;
        }
#endif

        switch (array->i.type)
        {
            case GL_SHORT: crPackIndexsv((GLshort *)p); break;
            case GL_INT: crPackIndexiv((GLint *)p); break;
            case GL_FLOAT: crPackIndexfv((GLfloat *)p); break;
            case GL_DOUBLE: crPackIndexdv((GLdouble *)p); break;
        }
    }

    if (array->e.enabled)
    {
        p = array->e.p + index * array->e.stride;

#ifdef CR_ARB_vertex_buffer_object
        if (array->e.buffer && array->e.buffer->data)
        {
            p = (unsigned char *)(array->e.buffer->data) + (unsigned long)p;
        }
#endif

        crPackEdgeFlagv(p);
    }

    for (attr = 1; attr < VERT_ATTRIB_MAX; attr++)
    {
        if (array->a[attr].enabled)
        {
            crPackVertexAttrib(array, attr, index);
        }
    }

    if (array->a[VERT_ATTRIB_POS].enabled)
    {
        crPackVertexAttrib(array, 0, index);
    }
    else if (array->v.enabled)
    {
        p = array->v.p + index * array->v.stride;

#ifdef CR_ARB_vertex_buffer_object
        if (array->v.buffer && array->v.buffer->data)
        {
            p = (unsigned char *)(array->v.buffer->data) + (unsigned long)p;
        }
#endif
        switch (array->v.type)
        {
            case GL_SHORT:
                switch (c->array.v.size)
                {
                    case 2: crPackVertex2sv((GLshort *)p); break;
                    case 3: crPackVertex3sv((GLshort *)p); break;
                    case 4: crPackVertex4sv((GLshort *)p); break;
                }
                break;
            case GL_INT:
                switch (c->array.v.size)
                {
                    case 2: crPackVertex2iv((GLint *)p); break;
                    case 3: crPackVertex3iv((GLint *)p); break;
                    case 4: crPackVertex4iv((GLint *)p); break;
                }
                break;
            case GL_FLOAT:
                switch (c->array.v.size)
                {
                    case 2: crPackVertex2fv((GLfloat *)p); break;
                    case 3: crPackVertex3fv((GLfloat *)p); break;
                    case 4: crPackVertex4fv((GLfloat *)p); break;
                }
                break;
            case GL_DOUBLE:
                switch (c->array.v.size)
                {
                    case 2: crPackVertex2dv((GLdouble *)p); break;
                    case 3: crPackVertex3dv((GLdouble *)p); break;
                    case 4: crPackVertex4dv((GLdouble *)p); break;
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

static GLsizei crPackElementsIndexSize(GLenum type)
{
    switch (type)
    {
        case GL_UNSIGNED_BYTE:
            return sizeof(GLubyte);
        case GL_UNSIGNED_SHORT:
            return sizeof(GLushort);
        case GL_UNSIGNED_INT:
            return sizeof(GLuint);
        default:
            crError("Unknown type 0x%x in crPackElementsIndexSize", type);
            return 0;
    }
}

void PACK_APIENTRY
crPackDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices)
{
    unsigned char *data_ptr, *start_ptr;
    int packet_length = sizeof(int) + sizeof(mode) + sizeof(count) + sizeof(type) + sizeof(GLuint);
    GLsizei indexsize;
#ifdef CR_ARB_vertex_buffer_object
    CRBufferObject *elementsBuffer = crStateGetCurrent()->bufferobject.elementsBuffer;
    packet_length += sizeof(GLint);
    if (elementsBuffer && elementsBuffer->name)
    {
        /*@todo not sure it's possible, and not sure what to do*/
        if (!elementsBuffer->data)
        {
            crWarning("crPackDrawElements: trying to use bound but empty elements buffer, ignoring.");
            return;
        }
        indexsize = 0;
    }
    else
#endif
    {
        indexsize = crPackElementsIndexSize(type);
    }

    packet_length += count * indexsize;

    start_ptr = data_ptr = (unsigned char *) crPackAlloc(packet_length);
    WRITE_DATA_AI(GLenum, CR_DRAWELEMENTS_EXTEND_OPCODE );
    WRITE_DATA_AI(GLenum, mode );
    WRITE_DATA_AI(GLsizei, count);
    WRITE_DATA_AI(GLenum, type);
    WRITE_DATA_AI(GLuint, (GLuint) indices );
#ifdef CR_ARB_vertex_buffer_object
    WRITE_DATA_AI(GLint, (GLint)(indexsize>0));
#endif
    if (indexsize>0)
    {
        crMemcpy(data_ptr, indices, count * indexsize);
    }
    crHugePacket(CR_EXTEND_OPCODE, start_ptr);
    crPackFree(start_ptr);
}

void PACK_APIENTRY
crPackDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count,
                                                GLenum type, const GLvoid *indices)
{
    unsigned char *data_ptr, *start_ptr;
    int packet_length = sizeof(int) + sizeof(mode) + sizeof(start)
        + sizeof(end) + sizeof(count) + sizeof(type) + sizeof(GLuint);
    GLsizei indexsize;

#ifdef CR_ARB_vertex_buffer_object
    CRBufferObject *elementsBuffer = crStateGetCurrent()->bufferobject.elementsBuffer;
    packet_length += sizeof(GLint);
    if (elementsBuffer && elementsBuffer->name)
    {
        /*@todo not sure it's possible, and not sure what to do*/
        if (!elementsBuffer->data)
        {
            crWarning("crPackDrawElements: trying to use bound but empty elements buffer, ignoring.");
            return;
        }
        indexsize = 0;
    }
    else
#endif
    {
      indexsize = crPackElementsIndexSize(type);
    }

    packet_length += count * indexsize;

    start_ptr = data_ptr = (unsigned char *) crPackAlloc(packet_length);
    WRITE_DATA_AI(GLenum, CR_DRAWRANGEELEMENTS_EXTEND_OPCODE);
    WRITE_DATA_AI(GLenum, mode);
    WRITE_DATA_AI(GLuint, start);
    WRITE_DATA_AI(GLuint, end);
    WRITE_DATA_AI(GLsizei, count);
    WRITE_DATA_AI(GLenum, type);
    WRITE_DATA_AI(GLuint, (GLuint) indices);
#ifdef CR_ARB_vertex_buffer_object
    WRITE_DATA_AI(GLint, (GLint) (indexsize>0));
#endif
    if (indexsize>0)
    {
        crMemcpy(data_ptr, indices, count * indexsize);
    }
    crHugePacket(CR_EXTEND_OPCODE, start_ptr);
    crPackFree(start_ptr);
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
    GLubyte *p = (GLubyte *)indices;
#ifdef CR_ARB_vertex_buffer_object
    CRBufferObject *elementsBuffer = crStateGetCurrent()->bufferobject.elementsBuffer;
#endif

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

#ifdef CR_ARB_vertex_buffer_object
    if (elementsBuffer && elementsBuffer->data)
    {
        p = (unsigned char *)(elementsBuffer->data) + (unsigned long)p;
    }
#endif

    if (mode != 999)
        crPackBegin(mode);

    //crDebug("crPackExpandDrawElements mode:0x%x, count:%d, type:0x%x", mode, count, type);

    switch (type)
    {
        case GL_UNSIGNED_BYTE:
            for (i=0; i<count; i++)
            {
                crPackExpandArrayElement((GLint) *p++, c);
            }
            break;
        case GL_UNSIGNED_SHORT:
            for (i=0; i<count; i++)
            {
                crPackExpandArrayElement((GLint) * (GLushort *) p, c);
                p+=sizeof (GLushort);
            }
            break;
        case GL_UNSIGNED_INT:
            for (i=0; i<count; i++)
            {
                crPackExpandArrayElement((GLint) * (GLuint *) p, c);
                p+=sizeof (GLuint);
            }
            break;
        default:
            crError( "this can't happen: array_spu.self.DrawElements" );
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
 * glDrawRangeElements, expanded into crPackBegin/Vertex/End/etc.
 */
void
crPackExpandDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices, CRClientState *c)
{
    if (start>end)
    {
        crWarning("crPackExpandDrawRangeElements start>end (%d>%d)", start, end);
        return;
    }

    crPackExpandDrawElements(mode, count, type, indices, c);
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
