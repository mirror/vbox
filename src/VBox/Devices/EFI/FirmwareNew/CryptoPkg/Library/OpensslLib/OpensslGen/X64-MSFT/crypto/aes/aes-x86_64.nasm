default rel
%define XMMWORD
%define YMMWORD
%define ZMMWORD
section .text code align=64


ALIGN   16
_x86_64_AES_encrypt:

        xor     eax,DWORD[r15]
        xor     ebx,DWORD[4+r15]
        xor     ecx,DWORD[8+r15]
        xor     edx,DWORD[12+r15]

        mov     r13d,DWORD[240+r15]
        sub     r13d,1
        jmp     NEAR $L$enc_loop
ALIGN   16
$L$enc_loop:

        movzx   esi,al
        movzx   edi,bl
        movzx   ebp,cl
        mov     r10d,DWORD[rsi*8+r14]
        mov     r11d,DWORD[rdi*8+r14]
        mov     r12d,DWORD[rbp*8+r14]

        movzx   esi,bh
        movzx   edi,ch
        movzx   ebp,dl
        xor     r10d,DWORD[3+rsi*8+r14]
        xor     r11d,DWORD[3+rdi*8+r14]
        mov     r8d,DWORD[rbp*8+r14]

        movzx   esi,dh
        shr     ecx,16
        movzx   ebp,ah
        xor     r12d,DWORD[3+rsi*8+r14]
        shr     edx,16
        xor     r8d,DWORD[3+rbp*8+r14]

        shr     ebx,16
        lea     r15,[16+r15]
        shr     eax,16

        movzx   esi,cl
        movzx   edi,dl
        movzx   ebp,al
        xor     r10d,DWORD[2+rsi*8+r14]
        xor     r11d,DWORD[2+rdi*8+r14]
        xor     r12d,DWORD[2+rbp*8+r14]

        movzx   esi,dh
        movzx   edi,ah
        movzx   ebp,bl
        xor     r10d,DWORD[1+rsi*8+r14]
        xor     r11d,DWORD[1+rdi*8+r14]
        xor     r8d,DWORD[2+rbp*8+r14]

        mov     edx,DWORD[12+r15]
        movzx   edi,bh
        movzx   ebp,ch
        mov     eax,DWORD[r15]
        xor     r12d,DWORD[1+rdi*8+r14]
        xor     r8d,DWORD[1+rbp*8+r14]

        mov     ebx,DWORD[4+r15]
        mov     ecx,DWORD[8+r15]
        xor     eax,r10d
        xor     ebx,r11d
        xor     ecx,r12d
        xor     edx,r8d
        sub     r13d,1
        jnz     NEAR $L$enc_loop
        movzx   esi,al
        movzx   edi,bl
        movzx   ebp,cl
        movzx   r10d,BYTE[2+rsi*8+r14]
        movzx   r11d,BYTE[2+rdi*8+r14]
        movzx   r12d,BYTE[2+rbp*8+r14]

        movzx   esi,dl
        movzx   edi,bh
        movzx   ebp,ch
        movzx   r8d,BYTE[2+rsi*8+r14]
        mov     edi,DWORD[rdi*8+r14]
        mov     ebp,DWORD[rbp*8+r14]

        and     edi,0x0000ff00
        and     ebp,0x0000ff00

        xor     r10d,edi
        xor     r11d,ebp
        shr     ecx,16

        movzx   esi,dh
        movzx   edi,ah
        shr     edx,16
        mov     esi,DWORD[rsi*8+r14]
        mov     edi,DWORD[rdi*8+r14]

        and     esi,0x0000ff00
        and     edi,0x0000ff00
        shr     ebx,16
        xor     r12d,esi
        xor     r8d,edi
        shr     eax,16

        movzx   esi,cl
        movzx   edi,dl
        movzx   ebp,al
        mov     esi,DWORD[rsi*8+r14]
        mov     edi,DWORD[rdi*8+r14]
        mov     ebp,DWORD[rbp*8+r14]

        and     esi,0x00ff0000
        and     edi,0x00ff0000
        and     ebp,0x00ff0000

        xor     r10d,esi
        xor     r11d,edi
        xor     r12d,ebp

        movzx   esi,bl
        movzx   edi,dh
        movzx   ebp,ah
        mov     esi,DWORD[rsi*8+r14]
        mov     edi,DWORD[2+rdi*8+r14]
        mov     ebp,DWORD[2+rbp*8+r14]

        and     esi,0x00ff0000
        and     edi,0xff000000
        and     ebp,0xff000000

        xor     r8d,esi
        xor     r10d,edi
        xor     r11d,ebp

        movzx   esi,bh
        movzx   edi,ch
        mov     edx,DWORD[((16+12))+r15]
        mov     esi,DWORD[2+rsi*8+r14]
        mov     edi,DWORD[2+rdi*8+r14]
        mov     eax,DWORD[((16+0))+r15]

        and     esi,0xff000000
        and     edi,0xff000000

        xor     r12d,esi
        xor     r8d,edi

        mov     ebx,DWORD[((16+4))+r15]
        mov     ecx,DWORD[((16+8))+r15]
        xor     eax,r10d
        xor     ebx,r11d
        xor     ecx,r12d
        xor     edx,r8d
DB      0xf3,0xc3



ALIGN   16
_x86_64_AES_encrypt_compact:

        lea     r8,[128+r14]
        mov     edi,DWORD[((0-128))+r8]
        mov     ebp,DWORD[((32-128))+r8]
        mov     r10d,DWORD[((64-128))+r8]
        mov     r11d,DWORD[((96-128))+r8]
        mov     edi,DWORD[((128-128))+r8]
        mov     ebp,DWORD[((160-128))+r8]
        mov     r10d,DWORD[((192-128))+r8]
        mov     r11d,DWORD[((224-128))+r8]
        jmp     NEAR $L$enc_loop_compact
ALIGN   16
$L$enc_loop_compact:
        xor     eax,DWORD[r15]
        xor     ebx,DWORD[4+r15]
        xor     ecx,DWORD[8+r15]
        xor     edx,DWORD[12+r15]
        lea     r15,[16+r15]
        movzx   r10d,al
        movzx   r11d,bl
        movzx   r12d,cl
        movzx   r8d,dl
        movzx   esi,bh
        movzx   edi,ch
        shr     ecx,16
        movzx   ebp,dh
        movzx   r10d,BYTE[r10*1+r14]
        movzx   r11d,BYTE[r11*1+r14]
        movzx   r12d,BYTE[r12*1+r14]
        movzx   r8d,BYTE[r8*1+r14]

        movzx   r9d,BYTE[rsi*1+r14]
        movzx   esi,ah
        movzx   r13d,BYTE[rdi*1+r14]
        movzx   edi,cl
        movzx   ebp,BYTE[rbp*1+r14]
        movzx   esi,BYTE[rsi*1+r14]

        shl     r9d,8
        shr     edx,16
        shl     r13d,8
        xor     r10d,r9d
        shr     eax,16
        movzx   r9d,dl
        shr     ebx,16
        xor     r11d,r13d
        shl     ebp,8
        movzx   r13d,al
        movzx   edi,BYTE[rdi*1+r14]
        xor     r12d,ebp

        shl     esi,8
        movzx   ebp,bl
        shl     edi,16
        xor     r8d,esi
        movzx   r9d,BYTE[r9*1+r14]
        movzx   esi,dh
        movzx   r13d,BYTE[r13*1+r14]
        xor     r10d,edi

        shr     ecx,8
        movzx   edi,ah
        shl     r9d,16
        shr     ebx,8
        shl     r13d,16
        xor     r11d,r9d
        movzx   ebp,BYTE[rbp*1+r14]
        movzx   esi,BYTE[rsi*1+r14]
        movzx   edi,BYTE[rdi*1+r14]
        movzx   edx,BYTE[rcx*1+r14]
        movzx   ecx,BYTE[rbx*1+r14]

        shl     ebp,16
        xor     r12d,r13d
        shl     esi,24
        xor     r8d,ebp
        shl     edi,24
        xor     r10d,esi
        shl     edx,24
        xor     r11d,edi
        shl     ecx,24
        mov     eax,r10d
        mov     ebx,r11d
        xor     ecx,r12d
        xor     edx,r8d
        cmp     r15,QWORD[16+rsp]
        je      NEAR $L$enc_compact_done
        mov     r10d,0x80808080
        mov     r11d,0x80808080
        and     r10d,eax
        and     r11d,ebx
        mov     esi,r10d
        mov     edi,r11d
        shr     r10d,7
        lea     r8d,[rax*1+rax]
        shr     r11d,7
        lea     r9d,[rbx*1+rbx]
        sub     esi,r10d
        sub     edi,r11d
        and     r8d,0xfefefefe
        and     r9d,0xfefefefe
        and     esi,0x1b1b1b1b
        and     edi,0x1b1b1b1b
        mov     r10d,eax
        mov     r11d,ebx
        xor     r8d,esi
        xor     r9d,edi

        xor     eax,r8d
        xor     ebx,r9d
        mov     r12d,0x80808080
        rol     eax,24
        mov     ebp,0x80808080
        rol     ebx,24
        and     r12d,ecx
        and     ebp,edx
        xor     eax,r8d
        xor     ebx,r9d
        mov     esi,r12d
        ror     r10d,16
        mov     edi,ebp
        ror     r11d,16
        lea     r8d,[rcx*1+rcx]
        shr     r12d,7
        xor     eax,r10d
        shr     ebp,7
        xor     ebx,r11d
        ror     r10d,8
        lea     r9d,[rdx*1+rdx]
        ror     r11d,8
        sub     esi,r12d
        sub     edi,ebp
        xor     eax,r10d
        xor     ebx,r11d

        and     r8d,0xfefefefe
        and     r9d,0xfefefefe
        and     esi,0x1b1b1b1b
        and     edi,0x1b1b1b1b
        mov     r12d,ecx
        mov     ebp,edx
        xor     r8d,esi
        xor     r9d,edi

        ror     r12d,16
        xor     ecx,r8d
        ror     ebp,16
        xor     edx,r9d
        rol     ecx,24
        mov     esi,DWORD[r14]
        rol     edx,24
        xor     ecx,r8d
        mov     edi,DWORD[64+r14]
        xor     edx,r9d
        mov     r8d,DWORD[128+r14]
        xor     ecx,r12d
        ror     r12d,8
        xor     edx,ebp
        ror     ebp,8
        xor     ecx,r12d
        mov     r9d,DWORD[192+r14]
        xor     edx,ebp
        jmp     NEAR $L$enc_loop_compact
ALIGN   16
$L$enc_compact_done:
        xor     eax,DWORD[r15]
        xor     ebx,DWORD[4+r15]
        xor     ecx,DWORD[8+r15]
        xor     edx,DWORD[12+r15]
DB      0xf3,0xc3


global  AES_encrypt

ALIGN   16
global  asm_AES_encrypt

asm_AES_encrypt:
AES_encrypt:
        mov     QWORD[8+rsp],rdi        ;WIN64 prologue
        mov     QWORD[16+rsp],rsi
        mov     rax,rsp
$L$SEH_begin_AES_encrypt:
        mov     rdi,rcx
        mov     rsi,rdx
        mov     rdx,r8



DB      243,15,30,250
        mov     rax,rsp

        push    rbx

        push    rbp

        push    r12

        push    r13

        push    r14

        push    r15



        lea     rcx,[((-63))+rdx]
        and     rsp,-64
        sub     rcx,rsp
        neg     rcx
        and     rcx,0x3c0
        sub     rsp,rcx
        sub     rsp,32

        mov     QWORD[16+rsp],rsi
        mov     QWORD[24+rsp],rax

$L$enc_prologue:

        mov     r15,rdx
        mov     r13d,DWORD[240+r15]

        mov     eax,DWORD[rdi]
        mov     ebx,DWORD[4+rdi]
        mov     ecx,DWORD[8+rdi]
        mov     edx,DWORD[12+rdi]

        shl     r13d,4
        lea     rbp,[r13*1+r15]
        mov     QWORD[rsp],r15
        mov     QWORD[8+rsp],rbp


        lea     r14,[(($L$AES_Te+2048))]
        lea     rbp,[768+rsp]
        sub     rbp,r14
        and     rbp,0x300
        lea     r14,[rbp*1+r14]

        call    _x86_64_AES_encrypt_compact

        mov     r9,QWORD[16+rsp]
        mov     rsi,QWORD[24+rsp]

        mov     DWORD[r9],eax
        mov     DWORD[4+r9],ebx
        mov     DWORD[8+r9],ecx
        mov     DWORD[12+r9],edx

        mov     r15,QWORD[((-48))+rsi]

        mov     r14,QWORD[((-40))+rsi]

        mov     r13,QWORD[((-32))+rsi]

        mov     r12,QWORD[((-24))+rsi]

        mov     rbp,QWORD[((-16))+rsi]

        mov     rbx,QWORD[((-8))+rsi]

        lea     rsp,[rsi]

$L$enc_epilogue:
        mov     rdi,QWORD[8+rsp]        ;WIN64 epilogue
        mov     rsi,QWORD[16+rsp]
        DB      0F3h,0C3h               ;repret

$L$SEH_end_AES_encrypt:

ALIGN   16
_x86_64_AES_decrypt:

        xor     eax,DWORD[r15]
        xor     ebx,DWORD[4+r15]
        xor     ecx,DWORD[8+r15]
        xor     edx,DWORD[12+r15]

        mov     r13d,DWORD[240+r15]
        sub     r13d,1
        jmp     NEAR $L$dec_loop
