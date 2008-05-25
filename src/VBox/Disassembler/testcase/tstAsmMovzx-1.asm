    BITS TEST_BITS

    movzx ax, al
    movzx eax, al
    movzx ax, [ebx]
    movzx eax, byte [ebx]
    movzx eax, word [ebx]
%if TEST_BITS != 64
    movzx ax, [bx+si+8]
    movzx eax, byte [bx+si+8]
    movzx eax, word [bx+si+8]
%else
    movzx rax, al
    movzx rax, ax
    movzx rax, byte [rsi]
    movzx rax, word [rsi]
%endif

