#extension GL_ARB_texture_rectangle : enable
uniform sampler2DRect uSrcTex;
uniform sampler2DRect uVTex;
uniform sampler2DRect uUTex;
float vboxSplitBGRA(vec4 color, float coord);
void vboxCConvApplyAYUV(vec4 color);
void vboxCConv()
{
    vec2 clrCoordY = vec2(gl_TexCoord[0]);
    vec2 clrCoordV = vec2(gl_TexCoord[1]);
    int ix = int(clrCoordY.x);
    vec2 coordY = vec2(float(ix), clrCoordY.y);
    ix = int(clrCoordV.x);
    vec2 coordV = vec2(float(ix), clrCoordV.y);
    vec4 clrY = texture2DRect(uSrcTex, vec2(coordY));
    vec4 clrV = texture2DRect(uVTex, vec2(coordV));
    vec4 clrU = texture2DRect(uUTex, vec2(coordV));
    float y = vboxSplitBGRA(clrY, clrCoordY.x);
    float v = vboxSplitBGRA(clrV, clrCoordV.x);
    float u = vboxSplitBGRA(clrU, clrCoordV.x);
    vboxCConvApplyAYUV(vec4(u, y, 0.0, v));
}