ALIGN   16
$L$dec_loop:

        movzx   esi,al
        movzx   edi,bl
        movzx   ebp,cl
        mov     r10d,DWORD[rsi*8+r14]
        mov     r11d,DWORD[rdi*8+r14]
        mov     r12d,DWORD[rbp*8+r14]

        movzx   esi,dh
        movzx   edi,ah
        movzx   ebp,dl
        xor     r10d,DWORD[3+rsi*8+r14]
        xor     r11d,DWORD[3+rdi*8+r14]
        mov     r8d,DWORD[rbp*8+r14]

        movzx   esi,bh
        shr     eax,16
        movzx   ebp,ch
        xor     r12d,DWORD[3+rsi*8+r14]
        shr     edx,16
        xor     r8d,DWORD[3+rbp*8+r14]

        shr     ebx,16
        lea     r15,[16+r15]
        shr     ecx,16

        movzx   esi,cl
        movzx   edi,dl
        movzx   ebp,al
        xor     r10d,DWORD[2+rsi*8+r14]
        xor     r11d,DWORD[2+rdi*8+r14]
        xor     r12d,DWORD[2+rbp*8+r14]

        movzx   esi,bh
        movzx   edi,ch
        movzx   ebp,bl
        xor     r10d,DWORD[1+rsi*8+r14]
        xor     r11d,DWORD[1+rdi*8+r14]
        xor     r8d,DWORD[2+rbp*8+r14]

        movzx   esi,dh
        mov     edx,DWORD[12+r15]
        movzx   ebp,ah
        xor     r12d,DWORD[1+rsi*8+r14]
        mov     eax,DWORD[r15]
        xor     r8d,DWORD[1+rbp*8+r14]

        xor     eax,r10d
        mov     ebx,DWORD[4+r15]
        mov     ecx,DWORD[8+r15]
        xor     ecx,r12d
        xor     ebx,r11d
        xor     edx,r8d
        sub     r13d,1
        jnz     NEAR $L$dec_loop
        lea     r14,[2048+r14]
        movzx   esi,al
        movzx   edi,bl
        movzx   ebp,cl
        movzx   r10d,BYTE[rsi*1+r14]
        movzx   r11d,BYTE[rdi*1+r14]
        movzx   r12d,BYTE[rbp*1+r14]

        movzx   esi,dl
        movzx   edi,dh
        movzx   ebp,ah
        movzx   r8d,BYTE[rsi*1+r14]
        movzx   edi,BYTE[rdi*1+r14]
        movzx   ebp,BYTE[rbp*1+r14]

        shl     edi,8
        shl     ebp,8

        xor     r10d,edi
        xor     r11d,ebp
        shr     edx,16

        movzx   esi,bh
        movzx   edi,ch
        shr     eax,16
        movzx   esi,BYTE[rsi*1+r14]
        movzx   edi,BYTE[rdi*1+r14]

        shl     esi,8
        shl     edi,8
        shr     ebx,16
        xor     r12d,esi
        xor     r8d,edi
        shr     ecx,16

        movzx   esi,cl
        movzx   edi,dl
        movzx   ebp,al
        movzx   esi,BYTE[rsi*1+r14]
        movzx   edi,BYTE[rdi*1+r14]
        movzx   ebp,BYTE[rbp*1+r14]

        shl     esi,16
        shl     edi,16
        shl     ebp,16

        xor     r10d,esi
        xor     r11d,edi
        xor     r12d,ebp

        movzx   esi,bl
        movzx   edi,bh
        movzx   ebp,ch
        movzx   esi,BYTE[rsi*1+r14]
        movzx   edi,BYTE[rdi*1+r14]
        movzx   ebp,BYTE[rbp*1+r14]

        shl     esi,16
        shl     edi,24
        shl     ebp,24

        xor     r8d,esi
        xor     r10d,edi
        xor     r11d,ebp

        movzx   esi,dh
        movzx   edi,ah
        mov     edx,DWORD[((16+12))+r15]
        movzx   esi,BYTE[rsi*1+r14]
        movzx   edi,BYTE[rdi*1+r14]
        mov     eax,DWORD[((16+0))+r15]

        shl     esi,24
        shl     edi,24

        xor     r12d,esi
        xor     r8d,edi

        mov     ebx,DWORD[((16+4))+r15]
        mov     ecx,DWORD[((16+8))+r15]
        lea     r14,[((-2048))+r14]
        xor     eax,r10d
        xor     ebx,r11d
        xor     ecx,r12d
        xor     edx,r8d
DB      0xf3,0xc3



ALIGN   16
_x86_64_AES_decrypt_compact:

        lea     r8,[128+r14]
        mov     edi,DWORD[((0-128))+r8]
        mov     ebp,DWORD[((32-128))+r8]
        mov     r10d,DWORD[((64-128))+r8]
        mov     r11d,DWORD[((96-128))+r8]
        mov     edi,DWORD[((128-128))+r8]
        mov     ebp,DWORD[((160-128))+r8]
        mov     r10d,DWORD[((192-128))+r8]
        mov     r11d,DWORD[((224-128))+r8]
        jmp     NEAR $L$dec_loop_compact

ALIGN   16
$L$dec_loop_compact:
        xor     eax,DWORD[r15]
        xor     ebx,DWORD[4+r15]
        xor     ecx,DWORD[8+r15]
        xor     edx,DWORD[12+r15]
        lea     r15,[16+r15]
        movzx   r10d,al
        movzx   r11d,bl
        movzx   r12d,cl
        movzx   r8d,dl
        movzx   esi,dh
        movzx   edi,ah
        shr     edx,16
        movzx   ebp,bh
        movzx   r10d,BYTE[r10*1+r14]
        movzx   r11d,BYTE[r11*1+r14]
        movzx   r12d,BYTE[r12*1+r14]
        movzx   r8d,BYTE[r8*1+r14]

        movzx   r9d,BYTE[rsi*1+r14]
        movzx   esi,ch
        movzx   r13d,BYTE[rdi*1+r14]
        movzx   ebp,BYTE[rbp*1+r14]
        movzx   esi,BYTE[rsi*1+r14]

        shr     ecx,16
        shl     r13d,8
        shl     r9d,8
        movzx   edi,cl
        shr     eax,16
        xor     r10d,r9d
        shr     ebx,16
        movzx   r9d,dl

        shl     ebp,8
        xor     r11d,r13d
        shl     esi,8
        movzx   r13d,al
        movzx   edi,BYTE[rdi*1+r14]
        xor     r12d,ebp
        movzx   ebp,bl

        shl     edi,16
        xor     r8d,esi
        movzx   r9d,BYTE[r9*1+r14]
        movzx   esi,bh
        movzx   ebp,BYTE[rbp*1+r14]
        xor     r10d,edi
        movzx   r13d,BYTE[r13*1+r14]
        movzx   edi,ch

        shl     ebp,16
        shl     r9d,16
        shl     r13d,16
        xor     r8d,ebp
        movzx   ebp,dh
        xor     r11d,r9d
        shr     eax,8
        xor     r12d,r13d

        movzx   esi,BYTE[rsi*1+r14]
        movzx   ebx,BYTE[rdi*1+r14]
        movzx   ecx,BYTE[rbp*1+r14]
        movzx   edx,BYTE[rax*1+r14]

        mov     eax,r10d
        shl     esi,24
        shl     ebx,24
        shl     ecx,24
        xor     eax,esi
        shl     edx,24
        xor     ebx,r11d
        xor     ecx,r12d
        xor     edx,r8d
        cmp     r15,QWORD[16+rsp]
        je      NEAR $L$dec_compact_done

        mov     rsi,QWORD[((256+0))+r14]
        shl     rbx,32
        shl     rdx,32
        mov     rdi,QWORD[((256+8))+r14]
        or      rax,rbx
        or      rcx,rdx
        mov     rbp,QWORD[((256+16))+r14]
        mov     r9,rsi
        mov     r12,rsi
        and     r9,rax
        and     r12,rcx
        mov     rbx,r9
        mov     rdx,r12
        shr     r9,7
        lea     r8,[rax*1+rax]
        shr     r12,7
        lea     r11,[rcx*1+rcx]
        sub     rbx,r9
        sub     rdx,r12
        and     r8,rdi
        and     r11,rdi
        and     rbx,rbp
        and     rdx,rbp
        xor     r8,rbx
        xor     r11,rdx
        mov     r10,rsi
        mov     r13,rsi

        and     r10,r8
        and     r13,r11
        mov     rbx,r10
        mov     rdx,r13
        shr     r10,7
        lea     r9,[r8*1+r8]
        shr     r13,7
        lea     r12,[r11*1+r11]
        sub     rbx,r10
        sub     rdx,r13
        and     r9,rdi
        and     r12,rdi
        and     rbx,rbp
        and     rdx,rbp
        xor     r9,rbx
        xor     r12,rdx
        mov     r10,rsi
        mov     r13,rsi

        and     r10,r9
        and     r13,r12
        mov     rbx,r10
        mov     rdx,r13
        shr     r10,7
        xor     r8,rax
        shr     r13,7
        xor     r11,rcx
        sub     rbx,r10
        sub     rdx,r13
        lea     r10,[r9*1+r9]
        lea     r13,[r12*1+r12]
        xor     r9,rax
        xor     r12,rcx
        and     r10,rdi
        and     r13,rdi
        and     rbx,rbp
        and     rdx,rbp
        xor     r10,rbx
        xor     r13,rdx

        xor     rax,r10
        xor     rcx,r13
        xor     r8,r10
        xor     r11,r13
        mov     rbx,rax
        mov     rdx,rcx
        xor     r9,r10
        shr     rbx,32
        xor     r12,r13
        shr     rdx,32
        xor     r10,r8
        rol     eax,8
        xor     r13,r11
        rol     ecx,8
        xor     r10,r9
        rol     ebx,8
        xor     r13,r12

        rol     edx,8
        xor     eax,r10d
        shr     r10,32
        xor     ecx,r13d
        shr     r13,32
        xor     ebx,r10d
        xor     edx,r13d

        mov     r10,r8
        rol     r8d,24
        mov     r13,r11
        rol     r11d,24
        shr     r10,32
        xor     eax,r8d
        shr     r13,32
        xor     ecx,r11d
        rol     r10d,24
        mov     r8,r9
        rol     r13d,24
        mov     r11,r12
        shr     r8,32
        xor     ebx,r10d
        shr     r11,32
        xor     edx,r13d

        mov     rsi,QWORD[r14]
        rol     r9d,16
        mov     rdi,QWORD[64+r14]
        rol     r12d,16
        mov     rbp,QWORD[128+r14]
        rol     r8d,16
        mov     r10,QWORD[192+r14]
        xor     eax,r9d
        rol     r11d,16
        xor     ecx,r12d
        mov     r13,QWORD[256+r14]
        xor     ebx,r8d
        xor     edx,r11d
        jmp     NEAR $L$dec_loop_compact
ALIGN   16
$L$dec_compact_done:
        xor     eax,DWORD[r15]
        xor     ebx,DWORD[4+r15]
        xor     ecx,DWORD[8+r15]
        xor     edx,DWORD[12+r15]
DB      0xf3,0xc3


global  AES_decrypt

ALIGN   16
global  asm_AES_decrypt

asm_AES_decrypt:
AES_decrypt:
        mov     QWORD[8+rsp],rdi        ;WIN64 prologue
        mov     QWORD[16+rsp],rsi
        mov     rax,rsp
$L$SEH_begin_AES_decrypt:
        mov     rdi,rcx
        mov     rsi,rdx
        mov     rdx,r8



DB      243,15,30,250
        mov     rax,rsp

        push    rbx

        push    rbp

        push    r12

        push    r13

        push    r14

        push    r15



        lea     rcx,[((-63))+rdx]
        and     rsp,-64
        sub     rcx,rsp
        neg     rcx
        and     rcx,0x3c0
        sub     rsp,rcx
        sub     rsp,32

        mov     QWORD[16+rsp],rsi
        mov     QWORD[24+rsp],rax

$L$dec_prologue:

        mov     r15,rdx
        mov     r13d,DWORD[240+r15]

        mov     eax,DWORD[rdi]
        mov     ebx,DWORD[4+rdi]
        mov     ecx,DWORD[8+rdi]
        mov     edx,DWORD[12+rdi]

        shl     r13d,4
        lea     rbp,[r13*1+r15]
        mov     QWORD[rsp],r15
        mov     QWORD[8+rsp],rbp


        lea     r14,[(($L$AES_Td+2048))]
        lea     rbp,[768+rsp]
        sub     rbp,r14
        and     rbp,0x300
        lea     r14,[rbp*1+r14]
        shr     rbp,3
        add     r14,rbp

        call    _x86_64_AES_decrypt_compact

        mov     r9,QWORD[16+rsp]
        mov     rsi,QWORD[24+rsp]

        mov     DWORD[r9],eax
        mov     DWORD[4+r9],ebx
        mov     DWORD[8+r9],ecx
        mov     DWORD[12+r9],edx

        mov     r15,QWORD[((-48))+rsi]

        mov     r14,QWORD[((-40))+rsi]

        mov     r13,QWORD[((-32))+rsi]

        mov     r12,QWORD[((-24))+rsi]

        mov     rbp,QWORD[((-16))+rsi]

        mov     rbx,QWORD[((-8))+rsi]

        lea     rsp,[rsi]

$L$dec_epilogue:
        mov     rdi,QWORD[8+rsp]        ;WIN64 epilogue
        mov     rsi,QWORD[16+rsp]
        DB      0F3h,0C3h               ;repret

