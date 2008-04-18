/** @file
 *
 * VirtualBox Windows NT/2000/XP guest OpenGL ICD
 *
 * Simple buffered OpenGL functions
 *
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#include "VBoxOGL.h"

/** @todo sync? */
GL_GEN_FUNC1(ReadBuffer, GLenum);

/** @todo sync? */
GL_GEN_VPAR_FUNC5(CopyPixels, GLint, GLint, GLsizei, GLsizei, GLenum);

GL_GEN_FUNC1(Enable,        GLenum);
GL_GEN_FUNC1(Disable,       GLenum);

GL_GEN_VPAR_FUNC2(Accum,        GLenum,     GLfloat);
GL_GEN_VPAR_FUNC2(AlphaFunc,    GLenum,     GLclampf);
GL_GEN_VPAR_FUNC2(BindTexture,  GLenum,     GLuint);

GL_GEN_FUNC1(ArrayElement,  GLint);

GL_GEN_FUNC1(Begin,         GLenum);
GL_GEN_FUNC2(BlendFunc,     GLenum);

GL_GEN_FUNC1(CallList,      GLuint);

GL_GEN_FUNC3V(Color3b,      GLbyte);
GL_GEN_FUNC3V(Color3d,      GLdouble);
GL_GEN_FUNC3V(Color3f,      GLfloat);
GL_GEN_FUNC3V(Color3i,      GLint);
GL_GEN_FUNC3V(Color3s,      GLshort);

GL_GEN_FUNC3V(Color3ub,     GLubyte);
GL_GEN_FUNC3V(Color3ui,     GLuint);
GL_GEN_FUNC3V(Color3us,     GLushort);

GL_GEN_FUNC4V(Color4b,      GLbyte);
GL_GEN_FUNC4V(Color4d,      GLdouble);
GL_GEN_FUNC4V(Color4f,      GLfloat);
GL_GEN_FUNC4V(Color4i,      GLint);
GL_GEN_FUNC4V(Color4s,      GLshort);

GL_GEN_FUNC4V(Color4ub,     GLubyte);
GL_GEN_FUNC4V(Color4ui,     GLuint);
GL_GEN_FUNC4V(Color4us,     GLushort);

GL_GEN_FUNC1(Clear,         GLbitfield);
GL_GEN_FUNC4(ClearAccum,    GLfloat);
GL_GEN_FUNC4(ClearColor,    GLclampf);
GL_GEN_FUNC1(ClearDepth,    GLclampd);
GL_GEN_FUNC1(ClearIndex,    GLfloat);
GL_GEN_FUNC1(ClearStencil,  GLint);

GL_GEN_FUNC2V(Vertex2d,     GLdouble);
GL_GEN_FUNC2V(Vertex2f,     GLfloat);
GL_GEN_FUNC2V(Vertex2i,     GLint);
GL_GEN_FUNC2V(Vertex2s,     GLshort);

GL_GEN_FUNC3V(Vertex3d,     GLdouble);
GL_GEN_FUNC3V(Vertex3f,     GLfloat);
GL_GEN_FUNC3V(Vertex3i,     GLint);
GL_GEN_FUNC3V(Vertex3s,     GLshort);

GL_GEN_FUNC4V(Vertex4d,     GLdouble);
GL_GEN_FUNC4V(Vertex4f,     GLfloat);
GL_GEN_FUNC4V(Vertex4i,     GLint);
GL_GEN_FUNC4V(Vertex4s,     GLshort);

GL_GEN_FUNC1V(TexCoord1d,   GLdouble);
GL_GEN_FUNC1V(TexCoord1f,   GLfloat);
GL_GEN_FUNC1V(TexCoord1i,   GLint);
GL_GEN_FUNC1V(TexCoord1s,   GLshort);

GL_GEN_FUNC2V(TexCoord2d,   GLdouble);
GL_GEN_FUNC2V(TexCoord2f,   GLfloat);
GL_GEN_FUNC2V(TexCoord2i,   GLint);
GL_GEN_FUNC2V(TexCoord2s,   GLshort);

GL_GEN_FUNC3V(TexCoord3d,   GLdouble);
GL_GEN_FUNC3V(TexCoord3f,   GLfloat);
GL_GEN_FUNC3V(TexCoord3i,   GLint);
GL_GEN_FUNC3V(TexCoord3s,   GLshort);

GL_GEN_FUNC4V(TexCoord4d,   GLdouble);
GL_GEN_FUNC4V(TexCoord4f,   GLfloat);
GL_GEN_FUNC4V(TexCoord4i,   GLint);
GL_GEN_FUNC4V(TexCoord4s,   GLshort);

