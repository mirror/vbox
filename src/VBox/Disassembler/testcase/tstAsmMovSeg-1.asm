    BITS TEST_BITS

    mov fs, eax
    mov fs, ax
%if TEST_BITS == 64
    mov fs, rax
%endif

    mov fs, [ebx]
%if TEST_BITS != 64
    mov fs, [bx]
%else
    mov fs, [rbx]
%endif

    mov ax, fs
    mov eax, fs
%if TEST_BITS == 64
    mov rax, fs
%endif

    mov [ebx], fs
%if TEST_BITS != 64
    mov [bx], fs
%else
    mov [rbx], fs
%endif