$L$SEH_end_AES_decrypt:
global  AES_set_encrypt_key

ALIGN   16
AES_set_encrypt_key:
        mov     QWORD[8+rsp],rdi        ;WIN64 prologue
        mov     QWORD[16+rsp],rsi
        mov     rax,rsp
$L$SEH_begin_AES_set_encrypt_key:
        mov     rdi,rcx
        mov     rsi,rdx
        mov     rdx,r8



DB      243,15,30,250
        push    rbx

        push    rbp

        push    r12

        push    r13

        push    r14

        push    r15

        sub     rsp,8

$L$enc_key_prologue:

        call    _x86_64_AES_set_encrypt_key

        mov     rbp,QWORD[40+rsp]

        mov     rbx,QWORD[48+rsp]

        add     rsp,56

$L$enc_key_epilogue:
        mov     rdi,QWORD[8+rsp]        ;WIN64 epilogue
        mov     rsi,QWORD[16+rsp]
        DB      0F3h,0C3h               ;repret

$L$SEH_end_AES_set_encrypt_key:


ALIGN   16
_x86_64_AES_set_encrypt_key:

        mov     ecx,esi
        mov     rsi,rdi
        mov     rdi,rdx

        test    rsi,-1
        jz      NEAR $L$badpointer
        test    rdi,-1
        jz      NEAR $L$badpointer

        lea     rbp,[$L$AES_Te]
        lea     rbp,[((2048+128))+rbp]


        mov     eax,DWORD[((0-128))+rbp]
        mov     ebx,DWORD[((32-128))+rbp]
        mov     r8d,DWORD[((64-128))+rbp]
        mov     edx,DWORD[((96-128))+rbp]
        mov     eax,DWORD[((128-128))+rbp]
        mov     ebx,DWORD[((160-128))+rbp]
        mov     r8d,DWORD[((192-128))+rbp]
        mov     edx,DWORD[((224-128))+rbp]

        cmp     ecx,128
        je      NEAR $L$10rounds
        cmp     ecx,192
        je      NEAR $L$12rounds
        cmp     ecx,256
        je      NEAR $L$14rounds
        mov     rax,-2
        jmp     NEAR $L$exit

$L$10rounds:
        mov     rax,QWORD[rsi]
        mov     rdx,QWORD[8+rsi]
        mov     QWORD[rdi],rax
        mov     QWORD[8+rdi],rdx

        shr     rdx,32
        xor     ecx,ecx
        jmp     NEAR $L$10shortcut
ALIGN   4
$L$10loop:
        mov     eax,DWORD[rdi]
        mov     edx,DWORD[12+rdi]
$L$10shortcut:
        movzx   esi,dl
        movzx   ebx,BYTE[((-128))+rsi*1+rbp]
        movzx   esi,dh
        shl     ebx,24
        xor     eax,ebx

        movzx   ebx,BYTE[((-128))+rsi*1+rbp]
        shr     edx,16
        movzx   esi,dl
        xor     eax,ebx

        movzx   ebx,BYTE[((-128))+rsi*1+rbp]
        movzx   esi,dh
        shl     ebx,8
        xor     eax,ebx

        movzx   ebx,BYTE[((-128))+rsi*1+rbp]
        shl     ebx,16
        xor     eax,ebx

        xor     eax,DWORD[((1024-128))+rcx*4+rbp]
        mov     DWORD[16+rdi],eax
        xor     eax,DWORD[4+rdi]
        mov     DWORD[20+rdi],eax
        xor     eax,DWORD[8+rdi]
        mov     DWORD[24+rdi],eax
        xor     eax,DWORD[12+rdi]
        mov     DWORD[28+rdi],eax
        add     ecx,1
        lea     rdi,[16+rdi]
        cmp     ecx,10
        jl      NEAR $L$10loop

        mov     DWORD[80+rdi],10
        xor     rax,rax
        jmp     NEAR $L$exit

$L$12rounds:
        mov     rax,QWORD[rsi]
        mov     rbx,QWORD[8+rsi]
        mov     rdx,QWORD[16+rsi]
        mov     QWORD[rdi],rax
        mov     QWORD[8+rdi],rbx
        mov     QWORD[16+rdi],rdx

        shr     rdx,32
        xor     ecx,ecx
        jmp     NEAR $L$12shortcut
ALIGN   4
$L$12loop:
        mov     eax,DWORD[rdi]
        mov     edx,DWORD[20+rdi]
$L$12shortcut:
        movzx   esi,dl
        movzx   ebx,BYTE[((-128))+rsi*1+rbp]
        movzx   esi,dh
        shl     ebx,24
        xor     eax,ebx

        movzx   ebx,BYTE[((-128))+rsi*1+rbp]
        shr     edx,16
        movzx   esi,dl
        xor     eax,ebx

        movzx   ebx,BYTE[((-128))+rsi*1+rbp]
        movzx   esi,dh
        shl     ebx,8
        xor     eax,ebx

        movzx   ebx,BYTE[((-128))+rsi*1+rbp]
        shl     ebx,16
        xor     eax,ebx

        xor     eax,DWORD[((1024-128))+rcx*4+rbp]
        mov     DWORD[24+rdi],eax
        xor     eax,DWORD[4+rdi]
        mov     DWORD[28+rdi],eax
        xor     eax,DWORD[8+rdi]
        mov     DWORD[32+rdi],eax
        xor     eax,DWORD[12+rdi]
        mov     DWORD[36+rdi],eax

        cmp     ecx,7
        je      NEAR $L$12break
        add     ecx,1

        xor     eax,DWORD[16+rdi]
        mov     DWORD[40+rdi],eax
        xor     eax,DWORD[20+rdi]
        mov     DWORD[44+rdi],eax

        lea     rdi,[24+rdi]
        jmp     NEAR $L$12loop
$L$12break:
        mov     DWORD[72+rdi],12
        xor     rax,rax
        jmp     NEAR $L$exit

$L$14rounds:
        mov     rax,QWORD[rsi]
        mov     rbx,QWORD[8+rsi]
        mov     rcx,QWORD[16+rsi]
        mov     rdx,QWORD[24+rsi]
        mov     QWORD[rdi],rax
        mov     QWORD[8+rdi],rbx
        mov     QWORD[16+rdi],rcx
        mov     QWORD[24+rdi],rdx

        shr     rdx,32
        xor     ecx,ecx
        jmp     NEAR $L$14shortcut
ALIGN   4
$L$14loop:
        mov     eax,DWORD[rdi]
        mov     edx,DWORD[28+rdi]
$L$14shortcut:
        movzx   esi,dl
        movzx   ebx,BYTE[((-128))+rsi*1+rbp]
        movzx   esi,dh
        shl     ebx,24
        xor     eax,ebx

        movzx   ebx,BYTE[((-128))+rsi*1+rbp]
        shr     edx,16
        movzx   esi,dl
        xor     eax,ebx

        movzx   ebx,BYTE[((-128))+rsi*1+rbp]
        movzx   esi,dh
        shl     ebx,8
        xor     eax,ebx

        movzx   ebx,BYTE[((-128))+rsi*1+rbp]
        shl     ebx,16
        xor     eax,ebx

        xor     eax,DWORD[((1024-128))+rcx*4+rbp]
        mov     DWORD[32+rdi],eax
        xor     eax,DWORD[4+rdi]
        mov     DWORD[36+rdi],eax
        xor     eax,DWORD[8+rdi]
        mov     DWORD[40+rdi],eax
        xor     eax,DWORD[12+rdi]
        mov     DWORD[44+rdi],eax

        cmp     ecx,6
        je      NEAR $L$14break
        add     ecx,1

        mov     edx,eax
        mov     eax,DWORD[16+rdi]
        movzx   esi,dl
        movzx   ebx,BYTE[((-128))+rsi*1+rbp]
        movzx   esi,dh
        xor     eax,ebx

        movzx   ebx,BYTE[((-128))+rsi*1+rbp]
        shr     edx,16
        shl     ebx,8
        movzx   esi,dl
        xor     eax,ebx

        movzx   ebx,BYTE[((-128))+rsi*1+rbp]
        movzx   esi,dh
        shl     ebx,16
        xor     eax,ebx

        movzx   ebx,BYTE[((-128))+rsi*1+rbp]
        shl     ebx,24
        xor     eax,ebx

        mov     DWORD[48+rdi],eax
        xor     eax,DWORD[20+rdi]
        mov     DWORD[52+rdi],eax
        xor     eax,DWORD[24+rdi]
        mov     DWORD[56+rdi],eax
        xor     eax,DWORD[28+rdi]
        mov     DWORD[60+rdi],eax

        lea     rdi,[32+rdi]
        jmp     NEAR $L$14loop
$L$14break:
        mov     DWORD[48+rdi],14
        xor     rax,rax
        jmp     NEAR $L$exit

$L$badpointer:
        mov     rax,-1
$L$exit:
DB      0xf3,0xc3


global  AES_set_decrypt_key

ALIGN   16
AES_set_decrypt_key:
        mov     QWORD[8+rsp],rdi        ;WIN64 prologue
        mov     QWORD[16+rsp],rsi
        mov     rax,rsp
$L$SEH_begin_AES_set_decrypt_key:
        mov     rdi,rcx
        mov     rsi,rdx
        mov     rdx,r8



DB      243,15,30,250
        push    rbx

        push    rbp

        push    r12

        push    r13

        push    r14

        push    r15

        push    rdx

$L$dec_key_prologue:

        call    _x86_64_AES_set_encrypt_key
        mov     r8,QWORD[rsp]
        cmp     eax,0
        jne     NEAR $L$abort

        mov     r14d,DWORD[240+r8]
        xor     rdi,rdi
        lea     rcx,[r14*4+rdi]
        mov     rsi,r8
        lea     rdi,[rcx*4+r8]
ALIGN   4
$L$invert:
        mov     rax,QWORD[rsi]
        mov     rbx,QWORD[8+rsi]
        mov     rcx,QWORD[rdi]
        mov     rdx,QWORD[8+rdi]
        mov     QWORD[rdi],rax
        mov     QWORD[8+rdi],rbx
        mov     QWORD[rsi],rcx
        mov     QWORD[8+rsi],rdx
        lea     rsi,[16+rsi]
        lea     rdi,[((-16))+rdi]
        cmp     rdi,rsi
        jne     NEAR $L$invert

        lea     rax,[(($L$AES_Te+2048+1024))]

        mov     rsi,QWORD[40+rax]
        mov     rdi,QWORD[48+rax]
        mov     rbp,QWORD[56+rax]

        mov     r15,r8
        sub     r14d,1
ALIGN   4
$L$permute:
        lea     r15,[16+r15]
        mov     rax,QWORD[r15]
        mov     rcx,QWORD[8+r15]
        mov     r9,rsi
        mov     r12,rsi
        and     r9,rax
        and     r12,rcx
        mov     rbx,r9
        mov     rdx,r12
        shr     r9,7
        lea     r8,[rax*1+rax]
        shr     r12,7
        lea     r11,[rcx*1+rcx]
        sub     rbx,r9
        sub     rdx,r12
        and     r8,rdi
        and     r11,rdi
        and     rbx,rbp
        and     rdx,rbp
        xor     r8,rbx
        xor     r11,rdx
        mov     r10,rsi
        mov     r13,rsi

        and     r10,r8
        and     r13,r11
        mov     rbx,r10
        mov     rdx,r13
        shr     r10,7
        lea     r9,[r8*1+r8]
        shr     r13,7
        lea     r12,[r11*1+r11]
        sub     rbx,r10
        sub     rdx,r13
        and     r9,rdi
        and     r12,rdi
        and     rbx,rbp
        and     rdx,rbp
        xor     r9,rbx
        xor     r12,rdx
        mov     r10,rsi
        mov     r13,rsi

        and     r10,r9
        and     r13,r12
        mov     rbx,r10
        mov     rdx,r13
        shr     r10,7
        xor     r8,rax
        shr     r13,7
        xor     r11,rcx
        sub     rbx,r10
        sub     rdx,r13
        lea     r10,[r9*1+r9]
        lea     r13,[r12*1+r12]
        xor     r9,rax
        xor     r12,rcx
        and     r10,rdi
        and     r13,rdi
        and     rbx,rbp
        and     rdx,rbp
        xor     r10,rbx
        xor     r13,rdx

        xor     rax,r10
        xor     rcx,r13
        xor     r8,r10
        xor     r11,r13
        mov     rbx,rax
        mov     rdx,rcx
        xor     r9,r10
        shr     rbx,32
        xor     r12,r13
        shr     rdx,32
        xor     r10,r8
        rol     eax,8
        xor     r13,r11
        rol     ecx,8
        xor     r10,r9
        rol     ebx,8
        xor     r13,r12

        rol     edx,8
        xor     eax,r10d
        shr     r10,32
        xor     ecx,r13d
        shr     r13,32
        xor     ebx,r10d
        xor     edx,r13d

        mov     r10,r8
        rol     r8d,24
        mov     r13,r11
        rol     r11d,24
        shr     r10,32
        xor     eax,r8d
        shr     r13,32
        xor     ecx,r11d
        rol     r10d,24
        mov     r8,r9
        rol     r13d,24
        mov     r11,r12
        shr     r8,32
        xor     ebx,r10d
        shr     r11,32
        xor     edx,r13d


        rol     r9d,16

        rol     r12d,16

        rol     r8d,16

        xor     eax,r9d
        rol     r11d,16
        xor     ecx,r12d

        xor     ebx,r8d
        xor     edx,r11d
        mov     DWORD[r15],eax
        mov     DWORD[4+r15],ebx
        mov     DWORD[8+r15],ecx
        mov     DWORD[12+r15],edx
        sub     r14d,1
        jnz     NEAR $L$permute

        xor     rax,rax