GL_GEN_FUNC3V(Normal3b,     GLbyte);
GL_GEN_FUNC3V(Normal3d,     GLdouble);
GL_GEN_FUNC3V(Normal3f,     GLfloat);
GL_GEN_FUNC3V(Normal3i,     GLint);
GL_GEN_FUNC3V(Normal3s,     GLshort);

GL_GEN_FUNC2V(RasterPos2d,  GLdouble);
GL_GEN_FUNC2V(RasterPos2f,  GLfloat);
GL_GEN_FUNC2V(RasterPos2i,  GLint);
GL_GEN_FUNC2V(RasterPos2s,  GLshort);

GL_GEN_FUNC3V(RasterPos3d,  GLdouble);
GL_GEN_FUNC3V(RasterPos3f,  GLfloat);
GL_GEN_FUNC3V(RasterPos3i,  GLint);
GL_GEN_FUNC3V(RasterPos3s,  GLshort);

GL_GEN_FUNC4V(RasterPos4d,  GLdouble);
GL_GEN_FUNC4V(RasterPos4f,  GLfloat);
GL_GEN_FUNC4V(RasterPos4i,  GLint);
GL_GEN_FUNC4V(RasterPos4s,  GLshort);

GL_GEN_FUNC1V(EvalCoord1d,  GLdouble);
GL_GEN_FUNC1V(EvalCoord1f,  GLfloat);

GL_GEN_FUNC2V(EvalCoord2d,  GLdouble);
GL_GEN_FUNC2V(EvalCoord2f,  GLfloat);

GL_GEN_FUNC1(EvalPoint1,    GLint);
GL_GEN_FUNC2(EvalPoint2,    GLint);

GL_GEN_FUNC1V(Indexd,       GLdouble);
GL_GEN_FUNC1V(Indexf,       GLfloat);
GL_GEN_FUNC1V(Indexi,       GLint);
GL_GEN_FUNC1V(Indexs,       GLshort);
GL_GEN_FUNC1V(Indexub,      GLubyte);

GL_GEN_FUNC4(Rotated,       GLdouble);
GL_GEN_FUNC4(Rotatef,       GLfloat);

GL_GEN_FUNC3(Scaled,        GLdouble);
GL_GEN_FUNC3(Scalef,        GLfloat);

GL_GEN_FUNC3(Translated,    GLdouble);
GL_GEN_FUNC3(Translatef,    GLfloat);

GL_GEN_FUNC1(DepthFunc,     GLenum);
GL_GEN_FUNC1(DepthMask,     GLboolean);

GL_GEN_FUNC1(CullFace,      GLenum);

GL_GEN_VPAR_FUNC2(DeleteLists,      GLuint, GLsizei);

GL_GEN_VPAR_FUNC2(DepthRange,       GLclampd,   GLclampd);
GL_GEN_VPAR_FUNC3(EvalMesh1,        GLenum,     GLint,      GLint);
GL_GEN_VPAR_FUNC5(EvalMesh2,        GLenum,     GLint,      GLint,  GLint,  GLint);
GL_GEN_VPAR_FUNC2(LineStipple,      GLint,      GLushort);
GL_GEN_FUNC1(LineWidth,             GLfloat);
GL_GEN_FUNC1(ListBase,              GLuint);

GL_GEN_VPAR_FUNC2(Fogf,             GLenum,     GLfloat);
GL_GEN_VPAR_FUNC2(Fogi,             GLenum,     GLint);


GL_GEN_VPAR_FUNC2(LightModelf,      GLenum,     GLfloat);
GL_GEN_VPAR_FUNC2(LightModeli,      GLenum,     GLint);


GL_GEN_VPAR_FUNC3(Lightf,           GLenum,     GLenum,     GLfloat);
GL_GEN_VPAR_FUNC3(Lighti,           GLenum,     GLenum,     GLint);


GL_GEN_VPAR_FUNC3(DrawArrays,       GLenum,     GLint,      GLsizei);

GL_GEN_FUNC1(DrawBuffer, GLenum);

GL_GEN_FUNC1V(EdgeFlag, GLboolean);

GL_GEN_FUNC(End);
GL_GEN_FUNC(EndList);

GL_GEN_FUNC1(FrontFace,     GLenum);


GL_GEN_VPAR_FUNC7(CopyTexImage1D, GLenum , GLint , GLenum , GLint , GLint , GLsizei , GLint );

GL_GEN_VPAR_FUNC8(CopyTexImage2D, GLenum , GLint , GLenum , GLint , GLint , GLsizei , GLsizei , GLint );

