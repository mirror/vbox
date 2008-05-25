    BITS TEST_BITS
%if TEST_BITS != 64
    pop     bp
    pop     ebp
    pop     word [bp]
    pop     dword [bp]
    pop     word [ebp]
    pop     dword [ebp]
%else
 %if 0 ; doesn't work yet
    pop     rbp
    pop     qword [rbp]
 %endif
%endif

