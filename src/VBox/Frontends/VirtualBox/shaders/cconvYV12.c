#extension GL_ARB_texture_rectangle : enable

uniform sampler2DRect uSrcTex;
uniform sampler2DRect uVTex;
uniform sampler2DRect uUTex;

float splitBGRA(vec4 color, float coord);
void cconvApplyAYUV(vec4 color);
/* texture internalFormat: GL_LUMINANCE
 * size: width X height
 * data format: GL_LUMINANCE
 *
/* YV12-rgb888 conversion shader */
void cconvYV12(vec2 srcCoord)
{
    vec4 clrY = texture2DRect(uSrcTex, srcCoord);
    vec4 clrV = texture2DRect(uVTex, vec2(gl_TexCoord[2]));
    vec4 clrU = texture2DRect(uUTex, vec2(gl_TexCoord[3]));
    float y = splitBGRA(clrY, srcCoord.x);
    float v = splitBGRA(clrV, gl_TexCoord[2].x);
    float u = splitBGRA(clrU, gl_TexCoord[3].x);

    /* convert it to AYUV (for the GL_BGRA_EXT texture this is mapped as follows:
     * A -> B
     * Y -> G
     * U -> R
     * V -> A */
    cconvApplyAYUV(vec4(u, y, 0.0, v));
//    gl_FragColor = vec4(clrY.r,clrY.r,clrY.r,1.0);
}
