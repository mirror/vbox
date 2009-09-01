#extension GL_ARB_texture_rectangle : enable

uniform sampler2DRect uSrcTex;

void cconvApplyAYUV(vec4 color);
void cconvAYUV(vec2 srcCoord)
{
    vec4 color = texture2DRect(uSrcTex, srcCoord);
    cconvApplyAYUV(color);
}