$L$abort:
        mov     r15,QWORD[8+rsp]

        mov     r14,QWORD[16+rsp]

        mov     r13,QWORD[24+rsp]

        mov     r12,QWORD[32+rsp]

        mov     rbp,QWORD[40+rsp]

        mov     rbx,QWORD[48+rsp]

        add     rsp,56

$L$dec_key_epilogue:
        mov     rdi,QWORD[8+rsp]        ;WIN64 epilogue
        mov     rsi,QWORD[16+rsp]
        DB      0F3h,0C3h               ;repret

$L$SEH_end_AES_set_decrypt_key:
global  AES_cbc_encrypt

ALIGN   16
EXTERN  OPENSSL_ia32cap_P
global  asm_AES_cbc_encrypt

asm_AES_cbc_encrypt:
AES_cbc_encrypt:
        mov     QWORD[8+rsp],rdi        ;WIN64 prologue
        mov     QWORD[16+rsp],rsi
        mov     rax,rsp
$L$SEH_begin_AES_cbc_encrypt:
        mov     rdi,rcx
        mov     rsi,rdx
        mov     rdx,r8
        mov     rcx,r9
        mov     r8,QWORD[40+rsp]
        mov     r9,QWORD[48+rsp]



DB      243,15,30,250
        cmp     rdx,0
        je      NEAR $L$cbc_epilogue
        pushfq



        push    rbx

        push    rbp

        push    r12

        push    r13

        push    r14

        push    r15

$L$cbc_prologue:

        cld
        mov     r9d,r9d

        lea     r14,[$L$AES_Te]
        lea     r10,[$L$AES_Td]
        cmp     r9,0
        cmove   r14,r10


        mov     r10d,DWORD[OPENSSL_ia32cap_P]
        cmp     rdx,512
        jb      NEAR $L$cbc_slow_prologue
        test    rdx,15
        jnz     NEAR $L$cbc_slow_prologue
        bt      r10d,28
        jc      NEAR $L$cbc_slow_prologue


        lea     r15,[((-88-248))+rsp]
        and     r15,-64


        mov     r10,r14
        lea     r11,[2304+r14]
        mov     r12,r15
        and     r10,0xFFF
        and     r11,0xFFF
        and     r12,0xFFF

        cmp     r12,r11
        jb      NEAR $L$cbc_te_break_out
        sub     r12,r11
        sub     r15,r12
        jmp     NEAR $L$cbc_te_ok
$L$cbc_te_break_out:
        sub     r12,r10
        and     r12,0xFFF
        add     r12,320
        sub     r15,r12
ALIGN   4
$L$cbc_te_ok:

        xchg    r15,rsp


        mov     QWORD[16+rsp],r15

$L$cbc_fast_body:
        mov     QWORD[24+rsp],rdi
        mov     QWORD[32+rsp],rsi
        mov     QWORD[40+rsp],rdx
        mov     QWORD[48+rsp],rcx
        mov     QWORD[56+rsp],r8
        mov     DWORD[((80+240))+rsp],0
        mov     rbp,r8
        mov     rbx,r9
        mov     r9,rsi
        mov     r8,rdi
        mov     r15,rcx

        mov     eax,DWORD[240+r15]

        mov     r10,r15
        sub     r10,r14
        and     r10,0xfff
        cmp     r10,2304
        jb      NEAR $L$cbc_do_ecopy
        cmp     r10,4096-248
        jb      NEAR $L$cbc_skip_ecopy
ALIGN   4
$L$cbc_do_ecopy:
        mov     rsi,r15
        lea     rdi,[80+rsp]
        lea     r15,[80+rsp]
        mov     ecx,240/8
        DD      0x90A548F3
        mov     DWORD[rdi],eax
$L$cbc_skip_ecopy:
        mov     QWORD[rsp],r15

        mov     ecx,18
ALIGN   4
$L$cbc_prefetch_te:
        mov     r10,QWORD[r14]
        mov     r11,QWORD[32+r14]
        mov     r12,QWORD[64+r14]
        mov     r13,QWORD[96+r14]
        lea     r14,[128+r14]
        sub     ecx,1
        jnz     NEAR $L$cbc_prefetch_te
        lea     r14,[((-2304))+r14]

        cmp     rbx,0
        je      NEAR $L$FAST_DECRYPT


        mov     eax,DWORD[rbp]
        mov     ebx,DWORD[4+rbp]
        mov     ecx,DWORD[8+rbp]
        mov     edx,DWORD[12+rbp]

ALIGN   4
$L$cbc_fast_enc_loop:
        xor     eax,DWORD[r8]
        xor     ebx,DWORD[4+r8]
        xor     ecx,DWORD[8+r8]
        xor     edx,DWORD[12+r8]
        mov     r15,QWORD[rsp]
        mov     QWORD[24+rsp],r8

        call    _x86_64_AES_encrypt

        mov     r8,QWORD[24+rsp]
        mov     r10,QWORD[40+rsp]
        mov     DWORD[r9],eax
        mov     DWORD[4+r9],ebx
        mov     DWORD[8+r9],ecx
        mov     DWORD[12+r9],edx

        lea     r8,[16+r8]
        lea     r9,[16+r9]
        sub     r10,16
        test    r10,-16
        mov     QWORD[40+rsp],r10
        jnz     NEAR $L$cbc_fast_enc_loop
        mov     rbp,QWORD[56+rsp]
        mov     DWORD[rbp],eax
        mov     DWORD[4+rbp],ebx
        mov     DWORD[8+rbp],ecx
        mov     DWORD[12+rbp],edx

        jmp     NEAR $L$cbc_fast_cleanup


ALIGN   16
$L$FAST_DECRYPT:
        cmp     r9,r8
        je      NEAR $L$cbc_fast_dec_in_place

        mov     QWORD[64+rsp],rbp
ALIGN   4
$L$cbc_fast_dec_loop:
        mov     eax,DWORD[r8]
        mov     ebx,DWORD[4+r8]
        mov     ecx,DWORD[8+r8]
        mov     edx,DWORD[12+r8]
        mov     r15,QWORD[rsp]
        mov     QWORD[24+rsp],r8

        call    _x86_64_AES_decrypt

        mov     rbp,QWORD[64+rsp]
        mov     r8,QWORD[24+rsp]
        mov     r10,QWORD[40+rsp]
        xor     eax,DWORD[rbp]
        xor     ebx,DWORD[4+rbp]
        xor     ecx,DWORD[8+rbp]
        xor     edx,DWORD[12+rbp]
        mov     rbp,r8

        sub     r10,16
        mov     QWORD[40+rsp],r10
        mov     QWORD[64+rsp],rbp

        mov     DWORD[r9],eax
        mov     DWORD[4+r9],ebx
        mov     DWORD[8+r9],ecx
        mov     DWORD[12+r9],edx

        lea     r8,[16+r8]
        lea     r9,[16+r9]
        jnz     NEAR $L$cbc_fast_dec_loop
        mov     r12,QWORD[56+rsp]
        mov     r10,QWORD[rbp]
        mov     r11,QWORD[8+rbp]
        mov     QWORD[r12],r10
        mov     QWORD[8+r12],r11
        jmp     NEAR $L$cbc_fast_cleanup

ALIGN   16
$L$cbc_fast_dec_in_place:
        mov     r10,QWORD[rbp]
        mov     r11,QWORD[8+rbp]
        mov     QWORD[((0+64))+rsp],r10
        mov     QWORD[((8+64))+rsp],r11
ALIGN   4
$L$cbc_fast_dec_in_place_loop:
        mov     eax,DWORD[r8]
        mov     ebx,DWORD[4+r8]
        mov     ecx,DWORD[8+r8]
        mov     edx,DWORD[12+r8]
        mov     r15,QWORD[rsp]
        mov     QWORD[24+rsp],r8

        call    _x86_64_AES_decrypt

        mov     r8,QWORD[24+rsp]
        mov     r10,QWORD[40+rsp]
        xor     eax,DWORD[((0+64))+rsp]
        xor     ebx,DWORD[((4+64))+rsp]
        xor     ecx,DWORD[((8+64))+rsp]
        xor     edx,DWORD[((12+64))+rsp]

        mov     r11,QWORD[r8]
        mov     r12,QWORD[8+r8]
        sub     r10,16
        jz      NEAR $L$cbc_fast_dec_in_place_done

        mov     QWORD[((0+64))+rsp],r11
        mov     QWORD[((8+64))+rsp],r12

        mov     DWORD[r9],eax
        mov     DWORD[4+r9],ebx
        mov     DWORD[8+r9],ecx
        mov     DWORD[12+r9],edx

        lea     r8,[16+r8]
        lea     r9,[16+r9]
        mov     QWORD[40+rsp],r10
        jmp     NEAR $L$cbc_fast_dec_in_place_loop
$L$cbc_fast_dec_in_place_done:
        mov     rdi,QWORD[56+rsp]
        mov     QWORD[rdi],r11
        mov     QWORD[8+rdi],r12

        mov     DWORD[r9],eax
        mov     DWORD[4+r9],ebx
        mov     DWORD[8+r9],ecx
        mov     DWORD[12+r9],edx

ALIGN   4
$L$cbc_fast_cleanup:
        cmp     DWORD[((80+240))+rsp],0
        lea     rdi,[80+rsp]
        je      NEAR $L$cbc_exit
        mov     ecx,240/8
        xor     rax,rax
        DD      0x90AB48F3

        jmp     NEAR $L$cbc_exit


ALIGN   16
$L$cbc_slow_prologue:


        lea     rbp,[((-88))+rsp]
        and     rbp,-64

        lea     r10,[((-88-63))+rcx]
        sub     r10,rbp
        neg     r10
        and     r10,0x3c0
        sub     rbp,r10

        xchg    rbp,rsp


        mov     QWORD[16+rsp],rbp

$L$cbc_slow_body:




        mov     QWORD[56+rsp],r8
        mov     rbp,r8
        mov     rbx,r9
        mov     r9,rsi
        mov     r8,rdi
        mov     r15,rcx
        mov     r10,rdx

        mov     eax,DWORD[240+r15]
        mov     QWORD[rsp],r15
        shl     eax,4
        lea     rax,[rax*1+r15]
        mov     QWORD[8+rsp],rax


        lea     r14,[2048+r14]
        lea     rax,[((768-8))+rsp]
        sub     rax,r14
        and     rax,0x300
        lea     r14,[rax*1+r14]

        cmp     rbx,0
        je      NEAR $L$SLOW_DECRYPT


        test    r10,-16
        mov     eax,DWORD[rbp]
        mov     ebx,DWORD[4+rbp]
        mov     ecx,DWORD[8+rbp]
        mov     edx,DWORD[12+rbp]
        jz      NEAR $L$cbc_slow_enc_tail

ALIGN   4
$L$cbc_slow_enc_loop:
        xor     eax,DWORD[r8]
        xor     ebx,DWORD[4+r8]
        xor     ecx,DWORD[8+r8]
        xor     edx,DWORD[12+r8]
        mov     r15,QWORD[rsp]
        mov     QWORD[24+rsp],r8
        mov     QWORD[32+rsp],r9
        mov     QWORD[40+rsp],r10

        call    _x86_64_AES_encrypt_compact

        mov     r8,QWORD[24+rsp]
        mov     r9,QWORD[32+rsp]
        mov     r10,QWORD[40+rsp]
        mov     DWORD[r9],eax
        mov     DWORD[4+r9],ebx
        mov     DWORD[8+r9],ecx
        mov     DWORD[12+r9],edx

        lea     r8,[16+r8]
        lea     r9,[16+r9]
        sub     r10,16
        test    r10,-16
        jnz     NEAR $L$cbc_slow_enc_loop
        test    r10,15
        jnz     NEAR $L$cbc_slow_enc_tail
        mov     rbp,QWORD[56+rsp]
        mov     DWORD[rbp],eax
        mov     DWORD[4+rbp],ebx
        mov     DWORD[8+rbp],ecx
        mov     DWORD[12+rbp],edx

        jmp     NEAR $L$cbc_exit

ALIGN   4
$L$cbc_slow_enc_tail:
        mov     r11,rax
        mov     r12,rcx
        mov     rcx,r10
        mov     rsi,r8
        mov     rdi,r9
        DD      0x9066A4F3
        mov     rcx,16
        sub     rcx,r10
        xor     rax,rax
        DD      0x9066AAF3
        mov     r8,r9
        mov     r10,16
        mov     rax,r11
        mov     rcx,r12
        jmp     NEAR $L$cbc_slow_enc_loop

ALIGN   16
$L$SLOW_DECRYPT:
        shr     rax,3
        add     r14,rax

        mov     r11,QWORD[rbp]
        mov     r12,QWORD[8+rbp]
        mov     QWORD[((0+64))+rsp],r11
        mov     QWORD[((8+64))+rsp],r12

