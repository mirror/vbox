#ifndef __SHADERLIB_H__
#define __SHADERLIB_H__

#include <VBox/cdefs.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef IN_SHADERLIB_STATIC
#  define SHADERDECL(type)           DECLHIDDEN(type) RTCALL
#else
#  define SHADERDECL(type)           DECLEXPORT(type) RTCALL
#endif


SHADERDECL(int) ShaderInitLib();
SHADERDECL(int) ShaderDestroyLib();

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

#ifdef __cplusplus
}
#endif

#endif /* __SHADERLIB_H__ */
