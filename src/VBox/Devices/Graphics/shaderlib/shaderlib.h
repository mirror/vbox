/* $Id$ */
/** @file
 * shaderlib -- interface to WINE's Direct3D shader functions
 */

/*
 * Copyright (C) 2014-2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


#ifndef ___shaderlib_h___
#define ___shaderlib_h___

#include <VBox/cdefs.h>

RT_C_DECLS_BEGIN

#ifdef IN_SHADERLIB_STATIC
# define SHADERDECL(type)           DECLHIDDEN(type) RTCALL
#else
# define SHADERDECL(type)           DECLEXPORT(type) RTCALL
#endif


SHADERDECL(int) ShaderInitLib(void);
SHADERDECL(int) ShaderDestroyLib(void);

SHADERDECL(int) ShaderContextCreate(void **ppShaderContext);
SHADERDECL(int) ShaderContextDestroy(void *pShaderContext);

SHADERDECL(int) ShaderCreateVertexShader(void *pShaderContext, const uint32_t *pShaderData, void **pShaderObj);
SHADERDECL(int) ShaderCreatePixelShader(void *pShaderContext, const uint32_t *pShaderData, void **pShaderObj);

SHADERDECL(int) ShaderDestroyVertexShader(void *pShaderContext, void *pShaderObj);
SHADERDECL(int) ShaderDestroyPixelShader(void *pShaderContext, void *pShaderObj);

SHADERDECL(int) ShaderSetVertexShader(void *pShaderContext, void *pShaderObj);
SHADERDECL(int) ShaderSetPixelShader(void *pShaderContext, void *pShaderObj);

SHADERDECL(int) ShaderSetVertexShaderConstantB(void *pShaderContext, uint32_t reg, const uint8_t *pValues, uint32_t cRegisters);
SHADERDECL(int) ShaderSetVertexShaderConstantI(void *pShaderContext, uint32_t reg, const int32_t *pValues, uint32_t cRegisters);
SHADERDECL(int) ShaderSetVertexShaderConstantF(void *pShaderContext, uint32_t reg, const float *pValues, uint32_t cRegisters);

SHADERDECL(int) ShaderSetPixelShaderConstantB(void *pShaderContext, uint32_t reg, const uint8_t *pValues, uint32_t cRegisters);
SHADERDECL(int) ShaderSetPixelShaderConstantI(void *pShaderContext, uint32_t reg, const int32_t *pValues, uint32_t cRegisters);
SHADERDECL(int) ShaderSetPixelShaderConstantF(void *pShaderContext, uint32_t reg, const float *pValues, uint32_t cRegisters);

SHADERDECL(int) ShaderUpdateState(void *pShaderContext, uint32_t rtHeight);

SHADERDECL(int) ShaderTransformProjection(unsigned cxViewPort, unsigned cyViewPort, float matrix[16]);

RT_C_DECLS_END

#endif /* !___shaderlib_h___ */