ALIGN   4
$L$cbc_slow_dec_loop:
        mov     eax,DWORD[r8]
        mov     ebx,DWORD[4+r8]
        mov     ecx,DWORD[8+r8]
        mov     edx,DWORD[12+r8]
        mov     r15,QWORD[rsp]
        mov     QWORD[24+rsp],r8
        mov     QWORD[32+rsp],r9
        mov     QWORD[40+rsp],r10

        call    _x86_64_AES_decrypt_compact

        mov     r8,QWORD[24+rsp]
        mov     r9,QWORD[32+rsp]
        mov     r10,QWORD[40+rsp]
        xor     eax,DWORD[((0+64))+rsp]
        xor     ebx,DWORD[((4+64))+rsp]
        xor     ecx,DWORD[((8+64))+rsp]
        xor     edx,DWORD[((12+64))+rsp]

        mov     r11,QWORD[r8]
        mov     r12,QWORD[8+r8]
        sub     r10,16
        jc      NEAR $L$cbc_slow_dec_partial
        jz      NEAR $L$cbc_slow_dec_done

        mov     QWORD[((0+64))+rsp],r11
        mov     QWORD[((8+64))+rsp],r12

        mov     DWORD[r9],eax
        mov     DWORD[4+r9],ebx
        mov     DWORD[8+r9],ecx
        mov     DWORD[12+r9],edx

        lea     r8,[16+r8]
        lea     r9,[16+r9]
        jmp     NEAR $L$cbc_slow_dec_loop
$L$cbc_slow_dec_done:
        mov     rdi,QWORD[56+rsp]
        mov     QWORD[rdi],r11
        mov     QWORD[8+rdi],r12

        mov     DWORD[r9],eax
        mov     DWORD[4+r9],ebx
        mov     DWORD[8+r9],ecx
        mov     DWORD[12+r9],edx

        jmp     NEAR $L$cbc_exit

ALIGN   4
$L$cbc_slow_dec_partial:
        mov     rdi,QWORD[56+rsp]
        mov     QWORD[rdi],r11
        mov     QWORD[8+rdi],r12

        mov     DWORD[((0+64))+rsp],eax
        mov     DWORD[((4+64))+rsp],ebx
        mov     DWORD[((8+64))+rsp],ecx
        mov     DWORD[((12+64))+rsp],edx

        mov     rdi,r9
        lea     rsi,[64+rsp]
        lea     rcx,[16+r10]
        DD      0x9066A4F3
        jmp     NEAR $L$cbc_exit

ALIGN   16
$L$cbc_exit:
        mov     rsi,QWORD[16+rsp]

        mov     r15,QWORD[rsi]

        mov     r14,QWORD[8+rsi]

        mov     r13,QWORD[16+rsi]

        mov     r12,QWORD[24+rsi]

        mov     rbp,QWORD[32+rsi]

        mov     rbx,QWORD[40+rsi]

        lea     rsp,[48+rsi]

$L$cbc_popfq:
        popfq



$L$cbc_epilogue:
        mov     rdi,QWORD[8+rsp]        ;WIN64 epilogue
        mov     rsi,QWORD[16+rsp]
        DB      0F3h,0C3h               ;repret

$L$SEH_end_AES_cbc_encrypt:
ALIGN   64
$L$AES_Te:
        DD      0xa56363c6,0xa56363c6
        DD      0x847c7cf8,0x847c7cf8
        DD      0x997777ee,0x997777ee
        DD      0x8d7b7bf6,0x8d7b7bf6
        DD      0x0df2f2ff,0x0df2f2ff
        DD      0xbd6b6bd6,0xbd6b6bd6
        DD      0xb16f6fde,0xb16f6fde
        DD      0x54c5c591,0x54c5c591
        DD      0x50303060,0x50303060
        DD      0x03010102,0x03010102
        DD      0xa96767ce,0xa96767ce
        DD      0x7d2b2b56,0x7d2b2b56
        DD      0x19fefee7,0x19fefee7
        DD      0x62d7d7b5,0x62d7d7b5
        DD      0xe6abab4d,0xe6abab4d
        DD      0x9a7676ec,0x9a7676ec
        DD      0x45caca8f,0x45caca8f
        DD      0x9d82821f,0x9d82821f
        DD      0x40c9c989,0x40c9c989
        DD      0x877d7dfa,0x877d7dfa
        DD      0x15fafaef,0x15fafaef
        DD      0xeb5959b2,0xeb5959b2
        DD      0xc947478e,0xc947478e
        DD      0x0bf0f0fb,0x0bf0f0fb
        DD      0xecadad41,0xecadad41
        DD      0x67d4d4b3,0x67d4d4b3
        DD      0xfda2a25f,0xfda2a25f
        DD      0xeaafaf45,0xeaafaf45
        DD      0xbf9c9c23,0xbf9c9c23
        DD      0xf7a4a453,0xf7a4a453
        DD      0x967272e4,0x967272e4
        DD      0x5bc0c09b,0x5bc0c09b
        DD      0xc2b7b775,0xc2b7b775
        DD      0x1cfdfde1,0x1cfdfde1
        DD      0xae93933d,0xae93933d
        DD      0x6a26264c,0x6a26264c
        DD      0x5a36366c,0x5a36366c
        DD      0x413f3f7e,0x413f3f7e
        DD      0x02f7f7f5,0x02f7f7f5
        DD      0x4fcccc83,0x4fcccc83
        DD      0x5c343468,0x5c343468
        DD      0xf4a5a551,0xf4a5a551
        DD      0x34e5e5d1,0x34e5e5d1
        DD      0x08f1f1f9,0x08f1f1f9
        DD      0x937171e2,0x937171e2
        DD      0x73d8d8ab,0x73d8d8ab
        DD      0x53313162,0x53313162
        DD      0x3f15152a,0x3f15152a
        DD      0x0c040408,0x0c040408
        DD      0x52c7c795,0x52c7c795
        DD      0x65232346,0x65232346
        DD      0x5ec3c39d,0x5ec3c39d
        DD      0x28181830,0x28181830
        DD      0xa1969637,0xa1969637
        DD      0x0f05050a,0x0f05050a
        DD      0xb59a9a2f,0xb59a9a2f
        DD      0x0907070e,0x0907070e
        DD      0x36121224,0x36121224
        DD      0x9b80801b,0x9b80801b
        DD      0x3de2e2df,0x3de2e2df
        DD      0x26ebebcd,0x26ebebcd
        DD      0x6927274e,0x6927274e
        DD      0xcdb2b27f,0xcdb2b27f
        DD      0x9f7575ea,0x9f7575ea
        DD      0x1b090912,0x1b090912
        DD      0x9e83831d,0x9e83831d
        DD      0x742c2c58,0x742c2c58
        DD      0x2e1a1a34,0x2e1a1a34
        DD      0x2d1b1b36,0x2d1b1b36
        DD      0xb26e6edc,0xb26e6edc
        DD      0xee5a5ab4,0xee5a5ab4
        DD      0xfba0a05b,0xfba0a05b
        DD      0xf65252a4,0xf65252a4
        DD      0x4d3b3b76,0x4d3b3b76
        DD      0x61d6d6b7,0x61d6d6b7
        DD      0xceb3b37d,0xceb3b37d
        DD      0x7b292952,0x7b292952
        DD      0x3ee3e3dd,0x3ee3e3dd
        DD      0x712f2f5e,0x712f2f5e
        DD      0x97848413,0x97848413
        DD      0xf55353a6,0xf55353a6
        DD      0x68d1d1b9,0x68d1d1b9
        DD      0x00000000,0x00000000
        DD      0x2cededc1,0x2cededc1
        DD      0x60202040,0x60202040
        DD      0x1ffcfce3,0x1ffcfce3
        DD      0xc8b1b179,0xc8b1b179
        DD      0xed5b5bb6,0xed5b5bb6
        DD      0xbe6a6ad4,0xbe6a6ad4
        DD      0x46cbcb8d,0x46cbcb8d
        DD      0xd9bebe67,0xd9bebe67
        DD      0x4b393972,0x4b393972
        DD      0xde4a4a94,0xde4a4a94
        DD      0xd44c4c98,0xd44c4c98
        DD      0xe85858b0,0xe85858b0
        DD      0x4acfcf85,0x4acfcf85
        DD      0x6bd0d0bb,0x6bd0d0bb
        DD      0x2aefefc5,0x2aefefc5
        DD      0xe5aaaa4f,0xe5aaaa4f
        DD      0x16fbfbed,0x16fbfbed
        DD      0xc5434386,0xc5434386
        DD      0xd74d4d9a,0xd74d4d9a
        DD      0x55333366,0x55333366
        DD      0x94858511,0x94858511
        DD      0xcf45458a,0xcf45458a
        DD      0x10f9f9e9,0x10f9f9e9
        DD      0x06020204,0x06020204
        DD      0x817f7ffe,0x817f7ffe
        DD      0xf05050a0,0xf05050a0
        DD      0x443c3c78,0x443c3c78
        DD      0xba9f9f25,0xba9f9f25
        DD      0xe3a8a84b,0xe3a8a84b
        DD      0xf35151a2,0xf35151a2
        DD      0xfea3a35d,0xfea3a35d
        DD      0xc0404080,0xc0404080
        DD      0x8a8f8f05,0x8a8f8f05
        DD      0xad92923f,0xad92923f
        DD      0xbc9d9d21,0xbc9d9d21
        DD      0x48383870,0x48383870
        DD      0x04f5f5f1,0x04f5f5f1
        DD      0xdfbcbc63,0xdfbcbc63
        DD      0xc1b6b677,0xc1b6b677
        DD      0x75dadaaf,0x75dadaaf
        DD      0x63212142,0x63212142
        DD      0x30101020,0x30101020
        DD      0x1affffe5,0x1affffe5
        DD      0x0ef3f3fd,0x0ef3f3fd
        DD      0x6dd2d2bf,0x6dd2d2bf
        DD      0x4ccdcd81,0x4ccdcd81
        DD      0x140c0c18,0x140c0c18
        DD      0x35131326,0x35131326
        DD      0x2fececc3,0x2fececc3
        DD      0xe15f5fbe,0xe15f5fbe
        DD      0xa2979735,0xa2979735
        DD      0xcc444488,0xcc444488
        DD      0x3917172e,0x3917172e
        DD      0x57c4c493,0x57c4c493
        DD      0xf2a7a755,0xf2a7a755
        DD      0x827e7efc,0x827e7efc
        DD      0x473d3d7a,0x473d3d7a
        DD      0xac6464c8,0xac6464c8
        DD      0xe75d5dba,0xe75d5dba
        DD      0x2b191932,0x2b191932
        DD      0x957373e6,0x957373e6
        DD      0xa06060c0,0xa06060c0
        DD      0x98818119,0x98818119
        DD      0xd14f4f9e,0xd14f4f9e
        DD      0x7fdcdca3,0x7fdcdca3
        DD      0x66222244,0x66222244
        DD      0x7e2a2a54,0x7e2a2a54
        DD      0xab90903b,0xab90903b
        DD      0x8388880b,0x8388880b
        DD      0xca46468c,0xca46468c
        DD      0x29eeeec7,0x29eeeec7
        DD      0xd3b8b86b,0xd3b8b86b
        DD      0x3c141428,0x3c141428
        DD      0x79dedea7,0x79dedea7
        DD      0xe25e5ebc,0xe25e5ebc
        DD      0x1d0b0b16,0x1d0b0b16
        DD      0x76dbdbad,0x76dbdbad
        DD      0x3be0e0db,0x3be0e0db
        DD      0x56323264,0x56323264
        DD      0x4e3a3a74,0x4e3a3a74
        DD      0x1e0a0a14,0x1e0a0a14
        DD      0xdb494992,0xdb494992
        DD      0x0a06060c,0x0a06060c
        DD      0x6c242448,0x6c242448
        DD      0xe45c5cb8,0xe45c5cb8
        DD      0x5dc2c29f,0x5dc2c29f
        DD      0x6ed3d3bd,0x6ed3d3bd
        DD      0xefacac43,0xefacac43
        DD      0xa66262c4,0xa66262c4
        DD      0xa8919139,0xa8919139
        DD      0xa4959531,0xa4959531
        DD      0x37e4e4d3,0x37e4e4d3
        DD      0x8b7979f2,0x8b7979f2
        DD      0x32e7e7d5,0x32e7e7d5
        DD      0x43c8c88b,0x43c8c88b
        DD      0x5937376e,0x5937376e
        DD      0xb76d6dda,0xb76d6dda
        DD      0x8c8d8d01,0x8c8d8d01
        DD      0x64d5d5b1,0x64d5d5b1
        DD      0xd24e4e9c,0xd24e4e9c
        DD      0xe0a9a949,0xe0a9a949
        DD      0xb46c6cd8,0xb46c6cd8
        DD      0xfa5656ac,0xfa5656ac
        DD      0x07f4f4f3,0x07f4f4f3
        DD      0x25eaeacf,0x25eaeacf
        DD      0xaf6565ca,0xaf6565ca
        DD      0x8e7a7af4,0x8e7a7af4
        DD      0xe9aeae47,0xe9aeae47
        DD      0x18080810,0x18080810
        DD      0xd5baba6f,0xd5baba6f
        DD      0x887878f0,0x887878f0
        DD      0x6f25254a,0x6f25254a
        DD      0x722e2e5c,0x722e2e5c
        DD      0x241c1c38,0x241c1c38
        DD      0xf1a6a657,0xf1a6a657
        DD      0xc7b4b473,0xc7b4b473
        DD      0x51c6c697,0x51c6c697
        DD      0x23e8e8cb,0x23e8e8cb
        DD      0x7cdddda1,0x7cdddda1
        DD      0x9c7474e8,0x9c7474e8
        DD      0x211f1f3e,0x211f1f3e
        DD      0xdd4b4b96,0xdd4b4b96
        DD      0xdcbdbd61,0xdcbdbd61
        DD      0x868b8b0d,0x868b8b0d
        DD      0x858a8a0f,0x858a8a0f
        DD      0x907070e0,0x907070e0
        DD      0x423e3e7c,0x423e3e7c
        DD      0xc4b5b571,0xc4b5b571
        DD      0xaa6666cc,0xaa6666cc
        DD      0xd8484890,0xd8484890
        DD      0x05030306,0x05030306
        DD      0x01f6f6f7,0x01f6f6f7
        DD      0x120e0e1c,0x120e0e1c
        DD      0xa36161c2,0xa36161c2
        DD      0x5f35356a,0x5f35356a
        DD      0xf95757ae,0xf95757ae
        DD      0xd0b9b969,0xd0b9b969
        DD      0x91868617,0x91868617
        DD      0x58c1c199,0x58c1c199
        DD      0x271d1d3a,0x271d1d3a
        DD      0xb99e9e27,0xb99e9e27
        DD      0x38e1e1d9,0x38e1e1d9
        DD      0x13f8f8eb,0x13f8f8eb
        DD      0xb398982b,0xb398982b
        DD      0x33111122,0x33111122
        DD      0xbb6969d2,0xbb6969d2
        DD      0x70d9d9a9,0x70d9d9a9
        DD      0x898e8e07,0x898e8e07
        DD      0xa7949433,0xa7949433
        DD      0xb69b9b2d,0xb69b9b2d
        DD      0x221e1e3c,0x221e1e3c
        DD      0x92878715,0x92878715
        DD      0x20e9e9c9,0x20e9e9c9
        DD      0x49cece87,0x49cece87
        DD      0xff5555aa,0xff5555aa
        DD      0x78282850,0x78282850
        DD      0x7adfdfa5,0x7adfdfa5
        DD      0x8f8c8c03,0x8f8c8c03
        DD      0xf8a1a159,0xf8a1a159
        DD      0x80898909,0x80898909
        DD      0x170d0d1a,0x170d0d1a
        DD      0xdabfbf65,0xdabfbf65
        DD      0x31e6e6d7,0x31e6e6d7
        DD      0xc6424284,0xc6424284
        DD      0xb86868d0,0xb86868d0
        DD      0xc3414182,0xc3414182
        DD      0xb0999929,0xb0999929
        DD      0x772d2d5a,0x772d2d5a
        DD      0x110f0f1e,0x110f0f1e
        DD      0xcbb0b07b,0xcbb0b07b
        DD      0xfc5454a8,0xfc5454a8
        DD      0xd6bbbb6d,0xd6bbbb6d
        DD      0x3a16162c,0x3a16162c
