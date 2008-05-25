%include "tstAsm.mac"
    BITS TEST_BITS

    movsx ax, al
    movsx eax, al
    movsx eax, ax

    ;
    ; ParseImmByteSX
    ;

    ; 83 /x
    add eax, strict byte 8
    add eax, strict byte -1
    cmp ebx, strict byte -1

    add ax, strict byte 8
    add ax, strict byte -1
    cmp bx, strict byte -1

%if TEST_BITS == 64
    add rax, strict byte 8
    add rax, strict byte -1
    cmp rbx, strict byte -1
%endif

    ; push %Ib
    push strict byte -1
    push strict byte -128
    push strict byte 127

    ;; @todo imul
