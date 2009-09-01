#extension GL_ARB_texture_rectangle : enable

uniform sampler2DRect uSrcTex;

void cconvApplyAYUV(vec4 color);
/* texture internalFormat: 4
 * size: width/2 X height
 * data format: GL_BGRA_EXT
 * YUYV ->
 * U = G
 * V = A
 * for even -> Y = B
 * for odd  -> Y = R
/* UYVY-rgb888 conversion shader */
/* this is also YUNV, V422 and YUYV */
void cconvYUY2(vec2 srcCoord)
{
    float x = srcCoord.x;
    vec4 srcClr = texture2DRect(uSrcTex, vec2(x, srcCoord.y));
    float u = srcClr.g;
    float v = srcClr.a;
    int pix = int(x);
    float part = x - float(pix);
    float y;
    if(part < 0.5)
    {
        y = srcClr.b;
    }
    else
    {
        y = srcClr.r;
    }
    /* convert it to AYUV (for the GL_BGRA_EXT texture this is mapped as follows:
     * A -> B
     * Y -> G
     * U -> R
     * V -> A */
    cconvApplyAYUV(vec4(u, y, 0.0, v));
}