DB      0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5
DB      0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76
DB      0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0
DB      0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0
DB      0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc
DB      0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15
DB      0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a
DB      0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75
DB      0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0
DB      0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84
DB      0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b
DB      0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf
DB      0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85
DB      0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8
DB      0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5
DB      0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2
DB      0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17
DB      0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73
DB      0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88
DB      0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb
DB      0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c
DB      0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79
DB      0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9
DB      0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08
DB      0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6
DB      0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a
DB      0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e
DB      0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e
DB      0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94
DB      0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf
DB      0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68
DB      0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16
DB      0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5
DB      0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76
DB      0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0
DB      0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0
DB      0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc
DB      0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15
DB      0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a
DB      0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75
DB      0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0
DB      0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84
DB      0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b
DB      0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf
DB      0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85
DB      0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8
DB      0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5
DB      0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2
DB      0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17
DB      0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73
DB      0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88
DB      0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb
DB      0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c
DB      0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79
DB      0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9
DB      0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08
DB      0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6
DB      0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a
DB      0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e
DB      0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e
DB      0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94
DB      0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf
DB      0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68
DB      0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16
DB      0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5
DB      0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76
DB      0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0
DB      0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0
DB      0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc
DB      0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15
DB      0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a
DB      0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75
DB      0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0
DB      0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84
DB      0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b
DB      0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf
DB      0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85
DB      0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8
DB      0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5
DB      0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2
DB      0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17
DB      0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73
DB      0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88
DB      0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb
DB      0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c
DB      0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79
DB      0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9
DB      0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08
DB      0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6
DB      0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a
DB      0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e
DB      0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e
DB      0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94
DB      0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf
DB      0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68
DB      0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16
DB      0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5
DB      0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76
DB      0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0
DB      0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0
DB      0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc
DB      0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15
DB      0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a
DB      0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75
DB      0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0
DB      0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84
DB      0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b
DB      0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf
DB      0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85
DB      0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8
DB      0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5
DB      0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2
DB      0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17
DB      0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73
DB      0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88
DB      0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb
DB      0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c
DB      0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79
DB      0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9
DB      0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08
DB      0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6
DB      0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a
DB      0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e
DB      0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e
DB      0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94
DB      0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf
DB      0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68
DB      0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16
        DD      0x00000001,0x00000002,0x00000004,0x00000008
        DD      0x00000010,0x00000020,0x00000040,0x00000080
        DD      0x0000001b,0x00000036,0x80808080,0x80808080
        DD      0xfefefefe,0xfefefefe,0x1b1b1b1b,0x1b1b1b1b
ALIGN   64
$L$AES_Td:
        DD      0x50a7f451,0x50a7f451
        DD      0x5365417e,0x5365417e
        DD      0xc3a4171a,0xc3a4171a
        DD      0x965e273a,0x965e273a
        DD      0xcb6bab3b,0xcb6bab3b
        DD      0xf1459d1f,0xf1459d1f
        DD      0xab58faac,0xab58faac
        DD      0x9303e34b,0x9303e34b
        DD      0x55fa3020,0x55fa3020
        DD      0xf66d76ad,0xf66d76ad
        DD      0x9176cc88,0x9176cc88
        DD      0x254c02f5,0x254c02f5
        DD      0xfcd7e54f,0xfcd7e54f
        DD      0xd7cb2ac5,0xd7cb2ac5
        DD      0x80443526,0x80443526
        DD      0x8fa362b5,0x8fa362b5
        DD      0x495ab1de,0x495ab1de
        DD      0x671bba25,0x671bba25
        DD      0x980eea45,0x980eea45
        DD      0xe1c0fe5d,0xe1c0fe5d
        DD      0x02752fc3,0x02752fc3
        DD      0x12f04c81,0x12f04c81
        DD      0xa397468d,0xa397468d
        DD      0xc6f9d36b,0xc6f9d36b
        DD      0xe75f8f03,0xe75f8f03
        DD      0x959c9215,0x959c9215
        DD      0xeb7a6dbf,0xeb7a6dbf
        DD      0xda595295,0xda595295
        DD      0x2d83bed4,0x2d83bed4
        DD      0xd3217458,0xd3217458
        DD      0x2969e049,0x2969e049
        DD      0x44c8c98e,0x44c8c98e
        DD      0x6a89c275,0x6a89c275
        DD      0x78798ef4,0x78798ef4
        DD      0x6b3e5899,0x6b3e5899
        DD      0xdd71b927,0xdd71b927
        DD      0xb64fe1be,0xb64fe1be
        DD      0x17ad88f0,0x17ad88f0
        DD      0x66ac20c9,0x66ac20c9
        DD      0xb43ace7d,0xb43ace7d
        DD      0x184adf63,0x184adf63
        DD      0x82311ae5,0x82311ae5
        DD      0x60335197,0x60335197
        DD      0x457f5362,0x457f5362
        DD      0xe07764b1,0xe07764b1
        DD      0x84ae6bbb,0x84ae6bbb
        DD      0x1ca081fe,0x1ca081fe
        DD      0x942b08f9,0x942b08f9
        DD      0x58684870,0x58684870
        DD      0x19fd458f,0x19fd458f
        DD      0x876cde94,0x876cde94
        DD      0xb7f87b52,0xb7f87b52
        DD      0x23d373ab,0x23d373ab
        DD      0xe2024b72,0xe2024b72
        DD      0x578f1fe3,0x578f1fe3
        DD      0x2aab5566,0x2aab5566
        DD      0x0728ebb2,0x0728ebb2
        DD      0x03c2b52f,0x03c2b52f
        DD      0x9a7bc586,0x9a7bc586
        DD      0xa50837d3,0xa50837d3
        DD      0xf2872830,0xf2872830
        DD      0xb2a5bf23,0xb2a5bf23
        DD      0xba6a0302,0xba6a0302
        DD      0x5c8216ed,0x5c8216ed
        DD      0x2b1ccf8a,0x2b1ccf8a
        DD      0x92b479a7,0x92b479a7
        DD      0xf0f207f3,0xf0f207f3
        DD      0xa1e2694e,0xa1e2694e
        DD      0xcdf4da65,0xcdf4da65
        DD      0xd5be0506,0xd5be0506
        DD      0x1f6234d1,0x1f6234d1
        DD      0x8afea6c4,0x8afea6c4
        DD      0x9d532e34,0x9d532e34
        DD      0xa055f3a2,0xa055f3a2
        DD      0x32e18a05,0x32e18a05
        DD      0x75ebf6a4,0x75ebf6a4
        DD      0x39ec830b,0x39ec830b
        DD      0xaaef6040,0xaaef6040
        DD      0x069f715e,0x069f715e
        DD      0x51106ebd,0x51106ebd
        DD      0xf98a213e,0xf98a213e
        DD      0x3d06dd96,0x3d06dd96
        DD      0xae053edd,0xae053edd
        DD      0x46bde64d,0x46bde64d
        DD      0xb58d5491,0xb58d5491
        DD      0x055dc471,0x055dc471
        DD      0x6fd40604,0x6fd40604
        DD      0xff155060,0xff155060
        DD      0x24fb9819,0x24fb9819
        DD      0x97e9bdd6,0x97e9bdd6
        DD      0xcc434089,0xcc434089
        DD      0x779ed967,0x779ed967
        DD      0xbd42e8b0,0xbd42e8b0
        DD      0x888b8907,0x888b8907
        DD      0x385b19e7,0x385b19e7
        DD      0xdbeec879,0xdbeec879
        DD      0x470a7ca1,0x470a7ca1
        DD      0xe90f427c,0xe90f427c
        DD      0xc91e84f8,0xc91e84f8
        DD      0x00000000,0x00000000
        DD      0x83868009,0x83868009
        DD      0x48ed2b32,0x48ed2b32
        DD      0xac70111e,0xac70111e
        DD      0x4e725a6c,0x4e725a6c
        DD      0xfbff0efd,0xfbff0efd
        DD      0x5638850f,0x5638850f
        DD      0x1ed5ae3d,0x1ed5ae3d
        DD      0x27392d36,0x27392d36
        DD      0x64d90f0a,0x64d90f0a
        DD      0x21a65c68,0x21a65c68
        DD      0xd1545b9b,0xd1545b9b
        DD      0x3a2e3624,0x3a2e3624
        DD      0xb1670a0c,0xb1670a0c
        DD      0x0fe75793,0x0fe75793
        DD      0xd296eeb4,0xd296eeb4
        DD      0x9e919b1b,0x9e919b1b
        DD      0x4fc5c080,0x4fc5c080
        DD      0xa220dc61,0xa220dc61
        DD      0x694b775a,0x694b775a
        DD      0x161a121c,0x161a121c
        DD      0x0aba93e2,0x0aba93e2
        DD      0xe52aa0c0,0xe52aa0c0
        DD      0x43e0223c,0x43e0223c
        DD      0x1d171b12,0x1d171b12
        DD      0x0b0d090e,0x0b0d090e
        DD      0xadc78bf2,0xadc78bf2
        DD      0xb9a8b62d,0xb9a8b62d
        DD      0xc8a91e14,0xc8a91e14
        DD      0x8519f157,0x8519f157
        DD      0x4c0775af,0x4c0775af
        DD      0xbbdd99ee,0xbbdd99ee
        DD      0xfd607fa3,0xfd607fa3
        DD      0x9f2601f7,0x9f2601f7
        DD      0xbcf5725c,0xbcf5725c
        DD      0xc53b6644,0xc53b6644
        DD      0x347efb5b,0x347efb5b
        DD      0x7629438b,0x7629438b
        DD      0xdcc623cb,0xdcc623cb
        DD      0x68fcedb6,0x68fcedb6
        DD      0x63f1e4b8,0x63f1e4b8
        DD      0xcadc31d7,0xcadc31d7
        DD      0x10856342,0x10856342
        DD      0x40229713,0x40229713
        DD      0x2011c684,0x2011c684
        DD      0x7d244a85,0x7d244a85
        DD      0xf83dbbd2,0xf83dbbd2
        DD      0x1132f9ae,0x1132f9ae
        DD      0x6da129c7,0x6da129c7
        DD      0x4b2f9e1d,0x4b2f9e1d
        DD      0xf330b2dc,0xf330b2dc
        DD      0xec52860d,0xec52860d
        DD      0xd0e3c177,0xd0e3c177
        DD      0x6c16b32b,0x6c16b32b
        DD      0x99b970a9,0x99b970a9
        DD      0xfa489411,0xfa489411
        DD      0x2264e947,0x2264e947
        DD      0xc48cfca8,0xc48cfca8
        DD      0x1a3ff0a0,0x1a3ff0a0
        DD      0xd82c7d56,0xd82c7d56
        DD      0xef903322,0xef903322
        DD      0xc74e4987,0xc74e4987
        DD      0xc1d138d9,0xc1d138d9
        DD      0xfea2ca8c,0xfea2ca8c
        DD      0x360bd498,0x360bd498
        DD      0xcf81f5a6,0xcf81f5a6
        DD      0x28de7aa5,0x28de7aa5
        DD      0x268eb7da,0x268eb7da
        DD      0xa4bfad3f,0xa4bfad3f
        DD      0xe49d3a2c,0xe49d3a2c
        DD      0x0d927850,0x0d927850
        DD      0x9bcc5f6a,0x9bcc5f6a
        DD      0x62467e54,0x62467e54
        DD      0xc2138df6,0xc2138df6
        DD      0xe8b8d890,0xe8b8d890
        DD      0x5ef7392e,0x5ef7392e
        DD      0xf5afc382,0xf5afc382
        DD      0xbe805d9f,0xbe805d9f
        DD      0x7c93d069,0x7c93d069
        DD      0xa92dd56f,0xa92dd56f
        DD      0xb31225cf,0xb31225cf
        DD      0x3b99acc8,0x3b99acc8
        DD      0xa77d1810,0xa77d1810
        DD      0x6e639ce8,0x6e639ce8
        DD      0x7bbb3bdb,0x7bbb3bdb
        DD      0x097826cd,0x097826cd
        DD      0xf418596e,0xf418596e
        DD      0x01b79aec,0x01b79aec
        DD      0xa89a4f83,0xa89a4f83
        DD      0x656e95e6,0x656e95e6
        DD      0x7ee6ffaa,0x7ee6ffaa
        DD      0x08cfbc21,0x08cfbc21
        DD      0xe6e815ef,0xe6e815ef
        DD      0xd99be7ba,0xd99be7ba
        DD      0xce366f4a,0xce366f4a
        DD      0xd4099fea,0xd4099fea
        DD      0xd67cb029,0xd67cb029
        DD      0xafb2a431,0xafb2a431
        DD      0x31233f2a,0x31233f2a
        DD      0x3094a5c6,0x3094a5c6
        DD      0xc066a235,0xc066a235
        DD      0x37bc4e74,0x37bc4e74
        DD      0xa6ca82fc,0xa6ca82fc
        DD      0xb0d090e0,0xb0d090e0
        DD      0x15d8a733,0x15d8a733
        DD      0x4a9804f1,0x4a9804f1
        DD      0xf7daec41,0xf7daec41
        DD      0x0e50cd7f,0x0e50cd7f
        DD      0x2ff69117,0x2ff69117
        DD      0x8dd64d76,0x8dd64d76
        DD      0x4db0ef43,0x4db0ef43
        DD      0x544daacc,0x544daacc
        DD      0xdf0496e4,0xdf0496e4
        DD      0xe3b5d19e,0xe3b5d19e
        DD      0x1b886a4c,0x1b886a4c
        DD      0xb81f2cc1,0xb81f2cc1
        DD      0x7f516546,0x7f516546
        DD      0x04ea5e9d,0x04ea5e9d
        DD      0x5d358c01,0x5d358c01
        DD      0x737487fa,0x737487fa
        DD      0x2e410bfb,0x2e410bfb
        DD      0x5a1d67b3,0x5a1d67b3
        DD      0x52d2db92,0x52d2db92
        DD      0x335610e9,0x335610e9
        DD      0x1347d66d,0x1347d66d
        DD      0x8c61d79a,0x8c61d79a
        DD      0x7a0ca137,0x7a0ca137
        DD      0x8e14f859,0x8e14f859
        DD      0x893c13eb,0x893c13eb
        DD      0xee27a9ce,0xee27a9ce
        DD      0x35c961b7,0x35c961b7
        DD      0xede51ce1,0xede51ce1
        DD      0x3cb1477a,0x3cb1477a
        DD      0x59dfd29c,0x59dfd29c
        DD      0x3f73f255,0x3f73f255
        DD      0x79ce1418,0x79ce1418
        DD      0xbf37c773,0xbf37c773
        DD      0xeacdf753,0xeacdf753
        DD      0x5baafd5f,0x5baafd5f
        DD      0x146f3ddf,0x146f3ddf
        DD      0x86db4478,0x86db4478
        DD      0x81f3afca,0x81f3afca
        DD      0x3ec468b9,0x3ec468b9
        DD      0x2c342438,0x2c342438
        DD      0x5f40a3c2,0x5f40a3c2
        DD      0x72c31d16,0x72c31d16
        DD      0x0c25e2bc,0x0c25e2bc
        DD      0x8b493c28,0x8b493c28
        DD      0x41950dff,0x41950dff
        DD      0x7101a839,0x7101a839
        DD      0xdeb30c08,0xdeb30c08
        DD      0x9ce4b4d8,0x9ce4b4d8
        DD      0x90c15664,0x90c15664
        DD      0x6184cb7b,0x6184cb7b
        DD      0x70b632d5,0x70b632d5
        DD      0x745c6c48,0x745c6c48
        DD      0x4257b8d0,0x4257b8d0
