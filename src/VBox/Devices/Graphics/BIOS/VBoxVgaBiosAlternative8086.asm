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

;
; Oracle LGPL Disclaimer: For the avoidance of doubt, except that if any license choice
; other than GPL or LGPL is available it will apply instead, Oracle elects to use only
; the Lesser General Public License version 2.1 (LGPLv2) at this time for any software where
; a choice of LGPL license versions is made available with the language indicating
; that LGPLv2 or any later version may be used, or where a choice of which version
; of the LGPL is applied is otherwise unspecified.
;





section VGAROM progbits vstart=0x0 align=1 ; size=0x942 class=CODE group=AUTO
  ; disGetNextSymbol 0xc0000 LB 0x942 -> off=0x22 cb=000000000000056e uValue=00000000000c0022 'vgabios_int10_handler'
    db  055h, 0aah, 040h, 0e9h, 0e4h, 009h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 049h, 042h
    db  04dh, 000h
vgabios_int10_handler:                       ; 0xc0022 LB 0x56e
    pushfw                                    ; 9c                          ; 0xc0022 vgarom.asm:84
    cmp ah, 00fh                              ; 80 fc 0f                    ; 0xc0023 vgarom.asm:96
    jne short 0002eh                          ; 75 06                       ; 0xc0026 vgarom.asm:97
    call 00189h                               ; e8 5e 01                    ; 0xc0028 vgarom.asm:98
    jmp near 000f9h                           ; e9 cb 00                    ; 0xc002b vgarom.asm:99
    cmp ah, 01ah                              ; 80 fc 1a                    ; 0xc002e vgarom.asm:101
    jne short 00039h                          ; 75 06                       ; 0xc0031 vgarom.asm:102
    call 0055ch                               ; e8 26 05                    ; 0xc0033 vgarom.asm:103
    jmp near 000f9h                           ; e9 c0 00                    ; 0xc0036 vgarom.asm:104
    cmp ah, 00bh                              ; 80 fc 0b                    ; 0xc0039 vgarom.asm:106
    jne short 00044h                          ; 75 06                       ; 0xc003c vgarom.asm:107
    call 000fbh                               ; e8 ba 00                    ; 0xc003e vgarom.asm:108
    jmp near 000f9h                           ; e9 b5 00                    ; 0xc0041 vgarom.asm:109
    cmp ax, 01103h                            ; 3d 03 11                    ; 0xc0044 vgarom.asm:111
    jne short 0004fh                          ; 75 06                       ; 0xc0047 vgarom.asm:112
    call 00450h                               ; e8 04 04                    ; 0xc0049 vgarom.asm:113
    jmp near 000f9h                           ; e9 aa 00                    ; 0xc004c vgarom.asm:114
    cmp ah, 012h                              ; 80 fc 12                    ; 0xc004f vgarom.asm:116
    jne short 00093h                          ; 75 3f                       ; 0xc0052 vgarom.asm:117
    cmp bl, 010h                              ; 80 fb 10                    ; 0xc0054 vgarom.asm:118
    jne short 0005fh                          ; 75 06                       ; 0xc0057 vgarom.asm:119
    call 0045dh                               ; e8 01 04                    ; 0xc0059 vgarom.asm:120
    jmp near 000f9h                           ; e9 9a 00                    ; 0xc005c vgarom.asm:121
    cmp bl, 030h                              ; 80 fb 30                    ; 0xc005f vgarom.asm:123
    jne short 0006ah                          ; 75 06                       ; 0xc0062 vgarom.asm:124
    call 00480h                               ; e8 19 04                    ; 0xc0064 vgarom.asm:125
    jmp near 000f9h                           ; e9 8f 00                    ; 0xc0067 vgarom.asm:126
    cmp bl, 031h                              ; 80 fb 31                    ; 0xc006a vgarom.asm:128
    jne short 00075h                          ; 75 06                       ; 0xc006d vgarom.asm:129
    call 004d3h                               ; e8 61 04                    ; 0xc006f vgarom.asm:130
    jmp near 000f9h                           ; e9 84 00                    ; 0xc0072 vgarom.asm:131
    cmp bl, 032h                              ; 80 fb 32                    ; 0xc0075 vgarom.asm:133
    jne short 0007fh                          ; 75 05                       ; 0xc0078 vgarom.asm:134
    call 004f8h                               ; e8 7b 04                    ; 0xc007a vgarom.asm:135
    jmp short 000f9h                          ; eb 7a                       ; 0xc007d vgarom.asm:136
    cmp bl, 033h                              ; 80 fb 33                    ; 0xc007f vgarom.asm:138
    jne short 00089h                          ; 75 05                       ; 0xc0082 vgarom.asm:139
    call 00516h                               ; e8 8f 04                    ; 0xc0084 vgarom.asm:140
    jmp short 000f9h                          ; eb 70                       ; 0xc0087 vgarom.asm:141
    cmp bl, 034h                              ; 80 fb 34                    ; 0xc0089 vgarom.asm:143
    jne short 000ddh                          ; 75 4f                       ; 0xc008c vgarom.asm:144
    call 0053ah                               ; e8 a9 04                    ; 0xc008e vgarom.asm:145
    jmp short 000f9h                          ; eb 66                       ; 0xc0091 vgarom.asm:146
    cmp ax, 0101bh                            ; 3d 1b 10                    ; 0xc0093 vgarom.asm:148
    je short 000ddh                           ; 74 45                       ; 0xc0096 vgarom.asm:149
    cmp ah, 010h                              ; 80 fc 10                    ; 0xc0098 vgarom.asm:150
    jne short 000a2h                          ; 75 05                       ; 0xc009b vgarom.asm:154
    call 001b0h                               ; e8 10 01                    ; 0xc009d vgarom.asm:156
    jmp short 000f9h                          ; eb 57                       ; 0xc00a0 vgarom.asm:157
    cmp ah, 04fh                              ; 80 fc 4f                    ; 0xc00a2 vgarom.asm:160
    jne short 000ddh                          ; 75 36                       ; 0xc00a5 vgarom.asm:161
    cmp AL, strict byte 003h                  ; 3c 03                       ; 0xc00a7 vgarom.asm:162
    jne short 000b0h                          ; 75 05                       ; 0xc00a9 vgarom.asm:163
    call 007fbh                               ; e8 4d 07                    ; 0xc00ab vgarom.asm:164
    jmp short 000f9h                          ; eb 49                       ; 0xc00ae vgarom.asm:165
    cmp AL, strict byte 005h                  ; 3c 05                       ; 0xc00b0 vgarom.asm:167
    jne short 000b9h                          ; 75 05                       ; 0xc00b2 vgarom.asm:168
    call 00820h                               ; e8 69 07                    ; 0xc00b4 vgarom.asm:169
    jmp short 000f9h                          ; eb 40                       ; 0xc00b7 vgarom.asm:170
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc00b9 vgarom.asm:172
    jne short 000c2h                          ; 75 05                       ; 0xc00bb vgarom.asm:173
    call 0084dh                               ; e8 8d 07                    ; 0xc00bd vgarom.asm:174
    jmp short 000f9h                          ; eb 37                       ; 0xc00c0 vgarom.asm:175
    cmp AL, strict byte 008h                  ; 3c 08                       ; 0xc00c2 vgarom.asm:177
    jne short 000cbh                          ; 75 05                       ; 0xc00c4 vgarom.asm:178
    call 00881h                               ; e8 b8 07                    ; 0xc00c6 vgarom.asm:179
    jmp short 000f9h                          ; eb 2e                       ; 0xc00c9 vgarom.asm:180
    cmp AL, strict byte 009h                  ; 3c 09                       ; 0xc00cb vgarom.asm:182
    jne short 000d4h                          ; 75 05                       ; 0xc00cd vgarom.asm:183
    call 008b8h                               ; e8 e6 07                    ; 0xc00cf vgarom.asm:184
    jmp short 000f9h                          ; eb 25                       ; 0xc00d2 vgarom.asm:185
    cmp AL, strict byte 00ah                  ; 3c 0a                       ; 0xc00d4 vgarom.asm:187
    jne short 000ddh                          ; 75 05                       ; 0xc00d6 vgarom.asm:188
    call 0092bh                               ; e8 50 08                    ; 0xc00d8 vgarom.asm:189
    jmp short 000f9h                          ; eb 1c                       ; 0xc00db vgarom.asm:190
    push ES                                   ; 06                          ; 0xc00dd vgarom.asm:194
    push DS                                   ; 1e                          ; 0xc00de vgarom.asm:195
    push ax                                   ; 50                          ; 0xc00df vgarom.asm:99
    push cx                                   ; 51                          ; 0xc00e0 vgarom.asm:100
    push dx                                   ; 52                          ; 0xc00e1 vgarom.asm:101
    push bx                                   ; 53                          ; 0xc00e2 vgarom.asm:102
    push sp                                   ; 54                          ; 0xc00e3 vgarom.asm:103
    push bp                                   ; 55                          ; 0xc00e4 vgarom.asm:104
    push si                                   ; 56                          ; 0xc00e5 vgarom.asm:105
    push di                                   ; 57                          ; 0xc00e6 vgarom.asm:106
    mov bx, 0c000h                            ; bb 00 c0                    ; 0xc00e7 vgarom.asm:199
    mov ds, bx                                ; 8e db                       ; 0xc00ea vgarom.asm:200
    call 03711h                               ; e8 22 36                    ; 0xc00ec vgarom.asm:201
    pop di                                    ; 5f                          ; 0xc00ef vgarom.asm:116
    pop si                                    ; 5e                          ; 0xc00f0 vgarom.asm:117
    pop bp                                    ; 5d                          ; 0xc00f1 vgarom.asm:118
    pop bx                                    ; 5b                          ; 0xc00f2 vgarom.asm:119
    pop bx                                    ; 5b                          ; 0xc00f3 vgarom.asm:120
    pop dx                                    ; 5a                          ; 0xc00f4 vgarom.asm:121
    pop cx                                    ; 59                          ; 0xc00f5 vgarom.asm:122
    pop ax                                    ; 58                          ; 0xc00f6 vgarom.asm:123
    pop DS                                    ; 1f                          ; 0xc00f7 vgarom.asm:204
    pop ES                                    ; 07                          ; 0xc00f8 vgarom.asm:205
    popfw                                     ; 9d                          ; 0xc00f9 vgarom.asm:207
    iret                                      ; cf                          ; 0xc00fa vgarom.asm:208
    cmp bh, 000h                              ; 80 ff 00                    ; 0xc00fb vgarom.asm:213
    je short 00106h                           ; 74 06                       ; 0xc00fe vgarom.asm:214
    cmp bh, 001h                              ; 80 ff 01                    ; 0xc0100 vgarom.asm:215
    je short 00157h                           ; 74 52                       ; 0xc0103 vgarom.asm:216
    retn                                      ; c3                          ; 0xc0105 vgarom.asm:220
    push ax                                   ; 50                          ; 0xc0106 vgarom.asm:222
    push bx                                   ; 53                          ; 0xc0107 vgarom.asm:223
    push cx                                   ; 51                          ; 0xc0108 vgarom.asm:224
    push dx                                   ; 52                          ; 0xc0109 vgarom.asm:225
    push DS                                   ; 1e                          ; 0xc010a vgarom.asm:226
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc010b vgarom.asm:227
    mov ds, dx                                ; 8e da                       ; 0xc010e vgarom.asm:228
    mov dx, 003dah                            ; ba da 03                    ; 0xc0110 vgarom.asm:229
    in AL, DX                                 ; ec                          ; 0xc0113 vgarom.asm:230
    cmp byte [word 00049h], 003h              ; 80 3e 49 00 03              ; 0xc0114 vgarom.asm:231
    jbe short 0014ah                          ; 76 2f                       ; 0xc0119 vgarom.asm:232
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc011b vgarom.asm:233
    mov AL, strict byte 000h                  ; b0 00                       ; 0xc011e vgarom.asm:234
    out DX, AL                                ; ee                          ; 0xc0120 vgarom.asm:235
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc0121 vgarom.asm:236
    and AL, strict byte 00fh                  ; 24 0f                       ; 0xc0123 vgarom.asm:237
    test AL, strict byte 008h                 ; a8 08                       ; 0xc0125 vgarom.asm:238
    je short 0012bh                           ; 74 02                       ; 0xc0127 vgarom.asm:239
    add AL, strict byte 008h                  ; 04 08                       ; 0xc0129 vgarom.asm:240
    out DX, AL                                ; ee                          ; 0xc012b vgarom.asm:242
    mov CL, strict byte 001h                  ; b1 01                       ; 0xc012c vgarom.asm:243
    and bl, 010h                              ; 80 e3 10                    ; 0xc012e vgarom.asm:244
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0131 vgarom.asm:246
    db  08ah, 0c1h
    ; mov al, cl                                ; 8a c1                     ; 0xc0134 vgarom.asm:247
    out DX, AL                                ; ee                          ; 0xc0136 vgarom.asm:248
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc0137 vgarom.asm:249
    in AL, DX                                 ; ec                          ; 0xc013a vgarom.asm:250
    and AL, strict byte 0efh                  ; 24 ef                       ; 0xc013b vgarom.asm:251
    db  00ah, 0c3h
    ; or al, bl                                 ; 0a c3                     ; 0xc013d vgarom.asm:252
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc013f vgarom.asm:253
    out DX, AL                                ; ee                          ; 0xc0142 vgarom.asm:254
    db  0feh, 0c1h
    ; inc cl                                    ; fe c1                     ; 0xc0143 vgarom.asm:255
    cmp cl, 004h                              ; 80 f9 04                    ; 0xc0145 vgarom.asm:256
    jne short 00131h                          ; 75 e7                       ; 0xc0148 vgarom.asm:257
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc014a vgarom.asm:259
    out DX, AL                                ; ee                          ; 0xc014c vgarom.asm:260
    mov dx, 003dah                            ; ba da 03                    ; 0xc014d vgarom.asm:262
    in AL, DX                                 ; ec                          ; 0xc0150 vgarom.asm:263
    pop DS                                    ; 1f                          ; 0xc0151 vgarom.asm:265
    pop dx                                    ; 5a                          ; 0xc0152 vgarom.asm:266
    pop cx                                    ; 59                          ; 0xc0153 vgarom.asm:267
    pop bx                                    ; 5b                          ; 0xc0154 vgarom.asm:268
    pop ax                                    ; 58                          ; 0xc0155 vgarom.asm:269
    retn                                      ; c3                          ; 0xc0156 vgarom.asm:270
    push ax                                   ; 50                          ; 0xc0157 vgarom.asm:272
    push bx                                   ; 53                          ; 0xc0158 vgarom.asm:273
    push cx                                   ; 51                          ; 0xc0159 vgarom.asm:274
    push dx                                   ; 52                          ; 0xc015a vgarom.asm:275
    mov dx, 003dah                            ; ba da 03                    ; 0xc015b vgarom.asm:276
    in AL, DX                                 ; ec                          ; 0xc015e vgarom.asm:277
    mov CL, strict byte 001h                  ; b1 01                       ; 0xc015f vgarom.asm:278
    and bl, 001h                              ; 80 e3 01                    ; 0xc0161 vgarom.asm:279
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0164 vgarom.asm:281
    db  08ah, 0c1h
    ; mov al, cl                                ; 8a c1                     ; 0xc0167 vgarom.asm:282
    out DX, AL                                ; ee                          ; 0xc0169 vgarom.asm:283
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc016a vgarom.asm:284
    in AL, DX                                 ; ec                          ; 0xc016d vgarom.asm:285
    and AL, strict byte 0feh                  ; 24 fe                       ; 0xc016e vgarom.asm:286
    db  00ah, 0c3h
    ; or al, bl                                 ; 0a c3                     ; 0xc0170 vgarom.asm:287
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0172 vgarom.asm:288
    out DX, AL                                ; ee                          ; 0xc0175 vgarom.asm:289
    db  0feh, 0c1h
    ; inc cl                                    ; fe c1                     ; 0xc0176 vgarom.asm:290
    cmp cl, 004h                              ; 80 f9 04                    ; 0xc0178 vgarom.asm:291
    jne short 00164h                          ; 75 e7                       ; 0xc017b vgarom.asm:292
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc017d vgarom.asm:293
    out DX, AL                                ; ee                          ; 0xc017f vgarom.asm:294
    mov dx, 003dah                            ; ba da 03                    ; 0xc0180 vgarom.asm:296
    in AL, DX                                 ; ec                          ; 0xc0183 vgarom.asm:297
    pop dx                                    ; 5a                          ; 0xc0184 vgarom.asm:299
    pop cx                                    ; 59                          ; 0xc0185 vgarom.asm:300
    pop bx                                    ; 5b                          ; 0xc0186 vgarom.asm:301
    pop ax                                    ; 58                          ; 0xc0187 vgarom.asm:302
    retn                                      ; c3                          ; 0xc0188 vgarom.asm:303
    push DS                                   ; 1e                          ; 0xc0189 vgarom.asm:308
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc018a vgarom.asm:309
    mov ds, ax                                ; 8e d8                       ; 0xc018d vgarom.asm:310
    push bx                                   ; 53                          ; 0xc018f vgarom.asm:311
    mov bx, strict word 00062h                ; bb 62 00                    ; 0xc0190 vgarom.asm:312
    mov al, byte [bx]                         ; 8a 07                       ; 0xc0193 vgarom.asm:313
    pop bx                                    ; 5b                          ; 0xc0195 vgarom.asm:314
    db  08ah, 0f8h
    ; mov bh, al                                ; 8a f8                     ; 0xc0196 vgarom.asm:315
    push bx                                   ; 53                          ; 0xc0198 vgarom.asm:316
    mov bx, 00087h                            ; bb 87 00                    ; 0xc0199 vgarom.asm:317
    mov ah, byte [bx]                         ; 8a 27                       ; 0xc019c vgarom.asm:318
    and ah, 080h                              ; 80 e4 80                    ; 0xc019e vgarom.asm:319
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc01a1 vgarom.asm:320
    mov al, byte [bx]                         ; 8a 07                       ; 0xc01a4 vgarom.asm:321
    db  00ah, 0c4h
    ; or al, ah                                 ; 0a c4                     ; 0xc01a6 vgarom.asm:322
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc01a8 vgarom.asm:323
    mov ah, byte [bx]                         ; 8a 27                       ; 0xc01ab vgarom.asm:324
    pop bx                                    ; 5b                          ; 0xc01ad vgarom.asm:325
    pop DS                                    ; 1f                          ; 0xc01ae vgarom.asm:326
    retn                                      ; c3                          ; 0xc01af vgarom.asm:327
    cmp AL, strict byte 000h                  ; 3c 00                       ; 0xc01b0 vgarom.asm:332
    jne short 001b6h                          ; 75 02                       ; 0xc01b2 vgarom.asm:333
    jmp short 00217h                          ; eb 61                       ; 0xc01b4 vgarom.asm:334
    cmp AL, strict byte 001h                  ; 3c 01                       ; 0xc01b6 vgarom.asm:336
    jne short 001bch                          ; 75 02                       ; 0xc01b8 vgarom.asm:337
    jmp short 00235h                          ; eb 79                       ; 0xc01ba vgarom.asm:338
    cmp AL, strict byte 002h                  ; 3c 02                       ; 0xc01bc vgarom.asm:340
    jne short 001c2h                          ; 75 02                       ; 0xc01be vgarom.asm:341
    jmp short 0023dh                          ; eb 7b                       ; 0xc01c0 vgarom.asm:342
    cmp AL, strict byte 003h                  ; 3c 03                       ; 0xc01c2 vgarom.asm:344
    jne short 001c9h                          ; 75 03                       ; 0xc01c4 vgarom.asm:345
    jmp near 0026eh                           ; e9 a5 00                    ; 0xc01c6 vgarom.asm:346
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc01c9 vgarom.asm:348
    jne short 001d0h                          ; 75 03                       ; 0xc01cb vgarom.asm:349
    jmp near 0029bh                           ; e9 cb 00                    ; 0xc01cd vgarom.asm:350
    cmp AL, strict byte 008h                  ; 3c 08                       ; 0xc01d0 vgarom.asm:352
    jne short 001d7h                          ; 75 03                       ; 0xc01d2 vgarom.asm:353
    jmp near 002c3h                           ; e9 ec 00                    ; 0xc01d4 vgarom.asm:354
    cmp AL, strict byte 009h                  ; 3c 09                       ; 0xc01d7 vgarom.asm:356
    jne short 001deh                          ; 75 03                       ; 0xc01d9 vgarom.asm:357
    jmp near 002d1h                           ; e9 f3 00                    ; 0xc01db vgarom.asm:358
    cmp AL, strict byte 010h                  ; 3c 10                       ; 0xc01de vgarom.asm:360
    jne short 001e5h                          ; 75 03                       ; 0xc01e0 vgarom.asm:361
    jmp near 00316h                           ; e9 31 01                    ; 0xc01e2 vgarom.asm:362
    cmp AL, strict byte 012h                  ; 3c 12                       ; 0xc01e5 vgarom.asm:364
    jne short 001ech                          ; 75 03                       ; 0xc01e7 vgarom.asm:365
    jmp near 0032fh                           ; e9 43 01                    ; 0xc01e9 vgarom.asm:366
    cmp AL, strict byte 013h                  ; 3c 13                       ; 0xc01ec vgarom.asm:368
    jne short 001f3h                          ; 75 03                       ; 0xc01ee vgarom.asm:369
    jmp near 00357h                           ; e9 64 01                    ; 0xc01f0 vgarom.asm:370
    cmp AL, strict byte 015h                  ; 3c 15                       ; 0xc01f3 vgarom.asm:372
    jne short 001fah                          ; 75 03                       ; 0xc01f5 vgarom.asm:373
    jmp near 003aah                           ; e9 b0 01                    ; 0xc01f7 vgarom.asm:374
    cmp AL, strict byte 017h                  ; 3c 17                       ; 0xc01fa vgarom.asm:376
    jne short 00201h                          ; 75 03                       ; 0xc01fc vgarom.asm:377
    jmp near 003c5h                           ; e9 c4 01                    ; 0xc01fe vgarom.asm:378
    cmp AL, strict byte 018h                  ; 3c 18                       ; 0xc0201 vgarom.asm:380
    jne short 00208h                          ; 75 03                       ; 0xc0203 vgarom.asm:381
    jmp near 003edh                           ; e9 e5 01                    ; 0xc0205 vgarom.asm:382
    cmp AL, strict byte 019h                  ; 3c 19                       ; 0xc0208 vgarom.asm:384
    jne short 0020fh                          ; 75 03                       ; 0xc020a vgarom.asm:385
    jmp near 003f8h                           ; e9 e9 01                    ; 0xc020c vgarom.asm:386
    cmp AL, strict byte 01ah                  ; 3c 1a                       ; 0xc020f vgarom.asm:388
    jne short 00216h                          ; 75 03                       ; 0xc0211 vgarom.asm:389
    jmp near 00403h                           ; e9 ed 01                    ; 0xc0213 vgarom.asm:390
    retn                                      ; c3                          ; 0xc0216 vgarom.asm:395
    cmp bl, 014h                              ; 80 fb 14                    ; 0xc0217 vgarom.asm:398
    jnbe short 00234h                         ; 77 18                       ; 0xc021a vgarom.asm:399
    push ax                                   ; 50                          ; 0xc021c vgarom.asm:400
    push dx                                   ; 52                          ; 0xc021d vgarom.asm:401
    mov dx, 003dah                            ; ba da 03                    ; 0xc021e vgarom.asm:402
    in AL, DX                                 ; ec                          ; 0xc0221 vgarom.asm:403
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0222 vgarom.asm:404
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc0225 vgarom.asm:405
    out DX, AL                                ; ee                          ; 0xc0227 vgarom.asm:406
    db  08ah, 0c7h
    ; mov al, bh                                ; 8a c7                     ; 0xc0228 vgarom.asm:407
    out DX, AL                                ; ee                          ; 0xc022a vgarom.asm:408
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc022b vgarom.asm:409
    out DX, AL                                ; ee                          ; 0xc022d vgarom.asm:410
    mov dx, 003dah                            ; ba da 03                    ; 0xc022e vgarom.asm:412
    in AL, DX                                 ; ec                          ; 0xc0231 vgarom.asm:413
    pop dx                                    ; 5a                          ; 0xc0232 vgarom.asm:415
    pop ax                                    ; 58                          ; 0xc0233 vgarom.asm:416
    retn                                      ; c3                          ; 0xc0234 vgarom.asm:418
    push bx                                   ; 53                          ; 0xc0235 vgarom.asm:423
    mov BL, strict byte 011h                  ; b3 11                       ; 0xc0236 vgarom.asm:424
    call 00217h                               ; e8 dc ff                    ; 0xc0238 vgarom.asm:425
    pop bx                                    ; 5b                          ; 0xc023b vgarom.asm:426
    retn                                      ; c3                          ; 0xc023c vgarom.asm:427
    push ax                                   ; 50                          ; 0xc023d vgarom.asm:432
    push bx                                   ; 53                          ; 0xc023e vgarom.asm:433
    push cx                                   ; 51                          ; 0xc023f vgarom.asm:434
    push dx                                   ; 52                          ; 0xc0240 vgarom.asm:435
    db  08bh, 0dah
    ; mov bx, dx                                ; 8b da                     ; 0xc0241 vgarom.asm:436
    mov dx, 003dah                            ; ba da 03                    ; 0xc0243 vgarom.asm:437
    in AL, DX                                 ; ec                          ; 0xc0246 vgarom.asm:438
    mov CL, strict byte 000h                  ; b1 00                       ; 0xc0247 vgarom.asm:439
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0249 vgarom.asm:440
    db  08ah, 0c1h
    ; mov al, cl                                ; 8a c1                     ; 0xc024c vgarom.asm:442
    out DX, AL                                ; ee                          ; 0xc024e vgarom.asm:443
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc024f vgarom.asm:444
    out DX, AL                                ; ee                          ; 0xc0252 vgarom.asm:445
    inc bx                                    ; 43                          ; 0xc0253 vgarom.asm:446
    db  0feh, 0c1h
    ; inc cl                                    ; fe c1                     ; 0xc0254 vgarom.asm:447
    cmp cl, 010h                              ; 80 f9 10                    ; 0xc0256 vgarom.asm:448
    jne short 0024ch                          ; 75 f1                       ; 0xc0259 vgarom.asm:449
    mov AL, strict byte 011h                  ; b0 11                       ; 0xc025b vgarom.asm:450
    out DX, AL                                ; ee                          ; 0xc025d vgarom.asm:451
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc025e vgarom.asm:452
    out DX, AL                                ; ee                          ; 0xc0261 vgarom.asm:453
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc0262 vgarom.asm:454
    out DX, AL                                ; ee                          ; 0xc0264 vgarom.asm:455
    mov dx, 003dah                            ; ba da 03                    ; 0xc0265 vgarom.asm:457
    in AL, DX                                 ; ec                          ; 0xc0268 vgarom.asm:458
    pop dx                                    ; 5a                          ; 0xc0269 vgarom.asm:460
    pop cx                                    ; 59                          ; 0xc026a vgarom.asm:461
    pop bx                                    ; 5b                          ; 0xc026b vgarom.asm:462
    pop ax                                    ; 58                          ; 0xc026c vgarom.asm:463
    retn                                      ; c3                          ; 0xc026d vgarom.asm:464
    push ax                                   ; 50                          ; 0xc026e vgarom.asm:469
    push bx                                   ; 53                          ; 0xc026f vgarom.asm:470
    push dx                                   ; 52                          ; 0xc0270 vgarom.asm:471
    mov dx, 003dah                            ; ba da 03                    ; 0xc0271 vgarom.asm:472
    in AL, DX                                 ; ec                          ; 0xc0274 vgarom.asm:473
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0275 vgarom.asm:474
    mov AL, strict byte 010h                  ; b0 10                       ; 0xc0278 vgarom.asm:475
    out DX, AL                                ; ee                          ; 0xc027a vgarom.asm:476
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc027b vgarom.asm:477
    in AL, DX                                 ; ec                          ; 0xc027e vgarom.asm:478
    and AL, strict byte 0f7h                  ; 24 f7                       ; 0xc027f vgarom.asm:479
    and bl, 001h                              ; 80 e3 01                    ; 0xc0281 vgarom.asm:480
    sal bl, 1                                 ; d0 e3                       ; 0xc0284 vgarom.asm:484
    sal bl, 1                                 ; d0 e3                       ; 0xc0286 vgarom.asm:485
    sal bl, 1                                 ; d0 e3                       ; 0xc0288 vgarom.asm:486
    db  00ah, 0c3h
    ; or al, bl                                 ; 0a c3                     ; 0xc028a vgarom.asm:488
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc028c vgarom.asm:489
    out DX, AL                                ; ee                          ; 0xc028f vgarom.asm:490
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc0290 vgarom.asm:491
    out DX, AL                                ; ee                          ; 0xc0292 vgarom.asm:492
    mov dx, 003dah                            ; ba da 03                    ; 0xc0293 vgarom.asm:494
    in AL, DX                                 ; ec                          ; 0xc0296 vgarom.asm:495
    pop dx                                    ; 5a                          ; 0xc0297 vgarom.asm:497
    pop bx                                    ; 5b                          ; 0xc0298 vgarom.asm:498
    pop ax                                    ; 58                          ; 0xc0299 vgarom.asm:499
    retn                                      ; c3                          ; 0xc029a vgarom.asm:500
    cmp bl, 014h                              ; 80 fb 14                    ; 0xc029b vgarom.asm:505
    jnbe short 002c2h                         ; 77 22                       ; 0xc029e vgarom.asm:506
    push ax                                   ; 50                          ; 0xc02a0 vgarom.asm:507
    push dx                                   ; 52                          ; 0xc02a1 vgarom.asm:508
    mov dx, 003dah                            ; ba da 03                    ; 0xc02a2 vgarom.asm:509
    in AL, DX                                 ; ec                          ; 0xc02a5 vgarom.asm:510
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc02a6 vgarom.asm:511
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc02a9 vgarom.asm:512
    out DX, AL                                ; ee                          ; 0xc02ab vgarom.asm:513
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc02ac vgarom.asm:514
    in AL, DX                                 ; ec                          ; 0xc02af vgarom.asm:515
    db  08ah, 0f8h
    ; mov bh, al                                ; 8a f8                     ; 0xc02b0 vgarom.asm:516
    mov dx, 003dah                            ; ba da 03                    ; 0xc02b2 vgarom.asm:517
    in AL, DX                                 ; ec                          ; 0xc02b5 vgarom.asm:518
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc02b6 vgarom.asm:519
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc02b9 vgarom.asm:520
    out DX, AL                                ; ee                          ; 0xc02bb vgarom.asm:521
    mov dx, 003dah                            ; ba da 03                    ; 0xc02bc vgarom.asm:523
    in AL, DX                                 ; ec                          ; 0xc02bf vgarom.asm:524
    pop dx                                    ; 5a                          ; 0xc02c0 vgarom.asm:526
    pop ax                                    ; 58                          ; 0xc02c1 vgarom.asm:527
    retn                                      ; c3                          ; 0xc02c2 vgarom.asm:529
    push ax                                   ; 50                          ; 0xc02c3 vgarom.asm:534
    push bx                                   ; 53                          ; 0xc02c4 vgarom.asm:535
    mov BL, strict byte 011h                  ; b3 11                       ; 0xc02c5 vgarom.asm:536
    call 0029bh                               ; e8 d1 ff                    ; 0xc02c7 vgarom.asm:537
    db  08ah, 0c7h
    ; mov al, bh                                ; 8a c7                     ; 0xc02ca vgarom.asm:538
    pop bx                                    ; 5b                          ; 0xc02cc vgarom.asm:539
    db  08ah, 0f8h
    ; mov bh, al                                ; 8a f8                     ; 0xc02cd vgarom.asm:540
    pop ax                                    ; 58                          ; 0xc02cf vgarom.asm:541
    retn                                      ; c3                          ; 0xc02d0 vgarom.asm:542
    push ax                                   ; 50                          ; 0xc02d1 vgarom.asm:547
    push bx                                   ; 53                          ; 0xc02d2 vgarom.asm:548
    push cx                                   ; 51                          ; 0xc02d3 vgarom.asm:549
    push dx                                   ; 52                          ; 0xc02d4 vgarom.asm:550
    db  08bh, 0dah
    ; mov bx, dx                                ; 8b da                     ; 0xc02d5 vgarom.asm:551
    mov CL, strict byte 000h                  ; b1 00                       ; 0xc02d7 vgarom.asm:552
    mov dx, 003dah                            ; ba da 03                    ; 0xc02d9 vgarom.asm:554
    in AL, DX                                 ; ec                          ; 0xc02dc vgarom.asm:555
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc02dd vgarom.asm:556
    db  08ah, 0c1h
    ; mov al, cl                                ; 8a c1                     ; 0xc02e0 vgarom.asm:557
    out DX, AL                                ; ee                          ; 0xc02e2 vgarom.asm:558
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc02e3 vgarom.asm:559
    in AL, DX                                 ; ec                          ; 0xc02e6 vgarom.asm:560
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc02e7 vgarom.asm:561
    inc bx                                    ; 43                          ; 0xc02ea vgarom.asm:562
    db  0feh, 0c1h
    ; inc cl                                    ; fe c1                     ; 0xc02eb vgarom.asm:563
    cmp cl, 010h                              ; 80 f9 10                    ; 0xc02ed vgarom.asm:564
    jne short 002d9h                          ; 75 e7                       ; 0xc02f0 vgarom.asm:565
    mov dx, 003dah                            ; ba da 03                    ; 0xc02f2 vgarom.asm:566
    in AL, DX                                 ; ec                          ; 0xc02f5 vgarom.asm:567
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc02f6 vgarom.asm:568
    mov AL, strict byte 011h                  ; b0 11                       ; 0xc02f9 vgarom.asm:569
    out DX, AL                                ; ee                          ; 0xc02fb vgarom.asm:570
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc02fc vgarom.asm:571
    in AL, DX                                 ; ec                          ; 0xc02ff vgarom.asm:572
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc0300 vgarom.asm:573
    mov dx, 003dah                            ; ba da 03                    ; 0xc0303 vgarom.asm:574
    in AL, DX                                 ; ec                          ; 0xc0306 vgarom.asm:575
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0307 vgarom.asm:576
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc030a vgarom.asm:577
    out DX, AL                                ; ee                          ; 0xc030c vgarom.asm:578
    mov dx, 003dah                            ; ba da 03                    ; 0xc030d vgarom.asm:580
    in AL, DX                                 ; ec                          ; 0xc0310 vgarom.asm:581
    pop dx                                    ; 5a                          ; 0xc0311 vgarom.asm:583
    pop cx                                    ; 59                          ; 0xc0312 vgarom.asm:584
    pop bx                                    ; 5b                          ; 0xc0313 vgarom.asm:585
    pop ax                                    ; 58                          ; 0xc0314 vgarom.asm:586
    retn                                      ; c3                          ; 0xc0315 vgarom.asm:587
    push ax                                   ; 50                          ; 0xc0316 vgarom.asm:592
    push dx                                   ; 52                          ; 0xc0317 vgarom.asm:593
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc0318 vgarom.asm:594
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc031b vgarom.asm:595
    out DX, AL                                ; ee                          ; 0xc031d vgarom.asm:596
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc031e vgarom.asm:597
    pop ax                                    ; 58                          ; 0xc0321 vgarom.asm:598
    push ax                                   ; 50                          ; 0xc0322 vgarom.asm:599
    db  08ah, 0c4h
    ; mov al, ah                                ; 8a c4                     ; 0xc0323 vgarom.asm:600
    out DX, AL                                ; ee                          ; 0xc0325 vgarom.asm:601
    db  08ah, 0c5h
    ; mov al, ch                                ; 8a c5                     ; 0xc0326 vgarom.asm:602
    out DX, AL                                ; ee                          ; 0xc0328 vgarom.asm:603
    db  08ah, 0c1h
    ; mov al, cl                                ; 8a c1                     ; 0xc0329 vgarom.asm:604
    out DX, AL                                ; ee                          ; 0xc032b vgarom.asm:605
    pop dx                                    ; 5a                          ; 0xc032c vgarom.asm:606
    pop ax                                    ; 58                          ; 0xc032d vgarom.asm:607
    retn                                      ; c3                          ; 0xc032e vgarom.asm:608
    push ax                                   ; 50                          ; 0xc032f vgarom.asm:613
    push bx                                   ; 53                          ; 0xc0330 vgarom.asm:614
    push cx                                   ; 51                          ; 0xc0331 vgarom.asm:615
    push dx                                   ; 52                          ; 0xc0332 vgarom.asm:616
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc0333 vgarom.asm:617
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc0336 vgarom.asm:618
    out DX, AL                                ; ee                          ; 0xc0338 vgarom.asm:619
    pop dx                                    ; 5a                          ; 0xc0339 vgarom.asm:620
    push dx                                   ; 52                          ; 0xc033a vgarom.asm:621
    db  08bh, 0dah
    ; mov bx, dx                                ; 8b da                     ; 0xc033b vgarom.asm:622
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc033d vgarom.asm:623
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0340 vgarom.asm:625
    out DX, AL                                ; ee                          ; 0xc0343 vgarom.asm:626
    inc bx                                    ; 43                          ; 0xc0344 vgarom.asm:627
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0345 vgarom.asm:628
    out DX, AL                                ; ee                          ; 0xc0348 vgarom.asm:629
    inc bx                                    ; 43                          ; 0xc0349 vgarom.asm:630
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc034a vgarom.asm:631
    out DX, AL                                ; ee                          ; 0xc034d vgarom.asm:632
    inc bx                                    ; 43                          ; 0xc034e vgarom.asm:633
    dec cx                                    ; 49                          ; 0xc034f vgarom.asm:634
    jne short 00340h                          ; 75 ee                       ; 0xc0350 vgarom.asm:635
    pop dx                                    ; 5a                          ; 0xc0352 vgarom.asm:636
    pop cx                                    ; 59                          ; 0xc0353 vgarom.asm:637
    pop bx                                    ; 5b                          ; 0xc0354 vgarom.asm:638
    pop ax                                    ; 58                          ; 0xc0355 vgarom.asm:639
    retn                                      ; c3                          ; 0xc0356 vgarom.asm:640
    push ax                                   ; 50                          ; 0xc0357 vgarom.asm:645
    push bx                                   ; 53                          ; 0xc0358 vgarom.asm:646
    push dx                                   ; 52                          ; 0xc0359 vgarom.asm:647
    mov dx, 003dah                            ; ba da 03                    ; 0xc035a vgarom.asm:648
    in AL, DX                                 ; ec                          ; 0xc035d vgarom.asm:649
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc035e vgarom.asm:650
    mov AL, strict byte 010h                  ; b0 10                       ; 0xc0361 vgarom.asm:651
    out DX, AL                                ; ee                          ; 0xc0363 vgarom.asm:652
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc0364 vgarom.asm:653
    in AL, DX                                 ; ec                          ; 0xc0367 vgarom.asm:654
    and bl, 001h                              ; 80 e3 01                    ; 0xc0368 vgarom.asm:655
    jne short 00385h                          ; 75 18                       ; 0xc036b vgarom.asm:656
    and AL, strict byte 07fh                  ; 24 7f                       ; 0xc036d vgarom.asm:657
    sal bh, 1                                 ; d0 e7                       ; 0xc036f vgarom.asm:661
    sal bh, 1                                 ; d0 e7                       ; 0xc0371 vgarom.asm:662
    sal bh, 1                                 ; d0 e7                       ; 0xc0373 vgarom.asm:663
    sal bh, 1                                 ; d0 e7                       ; 0xc0375 vgarom.asm:664
    sal bh, 1                                 ; d0 e7                       ; 0xc0377 vgarom.asm:665
    sal bh, 1                                 ; d0 e7                       ; 0xc0379 vgarom.asm:666
    sal bh, 1                                 ; d0 e7                       ; 0xc037b vgarom.asm:667
    db  00ah, 0c7h
    ; or al, bh                                 ; 0a c7                     ; 0xc037d vgarom.asm:669
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc037f vgarom.asm:670
    out DX, AL                                ; ee                          ; 0xc0382 vgarom.asm:671
    jmp short 0039fh                          ; eb 1a                       ; 0xc0383 vgarom.asm:672
    push ax                                   ; 50                          ; 0xc0385 vgarom.asm:674
    mov dx, 003dah                            ; ba da 03                    ; 0xc0386 vgarom.asm:675
    in AL, DX                                 ; ec                          ; 0xc0389 vgarom.asm:676
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc038a vgarom.asm:677
    mov AL, strict byte 014h                  ; b0 14                       ; 0xc038d vgarom.asm:678
    out DX, AL                                ; ee                          ; 0xc038f vgarom.asm:679
    pop ax                                    ; 58                          ; 0xc0390 vgarom.asm:680
    and AL, strict byte 080h                  ; 24 80                       ; 0xc0391 vgarom.asm:681
    jne short 00399h                          ; 75 04                       ; 0xc0393 vgarom.asm:682
    sal bh, 1                                 ; d0 e7                       ; 0xc0395 vgarom.asm:686
    sal bh, 1                                 ; d0 e7                       ; 0xc0397 vgarom.asm:687
    and bh, 00fh                              ; 80 e7 0f                    ; 0xc0399 vgarom.asm:690
    db  08ah, 0c7h
    ; mov al, bh                                ; 8a c7                     ; 0xc039c vgarom.asm:691
    out DX, AL                                ; ee                          ; 0xc039e vgarom.asm:692
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc039f vgarom.asm:694
    out DX, AL                                ; ee                          ; 0xc03a1 vgarom.asm:695
    mov dx, 003dah                            ; ba da 03                    ; 0xc03a2 vgarom.asm:697
    in AL, DX                                 ; ec                          ; 0xc03a5 vgarom.asm:698
    pop dx                                    ; 5a                          ; 0xc03a6 vgarom.asm:700
    pop bx                                    ; 5b                          ; 0xc03a7 vgarom.asm:701
    pop ax                                    ; 58                          ; 0xc03a8 vgarom.asm:702
    retn                                      ; c3                          ; 0xc03a9 vgarom.asm:703
    push ax                                   ; 50                          ; 0xc03aa vgarom.asm:708
    push dx                                   ; 52                          ; 0xc03ab vgarom.asm:709
    mov dx, 003c7h                            ; ba c7 03                    ; 0xc03ac vgarom.asm:710
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc03af vgarom.asm:711
    out DX, AL                                ; ee                          ; 0xc03b1 vgarom.asm:712
    pop ax                                    ; 58                          ; 0xc03b2 vgarom.asm:713
    db  08ah, 0e0h
    ; mov ah, al                                ; 8a e0                     ; 0xc03b3 vgarom.asm:714
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc03b5 vgarom.asm:715
    in AL, DX                                 ; ec                          ; 0xc03b8 vgarom.asm:716
    xchg al, ah                               ; 86 e0                       ; 0xc03b9 vgarom.asm:717
    push ax                                   ; 50                          ; 0xc03bb vgarom.asm:718
    in AL, DX                                 ; ec                          ; 0xc03bc vgarom.asm:719
    db  08ah, 0e8h
    ; mov ch, al                                ; 8a e8                     ; 0xc03bd vgarom.asm:720
    in AL, DX                                 ; ec                          ; 0xc03bf vgarom.asm:721
    db  08ah, 0c8h
    ; mov cl, al                                ; 8a c8                     ; 0xc03c0 vgarom.asm:722
    pop dx                                    ; 5a                          ; 0xc03c2 vgarom.asm:723
    pop ax                                    ; 58                          ; 0xc03c3 vgarom.asm:724
    retn                                      ; c3                          ; 0xc03c4 vgarom.asm:725
    push ax                                   ; 50                          ; 0xc03c5 vgarom.asm:730
    push bx                                   ; 53                          ; 0xc03c6 vgarom.asm:731
    push cx                                   ; 51                          ; 0xc03c7 vgarom.asm:732
    push dx                                   ; 52                          ; 0xc03c8 vgarom.asm:733
    mov dx, 003c7h                            ; ba c7 03                    ; 0xc03c9 vgarom.asm:734
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc03cc vgarom.asm:735
    out DX, AL                                ; ee                          ; 0xc03ce vgarom.asm:736
    pop dx                                    ; 5a                          ; 0xc03cf vgarom.asm:737
    push dx                                   ; 52                          ; 0xc03d0 vgarom.asm:738
    db  08bh, 0dah
    ; mov bx, dx                                ; 8b da                     ; 0xc03d1 vgarom.asm:739
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc03d3 vgarom.asm:740
    in AL, DX                                 ; ec                          ; 0xc03d6 vgarom.asm:742
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc03d7 vgarom.asm:743
    inc bx                                    ; 43                          ; 0xc03da vgarom.asm:744
    in AL, DX                                 ; ec                          ; 0xc03db vgarom.asm:745
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc03dc vgarom.asm:746
    inc bx                                    ; 43                          ; 0xc03df vgarom.asm:747
    in AL, DX                                 ; ec                          ; 0xc03e0 vgarom.asm:748
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc03e1 vgarom.asm:749
    inc bx                                    ; 43                          ; 0xc03e4 vgarom.asm:750
    dec cx                                    ; 49                          ; 0xc03e5 vgarom.asm:751
    jne short 003d6h                          ; 75 ee                       ; 0xc03e6 vgarom.asm:752
    pop dx                                    ; 5a                          ; 0xc03e8 vgarom.asm:753
    pop cx                                    ; 59                          ; 0xc03e9 vgarom.asm:754
    pop bx                                    ; 5b                          ; 0xc03ea vgarom.asm:755
    pop ax                                    ; 58                          ; 0xc03eb vgarom.asm:756
    retn                                      ; c3                          ; 0xc03ec vgarom.asm:757
    push ax                                   ; 50                          ; 0xc03ed vgarom.asm:762
    push dx                                   ; 52                          ; 0xc03ee vgarom.asm:763
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc03ef vgarom.asm:764
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc03f2 vgarom.asm:765
    out DX, AL                                ; ee                          ; 0xc03f4 vgarom.asm:766
    pop dx                                    ; 5a                          ; 0xc03f5 vgarom.asm:767
    pop ax                                    ; 58                          ; 0xc03f6 vgarom.asm:768
    retn                                      ; c3                          ; 0xc03f7 vgarom.asm:769
    push ax                                   ; 50                          ; 0xc03f8 vgarom.asm:774
    push dx                                   ; 52                          ; 0xc03f9 vgarom.asm:775
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc03fa vgarom.asm:776
    in AL, DX                                 ; ec                          ; 0xc03fd vgarom.asm:777
    db  08ah, 0d8h
    ; mov bl, al                                ; 8a d8                     ; 0xc03fe vgarom.asm:778
    pop dx                                    ; 5a                          ; 0xc0400 vgarom.asm:779
    pop ax                                    ; 58                          ; 0xc0401 vgarom.asm:780
    retn                                      ; c3                          ; 0xc0402 vgarom.asm:781
    push ax                                   ; 50                          ; 0xc0403 vgarom.asm:786
    push dx                                   ; 52                          ; 0xc0404 vgarom.asm:787
    mov dx, 003dah                            ; ba da 03                    ; 0xc0405 vgarom.asm:788
    in AL, DX                                 ; ec                          ; 0xc0408 vgarom.asm:789
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0409 vgarom.asm:790
    mov AL, strict byte 010h                  ; b0 10                       ; 0xc040c vgarom.asm:791
    out DX, AL                                ; ee                          ; 0xc040e vgarom.asm:792
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc040f vgarom.asm:793
    in AL, DX                                 ; ec                          ; 0xc0412 vgarom.asm:794
    db  08ah, 0d8h
    ; mov bl, al                                ; 8a d8                     ; 0xc0413 vgarom.asm:795
    shr bl, 1                                 ; d0 eb                       ; 0xc0415 vgarom.asm:799
    shr bl, 1                                 ; d0 eb                       ; 0xc0417 vgarom.asm:800
    shr bl, 1                                 ; d0 eb                       ; 0xc0419 vgarom.asm:801
    shr bl, 1                                 ; d0 eb                       ; 0xc041b vgarom.asm:802
    shr bl, 1                                 ; d0 eb                       ; 0xc041d vgarom.asm:803
    shr bl, 1                                 ; d0 eb                       ; 0xc041f vgarom.asm:804
    shr bl, 1                                 ; d0 eb                       ; 0xc0421 vgarom.asm:805
    mov dx, 003dah                            ; ba da 03                    ; 0xc0423 vgarom.asm:807
    in AL, DX                                 ; ec                          ; 0xc0426 vgarom.asm:808
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0427 vgarom.asm:809
    mov AL, strict byte 014h                  ; b0 14                       ; 0xc042a vgarom.asm:810
    out DX, AL                                ; ee                          ; 0xc042c vgarom.asm:811
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc042d vgarom.asm:812
    in AL, DX                                 ; ec                          ; 0xc0430 vgarom.asm:813
    db  08ah, 0f8h
    ; mov bh, al                                ; 8a f8                     ; 0xc0431 vgarom.asm:814
    and bh, 00fh                              ; 80 e7 0f                    ; 0xc0433 vgarom.asm:815
    test bl, 001h                             ; f6 c3 01                    ; 0xc0436 vgarom.asm:816
    jne short 0043fh                          ; 75 04                       ; 0xc0439 vgarom.asm:817
    shr bh, 1                                 ; d0 ef                       ; 0xc043b vgarom.asm:821
    shr bh, 1                                 ; d0 ef                       ; 0xc043d vgarom.asm:822
    mov dx, 003dah                            ; ba da 03                    ; 0xc043f vgarom.asm:825
    in AL, DX                                 ; ec                          ; 0xc0442 vgarom.asm:826
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0443 vgarom.asm:827
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc0446 vgarom.asm:828
    out DX, AL                                ; ee                          ; 0xc0448 vgarom.asm:829
    mov dx, 003dah                            ; ba da 03                    ; 0xc0449 vgarom.asm:831
    in AL, DX                                 ; ec                          ; 0xc044c vgarom.asm:832
    pop dx                                    ; 5a                          ; 0xc044d vgarom.asm:834
    pop ax                                    ; 58                          ; 0xc044e vgarom.asm:835
    retn                                      ; c3                          ; 0xc044f vgarom.asm:836
    push ax                                   ; 50                          ; 0xc0450 vgarom.asm:841
    push dx                                   ; 52                          ; 0xc0451 vgarom.asm:842
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc0452 vgarom.asm:843
    db  08ah, 0e3h
    ; mov ah, bl                                ; 8a e3                     ; 0xc0455 vgarom.asm:844
    mov AL, strict byte 003h                  ; b0 03                       ; 0xc0457 vgarom.asm:845
    out DX, ax                                ; ef                          ; 0xc0459 vgarom.asm:846
    pop dx                                    ; 5a                          ; 0xc045a vgarom.asm:847
    pop ax                                    ; 58                          ; 0xc045b vgarom.asm:848
    retn                                      ; c3                          ; 0xc045c vgarom.asm:849
    push DS                                   ; 1e                          ; 0xc045d vgarom.asm:854
    push ax                                   ; 50                          ; 0xc045e vgarom.asm:855
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc045f vgarom.asm:856
    mov ds, ax                                ; 8e d8                       ; 0xc0462 vgarom.asm:857
    db  032h, 0edh
    ; xor ch, ch                                ; 32 ed                     ; 0xc0464 vgarom.asm:858
    mov bx, 00088h                            ; bb 88 00                    ; 0xc0466 vgarom.asm:859
    mov cl, byte [bx]                         ; 8a 0f                       ; 0xc0469 vgarom.asm:860
    and cl, 00fh                              ; 80 e1 0f                    ; 0xc046b vgarom.asm:861
    mov bx, strict word 00063h                ; bb 63 00                    ; 0xc046e vgarom.asm:862
    mov ax, word [bx]                         ; 8b 07                       ; 0xc0471 vgarom.asm:863
    mov bx, strict word 00003h                ; bb 03 00                    ; 0xc0473 vgarom.asm:864
    cmp ax, 003b4h                            ; 3d b4 03                    ; 0xc0476 vgarom.asm:865
    jne short 0047dh                          ; 75 02                       ; 0xc0479 vgarom.asm:866
    mov BH, strict byte 001h                  ; b7 01                       ; 0xc047b vgarom.asm:867
    pop ax                                    ; 58                          ; 0xc047d vgarom.asm:869
    pop DS                                    ; 1f                          ; 0xc047e vgarom.asm:870
    retn                                      ; c3                          ; 0xc047f vgarom.asm:871
    push DS                                   ; 1e                          ; 0xc0480 vgarom.asm:879
    push bx                                   ; 53                          ; 0xc0481 vgarom.asm:880
    push dx                                   ; 52                          ; 0xc0482 vgarom.asm:881
    db  08ah, 0d0h
    ; mov dl, al                                ; 8a d0                     ; 0xc0483 vgarom.asm:882
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0485 vgarom.asm:883
    mov ds, ax                                ; 8e d8                       ; 0xc0488 vgarom.asm:884
    mov bx, 00089h                            ; bb 89 00                    ; 0xc048a vgarom.asm:885
    mov al, byte [bx]                         ; 8a 07                       ; 0xc048d vgarom.asm:886
    mov bx, 00088h                            ; bb 88 00                    ; 0xc048f vgarom.asm:887
    mov ah, byte [bx]                         ; 8a 27                       ; 0xc0492 vgarom.asm:888
    cmp dl, 001h                              ; 80 fa 01                    ; 0xc0494 vgarom.asm:889
    je short 004aeh                           ; 74 15                       ; 0xc0497 vgarom.asm:890
    jc short 004b8h                           ; 72 1d                       ; 0xc0499 vgarom.asm:891
    cmp dl, 002h                              ; 80 fa 02                    ; 0xc049b vgarom.asm:892
    je short 004a2h                           ; 74 02                       ; 0xc049e vgarom.asm:893
    jmp short 004cch                          ; eb 2a                       ; 0xc04a0 vgarom.asm:903
    and AL, strict byte 07fh                  ; 24 7f                       ; 0xc04a2 vgarom.asm:909
    or AL, strict byte 010h                   ; 0c 10                       ; 0xc04a4 vgarom.asm:910
    and ah, 0f0h                              ; 80 e4 f0                    ; 0xc04a6 vgarom.asm:911
    or ah, 009h                               ; 80 cc 09                    ; 0xc04a9 vgarom.asm:912
    jne short 004c2h                          ; 75 14                       ; 0xc04ac vgarom.asm:913
    and AL, strict byte 06fh                  ; 24 6f                       ; 0xc04ae vgarom.asm:919
    and ah, 0f0h                              ; 80 e4 f0                    ; 0xc04b0 vgarom.asm:920
    or ah, 009h                               ; 80 cc 09                    ; 0xc04b3 vgarom.asm:921
    jne short 004c2h                          ; 75 0a                       ; 0xc04b6 vgarom.asm:922
    and AL, strict byte 0efh                  ; 24 ef                       ; 0xc04b8 vgarom.asm:928
    or AL, strict byte 080h                   ; 0c 80                       ; 0xc04ba vgarom.asm:929
    and ah, 0f0h                              ; 80 e4 f0                    ; 0xc04bc vgarom.asm:930
    or ah, 008h                               ; 80 cc 08                    ; 0xc04bf vgarom.asm:931
    mov bx, 00089h                            ; bb 89 00                    ; 0xc04c2 vgarom.asm:933
    mov byte [bx], al                         ; 88 07                       ; 0xc04c5 vgarom.asm:934
    mov bx, 00088h                            ; bb 88 00                    ; 0xc04c7 vgarom.asm:935
    mov byte [bx], ah                         ; 88 27                       ; 0xc04ca vgarom.asm:936
    mov ax, 01212h                            ; b8 12 12                    ; 0xc04cc vgarom.asm:938
    pop dx                                    ; 5a                          ; 0xc04cf vgarom.asm:939
    pop bx                                    ; 5b                          ; 0xc04d0 vgarom.asm:940
    pop DS                                    ; 1f                          ; 0xc04d1 vgarom.asm:941
    retn                                      ; c3                          ; 0xc04d2 vgarom.asm:942
    push DS                                   ; 1e                          ; 0xc04d3 vgarom.asm:951
    push bx                                   ; 53                          ; 0xc04d4 vgarom.asm:952
    push dx                                   ; 52                          ; 0xc04d5 vgarom.asm:953
    db  08ah, 0d0h
    ; mov dl, al                                ; 8a d0                     ; 0xc04d6 vgarom.asm:954
    and dl, 001h                              ; 80 e2 01                    ; 0xc04d8 vgarom.asm:955
    sal dl, 1                                 ; d0 e2                       ; 0xc04db vgarom.asm:959
    sal dl, 1                                 ; d0 e2                       ; 0xc04dd vgarom.asm:960
    sal dl, 1                                 ; d0 e2                       ; 0xc04df vgarom.asm:961
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc04e1 vgarom.asm:963
    mov ds, ax                                ; 8e d8                       ; 0xc04e4 vgarom.asm:964
    mov bx, 00089h                            ; bb 89 00                    ; 0xc04e6 vgarom.asm:965
    mov al, byte [bx]                         ; 8a 07                       ; 0xc04e9 vgarom.asm:966
    and AL, strict byte 0f7h                  ; 24 f7                       ; 0xc04eb vgarom.asm:967
    db  00ah, 0c2h
    ; or al, dl                                 ; 0a c2                     ; 0xc04ed vgarom.asm:968
    mov byte [bx], al                         ; 88 07                       ; 0xc04ef vgarom.asm:969
    mov ax, 01212h                            ; b8 12 12                    ; 0xc04f1 vgarom.asm:970
    pop dx                                    ; 5a                          ; 0xc04f4 vgarom.asm:971
    pop bx                                    ; 5b                          ; 0xc04f5 vgarom.asm:972
    pop DS                                    ; 1f                          ; 0xc04f6 vgarom.asm:973
    retn                                      ; c3                          ; 0xc04f7 vgarom.asm:974
    push bx                                   ; 53                          ; 0xc04f8 vgarom.asm:978
    push dx                                   ; 52                          ; 0xc04f9 vgarom.asm:979
    db  08ah, 0d8h
    ; mov bl, al                                ; 8a d8                     ; 0xc04fa vgarom.asm:980
    and bl, 001h                              ; 80 e3 01                    ; 0xc04fc vgarom.asm:981
    xor bl, 001h                              ; 80 f3 01                    ; 0xc04ff vgarom.asm:982
    sal bl, 1                                 ; d0 e3                       ; 0xc0502 vgarom.asm:983
    mov dx, 003cch                            ; ba cc 03                    ; 0xc0504 vgarom.asm:984
    in AL, DX                                 ; ec                          ; 0xc0507 vgarom.asm:985
    and AL, strict byte 0fdh                  ; 24 fd                       ; 0xc0508 vgarom.asm:986
    db  00ah, 0c3h
    ; or al, bl                                 ; 0a c3                     ; 0xc050a vgarom.asm:987
    mov dx, 003c2h                            ; ba c2 03                    ; 0xc050c vgarom.asm:988
    out DX, AL                                ; ee                          ; 0xc050f vgarom.asm:989
    mov ax, 01212h                            ; b8 12 12                    ; 0xc0510 vgarom.asm:990
    pop dx                                    ; 5a                          ; 0xc0513 vgarom.asm:991
    pop bx                                    ; 5b                          ; 0xc0514 vgarom.asm:992
    retn                                      ; c3                          ; 0xc0515 vgarom.asm:993
    push DS                                   ; 1e                          ; 0xc0516 vgarom.asm:997
    push bx                                   ; 53                          ; 0xc0517 vgarom.asm:998
    push dx                                   ; 52                          ; 0xc0518 vgarom.asm:999
    db  08ah, 0d0h
    ; mov dl, al                                ; 8a d0                     ; 0xc0519 vgarom.asm:1000
    and dl, 001h                              ; 80 e2 01                    ; 0xc051b vgarom.asm:1001
    xor dl, 001h                              ; 80 f2 01                    ; 0xc051e vgarom.asm:1002
    sal dl, 1                                 ; d0 e2                       ; 0xc0521 vgarom.asm:1003
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0523 vgarom.asm:1004
    mov ds, ax                                ; 8e d8                       ; 0xc0526 vgarom.asm:1005
    mov bx, 00089h                            ; bb 89 00                    ; 0xc0528 vgarom.asm:1006
    mov al, byte [bx]                         ; 8a 07                       ; 0xc052b vgarom.asm:1007
    and AL, strict byte 0fdh                  ; 24 fd                       ; 0xc052d vgarom.asm:1008
    db  00ah, 0c2h
    ; or al, dl                                 ; 0a c2                     ; 0xc052f vgarom.asm:1009
    mov byte [bx], al                         ; 88 07                       ; 0xc0531 vgarom.asm:1010
    mov ax, 01212h                            ; b8 12 12                    ; 0xc0533 vgarom.asm:1011
    pop dx                                    ; 5a                          ; 0xc0536 vgarom.asm:1012
    pop bx                                    ; 5b                          ; 0xc0537 vgarom.asm:1013
    pop DS                                    ; 1f                          ; 0xc0538 vgarom.asm:1014
    retn                                      ; c3                          ; 0xc0539 vgarom.asm:1015
    push DS                                   ; 1e                          ; 0xc053a vgarom.asm:1019
    push bx                                   ; 53                          ; 0xc053b vgarom.asm:1020
    push dx                                   ; 52                          ; 0xc053c vgarom.asm:1021
    db  08ah, 0d0h
    ; mov dl, al                                ; 8a d0                     ; 0xc053d vgarom.asm:1022
    and dl, 001h                              ; 80 e2 01                    ; 0xc053f vgarom.asm:1023
    xor dl, 001h                              ; 80 f2 01                    ; 0xc0542 vgarom.asm:1024
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0545 vgarom.asm:1025
    mov ds, ax                                ; 8e d8                       ; 0xc0548 vgarom.asm:1026
    mov bx, 00089h                            ; bb 89 00                    ; 0xc054a vgarom.asm:1027
    mov al, byte [bx]                         ; 8a 07                       ; 0xc054d vgarom.asm:1028
    and AL, strict byte 0feh                  ; 24 fe                       ; 0xc054f vgarom.asm:1029
    db  00ah, 0c2h
    ; or al, dl                                 ; 0a c2                     ; 0xc0551 vgarom.asm:1030
    mov byte [bx], al                         ; 88 07                       ; 0xc0553 vgarom.asm:1031
    mov ax, 01212h                            ; b8 12 12                    ; 0xc0555 vgarom.asm:1032
    pop dx                                    ; 5a                          ; 0xc0558 vgarom.asm:1033
    pop bx                                    ; 5b                          ; 0xc0559 vgarom.asm:1034
    pop DS                                    ; 1f                          ; 0xc055a vgarom.asm:1035
    retn                                      ; c3                          ; 0xc055b vgarom.asm:1036
    cmp AL, strict byte 000h                  ; 3c 00                       ; 0xc055c vgarom.asm:1041
    je short 00565h                           ; 74 05                       ; 0xc055e vgarom.asm:1042
    cmp AL, strict byte 001h                  ; 3c 01                       ; 0xc0560 vgarom.asm:1043
    je short 0057ah                           ; 74 16                       ; 0xc0562 vgarom.asm:1044
    retn                                      ; c3                          ; 0xc0564 vgarom.asm:1048
    push DS                                   ; 1e                          ; 0xc0565 vgarom.asm:1050
    push ax                                   ; 50                          ; 0xc0566 vgarom.asm:1051
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0567 vgarom.asm:1052
    mov ds, ax                                ; 8e d8                       ; 0xc056a vgarom.asm:1053
    mov bx, 0008ah                            ; bb 8a 00                    ; 0xc056c vgarom.asm:1054
    mov al, byte [bx]                         ; 8a 07                       ; 0xc056f vgarom.asm:1055
    db  08ah, 0d8h
    ; mov bl, al                                ; 8a d8                     ; 0xc0571 vgarom.asm:1056
    db  032h, 0ffh
    ; xor bh, bh                                ; 32 ff                     ; 0xc0573 vgarom.asm:1057
    pop ax                                    ; 58                          ; 0xc0575 vgarom.asm:1058
    db  08ah, 0c4h
    ; mov al, ah                                ; 8a c4                     ; 0xc0576 vgarom.asm:1059
    pop DS                                    ; 1f                          ; 0xc0578 vgarom.asm:1060
    retn                                      ; c3                          ; 0xc0579 vgarom.asm:1061
    push DS                                   ; 1e                          ; 0xc057a vgarom.asm:1063
    push ax                                   ; 50                          ; 0xc057b vgarom.asm:1064
    push bx                                   ; 53                          ; 0xc057c vgarom.asm:1065
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc057d vgarom.asm:1066
    mov ds, ax                                ; 8e d8                       ; 0xc0580 vgarom.asm:1067
    db  08bh, 0c3h
    ; mov ax, bx                                ; 8b c3                     ; 0xc0582 vgarom.asm:1068
    mov bx, 0008ah                            ; bb 8a 00                    ; 0xc0584 vgarom.asm:1069
    mov byte [bx], al                         ; 88 07                       ; 0xc0587 vgarom.asm:1070
    pop bx                                    ; 5b                          ; 0xc0589 vgarom.asm:1080
    pop ax                                    ; 58                          ; 0xc058a vgarom.asm:1081
    db  08ah, 0c4h
    ; mov al, ah                                ; 8a c4                     ; 0xc058b vgarom.asm:1082
    pop DS                                    ; 1f                          ; 0xc058d vgarom.asm:1083
    retn                                      ; c3                          ; 0xc058e vgarom.asm:1084
    times 0x1 db 0
  ; disGetNextSymbol 0xc0590 LB 0x3b2 -> off=0x0 cb=0000000000000007 uValue=00000000000c0590 'do_out_dx_ax'
do_out_dx_ax:                                ; 0xc0590 LB 0x7
    xchg ah, al                               ; 86 c4                       ; 0xc0590 vberom.asm:69
    out DX, AL                                ; ee                          ; 0xc0592 vberom.asm:70
    xchg ah, al                               ; 86 c4                       ; 0xc0593 vberom.asm:71
    out DX, AL                                ; ee                          ; 0xc0595 vberom.asm:72
    retn                                      ; c3                          ; 0xc0596 vberom.asm:73
  ; disGetNextSymbol 0xc0597 LB 0x3ab -> off=0x0 cb=0000000000000043 uValue=00000000000c0597 'do_in_ax_dx'
do_in_ax_dx:                                 ; 0xc0597 LB 0x43
    in AL, DX                                 ; ec                          ; 0xc0597 vberom.asm:76
    xchg ah, al                               ; 86 c4                       ; 0xc0598 vberom.asm:77
    in AL, DX                                 ; ec                          ; 0xc059a vberom.asm:78
    retn                                      ; c3                          ; 0xc059b vberom.asm:79
    push ax                                   ; 50                          ; 0xc059c vberom.asm:90
    push dx                                   ; 52                          ; 0xc059d vberom.asm:91
    mov dx, 003dah                            ; ba da 03                    ; 0xc059e vberom.asm:92
    in AL, DX                                 ; ec                          ; 0xc05a1 vberom.asm:94
    test AL, strict byte 008h                 ; a8 08                       ; 0xc05a2 vberom.asm:95
    je short 005a1h                           ; 74 fb                       ; 0xc05a4 vberom.asm:96
    pop dx                                    ; 5a                          ; 0xc05a6 vberom.asm:97
    pop ax                                    ; 58                          ; 0xc05a7 vberom.asm:98
    retn                                      ; c3                          ; 0xc05a8 vberom.asm:99
    push ax                                   ; 50                          ; 0xc05a9 vberom.asm:102
    push dx                                   ; 52                          ; 0xc05aa vberom.asm:103
    mov dx, 003dah                            ; ba da 03                    ; 0xc05ab vberom.asm:104
    in AL, DX                                 ; ec                          ; 0xc05ae vberom.asm:106
    test AL, strict byte 008h                 ; a8 08                       ; 0xc05af vberom.asm:107
    jne short 005aeh                          ; 75 fb                       ; 0xc05b1 vberom.asm:108
    pop dx                                    ; 5a                          ; 0xc05b3 vberom.asm:109
    pop ax                                    ; 58                          ; 0xc05b4 vberom.asm:110
    retn                                      ; c3                          ; 0xc05b5 vberom.asm:111
    push dx                                   ; 52                          ; 0xc05b6 vberom.asm:116
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc05b7 vberom.asm:117
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc05ba vberom.asm:118
    call 00590h                               ; e8 d0 ff                    ; 0xc05bd vberom.asm:119
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc05c0 vberom.asm:120
    call 00597h                               ; e8 d1 ff                    ; 0xc05c3 vberom.asm:121
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc05c6 vberom.asm:122
    jbe short 005d8h                          ; 76 0e                       ; 0xc05c8 vberom.asm:123
    db  08ah, 0e0h
    ; mov ah, al                                ; 8a e0                     ; 0xc05ca vberom.asm:124
    shr ah, 1                                 ; d0 ec                       ; 0xc05cc vberom.asm:128
    shr ah, 1                                 ; d0 ec                       ; 0xc05ce vberom.asm:129
    shr ah, 1                                 ; d0 ec                       ; 0xc05d0 vberom.asm:130
    test AL, strict byte 007h                 ; a8 07                       ; 0xc05d2 vberom.asm:132
    je short 005d8h                           ; 74 02                       ; 0xc05d4 vberom.asm:133
    db  0feh, 0c4h
    ; inc ah                                    ; fe c4                     ; 0xc05d6 vberom.asm:134
    pop dx                                    ; 5a                          ; 0xc05d8 vberom.asm:136
    retn                                      ; c3                          ; 0xc05d9 vberom.asm:137
  ; disGetNextSymbol 0xc05da LB 0x368 -> off=0x0 cb=0000000000000026 uValue=00000000000c05da '_dispi_get_max_bpp'
_dispi_get_max_bpp:                          ; 0xc05da LB 0x26
    push dx                                   ; 52                          ; 0xc05da vberom.asm:142
    push bx                                   ; 53                          ; 0xc05db vberom.asm:143
    call 00614h                               ; e8 35 00                    ; 0xc05dc vberom.asm:144
    db  08bh, 0d8h
    ; mov bx, ax                                ; 8b d8                     ; 0xc05df vberom.asm:145
    or ax, strict byte 00002h                 ; 83 c8 02                    ; 0xc05e1 vberom.asm:146
    call 00600h                               ; e8 19 00                    ; 0xc05e4 vberom.asm:147
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc05e7 vberom.asm:148
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc05ea vberom.asm:149
    call 00590h                               ; e8 a0 ff                    ; 0xc05ed vberom.asm:150
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc05f0 vberom.asm:151
    call 00597h                               ; e8 a1 ff                    ; 0xc05f3 vberom.asm:152
    push ax                                   ; 50                          ; 0xc05f6 vberom.asm:153
    db  08bh, 0c3h
    ; mov ax, bx                                ; 8b c3                     ; 0xc05f7 vberom.asm:154
    call 00600h                               ; e8 04 00                    ; 0xc05f9 vberom.asm:155
    pop ax                                    ; 58                          ; 0xc05fc vberom.asm:156
    pop bx                                    ; 5b                          ; 0xc05fd vberom.asm:157
    pop dx                                    ; 5a                          ; 0xc05fe vberom.asm:158
    retn                                      ; c3                          ; 0xc05ff vberom.asm:159
  ; disGetNextSymbol 0xc0600 LB 0x342 -> off=0x0 cb=0000000000000026 uValue=00000000000c0600 'dispi_set_enable_'
dispi_set_enable_:                           ; 0xc0600 LB 0x26
    push dx                                   ; 52                          ; 0xc0600 vberom.asm:162
    push ax                                   ; 50                          ; 0xc0601 vberom.asm:163
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc0602 vberom.asm:164
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc0605 vberom.asm:165
    call 00590h                               ; e8 85 ff                    ; 0xc0608 vberom.asm:166
    pop ax                                    ; 58                          ; 0xc060b vberom.asm:167
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc060c vberom.asm:168
    call 00590h                               ; e8 7e ff                    ; 0xc060f vberom.asm:169
    pop dx                                    ; 5a                          ; 0xc0612 vberom.asm:170
    retn                                      ; c3                          ; 0xc0613 vberom.asm:171
    push dx                                   ; 52                          ; 0xc0614 vberom.asm:174
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc0615 vberom.asm:175
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc0618 vberom.asm:176
    call 00590h                               ; e8 72 ff                    ; 0xc061b vberom.asm:177
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc061e vberom.asm:178
    call 00597h                               ; e8 73 ff                    ; 0xc0621 vberom.asm:179
    pop dx                                    ; 5a                          ; 0xc0624 vberom.asm:180
    retn                                      ; c3                          ; 0xc0625 vberom.asm:181
  ; disGetNextSymbol 0xc0626 LB 0x31c -> off=0x0 cb=0000000000000026 uValue=00000000000c0626 'dispi_set_bank_'
dispi_set_bank_:                             ; 0xc0626 LB 0x26
    push dx                                   ; 52                          ; 0xc0626 vberom.asm:184
    push ax                                   ; 50                          ; 0xc0627 vberom.asm:185
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc0628 vberom.asm:186
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc062b vberom.asm:187
    call 00590h                               ; e8 5f ff                    ; 0xc062e vberom.asm:188
    pop ax                                    ; 58                          ; 0xc0631 vberom.asm:189
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc0632 vberom.asm:190
    call 00590h                               ; e8 58 ff                    ; 0xc0635 vberom.asm:191
    pop dx                                    ; 5a                          ; 0xc0638 vberom.asm:192
    retn                                      ; c3                          ; 0xc0639 vberom.asm:193
    push dx                                   ; 52                          ; 0xc063a vberom.asm:196
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc063b vberom.asm:197
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc063e vberom.asm:198
    call 00590h                               ; e8 4c ff                    ; 0xc0641 vberom.asm:199
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc0644 vberom.asm:200
    call 00597h                               ; e8 4d ff                    ; 0xc0647 vberom.asm:201
    pop dx                                    ; 5a                          ; 0xc064a vberom.asm:202
    retn                                      ; c3                          ; 0xc064b vberom.asm:203
  ; disGetNextSymbol 0xc064c LB 0x2f6 -> off=0x0 cb=00000000000000ac uValue=00000000000c064c '_dispi_set_bank_farcall'
_dispi_set_bank_farcall:                     ; 0xc064c LB 0xac
    cmp bx, 00100h                            ; 81 fb 00 01                 ; 0xc064c vberom.asm:206
    je short 00676h                           ; 74 24                       ; 0xc0650 vberom.asm:207
    db  00bh, 0dbh
    ; or bx, bx                                 ; 0b db                     ; 0xc0652 vberom.asm:208
    jne short 00688h                          ; 75 32                       ; 0xc0654 vberom.asm:209
    db  08bh, 0c2h
    ; mov ax, dx                                ; 8b c2                     ; 0xc0656 vberom.asm:210
    push dx                                   ; 52                          ; 0xc0658 vberom.asm:211
    push ax                                   ; 50                          ; 0xc0659 vberom.asm:212
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc065a vberom.asm:213
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc065d vberom.asm:214
    call 00590h                               ; e8 2d ff                    ; 0xc0660 vberom.asm:215
    pop ax                                    ; 58                          ; 0xc0663 vberom.asm:216
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc0664 vberom.asm:217
    call 00590h                               ; e8 26 ff                    ; 0xc0667 vberom.asm:218
    call 00597h                               ; e8 2a ff                    ; 0xc066a vberom.asm:219
    pop dx                                    ; 5a                          ; 0xc066d vberom.asm:220
    db  03bh, 0d0h
    ; cmp dx, ax                                ; 3b d0                     ; 0xc066e vberom.asm:221
    jne short 00688h                          ; 75 16                       ; 0xc0670 vberom.asm:222
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc0672 vberom.asm:223
    retf                                      ; cb                          ; 0xc0675 vberom.asm:224
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc0676 vberom.asm:226
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc0679 vberom.asm:227
    call 00590h                               ; e8 11 ff                    ; 0xc067c vberom.asm:228
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc067f vberom.asm:229
    call 00597h                               ; e8 12 ff                    ; 0xc0682 vberom.asm:230
    db  08bh, 0d0h
    ; mov dx, ax                                ; 8b d0                     ; 0xc0685 vberom.asm:231
    retf                                      ; cb                          ; 0xc0687 vberom.asm:232
    mov ax, 0014fh                            ; b8 4f 01                    ; 0xc0688 vberom.asm:234
    retf                                      ; cb                          ; 0xc068b vberom.asm:235
    push dx                                   ; 52                          ; 0xc068c vberom.asm:238
    push ax                                   ; 50                          ; 0xc068d vberom.asm:239
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc068e vberom.asm:240
    mov ax, strict word 00008h                ; b8 08 00                    ; 0xc0691 vberom.asm:241
    call 00590h                               ; e8 f9 fe                    ; 0xc0694 vberom.asm:242
    pop ax                                    ; 58                          ; 0xc0697 vberom.asm:243
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc0698 vberom.asm:244
    call 00590h                               ; e8 f2 fe                    ; 0xc069b vberom.asm:245
    pop dx                                    ; 5a                          ; 0xc069e vberom.asm:246
    retn                                      ; c3                          ; 0xc069f vberom.asm:247
    push dx                                   ; 52                          ; 0xc06a0 vberom.asm:250
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc06a1 vberom.asm:251
    mov ax, strict word 00008h                ; b8 08 00                    ; 0xc06a4 vberom.asm:252
    call 00590h                               ; e8 e6 fe                    ; 0xc06a7 vberom.asm:253
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc06aa vberom.asm:254
    call 00597h                               ; e8 e7 fe                    ; 0xc06ad vberom.asm:255
    pop dx                                    ; 5a                          ; 0xc06b0 vberom.asm:256
    retn                                      ; c3                          ; 0xc06b1 vberom.asm:257
    push dx                                   ; 52                          ; 0xc06b2 vberom.asm:260
    push ax                                   ; 50                          ; 0xc06b3 vberom.asm:261
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc06b4 vberom.asm:262
    mov ax, strict word 00009h                ; b8 09 00                    ; 0xc06b7 vberom.asm:263
    call 00590h                               ; e8 d3 fe                    ; 0xc06ba vberom.asm:264
    pop ax                                    ; 58                          ; 0xc06bd vberom.asm:265
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc06be vberom.asm:266
    call 00590h                               ; e8 cc fe                    ; 0xc06c1 vberom.asm:267
    pop dx                                    ; 5a                          ; 0xc06c4 vberom.asm:268
    retn                                      ; c3                          ; 0xc06c5 vberom.asm:269
    push dx                                   ; 52                          ; 0xc06c6 vberom.asm:272
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc06c7 vberom.asm:273
    mov ax, strict word 00009h                ; b8 09 00                    ; 0xc06ca vberom.asm:274
    call 00590h                               ; e8 c0 fe                    ; 0xc06cd vberom.asm:275
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc06d0 vberom.asm:276
    call 00597h                               ; e8 c1 fe                    ; 0xc06d3 vberom.asm:277
    pop dx                                    ; 5a                          ; 0xc06d6 vberom.asm:278
    retn                                      ; c3                          ; 0xc06d7 vberom.asm:279
    push ax                                   ; 50                          ; 0xc06d8 vberom.asm:282
    push bx                                   ; 53                          ; 0xc06d9 vberom.asm:283
    push dx                                   ; 52                          ; 0xc06da vberom.asm:284
    db  08bh, 0d8h
    ; mov bx, ax                                ; 8b d8                     ; 0xc06db vberom.asm:285
    call 005b6h                               ; e8 d6 fe                    ; 0xc06dd vberom.asm:286
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc06e0 vberom.asm:287
    jnbe short 006e6h                         ; 77 02                       ; 0xc06e2 vberom.asm:288
    shr bx, 1                                 ; d1 eb                       ; 0xc06e4 vberom.asm:289
    shr bx, 1                                 ; d1 eb                       ; 0xc06e6 vberom.asm:294
    shr bx, 1                                 ; d1 eb                       ; 0xc06e8 vberom.asm:295
    shr bx, 1                                 ; d1 eb                       ; 0xc06ea vberom.asm:296
    mov dx, 003d4h                            ; ba d4 03                    ; 0xc06ec vberom.asm:298
    db  08ah, 0e3h
    ; mov ah, bl                                ; 8a e3                     ; 0xc06ef vberom.asm:299
    mov AL, strict byte 013h                  ; b0 13                       ; 0xc06f1 vberom.asm:300
    out DX, ax                                ; ef                          ; 0xc06f3 vberom.asm:301
    pop dx                                    ; 5a                          ; 0xc06f4 vberom.asm:302
    pop bx                                    ; 5b                          ; 0xc06f5 vberom.asm:303
    pop ax                                    ; 58                          ; 0xc06f6 vberom.asm:304
    retn                                      ; c3                          ; 0xc06f7 vberom.asm:305
  ; disGetNextSymbol 0xc06f8 LB 0x24a -> off=0x0 cb=00000000000000f0 uValue=00000000000c06f8 '_vga_compat_setup'
_vga_compat_setup:                           ; 0xc06f8 LB 0xf0
    push ax                                   ; 50                          ; 0xc06f8 vberom.asm:308
    push dx                                   ; 52                          ; 0xc06f9 vberom.asm:309
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc06fa vberom.asm:312
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc06fd vberom.asm:313
    call 00590h                               ; e8 8d fe                    ; 0xc0700 vberom.asm:314
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc0703 vberom.asm:315
    call 00597h                               ; e8 8e fe                    ; 0xc0706 vberom.asm:316
    push ax                                   ; 50                          ; 0xc0709 vberom.asm:317
    mov dx, 003d4h                            ; ba d4 03                    ; 0xc070a vberom.asm:318
    mov ax, strict word 00011h                ; b8 11 00                    ; 0xc070d vberom.asm:319
    out DX, ax                                ; ef                          ; 0xc0710 vberom.asm:320
    pop ax                                    ; 58                          ; 0xc0711 vberom.asm:321
    push ax                                   ; 50                          ; 0xc0712 vberom.asm:322
    shr ax, 1                                 ; d1 e8                       ; 0xc0713 vberom.asm:326
    shr ax, 1                                 ; d1 e8                       ; 0xc0715 vberom.asm:327
    shr ax, 1                                 ; d1 e8                       ; 0xc0717 vberom.asm:328
    dec ax                                    ; 48                          ; 0xc0719 vberom.asm:330
    db  08ah, 0e0h
    ; mov ah, al                                ; 8a e0                     ; 0xc071a vberom.asm:331
    mov AL, strict byte 001h                  ; b0 01                       ; 0xc071c vberom.asm:332
    out DX, ax                                ; ef                          ; 0xc071e vberom.asm:333
    pop ax                                    ; 58                          ; 0xc071f vberom.asm:334
    call 006d8h                               ; e8 b5 ff                    ; 0xc0720 vberom.asm:335
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc0723 vberom.asm:338
    mov ax, strict word 00002h                ; b8 02 00                    ; 0xc0726 vberom.asm:339
    call 00590h                               ; e8 64 fe                    ; 0xc0729 vberom.asm:340
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc072c vberom.asm:341
    call 00597h                               ; e8 65 fe                    ; 0xc072f vberom.asm:342
    dec ax                                    ; 48                          ; 0xc0732 vberom.asm:343
    push ax                                   ; 50                          ; 0xc0733 vberom.asm:344
    mov dx, 003d4h                            ; ba d4 03                    ; 0xc0734 vberom.asm:345
    db  08ah, 0e0h
    ; mov ah, al                                ; 8a e0                     ; 0xc0737 vberom.asm:346
    mov AL, strict byte 012h                  ; b0 12                       ; 0xc0739 vberom.asm:347
    out DX, ax                                ; ef                          ; 0xc073b vberom.asm:348
    pop ax                                    ; 58                          ; 0xc073c vberom.asm:349
    mov AL, strict byte 007h                  ; b0 07                       ; 0xc073d vberom.asm:350
    out DX, AL                                ; ee                          ; 0xc073f vberom.asm:351
    inc dx                                    ; 42                          ; 0xc0740 vberom.asm:352
    in AL, DX                                 ; ec                          ; 0xc0741 vberom.asm:353
    and AL, strict byte 0bdh                  ; 24 bd                       ; 0xc0742 vberom.asm:354
    test ah, 001h                             ; f6 c4 01                    ; 0xc0744 vberom.asm:355
    je short 0074bh                           ; 74 02                       ; 0xc0747 vberom.asm:356
    or AL, strict byte 002h                   ; 0c 02                       ; 0xc0749 vberom.asm:357
    test ah, 002h                             ; f6 c4 02                    ; 0xc074b vberom.asm:359
    je short 00752h                           ; 74 02                       ; 0xc074e vberom.asm:360
    or AL, strict byte 040h                   ; 0c 40                       ; 0xc0750 vberom.asm:361
    out DX, AL                                ; ee                          ; 0xc0752 vberom.asm:363
    mov dx, 003d4h                            ; ba d4 03                    ; 0xc0753 vberom.asm:366
    mov ax, strict word 00009h                ; b8 09 00                    ; 0xc0756 vberom.asm:367
    out DX, AL                                ; ee                          ; 0xc0759 vberom.asm:368
    mov dx, 003d5h                            ; ba d5 03                    ; 0xc075a vberom.asm:369
    in AL, DX                                 ; ec                          ; 0xc075d vberom.asm:370
    and AL, strict byte 060h                  ; 24 60                       ; 0xc075e vberom.asm:371
    out DX, AL                                ; ee                          ; 0xc0760 vberom.asm:372
    mov dx, 003d4h                            ; ba d4 03                    ; 0xc0761 vberom.asm:373
    mov AL, strict byte 017h                  ; b0 17                       ; 0xc0764 vberom.asm:374
    out DX, AL                                ; ee                          ; 0xc0766 vberom.asm:375
    mov dx, 003d5h                            ; ba d5 03                    ; 0xc0767 vberom.asm:376
    in AL, DX                                 ; ec                          ; 0xc076a vberom.asm:377
    or AL, strict byte 003h                   ; 0c 03                       ; 0xc076b vberom.asm:378
    out DX, AL                                ; ee                          ; 0xc076d vberom.asm:379
    mov dx, 003dah                            ; ba da 03                    ; 0xc076e vberom.asm:380
    in AL, DX                                 ; ec                          ; 0xc0771 vberom.asm:381
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0772 vberom.asm:382
    mov AL, strict byte 010h                  ; b0 10                       ; 0xc0775 vberom.asm:383
    out DX, AL                                ; ee                          ; 0xc0777 vberom.asm:384
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc0778 vberom.asm:385
    in AL, DX                                 ; ec                          ; 0xc077b vberom.asm:386
    or AL, strict byte 001h                   ; 0c 01                       ; 0xc077c vberom.asm:387
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc077e vberom.asm:388
    out DX, AL                                ; ee                          ; 0xc0781 vberom.asm:389
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc0782 vberom.asm:390
    out DX, AL                                ; ee                          ; 0xc0784 vberom.asm:391
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc0785 vberom.asm:392
    mov ax, 00506h                            ; b8 06 05                    ; 0xc0788 vberom.asm:393
    out DX, ax                                ; ef                          ; 0xc078b vberom.asm:394
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc078c vberom.asm:395
    mov ax, 00f02h                            ; b8 02 0f                    ; 0xc078f vberom.asm:396
    out DX, ax                                ; ef                          ; 0xc0792 vberom.asm:397
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc0793 vberom.asm:400
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc0796 vberom.asm:401
    call 00590h                               ; e8 f4 fd                    ; 0xc0799 vberom.asm:402
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc079c vberom.asm:403
    call 00597h                               ; e8 f5 fd                    ; 0xc079f vberom.asm:404
    cmp AL, strict byte 008h                  ; 3c 08                       ; 0xc07a2 vberom.asm:405
    jc short 007e6h                           ; 72 40                       ; 0xc07a4 vberom.asm:406
    mov dx, 003d4h                            ; ba d4 03                    ; 0xc07a6 vberom.asm:407
    mov AL, strict byte 014h                  ; b0 14                       ; 0xc07a9 vberom.asm:408
    out DX, AL                                ; ee                          ; 0xc07ab vberom.asm:409
    mov dx, 003d5h                            ; ba d5 03                    ; 0xc07ac vberom.asm:410
    in AL, DX                                 ; ec                          ; 0xc07af vberom.asm:411
    or AL, strict byte 040h                   ; 0c 40                       ; 0xc07b0 vberom.asm:412
    out DX, AL                                ; ee                          ; 0xc07b2 vberom.asm:413
    mov dx, 003dah                            ; ba da 03                    ; 0xc07b3 vberom.asm:414
    in AL, DX                                 ; ec                          ; 0xc07b6 vberom.asm:415
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc07b7 vberom.asm:416
    mov AL, strict byte 010h                  ; b0 10                       ; 0xc07ba vberom.asm:417
    out DX, AL                                ; ee                          ; 0xc07bc vberom.asm:418
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc07bd vberom.asm:419
    in AL, DX                                 ; ec                          ; 0xc07c0 vberom.asm:420
    or AL, strict byte 040h                   ; 0c 40                       ; 0xc07c1 vberom.asm:421
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc07c3 vberom.asm:422
    out DX, AL                                ; ee                          ; 0xc07c6 vberom.asm:423
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc07c7 vberom.asm:424
    out DX, AL                                ; ee                          ; 0xc07c9 vberom.asm:425
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc07ca vberom.asm:426
    mov AL, strict byte 004h                  ; b0 04                       ; 0xc07cd vberom.asm:427
    out DX, AL                                ; ee                          ; 0xc07cf vberom.asm:428
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc07d0 vberom.asm:429
    in AL, DX                                 ; ec                          ; 0xc07d3 vberom.asm:430
    or AL, strict byte 008h                   ; 0c 08                       ; 0xc07d4 vberom.asm:431
    out DX, AL                                ; ee                          ; 0xc07d6 vberom.asm:432
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc07d7 vberom.asm:433
    mov AL, strict byte 005h                  ; b0 05                       ; 0xc07da vberom.asm:434
    out DX, AL                                ; ee                          ; 0xc07dc vberom.asm:435
    mov dx, 003cfh                            ; ba cf 03                    ; 0xc07dd vberom.asm:436
    in AL, DX                                 ; ec                          ; 0xc07e0 vberom.asm:437
    and AL, strict byte 09fh                  ; 24 9f                       ; 0xc07e1 vberom.asm:438
    or AL, strict byte 040h                   ; 0c 40                       ; 0xc07e3 vberom.asm:439
    out DX, AL                                ; ee                          ; 0xc07e5 vberom.asm:440
    pop dx                                    ; 5a                          ; 0xc07e6 vberom.asm:443
    pop ax                                    ; 58                          ; 0xc07e7 vberom.asm:444
  ; disGetNextSymbol 0xc07e8 LB 0x15a -> off=0x0 cb=0000000000000013 uValue=00000000000c07e8 '_vbe_has_vbe_display'
_vbe_has_vbe_display:                        ; 0xc07e8 LB 0x13
    push DS                                   ; 1e                          ; 0xc07e8 vberom.asm:450
    push bx                                   ; 53                          ; 0xc07e9 vberom.asm:451
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc07ea vberom.asm:452
    mov ds, ax                                ; 8e d8                       ; 0xc07ed vberom.asm:453
    mov bx, 000b9h                            ; bb b9 00                    ; 0xc07ef vberom.asm:454
    mov al, byte [bx]                         ; 8a 07                       ; 0xc07f2 vberom.asm:455
    and AL, strict byte 001h                  ; 24 01                       ; 0xc07f4 vberom.asm:456
    db  032h, 0e4h
    ; xor ah, ah                                ; 32 e4                     ; 0xc07f6 vberom.asm:457
    pop bx                                    ; 5b                          ; 0xc07f8 vberom.asm:458
    pop DS                                    ; 1f                          ; 0xc07f9 vberom.asm:459
    retn                                      ; c3                          ; 0xc07fa vberom.asm:460
  ; disGetNextSymbol 0xc07fb LB 0x147 -> off=0x0 cb=0000000000000025 uValue=00000000000c07fb 'vbe_biosfn_return_current_mode'
vbe_biosfn_return_current_mode:              ; 0xc07fb LB 0x25
    push DS                                   ; 1e                          ; 0xc07fb vberom.asm:473
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc07fc vberom.asm:474
    mov ds, ax                                ; 8e d8                       ; 0xc07ff vberom.asm:475
    call 00614h                               ; e8 10 fe                    ; 0xc0801 vberom.asm:476
    and ax, strict byte 00001h                ; 83 e0 01                    ; 0xc0804 vberom.asm:477
    je short 00812h                           ; 74 09                       ; 0xc0807 vberom.asm:478
    mov bx, 000bah                            ; bb ba 00                    ; 0xc0809 vberom.asm:479
    mov ax, word [bx]                         ; 8b 07                       ; 0xc080c vberom.asm:480
    db  08bh, 0d8h
    ; mov bx, ax                                ; 8b d8                     ; 0xc080e vberom.asm:481
    jne short 0081bh                          ; 75 09                       ; 0xc0810 vberom.asm:482
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc0812 vberom.asm:484
    mov al, byte [bx]                         ; 8a 07                       ; 0xc0815 vberom.asm:485
    db  08ah, 0d8h
    ; mov bl, al                                ; 8a d8                     ; 0xc0817 vberom.asm:486
    db  032h, 0ffh
    ; xor bh, bh                                ; 32 ff                     ; 0xc0819 vberom.asm:487
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc081b vberom.asm:489
    pop DS                                    ; 1f                          ; 0xc081e vberom.asm:490
    retn                                      ; c3                          ; 0xc081f vberom.asm:491
  ; disGetNextSymbol 0xc0820 LB 0x122 -> off=0x0 cb=000000000000002d uValue=00000000000c0820 'vbe_biosfn_display_window_control'
vbe_biosfn_display_window_control:           ; 0xc0820 LB 0x2d
    cmp bl, 000h                              ; 80 fb 00                    ; 0xc0820 vberom.asm:515
    jne short 00849h                          ; 75 24                       ; 0xc0823 vberom.asm:516
    cmp bh, 001h                              ; 80 ff 01                    ; 0xc0825 vberom.asm:517
    je short 00840h                           ; 74 16                       ; 0xc0828 vberom.asm:518
    jc short 00830h                           ; 72 04                       ; 0xc082a vberom.asm:519
    mov ax, 00100h                            ; b8 00 01                    ; 0xc082c vberom.asm:520
    retn                                      ; c3                          ; 0xc082f vberom.asm:521
    db  08bh, 0c2h
    ; mov ax, dx                                ; 8b c2                     ; 0xc0830 vberom.asm:523
    call 00626h                               ; e8 f1 fd                    ; 0xc0832 vberom.asm:524
    call 0063ah                               ; e8 02 fe                    ; 0xc0835 vberom.asm:525
    db  03bh, 0c2h
    ; cmp ax, dx                                ; 3b c2                     ; 0xc0838 vberom.asm:526
    jne short 00849h                          ; 75 0d                       ; 0xc083a vberom.asm:527
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc083c vberom.asm:528
    retn                                      ; c3                          ; 0xc083f vberom.asm:529
    call 0063ah                               ; e8 f7 fd                    ; 0xc0840 vberom.asm:531
    db  08bh, 0d0h
    ; mov dx, ax                                ; 8b d0                     ; 0xc0843 vberom.asm:532
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc0845 vberom.asm:533
    retn                                      ; c3                          ; 0xc0848 vberom.asm:534
    mov ax, 0014fh                            ; b8 4f 01                    ; 0xc0849 vberom.asm:536
    retn                                      ; c3                          ; 0xc084c vberom.asm:537
  ; disGetNextSymbol 0xc084d LB 0xf5 -> off=0x0 cb=0000000000000034 uValue=00000000000c084d 'vbe_biosfn_set_get_display_start'
vbe_biosfn_set_get_display_start:            ; 0xc084d LB 0x34
    cmp bl, 080h                              ; 80 fb 80                    ; 0xc084d vberom.asm:577
    je short 0085dh                           ; 74 0b                       ; 0xc0850 vberom.asm:578
    cmp bl, 001h                              ; 80 fb 01                    ; 0xc0852 vberom.asm:579
    je short 00871h                           ; 74 1a                       ; 0xc0855 vberom.asm:580
    jc short 00863h                           ; 72 0a                       ; 0xc0857 vberom.asm:581
    mov ax, 00100h                            ; b8 00 01                    ; 0xc0859 vberom.asm:582
    retn                                      ; c3                          ; 0xc085c vberom.asm:583
    call 005a9h                               ; e8 49 fd                    ; 0xc085d vberom.asm:585
    call 0059ch                               ; e8 39 fd                    ; 0xc0860 vberom.asm:586
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1                     ; 0xc0863 vberom.asm:588
    call 0068ch                               ; e8 24 fe                    ; 0xc0865 vberom.asm:589
    db  08bh, 0c2h
    ; mov ax, dx                                ; 8b c2                     ; 0xc0868 vberom.asm:590
    call 006b2h                               ; e8 45 fe                    ; 0xc086a vberom.asm:591
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc086d vberom.asm:592
    retn                                      ; c3                          ; 0xc0870 vberom.asm:593
    call 006a0h                               ; e8 2c fe                    ; 0xc0871 vberom.asm:595
    db  08bh, 0c8h
    ; mov cx, ax                                ; 8b c8                     ; 0xc0874 vberom.asm:596
    call 006c6h                               ; e8 4d fe                    ; 0xc0876 vberom.asm:597
    db  08bh, 0d0h
    ; mov dx, ax                                ; 8b d0                     ; 0xc0879 vberom.asm:598
    db  032h, 0ffh
    ; xor bh, bh                                ; 32 ff                     ; 0xc087b vberom.asm:599
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc087d vberom.asm:600
    retn                                      ; c3                          ; 0xc0880 vberom.asm:601
  ; disGetNextSymbol 0xc0881 LB 0xc1 -> off=0x0 cb=0000000000000037 uValue=00000000000c0881 'vbe_biosfn_set_get_dac_palette_format'
vbe_biosfn_set_get_dac_palette_format:       ; 0xc0881 LB 0x37
    cmp bl, 001h                              ; 80 fb 01                    ; 0xc0881 vberom.asm:616
    je short 008a4h                           ; 74 1e                       ; 0xc0884 vberom.asm:617
    jc short 0088ch                           ; 72 04                       ; 0xc0886 vberom.asm:618
    mov ax, 00100h                            ; b8 00 01                    ; 0xc0888 vberom.asm:619
    retn                                      ; c3                          ; 0xc088b vberom.asm:620
    call 00614h                               ; e8 85 fd                    ; 0xc088c vberom.asm:622
    cmp bh, 006h                              ; 80 ff 06                    ; 0xc088f vberom.asm:623
    je short 0089eh                           ; 74 0a                       ; 0xc0892 vberom.asm:624
    cmp bh, 008h                              ; 80 ff 08                    ; 0xc0894 vberom.asm:625
    jne short 008b4h                          ; 75 1b                       ; 0xc0897 vberom.asm:626
    or ax, strict byte 00020h                 ; 83 c8 20                    ; 0xc0899 vberom.asm:627
    jne short 008a1h                          ; 75 03                       ; 0xc089c vberom.asm:628
    and ax, strict byte 0ffdfh                ; 83 e0 df                    ; 0xc089e vberom.asm:630
    call 00600h                               ; e8 5c fd                    ; 0xc08a1 vberom.asm:632
    mov BH, strict byte 006h                  ; b7 06                       ; 0xc08a4 vberom.asm:634
    call 00614h                               ; e8 6b fd                    ; 0xc08a6 vberom.asm:635
    and ax, strict byte 00020h                ; 83 e0 20                    ; 0xc08a9 vberom.asm:636
    je short 008b0h                           ; 74 02                       ; 0xc08ac vberom.asm:637
    mov BH, strict byte 008h                  ; b7 08                       ; 0xc08ae vberom.asm:638
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc08b0 vberom.asm:640
    retn                                      ; c3                          ; 0xc08b3 vberom.asm:641
    mov ax, 0014fh                            ; b8 4f 01                    ; 0xc08b4 vberom.asm:643
    retn                                      ; c3                          ; 0xc08b7 vberom.asm:644
  ; disGetNextSymbol 0xc08b8 LB 0x8a -> off=0x0 cb=0000000000000073 uValue=00000000000c08b8 'vbe_biosfn_set_get_palette_data'
vbe_biosfn_set_get_palette_data:             ; 0xc08b8 LB 0x73
    test bl, bl                               ; 84 db                       ; 0xc08b8 vberom.asm:683
    je short 008cbh                           ; 74 0f                       ; 0xc08ba vberom.asm:684
    cmp bl, 001h                              ; 80 fb 01                    ; 0xc08bc vberom.asm:685
    je short 008f9h                           ; 74 38                       ; 0xc08bf vberom.asm:686
    cmp bl, 003h                              ; 80 fb 03                    ; 0xc08c1 vberom.asm:687
    jbe short 00927h                          ; 76 61                       ; 0xc08c4 vberom.asm:688
    cmp bl, 080h                              ; 80 fb 80                    ; 0xc08c6 vberom.asm:689
    jne short 00923h                          ; 75 58                       ; 0xc08c9 vberom.asm:690
    push ax                                   ; 50                          ; 0xc08cb vberom.asm:135
    push cx                                   ; 51                          ; 0xc08cc vberom.asm:136
    push dx                                   ; 52                          ; 0xc08cd vberom.asm:137
    push bx                                   ; 53                          ; 0xc08ce vberom.asm:138
    push sp                                   ; 54                          ; 0xc08cf vberom.asm:139
    push bp                                   ; 55                          ; 0xc08d0 vberom.asm:140
    push si                                   ; 56                          ; 0xc08d1 vberom.asm:141
    push di                                   ; 57                          ; 0xc08d2 vberom.asm:142
    push DS                                   ; 1e                          ; 0xc08d3 vberom.asm:696
    push ES                                   ; 06                          ; 0xc08d4 vberom.asm:697
    pop DS                                    ; 1f                          ; 0xc08d5 vberom.asm:698
    db  08ah, 0c2h
    ; mov al, dl                                ; 8a c2                     ; 0xc08d6 vberom.asm:699
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc08d8 vberom.asm:700
    out DX, AL                                ; ee                          ; 0xc08db vberom.asm:701
    inc dx                                    ; 42                          ; 0xc08dc vberom.asm:702
    db  08bh, 0f7h
    ; mov si, di                                ; 8b f7                     ; 0xc08dd vberom.asm:703
    lodsw                                     ; ad                          ; 0xc08df vberom.asm:714
    db  08bh, 0d8h
    ; mov bx, ax                                ; 8b d8                     ; 0xc08e0 vberom.asm:715
    lodsw                                     ; ad                          ; 0xc08e2 vberom.asm:716
    out DX, AL                                ; ee                          ; 0xc08e3 vberom.asm:717
    db  08ah, 0c7h
    ; mov al, bh                                ; 8a c7                     ; 0xc08e4 vberom.asm:718
    out DX, AL                                ; ee                          ; 0xc08e6 vberom.asm:719
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc08e7 vberom.asm:720
    out DX, AL                                ; ee                          ; 0xc08e9 vberom.asm:721
    loop 008dfh                               ; e2 f3                       ; 0xc08ea vberom.asm:723
    pop DS                                    ; 1f                          ; 0xc08ec vberom.asm:724
    pop di                                    ; 5f                          ; 0xc08ed vberom.asm:154
    pop si                                    ; 5e                          ; 0xc08ee vberom.asm:155
    pop bp                                    ; 5d                          ; 0xc08ef vberom.asm:156
    pop bx                                    ; 5b                          ; 0xc08f0 vberom.asm:157
    pop bx                                    ; 5b                          ; 0xc08f1 vberom.asm:158
    pop dx                                    ; 5a                          ; 0xc08f2 vberom.asm:159
    pop cx                                    ; 59                          ; 0xc08f3 vberom.asm:160
    pop ax                                    ; 58                          ; 0xc08f4 vberom.asm:161
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc08f5 vberom.asm:727
    retn                                      ; c3                          ; 0xc08f8 vberom.asm:728
    push ax                                   ; 50                          ; 0xc08f9 vberom.asm:135
    push cx                                   ; 51                          ; 0xc08fa vberom.asm:136
    push dx                                   ; 52                          ; 0xc08fb vberom.asm:137
    push bx                                   ; 53                          ; 0xc08fc vberom.asm:138
    push sp                                   ; 54                          ; 0xc08fd vberom.asm:139
    push bp                                   ; 55                          ; 0xc08fe vberom.asm:140
    push si                                   ; 56                          ; 0xc08ff vberom.asm:141
    push di                                   ; 57                          ; 0xc0900 vberom.asm:142
    db  08ah, 0c2h
    ; mov al, dl                                ; 8a c2                     ; 0xc0901 vberom.asm:732
    mov dx, 003c7h                            ; ba c7 03                    ; 0xc0903 vberom.asm:733
    out DX, AL                                ; ee                          ; 0xc0906 vberom.asm:734
    add dl, 002h                              ; 80 c2 02                    ; 0xc0907 vberom.asm:735
    db  033h, 0dbh
    ; xor bx, bx                                ; 33 db                     ; 0xc090a vberom.asm:746
    in AL, DX                                 ; ec                          ; 0xc090c vberom.asm:748
    db  08ah, 0d8h
    ; mov bl, al                                ; 8a d8                     ; 0xc090d vberom.asm:749
    in AL, DX                                 ; ec                          ; 0xc090f vberom.asm:750
    db  08ah, 0e0h
    ; mov ah, al                                ; 8a e0                     ; 0xc0910 vberom.asm:751
    in AL, DX                                 ; ec                          ; 0xc0912 vberom.asm:752
    stosw                                     ; ab                          ; 0xc0913 vberom.asm:753
    db  08bh, 0c3h
    ; mov ax, bx                                ; 8b c3                     ; 0xc0914 vberom.asm:754
    stosw                                     ; ab                          ; 0xc0916 vberom.asm:755
    loop 0090ch                               ; e2 f3                       ; 0xc0917 vberom.asm:757
    pop di                                    ; 5f                          ; 0xc0919 vberom.asm:154
    pop si                                    ; 5e                          ; 0xc091a vberom.asm:155
    pop bp                                    ; 5d                          ; 0xc091b vberom.asm:156
    pop bx                                    ; 5b                          ; 0xc091c vberom.asm:157
    pop bx                                    ; 5b                          ; 0xc091d vberom.asm:158
    pop dx                                    ; 5a                          ; 0xc091e vberom.asm:159
    pop cx                                    ; 59                          ; 0xc091f vberom.asm:160
    pop ax                                    ; 58                          ; 0xc0920 vberom.asm:161
    jmp short 008f5h                          ; eb d2                       ; 0xc0921 vberom.asm:759
    mov ax, 0014fh                            ; b8 4f 01                    ; 0xc0923 vberom.asm:762
    retn                                      ; c3                          ; 0xc0926 vberom.asm:763
    mov ax, 0024fh                            ; b8 4f 02                    ; 0xc0927 vberom.asm:765
    retn                                      ; c3                          ; 0xc092a vberom.asm:766
  ; disGetNextSymbol 0xc092b LB 0x17 -> off=0x0 cb=0000000000000017 uValue=00000000000c092b 'vbe_biosfn_return_protected_mode_interface'
vbe_biosfn_return_protected_mode_interface: ; 0xc092b LB 0x17
    test bl, bl                               ; 84 db                       ; 0xc092b vberom.asm:780
    jne short 0093eh                          ; 75 0f                       ; 0xc092d vberom.asm:781
    mov di, 0c000h                            ; bf 00 c0                    ; 0xc092f vberom.asm:782
    mov es, di                                ; 8e c7                       ; 0xc0932 vberom.asm:783
    mov di, 04600h                            ; bf 00 46                    ; 0xc0934 vberom.asm:784
    mov cx, 00115h                            ; b9 15 01                    ; 0xc0937 vberom.asm:785
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc093a vberom.asm:786
    retn                                      ; c3                          ; 0xc093d vberom.asm:787
    mov ax, 0014fh                            ; b8 4f 01                    ; 0xc093e vberom.asm:789
    retn                                      ; c3                          ; 0xc0941 vberom.asm:790

  ; Padding 0x3e bytes at 0xc0942
  times 62 db 0

section _TEXT progbits vstart=0x980 align=1 ; size=0x3929 class=CODE group=AUTO
  ; disGetNextSymbol 0xc0980 LB 0x3929 -> off=0x0 cb=000000000000001c uValue=00000000000c0980 'set_int_vector'
set_int_vector:                              ; 0xc0980 LB 0x1c
    push bx                                   ; 53                          ; 0xc0980 vgabios.c:87
    push bp                                   ; 55                          ; 0xc0981
    mov bp, sp                                ; 89 e5                       ; 0xc0982
    mov bl, al                                ; 88 c3                       ; 0xc0984
    xor bh, bh                                ; 30 ff                       ; 0xc0986 vgabios.c:91
    sal bx, 1                                 ; d1 e3                       ; 0xc0988
    sal bx, 1                                 ; d1 e3                       ; 0xc098a
    xor ax, ax                                ; 31 c0                       ; 0xc098c
    mov es, ax                                ; 8e c0                       ; 0xc098e
    mov word [es:bx], dx                      ; 26 89 17                    ; 0xc0990
    mov word [es:bx+002h], 0c000h             ; 26 c7 47 02 00 c0           ; 0xc0993
    pop bp                                    ; 5d                          ; 0xc0999 vgabios.c:92
    pop bx                                    ; 5b                          ; 0xc099a
    retn                                      ; c3                          ; 0xc099b
  ; disGetNextSymbol 0xc099c LB 0x390d -> off=0x0 cb=000000000000001c uValue=00000000000c099c 'init_vga_card'
init_vga_card:                               ; 0xc099c LB 0x1c
    push bp                                   ; 55                          ; 0xc099c vgabios.c:143
    mov bp, sp                                ; 89 e5                       ; 0xc099d
    push dx                                   ; 52                          ; 0xc099f
    mov AL, strict byte 0c3h                  ; b0 c3                       ; 0xc09a0 vgabios.c:146
    mov dx, 003c2h                            ; ba c2 03                    ; 0xc09a2
    out DX, AL                                ; ee                          ; 0xc09a5
    mov AL, strict byte 004h                  ; b0 04                       ; 0xc09a6 vgabios.c:149
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc09a8
    out DX, AL                                ; ee                          ; 0xc09ab
    mov AL, strict byte 002h                  ; b0 02                       ; 0xc09ac vgabios.c:150
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc09ae
    out DX, AL                                ; ee                          ; 0xc09b1
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc09b2 vgabios.c:155
    pop dx                                    ; 5a                          ; 0xc09b5
    pop bp                                    ; 5d                          ; 0xc09b6
    retn                                      ; c3                          ; 0xc09b7
  ; disGetNextSymbol 0xc09b8 LB 0x38f1 -> off=0x0 cb=0000000000000032 uValue=00000000000c09b8 'init_bios_area'
init_bios_area:                              ; 0xc09b8 LB 0x32
    push bx                                   ; 53                          ; 0xc09b8 vgabios.c:164
    push bp                                   ; 55                          ; 0xc09b9
    mov bp, sp                                ; 89 e5                       ; 0xc09ba
    xor bx, bx                                ; 31 db                       ; 0xc09bc vgabios.c:168
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc09be
    mov es, ax                                ; 8e c0                       ; 0xc09c1
    mov al, byte [es:bx+010h]                 ; 26 8a 47 10                 ; 0xc09c3 vgabios.c:171
    and AL, strict byte 0cfh                  ; 24 cf                       ; 0xc09c7
    or AL, strict byte 020h                   ; 0c 20                       ; 0xc09c9
    mov byte [es:bx+010h], al                 ; 26 88 47 10                 ; 0xc09cb
    mov byte [es:bx+00085h], 010h             ; 26 c6 87 85 00 10           ; 0xc09cf vgabios.c:175
    mov word [es:bx+00087h], 0f960h           ; 26 c7 87 87 00 60 f9        ; 0xc09d5 vgabios.c:177
    mov byte [es:bx+00089h], 051h             ; 26 c6 87 89 00 51           ; 0xc09dc vgabios.c:181
    mov byte [es:bx+065h], 009h               ; 26 c6 47 65 09              ; 0xc09e2 vgabios.c:183
    pop bp                                    ; 5d                          ; 0xc09e7 vgabios.c:184
    pop bx                                    ; 5b                          ; 0xc09e8
    retn                                      ; c3                          ; 0xc09e9
  ; disGetNextSymbol 0xc09ea LB 0x38bf -> off=0x0 cb=0000000000000022 uValue=00000000000c09ea 'vgabios_init_func'
vgabios_init_func:                           ; 0xc09ea LB 0x22
    inc bp                                    ; 45                          ; 0xc09ea vgabios.c:224
    push bp                                   ; 55                          ; 0xc09eb
    mov bp, sp                                ; 89 e5                       ; 0xc09ec
    call 0099ch                               ; e8 ab ff                    ; 0xc09ee vgabios.c:226
    call 009b8h                               ; e8 c4 ff                    ; 0xc09f1 vgabios.c:227
    call 03c30h                               ; e8 39 32                    ; 0xc09f4 vgabios.c:229
    mov dx, strict word 00022h                ; ba 22 00                    ; 0xc09f7 vgabios.c:231
    mov ax, strict word 00010h                ; b8 10 00                    ; 0xc09fa
    call 00980h                               ; e8 80 ff                    ; 0xc09fd
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc0a00 vgabios.c:257
    db  032h, 0e4h
    ; xor ah, ah                                ; 32 e4                     ; 0xc0a03
    int 010h                                  ; cd 10                       ; 0xc0a05
    mov sp, bp                                ; 89 ec                       ; 0xc0a07 vgabios.c:260
    pop bp                                    ; 5d                          ; 0xc0a09
    dec bp                                    ; 4d                          ; 0xc0a0a
    retf                                      ; cb                          ; 0xc0a0b
  ; disGetNextSymbol 0xc0a0c LB 0x389d -> off=0x0 cb=0000000000000040 uValue=00000000000c0a0c 'vga_get_cursor_pos'
vga_get_cursor_pos:                          ; 0xc0a0c LB 0x40
    push si                                   ; 56                          ; 0xc0a0c vgabios.c:329
    push di                                   ; 57                          ; 0xc0a0d
    push bp                                   ; 55                          ; 0xc0a0e
    mov bp, sp                                ; 89 e5                       ; 0xc0a0f
    mov si, dx                                ; 89 d6                       ; 0xc0a11
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc0a13 vgabios.c:331
    jbe short 00a25h                          ; 76 0e                       ; 0xc0a15
    push SS                                   ; 16                          ; 0xc0a17 vgabios.c:332
    pop ES                                    ; 07                          ; 0xc0a18
    mov word [es:si], strict word 00000h      ; 26 c7 04 00 00              ; 0xc0a19
    mov word [es:bx], strict word 00000h      ; 26 c7 07 00 00              ; 0xc0a1e vgabios.c:333
    jmp short 00a48h                          ; eb 23                       ; 0xc0a23 vgabios.c:334
    mov di, strict word 00060h                ; bf 60 00                    ; 0xc0a25 vgabios.c:47
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc0a28
    mov es, dx                                ; 8e c2                       ; 0xc0a2b
    mov di, word [es:di]                      ; 26 8b 3d                    ; 0xc0a2d
    push SS                                   ; 16                          ; 0xc0a30 vgabios.c:48
    pop ES                                    ; 07                          ; 0xc0a31
    mov word [es:si], di                      ; 26 89 3c                    ; 0xc0a32
    xor ah, ah                                ; 30 e4                       ; 0xc0a35 vgabios.c:337
    mov si, ax                                ; 89 c6                       ; 0xc0a37
    sal si, 1                                 ; d1 e6                       ; 0xc0a39
    add si, strict byte 00050h                ; 83 c6 50                    ; 0xc0a3b
    mov es, dx                                ; 8e c2                       ; 0xc0a3e vgabios.c:47
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc0a40
    push SS                                   ; 16                          ; 0xc0a43 vgabios.c:48
    pop ES                                    ; 07                          ; 0xc0a44
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc0a45
    pop bp                                    ; 5d                          ; 0xc0a48 vgabios.c:339
    pop di                                    ; 5f                          ; 0xc0a49
    pop si                                    ; 5e                          ; 0xc0a4a
    retn                                      ; c3                          ; 0xc0a4b
  ; disGetNextSymbol 0xc0a4c LB 0x385d -> off=0x0 cb=000000000000005e uValue=00000000000c0a4c 'vga_find_glyph'
vga_find_glyph:                              ; 0xc0a4c LB 0x5e
    push bp                                   ; 55                          ; 0xc0a4c vgabios.c:342
    mov bp, sp                                ; 89 e5                       ; 0xc0a4d
    push si                                   ; 56                          ; 0xc0a4f
    push di                                   ; 57                          ; 0xc0a50
    push ax                                   ; 50                          ; 0xc0a51
    push ax                                   ; 50                          ; 0xc0a52
    push dx                                   ; 52                          ; 0xc0a53
    push bx                                   ; 53                          ; 0xc0a54
    mov bl, cl                                ; 88 cb                       ; 0xc0a55
    mov word [bp-006h], strict word 00000h    ; c7 46 fa 00 00              ; 0xc0a57 vgabios.c:344
    dec word [bp+004h]                        ; ff 4e 04                    ; 0xc0a5c vgabios.c:346
    cmp word [bp+004h], strict byte 0ffffh    ; 83 7e 04 ff                 ; 0xc0a5f
    je short 00a9eh                           ; 74 39                       ; 0xc0a63
    mov cl, byte [bp+006h]                    ; 8a 4e 06                    ; 0xc0a65 vgabios.c:347
    xor ch, ch                                ; 30 ed                       ; 0xc0a68
    mov dx, ss                                ; 8c d2                       ; 0xc0a6a
    mov si, word [bp-00ch]                    ; 8b 76 f4                    ; 0xc0a6c
    mov di, word [bp-008h]                    ; 8b 7e f8                    ; 0xc0a6f
    mov es, [bp-00ah]                         ; 8e 46 f6                    ; 0xc0a72
    push DS                                   ; 1e                          ; 0xc0a75
    mov ds, dx                                ; 8e da                       ; 0xc0a76
    rep cmpsb                                 ; f3 a6                       ; 0xc0a78
    pop DS                                    ; 1f                          ; 0xc0a7a
    mov ax, strict word 00000h                ; b8 00 00                    ; 0xc0a7b
    je short 00a82h                           ; 74 02                       ; 0xc0a7e
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc0a80
    test ax, ax                               ; 85 c0                       ; 0xc0a82
    jne short 00a92h                          ; 75 0c                       ; 0xc0a84
    mov al, bl                                ; 88 d8                       ; 0xc0a86 vgabios.c:348
    xor ah, ah                                ; 30 e4                       ; 0xc0a88
    or ah, 080h                               ; 80 cc 80                    ; 0xc0a8a
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc0a8d
    jmp short 00a9eh                          ; eb 0c                       ; 0xc0a90 vgabios.c:349
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc0a92 vgabios.c:351
    xor ah, ah                                ; 30 e4                       ; 0xc0a95
    add word [bp-008h], ax                    ; 01 46 f8                    ; 0xc0a97
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3                     ; 0xc0a9a vgabios.c:352
    jmp short 00a5ch                          ; eb be                       ; 0xc0a9c vgabios.c:353
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc0a9e vgabios.c:355
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0aa1
    pop di                                    ; 5f                          ; 0xc0aa4
    pop si                                    ; 5e                          ; 0xc0aa5
    pop bp                                    ; 5d                          ; 0xc0aa6
    retn 00004h                               ; c2 04 00                    ; 0xc0aa7
  ; disGetNextSymbol 0xc0aaa LB 0x37ff -> off=0x0 cb=0000000000000046 uValue=00000000000c0aaa 'vga_read_glyph_planar'
vga_read_glyph_planar:                       ; 0xc0aaa LB 0x46
    push bp                                   ; 55                          ; 0xc0aaa vgabios.c:357
    mov bp, sp                                ; 89 e5                       ; 0xc0aab
    push si                                   ; 56                          ; 0xc0aad
    push di                                   ; 57                          ; 0xc0aae
    push ax                                   ; 50                          ; 0xc0aaf
    push ax                                   ; 50                          ; 0xc0ab0
    mov si, ax                                ; 89 c6                       ; 0xc0ab1
    mov word [bp-006h], dx                    ; 89 56 fa                    ; 0xc0ab3
    mov word [bp-008h], bx                    ; 89 5e f8                    ; 0xc0ab6
    mov bx, cx                                ; 89 cb                       ; 0xc0ab9
    mov ax, 00805h                            ; b8 05 08                    ; 0xc0abb vgabios.c:364
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc0abe
    out DX, ax                                ; ef                          ; 0xc0ac1
    dec byte [bp+004h]                        ; fe 4e 04                    ; 0xc0ac2 vgabios.c:366
    cmp byte [bp+004h], 0ffh                  ; 80 7e 04 ff                 ; 0xc0ac5
    je short 00ae0h                           ; 74 15                       ; 0xc0ac9
    mov es, [bp-006h]                         ; 8e 46 fa                    ; 0xc0acb vgabios.c:367
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc0ace
    not al                                    ; f6 d0                       ; 0xc0ad1
    mov di, bx                                ; 89 df                       ; 0xc0ad3
    inc bx                                    ; 43                          ; 0xc0ad5
    push SS                                   ; 16                          ; 0xc0ad6
    pop ES                                    ; 07                          ; 0xc0ad7
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc0ad8
    add si, word [bp-008h]                    ; 03 76 f8                    ; 0xc0adb vgabios.c:368
    jmp short 00ac2h                          ; eb e2                       ; 0xc0ade vgabios.c:369
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc0ae0 vgabios.c:372
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc0ae3
    out DX, ax                                ; ef                          ; 0xc0ae6
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0ae7 vgabios.c:373
    pop di                                    ; 5f                          ; 0xc0aea
    pop si                                    ; 5e                          ; 0xc0aeb
    pop bp                                    ; 5d                          ; 0xc0aec
    retn 00002h                               ; c2 02 00                    ; 0xc0aed
  ; disGetNextSymbol 0xc0af0 LB 0x37b9 -> off=0x0 cb=000000000000002f uValue=00000000000c0af0 'vga_char_ofs_planar'
vga_char_ofs_planar:                         ; 0xc0af0 LB 0x2f
    push si                                   ; 56                          ; 0xc0af0 vgabios.c:375
    push bp                                   ; 55                          ; 0xc0af1
    mov bp, sp                                ; 89 e5                       ; 0xc0af2
    mov ch, al                                ; 88 c5                       ; 0xc0af4
    mov al, dl                                ; 88 d0                       ; 0xc0af6
    xor ah, ah                                ; 30 e4                       ; 0xc0af8 vgabios.c:379
    mul bx                                    ; f7 e3                       ; 0xc0afa
    mov bl, byte [bp+006h]                    ; 8a 5e 06                    ; 0xc0afc
    xor bh, bh                                ; 30 ff                       ; 0xc0aff
    mul bx                                    ; f7 e3                       ; 0xc0b01
    mov bl, ch                                ; 88 eb                       ; 0xc0b03
    add bx, ax                                ; 01 c3                       ; 0xc0b05
    mov si, strict word 0004ch                ; be 4c 00                    ; 0xc0b07 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0b0a
    mov es, ax                                ; 8e c0                       ; 0xc0b0d
    mov si, word [es:si]                      ; 26 8b 34                    ; 0xc0b0f
    mov al, cl                                ; 88 c8                       ; 0xc0b12 vgabios.c:48
    xor ah, ah                                ; 30 e4                       ; 0xc0b14
    mul si                                    ; f7 e6                       ; 0xc0b16
    add ax, bx                                ; 01 d8                       ; 0xc0b18
    pop bp                                    ; 5d                          ; 0xc0b1a vgabios.c:383
    pop si                                    ; 5e                          ; 0xc0b1b
    retn 00002h                               ; c2 02 00                    ; 0xc0b1c
  ; disGetNextSymbol 0xc0b1f LB 0x378a -> off=0x0 cb=0000000000000045 uValue=00000000000c0b1f 'vga_read_char_planar'
vga_read_char_planar:                        ; 0xc0b1f LB 0x45
    push bp                                   ; 55                          ; 0xc0b1f vgabios.c:385
    mov bp, sp                                ; 89 e5                       ; 0xc0b20
    push cx                                   ; 51                          ; 0xc0b22
    push si                                   ; 56                          ; 0xc0b23
    sub sp, strict byte 00012h                ; 83 ec 12                    ; 0xc0b24
    mov si, ax                                ; 89 c6                       ; 0xc0b27
    mov ax, dx                                ; 89 d0                       ; 0xc0b29
    mov byte [bp-006h], bl                    ; 88 5e fa                    ; 0xc0b2b vgabios.c:389
    mov byte [bp-005h], 000h                  ; c6 46 fb 00                 ; 0xc0b2e
    push word [bp-006h]                       ; ff 76 fa                    ; 0xc0b32
    lea cx, [bp-016h]                         ; 8d 4e ea                    ; 0xc0b35
    mov bx, si                                ; 89 f3                       ; 0xc0b38
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc0b3a
    call 00aaah                               ; e8 6a ff                    ; 0xc0b3d
    push word [bp-006h]                       ; ff 76 fa                    ; 0xc0b40 vgabios.c:392
    mov ax, 00100h                            ; b8 00 01                    ; 0xc0b43
    push ax                                   ; 50                          ; 0xc0b46
    mov bx, 0010ch                            ; bb 0c 01                    ; 0xc0b47 vgabios.c:57
    xor ax, ax                                ; 31 c0                       ; 0xc0b4a
    mov es, ax                                ; 8e c0                       ; 0xc0b4c
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0b4e
    mov dx, word [es:bx+002h]                 ; 26 8b 57 02                 ; 0xc0b51
    xor cx, cx                                ; 31 c9                       ; 0xc0b55 vgabios.c:58
    lea bx, [bp-016h]                         ; 8d 5e ea                    ; 0xc0b57
    call 00a4ch                               ; e8 ef fe                    ; 0xc0b5a
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0b5d vgabios.c:393
    pop si                                    ; 5e                          ; 0xc0b60
    pop cx                                    ; 59                          ; 0xc0b61
    pop bp                                    ; 5d                          ; 0xc0b62
    retn                                      ; c3                          ; 0xc0b63
  ; disGetNextSymbol 0xc0b64 LB 0x3745 -> off=0x0 cb=0000000000000027 uValue=00000000000c0b64 'vga_char_ofs_linear'
vga_char_ofs_linear:                         ; 0xc0b64 LB 0x27
    push bp                                   ; 55                          ; 0xc0b64 vgabios.c:395
    mov bp, sp                                ; 89 e5                       ; 0xc0b65
    push ax                                   ; 50                          ; 0xc0b67
    mov byte [bp-002h], al                    ; 88 46 fe                    ; 0xc0b68
    mov al, dl                                ; 88 d0                       ; 0xc0b6b vgabios.c:399
    xor ah, ah                                ; 30 e4                       ; 0xc0b6d
    mul bx                                    ; f7 e3                       ; 0xc0b6f
    mov dl, byte [bp+004h]                    ; 8a 56 04                    ; 0xc0b71
    xor dh, dh                                ; 30 f6                       ; 0xc0b74
    mul dx                                    ; f7 e2                       ; 0xc0b76
    mov dx, ax                                ; 89 c2                       ; 0xc0b78
    mov al, byte [bp-002h]                    ; 8a 46 fe                    ; 0xc0b7a
    xor ah, ah                                ; 30 e4                       ; 0xc0b7d
    add ax, dx                                ; 01 d0                       ; 0xc0b7f
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc0b81 vgabios.c:400
    sal ax, CL                                ; d3 e0                       ; 0xc0b83
    mov sp, bp                                ; 89 ec                       ; 0xc0b85 vgabios.c:402
    pop bp                                    ; 5d                          ; 0xc0b87
    retn 00002h                               ; c2 02 00                    ; 0xc0b88
  ; disGetNextSymbol 0xc0b8b LB 0x371e -> off=0x0 cb=000000000000004e uValue=00000000000c0b8b 'vga_read_glyph_linear'
vga_read_glyph_linear:                       ; 0xc0b8b LB 0x4e
    push si                                   ; 56                          ; 0xc0b8b vgabios.c:404
    push di                                   ; 57                          ; 0xc0b8c
    push bp                                   ; 55                          ; 0xc0b8d
    mov bp, sp                                ; 89 e5                       ; 0xc0b8e
    push ax                                   ; 50                          ; 0xc0b90
    push ax                                   ; 50                          ; 0xc0b91
    mov si, ax                                ; 89 c6                       ; 0xc0b92
    mov word [bp-002h], dx                    ; 89 56 fe                    ; 0xc0b94
    mov word [bp-004h], bx                    ; 89 5e fc                    ; 0xc0b97
    mov bx, cx                                ; 89 cb                       ; 0xc0b9a
    dec byte [bp+008h]                        ; fe 4e 08                    ; 0xc0b9c vgabios.c:410
    cmp byte [bp+008h], 0ffh                  ; 80 7e 08 ff                 ; 0xc0b9f
    je short 00bd1h                           ; 74 2c                       ; 0xc0ba3
    xor dh, dh                                ; 30 f6                       ; 0xc0ba5 vgabios.c:411
    mov DL, strict byte 080h                  ; b2 80                       ; 0xc0ba7 vgabios.c:412
    xor ax, ax                                ; 31 c0                       ; 0xc0ba9 vgabios.c:413
    jmp short 00bb2h                          ; eb 05                       ; 0xc0bab
    cmp ax, strict word 00008h                ; 3d 08 00                    ; 0xc0bad
    jnl short 00bc6h                          ; 7d 14                       ; 0xc0bb0
    mov es, [bp-002h]                         ; 8e 46 fe                    ; 0xc0bb2 vgabios.c:414
    mov di, si                                ; 89 f7                       ; 0xc0bb5
    add di, ax                                ; 01 c7                       ; 0xc0bb7
    cmp byte [es:di], 000h                    ; 26 80 3d 00                 ; 0xc0bb9
    je short 00bc1h                           ; 74 02                       ; 0xc0bbd
    or dh, dl                                 ; 08 d6                       ; 0xc0bbf vgabios.c:415
    shr dl, 1                                 ; d0 ea                       ; 0xc0bc1 vgabios.c:416
    inc ax                                    ; 40                          ; 0xc0bc3 vgabios.c:417
    jmp short 00badh                          ; eb e7                       ; 0xc0bc4
    mov di, bx                                ; 89 df                       ; 0xc0bc6 vgabios.c:418
    inc bx                                    ; 43                          ; 0xc0bc8
    mov byte [ss:di], dh                      ; 36 88 35                    ; 0xc0bc9
    add si, word [bp-004h]                    ; 03 76 fc                    ; 0xc0bcc vgabios.c:419
    jmp short 00b9ch                          ; eb cb                       ; 0xc0bcf vgabios.c:420
    mov sp, bp                                ; 89 ec                       ; 0xc0bd1 vgabios.c:421
    pop bp                                    ; 5d                          ; 0xc0bd3
    pop di                                    ; 5f                          ; 0xc0bd4
    pop si                                    ; 5e                          ; 0xc0bd5
    retn 00002h                               ; c2 02 00                    ; 0xc0bd6
  ; disGetNextSymbol 0xc0bd9 LB 0x36d0 -> off=0x0 cb=0000000000000049 uValue=00000000000c0bd9 'vga_read_char_linear'
vga_read_char_linear:                        ; 0xc0bd9 LB 0x49
    push bp                                   ; 55                          ; 0xc0bd9 vgabios.c:423
    mov bp, sp                                ; 89 e5                       ; 0xc0bda
    push cx                                   ; 51                          ; 0xc0bdc
    push si                                   ; 56                          ; 0xc0bdd
    sub sp, strict byte 00012h                ; 83 ec 12                    ; 0xc0bde
    mov si, ax                                ; 89 c6                       ; 0xc0be1
    mov ax, dx                                ; 89 d0                       ; 0xc0be3
    mov byte [bp-006h], bl                    ; 88 5e fa                    ; 0xc0be5 vgabios.c:427
    mov byte [bp-005h], 000h                  ; c6 46 fb 00                 ; 0xc0be8
    push word [bp-006h]                       ; ff 76 fa                    ; 0xc0bec
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc0bef
    mov bx, si                                ; 89 f3                       ; 0xc0bf1
    sal bx, CL                                ; d3 e3                       ; 0xc0bf3
    lea cx, [bp-016h]                         ; 8d 4e ea                    ; 0xc0bf5
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc0bf8
    call 00b8bh                               ; e8 8d ff                    ; 0xc0bfb
    push word [bp-006h]                       ; ff 76 fa                    ; 0xc0bfe vgabios.c:430
    mov ax, 00100h                            ; b8 00 01                    ; 0xc0c01
    push ax                                   ; 50                          ; 0xc0c04
    mov bx, 0010ch                            ; bb 0c 01                    ; 0xc0c05 vgabios.c:57
    xor ax, ax                                ; 31 c0                       ; 0xc0c08
    mov es, ax                                ; 8e c0                       ; 0xc0c0a
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0c0c
    mov dx, word [es:bx+002h]                 ; 26 8b 57 02                 ; 0xc0c0f
    xor cx, cx                                ; 31 c9                       ; 0xc0c13 vgabios.c:58
    lea bx, [bp-016h]                         ; 8d 5e ea                    ; 0xc0c15
    call 00a4ch                               ; e8 31 fe                    ; 0xc0c18
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0c1b vgabios.c:431
    pop si                                    ; 5e                          ; 0xc0c1e
    pop cx                                    ; 59                          ; 0xc0c1f
    pop bp                                    ; 5d                          ; 0xc0c20
    retn                                      ; c3                          ; 0xc0c21
  ; disGetNextSymbol 0xc0c22 LB 0x3687 -> off=0x0 cb=0000000000000036 uValue=00000000000c0c22 'vga_read_2bpp_char'
vga_read_2bpp_char:                          ; 0xc0c22 LB 0x36
    push bp                                   ; 55                          ; 0xc0c22 vgabios.c:433
    mov bp, sp                                ; 89 e5                       ; 0xc0c23
    push bx                                   ; 53                          ; 0xc0c25
    push cx                                   ; 51                          ; 0xc0c26
    mov bx, ax                                ; 89 c3                       ; 0xc0c27
    mov es, dx                                ; 8e c2                       ; 0xc0c29
    mov cx, 0c000h                            ; b9 00 c0                    ; 0xc0c2b vgabios.c:439
    mov DH, strict byte 080h                  ; b6 80                       ; 0xc0c2e vgabios.c:440
    xor dl, dl                                ; 30 d2                       ; 0xc0c30 vgabios.c:441
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0c32 vgabios.c:442
    xchg ah, al                               ; 86 c4                       ; 0xc0c35
    xor bx, bx                                ; 31 db                       ; 0xc0c37 vgabios.c:444
    jmp short 00c40h                          ; eb 05                       ; 0xc0c39
    cmp bx, strict byte 00008h                ; 83 fb 08                    ; 0xc0c3b
    jnl short 00c4fh                          ; 7d 0f                       ; 0xc0c3e
    test ax, cx                               ; 85 c8                       ; 0xc0c40 vgabios.c:445
    je short 00c46h                           ; 74 02                       ; 0xc0c42
    or dl, dh                                 ; 08 f2                       ; 0xc0c44 vgabios.c:446
    shr dh, 1                                 ; d0 ee                       ; 0xc0c46 vgabios.c:447
    shr cx, 1                                 ; d1 e9                       ; 0xc0c48 vgabios.c:448
    shr cx, 1                                 ; d1 e9                       ; 0xc0c4a
    inc bx                                    ; 43                          ; 0xc0c4c vgabios.c:449
    jmp short 00c3bh                          ; eb ec                       ; 0xc0c4d
    mov al, dl                                ; 88 d0                       ; 0xc0c4f vgabios.c:451
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0c51
    pop cx                                    ; 59                          ; 0xc0c54
    pop bx                                    ; 5b                          ; 0xc0c55
    pop bp                                    ; 5d                          ; 0xc0c56
    retn                                      ; c3                          ; 0xc0c57
  ; disGetNextSymbol 0xc0c58 LB 0x3651 -> off=0x0 cb=0000000000000084 uValue=00000000000c0c58 'vga_read_glyph_cga'
vga_read_glyph_cga:                          ; 0xc0c58 LB 0x84
    push bp                                   ; 55                          ; 0xc0c58 vgabios.c:453
    mov bp, sp                                ; 89 e5                       ; 0xc0c59
    push cx                                   ; 51                          ; 0xc0c5b
    push si                                   ; 56                          ; 0xc0c5c
    push di                                   ; 57                          ; 0xc0c5d
    push ax                                   ; 50                          ; 0xc0c5e
    mov si, dx                                ; 89 d6                       ; 0xc0c5f
    cmp bl, 006h                              ; 80 fb 06                    ; 0xc0c61 vgabios.c:461
    je short 00ca0h                           ; 74 3a                       ; 0xc0c64
    mov bx, ax                                ; 89 c3                       ; 0xc0c66 vgabios.c:463
    sal bx, 1                                 ; d1 e3                       ; 0xc0c68
    mov word [bp-008h], 0b800h                ; c7 46 f8 00 b8              ; 0xc0c6a
    xor cx, cx                                ; 31 c9                       ; 0xc0c6f vgabios.c:465
    jmp short 00c78h                          ; eb 05                       ; 0xc0c71
    cmp cx, strict byte 00004h                ; 83 f9 04                    ; 0xc0c73
    jnl short 00cd4h                          ; 7d 5c                       ; 0xc0c76
    mov ax, bx                                ; 89 d8                       ; 0xc0c78 vgabios.c:466
    mov dx, word [bp-008h]                    ; 8b 56 f8                    ; 0xc0c7a
    call 00c22h                               ; e8 a2 ff                    ; 0xc0c7d
    mov di, si                                ; 89 f7                       ; 0xc0c80
    inc si                                    ; 46                          ; 0xc0c82
    push SS                                   ; 16                          ; 0xc0c83
    pop ES                                    ; 07                          ; 0xc0c84
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc0c85
    lea ax, [bx+02000h]                       ; 8d 87 00 20                 ; 0xc0c88 vgabios.c:467
    mov dx, word [bp-008h]                    ; 8b 56 f8                    ; 0xc0c8c
    call 00c22h                               ; e8 90 ff                    ; 0xc0c8f
    mov di, si                                ; 89 f7                       ; 0xc0c92
    inc si                                    ; 46                          ; 0xc0c94
    push SS                                   ; 16                          ; 0xc0c95
    pop ES                                    ; 07                          ; 0xc0c96
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc0c97
    add bx, strict byte 00050h                ; 83 c3 50                    ; 0xc0c9a vgabios.c:468
    inc cx                                    ; 41                          ; 0xc0c9d vgabios.c:469
    jmp short 00c73h                          ; eb d3                       ; 0xc0c9e
    mov bx, ax                                ; 89 c3                       ; 0xc0ca0 vgabios.c:471
    mov word [bp-008h], 0b800h                ; c7 46 f8 00 b8              ; 0xc0ca2
    xor cx, cx                                ; 31 c9                       ; 0xc0ca7 vgabios.c:472
    jmp short 00cb0h                          ; eb 05                       ; 0xc0ca9
    cmp cx, strict byte 00004h                ; 83 f9 04                    ; 0xc0cab
    jnl short 00cd4h                          ; 7d 24                       ; 0xc0cae
    mov di, si                                ; 89 f7                       ; 0xc0cb0 vgabios.c:473
    inc si                                    ; 46                          ; 0xc0cb2
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc0cb3
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0cb6
    push SS                                   ; 16                          ; 0xc0cb9
    pop ES                                    ; 07                          ; 0xc0cba
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc0cbb
    mov di, si                                ; 89 f7                       ; 0xc0cbe vgabios.c:474
    inc si                                    ; 46                          ; 0xc0cc0
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc0cc1
    mov al, byte [es:bx+02000h]               ; 26 8a 87 00 20              ; 0xc0cc4
    push SS                                   ; 16                          ; 0xc0cc9
    pop ES                                    ; 07                          ; 0xc0cca
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc0ccb
    add bx, strict byte 00050h                ; 83 c3 50                    ; 0xc0cce vgabios.c:475
    inc cx                                    ; 41                          ; 0xc0cd1 vgabios.c:476
    jmp short 00cabh                          ; eb d7                       ; 0xc0cd2
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc0cd4 vgabios.c:478
    pop di                                    ; 5f                          ; 0xc0cd7
    pop si                                    ; 5e                          ; 0xc0cd8
    pop cx                                    ; 59                          ; 0xc0cd9
    pop bp                                    ; 5d                          ; 0xc0cda
    retn                                      ; c3                          ; 0xc0cdb
  ; disGetNextSymbol 0xc0cdc LB 0x35cd -> off=0x0 cb=000000000000001b uValue=00000000000c0cdc 'vga_char_ofs_cga'
vga_char_ofs_cga:                            ; 0xc0cdc LB 0x1b
    push cx                                   ; 51                          ; 0xc0cdc vgabios.c:480
    push bp                                   ; 55                          ; 0xc0cdd
    mov bp, sp                                ; 89 e5                       ; 0xc0cde
    mov cl, al                                ; 88 c1                       ; 0xc0ce0
    mov al, dl                                ; 88 d0                       ; 0xc0ce2
    xor ah, ah                                ; 30 e4                       ; 0xc0ce4 vgabios.c:485
    mul bx                                    ; f7 e3                       ; 0xc0ce6
    mov bx, ax                                ; 89 c3                       ; 0xc0ce8
    sal bx, 1                                 ; d1 e3                       ; 0xc0cea
    sal bx, 1                                 ; d1 e3                       ; 0xc0cec
    mov al, cl                                ; 88 c8                       ; 0xc0cee
    xor ah, ah                                ; 30 e4                       ; 0xc0cf0
    add ax, bx                                ; 01 d8                       ; 0xc0cf2
    pop bp                                    ; 5d                          ; 0xc0cf4 vgabios.c:486
    pop cx                                    ; 59                          ; 0xc0cf5
    retn                                      ; c3                          ; 0xc0cf6
  ; disGetNextSymbol 0xc0cf7 LB 0x35b2 -> off=0x0 cb=000000000000006b uValue=00000000000c0cf7 'vga_read_char_cga'
vga_read_char_cga:                           ; 0xc0cf7 LB 0x6b
    push bp                                   ; 55                          ; 0xc0cf7 vgabios.c:488
    mov bp, sp                                ; 89 e5                       ; 0xc0cf8
    push bx                                   ; 53                          ; 0xc0cfa
    push cx                                   ; 51                          ; 0xc0cfb
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc0cfc
    mov bl, dl                                ; 88 d3                       ; 0xc0cff vgabios.c:494
    xor bh, bh                                ; 30 ff                       ; 0xc0d01
    lea dx, [bp-00eh]                         ; 8d 56 f2                    ; 0xc0d03
    call 00c58h                               ; e8 4f ff                    ; 0xc0d06
    mov ax, strict word 00008h                ; b8 08 00                    ; 0xc0d09 vgabios.c:497
    push ax                                   ; 50                          ; 0xc0d0c
    mov ax, 00080h                            ; b8 80 00                    ; 0xc0d0d
    push ax                                   ; 50                          ; 0xc0d10
    mov bx, 0010ch                            ; bb 0c 01                    ; 0xc0d11 vgabios.c:57
    xor ax, ax                                ; 31 c0                       ; 0xc0d14
    mov es, ax                                ; 8e c0                       ; 0xc0d16
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0d18
    mov dx, word [es:bx+002h]                 ; 26 8b 57 02                 ; 0xc0d1b
    xor cx, cx                                ; 31 c9                       ; 0xc0d1f vgabios.c:58
    lea bx, [bp-00eh]                         ; 8d 5e f2                    ; 0xc0d21
    call 00a4ch                               ; e8 25 fd                    ; 0xc0d24
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc0d27
    test ah, 080h                             ; f6 c4 80                    ; 0xc0d2a vgabios.c:499
    jne short 00d58h                          ; 75 29                       ; 0xc0d2d
    mov bx, strict word 0007ch                ; bb 7c 00                    ; 0xc0d2f vgabios.c:57
    xor ax, ax                                ; 31 c0                       ; 0xc0d32
    mov es, ax                                ; 8e c0                       ; 0xc0d34
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0d36
    mov dx, word [es:bx+002h]                 ; 26 8b 57 02                 ; 0xc0d39
    test dx, dx                               ; 85 d2                       ; 0xc0d3d vgabios.c:503
    jne short 00d45h                          ; 75 04                       ; 0xc0d3f
    test ax, ax                               ; 85 c0                       ; 0xc0d41
    je short 00d58h                           ; 74 13                       ; 0xc0d43
    mov bx, strict word 00008h                ; bb 08 00                    ; 0xc0d45 vgabios.c:504
    push bx                                   ; 53                          ; 0xc0d48
    mov bx, 00080h                            ; bb 80 00                    ; 0xc0d49
    push bx                                   ; 53                          ; 0xc0d4c
    mov cx, bx                                ; 89 d9                       ; 0xc0d4d
    lea bx, [bp-00eh]                         ; 8d 5e f2                    ; 0xc0d4f
    call 00a4ch                               ; e8 f7 fc                    ; 0xc0d52
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc0d55
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc0d58 vgabios.c:507
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0d5b
    pop cx                                    ; 59                          ; 0xc0d5e
    pop bx                                    ; 5b                          ; 0xc0d5f
    pop bp                                    ; 5d                          ; 0xc0d60
    retn                                      ; c3                          ; 0xc0d61
  ; disGetNextSymbol 0xc0d62 LB 0x3547 -> off=0x0 cb=0000000000000147 uValue=00000000000c0d62 'vga_read_char_attr'
vga_read_char_attr:                          ; 0xc0d62 LB 0x147
    push bp                                   ; 55                          ; 0xc0d62 vgabios.c:509
    mov bp, sp                                ; 89 e5                       ; 0xc0d63
    push bx                                   ; 53                          ; 0xc0d65
    push cx                                   ; 51                          ; 0xc0d66
    push si                                   ; 56                          ; 0xc0d67
    push di                                   ; 57                          ; 0xc0d68
    sub sp, strict byte 00012h                ; 83 ec 12                    ; 0xc0d69
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc0d6c
    mov si, dx                                ; 89 d6                       ; 0xc0d6f
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc0d71 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0d74
    mov es, ax                                ; 8e c0                       ; 0xc0d77
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0d79
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc0d7c vgabios.c:38
    xor ah, ah                                ; 30 e4                       ; 0xc0d7f vgabios.c:517
    call 03651h                               ; e8 cd 28                    ; 0xc0d81
    mov cl, al                                ; 88 c1                       ; 0xc0d84
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc0d86 vgabios.c:518
    jne short 00d8dh                          ; 75 03                       ; 0xc0d88
    jmp near 00ea0h                           ; e9 13 01                    ; 0xc0d8a
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc0d8d vgabios.c:522
    mov byte [bp-014h], al                    ; 88 46 ec                    ; 0xc0d90
    mov byte [bp-013h], 000h                  ; c6 46 ed 00                 ; 0xc0d93
    lea bx, [bp-01ah]                         ; 8d 5e e6                    ; 0xc0d97
    lea dx, [bp-018h]                         ; 8d 56 e8                    ; 0xc0d9a
    mov ax, word [bp-014h]                    ; 8b 46 ec                    ; 0xc0d9d
    call 00a0ch                               ; e8 69 fc                    ; 0xc0da0
    mov ch, byte [bp-01ah]                    ; 8a 6e e6                    ; 0xc0da3 vgabios.c:523
    mov ax, word [bp-01ah]                    ; 8b 46 e6                    ; 0xc0da6 vgabios.c:524
    mov al, ah                                ; 88 e0                       ; 0xc0da9
    xor ah, ah                                ; 30 e4                       ; 0xc0dab
    mov word [bp-012h], ax                    ; 89 46 ee                    ; 0xc0dad
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc0db0
    mov byte [bp-00eh], al                    ; 88 46 f2                    ; 0xc0db3
    mov bx, 00084h                            ; bb 84 00                    ; 0xc0db6 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0db9
    mov es, ax                                ; 8e c0                       ; 0xc0dbc
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0dbe
    xor ah, ah                                ; 30 e4                       ; 0xc0dc1 vgabios.c:38
    mov dx, ax                                ; 89 c2                       ; 0xc0dc3
    inc dx                                    ; 42                          ; 0xc0dc5
    mov di, strict word 0004ah                ; bf 4a 00                    ; 0xc0dc6 vgabios.c:47
    mov di, word [es:di]                      ; 26 8b 3d                    ; 0xc0dc9
    mov word [bp-016h], di                    ; 89 7e ea                    ; 0xc0dcc vgabios.c:48
    mov bl, cl                                ; 88 cb                       ; 0xc0dcf vgabios.c:530
    xor bh, bh                                ; 30 ff                       ; 0xc0dd1
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc0dd3
    sal bx, CL                                ; d3 e3                       ; 0xc0dd5
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc0dd7
    jne short 00e14h                          ; 75 36                       ; 0xc0ddc
    mov ax, di                                ; 89 f8                       ; 0xc0dde vgabios.c:532
    mul dx                                    ; f7 e2                       ; 0xc0de0
    sal ax, 1                                 ; d1 e0                       ; 0xc0de2
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc0de4
    mov dl, byte [bp-00ch]                    ; 8a 56 f4                    ; 0xc0de6
    xor dh, dh                                ; 30 f6                       ; 0xc0de9
    inc ax                                    ; 40                          ; 0xc0deb
    mul dx                                    ; f7 e2                       ; 0xc0dec
    mov word [bp-010h], ax                    ; 89 46 f0                    ; 0xc0dee
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc0df1
    xor ah, ah                                ; 30 e4                       ; 0xc0df4
    mul di                                    ; f7 e7                       ; 0xc0df6
    mov dl, ch                                ; 88 ea                       ; 0xc0df8
    xor dh, dh                                ; 30 f6                       ; 0xc0dfa
    add ax, dx                                ; 01 d0                       ; 0xc0dfc
    sal ax, 1                                 ; d1 e0                       ; 0xc0dfe
    mov di, word [bp-010h]                    ; 8b 7e f0                    ; 0xc0e00
    add di, ax                                ; 01 c7                       ; 0xc0e03
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc0e05 vgabios.c:45
    mov ax, word [es:di]                      ; 26 8b 05                    ; 0xc0e09
    push SS                                   ; 16                          ; 0xc0e0c vgabios.c:48
    pop ES                                    ; 07                          ; 0xc0e0d
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc0e0e
    jmp near 00ea0h                           ; e9 8c 00                    ; 0xc0e11 vgabios.c:534
    mov bl, byte [bx+047b0h]                  ; 8a 9f b0 47                 ; 0xc0e14 vgabios.c:535
    cmp bl, 005h                              ; 80 fb 05                    ; 0xc0e18
    je short 00e73h                           ; 74 56                       ; 0xc0e1b
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc0e1d
    jc short 00e29h                           ; 72 07                       ; 0xc0e20
    jbe short 00e2bh                          ; 76 07                       ; 0xc0e22
    cmp bl, 004h                              ; 80 fb 04                    ; 0xc0e24
    jbe short 00e46h                          ; 76 1d                       ; 0xc0e27
    jmp short 00ea0h                          ; eb 75                       ; 0xc0e29
    mov dl, byte [bp-00eh]                    ; 8a 56 f2                    ; 0xc0e2b vgabios.c:538
    xor dh, dh                                ; 30 f6                       ; 0xc0e2e
    mov al, ch                                ; 88 e8                       ; 0xc0e30
    xor ah, ah                                ; 30 e4                       ; 0xc0e32
    mov bx, word [bp-016h]                    ; 8b 5e ea                    ; 0xc0e34
    call 00cdch                               ; e8 a2 fe                    ; 0xc0e37
    mov dl, byte [bp-00ah]                    ; 8a 56 f6                    ; 0xc0e3a vgabios.c:539
    xor dh, dh                                ; 30 f6                       ; 0xc0e3d
    call 00cf7h                               ; e8 b5 fe                    ; 0xc0e3f
    xor ah, ah                                ; 30 e4                       ; 0xc0e42
    jmp short 00e0ch                          ; eb c6                       ; 0xc0e44
    mov bx, 00085h                            ; bb 85 00                    ; 0xc0e46 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0e49
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc0e4c vgabios.c:544
    mov byte [bp-00fh], 000h                  ; c6 46 f1 00                 ; 0xc0e4f
    push word [bp-010h]                       ; ff 76 f0                    ; 0xc0e53
    mov dl, byte [bp-012h]                    ; 8a 56 ee                    ; 0xc0e56
    xor dh, dh                                ; 30 f6                       ; 0xc0e59
    mov al, ch                                ; 88 e8                       ; 0xc0e5b
    xor ah, ah                                ; 30 e4                       ; 0xc0e5d
    mov cx, word [bp-014h]                    ; 8b 4e ec                    ; 0xc0e5f
    mov bx, di                                ; 89 fb                       ; 0xc0e62
    call 00af0h                               ; e8 89 fc                    ; 0xc0e64
    mov bx, word [bp-010h]                    ; 8b 5e f0                    ; 0xc0e67 vgabios.c:545
    mov dx, ax                                ; 89 c2                       ; 0xc0e6a
    mov ax, di                                ; 89 f8                       ; 0xc0e6c
    call 00b1fh                               ; e8 ae fc                    ; 0xc0e6e
    jmp short 00e42h                          ; eb cf                       ; 0xc0e71
    mov bx, 00085h                            ; bb 85 00                    ; 0xc0e73 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0e76
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc0e79 vgabios.c:549
    mov byte [bp-00fh], 000h                  ; c6 46 f1 00                 ; 0xc0e7c
    push word [bp-010h]                       ; ff 76 f0                    ; 0xc0e80
    mov dl, byte [bp-012h]                    ; 8a 56 ee                    ; 0xc0e83
    xor dh, dh                                ; 30 f6                       ; 0xc0e86
    mov al, ch                                ; 88 e8                       ; 0xc0e88
    xor ah, ah                                ; 30 e4                       ; 0xc0e8a
    mov cx, word [bp-014h]                    ; 8b 4e ec                    ; 0xc0e8c
    mov bx, di                                ; 89 fb                       ; 0xc0e8f
    call 00b64h                               ; e8 d0 fc                    ; 0xc0e91
    mov bx, word [bp-010h]                    ; 8b 5e f0                    ; 0xc0e94 vgabios.c:550
    mov dx, ax                                ; 89 c2                       ; 0xc0e97
    mov ax, di                                ; 89 f8                       ; 0xc0e99
    call 00bd9h                               ; e8 3b fd                    ; 0xc0e9b
    jmp short 00e42h                          ; eb a2                       ; 0xc0e9e
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc0ea0 vgabios.c:559
    pop di                                    ; 5f                          ; 0xc0ea3
    pop si                                    ; 5e                          ; 0xc0ea4
    pop cx                                    ; 59                          ; 0xc0ea5
    pop bx                                    ; 5b                          ; 0xc0ea6
    pop bp                                    ; 5d                          ; 0xc0ea7
    retn                                      ; c3                          ; 0xc0ea8
  ; disGetNextSymbol 0xc0ea9 LB 0x3400 -> off=0x10 cb=000000000000008b uValue=00000000000c0eb9 'vga_get_font_info'
    db  0d4h, 00eh, 01ch, 00fh, 021h, 00fh, 029h, 00fh, 02eh, 00fh, 033h, 00fh, 038h, 00fh, 03dh, 00fh
vga_get_font_info:                           ; 0xc0eb9 LB 0x8b
    push si                                   ; 56                          ; 0xc0eb9 vgabios.c:561
    push di                                   ; 57                          ; 0xc0eba
    push bp                                   ; 55                          ; 0xc0ebb
    mov bp, sp                                ; 89 e5                       ; 0xc0ebc
    push ax                                   ; 50                          ; 0xc0ebe
    mov di, dx                                ; 89 d7                       ; 0xc0ebf
    mov word [bp-002h], bx                    ; 89 5e fe                    ; 0xc0ec1
    mov si, cx                                ; 89 ce                       ; 0xc0ec4
    cmp ax, strict word 00007h                ; 3d 07 00                    ; 0xc0ec6 vgabios.c:566
    jnbe short 00f14h                         ; 77 49                       ; 0xc0ec9
    mov bx, ax                                ; 89 c3                       ; 0xc0ecb
    sal bx, 1                                 ; d1 e3                       ; 0xc0ecd
    jmp word [cs:bx+00ea9h]                   ; 2e ff a7 a9 0e              ; 0xc0ecf
    mov bx, strict word 0007ch                ; bb 7c 00                    ; 0xc0ed4 vgabios.c:57
    xor ax, ax                                ; 31 c0                       ; 0xc0ed7
    mov es, ax                                ; 8e c0                       ; 0xc0ed9
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0edb
    mov dx, word [es:bx+002h]                 ; 26 8b 57 02                 ; 0xc0ede
    push SS                                   ; 16                          ; 0xc0ee2 vgabios.c:569
    pop ES                                    ; 07                          ; 0xc0ee3
    mov bx, word [bp-002h]                    ; 8b 5e fe                    ; 0xc0ee4
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc0ee7
    mov word [es:di], dx                      ; 26 89 15                    ; 0xc0eea
    mov bx, 00085h                            ; bb 85 00                    ; 0xc0eed
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0ef0
    mov es, ax                                ; 8e c0                       ; 0xc0ef3
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0ef5
    xor ah, ah                                ; 30 e4                       ; 0xc0ef8
    push SS                                   ; 16                          ; 0xc0efa
    pop ES                                    ; 07                          ; 0xc0efb
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc0efc
    mov bx, 00084h                            ; bb 84 00                    ; 0xc0eff
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0f02
    mov es, ax                                ; 8e c0                       ; 0xc0f05
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0f07
    xor ah, ah                                ; 30 e4                       ; 0xc0f0a
    push SS                                   ; 16                          ; 0xc0f0c
    pop ES                                    ; 07                          ; 0xc0f0d
    mov bx, word [bp+008h]                    ; 8b 5e 08                    ; 0xc0f0e
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc0f11
    mov sp, bp                                ; 89 ec                       ; 0xc0f14
    pop bp                                    ; 5d                          ; 0xc0f16
    pop di                                    ; 5f                          ; 0xc0f17
    pop si                                    ; 5e                          ; 0xc0f18
    retn 00002h                               ; c2 02 00                    ; 0xc0f19
    mov bx, 0010ch                            ; bb 0c 01                    ; 0xc0f1c vgabios.c:57
    jmp short 00ed7h                          ; eb b6                       ; 0xc0f1f
    mov ax, 05d6ch                            ; b8 6c 5d                    ; 0xc0f21 vgabios.c:574
    mov dx, 0c000h                            ; ba 00 c0                    ; 0xc0f24
    jmp short 00ee2h                          ; eb b9                       ; 0xc0f27 vgabios.c:575
    mov ax, 0556ch                            ; b8 6c 55                    ; 0xc0f29 vgabios.c:577
    jmp short 00f24h                          ; eb f6                       ; 0xc0f2c
    mov ax, 0596ch                            ; b8 6c 59                    ; 0xc0f2e vgabios.c:580
    jmp short 00f24h                          ; eb f1                       ; 0xc0f31
    mov ax, 07b6ch                            ; b8 6c 7b                    ; 0xc0f33 vgabios.c:583
    jmp short 00f24h                          ; eb ec                       ; 0xc0f36
    mov ax, 06b6ch                            ; b8 6c 6b                    ; 0xc0f38 vgabios.c:586
    jmp short 00f24h                          ; eb e7                       ; 0xc0f3b
    mov ax, 07c99h                            ; b8 99 7c                    ; 0xc0f3d vgabios.c:589
    jmp short 00f24h                          ; eb e2                       ; 0xc0f40
    jmp short 00f14h                          ; eb d0                       ; 0xc0f42 vgabios.c:595
  ; disGetNextSymbol 0xc0f44 LB 0x3365 -> off=0x0 cb=000000000000016d uValue=00000000000c0f44 'vga_read_pixel'
vga_read_pixel:                              ; 0xc0f44 LB 0x16d
    push bp                                   ; 55                          ; 0xc0f44 vgabios.c:608
    mov bp, sp                                ; 89 e5                       ; 0xc0f45
    push si                                   ; 56                          ; 0xc0f47
    push di                                   ; 57                          ; 0xc0f48
    sub sp, strict byte 0000ch                ; 83 ec 0c                    ; 0xc0f49
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc0f4c
    mov si, dx                                ; 89 d6                       ; 0xc0f4f
    mov word [bp-010h], bx                    ; 89 5e f0                    ; 0xc0f51
    mov word [bp-00eh], cx                    ; 89 4e f2                    ; 0xc0f54
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc0f57 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0f5a
    mov es, ax                                ; 8e c0                       ; 0xc0f5d
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0f5f
    xor ah, ah                                ; 30 e4                       ; 0xc0f62 vgabios.c:615
    call 03651h                               ; e8 ea 26                    ; 0xc0f64
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc0f67
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc0f6a vgabios.c:616
    je short 00f7dh                           ; 74 0f                       ; 0xc0f6c
    mov bl, al                                ; 88 c3                       ; 0xc0f6e vgabios.c:618
    xor bh, bh                                ; 30 ff                       ; 0xc0f70
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc0f72
    sal bx, CL                                ; d3 e3                       ; 0xc0f74
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc0f76
    jne short 00f80h                          ; 75 03                       ; 0xc0f7b
    jmp near 010aah                           ; e9 2a 01                    ; 0xc0f7d vgabios.c:619
    mov ch, byte [bx+047b0h]                  ; 8a af b0 47                 ; 0xc0f80 vgabios.c:622
    cmp ch, cl                                ; 38 cd                       ; 0xc0f84
    jc short 00f97h                           ; 72 0f                       ; 0xc0f86
    jbe short 00f9fh                          ; 76 15                       ; 0xc0f88
    cmp ch, 005h                              ; 80 fd 05                    ; 0xc0f8a
    je short 00fd8h                           ; 74 49                       ; 0xc0f8d
    cmp ch, 004h                              ; 80 fd 04                    ; 0xc0f8f
    je short 00f9fh                           ; 74 0b                       ; 0xc0f92
    jmp near 010a0h                           ; e9 09 01                    ; 0xc0f94
    cmp ch, 002h                              ; 80 fd 02                    ; 0xc0f97
    je short 0100ch                           ; 74 70                       ; 0xc0f9a
    jmp near 010a0h                           ; e9 01 01                    ; 0xc0f9c
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc0f9f vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0fa2
    mov es, ax                                ; 8e c0                       ; 0xc0fa5
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc0fa7
    mov ax, word [bp-010h]                    ; 8b 46 f0                    ; 0xc0faa vgabios.c:48
    mul bx                                    ; f7 e3                       ; 0xc0fad
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc0faf
    mov bx, si                                ; 89 f3                       ; 0xc0fb1
    shr bx, CL                                ; d3 eb                       ; 0xc0fb3
    add bx, ax                                ; 01 c3                       ; 0xc0fb5
    mov di, strict word 0004ch                ; bf 4c 00                    ; 0xc0fb7 vgabios.c:47
    mov ax, word [es:di]                      ; 26 8b 05                    ; 0xc0fba
    mov cl, byte [bp-00ch]                    ; 8a 4e f4                    ; 0xc0fbd vgabios.c:48
    xor ch, ch                                ; 30 ed                       ; 0xc0fc0
    mul cx                                    ; f7 e1                       ; 0xc0fc2
    add bx, ax                                ; 01 c3                       ; 0xc0fc4
    mov cx, si                                ; 89 f1                       ; 0xc0fc6 vgabios.c:627
    and cx, strict byte 00007h                ; 83 e1 07                    ; 0xc0fc8
    mov ax, 00080h                            ; b8 80 00                    ; 0xc0fcb
    sar ax, CL                                ; d3 f8                       ; 0xc0fce
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc0fd0
    mov byte [bp-008h], ch                    ; 88 6e f8                    ; 0xc0fd3 vgabios.c:629
    jmp short 00fe1h                          ; eb 09                       ; 0xc0fd6
    jmp near 01080h                           ; e9 a5 00                    ; 0xc0fd8
    cmp byte [bp-008h], 004h                  ; 80 7e f8 04                 ; 0xc0fdb
    jnc short 01009h                          ; 73 28                       ; 0xc0fdf
    mov ah, byte [bp-008h]                    ; 8a 66 f8                    ; 0xc0fe1 vgabios.c:630
    xor al, al                                ; 30 c0                       ; 0xc0fe4
    or AL, strict byte 004h                   ; 0c 04                       ; 0xc0fe6
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc0fe8
    out DX, ax                                ; ef                          ; 0xc0feb
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc0fec vgabios.c:37
    mov es, ax                                ; 8e c0                       ; 0xc0fef
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0ff1
    and al, byte [bp-00ah]                    ; 22 46 f6                    ; 0xc0ff4 vgabios.c:38
    test al, al                               ; 84 c0                       ; 0xc0ff7 vgabios.c:632
    jbe short 01004h                          ; 76 09                       ; 0xc0ff9
    mov cl, byte [bp-008h]                    ; 8a 4e f8                    ; 0xc0ffb vgabios.c:633
    mov AL, strict byte 001h                  ; b0 01                       ; 0xc0ffe
    sal al, CL                                ; d2 e0                       ; 0xc1000
    or ch, al                                 ; 08 c5                       ; 0xc1002
    inc byte [bp-008h]                        ; fe 46 f8                    ; 0xc1004 vgabios.c:634
    jmp short 00fdbh                          ; eb d2                       ; 0xc1007
    jmp near 010a2h                           ; e9 96 00                    ; 0xc1009
    mov al, byte [bx+047b1h]                  ; 8a 87 b1 47                 ; 0xc100c vgabios.c:637
    xor ah, ah                                ; 30 e4                       ; 0xc1010
    mov cx, strict word 00004h                ; b9 04 00                    ; 0xc1012
    sub cx, ax                                ; 29 c1                       ; 0xc1015
    mov ax, dx                                ; 89 d0                       ; 0xc1017
    shr ax, CL                                ; d3 e8                       ; 0xc1019
    mov cx, ax                                ; 89 c1                       ; 0xc101b
    mov ax, word [bp-010h]                    ; 8b 46 f0                    ; 0xc101d
    shr ax, 1                                 ; d1 e8                       ; 0xc1020
    mov bx, strict word 00050h                ; bb 50 00                    ; 0xc1022
    mul bx                                    ; f7 e3                       ; 0xc1025
    mov bx, cx                                ; 89 cb                       ; 0xc1027
    add bx, ax                                ; 01 c3                       ; 0xc1029
    test byte [bp-010h], 001h                 ; f6 46 f0 01                 ; 0xc102b vgabios.c:638
    je short 01034h                           ; 74 03                       ; 0xc102f
    add bh, 020h                              ; 80 c7 20                    ; 0xc1031 vgabios.c:639
    mov ax, 0b800h                            ; b8 00 b8                    ; 0xc1034 vgabios.c:37
    mov es, ax                                ; 8e c0                       ; 0xc1037
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc1039
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc103c vgabios.c:641
    xor bh, bh                                ; 30 ff                       ; 0xc103f
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc1041
    sal bx, CL                                ; d3 e3                       ; 0xc1043
    cmp byte [bx+047b1h], 002h                ; 80 bf b1 47 02              ; 0xc1045
    jne short 01067h                          ; 75 1b                       ; 0xc104a
    mov cx, si                                ; 89 f1                       ; 0xc104c vgabios.c:642
    xor ch, ch                                ; 30 ed                       ; 0xc104e
    and cl, 003h                              ; 80 e1 03                    ; 0xc1050
    mov bx, strict word 00003h                ; bb 03 00                    ; 0xc1053
    sub bx, cx                                ; 29 cb                       ; 0xc1056
    mov cx, bx                                ; 89 d9                       ; 0xc1058
    sal cx, 1                                 ; d1 e1                       ; 0xc105a
    xor ah, ah                                ; 30 e4                       ; 0xc105c
    sar ax, CL                                ; d3 f8                       ; 0xc105e
    mov ch, al                                ; 88 c5                       ; 0xc1060
    and ch, 003h                              ; 80 e5 03                    ; 0xc1062
    jmp short 010a2h                          ; eb 3b                       ; 0xc1065 vgabios.c:643
    mov cx, si                                ; 89 f1                       ; 0xc1067 vgabios.c:644
    xor ch, ch                                ; 30 ed                       ; 0xc1069
    and cl, 007h                              ; 80 e1 07                    ; 0xc106b
    mov bx, strict word 00007h                ; bb 07 00                    ; 0xc106e
    sub bx, cx                                ; 29 cb                       ; 0xc1071
    mov cx, bx                                ; 89 d9                       ; 0xc1073
    xor ah, ah                                ; 30 e4                       ; 0xc1075
    sar ax, CL                                ; d3 f8                       ; 0xc1077
    mov ch, al                                ; 88 c5                       ; 0xc1079
    and ch, 001h                              ; 80 e5 01                    ; 0xc107b
    jmp short 010a2h                          ; eb 22                       ; 0xc107e vgabios.c:645
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc1080 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1083
    mov es, ax                                ; 8e c0                       ; 0xc1086
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc1088
    sal bx, CL                                ; d3 e3                       ; 0xc108b vgabios.c:48
    mov ax, word [bp-010h]                    ; 8b 46 f0                    ; 0xc108d
    mul bx                                    ; f7 e3                       ; 0xc1090
    mov bx, si                                ; 89 f3                       ; 0xc1092
    add bx, ax                                ; 01 c3                       ; 0xc1094
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc1096 vgabios.c:37
    mov es, ax                                ; 8e c0                       ; 0xc1099
    mov ch, byte [es:bx]                      ; 26 8a 2f                    ; 0xc109b
    jmp short 010a2h                          ; eb 02                       ; 0xc109e vgabios.c:649
    xor ch, ch                                ; 30 ed                       ; 0xc10a0 vgabios.c:654
    push SS                                   ; 16                          ; 0xc10a2 vgabios.c:656
    pop ES                                    ; 07                          ; 0xc10a3
    mov bx, word [bp-00eh]                    ; 8b 5e f2                    ; 0xc10a4
    mov byte [es:bx], ch                      ; 26 88 2f                    ; 0xc10a7
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc10aa vgabios.c:657
    pop di                                    ; 5f                          ; 0xc10ad
    pop si                                    ; 5e                          ; 0xc10ae
    pop bp                                    ; 5d                          ; 0xc10af
    retn                                      ; c3                          ; 0xc10b0
  ; disGetNextSymbol 0xc10b1 LB 0x31f8 -> off=0x0 cb=000000000000009f uValue=00000000000c10b1 'biosfn_perform_gray_scale_summing'
biosfn_perform_gray_scale_summing:           ; 0xc10b1 LB 0x9f
    push bp                                   ; 55                          ; 0xc10b1 vgabios.c:662
    mov bp, sp                                ; 89 e5                       ; 0xc10b2
    push bx                                   ; 53                          ; 0xc10b4
    push cx                                   ; 51                          ; 0xc10b5
    push si                                   ; 56                          ; 0xc10b6
    push di                                   ; 57                          ; 0xc10b7
    push ax                                   ; 50                          ; 0xc10b8
    push ax                                   ; 50                          ; 0xc10b9
    mov bx, ax                                ; 89 c3                       ; 0xc10ba
    mov di, dx                                ; 89 d7                       ; 0xc10bc
    mov dx, 003dah                            ; ba da 03                    ; 0xc10be vgabios.c:667
    in AL, DX                                 ; ec                          ; 0xc10c1
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc10c2
    xor al, al                                ; 30 c0                       ; 0xc10c4 vgabios.c:668
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc10c6
    out DX, AL                                ; ee                          ; 0xc10c9
    xor si, si                                ; 31 f6                       ; 0xc10ca vgabios.c:670
    cmp si, di                                ; 39 fe                       ; 0xc10cc
    jnc short 01135h                          ; 73 65                       ; 0xc10ce
    mov al, bl                                ; 88 d8                       ; 0xc10d0 vgabios.c:673
    mov dx, 003c7h                            ; ba c7 03                    ; 0xc10d2
    out DX, AL                                ; ee                          ; 0xc10d5
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc10d6 vgabios.c:675
    in AL, DX                                 ; ec                          ; 0xc10d9
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc10da
    mov cx, ax                                ; 89 c1                       ; 0xc10dc
    in AL, DX                                 ; ec                          ; 0xc10de vgabios.c:676
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc10df
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc10e1
    in AL, DX                                 ; ec                          ; 0xc10e4 vgabios.c:677
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc10e5
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc10e7
    mov al, cl                                ; 88 c8                       ; 0xc10ea vgabios.c:680
    xor ah, ah                                ; 30 e4                       ; 0xc10ec
    mov cx, strict word 0004dh                ; b9 4d 00                    ; 0xc10ee
    imul cx                                   ; f7 e9                       ; 0xc10f1
    mov cx, ax                                ; 89 c1                       ; 0xc10f3
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc10f5
    xor ah, ah                                ; 30 e4                       ; 0xc10f8
    mov dx, 00097h                            ; ba 97 00                    ; 0xc10fa
    imul dx                                   ; f7 ea                       ; 0xc10fd
    add cx, ax                                ; 01 c1                       ; 0xc10ff
    mov word [bp-00ah], cx                    ; 89 4e f6                    ; 0xc1101
    mov cl, byte [bp-00ch]                    ; 8a 4e f4                    ; 0xc1104
    xor ch, ch                                ; 30 ed                       ; 0xc1107
    mov ax, cx                                ; 89 c8                       ; 0xc1109
    mov dx, strict word 0001ch                ; ba 1c 00                    ; 0xc110b
    imul dx                                   ; f7 ea                       ; 0xc110e
    add ax, word [bp-00ah]                    ; 03 46 f6                    ; 0xc1110
    add ax, 00080h                            ; 05 80 00                    ; 0xc1113
    mov al, ah                                ; 88 e0                       ; 0xc1116
    cbw                                       ; 98                          ; 0xc1118
    mov cx, ax                                ; 89 c1                       ; 0xc1119
    cmp ax, strict word 0003fh                ; 3d 3f 00                    ; 0xc111b vgabios.c:682
    jbe short 01123h                          ; 76 03                       ; 0xc111e
    mov cx, strict word 0003fh                ; b9 3f 00                    ; 0xc1120
    mov al, bl                                ; 88 d8                       ; 0xc1123 vgabios.c:685
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc1125
    out DX, AL                                ; ee                          ; 0xc1128
    mov al, cl                                ; 88 c8                       ; 0xc1129 vgabios.c:687
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc112b
    out DX, AL                                ; ee                          ; 0xc112e
    out DX, AL                                ; ee                          ; 0xc112f vgabios.c:688
    out DX, AL                                ; ee                          ; 0xc1130 vgabios.c:689
    inc bx                                    ; 43                          ; 0xc1131 vgabios.c:690
    inc si                                    ; 46                          ; 0xc1132 vgabios.c:691
    jmp short 010cch                          ; eb 97                       ; 0xc1133
    mov dx, 003dah                            ; ba da 03                    ; 0xc1135 vgabios.c:692
    in AL, DX                                 ; ec                          ; 0xc1138
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc1139
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc113b vgabios.c:693
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc113d
    out DX, AL                                ; ee                          ; 0xc1140
    mov dx, 003dah                            ; ba da 03                    ; 0xc1141 vgabios.c:695
    in AL, DX                                 ; ec                          ; 0xc1144
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc1145
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc1147 vgabios.c:697
    pop di                                    ; 5f                          ; 0xc114a
    pop si                                    ; 5e                          ; 0xc114b
    pop cx                                    ; 59                          ; 0xc114c
    pop bx                                    ; 5b                          ; 0xc114d
    pop bp                                    ; 5d                          ; 0xc114e
    retn                                      ; c3                          ; 0xc114f
  ; disGetNextSymbol 0xc1150 LB 0x3159 -> off=0x0 cb=00000000000000fc uValue=00000000000c1150 'biosfn_set_cursor_shape'
biosfn_set_cursor_shape:                     ; 0xc1150 LB 0xfc
    push bp                                   ; 55                          ; 0xc1150 vgabios.c:700
    mov bp, sp                                ; 89 e5                       ; 0xc1151
    push bx                                   ; 53                          ; 0xc1153
    push cx                                   ; 51                          ; 0xc1154
    push si                                   ; 56                          ; 0xc1155
    push ax                                   ; 50                          ; 0xc1156
    push ax                                   ; 50                          ; 0xc1157
    mov ah, al                                ; 88 c4                       ; 0xc1158
    mov bl, dl                                ; 88 d3                       ; 0xc115a
    mov dh, al                                ; 88 c6                       ; 0xc115c vgabios.c:706
    mov si, strict word 00060h                ; be 60 00                    ; 0xc115e vgabios.c:52
    mov cx, strict word 00040h                ; b9 40 00                    ; 0xc1161
    mov es, cx                                ; 8e c1                       ; 0xc1164
    mov word [es:si], dx                      ; 26 89 14                    ; 0xc1166
    mov si, 00087h                            ; be 87 00                    ; 0xc1169 vgabios.c:37
    mov dl, byte [es:si]                      ; 26 8a 14                    ; 0xc116c
    test dl, 008h                             ; f6 c2 08                    ; 0xc116f vgabios.c:38
    jne short 011b1h                          ; 75 3d                       ; 0xc1172
    mov dl, al                                ; 88 c2                       ; 0xc1174 vgabios.c:712
    and dl, 060h                              ; 80 e2 60                    ; 0xc1176
    cmp dl, 020h                              ; 80 fa 20                    ; 0xc1179
    jne short 01184h                          ; 75 06                       ; 0xc117c
    mov AH, strict byte 01eh                  ; b4 1e                       ; 0xc117e vgabios.c:714
    xor bl, bl                                ; 30 db                       ; 0xc1180 vgabios.c:715
    jmp short 011b1h                          ; eb 2d                       ; 0xc1182 vgabios.c:716
    mov dl, byte [es:si]                      ; 26 8a 14                    ; 0xc1184 vgabios.c:37
    test dl, 001h                             ; f6 c2 01                    ; 0xc1187 vgabios.c:38
    jne short 011e6h                          ; 75 5a                       ; 0xc118a
    cmp ah, 020h                              ; 80 fc 20                    ; 0xc118c
    jnc short 011e6h                          ; 73 55                       ; 0xc118f
    cmp bl, 020h                              ; 80 fb 20                    ; 0xc1191
    jnc short 011e6h                          ; 73 50                       ; 0xc1194
    mov si, 00085h                            ; be 85 00                    ; 0xc1196 vgabios.c:47
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc1199
    mov es, dx                                ; 8e c2                       ; 0xc119c
    mov cx, word [es:si]                      ; 26 8b 0c                    ; 0xc119e
    mov dx, cx                                ; 89 ca                       ; 0xc11a1 vgabios.c:48
    cmp bl, ah                                ; 38 e3                       ; 0xc11a3 vgabios.c:727
    jnc short 011b3h                          ; 73 0c                       ; 0xc11a5
    test bl, bl                               ; 84 db                       ; 0xc11a7 vgabios.c:729
    je short 011e6h                           ; 74 3b                       ; 0xc11a9
    xor ah, ah                                ; 30 e4                       ; 0xc11ab vgabios.c:730
    mov bl, cl                                ; 88 cb                       ; 0xc11ad vgabios.c:731
    db  0feh, 0cbh
    ; dec bl                                    ; fe cb                     ; 0xc11af
    jmp short 011e6h                          ; eb 33                       ; 0xc11b1 vgabios.c:733
    mov byte [bp-008h], bl                    ; 88 5e f8                    ; 0xc11b3 vgabios.c:734
    xor al, al                                ; 30 c0                       ; 0xc11b6
    mov byte [bp-007h], al                    ; 88 46 f9                    ; 0xc11b8
    mov byte [bp-00ah], ah                    ; 88 66 f6                    ; 0xc11bb
    mov byte [bp-009h], al                    ; 88 46 f7                    ; 0xc11be
    mov si, word [bp-008h]                    ; 8b 76 f8                    ; 0xc11c1
    or si, word [bp-00ah]                     ; 0b 76 f6                    ; 0xc11c4
    cmp si, cx                                ; 39 ce                       ; 0xc11c7
    jnc short 011e8h                          ; 73 1d                       ; 0xc11c9
    mov byte [bp-008h], bl                    ; 88 5e f8                    ; 0xc11cb
    mov byte [bp-007h], al                    ; 88 46 f9                    ; 0xc11ce
    mov si, cx                                ; 89 ce                       ; 0xc11d1
    dec si                                    ; 4e                          ; 0xc11d3
    cmp si, word [bp-008h]                    ; 3b 76 f8                    ; 0xc11d4
    je short 01222h                           ; 74 49                       ; 0xc11d7
    mov byte [bp-008h], ah                    ; 88 66 f8                    ; 0xc11d9
    mov byte [bp-007h], al                    ; 88 46 f9                    ; 0xc11dc
    dec cx                                    ; 49                          ; 0xc11df
    dec cx                                    ; 49                          ; 0xc11e0
    cmp cx, word [bp-008h]                    ; 3b 4e f8                    ; 0xc11e1
    jne short 011e8h                          ; 75 02                       ; 0xc11e4
    jmp short 01222h                          ; eb 3a                       ; 0xc11e6
    cmp bl, 003h                              ; 80 fb 03                    ; 0xc11e8 vgabios.c:736
    jbe short 01222h                          ; 76 35                       ; 0xc11eb
    mov cl, ah                                ; 88 e1                       ; 0xc11ed vgabios.c:737
    xor ch, ch                                ; 30 ed                       ; 0xc11ef
    mov byte [bp-008h], bl                    ; 88 5e f8                    ; 0xc11f1
    mov byte [bp-007h], ch                    ; 88 6e f9                    ; 0xc11f4
    mov si, cx                                ; 89 ce                       ; 0xc11f7
    inc si                                    ; 46                          ; 0xc11f9
    inc si                                    ; 46                          ; 0xc11fa
    mov cl, dl                                ; 88 d1                       ; 0xc11fb
    db  0feh, 0c9h
    ; dec cl                                    ; fe c9                     ; 0xc11fd
    cmp si, word [bp-008h]                    ; 3b 76 f8                    ; 0xc11ff
    jl short 01217h                           ; 7c 13                       ; 0xc1202
    sub ah, bl                                ; 28 dc                       ; 0xc1204 vgabios.c:739
    add ah, dl                                ; 00 d4                       ; 0xc1206
    db  0feh, 0cch
    ; dec ah                                    ; fe cc                     ; 0xc1208
    mov bl, cl                                ; 88 cb                       ; 0xc120a vgabios.c:740
    cmp dx, strict byte 0000eh                ; 83 fa 0e                    ; 0xc120c vgabios.c:741
    jc short 01222h                           ; 72 11                       ; 0xc120f
    db  0feh, 0cbh
    ; dec bl                                    ; fe cb                     ; 0xc1211 vgabios.c:743
    db  0feh, 0cch
    ; dec ah                                    ; fe cc                     ; 0xc1213 vgabios.c:744
    jmp short 01222h                          ; eb 0b                       ; 0xc1215 vgabios.c:746
    cmp ah, 002h                              ; 80 fc 02                    ; 0xc1217
    jbe short 01220h                          ; 76 04                       ; 0xc121a
    shr dx, 1                                 ; d1 ea                       ; 0xc121c vgabios.c:748
    mov ah, dl                                ; 88 d4                       ; 0xc121e
    mov bl, cl                                ; 88 cb                       ; 0xc1220 vgabios.c:752
    mov si, strict word 00063h                ; be 63 00                    ; 0xc1222 vgabios.c:47
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc1225
    mov es, dx                                ; 8e c2                       ; 0xc1228
    mov cx, word [es:si]                      ; 26 8b 0c                    ; 0xc122a
    mov AL, strict byte 00ah                  ; b0 0a                       ; 0xc122d vgabios.c:763
    mov dx, cx                                ; 89 ca                       ; 0xc122f
    out DX, AL                                ; ee                          ; 0xc1231
    mov si, cx                                ; 89 ce                       ; 0xc1232 vgabios.c:764
    inc si                                    ; 46                          ; 0xc1234
    mov al, ah                                ; 88 e0                       ; 0xc1235
    mov dx, si                                ; 89 f2                       ; 0xc1237
    out DX, AL                                ; ee                          ; 0xc1239
    mov AL, strict byte 00bh                  ; b0 0b                       ; 0xc123a vgabios.c:765
    mov dx, cx                                ; 89 ca                       ; 0xc123c
    out DX, AL                                ; ee                          ; 0xc123e
    mov al, bl                                ; 88 d8                       ; 0xc123f vgabios.c:766
    mov dx, si                                ; 89 f2                       ; 0xc1241
    out DX, AL                                ; ee                          ; 0xc1243
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc1244 vgabios.c:767
    pop si                                    ; 5e                          ; 0xc1247
    pop cx                                    ; 59                          ; 0xc1248
    pop bx                                    ; 5b                          ; 0xc1249
    pop bp                                    ; 5d                          ; 0xc124a
    retn                                      ; c3                          ; 0xc124b
  ; disGetNextSymbol 0xc124c LB 0x305d -> off=0x0 cb=000000000000008d uValue=00000000000c124c 'biosfn_set_cursor_pos'
biosfn_set_cursor_pos:                       ; 0xc124c LB 0x8d
    push bp                                   ; 55                          ; 0xc124c vgabios.c:770
    mov bp, sp                                ; 89 e5                       ; 0xc124d
    push bx                                   ; 53                          ; 0xc124f
    push cx                                   ; 51                          ; 0xc1250
    push si                                   ; 56                          ; 0xc1251
    push di                                   ; 57                          ; 0xc1252
    push ax                                   ; 50                          ; 0xc1253
    mov bl, al                                ; 88 c3                       ; 0xc1254
    mov cx, dx                                ; 89 d1                       ; 0xc1256
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc1258 vgabios.c:776
    jnbe short 012d0h                         ; 77 74                       ; 0xc125a
    xor ah, ah                                ; 30 e4                       ; 0xc125c vgabios.c:779
    mov si, ax                                ; 89 c6                       ; 0xc125e
    sal si, 1                                 ; d1 e6                       ; 0xc1260
    add si, strict byte 00050h                ; 83 c6 50                    ; 0xc1262
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1265 vgabios.c:52
    mov es, ax                                ; 8e c0                       ; 0xc1268
    mov word [es:si], dx                      ; 26 89 14                    ; 0xc126a
    mov si, strict word 00062h                ; be 62 00                    ; 0xc126d vgabios.c:37
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc1270
    cmp bl, al                                ; 38 c3                       ; 0xc1273 vgabios.c:783
    jne short 012d0h                          ; 75 59                       ; 0xc1275
    mov si, strict word 0004ah                ; be 4a 00                    ; 0xc1277 vgabios.c:47
    mov si, word [es:si]                      ; 26 8b 34                    ; 0xc127a
    mov di, 00084h                            ; bf 84 00                    ; 0xc127d vgabios.c:37
    mov al, byte [es:di]                      ; 26 8a 05                    ; 0xc1280
    xor ah, ah                                ; 30 e4                       ; 0xc1283 vgabios.c:38
    mov di, ax                                ; 89 c7                       ; 0xc1285
    inc di                                    ; 47                          ; 0xc1287
    mov ax, dx                                ; 89 d0                       ; 0xc1288 vgabios.c:789
    mov al, dh                                ; 88 f0                       ; 0xc128a
    xor ah, dh                                ; 30 f4                       ; 0xc128c
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc128e
    mov ax, si                                ; 89 f0                       ; 0xc1291 vgabios.c:792
    mul di                                    ; f7 e7                       ; 0xc1293
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc1295
    xor bh, bh                                ; 30 ff                       ; 0xc1297
    inc ax                                    ; 40                          ; 0xc1299
    mul bx                                    ; f7 e3                       ; 0xc129a
    mov bx, ax                                ; 89 c3                       ; 0xc129c
    mov al, cl                                ; 88 c8                       ; 0xc129e
    xor ah, ah                                ; 30 e4                       ; 0xc12a0
    add bx, ax                                ; 01 c3                       ; 0xc12a2
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc12a4
    mul si                                    ; f7 e6                       ; 0xc12a7
    mov si, bx                                ; 89 de                       ; 0xc12a9
    add si, ax                                ; 01 c6                       ; 0xc12ab
    mov bx, strict word 00063h                ; bb 63 00                    ; 0xc12ad vgabios.c:47
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc12b0
    mov AL, strict byte 00eh                  ; b0 0e                       ; 0xc12b3 vgabios.c:796
    mov dx, bx                                ; 89 da                       ; 0xc12b5
    out DX, AL                                ; ee                          ; 0xc12b7
    mov ax, si                                ; 89 f0                       ; 0xc12b8 vgabios.c:797
    mov al, ah                                ; 88 e0                       ; 0xc12ba
    lea cx, [bx+001h]                         ; 8d 4f 01                    ; 0xc12bc
    mov dx, cx                                ; 89 ca                       ; 0xc12bf
    out DX, AL                                ; ee                          ; 0xc12c1
    mov AL, strict byte 00fh                  ; b0 0f                       ; 0xc12c2 vgabios.c:798
    mov dx, bx                                ; 89 da                       ; 0xc12c4
    out DX, AL                                ; ee                          ; 0xc12c6
    and si, 000ffh                            ; 81 e6 ff 00                 ; 0xc12c7 vgabios.c:799
    mov ax, si                                ; 89 f0                       ; 0xc12cb
    mov dx, cx                                ; 89 ca                       ; 0xc12cd
    out DX, AL                                ; ee                          ; 0xc12cf
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc12d0 vgabios.c:801
    pop di                                    ; 5f                          ; 0xc12d3
    pop si                                    ; 5e                          ; 0xc12d4
    pop cx                                    ; 59                          ; 0xc12d5
    pop bx                                    ; 5b                          ; 0xc12d6
    pop bp                                    ; 5d                          ; 0xc12d7
    retn                                      ; c3                          ; 0xc12d8
  ; disGetNextSymbol 0xc12d9 LB 0x2fd0 -> off=0x0 cb=00000000000000d5 uValue=00000000000c12d9 'biosfn_set_active_page'
biosfn_set_active_page:                      ; 0xc12d9 LB 0xd5
    push bp                                   ; 55                          ; 0xc12d9 vgabios.c:804
    mov bp, sp                                ; 89 e5                       ; 0xc12da
    push bx                                   ; 53                          ; 0xc12dc
    push cx                                   ; 51                          ; 0xc12dd
    push dx                                   ; 52                          ; 0xc12de
    push si                                   ; 56                          ; 0xc12df
    push di                                   ; 57                          ; 0xc12e0
    sub sp, strict byte 00006h                ; 83 ec 06                    ; 0xc12e1
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc12e4
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc12e7 vgabios.c:810
    jnbe short 01301h                         ; 77 16                       ; 0xc12e9
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc12eb vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc12ee
    mov es, ax                                ; 8e c0                       ; 0xc12f1
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc12f3
    xor ah, ah                                ; 30 e4                       ; 0xc12f6 vgabios.c:814
    call 03651h                               ; e8 56 23                    ; 0xc12f8
    mov cl, al                                ; 88 c1                       ; 0xc12fb
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc12fd vgabios.c:815
    jne short 01304h                          ; 75 03                       ; 0xc12ff
    jmp near 013a4h                           ; e9 a0 00                    ; 0xc1301
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1304 vgabios.c:818
    xor ah, ah                                ; 30 e4                       ; 0xc1307
    lea bx, [bp-010h]                         ; 8d 5e f0                    ; 0xc1309
    lea dx, [bp-00eh]                         ; 8d 56 f2                    ; 0xc130c
    call 00a0ch                               ; e8 fa f6                    ; 0xc130f
    mov bl, cl                                ; 88 cb                       ; 0xc1312 vgabios.c:820
    xor bh, bh                                ; 30 ff                       ; 0xc1314
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc1316
    mov si, bx                                ; 89 de                       ; 0xc1318
    sal si, CL                                ; d3 e6                       ; 0xc131a
    cmp byte [si+047afh], 000h                ; 80 bc af 47 00              ; 0xc131c
    jne short 0135eh                          ; 75 3b                       ; 0xc1321
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc1323 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1326
    mov es, ax                                ; 8e c0                       ; 0xc1329
    mov dx, word [es:bx]                      ; 26 8b 17                    ; 0xc132b
    mov bx, 00084h                            ; bb 84 00                    ; 0xc132e vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc1331
    xor ah, ah                                ; 30 e4                       ; 0xc1334 vgabios.c:38
    mov bx, ax                                ; 89 c3                       ; 0xc1336
    inc bx                                    ; 43                          ; 0xc1338
    mov ax, dx                                ; 89 d0                       ; 0xc1339 vgabios.c:827
    mul bx                                    ; f7 e3                       ; 0xc133b
    mov di, ax                                ; 89 c7                       ; 0xc133d
    sal ax, 1                                 ; d1 e0                       ; 0xc133f
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc1341
    mov bl, byte [bp-00ch]                    ; 8a 5e f4                    ; 0xc1343
    xor bh, bh                                ; 30 ff                       ; 0xc1346
    inc ax                                    ; 40                          ; 0xc1348
    mul bx                                    ; f7 e3                       ; 0xc1349
    mov cx, ax                                ; 89 c1                       ; 0xc134b
    mov si, strict word 0004eh                ; be 4e 00                    ; 0xc134d vgabios.c:52
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc1350
    or di, 000ffh                             ; 81 cf ff 00                 ; 0xc1353 vgabios.c:831
    lea ax, [di+001h]                         ; 8d 45 01                    ; 0xc1357
    mul bx                                    ; f7 e3                       ; 0xc135a
    jmp short 0136fh                          ; eb 11                       ; 0xc135c vgabios.c:833
    mov bl, byte [bx+0482eh]                  ; 8a 9f 2e 48                 ; 0xc135e vgabios.c:835
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc1362
    sal bx, CL                                ; d3 e3                       ; 0xc1364
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1366
    xor ah, ah                                ; 30 e4                       ; 0xc1369
    mul word [bx+04845h]                      ; f7 a7 45 48                 ; 0xc136b
    mov cx, ax                                ; 89 c1                       ; 0xc136f
    mov bx, strict word 00063h                ; bb 63 00                    ; 0xc1371 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1374
    mov es, ax                                ; 8e c0                       ; 0xc1377
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc1379
    mov AL, strict byte 00ch                  ; b0 0c                       ; 0xc137c vgabios.c:840
    mov dx, bx                                ; 89 da                       ; 0xc137e
    out DX, AL                                ; ee                          ; 0xc1380
    mov al, ch                                ; 88 e8                       ; 0xc1381 vgabios.c:841
    lea si, [bx+001h]                         ; 8d 77 01                    ; 0xc1383
    mov dx, si                                ; 89 f2                       ; 0xc1386
    out DX, AL                                ; ee                          ; 0xc1388
    mov AL, strict byte 00dh                  ; b0 0d                       ; 0xc1389 vgabios.c:842
    mov dx, bx                                ; 89 da                       ; 0xc138b
    out DX, AL                                ; ee                          ; 0xc138d
    xor ch, ch                                ; 30 ed                       ; 0xc138e vgabios.c:843
    mov ax, cx                                ; 89 c8                       ; 0xc1390
    mov dx, si                                ; 89 f2                       ; 0xc1392
    out DX, AL                                ; ee                          ; 0xc1394
    mov bx, strict word 00062h                ; bb 62 00                    ; 0xc1395 vgabios.c:42
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1398
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc139b
    mov dx, word [bp-010h]                    ; 8b 56 f0                    ; 0xc139e vgabios.c:853
    call 0124ch                               ; e8 a8 fe                    ; 0xc13a1
    lea sp, [bp-00ah]                         ; 8d 66 f6                    ; 0xc13a4 vgabios.c:854
    pop di                                    ; 5f                          ; 0xc13a7
    pop si                                    ; 5e                          ; 0xc13a8
    pop dx                                    ; 5a                          ; 0xc13a9
    pop cx                                    ; 59                          ; 0xc13aa
    pop bx                                    ; 5b                          ; 0xc13ab
    pop bp                                    ; 5d                          ; 0xc13ac
    retn                                      ; c3                          ; 0xc13ad
  ; disGetNextSymbol 0xc13ae LB 0x2efb -> off=0x0 cb=0000000000000397 uValue=00000000000c13ae 'biosfn_set_video_mode'
biosfn_set_video_mode:                       ; 0xc13ae LB 0x397
    push bp                                   ; 55                          ; 0xc13ae vgabios.c:874
    mov bp, sp                                ; 89 e5                       ; 0xc13af
    push bx                                   ; 53                          ; 0xc13b1
    push cx                                   ; 51                          ; 0xc13b2
    push dx                                   ; 52                          ; 0xc13b3
    push si                                   ; 56                          ; 0xc13b4
    push di                                   ; 57                          ; 0xc13b5
    sub sp, strict byte 00012h                ; 83 ec 12                    ; 0xc13b6
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc13b9
    and AL, strict byte 080h                  ; 24 80                       ; 0xc13bc vgabios.c:878
    mov byte [bp-012h], al                    ; 88 46 ee                    ; 0xc13be
    call 007e8h                               ; e8 24 f4                    ; 0xc13c1 vgabios.c:885
    test ax, ax                               ; 85 c0                       ; 0xc13c4
    je short 013d4h                           ; 74 0c                       ; 0xc13c6
    mov AL, strict byte 007h                  ; b0 07                       ; 0xc13c8 vgabios.c:887
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc13ca
    out DX, AL                                ; ee                          ; 0xc13cd
    xor al, al                                ; 30 c0                       ; 0xc13ce vgabios.c:888
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc13d0
    out DX, AL                                ; ee                          ; 0xc13d3
    and byte [bp-00ch], 07fh                  ; 80 66 f4 7f                 ; 0xc13d4 vgabios.c:893
    cmp byte [bp-00ch], 007h                  ; 80 7e f4 07                 ; 0xc13d8 vgabios.c:897
    jne short 013e2h                          ; 75 04                       ; 0xc13dc
    mov byte [bp-00ch], 000h                  ; c6 46 f4 00                 ; 0xc13de vgabios.c:898
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc13e2 vgabios.c:901
    xor ah, ah                                ; 30 e4                       ; 0xc13e5
    call 03651h                               ; e8 67 22                    ; 0xc13e7
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc13ea
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc13ed vgabios.c:907
    je short 01456h                           ; 74 65                       ; 0xc13ef
    mov dl, al                                ; 88 c2                       ; 0xc13f1 vgabios.c:910
    xor dh, dh                                ; 30 f6                       ; 0xc13f3
    mov bx, dx                                ; 89 d3                       ; 0xc13f5
    mov al, byte [bx+0482eh]                  ; 8a 87 2e 48                 ; 0xc13f7
    mov byte [bp-00eh], al                    ; 88 46 f2                    ; 0xc13fb
    mov bl, al                                ; 88 c3                       ; 0xc13fe vgabios.c:911
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc1400
    sal bx, CL                                ; d3 e3                       ; 0xc1402
    mov al, byte [bx+04842h]                  ; 8a 87 42 48                 ; 0xc1404
    xor ah, ah                                ; 30 e4                       ; 0xc1408
    mov word [bp-014h], ax                    ; 89 46 ec                    ; 0xc140a
    mov al, byte [bx+04843h]                  ; 8a 87 43 48                 ; 0xc140d vgabios.c:912
    mov word [bp-018h], ax                    ; 89 46 e8                    ; 0xc1411
    mov al, byte [bx+04844h]                  ; 8a 87 44 48                 ; 0xc1414 vgabios.c:913
    mov word [bp-01ah], ax                    ; 89 46 e6                    ; 0xc1418
    mov bx, 00089h                            ; bb 89 00                    ; 0xc141b vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc141e
    mov es, ax                                ; 8e c0                       ; 0xc1421
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc1423
    mov ch, al                                ; 88 c5                       ; 0xc1426 vgabios.c:38
    test AL, strict byte 008h                 ; a8 08                       ; 0xc1428 vgabios.c:928
    jne short 01473h                          ; 75 47                       ; 0xc142a
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc142c vgabios.c:930
    mov bx, dx                                ; 89 d3                       ; 0xc142e
    sal bx, CL                                ; d3 e3                       ; 0xc1430
    mov al, byte [bx+047b4h]                  ; 8a 87 b4 47                 ; 0xc1432
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc1436
    out DX, AL                                ; ee                          ; 0xc1439
    xor al, al                                ; 30 c0                       ; 0xc143a vgabios.c:933
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc143c
    out DX, AL                                ; ee                          ; 0xc143f
    mov bl, byte [bx+047b5h]                  ; 8a 9f b5 47                 ; 0xc1440 vgabios.c:936
    cmp bl, 001h                              ; 80 fb 01                    ; 0xc1444
    jc short 01459h                           ; 72 10                       ; 0xc1447
    jbe short 01462h                          ; 76 17                       ; 0xc1449
    cmp bl, cl                                ; 38 cb                       ; 0xc144b
    je short 0146ch                           ; 74 1d                       ; 0xc144d
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc144f
    je short 01467h                           ; 74 13                       ; 0xc1452
    jmp short 0146fh                          ; eb 19                       ; 0xc1454
    jmp near 0173bh                           ; e9 e2 02                    ; 0xc1456
    test bl, bl                               ; 84 db                       ; 0xc1459
    jne short 0146fh                          ; 75 12                       ; 0xc145b
    mov di, 04fc2h                            ; bf c2 4f                    ; 0xc145d vgabios.c:938
    jmp short 0146fh                          ; eb 0d                       ; 0xc1460 vgabios.c:939
    mov di, 05082h                            ; bf 82 50                    ; 0xc1462 vgabios.c:941
    jmp short 0146fh                          ; eb 08                       ; 0xc1465 vgabios.c:942
    mov di, 05142h                            ; bf 42 51                    ; 0xc1467 vgabios.c:944
    jmp short 0146fh                          ; eb 03                       ; 0xc146a vgabios.c:945
    mov di, 05202h                            ; bf 02 52                    ; 0xc146c vgabios.c:947
    xor bx, bx                                ; 31 db                       ; 0xc146f vgabios.c:951
    jmp short 0147bh                          ; eb 08                       ; 0xc1471
    jmp short 014c7h                          ; eb 52                       ; 0xc1473
    cmp bx, 00100h                            ; 81 fb 00 01                 ; 0xc1475
    jnc short 014bah                          ; 73 3f                       ; 0xc1479
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc147b vgabios.c:952
    xor ah, ah                                ; 30 e4                       ; 0xc147e
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc1480
    mov si, ax                                ; 89 c6                       ; 0xc1482
    sal si, CL                                ; d3 e6                       ; 0xc1484
    mov al, byte [si+047b5h]                  ; 8a 84 b5 47                 ; 0xc1486
    mov si, ax                                ; 89 c6                       ; 0xc148a
    mov al, byte [si+0483eh]                  ; 8a 84 3e 48                 ; 0xc148c
    cmp bx, ax                                ; 39 c3                       ; 0xc1490
    jnbe short 014afh                         ; 77 1b                       ; 0xc1492
    mov ax, bx                                ; 89 d8                       ; 0xc1494 vgabios.c:953
    mov dx, strict word 00003h                ; ba 03 00                    ; 0xc1496
    mul dx                                    ; f7 e2                       ; 0xc1499
    mov si, di                                ; 89 fe                       ; 0xc149b
    add si, ax                                ; 01 c6                       ; 0xc149d
    mov al, byte [si]                         ; 8a 04                       ; 0xc149f
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc14a1
    out DX, AL                                ; ee                          ; 0xc14a4
    mov al, byte [si+001h]                    ; 8a 44 01                    ; 0xc14a5 vgabios.c:954
    out DX, AL                                ; ee                          ; 0xc14a8
    mov al, byte [si+002h]                    ; 8a 44 02                    ; 0xc14a9 vgabios.c:955
    out DX, AL                                ; ee                          ; 0xc14ac
    jmp short 014b7h                          ; eb 08                       ; 0xc14ad vgabios.c:957
    xor al, al                                ; 30 c0                       ; 0xc14af vgabios.c:958
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc14b1
    out DX, AL                                ; ee                          ; 0xc14b4
    out DX, AL                                ; ee                          ; 0xc14b5 vgabios.c:959
    out DX, AL                                ; ee                          ; 0xc14b6 vgabios.c:960
    inc bx                                    ; 43                          ; 0xc14b7 vgabios.c:962
    jmp short 01475h                          ; eb bb                       ; 0xc14b8
    test ch, 002h                             ; f6 c5 02                    ; 0xc14ba vgabios.c:963
    je short 014c7h                           ; 74 08                       ; 0xc14bd
    mov dx, 00100h                            ; ba 00 01                    ; 0xc14bf vgabios.c:965
    xor ax, ax                                ; 31 c0                       ; 0xc14c2
    call 010b1h                               ; e8 ea fb                    ; 0xc14c4
    mov dx, 003dah                            ; ba da 03                    ; 0xc14c7 vgabios.c:970
    in AL, DX                                 ; ec                          ; 0xc14ca
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc14cb
    xor bx, bx                                ; 31 db                       ; 0xc14cd vgabios.c:973
    jmp short 014d6h                          ; eb 05                       ; 0xc14cf
    cmp bx, strict byte 00013h                ; 83 fb 13                    ; 0xc14d1
    jnbe short 014f1h                         ; 77 1b                       ; 0xc14d4
    mov al, bl                                ; 88 d8                       ; 0xc14d6 vgabios.c:974
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc14d8
    out DX, AL                                ; ee                          ; 0xc14db
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc14dc vgabios.c:975
    xor ah, ah                                ; 30 e4                       ; 0xc14df
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc14e1
    mov si, ax                                ; 89 c6                       ; 0xc14e3
    sal si, CL                                ; d3 e6                       ; 0xc14e5
    add si, bx                                ; 01 de                       ; 0xc14e7
    mov al, byte [si+04865h]                  ; 8a 84 65 48                 ; 0xc14e9
    out DX, AL                                ; ee                          ; 0xc14ed
    inc bx                                    ; 43                          ; 0xc14ee vgabios.c:976
    jmp short 014d1h                          ; eb e0                       ; 0xc14ef
    mov AL, strict byte 014h                  ; b0 14                       ; 0xc14f1 vgabios.c:977
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc14f3
    out DX, AL                                ; ee                          ; 0xc14f6
    xor al, al                                ; 30 c0                       ; 0xc14f7 vgabios.c:978
    out DX, AL                                ; ee                          ; 0xc14f9
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc14fa vgabios.c:981
    out DX, AL                                ; ee                          ; 0xc14fd
    mov AL, strict byte 003h                  ; b0 03                       ; 0xc14fe vgabios.c:982
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc1500
    out DX, AL                                ; ee                          ; 0xc1503
    mov bx, strict word 00001h                ; bb 01 00                    ; 0xc1504 vgabios.c:983
    jmp short 0150eh                          ; eb 05                       ; 0xc1507
    cmp bx, strict byte 00004h                ; 83 fb 04                    ; 0xc1509
    jnbe short 0152ch                         ; 77 1e                       ; 0xc150c
    mov al, bl                                ; 88 d8                       ; 0xc150e vgabios.c:984
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc1510
    out DX, AL                                ; ee                          ; 0xc1513
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1514 vgabios.c:985
    xor ah, ah                                ; 30 e4                       ; 0xc1517
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc1519
    mov si, ax                                ; 89 c6                       ; 0xc151b
    sal si, CL                                ; d3 e6                       ; 0xc151d
    add si, bx                                ; 01 de                       ; 0xc151f
    mov al, byte [si+04846h]                  ; 8a 84 46 48                 ; 0xc1521
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc1525
    out DX, AL                                ; ee                          ; 0xc1528
    inc bx                                    ; 43                          ; 0xc1529 vgabios.c:986
    jmp short 01509h                          ; eb dd                       ; 0xc152a
    xor bx, bx                                ; 31 db                       ; 0xc152c vgabios.c:989
    jmp short 01535h                          ; eb 05                       ; 0xc152e
    cmp bx, strict byte 00008h                ; 83 fb 08                    ; 0xc1530
    jnbe short 01553h                         ; 77 1e                       ; 0xc1533
    mov al, bl                                ; 88 d8                       ; 0xc1535 vgabios.c:990
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1537
    out DX, AL                                ; ee                          ; 0xc153a
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc153b vgabios.c:991
    xor ah, ah                                ; 30 e4                       ; 0xc153e
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc1540
    mov si, ax                                ; 89 c6                       ; 0xc1542
    sal si, CL                                ; d3 e6                       ; 0xc1544
    add si, bx                                ; 01 de                       ; 0xc1546
    mov al, byte [si+04879h]                  ; 8a 84 79 48                 ; 0xc1548
    mov dx, 003cfh                            ; ba cf 03                    ; 0xc154c
    out DX, AL                                ; ee                          ; 0xc154f
    inc bx                                    ; 43                          ; 0xc1550 vgabios.c:992
    jmp short 01530h                          ; eb dd                       ; 0xc1551
    mov bl, byte [bp-010h]                    ; 8a 5e f0                    ; 0xc1553 vgabios.c:995
    xor bh, bh                                ; 30 ff                       ; 0xc1556
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc1558
    sal bx, CL                                ; d3 e3                       ; 0xc155a
    cmp byte [bx+047b0h], 001h                ; 80 bf b0 47 01              ; 0xc155c
    jne short 01568h                          ; 75 05                       ; 0xc1561
    mov dx, 003b4h                            ; ba b4 03                    ; 0xc1563
    jmp short 0156bh                          ; eb 03                       ; 0xc1566
    mov dx, 003d4h                            ; ba d4 03                    ; 0xc1568
    mov word [bp-016h], dx                    ; 89 56 ea                    ; 0xc156b
    mov ax, strict word 00011h                ; b8 11 00                    ; 0xc156e vgabios.c:998
    out DX, ax                                ; ef                          ; 0xc1571
    xor bx, bx                                ; 31 db                       ; 0xc1572 vgabios.c:1000
    jmp short 0157bh                          ; eb 05                       ; 0xc1574
    cmp bx, strict byte 00018h                ; 83 fb 18                    ; 0xc1576
    jnbe short 01599h                         ; 77 1e                       ; 0xc1579
    mov al, bl                                ; 88 d8                       ; 0xc157b vgabios.c:1001
    mov dx, word [bp-016h]                    ; 8b 56 ea                    ; 0xc157d
    out DX, AL                                ; ee                          ; 0xc1580
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1581 vgabios.c:1002
    xor ah, ah                                ; 30 e4                       ; 0xc1584
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc1586
    mov si, ax                                ; 89 c6                       ; 0xc1588
    sal si, CL                                ; d3 e6                       ; 0xc158a
    mov di, si                                ; 89 f7                       ; 0xc158c
    add di, bx                                ; 01 df                       ; 0xc158e
    inc dx                                    ; 42                          ; 0xc1590
    mov al, byte [di+0484ch]                  ; 8a 85 4c 48                 ; 0xc1591
    out DX, AL                                ; ee                          ; 0xc1595
    inc bx                                    ; 43                          ; 0xc1596 vgabios.c:1003
    jmp short 01576h                          ; eb dd                       ; 0xc1597
    mov al, byte [si+0484bh]                  ; 8a 84 4b 48                 ; 0xc1599 vgabios.c:1006
    mov dx, 003c2h                            ; ba c2 03                    ; 0xc159d
    out DX, AL                                ; ee                          ; 0xc15a0
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc15a1 vgabios.c:1009
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc15a3
    out DX, AL                                ; ee                          ; 0xc15a6
    mov dx, 003dah                            ; ba da 03                    ; 0xc15a7 vgabios.c:1010
    in AL, DX                                 ; ec                          ; 0xc15aa
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc15ab
    cmp byte [bp-012h], 000h                  ; 80 7e ee 00                 ; 0xc15ad vgabios.c:1012
    jne short 01614h                          ; 75 61                       ; 0xc15b1
    mov bl, byte [bp-010h]                    ; 8a 5e f0                    ; 0xc15b3 vgabios.c:1014
    xor bh, bh                                ; 30 ff                       ; 0xc15b6
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc15b8
    sal bx, CL                                ; d3 e3                       ; 0xc15ba
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc15bc
    jne short 015d6h                          ; 75 13                       ; 0xc15c1
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc15c3 vgabios.c:1016
    mov cx, 04000h                            ; b9 00 40                    ; 0xc15c7
    mov ax, 00720h                            ; b8 20 07                    ; 0xc15ca
    xor di, di                                ; 31 ff                       ; 0xc15cd
    cld                                       ; fc                          ; 0xc15cf
    jcxz 015d4h                               ; e3 02                       ; 0xc15d0
    rep stosw                                 ; f3 ab                       ; 0xc15d2
    jmp short 01614h                          ; eb 3e                       ; 0xc15d4 vgabios.c:1018
    cmp byte [bp-00ch], 00dh                  ; 80 7e f4 0d                 ; 0xc15d6 vgabios.c:1020
    jnc short 015eeh                          ; 73 12                       ; 0xc15da
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc15dc vgabios.c:1022
    mov cx, 04000h                            ; b9 00 40                    ; 0xc15e0
    xor ax, ax                                ; 31 c0                       ; 0xc15e3
    xor di, di                                ; 31 ff                       ; 0xc15e5
    cld                                       ; fc                          ; 0xc15e7
    jcxz 015ech                               ; e3 02                       ; 0xc15e8
    rep stosw                                 ; f3 ab                       ; 0xc15ea
    jmp short 01614h                          ; eb 26                       ; 0xc15ec vgabios.c:1024
    mov AL, strict byte 002h                  ; b0 02                       ; 0xc15ee vgabios.c:1026
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc15f0
    out DX, AL                                ; ee                          ; 0xc15f3
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc15f4 vgabios.c:1027
    in AL, DX                                 ; ec                          ; 0xc15f7
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc15f8
    mov word [bp-01ch], ax                    ; 89 46 e4                    ; 0xc15fa
    mov AL, strict byte 00fh                  ; b0 0f                       ; 0xc15fd vgabios.c:1028
    out DX, AL                                ; ee                          ; 0xc15ff
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc1600 vgabios.c:1029
    mov cx, 08000h                            ; b9 00 80                    ; 0xc1604
    xor ax, ax                                ; 31 c0                       ; 0xc1607
    xor di, di                                ; 31 ff                       ; 0xc1609
    cld                                       ; fc                          ; 0xc160b
    jcxz 01610h                               ; e3 02                       ; 0xc160c
    rep stosw                                 ; f3 ab                       ; 0xc160e
    mov al, byte [bp-01ch]                    ; 8a 46 e4                    ; 0xc1610 vgabios.c:1030
    out DX, AL                                ; ee                          ; 0xc1613
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc1614 vgabios.c:42
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1617
    mov es, ax                                ; 8e c0                       ; 0xc161a
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc161c
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc161f
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc1622 vgabios.c:52
    mov ax, word [bp-014h]                    ; 8b 46 ec                    ; 0xc1625
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc1628
    mov bl, byte [bp-00eh]                    ; 8a 5e f2                    ; 0xc162b vgabios.c:1038
    xor bh, bh                                ; 30 ff                       ; 0xc162e
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc1630
    sal bx, CL                                ; d3 e3                       ; 0xc1632
    mov ax, word [bx+04845h]                  ; 8b 87 45 48                 ; 0xc1634 vgabios.c:50
    mov bx, strict word 0004ch                ; bb 4c 00                    ; 0xc1638 vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc163b
    mov bx, strict word 00063h                ; bb 63 00                    ; 0xc163e vgabios.c:52
    mov ax, word [bp-016h]                    ; 8b 46 ea                    ; 0xc1641
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc1644
    mov bx, 00084h                            ; bb 84 00                    ; 0xc1647 vgabios.c:42
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc164a
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc164d
    mov bx, 00085h                            ; bb 85 00                    ; 0xc1650 vgabios.c:52
    mov ax, word [bp-01ah]                    ; 8b 46 e6                    ; 0xc1653
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc1656
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc1659 vgabios.c:1042
    or AL, strict byte 060h                   ; 0c 60                       ; 0xc165c
    mov bx, 00087h                            ; bb 87 00                    ; 0xc165e vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc1661
    mov bx, 00088h                            ; bb 88 00                    ; 0xc1664 vgabios.c:42
    mov byte [es:bx], 0f9h                    ; 26 c6 07 f9                 ; 0xc1667
    mov bx, 00089h                            ; bb 89 00                    ; 0xc166b vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc166e
    and AL, strict byte 07fh                  ; 24 7f                       ; 0xc1671 vgabios.c:38
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc1673 vgabios.c:42
    mov bx, 0008ah                            ; bb 8a 00                    ; 0xc1676 vgabios.c:42
    mov byte [es:bx], 008h                    ; 26 c6 07 08                 ; 0xc1679
    mov ax, ds                                ; 8c d8                       ; 0xc167d vgabios.c:1048
    mov bx, 000a8h                            ; bb a8 00                    ; 0xc167f vgabios.c:62
    mov word [es:bx], 05550h                  ; 26 c7 07 50 55              ; 0xc1682
    mov [es:bx+002h], ds                      ; 26 8c 5f 02                 ; 0xc1687
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc168b vgabios.c:1050
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc168e
    jnbe short 016b8h                         ; 77 26                       ; 0xc1690
    mov bl, al                                ; 88 c3                       ; 0xc1692 vgabios.c:1052
    xor bh, bh                                ; 30 ff                       ; 0xc1694
    mov al, byte [bx+07dddh]                  ; 8a 87 dd 7d                 ; 0xc1696 vgabios.c:40
    mov bx, strict word 00065h                ; bb 65 00                    ; 0xc169a vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc169d
    cmp cl, byte [bp-00ch]                    ; 3a 4e f4                    ; 0xc16a0 vgabios.c:1053
    jne short 016aah                          ; 75 05                       ; 0xc16a3
    mov ax, strict word 0003fh                ; b8 3f 00                    ; 0xc16a5
    jmp short 016adh                          ; eb 03                       ; 0xc16a8
    mov ax, strict word 00030h                ; b8 30 00                    ; 0xc16aa
    mov bx, strict word 00066h                ; bb 66 00                    ; 0xc16ad vgabios.c:42
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc16b0
    mov es, dx                                ; 8e c2                       ; 0xc16b3
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc16b5
    mov bl, byte [bp-010h]                    ; 8a 5e f0                    ; 0xc16b8 vgabios.c:1057
    xor bh, bh                                ; 30 ff                       ; 0xc16bb
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc16bd
    sal bx, CL                                ; d3 e3                       ; 0xc16bf
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc16c1
    jne short 016d1h                          ; 75 09                       ; 0xc16c6
    mov dx, strict word 00007h                ; ba 07 00                    ; 0xc16c8 vgabios.c:1059
    mov ax, strict word 00006h                ; b8 06 00                    ; 0xc16cb
    call 01150h                               ; e8 7f fa                    ; 0xc16ce
    xor bx, bx                                ; 31 db                       ; 0xc16d1 vgabios.c:1063
    jmp short 016dah                          ; eb 05                       ; 0xc16d3
    cmp bx, strict byte 00008h                ; 83 fb 08                    ; 0xc16d5
    jnc short 016e6h                          ; 73 0c                       ; 0xc16d8
    mov al, bl                                ; 88 d8                       ; 0xc16da vgabios.c:1064
    xor ah, ah                                ; 30 e4                       ; 0xc16dc
    xor dx, dx                                ; 31 d2                       ; 0xc16de
    call 0124ch                               ; e8 69 fb                    ; 0xc16e0
    inc bx                                    ; 43                          ; 0xc16e3
    jmp short 016d5h                          ; eb ef                       ; 0xc16e4
    xor ax, ax                                ; 31 c0                       ; 0xc16e6 vgabios.c:1067
    call 012d9h                               ; e8 ee fb                    ; 0xc16e8
    mov bl, byte [bp-010h]                    ; 8a 5e f0                    ; 0xc16eb vgabios.c:1070
    xor bh, bh                                ; 30 ff                       ; 0xc16ee
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc16f0
    sal bx, CL                                ; d3 e3                       ; 0xc16f2
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc16f4
    jne short 0170bh                          ; 75 10                       ; 0xc16f9
    xor bl, bl                                ; 30 db                       ; 0xc16fb vgabios.c:1072
    mov AL, strict byte 004h                  ; b0 04                       ; 0xc16fd
    mov AH, strict byte 011h                  ; b4 11                       ; 0xc16ff
    int 010h                                  ; cd 10                       ; 0xc1701
    xor bl, bl                                ; 30 db                       ; 0xc1703 vgabios.c:1073
    mov al, cl                                ; 88 c8                       ; 0xc1705
    mov AH, strict byte 011h                  ; b4 11                       ; 0xc1707
    int 010h                                  ; cd 10                       ; 0xc1709
    mov dx, 0596ch                            ; ba 6c 59                    ; 0xc170b vgabios.c:1077
    mov ax, strict word 0001fh                ; b8 1f 00                    ; 0xc170e
    call 00980h                               ; e8 6c f2                    ; 0xc1711
    mov ax, word [bp-01ah]                    ; 8b 46 e6                    ; 0xc1714 vgabios.c:1079
    cmp ax, strict word 00010h                ; 3d 10 00                    ; 0xc1717
    je short 01736h                           ; 74 1a                       ; 0xc171a
    cmp ax, strict word 0000eh                ; 3d 0e 00                    ; 0xc171c
    je short 01731h                           ; 74 10                       ; 0xc171f
    cmp ax, strict word 00008h                ; 3d 08 00                    ; 0xc1721
    jne short 0173bh                          ; 75 15                       ; 0xc1724
    mov dx, 0556ch                            ; ba 6c 55                    ; 0xc1726 vgabios.c:1081
    mov ax, strict word 00043h                ; b8 43 00                    ; 0xc1729
    call 00980h                               ; e8 51 f2                    ; 0xc172c
    jmp short 0173bh                          ; eb 0a                       ; 0xc172f vgabios.c:1082
    mov dx, 05d6ch                            ; ba 6c 5d                    ; 0xc1731 vgabios.c:1084
    jmp short 01729h                          ; eb f3                       ; 0xc1734
    mov dx, 06b6ch                            ; ba 6c 6b                    ; 0xc1736 vgabios.c:1087
    jmp short 01729h                          ; eb ee                       ; 0xc1739
    lea sp, [bp-00ah]                         ; 8d 66 f6                    ; 0xc173b vgabios.c:1090
    pop di                                    ; 5f                          ; 0xc173e
    pop si                                    ; 5e                          ; 0xc173f
    pop dx                                    ; 5a                          ; 0xc1740
    pop cx                                    ; 59                          ; 0xc1741
    pop bx                                    ; 5b                          ; 0xc1742
    pop bp                                    ; 5d                          ; 0xc1743
    retn                                      ; c3                          ; 0xc1744
  ; disGetNextSymbol 0xc1745 LB 0x2b64 -> off=0x0 cb=000000000000008f uValue=00000000000c1745 'vgamem_copy_pl4'
vgamem_copy_pl4:                             ; 0xc1745 LB 0x8f
    push bp                                   ; 55                          ; 0xc1745 vgabios.c:1093
    mov bp, sp                                ; 89 e5                       ; 0xc1746
    push si                                   ; 56                          ; 0xc1748
    push di                                   ; 57                          ; 0xc1749
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc174a
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc174d
    mov al, dl                                ; 88 d0                       ; 0xc1750
    mov byte [bp-00ah], bl                    ; 88 5e f6                    ; 0xc1752
    mov byte [bp-006h], cl                    ; 88 4e fa                    ; 0xc1755
    xor ah, ah                                ; 30 e4                       ; 0xc1758 vgabios.c:1099
    mov dl, byte [bp+006h]                    ; 8a 56 06                    ; 0xc175a
    xor dh, dh                                ; 30 f6                       ; 0xc175d
    mov cx, dx                                ; 89 d1                       ; 0xc175f
    imul dx                                   ; f7 ea                       ; 0xc1761
    mov dl, byte [bp+004h]                    ; 8a 56 04                    ; 0xc1763
    xor dh, dh                                ; 30 f6                       ; 0xc1766
    mov si, dx                                ; 89 d6                       ; 0xc1768
    imul dx                                   ; f7 ea                       ; 0xc176a
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc176c
    xor dh, dh                                ; 30 f6                       ; 0xc176f
    mov bx, dx                                ; 89 d3                       ; 0xc1771
    add ax, dx                                ; 01 d0                       ; 0xc1773
    mov word [bp-00eh], ax                    ; 89 46 f2                    ; 0xc1775
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc1778 vgabios.c:1100
    xor ah, ah                                ; 30 e4                       ; 0xc177b
    imul cx                                   ; f7 e9                       ; 0xc177d
    imul si                                   ; f7 ee                       ; 0xc177f
    add ax, bx                                ; 01 d8                       ; 0xc1781
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc1783
    mov ax, 00105h                            ; b8 05 01                    ; 0xc1786 vgabios.c:1101
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1789
    out DX, ax                                ; ef                          ; 0xc178c
    xor bl, bl                                ; 30 db                       ; 0xc178d vgabios.c:1102
    cmp bl, byte [bp+006h]                    ; 3a 5e 06                    ; 0xc178f
    jnc short 017c4h                          ; 73 30                       ; 0xc1792
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1794 vgabios.c:1104
    xor ah, ah                                ; 30 e4                       ; 0xc1797
    mov cx, ax                                ; 89 c1                       ; 0xc1799
    mov al, bl                                ; 88 d8                       ; 0xc179b
    mov dx, ax                                ; 89 c2                       ; 0xc179d
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc179f
    mov si, ax                                ; 89 c6                       ; 0xc17a2
    mov ax, dx                                ; 89 d0                       ; 0xc17a4
    imul si                                   ; f7 ee                       ; 0xc17a6
    mov si, word [bp-00eh]                    ; 8b 76 f2                    ; 0xc17a8
    add si, ax                                ; 01 c6                       ; 0xc17ab
    mov di, word [bp-00ch]                    ; 8b 7e f4                    ; 0xc17ad
    add di, ax                                ; 01 c7                       ; 0xc17b0
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc17b2
    mov es, dx                                ; 8e c2                       ; 0xc17b5
    cld                                       ; fc                          ; 0xc17b7
    jcxz 017c0h                               ; e3 06                       ; 0xc17b8
    push DS                                   ; 1e                          ; 0xc17ba
    mov ds, dx                                ; 8e da                       ; 0xc17bb
    rep movsb                                 ; f3 a4                       ; 0xc17bd
    pop DS                                    ; 1f                          ; 0xc17bf
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3                     ; 0xc17c0 vgabios.c:1105
    jmp short 0178fh                          ; eb cb                       ; 0xc17c2
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc17c4 vgabios.c:1106
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc17c7
    out DX, ax                                ; ef                          ; 0xc17ca
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc17cb vgabios.c:1107
    pop di                                    ; 5f                          ; 0xc17ce
    pop si                                    ; 5e                          ; 0xc17cf
    pop bp                                    ; 5d                          ; 0xc17d0
    retn 00004h                               ; c2 04 00                    ; 0xc17d1
  ; disGetNextSymbol 0xc17d4 LB 0x2ad5 -> off=0x0 cb=000000000000007c uValue=00000000000c17d4 'vgamem_fill_pl4'
vgamem_fill_pl4:                             ; 0xc17d4 LB 0x7c
    push bp                                   ; 55                          ; 0xc17d4 vgabios.c:1110
    mov bp, sp                                ; 89 e5                       ; 0xc17d5
    push si                                   ; 56                          ; 0xc17d7
    push di                                   ; 57                          ; 0xc17d8
    sub sp, strict byte 00006h                ; 83 ec 06                    ; 0xc17d9
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc17dc
    mov al, dl                                ; 88 d0                       ; 0xc17df
    mov byte [bp-006h], bl                    ; 88 5e fa                    ; 0xc17e1
    mov bh, cl                                ; 88 cf                       ; 0xc17e4
    xor ah, ah                                ; 30 e4                       ; 0xc17e6 vgabios.c:1116
    mov dx, ax                                ; 89 c2                       ; 0xc17e8
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc17ea
    mov cx, ax                                ; 89 c1                       ; 0xc17ed
    mov ax, dx                                ; 89 d0                       ; 0xc17ef
    imul cx                                   ; f7 e9                       ; 0xc17f1
    mov dl, bh                                ; 88 fa                       ; 0xc17f3
    xor dh, dh                                ; 30 f6                       ; 0xc17f5
    imul dx                                   ; f7 ea                       ; 0xc17f7
    mov dx, ax                                ; 89 c2                       ; 0xc17f9
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc17fb
    xor ah, ah                                ; 30 e4                       ; 0xc17fe
    add dx, ax                                ; 01 c2                       ; 0xc1800
    mov word [bp-00ah], dx                    ; 89 56 f6                    ; 0xc1802
    mov ax, 00205h                            ; b8 05 02                    ; 0xc1805 vgabios.c:1117
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1808
    out DX, ax                                ; ef                          ; 0xc180b
    xor bl, bl                                ; 30 db                       ; 0xc180c vgabios.c:1118
    cmp bl, byte [bp+004h]                    ; 3a 5e 04                    ; 0xc180e
    jnc short 01840h                          ; 73 2d                       ; 0xc1811
    mov cl, byte [bp-006h]                    ; 8a 4e fa                    ; 0xc1813 vgabios.c:1120
    xor ch, ch                                ; 30 ed                       ; 0xc1816
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc1818
    xor ah, ah                                ; 30 e4                       ; 0xc181b
    mov si, ax                                ; 89 c6                       ; 0xc181d
    mov al, bl                                ; 88 d8                       ; 0xc181f
    mov dx, ax                                ; 89 c2                       ; 0xc1821
    mov al, bh                                ; 88 f8                       ; 0xc1823
    mov di, ax                                ; 89 c7                       ; 0xc1825
    mov ax, dx                                ; 89 d0                       ; 0xc1827
    imul di                                   ; f7 ef                       ; 0xc1829
    mov di, word [bp-00ah]                    ; 8b 7e f6                    ; 0xc182b
    add di, ax                                ; 01 c7                       ; 0xc182e
    mov ax, si                                ; 89 f0                       ; 0xc1830
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc1832
    mov es, dx                                ; 8e c2                       ; 0xc1835
    cld                                       ; fc                          ; 0xc1837
    jcxz 0183ch                               ; e3 02                       ; 0xc1838
    rep stosb                                 ; f3 aa                       ; 0xc183a
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3                     ; 0xc183c vgabios.c:1121
    jmp short 0180eh                          ; eb ce                       ; 0xc183e
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc1840 vgabios.c:1122
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1843
    out DX, ax                                ; ef                          ; 0xc1846
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc1847 vgabios.c:1123
    pop di                                    ; 5f                          ; 0xc184a
    pop si                                    ; 5e                          ; 0xc184b
    pop bp                                    ; 5d                          ; 0xc184c
    retn 00004h                               ; c2 04 00                    ; 0xc184d
  ; disGetNextSymbol 0xc1850 LB 0x2a59 -> off=0x0 cb=00000000000000b8 uValue=00000000000c1850 'vgamem_copy_cga'
vgamem_copy_cga:                             ; 0xc1850 LB 0xb8
    push bp                                   ; 55                          ; 0xc1850 vgabios.c:1126
    mov bp, sp                                ; 89 e5                       ; 0xc1851
    push si                                   ; 56                          ; 0xc1853
    push di                                   ; 57                          ; 0xc1854
    sub sp, strict byte 0000eh                ; 83 ec 0e                    ; 0xc1855
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc1858
    mov byte [bp-008h], bl                    ; 88 5e f8                    ; 0xc185b
    mov byte [bp-00ah], cl                    ; 88 4e f6                    ; 0xc185e
    mov al, dl                                ; 88 d0                       ; 0xc1861 vgabios.c:1132
    xor ah, ah                                ; 30 e4                       ; 0xc1863
    mov bx, ax                                ; 89 c3                       ; 0xc1865
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc1867
    mov si, ax                                ; 89 c6                       ; 0xc186a
    mov ax, bx                                ; 89 d8                       ; 0xc186c
    imul si                                   ; f7 ee                       ; 0xc186e
    mov bl, byte [bp+004h]                    ; 8a 5e 04                    ; 0xc1870
    mov di, bx                                ; 89 df                       ; 0xc1873
    imul bx                                   ; f7 eb                       ; 0xc1875
    mov dx, ax                                ; 89 c2                       ; 0xc1877
    sar dx, 1                                 ; d1 fa                       ; 0xc1879
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc187b
    xor ah, ah                                ; 30 e4                       ; 0xc187e
    mov bx, ax                                ; 89 c3                       ; 0xc1880
    add dx, ax                                ; 01 c2                       ; 0xc1882
    mov word [bp-00eh], dx                    ; 89 56 f2                    ; 0xc1884
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1887 vgabios.c:1133
    imul si                                   ; f7 ee                       ; 0xc188a
    imul di                                   ; f7 ef                       ; 0xc188c
    sar ax, 1                                 ; d1 f8                       ; 0xc188e
    add ax, bx                                ; 01 d8                       ; 0xc1890
    mov word [bp-010h], ax                    ; 89 46 f0                    ; 0xc1892
    mov byte [bp-006h], bh                    ; 88 7e fa                    ; 0xc1895 vgabios.c:1134
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc1898
    xor ah, ah                                ; 30 e4                       ; 0xc189b
    cwd                                       ; 99                          ; 0xc189d
    db  02bh, 0c2h
    ; sub ax, dx                                ; 2b c2                     ; 0xc189e
    sar ax, 1                                 ; d1 f8                       ; 0xc18a0
    mov bx, ax                                ; 89 c3                       ; 0xc18a2
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc18a4
    xor ah, ah                                ; 30 e4                       ; 0xc18a7
    cmp ax, bx                                ; 39 d8                       ; 0xc18a9
    jnl short 018ffh                          ; 7d 52                       ; 0xc18ab
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc18ad vgabios.c:1136
    xor bh, bh                                ; 30 ff                       ; 0xc18b0
    mov word [bp-012h], bx                    ; 89 5e ee                    ; 0xc18b2
    mov bl, byte [bp+004h]                    ; 8a 5e 04                    ; 0xc18b5
    imul bx                                   ; f7 eb                       ; 0xc18b8
    mov bx, ax                                ; 89 c3                       ; 0xc18ba
    mov si, word [bp-00eh]                    ; 8b 76 f2                    ; 0xc18bc
    add si, ax                                ; 01 c6                       ; 0xc18bf
    mov di, word [bp-010h]                    ; 8b 7e f0                    ; 0xc18c1
    add di, ax                                ; 01 c7                       ; 0xc18c4
    mov cx, word [bp-012h]                    ; 8b 4e ee                    ; 0xc18c6
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc18c9
    mov es, dx                                ; 8e c2                       ; 0xc18cc
    cld                                       ; fc                          ; 0xc18ce
    jcxz 018d7h                               ; e3 06                       ; 0xc18cf
    push DS                                   ; 1e                          ; 0xc18d1
    mov ds, dx                                ; 8e da                       ; 0xc18d2
    rep movsb                                 ; f3 a4                       ; 0xc18d4
    pop DS                                    ; 1f                          ; 0xc18d6
    mov si, word [bp-00eh]                    ; 8b 76 f2                    ; 0xc18d7 vgabios.c:1137
    add si, 02000h                            ; 81 c6 00 20                 ; 0xc18da
    add si, bx                                ; 01 de                       ; 0xc18de
    mov di, word [bp-010h]                    ; 8b 7e f0                    ; 0xc18e0
    add di, 02000h                            ; 81 c7 00 20                 ; 0xc18e3
    add di, bx                                ; 01 df                       ; 0xc18e7
    mov cx, word [bp-012h]                    ; 8b 4e ee                    ; 0xc18e9
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc18ec
    mov es, dx                                ; 8e c2                       ; 0xc18ef
    cld                                       ; fc                          ; 0xc18f1
    jcxz 018fah                               ; e3 06                       ; 0xc18f2
    push DS                                   ; 1e                          ; 0xc18f4
    mov ds, dx                                ; 8e da                       ; 0xc18f5
    rep movsb                                 ; f3 a4                       ; 0xc18f7
    pop DS                                    ; 1f                          ; 0xc18f9
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc18fa vgabios.c:1138
    jmp short 01898h                          ; eb 99                       ; 0xc18fd
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc18ff vgabios.c:1139
    pop di                                    ; 5f                          ; 0xc1902
    pop si                                    ; 5e                          ; 0xc1903
    pop bp                                    ; 5d                          ; 0xc1904
    retn 00004h                               ; c2 04 00                    ; 0xc1905
  ; disGetNextSymbol 0xc1908 LB 0x29a1 -> off=0x0 cb=0000000000000096 uValue=00000000000c1908 'vgamem_fill_cga'
vgamem_fill_cga:                             ; 0xc1908 LB 0x96
    push bp                                   ; 55                          ; 0xc1908 vgabios.c:1142
    mov bp, sp                                ; 89 e5                       ; 0xc1909
    push si                                   ; 56                          ; 0xc190b
    push di                                   ; 57                          ; 0xc190c
    sub sp, strict byte 0000ch                ; 83 ec 0c                    ; 0xc190d
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc1910
    mov al, dl                                ; 88 d0                       ; 0xc1913
    mov byte [bp-00ch], bl                    ; 88 5e f4                    ; 0xc1915
    mov byte [bp-008h], cl                    ; 88 4e f8                    ; 0xc1918
    xor ah, ah                                ; 30 e4                       ; 0xc191b vgabios.c:1148
    mov dx, ax                                ; 89 c2                       ; 0xc191d
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc191f
    mov bx, ax                                ; 89 c3                       ; 0xc1922
    mov ax, dx                                ; 89 d0                       ; 0xc1924
    imul bx                                   ; f7 eb                       ; 0xc1926
    mov dl, cl                                ; 88 ca                       ; 0xc1928
    xor dh, dh                                ; 30 f6                       ; 0xc192a
    imul dx                                   ; f7 ea                       ; 0xc192c
    mov dx, ax                                ; 89 c2                       ; 0xc192e
    sar dx, 1                                 ; d1 fa                       ; 0xc1930
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc1932
    xor ah, ah                                ; 30 e4                       ; 0xc1935
    add dx, ax                                ; 01 c2                       ; 0xc1937
    mov word [bp-00eh], dx                    ; 89 56 f2                    ; 0xc1939
    mov byte [bp-006h], ah                    ; 88 66 fa                    ; 0xc193c vgabios.c:1149
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc193f
    xor ah, ah                                ; 30 e4                       ; 0xc1942
    cwd                                       ; 99                          ; 0xc1944
    db  02bh, 0c2h
    ; sub ax, dx                                ; 2b c2                     ; 0xc1945
    sar ax, 1                                 ; d1 f8                       ; 0xc1947
    mov dx, ax                                ; 89 c2                       ; 0xc1949
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc194b
    xor ah, ah                                ; 30 e4                       ; 0xc194e
    cmp ax, dx                                ; 39 d0                       ; 0xc1950
    jnl short 01995h                          ; 7d 41                       ; 0xc1952
    mov bl, byte [bp-00ch]                    ; 8a 5e f4                    ; 0xc1954 vgabios.c:1151
    xor bh, bh                                ; 30 ff                       ; 0xc1957
    mov dl, byte [bp+006h]                    ; 8a 56 06                    ; 0xc1959
    xor dh, dh                                ; 30 f6                       ; 0xc195c
    mov si, dx                                ; 89 d6                       ; 0xc195e
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc1960
    imul dx                                   ; f7 ea                       ; 0xc1963
    mov word [bp-010h], ax                    ; 89 46 f0                    ; 0xc1965
    mov di, word [bp-00eh]                    ; 8b 7e f2                    ; 0xc1968
    add di, ax                                ; 01 c7                       ; 0xc196b
    mov cx, bx                                ; 89 d9                       ; 0xc196d
    mov ax, si                                ; 89 f0                       ; 0xc196f
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc1971
    mov es, dx                                ; 8e c2                       ; 0xc1974
    cld                                       ; fc                          ; 0xc1976
    jcxz 0197bh                               ; e3 02                       ; 0xc1977
    rep stosb                                 ; f3 aa                       ; 0xc1979
    mov di, word [bp-00eh]                    ; 8b 7e f2                    ; 0xc197b vgabios.c:1152
    add di, 02000h                            ; 81 c7 00 20                 ; 0xc197e
    add di, word [bp-010h]                    ; 03 7e f0                    ; 0xc1982
    mov cx, bx                                ; 89 d9                       ; 0xc1985
    mov ax, si                                ; 89 f0                       ; 0xc1987
    mov es, dx                                ; 8e c2                       ; 0xc1989
    cld                                       ; fc                          ; 0xc198b
    jcxz 01990h                               ; e3 02                       ; 0xc198c
    rep stosb                                 ; f3 aa                       ; 0xc198e
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc1990 vgabios.c:1153
    jmp short 0193fh                          ; eb aa                       ; 0xc1993
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc1995 vgabios.c:1154
    pop di                                    ; 5f                          ; 0xc1998
    pop si                                    ; 5e                          ; 0xc1999
    pop bp                                    ; 5d                          ; 0xc199a
    retn 00004h                               ; c2 04 00                    ; 0xc199b
  ; disGetNextSymbol 0xc199e LB 0x290b -> off=0x0 cb=0000000000000084 uValue=00000000000c199e 'vgamem_copy_linear'
vgamem_copy_linear:                          ; 0xc199e LB 0x84
    push bp                                   ; 55                          ; 0xc199e vgabios.c:1157
    mov bp, sp                                ; 89 e5                       ; 0xc199f
    push si                                   ; 56                          ; 0xc19a1
    push di                                   ; 57                          ; 0xc19a2
    sub sp, strict byte 0000ch                ; 83 ec 0c                    ; 0xc19a3
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc19a6
    mov al, dl                                ; 88 d0                       ; 0xc19a9
    mov byte [bp-00ah], bl                    ; 88 5e f6                    ; 0xc19ab
    mov bx, cx                                ; 89 cb                       ; 0xc19ae
    xor ah, ah                                ; 30 e4                       ; 0xc19b0 vgabios.c:1163
    mov si, ax                                ; 89 c6                       ; 0xc19b2
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc19b4
    mov word [bp-010h], ax                    ; 89 46 f0                    ; 0xc19b7
    mov ax, si                                ; 89 f0                       ; 0xc19ba
    imul word [bp-010h]                       ; f7 6e f0                    ; 0xc19bc
    mul word [bp+004h]                        ; f7 66 04                    ; 0xc19bf
    mov si, ax                                ; 89 c6                       ; 0xc19c2
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc19c4
    xor ah, ah                                ; 30 e4                       ; 0xc19c7
    mov di, ax                                ; 89 c7                       ; 0xc19c9
    add si, ax                                ; 01 c6                       ; 0xc19cb
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc19cd
    sal si, CL                                ; d3 e6                       ; 0xc19cf
    mov word [bp-00ch], si                    ; 89 76 f4                    ; 0xc19d1
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc19d4 vgabios.c:1164
    imul word [bp-010h]                       ; f7 6e f0                    ; 0xc19d7
    mul word [bp+004h]                        ; f7 66 04                    ; 0xc19da
    add ax, di                                ; 01 f8                       ; 0xc19dd
    sal ax, CL                                ; d3 e0                       ; 0xc19df
    mov word [bp-00eh], ax                    ; 89 46 f2                    ; 0xc19e1
    sal bx, CL                                ; d3 e3                       ; 0xc19e4 vgabios.c:1165
    sal word [bp+004h], CL                    ; d3 66 04                    ; 0xc19e6 vgabios.c:1166
    mov byte [bp-006h], 000h                  ; c6 46 fa 00                 ; 0xc19e9 vgabios.c:1167
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc19ed
    cmp al, byte [bp+006h]                    ; 3a 46 06                    ; 0xc19f0
    jnc short 01a19h                          ; 73 24                       ; 0xc19f3
    xor ah, ah                                ; 30 e4                       ; 0xc19f5 vgabios.c:1169
    mul word [bp+004h]                        ; f7 66 04                    ; 0xc19f7
    mov si, word [bp-00ch]                    ; 8b 76 f4                    ; 0xc19fa
    add si, ax                                ; 01 c6                       ; 0xc19fd
    mov di, word [bp-00eh]                    ; 8b 7e f2                    ; 0xc19ff
    add di, ax                                ; 01 c7                       ; 0xc1a02
    mov cx, bx                                ; 89 d9                       ; 0xc1a04
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc1a06
    mov es, dx                                ; 8e c2                       ; 0xc1a09
    cld                                       ; fc                          ; 0xc1a0b
    jcxz 01a14h                               ; e3 06                       ; 0xc1a0c
    push DS                                   ; 1e                          ; 0xc1a0e
    mov ds, dx                                ; 8e da                       ; 0xc1a0f
    rep movsb                                 ; f3 a4                       ; 0xc1a11
    pop DS                                    ; 1f                          ; 0xc1a13
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc1a14 vgabios.c:1170
    jmp short 019edh                          ; eb d4                       ; 0xc1a17
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc1a19 vgabios.c:1171
    pop di                                    ; 5f                          ; 0xc1a1c
    pop si                                    ; 5e                          ; 0xc1a1d
    pop bp                                    ; 5d                          ; 0xc1a1e
    retn 00004h                               ; c2 04 00                    ; 0xc1a1f
  ; disGetNextSymbol 0xc1a22 LB 0x2887 -> off=0x0 cb=000000000000006d uValue=00000000000c1a22 'vgamem_fill_linear'
vgamem_fill_linear:                          ; 0xc1a22 LB 0x6d
    push bp                                   ; 55                          ; 0xc1a22 vgabios.c:1174
    mov bp, sp                                ; 89 e5                       ; 0xc1a23
    push si                                   ; 56                          ; 0xc1a25
    push di                                   ; 57                          ; 0xc1a26
    sub sp, strict byte 00008h                ; 83 ec 08                    ; 0xc1a27
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc1a2a
    mov al, dl                                ; 88 d0                       ; 0xc1a2d
    mov si, cx                                ; 89 ce                       ; 0xc1a2f
    xor ah, ah                                ; 30 e4                       ; 0xc1a31 vgabios.c:1180
    mov dx, ax                                ; 89 c2                       ; 0xc1a33
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1a35
    mov di, ax                                ; 89 c7                       ; 0xc1a38
    mov ax, dx                                ; 89 d0                       ; 0xc1a3a
    imul di                                   ; f7 ef                       ; 0xc1a3c
    mul cx                                    ; f7 e1                       ; 0xc1a3e
    mov dx, ax                                ; 89 c2                       ; 0xc1a40
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1a42
    xor ah, ah                                ; 30 e4                       ; 0xc1a45
    add ax, dx                                ; 01 d0                       ; 0xc1a47
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc1a49
    sal ax, CL                                ; d3 e0                       ; 0xc1a4b
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc1a4d
    sal bx, CL                                ; d3 e3                       ; 0xc1a50 vgabios.c:1181
    sal si, CL                                ; d3 e6                       ; 0xc1a52 vgabios.c:1182
    mov byte [bp-008h], 000h                  ; c6 46 f8 00                 ; 0xc1a54 vgabios.c:1183
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1a58
    cmp al, byte [bp+004h]                    ; 3a 46 04                    ; 0xc1a5b
    jnc short 01a86h                          ; 73 26                       ; 0xc1a5e
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc1a60 vgabios.c:1185
    xor ah, ah                                ; 30 e4                       ; 0xc1a63
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc1a65
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1a68
    mul si                                    ; f7 e6                       ; 0xc1a6b
    mov di, word [bp-00ah]                    ; 8b 7e f6                    ; 0xc1a6d
    add di, ax                                ; 01 c7                       ; 0xc1a70
    mov cx, bx                                ; 89 d9                       ; 0xc1a72
    mov ax, word [bp-00ch]                    ; 8b 46 f4                    ; 0xc1a74
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc1a77
    mov es, dx                                ; 8e c2                       ; 0xc1a7a
    cld                                       ; fc                          ; 0xc1a7c
    jcxz 01a81h                               ; e3 02                       ; 0xc1a7d
    rep stosb                                 ; f3 aa                       ; 0xc1a7f
    inc byte [bp-008h]                        ; fe 46 f8                    ; 0xc1a81 vgabios.c:1186
    jmp short 01a58h                          ; eb d2                       ; 0xc1a84
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc1a86 vgabios.c:1187
    pop di                                    ; 5f                          ; 0xc1a89
    pop si                                    ; 5e                          ; 0xc1a8a
    pop bp                                    ; 5d                          ; 0xc1a8b
    retn 00004h                               ; c2 04 00                    ; 0xc1a8c
  ; disGetNextSymbol 0xc1a8f LB 0x281a -> off=0x0 cb=00000000000006ab uValue=00000000000c1a8f 'biosfn_scroll'
biosfn_scroll:                               ; 0xc1a8f LB 0x6ab
    push bp                                   ; 55                          ; 0xc1a8f vgabios.c:1190
    mov bp, sp                                ; 89 e5                       ; 0xc1a90
    push si                                   ; 56                          ; 0xc1a92
    push di                                   ; 57                          ; 0xc1a93
    sub sp, strict byte 00020h                ; 83 ec 20                    ; 0xc1a94
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc1a97
    mov byte [bp-010h], dl                    ; 88 56 f0                    ; 0xc1a9a
    mov byte [bp-00ch], bl                    ; 88 5e f4                    ; 0xc1a9d
    mov byte [bp-008h], cl                    ; 88 4e f8                    ; 0xc1aa0
    mov ch, byte [bp+006h]                    ; 8a 6e 06                    ; 0xc1aa3
    cmp bl, byte [bp+004h]                    ; 3a 5e 04                    ; 0xc1aa6 vgabios.c:1199
    jnbe short 01ac6h                         ; 77 1b                       ; 0xc1aa9
    cmp ch, cl                                ; 38 cd                       ; 0xc1aab vgabios.c:1200
    jc short 01ac6h                           ; 72 17                       ; 0xc1aad
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc1aaf vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1ab2
    mov es, ax                                ; 8e c0                       ; 0xc1ab5
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc1ab7
    xor ah, ah                                ; 30 e4                       ; 0xc1aba vgabios.c:1204
    call 03651h                               ; e8 92 1b                    ; 0xc1abc
    mov byte [bp-012h], al                    ; 88 46 ee                    ; 0xc1abf
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc1ac2 vgabios.c:1205
    jne short 01ac9h                          ; 75 03                       ; 0xc1ac4
    jmp near 02131h                           ; e9 68 06                    ; 0xc1ac6
    mov bx, 00084h                            ; bb 84 00                    ; 0xc1ac9 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1acc
    mov es, ax                                ; 8e c0                       ; 0xc1acf
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc1ad1
    xor ah, ah                                ; 30 e4                       ; 0xc1ad4 vgabios.c:38
    inc ax                                    ; 40                          ; 0xc1ad6
    mov word [bp-024h], ax                    ; 89 46 dc                    ; 0xc1ad7
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc1ada vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc1add
    mov word [bp-018h], ax                    ; 89 46 e8                    ; 0xc1ae0 vgabios.c:48
    cmp byte [bp+008h], 0ffh                  ; 80 7e 08 ff                 ; 0xc1ae3 vgabios.c:1212
    jne short 01af2h                          ; 75 09                       ; 0xc1ae7
    mov bx, strict word 00062h                ; bb 62 00                    ; 0xc1ae9 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc1aec
    mov byte [bp+008h], al                    ; 88 46 08                    ; 0xc1aef vgabios.c:38
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1af2 vgabios.c:1215
    xor ah, ah                                ; 30 e4                       ; 0xc1af5
    cmp ax, word [bp-024h]                    ; 3b 46 dc                    ; 0xc1af7
    jc short 01b04h                           ; 72 08                       ; 0xc1afa
    mov al, byte [bp-024h]                    ; 8a 46 dc                    ; 0xc1afc
    db  0feh, 0c8h
    ; dec al                                    ; fe c8                     ; 0xc1aff
    mov byte [bp+004h], al                    ; 88 46 04                    ; 0xc1b01
    mov al, ch                                ; 88 e8                       ; 0xc1b04 vgabios.c:1216
    xor ah, ah                                ; 30 e4                       ; 0xc1b06
    cmp ax, word [bp-018h]                    ; 3b 46 e8                    ; 0xc1b08
    jc short 01b12h                           ; 72 05                       ; 0xc1b0b
    mov ch, byte [bp-018h]                    ; 8a 6e e8                    ; 0xc1b0d
    db  0feh, 0cdh
    ; dec ch                                    ; fe cd                     ; 0xc1b10
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1b12 vgabios.c:1217
    xor ah, ah                                ; 30 e4                       ; 0xc1b15
    cmp ax, word [bp-024h]                    ; 3b 46 dc                    ; 0xc1b17
    jbe short 01b1fh                          ; 76 03                       ; 0xc1b1a
    mov byte [bp-006h], ah                    ; 88 66 fa                    ; 0xc1b1c
    mov al, ch                                ; 88 e8                       ; 0xc1b1f vgabios.c:1218
    sub al, byte [bp-008h]                    ; 2a 46 f8                    ; 0xc1b21
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc1b24
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc1b26
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc1b29 vgabios.c:1220
    mov byte [bp-01eh], al                    ; 88 46 e2                    ; 0xc1b2c
    mov byte [bp-01dh], 000h                  ; c6 46 e3 00                 ; 0xc1b2f
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc1b33
    mov bx, word [bp-01eh]                    ; 8b 5e e2                    ; 0xc1b35
    sal bx, CL                                ; d3 e3                       ; 0xc1b38
    mov ax, word [bp-018h]                    ; 8b 46 e8                    ; 0xc1b3a
    dec ax                                    ; 48                          ; 0xc1b3d
    mov word [bp-020h], ax                    ; 89 46 e0                    ; 0xc1b3e
    mov ax, word [bp-024h]                    ; 8b 46 dc                    ; 0xc1b41
    dec ax                                    ; 48                          ; 0xc1b44
    mov word [bp-022h], ax                    ; 89 46 de                    ; 0xc1b45
    mov ax, word [bp-018h]                    ; 8b 46 e8                    ; 0xc1b48
    mul word [bp-024h]                        ; f7 66 dc                    ; 0xc1b4b
    mov di, ax                                ; 89 c7                       ; 0xc1b4e
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc1b50
    jne short 01ba2h                          ; 75 4b                       ; 0xc1b55
    sal ax, 1                                 ; d1 e0                       ; 0xc1b57 vgabios.c:1223
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc1b59
    mov dl, byte [bp+008h]                    ; 8a 56 08                    ; 0xc1b5b
    xor dh, dh                                ; 30 f6                       ; 0xc1b5e
    inc ax                                    ; 40                          ; 0xc1b60
    mul dx                                    ; f7 e2                       ; 0xc1b61
    mov word [bp-01ch], ax                    ; 89 46 e4                    ; 0xc1b63
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1b66 vgabios.c:1228
    jne short 01ba5h                          ; 75 39                       ; 0xc1b6a
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00                 ; 0xc1b6c
    jne short 01ba5h                          ; 75 33                       ; 0xc1b70
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1b72
    jne short 01ba5h                          ; 75 2d                       ; 0xc1b76
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1b78
    xor ah, ah                                ; 30 e4                       ; 0xc1b7b
    cmp ax, word [bp-022h]                    ; 3b 46 de                    ; 0xc1b7d
    jne short 01ba5h                          ; 75 23                       ; 0xc1b80
    mov al, ch                                ; 88 e8                       ; 0xc1b82
    cmp ax, word [bp-020h]                    ; 3b 46 e0                    ; 0xc1b84
    jne short 01ba5h                          ; 75 1c                       ; 0xc1b87
    mov ah, byte [bp-010h]                    ; 8a 66 f0                    ; 0xc1b89 vgabios.c:1230
    xor al, ch                                ; 30 e8                       ; 0xc1b8c
    add ax, strict word 00020h                ; 05 20 00                    ; 0xc1b8e
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc1b91
    mov cx, di                                ; 89 f9                       ; 0xc1b95
    mov di, word [bp-01ch]                    ; 8b 7e e4                    ; 0xc1b97
    cld                                       ; fc                          ; 0xc1b9a
    jcxz 01b9fh                               ; e3 02                       ; 0xc1b9b
    rep stosw                                 ; f3 ab                       ; 0xc1b9d
    jmp near 02131h                           ; e9 8f 05                    ; 0xc1b9f vgabios.c:1232
    jmp near 01d33h                           ; e9 8e 01                    ; 0xc1ba2
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc1ba5 vgabios.c:1234
    jne short 01c11h                          ; 75 66                       ; 0xc1ba9
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1bab vgabios.c:1235
    xor ah, ah                                ; 30 e4                       ; 0xc1bae
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc1bb0
    mov dl, byte [bp+004h]                    ; 8a 56 04                    ; 0xc1bb3
    xor dh, dh                                ; 30 f6                       ; 0xc1bb6
    cmp dx, word [bp-016h]                    ; 3b 56 ea                    ; 0xc1bb8
    jc short 01c13h                           ; 72 56                       ; 0xc1bbb
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1bbd vgabios.c:1237
    xor ah, ah                                ; 30 e4                       ; 0xc1bc0
    add ax, word [bp-016h]                    ; 03 46 ea                    ; 0xc1bc2
    cmp ax, dx                                ; 39 d0                       ; 0xc1bc5
    jnbe short 01bcfh                         ; 77 06                       ; 0xc1bc7
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1bc9
    jne short 01c16h                          ; 75 47                       ; 0xc1bcd
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc1bcf vgabios.c:1238
    mov byte [bp-01ah], al                    ; 88 46 e6                    ; 0xc1bd2
    xor al, al                                ; 30 c0                       ; 0xc1bd5
    mov byte [bp-019h], al                    ; 88 46 e7                    ; 0xc1bd7
    mov ah, byte [bp-010h]                    ; 8a 66 f0                    ; 0xc1bda
    mov si, ax                                ; 89 c6                       ; 0xc1bdd
    add si, strict byte 00020h                ; 83 c6 20                    ; 0xc1bdf
    mov ax, word [bp-016h]                    ; 8b 46 ea                    ; 0xc1be2
    mul word [bp-018h]                        ; f7 66 e8                    ; 0xc1be5
    mov dx, ax                                ; 89 c2                       ; 0xc1be8
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1bea
    xor ah, ah                                ; 30 e4                       ; 0xc1bed
    add ax, dx                                ; 01 d0                       ; 0xc1bef
    sal ax, 1                                 ; d1 e0                       ; 0xc1bf1
    mov di, word [bp-01ch]                    ; 8b 7e e4                    ; 0xc1bf3
    add di, ax                                ; 01 c7                       ; 0xc1bf6
    mov bl, byte [bp-012h]                    ; 8a 5e ee                    ; 0xc1bf8
    xor bh, bh                                ; 30 ff                       ; 0xc1bfb
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc1bfd
    sal bx, CL                                ; d3 e3                       ; 0xc1bff
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc1c01
    mov cx, word [bp-01ah]                    ; 8b 4e e6                    ; 0xc1c05
    mov ax, si                                ; 89 f0                       ; 0xc1c08
    cld                                       ; fc                          ; 0xc1c0a
    jcxz 01c0fh                               ; e3 02                       ; 0xc1c0b
    rep stosw                                 ; f3 ab                       ; 0xc1c0d
    jmp short 01c60h                          ; eb 4f                       ; 0xc1c0f vgabios.c:1239
    jmp short 01c66h                          ; eb 53                       ; 0xc1c11
    jmp near 02131h                           ; e9 1b 05                    ; 0xc1c13
    mov dl, byte [bp-00ah]                    ; 8a 56 f6                    ; 0xc1c16 vgabios.c:1240
    mov byte [bp-014h], dl                    ; 88 56 ec                    ; 0xc1c19
    mov byte [bp-013h], dh                    ; 88 76 ed                    ; 0xc1c1c
    mul word [bp-018h]                        ; f7 66 e8                    ; 0xc1c1f
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc1c22
    mov byte [bp-01ah], dl                    ; 88 56 e6                    ; 0xc1c25
    mov byte [bp-019h], 000h                  ; c6 46 e7 00                 ; 0xc1c28
    mov si, ax                                ; 89 c6                       ; 0xc1c2c
    add si, word [bp-01ah]                    ; 03 76 e6                    ; 0xc1c2e
    sal si, 1                                 ; d1 e6                       ; 0xc1c31
    mov bl, byte [bp-012h]                    ; 8a 5e ee                    ; 0xc1c33
    xor bh, bh                                ; 30 ff                       ; 0xc1c36
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc1c38
    sal bx, CL                                ; d3 e3                       ; 0xc1c3a
    mov bx, word [bx+047b2h]                  ; 8b 9f b2 47                 ; 0xc1c3c
    mov ax, word [bp-016h]                    ; 8b 46 ea                    ; 0xc1c40
    mul word [bp-018h]                        ; f7 66 e8                    ; 0xc1c43
    add ax, word [bp-01ah]                    ; 03 46 e6                    ; 0xc1c46
    sal ax, 1                                 ; d1 e0                       ; 0xc1c49
    mov di, word [bp-01ch]                    ; 8b 7e e4                    ; 0xc1c4b
    add di, ax                                ; 01 c7                       ; 0xc1c4e
    mov cx, word [bp-014h]                    ; 8b 4e ec                    ; 0xc1c50
    mov dx, bx                                ; 89 da                       ; 0xc1c53
    mov es, bx                                ; 8e c3                       ; 0xc1c55
    cld                                       ; fc                          ; 0xc1c57
    jcxz 01c60h                               ; e3 06                       ; 0xc1c58
    push DS                                   ; 1e                          ; 0xc1c5a
    mov ds, dx                                ; 8e da                       ; 0xc1c5b
    rep movsw                                 ; f3 a5                       ; 0xc1c5d
    pop DS                                    ; 1f                          ; 0xc1c5f
    inc word [bp-016h]                        ; ff 46 ea                    ; 0xc1c60 vgabios.c:1241
    jmp near 01bb3h                           ; e9 4d ff                    ; 0xc1c63
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1c66 vgabios.c:1244
    xor ah, ah                                ; 30 e4                       ; 0xc1c69
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc1c6b
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1c6e
    xor ah, ah                                ; 30 e4                       ; 0xc1c71
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1c73
    jnbe short 01c13h                         ; 77 9b                       ; 0xc1c76
    mov dl, al                                ; 88 c2                       ; 0xc1c78 vgabios.c:1246
    xor dh, dh                                ; 30 f6                       ; 0xc1c7a
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1c7c
    add ax, dx                                ; 01 d0                       ; 0xc1c7f
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1c81
    jnbe short 01c8ch                         ; 77 06                       ; 0xc1c84
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1c86
    jne short 01ccdh                          ; 75 41                       ; 0xc1c8a
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc1c8c vgabios.c:1247
    xor bh, bh                                ; 30 ff                       ; 0xc1c8f
    mov ah, byte [bp-010h]                    ; 8a 66 f0                    ; 0xc1c91
    xor al, al                                ; 30 c0                       ; 0xc1c94
    mov si, ax                                ; 89 c6                       ; 0xc1c96
    add si, strict byte 00020h                ; 83 c6 20                    ; 0xc1c98
    mov ax, word [bp-016h]                    ; 8b 46 ea                    ; 0xc1c9b
    mul word [bp-018h]                        ; f7 66 e8                    ; 0xc1c9e
    mov dx, ax                                ; 89 c2                       ; 0xc1ca1
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1ca3
    xor ah, ah                                ; 30 e4                       ; 0xc1ca6
    add ax, dx                                ; 01 d0                       ; 0xc1ca8
    sal ax, 1                                 ; d1 e0                       ; 0xc1caa
    mov dx, word [bp-01ch]                    ; 8b 56 e4                    ; 0xc1cac
    add dx, ax                                ; 01 c2                       ; 0xc1caf
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc1cb1
    xor ah, ah                                ; 30 e4                       ; 0xc1cb4
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc1cb6
    mov di, ax                                ; 89 c7                       ; 0xc1cb8
    sal di, CL                                ; d3 e7                       ; 0xc1cba
    mov es, [di+047b2h]                       ; 8e 85 b2 47                 ; 0xc1cbc
    mov cx, bx                                ; 89 d9                       ; 0xc1cc0
    mov ax, si                                ; 89 f0                       ; 0xc1cc2
    mov di, dx                                ; 89 d7                       ; 0xc1cc4
    cld                                       ; fc                          ; 0xc1cc6
    jcxz 01ccbh                               ; e3 02                       ; 0xc1cc7
    rep stosw                                 ; f3 ab                       ; 0xc1cc9
    jmp short 01d23h                          ; eb 56                       ; 0xc1ccb vgabios.c:1248
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc1ccd vgabios.c:1249
    mov byte [bp-01ah], al                    ; 88 46 e6                    ; 0xc1cd0
    mov byte [bp-019h], dh                    ; 88 76 e7                    ; 0xc1cd3
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1cd6
    xor ah, ah                                ; 30 e4                       ; 0xc1cd9
    mov dx, word [bp-016h]                    ; 8b 56 ea                    ; 0xc1cdb
    sub dx, ax                                ; 29 c2                       ; 0xc1cde
    mov ax, dx                                ; 89 d0                       ; 0xc1ce0
    mul word [bp-018h]                        ; f7 66 e8                    ; 0xc1ce2
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc1ce5
    mov byte [bp-014h], dl                    ; 88 56 ec                    ; 0xc1ce8
    mov byte [bp-013h], 000h                  ; c6 46 ed 00                 ; 0xc1ceb
    mov si, ax                                ; 89 c6                       ; 0xc1cef
    add si, word [bp-014h]                    ; 03 76 ec                    ; 0xc1cf1
    sal si, 1                                 ; d1 e6                       ; 0xc1cf4
    mov bl, byte [bp-012h]                    ; 8a 5e ee                    ; 0xc1cf6
    xor bh, bh                                ; 30 ff                       ; 0xc1cf9
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc1cfb
    sal bx, CL                                ; d3 e3                       ; 0xc1cfd
    mov bx, word [bx+047b2h]                  ; 8b 9f b2 47                 ; 0xc1cff
    mov ax, word [bp-016h]                    ; 8b 46 ea                    ; 0xc1d03
    mul word [bp-018h]                        ; f7 66 e8                    ; 0xc1d06
    add ax, word [bp-014h]                    ; 03 46 ec                    ; 0xc1d09
    sal ax, 1                                 ; d1 e0                       ; 0xc1d0c
    mov di, word [bp-01ch]                    ; 8b 7e e4                    ; 0xc1d0e
    add di, ax                                ; 01 c7                       ; 0xc1d11
    mov cx, word [bp-01ah]                    ; 8b 4e e6                    ; 0xc1d13
    mov dx, bx                                ; 89 da                       ; 0xc1d16
    mov es, bx                                ; 8e c3                       ; 0xc1d18
    cld                                       ; fc                          ; 0xc1d1a
    jcxz 01d23h                               ; e3 06                       ; 0xc1d1b
    push DS                                   ; 1e                          ; 0xc1d1d
    mov ds, dx                                ; 8e da                       ; 0xc1d1e
    rep movsw                                 ; f3 a5                       ; 0xc1d20
    pop DS                                    ; 1f                          ; 0xc1d22
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1d23 vgabios.c:1250
    xor ah, ah                                ; 30 e4                       ; 0xc1d26
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1d28
    jc short 01d61h                           ; 72 34                       ; 0xc1d2b
    dec word [bp-016h]                        ; ff 4e ea                    ; 0xc1d2d vgabios.c:1251
    jmp near 01c6eh                           ; e9 3b ff                    ; 0xc1d30
    mov si, word [bp-01eh]                    ; 8b 76 e2                    ; 0xc1d33 vgabios.c:1257
    mov al, byte [si+0482eh]                  ; 8a 84 2e 48                 ; 0xc1d36
    xor ah, ah                                ; 30 e4                       ; 0xc1d3a
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc1d3c
    mov si, ax                                ; 89 c6                       ; 0xc1d3e
    sal si, CL                                ; d3 e6                       ; 0xc1d40
    mov al, byte [si+04844h]                  ; 8a 84 44 48                 ; 0xc1d42
    mov byte [bp-00eh], al                    ; 88 46 f2                    ; 0xc1d46
    mov al, byte [bx+047b0h]                  ; 8a 87 b0 47                 ; 0xc1d49 vgabios.c:1258
    cmp AL, strict byte 003h                  ; 3c 03                       ; 0xc1d4d
    jc short 01d5dh                           ; 72 0c                       ; 0xc1d4f
    jbe short 01d64h                          ; 76 11                       ; 0xc1d51
    cmp AL, strict byte 005h                  ; 3c 05                       ; 0xc1d53
    je short 01d91h                           ; 74 3a                       ; 0xc1d55
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc1d57
    je short 01d64h                           ; 74 09                       ; 0xc1d59
    jmp short 01d61h                          ; eb 04                       ; 0xc1d5b
    cmp AL, strict byte 002h                  ; 3c 02                       ; 0xc1d5d
    je short 01d94h                           ; 74 33                       ; 0xc1d5f
    jmp near 02131h                           ; e9 cd 03                    ; 0xc1d61
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1d64 vgabios.c:1262
    jne short 01d8fh                          ; 75 25                       ; 0xc1d68
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00                 ; 0xc1d6a
    jne short 01dd3h                          ; 75 63                       ; 0xc1d6e
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1d70
    jne short 01dd3h                          ; 75 5d                       ; 0xc1d74
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1d76
    xor ah, ah                                ; 30 e4                       ; 0xc1d79
    mov dx, word [bp-024h]                    ; 8b 56 dc                    ; 0xc1d7b
    dec dx                                    ; 4a                          ; 0xc1d7e
    cmp ax, dx                                ; 39 d0                       ; 0xc1d7f
    jne short 01dd3h                          ; 75 50                       ; 0xc1d81
    mov al, ch                                ; 88 e8                       ; 0xc1d83
    xor ah, dh                                ; 30 f4                       ; 0xc1d85
    mov dx, word [bp-018h]                    ; 8b 56 e8                    ; 0xc1d87
    dec dx                                    ; 4a                          ; 0xc1d8a
    cmp ax, dx                                ; 39 d0                       ; 0xc1d8b
    je short 01d97h                           ; 74 08                       ; 0xc1d8d
    jmp short 01dd3h                          ; eb 42                       ; 0xc1d8f
    jmp near 02014h                           ; e9 80 02                    ; 0xc1d91
    jmp near 01ebfh                           ; e9 28 01                    ; 0xc1d94
    mov ax, 00205h                            ; b8 05 02                    ; 0xc1d97 vgabios.c:1264
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1d9a
    out DX, ax                                ; ef                          ; 0xc1d9d
    mov ax, word [bp-024h]                    ; 8b 46 dc                    ; 0xc1d9e vgabios.c:1265
    mul word [bp-018h]                        ; f7 66 e8                    ; 0xc1da1
    mov dl, byte [bp-00eh]                    ; 8a 56 f2                    ; 0xc1da4
    xor dh, dh                                ; 30 f6                       ; 0xc1da7
    mul dx                                    ; f7 e2                       ; 0xc1da9
    mov dx, ax                                ; 89 c2                       ; 0xc1dab
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1dad
    xor ah, ah                                ; 30 e4                       ; 0xc1db0
    mov bl, byte [bp-012h]                    ; 8a 5e ee                    ; 0xc1db2
    xor bh, bh                                ; 30 ff                       ; 0xc1db5
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc1db7
    sal bx, CL                                ; d3 e3                       ; 0xc1db9
    mov bx, word [bx+047b2h]                  ; 8b 9f b2 47                 ; 0xc1dbb
    mov cx, dx                                ; 89 d1                       ; 0xc1dbf
    xor di, di                                ; 31 ff                       ; 0xc1dc1
    mov es, bx                                ; 8e c3                       ; 0xc1dc3
    cld                                       ; fc                          ; 0xc1dc5
    jcxz 01dcah                               ; e3 02                       ; 0xc1dc6
    rep stosb                                 ; f3 aa                       ; 0xc1dc8
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc1dca vgabios.c:1266
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1dcd
    out DX, ax                                ; ef                          ; 0xc1dd0
    jmp short 01d61h                          ; eb 8e                       ; 0xc1dd1 vgabios.c:1268
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc1dd3 vgabios.c:1270
    jne short 01e45h                          ; 75 6c                       ; 0xc1dd7
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1dd9 vgabios.c:1271
    xor ah, ah                                ; 30 e4                       ; 0xc1ddc
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc1dde
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1de1
    xor ah, ah                                ; 30 e4                       ; 0xc1de4
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1de6
    jc short 01e42h                           ; 72 57                       ; 0xc1de9
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc1deb vgabios.c:1273
    xor dh, dh                                ; 30 f6                       ; 0xc1dee
    add dx, word [bp-016h]                    ; 03 56 ea                    ; 0xc1df0
    cmp dx, ax                                ; 39 c2                       ; 0xc1df3
    jnbe short 01dfdh                         ; 77 06                       ; 0xc1df5
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1df7
    jne short 01e1eh                          ; 75 21                       ; 0xc1dfb
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1dfd vgabios.c:1274
    xor ah, ah                                ; 30 e4                       ; 0xc1e00
    push ax                                   ; 50                          ; 0xc1e02
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1e03
    push ax                                   ; 50                          ; 0xc1e06
    mov cl, byte [bp-018h]                    ; 8a 4e e8                    ; 0xc1e07
    xor ch, ch                                ; 30 ed                       ; 0xc1e0a
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc1e0c
    xor bh, bh                                ; 30 ff                       ; 0xc1e0f
    mov dl, byte [bp-016h]                    ; 8a 56 ea                    ; 0xc1e11
    xor dh, dh                                ; 30 f6                       ; 0xc1e14
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1e16
    call 017d4h                               ; e8 b8 f9                    ; 0xc1e19
    jmp short 01e3dh                          ; eb 1f                       ; 0xc1e1c vgabios.c:1275
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1e1e vgabios.c:1276
    push ax                                   ; 50                          ; 0xc1e21
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc1e22
    push ax                                   ; 50                          ; 0xc1e25
    mov cl, byte [bp-00ah]                    ; 8a 4e f6                    ; 0xc1e26
    xor ch, ch                                ; 30 ed                       ; 0xc1e29
    mov bl, byte [bp-016h]                    ; 8a 5e ea                    ; 0xc1e2b
    xor bh, bh                                ; 30 ff                       ; 0xc1e2e
    mov dl, bl                                ; 88 da                       ; 0xc1e30
    add dl, byte [bp-006h]                    ; 02 56 fa                    ; 0xc1e32
    xor dh, dh                                ; 30 f6                       ; 0xc1e35
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1e37
    call 01745h                               ; e8 08 f9                    ; 0xc1e3a
    inc word [bp-016h]                        ; ff 46 ea                    ; 0xc1e3d vgabios.c:1277
    jmp short 01de1h                          ; eb 9f                       ; 0xc1e40
    jmp near 02131h                           ; e9 ec 02                    ; 0xc1e42
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1e45 vgabios.c:1280
    xor ah, ah                                ; 30 e4                       ; 0xc1e48
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc1e4a
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1e4d
    xor ah, ah                                ; 30 e4                       ; 0xc1e50
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1e52
    jnbe short 01e42h                         ; 77 eb                       ; 0xc1e55
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc1e57 vgabios.c:1282
    xor dh, dh                                ; 30 f6                       ; 0xc1e5a
    add ax, dx                                ; 01 d0                       ; 0xc1e5c
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1e5e
    jnbe short 01e67h                         ; 77 04                       ; 0xc1e61
    test dl, dl                               ; 84 d2                       ; 0xc1e63
    jne short 01e88h                          ; 75 21                       ; 0xc1e65
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1e67 vgabios.c:1283
    xor ah, ah                                ; 30 e4                       ; 0xc1e6a
    push ax                                   ; 50                          ; 0xc1e6c
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1e6d
    push ax                                   ; 50                          ; 0xc1e70
    mov cl, byte [bp-018h]                    ; 8a 4e e8                    ; 0xc1e71
    xor ch, ch                                ; 30 ed                       ; 0xc1e74
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc1e76
    xor bh, bh                                ; 30 ff                       ; 0xc1e79
    mov dl, byte [bp-016h]                    ; 8a 56 ea                    ; 0xc1e7b
    xor dh, dh                                ; 30 f6                       ; 0xc1e7e
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1e80
    call 017d4h                               ; e8 4e f9                    ; 0xc1e83
    jmp short 01eb0h                          ; eb 28                       ; 0xc1e86 vgabios.c:1284
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1e88 vgabios.c:1285
    xor ah, ah                                ; 30 e4                       ; 0xc1e8b
    push ax                                   ; 50                          ; 0xc1e8d
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc1e8e
    push ax                                   ; 50                          ; 0xc1e91
    mov cl, byte [bp-00ah]                    ; 8a 4e f6                    ; 0xc1e92
    xor ch, ch                                ; 30 ed                       ; 0xc1e95
    mov bl, byte [bp-016h]                    ; 8a 5e ea                    ; 0xc1e97
    xor bh, bh                                ; 30 ff                       ; 0xc1e9a
    mov dl, bl                                ; 88 da                       ; 0xc1e9c
    sub dl, byte [bp-006h]                    ; 2a 56 fa                    ; 0xc1e9e
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1ea1
    mov byte [bp-01ah], al                    ; 88 46 e6                    ; 0xc1ea4
    mov byte [bp-019h], dh                    ; 88 76 e7                    ; 0xc1ea7
    mov ax, word [bp-01ah]                    ; 8b 46 e6                    ; 0xc1eaa
    call 01745h                               ; e8 95 f8                    ; 0xc1ead
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1eb0 vgabios.c:1286
    xor ah, ah                                ; 30 e4                       ; 0xc1eb3
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1eb5
    jc short 01f09h                           ; 72 4f                       ; 0xc1eb8
    dec word [bp-016h]                        ; ff 4e ea                    ; 0xc1eba vgabios.c:1287
    jmp short 01e4dh                          ; eb 8e                       ; 0xc1ebd
    mov cl, byte [bx+047b1h]                  ; 8a 8f b1 47                 ; 0xc1ebf vgabios.c:1292
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1ec3 vgabios.c:1293
    jne short 01f0ch                          ; 75 43                       ; 0xc1ec7
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00                 ; 0xc1ec9
    jne short 01f0ch                          ; 75 3d                       ; 0xc1ecd
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1ecf
    jne short 01f0ch                          ; 75 37                       ; 0xc1ed3
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1ed5
    cmp ax, word [bp-022h]                    ; 3b 46 de                    ; 0xc1ed8
    jne short 01f0ch                          ; 75 2f                       ; 0xc1edb
    mov al, ch                                ; 88 e8                       ; 0xc1edd
    cmp ax, word [bp-020h]                    ; 3b 46 e0                    ; 0xc1edf
    jne short 01f0ch                          ; 75 28                       ; 0xc1ee2
    mov dl, byte [bp-00eh]                    ; 8a 56 f2                    ; 0xc1ee4 vgabios.c:1295
    xor dh, dh                                ; 30 f6                       ; 0xc1ee7
    mov ax, di                                ; 89 f8                       ; 0xc1ee9
    mul dx                                    ; f7 e2                       ; 0xc1eeb
    mov dl, cl                                ; 88 ca                       ; 0xc1eed
    xor dh, dh                                ; 30 f6                       ; 0xc1eef
    mul dx                                    ; f7 e2                       ; 0xc1ef1
    mov dl, byte [bp-010h]                    ; 8a 56 f0                    ; 0xc1ef3
    xor dh, dh                                ; 30 f6                       ; 0xc1ef6
    mov bx, word [bx+047b2h]                  ; 8b 9f b2 47                 ; 0xc1ef8
    mov cx, ax                                ; 89 c1                       ; 0xc1efc
    mov ax, dx                                ; 89 d0                       ; 0xc1efe
    xor di, di                                ; 31 ff                       ; 0xc1f00
    mov es, bx                                ; 8e c3                       ; 0xc1f02
    cld                                       ; fc                          ; 0xc1f04
    jcxz 01f09h                               ; e3 02                       ; 0xc1f05
    rep stosb                                 ; f3 aa                       ; 0xc1f07
    jmp near 02131h                           ; e9 25 02                    ; 0xc1f09 vgabios.c:1297
    cmp cl, 002h                              ; 80 f9 02                    ; 0xc1f0c vgabios.c:1299
    jne short 01f1ah                          ; 75 09                       ; 0xc1f0f
    sal byte [bp-008h], 1                     ; d0 66 f8                    ; 0xc1f11 vgabios.c:1301
    sal byte [bp-00ah], 1                     ; d0 66 f6                    ; 0xc1f14 vgabios.c:1302
    sal word [bp-018h], 1                     ; d1 66 e8                    ; 0xc1f17 vgabios.c:1303
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc1f1a vgabios.c:1306
    jne short 01f89h                          ; 75 69                       ; 0xc1f1e
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1f20 vgabios.c:1307
    xor ah, ah                                ; 30 e4                       ; 0xc1f23
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc1f25
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1f28
    xor ah, ah                                ; 30 e4                       ; 0xc1f2b
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1f2d
    jc short 01f09h                           ; 72 d7                       ; 0xc1f30
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc1f32 vgabios.c:1309
    xor dh, dh                                ; 30 f6                       ; 0xc1f35
    add dx, word [bp-016h]                    ; 03 56 ea                    ; 0xc1f37
    cmp dx, ax                                ; 39 c2                       ; 0xc1f3a
    jnbe short 01f44h                         ; 77 06                       ; 0xc1f3c
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1f3e
    jne short 01f65h                          ; 75 21                       ; 0xc1f42
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1f44 vgabios.c:1310
    xor ah, ah                                ; 30 e4                       ; 0xc1f47
    push ax                                   ; 50                          ; 0xc1f49
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1f4a
    push ax                                   ; 50                          ; 0xc1f4d
    mov cl, byte [bp-018h]                    ; 8a 4e e8                    ; 0xc1f4e
    xor ch, ch                                ; 30 ed                       ; 0xc1f51
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc1f53
    xor bh, bh                                ; 30 ff                       ; 0xc1f56
    mov dl, byte [bp-016h]                    ; 8a 56 ea                    ; 0xc1f58
    xor dh, dh                                ; 30 f6                       ; 0xc1f5b
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1f5d
    call 01908h                               ; e8 a5 f9                    ; 0xc1f60
    jmp short 01f84h                          ; eb 1f                       ; 0xc1f63 vgabios.c:1311
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1f65 vgabios.c:1312
    push ax                                   ; 50                          ; 0xc1f68
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc1f69
    push ax                                   ; 50                          ; 0xc1f6c
    mov cl, byte [bp-00ah]                    ; 8a 4e f6                    ; 0xc1f6d
    xor ch, ch                                ; 30 ed                       ; 0xc1f70
    mov bl, byte [bp-016h]                    ; 8a 5e ea                    ; 0xc1f72
    xor bh, bh                                ; 30 ff                       ; 0xc1f75
    mov dl, bl                                ; 88 da                       ; 0xc1f77
    add dl, byte [bp-006h]                    ; 02 56 fa                    ; 0xc1f79
    xor dh, dh                                ; 30 f6                       ; 0xc1f7c
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1f7e
    call 01850h                               ; e8 cc f8                    ; 0xc1f81
    inc word [bp-016h]                        ; ff 46 ea                    ; 0xc1f84 vgabios.c:1313
    jmp short 01f28h                          ; eb 9f                       ; 0xc1f87
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1f89 vgabios.c:1316
    xor ah, ah                                ; 30 e4                       ; 0xc1f8c
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc1f8e
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1f91
    xor ah, ah                                ; 30 e4                       ; 0xc1f94
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1f96
    jnbe short 01fd9h                         ; 77 3e                       ; 0xc1f99
    mov dl, al                                ; 88 c2                       ; 0xc1f9b vgabios.c:1318
    xor dh, dh                                ; 30 f6                       ; 0xc1f9d
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1f9f
    add ax, dx                                ; 01 d0                       ; 0xc1fa2
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1fa4
    jnbe short 01fafh                         ; 77 06                       ; 0xc1fa7
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1fa9
    jne short 01fdch                          ; 75 2d                       ; 0xc1fad
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1faf vgabios.c:1319
    xor ah, ah                                ; 30 e4                       ; 0xc1fb2
    push ax                                   ; 50                          ; 0xc1fb4
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1fb5
    push ax                                   ; 50                          ; 0xc1fb8
    mov cl, byte [bp-018h]                    ; 8a 4e e8                    ; 0xc1fb9
    xor ch, ch                                ; 30 ed                       ; 0xc1fbc
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc1fbe
    xor bh, bh                                ; 30 ff                       ; 0xc1fc1
    mov dl, byte [bp-016h]                    ; 8a 56 ea                    ; 0xc1fc3
    xor dh, dh                                ; 30 f6                       ; 0xc1fc6
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1fc8
    mov byte [bp-014h], al                    ; 88 46 ec                    ; 0xc1fcb
    mov byte [bp-013h], ah                    ; 88 66 ed                    ; 0xc1fce
    mov ax, word [bp-014h]                    ; 8b 46 ec                    ; 0xc1fd1
    call 01908h                               ; e8 31 f9                    ; 0xc1fd4
    jmp short 02004h                          ; eb 2b                       ; 0xc1fd7 vgabios.c:1320
    jmp near 02131h                           ; e9 55 01                    ; 0xc1fd9
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1fdc vgabios.c:1321
    xor ah, ah                                ; 30 e4                       ; 0xc1fdf
    push ax                                   ; 50                          ; 0xc1fe1
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc1fe2
    push ax                                   ; 50                          ; 0xc1fe5
    mov cl, byte [bp-00ah]                    ; 8a 4e f6                    ; 0xc1fe6
    xor ch, ch                                ; 30 ed                       ; 0xc1fe9
    mov bl, byte [bp-016h]                    ; 8a 5e ea                    ; 0xc1feb
    xor bh, bh                                ; 30 ff                       ; 0xc1fee
    mov dl, bl                                ; 88 da                       ; 0xc1ff0
    sub dl, byte [bp-006h]                    ; 2a 56 fa                    ; 0xc1ff2
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1ff5
    mov byte [bp-014h], al                    ; 88 46 ec                    ; 0xc1ff8
    mov byte [bp-013h], dh                    ; 88 76 ed                    ; 0xc1ffb
    mov ax, word [bp-014h]                    ; 8b 46 ec                    ; 0xc1ffe
    call 01850h                               ; e8 4c f8                    ; 0xc2001
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc2004 vgabios.c:1322
    xor ah, ah                                ; 30 e4                       ; 0xc2007
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc2009
    jc short 02054h                           ; 72 46                       ; 0xc200c
    dec word [bp-016h]                        ; ff 4e ea                    ; 0xc200e vgabios.c:1323
    jmp near 01f91h                           ; e9 7d ff                    ; 0xc2011
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc2014 vgabios.c:1328
    jne short 02057h                          ; 75 3d                       ; 0xc2018
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00                 ; 0xc201a
    jne short 02057h                          ; 75 37                       ; 0xc201e
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc2020
    jne short 02057h                          ; 75 31                       ; 0xc2024
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc2026
    cmp ax, word [bp-022h]                    ; 3b 46 de                    ; 0xc2029
    jne short 02057h                          ; 75 29                       ; 0xc202c
    mov al, ch                                ; 88 e8                       ; 0xc202e
    cmp ax, word [bp-020h]                    ; 3b 46 e0                    ; 0xc2030
    jne short 02057h                          ; 75 22                       ; 0xc2033
    mov dl, byte [bp-00eh]                    ; 8a 56 f2                    ; 0xc2035 vgabios.c:1330
    xor dh, dh                                ; 30 f6                       ; 0xc2038
    mov ax, di                                ; 89 f8                       ; 0xc203a
    mul dx                                    ; f7 e2                       ; 0xc203c
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc203e
    sal ax, CL                                ; d3 e0                       ; 0xc2040
    mov cx, ax                                ; 89 c1                       ; 0xc2042
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc2044
    xor ah, ah                                ; 30 e4                       ; 0xc2047
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc2049
    xor di, di                                ; 31 ff                       ; 0xc204d
    cld                                       ; fc                          ; 0xc204f
    jcxz 02054h                               ; e3 02                       ; 0xc2050
    rep stosb                                 ; f3 aa                       ; 0xc2052
    jmp near 02131h                           ; e9 da 00                    ; 0xc2054 vgabios.c:1332
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc2057 vgabios.c:1335
    jne short 020c3h                          ; 75 66                       ; 0xc205b
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc205d vgabios.c:1336
    xor ah, ah                                ; 30 e4                       ; 0xc2060
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc2062
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc2065
    xor ah, ah                                ; 30 e4                       ; 0xc2068
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc206a
    jc short 02054h                           ; 72 e5                       ; 0xc206d
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc206f vgabios.c:1338
    xor dh, dh                                ; 30 f6                       ; 0xc2072
    add dx, word [bp-016h]                    ; 03 56 ea                    ; 0xc2074
    cmp dx, ax                                ; 39 c2                       ; 0xc2077
    jnbe short 02081h                         ; 77 06                       ; 0xc2079
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc207b
    jne short 020a0h                          ; 75 1f                       ; 0xc207f
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc2081 vgabios.c:1339
    xor ah, ah                                ; 30 e4                       ; 0xc2084
    push ax                                   ; 50                          ; 0xc2086
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc2087
    push ax                                   ; 50                          ; 0xc208a
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc208b
    xor bh, bh                                ; 30 ff                       ; 0xc208e
    mov dl, byte [bp-016h]                    ; 8a 56 ea                    ; 0xc2090
    xor dh, dh                                ; 30 f6                       ; 0xc2093
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2095
    mov cx, word [bp-018h]                    ; 8b 4e e8                    ; 0xc2098
    call 01a22h                               ; e8 84 f9                    ; 0xc209b
    jmp short 020beh                          ; eb 1e                       ; 0xc209e vgabios.c:1340
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc20a0 vgabios.c:1341
    push ax                                   ; 50                          ; 0xc20a3
    push word [bp-018h]                       ; ff 76 e8                    ; 0xc20a4
    mov cl, byte [bp-00ah]                    ; 8a 4e f6                    ; 0xc20a7
    xor ch, ch                                ; 30 ed                       ; 0xc20aa
    mov bl, byte [bp-016h]                    ; 8a 5e ea                    ; 0xc20ac
    xor bh, bh                                ; 30 ff                       ; 0xc20af
    mov dl, bl                                ; 88 da                       ; 0xc20b1
    add dl, byte [bp-006h]                    ; 02 56 fa                    ; 0xc20b3
    xor dh, dh                                ; 30 f6                       ; 0xc20b6
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc20b8
    call 0199eh                               ; e8 e0 f8                    ; 0xc20bb
    inc word [bp-016h]                        ; ff 46 ea                    ; 0xc20be vgabios.c:1342
    jmp short 02065h                          ; eb a2                       ; 0xc20c1
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc20c3 vgabios.c:1345
    xor ah, ah                                ; 30 e4                       ; 0xc20c6
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc20c8
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc20cb
    xor ah, ah                                ; 30 e4                       ; 0xc20ce
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc20d0
    jnbe short 02131h                         ; 77 5c                       ; 0xc20d3
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc20d5 vgabios.c:1347
    xor dh, dh                                ; 30 f6                       ; 0xc20d8
    add ax, dx                                ; 01 d0                       ; 0xc20da
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc20dc
    jnbe short 020e5h                         ; 77 04                       ; 0xc20df
    test dl, dl                               ; 84 d2                       ; 0xc20e1
    jne short 02104h                          ; 75 1f                       ; 0xc20e3
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc20e5 vgabios.c:1348
    xor ah, ah                                ; 30 e4                       ; 0xc20e8
    push ax                                   ; 50                          ; 0xc20ea
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc20eb
    push ax                                   ; 50                          ; 0xc20ee
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc20ef
    xor bh, bh                                ; 30 ff                       ; 0xc20f2
    mov dl, byte [bp-016h]                    ; 8a 56 ea                    ; 0xc20f4
    xor dh, dh                                ; 30 f6                       ; 0xc20f7
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc20f9
    mov cx, word [bp-018h]                    ; 8b 4e e8                    ; 0xc20fc
    call 01a22h                               ; e8 20 f9                    ; 0xc20ff
    jmp short 02122h                          ; eb 1e                       ; 0xc2102 vgabios.c:1349
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc2104 vgabios.c:1350
    xor ah, ah                                ; 30 e4                       ; 0xc2107
    push ax                                   ; 50                          ; 0xc2109
    push word [bp-018h]                       ; ff 76 e8                    ; 0xc210a
    mov cl, byte [bp-00ah]                    ; 8a 4e f6                    ; 0xc210d
    xor ch, ch                                ; 30 ed                       ; 0xc2110
    mov bl, byte [bp-016h]                    ; 8a 5e ea                    ; 0xc2112
    xor bh, bh                                ; 30 ff                       ; 0xc2115
    mov dl, bl                                ; 88 da                       ; 0xc2117
    sub dl, byte [bp-006h]                    ; 2a 56 fa                    ; 0xc2119
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc211c
    call 0199eh                               ; e8 7c f8                    ; 0xc211f
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc2122 vgabios.c:1351
    xor ah, ah                                ; 30 e4                       ; 0xc2125
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc2127
    jc short 02131h                           ; 72 05                       ; 0xc212a
    dec word [bp-016h]                        ; ff 4e ea                    ; 0xc212c vgabios.c:1352
    jmp short 020cbh                          ; eb 9a                       ; 0xc212f
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc2131 vgabios.c:1363
    pop di                                    ; 5f                          ; 0xc2134
    pop si                                    ; 5e                          ; 0xc2135
    pop bp                                    ; 5d                          ; 0xc2136
    retn 00008h                               ; c2 08 00                    ; 0xc2137
  ; disGetNextSymbol 0xc213a LB 0x216f -> off=0x0 cb=0000000000000112 uValue=00000000000c213a 'write_gfx_char_pl4'
write_gfx_char_pl4:                          ; 0xc213a LB 0x112
    push bp                                   ; 55                          ; 0xc213a vgabios.c:1366
    mov bp, sp                                ; 89 e5                       ; 0xc213b
    push si                                   ; 56                          ; 0xc213d
    push di                                   ; 57                          ; 0xc213e
    sub sp, strict byte 00010h                ; 83 ec 10                    ; 0xc213f
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc2142
    mov byte [bp-00ah], dl                    ; 88 56 f6                    ; 0xc2145
    mov byte [bp-006h], bl                    ; 88 5e fa                    ; 0xc2148
    mov al, cl                                ; 88 c8                       ; 0xc214b
    mov bx, 0010ch                            ; bb 0c 01                    ; 0xc214d vgabios.c:57
    xor cx, cx                                ; 31 c9                       ; 0xc2150
    mov es, cx                                ; 8e c1                       ; 0xc2152
    mov cx, word [es:bx]                      ; 26 8b 0f                    ; 0xc2154
    mov bx, word [es:bx+002h]                 ; 26 8b 5f 02                 ; 0xc2157
    mov word [bp-014h], cx                    ; 89 4e ec                    ; 0xc215b vgabios.c:58
    mov word [bp-010h], bx                    ; 89 5e f0                    ; 0xc215e
    xor ah, ah                                ; 30 e4                       ; 0xc2161 vgabios.c:1375
    mov cl, byte [bp+006h]                    ; 8a 4e 06                    ; 0xc2163
    xor ch, ch                                ; 30 ed                       ; 0xc2166
    imul cx                                   ; f7 e9                       ; 0xc2168
    mov bl, byte [bp+004h]                    ; 8a 5e 04                    ; 0xc216a
    xor bh, bh                                ; 30 ff                       ; 0xc216d
    imul bx                                   ; f7 eb                       ; 0xc216f
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc2171
    mov si, bx                                ; 89 de                       ; 0xc2174
    add si, ax                                ; 01 c6                       ; 0xc2176
    mov bx, strict word 0004ch                ; bb 4c 00                    ; 0xc2178 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc217b
    mov es, ax                                ; 8e c0                       ; 0xc217e
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc2180
    mov bl, byte [bp+008h]                    ; 8a 5e 08                    ; 0xc2183 vgabios.c:48
    xor bh, bh                                ; 30 ff                       ; 0xc2186
    mul bx                                    ; f7 e3                       ; 0xc2188
    add si, ax                                ; 01 c6                       ; 0xc218a
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc218c vgabios.c:1377
    xor ah, ah                                ; 30 e4                       ; 0xc218f
    imul cx                                   ; f7 e9                       ; 0xc2191
    mov word [bp-012h], ax                    ; 89 46 ee                    ; 0xc2193
    mov ax, 00f02h                            ; b8 02 0f                    ; 0xc2196 vgabios.c:1378
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc2199
    out DX, ax                                ; ef                          ; 0xc219c
    mov ax, 00205h                            ; b8 05 02                    ; 0xc219d vgabios.c:1379
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc21a0
    out DX, ax                                ; ef                          ; 0xc21a3
    test byte [bp-00ah], 080h                 ; f6 46 f6 80                 ; 0xc21a4 vgabios.c:1380
    je short 021b0h                           ; 74 06                       ; 0xc21a8
    mov ax, 01803h                            ; b8 03 18                    ; 0xc21aa vgabios.c:1382
    out DX, ax                                ; ef                          ; 0xc21ad
    jmp short 021b4h                          ; eb 04                       ; 0xc21ae vgabios.c:1384
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc21b0 vgabios.c:1386
    out DX, ax                                ; ef                          ; 0xc21b3
    xor ch, ch                                ; 30 ed                       ; 0xc21b4 vgabios.c:1388
    cmp ch, byte [bp+006h]                    ; 3a 6e 06                    ; 0xc21b6
    jnc short 021d0h                          ; 73 15                       ; 0xc21b9
    mov al, ch                                ; 88 e8                       ; 0xc21bb vgabios.c:1390
    xor ah, ah                                ; 30 e4                       ; 0xc21bd
    mov bl, byte [bp+004h]                    ; 8a 5e 04                    ; 0xc21bf
    xor bh, bh                                ; 30 ff                       ; 0xc21c2
    imul bx                                   ; f7 eb                       ; 0xc21c4
    mov bx, si                                ; 89 f3                       ; 0xc21c6
    add bx, ax                                ; 01 c3                       ; 0xc21c8
    mov byte [bp-008h], 000h                  ; c6 46 f8 00                 ; 0xc21ca vgabios.c:1391
    jmp short 021e4h                          ; eb 14                       ; 0xc21ce
    jmp short 02234h                          ; eb 62                       ; 0xc21d0 vgabios.c:1400
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc21d2 vgabios.c:42
    mov es, ax                                ; 8e c0                       ; 0xc21d5
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc21d7
    inc byte [bp-008h]                        ; fe 46 f8                    ; 0xc21db vgabios.c:1404
    cmp byte [bp-008h], 008h                  ; 80 7e f8 08                 ; 0xc21de
    jnc short 02230h                          ; 73 4c                       ; 0xc21e2
    mov cl, byte [bp-008h]                    ; 8a 4e f8                    ; 0xc21e4
    mov ax, 00080h                            ; b8 80 00                    ; 0xc21e7
    sar ax, CL                                ; d3 f8                       ; 0xc21ea
    mov byte [bp-00eh], al                    ; 88 46 f2                    ; 0xc21ec
    mov byte [bp-00dh], 000h                  ; c6 46 f3 00                 ; 0xc21ef
    mov ax, word [bp-00eh]                    ; 8b 46 f2                    ; 0xc21f3
    mov ah, al                                ; 88 c4                       ; 0xc21f6
    xor al, al                                ; 30 c0                       ; 0xc21f8
    or AL, strict byte 008h                   ; 0c 08                       ; 0xc21fa
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc21fc
    out DX, ax                                ; ef                          ; 0xc21ff
    mov dx, bx                                ; 89 da                       ; 0xc2200
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2202
    call 0367ch                               ; e8 74 14                    ; 0xc2205
    mov al, ch                                ; 88 e8                       ; 0xc2208
    xor ah, ah                                ; 30 e4                       ; 0xc220a
    add ax, word [bp-012h]                    ; 03 46 ee                    ; 0xc220c
    mov es, [bp-010h]                         ; 8e 46 f0                    ; 0xc220f
    mov di, word [bp-014h]                    ; 8b 7e ec                    ; 0xc2212
    add di, ax                                ; 01 c7                       ; 0xc2215
    mov al, byte [es:di]                      ; 26 8a 05                    ; 0xc2217
    xor ah, ah                                ; 30 e4                       ; 0xc221a
    test word [bp-00eh], ax                   ; 85 46 f2                    ; 0xc221c
    je short 021d2h                           ; 74 b1                       ; 0xc221f
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2221
    and AL, strict byte 00fh                  ; 24 0f                       ; 0xc2224
    mov di, 0a000h                            ; bf 00 a0                    ; 0xc2226
    mov es, di                                ; 8e c7                       ; 0xc2229
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc222b
    jmp short 021dbh                          ; eb ab                       ; 0xc222e
    db  0feh, 0c5h
    ; inc ch                                    ; fe c5                     ; 0xc2230 vgabios.c:1405
    jmp short 021b6h                          ; eb 82                       ; 0xc2232
    mov ax, 0ff08h                            ; b8 08 ff                    ; 0xc2234 vgabios.c:1406
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2237
    out DX, ax                                ; ef                          ; 0xc223a
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc223b vgabios.c:1407
    out DX, ax                                ; ef                          ; 0xc223e
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc223f vgabios.c:1408
    out DX, ax                                ; ef                          ; 0xc2242
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc2243 vgabios.c:1409
    pop di                                    ; 5f                          ; 0xc2246
    pop si                                    ; 5e                          ; 0xc2247
    pop bp                                    ; 5d                          ; 0xc2248
    retn 00006h                               ; c2 06 00                    ; 0xc2249
  ; disGetNextSymbol 0xc224c LB 0x205d -> off=0x0 cb=0000000000000112 uValue=00000000000c224c 'write_gfx_char_cga'
write_gfx_char_cga:                          ; 0xc224c LB 0x112
    push si                                   ; 56                          ; 0xc224c vgabios.c:1412
    push di                                   ; 57                          ; 0xc224d
    push bp                                   ; 55                          ; 0xc224e
    mov bp, sp                                ; 89 e5                       ; 0xc224f
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc2251
    mov ch, al                                ; 88 c5                       ; 0xc2254
    mov byte [bp-002h], dl                    ; 88 56 fe                    ; 0xc2256
    mov al, bl                                ; 88 d8                       ; 0xc2259
    mov si, 0556ch                            ; be 6c 55                    ; 0xc225b vgabios.c:1419
    xor ah, ah                                ; 30 e4                       ; 0xc225e vgabios.c:1420
    mov bl, byte [bp+00ah]                    ; 8a 5e 0a                    ; 0xc2260
    xor bh, bh                                ; 30 ff                       ; 0xc2263
    imul bx                                   ; f7 eb                       ; 0xc2265
    mov bx, ax                                ; 89 c3                       ; 0xc2267
    mov al, cl                                ; 88 c8                       ; 0xc2269
    xor ah, ah                                ; 30 e4                       ; 0xc226b
    mov di, 00140h                            ; bf 40 01                    ; 0xc226d
    imul di                                   ; f7 ef                       ; 0xc2270
    add bx, ax                                ; 01 c3                       ; 0xc2272
    mov word [bp-004h], bx                    ; 89 5e fc                    ; 0xc2274
    mov al, ch                                ; 88 e8                       ; 0xc2277 vgabios.c:1421
    xor ah, ah                                ; 30 e4                       ; 0xc2279
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc227b
    sal ax, CL                                ; d3 e0                       ; 0xc227d
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc227f
    xor ch, ch                                ; 30 ed                       ; 0xc2282 vgabios.c:1422
    jmp near 022a3h                           ; e9 1c 00                    ; 0xc2284
    mov al, ch                                ; 88 e8                       ; 0xc2287 vgabios.c:1437
    xor ah, ah                                ; 30 e4                       ; 0xc2289
    add ax, word [bp-008h]                    ; 03 46 f8                    ; 0xc228b
    mov di, si                                ; 89 f7                       ; 0xc228e
    add di, ax                                ; 01 c7                       ; 0xc2290
    mov al, byte [di]                         ; 8a 05                       ; 0xc2292
    mov di, 0b800h                            ; bf 00 b8                    ; 0xc2294 vgabios.c:42
    mov es, di                                ; 8e c7                       ; 0xc2297
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2299
    db  0feh, 0c5h
    ; inc ch                                    ; fe c5                     ; 0xc229c vgabios.c:1441
    cmp ch, 008h                              ; 80 fd 08                    ; 0xc229e
    jnc short 022fbh                          ; 73 58                       ; 0xc22a1
    mov al, ch                                ; 88 e8                       ; 0xc22a3
    xor ah, ah                                ; 30 e4                       ; 0xc22a5
    sar ax, 1                                 ; d1 f8                       ; 0xc22a7
    mov bx, strict word 00050h                ; bb 50 00                    ; 0xc22a9
    imul bx                                   ; f7 eb                       ; 0xc22ac
    mov bx, word [bp-004h]                    ; 8b 5e fc                    ; 0xc22ae
    add bx, ax                                ; 01 c3                       ; 0xc22b1
    test ch, 001h                             ; f6 c5 01                    ; 0xc22b3
    je short 022bbh                           ; 74 03                       ; 0xc22b6
    add bh, 020h                              ; 80 c7 20                    ; 0xc22b8
    mov DL, strict byte 080h                  ; b2 80                       ; 0xc22bb
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc22bd
    jne short 022e1h                          ; 75 1e                       ; 0xc22c1
    test byte [bp-002h], dl                   ; 84 56 fe                    ; 0xc22c3
    je short 02287h                           ; 74 bf                       ; 0xc22c6
    mov ax, 0b800h                            ; b8 00 b8                    ; 0xc22c8
    mov es, ax                                ; 8e c0                       ; 0xc22cb
    mov dl, byte [es:bx]                      ; 26 8a 17                    ; 0xc22cd
    mov al, ch                                ; 88 e8                       ; 0xc22d0
    xor ah, ah                                ; 30 e4                       ; 0xc22d2
    add ax, word [bp-008h]                    ; 03 46 f8                    ; 0xc22d4
    mov di, si                                ; 89 f7                       ; 0xc22d7
    add di, ax                                ; 01 c7                       ; 0xc22d9
    mov al, byte [di]                         ; 8a 05                       ; 0xc22db
    xor al, dl                                ; 30 d0                       ; 0xc22dd
    jmp short 02294h                          ; eb b3                       ; 0xc22df
    test dl, dl                               ; 84 d2                       ; 0xc22e1 vgabios.c:1443
    jbe short 0229ch                          ; 76 b7                       ; 0xc22e3
    test byte [bp-002h], 080h                 ; f6 46 fe 80                 ; 0xc22e5 vgabios.c:1445
    je short 022f5h                           ; 74 0a                       ; 0xc22e9
    mov ax, 0b800h                            ; b8 00 b8                    ; 0xc22eb vgabios.c:37
    mov es, ax                                ; 8e c0                       ; 0xc22ee
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc22f0
    jmp short 022f7h                          ; eb 02                       ; 0xc22f3 vgabios.c:1449
    xor al, al                                ; 30 c0                       ; 0xc22f5 vgabios.c:1451
    xor ah, ah                                ; 30 e4                       ; 0xc22f7 vgabios.c:1453
    jmp short 02302h                          ; eb 07                       ; 0xc22f9
    jmp short 02356h                          ; eb 59                       ; 0xc22fb
    cmp ah, 004h                              ; 80 fc 04                    ; 0xc22fd
    jnc short 0234bh                          ; 73 49                       ; 0xc2300
    mov byte [bp-006h], ch                    ; 88 6e fa                    ; 0xc2302 vgabios.c:1455
    mov byte [bp-005h], 000h                  ; c6 46 fb 00                 ; 0xc2305
    mov di, word [bp-008h]                    ; 8b 7e f8                    ; 0xc2309
    add di, word [bp-006h]                    ; 03 7e fa                    ; 0xc230c
    add di, si                                ; 01 f7                       ; 0xc230f
    mov cl, byte [di]                         ; 8a 0d                       ; 0xc2311
    mov byte [bp-00ah], cl                    ; 88 4e f6                    ; 0xc2313
    mov byte [bp-009h], 000h                  ; c6 46 f7 00                 ; 0xc2316
    mov byte [bp-006h], dl                    ; 88 56 fa                    ; 0xc231a
    mov byte [bp-005h], 000h                  ; c6 46 fb 00                 ; 0xc231d
    mov di, word [bp-00ah]                    ; 8b 7e f6                    ; 0xc2321
    test word [bp-006h], di                   ; 85 7e fa                    ; 0xc2324
    je short 02345h                           ; 74 1c                       ; 0xc2327
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc2329 vgabios.c:1456
    sub cl, ah                                ; 28 e1                       ; 0xc232b
    mov dh, byte [bp-002h]                    ; 8a 76 fe                    ; 0xc232d
    and dh, 003h                              ; 80 e6 03                    ; 0xc2330
    sal cl, 1                                 ; d0 e1                       ; 0xc2333
    sal dh, CL                                ; d2 e6                       ; 0xc2335
    mov cl, dh                                ; 88 f1                       ; 0xc2337
    test byte [bp-002h], 080h                 ; f6 46 fe 80                 ; 0xc2339 vgabios.c:1457
    je short 02343h                           ; 74 04                       ; 0xc233d
    xor al, dh                                ; 30 f0                       ; 0xc233f vgabios.c:1459
    jmp short 02345h                          ; eb 02                       ; 0xc2341 vgabios.c:1461
    or al, dh                                 ; 08 f0                       ; 0xc2343 vgabios.c:1463
    shr dl, 1                                 ; d0 ea                       ; 0xc2345 vgabios.c:1466
    db  0feh, 0c4h
    ; inc ah                                    ; fe c4                     ; 0xc2347 vgabios.c:1467
    jmp short 022fdh                          ; eb b2                       ; 0xc2349
    mov di, 0b800h                            ; bf 00 b8                    ; 0xc234b vgabios.c:42
    mov es, di                                ; 8e c7                       ; 0xc234e
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2350
    inc bx                                    ; 43                          ; 0xc2353 vgabios.c:1469
    jmp short 022e1h                          ; eb 8b                       ; 0xc2354 vgabios.c:1470
    mov sp, bp                                ; 89 ec                       ; 0xc2356 vgabios.c:1473
    pop bp                                    ; 5d                          ; 0xc2358
    pop di                                    ; 5f                          ; 0xc2359
    pop si                                    ; 5e                          ; 0xc235a
    retn 00004h                               ; c2 04 00                    ; 0xc235b
  ; disGetNextSymbol 0xc235e LB 0x1f4b -> off=0x0 cb=00000000000000a1 uValue=00000000000c235e 'write_gfx_char_lin'
write_gfx_char_lin:                          ; 0xc235e LB 0xa1
    push si                                   ; 56                          ; 0xc235e vgabios.c:1476
    push di                                   ; 57                          ; 0xc235f
    push bp                                   ; 55                          ; 0xc2360
    mov bp, sp                                ; 89 e5                       ; 0xc2361
    sub sp, strict byte 00008h                ; 83 ec 08                    ; 0xc2363
    mov bh, al                                ; 88 c7                       ; 0xc2366
    mov ch, dl                                ; 88 d5                       ; 0xc2368
    mov al, cl                                ; 88 c8                       ; 0xc236a
    mov di, 0556ch                            ; bf 6c 55                    ; 0xc236c vgabios.c:1483
    xor ah, ah                                ; 30 e4                       ; 0xc236f vgabios.c:1484
    mov dl, byte [bp+008h]                    ; 8a 56 08                    ; 0xc2371
    xor dh, dh                                ; 30 f6                       ; 0xc2374
    imul dx                                   ; f7 ea                       ; 0xc2376
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc2378
    mov dx, ax                                ; 89 c2                       ; 0xc237a
    sal dx, CL                                ; d3 e2                       ; 0xc237c
    mov al, bl                                ; 88 d8                       ; 0xc237e
    xor ah, ah                                ; 30 e4                       ; 0xc2380
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc2382
    sal ax, CL                                ; d3 e0                       ; 0xc2384
    add ax, dx                                ; 01 d0                       ; 0xc2386
    mov word [bp-002h], ax                    ; 89 46 fe                    ; 0xc2388
    mov al, bh                                ; 88 f8                       ; 0xc238b vgabios.c:1485
    xor ah, ah                                ; 30 e4                       ; 0xc238d
    sal ax, CL                                ; d3 e0                       ; 0xc238f
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc2391
    xor bl, bl                                ; 30 db                       ; 0xc2394 vgabios.c:1486
    jmp short 023dah                          ; eb 42                       ; 0xc2396
    cmp AL, strict byte 008h                  ; 3c 08                       ; 0xc2398 vgabios.c:1490
    jnc short 023d3h                          ; 73 37                       ; 0xc239a
    xor bh, bh                                ; 30 ff                       ; 0xc239c vgabios.c:1492
    mov dl, bl                                ; 88 da                       ; 0xc239e vgabios.c:1493
    xor dh, dh                                ; 30 f6                       ; 0xc23a0
    add dx, word [bp-006h]                    ; 03 56 fa                    ; 0xc23a2
    mov si, di                                ; 89 fe                       ; 0xc23a5
    add si, dx                                ; 01 d6                       ; 0xc23a7
    mov dl, byte [si]                         ; 8a 14                       ; 0xc23a9
    mov byte [bp-004h], dl                    ; 88 56 fc                    ; 0xc23ab
    mov byte [bp-003h], bh                    ; 88 7e fd                    ; 0xc23ae
    mov dl, ah                                ; 88 e2                       ; 0xc23b1
    xor dh, dh                                ; 30 f6                       ; 0xc23b3
    test word [bp-004h], dx                   ; 85 56 fc                    ; 0xc23b5
    je short 023bch                           ; 74 02                       ; 0xc23b8
    mov bh, ch                                ; 88 ef                       ; 0xc23ba vgabios.c:1495
    mov dl, al                                ; 88 c2                       ; 0xc23bc vgabios.c:1497
    xor dh, dh                                ; 30 f6                       ; 0xc23be
    mov si, word [bp-008h]                    ; 8b 76 f8                    ; 0xc23c0
    add si, dx                                ; 01 d6                       ; 0xc23c3
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc23c5 vgabios.c:42
    mov es, dx                                ; 8e c2                       ; 0xc23c8
    mov byte [es:si], bh                      ; 26 88 3c                    ; 0xc23ca
    shr ah, 1                                 ; d0 ec                       ; 0xc23cd vgabios.c:1498
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc23cf vgabios.c:1499
    jmp short 02398h                          ; eb c5                       ; 0xc23d1
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3                     ; 0xc23d3 vgabios.c:1500
    cmp bl, 008h                              ; 80 fb 08                    ; 0xc23d5
    jnc short 023f7h                          ; 73 1d                       ; 0xc23d8
    mov al, bl                                ; 88 d8                       ; 0xc23da
    xor ah, ah                                ; 30 e4                       ; 0xc23dc
    mov dl, byte [bp+008h]                    ; 8a 56 08                    ; 0xc23de
    xor dh, dh                                ; 30 f6                       ; 0xc23e1
    imul dx                                   ; f7 ea                       ; 0xc23e3
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc23e5
    sal ax, CL                                ; d3 e0                       ; 0xc23e7
    mov dx, word [bp-002h]                    ; 8b 56 fe                    ; 0xc23e9
    add dx, ax                                ; 01 c2                       ; 0xc23ec
    mov word [bp-008h], dx                    ; 89 56 f8                    ; 0xc23ee
    mov AH, strict byte 080h                  ; b4 80                       ; 0xc23f1
    xor al, al                                ; 30 c0                       ; 0xc23f3
    jmp short 0239ch                          ; eb a5                       ; 0xc23f5
    mov sp, bp                                ; 89 ec                       ; 0xc23f7 vgabios.c:1501
    pop bp                                    ; 5d                          ; 0xc23f9
    pop di                                    ; 5f                          ; 0xc23fa
    pop si                                    ; 5e                          ; 0xc23fb
    retn 00002h                               ; c2 02 00                    ; 0xc23fc
  ; disGetNextSymbol 0xc23ff LB 0x1eaa -> off=0x0 cb=0000000000000173 uValue=00000000000c23ff 'biosfn_write_char_attr'
biosfn_write_char_attr:                      ; 0xc23ff LB 0x173
    push bp                                   ; 55                          ; 0xc23ff vgabios.c:1504
    mov bp, sp                                ; 89 e5                       ; 0xc2400
    push si                                   ; 56                          ; 0xc2402
    push di                                   ; 57                          ; 0xc2403
    sub sp, strict byte 0001ah                ; 83 ec 1a                    ; 0xc2404
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc2407
    mov byte [bp-00eh], dl                    ; 88 56 f2                    ; 0xc240a
    mov byte [bp-008h], bl                    ; 88 5e f8                    ; 0xc240d
    mov si, cx                                ; 89 ce                       ; 0xc2410
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc2412 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2415
    mov es, ax                                ; 8e c0                       ; 0xc2418
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc241a
    xor ah, ah                                ; 30 e4                       ; 0xc241d vgabios.c:1512
    call 03651h                               ; e8 2f 12                    ; 0xc241f
    mov cl, al                                ; 88 c1                       ; 0xc2422
    mov byte [bp-014h], al                    ; 88 46 ec                    ; 0xc2424
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc2427 vgabios.c:1513
    jne short 0242eh                          ; 75 03                       ; 0xc2429
    jmp near 0256bh                           ; e9 3d 01                    ; 0xc242b
    mov al, dl                                ; 88 d0                       ; 0xc242e vgabios.c:1516
    xor ah, ah                                ; 30 e4                       ; 0xc2430
    lea bx, [bp-01ch]                         ; 8d 5e e4                    ; 0xc2432
    lea dx, [bp-01eh]                         ; 8d 56 e2                    ; 0xc2435
    call 00a0ch                               ; e8 d1 e5                    ; 0xc2438
    mov al, byte [bp-01ch]                    ; 8a 46 e4                    ; 0xc243b vgabios.c:1517
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc243e
    mov ax, word [bp-01ch]                    ; 8b 46 e4                    ; 0xc2441
    mov al, ah                                ; 88 e0                       ; 0xc2444
    xor ah, ah                                ; 30 e4                       ; 0xc2446
    mov word [bp-018h], ax                    ; 89 46 e8                    ; 0xc2448
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc244b
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc244e
    mov bx, 00084h                            ; bb 84 00                    ; 0xc2451 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2454
    mov es, ax                                ; 8e c0                       ; 0xc2457
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc2459
    xor ah, ah                                ; 30 e4                       ; 0xc245c vgabios.c:38
    mov dx, ax                                ; 89 c2                       ; 0xc245e
    inc dx                                    ; 42                          ; 0xc2460
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc2461 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc2464
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc2467
    mov word [bp-01ah], ax                    ; 89 46 e6                    ; 0xc246a vgabios.c:48
    mov bl, cl                                ; 88 cb                       ; 0xc246d vgabios.c:1523
    xor bh, bh                                ; 30 ff                       ; 0xc246f
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc2471
    mov di, bx                                ; 89 df                       ; 0xc2473
    sal di, CL                                ; d3 e7                       ; 0xc2475
    cmp byte [di+047afh], 000h                ; 80 bd af 47 00              ; 0xc2477
    jne short 024bfh                          ; 75 41                       ; 0xc247c
    mul dx                                    ; f7 e2                       ; 0xc247e vgabios.c:1526
    sal ax, 1                                 ; d1 e0                       ; 0xc2480
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc2482
    mov dl, byte [bp-00eh]                    ; 8a 56 f2                    ; 0xc2484
    xor dh, dh                                ; 30 f6                       ; 0xc2487
    inc ax                                    ; 40                          ; 0xc2489
    mul dx                                    ; f7 e2                       ; 0xc248a
    mov bx, ax                                ; 89 c3                       ; 0xc248c
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc248e
    xor ah, ah                                ; 30 e4                       ; 0xc2491
    mul word [bp-016h]                        ; f7 66 ea                    ; 0xc2493
    mov dx, ax                                ; 89 c2                       ; 0xc2496
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2498
    xor ah, ah                                ; 30 e4                       ; 0xc249b
    add ax, dx                                ; 01 d0                       ; 0xc249d
    sal ax, 1                                 ; d1 e0                       ; 0xc249f
    add bx, ax                                ; 01 c3                       ; 0xc24a1
    mov ah, byte [bp-008h]                    ; 8a 66 f8                    ; 0xc24a3 vgabios.c:1528
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc24a6
    mov word [bp-01eh], ax                    ; 89 46 e2                    ; 0xc24a9
    mov ax, word [bp-01eh]                    ; 8b 46 e2                    ; 0xc24ac vgabios.c:1529
    mov es, [di+047b2h]                       ; 8e 85 b2 47                 ; 0xc24af
    mov cx, si                                ; 89 f1                       ; 0xc24b3
    mov di, bx                                ; 89 df                       ; 0xc24b5
    cld                                       ; fc                          ; 0xc24b7
    jcxz 024bch                               ; e3 02                       ; 0xc24b8
    rep stosw                                 ; f3 ab                       ; 0xc24ba
    jmp near 0256bh                           ; e9 ac 00                    ; 0xc24bc vgabios.c:1531
    mov bl, byte [bx+0482eh]                  ; 8a 9f 2e 48                 ; 0xc24bf vgabios.c:1534
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc24c3
    sal bx, CL                                ; d3 e3                       ; 0xc24c5
    mov al, byte [bx+04844h]                  ; 8a 87 44 48                 ; 0xc24c7
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc24cb
    mov al, byte [di+047b1h]                  ; 8a 85 b1 47                 ; 0xc24ce vgabios.c:1535
    mov byte [bp-012h], al                    ; 88 46 ee                    ; 0xc24d2
    dec si                                    ; 4e                          ; 0xc24d5 vgabios.c:1536
    cmp si, strict byte 0ffffh                ; 83 fe ff                    ; 0xc24d6
    je short 02527h                           ; 74 4c                       ; 0xc24d9
    mov bl, byte [bp-014h]                    ; 8a 5e ec                    ; 0xc24db vgabios.c:1538
    xor bh, bh                                ; 30 ff                       ; 0xc24de
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc24e0
    sal bx, CL                                ; d3 e3                       ; 0xc24e2
    mov al, byte [bx+047b0h]                  ; 8a 87 b0 47                 ; 0xc24e4
    cmp al, cl                                ; 38 c8                       ; 0xc24e8
    jc short 024f8h                           ; 72 0c                       ; 0xc24ea
    jbe short 024feh                          ; 76 10                       ; 0xc24ec
    cmp AL, strict byte 005h                  ; 3c 05                       ; 0xc24ee
    je short 0254ah                           ; 74 58                       ; 0xc24f0
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc24f2
    je short 02502h                           ; 74 0c                       ; 0xc24f4
    jmp short 02565h                          ; eb 6d                       ; 0xc24f6
    cmp AL, strict byte 002h                  ; 3c 02                       ; 0xc24f8
    je short 02529h                           ; 74 2d                       ; 0xc24fa
    jmp short 02565h                          ; eb 67                       ; 0xc24fc
    or byte [bp-008h], 001h                   ; 80 4e f8 01                 ; 0xc24fe vgabios.c:1541
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc2502 vgabios.c:1543
    xor ah, ah                                ; 30 e4                       ; 0xc2505
    push ax                                   ; 50                          ; 0xc2507
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc2508
    push ax                                   ; 50                          ; 0xc250b
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc250c
    push ax                                   ; 50                          ; 0xc250f
    mov cl, byte [bp-006h]                    ; 8a 4e fa                    ; 0xc2510
    xor ch, ch                                ; 30 ed                       ; 0xc2513
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc2515
    xor bh, bh                                ; 30 ff                       ; 0xc2518
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc251a
    xor dh, dh                                ; 30 f6                       ; 0xc251d
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc251f
    call 0213ah                               ; e8 15 fc                    ; 0xc2522
    jmp short 02565h                          ; eb 3e                       ; 0xc2525 vgabios.c:1544
    jmp short 0256bh                          ; eb 42                       ; 0xc2527
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc2529 vgabios.c:1546
    xor ah, ah                                ; 30 e4                       ; 0xc252c
    push ax                                   ; 50                          ; 0xc252e
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc252f
    push ax                                   ; 50                          ; 0xc2532
    mov cl, byte [bp-006h]                    ; 8a 4e fa                    ; 0xc2533
    xor ch, ch                                ; 30 ed                       ; 0xc2536
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc2538
    xor bh, bh                                ; 30 ff                       ; 0xc253b
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc253d
    xor dh, dh                                ; 30 f6                       ; 0xc2540
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc2542
    call 0224ch                               ; e8 04 fd                    ; 0xc2545
    jmp short 02565h                          ; eb 1b                       ; 0xc2548 vgabios.c:1547
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc254a vgabios.c:1549
    xor ah, ah                                ; 30 e4                       ; 0xc254d
    push ax                                   ; 50                          ; 0xc254f
    mov cl, byte [bp-006h]                    ; 8a 4e fa                    ; 0xc2550
    xor ch, ch                                ; 30 ed                       ; 0xc2553
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc2555
    xor bh, bh                                ; 30 ff                       ; 0xc2558
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc255a
    xor dh, dh                                ; 30 f6                       ; 0xc255d
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc255f
    call 0235eh                               ; e8 f9 fd                    ; 0xc2562
    inc byte [bp-00ah]                        ; fe 46 f6                    ; 0xc2565 vgabios.c:1556
    jmp near 024d5h                           ; e9 6a ff                    ; 0xc2568 vgabios.c:1557
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc256b vgabios.c:1559
    pop di                                    ; 5f                          ; 0xc256e
    pop si                                    ; 5e                          ; 0xc256f
    pop bp                                    ; 5d                          ; 0xc2570
    retn                                      ; c3                          ; 0xc2571
  ; disGetNextSymbol 0xc2572 LB 0x1d37 -> off=0x0 cb=0000000000000183 uValue=00000000000c2572 'biosfn_write_char_only'
biosfn_write_char_only:                      ; 0xc2572 LB 0x183
    push bp                                   ; 55                          ; 0xc2572 vgabios.c:1562
    mov bp, sp                                ; 89 e5                       ; 0xc2573
    push si                                   ; 56                          ; 0xc2575
    push di                                   ; 57                          ; 0xc2576
    sub sp, strict byte 0001ah                ; 83 ec 1a                    ; 0xc2577
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc257a
    mov byte [bp-00eh], dl                    ; 88 56 f2                    ; 0xc257d
    mov byte [bp-008h], bl                    ; 88 5e f8                    ; 0xc2580
    mov si, cx                                ; 89 ce                       ; 0xc2583
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc2585 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2588
    mov es, ax                                ; 8e c0                       ; 0xc258b
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc258d
    xor ah, ah                                ; 30 e4                       ; 0xc2590 vgabios.c:1570
    call 03651h                               ; e8 bc 10                    ; 0xc2592
    mov cl, al                                ; 88 c1                       ; 0xc2595
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc2597
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc259a vgabios.c:1571
    jne short 025a1h                          ; 75 03                       ; 0xc259c
    jmp near 026eeh                           ; e9 4d 01                    ; 0xc259e
    mov al, dl                                ; 88 d0                       ; 0xc25a1 vgabios.c:1574
    xor ah, ah                                ; 30 e4                       ; 0xc25a3
    lea bx, [bp-01eh]                         ; 8d 5e e2                    ; 0xc25a5
    lea dx, [bp-01ch]                         ; 8d 56 e4                    ; 0xc25a8
    call 00a0ch                               ; e8 5e e4                    ; 0xc25ab
    mov al, byte [bp-01eh]                    ; 8a 46 e2                    ; 0xc25ae vgabios.c:1575
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc25b1
    mov ax, word [bp-01eh]                    ; 8b 46 e2                    ; 0xc25b4
    mov al, ah                                ; 88 e0                       ; 0xc25b7
    xor ah, ah                                ; 30 e4                       ; 0xc25b9
    mov word [bp-01ah], ax                    ; 89 46 e6                    ; 0xc25bb
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc25be
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc25c1
    mov bx, 00084h                            ; bb 84 00                    ; 0xc25c4 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc25c7
    mov es, ax                                ; 8e c0                       ; 0xc25ca
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc25cc
    xor ah, ah                                ; 30 e4                       ; 0xc25cf vgabios.c:38
    mov dx, ax                                ; 89 c2                       ; 0xc25d1
    inc dx                                    ; 42                          ; 0xc25d3
    mov di, strict word 0004ah                ; bf 4a 00                    ; 0xc25d4 vgabios.c:47
    mov di, word [es:di]                      ; 26 8b 3d                    ; 0xc25d7
    mov word [bp-018h], di                    ; 89 7e e8                    ; 0xc25da vgabios.c:48
    mov al, cl                                ; 88 c8                       ; 0xc25dd vgabios.c:1581
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc25df
    mov bx, ax                                ; 89 c3                       ; 0xc25e1
    sal bx, CL                                ; d3 e3                       ; 0xc25e3
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc25e5
    jne short 02631h                          ; 75 45                       ; 0xc25ea
    mov ax, di                                ; 89 f8                       ; 0xc25ec vgabios.c:1584
    mul dx                                    ; f7 e2                       ; 0xc25ee
    sal ax, 1                                 ; d1 e0                       ; 0xc25f0
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc25f2
    mov dl, byte [bp-00eh]                    ; 8a 56 f2                    ; 0xc25f4
    xor dh, dh                                ; 30 f6                       ; 0xc25f7
    inc ax                                    ; 40                          ; 0xc25f9
    mul dx                                    ; f7 e2                       ; 0xc25fa
    mov bx, ax                                ; 89 c3                       ; 0xc25fc
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc25fe
    xor ah, ah                                ; 30 e4                       ; 0xc2601
    mul di                                    ; f7 e7                       ; 0xc2603
    mov dx, ax                                ; 89 c2                       ; 0xc2605
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc2607
    xor ah, ah                                ; 30 e4                       ; 0xc260a
    add ax, dx                                ; 01 d0                       ; 0xc260c
    sal ax, 1                                 ; d1 e0                       ; 0xc260e
    add bx, ax                                ; 01 c3                       ; 0xc2610
    dec si                                    ; 4e                          ; 0xc2612 vgabios.c:1586
    cmp si, strict byte 0ffffh                ; 83 fe ff                    ; 0xc2613
    je short 0259eh                           ; 74 86                       ; 0xc2616
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc2618 vgabios.c:1587
    xor ah, ah                                ; 30 e4                       ; 0xc261b
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc261d
    mov di, ax                                ; 89 c7                       ; 0xc261f
    sal di, CL                                ; d3 e7                       ; 0xc2621
    mov es, [di+047b2h]                       ; 8e 85 b2 47                 ; 0xc2623 vgabios.c:40
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2627 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc262a
    inc bx                                    ; 43                          ; 0xc262d vgabios.c:1588
    inc bx                                    ; 43                          ; 0xc262e
    jmp short 02612h                          ; eb e1                       ; 0xc262f vgabios.c:1589
    mov di, ax                                ; 89 c7                       ; 0xc2631 vgabios.c:1594
    mov al, byte [di+0482eh]                  ; 8a 85 2e 48                 ; 0xc2633
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc2637
    mov di, ax                                ; 89 c7                       ; 0xc2639
    sal di, CL                                ; d3 e7                       ; 0xc263b
    mov al, byte [di+04844h]                  ; 8a 85 44 48                 ; 0xc263d
    mov byte [bp-012h], al                    ; 88 46 ee                    ; 0xc2641
    mov al, byte [bx+047b1h]                  ; 8a 87 b1 47                 ; 0xc2644 vgabios.c:1595
    mov byte [bp-014h], al                    ; 88 46 ec                    ; 0xc2648
    dec si                                    ; 4e                          ; 0xc264b vgabios.c:1596
    cmp si, strict byte 0ffffh                ; 83 fe ff                    ; 0xc264c
    je short 026a1h                           ; 74 50                       ; 0xc264f
    mov bl, byte [bp-010h]                    ; 8a 5e f0                    ; 0xc2651 vgabios.c:1598
    xor bh, bh                                ; 30 ff                       ; 0xc2654
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc2656
    sal bx, CL                                ; d3 e3                       ; 0xc2658
    mov bl, byte [bx+047b0h]                  ; 8a 9f b0 47                 ; 0xc265a
    cmp bl, cl                                ; 38 cb                       ; 0xc265e
    jc short 02671h                           ; 72 0f                       ; 0xc2660
    jbe short 02678h                          ; 76 14                       ; 0xc2662
    cmp bl, 005h                              ; 80 fb 05                    ; 0xc2664
    je short 026cdh                           ; 74 64                       ; 0xc2667
    cmp bl, 004h                              ; 80 fb 04                    ; 0xc2669
    je short 0267ch                           ; 74 0e                       ; 0xc266c
    jmp near 026e8h                           ; e9 77 00                    ; 0xc266e
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc2671
    je short 026a3h                           ; 74 2d                       ; 0xc2674
    jmp short 026e8h                          ; eb 70                       ; 0xc2676
    or byte [bp-008h], 001h                   ; 80 4e f8 01                 ; 0xc2678 vgabios.c:1601
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc267c vgabios.c:1603
    xor ah, ah                                ; 30 e4                       ; 0xc267f
    push ax                                   ; 50                          ; 0xc2681
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc2682
    push ax                                   ; 50                          ; 0xc2685
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc2686
    push ax                                   ; 50                          ; 0xc2689
    mov cl, byte [bp-00ch]                    ; 8a 4e f4                    ; 0xc268a
    xor ch, ch                                ; 30 ed                       ; 0xc268d
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc268f
    xor bh, bh                                ; 30 ff                       ; 0xc2692
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc2694
    xor dh, dh                                ; 30 f6                       ; 0xc2697
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2699
    call 0213ah                               ; e8 9b fa                    ; 0xc269c
    jmp short 026e8h                          ; eb 47                       ; 0xc269f vgabios.c:1604
    jmp short 026eeh                          ; eb 4b                       ; 0xc26a1
    mov al, byte [bp-014h]                    ; 8a 46 ec                    ; 0xc26a3 vgabios.c:1606
    xor ah, ah                                ; 30 e4                       ; 0xc26a6
    push ax                                   ; 50                          ; 0xc26a8
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc26a9
    push ax                                   ; 50                          ; 0xc26ac
    mov cl, byte [bp-00ch]                    ; 8a 4e f4                    ; 0xc26ad
    xor ch, ch                                ; 30 ed                       ; 0xc26b0
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc26b2
    xor bh, bh                                ; 30 ff                       ; 0xc26b5
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc26b7
    xor dh, dh                                ; 30 f6                       ; 0xc26ba
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc26bc
    mov byte [bp-016h], al                    ; 88 46 ea                    ; 0xc26bf
    mov byte [bp-015h], ah                    ; 88 66 eb                    ; 0xc26c2
    mov ax, word [bp-016h]                    ; 8b 46 ea                    ; 0xc26c5
    call 0224ch                               ; e8 81 fb                    ; 0xc26c8
    jmp short 026e8h                          ; eb 1b                       ; 0xc26cb vgabios.c:1607
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc26cd vgabios.c:1609
    xor ah, ah                                ; 30 e4                       ; 0xc26d0
    push ax                                   ; 50                          ; 0xc26d2
    mov cl, byte [bp-00ch]                    ; 8a 4e f4                    ; 0xc26d3
    xor ch, ch                                ; 30 ed                       ; 0xc26d6
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc26d8
    xor bh, bh                                ; 30 ff                       ; 0xc26db
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc26dd
    xor dh, dh                                ; 30 f6                       ; 0xc26e0
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc26e2
    call 0235eh                               ; e8 76 fc                    ; 0xc26e5
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc26e8 vgabios.c:1616
    jmp near 0264bh                           ; e9 5d ff                    ; 0xc26eb vgabios.c:1617
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc26ee vgabios.c:1619
    pop di                                    ; 5f                          ; 0xc26f1
    pop si                                    ; 5e                          ; 0xc26f2
    pop bp                                    ; 5d                          ; 0xc26f3
    retn                                      ; c3                          ; 0xc26f4
  ; disGetNextSymbol 0xc26f5 LB 0x1bb4 -> off=0x0 cb=000000000000017a uValue=00000000000c26f5 'biosfn_write_pixel'
biosfn_write_pixel:                          ; 0xc26f5 LB 0x17a
    push bp                                   ; 55                          ; 0xc26f5 vgabios.c:1622
    mov bp, sp                                ; 89 e5                       ; 0xc26f6
    push si                                   ; 56                          ; 0xc26f8
    sub sp, strict byte 00008h                ; 83 ec 08                    ; 0xc26f9
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc26fc
    mov byte [bp-004h], dl                    ; 88 56 fc                    ; 0xc26ff
    mov word [bp-008h], bx                    ; 89 5e f8                    ; 0xc2702
    mov word [bp-00ah], cx                    ; 89 4e f6                    ; 0xc2705
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc2708 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc270b
    mov es, ax                                ; 8e c0                       ; 0xc270e
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc2710
    xor ah, ah                                ; 30 e4                       ; 0xc2713 vgabios.c:1629
    call 03651h                               ; e8 39 0f                    ; 0xc2715
    mov ch, al                                ; 88 c5                       ; 0xc2718
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc271a vgabios.c:1630
    je short 02745h                           ; 74 27                       ; 0xc271c
    mov bl, al                                ; 88 c3                       ; 0xc271e vgabios.c:1631
    xor bh, bh                                ; 30 ff                       ; 0xc2720
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc2722
    sal bx, CL                                ; d3 e3                       ; 0xc2724
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc2726
    je short 02745h                           ; 74 18                       ; 0xc272b
    mov al, byte [bx+047b0h]                  ; 8a 87 b0 47                 ; 0xc272d vgabios.c:1633
    cmp al, cl                                ; 38 c8                       ; 0xc2731
    jc short 02741h                           ; 72 0c                       ; 0xc2733
    jbe short 0274bh                          ; 76 14                       ; 0xc2735
    cmp AL, strict byte 005h                  ; 3c 05                       ; 0xc2737
    je short 02748h                           ; 74 0d                       ; 0xc2739
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc273b
    je short 0274bh                           ; 74 0c                       ; 0xc273d
    jmp short 02745h                          ; eb 04                       ; 0xc273f
    cmp AL, strict byte 002h                  ; 3c 02                       ; 0xc2741
    je short 027bdh                           ; 74 78                       ; 0xc2743
    jmp near 02848h                           ; e9 00 01                    ; 0xc2745
    jmp near 0284eh                           ; e9 03 01                    ; 0xc2748
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc274b vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc274e
    mov es, ax                                ; 8e c0                       ; 0xc2751
    mov dx, word [es:bx]                      ; 26 8b 17                    ; 0xc2753
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc2756 vgabios.c:48
    mul dx                                    ; f7 e2                       ; 0xc2759
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc275b
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc275d
    shr bx, CL                                ; d3 eb                       ; 0xc2760
    add bx, ax                                ; 01 c3                       ; 0xc2762
    mov si, strict word 0004ch                ; be 4c 00                    ; 0xc2764 vgabios.c:47
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc2767
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc276a vgabios.c:48
    xor dh, dh                                ; 30 f6                       ; 0xc276d
    mul dx                                    ; f7 e2                       ; 0xc276f
    add bx, ax                                ; 01 c3                       ; 0xc2771
    mov cx, word [bp-008h]                    ; 8b 4e f8                    ; 0xc2773 vgabios.c:1639
    and cl, 007h                              ; 80 e1 07                    ; 0xc2776
    mov ax, 00080h                            ; b8 80 00                    ; 0xc2779
    sar ax, CL                                ; d3 f8                       ; 0xc277c
    mov ah, al                                ; 88 c4                       ; 0xc277e vgabios.c:1640
    xor al, al                                ; 30 c0                       ; 0xc2780
    or AL, strict byte 008h                   ; 0c 08                       ; 0xc2782
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2784
    out DX, ax                                ; ef                          ; 0xc2787
    mov ax, 00205h                            ; b8 05 02                    ; 0xc2788 vgabios.c:1641
    out DX, ax                                ; ef                          ; 0xc278b
    mov dx, bx                                ; 89 da                       ; 0xc278c vgabios.c:1642
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc278e
    call 0367ch                               ; e8 e8 0e                    ; 0xc2791
    test byte [bp-004h], 080h                 ; f6 46 fc 80                 ; 0xc2794 vgabios.c:1643
    je short 027a1h                           ; 74 07                       ; 0xc2798
    mov ax, 01803h                            ; b8 03 18                    ; 0xc279a vgabios.c:1645
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc279d
    out DX, ax                                ; ef                          ; 0xc27a0
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc27a1 vgabios.c:42
    mov es, ax                                ; 8e c0                       ; 0xc27a4
    mov al, byte [bp-004h]                    ; 8a 46 fc                    ; 0xc27a6
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc27a9
    mov ax, 0ff08h                            ; b8 08 ff                    ; 0xc27ac vgabios.c:1648
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc27af
    out DX, ax                                ; ef                          ; 0xc27b2
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc27b3 vgabios.c:1649
    out DX, ax                                ; ef                          ; 0xc27b6
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc27b7 vgabios.c:1650
    out DX, ax                                ; ef                          ; 0xc27ba
    jmp short 02745h                          ; eb 88                       ; 0xc27bb vgabios.c:1651
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc27bd vgabios.c:1653
    shr ax, 1                                 ; d1 e8                       ; 0xc27c0
    mov dx, strict word 00050h                ; ba 50 00                    ; 0xc27c2
    mul dx                                    ; f7 e2                       ; 0xc27c5
    cmp byte [bx+047b1h], 002h                ; 80 bf b1 47 02              ; 0xc27c7
    jne short 027d7h                          ; 75 09                       ; 0xc27cc
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc27ce vgabios.c:1655
    shr bx, 1                                 ; d1 eb                       ; 0xc27d1
    shr bx, 1                                 ; d1 eb                       ; 0xc27d3
    jmp short 027dch                          ; eb 05                       ; 0xc27d5 vgabios.c:1657
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc27d7 vgabios.c:1659
    shr bx, CL                                ; d3 eb                       ; 0xc27da
    add bx, ax                                ; 01 c3                       ; 0xc27dc
    test byte [bp-00ah], 001h                 ; f6 46 f6 01                 ; 0xc27de vgabios.c:1661
    je short 027e7h                           ; 74 03                       ; 0xc27e2
    add bh, 020h                              ; 80 c7 20                    ; 0xc27e4
    mov ax, 0b800h                            ; b8 00 b8                    ; 0xc27e7 vgabios.c:37
    mov es, ax                                ; 8e c0                       ; 0xc27ea
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc27ec
    mov dl, ch                                ; 88 ea                       ; 0xc27ef vgabios.c:1663
    xor dh, dh                                ; 30 f6                       ; 0xc27f1
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc27f3
    mov si, dx                                ; 89 d6                       ; 0xc27f5
    sal si, CL                                ; d3 e6                       ; 0xc27f7
    cmp byte [si+047b1h], 002h                ; 80 bc b1 47 02              ; 0xc27f9
    jne short 0281ah                          ; 75 1a                       ; 0xc27fe
    mov ah, byte [bp-008h]                    ; 8a 66 f8                    ; 0xc2800 vgabios.c:1665
    and ah, cl                                ; 20 cc                       ; 0xc2803
    mov dl, cl                                ; 88 ca                       ; 0xc2805
    sub dl, ah                                ; 28 e2                       ; 0xc2807
    mov ah, dl                                ; 88 d4                       ; 0xc2809
    sal ah, 1                                 ; d0 e4                       ; 0xc280b
    mov dl, byte [bp-004h]                    ; 8a 56 fc                    ; 0xc280d
    and dl, cl                                ; 20 ca                       ; 0xc2810
    mov cl, ah                                ; 88 e1                       ; 0xc2812
    sal dl, CL                                ; d2 e2                       ; 0xc2814
    mov AH, strict byte 003h                  ; b4 03                       ; 0xc2816 vgabios.c:1666
    jmp short 0282eh                          ; eb 14                       ; 0xc2818 vgabios.c:1668
    mov ah, byte [bp-008h]                    ; 8a 66 f8                    ; 0xc281a vgabios.c:1670
    and ah, 007h                              ; 80 e4 07                    ; 0xc281d
    mov CL, strict byte 007h                  ; b1 07                       ; 0xc2820
    sub cl, ah                                ; 28 e1                       ; 0xc2822
    mov dl, byte [bp-004h]                    ; 8a 56 fc                    ; 0xc2824
    and dl, 001h                              ; 80 e2 01                    ; 0xc2827
    sal dl, CL                                ; d2 e2                       ; 0xc282a
    mov AH, strict byte 001h                  ; b4 01                       ; 0xc282c vgabios.c:1671
    sal ah, CL                                ; d2 e4                       ; 0xc282e
    test byte [bp-004h], 080h                 ; f6 46 fc 80                 ; 0xc2830 vgabios.c:1673
    je short 0283ah                           ; 74 04                       ; 0xc2834
    xor al, dl                                ; 30 d0                       ; 0xc2836 vgabios.c:1675
    jmp short 02840h                          ; eb 06                       ; 0xc2838 vgabios.c:1677
    not ah                                    ; f6 d4                       ; 0xc283a vgabios.c:1679
    and al, ah                                ; 20 e0                       ; 0xc283c
    or al, dl                                 ; 08 d0                       ; 0xc283e vgabios.c:1680
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc2840 vgabios.c:42
    mov es, dx                                ; 8e c2                       ; 0xc2843
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2845
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc2848 vgabios.c:1683
    pop si                                    ; 5e                          ; 0xc284b
    pop bp                                    ; 5d                          ; 0xc284c
    retn                                      ; c3                          ; 0xc284d
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc284e vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2851
    mov es, ax                                ; 8e c0                       ; 0xc2854
    mov dx, word [es:bx]                      ; 26 8b 17                    ; 0xc2856
    sal dx, CL                                ; d3 e2                       ; 0xc2859 vgabios.c:48
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc285b
    mul dx                                    ; f7 e2                       ; 0xc285e
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc2860
    add bx, ax                                ; 01 c3                       ; 0xc2863
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2865 vgabios.c:42
    mov es, ax                                ; 8e c0                       ; 0xc2868
    mov al, byte [bp-004h]                    ; 8a 46 fc                    ; 0xc286a
    jmp short 02845h                          ; eb d6                       ; 0xc286d
  ; disGetNextSymbol 0xc286f LB 0x1a3a -> off=0x0 cb=0000000000000263 uValue=00000000000c286f 'biosfn_write_teletype'
biosfn_write_teletype:                       ; 0xc286f LB 0x263
    push bp                                   ; 55                          ; 0xc286f vgabios.c:1696
    mov bp, sp                                ; 89 e5                       ; 0xc2870
    push si                                   ; 56                          ; 0xc2872
    sub sp, strict byte 00016h                ; 83 ec 16                    ; 0xc2873
    mov ch, al                                ; 88 c5                       ; 0xc2876
    mov byte [bp-00ah], dl                    ; 88 56 f6                    ; 0xc2878
    mov byte [bp-00ch], bl                    ; 88 5e f4                    ; 0xc287b
    mov byte [bp-004h], cl                    ; 88 4e fc                    ; 0xc287e
    cmp dl, 0ffh                              ; 80 fa ff                    ; 0xc2881 vgabios.c:1704
    jne short 02894h                          ; 75 0e                       ; 0xc2884
    mov bx, strict word 00062h                ; bb 62 00                    ; 0xc2886 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2889
    mov es, ax                                ; 8e c0                       ; 0xc288c
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc288e
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc2891 vgabios.c:38
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc2894 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2897
    mov es, ax                                ; 8e c0                       ; 0xc289a
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc289c
    xor ah, ah                                ; 30 e4                       ; 0xc289f vgabios.c:1709
    call 03651h                               ; e8 ad 0d                    ; 0xc28a1
    mov byte [bp-00eh], al                    ; 88 46 f2                    ; 0xc28a4
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc28a7 vgabios.c:1710
    je short 02910h                           ; 74 65                       ; 0xc28a9
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc28ab vgabios.c:1713
    xor ah, ah                                ; 30 e4                       ; 0xc28ae
    lea bx, [bp-016h]                         ; 8d 5e ea                    ; 0xc28b0
    lea dx, [bp-018h]                         ; 8d 56 e8                    ; 0xc28b3
    call 00a0ch                               ; e8 53 e1                    ; 0xc28b6
    mov al, byte [bp-016h]                    ; 8a 46 ea                    ; 0xc28b9 vgabios.c:1714
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc28bc
    mov ax, word [bp-016h]                    ; 8b 46 ea                    ; 0xc28bf
    mov al, ah                                ; 88 e0                       ; 0xc28c2
    xor ah, ah                                ; 30 e4                       ; 0xc28c4
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc28c6
    mov bx, 00084h                            ; bb 84 00                    ; 0xc28c9 vgabios.c:37
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc28cc
    mov es, dx                                ; 8e c2                       ; 0xc28cf
    mov dl, byte [es:bx]                      ; 26 8a 17                    ; 0xc28d1
    xor dh, dh                                ; 30 f6                       ; 0xc28d4 vgabios.c:38
    inc dx                                    ; 42                          ; 0xc28d6
    mov word [bp-014h], dx                    ; 89 56 ec                    ; 0xc28d7
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc28da vgabios.c:47
    mov dx, word [es:bx]                      ; 26 8b 17                    ; 0xc28dd
    mov word [bp-012h], dx                    ; 89 56 ee                    ; 0xc28e0 vgabios.c:48
    cmp ch, 008h                              ; 80 fd 08                    ; 0xc28e3 vgabios.c:1720
    jc short 028f6h                           ; 72 0e                       ; 0xc28e6
    jbe short 028feh                          ; 76 14                       ; 0xc28e8
    cmp ch, 00dh                              ; 80 fd 0d                    ; 0xc28ea
    je short 02913h                           ; 74 24                       ; 0xc28ed
    cmp ch, 00ah                              ; 80 fd 0a                    ; 0xc28ef
    je short 02909h                           ; 74 15                       ; 0xc28f2
    jmp short 02919h                          ; eb 23                       ; 0xc28f4
    cmp ch, 007h                              ; 80 fd 07                    ; 0xc28f6
    jne short 02919h                          ; 75 1e                       ; 0xc28f9
    jmp near 02a21h                           ; e9 23 01                    ; 0xc28fb
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc28fe vgabios.c:1727
    jbe short 02916h                          ; 76 12                       ; 0xc2902
    dec byte [bp-006h]                        ; fe 4e fa                    ; 0xc2904
    jmp short 02916h                          ; eb 0d                       ; 0xc2907 vgabios.c:1728
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc2909 vgabios.c:1731
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc290b
    jmp short 02916h                          ; eb 06                       ; 0xc290e vgabios.c:1732
    jmp near 02acch                           ; e9 b9 01                    ; 0xc2910
    mov byte [bp-006h], ah                    ; 88 66 fa                    ; 0xc2913 vgabios.c:1735
    jmp near 02a21h                           ; e9 08 01                    ; 0xc2916 vgabios.c:1736
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc2919 vgabios.c:1740
    xor ah, ah                                ; 30 e4                       ; 0xc291c
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc291e
    mov bx, ax                                ; 89 c3                       ; 0xc2920
    sal bx, CL                                ; d3 e3                       ; 0xc2922
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc2924
    jne short 0296dh                          ; 75 42                       ; 0xc2929
    mov ax, word [bp-012h]                    ; 8b 46 ee                    ; 0xc292b vgabios.c:1743
    mul word [bp-014h]                        ; f7 66 ec                    ; 0xc292e
    sal ax, 1                                 ; d1 e0                       ; 0xc2931
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc2933
    mov dl, byte [bp-00ah]                    ; 8a 56 f6                    ; 0xc2935
    xor dh, dh                                ; 30 f6                       ; 0xc2938
    inc ax                                    ; 40                          ; 0xc293a
    mul dx                                    ; f7 e2                       ; 0xc293b
    mov si, ax                                ; 89 c6                       ; 0xc293d
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc293f
    xor ah, ah                                ; 30 e4                       ; 0xc2942
    mul word [bp-012h]                        ; f7 66 ee                    ; 0xc2944
    mov dx, ax                                ; 89 c2                       ; 0xc2947
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc2949
    xor ah, ah                                ; 30 e4                       ; 0xc294c
    add ax, dx                                ; 01 d0                       ; 0xc294e
    sal ax, 1                                 ; d1 e0                       ; 0xc2950
    add si, ax                                ; 01 c6                       ; 0xc2952
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc2954 vgabios.c:40
    mov byte [es:si], ch                      ; 26 88 2c                    ; 0xc2958 vgabios.c:42
    cmp cl, byte [bp-004h]                    ; 3a 4e fc                    ; 0xc295b vgabios.c:1748
    jne short 0299dh                          ; 75 3d                       ; 0xc295e
    inc si                                    ; 46                          ; 0xc2960 vgabios.c:1749
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc2961 vgabios.c:40
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc2965
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc2968
    jmp short 0299dh                          ; eb 30                       ; 0xc296b vgabios.c:1751
    mov si, ax                                ; 89 c6                       ; 0xc296d vgabios.c:1754
    mov al, byte [si+0482eh]                  ; 8a 84 2e 48                 ; 0xc296f
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc2973
    mov si, ax                                ; 89 c6                       ; 0xc2975
    sal si, CL                                ; d3 e6                       ; 0xc2977
    mov dl, byte [si+04844h]                  ; 8a 94 44 48                 ; 0xc2979
    mov al, byte [bx+047b1h]                  ; 8a 87 b1 47                 ; 0xc297d vgabios.c:1755
    mov bl, byte [bx+047b0h]                  ; 8a 9f b0 47                 ; 0xc2981 vgabios.c:1756
    cmp bl, 003h                              ; 80 fb 03                    ; 0xc2985
    jc short 02998h                           ; 72 0e                       ; 0xc2988
    jbe short 0299fh                          ; 76 13                       ; 0xc298a
    cmp bl, 005h                              ; 80 fb 05                    ; 0xc298c
    je short 029efh                           ; 74 5e                       ; 0xc298f
    cmp bl, 004h                              ; 80 fb 04                    ; 0xc2991
    je short 029a3h                           ; 74 0d                       ; 0xc2994
    jmp short 02a0eh                          ; eb 76                       ; 0xc2996
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc2998
    je short 029cdh                           ; 74 30                       ; 0xc299b
    jmp short 02a0eh                          ; eb 6f                       ; 0xc299d
    or byte [bp-00ch], 001h                   ; 80 4e f4 01                 ; 0xc299f vgabios.c:1759
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc29a3 vgabios.c:1761
    xor ah, ah                                ; 30 e4                       ; 0xc29a6
    push ax                                   ; 50                          ; 0xc29a8
    mov al, dl                                ; 88 d0                       ; 0xc29a9
    push ax                                   ; 50                          ; 0xc29ab
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc29ac
    push ax                                   ; 50                          ; 0xc29af
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc29b0
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc29b3
    xor bh, bh                                ; 30 ff                       ; 0xc29b6
    mov dl, byte [bp-00ch]                    ; 8a 56 f4                    ; 0xc29b8
    xor dh, dh                                ; 30 f6                       ; 0xc29bb
    mov byte [bp-010h], ch                    ; 88 6e f0                    ; 0xc29bd
    mov byte [bp-00fh], ah                    ; 88 66 f1                    ; 0xc29c0
    mov cx, ax                                ; 89 c1                       ; 0xc29c3
    mov ax, word [bp-010h]                    ; 8b 46 f0                    ; 0xc29c5
    call 0213ah                               ; e8 6f f7                    ; 0xc29c8
    jmp short 02a0eh                          ; eb 41                       ; 0xc29cb vgabios.c:1762
    push ax                                   ; 50                          ; 0xc29cd vgabios.c:1764
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc29ce
    push ax                                   ; 50                          ; 0xc29d1
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc29d2
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc29d5
    xor bh, bh                                ; 30 ff                       ; 0xc29d8
    mov dl, byte [bp-00ch]                    ; 8a 56 f4                    ; 0xc29da
    xor dh, dh                                ; 30 f6                       ; 0xc29dd
    mov byte [bp-010h], ch                    ; 88 6e f0                    ; 0xc29df
    mov byte [bp-00fh], ah                    ; 88 66 f1                    ; 0xc29e2
    mov cx, ax                                ; 89 c1                       ; 0xc29e5
    mov ax, word [bp-010h]                    ; 8b 46 f0                    ; 0xc29e7
    call 0224ch                               ; e8 5f f8                    ; 0xc29ea
    jmp short 02a0eh                          ; eb 1f                       ; 0xc29ed vgabios.c:1765
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc29ef vgabios.c:1767
    push ax                                   ; 50                          ; 0xc29f2
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc29f3
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc29f6
    mov byte [bp-00fh], ah                    ; 88 66 f1                    ; 0xc29f9
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc29fc
    xor bh, bh                                ; 30 ff                       ; 0xc29ff
    mov dl, byte [bp-00ch]                    ; 8a 56 f4                    ; 0xc2a01
    xor dh, dh                                ; 30 f6                       ; 0xc2a04
    mov al, ch                                ; 88 e8                       ; 0xc2a06
    mov cx, word [bp-010h]                    ; 8b 4e f0                    ; 0xc2a08
    call 0235eh                               ; e8 50 f9                    ; 0xc2a0b
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc2a0e vgabios.c:1775
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc2a11 vgabios.c:1777
    xor ah, ah                                ; 30 e4                       ; 0xc2a14
    cmp ax, word [bp-012h]                    ; 3b 46 ee                    ; 0xc2a16
    jne short 02a21h                          ; 75 06                       ; 0xc2a19
    mov byte [bp-006h], ah                    ; 88 66 fa                    ; 0xc2a1b vgabios.c:1778
    inc byte [bp-008h]                        ; fe 46 f8                    ; 0xc2a1e vgabios.c:1779
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2a21 vgabios.c:1784
    xor ah, ah                                ; 30 e4                       ; 0xc2a24
    cmp ax, word [bp-014h]                    ; 3b 46 ec                    ; 0xc2a26
    jne short 02a8fh                          ; 75 64                       ; 0xc2a29
    mov bl, byte [bp-00eh]                    ; 8a 5e f2                    ; 0xc2a2b vgabios.c:1786
    xor bh, bh                                ; 30 ff                       ; 0xc2a2e
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc2a30
    sal bx, CL                                ; d3 e3                       ; 0xc2a32
    mov cl, byte [bp-014h]                    ; 8a 4e ec                    ; 0xc2a34
    db  0feh, 0c9h
    ; dec cl                                    ; fe c9                     ; 0xc2a37
    mov ch, byte [bp-012h]                    ; 8a 6e ee                    ; 0xc2a39
    db  0feh, 0cdh
    ; dec ch                                    ; fe cd                     ; 0xc2a3c
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc2a3e
    jne short 02a91h                          ; 75 4c                       ; 0xc2a43
    mov ax, word [bp-012h]                    ; 8b 46 ee                    ; 0xc2a45 vgabios.c:1788
    mul word [bp-014h]                        ; f7 66 ec                    ; 0xc2a48
    sal ax, 1                                 ; d1 e0                       ; 0xc2a4b
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc2a4d
    mov dl, byte [bp-00ah]                    ; 8a 56 f6                    ; 0xc2a4f
    xor dh, dh                                ; 30 f6                       ; 0xc2a52
    inc ax                                    ; 40                          ; 0xc2a54
    mul dx                                    ; f7 e2                       ; 0xc2a55
    mov si, ax                                ; 89 c6                       ; 0xc2a57
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2a59
    xor ah, ah                                ; 30 e4                       ; 0xc2a5c
    dec ax                                    ; 48                          ; 0xc2a5e
    mul word [bp-012h]                        ; f7 66 ee                    ; 0xc2a5f
    mov dx, ax                                ; 89 c2                       ; 0xc2a62
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc2a64
    xor ah, ah                                ; 30 e4                       ; 0xc2a67
    add ax, dx                                ; 01 d0                       ; 0xc2a69
    sal ax, 1                                 ; d1 e0                       ; 0xc2a6b
    add si, ax                                ; 01 c6                       ; 0xc2a6d
    inc si                                    ; 46                          ; 0xc2a6f vgabios.c:1789
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc2a70 vgabios.c:35
    mov dl, byte [es:si]                      ; 26 8a 14                    ; 0xc2a74 vgabios.c:37
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc2a77 vgabios.c:1790
    push ax                                   ; 50                          ; 0xc2a7a
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2a7b
    xor ah, ah                                ; 30 e4                       ; 0xc2a7e
    push ax                                   ; 50                          ; 0xc2a80
    mov al, ch                                ; 88 e8                       ; 0xc2a81
    push ax                                   ; 50                          ; 0xc2a83
    mov al, cl                                ; 88 c8                       ; 0xc2a84
    push ax                                   ; 50                          ; 0xc2a86
    xor dh, dh                                ; 30 f6                       ; 0xc2a87
    xor cx, cx                                ; 31 c9                       ; 0xc2a89
    xor bx, bx                                ; 31 db                       ; 0xc2a8b
    jmp short 02aa7h                          ; eb 18                       ; 0xc2a8d vgabios.c:1792
    jmp short 02ab0h                          ; eb 1f                       ; 0xc2a8f
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc2a91 vgabios.c:1794
    push ax                                   ; 50                          ; 0xc2a94
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2a95
    xor ah, ah                                ; 30 e4                       ; 0xc2a98
    push ax                                   ; 50                          ; 0xc2a9a
    mov al, ch                                ; 88 e8                       ; 0xc2a9b
    push ax                                   ; 50                          ; 0xc2a9d
    mov al, cl                                ; 88 c8                       ; 0xc2a9e
    push ax                                   ; 50                          ; 0xc2aa0
    xor cx, cx                                ; 31 c9                       ; 0xc2aa1
    xor bx, bx                                ; 31 db                       ; 0xc2aa3
    xor dx, dx                                ; 31 d2                       ; 0xc2aa5
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc2aa7
    call 01a8fh                               ; e8 e2 ef                    ; 0xc2aaa
    dec byte [bp-008h]                        ; fe 4e f8                    ; 0xc2aad vgabios.c:1796
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2ab0 vgabios.c:1800
    xor ah, ah                                ; 30 e4                       ; 0xc2ab3
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc2ab5
    mov CL, strict byte 008h                  ; b1 08                       ; 0xc2ab8
    sal word [bp-016h], CL                    ; d3 66 ea                    ; 0xc2aba
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc2abd
    add word [bp-016h], ax                    ; 01 46 ea                    ; 0xc2ac0
    mov dx, word [bp-016h]                    ; 8b 56 ea                    ; 0xc2ac3 vgabios.c:1801
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2ac6
    call 0124ch                               ; e8 80 e7                    ; 0xc2ac9
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc2acc vgabios.c:1802
    pop si                                    ; 5e                          ; 0xc2acf
    pop bp                                    ; 5d                          ; 0xc2ad0
    retn                                      ; c3                          ; 0xc2ad1
  ; disGetNextSymbol 0xc2ad2 LB 0x17d7 -> off=0x0 cb=000000000000002c uValue=00000000000c2ad2 'get_font_access'
get_font_access:                             ; 0xc2ad2 LB 0x2c
    push bp                                   ; 55                          ; 0xc2ad2 vgabios.c:1805
    mov bp, sp                                ; 89 e5                       ; 0xc2ad3
    push dx                                   ; 52                          ; 0xc2ad5
    mov ax, 00100h                            ; b8 00 01                    ; 0xc2ad6 vgabios.c:1807
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc2ad9
    out DX, ax                                ; ef                          ; 0xc2adc
    mov ax, 00402h                            ; b8 02 04                    ; 0xc2add vgabios.c:1808
    out DX, ax                                ; ef                          ; 0xc2ae0
    mov ax, 00704h                            ; b8 04 07                    ; 0xc2ae1 vgabios.c:1809
    out DX, ax                                ; ef                          ; 0xc2ae4
    mov ax, 00300h                            ; b8 00 03                    ; 0xc2ae5 vgabios.c:1810
    out DX, ax                                ; ef                          ; 0xc2ae8
    mov ax, 00204h                            ; b8 04 02                    ; 0xc2ae9 vgabios.c:1811
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2aec
    out DX, ax                                ; ef                          ; 0xc2aef
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc2af0 vgabios.c:1812
    out DX, ax                                ; ef                          ; 0xc2af3
    mov ax, 00406h                            ; b8 06 04                    ; 0xc2af4 vgabios.c:1813
    out DX, ax                                ; ef                          ; 0xc2af7
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc2af8 vgabios.c:1814
    pop dx                                    ; 5a                          ; 0xc2afb
    pop bp                                    ; 5d                          ; 0xc2afc
    retn                                      ; c3                          ; 0xc2afd
  ; disGetNextSymbol 0xc2afe LB 0x17ab -> off=0x0 cb=000000000000003f uValue=00000000000c2afe 'release_font_access'
release_font_access:                         ; 0xc2afe LB 0x3f
    push bp                                   ; 55                          ; 0xc2afe vgabios.c:1816
    mov bp, sp                                ; 89 e5                       ; 0xc2aff
    push dx                                   ; 52                          ; 0xc2b01
    mov ax, 00100h                            ; b8 00 01                    ; 0xc2b02 vgabios.c:1818
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc2b05
    out DX, ax                                ; ef                          ; 0xc2b08
    mov ax, 00302h                            ; b8 02 03                    ; 0xc2b09 vgabios.c:1819
    out DX, ax                                ; ef                          ; 0xc2b0c
    mov ax, 00304h                            ; b8 04 03                    ; 0xc2b0d vgabios.c:1820
    out DX, ax                                ; ef                          ; 0xc2b10
    mov ax, 00300h                            ; b8 00 03                    ; 0xc2b11 vgabios.c:1821
    out DX, ax                                ; ef                          ; 0xc2b14
    mov dx, 003cch                            ; ba cc 03                    ; 0xc2b15 vgabios.c:1822
    in AL, DX                                 ; ec                          ; 0xc2b18
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2b19
    and ax, strict word 00001h                ; 25 01 00                    ; 0xc2b1b
    sal ax, 1                                 ; d1 e0                       ; 0xc2b1e
    sal ax, 1                                 ; d1 e0                       ; 0xc2b20
    mov ah, al                                ; 88 c4                       ; 0xc2b22
    or ah, 00ah                               ; 80 cc 0a                    ; 0xc2b24
    xor al, al                                ; 30 c0                       ; 0xc2b27
    or AL, strict byte 006h                   ; 0c 06                       ; 0xc2b29
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2b2b
    out DX, ax                                ; ef                          ; 0xc2b2e
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc2b2f vgabios.c:1823
    out DX, ax                                ; ef                          ; 0xc2b32
    mov ax, 01005h                            ; b8 05 10                    ; 0xc2b33 vgabios.c:1824
    out DX, ax                                ; ef                          ; 0xc2b36
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc2b37 vgabios.c:1825
    pop dx                                    ; 5a                          ; 0xc2b3a
    pop bp                                    ; 5d                          ; 0xc2b3b
    retn                                      ; c3                          ; 0xc2b3c
  ; disGetNextSymbol 0xc2b3d LB 0x176c -> off=0x0 cb=00000000000000b3 uValue=00000000000c2b3d 'set_scan_lines'
set_scan_lines:                              ; 0xc2b3d LB 0xb3
    push bp                                   ; 55                          ; 0xc2b3d vgabios.c:1827
    mov bp, sp                                ; 89 e5                       ; 0xc2b3e
    push bx                                   ; 53                          ; 0xc2b40
    push cx                                   ; 51                          ; 0xc2b41
    push dx                                   ; 52                          ; 0xc2b42
    push si                                   ; 56                          ; 0xc2b43
    push di                                   ; 57                          ; 0xc2b44
    mov bl, al                                ; 88 c3                       ; 0xc2b45
    mov si, strict word 00063h                ; be 63 00                    ; 0xc2b47 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2b4a
    mov es, ax                                ; 8e c0                       ; 0xc2b4d
    mov si, word [es:si]                      ; 26 8b 34                    ; 0xc2b4f
    mov cx, si                                ; 89 f1                       ; 0xc2b52 vgabios.c:48
    mov AL, strict byte 009h                  ; b0 09                       ; 0xc2b54 vgabios.c:1833
    mov dx, si                                ; 89 f2                       ; 0xc2b56
    out DX, AL                                ; ee                          ; 0xc2b58
    lea dx, [si+001h]                         ; 8d 54 01                    ; 0xc2b59 vgabios.c:1834
    in AL, DX                                 ; ec                          ; 0xc2b5c
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2b5d
    mov ah, al                                ; 88 c4                       ; 0xc2b5f vgabios.c:1835
    and ah, 0e0h                              ; 80 e4 e0                    ; 0xc2b61
    mov al, bl                                ; 88 d8                       ; 0xc2b64
    db  0feh, 0c8h
    ; dec al                                    ; fe c8                     ; 0xc2b66
    or al, ah                                 ; 08 e0                       ; 0xc2b68
    out DX, AL                                ; ee                          ; 0xc2b6a vgabios.c:1836
    cmp bl, 008h                              ; 80 fb 08                    ; 0xc2b6b vgabios.c:1837
    jne short 02b78h                          ; 75 08                       ; 0xc2b6e
    mov dx, strict word 00007h                ; ba 07 00                    ; 0xc2b70 vgabios.c:1839
    mov ax, strict word 00006h                ; b8 06 00                    ; 0xc2b73
    jmp short 02b85h                          ; eb 0d                       ; 0xc2b76 vgabios.c:1841
    mov dl, bl                                ; 88 da                       ; 0xc2b78 vgabios.c:1843
    sub dl, 003h                              ; 80 ea 03                    ; 0xc2b7a
    xor dh, dh                                ; 30 f6                       ; 0xc2b7d
    mov al, bl                                ; 88 d8                       ; 0xc2b7f
    sub AL, strict byte 004h                  ; 2c 04                       ; 0xc2b81
    xor ah, ah                                ; 30 e4                       ; 0xc2b83
    call 01150h                               ; e8 c8 e5                    ; 0xc2b85
    xor bh, bh                                ; 30 ff                       ; 0xc2b88 vgabios.c:1845
    mov si, 00085h                            ; be 85 00                    ; 0xc2b8a vgabios.c:52
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2b8d
    mov es, ax                                ; 8e c0                       ; 0xc2b90
    mov word [es:si], bx                      ; 26 89 1c                    ; 0xc2b92
    mov AL, strict byte 012h                  ; b0 12                       ; 0xc2b95 vgabios.c:1846
    mov dx, cx                                ; 89 ca                       ; 0xc2b97
    out DX, AL                                ; ee                          ; 0xc2b99
    mov si, cx                                ; 89 ce                       ; 0xc2b9a vgabios.c:1847
    inc si                                    ; 46                          ; 0xc2b9c
    mov dx, si                                ; 89 f2                       ; 0xc2b9d
    in AL, DX                                 ; ec                          ; 0xc2b9f
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2ba0
    mov di, ax                                ; 89 c7                       ; 0xc2ba2
    mov AL, strict byte 007h                  ; b0 07                       ; 0xc2ba4 vgabios.c:1848
    mov dx, cx                                ; 89 ca                       ; 0xc2ba6
    out DX, AL                                ; ee                          ; 0xc2ba8
    mov dx, si                                ; 89 f2                       ; 0xc2ba9 vgabios.c:1849
    in AL, DX                                 ; ec                          ; 0xc2bab
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2bac
    mov dl, al                                ; 88 c2                       ; 0xc2bae vgabios.c:1850
    and dl, 002h                              ; 80 e2 02                    ; 0xc2bb0
    xor dh, dh                                ; 30 f6                       ; 0xc2bb3
    mov CL, strict byte 007h                  ; b1 07                       ; 0xc2bb5
    sal dx, CL                                ; d3 e2                       ; 0xc2bb7
    and AL, strict byte 040h                  ; 24 40                       ; 0xc2bb9
    xor ah, ah                                ; 30 e4                       ; 0xc2bbb
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc2bbd
    sal ax, CL                                ; d3 e0                       ; 0xc2bbf
    add ax, dx                                ; 01 d0                       ; 0xc2bc1
    inc ax                                    ; 40                          ; 0xc2bc3
    add ax, di                                ; 01 f8                       ; 0xc2bc4
    xor dx, dx                                ; 31 d2                       ; 0xc2bc6 vgabios.c:1851
    div bx                                    ; f7 f3                       ; 0xc2bc8
    mov dl, al                                ; 88 c2                       ; 0xc2bca vgabios.c:1852
    db  0feh, 0cah
    ; dec dl                                    ; fe ca                     ; 0xc2bcc
    mov si, 00084h                            ; be 84 00                    ; 0xc2bce vgabios.c:42
    mov byte [es:si], dl                      ; 26 88 14                    ; 0xc2bd1
    mov si, strict word 0004ah                ; be 4a 00                    ; 0xc2bd4 vgabios.c:47
    mov dx, word [es:si]                      ; 26 8b 14                    ; 0xc2bd7
    xor ah, ah                                ; 30 e4                       ; 0xc2bda vgabios.c:1854
    mul dx                                    ; f7 e2                       ; 0xc2bdc
    sal ax, 1                                 ; d1 e0                       ; 0xc2bde
    mov si, strict word 0004ch                ; be 4c 00                    ; 0xc2be0 vgabios.c:52
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc2be3
    lea sp, [bp-00ah]                         ; 8d 66 f6                    ; 0xc2be6 vgabios.c:1855
    pop di                                    ; 5f                          ; 0xc2be9
    pop si                                    ; 5e                          ; 0xc2bea
    pop dx                                    ; 5a                          ; 0xc2beb
    pop cx                                    ; 59                          ; 0xc2bec
    pop bx                                    ; 5b                          ; 0xc2bed
    pop bp                                    ; 5d                          ; 0xc2bee
    retn                                      ; c3                          ; 0xc2bef
  ; disGetNextSymbol 0xc2bf0 LB 0x16b9 -> off=0x0 cb=0000000000000085 uValue=00000000000c2bf0 'biosfn_load_text_user_pat'
biosfn_load_text_user_pat:                   ; 0xc2bf0 LB 0x85
    push bp                                   ; 55                          ; 0xc2bf0 vgabios.c:1857
    mov bp, sp                                ; 89 e5                       ; 0xc2bf1
    push si                                   ; 56                          ; 0xc2bf3
    push di                                   ; 57                          ; 0xc2bf4
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc2bf5
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc2bf8
    mov word [bp-00eh], dx                    ; 89 56 f2                    ; 0xc2bfb
    mov word [bp-00ah], bx                    ; 89 5e f6                    ; 0xc2bfe
    mov word [bp-00ch], cx                    ; 89 4e f4                    ; 0xc2c01
    call 02ad2h                               ; e8 cb fe                    ; 0xc2c04 vgabios.c:1862
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc2c07 vgabios.c:1863
    and AL, strict byte 003h                  ; 24 03                       ; 0xc2c0a
    xor ah, ah                                ; 30 e4                       ; 0xc2c0c
    mov CL, strict byte 00eh                  ; b1 0e                       ; 0xc2c0e
    mov bx, ax                                ; 89 c3                       ; 0xc2c10
    sal bx, CL                                ; d3 e3                       ; 0xc2c12
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc2c14
    and AL, strict byte 004h                  ; 24 04                       ; 0xc2c17
    mov CL, strict byte 00bh                  ; b1 0b                       ; 0xc2c19
    sal ax, CL                                ; d3 e0                       ; 0xc2c1b
    add bx, ax                                ; 01 c3                       ; 0xc2c1d
    mov word [bp-008h], bx                    ; 89 5e f8                    ; 0xc2c1f
    xor bx, bx                                ; 31 db                       ; 0xc2c22 vgabios.c:1864
    cmp bx, word [bp-00ch]                    ; 3b 5e f4                    ; 0xc2c24
    jnc short 02c5bh                          ; 73 32                       ; 0xc2c27
    mov al, byte [bp+008h]                    ; 8a 46 08                    ; 0xc2c29 vgabios.c:1866
    xor ah, ah                                ; 30 e4                       ; 0xc2c2c
    mov si, ax                                ; 89 c6                       ; 0xc2c2e
    mov ax, bx                                ; 89 d8                       ; 0xc2c30
    mul si                                    ; f7 e6                       ; 0xc2c32
    add ax, word [bp-00ah]                    ; 03 46 f6                    ; 0xc2c34
    mov di, word [bp+004h]                    ; 8b 7e 04                    ; 0xc2c37 vgabios.c:1867
    add di, bx                                ; 01 df                       ; 0xc2c3a
    mov CL, strict byte 005h                  ; b1 05                       ; 0xc2c3c
    sal di, CL                                ; d3 e7                       ; 0xc2c3e
    add di, word [bp-008h]                    ; 03 7e f8                    ; 0xc2c40
    mov cx, si                                ; 89 f1                       ; 0xc2c43 vgabios.c:1868
    mov si, ax                                ; 89 c6                       ; 0xc2c45
    mov dx, word [bp-00eh]                    ; 8b 56 f2                    ; 0xc2c47
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2c4a
    mov es, ax                                ; 8e c0                       ; 0xc2c4d
    cld                                       ; fc                          ; 0xc2c4f
    jcxz 02c58h                               ; e3 06                       ; 0xc2c50
    push DS                                   ; 1e                          ; 0xc2c52
    mov ds, dx                                ; 8e da                       ; 0xc2c53
    rep movsb                                 ; f3 a4                       ; 0xc2c55
    pop DS                                    ; 1f                          ; 0xc2c57
    inc bx                                    ; 43                          ; 0xc2c58 vgabios.c:1869
    jmp short 02c24h                          ; eb c9                       ; 0xc2c59
    call 02afeh                               ; e8 a0 fe                    ; 0xc2c5b vgabios.c:1870
    cmp byte [bp-006h], 010h                  ; 80 7e fa 10                 ; 0xc2c5e vgabios.c:1871
    jc short 02c6ch                           ; 72 08                       ; 0xc2c62
    mov al, byte [bp+008h]                    ; 8a 46 08                    ; 0xc2c64 vgabios.c:1873
    xor ah, ah                                ; 30 e4                       ; 0xc2c67
    call 02b3dh                               ; e8 d1 fe                    ; 0xc2c69
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc2c6c vgabios.c:1875
    pop di                                    ; 5f                          ; 0xc2c6f
    pop si                                    ; 5e                          ; 0xc2c70
    pop bp                                    ; 5d                          ; 0xc2c71
    retn 00006h                               ; c2 06 00                    ; 0xc2c72
  ; disGetNextSymbol 0xc2c75 LB 0x1634 -> off=0x0 cb=0000000000000076 uValue=00000000000c2c75 'biosfn_load_text_8_14_pat'
biosfn_load_text_8_14_pat:                   ; 0xc2c75 LB 0x76
    push bp                                   ; 55                          ; 0xc2c75 vgabios.c:1877
    mov bp, sp                                ; 89 e5                       ; 0xc2c76
    push bx                                   ; 53                          ; 0xc2c78
    push cx                                   ; 51                          ; 0xc2c79
    push si                                   ; 56                          ; 0xc2c7a
    push di                                   ; 57                          ; 0xc2c7b
    push ax                                   ; 50                          ; 0xc2c7c
    push ax                                   ; 50                          ; 0xc2c7d
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc2c7e
    call 02ad2h                               ; e8 4e fe                    ; 0xc2c81 vgabios.c:1881
    mov al, dl                                ; 88 d0                       ; 0xc2c84 vgabios.c:1882
    and AL, strict byte 003h                  ; 24 03                       ; 0xc2c86
    xor ah, ah                                ; 30 e4                       ; 0xc2c88
    mov CL, strict byte 00eh                  ; b1 0e                       ; 0xc2c8a
    mov bx, ax                                ; 89 c3                       ; 0xc2c8c
    sal bx, CL                                ; d3 e3                       ; 0xc2c8e
    mov al, dl                                ; 88 d0                       ; 0xc2c90
    and AL, strict byte 004h                  ; 24 04                       ; 0xc2c92
    mov CL, strict byte 00bh                  ; b1 0b                       ; 0xc2c94
    sal ax, CL                                ; d3 e0                       ; 0xc2c96
    add bx, ax                                ; 01 c3                       ; 0xc2c98
    mov word [bp-00ch], bx                    ; 89 5e f4                    ; 0xc2c9a
    xor bx, bx                                ; 31 db                       ; 0xc2c9d vgabios.c:1883
    jmp short 02ca7h                          ; eb 06                       ; 0xc2c9f
    cmp bx, 00100h                            ; 81 fb 00 01                 ; 0xc2ca1
    jnc short 02cd3h                          ; 73 2c                       ; 0xc2ca5
    mov ax, bx                                ; 89 d8                       ; 0xc2ca7 vgabios.c:1885
    mov si, strict word 0000eh                ; be 0e 00                    ; 0xc2ca9
    mul si                                    ; f7 e6                       ; 0xc2cac
    mov CL, strict byte 005h                  ; b1 05                       ; 0xc2cae vgabios.c:1886
    mov di, bx                                ; 89 df                       ; 0xc2cb0
    sal di, CL                                ; d3 e7                       ; 0xc2cb2
    add di, word [bp-00ch]                    ; 03 7e f4                    ; 0xc2cb4
    mov si, 05d6ch                            ; be 6c 5d                    ; 0xc2cb7 vgabios.c:1887
    add si, ax                                ; 01 c6                       ; 0xc2cba
    mov cx, strict word 0000eh                ; b9 0e 00                    ; 0xc2cbc
    mov dx, 0c000h                            ; ba 00 c0                    ; 0xc2cbf
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2cc2
    mov es, ax                                ; 8e c0                       ; 0xc2cc5
    cld                                       ; fc                          ; 0xc2cc7
    jcxz 02cd0h                               ; e3 06                       ; 0xc2cc8
    push DS                                   ; 1e                          ; 0xc2cca
    mov ds, dx                                ; 8e da                       ; 0xc2ccb
    rep movsb                                 ; f3 a4                       ; 0xc2ccd
    pop DS                                    ; 1f                          ; 0xc2ccf
    inc bx                                    ; 43                          ; 0xc2cd0 vgabios.c:1888
    jmp short 02ca1h                          ; eb ce                       ; 0xc2cd1
    call 02afeh                               ; e8 28 fe                    ; 0xc2cd3 vgabios.c:1889
    cmp byte [bp-00ah], 010h                  ; 80 7e f6 10                 ; 0xc2cd6 vgabios.c:1890
    jc short 02ce2h                           ; 72 06                       ; 0xc2cda
    mov ax, strict word 0000eh                ; b8 0e 00                    ; 0xc2cdc vgabios.c:1892
    call 02b3dh                               ; e8 5b fe                    ; 0xc2cdf
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc2ce2 vgabios.c:1894
    pop di                                    ; 5f                          ; 0xc2ce5
    pop si                                    ; 5e                          ; 0xc2ce6
    pop cx                                    ; 59                          ; 0xc2ce7
    pop bx                                    ; 5b                          ; 0xc2ce8
    pop bp                                    ; 5d                          ; 0xc2ce9
    retn                                      ; c3                          ; 0xc2cea
  ; disGetNextSymbol 0xc2ceb LB 0x15be -> off=0x0 cb=0000000000000074 uValue=00000000000c2ceb 'biosfn_load_text_8_8_pat'
biosfn_load_text_8_8_pat:                    ; 0xc2ceb LB 0x74
    push bp                                   ; 55                          ; 0xc2ceb vgabios.c:1896
    mov bp, sp                                ; 89 e5                       ; 0xc2cec
    push bx                                   ; 53                          ; 0xc2cee
    push cx                                   ; 51                          ; 0xc2cef
    push si                                   ; 56                          ; 0xc2cf0
    push di                                   ; 57                          ; 0xc2cf1
    push ax                                   ; 50                          ; 0xc2cf2
    push ax                                   ; 50                          ; 0xc2cf3
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc2cf4
    call 02ad2h                               ; e8 d8 fd                    ; 0xc2cf7 vgabios.c:1900
    mov al, dl                                ; 88 d0                       ; 0xc2cfa vgabios.c:1901
    and AL, strict byte 003h                  ; 24 03                       ; 0xc2cfc
    xor ah, ah                                ; 30 e4                       ; 0xc2cfe
    mov CL, strict byte 00eh                  ; b1 0e                       ; 0xc2d00
    mov bx, ax                                ; 89 c3                       ; 0xc2d02
    sal bx, CL                                ; d3 e3                       ; 0xc2d04
    mov al, dl                                ; 88 d0                       ; 0xc2d06
    and AL, strict byte 004h                  ; 24 04                       ; 0xc2d08
    mov CL, strict byte 00bh                  ; b1 0b                       ; 0xc2d0a
    sal ax, CL                                ; d3 e0                       ; 0xc2d0c
    add bx, ax                                ; 01 c3                       ; 0xc2d0e
    mov word [bp-00ch], bx                    ; 89 5e f4                    ; 0xc2d10
    xor bx, bx                                ; 31 db                       ; 0xc2d13 vgabios.c:1902
    jmp short 02d1dh                          ; eb 06                       ; 0xc2d15
    cmp bx, 00100h                            ; 81 fb 00 01                 ; 0xc2d17
    jnc short 02d47h                          ; 73 2a                       ; 0xc2d1b
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc2d1d vgabios.c:1904
    mov si, bx                                ; 89 de                       ; 0xc2d1f
    sal si, CL                                ; d3 e6                       ; 0xc2d21
    mov CL, strict byte 005h                  ; b1 05                       ; 0xc2d23 vgabios.c:1905
    mov di, bx                                ; 89 df                       ; 0xc2d25
    sal di, CL                                ; d3 e7                       ; 0xc2d27
    add di, word [bp-00ch]                    ; 03 7e f4                    ; 0xc2d29
    add si, 0556ch                            ; 81 c6 6c 55                 ; 0xc2d2c vgabios.c:1906
    mov cx, strict word 00008h                ; b9 08 00                    ; 0xc2d30
    mov dx, 0c000h                            ; ba 00 c0                    ; 0xc2d33
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2d36
    mov es, ax                                ; 8e c0                       ; 0xc2d39
    cld                                       ; fc                          ; 0xc2d3b
    jcxz 02d44h                               ; e3 06                       ; 0xc2d3c
    push DS                                   ; 1e                          ; 0xc2d3e
    mov ds, dx                                ; 8e da                       ; 0xc2d3f
    rep movsb                                 ; f3 a4                       ; 0xc2d41
    pop DS                                    ; 1f                          ; 0xc2d43
    inc bx                                    ; 43                          ; 0xc2d44 vgabios.c:1907
    jmp short 02d17h                          ; eb d0                       ; 0xc2d45
    call 02afeh                               ; e8 b4 fd                    ; 0xc2d47 vgabios.c:1908
    cmp byte [bp-00ah], 010h                  ; 80 7e f6 10                 ; 0xc2d4a vgabios.c:1909
    jc short 02d56h                           ; 72 06                       ; 0xc2d4e
    mov ax, strict word 00008h                ; b8 08 00                    ; 0xc2d50 vgabios.c:1911
    call 02b3dh                               ; e8 e7 fd                    ; 0xc2d53
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc2d56 vgabios.c:1913
    pop di                                    ; 5f                          ; 0xc2d59
    pop si                                    ; 5e                          ; 0xc2d5a
    pop cx                                    ; 59                          ; 0xc2d5b
    pop bx                                    ; 5b                          ; 0xc2d5c
    pop bp                                    ; 5d                          ; 0xc2d5d
    retn                                      ; c3                          ; 0xc2d5e
  ; disGetNextSymbol 0xc2d5f LB 0x154a -> off=0x0 cb=0000000000000074 uValue=00000000000c2d5f 'biosfn_load_text_8_16_pat'
biosfn_load_text_8_16_pat:                   ; 0xc2d5f LB 0x74
    push bp                                   ; 55                          ; 0xc2d5f vgabios.c:1916
    mov bp, sp                                ; 89 e5                       ; 0xc2d60
    push bx                                   ; 53                          ; 0xc2d62
    push cx                                   ; 51                          ; 0xc2d63
    push si                                   ; 56                          ; 0xc2d64
    push di                                   ; 57                          ; 0xc2d65
    push ax                                   ; 50                          ; 0xc2d66
    push ax                                   ; 50                          ; 0xc2d67
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc2d68
    call 02ad2h                               ; e8 64 fd                    ; 0xc2d6b vgabios.c:1920
    mov al, dl                                ; 88 d0                       ; 0xc2d6e vgabios.c:1921
    and AL, strict byte 003h                  ; 24 03                       ; 0xc2d70
    xor ah, ah                                ; 30 e4                       ; 0xc2d72
    mov CL, strict byte 00eh                  ; b1 0e                       ; 0xc2d74
    mov bx, ax                                ; 89 c3                       ; 0xc2d76
    sal bx, CL                                ; d3 e3                       ; 0xc2d78
    mov al, dl                                ; 88 d0                       ; 0xc2d7a
    and AL, strict byte 004h                  ; 24 04                       ; 0xc2d7c
    mov CL, strict byte 00bh                  ; b1 0b                       ; 0xc2d7e
    sal ax, CL                                ; d3 e0                       ; 0xc2d80
    add bx, ax                                ; 01 c3                       ; 0xc2d82
    mov word [bp-00ch], bx                    ; 89 5e f4                    ; 0xc2d84
    xor bx, bx                                ; 31 db                       ; 0xc2d87 vgabios.c:1922
    jmp short 02d91h                          ; eb 06                       ; 0xc2d89
    cmp bx, 00100h                            ; 81 fb 00 01                 ; 0xc2d8b
    jnc short 02dbbh                          ; 73 2a                       ; 0xc2d8f
    mov CL, strict byte 004h                  ; b1 04                       ; 0xc2d91 vgabios.c:1924
    mov si, bx                                ; 89 de                       ; 0xc2d93
    sal si, CL                                ; d3 e6                       ; 0xc2d95
    mov CL, strict byte 005h                  ; b1 05                       ; 0xc2d97 vgabios.c:1925
    mov di, bx                                ; 89 df                       ; 0xc2d99
    sal di, CL                                ; d3 e7                       ; 0xc2d9b
    add di, word [bp-00ch]                    ; 03 7e f4                    ; 0xc2d9d
    add si, 06b6ch                            ; 81 c6 6c 6b                 ; 0xc2da0 vgabios.c:1926
    mov cx, strict word 00010h                ; b9 10 00                    ; 0xc2da4
    mov dx, 0c000h                            ; ba 00 c0                    ; 0xc2da7
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2daa
    mov es, ax                                ; 8e c0                       ; 0xc2dad
    cld                                       ; fc                          ; 0xc2daf
    jcxz 02db8h                               ; e3 06                       ; 0xc2db0
    push DS                                   ; 1e                          ; 0xc2db2
    mov ds, dx                                ; 8e da                       ; 0xc2db3
    rep movsb                                 ; f3 a4                       ; 0xc2db5
    pop DS                                    ; 1f                          ; 0xc2db7
    inc bx                                    ; 43                          ; 0xc2db8 vgabios.c:1927
    jmp short 02d8bh                          ; eb d0                       ; 0xc2db9
    call 02afeh                               ; e8 40 fd                    ; 0xc2dbb vgabios.c:1928
    cmp byte [bp-00ah], 010h                  ; 80 7e f6 10                 ; 0xc2dbe vgabios.c:1929
    jc short 02dcah                           ; 72 06                       ; 0xc2dc2
    mov ax, strict word 00010h                ; b8 10 00                    ; 0xc2dc4 vgabios.c:1931
    call 02b3dh                               ; e8 73 fd                    ; 0xc2dc7
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc2dca vgabios.c:1933
    pop di                                    ; 5f                          ; 0xc2dcd
    pop si                                    ; 5e                          ; 0xc2dce
    pop cx                                    ; 59                          ; 0xc2dcf
    pop bx                                    ; 5b                          ; 0xc2dd0
    pop bp                                    ; 5d                          ; 0xc2dd1
    retn                                      ; c3                          ; 0xc2dd2
  ; disGetNextSymbol 0xc2dd3 LB 0x14d6 -> off=0x0 cb=0000000000000005 uValue=00000000000c2dd3 'biosfn_load_gfx_8_8_chars'
biosfn_load_gfx_8_8_chars:                   ; 0xc2dd3 LB 0x5
    push bp                                   ; 55                          ; 0xc2dd3 vgabios.c:1935
    mov bp, sp                                ; 89 e5                       ; 0xc2dd4
    pop bp                                    ; 5d                          ; 0xc2dd6 vgabios.c:1940
    retn                                      ; c3                          ; 0xc2dd7
  ; disGetNextSymbol 0xc2dd8 LB 0x14d1 -> off=0x0 cb=0000000000000007 uValue=00000000000c2dd8 'biosfn_load_gfx_user_chars'
biosfn_load_gfx_user_chars:                  ; 0xc2dd8 LB 0x7
    push bp                                   ; 55                          ; 0xc2dd8 vgabios.c:1941
    mov bp, sp                                ; 89 e5                       ; 0xc2dd9
    pop bp                                    ; 5d                          ; 0xc2ddb vgabios.c:1947
    retn 00002h                               ; c2 02 00                    ; 0xc2ddc
  ; disGetNextSymbol 0xc2ddf LB 0x14ca -> off=0x0 cb=0000000000000005 uValue=00000000000c2ddf 'biosfn_load_gfx_8_14_chars'
biosfn_load_gfx_8_14_chars:                  ; 0xc2ddf LB 0x5
    push bp                                   ; 55                          ; 0xc2ddf vgabios.c:1948
    mov bp, sp                                ; 89 e5                       ; 0xc2de0
    pop bp                                    ; 5d                          ; 0xc2de2 vgabios.c:1953
    retn                                      ; c3                          ; 0xc2de3
  ; disGetNextSymbol 0xc2de4 LB 0x14c5 -> off=0x0 cb=0000000000000005 uValue=00000000000c2de4 'biosfn_load_gfx_8_8_dd_chars'
biosfn_load_gfx_8_8_dd_chars:                ; 0xc2de4 LB 0x5
    push bp                                   ; 55                          ; 0xc2de4 vgabios.c:1954
    mov bp, sp                                ; 89 e5                       ; 0xc2de5
    pop bp                                    ; 5d                          ; 0xc2de7 vgabios.c:1959
    retn                                      ; c3                          ; 0xc2de8
  ; disGetNextSymbol 0xc2de9 LB 0x14c0 -> off=0x0 cb=0000000000000005 uValue=00000000000c2de9 'biosfn_load_gfx_8_16_chars'
biosfn_load_gfx_8_16_chars:                  ; 0xc2de9 LB 0x5
    push bp                                   ; 55                          ; 0xc2de9 vgabios.c:1960
    mov bp, sp                                ; 89 e5                       ; 0xc2dea
    pop bp                                    ; 5d                          ; 0xc2dec vgabios.c:1965
    retn                                      ; c3                          ; 0xc2ded
  ; disGetNextSymbol 0xc2dee LB 0x14bb -> off=0x0 cb=0000000000000005 uValue=00000000000c2dee 'biosfn_alternate_prtsc'
biosfn_alternate_prtsc:                      ; 0xc2dee LB 0x5
    push bp                                   ; 55                          ; 0xc2dee vgabios.c:1967
    mov bp, sp                                ; 89 e5                       ; 0xc2def
    pop bp                                    ; 5d                          ; 0xc2df1 vgabios.c:1972
    retn                                      ; c3                          ; 0xc2df2
  ; disGetNextSymbol 0xc2df3 LB 0x14b6 -> off=0x0 cb=0000000000000005 uValue=00000000000c2df3 'biosfn_switch_video_interface'
biosfn_switch_video_interface:               ; 0xc2df3 LB 0x5
    push bp                                   ; 55                          ; 0xc2df3 vgabios.c:1975
    mov bp, sp                                ; 89 e5                       ; 0xc2df4
    pop bp                                    ; 5d                          ; 0xc2df6 vgabios.c:1980
    retn                                      ; c3                          ; 0xc2df7
  ; disGetNextSymbol 0xc2df8 LB 0x14b1 -> off=0x0 cb=0000000000000005 uValue=00000000000c2df8 'biosfn_enable_video_refresh_control'
biosfn_enable_video_refresh_control:         ; 0xc2df8 LB 0x5
    push bp                                   ; 55                          ; 0xc2df8 vgabios.c:1981
    mov bp, sp                                ; 89 e5                       ; 0xc2df9
    pop bp                                    ; 5d                          ; 0xc2dfb vgabios.c:1986
    retn                                      ; c3                          ; 0xc2dfc
  ; disGetNextSymbol 0xc2dfd LB 0x14ac -> off=0x0 cb=000000000000008f uValue=00000000000c2dfd 'biosfn_write_string'
biosfn_write_string:                         ; 0xc2dfd LB 0x8f
    push bp                                   ; 55                          ; 0xc2dfd vgabios.c:1989
    mov bp, sp                                ; 89 e5                       ; 0xc2dfe
    push si                                   ; 56                          ; 0xc2e00
    push di                                   ; 57                          ; 0xc2e01
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc2e02
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc2e05
    mov byte [bp-006h], dl                    ; 88 56 fa                    ; 0xc2e08
    mov byte [bp-00ah], bl                    ; 88 5e f6                    ; 0xc2e0b
    mov si, cx                                ; 89 ce                       ; 0xc2e0e
    mov di, word [bp+00ah]                    ; 8b 7e 0a                    ; 0xc2e10
    mov al, dl                                ; 88 d0                       ; 0xc2e13 vgabios.c:1996
    xor ah, ah                                ; 30 e4                       ; 0xc2e15
    lea bx, [bp-00eh]                         ; 8d 5e f2                    ; 0xc2e17
    lea dx, [bp-00ch]                         ; 8d 56 f4                    ; 0xc2e1a
    call 00a0ch                               ; e8 ec db                    ; 0xc2e1d
    cmp byte [bp+004h], 0ffh                  ; 80 7e 04 ff                 ; 0xc2e20 vgabios.c:1999
    jne short 02e32h                          ; 75 0c                       ; 0xc2e24
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc2e26 vgabios.c:2000
    mov byte [bp+006h], al                    ; 88 46 06                    ; 0xc2e29
    mov ax, word [bp-00eh]                    ; 8b 46 f2                    ; 0xc2e2c vgabios.c:2001
    mov byte [bp+004h], ah                    ; 88 66 04                    ; 0xc2e2f
    mov dh, byte [bp+004h]                    ; 8a 76 04                    ; 0xc2e32 vgabios.c:2004
    mov dl, byte [bp+006h]                    ; 8a 56 06                    ; 0xc2e35
    xor ah, ah                                ; 30 e4                       ; 0xc2e38
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc2e3a vgabios.c:2005
    call 0124ch                               ; e8 0c e4                    ; 0xc2e3d
    dec si                                    ; 4e                          ; 0xc2e40 vgabios.c:2007
    cmp si, strict byte 0ffffh                ; 83 fe ff                    ; 0xc2e41
    je short 02e72h                           ; 74 2c                       ; 0xc2e44
    mov bx, di                                ; 89 fb                       ; 0xc2e46 vgabios.c:2009
    inc di                                    ; 47                          ; 0xc2e48
    mov es, [bp+008h]                         ; 8e 46 08                    ; 0xc2e49 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc2e4c
    test byte [bp-008h], 002h                 ; f6 46 f8 02                 ; 0xc2e4f vgabios.c:2010
    je short 02e5eh                           ; 74 09                       ; 0xc2e53
    mov bx, di                                ; 89 fb                       ; 0xc2e55 vgabios.c:2011
    inc di                                    ; 47                          ; 0xc2e57
    mov ah, byte [es:bx]                      ; 26 8a 27                    ; 0xc2e58 vgabios.c:37
    mov byte [bp-00ah], ah                    ; 88 66 f6                    ; 0xc2e5b vgabios.c:38
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc2e5e vgabios.c:2013
    xor bh, bh                                ; 30 ff                       ; 0xc2e61
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc2e63
    xor dh, dh                                ; 30 f6                       ; 0xc2e66
    xor ah, ah                                ; 30 e4                       ; 0xc2e68
    mov cx, strict word 00003h                ; b9 03 00                    ; 0xc2e6a
    call 0286fh                               ; e8 ff f9                    ; 0xc2e6d
    jmp short 02e40h                          ; eb ce                       ; 0xc2e70 vgabios.c:2014
    test byte [bp-008h], 001h                 ; f6 46 f8 01                 ; 0xc2e72 vgabios.c:2017
    jne short 02e83h                          ; 75 0b                       ; 0xc2e76
    mov dx, word [bp-00eh]                    ; 8b 56 f2                    ; 0xc2e78 vgabios.c:2018
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc2e7b
    xor ah, ah                                ; 30 e4                       ; 0xc2e7e
    call 0124ch                               ; e8 c9 e3                    ; 0xc2e80
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc2e83 vgabios.c:2019
    pop di                                    ; 5f                          ; 0xc2e86
    pop si                                    ; 5e                          ; 0xc2e87
    pop bp                                    ; 5d                          ; 0xc2e88
    retn 00008h                               ; c2 08 00                    ; 0xc2e89
  ; disGetNextSymbol 0xc2e8c LB 0x141d -> off=0x0 cb=00000000000001f5 uValue=00000000000c2e8c 'biosfn_read_state_info'
biosfn_read_state_info:                      ; 0xc2e8c LB 0x1f5
    push bp                                   ; 55                          ; 0xc2e8c vgabios.c:2022
    mov bp, sp                                ; 89 e5                       ; 0xc2e8d
    push cx                                   ; 51                          ; 0xc2e8f
    push si                                   ; 56                          ; 0xc2e90
    push di                                   ; 57                          ; 0xc2e91
    push ax                                   ; 50                          ; 0xc2e92
    push ax                                   ; 50                          ; 0xc2e93
    push dx                                   ; 52                          ; 0xc2e94
    mov si, strict word 00049h                ; be 49 00                    ; 0xc2e95 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2e98
    mov es, ax                                ; 8e c0                       ; 0xc2e9b
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc2e9d
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc2ea0 vgabios.c:38
    mov si, strict word 0004ch                ; be 4c 00                    ; 0xc2ea3 vgabios.c:47
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc2ea6
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc2ea9 vgabios.c:48
    mov ax, ds                                ; 8c d8                       ; 0xc2eac vgabios.c:2033
    mov es, dx                                ; 8e c2                       ; 0xc2eae vgabios.c:62
    mov word [es:bx], 05502h                  ; 26 c7 07 02 55              ; 0xc2eb0
    mov [es:bx+002h], ds                      ; 26 8c 5f 02                 ; 0xc2eb5
    lea di, [bx+004h]                         ; 8d 7f 04                    ; 0xc2eb9 vgabios.c:2038
    mov cx, strict word 0001eh                ; b9 1e 00                    ; 0xc2ebc
    mov si, strict word 00049h                ; be 49 00                    ; 0xc2ebf
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc2ec2
    cld                                       ; fc                          ; 0xc2ec5
    jcxz 02eceh                               ; e3 06                       ; 0xc2ec6
    push DS                                   ; 1e                          ; 0xc2ec8
    mov ds, dx                                ; 8e da                       ; 0xc2ec9
    rep movsb                                 ; f3 a4                       ; 0xc2ecb
    pop DS                                    ; 1f                          ; 0xc2ecd
    mov si, 00084h                            ; be 84 00                    ; 0xc2ece vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2ed1
    mov es, ax                                ; 8e c0                       ; 0xc2ed4
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc2ed6
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc2ed9 vgabios.c:38
    lea si, [bx+022h]                         ; 8d 77 22                    ; 0xc2edb
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2ede vgabios.c:42
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc2ee1
    lea di, [bx+023h]                         ; 8d 7f 23                    ; 0xc2ee4 vgabios.c:2040
    mov cx, strict word 00002h                ; b9 02 00                    ; 0xc2ee7
    mov si, 00085h                            ; be 85 00                    ; 0xc2eea
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc2eed
    cld                                       ; fc                          ; 0xc2ef0
    jcxz 02ef9h                               ; e3 06                       ; 0xc2ef1
    push DS                                   ; 1e                          ; 0xc2ef3
    mov ds, dx                                ; 8e da                       ; 0xc2ef4
    rep movsb                                 ; f3 a4                       ; 0xc2ef6
    pop DS                                    ; 1f                          ; 0xc2ef8
    mov si, 0008ah                            ; be 8a 00                    ; 0xc2ef9 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2efc
    mov es, ax                                ; 8e c0                       ; 0xc2eff
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc2f01
    lea si, [bx+025h]                         ; 8d 77 25                    ; 0xc2f04 vgabios.c:38
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2f07 vgabios.c:42
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc2f0a
    lea si, [bx+026h]                         ; 8d 77 26                    ; 0xc2f0d vgabios.c:2043
    mov byte [es:si], 000h                    ; 26 c6 04 00                 ; 0xc2f10 vgabios.c:42
    lea si, [bx+027h]                         ; 8d 77 27                    ; 0xc2f14 vgabios.c:2044
    mov word [es:si], strict word 00010h      ; 26 c7 04 10 00              ; 0xc2f17 vgabios.c:52
    lea si, [bx+029h]                         ; 8d 77 29                    ; 0xc2f1c vgabios.c:2045
    mov byte [es:si], 008h                    ; 26 c6 04 08                 ; 0xc2f1f vgabios.c:42
    lea si, [bx+02ah]                         ; 8d 77 2a                    ; 0xc2f23 vgabios.c:2046
    mov byte [es:si], 002h                    ; 26 c6 04 02                 ; 0xc2f26 vgabios.c:42
    lea si, [bx+02bh]                         ; 8d 77 2b                    ; 0xc2f2a vgabios.c:2047
    mov byte [es:si], 000h                    ; 26 c6 04 00                 ; 0xc2f2d vgabios.c:42
    lea si, [bx+02ch]                         ; 8d 77 2c                    ; 0xc2f31 vgabios.c:2048
    mov byte [es:si], 000h                    ; 26 c6 04 00                 ; 0xc2f34 vgabios.c:42
    lea si, [bx+02dh]                         ; 8d 77 2d                    ; 0xc2f38 vgabios.c:2049
    mov byte [es:si], 021h                    ; 26 c6 04 21                 ; 0xc2f3b vgabios.c:42
    lea si, [bx+031h]                         ; 8d 77 31                    ; 0xc2f3f vgabios.c:2050
    mov byte [es:si], 003h                    ; 26 c6 04 03                 ; 0xc2f42 vgabios.c:42
    lea si, [bx+032h]                         ; 8d 77 32                    ; 0xc2f46 vgabios.c:2051
    mov byte [es:si], 000h                    ; 26 c6 04 00                 ; 0xc2f49 vgabios.c:42
    mov si, 00089h                            ; be 89 00                    ; 0xc2f4d vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2f50
    mov es, ax                                ; 8e c0                       ; 0xc2f53
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc2f55
    mov dl, al                                ; 88 c2                       ; 0xc2f58 vgabios.c:2056
    and dl, 080h                              ; 80 e2 80                    ; 0xc2f5a
    xor dh, dh                                ; 30 f6                       ; 0xc2f5d
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc2f5f
    sar dx, CL                                ; d3 fa                       ; 0xc2f61
    and AL, strict byte 010h                  ; 24 10                       ; 0xc2f63
    xor ah, ah                                ; 30 e4                       ; 0xc2f65
    mov CL, strict byte 004h                  ; b1 04                       ; 0xc2f67
    sar ax, CL                                ; d3 f8                       ; 0xc2f69
    or ax, dx                                 ; 09 d0                       ; 0xc2f6b
    cmp ax, strict word 00002h                ; 3d 02 00                    ; 0xc2f6d vgabios.c:2057
    je short 02f83h                           ; 74 11                       ; 0xc2f70
    cmp ax, strict word 00001h                ; 3d 01 00                    ; 0xc2f72
    je short 02f7fh                           ; 74 08                       ; 0xc2f75
    test ax, ax                               ; 85 c0                       ; 0xc2f77
    jne short 02f83h                          ; 75 08                       ; 0xc2f79
    mov AL, strict byte 001h                  ; b0 01                       ; 0xc2f7b vgabios.c:2058
    jmp short 02f85h                          ; eb 06                       ; 0xc2f7d
    mov AL, strict byte 002h                  ; b0 02                       ; 0xc2f7f vgabios.c:2059
    jmp short 02f85h                          ; eb 02                       ; 0xc2f81
    xor al, al                                ; 30 c0                       ; 0xc2f83 vgabios.c:2061
    lea si, [bx+02ah]                         ; 8d 77 2a                    ; 0xc2f85 vgabios.c:2063
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2f88 vgabios.c:42
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc2f8b
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2f8e vgabios.c:2066
    cmp AL, strict byte 00eh                  ; 3c 0e                       ; 0xc2f91
    jc short 02fb5h                           ; 72 20                       ; 0xc2f93
    cmp AL, strict byte 012h                  ; 3c 12                       ; 0xc2f95
    jnbe short 02fb5h                         ; 77 1c                       ; 0xc2f97
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc2f99 vgabios.c:2067
    test ax, ax                               ; 85 c0                       ; 0xc2f9c
    je short 02ff7h                           ; 74 57                       ; 0xc2f9e
    mov si, ax                                ; 89 c6                       ; 0xc2fa0 vgabios.c:2068
    shr si, 1                                 ; d1 ee                       ; 0xc2fa2
    shr si, 1                                 ; d1 ee                       ; 0xc2fa4
    mov ax, 04000h                            ; b8 00 40                    ; 0xc2fa6
    xor dx, dx                                ; 31 d2                       ; 0xc2fa9
    div si                                    ; f7 f6                       ; 0xc2fab
    lea si, [bx+029h]                         ; 8d 77 29                    ; 0xc2fad
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc2fb0 vgabios.c:42
    jmp short 02ff7h                          ; eb 42                       ; 0xc2fb3 vgabios.c:2069
    lea si, [bx+029h]                         ; 8d 77 29                    ; 0xc2fb5
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2fb8
    cmp AL, strict byte 013h                  ; 3c 13                       ; 0xc2fbb
    jne short 02fd0h                          ; 75 11                       ; 0xc2fbd
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2fbf vgabios.c:42
    mov byte [es:si], 001h                    ; 26 c6 04 01                 ; 0xc2fc2
    lea si, [bx+027h]                         ; 8d 77 27                    ; 0xc2fc6 vgabios.c:2071
    mov word [es:si], 00100h                  ; 26 c7 04 00 01              ; 0xc2fc9 vgabios.c:52
    jmp short 02ff7h                          ; eb 27                       ; 0xc2fce vgabios.c:2072
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc2fd0
    jc short 02ff7h                           ; 72 23                       ; 0xc2fd2
    cmp AL, strict byte 006h                  ; 3c 06                       ; 0xc2fd4
    jnbe short 02ff7h                         ; 77 1f                       ; 0xc2fd6
    cmp word [bp-00ah], strict byte 00000h    ; 83 7e f6 00                 ; 0xc2fd8 vgabios.c:2074
    je short 02fech                           ; 74 0e                       ; 0xc2fdc
    mov ax, 04000h                            ; b8 00 40                    ; 0xc2fde vgabios.c:2075
    xor dx, dx                                ; 31 d2                       ; 0xc2fe1
    div word [bp-00ah]                        ; f7 76 f6                    ; 0xc2fe3
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2fe6 vgabios.c:42
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc2fe9
    lea si, [bx+027h]                         ; 8d 77 27                    ; 0xc2fec vgabios.c:2076
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2fef vgabios.c:52
    mov word [es:si], strict word 00004h      ; 26 c7 04 04 00              ; 0xc2ff2
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2ff7 vgabios.c:2078
    cmp AL, strict byte 006h                  ; 3c 06                       ; 0xc2ffa
    je short 03002h                           ; 74 04                       ; 0xc2ffc
    cmp AL, strict byte 011h                  ; 3c 11                       ; 0xc2ffe
    jne short 0300dh                          ; 75 0b                       ; 0xc3000
    lea si, [bx+027h]                         ; 8d 77 27                    ; 0xc3002 vgabios.c:2079
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc3005 vgabios.c:52
    mov word [es:si], strict word 00002h      ; 26 c7 04 02 00              ; 0xc3008
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc300d vgabios.c:2081
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc3010
    jc short 03069h                           ; 72 55                       ; 0xc3012
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc3014
    je short 03069h                           ; 74 51                       ; 0xc3016
    lea si, [bx+02dh]                         ; 8d 77 2d                    ; 0xc3018 vgabios.c:2082
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc301b vgabios.c:42
    mov byte [es:si], 001h                    ; 26 c6 04 01                 ; 0xc301e
    mov si, 00084h                            ; be 84 00                    ; 0xc3022 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3025
    mov es, ax                                ; 8e c0                       ; 0xc3028
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc302a
    xor ah, ah                                ; 30 e4                       ; 0xc302d vgabios.c:38
    inc ax                                    ; 40                          ; 0xc302f
    mov si, 00085h                            ; be 85 00                    ; 0xc3030 vgabios.c:37
    mov dl, byte [es:si]                      ; 26 8a 14                    ; 0xc3033
    xor dh, dh                                ; 30 f6                       ; 0xc3036 vgabios.c:38
    imul dx                                   ; f7 ea                       ; 0xc3038
    cmp ax, 0015eh                            ; 3d 5e 01                    ; 0xc303a vgabios.c:2084
    jc short 0304dh                           ; 72 0e                       ; 0xc303d
    jbe short 03056h                          ; 76 15                       ; 0xc303f
    cmp ax, 001e0h                            ; 3d e0 01                    ; 0xc3041
    je short 0305eh                           ; 74 18                       ; 0xc3044
    cmp ax, 00190h                            ; 3d 90 01                    ; 0xc3046
    je short 0305ah                           ; 74 0f                       ; 0xc3049
    jmp short 0305eh                          ; eb 11                       ; 0xc304b
    cmp ax, 000c8h                            ; 3d c8 00                    ; 0xc304d
    jne short 0305eh                          ; 75 0c                       ; 0xc3050
    xor al, al                                ; 30 c0                       ; 0xc3052 vgabios.c:2085
    jmp short 03060h                          ; eb 0a                       ; 0xc3054
    mov AL, strict byte 001h                  ; b0 01                       ; 0xc3056 vgabios.c:2086
    jmp short 03060h                          ; eb 06                       ; 0xc3058
    mov AL, strict byte 002h                  ; b0 02                       ; 0xc305a vgabios.c:2087
    jmp short 03060h                          ; eb 02                       ; 0xc305c
    mov AL, strict byte 003h                  ; b0 03                       ; 0xc305e vgabios.c:2089
    lea si, [bx+02ah]                         ; 8d 77 2a                    ; 0xc3060 vgabios.c:2091
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc3063 vgabios.c:42
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc3066
    lea di, [bx+033h]                         ; 8d 7f 33                    ; 0xc3069 vgabios.c:2094
    mov cx, strict word 0000dh                ; b9 0d 00                    ; 0xc306c
    xor ax, ax                                ; 31 c0                       ; 0xc306f
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc3071
    cld                                       ; fc                          ; 0xc3074
    jcxz 03079h                               ; e3 02                       ; 0xc3075
    rep stosb                                 ; f3 aa                       ; 0xc3077
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc3079 vgabios.c:2095
    pop di                                    ; 5f                          ; 0xc307c
    pop si                                    ; 5e                          ; 0xc307d
    pop cx                                    ; 59                          ; 0xc307e
    pop bp                                    ; 5d                          ; 0xc307f
    retn                                      ; c3                          ; 0xc3080
  ; disGetNextSymbol 0xc3081 LB 0x1228 -> off=0x0 cb=0000000000000023 uValue=00000000000c3081 'biosfn_read_video_state_size2'
biosfn_read_video_state_size2:               ; 0xc3081 LB 0x23
    push dx                                   ; 52                          ; 0xc3081 vgabios.c:2098
    push bp                                   ; 55                          ; 0xc3082
    mov bp, sp                                ; 89 e5                       ; 0xc3083
    mov dx, ax                                ; 89 c2                       ; 0xc3085
    xor ax, ax                                ; 31 c0                       ; 0xc3087 vgabios.c:2102
    test dl, 001h                             ; f6 c2 01                    ; 0xc3089 vgabios.c:2103
    je short 03091h                           ; 74 03                       ; 0xc308c
    mov ax, strict word 00046h                ; b8 46 00                    ; 0xc308e vgabios.c:2104
    test dl, 002h                             ; f6 c2 02                    ; 0xc3091 vgabios.c:2106
    je short 03099h                           ; 74 03                       ; 0xc3094
    add ax, strict word 0002ah                ; 05 2a 00                    ; 0xc3096 vgabios.c:2107
    test dl, 004h                             ; f6 c2 04                    ; 0xc3099 vgabios.c:2109
    je short 030a1h                           ; 74 03                       ; 0xc309c
    add ax, 00304h                            ; 05 04 03                    ; 0xc309e vgabios.c:2110
    pop bp                                    ; 5d                          ; 0xc30a1 vgabios.c:2113
    pop dx                                    ; 5a                          ; 0xc30a2
    retn                                      ; c3                          ; 0xc30a3
  ; disGetNextSymbol 0xc30a4 LB 0x1205 -> off=0x0 cb=000000000000001b uValue=00000000000c30a4 'vga_get_video_state_size'
vga_get_video_state_size:                    ; 0xc30a4 LB 0x1b
    push bp                                   ; 55                          ; 0xc30a4 vgabios.c:2115
    mov bp, sp                                ; 89 e5                       ; 0xc30a5
    push bx                                   ; 53                          ; 0xc30a7
    push cx                                   ; 51                          ; 0xc30a8
    mov bx, dx                                ; 89 d3                       ; 0xc30a9
    call 03081h                               ; e8 d3 ff                    ; 0xc30ab vgabios.c:2118
    add ax, strict word 0003fh                ; 05 3f 00                    ; 0xc30ae
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc30b1
    shr ax, CL                                ; d3 e8                       ; 0xc30b3
    mov word [ss:bx], ax                      ; 36 89 07                    ; 0xc30b5
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc30b8 vgabios.c:2119
    pop cx                                    ; 59                          ; 0xc30bb
    pop bx                                    ; 5b                          ; 0xc30bc
    pop bp                                    ; 5d                          ; 0xc30bd
    retn                                      ; c3                          ; 0xc30be
  ; disGetNextSymbol 0xc30bf LB 0x11ea -> off=0x0 cb=00000000000002d8 uValue=00000000000c30bf 'biosfn_save_video_state'
biosfn_save_video_state:                     ; 0xc30bf LB 0x2d8
    push bp                                   ; 55                          ; 0xc30bf vgabios.c:2121
    mov bp, sp                                ; 89 e5                       ; 0xc30c0
    push cx                                   ; 51                          ; 0xc30c2
    push si                                   ; 56                          ; 0xc30c3
    push di                                   ; 57                          ; 0xc30c4
    push ax                                   ; 50                          ; 0xc30c5
    push ax                                   ; 50                          ; 0xc30c6
    push ax                                   ; 50                          ; 0xc30c7
    mov cx, dx                                ; 89 d1                       ; 0xc30c8
    mov si, strict word 00063h                ; be 63 00                    ; 0xc30ca vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc30cd
    mov es, ax                                ; 8e c0                       ; 0xc30d0
    mov di, word [es:si]                      ; 26 8b 3c                    ; 0xc30d2
    mov si, di                                ; 89 fe                       ; 0xc30d5 vgabios.c:48
    test byte [bp-00ch], 001h                 ; f6 46 f4 01                 ; 0xc30d7 vgabios.c:2126
    je short 03143h                           ; 74 66                       ; 0xc30db
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc30dd vgabios.c:2127
    in AL, DX                                 ; ec                          ; 0xc30e0
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc30e1
    mov es, cx                                ; 8e c1                       ; 0xc30e3 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc30e5
    inc bx                                    ; 43                          ; 0xc30e8 vgabios.c:2127
    mov dx, di                                ; 89 fa                       ; 0xc30e9
    in AL, DX                                 ; ec                          ; 0xc30eb
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc30ec
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc30ee vgabios.c:42
    inc bx                                    ; 43                          ; 0xc30f1 vgabios.c:2128
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc30f2
    in AL, DX                                 ; ec                          ; 0xc30f5
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc30f6
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc30f8 vgabios.c:42
    inc bx                                    ; 43                          ; 0xc30fb vgabios.c:2129
    mov dx, 003dah                            ; ba da 03                    ; 0xc30fc
    in AL, DX                                 ; ec                          ; 0xc30ff
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3100
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc3102 vgabios.c:2131
    in AL, DX                                 ; ec                          ; 0xc3105
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3106
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc3108
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc310b vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc310e
    inc bx                                    ; 43                          ; 0xc3111 vgabios.c:2132
    mov dx, 003cah                            ; ba ca 03                    ; 0xc3112
    in AL, DX                                 ; ec                          ; 0xc3115
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3116
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3118 vgabios.c:42
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc311b vgabios.c:2135
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc311e
    add bx, ax                                ; 01 c3                       ; 0xc3121 vgabios.c:2133
    jmp short 0312bh                          ; eb 06                       ; 0xc3123
    cmp word [bp-008h], strict byte 00004h    ; 83 7e f8 04                 ; 0xc3125
    jnbe short 03146h                         ; 77 1b                       ; 0xc3129
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc312b vgabios.c:2136
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc312e
    out DX, AL                                ; ee                          ; 0xc3131
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc3132 vgabios.c:2137
    in AL, DX                                 ; ec                          ; 0xc3135
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3136
    mov es, cx                                ; 8e c1                       ; 0xc3138 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc313a
    inc bx                                    ; 43                          ; 0xc313d vgabios.c:2137
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc313e vgabios.c:2138
    jmp short 03125h                          ; eb e2                       ; 0xc3141
    jmp near 031f3h                           ; e9 ad 00                    ; 0xc3143
    xor al, al                                ; 30 c0                       ; 0xc3146 vgabios.c:2139
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc3148
    out DX, AL                                ; ee                          ; 0xc314b
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc314c vgabios.c:2140
    in AL, DX                                 ; ec                          ; 0xc314f
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3150
    mov es, cx                                ; 8e c1                       ; 0xc3152 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3154
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc3157 vgabios.c:2142
    inc bx                                    ; 43                          ; 0xc315c vgabios.c:2140
    jmp short 03165h                          ; eb 06                       ; 0xc315d
    cmp word [bp-008h], strict byte 00018h    ; 83 7e f8 18                 ; 0xc315f
    jnbe short 0317ch                         ; 77 17                       ; 0xc3163
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc3165 vgabios.c:2143
    mov dx, si                                ; 89 f2                       ; 0xc3168
    out DX, AL                                ; ee                          ; 0xc316a
    lea dx, [si+001h]                         ; 8d 54 01                    ; 0xc316b vgabios.c:2144
    in AL, DX                                 ; ec                          ; 0xc316e
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc316f
    mov es, cx                                ; 8e c1                       ; 0xc3171 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3173
    inc bx                                    ; 43                          ; 0xc3176 vgabios.c:2144
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc3177 vgabios.c:2145
    jmp short 0315fh                          ; eb e3                       ; 0xc317a
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc317c vgabios.c:2147
    jmp short 03189h                          ; eb 06                       ; 0xc3181
    cmp word [bp-008h], strict byte 00013h    ; 83 7e f8 13                 ; 0xc3183
    jnbe short 031adh                         ; 77 24                       ; 0xc3187
    mov dx, 003dah                            ; ba da 03                    ; 0xc3189 vgabios.c:2148
    in AL, DX                                 ; ec                          ; 0xc318c
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc318d
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc318f vgabios.c:2149
    and ax, strict word 00020h                ; 25 20 00                    ; 0xc3192
    or ax, word [bp-008h]                     ; 0b 46 f8                    ; 0xc3195
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc3198
    out DX, AL                                ; ee                          ; 0xc319b
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc319c vgabios.c:2150
    in AL, DX                                 ; ec                          ; 0xc319f
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc31a0
    mov es, cx                                ; 8e c1                       ; 0xc31a2 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc31a4
    inc bx                                    ; 43                          ; 0xc31a7 vgabios.c:2150
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc31a8 vgabios.c:2151
    jmp short 03183h                          ; eb d6                       ; 0xc31ab
    mov dx, 003dah                            ; ba da 03                    ; 0xc31ad vgabios.c:2152
    in AL, DX                                 ; ec                          ; 0xc31b0
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc31b1
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc31b3 vgabios.c:2154
    jmp short 031c0h                          ; eb 06                       ; 0xc31b8
    cmp word [bp-008h], strict byte 00008h    ; 83 7e f8 08                 ; 0xc31ba
    jnbe short 031d8h                         ; 77 18                       ; 0xc31be
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc31c0 vgabios.c:2155
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc31c3
    out DX, AL                                ; ee                          ; 0xc31c6
    mov dx, 003cfh                            ; ba cf 03                    ; 0xc31c7 vgabios.c:2156
    in AL, DX                                 ; ec                          ; 0xc31ca
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc31cb
    mov es, cx                                ; 8e c1                       ; 0xc31cd vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc31cf
    inc bx                                    ; 43                          ; 0xc31d2 vgabios.c:2156
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc31d3 vgabios.c:2157
    jmp short 031bah                          ; eb e2                       ; 0xc31d6
    mov es, cx                                ; 8e c1                       ; 0xc31d8 vgabios.c:52
    mov word [es:bx], si                      ; 26 89 37                    ; 0xc31da
    inc bx                                    ; 43                          ; 0xc31dd vgabios.c:2159
    inc bx                                    ; 43                          ; 0xc31de
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc31df vgabios.c:42
    inc bx                                    ; 43                          ; 0xc31e3 vgabios.c:2162
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc31e4 vgabios.c:42
    inc bx                                    ; 43                          ; 0xc31e8 vgabios.c:2163
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc31e9 vgabios.c:42
    inc bx                                    ; 43                          ; 0xc31ed vgabios.c:2164
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc31ee vgabios.c:42
    inc bx                                    ; 43                          ; 0xc31f2 vgabios.c:2165
    test byte [bp-00ch], 002h                 ; f6 46 f4 02                 ; 0xc31f3 vgabios.c:2167
    jne short 031fch                          ; 75 03                       ; 0xc31f7
    jmp near 0333bh                           ; e9 3f 01                    ; 0xc31f9
    mov si, strict word 00049h                ; be 49 00                    ; 0xc31fc vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc31ff
    mov es, ax                                ; 8e c0                       ; 0xc3202
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc3204
    mov es, cx                                ; 8e c1                       ; 0xc3207 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3209
    inc bx                                    ; 43                          ; 0xc320c vgabios.c:2168
    mov si, strict word 0004ah                ; be 4a 00                    ; 0xc320d vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3210
    mov es, ax                                ; 8e c0                       ; 0xc3213
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3215
    mov es, cx                                ; 8e c1                       ; 0xc3218 vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc321a
    inc bx                                    ; 43                          ; 0xc321d vgabios.c:2169
    inc bx                                    ; 43                          ; 0xc321e
    mov si, strict word 0004ch                ; be 4c 00                    ; 0xc321f vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3222
    mov es, ax                                ; 8e c0                       ; 0xc3225
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3227
    mov es, cx                                ; 8e c1                       ; 0xc322a vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc322c
    inc bx                                    ; 43                          ; 0xc322f vgabios.c:2170
    inc bx                                    ; 43                          ; 0xc3230
    mov si, strict word 00063h                ; be 63 00                    ; 0xc3231 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3234
    mov es, ax                                ; 8e c0                       ; 0xc3237
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3239
    mov es, cx                                ; 8e c1                       ; 0xc323c vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc323e
    inc bx                                    ; 43                          ; 0xc3241 vgabios.c:2171
    inc bx                                    ; 43                          ; 0xc3242
    mov si, 00084h                            ; be 84 00                    ; 0xc3243 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3246
    mov es, ax                                ; 8e c0                       ; 0xc3249
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc324b
    mov es, cx                                ; 8e c1                       ; 0xc324e vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3250
    inc bx                                    ; 43                          ; 0xc3253 vgabios.c:2172
    mov si, 00085h                            ; be 85 00                    ; 0xc3254 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3257
    mov es, ax                                ; 8e c0                       ; 0xc325a
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc325c
    mov es, cx                                ; 8e c1                       ; 0xc325f vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3261
    inc bx                                    ; 43                          ; 0xc3264 vgabios.c:2173
    inc bx                                    ; 43                          ; 0xc3265
    mov si, 00087h                            ; be 87 00                    ; 0xc3266 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3269
    mov es, ax                                ; 8e c0                       ; 0xc326c
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc326e
    mov es, cx                                ; 8e c1                       ; 0xc3271 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3273
    inc bx                                    ; 43                          ; 0xc3276 vgabios.c:2174
    mov si, 00088h                            ; be 88 00                    ; 0xc3277 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc327a
    mov es, ax                                ; 8e c0                       ; 0xc327d
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc327f
    mov es, cx                                ; 8e c1                       ; 0xc3282 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3284
    inc bx                                    ; 43                          ; 0xc3287 vgabios.c:2175
    mov si, 00089h                            ; be 89 00                    ; 0xc3288 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc328b
    mov es, ax                                ; 8e c0                       ; 0xc328e
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc3290
    mov es, cx                                ; 8e c1                       ; 0xc3293 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3295
    inc bx                                    ; 43                          ; 0xc3298 vgabios.c:2176
    mov si, strict word 00060h                ; be 60 00                    ; 0xc3299 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc329c
    mov es, ax                                ; 8e c0                       ; 0xc329f
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc32a1
    mov es, cx                                ; 8e c1                       ; 0xc32a4 vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc32a6
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc32a9 vgabios.c:2178
    inc bx                                    ; 43                          ; 0xc32ae vgabios.c:2177
    inc bx                                    ; 43                          ; 0xc32af
    jmp short 032b8h                          ; eb 06                       ; 0xc32b0
    cmp word [bp-008h], strict byte 00008h    ; 83 7e f8 08                 ; 0xc32b2
    jnc short 032d4h                          ; 73 1c                       ; 0xc32b6
    mov si, word [bp-008h]                    ; 8b 76 f8                    ; 0xc32b8 vgabios.c:2179
    sal si, 1                                 ; d1 e6                       ; 0xc32bb
    add si, strict byte 00050h                ; 83 c6 50                    ; 0xc32bd
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc32c0 vgabios.c:47
    mov es, ax                                ; 8e c0                       ; 0xc32c3
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc32c5
    mov es, cx                                ; 8e c1                       ; 0xc32c8 vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc32ca
    inc bx                                    ; 43                          ; 0xc32cd vgabios.c:2180
    inc bx                                    ; 43                          ; 0xc32ce
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc32cf vgabios.c:2181
    jmp short 032b2h                          ; eb de                       ; 0xc32d2
    mov si, strict word 0004eh                ; be 4e 00                    ; 0xc32d4 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc32d7
    mov es, ax                                ; 8e c0                       ; 0xc32da
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc32dc
    mov es, cx                                ; 8e c1                       ; 0xc32df vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc32e1
    inc bx                                    ; 43                          ; 0xc32e4 vgabios.c:2182
    inc bx                                    ; 43                          ; 0xc32e5
    mov si, strict word 00062h                ; be 62 00                    ; 0xc32e6 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc32e9
    mov es, ax                                ; 8e c0                       ; 0xc32ec
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc32ee
    mov es, cx                                ; 8e c1                       ; 0xc32f1 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc32f3
    inc bx                                    ; 43                          ; 0xc32f6 vgabios.c:2183
    mov si, strict word 0007ch                ; be 7c 00                    ; 0xc32f7 vgabios.c:47
    xor ax, ax                                ; 31 c0                       ; 0xc32fa
    mov es, ax                                ; 8e c0                       ; 0xc32fc
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc32fe
    mov es, cx                                ; 8e c1                       ; 0xc3301 vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3303
    inc bx                                    ; 43                          ; 0xc3306 vgabios.c:2185
    inc bx                                    ; 43                          ; 0xc3307
    mov si, strict word 0007eh                ; be 7e 00                    ; 0xc3308 vgabios.c:47
    xor ax, ax                                ; 31 c0                       ; 0xc330b
    mov es, ax                                ; 8e c0                       ; 0xc330d
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc330f
    mov es, cx                                ; 8e c1                       ; 0xc3312 vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3314
    inc bx                                    ; 43                          ; 0xc3317 vgabios.c:2186
    inc bx                                    ; 43                          ; 0xc3318
    mov si, 0010ch                            ; be 0c 01                    ; 0xc3319 vgabios.c:47
    xor ax, ax                                ; 31 c0                       ; 0xc331c
    mov es, ax                                ; 8e c0                       ; 0xc331e
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3320
    mov es, cx                                ; 8e c1                       ; 0xc3323 vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3325
    inc bx                                    ; 43                          ; 0xc3328 vgabios.c:2187
    inc bx                                    ; 43                          ; 0xc3329
    mov si, 0010eh                            ; be 0e 01                    ; 0xc332a vgabios.c:47
    xor ax, ax                                ; 31 c0                       ; 0xc332d
    mov es, ax                                ; 8e c0                       ; 0xc332f
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3331
    mov es, cx                                ; 8e c1                       ; 0xc3334 vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3336
    inc bx                                    ; 43                          ; 0xc3339 vgabios.c:2188
    inc bx                                    ; 43                          ; 0xc333a
    test byte [bp-00ch], 004h                 ; f6 46 f4 04                 ; 0xc333b vgabios.c:2190
    je short 0338dh                           ; 74 4c                       ; 0xc333f
    mov dx, 003c7h                            ; ba c7 03                    ; 0xc3341 vgabios.c:2192
    in AL, DX                                 ; ec                          ; 0xc3344
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3345
    mov es, cx                                ; 8e c1                       ; 0xc3347 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3349
    inc bx                                    ; 43                          ; 0xc334c vgabios.c:2192
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc334d
    in AL, DX                                 ; ec                          ; 0xc3350
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3351
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3353 vgabios.c:42
    inc bx                                    ; 43                          ; 0xc3356 vgabios.c:2193
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc3357
    in AL, DX                                 ; ec                          ; 0xc335a
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc335b
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc335d vgabios.c:42
    inc bx                                    ; 43                          ; 0xc3360 vgabios.c:2194
    xor al, al                                ; 30 c0                       ; 0xc3361
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc3363
    out DX, AL                                ; ee                          ; 0xc3366
    xor ah, ah                                ; 30 e4                       ; 0xc3367 vgabios.c:2197
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc3369
    jmp short 03375h                          ; eb 07                       ; 0xc336c
    cmp word [bp-008h], 00300h                ; 81 7e f8 00 03              ; 0xc336e
    jnc short 03386h                          ; 73 11                       ; 0xc3373
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc3375 vgabios.c:2198
    in AL, DX                                 ; ec                          ; 0xc3378
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3379
    mov es, cx                                ; 8e c1                       ; 0xc337b vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc337d
    inc bx                                    ; 43                          ; 0xc3380 vgabios.c:2198
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc3381 vgabios.c:2199
    jmp short 0336eh                          ; eb e8                       ; 0xc3384
    mov es, cx                                ; 8e c1                       ; 0xc3386 vgabios.c:42
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc3388
    inc bx                                    ; 43                          ; 0xc338c vgabios.c:2200
    mov ax, bx                                ; 89 d8                       ; 0xc338d vgabios.c:2203
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc338f
    pop di                                    ; 5f                          ; 0xc3392
    pop si                                    ; 5e                          ; 0xc3393
    pop cx                                    ; 59                          ; 0xc3394
    pop bp                                    ; 5d                          ; 0xc3395
    retn                                      ; c3                          ; 0xc3396
  ; disGetNextSymbol 0xc3397 LB 0xf12 -> off=0x0 cb=00000000000002ba uValue=00000000000c3397 'biosfn_restore_video_state'
biosfn_restore_video_state:                  ; 0xc3397 LB 0x2ba
    push bp                                   ; 55                          ; 0xc3397 vgabios.c:2205
    mov bp, sp                                ; 89 e5                       ; 0xc3398
    push cx                                   ; 51                          ; 0xc339a
    push si                                   ; 56                          ; 0xc339b
    push di                                   ; 57                          ; 0xc339c
    sub sp, strict byte 00008h                ; 83 ec 08                    ; 0xc339d
    push ax                                   ; 50                          ; 0xc33a0
    mov cx, dx                                ; 89 d1                       ; 0xc33a1
    test byte [bp-010h], 001h                 ; f6 46 f0 01                 ; 0xc33a3 vgabios.c:2209
    je short 03400h                           ; 74 57                       ; 0xc33a7
    mov dx, 003dah                            ; ba da 03                    ; 0xc33a9 vgabios.c:2211
    in AL, DX                                 ; ec                          ; 0xc33ac
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc33ad
    lea si, [bx+040h]                         ; 8d 77 40                    ; 0xc33af vgabios.c:2213
    mov es, cx                                ; 8e c1                       ; 0xc33b2 vgabios.c:47
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc33b4
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc33b7 vgabios.c:48
    mov si, bx                                ; 89 de                       ; 0xc33ba vgabios.c:2214
    mov word [bp-008h], strict word 00001h    ; c7 46 f8 01 00              ; 0xc33bc vgabios.c:2217
    add bx, strict byte 00005h                ; 83 c3 05                    ; 0xc33c1 vgabios.c:2215
    jmp short 033cch                          ; eb 06                       ; 0xc33c4
    cmp word [bp-008h], strict byte 00004h    ; 83 7e f8 04                 ; 0xc33c6
    jnbe short 033e2h                         ; 77 16                       ; 0xc33ca
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc33cc vgabios.c:2218
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc33cf
    out DX, AL                                ; ee                          ; 0xc33d2
    mov es, cx                                ; 8e c1                       ; 0xc33d3 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc33d5
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc33d8 vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc33db
    inc bx                                    ; 43                          ; 0xc33dc vgabios.c:2219
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc33dd vgabios.c:2220
    jmp short 033c6h                          ; eb e4                       ; 0xc33e0
    xor al, al                                ; 30 c0                       ; 0xc33e2 vgabios.c:2221
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc33e4
    out DX, AL                                ; ee                          ; 0xc33e7
    mov es, cx                                ; 8e c1                       ; 0xc33e8 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc33ea
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc33ed vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc33f0
    inc bx                                    ; 43                          ; 0xc33f1 vgabios.c:2222
    mov ax, strict word 00011h                ; b8 11 00                    ; 0xc33f2
    mov dx, word [bp-00ch]                    ; 8b 56 f4                    ; 0xc33f5
    out DX, ax                                ; ef                          ; 0xc33f8
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc33f9 vgabios.c:2227
    jmp short 03409h                          ; eb 09                       ; 0xc33fe
    jmp near 034e0h                           ; e9 dd 00                    ; 0xc3400
    cmp word [bp-008h], strict byte 00018h    ; 83 7e f8 18                 ; 0xc3403
    jnbe short 03423h                         ; 77 1a                       ; 0xc3407
    cmp word [bp-008h], strict byte 00011h    ; 83 7e f8 11                 ; 0xc3409 vgabios.c:2228
    je short 0341dh                           ; 74 0e                       ; 0xc340d
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc340f vgabios.c:2229
    mov dx, word [bp-00ch]                    ; 8b 56 f4                    ; 0xc3412
    out DX, AL                                ; ee                          ; 0xc3415
    mov es, cx                                ; 8e c1                       ; 0xc3416 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc3418
    inc dx                                    ; 42                          ; 0xc341b vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc341c
    inc bx                                    ; 43                          ; 0xc341d vgabios.c:2232
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc341e vgabios.c:2233
    jmp short 03403h                          ; eb e0                       ; 0xc3421
    mov dx, 003cch                            ; ba cc 03                    ; 0xc3423 vgabios.c:2235
    in AL, DX                                 ; ec                          ; 0xc3426
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3427
    and AL, strict byte 0feh                  ; 24 fe                       ; 0xc3429
    mov word [bp-00eh], ax                    ; 89 46 f2                    ; 0xc342b
    cmp word [bp-00ch], 003d4h                ; 81 7e f4 d4 03              ; 0xc342e vgabios.c:2236
    jne short 03439h                          ; 75 04                       ; 0xc3433
    or byte [bp-00eh], 001h                   ; 80 4e f2 01                 ; 0xc3435 vgabios.c:2237
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc3439 vgabios.c:2238
    mov dx, 003c2h                            ; ba c2 03                    ; 0xc343c
    out DX, AL                                ; ee                          ; 0xc343f
    mov AL, strict byte 011h                  ; b0 11                       ; 0xc3440 vgabios.c:2241
    mov dx, word [bp-00ch]                    ; 8b 56 f4                    ; 0xc3442
    out DX, AL                                ; ee                          ; 0xc3445
    lea di, [word bx-00007h]                  ; 8d bf f9 ff                 ; 0xc3446 vgabios.c:2242
    mov es, cx                                ; 8e c1                       ; 0xc344a vgabios.c:37
    mov al, byte [es:di]                      ; 26 8a 05                    ; 0xc344c
    inc dx                                    ; 42                          ; 0xc344f vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc3450
    lea di, [si+003h]                         ; 8d 7c 03                    ; 0xc3451 vgabios.c:2245
    mov dl, byte [es:di]                      ; 26 8a 15                    ; 0xc3454 vgabios.c:37
    xor dh, dh                                ; 30 f6                       ; 0xc3457 vgabios.c:38
    mov word [bp-00ah], dx                    ; 89 56 f6                    ; 0xc3459
    mov dx, 003dah                            ; ba da 03                    ; 0xc345c vgabios.c:2246
    in AL, DX                                 ; ec                          ; 0xc345f
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3460
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc3462 vgabios.c:2247
    jmp short 0346fh                          ; eb 06                       ; 0xc3467
    cmp word [bp-008h], strict byte 00013h    ; 83 7e f8 13                 ; 0xc3469
    jnbe short 03488h                         ; 77 19                       ; 0xc346d
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc346f vgabios.c:2248
    and ax, strict word 00020h                ; 25 20 00                    ; 0xc3472
    or ax, word [bp-008h]                     ; 0b 46 f8                    ; 0xc3475
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc3478
    out DX, AL                                ; ee                          ; 0xc347b
    mov es, cx                                ; 8e c1                       ; 0xc347c vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc347e
    out DX, AL                                ; ee                          ; 0xc3481 vgabios.c:38
    inc bx                                    ; 43                          ; 0xc3482 vgabios.c:2249
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc3483 vgabios.c:2250
    jmp short 03469h                          ; eb e1                       ; 0xc3486
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc3488 vgabios.c:2251
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc348b
    out DX, AL                                ; ee                          ; 0xc348e
    mov dx, 003dah                            ; ba da 03                    ; 0xc348f vgabios.c:2252
    in AL, DX                                 ; ec                          ; 0xc3492
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3493
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc3495 vgabios.c:2254
    jmp short 034a2h                          ; eb 06                       ; 0xc349a
    cmp word [bp-008h], strict byte 00008h    ; 83 7e f8 08                 ; 0xc349c
    jnbe short 034b8h                         ; 77 16                       ; 0xc34a0
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc34a2 vgabios.c:2255
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc34a5
    out DX, AL                                ; ee                          ; 0xc34a8
    mov es, cx                                ; 8e c1                       ; 0xc34a9 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc34ab
    mov dx, 003cfh                            ; ba cf 03                    ; 0xc34ae vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc34b1
    inc bx                                    ; 43                          ; 0xc34b2 vgabios.c:2256
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc34b3 vgabios.c:2257
    jmp short 0349ch                          ; eb e4                       ; 0xc34b6
    add bx, strict byte 00006h                ; 83 c3 06                    ; 0xc34b8 vgabios.c:2258
    mov es, cx                                ; 8e c1                       ; 0xc34bb vgabios.c:37
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc34bd
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc34c0 vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc34c3
    inc si                                    ; 46                          ; 0xc34c4 vgabios.c:2261
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc34c5 vgabios.c:37
    mov dx, word [bp-00ch]                    ; 8b 56 f4                    ; 0xc34c8 vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc34cb
    inc si                                    ; 46                          ; 0xc34cc vgabios.c:2262
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc34cd vgabios.c:37
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc34d0 vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc34d3
    inc si                                    ; 46                          ; 0xc34d4 vgabios.c:2263
    inc si                                    ; 46                          ; 0xc34d5
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc34d6 vgabios.c:37
    mov dx, word [bp-00ch]                    ; 8b 56 f4                    ; 0xc34d9 vgabios.c:38
    add dx, strict byte 00006h                ; 83 c2 06                    ; 0xc34dc
    out DX, AL                                ; ee                          ; 0xc34df
    test byte [bp-010h], 002h                 ; f6 46 f0 02                 ; 0xc34e0 vgabios.c:2267
    jne short 034e9h                          ; 75 03                       ; 0xc34e4
    jmp near 03604h                           ; e9 1b 01                    ; 0xc34e6
    mov es, cx                                ; 8e c1                       ; 0xc34e9 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc34eb
    mov si, strict word 00049h                ; be 49 00                    ; 0xc34ee vgabios.c:42
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc34f1
    mov es, dx                                ; 8e c2                       ; 0xc34f4
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc34f6
    inc bx                                    ; 43                          ; 0xc34f9 vgabios.c:2268
    mov es, cx                                ; 8e c1                       ; 0xc34fa vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc34fc
    mov si, strict word 0004ah                ; be 4a 00                    ; 0xc34ff vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc3502
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3504
    inc bx                                    ; 43                          ; 0xc3507 vgabios.c:2269
    inc bx                                    ; 43                          ; 0xc3508
    mov es, cx                                ; 8e c1                       ; 0xc3509 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc350b
    mov si, strict word 0004ch                ; be 4c 00                    ; 0xc350e vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc3511
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3513
    inc bx                                    ; 43                          ; 0xc3516 vgabios.c:2270
    inc bx                                    ; 43                          ; 0xc3517
    mov es, cx                                ; 8e c1                       ; 0xc3518 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc351a
    mov si, strict word 00063h                ; be 63 00                    ; 0xc351d vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc3520
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3522
    inc bx                                    ; 43                          ; 0xc3525 vgabios.c:2271
    inc bx                                    ; 43                          ; 0xc3526
    mov es, cx                                ; 8e c1                       ; 0xc3527 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc3529
    mov si, 00084h                            ; be 84 00                    ; 0xc352c vgabios.c:42
    mov es, dx                                ; 8e c2                       ; 0xc352f
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc3531
    inc bx                                    ; 43                          ; 0xc3534 vgabios.c:2272
    mov es, cx                                ; 8e c1                       ; 0xc3535 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3537
    mov si, 00085h                            ; be 85 00                    ; 0xc353a vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc353d
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc353f
    inc bx                                    ; 43                          ; 0xc3542 vgabios.c:2273
    inc bx                                    ; 43                          ; 0xc3543
    mov es, cx                                ; 8e c1                       ; 0xc3544 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc3546
    mov si, 00087h                            ; be 87 00                    ; 0xc3549 vgabios.c:42
    mov es, dx                                ; 8e c2                       ; 0xc354c
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc354e
    inc bx                                    ; 43                          ; 0xc3551 vgabios.c:2274
    mov es, cx                                ; 8e c1                       ; 0xc3552 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc3554
    mov si, 00088h                            ; be 88 00                    ; 0xc3557 vgabios.c:42
    mov es, dx                                ; 8e c2                       ; 0xc355a
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc355c
    inc bx                                    ; 43                          ; 0xc355f vgabios.c:2275
    mov es, cx                                ; 8e c1                       ; 0xc3560 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc3562
    mov si, 00089h                            ; be 89 00                    ; 0xc3565 vgabios.c:42
    mov es, dx                                ; 8e c2                       ; 0xc3568
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc356a
    inc bx                                    ; 43                          ; 0xc356d vgabios.c:2276
    mov es, cx                                ; 8e c1                       ; 0xc356e vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3570
    mov si, strict word 00060h                ; be 60 00                    ; 0xc3573 vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc3576
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3578
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc357b vgabios.c:2278
    inc bx                                    ; 43                          ; 0xc3580 vgabios.c:2277
    inc bx                                    ; 43                          ; 0xc3581
    jmp short 0358ah                          ; eb 06                       ; 0xc3582
    cmp word [bp-008h], strict byte 00008h    ; 83 7e f8 08                 ; 0xc3584
    jnc short 035a6h                          ; 73 1c                       ; 0xc3588
    mov es, cx                                ; 8e c1                       ; 0xc358a vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc358c
    mov si, word [bp-008h]                    ; 8b 76 f8                    ; 0xc358f vgabios.c:48
    sal si, 1                                 ; d1 e6                       ; 0xc3592
    add si, strict byte 00050h                ; 83 c6 50                    ; 0xc3594
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc3597 vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc359a
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc359c
    inc bx                                    ; 43                          ; 0xc359f vgabios.c:2280
    inc bx                                    ; 43                          ; 0xc35a0
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc35a1 vgabios.c:2281
    jmp short 03584h                          ; eb de                       ; 0xc35a4
    mov es, cx                                ; 8e c1                       ; 0xc35a6 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc35a8
    mov si, strict word 0004eh                ; be 4e 00                    ; 0xc35ab vgabios.c:52
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc35ae
    mov es, dx                                ; 8e c2                       ; 0xc35b1
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc35b3
    inc bx                                    ; 43                          ; 0xc35b6 vgabios.c:2282
    inc bx                                    ; 43                          ; 0xc35b7
    mov es, cx                                ; 8e c1                       ; 0xc35b8 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc35ba
    mov si, strict word 00062h                ; be 62 00                    ; 0xc35bd vgabios.c:42
    mov es, dx                                ; 8e c2                       ; 0xc35c0
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc35c2
    inc bx                                    ; 43                          ; 0xc35c5 vgabios.c:2283
    mov es, cx                                ; 8e c1                       ; 0xc35c6 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc35c8
    mov si, strict word 0007ch                ; be 7c 00                    ; 0xc35cb vgabios.c:52
    xor dx, dx                                ; 31 d2                       ; 0xc35ce
    mov es, dx                                ; 8e c2                       ; 0xc35d0
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc35d2
    inc bx                                    ; 43                          ; 0xc35d5 vgabios.c:2285
    inc bx                                    ; 43                          ; 0xc35d6
    mov es, cx                                ; 8e c1                       ; 0xc35d7 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc35d9
    mov si, strict word 0007eh                ; be 7e 00                    ; 0xc35dc vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc35df
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc35e1
    inc bx                                    ; 43                          ; 0xc35e4 vgabios.c:2286
    inc bx                                    ; 43                          ; 0xc35e5
    mov es, cx                                ; 8e c1                       ; 0xc35e6 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc35e8
    mov si, 0010ch                            ; be 0c 01                    ; 0xc35eb vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc35ee
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc35f0
    inc bx                                    ; 43                          ; 0xc35f3 vgabios.c:2287
    inc bx                                    ; 43                          ; 0xc35f4
    mov es, cx                                ; 8e c1                       ; 0xc35f5 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc35f7
    mov si, 0010eh                            ; be 0e 01                    ; 0xc35fa vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc35fd
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc35ff
    inc bx                                    ; 43                          ; 0xc3602 vgabios.c:2288
    inc bx                                    ; 43                          ; 0xc3603
    test byte [bp-010h], 004h                 ; f6 46 f0 04                 ; 0xc3604 vgabios.c:2290
    je short 03647h                           ; 74 3d                       ; 0xc3608
    inc bx                                    ; 43                          ; 0xc360a vgabios.c:2291
    mov es, cx                                ; 8e c1                       ; 0xc360b vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc360d
    xor ah, ah                                ; 30 e4                       ; 0xc3610 vgabios.c:38
    mov word [bp-00eh], ax                    ; 89 46 f2                    ; 0xc3612
    inc bx                                    ; 43                          ; 0xc3615 vgabios.c:2292
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc3616 vgabios.c:37
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc3619 vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc361c
    inc bx                                    ; 43                          ; 0xc361d vgabios.c:2293
    xor al, al                                ; 30 c0                       ; 0xc361e
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc3620
    out DX, AL                                ; ee                          ; 0xc3623
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc3624 vgabios.c:2296
    jmp short 03630h                          ; eb 07                       ; 0xc3627
    cmp word [bp-008h], 00300h                ; 81 7e f8 00 03              ; 0xc3629
    jnc short 0363fh                          ; 73 0f                       ; 0xc362e
    mov es, cx                                ; 8e c1                       ; 0xc3630 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc3632
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc3635 vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc3638
    inc bx                                    ; 43                          ; 0xc3639 vgabios.c:2297
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc363a vgabios.c:2298
    jmp short 03629h                          ; eb ea                       ; 0xc363d
    inc bx                                    ; 43                          ; 0xc363f vgabios.c:2299
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc3640
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc3643
    out DX, AL                                ; ee                          ; 0xc3646
    mov ax, bx                                ; 89 d8                       ; 0xc3647 vgabios.c:2303
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc3649
    pop di                                    ; 5f                          ; 0xc364c
    pop si                                    ; 5e                          ; 0xc364d
    pop cx                                    ; 59                          ; 0xc364e
    pop bp                                    ; 5d                          ; 0xc364f
    retn                                      ; c3                          ; 0xc3650
  ; disGetNextSymbol 0xc3651 LB 0xc58 -> off=0x0 cb=000000000000002b uValue=00000000000c3651 'find_vga_entry'
find_vga_entry:                              ; 0xc3651 LB 0x2b
    push bx                                   ; 53                          ; 0xc3651 vgabios.c:2312
    push cx                                   ; 51                          ; 0xc3652
    push dx                                   ; 52                          ; 0xc3653
    push bp                                   ; 55                          ; 0xc3654
    mov bp, sp                                ; 89 e5                       ; 0xc3655
    mov dl, al                                ; 88 c2                       ; 0xc3657
    mov AH, strict byte 0ffh                  ; b4 ff                       ; 0xc3659 vgabios.c:2314
    xor al, al                                ; 30 c0                       ; 0xc365b vgabios.c:2315
    jmp short 03665h                          ; eb 06                       ; 0xc365d
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc365f vgabios.c:2316
    cmp AL, strict byte 00fh                  ; 3c 0f                       ; 0xc3661
    jnbe short 03675h                         ; 77 10                       ; 0xc3663
    mov bl, al                                ; 88 c3                       ; 0xc3665
    xor bh, bh                                ; 30 ff                       ; 0xc3667
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc3669
    sal bx, CL                                ; d3 e3                       ; 0xc366b
    cmp dl, byte [bx+047aeh]                  ; 3a 97 ae 47                 ; 0xc366d
    jne short 0365fh                          ; 75 ec                       ; 0xc3671
    mov ah, al                                ; 88 c4                       ; 0xc3673
    mov al, ah                                ; 88 e0                       ; 0xc3675 vgabios.c:2321
    pop bp                                    ; 5d                          ; 0xc3677
    pop dx                                    ; 5a                          ; 0xc3678
    pop cx                                    ; 59                          ; 0xc3679
    pop bx                                    ; 5b                          ; 0xc367a
    retn                                      ; c3                          ; 0xc367b
  ; disGetNextSymbol 0xc367c LB 0xc2d -> off=0x0 cb=000000000000000e uValue=00000000000c367c 'xread_byte'
xread_byte:                                  ; 0xc367c LB 0xe
    push bx                                   ; 53                          ; 0xc367c vgabios.c:2333
    push bp                                   ; 55                          ; 0xc367d
    mov bp, sp                                ; 89 e5                       ; 0xc367e
    mov bx, dx                                ; 89 d3                       ; 0xc3680
    mov es, ax                                ; 8e c0                       ; 0xc3682 vgabios.c:2335
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc3684
    pop bp                                    ; 5d                          ; 0xc3687 vgabios.c:2336
    pop bx                                    ; 5b                          ; 0xc3688
    retn                                      ; c3                          ; 0xc3689
  ; disGetNextSymbol 0xc368a LB 0xc1f -> off=0x87 cb=00000000000003eb uValue=00000000000c3711 'int10_func'
    db  056h, 04fh, 01ch, 01bh, 013h, 012h, 011h, 010h, 00eh, 00dh, 00ch, 00ah, 009h, 008h, 007h, 006h
    db  005h, 004h, 003h, 002h, 001h, 000h, 0f5h, 03ah, 03bh, 037h, 078h, 037h, 085h, 037h, 093h, 037h
    db  0a3h, 037h, 0b3h, 037h, 0bdh, 037h, 0e6h, 037h, 0feh, 037h, 00bh, 038h, 023h, 038h, 040h, 038h
    db  056h, 038h, 06ah, 038h, 080h, 038h, 08ch, 038h, 052h, 039h, 0c1h, 039h, 0e5h, 039h, 0fah, 039h
    db  03ch, 03ah, 0c7h, 03ah, 030h, 024h, 023h, 022h, 021h, 020h, 014h, 012h, 011h, 010h, 004h, 002h
    db  001h, 000h, 0f5h, 03ah, 0abh, 038h, 0c9h, 038h, 0d8h, 038h, 0e7h, 038h, 0abh, 038h, 0c9h, 038h
    db  0d8h, 038h, 0e7h, 038h, 0f6h, 038h, 002h, 039h, 01dh, 039h, 027h, 039h, 031h, 039h, 03bh, 039h
    db  00ah, 009h, 006h, 004h, 002h, 001h, 000h, 0b9h, 03ah, 062h, 03ah, 070h, 03ah, 081h, 03ah, 091h
    db  03ah, 0a6h, 03ah, 0b9h, 03ah, 0b9h, 03ah
int10_func:                                  ; 0xc3711 LB 0x3eb
    push bp                                   ; 55                          ; 0xc3711 vgabios.c:2414
    mov bp, sp                                ; 89 e5                       ; 0xc3712
    push si                                   ; 56                          ; 0xc3714
    push di                                   ; 57                          ; 0xc3715
    push ax                                   ; 50                          ; 0xc3716
    mov si, word [bp+004h]                    ; 8b 76 04                    ; 0xc3717
    mov al, byte [bp+013h]                    ; 8a 46 13                    ; 0xc371a vgabios.c:2419
    xor ah, ah                                ; 30 e4                       ; 0xc371d
    mov dx, ax                                ; 89 c2                       ; 0xc371f
    cmp ax, strict word 00056h                ; 3d 56 00                    ; 0xc3721
    jnbe short 03790h                         ; 77 6a                       ; 0xc3724
    push CS                                   ; 0e                          ; 0xc3726
    pop ES                                    ; 07                          ; 0xc3727
    mov cx, strict word 00017h                ; b9 17 00                    ; 0xc3728
    mov di, 0368ah                            ; bf 8a 36                    ; 0xc372b
    repne scasb                               ; f2 ae                       ; 0xc372e
    sal cx, 1                                 ; d1 e1                       ; 0xc3730
    mov di, cx                                ; 89 cf                       ; 0xc3732
    mov ax, word [cs:di+036a0h]               ; 2e 8b 85 a0 36              ; 0xc3734
    jmp ax                                    ; ff e0                       ; 0xc3739
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc373b vgabios.c:2422
    xor ah, ah                                ; 30 e4                       ; 0xc373e
    call 013aeh                               ; e8 6b dc                    ; 0xc3740
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3743 vgabios.c:2423
    and ax, strict word 0007fh                ; 25 7f 00                    ; 0xc3746
    cmp ax, strict word 00007h                ; 3d 07 00                    ; 0xc3749
    je short 03763h                           ; 74 15                       ; 0xc374c
    cmp ax, strict word 00006h                ; 3d 06 00                    ; 0xc374e
    je short 0375ah                           ; 74 07                       ; 0xc3751
    cmp ax, strict word 00005h                ; 3d 05 00                    ; 0xc3753
    jbe short 03763h                          ; 76 0b                       ; 0xc3756
    jmp short 0376ch                          ; eb 12                       ; 0xc3758
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc375a vgabios.c:2425
    xor al, al                                ; 30 c0                       ; 0xc375d
    or AL, strict byte 03fh                   ; 0c 3f                       ; 0xc375f
    jmp short 03773h                          ; eb 10                       ; 0xc3761 vgabios.c:2426
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3763 vgabios.c:2434
    xor al, al                                ; 30 c0                       ; 0xc3766
    or AL, strict byte 030h                   ; 0c 30                       ; 0xc3768
    jmp short 03773h                          ; eb 07                       ; 0xc376a
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc376c vgabios.c:2437
    xor al, al                                ; 30 c0                       ; 0xc376f
    or AL, strict byte 020h                   ; 0c 20                       ; 0xc3771
    mov word [bp+012h], ax                    ; 89 46 12                    ; 0xc3773
    jmp short 03790h                          ; eb 18                       ; 0xc3776 vgabios.c:2439
    mov dl, byte [bp+010h]                    ; 8a 56 10                    ; 0xc3778 vgabios.c:2441
    mov al, byte [bp+011h]                    ; 8a 46 11                    ; 0xc377b
    xor ah, ah                                ; 30 e4                       ; 0xc377e
    call 01150h                               ; e8 cd d9                    ; 0xc3780
    jmp short 03790h                          ; eb 0b                       ; 0xc3783 vgabios.c:2442
    mov dx, word [bp+00eh]                    ; 8b 56 0e                    ; 0xc3785 vgabios.c:2444
    mov al, byte [bp+00dh]                    ; 8a 46 0d                    ; 0xc3788
    xor ah, ah                                ; 30 e4                       ; 0xc378b
    call 0124ch                               ; e8 bc da                    ; 0xc378d
    jmp near 03af5h                           ; e9 62 03                    ; 0xc3790 vgabios.c:2445
    lea bx, [bp+00eh]                         ; 8d 5e 0e                    ; 0xc3793 vgabios.c:2447
    lea dx, [bp+010h]                         ; 8d 56 10                    ; 0xc3796
    mov al, byte [bp+00dh]                    ; 8a 46 0d                    ; 0xc3799
    xor ah, ah                                ; 30 e4                       ; 0xc379c
    call 00a0ch                               ; e8 6b d2                    ; 0xc379e
    jmp short 03790h                          ; eb ed                       ; 0xc37a1 vgabios.c:2448
    xor ax, ax                                ; 31 c0                       ; 0xc37a3 vgabios.c:2454
    mov word [bp+012h], ax                    ; 89 46 12                    ; 0xc37a5
    mov word [bp+00ch], ax                    ; 89 46 0c                    ; 0xc37a8 vgabios.c:2455
    mov word [bp+010h], ax                    ; 89 46 10                    ; 0xc37ab vgabios.c:2456
    mov word [bp+00eh], ax                    ; 89 46 0e                    ; 0xc37ae vgabios.c:2457
    jmp short 03790h                          ; eb dd                       ; 0xc37b1 vgabios.c:2458
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc37b3 vgabios.c:2460
    xor ah, ah                                ; 30 e4                       ; 0xc37b6
    call 012d9h                               ; e8 1e db                    ; 0xc37b8
    jmp short 03790h                          ; eb d3                       ; 0xc37bb vgabios.c:2461
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc37bd vgabios.c:2463
    push ax                                   ; 50                          ; 0xc37c0
    mov ax, 000ffh                            ; b8 ff 00                    ; 0xc37c1
    push ax                                   ; 50                          ; 0xc37c4
    mov al, byte [bp+00eh]                    ; 8a 46 0e                    ; 0xc37c5
    xor ah, ah                                ; 30 e4                       ; 0xc37c8
    push ax                                   ; 50                          ; 0xc37ca
    mov al, byte [bp+00fh]                    ; 8a 46 0f                    ; 0xc37cb
    push ax                                   ; 50                          ; 0xc37ce
    mov al, byte [bp+010h]                    ; 8a 46 10                    ; 0xc37cf
    mov cx, ax                                ; 89 c1                       ; 0xc37d2
    mov al, byte [bp+011h]                    ; 8a 46 11                    ; 0xc37d4
    mov bx, ax                                ; 89 c3                       ; 0xc37d7
    mov al, byte [bp+00dh]                    ; 8a 46 0d                    ; 0xc37d9
    mov dx, ax                                ; 89 c2                       ; 0xc37dc
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc37de
    call 01a8fh                               ; e8 ab e2                    ; 0xc37e1
    jmp short 03790h                          ; eb aa                       ; 0xc37e4 vgabios.c:2464
    xor ax, ax                                ; 31 c0                       ; 0xc37e6 vgabios.c:2466
    push ax                                   ; 50                          ; 0xc37e8
    mov ax, 000ffh                            ; b8 ff 00                    ; 0xc37e9
    push ax                                   ; 50                          ; 0xc37ec
    mov al, byte [bp+00eh]                    ; 8a 46 0e                    ; 0xc37ed
    xor ah, ah                                ; 30 e4                       ; 0xc37f0
    push ax                                   ; 50                          ; 0xc37f2
    mov al, byte [bp+00fh]                    ; 8a 46 0f                    ; 0xc37f3
    push ax                                   ; 50                          ; 0xc37f6
    mov cl, byte [bp+010h]                    ; 8a 4e 10                    ; 0xc37f7
    xor ch, ch                                ; 30 ed                       ; 0xc37fa
    jmp short 037d4h                          ; eb d6                       ; 0xc37fc
    lea dx, [bp+012h]                         ; 8d 56 12                    ; 0xc37fe vgabios.c:2469
    mov al, byte [bp+00dh]                    ; 8a 46 0d                    ; 0xc3801
    xor ah, ah                                ; 30 e4                       ; 0xc3804
    call 00d62h                               ; e8 59 d5                    ; 0xc3806
    jmp short 03790h                          ; eb 85                       ; 0xc3809 vgabios.c:2470
    mov cx, word [bp+010h]                    ; 8b 4e 10                    ; 0xc380b vgabios.c:2472
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc380e
    xor ah, ah                                ; 30 e4                       ; 0xc3811
    mov bx, ax                                ; 89 c3                       ; 0xc3813
    mov al, byte [bp+00dh]                    ; 8a 46 0d                    ; 0xc3815
    mov dx, ax                                ; 89 c2                       ; 0xc3818
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc381a
    call 023ffh                               ; e8 df eb                    ; 0xc381d
    jmp near 03af5h                           ; e9 d2 02                    ; 0xc3820 vgabios.c:2473
    mov cx, word [bp+010h]                    ; 8b 4e 10                    ; 0xc3823 vgabios.c:2475
    mov bl, byte [bp+00ch]                    ; 8a 5e 0c                    ; 0xc3826
    xor bh, bh                                ; 30 ff                       ; 0xc3829
    mov dl, byte [bp+00dh]                    ; 8a 56 0d                    ; 0xc382b
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc382e
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc3831
    mov byte [bp-005h], dh                    ; 88 76 fb                    ; 0xc3834
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc3837
    call 02572h                               ; e8 35 ed                    ; 0xc383a
    jmp near 03af5h                           ; e9 b5 02                    ; 0xc383d vgabios.c:2476
    mov cx, word [bp+00eh]                    ; 8b 4e 0e                    ; 0xc3840 vgabios.c:2478
    mov bx, word [bp+010h]                    ; 8b 5e 10                    ; 0xc3843
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3846
    xor ah, ah                                ; 30 e4                       ; 0xc3849
    mov dx, ax                                ; 89 c2                       ; 0xc384b
    mov al, byte [bp+00dh]                    ; 8a 46 0d                    ; 0xc384d
    call 026f5h                               ; e8 a2 ee                    ; 0xc3850
    jmp near 03af5h                           ; e9 9f 02                    ; 0xc3853 vgabios.c:2479
    lea cx, [bp+012h]                         ; 8d 4e 12                    ; 0xc3856 vgabios.c:2481
    mov bx, word [bp+00eh]                    ; 8b 5e 0e                    ; 0xc3859
    mov dx, word [bp+010h]                    ; 8b 56 10                    ; 0xc385c
    mov al, byte [bp+00dh]                    ; 8a 46 0d                    ; 0xc385f
    xor ah, ah                                ; 30 e4                       ; 0xc3862
    call 00f44h                               ; e8 dd d6                    ; 0xc3864
    jmp near 03af5h                           ; e9 8b 02                    ; 0xc3867 vgabios.c:2482
    mov cx, strict word 00002h                ; b9 02 00                    ; 0xc386a vgabios.c:2490
    mov bl, byte [bp+00ch]                    ; 8a 5e 0c                    ; 0xc386d
    xor bh, bh                                ; 30 ff                       ; 0xc3870
    mov dx, 000ffh                            ; ba ff 00                    ; 0xc3872
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3875
    xor ah, ah                                ; 30 e4                       ; 0xc3878
    call 0286fh                               ; e8 f2 ef                    ; 0xc387a
    jmp near 03af5h                           ; e9 75 02                    ; 0xc387d vgabios.c:2491
    mov dx, word [bp+010h]                    ; 8b 56 10                    ; 0xc3880 vgabios.c:2494
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3883
    call 010b1h                               ; e8 28 d8                    ; 0xc3886
    jmp near 03af5h                           ; e9 69 02                    ; 0xc3889 vgabios.c:2495
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc388c vgabios.c:2497
    xor ah, ah                                ; 30 e4                       ; 0xc388f
    cmp ax, strict word 00030h                ; 3d 30 00                    ; 0xc3891
    jnbe short 038ffh                         ; 77 69                       ; 0xc3894
    push CS                                   ; 0e                          ; 0xc3896
    pop ES                                    ; 07                          ; 0xc3897
    mov cx, strict word 0000fh                ; b9 0f 00                    ; 0xc3898
    mov di, 036ceh                            ; bf ce 36                    ; 0xc389b
    repne scasb                               ; f2 ae                       ; 0xc389e
    sal cx, 1                                 ; d1 e1                       ; 0xc38a0
    mov di, cx                                ; 89 cf                       ; 0xc38a2
    mov ax, word [cs:di+036dch]               ; 2e 8b 85 dc 36              ; 0xc38a4
    jmp ax                                    ; ff e0                       ; 0xc38a9
    mov al, byte [bp+00dh]                    ; 8a 46 0d                    ; 0xc38ab vgabios.c:2501
    xor ah, ah                                ; 30 e4                       ; 0xc38ae
    push ax                                   ; 50                          ; 0xc38b0
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc38b1
    push ax                                   ; 50                          ; 0xc38b4
    push word [bp+00eh]                       ; ff 76 0e                    ; 0xc38b5
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc38b8
    mov cx, word [bp+010h]                    ; 8b 4e 10                    ; 0xc38bb
    mov bx, word [bp+008h]                    ; 8b 5e 08                    ; 0xc38be
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc38c1
    call 02bf0h                               ; e8 29 f3                    ; 0xc38c4
    jmp short 038ffh                          ; eb 36                       ; 0xc38c7 vgabios.c:2502
    mov dl, byte [bp+00ch]                    ; 8a 56 0c                    ; 0xc38c9 vgabios.c:2505
    xor dh, dh                                ; 30 f6                       ; 0xc38cc
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc38ce
    xor ah, ah                                ; 30 e4                       ; 0xc38d1
    call 02c75h                               ; e8 9f f3                    ; 0xc38d3
    jmp short 038ffh                          ; eb 27                       ; 0xc38d6 vgabios.c:2506
    mov dl, byte [bp+00ch]                    ; 8a 56 0c                    ; 0xc38d8 vgabios.c:2509
    xor dh, dh                                ; 30 f6                       ; 0xc38db
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc38dd
    xor ah, ah                                ; 30 e4                       ; 0xc38e0
    call 02cebh                               ; e8 06 f4                    ; 0xc38e2
    jmp short 038ffh                          ; eb 18                       ; 0xc38e5 vgabios.c:2510
    mov dl, byte [bp+00ch]                    ; 8a 56 0c                    ; 0xc38e7 vgabios.c:2513
    xor dh, dh                                ; 30 f6                       ; 0xc38ea
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc38ec
    xor ah, ah                                ; 30 e4                       ; 0xc38ef
    call 02d5fh                               ; e8 6b f4                    ; 0xc38f1
    jmp short 038ffh                          ; eb 09                       ; 0xc38f4 vgabios.c:2514
    mov dx, word [bp+008h]                    ; 8b 56 08                    ; 0xc38f6 vgabios.c:2516
    mov ax, word [bp+016h]                    ; 8b 46 16                    ; 0xc38f9
    call 02dd3h                               ; e8 d4 f4                    ; 0xc38fc
    jmp near 03af5h                           ; e9 f3 01                    ; 0xc38ff vgabios.c:2517
    mov al, byte [bp+00eh]                    ; 8a 46 0e                    ; 0xc3902 vgabios.c:2519
    xor ah, ah                                ; 30 e4                       ; 0xc3905
    push ax                                   ; 50                          ; 0xc3907
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc3908
    mov bx, word [bp+010h]                    ; 8b 5e 10                    ; 0xc390b
    mov dx, word [bp+008h]                    ; 8b 56 08                    ; 0xc390e
    mov si, word [bp+016h]                    ; 8b 76 16                    ; 0xc3911
    mov cx, ax                                ; 89 c1                       ; 0xc3914
    mov ax, si                                ; 89 f0                       ; 0xc3916
    call 02dd8h                               ; e8 bd f4                    ; 0xc3918
    jmp short 038ffh                          ; eb e2                       ; 0xc391b vgabios.c:2520
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc391d vgabios.c:2522
    xor ah, ah                                ; 30 e4                       ; 0xc3920
    call 02ddfh                               ; e8 ba f4                    ; 0xc3922
    jmp short 038ffh                          ; eb d8                       ; 0xc3925 vgabios.c:2523
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc3927 vgabios.c:2525
    xor ah, ah                                ; 30 e4                       ; 0xc392a
    call 02de4h                               ; e8 b5 f4                    ; 0xc392c
    jmp short 038ffh                          ; eb ce                       ; 0xc392f vgabios.c:2526
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc3931 vgabios.c:2528
    xor ah, ah                                ; 30 e4                       ; 0xc3934
    call 02de9h                               ; e8 b0 f4                    ; 0xc3936
    jmp short 038ffh                          ; eb c4                       ; 0xc3939 vgabios.c:2529
    lea ax, [bp+00eh]                         ; 8d 46 0e                    ; 0xc393b vgabios.c:2531
    push ax                                   ; 50                          ; 0xc393e
    lea cx, [bp+010h]                         ; 8d 4e 10                    ; 0xc393f
    lea bx, [bp+008h]                         ; 8d 5e 08                    ; 0xc3942
    lea dx, [bp+016h]                         ; 8d 56 16                    ; 0xc3945
    mov al, byte [bp+00dh]                    ; 8a 46 0d                    ; 0xc3948
    xor ah, ah                                ; 30 e4                       ; 0xc394b
    call 00eb9h                               ; e8 69 d5                    ; 0xc394d
    jmp short 038ffh                          ; eb ad                       ; 0xc3950 vgabios.c:2539
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3952 vgabios.c:2541
    xor ah, ah                                ; 30 e4                       ; 0xc3955
    cmp ax, strict word 00034h                ; 3d 34 00                    ; 0xc3957
    jc short 0396ah                           ; 72 0e                       ; 0xc395a
    jbe short 03974h                          ; 76 16                       ; 0xc395c
    cmp ax, strict word 00036h                ; 3d 36 00                    ; 0xc395e
    je short 039b9h                           ; 74 56                       ; 0xc3961
    cmp ax, strict word 00035h                ; 3d 35 00                    ; 0xc3963
    je short 039abh                           ; 74 43                       ; 0xc3966
    jmp short 038ffh                          ; eb 95                       ; 0xc3968
    cmp ax, strict word 00020h                ; 3d 20 00                    ; 0xc396a
    jne short 039e2h                          ; 75 73                       ; 0xc396d
    call 02deeh                               ; e8 7c f4                    ; 0xc396f vgabios.c:2544
    jmp short 039e2h                          ; eb 6e                       ; 0xc3972 vgabios.c:2545
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3974 vgabios.c:2547
    xor ah, ah                                ; 30 e4                       ; 0xc3977
    cmp ax, strict word 00002h                ; 3d 02 00                    ; 0xc3979
    jnc short 039a6h                          ; 73 28                       ; 0xc397c
    mov dx, 00087h                            ; ba 87 00                    ; 0xc397e vgabios.c:2548
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3981
    call 0367ch                               ; e8 f5 fc                    ; 0xc3984
    mov dl, al                                ; 88 c2                       ; 0xc3987
    and dl, 0feh                              ; 80 e2 fe                    ; 0xc3989
    mov ah, byte [bp+012h]                    ; 8a 66 12                    ; 0xc398c
    or dl, ah                                 ; 08 e2                       ; 0xc398f
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3991 vgabios.c:40
    mov bx, 00087h                            ; bb 87 00                    ; 0xc3994
    mov es, ax                                ; 8e c0                       ; 0xc3997 vgabios.c:42
    mov byte [es:bx], dl                      ; 26 88 17                    ; 0xc3999
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc399c vgabios.c:2550
    xor al, al                                ; 30 c0                       ; 0xc399f
    or AL, strict byte 012h                   ; 0c 12                       ; 0xc39a1
    jmp near 03773h                           ; e9 cd fd                    ; 0xc39a3
    mov byte [bp+012h], ah                    ; 88 66 12                    ; 0xc39a6 vgabios.c:2553
    jmp short 039e2h                          ; eb 37                       ; 0xc39a9 vgabios.c:2554
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc39ab vgabios.c:2556
    mov bx, word [bp+00eh]                    ; 8b 5e 0e                    ; 0xc39ae
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc39b1
    call 02df3h                               ; e8 3c f4                    ; 0xc39b4
    jmp short 0399ch                          ; eb e3                       ; 0xc39b7
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc39b9 vgabios.c:2560
    call 02df8h                               ; e8 39 f4                    ; 0xc39bc
    jmp short 0399ch                          ; eb db                       ; 0xc39bf
    push word [bp+008h]                       ; ff 76 08                    ; 0xc39c1 vgabios.c:2570
    push word [bp+016h]                       ; ff 76 16                    ; 0xc39c4
    mov al, byte [bp+00eh]                    ; 8a 46 0e                    ; 0xc39c7
    xor ah, ah                                ; 30 e4                       ; 0xc39ca
    push ax                                   ; 50                          ; 0xc39cc
    mov al, byte [bp+00fh]                    ; 8a 46 0f                    ; 0xc39cd
    push ax                                   ; 50                          ; 0xc39d0
    mov bl, byte [bp+00ch]                    ; 8a 5e 0c                    ; 0xc39d1
    xor bh, bh                                ; 30 ff                       ; 0xc39d4
    mov dl, byte [bp+00dh]                    ; 8a 56 0d                    ; 0xc39d6
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc39d9
    mov cx, word [bp+010h]                    ; 8b 4e 10                    ; 0xc39dc
    call 02dfdh                               ; e8 1b f4                    ; 0xc39df
    jmp near 03af5h                           ; e9 10 01                    ; 0xc39e2 vgabios.c:2571
    mov bx, si                                ; 89 f3                       ; 0xc39e5 vgabios.c:2573
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc39e7
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc39ea
    call 02e8ch                               ; e8 9c f4                    ; 0xc39ed
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc39f0 vgabios.c:2574
    xor al, al                                ; 30 c0                       ; 0xc39f3
    or AL, strict byte 01bh                   ; 0c 1b                       ; 0xc39f5
    jmp near 03773h                           ; e9 79 fd                    ; 0xc39f7
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc39fa vgabios.c:2577
    xor ah, ah                                ; 30 e4                       ; 0xc39fd
    cmp ax, strict word 00002h                ; 3d 02 00                    ; 0xc39ff
    je short 03a26h                           ; 74 22                       ; 0xc3a02
    cmp ax, strict word 00001h                ; 3d 01 00                    ; 0xc3a04
    je short 03a18h                           ; 74 0f                       ; 0xc3a07
    test ax, ax                               ; 85 c0                       ; 0xc3a09
    jne short 03a32h                          ; 75 25                       ; 0xc3a0b
    lea dx, [bp+00ch]                         ; 8d 56 0c                    ; 0xc3a0d vgabios.c:2580
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc3a10
    call 030a4h                               ; e8 8e f6                    ; 0xc3a13
    jmp short 03a32h                          ; eb 1a                       ; 0xc3a16 vgabios.c:2581
    mov bx, word [bp+00ch]                    ; 8b 5e 0c                    ; 0xc3a18 vgabios.c:2583
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc3a1b
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc3a1e
    call 030bfh                               ; e8 9b f6                    ; 0xc3a21
    jmp short 03a32h                          ; eb 0c                       ; 0xc3a24 vgabios.c:2584
    mov bx, word [bp+00ch]                    ; 8b 5e 0c                    ; 0xc3a26 vgabios.c:2586
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc3a29
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc3a2c
    call 03397h                               ; e8 65 f9                    ; 0xc3a2f
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3a32 vgabios.c:2593
    xor al, al                                ; 30 c0                       ; 0xc3a35
    or AL, strict byte 01ch                   ; 0c 1c                       ; 0xc3a37
    jmp near 03773h                           ; e9 37 fd                    ; 0xc3a39
    call 007e8h                               ; e8 a9 cd                    ; 0xc3a3c vgabios.c:2598
    test ax, ax                               ; 85 c0                       ; 0xc3a3f
    je short 03ab7h                           ; 74 74                       ; 0xc3a41
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3a43 vgabios.c:2599
    xor ah, ah                                ; 30 e4                       ; 0xc3a46
    cmp ax, strict word 0000ah                ; 3d 0a 00                    ; 0xc3a48
    jnbe short 03ab9h                         ; 77 6c                       ; 0xc3a4b
    push CS                                   ; 0e                          ; 0xc3a4d
    pop ES                                    ; 07                          ; 0xc3a4e
    mov cx, strict word 00008h                ; b9 08 00                    ; 0xc3a4f
    mov di, 036fah                            ; bf fa 36                    ; 0xc3a52
    repne scasb                               ; f2 ae                       ; 0xc3a55
    sal cx, 1                                 ; d1 e1                       ; 0xc3a57
    mov di, cx                                ; 89 cf                       ; 0xc3a59
    mov ax, word [cs:di+03701h]               ; 2e 8b 85 01 37              ; 0xc3a5b
    jmp ax                                    ; ff e0                       ; 0xc3a60
    mov bx, si                                ; 89 f3                       ; 0xc3a62 vgabios.c:2602
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc3a64
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3a67
    call 03cafh                               ; e8 42 02                    ; 0xc3a6a
    jmp near 03af5h                           ; e9 85 00                    ; 0xc3a6d vgabios.c:2603
    mov cx, si                                ; 89 f1                       ; 0xc3a70 vgabios.c:2605
    mov bx, word [bp+016h]                    ; 8b 5e 16                    ; 0xc3a72
    mov dx, word [bp+010h]                    ; 8b 56 10                    ; 0xc3a75
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3a78
    call 03ddah                               ; e8 5c 03                    ; 0xc3a7b
    jmp near 03af5h                           ; e9 74 00                    ; 0xc3a7e vgabios.c:2606
    mov cx, si                                ; 89 f1                       ; 0xc3a81 vgabios.c:2608
    mov bx, word [bp+016h]                    ; 8b 5e 16                    ; 0xc3a83
    mov dx, word [bp+00ch]                    ; 8b 56 0c                    ; 0xc3a86
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3a89
    call 03e7ah                               ; e8 eb 03                    ; 0xc3a8c
    jmp short 03af5h                          ; eb 64                       ; 0xc3a8f vgabios.c:2609
    lea ax, [bp+00ch]                         ; 8d 46 0c                    ; 0xc3a91 vgabios.c:2611
    push ax                                   ; 50                          ; 0xc3a94
    mov cx, word [bp+016h]                    ; 8b 4e 16                    ; 0xc3a95
    mov bx, word [bp+00eh]                    ; 8b 5e 0e                    ; 0xc3a98
    mov dx, word [bp+010h]                    ; 8b 56 10                    ; 0xc3a9b
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3a9e
    call 04043h                               ; e8 9f 05                    ; 0xc3aa1
    jmp short 03af5h                          ; eb 4f                       ; 0xc3aa4 vgabios.c:2612
    lea cx, [bp+00eh]                         ; 8d 4e 0e                    ; 0xc3aa6 vgabios.c:2614
    lea bx, [bp+010h]                         ; 8d 5e 10                    ; 0xc3aa9
    lea dx, [bp+00ch]                         ; 8d 56 0c                    ; 0xc3aac
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3aaf
    call 040d0h                               ; e8 1b 06                    ; 0xc3ab2
    jmp short 03af5h                          ; eb 3e                       ; 0xc3ab5 vgabios.c:2615
    jmp short 03ac0h                          ; eb 07                       ; 0xc3ab7
    mov word [bp+012h], 00100h                ; c7 46 12 00 01              ; 0xc3ab9 vgabios.c:2637
    jmp short 03af5h                          ; eb 35                       ; 0xc3abe vgabios.c:2640
    mov word [bp+012h], 00100h                ; c7 46 12 00 01              ; 0xc3ac0 vgabios.c:2642
    jmp short 03af5h                          ; eb 2e                       ; 0xc3ac5 vgabios.c:2644
    call 007e8h                               ; e8 1e cd                    ; 0xc3ac7 vgabios.c:2646
    test ax, ax                               ; 85 c0                       ; 0xc3aca
    je short 03af0h                           ; 74 22                       ; 0xc3acc
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3ace vgabios.c:2647
    xor ah, ah                                ; 30 e4                       ; 0xc3ad1
    cmp ax, strict word 00042h                ; 3d 42 00                    ; 0xc3ad3
    jne short 03ae9h                          ; 75 11                       ; 0xc3ad6
    lea cx, [bp+00eh]                         ; 8d 4e 0e                    ; 0xc3ad8 vgabios.c:2650
    lea bx, [bp+010h]                         ; 8d 5e 10                    ; 0xc3adb
    lea dx, [bp+00ch]                         ; 8d 56 0c                    ; 0xc3ade
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3ae1
    call 041b2h                               ; e8 cb 06                    ; 0xc3ae4
    jmp short 03af5h                          ; eb 0c                       ; 0xc3ae7 vgabios.c:2651
    mov word [bp+012h], 00100h                ; c7 46 12 00 01              ; 0xc3ae9 vgabios.c:2653
    jmp short 03af5h                          ; eb 05                       ; 0xc3aee vgabios.c:2656
    mov word [bp+012h], 00100h                ; c7 46 12 00 01              ; 0xc3af0 vgabios.c:2658
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3af5 vgabios.c:2668
    pop di                                    ; 5f                          ; 0xc3af8
    pop si                                    ; 5e                          ; 0xc3af9
    pop bp                                    ; 5d                          ; 0xc3afa
    retn                                      ; c3                          ; 0xc3afb
  ; disGetNextSymbol 0xc3afc LB 0x7ad -> off=0x0 cb=000000000000001f uValue=00000000000c3afc 'dispi_set_xres'
dispi_set_xres:                              ; 0xc3afc LB 0x1f
    push bp                                   ; 55                          ; 0xc3afc vbe.c:100
    mov bp, sp                                ; 89 e5                       ; 0xc3afd
    push bx                                   ; 53                          ; 0xc3aff
    push dx                                   ; 52                          ; 0xc3b00
    mov bx, ax                                ; 89 c3                       ; 0xc3b01
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc3b03 vbe.c:105
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3b06
    call 00590h                               ; e8 84 ca                    ; 0xc3b09
    mov ax, bx                                ; 89 d8                       ; 0xc3b0c vbe.c:106
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3b0e
    call 00590h                               ; e8 7c ca                    ; 0xc3b11
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3b14 vbe.c:107
    pop dx                                    ; 5a                          ; 0xc3b17
    pop bx                                    ; 5b                          ; 0xc3b18
    pop bp                                    ; 5d                          ; 0xc3b19
    retn                                      ; c3                          ; 0xc3b1a
  ; disGetNextSymbol 0xc3b1b LB 0x78e -> off=0x0 cb=000000000000001f uValue=00000000000c3b1b 'dispi_set_yres'
dispi_set_yres:                              ; 0xc3b1b LB 0x1f
    push bp                                   ; 55                          ; 0xc3b1b vbe.c:109
    mov bp, sp                                ; 89 e5                       ; 0xc3b1c
    push bx                                   ; 53                          ; 0xc3b1e
    push dx                                   ; 52                          ; 0xc3b1f
    mov bx, ax                                ; 89 c3                       ; 0xc3b20
    mov ax, strict word 00002h                ; b8 02 00                    ; 0xc3b22 vbe.c:114
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3b25
    call 00590h                               ; e8 65 ca                    ; 0xc3b28
    mov ax, bx                                ; 89 d8                       ; 0xc3b2b vbe.c:115
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3b2d
    call 00590h                               ; e8 5d ca                    ; 0xc3b30
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3b33 vbe.c:116
    pop dx                                    ; 5a                          ; 0xc3b36
    pop bx                                    ; 5b                          ; 0xc3b37
    pop bp                                    ; 5d                          ; 0xc3b38
    retn                                      ; c3                          ; 0xc3b39
  ; disGetNextSymbol 0xc3b3a LB 0x76f -> off=0x0 cb=0000000000000019 uValue=00000000000c3b3a 'dispi_get_yres'
dispi_get_yres:                              ; 0xc3b3a LB 0x19
    push bp                                   ; 55                          ; 0xc3b3a vbe.c:118
    mov bp, sp                                ; 89 e5                       ; 0xc3b3b
    push dx                                   ; 52                          ; 0xc3b3d
    mov ax, strict word 00002h                ; b8 02 00                    ; 0xc3b3e vbe.c:120
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3b41
    call 00590h                               ; e8 49 ca                    ; 0xc3b44
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3b47 vbe.c:121
    call 00597h                               ; e8 4a ca                    ; 0xc3b4a
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3b4d vbe.c:122
    pop dx                                    ; 5a                          ; 0xc3b50
    pop bp                                    ; 5d                          ; 0xc3b51
    retn                                      ; c3                          ; 0xc3b52
  ; disGetNextSymbol 0xc3b53 LB 0x756 -> off=0x0 cb=000000000000001f uValue=00000000000c3b53 'dispi_set_bpp'
dispi_set_bpp:                               ; 0xc3b53 LB 0x1f
    push bp                                   ; 55                          ; 0xc3b53 vbe.c:124
    mov bp, sp                                ; 89 e5                       ; 0xc3b54
    push bx                                   ; 53                          ; 0xc3b56
    push dx                                   ; 52                          ; 0xc3b57
    mov bx, ax                                ; 89 c3                       ; 0xc3b58
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc3b5a vbe.c:129
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3b5d
    call 00590h                               ; e8 2d ca                    ; 0xc3b60
    mov ax, bx                                ; 89 d8                       ; 0xc3b63 vbe.c:130
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3b65
    call 00590h                               ; e8 25 ca                    ; 0xc3b68
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3b6b vbe.c:131
    pop dx                                    ; 5a                          ; 0xc3b6e
    pop bx                                    ; 5b                          ; 0xc3b6f
    pop bp                                    ; 5d                          ; 0xc3b70
    retn                                      ; c3                          ; 0xc3b71
  ; disGetNextSymbol 0xc3b72 LB 0x737 -> off=0x0 cb=0000000000000019 uValue=00000000000c3b72 'dispi_get_bpp'
dispi_get_bpp:                               ; 0xc3b72 LB 0x19
    push bp                                   ; 55                          ; 0xc3b72 vbe.c:133
    mov bp, sp                                ; 89 e5                       ; 0xc3b73
    push dx                                   ; 52                          ; 0xc3b75
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc3b76 vbe.c:135
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3b79
    call 00590h                               ; e8 11 ca                    ; 0xc3b7c
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3b7f vbe.c:136
    call 00597h                               ; e8 12 ca                    ; 0xc3b82
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3b85 vbe.c:137
    pop dx                                    ; 5a                          ; 0xc3b88
    pop bp                                    ; 5d                          ; 0xc3b89
    retn                                      ; c3                          ; 0xc3b8a
  ; disGetNextSymbol 0xc3b8b LB 0x71e -> off=0x0 cb=000000000000001f uValue=00000000000c3b8b 'dispi_set_virt_width'
dispi_set_virt_width:                        ; 0xc3b8b LB 0x1f
    push bp                                   ; 55                          ; 0xc3b8b vbe.c:139
    mov bp, sp                                ; 89 e5                       ; 0xc3b8c
    push bx                                   ; 53                          ; 0xc3b8e
    push dx                                   ; 52                          ; 0xc3b8f
    mov bx, ax                                ; 89 c3                       ; 0xc3b90
    mov ax, strict word 00006h                ; b8 06 00                    ; 0xc3b92 vbe.c:144
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3b95
    call 00590h                               ; e8 f5 c9                    ; 0xc3b98
    mov ax, bx                                ; 89 d8                       ; 0xc3b9b vbe.c:145
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3b9d
    call 00590h                               ; e8 ed c9                    ; 0xc3ba0
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3ba3 vbe.c:146
    pop dx                                    ; 5a                          ; 0xc3ba6
    pop bx                                    ; 5b                          ; 0xc3ba7
    pop bp                                    ; 5d                          ; 0xc3ba8
    retn                                      ; c3                          ; 0xc3ba9
  ; disGetNextSymbol 0xc3baa LB 0x6ff -> off=0x0 cb=0000000000000019 uValue=00000000000c3baa 'dispi_get_virt_width'
dispi_get_virt_width:                        ; 0xc3baa LB 0x19
    push bp                                   ; 55                          ; 0xc3baa vbe.c:148
    mov bp, sp                                ; 89 e5                       ; 0xc3bab
    push dx                                   ; 52                          ; 0xc3bad
    mov ax, strict word 00006h                ; b8 06 00                    ; 0xc3bae vbe.c:150
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3bb1
    call 00590h                               ; e8 d9 c9                    ; 0xc3bb4
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3bb7 vbe.c:151
    call 00597h                               ; e8 da c9                    ; 0xc3bba
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3bbd vbe.c:152
    pop dx                                    ; 5a                          ; 0xc3bc0
    pop bp                                    ; 5d                          ; 0xc3bc1
    retn                                      ; c3                          ; 0xc3bc2
  ; disGetNextSymbol 0xc3bc3 LB 0x6e6 -> off=0x0 cb=0000000000000019 uValue=00000000000c3bc3 'dispi_get_virt_height'
dispi_get_virt_height:                       ; 0xc3bc3 LB 0x19
    push bp                                   ; 55                          ; 0xc3bc3 vbe.c:154
    mov bp, sp                                ; 89 e5                       ; 0xc3bc4
    push dx                                   ; 52                          ; 0xc3bc6
    mov ax, strict word 00007h                ; b8 07 00                    ; 0xc3bc7 vbe.c:156
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3bca
    call 00590h                               ; e8 c0 c9                    ; 0xc3bcd
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3bd0 vbe.c:157
    call 00597h                               ; e8 c1 c9                    ; 0xc3bd3
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3bd6 vbe.c:158
    pop dx                                    ; 5a                          ; 0xc3bd9
    pop bp                                    ; 5d                          ; 0xc3bda
    retn                                      ; c3                          ; 0xc3bdb
  ; disGetNextSymbol 0xc3bdc LB 0x6cd -> off=0x0 cb=0000000000000012 uValue=00000000000c3bdc 'in_word'
in_word:                                     ; 0xc3bdc LB 0x12
    push bp                                   ; 55                          ; 0xc3bdc vbe.c:160
    mov bp, sp                                ; 89 e5                       ; 0xc3bdd
    push bx                                   ; 53                          ; 0xc3bdf
    mov bx, ax                                ; 89 c3                       ; 0xc3be0
    mov ax, dx                                ; 89 d0                       ; 0xc3be2
    mov dx, bx                                ; 89 da                       ; 0xc3be4 vbe.c:162
    out DX, ax                                ; ef                          ; 0xc3be6
    in ax, DX                                 ; ed                          ; 0xc3be7 vbe.c:163
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3be8 vbe.c:164
    pop bx                                    ; 5b                          ; 0xc3beb
    pop bp                                    ; 5d                          ; 0xc3bec
    retn                                      ; c3                          ; 0xc3bed
  ; disGetNextSymbol 0xc3bee LB 0x6bb -> off=0x0 cb=0000000000000014 uValue=00000000000c3bee 'in_byte'
in_byte:                                     ; 0xc3bee LB 0x14
    push bp                                   ; 55                          ; 0xc3bee vbe.c:166
    mov bp, sp                                ; 89 e5                       ; 0xc3bef
    push bx                                   ; 53                          ; 0xc3bf1
    mov bx, ax                                ; 89 c3                       ; 0xc3bf2
    mov ax, dx                                ; 89 d0                       ; 0xc3bf4
    mov dx, bx                                ; 89 da                       ; 0xc3bf6 vbe.c:168
    out DX, ax                                ; ef                          ; 0xc3bf8
    in AL, DX                                 ; ec                          ; 0xc3bf9 vbe.c:169
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3bfa
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3bfc vbe.c:170
    pop bx                                    ; 5b                          ; 0xc3bff
    pop bp                                    ; 5d                          ; 0xc3c00
    retn                                      ; c3                          ; 0xc3c01
  ; disGetNextSymbol 0xc3c02 LB 0x6a7 -> off=0x0 cb=0000000000000014 uValue=00000000000c3c02 'dispi_get_id'
dispi_get_id:                                ; 0xc3c02 LB 0x14
    push bp                                   ; 55                          ; 0xc3c02 vbe.c:173
    mov bp, sp                                ; 89 e5                       ; 0xc3c03
    push dx                                   ; 52                          ; 0xc3c05
    xor ax, ax                                ; 31 c0                       ; 0xc3c06 vbe.c:175
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3c08
    out DX, ax                                ; ef                          ; 0xc3c0b
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3c0c vbe.c:176
    in ax, DX                                 ; ed                          ; 0xc3c0f
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3c10 vbe.c:177
    pop dx                                    ; 5a                          ; 0xc3c13
    pop bp                                    ; 5d                          ; 0xc3c14
    retn                                      ; c3                          ; 0xc3c15
  ; disGetNextSymbol 0xc3c16 LB 0x693 -> off=0x0 cb=000000000000001a uValue=00000000000c3c16 'dispi_set_id'
dispi_set_id:                                ; 0xc3c16 LB 0x1a
    push bp                                   ; 55                          ; 0xc3c16 vbe.c:179
    mov bp, sp                                ; 89 e5                       ; 0xc3c17
    push bx                                   ; 53                          ; 0xc3c19
    push dx                                   ; 52                          ; 0xc3c1a
    mov bx, ax                                ; 89 c3                       ; 0xc3c1b
    xor ax, ax                                ; 31 c0                       ; 0xc3c1d vbe.c:181
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3c1f
    out DX, ax                                ; ef                          ; 0xc3c22
    mov ax, bx                                ; 89 d8                       ; 0xc3c23 vbe.c:182
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3c25
    out DX, ax                                ; ef                          ; 0xc3c28
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3c29 vbe.c:183
    pop dx                                    ; 5a                          ; 0xc3c2c
    pop bx                                    ; 5b                          ; 0xc3c2d
    pop bp                                    ; 5d                          ; 0xc3c2e
    retn                                      ; c3                          ; 0xc3c2f
  ; disGetNextSymbol 0xc3c30 LB 0x679 -> off=0x0 cb=000000000000002a uValue=00000000000c3c30 'vbe_init'
vbe_init:                                    ; 0xc3c30 LB 0x2a
    push bp                                   ; 55                          ; 0xc3c30 vbe.c:188
    mov bp, sp                                ; 89 e5                       ; 0xc3c31
    push bx                                   ; 53                          ; 0xc3c33
    mov ax, 0b0c0h                            ; b8 c0 b0                    ; 0xc3c34 vbe.c:190
    call 03c16h                               ; e8 dc ff                    ; 0xc3c37
    call 03c02h                               ; e8 c5 ff                    ; 0xc3c3a vbe.c:191
    cmp ax, 0b0c0h                            ; 3d c0 b0                    ; 0xc3c3d
    jne short 03c54h                          ; 75 12                       ; 0xc3c40
    mov bx, 000b9h                            ; bb b9 00                    ; 0xc3c42 vbe.c:42
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3c45
    mov es, ax                                ; 8e c0                       ; 0xc3c48
    mov byte [es:bx], 001h                    ; 26 c6 07 01                 ; 0xc3c4a
    mov ax, 0b0c4h                            ; b8 c4 b0                    ; 0xc3c4e vbe.c:194
    call 03c16h                               ; e8 c2 ff                    ; 0xc3c51
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3c54 vbe.c:199
    pop bx                                    ; 5b                          ; 0xc3c57
    pop bp                                    ; 5d                          ; 0xc3c58
    retn                                      ; c3                          ; 0xc3c59
  ; disGetNextSymbol 0xc3c5a LB 0x64f -> off=0x0 cb=0000000000000055 uValue=00000000000c3c5a 'mode_info_find_mode'
mode_info_find_mode:                         ; 0xc3c5a LB 0x55
    push bp                                   ; 55                          ; 0xc3c5a vbe.c:202
    mov bp, sp                                ; 89 e5                       ; 0xc3c5b
    push bx                                   ; 53                          ; 0xc3c5d
    push cx                                   ; 51                          ; 0xc3c5e
    push si                                   ; 56                          ; 0xc3c5f
    push di                                   ; 57                          ; 0xc3c60
    mov di, ax                                ; 89 c7                       ; 0xc3c61
    mov si, dx                                ; 89 d6                       ; 0xc3c63
    xor dx, dx                                ; 31 d2                       ; 0xc3c65 vbe.c:208
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3c67
    call 03bdch                               ; e8 6f ff                    ; 0xc3c6a
    cmp ax, 077cch                            ; 3d cc 77                    ; 0xc3c6d vbe.c:209
    jne short 03ca4h                          ; 75 32                       ; 0xc3c70
    mov bx, strict word 00004h                ; bb 04 00                    ; 0xc3c72 vbe.c:213
    mov dx, bx                                ; 89 da                       ; 0xc3c75 vbe.c:218
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3c77
    call 03bdch                               ; e8 5f ff                    ; 0xc3c7a
    mov cx, ax                                ; 89 c1                       ; 0xc3c7d
    cmp cx, strict byte 0ffffh                ; 83 f9 ff                    ; 0xc3c7f vbe.c:219
    je short 03ca4h                           ; 74 20                       ; 0xc3c82
    lea dx, [bx+002h]                         ; 8d 57 02                    ; 0xc3c84 vbe.c:221
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3c87
    call 03bdch                               ; e8 4f ff                    ; 0xc3c8a
    lea dx, [bx+044h]                         ; 8d 57 44                    ; 0xc3c8d
    cmp cx, di                                ; 39 f9                       ; 0xc3c90 vbe.c:223
    jne short 03ca0h                          ; 75 0c                       ; 0xc3c92
    test si, si                               ; 85 f6                       ; 0xc3c94 vbe.c:225
    jne short 03c9ch                          ; 75 04                       ; 0xc3c96
    mov ax, bx                                ; 89 d8                       ; 0xc3c98 vbe.c:226
    jmp short 03ca6h                          ; eb 0a                       ; 0xc3c9a
    test AL, strict byte 080h                 ; a8 80                       ; 0xc3c9c vbe.c:227
    jne short 03c98h                          ; 75 f8                       ; 0xc3c9e
    mov bx, dx                                ; 89 d3                       ; 0xc3ca0 vbe.c:230
    jmp short 03c77h                          ; eb d3                       ; 0xc3ca2 vbe.c:235
    xor ax, ax                                ; 31 c0                       ; 0xc3ca4 vbe.c:238
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc3ca6 vbe.c:239
    pop di                                    ; 5f                          ; 0xc3ca9
    pop si                                    ; 5e                          ; 0xc3caa
    pop cx                                    ; 59                          ; 0xc3cab
    pop bx                                    ; 5b                          ; 0xc3cac
    pop bp                                    ; 5d                          ; 0xc3cad
    retn                                      ; c3                          ; 0xc3cae
  ; disGetNextSymbol 0xc3caf LB 0x5fa -> off=0x0 cb=000000000000012b uValue=00000000000c3caf 'vbe_biosfn_return_controller_information'
vbe_biosfn_return_controller_information: ; 0xc3caf LB 0x12b
    push bp                                   ; 55                          ; 0xc3caf vbe.c:270
    mov bp, sp                                ; 89 e5                       ; 0xc3cb0
    push cx                                   ; 51                          ; 0xc3cb2
    push si                                   ; 56                          ; 0xc3cb3
    push di                                   ; 57                          ; 0xc3cb4
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc3cb5
    mov si, ax                                ; 89 c6                       ; 0xc3cb8
    mov word [bp-00ah], dx                    ; 89 56 f6                    ; 0xc3cba
    mov di, bx                                ; 89 df                       ; 0xc3cbd
    mov word [bp-00ch], strict word 00022h    ; c7 46 f4 22 00              ; 0xc3cbf vbe.c:275
    call 005dah                               ; e8 13 c9                    ; 0xc3cc4 vbe.c:278
    mov word [bp-010h], ax                    ; 89 46 f0                    ; 0xc3cc7
    mov bx, di                                ; 89 fb                       ; 0xc3cca vbe.c:281
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc3ccc
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc3ccf
    xor dx, dx                                ; 31 d2                       ; 0xc3cd2 vbe.c:284
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3cd4
    call 03bdch                               ; e8 02 ff                    ; 0xc3cd7
    cmp ax, 077cch                            ; 3d cc 77                    ; 0xc3cda vbe.c:285
    je short 03ce9h                           ; 74 0a                       ; 0xc3cdd
    push SS                                   ; 16                          ; 0xc3cdf vbe.c:287
    pop ES                                    ; 07                          ; 0xc3ce0
    mov word [es:si], 00100h                  ; 26 c7 04 00 01              ; 0xc3ce1
    jmp near 03dd2h                           ; e9 e9 00                    ; 0xc3ce6 vbe.c:291
    mov cx, strict word 00004h                ; b9 04 00                    ; 0xc3ce9 vbe.c:293
    mov word [bp-00eh], strict word 00000h    ; c7 46 f2 00 00              ; 0xc3cec vbe.c:300
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc3cf1 vbe.c:308
    cmp word [es:bx+002h], 03245h             ; 26 81 7f 02 45 32           ; 0xc3cf4
    jne short 03d03h                          ; 75 07                       ; 0xc3cfa
    cmp word [es:bx], 04256h                  ; 26 81 3f 56 42              ; 0xc3cfc
    je short 03d12h                           ; 74 0f                       ; 0xc3d01
    cmp word [es:bx+002h], 04153h             ; 26 81 7f 02 53 41           ; 0xc3d03
    jne short 03d17h                          ; 75 0c                       ; 0xc3d09
    cmp word [es:bx], 04556h                  ; 26 81 3f 56 45              ; 0xc3d0b
    jne short 03d17h                          ; 75 05                       ; 0xc3d10
    mov word [bp-00eh], strict word 00001h    ; c7 46 f2 01 00              ; 0xc3d12 vbe.c:310
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc3d17 vbe.c:318
    mov word [es:bx], 04556h                  ; 26 c7 07 56 45              ; 0xc3d1a
    mov word [es:bx+002h], 04153h             ; 26 c7 47 02 53 41           ; 0xc3d1f vbe.c:320
    mov word [es:bx+004h], 00200h             ; 26 c7 47 04 00 02           ; 0xc3d25 vbe.c:324
    mov word [es:bx+006h], 07de6h             ; 26 c7 47 06 e6 7d           ; 0xc3d2b vbe.c:327
    mov [es:bx+008h], ds                      ; 26 8c 5f 08                 ; 0xc3d31
    mov word [es:bx+00ah], strict word 00001h ; 26 c7 47 0a 01 00           ; 0xc3d35 vbe.c:330
    mov word [es:bx+00ch], strict word 00000h ; 26 c7 47 0c 00 00           ; 0xc3d3b vbe.c:332
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc3d41 vbe.c:336
    mov word [es:bx+010h], ax                 ; 26 89 47 10                 ; 0xc3d44
    lea ax, [di+022h]                         ; 8d 45 22                    ; 0xc3d48 vbe.c:337
    mov word [es:bx+00eh], ax                 ; 26 89 47 0e                 ; 0xc3d4b
    mov dx, strict word 0ffffh                ; ba ff ff                    ; 0xc3d4f vbe.c:340
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3d52
    call 03bdch                               ; e8 84 fe                    ; 0xc3d55
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc3d58
    mov word [es:bx+012h], ax                 ; 26 89 47 12                 ; 0xc3d5b
    cmp word [bp-00eh], strict byte 00000h    ; 83 7e f2 00                 ; 0xc3d5f vbe.c:342
    je short 03d89h                           ; 74 24                       ; 0xc3d63
    mov word [es:bx+014h], strict word 00003h ; 26 c7 47 14 03 00           ; 0xc3d65 vbe.c:345
    mov word [es:bx+016h], 07dfbh             ; 26 c7 47 16 fb 7d           ; 0xc3d6b vbe.c:346
    mov [es:bx+018h], ds                      ; 26 8c 5f 18                 ; 0xc3d71
    mov word [es:bx+01ah], 07e0eh             ; 26 c7 47 1a 0e 7e           ; 0xc3d75 vbe.c:347
    mov [es:bx+01ch], ds                      ; 26 8c 5f 1c                 ; 0xc3d7b
    mov word [es:bx+01eh], 07e2fh             ; 26 c7 47 1e 2f 7e           ; 0xc3d7f vbe.c:348
    mov [es:bx+020h], ds                      ; 26 8c 5f 20                 ; 0xc3d85
    mov dx, cx                                ; 89 ca                       ; 0xc3d89 vbe.c:355
    add dx, strict byte 0001bh                ; 83 c2 1b                    ; 0xc3d8b
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3d8e
    call 03beeh                               ; e8 5a fe                    ; 0xc3d91
    xor ah, ah                                ; 30 e4                       ; 0xc3d94 vbe.c:356
    cmp ax, word [bp-010h]                    ; 3b 46 f0                    ; 0xc3d96
    jnbe short 03db2h                         ; 77 17                       ; 0xc3d99
    mov dx, cx                                ; 89 ca                       ; 0xc3d9b vbe.c:358
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3d9d
    call 03bdch                               ; e8 39 fe                    ; 0xc3da0
    mov bx, word [bp-00ch]                    ; 8b 5e f4                    ; 0xc3da3 vbe.c:362
    add bx, di                                ; 01 fb                       ; 0xc3da6
    mov es, [bp-00ah]                         ; 8e 46 f6                    ; 0xc3da8 vbe.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3dab
    add word [bp-00ch], strict byte 00002h    ; 83 46 f4 02                 ; 0xc3dae vbe.c:364
    add cx, strict byte 00044h                ; 83 c1 44                    ; 0xc3db2 vbe.c:366
    mov dx, cx                                ; 89 ca                       ; 0xc3db5 vbe.c:367
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3db7
    call 03bdch                               ; e8 1f fe                    ; 0xc3dba
    cmp ax, strict word 0ffffh                ; 3d ff ff                    ; 0xc3dbd vbe.c:368
    jne short 03d89h                          ; 75 c7                       ; 0xc3dc0
    add di, word [bp-00ch]                    ; 03 7e f4                    ; 0xc3dc2 vbe.c:371
    mov es, [bp-00ah]                         ; 8e 46 f6                    ; 0xc3dc5 vbe.c:52
    mov word [es:di], ax                      ; 26 89 05                    ; 0xc3dc8
    push SS                                   ; 16                          ; 0xc3dcb vbe.c:372
    pop ES                                    ; 07                          ; 0xc3dcc
    mov word [es:si], strict word 0004fh      ; 26 c7 04 4f 00              ; 0xc3dcd
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc3dd2 vbe.c:373
    pop di                                    ; 5f                          ; 0xc3dd5
    pop si                                    ; 5e                          ; 0xc3dd6
    pop cx                                    ; 59                          ; 0xc3dd7
    pop bp                                    ; 5d                          ; 0xc3dd8
    retn                                      ; c3                          ; 0xc3dd9
  ; disGetNextSymbol 0xc3dda LB 0x4cf -> off=0x0 cb=00000000000000a0 uValue=00000000000c3dda 'vbe_biosfn_return_mode_information'
vbe_biosfn_return_mode_information:          ; 0xc3dda LB 0xa0
    push bp                                   ; 55                          ; 0xc3dda vbe.c:385
    mov bp, sp                                ; 89 e5                       ; 0xc3ddb
    push si                                   ; 56                          ; 0xc3ddd
    push di                                   ; 57                          ; 0xc3dde
    push ax                                   ; 50                          ; 0xc3ddf
    push ax                                   ; 50                          ; 0xc3de0
    mov ax, dx                                ; 89 d0                       ; 0xc3de1
    mov si, bx                                ; 89 de                       ; 0xc3de3
    mov bx, cx                                ; 89 cb                       ; 0xc3de5
    test dh, 040h                             ; f6 c6 40                    ; 0xc3de7 vbe.c:396
    je short 03df1h                           ; 74 05                       ; 0xc3dea
    mov dx, strict word 00001h                ; ba 01 00                    ; 0xc3dec
    jmp short 03df3h                          ; eb 02                       ; 0xc3def
    xor dx, dx                                ; 31 d2                       ; 0xc3df1
    and ah, 001h                              ; 80 e4 01                    ; 0xc3df3 vbe.c:397
    call 03c5ah                               ; e8 61 fe                    ; 0xc3df6 vbe.c:399
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc3df9
    test ax, ax                               ; 85 c0                       ; 0xc3dfc vbe.c:401
    je short 03e68h                           ; 74 68                       ; 0xc3dfe
    mov cx, 00100h                            ; b9 00 01                    ; 0xc3e00 vbe.c:406
    xor ax, ax                                ; 31 c0                       ; 0xc3e03
    mov di, bx                                ; 89 df                       ; 0xc3e05
    mov es, si                                ; 8e c6                       ; 0xc3e07
    cld                                       ; fc                          ; 0xc3e09
    jcxz 03e0eh                               ; e3 02                       ; 0xc3e0a
    rep stosb                                 ; f3 aa                       ; 0xc3e0c
    xor cx, cx                                ; 31 c9                       ; 0xc3e0e vbe.c:407
    jmp short 03e17h                          ; eb 05                       ; 0xc3e10
    cmp cx, strict byte 00042h                ; 83 f9 42                    ; 0xc3e12
    jnc short 03e30h                          ; 73 19                       ; 0xc3e15
    mov dx, word [bp-006h]                    ; 8b 56 fa                    ; 0xc3e17 vbe.c:410
    inc dx                                    ; 42                          ; 0xc3e1a
    inc dx                                    ; 42                          ; 0xc3e1b
    add dx, cx                                ; 01 ca                       ; 0xc3e1c
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3e1e
    call 03beeh                               ; e8 ca fd                    ; 0xc3e21
    mov di, bx                                ; 89 df                       ; 0xc3e24 vbe.c:411
    add di, cx                                ; 01 cf                       ; 0xc3e26
    mov es, si                                ; 8e c6                       ; 0xc3e28 vbe.c:42
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc3e2a
    inc cx                                    ; 41                          ; 0xc3e2d vbe.c:412
    jmp short 03e12h                          ; eb e2                       ; 0xc3e2e
    lea di, [bx+002h]                         ; 8d 7f 02                    ; 0xc3e30 vbe.c:413
    mov es, si                                ; 8e c6                       ; 0xc3e33 vbe.c:37
    mov al, byte [es:di]                      ; 26 8a 05                    ; 0xc3e35
    test AL, strict byte 001h                 ; a8 01                       ; 0xc3e38 vbe.c:414
    je short 03e4ch                           ; 74 10                       ; 0xc3e3a
    lea di, [bx+00ch]                         ; 8d 7f 0c                    ; 0xc3e3c vbe.c:415
    mov word [es:di], 0064ch                  ; 26 c7 05 4c 06              ; 0xc3e3f vbe.c:52
    lea di, [bx+00eh]                         ; 8d 7f 0e                    ; 0xc3e44 vbe.c:417
    mov word [es:di], 0c000h                  ; 26 c7 05 00 c0              ; 0xc3e47 vbe.c:52
    mov ax, strict word 0000bh                ; b8 0b 00                    ; 0xc3e4c vbe.c:420
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3e4f
    call 00590h                               ; e8 3b c7                    ; 0xc3e52
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3e55 vbe.c:421
    call 00597h                               ; e8 3c c7                    ; 0xc3e58
    add bx, strict byte 0002ah                ; 83 c3 2a                    ; 0xc3e5b
    mov es, si                                ; 8e c6                       ; 0xc3e5e vbe.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3e60
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc3e63 vbe.c:423
    jmp short 03e6bh                          ; eb 03                       ; 0xc3e66 vbe.c:424
    mov ax, 00100h                            ; b8 00 01                    ; 0xc3e68 vbe.c:428
    push SS                                   ; 16                          ; 0xc3e6b vbe.c:431
    pop ES                                    ; 07                          ; 0xc3e6c
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc3e6d
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3e70
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3e73 vbe.c:432
    pop di                                    ; 5f                          ; 0xc3e76
    pop si                                    ; 5e                          ; 0xc3e77
    pop bp                                    ; 5d                          ; 0xc3e78
    retn                                      ; c3                          ; 0xc3e79
  ; disGetNextSymbol 0xc3e7a LB 0x42f -> off=0x0 cb=00000000000000e7 uValue=00000000000c3e7a 'vbe_biosfn_set_mode'
vbe_biosfn_set_mode:                         ; 0xc3e7a LB 0xe7
    push bp                                   ; 55                          ; 0xc3e7a vbe.c:444
    mov bp, sp                                ; 89 e5                       ; 0xc3e7b
    push si                                   ; 56                          ; 0xc3e7d
    push di                                   ; 57                          ; 0xc3e7e
    sub sp, strict byte 00006h                ; 83 ec 06                    ; 0xc3e7f
    mov si, ax                                ; 89 c6                       ; 0xc3e82
    mov word [bp-00ah], dx                    ; 89 56 f6                    ; 0xc3e84
    test byte [bp-009h], 040h                 ; f6 46 f7 40                 ; 0xc3e87 vbe.c:452
    je short 03e92h                           ; 74 05                       ; 0xc3e8b
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc3e8d
    jmp short 03e94h                          ; eb 02                       ; 0xc3e90
    xor ax, ax                                ; 31 c0                       ; 0xc3e92
    mov dx, ax                                ; 89 c2                       ; 0xc3e94
    test ax, ax                               ; 85 c0                       ; 0xc3e96 vbe.c:453
    je short 03e9dh                           ; 74 03                       ; 0xc3e98
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3e9a
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc3e9d
    test byte [bp-009h], 080h                 ; f6 46 f7 80                 ; 0xc3ea0 vbe.c:454
    je short 03eabh                           ; 74 05                       ; 0xc3ea4
    mov ax, 00080h                            ; b8 80 00                    ; 0xc3ea6
    jmp short 03eadh                          ; eb 02                       ; 0xc3ea9
    xor ax, ax                                ; 31 c0                       ; 0xc3eab
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc3ead
    and byte [bp-009h], 001h                  ; 80 66 f7 01                 ; 0xc3eb0 vbe.c:456
    cmp word [bp-00ah], 00100h                ; 81 7e f6 00 01              ; 0xc3eb4 vbe.c:459
    jnc short 03eceh                          ; 73 13                       ; 0xc3eb9
    xor ax, ax                                ; 31 c0                       ; 0xc3ebb vbe.c:463
    call 00600h                               ; e8 40 c7                    ; 0xc3ebd
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc3ec0 vbe.c:467
    xor ah, ah                                ; 30 e4                       ; 0xc3ec3
    call 013aeh                               ; e8 e6 d4                    ; 0xc3ec5
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc3ec8 vbe.c:468
    jmp near 03f55h                           ; e9 87 00                    ; 0xc3ecb vbe.c:469
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc3ece vbe.c:472
    call 03c5ah                               ; e8 86 fd                    ; 0xc3ed1
    mov bx, ax                                ; 89 c3                       ; 0xc3ed4
    test ax, ax                               ; 85 c0                       ; 0xc3ed6 vbe.c:474
    je short 03f52h                           ; 74 78                       ; 0xc3ed8
    lea dx, [bx+014h]                         ; 8d 57 14                    ; 0xc3eda vbe.c:479
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3edd
    call 03bdch                               ; e8 f9 fc                    ; 0xc3ee0
    mov cx, ax                                ; 89 c1                       ; 0xc3ee3
    lea dx, [bx+016h]                         ; 8d 57 16                    ; 0xc3ee5 vbe.c:480
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3ee8
    call 03bdch                               ; e8 ee fc                    ; 0xc3eeb
    mov di, ax                                ; 89 c7                       ; 0xc3eee
    lea dx, [bx+01bh]                         ; 8d 57 1b                    ; 0xc3ef0 vbe.c:481
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3ef3
    call 03beeh                               ; e8 f5 fc                    ; 0xc3ef6
    mov bl, al                                ; 88 c3                       ; 0xc3ef9
    mov dl, al                                ; 88 c2                       ; 0xc3efb
    xor ax, ax                                ; 31 c0                       ; 0xc3efd vbe.c:489
    call 00600h                               ; e8 fe c6                    ; 0xc3eff
    cmp bl, 004h                              ; 80 fb 04                    ; 0xc3f02 vbe.c:491
    jne short 03f0dh                          ; 75 06                       ; 0xc3f05
    mov ax, strict word 0006ah                ; b8 6a 00                    ; 0xc3f07 vbe.c:493
    call 013aeh                               ; e8 a1 d4                    ; 0xc3f0a
    mov al, dl                                ; 88 d0                       ; 0xc3f0d vbe.c:496
    xor ah, ah                                ; 30 e4                       ; 0xc3f0f
    call 03b53h                               ; e8 3f fc                    ; 0xc3f11
    mov ax, cx                                ; 89 c8                       ; 0xc3f14 vbe.c:497
    call 03afch                               ; e8 e3 fb                    ; 0xc3f16
    mov ax, di                                ; 89 f8                       ; 0xc3f19 vbe.c:498
    call 03b1bh                               ; e8 fd fb                    ; 0xc3f1b
    xor ax, ax                                ; 31 c0                       ; 0xc3f1e vbe.c:499
    call 00626h                               ; e8 03 c7                    ; 0xc3f20
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc3f23 vbe.c:500
    or dl, 001h                               ; 80 ca 01                    ; 0xc3f26
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc3f29
    xor ah, ah                                ; 30 e4                       ; 0xc3f2c
    or al, dl                                 ; 08 d0                       ; 0xc3f2e
    call 00600h                               ; e8 cd c6                    ; 0xc3f30
    call 006f8h                               ; e8 c2 c7                    ; 0xc3f33 vbe.c:501
    mov bx, 000bah                            ; bb ba 00                    ; 0xc3f36 vbe.c:52
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3f39
    mov es, ax                                ; 8e c0                       ; 0xc3f3c
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc3f3e
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3f41
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc3f44 vbe.c:504
    or AL, strict byte 060h                   ; 0c 60                       ; 0xc3f47
    mov bx, 00087h                            ; bb 87 00                    ; 0xc3f49 vbe.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3f4c
    jmp near 03ec8h                           ; e9 76 ff                    ; 0xc3f4f
    mov ax, 00100h                            ; b8 00 01                    ; 0xc3f52 vbe.c:513
    push SS                                   ; 16                          ; 0xc3f55 vbe.c:517
    pop ES                                    ; 07                          ; 0xc3f56
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3f57
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3f5a vbe.c:518
    pop di                                    ; 5f                          ; 0xc3f5d
    pop si                                    ; 5e                          ; 0xc3f5e
    pop bp                                    ; 5d                          ; 0xc3f5f
    retn                                      ; c3                          ; 0xc3f60
  ; disGetNextSymbol 0xc3f61 LB 0x348 -> off=0x0 cb=0000000000000008 uValue=00000000000c3f61 'vbe_biosfn_read_video_state_size'
vbe_biosfn_read_video_state_size:            ; 0xc3f61 LB 0x8
    push bp                                   ; 55                          ; 0xc3f61 vbe.c:520
    mov bp, sp                                ; 89 e5                       ; 0xc3f62
    mov ax, strict word 00012h                ; b8 12 00                    ; 0xc3f64 vbe.c:523
    pop bp                                    ; 5d                          ; 0xc3f67
    retn                                      ; c3                          ; 0xc3f68
  ; disGetNextSymbol 0xc3f69 LB 0x340 -> off=0x0 cb=000000000000004b uValue=00000000000c3f69 'vbe_biosfn_save_video_state'
vbe_biosfn_save_video_state:                 ; 0xc3f69 LB 0x4b
    push bp                                   ; 55                          ; 0xc3f69 vbe.c:525
    mov bp, sp                                ; 89 e5                       ; 0xc3f6a
    push bx                                   ; 53                          ; 0xc3f6c
    push cx                                   ; 51                          ; 0xc3f6d
    push si                                   ; 56                          ; 0xc3f6e
    mov si, ax                                ; 89 c6                       ; 0xc3f6f
    mov bx, dx                                ; 89 d3                       ; 0xc3f71
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc3f73 vbe.c:529
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3f76
    out DX, ax                                ; ef                          ; 0xc3f79
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3f7a vbe.c:530
    in ax, DX                                 ; ed                          ; 0xc3f7d
    mov es, si                                ; 8e c6                       ; 0xc3f7e vbe.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3f80
    inc bx                                    ; 43                          ; 0xc3f83 vbe.c:532
    inc bx                                    ; 43                          ; 0xc3f84
    test AL, strict byte 001h                 ; a8 01                       ; 0xc3f85 vbe.c:533
    je short 03fach                           ; 74 23                       ; 0xc3f87
    mov cx, strict word 00001h                ; b9 01 00                    ; 0xc3f89 vbe.c:535
    jmp short 03f93h                          ; eb 05                       ; 0xc3f8c
    cmp cx, strict byte 00009h                ; 83 f9 09                    ; 0xc3f8e
    jnbe short 03fach                         ; 77 19                       ; 0xc3f91
    cmp cx, strict byte 00004h                ; 83 f9 04                    ; 0xc3f93 vbe.c:536
    je short 03fa9h                           ; 74 11                       ; 0xc3f96
    mov ax, cx                                ; 89 c8                       ; 0xc3f98 vbe.c:537
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3f9a
    out DX, ax                                ; ef                          ; 0xc3f9d
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3f9e vbe.c:538
    in ax, DX                                 ; ed                          ; 0xc3fa1
    mov es, si                                ; 8e c6                       ; 0xc3fa2 vbe.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3fa4
    inc bx                                    ; 43                          ; 0xc3fa7 vbe.c:539
    inc bx                                    ; 43                          ; 0xc3fa8
    inc cx                                    ; 41                          ; 0xc3fa9 vbe.c:541
    jmp short 03f8eh                          ; eb e2                       ; 0xc3faa
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc3fac vbe.c:542
    pop si                                    ; 5e                          ; 0xc3faf
    pop cx                                    ; 59                          ; 0xc3fb0
    pop bx                                    ; 5b                          ; 0xc3fb1
    pop bp                                    ; 5d                          ; 0xc3fb2
    retn                                      ; c3                          ; 0xc3fb3
  ; disGetNextSymbol 0xc3fb4 LB 0x2f5 -> off=0x0 cb=000000000000008f uValue=00000000000c3fb4 'vbe_biosfn_restore_video_state'
vbe_biosfn_restore_video_state:              ; 0xc3fb4 LB 0x8f
    push bp                                   ; 55                          ; 0xc3fb4 vbe.c:545
    mov bp, sp                                ; 89 e5                       ; 0xc3fb5
    push bx                                   ; 53                          ; 0xc3fb7
    push cx                                   ; 51                          ; 0xc3fb8
    push si                                   ; 56                          ; 0xc3fb9
    push ax                                   ; 50                          ; 0xc3fba
    mov cx, ax                                ; 89 c1                       ; 0xc3fbb
    mov bx, dx                                ; 89 d3                       ; 0xc3fbd
    mov es, ax                                ; 8e c0                       ; 0xc3fbf vbe.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3fc1
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc3fc4
    inc bx                                    ; 43                          ; 0xc3fc7 vbe.c:550
    inc bx                                    ; 43                          ; 0xc3fc8
    test byte [bp-008h], 001h                 ; f6 46 f8 01                 ; 0xc3fc9 vbe.c:552
    jne short 03fdfh                          ; 75 10                       ; 0xc3fcd
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc3fcf vbe.c:553
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3fd2
    out DX, ax                                ; ef                          ; 0xc3fd5
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc3fd6 vbe.c:554
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3fd9
    out DX, ax                                ; ef                          ; 0xc3fdc
    jmp short 0403bh                          ; eb 5c                       ; 0xc3fdd vbe.c:555
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc3fdf vbe.c:556
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3fe2
    out DX, ax                                ; ef                          ; 0xc3fe5
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3fe6 vbe.c:47
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3fe9 vbe.c:48
    out DX, ax                                ; ef                          ; 0xc3fec
    inc bx                                    ; 43                          ; 0xc3fed vbe.c:558
    inc bx                                    ; 43                          ; 0xc3fee
    mov ax, strict word 00002h                ; b8 02 00                    ; 0xc3fef
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3ff2
    out DX, ax                                ; ef                          ; 0xc3ff5
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3ff6 vbe.c:47
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3ff9 vbe.c:48
    out DX, ax                                ; ef                          ; 0xc3ffc
    inc bx                                    ; 43                          ; 0xc3ffd vbe.c:561
    inc bx                                    ; 43                          ; 0xc3ffe
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc3fff
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc4002
    out DX, ax                                ; ef                          ; 0xc4005
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc4006 vbe.c:47
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc4009 vbe.c:48
    out DX, ax                                ; ef                          ; 0xc400c
    inc bx                                    ; 43                          ; 0xc400d vbe.c:564
    inc bx                                    ; 43                          ; 0xc400e
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc400f
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc4012
    out DX, ax                                ; ef                          ; 0xc4015
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc4016 vbe.c:566
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc4019
    out DX, ax                                ; ef                          ; 0xc401c
    mov si, strict word 00005h                ; be 05 00                    ; 0xc401d vbe.c:568
    jmp short 04027h                          ; eb 05                       ; 0xc4020
    cmp si, strict byte 00009h                ; 83 fe 09                    ; 0xc4022
    jnbe short 0403bh                         ; 77 14                       ; 0xc4025
    mov ax, si                                ; 89 f0                       ; 0xc4027 vbe.c:569
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc4029
    out DX, ax                                ; ef                          ; 0xc402c
    mov es, cx                                ; 8e c1                       ; 0xc402d vbe.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc402f
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc4032 vbe.c:48
    out DX, ax                                ; ef                          ; 0xc4035
    inc bx                                    ; 43                          ; 0xc4036 vbe.c:571
    inc bx                                    ; 43                          ; 0xc4037
    inc si                                    ; 46                          ; 0xc4038 vbe.c:572
    jmp short 04022h                          ; eb e7                       ; 0xc4039
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc403b vbe.c:574
    pop si                                    ; 5e                          ; 0xc403e
    pop cx                                    ; 59                          ; 0xc403f
    pop bx                                    ; 5b                          ; 0xc4040
    pop bp                                    ; 5d                          ; 0xc4041
    retn                                      ; c3                          ; 0xc4042
  ; disGetNextSymbol 0xc4043 LB 0x266 -> off=0x0 cb=000000000000008d uValue=00000000000c4043 'vbe_biosfn_save_restore_state'
vbe_biosfn_save_restore_state:               ; 0xc4043 LB 0x8d
    push bp                                   ; 55                          ; 0xc4043 vbe.c:590
    mov bp, sp                                ; 89 e5                       ; 0xc4044
    push si                                   ; 56                          ; 0xc4046
    push di                                   ; 57                          ; 0xc4047
    push ax                                   ; 50                          ; 0xc4048
    mov si, ax                                ; 89 c6                       ; 0xc4049
    mov word [bp-006h], dx                    ; 89 56 fa                    ; 0xc404b
    mov ax, bx                                ; 89 d8                       ; 0xc404e
    mov bx, word [bp+004h]                    ; 8b 5e 04                    ; 0xc4050
    mov di, strict word 0004fh                ; bf 4f 00                    ; 0xc4053 vbe.c:595
    xor ah, ah                                ; 30 e4                       ; 0xc4056 vbe.c:596
    cmp ax, strict word 00002h                ; 3d 02 00                    ; 0xc4058
    je short 040a3h                           ; 74 46                       ; 0xc405b
    cmp ax, strict word 00001h                ; 3d 01 00                    ; 0xc405d
    je short 04087h                           ; 74 25                       ; 0xc4060
    test ax, ax                               ; 85 c0                       ; 0xc4062
    jne short 040bfh                          ; 75 59                       ; 0xc4064
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc4066 vbe.c:598
    call 03081h                               ; e8 15 f0                    ; 0xc4069
    mov cx, ax                                ; 89 c1                       ; 0xc406c
    test byte [bp-006h], 008h                 ; f6 46 fa 08                 ; 0xc406e vbe.c:602
    je short 04079h                           ; 74 05                       ; 0xc4072
    call 03f61h                               ; e8 ea fe                    ; 0xc4074 vbe.c:603
    add ax, cx                                ; 01 c8                       ; 0xc4077
    add ax, strict word 0003fh                ; 05 3f 00                    ; 0xc4079 vbe.c:604
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc407c
    shr ax, CL                                ; d3 e8                       ; 0xc407e
    push SS                                   ; 16                          ; 0xc4080
    pop ES                                    ; 07                          ; 0xc4081
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc4082
    jmp short 040c2h                          ; eb 3b                       ; 0xc4085 vbe.c:605
    push SS                                   ; 16                          ; 0xc4087 vbe.c:607
    pop ES                                    ; 07                          ; 0xc4088
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc4089
    mov dx, cx                                ; 89 ca                       ; 0xc408c vbe.c:608
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc408e
    call 030bfh                               ; e8 2b f0                    ; 0xc4091
    test byte [bp-006h], 008h                 ; f6 46 fa 08                 ; 0xc4094 vbe.c:612
    je short 040c2h                           ; 74 28                       ; 0xc4098
    mov dx, ax                                ; 89 c2                       ; 0xc409a vbe.c:613
    mov ax, cx                                ; 89 c8                       ; 0xc409c
    call 03f69h                               ; e8 c8 fe                    ; 0xc409e
    jmp short 040c2h                          ; eb 1f                       ; 0xc40a1 vbe.c:614
    push SS                                   ; 16                          ; 0xc40a3 vbe.c:616
    pop ES                                    ; 07                          ; 0xc40a4
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc40a5
    mov dx, cx                                ; 89 ca                       ; 0xc40a8 vbe.c:617
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc40aa
    call 03397h                               ; e8 e7 f2                    ; 0xc40ad
    test byte [bp-006h], 008h                 ; f6 46 fa 08                 ; 0xc40b0 vbe.c:621
    je short 040c2h                           ; 74 0c                       ; 0xc40b4
    mov dx, ax                                ; 89 c2                       ; 0xc40b6 vbe.c:622
    mov ax, cx                                ; 89 c8                       ; 0xc40b8
    call 03fb4h                               ; e8 f7 fe                    ; 0xc40ba
    jmp short 040c2h                          ; eb 03                       ; 0xc40bd vbe.c:623
    mov di, 00100h                            ; bf 00 01                    ; 0xc40bf vbe.c:626
    push SS                                   ; 16                          ; 0xc40c2 vbe.c:629
    pop ES                                    ; 07                          ; 0xc40c3
    mov word [es:si], di                      ; 26 89 3c                    ; 0xc40c4
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc40c7 vbe.c:630
    pop di                                    ; 5f                          ; 0xc40ca
    pop si                                    ; 5e                          ; 0xc40cb
    pop bp                                    ; 5d                          ; 0xc40cc
    retn 00002h                               ; c2 02 00                    ; 0xc40cd
  ; disGetNextSymbol 0xc40d0 LB 0x1d9 -> off=0x0 cb=00000000000000e2 uValue=00000000000c40d0 'vbe_biosfn_get_set_scanline_length'
vbe_biosfn_get_set_scanline_length:          ; 0xc40d0 LB 0xe2
    push bp                                   ; 55                          ; 0xc40d0 vbe.c:651
    mov bp, sp                                ; 89 e5                       ; 0xc40d1
    push si                                   ; 56                          ; 0xc40d3
    push di                                   ; 57                          ; 0xc40d4
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc40d5
    push ax                                   ; 50                          ; 0xc40d8
    mov di, dx                                ; 89 d7                       ; 0xc40d9
    mov word [bp-006h], bx                    ; 89 5e fa                    ; 0xc40db
    mov si, cx                                ; 89 ce                       ; 0xc40de
    call 03b72h                               ; e8 8f fa                    ; 0xc40e0 vbe.c:660
    cmp AL, strict byte 00fh                  ; 3c 0f                       ; 0xc40e3 vbe.c:661
    jne short 040ech                          ; 75 05                       ; 0xc40e5
    mov cx, strict word 00010h                ; b9 10 00                    ; 0xc40e7
    jmp short 040f0h                          ; eb 04                       ; 0xc40ea
    xor ah, ah                                ; 30 e4                       ; 0xc40ec
    mov cx, ax                                ; 89 c1                       ; 0xc40ee
    mov ch, cl                                ; 88 cd                       ; 0xc40f0
    call 03baah                               ; e8 b5 fa                    ; 0xc40f2 vbe.c:662
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc40f5
    mov word [bp-00ch], strict word 0004fh    ; c7 46 f4 4f 00              ; 0xc40f8 vbe.c:663
    push SS                                   ; 16                          ; 0xc40fd vbe.c:664
    pop ES                                    ; 07                          ; 0xc40fe
    mov bx, word [bp-006h]                    ; 8b 5e fa                    ; 0xc40ff
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc4102
    mov al, byte [es:di]                      ; 26 8a 05                    ; 0xc4105 vbe.c:665
    cmp AL, strict byte 002h                  ; 3c 02                       ; 0xc4108 vbe.c:669
    je short 04117h                           ; 74 0b                       ; 0xc410a
    cmp AL, strict byte 001h                  ; 3c 01                       ; 0xc410c
    je short 04140h                           ; 74 30                       ; 0xc410e
    test al, al                               ; 84 c0                       ; 0xc4110
    je short 0413bh                           ; 74 27                       ; 0xc4112
    jmp near 0419bh                           ; e9 84 00                    ; 0xc4114
    cmp ch, 004h                              ; 80 fd 04                    ; 0xc4117 vbe.c:671
    jne short 04122h                          ; 75 06                       ; 0xc411a
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc411c vbe.c:672
    sal bx, CL                                ; d3 e3                       ; 0xc411e
    jmp short 0413bh                          ; eb 19                       ; 0xc4120 vbe.c:673
    mov al, ch                                ; 88 e8                       ; 0xc4122 vbe.c:674
    xor ah, ah                                ; 30 e4                       ; 0xc4124
    cwd                                       ; 99                          ; 0xc4126
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc4127
    sal dx, CL                                ; d3 e2                       ; 0xc4129
    db  01bh, 0c2h
    ; sbb ax, dx                                ; 1b c2                     ; 0xc412b
    sar ax, CL                                ; d3 f8                       ; 0xc412d
    mov word [bp-00eh], ax                    ; 89 46 f2                    ; 0xc412f
    mov ax, bx                                ; 89 d8                       ; 0xc4132
    xor dx, dx                                ; 31 d2                       ; 0xc4134
    div word [bp-00eh]                        ; f7 76 f2                    ; 0xc4136
    mov bx, ax                                ; 89 c3                       ; 0xc4139
    mov ax, bx                                ; 89 d8                       ; 0xc413b vbe.c:677
    call 03b8bh                               ; e8 4b fa                    ; 0xc413d
    call 03baah                               ; e8 67 fa                    ; 0xc4140 vbe.c:680
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc4143
    push SS                                   ; 16                          ; 0xc4146 vbe.c:681
    pop ES                                    ; 07                          ; 0xc4147
    mov bx, word [bp-006h]                    ; 8b 5e fa                    ; 0xc4148
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc414b
    cmp ch, 004h                              ; 80 fd 04                    ; 0xc414e vbe.c:682
    jne short 0415bh                          ; 75 08                       ; 0xc4151
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc4153 vbe.c:683
    mov bx, ax                                ; 89 c3                       ; 0xc4155
    shr bx, CL                                ; d3 eb                       ; 0xc4157
    jmp short 04171h                          ; eb 16                       ; 0xc4159 vbe.c:684
    mov al, ch                                ; 88 e8                       ; 0xc415b vbe.c:685
    xor ah, ah                                ; 30 e4                       ; 0xc415d
    cwd                                       ; 99                          ; 0xc415f
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc4160
    sal dx, CL                                ; d3 e2                       ; 0xc4162
    db  01bh, 0c2h
    ; sbb ax, dx                                ; 1b c2                     ; 0xc4164
    sar ax, CL                                ; d3 f8                       ; 0xc4166
    mov bx, ax                                ; 89 c3                       ; 0xc4168
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc416a
    mul bx                                    ; f7 e3                       ; 0xc416d
    mov bx, ax                                ; 89 c3                       ; 0xc416f
    add bx, strict byte 00003h                ; 83 c3 03                    ; 0xc4171 vbe.c:686
    and bl, 0fch                              ; 80 e3 fc                    ; 0xc4174
    push SS                                   ; 16                          ; 0xc4177 vbe.c:687
    pop ES                                    ; 07                          ; 0xc4178
    mov word [es:di], bx                      ; 26 89 1d                    ; 0xc4179
    call 03bc3h                               ; e8 44 fa                    ; 0xc417c vbe.c:688
    push SS                                   ; 16                          ; 0xc417f
    pop ES                                    ; 07                          ; 0xc4180
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc4181
    call 03b3ah                               ; e8 b3 f9                    ; 0xc4184 vbe.c:689
    push SS                                   ; 16                          ; 0xc4187
    pop ES                                    ; 07                          ; 0xc4188
    cmp ax, word [es:si]                      ; 26 3b 04                    ; 0xc4189
    jbe short 041a0h                          ; 76 12                       ; 0xc418c
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc418e vbe.c:690
    call 03b8bh                               ; e8 f7 f9                    ; 0xc4191
    mov word [bp-00ch], 00200h                ; c7 46 f4 00 02              ; 0xc4194 vbe.c:691
    jmp short 041a0h                          ; eb 05                       ; 0xc4199 vbe.c:693
    mov word [bp-00ch], 00100h                ; c7 46 f4 00 01              ; 0xc419b vbe.c:696
    push SS                                   ; 16                          ; 0xc41a0 vbe.c:699
    pop ES                                    ; 07                          ; 0xc41a1
    mov ax, word [bp-00ch]                    ; 8b 46 f4                    ; 0xc41a2
    mov bx, word [bp-010h]                    ; 8b 5e f0                    ; 0xc41a5
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc41a8
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc41ab vbe.c:700
    pop di                                    ; 5f                          ; 0xc41ae
    pop si                                    ; 5e                          ; 0xc41af
    pop bp                                    ; 5d                          ; 0xc41b0
    retn                                      ; c3                          ; 0xc41b1
  ; disGetNextSymbol 0xc41b2 LB 0xf7 -> off=0x0 cb=00000000000000f7 uValue=00000000000c41b2 'private_biosfn_custom_mode'
private_biosfn_custom_mode:                  ; 0xc41b2 LB 0xf7
    push bp                                   ; 55                          ; 0xc41b2 vbe.c:726
    mov bp, sp                                ; 89 e5                       ; 0xc41b3
    push si                                   ; 56                          ; 0xc41b5
    push di                                   ; 57                          ; 0xc41b6
    sub sp, strict byte 00006h                ; 83 ec 06                    ; 0xc41b7
    push ax                                   ; 50                          ; 0xc41ba
    mov si, dx                                ; 89 d6                       ; 0xc41bb
    mov di, cx                                ; 89 cf                       ; 0xc41bd
    mov word [bp-00ah], strict word 0004fh    ; c7 46 f6 4f 00              ; 0xc41bf vbe.c:739
    push SS                                   ; 16                          ; 0xc41c4 vbe.c:740
    pop ES                                    ; 07                          ; 0xc41c5
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc41c6
    test al, al                               ; 84 c0                       ; 0xc41c9 vbe.c:741
    jne short 041edh                          ; 75 20                       ; 0xc41cb
    push SS                                   ; 16                          ; 0xc41cd vbe.c:743
    pop ES                                    ; 07                          ; 0xc41ce
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc41cf
    mov ax, word [es:di]                      ; 26 8b 05                    ; 0xc41d2 vbe.c:744
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc41d5
    mov al, byte [es:si+001h]                 ; 26 8a 44 01                 ; 0xc41d8 vbe.c:745
    and ax, strict word 0007fh                ; 25 7f 00                    ; 0xc41dc
    mov ch, al                                ; 88 c5                       ; 0xc41df
    cmp AL, strict byte 008h                  ; 3c 08                       ; 0xc41e1 vbe.c:750
    je short 041f5h                           ; 74 10                       ; 0xc41e3
    cmp AL, strict byte 010h                  ; 3c 10                       ; 0xc41e5
    je short 041f5h                           ; 74 0c                       ; 0xc41e7
    cmp AL, strict byte 020h                  ; 3c 20                       ; 0xc41e9
    je short 041f5h                           ; 74 08                       ; 0xc41eb
    mov word [bp-00ah], 00100h                ; c7 46 f6 00 01              ; 0xc41ed vbe.c:751
    jmp near 04297h                           ; e9 a2 00                    ; 0xc41f2 vbe.c:752
    push SS                                   ; 16                          ; 0xc41f5 vbe.c:756
    pop ES                                    ; 07                          ; 0xc41f6
    test byte [es:si+001h], 080h              ; 26 f6 44 01 80              ; 0xc41f7
    je short 04203h                           ; 74 05                       ; 0xc41fc
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc41fe
    jmp short 04205h                          ; eb 02                       ; 0xc4201
    xor ax, ax                                ; 31 c0                       ; 0xc4203
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc4205
    cmp bx, 00280h                            ; 81 fb 80 02                 ; 0xc4208 vbe.c:759
    jnc short 04213h                          ; 73 05                       ; 0xc420c
    mov bx, 00280h                            ; bb 80 02                    ; 0xc420e vbe.c:760
    jmp short 0421ch                          ; eb 09                       ; 0xc4211 vbe.c:761
    cmp bx, 00a00h                            ; 81 fb 00 0a                 ; 0xc4213
    jbe short 0421ch                          ; 76 03                       ; 0xc4217
    mov bx, 00a00h                            ; bb 00 0a                    ; 0xc4219 vbe.c:762
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc421c vbe.c:763
    cmp ax, 001e0h                            ; 3d e0 01                    ; 0xc421f
    jnc short 0422bh                          ; 73 07                       ; 0xc4222
    mov word [bp-008h], 001e0h                ; c7 46 f8 e0 01              ; 0xc4224 vbe.c:764
    jmp short 04235h                          ; eb 0a                       ; 0xc4229 vbe.c:765
    cmp ax, 00780h                            ; 3d 80 07                    ; 0xc422b
    jbe short 04235h                          ; 76 05                       ; 0xc422e
    mov word [bp-008h], 00780h                ; c7 46 f8 80 07              ; 0xc4230 vbe.c:766
    mov dx, strict word 0ffffh                ; ba ff ff                    ; 0xc4235 vbe.c:772
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc4238
    call 03bdch                               ; e8 9e f9                    ; 0xc423b
    mov si, ax                                ; 89 c6                       ; 0xc423e
    mov al, ch                                ; 88 e8                       ; 0xc4240 vbe.c:775
    xor ah, ah                                ; 30 e4                       ; 0xc4242
    cwd                                       ; 99                          ; 0xc4244
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc4245
    sal dx, CL                                ; d3 e2                       ; 0xc4247
    db  01bh, 0c2h
    ; sbb ax, dx                                ; 1b c2                     ; 0xc4249
    sar ax, CL                                ; d3 f8                       ; 0xc424b
    mov dx, ax                                ; 89 c2                       ; 0xc424d
    mov ax, bx                                ; 89 d8                       ; 0xc424f
    mul dx                                    ; f7 e2                       ; 0xc4251
    add ax, strict word 00003h                ; 05 03 00                    ; 0xc4253 vbe.c:776
    and AL, strict byte 0fch                  ; 24 fc                       ; 0xc4256
    mov dx, word [bp-008h]                    ; 8b 56 f8                    ; 0xc4258 vbe.c:778
    mul dx                                    ; f7 e2                       ; 0xc425b
    cmp dx, si                                ; 39 f2                       ; 0xc425d vbe.c:780
    jnbe short 04267h                         ; 77 06                       ; 0xc425f
    jne short 0426eh                          ; 75 0b                       ; 0xc4261
    test ax, ax                               ; 85 c0                       ; 0xc4263
    jbe short 0426eh                          ; 76 07                       ; 0xc4265
    mov word [bp-00ah], 00200h                ; c7 46 f6 00 02              ; 0xc4267 vbe.c:782
    jmp short 04297h                          ; eb 29                       ; 0xc426c vbe.c:783
    xor ax, ax                                ; 31 c0                       ; 0xc426e vbe.c:787
    call 00600h                               ; e8 8d c3                    ; 0xc4270
    mov al, ch                                ; 88 e8                       ; 0xc4273 vbe.c:788
    xor ah, ah                                ; 30 e4                       ; 0xc4275
    call 03b53h                               ; e8 d9 f8                    ; 0xc4277
    mov ax, bx                                ; 89 d8                       ; 0xc427a vbe.c:789
    call 03afch                               ; e8 7d f8                    ; 0xc427c
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc427f vbe.c:790
    call 03b1bh                               ; e8 96 f8                    ; 0xc4282
    xor ax, ax                                ; 31 c0                       ; 0xc4285 vbe.c:791
    call 00626h                               ; e8 9c c3                    ; 0xc4287
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc428a vbe.c:792
    or AL, strict byte 001h                   ; 0c 01                       ; 0xc428d
    xor ah, ah                                ; 30 e4                       ; 0xc428f
    call 00600h                               ; e8 6c c3                    ; 0xc4291
    call 006f8h                               ; e8 61 c4                    ; 0xc4294 vbe.c:793
    push SS                                   ; 16                          ; 0xc4297 vbe.c:801
    pop ES                                    ; 07                          ; 0xc4298
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc4299
    mov bx, word [bp-00ch]                    ; 8b 5e f4                    ; 0xc429c
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc429f
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc42a2 vbe.c:802
    pop di                                    ; 5f                          ; 0xc42a5
    pop si                                    ; 5e                          ; 0xc42a6
    pop bp                                    ; 5d                          ; 0xc42a7
    retn                                      ; c3                          ; 0xc42a8

  ; Padding 0x357 bytes at 0xc42a9
  times 855 db 0

section VBE32 progbits vstart=0x4600 align=1 ; size=0x115 class=CODE group=AUTO
  ; disGetNextSymbol 0xc4600 LB 0x115 -> off=0x0 cb=0000000000000114 uValue=00000000000c0000 'vesa_pm_start'
vesa_pm_start:                               ; 0xc4600 LB 0x114
    sbb byte [bx+si], al                      ; 18 00                       ; 0xc4600
    dec di                                    ; 4f                          ; 0xc4602
    add byte [bx+si], dl                      ; 00 10                       ; 0xc4603
    add word [bx+si], cx                      ; 01 08                       ; 0xc4605
    add dh, cl                                ; 00 ce                       ; 0xc4607
    add di, cx                                ; 01 cf                       ; 0xc4609
    add di, cx                                ; 01 cf                       ; 0xc460b
    add ax, dx                                ; 01 d0                       ; 0xc460d
    add word [bp-048fdh], si                  ; 01 b6 03 b7                 ; 0xc460f
    db  003h, 0ffh
    ; add di, di                                ; 03 ff                     ; 0xc4613
    db  0ffh
    db  0ffh
    jmp word [bp-07dh]                        ; ff 66 83                    ; 0xc4617
    sti                                       ; fb                          ; 0xc461a
    add byte [si+005h], dh                    ; 00 74 05                    ; 0xc461b
    mov eax, strict dword 066c30100h          ; 66 b8 00 01 c3 66           ; 0xc461e vberom.asm:825
    db  08bh, 0c2h
    ; mov ax, dx                                ; 8b c2                     ; 0xc4624
    push edx                                  ; 66 52                       ; 0xc4626 vberom.asm:829
    push eax                                  ; 66 50                       ; 0xc4628 vberom.asm:830
    mov edx, strict dword 0b86601ceh          ; 66 ba ce 01 66 b8           ; 0xc462a vberom.asm:831
    add ax, 06600h                            ; 05 00 66                    ; 0xc4630
    out DX, ax                                ; ef                          ; 0xc4633
    pop eax                                   ; 66 58                       ; 0xc4634 vberom.asm:834
    mov edx, strict dword 0ef6601cfh          ; 66 ba cf 01 66 ef           ; 0xc4636 vberom.asm:835
    in eax, DX                                ; 66 ed                       ; 0xc463c vberom.asm:837
    pop edx                                   ; 66 5a                       ; 0xc463e vberom.asm:838
    db  066h, 03bh, 0d0h
    ; cmp edx, eax                              ; 66 3b d0                  ; 0xc4640 vberom.asm:839
    jne short 0464ah                          ; 75 05                       ; 0xc4643 vberom.asm:840
    mov eax, strict dword 066c3004fh          ; 66 b8 4f 00 c3 66           ; 0xc4645 vberom.asm:841
    mov ax, 0014fh                            ; b8 4f 01                    ; 0xc464b
    retn                                      ; c3                          ; 0xc464e vberom.asm:845
    cmp bl, 080h                              ; 80 fb 80                    ; 0xc464f vberom.asm:847
    je short 0465eh                           ; 74 0a                       ; 0xc4652 vberom.asm:848
    cmp bl, 000h                              ; 80 fb 00                    ; 0xc4654 vberom.asm:849
    je short 0466eh                           ; 74 15                       ; 0xc4657 vberom.asm:850
    mov eax, strict dword 052c30100h          ; 66 b8 00 01 c3 52           ; 0xc4659 vberom.asm:851
    mov edx, strict dword 0a8ec03dah          ; 66 ba da 03 ec a8           ; 0xc465f vberom.asm:855
    or byte [di-005h], dh                     ; 08 75 fb                    ; 0xc4665
    in AL, DX                                 ; ec                          ; 0xc4668 vberom.asm:861
    test AL, strict byte 008h                 ; a8 08                       ; 0xc4669 vberom.asm:862
    je short 04668h                           ; 74 fb                       ; 0xc466b vberom.asm:863
    pop dx                                    ; 5a                          ; 0xc466d vberom.asm:864
    push ax                                   ; 50                          ; 0xc466e vberom.asm:868
    push cx                                   ; 51                          ; 0xc466f vberom.asm:869
    push dx                                   ; 52                          ; 0xc4670 vberom.asm:870
    push si                                   ; 56                          ; 0xc4671 vberom.asm:871
    push di                                   ; 57                          ; 0xc4672 vberom.asm:872
    sal dx, 010h                              ; c1 e2 10                    ; 0xc4673 vberom.asm:873
    and cx, strict word 0ffffh                ; 81 e1 ff ff                 ; 0xc4676 vberom.asm:874
    add byte [bx+si], al                      ; 00 00                       ; 0xc467a
    db  00bh, 0cah
    ; or cx, dx                                 ; 0b ca                     ; 0xc467c vberom.asm:875
    sal cx, 002h                              ; c1 e1 02                    ; 0xc467e vberom.asm:876
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1                     ; 0xc4681 vberom.asm:877
    push ax                                   ; 50                          ; 0xc4683 vberom.asm:878
    mov edx, strict dword 0b86601ceh          ; 66 ba ce 01 66 b8           ; 0xc4684 vberom.asm:879
    push ES                                   ; 06                          ; 0xc468a
    add byte [bp-011h], ah                    ; 00 66 ef                    ; 0xc468b
    mov edx, strict dword 0ed6601cfh          ; 66 ba cf 01 66 ed           ; 0xc468e vberom.asm:882
    db  00fh, 0b7h, 0c8h
    ; movzx cx, ax                              ; 0f b7 c8                  ; 0xc4694 vberom.asm:884
    mov edx, strict dword 0b86601ceh          ; 66 ba ce 01 66 b8           ; 0xc4697 vberom.asm:885
    add ax, word [bx+si]                      ; 03 00                       ; 0xc469d
    out DX, eax                               ; 66 ef                       ; 0xc469f vberom.asm:887
    mov edx, strict dword 0ed6601cfh          ; 66 ba cf 01 66 ed           ; 0xc46a1 vberom.asm:888
    db  00fh, 0b7h, 0f0h
    ; movzx si, ax                              ; 0f b7 f0                  ; 0xc46a7 vberom.asm:890
    pop ax                                    ; 58                          ; 0xc46aa vberom.asm:891
    cmp si, strict byte 00004h                ; 83 fe 04                    ; 0xc46ab vberom.asm:893
    je short 046c7h                           ; 74 17                       ; 0xc46ae vberom.asm:894
    add si, strict byte 00007h                ; 83 c6 07                    ; 0xc46b0 vberom.asm:895
    shr si, 003h                              ; c1 ee 03                    ; 0xc46b3 vberom.asm:896
    imul cx, si                               ; 0f af ce                    ; 0xc46b6 vberom.asm:897
    db  033h, 0d2h
    ; xor dx, dx                                ; 33 d2                     ; 0xc46b9 vberom.asm:898
    div cx                                    ; f7 f1                       ; 0xc46bb vberom.asm:899
    db  08bh, 0f8h
    ; mov di, ax                                ; 8b f8                     ; 0xc46bd vberom.asm:900
    db  08bh, 0c2h
    ; mov ax, dx                                ; 8b c2                     ; 0xc46bf vberom.asm:901
    db  033h, 0d2h
    ; xor dx, dx                                ; 33 d2                     ; 0xc46c1 vberom.asm:902
    div si                                    ; f7 f6                       ; 0xc46c3 vberom.asm:903
    jmp short 046d3h                          ; eb 0c                       ; 0xc46c5 vberom.asm:904
    shr cx, 1                                 ; d1 e9                       ; 0xc46c7 vberom.asm:907
    db  033h, 0d2h
    ; xor dx, dx                                ; 33 d2                     ; 0xc46c9 vberom.asm:908
    div cx                                    ; f7 f1                       ; 0xc46cb vberom.asm:909
    db  08bh, 0f8h
    ; mov di, ax                                ; 8b f8                     ; 0xc46cd vberom.asm:910
    db  08bh, 0c2h
    ; mov ax, dx                                ; 8b c2                     ; 0xc46cf vberom.asm:911
    sal ax, 1                                 ; d1 e0                       ; 0xc46d1 vberom.asm:912
    push edx                                  ; 66 52                       ; 0xc46d3 vberom.asm:915
    push eax                                  ; 66 50                       ; 0xc46d5 vberom.asm:916
    mov edx, strict dword 0b86601ceh          ; 66 ba ce 01 66 b8           ; 0xc46d7 vberom.asm:917
    or byte [bx+si], al                       ; 08 00                       ; 0xc46dd
    out DX, eax                               ; 66 ef                       ; 0xc46df vberom.asm:919
    pop eax                                   ; 66 58                       ; 0xc46e1 vberom.asm:920
    mov edx, strict dword 0ef6601cfh          ; 66 ba cf 01 66 ef           ; 0xc46e3 vberom.asm:921
    pop edx                                   ; 66 5a                       ; 0xc46e9 vberom.asm:923
    db  066h, 08bh, 0c7h
    ; mov eax, edi                              ; 66 8b c7                  ; 0xc46eb vberom.asm:925
    push edx                                  ; 66 52                       ; 0xc46ee vberom.asm:926
    push eax                                  ; 66 50                       ; 0xc46f0 vberom.asm:927
    mov edx, strict dword 0b86601ceh          ; 66 ba ce 01 66 b8           ; 0xc46f2 vberom.asm:928
    or word [bx+si], ax                       ; 09 00                       ; 0xc46f8
    out DX, eax                               ; 66 ef                       ; 0xc46fa vberom.asm:930
    pop eax                                   ; 66 58                       ; 0xc46fc vberom.asm:931
    mov edx, strict dword 0ef6601cfh          ; 66 ba cf 01 66 ef           ; 0xc46fe vberom.asm:932
    pop edx                                   ; 66 5a                       ; 0xc4704 vberom.asm:934
    pop di                                    ; 5f                          ; 0xc4706 vberom.asm:936
    pop si                                    ; 5e                          ; 0xc4707 vberom.asm:937
    pop dx                                    ; 5a                          ; 0xc4708 vberom.asm:938
    pop cx                                    ; 59                          ; 0xc4709 vberom.asm:939
    pop ax                                    ; 58                          ; 0xc470a vberom.asm:940
    mov eax, strict dword 066c3004fh          ; 66 b8 4f 00 c3 66           ; 0xc470b vberom.asm:941
    mov ax, 0014fh                            ; b8 4f 01                    ; 0xc4711
  ; disGetNextSymbol 0xc4714 LB 0x1 -> off=0x0 cb=0000000000000001 uValue=0000000000000114 'vesa_pm_end'
vesa_pm_end:                                 ; 0xc4714 LB 0x1
    retn                                      ; c3                          ; 0xc4714 vberom.asm:946

  ; Padding 0x6b bytes at 0xc4715
  times 107 db 0

section _DATA progbits vstart=0x4780 align=1 ; size=0x3726 class=DATA group=DGROUP
  ; disGetNextSymbol 0xc4780 LB 0x3726 -> off=0x0 cb=000000000000002e uValue=00000000000c0000 '_msg_vga_init'
_msg_vga_init:                               ; 0xc4780 LB 0x2e
    db  'Oracle VM VirtualBox Version 6.1.1 VGA BIOS', 00dh, 00ah, 000h
  ; disGetNextSymbol 0xc47ae LB 0x36f8 -> off=0x0 cb=0000000000000080 uValue=00000000000c002e 'vga_modes'
vga_modes:                                   ; 0xc47ae LB 0x80
    db  000h, 000h, 000h, 004h, 000h, 0b8h, 0ffh, 002h, 001h, 000h, 000h, 004h, 000h, 0b8h, 0ffh, 002h
    db  002h, 000h, 000h, 004h, 000h, 0b8h, 0ffh, 002h, 003h, 000h, 000h, 004h, 000h, 0b8h, 0ffh, 002h
    db  004h, 001h, 002h, 002h, 000h, 0b8h, 0ffh, 001h, 005h, 001h, 002h, 002h, 000h, 0b8h, 0ffh, 001h
    db  006h, 001h, 002h, 001h, 000h, 0b8h, 0ffh, 001h, 007h, 000h, 001h, 004h, 000h, 0b0h, 0ffh, 000h
    db  00dh, 001h, 004h, 004h, 000h, 0a0h, 0ffh, 001h, 00eh, 001h, 004h, 004h, 000h, 0a0h, 0ffh, 001h
    db  00fh, 001h, 003h, 001h, 000h, 0a0h, 0ffh, 000h, 010h, 001h, 004h, 004h, 000h, 0a0h, 0ffh, 002h
    db  011h, 001h, 003h, 001h, 000h, 0a0h, 0ffh, 002h, 012h, 001h, 004h, 004h, 000h, 0a0h, 0ffh, 002h
    db  013h, 001h, 005h, 008h, 000h, 0a0h, 0ffh, 003h, 06ah, 001h, 004h, 004h, 000h, 0a0h, 0ffh, 002h
  ; disGetNextSymbol 0xc482e LB 0x3678 -> off=0x0 cb=0000000000000010 uValue=00000000000c00ae 'line_to_vpti'
line_to_vpti:                                ; 0xc482e LB 0x10
    db  017h, 017h, 018h, 018h, 004h, 005h, 006h, 007h, 00dh, 00eh, 011h, 012h, 01ah, 01bh, 01ch, 01dh
  ; disGetNextSymbol 0xc483e LB 0x3668 -> off=0x0 cb=0000000000000004 uValue=00000000000c00be 'dac_regs'
dac_regs:                                    ; 0xc483e LB 0x4
    dd  0ff3f3f3fh
  ; disGetNextSymbol 0xc4842 LB 0x3664 -> off=0x0 cb=0000000000000780 uValue=00000000000c00c2 'video_param_table'
video_param_table:                           ; 0xc4842 LB 0x780
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
    db  028h, 018h, 008h, 000h, 040h, 009h, 003h, 000h, 002h, 063h, 02dh, 027h, 028h, 090h, 02bh, 080h
    db  0bfh, 01fh, 000h, 0c1h, 000h, 000h, 000h, 000h, 000h, 000h, 09ch, 08eh, 08fh, 014h, 000h, 096h
    db  0b9h, 0a2h, 0ffh, 000h, 013h, 015h, 017h, 002h, 004h, 006h, 007h, 010h, 011h, 012h, 013h, 014h
    db  015h, 016h, 017h, 001h, 000h, 003h, 000h, 000h, 000h, 000h, 000h, 000h, 030h, 00fh, 00fh, 0ffh
    db  028h, 018h, 008h, 000h, 040h, 009h, 003h, 000h, 002h, 063h, 02dh, 027h, 028h, 090h, 02bh, 080h
    db  0bfh, 01fh, 000h, 0c1h, 000h, 000h, 000h, 000h, 000h, 000h, 09ch, 08eh, 08fh, 014h, 000h, 096h
    db  0b9h, 0a2h, 0ffh, 000h, 013h, 015h, 017h, 002h, 004h, 006h, 007h, 010h, 011h, 012h, 013h, 014h
    db  015h, 016h, 017h, 001h, 000h, 003h, 000h, 000h, 000h, 000h, 000h, 000h, 030h, 00fh, 00fh, 0ffh
    db  050h, 018h, 008h, 000h, 040h, 001h, 001h, 000h, 006h, 063h, 05fh, 04fh, 050h, 082h, 054h, 080h
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
    db  050h, 01dh, 010h, 000h, 0a0h, 001h, 00fh, 000h, 006h, 0e3h, 05fh, 04fh, 050h, 082h, 054h, 080h
    db  00bh, 03eh, 000h, 040h, 000h, 000h, 000h, 000h, 000h, 000h, 0eah, 08ch, 0dfh, 028h, 000h, 0e7h
    db  004h, 0c3h, 0ffh, 000h, 03fh, 000h, 03fh, 000h, 03fh, 000h, 03fh, 000h, 03fh, 000h, 03fh, 000h
    db  03fh, 000h, 03fh, 001h, 000h, 00fh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 005h, 00fh, 0ffh
    db  050h, 01dh, 010h, 000h, 0a0h, 001h, 00fh, 000h, 006h, 0e3h, 05fh, 04fh, 050h, 082h, 054h, 080h
    db  00bh, 03eh, 000h, 040h, 000h, 000h, 000h, 000h, 000h, 000h, 0eah, 08ch, 0dfh, 028h, 000h, 0e7h
    db  004h, 0e3h, 0ffh, 000h, 001h, 002h, 003h, 004h, 005h, 014h, 007h, 038h, 039h, 03ah, 03bh, 03ch
    db  03dh, 03eh, 03fh, 001h, 000h, 00fh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 005h, 00fh, 0ffh
    db  028h, 018h, 008h, 000h, 020h, 001h, 00fh, 000h, 00eh, 063h, 05fh, 04fh, 050h, 082h, 054h, 080h
    db  0bfh, 01fh, 000h, 041h, 000h, 000h, 000h, 000h, 000h, 000h, 09ch, 08eh, 08fh, 028h, 040h, 096h
    db  0b9h, 0a3h, 0ffh, 000h, 001h, 002h, 003h, 004h, 005h, 006h, 007h, 008h, 009h, 00ah, 00bh, 00ch
    db  00dh, 00eh, 00fh, 041h, 000h, 00fh, 000h, 000h, 000h, 000h, 000h, 000h, 040h, 005h, 00fh, 0ffh
    db  064h, 024h, 010h, 000h, 000h, 001h, 00fh, 000h, 006h, 0e3h, 07fh, 063h, 063h, 083h, 06bh, 01bh
    db  072h, 0f0h, 000h, 060h, 000h, 000h, 000h, 000h, 000h, 000h, 059h, 08dh, 057h, 032h, 000h, 057h
    db  073h, 0e3h, 0ffh, 000h, 001h, 002h, 003h, 004h, 005h, 014h, 007h, 038h, 039h, 03ah, 03bh, 03ch
    db  03dh, 03eh, 03fh, 001h, 000h, 00fh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 005h, 00fh, 0ffh
  ; disGetNextSymbol 0xc4fc2 LB 0x2ee4 -> off=0x0 cb=00000000000000c0 uValue=00000000000c0842 'palette0'
palette0:                                    ; 0xc4fc2 LB 0xc0
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
  ; disGetNextSymbol 0xc5082 LB 0x2e24 -> off=0x0 cb=00000000000000c0 uValue=00000000000c0902 'palette1'
palette1:                                    ; 0xc5082 LB 0xc0
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
  ; disGetNextSymbol 0xc5142 LB 0x2d64 -> off=0x0 cb=00000000000000c0 uValue=00000000000c09c2 'palette2'
palette2:                                    ; 0xc5142 LB 0xc0
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
  ; disGetNextSymbol 0xc5202 LB 0x2ca4 -> off=0x0 cb=0000000000000300 uValue=00000000000c0a82 'palette3'
palette3:                                    ; 0xc5202 LB 0x300
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
  ; disGetNextSymbol 0xc5502 LB 0x29a4 -> off=0x0 cb=0000000000000010 uValue=00000000000c0d82 'static_functionality'
static_functionality:                        ; 0xc5502 LB 0x10
    db  0ffh, 0e0h, 00fh, 000h, 000h, 000h, 000h, 007h, 002h, 008h, 0e7h, 00ch, 000h, 000h, 000h, 000h
  ; disGetNextSymbol 0xc5512 LB 0x2994 -> off=0x0 cb=0000000000000024 uValue=00000000000c0d92 '_dcc_table'
_dcc_table:                                  ; 0xc5512 LB 0x24
    db  010h, 001h, 007h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h
  ; disGetNextSymbol 0xc5536 LB 0x2970 -> off=0x0 cb=000000000000001a uValue=00000000000c0db6 '_secondary_save_area'
_secondary_save_area:                        ; 0xc5536 LB 0x1a
    db  01ah, 000h, 012h, 055h, 000h, 0c0h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
  ; disGetNextSymbol 0xc5550 LB 0x2956 -> off=0x0 cb=000000000000001c uValue=00000000000c0dd0 '_video_save_pointer_table'
_video_save_pointer_table:                   ; 0xc5550 LB 0x1c
    db  042h, 048h, 000h, 0c0h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  036h, 055h, 000h, 0c0h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
  ; disGetNextSymbol 0xc556c LB 0x293a -> off=0x0 cb=0000000000000800 uValue=00000000000c0dec 'vgafont8'
vgafont8:                                    ; 0xc556c LB 0x800
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
  ; disGetNextSymbol 0xc5d6c LB 0x213a -> off=0x0 cb=0000000000000e00 uValue=00000000000c15ec 'vgafont14'
vgafont14:                                   ; 0xc5d6c LB 0xe00
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
  ; disGetNextSymbol 0xc6b6c LB 0x133a -> off=0x0 cb=0000000000001000 uValue=00000000000c23ec 'vgafont16'
vgafont16:                                   ; 0xc6b6c LB 0x1000
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
  ; disGetNextSymbol 0xc7b6c LB 0x33a -> off=0x0 cb=000000000000012d uValue=00000000000c33ec 'vgafont14alt'
vgafont14alt:                                ; 0xc7b6c LB 0x12d
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
  ; disGetNextSymbol 0xc7c99 LB 0x20d -> off=0x0 cb=0000000000000144 uValue=00000000000c3519 'vgafont16alt'
vgafont16alt:                                ; 0xc7c99 LB 0x144
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
  ; disGetNextSymbol 0xc7ddd LB 0xc9 -> off=0x0 cb=0000000000000009 uValue=00000000000c365d '_cga_msr'
_cga_msr:                                    ; 0xc7ddd LB 0x9
    db  02ch, 028h, 02dh, 029h, 02ah, 02eh, 01eh, 029h, 000h
  ; disGetNextSymbol 0xc7de6 LB 0xc0 -> off=0x0 cb=0000000000000015 uValue=00000000000c3666 '_vbebios_copyright'
_vbebios_copyright:                          ; 0xc7de6 LB 0x15
    db  'VirtualBox VESA BIOS', 000h
  ; disGetNextSymbol 0xc7dfb LB 0xab -> off=0x0 cb=0000000000000013 uValue=00000000000c367b '_vbebios_vendor_name'
_vbebios_vendor_name:                        ; 0xc7dfb LB 0x13
    db  'Oracle Corporation', 000h
  ; disGetNextSymbol 0xc7e0e LB 0x98 -> off=0x0 cb=0000000000000021 uValue=00000000000c368e '_vbebios_product_name'
_vbebios_product_name:                       ; 0xc7e0e LB 0x21
    db  'Oracle VM VirtualBox VBE Adapter', 000h
  ; disGetNextSymbol 0xc7e2f LB 0x77 -> off=0x0 cb=0000000000000023 uValue=00000000000c36af '_vbebios_product_revision'
_vbebios_product_revision:                   ; 0xc7e2f LB 0x23
    db  'Oracle VM VirtualBox Version 6.1.1', 000h
  ; disGetNextSymbol 0xc7e52 LB 0x54 -> off=0x0 cb=000000000000002b uValue=00000000000c36d2 '_vbebios_info_string'
_vbebios_info_string:                        ; 0xc7e52 LB 0x2b
    db  'VirtualBox VBE Display Adapter enabled', 00dh, 00ah, 00dh, 00ah, 000h
  ; disGetNextSymbol 0xc7e7d LB 0x29 -> off=0x0 cb=0000000000000029 uValue=00000000000c36fd '_no_vbebios_info_string'
_no_vbebios_info_string:                     ; 0xc7e7d LB 0x29
    db  'No VirtualBox VBE support available!', 00dh, 00ah, 00dh, 00ah, 000h

section CONST progbits vstart=0x7ea6 align=1 ; size=0x0 class=DATA group=DGROUP

section CONST2 progbits vstart=0x7ea6 align=1 ; size=0x0 class=DATA group=DGROUP

  ; Padding 0x15a bytes at 0xc7ea6
    db  001h, 000h, 000h, 000h, 000h, 001h, 000h, 000h, 000h, 000h, 000h, 000h, 044h, 03ah, 05ch, 052h
    db  065h, 070h, 06fh, 073h, 069h, 074h, 06fh, 072h, 079h, 05ch, 074h, 072h, 075h, 06eh, 06bh, 05ch
    db  06fh, 075h, 074h, 05ch, 077h, 069h, 06eh, 02eh, 061h, 06dh, 064h, 036h, 034h, 05ch, 072h, 065h
    db  06ch, 065h, 061h, 073h, 065h, 05ch, 06fh, 062h, 06ah, 05ch, 056h, 042h, 06fh, 078h, 056h, 067h
    db  061h, 042h, 069h, 06fh, 073h, 038h, 030h, 038h, 036h, 05ch, 056h, 042h, 06fh, 078h, 056h, 067h
    db  061h, 042h, 069h, 06fh, 073h, 038h, 030h, 038h, 036h, 02eh, 073h, 079h, 06dh, 000h, 000h, 000h
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
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 052h
