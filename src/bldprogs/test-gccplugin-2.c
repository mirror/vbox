/* Only valid stuff in this one. */
extern void MyIprtPrintf(const char *pszFormat, ...) __attribute__((__iprt_format__(1,2)));
extern void foo(void);

extern unsigned long long g_ull;

typedef unsigned long long RTGCPHYS;
extern RTGCPHYS g_GCPhys;

typedef RTGCPHYS *PRTGCPHYS;
extern PRTGCPHYS  g_pGCPhys;

void foo(void)
{
    MyIprtPrintf("%s %RX64 %RGp %p %RGp", "foobar", g_ull, g_GCPhys, g_pGCPhys, *g_pGCPhys);
    MyIprtPrintf("%RX32 %d %s\n", 10, 42, "string");
}