DB      0x52,0x09,0x6a,0xd5,0x30,0x36,0xa5,0x38
DB      0xbf,0x40,0xa3,0x9e,0x81,0xf3,0xd7,0xfb
DB      0x7c,0xe3,0x39,0x82,0x9b,0x2f,0xff,0x87
DB      0x34,0x8e,0x43,0x44,0xc4,0xde,0xe9,0xcb
DB      0x54,0x7b,0x94,0x32,0xa6,0xc2,0x23,0x3d
DB      0xee,0x4c,0x95,0x0b,0x42,0xfa,0xc3,0x4e
DB      0x08,0x2e,0xa1,0x66,0x28,0xd9,0x24,0xb2
DB      0x76,0x5b,0xa2,0x49,0x6d,0x8b,0xd1,0x25
DB      0x72,0xf8,0xf6,0x64,0x86,0x68,0x98,0x16
DB      0xd4,0xa4,0x5c,0xcc,0x5d,0x65,0xb6,0x92
DB      0x6c,0x70,0x48,0x50,0xfd,0xed,0xb9,0xda
DB      0x5e,0x15,0x46,0x57,0xa7,0x8d,0x9d,0x84
DB      0x90,0xd8,0xab,0x00,0x8c,0xbc,0xd3,0x0a
DB      0xf7,0xe4,0x58,0x05,0xb8,0xb3,0x45,0x06
DB      0xd0,0x2c,0x1e,0x8f,0xca,0x3f,0x0f,0x02
DB      0xc1,0xaf,0xbd,0x03,0x01,0x13,0x8a,0x6b
DB      0x3a,0x91,0x11,0x41,0x4f,0x67,0xdc,0xea
DB      0x97,0xf2,0xcf,0xce,0xf0,0xb4,0xe6,0x73
DB      0x96,0xac,0x74,0x22,0xe7,0xad,0x35,0x85
DB      0xe2,0xf9,0x37,0xe8,0x1c,0x75,0xdf,0x6e
DB      0x47,0xf1,0x1a,0x71,0x1d,0x29,0xc5,0x89
DB      0x6f,0xb7,0x62,0x0e,0xaa,0x18,0xbe,0x1b
DB      0xfc,0x56,0x3e,0x4b,0xc6,0xd2,0x79,0x20
DB      0x9a,0xdb,0xc0,0xfe,0x78,0xcd,0x5a,0xf4
DB      0x1f,0xdd,0xa8,0x33,0x88,0x07,0xc7,0x31
DB      0xb1,0x12,0x10,0x59,0x27,0x80,0xec,0x5f
DB      0x60,0x51,0x7f,0xa9,0x19,0xb5,0x4a,0x0d
DB      0x2d,0xe5,0x7a,0x9f,0x93,0xc9,0x9c,0xef
DB      0xa0,0xe0,0x3b,0x4d,0xae,0x2a,0xf5,0xb0
DB      0xc8,0xeb,0xbb,0x3c,0x83,0x53,0x99,0x61
DB      0x17,0x2b,0x04,0x7e,0xba,0x77,0xd6,0x26
DB      0xe1,0x69,0x14,0x63,0x55,0x21,0x0c,0x7d
        DD      0x80808080,0x80808080,0xfefefefe,0xfefefefe
        DD      0x1b1b1b1b,0x1b1b1b1b,0,0
DB      0x52,0x09,0x6a,0xd5,0x30,0x36,0xa5,0x38
DB      0xbf,0x40,0xa3,0x9e,0x81,0xf3,0xd7,0xfb
DB      0x7c,0xe3,0x39,0x82,0x9b,0x2f,0xff,0x87
DB      0x34,0x8e,0x43,0x44,0xc4,0xde,0xe9,0xcb
DB      0x54,0x7b,0x94,0x32,0xa6,0xc2,0x23,0x3d
DB      0xee,0x4c,0x95,0x0b,0x42,0xfa,0xc3,0x4e
DB      0x08,0x2e,0xa1,0x66,0x28,0xd9,0x24,0xb2
DB      0x76,0x5b,0xa2,0x49,0x6d,0x8b,0xd1,0x25
DB      0x72,0xf8,0xf6,0x64,0x86,0x68,0x98,0x16
DB      0xd4,0xa4,0x5c,0xcc,0x5d,0x65,0xb6,0x92
DB      0x6c,0x70,0x48,0x50,0xfd,0xed,0xb9,0xda
DB      0x5e,0x15,0x46,0x57,0xa7,0x8d,0x9d,0x84
DB      0x90,0xd8,0xab,0x00,0x8c,0xbc,0xd3,0x0a
DB      0xf7,0xe4,0x58,0x05,0xb8,0xb3,0x45,0x06
DB      0xd0,0x2c,0x1e,0x8f,0xca,0x3f,0x0f,0x02
DB      0xc1,0xaf,0xbd,0x03,0x01,0x13,0x8a,0x6b
DB      0x3a,0x91,0x11,0x41,0x4f,0x67,0xdc,0xea
DB      0x97,0xf2,0xcf,0xce,0xf0,0xb4,0xe6,0x73
DB      0x96,0xac,0x74,0x22,0xe7,0xad,0x35,0x85
DB      0xe2,0xf9,0x37,0xe8,0x1c,0x75,0xdf,0x6e
DB      0x47,0xf1,0x1a,0x71,0x1d,0x29,0xc5,0x89
DB      0x6f,0xb7,0x62,0x0e,0xaa,0x18,0xbe,0x1b
DB      0xfc,0x56,0x3e,0x4b,0xc6,0xd2,0x79,0x20
DB      0x9a,0xdb,0xc0,0xfe,0x78,0xcd,0x5a,0xf4
DB      0x1f,0xdd,0xa8,0x33,0x88,0x07,0xc7,0x31
DB      0xb1,0x12,0x10,0x59,0x27,0x80,0xec,0x5f
DB      0x60,0x51,0x7f,0xa9,0x19,0xb5,0x4a,0x0d
DB      0x2d,0xe5,0x7a,0x9f,0x93,0xc9,0x9c,0xef
DB      0xa0,0xe0,0x3b,0x4d,0xae,0x2a,0xf5,0xb0
DB      0xc8,0xeb,0xbb,0x3c,0x83,0x53,0x99,0x61
DB      0x17,0x2b,0x04,0x7e,0xba,0x77,0xd6,0x26
DB      0xe1,0x69,0x14,0x63,0x55,0x21,0x0c,0x7d
        DD      0x80808080,0x80808080,0xfefefefe,0xfefefefe
        DD      0x1b1b1b1b,0x1b1b1b1b,0,0
DB      0x52,0x09,0x6a,0xd5,0x30,0x36,0xa5,0x38
DB      0xbf,0x40,0xa3,0x9e,0x81,0xf3,0xd7,0xfb
DB      0x7c,0xe3,0x39,0x82,0x9b,0x2f,0xff,0x87
DB      0x34,0x8e,0x43,0x44,0xc4,0xde,0xe9,0xcb
DB      0x54,0x7b,0x94,0x32,0xa6,0xc2,0x23,0x3d
DB      0xee,0x4c,0x95,0x0b,0x42,0xfa,0xc3,0x4e
DB      0x08,0x2e,0xa1,0x66,0x28,0xd9,0x24,0xb2
DB      0x76,0x5b,0xa2,0x49,0x6d,0x8b,0xd1,0x25
DB      0x72,0xf8,0xf6,0x64,0x86,0x68,0x98,0x16
DB      0xd4,0xa4,0x5c,0xcc,0x5d,0x65,0xb6,0x92
DB      0x6c,0x70,0x48,0x50,0xfd,0xed,0xb9,0xda
DB      0x5e,0x15,0x46,0x57,0xa7,0x8d,0x9d,0x84
DB      0x90,0xd8,0xab,0x00,0x8c,0xbc,0xd3,0x0a
DB      0xf7,0xe4,0x58,0x05,0xb8,0xb3,0x45,0x06
DB      0xd0,0x2c,0x1e,0x8f,0xca,0x3f,0x0f,0x02
DB      0xc1,0xaf,0xbd,0x03,0x01,0x13,0x8a,0x6b
DB      0x3a,0x91,0x11,0x41,0x4f,0x67,0xdc,0xea
DB      0x97,0xf2,0xcf,0xce,0xf0,0xb4,0xe6,0x73
DB      0x96,0xac,0x74,0x22,0xe7,0xad,0x35,0x85
DB      0xe2,0xf9,0x37,0xe8,0x1c,0x75,0xdf,0x6e
DB      0x47,0xf1,0x1a,0x71,0x1d,0x29,0xc5,0x89
DB      0x6f,0xb7,0x62,0x0e,0xaa,0x18,0xbe,0x1b
DB      0xfc,0x56,0x3e,0x4b,0xc6,0xd2,0x79,0x20
DB      0x9a,0xdb,0xc0,0xfe,0x78,0xcd,0x5a,0xf4
DB      0x1f,0xdd,0xa8,0x33,0x88,0x07,0xc7,0x31
DB      0xb1,0x12,0x10,0x59,0x27,0x80,0xec,0x5f
DB      0x60,0x51,0x7f,0xa9,0x19,0xb5,0x4a,0x0d
DB      0x2d,0xe5,0x7a,0x9f,0x93,0xc9,0x9c,0xef
DB      0xa0,0xe0,0x3b,0x4d,0xae,0x2a,0xf5,0xb0
DB      0xc8,0xeb,0xbb,0x3c,0x83,0x53,0x99,0x61
DB      0x17,0x2b,0x04,0x7e,0xba,0x77,0xd6,0x26
DB      0xe1,0x69,0x14,0x63,0x55,0x21,0x0c,0x7d
        DD      0x80808080,0x80808080,0xfefefefe,0xfefefefe
        DD      0x1b1b1b1b,0x1b1b1b1b,0,0
