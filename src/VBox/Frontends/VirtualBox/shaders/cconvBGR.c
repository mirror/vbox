#extension GL_ARB_texture_rectangle : enable

uniform sampler2DRect uSrcTex;

void cconvBGR(vec2 srcCoord)
{
    gl_FragColor = texture2DRect(uSrcTex, vec2(srcCoord.x, srcCoord.y));
}
