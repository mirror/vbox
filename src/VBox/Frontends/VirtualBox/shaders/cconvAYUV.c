#extension GL_ARB_texture_rectangle : enable
uniform sampler2DRect uSrcTex;
void vboxCConvApplyAYUV(vec4 color);
void vboxCConv(int srcI)
{
    vec2 srcCoord = vec2(gl_TexCoord[srcI]);
    vec4 color = texture2DRect(uSrcTex, srcCoord);
    vboxCConvApplyAYUV(color);
}
