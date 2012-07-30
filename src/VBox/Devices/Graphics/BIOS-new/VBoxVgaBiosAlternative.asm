; $Id$ 
;; @file
; Auto Generated source file. Do not edit.
;

;
; Source file: vgarom.asm
;
;  ============================================================================================
;  
;   Copyright (C) 2001,2002 the LGPL VGABios developers Team
;  
;   This library is free software; you can redistribute it and/or
;   modify it under the terms of the GNU Lesser General Public
;   License as published by the Free Software Foundation; either
;   version 2 of the License, or (at your option) any later version.
;  
;   This library is distributed in the hope that it will be useful,
;   but WITHOUT ANY WARRANTY; without even the implied warranty of
;   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;   Lesser General Public License for more details.
;  
;   You should have received a copy of the GNU Lesser General Public
;   License along with this library; if not, write to the Free Software
;   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
;  
;  ============================================================================================
;  
;   This VGA Bios is specific to the plex86/bochs Emulated VGA card.
;   You can NOT drive any physical vga card with it.
;  
;  ============================================================================================
;  

;
; Source file: vberom.asm
;
;  ============================================================================================
;  
;   Copyright (C) 2002 Jeroen Janssen
;  
;   This library is free software; you can redistribute it and/or
;   modify it under the terms of the GNU Lesser General Public
;   License as published by the Free Software Foundation; either
;   version 2 of the License, or (at your option) any later version.
;  
;   This library is distributed in the hope that it will be useful,
;   but WITHOUT ANY WARRANTY; without even the implied warranty of
;   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;   Lesser General Public License for more details.
;  
;   You should have received a copy of the GNU Lesser General Public
;   License along with this library; if not, write to the Free Software
;   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
;  
;  ============================================================================================
;  
;   This VBE is part of the VGA Bios specific to the plex86/bochs Emulated VGA card.
;   You can NOT drive any physical vga card with it.
;  
;  ============================================================================================
;  
;   This VBE Bios is based on information taken from :
;    - VESA BIOS EXTENSION (VBE) Core Functions Standard Version 3.0 located at www.vesa.org
;  
;  ============================================================================================

;
; Source file: vgabios.c
;
;  // ============================================================================================
;  
;  vgabios.c
;  
;  // ============================================================================================
;  //
;  //  Copyright (C) 2001,2002 the LGPL VGABios developers Team
;  //
;  //  This library is free software; you can redistribute it and/or
;  //  modify it under the terms of the GNU Lesser General Public
;  //  License as published by the Free Software Foundation; either
;  //  version 2 of the License, or (at your option) any later version.
;  //
;  //  This library is distributed in the hope that it will be useful,
;  //  but WITHOUT ANY WARRANTY; without even the implied warranty of
;  //  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;  //  Lesser General Public License for more details.
;  //
;  //  You should have received a copy of the GNU Lesser General Public
;  //  License along with this library; if not, write to the Free Software
;  //  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
;  //
;  // ============================================================================================
;  //
;  //  This VGA Bios is specific to the plex86/bochs Emulated VGA card.
;  //  You can NOT drive any physical vga card with it.
;  //
;  // ============================================================================================
;  //
;  //  This file contains code ripped from :
;  //   - rombios.c of plex86
;  //
;  //  This VGA Bios contains fonts from :
;  //   - fntcol16.zip (c) by Joseph Gil avalable at :
;  //      ftp://ftp.simtel.net/pub/simtelnet/msdos/screen/fntcol16.zip
;  //     These fonts are public domain
;  //
;  //  This VGA Bios is based on information taken from :
;  //   - Kevin Lawton's vga card emulation for bochs/plex86
;  //   - Ralf Brown's interrupts list available at http://www.cs.cmu.edu/afs/cs/user/ralf/pub/WWW/files.html
;  //   - Finn Thogersons' VGADOC4b available at http://home.worldonline.dk/~finth/
;  //   - Michael Abrash's Graphics Programming Black Book
;  //   - Francois Gervais' book "programmation des cartes graphiques cga-ega-vga" edited by sybex
;  //   - DOSEMU 1.0.1 source code for several tables values and formulas
;  //
;  // Thanks for patches, comments and ideas to :
;  //   - techt@pikeonline.net
;  //
;  // ============================================================================================
;  #include "vgabios.h"
;  #include <inttypes.h>

;
; Source file: vbe.c
;
;  // ============================================================================================
;  //
;  //  Copyright (C) 2002 Jeroen Janssen
;  //
;  //  This library is free software; you can redistribute it and/or
;  //  modify it under the terms of the GNU Lesser General Public
;  //  License as published by the Free Software Foundation; either
;  //  version 2 of the License, or (at your option) any later version.
;  //
;  //  This library is distributed in the hope that it will be useful,
;  //  but WITHOUT ANY WARRANTY; without even the implied warranty of
;  //  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;  //  Lesser General Public License for more details.
;  //
;  //  You should have received a copy of the GNU Lesser General Public
;  //  License along with this library; if not, write to the Free Software
;  //  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
;  //
;  // ============================================================================================
;  //
;  //  This VBE is part of the VGA Bios specific to the plex86/bochs Emulated VGA card.
;  //  You can NOT drive any physical vga card with it.
;  //
;  // ============================================================================================
;  //
;  //  This VBE Bios is based on information taken from :
;  //   - VESA BIOS EXTENSION (VBE) Core Functions Standard Version 3.0 located at www.vesa.org
;  //
;  // ============================================================================================




section VGAROM progbits vstart=0x0 align=1 ; size=0x993 class=CODE group=AUTO
    db  055h, 0aah, 040h, 0e9h, 068h, 00ah, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 049h, 042h
    db  04dh, 000h
vgabios_int10_handler:                       ; 0xc0022 LB 0x5b5
    pushfw                                    ; 9c
    cmp ah, 00fh                              ; 80 fc 0f
    jne short 0002eh                          ; 75 06
    call 00175h                               ; e8 4a 01
    jmp near 000f3h                           ; e9 c5 00
    cmp ah, 01ah                              ; 80 fc 1a
    jne short 00039h                          ; 75 06
    call 0052ah                               ; e8 f4 04
    jmp near 000f3h                           ; e9 ba 00
    cmp ah, 00bh                              ; 80 fc 0b
    jne short 00044h                          ; 75 06
    call 000f5h                               ; e8 b4 00
    jmp near 000f3h                           ; e9 af 00
    cmp ax, 01103h                            ; 3d 03 11
    jne short 0004fh                          ; 75 06
    call 00421h                               ; e8 d5 03
    jmp near 000f3h                           ; e9 a4 00
    cmp ah, 012h                              ; 80 fc 12
    jne short 00092h                          ; 75 3e
    cmp bl, 010h                              ; 80 fb 10
    jne short 0005fh                          ; 75 06
    call 0042eh                               ; e8 d2 03
    jmp near 000f3h                           ; e9 94 00
    cmp bl, 030h                              ; 80 fb 30
    jne short 0006ah                          ; 75 06
    call 00451h                               ; e8 ea 03
    jmp near 000f3h                           ; e9 89 00
    cmp bl, 031h                              ; 80 fb 31
    jne short 00074h                          ; 75 05
    call 004a4h                               ; e8 32 04
    jmp short 000f3h                          ; eb 7f
    cmp bl, 032h                              ; 80 fb 32
    jne short 0007eh                          ; 75 05
    call 004c6h                               ; e8 4a 04
    jmp short 000f3h                          ; eb 75
    cmp bl, 033h                              ; 80 fb 33
    jne short 00088h                          ; 75 05
    call 004e4h                               ; e8 5e 04
    jmp short 000f3h                          ; eb 6b
    cmp bl, 034h                              ; 80 fb 34
    jne short 000e5h                          ; 75 58
    call 00508h                               ; e8 78 04
    jmp short 000f3h                          ; eb 61
    cmp ax, 0101bh                            ; 3d 1b 10
    je short 000e5h                           ; 74 4e
    cmp ah, 010h                              ; 80 fc 10
    jne short 000a1h                          ; 75 05
    call 0019ch                               ; e8 fd 00
    jmp short 000f3h                          ; eb 52
    cmp ah, 04fh                              ; 80 fc 4f
    jne short 000e5h                          ; 75 3f
    cmp AL, strict byte 003h                  ; 3c 03
    jne short 000afh                          ; 75 05
    call 0080ch                               ; e8 5f 07
    jmp short 000f3h                          ; eb 44
    cmp AL, strict byte 005h                  ; 3c 05
    jne short 000b8h                          ; 75 05
    call 00831h                               ; e8 7b 07
    jmp short 000f3h                          ; eb 3b
    cmp AL, strict byte 006h                  ; 3c 06
    jne short 000c1h                          ; 75 05
    call 0085eh                               ; e8 9f 07
    jmp short 000f3h                          ; eb 32
    cmp AL, strict byte 007h                  ; 3c 07
    jne short 000cah                          ; 75 05
    call 008abh                               ; e8 e3 07
    jmp short 000f3h                          ; eb 29
    cmp AL, strict byte 008h                  ; 3c 08
    jne short 000d3h                          ; 75 05
    call 008dfh                               ; e8 0e 08
    jmp short 000f3h                          ; eb 20
    cmp AL, strict byte 009h                  ; 3c 09
    jne short 000dch                          ; 75 05
    call 00916h                               ; e8 3c 08
    jmp short 000f3h                          ; eb 17
    cmp AL, strict byte 00ah                  ; 3c 0a
    jne short 000e5h                          ; 75 05
    call 0097ah                               ; e8 97 08
    jmp short 000f3h                          ; eb 0e
    push ES                                   ; 06
    push DS                                   ; 1e
    pushaw                                    ; 60
    mov bx, 0c000h                            ; bb 00 c0
    mov ds, bx                                ; 8e db
    call 0305ch                               ; e8 6c 2f
    popaw                                     ; 61
    pop DS                                    ; 1f
    pop ES                                    ; 07
    popfw                                     ; 9d
    iret                                      ; cf
    cmp bh, 000h                              ; 80 ff 00
    je short 00100h                           ; 74 06
    cmp bh, 001h                              ; 80 ff 01
    je short 00143h                           ; 74 44
    retn                                      ; c3
    push ax                                   ; 50
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    mov dx, 003dah                            ; ba da 03
    in AL, DX                                 ; ec
    mov dx, 003c0h                            ; ba c0 03
    mov AL, strict byte 000h                  ; b0 00
    out DX, AL                                ; ee
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3
    and AL, strict byte 00fh                  ; 24 0f
    test AL, strict byte 008h                 ; a8 08
    je short 00118h                           ; 74 02
    add AL, strict byte 008h                  ; 04 08
    out DX, AL                                ; ee
    mov CL, strict byte 001h                  ; b1 01
    and bl, 010h                              ; 80 e3 10
    mov dx, 003c0h                            ; ba c0 03
    db  08ah, 0c1h
    ; mov al, cl                                ; 8a c1
    out DX, AL                                ; ee
    mov dx, 003c1h                            ; ba c1 03
    in AL, DX                                 ; ec
    and AL, strict byte 0efh                  ; 24 ef
    db  00ah, 0c3h
    ; or al, bl                                 ; 0a c3
    mov dx, 003c0h                            ; ba c0 03
    out DX, AL                                ; ee
    db  0feh, 0c1h
    ; inc cl                                    ; fe c1
    cmp cl, 004h                              ; 80 f9 04
    jne short 0011eh                          ; 75 e7
    mov AL, strict byte 020h                  ; b0 20
    out DX, AL                                ; ee
    mov dx, 003dah                            ; ba da 03
    in AL, DX                                 ; ec
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    pop ax                                    ; 58
    retn                                      ; c3
    push ax                                   ; 50
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    mov dx, 003dah                            ; ba da 03
    in AL, DX                                 ; ec
    mov CL, strict byte 001h                  ; b1 01
    and bl, 001h                              ; 80 e3 01
    mov dx, 003c0h                            ; ba c0 03
    db  08ah, 0c1h
    ; mov al, cl                                ; 8a c1
    out DX, AL                                ; ee
    mov dx, 003c1h                            ; ba c1 03
    in AL, DX                                 ; ec
    and AL, strict byte 0feh                  ; 24 fe
    db  00ah, 0c3h
    ; or al, bl                                 ; 0a c3
    mov dx, 003c0h                            ; ba c0 03
    out DX, AL                                ; ee
    db  0feh, 0c1h
    ; inc cl                                    ; fe c1
    cmp cl, 004h                              ; 80 f9 04
    jne short 00150h                          ; 75 e7
    mov AL, strict byte 020h                  ; b0 20
    out DX, AL                                ; ee
    mov dx, 003dah                            ; ba da 03
    in AL, DX                                 ; ec
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    pop ax                                    ; 58
    retn                                      ; c3
    push DS                                   ; 1e
    mov ax, strict word 00040h                ; b8 40 00
    mov ds, ax                                ; 8e d8
    push bx                                   ; 53
    mov bx, strict word 00062h                ; bb 62 00
    mov al, byte [bx]                         ; 8a 07
    pop bx                                    ; 5b
    db  08ah, 0f8h
    ; mov bh, al                                ; 8a f8
    push bx                                   ; 53
    mov bx, 00087h                            ; bb 87 00
    mov ah, byte [bx]                         ; 8a 27
    and ah, 080h                              ; 80 e4 80
    mov bx, strict word 00049h                ; bb 49 00
    mov al, byte [bx]                         ; 8a 07
    db  00ah, 0c4h
    ; or al, ah                                 ; 0a c4
    mov bx, strict word 0004ah                ; bb 4a 00
    mov ah, byte [bx]                         ; 8a 27
    pop bx                                    ; 5b
    pop DS                                    ; 1f
    retn                                      ; c3
    cmp AL, strict byte 000h                  ; 3c 00
    jne short 001a2h                          ; 75 02
    jmp short 00203h                          ; eb 61
    cmp AL, strict byte 001h                  ; 3c 01
    jne short 001a8h                          ; 75 02
    jmp short 00221h                          ; eb 79
    cmp AL, strict byte 002h                  ; 3c 02
    jne short 001aeh                          ; 75 02
    jmp short 00229h                          ; eb 7b
    cmp AL, strict byte 003h                  ; 3c 03
    jne short 001b5h                          ; 75 03
    jmp near 0025ah                           ; e9 a5 00
    cmp AL, strict byte 007h                  ; 3c 07
    jne short 001bch                          ; 75 03
    jmp near 00284h                           ; e9 c8 00
    cmp AL, strict byte 008h                  ; 3c 08
    jne short 001c3h                          ; 75 03
    jmp near 002ach                           ; e9 e9 00
    cmp AL, strict byte 009h                  ; 3c 09
    jne short 001cah                          ; 75 03
    jmp near 002bah                           ; e9 f0 00
    cmp AL, strict byte 010h                  ; 3c 10
    jne short 001d1h                          ; 75 03
    jmp near 002ffh                           ; e9 2e 01
    cmp AL, strict byte 012h                  ; 3c 12
    jne short 001d8h                          ; 75 03
    jmp near 00318h                           ; e9 40 01
    cmp AL, strict byte 013h                  ; 3c 13
    jne short 001dfh                          ; 75 03
    jmp near 00340h                           ; e9 61 01
    cmp AL, strict byte 015h                  ; 3c 15
    jne short 001e6h                          ; 75 03
    jmp near 00387h                           ; e9 a1 01
    cmp AL, strict byte 017h                  ; 3c 17
    jne short 001edh                          ; 75 03
    jmp near 003a2h                           ; e9 b5 01
    cmp AL, strict byte 018h                  ; 3c 18
    jne short 001f4h                          ; 75 03
    jmp near 003cah                           ; e9 d6 01
    cmp AL, strict byte 019h                  ; 3c 19
    jne short 001fbh                          ; 75 03
    jmp near 003d5h                           ; e9 da 01
    cmp AL, strict byte 01ah                  ; 3c 1a
    jne short 00202h                          ; 75 03
    jmp near 003e0h                           ; e9 de 01
    retn                                      ; c3
    cmp bl, 014h                              ; 80 fb 14
    jnbe short 00220h                         ; 77 18
    push ax                                   ; 50
    push dx                                   ; 52
    mov dx, 003dah                            ; ba da 03
    in AL, DX                                 ; ec
    mov dx, 003c0h                            ; ba c0 03
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3
    out DX, AL                                ; ee
    db  08ah, 0c7h
    ; mov al, bh                                ; 8a c7
    out DX, AL                                ; ee
    mov AL, strict byte 020h                  ; b0 20
    out DX, AL                                ; ee
    mov dx, 003dah                            ; ba da 03
    in AL, DX                                 ; ec
    pop dx                                    ; 5a
    pop ax                                    ; 58
    retn                                      ; c3
    push bx                                   ; 53
    mov BL, strict byte 011h                  ; b3 11
    call 00203h                               ; e8 dc ff
    pop bx                                    ; 5b
    retn                                      ; c3
    push ax                                   ; 50
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    db  08bh, 0dah
    ; mov bx, dx                                ; 8b da
    mov dx, 003dah                            ; ba da 03
    in AL, DX                                 ; ec
    mov CL, strict byte 000h                  ; b1 00
    mov dx, 003c0h                            ; ba c0 03
    db  08ah, 0c1h
    ; mov al, cl                                ; 8a c1
    out DX, AL                                ; ee
    mov al, byte [es:bx]                      ; 26 8a 07
    out DX, AL                                ; ee
    inc bx                                    ; 43
    db  0feh, 0c1h
    ; inc cl                                    ; fe c1
    cmp cl, 010h                              ; 80 f9 10
    jne short 00238h                          ; 75 f1
    mov AL, strict byte 011h                  ; b0 11
    out DX, AL                                ; ee
    mov al, byte [es:bx]                      ; 26 8a 07
    out DX, AL                                ; ee
    mov AL, strict byte 020h                  ; b0 20
    out DX, AL                                ; ee
    mov dx, 003dah                            ; ba da 03
    in AL, DX                                 ; ec
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    pop ax                                    ; 58
    retn                                      ; c3
    push ax                                   ; 50
    push bx                                   ; 53
    push dx                                   ; 52
    mov dx, 003dah                            ; ba da 03
    in AL, DX                                 ; ec
    mov dx, 003c0h                            ; ba c0 03
    mov AL, strict byte 010h                  ; b0 10
    out DX, AL                                ; ee
    mov dx, 003c1h                            ; ba c1 03
    in AL, DX                                 ; ec
    and AL, strict byte 0f7h                  ; 24 f7
    and bl, 001h                              ; 80 e3 01
    sal bl, 003h                              ; c0 e3 03
    db  00ah, 0c3h
    ; or al, bl                                 ; 0a c3
    mov dx, 003c0h                            ; ba c0 03
    out DX, AL                                ; ee
    mov AL, strict byte 020h                  ; b0 20
    out DX, AL                                ; ee
    mov dx, 003dah                            ; ba da 03
    in AL, DX                                 ; ec
    pop dx                                    ; 5a
    pop bx                                    ; 5b
    pop ax                                    ; 58
    retn                                      ; c3
    cmp bl, 014h                              ; 80 fb 14
    jnbe short 002abh                         ; 77 22
    push ax                                   ; 50
    push dx                                   ; 52
    mov dx, 003dah                            ; ba da 03
    in AL, DX                                 ; ec
    mov dx, 003c0h                            ; ba c0 03
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3
    out DX, AL                                ; ee
    mov dx, 003c1h                            ; ba c1 03
    in AL, DX                                 ; ec
    db  08ah, 0f8h
    ; mov bh, al                                ; 8a f8
    mov dx, 003dah                            ; ba da 03
    in AL, DX                                 ; ec
    mov dx, 003c0h                            ; ba c0 03
    mov AL, strict byte 020h                  ; b0 20
    out DX, AL                                ; ee
    mov dx, 003dah                            ; ba da 03
    in AL, DX                                 ; ec
    pop dx                                    ; 5a
    pop ax                                    ; 58
    retn                                      ; c3
    push ax                                   ; 50
    push bx                                   ; 53
    mov BL, strict byte 011h                  ; b3 11
    call 00284h                               ; e8 d1 ff
    db  08ah, 0c7h
    ; mov al, bh                                ; 8a c7
    pop bx                                    ; 5b
    db  08ah, 0f8h
    ; mov bh, al                                ; 8a f8
    pop ax                                    ; 58
    retn                                      ; c3
    push ax                                   ; 50
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    db  08bh, 0dah
    ; mov bx, dx                                ; 8b da
    mov CL, strict byte 000h                  ; b1 00
    mov dx, 003dah                            ; ba da 03
    in AL, DX                                 ; ec
    mov dx, 003c0h                            ; ba c0 03
    db  08ah, 0c1h
    ; mov al, cl                                ; 8a c1
    out DX, AL                                ; ee
    mov dx, 003c1h                            ; ba c1 03
    in AL, DX                                 ; ec
    mov byte [es:bx], al                      ; 26 88 07
    inc bx                                    ; 43
    db  0feh, 0c1h
    ; inc cl                                    ; fe c1
    cmp cl, 010h                              ; 80 f9 10
    jne short 002c2h                          ; 75 e7
    mov dx, 003dah                            ; ba da 03
    in AL, DX                                 ; ec
    mov dx, 003c0h                            ; ba c0 03
    mov AL, strict byte 011h                  ; b0 11
    out DX, AL                                ; ee
    mov dx, 003c1h                            ; ba c1 03
    in AL, DX                                 ; ec
    mov byte [es:bx], al                      ; 26 88 07
    mov dx, 003dah                            ; ba da 03
    in AL, DX                                 ; ec
    mov dx, 003c0h                            ; ba c0 03
    mov AL, strict byte 020h                  ; b0 20
    out DX, AL                                ; ee
    mov dx, 003dah                            ; ba da 03
    in AL, DX                                 ; ec
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    pop ax                                    ; 58
    retn                                      ; c3
    push ax                                   ; 50
    push dx                                   ; 52
    mov dx, 003c8h                            ; ba c8 03
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3
    out DX, AL                                ; ee
    mov dx, 003c9h                            ; ba c9 03
    pop ax                                    ; 58
    push ax                                   ; 50
    db  08ah, 0c4h
    ; mov al, ah                                ; 8a c4
    out DX, AL                                ; ee
    db  08ah, 0c5h
    ; mov al, ch                                ; 8a c5
    out DX, AL                                ; ee
    db  08ah, 0c1h
    ; mov al, cl                                ; 8a c1
    out DX, AL                                ; ee
    pop dx                                    ; 5a
    pop ax                                    ; 58
    retn                                      ; c3
    push ax                                   ; 50
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    mov dx, 003c8h                            ; ba c8 03
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3
    out DX, AL                                ; ee
    pop dx                                    ; 5a
    push dx                                   ; 52
    db  08bh, 0dah
    ; mov bx, dx                                ; 8b da
    mov dx, 003c9h                            ; ba c9 03
    mov al, byte [es:bx]                      ; 26 8a 07
    out DX, AL                                ; ee
    inc bx                                    ; 43
    mov al, byte [es:bx]                      ; 26 8a 07
    out DX, AL                                ; ee
    inc bx                                    ; 43
    mov al, byte [es:bx]                      ; 26 8a 07
    out DX, AL                                ; ee
    inc bx                                    ; 43
    dec cx                                    ; 49
    jne short 00329h                          ; 75 ee
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    pop ax                                    ; 58
    retn                                      ; c3
    push ax                                   ; 50
    push bx                                   ; 53
    push dx                                   ; 52
    mov dx, 003dah                            ; ba da 03
    in AL, DX                                 ; ec
    mov dx, 003c0h                            ; ba c0 03
    mov AL, strict byte 010h                  ; b0 10
    out DX, AL                                ; ee
    mov dx, 003c1h                            ; ba c1 03
    in AL, DX                                 ; ec
    and bl, 001h                              ; 80 e3 01
    jne short 00363h                          ; 75 0d
    and AL, strict byte 07fh                  ; 24 7f
    sal bh, 007h                              ; c0 e7 07
    db  00ah, 0c7h
    ; or al, bh                                 ; 0a c7
    mov dx, 003c0h                            ; ba c0 03
    out DX, AL                                ; ee
    jmp short 0037ch                          ; eb 19
    push ax                                   ; 50
    mov dx, 003dah                            ; ba da 03
    in AL, DX                                 ; ec
    mov dx, 003c0h                            ; ba c0 03
    mov AL, strict byte 014h                  ; b0 14
    out DX, AL                                ; ee
    pop ax                                    ; 58
    and AL, strict byte 080h                  ; 24 80
    jne short 00376h                          ; 75 03
    sal bh, 002h                              ; c0 e7 02
    and bh, 00fh                              ; 80 e7 0f
    db  08ah, 0c7h
    ; mov al, bh                                ; 8a c7
    out DX, AL                                ; ee
    mov AL, strict byte 020h                  ; b0 20
    out DX, AL                                ; ee
    mov dx, 003dah                            ; ba da 03
    in AL, DX                                 ; ec
    pop dx                                    ; 5a
    pop bx                                    ; 5b
    pop ax                                    ; 58
    retn                                      ; c3
    push ax                                   ; 50
    push dx                                   ; 52
    mov dx, 003c7h                            ; ba c7 03
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3
    out DX, AL                                ; ee
    pop ax                                    ; 58
    db  08ah, 0e0h
    ; mov ah, al                                ; 8a e0
    mov dx, 003c9h                            ; ba c9 03
    in AL, DX                                 ; ec
    xchg al, ah                               ; 86 e0
    push ax                                   ; 50
    in AL, DX                                 ; ec
    db  08ah, 0e8h
    ; mov ch, al                                ; 8a e8
    in AL, DX                                 ; ec
    db  08ah, 0c8h
    ; mov cl, al                                ; 8a c8
    pop dx                                    ; 5a
    pop ax                                    ; 58
    retn                                      ; c3
    push ax                                   ; 50
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    mov dx, 003c7h                            ; ba c7 03
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3
    out DX, AL                                ; ee
    pop dx                                    ; 5a
    push dx                                   ; 52
    db  08bh, 0dah
    ; mov bx, dx                                ; 8b da
    mov dx, 003c9h                            ; ba c9 03
    in AL, DX                                 ; ec
    mov byte [es:bx], al                      ; 26 88 07
    inc bx                                    ; 43
    in AL, DX                                 ; ec
    mov byte [es:bx], al                      ; 26 88 07
    inc bx                                    ; 43
    in AL, DX                                 ; ec
    mov byte [es:bx], al                      ; 26 88 07
    inc bx                                    ; 43
    dec cx                                    ; 49
    jne short 003b3h                          ; 75 ee
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    pop ax                                    ; 58
    retn                                      ; c3
    push ax                                   ; 50
    push dx                                   ; 52
    mov dx, 003c6h                            ; ba c6 03
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3
    out DX, AL                                ; ee
    pop dx                                    ; 5a
    pop ax                                    ; 58
    retn                                      ; c3
    push ax                                   ; 50
    push dx                                   ; 52
    mov dx, 003c6h                            ; ba c6 03
    in AL, DX                                 ; ec
    db  08ah, 0d8h
    ; mov bl, al                                ; 8a d8
    pop dx                                    ; 5a
    pop ax                                    ; 58
    retn                                      ; c3
    push ax                                   ; 50
    push dx                                   ; 52
    mov dx, 003dah                            ; ba da 03
    in AL, DX                                 ; ec
    mov dx, 003c0h                            ; ba c0 03
    mov AL, strict byte 010h                  ; b0 10
    out DX, AL                                ; ee
    mov dx, 003c1h                            ; ba c1 03
    in AL, DX                                 ; ec
    db  08ah, 0d8h
    ; mov bl, al                                ; 8a d8
    shr bl, 007h                              ; c0 eb 07
    mov dx, 003dah                            ; ba da 03
    in AL, DX                                 ; ec
    mov dx, 003c0h                            ; ba c0 03
    mov AL, strict byte 014h                  ; b0 14
    out DX, AL                                ; ee
    mov dx, 003c1h                            ; ba c1 03
    in AL, DX                                 ; ec
    db  08ah, 0f8h
    ; mov bh, al                                ; 8a f8
    and bh, 00fh                              ; 80 e7 0f
    test bl, 001h                             ; f6 c3 01
    jne short 00410h                          ; 75 03
    shr bh, 002h                              ; c0 ef 02
    mov dx, 003dah                            ; ba da 03
    in AL, DX                                 ; ec
    mov dx, 003c0h                            ; ba c0 03
    mov AL, strict byte 020h                  ; b0 20
    out DX, AL                                ; ee
    mov dx, 003dah                            ; ba da 03
    in AL, DX                                 ; ec
    pop dx                                    ; 5a
    pop ax                                    ; 58
    retn                                      ; c3
    push ax                                   ; 50
    push dx                                   ; 52
    mov dx, 003c4h                            ; ba c4 03
    db  08ah, 0e3h
    ; mov ah, bl                                ; 8a e3
    mov AL, strict byte 003h                  ; b0 03
    out DX, ax                                ; ef
    pop dx                                    ; 5a
    pop ax                                    ; 58
    retn                                      ; c3
    push DS                                   ; 1e
    push ax                                   ; 50
    mov ax, strict word 00040h                ; b8 40 00
    mov ds, ax                                ; 8e d8
    db  032h, 0edh
    ; xor ch, ch                                ; 32 ed
    mov bx, 00088h                            ; bb 88 00
    mov cl, byte [bx]                         ; 8a 0f
    and cl, 00fh                              ; 80 e1 0f
    mov bx, strict word 00063h                ; bb 63 00
    mov ax, word [bx]                         ; 8b 07
    mov bx, strict word 00003h                ; bb 03 00
    cmp ax, 003b4h                            ; 3d b4 03
    jne short 0044eh                          ; 75 02
    mov BH, strict byte 001h                  ; b7 01
    pop ax                                    ; 58
    pop DS                                    ; 1f
    retn                                      ; c3
    push DS                                   ; 1e
    push bx                                   ; 53
    push dx                                   ; 52
    db  08ah, 0d0h
    ; mov dl, al                                ; 8a d0
    mov ax, strict word 00040h                ; b8 40 00
    mov ds, ax                                ; 8e d8
    mov bx, 00089h                            ; bb 89 00
    mov al, byte [bx]                         ; 8a 07
    mov bx, 00088h                            ; bb 88 00
    mov ah, byte [bx]                         ; 8a 27
    cmp dl, 001h                              ; 80 fa 01
    je short 0047fh                           ; 74 15
    jc short 00489h                           ; 72 1d
    cmp dl, 002h                              ; 80 fa 02
    je short 00473h                           ; 74 02
    jmp short 0049dh                          ; eb 2a
    and AL, strict byte 07fh                  ; 24 7f
    or AL, strict byte 010h                   ; 0c 10
    and ah, 0f0h                              ; 80 e4 f0
    or ah, 009h                               ; 80 cc 09
    jne short 00493h                          ; 75 14
    and AL, strict byte 06fh                  ; 24 6f
    and ah, 0f0h                              ; 80 e4 f0
    or ah, 009h                               ; 80 cc 09
    jne short 00493h                          ; 75 0a
    and AL, strict byte 0efh                  ; 24 ef
    or AL, strict byte 080h                   ; 0c 80
    and ah, 0f0h                              ; 80 e4 f0
    or ah, 008h                               ; 80 cc 08
    mov bx, 00089h                            ; bb 89 00
    mov byte [bx], al                         ; 88 07
    mov bx, 00088h                            ; bb 88 00
    mov byte [bx], ah                         ; 88 27
    mov ax, 01212h                            ; b8 12 12
    pop dx                                    ; 5a
    pop bx                                    ; 5b
    pop DS                                    ; 1f
    retn                                      ; c3
    push DS                                   ; 1e
    push bx                                   ; 53
    push dx                                   ; 52
    db  08ah, 0d0h
    ; mov dl, al                                ; 8a d0
    and dl, 001h                              ; 80 e2 01
    sal dl, 003h                              ; c0 e2 03
    mov ax, strict word 00040h                ; b8 40 00
    mov ds, ax                                ; 8e d8
    mov bx, 00089h                            ; bb 89 00
    mov al, byte [bx]                         ; 8a 07
    and AL, strict byte 0f7h                  ; 24 f7
    db  00ah, 0c2h
    ; or al, dl                                 ; 0a c2
    mov byte [bx], al                         ; 88 07
    mov ax, 01212h                            ; b8 12 12
    pop dx                                    ; 5a
    pop bx                                    ; 5b
    pop DS                                    ; 1f
    retn                                      ; c3
    push bx                                   ; 53
    push dx                                   ; 52
    db  08ah, 0d8h
    ; mov bl, al                                ; 8a d8
    and bl, 001h                              ; 80 e3 01
    xor bl, 001h                              ; 80 f3 01
    sal bl, 1                                 ; d0 e3
    mov dx, 003cch                            ; ba cc 03
    in AL, DX                                 ; ec
    and AL, strict byte 0fdh                  ; 24 fd
    db  00ah, 0c3h
    ; or al, bl                                 ; 0a c3
    mov dx, 003c2h                            ; ba c2 03
    out DX, AL                                ; ee
    mov ax, 01212h                            ; b8 12 12
    pop dx                                    ; 5a
    pop bx                                    ; 5b
    retn                                      ; c3
    push DS                                   ; 1e
    push bx                                   ; 53
    push dx                                   ; 52
    db  08ah, 0d0h
    ; mov dl, al                                ; 8a d0
    and dl, 001h                              ; 80 e2 01
    xor dl, 001h                              ; 80 f2 01
    sal dl, 1                                 ; d0 e2
    mov ax, strict word 00040h                ; b8 40 00
    mov ds, ax                                ; 8e d8
    mov bx, 00089h                            ; bb 89 00
    mov al, byte [bx]                         ; 8a 07
    and AL, strict byte 0fdh                  ; 24 fd
    db  00ah, 0c2h
    ; or al, dl                                 ; 0a c2
    mov byte [bx], al                         ; 88 07
    mov ax, 01212h                            ; b8 12 12
    pop dx                                    ; 5a
    pop bx                                    ; 5b
    pop DS                                    ; 1f
    retn                                      ; c3
    push DS                                   ; 1e
    push bx                                   ; 53
    push dx                                   ; 52
    db  08ah, 0d0h
    ; mov dl, al                                ; 8a d0
    and dl, 001h                              ; 80 e2 01
    xor dl, 001h                              ; 80 f2 01
    mov ax, strict word 00040h                ; b8 40 00
    mov ds, ax                                ; 8e d8
    mov bx, 00089h                            ; bb 89 00
    mov al, byte [bx]                         ; 8a 07
    and AL, strict byte 0feh                  ; 24 fe
    db  00ah, 0c2h
    ; or al, dl                                 ; 0a c2
    mov byte [bx], al                         ; 88 07
    mov ax, 01212h                            ; b8 12 12
    pop dx                                    ; 5a
    pop bx                                    ; 5b
    pop DS                                    ; 1f
    retn                                      ; c3
    cmp AL, strict byte 000h                  ; 3c 00
    je short 00533h                           ; 74 05
    cmp AL, strict byte 001h                  ; 3c 01
    je short 00548h                           ; 74 16
    retn                                      ; c3
    push DS                                   ; 1e
    push ax                                   ; 50
    mov ax, strict word 00040h                ; b8 40 00
    mov ds, ax                                ; 8e d8
    mov bx, 0008ah                            ; bb 8a 00
    mov al, byte [bx]                         ; 8a 07
    db  08ah, 0d8h
    ; mov bl, al                                ; 8a d8
    db  032h, 0ffh
    ; xor bh, bh                                ; 32 ff
    pop ax                                    ; 58
    db  08ah, 0c4h
    ; mov al, ah                                ; 8a c4
    pop DS                                    ; 1f
    retn                                      ; c3
    push DS                                   ; 1e
    push ax                                   ; 50
    push bx                                   ; 53
    mov ax, strict word 00040h                ; b8 40 00
    mov ds, ax                                ; 8e d8
    db  08bh, 0c3h
    ; mov ax, bx                                ; 8b c3
    mov bx, 0008ah                            ; bb 8a 00
    mov byte [bx], al                         ; 88 07
    pop bx                                    ; 5b
    pop ax                                    ; 58
    db  08ah, 0c4h
    ; mov al, ah                                ; 8a c4
    pop DS                                    ; 1f
    retn                                      ; c3
    add byte [bx+si], al                      ; 00 00
    add byte [bx+si+052h], dl                 ; 00 50 52
    mov dx, 003dah                            ; ba da 03
    in AL, DX                                 ; ec
    test AL, strict byte 008h                 ; a8 08
    je short 00565h                           ; 74 fb
    pop dx                                    ; 5a
    pop ax                                    ; 58
    retn                                      ; c3
    push ax                                   ; 50
    push dx                                   ; 52
    mov dx, 003dah                            ; ba da 03
    in AL, DX                                 ; ec
    test AL, strict byte 008h                 ; a8 08
    jne short 00572h                          ; 75 fb
    pop dx                                    ; 5a
    pop ax                                    ; 58
    retn                                      ; c3
    push dx                                   ; 52
    mov dx, 001ceh                            ; ba ce 01
    mov ax, strict word 00000h                ; b8 00 00
    out DX, ax                                ; ef
    mov dx, 001cfh                            ; ba cf 01
    in ax, DX                                 ; ed
    pop dx                                    ; 5a
    retn                                      ; c3
    push dx                                   ; 52
    push ax                                   ; 50
    mov dx, 001ceh                            ; ba ce 01
    mov ax, strict word 00000h                ; b8 00 00
    out DX, ax                                ; ef
    pop ax                                    ; 58
    mov dx, 001cfh                            ; ba cf 01
    out DX, ax                                ; ef
    pop dx                                    ; 5a
    retn                                      ; c3
    push dx                                   ; 52
    mov dx, 001ceh                            ; ba ce 01
    mov ax, strict word 00003h                ; b8 03 00
    out DX, ax                                ; ef
    mov dx, 001cfh                            ; ba cf 01
    in ax, DX                                 ; ed
    cmp AL, strict byte 004h                  ; 3c 04
    jbe short 005b3h                          ; 76 0b
    db  08ah, 0e0h
    ; mov ah, al                                ; 8a e0
    shr ah, 003h                              ; c0 ec 03
    test AL, strict byte 007h                 ; a8 07
    je short 005b3h                           ; 74 02
    db  0feh, 0c4h
    ; inc ah                                    ; fe c4
    pop dx                                    ; 5a
    retn                                      ; c3
    push dx                                   ; 52
    push bx                                   ; 53
    call 00609h                               ; e8 4f 00
    db  08bh, 0d8h
    ; mov bx, ax                                ; 8b d8
    or ax, strict byte 00002h                 ; 83 c8 02
    call 005f9h                               ; e8 37 00
    mov dx, 001ceh                            ; ba ce 01
    mov ax, strict word 00001h                ; b8 01 00
    out DX, ax                                ; ef
    mov dx, 001cfh                            ; ba cf 01
    in ax, DX                                 ; ed
    push ax                                   ; 50
    db  08bh, 0c3h
    ; mov ax, bx                                ; 8b c3
    call 005f9h                               ; e8 26 00
    pop ax                                    ; 58
    pop bx                                    ; 5b
    pop dx                                    ; 5a
    retn                                      ; c3
_dispi_get_max_bpp:                          ; 0xc05d7 LB 0x22
    push dx                                   ; 52
    push bx                                   ; 53
    call 00609h                               ; e8 2d 00
    db  08bh, 0d8h
    ; mov bx, ax                                ; 8b d8
    or ax, strict byte 00002h                 ; 83 c8 02
    call 005f9h                               ; e8 15 00
    mov dx, 001ceh                            ; ba ce 01
    mov ax, strict word 00003h                ; b8 03 00
    out DX, ax                                ; ef
    mov dx, 001cfh                            ; ba cf 01
    in ax, DX                                 ; ed
    push ax                                   ; 50
    db  08bh, 0c3h
    ; mov ax, bx                                ; 8b c3
    call 005f9h                               ; e8 04 00
    pop ax                                    ; 58
    pop bx                                    ; 5b
    pop dx                                    ; 5a
    retn                                      ; c3
dispi_set_enable_:                           ; 0xc05f9 LB 0x1e
    push dx                                   ; 52
    push ax                                   ; 50
    mov dx, 001ceh                            ; ba ce 01
    mov ax, strict word 00004h                ; b8 04 00
    out DX, ax                                ; ef
    pop ax                                    ; 58
    mov dx, 001cfh                            ; ba cf 01
    out DX, ax                                ; ef
    pop dx                                    ; 5a
    retn                                      ; c3
    push dx                                   ; 52
    mov dx, 001ceh                            ; ba ce 01
    mov ax, strict word 00004h                ; b8 04 00
    out DX, ax                                ; ef
    mov dx, 001cfh                            ; ba cf 01
    in ax, DX                                 ; ed
    pop dx                                    ; 5a
    retn                                      ; c3
dispi_set_bank_:                             ; 0xc0617 LB 0x1e
    push dx                                   ; 52
    push ax                                   ; 50
    mov dx, 001ceh                            ; ba ce 01
    mov ax, strict word 00005h                ; b8 05 00
    out DX, ax                                ; ef
    pop ax                                    ; 58
    mov dx, 001cfh                            ; ba cf 01
    out DX, ax                                ; ef
    pop dx                                    ; 5a
    retn                                      ; c3
    push dx                                   ; 52
    mov dx, 001ceh                            ; ba ce 01
    mov ax, strict word 00005h                ; b8 05 00
    out DX, ax                                ; ef
    mov dx, 001cfh                            ; ba cf 01
    in ax, DX                                 ; ed
    pop dx                                    ; 5a
    retn                                      ; c3
_dispi_set_bank_farcall:                     ; 0xc0635 LB 0xbe
    cmp bx, 00100h                            ; 81 fb 00 01
    je short 00659h                           ; 74 1e
    db  00bh, 0dbh
    ; or bx, bx                                 ; 0b db
    jne short 00667h                          ; 75 28
    db  08bh, 0c2h
    ; mov ax, dx                                ; 8b c2
    push dx                                   ; 52
    push ax                                   ; 50
    mov ax, strict word 00005h                ; b8 05 00
    mov dx, 001ceh                            ; ba ce 01
    out DX, ax                                ; ef
    pop ax                                    ; 58
    mov dx, 001cfh                            ; ba cf 01
    out DX, ax                                ; ef
    in ax, DX                                 ; ed
    pop dx                                    ; 5a
    db  03bh, 0d0h
    ; cmp dx, ax                                ; 3b d0
    jne short 00667h                          ; 75 12
    mov ax, strict word 0004fh                ; b8 4f 00
    retf                                      ; cb
    mov ax, strict word 00005h                ; b8 05 00
    mov dx, 001ceh                            ; ba ce 01
    out DX, ax                                ; ef
    mov dx, 001cfh                            ; ba cf 01
    in ax, DX                                 ; ed
    db  08bh, 0d0h
    ; mov dx, ax                                ; 8b d0
    retf                                      ; cb
    mov ax, 0014fh                            ; b8 4f 01
    retf                                      ; cb
    push dx                                   ; 52
    push ax                                   ; 50
    mov dx, 001ceh                            ; ba ce 01
    mov ax, strict word 00008h                ; b8 08 00
    out DX, ax                                ; ef
    pop ax                                    ; 58
    mov dx, 001cfh                            ; ba cf 01
    out DX, ax                                ; ef
    pop dx                                    ; 5a
    retn                                      ; c3
    push dx                                   ; 52
    mov dx, 001ceh                            ; ba ce 01
    mov ax, strict word 00008h                ; b8 08 00
    out DX, ax                                ; ef
    mov dx, 001cfh                            ; ba cf 01
    in ax, DX                                 ; ed
    pop dx                                    ; 5a
    retn                                      ; c3
    push dx                                   ; 52
    push ax                                   ; 50
    mov dx, 001ceh                            ; ba ce 01
    mov ax, strict word 00009h                ; b8 09 00
    out DX, ax                                ; ef
    pop ax                                    ; 58
    mov dx, 001cfh                            ; ba cf 01
    out DX, ax                                ; ef
    pop dx                                    ; 5a
    retn                                      ; c3
    push dx                                   ; 52
    mov dx, 001ceh                            ; ba ce 01
    mov ax, strict word 00009h                ; b8 09 00
    out DX, ax                                ; ef
    mov dx, 001cfh                            ; ba cf 01
    in ax, DX                                 ; ed
    pop dx                                    ; 5a
    retn                                      ; c3
    push ax                                   ; 50
    push bx                                   ; 53
    push dx                                   ; 52
    db  08bh, 0d8h
    ; mov bx, ax                                ; 8b d8
    call 00598h                               ; e8 e9 fe
    cmp AL, strict byte 004h                  ; 3c 04
    jnbe short 006b5h                         ; 77 02
    shr bx, 1                                 ; d1 eb
    shr bx, 003h                              ; c1 eb 03
    mov dx, 003d4h                            ; ba d4 03
    db  08ah, 0e3h
    ; mov ah, bl                                ; 8a e3
    mov AL, strict byte 013h                  ; b0 13
    out DX, ax                                ; ef
    pop dx                                    ; 5a
    pop bx                                    ; 5b
    pop ax                                    ; 58
    retn                                      ; c3
    call 006a7h                               ; e8 e0 ff
    push dx                                   ; 52
    push ax                                   ; 50
    mov dx, 001ceh                            ; ba ce 01
    mov ax, strict word 00006h                ; b8 06 00
    out DX, ax                                ; ef
    pop ax                                    ; 58
    mov dx, 001cfh                            ; ba cf 01
    out DX, ax                                ; ef
    pop dx                                    ; 5a
    retn                                      ; c3
    push dx                                   ; 52
    mov dx, 001ceh                            ; ba ce 01
    mov ax, strict word 00006h                ; b8 06 00
    out DX, ax                                ; ef
    mov dx, 001cfh                            ; ba cf 01
    in ax, DX                                 ; ed
    pop dx                                    ; 5a
    retn                                      ; c3
    push dx                                   ; 52
    mov dx, 001ceh                            ; ba ce 01
    mov ax, strict word 00007h                ; b8 07 00
    out DX, ax                                ; ef
    mov dx, 001cfh                            ; ba cf 01
    in ax, DX                                 ; ed
    pop dx                                    ; 5a
    retn                                      ; c3
_vga_compat_setup:                           ; 0xc06f3 LB 0xe1
    push ax                                   ; 50
    push dx                                   ; 52
    mov dx, 001ceh                            ; ba ce 01
    mov ax, strict word 00001h                ; b8 01 00
    out DX, ax                                ; ef
    mov dx, 001cfh                            ; ba cf 01
    in ax, DX                                 ; ed
    push ax                                   ; 50
    mov dx, 003d4h                            ; ba d4 03
    mov ax, strict word 00011h                ; b8 11 00
    out DX, ax                                ; ef
    pop ax                                    ; 58
    push ax                                   ; 50
    shr ax, 003h                              ; c1 e8 03
    dec ax                                    ; 48
    db  08ah, 0e0h
    ; mov ah, al                                ; 8a e0
    mov AL, strict byte 001h                  ; b0 01
    out DX, ax                                ; ef
    pop ax                                    ; 58
    call 006a7h                               ; e8 90 ff
    mov dx, 001ceh                            ; ba ce 01
    mov ax, strict word 00002h                ; b8 02 00
    out DX, ax                                ; ef
    mov dx, 001cfh                            ; ba cf 01
    in ax, DX                                 ; ed
    dec ax                                    ; 48
    push ax                                   ; 50
    mov dx, 003d4h                            ; ba d4 03
    db  08ah, 0e0h
    ; mov ah, al                                ; 8a e0
    mov AL, strict byte 012h                  ; b0 12
    out DX, ax                                ; ef
    pop ax                                    ; 58
    mov AL, strict byte 007h                  ; b0 07
    out DX, AL                                ; ee
    inc dx                                    ; 42
    in AL, DX                                 ; ec
    and AL, strict byte 0bdh                  ; 24 bd
    test ah, 001h                             ; f6 c4 01
    je short 0073bh                           ; 74 02
    or AL, strict byte 002h                   ; 0c 02
    test ah, 002h                             ; f6 c4 02
    je short 00742h                           ; 74 02
    or AL, strict byte 040h                   ; 0c 40
    out DX, AL                                ; ee
    mov dx, 003d4h                            ; ba d4 03
    mov ax, strict word 00009h                ; b8 09 00
    out DX, AL                                ; ee
    mov dx, 003d5h                            ; ba d5 03
    in AL, DX                                 ; ec
    and AL, strict byte 060h                  ; 24 60
    out DX, AL                                ; ee
    mov dx, 003d4h                            ; ba d4 03
    mov AL, strict byte 017h                  ; b0 17
    out DX, AL                                ; ee
    mov dx, 003d5h                            ; ba d5 03
    in AL, DX                                 ; ec
    or AL, strict byte 003h                   ; 0c 03
    out DX, AL                                ; ee
    mov dx, 003dah                            ; ba da 03
    in AL, DX                                 ; ec
    mov dx, 003c0h                            ; ba c0 03
    mov AL, strict byte 010h                  ; b0 10
    out DX, AL                                ; ee
    mov dx, 003c1h                            ; ba c1 03
    in AL, DX                                 ; ec
    or AL, strict byte 001h                   ; 0c 01
    mov dx, 003c0h                            ; ba c0 03
    out DX, AL                                ; ee
    mov AL, strict byte 020h                  ; b0 20
    out DX, AL                                ; ee
    mov dx, 003ceh                            ; ba ce 03
    mov ax, 00506h                            ; b8 06 05
    out DX, ax                                ; ef
    mov dx, 003c4h                            ; ba c4 03
    mov ax, 00f02h                            ; b8 02 0f
    out DX, ax                                ; ef
    mov dx, 001ceh                            ; ba ce 01
    mov ax, strict word 00003h                ; b8 03 00
    out DX, ax                                ; ef
    mov dx, 001cfh                            ; ba cf 01
    in ax, DX                                 ; ed
    cmp AL, strict byte 008h                  ; 3c 08
    jc short 007d2h                           ; 72 40
    mov dx, 003d4h                            ; ba d4 03
    mov AL, strict byte 014h                  ; b0 14
    out DX, AL                                ; ee
    mov dx, 003d5h                            ; ba d5 03
    in AL, DX                                 ; ec
    or AL, strict byte 040h                   ; 0c 40
    out DX, AL                                ; ee
    mov dx, 003dah                            ; ba da 03
    in AL, DX                                 ; ec
    mov dx, 003c0h                            ; ba c0 03
    mov AL, strict byte 010h                  ; b0 10
    out DX, AL                                ; ee
    mov dx, 003c1h                            ; ba c1 03
    in AL, DX                                 ; ec
    or AL, strict byte 040h                   ; 0c 40
    mov dx, 003c0h                            ; ba c0 03
    out DX, AL                                ; ee
    mov AL, strict byte 020h                  ; b0 20
    out DX, AL                                ; ee
    mov dx, 003c4h                            ; ba c4 03
    mov AL, strict byte 004h                  ; b0 04
    out DX, AL                                ; ee
    mov dx, 003c5h                            ; ba c5 03
    in AL, DX                                 ; ec
    or AL, strict byte 008h                   ; 0c 08
    out DX, AL                                ; ee
    mov dx, 003ceh                            ; ba ce 03
    mov AL, strict byte 005h                  ; b0 05
    out DX, AL                                ; ee
    mov dx, 003cfh                            ; ba cf 03
    in AL, DX                                 ; ec
    and AL, strict byte 09fh                  ; 24 9f
    or AL, strict byte 040h                   ; 0c 40
    out DX, AL                                ; ee
    pop dx                                    ; 5a
    pop ax                                    ; 58
_vbe_has_vbe_display:                        ; 0xc07d4 LB 0x13
    push DS                                   ; 1e
    push bx                                   ; 53
    mov ax, strict word 00040h                ; b8 40 00
    mov ds, ax                                ; 8e d8
    mov bx, 000b9h                            ; bb b9 00
    mov al, byte [bx]                         ; 8a 07
    and AL, strict byte 001h                  ; 24 01
    db  032h, 0e4h
    ; xor ah, ah                                ; 32 e4
    pop bx                                    ; 5b
    pop DS                                    ; 1f
    retn                                      ; c3
_vbe_init:                                   ; 0xc07e7 LB 0x25
    mov ax, 0b0c0h                            ; b8 c0 b0
    call 00588h                               ; e8 9b fd
    call 0057ah                               ; e8 8a fd
    cmp ax, 0b0c0h                            ; 3d c0 b0
    jne short 0080bh                          ; 75 16
    push DS                                   ; 1e
    push bx                                   ; 53
    mov ax, strict word 00040h                ; b8 40 00
    mov ds, ax                                ; 8e d8
    mov bx, 000b9h                            ; bb b9 00
    mov AL, strict byte 001h                  ; b0 01
    mov byte [bx], al                         ; 88 07
    pop bx                                    ; 5b
    pop DS                                    ; 1f
    mov ax, 0b0c4h                            ; b8 c4 b0
    call 00588h                               ; e8 7d fd
    retn                                      ; c3
vbe_biosfn_return_current_mode:              ; 0xc080c LB 0x25
    push DS                                   ; 1e
    mov ax, strict word 00040h                ; b8 40 00
    mov ds, ax                                ; 8e d8
    call 00609h                               ; e8 f4 fd
    and ax, strict byte 00001h                ; 83 e0 01
    je short 00823h                           ; 74 09
    mov bx, 000bah                            ; bb ba 00
    mov ax, word [bx]                         ; 8b 07
    db  08bh, 0d8h
    ; mov bx, ax                                ; 8b d8
    jne short 0082ch                          ; 75 09
    mov bx, strict word 00049h                ; bb 49 00
    mov al, byte [bx]                         ; 8a 07
    db  08ah, 0d8h
    ; mov bl, al                                ; 8a d8
    db  032h, 0ffh
    ; xor bh, bh                                ; 32 ff
    mov ax, strict word 0004fh                ; b8 4f 00
    pop DS                                    ; 1f
    retn                                      ; c3
vbe_biosfn_display_window_control:           ; 0xc0831 LB 0x2d
    cmp bl, 000h                              ; 80 fb 00
    jne short 0085ah                          ; 75 24
    cmp bh, 001h                              ; 80 ff 01
    je short 00851h                           ; 74 16
    jc short 00841h                           ; 72 04
    mov ax, 00100h                            ; b8 00 01
    retn                                      ; c3
    db  08bh, 0c2h
    ; mov ax, dx                                ; 8b c2
    call 00617h                               ; e8 d1 fd
    call 00627h                               ; e8 de fd
    db  03bh, 0c2h
    ; cmp ax, dx                                ; 3b c2
    jne short 0085ah                          ; 75 0d
    mov ax, strict word 0004fh                ; b8 4f 00
    retn                                      ; c3
    call 00627h                               ; e8 d3 fd
    db  08bh, 0d0h
    ; mov dx, ax                                ; 8b d0
    mov ax, strict word 0004fh                ; b8 4f 00
    retn                                      ; c3
    mov ax, 0014fh                            ; b8 4f 01
    retn                                      ; c3
vbe_biosfn_set_get_logical_scan_line_length: ; 0xc085e LB 0x4d
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    cmp bl, 001h                              ; 80 fb 01
    je short 00889h                           ; 74 24
    cmp bl, 002h                              ; 80 fb 02
    je short 00870h                           ; 74 06
    jc short 00886h                           ; 72 1a
    mov ax, 00100h                            ; b8 00 01
    retn                                      ; c3
    push ax                                   ; 50
    call 00598h                               ; e8 24 fd
    db  032h, 0ffh
    ; xor bh, bh                                ; 32 ff
    db  08ah, 0dch
    ; mov bl, ah                                ; 8a dc
    db  00ah, 0dbh
    ; or bl, bl                                 ; 0a db
    jne short 00881h                          ; 75 05
    sal ax, 003h                              ; c1 e0 03
    mov BL, strict byte 001h                  ; b3 01
    db  033h, 0d2h
    ; xor dx, dx                                ; 33 d2
    pop ax                                    ; 58
    div bx                                    ; f7 f3
    call 006c4h                               ; e8 3b fe
    call 00598h                               ; e8 0c fd
    db  032h, 0ffh
    ; xor bh, bh                                ; 32 ff
    db  08ah, 0dch
    ; mov bl, ah                                ; 8a dc
    call 006d7h                               ; e8 44 fe
    db  08bh, 0c8h
    ; mov cx, ax                                ; 8b c8
    db  00ah, 0dbh
    ; or bl, bl                                 ; 0a db
    jne short 0089eh                          ; 75 05
    shr ax, 003h                              ; c1 e8 03
    mov BL, strict byte 001h                  ; b3 01
    mul bx                                    ; f7 e3
    db  08bh, 0d8h
    ; mov bx, ax                                ; 8b d8
    call 006e5h                               ; e8 40 fe
    db  08bh, 0d0h
    ; mov dx, ax                                ; 8b d0
    mov ax, strict word 0004fh                ; b8 4f 00
    retn                                      ; c3
vbe_biosfn_set_get_display_start:            ; 0xc08ab LB 0x34
    cmp bl, 080h                              ; 80 fb 80
    je short 008bbh                           ; 74 0b
    cmp bl, 001h                              ; 80 fb 01
    je short 008cfh                           ; 74 1a
    jc short 008c1h                           ; 72 0a
    mov ax, 00100h                            ; b8 00 01
    retn                                      ; c3
    call 0056dh                               ; e8 af fc
    call 00560h                               ; e8 9f fc
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    call 0066bh                               ; e8 a5 fd
    db  08bh, 0c2h
    ; mov ax, dx                                ; 8b c2
    call 00689h                               ; e8 be fd
    mov ax, strict word 0004fh                ; b8 4f 00
    retn                                      ; c3
    call 0067bh                               ; e8 a9 fd
    db  08bh, 0c8h
    ; mov cx, ax                                ; 8b c8
    call 00699h                               ; e8 c2 fd
    db  08bh, 0d0h
    ; mov dx, ax                                ; 8b d0
    db  032h, 0ffh
    ; xor bh, bh                                ; 32 ff
    mov ax, strict word 0004fh                ; b8 4f 00
    retn                                      ; c3
vbe_biosfn_set_get_dac_palette_format:       ; 0xc08df LB 0x37
    cmp bl, 001h                              ; 80 fb 01
    je short 00902h                           ; 74 1e
    jc short 008eah                           ; 72 04
    mov ax, 00100h                            ; b8 00 01
    retn                                      ; c3
    call 00609h                               ; e8 1c fd
    cmp bh, 006h                              ; 80 ff 06
    je short 008fch                           ; 74 0a
    cmp bh, 008h                              ; 80 ff 08
    jne short 00912h                          ; 75 1b
    or ax, strict byte 00020h                 ; 83 c8 20
    jne short 008ffh                          ; 75 03
    and ax, strict byte 0ffdfh                ; 83 e0 df
    call 005f9h                               ; e8 f7 fc
    mov BH, strict byte 006h                  ; b7 06
    call 00609h                               ; e8 02 fd
    and ax, strict byte 00020h                ; 83 e0 20
    je short 0090eh                           ; 74 02
    mov BH, strict byte 008h                  ; b7 08
    mov ax, strict word 0004fh                ; b8 4f 00
    retn                                      ; c3
    mov ax, 0014fh                            ; b8 4f 01
    retn                                      ; c3
vbe_biosfn_set_get_palette_data:             ; 0xc0916 LB 0x64
    test bl, bl                               ; 84 db
    je short 00929h                           ; 74 0f
    cmp bl, 001h                              ; 80 fb 01
    je short 00951h                           ; 74 32
    cmp bl, 003h                              ; 80 fb 03
    jbe short 00976h                          ; 76 52
    cmp bl, 080h                              ; 80 fb 80
    jne short 00972h                          ; 75 49
    pushad                                    ; 66 60
    push DS                                   ; 1e
    push ES                                   ; 06
    pop DS                                    ; 1f
    db  08ah, 0c2h
    ; mov al, dl                                ; 8a c2
    mov dx, 003c8h                            ; ba c8 03
    out DX, AL                                ; ee
    inc dx                                    ; 42
    db  08bh, 0f7h
    ; mov si, di                                ; 8b f7
    lodsd                                     ; 66 ad
    ror eax, 010h                             ; 66 c1 c8 10
    out DX, AL                                ; ee
    rol eax, 008h                             ; 66 c1 c0 08
    out DX, AL                                ; ee
    rol eax, 008h                             ; 66 c1 c0 08
    out DX, AL                                ; ee
    loop 00937h                               ; e2 ed
    pop DS                                    ; 1f
    popad                                     ; 66 61
    mov ax, strict word 0004fh                ; b8 4f 00
    retn                                      ; c3
    pushad                                    ; 66 60
    db  08ah, 0c2h
    ; mov al, dl                                ; 8a c2
    mov dx, 003c7h                            ; ba c7 03
    out DX, AL                                ; ee
    add dl, 002h                              ; 80 c2 02
    db  066h, 033h, 0c0h
    ; xor eax, eax                              ; 66 33 c0
    in AL, DX                                 ; ec
    sal eax, 008h                             ; 66 c1 e0 08
    in AL, DX                                 ; ec
    sal eax, 008h                             ; 66 c1 e0 08
    in AL, DX                                 ; ec
    stosd                                     ; 66 ab
    loop 0095ch                               ; e2 ee
    popad                                     ; 66 61
    jmp short 0094dh                          ; eb db
    mov ax, 0014fh                            ; b8 4f 01
    retn                                      ; c3
    mov ax, 0024fh                            ; b8 4f 02
    retn                                      ; c3
vbe_biosfn_return_protected_mode_interface: ; 0xc097a LB 0x19
    test bl, bl                               ; 84 db
    jne short 0098fh                          ; 75 11
    mov di, 0c000h                            ; bf 00 c0
    mov es, di                                ; 8e c7
    mov di, 04600h                            ; bf 00 46
    mov cx, 04711h                            ; b9 11 47
    db  02bh, 0cfh
    ; sub cx, di                                ; 2b cf
    mov ax, strict word 0004fh                ; b8 4f 00
    retn                                      ; c3
    mov ax, 0014fh                            ; b8 4f 01
    retn                                      ; c3

  ; Padding 0x6d bytes at 0xc0993
  times 109 db 0

section _TEXT progbits vstart=0xa00 align=1 ; size=0x2ff9 class=CODE group=AUTO
set_int_vector_:                             ; 0xc0a00 LB 0x1a
    push bx                                   ; 53
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    movzx bx, al                              ; 0f b6 d8
    sal bx, 002h                              ; c1 e3 02
    xor ax, ax                                ; 31 c0
    mov es, ax                                ; 8e c0
    mov word [es:bx], dx                      ; 26 89 17
    mov word [es:bx+002h], 0c000h             ; 26 c7 47 02 00 c0
    pop bp                                    ; 5d
    pop bx                                    ; 5b
    retn                                      ; c3
init_vga_card_:                              ; 0xc0a1a LB 0x22
    push dx                                   ; 52
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov AL, strict byte 0c3h                  ; b0 c3
    mov dx, 003c2h                            ; ba c2 03
    out DX, AL                                ; ee
    mov AL, strict byte 004h                  ; b0 04
    mov dx, 003c4h                            ; ba c4 03
    out DX, AL                                ; ee
    mov AL, strict byte 002h                  ; b0 02
    mov dx, 003c5h                            ; ba c5 03
    out DX, AL                                ; ee
    push 04800h                               ; 68 00 48
    call 02f54h                               ; e8 1e 25
    add sp, strict byte 00002h                ; 83 c4 02
    pop bp                                    ; 5d
    pop dx                                    ; 5a
    retn                                      ; c3
init_bios_area_:                             ; 0xc0a3c LB 0x32
    push bx                                   ; 53
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    xor bx, bx                                ; 31 db
    mov ax, strict word 00040h                ; b8 40 00
    mov es, ax                                ; 8e c0
    mov al, byte [es:bx+010h]                 ; 26 8a 47 10
    and AL, strict byte 0cfh                  ; 24 cf
    or AL, strict byte 020h                   ; 0c 20
    mov byte [es:bx+010h], al                 ; 26 88 47 10
    mov byte [es:bx+00085h], 010h             ; 26 c6 87 85 00 10
    mov word [es:bx+00087h], 0f960h           ; 26 c7 87 87 00 60 f9
    mov byte [es:bx+00089h], 051h             ; 26 c6 87 89 00 51
    mov byte [es:bx+065h], 009h               ; 26 c6 47 65 09
    pop bp                                    ; 5d
    pop bx                                    ; 5b
    retn                                      ; c3
_vgabios_init_func:                          ; 0xc0a6e LB 0x1e
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    call 00a1ah                               ; e8 a6 ff
    call 00a3ch                               ; e8 c5 ff
    call 007e7h                               ; e8 6d fd
    mov dx, strict word 00022h                ; ba 22 00
    mov ax, strict word 00010h                ; b8 10 00
    call 00a00h                               ; e8 7d ff
    mov ax, strict word 00003h                ; b8 03 00
    db  032h, 0e4h
    ; xor ah, ah                                ; 32 e4
    int 010h                                  ; cd 10
    pop bp                                    ; 5d
    retf                                      ; cb
vga_get_cursor_pos_:                         ; 0xc0a8c LB 0x40
    push cx                                   ; 51
    push si                                   ; 56
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov cl, al                                ; 88 c1
    mov si, dx                                ; 89 d6
    cmp AL, strict byte 007h                  ; 3c 07
    jbe short 00aa7h                          ; 76 0e
    push SS                                   ; 16
    pop ES                                    ; 07
    mov word [es:si], strict word 00000h      ; 26 c7 04 00 00
    mov word [es:bx], strict word 00000h      ; 26 c7 07 00 00
    jmp short 00ac8h                          ; eb 21
    mov dx, strict word 00060h                ; ba 60 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f26h                               ; e8 76 24
    push SS                                   ; 16
    pop ES                                    ; 07
    mov word [es:si], ax                      ; 26 89 04
    movzx dx, cl                              ; 0f b6 d1
    add dx, dx                                ; 01 d2
    add dx, strict byte 00050h                ; 83 c2 50
    mov ax, strict word 00040h                ; b8 40 00
    call 02f26h                               ; e8 63 24
    push SS                                   ; 16
    pop ES                                    ; 07
    mov word [es:bx], ax                      ; 26 89 07
    pop bp                                    ; 5d
    pop si                                    ; 5e
    pop cx                                    ; 59
    retn                                      ; c3
vga_read_char_attr_:                         ; 0xc0acc LB 0xa3
    push bx                                   ; 53
    push cx                                   ; 51
    push si                                   ; 56
    push di                                   ; 57
    enter 00008h, 000h                        ; c8 08 00 00
    mov cl, al                                ; 88 c1
    mov si, dx                                ; 89 d6
    mov dx, strict word 00049h                ; ba 49 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f0ah                               ; e8 29 24
    xor ah, ah                                ; 30 e4
    call 02ee3h                               ; e8 fd 23
    mov ch, al                                ; 88 c5
    cmp AL, strict byte 0ffh                  ; 3c ff
    je short 00b59h                           ; 74 6d
    movzx ax, cl                              ; 0f b6 c1
    lea bx, [bp-008h]                         ; 8d 5e f8
    lea dx, [bp-006h]                         ; 8d 56 fa
    call 00a8ch                               ; e8 94 ff
    mov al, byte [bp-008h]                    ; 8a 46 f8
    mov byte [bp-002h], al                    ; 88 46 fe
    mov ax, word [bp-008h]                    ; 8b 46 f8
    xor al, al                                ; 30 c0
    shr ax, 008h                              ; c1 e8 08
    mov word [bp-004h], ax                    ; 89 46 fc
    mov dx, 00084h                            ; ba 84 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f0ah                               ; e8 f8 23
    movzx di, al                              ; 0f b6 f8
    inc di                                    ; 47
    mov dx, strict word 0004ah                ; ba 4a 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f26h                               ; e8 07 24
    movzx bx, ch                              ; 0f b6 dd
    sal bx, 003h                              ; c1 e3 03
    cmp byte [bx+04830h], 000h                ; 80 bf 30 48 00
    jne short 00b59h                          ; 75 2d
    mov dx, ax                                ; 89 c2
    imul dx, di                               ; 0f af d7
    add dx, dx                                ; 01 d2
    or dl, 0ffh                               ; 80 ca ff
    xor ch, ch                                ; 30 ed
    inc dx                                    ; 42
    imul cx, dx                               ; 0f af ca
    movzx dx, byte [bp-004h]                  ; 0f b6 56 fc
    imul dx, ax                               ; 0f af d0
    movzx ax, byte [bp-002h]                  ; 0f b6 46 fe
    add ax, dx                                ; 01 d0
    add ax, ax                                ; 01 c0
    mov dx, cx                                ; 89 ca
    add dx, ax                                ; 01 c2
    mov ax, word [bx+04833h]                  ; 8b 87 33 48
    call 02f26h                               ; e8 d0 23
    mov word [ss:si], ax                      ; 36 89 04
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
    mov cl, byte [bp+di]                      ; 8a 0b
    db  0c6h, 00bh, 0cbh
    ; mov byte [bp+di], 0cbh                    ; c6 0b cb
    db  00bh, 0d3h
    ; or dx, bx                                 ; 0b d3
    db  00bh, 0d8h
    ; or bx, ax                                 ; 0b d8
    db  00bh, 0ddh
    ; or bx, bp                                 ; 0b dd
    db  00bh, 0e2h
    ; or sp, dx                                 ; 0b e2
    db  00bh, 0e7h
    ; or sp, di                                 ; 0b e7
    db  00bh
vga_get_font_info_:                          ; 0xc0b6f LB 0x7f
    push si                                   ; 56
    push di                                   ; 57
    enter 00002h, 000h                        ; c8 02 00 00
    mov si, dx                                ; 89 d6
    mov word [bp-002h], bx                    ; 89 5e fe
    mov bx, cx                                ; 89 cb
    cmp ax, strict word 00007h                ; 3d 07 00
    jnbe short 00bc0h                         ; 77 3f
    mov di, ax                                ; 89 c7
    add di, ax                                ; 01 c7
    jmp word [cs:di+00b5fh]                   ; 2e ff a5 5f 0b
    mov dx, strict word 0007ch                ; ba 7c 00
    xor ax, ax                                ; 31 c0
    call 02f42h                               ; e8 b0 23
    push SS                                   ; 16
    pop ES                                    ; 07
    mov di, word [bp-002h]                    ; 8b 7e fe
    mov word [es:di], ax                      ; 26 89 05
    mov word [es:si], dx                      ; 26 89 14
    mov dx, 00085h                            ; ba 85 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f0ah                               ; e8 64 23
    xor ah, ah                                ; 30 e4
    push SS                                   ; 16
    pop ES                                    ; 07
    mov word [es:bx], ax                      ; 26 89 07
    mov dx, 00084h                            ; ba 84 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f0ah                               ; e8 54 23
    xor ah, ah                                ; 30 e4
    push SS                                   ; 16
    pop ES                                    ; 07
    mov bx, word [bp+008h]                    ; 8b 5e 08
    mov word [es:bx], ax                      ; 26 89 07
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn 00002h                               ; c2 02 00
    mov dx, 0010ch                            ; ba 0c 01
    jmp short 00b8dh                          ; eb c2
    mov ax, 05dafh                            ; b8 af 5d
    mov dx, 0c000h                            ; ba 00 c0
    jmp short 00b92h                          ; eb bf
    mov ax, 055afh                            ; b8 af 55
    jmp short 00bceh                          ; eb f6
    mov ax, 059afh                            ; b8 af 59
    jmp short 00bceh                          ; eb f1
    mov ax, 07bafh                            ; b8 af 7b
    jmp short 00bceh                          ; eb ec
    mov ax, 06bafh                            ; b8 af 6b
    jmp short 00bceh                          ; eb e7
    mov ax, 07cdch                            ; b8 dc 7c
    jmp short 00bceh                          ; eb e2
    jmp short 00bc0h                          ; eb d2
vga_read_pixel_:                             ; 0xc0bee LB 0x134
    push si                                   ; 56
    push di                                   ; 57
    enter 00006h, 000h                        ; c8 06 00 00
    mov si, dx                                ; 89 d6
    mov word [bp-006h], bx                    ; 89 5e fa
    mov di, cx                                ; 89 cf
    mov dx, strict word 00049h                ; ba 49 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f0ah                               ; e8 06 23
    xor ah, ah                                ; 30 e4
    call 02ee3h                               ; e8 da 22
    mov cl, al                                ; 88 c1
    cmp AL, strict byte 0ffh                  ; 3c ff
    je near 00d1eh                            ; 0f 84 0d 01
    movzx bx, al                              ; 0f b6 d8
    sal bx, 003h                              ; c1 e3 03
    cmp byte [bx+04830h], 000h                ; 80 bf 30 48 00
    je near 00d1eh                            ; 0f 84 fe 00
    mov bl, byte [bx+04831h]                  ; 8a 9f 31 48
    cmp bl, 003h                              ; 80 fb 03
    jc short 00c3ah                           ; 72 11
    jbe short 00c42h                          ; 76 17
    cmp bl, 005h                              ; 80 fb 05
    je near 00cfbh                            ; 0f 84 c9 00
    cmp bl, 004h                              ; 80 fb 04
    je short 00c42h                           ; 74 0b
    jmp near 00d19h                           ; e9 df 00
    cmp bl, 002h                              ; 80 fb 02
    je short 00c9ah                           ; 74 5b
    jmp near 00d19h                           ; e9 d7 00
    mov dx, strict word 0004ah                ; ba 4a 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f26h                               ; e8 db 22
    imul ax, word [bp-006h]                   ; 0f af 46 fa
    mov bx, si                                ; 89 f3
    shr bx, 003h                              ; c1 eb 03
    add bx, ax                                ; 01 c3
    mov cx, si                                ; 89 f1
    and cx, strict byte 00007h                ; 83 e1 07
    mov ax, 00080h                            ; b8 80 00
    sar ax, CL                                ; d3 f8
    mov byte [bp-004h], al                    ; 88 46 fc
    mov byte [bp-002h], ch                    ; 88 6e fe
    jmp short 00c70h                          ; eb 08
    cmp byte [bp-002h], 004h                  ; 80 7e fe 04
    jnc near 00d1bh                           ; 0f 83 ab 00
    movzx ax, byte [bp-002h]                  ; 0f b6 46 fe
    sal ax, 008h                              ; c1 e0 08
    or AL, strict byte 004h                   ; 0c 04
    mov dx, 003ceh                            ; ba ce 03
    out DX, ax                                ; ef
    mov dx, bx                                ; 89 da
    mov ax, 0a000h                            ; b8 00 a0
    call 02f0ah                               ; e8 85 22
    and al, byte [bp-004h]                    ; 22 46 fc
    test al, al                               ; 84 c0
    jbe short 00c95h                          ; 76 09
    mov cl, byte [bp-002h]                    ; 8a 4e fe
    mov AL, strict byte 001h                  ; b0 01
    sal al, CL                                ; d2 e0
    or ch, al                                 ; 08 c5
    inc byte [bp-002h]                        ; fe 46 fe
    jmp short 00c68h                          ; eb ce
    mov ax, word [bp-006h]                    ; 8b 46 fa
    shr ax, 1                                 ; d1 e8
    imul ax, ax, strict byte 00050h           ; 6b c0 50
    mov bx, si                                ; 89 f3
    shr bx, 002h                              ; c1 eb 02
    add bx, ax                                ; 01 c3
    test byte [bp-006h], 001h                 ; f6 46 fa 01
    je short 00cb2h                           ; 74 03
    add bh, 020h                              ; 80 c7 20
    mov dx, bx                                ; 89 da
    mov ax, 0b800h                            ; b8 00 b8
    call 02f0ah                               ; e8 50 22
    movzx bx, cl                              ; 0f b6 d9
    sal bx, 003h                              ; c1 e3 03
    cmp byte [bx+04832h], 002h                ; 80 bf 32 48 02
    jne short 00ce2h                          ; 75 1b
    mov cx, si                                ; 89 f1
    xor ch, ch                                ; 30 ed
    and cl, 003h                              ; 80 e1 03
    mov bx, strict word 00003h                ; bb 03 00
    sub bx, cx                                ; 29 cb
    mov cx, bx                                ; 89 d9
    add cx, bx                                ; 01 d9
    xor ah, ah                                ; 30 e4
    sar ax, CL                                ; d3 f8
    mov ch, al                                ; 88 c5
    and ch, 003h                              ; 80 e5 03
    jmp short 00d1bh                          ; eb 39
    mov cx, si                                ; 89 f1
    xor ch, ch                                ; 30 ed
    and cl, 007h                              ; 80 e1 07
    mov bx, strict word 00007h                ; bb 07 00
    sub bx, cx                                ; 29 cb
    mov cx, bx                                ; 89 d9
    xor ah, ah                                ; 30 e4
    sar ax, CL                                ; d3 f8
    mov ch, al                                ; 88 c5
    and ch, 001h                              ; 80 e5 01
    jmp short 00d1bh                          ; eb 20
    mov dx, strict word 0004ah                ; ba 4a 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f26h                               ; e8 22 22
    sal ax, 003h                              ; c1 e0 03
    imul ax, word [bp-006h]                   ; 0f af 46 fa
    mov dx, si                                ; 89 f2
    add dx, ax                                ; 01 c2
    mov ax, 0a000h                            ; b8 00 a0
    call 02f0ah                               ; e8 f5 21
    mov ch, al                                ; 88 c5
    jmp short 00d1bh                          ; eb 02
    xor ch, ch                                ; 30 ed
    mov byte [ss:di], ch                      ; 36 88 2d
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn                                      ; c3
biosfn_perform_gray_scale_summing_:          ; 0xc0d22 LB 0x88
    push bx                                   ; 53
    push cx                                   ; 51
    push si                                   ; 56
    push di                                   ; 57
    enter 00004h, 000h                        ; c8 04 00 00
    mov bx, ax                                ; 89 c3
    mov di, dx                                ; 89 d7
    mov dx, 003dah                            ; ba da 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    xor al, al                                ; 30 c0
    mov dx, 003c0h                            ; ba c0 03
    out DX, AL                                ; ee
    xor si, si                                ; 31 f6
    cmp si, di                                ; 39 fe
    jnc short 00d92h                          ; 73 52
    mov al, bl                                ; 88 d8
    mov dx, 003c7h                            ; ba c7 03
    out DX, AL                                ; ee
    mov dx, 003c9h                            ; ba c9 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov cx, ax                                ; 89 c1
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov word [bp-004h], ax                    ; 89 46 fc
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    xor ch, ch                                ; 30 ed
    imul cx, cx, strict byte 0004dh           ; 6b c9 4d
    mov word [bp-002h], cx                    ; 89 4e fe
    movzx cx, byte [bp-004h]                  ; 0f b6 4e fc
    imul cx, cx, 00097h                       ; 69 c9 97 00
    add cx, word [bp-002h]                    ; 03 4e fe
    xor ah, ah                                ; 30 e4
    imul ax, ax, strict byte 0001ch           ; 6b c0 1c
    add cx, ax                                ; 01 c1
    add cx, 00080h                            ; 81 c1 80 00
    sar cx, 008h                              ; c1 f9 08
    cmp cx, strict byte 0003fh                ; 83 f9 3f
    jbe short 00d80h                          ; 76 03
    mov cx, strict word 0003fh                ; b9 3f 00
    mov al, bl                                ; 88 d8
    mov dx, 003c8h                            ; ba c8 03
    out DX, AL                                ; ee
    mov al, cl                                ; 88 c8
    mov dx, 003c9h                            ; ba c9 03
    out DX, AL                                ; ee
    out DX, AL                                ; ee
    out DX, AL                                ; ee
    inc bx                                    ; 43
    inc si                                    ; 46
    jmp short 00d3ch                          ; eb aa
    mov dx, 003dah                            ; ba da 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov AL, strict byte 020h                  ; b0 20
    mov dx, 003c0h                            ; ba c0 03
    out DX, AL                                ; ee
    mov dx, 003dah                            ; ba da 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
biosfn_set_cursor_shape_:                    ; 0xc0daa LB 0xa1
    push bx                                   ; 53
    push cx                                   ; 51
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov ch, al                                ; 88 c5
    mov cl, dl                                ; 88 d1
    and ch, 03fh                              ; 80 e5 3f
    and cl, 01fh                              ; 80 e1 1f
    movzx di, ch                              ; 0f b6 fd
    mov bx, di                                ; 89 fb
    sal bx, 008h                              ; c1 e3 08
    movzx si, cl                              ; 0f b6 f1
    add bx, si                                ; 01 f3
    mov dx, strict word 00060h                ; ba 60 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f34h                               ; e8 63 21
    mov dx, 00089h                            ; ba 89 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f0ah                               ; e8 30 21
    mov bl, al                                ; 88 c3
    mov dx, 00085h                            ; ba 85 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f26h                               ; e8 41 21
    mov dx, ax                                ; 89 c2
    test bl, 001h                             ; f6 c3 01
    je short 00e23h                           ; 74 37
    cmp ax, strict word 00008h                ; 3d 08 00
    jbe short 00e23h                          ; 76 32
    cmp cl, 008h                              ; 80 f9 08
    jnc short 00e23h                          ; 73 2d
    cmp ch, 020h                              ; 80 fd 20
    jnc short 00e23h                          ; 73 28
    inc di                                    ; 47
    cmp si, di                                ; 39 fe
    je short 00e09h                           ; 74 09
    imul ax, di                               ; 0f af c7
    shr ax, 003h                              ; c1 e8 03
    dec ax                                    ; 48
    jmp short 00e14h                          ; eb 0b
    lea si, [di+001h]                         ; 8d 75 01
    imul ax, si                               ; 0f af c6
    shr ax, 003h                              ; c1 e8 03
    dec ax                                    ; 48
    dec ax                                    ; 48
    mov ch, al                                ; 88 c5
    movzx ax, cl                              ; 0f b6 c1
    inc ax                                    ; 40
    imul ax, dx                               ; 0f af c2
    shr ax, 003h                              ; c1 e8 03
    dec ax                                    ; 48
    mov cl, al                                ; 88 c1
    mov dx, strict word 00063h                ; ba 63 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f26h                               ; e8 fa 20
    mov bx, ax                                ; 89 c3
    mov AL, strict byte 00ah                  ; b0 0a
    mov dx, bx                                ; 89 da
    out DX, AL                                ; ee
    lea si, [bx+001h]                         ; 8d 77 01
    mov al, ch                                ; 88 e8
    mov dx, si                                ; 89 f2
    out DX, AL                                ; ee
    mov AL, strict byte 00bh                  ; b0 0b
    mov dx, bx                                ; 89 da
    out DX, AL                                ; ee
    mov al, cl                                ; 88 c8
    mov dx, si                                ; 89 f2
    out DX, AL                                ; ee
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
biosfn_set_cursor_pos_:                      ; 0xc0e4b LB 0x9e
    push bx                                   ; 53
    push cx                                   ; 51
    push si                                   ; 56
    enter 00004h, 000h                        ; c8 04 00 00
    mov byte [bp-002h], al                    ; 88 46 fe
    mov cx, dx                                ; 89 d1
    cmp AL, strict byte 007h                  ; 3c 07
    jnbe near 00ee4h                          ; 0f 87 87 00
    movzx dx, al                              ; 0f b6 d0
    add dx, dx                                ; 01 d2
    add dx, strict byte 00050h                ; 83 c2 50
    mov bx, cx                                ; 89 cb
    mov ax, strict word 00040h                ; b8 40 00
    call 02f34h                               ; e8 c7 20
    mov dx, strict word 00062h                ; ba 62 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f0ah                               ; e8 94 20
    cmp al, byte [bp-002h]                    ; 3a 46 fe
    jne short 00ee4h                          ; 75 69
    mov dx, strict word 0004ah                ; ba 4a 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f26h                               ; e8 a2 20
    mov bx, ax                                ; 89 c3
    mov dx, 00084h                            ; ba 84 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f0ah                               ; e8 7b 20
    xor ah, ah                                ; 30 e4
    mov dx, ax                                ; 89 c2
    inc dx                                    ; 42
    mov al, cl                                ; 88 c8
    xor cl, cl                                ; 30 c9
    shr cx, 008h                              ; c1 e9 08
    mov byte [bp-004h], cl                    ; 88 4e fc
    imul dx, bx                               ; 0f af d3
    or dl, 0ffh                               ; 80 ca ff
    movzx cx, byte [bp-002h]                  ; 0f b6 4e fe
    inc dx                                    ; 42
    imul dx, cx                               ; 0f af d1
    mov si, ax                                ; 89 c6
    add si, dx                                ; 01 d6
    movzx dx, byte [bp-004h]                  ; 0f b6 56 fc
    imul bx, dx                               ; 0f af da
    add si, bx                                ; 01 de
    mov dx, strict word 00063h                ; ba 63 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f26h                               ; e8 64 20
    mov bx, ax                                ; 89 c3
    mov AL, strict byte 00eh                  ; b0 0e
    mov dx, bx                                ; 89 da
    out DX, AL                                ; ee
    mov ax, si                                ; 89 f0
    xor al, al                                ; 30 c0
    shr ax, 008h                              ; c1 e8 08
    lea cx, [bx+001h]                         ; 8d 4f 01
    mov dx, cx                                ; 89 ca
    out DX, AL                                ; ee
    mov AL, strict byte 00fh                  ; b0 0f
    mov dx, bx                                ; 89 da
    out DX, AL                                ; ee
    and si, 000ffh                            ; 81 e6 ff 00
    mov ax, si                                ; 89 f0
    mov dx, cx                                ; 89 ca
    out DX, AL                                ; ee
    leave                                     ; c9
    pop si                                    ; 5e
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
biosfn_set_active_page_:                     ; 0xc0ee9 LB 0xd8
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    push si                                   ; 56
    push di                                   ; 57
    enter 00004h, 000h                        ; c8 04 00 00
    mov cl, al                                ; 88 c1
    cmp AL, strict byte 007h                  ; 3c 07
    jnbe near 00fbah                          ; 0f 87 c0 00
    mov dx, strict word 00049h                ; ba 49 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f0ah                               ; e8 07 20
    xor ah, ah                                ; 30 e4
    call 02ee3h                               ; e8 db 1f
    mov ch, al                                ; 88 c5
    cmp AL, strict byte 0ffh                  ; 3c ff
    je near 00fbah                            ; 0f 84 aa 00
    movzx ax, cl                              ; 0f b6 c1
    lea bx, [bp-004h]                         ; 8d 5e fc
    lea dx, [bp-002h]                         ; 8d 56 fe
    call 00a8ch                               ; e8 70 fb
    movzx bx, ch                              ; 0f b6 dd
    mov si, bx                                ; 89 de
    sal si, 003h                              ; c1 e6 03
    cmp byte [si+04830h], 000h                ; 80 bc 30 48 00
    jne short 00f6bh                          ; 75 40
    mov dx, strict word 0004ah                ; ba 4a 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f26h                               ; e8 f2 1f
    mov bx, ax                                ; 89 c3
    mov dx, 00084h                            ; ba 84 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f0ah                               ; e8 cb 1f
    xor ah, ah                                ; 30 e4
    inc ax                                    ; 40
    mov si, bx                                ; 89 de
    imul si, ax                               ; 0f af f0
    mov ax, si                                ; 89 f0
    add ax, si                                ; 01 f0
    or AL, strict byte 0ffh                   ; 0c ff
    movzx di, cl                              ; 0f b6 f9
    mov bx, ax                                ; 89 c3
    inc bx                                    ; 43
    imul bx, di                               ; 0f af df
    mov dx, strict word 0004eh                ; ba 4e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f34h                               ; e8 d5 1f
    or si, 000ffh                             ; 81 ce ff 00
    lea bx, [si+001h]                         ; 8d 5c 01
    imul bx, di                               ; 0f af df
    jmp short 00f7dh                          ; eb 12
    movzx bx, byte [bx+048afh]                ; 0f b6 9f af 48
    sal bx, 006h                              ; c1 e3 06
    movzx ax, cl                              ; 0f b6 c1
    mov bx, word [bx+048c6h]                  ; 8b 9f c6 48
    imul bx, ax                               ; 0f af d8
    mov dx, strict word 00063h                ; ba 63 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f26h                               ; e8 a0 1f
    mov si, ax                                ; 89 c6
    mov AL, strict byte 00ch                  ; b0 0c
    mov dx, si                                ; 89 f2
    out DX, AL                                ; ee
    mov ax, bx                                ; 89 d8
    xor al, bl                                ; 30 d8
    shr ax, 008h                              ; c1 e8 08
    lea di, [si+001h]                         ; 8d 7c 01
    mov dx, di                                ; 89 fa
    out DX, AL                                ; ee
    mov AL, strict byte 00dh                  ; b0 0d
    mov dx, si                                ; 89 f2
    out DX, AL                                ; ee
    mov al, bl                                ; 88 d8
    mov dx, di                                ; 89 fa
    out DX, AL                                ; ee
    movzx si, cl                              ; 0f b6 f1
    mov bx, si                                ; 89 f3
    mov dx, strict word 00062h                ; ba 62 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f18h                               ; e8 66 1f
    mov dx, word [bp-004h]                    ; 8b 56 fc
    mov ax, si                                ; 89 f0
    call 00e4bh                               ; e8 91 fe
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
biosfn_set_video_mode_:                      ; 0xc0fc1 LB 0x38c
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    push si                                   ; 56
    push di                                   ; 57
    enter 00010h, 000h                        ; c8 10 00 00
    mov byte [bp-008h], al                    ; 88 46 f8
    and AL, strict byte 080h                  ; 24 80
    mov byte [bp-002h], al                    ; 88 46 fe
    call 007d4h                               ; e8 ff f7
    test ax, ax                               ; 85 c0
    je short 00fe5h                           ; 74 0c
    mov AL, strict byte 007h                  ; b0 07
    mov dx, 003c4h                            ; ba c4 03
    out DX, AL                                ; ee
    xor al, al                                ; 30 c0
    mov dx, 003c5h                            ; ba c5 03
    out DX, AL                                ; ee
    and byte [bp-008h], 07fh                  ; 80 66 f8 7f
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8
    call 02ee3h                               ; e8 f3 1e
    mov byte [bp-004h], al                    ; 88 46 fc
    cmp AL, strict byte 0ffh                  ; 3c ff
    je near 01346h                            ; 0f 84 4d 03
    movzx si, al                              ; 0f b6 f0
    mov al, byte [si+048afh]                  ; 8a 84 af 48
    mov byte [bp-006h], al                    ; 88 46 fa
    movzx bx, al                              ; 0f b6 d8
    sal bx, 006h                              ; c1 e3 06
    movzx ax, byte [bx+048c3h]                ; 0f b6 87 c3 48
    mov word [bp-00ch], ax                    ; 89 46 f4
    movzx ax, byte [bx+048c4h]                ; 0f b6 87 c4 48
    mov word [bp-00ah], ax                    ; 89 46 f6
    movzx ax, byte [bx+048c5h]                ; 0f b6 87 c5 48
    mov word [bp-00eh], ax                    ; 89 46 f2
    mov dx, 00087h                            ; ba 87 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f0ah                               ; e8 e0 1e
    mov dx, 00088h                            ; ba 88 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f0ah                               ; e8 d7 1e
    mov dx, 00089h                            ; ba 89 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f0ah                               ; e8 ce 1e
    mov ah, al                                ; 88 c4
    test AL, strict byte 008h                 ; a8 08
    jne near 010ceh                           ; 0f 85 8a 00
    mov bx, si                                ; 89 f3
    sal bx, 003h                              ; c1 e3 03
    mov al, byte [bx+04835h]                  ; 8a 87 35 48
    mov dx, 003c6h                            ; ba c6 03
    out DX, AL                                ; ee
    xor al, al                                ; 30 c0
    mov dx, 003c8h                            ; ba c8 03
    out DX, AL                                ; ee
    mov bl, byte [bx+04836h]                  ; 8a 9f 36 48
    cmp bl, 001h                              ; 80 fb 01
    jc short 0106eh                           ; 72 0e
    jbe short 01077h                          ; 76 15
    cmp bl, 003h                              ; 80 fb 03
    je short 01081h                           ; 74 1a
    cmp bl, 002h                              ; 80 fb 02
    je short 0107ch                           ; 74 10
    jmp short 01084h                          ; eb 16
    test bl, bl                               ; 84 db
    jne short 01084h                          ; 75 12
    mov di, 05043h                            ; bf 43 50
    jmp short 01084h                          ; eb 0d
    mov di, 05103h                            ; bf 03 51
    jmp short 01084h                          ; eb 08
    mov di, 051c3h                            ; bf c3 51
    jmp short 01084h                          ; eb 03
    mov di, 05283h                            ; bf 83 52
    xor bx, bx                                ; 31 db
    jmp short 01097h                          ; eb 0f
    xor al, al                                ; 30 c0
    mov dx, 003c9h                            ; ba c9 03
    out DX, AL                                ; ee
    out DX, AL                                ; ee
    out DX, AL                                ; ee
    inc bx                                    ; 43
    cmp bx, 00100h                            ; 81 fb 00 01
    jnc short 010c1h                          ; 73 2a
    movzx si, byte [bp-004h]                  ; 0f b6 76 fc
    sal si, 003h                              ; c1 e6 03
    movzx si, byte [si+04836h]                ; 0f b6 b4 36 48
    movzx dx, byte [si+048bfh]                ; 0f b6 94 bf 48
    cmp bx, dx                                ; 39 d3
    jnbe short 01088h                         ; 77 dc
    imul si, bx, strict byte 00003h           ; 6b f3 03
    add si, di                                ; 01 fe
    mov al, byte [si]                         ; 8a 04
    mov dx, 003c9h                            ; ba c9 03
    out DX, AL                                ; ee
    mov al, byte [si+001h]                    ; 8a 44 01
    out DX, AL                                ; ee
    mov al, byte [si+002h]                    ; 8a 44 02
    out DX, AL                                ; ee
    jmp short 01090h                          ; eb cf
    test ah, 002h                             ; f6 c4 02
    je short 010ceh                           ; 74 08
    mov dx, 00100h                            ; ba 00 01
    xor ax, ax                                ; 31 c0
    call 00d22h                               ; e8 54 fc
    mov dx, 003dah                            ; ba da 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    xor bx, bx                                ; 31 db
    jmp short 010ddh                          ; eb 05
    cmp bx, strict byte 00013h                ; 83 fb 13
    jnbe short 010f4h                         ; 77 17
    mov al, bl                                ; 88 d8
    mov dx, 003c0h                            ; ba c0 03
    out DX, AL                                ; ee
    movzx si, byte [bp-006h]                  ; 0f b6 76 fa
    sal si, 006h                              ; c1 e6 06
    add si, bx                                ; 01 de
    mov al, byte [si+048e6h]                  ; 8a 84 e6 48
    out DX, AL                                ; ee
    inc bx                                    ; 43
    jmp short 010d8h                          ; eb e4
    mov AL, strict byte 014h                  ; b0 14
    mov dx, 003c0h                            ; ba c0 03
    out DX, AL                                ; ee
    xor al, al                                ; 30 c0
    out DX, AL                                ; ee
    mov dx, 003c4h                            ; ba c4 03
    out DX, AL                                ; ee
    mov AL, strict byte 003h                  ; b0 03
    mov dx, 003c5h                            ; ba c5 03
    out DX, AL                                ; ee
    mov bx, strict word 00001h                ; bb 01 00
    jmp short 01111h                          ; eb 05
    cmp bx, strict byte 00004h                ; 83 fb 04
    jnbe short 0112bh                         ; 77 1a
    mov al, bl                                ; 88 d8
    mov dx, 003c4h                            ; ba c4 03
    out DX, AL                                ; ee
    movzx si, byte [bp-006h]                  ; 0f b6 76 fa
    sal si, 006h                              ; c1 e6 06
    add si, bx                                ; 01 de
    mov al, byte [si+048c7h]                  ; 8a 84 c7 48
    mov dx, 003c5h                            ; ba c5 03
    out DX, AL                                ; ee
    inc bx                                    ; 43
    jmp short 0110ch                          ; eb e1
    xor bx, bx                                ; 31 db
    jmp short 01134h                          ; eb 05
    cmp bx, strict byte 00008h                ; 83 fb 08
    jnbe short 0114eh                         ; 77 1a
    mov al, bl                                ; 88 d8
    mov dx, 003ceh                            ; ba ce 03
    out DX, AL                                ; ee
    movzx si, byte [bp-006h]                  ; 0f b6 76 fa
    sal si, 006h                              ; c1 e6 06
    add si, bx                                ; 01 de
    mov al, byte [si+048fah]                  ; 8a 84 fa 48
    mov dx, 003cfh                            ; ba cf 03
    out DX, AL                                ; ee
    inc bx                                    ; 43
    jmp short 0112fh                          ; eb e1
    movzx bx, byte [bp-004h]                  ; 0f b6 5e fc
    sal bx, 003h                              ; c1 e3 03
    cmp byte [bx+04831h], 001h                ; 80 bf 31 48 01
    jne short 01161h                          ; 75 05
    mov dx, 003b4h                            ; ba b4 03
    jmp short 01164h                          ; eb 03
    mov dx, 003d4h                            ; ba d4 03
    mov si, dx                                ; 89 d6
    mov ax, strict word 00011h                ; b8 11 00
    out DX, ax                                ; ef
    xor bx, bx                                ; 31 db
    jmp short 01173h                          ; eb 05
    cmp bx, strict byte 00018h                ; 83 fb 18
    jnbe short 0118eh                         ; 77 1b
    mov al, bl                                ; 88 d8
    mov dx, si                                ; 89 f2
    out DX, AL                                ; ee
    movzx cx, byte [bp-006h]                  ; 0f b6 4e fa
    sal cx, 006h                              ; c1 e1 06
    mov di, cx                                ; 89 cf
    add di, bx                                ; 01 df
    lea dx, [si+001h]                         ; 8d 54 01
    mov al, byte [di+048cdh]                  ; 8a 85 cd 48
    out DX, AL                                ; ee
    inc bx                                    ; 43
    jmp short 0116eh                          ; eb e0
    mov bx, cx                                ; 89 cb
    mov al, byte [bx+048cch]                  ; 8a 87 cc 48
    mov dx, 003c2h                            ; ba c2 03
    out DX, AL                                ; ee
    mov AL, strict byte 020h                  ; b0 20
    mov dx, 003c0h                            ; ba c0 03
    out DX, AL                                ; ee
    mov dx, 003dah                            ; ba da 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    cmp byte [bp-002h], 000h                  ; 80 7e fe 00
    jne short 01209h                          ; 75 5f
    movzx bx, byte [bp-004h]                  ; 0f b6 5e fc
    sal bx, 003h                              ; c1 e3 03
    cmp byte [bx+04830h], 000h                ; 80 bf 30 48 00
    jne short 011cbh                          ; 75 13
    mov es, [bx+04833h]                       ; 8e 87 33 48
    mov cx, 04000h                            ; b9 00 40
    mov ax, 00720h                            ; b8 20 07
    xor di, di                                ; 31 ff
    cld                                       ; fc
    jcxz 011c9h                               ; e3 02
    rep stosw                                 ; f3 ab
    jmp short 01209h                          ; eb 3e
    cmp byte [bp-008h], 00dh                  ; 80 7e f8 0d
    jnc short 011e3h                          ; 73 12
    mov es, [bx+04833h]                       ; 8e 87 33 48
    mov cx, 04000h                            ; b9 00 40
    xor ax, ax                                ; 31 c0
    xor di, di                                ; 31 ff
    cld                                       ; fc
    jcxz 011e1h                               ; e3 02
    rep stosw                                 ; f3 ab
    jmp short 01209h                          ; eb 26
    mov AL, strict byte 002h                  ; b0 02
    mov dx, 003c4h                            ; ba c4 03
    out DX, AL                                ; ee
    mov dx, 003c5h                            ; ba c5 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov word [bp-010h], ax                    ; 89 46 f0
    mov AL, strict byte 00fh                  ; b0 0f
    out DX, AL                                ; ee
    mov es, [bx+04833h]                       ; 8e 87 33 48
    mov cx, 08000h                            ; b9 00 80
    xor ax, ax                                ; 31 c0
    xor di, di                                ; 31 ff
    cld                                       ; fc
    jcxz 01205h                               ; e3 02
    rep stosw                                 ; f3 ab
    mov al, byte [bp-010h]                    ; 8a 46 f0
    out DX, AL                                ; ee
    movzx bx, byte [bp-008h]                  ; 0f b6 5e f8
    mov dx, strict word 00049h                ; ba 49 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f18h                               ; e8 02 1d
    mov bx, word [bp-00ch]                    ; 8b 5e f4
    mov dx, strict word 0004ah                ; ba 4a 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f34h                               ; e8 12 1d
    movzx bx, byte [bp-006h]                  ; 0f b6 5e fa
    sal bx, 006h                              ; c1 e3 06
    mov bx, word [bx+048c6h]                  ; 8b 9f c6 48
    mov dx, strict word 0004ch                ; ba 4c 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f34h                               ; e8 fe 1c
    mov bx, si                                ; 89 f3
    mov dx, strict word 00063h                ; ba 63 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f34h                               ; e8 f3 1c
    movzx bx, byte [bp-00ah]                  ; 0f b6 5e f6
    mov dx, 00084h                            ; ba 84 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f18h                               ; e8 ca 1c
    mov bx, word [bp-00eh]                    ; 8b 5e f2
    mov dx, 00085h                            ; ba 85 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f34h                               ; e8 da 1c
    mov al, byte [bp-002h]                    ; 8a 46 fe
    or AL, strict byte 060h                   ; 0c 60
    movzx bx, al                              ; 0f b6 d8
    mov dx, 00087h                            ; ba 87 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f18h                               ; e8 ad 1c
    mov bx, 000f9h                            ; bb f9 00
    mov dx, 00088h                            ; ba 88 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f18h                               ; e8 a1 1c
    mov dx, 00089h                            ; ba 89 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f0ah                               ; e8 8a 1c
    and AL, strict byte 07fh                  ; 24 7f
    movzx bx, al                              ; 0f b6 d8
    mov dx, 00089h                            ; ba 89 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f18h                               ; e8 8a 1c
    mov bx, strict word 00008h                ; bb 08 00
    mov dx, 0008ah                            ; ba 8a 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f18h                               ; e8 7e 1c
    mov bx, 05593h                            ; bb 93 55
    mov dx, 000a8h                            ; ba a8 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f34h                               ; e8 8e 1c
    mov bx, 0c000h                            ; bb 00 c0
    mov dx, 000aah                            ; ba aa 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f34h                               ; e8 82 1c
    xor bx, bx                                ; 31 db
    mov dx, strict word 00065h                ; ba 65 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f18h                               ; e8 5b 1c
    xor bx, bx                                ; 31 db
    mov dx, strict word 00066h                ; ba 66 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f18h                               ; e8 50 1c
    movzx bx, byte [bp-004h]                  ; 0f b6 5e fc
    sal bx, 003h                              ; c1 e3 03
    cmp byte [bx+04830h], 000h                ; 80 bf 30 48 00
    jne short 012dfh                          ; 75 09
    mov dx, strict word 00007h                ; ba 07 00
    mov ax, strict word 00006h                ; b8 06 00
    call 00daah                               ; e8 cb fa
    xor bx, bx                                ; 31 db
    jmp short 012e8h                          ; eb 05
    cmp bx, strict byte 00008h                ; 83 fb 08
    jnc short 012f3h                          ; 73 0b
    movzx ax, bl                              ; 0f b6 c3
    xor dx, dx                                ; 31 d2
    call 00e4bh                               ; e8 5b fb
    inc bx                                    ; 43
    jmp short 012e3h                          ; eb f0
    xor ax, ax                                ; 31 c0
    call 00ee9h                               ; e8 f1 fb
    movzx bx, byte [bp-004h]                  ; 0f b6 5e fc
    sal bx, 003h                              ; c1 e3 03
    cmp byte [bx+04830h], 000h                ; 80 bf 30 48 00
    jne short 01316h                          ; 75 10
    xor bl, bl                                ; 30 db
    mov AL, strict byte 004h                  ; b0 04
    mov AH, strict byte 011h                  ; b4 11
    int 010h                                  ; cd 10
    xor bl, bl                                ; 30 db
    mov AL, strict byte 003h                  ; b0 03
    mov AH, strict byte 011h                  ; b4 11
    int 010h                                  ; cd 10
    mov dx, 059afh                            ; ba af 59
    mov ax, strict word 0001fh                ; b8 1f 00
    call 00a00h                               ; e8 e1 f6
    mov ax, word [bp-00eh]                    ; 8b 46 f2
    cmp ax, strict word 00010h                ; 3d 10 00
    je short 01341h                           ; 74 1a
    cmp ax, strict word 0000eh                ; 3d 0e 00
    je short 0133ch                           ; 74 10
    cmp ax, strict word 00008h                ; 3d 08 00
    jne short 01346h                          ; 75 15
    mov dx, 055afh                            ; ba af 55
    mov ax, strict word 00043h                ; b8 43 00
    call 00a00h                               ; e8 c6 f6
    jmp short 01346h                          ; eb 0a
    mov dx, 05dafh                            ; ba af 5d
    jmp short 01334h                          ; eb f3
    mov dx, 06bafh                            ; ba af 6b
    jmp short 01334h                          ; eb ee
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
vgamem_copy_pl4_:                            ; 0xc134d LB 0x72
    push si                                   ; 56
    push di                                   ; 57
    enter 00004h, 000h                        ; c8 04 00 00
    mov bh, cl                                ; 88 cf
    movzx di, dl                              ; 0f b6 fa
    movzx cx, byte [bp+00ah]                  ; 0f b6 4e 0a
    imul di, cx                               ; 0f af f9
    movzx si, byte [bp+008h]                  ; 0f b6 76 08
    imul di, si                               ; 0f af fe
    xor ah, ah                                ; 30 e4
    add di, ax                                ; 01 c7
    mov word [bp-004h], di                    ; 89 7e fc
    movzx di, bl                              ; 0f b6 fb
    imul cx, di                               ; 0f af cf
    imul cx, si                               ; 0f af ce
    add cx, ax                                ; 01 c1
    mov word [bp-002h], cx                    ; 89 4e fe
    mov ax, 00105h                            ; b8 05 01
    mov dx, 003ceh                            ; ba ce 03
    out DX, ax                                ; ef
    xor bl, bl                                ; 30 db
    cmp bl, byte [bp+00ah]                    ; 3a 5e 0a
    jnc short 013b2h                          ; 73 29
    movzx cx, bh                              ; 0f b6 cf
    movzx si, bl                              ; 0f b6 f3
    movzx ax, byte [bp+008h]                  ; 0f b6 46 08
    imul ax, si                               ; 0f af c6
    mov si, word [bp-004h]                    ; 8b 76 fc
    add si, ax                                ; 01 c6
    mov di, word [bp-002h]                    ; 8b 7e fe
    add di, ax                                ; 01 c7
    mov dx, 0a000h                            ; ba 00 a0
    mov es, dx                                ; 8e c2
    cld                                       ; fc
    jcxz 013aeh                               ; e3 06
    push DS                                   ; 1e
    mov ds, dx                                ; 8e da
    rep movsb                                 ; f3 a4
    pop DS                                    ; 1f
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3
    jmp short 01384h                          ; eb d2
    mov ax, strict word 00005h                ; b8 05 00
    mov dx, 003ceh                            ; ba ce 03
    out DX, ax                                ; ef
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn 00004h                               ; c2 04 00
vgamem_fill_pl4_:                            ; 0xc13bf LB 0x5d
    push di                                   ; 57
    enter 00004h, 000h                        ; c8 04 00 00
    mov byte [bp-002h], bl                    ; 88 5e fe
    mov bh, cl                                ; 88 cf
    movzx cx, dl                              ; 0f b6 ca
    movzx dx, byte [bp+006h]                  ; 0f b6 56 06
    imul cx, dx                               ; 0f af ca
    movzx dx, bh                              ; 0f b6 d7
    imul dx, cx                               ; 0f af d1
    xor ah, ah                                ; 30 e4
    add dx, ax                                ; 01 c2
    mov word [bp-004h], dx                    ; 89 56 fc
    mov ax, 00205h                            ; b8 05 02
    mov dx, 003ceh                            ; ba ce 03
    out DX, ax                                ; ef
    xor bl, bl                                ; 30 db
    cmp bl, byte [bp+006h]                    ; 3a 5e 06
    jnc short 01410h                          ; 73 22
    movzx cx, byte [bp-002h]                  ; 0f b6 4e fe
    movzx ax, byte [bp+008h]                  ; 0f b6 46 08
    movzx dx, bl                              ; 0f b6 d3
    movzx di, bh                              ; 0f b6 ff
    imul di, dx                               ; 0f af fa
    add di, word [bp-004h]                    ; 03 7e fc
    mov dx, 0a000h                            ; ba 00 a0
    mov es, dx                                ; 8e c2
    cld                                       ; fc
    jcxz 0140ch                               ; e3 02
    rep stosb                                 ; f3 aa
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3
    jmp short 013e9h                          ; eb d9
    mov ax, strict word 00005h                ; b8 05 00
    mov dx, 003ceh                            ; ba ce 03
    out DX, ax                                ; ef
    leave                                     ; c9
    pop di                                    ; 5f
    retn 00004h                               ; c2 04 00
vgamem_copy_cga_:                            ; 0xc141c LB 0xa0
    push si                                   ; 56
    push di                                   ; 57
    enter 00004h, 000h                        ; c8 04 00 00
    mov bh, cl                                ; 88 cf
    movzx di, dl                              ; 0f b6 fa
    movzx cx, byte [bp+00ah]                  ; 0f b6 4e 0a
    imul di, cx                               ; 0f af f9
    movzx si, byte [bp+008h]                  ; 0f b6 76 08
    imul di, si                               ; 0f af fe
    sar di, 1                                 ; d1 ff
    xor ah, ah                                ; 30 e4
    add di, ax                                ; 01 c7
    mov word [bp-002h], di                    ; 89 7e fe
    movzx di, bl                              ; 0f b6 fb
    imul cx, di                               ; 0f af cf
    imul si, cx                               ; 0f af f1
    sar si, 1                                 ; d1 fe
    add si, ax                                ; 01 c6
    mov word [bp-004h], si                    ; 89 76 fc
    xor bl, bl                                ; 30 db
    cmp bl, byte [bp+00ah]                    ; 3a 5e 0a
    jnc short 014b6h                          ; 73 61
    test bl, 001h                             ; f6 c3 01
    je short 0148bh                           ; 74 31
    movzx cx, bh                              ; 0f b6 cf
    movzx si, bl                              ; 0f b6 f3
    sar si, 1                                 ; d1 fe
    movzx ax, byte [bp+008h]                  ; 0f b6 46 08
    imul ax, si                               ; 0f af c6
    mov si, word [bp-002h]                    ; 8b 76 fe
    add si, 02000h                            ; 81 c6 00 20
    add si, ax                                ; 01 c6
    mov di, word [bp-004h]                    ; 8b 7e fc
    add di, 02000h                            ; 81 c7 00 20
    add di, ax                                ; 01 c7
    mov dx, 0b800h                            ; ba 00 b8
    mov es, dx                                ; 8e c2
    cld                                       ; fc
    jcxz 01489h                               ; e3 06
    push DS                                   ; 1e
    mov ds, dx                                ; 8e da
    rep movsb                                 ; f3 a4
    pop DS                                    ; 1f
    jmp short 014b2h                          ; eb 27
    movzx cx, bh                              ; 0f b6 cf
    movzx ax, bl                              ; 0f b6 c3
    sar ax, 1                                 ; d1 f8
    movzx si, byte [bp+008h]                  ; 0f b6 76 08
    imul ax, si                               ; 0f af c6
    mov si, word [bp-002h]                    ; 8b 76 fe
    add si, ax                                ; 01 c6
    mov di, word [bp-004h]                    ; 8b 7e fc
    add di, ax                                ; 01 c7
    mov dx, 0b800h                            ; ba 00 b8
    mov es, dx                                ; 8e c2
    cld                                       ; fc
    jcxz 014b2h                               ; e3 06
    push DS                                   ; 1e
    mov ds, dx                                ; 8e da
    rep movsb                                 ; f3 a4
    pop DS                                    ; 1f
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3
    jmp short 01450h                          ; eb 9a
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn 00004h                               ; c2 04 00
vgamem_fill_cga_:                            ; 0xc14bc LB 0x86
    push si                                   ; 56
    push di                                   ; 57
    enter 00004h, 000h                        ; c8 04 00 00
    mov byte [bp-002h], bl                    ; 88 5e fe
    mov bh, cl                                ; 88 cf
    movzx cx, dl                              ; 0f b6 ca
    movzx dx, byte [bp+008h]                  ; 0f b6 56 08
    imul dx, cx                               ; 0f af d1
    movzx cx, bh                              ; 0f b6 cf
    imul dx, cx                               ; 0f af d1
    sar dx, 1                                 ; d1 fa
    movzx si, al                              ; 0f b6 f0
    add si, dx                                ; 01 d6
    xor bl, bl                                ; 30 db
    cmp bl, byte [bp+008h]                    ; 3a 5e 08
    jnc short 0153ch                          ; 73 57
    test bl, 001h                             ; f6 c3 01
    je short 01519h                           ; 74 2f
    movzx cx, byte [bp-002h]                  ; 0f b6 4e fe
    movzx ax, byte [bp+00ah]                  ; 0f b6 46 0a
    movzx dx, bl                              ; 0f b6 d3
    sar dx, 1                                 ; d1 fa
    mov word [bp-004h], dx                    ; 89 56 fc
    movzx dx, bh                              ; 0f b6 d7
    mov di, word [bp-004h]                    ; 8b 7e fc
    imul di, dx                               ; 0f af fa
    mov word [bp-004h], di                    ; 89 7e fc
    lea di, [si+02000h]                       ; 8d bc 00 20
    add di, word [bp-004h]                    ; 03 7e fc
    mov dx, 0b800h                            ; ba 00 b8
    mov es, dx                                ; 8e c2
    cld                                       ; fc
    jcxz 01517h                               ; e3 02
    rep stosb                                 ; f3 aa
    jmp short 01538h                          ; eb 1f
    movzx cx, byte [bp-002h]                  ; 0f b6 4e fe
    movzx ax, byte [bp+00ah]                  ; 0f b6 46 0a
    movzx di, bl                              ; 0f b6 fb
    sar di, 1                                 ; d1 ff
    movzx dx, bh                              ; 0f b6 d7
    imul di, dx                               ; 0f af fa
    add di, si                                ; 01 f7
    mov dx, 0b800h                            ; ba 00 b8
    mov es, dx                                ; 8e c2
    cld                                       ; fc
    jcxz 01538h                               ; e3 02
    rep stosb                                 ; f3 aa
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3
    jmp short 014e0h                          ; eb a4
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn 00004h                               ; c2 04 00
biosfn_scroll_:                              ; 0xc1542 LB 0x501
    push si                                   ; 56
    push di                                   ; 57
    enter 00018h, 000h                        ; c8 18 00 00
    mov byte [bp-00ch], al                    ; 88 46 f4
    mov byte [bp-008h], dl                    ; 88 56 f8
    mov byte [bp-004h], bl                    ; 88 5e fc
    mov byte [bp-002h], cl                    ; 88 4e fe
    cmp bl, byte [bp+008h]                    ; 3a 5e 08
    jnbe near 01a3dh                          ; 0f 87 e2 04
    cmp cl, byte [bp+00ah]                    ; 3a 4e 0a
    jnbe near 01a3dh                          ; 0f 87 db 04
    mov dx, strict word 00049h                ; ba 49 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f0ah                               ; e8 9f 19
    xor ah, ah                                ; 30 e4
    call 02ee3h                               ; e8 73 19
    mov byte [bp-00ah], al                    ; 88 46 f6
    cmp AL, strict byte 0ffh                  ; 3c ff
    je near 01a3dh                            ; 0f 84 c4 04
    mov dx, 00084h                            ; ba 84 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f0ah                               ; e8 88 19
    movzx cx, al                              ; 0f b6 c8
    inc cx                                    ; 41
    mov dx, strict word 0004ah                ; ba 4a 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f26h                               ; e8 97 19
    mov word [bp-012h], ax                    ; 89 46 ee
    cmp byte [bp+00ch], 0ffh                  ; 80 7e 0c ff
    jne short 015a4h                          ; 75 0c
    mov dx, strict word 00062h                ; ba 62 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f0ah                               ; e8 69 19
    mov byte [bp+00ch], al                    ; 88 46 0c
    movzx ax, byte [bp+008h]                  ; 0f b6 46 08
    cmp ax, cx                                ; 39 c8
    jc short 015b3h                           ; 72 07
    mov al, cl                                ; 88 c8
    db  0feh, 0c8h
    ; dec al                                    ; fe c8
    mov byte [bp+008h], al                    ; 88 46 08
    movzx ax, byte [bp+00ah]                  ; 0f b6 46 0a
    cmp ax, word [bp-012h]                    ; 3b 46 ee
    jc short 015c4h                           ; 72 08
    mov al, byte [bp-012h]                    ; 8a 46 ee
    db  0feh, 0c8h
    ; dec al                                    ; fe c8
    mov byte [bp+00ah], al                    ; 88 46 0a
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4
    cmp ax, cx                                ; 39 c8
    jbe short 015d0h                          ; 76 04
    mov byte [bp-00ch], 000h                  ; c6 46 f4 00
    mov al, byte [bp+00ah]                    ; 8a 46 0a
    sub al, byte [bp-002h]                    ; 2a 46 fe
    db  0feh, 0c0h
    ; inc al                                    ; fe c0
    mov byte [bp-00eh], al                    ; 88 46 f2
    movzx si, byte [bp-00ah]                  ; 0f b6 76 f6
    mov di, si                                ; 89 f7
    sal di, 003h                              ; c1 e7 03
    mov ax, word [bp-012h]                    ; 8b 46 ee
    dec ax                                    ; 48
    mov word [bp-014h], ax                    ; 89 46 ec
    mov ax, cx                                ; 89 c8
    dec ax                                    ; 48
    mov word [bp-016h], ax                    ; 89 46 ea
    mov ax, word [bp-012h]                    ; 8b 46 ee
    imul ax, cx                               ; 0f af c1
    cmp byte [di+04830h], 000h                ; 80 bd 30 48 00
    jne near 0179fh                           ; 0f 85 9f 01
    mov dx, ax                                ; 89 c2
    add dx, ax                                ; 01 c2
    or dl, 0ffh                               ; 80 ca ff
    movzx bx, byte [bp+00ch]                  ; 0f b6 5e 0c
    inc dx                                    ; 42
    imul bx, dx                               ; 0f af da
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00
    jne short 0164fh                          ; 75 3a
    cmp byte [bp-004h], 000h                  ; 80 7e fc 00
    jne short 0164fh                          ; 75 34
    cmp byte [bp-002h], 000h                  ; 80 7e fe 00
    jne short 0164fh                          ; 75 2e
    movzx dx, byte [bp+008h]                  ; 0f b6 56 08
    cmp dx, word [bp-016h]                    ; 3b 56 ea
    jne short 0164fh                          ; 75 25
    movzx dx, byte [bp+00ah]                  ; 0f b6 56 0a
    cmp dx, word [bp-014h]                    ; 3b 56 ec
    jne short 0164fh                          ; 75 1c
    movzx dx, byte [bp-008h]                  ; 0f b6 56 f8
    sal dx, 008h                              ; c1 e2 08
    add dx, strict byte 00020h                ; 83 c2 20
    mov es, [di+04833h]                       ; 8e 85 33 48
    mov cx, ax                                ; 89 c1
    mov ax, dx                                ; 89 d0
    mov di, bx                                ; 89 df
    cld                                       ; fc
    jcxz 0164ch                               ; e3 02
    rep stosw                                 ; f3 ab
    jmp near 01a3dh                           ; e9 ee 03
    cmp byte [bp+00eh], 001h                  ; 80 7e 0e 01
    jne near 016f4h                           ; 0f 85 9d 00
    movzx ax, byte [bp-004h]                  ; 0f b6 46 fc
    mov word [bp-010h], ax                    ; 89 46 f0
    movzx dx, byte [bp+008h]                  ; 0f b6 56 08
    cmp dx, word [bp-010h]                    ; 3b 56 f0
    jc near 01a3dh                            ; 0f 82 d4 03
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4
    add ax, word [bp-010h]                    ; 03 46 f0
    cmp ax, dx                                ; 39 d0
    jnbe short 0167ah                         ; 77 06
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00
    jne short 016adh                          ; 75 33
    movzx cx, byte [bp-00eh]                  ; 0f b6 4e f2
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8
    sal ax, 008h                              ; c1 e0 08
    add ax, strict word 00020h                ; 05 20 00
    mov si, word [bp-010h]                    ; 8b 76 f0
    imul si, word [bp-012h]                   ; 0f af 76 ee
    movzx dx, byte [bp-002h]                  ; 0f b6 56 fe
    add dx, si                                ; 01 f2
    add dx, dx                                ; 01 d2
    mov di, bx                                ; 89 df
    add di, dx                                ; 01 d7
    movzx si, byte [bp-00ah]                  ; 0f b6 76 f6
    sal si, 003h                              ; c1 e6 03
    mov es, [si+04833h]                       ; 8e 84 33 48
    cld                                       ; fc
    jcxz 016abh                               ; e3 02
    rep stosw                                 ; f3 ab
    jmp short 016eeh                          ; eb 41
    movzx dx, byte [bp-00eh]                  ; 0f b6 56 f2
    mov word [bp-018h], dx                    ; 89 56 e8
    mov dx, ax                                ; 89 c2
    imul dx, word [bp-012h]                   ; 0f af 56 ee
    movzx cx, byte [bp-002h]                  ; 0f b6 4e fe
    add dx, cx                                ; 01 ca
    add dx, dx                                ; 01 d2
    movzx si, byte [bp-00ah]                  ; 0f b6 76 f6
    sal si, 003h                              ; c1 e6 03
    mov ax, word [si+04833h]                  ; 8b 84 33 48
    mov si, word [bp-010h]                    ; 8b 76 f0
    imul si, word [bp-012h]                   ; 0f af 76 ee
    add cx, si                                ; 01 f1
    add cx, cx                                ; 01 c9
    mov di, bx                                ; 89 df
    add di, cx                                ; 01 cf
    mov cx, word [bp-018h]                    ; 8b 4e e8
    mov si, dx                                ; 89 d6
    mov dx, ax                                ; 89 c2
    mov es, ax                                ; 8e c0
    cld                                       ; fc
    jcxz 016eeh                               ; e3 06
    push DS                                   ; 1e
    mov ds, dx                                ; 8e da
    rep movsw                                 ; f3 a5
    pop DS                                    ; 1f
    inc word [bp-010h]                        ; ff 46 f0
    jmp near 0165eh                           ; e9 6a ff
    movzx ax, byte [bp+008h]                  ; 0f b6 46 08
    mov word [bp-010h], ax                    ; 89 46 f0
    movzx ax, byte [bp-004h]                  ; 0f b6 46 fc
    cmp ax, word [bp-010h]                    ; 3b 46 f0
    jnbe near 01a3dh                          ; 0f 87 37 03
    movzx dx, byte [bp-004h]                  ; 0f b6 56 fc
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4
    add ax, dx                                ; 01 d0
    cmp ax, word [bp-010h]                    ; 3b 46 f0
    jnbe short 0171bh                         ; 77 06
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00
    jne short 0174eh                          ; 75 33
    movzx cx, byte [bp-00eh]                  ; 0f b6 4e f2
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8
    sal ax, 008h                              ; c1 e0 08
    add ax, strict word 00020h                ; 05 20 00
    mov si, word [bp-010h]                    ; 8b 76 f0
    imul si, word [bp-012h]                   ; 0f af 76 ee
    movzx dx, byte [bp-002h]                  ; 0f b6 56 fe
    add dx, si                                ; 01 f2
    add dx, dx                                ; 01 d2
    mov di, bx                                ; 89 df
    add di, dx                                ; 01 d7
    movzx si, byte [bp-00ah]                  ; 0f b6 76 f6
    sal si, 003h                              ; c1 e6 03
    mov es, [si+04833h]                       ; 8e 84 33 48
    cld                                       ; fc
    jcxz 0174ch                               ; e3 02
    rep stosw                                 ; f3 ab
    jmp short 0178eh                          ; eb 40
    movzx cx, byte [bp-00eh]                  ; 0f b6 4e f2
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4
    mov dx, word [bp-010h]                    ; 8b 56 f0
    sub dx, ax                                ; 29 c2
    imul dx, word [bp-012h]                   ; 0f af 56 ee
    movzx di, byte [bp-002h]                  ; 0f b6 7e fe
    add dx, di                                ; 01 fa
    add dx, dx                                ; 01 d2
    movzx si, byte [bp-00ah]                  ; 0f b6 76 f6
    sal si, 003h                              ; c1 e6 03
    mov ax, word [si+04833h]                  ; 8b 84 33 48
    mov si, word [bp-010h]                    ; 8b 76 f0
    imul si, word [bp-012h]                   ; 0f af 76 ee
    add di, si                                ; 01 f7
    add di, di                                ; 01 ff
    add di, bx                                ; 01 df
    mov si, dx                                ; 89 d6
    mov dx, ax                                ; 89 c2
    mov es, ax                                ; 8e c0
    cld                                       ; fc
    jcxz 0178eh                               ; e3 06
    push DS                                   ; 1e
    mov ds, dx                                ; 8e da
    rep movsw                                 ; f3 a5
    pop DS                                    ; 1f
    movzx ax, byte [bp+008h]                  ; 0f b6 46 08
    cmp ax, word [bp-010h]                    ; 3b 46 f0
    jc near 01a3dh                            ; 0f 82 a4 02
    dec word [bp-010h]                        ; ff 4e f0
    jmp near 016fbh                           ; e9 5c ff
    movzx bx, byte [si+048afh]                ; 0f b6 9c af 48
    sal bx, 006h                              ; c1 e3 06
    mov dl, byte [bx+048c5h]                  ; 8a 97 c5 48
    mov byte [bp-006h], dl                    ; 88 56 fa
    mov bl, byte [di+04831h]                  ; 8a 9d 31 48
    cmp bl, 004h                              ; 80 fb 04
    je short 017c6h                           ; 74 0f
    cmp bl, 003h                              ; 80 fb 03
    je short 017c6h                           ; 74 0a
    cmp bl, 002h                              ; 80 fb 02
    je near 01905h                            ; 0f 84 42 01
    jmp near 01a3dh                           ; e9 77 02
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00
    jne short 0181eh                          ; 75 52
    cmp byte [bp-004h], 000h                  ; 80 7e fc 00
    jne short 0181eh                          ; 75 4c
    cmp byte [bp-002h], 000h                  ; 80 7e fe 00
    jne short 0181eh                          ; 75 46
    movzx dx, byte [bp+008h]                  ; 0f b6 56 08
    mov ax, cx                                ; 89 c8
    dec ax                                    ; 48
    cmp dx, ax                                ; 39 c2
    jne short 0181eh                          ; 75 3b
    movzx dx, byte [bp+00ah]                  ; 0f b6 56 0a
    mov ax, word [bp-012h]                    ; 8b 46 ee
    dec ax                                    ; 48
    cmp dx, ax                                ; 39 c2
    jne short 0181eh                          ; 75 2f
    mov ax, 00205h                            ; b8 05 02
    mov dx, 003ceh                            ; ba ce 03
    out DX, ax                                ; ef
    imul cx, word [bp-012h]                   ; 0f af 4e ee
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa
    imul cx, ax                               ; 0f af c8
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8
    movzx bx, byte [bp-00ah]                  ; 0f b6 5e f6
    sal bx, 003h                              ; c1 e3 03
    mov es, [bx+04833h]                       ; 8e 87 33 48
    xor di, di                                ; 31 ff
    cld                                       ; fc
    jcxz 01817h                               ; e3 02
    rep stosb                                 ; f3 aa
    mov ax, strict word 00005h                ; b8 05 00
    out DX, ax                                ; ef
    jmp near 01a3dh                           ; e9 1f 02
    cmp byte [bp+00eh], 001h                  ; 80 7e 0e 01
    jne short 0188dh                          ; 75 69
    movzx ax, byte [bp-004h]                  ; 0f b6 46 fc
    mov word [bp-010h], ax                    ; 89 46 f0
    movzx ax, byte [bp+008h]                  ; 0f b6 46 08
    cmp ax, word [bp-010h]                    ; 3b 46 f0
    jc near 01a3dh                            ; 0f 82 07 02
    movzx dx, byte [bp-00ch]                  ; 0f b6 56 f4
    add dx, word [bp-010h]                    ; 03 56 f0
    cmp dx, ax                                ; 39 c2
    jnbe short 01847h                         ; 77 06
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00
    jne short 01866h                          ; 75 1f
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8
    push ax                                   ; 50
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa
    push ax                                   ; 50
    movzx cx, byte [bp-012h]                  ; 0f b6 4e ee
    movzx bx, byte [bp-00eh]                  ; 0f b6 5e f2
    movzx dx, byte [bp-010h]                  ; 0f b6 56 f0
    movzx ax, byte [bp-002h]                  ; 0f b6 46 fe
    call 013bfh                               ; e8 5b fb
    jmp short 01888h                          ; eb 22
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa
    push ax                                   ; 50
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee
    push ax                                   ; 50
    movzx cx, byte [bp-00eh]                  ; 0f b6 4e f2
    movzx bx, byte [bp-010h]                  ; 0f b6 5e f0
    mov al, byte [bp-010h]                    ; 8a 46 f0
    add al, byte [bp-00ch]                    ; 02 46 f4
    movzx dx, al                              ; 0f b6 d0
    movzx ax, byte [bp-002h]                  ; 0f b6 46 fe
    call 0134dh                               ; e8 c5 fa
    inc word [bp-010h]                        ; ff 46 f0
    jmp short 0182bh                          ; eb 9e
    movzx ax, byte [bp+008h]                  ; 0f b6 46 08
    mov word [bp-010h], ax                    ; 89 46 f0
    movzx ax, byte [bp-004h]                  ; 0f b6 46 fc
    cmp ax, word [bp-010h]                    ; 3b 46 f0
    jnbe near 01a3dh                          ; 0f 87 9e 01
    movzx ax, byte [bp-004h]                  ; 0f b6 46 fc
    movzx dx, byte [bp-00ch]                  ; 0f b6 56 f4
    add ax, dx                                ; 01 d0
    cmp ax, word [bp-010h]                    ; 3b 46 f0
    jnbe short 018b4h                         ; 77 06
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00
    jne short 018d3h                          ; 75 1f
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8
    push ax                                   ; 50
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa
    push ax                                   ; 50
    movzx cx, byte [bp-012h]                  ; 0f b6 4e ee
    movzx bx, byte [bp-00eh]                  ; 0f b6 5e f2
    movzx dx, byte [bp-010h]                  ; 0f b6 56 f0
    movzx ax, byte [bp-002h]                  ; 0f b6 46 fe
    call 013bfh                               ; e8 ee fa
    jmp short 018f5h                          ; eb 22
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa
    push ax                                   ; 50
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee
    push ax                                   ; 50
    movzx cx, byte [bp-00eh]                  ; 0f b6 4e f2
    mov al, byte [bp-010h]                    ; 8a 46 f0
    sub al, byte [bp-00ch]                    ; 2a 46 f4
    movzx bx, al                              ; 0f b6 d8
    movzx dx, byte [bp-010h]                  ; 0f b6 56 f0
    movzx ax, byte [bp-002h]                  ; 0f b6 46 fe
    call 0134dh                               ; e8 58 fa
    movzx ax, byte [bp+008h]                  ; 0f b6 46 08
    cmp ax, word [bp-010h]                    ; 3b 46 f0
    jc near 01a3dh                            ; 0f 82 3d 01
    dec word [bp-010h]                        ; ff 4e f0
    jmp short 01894h                          ; eb 8f
    mov dl, byte [di+04832h]                  ; 8a 95 32 48
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00
    jne short 0194ch                          ; 75 3d
    cmp byte [bp-004h], 000h                  ; 80 7e fc 00
    jne short 0194ch                          ; 75 37
    cmp byte [bp-002h], 000h                  ; 80 7e fe 00
    jne short 0194ch                          ; 75 31
    movzx bx, byte [bp+008h]                  ; 0f b6 5e 08
    cmp bx, word [bp-016h]                    ; 3b 5e ea
    jne short 0194ch                          ; 75 28
    movzx bx, byte [bp+00ah]                  ; 0f b6 5e 0a
    cmp bx, word [bp-014h]                    ; 3b 5e ec
    jne short 0194ch                          ; 75 1f
    movzx bx, byte [bp-006h]                  ; 0f b6 5e fa
    imul ax, bx                               ; 0f af c3
    movzx cx, dl                              ; 0f b6 ca
    imul cx, ax                               ; 0f af c8
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8
    mov es, [di+04833h]                       ; 8e 85 33 48
    xor di, di                                ; 31 ff
    cld                                       ; fc
    jcxz 01949h                               ; e3 02
    rep stosb                                 ; f3 aa
    jmp near 01a3dh                           ; e9 f1 00
    cmp dl, 002h                              ; 80 fa 02
    jne short 0195ah                          ; 75 09
    sal byte [bp-002h], 1                     ; d0 66 fe
    sal byte [bp-00eh], 1                     ; d0 66 f2
    sal word [bp-012h], 1                     ; d1 66 ee
    cmp byte [bp+00eh], 001h                  ; 80 7e 0e 01
    jne short 019c9h                          ; 75 69
    movzx ax, byte [bp-004h]                  ; 0f b6 46 fc
    mov word [bp-010h], ax                    ; 89 46 f0
    movzx ax, byte [bp+008h]                  ; 0f b6 46 08
    cmp ax, word [bp-010h]                    ; 3b 46 f0
    jc near 01a3dh                            ; 0f 82 cb 00
    movzx dx, byte [bp-00ch]                  ; 0f b6 56 f4
    add dx, word [bp-010h]                    ; 03 56 f0
    cmp dx, ax                                ; 39 c2
    jnbe short 01983h                         ; 77 06
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00
    jne short 019a2h                          ; 75 1f
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8
    push ax                                   ; 50
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa
    push ax                                   ; 50
    movzx cx, byte [bp-012h]                  ; 0f b6 4e ee
    movzx bx, byte [bp-00eh]                  ; 0f b6 5e f2
    movzx dx, byte [bp-010h]                  ; 0f b6 56 f0
    movzx ax, byte [bp-002h]                  ; 0f b6 46 fe
    call 014bch                               ; e8 1c fb
    jmp short 019c4h                          ; eb 22
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa
    push ax                                   ; 50
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee
    push ax                                   ; 50
    movzx cx, byte [bp-00eh]                  ; 0f b6 4e f2
    movzx bx, byte [bp-010h]                  ; 0f b6 5e f0
    mov al, byte [bp-010h]                    ; 8a 46 f0
    add al, byte [bp-00ch]                    ; 02 46 f4
    movzx dx, al                              ; 0f b6 d0
    movzx ax, byte [bp-002h]                  ; 0f b6 46 fe
    call 0141ch                               ; e8 58 fa
    inc word [bp-010h]                        ; ff 46 f0
    jmp short 01967h                          ; eb 9e
    movzx ax, byte [bp+008h]                  ; 0f b6 46 08
    mov word [bp-010h], ax                    ; 89 46 f0
    movzx ax, byte [bp-004h]                  ; 0f b6 46 fc
    cmp ax, word [bp-010h]                    ; 3b 46 f0
    jnbe short 01a3dh                         ; 77 64
    movzx ax, byte [bp-004h]                  ; 0f b6 46 fc
    movzx dx, byte [bp-00ch]                  ; 0f b6 56 f4
    add ax, dx                                ; 01 d0
    cmp ax, word [bp-010h]                    ; 3b 46 f0
    jnbe short 019eeh                         ; 77 06
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00
    jne short 01a0dh                          ; 75 1f
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8
    push ax                                   ; 50
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa
    push ax                                   ; 50
    movzx cx, byte [bp-012h]                  ; 0f b6 4e ee
    movzx bx, byte [bp-00eh]                  ; 0f b6 5e f2
    movzx dx, byte [bp-010h]                  ; 0f b6 56 f0
    movzx ax, byte [bp-002h]                  ; 0f b6 46 fe
    call 014bch                               ; e8 b1 fa
    jmp short 01a2fh                          ; eb 22
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa
    push ax                                   ; 50
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee
    push ax                                   ; 50
    movzx cx, byte [bp-00eh]                  ; 0f b6 4e f2
    mov al, byte [bp-010h]                    ; 8a 46 f0
    sub al, byte [bp-00ch]                    ; 2a 46 f4
    movzx bx, al                              ; 0f b6 d8
    movzx dx, byte [bp-010h]                  ; 0f b6 56 f0
    movzx ax, byte [bp-002h]                  ; 0f b6 46 fe
    call 0141ch                               ; e8 ed f9
    movzx ax, byte [bp+008h]                  ; 0f b6 46 08
    cmp ax, word [bp-010h]                    ; 3b 46 f0
    jc short 01a3dh                           ; 72 05
    dec word [bp-010h]                        ; ff 4e f0
    jmp short 019d0h                          ; eb 93
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn 00008h                               ; c2 08 00
write_gfx_char_pl4_:                         ; 0xc1a43 LB 0xe6
    push si                                   ; 56
    push di                                   ; 57
    enter 0000ah, 000h                        ; c8 0a 00 00
    mov byte [bp-002h], dl                    ; 88 56 fe
    mov ah, bl                                ; 88 dc
    cmp byte [bp+00ah], 010h                  ; 80 7e 0a 10
    je short 01a5fh                           ; 74 0b
    cmp byte [bp+00ah], 00eh                  ; 80 7e 0a 0e
    jne short 01a64h                          ; 75 0a
    mov di, 05dafh                            ; bf af 5d
    jmp short 01a67h                          ; eb 08
    mov di, 06bafh                            ; bf af 6b
    jmp short 01a67h                          ; eb 03
    mov di, 055afh                            ; bf af 55
    movzx si, cl                              ; 0f b6 f1
    movzx bx, byte [bp+00ah]                  ; 0f b6 5e 0a
    imul si, bx                               ; 0f af f3
    movzx cx, byte [bp+008h]                  ; 0f b6 4e 08
    imul cx, si                               ; 0f af ce
    movzx si, ah                              ; 0f b6 f4
    add si, cx                                ; 01 ce
    mov word [bp-00ah], si                    ; 89 76 f6
    xor ah, ah                                ; 30 e4
    imul ax, bx                               ; 0f af c3
    mov word [bp-006h], ax                    ; 89 46 fa
    mov ax, 00f02h                            ; b8 02 0f
    mov dx, 003c4h                            ; ba c4 03
    out DX, ax                                ; ef
    mov ax, 00205h                            ; b8 05 02
    mov dx, 003ceh                            ; ba ce 03
    out DX, ax                                ; ef
    test byte [bp-002h], 080h                 ; f6 46 fe 80
    je short 01aa2h                           ; 74 06
    mov ax, 01803h                            ; b8 03 18
    out DX, ax                                ; ef
    jmp short 01aa6h                          ; eb 04
    mov ax, strict word 00003h                ; b8 03 00
    out DX, ax                                ; ef
    xor ch, ch                                ; 30 ed
    cmp ch, byte [bp+00ah]                    ; 3a 6e 0a
    jnc short 01b14h                          ; 73 67
    movzx si, ch                              ; 0f b6 f5
    movzx ax, byte [bp+008h]                  ; 0f b6 46 08
    imul si, ax                               ; 0f af f0
    add si, word [bp-00ah]                    ; 03 76 f6
    mov byte [bp-004h], 000h                  ; c6 46 fc 00
    jmp short 01ad3h                          ; eb 13
    xor bx, bx                                ; 31 db
    mov dx, si                                ; 89 f2
    mov ax, 0a000h                            ; b8 00 a0
    call 02f18h                               ; e8 4e 14
    inc byte [bp-004h]                        ; fe 46 fc
    cmp byte [bp-004h], 008h                  ; 80 7e fc 08
    jnc short 01b10h                          ; 73 3d
    movzx ax, byte [bp-004h]                  ; 0f b6 46 fc
    mov cl, al                                ; 88 c1
    mov ax, 00080h                            ; b8 80 00
    sar ax, CL                                ; d3 f8
    xor ah, ah                                ; 30 e4
    mov word [bp-008h], ax                    ; 89 46 f8
    sal ax, 008h                              ; c1 e0 08
    or AL, strict byte 008h                   ; 0c 08
    mov dx, 003ceh                            ; ba ce 03
    out DX, ax                                ; ef
    mov dx, si                                ; 89 f2
    mov ax, 0a000h                            ; b8 00 a0
    call 02f0ah                               ; e8 16 14
    movzx ax, ch                              ; 0f b6 c5
    add ax, word [bp-006h]                    ; 03 46 fa
    mov bx, di                                ; 89 fb
    add bx, ax                                ; 01 c3
    movzx ax, byte [bx]                       ; 0f b6 07
    test word [bp-008h], ax                   ; 85 46 f8
    je short 01ac0h                           ; 74 ba
    mov al, byte [bp-002h]                    ; 8a 46 fe
    and AL, strict byte 00fh                  ; 24 0f
    movzx bx, al                              ; 0f b6 d8
    jmp short 01ac2h                          ; eb b2
    db  0feh, 0c5h
    ; inc ch                                    ; fe c5
    jmp short 01aa8h                          ; eb 94
    mov ax, 0ff08h                            ; b8 08 ff
    mov dx, 003ceh                            ; ba ce 03
    out DX, ax                                ; ef
    mov ax, strict word 00005h                ; b8 05 00
    out DX, ax                                ; ef
    mov ax, strict word 00003h                ; b8 03 00
    out DX, ax                                ; ef
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn 00004h                               ; c2 04 00
write_gfx_char_cga_:                         ; 0xc1b29 LB 0x119
    push si                                   ; 56
    push di                                   ; 57
    enter 00008h, 000h                        ; c8 08 00 00
    mov byte [bp-004h], dl                    ; 88 56 fc
    mov si, 055afh                            ; be af 55
    xor bh, bh                                ; 30 ff
    movzx di, byte [bp+00ah]                  ; 0f b6 7e 0a
    imul di, bx                               ; 0f af fb
    movzx bx, cl                              ; 0f b6 d9
    imul bx, bx, 00140h                       ; 69 db 40 01
    add di, bx                                ; 01 df
    mov word [bp-008h], di                    ; 89 7e f8
    movzx di, al                              ; 0f b6 f8
    sal di, 003h                              ; c1 e7 03
    mov byte [bp-002h], 000h                  ; c6 46 fe 00
    jmp near 01ba7h                           ; e9 50 00
    xor al, al                                ; 30 c0
    xor ah, ah                                ; 30 e4
    jmp short 01b68h                          ; eb 0b
    or al, bl                                 ; 08 d8
    shr ch, 1                                 ; d0 ed
    db  0feh, 0c4h
    ; inc ah                                    ; fe c4
    cmp ah, 008h                              ; 80 fc 08
    jnc short 01b90h                          ; 73 28
    movzx bx, byte [bp-002h]                  ; 0f b6 5e fe
    add bx, di                                ; 01 fb
    add bx, si                                ; 01 f3
    movzx bx, byte [bx]                       ; 0f b6 1f
    movzx dx, ch                              ; 0f b6 d5
    test bx, dx                               ; 85 d3
    je short 01b5fh                           ; 74 e5
    mov CL, strict byte 007h                  ; b1 07
    sub cl, ah                                ; 28 e1
    mov bl, byte [bp-004h]                    ; 8a 5e fc
    and bl, 001h                              ; 80 e3 01
    sal bl, CL                                ; d2 e3
    test byte [bp-004h], 080h                 ; f6 46 fc 80
    je short 01b5dh                           ; 74 d1
    xor al, bl                                ; 30 d8
    jmp short 01b5fh                          ; eb cf
    movzx bx, al                              ; 0f b6 d8
    mov dx, word [bp-006h]                    ; 8b 56 fa
    mov ax, 0b800h                            ; b8 00 b8
    call 02f18h                               ; e8 7c 13
    inc byte [bp-002h]                        ; fe 46 fe
    cmp byte [bp-002h], 008h                  ; 80 7e fe 08
    jnc near 01c3ch                           ; 0f 83 95 00
    movzx ax, byte [bp-002h]                  ; 0f b6 46 fe
    sar ax, 1                                 ; d1 f8
    imul ax, ax, strict byte 00050h           ; 6b c0 50
    mov bx, word [bp-008h]                    ; 8b 5e f8
    add bx, ax                                ; 01 c3
    mov word [bp-006h], bx                    ; 89 5e fa
    test byte [bp-002h], 001h                 ; f6 46 fe 01
    je short 01bc2h                           ; 74 04
    add byte [bp-005h], 020h                  ; 80 46 fb 20
    mov CH, strict byte 080h                  ; b5 80
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01
    jne short 01bdbh                          ; 75 11
    test byte [bp-004h], ch                   ; 84 6e fc
    je short 01b57h                           ; 74 88
    mov dx, word [bp-006h]                    ; 8b 56 fa
    mov ax, 0b800h                            ; b8 00 b8
    call 02f0ah                               ; e8 32 13
    jmp near 01b59h                           ; e9 7e ff
    test ch, ch                               ; 84 ed
    jbe short 01b9ch                          ; 76 bd
    test byte [bp-004h], 080h                 ; f6 46 fc 80
    je short 01bf0h                           ; 74 0b
    mov dx, word [bp-006h]                    ; 8b 56 fa
    mov ax, 0b800h                            ; b8 00 b8
    call 02f0ah                               ; e8 1c 13
    jmp short 01bf2h                          ; eb 02
    xor al, al                                ; 30 c0
    xor ah, ah                                ; 30 e4
    jmp short 01c01h                          ; eb 0b
    or al, bl                                 ; 08 d8
    shr ch, 1                                 ; d0 ed
    db  0feh, 0c4h
    ; inc ah                                    ; fe c4
    cmp ah, 004h                              ; 80 fc 04
    jnc short 01c2bh                          ; 73 2a
    movzx bx, byte [bp-002h]                  ; 0f b6 5e fe
    add bx, di                                ; 01 fb
    add bx, si                                ; 01 f3
    movzx dx, byte [bx]                       ; 0f b6 17
    movzx bx, ch                              ; 0f b6 dd
    test bx, dx                               ; 85 d3
    je short 01bf8h                           ; 74 e5
    mov CL, strict byte 003h                  ; b1 03
    sub cl, ah                                ; 28 e1
    mov bl, byte [bp-004h]                    ; 8a 5e fc
    and bl, 003h                              ; 80 e3 03
    add cl, cl                                ; 00 c9
    sal bl, CL                                ; d2 e3
    test byte [bp-004h], 080h                 ; f6 46 fc 80
    je short 01bf6h                           ; 74 cf
    xor al, bl                                ; 30 d8
    jmp short 01bf8h                          ; eb cd
    movzx bx, al                              ; 0f b6 d8
    mov dx, word [bp-006h]                    ; 8b 56 fa
    mov ax, 0b800h                            ; b8 00 b8
    call 02f18h                               ; e8 e1 12
    inc word [bp-006h]                        ; ff 46 fa
    jmp short 01bdbh                          ; eb 9f
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn 00004h                               ; c2 04 00
write_gfx_char_lin_:                         ; 0xc1c42 LB 0x8c
    push si                                   ; 56
    push di                                   ; 57
    enter 00008h, 000h                        ; c8 08 00 00
    mov byte [bp-002h], dl                    ; 88 56 fe
    mov di, 055afh                            ; bf af 55
    movzx dx, cl                              ; 0f b6 d1
    movzx cx, byte [bp+008h]                  ; 0f b6 4e 08
    imul cx, dx                               ; 0f af ca
    sal cx, 006h                              ; c1 e1 06
    movzx dx, bl                              ; 0f b6 d3
    sal dx, 003h                              ; c1 e2 03
    add dx, cx                                ; 01 ca
    mov word [bp-008h], dx                    ; 89 56 f8
    movzx si, al                              ; 0f b6 f0
    sal si, 003h                              ; c1 e6 03
    xor cl, cl                                ; 30 c9
    jmp short 01cabh                          ; eb 3b
    cmp ch, 008h                              ; 80 fd 08
    jnc short 01ca4h                          ; 73 2f
    xor al, al                                ; 30 c0
    movzx dx, cl                              ; 0f b6 d1
    add dx, si                                ; 01 f2
    mov bx, di                                ; 89 fb
    add bx, dx                                ; 01 d3
    movzx dx, byte [bx]                       ; 0f b6 17
    movzx bx, byte [bp-004h]                  ; 0f b6 5e fc
    test dx, bx                               ; 85 da
    je short 01c8eh                           ; 74 03
    mov al, byte [bp-002h]                    ; 8a 46 fe
    movzx bx, al                              ; 0f b6 d8
    movzx dx, ch                              ; 0f b6 d5
    add dx, word [bp-006h]                    ; 03 56 fa
    mov ax, 0a000h                            ; b8 00 a0
    call 02f18h                               ; e8 7b 12
    shr byte [bp-004h], 1                     ; d0 6e fc
    db  0feh, 0c5h
    ; inc ch                                    ; fe c5
    jmp short 01c70h                          ; eb cc
    db  0feh, 0c1h
    ; inc cl                                    ; fe c1
    cmp cl, 008h                              ; 80 f9 08
    jnc short 01cc8h                          ; 73 1d
    movzx bx, cl                              ; 0f b6 d9
    movzx dx, byte [bp+008h]                  ; 0f b6 56 08
    imul dx, bx                               ; 0f af d3
    sal dx, 003h                              ; c1 e2 03
    mov bx, word [bp-008h]                    ; 8b 5e f8
    add bx, dx                                ; 01 d3
    mov word [bp-006h], bx                    ; 89 5e fa
    mov byte [bp-004h], 080h                  ; c6 46 fc 80
    xor ch, ch                                ; 30 ed
    jmp short 01c75h                          ; eb ad
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn 00002h                               ; c2 02 00
biosfn_write_char_attr_:                     ; 0xc1cce LB 0x163
    push si                                   ; 56
    push di                                   ; 57
    enter 00018h, 000h                        ; c8 18 00 00
    mov byte [bp-00ah], al                    ; 88 46 f6
    mov byte [bp-00ch], dl                    ; 88 56 f4
    mov byte [bp-00eh], bl                    ; 88 5e f2
    mov si, cx                                ; 89 ce
    mov dx, strict word 00049h                ; ba 49 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f0ah                               ; e8 22 12
    xor ah, ah                                ; 30 e4
    call 02ee3h                               ; e8 f6 11
    mov cl, al                                ; 88 c1
    mov byte [bp-002h], al                    ; 88 46 fe
    cmp AL, strict byte 0ffh                  ; 3c ff
    je near 01e2dh                            ; 0f 84 35 01
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4
    lea bx, [bp-018h]                         ; 8d 5e e8
    lea dx, [bp-016h]                         ; 8d 56 ea
    call 00a8ch                               ; e8 87 ed
    mov al, byte [bp-018h]                    ; 8a 46 e8
    mov byte [bp-008h], al                    ; 88 46 f8
    mov ax, word [bp-018h]                    ; 8b 46 e8
    xor al, al                                ; 30 c0
    shr ax, 008h                              ; c1 e8 08
    mov byte [bp-006h], al                    ; 88 46 fa
    mov dx, 00084h                            ; ba 84 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f0ah                               ; e8 eb 11
    xor ah, ah                                ; 30 e4
    inc ax                                    ; 40
    mov word [bp-014h], ax                    ; 89 46 ec
    mov dx, strict word 0004ah                ; ba 4a 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f26h                               ; e8 f8 11
    mov word [bp-012h], ax                    ; 89 46 ee
    movzx bx, cl                              ; 0f b6 d9
    mov di, bx                                ; 89 df
    sal di, 003h                              ; c1 e7 03
    cmp byte [di+04830h], 000h                ; 80 bd 30 48 00
    jne short 01d87h                          ; 75 47
    mov bx, word [bp-014h]                    ; 8b 5e ec
    imul bx, ax                               ; 0f af d8
    add bx, bx                                ; 01 db
    or bl, 0ffh                               ; 80 cb ff
    movzx dx, byte [bp-00ch]                  ; 0f b6 56 f4
    inc bx                                    ; 43
    imul dx, bx                               ; 0f af d3
    movzx bx, byte [bp-006h]                  ; 0f b6 5e fa
    imul ax, bx                               ; 0f af c3
    movzx bx, byte [bp-008h]                  ; 0f b6 5e f8
    add ax, bx                                ; 01 d8
    add ax, ax                                ; 01 c0
    add dx, ax                                ; 01 c2
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2
    sal ax, 008h                              ; c1 e0 08
    movzx bx, byte [bp-00ah]                  ; 0f b6 5e f6
    add ax, bx                                ; 01 d8
    mov word [bp-016h], ax                    ; 89 46 ea
    mov ax, word [bp-016h]                    ; 8b 46 ea
    mov es, [di+04833h]                       ; 8e 85 33 48
    mov cx, si                                ; 89 f1
    mov di, dx                                ; 89 d7
    cld                                       ; fc
    jcxz 01d84h                               ; e3 02
    rep stosw                                 ; f3 ab
    jmp near 01e2dh                           ; e9 a6 00
    movzx bx, byte [bx+048afh]                ; 0f b6 9f af 48
    sal bx, 006h                              ; c1 e3 06
    mov al, byte [bx+048c5h]                  ; 8a 87 c5 48
    mov byte [bp-004h], al                    ; 88 46 fc
    mov al, byte [di+04832h]                  ; 8a 85 32 48
    mov byte [bp-010h], al                    ; 88 46 f0
    dec si                                    ; 4e
    cmp si, strict byte 0ffffh                ; 83 fe ff
    je near 01e2dh                            ; 0f 84 88 00
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8
    cmp ax, word [bp-012h]                    ; 3b 46 ee
    jnc near 01e2dh                           ; 0f 83 7d 00
    movzx bx, byte [bp-002h]                  ; 0f b6 5e fe
    sal bx, 003h                              ; c1 e3 03
    mov al, byte [bx+04831h]                  ; 8a 87 31 48
    cmp AL, strict byte 003h                  ; 3c 03
    jc short 01dcbh                           ; 72 0c
    jbe short 01dd1h                          ; 76 10
    cmp AL, strict byte 005h                  ; 3c 05
    je short 01e0fh                           ; 74 4a
    cmp AL, strict byte 004h                  ; 3c 04
    je short 01dd1h                           ; 74 08
    jmp short 01e27h                          ; eb 5c
    cmp AL, strict byte 002h                  ; 3c 02
    je short 01df0h                           ; 74 21
    jmp short 01e27h                          ; eb 56
    movzx ax, byte [bp-004h]                  ; 0f b6 46 fc
    push ax                                   ; 50
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee
    push ax                                   ; 50
    movzx cx, byte [bp-006h]                  ; 0f b6 4e fa
    movzx bx, byte [bp-008h]                  ; 0f b6 5e f8
    movzx dx, byte [bp-00eh]                  ; 0f b6 56 f2
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6
    call 01a43h                               ; e8 55 fc
    jmp short 01e27h                          ; eb 37
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0
    push ax                                   ; 50
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee
    push ax                                   ; 50
    movzx cx, byte [bp-006h]                  ; 0f b6 4e fa
    movzx bx, byte [bp-008h]                  ; 0f b6 5e f8
    movzx dx, byte [bp-00eh]                  ; 0f b6 56 f2
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6
    call 01b29h                               ; e8 1c fd
    jmp short 01e27h                          ; eb 18
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee
    push ax                                   ; 50
    movzx cx, byte [bp-006h]                  ; 0f b6 4e fa
    movzx bx, byte [bp-008h]                  ; 0f b6 5e f8
    movzx dx, byte [bp-00eh]                  ; 0f b6 56 f2
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6
    call 01c42h                               ; e8 1b fe
    inc byte [bp-008h]                        ; fe 46 f8
    jmp near 01d9dh                           ; e9 70 ff
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn                                      ; c3
biosfn_write_char_only_:                     ; 0xc1e31 LB 0x16a
    push si                                   ; 56
    push di                                   ; 57
    enter 00018h, 000h                        ; c8 18 00 00
    mov byte [bp-00eh], al                    ; 88 46 f2
    mov byte [bp-002h], dl                    ; 88 56 fe
    mov byte [bp-010h], bl                    ; 88 5e f0
    mov si, cx                                ; 89 ce
    mov dx, strict word 00049h                ; ba 49 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f0ah                               ; e8 bf 10
    xor ah, ah                                ; 30 e4
    call 02ee3h                               ; e8 93 10
    mov cl, al                                ; 88 c1
    mov byte [bp-00ah], al                    ; 88 46 f6
    cmp AL, strict byte 0ffh                  ; 3c ff
    je near 01f97h                            ; 0f 84 3c 01
    movzx ax, byte [bp-002h]                  ; 0f b6 46 fe
    lea bx, [bp-018h]                         ; 8d 5e e8
    lea dx, [bp-016h]                         ; 8d 56 ea
    call 00a8ch                               ; e8 24 ec
    mov al, byte [bp-018h]                    ; 8a 46 e8
    mov byte [bp-008h], al                    ; 88 46 f8
    mov ax, word [bp-018h]                    ; 8b 46 e8
    xor al, al                                ; 30 c0
    shr ax, 008h                              ; c1 e8 08
    mov byte [bp-00ch], al                    ; 88 46 f4
    mov dx, 00084h                            ; ba 84 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f0ah                               ; e8 88 10
    xor ah, ah                                ; 30 e4
    inc ax                                    ; 40
    mov word [bp-014h], ax                    ; 89 46 ec
    mov dx, strict word 0004ah                ; ba 4a 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f26h                               ; e8 95 10
    mov word [bp-012h], ax                    ; 89 46 ee
    movzx di, cl                              ; 0f b6 f9
    mov bx, di                                ; 89 fb
    sal bx, 003h                              ; c1 e3 03
    cmp byte [bx+04830h], 000h                ; 80 bf 30 48 00
    jne short 01eedh                          ; 75 4a
    mov dx, word [bp-014h]                    ; 8b 56 ec
    imul dx, ax                               ; 0f af d0
    add dx, dx                                ; 01 d2
    or dl, 0ffh                               ; 80 ca ff
    movzx bx, byte [bp-002h]                  ; 0f b6 5e fe
    inc dx                                    ; 42
    imul bx, dx                               ; 0f af da
    movzx dx, byte [bp-00ch]                  ; 0f b6 56 f4
    mov cx, ax                                ; 89 c1
    imul cx, dx                               ; 0f af ca
    movzx dx, byte [bp-008h]                  ; 0f b6 56 f8
    add cx, dx                                ; 01 d1
    add cx, cx                                ; 01 c9
    add cx, bx                                ; 01 d9
    dec si                                    ; 4e
    cmp si, strict byte 0ffffh                ; 83 fe ff
    je near 01f97h                            ; 0f 84 c6 00
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2
    movzx bx, byte [bp-00ah]                  ; 0f b6 5e f6
    sal bx, 003h                              ; c1 e3 03
    mov di, word [bx+04833h]                  ; 8b bf 33 48
    mov bx, ax                                ; 89 c3
    mov dx, cx                                ; 89 ca
    mov ax, di                                ; 89 f8
    call 02f18h                               ; e8 2f 10
    inc cx                                    ; 41
    inc cx                                    ; 41
    jmp short 01ec9h                          ; eb dc
    movzx di, byte [di+048afh]                ; 0f b6 bd af 48
    sal di, 006h                              ; c1 e7 06
    mov al, byte [di+048c5h]                  ; 8a 85 c5 48
    mov byte [bp-006h], al                    ; 88 46 fa
    mov al, byte [bx+04832h]                  ; 8a 87 32 48
    mov byte [bp-004h], al                    ; 88 46 fc
    dec si                                    ; 4e
    cmp si, strict byte 0ffffh                ; 83 fe ff
    je near 01f97h                            ; 0f 84 8c 00
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8
    cmp ax, word [bp-012h]                    ; 3b 46 ee
    jnc near 01f97h                           ; 0f 83 81 00
    movzx bx, byte [bp-00ah]                  ; 0f b6 5e f6
    sal bx, 003h                              ; c1 e3 03
    mov bl, byte [bx+04831h]                  ; 8a 9f 31 48
    cmp bl, 003h                              ; 80 fb 03
    jc short 01f34h                           ; 72 0e
    jbe short 01f3bh                          ; 76 13
    cmp bl, 005h                              ; 80 fb 05
    je short 01f79h                           ; 74 4c
    cmp bl, 004h                              ; 80 fb 04
    je short 01f3bh                           ; 74 09
    jmp short 01f91h                          ; eb 5d
    cmp bl, 002h                              ; 80 fb 02
    je short 01f5ah                           ; 74 21
    jmp short 01f91h                          ; eb 56
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa
    push ax                                   ; 50
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee
    push ax                                   ; 50
    movzx cx, byte [bp-00ch]                  ; 0f b6 4e f4
    movzx bx, byte [bp-008h]                  ; 0f b6 5e f8
    movzx dx, byte [bp-010h]                  ; 0f b6 56 f0
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2
    call 01a43h                               ; e8 eb fa
    jmp short 01f91h                          ; eb 37
    movzx ax, byte [bp-004h]                  ; 0f b6 46 fc
    push ax                                   ; 50
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee
    push ax                                   ; 50
    movzx cx, byte [bp-00ch]                  ; 0f b6 4e f4
    movzx bx, byte [bp-008h]                  ; 0f b6 5e f8
    movzx dx, byte [bp-010h]                  ; 0f b6 56 f0
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2
    call 01b29h                               ; e8 b2 fb
    jmp short 01f91h                          ; eb 18
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee
    push ax                                   ; 50
    movzx cx, byte [bp-00ch]                  ; 0f b6 4e f4
    movzx bx, byte [bp-008h]                  ; 0f b6 5e f8
    movzx dx, byte [bp-010h]                  ; 0f b6 56 f0
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2
    call 01c42h                               ; e8 b1 fc
    inc byte [bp-008h]                        ; fe 46 f8
    jmp near 01f03h                           ; e9 6c ff
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn                                      ; c3
biosfn_write_pixel_:                         ; 0xc1f9b LB 0x168
    push si                                   ; 56
    enter 00008h, 000h                        ; c8 08 00 00
    mov byte [bp-004h], dl                    ; 88 56 fc
    mov word [bp-008h], bx                    ; 89 5e f8
    mov dx, strict word 00049h                ; ba 49 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f0ah                               ; e8 5b 0f
    xor ah, ah                                ; 30 e4
    call 02ee3h                               ; e8 2f 0f
    mov byte [bp-002h], al                    ; 88 46 fe
    cmp AL, strict byte 0ffh                  ; 3c ff
    je near 02100h                            ; 0f 84 43 01
    movzx bx, al                              ; 0f b6 d8
    sal bx, 003h                              ; c1 e3 03
    cmp byte [bx+04830h], 000h                ; 80 bf 30 48 00
    je near 02100h                            ; 0f 84 34 01
    mov al, byte [bx+04831h]                  ; 8a 87 31 48
    cmp AL, strict byte 003h                  ; 3c 03
    jc short 01fe3h                           ; 72 0f
    jbe short 01feah                          ; 76 14
    cmp AL, strict byte 005h                  ; 3c 05
    je near 020deh                            ; 0f 84 02 01
    cmp AL, strict byte 004h                  ; 3c 04
    je short 01feah                           ; 74 0a
    leave                                     ; c9
    pop si                                    ; 5e
    retn                                      ; c3
    cmp AL, strict byte 002h                  ; 3c 02
    je short 0204fh                           ; 74 68
    leave                                     ; c9
    pop si                                    ; 5e
    retn                                      ; c3
    mov dx, strict word 0004ah                ; ba 4a 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f26h                               ; e8 33 0f
    imul ax, cx                               ; 0f af c1
    mov bx, word [bp-008h]                    ; 8b 5e f8
    shr bx, 003h                              ; c1 eb 03
    add bx, ax                                ; 01 c3
    mov word [bp-006h], bx                    ; 89 5e fa
    mov cx, word [bp-008h]                    ; 8b 4e f8
    and cl, 007h                              ; 80 e1 07
    mov ax, 00080h                            ; b8 80 00
    sar ax, CL                                ; d3 f8
    xor ah, ah                                ; 30 e4
    sal ax, 008h                              ; c1 e0 08
    or AL, strict byte 008h                   ; 0c 08
    mov dx, 003ceh                            ; ba ce 03
    out DX, ax                                ; ef
    mov ax, 00205h                            ; b8 05 02
    out DX, ax                                ; ef
    mov dx, bx                                ; 89 da
    mov ax, 0a000h                            ; b8 00 a0
    call 02f0ah                               ; e8 e7 0e
    test byte [bp-004h], 080h                 ; f6 46 fc 80
    je short 02030h                           ; 74 07
    mov ax, 01803h                            ; b8 03 18
    mov dx, 003ceh                            ; ba ce 03
    out DX, ax                                ; ef
    movzx bx, byte [bp-004h]                  ; 0f b6 5e fc
    mov dx, word [bp-006h]                    ; 8b 56 fa
    mov ax, 0a000h                            ; b8 00 a0
    call 02f18h                               ; e8 db 0e
    mov ax, 0ff08h                            ; b8 08 ff
    mov dx, 003ceh                            ; ba ce 03
    out DX, ax                                ; ef
    mov ax, strict word 00005h                ; b8 05 00
    out DX, ax                                ; ef
    mov ax, strict word 00003h                ; b8 03 00
    out DX, ax                                ; ef
    leave                                     ; c9
    pop si                                    ; 5e
    retn                                      ; c3
    mov ax, cx                                ; 89 c8
    shr ax, 1                                 ; d1 e8
    imul ax, ax, strict byte 00050h           ; 6b c0 50
    cmp byte [bx+04832h], 002h                ; 80 bf 32 48 02
    jne short 02065h                          ; 75 08
    mov bx, word [bp-008h]                    ; 8b 5e f8
    shr bx, 002h                              ; c1 eb 02
    jmp short 0206bh                          ; eb 06
    mov bx, word [bp-008h]                    ; 8b 5e f8
    shr bx, 003h                              ; c1 eb 03
    add bx, ax                                ; 01 c3
    mov word [bp-006h], bx                    ; 89 5e fa
    test cl, 001h                             ; f6 c1 01
    je short 02079h                           ; 74 04
    add byte [bp-005h], 020h                  ; 80 46 fb 20
    mov dx, word [bp-006h]                    ; 8b 56 fa
    mov ax, 0b800h                            ; b8 00 b8
    call 02f0ah                               ; e8 88 0e
    mov bl, al                                ; 88 c3
    movzx si, byte [bp-002h]                  ; 0f b6 76 fe
    sal si, 003h                              ; c1 e6 03
    cmp byte [si+04832h], 002h                ; 80 bc 32 48 02
    jne short 020abh                          ; 75 19
    mov al, byte [bp-008h]                    ; 8a 46 f8
    and AL, strict byte 003h                  ; 24 03
    mov AH, strict byte 003h                  ; b4 03
    sub ah, al                                ; 28 c4
    mov cl, ah                                ; 88 e1
    add cl, ah                                ; 00 e1
    mov bh, byte [bp-004h]                    ; 8a 7e fc
    and bh, 003h                              ; 80 e7 03
    sal bh, CL                                ; d2 e7
    mov AL, strict byte 003h                  ; b0 03
    jmp short 020beh                          ; eb 13
    mov al, byte [bp-008h]                    ; 8a 46 f8
    and AL, strict byte 007h                  ; 24 07
    mov CL, strict byte 007h                  ; b1 07
    sub cl, al                                ; 28 c1
    mov bh, byte [bp-004h]                    ; 8a 7e fc
    and bh, 001h                              ; 80 e7 01
    sal bh, CL                                ; d2 e7
    mov AL, strict byte 001h                  ; b0 01
    sal al, CL                                ; d2 e0
    test byte [bp-004h], 080h                 ; f6 46 fc 80
    je short 020cah                           ; 74 04
    xor bl, bh                                ; 30 fb
    jmp short 020d0h                          ; eb 06
    not al                                    ; f6 d0
    and bl, al                                ; 20 c3
    or bl, bh                                 ; 08 fb
    xor bh, bh                                ; 30 ff
    mov dx, word [bp-006h]                    ; 8b 56 fa
    mov ax, 0b800h                            ; b8 00 b8
    call 02f18h                               ; e8 3d 0e
    leave                                     ; c9
    pop si                                    ; 5e
    retn                                      ; c3
    mov dx, strict word 0004ah                ; ba 4a 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f26h                               ; e8 3f 0e
    sal ax, 003h                              ; c1 e0 03
    imul cx, ax                               ; 0f af c8
    mov ax, word [bp-008h]                    ; 8b 46 f8
    add ax, cx                                ; 01 c8
    mov word [bp-006h], ax                    ; 89 46 fa
    movzx bx, byte [bp-004h]                  ; 0f b6 5e fc
    mov dx, ax                                ; 89 c2
    mov ax, 0a000h                            ; b8 00 a0
    jmp short 020d8h                          ; eb d8
    leave                                     ; c9
    pop si                                    ; 5e
    retn                                      ; c3
biosfn_write_teletype_:                      ; 0xc2103 LB 0x27f
    push si                                   ; 56
    enter 00016h, 000h                        ; c8 16 00 00
    mov byte [bp-00ah], al                    ; 88 46 f6
    mov byte [bp-002h], dl                    ; 88 56 fe
    mov byte [bp-004h], bl                    ; 88 5e fc
    mov byte [bp-00eh], cl                    ; 88 4e f2
    cmp dl, 0ffh                              ; 80 fa ff
    jne short 02125h                          ; 75 0c
    mov dx, strict word 00062h                ; ba 62 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f0ah                               ; e8 e8 0d
    mov byte [bp-002h], al                    ; 88 46 fe
    mov dx, strict word 00049h                ; ba 49 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f0ah                               ; e8 dc 0d
    xor ah, ah                                ; 30 e4
    call 02ee3h                               ; e8 b0 0d
    mov byte [bp-00ch], al                    ; 88 46 f4
    cmp AL, strict byte 0ffh                  ; 3c ff
    je near 0237fh                            ; 0f 84 43 02
    movzx ax, byte [bp-002h]                  ; 0f b6 46 fe
    lea bx, [bp-016h]                         ; 8d 5e ea
    lea dx, [bp-014h]                         ; 8d 56 ec
    call 00a8ch                               ; e8 43 e9
    mov al, byte [bp-016h]                    ; 8a 46 ea
    mov byte [bp-006h], al                    ; 88 46 fa
    mov ax, word [bp-016h]                    ; 8b 46 ea
    xor al, al                                ; 30 c0
    shr ax, 008h                              ; c1 e8 08
    mov byte [bp-008h], al                    ; 88 46 f8
    mov dx, 00084h                            ; ba 84 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f0ah                               ; e8 a7 0d
    xor ah, ah                                ; 30 e4
    inc ax                                    ; 40
    mov word [bp-012h], ax                    ; 89 46 ee
    mov dx, strict word 0004ah                ; ba 4a 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f26h                               ; e8 b4 0d
    mov word [bp-010h], ax                    ; 89 46 f0
    mov al, byte [bp-00ah]                    ; 8a 46 f6
    cmp AL, strict byte 009h                  ; 3c 09
    jc short 02188h                           ; 72 0c
    jbe short 021afh                          ; 76 31
    cmp AL, strict byte 00dh                  ; 3c 0d
    je short 021a2h                           ; 74 20
    cmp AL, strict byte 00ah                  ; 3c 0a
    je short 021a9h                           ; 74 23
    jmp short 021f0h                          ; eb 68
    cmp AL, strict byte 008h                  ; 3c 08
    je short 02194h                           ; 74 08
    cmp AL, strict byte 007h                  ; 3c 07
    je near 022cch                            ; 0f 84 3a 01
    jmp short 021f0h                          ; eb 5c
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00
    jbe near 022cch                           ; 0f 86 30 01
    dec byte [bp-006h]                        ; fe 4e fa
    jmp near 022cch                           ; e9 2a 01
    mov byte [bp-006h], 000h                  ; c6 46 fa 00
    jmp near 022cch                           ; e9 23 01
    inc byte [bp-008h]                        ; fe 46 f8
    jmp near 022cch                           ; e9 1d 01
    movzx cx, byte [bp-00eh]                  ; 0f b6 4e f2
    movzx bx, byte [bp-004h]                  ; 0f b6 5e fc
    movzx si, byte [bp-002h]                  ; 0f b6 76 fe
    mov dx, si                                ; 89 f2
    mov ax, strict word 00020h                ; b8 20 00
    call 02103h                               ; e8 40 ff
    lea bx, [bp-016h]                         ; 8d 5e ea
    lea dx, [bp-014h]                         ; 8d 56 ec
    mov ax, si                                ; 89 f0
    call 00a8ch                               ; e8 be e8
    mov al, byte [bp-016h]                    ; 8a 46 ea
    mov byte [bp-006h], al                    ; 88 46 fa
    mov ax, word [bp-016h]                    ; 8b 46 ea
    xor al, al                                ; 30 c0
    shr ax, 008h                              ; c1 e8 08
    mov byte [bp-008h], al                    ; 88 46 f8
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa
    mov bx, strict word 00008h                ; bb 08 00
    cwd                                       ; 99
    idiv bx                                   ; f7 fb
    test dx, dx                               ; 85 d2
    je short 021afh                           ; 74 c2
    jmp near 022cch                           ; e9 dc 00
    movzx bx, byte [bp-00ch]                  ; 0f b6 5e f4
    mov si, bx                                ; 89 de
    sal si, 003h                              ; c1 e6 03
    cmp byte [si+04830h], 000h                ; 80 bc 30 48 00
    jne short 0224bh                          ; 75 4b
    mov ax, word [bp-010h]                    ; 8b 46 f0
    imul ax, word [bp-012h]                   ; 0f af 46 ee
    add ax, ax                                ; 01 c0
    or AL, strict byte 0ffh                   ; 0c ff
    movzx dx, byte [bp-002h]                  ; 0f b6 56 fe
    inc ax                                    ; 40
    imul dx, ax                               ; 0f af d0
    movzx cx, byte [bp-008h]                  ; 0f b6 4e f8
    imul cx, word [bp-010h]                   ; 0f af 4e f0
    movzx bx, byte [bp-006h]                  ; 0f b6 5e fa
    add cx, bx                                ; 01 d9
    add cx, cx                                ; 01 c9
    add cx, dx                                ; 01 d1
    movzx bx, byte [bp-00ah]                  ; 0f b6 5e f6
    mov ax, word [si+04833h]                  ; 8b 84 33 48
    mov dx, cx                                ; 89 ca
    call 02f18h                               ; e8 e6 0c
    cmp byte [bp-00eh], 003h                  ; 80 7e f2 03
    jne near 022c9h                           ; 0f 85 8f 00
    movzx bx, byte [bp-004h]                  ; 0f b6 5e fc
    mov dx, cx                                ; 89 ca
    inc dx                                    ; 42
    mov ax, word [si+04833h]                  ; 8b 84 33 48
    call 02f18h                               ; e8 d0 0c
    jmp near 022c9h                           ; e9 7e 00
    movzx bx, byte [bx+048afh]                ; 0f b6 9f af 48
    sal bx, 006h                              ; c1 e3 06
    mov ah, byte [bx+048c5h]                  ; 8a a7 c5 48
    mov dl, byte [si+04832h]                  ; 8a 94 32 48
    mov al, byte [si+04831h]                  ; 8a 84 31 48
    cmp AL, strict byte 003h                  ; 3c 03
    jc short 0226fh                           ; 72 0c
    jbe short 02275h                          ; 76 10
    cmp AL, strict byte 005h                  ; 3c 05
    je short 022b1h                           ; 74 48
    cmp AL, strict byte 004h                  ; 3c 04
    je short 02275h                           ; 74 08
    jmp short 022c9h                          ; eb 5a
    cmp AL, strict byte 002h                  ; 3c 02
    je short 02293h                           ; 74 20
    jmp short 022c9h                          ; eb 54
    movzx ax, ah                              ; 0f b6 c4
    push ax                                   ; 50
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0
    push ax                                   ; 50
    movzx cx, byte [bp-008h]                  ; 0f b6 4e f8
    movzx bx, byte [bp-006h]                  ; 0f b6 5e fa
    movzx dx, byte [bp-004h]                  ; 0f b6 56 fc
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6
    call 01a43h                               ; e8 b2 f7
    jmp short 022c9h                          ; eb 36
    movzx ax, dl                              ; 0f b6 c2
    push ax                                   ; 50
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0
    push ax                                   ; 50
    movzx cx, byte [bp-008h]                  ; 0f b6 4e f8
    movzx bx, byte [bp-006h]                  ; 0f b6 5e fa
    movzx dx, byte [bp-004h]                  ; 0f b6 56 fc
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6
    call 01b29h                               ; e8 7a f8
    jmp short 022c9h                          ; eb 18
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0
    push ax                                   ; 50
    movzx cx, byte [bp-008h]                  ; 0f b6 4e f8
    movzx bx, byte [bp-006h]                  ; 0f b6 5e fa
    movzx dx, byte [bp-004h]                  ; 0f b6 56 fc
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6
    call 01c42h                               ; e8 79 f9
    inc byte [bp-006h]                        ; fe 46 fa
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa
    cmp ax, word [bp-010h]                    ; 3b 46 f0
    jne short 022dch                          ; 75 07
    mov byte [bp-006h], 000h                  ; c6 46 fa 00
    inc byte [bp-008h]                        ; fe 46 f8
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8
    cmp ax, word [bp-012h]                    ; 3b 46 ee
    jne near 02363h                           ; 0f 85 7c 00
    movzx si, byte [bp-00ch]                  ; 0f b6 76 f4
    sal si, 003h                              ; c1 e6 03
    mov bl, byte [bp-012h]                    ; 8a 5e ee
    db  0feh, 0cbh
    ; dec bl                                    ; fe cb
    mov bh, byte [bp-010h]                    ; 8a 7e f0
    db  0feh, 0cfh
    ; dec bh                                    ; fe cf
    cmp byte [si+04830h], 000h                ; 80 bc 30 48 00
    jne short 02345h                          ; 75 46
    mov ax, word [bp-010h]                    ; 8b 46 f0
    imul ax, word [bp-012h]                   ; 0f af 46 ee
    add ax, ax                                ; 01 c0
    or AL, strict byte 0ffh                   ; 0c ff
    movzx dx, byte [bp-002h]                  ; 0f b6 56 fe
    inc ax                                    ; 40
    imul dx, ax                               ; 0f af d0
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8
    dec ax                                    ; 48
    imul ax, word [bp-010h]                   ; 0f af 46 f0
    movzx cx, byte [bp-006h]                  ; 0f b6 4e fa
    add cx, ax                                ; 01 c1
    add cx, cx                                ; 01 c9
    add dx, cx                                ; 01 ca
    inc dx                                    ; 42
    mov ax, word [si+04833h]                  ; 8b 84 33 48
    call 02f0ah                               ; e8 dd 0b
    push strict byte 00001h                   ; 6a 01
    movzx dx, byte [bp-002h]                  ; 0f b6 56 fe
    push dx                                   ; 52
    movzx dx, bh                              ; 0f b6 d7
    push dx                                   ; 52
    movzx dx, bl                              ; 0f b6 d3
    push dx                                   ; 52
    movzx dx, al                              ; 0f b6 d0
    xor cx, cx                                ; 31 c9
    xor bx, bx                                ; 31 db
    jmp short 0235ah                          ; eb 15
    push strict byte 00001h                   ; 6a 01
    movzx ax, byte [bp-002h]                  ; 0f b6 46 fe
    push ax                                   ; 50
    movzx ax, bh                              ; 0f b6 c7
    push ax                                   ; 50
    movzx ax, bl                              ; 0f b6 c3
    push ax                                   ; 50
    xor cx, cx                                ; 31 c9
    xor bx, bx                                ; 31 db
    xor dx, dx                                ; 31 d2
    mov ax, strict word 00001h                ; b8 01 00
    call 01542h                               ; e8 e2 f1
    dec byte [bp-008h]                        ; fe 4e f8
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8
    mov word [bp-016h], ax                    ; 89 46 ea
    sal word [bp-016h], 008h                  ; c1 66 ea 08
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa
    add word [bp-016h], ax                    ; 01 46 ea
    mov dx, word [bp-016h]                    ; 8b 56 ea
    movzx ax, byte [bp-002h]                  ; 0f b6 46 fe
    call 00e4bh                               ; e8 cc ea
    leave                                     ; c9
    pop si                                    ; 5e
    retn                                      ; c3
get_font_access_:                            ; 0xc2382 LB 0x29
    push dx                                   ; 52
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov ax, 00100h                            ; b8 00 01
    mov dx, 003c4h                            ; ba c4 03
    out DX, ax                                ; ef
    mov ax, 00402h                            ; b8 02 04
    out DX, ax                                ; ef
    mov ax, 00704h                            ; b8 04 07
    out DX, ax                                ; ef
    mov ax, 00300h                            ; b8 00 03
    out DX, ax                                ; ef
    mov ax, 00204h                            ; b8 04 02
    mov dx, 003ceh                            ; ba ce 03
    out DX, ax                                ; ef
    mov ax, strict word 00005h                ; b8 05 00
    out DX, ax                                ; ef
    mov ax, 00406h                            ; b8 06 04
    out DX, ax                                ; ef
    pop bp                                    ; 5d
    pop dx                                    ; 5a
    retn                                      ; c3
release_font_access_:                        ; 0xc23ab LB 0x39
    push dx                                   ; 52
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov ax, 00100h                            ; b8 00 01
    mov dx, 003c4h                            ; ba c4 03
    out DX, ax                                ; ef
    mov ax, 00302h                            ; b8 02 03
    out DX, ax                                ; ef
    mov ax, 00304h                            ; b8 04 03
    out DX, ax                                ; ef
    mov ax, 00300h                            ; b8 00 03
    out DX, ax                                ; ef
    mov dx, 003cch                            ; ba cc 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    and ax, strict word 00001h                ; 25 01 00
    sal ax, 002h                              ; c1 e0 02
    or AL, strict byte 00ah                   ; 0c 0a
    sal ax, 008h                              ; c1 e0 08
    or AL, strict byte 006h                   ; 0c 06
    mov dx, 003ceh                            ; ba ce 03
    out DX, ax                                ; ef
    mov ax, strict word 00004h                ; b8 04 00
    out DX, ax                                ; ef
    mov ax, 01005h                            ; b8 05 10
    out DX, ax                                ; ef
    pop bp                                    ; 5d
    pop dx                                    ; 5a
    retn                                      ; c3
set_scan_lines_:                             ; 0xc23e4 LB 0xbc
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bl, al                                ; 88 c3
    mov dx, strict word 00063h                ; ba 63 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f26h                               ; e8 2f 0b
    mov dx, ax                                ; 89 c2
    mov si, ax                                ; 89 c6
    mov AL, strict byte 009h                  ; b0 09
    out DX, AL                                ; ee
    inc dx                                    ; 42
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov ah, al                                ; 88 c4
    and ah, 0e0h                              ; 80 e4 e0
    mov al, bl                                ; 88 d8
    db  0feh, 0c8h
    ; dec al                                    ; fe c8
    or al, ah                                 ; 08 e0
    out DX, AL                                ; ee
    cmp bl, 008h                              ; 80 fb 08
    jne short 0241bh                          ; 75 08
    mov dx, strict word 00007h                ; ba 07 00
    mov ax, strict word 00006h                ; b8 06 00
    jmp short 02428h                          ; eb 0d
    mov al, bl                                ; 88 d8
    sub AL, strict byte 003h                  ; 2c 03
    movzx dx, al                              ; 0f b6 d0
    mov al, bl                                ; 88 d8
    sub AL, strict byte 004h                  ; 2c 04
    xor ah, ah                                ; 30 e4
    call 00daah                               ; e8 7f e9
    movzx di, bl                              ; 0f b6 fb
    mov bx, di                                ; 89 fb
    mov dx, 00085h                            ; ba 85 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f34h                               ; e8 fb 0a
    mov AL, strict byte 012h                  ; b0 12
    mov dx, si                                ; 89 f2
    out DX, AL                                ; ee
    lea cx, [si+001h]                         ; 8d 4c 01
    mov dx, cx                                ; 89 ca
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov bx, ax                                ; 89 c3
    mov AL, strict byte 007h                  ; b0 07
    mov dx, si                                ; 89 f2
    out DX, AL                                ; ee
    mov dx, cx                                ; 89 ca
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov ah, al                                ; 88 c4
    and ah, 002h                              ; 80 e4 02
    movzx dx, ah                              ; 0f b6 d4
    sal dx, 007h                              ; c1 e2 07
    and AL, strict byte 040h                  ; 24 40
    xor ah, ah                                ; 30 e4
    sal ax, 003h                              ; c1 e0 03
    add ax, dx                                ; 01 d0
    inc ax                                    ; 40
    add ax, bx                                ; 01 d8
    xor dx, dx                                ; 31 d2
    div di                                    ; f7 f7
    mov cx, ax                                ; 89 c1
    db  0feh, 0c8h
    ; dec al                                    ; fe c8
    movzx bx, al                              ; 0f b6 d8
    mov dx, 00084h                            ; ba 84 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f18h                               ; e8 9b 0a
    mov dx, strict word 0004ah                ; ba 4a 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f26h                               ; e8 a0 0a
    movzx dx, cl                              ; 0f b6 d1
    mov bx, ax                                ; 89 c3
    imul bx, dx                               ; 0f af da
    add bx, bx                                ; 01 db
    mov dx, strict word 0004ch                ; ba 4c 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f34h                               ; e8 9b 0a
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
biosfn_load_text_user_pat_:                  ; 0xc24a0 LB 0x78
    push si                                   ; 56
    push di                                   ; 57
    enter 0000ah, 000h                        ; c8 0a 00 00
    mov byte [bp-002h], al                    ; 88 46 fe
    mov word [bp-008h], dx                    ; 89 56 f8
    mov word [bp-004h], bx                    ; 89 5e fc
    mov word [bp-006h], cx                    ; 89 4e fa
    call 02382h                               ; e8 cd fe
    mov al, byte [bp+00ah]                    ; 8a 46 0a
    and AL, strict byte 003h                  ; 24 03
    xor ah, ah                                ; 30 e4
    mov bx, ax                                ; 89 c3
    sal bx, 00eh                              ; c1 e3 0e
    mov al, byte [bp+00ah]                    ; 8a 46 0a
    and AL, strict byte 004h                  ; 24 04
    xor ah, ah                                ; 30 e4
    sal ax, 00bh                              ; c1 e0 0b
    add bx, ax                                ; 01 c3
    mov word [bp-00ah], bx                    ; 89 5e f6
    xor bx, bx                                ; 31 db
    cmp bx, word [bp-006h]                    ; 3b 5e fa
    jnc short 02502h                          ; 73 2b
    movzx cx, byte [bp+00ch]                  ; 0f b6 4e 0c
    mov si, bx                                ; 89 de
    imul si, cx                               ; 0f af f1
    add si, word [bp-004h]                    ; 03 76 fc
    mov di, word [bp+008h]                    ; 8b 7e 08
    add di, bx                                ; 01 df
    sal di, 005h                              ; c1 e7 05
    add di, word [bp-00ah]                    ; 03 7e f6
    mov dx, word [bp-008h]                    ; 8b 56 f8
    mov ax, 0a000h                            ; b8 00 a0
    mov es, ax                                ; 8e c0
    cld                                       ; fc
    jcxz 024ffh                               ; e3 06
    push DS                                   ; 1e
    mov ds, dx                                ; 8e da
    rep movsb                                 ; f3 a4
    pop DS                                    ; 1f
    inc bx                                    ; 43
    jmp short 024d2h                          ; eb d0
    call 023abh                               ; e8 a6 fe
    cmp byte [bp-002h], 010h                  ; 80 7e fe 10
    jc short 02512h                           ; 72 07
    movzx ax, byte [bp+00ch]                  ; 0f b6 46 0c
    call 023e4h                               ; e8 d2 fe
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn 00006h                               ; c2 06 00
biosfn_load_text_8_14_pat_:                  ; 0xc2518 LB 0x6c
    push bx                                   ; 53
    push cx                                   ; 51
    push si                                   ; 56
    push di                                   ; 57
    enter 00004h, 000h                        ; c8 04 00 00
    mov byte [bp-002h], al                    ; 88 46 fe
    call 02382h                               ; e8 5c fe
    mov al, dl                                ; 88 d0
    and AL, strict byte 003h                  ; 24 03
    xor ah, ah                                ; 30 e4
    mov bx, ax                                ; 89 c3
    sal bx, 00eh                              ; c1 e3 0e
    mov al, dl                                ; 88 d0
    and AL, strict byte 004h                  ; 24 04
    xor ah, ah                                ; 30 e4
    sal ax, 00bh                              ; c1 e0 0b
    add bx, ax                                ; 01 c3
    mov word [bp-004h], bx                    ; 89 5e fc
    xor bx, bx                                ; 31 db
    jmp short 02549h                          ; eb 06
    cmp bx, 00100h                            ; 81 fb 00 01
    jnc short 0256fh                          ; 73 26
    imul si, bx, strict byte 0000eh           ; 6b f3 0e
    mov di, bx                                ; 89 df
    sal di, 005h                              ; c1 e7 05
    add di, word [bp-004h]                    ; 03 7e fc
    add si, 05dafh                            ; 81 c6 af 5d
    mov cx, strict word 0000eh                ; b9 0e 00
    mov dx, 0c000h                            ; ba 00 c0
    mov ax, 0a000h                            ; b8 00 a0
    mov es, ax                                ; 8e c0
    cld                                       ; fc
    jcxz 0256ch                               ; e3 06
    push DS                                   ; 1e
    mov ds, dx                                ; 8e da
    rep movsb                                 ; f3 a4
    pop DS                                    ; 1f
    inc bx                                    ; 43
    jmp short 02543h                          ; eb d4
    call 023abh                               ; e8 39 fe
    cmp byte [bp-002h], 010h                  ; 80 7e fe 10
    jc short 0257eh                           ; 72 06
    mov ax, strict word 0000eh                ; b8 0e 00
    call 023e4h                               ; e8 66 fe
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
biosfn_load_text_8_8_pat_:                   ; 0xc2584 LB 0x6e
    push bx                                   ; 53
    push cx                                   ; 51
    push si                                   ; 56
    push di                                   ; 57
    enter 00004h, 000h                        ; c8 04 00 00
    mov byte [bp-002h], al                    ; 88 46 fe
    call 02382h                               ; e8 f0 fd
    mov al, dl                                ; 88 d0
    and AL, strict byte 003h                  ; 24 03
    xor ah, ah                                ; 30 e4
    mov bx, ax                                ; 89 c3
    sal bx, 00eh                              ; c1 e3 0e
    mov al, dl                                ; 88 d0
    and AL, strict byte 004h                  ; 24 04
    xor ah, ah                                ; 30 e4
    sal ax, 00bh                              ; c1 e0 0b
    add bx, ax                                ; 01 c3
    mov word [bp-004h], bx                    ; 89 5e fc
    xor bx, bx                                ; 31 db
    jmp short 025b5h                          ; eb 06
    cmp bx, 00100h                            ; 81 fb 00 01
    jnc short 025ddh                          ; 73 28
    mov si, bx                                ; 89 de
    sal si, 003h                              ; c1 e6 03
    mov di, bx                                ; 89 df
    sal di, 005h                              ; c1 e7 05
    add di, word [bp-004h]                    ; 03 7e fc
    add si, 055afh                            ; 81 c6 af 55
    mov cx, strict word 00008h                ; b9 08 00
    mov dx, 0c000h                            ; ba 00 c0
    mov ax, 0a000h                            ; b8 00 a0
    mov es, ax                                ; 8e c0
    cld                                       ; fc
    jcxz 025dah                               ; e3 06
    push DS                                   ; 1e
    mov ds, dx                                ; 8e da
    rep movsb                                 ; f3 a4
    pop DS                                    ; 1f
    inc bx                                    ; 43
    jmp short 025afh                          ; eb d2
    call 023abh                               ; e8 cb fd
    cmp byte [bp-002h], 010h                  ; 80 7e fe 10
    jc short 025ech                           ; 72 06
    mov ax, strict word 00008h                ; b8 08 00
    call 023e4h                               ; e8 f8 fd
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
biosfn_load_text_8_16_pat_:                  ; 0xc25f2 LB 0x6e
    push bx                                   ; 53
    push cx                                   ; 51
    push si                                   ; 56
    push di                                   ; 57
    enter 00004h, 000h                        ; c8 04 00 00
    mov byte [bp-002h], al                    ; 88 46 fe
    call 02382h                               ; e8 82 fd
    mov al, dl                                ; 88 d0
    and AL, strict byte 003h                  ; 24 03
    xor ah, ah                                ; 30 e4
    mov bx, ax                                ; 89 c3
    sal bx, 00eh                              ; c1 e3 0e
    mov al, dl                                ; 88 d0
    and AL, strict byte 004h                  ; 24 04
    xor ah, ah                                ; 30 e4
    sal ax, 00bh                              ; c1 e0 0b
    add bx, ax                                ; 01 c3
    mov word [bp-004h], bx                    ; 89 5e fc
    xor bx, bx                                ; 31 db
    jmp short 02623h                          ; eb 06
    cmp bx, 00100h                            ; 81 fb 00 01
    jnc short 0264bh                          ; 73 28
    mov si, bx                                ; 89 de
    sal si, 004h                              ; c1 e6 04
    mov di, bx                                ; 89 df
    sal di, 005h                              ; c1 e7 05
    add di, word [bp-004h]                    ; 03 7e fc
    add si, 06bafh                            ; 81 c6 af 6b
    mov cx, strict word 00010h                ; b9 10 00
    mov dx, 0c000h                            ; ba 00 c0
    mov ax, 0a000h                            ; b8 00 a0
    mov es, ax                                ; 8e c0
    cld                                       ; fc
    jcxz 02648h                               ; e3 06
    push DS                                   ; 1e
    mov ds, dx                                ; 8e da
    rep movsb                                 ; f3 a4
    pop DS                                    ; 1f
    inc bx                                    ; 43
    jmp short 0261dh                          ; eb d2
    call 023abh                               ; e8 5d fd
    cmp byte [bp-002h], 010h                  ; 80 7e fe 10
    jc short 0265ah                           ; 72 06
    mov ax, strict word 00010h                ; b8 10 00
    call 023e4h                               ; e8 8a fd
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
biosfn_load_gfx_8_8_chars_:                  ; 0xc2660 LB 0x5
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    pop bp                                    ; 5d
    retn                                      ; c3
biosfn_load_gfx_user_chars_:                 ; 0xc2665 LB 0x7
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    pop bp                                    ; 5d
    retn 00002h                               ; c2 02 00
biosfn_load_gfx_8_14_chars_:                 ; 0xc266c LB 0x5
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    pop bp                                    ; 5d
    retn                                      ; c3
biosfn_load_gfx_8_8_dd_chars_:               ; 0xc2671 LB 0x5
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    pop bp                                    ; 5d
    retn                                      ; c3
biosfn_load_gfx_8_16_chars_:                 ; 0xc2676 LB 0x5
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    pop bp                                    ; 5d
    retn                                      ; c3
biosfn_alternate_prtsc_:                     ; 0xc267b LB 0x5
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    pop bp                                    ; 5d
    retn                                      ; c3
biosfn_switch_video_interface_:              ; 0xc2680 LB 0x5
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    pop bp                                    ; 5d
    retn                                      ; c3
biosfn_enable_video_refresh_control_:        ; 0xc2685 LB 0x5
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    pop bp                                    ; 5d
    retn                                      ; c3
biosfn_write_string_:                        ; 0xc268a LB 0x97
    push si                                   ; 56
    push di                                   ; 57
    enter 0000ah, 000h                        ; c8 0a 00 00
    mov byte [bp-006h], al                    ; 88 46 fa
    mov byte [bp-004h], dl                    ; 88 56 fc
    mov byte [bp-002h], bl                    ; 88 5e fe
    mov si, cx                                ; 89 ce
    mov di, word [bp+00eh]                    ; 8b 7e 0e
    movzx ax, dl                              ; 0f b6 c2
    lea bx, [bp-00ah]                         ; 8d 5e f6
    lea dx, [bp-008h]                         ; 8d 56 f8
    call 00a8ch                               ; e8 e2 e3
    cmp byte [bp+008h], 0ffh                  ; 80 7e 08 ff
    jne short 026c1h                          ; 75 11
    mov al, byte [bp-00ah]                    ; 8a 46 f6
    mov byte [bp+00ah], al                    ; 88 46 0a
    mov ax, word [bp-00ah]                    ; 8b 46 f6
    xor al, al                                ; 30 c0
    shr ax, 008h                              ; c1 e8 08
    mov byte [bp+008h], al                    ; 88 46 08
    movzx dx, byte [bp+008h]                  ; 0f b6 56 08
    sal dx, 008h                              ; c1 e2 08
    movzx ax, byte [bp+00ah]                  ; 0f b6 46 0a
    add dx, ax                                ; 01 c2
    movzx ax, byte [bp-004h]                  ; 0f b6 46 fc
    call 00e4bh                               ; e8 76 e7
    dec si                                    ; 4e
    cmp si, strict byte 0ffffh                ; 83 fe ff
    je short 0270bh                           ; 74 30
    mov dx, di                                ; 89 fa
    inc di                                    ; 47
    mov ax, word [bp+00ch]                    ; 8b 46 0c
    call 02f0ah                               ; e8 26 08
    mov cl, al                                ; 88 c1
    test byte [bp-006h], 002h                 ; f6 46 fa 02
    je short 026f8h                           ; 74 0c
    mov dx, di                                ; 89 fa
    inc di                                    ; 47
    mov ax, word [bp+00ch]                    ; 8b 46 0c
    call 02f0ah                               ; e8 15 08
    mov byte [bp-002h], al                    ; 88 46 fe
    movzx bx, byte [bp-002h]                  ; 0f b6 5e fe
    movzx dx, byte [bp-004h]                  ; 0f b6 56 fc
    movzx ax, cl                              ; 0f b6 c1
    mov cx, strict word 00003h                ; b9 03 00
    call 02103h                               ; e8 fa f9
    jmp short 026d5h                          ; eb ca
    test byte [bp-006h], 001h                 ; f6 46 fa 01
    jne short 0271bh                          ; 75 0a
    mov dx, word [bp-00ah]                    ; 8b 56 f6
    movzx ax, byte [bp-004h]                  ; 0f b6 46 fc
    call 00e4bh                               ; e8 30 e7
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn 00008h                               ; c2 08 00
biosfn_read_state_info_:                     ; 0xc2721 LB 0x10a
    push cx                                   ; 51
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push dx                                   ; 52
    push bx                                   ; 53
    mov bx, 05583h                            ; bb 83 55
    mov dx, word [bp-004h]                    ; 8b 56 fc
    mov ax, word [bp-002h]                    ; 8b 46 fe
    call 02f34h                               ; e8 ff 07
    mov dx, word [bp-004h]                    ; 8b 56 fc
    inc dx                                    ; 42
    inc dx                                    ; 42
    mov bx, 0c000h                            ; bb 00 c0
    mov ax, word [bp-002h]                    ; 8b 46 fe
    call 02f34h                               ; e8 f1 07
    mov di, word [bp-004h]                    ; 8b 7e fc
    add di, strict byte 00004h                ; 83 c7 04
    mov cx, strict word 0001eh                ; b9 1e 00
    mov si, strict word 00049h                ; be 49 00
    mov dx, strict word 00040h                ; ba 40 00
    mov es, [bp-002h]                         ; 8e 46 fe
    cld                                       ; fc
    jcxz 0275eh                               ; e3 06
    push DS                                   ; 1e
    mov ds, dx                                ; 8e da
    rep movsb                                 ; f3 a4
    pop DS                                    ; 1f
    mov di, word [bp-004h]                    ; 8b 7e fc
    add di, strict byte 00022h                ; 83 c7 22
    mov cx, strict word 00003h                ; b9 03 00
    mov si, 00084h                            ; be 84 00
    mov dx, strict word 00040h                ; ba 40 00
    mov es, [bp-002h]                         ; 8e 46 fe
    cld                                       ; fc
    jcxz 02779h                               ; e3 06
    push DS                                   ; 1e
    mov ds, dx                                ; 8e da
    rep movsb                                 ; f3 a4
    pop DS                                    ; 1f
    mov dx, 0008ah                            ; ba 8a 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f0ah                               ; e8 88 07
    movzx bx, al                              ; 0f b6 d8
    mov dx, word [bp-004h]                    ; 8b 56 fc
    add dx, strict byte 00025h                ; 83 c2 25
    mov ax, word [bp-002h]                    ; 8b 46 fe
    call 02f18h                               ; e8 87 07
    mov dx, word [bp-004h]                    ; 8b 56 fc
    add dx, strict byte 00026h                ; 83 c2 26
    xor bx, bx                                ; 31 db
    mov ax, word [bp-002h]                    ; 8b 46 fe
    call 02f18h                               ; e8 79 07
    mov dx, word [bp-004h]                    ; 8b 56 fc
    add dx, strict byte 00027h                ; 83 c2 27
    mov bx, strict word 00010h                ; bb 10 00
    mov ax, word [bp-002h]                    ; 8b 46 fe
    call 02f18h                               ; e8 6a 07
    mov dx, word [bp-004h]                    ; 8b 56 fc
    add dx, strict byte 00028h                ; 83 c2 28
    xor bx, bx                                ; 31 db
    mov ax, word [bp-002h]                    ; 8b 46 fe
    call 02f18h                               ; e8 5c 07
    mov dx, word [bp-004h]                    ; 8b 56 fc
    add dx, strict byte 00029h                ; 83 c2 29
    mov bx, strict word 00008h                ; bb 08 00
    mov ax, word [bp-002h]                    ; 8b 46 fe
    call 02f18h                               ; e8 4d 07
    mov dx, word [bp-004h]                    ; 8b 56 fc
    add dx, strict byte 0002ah                ; 83 c2 2a
    mov bx, strict word 00002h                ; bb 02 00
    mov ax, word [bp-002h]                    ; 8b 46 fe
    call 02f18h                               ; e8 3e 07
    mov dx, word [bp-004h]                    ; 8b 56 fc
    add dx, strict byte 0002bh                ; 83 c2 2b
    xor bx, bx                                ; 31 db
    mov ax, word [bp-002h]                    ; 8b 46 fe
    call 02f18h                               ; e8 30 07
    mov dx, word [bp-004h]                    ; 8b 56 fc
    add dx, strict byte 0002ch                ; 83 c2 2c
    xor bx, bx                                ; 31 db
    mov ax, word [bp-002h]                    ; 8b 46 fe
    call 02f18h                               ; e8 22 07
    mov dx, word [bp-004h]                    ; 8b 56 fc
    add dx, strict byte 00031h                ; 83 c2 31
    mov bx, strict word 00003h                ; bb 03 00
    mov ax, word [bp-002h]                    ; 8b 46 fe
    call 02f18h                               ; e8 13 07
    mov dx, word [bp-004h]                    ; 8b 56 fc
    add dx, strict byte 00032h                ; 83 c2 32
    xor bx, bx                                ; 31 db
    mov ax, word [bp-002h]                    ; 8b 46 fe
    call 02f18h                               ; e8 05 07
    mov di, word [bp-004h]                    ; 8b 7e fc
    add di, strict byte 00033h                ; 83 c7 33
    mov cx, strict word 0000dh                ; b9 0d 00
    xor ax, ax                                ; 31 c0
    mov es, [bp-002h]                         ; 8e 46 fe
    cld                                       ; fc
    jcxz 02826h                               ; e3 02
    rep stosb                                 ; f3 aa
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop cx                                    ; 59
    retn                                      ; c3
biosfn_read_video_state_size2_:              ; 0xc282b LB 0x23
    push dx                                   ; 52
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov dx, ax                                ; 89 c2
    xor ax, ax                                ; 31 c0
    test dl, 001h                             ; f6 c2 01
    je short 0283bh                           ; 74 03
    mov ax, strict word 00046h                ; b8 46 00
    test dl, 002h                             ; f6 c2 02
    je short 02843h                           ; 74 03
    add ax, strict word 0002ah                ; 05 2a 00
    test dl, 004h                             ; f6 c2 04
    je short 0284bh                           ; 74 03
    add ax, 00304h                            ; 05 04 03
    pop bp                                    ; 5d
    pop dx                                    ; 5a
    retn                                      ; c3
vga_get_video_state_size_:                   ; 0xc284e LB 0xf
    push bx                                   ; 53
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bx, dx                                ; 89 d3
    call 0282bh                               ; e8 d4 ff
    mov word [ss:bx], ax                      ; 36 89 07
    pop bp                                    ; 5d
    pop bx                                    ; 5b
    retn                                      ; c3
biosfn_save_video_state_:                    ; 0xc285d LB 0x365
    push cx                                   ; 51
    push si                                   ; 56
    push di                                   ; 57
    enter 00004h, 000h                        ; c8 04 00 00
    push ax                                   ; 50
    mov si, dx                                ; 89 d6
    mov cx, bx                                ; 89 d9
    mov dx, strict word 00063h                ; ba 63 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f26h                               ; e8 b4 06
    mov di, ax                                ; 89 c7
    test byte [bp-006h], 001h                 ; f6 46 fa 01
    je near 029dfh                            ; 0f 84 63 01
    mov dx, 003c4h                            ; ba c4 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    movzx bx, al                              ; 0f b6 d8
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f18h                               ; e8 8c 06
    inc cx                                    ; 41
    mov dx, di                                ; 89 fa
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    movzx bx, al                              ; 0f b6 d8
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f18h                               ; e8 7c 06
    inc cx                                    ; 41
    mov dx, 003ceh                            ; ba ce 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    movzx bx, al                              ; 0f b6 d8
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f18h                               ; e8 6b 06
    inc cx                                    ; 41
    mov dx, 003dah                            ; ba da 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov dx, 003c0h                            ; ba c0 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov word [bp-002h], ax                    ; 89 46 fe
    movzx bx, byte [bp-002h]                  ; 0f b6 5e fe
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f18h                               ; e8 50 06
    inc cx                                    ; 41
    mov dx, 003cah                            ; ba ca 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    movzx bx, al                              ; 0f b6 d8
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f18h                               ; e8 3f 06
    mov ax, strict word 00001h                ; b8 01 00
    mov word [bp-004h], ax                    ; 89 46 fc
    add cx, ax                                ; 01 c1
    jmp short 028e9h                          ; eb 06
    cmp word [bp-004h], strict byte 00004h    ; 83 7e fc 04
    jnbe short 02906h                         ; 77 1d
    mov al, byte [bp-004h]                    ; 8a 46 fc
    mov dx, 003c4h                            ; ba c4 03
    out DX, AL                                ; ee
    mov dx, 003c5h                            ; ba c5 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    movzx bx, al                              ; 0f b6 d8
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f18h                               ; e8 18 06
    inc cx                                    ; 41
    inc word [bp-004h]                        ; ff 46 fc
    jmp short 028e3h                          ; eb dd
    xor al, al                                ; 30 c0
    mov dx, 003c4h                            ; ba c4 03
    out DX, AL                                ; ee
    mov dx, 003c5h                            ; ba c5 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    movzx bx, al                              ; 0f b6 d8
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f18h                               ; e8 fc 05
    mov word [bp-004h], strict word 00000h    ; c7 46 fc 00 00
    inc cx                                    ; 41
    jmp short 0292ah                          ; eb 06
    cmp word [bp-004h], strict byte 00018h    ; 83 7e fc 18
    jnbe short 02946h                         ; 77 1c
    mov al, byte [bp-004h]                    ; 8a 46 fc
    mov dx, di                                ; 89 fa
    out DX, AL                                ; ee
    lea dx, [di+001h]                         ; 8d 55 01
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    movzx bx, al                              ; 0f b6 d8
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f18h                               ; e8 d8 05
    inc cx                                    ; 41
    inc word [bp-004h]                        ; ff 46 fc
    jmp short 02924h                          ; eb de
    mov word [bp-004h], strict word 00000h    ; c7 46 fc 00 00
    jmp short 02953h                          ; eb 06
    cmp word [bp-004h], strict byte 00013h    ; 83 7e fc 13
    jnbe short 0297ch                         ; 77 29
    mov dx, 003dah                            ; ba da 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov ax, word [bp-002h]                    ; 8b 46 fe
    and ax, strict word 00020h                ; 25 20 00
    or ax, word [bp-004h]                     ; 0b 46 fc
    mov dx, 003c0h                            ; ba c0 03
    out DX, AL                                ; ee
    mov dx, 003c1h                            ; ba c1 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    movzx bx, al                              ; 0f b6 d8
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f18h                               ; e8 a2 05
    inc cx                                    ; 41
    inc word [bp-004h]                        ; ff 46 fc
    jmp short 0294dh                          ; eb d1
    mov dx, 003dah                            ; ba da 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov word [bp-004h], strict word 00000h    ; c7 46 fc 00 00
    jmp short 0298fh                          ; eb 06
    cmp word [bp-004h], strict byte 00008h    ; 83 7e fc 08
    jnbe short 029ach                         ; 77 1d
    mov al, byte [bp-004h]                    ; 8a 46 fc
    mov dx, 003ceh                            ; ba ce 03
    out DX, AL                                ; ee
    mov dx, 003cfh                            ; ba cf 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    movzx bx, al                              ; 0f b6 d8
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f18h                               ; e8 72 05
    inc cx                                    ; 41
    inc word [bp-004h]                        ; ff 46 fc
    jmp short 02989h                          ; eb dd
    mov bx, di                                ; 89 fb
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f34h                               ; e8 7f 05
    inc cx                                    ; 41
    inc cx                                    ; 41
    xor bx, bx                                ; 31 db
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f18h                               ; e8 58 05
    inc cx                                    ; 41
    xor bx, bx                                ; 31 db
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f18h                               ; e8 4e 05
    inc cx                                    ; 41
    xor bx, bx                                ; 31 db
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f18h                               ; e8 44 05
    inc cx                                    ; 41
    xor bx, bx                                ; 31 db
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f18h                               ; e8 3a 05
    inc cx                                    ; 41
    test byte [bp-006h], 002h                 ; f6 46 fa 02
    je near 02b4eh                            ; 0f 84 67 01
    mov dx, strict word 00049h                ; ba 49 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f0ah                               ; e8 1a 05
    movzx bx, al                              ; 0f b6 d8
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f18h                               ; e8 1e 05
    inc cx                                    ; 41
    mov dx, strict word 0004ah                ; ba 4a 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f26h                               ; e8 22 05
    mov bx, ax                                ; 89 c3
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f34h                               ; e8 27 05
    inc cx                                    ; 41
    inc cx                                    ; 41
    mov dx, strict word 0004ch                ; ba 4c 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f26h                               ; e8 0e 05
    mov bx, ax                                ; 89 c3
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f34h                               ; e8 13 05
    inc cx                                    ; 41
    inc cx                                    ; 41
    mov dx, strict word 00063h                ; ba 63 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f26h                               ; e8 fa 04
    mov bx, ax                                ; 89 c3
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f34h                               ; e8 ff 04
    inc cx                                    ; 41
    inc cx                                    ; 41
    mov dx, 00084h                            ; ba 84 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f0ah                               ; e8 ca 04
    movzx bx, al                              ; 0f b6 d8
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f18h                               ; e8 ce 04
    inc cx                                    ; 41
    mov dx, 00085h                            ; ba 85 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f26h                               ; e8 d2 04
    mov bx, ax                                ; 89 c3
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f34h                               ; e8 d7 04
    inc cx                                    ; 41
    inc cx                                    ; 41
    mov dx, 00087h                            ; ba 87 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f0ah                               ; e8 a2 04
    movzx bx, al                              ; 0f b6 d8
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f18h                               ; e8 a6 04
    inc cx                                    ; 41
    mov dx, 00088h                            ; ba 88 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f0ah                               ; e8 8e 04
    movzx bx, al                              ; 0f b6 d8
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f18h                               ; e8 92 04
    inc cx                                    ; 41
    mov dx, 00089h                            ; ba 89 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f0ah                               ; e8 7a 04
    movzx bx, al                              ; 0f b6 d8
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f18h                               ; e8 7e 04
    inc cx                                    ; 41
    mov dx, strict word 00060h                ; ba 60 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f26h                               ; e8 82 04
    mov bx, ax                                ; 89 c3
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f34h                               ; e8 87 04
    mov word [bp-004h], strict word 00000h    ; c7 46 fc 00 00
    inc cx                                    ; 41
    inc cx                                    ; 41
    jmp short 02abch                          ; eb 06
    cmp word [bp-004h], strict byte 00008h    ; 83 7e fc 08
    jnc short 02adah                          ; 73 1e
    mov dx, word [bp-004h]                    ; 8b 56 fc
    add dx, dx                                ; 01 d2
    add dx, strict byte 00050h                ; 83 c2 50
    mov ax, strict word 00040h                ; b8 40 00
    call 02f26h                               ; e8 5c 04
    mov bx, ax                                ; 89 c3
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f34h                               ; e8 61 04
    inc cx                                    ; 41
    inc cx                                    ; 41
    inc word [bp-004h]                        ; ff 46 fc
    jmp short 02ab6h                          ; eb dc
    mov dx, strict word 0004eh                ; ba 4e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f26h                               ; e8 43 04
    mov bx, ax                                ; 89 c3
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f34h                               ; e8 48 04
    inc cx                                    ; 41
    inc cx                                    ; 41
    mov dx, strict word 00062h                ; ba 62 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f0ah                               ; e8 13 04
    movzx bx, al                              ; 0f b6 d8
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f18h                               ; e8 17 04
    inc cx                                    ; 41
    mov dx, strict word 0007ch                ; ba 7c 00
    xor ax, ax                                ; 31 c0
    call 02f26h                               ; e8 1c 04
    mov bx, ax                                ; 89 c3
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f34h                               ; e8 21 04
    inc cx                                    ; 41
    inc cx                                    ; 41
    mov dx, strict word 0007eh                ; ba 7e 00
    xor ax, ax                                ; 31 c0
    call 02f26h                               ; e8 09 04
    mov bx, ax                                ; 89 c3
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f34h                               ; e8 0e 04
    inc cx                                    ; 41
    inc cx                                    ; 41
    mov dx, 0010ch                            ; ba 0c 01
    xor ax, ax                                ; 31 c0
    call 02f26h                               ; e8 f6 03
    mov bx, ax                                ; 89 c3
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f34h                               ; e8 fb 03
    inc cx                                    ; 41
    inc cx                                    ; 41
    mov dx, 0010eh                            ; ba 0e 01
    xor ax, ax                                ; 31 c0
    call 02f26h                               ; e8 e3 03
    mov bx, ax                                ; 89 c3
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f34h                               ; e8 e8 03
    inc cx                                    ; 41
    inc cx                                    ; 41
    test byte [bp-006h], 004h                 ; f6 46 fa 04
    je short 02bbbh                           ; 74 67
    mov dx, 003c7h                            ; ba c7 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    movzx bx, al                              ; 0f b6 d8
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f18h                               ; e8 b4 03
    inc cx                                    ; 41
    mov dx, 003c8h                            ; ba c8 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    movzx bx, al                              ; 0f b6 d8
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f18h                               ; e8 a3 03
    inc cx                                    ; 41
    mov dx, 003c6h                            ; ba c6 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    movzx bx, al                              ; 0f b6 d8
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f18h                               ; e8 92 03
    inc cx                                    ; 41
    xor al, al                                ; 30 c0
    mov dx, 003c8h                            ; ba c8 03
    out DX, AL                                ; ee
    xor ah, ah                                ; 30 e4
    mov word [bp-004h], ax                    ; 89 46 fc
    jmp short 02b9bh                          ; eb 07
    cmp word [bp-004h], 00300h                ; 81 7e fc 00 03
    jnc short 02bb1h                          ; 73 16
    mov dx, 003c9h                            ; ba c9 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    movzx bx, al                              ; 0f b6 d8
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f18h                               ; e8 6d 03
    inc cx                                    ; 41
    inc word [bp-004h]                        ; ff 46 fc
    jmp short 02b94h                          ; eb e3
    xor bx, bx                                ; 31 db
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f18h                               ; e8 5e 03
    inc cx                                    ; 41
    mov ax, cx                                ; 89 c8
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop cx                                    ; 59
    retn                                      ; c3
biosfn_restore_video_state_:                 ; 0xc2bc2 LB 0x321
    push cx                                   ; 51
    push si                                   ; 56
    push di                                   ; 57
    enter 00006h, 000h                        ; c8 06 00 00
    push ax                                   ; 50
    mov si, dx                                ; 89 d6
    mov cx, bx                                ; 89 d9
    test byte [bp-008h], 001h                 ; f6 46 f8 01
    je near 02d20h                            ; 0f 84 4a 01
    mov dx, 003dah                            ; ba da 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    lea dx, [bx+040h]                         ; 8d 57 40
    mov ax, si                                ; 89 f0
    call 02f26h                               ; e8 42 03
    mov di, ax                                ; 89 c7
    mov word [bp-002h], strict word 00001h    ; c7 46 fe 01 00
    lea cx, [bx+005h]                         ; 8d 4f 05
    jmp short 02bf6h                          ; eb 06
    cmp word [bp-002h], strict byte 00004h    ; 83 7e fe 04
    jnbe short 02c0eh                         ; 77 18
    mov al, byte [bp-002h]                    ; 8a 46 fe
    mov dx, 003c4h                            ; ba c4 03
    out DX, AL                                ; ee
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f0ah                               ; e8 06 03
    mov dx, 003c5h                            ; ba c5 03
    out DX, AL                                ; ee
    inc cx                                    ; 41
    inc word [bp-002h]                        ; ff 46 fe
    jmp short 02bf0h                          ; eb e2
    xor al, al                                ; 30 c0
    mov dx, 003c4h                            ; ba c4 03
    out DX, AL                                ; ee
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f0ah                               ; e8 ef 02
    mov dx, 003c5h                            ; ba c5 03
    out DX, AL                                ; ee
    inc cx                                    ; 41
    mov ax, strict word 00011h                ; b8 11 00
    mov dx, di                                ; 89 fa
    out DX, ax                                ; ef
    mov word [bp-002h], strict word 00000h    ; c7 46 fe 00 00
    jmp short 02c33h                          ; eb 06
    cmp word [bp-002h], strict byte 00018h    ; 83 7e fe 18
    jnbe short 02c50h                         ; 77 1d
    cmp word [bp-002h], strict byte 00011h    ; 83 7e fe 11
    je short 02c4ah                           ; 74 11
    mov al, byte [bp-002h]                    ; 8a 46 fe
    mov dx, di                                ; 89 fa
    out DX, AL                                ; ee
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f0ah                               ; e8 c4 02
    lea dx, [di+001h]                         ; 8d 55 01
    out DX, AL                                ; ee
    inc cx                                    ; 41
    inc word [bp-002h]                        ; ff 46 fe
    jmp short 02c2dh                          ; eb dd
    mov dx, 003cch                            ; ba cc 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    and AL, strict byte 0feh                  ; 24 fe
    mov word [bp-004h], ax                    ; 89 46 fc
    cmp di, 003d4h                            ; 81 ff d4 03
    jne short 02c65h                          ; 75 04
    or byte [bp-004h], 001h                   ; 80 4e fc 01
    mov al, byte [bp-004h]                    ; 8a 46 fc
    mov dx, 003c2h                            ; ba c2 03
    out DX, AL                                ; ee
    mov AL, strict byte 011h                  ; b0 11
    mov dx, di                                ; 89 fa
    out DX, AL                                ; ee
    mov dx, cx                                ; 89 ca
    add dx, strict byte 0fff9h                ; 83 c2 f9
    mov ax, si                                ; 89 f0
    call 02f0ah                               ; e8 8f 02
    lea dx, [di+001h]                         ; 8d 55 01
    out DX, AL                                ; ee
    lea dx, [bx+003h]                         ; 8d 57 03
    mov ax, si                                ; 89 f0
    call 02f0ah                               ; e8 83 02
    xor ah, ah                                ; 30 e4
    mov word [bp-006h], ax                    ; 89 46 fa
    mov dx, 003dah                            ; ba da 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov word [bp-002h], strict word 00000h    ; c7 46 fe 00 00
    jmp short 02c9fh                          ; eb 06
    cmp word [bp-002h], strict byte 00013h    ; 83 7e fe 13
    jnbe short 02cbdh                         ; 77 1e
    mov ax, word [bp-006h]                    ; 8b 46 fa
    and ax, strict word 00020h                ; 25 20 00
    or ax, word [bp-002h]                     ; 0b 46 fe
    mov dx, 003c0h                            ; ba c0 03
    out DX, AL                                ; ee
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f0ah                               ; e8 57 02
    mov dx, 003c0h                            ; ba c0 03
    out DX, AL                                ; ee
    inc cx                                    ; 41
    inc word [bp-002h]                        ; ff 46 fe
    jmp short 02c99h                          ; eb dc
    mov al, byte [bp-006h]                    ; 8a 46 fa
    mov dx, 003c0h                            ; ba c0 03
    out DX, AL                                ; ee
    mov dx, 003dah                            ; ba da 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov word [bp-002h], strict word 00000h    ; c7 46 fe 00 00
    jmp short 02cd7h                          ; eb 06
    cmp word [bp-002h], strict byte 00008h    ; 83 7e fe 08
    jnbe short 02cefh                         ; 77 18
    mov al, byte [bp-002h]                    ; 8a 46 fe
    mov dx, 003ceh                            ; ba ce 03
    out DX, AL                                ; ee
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f0ah                               ; e8 25 02
    mov dx, 003cfh                            ; ba cf 03
    out DX, AL                                ; ee
    inc cx                                    ; 41
    inc word [bp-002h]                        ; ff 46 fe
    jmp short 02cd1h                          ; eb e2
    add cx, strict byte 00006h                ; 83 c1 06
    mov dx, bx                                ; 89 da
    mov ax, si                                ; 89 f0
    call 02f0ah                               ; e8 11 02
    mov dx, 003c4h                            ; ba c4 03
    out DX, AL                                ; ee
    inc bx                                    ; 43
    mov dx, bx                                ; 89 da
    mov ax, si                                ; 89 f0
    call 02f0ah                               ; e8 05 02
    mov dx, di                                ; 89 fa
    out DX, AL                                ; ee
    inc bx                                    ; 43
    mov dx, bx                                ; 89 da
    mov ax, si                                ; 89 f0
    call 02f0ah                               ; e8 fa 01
    mov dx, 003ceh                            ; ba ce 03
    out DX, AL                                ; ee
    lea dx, [bx+002h]                         ; 8d 57 02
    mov ax, si                                ; 89 f0
    call 02f0ah                               ; e8 ee 01
    lea dx, [di+006h]                         ; 8d 55 06
    out DX, AL                                ; ee
    test byte [bp-008h], 002h                 ; f6 46 f8 02
    je near 02e8fh                            ; 0f 84 67 01
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f0ah                               ; e8 db 01
    movzx bx, al                              ; 0f b6 d8
    mov dx, strict word 00049h                ; ba 49 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f18h                               ; e8 dd 01
    inc cx                                    ; 41
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f26h                               ; e8 e3 01
    mov bx, ax                                ; 89 c3
    mov dx, strict word 0004ah                ; ba 4a 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f34h                               ; e8 e6 01
    inc cx                                    ; 41
    inc cx                                    ; 41
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f26h                               ; e8 cf 01
    mov bx, ax                                ; 89 c3
    mov dx, strict word 0004ch                ; ba 4c 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f34h                               ; e8 d2 01
    inc cx                                    ; 41
    inc cx                                    ; 41
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f26h                               ; e8 bb 01
    mov bx, ax                                ; 89 c3
    mov dx, strict word 00063h                ; ba 63 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f34h                               ; e8 be 01
    inc cx                                    ; 41
    inc cx                                    ; 41
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f0ah                               ; e8 8b 01
    movzx bx, al                              ; 0f b6 d8
    mov dx, 00084h                            ; ba 84 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f18h                               ; e8 8d 01
    inc cx                                    ; 41
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f26h                               ; e8 93 01
    mov bx, ax                                ; 89 c3
    mov dx, 00085h                            ; ba 85 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f34h                               ; e8 96 01
    inc cx                                    ; 41
    inc cx                                    ; 41
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f0ah                               ; e8 63 01
    movzx bx, al                              ; 0f b6 d8
    mov dx, 00087h                            ; ba 87 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f18h                               ; e8 65 01
    inc cx                                    ; 41
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f0ah                               ; e8 4f 01
    movzx bx, al                              ; 0f b6 d8
    mov dx, 00088h                            ; ba 88 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f18h                               ; e8 51 01
    inc cx                                    ; 41
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f0ah                               ; e8 3b 01
    movzx bx, al                              ; 0f b6 d8
    mov dx, 00089h                            ; ba 89 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f18h                               ; e8 3d 01
    inc cx                                    ; 41
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f26h                               ; e8 43 01
    mov bx, ax                                ; 89 c3
    mov dx, strict word 00060h                ; ba 60 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f34h                               ; e8 46 01
    mov word [bp-002h], strict word 00000h    ; c7 46 fe 00 00
    inc cx                                    ; 41
    inc cx                                    ; 41
    jmp short 02dfdh                          ; eb 06
    cmp word [bp-002h], strict byte 00008h    ; 83 7e fe 08
    jnc short 02e1bh                          ; 73 1e
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f26h                               ; e8 22 01
    mov bx, ax                                ; 89 c3
    mov dx, word [bp-002h]                    ; 8b 56 fe
    add dx, dx                                ; 01 d2
    add dx, strict byte 00050h                ; 83 c2 50
    mov ax, strict word 00040h                ; b8 40 00
    call 02f34h                               ; e8 20 01
    inc cx                                    ; 41
    inc cx                                    ; 41
    inc word [bp-002h]                        ; ff 46 fe
    jmp short 02df7h                          ; eb dc
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f26h                               ; e8 04 01
    mov bx, ax                                ; 89 c3
    mov dx, strict word 0004eh                ; ba 4e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f34h                               ; e8 07 01
    inc cx                                    ; 41
    inc cx                                    ; 41
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f0ah                               ; e8 d4 00
    movzx bx, al                              ; 0f b6 d8
    mov dx, strict word 00062h                ; ba 62 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f18h                               ; e8 d6 00
    inc cx                                    ; 41
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f26h                               ; e8 dc 00
    mov bx, ax                                ; 89 c3
    mov dx, strict word 0007ch                ; ba 7c 00
    xor ax, ax                                ; 31 c0
    call 02f34h                               ; e8 e0 00
    inc cx                                    ; 41
    inc cx                                    ; 41
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f26h                               ; e8 c9 00
    mov bx, ax                                ; 89 c3
    mov dx, strict word 0007eh                ; ba 7e 00
    xor ax, ax                                ; 31 c0
    call 02f34h                               ; e8 cd 00
    inc cx                                    ; 41
    inc cx                                    ; 41
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f26h                               ; e8 b6 00
    mov bx, ax                                ; 89 c3
    mov dx, 0010ch                            ; ba 0c 01
    xor ax, ax                                ; 31 c0
    call 02f34h                               ; e8 ba 00
    inc cx                                    ; 41
    inc cx                                    ; 41
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f26h                               ; e8 a3 00
    mov bx, ax                                ; 89 c3
    mov dx, 0010eh                            ; ba 0e 01
    xor ax, ax                                ; 31 c0
    call 02f34h                               ; e8 a7 00
    inc cx                                    ; 41
    inc cx                                    ; 41
    test byte [bp-008h], 004h                 ; f6 46 f8 04
    je short 02edch                           ; 74 47
    inc cx                                    ; 41
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f0ah                               ; e8 6d 00
    xor ah, ah                                ; 30 e4
    mov word [bp-004h], ax                    ; 89 46 fc
    inc cx                                    ; 41
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f0ah                               ; e8 60 00
    mov dx, 003c6h                            ; ba c6 03
    out DX, AL                                ; ee
    inc cx                                    ; 41
    xor al, al                                ; 30 c0
    mov dx, 003c8h                            ; ba c8 03
    out DX, AL                                ; ee
    xor ah, ah                                ; 30 e4
    mov word [bp-002h], ax                    ; 89 46 fe
    jmp short 02ec3h                          ; eb 07
    cmp word [bp-002h], 00300h                ; 81 7e fe 00 03
    jnc short 02ed4h                          ; 73 11
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 02f0ah                               ; e8 40 00
    mov dx, 003c9h                            ; ba c9 03
    out DX, AL                                ; ee
    inc cx                                    ; 41
    inc word [bp-002h]                        ; ff 46 fe
    jmp short 02ebch                          ; eb e8
    inc cx                                    ; 41
    mov al, byte [bp-004h]                    ; 8a 46 fc
    mov dx, 003c8h                            ; ba c8 03
    out DX, AL                                ; ee
    mov ax, cx                                ; 89 c8
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop cx                                    ; 59
    retn                                      ; c3
find_vga_entry_:                             ; 0xc2ee3 LB 0x27
    push bx                                   ; 53
    push dx                                   ; 52
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov dl, al                                ; 88 c2
    mov AH, strict byte 0ffh                  ; b4 ff
    xor al, al                                ; 30 c0
    jmp short 02ef6h                          ; eb 06
    db  0feh, 0c0h
    ; inc al                                    ; fe c0
    cmp AL, strict byte 00fh                  ; 3c 0f
    jnbe short 02f04h                         ; 77 0e
    movzx bx, al                              ; 0f b6 d8
    sal bx, 003h                              ; c1 e3 03
    cmp dl, byte [bx+0482fh]                  ; 3a 97 2f 48
    jne short 02ef0h                          ; 75 ee
    mov ah, al                                ; 88 c4
    mov al, ah                                ; 88 e0
    pop bp                                    ; 5d
    pop dx                                    ; 5a
    pop bx                                    ; 5b
    retn                                      ; c3
read_byte_:                                  ; 0xc2f0a LB 0xe
    push bx                                   ; 53
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bx, dx                                ; 89 d3
    mov es, ax                                ; 8e c0
    mov al, byte [es:bx]                      ; 26 8a 07
    pop bp                                    ; 5d
    pop bx                                    ; 5b
    retn                                      ; c3
write_byte_:                                 ; 0xc2f18 LB 0xe
    push si                                   ; 56
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov si, dx                                ; 89 d6
    mov es, ax                                ; 8e c0
    mov byte [es:si], bl                      ; 26 88 1c
    pop bp                                    ; 5d
    pop si                                    ; 5e
    retn                                      ; c3
read_word_:                                  ; 0xc2f26 LB 0xe
    push bx                                   ; 53
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bx, dx                                ; 89 d3
    mov es, ax                                ; 8e c0
    mov ax, word [es:bx]                      ; 26 8b 07
    pop bp                                    ; 5d
    pop bx                                    ; 5b
    retn                                      ; c3
write_word_:                                 ; 0xc2f34 LB 0xe
    push si                                   ; 56
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov si, dx                                ; 89 d6
    mov es, ax                                ; 8e c0
    mov word [es:si], bx                      ; 26 89 1c
    pop bp                                    ; 5d
    pop si                                    ; 5e
    retn                                      ; c3
read_dword_:                                 ; 0xc2f42 LB 0x12
    push bx                                   ; 53
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bx, dx                                ; 89 d3
    mov es, ax                                ; 8e c0
    mov ax, word [es:bx]                      ; 26 8b 07
    mov dx, word [es:bx+002h]                 ; 26 8b 57 02
    pop bp                                    ; 5d
    pop bx                                    ; 5b
    retn                                      ; c3
printf_:                                     ; 0xc2f54 LB 0x108
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    push si                                   ; 56
    push di                                   ; 57
    enter 00004h, 000h                        ; c8 04 00 00
    lea si, [bp+00eh]                         ; 8d 76 0e
    xor cx, cx                                ; 31 c9
    xor bx, bx                                ; 31 db
    mov di, word [bp+00eh]                    ; 8b 7e 0e
    mov al, byte [di]                         ; 8a 05
    test al, al                               ; 84 c0
    je near 02fe8h                            ; 0f 84 79 00
    cmp AL, strict byte 025h                  ; 3c 25
    jne short 02f7ah                          ; 75 07
    mov cx, strict word 00001h                ; b9 01 00
    xor bx, bx                                ; 31 db
    jmp short 02fe2h                          ; eb 68
    test cx, cx                               ; 85 c9
    je short 02fdeh                           ; 74 60
    cmp AL, strict byte 030h                  ; 3c 30
    jc short 02f92h                           ; 72 10
    cmp AL, strict byte 039h                  ; 3c 39
    jnbe short 02f92h                         ; 77 0c
    xor ah, ah                                ; 30 e4
    imul bx, bx, strict byte 0000ah           ; 6b db 0a
    sub ax, strict word 00030h                ; 2d 30 00
    add bx, ax                                ; 01 c3
    jmp short 02fe2h                          ; eb 50
    cmp AL, strict byte 078h                  ; 3c 78
    jne short 02fe2h                          ; 75 4c
    inc si                                    ; 46
    inc si                                    ; 46
    mov ax, word [ss:si]                      ; 36 8b 04
    mov word [bp-004h], ax                    ; 89 46 fc
    test bx, bx                               ; 85 db
    jne short 02fa5h                          ; 75 03
    mov bx, strict word 00004h                ; bb 04 00
    lea di, [bx-001h]                         ; 8d 7f ff
    mov word [bp-002h], strict word 00000h    ; c7 46 fe 00 00
    cmp bx, word [bp-002h]                    ; 3b 5e fe
    jbe short 02fdah                          ; 76 28
    mov cx, di                                ; 89 f9
    sal cx, 002h                              ; c1 e1 02
    mov ax, word [bp-004h]                    ; 8b 46 fc
    shr ax, CL                                ; d3 e8
    and ax, strict word 0000fh                ; 25 0f 00
    cmp ax, strict word 00009h                ; 3d 09 00
    jnbe short 02fcdh                         ; 77 09
    add ax, strict word 00030h                ; 05 30 00
    mov dx, 00504h                            ; ba 04 05
    out DX, AL                                ; ee
    jmp short 02fd4h                          ; eb 07
    add ax, strict word 00037h                ; 05 37 00
    mov dx, 00504h                            ; ba 04 05
    out DX, AL                                ; ee
    dec di                                    ; 4f
    inc word [bp-002h]                        ; ff 46 fe
    jmp short 02fadh                          ; eb d3
    xor cx, cx                                ; 31 c9
    jmp short 02fe2h                          ; eb 04
    mov dx, 00504h                            ; ba 04 05
    out DX, AL                                ; ee
    inc word [bp+00eh]                        ; ff 46 0e
    jmp near 02f64h                           ; e9 7c ff
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
    dec di                                    ; 4f
    sbb AL, strict byte 01bh                  ; 1c 1b
    adc dx, word [bp+si]                      ; 13 12
    adc word [bx+si], dx                      ; 11 10
    push CS                                   ; 0e
    or ax, 00a0ch                             ; 0d 0c 0a
    or word [bx+si], cx                       ; 09 08
    pop ES                                    ; 07
    push ES                                   ; 06
    add ax, 00304h                            ; 05 04 03
    add al, byte [bx+di]                      ; 02 01
    add byte [bx], bl                         ; 00 1f
    xor AL, strict byte 08ah                  ; 34 8a
    xor al, cl                                ; 30 c8
    xor ah, bl                                ; 30 dc
    xor ch, ch                                ; 30 ed
    xor byte [bx+di], al                      ; 30 01
    xor word [bp+si], dx                      ; 31 12
    xor word [si], bx                         ; 31 1c
    xor word [bp+031h], dx                    ; 31 56 31
    pop dx                                    ; 5a
    xor word [bp+di+031h], bp                 ; 31 6b 31
    mov byte [bx+di], dh                      ; 88 31
    movsw                                     ; a5
    xor bp, ax                                ; 31 c5
    xor dx, sp                                ; 31 e2
    xor cx, di                                ; 31 f9
    xor word [di], ax                         ; 31 05
    db  032h, 0e0h
    ; xor ah, al                                ; 32 e0
    xor bl, byte [bp+di]                      ; 32 1b
    xor cx, word [bp+di+033h]                 ; 33 4b 33
    pushaw                                    ; 60
    xor sp, word [bp+si+03033h]               ; 33 a2 33 30
    and AL, strict byte 023h                  ; 24 23
    and ah, byte [bx+di]                      ; 22 21
    and byte [si], dl                         ; 20 14
    adc dl, byte [bx+di]                      ; 12 11
    adc byte [si], al                         ; 10 04
    add al, byte [bx+di]                      ; 02 01
    add byte [bx], bl                         ; 00 1f
    xor AL, strict byte 026h                  ; 34 26
    xor cl, byte [si+032h]                    ; 32 4c 32
    pop bp                                    ; 5d
    xor ch, byte [bp+032h]                    ; 32 6e 32
    xor cl, byte [es:si+032h]                 ; 26 32 4c 32
    pop bp                                    ; 5d
    xor ch, byte [bp+032h]                    ; 32 6e 32
    jnle short 03084h                         ; 7f 32
    mov si, word [bp+si]                      ; 8b 32
    cmpsb                                     ; a6
    xor dh, byte [bx+di-043ceh]               ; 32 b1 32 bc
    db  032h, 0c7h
    ; xor al, bh                                ; 32 c7
    db  032h
_int10_func:                                 ; 0xc305c LB 0x3c7
    push si                                   ; 56
    push di                                   ; 57
    enter 00002h, 000h                        ; c8 02 00 00
    mov si, word [bp+008h]                    ; 8b 76 08
    mov ax, word [bp+016h]                    ; 8b 46 16
    shr ax, 008h                              ; c1 e8 08
    cmp ax, strict word 0004fh                ; 3d 4f 00
    jnbe near 0341fh                          ; 0f 87 ad 03
    push CS                                   ; 0e
    pop ES                                    ; 07
    mov cx, strict word 00016h                ; b9 16 00
    mov di, 02fefh                            ; bf ef 2f
    repne scasb                               ; f2 ae
    sal cx, 1                                 ; d1 e1
    mov di, cx                                ; 89 cf
    mov ax, word [cs:di+03004h]               ; 2e 8b 85 04 30
    mov cl, byte [bp+016h]                    ; 8a 4e 16
    jmp ax                                    ; ff e0
    mov al, byte [bp+016h]                    ; 8a 46 16
    xor ah, ah                                ; 30 e4
    call 00fc1h                               ; e8 2f df
    mov ax, word [bp+016h]                    ; 8b 46 16
    and ax, strict word 0007fh                ; 25 7f 00
    cmp ax, strict word 00007h                ; 3d 07 00
    je short 030b2h                           ; 74 15
    cmp ax, strict word 00006h                ; 3d 06 00
    je short 030a9h                           ; 74 07
    cmp ax, strict word 00005h                ; 3d 05 00
    jbe short 030b2h                          ; 76 0b
    jmp short 030bbh                          ; eb 12
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor al, al                                ; 30 c0
    or AL, strict byte 03fh                   ; 0c 3f
    jmp short 030c2h                          ; eb 10
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor al, al                                ; 30 c0
    or AL, strict byte 030h                   ; 0c 30
    jmp short 030c2h                          ; eb 07
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor al, al                                ; 30 c0
    or AL, strict byte 020h                   ; 0c 20
    mov word [bp+016h], ax                    ; 89 46 16
    jmp near 0341fh                           ; e9 57 03
    mov al, byte [bp+014h]                    ; 8a 46 14
    movzx dx, al                              ; 0f b6 d0
    mov ax, word [bp+014h]                    ; 8b 46 14
    shr ax, 008h                              ; c1 e8 08
    xor ah, ah                                ; 30 e4
    call 00daah                               ; e8 d1 dc
    jmp near 0341fh                           ; e9 43 03
    mov dx, word [bp+012h]                    ; 8b 56 12
    mov ax, word [bp+010h]                    ; 8b 46 10
    shr ax, 008h                              ; c1 e8 08
    xor ah, ah                                ; 30 e4
    call 00e4bh                               ; e8 61 dd
    jmp near 0341fh                           ; e9 32 03
    lea bx, [bp+012h]                         ; 8d 5e 12
    lea dx, [bp+014h]                         ; 8d 56 14
    mov ax, word [bp+010h]                    ; 8b 46 10
    shr ax, 008h                              ; c1 e8 08
    xor ah, ah                                ; 30 e4
    call 00a8ch                               ; e8 8e d9
    jmp near 0341fh                           ; e9 1e 03
    xor ax, ax                                ; 31 c0
    mov word [bp+016h], ax                    ; 89 46 16
    mov word [bp+010h], ax                    ; 89 46 10
    mov word [bp+014h], ax                    ; 89 46 14
    mov word [bp+012h], ax                    ; 89 46 12
    jmp near 0341fh                           ; e9 0d 03
    mov al, cl                                ; 88 c8
    xor ah, ah                                ; 30 e4
    call 00ee9h                               ; e8 d0 dd
    jmp near 0341fh                           ; e9 03 03
    mov ax, strict word 00001h                ; b8 01 00
    push ax                                   ; 50
    mov ax, 000ffh                            ; b8 ff 00
    push ax                                   ; 50
    mov al, byte [bp+012h]                    ; 8a 46 12
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov ax, word [bp+012h]                    ; 8b 46 12
    shr ax, 008h                              ; c1 e8 08
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov al, byte [bp+014h]                    ; 8a 46 14
    movzx cx, al                              ; 0f b6 c8
    mov ax, word [bp+014h]                    ; 8b 46 14
    shr ax, 008h                              ; c1 e8 08
    movzx bx, al                              ; 0f b6 d8
    mov ax, word [bp+010h]                    ; 8b 46 10
    shr ax, 008h                              ; c1 e8 08
    movzx dx, al                              ; 0f b6 d0
    mov al, byte [bp+016h]                    ; 8a 46 16
    xor ah, ah                                ; 30 e4
    call 01542h                               ; e8 ef e3
    jmp near 0341fh                           ; e9 c9 02
    xor ax, ax                                ; 31 c0
    jmp short 0311fh                          ; eb c5
    lea dx, [bp+016h]                         ; 8d 56 16
    mov ax, word [bp+010h]                    ; 8b 46 10
    shr ax, 008h                              ; c1 e8 08
    xor ah, ah                                ; 30 e4
    call 00acch                               ; e8 64 d9
    jmp near 0341fh                           ; e9 b4 02
    mov cx, word [bp+014h]                    ; 8b 4e 14
    mov al, byte [bp+010h]                    ; 8a 46 10
    movzx bx, al                              ; 0f b6 d8
    mov ax, word [bp+010h]                    ; 8b 46 10
    shr ax, 008h                              ; c1 e8 08
    movzx dx, al                              ; 0f b6 d0
    mov al, byte [bp+016h]                    ; 8a 46 16
    xor ah, ah                                ; 30 e4
    call 01cceh                               ; e8 49 eb
    jmp near 0341fh                           ; e9 97 02
    mov cx, word [bp+014h]                    ; 8b 4e 14
    mov al, byte [bp+010h]                    ; 8a 46 10
    movzx bx, al                              ; 0f b6 d8
    mov ax, word [bp+010h]                    ; 8b 46 10
    shr ax, 008h                              ; c1 e8 08
    movzx dx, al                              ; 0f b6 d0
    mov al, byte [bp+016h]                    ; 8a 46 16
    xor ah, ah                                ; 30 e4
    call 01e31h                               ; e8 8f ec
    jmp near 0341fh                           ; e9 7a 02
    mov cx, word [bp+012h]                    ; 8b 4e 12
    mov bx, word [bp+014h]                    ; 8b 5e 14
    mov al, byte [bp+016h]                    ; 8a 46 16
    movzx dx, al                              ; 0f b6 d0
    mov ax, word [bp+010h]                    ; 8b 46 10
    shr ax, 008h                              ; c1 e8 08
    mov word [bp-002h], ax                    ; 89 46 fe
    mov al, byte [bp-002h]                    ; 8a 46 fe
    xor ah, ah                                ; 30 e4
    call 01f9bh                               ; e8 d9 ed
    jmp near 0341fh                           ; e9 5a 02
    lea cx, [bp+016h]                         ; 8d 4e 16
    mov bx, word [bp+012h]                    ; 8b 5e 12
    mov dx, word [bp+014h]                    ; 8b 56 14
    mov ax, word [bp+010h]                    ; 8b 46 10
    shr ax, 008h                              ; c1 e8 08
    mov word [bp-002h], ax                    ; 89 46 fe
    mov al, byte [bp-002h]                    ; 8a 46 fe
    xor ah, ah                                ; 30 e4
    call 00beeh                               ; e8 0f da
    jmp near 0341fh                           ; e9 3d 02
    mov cx, strict word 00002h                ; b9 02 00
    mov al, byte [bp+010h]                    ; 8a 46 10
    movzx bx, al                              ; 0f b6 d8
    mov dx, 000ffh                            ; ba ff 00
    mov al, byte [bp+016h]                    ; 8a 46 16
    xor ah, ah                                ; 30 e4
    call 02103h                               ; e8 0d ef
    jmp near 0341fh                           ; e9 26 02
    mov dx, word [bp+014h]                    ; 8b 56 14
    mov ax, word [bp+010h]                    ; 8b 46 10
    call 00d22h                               ; e8 20 db
    jmp near 0341fh                           ; e9 1a 02
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    cmp ax, strict word 00030h                ; 3d 30 00
    jnbe near 0341fh                          ; 0f 87 0e 02
    push CS                                   ; 0e
    pop ES                                    ; 07
    mov cx, strict word 0000fh                ; b9 0f 00
    mov di, 03030h                            ; bf 30 30
    repne scasb                               ; f2 ae
    sal cx, 1                                 ; d1 e1
    mov di, cx                                ; 89 cf
    mov ax, word [cs:di+0303eh]               ; 2e 8b 85 3e 30
    jmp ax                                    ; ff e0
    mov ax, word [bp+010h]                    ; 8b 46 10
    shr ax, 008h                              ; c1 e8 08
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov al, byte [bp+010h]                    ; 8a 46 10
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    push word [bp+012h]                       ; ff 76 12
    mov al, byte [bp+016h]                    ; 8a 46 16
    xor ah, ah                                ; 30 e4
    mov cx, word [bp+014h]                    ; 8b 4e 14
    mov bx, word [bp+00ch]                    ; 8b 5e 0c
    mov dx, word [bp+01ah]                    ; 8b 56 1a
    call 024a0h                               ; e8 57 f2
    jmp near 0341fh                           ; e9 d3 01
    mov al, byte [bp+010h]                    ; 8a 46 10
    movzx dx, al                              ; 0f b6 d0
    mov al, byte [bp+016h]                    ; 8a 46 16
    xor ah, ah                                ; 30 e4
    call 02518h                               ; e8 be f2
    jmp near 0341fh                           ; e9 c2 01
    mov al, byte [bp+010h]                    ; 8a 46 10
    movzx dx, al                              ; 0f b6 d0
    mov al, byte [bp+016h]                    ; 8a 46 16
    xor ah, ah                                ; 30 e4
    call 02584h                               ; e8 19 f3
    jmp near 0341fh                           ; e9 b1 01
    mov al, byte [bp+010h]                    ; 8a 46 10
    movzx dx, al                              ; 0f b6 d0
    mov al, byte [bp+016h]                    ; 8a 46 16
    xor ah, ah                                ; 30 e4
    call 025f2h                               ; e8 76 f3
    jmp near 0341fh                           ; e9 a0 01
    mov dx, word [bp+00ch]                    ; 8b 56 0c
    mov ax, word [bp+01ah]                    ; 8b 46 1a
    call 02660h                               ; e8 d8 f3
    jmp near 0341fh                           ; e9 94 01
    mov al, byte [bp+012h]                    ; 8a 46 12
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov al, byte [bp+010h]                    ; 8a 46 10
    movzx cx, al                              ; 0f b6 c8
    mov bx, word [bp+014h]                    ; 8b 5e 14
    mov dx, word [bp+00ch]                    ; 8b 56 0c
    mov ax, word [bp+01ah]                    ; 8b 46 1a
    call 02665h                               ; e8 c2 f3
    jmp near 0341fh                           ; e9 79 01
    mov al, byte [bp+010h]                    ; 8a 46 10
    xor ah, ah                                ; 30 e4
    call 0266ch                               ; e8 be f3
    jmp near 0341fh                           ; e9 6e 01
    mov al, byte [bp+010h]                    ; 8a 46 10
    xor ah, ah                                ; 30 e4
    call 02671h                               ; e8 b8 f3
    jmp near 0341fh                           ; e9 63 01
    mov al, byte [bp+010h]                    ; 8a 46 10
    xor ah, ah                                ; 30 e4
    call 02676h                               ; e8 b2 f3
    jmp near 0341fh                           ; e9 58 01
    lea ax, [bp+012h]                         ; 8d 46 12
    push ax                                   ; 50
    lea cx, [bp+014h]                         ; 8d 4e 14
    lea bx, [bp+00ch]                         ; 8d 5e 0c
    lea dx, [bp+01ah]                         ; 8d 56 1a
    mov ax, word [bp+010h]                    ; 8b 46 10
    shr ax, 008h                              ; c1 e8 08
    call 00b6fh                               ; e8 92 d8
    jmp near 0341fh                           ; e9 3f 01
    mov ax, word [bp+010h]                    ; 8b 46 10
    xor ah, ah                                ; 30 e4
    cmp ax, strict word 00036h                ; 3d 36 00
    je short 03312h                           ; 74 28
    cmp ax, strict word 00035h                ; 3d 35 00
    je short 032fch                           ; 74 0d
    cmp ax, strict word 00020h                ; 3d 20 00
    jne near 0341fh                           ; 0f 85 29 01
    call 0267bh                               ; e8 82 f3
    jmp near 0341fh                           ; e9 23 01
    movzx ax, cl                              ; 0f b6 c1
    mov bx, word [bp+012h]                    ; 8b 5e 12
    mov dx, word [bp+01ah]                    ; 8b 56 1a
    call 02680h                               ; e8 78 f3
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor al, al                                ; 30 c0
    or AL, strict byte 012h                   ; 0c 12
    jmp near 030c2h                           ; e9 b0 fd
    mov al, cl                                ; 88 c8
    xor ah, ah                                ; 30 e4
    call 02685h                               ; e8 6c f3
    jmp short 03308h                          ; eb ed
    push word [bp+00ch]                       ; ff 76 0c
    push word [bp+01ah]                       ; ff 76 1a
    mov al, byte [bp+012h]                    ; 8a 46 12
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov ax, word [bp+012h]                    ; 8b 46 12
    shr ax, 008h                              ; c1 e8 08
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov al, byte [bp+010h]                    ; 8a 46 10
    movzx bx, al                              ; 0f b6 d8
    mov ax, word [bp+010h]                    ; 8b 46 10
    shr ax, 008h                              ; c1 e8 08
    movzx dx, al                              ; 0f b6 d0
    movzx ax, cl                              ; 0f b6 c1
    mov cx, word [bp+014h]                    ; 8b 4e 14
    call 0268ah                               ; e8 42 f3
    jmp near 0341fh                           ; e9 d4 00
    mov bx, si                                ; 89 f3
    mov dx, word [bp+01ah]                    ; 8b 56 1a
    mov ax, word [bp+010h]                    ; 8b 46 10
    call 02721h                               ; e8 cb f3
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor al, al                                ; 30 c0
    or AL, strict byte 01bh                   ; 0c 1b
    jmp near 030c2h                           ; e9 62 fd
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    cmp ax, strict word 00002h                ; 3d 02 00
    je short 0338ch                           ; 74 22
    cmp ax, strict word 00001h                ; 3d 01 00
    je short 0337eh                           ; 74 0f
    test ax, ax                               ; 85 c0
    jne short 03398h                          ; 75 25
    lea dx, [bp+010h]                         ; 8d 56 10
    mov ax, word [bp+014h]                    ; 8b 46 14
    call 0284eh                               ; e8 d2 f4
    jmp short 03398h                          ; eb 1a
    mov bx, word [bp+010h]                    ; 8b 5e 10
    mov dx, word [bp+01ah]                    ; 8b 56 1a
    mov ax, word [bp+014h]                    ; 8b 46 14
    call 0285dh                               ; e8 d3 f4
    jmp short 03398h                          ; eb 0c
    mov bx, word [bp+010h]                    ; 8b 5e 10
    mov dx, word [bp+01ah]                    ; 8b 56 1a
    mov ax, word [bp+014h]                    ; 8b 46 14
    call 02bc2h                               ; e8 2a f8
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor al, al                                ; 30 c0
    or AL, strict byte 01ch                   ; 0c 1c
    jmp near 030c2h                           ; e9 20 fd
    call 007d4h                               ; e8 2f d4
    test ax, ax                               ; 85 c0
    je near 0341ah                            ; 0f 84 6f 00
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    cmp ax, strict word 00002h                ; 3d 02 00
    jc short 033c8h                           ; 72 13
    jbe short 033eeh                          ; 76 37
    cmp ax, strict word 0000ah                ; 3d 0a 00
    je short 03413h                           ; 74 57
    cmp ax, strict word 00009h                ; 3d 09 00
    je short 03413h                           ; 74 52
    cmp ax, strict word 00004h                ; 3d 04 00
    je short 033feh                           ; 74 38
    jmp short 03413h                          ; eb 4b
    cmp ax, strict word 00001h                ; 3d 01 00
    je short 033deh                           ; 74 11
    test ax, ax                               ; 85 c0
    jne short 03413h                          ; 75 42
    mov bx, si                                ; 89 f3
    mov dx, word [bp+01ah]                    ; 8b 56 1a
    lea ax, [bp+016h]                         ; 8d 46 16
    call 034e7h                               ; e8 0b 01
    jmp short 0341fh                          ; eb 41
    mov cx, si                                ; 89 f1
    mov bx, word [bp+01ah]                    ; 8b 5e 1a
    mov dx, word [bp+014h]                    ; 8b 56 14
    lea ax, [bp+016h]                         ; 8d 46 16
    call 036d1h                               ; e8 e5 02
    jmp short 0341fh                          ; eb 31
    mov cx, si                                ; 89 f1
    mov bx, word [bp+01ah]                    ; 8b 5e 1a
    mov dx, word [bp+010h]                    ; 8b 56 10
    lea ax, [bp+016h]                         ; 8d 46 16
    call 03781h                               ; e8 85 03
    jmp short 0341fh                          ; eb 21
    lea ax, [bp+010h]                         ; 8d 46 10
    push ax                                   ; 50
    mov cx, word [bp+01ah]                    ; 8b 4e 1a
    mov bx, word [bp+012h]                    ; 8b 5e 12
    mov dx, word [bp+014h]                    ; 8b 56 14
    lea ax, [bp+016h]                         ; 8d 46 16
    call 03970h                               ; e8 5f 05
    jmp short 0341fh                          ; eb 0c
    mov word [bp+016h], 00100h                ; c7 46 16 00 01
    jmp short 0341fh                          ; eb 05
    mov word [bp+016h], 00100h                ; c7 46 16 00 01
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn                                      ; c3
dispi_set_xres_:                             ; 0xc3423 LB 0x18
    push bx                                   ; 53
    push dx                                   ; 52
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bx, ax                                ; 89 c3
    mov ax, strict word 00001h                ; b8 01 00
    mov dx, 001ceh                            ; ba ce 01
    out DX, ax                                ; ef
    mov ax, bx                                ; 89 d8
    mov dx, 001cfh                            ; ba cf 01
    out DX, ax                                ; ef
    pop bp                                    ; 5d
    pop dx                                    ; 5a
    pop bx                                    ; 5b
    retn                                      ; c3
dispi_set_yres_:                             ; 0xc343b LB 0x18
    push bx                                   ; 53
    push dx                                   ; 52
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bx, ax                                ; 89 c3
    mov ax, strict word 00002h                ; b8 02 00
    mov dx, 001ceh                            ; ba ce 01
    out DX, ax                                ; ef
    mov ax, bx                                ; 89 d8
    mov dx, 001cfh                            ; ba cf 01
    out DX, ax                                ; ef
    pop bp                                    ; 5d
    pop dx                                    ; 5a
    pop bx                                    ; 5b
    retn                                      ; c3
dispi_set_bpp_:                              ; 0xc3453 LB 0x18
    push bx                                   ; 53
    push dx                                   ; 52
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bx, ax                                ; 89 c3
    mov ax, strict word 00003h                ; b8 03 00
    mov dx, 001ceh                            ; ba ce 01
    out DX, ax                                ; ef
    mov ax, bx                                ; 89 d8
    mov dx, 001cfh                            ; ba cf 01
    out DX, ax                                ; ef
    pop bp                                    ; 5d
    pop dx                                    ; 5a
    pop bx                                    ; 5b
    retn                                      ; c3
in_word_:                                    ; 0xc346b LB 0xf
    push bx                                   ; 53
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bx, ax                                ; 89 c3
    mov ax, dx                                ; 89 d0
    mov dx, bx                                ; 89 da
    out DX, ax                                ; ef
    in ax, DX                                 ; ed
    pop bp                                    ; 5d
    pop bx                                    ; 5b
    retn                                      ; c3
in_byte_:                                    ; 0xc347a LB 0x11
    push bx                                   ; 53
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bx, ax                                ; 89 c3
    mov ax, dx                                ; 89 d0
    mov dx, bx                                ; 89 da
    out DX, ax                                ; ef
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    pop bp                                    ; 5d
    pop bx                                    ; 5b
    retn                                      ; c3
mode_info_find_mode_:                        ; 0xc348b LB 0x5c
    push bx                                   ; 53
    push cx                                   ; 51
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov di, ax                                ; 89 c7
    mov si, dx                                ; 89 d6
    xor dx, dx                                ; 31 d2
    mov ax, 003b6h                            ; b8 b6 03
    call 0346bh                               ; e8 cd ff
    cmp ax, 077cch                            ; 3d cc 77
    je short 034afh                           ; 74 0c
    push ax                                   ; 50
    push 07ee2h                               ; 68 e2 7e
    call 02f54h                               ; e8 aa fa
    add sp, strict byte 00004h                ; 83 c4 04
    jmp short 034dfh                          ; eb 30
    mov bx, strict word 00004h                ; bb 04 00
    mov dx, bx                                ; 89 da
    mov ax, 003b6h                            ; b8 b6 03
    call 0346bh                               ; e8 b1 ff
    mov cx, ax                                ; 89 c1
    cmp cx, strict byte 0ffffh                ; 83 f9 ff
    je short 034dfh                           ; 74 1e
    lea dx, [bx+002h]                         ; 8d 57 02
    mov ax, 003b6h                            ; b8 b6 03
    call 0346bh                               ; e8 a1 ff
    cmp cx, di                                ; 39 f9
    jne short 034dah                          ; 75 0c
    test si, si                               ; 85 f6
    jne short 034d6h                          ; 75 04
    mov ax, bx                                ; 89 d8
    jmp short 034e1h                          ; eb 0b
    test AL, strict byte 080h                 ; a8 80
    jne short 034d2h                          ; 75 f8
    add bx, strict byte 00044h                ; 83 c3 44
    jmp short 034b2h                          ; eb d3
    xor ax, ax                                ; 31 c0
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
vbe_biosfn_return_controller_information_: ; 0xc34e7 LB 0x1ea
    push cx                                   ; 51
    push si                                   ; 56
    push di                                   ; 57
    enter 00006h, 000h                        ; c8 06 00 00
    push ax                                   ; 50
    mov si, dx                                ; 89 d6
    mov di, bx                                ; 89 df
    mov word [bp-004h], strict word 00022h    ; c7 46 fc 22 00
    call 005d7h                               ; e8 dc d0
    mov word [bp-002h], ax                    ; 89 46 fe
    xor dx, dx                                ; 31 d2
    mov ax, 003b6h                            ; b8 b6 03
    call 0346bh                               ; e8 65 ff
    cmp ax, 077cch                            ; 3d cc 77
    je short 03521h                           ; 74 16
    push SS                                   ; 16
    pop ES                                    ; 07
    mov bx, word [bp-008h]                    ; 8b 5e f8
    mov word [es:bx], 00100h                  ; 26 c7 07 00 01
    push 07efbh                               ; 68 fb 7e
    call 02f54h                               ; e8 39 fa
    add sp, strict byte 00002h                ; 83 c4 02
    jmp near 036cch                           ; e9 ab 01
    mov cx, strict word 00004h                ; b9 04 00
    mov word [bp-006h], strict word 00000h    ; c7 46 fa 00 00
    mov dx, di                                ; 89 fa
    mov ax, si                                ; 89 f0
    call 02f0ah                               ; e8 da f9
    cmp AL, strict byte 056h                  ; 3c 56
    jne short 03558h                          ; 75 24
    lea dx, [di+001h]                         ; 8d 55 01
    mov ax, si                                ; 89 f0
    call 02f0ah                               ; e8 ce f9
    cmp AL, strict byte 042h                  ; 3c 42
    jne short 03558h                          ; 75 18
    lea dx, [di+002h]                         ; 8d 55 02
    mov ax, si                                ; 89 f0
    call 02f0ah                               ; e8 c2 f9
    cmp AL, strict byte 045h                  ; 3c 45
    jne short 03558h                          ; 75 0c
    lea dx, [di+003h]                         ; 8d 55 03
    mov ax, si                                ; 89 f0
    call 02f0ah                               ; e8 b6 f9
    cmp AL, strict byte 032h                  ; 3c 32
    je short 03587h                           ; 74 2f
    mov dx, di                                ; 89 fa
    mov ax, si                                ; 89 f0
    call 02f0ah                               ; e8 ab f9
    cmp AL, strict byte 056h                  ; 3c 56
    jne short 0358ch                          ; 75 29
    lea dx, [di+001h]                         ; 8d 55 01
    mov ax, si                                ; 89 f0
    call 02f0ah                               ; e8 9f f9
    cmp AL, strict byte 045h                  ; 3c 45
    jne short 0358ch                          ; 75 1d
    lea dx, [di+002h]                         ; 8d 55 02
    mov ax, si                                ; 89 f0
    call 02f0ah                               ; e8 93 f9
    cmp AL, strict byte 053h                  ; 3c 53
    jne short 0358ch                          ; 75 11
    lea dx, [di+003h]                         ; 8d 55 03
    mov ax, si                                ; 89 f0
    call 02f0ah                               ; e8 87 f9
    cmp AL, strict byte 041h                  ; 3c 41
    jne short 0358ch                          ; 75 05
    mov word [bp-006h], strict word 00001h    ; c7 46 fa 01 00
    mov bx, strict word 00056h                ; bb 56 00
    mov dx, di                                ; 89 fa
    mov ax, si                                ; 89 f0
    call 02f18h                               ; e8 82 f9
    lea dx, [di+001h]                         ; 8d 55 01
    mov bx, strict word 00045h                ; bb 45 00
    mov ax, si                                ; 89 f0
    call 02f18h                               ; e8 77 f9
    lea dx, [di+002h]                         ; 8d 55 02
    mov bx, strict word 00053h                ; bb 53 00
    mov ax, si                                ; 89 f0
    call 02f18h                               ; e8 6c f9
    lea dx, [di+003h]                         ; 8d 55 03
    mov bx, strict word 00041h                ; bb 41 00
    mov ax, si                                ; 89 f0
    call 02f18h                               ; e8 61 f9
    lea dx, [di+004h]                         ; 8d 55 04
    mov bx, 00200h                            ; bb 00 02
    mov ax, si                                ; 89 f0
    call 02f34h                               ; e8 72 f9
    lea dx, [di+008h]                         ; 8d 55 08
    mov bx, 0c000h                            ; bb 00 c0
    mov ax, si                                ; 89 f0
    call 02f34h                               ; e8 67 f9
    lea dx, [di+006h]                         ; 8d 55 06
    mov bx, 07e20h                            ; bb 20 7e
    mov ax, si                                ; 89 f0
    call 02f34h                               ; e8 5c f9
    lea dx, [di+00ah]                         ; 8d 55 0a
    mov bx, strict word 00001h                ; bb 01 00
    mov ax, si                                ; 89 f0
    call 02f18h                               ; e8 35 f9
    lea dx, [di+00bh]                         ; 8d 55 0b
    xor bx, bx                                ; 31 db
    mov ax, si                                ; 89 f0
    call 02f18h                               ; e8 2b f9
    lea dx, [di+00ch]                         ; 8d 55 0c
    xor bx, bx                                ; 31 db
    mov ax, si                                ; 89 f0
    call 02f18h                               ; e8 21 f9
    lea dx, [di+00dh]                         ; 8d 55 0d
    xor bx, bx                                ; 31 db
    mov ax, si                                ; 89 f0
    call 02f18h                               ; e8 17 f9
    lea dx, [di+010h]                         ; 8d 55 10
    mov bx, si                                ; 89 f3
    mov ax, si                                ; 89 f0
    call 02f34h                               ; e8 29 f9
    lea bx, [di+022h]                         ; 8d 5d 22
    lea dx, [di+00eh]                         ; 8d 55 0e
    mov ax, si                                ; 89 f0
    call 02f34h                               ; e8 1e f9
    mov dx, strict word 0ffffh                ; ba ff ff
    mov ax, 003b6h                            ; b8 b6 03
    call 0346bh                               ; e8 4c fe
    lea dx, [di+012h]                         ; 8d 55 12
    mov bx, ax                                ; 89 c3
    mov ax, si                                ; 89 f0
    call 02f34h                               ; e8 0b f9
    cmp word [bp-006h], strict byte 00000h    ; 83 7e fa 00
    je short 0367ch                           ; 74 4d
    lea dx, [di+014h]                         ; 8d 55 14
    mov bx, strict word 00003h                ; bb 03 00
    mov ax, si                                ; 89 f0
    call 02f34h                               ; e8 fa f8
    lea dx, [di+018h]                         ; 8d 55 18
    mov bx, 0c000h                            ; bb 00 c0
    mov ax, si                                ; 89 f0
    call 02f34h                               ; e8 ef f8
    lea dx, [di+016h]                         ; 8d 55 16
    mov bx, 07e35h                            ; bb 35 7e
    mov ax, si                                ; 89 f0
    call 02f34h                               ; e8 e4 f8
    lea dx, [di+01ch]                         ; 8d 55 1c
    mov bx, 0c000h                            ; bb 00 c0
    mov ax, si                                ; 89 f0
    call 02f34h                               ; e8 d9 f8
    lea dx, [di+01ah]                         ; 8d 55 1a
    mov bx, 07e48h                            ; bb 48 7e
    mov ax, si                                ; 89 f0
    call 02f34h                               ; e8 ce f8
    lea dx, [di+020h]                         ; 8d 55 20
    mov bx, 0c000h                            ; bb 00 c0
    mov ax, si                                ; 89 f0
    call 02f34h                               ; e8 c3 f8
    lea dx, [di+01eh]                         ; 8d 55 1e
    mov bx, 07e69h                            ; bb 69 7e
    mov ax, si                                ; 89 f0
    call 02f34h                               ; e8 b8 f8
    mov dx, cx                                ; 89 ca
    add dx, strict byte 0001bh                ; 83 c2 1b
    mov ax, 003b6h                            ; b8 b6 03
    call 0347ah                               ; e8 f3 fd
    xor ah, ah                                ; 30 e4
    cmp ax, word [bp-002h]                    ; 3b 46 fe
    jnbe short 036a6h                         ; 77 18
    mov dx, cx                                ; 89 ca
    mov ax, 003b6h                            ; b8 b6 03
    call 0346bh                               ; e8 d5 fd
    mov dx, word [bp-004h]                    ; 8b 56 fc
    add dx, di                                ; 01 fa
    mov bx, ax                                ; 89 c3
    mov ax, si                                ; 89 f0
    call 02f34h                               ; e8 92 f8
    add word [bp-004h], strict byte 00002h    ; 83 46 fc 02
    add cx, strict byte 00044h                ; 83 c1 44
    mov dx, cx                                ; 89 ca
    mov ax, 003b6h                            ; b8 b6 03
    call 0346bh                               ; e8 ba fd
    cmp ax, strict word 0ffffh                ; 3d ff ff
    jne short 0367ch                          ; 75 c6
    mov dx, word [bp-004h]                    ; 8b 56 fc
    add dx, di                                ; 01 fa
    mov bx, ax                                ; 89 c3
    mov ax, si                                ; 89 f0
    call 02f34h                               ; e8 72 f8
    push SS                                   ; 16
    pop ES                                    ; 07
    mov bx, word [bp-008h]                    ; 8b 5e f8
    mov word [es:bx], strict word 0004fh      ; 26 c7 07 4f 00
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop cx                                    ; 59
    retn                                      ; c3
vbe_biosfn_return_mode_information_:         ; 0xc36d1 LB 0xb0
    push si                                   ; 56
    push di                                   ; 57
    enter 00004h, 000h                        ; c8 04 00 00
    push ax                                   ; 50
    mov ax, dx                                ; 89 d0
    mov si, bx                                ; 89 de
    mov word [bp-002h], cx                    ; 89 4e fe
    test dh, 040h                             ; f6 c6 40
    db  00fh, 095h, 0c2h
    ; setne dl                                  ; 0f 95 c2
    xor dh, dh                                ; 30 f6
    and ah, 001h                              ; 80 e4 01
    call 0348bh                               ; e8 9e fd
    mov word [bp-004h], ax                    ; 89 46 fc
    test ax, ax                               ; 85 c0
    je near 03772h                            ; 0f 84 7c 00
    mov cx, 00100h                            ; b9 00 01
    xor ax, ax                                ; 31 c0
    mov di, word [bp-002h]                    ; 8b 7e fe
    mov es, bx                                ; 8e c3
    cld                                       ; fc
    jcxz 03705h                               ; e3 02
    rep stosb                                 ; f3 aa
    xor cx, cx                                ; 31 c9
    jmp short 0370eh                          ; eb 05
    cmp cx, strict byte 00042h                ; 83 f9 42
    jnc short 0372bh                          ; 73 1d
    mov dx, word [bp-004h]                    ; 8b 56 fc
    inc dx                                    ; 42
    inc dx                                    ; 42
    add dx, cx                                ; 01 ca
    mov ax, 003b6h                            ; b8 b6 03
    call 0347ah                               ; e8 5f fd
    movzx bx, al                              ; 0f b6 d8
    mov dx, word [bp-002h]                    ; 8b 56 fe
    add dx, cx                                ; 01 ca
    mov ax, si                                ; 89 f0
    call 02f18h                               ; e8 f0 f7
    inc cx                                    ; 41
    jmp short 03709h                          ; eb de
    mov dx, word [bp-002h]                    ; 8b 56 fe
    inc dx                                    ; 42
    inc dx                                    ; 42
    mov ax, si                                ; 89 f0
    call 02f0ah                               ; e8 d5 f7
    test AL, strict byte 001h                 ; a8 01
    je short 03755h                           ; 74 1c
    mov dx, word [bp-002h]                    ; 8b 56 fe
    add dx, strict byte 0000ch                ; 83 c2 0c
    mov bx, 00635h                            ; bb 35 06
    mov ax, si                                ; 89 f0
    call 02f34h                               ; e8 ed f7
    mov dx, word [bp-002h]                    ; 8b 56 fe
    add dx, strict byte 0000eh                ; 83 c2 0e
    mov bx, 0c000h                            ; bb 00 c0
    mov ax, si                                ; 89 f0
    call 02f34h                               ; e8 df f7
    mov ax, strict word 0000bh                ; b8 0b 00
    mov dx, 001ceh                            ; ba ce 01
    out DX, ax                                ; ef
    mov dx, 001cfh                            ; ba cf 01
    in ax, DX                                 ; ed
    mov dx, word [bp-002h]                    ; 8b 56 fe
    add dx, strict byte 0002ah                ; 83 c2 2a
    mov bx, ax                                ; 89 c3
    mov ax, si                                ; 89 f0
    call 02f34h                               ; e8 c7 f7
    mov ax, strict word 0004fh                ; b8 4f 00
    jmp short 03775h                          ; eb 03
    mov ax, 00100h                            ; b8 00 01
    push SS                                   ; 16
    pop ES                                    ; 07
    mov bx, word [bp-006h]                    ; 8b 5e fa
    mov word [es:bx], ax                      ; 26 89 07
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn                                      ; c3
vbe_biosfn_set_mode_:                        ; 0xc3781 LB 0xf7
    push si                                   ; 56
    enter 00006h, 000h                        ; c8 06 00 00
    mov si, ax                                ; 89 c6
    mov word [bp-006h], dx                    ; 89 56 fa
    test byte [bp-005h], 040h                 ; f6 46 fb 40
    db  00fh, 095h, 0c0h
    ; setne al                                  ; 0f 95 c0
    xor ah, ah                                ; 30 e4
    mov dx, ax                                ; 89 c2
    test ax, ax                               ; 85 c0
    je short 0379dh                           ; 74 03
    mov ax, strict word 00040h                ; b8 40 00
    mov byte [bp-002h], al                    ; 88 46 fe
    test byte [bp-005h], 080h                 ; f6 46 fb 80
    je short 037abh                           ; 74 05
    mov ax, 00080h                            ; b8 80 00
    jmp short 037adh                          ; eb 02
    xor ax, ax                                ; 31 c0
    mov byte [bp-004h], al                    ; 88 46 fc
    and byte [bp-005h], 001h                  ; 80 66 fb 01
    cmp word [bp-006h], 00100h                ; 81 7e fa 00 01
    jnc short 037cdh                          ; 73 12
    xor ax, ax                                ; 31 c0
    call 005f9h                               ; e8 39 ce
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa
    call 00fc1h                               ; e8 fa d7
    mov ax, strict word 0004fh                ; b8 4f 00
    jmp near 03872h                           ; e9 a5 00
    mov ax, word [bp-006h]                    ; 8b 46 fa
    call 0348bh                               ; e8 b8 fc
    mov bx, ax                                ; 89 c3
    mov cx, ax                                ; 89 c1
    test ax, ax                               ; 85 c0
    je near 0386fh                            ; 0f 84 92 00
    lea dx, [bx+014h]                         ; 8d 57 14
    mov ax, 003b6h                            ; b8 b6 03
    call 0346bh                               ; e8 85 fc
    lea dx, [bx+016h]                         ; 8d 57 16
    mov ax, 003b6h                            ; b8 b6 03
    call 0346bh                               ; e8 7c fc
    lea dx, [bx+01bh]                         ; 8d 57 1b
    mov ax, 003b6h                            ; b8 b6 03
    call 0347ah                               ; e8 82 fc
    mov dl, al                                ; 88 c2
    xor ax, ax                                ; 31 c0
    call 005f9h                               ; e8 fa cd
    cmp dl, 004h                              ; 80 fa 04
    jne short 0380ah                          ; 75 06
    mov ax, strict word 0006ah                ; b8 6a 00
    call 00fc1h                               ; e8 b7 d7
    mov dx, cx                                ; 89 ca
    add dx, strict byte 0001bh                ; 83 c2 1b
    mov ax, 003b6h                            ; b8 b6 03
    call 0347ah                               ; e8 65 fc
    xor ah, ah                                ; 30 e4
    call 03453h                               ; e8 39 fc
    mov dx, cx                                ; 89 ca
    add dx, strict byte 00014h                ; 83 c2 14
    mov ax, 003b6h                            ; b8 b6 03
    call 0346bh                               ; e8 46 fc
    call 03423h                               ; e8 fb fb
    mov dx, cx                                ; 89 ca
    add dx, strict byte 00016h                ; 83 c2 16
    mov ax, 003b6h                            ; b8 b6 03
    call 0346bh                               ; e8 38 fc
    call 0343bh                               ; e8 05 fc
    xor ax, ax                                ; 31 c0
    call 00617h                               ; e8 dc cd
    mov al, byte [bp-004h]                    ; 8a 46 fc
    or AL, strict byte 001h                   ; 0c 01
    movzx dx, al                              ; 0f b6 d0
    movzx ax, byte [bp-002h]                  ; 0f b6 46 fe
    or ax, dx                                 ; 09 d0
    call 005f9h                               ; e8 ad cd
    call 006f3h                               ; e8 a4 ce
    mov bx, word [bp-006h]                    ; 8b 5e fa
    mov dx, 000bah                            ; ba ba 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f34h                               ; e8 d9 f6
    mov al, byte [bp-004h]                    ; 8a 46 fc
    or AL, strict byte 060h                   ; 0c 60
    movzx bx, al                              ; 0f b6 d8
    mov dx, 00087h                            ; ba 87 00
    mov ax, strict word 00040h                ; b8 40 00
    call 02f18h                               ; e8 ac f6
    jmp near 037c7h                           ; e9 58 ff
    mov ax, 00100h                            ; b8 00 01
    mov word [ss:si], ax                      ; 36 89 04
    leave                                     ; c9
    pop si                                    ; 5e
    retn                                      ; c3
vbe_biosfn_read_video_state_size_:           ; 0xc3878 LB 0x8
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov ax, strict word 00012h                ; b8 12 00
    pop bp                                    ; 5d
    retn                                      ; c3
vbe_biosfn_save_video_state_:                ; 0xc3880 LB 0x58
    push bx                                   ; 53
    push cx                                   ; 51
    push si                                   ; 56
    push di                                   ; 57
    enter 00002h, 000h                        ; c8 02 00 00
    mov di, ax                                ; 89 c7
    mov cx, dx                                ; 89 d1
    mov ax, strict word 00004h                ; b8 04 00
    mov dx, 001ceh                            ; ba ce 01
    out DX, ax                                ; ef
    mov dx, 001cfh                            ; ba cf 01
    in ax, DX                                 ; ed
    mov word [bp-002h], ax                    ; 89 46 fe
    mov bx, ax                                ; 89 c3
    mov dx, cx                                ; 89 ca
    mov ax, di                                ; 89 f8
    call 02f34h                               ; e8 91 f6
    inc cx                                    ; 41
    inc cx                                    ; 41
    test byte [bp-002h], 001h                 ; f6 46 fe 01
    je short 038d2h                           ; 74 27
    mov si, strict word 00001h                ; be 01 00
    jmp short 038b5h                          ; eb 05
    cmp si, strict byte 00009h                ; 83 fe 09
    jnbe short 038d2h                         ; 77 1d
    cmp si, strict byte 00004h                ; 83 fe 04
    je short 038cfh                           ; 74 15
    mov ax, si                                ; 89 f0
    mov dx, 001ceh                            ; ba ce 01
    out DX, ax                                ; ef
    mov dx, 001cfh                            ; ba cf 01
    in ax, DX                                 ; ed
    mov bx, ax                                ; 89 c3
    mov dx, cx                                ; 89 ca
    mov ax, di                                ; 89 f8
    call 02f34h                               ; e8 67 f6
    inc cx                                    ; 41
    inc cx                                    ; 41
    inc si                                    ; 46
    jmp short 038b0h                          ; eb de
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
vbe_biosfn_restore_video_state_:             ; 0xc38d8 LB 0x98
    push bx                                   ; 53
    push cx                                   ; 51
    push si                                   ; 56
    enter 00002h, 000h                        ; c8 02 00 00
    mov cx, ax                                ; 89 c1
    mov bx, dx                                ; 89 d3
    call 02f26h                               ; e8 40 f6
    mov word [bp-002h], ax                    ; 89 46 fe
    inc bx                                    ; 43
    inc bx                                    ; 43
    test byte [bp-002h], 001h                 ; f6 46 fe 01
    jne short 03901h                          ; 75 10
    mov ax, strict word 00004h                ; b8 04 00
    mov dx, 001ceh                            ; ba ce 01
    out DX, ax                                ; ef
    mov ax, word [bp-002h]                    ; 8b 46 fe
    mov dx, 001cfh                            ; ba cf 01
    out DX, ax                                ; ef
    jmp short 0396bh                          ; eb 6a
    mov ax, strict word 00001h                ; b8 01 00
    mov dx, 001ceh                            ; ba ce 01
    out DX, ax                                ; ef
    mov dx, bx                                ; 89 da
    mov ax, cx                                ; 89 c8
    call 02f26h                               ; e8 17 f6
    mov dx, 001cfh                            ; ba cf 01
    out DX, ax                                ; ef
    inc bx                                    ; 43
    inc bx                                    ; 43
    mov ax, strict word 00002h                ; b8 02 00
    mov dx, 001ceh                            ; ba ce 01
    out DX, ax                                ; ef
    mov dx, bx                                ; 89 da
    mov ax, cx                                ; 89 c8
    call 02f26h                               ; e8 03 f6
    mov dx, 001cfh                            ; ba cf 01
    out DX, ax                                ; ef
    inc bx                                    ; 43
    inc bx                                    ; 43
    mov ax, strict word 00003h                ; b8 03 00
    mov dx, 001ceh                            ; ba ce 01
    out DX, ax                                ; ef
    mov dx, bx                                ; 89 da
    mov ax, cx                                ; 89 c8
    call 02f26h                               ; e8 ef f5
    mov dx, 001cfh                            ; ba cf 01
    out DX, ax                                ; ef
    inc bx                                    ; 43
    inc bx                                    ; 43
    mov ax, strict word 00004h                ; b8 04 00
    mov dx, 001ceh                            ; ba ce 01
    out DX, ax                                ; ef
    mov ax, word [bp-002h]                    ; 8b 46 fe
    mov dx, 001cfh                            ; ba cf 01
    out DX, ax                                ; ef
    mov si, strict word 00005h                ; be 05 00
    jmp short 03955h                          ; eb 05
    cmp si, strict byte 00009h                ; 83 fe 09
    jnbe short 0396bh                         ; 77 16
    mov ax, si                                ; 89 f0
    mov dx, 001ceh                            ; ba ce 01
    out DX, ax                                ; ef
    mov dx, bx                                ; 89 da
    mov ax, cx                                ; 89 c8
    call 02f26h                               ; e8 c4 f5
    mov dx, 001cfh                            ; ba cf 01
    out DX, ax                                ; ef
    inc bx                                    ; 43
    inc bx                                    ; 43
    inc si                                    ; 46
    jmp short 03950h                          ; eb e5
    leave                                     ; c9
    pop si                                    ; 5e
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
vbe_biosfn_save_restore_state_:              ; 0xc3970 LB 0x89
    push si                                   ; 56
    push di                                   ; 57
    enter 00002h, 000h                        ; c8 02 00 00
    mov si, ax                                ; 89 c6
    mov word [bp-002h], dx                    ; 89 56 fe
    mov ax, bx                                ; 89 d8
    mov bx, word [bp+008h]                    ; 8b 5e 08
    mov di, strict word 0004fh                ; bf 4f 00
    xor ah, ah                                ; 30 e4
    cmp ax, strict word 00002h                ; 3d 02 00
    je short 039cfh                           ; 74 45
    cmp ax, strict word 00001h                ; 3d 01 00
    je short 039b3h                           ; 74 24
    test ax, ax                               ; 85 c0
    jne short 039ebh                          ; 75 58
    mov ax, word [bp-002h]                    ; 8b 46 fe
    call 0282bh                               ; e8 92 ee
    mov cx, ax                                ; 89 c1
    test byte [bp-002h], 008h                 ; f6 46 fe 08
    je short 039a6h                           ; 74 05
    call 03878h                               ; e8 d4 fe
    add ax, cx                                ; 01 c8
    add ax, strict word 0003fh                ; 05 3f 00
    shr ax, 006h                              ; c1 e8 06
    push SS                                   ; 16
    pop ES                                    ; 07
    mov word [es:bx], ax                      ; 26 89 07
    jmp short 039eeh                          ; eb 3b
    push SS                                   ; 16
    pop ES                                    ; 07
    mov bx, word [es:bx]                      ; 26 8b 1f
    mov dx, cx                                ; 89 ca
    mov ax, word [bp-002h]                    ; 8b 46 fe
    call 0285dh                               ; e8 9d ee
    test byte [bp-002h], 008h                 ; f6 46 fe 08
    je short 039eeh                           ; 74 28
    mov dx, ax                                ; 89 c2
    mov ax, cx                                ; 89 c8
    call 03880h                               ; e8 b3 fe
    jmp short 039eeh                          ; eb 1f
    push SS                                   ; 16
    pop ES                                    ; 07
    mov bx, word [es:bx]                      ; 26 8b 1f
    mov dx, cx                                ; 89 ca
    mov ax, word [bp-002h]                    ; 8b 46 fe
    call 02bc2h                               ; e8 e6 f1
    test byte [bp-002h], 008h                 ; f6 46 fe 08
    je short 039eeh                           ; 74 0c
    mov dx, ax                                ; 89 c2
    mov ax, cx                                ; 89 c8
    call 038d8h                               ; e8 ef fe
    jmp short 039eeh                          ; eb 03
    mov di, 00100h                            ; bf 00 01
    push SS                                   ; 16
    pop ES                                    ; 07
    mov word [es:si], di                      ; 26 89 3c
    leave                                     ; c9
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn 00002h                               ; c2 02 00

  ; Padding 0xc07 bytes at 0xc39f9
  times 3079 db 0

section VBE32 progbits vstart=0x4600 align=1 ; size=0x111 class=CODE group=AUTO
vesa_pm_start:                               ; 0xc4600 LB 0x110
    adc AL, strict byte 000h                  ; 14 00
    dec bx                                    ; 4b
    add byte [si], cl                         ; 00 0c
    add word [bx+si], cx                      ; 01 08
    add dh, cl                                ; 00 ce
    add di, cx                                ; 01 cf
    add word [bp-048fdh], si                  ; 01 b6 03 b7
    db  003h, 0ffh
    ; add di, di                                ; 03 ff
    db  0ffh
    db  0ffh
    jmp word [bp-07dh]                        ; ff 66 83
    sti                                       ; fb
    add byte [si+005h], dh                    ; 00 74 05
    mov eax, strict dword 066c30100h          ; 66 b8 00 01 c3 66
    db  08bh, 0c2h
    ; mov ax, dx                                ; 8b c2
    push edx                                  ; 66 52
    push eax                                  ; 66 50
    mov edx, strict dword 0b86601ceh          ; 66 ba ce 01 66 b8
    add ax, 06600h                            ; 05 00 66
    out DX, ax                                ; ef
    pop eax                                   ; 66 58
    mov edx, strict dword 0ef6601cfh          ; 66 ba cf 01 66 ef
    in eax, DX                                ; 66 ed
    pop edx                                   ; 66 5a
    db  066h, 03bh, 0d0h
    ; cmp edx, eax                              ; 66 3b d0
    jne short 04646h                          ; 75 05
    mov eax, strict dword 066c3004fh          ; 66 b8 4f 00 c3 66
    mov ax, 0014fh                            ; b8 4f 01
    retn                                      ; c3
    cmp bl, 080h                              ; 80 fb 80
    je short 0465ah                           ; 74 0a
    cmp bl, 000h                              ; 80 fb 00
    je short 0466ah                           ; 74 15
    mov eax, strict dword 052c30100h          ; 66 b8 00 01 c3 52
    mov edx, strict dword 0a8ec03dah          ; 66 ba da 03 ec a8
    or byte [di-005h], dh                     ; 08 75 fb
    in AL, DX                                 ; ec
    test AL, strict byte 008h                 ; a8 08
    je short 04664h                           ; 74 fb
    pop dx                                    ; 5a
    push ax                                   ; 50
    push cx                                   ; 51
    push dx                                   ; 52
    push si                                   ; 56
    push di                                   ; 57
    sal dx, 010h                              ; c1 e2 10
    and cx, strict word 0ffffh                ; 81 e1 ff ff
    add byte [bx+si], al                      ; 00 00
    db  00bh, 0cah
    ; or cx, dx                                 ; 0b ca
    sal cx, 002h                              ; c1 e1 02
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    push ax                                   ; 50
    mov edx, strict dword 0b86601ceh          ; 66 ba ce 01 66 b8
    push ES                                   ; 06
    add byte [bp-011h], ah                    ; 00 66 ef
    mov edx, strict dword 0ed6601cfh          ; 66 ba cf 01 66 ed
    db  00fh, 0b7h, 0c8h
    ; movzx cx, ax                              ; 0f b7 c8
    mov edx, strict dword 0b86601ceh          ; 66 ba ce 01 66 b8
    add ax, word [bx+si]                      ; 03 00
    out DX, eax                               ; 66 ef
    mov edx, strict dword 0ed6601cfh          ; 66 ba cf 01 66 ed
    db  00fh, 0b7h, 0f0h
    ; movzx si, ax                              ; 0f b7 f0
    pop ax                                    ; 58
    cmp si, strict byte 00004h                ; 83 fe 04
    je short 046c3h                           ; 74 17
    add si, strict byte 00007h                ; 83 c6 07
    shr si, 003h                              ; c1 ee 03
    imul cx, si                               ; 0f af ce
    db  033h, 0d2h
    ; xor dx, dx                                ; 33 d2
    div cx                                    ; f7 f1
    db  08bh, 0f8h
    ; mov di, ax                                ; 8b f8
    db  08bh, 0c2h
    ; mov ax, dx                                ; 8b c2
    db  033h, 0d2h
    ; xor dx, dx                                ; 33 d2
    div si                                    ; f7 f6
    jmp short 046cfh                          ; eb 0c
    shr cx, 1                                 ; d1 e9
    db  033h, 0d2h
    ; xor dx, dx                                ; 33 d2
    div cx                                    ; f7 f1
    db  08bh, 0f8h
    ; mov di, ax                                ; 8b f8
    db  08bh, 0c2h
    ; mov ax, dx                                ; 8b c2
    sal ax, 1                                 ; d1 e0
    push edx                                  ; 66 52
    push eax                                  ; 66 50
    mov edx, strict dword 0b86601ceh          ; 66 ba ce 01 66 b8
    or byte [bx+si], al                       ; 08 00
    out DX, eax                               ; 66 ef
    pop eax                                   ; 66 58
    mov edx, strict dword 0ef6601cfh          ; 66 ba cf 01 66 ef
    pop edx                                   ; 66 5a
    db  066h, 08bh, 0c7h
    ; mov eax, edi                              ; 66 8b c7
    push edx                                  ; 66 52
    push eax                                  ; 66 50
    mov edx, strict dword 0b86601ceh          ; 66 ba ce 01 66 b8
    or word [bx+si], ax                       ; 09 00
    out DX, eax                               ; 66 ef
    pop eax                                   ; 66 58
    mov edx, strict dword 0ef6601cfh          ; 66 ba cf 01 66 ef
    pop edx                                   ; 66 5a
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop ax                                    ; 58
    mov eax, strict dword 066c3004fh          ; 66 b8 4f 00 c3 66
    mov ax, 0014fh                            ; b8 4f 01
vesa_pm_end:                                 ; 0xc4710 LB 0x1
    retn                                      ; c3

  ; Padding 0xef bytes at 0xc4711
  times 239 db 0

section _DATA progbits vstart=0x4800 align=1 ; size=0x36e1 class=DATA group=DGROUP
_msg_vga_init:                               ; 0xc4800 LB 0x2f
    db  'Oracle VM VirtualBox Version 4.1.51 VGA BIOS', 00dh, 00ah, 000h
_vga_modes:                                  ; 0xc482f LB 0x80
    db  000h, 000h, 000h, 004h, 000h, 0b8h, 0ffh, 002h, 001h, 000h, 000h, 004h, 000h, 0b8h, 0ffh, 002h
    db  002h, 000h, 000h, 004h, 000h, 0b8h, 0ffh, 002h, 003h, 000h, 000h, 004h, 000h, 0b8h, 0ffh, 002h
    db  004h, 001h, 002h, 002h, 000h, 0b8h, 0ffh, 001h, 005h, 001h, 002h, 002h, 000h, 0b8h, 0ffh, 001h
    db  006h, 001h, 002h, 001h, 000h, 0b8h, 0ffh, 001h, 007h, 000h, 001h, 004h, 000h, 0b0h, 0ffh, 000h
    db  00dh, 001h, 004h, 004h, 000h, 0a0h, 0ffh, 001h, 00eh, 001h, 004h, 004h, 000h, 0a0h, 0ffh, 001h
    db  00fh, 001h, 003h, 001h, 000h, 0a0h, 0ffh, 000h, 010h, 001h, 004h, 004h, 000h, 0a0h, 0ffh, 002h
    db  011h, 001h, 003h, 001h, 000h, 0a0h, 0ffh, 002h, 012h, 001h, 004h, 004h, 000h, 0a0h, 0ffh, 002h
    db  013h, 001h, 005h, 008h, 000h, 0a0h, 0ffh, 003h, 06ah, 001h, 004h, 004h, 000h, 0a0h, 0ffh, 002h
_line_to_vpti:                               ; 0xc48af LB 0x10
    db  017h, 017h, 018h, 018h, 004h, 005h, 006h, 007h, 00dh, 00eh, 011h, 012h, 01ah, 01bh, 01ch, 01dh
_dac_regs:                                   ; 0xc48bf LB 0x4
    dd  0ff3f3f3fh
_video_param_table:                          ; 0xc48c3 LB 0x780
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  028h, 018h, 008h, 000h, 008h, 009h, 003h, 000h, 002h, 063h, 02dh, 027h, 028h, 090h, 02bh, 080h
    db  0bfh, 01fh, 000h, 0c1h, 000h, 000h, 000h, 000h, 000h, 000h, 09ch, 08eh, 08fh, 014h, 000h, 096h
    db  0b9h, 0a2h, 0ffh, 000h, 013h, 015h, 017h, 002h, 004h, 006h, 007h, 010h, 011h, 012h, 013h, 014h
    db  015h, 016h, 017h, 001h, 000h, 003h, 000h, 000h, 000h, 000h, 000h, 000h, 030h, 00fh, 00fh, 0ffh
    db  028h, 018h, 008h, 000h, 008h, 009h, 003h, 000h, 002h, 063h, 02dh, 027h, 028h, 090h, 02bh, 080h
    db  0bfh, 01fh, 000h, 0c1h, 000h, 000h, 000h, 000h, 000h, 000h, 09ch, 08eh, 08fh, 014h, 000h, 096h
    db  0b9h, 0a2h, 0ffh, 000h, 013h, 015h, 017h, 002h, 004h, 006h, 007h, 010h, 011h, 012h, 013h, 014h
    db  015h, 016h, 017h, 001h, 000h, 003h, 000h, 000h, 000h, 000h, 000h, 000h, 030h, 00fh, 00fh, 0ffh
    db  050h, 018h, 008h, 000h, 010h, 001h, 001h, 000h, 006h, 063h, 05fh, 04fh, 050h, 082h, 054h, 080h
    db  0bfh, 01fh, 000h, 0c1h, 000h, 000h, 000h, 000h, 000h, 000h, 09ch, 08eh, 08fh, 028h, 000h, 096h
    db  0b9h, 0c2h, 0ffh, 000h, 017h, 017h, 017h, 017h, 017h, 017h, 017h, 017h, 017h, 017h, 017h, 017h
    db  017h, 017h, 017h, 001h, 000h, 001h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 00dh, 00fh, 0ffh
    db  050h, 018h, 010h, 000h, 010h, 000h, 003h, 000h, 002h, 066h, 05fh, 04fh, 050h, 082h, 055h, 081h
    db  0bfh, 01fh, 000h, 04fh, 00dh, 00eh, 000h, 000h, 000h, 000h, 09ch, 08eh, 08fh, 028h, 00fh, 096h
    db  0b9h, 0a3h, 0ffh, 000h, 008h, 008h, 008h, 008h, 008h, 008h, 008h, 010h, 018h, 018h, 018h, 018h
    db  018h, 018h, 018h, 00eh, 000h, 00fh, 008h, 000h, 000h, 000h, 000h, 000h, 010h, 00ah, 00fh, 0ffh
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  028h, 018h, 008h, 000h, 020h, 009h, 00fh, 000h, 006h, 063h, 02dh, 027h, 028h, 090h, 02bh, 080h
    db  0bfh, 01fh, 000h, 0c0h, 000h, 000h, 000h, 000h, 000h, 000h, 09ch, 08eh, 08fh, 014h, 000h, 096h
    db  0b9h, 0e3h, 0ffh, 000h, 001h, 002h, 003h, 004h, 005h, 006h, 007h, 010h, 011h, 012h, 013h, 014h
    db  015h, 016h, 017h, 001h, 000h, 00fh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 005h, 00fh, 0ffh
    db  050h, 018h, 008h, 000h, 040h, 001h, 00fh, 000h, 006h, 063h, 05fh, 04fh, 050h, 082h, 054h, 080h
    db  0bfh, 01fh, 000h, 0c0h, 000h, 000h, 000h, 000h, 000h, 000h, 09ch, 08eh, 08fh, 028h, 000h, 096h
    db  0b9h, 0e3h, 0ffh, 000h, 001h, 002h, 003h, 004h, 005h, 006h, 007h, 010h, 011h, 012h, 013h, 014h
    db  015h, 016h, 017h, 001h, 000h, 00fh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 005h, 00fh, 0ffh
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  050h, 018h, 00eh, 000h, 080h, 001h, 00fh, 000h, 006h, 0a3h, 05fh, 04fh, 050h, 082h, 054h, 080h
    db  0bfh, 01fh, 000h, 040h, 000h, 000h, 000h, 000h, 000h, 000h, 083h, 085h, 05dh, 028h, 00fh, 063h
    db  0bah, 0e3h, 0ffh, 000h, 008h, 000h, 000h, 018h, 018h, 000h, 000h, 000h, 008h, 000h, 000h, 000h
    db  018h, 000h, 000h, 001h, 000h, 001h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 005h, 00fh, 0ffh
    db  050h, 018h, 00eh, 000h, 080h, 001h, 00fh, 000h, 006h, 0a3h, 05fh, 04fh, 050h, 082h, 054h, 080h
    db  0bfh, 01fh, 000h, 040h, 000h, 000h, 000h, 000h, 000h, 000h, 083h, 085h, 05dh, 028h, 00fh, 063h
    db  0bah, 0e3h, 0ffh, 000h, 001h, 002h, 003h, 004h, 005h, 014h, 007h, 038h, 039h, 03ah, 03bh, 03ch
    db  03dh, 03eh, 03fh, 001h, 000h, 00fh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 005h, 00fh, 0ffh
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  050h, 018h, 00eh, 000h, 010h, 000h, 003h, 000h, 002h, 067h, 05fh, 04fh, 050h, 082h, 055h, 081h
    db  0bfh, 01fh, 000h, 04fh, 00dh, 00eh, 000h, 000h, 000h, 000h, 09ch, 08eh, 08fh, 028h, 01fh, 096h
    db  0b9h, 0a3h, 0ffh, 000h, 001h, 002h, 003h, 004h, 005h, 014h, 007h, 038h, 039h, 03ah, 03bh, 03ch
    db  03dh, 03eh, 03fh, 00ch, 000h, 00fh, 008h, 000h, 000h, 000h, 000h, 000h, 010h, 00eh, 00fh, 0ffh
    db  028h, 018h, 010h, 000h, 008h, 008h, 003h, 000h, 002h, 067h, 02dh, 027h, 028h, 090h, 02bh, 0a0h
    db  0bfh, 01fh, 000h, 04fh, 00dh, 00eh, 000h, 000h, 000h, 000h, 09ch, 08eh, 08fh, 014h, 01fh, 096h
    db  0b9h, 0a3h, 0ffh, 000h, 001h, 002h, 003h, 004h, 005h, 014h, 007h, 038h, 039h, 03ah, 03bh, 03ch
    db  03dh, 03eh, 03fh, 00ch, 000h, 00fh, 008h, 000h, 000h, 000h, 000h, 000h, 010h, 00eh, 00fh, 0ffh
    db  050h, 018h, 010h, 000h, 010h, 000h, 003h, 000h, 002h, 067h, 05fh, 04fh, 050h, 082h, 055h, 081h
    db  0bfh, 01fh, 000h, 04fh, 00dh, 00eh, 000h, 000h, 000h, 000h, 09ch, 08eh, 08fh, 028h, 01fh, 096h
    db  0b9h, 0a3h, 0ffh, 000h, 001h, 002h, 003h, 004h, 005h, 014h, 007h, 038h, 039h, 03ah, 03bh, 03ch
    db  03dh, 03eh, 03fh, 00ch, 000h, 00fh, 008h, 000h, 000h, 000h, 000h, 000h, 010h, 00eh, 00fh, 0ffh
    db  050h, 018h, 010h, 000h, 010h, 000h, 003h, 000h, 002h, 066h, 05fh, 04fh, 050h, 082h, 055h, 081h
    db  0bfh, 01fh, 000h, 04fh, 00dh, 00eh, 000h, 000h, 000h, 000h, 09ch, 08eh, 08fh, 028h, 00fh, 096h
    db  0b9h, 0a3h, 0ffh, 000h, 008h, 008h, 008h, 008h, 008h, 008h, 008h, 010h, 018h, 018h, 018h, 018h
    db  018h, 018h, 018h, 00eh, 000h, 00fh, 008h, 000h, 000h, 000h, 000h, 000h, 010h, 00ah, 00fh, 0ffh
    db  050h, 01dh, 010h, 000h, 000h, 001h, 00fh, 000h, 006h, 0e3h, 05fh, 04fh, 050h, 082h, 054h, 080h
    db  00bh, 03eh, 000h, 040h, 000h, 000h, 000h, 000h, 000h, 000h, 0eah, 08ch, 0dfh, 028h, 000h, 0e7h
    db  004h, 0e3h, 0ffh, 000h, 03fh, 000h, 03fh, 000h, 03fh, 000h, 03fh, 000h, 03fh, 000h, 03fh, 000h
    db  03fh, 000h, 03fh, 001h, 000h, 00fh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 005h, 00fh, 0ffh
    db  050h, 01dh, 010h, 000h, 000h, 001h, 00fh, 000h, 006h, 0e3h, 05fh, 04fh, 050h, 082h, 054h, 080h
    db  00bh, 03eh, 000h, 040h, 000h, 000h, 000h, 000h, 000h, 000h, 0eah, 08ch, 0dfh, 028h, 000h, 0e7h
    db  004h, 0e3h, 0ffh, 000h, 001h, 002h, 003h, 004h, 005h, 014h, 007h, 038h, 039h, 03ah, 03bh, 03ch
    db  03dh, 03eh, 03fh, 001h, 000h, 00fh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 005h, 00fh, 0ffh
    db  028h, 018h, 008h, 000h, 000h, 001h, 00fh, 000h, 00eh, 063h, 05fh, 04fh, 050h, 082h, 054h, 080h
    db  0bfh, 01fh, 000h, 041h, 000h, 000h, 000h, 000h, 000h, 000h, 09ch, 08eh, 08fh, 028h, 040h, 096h
    db  0b9h, 0a3h, 0ffh, 000h, 001h, 002h, 003h, 004h, 005h, 006h, 007h, 008h, 009h, 00ah, 00bh, 00ch
    db  00dh, 00eh, 00fh, 041h, 000h, 00fh, 000h, 000h, 000h, 000h, 000h, 000h, 040h, 005h, 00fh, 0ffh
    db  064h, 024h, 010h, 000h, 000h, 001h, 00fh, 000h, 006h, 0e3h, 07fh, 063h, 063h, 083h, 06bh, 01bh
    db  072h, 0f0h, 000h, 060h, 000h, 000h, 000h, 000h, 000h, 000h, 059h, 08dh, 057h, 032h, 000h, 057h
    db  073h, 0e3h, 0ffh, 000h, 001h, 002h, 003h, 004h, 005h, 014h, 007h, 038h, 039h, 03ah, 03bh, 03ch
    db  03dh, 03eh, 03fh, 001h, 000h, 00fh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 005h, 00fh, 0ffh
_palette0:                                   ; 0xc5043 LB 0xc0
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah
    db  02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah
    db  02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah
    db  02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh
    db  03fh, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah
    db  02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah
    db  02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah
    db  02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh
    db  03fh, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh
_palette1:                                   ; 0xc5103 LB 0xc0
    db  000h, 000h, 000h, 000h, 000h, 02ah, 000h, 02ah, 000h, 000h, 02ah, 02ah, 02ah, 000h, 000h, 02ah
    db  000h, 02ah, 02ah, 015h, 000h, 02ah, 02ah, 02ah, 000h, 000h, 000h, 000h, 000h, 02ah, 000h, 02ah
    db  000h, 000h, 02ah, 02ah, 02ah, 000h, 000h, 02ah, 000h, 02ah, 02ah, 015h, 000h, 02ah, 02ah, 02ah
    db  015h, 015h, 015h, 015h, 015h, 03fh, 015h, 03fh, 015h, 015h, 03fh, 03fh, 03fh, 015h, 015h, 03fh
    db  015h, 03fh, 03fh, 03fh, 015h, 03fh, 03fh, 03fh, 015h, 015h, 015h, 015h, 015h, 03fh, 015h, 03fh
    db  015h, 015h, 03fh, 03fh, 03fh, 015h, 015h, 03fh, 015h, 03fh, 03fh, 03fh, 015h, 03fh, 03fh, 03fh
    db  000h, 000h, 000h, 000h, 000h, 02ah, 000h, 02ah, 000h, 000h, 02ah, 02ah, 02ah, 000h, 000h, 02ah
    db  000h, 02ah, 02ah, 015h, 000h, 02ah, 02ah, 02ah, 000h, 000h, 000h, 000h, 000h, 02ah, 000h, 02ah
    db  000h, 000h, 02ah, 02ah, 02ah, 000h, 000h, 02ah, 000h, 02ah, 02ah, 015h, 000h, 02ah, 02ah, 02ah
    db  015h, 015h, 015h, 015h, 015h, 03fh, 015h, 03fh, 015h, 015h, 03fh, 03fh, 03fh, 015h, 015h, 03fh
    db  015h, 03fh, 03fh, 03fh, 015h, 03fh, 03fh, 03fh, 015h, 015h, 015h, 015h, 015h, 03fh, 015h, 03fh
    db  015h, 015h, 03fh, 03fh, 03fh, 015h, 015h, 03fh, 015h, 03fh, 03fh, 03fh, 015h, 03fh, 03fh, 03fh
_palette2:                                   ; 0xc51c3 LB 0xc0
    db  000h, 000h, 000h, 000h, 000h, 02ah, 000h, 02ah, 000h, 000h, 02ah, 02ah, 02ah, 000h, 000h, 02ah
    db  000h, 02ah, 02ah, 02ah, 000h, 02ah, 02ah, 02ah, 000h, 000h, 015h, 000h, 000h, 03fh, 000h, 02ah
    db  015h, 000h, 02ah, 03fh, 02ah, 000h, 015h, 02ah, 000h, 03fh, 02ah, 02ah, 015h, 02ah, 02ah, 03fh
    db  000h, 015h, 000h, 000h, 015h, 02ah, 000h, 03fh, 000h, 000h, 03fh, 02ah, 02ah, 015h, 000h, 02ah
    db  015h, 02ah, 02ah, 03fh, 000h, 02ah, 03fh, 02ah, 000h, 015h, 015h, 000h, 015h, 03fh, 000h, 03fh
    db  015h, 000h, 03fh, 03fh, 02ah, 015h, 015h, 02ah, 015h, 03fh, 02ah, 03fh, 015h, 02ah, 03fh, 03fh
    db  015h, 000h, 000h, 015h, 000h, 02ah, 015h, 02ah, 000h, 015h, 02ah, 02ah, 03fh, 000h, 000h, 03fh
    db  000h, 02ah, 03fh, 02ah, 000h, 03fh, 02ah, 02ah, 015h, 000h, 015h, 015h, 000h, 03fh, 015h, 02ah
    db  015h, 015h, 02ah, 03fh, 03fh, 000h, 015h, 03fh, 000h, 03fh, 03fh, 02ah, 015h, 03fh, 02ah, 03fh
    db  015h, 015h, 000h, 015h, 015h, 02ah, 015h, 03fh, 000h, 015h, 03fh, 02ah, 03fh, 015h, 000h, 03fh
    db  015h, 02ah, 03fh, 03fh, 000h, 03fh, 03fh, 02ah, 015h, 015h, 015h, 015h, 015h, 03fh, 015h, 03fh
    db  015h, 015h, 03fh, 03fh, 03fh, 015h, 015h, 03fh, 015h, 03fh, 03fh, 03fh, 015h, 03fh, 03fh, 03fh
_palette3:                                   ; 0xc5283 LB 0x300
    db  000h, 000h, 000h, 000h, 000h, 02ah, 000h, 02ah, 000h, 000h, 02ah, 02ah, 02ah, 000h, 000h, 02ah
    db  000h, 02ah, 02ah, 015h, 000h, 02ah, 02ah, 02ah, 015h, 015h, 015h, 015h, 015h, 03fh, 015h, 03fh
    db  015h, 015h, 03fh, 03fh, 03fh, 015h, 015h, 03fh, 015h, 03fh, 03fh, 03fh, 015h, 03fh, 03fh, 03fh
    db  000h, 000h, 000h, 005h, 005h, 005h, 008h, 008h, 008h, 00bh, 00bh, 00bh, 00eh, 00eh, 00eh, 011h
    db  011h, 011h, 014h, 014h, 014h, 018h, 018h, 018h, 01ch, 01ch, 01ch, 020h, 020h, 020h, 024h, 024h
    db  024h, 028h, 028h, 028h, 02dh, 02dh, 02dh, 032h, 032h, 032h, 038h, 038h, 038h, 03fh, 03fh, 03fh
    db  000h, 000h, 03fh, 010h, 000h, 03fh, 01fh, 000h, 03fh, 02fh, 000h, 03fh, 03fh, 000h, 03fh, 03fh
    db  000h, 02fh, 03fh, 000h, 01fh, 03fh, 000h, 010h, 03fh, 000h, 000h, 03fh, 010h, 000h, 03fh, 01fh
    db  000h, 03fh, 02fh, 000h, 03fh, 03fh, 000h, 02fh, 03fh, 000h, 01fh, 03fh, 000h, 010h, 03fh, 000h
    db  000h, 03fh, 000h, 000h, 03fh, 010h, 000h, 03fh, 01fh, 000h, 03fh, 02fh, 000h, 03fh, 03fh, 000h
    db  02fh, 03fh, 000h, 01fh, 03fh, 000h, 010h, 03fh, 01fh, 01fh, 03fh, 027h, 01fh, 03fh, 02fh, 01fh
    db  03fh, 037h, 01fh, 03fh, 03fh, 01fh, 03fh, 03fh, 01fh, 037h, 03fh, 01fh, 02fh, 03fh, 01fh, 027h
    db  03fh, 01fh, 01fh, 03fh, 027h, 01fh, 03fh, 02fh, 01fh, 03fh, 037h, 01fh, 03fh, 03fh, 01fh, 037h
    db  03fh, 01fh, 02fh, 03fh, 01fh, 027h, 03fh, 01fh, 01fh, 03fh, 01fh, 01fh, 03fh, 027h, 01fh, 03fh
    db  02fh, 01fh, 03fh, 037h, 01fh, 03fh, 03fh, 01fh, 037h, 03fh, 01fh, 02fh, 03fh, 01fh, 027h, 03fh
    db  02dh, 02dh, 03fh, 031h, 02dh, 03fh, 036h, 02dh, 03fh, 03ah, 02dh, 03fh, 03fh, 02dh, 03fh, 03fh
    db  02dh, 03ah, 03fh, 02dh, 036h, 03fh, 02dh, 031h, 03fh, 02dh, 02dh, 03fh, 031h, 02dh, 03fh, 036h
    db  02dh, 03fh, 03ah, 02dh, 03fh, 03fh, 02dh, 03ah, 03fh, 02dh, 036h, 03fh, 02dh, 031h, 03fh, 02dh
    db  02dh, 03fh, 02dh, 02dh, 03fh, 031h, 02dh, 03fh, 036h, 02dh, 03fh, 03ah, 02dh, 03fh, 03fh, 02dh
    db  03ah, 03fh, 02dh, 036h, 03fh, 02dh, 031h, 03fh, 000h, 000h, 01ch, 007h, 000h, 01ch, 00eh, 000h
    db  01ch, 015h, 000h, 01ch, 01ch, 000h, 01ch, 01ch, 000h, 015h, 01ch, 000h, 00eh, 01ch, 000h, 007h
    db  01ch, 000h, 000h, 01ch, 007h, 000h, 01ch, 00eh, 000h, 01ch, 015h, 000h, 01ch, 01ch, 000h, 015h
    db  01ch, 000h, 00eh, 01ch, 000h, 007h, 01ch, 000h, 000h, 01ch, 000h, 000h, 01ch, 007h, 000h, 01ch
    db  00eh, 000h, 01ch, 015h, 000h, 01ch, 01ch, 000h, 015h, 01ch, 000h, 00eh, 01ch, 000h, 007h, 01ch
    db  00eh, 00eh, 01ch, 011h, 00eh, 01ch, 015h, 00eh, 01ch, 018h, 00eh, 01ch, 01ch, 00eh, 01ch, 01ch
    db  00eh, 018h, 01ch, 00eh, 015h, 01ch, 00eh, 011h, 01ch, 00eh, 00eh, 01ch, 011h, 00eh, 01ch, 015h
    db  00eh, 01ch, 018h, 00eh, 01ch, 01ch, 00eh, 018h, 01ch, 00eh, 015h, 01ch, 00eh, 011h, 01ch, 00eh
    db  00eh, 01ch, 00eh, 00eh, 01ch, 011h, 00eh, 01ch, 015h, 00eh, 01ch, 018h, 00eh, 01ch, 01ch, 00eh
    db  018h, 01ch, 00eh, 015h, 01ch, 00eh, 011h, 01ch, 014h, 014h, 01ch, 016h, 014h, 01ch, 018h, 014h
    db  01ch, 01ah, 014h, 01ch, 01ch, 014h, 01ch, 01ch, 014h, 01ah, 01ch, 014h, 018h, 01ch, 014h, 016h
    db  01ch, 014h, 014h, 01ch, 016h, 014h, 01ch, 018h, 014h, 01ch, 01ah, 014h, 01ch, 01ch, 014h, 01ah
    db  01ch, 014h, 018h, 01ch, 014h, 016h, 01ch, 014h, 014h, 01ch, 014h, 014h, 01ch, 016h, 014h, 01ch
    db  018h, 014h, 01ch, 01ah, 014h, 01ch, 01ch, 014h, 01ah, 01ch, 014h, 018h, 01ch, 014h, 016h, 01ch
    db  000h, 000h, 010h, 004h, 000h, 010h, 008h, 000h, 010h, 00ch, 000h, 010h, 010h, 000h, 010h, 010h
    db  000h, 00ch, 010h, 000h, 008h, 010h, 000h, 004h, 010h, 000h, 000h, 010h, 004h, 000h, 010h, 008h
    db  000h, 010h, 00ch, 000h, 010h, 010h, 000h, 00ch, 010h, 000h, 008h, 010h, 000h, 004h, 010h, 000h
    db  000h, 010h, 000h, 000h, 010h, 004h, 000h, 010h, 008h, 000h, 010h, 00ch, 000h, 010h, 010h, 000h
    db  00ch, 010h, 000h, 008h, 010h, 000h, 004h, 010h, 008h, 008h, 010h, 00ah, 008h, 010h, 00ch, 008h
    db  010h, 00eh, 008h, 010h, 010h, 008h, 010h, 010h, 008h, 00eh, 010h, 008h, 00ch, 010h, 008h, 00ah
    db  010h, 008h, 008h, 010h, 00ah, 008h, 010h, 00ch, 008h, 010h, 00eh, 008h, 010h, 010h, 008h, 00eh
    db  010h, 008h, 00ch, 010h, 008h, 00ah, 010h, 008h, 008h, 010h, 008h, 008h, 010h, 00ah, 008h, 010h
    db  00ch, 008h, 010h, 00eh, 008h, 010h, 010h, 008h, 00eh, 010h, 008h, 00ch, 010h, 008h, 00ah, 010h
    db  00bh, 00bh, 010h, 00ch, 00bh, 010h, 00dh, 00bh, 010h, 00fh, 00bh, 010h, 010h, 00bh, 010h, 010h
    db  00bh, 00fh, 010h, 00bh, 00dh, 010h, 00bh, 00ch, 010h, 00bh, 00bh, 010h, 00ch, 00bh, 010h, 00dh
    db  00bh, 010h, 00fh, 00bh, 010h, 010h, 00bh, 00fh, 010h, 00bh, 00dh, 010h, 00bh, 00ch, 010h, 00bh
    db  00bh, 010h, 00bh, 00bh, 010h, 00ch, 00bh, 010h, 00dh, 00bh, 010h, 00fh, 00bh, 010h, 010h, 00bh
    db  00fh, 010h, 00bh, 00dh, 010h, 00bh, 00ch, 010h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
_static_functionality:                       ; 0xc5583 LB 0x10
    db  0ffh, 0e0h, 00fh, 000h, 000h, 000h, 000h, 007h, 002h, 008h, 0e7h, 00ch, 000h, 000h, 000h, 000h
_video_save_pointer_table:                   ; 0xc5593 LB 0x1c
    db  0c3h, 048h, 000h, 0c0h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
_vgafont8:                                   ; 0xc55af LB 0x800
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 07eh, 081h, 0a5h, 081h, 0bdh, 099h, 081h, 07eh
    db  07eh, 0ffh, 0dbh, 0ffh, 0c3h, 0e7h, 0ffh, 07eh, 06ch, 0feh, 0feh, 0feh, 07ch, 038h, 010h, 000h
    db  010h, 038h, 07ch, 0feh, 07ch, 038h, 010h, 000h, 038h, 07ch, 038h, 0feh, 0feh, 07ch, 038h, 07ch
    db  010h, 010h, 038h, 07ch, 0feh, 07ch, 038h, 07ch, 000h, 000h, 018h, 03ch, 03ch, 018h, 000h, 000h
    db  0ffh, 0ffh, 0e7h, 0c3h, 0c3h, 0e7h, 0ffh, 0ffh, 000h, 03ch, 066h, 042h, 042h, 066h, 03ch, 000h
    db  0ffh, 0c3h, 099h, 0bdh, 0bdh, 099h, 0c3h, 0ffh, 00fh, 007h, 00fh, 07dh, 0cch, 0cch, 0cch, 078h
    db  03ch, 066h, 066h, 066h, 03ch, 018h, 07eh, 018h, 03fh, 033h, 03fh, 030h, 030h, 070h, 0f0h, 0e0h
    db  07fh, 063h, 07fh, 063h, 063h, 067h, 0e6h, 0c0h, 099h, 05ah, 03ch, 0e7h, 0e7h, 03ch, 05ah, 099h
    db  080h, 0e0h, 0f8h, 0feh, 0f8h, 0e0h, 080h, 000h, 002h, 00eh, 03eh, 0feh, 03eh, 00eh, 002h, 000h
    db  018h, 03ch, 07eh, 018h, 018h, 07eh, 03ch, 018h, 066h, 066h, 066h, 066h, 066h, 000h, 066h, 000h
    db  07fh, 0dbh, 0dbh, 07bh, 01bh, 01bh, 01bh, 000h, 03eh, 063h, 038h, 06ch, 06ch, 038h, 0cch, 078h
    db  000h, 000h, 000h, 000h, 07eh, 07eh, 07eh, 000h, 018h, 03ch, 07eh, 018h, 07eh, 03ch, 018h, 0ffh
    db  018h, 03ch, 07eh, 018h, 018h, 018h, 018h, 000h, 018h, 018h, 018h, 018h, 07eh, 03ch, 018h, 000h
    db  000h, 018h, 00ch, 0feh, 00ch, 018h, 000h, 000h, 000h, 030h, 060h, 0feh, 060h, 030h, 000h, 000h
    db  000h, 000h, 0c0h, 0c0h, 0c0h, 0feh, 000h, 000h, 000h, 024h, 066h, 0ffh, 066h, 024h, 000h, 000h
    db  000h, 018h, 03ch, 07eh, 0ffh, 0ffh, 000h, 000h, 000h, 0ffh, 0ffh, 07eh, 03ch, 018h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 030h, 078h, 078h, 030h, 030h, 000h, 030h, 000h
    db  06ch, 06ch, 06ch, 000h, 000h, 000h, 000h, 000h, 06ch, 06ch, 0feh, 06ch, 0feh, 06ch, 06ch, 000h
    db  030h, 07ch, 0c0h, 078h, 00ch, 0f8h, 030h, 000h, 000h, 0c6h, 0cch, 018h, 030h, 066h, 0c6h, 000h
    db  038h, 06ch, 038h, 076h, 0dch, 0cch, 076h, 000h, 060h, 060h, 0c0h, 000h, 000h, 000h, 000h, 000h
    db  018h, 030h, 060h, 060h, 060h, 030h, 018h, 000h, 060h, 030h, 018h, 018h, 018h, 030h, 060h, 000h
    db  000h, 066h, 03ch, 0ffh, 03ch, 066h, 000h, 000h, 000h, 030h, 030h, 0fch, 030h, 030h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 030h, 030h, 060h, 000h, 000h, 000h, 0fch, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 030h, 030h, 000h, 006h, 00ch, 018h, 030h, 060h, 0c0h, 080h, 000h
    db  07ch, 0c6h, 0ceh, 0deh, 0f6h, 0e6h, 07ch, 000h, 030h, 070h, 030h, 030h, 030h, 030h, 0fch, 000h
    db  078h, 0cch, 00ch, 038h, 060h, 0cch, 0fch, 000h, 078h, 0cch, 00ch, 038h, 00ch, 0cch, 078h, 000h
    db  01ch, 03ch, 06ch, 0cch, 0feh, 00ch, 01eh, 000h, 0fch, 0c0h, 0f8h, 00ch, 00ch, 0cch, 078h, 000h
    db  038h, 060h, 0c0h, 0f8h, 0cch, 0cch, 078h, 000h, 0fch, 0cch, 00ch, 018h, 030h, 030h, 030h, 000h
    db  078h, 0cch, 0cch, 078h, 0cch, 0cch, 078h, 000h, 078h, 0cch, 0cch, 07ch, 00ch, 018h, 070h, 000h
    db  000h, 030h, 030h, 000h, 000h, 030h, 030h, 000h, 000h, 030h, 030h, 000h, 000h, 030h, 030h, 060h
    db  018h, 030h, 060h, 0c0h, 060h, 030h, 018h, 000h, 000h, 000h, 0fch, 000h, 000h, 0fch, 000h, 000h
    db  060h, 030h, 018h, 00ch, 018h, 030h, 060h, 000h, 078h, 0cch, 00ch, 018h, 030h, 000h, 030h, 000h
    db  07ch, 0c6h, 0deh, 0deh, 0deh, 0c0h, 078h, 000h, 030h, 078h, 0cch, 0cch, 0fch, 0cch, 0cch, 000h
    db  0fch, 066h, 066h, 07ch, 066h, 066h, 0fch, 000h, 03ch, 066h, 0c0h, 0c0h, 0c0h, 066h, 03ch, 000h
    db  0f8h, 06ch, 066h, 066h, 066h, 06ch, 0f8h, 000h, 0feh, 062h, 068h, 078h, 068h, 062h, 0feh, 000h
    db  0feh, 062h, 068h, 078h, 068h, 060h, 0f0h, 000h, 03ch, 066h, 0c0h, 0c0h, 0ceh, 066h, 03eh, 000h
    db  0cch, 0cch, 0cch, 0fch, 0cch, 0cch, 0cch, 000h, 078h, 030h, 030h, 030h, 030h, 030h, 078h, 000h
    db  01eh, 00ch, 00ch, 00ch, 0cch, 0cch, 078h, 000h, 0e6h, 066h, 06ch, 078h, 06ch, 066h, 0e6h, 000h
    db  0f0h, 060h, 060h, 060h, 062h, 066h, 0feh, 000h, 0c6h, 0eeh, 0feh, 0feh, 0d6h, 0c6h, 0c6h, 000h
    db  0c6h, 0e6h, 0f6h, 0deh, 0ceh, 0c6h, 0c6h, 000h, 038h, 06ch, 0c6h, 0c6h, 0c6h, 06ch, 038h, 000h
    db  0fch, 066h, 066h, 07ch, 060h, 060h, 0f0h, 000h, 078h, 0cch, 0cch, 0cch, 0dch, 078h, 01ch, 000h
    db  0fch, 066h, 066h, 07ch, 06ch, 066h, 0e6h, 000h, 078h, 0cch, 0e0h, 070h, 01ch, 0cch, 078h, 000h
    db  0fch, 0b4h, 030h, 030h, 030h, 030h, 078h, 000h, 0cch, 0cch, 0cch, 0cch, 0cch, 0cch, 0fch, 000h
    db  0cch, 0cch, 0cch, 0cch, 0cch, 078h, 030h, 000h, 0c6h, 0c6h, 0c6h, 0d6h, 0feh, 0eeh, 0c6h, 000h
    db  0c6h, 0c6h, 06ch, 038h, 038h, 06ch, 0c6h, 000h, 0cch, 0cch, 0cch, 078h, 030h, 030h, 078h, 000h
    db  0feh, 0c6h, 08ch, 018h, 032h, 066h, 0feh, 000h, 078h, 060h, 060h, 060h, 060h, 060h, 078h, 000h
    db  0c0h, 060h, 030h, 018h, 00ch, 006h, 002h, 000h, 078h, 018h, 018h, 018h, 018h, 018h, 078h, 000h
    db  010h, 038h, 06ch, 0c6h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 0ffh
    db  030h, 030h, 018h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 078h, 00ch, 07ch, 0cch, 076h, 000h
    db  0e0h, 060h, 060h, 07ch, 066h, 066h, 0dch, 000h, 000h, 000h, 078h, 0cch, 0c0h, 0cch, 078h, 000h
    db  01ch, 00ch, 00ch, 07ch, 0cch, 0cch, 076h, 000h, 000h, 000h, 078h, 0cch, 0fch, 0c0h, 078h, 000h
    db  038h, 06ch, 060h, 0f0h, 060h, 060h, 0f0h, 000h, 000h, 000h, 076h, 0cch, 0cch, 07ch, 00ch, 0f8h
    db  0e0h, 060h, 06ch, 076h, 066h, 066h, 0e6h, 000h, 030h, 000h, 070h, 030h, 030h, 030h, 078h, 000h
    db  00ch, 000h, 00ch, 00ch, 00ch, 0cch, 0cch, 078h, 0e0h, 060h, 066h, 06ch, 078h, 06ch, 0e6h, 000h
    db  070h, 030h, 030h, 030h, 030h, 030h, 078h, 000h, 000h, 000h, 0cch, 0feh, 0feh, 0d6h, 0c6h, 000h
    db  000h, 000h, 0f8h, 0cch, 0cch, 0cch, 0cch, 000h, 000h, 000h, 078h, 0cch, 0cch, 0cch, 078h, 000h
    db  000h, 000h, 0dch, 066h, 066h, 07ch, 060h, 0f0h, 000h, 000h, 076h, 0cch, 0cch, 07ch, 00ch, 01eh
    db  000h, 000h, 0dch, 076h, 066h, 060h, 0f0h, 000h, 000h, 000h, 07ch, 0c0h, 078h, 00ch, 0f8h, 000h
    db  010h, 030h, 07ch, 030h, 030h, 034h, 018h, 000h, 000h, 000h, 0cch, 0cch, 0cch, 0cch, 076h, 000h
    db  000h, 000h, 0cch, 0cch, 0cch, 078h, 030h, 000h, 000h, 000h, 0c6h, 0d6h, 0feh, 0feh, 06ch, 000h
    db  000h, 000h, 0c6h, 06ch, 038h, 06ch, 0c6h, 000h, 000h, 000h, 0cch, 0cch, 0cch, 07ch, 00ch, 0f8h
    db  000h, 000h, 0fch, 098h, 030h, 064h, 0fch, 000h, 01ch, 030h, 030h, 0e0h, 030h, 030h, 01ch, 000h
    db  018h, 018h, 018h, 000h, 018h, 018h, 018h, 000h, 0e0h, 030h, 030h, 01ch, 030h, 030h, 0e0h, 000h
    db  076h, 0dch, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 010h, 038h, 06ch, 0c6h, 0c6h, 0feh, 000h
    db  078h, 0cch, 0c0h, 0cch, 078h, 018h, 00ch, 078h, 000h, 0cch, 000h, 0cch, 0cch, 0cch, 07eh, 000h
    db  01ch, 000h, 078h, 0cch, 0fch, 0c0h, 078h, 000h, 07eh, 0c3h, 03ch, 006h, 03eh, 066h, 03fh, 000h
    db  0cch, 000h, 078h, 00ch, 07ch, 0cch, 07eh, 000h, 0e0h, 000h, 078h, 00ch, 07ch, 0cch, 07eh, 000h
    db  030h, 030h, 078h, 00ch, 07ch, 0cch, 07eh, 000h, 000h, 000h, 078h, 0c0h, 0c0h, 078h, 00ch, 038h
    db  07eh, 0c3h, 03ch, 066h, 07eh, 060h, 03ch, 000h, 0cch, 000h, 078h, 0cch, 0fch, 0c0h, 078h, 000h
    db  0e0h, 000h, 078h, 0cch, 0fch, 0c0h, 078h, 000h, 0cch, 000h, 070h, 030h, 030h, 030h, 078h, 000h
    db  07ch, 0c6h, 038h, 018h, 018h, 018h, 03ch, 000h, 0e0h, 000h, 070h, 030h, 030h, 030h, 078h, 000h
    db  0c6h, 038h, 06ch, 0c6h, 0feh, 0c6h, 0c6h, 000h, 030h, 030h, 000h, 078h, 0cch, 0fch, 0cch, 000h
    db  01ch, 000h, 0fch, 060h, 078h, 060h, 0fch, 000h, 000h, 000h, 07fh, 00ch, 07fh, 0cch, 07fh, 000h
    db  03eh, 06ch, 0cch, 0feh, 0cch, 0cch, 0ceh, 000h, 078h, 0cch, 000h, 078h, 0cch, 0cch, 078h, 000h
    db  000h, 0cch, 000h, 078h, 0cch, 0cch, 078h, 000h, 000h, 0e0h, 000h, 078h, 0cch, 0cch, 078h, 000h
    db  078h, 0cch, 000h, 0cch, 0cch, 0cch, 07eh, 000h, 000h, 0e0h, 000h, 0cch, 0cch, 0cch, 07eh, 000h
    db  000h, 0cch, 000h, 0cch, 0cch, 07ch, 00ch, 0f8h, 0c3h, 018h, 03ch, 066h, 066h, 03ch, 018h, 000h
    db  0cch, 000h, 0cch, 0cch, 0cch, 0cch, 078h, 000h, 018h, 018h, 07eh, 0c0h, 0c0h, 07eh, 018h, 018h
    db  038h, 06ch, 064h, 0f0h, 060h, 0e6h, 0fch, 000h, 0cch, 0cch, 078h, 0fch, 030h, 0fch, 030h, 030h
    db  0f8h, 0cch, 0cch, 0fah, 0c6h, 0cfh, 0c6h, 0c7h, 00eh, 01bh, 018h, 03ch, 018h, 018h, 0d8h, 070h
    db  01ch, 000h, 078h, 00ch, 07ch, 0cch, 07eh, 000h, 038h, 000h, 070h, 030h, 030h, 030h, 078h, 000h
    db  000h, 01ch, 000h, 078h, 0cch, 0cch, 078h, 000h, 000h, 01ch, 000h, 0cch, 0cch, 0cch, 07eh, 000h
    db  000h, 0f8h, 000h, 0f8h, 0cch, 0cch, 0cch, 000h, 0fch, 000h, 0cch, 0ech, 0fch, 0dch, 0cch, 000h
    db  03ch, 06ch, 06ch, 03eh, 000h, 07eh, 000h, 000h, 038h, 06ch, 06ch, 038h, 000h, 07ch, 000h, 000h
    db  030h, 000h, 030h, 060h, 0c0h, 0cch, 078h, 000h, 000h, 000h, 000h, 0fch, 0c0h, 0c0h, 000h, 000h
    db  000h, 000h, 000h, 0fch, 00ch, 00ch, 000h, 000h, 0c3h, 0c6h, 0cch, 0deh, 033h, 066h, 0cch, 00fh
    db  0c3h, 0c6h, 0cch, 0dbh, 037h, 06fh, 0cfh, 003h, 018h, 018h, 000h, 018h, 018h, 018h, 018h, 000h
    db  000h, 033h, 066h, 0cch, 066h, 033h, 000h, 000h, 000h, 0cch, 066h, 033h, 066h, 0cch, 000h, 000h
    db  022h, 088h, 022h, 088h, 022h, 088h, 022h, 088h, 055h, 0aah, 055h, 0aah, 055h, 0aah, 055h, 0aah
    db  0dbh, 077h, 0dbh, 0eeh, 0dbh, 077h, 0dbh, 0eeh, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h
    db  018h, 018h, 018h, 018h, 0f8h, 018h, 018h, 018h, 018h, 018h, 0f8h, 018h, 0f8h, 018h, 018h, 018h
    db  036h, 036h, 036h, 036h, 0f6h, 036h, 036h, 036h, 000h, 000h, 000h, 000h, 0feh, 036h, 036h, 036h
    db  000h, 000h, 0f8h, 018h, 0f8h, 018h, 018h, 018h, 036h, 036h, 0f6h, 006h, 0f6h, 036h, 036h, 036h
    db  036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 000h, 000h, 0feh, 006h, 0f6h, 036h, 036h, 036h
    db  036h, 036h, 0f6h, 006h, 0feh, 000h, 000h, 000h, 036h, 036h, 036h, 036h, 0feh, 000h, 000h, 000h
    db  018h, 018h, 0f8h, 018h, 0f8h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 0f8h, 018h, 018h, 018h
    db  018h, 018h, 018h, 018h, 01fh, 000h, 000h, 000h, 018h, 018h, 018h, 018h, 0ffh, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 0ffh, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 01fh, 018h, 018h, 018h
    db  000h, 000h, 000h, 000h, 0ffh, 000h, 000h, 000h, 018h, 018h, 018h, 018h, 0ffh, 018h, 018h, 018h
    db  018h, 018h, 01fh, 018h, 01fh, 018h, 018h, 018h, 036h, 036h, 036h, 036h, 037h, 036h, 036h, 036h
    db  036h, 036h, 037h, 030h, 03fh, 000h, 000h, 000h, 000h, 000h, 03fh, 030h, 037h, 036h, 036h, 036h
    db  036h, 036h, 0f7h, 000h, 0ffh, 000h, 000h, 000h, 000h, 000h, 0ffh, 000h, 0f7h, 036h, 036h, 036h
    db  036h, 036h, 037h, 030h, 037h, 036h, 036h, 036h, 000h, 000h, 0ffh, 000h, 0ffh, 000h, 000h, 000h
    db  036h, 036h, 0f7h, 000h, 0f7h, 036h, 036h, 036h, 018h, 018h, 0ffh, 000h, 0ffh, 000h, 000h, 000h
    db  036h, 036h, 036h, 036h, 0ffh, 000h, 000h, 000h, 000h, 000h, 0ffh, 000h, 0ffh, 018h, 018h, 018h
    db  000h, 000h, 000h, 000h, 0ffh, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 03fh, 000h, 000h, 000h
    db  018h, 018h, 01fh, 018h, 01fh, 000h, 000h, 000h, 000h, 000h, 01fh, 018h, 01fh, 018h, 018h, 018h
    db  000h, 000h, 000h, 000h, 03fh, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 0ffh, 036h, 036h, 036h
    db  018h, 018h, 0ffh, 018h, 0ffh, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 0f8h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 01fh, 018h, 018h, 018h, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh
    db  000h, 000h, 000h, 000h, 0ffh, 0ffh, 0ffh, 0ffh, 0f0h, 0f0h, 0f0h, 0f0h, 0f0h, 0f0h, 0f0h, 0f0h
    db  00fh, 00fh, 00fh, 00fh, 00fh, 00fh, 00fh, 00fh, 0ffh, 0ffh, 0ffh, 0ffh, 000h, 000h, 000h, 000h
    db  000h, 000h, 076h, 0dch, 0c8h, 0dch, 076h, 000h, 000h, 078h, 0cch, 0f8h, 0cch, 0f8h, 0c0h, 0c0h
    db  000h, 0fch, 0cch, 0c0h, 0c0h, 0c0h, 0c0h, 000h, 000h, 0feh, 06ch, 06ch, 06ch, 06ch, 06ch, 000h
    db  0fch, 0cch, 060h, 030h, 060h, 0cch, 0fch, 000h, 000h, 000h, 07eh, 0d8h, 0d8h, 0d8h, 070h, 000h
    db  000h, 066h, 066h, 066h, 066h, 07ch, 060h, 0c0h, 000h, 076h, 0dch, 018h, 018h, 018h, 018h, 000h
    db  0fch, 030h, 078h, 0cch, 0cch, 078h, 030h, 0fch, 038h, 06ch, 0c6h, 0feh, 0c6h, 06ch, 038h, 000h
    db  038h, 06ch, 0c6h, 0c6h, 06ch, 06ch, 0eeh, 000h, 01ch, 030h, 018h, 07ch, 0cch, 0cch, 078h, 000h
    db  000h, 000h, 07eh, 0dbh, 0dbh, 07eh, 000h, 000h, 006h, 00ch, 07eh, 0dbh, 0dbh, 07eh, 060h, 0c0h
    db  038h, 060h, 0c0h, 0f8h, 0c0h, 060h, 038h, 000h, 078h, 0cch, 0cch, 0cch, 0cch, 0cch, 0cch, 000h
    db  000h, 0fch, 000h, 0fch, 000h, 0fch, 000h, 000h, 030h, 030h, 0fch, 030h, 030h, 000h, 0fch, 000h
    db  060h, 030h, 018h, 030h, 060h, 000h, 0fch, 000h, 018h, 030h, 060h, 030h, 018h, 000h, 0fch, 000h
    db  00eh, 01bh, 01bh, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 0d8h, 0d8h, 070h
    db  030h, 030h, 000h, 0fch, 000h, 030h, 030h, 000h, 000h, 076h, 0dch, 000h, 076h, 0dch, 000h, 000h
    db  038h, 06ch, 06ch, 038h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 018h, 018h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 018h, 000h, 000h, 000h, 00fh, 00ch, 00ch, 00ch, 0ech, 06ch, 03ch, 01ch
    db  078h, 06ch, 06ch, 06ch, 06ch, 000h, 000h, 000h, 070h, 018h, 030h, 060h, 078h, 000h, 000h, 000h
    db  000h, 000h, 03ch, 03ch, 03ch, 03ch, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
_vgafont14:                                  ; 0xc5daf LB 0xe00
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  07eh, 081h, 0a5h, 081h, 081h, 0bdh, 099h, 081h, 07eh, 000h, 000h, 000h, 000h, 000h, 07eh, 0ffh
    db  0dbh, 0ffh, 0ffh, 0c3h, 0e7h, 0ffh, 07eh, 000h, 000h, 000h, 000h, 000h, 000h, 06ch, 0feh, 0feh
    db  0feh, 0feh, 07ch, 038h, 010h, 000h, 000h, 000h, 000h, 000h, 000h, 010h, 038h, 07ch, 0feh, 07ch
    db  038h, 010h, 000h, 000h, 000h, 000h, 000h, 000h, 018h, 03ch, 03ch, 0e7h, 0e7h, 0e7h, 018h, 018h
    db  03ch, 000h, 000h, 000h, 000h, 000h, 018h, 03ch, 07eh, 0ffh, 0ffh, 07eh, 018h, 018h, 03ch, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 018h, 03ch, 03ch, 018h, 000h, 000h, 000h, 000h, 000h
    db  0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0e7h, 0c3h, 0c3h, 0e7h, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 000h, 000h
    db  000h, 000h, 03ch, 066h, 042h, 042h, 066h, 03ch, 000h, 000h, 000h, 000h, 0ffh, 0ffh, 0ffh, 0ffh
    db  0c3h, 099h, 0bdh, 0bdh, 099h, 0c3h, 0ffh, 0ffh, 0ffh, 0ffh, 000h, 000h, 01eh, 00eh, 01ah, 032h
    db  078h, 0cch, 0cch, 0cch, 078h, 000h, 000h, 000h, 000h, 000h, 03ch, 066h, 066h, 066h, 03ch, 018h
    db  07eh, 018h, 018h, 000h, 000h, 000h, 000h, 000h, 03fh, 033h, 03fh, 030h, 030h, 030h, 070h, 0f0h
    db  0e0h, 000h, 000h, 000h, 000h, 000h, 07fh, 063h, 07fh, 063h, 063h, 063h, 067h, 0e7h, 0e6h, 0c0h
    db  000h, 000h, 000h, 000h, 018h, 018h, 0dbh, 03ch, 0e7h, 03ch, 0dbh, 018h, 018h, 000h, 000h, 000h
    db  000h, 000h, 080h, 0c0h, 0e0h, 0f8h, 0feh, 0f8h, 0e0h, 0c0h, 080h, 000h, 000h, 000h, 000h, 000h
    db  002h, 006h, 00eh, 03eh, 0feh, 03eh, 00eh, 006h, 002h, 000h, 000h, 000h, 000h, 000h, 018h, 03ch
    db  07eh, 018h, 018h, 018h, 07eh, 03ch, 018h, 000h, 000h, 000h, 000h, 000h, 066h, 066h, 066h, 066h
    db  066h, 066h, 000h, 066h, 066h, 000h, 000h, 000h, 000h, 000h, 07fh, 0dbh, 0dbh, 0dbh, 07bh, 01bh
    db  01bh, 01bh, 01bh, 000h, 000h, 000h, 000h, 07ch, 0c6h, 060h, 038h, 06ch, 0c6h, 0c6h, 06ch, 038h
    db  00ch, 0c6h, 07ch, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 0feh, 0feh, 0feh, 000h
    db  000h, 000h, 000h, 000h, 018h, 03ch, 07eh, 018h, 018h, 018h, 07eh, 03ch, 018h, 07eh, 000h, 000h
    db  000h, 000h, 018h, 03ch, 07eh, 018h, 018h, 018h, 018h, 018h, 018h, 000h, 000h, 000h, 000h, 000h
    db  018h, 018h, 018h, 018h, 018h, 018h, 07eh, 03ch, 018h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  018h, 00ch, 0feh, 00ch, 018h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 030h, 060h
    db  0feh, 060h, 030h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 0c0h, 0c0h, 0c0h
    db  0feh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 028h, 06ch, 0feh, 06ch, 028h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 010h, 038h, 038h, 07ch, 07ch, 0feh, 0feh, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 0feh, 0feh, 07ch, 07ch, 038h, 038h, 010h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  018h, 03ch, 03ch, 03ch, 018h, 018h, 000h, 018h, 018h, 000h, 000h, 000h, 000h, 066h, 066h, 066h
    db  024h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 06ch, 06ch, 0feh, 06ch
    db  06ch, 06ch, 0feh, 06ch, 06ch, 000h, 000h, 000h, 018h, 018h, 07ch, 0c6h, 0c2h, 0c0h, 07ch, 006h
    db  086h, 0c6h, 07ch, 018h, 018h, 000h, 000h, 000h, 000h, 000h, 0c2h, 0c6h, 00ch, 018h, 030h, 066h
    db  0c6h, 000h, 000h, 000h, 000h, 000h, 038h, 06ch, 06ch, 038h, 076h, 0dch, 0cch, 0cch, 076h, 000h
    db  000h, 000h, 000h, 030h, 030h, 030h, 060h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 00ch, 018h, 030h, 030h, 030h, 030h, 030h, 018h, 00ch, 000h, 000h, 000h, 000h, 000h
    db  030h, 018h, 00ch, 00ch, 00ch, 00ch, 00ch, 018h, 030h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  066h, 03ch, 0ffh, 03ch, 066h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 018h, 018h
    db  07eh, 018h, 018h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  018h, 018h, 018h, 030h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 0feh, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 018h, 018h, 000h
    db  000h, 000h, 000h, 000h, 002h, 006h, 00ch, 018h, 030h, 060h, 0c0h, 080h, 000h, 000h, 000h, 000h
    db  000h, 000h, 07ch, 0c6h, 0ceh, 0deh, 0f6h, 0e6h, 0c6h, 0c6h, 07ch, 000h, 000h, 000h, 000h, 000h
    db  018h, 038h, 078h, 018h, 018h, 018h, 018h, 018h, 07eh, 000h, 000h, 000h, 000h, 000h, 07ch, 0c6h
    db  006h, 00ch, 018h, 030h, 060h, 0c6h, 0feh, 000h, 000h, 000h, 000h, 000h, 07ch, 0c6h, 006h, 006h
    db  03ch, 006h, 006h, 0c6h, 07ch, 000h, 000h, 000h, 000h, 000h, 00ch, 01ch, 03ch, 06ch, 0cch, 0feh
    db  00ch, 00ch, 01eh, 000h, 000h, 000h, 000h, 000h, 0feh, 0c0h, 0c0h, 0c0h, 0fch, 006h, 006h, 0c6h
    db  07ch, 000h, 000h, 000h, 000h, 000h, 038h, 060h, 0c0h, 0c0h, 0fch, 0c6h, 0c6h, 0c6h, 07ch, 000h
    db  000h, 000h, 000h, 000h, 0feh, 0c6h, 006h, 00ch, 018h, 030h, 030h, 030h, 030h, 000h, 000h, 000h
    db  000h, 000h, 07ch, 0c6h, 0c6h, 0c6h, 07ch, 0c6h, 0c6h, 0c6h, 07ch, 000h, 000h, 000h, 000h, 000h
    db  07ch, 0c6h, 0c6h, 0c6h, 07eh, 006h, 006h, 00ch, 078h, 000h, 000h, 000h, 000h, 000h, 000h, 018h
    db  018h, 000h, 000h, 000h, 018h, 018h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 018h, 018h, 000h
    db  000h, 000h, 018h, 018h, 030h, 000h, 000h, 000h, 000h, 000h, 006h, 00ch, 018h, 030h, 060h, 030h
    db  018h, 00ch, 006h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 07eh, 000h, 000h, 07eh, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 060h, 030h, 018h, 00ch, 006h, 00ch, 018h, 030h, 060h, 000h
    db  000h, 000h, 000h, 000h, 07ch, 0c6h, 0c6h, 00ch, 018h, 018h, 000h, 018h, 018h, 000h, 000h, 000h
    db  000h, 000h, 07ch, 0c6h, 0c6h, 0deh, 0deh, 0deh, 0dch, 0c0h, 07ch, 000h, 000h, 000h, 000h, 000h
    db  010h, 038h, 06ch, 0c6h, 0c6h, 0feh, 0c6h, 0c6h, 0c6h, 000h, 000h, 000h, 000h, 000h, 0fch, 066h
    db  066h, 066h, 07ch, 066h, 066h, 066h, 0fch, 000h, 000h, 000h, 000h, 000h, 03ch, 066h, 0c2h, 0c0h
    db  0c0h, 0c0h, 0c2h, 066h, 03ch, 000h, 000h, 000h, 000h, 000h, 0f8h, 06ch, 066h, 066h, 066h, 066h
    db  066h, 06ch, 0f8h, 000h, 000h, 000h, 000h, 000h, 0feh, 066h, 062h, 068h, 078h, 068h, 062h, 066h
    db  0feh, 000h, 000h, 000h, 000h, 000h, 0feh, 066h, 062h, 068h, 078h, 068h, 060h, 060h, 0f0h, 000h
    db  000h, 000h, 000h, 000h, 03ch, 066h, 0c2h, 0c0h, 0c0h, 0deh, 0c6h, 066h, 03ah, 000h, 000h, 000h
    db  000h, 000h, 0c6h, 0c6h, 0c6h, 0c6h, 0feh, 0c6h, 0c6h, 0c6h, 0c6h, 000h, 000h, 000h, 000h, 000h
    db  03ch, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 03ch, 000h, 000h, 000h, 000h, 000h, 01eh, 00ch
    db  00ch, 00ch, 00ch, 00ch, 0cch, 0cch, 078h, 000h, 000h, 000h, 000h, 000h, 0e6h, 066h, 06ch, 06ch
    db  078h, 06ch, 06ch, 066h, 0e6h, 000h, 000h, 000h, 000h, 000h, 0f0h, 060h, 060h, 060h, 060h, 060h
    db  062h, 066h, 0feh, 000h, 000h, 000h, 000h, 000h, 0c6h, 0eeh, 0feh, 0feh, 0d6h, 0c6h, 0c6h, 0c6h
    db  0c6h, 000h, 000h, 000h, 000h, 000h, 0c6h, 0e6h, 0f6h, 0feh, 0deh, 0ceh, 0c6h, 0c6h, 0c6h, 000h
    db  000h, 000h, 000h, 000h, 038h, 06ch, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 06ch, 038h, 000h, 000h, 000h
    db  000h, 000h, 0fch, 066h, 066h, 066h, 07ch, 060h, 060h, 060h, 0f0h, 000h, 000h, 000h, 000h, 000h
    db  07ch, 0c6h, 0c6h, 0c6h, 0c6h, 0d6h, 0deh, 07ch, 00ch, 00eh, 000h, 000h, 000h, 000h, 0fch, 066h
    db  066h, 066h, 07ch, 06ch, 066h, 066h, 0e6h, 000h, 000h, 000h, 000h, 000h, 07ch, 0c6h, 0c6h, 060h
    db  038h, 00ch, 0c6h, 0c6h, 07ch, 000h, 000h, 000h, 000h, 000h, 07eh, 07eh, 05ah, 018h, 018h, 018h
    db  018h, 018h, 03ch, 000h, 000h, 000h, 000h, 000h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h
    db  07ch, 000h, 000h, 000h, 000h, 000h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 06ch, 038h, 010h, 000h
    db  000h, 000h, 000h, 000h, 0c6h, 0c6h, 0c6h, 0c6h, 0d6h, 0d6h, 0feh, 07ch, 06ch, 000h, 000h, 000h
    db  000h, 000h, 0c6h, 0c6h, 06ch, 038h, 038h, 038h, 06ch, 0c6h, 0c6h, 000h, 000h, 000h, 000h, 000h
    db  066h, 066h, 066h, 066h, 03ch, 018h, 018h, 018h, 03ch, 000h, 000h, 000h, 000h, 000h, 0feh, 0c6h
    db  08ch, 018h, 030h, 060h, 0c2h, 0c6h, 0feh, 000h, 000h, 000h, 000h, 000h, 03ch, 030h, 030h, 030h
    db  030h, 030h, 030h, 030h, 03ch, 000h, 000h, 000h, 000h, 000h, 080h, 0c0h, 0e0h, 070h, 038h, 01ch
    db  00eh, 006h, 002h, 000h, 000h, 000h, 000h, 000h, 03ch, 00ch, 00ch, 00ch, 00ch, 00ch, 00ch, 00ch
    db  03ch, 000h, 000h, 000h, 010h, 038h, 06ch, 0c6h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 0ffh, 000h
    db  030h, 030h, 018h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 078h, 00ch, 07ch, 0cch, 0cch, 076h, 000h, 000h, 000h, 000h, 000h, 0e0h, 060h
    db  060h, 078h, 06ch, 066h, 066h, 066h, 07ch, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 07ch
    db  0c6h, 0c0h, 0c0h, 0c6h, 07ch, 000h, 000h, 000h, 000h, 000h, 01ch, 00ch, 00ch, 03ch, 06ch, 0cch
    db  0cch, 0cch, 076h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 07ch, 0c6h, 0feh, 0c0h, 0c6h
    db  07ch, 000h, 000h, 000h, 000h, 000h, 038h, 06ch, 064h, 060h, 0f0h, 060h, 060h, 060h, 0f0h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 076h, 0cch, 0cch, 0cch, 07ch, 00ch, 0cch, 078h, 000h
    db  000h, 000h, 0e0h, 060h, 060h, 06ch, 076h, 066h, 066h, 066h, 0e6h, 000h, 000h, 000h, 000h, 000h
    db  018h, 018h, 000h, 038h, 018h, 018h, 018h, 018h, 03ch, 000h, 000h, 000h, 000h, 000h, 006h, 006h
    db  000h, 00eh, 006h, 006h, 006h, 006h, 066h, 066h, 03ch, 000h, 000h, 000h, 0e0h, 060h, 060h, 066h
    db  06ch, 078h, 06ch, 066h, 0e6h, 000h, 000h, 000h, 000h, 000h, 038h, 018h, 018h, 018h, 018h, 018h
    db  018h, 018h, 03ch, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 0ech, 0feh, 0d6h, 0d6h, 0d6h
    db  0c6h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 0dch, 066h, 066h, 066h, 066h, 066h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 07ch, 0c6h, 0c6h, 0c6h, 0c6h, 07ch, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 0dch, 066h, 066h, 066h, 07ch, 060h, 060h, 0f0h, 000h, 000h, 000h
    db  000h, 000h, 000h, 076h, 0cch, 0cch, 0cch, 07ch, 00ch, 00ch, 01eh, 000h, 000h, 000h, 000h, 000h
    db  000h, 0dch, 076h, 066h, 060h, 060h, 0f0h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 07ch
    db  0c6h, 070h, 01ch, 0c6h, 07ch, 000h, 000h, 000h, 000h, 000h, 010h, 030h, 030h, 0fch, 030h, 030h
    db  030h, 036h, 01ch, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 0cch, 0cch, 0cch, 0cch, 0cch
    db  076h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 066h, 066h, 066h, 066h, 03ch, 018h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 0c6h, 0c6h, 0d6h, 0d6h, 0feh, 06ch, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 0c6h, 06ch, 038h, 038h, 06ch, 0c6h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 0c6h, 0c6h, 0c6h, 0c6h, 07eh, 006h, 00ch, 0f8h, 000h, 000h, 000h, 000h, 000h
    db  000h, 0feh, 0cch, 018h, 030h, 066h, 0feh, 000h, 000h, 000h, 000h, 000h, 00eh, 018h, 018h, 018h
    db  070h, 018h, 018h, 018h, 00eh, 000h, 000h, 000h, 000h, 000h, 018h, 018h, 018h, 018h, 000h, 018h
    db  018h, 018h, 018h, 000h, 000h, 000h, 000h, 000h, 070h, 018h, 018h, 018h, 00eh, 018h, 018h, 018h
    db  070h, 000h, 000h, 000h, 000h, 000h, 076h, 0dch, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 010h, 038h, 06ch, 0c6h, 0c6h, 0feh, 000h, 000h, 000h, 000h
    db  000h, 000h, 03ch, 066h, 0c2h, 0c0h, 0c0h, 0c2h, 066h, 03ch, 00ch, 006h, 07ch, 000h, 000h, 000h
    db  0cch, 0cch, 000h, 0cch, 0cch, 0cch, 0cch, 0cch, 076h, 000h, 000h, 000h, 000h, 00ch, 018h, 030h
    db  000h, 07ch, 0c6h, 0feh, 0c0h, 0c6h, 07ch, 000h, 000h, 000h, 000h, 010h, 038h, 06ch, 000h, 078h
    db  00ch, 07ch, 0cch, 0cch, 076h, 000h, 000h, 000h, 000h, 000h, 0cch, 0cch, 000h, 078h, 00ch, 07ch
    db  0cch, 0cch, 076h, 000h, 000h, 000h, 000h, 060h, 030h, 018h, 000h, 078h, 00ch, 07ch, 0cch, 0cch
    db  076h, 000h, 000h, 000h, 000h, 038h, 06ch, 038h, 000h, 078h, 00ch, 07ch, 0cch, 0cch, 076h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 03ch, 066h, 060h, 066h, 03ch, 00ch, 006h, 03ch, 000h, 000h
    db  000h, 010h, 038h, 06ch, 000h, 07ch, 0c6h, 0feh, 0c0h, 0c6h, 07ch, 000h, 000h, 000h, 000h, 000h
    db  0cch, 0cch, 000h, 07ch, 0c6h, 0feh, 0c0h, 0c6h, 07ch, 000h, 000h, 000h, 000h, 060h, 030h, 018h
    db  000h, 07ch, 0c6h, 0feh, 0c0h, 0c6h, 07ch, 000h, 000h, 000h, 000h, 000h, 066h, 066h, 000h, 038h
    db  018h, 018h, 018h, 018h, 03ch, 000h, 000h, 000h, 000h, 018h, 03ch, 066h, 000h, 038h, 018h, 018h
    db  018h, 018h, 03ch, 000h, 000h, 000h, 000h, 060h, 030h, 018h, 000h, 038h, 018h, 018h, 018h, 018h
    db  03ch, 000h, 000h, 000h, 000h, 0c6h, 0c6h, 010h, 038h, 06ch, 0c6h, 0c6h, 0feh, 0c6h, 0c6h, 000h
    db  000h, 000h, 038h, 06ch, 038h, 000h, 038h, 06ch, 0c6h, 0c6h, 0feh, 0c6h, 0c6h, 000h, 000h, 000h
    db  018h, 030h, 060h, 000h, 0feh, 066h, 060h, 07ch, 060h, 066h, 0feh, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 0cch, 076h, 036h, 07eh, 0d8h, 0d8h, 06eh, 000h, 000h, 000h, 000h, 000h, 03eh, 06ch
    db  0cch, 0cch, 0feh, 0cch, 0cch, 0cch, 0ceh, 000h, 000h, 000h, 000h, 010h, 038h, 06ch, 000h, 07ch
    db  0c6h, 0c6h, 0c6h, 0c6h, 07ch, 000h, 000h, 000h, 000h, 000h, 0c6h, 0c6h, 000h, 07ch, 0c6h, 0c6h
    db  0c6h, 0c6h, 07ch, 000h, 000h, 000h, 000h, 060h, 030h, 018h, 000h, 07ch, 0c6h, 0c6h, 0c6h, 0c6h
    db  07ch, 000h, 000h, 000h, 000h, 030h, 078h, 0cch, 000h, 0cch, 0cch, 0cch, 0cch, 0cch, 076h, 000h
    db  000h, 000h, 000h, 060h, 030h, 018h, 000h, 0cch, 0cch, 0cch, 0cch, 0cch, 076h, 000h, 000h, 000h
    db  000h, 000h, 0c6h, 0c6h, 000h, 0c6h, 0c6h, 0c6h, 0c6h, 07eh, 006h, 00ch, 078h, 000h, 000h, 0c6h
    db  0c6h, 038h, 06ch, 0c6h, 0c6h, 0c6h, 0c6h, 06ch, 038h, 000h, 000h, 000h, 000h, 0c6h, 0c6h, 000h
    db  0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 07ch, 000h, 000h, 000h, 000h, 018h, 018h, 03ch, 066h, 060h
    db  060h, 066h, 03ch, 018h, 018h, 000h, 000h, 000h, 000h, 038h, 06ch, 064h, 060h, 0f0h, 060h, 060h
    db  060h, 0e6h, 0fch, 000h, 000h, 000h, 000h, 000h, 066h, 066h, 03ch, 018h, 07eh, 018h, 07eh, 018h
    db  018h, 000h, 000h, 000h, 000h, 0f8h, 0cch, 0cch, 0f8h, 0c4h, 0cch, 0deh, 0cch, 0cch, 0c6h, 000h
    db  000h, 000h, 000h, 00eh, 01bh, 018h, 018h, 018h, 07eh, 018h, 018h, 018h, 018h, 0d8h, 070h, 000h
    db  000h, 018h, 030h, 060h, 000h, 078h, 00ch, 07ch, 0cch, 0cch, 076h, 000h, 000h, 000h, 000h, 00ch
    db  018h, 030h, 000h, 038h, 018h, 018h, 018h, 018h, 03ch, 000h, 000h, 000h, 000h, 018h, 030h, 060h
    db  000h, 07ch, 0c6h, 0c6h, 0c6h, 0c6h, 07ch, 000h, 000h, 000h, 000h, 018h, 030h, 060h, 000h, 0cch
    db  0cch, 0cch, 0cch, 0cch, 076h, 000h, 000h, 000h, 000h, 000h, 076h, 0dch, 000h, 0dch, 066h, 066h
    db  066h, 066h, 066h, 000h, 000h, 000h, 076h, 0dch, 000h, 0c6h, 0e6h, 0f6h, 0feh, 0deh, 0ceh, 0c6h
    db  0c6h, 000h, 000h, 000h, 000h, 03ch, 06ch, 06ch, 03eh, 000h, 07eh, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 038h, 06ch, 06ch, 038h, 000h, 07ch, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 030h, 030h, 000h, 030h, 030h, 060h, 0c6h, 0c6h, 07ch, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 0feh, 0c0h, 0c0h, 0c0h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 0feh, 006h, 006h, 006h, 000h, 000h, 000h, 000h, 000h, 0c0h, 0c0h, 0c6h, 0cch, 0d8h
    db  030h, 060h, 0dch, 086h, 00ch, 018h, 03eh, 000h, 000h, 0c0h, 0c0h, 0c6h, 0cch, 0d8h, 030h, 066h
    db  0ceh, 09eh, 03eh, 006h, 006h, 000h, 000h, 000h, 018h, 018h, 000h, 018h, 018h, 03ch, 03ch, 03ch
    db  018h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 036h, 06ch, 0d8h, 06ch, 036h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 0d8h, 06ch, 036h, 06ch, 0d8h, 000h, 000h, 000h, 000h, 000h
    db  011h, 044h, 011h, 044h, 011h, 044h, 011h, 044h, 011h, 044h, 011h, 044h, 011h, 044h, 055h, 0aah
    db  055h, 0aah, 055h, 0aah, 055h, 0aah, 055h, 0aah, 055h, 0aah, 055h, 0aah, 0ddh, 077h, 0ddh, 077h
    db  0ddh, 077h, 0ddh, 077h, 0ddh, 077h, 0ddh, 077h, 0ddh, 077h, 018h, 018h, 018h, 018h, 018h, 018h
    db  018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 0f8h
    db  018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 0f8h, 018h, 0f8h, 018h, 018h
    db  018h, 018h, 018h, 018h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 0f6h, 036h, 036h, 036h, 036h
    db  036h, 036h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 0feh, 036h, 036h, 036h, 036h, 036h, 036h
    db  000h, 000h, 000h, 000h, 000h, 0f8h, 018h, 0f8h, 018h, 018h, 018h, 018h, 018h, 018h, 036h, 036h
    db  036h, 036h, 036h, 0f6h, 006h, 0f6h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h
    db  036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 000h, 000h, 000h, 000h, 000h, 0feh
    db  006h, 0f6h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 0f6h, 006h, 0feh
    db  000h, 000h, 000h, 000h, 000h, 000h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 0feh, 000h, 000h
    db  000h, 000h, 000h, 000h, 018h, 018h, 018h, 018h, 018h, 0f8h, 018h, 0f8h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 0f8h, 018h, 018h, 018h, 018h, 018h, 018h
    db  018h, 018h, 018h, 018h, 018h, 018h, 018h, 01fh, 000h, 000h, 000h, 000h, 000h, 000h, 018h, 018h
    db  018h, 018h, 018h, 018h, 018h, 0ffh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 0ffh, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h
    db  018h, 01fh, 018h, 018h, 018h, 018h, 018h, 018h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 0ffh
    db  000h, 000h, 000h, 000h, 000h, 000h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 0ffh, 018h, 018h
    db  018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 01fh, 018h, 01fh, 018h, 018h, 018h, 018h
    db  018h, 018h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 037h, 036h, 036h, 036h, 036h, 036h, 036h
    db  036h, 036h, 036h, 036h, 036h, 037h, 030h, 03fh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 03fh, 030h, 037h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h
    db  036h, 0f7h, 000h, 0ffh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 0ffh
    db  000h, 0f7h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 037h, 030h, 037h
    db  036h, 036h, 036h, 036h, 036h, 036h, 000h, 000h, 000h, 000h, 000h, 0ffh, 000h, 0ffh, 000h, 000h
    db  000h, 000h, 000h, 000h, 036h, 036h, 036h, 036h, 036h, 0f7h, 000h, 0f7h, 036h, 036h, 036h, 036h
    db  036h, 036h, 018h, 018h, 018h, 018h, 018h, 0ffh, 000h, 0ffh, 000h, 000h, 000h, 000h, 000h, 000h
    db  036h, 036h, 036h, 036h, 036h, 036h, 036h, 0ffh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 0ffh, 000h, 0ffh, 018h, 018h, 018h, 018h, 018h, 018h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 0ffh, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h
    db  036h, 03fh, 000h, 000h, 000h, 000h, 000h, 000h, 018h, 018h, 018h, 018h, 018h, 01fh, 018h, 01fh
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 01fh, 018h, 01fh, 018h, 018h
    db  018h, 018h, 018h, 018h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 03fh, 036h, 036h, 036h, 036h
    db  036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 0ffh, 036h, 036h, 036h, 036h, 036h, 036h
    db  018h, 018h, 018h, 018h, 018h, 0ffh, 018h, 0ffh, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h
    db  018h, 018h, 018h, 018h, 018h, 0f8h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 01fh, 018h, 018h, 018h, 018h, 018h, 018h, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh
    db  0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 0ffh
    db  0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0f0h, 0f0h, 0f0h, 0f0h, 0f0h, 0f0h, 0f0h, 0f0h, 0f0h, 0f0h
    db  0f0h, 0f0h, 0f0h, 0f0h, 00fh, 00fh, 00fh, 00fh, 00fh, 00fh, 00fh, 00fh, 00fh, 00fh, 00fh, 00fh
    db  00fh, 00fh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 076h, 0dch, 0d8h, 0d8h, 0dch, 076h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 07ch, 0c6h, 0fch, 0c6h, 0c6h, 0fch, 0c0h, 0c0h, 040h, 000h, 000h, 000h, 0feh, 0c6h
    db  0c6h, 0c0h, 0c0h, 0c0h, 0c0h, 0c0h, 0c0h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 0feh, 06ch
    db  06ch, 06ch, 06ch, 06ch, 06ch, 000h, 000h, 000h, 000h, 000h, 0feh, 0c6h, 060h, 030h, 018h, 030h
    db  060h, 0c6h, 0feh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 07eh, 0d8h, 0d8h, 0d8h, 0d8h
    db  070h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 066h, 066h, 066h, 066h, 07ch, 060h, 060h, 0c0h
    db  000h, 000h, 000h, 000h, 000h, 000h, 076h, 0dch, 018h, 018h, 018h, 018h, 018h, 000h, 000h, 000h
    db  000h, 000h, 07eh, 018h, 03ch, 066h, 066h, 066h, 03ch, 018h, 07eh, 000h, 000h, 000h, 000h, 000h
    db  038h, 06ch, 0c6h, 0c6h, 0feh, 0c6h, 0c6h, 06ch, 038h, 000h, 000h, 000h, 000h, 000h, 038h, 06ch
    db  0c6h, 0c6h, 0c6h, 06ch, 06ch, 06ch, 0eeh, 000h, 000h, 000h, 000h, 000h, 01eh, 030h, 018h, 00ch
    db  03eh, 066h, 066h, 066h, 03ch, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 07eh, 0dbh, 0dbh
    db  07eh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 003h, 006h, 07eh, 0dbh, 0dbh, 0f3h, 07eh, 060h
    db  0c0h, 000h, 000h, 000h, 000h, 000h, 01ch, 030h, 060h, 060h, 07ch, 060h, 060h, 030h, 01ch, 000h
    db  000h, 000h, 000h, 000h, 000h, 07ch, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 000h, 000h, 000h
    db  000h, 000h, 000h, 0feh, 000h, 000h, 0feh, 000h, 000h, 0feh, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 018h, 018h, 07eh, 018h, 018h, 000h, 000h, 0ffh, 000h, 000h, 000h, 000h, 000h, 030h, 018h
    db  00ch, 006h, 00ch, 018h, 030h, 000h, 07eh, 000h, 000h, 000h, 000h, 000h, 00ch, 018h, 030h, 060h
    db  030h, 018h, 00ch, 000h, 07eh, 000h, 000h, 000h, 000h, 000h, 00eh, 01bh, 01bh, 018h, 018h, 018h
    db  018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 0d8h, 0d8h
    db  070h, 000h, 000h, 000h, 000h, 000h, 000h, 018h, 018h, 000h, 07eh, 000h, 018h, 018h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 076h, 0dch, 000h, 076h, 0dch, 000h, 000h, 000h, 000h, 000h
    db  000h, 038h, 06ch, 06ch, 038h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 018h, 018h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 018h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 00fh, 00ch, 00ch, 00ch, 00ch
    db  00ch, 0ech, 06ch, 03ch, 01ch, 000h, 000h, 000h, 000h, 0d8h, 06ch, 06ch, 06ch, 06ch, 06ch, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 070h, 0d8h, 030h, 060h, 0c8h, 0f8h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 07ch, 07ch, 07ch, 07ch, 07ch, 07ch, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
_vgafont16:                                  ; 0xc6baf LB 0x1000
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 07eh, 081h, 0a5h, 081h, 081h, 0bdh, 099h, 081h, 081h, 07eh, 000h, 000h, 000h, 000h
    db  000h, 000h, 07eh, 0ffh, 0dbh, 0ffh, 0ffh, 0c3h, 0e7h, 0ffh, 0ffh, 07eh, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 06ch, 0feh, 0feh, 0feh, 0feh, 07ch, 038h, 010h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 010h, 038h, 07ch, 0feh, 07ch, 038h, 010h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 018h, 03ch, 03ch, 0e7h, 0e7h, 0e7h, 018h, 018h, 03ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 018h, 03ch, 07eh, 0ffh, 0ffh, 07eh, 018h, 018h, 03ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 018h, 03ch, 03ch, 018h, 000h, 000h, 000h, 000h, 000h, 000h
    db  0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0e7h, 0c3h, 0c3h, 0e7h, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh
    db  000h, 000h, 000h, 000h, 000h, 03ch, 066h, 042h, 042h, 066h, 03ch, 000h, 000h, 000h, 000h, 000h
    db  0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0c3h, 099h, 0bdh, 0bdh, 099h, 0c3h, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh
    db  000h, 000h, 01eh, 00eh, 01ah, 032h, 078h, 0cch, 0cch, 0cch, 0cch, 078h, 000h, 000h, 000h, 000h
    db  000h, 000h, 03ch, 066h, 066h, 066h, 066h, 03ch, 018h, 07eh, 018h, 018h, 000h, 000h, 000h, 000h
    db  000h, 000h, 03fh, 033h, 03fh, 030h, 030h, 030h, 030h, 070h, 0f0h, 0e0h, 000h, 000h, 000h, 000h
    db  000h, 000h, 07fh, 063h, 07fh, 063h, 063h, 063h, 063h, 067h, 0e7h, 0e6h, 0c0h, 000h, 000h, 000h
    db  000h, 000h, 000h, 018h, 018h, 0dbh, 03ch, 0e7h, 03ch, 0dbh, 018h, 018h, 000h, 000h, 000h, 000h
    db  000h, 080h, 0c0h, 0e0h, 0f0h, 0f8h, 0feh, 0f8h, 0f0h, 0e0h, 0c0h, 080h, 000h, 000h, 000h, 000h
    db  000h, 002h, 006h, 00eh, 01eh, 03eh, 0feh, 03eh, 01eh, 00eh, 006h, 002h, 000h, 000h, 000h, 000h
    db  000h, 000h, 018h, 03ch, 07eh, 018h, 018h, 018h, 07eh, 03ch, 018h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 066h, 066h, 066h, 066h, 066h, 066h, 066h, 000h, 066h, 066h, 000h, 000h, 000h, 000h
    db  000h, 000h, 07fh, 0dbh, 0dbh, 0dbh, 07bh, 01bh, 01bh, 01bh, 01bh, 01bh, 000h, 000h, 000h, 000h
    db  000h, 07ch, 0c6h, 060h, 038h, 06ch, 0c6h, 0c6h, 06ch, 038h, 00ch, 0c6h, 07ch, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 0feh, 0feh, 0feh, 0feh, 000h, 000h, 000h, 000h
    db  000h, 000h, 018h, 03ch, 07eh, 018h, 018h, 018h, 07eh, 03ch, 018h, 07eh, 000h, 000h, 000h, 000h
    db  000h, 000h, 018h, 03ch, 07eh, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 000h, 000h, 000h, 000h
    db  000h, 000h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 07eh, 03ch, 018h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 018h, 00ch, 0feh, 00ch, 018h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 030h, 060h, 0feh, 060h, 030h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 0c0h, 0c0h, 0c0h, 0feh, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 024h, 066h, 0ffh, 066h, 024h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 010h, 038h, 038h, 07ch, 07ch, 0feh, 0feh, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 0feh, 0feh, 07ch, 07ch, 038h, 038h, 010h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 018h, 03ch, 03ch, 03ch, 018h, 018h, 018h, 000h, 018h, 018h, 000h, 000h, 000h, 000h
    db  000h, 066h, 066h, 066h, 024h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 06ch, 06ch, 0feh, 06ch, 06ch, 06ch, 0feh, 06ch, 06ch, 000h, 000h, 000h, 000h
    db  018h, 018h, 07ch, 0c6h, 0c2h, 0c0h, 07ch, 006h, 006h, 086h, 0c6h, 07ch, 018h, 018h, 000h, 000h
    db  000h, 000h, 000h, 000h, 0c2h, 0c6h, 00ch, 018h, 030h, 060h, 0c6h, 086h, 000h, 000h, 000h, 000h
    db  000h, 000h, 038h, 06ch, 06ch, 038h, 076h, 0dch, 0cch, 0cch, 0cch, 076h, 000h, 000h, 000h, 000h
    db  000h, 030h, 030h, 030h, 060h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 00ch, 018h, 030h, 030h, 030h, 030h, 030h, 030h, 018h, 00ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 030h, 018h, 00ch, 00ch, 00ch, 00ch, 00ch, 00ch, 018h, 030h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 066h, 03ch, 0ffh, 03ch, 066h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 018h, 018h, 07eh, 018h, 018h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 018h, 018h, 018h, 030h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 0feh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 018h, 018h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 002h, 006h, 00ch, 018h, 030h, 060h, 0c0h, 080h, 000h, 000h, 000h, 000h
    db  000h, 000h, 03ch, 066h, 0c3h, 0c3h, 0dbh, 0dbh, 0c3h, 0c3h, 066h, 03ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 018h, 038h, 078h, 018h, 018h, 018h, 018h, 018h, 018h, 07eh, 000h, 000h, 000h, 000h
    db  000h, 000h, 07ch, 0c6h, 006h, 00ch, 018h, 030h, 060h, 0c0h, 0c6h, 0feh, 000h, 000h, 000h, 000h
    db  000h, 000h, 07ch, 0c6h, 006h, 006h, 03ch, 006h, 006h, 006h, 0c6h, 07ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 00ch, 01ch, 03ch, 06ch, 0cch, 0feh, 00ch, 00ch, 00ch, 01eh, 000h, 000h, 000h, 000h
    db  000h, 000h, 0feh, 0c0h, 0c0h, 0c0h, 0fch, 006h, 006h, 006h, 0c6h, 07ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 038h, 060h, 0c0h, 0c0h, 0fch, 0c6h, 0c6h, 0c6h, 0c6h, 07ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 0feh, 0c6h, 006h, 006h, 00ch, 018h, 030h, 030h, 030h, 030h, 000h, 000h, 000h, 000h
    db  000h, 000h, 07ch, 0c6h, 0c6h, 0c6h, 07ch, 0c6h, 0c6h, 0c6h, 0c6h, 07ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 07ch, 0c6h, 0c6h, 0c6h, 07eh, 006h, 006h, 006h, 00ch, 078h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 018h, 018h, 000h, 000h, 000h, 018h, 018h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 018h, 018h, 000h, 000h, 000h, 018h, 018h, 030h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 006h, 00ch, 018h, 030h, 060h, 030h, 018h, 00ch, 006h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 07eh, 000h, 000h, 07eh, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 060h, 030h, 018h, 00ch, 006h, 00ch, 018h, 030h, 060h, 000h, 000h, 000h, 000h
    db  000h, 000h, 07ch, 0c6h, 0c6h, 00ch, 018h, 018h, 018h, 000h, 018h, 018h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 07ch, 0c6h, 0c6h, 0deh, 0deh, 0deh, 0dch, 0c0h, 07ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 010h, 038h, 06ch, 0c6h, 0c6h, 0feh, 0c6h, 0c6h, 0c6h, 0c6h, 000h, 000h, 000h, 000h
    db  000h, 000h, 0fch, 066h, 066h, 066h, 07ch, 066h, 066h, 066h, 066h, 0fch, 000h, 000h, 000h, 000h
    db  000h, 000h, 03ch, 066h, 0c2h, 0c0h, 0c0h, 0c0h, 0c0h, 0c2h, 066h, 03ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 0f8h, 06ch, 066h, 066h, 066h, 066h, 066h, 066h, 06ch, 0f8h, 000h, 000h, 000h, 000h
    db  000h, 000h, 0feh, 066h, 062h, 068h, 078h, 068h, 060h, 062h, 066h, 0feh, 000h, 000h, 000h, 000h
    db  000h, 000h, 0feh, 066h, 062h, 068h, 078h, 068h, 060h, 060h, 060h, 0f0h, 000h, 000h, 000h, 000h
    db  000h, 000h, 03ch, 066h, 0c2h, 0c0h, 0c0h, 0deh, 0c6h, 0c6h, 066h, 03ah, 000h, 000h, 000h, 000h
    db  000h, 000h, 0c6h, 0c6h, 0c6h, 0c6h, 0feh, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 000h, 000h, 000h, 000h
    db  000h, 000h, 03ch, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 03ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 01eh, 00ch, 00ch, 00ch, 00ch, 00ch, 0cch, 0cch, 0cch, 078h, 000h, 000h, 000h, 000h
    db  000h, 000h, 0e6h, 066h, 066h, 06ch, 078h, 078h, 06ch, 066h, 066h, 0e6h, 000h, 000h, 000h, 000h
    db  000h, 000h, 0f0h, 060h, 060h, 060h, 060h, 060h, 060h, 062h, 066h, 0feh, 000h, 000h, 000h, 000h
    db  000h, 000h, 0c3h, 0e7h, 0ffh, 0ffh, 0dbh, 0c3h, 0c3h, 0c3h, 0c3h, 0c3h, 000h, 000h, 000h, 000h
    db  000h, 000h, 0c6h, 0e6h, 0f6h, 0feh, 0deh, 0ceh, 0c6h, 0c6h, 0c6h, 0c6h, 000h, 000h, 000h, 000h
    db  000h, 000h, 07ch, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 07ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 0fch, 066h, 066h, 066h, 07ch, 060h, 060h, 060h, 060h, 0f0h, 000h, 000h, 000h, 000h
    db  000h, 000h, 07ch, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0d6h, 0deh, 07ch, 00ch, 00eh, 000h, 000h
    db  000h, 000h, 0fch, 066h, 066h, 066h, 07ch, 06ch, 066h, 066h, 066h, 0e6h, 000h, 000h, 000h, 000h
    db  000h, 000h, 07ch, 0c6h, 0c6h, 060h, 038h, 00ch, 006h, 0c6h, 0c6h, 07ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 0ffh, 0dbh, 099h, 018h, 018h, 018h, 018h, 018h, 018h, 03ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 07ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 0c3h, 0c3h, 0c3h, 0c3h, 0c3h, 0c3h, 0c3h, 066h, 03ch, 018h, 000h, 000h, 000h, 000h
    db  000h, 000h, 0c3h, 0c3h, 0c3h, 0c3h, 0c3h, 0dbh, 0dbh, 0ffh, 066h, 066h, 000h, 000h, 000h, 000h
    db  000h, 000h, 0c3h, 0c3h, 066h, 03ch, 018h, 018h, 03ch, 066h, 0c3h, 0c3h, 000h, 000h, 000h, 000h
    db  000h, 000h, 0c3h, 0c3h, 0c3h, 066h, 03ch, 018h, 018h, 018h, 018h, 03ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 0ffh, 0c3h, 086h, 00ch, 018h, 030h, 060h, 0c1h, 0c3h, 0ffh, 000h, 000h, 000h, 000h
    db  000h, 000h, 03ch, 030h, 030h, 030h, 030h, 030h, 030h, 030h, 030h, 03ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 080h, 0c0h, 0e0h, 070h, 038h, 01ch, 00eh, 006h, 002h, 000h, 000h, 000h, 000h
    db  000h, 000h, 03ch, 00ch, 00ch, 00ch, 00ch, 00ch, 00ch, 00ch, 00ch, 03ch, 000h, 000h, 000h, 000h
    db  010h, 038h, 06ch, 0c6h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 0ffh, 000h, 000h
    db  030h, 030h, 018h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 078h, 00ch, 07ch, 0cch, 0cch, 0cch, 076h, 000h, 000h, 000h, 000h
    db  000h, 000h, 0e0h, 060h, 060h, 078h, 06ch, 066h, 066h, 066h, 066h, 07ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 07ch, 0c6h, 0c0h, 0c0h, 0c0h, 0c6h, 07ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 01ch, 00ch, 00ch, 03ch, 06ch, 0cch, 0cch, 0cch, 0cch, 076h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 07ch, 0c6h, 0feh, 0c0h, 0c0h, 0c6h, 07ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 038h, 06ch, 064h, 060h, 0f0h, 060h, 060h, 060h, 060h, 0f0h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 076h, 0cch, 0cch, 0cch, 0cch, 0cch, 07ch, 00ch, 0cch, 078h, 000h
    db  000h, 000h, 0e0h, 060h, 060h, 06ch, 076h, 066h, 066h, 066h, 066h, 0e6h, 000h, 000h, 000h, 000h
    db  000h, 000h, 018h, 018h, 000h, 038h, 018h, 018h, 018h, 018h, 018h, 03ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 006h, 006h, 000h, 00eh, 006h, 006h, 006h, 006h, 006h, 006h, 066h, 066h, 03ch, 000h
    db  000h, 000h, 0e0h, 060h, 060h, 066h, 06ch, 078h, 078h, 06ch, 066h, 0e6h, 000h, 000h, 000h, 000h
    db  000h, 000h, 038h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 03ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 0e6h, 0ffh, 0dbh, 0dbh, 0dbh, 0dbh, 0dbh, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 0dch, 066h, 066h, 066h, 066h, 066h, 066h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 07ch, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 07ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 0dch, 066h, 066h, 066h, 066h, 066h, 07ch, 060h, 060h, 0f0h, 000h
    db  000h, 000h, 000h, 000h, 000h, 076h, 0cch, 0cch, 0cch, 0cch, 0cch, 07ch, 00ch, 00ch, 01eh, 000h
    db  000h, 000h, 000h, 000h, 000h, 0dch, 076h, 066h, 060h, 060h, 060h, 0f0h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 07ch, 0c6h, 060h, 038h, 00ch, 0c6h, 07ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 010h, 030h, 030h, 0fch, 030h, 030h, 030h, 030h, 036h, 01ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 0cch, 0cch, 0cch, 0cch, 0cch, 0cch, 076h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 0c3h, 0c3h, 0c3h, 0c3h, 066h, 03ch, 018h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 0c3h, 0c3h, 0c3h, 0dbh, 0dbh, 0ffh, 066h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 0c3h, 066h, 03ch, 018h, 03ch, 066h, 0c3h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 07eh, 006h, 00ch, 0f8h, 000h
    db  000h, 000h, 000h, 000h, 000h, 0feh, 0cch, 018h, 030h, 060h, 0c6h, 0feh, 000h, 000h, 000h, 000h
    db  000h, 000h, 00eh, 018h, 018h, 018h, 070h, 018h, 018h, 018h, 018h, 00eh, 000h, 000h, 000h, 000h
    db  000h, 000h, 018h, 018h, 018h, 018h, 000h, 018h, 018h, 018h, 018h, 018h, 000h, 000h, 000h, 000h
    db  000h, 000h, 070h, 018h, 018h, 018h, 00eh, 018h, 018h, 018h, 018h, 070h, 000h, 000h, 000h, 000h
    db  000h, 000h, 076h, 0dch, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 010h, 038h, 06ch, 0c6h, 0c6h, 0c6h, 0feh, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 03ch, 066h, 0c2h, 0c0h, 0c0h, 0c0h, 0c2h, 066h, 03ch, 00ch, 006h, 07ch, 000h, 000h
    db  000h, 000h, 0cch, 000h, 000h, 0cch, 0cch, 0cch, 0cch, 0cch, 0cch, 076h, 000h, 000h, 000h, 000h
    db  000h, 00ch, 018h, 030h, 000h, 07ch, 0c6h, 0feh, 0c0h, 0c0h, 0c6h, 07ch, 000h, 000h, 000h, 000h
    db  000h, 010h, 038h, 06ch, 000h, 078h, 00ch, 07ch, 0cch, 0cch, 0cch, 076h, 000h, 000h, 000h, 000h
    db  000h, 000h, 0cch, 000h, 000h, 078h, 00ch, 07ch, 0cch, 0cch, 0cch, 076h, 000h, 000h, 000h, 000h
    db  000h, 060h, 030h, 018h, 000h, 078h, 00ch, 07ch, 0cch, 0cch, 0cch, 076h, 000h, 000h, 000h, 000h
    db  000h, 038h, 06ch, 038h, 000h, 078h, 00ch, 07ch, 0cch, 0cch, 0cch, 076h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 03ch, 066h, 060h, 060h, 066h, 03ch, 00ch, 006h, 03ch, 000h, 000h, 000h
    db  000h, 010h, 038h, 06ch, 000h, 07ch, 0c6h, 0feh, 0c0h, 0c0h, 0c6h, 07ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 0c6h, 000h, 000h, 07ch, 0c6h, 0feh, 0c0h, 0c0h, 0c6h, 07ch, 000h, 000h, 000h, 000h
    db  000h, 060h, 030h, 018h, 000h, 07ch, 0c6h, 0feh, 0c0h, 0c0h, 0c6h, 07ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 066h, 000h, 000h, 038h, 018h, 018h, 018h, 018h, 018h, 03ch, 000h, 000h, 000h, 000h
    db  000h, 018h, 03ch, 066h, 000h, 038h, 018h, 018h, 018h, 018h, 018h, 03ch, 000h, 000h, 000h, 000h
    db  000h, 060h, 030h, 018h, 000h, 038h, 018h, 018h, 018h, 018h, 018h, 03ch, 000h, 000h, 000h, 000h
    db  000h, 0c6h, 000h, 010h, 038h, 06ch, 0c6h, 0c6h, 0feh, 0c6h, 0c6h, 0c6h, 000h, 000h, 000h, 000h
    db  038h, 06ch, 038h, 000h, 038h, 06ch, 0c6h, 0c6h, 0feh, 0c6h, 0c6h, 0c6h, 000h, 000h, 000h, 000h
    db  018h, 030h, 060h, 000h, 0feh, 066h, 060h, 07ch, 060h, 060h, 066h, 0feh, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 06eh, 03bh, 01bh, 07eh, 0d8h, 0dch, 077h, 000h, 000h, 000h, 000h
    db  000h, 000h, 03eh, 06ch, 0cch, 0cch, 0feh, 0cch, 0cch, 0cch, 0cch, 0ceh, 000h, 000h, 000h, 000h
    db  000h, 010h, 038h, 06ch, 000h, 07ch, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 07ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 0c6h, 000h, 000h, 07ch, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 07ch, 000h, 000h, 000h, 000h
    db  000h, 060h, 030h, 018h, 000h, 07ch, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 07ch, 000h, 000h, 000h, 000h
    db  000h, 030h, 078h, 0cch, 000h, 0cch, 0cch, 0cch, 0cch, 0cch, 0cch, 076h, 000h, 000h, 000h, 000h
    db  000h, 060h, 030h, 018h, 000h, 0cch, 0cch, 0cch, 0cch, 0cch, 0cch, 076h, 000h, 000h, 000h, 000h
    db  000h, 000h, 0c6h, 000h, 000h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 07eh, 006h, 00ch, 078h, 000h
    db  000h, 0c6h, 000h, 07ch, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 07ch, 000h, 000h, 000h, 000h
    db  000h, 0c6h, 000h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 07ch, 000h, 000h, 000h, 000h
    db  000h, 018h, 018h, 07eh, 0c3h, 0c0h, 0c0h, 0c0h, 0c3h, 07eh, 018h, 018h, 000h, 000h, 000h, 000h
    db  000h, 038h, 06ch, 064h, 060h, 0f0h, 060h, 060h, 060h, 060h, 0e6h, 0fch, 000h, 000h, 000h, 000h
    db  000h, 000h, 0c3h, 066h, 03ch, 018h, 0ffh, 018h, 0ffh, 018h, 018h, 018h, 000h, 000h, 000h, 000h
    db  000h, 0fch, 066h, 066h, 07ch, 062h, 066h, 06fh, 066h, 066h, 066h, 0f3h, 000h, 000h, 000h, 000h
    db  000h, 00eh, 01bh, 018h, 018h, 018h, 07eh, 018h, 018h, 018h, 018h, 018h, 0d8h, 070h, 000h, 000h
    db  000h, 018h, 030h, 060h, 000h, 078h, 00ch, 07ch, 0cch, 0cch, 0cch, 076h, 000h, 000h, 000h, 000h
    db  000h, 00ch, 018h, 030h, 000h, 038h, 018h, 018h, 018h, 018h, 018h, 03ch, 000h, 000h, 000h, 000h
    db  000h, 018h, 030h, 060h, 000h, 07ch, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 07ch, 000h, 000h, 000h, 000h
    db  000h, 018h, 030h, 060h, 000h, 0cch, 0cch, 0cch, 0cch, 0cch, 0cch, 076h, 000h, 000h, 000h, 000h
    db  000h, 000h, 076h, 0dch, 000h, 0dch, 066h, 066h, 066h, 066h, 066h, 066h, 000h, 000h, 000h, 000h
    db  076h, 0dch, 000h, 0c6h, 0e6h, 0f6h, 0feh, 0deh, 0ceh, 0c6h, 0c6h, 0c6h, 000h, 000h, 000h, 000h
    db  000h, 03ch, 06ch, 06ch, 03eh, 000h, 07eh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 038h, 06ch, 06ch, 038h, 000h, 07ch, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 030h, 030h, 000h, 030h, 030h, 060h, 0c0h, 0c6h, 0c6h, 07ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 0feh, 0c0h, 0c0h, 0c0h, 0c0h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 0feh, 006h, 006h, 006h, 006h, 000h, 000h, 000h, 000h, 000h
    db  000h, 0c0h, 0c0h, 0c2h, 0c6h, 0cch, 018h, 030h, 060h, 0ceh, 09bh, 006h, 00ch, 01fh, 000h, 000h
    db  000h, 0c0h, 0c0h, 0c2h, 0c6h, 0cch, 018h, 030h, 066h, 0ceh, 096h, 03eh, 006h, 006h, 000h, 000h
    db  000h, 000h, 018h, 018h, 000h, 018h, 018h, 018h, 03ch, 03ch, 03ch, 018h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 036h, 06ch, 0d8h, 06ch, 036h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 0d8h, 06ch, 036h, 06ch, 0d8h, 000h, 000h, 000h, 000h, 000h, 000h
    db  011h, 044h, 011h, 044h, 011h, 044h, 011h, 044h, 011h, 044h, 011h, 044h, 011h, 044h, 011h, 044h
    db  055h, 0aah, 055h, 0aah, 055h, 0aah, 055h, 0aah, 055h, 0aah, 055h, 0aah, 055h, 0aah, 055h, 0aah
    db  0ddh, 077h, 0ddh, 077h, 0ddh, 077h, 0ddh, 077h, 0ddh, 077h, 0ddh, 077h, 0ddh, 077h, 0ddh, 077h
    db  018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h
    db  018h, 018h, 018h, 018h, 018h, 018h, 018h, 0f8h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h
    db  018h, 018h, 018h, 018h, 018h, 0f8h, 018h, 0f8h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h
    db  036h, 036h, 036h, 036h, 036h, 036h, 036h, 0f6h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 0feh, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h
    db  000h, 000h, 000h, 000h, 000h, 0f8h, 018h, 0f8h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h
    db  036h, 036h, 036h, 036h, 036h, 0f6h, 006h, 0f6h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h
    db  036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h
    db  000h, 000h, 000h, 000h, 000h, 0feh, 006h, 0f6h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h
    db  036h, 036h, 036h, 036h, 036h, 0f6h, 006h, 0feh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  036h, 036h, 036h, 036h, 036h, 036h, 036h, 0feh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  018h, 018h, 018h, 018h, 018h, 0f8h, 018h, 0f8h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 0f8h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h
    db  018h, 018h, 018h, 018h, 018h, 018h, 018h, 01fh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  018h, 018h, 018h, 018h, 018h, 018h, 018h, 0ffh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 0ffh, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h
    db  018h, 018h, 018h, 018h, 018h, 018h, 018h, 01fh, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 0ffh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  018h, 018h, 018h, 018h, 018h, 018h, 018h, 0ffh, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h
    db  018h, 018h, 018h, 018h, 018h, 01fh, 018h, 01fh, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h
    db  036h, 036h, 036h, 036h, 036h, 036h, 036h, 037h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h
    db  036h, 036h, 036h, 036h, 036h, 037h, 030h, 03fh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 03fh, 030h, 037h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h
    db  036h, 036h, 036h, 036h, 036h, 0f7h, 000h, 0ffh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 0ffh, 000h, 0f7h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h
    db  036h, 036h, 036h, 036h, 036h, 037h, 030h, 037h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h
    db  000h, 000h, 000h, 000h, 000h, 0ffh, 000h, 0ffh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  036h, 036h, 036h, 036h, 036h, 0f7h, 000h, 0f7h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h
    db  018h, 018h, 018h, 018h, 018h, 0ffh, 000h, 0ffh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  036h, 036h, 036h, 036h, 036h, 036h, 036h, 0ffh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 0ffh, 000h, 0ffh, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 0ffh, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h
    db  036h, 036h, 036h, 036h, 036h, 036h, 036h, 03fh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  018h, 018h, 018h, 018h, 018h, 01fh, 018h, 01fh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 01fh, 018h, 01fh, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 03fh, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h
    db  036h, 036h, 036h, 036h, 036h, 036h, 036h, 0ffh, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h
    db  018h, 018h, 018h, 018h, 018h, 0ffh, 018h, 0ffh, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h
    db  018h, 018h, 018h, 018h, 018h, 018h, 018h, 0f8h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 01fh, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h
    db  0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh
    db  0f0h, 0f0h, 0f0h, 0f0h, 0f0h, 0f0h, 0f0h, 0f0h, 0f0h, 0f0h, 0f0h, 0f0h, 0f0h, 0f0h, 0f0h, 0f0h
    db  00fh, 00fh, 00fh, 00fh, 00fh, 00fh, 00fh, 00fh, 00fh, 00fh, 00fh, 00fh, 00fh, 00fh, 00fh, 00fh
    db  0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 076h, 0dch, 0d8h, 0d8h, 0d8h, 0dch, 076h, 000h, 000h, 000h, 000h
    db  000h, 000h, 078h, 0cch, 0cch, 0cch, 0d8h, 0cch, 0c6h, 0c6h, 0c6h, 0cch, 000h, 000h, 000h, 000h
    db  000h, 000h, 0feh, 0c6h, 0c6h, 0c0h, 0c0h, 0c0h, 0c0h, 0c0h, 0c0h, 0c0h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 0feh, 06ch, 06ch, 06ch, 06ch, 06ch, 06ch, 06ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 0feh, 0c6h, 060h, 030h, 018h, 030h, 060h, 0c6h, 0feh, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 07eh, 0d8h, 0d8h, 0d8h, 0d8h, 0d8h, 070h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 066h, 066h, 066h, 066h, 066h, 07ch, 060h, 060h, 0c0h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 076h, 0dch, 018h, 018h, 018h, 018h, 018h, 018h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 07eh, 018h, 03ch, 066h, 066h, 066h, 03ch, 018h, 07eh, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 038h, 06ch, 0c6h, 0c6h, 0feh, 0c6h, 0c6h, 06ch, 038h, 000h, 000h, 000h, 000h
    db  000h, 000h, 038h, 06ch, 0c6h, 0c6h, 0c6h, 06ch, 06ch, 06ch, 06ch, 0eeh, 000h, 000h, 000h, 000h
    db  000h, 000h, 01eh, 030h, 018h, 00ch, 03eh, 066h, 066h, 066h, 066h, 03ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 07eh, 0dbh, 0dbh, 0dbh, 07eh, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 003h, 006h, 07eh, 0dbh, 0dbh, 0f3h, 07eh, 060h, 0c0h, 000h, 000h, 000h, 000h
    db  000h, 000h, 01ch, 030h, 060h, 060h, 07ch, 060h, 060h, 060h, 030h, 01ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 07ch, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 0feh, 000h, 000h, 0feh, 000h, 000h, 0feh, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 018h, 018h, 07eh, 018h, 018h, 000h, 000h, 0ffh, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 030h, 018h, 00ch, 006h, 00ch, 018h, 030h, 000h, 07eh, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 00ch, 018h, 030h, 060h, 030h, 018h, 00ch, 000h, 07eh, 000h, 000h, 000h, 000h
    db  000h, 000h, 00eh, 01bh, 01bh, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h
    db  018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 0d8h, 0d8h, 0d8h, 070h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 018h, 018h, 000h, 07eh, 000h, 018h, 018h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 076h, 0dch, 000h, 076h, 0dch, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 038h, 06ch, 06ch, 038h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 018h, 018h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 018h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 00fh, 00ch, 00ch, 00ch, 00ch, 00ch, 0ech, 06ch, 06ch, 03ch, 01ch, 000h, 000h, 000h, 000h
    db  000h, 0d8h, 06ch, 06ch, 06ch, 06ch, 06ch, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 070h, 0d8h, 030h, 060h, 0c8h, 0f8h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 07ch, 07ch, 07ch, 07ch, 07ch, 07ch, 07ch, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
_vgafont14alt:                               ; 0xc7baf LB 0x12d
    db  01dh, 000h, 000h, 000h, 000h, 024h, 066h, 0ffh, 066h, 024h, 000h, 000h, 000h, 000h, 000h, 022h
    db  000h, 063h, 063h, 063h, 022h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 02bh, 000h
    db  000h, 000h, 018h, 018h, 018h, 0ffh, 018h, 018h, 018h, 000h, 000h, 000h, 000h, 02dh, 000h, 000h
    db  000h, 000h, 000h, 000h, 0ffh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 04dh, 000h, 000h, 0c3h
    db  0e7h, 0ffh, 0dbh, 0c3h, 0c3h, 0c3h, 0c3h, 0c3h, 000h, 000h, 000h, 054h, 000h, 000h, 0ffh, 0dbh
    db  099h, 018h, 018h, 018h, 018h, 018h, 03ch, 000h, 000h, 000h, 056h, 000h, 000h, 0c3h, 0c3h, 0c3h
    db  0c3h, 0c3h, 0c3h, 066h, 03ch, 018h, 000h, 000h, 000h, 057h, 000h, 000h, 0c3h, 0c3h, 0c3h, 0c3h
    db  0dbh, 0dbh, 0ffh, 066h, 066h, 000h, 000h, 000h, 058h, 000h, 000h, 0c3h, 0c3h, 066h, 03ch, 018h
    db  03ch, 066h, 0c3h, 0c3h, 000h, 000h, 000h, 059h, 000h, 000h, 0c3h, 0c3h, 0c3h, 066h, 03ch, 018h
    db  018h, 018h, 03ch, 000h, 000h, 000h, 05ah, 000h, 000h, 0ffh, 0c3h, 086h, 00ch, 018h, 030h, 061h
    db  0c3h, 0ffh, 000h, 000h, 000h, 06dh, 000h, 000h, 000h, 000h, 000h, 0e6h, 0ffh, 0dbh, 0dbh, 0dbh
    db  0dbh, 000h, 000h, 000h, 076h, 000h, 000h, 000h, 000h, 000h, 0c3h, 0c3h, 0c3h, 066h, 03ch, 018h
    db  000h, 000h, 000h, 077h, 000h, 000h, 000h, 000h, 000h, 0c3h, 0c3h, 0dbh, 0dbh, 0ffh, 066h, 000h
    db  000h, 000h, 091h, 000h, 000h, 000h, 000h, 06eh, 03bh, 01bh, 07eh, 0d8h, 0dch, 077h, 000h, 000h
    db  000h, 09bh, 000h, 018h, 018h, 07eh, 0c3h, 0c0h, 0c0h, 0c3h, 07eh, 018h, 018h, 000h, 000h, 000h
    db  09dh, 000h, 000h, 0c3h, 066h, 03ch, 018h, 0ffh, 018h, 0ffh, 018h, 018h, 000h, 000h, 000h, 09eh
    db  000h, 0fch, 066h, 066h, 07ch, 062h, 066h, 06fh, 066h, 066h, 0f3h, 000h, 000h, 000h, 0f1h, 000h
    db  000h, 018h, 018h, 018h, 0ffh, 018h, 018h, 018h, 000h, 0ffh, 000h, 000h, 000h, 0f6h, 000h, 000h
    db  018h, 018h, 000h, 000h, 0ffh, 000h, 000h, 018h, 018h, 000h, 000h, 000h, 000h
_vgafont16alt:                               ; 0xc7cdc LB 0x144
    db  01dh, 000h, 000h, 000h, 000h, 000h, 024h, 066h, 0ffh, 066h, 024h, 000h, 000h, 000h, 000h, 000h
    db  000h, 030h, 000h, 000h, 03ch, 066h, 0c3h, 0c3h, 0dbh, 0dbh, 0c3h, 0c3h, 066h, 03ch, 000h, 000h
    db  000h, 000h, 04dh, 000h, 000h, 0c3h, 0e7h, 0ffh, 0ffh, 0dbh, 0c3h, 0c3h, 0c3h, 0c3h, 0c3h, 000h
    db  000h, 000h, 000h, 054h, 000h, 000h, 0ffh, 0dbh, 099h, 018h, 018h, 018h, 018h, 018h, 018h, 03ch
    db  000h, 000h, 000h, 000h, 056h, 000h, 000h, 0c3h, 0c3h, 0c3h, 0c3h, 0c3h, 0c3h, 0c3h, 066h, 03ch
    db  018h, 000h, 000h, 000h, 000h, 057h, 000h, 000h, 0c3h, 0c3h, 0c3h, 0c3h, 0c3h, 0dbh, 0dbh, 0ffh
    db  066h, 066h, 000h, 000h, 000h, 000h, 058h, 000h, 000h, 0c3h, 0c3h, 066h, 03ch, 018h, 018h, 03ch
    db  066h, 0c3h, 0c3h, 000h, 000h, 000h, 000h, 059h, 000h, 000h, 0c3h, 0c3h, 0c3h, 066h, 03ch, 018h
    db  018h, 018h, 018h, 03ch, 000h, 000h, 000h, 000h, 05ah, 000h, 000h, 0ffh, 0c3h, 086h, 00ch, 018h
    db  030h, 060h, 0c1h, 0c3h, 0ffh, 000h, 000h, 000h, 000h, 06dh, 000h, 000h, 000h, 000h, 000h, 0e6h
    db  0ffh, 0dbh, 0dbh, 0dbh, 0dbh, 0dbh, 000h, 000h, 000h, 000h, 076h, 000h, 000h, 000h, 000h, 000h
    db  0c3h, 0c3h, 0c3h, 0c3h, 066h, 03ch, 018h, 000h, 000h, 000h, 000h, 077h, 000h, 000h, 000h, 000h
    db  000h, 0c3h, 0c3h, 0c3h, 0dbh, 0dbh, 0ffh, 066h, 000h, 000h, 000h, 000h, 078h, 000h, 000h, 000h
    db  000h, 000h, 0c3h, 066h, 03ch, 018h, 03ch, 066h, 0c3h, 000h, 000h, 000h, 000h, 091h, 000h, 000h
    db  000h, 000h, 000h, 06eh, 03bh, 01bh, 07eh, 0d8h, 0dch, 077h, 000h, 000h, 000h, 000h, 09bh, 000h
    db  018h, 018h, 07eh, 0c3h, 0c0h, 0c0h, 0c0h, 0c3h, 07eh, 018h, 018h, 000h, 000h, 000h, 000h, 09dh
    db  000h, 000h, 0c3h, 066h, 03ch, 018h, 0ffh, 018h, 0ffh, 018h, 018h, 018h, 000h, 000h, 000h, 000h
    db  09eh, 000h, 0fch, 066h, 066h, 07ch, 062h, 066h, 06fh, 066h, 066h, 066h, 0f3h, 000h, 000h, 000h
    db  000h, 0abh, 000h, 0c0h, 0c0h, 0c2h, 0c6h, 0cch, 018h, 030h, 060h, 0ceh, 09bh, 006h, 00ch, 01fh
    db  000h, 000h, 0ach, 000h, 0c0h, 0c0h, 0c2h, 0c6h, 0cch, 018h, 030h, 066h, 0ceh, 096h, 03eh, 006h
    db  006h, 000h, 000h, 000h
_vbebios_copyright:                          ; 0xc7e20 LB 0x15
    db  'VirtualBox VESA BIOS', 000h
_vbebios_vendor_name:                        ; 0xc7e35 LB 0x13
    db  'Oracle Corporation', 000h
_vbebios_product_name:                       ; 0xc7e48 LB 0x21
    db  'Oracle VM VirtualBox VBE Adapter', 000h
_vbebios_product_revision:                   ; 0xc7e69 LB 0x24
    db  'Oracle VM VirtualBox Version 4.1.51', 000h
_vbebios_info_string:                        ; 0xc7e8d LB 0x2b
    db  'VirtualBox VBE Display Adapter enabled', 00dh, 00ah, 00dh, 00ah, 000h
_no_vbebios_info_string:                     ; 0xc7eb8 LB 0x29
    db  'No VirtualBox VBE support available!', 00dh, 00ah, 00dh, 00ah, 000h

  ; Padding 0x1 bytes at 0xc7ee1
  times 1 db 0

section CONST progbits vstart=0x7ee2 align=1 ; size=0x2e class=DATA group=DGROUP
    db   'Signature NOT found! %x', 00ah, 000h
    db   'Signature NOT found', 00ah, 000h

section CONST2 progbits vstart=0x7f10 align=1 ; size=0x0 class=DATA group=DGROUP

  ; Padding 0xf0 bytes at 0xc7f10
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 041h
