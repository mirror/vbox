#extension GL_ARB_texture_rectangle : enable
uniform sampler2DRect uSrcTex;
void vboxCConv(int srcI)
{
    vec2 srcCoord = vec2(gl_TexCoord[srcI]);
    gl_FragColor = texture2DRect(uSrcTex, vec2(srcCoord.x, srcCoord.y));
}
