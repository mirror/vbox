%include "tstAsm.mac"
    BITS TEST_BITS

    fstsw ax
    fnstsw ax
    fstsw [xBX]
    fnstsw [xBX]
    fstsw [xBX+xDI]
    fnstsw [xBX+xDI]