DB      0x52,0x09,0x6a,0xd5,0x30,0x36,0xa5,0x38
DB      0xbf,0x40,0xa3,0x9e,0x81,0xf3,0xd7,0xfb
DB      0x7c,0xe3,0x39,0x82,0x9b,0x2f,0xff,0x87
DB      0x34,0x8e,0x43,0x44,0xc4,0xde,0xe9,0xcb
DB      0x54,0x7b,0x94,0x32,0xa6,0xc2,0x23,0x3d
DB      0xee,0x4c,0x95,0x0b,0x42,0xfa,0xc3,0x4e
DB      0x08,0x2e,0xa1,0x66,0x28,0xd9,0x24,0xb2
DB      0x76,0x5b,0xa2,0x49,0x6d,0x8b,0xd1,0x25
DB      0x72,0xf8,0xf6,0x64,0x86,0x68,0x98,0x16
DB      0xd4,0xa4,0x5c,0xcc,0x5d,0x65,0xb6,0x92
DB      0x6c,0x70,0x48,0x50,0xfd,0xed,0xb9,0xda
DB      0x5e,0x15,0x46,0x57,0xa7,0x8d,0x9d,0x84
DB      0x90,0xd8,0xab,0x00,0x8c,0xbc,0xd3,0x0a
DB      0xf7,0xe4,0x58,0x05,0xb8,0xb3,0x45,0x06
DB      0xd0,0x2c,0x1e,0x8f,0xca,0x3f,0x0f,0x02
DB      0xc1,0xaf,0xbd,0x03,0x01,0x13,0x8a,0x6b
DB      0x3a,0x91,0x11,0x41,0x4f,0x67,0xdc,0xea
DB      0x97,0xf2,0xcf,0xce,0xf0,0xb4,0xe6,0x73
DB      0x96,0xac,0x74,0x22,0xe7,0xad,0x35,0x85
DB      0xe2,0xf9,0x37,0xe8,0x1c,0x75,0xdf,0x6e
DB      0x47,0xf1,0x1a,0x71,0x1d,0x29,0xc5,0x89
DB      0x6f,0xb7,0x62,0x0e,0xaa,0x18,0xbe,0x1b
DB      0xfc,0x56,0x3e,0x4b,0xc6,0xd2,0x79,0x20
DB      0x9a,0xdb,0xc0,0xfe,0x78,0xcd,0x5a,0xf4
DB      0x1f,0xdd,0xa8,0x33,0x88,0x07,0xc7,0x31
DB      0xb1,0x12,0x10,0x59,0x27,0x80,0xec,0x5f
DB      0x60,0x51,0x7f,0xa9,0x19,0xb5,0x4a,0x0d
DB      0x2d,0xe5,0x7a,0x9f,0x93,0xc9,0x9c,0xef
DB      0xa0,0xe0,0x3b,0x4d,0xae,0x2a,0xf5,0xb0
DB      0xc8,0xeb,0xbb,0x3c,0x83,0x53,0x99,0x61
DB      0x17,0x2b,0x04,0x7e,0xba,0x77,0xd6,0x26
DB      0xe1,0x69,0x14,0x63,0x55,0x21,0x0c,0x7d
        DD      0x80808080,0x80808080,0xfefefefe,0xfefefefe
        DD      0x1b1b1b1b,0x1b1b1b1b,0,0
DB      65,69,83,32,102,111,114,32,120,56,54,95,54,52,44,32
DB      67,82,89,80,84,79,71,65,77,83,32,98,121,32,60,97
DB      112,112,114,111,64,111,112,101,110,115,115,108,46,111,114,103
DB      62,0
ALIGN   64
EXTERN  __imp_RtlVirtualUnwind

ALIGN   16
block_se_handler:
        push    rsi
        push    rdi
        push    rbx
        push    rbp
        push    r12
        push    r13
        push    r14
        push    r15
        pushfq
        sub     rsp,64

        mov     rax,QWORD[120+r8]
        mov     rbx,QWORD[248+r8]

        mov     rsi,QWORD[8+r9]
        mov     r11,QWORD[56+r9]

        mov     r10d,DWORD[r11]
        lea     r10,[r10*1+rsi]
        cmp     rbx,r10
        jb      NEAR $L$in_block_prologue

        mov     rax,QWORD[152+r8]

        mov     r10d,DWORD[4+r11]
        lea     r10,[r10*1+rsi]
        cmp     rbx,r10
        jae     NEAR $L$in_block_prologue

        mov     rax,QWORD[24+rax]

        mov     rbx,QWORD[((-8))+rax]
        mov     rbp,QWORD[((-16))+rax]
        mov     r12,QWORD[((-24))+rax]
        mov     r13,QWORD[((-32))+rax]
        mov     r14,QWORD[((-40))+rax]
        mov     r15,QWORD[((-48))+rax]
        mov     QWORD[144+r8],rbx
        mov     QWORD[160+r8],rbp
        mov     QWORD[216+r8],r12
        mov     QWORD[224+r8],r13
        mov     QWORD[232+r8],r14
        mov     QWORD[240+r8],r15

$L$in_block_prologue:
        mov     rdi,QWORD[8+rax]
        mov     rsi,QWORD[16+rax]
        mov     QWORD[152+r8],rax
        mov     QWORD[168+r8],rsi
        mov     QWORD[176+r8],rdi

        jmp     NEAR $L$common_seh_exit



ALIGN   16
key_se_handler:
        push    rsi
        push    rdi
        push    rbx
        push    rbp
        push    r12
        push    r13
        push    r14
        push    r15
        pushfq
        sub     rsp,64

        mov     rax,QWORD[120+r8]
        mov     rbx,QWORD[248+r8]

        mov     rsi,QWORD[8+r9]
        mov     r11,QWORD[56+r9]

        mov     r10d,DWORD[r11]
        lea     r10,[r10*1+rsi]
        cmp     rbx,r10
        jb      NEAR $L$in_key_prologue

        mov     rax,QWORD[152+r8]

        mov     r10d,DWORD[4+r11]
        lea     r10,[r10*1+rsi]
        cmp     rbx,r10
        jae     NEAR $L$in_key_prologue

        lea     rax,[56+rax]

        mov     rbx,QWORD[((-8))+rax]
        mov     rbp,QWORD[((-16))+rax]
        mov     r12,QWORD[((-24))+rax]
        mov     r13,QWORD[((-32))+rax]
        mov     r14,QWORD[((-40))+rax]
        mov     r15,QWORD[((-48))+rax]
        mov     QWORD[144+r8],rbx
        mov     QWORD[160+r8],rbp
        mov     QWORD[216+r8],r12
        mov     QWORD[224+r8],r13
        mov     QWORD[232+r8],r14
        mov     QWORD[240+r8],r15

$L$in_key_prologue:
        mov     rdi,QWORD[8+rax]
        mov     rsi,QWORD[16+rax]
        mov     QWORD[152+r8],rax
        mov     QWORD[168+r8],rsi
        mov     QWORD[176+r8],rdi

        jmp     NEAR $L$common_seh_exit



ALIGN   16
cbc_se_handler:
        push    rsi
        push    rdi
        push    rbx
        push    rbp
        push    r12
        push    r13
        push    r14
        push    r15
        pushfq
        sub     rsp,64

        mov     rax,QWORD[120+r8]
        mov     rbx,QWORD[248+r8]

        lea     r10,[$L$cbc_prologue]
        cmp     rbx,r10
        jb      NEAR $L$in_cbc_prologue

        lea     r10,[$L$cbc_fast_body]
        cmp     rbx,r10
        jb      NEAR $L$in_cbc_frame_setup

        lea     r10,[$L$cbc_slow_prologue]
        cmp     rbx,r10
        jb      NEAR $L$in_cbc_body

        lea     r10,[$L$cbc_slow_body]
        cmp     rbx,r10
        jb      NEAR $L$in_cbc_frame_setup

$L$in_cbc_body:
        mov     rax,QWORD[152+r8]

        lea     r10,[$L$cbc_epilogue]
        cmp     rbx,r10
        jae     NEAR $L$in_cbc_prologue

        lea     rax,[8+rax]

        lea     r10,[$L$cbc_popfq]
        cmp     rbx,r10
        jae     NEAR $L$in_cbc_prologue

        mov     rax,QWORD[8+rax]
        lea     rax,[56+rax]

$L$in_cbc_frame_setup:
        mov     rbx,QWORD[((-16))+rax]
        mov     rbp,QWORD[((-24))+rax]
        mov     r12,QWORD[((-32))+rax]
        mov     r13,QWORD[((-40))+rax]
        mov     r14,QWORD[((-48))+rax]
        mov     r15,QWORD[((-56))+rax]
        mov     QWORD[144+r8],rbx
        mov     QWORD[160+r8],rbp
        mov     QWORD[216+r8],r12
        mov     QWORD[224+r8],r13
        mov     QWORD[232+r8],r14
        mov     QWORD[240+r8],r15

$L$in_cbc_prologue:
        mov     rdi,QWORD[8+rax]
        mov     rsi,QWORD[16+rax]
        mov     QWORD[152+r8],rax
        mov     QWORD[168+r8],rsi
        mov     QWORD[176+r8],rdi

$L$common_seh_exit:

        mov     rdi,QWORD[40+r9]
        mov     rsi,r8
        mov     ecx,154
        DD      0xa548f3fc

        mov     rsi,r9
        xor     rcx,rcx
        mov     rdx,QWORD[8+rsi]
        mov     r8,QWORD[rsi]
        mov     r9,QWORD[16+rsi]
        mov     r10,QWORD[40+rsi]
        lea     r11,[56+rsi]
        lea     r12,[24+rsi]
        mov     QWORD[32+rsp],r10
        mov     QWORD[40+rsp],r11
        mov     QWORD[48+rsp],r12
        mov     QWORD[56+rsp],rcx
        call    QWORD[__imp_RtlVirtualUnwind]

        mov     eax,1
        add     rsp,64
        popfq
        pop     r15
        pop     r14
        pop     r13
        pop     r12
        pop     rbp
        pop     rbx
        pop     rdi
        pop     rsi
        DB      0F3h,0C3h               ;repret


section .pdata rdata align=4
ALIGN   4
        DD      $L$SEH_begin_AES_encrypt wrt ..imagebase
        DD      $L$SEH_end_AES_encrypt wrt ..imagebase
        DD      $L$SEH_info_AES_encrypt wrt ..imagebase

        DD      $L$SEH_begin_AES_decrypt wrt ..imagebase
        DD      $L$SEH_end_AES_decrypt wrt ..imagebase
        DD      $L$SEH_info_AES_decrypt wrt ..imagebase

        DD      $L$SEH_begin_AES_set_encrypt_key wrt ..imagebase
        DD      $L$SEH_end_AES_set_encrypt_key wrt ..imagebase
        DD      $L$SEH_info_AES_set_encrypt_key wrt ..imagebase

        DD      $L$SEH_begin_AES_set_decrypt_key wrt ..imagebase
        DD      $L$SEH_end_AES_set_decrypt_key wrt ..imagebase
        DD      $L$SEH_info_AES_set_decrypt_key wrt ..imagebase

        DD      $L$SEH_begin_AES_cbc_encrypt wrt ..imagebase
        DD      $L$SEH_end_AES_cbc_encrypt wrt ..imagebase
        DD      $L$SEH_info_AES_cbc_encrypt wrt ..imagebase

section .xdata rdata align=8
ALIGN   8
$L$SEH_info_AES_encrypt:
DB      9,0,0,0
        DD      block_se_handler wrt ..imagebase
        DD      $L$enc_prologue wrt ..imagebase,$L$enc_epilogue wrt ..imagebase
$L$SEH_info_AES_decrypt:
DB      9,0,0,0
        DD      block_se_handler wrt ..imagebase
        DD      $L$dec_prologue wrt ..imagebase,$L$dec_epilogue wrt ..imagebase
$L$SEH_info_AES_set_encrypt_key:
DB      9,0,0,0
        DD      key_se_handler wrt ..imagebase
        DD      $L$enc_key_prologue wrt ..imagebase,$L$enc_key_epilogue wrt ..imagebase
$L$SEH_info_AES_set_decrypt_key:
DB      9,0,0,0
        DD      key_se_handler wrt ..imagebase
        DD      $L$dec_key_prologue wrt ..imagebase,$L$dec_key_epilogue wrt ..imagebase
$L$SEH_info_AES_cbc_encrypt:
DB      9,0,0,0
        DD      cbc_se_handler wrt ..imagebase
