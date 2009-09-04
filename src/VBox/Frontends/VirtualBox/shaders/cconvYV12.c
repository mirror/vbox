#extension GL_ARB_texture_rectangle : enable
uniform sampler2DRect uSrcTex;
uniform sampler2DRect uVTex;
uniform sampler2DRect uUTex;
float vboxSplitBGRA(vec4 color, float coord);
void vboxCConvApplyAYUV(vec4 color);
void vboxCConv(int srcI)
{
    vec2 coordY = vec2(gl_TexCoord[srcI]);
    vec2 coordV = vec2(gl_TexCoord[srcI+1]);
    vec2 coordU = vec2(gl_TexCoord[srcI+2]);
    vec4 clrY = texture2DRect(uSrcTex, vec2(coordY));
    vec4 clrV = texture2DRect(uVTex, vec2(coordV));
    vec4 clrU = texture2DRect(uUTex, vec2(coordU));
    float y = vboxSplitBGRA(clrY, coordY.x);
    float v = vboxSplitBGRA(clrV, coordV.x);
    float u = vboxSplitBGRA(clrU, coordU.x);
    vboxCConvApplyAYUV(vec4(u, y, 0.0, v));
}
