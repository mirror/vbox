
#include "bs3kit.h"
#include <iprt/assert.h>

AssertCompileSize(uint16_t, 2);
AssertCompileSize(uint32_t, 4);
AssertCompileSize(uint64_t, 8);


/* Just a sample. */
void Main_rm(void)
{
    Bs3Shutdown();
    return;
}

