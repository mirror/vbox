
#include "bs3kit.h"
#include <iprt/assert.h>

AssertCompileSize(uint16_t, 2);
AssertCompileSize(uint32_t, 4);
AssertCompileSize(uint64_t, 8);


/* Just a sample. */
BS3_DECL(void) Main_rm(void)
{
    void BS3_FAR *pvTmp1;
    void BS3_FAR *pvTmp2;
    void BS3_FAR *pvTmp3;
    void BS3_FAR *pvTmp4;
    Bs3InitMemory_rm();

    Bs3TestInit("bs3-shutdown");

Bs3PrintStr("Bs3PrintX32:");
Bs3PrintX32(UINT32_C(0xfdb97531));
Bs3PrintStr("\r\n");

pvTmp2 = Bs3MemAlloc(BS3MEMKIND_REAL, _4K);
Bs3PrintStr("pvTmp2=");
Bs3PrintX32((uintptr_t)pvTmp2);
Bs3PrintStr("\r\n");

pvTmp3 = Bs3MemAlloc(BS3MEMKIND_REAL, _4K);
Bs3PrintStr("pvTmp3=");
Bs3PrintX32((uintptr_t)pvTmp3);
Bs3PrintStr("\r\n");
Bs3MemFree(pvTmp2, _4K);

pvTmp4 = Bs3MemAlloc(BS3MEMKIND_REAL, _4K);
Bs3PrintStr("pvTmp4=");
Bs3PrintX32((uintptr_t)pvTmp4);
Bs3PrintStr("\r\n");
Bs3PrintStr("\r\n");

pvTmp1 = Bs3MemAlloc(BS3MEMKIND_REAL, 31);
Bs3PrintStr("pvTmp1=");
Bs3PrintX32((uintptr_t)pvTmp1);
Bs3PrintStr("\r\n");

pvTmp2 = Bs3MemAlloc(BS3MEMKIND_REAL, 17);
Bs3PrintStr("pvTmp2=");
Bs3PrintX32((uintptr_t)pvTmp2);
Bs3PrintStr("\r\n");

Bs3MemFree(pvTmp1, 31);
pvTmp3 = Bs3MemAlloc(BS3MEMKIND_REAL, 17);
Bs3PrintStr("pvTmp3=");
Bs3PrintX32((uintptr_t)pvTmp3);
Bs3PrintStr("\r\n");


Bs3Panic();
    Bs3Shutdown();
    return;
}

