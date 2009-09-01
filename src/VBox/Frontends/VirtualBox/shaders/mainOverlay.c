vec2 ckeyDst(void);
void cconvYV12(vec2 srcCoord);
void cconvUYVY(vec2 srcCoord);
void cconvYUY2(vec2 srcCoord);
void cconvAYUV(vec2 srcCoord);
void cconvBGR(vec2 srcCoord);

void main(void)
{
	vec2 srcCoord = ckeyDst();

	cconvYV12(srcCoord);
	cconvUYVY(srcCoord);
	cconvYUY2(srcCoord);
	cconvAYUV(srcCoord);
	cconvBGR(srcCoord);
}
