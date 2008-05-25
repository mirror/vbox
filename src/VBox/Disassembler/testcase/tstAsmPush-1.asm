    BITS TEST_BITS
%if TEST_BITS != 64
    push    bp
    push    ebp
    push    word [bp]
    push    dword [bp]
    push    word [ebp]
    push    dword [ebp]
%else
 %if 0 ; doesn't work yet - default operand size is wrong?
    push    rbp
    push    qword [rbp]
 %endif
%endif