GL_GEN_VPAR_FUNC6(CopyTexSubImage1D, GLenum , GLint , GLint , GLint , GLint , GLsizei );

GL_GEN_VPAR_FUNC8(CopyTexSubImage2D, GLenum , GLint , GLint , GLint , GLint , GLint , GLsizei , GLsizei );

GL_GEN_VPAR_FUNC4(ColorMask, GLboolean , GLboolean , GLboolean , GLboolean );

GL_GEN_VPAR_FUNC2(ColorMaterial, GLenum, GLenum );

GL_GEN_FUNC1(LogicOp, GLenum);

GL_GEN_VPAR_FUNC3(MapGrid1d, GLint, GLdouble, GLdouble);
GL_GEN_VPAR_FUNC3(MapGrid1f, GLint, GLfloat, GLfloat);

GL_GEN_VPAR_FUNC6(MapGrid2d, GLint , GLdouble , GLdouble , GLint , GLdouble , GLdouble );

GL_GEN_VPAR_FUNC6(MapGrid2f, GLint , GLfloat , GLfloat , GLint , GLfloat , GLfloat );

GL_GEN_VPAR_FUNC3(Materialf, GLenum , GLenum , GLfloat );

GL_GEN_VPAR_FUNC3(Materiali, GLenum , GLenum , GLint );

GL_GEN_FUNC1(MatrixMode, GLenum);

GL_GEN_VPAR_FUNC6(Ortho, GLdouble , GLdouble , GLdouble , GLdouble , GLdouble , GLdouble );

GL_GEN_FUNC1(PassThrough, GLfloat);

GL_GEN_VPAR_FUNC2(PixelStoref, GLenum, GLfloat);
GL_GEN_VPAR_FUNC2(PixelStorei, GLenum, GLint);
GL_GEN_VPAR_FUNC2(PixelTransferf, GLenum, GLfloat);
GL_GEN_VPAR_FUNC2(PixelTransferi, GLenum, GLint);

GL_GEN_FUNC2(PixelZoom, GLfloat);
GL_GEN_FUNC1(PointSize, GLfloat);

GL_GEN_FUNC2(PolygonMode, GLenum)
GL_GEN_FUNC2(PolygonOffset, GLfloat);

GL_GEN_FUNC(PopAttrib);
GL_GEN_FUNC(PopClientAttrib);
GL_GEN_FUNC(PopMatrix);
GL_GEN_FUNC(PopName);


GL_GEN_FUNC1(PushAttrib, GLbitfield);
GL_GEN_FUNC1(PushClientAttrib, GLbitfield);
GL_GEN_FUNC (PushMatrix);
GL_GEN_FUNC1(PushName, GLuint);

GL_GEN_VPAR_FUNC3(TexEnvf, GLenum , GLenum , GLfloat);
GL_GEN_VPAR_FUNC3(TexEnvi, GLenum , GLenum , GLint);
GL_GEN_VPAR_FUNC3(TexGend, GLenum , GLenum , GLdouble);
GL_GEN_VPAR_FUNC3(TexGenf, GLenum , GLenum , GLfloat);
GL_GEN_VPAR_FUNC3(TexGeni, GLenum , GLenum , GLint);

GL_GEN_VPAR_FUNC3(TexParameterf, GLenum , GLenum , GLfloat );
GL_GEN_VPAR_FUNC3(TexParameteri, GLenum , GLenum , GLint );

GL_GEN_FUNC (LoadIdentity);
GL_GEN_FUNC1(LoadName, GLuint);

GL_GEN_FUNC1(ShadeModel, GLenum);
GL_GEN_VPAR_FUNC3(StencilFunc, GLenum, GLint, GLuint);
GL_GEN_FUNC1(StencilMask, GLuint);
GL_GEN_VPAR_FUNC3(StencilOp, GLenum, GLenum, GLenum);

GL_GEN_VPAR_FUNC4(Viewport, GLint, GLint, GLsizei, GLsizei);
GL_GEN_VPAR_FUNC4(Scissor, GLint, GLint, GLsizei, GLsizei);


GL_GEN_FUNC4(Rectd, GLdouble);
GL_GEN_FUNC4(Rectf, GLfloat);
GL_GEN_FUNC4(Recti, GLint);
GL_GEN_FUNC4(Rects, GLshort);

GL_GEN_VPAR_FUNC2(NewList, GLuint, GLenum);

GL_GEN_FUNC2(Hint, GLenum);
GL_GEN_FUNC1(IndexMask, GLuint);
GL_GEN_FUNC(InitNames);


GL_GEN_FUNC6(Frustum, GLdouble);

