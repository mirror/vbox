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
    db  055h, 0aah, 040h, 0e9h, 064h, 00ah, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
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
    call 0329eh                               ; e8 af 31                    ; 0xc00ec vgarom.asm:201
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
    mov di, 04400h                            ; bf 00 44                    ; 0xc0934 vberom.asm:784
    mov cx, 00115h                            ; b9 15 01                    ; 0xc0937 vberom.asm:785
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc093a vberom.asm:786
    retn                                      ; c3                          ; 0xc093d vberom.asm:787
    mov ax, 0014fh                            ; b8 4f 01                    ; 0xc093e vberom.asm:789
    retn                                      ; c3                          ; 0xc0941 vberom.asm:790

  ; Padding 0xbe bytes at 0xc0942
  times 190 db 0

section _TEXT progbits vstart=0xa00 align=1 ; size=0x32df class=CODE group=AUTO
  ; disGetNextSymbol 0xc0a00 LB 0x32df -> off=0x0 cb=000000000000001c uValue=00000000000c0a00 'set_int_vector'
set_int_vector:                              ; 0xc0a00 LB 0x1c
    push bx                                   ; 53                          ; 0xc0a00 vgabios.c:85
    push bp                                   ; 55                          ; 0xc0a01
    mov bp, sp                                ; 89 e5                       ; 0xc0a02
    mov bl, al                                ; 88 c3                       ; 0xc0a04
    xor bh, bh                                ; 30 ff                       ; 0xc0a06 vgabios.c:89
    sal bx, 1                                 ; d1 e3                       ; 0xc0a08
    sal bx, 1                                 ; d1 e3                       ; 0xc0a0a
    xor ax, ax                                ; 31 c0                       ; 0xc0a0c
    mov es, ax                                ; 8e c0                       ; 0xc0a0e
    mov word [es:bx], dx                      ; 26 89 17                    ; 0xc0a10
    mov word [es:bx+002h], 0c000h             ; 26 c7 47 02 00 c0           ; 0xc0a13
    pop bp                                    ; 5d                          ; 0xc0a19 vgabios.c:90
    pop bx                                    ; 5b                          ; 0xc0a1a
    retn                                      ; c3                          ; 0xc0a1b
  ; disGetNextSymbol 0xc0a1c LB 0x32c3 -> off=0x0 cb=000000000000001c uValue=00000000000c0a1c 'init_vga_card'
init_vga_card:                               ; 0xc0a1c LB 0x1c
    push bp                                   ; 55                          ; 0xc0a1c vgabios.c:141
    mov bp, sp                                ; 89 e5                       ; 0xc0a1d
    push dx                                   ; 52                          ; 0xc0a1f
    mov AL, strict byte 0c3h                  ; b0 c3                       ; 0xc0a20 vgabios.c:144
    mov dx, 003c2h                            ; ba c2 03                    ; 0xc0a22
    out DX, AL                                ; ee                          ; 0xc0a25
    mov AL, strict byte 004h                  ; b0 04                       ; 0xc0a26 vgabios.c:147
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc0a28
    out DX, AL                                ; ee                          ; 0xc0a2b
    mov AL, strict byte 002h                  ; b0 02                       ; 0xc0a2c vgabios.c:148
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc0a2e
    out DX, AL                                ; ee                          ; 0xc0a31
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc0a32 vgabios.c:153
    pop dx                                    ; 5a                          ; 0xc0a35
    pop bp                                    ; 5d                          ; 0xc0a36
    retn                                      ; c3                          ; 0xc0a37
  ; disGetNextSymbol 0xc0a38 LB 0x32a7 -> off=0x0 cb=0000000000000032 uValue=00000000000c0a38 'init_bios_area'
init_bios_area:                              ; 0xc0a38 LB 0x32
    push bx                                   ; 53                          ; 0xc0a38 vgabios.c:162
    push bp                                   ; 55                          ; 0xc0a39
    mov bp, sp                                ; 89 e5                       ; 0xc0a3a
    xor bx, bx                                ; 31 db                       ; 0xc0a3c vgabios.c:166
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0a3e
    mov es, ax                                ; 8e c0                       ; 0xc0a41
    mov al, byte [es:bx+010h]                 ; 26 8a 47 10                 ; 0xc0a43 vgabios.c:169
    and AL, strict byte 0cfh                  ; 24 cf                       ; 0xc0a47
    or AL, strict byte 020h                   ; 0c 20                       ; 0xc0a49
    mov byte [es:bx+010h], al                 ; 26 88 47 10                 ; 0xc0a4b
    mov byte [es:bx+00085h], 010h             ; 26 c6 87 85 00 10           ; 0xc0a4f vgabios.c:173
    mov word [es:bx+00087h], 0f960h           ; 26 c7 87 87 00 60 f9        ; 0xc0a55 vgabios.c:175
    mov byte [es:bx+00089h], 051h             ; 26 c6 87 89 00 51           ; 0xc0a5c vgabios.c:179
    mov byte [es:bx+065h], 009h               ; 26 c6 47 65 09              ; 0xc0a62 vgabios.c:181
    pop bp                                    ; 5d                          ; 0xc0a67 vgabios.c:182
    pop bx                                    ; 5b                          ; 0xc0a68
    retn                                      ; c3                          ; 0xc0a69
  ; disGetNextSymbol 0xc0a6a LB 0x3275 -> off=0x0 cb=0000000000000022 uValue=00000000000c0a6a 'vgabios_init_func'
vgabios_init_func:                           ; 0xc0a6a LB 0x22
    inc bp                                    ; 45                          ; 0xc0a6a vgabios.c:222
    push bp                                   ; 55                          ; 0xc0a6b
    mov bp, sp                                ; 89 e5                       ; 0xc0a6c
    call 00a1ch                               ; e8 ab ff                    ; 0xc0a6e vgabios.c:224
    call 00a38h                               ; e8 c4 ff                    ; 0xc0a71 vgabios.c:225
    call 0371ah                               ; e8 a3 2c                    ; 0xc0a74 vgabios.c:227
    mov dx, strict word 00022h                ; ba 22 00                    ; 0xc0a77 vgabios.c:229
    mov ax, strict word 00010h                ; b8 10 00                    ; 0xc0a7a
    call 00a00h                               ; e8 80 ff                    ; 0xc0a7d
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc0a80 vgabios.c:255
    db  032h, 0e4h
    ; xor ah, ah                                ; 32 e4                     ; 0xc0a83
    int 010h                                  ; cd 10                       ; 0xc0a85
    mov sp, bp                                ; 89 ec                       ; 0xc0a87 vgabios.c:258
    pop bp                                    ; 5d                          ; 0xc0a89
    dec bp                                    ; 4d                          ; 0xc0a8a
    retf                                      ; cb                          ; 0xc0a8b
  ; disGetNextSymbol 0xc0a8c LB 0x3253 -> off=0x0 cb=0000000000000046 uValue=00000000000c0a8c 'vga_get_cursor_pos'
vga_get_cursor_pos:                          ; 0xc0a8c LB 0x46
    push bp                                   ; 55                          ; 0xc0a8c vgabios.c:327
    mov bp, sp                                ; 89 e5                       ; 0xc0a8d
    push cx                                   ; 51                          ; 0xc0a8f
    push si                                   ; 56                          ; 0xc0a90
    mov cl, al                                ; 88 c1                       ; 0xc0a91
    mov si, dx                                ; 89 d6                       ; 0xc0a93
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc0a95 vgabios.c:329
    jbe short 00aa7h                          ; 76 0e                       ; 0xc0a97
    push SS                                   ; 16                          ; 0xc0a99 vgabios.c:330
    pop ES                                    ; 07                          ; 0xc0a9a
    mov word [es:si], strict word 00000h      ; 26 c7 04 00 00              ; 0xc0a9b
    mov word [es:bx], strict word 00000h      ; 26 c7 07 00 00              ; 0xc0aa0 vgabios.c:331
    jmp short 00acbh                          ; eb 24                       ; 0xc0aa5 vgabios.c:332
    mov dx, strict word 00060h                ; ba 60 00                    ; 0xc0aa7 vgabios.c:334
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0aaa
    call 031dah                               ; e8 2a 27                    ; 0xc0aad
    push SS                                   ; 16                          ; 0xc0ab0
    pop ES                                    ; 07                          ; 0xc0ab1
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc0ab2
    mov al, cl                                ; 88 c8                       ; 0xc0ab5 vgabios.c:335
    xor ah, ah                                ; 30 e4                       ; 0xc0ab7
    mov dx, ax                                ; 89 c2                       ; 0xc0ab9
    sal dx, 1                                 ; d1 e2                       ; 0xc0abb
    add dx, strict byte 00050h                ; 83 c2 50                    ; 0xc0abd
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0ac0
    call 031dah                               ; e8 14 27                    ; 0xc0ac3
    push SS                                   ; 16                          ; 0xc0ac6
    pop ES                                    ; 07                          ; 0xc0ac7
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc0ac8
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0acb vgabios.c:337
    pop si                                    ; 5e                          ; 0xc0ace
    pop cx                                    ; 59                          ; 0xc0acf
    pop bp                                    ; 5d                          ; 0xc0ad0
    retn                                      ; c3                          ; 0xc0ad1
  ; disGetNextSymbol 0xc0ad2 LB 0x320d -> off=0x0 cb=000000000000009f uValue=00000000000c0ad2 'vga_read_char_attr'
vga_read_char_attr:                          ; 0xc0ad2 LB 0x9f
    push bp                                   ; 55                          ; 0xc0ad2 vgabios.c:340
    mov bp, sp                                ; 89 e5                       ; 0xc0ad3
    push bx                                   ; 53                          ; 0xc0ad5
    push cx                                   ; 51                          ; 0xc0ad6
    push si                                   ; 56                          ; 0xc0ad7
    push di                                   ; 57                          ; 0xc0ad8
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc0ad9
    mov ch, al                                ; 88 c5                       ; 0xc0adc
    mov si, dx                                ; 89 d6                       ; 0xc0ade
    mov dx, strict word 00049h                ; ba 49 00                    ; 0xc0ae0 vgabios.c:347
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0ae3
    call 031beh                               ; e8 d5 26                    ; 0xc0ae6
    xor ah, ah                                ; 30 e4                       ; 0xc0ae9 vgabios.c:348
    call 03193h                               ; e8 a5 26                    ; 0xc0aeb
    mov cl, al                                ; 88 c1                       ; 0xc0aee
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc0af0 vgabios.c:349
    je short 00b68h                           ; 74 74                       ; 0xc0af2
    mov al, ch                                ; 88 e8                       ; 0xc0af4 vgabios.c:353
    xor ah, ah                                ; 30 e4                       ; 0xc0af6
    lea bx, [bp-012h]                         ; 8d 5e ee                    ; 0xc0af8
    lea dx, [bp-010h]                         ; 8d 56 f0                    ; 0xc0afb
    call 00a8ch                               ; e8 8b ff                    ; 0xc0afe
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc0b01 vgabios.c:354
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc0b04
    mov ax, word [bp-012h]                    ; 8b 46 ee                    ; 0xc0b07 vgabios.c:355
    mov al, ah                                ; 88 e0                       ; 0xc0b0a
    xor ah, ah                                ; 30 e4                       ; 0xc0b0c
    mov word [bp-00eh], ax                    ; 89 46 f2                    ; 0xc0b0e
    mov dx, 00084h                            ; ba 84 00                    ; 0xc0b11 vgabios.c:358
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0b14
    call 031beh                               ; e8 a4 26                    ; 0xc0b17
    xor ah, ah                                ; 30 e4                       ; 0xc0b1a
    inc ax                                    ; 40                          ; 0xc0b1c
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc0b1d
    mov dx, strict word 0004ah                ; ba 4a 00                    ; 0xc0b20 vgabios.c:359
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0b23
    call 031dah                               ; e8 b1 26                    ; 0xc0b26
    mov di, ax                                ; 89 c7                       ; 0xc0b29
    mov bl, cl                                ; 88 cb                       ; 0xc0b2b vgabios.c:361
    xor bh, bh                                ; 30 ff                       ; 0xc0b2d
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc0b2f
    sal bx, CL                                ; d3 e3                       ; 0xc0b31
    cmp byte [bx+04635h], 000h                ; 80 bf 35 46 00              ; 0xc0b33
    jne short 00b68h                          ; 75 2e                       ; 0xc0b38
    mul word [bp-00ch]                        ; f7 66 f4                    ; 0xc0b3a vgabios.c:363
    sal ax, 1                                 ; d1 e0                       ; 0xc0b3d
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc0b3f
    mov cl, ch                                ; 88 e9                       ; 0xc0b41
    xor ch, ch                                ; 30 ed                       ; 0xc0b43
    inc ax                                    ; 40                          ; 0xc0b45
    mul cx                                    ; f7 e1                       ; 0xc0b46
    mov cx, ax                                ; 89 c1                       ; 0xc0b48
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc0b4a
    xor ah, ah                                ; 30 e4                       ; 0xc0b4d
    mul di                                    ; f7 e7                       ; 0xc0b4f
    mov dx, ax                                ; 89 c2                       ; 0xc0b51
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc0b53
    xor ah, ah                                ; 30 e4                       ; 0xc0b56
    add dx, ax                                ; 01 c2                       ; 0xc0b58
    sal dx, 1                                 ; d1 e2                       ; 0xc0b5a
    add dx, cx                                ; 01 ca                       ; 0xc0b5c
    mov ax, word [bx+04638h]                  ; 8b 87 38 46                 ; 0xc0b5e vgabios.c:364
    call 031dah                               ; e8 75 26                    ; 0xc0b62
    mov word [ss:si], ax                      ; 36 89 04                    ; 0xc0b65
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc0b68 vgabios.c:371
    pop di                                    ; 5f                          ; 0xc0b6b
    pop si                                    ; 5e                          ; 0xc0b6c
    pop cx                                    ; 59                          ; 0xc0b6d
    pop bx                                    ; 5b                          ; 0xc0b6e
    pop bp                                    ; 5d                          ; 0xc0b6f
    retn                                      ; c3                          ; 0xc0b70
  ; disGetNextSymbol 0xc0b71 LB 0x316e -> off=0x10 cb=000000000000007b uValue=00000000000c0b81 'vga_get_font_info'
    db  096h, 00bh, 0d4h, 00bh, 0d9h, 00bh, 0e1h, 00bh, 0e6h, 00bh, 0ebh, 00bh, 0f0h, 00bh, 0f5h, 00bh
vga_get_font_info:                           ; 0xc0b81 LB 0x7b
    push bp                                   ; 55                          ; 0xc0b81 vgabios.c:373
    mov bp, sp                                ; 89 e5                       ; 0xc0b82
    push si                                   ; 56                          ; 0xc0b84
    push di                                   ; 57                          ; 0xc0b85
    mov si, dx                                ; 89 d6                       ; 0xc0b86
    cmp ax, strict word 00007h                ; 3d 07 00                    ; 0xc0b88 vgabios.c:378
    jnbe short 00bcbh                         ; 77 3e                       ; 0xc0b8b
    mov di, ax                                ; 89 c7                       ; 0xc0b8d
    sal di, 1                                 ; d1 e7                       ; 0xc0b8f
    jmp word [cs:di+00b71h]                   ; 2e ff a5 71 0b              ; 0xc0b91
    mov dx, strict word 0007ch                ; ba 7c 00                    ; 0xc0b96 vgabios.c:380
    xor ax, ax                                ; 31 c0                       ; 0xc0b99
    call 031f6h                               ; e8 58 26                    ; 0xc0b9b
    push SS                                   ; 16                          ; 0xc0b9e vgabios.c:381
    pop ES                                    ; 07                          ; 0xc0b9f
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc0ba0
    mov word [es:si], dx                      ; 26 89 14                    ; 0xc0ba3
    mov dx, 00085h                            ; ba 85 00                    ; 0xc0ba6
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0ba9
    call 031beh                               ; e8 0f 26                    ; 0xc0bac
    xor ah, ah                                ; 30 e4                       ; 0xc0baf
    push SS                                   ; 16                          ; 0xc0bb1
    pop ES                                    ; 07                          ; 0xc0bb2
    mov bx, cx                                ; 89 cb                       ; 0xc0bb3
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc0bb5
    mov dx, 00084h                            ; ba 84 00                    ; 0xc0bb8
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0bbb
    call 031beh                               ; e8 fd 25                    ; 0xc0bbe
    xor ah, ah                                ; 30 e4                       ; 0xc0bc1
    push SS                                   ; 16                          ; 0xc0bc3
    pop ES                                    ; 07                          ; 0xc0bc4
    mov bx, word [bp+004h]                    ; 8b 5e 04                    ; 0xc0bc5
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc0bc8
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0bcb
    pop di                                    ; 5f                          ; 0xc0bce
    pop si                                    ; 5e                          ; 0xc0bcf
    pop bp                                    ; 5d                          ; 0xc0bd0
    retn 00002h                               ; c2 02 00                    ; 0xc0bd1
    mov dx, 0010ch                            ; ba 0c 01                    ; 0xc0bd4 vgabios.c:383
    jmp short 00b99h                          ; eb c0                       ; 0xc0bd7
    mov ax, 05bf2h                            ; b8 f2 5b                    ; 0xc0bd9 vgabios.c:386
    mov dx, 0c000h                            ; ba 00 c0                    ; 0xc0bdc
    jmp short 00b9eh                          ; eb bd                       ; 0xc0bdf vgabios.c:387
    mov ax, 053f2h                            ; b8 f2 53                    ; 0xc0be1 vgabios.c:389
    jmp short 00bdch                          ; eb f6                       ; 0xc0be4
    mov ax, 057f2h                            ; b8 f2 57                    ; 0xc0be6 vgabios.c:392
    jmp short 00bdch                          ; eb f1                       ; 0xc0be9
    mov ax, 079f2h                            ; b8 f2 79                    ; 0xc0beb vgabios.c:395
    jmp short 00bdch                          ; eb ec                       ; 0xc0bee
    mov ax, 069f2h                            ; b8 f2 69                    ; 0xc0bf0 vgabios.c:398
    jmp short 00bdch                          ; eb e7                       ; 0xc0bf3
    mov ax, 07b1fh                            ; b8 1f 7b                    ; 0xc0bf5 vgabios.c:401
    jmp short 00bdch                          ; eb e2                       ; 0xc0bf8
    jmp short 00bcbh                          ; eb cf                       ; 0xc0bfa vgabios.c:407
  ; disGetNextSymbol 0xc0bfc LB 0x30e3 -> off=0x0 cb=0000000000000143 uValue=00000000000c0bfc 'vga_read_pixel'
vga_read_pixel:                              ; 0xc0bfc LB 0x143
    push bp                                   ; 55                          ; 0xc0bfc vgabios.c:420
    mov bp, sp                                ; 89 e5                       ; 0xc0bfd
    push si                                   ; 56                          ; 0xc0bff
    push di                                   ; 57                          ; 0xc0c00
    sub sp, strict byte 00006h                ; 83 ec 06                    ; 0xc0c01
    mov si, dx                                ; 89 d6                       ; 0xc0c04
    mov word [bp-00ah], bx                    ; 89 5e f6                    ; 0xc0c06
    mov di, cx                                ; 89 cf                       ; 0xc0c09
    mov dx, strict word 00049h                ; ba 49 00                    ; 0xc0c0b vgabios.c:426
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0c0e
    call 031beh                               ; e8 aa 25                    ; 0xc0c11
    xor ah, ah                                ; 30 e4                       ; 0xc0c14 vgabios.c:427
    call 03193h                               ; e8 7a 25                    ; 0xc0c16
    mov ch, al                                ; 88 c5                       ; 0xc0c19
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc0c1b vgabios.c:428
    je short 00c2eh                           ; 74 0f                       ; 0xc0c1d
    mov bl, al                                ; 88 c3                       ; 0xc0c1f vgabios.c:430
    xor bh, bh                                ; 30 ff                       ; 0xc0c21
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc0c23
    sal bx, CL                                ; d3 e3                       ; 0xc0c25
    cmp byte [bx+04635h], 000h                ; 80 bf 35 46 00              ; 0xc0c27
    jne short 00c31h                          ; 75 03                       ; 0xc0c2c
    jmp near 00d38h                           ; e9 07 01                    ; 0xc0c2e vgabios.c:431
    mov bl, byte [bx+04636h]                  ; 8a 9f 36 46                 ; 0xc0c31 vgabios.c:434
    cmp bl, cl                                ; 38 cb                       ; 0xc0c35
    jc short 00c48h                           ; 72 0f                       ; 0xc0c37
    jbe short 00c50h                          ; 76 15                       ; 0xc0c39
    cmp bl, 005h                              ; 80 fb 05                    ; 0xc0c3b
    je short 00ca8h                           ; 74 68                       ; 0xc0c3e
    cmp bl, 004h                              ; 80 fb 04                    ; 0xc0c40
    je short 00c50h                           ; 74 0b                       ; 0xc0c43
    jmp near 00d33h                           ; e9 eb 00                    ; 0xc0c45
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc0c48
    je short 00cadh                           ; 74 60                       ; 0xc0c4b
    jmp near 00d33h                           ; e9 e3 00                    ; 0xc0c4d
    mov dx, strict word 0004ah                ; ba 4a 00                    ; 0xc0c50 vgabios.c:437
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0c53
    call 031dah                               ; e8 81 25                    ; 0xc0c56
    mov bx, ax                                ; 89 c3                       ; 0xc0c59
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc0c5b
    mul bx                                    ; f7 e3                       ; 0xc0c5e
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc0c60
    mov bx, si                                ; 89 f3                       ; 0xc0c62
    shr bx, CL                                ; d3 eb                       ; 0xc0c64
    add bx, ax                                ; 01 c3                       ; 0xc0c66
    mov cx, si                                ; 89 f1                       ; 0xc0c68 vgabios.c:438
    and cx, strict byte 00007h                ; 83 e1 07                    ; 0xc0c6a
    mov ax, 00080h                            ; b8 80 00                    ; 0xc0c6d
    sar ax, CL                                ; d3 f8                       ; 0xc0c70
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc0c72
    mov byte [bp-006h], ch                    ; 88 6e fa                    ; 0xc0c75 vgabios.c:440
    jmp short 00c80h                          ; eb 06                       ; 0xc0c78
    cmp byte [bp-006h], 004h                  ; 80 7e fa 04                 ; 0xc0c7a
    jnc short 00caah                          ; 73 2a                       ; 0xc0c7e
    mov ah, byte [bp-006h]                    ; 8a 66 fa                    ; 0xc0c80 vgabios.c:441
    xor al, al                                ; 30 c0                       ; 0xc0c83
    or AL, strict byte 004h                   ; 0c 04                       ; 0xc0c85
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc0c87
    out DX, ax                                ; ef                          ; 0xc0c8a
    mov dx, bx                                ; 89 da                       ; 0xc0c8b vgabios.c:442
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc0c8d
    call 031beh                               ; e8 2b 25                    ; 0xc0c90
    and al, byte [bp-008h]                    ; 22 46 f8                    ; 0xc0c93
    test al, al                               ; 84 c0                       ; 0xc0c96 vgabios.c:443
    jbe short 00ca3h                          ; 76 09                       ; 0xc0c98
    mov cl, byte [bp-006h]                    ; 8a 4e fa                    ; 0xc0c9a vgabios.c:444
    mov AL, strict byte 001h                  ; b0 01                       ; 0xc0c9d
    sal al, CL                                ; d2 e0                       ; 0xc0c9f
    or ch, al                                 ; 08 c5                       ; 0xc0ca1
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc0ca3 vgabios.c:445
    jmp short 00c7ah                          ; eb d2                       ; 0xc0ca6
    jmp short 00d13h                          ; eb 69                       ; 0xc0ca8
    jmp near 00d35h                           ; e9 88 00                    ; 0xc0caa
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc0cad vgabios.c:448
    shr ax, 1                                 ; d1 e8                       ; 0xc0cb0
    mov bx, strict word 00050h                ; bb 50 00                    ; 0xc0cb2
    mul bx                                    ; f7 e3                       ; 0xc0cb5
    mov bx, si                                ; 89 f3                       ; 0xc0cb7
    shr bx, 1                                 ; d1 eb                       ; 0xc0cb9
    shr bx, 1                                 ; d1 eb                       ; 0xc0cbb
    add bx, ax                                ; 01 c3                       ; 0xc0cbd
    test byte [bp-00ah], 001h                 ; f6 46 f6 01                 ; 0xc0cbf vgabios.c:449
    je short 00cc8h                           ; 74 03                       ; 0xc0cc3
    add bh, 020h                              ; 80 c7 20                    ; 0xc0cc5 vgabios.c:450
    mov dx, bx                                ; 89 da                       ; 0xc0cc8 vgabios.c:451
    mov ax, 0b800h                            ; b8 00 b8                    ; 0xc0cca
    call 031beh                               ; e8 ee 24                    ; 0xc0ccd
    mov bl, ch                                ; 88 eb                       ; 0xc0cd0 vgabios.c:452
    xor bh, bh                                ; 30 ff                       ; 0xc0cd2
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc0cd4
    sal bx, CL                                ; d3 e3                       ; 0xc0cd6
    cmp byte [bx+04637h], 002h                ; 80 bf 37 46 02              ; 0xc0cd8
    jne short 00cfah                          ; 75 1b                       ; 0xc0cdd
    mov cx, si                                ; 89 f1                       ; 0xc0cdf vgabios.c:453
    xor ch, ch                                ; 30 ed                       ; 0xc0ce1
    and cl, 003h                              ; 80 e1 03                    ; 0xc0ce3
    mov bx, strict word 00003h                ; bb 03 00                    ; 0xc0ce6
    sub bx, cx                                ; 29 cb                       ; 0xc0ce9
    mov cx, bx                                ; 89 d9                       ; 0xc0ceb
    sal cx, 1                                 ; d1 e1                       ; 0xc0ced
    xor ah, ah                                ; 30 e4                       ; 0xc0cef
    sar ax, CL                                ; d3 f8                       ; 0xc0cf1
    mov ch, al                                ; 88 c5                       ; 0xc0cf3
    and ch, 003h                              ; 80 e5 03                    ; 0xc0cf5
    jmp short 00d35h                          ; eb 3b                       ; 0xc0cf8 vgabios.c:454
    mov cx, si                                ; 89 f1                       ; 0xc0cfa vgabios.c:455
    xor ch, ch                                ; 30 ed                       ; 0xc0cfc
    and cl, 007h                              ; 80 e1 07                    ; 0xc0cfe
    mov bx, strict word 00007h                ; bb 07 00                    ; 0xc0d01
    sub bx, cx                                ; 29 cb                       ; 0xc0d04
    mov cx, bx                                ; 89 d9                       ; 0xc0d06
    xor ah, ah                                ; 30 e4                       ; 0xc0d08
    sar ax, CL                                ; d3 f8                       ; 0xc0d0a
    mov ch, al                                ; 88 c5                       ; 0xc0d0c
    and ch, 001h                              ; 80 e5 01                    ; 0xc0d0e
    jmp short 00d35h                          ; eb 22                       ; 0xc0d11 vgabios.c:456
    mov dx, strict word 0004ah                ; ba 4a 00                    ; 0xc0d13 vgabios.c:458
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0d16
    call 031dah                               ; e8 be 24                    ; 0xc0d19
    mov bx, ax                                ; 89 c3                       ; 0xc0d1c
    sal bx, CL                                ; d3 e3                       ; 0xc0d1e
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc0d20
    mul bx                                    ; f7 e3                       ; 0xc0d23
    mov dx, si                                ; 89 f2                       ; 0xc0d25
    add dx, ax                                ; 01 c2                       ; 0xc0d27
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc0d29
    call 031beh                               ; e8 8f 24                    ; 0xc0d2c
    mov ch, al                                ; 88 c5                       ; 0xc0d2f
    jmp short 00d35h                          ; eb 02                       ; 0xc0d31 vgabios.c:460
    xor ch, ch                                ; 30 ed                       ; 0xc0d33 vgabios.c:465
    mov byte [ss:di], ch                      ; 36 88 2d                    ; 0xc0d35 vgabios.c:467
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0d38 vgabios.c:468
    pop di                                    ; 5f                          ; 0xc0d3b
    pop si                                    ; 5e                          ; 0xc0d3c
    pop bp                                    ; 5d                          ; 0xc0d3d
    retn                                      ; c3                          ; 0xc0d3e
  ; disGetNextSymbol 0xc0d3f LB 0x2fa0 -> off=0x0 cb=000000000000009f uValue=00000000000c0d3f 'biosfn_perform_gray_scale_summing'
biosfn_perform_gray_scale_summing:           ; 0xc0d3f LB 0x9f
    push bp                                   ; 55                          ; 0xc0d3f vgabios.c:473
    mov bp, sp                                ; 89 e5                       ; 0xc0d40
    push bx                                   ; 53                          ; 0xc0d42
    push cx                                   ; 51                          ; 0xc0d43
    push si                                   ; 56                          ; 0xc0d44
    push di                                   ; 57                          ; 0xc0d45
    push ax                                   ; 50                          ; 0xc0d46
    push ax                                   ; 50                          ; 0xc0d47
    mov bx, ax                                ; 89 c3                       ; 0xc0d48
    mov di, dx                                ; 89 d7                       ; 0xc0d4a
    mov dx, 003dah                            ; ba da 03                    ; 0xc0d4c vgabios.c:478
    in AL, DX                                 ; ec                          ; 0xc0d4f
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc0d50
    xor al, al                                ; 30 c0                       ; 0xc0d52 vgabios.c:479
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0d54
    out DX, AL                                ; ee                          ; 0xc0d57
    xor si, si                                ; 31 f6                       ; 0xc0d58 vgabios.c:481
    cmp si, di                                ; 39 fe                       ; 0xc0d5a
    jnc short 00dc3h                          ; 73 65                       ; 0xc0d5c
    mov al, bl                                ; 88 d8                       ; 0xc0d5e vgabios.c:484
    mov dx, 003c7h                            ; ba c7 03                    ; 0xc0d60
    out DX, AL                                ; ee                          ; 0xc0d63
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc0d64 vgabios.c:486
    in AL, DX                                 ; ec                          ; 0xc0d67
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc0d68
    mov cx, ax                                ; 89 c1                       ; 0xc0d6a
    in AL, DX                                 ; ec                          ; 0xc0d6c vgabios.c:487
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc0d6d
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc0d6f
    in AL, DX                                 ; ec                          ; 0xc0d72 vgabios.c:488
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc0d73
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc0d75
    mov al, cl                                ; 88 c8                       ; 0xc0d78 vgabios.c:491
    xor ah, ah                                ; 30 e4                       ; 0xc0d7a
    mov cx, strict word 0004dh                ; b9 4d 00                    ; 0xc0d7c
    imul cx                                   ; f7 e9                       ; 0xc0d7f
    mov cx, ax                                ; 89 c1                       ; 0xc0d81
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc0d83
    xor ah, ah                                ; 30 e4                       ; 0xc0d86
    mov dx, 00097h                            ; ba 97 00                    ; 0xc0d88
    imul dx                                   ; f7 ea                       ; 0xc0d8b
    add cx, ax                                ; 01 c1                       ; 0xc0d8d
    mov word [bp-00ah], cx                    ; 89 4e f6                    ; 0xc0d8f
    mov cl, byte [bp-00ch]                    ; 8a 4e f4                    ; 0xc0d92
    xor ch, ch                                ; 30 ed                       ; 0xc0d95
    mov ax, cx                                ; 89 c8                       ; 0xc0d97
    mov dx, strict word 0001ch                ; ba 1c 00                    ; 0xc0d99
    imul dx                                   ; f7 ea                       ; 0xc0d9c
    add ax, word [bp-00ah]                    ; 03 46 f6                    ; 0xc0d9e
    add ax, 00080h                            ; 05 80 00                    ; 0xc0da1
    mov al, ah                                ; 88 e0                       ; 0xc0da4
    cbw                                       ; 98                          ; 0xc0da6
    mov cx, ax                                ; 89 c1                       ; 0xc0da7
    cmp ax, strict word 0003fh                ; 3d 3f 00                    ; 0xc0da9 vgabios.c:493
    jbe short 00db1h                          ; 76 03                       ; 0xc0dac
    mov cx, strict word 0003fh                ; b9 3f 00                    ; 0xc0dae
    mov al, bl                                ; 88 d8                       ; 0xc0db1 vgabios.c:496
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc0db3
    out DX, AL                                ; ee                          ; 0xc0db6
    mov al, cl                                ; 88 c8                       ; 0xc0db7 vgabios.c:498
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc0db9
    out DX, AL                                ; ee                          ; 0xc0dbc
    out DX, AL                                ; ee                          ; 0xc0dbd vgabios.c:499
    out DX, AL                                ; ee                          ; 0xc0dbe vgabios.c:500
    inc bx                                    ; 43                          ; 0xc0dbf vgabios.c:501
    inc si                                    ; 46                          ; 0xc0dc0 vgabios.c:502
    jmp short 00d5ah                          ; eb 97                       ; 0xc0dc1
    mov dx, 003dah                            ; ba da 03                    ; 0xc0dc3 vgabios.c:503
    in AL, DX                                 ; ec                          ; 0xc0dc6
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc0dc7
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc0dc9 vgabios.c:504
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0dcb
    out DX, AL                                ; ee                          ; 0xc0dce
    mov dx, 003dah                            ; ba da 03                    ; 0xc0dcf vgabios.c:506
    in AL, DX                                 ; ec                          ; 0xc0dd2
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc0dd3
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc0dd5 vgabios.c:508
    pop di                                    ; 5f                          ; 0xc0dd8
    pop si                                    ; 5e                          ; 0xc0dd9
    pop cx                                    ; 59                          ; 0xc0dda
    pop bx                                    ; 5b                          ; 0xc0ddb
    pop bp                                    ; 5d                          ; 0xc0ddc
    retn                                      ; c3                          ; 0xc0ddd
  ; disGetNextSymbol 0xc0dde LB 0x2f01 -> off=0x0 cb=00000000000000b3 uValue=00000000000c0dde 'biosfn_set_cursor_shape'
biosfn_set_cursor_shape:                     ; 0xc0dde LB 0xb3
    push bp                                   ; 55                          ; 0xc0dde vgabios.c:511
    mov bp, sp                                ; 89 e5                       ; 0xc0ddf
    push bx                                   ; 53                          ; 0xc0de1
    push cx                                   ; 51                          ; 0xc0de2
    push si                                   ; 56                          ; 0xc0de3
    push di                                   ; 57                          ; 0xc0de4
    push ax                                   ; 50                          ; 0xc0de5
    push ax                                   ; 50                          ; 0xc0de6
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc0de7
    mov ch, dl                                ; 88 d5                       ; 0xc0dea
    and byte [bp-00ah], 03fh                  ; 80 66 f6 3f                 ; 0xc0dec vgabios.c:515
    and ch, 01fh                              ; 80 e5 1f                    ; 0xc0df0 vgabios.c:516
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc0df3 vgabios.c:518
    xor ah, ah                                ; 30 e4                       ; 0xc0df6
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc0df8
    mov bh, byte [bp-00ch]                    ; 8a 7e f4                    ; 0xc0dfb
    mov al, ch                                ; 88 e8                       ; 0xc0dfe
    mov si, ax                                ; 89 c6                       ; 0xc0e00
    mov bl, ch                                ; 88 eb                       ; 0xc0e02
    mov dx, strict word 00060h                ; ba 60 00                    ; 0xc0e04 vgabios.c:519
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0e07
    call 031e8h                               ; e8 db 23                    ; 0xc0e0a
    mov dx, 00089h                            ; ba 89 00                    ; 0xc0e0d vgabios.c:521
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0e10
    call 031beh                               ; e8 a8 23                    ; 0xc0e13
    mov cl, al                                ; 88 c1                       ; 0xc0e16
    mov dx, 00085h                            ; ba 85 00                    ; 0xc0e18 vgabios.c:522
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0e1b
    call 031dah                               ; e8 b9 23                    ; 0xc0e1e
    mov bx, ax                                ; 89 c3                       ; 0xc0e21
    mov di, ax                                ; 89 c7                       ; 0xc0e23
    test cl, 001h                             ; f6 c1 01                    ; 0xc0e25 vgabios.c:523
    je short 00e65h                           ; 74 3b                       ; 0xc0e28
    cmp ax, strict word 00008h                ; 3d 08 00                    ; 0xc0e2a
    jbe short 00e65h                          ; 76 36                       ; 0xc0e2d
    cmp ch, 008h                              ; 80 fd 08                    ; 0xc0e2f
    jnc short 00e65h                          ; 73 31                       ; 0xc0e32
    cmp byte [bp-00ah], 020h                  ; 80 7e f6 20                 ; 0xc0e34
    jnc short 00e65h                          ; 73 2b                       ; 0xc0e38
    mov ax, word [bp-00ch]                    ; 8b 46 f4                    ; 0xc0e3a vgabios.c:525
    inc ax                                    ; 40                          ; 0xc0e3d
    cmp si, ax                                ; 39 c6                       ; 0xc0e3e
    je short 00e4bh                           ; 74 09                       ; 0xc0e40
    mul bx                                    ; f7 e3                       ; 0xc0e42 vgabios.c:527
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc0e44
    shr ax, CL                                ; d3 e8                       ; 0xc0e46
    dec ax                                    ; 48                          ; 0xc0e48
    jmp short 00e54h                          ; eb 09                       ; 0xc0e49 vgabios.c:529
    inc ax                                    ; 40                          ; 0xc0e4b vgabios.c:531
    mul bx                                    ; f7 e3                       ; 0xc0e4c
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc0e4e
    shr ax, CL                                ; d3 e8                       ; 0xc0e50
    dec ax                                    ; 48                          ; 0xc0e52
    dec ax                                    ; 48                          ; 0xc0e53
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc0e54
    mov al, ch                                ; 88 e8                       ; 0xc0e57 vgabios.c:533
    xor ah, ah                                ; 30 e4                       ; 0xc0e59
    inc ax                                    ; 40                          ; 0xc0e5b
    mul di                                    ; f7 e7                       ; 0xc0e5c
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc0e5e
    shr ax, CL                                ; d3 e8                       ; 0xc0e60
    dec ax                                    ; 48                          ; 0xc0e62
    mov ch, al                                ; 88 c5                       ; 0xc0e63
    mov dx, strict word 00063h                ; ba 63 00                    ; 0xc0e65 vgabios.c:537
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0e68
    call 031dah                               ; e8 6c 23                    ; 0xc0e6b
    mov bx, ax                                ; 89 c3                       ; 0xc0e6e
    mov AL, strict byte 00ah                  ; b0 0a                       ; 0xc0e70 vgabios.c:538
    mov dx, bx                                ; 89 da                       ; 0xc0e72
    out DX, AL                                ; ee                          ; 0xc0e74
    lea si, [bx+001h]                         ; 8d 77 01                    ; 0xc0e75 vgabios.c:539
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc0e78
    mov dx, si                                ; 89 f2                       ; 0xc0e7b
    out DX, AL                                ; ee                          ; 0xc0e7d
    mov AL, strict byte 00bh                  ; b0 0b                       ; 0xc0e7e vgabios.c:540
    mov dx, bx                                ; 89 da                       ; 0xc0e80
    out DX, AL                                ; ee                          ; 0xc0e82
    mov al, ch                                ; 88 e8                       ; 0xc0e83 vgabios.c:541
    mov dx, si                                ; 89 f2                       ; 0xc0e85
    out DX, AL                                ; ee                          ; 0xc0e87
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc0e88 vgabios.c:542
    pop di                                    ; 5f                          ; 0xc0e8b
    pop si                                    ; 5e                          ; 0xc0e8c
    pop cx                                    ; 59                          ; 0xc0e8d
    pop bx                                    ; 5b                          ; 0xc0e8e
    pop bp                                    ; 5d                          ; 0xc0e8f
    retn                                      ; c3                          ; 0xc0e90
  ; disGetNextSymbol 0xc0e91 LB 0x2e4e -> off=0x0 cb=00000000000000a3 uValue=00000000000c0e91 'biosfn_set_cursor_pos'
biosfn_set_cursor_pos:                       ; 0xc0e91 LB 0xa3
    push bp                                   ; 55                          ; 0xc0e91 vgabios.c:545
    mov bp, sp                                ; 89 e5                       ; 0xc0e92
    push bx                                   ; 53                          ; 0xc0e94
    push cx                                   ; 51                          ; 0xc0e95
    push si                                   ; 56                          ; 0xc0e96
    push ax                                   ; 50                          ; 0xc0e97
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc0e98
    mov cx, dx                                ; 89 d1                       ; 0xc0e9b
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc0e9d vgabios.c:551
    jbe short 00ea4h                          ; 76 03                       ; 0xc0e9f
    jmp near 00f2ch                           ; e9 88 00                    ; 0xc0ea1
    xor ah, ah                                ; 30 e4                       ; 0xc0ea4 vgabios.c:554
    mov dx, ax                                ; 89 c2                       ; 0xc0ea6
    sal dx, 1                                 ; d1 e2                       ; 0xc0ea8
    add dx, strict byte 00050h                ; 83 c2 50                    ; 0xc0eaa
    mov bx, cx                                ; 89 cb                       ; 0xc0ead
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0eaf
    call 031e8h                               ; e8 33 23                    ; 0xc0eb2
    mov dx, strict word 00062h                ; ba 62 00                    ; 0xc0eb5 vgabios.c:557
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0eb8
    call 031beh                               ; e8 00 23                    ; 0xc0ebb
    cmp al, byte [bp-008h]                    ; 3a 46 f8                    ; 0xc0ebe vgabios.c:558
    jne short 00f2ch                          ; 75 69                       ; 0xc0ec1
    mov dx, strict word 0004ah                ; ba 4a 00                    ; 0xc0ec3 vgabios.c:561
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0ec6
    call 031dah                               ; e8 0e 23                    ; 0xc0ec9
    mov bx, ax                                ; 89 c3                       ; 0xc0ecc
    mov dx, 00084h                            ; ba 84 00                    ; 0xc0ece vgabios.c:562
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0ed1
    call 031beh                               ; e8 e7 22                    ; 0xc0ed4
    xor ah, ah                                ; 30 e4                       ; 0xc0ed7
    mov dx, ax                                ; 89 c2                       ; 0xc0ed9
    inc dx                                    ; 42                          ; 0xc0edb
    mov ax, bx                                ; 89 d8                       ; 0xc0edc vgabios.c:567
    mul dx                                    ; f7 e2                       ; 0xc0ede
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc0ee0
    mov dx, ax                                ; 89 c2                       ; 0xc0ee2
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc0ee4
    xor ah, ah                                ; 30 e4                       ; 0xc0ee7
    mov si, ax                                ; 89 c6                       ; 0xc0ee9
    mov ax, dx                                ; 89 d0                       ; 0xc0eeb
    inc ax                                    ; 40                          ; 0xc0eed
    mul si                                    ; f7 e6                       ; 0xc0eee
    mov dl, cl                                ; 88 ca                       ; 0xc0ef0
    xor dh, dh                                ; 30 f6                       ; 0xc0ef2
    mov si, ax                                ; 89 c6                       ; 0xc0ef4
    add si, dx                                ; 01 d6                       ; 0xc0ef6
    mov al, ch                                ; 88 e8                       ; 0xc0ef8
    xor ah, ah                                ; 30 e4                       ; 0xc0efa
    mul bx                                    ; f7 e3                       ; 0xc0efc
    add si, ax                                ; 01 c6                       ; 0xc0efe
    mov dx, strict word 00063h                ; ba 63 00                    ; 0xc0f00 vgabios.c:570
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0f03
    call 031dah                               ; e8 d1 22                    ; 0xc0f06
    mov bx, ax                                ; 89 c3                       ; 0xc0f09
    mov AL, strict byte 00eh                  ; b0 0e                       ; 0xc0f0b vgabios.c:571
    mov dx, bx                                ; 89 da                       ; 0xc0f0d
    out DX, AL                                ; ee                          ; 0xc0f0f
    mov cx, si                                ; 89 f1                       ; 0xc0f10 vgabios.c:572
    mov cl, ch                                ; 88 e9                       ; 0xc0f12
    xor ch, ch                                ; 30 ed                       ; 0xc0f14
    mov ax, cx                                ; 89 c8                       ; 0xc0f16
    lea cx, [bx+001h]                         ; 8d 4f 01                    ; 0xc0f18
    mov dx, cx                                ; 89 ca                       ; 0xc0f1b
    out DX, AL                                ; ee                          ; 0xc0f1d
    mov AL, strict byte 00fh                  ; b0 0f                       ; 0xc0f1e vgabios.c:573
    mov dx, bx                                ; 89 da                       ; 0xc0f20
    out DX, AL                                ; ee                          ; 0xc0f22
    and si, 000ffh                            ; 81 e6 ff 00                 ; 0xc0f23 vgabios.c:574
    mov ax, si                                ; 89 f0                       ; 0xc0f27
    mov dx, cx                                ; 89 ca                       ; 0xc0f29
    out DX, AL                                ; ee                          ; 0xc0f2b
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc0f2c vgabios.c:576
    pop si                                    ; 5e                          ; 0xc0f2f
    pop cx                                    ; 59                          ; 0xc0f30
    pop bx                                    ; 5b                          ; 0xc0f31
    pop bp                                    ; 5d                          ; 0xc0f32
    retn                                      ; c3                          ; 0xc0f33
  ; disGetNextSymbol 0xc0f34 LB 0x2dab -> off=0x0 cb=00000000000000e5 uValue=00000000000c0f34 'biosfn_set_active_page'
biosfn_set_active_page:                      ; 0xc0f34 LB 0xe5
    push bp                                   ; 55                          ; 0xc0f34 vgabios.c:579
    mov bp, sp                                ; 89 e5                       ; 0xc0f35
    push bx                                   ; 53                          ; 0xc0f37
    push cx                                   ; 51                          ; 0xc0f38
    push dx                                   ; 52                          ; 0xc0f39
    push si                                   ; 56                          ; 0xc0f3a
    push di                                   ; 57                          ; 0xc0f3b
    push ax                                   ; 50                          ; 0xc0f3c
    push ax                                   ; 50                          ; 0xc0f3d
    mov ch, al                                ; 88 c5                       ; 0xc0f3e
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc0f40 vgabios.c:585
    jnbe short 00f58h                         ; 77 14                       ; 0xc0f42
    mov dx, strict word 00049h                ; ba 49 00                    ; 0xc0f44 vgabios.c:588
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0f47
    call 031beh                               ; e8 71 22                    ; 0xc0f4a
    xor ah, ah                                ; 30 e4                       ; 0xc0f4d vgabios.c:589
    call 03193h                               ; e8 41 22                    ; 0xc0f4f
    mov cl, al                                ; 88 c1                       ; 0xc0f52
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc0f54 vgabios.c:590
    jne short 00f5bh                          ; 75 03                       ; 0xc0f56
    jmp near 0100fh                           ; e9 b4 00                    ; 0xc0f58
    mov al, ch                                ; 88 e8                       ; 0xc0f5b vgabios.c:593
    xor ah, ah                                ; 30 e4                       ; 0xc0f5d
    lea bx, [bp-00eh]                         ; 8d 5e f2                    ; 0xc0f5f
    lea dx, [bp-00ch]                         ; 8d 56 f4                    ; 0xc0f62
    call 00a8ch                               ; e8 24 fb                    ; 0xc0f65
    mov bl, cl                                ; 88 cb                       ; 0xc0f68 vgabios.c:595
    xor bh, bh                                ; 30 ff                       ; 0xc0f6a
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc0f6c
    mov si, bx                                ; 89 de                       ; 0xc0f6e
    sal si, CL                                ; d3 e6                       ; 0xc0f70
    cmp byte [si+04635h], 000h                ; 80 bc 35 46 00              ; 0xc0f72
    jne short 00fc0h                          ; 75 47                       ; 0xc0f77
    mov dx, strict word 0004ah                ; ba 4a 00                    ; 0xc0f79 vgabios.c:598
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0f7c
    call 031dah                               ; e8 58 22                    ; 0xc0f7f
    mov bx, ax                                ; 89 c3                       ; 0xc0f82
    mov dx, 00084h                            ; ba 84 00                    ; 0xc0f84 vgabios.c:599
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0f87
    call 031beh                               ; e8 31 22                    ; 0xc0f8a
    xor ah, ah                                ; 30 e4                       ; 0xc0f8d
    mov dx, ax                                ; 89 c2                       ; 0xc0f8f
    inc dx                                    ; 42                          ; 0xc0f91
    mov ax, bx                                ; 89 d8                       ; 0xc0f92 vgabios.c:602
    mul dx                                    ; f7 e2                       ; 0xc0f94
    mov si, ax                                ; 89 c6                       ; 0xc0f96
    mov dx, ax                                ; 89 c2                       ; 0xc0f98
    sal dx, 1                                 ; d1 e2                       ; 0xc0f9a
    or dl, 0ffh                               ; 80 ca ff                    ; 0xc0f9c
    mov al, ch                                ; 88 e8                       ; 0xc0f9f
    xor ah, ah                                ; 30 e4                       ; 0xc0fa1
    mov di, ax                                ; 89 c7                       ; 0xc0fa3
    mov ax, dx                                ; 89 d0                       ; 0xc0fa5
    inc ax                                    ; 40                          ; 0xc0fa7
    mul di                                    ; f7 e7                       ; 0xc0fa8
    mov bx, ax                                ; 89 c3                       ; 0xc0faa
    mov dx, strict word 0004eh                ; ba 4e 00                    ; 0xc0fac vgabios.c:603
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0faf
    call 031e8h                               ; e8 33 22                    ; 0xc0fb2
    or si, 000ffh                             ; 81 ce ff 00                 ; 0xc0fb5 vgabios.c:606
    lea ax, [si+001h]                         ; 8d 44 01                    ; 0xc0fb9
    mul di                                    ; f7 e7                       ; 0xc0fbc
    jmp short 00fd0h                          ; eb 10                       ; 0xc0fbe vgabios.c:608
    mov bl, byte [bx+046b4h]                  ; 8a 9f b4 46                 ; 0xc0fc0 vgabios.c:610
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc0fc4
    sal bx, CL                                ; d3 e3                       ; 0xc0fc6
    mov al, ch                                ; 88 e8                       ; 0xc0fc8
    xor ah, ah                                ; 30 e4                       ; 0xc0fca
    mul word [bx+046cbh]                      ; f7 a7 cb 46                 ; 0xc0fcc
    mov bx, ax                                ; 89 c3                       ; 0xc0fd0
    mov dx, strict word 00063h                ; ba 63 00                    ; 0xc0fd2 vgabios.c:614
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0fd5
    call 031dah                               ; e8 ff 21                    ; 0xc0fd8
    mov si, ax                                ; 89 c6                       ; 0xc0fdb
    mov AL, strict byte 00ch                  ; b0 0c                       ; 0xc0fdd vgabios.c:615
    mov dx, si                                ; 89 f2                       ; 0xc0fdf
    out DX, AL                                ; ee                          ; 0xc0fe1
    mov ax, bx                                ; 89 d8                       ; 0xc0fe2 vgabios.c:616
    mov al, bh                                ; 88 f8                       ; 0xc0fe4
    lea di, [si+001h]                         ; 8d 7c 01                    ; 0xc0fe6
    mov dx, di                                ; 89 fa                       ; 0xc0fe9
    out DX, AL                                ; ee                          ; 0xc0feb
    mov AL, strict byte 00dh                  ; b0 0d                       ; 0xc0fec vgabios.c:617
    mov dx, si                                ; 89 f2                       ; 0xc0fee
    out DX, AL                                ; ee                          ; 0xc0ff0
    mov al, bl                                ; 88 d8                       ; 0xc0ff1 vgabios.c:618
    mov dx, di                                ; 89 fa                       ; 0xc0ff3
    out DX, AL                                ; ee                          ; 0xc0ff5
    mov al, ch                                ; 88 e8                       ; 0xc0ff6 vgabios.c:621
    xor ah, bh                                ; 30 fc                       ; 0xc0ff8
    mov si, ax                                ; 89 c6                       ; 0xc0ffa
    mov bx, ax                                ; 89 c3                       ; 0xc0ffc
    mov dx, strict word 00062h                ; ba 62 00                    ; 0xc0ffe
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1001
    call 031cch                               ; e8 c5 21                    ; 0xc1004
    mov dx, word [bp-00eh]                    ; 8b 56 f2                    ; 0xc1007 vgabios.c:628
    mov ax, si                                ; 89 f0                       ; 0xc100a
    call 00e91h                               ; e8 82 fe                    ; 0xc100c
    lea sp, [bp-00ah]                         ; 8d 66 f6                    ; 0xc100f vgabios.c:629
    pop di                                    ; 5f                          ; 0xc1012
    pop si                                    ; 5e                          ; 0xc1013
    pop dx                                    ; 5a                          ; 0xc1014
    pop cx                                    ; 59                          ; 0xc1015
    pop bx                                    ; 5b                          ; 0xc1016
    pop bp                                    ; 5d                          ; 0xc1017
    retn                                      ; c3                          ; 0xc1018
  ; disGetNextSymbol 0xc1019 LB 0x2cc6 -> off=0x0 cb=00000000000003ea uValue=00000000000c1019 'biosfn_set_video_mode'
biosfn_set_video_mode:                       ; 0xc1019 LB 0x3ea
    push bp                                   ; 55                          ; 0xc1019 vgabios.c:649
    mov bp, sp                                ; 89 e5                       ; 0xc101a
    push bx                                   ; 53                          ; 0xc101c
    push cx                                   ; 51                          ; 0xc101d
    push dx                                   ; 52                          ; 0xc101e
    push si                                   ; 56                          ; 0xc101f
    push di                                   ; 57                          ; 0xc1020
    sub sp, strict byte 00014h                ; 83 ec 14                    ; 0xc1021
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc1024
    and AL, strict byte 080h                  ; 24 80                       ; 0xc1027 vgabios.c:653
    mov byte [bp-012h], al                    ; 88 46 ee                    ; 0xc1029
    call 007e8h                               ; e8 b9 f7                    ; 0xc102c vgabios.c:660
    test ax, ax                               ; 85 c0                       ; 0xc102f
    je short 0103fh                           ; 74 0c                       ; 0xc1031
    mov AL, strict byte 007h                  ; b0 07                       ; 0xc1033 vgabios.c:662
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc1035
    out DX, AL                                ; ee                          ; 0xc1038
    xor al, al                                ; 30 c0                       ; 0xc1039 vgabios.c:663
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc103b
    out DX, AL                                ; ee                          ; 0xc103e
    and byte [bp-00ch], 07fh                  ; 80 66 f4 7f                 ; 0xc103f vgabios.c:668
    cmp byte [bp-00ch], 007h                  ; 80 7e f4 07                 ; 0xc1043 vgabios.c:672
    jne short 0104dh                          ; 75 04                       ; 0xc1047
    mov byte [bp-00ch], 000h                  ; c6 46 f4 00                 ; 0xc1049 vgabios.c:673
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc104d vgabios.c:676
    xor ah, ah                                ; 30 e4                       ; 0xc1050
    call 03193h                               ; e8 3e 21                    ; 0xc1052
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc1055
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc1058 vgabios.c:682
    jne short 0105fh                          ; 75 03                       ; 0xc105a
    jmp near 013f9h                           ; e9 9a 03                    ; 0xc105c
    mov byte [bp-014h], al                    ; 88 46 ec                    ; 0xc105f vgabios.c:685
    mov byte [bp-013h], 000h                  ; c6 46 ed 00                 ; 0xc1062
    mov bx, word [bp-014h]                    ; 8b 5e ec                    ; 0xc1066
    mov al, byte [bx+046b4h]                  ; 8a 87 b4 46                 ; 0xc1069
    mov byte [bp-00eh], al                    ; 88 46 f2                    ; 0xc106d
    mov bl, al                                ; 88 c3                       ; 0xc1070 vgabios.c:686
    xor bh, bh                                ; 30 ff                       ; 0xc1072
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc1074
    sal bx, CL                                ; d3 e3                       ; 0xc1076
    mov al, byte [bx+046c8h]                  ; 8a 87 c8 46                 ; 0xc1078
    xor ah, ah                                ; 30 e4                       ; 0xc107c
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc107e
    mov al, byte [bx+046c9h]                  ; 8a 87 c9 46                 ; 0xc1081 vgabios.c:687
    mov word [bp-01ch], ax                    ; 89 46 e4                    ; 0xc1085
    mov al, byte [bx+046cah]                  ; 8a 87 ca 46                 ; 0xc1088 vgabios.c:688
    mov word [bp-018h], ax                    ; 89 46 e8                    ; 0xc108c
    mov dx, 00087h                            ; ba 87 00                    ; 0xc108f vgabios.c:691
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1092
    call 031beh                               ; e8 26 21                    ; 0xc1095
    mov dx, 00088h                            ; ba 88 00                    ; 0xc1098 vgabios.c:694
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc109b
    call 031beh                               ; e8 1d 21                    ; 0xc109e
    mov dx, 00089h                            ; ba 89 00                    ; 0xc10a1 vgabios.c:697
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc10a4
    call 031beh                               ; e8 14 21                    ; 0xc10a7
    mov ch, al                                ; 88 c5                       ; 0xc10aa
    test AL, strict byte 008h                 ; a8 08                       ; 0xc10ac vgabios.c:703
    jne short 010f5h                          ; 75 45                       ; 0xc10ae
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc10b0 vgabios.c:705
    mov bx, word [bp-014h]                    ; 8b 5e ec                    ; 0xc10b2
    sal bx, CL                                ; d3 e3                       ; 0xc10b5
    mov al, byte [bx+0463ah]                  ; 8a 87 3a 46                 ; 0xc10b7
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc10bb
    out DX, AL                                ; ee                          ; 0xc10be
    xor al, al                                ; 30 c0                       ; 0xc10bf vgabios.c:708
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc10c1
    out DX, AL                                ; ee                          ; 0xc10c4
    mov bl, byte [bx+0463bh]                  ; 8a 9f 3b 46                 ; 0xc10c5 vgabios.c:711
    cmp bl, 001h                              ; 80 fb 01                    ; 0xc10c9
    jc short 010dbh                           ; 72 0d                       ; 0xc10cc
    jbe short 010e4h                          ; 76 14                       ; 0xc10ce
    cmp bl, cl                                ; 38 cb                       ; 0xc10d0
    je short 010eeh                           ; 74 1a                       ; 0xc10d2
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc10d4
    je short 010e9h                           ; 74 10                       ; 0xc10d7
    jmp short 010f1h                          ; eb 16                       ; 0xc10d9
    test bl, bl                               ; 84 db                       ; 0xc10db
    jne short 010f1h                          ; 75 12                       ; 0xc10dd
    mov di, 04e48h                            ; bf 48 4e                    ; 0xc10df vgabios.c:713
    jmp short 010f1h                          ; eb 0d                       ; 0xc10e2 vgabios.c:714
    mov di, 04f08h                            ; bf 08 4f                    ; 0xc10e4 vgabios.c:716
    jmp short 010f1h                          ; eb 08                       ; 0xc10e7 vgabios.c:717
    mov di, 04fc8h                            ; bf c8 4f                    ; 0xc10e9 vgabios.c:719
    jmp short 010f1h                          ; eb 03                       ; 0xc10ec vgabios.c:720
    mov di, 05088h                            ; bf 88 50                    ; 0xc10ee vgabios.c:722
    xor bx, bx                                ; 31 db                       ; 0xc10f1 vgabios.c:726
    jmp short 010fdh                          ; eb 08                       ; 0xc10f3
    jmp short 01149h                          ; eb 52                       ; 0xc10f5
    cmp bx, 00100h                            ; 81 fb 00 01                 ; 0xc10f7
    jnc short 0113ch                          ; 73 3f                       ; 0xc10fb
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc10fd vgabios.c:727
    xor ah, ah                                ; 30 e4                       ; 0xc1100
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc1102
    mov si, ax                                ; 89 c6                       ; 0xc1104
    sal si, CL                                ; d3 e6                       ; 0xc1106
    mov al, byte [si+0463bh]                  ; 8a 84 3b 46                 ; 0xc1108
    mov si, ax                                ; 89 c6                       ; 0xc110c
    mov al, byte [si+046c4h]                  ; 8a 84 c4 46                 ; 0xc110e
    cmp bx, ax                                ; 39 c3                       ; 0xc1112
    jnbe short 01131h                         ; 77 1b                       ; 0xc1114
    mov ax, bx                                ; 89 d8                       ; 0xc1116 vgabios.c:728
    mov dx, strict word 00003h                ; ba 03 00                    ; 0xc1118
    mul dx                                    ; f7 e2                       ; 0xc111b
    mov si, di                                ; 89 fe                       ; 0xc111d
    add si, ax                                ; 01 c6                       ; 0xc111f
    mov al, byte [si]                         ; 8a 04                       ; 0xc1121
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc1123
    out DX, AL                                ; ee                          ; 0xc1126
    mov al, byte [si+001h]                    ; 8a 44 01                    ; 0xc1127 vgabios.c:729
    out DX, AL                                ; ee                          ; 0xc112a
    mov al, byte [si+002h]                    ; 8a 44 02                    ; 0xc112b vgabios.c:730
    out DX, AL                                ; ee                          ; 0xc112e
    jmp short 01139h                          ; eb 08                       ; 0xc112f vgabios.c:732
    xor al, al                                ; 30 c0                       ; 0xc1131 vgabios.c:733
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc1133
    out DX, AL                                ; ee                          ; 0xc1136
    out DX, AL                                ; ee                          ; 0xc1137 vgabios.c:734
    out DX, AL                                ; ee                          ; 0xc1138 vgabios.c:735
    inc bx                                    ; 43                          ; 0xc1139 vgabios.c:737
    jmp short 010f7h                          ; eb bb                       ; 0xc113a
    test ch, 002h                             ; f6 c5 02                    ; 0xc113c vgabios.c:738
    je short 01149h                           ; 74 08                       ; 0xc113f
    mov dx, 00100h                            ; ba 00 01                    ; 0xc1141 vgabios.c:740
    xor ax, ax                                ; 31 c0                       ; 0xc1144
    call 00d3fh                               ; e8 f6 fb                    ; 0xc1146
    mov dx, 003dah                            ; ba da 03                    ; 0xc1149 vgabios.c:745
    in AL, DX                                 ; ec                          ; 0xc114c
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc114d
    xor bx, bx                                ; 31 db                       ; 0xc114f vgabios.c:748
    jmp short 01158h                          ; eb 05                       ; 0xc1151
    cmp bx, strict byte 00013h                ; 83 fb 13                    ; 0xc1153
    jnbe short 01173h                         ; 77 1b                       ; 0xc1156
    mov al, bl                                ; 88 d8                       ; 0xc1158 vgabios.c:749
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc115a
    out DX, AL                                ; ee                          ; 0xc115d
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc115e vgabios.c:750
    xor ah, ah                                ; 30 e4                       ; 0xc1161
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc1163
    mov si, ax                                ; 89 c6                       ; 0xc1165
    sal si, CL                                ; d3 e6                       ; 0xc1167
    add si, bx                                ; 01 de                       ; 0xc1169
    mov al, byte [si+046ebh]                  ; 8a 84 eb 46                 ; 0xc116b
    out DX, AL                                ; ee                          ; 0xc116f
    inc bx                                    ; 43                          ; 0xc1170 vgabios.c:751
    jmp short 01153h                          ; eb e0                       ; 0xc1171
    mov AL, strict byte 014h                  ; b0 14                       ; 0xc1173 vgabios.c:752
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc1175
    out DX, AL                                ; ee                          ; 0xc1178
    xor al, al                                ; 30 c0                       ; 0xc1179 vgabios.c:753
    out DX, AL                                ; ee                          ; 0xc117b
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc117c vgabios.c:756
    out DX, AL                                ; ee                          ; 0xc117f
    mov AL, strict byte 003h                  ; b0 03                       ; 0xc1180 vgabios.c:757
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc1182
    out DX, AL                                ; ee                          ; 0xc1185
    mov bx, strict word 00001h                ; bb 01 00                    ; 0xc1186 vgabios.c:758
    jmp short 01190h                          ; eb 05                       ; 0xc1189
    cmp bx, strict byte 00004h                ; 83 fb 04                    ; 0xc118b
    jnbe short 011aeh                         ; 77 1e                       ; 0xc118e
    mov al, bl                                ; 88 d8                       ; 0xc1190 vgabios.c:759
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc1192
    out DX, AL                                ; ee                          ; 0xc1195
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1196 vgabios.c:760
    xor ah, ah                                ; 30 e4                       ; 0xc1199
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc119b
    mov si, ax                                ; 89 c6                       ; 0xc119d
    sal si, CL                                ; d3 e6                       ; 0xc119f
    add si, bx                                ; 01 de                       ; 0xc11a1
    mov al, byte [si+046cch]                  ; 8a 84 cc 46                 ; 0xc11a3
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc11a7
    out DX, AL                                ; ee                          ; 0xc11aa
    inc bx                                    ; 43                          ; 0xc11ab vgabios.c:761
    jmp short 0118bh                          ; eb dd                       ; 0xc11ac
    xor bx, bx                                ; 31 db                       ; 0xc11ae vgabios.c:764
    jmp short 011b7h                          ; eb 05                       ; 0xc11b0
    cmp bx, strict byte 00008h                ; 83 fb 08                    ; 0xc11b2
    jnbe short 011d5h                         ; 77 1e                       ; 0xc11b5
    mov al, bl                                ; 88 d8                       ; 0xc11b7 vgabios.c:765
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc11b9
    out DX, AL                                ; ee                          ; 0xc11bc
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc11bd vgabios.c:766
    xor ah, ah                                ; 30 e4                       ; 0xc11c0
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc11c2
    mov si, ax                                ; 89 c6                       ; 0xc11c4
    sal si, CL                                ; d3 e6                       ; 0xc11c6
    add si, bx                                ; 01 de                       ; 0xc11c8
    mov al, byte [si+046ffh]                  ; 8a 84 ff 46                 ; 0xc11ca
    mov dx, 003cfh                            ; ba cf 03                    ; 0xc11ce
    out DX, AL                                ; ee                          ; 0xc11d1
    inc bx                                    ; 43                          ; 0xc11d2 vgabios.c:767
    jmp short 011b2h                          ; eb dd                       ; 0xc11d3
    mov bl, byte [bp-010h]                    ; 8a 5e f0                    ; 0xc11d5 vgabios.c:770
    xor bh, bh                                ; 30 ff                       ; 0xc11d8
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc11da
    sal bx, CL                                ; d3 e3                       ; 0xc11dc
    cmp byte [bx+04636h], 001h                ; 80 bf 36 46 01              ; 0xc11de
    jne short 011eah                          ; 75 05                       ; 0xc11e3
    mov dx, 003b4h                            ; ba b4 03                    ; 0xc11e5
    jmp short 011edh                          ; eb 03                       ; 0xc11e8
    mov dx, 003d4h                            ; ba d4 03                    ; 0xc11ea
    mov si, dx                                ; 89 d6                       ; 0xc11ed
    mov ax, strict word 00011h                ; b8 11 00                    ; 0xc11ef vgabios.c:773
    out DX, ax                                ; ef                          ; 0xc11f2
    xor bx, bx                                ; 31 db                       ; 0xc11f3 vgabios.c:775
    jmp short 011fch                          ; eb 05                       ; 0xc11f5
    cmp bx, strict byte 00018h                ; 83 fb 18                    ; 0xc11f7
    jnbe short 0121bh                         ; 77 1f                       ; 0xc11fa
    mov al, bl                                ; 88 d8                       ; 0xc11fc vgabios.c:776
    mov dx, si                                ; 89 f2                       ; 0xc11fe
    out DX, AL                                ; ee                          ; 0xc1200
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1201 vgabios.c:777
    xor ah, ah                                ; 30 e4                       ; 0xc1204
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc1206
    sal ax, CL                                ; d3 e0                       ; 0xc1208
    mov cx, ax                                ; 89 c1                       ; 0xc120a
    mov di, ax                                ; 89 c7                       ; 0xc120c
    add di, bx                                ; 01 df                       ; 0xc120e
    lea dx, [si+001h]                         ; 8d 54 01                    ; 0xc1210
    mov al, byte [di+046d2h]                  ; 8a 85 d2 46                 ; 0xc1213
    out DX, AL                                ; ee                          ; 0xc1217
    inc bx                                    ; 43                          ; 0xc1218 vgabios.c:778
    jmp short 011f7h                          ; eb dc                       ; 0xc1219
    mov bx, cx                                ; 89 cb                       ; 0xc121b vgabios.c:781
    mov al, byte [bx+046d1h]                  ; 8a 87 d1 46                 ; 0xc121d
    mov dx, 003c2h                            ; ba c2 03                    ; 0xc1221
    out DX, AL                                ; ee                          ; 0xc1224
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc1225 vgabios.c:784
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc1227
    out DX, AL                                ; ee                          ; 0xc122a
    mov dx, 003dah                            ; ba da 03                    ; 0xc122b vgabios.c:785
    in AL, DX                                 ; ec                          ; 0xc122e
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc122f
    cmp byte [bp-012h], 000h                  ; 80 7e ee 00                 ; 0xc1231 vgabios.c:787
    jne short 01298h                          ; 75 61                       ; 0xc1235
    mov bl, byte [bp-010h]                    ; 8a 5e f0                    ; 0xc1237 vgabios.c:789
    xor bh, ch                                ; 30 ef                       ; 0xc123a
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc123c
    sal bx, CL                                ; d3 e3                       ; 0xc123e
    cmp byte [bx+04635h], 000h                ; 80 bf 35 46 00              ; 0xc1240
    jne short 0125ah                          ; 75 13                       ; 0xc1245
    mov es, [bx+04638h]                       ; 8e 87 38 46                 ; 0xc1247 vgabios.c:791
    mov cx, 04000h                            ; b9 00 40                    ; 0xc124b
    mov ax, 00720h                            ; b8 20 07                    ; 0xc124e
    xor di, di                                ; 31 ff                       ; 0xc1251
    cld                                       ; fc                          ; 0xc1253
    jcxz 01258h                               ; e3 02                       ; 0xc1254
    rep stosw                                 ; f3 ab                       ; 0xc1256
    jmp short 01298h                          ; eb 3e                       ; 0xc1258 vgabios.c:793
    cmp byte [bp-00ch], 00dh                  ; 80 7e f4 0d                 ; 0xc125a vgabios.c:795
    jnc short 01272h                          ; 73 12                       ; 0xc125e
    mov es, [bx+04638h]                       ; 8e 87 38 46                 ; 0xc1260 vgabios.c:797
    mov cx, 04000h                            ; b9 00 40                    ; 0xc1264
    xor ax, ax                                ; 31 c0                       ; 0xc1267
    xor di, di                                ; 31 ff                       ; 0xc1269
    cld                                       ; fc                          ; 0xc126b
    jcxz 01270h                               ; e3 02                       ; 0xc126c
    rep stosw                                 ; f3 ab                       ; 0xc126e
    jmp short 01298h                          ; eb 26                       ; 0xc1270 vgabios.c:799
    mov AL, strict byte 002h                  ; b0 02                       ; 0xc1272 vgabios.c:801
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc1274
    out DX, AL                                ; ee                          ; 0xc1277
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc1278 vgabios.c:802
    in AL, DX                                 ; ec                          ; 0xc127b
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc127c
    mov word [bp-01eh], ax                    ; 89 46 e2                    ; 0xc127e
    mov AL, strict byte 00fh                  ; b0 0f                       ; 0xc1281 vgabios.c:803
    out DX, AL                                ; ee                          ; 0xc1283
    mov es, [bx+04638h]                       ; 8e 87 38 46                 ; 0xc1284 vgabios.c:804
    mov cx, 08000h                            ; b9 00 80                    ; 0xc1288
    xor ax, ax                                ; 31 c0                       ; 0xc128b
    xor di, di                                ; 31 ff                       ; 0xc128d
    cld                                       ; fc                          ; 0xc128f
    jcxz 01294h                               ; e3 02                       ; 0xc1290
    rep stosw                                 ; f3 ab                       ; 0xc1292
    mov al, byte [bp-01eh]                    ; 8a 46 e2                    ; 0xc1294 vgabios.c:805
    out DX, AL                                ; ee                          ; 0xc1297
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1298 vgabios.c:811
    mov byte [bp-01ah], al                    ; 88 46 e6                    ; 0xc129b
    mov byte [bp-019h], 000h                  ; c6 46 e7 00                 ; 0xc129e
    mov bx, word [bp-01ah]                    ; 8b 5e e6                    ; 0xc12a2
    mov dx, strict word 00049h                ; ba 49 00                    ; 0xc12a5
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc12a8
    call 031cch                               ; e8 1e 1f                    ; 0xc12ab
    mov bx, word [bp-016h]                    ; 8b 5e ea                    ; 0xc12ae vgabios.c:812
    mov dx, strict word 0004ah                ; ba 4a 00                    ; 0xc12b1
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc12b4
    call 031e8h                               ; e8 2e 1f                    ; 0xc12b7
    mov bl, byte [bp-00eh]                    ; 8a 5e f2                    ; 0xc12ba vgabios.c:813
    xor bh, bh                                ; 30 ff                       ; 0xc12bd
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc12bf
    sal bx, CL                                ; d3 e3                       ; 0xc12c1
    mov bx, word [bx+046cbh]                  ; 8b 9f cb 46                 ; 0xc12c3
    mov dx, strict word 0004ch                ; ba 4c 00                    ; 0xc12c7
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc12ca
    call 031e8h                               ; e8 18 1f                    ; 0xc12cd
    mov bx, si                                ; 89 f3                       ; 0xc12d0 vgabios.c:814
    mov dx, strict word 00063h                ; ba 63 00                    ; 0xc12d2
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc12d5
    call 031e8h                               ; e8 0d 1f                    ; 0xc12d8
    mov bl, byte [bp-01ch]                    ; 8a 5e e4                    ; 0xc12db vgabios.c:815
    xor bh, bh                                ; 30 ff                       ; 0xc12de
    mov dx, 00084h                            ; ba 84 00                    ; 0xc12e0
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc12e3
    call 031cch                               ; e8 e3 1e                    ; 0xc12e6
    mov bx, word [bp-018h]                    ; 8b 5e e8                    ; 0xc12e9 vgabios.c:816
    mov dx, 00085h                            ; ba 85 00                    ; 0xc12ec
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc12ef
    call 031e8h                               ; e8 f3 1e                    ; 0xc12f2
    mov bl, byte [bp-012h]                    ; 8a 5e ee                    ; 0xc12f5 vgabios.c:817
    or bl, 060h                               ; 80 cb 60                    ; 0xc12f8
    xor bh, bh                                ; 30 ff                       ; 0xc12fb
    mov dx, 00087h                            ; ba 87 00                    ; 0xc12fd
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1300
    call 031cch                               ; e8 c6 1e                    ; 0xc1303
    mov bx, 000f9h                            ; bb f9 00                    ; 0xc1306 vgabios.c:818
    mov dx, 00088h                            ; ba 88 00                    ; 0xc1309
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc130c
    call 031cch                               ; e8 ba 1e                    ; 0xc130f
    mov dx, 00089h                            ; ba 89 00                    ; 0xc1312 vgabios.c:819
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1315
    call 031beh                               ; e8 a3 1e                    ; 0xc1318
    mov bl, al                                ; 88 c3                       ; 0xc131b
    and bl, 07fh                              ; 80 e3 7f                    ; 0xc131d
    xor bh, bh                                ; 30 ff                       ; 0xc1320
    mov dx, 00089h                            ; ba 89 00                    ; 0xc1322
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1325
    call 031cch                               ; e8 a1 1e                    ; 0xc1328
    mov bx, strict word 00008h                ; bb 08 00                    ; 0xc132b vgabios.c:822
    mov dx, 0008ah                            ; ba 8a 00                    ; 0xc132e
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1331
    call 031cch                               ; e8 95 1e                    ; 0xc1334
    mov cx, ds                                ; 8c d9                       ; 0xc1337 vgabios.c:823
    mov bx, 053d6h                            ; bb d6 53                    ; 0xc1339
    mov dx, 000a8h                            ; ba a8 00                    ; 0xc133c
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc133f
    call 03208h                               ; e8 c3 1e                    ; 0xc1342
    cmp byte [bp-00ch], 007h                  ; 80 7e f4 07                 ; 0xc1345 vgabios.c:825
    jnbe short 01376h                         ; 77 2b                       ; 0xc1349
    mov bx, word [bp-01ah]                    ; 8b 5e e6                    ; 0xc134b vgabios.c:827
    mov bl, byte [bx+07c63h]                  ; 8a 9f 63 7c                 ; 0xc134e
    xor bh, bh                                ; 30 ff                       ; 0xc1352
    mov dx, strict word 00065h                ; ba 65 00                    ; 0xc1354
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1357
    call 031cch                               ; e8 6f 1e                    ; 0xc135a
    cmp byte [bp-00ch], 006h                  ; 80 7e f4 06                 ; 0xc135d vgabios.c:828
    jne short 01368h                          ; 75 05                       ; 0xc1361
    mov bx, strict word 0003fh                ; bb 3f 00                    ; 0xc1363
    jmp short 0136bh                          ; eb 03                       ; 0xc1366
    mov bx, strict word 00030h                ; bb 30 00                    ; 0xc1368
    xor bh, bh                                ; 30 ff                       ; 0xc136b
    mov dx, strict word 00066h                ; ba 66 00                    ; 0xc136d
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1370
    call 031cch                               ; e8 56 1e                    ; 0xc1373
    mov bl, byte [bp-010h]                    ; 8a 5e f0                    ; 0xc1376 vgabios.c:832
    xor bh, bh                                ; 30 ff                       ; 0xc1379
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc137b
    sal bx, CL                                ; d3 e3                       ; 0xc137d
    cmp byte [bx+04635h], 000h                ; 80 bf 35 46 00              ; 0xc137f
    jne short 0138fh                          ; 75 09                       ; 0xc1384
    mov dx, strict word 00007h                ; ba 07 00                    ; 0xc1386 vgabios.c:834
    mov ax, strict word 00006h                ; b8 06 00                    ; 0xc1389
    call 00ddeh                               ; e8 4f fa                    ; 0xc138c
    xor bx, bx                                ; 31 db                       ; 0xc138f vgabios.c:838
    jmp short 01398h                          ; eb 05                       ; 0xc1391
    cmp bx, strict byte 00008h                ; 83 fb 08                    ; 0xc1393
    jnc short 013a4h                          ; 73 0c                       ; 0xc1396
    mov al, bl                                ; 88 d8                       ; 0xc1398 vgabios.c:839
    xor ah, ah                                ; 30 e4                       ; 0xc139a
    xor dx, dx                                ; 31 d2                       ; 0xc139c
    call 00e91h                               ; e8 f0 fa                    ; 0xc139e
    inc bx                                    ; 43                          ; 0xc13a1
    jmp short 01393h                          ; eb ef                       ; 0xc13a2
    xor ax, ax                                ; 31 c0                       ; 0xc13a4 vgabios.c:842
    call 00f34h                               ; e8 8b fb                    ; 0xc13a6
    mov bl, byte [bp-010h]                    ; 8a 5e f0                    ; 0xc13a9 vgabios.c:845
    xor bh, bh                                ; 30 ff                       ; 0xc13ac
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc13ae
    sal bx, CL                                ; d3 e3                       ; 0xc13b0
    cmp byte [bx+04635h], 000h                ; 80 bf 35 46 00              ; 0xc13b2
    jne short 013c9h                          ; 75 10                       ; 0xc13b7
    xor bl, bl                                ; 30 db                       ; 0xc13b9 vgabios.c:847
    mov AL, strict byte 004h                  ; b0 04                       ; 0xc13bb
    mov AH, strict byte 011h                  ; b4 11                       ; 0xc13bd
    int 010h                                  ; cd 10                       ; 0xc13bf
    xor bl, bl                                ; 30 db                       ; 0xc13c1 vgabios.c:848
    mov al, cl                                ; 88 c8                       ; 0xc13c3
    mov AH, strict byte 011h                  ; b4 11                       ; 0xc13c5
    int 010h                                  ; cd 10                       ; 0xc13c7
    mov dx, 057f2h                            ; ba f2 57                    ; 0xc13c9 vgabios.c:852
    mov ax, strict word 0001fh                ; b8 1f 00                    ; 0xc13cc
    call 00a00h                               ; e8 2e f6                    ; 0xc13cf
    mov ax, word [bp-018h]                    ; 8b 46 e8                    ; 0xc13d2 vgabios.c:854
    cmp ax, strict word 00010h                ; 3d 10 00                    ; 0xc13d5
    je short 013f4h                           ; 74 1a                       ; 0xc13d8
    cmp ax, strict word 0000eh                ; 3d 0e 00                    ; 0xc13da
    je short 013efh                           ; 74 10                       ; 0xc13dd
    cmp ax, strict word 00008h                ; 3d 08 00                    ; 0xc13df
    jne short 013f9h                          ; 75 15                       ; 0xc13e2
    mov dx, 053f2h                            ; ba f2 53                    ; 0xc13e4 vgabios.c:856
    mov ax, strict word 00043h                ; b8 43 00                    ; 0xc13e7
    call 00a00h                               ; e8 13 f6                    ; 0xc13ea
    jmp short 013f9h                          ; eb 0a                       ; 0xc13ed vgabios.c:857
    mov dx, 05bf2h                            ; ba f2 5b                    ; 0xc13ef vgabios.c:859
    jmp short 013e7h                          ; eb f3                       ; 0xc13f2
    mov dx, 069f2h                            ; ba f2 69                    ; 0xc13f4 vgabios.c:862
    jmp short 013e7h                          ; eb ee                       ; 0xc13f7
    lea sp, [bp-00ah]                         ; 8d 66 f6                    ; 0xc13f9 vgabios.c:865
    pop di                                    ; 5f                          ; 0xc13fc
    pop si                                    ; 5e                          ; 0xc13fd
    pop dx                                    ; 5a                          ; 0xc13fe
    pop cx                                    ; 59                          ; 0xc13ff
    pop bx                                    ; 5b                          ; 0xc1400
    pop bp                                    ; 5d                          ; 0xc1401
    retn                                      ; c3                          ; 0xc1402
  ; disGetNextSymbol 0xc1403 LB 0x28dc -> off=0x0 cb=000000000000008f uValue=00000000000c1403 'vgamem_copy_pl4'
vgamem_copy_pl4:                             ; 0xc1403 LB 0x8f
    push bp                                   ; 55                          ; 0xc1403 vgabios.c:868
    mov bp, sp                                ; 89 e5                       ; 0xc1404
    push si                                   ; 56                          ; 0xc1406
    push di                                   ; 57                          ; 0xc1407
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc1408
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc140b
    mov al, dl                                ; 88 d0                       ; 0xc140e
    mov byte [bp-00ah], bl                    ; 88 5e f6                    ; 0xc1410
    mov byte [bp-006h], cl                    ; 88 4e fa                    ; 0xc1413
    xor ah, ah                                ; 30 e4                       ; 0xc1416 vgabios.c:874
    mov dl, byte [bp+006h]                    ; 8a 56 06                    ; 0xc1418
    xor dh, dh                                ; 30 f6                       ; 0xc141b
    mov cx, dx                                ; 89 d1                       ; 0xc141d
    imul dx                                   ; f7 ea                       ; 0xc141f
    mov dl, byte [bp+004h]                    ; 8a 56 04                    ; 0xc1421
    xor dh, dh                                ; 30 f6                       ; 0xc1424
    mov si, dx                                ; 89 d6                       ; 0xc1426
    imul dx                                   ; f7 ea                       ; 0xc1428
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc142a
    xor dh, dh                                ; 30 f6                       ; 0xc142d
    mov bx, dx                                ; 89 d3                       ; 0xc142f
    add ax, dx                                ; 01 d0                       ; 0xc1431
    mov word [bp-00eh], ax                    ; 89 46 f2                    ; 0xc1433
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc1436 vgabios.c:875
    xor ah, ah                                ; 30 e4                       ; 0xc1439
    imul cx                                   ; f7 e9                       ; 0xc143b
    imul si                                   ; f7 ee                       ; 0xc143d
    add ax, bx                                ; 01 d8                       ; 0xc143f
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc1441
    mov ax, 00105h                            ; b8 05 01                    ; 0xc1444 vgabios.c:876
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1447
    out DX, ax                                ; ef                          ; 0xc144a
    xor bl, bl                                ; 30 db                       ; 0xc144b vgabios.c:877
    cmp bl, byte [bp+006h]                    ; 3a 5e 06                    ; 0xc144d
    jnc short 01482h                          ; 73 30                       ; 0xc1450
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1452 vgabios.c:879
    xor ah, ah                                ; 30 e4                       ; 0xc1455
    mov cx, ax                                ; 89 c1                       ; 0xc1457
    mov al, bl                                ; 88 d8                       ; 0xc1459
    mov dx, ax                                ; 89 c2                       ; 0xc145b
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc145d
    mov si, ax                                ; 89 c6                       ; 0xc1460
    mov ax, dx                                ; 89 d0                       ; 0xc1462
    imul si                                   ; f7 ee                       ; 0xc1464
    mov si, word [bp-00eh]                    ; 8b 76 f2                    ; 0xc1466
    add si, ax                                ; 01 c6                       ; 0xc1469
    mov di, word [bp-00ch]                    ; 8b 7e f4                    ; 0xc146b
    add di, ax                                ; 01 c7                       ; 0xc146e
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc1470
    mov es, dx                                ; 8e c2                       ; 0xc1473
    cld                                       ; fc                          ; 0xc1475
    jcxz 0147eh                               ; e3 06                       ; 0xc1476
    push DS                                   ; 1e                          ; 0xc1478
    mov ds, dx                                ; 8e da                       ; 0xc1479
    rep movsb                                 ; f3 a4                       ; 0xc147b
    pop DS                                    ; 1f                          ; 0xc147d
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3                     ; 0xc147e vgabios.c:880
    jmp short 0144dh                          ; eb cb                       ; 0xc1480
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc1482 vgabios.c:881
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1485
    out DX, ax                                ; ef                          ; 0xc1488
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc1489 vgabios.c:882
    pop di                                    ; 5f                          ; 0xc148c
    pop si                                    ; 5e                          ; 0xc148d
    pop bp                                    ; 5d                          ; 0xc148e
    retn 00004h                               ; c2 04 00                    ; 0xc148f
  ; disGetNextSymbol 0xc1492 LB 0x284d -> off=0x0 cb=000000000000007c uValue=00000000000c1492 'vgamem_fill_pl4'
vgamem_fill_pl4:                             ; 0xc1492 LB 0x7c
    push bp                                   ; 55                          ; 0xc1492 vgabios.c:885
    mov bp, sp                                ; 89 e5                       ; 0xc1493
    push si                                   ; 56                          ; 0xc1495
    push di                                   ; 57                          ; 0xc1496
    sub sp, strict byte 00006h                ; 83 ec 06                    ; 0xc1497
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc149a
    mov al, dl                                ; 88 d0                       ; 0xc149d
    mov byte [bp-006h], bl                    ; 88 5e fa                    ; 0xc149f
    mov bh, cl                                ; 88 cf                       ; 0xc14a2
    xor ah, ah                                ; 30 e4                       ; 0xc14a4 vgabios.c:891
    mov dx, ax                                ; 89 c2                       ; 0xc14a6
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc14a8
    mov cx, ax                                ; 89 c1                       ; 0xc14ab
    mov ax, dx                                ; 89 d0                       ; 0xc14ad
    imul cx                                   ; f7 e9                       ; 0xc14af
    mov dl, bh                                ; 88 fa                       ; 0xc14b1
    xor dh, dh                                ; 30 f6                       ; 0xc14b3
    imul dx                                   ; f7 ea                       ; 0xc14b5
    mov dx, ax                                ; 89 c2                       ; 0xc14b7
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc14b9
    xor ah, ah                                ; 30 e4                       ; 0xc14bc
    add dx, ax                                ; 01 c2                       ; 0xc14be
    mov word [bp-00ah], dx                    ; 89 56 f6                    ; 0xc14c0
    mov ax, 00205h                            ; b8 05 02                    ; 0xc14c3 vgabios.c:892
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc14c6
    out DX, ax                                ; ef                          ; 0xc14c9
    xor bl, bl                                ; 30 db                       ; 0xc14ca vgabios.c:893
    cmp bl, byte [bp+004h]                    ; 3a 5e 04                    ; 0xc14cc
    jnc short 014feh                          ; 73 2d                       ; 0xc14cf
    mov cl, byte [bp-006h]                    ; 8a 4e fa                    ; 0xc14d1 vgabios.c:895
    xor ch, ch                                ; 30 ed                       ; 0xc14d4
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc14d6
    xor ah, ah                                ; 30 e4                       ; 0xc14d9
    mov si, ax                                ; 89 c6                       ; 0xc14db
    mov al, bl                                ; 88 d8                       ; 0xc14dd
    mov dx, ax                                ; 89 c2                       ; 0xc14df
    mov al, bh                                ; 88 f8                       ; 0xc14e1
    mov di, ax                                ; 89 c7                       ; 0xc14e3
    mov ax, dx                                ; 89 d0                       ; 0xc14e5
    imul di                                   ; f7 ef                       ; 0xc14e7
    mov di, word [bp-00ah]                    ; 8b 7e f6                    ; 0xc14e9
    add di, ax                                ; 01 c7                       ; 0xc14ec
    mov ax, si                                ; 89 f0                       ; 0xc14ee
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc14f0
    mov es, dx                                ; 8e c2                       ; 0xc14f3
    cld                                       ; fc                          ; 0xc14f5
    jcxz 014fah                               ; e3 02                       ; 0xc14f6
    rep stosb                                 ; f3 aa                       ; 0xc14f8
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3                     ; 0xc14fa vgabios.c:896
    jmp short 014cch                          ; eb ce                       ; 0xc14fc
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc14fe vgabios.c:897
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1501
    out DX, ax                                ; ef                          ; 0xc1504
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc1505 vgabios.c:898
    pop di                                    ; 5f                          ; 0xc1508
    pop si                                    ; 5e                          ; 0xc1509
    pop bp                                    ; 5d                          ; 0xc150a
    retn 00004h                               ; c2 04 00                    ; 0xc150b
  ; disGetNextSymbol 0xc150e LB 0x27d1 -> off=0x0 cb=00000000000000c2 uValue=00000000000c150e 'vgamem_copy_cga'
vgamem_copy_cga:                             ; 0xc150e LB 0xc2
    push bp                                   ; 55                          ; 0xc150e vgabios.c:901
    mov bp, sp                                ; 89 e5                       ; 0xc150f
    push si                                   ; 56                          ; 0xc1511
    push di                                   ; 57                          ; 0xc1512
    sub sp, strict byte 00008h                ; 83 ec 08                    ; 0xc1513
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc1516
    mov al, dl                                ; 88 d0                       ; 0xc1519
    mov bh, cl                                ; 88 cf                       ; 0xc151b
    xor ah, ah                                ; 30 e4                       ; 0xc151d vgabios.c:907
    mov dx, ax                                ; 89 c2                       ; 0xc151f
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc1521
    mov cx, ax                                ; 89 c1                       ; 0xc1524
    mov ax, dx                                ; 89 d0                       ; 0xc1526
    imul cx                                   ; f7 e9                       ; 0xc1528
    mov dl, byte [bp+004h]                    ; 8a 56 04                    ; 0xc152a
    xor dh, dh                                ; 30 f6                       ; 0xc152d
    mov di, dx                                ; 89 d7                       ; 0xc152f
    imul dx                                   ; f7 ea                       ; 0xc1531
    mov dx, ax                                ; 89 c2                       ; 0xc1533
    sar dx, 1                                 ; d1 fa                       ; 0xc1535
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1537
    xor ah, ah                                ; 30 e4                       ; 0xc153a
    mov si, ax                                ; 89 c6                       ; 0xc153c
    add dx, ax                                ; 01 c2                       ; 0xc153e
    mov word [bp-008h], dx                    ; 89 56 f8                    ; 0xc1540
    mov al, bl                                ; 88 d8                       ; 0xc1543 vgabios.c:908
    imul cx                                   ; f7 e9                       ; 0xc1545
    imul di                                   ; f7 ef                       ; 0xc1547
    sar ax, 1                                 ; d1 f8                       ; 0xc1549
    add ax, si                                ; 01 f0                       ; 0xc154b
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc154d
    xor bl, bl                                ; 30 db                       ; 0xc1550 vgabios.c:909
    cmp bl, byte [bp+006h]                    ; 3a 5e 06                    ; 0xc1552
    jnc short 015c7h                          ; 73 70                       ; 0xc1555
    test bl, 001h                             ; f6 c3 01                    ; 0xc1557 vgabios.c:911
    je short 01593h                           ; 74 37                       ; 0xc155a
    mov cl, bh                                ; 88 f9                       ; 0xc155c vgabios.c:912
    xor ch, ch                                ; 30 ed                       ; 0xc155e
    mov al, bl                                ; 88 d8                       ; 0xc1560
    xor ah, ah                                ; 30 e4                       ; 0xc1562
    mov dx, ax                                ; 89 c2                       ; 0xc1564
    sar dx, 1                                 ; d1 fa                       ; 0xc1566
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1568
    mov si, ax                                ; 89 c6                       ; 0xc156b
    mov ax, dx                                ; 89 d0                       ; 0xc156d
    imul si                                   ; f7 ee                       ; 0xc156f
    mov si, word [bp-008h]                    ; 8b 76 f8                    ; 0xc1571
    add si, 02000h                            ; 81 c6 00 20                 ; 0xc1574
    add si, ax                                ; 01 c6                       ; 0xc1578
    mov di, word [bp-00ah]                    ; 8b 7e f6                    ; 0xc157a
    add di, 02000h                            ; 81 c7 00 20                 ; 0xc157d
    add di, ax                                ; 01 c7                       ; 0xc1581
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc1583
    mov es, dx                                ; 8e c2                       ; 0xc1586
    cld                                       ; fc                          ; 0xc1588
    jcxz 01591h                               ; e3 06                       ; 0xc1589
    push DS                                   ; 1e                          ; 0xc158b
    mov ds, dx                                ; 8e da                       ; 0xc158c
    rep movsb                                 ; f3 a4                       ; 0xc158e
    pop DS                                    ; 1f                          ; 0xc1590
    jmp short 015c3h                          ; eb 30                       ; 0xc1591 vgabios.c:913
    mov al, bh                                ; 88 f8                       ; 0xc1593 vgabios.c:914
    xor ah, ah                                ; 30 e4                       ; 0xc1595
    mov cx, ax                                ; 89 c1                       ; 0xc1597
    mov al, bl                                ; 88 d8                       ; 0xc1599
    sar ax, 1                                 ; d1 f8                       ; 0xc159b
    mov dl, byte [bp+004h]                    ; 8a 56 04                    ; 0xc159d
    mov byte [bp-00ch], dl                    ; 88 56 f4                    ; 0xc15a0
    mov byte [bp-00bh], ch                    ; 88 6e f5                    ; 0xc15a3
    mov dx, word [bp-00ch]                    ; 8b 56 f4                    ; 0xc15a6
    imul dx                                   ; f7 ea                       ; 0xc15a9
    mov si, word [bp-008h]                    ; 8b 76 f8                    ; 0xc15ab
    add si, ax                                ; 01 c6                       ; 0xc15ae
    mov di, word [bp-00ah]                    ; 8b 7e f6                    ; 0xc15b0
    add di, ax                                ; 01 c7                       ; 0xc15b3
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc15b5
    mov es, dx                                ; 8e c2                       ; 0xc15b8
    cld                                       ; fc                          ; 0xc15ba
    jcxz 015c3h                               ; e3 06                       ; 0xc15bb
    push DS                                   ; 1e                          ; 0xc15bd
    mov ds, dx                                ; 8e da                       ; 0xc15be
    rep movsb                                 ; f3 a4                       ; 0xc15c0
    pop DS                                    ; 1f                          ; 0xc15c2
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3                     ; 0xc15c3 vgabios.c:915
    jmp short 01552h                          ; eb 8b                       ; 0xc15c5
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc15c7 vgabios.c:916
    pop di                                    ; 5f                          ; 0xc15ca
    pop si                                    ; 5e                          ; 0xc15cb
    pop bp                                    ; 5d                          ; 0xc15cc
    retn 00004h                               ; c2 04 00                    ; 0xc15cd
  ; disGetNextSymbol 0xc15d0 LB 0x270f -> off=0x0 cb=00000000000000a8 uValue=00000000000c15d0 'vgamem_fill_cga'
vgamem_fill_cga:                             ; 0xc15d0 LB 0xa8
    push bp                                   ; 55                          ; 0xc15d0 vgabios.c:919
    mov bp, sp                                ; 89 e5                       ; 0xc15d1
    push si                                   ; 56                          ; 0xc15d3
    push di                                   ; 57                          ; 0xc15d4
    sub sp, strict byte 00006h                ; 83 ec 06                    ; 0xc15d5
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc15d8
    mov al, dl                                ; 88 d0                       ; 0xc15db
    mov byte [bp-008h], bl                    ; 88 5e f8                    ; 0xc15dd
    mov bh, cl                                ; 88 cf                       ; 0xc15e0
    xor ah, ah                                ; 30 e4                       ; 0xc15e2 vgabios.c:925
    mov dx, ax                                ; 89 c2                       ; 0xc15e4
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc15e6
    mov cx, ax                                ; 89 c1                       ; 0xc15e9
    mov ax, dx                                ; 89 d0                       ; 0xc15eb
    imul cx                                   ; f7 e9                       ; 0xc15ed
    mov dl, bh                                ; 88 fa                       ; 0xc15ef
    xor dh, dh                                ; 30 f6                       ; 0xc15f1
    imul dx                                   ; f7 ea                       ; 0xc15f3
    mov dx, ax                                ; 89 c2                       ; 0xc15f5
    sar dx, 1                                 ; d1 fa                       ; 0xc15f7
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc15f9
    xor ah, ah                                ; 30 e4                       ; 0xc15fc
    add dx, ax                                ; 01 c2                       ; 0xc15fe
    mov word [bp-00ah], dx                    ; 89 56 f6                    ; 0xc1600
    xor bl, bl                                ; 30 db                       ; 0xc1603 vgabios.c:926
    cmp bl, byte [bp+004h]                    ; 3a 5e 04                    ; 0xc1605
    jnc short 0166fh                          ; 73 65                       ; 0xc1608
    test bl, 001h                             ; f6 c3 01                    ; 0xc160a vgabios.c:928
    je short 01640h                           ; 74 31                       ; 0xc160d
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc160f vgabios.c:929
    xor ah, ah                                ; 30 e4                       ; 0xc1612
    mov cx, ax                                ; 89 c1                       ; 0xc1614
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc1616
    mov si, ax                                ; 89 c6                       ; 0xc1619
    mov al, bl                                ; 88 d8                       ; 0xc161b
    mov dx, ax                                ; 89 c2                       ; 0xc161d
    sar dx, 1                                 ; d1 fa                       ; 0xc161f
    mov al, bh                                ; 88 f8                       ; 0xc1621
    mov di, ax                                ; 89 c7                       ; 0xc1623
    mov ax, dx                                ; 89 d0                       ; 0xc1625
    imul di                                   ; f7 ef                       ; 0xc1627
    mov di, word [bp-00ah]                    ; 8b 7e f6                    ; 0xc1629
    add di, 02000h                            ; 81 c7 00 20                 ; 0xc162c
    add di, ax                                ; 01 c7                       ; 0xc1630
    mov ax, si                                ; 89 f0                       ; 0xc1632
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc1634
    mov es, dx                                ; 8e c2                       ; 0xc1637
    cld                                       ; fc                          ; 0xc1639
    jcxz 0163eh                               ; e3 02                       ; 0xc163a
    rep stosb                                 ; f3 aa                       ; 0xc163c
    jmp short 0166bh                          ; eb 2b                       ; 0xc163e vgabios.c:930
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1640 vgabios.c:931
    xor ah, ah                                ; 30 e4                       ; 0xc1643
    mov cx, ax                                ; 89 c1                       ; 0xc1645
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc1647
    mov si, ax                                ; 89 c6                       ; 0xc164a
    mov al, bl                                ; 88 d8                       ; 0xc164c
    mov dx, ax                                ; 89 c2                       ; 0xc164e
    sar dx, 1                                 ; d1 fa                       ; 0xc1650
    mov al, bh                                ; 88 f8                       ; 0xc1652
    mov di, ax                                ; 89 c7                       ; 0xc1654
    mov ax, dx                                ; 89 d0                       ; 0xc1656
    imul di                                   ; f7 ef                       ; 0xc1658
    mov di, word [bp-00ah]                    ; 8b 7e f6                    ; 0xc165a
    add di, ax                                ; 01 c7                       ; 0xc165d
    mov ax, si                                ; 89 f0                       ; 0xc165f
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc1661
    mov es, dx                                ; 8e c2                       ; 0xc1664
    cld                                       ; fc                          ; 0xc1666
    jcxz 0166bh                               ; e3 02                       ; 0xc1667
    rep stosb                                 ; f3 aa                       ; 0xc1669
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3                     ; 0xc166b vgabios.c:932
    jmp short 01605h                          ; eb 96                       ; 0xc166d
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc166f vgabios.c:933
    pop di                                    ; 5f                          ; 0xc1672
    pop si                                    ; 5e                          ; 0xc1673
    pop bp                                    ; 5d                          ; 0xc1674
    retn 00004h                               ; c2 04 00                    ; 0xc1675
  ; disGetNextSymbol 0xc1678 LB 0x2667 -> off=0x0 cb=0000000000000576 uValue=00000000000c1678 'biosfn_scroll'
biosfn_scroll:                               ; 0xc1678 LB 0x576
    push bp                                   ; 55                          ; 0xc1678 vgabios.c:936
    mov bp, sp                                ; 89 e5                       ; 0xc1679
    push si                                   ; 56                          ; 0xc167b
    push di                                   ; 57                          ; 0xc167c
    sub sp, strict byte 0001eh                ; 83 ec 1e                    ; 0xc167d
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc1680
    mov byte [bp-008h], dl                    ; 88 56 f8                    ; 0xc1683
    mov byte [bp-00eh], bl                    ; 88 5e f2                    ; 0xc1686
    mov byte [bp-00ch], cl                    ; 88 4e f4                    ; 0xc1689
    mov ch, byte [bp+006h]                    ; 8a 6e 06                    ; 0xc168c
    cmp bl, byte [bp+004h]                    ; 3a 5e 04                    ; 0xc168f vgabios.c:945
    jnbe short 016adh                         ; 77 19                       ; 0xc1692
    cmp ch, cl                                ; 38 cd                       ; 0xc1694 vgabios.c:946
    jc short 016adh                           ; 72 15                       ; 0xc1696
    mov dx, strict word 00049h                ; ba 49 00                    ; 0xc1698 vgabios.c:949
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc169b
    call 031beh                               ; e8 1d 1b                    ; 0xc169e
    xor ah, ah                                ; 30 e4                       ; 0xc16a1 vgabios.c:950
    call 03193h                               ; e8 ed 1a                    ; 0xc16a3
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc16a6
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc16a9 vgabios.c:951
    jne short 016b0h                          ; 75 03                       ; 0xc16ab
    jmp near 01be5h                           ; e9 35 05                    ; 0xc16ad
    mov dx, 00084h                            ; ba 84 00                    ; 0xc16b0 vgabios.c:954
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc16b3
    call 031beh                               ; e8 05 1b                    ; 0xc16b6
    xor ah, ah                                ; 30 e4                       ; 0xc16b9
    mov bx, ax                                ; 89 c3                       ; 0xc16bb
    inc bx                                    ; 43                          ; 0xc16bd
    mov dx, strict word 0004ah                ; ba 4a 00                    ; 0xc16be vgabios.c:955
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc16c1
    call 031dah                               ; e8 13 1b                    ; 0xc16c4
    mov word [bp-018h], ax                    ; 89 46 e8                    ; 0xc16c7
    cmp byte [bp+008h], 0ffh                  ; 80 7e 08 ff                 ; 0xc16ca vgabios.c:958
    jne short 016dch                          ; 75 0c                       ; 0xc16ce
    mov dx, strict word 00062h                ; ba 62 00                    ; 0xc16d0 vgabios.c:959
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc16d3
    call 031beh                               ; e8 e5 1a                    ; 0xc16d6
    mov byte [bp+008h], al                    ; 88 46 08                    ; 0xc16d9
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc16dc vgabios.c:961
    xor ah, ah                                ; 30 e4                       ; 0xc16df
    cmp ax, bx                                ; 39 d8                       ; 0xc16e1
    jc short 016ech                           ; 72 07                       ; 0xc16e3
    mov al, bl                                ; 88 d8                       ; 0xc16e5
    db  0feh, 0c8h
    ; dec al                                    ; fe c8                     ; 0xc16e7
    mov byte [bp+004h], al                    ; 88 46 04                    ; 0xc16e9
    mov al, ch                                ; 88 e8                       ; 0xc16ec vgabios.c:962
    xor ah, ah                                ; 30 e4                       ; 0xc16ee
    cmp ax, word [bp-018h]                    ; 3b 46 e8                    ; 0xc16f0
    jc short 016fah                           ; 72 05                       ; 0xc16f3
    mov ch, byte [bp-018h]                    ; 8a 6e e8                    ; 0xc16f5
    db  0feh, 0cdh
    ; dec ch                                    ; fe cd                     ; 0xc16f8
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc16fa vgabios.c:963
    xor ah, ah                                ; 30 e4                       ; 0xc16fd
    cmp ax, bx                                ; 39 d8                       ; 0xc16ff
    jbe short 01706h                          ; 76 03                       ; 0xc1701
    mov byte [bp-00ah], ah                    ; 88 66 f6                    ; 0xc1703
    mov al, ch                                ; 88 e8                       ; 0xc1706 vgabios.c:964
    sub al, byte [bp-00ch]                    ; 2a 46 f4                    ; 0xc1708
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc170b
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc170d
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1710 vgabios.c:966
    xor ah, ah                                ; 30 e4                       ; 0xc1713
    mov si, ax                                ; 89 c6                       ; 0xc1715
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc1717
    mov di, ax                                ; 89 c7                       ; 0xc1719
    sal di, CL                                ; d3 e7                       ; 0xc171b
    mov ax, word [bp-018h]                    ; 8b 46 e8                    ; 0xc171d
    dec ax                                    ; 48                          ; 0xc1720
    mov word [bp-01ch], ax                    ; 89 46 e4                    ; 0xc1721
    lea ax, [bx-001h]                         ; 8d 47 ff                    ; 0xc1724
    mov word [bp-01eh], ax                    ; 89 46 e2                    ; 0xc1727
    mov ax, word [bp-018h]                    ; 8b 46 e8                    ; 0xc172a
    mul bx                                    ; f7 e3                       ; 0xc172d
    mov word [bp-01ah], ax                    ; 89 46 e6                    ; 0xc172f
    cmp byte [di+04635h], 000h                ; 80 bd 35 46 00              ; 0xc1732
    jne short 01789h                          ; 75 50                       ; 0xc1737
    sal ax, 1                                 ; d1 e0                       ; 0xc1739 vgabios.c:969
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc173b
    mov bx, ax                                ; 89 c3                       ; 0xc173d
    mov al, byte [bp+008h]                    ; 8a 46 08                    ; 0xc173f
    xor ah, ah                                ; 30 e4                       ; 0xc1742
    mov dx, ax                                ; 89 c2                       ; 0xc1744
    lea ax, [bx+001h]                         ; 8d 47 01                    ; 0xc1746
    mul dx                                    ; f7 e2                       ; 0xc1749
    mov bx, ax                                ; 89 c3                       ; 0xc174b
    cmp byte [bp-00ah], 000h                  ; 80 7e f6 00                 ; 0xc174d vgabios.c:974
    jne short 0178ch                          ; 75 39                       ; 0xc1751
    cmp byte [bp-00eh], 000h                  ; 80 7e f2 00                 ; 0xc1753
    jne short 0178ch                          ; 75 33                       ; 0xc1757
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00                 ; 0xc1759
    jne short 0178ch                          ; 75 2d                       ; 0xc175d
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc175f
    xor ah, ah                                ; 30 e4                       ; 0xc1762
    cmp ax, word [bp-01eh]                    ; 3b 46 e2                    ; 0xc1764
    jne short 0178ch                          ; 75 23                       ; 0xc1767
    mov al, ch                                ; 88 e8                       ; 0xc1769
    cmp ax, word [bp-01ch]                    ; 3b 46 e4                    ; 0xc176b
    jne short 0178ch                          ; 75 1c                       ; 0xc176e
    mov ah, byte [bp-008h]                    ; 8a 66 f8                    ; 0xc1770 vgabios.c:976
    xor al, ch                                ; 30 e8                       ; 0xc1773
    add ax, strict word 00020h                ; 05 20 00                    ; 0xc1775
    mov es, [di+04638h]                       ; 8e 85 38 46                 ; 0xc1778
    mov cx, word [bp-01ah]                    ; 8b 4e e6                    ; 0xc177c
    mov di, bx                                ; 89 df                       ; 0xc177f
    cld                                       ; fc                          ; 0xc1781
    jcxz 01786h                               ; e3 02                       ; 0xc1782
    rep stosw                                 ; f3 ab                       ; 0xc1784
    jmp near 01be5h                           ; e9 5c 04                    ; 0xc1786 vgabios.c:978
    jmp near 0191ah                           ; e9 8e 01                    ; 0xc1789
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc178c vgabios.c:980
    jne short 017fah                          ; 75 68                       ; 0xc1790
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1792 vgabios.c:981
    xor ah, ah                                ; 30 e4                       ; 0xc1795
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc1797
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc179a
    xor ah, ah                                ; 30 e4                       ; 0xc179d
    mov dx, ax                                ; 89 c2                       ; 0xc179f
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc17a1
    jc short 017fch                           ; 72 56                       ; 0xc17a4
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc17a6 vgabios.c:983
    add ax, word [bp-016h]                    ; 03 46 ea                    ; 0xc17a9
    cmp ax, dx                                ; 39 d0                       ; 0xc17ac
    jnbe short 017b6h                         ; 77 06                       ; 0xc17ae
    cmp byte [bp-00ah], 000h                  ; 80 7e f6 00                 ; 0xc17b0
    jne short 017ffh                          ; 75 49                       ; 0xc17b4
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc17b6 vgabios.c:984
    xor ah, ah                                ; 30 e4                       ; 0xc17b9
    mov word [bp-022h], ax                    ; 89 46 de                    ; 0xc17bb
    mov ah, byte [bp-008h]                    ; 8a 66 f8                    ; 0xc17be
    xor al, al                                ; 30 c0                       ; 0xc17c1
    mov di, ax                                ; 89 c7                       ; 0xc17c3
    add di, strict byte 00020h                ; 83 c7 20                    ; 0xc17c5
    mov ax, word [bp-016h]                    ; 8b 46 ea                    ; 0xc17c8
    mul word [bp-018h]                        ; f7 66 e8                    ; 0xc17cb
    mov dx, ax                                ; 89 c2                       ; 0xc17ce
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc17d0
    xor ah, ah                                ; 30 e4                       ; 0xc17d3
    add ax, dx                                ; 01 d0                       ; 0xc17d5
    sal ax, 1                                 ; d1 e0                       ; 0xc17d7
    mov dx, bx                                ; 89 da                       ; 0xc17d9
    add dx, ax                                ; 01 c2                       ; 0xc17db
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc17dd
    xor ah, ah                                ; 30 e4                       ; 0xc17e0
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc17e2
    mov si, ax                                ; 89 c6                       ; 0xc17e4
    sal si, CL                                ; d3 e6                       ; 0xc17e6
    mov es, [si+04638h]                       ; 8e 84 38 46                 ; 0xc17e8
    mov cx, word [bp-022h]                    ; 8b 4e de                    ; 0xc17ec
    mov ax, di                                ; 89 f8                       ; 0xc17ef
    mov di, dx                                ; 89 d7                       ; 0xc17f1
    cld                                       ; fc                          ; 0xc17f3
    jcxz 017f8h                               ; e3 02                       ; 0xc17f4
    rep stosw                                 ; f3 ab                       ; 0xc17f6
    jmp short 0184ah                          ; eb 50                       ; 0xc17f8 vgabios.c:985
    jmp short 01850h                          ; eb 54                       ; 0xc17fa
    jmp near 01be5h                           ; e9 e6 03                    ; 0xc17fc
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc17ff vgabios.c:986
    mov di, dx                                ; 89 d7                       ; 0xc1802
    mul word [bp-018h]                        ; f7 66 e8                    ; 0xc1804
    mov dl, byte [bp-00ch]                    ; 8a 56 f4                    ; 0xc1807
    xor dh, dh                                ; 30 f6                       ; 0xc180a
    mov word [bp-014h], dx                    ; 89 56 ec                    ; 0xc180c
    add ax, dx                                ; 01 d0                       ; 0xc180f
    sal ax, 1                                 ; d1 e0                       ; 0xc1811
    mov word [bp-020h], ax                    ; 89 46 e0                    ; 0xc1813
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1816
    xor ah, ah                                ; 30 e4                       ; 0xc1819
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc181b
    mov si, ax                                ; 89 c6                       ; 0xc181d
    sal si, CL                                ; d3 e6                       ; 0xc181f
    mov ax, word [si+04638h]                  ; 8b 84 38 46                 ; 0xc1821
    mov word [bp-022h], ax                    ; 89 46 de                    ; 0xc1825
    mov ax, word [bp-016h]                    ; 8b 46 ea                    ; 0xc1828
    mul word [bp-018h]                        ; f7 66 e8                    ; 0xc182b
    add ax, word [bp-014h]                    ; 03 46 ec                    ; 0xc182e
    sal ax, 1                                 ; d1 e0                       ; 0xc1831
    add ax, bx                                ; 01 d8                       ; 0xc1833
    mov cx, di                                ; 89 f9                       ; 0xc1835
    mov si, word [bp-020h]                    ; 8b 76 e0                    ; 0xc1837
    mov dx, word [bp-022h]                    ; 8b 56 de                    ; 0xc183a
    mov di, ax                                ; 89 c7                       ; 0xc183d
    mov es, dx                                ; 8e c2                       ; 0xc183f
    cld                                       ; fc                          ; 0xc1841
    jcxz 0184ah                               ; e3 06                       ; 0xc1842
    push DS                                   ; 1e                          ; 0xc1844
    mov ds, dx                                ; 8e da                       ; 0xc1845
    rep movsw                                 ; f3 a5                       ; 0xc1847
    pop DS                                    ; 1f                          ; 0xc1849
    inc word [bp-016h]                        ; ff 46 ea                    ; 0xc184a vgabios.c:987
    jmp near 0179ah                           ; e9 4a ff                    ; 0xc184d
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1850 vgabios.c:990
    xor ah, ah                                ; 30 e4                       ; 0xc1853
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc1855
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1858
    xor ah, ah                                ; 30 e4                       ; 0xc185b
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc185d
    jnbe short 017fch                         ; 77 9a                       ; 0xc1860
    mov dx, ax                                ; 89 c2                       ; 0xc1862 vgabios.c:992
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc1864
    add ax, dx                                ; 01 d0                       ; 0xc1867
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1869
    jnbe short 01874h                         ; 77 06                       ; 0xc186c
    cmp byte [bp-00ah], 000h                  ; 80 7e f6 00                 ; 0xc186e
    jne short 018b3h                          ; 75 3f                       ; 0xc1872
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1874 vgabios.c:993
    xor ah, ah                                ; 30 e4                       ; 0xc1877
    mov di, ax                                ; 89 c7                       ; 0xc1879
    mov ah, byte [bp-008h]                    ; 8a 66 f8                    ; 0xc187b
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc187e
    mov word [bp-022h], ax                    ; 89 46 de                    ; 0xc1880
    mov ax, word [bp-016h]                    ; 8b 46 ea                    ; 0xc1883
    mul word [bp-018h]                        ; f7 66 e8                    ; 0xc1886
    mov dx, ax                                ; 89 c2                       ; 0xc1889
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc188b
    xor ah, ah                                ; 30 e4                       ; 0xc188e
    add dx, ax                                ; 01 c2                       ; 0xc1890
    sal dx, 1                                 ; d1 e2                       ; 0xc1892
    add dx, bx                                ; 01 da                       ; 0xc1894
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1896
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc1899
    mov si, ax                                ; 89 c6                       ; 0xc189b
    sal si, CL                                ; d3 e6                       ; 0xc189d
    mov si, word [si+04638h]                  ; 8b b4 38 46                 ; 0xc189f
    mov cx, di                                ; 89 f9                       ; 0xc18a3
    mov ax, word [bp-022h]                    ; 8b 46 de                    ; 0xc18a5
    mov di, dx                                ; 89 d7                       ; 0xc18a8
    mov es, si                                ; 8e c6                       ; 0xc18aa
    cld                                       ; fc                          ; 0xc18ac
    jcxz 018b1h                               ; e3 02                       ; 0xc18ad
    rep stosw                                 ; f3 ab                       ; 0xc18af
    jmp short 0190ah                          ; eb 57                       ; 0xc18b1 vgabios.c:994
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc18b3 vgabios.c:995
    xor ah, ah                                ; 30 e4                       ; 0xc18b6
    mov di, ax                                ; 89 c7                       ; 0xc18b8
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc18ba
    mov dx, word [bp-016h]                    ; 8b 56 ea                    ; 0xc18bd
    sub dx, ax                                ; 29 c2                       ; 0xc18c0
    mov ax, dx                                ; 89 d0                       ; 0xc18c2
    mul word [bp-018h]                        ; f7 66 e8                    ; 0xc18c4
    mov dx, ax                                ; 89 c2                       ; 0xc18c7
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc18c9
    xor ah, ah                                ; 30 e4                       ; 0xc18cc
    mov word [bp-022h], ax                    ; 89 46 de                    ; 0xc18ce
    add dx, ax                                ; 01 c2                       ; 0xc18d1
    sal dx, 1                                 ; d1 e2                       ; 0xc18d3
    mov word [bp-020h], dx                    ; 89 56 e0                    ; 0xc18d5
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc18d8
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc18db
    mov si, ax                                ; 89 c6                       ; 0xc18dd
    sal si, CL                                ; d3 e6                       ; 0xc18df
    mov ax, word [si+04638h]                  ; 8b 84 38 46                 ; 0xc18e1
    mov word [bp-014h], ax                    ; 89 46 ec                    ; 0xc18e5
    mov ax, word [bp-016h]                    ; 8b 46 ea                    ; 0xc18e8
    mul word [bp-018h]                        ; f7 66 e8                    ; 0xc18eb
    add ax, word [bp-022h]                    ; 03 46 de                    ; 0xc18ee
    sal ax, 1                                 ; d1 e0                       ; 0xc18f1
    add ax, bx                                ; 01 d8                       ; 0xc18f3
    mov cx, di                                ; 89 f9                       ; 0xc18f5
    mov si, word [bp-020h]                    ; 8b 76 e0                    ; 0xc18f7
    mov dx, word [bp-014h]                    ; 8b 56 ec                    ; 0xc18fa
    mov di, ax                                ; 89 c7                       ; 0xc18fd
    mov es, dx                                ; 8e c2                       ; 0xc18ff
    cld                                       ; fc                          ; 0xc1901
    jcxz 0190ah                               ; e3 06                       ; 0xc1902
    push DS                                   ; 1e                          ; 0xc1904
    mov ds, dx                                ; 8e da                       ; 0xc1905
    rep movsw                                 ; f3 a5                       ; 0xc1907
    pop DS                                    ; 1f                          ; 0xc1909
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc190a vgabios.c:996
    xor ah, ah                                ; 30 e4                       ; 0xc190d
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc190f
    jc short 0193dh                           ; 72 29                       ; 0xc1912
    dec word [bp-016h]                        ; ff 4e ea                    ; 0xc1914 vgabios.c:997
    jmp near 01858h                           ; e9 3e ff                    ; 0xc1917
    mov al, byte [si+046b4h]                  ; 8a 84 b4 46                 ; 0xc191a vgabios.c:1004
    xor ah, ah                                ; 30 e4                       ; 0xc191e
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc1920
    mov si, ax                                ; 89 c6                       ; 0xc1922
    sal si, CL                                ; d3 e6                       ; 0xc1924
    mov al, byte [si+046cah]                  ; 8a 84 ca 46                 ; 0xc1926
    mov byte [bp-012h], al                    ; 88 46 ee                    ; 0xc192a
    mov al, byte [di+04636h]                  ; 8a 85 36 46                 ; 0xc192d vgabios.c:1005
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc1931
    je short 01940h                           ; 74 0b                       ; 0xc1933
    cmp AL, strict byte 003h                  ; 3c 03                       ; 0xc1935
    je short 01940h                           ; 74 07                       ; 0xc1937
    cmp AL, strict byte 002h                  ; 3c 02                       ; 0xc1939
    je short 0196eh                           ; 74 31                       ; 0xc193b
    jmp near 01be5h                           ; e9 a5 02                    ; 0xc193d
    cmp byte [bp-00ah], 000h                  ; 80 7e f6 00                 ; 0xc1940 vgabios.c:1009
    jne short 019ach                          ; 75 66                       ; 0xc1944
    cmp byte [bp-00eh], 000h                  ; 80 7e f2 00                 ; 0xc1946
    jne short 019ach                          ; 75 60                       ; 0xc194a
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00                 ; 0xc194c
    jne short 019ach                          ; 75 5a                       ; 0xc1950
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1952
    xor ah, ah                                ; 30 e4                       ; 0xc1955
    mov dx, ax                                ; 89 c2                       ; 0xc1957
    lea ax, [bx-001h]                         ; 8d 47 ff                    ; 0xc1959
    cmp dx, ax                                ; 39 c2                       ; 0xc195c
    jne short 019ach                          ; 75 4c                       ; 0xc195e
    mov al, ch                                ; 88 e8                       ; 0xc1960
    xor ah, ah                                ; 30 e4                       ; 0xc1962
    mov dx, word [bp-018h]                    ; 8b 56 e8                    ; 0xc1964
    dec dx                                    ; 4a                          ; 0xc1967
    cmp ax, dx                                ; 39 d0                       ; 0xc1968
    je short 01971h                           ; 74 05                       ; 0xc196a
    jmp short 019ach                          ; eb 3e                       ; 0xc196c
    jmp near 01aa8h                           ; e9 37 01                    ; 0xc196e
    mov ax, 00205h                            ; b8 05 02                    ; 0xc1971 vgabios.c:1011
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1974
    out DX, ax                                ; ef                          ; 0xc1977
    mov ax, bx                                ; 89 d8                       ; 0xc1978 vgabios.c:1012
    mul word [bp-018h]                        ; f7 66 e8                    ; 0xc197a
    mov dl, byte [bp-012h]                    ; 8a 56 ee                    ; 0xc197d
    xor dh, dh                                ; 30 f6                       ; 0xc1980
    mul dx                                    ; f7 e2                       ; 0xc1982
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc1984
    xor dh, dh                                ; 30 f6                       ; 0xc1987
    mov bl, byte [bp-010h]                    ; 8a 5e f0                    ; 0xc1989
    xor bh, bh                                ; 30 ff                       ; 0xc198c
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc198e
    sal bx, CL                                ; d3 e3                       ; 0xc1990
    mov bx, word [bx+04638h]                  ; 8b 9f 38 46                 ; 0xc1992
    mov cx, ax                                ; 89 c1                       ; 0xc1996
    mov ax, dx                                ; 89 d0                       ; 0xc1998
    xor di, di                                ; 31 ff                       ; 0xc199a
    mov es, bx                                ; 8e c3                       ; 0xc199c
    cld                                       ; fc                          ; 0xc199e
    jcxz 019a3h                               ; e3 02                       ; 0xc199f
    rep stosb                                 ; f3 aa                       ; 0xc19a1
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc19a3 vgabios.c:1013
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc19a6
    out DX, ax                                ; ef                          ; 0xc19a9
    jmp short 0193dh                          ; eb 91                       ; 0xc19aa vgabios.c:1015
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc19ac vgabios.c:1017
    jne short 019f7h                          ; 75 45                       ; 0xc19b0
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc19b2 vgabios.c:1018
    xor ah, ah                                ; 30 e4                       ; 0xc19b5
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc19b7
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc19ba
    xor ah, ah                                ; 30 e4                       ; 0xc19bd
    mov dx, ax                                ; 89 c2                       ; 0xc19bf
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc19c1
    jc short 01a28h                           ; 72 62                       ; 0xc19c4
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc19c6 vgabios.c:1020
    add ax, word [bp-016h]                    ; 03 46 ea                    ; 0xc19c9
    cmp ax, dx                                ; 39 d0                       ; 0xc19cc
    jnbe short 019d6h                         ; 77 06                       ; 0xc19ce
    cmp byte [bp-00ah], 000h                  ; 80 7e f6 00                 ; 0xc19d0
    jne short 019f9h                          ; 75 23                       ; 0xc19d4
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc19d6 vgabios.c:1021
    xor ah, ah                                ; 30 e4                       ; 0xc19d9
    push ax                                   ; 50                          ; 0xc19db
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc19dc
    push ax                                   ; 50                          ; 0xc19df
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc19e0
    mov cx, ax                                ; 89 c1                       ; 0xc19e3
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc19e5
    xor bh, bh                                ; 30 ff                       ; 0xc19e8
    mov al, byte [bp-016h]                    ; 8a 46 ea                    ; 0xc19ea
    mov dx, ax                                ; 89 c2                       ; 0xc19ed
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc19ef
    call 01492h                               ; e8 9d fa                    ; 0xc19f2
    jmp short 01a23h                          ; eb 2c                       ; 0xc19f5 vgabios.c:1022
    jmp short 01a2bh                          ; eb 32                       ; 0xc19f7
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc19f9 vgabios.c:1023
    xor ah, ah                                ; 30 e4                       ; 0xc19fc
    push ax                                   ; 50                          ; 0xc19fe
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc19ff
    push ax                                   ; 50                          ; 0xc1a02
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1a03
    mov cx, ax                                ; 89 c1                       ; 0xc1a06
    mov al, byte [bp-016h]                    ; 8a 46 ea                    ; 0xc1a08
    mov bx, ax                                ; 89 c3                       ; 0xc1a0b
    add al, byte [bp-00ah]                    ; 02 46 f6                    ; 0xc1a0d
    mov dl, byte [bp-00ch]                    ; 8a 56 f4                    ; 0xc1a10
    mov byte [bp-014h], dl                    ; 88 56 ec                    ; 0xc1a13
    mov byte [bp-013h], ah                    ; 88 66 ed                    ; 0xc1a16
    mov si, word [bp-014h]                    ; 8b 76 ec                    ; 0xc1a19
    mov dx, ax                                ; 89 c2                       ; 0xc1a1c
    mov ax, si                                ; 89 f0                       ; 0xc1a1e
    call 01403h                               ; e8 e0 f9                    ; 0xc1a20
    inc word [bp-016h]                        ; ff 46 ea                    ; 0xc1a23 vgabios.c:1024
    jmp short 019bah                          ; eb 92                       ; 0xc1a26
    jmp near 01be5h                           ; e9 ba 01                    ; 0xc1a28
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1a2b vgabios.c:1027
    xor ah, ah                                ; 30 e4                       ; 0xc1a2e
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc1a30
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1a33
    xor ah, ah                                ; 30 e4                       ; 0xc1a36
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1a38
    jnbe short 01a28h                         ; 77 eb                       ; 0xc1a3b
    mov dx, ax                                ; 89 c2                       ; 0xc1a3d vgabios.c:1029
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc1a3f
    add dx, ax                                ; 01 c2                       ; 0xc1a42
    cmp dx, word [bp-016h]                    ; 3b 56 ea                    ; 0xc1a44
    jnbe short 01a4dh                         ; 77 04                       ; 0xc1a47
    test al, al                               ; 84 c0                       ; 0xc1a49
    jne short 01a6eh                          ; 75 21                       ; 0xc1a4b
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1a4d vgabios.c:1030
    xor ah, ah                                ; 30 e4                       ; 0xc1a50
    push ax                                   ; 50                          ; 0xc1a52
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc1a53
    push ax                                   ; 50                          ; 0xc1a56
    mov cl, byte [bp-018h]                    ; 8a 4e e8                    ; 0xc1a57
    xor ch, ch                                ; 30 ed                       ; 0xc1a5a
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1a5c
    mov bx, ax                                ; 89 c3                       ; 0xc1a5f
    mov al, byte [bp-016h]                    ; 8a 46 ea                    ; 0xc1a61
    mov dx, ax                                ; 89 c2                       ; 0xc1a64
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1a66
    call 01492h                               ; e8 26 fa                    ; 0xc1a69
    jmp short 01a99h                          ; eb 2b                       ; 0xc1a6c vgabios.c:1031
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc1a6e vgabios.c:1032
    push ax                                   ; 50                          ; 0xc1a71
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc1a72
    push ax                                   ; 50                          ; 0xc1a75
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1a76
    mov cx, ax                                ; 89 c1                       ; 0xc1a79
    mov al, byte [bp-016h]                    ; 8a 46 ea                    ; 0xc1a7b
    sub al, byte [bp-00ah]                    ; 2a 46 f6                    ; 0xc1a7e
    mov bx, ax                                ; 89 c3                       ; 0xc1a81
    mov al, byte [bp-016h]                    ; 8a 46 ea                    ; 0xc1a83
    mov dl, byte [bp-00ch]                    ; 8a 56 f4                    ; 0xc1a86
    mov byte [bp-014h], dl                    ; 88 56 ec                    ; 0xc1a89
    mov byte [bp-013h], ah                    ; 88 66 ed                    ; 0xc1a8c
    mov si, word [bp-014h]                    ; 8b 76 ec                    ; 0xc1a8f
    mov dx, ax                                ; 89 c2                       ; 0xc1a92
    mov ax, si                                ; 89 f0                       ; 0xc1a94
    call 01403h                               ; e8 6a f9                    ; 0xc1a96
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1a99 vgabios.c:1033
    xor ah, ah                                ; 30 e4                       ; 0xc1a9c
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1a9e
    jc short 01aefh                           ; 72 4c                       ; 0xc1aa1
    dec word [bp-016h]                        ; ff 4e ea                    ; 0xc1aa3 vgabios.c:1034
    jmp short 01a33h                          ; eb 8b                       ; 0xc1aa6
    mov bl, byte [di+04637h]                  ; 8a 9d 37 46                 ; 0xc1aa8 vgabios.c:1039
    cmp byte [bp-00ah], 000h                  ; 80 7e f6 00                 ; 0xc1aac vgabios.c:1040
    jne short 01af2h                          ; 75 40                       ; 0xc1ab0
    cmp byte [bp-00eh], 000h                  ; 80 7e f2 00                 ; 0xc1ab2
    jne short 01af2h                          ; 75 3a                       ; 0xc1ab6
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00                 ; 0xc1ab8
    jne short 01af2h                          ; 75 34                       ; 0xc1abc
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1abe
    cmp ax, word [bp-01eh]                    ; 3b 46 e2                    ; 0xc1ac1
    jne short 01af2h                          ; 75 2c                       ; 0xc1ac4
    mov al, ch                                ; 88 e8                       ; 0xc1ac6
    cmp ax, word [bp-01ch]                    ; 3b 46 e4                    ; 0xc1ac8
    jne short 01af2h                          ; 75 25                       ; 0xc1acb
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc1acd vgabios.c:1042
    mov dx, ax                                ; 89 c2                       ; 0xc1ad0
    mov ax, word [bp-01ah]                    ; 8b 46 e6                    ; 0xc1ad2
    mul dx                                    ; f7 e2                       ; 0xc1ad5
    xor bh, bh                                ; 30 ff                       ; 0xc1ad7
    mul bx                                    ; f7 e3                       ; 0xc1ad9
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc1adb
    xor dh, dh                                ; 30 f6                       ; 0xc1ade
    mov es, [di+04638h]                       ; 8e 85 38 46                 ; 0xc1ae0
    mov cx, ax                                ; 89 c1                       ; 0xc1ae4
    mov ax, dx                                ; 89 d0                       ; 0xc1ae6
    xor di, di                                ; 31 ff                       ; 0xc1ae8
    cld                                       ; fc                          ; 0xc1aea
    jcxz 01aefh                               ; e3 02                       ; 0xc1aeb
    rep stosb                                 ; f3 aa                       ; 0xc1aed
    jmp near 01be5h                           ; e9 f3 00                    ; 0xc1aef vgabios.c:1044
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc1af2 vgabios.c:1046
    jne short 01b00h                          ; 75 09                       ; 0xc1af5
    sal byte [bp-00ch], 1                     ; d0 66 f4                    ; 0xc1af7 vgabios.c:1048
    sal byte [bp-006h], 1                     ; d0 66 fa                    ; 0xc1afa vgabios.c:1049
    sal word [bp-018h], 1                     ; d1 66 e8                    ; 0xc1afd vgabios.c:1050
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc1b00 vgabios.c:1053
    jne short 01b6fh                          ; 75 69                       ; 0xc1b04
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1b06 vgabios.c:1054
    xor ah, ah                                ; 30 e4                       ; 0xc1b09
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc1b0b
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1b0e
    xor ah, ah                                ; 30 e4                       ; 0xc1b11
    mov dx, ax                                ; 89 c2                       ; 0xc1b13
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1b15
    jc short 01aefh                           ; 72 d5                       ; 0xc1b18
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc1b1a vgabios.c:1056
    add ax, word [bp-016h]                    ; 03 46 ea                    ; 0xc1b1d
    cmp ax, dx                                ; 39 d0                       ; 0xc1b20
    jnbe short 01b2ah                         ; 77 06                       ; 0xc1b22
    cmp byte [bp-00ah], 000h                  ; 80 7e f6 00                 ; 0xc1b24
    jne short 01b4bh                          ; 75 21                       ; 0xc1b28
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1b2a vgabios.c:1057
    xor ah, ah                                ; 30 e4                       ; 0xc1b2d
    push ax                                   ; 50                          ; 0xc1b2f
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc1b30
    push ax                                   ; 50                          ; 0xc1b33
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc1b34
    mov cx, ax                                ; 89 c1                       ; 0xc1b37
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1b39
    mov bx, ax                                ; 89 c3                       ; 0xc1b3c
    mov al, byte [bp-016h]                    ; 8a 46 ea                    ; 0xc1b3e
    mov dx, ax                                ; 89 c2                       ; 0xc1b41
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1b43
    call 015d0h                               ; e8 87 fa                    ; 0xc1b46
    jmp short 01b6ah                          ; eb 1f                       ; 0xc1b49 vgabios.c:1058
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc1b4b vgabios.c:1059
    xor ah, ah                                ; 30 e4                       ; 0xc1b4e
    push ax                                   ; 50                          ; 0xc1b50
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc1b51
    push ax                                   ; 50                          ; 0xc1b54
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1b55
    mov cx, ax                                ; 89 c1                       ; 0xc1b58
    mov al, byte [bp-016h]                    ; 8a 46 ea                    ; 0xc1b5a
    mov bx, ax                                ; 89 c3                       ; 0xc1b5d
    add al, byte [bp-00ah]                    ; 02 46 f6                    ; 0xc1b5f
    mov dx, ax                                ; 89 c2                       ; 0xc1b62
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1b64
    call 0150eh                               ; e8 a4 f9                    ; 0xc1b67
    inc word [bp-016h]                        ; ff 46 ea                    ; 0xc1b6a vgabios.c:1060
    jmp short 01b0eh                          ; eb 9f                       ; 0xc1b6d
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1b6f vgabios.c:1063
    xor ah, ah                                ; 30 e4                       ; 0xc1b72
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc1b74
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1b77
    xor ah, ah                                ; 30 e4                       ; 0xc1b7a
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1b7c
    jnbe short 01be5h                         ; 77 64                       ; 0xc1b7f
    mov dx, ax                                ; 89 c2                       ; 0xc1b81 vgabios.c:1065
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc1b83
    add ax, dx                                ; 01 d0                       ; 0xc1b86
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1b88
    jnbe short 01b93h                         ; 77 06                       ; 0xc1b8b
    cmp byte [bp-00ah], 000h                  ; 80 7e f6 00                 ; 0xc1b8d
    jne short 01bb4h                          ; 75 21                       ; 0xc1b91
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1b93 vgabios.c:1066
    xor ah, ah                                ; 30 e4                       ; 0xc1b96
    push ax                                   ; 50                          ; 0xc1b98
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc1b99
    push ax                                   ; 50                          ; 0xc1b9c
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc1b9d
    mov cx, ax                                ; 89 c1                       ; 0xc1ba0
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1ba2
    mov bx, ax                                ; 89 c3                       ; 0xc1ba5
    mov al, byte [bp-016h]                    ; 8a 46 ea                    ; 0xc1ba7
    mov dx, ax                                ; 89 c2                       ; 0xc1baa
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1bac
    call 015d0h                               ; e8 1e fa                    ; 0xc1baf
    jmp short 01bd6h                          ; eb 22                       ; 0xc1bb2 vgabios.c:1067
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc1bb4 vgabios.c:1068
    xor ah, ah                                ; 30 e4                       ; 0xc1bb7
    push ax                                   ; 50                          ; 0xc1bb9
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc1bba
    push ax                                   ; 50                          ; 0xc1bbd
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1bbe
    mov cx, ax                                ; 89 c1                       ; 0xc1bc1
    mov al, byte [bp-016h]                    ; 8a 46 ea                    ; 0xc1bc3
    sub al, byte [bp-00ah]                    ; 2a 46 f6                    ; 0xc1bc6
    mov bx, ax                                ; 89 c3                       ; 0xc1bc9
    mov al, byte [bp-016h]                    ; 8a 46 ea                    ; 0xc1bcb
    mov dx, ax                                ; 89 c2                       ; 0xc1bce
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1bd0
    call 0150eh                               ; e8 38 f9                    ; 0xc1bd3
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1bd6 vgabios.c:1069
    xor ah, ah                                ; 30 e4                       ; 0xc1bd9
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1bdb
    jc short 01be5h                           ; 72 05                       ; 0xc1bde
    dec word [bp-016h]                        ; ff 4e ea                    ; 0xc1be0 vgabios.c:1070
    jmp short 01b77h                          ; eb 92                       ; 0xc1be3
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc1be5 vgabios.c:1081
    pop di                                    ; 5f                          ; 0xc1be8
    pop si                                    ; 5e                          ; 0xc1be9
    pop bp                                    ; 5d                          ; 0xc1bea
    retn 00008h                               ; c2 08 00                    ; 0xc1beb
  ; disGetNextSymbol 0xc1bee LB 0x20f1 -> off=0x0 cb=00000000000000f8 uValue=00000000000c1bee 'write_gfx_char_pl4'
write_gfx_char_pl4:                          ; 0xc1bee LB 0xf8
    push bp                                   ; 55                          ; 0xc1bee vgabios.c:1084
    mov bp, sp                                ; 89 e5                       ; 0xc1bef
    push si                                   ; 56                          ; 0xc1bf1
    push di                                   ; 57                          ; 0xc1bf2
    sub sp, strict byte 0000eh                ; 83 ec 0e                    ; 0xc1bf3
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc1bf6
    mov byte [bp-006h], dl                    ; 88 56 fa                    ; 0xc1bf9
    mov byte [bp-00ah], bl                    ; 88 5e f6                    ; 0xc1bfc
    mov al, cl                                ; 88 c8                       ; 0xc1bff
    cmp byte [bp+006h], 010h                  ; 80 7e 06 10                 ; 0xc1c01 vgabios.c:1091
    je short 01c12h                           ; 74 0b                       ; 0xc1c05
    cmp byte [bp+006h], 00eh                  ; 80 7e 06 0e                 ; 0xc1c07
    jne short 01c17h                          ; 75 0a                       ; 0xc1c0b
    mov di, 05bf2h                            ; bf f2 5b                    ; 0xc1c0d vgabios.c:1093
    jmp short 01c1ah                          ; eb 08                       ; 0xc1c10 vgabios.c:1094
    mov di, 069f2h                            ; bf f2 69                    ; 0xc1c12 vgabios.c:1096
    jmp short 01c1ah                          ; eb 03                       ; 0xc1c15 vgabios.c:1097
    mov di, 053f2h                            ; bf f2 53                    ; 0xc1c17 vgabios.c:1099
    xor ah, ah                                ; 30 e4                       ; 0xc1c1a vgabios.c:1101
    mov bx, ax                                ; 89 c3                       ; 0xc1c1c
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc1c1e
    mov si, ax                                ; 89 c6                       ; 0xc1c21
    mov ax, bx                                ; 89 d8                       ; 0xc1c23
    imul si                                   ; f7 ee                       ; 0xc1c25
    mov bl, byte [bp+004h]                    ; 8a 5e 04                    ; 0xc1c27
    imul bx                                   ; f7 eb                       ; 0xc1c2a
    mov bx, ax                                ; 89 c3                       ; 0xc1c2c
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc1c2e
    xor ah, ah                                ; 30 e4                       ; 0xc1c31
    add ax, bx                                ; 01 d8                       ; 0xc1c33
    mov word [bp-010h], ax                    ; 89 46 f0                    ; 0xc1c35
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1c38 vgabios.c:1102
    xor ah, ah                                ; 30 e4                       ; 0xc1c3b
    imul si                                   ; f7 ee                       ; 0xc1c3d
    mov word [bp-00eh], ax                    ; 89 46 f2                    ; 0xc1c3f
    mov ax, 00f02h                            ; b8 02 0f                    ; 0xc1c42 vgabios.c:1103
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc1c45
    out DX, ax                                ; ef                          ; 0xc1c48
    mov ax, 00205h                            ; b8 05 02                    ; 0xc1c49 vgabios.c:1104
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1c4c
    out DX, ax                                ; ef                          ; 0xc1c4f
    test byte [bp-006h], 080h                 ; f6 46 fa 80                 ; 0xc1c50 vgabios.c:1105
    je short 01c5ch                           ; 74 06                       ; 0xc1c54
    mov ax, 01803h                            ; b8 03 18                    ; 0xc1c56 vgabios.c:1107
    out DX, ax                                ; ef                          ; 0xc1c59
    jmp short 01c60h                          ; eb 04                       ; 0xc1c5a vgabios.c:1109
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc1c5c vgabios.c:1111
    out DX, ax                                ; ef                          ; 0xc1c5f
    xor ch, ch                                ; 30 ed                       ; 0xc1c60 vgabios.c:1113
    cmp ch, byte [bp+006h]                    ; 3a 6e 06                    ; 0xc1c62
    jnc short 01cceh                          ; 73 67                       ; 0xc1c65
    mov al, ch                                ; 88 e8                       ; 0xc1c67 vgabios.c:1115
    xor ah, ah                                ; 30 e4                       ; 0xc1c69
    mov bl, byte [bp+004h]                    ; 8a 5e 04                    ; 0xc1c6b
    xor bh, bh                                ; 30 ff                       ; 0xc1c6e
    imul bx                                   ; f7 eb                       ; 0xc1c70
    mov si, word [bp-010h]                    ; 8b 76 f0                    ; 0xc1c72
    add si, ax                                ; 01 c6                       ; 0xc1c75
    mov byte [bp-008h], bh                    ; 88 7e f8                    ; 0xc1c77 vgabios.c:1116
    jmp short 01c8fh                          ; eb 13                       ; 0xc1c7a
    xor bx, bx                                ; 31 db                       ; 0xc1c7c vgabios.c:1127
    mov dx, si                                ; 89 f2                       ; 0xc1c7e
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc1c80
    call 031cch                               ; e8 46 15                    ; 0xc1c83
    inc byte [bp-008h]                        ; fe 46 f8                    ; 0xc1c86 vgabios.c:1129
    cmp byte [bp-008h], 008h                  ; 80 7e f8 08                 ; 0xc1c89
    jnc short 01ccah                          ; 73 3b                       ; 0xc1c8d
    mov cl, byte [bp-008h]                    ; 8a 4e f8                    ; 0xc1c8f
    mov ax, 00080h                            ; b8 80 00                    ; 0xc1c92
    sar ax, CL                                ; d3 f8                       ; 0xc1c95
    xor ah, ah                                ; 30 e4                       ; 0xc1c97
    mov word [bp-012h], ax                    ; 89 46 ee                    ; 0xc1c99
    mov ah, byte [bp-012h]                    ; 8a 66 ee                    ; 0xc1c9c
    xor al, al                                ; 30 c0                       ; 0xc1c9f
    or AL, strict byte 008h                   ; 0c 08                       ; 0xc1ca1
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1ca3
    out DX, ax                                ; ef                          ; 0xc1ca6
    mov dx, si                                ; 89 f2                       ; 0xc1ca7
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc1ca9
    call 031beh                               ; e8 0f 15                    ; 0xc1cac
    mov al, ch                                ; 88 e8                       ; 0xc1caf
    xor ah, ah                                ; 30 e4                       ; 0xc1cb1
    mov bx, word [bp-00eh]                    ; 8b 5e f2                    ; 0xc1cb3
    add bx, ax                                ; 01 c3                       ; 0xc1cb6
    add bx, di                                ; 01 fb                       ; 0xc1cb8
    mov al, byte [bx]                         ; 8a 07                       ; 0xc1cba
    test word [bp-012h], ax                   ; 85 46 ee                    ; 0xc1cbc
    je short 01c7ch                           ; 74 bb                       ; 0xc1cbf
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1cc1
    and AL, strict byte 00fh                  ; 24 0f                       ; 0xc1cc4
    mov bx, ax                                ; 89 c3                       ; 0xc1cc6
    jmp short 01c7eh                          ; eb b4                       ; 0xc1cc8
    db  0feh, 0c5h
    ; inc ch                                    ; fe c5                     ; 0xc1cca vgabios.c:1130
    jmp short 01c62h                          ; eb 94                       ; 0xc1ccc
    mov ax, 0ff08h                            ; b8 08 ff                    ; 0xc1cce vgabios.c:1131
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1cd1
    out DX, ax                                ; ef                          ; 0xc1cd4
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc1cd5 vgabios.c:1132
    out DX, ax                                ; ef                          ; 0xc1cd8
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc1cd9 vgabios.c:1133
    out DX, ax                                ; ef                          ; 0xc1cdc
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc1cdd vgabios.c:1134
    pop di                                    ; 5f                          ; 0xc1ce0
    pop si                                    ; 5e                          ; 0xc1ce1
    pop bp                                    ; 5d                          ; 0xc1ce2
    retn 00004h                               ; c2 04 00                    ; 0xc1ce3
  ; disGetNextSymbol 0xc1ce6 LB 0x1ff9 -> off=0x0 cb=000000000000013a uValue=00000000000c1ce6 'write_gfx_char_cga'
write_gfx_char_cga:                          ; 0xc1ce6 LB 0x13a
    push bp                                   ; 55                          ; 0xc1ce6 vgabios.c:1137
    mov bp, sp                                ; 89 e5                       ; 0xc1ce7
    push si                                   ; 56                          ; 0xc1ce9
    push di                                   ; 57                          ; 0xc1cea
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc1ceb
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc1cee
    mov byte [bp-00ah], dl                    ; 88 56 f6                    ; 0xc1cf1
    mov si, 053f2h                            ; be f2 53                    ; 0xc1cf4 vgabios.c:1144
    xor bh, bh                                ; 30 ff                       ; 0xc1cf7 vgabios.c:1145
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc1cf9
    xor ah, ah                                ; 30 e4                       ; 0xc1cfc
    mov di, ax                                ; 89 c7                       ; 0xc1cfe
    mov ax, bx                                ; 89 d8                       ; 0xc1d00
    imul di                                   ; f7 ef                       ; 0xc1d02
    mov bx, ax                                ; 89 c3                       ; 0xc1d04
    mov al, cl                                ; 88 c8                       ; 0xc1d06
    xor ah, ah                                ; 30 e4                       ; 0xc1d08
    mov cx, 00140h                            ; b9 40 01                    ; 0xc1d0a
    imul cx                                   ; f7 e9                       ; 0xc1d0d
    add bx, ax                                ; 01 c3                       ; 0xc1d0f
    mov word [bp-00eh], bx                    ; 89 5e f2                    ; 0xc1d11
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1d14 vgabios.c:1146
    xor ah, ah                                ; 30 e4                       ; 0xc1d17
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc1d19
    mov di, ax                                ; 89 c7                       ; 0xc1d1b
    sal di, CL                                ; d3 e7                       ; 0xc1d1d
    mov byte [bp-006h], ah                    ; 88 66 fa                    ; 0xc1d1f vgabios.c:1147
    jmp near 01d77h                           ; e9 52 00                    ; 0xc1d22
    xor al, al                                ; 30 c0                       ; 0xc1d25 vgabios.c:1160
    xor ah, ah                                ; 30 e4                       ; 0xc1d27 vgabios.c:1162
    jmp short 01d36h                          ; eb 0b                       ; 0xc1d29
    or al, bl                                 ; 08 d8                       ; 0xc1d2b vgabios.c:1172
    shr ch, 1                                 ; d0 ed                       ; 0xc1d2d vgabios.c:1175
    db  0feh, 0c4h
    ; inc ah                                    ; fe c4                     ; 0xc1d2f vgabios.c:1176
    cmp ah, 008h                              ; 80 fc 08                    ; 0xc1d31
    jnc short 01d61h                          ; 73 2b                       ; 0xc1d34
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc1d36
    xor bh, bh                                ; 30 ff                       ; 0xc1d39
    add bx, di                                ; 01 fb                       ; 0xc1d3b
    add bx, si                                ; 01 f3                       ; 0xc1d3d
    mov bl, byte [bx]                         ; 8a 1f                       ; 0xc1d3f
    xor bh, bh                                ; 30 ff                       ; 0xc1d41
    mov dx, bx                                ; 89 da                       ; 0xc1d43
    mov bl, ch                                ; 88 eb                       ; 0xc1d45
    test dx, bx                               ; 85 da                       ; 0xc1d47
    je short 01d2dh                           ; 74 e2                       ; 0xc1d49
    mov CL, strict byte 007h                  ; b1 07                       ; 0xc1d4b
    sub cl, ah                                ; 28 e1                       ; 0xc1d4d
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc1d4f
    and bl, 001h                              ; 80 e3 01                    ; 0xc1d52
    sal bl, CL                                ; d2 e3                       ; 0xc1d55
    test byte [bp-00ah], 080h                 ; f6 46 f6 80                 ; 0xc1d57
    je short 01d2bh                           ; 74 ce                       ; 0xc1d5b
    xor al, bl                                ; 30 d8                       ; 0xc1d5d
    jmp short 01d2dh                          ; eb cc                       ; 0xc1d5f
    xor ah, ah                                ; 30 e4                       ; 0xc1d61 vgabios.c:1177
    mov bx, ax                                ; 89 c3                       ; 0xc1d63
    mov dx, word [bp-00ch]                    ; 8b 56 f4                    ; 0xc1d65
    mov ax, 0b800h                            ; b8 00 b8                    ; 0xc1d68
    call 031cch                               ; e8 5e 14                    ; 0xc1d6b
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc1d6e vgabios.c:1179
    cmp byte [bp-006h], 008h                  ; 80 7e fa 08                 ; 0xc1d71
    jnc short 01dc9h                          ; 73 52                       ; 0xc1d75
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1d77
    xor ah, ah                                ; 30 e4                       ; 0xc1d7a
    sar ax, 1                                 ; d1 f8                       ; 0xc1d7c
    mov bx, strict word 00050h                ; bb 50 00                    ; 0xc1d7e
    imul bx                                   ; f7 eb                       ; 0xc1d81
    mov dx, word [bp-00eh]                    ; 8b 56 f2                    ; 0xc1d83
    add dx, ax                                ; 01 c2                       ; 0xc1d86
    mov word [bp-00ch], dx                    ; 89 56 f4                    ; 0xc1d88
    test byte [bp-006h], 001h                 ; f6 46 fa 01                 ; 0xc1d8b
    je short 01d95h                           ; 74 04                       ; 0xc1d8f
    add byte [bp-00bh], 020h                  ; 80 46 f5 20                 ; 0xc1d91
    mov CH, strict byte 080h                  ; b5 80                       ; 0xc1d95
    cmp byte [bp+006h], 001h                  ; 80 7e 06 01                 ; 0xc1d97
    jne short 01daeh                          ; 75 11                       ; 0xc1d9b
    test byte [bp-00ah], ch                   ; 84 6e f6                    ; 0xc1d9d
    je short 01d25h                           ; 74 83                       ; 0xc1da0
    mov dx, word [bp-00ch]                    ; 8b 56 f4                    ; 0xc1da2
    mov ax, 0b800h                            ; b8 00 b8                    ; 0xc1da5
    call 031beh                               ; e8 13 14                    ; 0xc1da8
    jmp near 01d27h                           ; e9 79 ff                    ; 0xc1dab
    test ch, ch                               ; 84 ed                       ; 0xc1dae vgabios.c:1181
    jbe short 01d6eh                          ; 76 bc                       ; 0xc1db0
    test byte [bp-00ah], 080h                 ; f6 46 f6 80                 ; 0xc1db2 vgabios.c:1183
    je short 01dc3h                           ; 74 0b                       ; 0xc1db6
    mov dx, word [bp-00ch]                    ; 8b 56 f4                    ; 0xc1db8 vgabios.c:1185
    mov ax, 0b800h                            ; b8 00 b8                    ; 0xc1dbb
    call 031beh                               ; e8 fd 13                    ; 0xc1dbe
    jmp short 01dc5h                          ; eb 02                       ; 0xc1dc1 vgabios.c:1187
    xor al, al                                ; 30 c0                       ; 0xc1dc3 vgabios.c:1189
    xor ah, ah                                ; 30 e4                       ; 0xc1dc5 vgabios.c:1191
    jmp short 01dd0h                          ; eb 07                       ; 0xc1dc7
    jmp short 01e17h                          ; eb 4c                       ; 0xc1dc9
    cmp ah, 004h                              ; 80 fc 04                    ; 0xc1dcb
    jnc short 01e05h                          ; 73 35                       ; 0xc1dce
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc1dd0 vgabios.c:1193
    xor bh, bh                                ; 30 ff                       ; 0xc1dd3
    add bx, di                                ; 01 fb                       ; 0xc1dd5
    add bx, si                                ; 01 f3                       ; 0xc1dd7
    mov bl, byte [bx]                         ; 8a 1f                       ; 0xc1dd9
    xor bh, bh                                ; 30 ff                       ; 0xc1ddb
    mov dx, bx                                ; 89 da                       ; 0xc1ddd
    mov bl, ch                                ; 88 eb                       ; 0xc1ddf
    test dx, bx                               ; 85 da                       ; 0xc1de1
    je short 01dffh                           ; 74 1a                       ; 0xc1de3
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc1de5 vgabios.c:1194
    sub cl, ah                                ; 28 e1                       ; 0xc1de7
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc1de9
    and bl, 003h                              ; 80 e3 03                    ; 0xc1dec
    sal cl, 1                                 ; d0 e1                       ; 0xc1def
    sal bl, CL                                ; d2 e3                       ; 0xc1df1
    test byte [bp-00ah], 080h                 ; f6 46 f6 80                 ; 0xc1df3 vgabios.c:1195
    je short 01dfdh                           ; 74 04                       ; 0xc1df7
    xor al, bl                                ; 30 d8                       ; 0xc1df9 vgabios.c:1197
    jmp short 01dffh                          ; eb 02                       ; 0xc1dfb vgabios.c:1199
    or al, bl                                 ; 08 d8                       ; 0xc1dfd vgabios.c:1201
    shr ch, 1                                 ; d0 ed                       ; 0xc1dff vgabios.c:1204
    db  0feh, 0c4h
    ; inc ah                                    ; fe c4                     ; 0xc1e01 vgabios.c:1205
    jmp short 01dcbh                          ; eb c6                       ; 0xc1e03
    xor ah, ah                                ; 30 e4                       ; 0xc1e05 vgabios.c:1206
    mov bx, ax                                ; 89 c3                       ; 0xc1e07
    mov dx, word [bp-00ch]                    ; 8b 56 f4                    ; 0xc1e09
    mov ax, 0b800h                            ; b8 00 b8                    ; 0xc1e0c
    call 031cch                               ; e8 ba 13                    ; 0xc1e0f
    inc word [bp-00ch]                        ; ff 46 f4                    ; 0xc1e12 vgabios.c:1207
    jmp short 01daeh                          ; eb 97                       ; 0xc1e15 vgabios.c:1208
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc1e17 vgabios.c:1211
    pop di                                    ; 5f                          ; 0xc1e1a
    pop si                                    ; 5e                          ; 0xc1e1b
    pop bp                                    ; 5d                          ; 0xc1e1c
    retn 00004h                               ; c2 04 00                    ; 0xc1e1d
  ; disGetNextSymbol 0xc1e20 LB 0x1ebf -> off=0x0 cb=00000000000000ac uValue=00000000000c1e20 'write_gfx_char_lin'
write_gfx_char_lin:                          ; 0xc1e20 LB 0xac
    push bp                                   ; 55                          ; 0xc1e20 vgabios.c:1214
    mov bp, sp                                ; 89 e5                       ; 0xc1e21
    push si                                   ; 56                          ; 0xc1e23
    push di                                   ; 57                          ; 0xc1e24
    sub sp, strict byte 0000ch                ; 83 ec 0c                    ; 0xc1e25
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc1e28
    mov byte [bp-00ch], dl                    ; 88 56 f4                    ; 0xc1e2b
    mov byte [bp-006h], bl                    ; 88 5e fa                    ; 0xc1e2e
    mov al, cl                                ; 88 c8                       ; 0xc1e31
    mov si, 053f2h                            ; be f2 53                    ; 0xc1e33 vgabios.c:1221
    xor ah, ah                                ; 30 e4                       ; 0xc1e36 vgabios.c:1222
    mov bl, byte [bp+004h]                    ; 8a 5e 04                    ; 0xc1e38
    xor bh, bh                                ; 30 ff                       ; 0xc1e3b
    imul bx                                   ; f7 eb                       ; 0xc1e3d
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc1e3f
    sal ax, CL                                ; d3 e0                       ; 0xc1e41
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc1e43
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc1e46
    mov dx, bx                                ; 89 da                       ; 0xc1e48
    sal dx, CL                                ; d3 e2                       ; 0xc1e4a
    add dx, ax                                ; 01 c2                       ; 0xc1e4c
    mov word [bp-00eh], dx                    ; 89 56 f2                    ; 0xc1e4e
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc1e51 vgabios.c:1223
    mov di, bx                                ; 89 df                       ; 0xc1e54
    sal di, CL                                ; d3 e7                       ; 0xc1e56
    xor ch, ch                                ; 30 ed                       ; 0xc1e58 vgabios.c:1224
    jmp short 01ea0h                          ; eb 44                       ; 0xc1e5a
    cmp cl, 008h                              ; 80 f9 08                    ; 0xc1e5c vgabios.c:1228
    jnc short 01e99h                          ; 73 38                       ; 0xc1e5f
    xor dl, dl                                ; 30 d2                       ; 0xc1e61 vgabios.c:1230
    mov al, ch                                ; 88 e8                       ; 0xc1e63 vgabios.c:1231
    xor ah, ah                                ; 30 e4                       ; 0xc1e65
    add ax, di                                ; 01 f8                       ; 0xc1e67
    mov bx, si                                ; 89 f3                       ; 0xc1e69
    add bx, ax                                ; 01 c3                       ; 0xc1e6b
    mov al, byte [bx]                         ; 8a 07                       ; 0xc1e6d
    xor ah, ah                                ; 30 e4                       ; 0xc1e6f
    mov bl, byte [bp-008h]                    ; 8a 5e f8                    ; 0xc1e71
    xor bh, bh                                ; 30 ff                       ; 0xc1e74
    test ax, bx                               ; 85 d8                       ; 0xc1e76
    je short 01e7dh                           ; 74 03                       ; 0xc1e78
    mov dl, byte [bp-00ch]                    ; 8a 56 f4                    ; 0xc1e7a vgabios.c:1233
    mov bl, dl                                ; 88 d3                       ; 0xc1e7d vgabios.c:1235
    xor bh, bh                                ; 30 ff                       ; 0xc1e7f
    mov ax, bx                                ; 89 d8                       ; 0xc1e81
    mov bl, cl                                ; 88 cb                       ; 0xc1e83
    mov dx, word [bp-010h]                    ; 8b 56 f0                    ; 0xc1e85
    add dx, bx                                ; 01 da                       ; 0xc1e88
    mov bx, ax                                ; 89 c3                       ; 0xc1e8a
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc1e8c
    call 031cch                               ; e8 3a 13                    ; 0xc1e8f
    shr byte [bp-008h], 1                     ; d0 6e f8                    ; 0xc1e92 vgabios.c:1236
    db  0feh, 0c1h
    ; inc cl                                    ; fe c1                     ; 0xc1e95 vgabios.c:1237
    jmp short 01e5ch                          ; eb c3                       ; 0xc1e97
    db  0feh, 0c5h
    ; inc ch                                    ; fe c5                     ; 0xc1e99 vgabios.c:1238
    cmp ch, 008h                              ; 80 fd 08                    ; 0xc1e9b
    jnc short 01ec3h                          ; 73 23                       ; 0xc1e9e
    mov bl, ch                                ; 88 eb                       ; 0xc1ea0
    xor bh, bh                                ; 30 ff                       ; 0xc1ea2
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1ea4
    xor ah, ah                                ; 30 e4                       ; 0xc1ea7
    mov dx, ax                                ; 89 c2                       ; 0xc1ea9
    mov ax, bx                                ; 89 d8                       ; 0xc1eab
    imul dx                                   ; f7 ea                       ; 0xc1ead
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc1eaf
    sal ax, CL                                ; d3 e0                       ; 0xc1eb1
    mov dx, word [bp-00eh]                    ; 8b 56 f2                    ; 0xc1eb3
    add dx, ax                                ; 01 c2                       ; 0xc1eb6
    mov word [bp-010h], dx                    ; 89 56 f0                    ; 0xc1eb8
    mov byte [bp-008h], 080h                  ; c6 46 f8 80                 ; 0xc1ebb
    xor cl, cl                                ; 30 c9                       ; 0xc1ebf
    jmp short 01e61h                          ; eb 9e                       ; 0xc1ec1
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc1ec3 vgabios.c:1239
    pop di                                    ; 5f                          ; 0xc1ec6
    pop si                                    ; 5e                          ; 0xc1ec7
    pop bp                                    ; 5d                          ; 0xc1ec8
    retn 00002h                               ; c2 02 00                    ; 0xc1ec9
  ; disGetNextSymbol 0xc1ecc LB 0x1e13 -> off=0x0 cb=0000000000000192 uValue=00000000000c1ecc 'biosfn_write_char_attr'
biosfn_write_char_attr:                      ; 0xc1ecc LB 0x192
    push bp                                   ; 55                          ; 0xc1ecc vgabios.c:1242
    mov bp, sp                                ; 89 e5                       ; 0xc1ecd
    push si                                   ; 56                          ; 0xc1ecf
    push di                                   ; 57                          ; 0xc1ed0
    sub sp, strict byte 0001ah                ; 83 ec 1a                    ; 0xc1ed1
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc1ed4
    mov byte [bp-012h], dl                    ; 88 56 ee                    ; 0xc1ed7
    mov byte [bp-006h], bl                    ; 88 5e fa                    ; 0xc1eda
    mov si, cx                                ; 89 ce                       ; 0xc1edd
    mov dx, strict word 00049h                ; ba 49 00                    ; 0xc1edf vgabios.c:1249
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1ee2
    call 031beh                               ; e8 d6 12                    ; 0xc1ee5
    xor ah, ah                                ; 30 e4                       ; 0xc1ee8 vgabios.c:1250
    call 03193h                               ; e8 a6 12                    ; 0xc1eea
    mov cl, al                                ; 88 c1                       ; 0xc1eed
    mov byte [bp-014h], al                    ; 88 46 ec                    ; 0xc1eef
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc1ef2 vgabios.c:1251
    jne short 01ef9h                          ; 75 03                       ; 0xc1ef4
    jmp near 02057h                           ; e9 5e 01                    ; 0xc1ef6
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc1ef9 vgabios.c:1254
    xor ah, ah                                ; 30 e4                       ; 0xc1efc
    lea bx, [bp-01eh]                         ; 8d 5e e2                    ; 0xc1efe
    lea dx, [bp-01ch]                         ; 8d 56 e4                    ; 0xc1f01
    call 00a8ch                               ; e8 85 eb                    ; 0xc1f04
    mov al, byte [bp-01eh]                    ; 8a 46 e2                    ; 0xc1f07 vgabios.c:1255
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc1f0a
    mov ax, word [bp-01eh]                    ; 8b 46 e2                    ; 0xc1f0d
    mov byte [bp-00eh], ah                    ; 88 66 f2                    ; 0xc1f10
    mov dx, 00084h                            ; ba 84 00                    ; 0xc1f13 vgabios.c:1258
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1f16
    call 031beh                               ; e8 a2 12                    ; 0xc1f19
    xor ah, ah                                ; 30 e4                       ; 0xc1f1c
    inc ax                                    ; 40                          ; 0xc1f1e
    mov word [bp-01ah], ax                    ; 89 46 e6                    ; 0xc1f1f
    mov dx, strict word 0004ah                ; ba 4a 00                    ; 0xc1f22 vgabios.c:1259
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1f25
    call 031dah                               ; e8 af 12                    ; 0xc1f28
    mov bx, ax                                ; 89 c3                       ; 0xc1f2b
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc1f2d
    mov al, cl                                ; 88 c8                       ; 0xc1f30 vgabios.c:1261
    xor ah, ah                                ; 30 e4                       ; 0xc1f32
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc1f34
    mov di, ax                                ; 89 c7                       ; 0xc1f36
    sal di, CL                                ; d3 e7                       ; 0xc1f38
    cmp byte [di+04635h], 000h                ; 80 bd 35 46 00              ; 0xc1f3a
    jne short 01f8ah                          ; 75 49                       ; 0xc1f3f
    mov ax, bx                                ; 89 d8                       ; 0xc1f41 vgabios.c:1264
    mul word [bp-01ah]                        ; f7 66 e6                    ; 0xc1f43
    sal ax, 1                                 ; d1 e0                       ; 0xc1f46
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc1f48
    mov cx, ax                                ; 89 c1                       ; 0xc1f4a
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc1f4c
    xor ah, ah                                ; 30 e4                       ; 0xc1f4f
    mov dx, ax                                ; 89 c2                       ; 0xc1f51
    mov ax, cx                                ; 89 c8                       ; 0xc1f53
    inc ax                                    ; 40                          ; 0xc1f55
    mul dx                                    ; f7 e2                       ; 0xc1f56
    mov cx, ax                                ; 89 c1                       ; 0xc1f58
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1f5a
    xor ah, ah                                ; 30 e4                       ; 0xc1f5d
    mul bx                                    ; f7 e3                       ; 0xc1f5f
    mov bl, byte [bp-00ch]                    ; 8a 5e f4                    ; 0xc1f61
    xor bh, bh                                ; 30 ff                       ; 0xc1f64
    mov dx, ax                                ; 89 c2                       ; 0xc1f66
    add dx, bx                                ; 01 da                       ; 0xc1f68
    sal dx, 1                                 ; d1 e2                       ; 0xc1f6a
    add dx, cx                                ; 01 ca                       ; 0xc1f6c
    mov bh, byte [bp-006h]                    ; 8a 7e fa                    ; 0xc1f6e vgabios.c:1266
    mov bl, byte [bp-008h]                    ; 8a 5e f8                    ; 0xc1f71
    mov word [bp-01ch], bx                    ; 89 5e e4                    ; 0xc1f74
    mov ax, word [bp-01ch]                    ; 8b 46 e4                    ; 0xc1f77 vgabios.c:1267
    mov es, [di+04638h]                       ; 8e 85 38 46                 ; 0xc1f7a
    mov cx, si                                ; 89 f1                       ; 0xc1f7e
    mov di, dx                                ; 89 d7                       ; 0xc1f80
    cld                                       ; fc                          ; 0xc1f82
    jcxz 01f87h                               ; e3 02                       ; 0xc1f83
    rep stosw                                 ; f3 ab                       ; 0xc1f85
    jmp near 02057h                           ; e9 cd 00                    ; 0xc1f87 vgabios.c:1269
    mov bx, ax                                ; 89 c3                       ; 0xc1f8a vgabios.c:1272
    mov al, byte [bx+046b4h]                  ; 8a 87 b4 46                 ; 0xc1f8c
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc1f90
    mov bx, ax                                ; 89 c3                       ; 0xc1f92
    sal bx, CL                                ; d3 e3                       ; 0xc1f94
    mov al, byte [bx+046cah]                  ; 8a 87 ca 46                 ; 0xc1f96
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc1f9a
    mov al, byte [di+04637h]                  ; 8a 85 37 46                 ; 0xc1f9d vgabios.c:1273
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc1fa1
    dec si                                    ; 4e                          ; 0xc1fa4 vgabios.c:1274
    cmp si, strict byte 0ffffh                ; 83 fe ff                    ; 0xc1fa5
    je short 01fb4h                           ; 74 0a                       ; 0xc1fa8
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1faa
    xor ah, ah                                ; 30 e4                       ; 0xc1fad
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1faf
    jc short 01fb7h                           ; 72 03                       ; 0xc1fb2
    jmp near 02057h                           ; e9 a0 00                    ; 0xc1fb4
    mov al, byte [bp-014h]                    ; 8a 46 ec                    ; 0xc1fb7 vgabios.c:1276
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc1fba
    mov bx, ax                                ; 89 c3                       ; 0xc1fbc
    sal bx, CL                                ; d3 e3                       ; 0xc1fbe
    mov al, byte [bx+04636h]                  ; 8a 87 36 46                 ; 0xc1fc0
    cmp al, cl                                ; 38 c8                       ; 0xc1fc4
    jc short 01fd5h                           ; 72 0d                       ; 0xc1fc6
    jbe short 01fdbh                          ; 76 11                       ; 0xc1fc8
    cmp AL, strict byte 005h                  ; 3c 05                       ; 0xc1fca
    je short 02030h                           ; 74 62                       ; 0xc1fcc
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc1fce
    je short 01fdbh                           ; 74 09                       ; 0xc1fd0
    jmp near 02051h                           ; e9 7c 00                    ; 0xc1fd2
    cmp AL, strict byte 002h                  ; 3c 02                       ; 0xc1fd5
    je short 02004h                           ; 74 2b                       ; 0xc1fd7
    jmp short 02051h                          ; eb 76                       ; 0xc1fd9
    mov bl, byte [bp-010h]                    ; 8a 5e f0                    ; 0xc1fdb vgabios.c:1280
    xor bh, bh                                ; 30 ff                       ; 0xc1fde
    push bx                                   ; 53                          ; 0xc1fe0
    mov bl, byte [bp-016h]                    ; 8a 5e ea                    ; 0xc1fe1
    push bx                                   ; 53                          ; 0xc1fe4
    mov bl, byte [bp-00eh]                    ; 8a 5e f2                    ; 0xc1fe5
    mov cx, bx                                ; 89 d9                       ; 0xc1fe8
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1fea
    xor ah, ah                                ; 30 e4                       ; 0xc1fed
    mov dx, ax                                ; 89 c2                       ; 0xc1fef
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1ff1
    mov di, ax                                ; 89 c7                       ; 0xc1ff4
    mov bl, byte [bp-008h]                    ; 8a 5e f8                    ; 0xc1ff6
    mov ax, bx                                ; 89 d8                       ; 0xc1ff9
    mov bx, dx                                ; 89 d3                       ; 0xc1ffb
    mov dx, di                                ; 89 fa                       ; 0xc1ffd
    call 01beeh                               ; e8 ec fb                    ; 0xc1fff
    jmp short 02051h                          ; eb 4d                       ; 0xc2002 vgabios.c:1281
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2004 vgabios.c:1283
    push ax                                   ; 50                          ; 0xc2007
    mov al, byte [bp-016h]                    ; 8a 46 ea                    ; 0xc2008
    push ax                                   ; 50                          ; 0xc200b
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc200c
    mov cx, ax                                ; 89 c1                       ; 0xc200f
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc2011
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc2014
    xor bh, bh                                ; 30 ff                       ; 0xc2017
    mov dx, bx                                ; 89 da                       ; 0xc2019
    mov bl, byte [bp-008h]                    ; 8a 5e f8                    ; 0xc201b
    mov byte [bp-018h], bl                    ; 88 5e e8                    ; 0xc201e
    mov byte [bp-017h], ah                    ; 88 66 e9                    ; 0xc2021
    mov di, word [bp-018h]                    ; 8b 7e e8                    ; 0xc2024
    mov bx, ax                                ; 89 c3                       ; 0xc2027
    mov ax, di                                ; 89 f8                       ; 0xc2029
    call 01ce6h                               ; e8 b8 fc                    ; 0xc202b
    jmp short 02051h                          ; eb 21                       ; 0xc202e vgabios.c:1284
    mov bl, byte [bp-016h]                    ; 8a 5e ea                    ; 0xc2030 vgabios.c:1286
    xor bh, bh                                ; 30 ff                       ; 0xc2033
    push bx                                   ; 53                          ; 0xc2035
    mov bl, byte [bp-00eh]                    ; 8a 5e f2                    ; 0xc2036
    mov cx, bx                                ; 89 d9                       ; 0xc2039
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc203b
    mov dx, ax                                ; 89 c2                       ; 0xc203e
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc2040
    mov di, bx                                ; 89 df                       ; 0xc2043
    mov bl, byte [bp-008h]                    ; 8a 5e f8                    ; 0xc2045
    mov ax, bx                                ; 89 d8                       ; 0xc2048
    mov bx, dx                                ; 89 d3                       ; 0xc204a
    mov dx, di                                ; 89 fa                       ; 0xc204c
    call 01e20h                               ; e8 cf fd                    ; 0xc204e
    inc byte [bp-00ch]                        ; fe 46 f4                    ; 0xc2051 vgabios.c:1293
    jmp near 01fa4h                           ; e9 4d ff                    ; 0xc2054 vgabios.c:1294
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc2057 vgabios.c:1296
    pop di                                    ; 5f                          ; 0xc205a
    pop si                                    ; 5e                          ; 0xc205b
    pop bp                                    ; 5d                          ; 0xc205c
    retn                                      ; c3                          ; 0xc205d
  ; disGetNextSymbol 0xc205e LB 0x1c81 -> off=0x0 cb=0000000000000193 uValue=00000000000c205e 'biosfn_write_char_only'
biosfn_write_char_only:                      ; 0xc205e LB 0x193
    push bp                                   ; 55                          ; 0xc205e vgabios.c:1299
    mov bp, sp                                ; 89 e5                       ; 0xc205f
    push si                                   ; 56                          ; 0xc2061
    push di                                   ; 57                          ; 0xc2062
    sub sp, strict byte 00018h                ; 83 ec 18                    ; 0xc2063
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc2066
    mov byte [bp-008h], dl                    ; 88 56 f8                    ; 0xc2069
    mov byte [bp-00ah], bl                    ; 88 5e f6                    ; 0xc206c
    mov si, cx                                ; 89 ce                       ; 0xc206f
    mov dx, strict word 00049h                ; ba 49 00                    ; 0xc2071 vgabios.c:1306
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2074
    call 031beh                               ; e8 44 11                    ; 0xc2077
    xor ah, ah                                ; 30 e4                       ; 0xc207a vgabios.c:1307
    call 03193h                               ; e8 14 11                    ; 0xc207c
    mov cl, al                                ; 88 c1                       ; 0xc207f
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc2081
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc2084 vgabios.c:1308
    jne short 0208bh                          ; 75 03                       ; 0xc2086
    jmp near 021eah                           ; e9 5f 01                    ; 0xc2088
    mov bl, byte [bp-008h]                    ; 8a 5e f8                    ; 0xc208b vgabios.c:1311
    xor bh, bh                                ; 30 ff                       ; 0xc208e
    mov ax, bx                                ; 89 d8                       ; 0xc2090
    lea bx, [bp-01ch]                         ; 8d 5e e4                    ; 0xc2092
    lea dx, [bp-01ah]                         ; 8d 56 e6                    ; 0xc2095
    call 00a8ch                               ; e8 f1 e9                    ; 0xc2098
    mov al, byte [bp-01ch]                    ; 8a 46 e4                    ; 0xc209b vgabios.c:1312
    mov byte [bp-00eh], al                    ; 88 46 f2                    ; 0xc209e
    mov ax, word [bp-01ch]                    ; 8b 46 e4                    ; 0xc20a1
    mov byte [bp-014h], ah                    ; 88 66 ec                    ; 0xc20a4
    mov dx, 00084h                            ; ba 84 00                    ; 0xc20a7 vgabios.c:1315
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc20aa
    call 031beh                               ; e8 0e 11                    ; 0xc20ad
    mov bl, al                                ; 88 c3                       ; 0xc20b0
    xor bh, bh                                ; 30 ff                       ; 0xc20b2
    inc bx                                    ; 43                          ; 0xc20b4
    mov word [bp-018h], bx                    ; 89 5e e8                    ; 0xc20b5
    mov dx, strict word 0004ah                ; ba 4a 00                    ; 0xc20b8 vgabios.c:1316
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc20bb
    call 031dah                               ; e8 19 11                    ; 0xc20be
    mov di, ax                                ; 89 c7                       ; 0xc20c1
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc20c3
    mov bl, cl                                ; 88 cb                       ; 0xc20c6 vgabios.c:1318
    xor bh, bh                                ; 30 ff                       ; 0xc20c8
    mov ax, bx                                ; 89 d8                       ; 0xc20ca
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc20cc
    sal bx, CL                                ; d3 e3                       ; 0xc20ce
    cmp byte [bx+04635h], 000h                ; 80 bf 35 46 00              ; 0xc20d0
    jne short 02125h                          ; 75 4e                       ; 0xc20d5
    mov ax, di                                ; 89 f8                       ; 0xc20d7 vgabios.c:1321
    mul word [bp-018h]                        ; f7 66 e8                    ; 0xc20d9
    sal ax, 1                                 ; d1 e0                       ; 0xc20dc
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc20de
    mov bl, byte [bp-008h]                    ; 8a 5e f8                    ; 0xc20e0
    xor bh, bh                                ; 30 ff                       ; 0xc20e3
    inc ax                                    ; 40                          ; 0xc20e5
    mul bx                                    ; f7 e3                       ; 0xc20e6
    mov cx, ax                                ; 89 c1                       ; 0xc20e8
    mov bl, byte [bp-014h]                    ; 8a 5e ec                    ; 0xc20ea
    mov ax, bx                                ; 89 d8                       ; 0xc20ed
    mul di                                    ; f7 e7                       ; 0xc20ef
    mov dl, byte [bp-00eh]                    ; 8a 56 f2                    ; 0xc20f1
    xor dh, dh                                ; 30 f6                       ; 0xc20f4
    add ax, dx                                ; 01 d0                       ; 0xc20f6
    sal ax, 1                                 ; d1 e0                       ; 0xc20f8
    mov di, cx                                ; 89 cf                       ; 0xc20fa
    add di, ax                                ; 01 c7                       ; 0xc20fc
    dec si                                    ; 4e                          ; 0xc20fe vgabios.c:1323
    cmp si, strict byte 0ffffh                ; 83 fe ff                    ; 0xc20ff
    je short 02088h                           ; 74 84                       ; 0xc2102
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc2104 vgabios.c:1324
    xor dh, dh                                ; 30 f6                       ; 0xc2107
    mov ax, dx                                ; 89 d0                       ; 0xc2109
    mov dl, byte [bp-00ch]                    ; 8a 56 f4                    ; 0xc210b
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc210e
    mov bx, dx                                ; 89 d3                       ; 0xc2110
    sal bx, CL                                ; d3 e3                       ; 0xc2112
    mov cx, word [bx+04638h]                  ; 8b 8f 38 46                 ; 0xc2114
    mov bx, ax                                ; 89 c3                       ; 0xc2118
    mov dx, di                                ; 89 fa                       ; 0xc211a
    mov ax, cx                                ; 89 c8                       ; 0xc211c
    call 031cch                               ; e8 ab 10                    ; 0xc211e
    inc di                                    ; 47                          ; 0xc2121 vgabios.c:1325
    inc di                                    ; 47                          ; 0xc2122
    jmp short 020feh                          ; eb d9                       ; 0xc2123 vgabios.c:1326
    mov di, ax                                ; 89 c7                       ; 0xc2125 vgabios.c:1331
    mov dl, byte [di+046b4h]                  ; 8a 95 b4 46                 ; 0xc2127
    xor dh, dh                                ; 30 f6                       ; 0xc212b
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc212d
    mov di, dx                                ; 89 d7                       ; 0xc212f
    sal di, CL                                ; d3 e7                       ; 0xc2131
    mov al, byte [di+046cah]                  ; 8a 85 ca 46                 ; 0xc2133
    mov byte [bp-012h], al                    ; 88 46 ee                    ; 0xc2137
    mov al, byte [bx+04637h]                  ; 8a 87 37 46                 ; 0xc213a vgabios.c:1332
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc213e
    dec si                                    ; 4e                          ; 0xc2141 vgabios.c:1333
    cmp si, strict byte 0ffffh                ; 83 fe ff                    ; 0xc2142
    je short 0219ch                           ; 74 55                       ; 0xc2145
    mov dl, byte [bp-00eh]                    ; 8a 56 f2                    ; 0xc2147
    xor dh, dh                                ; 30 f6                       ; 0xc214a
    cmp dx, word [bp-016h]                    ; 3b 56 ea                    ; 0xc214c
    jnc short 0219ch                          ; 73 4b                       ; 0xc214f
    mov dl, byte [bp-00ch]                    ; 8a 56 f4                    ; 0xc2151 vgabios.c:1335
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc2154
    mov bx, dx                                ; 89 d3                       ; 0xc2156
    sal bx, CL                                ; d3 e3                       ; 0xc2158
    mov bl, byte [bx+04636h]                  ; 8a 9f 36 46                 ; 0xc215a
    cmp bl, cl                                ; 38 cb                       ; 0xc215e
    jc short 02170h                           ; 72 0e                       ; 0xc2160
    jbe short 02177h                          ; 76 13                       ; 0xc2162
    cmp bl, 005h                              ; 80 fb 05                    ; 0xc2164
    je short 021c3h                           ; 74 5a                       ; 0xc2167
    cmp bl, 004h                              ; 80 fb 04                    ; 0xc2169
    je short 02177h                           ; 74 09                       ; 0xc216c
    jmp short 021e4h                          ; eb 74                       ; 0xc216e
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc2170
    je short 0219eh                           ; 74 29                       ; 0xc2173
    jmp short 021e4h                          ; eb 6d                       ; 0xc2175
    mov dl, byte [bp-012h]                    ; 8a 56 ee                    ; 0xc2177 vgabios.c:1339
    xor dh, dh                                ; 30 f6                       ; 0xc217a
    push dx                                   ; 52                          ; 0xc217c
    mov bl, byte [bp-016h]                    ; 8a 5e ea                    ; 0xc217d
    xor bh, bh                                ; 30 ff                       ; 0xc2180
    push bx                                   ; 53                          ; 0xc2182
    mov bl, byte [bp-014h]                    ; 8a 5e ec                    ; 0xc2183
    mov cx, bx                                ; 89 d9                       ; 0xc2186
    mov bl, byte [bp-00eh]                    ; 8a 5e f2                    ; 0xc2188
    mov dl, byte [bp-00ah]                    ; 8a 56 f6                    ; 0xc218b
    mov di, dx                                ; 89 d7                       ; 0xc218e
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc2190
    mov ax, dx                                ; 89 d0                       ; 0xc2193
    mov dx, di                                ; 89 fa                       ; 0xc2195
    call 01beeh                               ; e8 54 fa                    ; 0xc2197
    jmp short 021e4h                          ; eb 48                       ; 0xc219a vgabios.c:1340
    jmp short 021eah                          ; eb 4c                       ; 0xc219c
    mov bl, byte [bp-010h]                    ; 8a 5e f0                    ; 0xc219e vgabios.c:1342
    xor bh, bh                                ; 30 ff                       ; 0xc21a1
    push bx                                   ; 53                          ; 0xc21a3
    mov bl, byte [bp-016h]                    ; 8a 5e ea                    ; 0xc21a4
    push bx                                   ; 53                          ; 0xc21a7
    mov bl, byte [bp-014h]                    ; 8a 5e ec                    ; 0xc21a8
    mov cx, bx                                ; 89 d9                       ; 0xc21ab
    mov dl, byte [bp-00eh]                    ; 8a 56 f2                    ; 0xc21ad
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc21b0
    mov di, bx                                ; 89 df                       ; 0xc21b3
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc21b5
    xor ah, ah                                ; 30 e4                       ; 0xc21b8
    mov bx, dx                                ; 89 d3                       ; 0xc21ba
    mov dx, di                                ; 89 fa                       ; 0xc21bc
    call 01ce6h                               ; e8 25 fb                    ; 0xc21be
    jmp short 021e4h                          ; eb 21                       ; 0xc21c1 vgabios.c:1343
    mov bl, byte [bp-016h]                    ; 8a 5e ea                    ; 0xc21c3 vgabios.c:1345
    xor bh, bh                                ; 30 ff                       ; 0xc21c6
    push bx                                   ; 53                          ; 0xc21c8
    mov bl, byte [bp-014h]                    ; 8a 5e ec                    ; 0xc21c9
    mov cx, bx                                ; 89 d9                       ; 0xc21cc
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc21ce
    xor ah, ah                                ; 30 e4                       ; 0xc21d1
    mov di, ax                                ; 89 c7                       ; 0xc21d3
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc21d5
    mov dx, bx                                ; 89 da                       ; 0xc21d8
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc21da
    mov ax, bx                                ; 89 d8                       ; 0xc21dd
    mov bx, di                                ; 89 fb                       ; 0xc21df
    call 01e20h                               ; e8 3c fc                    ; 0xc21e1
    inc byte [bp-00eh]                        ; fe 46 f2                    ; 0xc21e4 vgabios.c:1352
    jmp near 02141h                           ; e9 57 ff                    ; 0xc21e7 vgabios.c:1353
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc21ea vgabios.c:1355
    pop di                                    ; 5f                          ; 0xc21ed
    pop si                                    ; 5e                          ; 0xc21ee
    pop bp                                    ; 5d                          ; 0xc21ef
    retn                                      ; c3                          ; 0xc21f0
  ; disGetNextSymbol 0xc21f1 LB 0x1aee -> off=0x0 cb=000000000000017f uValue=00000000000c21f1 'biosfn_write_pixel'
biosfn_write_pixel:                          ; 0xc21f1 LB 0x17f
    push bp                                   ; 55                          ; 0xc21f1 vgabios.c:1358
    mov bp, sp                                ; 89 e5                       ; 0xc21f2
    push si                                   ; 56                          ; 0xc21f4
    sub sp, strict byte 00008h                ; 83 ec 08                    ; 0xc21f5
    mov byte [bp-004h], dl                    ; 88 56 fc                    ; 0xc21f8
    mov word [bp-008h], bx                    ; 89 5e f8                    ; 0xc21fb
    mov word [bp-00ah], cx                    ; 89 4e f6                    ; 0xc21fe
    mov dx, strict word 00049h                ; ba 49 00                    ; 0xc2201 vgabios.c:1364
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2204
    call 031beh                               ; e8 b4 0f                    ; 0xc2207
    xor ah, ah                                ; 30 e4                       ; 0xc220a vgabios.c:1365
    call 03193h                               ; e8 84 0f                    ; 0xc220c
    mov ch, al                                ; 88 c5                       ; 0xc220f
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc2211 vgabios.c:1366
    je short 0223ch                           ; 74 27                       ; 0xc2213
    xor ah, ah                                ; 30 e4                       ; 0xc2215 vgabios.c:1367
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc2217
    mov bx, ax                                ; 89 c3                       ; 0xc2219
    sal bx, CL                                ; d3 e3                       ; 0xc221b
    cmp byte [bx+04635h], 000h                ; 80 bf 35 46 00              ; 0xc221d
    je short 0223ch                           ; 74 18                       ; 0xc2222
    mov al, byte [bx+04636h]                  ; 8a 87 36 46                 ; 0xc2224 vgabios.c:1369
    cmp al, cl                                ; 38 c8                       ; 0xc2228
    jc short 02238h                           ; 72 0c                       ; 0xc222a
    jbe short 02242h                          ; 76 14                       ; 0xc222c
    cmp AL, strict byte 005h                  ; 3c 05                       ; 0xc222e
    je short 0223fh                           ; 74 0d                       ; 0xc2230
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc2232
    je short 02242h                           ; 74 0c                       ; 0xc2234
    jmp short 0223ch                          ; eb 04                       ; 0xc2236
    cmp AL, strict byte 002h                  ; 3c 02                       ; 0xc2238
    je short 022adh                           ; 74 71                       ; 0xc223a
    jmp near 02341h                           ; e9 02 01                    ; 0xc223c
    jmp near 02347h                           ; e9 05 01                    ; 0xc223f
    mov dx, strict word 0004ah                ; ba 4a 00                    ; 0xc2242 vgabios.c:1373
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2245
    call 031dah                               ; e8 8f 0f                    ; 0xc2248
    mov bx, ax                                ; 89 c3                       ; 0xc224b
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc224d
    mul bx                                    ; f7 e3                       ; 0xc2250
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc2252
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc2254
    shr bx, CL                                ; d3 eb                       ; 0xc2257
    add bx, ax                                ; 01 c3                       ; 0xc2259
    mov word [bp-006h], bx                    ; 89 5e fa                    ; 0xc225b
    mov cx, word [bp-008h]                    ; 8b 4e f8                    ; 0xc225e vgabios.c:1374
    and cl, 007h                              ; 80 e1 07                    ; 0xc2261
    mov ax, 00080h                            ; b8 80 00                    ; 0xc2264
    sar ax, CL                                ; d3 f8                       ; 0xc2267
    mov ah, al                                ; 88 c4                       ; 0xc2269 vgabios.c:1375
    xor al, al                                ; 30 c0                       ; 0xc226b
    or AL, strict byte 008h                   ; 0c 08                       ; 0xc226d
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc226f
    out DX, ax                                ; ef                          ; 0xc2272
    mov ax, 00205h                            ; b8 05 02                    ; 0xc2273 vgabios.c:1376
    out DX, ax                                ; ef                          ; 0xc2276
    mov dx, bx                                ; 89 da                       ; 0xc2277 vgabios.c:1377
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2279
    call 031beh                               ; e8 3f 0f                    ; 0xc227c
    test byte [bp-004h], 080h                 ; f6 46 fc 80                 ; 0xc227f vgabios.c:1378
    je short 0228ch                           ; 74 07                       ; 0xc2283
    mov ax, 01803h                            ; b8 03 18                    ; 0xc2285 vgabios.c:1380
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2288
    out DX, ax                                ; ef                          ; 0xc228b
    mov al, byte [bp-004h]                    ; 8a 46 fc                    ; 0xc228c vgabios.c:1382
    xor ah, ah                                ; 30 e4                       ; 0xc228f
    mov bx, ax                                ; 89 c3                       ; 0xc2291
    mov dx, word [bp-006h]                    ; 8b 56 fa                    ; 0xc2293
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2296
    call 031cch                               ; e8 30 0f                    ; 0xc2299
    mov ax, 0ff08h                            ; b8 08 ff                    ; 0xc229c vgabios.c:1383
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc229f
    out DX, ax                                ; ef                          ; 0xc22a2
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc22a3 vgabios.c:1384
    out DX, ax                                ; ef                          ; 0xc22a6
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc22a7 vgabios.c:1385
    out DX, ax                                ; ef                          ; 0xc22aa
    jmp short 0223ch                          ; eb 8f                       ; 0xc22ab vgabios.c:1386
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc22ad vgabios.c:1388
    shr ax, 1                                 ; d1 e8                       ; 0xc22b0
    mov si, strict word 00050h                ; be 50 00                    ; 0xc22b2
    mul si                                    ; f7 e6                       ; 0xc22b5
    cmp byte [bx+04637h], 002h                ; 80 bf 37 46 02              ; 0xc22b7
    jne short 022c7h                          ; 75 09                       ; 0xc22bc
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc22be vgabios.c:1390
    shr bx, 1                                 ; d1 eb                       ; 0xc22c1
    shr bx, 1                                 ; d1 eb                       ; 0xc22c3
    jmp short 022cch                          ; eb 05                       ; 0xc22c5 vgabios.c:1392
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc22c7 vgabios.c:1394
    shr bx, CL                                ; d3 eb                       ; 0xc22ca
    add bx, ax                                ; 01 c3                       ; 0xc22cc
    mov word [bp-006h], bx                    ; 89 5e fa                    ; 0xc22ce
    test byte [bp-00ah], 001h                 ; f6 46 f6 01                 ; 0xc22d1 vgabios.c:1396
    je short 022dbh                           ; 74 04                       ; 0xc22d5
    add byte [bp-005h], 020h                  ; 80 46 fb 20                 ; 0xc22d7
    mov dx, word [bp-006h]                    ; 8b 56 fa                    ; 0xc22db vgabios.c:1397
    mov ax, 0b800h                            ; b8 00 b8                    ; 0xc22de
    call 031beh                               ; e8 da 0e                    ; 0xc22e1
    mov bl, al                                ; 88 c3                       ; 0xc22e4
    mov al, ch                                ; 88 e8                       ; 0xc22e6 vgabios.c:1398
    xor ah, ah                                ; 30 e4                       ; 0xc22e8
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc22ea
    mov si, ax                                ; 89 c6                       ; 0xc22ec
    sal si, CL                                ; d3 e6                       ; 0xc22ee
    cmp byte [si+04637h], 002h                ; 80 bc 37 46 02              ; 0xc22f0
    jne short 02311h                          ; 75 1a                       ; 0xc22f5
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc22f7 vgabios.c:1400
    and al, cl                                ; 20 c8                       ; 0xc22fa
    mov ah, cl                                ; 88 cc                       ; 0xc22fc
    sub ah, al                                ; 28 c4                       ; 0xc22fe
    mov al, ah                                ; 88 e0                       ; 0xc2300
    sal al, 1                                 ; d0 e0                       ; 0xc2302
    mov bh, byte [bp-004h]                    ; 8a 7e fc                    ; 0xc2304
    and bh, cl                                ; 20 cf                       ; 0xc2307
    mov cl, al                                ; 88 c1                       ; 0xc2309
    sal bh, CL                                ; d2 e7                       ; 0xc230b
    mov AL, strict byte 003h                  ; b0 03                       ; 0xc230d vgabios.c:1401
    jmp short 02324h                          ; eb 13                       ; 0xc230f vgabios.c:1403
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2311 vgabios.c:1405
    and AL, strict byte 007h                  ; 24 07                       ; 0xc2314
    mov CL, strict byte 007h                  ; b1 07                       ; 0xc2316
    sub cl, al                                ; 28 c1                       ; 0xc2318
    mov bh, byte [bp-004h]                    ; 8a 7e fc                    ; 0xc231a
    and bh, 001h                              ; 80 e7 01                    ; 0xc231d
    sal bh, CL                                ; d2 e7                       ; 0xc2320
    mov AL, strict byte 001h                  ; b0 01                       ; 0xc2322 vgabios.c:1406
    sal al, CL                                ; d2 e0                       ; 0xc2324
    test byte [bp-004h], 080h                 ; f6 46 fc 80                 ; 0xc2326 vgabios.c:1408
    je short 02330h                           ; 74 04                       ; 0xc232a
    xor bl, bh                                ; 30 fb                       ; 0xc232c vgabios.c:1410
    jmp short 02336h                          ; eb 06                       ; 0xc232e vgabios.c:1412
    not al                                    ; f6 d0                       ; 0xc2330 vgabios.c:1414
    and bl, al                                ; 20 c3                       ; 0xc2332
    or bl, bh                                 ; 08 fb                       ; 0xc2334 vgabios.c:1415
    xor bh, bh                                ; 30 ff                       ; 0xc2336 vgabios.c:1417
    mov dx, word [bp-006h]                    ; 8b 56 fa                    ; 0xc2338
    mov ax, 0b800h                            ; b8 00 b8                    ; 0xc233b
    call 031cch                               ; e8 8b 0e                    ; 0xc233e
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc2341 vgabios.c:1418
    pop si                                    ; 5e                          ; 0xc2344
    pop bp                                    ; 5d                          ; 0xc2345
    retn                                      ; c3                          ; 0xc2346
    mov dx, strict word 0004ah                ; ba 4a 00                    ; 0xc2347 vgabios.c:1420
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc234a
    call 031dah                               ; e8 8a 0e                    ; 0xc234d
    mov bx, ax                                ; 89 c3                       ; 0xc2350
    sal bx, CL                                ; d3 e3                       ; 0xc2352
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc2354
    mul bx                                    ; f7 e3                       ; 0xc2357
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc2359
    add bx, ax                                ; 01 c3                       ; 0xc235c
    mov word [bp-006h], bx                    ; 89 5e fa                    ; 0xc235e
    mov al, byte [bp-004h]                    ; 8a 46 fc                    ; 0xc2361 vgabios.c:1421
    xor ah, ah                                ; 30 e4                       ; 0xc2364
    mov bx, ax                                ; 89 c3                       ; 0xc2366
    mov dx, word [bp-006h]                    ; 8b 56 fa                    ; 0xc2368
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc236b
    jmp short 0233eh                          ; eb ce                       ; 0xc236e
  ; disGetNextSymbol 0xc2370 LB 0x196f -> off=0x0 cb=000000000000025f uValue=00000000000c2370 'biosfn_write_teletype'
biosfn_write_teletype:                       ; 0xc2370 LB 0x25f
    push bp                                   ; 55                          ; 0xc2370 vgabios.c:1431
    mov bp, sp                                ; 89 e5                       ; 0xc2371
    push si                                   ; 56                          ; 0xc2373
    push di                                   ; 57                          ; 0xc2374
    sub sp, strict byte 0001ah                ; 83 ec 1a                    ; 0xc2375
    mov byte [bp-016h], al                    ; 88 46 ea                    ; 0xc2378
    mov byte [bp-014h], dl                    ; 88 56 ec                    ; 0xc237b
    mov byte [bp-00eh], bl                    ; 88 5e f2                    ; 0xc237e
    mov byte [bp-00ch], cl                    ; 88 4e f4                    ; 0xc2381
    cmp dl, 0ffh                              ; 80 fa ff                    ; 0xc2384 vgabios.c:1439
    jne short 02395h                          ; 75 0c                       ; 0xc2387
    mov dx, strict word 00062h                ; ba 62 00                    ; 0xc2389 vgabios.c:1440
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc238c
    call 031beh                               ; e8 2c 0e                    ; 0xc238f
    mov byte [bp-014h], al                    ; 88 46 ec                    ; 0xc2392
    mov dx, strict word 00049h                ; ba 49 00                    ; 0xc2395 vgabios.c:1443
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2398
    call 031beh                               ; e8 20 0e                    ; 0xc239b
    mov bl, al                                ; 88 c3                       ; 0xc239e vgabios.c:1444
    xor bh, bh                                ; 30 ff                       ; 0xc23a0
    mov ax, bx                                ; 89 d8                       ; 0xc23a2
    call 03193h                               ; e8 ec 0d                    ; 0xc23a4
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc23a7
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc23aa vgabios.c:1445
    je short 02411h                           ; 74 63                       ; 0xc23ac
    mov bl, byte [bp-014h]                    ; 8a 5e ec                    ; 0xc23ae vgabios.c:1448
    mov ax, bx                                ; 89 d8                       ; 0xc23b1
    lea bx, [bp-01eh]                         ; 8d 5e e2                    ; 0xc23b3
    lea dx, [bp-01ch]                         ; 8d 56 e4                    ; 0xc23b6
    call 00a8ch                               ; e8 d0 e6                    ; 0xc23b9
    mov al, byte [bp-01eh]                    ; 8a 46 e2                    ; 0xc23bc vgabios.c:1449
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc23bf
    mov bx, word [bp-01eh]                    ; 8b 5e e2                    ; 0xc23c2
    mov byte [bp-008h], bh                    ; 88 7e f8                    ; 0xc23c5
    mov dx, 00084h                            ; ba 84 00                    ; 0xc23c8 vgabios.c:1452
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc23cb
    call 031beh                               ; e8 ed 0d                    ; 0xc23ce
    mov bl, al                                ; 88 c3                       ; 0xc23d1
    xor bh, bh                                ; 30 ff                       ; 0xc23d3
    inc bx                                    ; 43                          ; 0xc23d5
    mov word [bp-018h], bx                    ; 89 5e e8                    ; 0xc23d6
    mov dx, strict word 0004ah                ; ba 4a 00                    ; 0xc23d9 vgabios.c:1453
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc23dc
    call 031dah                               ; e8 f8 0d                    ; 0xc23df
    mov word [bp-01ah], ax                    ; 89 46 e6                    ; 0xc23e2
    mov al, byte [bp-016h]                    ; 8a 46 ea                    ; 0xc23e5 vgabios.c:1455
    cmp AL, strict byte 008h                  ; 3c 08                       ; 0xc23e8
    jc short 023f8h                           ; 72 0c                       ; 0xc23ea
    jbe short 023ffh                          ; 76 11                       ; 0xc23ec
    cmp AL, strict byte 00dh                  ; 3c 0d                       ; 0xc23ee
    je short 0240ah                           ; 74 18                       ; 0xc23f0
    cmp AL, strict byte 00ah                  ; 3c 0a                       ; 0xc23f2
    je short 02414h                           ; 74 1e                       ; 0xc23f4
    jmp short 02417h                          ; eb 1f                       ; 0xc23f6
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc23f8
    jne short 02417h                          ; 75 1b                       ; 0xc23fa
    jmp near 0250dh                           ; e9 0e 01                    ; 0xc23fc
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc23ff vgabios.c:1462
    jbe short 0240eh                          ; 76 09                       ; 0xc2403
    dec byte [bp-006h]                        ; fe 4e fa                    ; 0xc2405
    jmp short 0240eh                          ; eb 04                       ; 0xc2408 vgabios.c:1463
    mov byte [bp-006h], 000h                  ; c6 46 fa 00                 ; 0xc240a vgabios.c:1470
    jmp near 0250dh                           ; e9 fc 00                    ; 0xc240e vgabios.c:1471
    jmp near 025c8h                           ; e9 b4 01                    ; 0xc2411
    jmp near 0250ah                           ; e9 f3 00                    ; 0xc2414
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc2417 vgabios.c:1475
    xor bh, bh                                ; 30 ff                       ; 0xc241a
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc241c
    mov si, bx                                ; 89 de                       ; 0xc241e
    sal si, CL                                ; d3 e6                       ; 0xc2420
    cmp byte [si+04635h], 000h                ; 80 bc 35 46 00              ; 0xc2422
    jne short 0246fh                          ; 75 46                       ; 0xc2427
    mov ax, word [bp-01ah]                    ; 8b 46 e6                    ; 0xc2429 vgabios.c:1478
    mul word [bp-018h]                        ; f7 66 e8                    ; 0xc242c
    sal ax, 1                                 ; d1 e0                       ; 0xc242f
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc2431
    mov bl, byte [bp-014h]                    ; 8a 5e ec                    ; 0xc2433
    inc ax                                    ; 40                          ; 0xc2436
    mul bx                                    ; f7 e3                       ; 0xc2437
    mov cx, ax                                ; 89 c1                       ; 0xc2439
    mov bl, byte [bp-008h]                    ; 8a 5e f8                    ; 0xc243b
    mov ax, bx                                ; 89 d8                       ; 0xc243e
    mul word [bp-01ah]                        ; f7 66 e6                    ; 0xc2440
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc2443
    add ax, bx                                ; 01 d8                       ; 0xc2446
    sal ax, 1                                 ; d1 e0                       ; 0xc2448
    add cx, ax                                ; 01 c1                       ; 0xc244a
    mov bl, byte [bp-016h]                    ; 8a 5e ea                    ; 0xc244c vgabios.c:1481
    mov ax, word [si+04638h]                  ; 8b 84 38 46                 ; 0xc244f
    mov dx, cx                                ; 89 ca                       ; 0xc2453
    call 031cch                               ; e8 74 0d                    ; 0xc2455
    cmp byte [bp-00ch], 003h                  ; 80 7e f4 03                 ; 0xc2458 vgabios.c:1483
    jne short 024b5h                          ; 75 57                       ; 0xc245c
    mov bl, byte [bp-00eh]                    ; 8a 5e f2                    ; 0xc245e vgabios.c:1484
    xor bh, bh                                ; 30 ff                       ; 0xc2461
    mov dx, cx                                ; 89 ca                       ; 0xc2463
    inc dx                                    ; 42                          ; 0xc2465
    mov ax, word [si+04638h]                  ; 8b 84 38 46                 ; 0xc2466
    call 031cch                               ; e8 5f 0d                    ; 0xc246a
    jmp short 024b5h                          ; eb 46                       ; 0xc246d vgabios.c:1486
    mov bl, byte [bx+046b4h]                  ; 8a 9f b4 46                 ; 0xc246f vgabios.c:1489
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc2473
    sal bx, CL                                ; d3 e3                       ; 0xc2475
    mov bl, byte [bx+046cah]                  ; 8a 9f ca 46                 ; 0xc2477
    mov ah, byte [si+04637h]                  ; 8a a4 37 46                 ; 0xc247b vgabios.c:1490
    mov al, byte [si+04636h]                  ; 8a 84 36 46                 ; 0xc247f vgabios.c:1491
    cmp AL, strict byte 003h                  ; 3c 03                       ; 0xc2483
    jc short 02493h                           ; 72 0c                       ; 0xc2485
    jbe short 02499h                          ; 76 10                       ; 0xc2487
    cmp AL, strict byte 005h                  ; 3c 05                       ; 0xc2489
    je short 024d7h                           ; 74 4a                       ; 0xc248b
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc248d
    je short 02499h                           ; 74 08                       ; 0xc248f
    jmp short 024fah                          ; eb 67                       ; 0xc2491
    cmp AL, strict byte 002h                  ; 3c 02                       ; 0xc2493
    je short 024b7h                           ; 74 20                       ; 0xc2495
    jmp short 024fah                          ; eb 61                       ; 0xc2497
    xor bh, bh                                ; 30 ff                       ; 0xc2499 vgabios.c:1495
    push bx                                   ; 53                          ; 0xc249b
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc249c
    xor ah, ah                                ; 30 e4                       ; 0xc249f
    push ax                                   ; 50                          ; 0xc24a1
    mov cl, byte [bp-008h]                    ; 8a 4e f8                    ; 0xc24a2
    xor ch, ch                                ; 30 ed                       ; 0xc24a5
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc24a7
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc24aa
    mov dx, ax                                ; 89 c2                       ; 0xc24ad
    mov al, byte [bp-016h]                    ; 8a 46 ea                    ; 0xc24af
    call 01beeh                               ; e8 39 f7                    ; 0xc24b2
    jmp short 024fah                          ; eb 43                       ; 0xc24b5 vgabios.c:1496
    mov al, ah                                ; 88 e0                       ; 0xc24b7 vgabios.c:1498
    xor ah, ah                                ; 30 e4                       ; 0xc24b9
    push ax                                   ; 50                          ; 0xc24bb
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc24bc
    push ax                                   ; 50                          ; 0xc24bf
    mov cl, byte [bp-008h]                    ; 8a 4e f8                    ; 0xc24c0
    xor ch, ch                                ; 30 ed                       ; 0xc24c3
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc24c5
    mov bx, ax                                ; 89 c3                       ; 0xc24c8
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc24ca
    mov dx, ax                                ; 89 c2                       ; 0xc24cd
    mov al, byte [bp-016h]                    ; 8a 46 ea                    ; 0xc24cf
    call 01ce6h                               ; e8 11 f8                    ; 0xc24d2
    jmp short 024fah                          ; eb 23                       ; 0xc24d5 vgabios.c:1499
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc24d7 vgabios.c:1501
    xor ah, ah                                ; 30 e4                       ; 0xc24da
    push ax                                   ; 50                          ; 0xc24dc
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc24dd
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc24e0
    xor bh, bh                                ; 30 ff                       ; 0xc24e3
    mov si, bx                                ; 89 de                       ; 0xc24e5
    mov bl, byte [bp-00eh]                    ; 8a 5e f2                    ; 0xc24e7
    mov dx, bx                                ; 89 da                       ; 0xc24ea
    mov bl, byte [bp-016h]                    ; 8a 5e ea                    ; 0xc24ec
    mov di, bx                                ; 89 df                       ; 0xc24ef
    mov cx, ax                                ; 89 c1                       ; 0xc24f1
    mov bx, si                                ; 89 f3                       ; 0xc24f3
    mov ax, di                                ; 89 f8                       ; 0xc24f5
    call 01e20h                               ; e8 26 f9                    ; 0xc24f7
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc24fa vgabios.c:1509
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc24fd vgabios.c:1511
    xor bh, bh                                ; 30 ff                       ; 0xc2500
    cmp bx, word [bp-01ah]                    ; 3b 5e e6                    ; 0xc2502
    jne short 0250dh                          ; 75 06                       ; 0xc2505
    mov byte [bp-006h], bh                    ; 88 7e fa                    ; 0xc2507 vgabios.c:1512
    inc byte [bp-008h]                        ; fe 46 f8                    ; 0xc250a vgabios.c:1513
    mov bl, byte [bp-008h]                    ; 8a 5e f8                    ; 0xc250d vgabios.c:1518
    xor bh, bh                                ; 30 ff                       ; 0xc2510
    mov ax, word [bp-018h]                    ; 8b 46 e8                    ; 0xc2512
    cmp bx, ax                                ; 39 c3                       ; 0xc2515
    jne short 0258ah                          ; 75 71                       ; 0xc2517
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc2519 vgabios.c:1520
    xor bh, ah                                ; 30 e7                       ; 0xc251c
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc251e
    mov si, bx                                ; 89 de                       ; 0xc2520
    sal si, CL                                ; d3 e6                       ; 0xc2522
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc2524
    db  0feh, 0c8h
    ; dec al                                    ; fe c8                     ; 0xc2527
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc2529
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc252c
    db  0feh, 0c8h
    ; dec al                                    ; fe c8                     ; 0xc252f
    mov byte [bp-012h], al                    ; 88 46 ee                    ; 0xc2531
    cmp byte [si+04635h], 000h                ; 80 bc 35 46 00              ; 0xc2534
    jne short 0258ch                          ; 75 51                       ; 0xc2539
    mov ax, word [bp-01ah]                    ; 8b 46 e6                    ; 0xc253b vgabios.c:1522
    mul word [bp-018h]                        ; f7 66 e8                    ; 0xc253e
    sal ax, 1                                 ; d1 e0                       ; 0xc2541
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc2543
    mov bl, byte [bp-014h]                    ; 8a 5e ec                    ; 0xc2545
    xor bh, bh                                ; 30 ff                       ; 0xc2548
    inc ax                                    ; 40                          ; 0xc254a
    mul bx                                    ; f7 e3                       ; 0xc254b
    mov cx, ax                                ; 89 c1                       ; 0xc254d
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc254f
    xor dh, dh                                ; 30 f6                       ; 0xc2552
    mov ax, dx                                ; 89 d0                       ; 0xc2554
    dec ax                                    ; 48                          ; 0xc2556
    mul word [bp-01ah]                        ; f7 66 e6                    ; 0xc2557
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc255a
    add ax, bx                                ; 01 d8                       ; 0xc255d
    sal ax, 1                                 ; d1 e0                       ; 0xc255f
    mov dx, cx                                ; 89 ca                       ; 0xc2561
    add dx, ax                                ; 01 c2                       ; 0xc2563
    inc dx                                    ; 42                          ; 0xc2565
    mov ax, word [si+04638h]                  ; 8b 84 38 46                 ; 0xc2566
    call 031beh                               ; e8 51 0c                    ; 0xc256a
    mov dx, strict word 00001h                ; ba 01 00                    ; 0xc256d vgabios.c:1524
    push dx                                   ; 52                          ; 0xc2570
    mov bl, byte [bp-014h]                    ; 8a 5e ec                    ; 0xc2571
    push bx                                   ; 53                          ; 0xc2574
    mov bl, byte [bp-012h]                    ; 8a 5e ee                    ; 0xc2575
    push bx                                   ; 53                          ; 0xc2578
    mov bl, byte [bp-010h]                    ; 8a 5e f0                    ; 0xc2579
    push bx                                   ; 53                          ; 0xc257c
    mov bl, al                                ; 88 c3                       ; 0xc257d
    mov dx, bx                                ; 89 da                       ; 0xc257f
    xor cx, cx                                ; 31 c9                       ; 0xc2581
    xor bl, al                                ; 30 c3                       ; 0xc2583
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc2585
    jmp short 025a4h                          ; eb 1a                       ; 0xc2588 vgabios.c:1526
    jmp short 025aah                          ; eb 1e                       ; 0xc258a
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc258c vgabios.c:1528
    push ax                                   ; 50                          ; 0xc258f
    mov bl, byte [bp-014h]                    ; 8a 5e ec                    ; 0xc2590
    xor bh, bh                                ; 30 ff                       ; 0xc2593
    push bx                                   ; 53                          ; 0xc2595
    mov bl, byte [bp-012h]                    ; 8a 5e ee                    ; 0xc2596
    push bx                                   ; 53                          ; 0xc2599
    mov bl, byte [bp-010h]                    ; 8a 5e f0                    ; 0xc259a
    push bx                                   ; 53                          ; 0xc259d
    xor cx, cx                                ; 31 c9                       ; 0xc259e
    xor bl, bl                                ; 30 db                       ; 0xc25a0
    xor dx, dx                                ; 31 d2                       ; 0xc25a2
    call 01678h                               ; e8 d1 f0                    ; 0xc25a4
    dec byte [bp-008h]                        ; fe 4e f8                    ; 0xc25a7 vgabios.c:1530
    mov bl, byte [bp-008h]                    ; 8a 5e f8                    ; 0xc25aa vgabios.c:1534
    xor bh, bh                                ; 30 ff                       ; 0xc25ad
    mov word [bp-01eh], bx                    ; 89 5e e2                    ; 0xc25af
    mov CL, strict byte 008h                  ; b1 08                       ; 0xc25b2
    sal word [bp-01eh], CL                    ; d3 66 e2                    ; 0xc25b4
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc25b7
    add word [bp-01eh], bx                    ; 01 5e e2                    ; 0xc25ba
    mov dx, word [bp-01eh]                    ; 8b 56 e2                    ; 0xc25bd vgabios.c:1535
    mov bl, byte [bp-014h]                    ; 8a 5e ec                    ; 0xc25c0
    mov ax, bx                                ; 89 d8                       ; 0xc25c3
    call 00e91h                               ; e8 c9 e8                    ; 0xc25c5
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc25c8 vgabios.c:1536
    pop di                                    ; 5f                          ; 0xc25cb
    pop si                                    ; 5e                          ; 0xc25cc
    pop bp                                    ; 5d                          ; 0xc25cd
    retn                                      ; c3                          ; 0xc25ce
  ; disGetNextSymbol 0xc25cf LB 0x1710 -> off=0x0 cb=000000000000002c uValue=00000000000c25cf 'get_font_access'
get_font_access:                             ; 0xc25cf LB 0x2c
    push bp                                   ; 55                          ; 0xc25cf vgabios.c:1539
    mov bp, sp                                ; 89 e5                       ; 0xc25d0
    push dx                                   ; 52                          ; 0xc25d2
    mov ax, 00100h                            ; b8 00 01                    ; 0xc25d3 vgabios.c:1541
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc25d6
    out DX, ax                                ; ef                          ; 0xc25d9
    mov ax, 00402h                            ; b8 02 04                    ; 0xc25da vgabios.c:1542
    out DX, ax                                ; ef                          ; 0xc25dd
    mov ax, 00704h                            ; b8 04 07                    ; 0xc25de vgabios.c:1543
    out DX, ax                                ; ef                          ; 0xc25e1
    mov ax, 00300h                            ; b8 00 03                    ; 0xc25e2 vgabios.c:1544
    out DX, ax                                ; ef                          ; 0xc25e5
    mov ax, 00204h                            ; b8 04 02                    ; 0xc25e6 vgabios.c:1545
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc25e9
    out DX, ax                                ; ef                          ; 0xc25ec
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc25ed vgabios.c:1546
    out DX, ax                                ; ef                          ; 0xc25f0
    mov ax, 00406h                            ; b8 06 04                    ; 0xc25f1 vgabios.c:1547
    out DX, ax                                ; ef                          ; 0xc25f4
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc25f5 vgabios.c:1548
    pop dx                                    ; 5a                          ; 0xc25f8
    pop bp                                    ; 5d                          ; 0xc25f9
    retn                                      ; c3                          ; 0xc25fa
  ; disGetNextSymbol 0xc25fb LB 0x16e4 -> off=0x0 cb=000000000000003f uValue=00000000000c25fb 'release_font_access'
release_font_access:                         ; 0xc25fb LB 0x3f
    push bp                                   ; 55                          ; 0xc25fb vgabios.c:1550
    mov bp, sp                                ; 89 e5                       ; 0xc25fc
    push dx                                   ; 52                          ; 0xc25fe
    mov ax, 00100h                            ; b8 00 01                    ; 0xc25ff vgabios.c:1552
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc2602
    out DX, ax                                ; ef                          ; 0xc2605
    mov ax, 00302h                            ; b8 02 03                    ; 0xc2606 vgabios.c:1553
    out DX, ax                                ; ef                          ; 0xc2609
    mov ax, 00304h                            ; b8 04 03                    ; 0xc260a vgabios.c:1554
    out DX, ax                                ; ef                          ; 0xc260d
    mov ax, 00300h                            ; b8 00 03                    ; 0xc260e vgabios.c:1555
    out DX, ax                                ; ef                          ; 0xc2611
    mov dx, 003cch                            ; ba cc 03                    ; 0xc2612 vgabios.c:1556
    in AL, DX                                 ; ec                          ; 0xc2615
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2616
    and ax, strict word 00001h                ; 25 01 00                    ; 0xc2618
    sal ax, 1                                 ; d1 e0                       ; 0xc261b
    sal ax, 1                                 ; d1 e0                       ; 0xc261d
    mov ah, al                                ; 88 c4                       ; 0xc261f
    or ah, 00ah                               ; 80 cc 0a                    ; 0xc2621
    xor al, al                                ; 30 c0                       ; 0xc2624
    or AL, strict byte 006h                   ; 0c 06                       ; 0xc2626
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2628
    out DX, ax                                ; ef                          ; 0xc262b
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc262c vgabios.c:1557
    out DX, ax                                ; ef                          ; 0xc262f
    mov ax, 01005h                            ; b8 05 10                    ; 0xc2630 vgabios.c:1558
    out DX, ax                                ; ef                          ; 0xc2633
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc2634 vgabios.c:1559
    pop dx                                    ; 5a                          ; 0xc2637
    pop bp                                    ; 5d                          ; 0xc2638
    retn                                      ; c3                          ; 0xc2639
  ; disGetNextSymbol 0xc263a LB 0x16a5 -> off=0x0 cb=00000000000000c8 uValue=00000000000c263a 'set_scan_lines'
set_scan_lines:                              ; 0xc263a LB 0xc8
    push bp                                   ; 55                          ; 0xc263a vgabios.c:1561
    mov bp, sp                                ; 89 e5                       ; 0xc263b
    push bx                                   ; 53                          ; 0xc263d
    push cx                                   ; 51                          ; 0xc263e
    push dx                                   ; 52                          ; 0xc263f
    push si                                   ; 56                          ; 0xc2640
    push ax                                   ; 50                          ; 0xc2641
    mov bl, al                                ; 88 c3                       ; 0xc2642
    mov dx, strict word 00063h                ; ba 63 00                    ; 0xc2644 vgabios.c:1566
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2647
    call 031dah                               ; e8 8d 0b                    ; 0xc264a
    mov dx, ax                                ; 89 c2                       ; 0xc264d
    mov si, ax                                ; 89 c6                       ; 0xc264f
    mov AL, strict byte 009h                  ; b0 09                       ; 0xc2651 vgabios.c:1567
    out DX, AL                                ; ee                          ; 0xc2653
    inc dx                                    ; 42                          ; 0xc2654 vgabios.c:1568
    in AL, DX                                 ; ec                          ; 0xc2655
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2656
    mov ah, al                                ; 88 c4                       ; 0xc2658 vgabios.c:1569
    and ah, 0e0h                              ; 80 e4 e0                    ; 0xc265a
    mov al, bl                                ; 88 d8                       ; 0xc265d
    db  0feh, 0c8h
    ; dec al                                    ; fe c8                     ; 0xc265f
    or al, ah                                 ; 08 e0                       ; 0xc2661
    out DX, AL                                ; ee                          ; 0xc2663 vgabios.c:1570
    cmp bl, 008h                              ; 80 fb 08                    ; 0xc2664 vgabios.c:1571
    jne short 02671h                          ; 75 08                       ; 0xc2667
    mov dx, strict word 00007h                ; ba 07 00                    ; 0xc2669 vgabios.c:1573
    mov ax, strict word 00006h                ; b8 06 00                    ; 0xc266c
    jmp short 0267eh                          ; eb 0d                       ; 0xc266f vgabios.c:1575
    mov dl, bl                                ; 88 da                       ; 0xc2671 vgabios.c:1577
    sub dl, 003h                              ; 80 ea 03                    ; 0xc2673
    xor dh, dh                                ; 30 f6                       ; 0xc2676
    mov al, bl                                ; 88 d8                       ; 0xc2678
    sub AL, strict byte 004h                  ; 2c 04                       ; 0xc267a
    xor ah, ah                                ; 30 e4                       ; 0xc267c
    call 00ddeh                               ; e8 5d e7                    ; 0xc267e
    mov byte [bp-00ah], bl                    ; 88 5e f6                    ; 0xc2681 vgabios.c:1579
    mov byte [bp-009h], 000h                  ; c6 46 f7 00                 ; 0xc2684
    mov bx, word [bp-00ah]                    ; 8b 5e f6                    ; 0xc2688
    mov dx, 00085h                            ; ba 85 00                    ; 0xc268b
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc268e
    call 031e8h                               ; e8 54 0b                    ; 0xc2691
    mov AL, strict byte 012h                  ; b0 12                       ; 0xc2694 vgabios.c:1580
    mov dx, si                                ; 89 f2                       ; 0xc2696
    out DX, AL                                ; ee                          ; 0xc2698
    lea cx, [si+001h]                         ; 8d 4c 01                    ; 0xc2699 vgabios.c:1581
    mov dx, cx                                ; 89 ca                       ; 0xc269c
    in AL, DX                                 ; ec                          ; 0xc269e
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc269f
    mov bx, ax                                ; 89 c3                       ; 0xc26a1
    mov AL, strict byte 007h                  ; b0 07                       ; 0xc26a3 vgabios.c:1582
    mov dx, si                                ; 89 f2                       ; 0xc26a5
    out DX, AL                                ; ee                          ; 0xc26a7
    mov dx, cx                                ; 89 ca                       ; 0xc26a8 vgabios.c:1583
    in AL, DX                                 ; ec                          ; 0xc26aa
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc26ab
    mov dl, al                                ; 88 c2                       ; 0xc26ad vgabios.c:1584
    and dl, 002h                              ; 80 e2 02                    ; 0xc26af
    xor dh, ch                                ; 30 ee                       ; 0xc26b2
    mov CL, strict byte 007h                  ; b1 07                       ; 0xc26b4
    sal dx, CL                                ; d3 e2                       ; 0xc26b6
    and AL, strict byte 040h                  ; 24 40                       ; 0xc26b8
    xor ah, ah                                ; 30 e4                       ; 0xc26ba
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc26bc
    sal ax, CL                                ; d3 e0                       ; 0xc26be
    add ax, dx                                ; 01 d0                       ; 0xc26c0
    inc ax                                    ; 40                          ; 0xc26c2
    add ax, bx                                ; 01 d8                       ; 0xc26c3
    xor dx, dx                                ; 31 d2                       ; 0xc26c5 vgabios.c:1585
    div word [bp-00ah]                        ; f7 76 f6                    ; 0xc26c7
    mov cx, ax                                ; 89 c1                       ; 0xc26ca
    mov bl, al                                ; 88 c3                       ; 0xc26cc vgabios.c:1586
    db  0feh, 0cbh
    ; dec bl                                    ; fe cb                     ; 0xc26ce
    xor bh, bh                                ; 30 ff                       ; 0xc26d0
    mov dx, 00084h                            ; ba 84 00                    ; 0xc26d2
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc26d5
    call 031cch                               ; e8 f1 0a                    ; 0xc26d8
    mov dx, strict word 0004ah                ; ba 4a 00                    ; 0xc26db vgabios.c:1587
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc26de
    call 031dah                               ; e8 f6 0a                    ; 0xc26e1
    mov dx, ax                                ; 89 c2                       ; 0xc26e4
    mov al, cl                                ; 88 c8                       ; 0xc26e6 vgabios.c:1588
    xor ah, ah                                ; 30 e4                       ; 0xc26e8
    mul dx                                    ; f7 e2                       ; 0xc26ea
    mov bx, ax                                ; 89 c3                       ; 0xc26ec
    sal bx, 1                                 ; d1 e3                       ; 0xc26ee
    mov dx, strict word 0004ch                ; ba 4c 00                    ; 0xc26f0
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc26f3
    call 031e8h                               ; e8 ef 0a                    ; 0xc26f6
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc26f9 vgabios.c:1589
    pop si                                    ; 5e                          ; 0xc26fc
    pop dx                                    ; 5a                          ; 0xc26fd
    pop cx                                    ; 59                          ; 0xc26fe
    pop bx                                    ; 5b                          ; 0xc26ff
    pop bp                                    ; 5d                          ; 0xc2700
    retn                                      ; c3                          ; 0xc2701
  ; disGetNextSymbol 0xc2702 LB 0x15dd -> off=0x0 cb=0000000000000085 uValue=00000000000c2702 'biosfn_load_text_user_pat'
biosfn_load_text_user_pat:                   ; 0xc2702 LB 0x85
    push bp                                   ; 55                          ; 0xc2702 vgabios.c:1591
    mov bp, sp                                ; 89 e5                       ; 0xc2703
    push si                                   ; 56                          ; 0xc2705
    push di                                   ; 57                          ; 0xc2706
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc2707
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc270a
    mov word [bp-00eh], dx                    ; 89 56 f2                    ; 0xc270d
    mov word [bp-00ah], bx                    ; 89 5e f6                    ; 0xc2710
    mov word [bp-00ch], cx                    ; 89 4e f4                    ; 0xc2713
    call 025cfh                               ; e8 b6 fe                    ; 0xc2716 vgabios.c:1596
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc2719 vgabios.c:1597
    and AL, strict byte 003h                  ; 24 03                       ; 0xc271c
    xor ah, ah                                ; 30 e4                       ; 0xc271e
    mov CL, strict byte 00eh                  ; b1 0e                       ; 0xc2720
    mov bx, ax                                ; 89 c3                       ; 0xc2722
    sal bx, CL                                ; d3 e3                       ; 0xc2724
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc2726
    and AL, strict byte 004h                  ; 24 04                       ; 0xc2729
    mov CL, strict byte 00bh                  ; b1 0b                       ; 0xc272b
    sal ax, CL                                ; d3 e0                       ; 0xc272d
    add bx, ax                                ; 01 c3                       ; 0xc272f
    mov word [bp-008h], bx                    ; 89 5e f8                    ; 0xc2731
    xor bx, bx                                ; 31 db                       ; 0xc2734 vgabios.c:1598
    cmp bx, word [bp-00ch]                    ; 3b 5e f4                    ; 0xc2736
    jnc short 0276dh                          ; 73 32                       ; 0xc2739
    mov al, byte [bp+008h]                    ; 8a 46 08                    ; 0xc273b vgabios.c:1600
    xor ah, ah                                ; 30 e4                       ; 0xc273e
    mov si, ax                                ; 89 c6                       ; 0xc2740
    mov ax, bx                                ; 89 d8                       ; 0xc2742
    mul si                                    ; f7 e6                       ; 0xc2744
    add ax, word [bp-00ah]                    ; 03 46 f6                    ; 0xc2746
    mov di, word [bp+004h]                    ; 8b 7e 04                    ; 0xc2749 vgabios.c:1601
    add di, bx                                ; 01 df                       ; 0xc274c
    mov CL, strict byte 005h                  ; b1 05                       ; 0xc274e
    sal di, CL                                ; d3 e7                       ; 0xc2750
    add di, word [bp-008h]                    ; 03 7e f8                    ; 0xc2752
    mov cx, si                                ; 89 f1                       ; 0xc2755 vgabios.c:1602
    mov si, ax                                ; 89 c6                       ; 0xc2757
    mov dx, word [bp-00eh]                    ; 8b 56 f2                    ; 0xc2759
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc275c
    mov es, ax                                ; 8e c0                       ; 0xc275f
    cld                                       ; fc                          ; 0xc2761
    jcxz 0276ah                               ; e3 06                       ; 0xc2762
    push DS                                   ; 1e                          ; 0xc2764
    mov ds, dx                                ; 8e da                       ; 0xc2765
    rep movsb                                 ; f3 a4                       ; 0xc2767
    pop DS                                    ; 1f                          ; 0xc2769
    inc bx                                    ; 43                          ; 0xc276a vgabios.c:1603
    jmp short 02736h                          ; eb c9                       ; 0xc276b
    call 025fbh                               ; e8 8b fe                    ; 0xc276d vgabios.c:1604
    cmp byte [bp-006h], 010h                  ; 80 7e fa 10                 ; 0xc2770 vgabios.c:1605
    jc short 0277eh                           ; 72 08                       ; 0xc2774
    mov al, byte [bp+008h]                    ; 8a 46 08                    ; 0xc2776 vgabios.c:1607
    xor ah, ah                                ; 30 e4                       ; 0xc2779
    call 0263ah                               ; e8 bc fe                    ; 0xc277b
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc277e vgabios.c:1609
    pop di                                    ; 5f                          ; 0xc2781
    pop si                                    ; 5e                          ; 0xc2782
    pop bp                                    ; 5d                          ; 0xc2783
    retn 00006h                               ; c2 06 00                    ; 0xc2784
  ; disGetNextSymbol 0xc2787 LB 0x1558 -> off=0x0 cb=0000000000000076 uValue=00000000000c2787 'biosfn_load_text_8_14_pat'
biosfn_load_text_8_14_pat:                   ; 0xc2787 LB 0x76
    push bp                                   ; 55                          ; 0xc2787 vgabios.c:1611
    mov bp, sp                                ; 89 e5                       ; 0xc2788
    push bx                                   ; 53                          ; 0xc278a
    push cx                                   ; 51                          ; 0xc278b
    push si                                   ; 56                          ; 0xc278c
    push di                                   ; 57                          ; 0xc278d
    push ax                                   ; 50                          ; 0xc278e
    push ax                                   ; 50                          ; 0xc278f
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc2790
    call 025cfh                               ; e8 39 fe                    ; 0xc2793 vgabios.c:1615
    mov al, dl                                ; 88 d0                       ; 0xc2796 vgabios.c:1616
    and AL, strict byte 003h                  ; 24 03                       ; 0xc2798
    xor ah, ah                                ; 30 e4                       ; 0xc279a
    mov CL, strict byte 00eh                  ; b1 0e                       ; 0xc279c
    mov bx, ax                                ; 89 c3                       ; 0xc279e
    sal bx, CL                                ; d3 e3                       ; 0xc27a0
    mov al, dl                                ; 88 d0                       ; 0xc27a2
    and AL, strict byte 004h                  ; 24 04                       ; 0xc27a4
    mov CL, strict byte 00bh                  ; b1 0b                       ; 0xc27a6
    sal ax, CL                                ; d3 e0                       ; 0xc27a8
    add bx, ax                                ; 01 c3                       ; 0xc27aa
    mov word [bp-00ch], bx                    ; 89 5e f4                    ; 0xc27ac
    xor bx, bx                                ; 31 db                       ; 0xc27af vgabios.c:1617
    jmp short 027b9h                          ; eb 06                       ; 0xc27b1
    cmp bx, 00100h                            ; 81 fb 00 01                 ; 0xc27b3
    jnc short 027e5h                          ; 73 2c                       ; 0xc27b7
    mov ax, bx                                ; 89 d8                       ; 0xc27b9 vgabios.c:1619
    mov si, strict word 0000eh                ; be 0e 00                    ; 0xc27bb
    mul si                                    ; f7 e6                       ; 0xc27be
    mov CL, strict byte 005h                  ; b1 05                       ; 0xc27c0 vgabios.c:1620
    mov di, bx                                ; 89 df                       ; 0xc27c2
    sal di, CL                                ; d3 e7                       ; 0xc27c4
    add di, word [bp-00ch]                    ; 03 7e f4                    ; 0xc27c6
    mov si, 05bf2h                            ; be f2 5b                    ; 0xc27c9 vgabios.c:1621
    add si, ax                                ; 01 c6                       ; 0xc27cc
    mov cx, strict word 0000eh                ; b9 0e 00                    ; 0xc27ce
    mov dx, 0c000h                            ; ba 00 c0                    ; 0xc27d1
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc27d4
    mov es, ax                                ; 8e c0                       ; 0xc27d7
    cld                                       ; fc                          ; 0xc27d9
    jcxz 027e2h                               ; e3 06                       ; 0xc27da
    push DS                                   ; 1e                          ; 0xc27dc
    mov ds, dx                                ; 8e da                       ; 0xc27dd
    rep movsb                                 ; f3 a4                       ; 0xc27df
    pop DS                                    ; 1f                          ; 0xc27e1
    inc bx                                    ; 43                          ; 0xc27e2 vgabios.c:1622
    jmp short 027b3h                          ; eb ce                       ; 0xc27e3
    call 025fbh                               ; e8 13 fe                    ; 0xc27e5 vgabios.c:1623
    cmp byte [bp-00ah], 010h                  ; 80 7e f6 10                 ; 0xc27e8 vgabios.c:1624
    jc short 027f4h                           ; 72 06                       ; 0xc27ec
    mov ax, strict word 0000eh                ; b8 0e 00                    ; 0xc27ee vgabios.c:1626
    call 0263ah                               ; e8 46 fe                    ; 0xc27f1
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc27f4 vgabios.c:1628
    pop di                                    ; 5f                          ; 0xc27f7
    pop si                                    ; 5e                          ; 0xc27f8
    pop cx                                    ; 59                          ; 0xc27f9
    pop bx                                    ; 5b                          ; 0xc27fa
    pop bp                                    ; 5d                          ; 0xc27fb
    retn                                      ; c3                          ; 0xc27fc
  ; disGetNextSymbol 0xc27fd LB 0x14e2 -> off=0x0 cb=0000000000000074 uValue=00000000000c27fd 'biosfn_load_text_8_8_pat'
biosfn_load_text_8_8_pat:                    ; 0xc27fd LB 0x74
    push bp                                   ; 55                          ; 0xc27fd vgabios.c:1630
    mov bp, sp                                ; 89 e5                       ; 0xc27fe
    push bx                                   ; 53                          ; 0xc2800
    push cx                                   ; 51                          ; 0xc2801
    push si                                   ; 56                          ; 0xc2802
    push di                                   ; 57                          ; 0xc2803
    push ax                                   ; 50                          ; 0xc2804
    push ax                                   ; 50                          ; 0xc2805
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc2806
    call 025cfh                               ; e8 c3 fd                    ; 0xc2809 vgabios.c:1634
    mov al, dl                                ; 88 d0                       ; 0xc280c vgabios.c:1635
    and AL, strict byte 003h                  ; 24 03                       ; 0xc280e
    xor ah, ah                                ; 30 e4                       ; 0xc2810
    mov CL, strict byte 00eh                  ; b1 0e                       ; 0xc2812
    mov bx, ax                                ; 89 c3                       ; 0xc2814
    sal bx, CL                                ; d3 e3                       ; 0xc2816
    mov al, dl                                ; 88 d0                       ; 0xc2818
    and AL, strict byte 004h                  ; 24 04                       ; 0xc281a
    mov CL, strict byte 00bh                  ; b1 0b                       ; 0xc281c
    sal ax, CL                                ; d3 e0                       ; 0xc281e
    add bx, ax                                ; 01 c3                       ; 0xc2820
    mov word [bp-00ch], bx                    ; 89 5e f4                    ; 0xc2822
    xor bx, bx                                ; 31 db                       ; 0xc2825 vgabios.c:1636
    jmp short 0282fh                          ; eb 06                       ; 0xc2827
    cmp bx, 00100h                            ; 81 fb 00 01                 ; 0xc2829
    jnc short 02859h                          ; 73 2a                       ; 0xc282d
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc282f vgabios.c:1638
    mov si, bx                                ; 89 de                       ; 0xc2831
    sal si, CL                                ; d3 e6                       ; 0xc2833
    mov CL, strict byte 005h                  ; b1 05                       ; 0xc2835 vgabios.c:1639
    mov di, bx                                ; 89 df                       ; 0xc2837
    sal di, CL                                ; d3 e7                       ; 0xc2839
    add di, word [bp-00ch]                    ; 03 7e f4                    ; 0xc283b
    add si, 053f2h                            ; 81 c6 f2 53                 ; 0xc283e vgabios.c:1640
    mov cx, strict word 00008h                ; b9 08 00                    ; 0xc2842
    mov dx, 0c000h                            ; ba 00 c0                    ; 0xc2845
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2848
    mov es, ax                                ; 8e c0                       ; 0xc284b
    cld                                       ; fc                          ; 0xc284d
    jcxz 02856h                               ; e3 06                       ; 0xc284e
    push DS                                   ; 1e                          ; 0xc2850
    mov ds, dx                                ; 8e da                       ; 0xc2851
    rep movsb                                 ; f3 a4                       ; 0xc2853
    pop DS                                    ; 1f                          ; 0xc2855
    inc bx                                    ; 43                          ; 0xc2856 vgabios.c:1641
    jmp short 02829h                          ; eb d0                       ; 0xc2857
    call 025fbh                               ; e8 9f fd                    ; 0xc2859 vgabios.c:1642
    cmp byte [bp-00ah], 010h                  ; 80 7e f6 10                 ; 0xc285c vgabios.c:1643
    jc short 02868h                           ; 72 06                       ; 0xc2860
    mov ax, strict word 00008h                ; b8 08 00                    ; 0xc2862 vgabios.c:1645
    call 0263ah                               ; e8 d2 fd                    ; 0xc2865
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc2868 vgabios.c:1647
    pop di                                    ; 5f                          ; 0xc286b
    pop si                                    ; 5e                          ; 0xc286c
    pop cx                                    ; 59                          ; 0xc286d
    pop bx                                    ; 5b                          ; 0xc286e
    pop bp                                    ; 5d                          ; 0xc286f
    retn                                      ; c3                          ; 0xc2870
  ; disGetNextSymbol 0xc2871 LB 0x146e -> off=0x0 cb=0000000000000074 uValue=00000000000c2871 'biosfn_load_text_8_16_pat'
biosfn_load_text_8_16_pat:                   ; 0xc2871 LB 0x74
    push bp                                   ; 55                          ; 0xc2871 vgabios.c:1650
    mov bp, sp                                ; 89 e5                       ; 0xc2872
    push bx                                   ; 53                          ; 0xc2874
    push cx                                   ; 51                          ; 0xc2875
    push si                                   ; 56                          ; 0xc2876
    push di                                   ; 57                          ; 0xc2877
    push ax                                   ; 50                          ; 0xc2878
    push ax                                   ; 50                          ; 0xc2879
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc287a
    call 025cfh                               ; e8 4f fd                    ; 0xc287d vgabios.c:1654
    mov al, dl                                ; 88 d0                       ; 0xc2880 vgabios.c:1655
    and AL, strict byte 003h                  ; 24 03                       ; 0xc2882
    xor ah, ah                                ; 30 e4                       ; 0xc2884
    mov CL, strict byte 00eh                  ; b1 0e                       ; 0xc2886
    mov bx, ax                                ; 89 c3                       ; 0xc2888
    sal bx, CL                                ; d3 e3                       ; 0xc288a
    mov al, dl                                ; 88 d0                       ; 0xc288c
    and AL, strict byte 004h                  ; 24 04                       ; 0xc288e
    mov CL, strict byte 00bh                  ; b1 0b                       ; 0xc2890
    sal ax, CL                                ; d3 e0                       ; 0xc2892
    add bx, ax                                ; 01 c3                       ; 0xc2894
    mov word [bp-00ch], bx                    ; 89 5e f4                    ; 0xc2896
    xor bx, bx                                ; 31 db                       ; 0xc2899 vgabios.c:1656
    jmp short 028a3h                          ; eb 06                       ; 0xc289b
    cmp bx, 00100h                            ; 81 fb 00 01                 ; 0xc289d
    jnc short 028cdh                          ; 73 2a                       ; 0xc28a1
    mov CL, strict byte 004h                  ; b1 04                       ; 0xc28a3 vgabios.c:1658
    mov si, bx                                ; 89 de                       ; 0xc28a5
    sal si, CL                                ; d3 e6                       ; 0xc28a7
    mov CL, strict byte 005h                  ; b1 05                       ; 0xc28a9 vgabios.c:1659
    mov di, bx                                ; 89 df                       ; 0xc28ab
    sal di, CL                                ; d3 e7                       ; 0xc28ad
    add di, word [bp-00ch]                    ; 03 7e f4                    ; 0xc28af
    add si, 069f2h                            ; 81 c6 f2 69                 ; 0xc28b2 vgabios.c:1660
    mov cx, strict word 00010h                ; b9 10 00                    ; 0xc28b6
    mov dx, 0c000h                            ; ba 00 c0                    ; 0xc28b9
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc28bc
    mov es, ax                                ; 8e c0                       ; 0xc28bf
    cld                                       ; fc                          ; 0xc28c1
    jcxz 028cah                               ; e3 06                       ; 0xc28c2
    push DS                                   ; 1e                          ; 0xc28c4
    mov ds, dx                                ; 8e da                       ; 0xc28c5
    rep movsb                                 ; f3 a4                       ; 0xc28c7
    pop DS                                    ; 1f                          ; 0xc28c9
    inc bx                                    ; 43                          ; 0xc28ca vgabios.c:1661
    jmp short 0289dh                          ; eb d0                       ; 0xc28cb
    call 025fbh                               ; e8 2b fd                    ; 0xc28cd vgabios.c:1662
    cmp byte [bp-00ah], 010h                  ; 80 7e f6 10                 ; 0xc28d0 vgabios.c:1663
    jc short 028dch                           ; 72 06                       ; 0xc28d4
    mov ax, strict word 00010h                ; b8 10 00                    ; 0xc28d6 vgabios.c:1665
    call 0263ah                               ; e8 5e fd                    ; 0xc28d9
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc28dc vgabios.c:1667
    pop di                                    ; 5f                          ; 0xc28df
    pop si                                    ; 5e                          ; 0xc28e0
    pop cx                                    ; 59                          ; 0xc28e1
    pop bx                                    ; 5b                          ; 0xc28e2
    pop bp                                    ; 5d                          ; 0xc28e3
    retn                                      ; c3                          ; 0xc28e4
  ; disGetNextSymbol 0xc28e5 LB 0x13fa -> off=0x0 cb=0000000000000005 uValue=00000000000c28e5 'biosfn_load_gfx_8_8_chars'
biosfn_load_gfx_8_8_chars:                   ; 0xc28e5 LB 0x5
    push bp                                   ; 55                          ; 0xc28e5 vgabios.c:1669
    mov bp, sp                                ; 89 e5                       ; 0xc28e6
    pop bp                                    ; 5d                          ; 0xc28e8 vgabios.c:1674
    retn                                      ; c3                          ; 0xc28e9
  ; disGetNextSymbol 0xc28ea LB 0x13f5 -> off=0x0 cb=0000000000000007 uValue=00000000000c28ea 'biosfn_load_gfx_user_chars'
biosfn_load_gfx_user_chars:                  ; 0xc28ea LB 0x7
    push bp                                   ; 55                          ; 0xc28ea vgabios.c:1675
    mov bp, sp                                ; 89 e5                       ; 0xc28eb
    pop bp                                    ; 5d                          ; 0xc28ed vgabios.c:1681
    retn 00002h                               ; c2 02 00                    ; 0xc28ee
  ; disGetNextSymbol 0xc28f1 LB 0x13ee -> off=0x0 cb=0000000000000005 uValue=00000000000c28f1 'biosfn_load_gfx_8_14_chars'
biosfn_load_gfx_8_14_chars:                  ; 0xc28f1 LB 0x5
    push bp                                   ; 55                          ; 0xc28f1 vgabios.c:1682
    mov bp, sp                                ; 89 e5                       ; 0xc28f2
    pop bp                                    ; 5d                          ; 0xc28f4 vgabios.c:1687
    retn                                      ; c3                          ; 0xc28f5
  ; disGetNextSymbol 0xc28f6 LB 0x13e9 -> off=0x0 cb=0000000000000005 uValue=00000000000c28f6 'biosfn_load_gfx_8_8_dd_chars'
biosfn_load_gfx_8_8_dd_chars:                ; 0xc28f6 LB 0x5
    push bp                                   ; 55                          ; 0xc28f6 vgabios.c:1688
    mov bp, sp                                ; 89 e5                       ; 0xc28f7
    pop bp                                    ; 5d                          ; 0xc28f9 vgabios.c:1693
    retn                                      ; c3                          ; 0xc28fa
  ; disGetNextSymbol 0xc28fb LB 0x13e4 -> off=0x0 cb=0000000000000005 uValue=00000000000c28fb 'biosfn_load_gfx_8_16_chars'
biosfn_load_gfx_8_16_chars:                  ; 0xc28fb LB 0x5
    push bp                                   ; 55                          ; 0xc28fb vgabios.c:1694
    mov bp, sp                                ; 89 e5                       ; 0xc28fc
    pop bp                                    ; 5d                          ; 0xc28fe vgabios.c:1699
    retn                                      ; c3                          ; 0xc28ff
  ; disGetNextSymbol 0xc2900 LB 0x13df -> off=0x0 cb=0000000000000005 uValue=00000000000c2900 'biosfn_alternate_prtsc'
biosfn_alternate_prtsc:                      ; 0xc2900 LB 0x5
    push bp                                   ; 55                          ; 0xc2900 vgabios.c:1701
    mov bp, sp                                ; 89 e5                       ; 0xc2901
    pop bp                                    ; 5d                          ; 0xc2903 vgabios.c:1706
    retn                                      ; c3                          ; 0xc2904
  ; disGetNextSymbol 0xc2905 LB 0x13da -> off=0x0 cb=0000000000000005 uValue=00000000000c2905 'biosfn_switch_video_interface'
biosfn_switch_video_interface:               ; 0xc2905 LB 0x5
    push bp                                   ; 55                          ; 0xc2905 vgabios.c:1709
    mov bp, sp                                ; 89 e5                       ; 0xc2906
    pop bp                                    ; 5d                          ; 0xc2908 vgabios.c:1714
    retn                                      ; c3                          ; 0xc2909
  ; disGetNextSymbol 0xc290a LB 0x13d5 -> off=0x0 cb=0000000000000005 uValue=00000000000c290a 'biosfn_enable_video_refresh_control'
biosfn_enable_video_refresh_control:         ; 0xc290a LB 0x5
    push bp                                   ; 55                          ; 0xc290a vgabios.c:1715
    mov bp, sp                                ; 89 e5                       ; 0xc290b
    pop bp                                    ; 5d                          ; 0xc290d vgabios.c:1720
    retn                                      ; c3                          ; 0xc290e
  ; disGetNextSymbol 0xc290f LB 0x13d0 -> off=0x0 cb=0000000000000096 uValue=00000000000c290f 'biosfn_write_string'
biosfn_write_string:                         ; 0xc290f LB 0x96
    push bp                                   ; 55                          ; 0xc290f vgabios.c:1723
    mov bp, sp                                ; 89 e5                       ; 0xc2910
    push si                                   ; 56                          ; 0xc2912
    push di                                   ; 57                          ; 0xc2913
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc2914
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc2917
    mov byte [bp-00ah], dl                    ; 88 56 f6                    ; 0xc291a
    mov byte [bp-006h], bl                    ; 88 5e fa                    ; 0xc291d
    mov si, cx                                ; 89 ce                       ; 0xc2920
    mov di, word [bp+00ah]                    ; 8b 7e 0a                    ; 0xc2922
    mov al, dl                                ; 88 d0                       ; 0xc2925 vgabios.c:1730
    xor ah, ah                                ; 30 e4                       ; 0xc2927
    lea bx, [bp-00eh]                         ; 8d 5e f2                    ; 0xc2929
    lea dx, [bp-00ch]                         ; 8d 56 f4                    ; 0xc292c
    call 00a8ch                               ; e8 5a e1                    ; 0xc292f
    cmp byte [bp+004h], 0ffh                  ; 80 7e 04 ff                 ; 0xc2932 vgabios.c:1733
    jne short 02944h                          ; 75 0c                       ; 0xc2936
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc2938 vgabios.c:1734
    mov byte [bp+006h], al                    ; 88 46 06                    ; 0xc293b
    mov ax, word [bp-00eh]                    ; 8b 46 f2                    ; 0xc293e vgabios.c:1735
    mov byte [bp+004h], ah                    ; 88 66 04                    ; 0xc2941
    mov dh, byte [bp+004h]                    ; 8a 76 04                    ; 0xc2944 vgabios.c:1738
    mov dl, byte [bp+006h]                    ; 8a 56 06                    ; 0xc2947
    xor ah, ah                                ; 30 e4                       ; 0xc294a
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc294c vgabios.c:1739
    call 00e91h                               ; e8 3f e5                    ; 0xc294f
    dec si                                    ; 4e                          ; 0xc2952 vgabios.c:1741
    cmp si, strict byte 0ffffh                ; 83 fe ff                    ; 0xc2953
    je short 0298bh                           ; 74 33                       ; 0xc2956
    mov dx, di                                ; 89 fa                       ; 0xc2958 vgabios.c:1743
    inc di                                    ; 47                          ; 0xc295a
    mov ax, word [bp+008h]                    ; 8b 46 08                    ; 0xc295b
    call 031beh                               ; e8 5d 08                    ; 0xc295e
    mov cl, al                                ; 88 c1                       ; 0xc2961
    test byte [bp-008h], 002h                 ; f6 46 f8 02                 ; 0xc2963 vgabios.c:1744
    je short 02975h                           ; 74 0c                       ; 0xc2967
    mov dx, di                                ; 89 fa                       ; 0xc2969 vgabios.c:1745
    inc di                                    ; 47                          ; 0xc296b
    mov ax, word [bp+008h]                    ; 8b 46 08                    ; 0xc296c
    call 031beh                               ; e8 4c 08                    ; 0xc296f
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc2972
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc2975 vgabios.c:1747
    xor ah, ah                                ; 30 e4                       ; 0xc2978
    mov bx, ax                                ; 89 c3                       ; 0xc297a
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc297c
    mov dx, ax                                ; 89 c2                       ; 0xc297f
    mov al, cl                                ; 88 c8                       ; 0xc2981
    mov cx, strict word 00003h                ; b9 03 00                    ; 0xc2983
    call 02370h                               ; e8 e7 f9                    ; 0xc2986
    jmp short 02952h                          ; eb c7                       ; 0xc2989 vgabios.c:1748
    test byte [bp-008h], 001h                 ; f6 46 f8 01                 ; 0xc298b vgabios.c:1751
    jne short 0299ch                          ; 75 0b                       ; 0xc298f
    mov dx, word [bp-00eh]                    ; 8b 56 f2                    ; 0xc2991 vgabios.c:1752
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2994
    xor ah, ah                                ; 30 e4                       ; 0xc2997
    call 00e91h                               ; e8 f5 e4                    ; 0xc2999
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc299c vgabios.c:1753
    pop di                                    ; 5f                          ; 0xc299f
    pop si                                    ; 5e                          ; 0xc29a0
    pop bp                                    ; 5d                          ; 0xc29a1
    retn 00008h                               ; c2 08 00                    ; 0xc29a2
  ; disGetNextSymbol 0xc29a5 LB 0x133a -> off=0x0 cb=0000000000000102 uValue=00000000000c29a5 'biosfn_read_state_info'
biosfn_read_state_info:                      ; 0xc29a5 LB 0x102
    push bp                                   ; 55                          ; 0xc29a5 vgabios.c:1756
    mov bp, sp                                ; 89 e5                       ; 0xc29a6
    push cx                                   ; 51                          ; 0xc29a8
    push si                                   ; 56                          ; 0xc29a9
    push di                                   ; 57                          ; 0xc29aa
    push dx                                   ; 52                          ; 0xc29ab
    push bx                                   ; 53                          ; 0xc29ac
    mov cx, ds                                ; 8c d9                       ; 0xc29ad vgabios.c:1759
    mov bx, 05388h                            ; bb 88 53                    ; 0xc29af
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc29b2
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc29b5
    call 03208h                               ; e8 4d 08                    ; 0xc29b8
    mov di, word [bp-00ah]                    ; 8b 7e f6                    ; 0xc29bb vgabios.c:1762
    add di, strict byte 00004h                ; 83 c7 04                    ; 0xc29be
    mov cx, strict word 0001eh                ; b9 1e 00                    ; 0xc29c1
    mov si, strict word 00049h                ; be 49 00                    ; 0xc29c4
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc29c7
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc29ca
    cld                                       ; fc                          ; 0xc29cd
    jcxz 029d6h                               ; e3 06                       ; 0xc29ce
    push DS                                   ; 1e                          ; 0xc29d0
    mov ds, dx                                ; 8e da                       ; 0xc29d1
    rep movsb                                 ; f3 a4                       ; 0xc29d3
    pop DS                                    ; 1f                          ; 0xc29d5
    mov di, word [bp-00ah]                    ; 8b 7e f6                    ; 0xc29d6 vgabios.c:1763
    add di, strict byte 00022h                ; 83 c7 22                    ; 0xc29d9
    mov cx, strict word 00003h                ; b9 03 00                    ; 0xc29dc
    mov si, 00084h                            ; be 84 00                    ; 0xc29df
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc29e2
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc29e5
    cld                                       ; fc                          ; 0xc29e8
    jcxz 029f1h                               ; e3 06                       ; 0xc29e9
    push DS                                   ; 1e                          ; 0xc29eb
    mov ds, dx                                ; 8e da                       ; 0xc29ec
    rep movsb                                 ; f3 a4                       ; 0xc29ee
    pop DS                                    ; 1f                          ; 0xc29f0
    mov dx, 0008ah                            ; ba 8a 00                    ; 0xc29f1 vgabios.c:1765
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc29f4
    call 031beh                               ; e8 c4 07                    ; 0xc29f7
    mov bl, al                                ; 88 c3                       ; 0xc29fa
    xor bh, bh                                ; 30 ff                       ; 0xc29fc
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc29fe
    add dx, strict byte 00025h                ; 83 c2 25                    ; 0xc2a01
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc2a04
    call 031cch                               ; e8 c2 07                    ; 0xc2a07
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc2a0a vgabios.c:1766
    add dx, strict byte 00026h                ; 83 c2 26                    ; 0xc2a0d
    xor bx, bx                                ; 31 db                       ; 0xc2a10
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc2a12
    call 031cch                               ; e8 b4 07                    ; 0xc2a15
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc2a18 vgabios.c:1767
    add dx, strict byte 00027h                ; 83 c2 27                    ; 0xc2a1b
    mov bx, strict word 00010h                ; bb 10 00                    ; 0xc2a1e
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc2a21
    call 031cch                               ; e8 a5 07                    ; 0xc2a24
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc2a27 vgabios.c:1768
    add dx, strict byte 00028h                ; 83 c2 28                    ; 0xc2a2a
    xor bx, bx                                ; 31 db                       ; 0xc2a2d
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc2a2f
    call 031cch                               ; e8 97 07                    ; 0xc2a32
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc2a35 vgabios.c:1769
    add dx, strict byte 00029h                ; 83 c2 29                    ; 0xc2a38
    mov bx, strict word 00008h                ; bb 08 00                    ; 0xc2a3b
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc2a3e
    call 031cch                               ; e8 88 07                    ; 0xc2a41
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc2a44 vgabios.c:1770
    add dx, strict byte 0002ah                ; 83 c2 2a                    ; 0xc2a47
    mov bx, strict word 00002h                ; bb 02 00                    ; 0xc2a4a
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc2a4d
    call 031cch                               ; e8 79 07                    ; 0xc2a50
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc2a53 vgabios.c:1771
    add dx, strict byte 0002bh                ; 83 c2 2b                    ; 0xc2a56
    xor bx, bx                                ; 31 db                       ; 0xc2a59
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc2a5b
    call 031cch                               ; e8 6b 07                    ; 0xc2a5e
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc2a61 vgabios.c:1772
    add dx, strict byte 0002ch                ; 83 c2 2c                    ; 0xc2a64
    xor bx, bx                                ; 31 db                       ; 0xc2a67
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc2a69
    call 031cch                               ; e8 5d 07                    ; 0xc2a6c
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc2a6f vgabios.c:1773
    add dx, strict byte 00031h                ; 83 c2 31                    ; 0xc2a72
    mov bx, strict word 00003h                ; bb 03 00                    ; 0xc2a75
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc2a78
    call 031cch                               ; e8 4e 07                    ; 0xc2a7b
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc2a7e vgabios.c:1774
    add dx, strict byte 00032h                ; 83 c2 32                    ; 0xc2a81
    xor bx, bx                                ; 31 db                       ; 0xc2a84
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc2a86
    call 031cch                               ; e8 40 07                    ; 0xc2a89
    mov di, word [bp-00ah]                    ; 8b 7e f6                    ; 0xc2a8c vgabios.c:1776
    add di, strict byte 00033h                ; 83 c7 33                    ; 0xc2a8f
    mov cx, strict word 0000dh                ; b9 0d 00                    ; 0xc2a92
    xor ax, ax                                ; 31 c0                       ; 0xc2a95
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc2a97
    cld                                       ; fc                          ; 0xc2a9a
    jcxz 02a9fh                               ; e3 02                       ; 0xc2a9b
    rep stosb                                 ; f3 aa                       ; 0xc2a9d
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc2a9f vgabios.c:1777
    pop di                                    ; 5f                          ; 0xc2aa2
    pop si                                    ; 5e                          ; 0xc2aa3
    pop cx                                    ; 59                          ; 0xc2aa4
    pop bp                                    ; 5d                          ; 0xc2aa5
    retn                                      ; c3                          ; 0xc2aa6
  ; disGetNextSymbol 0xc2aa7 LB 0x1238 -> off=0x0 cb=0000000000000023 uValue=00000000000c2aa7 'biosfn_read_video_state_size2'
biosfn_read_video_state_size2:               ; 0xc2aa7 LB 0x23
    push dx                                   ; 52                          ; 0xc2aa7 vgabios.c:1780
    push bp                                   ; 55                          ; 0xc2aa8
    mov bp, sp                                ; 89 e5                       ; 0xc2aa9
    mov dx, ax                                ; 89 c2                       ; 0xc2aab
    xor ax, ax                                ; 31 c0                       ; 0xc2aad vgabios.c:1784
    test dl, 001h                             ; f6 c2 01                    ; 0xc2aaf vgabios.c:1785
    je short 02ab7h                           ; 74 03                       ; 0xc2ab2
    mov ax, strict word 00046h                ; b8 46 00                    ; 0xc2ab4 vgabios.c:1786
    test dl, 002h                             ; f6 c2 02                    ; 0xc2ab7 vgabios.c:1788
    je short 02abfh                           ; 74 03                       ; 0xc2aba
    add ax, strict word 0002ah                ; 05 2a 00                    ; 0xc2abc vgabios.c:1789
    test dl, 004h                             ; f6 c2 04                    ; 0xc2abf vgabios.c:1791
    je short 02ac7h                           ; 74 03                       ; 0xc2ac2
    add ax, 00304h                            ; 05 04 03                    ; 0xc2ac4 vgabios.c:1792
    pop bp                                    ; 5d                          ; 0xc2ac7 vgabios.c:1796
    pop dx                                    ; 5a                          ; 0xc2ac8
    retn                                      ; c3                          ; 0xc2ac9
  ; disGetNextSymbol 0xc2aca LB 0x1215 -> off=0x0 cb=0000000000000012 uValue=00000000000c2aca 'vga_get_video_state_size'
vga_get_video_state_size:                    ; 0xc2aca LB 0x12
    push bp                                   ; 55                          ; 0xc2aca vgabios.c:1798
    mov bp, sp                                ; 89 e5                       ; 0xc2acb
    push bx                                   ; 53                          ; 0xc2acd
    mov bx, dx                                ; 89 d3                       ; 0xc2ace
    call 02aa7h                               ; e8 d4 ff                    ; 0xc2ad0 vgabios.c:1800
    mov word [ss:bx], ax                      ; 36 89 07                    ; 0xc2ad3
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc2ad6 vgabios.c:1801
    pop bx                                    ; 5b                          ; 0xc2ad9
    pop bp                                    ; 5d                          ; 0xc2ada
    retn                                      ; c3                          ; 0xc2adb
  ; disGetNextSymbol 0xc2adc LB 0x1203 -> off=0x0 cb=0000000000000381 uValue=00000000000c2adc 'biosfn_save_video_state'
biosfn_save_video_state:                     ; 0xc2adc LB 0x381
    push bp                                   ; 55                          ; 0xc2adc vgabios.c:1803
    mov bp, sp                                ; 89 e5                       ; 0xc2add
    push cx                                   ; 51                          ; 0xc2adf
    push si                                   ; 56                          ; 0xc2ae0
    push di                                   ; 57                          ; 0xc2ae1
    push ax                                   ; 50                          ; 0xc2ae2
    push ax                                   ; 50                          ; 0xc2ae3
    push ax                                   ; 50                          ; 0xc2ae4
    mov si, dx                                ; 89 d6                       ; 0xc2ae5
    mov cx, bx                                ; 89 d9                       ; 0xc2ae7
    mov dx, strict word 00063h                ; ba 63 00                    ; 0xc2ae9 vgabios.c:1807
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2aec
    call 031dah                               ; e8 e8 06                    ; 0xc2aef
    mov di, ax                                ; 89 c7                       ; 0xc2af2
    test byte [bp-00ch], 001h                 ; f6 46 f4 01                 ; 0xc2af4 vgabios.c:1808
    je short 02b68h                           ; 74 6e                       ; 0xc2af8
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc2afa vgabios.c:1809
    in AL, DX                                 ; ec                          ; 0xc2afd
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2afe
    xor ah, ah                                ; 30 e4                       ; 0xc2b00
    mov bx, ax                                ; 89 c3                       ; 0xc2b02
    mov dx, cx                                ; 89 ca                       ; 0xc2b04
    mov ax, si                                ; 89 f0                       ; 0xc2b06
    call 031cch                               ; e8 c1 06                    ; 0xc2b08
    inc cx                                    ; 41                          ; 0xc2b0b vgabios.c:1810
    mov dx, di                                ; 89 fa                       ; 0xc2b0c
    in AL, DX                                 ; ec                          ; 0xc2b0e
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2b0f
    xor ah, ah                                ; 30 e4                       ; 0xc2b11
    mov bx, ax                                ; 89 c3                       ; 0xc2b13
    mov dx, cx                                ; 89 ca                       ; 0xc2b15
    mov ax, si                                ; 89 f0                       ; 0xc2b17
    call 031cch                               ; e8 b0 06                    ; 0xc2b19
    inc cx                                    ; 41                          ; 0xc2b1c vgabios.c:1811
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2b1d
    in AL, DX                                 ; ec                          ; 0xc2b20
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2b21
    xor ah, ah                                ; 30 e4                       ; 0xc2b23
    mov bx, ax                                ; 89 c3                       ; 0xc2b25
    mov dx, cx                                ; 89 ca                       ; 0xc2b27
    mov ax, si                                ; 89 f0                       ; 0xc2b29
    call 031cch                               ; e8 9e 06                    ; 0xc2b2b
    inc cx                                    ; 41                          ; 0xc2b2e vgabios.c:1812
    mov dx, 003dah                            ; ba da 03                    ; 0xc2b2f
    in AL, DX                                 ; ec                          ; 0xc2b32
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2b33
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc2b35 vgabios.c:1813
    in AL, DX                                 ; ec                          ; 0xc2b38
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2b39
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc2b3b
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2b3e vgabios.c:1814
    xor ah, ah                                ; 30 e4                       ; 0xc2b41
    mov bx, ax                                ; 89 c3                       ; 0xc2b43
    mov dx, cx                                ; 89 ca                       ; 0xc2b45
    mov ax, si                                ; 89 f0                       ; 0xc2b47
    call 031cch                               ; e8 80 06                    ; 0xc2b49
    inc cx                                    ; 41                          ; 0xc2b4c vgabios.c:1815
    mov dx, 003cah                            ; ba ca 03                    ; 0xc2b4d
    in AL, DX                                 ; ec                          ; 0xc2b50
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2b51
    xor ah, ah                                ; 30 e4                       ; 0xc2b53
    mov bx, ax                                ; 89 c3                       ; 0xc2b55
    mov dx, cx                                ; 89 ca                       ; 0xc2b57
    mov ax, si                                ; 89 f0                       ; 0xc2b59
    call 031cch                               ; e8 6e 06                    ; 0xc2b5b
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc2b5e vgabios.c:1817
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc2b61
    add cx, ax                                ; 01 c1                       ; 0xc2b64
    jmp short 02b71h                          ; eb 09                       ; 0xc2b66
    jmp near 02c6ch                           ; e9 01 01                    ; 0xc2b68
    cmp word [bp-00ah], strict byte 00004h    ; 83 7e f6 04                 ; 0xc2b6b
    jnbe short 02b8fh                         ; 77 1e                       ; 0xc2b6f
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2b71 vgabios.c:1818
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc2b74
    out DX, AL                                ; ee                          ; 0xc2b77
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc2b78 vgabios.c:1819
    in AL, DX                                 ; ec                          ; 0xc2b7b
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2b7c
    xor ah, ah                                ; 30 e4                       ; 0xc2b7e
    mov bx, ax                                ; 89 c3                       ; 0xc2b80
    mov dx, cx                                ; 89 ca                       ; 0xc2b82
    mov ax, si                                ; 89 f0                       ; 0xc2b84
    call 031cch                               ; e8 43 06                    ; 0xc2b86
    inc cx                                    ; 41                          ; 0xc2b89
    inc word [bp-00ah]                        ; ff 46 f6                    ; 0xc2b8a vgabios.c:1820
    jmp short 02b6bh                          ; eb dc                       ; 0xc2b8d
    xor al, al                                ; 30 c0                       ; 0xc2b8f vgabios.c:1821
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc2b91
    out DX, AL                                ; ee                          ; 0xc2b94
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc2b95 vgabios.c:1822
    in AL, DX                                 ; ec                          ; 0xc2b98
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2b99
    xor ah, ah                                ; 30 e4                       ; 0xc2b9b
    mov bx, ax                                ; 89 c3                       ; 0xc2b9d
    mov dx, cx                                ; 89 ca                       ; 0xc2b9f
    mov ax, si                                ; 89 f0                       ; 0xc2ba1
    call 031cch                               ; e8 26 06                    ; 0xc2ba3
    mov word [bp-00ah], strict word 00000h    ; c7 46 f6 00 00              ; 0xc2ba6 vgabios.c:1824
    inc cx                                    ; 41                          ; 0xc2bab
    jmp short 02bb4h                          ; eb 06                       ; 0xc2bac
    cmp word [bp-00ah], strict byte 00018h    ; 83 7e f6 18                 ; 0xc2bae
    jnbe short 02bd1h                         ; 77 1d                       ; 0xc2bb2
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2bb4 vgabios.c:1825
    mov dx, di                                ; 89 fa                       ; 0xc2bb7
    out DX, AL                                ; ee                          ; 0xc2bb9
    lea dx, [di+001h]                         ; 8d 55 01                    ; 0xc2bba vgabios.c:1826
    in AL, DX                                 ; ec                          ; 0xc2bbd
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2bbe
    xor ah, ah                                ; 30 e4                       ; 0xc2bc0
    mov bx, ax                                ; 89 c3                       ; 0xc2bc2
    mov dx, cx                                ; 89 ca                       ; 0xc2bc4
    mov ax, si                                ; 89 f0                       ; 0xc2bc6
    call 031cch                               ; e8 01 06                    ; 0xc2bc8
    inc cx                                    ; 41                          ; 0xc2bcb
    inc word [bp-00ah]                        ; ff 46 f6                    ; 0xc2bcc vgabios.c:1827
    jmp short 02baeh                          ; eb dd                       ; 0xc2bcf
    mov word [bp-00ah], strict word 00000h    ; c7 46 f6 00 00              ; 0xc2bd1 vgabios.c:1829
    jmp short 02bdeh                          ; eb 06                       ; 0xc2bd6
    cmp word [bp-00ah], strict byte 00013h    ; 83 7e f6 13                 ; 0xc2bd8
    jnbe short 02c08h                         ; 77 2a                       ; 0xc2bdc
    mov dx, 003dah                            ; ba da 03                    ; 0xc2bde vgabios.c:1830
    in AL, DX                                 ; ec                          ; 0xc2be1
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2be2
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc2be4 vgabios.c:1831
    and ax, strict word 00020h                ; 25 20 00                    ; 0xc2be7
    or ax, word [bp-00ah]                     ; 0b 46 f6                    ; 0xc2bea
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc2bed
    out DX, AL                                ; ee                          ; 0xc2bf0
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc2bf1 vgabios.c:1832
    in AL, DX                                 ; ec                          ; 0xc2bf4
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2bf5
    xor ah, ah                                ; 30 e4                       ; 0xc2bf7
    mov bx, ax                                ; 89 c3                       ; 0xc2bf9
    mov dx, cx                                ; 89 ca                       ; 0xc2bfb
    mov ax, si                                ; 89 f0                       ; 0xc2bfd
    call 031cch                               ; e8 ca 05                    ; 0xc2bff
    inc cx                                    ; 41                          ; 0xc2c02
    inc word [bp-00ah]                        ; ff 46 f6                    ; 0xc2c03 vgabios.c:1833
    jmp short 02bd8h                          ; eb d0                       ; 0xc2c06
    mov dx, 003dah                            ; ba da 03                    ; 0xc2c08 vgabios.c:1834
    in AL, DX                                 ; ec                          ; 0xc2c0b
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2c0c
    mov word [bp-00ah], strict word 00000h    ; c7 46 f6 00 00              ; 0xc2c0e vgabios.c:1836
    jmp short 02c1bh                          ; eb 06                       ; 0xc2c13
    cmp word [bp-00ah], strict byte 00008h    ; 83 7e f6 08                 ; 0xc2c15
    jnbe short 02c39h                         ; 77 1e                       ; 0xc2c19
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2c1b vgabios.c:1837
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2c1e
    out DX, AL                                ; ee                          ; 0xc2c21
    mov dx, 003cfh                            ; ba cf 03                    ; 0xc2c22 vgabios.c:1838
    in AL, DX                                 ; ec                          ; 0xc2c25
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2c26
    xor ah, ah                                ; 30 e4                       ; 0xc2c28
    mov bx, ax                                ; 89 c3                       ; 0xc2c2a
    mov dx, cx                                ; 89 ca                       ; 0xc2c2c
    mov ax, si                                ; 89 f0                       ; 0xc2c2e
    call 031cch                               ; e8 99 05                    ; 0xc2c30
    inc cx                                    ; 41                          ; 0xc2c33
    inc word [bp-00ah]                        ; ff 46 f6                    ; 0xc2c34 vgabios.c:1839
    jmp short 02c15h                          ; eb dc                       ; 0xc2c37
    mov bx, di                                ; 89 fb                       ; 0xc2c39 vgabios.c:1841
    mov dx, cx                                ; 89 ca                       ; 0xc2c3b
    mov ax, si                                ; 89 f0                       ; 0xc2c3d
    call 031e8h                               ; e8 a6 05                    ; 0xc2c3f
    inc cx                                    ; 41                          ; 0xc2c42 vgabios.c:1844
    inc cx                                    ; 41                          ; 0xc2c43
    xor bx, bx                                ; 31 db                       ; 0xc2c44
    mov dx, cx                                ; 89 ca                       ; 0xc2c46
    mov ax, si                                ; 89 f0                       ; 0xc2c48
    call 031cch                               ; e8 7f 05                    ; 0xc2c4a
    inc cx                                    ; 41                          ; 0xc2c4d vgabios.c:1845
    xor bx, bx                                ; 31 db                       ; 0xc2c4e
    mov dx, cx                                ; 89 ca                       ; 0xc2c50
    mov ax, si                                ; 89 f0                       ; 0xc2c52
    call 031cch                               ; e8 75 05                    ; 0xc2c54
    inc cx                                    ; 41                          ; 0xc2c57 vgabios.c:1846
    xor bx, bx                                ; 31 db                       ; 0xc2c58
    mov dx, cx                                ; 89 ca                       ; 0xc2c5a
    mov ax, si                                ; 89 f0                       ; 0xc2c5c
    call 031cch                               ; e8 6b 05                    ; 0xc2c5e
    inc cx                                    ; 41                          ; 0xc2c61 vgabios.c:1847
    xor bx, bx                                ; 31 db                       ; 0xc2c62
    mov dx, cx                                ; 89 ca                       ; 0xc2c64
    mov ax, si                                ; 89 f0                       ; 0xc2c66
    call 031cch                               ; e8 61 05                    ; 0xc2c68
    inc cx                                    ; 41                          ; 0xc2c6b
    test byte [bp-00ch], 002h                 ; f6 46 f4 02                 ; 0xc2c6c vgabios.c:1849
    jne short 02c75h                          ; 75 03                       ; 0xc2c70
    jmp near 02de2h                           ; e9 6d 01                    ; 0xc2c72
    mov dx, strict word 00049h                ; ba 49 00                    ; 0xc2c75 vgabios.c:1850
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2c78
    call 031beh                               ; e8 40 05                    ; 0xc2c7b
    xor ah, ah                                ; 30 e4                       ; 0xc2c7e
    mov bx, ax                                ; 89 c3                       ; 0xc2c80
    mov dx, cx                                ; 89 ca                       ; 0xc2c82
    mov ax, si                                ; 89 f0                       ; 0xc2c84
    call 031cch                               ; e8 43 05                    ; 0xc2c86
    inc cx                                    ; 41                          ; 0xc2c89 vgabios.c:1851
    mov dx, strict word 0004ah                ; ba 4a 00                    ; 0xc2c8a
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2c8d
    call 031dah                               ; e8 47 05                    ; 0xc2c90
    mov bx, ax                                ; 89 c3                       ; 0xc2c93
    mov dx, cx                                ; 89 ca                       ; 0xc2c95
    mov ax, si                                ; 89 f0                       ; 0xc2c97
    call 031e8h                               ; e8 4c 05                    ; 0xc2c99
    inc cx                                    ; 41                          ; 0xc2c9c vgabios.c:1852
    inc cx                                    ; 41                          ; 0xc2c9d
    mov dx, strict word 0004ch                ; ba 4c 00                    ; 0xc2c9e
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2ca1
    call 031dah                               ; e8 33 05                    ; 0xc2ca4
    mov bx, ax                                ; 89 c3                       ; 0xc2ca7
    mov dx, cx                                ; 89 ca                       ; 0xc2ca9
    mov ax, si                                ; 89 f0                       ; 0xc2cab
    call 031e8h                               ; e8 38 05                    ; 0xc2cad
    inc cx                                    ; 41                          ; 0xc2cb0 vgabios.c:1853
    inc cx                                    ; 41                          ; 0xc2cb1
    mov dx, strict word 00063h                ; ba 63 00                    ; 0xc2cb2
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2cb5
    call 031dah                               ; e8 1f 05                    ; 0xc2cb8
    mov bx, ax                                ; 89 c3                       ; 0xc2cbb
    mov dx, cx                                ; 89 ca                       ; 0xc2cbd
    mov ax, si                                ; 89 f0                       ; 0xc2cbf
    call 031e8h                               ; e8 24 05                    ; 0xc2cc1
    inc cx                                    ; 41                          ; 0xc2cc4 vgabios.c:1854
    inc cx                                    ; 41                          ; 0xc2cc5
    mov dx, 00084h                            ; ba 84 00                    ; 0xc2cc6
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2cc9
    call 031beh                               ; e8 ef 04                    ; 0xc2ccc
    xor ah, ah                                ; 30 e4                       ; 0xc2ccf
    mov bx, ax                                ; 89 c3                       ; 0xc2cd1
    mov dx, cx                                ; 89 ca                       ; 0xc2cd3
    mov ax, si                                ; 89 f0                       ; 0xc2cd5
    call 031cch                               ; e8 f2 04                    ; 0xc2cd7
    inc cx                                    ; 41                          ; 0xc2cda vgabios.c:1855
    mov dx, 00085h                            ; ba 85 00                    ; 0xc2cdb
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2cde
    call 031dah                               ; e8 f6 04                    ; 0xc2ce1
    mov bx, ax                                ; 89 c3                       ; 0xc2ce4
    mov dx, cx                                ; 89 ca                       ; 0xc2ce6
    mov ax, si                                ; 89 f0                       ; 0xc2ce8
    call 031e8h                               ; e8 fb 04                    ; 0xc2cea
    inc cx                                    ; 41                          ; 0xc2ced vgabios.c:1856
    inc cx                                    ; 41                          ; 0xc2cee
    mov dx, 00087h                            ; ba 87 00                    ; 0xc2cef
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2cf2
    call 031beh                               ; e8 c6 04                    ; 0xc2cf5
    xor ah, ah                                ; 30 e4                       ; 0xc2cf8
    mov bx, ax                                ; 89 c3                       ; 0xc2cfa
    mov dx, cx                                ; 89 ca                       ; 0xc2cfc
    mov ax, si                                ; 89 f0                       ; 0xc2cfe
    call 031cch                               ; e8 c9 04                    ; 0xc2d00
    inc cx                                    ; 41                          ; 0xc2d03 vgabios.c:1857
    mov dx, 00088h                            ; ba 88 00                    ; 0xc2d04
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2d07
    call 031beh                               ; e8 b1 04                    ; 0xc2d0a
    mov bl, al                                ; 88 c3                       ; 0xc2d0d
    xor bh, bh                                ; 30 ff                       ; 0xc2d0f
    mov dx, cx                                ; 89 ca                       ; 0xc2d11
    mov ax, si                                ; 89 f0                       ; 0xc2d13
    call 031cch                               ; e8 b4 04                    ; 0xc2d15
    inc cx                                    ; 41                          ; 0xc2d18 vgabios.c:1858
    mov dx, 00089h                            ; ba 89 00                    ; 0xc2d19
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2d1c
    call 031beh                               ; e8 9c 04                    ; 0xc2d1f
    xor ah, ah                                ; 30 e4                       ; 0xc2d22
    mov bx, ax                                ; 89 c3                       ; 0xc2d24
    mov dx, cx                                ; 89 ca                       ; 0xc2d26
    mov ax, si                                ; 89 f0                       ; 0xc2d28
    call 031cch                               ; e8 9f 04                    ; 0xc2d2a
    inc cx                                    ; 41                          ; 0xc2d2d vgabios.c:1859
    mov dx, strict word 00060h                ; ba 60 00                    ; 0xc2d2e
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2d31
    call 031dah                               ; e8 a3 04                    ; 0xc2d34
    mov bx, ax                                ; 89 c3                       ; 0xc2d37
    mov dx, cx                                ; 89 ca                       ; 0xc2d39
    mov ax, si                                ; 89 f0                       ; 0xc2d3b
    call 031e8h                               ; e8 a8 04                    ; 0xc2d3d
    mov word [bp-00ah], strict word 00000h    ; c7 46 f6 00 00              ; 0xc2d40 vgabios.c:1860
    inc cx                                    ; 41                          ; 0xc2d45
    inc cx                                    ; 41                          ; 0xc2d46
    jmp short 02d4fh                          ; eb 06                       ; 0xc2d47
    cmp word [bp-00ah], strict byte 00008h    ; 83 7e f6 08                 ; 0xc2d49
    jnc short 02d6dh                          ; 73 1e                       ; 0xc2d4d
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc2d4f vgabios.c:1861
    sal dx, 1                                 ; d1 e2                       ; 0xc2d52
    add dx, strict byte 00050h                ; 83 c2 50                    ; 0xc2d54
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2d57
    call 031dah                               ; e8 7d 04                    ; 0xc2d5a
    mov bx, ax                                ; 89 c3                       ; 0xc2d5d
    mov dx, cx                                ; 89 ca                       ; 0xc2d5f
    mov ax, si                                ; 89 f0                       ; 0xc2d61
    call 031e8h                               ; e8 82 04                    ; 0xc2d63
    inc cx                                    ; 41                          ; 0xc2d66 vgabios.c:1862
    inc cx                                    ; 41                          ; 0xc2d67
    inc word [bp-00ah]                        ; ff 46 f6                    ; 0xc2d68 vgabios.c:1863
    jmp short 02d49h                          ; eb dc                       ; 0xc2d6b
    mov dx, strict word 0004eh                ; ba 4e 00                    ; 0xc2d6d vgabios.c:1864
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2d70
    call 031dah                               ; e8 64 04                    ; 0xc2d73
    mov bx, ax                                ; 89 c3                       ; 0xc2d76
    mov dx, cx                                ; 89 ca                       ; 0xc2d78
    mov ax, si                                ; 89 f0                       ; 0xc2d7a
    call 031e8h                               ; e8 69 04                    ; 0xc2d7c
    inc cx                                    ; 41                          ; 0xc2d7f vgabios.c:1865
    inc cx                                    ; 41                          ; 0xc2d80
    mov dx, strict word 00062h                ; ba 62 00                    ; 0xc2d81
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2d84
    call 031beh                               ; e8 34 04                    ; 0xc2d87
    xor ah, ah                                ; 30 e4                       ; 0xc2d8a
    mov bx, ax                                ; 89 c3                       ; 0xc2d8c
    mov dx, cx                                ; 89 ca                       ; 0xc2d8e
    mov ax, si                                ; 89 f0                       ; 0xc2d90
    call 031cch                               ; e8 37 04                    ; 0xc2d92
    inc cx                                    ; 41                          ; 0xc2d95 vgabios.c:1867
    mov dx, strict word 0007ch                ; ba 7c 00                    ; 0xc2d96
    xor ax, ax                                ; 31 c0                       ; 0xc2d99
    call 031dah                               ; e8 3c 04                    ; 0xc2d9b
    mov bx, ax                                ; 89 c3                       ; 0xc2d9e
    mov dx, cx                                ; 89 ca                       ; 0xc2da0
    mov ax, si                                ; 89 f0                       ; 0xc2da2
    call 031e8h                               ; e8 41 04                    ; 0xc2da4
    inc cx                                    ; 41                          ; 0xc2da7 vgabios.c:1868
    inc cx                                    ; 41                          ; 0xc2da8
    mov dx, strict word 0007eh                ; ba 7e 00                    ; 0xc2da9
    xor ax, ax                                ; 31 c0                       ; 0xc2dac
    call 031dah                               ; e8 29 04                    ; 0xc2dae
    mov bx, ax                                ; 89 c3                       ; 0xc2db1
    mov dx, cx                                ; 89 ca                       ; 0xc2db3
    mov ax, si                                ; 89 f0                       ; 0xc2db5
    call 031e8h                               ; e8 2e 04                    ; 0xc2db7
    inc cx                                    ; 41                          ; 0xc2dba vgabios.c:1869
    inc cx                                    ; 41                          ; 0xc2dbb
    mov dx, 0010ch                            ; ba 0c 01                    ; 0xc2dbc
    xor ax, ax                                ; 31 c0                       ; 0xc2dbf
    call 031dah                               ; e8 16 04                    ; 0xc2dc1
    mov bx, ax                                ; 89 c3                       ; 0xc2dc4
    mov dx, cx                                ; 89 ca                       ; 0xc2dc6
    mov ax, si                                ; 89 f0                       ; 0xc2dc8
    call 031e8h                               ; e8 1b 04                    ; 0xc2dca
    inc cx                                    ; 41                          ; 0xc2dcd vgabios.c:1870
    inc cx                                    ; 41                          ; 0xc2dce
    mov dx, 0010eh                            ; ba 0e 01                    ; 0xc2dcf
    xor ax, ax                                ; 31 c0                       ; 0xc2dd2
    call 031dah                               ; e8 03 04                    ; 0xc2dd4
    mov bx, ax                                ; 89 c3                       ; 0xc2dd7
    mov dx, cx                                ; 89 ca                       ; 0xc2dd9
    mov ax, si                                ; 89 f0                       ; 0xc2ddb
    call 031e8h                               ; e8 08 04                    ; 0xc2ddd
    inc cx                                    ; 41                          ; 0xc2de0
    inc cx                                    ; 41                          ; 0xc2de1
    test byte [bp-00ch], 004h                 ; f6 46 f4 04                 ; 0xc2de2 vgabios.c:1872
    je short 02e53h                           ; 74 6b                       ; 0xc2de6
    mov dx, 003c7h                            ; ba c7 03                    ; 0xc2de8 vgabios.c:1874
    in AL, DX                                 ; ec                          ; 0xc2deb
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2dec
    mov bl, al                                ; 88 c3                       ; 0xc2dee
    xor bh, bh                                ; 30 ff                       ; 0xc2df0
    mov dx, cx                                ; 89 ca                       ; 0xc2df2
    mov ax, si                                ; 89 f0                       ; 0xc2df4
    call 031cch                               ; e8 d3 03                    ; 0xc2df6
    inc cx                                    ; 41                          ; 0xc2df9 vgabios.c:1875
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc2dfa
    in AL, DX                                 ; ec                          ; 0xc2dfd
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2dfe
    mov bl, al                                ; 88 c3                       ; 0xc2e00
    xor bh, bh                                ; 30 ff                       ; 0xc2e02
    mov dx, cx                                ; 89 ca                       ; 0xc2e04
    mov ax, si                                ; 89 f0                       ; 0xc2e06
    call 031cch                               ; e8 c1 03                    ; 0xc2e08
    inc cx                                    ; 41                          ; 0xc2e0b vgabios.c:1876
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc2e0c
    in AL, DX                                 ; ec                          ; 0xc2e0f
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2e10
    xor ah, ah                                ; 30 e4                       ; 0xc2e12
    mov bx, ax                                ; 89 c3                       ; 0xc2e14
    mov dx, cx                                ; 89 ca                       ; 0xc2e16
    mov ax, si                                ; 89 f0                       ; 0xc2e18
    call 031cch                               ; e8 af 03                    ; 0xc2e1a
    inc cx                                    ; 41                          ; 0xc2e1d vgabios.c:1878
    xor al, al                                ; 30 c0                       ; 0xc2e1e
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc2e20
    out DX, AL                                ; ee                          ; 0xc2e23
    xor ah, ah                                ; 30 e4                       ; 0xc2e24 vgabios.c:1879
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc2e26
    jmp short 02e32h                          ; eb 07                       ; 0xc2e29
    cmp word [bp-00ah], 00300h                ; 81 7e f6 00 03              ; 0xc2e2b
    jnc short 02e49h                          ; 73 17                       ; 0xc2e30
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc2e32 vgabios.c:1880
    in AL, DX                                 ; ec                          ; 0xc2e35
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2e36
    mov bl, al                                ; 88 c3                       ; 0xc2e38
    xor bh, bh                                ; 30 ff                       ; 0xc2e3a
    mov dx, cx                                ; 89 ca                       ; 0xc2e3c
    mov ax, si                                ; 89 f0                       ; 0xc2e3e
    call 031cch                               ; e8 89 03                    ; 0xc2e40
    inc cx                                    ; 41                          ; 0xc2e43
    inc word [bp-00ah]                        ; ff 46 f6                    ; 0xc2e44 vgabios.c:1881
    jmp short 02e2bh                          ; eb e2                       ; 0xc2e47
    xor bx, bx                                ; 31 db                       ; 0xc2e49 vgabios.c:1882
    mov dx, cx                                ; 89 ca                       ; 0xc2e4b
    mov ax, si                                ; 89 f0                       ; 0xc2e4d
    call 031cch                               ; e8 7a 03                    ; 0xc2e4f
    inc cx                                    ; 41                          ; 0xc2e52
    mov ax, cx                                ; 89 c8                       ; 0xc2e53 vgabios.c:1885
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc2e55
    pop di                                    ; 5f                          ; 0xc2e58
    pop si                                    ; 5e                          ; 0xc2e59
    pop cx                                    ; 59                          ; 0xc2e5a
    pop bp                                    ; 5d                          ; 0xc2e5b
    retn                                      ; c3                          ; 0xc2e5c
  ; disGetNextSymbol 0xc2e5d LB 0xe82 -> off=0x0 cb=0000000000000336 uValue=00000000000c2e5d 'biosfn_restore_video_state'
biosfn_restore_video_state:                  ; 0xc2e5d LB 0x336
    push bp                                   ; 55                          ; 0xc2e5d vgabios.c:1887
    mov bp, sp                                ; 89 e5                       ; 0xc2e5e
    push cx                                   ; 51                          ; 0xc2e60
    push si                                   ; 56                          ; 0xc2e61
    push di                                   ; 57                          ; 0xc2e62
    sub sp, strict byte 00006h                ; 83 ec 06                    ; 0xc2e63
    push ax                                   ; 50                          ; 0xc2e66
    mov si, dx                                ; 89 d6                       ; 0xc2e67
    mov cx, bx                                ; 89 d9                       ; 0xc2e69
    test byte [bp-00eh], 001h                 ; f6 46 f2 01                 ; 0xc2e6b vgabios.c:1891
    je short 02ec8h                           ; 74 57                       ; 0xc2e6f
    mov dx, 003dah                            ; ba da 03                    ; 0xc2e71 vgabios.c:1893
    in AL, DX                                 ; ec                          ; 0xc2e74
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2e75
    lea dx, [bx+040h]                         ; 8d 57 40                    ; 0xc2e77 vgabios.c:1895
    mov ax, si                                ; 89 f0                       ; 0xc2e7a
    call 031dah                               ; e8 5b 03                    ; 0xc2e7c
    mov di, ax                                ; 89 c7                       ; 0xc2e7f
    mov word [bp-00ah], strict word 00001h    ; c7 46 f6 01 00              ; 0xc2e81 vgabios.c:1899
    lea cx, [bx+005h]                         ; 8d 4f 05                    ; 0xc2e86 vgabios.c:1897
    jmp short 02e91h                          ; eb 06                       ; 0xc2e89
    cmp word [bp-00ah], strict byte 00004h    ; 83 7e f6 04                 ; 0xc2e8b
    jnbe short 02ea9h                         ; 77 18                       ; 0xc2e8f
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2e91 vgabios.c:1900
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc2e94
    out DX, AL                                ; ee                          ; 0xc2e97
    mov dx, cx                                ; 89 ca                       ; 0xc2e98 vgabios.c:1901
    mov ax, si                                ; 89 f0                       ; 0xc2e9a
    call 031beh                               ; e8 1f 03                    ; 0xc2e9c
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc2e9f
    out DX, AL                                ; ee                          ; 0xc2ea2
    inc cx                                    ; 41                          ; 0xc2ea3
    inc word [bp-00ah]                        ; ff 46 f6                    ; 0xc2ea4 vgabios.c:1902
    jmp short 02e8bh                          ; eb e2                       ; 0xc2ea7
    xor al, al                                ; 30 c0                       ; 0xc2ea9 vgabios.c:1903
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc2eab
    out DX, AL                                ; ee                          ; 0xc2eae
    mov dx, cx                                ; 89 ca                       ; 0xc2eaf vgabios.c:1904
    mov ax, si                                ; 89 f0                       ; 0xc2eb1
    call 031beh                               ; e8 08 03                    ; 0xc2eb3
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc2eb6
    out DX, AL                                ; ee                          ; 0xc2eb9
    inc cx                                    ; 41                          ; 0xc2eba vgabios.c:1907
    mov ax, strict word 00011h                ; b8 11 00                    ; 0xc2ebb
    mov dx, di                                ; 89 fa                       ; 0xc2ebe
    out DX, ax                                ; ef                          ; 0xc2ec0
    mov word [bp-00ah], strict word 00000h    ; c7 46 f6 00 00              ; 0xc2ec1 vgabios.c:1909
    jmp short 02ed1h                          ; eb 09                       ; 0xc2ec6
    jmp near 02fbeh                           ; e9 f3 00                    ; 0xc2ec8
    cmp word [bp-00ah], strict byte 00018h    ; 83 7e f6 18                 ; 0xc2ecb
    jnbe short 02eeeh                         ; 77 1d                       ; 0xc2ecf
    cmp word [bp-00ah], strict byte 00011h    ; 83 7e f6 11                 ; 0xc2ed1 vgabios.c:1910
    je short 02ee8h                           ; 74 11                       ; 0xc2ed5
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2ed7 vgabios.c:1911
    mov dx, di                                ; 89 fa                       ; 0xc2eda
    out DX, AL                                ; ee                          ; 0xc2edc
    mov dx, cx                                ; 89 ca                       ; 0xc2edd vgabios.c:1912
    mov ax, si                                ; 89 f0                       ; 0xc2edf
    call 031beh                               ; e8 da 02                    ; 0xc2ee1
    lea dx, [di+001h]                         ; 8d 55 01                    ; 0xc2ee4
    out DX, AL                                ; ee                          ; 0xc2ee7
    inc cx                                    ; 41                          ; 0xc2ee8 vgabios.c:1914
    inc word [bp-00ah]                        ; ff 46 f6                    ; 0xc2ee9 vgabios.c:1915
    jmp short 02ecbh                          ; eb dd                       ; 0xc2eec
    mov dx, 003cch                            ; ba cc 03                    ; 0xc2eee vgabios.c:1917
    in AL, DX                                 ; ec                          ; 0xc2ef1
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2ef2
    and AL, strict byte 0feh                  ; 24 fe                       ; 0xc2ef4
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc2ef6
    cmp di, 003d4h                            ; 81 ff d4 03                 ; 0xc2ef9 vgabios.c:1918
    jne short 02f03h                          ; 75 04                       ; 0xc2efd
    or byte [bp-008h], 001h                   ; 80 4e f8 01                 ; 0xc2eff vgabios.c:1919
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2f03 vgabios.c:1920
    mov dx, 003c2h                            ; ba c2 03                    ; 0xc2f06
    out DX, AL                                ; ee                          ; 0xc2f09
    mov AL, strict byte 011h                  ; b0 11                       ; 0xc2f0a vgabios.c:1923
    mov dx, di                                ; 89 fa                       ; 0xc2f0c
    out DX, AL                                ; ee                          ; 0xc2f0e
    mov dx, cx                                ; 89 ca                       ; 0xc2f0f vgabios.c:1924
    add dx, strict byte 0fff9h                ; 83 c2 f9                    ; 0xc2f11
    mov ax, si                                ; 89 f0                       ; 0xc2f14
    call 031beh                               ; e8 a5 02                    ; 0xc2f16
    lea dx, [di+001h]                         ; 8d 55 01                    ; 0xc2f19
    out DX, AL                                ; ee                          ; 0xc2f1c
    lea dx, [bx+003h]                         ; 8d 57 03                    ; 0xc2f1d vgabios.c:1927
    mov ax, si                                ; 89 f0                       ; 0xc2f20
    call 031beh                               ; e8 99 02                    ; 0xc2f22
    xor ah, ah                                ; 30 e4                       ; 0xc2f25
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc2f27
    mov dx, 003dah                            ; ba da 03                    ; 0xc2f2a vgabios.c:1928
    in AL, DX                                 ; ec                          ; 0xc2f2d
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2f2e
    mov word [bp-00ah], strict word 00000h    ; c7 46 f6 00 00              ; 0xc2f30 vgabios.c:1929
    jmp short 02f3dh                          ; eb 06                       ; 0xc2f35
    cmp word [bp-00ah], strict byte 00013h    ; 83 7e f6 13                 ; 0xc2f37
    jnbe short 02f5bh                         ; 77 1e                       ; 0xc2f3b
    mov ax, word [bp-00ch]                    ; 8b 46 f4                    ; 0xc2f3d vgabios.c:1930
    and ax, strict word 00020h                ; 25 20 00                    ; 0xc2f40
    or ax, word [bp-00ah]                     ; 0b 46 f6                    ; 0xc2f43
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc2f46
    out DX, AL                                ; ee                          ; 0xc2f49
    mov dx, cx                                ; 89 ca                       ; 0xc2f4a vgabios.c:1931
    mov ax, si                                ; 89 f0                       ; 0xc2f4c
    call 031beh                               ; e8 6d 02                    ; 0xc2f4e
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc2f51
    out DX, AL                                ; ee                          ; 0xc2f54
    inc cx                                    ; 41                          ; 0xc2f55
    inc word [bp-00ah]                        ; ff 46 f6                    ; 0xc2f56 vgabios.c:1932
    jmp short 02f37h                          ; eb dc                       ; 0xc2f59
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc2f5b vgabios.c:1933
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc2f5e
    out DX, AL                                ; ee                          ; 0xc2f61
    mov dx, 003dah                            ; ba da 03                    ; 0xc2f62 vgabios.c:1934
    in AL, DX                                 ; ec                          ; 0xc2f65
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2f66
    mov word [bp-00ah], strict word 00000h    ; c7 46 f6 00 00              ; 0xc2f68 vgabios.c:1936
    jmp short 02f75h                          ; eb 06                       ; 0xc2f6d
    cmp word [bp-00ah], strict byte 00008h    ; 83 7e f6 08                 ; 0xc2f6f
    jnbe short 02f8dh                         ; 77 18                       ; 0xc2f73
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2f75 vgabios.c:1937
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2f78
    out DX, AL                                ; ee                          ; 0xc2f7b
    mov dx, cx                                ; 89 ca                       ; 0xc2f7c vgabios.c:1938
    mov ax, si                                ; 89 f0                       ; 0xc2f7e
    call 031beh                               ; e8 3b 02                    ; 0xc2f80
    mov dx, 003cfh                            ; ba cf 03                    ; 0xc2f83
    out DX, AL                                ; ee                          ; 0xc2f86
    inc cx                                    ; 41                          ; 0xc2f87
    inc word [bp-00ah]                        ; ff 46 f6                    ; 0xc2f88 vgabios.c:1939
    jmp short 02f6fh                          ; eb e2                       ; 0xc2f8b
    add cx, strict byte 00006h                ; 83 c1 06                    ; 0xc2f8d vgabios.c:1940
    mov dx, bx                                ; 89 da                       ; 0xc2f90 vgabios.c:1941
    mov ax, si                                ; 89 f0                       ; 0xc2f92
    call 031beh                               ; e8 27 02                    ; 0xc2f94
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc2f97
    out DX, AL                                ; ee                          ; 0xc2f9a
    inc bx                                    ; 43                          ; 0xc2f9b
    mov dx, bx                                ; 89 da                       ; 0xc2f9c vgabios.c:1944
    mov ax, si                                ; 89 f0                       ; 0xc2f9e
    call 031beh                               ; e8 1b 02                    ; 0xc2fa0
    mov dx, di                                ; 89 fa                       ; 0xc2fa3
    out DX, AL                                ; ee                          ; 0xc2fa5
    inc bx                                    ; 43                          ; 0xc2fa6
    mov dx, bx                                ; 89 da                       ; 0xc2fa7 vgabios.c:1945
    mov ax, si                                ; 89 f0                       ; 0xc2fa9
    call 031beh                               ; e8 10 02                    ; 0xc2fab
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2fae
    out DX, AL                                ; ee                          ; 0xc2fb1
    lea dx, [bx+002h]                         ; 8d 57 02                    ; 0xc2fb2
    mov ax, si                                ; 89 f0                       ; 0xc2fb5
    call 031beh                               ; e8 04 02                    ; 0xc2fb7
    lea dx, [di+006h]                         ; 8d 55 06                    ; 0xc2fba
    out DX, AL                                ; ee                          ; 0xc2fbd
    test byte [bp-00eh], 002h                 ; f6 46 f2 02                 ; 0xc2fbe vgabios.c:1949
    jne short 02fc7h                          ; 75 03                       ; 0xc2fc2
    jmp near 0313ch                           ; e9 75 01                    ; 0xc2fc4
    mov dx, cx                                ; 89 ca                       ; 0xc2fc7 vgabios.c:1950
    mov ax, si                                ; 89 f0                       ; 0xc2fc9
    call 031beh                               ; e8 f0 01                    ; 0xc2fcb
    xor ah, ah                                ; 30 e4                       ; 0xc2fce
    mov bx, ax                                ; 89 c3                       ; 0xc2fd0
    mov dx, strict word 00049h                ; ba 49 00                    ; 0xc2fd2
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2fd5
    call 031cch                               ; e8 f1 01                    ; 0xc2fd8
    inc cx                                    ; 41                          ; 0xc2fdb
    mov dx, cx                                ; 89 ca                       ; 0xc2fdc vgabios.c:1951
    mov ax, si                                ; 89 f0                       ; 0xc2fde
    call 031dah                               ; e8 f7 01                    ; 0xc2fe0
    mov bx, ax                                ; 89 c3                       ; 0xc2fe3
    mov dx, strict word 0004ah                ; ba 4a 00                    ; 0xc2fe5
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2fe8
    call 031e8h                               ; e8 fa 01                    ; 0xc2feb
    inc cx                                    ; 41                          ; 0xc2fee
    inc cx                                    ; 41                          ; 0xc2fef
    mov dx, cx                                ; 89 ca                       ; 0xc2ff0 vgabios.c:1952
    mov ax, si                                ; 89 f0                       ; 0xc2ff2
    call 031dah                               ; e8 e3 01                    ; 0xc2ff4
    mov bx, ax                                ; 89 c3                       ; 0xc2ff7
    mov dx, strict word 0004ch                ; ba 4c 00                    ; 0xc2ff9
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2ffc
    call 031e8h                               ; e8 e6 01                    ; 0xc2fff
    inc cx                                    ; 41                          ; 0xc3002
    inc cx                                    ; 41                          ; 0xc3003
    mov dx, cx                                ; 89 ca                       ; 0xc3004 vgabios.c:1953
    mov ax, si                                ; 89 f0                       ; 0xc3006
    call 031dah                               ; e8 cf 01                    ; 0xc3008
    mov bx, ax                                ; 89 c3                       ; 0xc300b
    mov dx, strict word 00063h                ; ba 63 00                    ; 0xc300d
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3010
    call 031e8h                               ; e8 d2 01                    ; 0xc3013
    inc cx                                    ; 41                          ; 0xc3016
    inc cx                                    ; 41                          ; 0xc3017
    mov dx, cx                                ; 89 ca                       ; 0xc3018 vgabios.c:1954
    mov ax, si                                ; 89 f0                       ; 0xc301a
    call 031beh                               ; e8 9f 01                    ; 0xc301c
    xor ah, ah                                ; 30 e4                       ; 0xc301f
    mov bx, ax                                ; 89 c3                       ; 0xc3021
    mov dx, 00084h                            ; ba 84 00                    ; 0xc3023
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3026
    call 031cch                               ; e8 a0 01                    ; 0xc3029
    inc cx                                    ; 41                          ; 0xc302c
    mov dx, cx                                ; 89 ca                       ; 0xc302d vgabios.c:1955
    mov ax, si                                ; 89 f0                       ; 0xc302f
    call 031dah                               ; e8 a6 01                    ; 0xc3031
    mov bx, ax                                ; 89 c3                       ; 0xc3034
    mov dx, 00085h                            ; ba 85 00                    ; 0xc3036
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3039
    call 031e8h                               ; e8 a9 01                    ; 0xc303c
    inc cx                                    ; 41                          ; 0xc303f
    inc cx                                    ; 41                          ; 0xc3040
    mov dx, cx                                ; 89 ca                       ; 0xc3041 vgabios.c:1956
    mov ax, si                                ; 89 f0                       ; 0xc3043
    call 031beh                               ; e8 76 01                    ; 0xc3045
    mov dl, al                                ; 88 c2                       ; 0xc3048
    xor dh, dh                                ; 30 f6                       ; 0xc304a
    mov bx, dx                                ; 89 d3                       ; 0xc304c
    mov dx, 00087h                            ; ba 87 00                    ; 0xc304e
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3051
    call 031cch                               ; e8 75 01                    ; 0xc3054
    inc cx                                    ; 41                          ; 0xc3057
    mov dx, cx                                ; 89 ca                       ; 0xc3058 vgabios.c:1957
    mov ax, si                                ; 89 f0                       ; 0xc305a
    call 031beh                               ; e8 5f 01                    ; 0xc305c
    mov dl, al                                ; 88 c2                       ; 0xc305f
    xor dh, dh                                ; 30 f6                       ; 0xc3061
    mov bx, dx                                ; 89 d3                       ; 0xc3063
    mov dx, 00088h                            ; ba 88 00                    ; 0xc3065
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3068
    call 031cch                               ; e8 5e 01                    ; 0xc306b
    inc cx                                    ; 41                          ; 0xc306e
    mov dx, cx                                ; 89 ca                       ; 0xc306f vgabios.c:1958
    mov ax, si                                ; 89 f0                       ; 0xc3071
    call 031beh                               ; e8 48 01                    ; 0xc3073
    mov dl, al                                ; 88 c2                       ; 0xc3076
    xor dh, dh                                ; 30 f6                       ; 0xc3078
    mov bx, dx                                ; 89 d3                       ; 0xc307a
    mov dx, 00089h                            ; ba 89 00                    ; 0xc307c
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc307f
    call 031cch                               ; e8 47 01                    ; 0xc3082
    inc cx                                    ; 41                          ; 0xc3085
    mov dx, cx                                ; 89 ca                       ; 0xc3086 vgabios.c:1959
    mov ax, si                                ; 89 f0                       ; 0xc3088
    call 031dah                               ; e8 4d 01                    ; 0xc308a
    mov bx, ax                                ; 89 c3                       ; 0xc308d
    mov dx, strict word 00060h                ; ba 60 00                    ; 0xc308f
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3092
    call 031e8h                               ; e8 50 01                    ; 0xc3095
    mov word [bp-00ah], strict word 00000h    ; c7 46 f6 00 00              ; 0xc3098 vgabios.c:1960
    inc cx                                    ; 41                          ; 0xc309d
    inc cx                                    ; 41                          ; 0xc309e
    jmp short 030a7h                          ; eb 06                       ; 0xc309f
    cmp word [bp-00ah], strict byte 00008h    ; 83 7e f6 08                 ; 0xc30a1
    jnc short 030c5h                          ; 73 1e                       ; 0xc30a5
    mov dx, cx                                ; 89 ca                       ; 0xc30a7 vgabios.c:1961
    mov ax, si                                ; 89 f0                       ; 0xc30a9
    call 031dah                               ; e8 2c 01                    ; 0xc30ab
    mov bx, ax                                ; 89 c3                       ; 0xc30ae
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc30b0
    sal dx, 1                                 ; d1 e2                       ; 0xc30b3
    add dx, strict byte 00050h                ; 83 c2 50                    ; 0xc30b5
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc30b8
    call 031e8h                               ; e8 2a 01                    ; 0xc30bb
    inc cx                                    ; 41                          ; 0xc30be vgabios.c:1962
    inc cx                                    ; 41                          ; 0xc30bf
    inc word [bp-00ah]                        ; ff 46 f6                    ; 0xc30c0 vgabios.c:1963
    jmp short 030a1h                          ; eb dc                       ; 0xc30c3
    mov dx, cx                                ; 89 ca                       ; 0xc30c5 vgabios.c:1964
    mov ax, si                                ; 89 f0                       ; 0xc30c7
    call 031dah                               ; e8 0e 01                    ; 0xc30c9
    mov bx, ax                                ; 89 c3                       ; 0xc30cc
    mov dx, strict word 0004eh                ; ba 4e 00                    ; 0xc30ce
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc30d1
    call 031e8h                               ; e8 11 01                    ; 0xc30d4
    inc cx                                    ; 41                          ; 0xc30d7
    inc cx                                    ; 41                          ; 0xc30d8
    mov dx, cx                                ; 89 ca                       ; 0xc30d9 vgabios.c:1965
    mov ax, si                                ; 89 f0                       ; 0xc30db
    call 031beh                               ; e8 de 00                    ; 0xc30dd
    mov dl, al                                ; 88 c2                       ; 0xc30e0
    xor dh, dh                                ; 30 f6                       ; 0xc30e2
    mov bx, dx                                ; 89 d3                       ; 0xc30e4
    mov dx, strict word 00062h                ; ba 62 00                    ; 0xc30e6
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc30e9
    call 031cch                               ; e8 dd 00                    ; 0xc30ec
    inc cx                                    ; 41                          ; 0xc30ef
    mov dx, cx                                ; 89 ca                       ; 0xc30f0 vgabios.c:1967
    mov ax, si                                ; 89 f0                       ; 0xc30f2
    call 031dah                               ; e8 e3 00                    ; 0xc30f4
    mov bx, ax                                ; 89 c3                       ; 0xc30f7
    mov dx, strict word 0007ch                ; ba 7c 00                    ; 0xc30f9
    xor ax, ax                                ; 31 c0                       ; 0xc30fc
    call 031e8h                               ; e8 e7 00                    ; 0xc30fe
    inc cx                                    ; 41                          ; 0xc3101
    inc cx                                    ; 41                          ; 0xc3102
    mov dx, cx                                ; 89 ca                       ; 0xc3103 vgabios.c:1968
    mov ax, si                                ; 89 f0                       ; 0xc3105
    call 031dah                               ; e8 d0 00                    ; 0xc3107
    mov bx, ax                                ; 89 c3                       ; 0xc310a
    mov dx, strict word 0007eh                ; ba 7e 00                    ; 0xc310c
    xor ax, ax                                ; 31 c0                       ; 0xc310f
    call 031e8h                               ; e8 d4 00                    ; 0xc3111
    inc cx                                    ; 41                          ; 0xc3114
    inc cx                                    ; 41                          ; 0xc3115
    mov dx, cx                                ; 89 ca                       ; 0xc3116 vgabios.c:1969
    mov ax, si                                ; 89 f0                       ; 0xc3118
    call 031dah                               ; e8 bd 00                    ; 0xc311a
    mov bx, ax                                ; 89 c3                       ; 0xc311d
    mov dx, 0010ch                            ; ba 0c 01                    ; 0xc311f
    xor ax, ax                                ; 31 c0                       ; 0xc3122
    call 031e8h                               ; e8 c1 00                    ; 0xc3124
    inc cx                                    ; 41                          ; 0xc3127
    inc cx                                    ; 41                          ; 0xc3128
    mov dx, cx                                ; 89 ca                       ; 0xc3129 vgabios.c:1970
    mov ax, si                                ; 89 f0                       ; 0xc312b
    call 031dah                               ; e8 aa 00                    ; 0xc312d
    mov bx, ax                                ; 89 c3                       ; 0xc3130
    mov dx, 0010eh                            ; ba 0e 01                    ; 0xc3132
    xor ax, ax                                ; 31 c0                       ; 0xc3135
    call 031e8h                               ; e8 ae 00                    ; 0xc3137
    inc cx                                    ; 41                          ; 0xc313a
    inc cx                                    ; 41                          ; 0xc313b
    test byte [bp-00eh], 004h                 ; f6 46 f2 04                 ; 0xc313c vgabios.c:1972
    je short 03189h                           ; 74 47                       ; 0xc3140
    inc cx                                    ; 41                          ; 0xc3142 vgabios.c:1973
    mov dx, cx                                ; 89 ca                       ; 0xc3143 vgabios.c:1974
    mov ax, si                                ; 89 f0                       ; 0xc3145
    call 031beh                               ; e8 74 00                    ; 0xc3147
    xor ah, ah                                ; 30 e4                       ; 0xc314a
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc314c
    inc cx                                    ; 41                          ; 0xc314f
    mov dx, cx                                ; 89 ca                       ; 0xc3150 vgabios.c:1975
    mov ax, si                                ; 89 f0                       ; 0xc3152
    call 031beh                               ; e8 67 00                    ; 0xc3154
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc3157
    out DX, AL                                ; ee                          ; 0xc315a
    inc cx                                    ; 41                          ; 0xc315b vgabios.c:1977
    xor al, al                                ; 30 c0                       ; 0xc315c
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc315e
    out DX, AL                                ; ee                          ; 0xc3161
    xor ah, ah                                ; 30 e4                       ; 0xc3162 vgabios.c:1978
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc3164
    jmp short 03170h                          ; eb 07                       ; 0xc3167
    cmp word [bp-00ah], 00300h                ; 81 7e f6 00 03              ; 0xc3169
    jnc short 03181h                          ; 73 11                       ; 0xc316e
    mov dx, cx                                ; 89 ca                       ; 0xc3170 vgabios.c:1979
    mov ax, si                                ; 89 f0                       ; 0xc3172
    call 031beh                               ; e8 47 00                    ; 0xc3174
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc3177
    out DX, AL                                ; ee                          ; 0xc317a
    inc cx                                    ; 41                          ; 0xc317b
    inc word [bp-00ah]                        ; ff 46 f6                    ; 0xc317c vgabios.c:1980
    jmp short 03169h                          ; eb e8                       ; 0xc317f
    inc cx                                    ; 41                          ; 0xc3181 vgabios.c:1981
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc3182
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc3185
    out DX, AL                                ; ee                          ; 0xc3188
    mov ax, cx                                ; 89 c8                       ; 0xc3189 vgabios.c:1985
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc318b
    pop di                                    ; 5f                          ; 0xc318e
    pop si                                    ; 5e                          ; 0xc318f
    pop cx                                    ; 59                          ; 0xc3190
    pop bp                                    ; 5d                          ; 0xc3191
    retn                                      ; c3                          ; 0xc3192
  ; disGetNextSymbol 0xc3193 LB 0xb4c -> off=0x0 cb=000000000000002b uValue=00000000000c3193 'find_vga_entry'
find_vga_entry:                              ; 0xc3193 LB 0x2b
    push bx                                   ; 53                          ; 0xc3193 vgabios.c:1994
    push cx                                   ; 51                          ; 0xc3194
    push dx                                   ; 52                          ; 0xc3195
    push bp                                   ; 55                          ; 0xc3196
    mov bp, sp                                ; 89 e5                       ; 0xc3197
    mov dl, al                                ; 88 c2                       ; 0xc3199
    mov AH, strict byte 0ffh                  ; b4 ff                       ; 0xc319b vgabios.c:1996
    xor al, al                                ; 30 c0                       ; 0xc319d vgabios.c:1997
    jmp short 031a7h                          ; eb 06                       ; 0xc319f
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc31a1 vgabios.c:1998
    cmp AL, strict byte 00fh                  ; 3c 0f                       ; 0xc31a3
    jnbe short 031b7h                         ; 77 10                       ; 0xc31a5
    mov bl, al                                ; 88 c3                       ; 0xc31a7
    xor bh, bh                                ; 30 ff                       ; 0xc31a9
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc31ab
    sal bx, CL                                ; d3 e3                       ; 0xc31ad
    cmp dl, byte [bx+04634h]                  ; 3a 97 34 46                 ; 0xc31af
    jne short 031a1h                          ; 75 ec                       ; 0xc31b3
    mov ah, al                                ; 88 c4                       ; 0xc31b5
    mov al, ah                                ; 88 e0                       ; 0xc31b7 vgabios.c:2003
    pop bp                                    ; 5d                          ; 0xc31b9
    pop dx                                    ; 5a                          ; 0xc31ba
    pop cx                                    ; 59                          ; 0xc31bb
    pop bx                                    ; 5b                          ; 0xc31bc
    retn                                      ; c3                          ; 0xc31bd
  ; disGetNextSymbol 0xc31be LB 0xb21 -> off=0x0 cb=000000000000000e uValue=00000000000c31be 'read_byte'
read_byte:                                   ; 0xc31be LB 0xe
    push bx                                   ; 53                          ; 0xc31be vgabios.c:2011
    push bp                                   ; 55                          ; 0xc31bf
    mov bp, sp                                ; 89 e5                       ; 0xc31c0
    mov bx, dx                                ; 89 d3                       ; 0xc31c2
    mov es, ax                                ; 8e c0                       ; 0xc31c4 vgabios.c:2013
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc31c6
    pop bp                                    ; 5d                          ; 0xc31c9 vgabios.c:2014
    pop bx                                    ; 5b                          ; 0xc31ca
    retn                                      ; c3                          ; 0xc31cb
  ; disGetNextSymbol 0xc31cc LB 0xb13 -> off=0x0 cb=000000000000000e uValue=00000000000c31cc 'write_byte'
write_byte:                                  ; 0xc31cc LB 0xe
    push si                                   ; 56                          ; 0xc31cc vgabios.c:2016
    push bp                                   ; 55                          ; 0xc31cd
    mov bp, sp                                ; 89 e5                       ; 0xc31ce
    mov si, dx                                ; 89 d6                       ; 0xc31d0
    mov es, ax                                ; 8e c0                       ; 0xc31d2 vgabios.c:2018
    mov byte [es:si], bl                      ; 26 88 1c                    ; 0xc31d4
    pop bp                                    ; 5d                          ; 0xc31d7 vgabios.c:2019
    pop si                                    ; 5e                          ; 0xc31d8
    retn                                      ; c3                          ; 0xc31d9
  ; disGetNextSymbol 0xc31da LB 0xb05 -> off=0x0 cb=000000000000000e uValue=00000000000c31da 'read_word'
read_word:                                   ; 0xc31da LB 0xe
    push bx                                   ; 53                          ; 0xc31da vgabios.c:2021
    push bp                                   ; 55                          ; 0xc31db
    mov bp, sp                                ; 89 e5                       ; 0xc31dc
    mov bx, dx                                ; 89 d3                       ; 0xc31de
    mov es, ax                                ; 8e c0                       ; 0xc31e0 vgabios.c:2023
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc31e2
    pop bp                                    ; 5d                          ; 0xc31e5 vgabios.c:2024
    pop bx                                    ; 5b                          ; 0xc31e6
    retn                                      ; c3                          ; 0xc31e7
  ; disGetNextSymbol 0xc31e8 LB 0xaf7 -> off=0x0 cb=000000000000000e uValue=00000000000c31e8 'write_word'
write_word:                                  ; 0xc31e8 LB 0xe
    push si                                   ; 56                          ; 0xc31e8 vgabios.c:2026
    push bp                                   ; 55                          ; 0xc31e9
    mov bp, sp                                ; 89 e5                       ; 0xc31ea
    mov si, dx                                ; 89 d6                       ; 0xc31ec
    mov es, ax                                ; 8e c0                       ; 0xc31ee vgabios.c:2028
    mov word [es:si], bx                      ; 26 89 1c                    ; 0xc31f0
    pop bp                                    ; 5d                          ; 0xc31f3 vgabios.c:2029
    pop si                                    ; 5e                          ; 0xc31f4
    retn                                      ; c3                          ; 0xc31f5
  ; disGetNextSymbol 0xc31f6 LB 0xae9 -> off=0x0 cb=0000000000000012 uValue=00000000000c31f6 'read_dword'
read_dword:                                  ; 0xc31f6 LB 0x12
    push bx                                   ; 53                          ; 0xc31f6 vgabios.c:2031
    push bp                                   ; 55                          ; 0xc31f7
    mov bp, sp                                ; 89 e5                       ; 0xc31f8
    mov bx, dx                                ; 89 d3                       ; 0xc31fa
    mov es, ax                                ; 8e c0                       ; 0xc31fc vgabios.c:2033
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc31fe
    mov dx, word [es:bx+002h]                 ; 26 8b 57 02                 ; 0xc3201
    pop bp                                    ; 5d                          ; 0xc3205 vgabios.c:2034
    pop bx                                    ; 5b                          ; 0xc3206
    retn                                      ; c3                          ; 0xc3207
  ; disGetNextSymbol 0xc3208 LB 0xad7 -> off=0x0 cb=0000000000000012 uValue=00000000000c3208 'write_dword'
write_dword:                                 ; 0xc3208 LB 0x12
    push si                                   ; 56                          ; 0xc3208 vgabios.c:2036
    push bp                                   ; 55                          ; 0xc3209
    mov bp, sp                                ; 89 e5                       ; 0xc320a
    mov si, dx                                ; 89 d6                       ; 0xc320c
    mov es, ax                                ; 8e c0                       ; 0xc320e vgabios.c:2038
    mov word [es:si], bx                      ; 26 89 1c                    ; 0xc3210
    mov word [es:si+002h], cx                 ; 26 89 4c 02                 ; 0xc3213
    pop bp                                    ; 5d                          ; 0xc3217 vgabios.c:2039
    pop si                                    ; 5e                          ; 0xc3218
    retn                                      ; c3                          ; 0xc3219
  ; disGetNextSymbol 0xc321a LB 0xac5 -> off=0x84 cb=0000000000000348 uValue=00000000000c329e 'int10_func'
    db  04fh, 01ch, 01bh, 013h, 012h, 011h, 010h, 00eh, 00dh, 00ch, 00ah, 009h, 008h, 007h, 006h, 005h
    db  004h, 003h, 002h, 001h, 000h, 0dfh, 035h, 0ceh, 032h, 00bh, 033h, 019h, 033h, 024h, 033h, 032h
    db  033h, 042h, 033h, 049h, 033h, 072h, 033h, 076h, 033h, 081h, 033h, 096h, 033h, 0ach, 033h, 0c5h
    db  033h, 0d7h, 033h, 0ebh, 033h, 0f7h, 033h, 0aah, 034h, 0dfh, 034h, 006h, 035h, 01bh, 035h, 058h
    db  035h, 030h, 024h, 023h, 022h, 021h, 020h, 014h, 012h, 011h, 010h, 004h, 002h, 001h, 000h, 0dfh
    db  035h, 014h, 034h, 032h, 034h, 041h, 034h, 050h, 034h, 014h, 034h, 032h, 034h, 041h, 034h, 050h
    db  034h, 05fh, 034h, 06bh, 034h, 084h, 034h, 089h, 034h, 08eh, 034h, 093h, 034h, 00ah, 009h, 006h
    db  004h, 002h, 001h, 000h, 0d3h, 035h, 07eh, 035h, 08bh, 035h, 09bh, 035h, 0abh, 035h, 0c0h, 035h
    db  0d3h, 035h, 0d3h, 035h
int10_func:                                  ; 0xc329e LB 0x348
    push bp                                   ; 55                          ; 0xc329e vgabios.c:2115
    mov bp, sp                                ; 89 e5                       ; 0xc329f
    push si                                   ; 56                          ; 0xc32a1
    push di                                   ; 57                          ; 0xc32a2
    push ax                                   ; 50                          ; 0xc32a3
    mov si, word [bp+004h]                    ; 8b 76 04                    ; 0xc32a4
    mov al, byte [bp+013h]                    ; 8a 46 13                    ; 0xc32a7 vgabios.c:2120
    xor ah, ah                                ; 30 e4                       ; 0xc32aa
    cmp ax, strict word 0004fh                ; 3d 4f 00                    ; 0xc32ac
    jnbe short 03316h                         ; 77 65                       ; 0xc32af
    push CS                                   ; 0e                          ; 0xc32b1
    pop ES                                    ; 07                          ; 0xc32b2
    mov cx, strict word 00016h                ; b9 16 00                    ; 0xc32b3
    mov di, 0321ah                            ; bf 1a 32                    ; 0xc32b6
    repne scasb                               ; f2 ae                       ; 0xc32b9
    sal cx, 1                                 ; d1 e1                       ; 0xc32bb
    mov di, cx                                ; 89 cf                       ; 0xc32bd
    mov bx, word [cs:di+0322fh]               ; 2e 8b 9d 2f 32              ; 0xc32bf
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc32c4
    xor ah, ah                                ; 30 e4                       ; 0xc32c7
    mov dl, byte [bp+012h]                    ; 8a 56 12                    ; 0xc32c9
    jmp bx                                    ; ff e3                       ; 0xc32cc
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc32ce vgabios.c:2123
    xor ah, ah                                ; 30 e4                       ; 0xc32d1
    call 01019h                               ; e8 43 dd                    ; 0xc32d3
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc32d6 vgabios.c:2124
    and ax, strict word 0007fh                ; 25 7f 00                    ; 0xc32d9
    cmp ax, strict word 00007h                ; 3d 07 00                    ; 0xc32dc
    je short 032f6h                           ; 74 15                       ; 0xc32df
    cmp ax, strict word 00006h                ; 3d 06 00                    ; 0xc32e1
    je short 032edh                           ; 74 07                       ; 0xc32e4
    cmp ax, strict word 00005h                ; 3d 05 00                    ; 0xc32e6
    jbe short 032f6h                          ; 76 0b                       ; 0xc32e9
    jmp short 032ffh                          ; eb 12                       ; 0xc32eb
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc32ed vgabios.c:2126
    xor al, al                                ; 30 c0                       ; 0xc32f0
    or AL, strict byte 03fh                   ; 0c 3f                       ; 0xc32f2
    jmp short 03306h                          ; eb 10                       ; 0xc32f4 vgabios.c:2127
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc32f6 vgabios.c:2135
    xor al, al                                ; 30 c0                       ; 0xc32f9
    or AL, strict byte 030h                   ; 0c 30                       ; 0xc32fb
    jmp short 03306h                          ; eb 07                       ; 0xc32fd
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc32ff vgabios.c:2138
    xor al, al                                ; 30 c0                       ; 0xc3302
    or AL, strict byte 020h                   ; 0c 20                       ; 0xc3304
    mov word [bp+012h], ax                    ; 89 46 12                    ; 0xc3306
    jmp short 03316h                          ; eb 0b                       ; 0xc3309 vgabios.c:2140
    mov al, byte [bp+010h]                    ; 8a 46 10                    ; 0xc330b vgabios.c:2142
    mov dx, ax                                ; 89 c2                       ; 0xc330e
    mov al, byte [bp+011h]                    ; 8a 46 11                    ; 0xc3310
    call 00ddeh                               ; e8 c8 da                    ; 0xc3313
    jmp near 035dfh                           ; e9 c6 02                    ; 0xc3316 vgabios.c:2143
    mov dx, word [bp+00eh]                    ; 8b 56 0e                    ; 0xc3319 vgabios.c:2145
    mov al, byte [bp+00dh]                    ; 8a 46 0d                    ; 0xc331c
    call 00e91h                               ; e8 6f db                    ; 0xc331f
    jmp short 03316h                          ; eb f2                       ; 0xc3322 vgabios.c:2146
    lea bx, [bp+00eh]                         ; 8d 5e 0e                    ; 0xc3324 vgabios.c:2148
    lea dx, [bp+010h]                         ; 8d 56 10                    ; 0xc3327
    mov al, byte [bp+00dh]                    ; 8a 46 0d                    ; 0xc332a
    call 00a8ch                               ; e8 5c d7                    ; 0xc332d
    jmp short 03316h                          ; eb e4                       ; 0xc3330 vgabios.c:2149
    xor al, al                                ; 30 c0                       ; 0xc3332 vgabios.c:2155
    mov word [bp+012h], ax                    ; 89 46 12                    ; 0xc3334
    mov word [bp+00ch], ax                    ; 89 46 0c                    ; 0xc3337 vgabios.c:2156
    mov word [bp+010h], ax                    ; 89 46 10                    ; 0xc333a vgabios.c:2157
    mov word [bp+00eh], ax                    ; 89 46 0e                    ; 0xc333d vgabios.c:2158
    jmp short 03316h                          ; eb d4                       ; 0xc3340 vgabios.c:2159
    mov al, dl                                ; 88 d0                       ; 0xc3342 vgabios.c:2161
    call 00f34h                               ; e8 ed db                    ; 0xc3344
    jmp short 03316h                          ; eb cd                       ; 0xc3347 vgabios.c:2162
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc3349 vgabios.c:2164
    push ax                                   ; 50                          ; 0xc334c
    mov ax, 000ffh                            ; b8 ff 00                    ; 0xc334d
    push ax                                   ; 50                          ; 0xc3350
    mov al, byte [bp+00eh]                    ; 8a 46 0e                    ; 0xc3351
    xor ah, ah                                ; 30 e4                       ; 0xc3354
    push ax                                   ; 50                          ; 0xc3356
    mov al, byte [bp+00fh]                    ; 8a 46 0f                    ; 0xc3357
    push ax                                   ; 50                          ; 0xc335a
    mov al, byte [bp+010h]                    ; 8a 46 10                    ; 0xc335b
    mov cx, ax                                ; 89 c1                       ; 0xc335e
    mov al, byte [bp+011h]                    ; 8a 46 11                    ; 0xc3360
    mov bx, ax                                ; 89 c3                       ; 0xc3363
    mov al, byte [bp+00dh]                    ; 8a 46 0d                    ; 0xc3365
    mov dx, ax                                ; 89 c2                       ; 0xc3368
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc336a
    call 01678h                               ; e8 08 e3                    ; 0xc336d
    jmp short 03316h                          ; eb a4                       ; 0xc3370 vgabios.c:2165
    xor al, al                                ; 30 c0                       ; 0xc3372 vgabios.c:2167
    jmp short 0334ch                          ; eb d6                       ; 0xc3374
    lea dx, [bp+012h]                         ; 8d 56 12                    ; 0xc3376 vgabios.c:2170
    mov al, byte [bp+00dh]                    ; 8a 46 0d                    ; 0xc3379
    call 00ad2h                               ; e8 53 d7                    ; 0xc337c
    jmp short 03316h                          ; eb 95                       ; 0xc337f vgabios.c:2171
    mov cx, word [bp+010h]                    ; 8b 4e 10                    ; 0xc3381 vgabios.c:2173
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc3384
    mov bx, ax                                ; 89 c3                       ; 0xc3387
    mov al, byte [bp+00dh]                    ; 8a 46 0d                    ; 0xc3389
    mov dx, ax                                ; 89 c2                       ; 0xc338c
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc338e
    call 01ecch                               ; e8 38 eb                    ; 0xc3391
    jmp short 03316h                          ; eb 80                       ; 0xc3394 vgabios.c:2174
    mov cx, word [bp+010h]                    ; 8b 4e 10                    ; 0xc3396 vgabios.c:2176
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc3399
    mov bx, ax                                ; 89 c3                       ; 0xc339c
    mov al, byte [bp+00dh]                    ; 8a 46 0d                    ; 0xc339e
    mov dx, ax                                ; 89 c2                       ; 0xc33a1
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc33a3
    call 0205eh                               ; e8 b5 ec                    ; 0xc33a6
    jmp near 035dfh                           ; e9 33 02                    ; 0xc33a9 vgabios.c:2177
    mov cx, word [bp+00eh]                    ; 8b 4e 0e                    ; 0xc33ac vgabios.c:2179
    mov bx, word [bp+010h]                    ; 8b 5e 10                    ; 0xc33af
    mov al, dl                                ; 88 d0                       ; 0xc33b2
    mov dx, ax                                ; 89 c2                       ; 0xc33b4
    mov al, byte [bp+00dh]                    ; 8a 46 0d                    ; 0xc33b6
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc33b9
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc33bc
    call 021f1h                               ; e8 2f ee                    ; 0xc33bf
    jmp near 035dfh                           ; e9 1a 02                    ; 0xc33c2 vgabios.c:2180
    lea cx, [bp+012h]                         ; 8d 4e 12                    ; 0xc33c5 vgabios.c:2182
    mov bx, word [bp+00eh]                    ; 8b 5e 0e                    ; 0xc33c8
    mov dx, word [bp+010h]                    ; 8b 56 10                    ; 0xc33cb
    mov al, byte [bp+00dh]                    ; 8a 46 0d                    ; 0xc33ce
    call 00bfch                               ; e8 28 d8                    ; 0xc33d1
    jmp near 035dfh                           ; e9 08 02                    ; 0xc33d4 vgabios.c:2183
    mov cx, strict word 00002h                ; b9 02 00                    ; 0xc33d7 vgabios.c:2191
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc33da
    mov bx, ax                                ; 89 c3                       ; 0xc33dd
    mov dx, 000ffh                            ; ba ff 00                    ; 0xc33df
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc33e2
    call 02370h                               ; e8 88 ef                    ; 0xc33e5
    jmp near 035dfh                           ; e9 f4 01                    ; 0xc33e8 vgabios.c:2192
    mov dx, word [bp+010h]                    ; 8b 56 10                    ; 0xc33eb vgabios.c:2195
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc33ee
    call 00d3fh                               ; e8 4b d9                    ; 0xc33f1
    jmp near 035dfh                           ; e9 e8 01                    ; 0xc33f4 vgabios.c:2196
    cmp ax, strict word 00030h                ; 3d 30 00                    ; 0xc33f7 vgabios.c:2198
    jnbe short 03468h                         ; 77 6c                       ; 0xc33fa
    push CS                                   ; 0e                          ; 0xc33fc
    pop ES                                    ; 07                          ; 0xc33fd
    mov cx, strict word 0000fh                ; b9 0f 00                    ; 0xc33fe
    mov di, 0325bh                            ; bf 5b 32                    ; 0xc3401
    repne scasb                               ; f2 ae                       ; 0xc3404
    sal cx, 1                                 ; d1 e1                       ; 0xc3406
    mov di, cx                                ; 89 cf                       ; 0xc3408
    mov dx, word [cs:di+03269h]               ; 2e 8b 95 69 32              ; 0xc340a
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc340f
    jmp dx                                    ; ff e2                       ; 0xc3412
    mov al, byte [bp+00dh]                    ; 8a 46 0d                    ; 0xc3414 vgabios.c:2202
    xor ah, ah                                ; 30 e4                       ; 0xc3417
    push ax                                   ; 50                          ; 0xc3419
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc341a
    push ax                                   ; 50                          ; 0xc341d
    push word [bp+00eh]                       ; ff 76 0e                    ; 0xc341e
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3421
    mov cx, word [bp+010h]                    ; 8b 4e 10                    ; 0xc3424
    mov bx, word [bp+008h]                    ; 8b 5e 08                    ; 0xc3427
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc342a
    call 02702h                               ; e8 d2 f2                    ; 0xc342d
    jmp short 03468h                          ; eb 36                       ; 0xc3430 vgabios.c:2203
    mov dl, byte [bp+00ch]                    ; 8a 56 0c                    ; 0xc3432 vgabios.c:2206
    xor dh, dh                                ; 30 f6                       ; 0xc3435
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3437
    xor ah, ah                                ; 30 e4                       ; 0xc343a
    call 02787h                               ; e8 48 f3                    ; 0xc343c
    jmp short 03468h                          ; eb 27                       ; 0xc343f vgabios.c:2207
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc3441 vgabios.c:2210
    xor ah, ah                                ; 30 e4                       ; 0xc3444
    mov dx, ax                                ; 89 c2                       ; 0xc3446
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3448
    call 027fdh                               ; e8 af f3                    ; 0xc344b
    jmp short 03468h                          ; eb 18                       ; 0xc344e vgabios.c:2211
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc3450 vgabios.c:2214
    xor ah, ah                                ; 30 e4                       ; 0xc3453
    mov dx, ax                                ; 89 c2                       ; 0xc3455
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3457
    call 02871h                               ; e8 14 f4                    ; 0xc345a
    jmp short 03468h                          ; eb 09                       ; 0xc345d vgabios.c:2215
    mov dx, word [bp+008h]                    ; 8b 56 08                    ; 0xc345f vgabios.c:2217
    mov ax, word [bp+016h]                    ; 8b 46 16                    ; 0xc3462
    call 028e5h                               ; e8 7d f4                    ; 0xc3465
    jmp near 035dfh                           ; e9 74 01                    ; 0xc3468 vgabios.c:2218
    mov al, byte [bp+00eh]                    ; 8a 46 0e                    ; 0xc346b vgabios.c:2220
    push ax                                   ; 50                          ; 0xc346e
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc346f
    mov bx, word [bp+010h]                    ; 8b 5e 10                    ; 0xc3472
    mov dx, word [bp+008h]                    ; 8b 56 08                    ; 0xc3475
    mov si, word [bp+016h]                    ; 8b 76 16                    ; 0xc3478
    mov cx, ax                                ; 89 c1                       ; 0xc347b
    mov ax, si                                ; 89 f0                       ; 0xc347d
    call 028eah                               ; e8 68 f4                    ; 0xc347f
    jmp short 03468h                          ; eb e4                       ; 0xc3482 vgabios.c:2221
    call 028f1h                               ; e8 6a f4                    ; 0xc3484 vgabios.c:2223
    jmp short 03468h                          ; eb df                       ; 0xc3487 vgabios.c:2224
    call 028f6h                               ; e8 6a f4                    ; 0xc3489 vgabios.c:2226
    jmp short 03468h                          ; eb da                       ; 0xc348c vgabios.c:2227
    call 028fbh                               ; e8 6a f4                    ; 0xc348e vgabios.c:2229
    jmp short 03468h                          ; eb d5                       ; 0xc3491 vgabios.c:2230
    lea ax, [bp+00eh]                         ; 8d 46 0e                    ; 0xc3493 vgabios.c:2232
    push ax                                   ; 50                          ; 0xc3496
    mov al, byte [bp+00dh]                    ; 8a 46 0d                    ; 0xc3497
    xor ah, ah                                ; 30 e4                       ; 0xc349a
    lea cx, [bp+010h]                         ; 8d 4e 10                    ; 0xc349c
    lea bx, [bp+008h]                         ; 8d 5e 08                    ; 0xc349f
    lea dx, [bp+016h]                         ; 8d 56 16                    ; 0xc34a2
    call 00b81h                               ; e8 d9 d6                    ; 0xc34a5
    jmp short 03468h                          ; eb be                       ; 0xc34a8 vgabios.c:2240
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc34aa vgabios.c:2242
    xor ah, ah                                ; 30 e4                       ; 0xc34ad
    cmp ax, strict word 00036h                ; 3d 36 00                    ; 0xc34af
    je short 034d8h                           ; 74 24                       ; 0xc34b2
    cmp ax, strict word 00035h                ; 3d 35 00                    ; 0xc34b4
    je short 034c3h                           ; 74 0a                       ; 0xc34b7
    cmp ax, strict word 00020h                ; 3d 20 00                    ; 0xc34b9
    jne short 03503h                          ; 75 45                       ; 0xc34bc
    call 02900h                               ; e8 3f f4                    ; 0xc34be vgabios.c:2245
    jmp short 03503h                          ; eb 40                       ; 0xc34c1 vgabios.c:2246
    mov al, dl                                ; 88 d0                       ; 0xc34c3 vgabios.c:2248
    mov bx, word [bp+00eh]                    ; 8b 5e 0e                    ; 0xc34c5
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc34c8
    call 02905h                               ; e8 37 f4                    ; 0xc34cb
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc34ce vgabios.c:2249
    xor al, al                                ; 30 c0                       ; 0xc34d1
    or AL, strict byte 012h                   ; 0c 12                       ; 0xc34d3
    jmp near 03306h                           ; e9 2e fe                    ; 0xc34d5
    mov al, dl                                ; 88 d0                       ; 0xc34d8 vgabios.c:2252
    call 0290ah                               ; e8 2d f4                    ; 0xc34da
    jmp short 034ceh                          ; eb ef                       ; 0xc34dd
    push word [bp+008h]                       ; ff 76 08                    ; 0xc34df vgabios.c:2262
    push word [bp+016h]                       ; ff 76 16                    ; 0xc34e2
    mov al, byte [bp+00eh]                    ; 8a 46 0e                    ; 0xc34e5
    push ax                                   ; 50                          ; 0xc34e8
    mov al, byte [bp+00fh]                    ; 8a 46 0f                    ; 0xc34e9
    push ax                                   ; 50                          ; 0xc34ec
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc34ed
    mov bx, ax                                ; 89 c3                       ; 0xc34f0
    mov al, byte [bp+00dh]                    ; 8a 46 0d                    ; 0xc34f2
    xor dh, dh                                ; 30 f6                       ; 0xc34f5
    mov si, dx                                ; 89 d6                       ; 0xc34f7
    mov cx, word [bp+010h]                    ; 8b 4e 10                    ; 0xc34f9
    mov dx, ax                                ; 89 c2                       ; 0xc34fc
    mov ax, si                                ; 89 f0                       ; 0xc34fe
    call 0290fh                               ; e8 0c f4                    ; 0xc3500
    jmp near 035dfh                           ; e9 d9 00                    ; 0xc3503 vgabios.c:2263
    mov bx, si                                ; 89 f3                       ; 0xc3506 vgabios.c:2265
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc3508
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc350b
    call 029a5h                               ; e8 94 f4                    ; 0xc350e
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3511 vgabios.c:2266
    xor al, al                                ; 30 c0                       ; 0xc3514
    or AL, strict byte 01bh                   ; 0c 1b                       ; 0xc3516
    jmp near 03306h                           ; e9 eb fd                    ; 0xc3518
    cmp ax, strict word 00002h                ; 3d 02 00                    ; 0xc351b vgabios.c:2269
    je short 03542h                           ; 74 22                       ; 0xc351e
    cmp ax, strict word 00001h                ; 3d 01 00                    ; 0xc3520
    je short 03534h                           ; 74 0f                       ; 0xc3523
    test ax, ax                               ; 85 c0                       ; 0xc3525
    jne short 0354eh                          ; 75 25                       ; 0xc3527
    lea dx, [bp+00ch]                         ; 8d 56 0c                    ; 0xc3529 vgabios.c:2272
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc352c
    call 02acah                               ; e8 98 f5                    ; 0xc352f
    jmp short 0354eh                          ; eb 1a                       ; 0xc3532 vgabios.c:2273
    mov bx, word [bp+00ch]                    ; 8b 5e 0c                    ; 0xc3534 vgabios.c:2275
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc3537
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc353a
    call 02adch                               ; e8 9c f5                    ; 0xc353d
    jmp short 0354eh                          ; eb 0c                       ; 0xc3540 vgabios.c:2276
    mov bx, word [bp+00ch]                    ; 8b 5e 0c                    ; 0xc3542 vgabios.c:2278
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc3545
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc3548
    call 02e5dh                               ; e8 0f f9                    ; 0xc354b
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc354e vgabios.c:2285
    xor al, al                                ; 30 c0                       ; 0xc3551
    or AL, strict byte 01ch                   ; 0c 1c                       ; 0xc3553
    jmp near 03306h                           ; e9 ae fd                    ; 0xc3555
    call 007e8h                               ; e8 8d d2                    ; 0xc3558 vgabios.c:2290
    test ax, ax                               ; 85 c0                       ; 0xc355b
    je short 035d1h                           ; 74 72                       ; 0xc355d
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc355f vgabios.c:2291
    xor ah, ah                                ; 30 e4                       ; 0xc3562
    cmp ax, strict word 0000ah                ; 3d 0a 00                    ; 0xc3564
    jnbe short 035d3h                         ; 77 6a                       ; 0xc3567
    push CS                                   ; 0e                          ; 0xc3569
    pop ES                                    ; 07                          ; 0xc356a
    mov cx, strict word 00008h                ; b9 08 00                    ; 0xc356b
    mov di, 03287h                            ; bf 87 32                    ; 0xc356e
    repne scasb                               ; f2 ae                       ; 0xc3571
    sal cx, 1                                 ; d1 e1                       ; 0xc3573
    mov di, cx                                ; 89 cf                       ; 0xc3575
    mov ax, word [cs:di+0328eh]               ; 2e 8b 85 8e 32              ; 0xc3577
    jmp ax                                    ; ff e0                       ; 0xc357c
    mov bx, si                                ; 89 f3                       ; 0xc357e vgabios.c:2294
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc3580
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3583
    call 0379bh                               ; e8 12 02                    ; 0xc3586
    jmp short 035dfh                          ; eb 54                       ; 0xc3589 vgabios.c:2295
    mov cx, si                                ; 89 f1                       ; 0xc358b vgabios.c:2297
    mov bx, word [bp+016h]                    ; 8b 5e 16                    ; 0xc358d
    mov dx, word [bp+010h]                    ; 8b 56 10                    ; 0xc3590
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3593
    call 038cah                               ; e8 31 03                    ; 0xc3596
    jmp short 035dfh                          ; eb 44                       ; 0xc3599 vgabios.c:2298
    mov cx, si                                ; 89 f1                       ; 0xc359b vgabios.c:2300
    mov bx, word [bp+016h]                    ; 8b 5e 16                    ; 0xc359d
    mov dx, word [bp+00ch]                    ; 8b 56 0c                    ; 0xc35a0
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc35a3
    call 03987h                               ; e8 de 03                    ; 0xc35a6
    jmp short 035dfh                          ; eb 34                       ; 0xc35a9 vgabios.c:2301
    lea ax, [bp+00ch]                         ; 8d 46 0c                    ; 0xc35ab vgabios.c:2303
    push ax                                   ; 50                          ; 0xc35ae
    mov cx, word [bp+016h]                    ; 8b 4e 16                    ; 0xc35af
    mov bx, word [bp+00eh]                    ; 8b 5e 0e                    ; 0xc35b2
    mov dx, word [bp+010h]                    ; 8b 56 10                    ; 0xc35b5
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc35b8
    call 03b70h                               ; e8 b2 05                    ; 0xc35bb
    jmp short 035dfh                          ; eb 1f                       ; 0xc35be vgabios.c:2304
    lea cx, [bp+00eh]                         ; 8d 4e 0e                    ; 0xc35c0 vgabios.c:2306
    lea bx, [bp+010h]                         ; 8d 5e 10                    ; 0xc35c3
    lea dx, [bp+00ch]                         ; 8d 56 0c                    ; 0xc35c6
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc35c9
    call 03bfdh                               ; e8 2e 06                    ; 0xc35cc
    jmp short 035dfh                          ; eb 0e                       ; 0xc35cf vgabios.c:2307
    jmp short 035dah                          ; eb 07                       ; 0xc35d1
    mov word [bp+012h], 00100h                ; c7 46 12 00 01              ; 0xc35d3 vgabios.c:2329
    jmp short 035dfh                          ; eb 05                       ; 0xc35d8 vgabios.c:2332
    mov word [bp+012h], 00100h                ; c7 46 12 00 01              ; 0xc35da vgabios.c:2334
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc35df vgabios.c:2344
    pop di                                    ; 5f                          ; 0xc35e2
    pop si                                    ; 5e                          ; 0xc35e3
    pop bp                                    ; 5d                          ; 0xc35e4
    retn                                      ; c3                          ; 0xc35e5
  ; disGetNextSymbol 0xc35e6 LB 0x6f9 -> off=0x0 cb=000000000000001f uValue=00000000000c35e6 'dispi_set_xres'
dispi_set_xres:                              ; 0xc35e6 LB 0x1f
    push bp                                   ; 55                          ; 0xc35e6 vbe.c:100
    mov bp, sp                                ; 89 e5                       ; 0xc35e7
    push bx                                   ; 53                          ; 0xc35e9
    push dx                                   ; 52                          ; 0xc35ea
    mov bx, ax                                ; 89 c3                       ; 0xc35eb
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc35ed vbe.c:105
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc35f0
    call 00590h                               ; e8 9a cf                    ; 0xc35f3
    mov ax, bx                                ; 89 d8                       ; 0xc35f6 vbe.c:106
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc35f8
    call 00590h                               ; e8 92 cf                    ; 0xc35fb
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc35fe vbe.c:107
    pop dx                                    ; 5a                          ; 0xc3601
    pop bx                                    ; 5b                          ; 0xc3602
    pop bp                                    ; 5d                          ; 0xc3603
    retn                                      ; c3                          ; 0xc3604
  ; disGetNextSymbol 0xc3605 LB 0x6da -> off=0x0 cb=000000000000001f uValue=00000000000c3605 'dispi_set_yres'
dispi_set_yres:                              ; 0xc3605 LB 0x1f
    push bp                                   ; 55                          ; 0xc3605 vbe.c:109
    mov bp, sp                                ; 89 e5                       ; 0xc3606
    push bx                                   ; 53                          ; 0xc3608
    push dx                                   ; 52                          ; 0xc3609
    mov bx, ax                                ; 89 c3                       ; 0xc360a
    mov ax, strict word 00002h                ; b8 02 00                    ; 0xc360c vbe.c:114
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc360f
    call 00590h                               ; e8 7b cf                    ; 0xc3612
    mov ax, bx                                ; 89 d8                       ; 0xc3615 vbe.c:115
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3617
    call 00590h                               ; e8 73 cf                    ; 0xc361a
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc361d vbe.c:116
    pop dx                                    ; 5a                          ; 0xc3620
    pop bx                                    ; 5b                          ; 0xc3621
    pop bp                                    ; 5d                          ; 0xc3622
    retn                                      ; c3                          ; 0xc3623
  ; disGetNextSymbol 0xc3624 LB 0x6bb -> off=0x0 cb=0000000000000019 uValue=00000000000c3624 'dispi_get_yres'
dispi_get_yres:                              ; 0xc3624 LB 0x19
    push bp                                   ; 55                          ; 0xc3624 vbe.c:118
    mov bp, sp                                ; 89 e5                       ; 0xc3625
    push dx                                   ; 52                          ; 0xc3627
    mov ax, strict word 00002h                ; b8 02 00                    ; 0xc3628 vbe.c:120
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc362b
    call 00590h                               ; e8 5f cf                    ; 0xc362e
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3631 vbe.c:121
    call 00597h                               ; e8 60 cf                    ; 0xc3634
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3637 vbe.c:122
    pop dx                                    ; 5a                          ; 0xc363a
    pop bp                                    ; 5d                          ; 0xc363b
    retn                                      ; c3                          ; 0xc363c
  ; disGetNextSymbol 0xc363d LB 0x6a2 -> off=0x0 cb=000000000000001f uValue=00000000000c363d 'dispi_set_bpp'
dispi_set_bpp:                               ; 0xc363d LB 0x1f
    push bp                                   ; 55                          ; 0xc363d vbe.c:124
    mov bp, sp                                ; 89 e5                       ; 0xc363e
    push bx                                   ; 53                          ; 0xc3640
    push dx                                   ; 52                          ; 0xc3641
    mov bx, ax                                ; 89 c3                       ; 0xc3642
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc3644 vbe.c:129
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3647
    call 00590h                               ; e8 43 cf                    ; 0xc364a
    mov ax, bx                                ; 89 d8                       ; 0xc364d vbe.c:130
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc364f
    call 00590h                               ; e8 3b cf                    ; 0xc3652
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3655 vbe.c:131
    pop dx                                    ; 5a                          ; 0xc3658
    pop bx                                    ; 5b                          ; 0xc3659
    pop bp                                    ; 5d                          ; 0xc365a
    retn                                      ; c3                          ; 0xc365b
  ; disGetNextSymbol 0xc365c LB 0x683 -> off=0x0 cb=0000000000000019 uValue=00000000000c365c 'dispi_get_bpp'
dispi_get_bpp:                               ; 0xc365c LB 0x19
    push bp                                   ; 55                          ; 0xc365c vbe.c:133
    mov bp, sp                                ; 89 e5                       ; 0xc365d
    push dx                                   ; 52                          ; 0xc365f
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc3660 vbe.c:135
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3663
    call 00590h                               ; e8 27 cf                    ; 0xc3666
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3669 vbe.c:136
    call 00597h                               ; e8 28 cf                    ; 0xc366c
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc366f vbe.c:137
    pop dx                                    ; 5a                          ; 0xc3672
    pop bp                                    ; 5d                          ; 0xc3673
    retn                                      ; c3                          ; 0xc3674
  ; disGetNextSymbol 0xc3675 LB 0x66a -> off=0x0 cb=000000000000001f uValue=00000000000c3675 'dispi_set_virt_width'
dispi_set_virt_width:                        ; 0xc3675 LB 0x1f
    push bp                                   ; 55                          ; 0xc3675 vbe.c:139
    mov bp, sp                                ; 89 e5                       ; 0xc3676
    push bx                                   ; 53                          ; 0xc3678
    push dx                                   ; 52                          ; 0xc3679
    mov bx, ax                                ; 89 c3                       ; 0xc367a
    mov ax, strict word 00006h                ; b8 06 00                    ; 0xc367c vbe.c:144
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc367f
    call 00590h                               ; e8 0b cf                    ; 0xc3682
    mov ax, bx                                ; 89 d8                       ; 0xc3685 vbe.c:145
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3687
    call 00590h                               ; e8 03 cf                    ; 0xc368a
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc368d vbe.c:146
    pop dx                                    ; 5a                          ; 0xc3690
    pop bx                                    ; 5b                          ; 0xc3691
    pop bp                                    ; 5d                          ; 0xc3692
    retn                                      ; c3                          ; 0xc3693
  ; disGetNextSymbol 0xc3694 LB 0x64b -> off=0x0 cb=0000000000000019 uValue=00000000000c3694 'dispi_get_virt_width'
dispi_get_virt_width:                        ; 0xc3694 LB 0x19
    push bp                                   ; 55                          ; 0xc3694 vbe.c:148
    mov bp, sp                                ; 89 e5                       ; 0xc3695
    push dx                                   ; 52                          ; 0xc3697
    mov ax, strict word 00006h                ; b8 06 00                    ; 0xc3698 vbe.c:150
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc369b
    call 00590h                               ; e8 ef ce                    ; 0xc369e
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc36a1 vbe.c:151
    call 00597h                               ; e8 f0 ce                    ; 0xc36a4
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc36a7 vbe.c:152
    pop dx                                    ; 5a                          ; 0xc36aa
    pop bp                                    ; 5d                          ; 0xc36ab
    retn                                      ; c3                          ; 0xc36ac
  ; disGetNextSymbol 0xc36ad LB 0x632 -> off=0x0 cb=0000000000000019 uValue=00000000000c36ad 'dispi_get_virt_height'
dispi_get_virt_height:                       ; 0xc36ad LB 0x19
    push bp                                   ; 55                          ; 0xc36ad vbe.c:154
    mov bp, sp                                ; 89 e5                       ; 0xc36ae
    push dx                                   ; 52                          ; 0xc36b0
    mov ax, strict word 00007h                ; b8 07 00                    ; 0xc36b1 vbe.c:156
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc36b4
    call 00590h                               ; e8 d6 ce                    ; 0xc36b7
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc36ba vbe.c:157
    call 00597h                               ; e8 d7 ce                    ; 0xc36bd
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc36c0 vbe.c:158
    pop dx                                    ; 5a                          ; 0xc36c3
    pop bp                                    ; 5d                          ; 0xc36c4
    retn                                      ; c3                          ; 0xc36c5
  ; disGetNextSymbol 0xc36c6 LB 0x619 -> off=0x0 cb=0000000000000012 uValue=00000000000c36c6 'in_word'
in_word:                                     ; 0xc36c6 LB 0x12
    push bp                                   ; 55                          ; 0xc36c6 vbe.c:160
    mov bp, sp                                ; 89 e5                       ; 0xc36c7
    push bx                                   ; 53                          ; 0xc36c9
    mov bx, ax                                ; 89 c3                       ; 0xc36ca
    mov ax, dx                                ; 89 d0                       ; 0xc36cc
    mov dx, bx                                ; 89 da                       ; 0xc36ce vbe.c:162
    out DX, ax                                ; ef                          ; 0xc36d0
    in ax, DX                                 ; ed                          ; 0xc36d1 vbe.c:163
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc36d2 vbe.c:164
    pop bx                                    ; 5b                          ; 0xc36d5
    pop bp                                    ; 5d                          ; 0xc36d6
    retn                                      ; c3                          ; 0xc36d7
  ; disGetNextSymbol 0xc36d8 LB 0x607 -> off=0x0 cb=0000000000000014 uValue=00000000000c36d8 'in_byte'
in_byte:                                     ; 0xc36d8 LB 0x14
    push bp                                   ; 55                          ; 0xc36d8 vbe.c:166
    mov bp, sp                                ; 89 e5                       ; 0xc36d9
    push bx                                   ; 53                          ; 0xc36db
    mov bx, ax                                ; 89 c3                       ; 0xc36dc
    mov ax, dx                                ; 89 d0                       ; 0xc36de
    mov dx, bx                                ; 89 da                       ; 0xc36e0 vbe.c:168
    out DX, ax                                ; ef                          ; 0xc36e2
    in AL, DX                                 ; ec                          ; 0xc36e3 vbe.c:169
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc36e4
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc36e6 vbe.c:170
    pop bx                                    ; 5b                          ; 0xc36e9
    pop bp                                    ; 5d                          ; 0xc36ea
    retn                                      ; c3                          ; 0xc36eb
  ; disGetNextSymbol 0xc36ec LB 0x5f3 -> off=0x0 cb=0000000000000014 uValue=00000000000c36ec 'dispi_get_id'
dispi_get_id:                                ; 0xc36ec LB 0x14
    push bp                                   ; 55                          ; 0xc36ec vbe.c:173
    mov bp, sp                                ; 89 e5                       ; 0xc36ed
    push dx                                   ; 52                          ; 0xc36ef
    xor ax, ax                                ; 31 c0                       ; 0xc36f0 vbe.c:175
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc36f2
    out DX, ax                                ; ef                          ; 0xc36f5
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc36f6 vbe.c:176
    in ax, DX                                 ; ed                          ; 0xc36f9
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc36fa vbe.c:177
    pop dx                                    ; 5a                          ; 0xc36fd
    pop bp                                    ; 5d                          ; 0xc36fe
    retn                                      ; c3                          ; 0xc36ff
  ; disGetNextSymbol 0xc3700 LB 0x5df -> off=0x0 cb=000000000000001a uValue=00000000000c3700 'dispi_set_id'
dispi_set_id:                                ; 0xc3700 LB 0x1a
    push bp                                   ; 55                          ; 0xc3700 vbe.c:179
    mov bp, sp                                ; 89 e5                       ; 0xc3701
    push bx                                   ; 53                          ; 0xc3703
    push dx                                   ; 52                          ; 0xc3704
    mov bx, ax                                ; 89 c3                       ; 0xc3705
    xor ax, ax                                ; 31 c0                       ; 0xc3707 vbe.c:181
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3709
    out DX, ax                                ; ef                          ; 0xc370c
    mov ax, bx                                ; 89 d8                       ; 0xc370d vbe.c:182
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc370f
    out DX, ax                                ; ef                          ; 0xc3712
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3713 vbe.c:183
    pop dx                                    ; 5a                          ; 0xc3716
    pop bx                                    ; 5b                          ; 0xc3717
    pop bp                                    ; 5d                          ; 0xc3718
    retn                                      ; c3                          ; 0xc3719
  ; disGetNextSymbol 0xc371a LB 0x5c5 -> off=0x0 cb=000000000000002c uValue=00000000000c371a 'vbe_init'
vbe_init:                                    ; 0xc371a LB 0x2c
    push bp                                   ; 55                          ; 0xc371a vbe.c:188
    mov bp, sp                                ; 89 e5                       ; 0xc371b
    push bx                                   ; 53                          ; 0xc371d
    push dx                                   ; 52                          ; 0xc371e
    mov ax, 0b0c0h                            ; b8 c0 b0                    ; 0xc371f vbe.c:190
    call 03700h                               ; e8 db ff                    ; 0xc3722
    call 036ech                               ; e8 c4 ff                    ; 0xc3725 vbe.c:191
    cmp ax, 0b0c0h                            ; 3d c0 b0                    ; 0xc3728
    jne short 0373fh                          ; 75 12                       ; 0xc372b
    mov bx, strict word 00001h                ; bb 01 00                    ; 0xc372d vbe.c:193
    mov dx, 000b9h                            ; ba b9 00                    ; 0xc3730
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3733
    call 031cch                               ; e8 93 fa                    ; 0xc3736
    mov ax, 0b0c4h                            ; b8 c4 b0                    ; 0xc3739 vbe.c:194
    call 03700h                               ; e8 c1 ff                    ; 0xc373c
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc373f vbe.c:199
    pop dx                                    ; 5a                          ; 0xc3742
    pop bx                                    ; 5b                          ; 0xc3743
    pop bp                                    ; 5d                          ; 0xc3744
    retn                                      ; c3                          ; 0xc3745
  ; disGetNextSymbol 0xc3746 LB 0x599 -> off=0x0 cb=0000000000000055 uValue=00000000000c3746 'mode_info_find_mode'
mode_info_find_mode:                         ; 0xc3746 LB 0x55
    push bp                                   ; 55                          ; 0xc3746 vbe.c:202
    mov bp, sp                                ; 89 e5                       ; 0xc3747
    push bx                                   ; 53                          ; 0xc3749
    push cx                                   ; 51                          ; 0xc374a
    push si                                   ; 56                          ; 0xc374b
    push di                                   ; 57                          ; 0xc374c
    mov di, ax                                ; 89 c7                       ; 0xc374d
    mov si, dx                                ; 89 d6                       ; 0xc374f
    xor dx, dx                                ; 31 d2                       ; 0xc3751 vbe.c:208
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3753
    call 036c6h                               ; e8 6d ff                    ; 0xc3756
    cmp ax, 077cch                            ; 3d cc 77                    ; 0xc3759 vbe.c:209
    jne short 03790h                          ; 75 32                       ; 0xc375c
    mov bx, strict word 00004h                ; bb 04 00                    ; 0xc375e vbe.c:213
    mov dx, bx                                ; 89 da                       ; 0xc3761 vbe.c:218
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3763
    call 036c6h                               ; e8 5d ff                    ; 0xc3766
    mov cx, ax                                ; 89 c1                       ; 0xc3769
    cmp cx, strict byte 0ffffh                ; 83 f9 ff                    ; 0xc376b vbe.c:219
    je short 03790h                           ; 74 20                       ; 0xc376e
    lea dx, [bx+002h]                         ; 8d 57 02                    ; 0xc3770 vbe.c:221
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3773
    call 036c6h                               ; e8 4d ff                    ; 0xc3776
    lea dx, [bx+044h]                         ; 8d 57 44                    ; 0xc3779
    cmp cx, di                                ; 39 f9                       ; 0xc377c vbe.c:223
    jne short 0378ch                          ; 75 0c                       ; 0xc377e
    test si, si                               ; 85 f6                       ; 0xc3780 vbe.c:225
    jne short 03788h                          ; 75 04                       ; 0xc3782
    mov ax, bx                                ; 89 d8                       ; 0xc3784 vbe.c:226
    jmp short 03792h                          ; eb 0a                       ; 0xc3786
    test AL, strict byte 080h                 ; a8 80                       ; 0xc3788 vbe.c:227
    jne short 03784h                          ; 75 f8                       ; 0xc378a
    mov bx, dx                                ; 89 d3                       ; 0xc378c vbe.c:230
    jmp short 03763h                          ; eb d3                       ; 0xc378e vbe.c:235
    xor ax, ax                                ; 31 c0                       ; 0xc3790 vbe.c:238
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc3792 vbe.c:239
    pop di                                    ; 5f                          ; 0xc3795
    pop si                                    ; 5e                          ; 0xc3796
    pop cx                                    ; 59                          ; 0xc3797
    pop bx                                    ; 5b                          ; 0xc3798
    pop bp                                    ; 5d                          ; 0xc3799
    retn                                      ; c3                          ; 0xc379a
  ; disGetNextSymbol 0xc379b LB 0x544 -> off=0x0 cb=000000000000012f uValue=00000000000c379b 'vbe_biosfn_return_controller_information'
vbe_biosfn_return_controller_information: ; 0xc379b LB 0x12f
    push bp                                   ; 55                          ; 0xc379b vbe.c:270
    mov bp, sp                                ; 89 e5                       ; 0xc379c
    push cx                                   ; 51                          ; 0xc379e
    push si                                   ; 56                          ; 0xc379f
    push di                                   ; 57                          ; 0xc37a0
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc37a1
    mov si, ax                                ; 89 c6                       ; 0xc37a4
    mov di, dx                                ; 89 d7                       ; 0xc37a6
    mov word [bp-00ah], bx                    ; 89 5e f6                    ; 0xc37a8
    mov word [bp-00ch], strict word 00022h    ; c7 46 f4 22 00              ; 0xc37ab vbe.c:275
    call 005dah                               ; e8 27 ce                    ; 0xc37b0 vbe.c:278
    mov word [bp-010h], ax                    ; 89 46 f0                    ; 0xc37b3
    mov bx, word [bp-00ah]                    ; 8b 5e f6                    ; 0xc37b6 vbe.c:281
    mov word [bp-008h], di                    ; 89 7e f8                    ; 0xc37b9
    xor dx, dx                                ; 31 d2                       ; 0xc37bc vbe.c:284
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc37be
    call 036c6h                               ; e8 02 ff                    ; 0xc37c1
    cmp ax, 077cch                            ; 3d cc 77                    ; 0xc37c4 vbe.c:285
    je short 037d3h                           ; 74 0a                       ; 0xc37c7
    push SS                                   ; 16                          ; 0xc37c9 vbe.c:287
    pop ES                                    ; 07                          ; 0xc37ca
    mov word [es:si], 00100h                  ; 26 c7 04 00 01              ; 0xc37cb
    jmp near 038c2h                           ; e9 ef 00                    ; 0xc37d0 vbe.c:291
    mov cx, strict word 00004h                ; b9 04 00                    ; 0xc37d3 vbe.c:293
    mov word [bp-00eh], strict word 00000h    ; c7 46 f2 00 00              ; 0xc37d6 vbe.c:300
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc37db vbe.c:308
    cmp word [es:bx+002h], 03245h             ; 26 81 7f 02 45 32           ; 0xc37de
    jne short 037edh                          ; 75 07                       ; 0xc37e4
    cmp word [es:bx], 04256h                  ; 26 81 3f 56 42              ; 0xc37e6
    je short 037fch                           ; 74 0f                       ; 0xc37eb
    cmp word [es:bx+002h], 04153h             ; 26 81 7f 02 53 41           ; 0xc37ed
    jne short 03801h                          ; 75 0c                       ; 0xc37f3
    cmp word [es:bx], 04556h                  ; 26 81 3f 56 45              ; 0xc37f5
    jne short 03801h                          ; 75 05                       ; 0xc37fa
    mov word [bp-00eh], strict word 00001h    ; c7 46 f2 01 00              ; 0xc37fc vbe.c:310
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc3801 vbe.c:318
    mov word [es:bx], 04556h                  ; 26 c7 07 56 45              ; 0xc3804
    mov word [es:bx+002h], 04153h             ; 26 c7 47 02 53 41           ; 0xc3809 vbe.c:320
    mov word [es:bx+004h], 00200h             ; 26 c7 47 04 00 02           ; 0xc380f vbe.c:324
    mov word [es:bx+006h], 07c6ch             ; 26 c7 47 06 6c 7c           ; 0xc3815 vbe.c:327
    mov [es:bx+008h], ds                      ; 26 8c 5f 08                 ; 0xc381b
    mov word [es:bx+00ah], strict word 00001h ; 26 c7 47 0a 01 00           ; 0xc381f vbe.c:330
    mov word [es:bx+00ch], strict word 00000h ; 26 c7 47 0c 00 00           ; 0xc3825 vbe.c:332
    mov word [es:bx+010h], di                 ; 26 89 7f 10                 ; 0xc382b vbe.c:336
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc382f vbe.c:337
    add ax, strict word 00022h                ; 05 22 00                    ; 0xc3832
    mov word [es:bx+00eh], ax                 ; 26 89 47 0e                 ; 0xc3835
    mov dx, strict word 0ffffh                ; ba ff ff                    ; 0xc3839 vbe.c:340
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc383c
    call 036c6h                               ; e8 84 fe                    ; 0xc383f
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc3842
    mov word [es:bx+012h], ax                 ; 26 89 47 12                 ; 0xc3845
    cmp word [bp-00eh], strict byte 00000h    ; 83 7e f2 00                 ; 0xc3849 vbe.c:342
    je short 03873h                           ; 74 24                       ; 0xc384d
    mov word [es:bx+014h], strict word 00003h ; 26 c7 47 14 03 00           ; 0xc384f vbe.c:345
    mov word [es:bx+016h], 07c81h             ; 26 c7 47 16 81 7c           ; 0xc3855 vbe.c:346
    mov [es:bx+018h], ds                      ; 26 8c 5f 18                 ; 0xc385b
    mov word [es:bx+01ah], 07c94h             ; 26 c7 47 1a 94 7c           ; 0xc385f vbe.c:347
    mov [es:bx+01ch], ds                      ; 26 8c 5f 1c                 ; 0xc3865
    mov word [es:bx+01eh], 07cb5h             ; 26 c7 47 1e b5 7c           ; 0xc3869 vbe.c:348
    mov [es:bx+020h], ds                      ; 26 8c 5f 20                 ; 0xc386f
    mov dx, cx                                ; 89 ca                       ; 0xc3873 vbe.c:355
    add dx, strict byte 0001bh                ; 83 c2 1b                    ; 0xc3875
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3878
    call 036d8h                               ; e8 5a fe                    ; 0xc387b
    xor ah, ah                                ; 30 e4                       ; 0xc387e vbe.c:356
    cmp ax, word [bp-010h]                    ; 3b 46 f0                    ; 0xc3880
    jnbe short 0389eh                         ; 77 19                       ; 0xc3883
    mov dx, cx                                ; 89 ca                       ; 0xc3885 vbe.c:358
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3887
    call 036c6h                               ; e8 39 fe                    ; 0xc388a
    mov bx, ax                                ; 89 c3                       ; 0xc388d
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc388f vbe.c:362
    add dx, word [bp-00ch]                    ; 03 56 f4                    ; 0xc3892
    mov ax, di                                ; 89 f8                       ; 0xc3895
    call 031e8h                               ; e8 4e f9                    ; 0xc3897
    add word [bp-00ch], strict byte 00002h    ; 83 46 f4 02                 ; 0xc389a vbe.c:364
    add cx, strict byte 00044h                ; 83 c1 44                    ; 0xc389e vbe.c:366
    mov dx, cx                                ; 89 ca                       ; 0xc38a1 vbe.c:367
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc38a3
    call 036c6h                               ; e8 1d fe                    ; 0xc38a6
    mov bx, ax                                ; 89 c3                       ; 0xc38a9
    cmp ax, strict word 0ffffh                ; 3d ff ff                    ; 0xc38ab vbe.c:368
    jne short 03873h                          ; 75 c3                       ; 0xc38ae
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc38b0 vbe.c:371
    add dx, word [bp-00ch]                    ; 03 56 f4                    ; 0xc38b3
    mov ax, di                                ; 89 f8                       ; 0xc38b6
    call 031e8h                               ; e8 2d f9                    ; 0xc38b8
    push SS                                   ; 16                          ; 0xc38bb vbe.c:372
    pop ES                                    ; 07                          ; 0xc38bc
    mov word [es:si], strict word 0004fh      ; 26 c7 04 4f 00              ; 0xc38bd
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc38c2 vbe.c:373
    pop di                                    ; 5f                          ; 0xc38c5
    pop si                                    ; 5e                          ; 0xc38c6
    pop cx                                    ; 59                          ; 0xc38c7
    pop bp                                    ; 5d                          ; 0xc38c8
    retn                                      ; c3                          ; 0xc38c9
  ; disGetNextSymbol 0xc38ca LB 0x415 -> off=0x0 cb=00000000000000bd uValue=00000000000c38ca 'vbe_biosfn_return_mode_information'
vbe_biosfn_return_mode_information:          ; 0xc38ca LB 0xbd
    push bp                                   ; 55                          ; 0xc38ca vbe.c:385
    mov bp, sp                                ; 89 e5                       ; 0xc38cb
    push si                                   ; 56                          ; 0xc38cd
    push di                                   ; 57                          ; 0xc38ce
    push ax                                   ; 50                          ; 0xc38cf
    push ax                                   ; 50                          ; 0xc38d0
    push ax                                   ; 50                          ; 0xc38d1
    mov ax, dx                                ; 89 d0                       ; 0xc38d2
    mov si, bx                                ; 89 de                       ; 0xc38d4
    mov word [bp-006h], cx                    ; 89 4e fa                    ; 0xc38d6
    test dh, 040h                             ; f6 c6 40                    ; 0xc38d9 vbe.c:396
    je short 038e3h                           ; 74 05                       ; 0xc38dc
    mov dx, strict word 00001h                ; ba 01 00                    ; 0xc38de
    jmp short 038e5h                          ; eb 02                       ; 0xc38e1
    xor dx, dx                                ; 31 d2                       ; 0xc38e3
    and ah, 001h                              ; 80 e4 01                    ; 0xc38e5 vbe.c:397
    call 03746h                               ; e8 5b fe                    ; 0xc38e8 vbe.c:399
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc38eb
    test ax, ax                               ; 85 c0                       ; 0xc38ee vbe.c:401
    je short 03928h                           ; 74 36                       ; 0xc38f0
    mov cx, 00100h                            ; b9 00 01                    ; 0xc38f2 vbe.c:406
    xor ax, ax                                ; 31 c0                       ; 0xc38f5
    mov di, word [bp-006h]                    ; 8b 7e fa                    ; 0xc38f7
    mov es, si                                ; 8e c6                       ; 0xc38fa
    cld                                       ; fc                          ; 0xc38fc
    jcxz 03901h                               ; e3 02                       ; 0xc38fd
    rep stosb                                 ; f3 aa                       ; 0xc38ff
    xor cx, cx                                ; 31 c9                       ; 0xc3901 vbe.c:407
    jmp short 0390ah                          ; eb 05                       ; 0xc3903
    cmp cx, strict byte 00042h                ; 83 f9 42                    ; 0xc3905
    jnc short 0392ah                          ; 73 20                       ; 0xc3908
    mov dx, word [bp-008h]                    ; 8b 56 f8                    ; 0xc390a vbe.c:410
    inc dx                                    ; 42                          ; 0xc390d
    inc dx                                    ; 42                          ; 0xc390e
    add dx, cx                                ; 01 ca                       ; 0xc390f
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3911
    call 036d8h                               ; e8 c1 fd                    ; 0xc3914
    mov bl, al                                ; 88 c3                       ; 0xc3917 vbe.c:411
    xor bh, bh                                ; 30 ff                       ; 0xc3919
    mov dx, word [bp-006h]                    ; 8b 56 fa                    ; 0xc391b
    add dx, cx                                ; 01 ca                       ; 0xc391e
    mov ax, si                                ; 89 f0                       ; 0xc3920
    call 031cch                               ; e8 a7 f8                    ; 0xc3922
    inc cx                                    ; 41                          ; 0xc3925 vbe.c:412
    jmp short 03905h                          ; eb dd                       ; 0xc3926
    jmp short 03975h                          ; eb 4b                       ; 0xc3928
    mov dx, word [bp-006h]                    ; 8b 56 fa                    ; 0xc392a vbe.c:413
    inc dx                                    ; 42                          ; 0xc392d
    inc dx                                    ; 42                          ; 0xc392e
    mov ax, si                                ; 89 f0                       ; 0xc392f
    call 031beh                               ; e8 8a f8                    ; 0xc3931
    test AL, strict byte 001h                 ; a8 01                       ; 0xc3934 vbe.c:414
    je short 03954h                           ; 74 1c                       ; 0xc3936
    mov dx, word [bp-006h]                    ; 8b 56 fa                    ; 0xc3938 vbe.c:415
    add dx, strict byte 0000ch                ; 83 c2 0c                    ; 0xc393b
    mov bx, 0064ch                            ; bb 4c 06                    ; 0xc393e
    mov ax, si                                ; 89 f0                       ; 0xc3941
    call 031e8h                               ; e8 a2 f8                    ; 0xc3943
    mov dx, word [bp-006h]                    ; 8b 56 fa                    ; 0xc3946 vbe.c:417
    add dx, strict byte 0000eh                ; 83 c2 0e                    ; 0xc3949
    mov bx, 0c000h                            ; bb 00 c0                    ; 0xc394c
    mov ax, si                                ; 89 f0                       ; 0xc394f
    call 031e8h                               ; e8 94 f8                    ; 0xc3951
    mov ax, strict word 0000bh                ; b8 0b 00                    ; 0xc3954 vbe.c:420
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3957
    call 00590h                               ; e8 33 cc                    ; 0xc395a
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc395d vbe.c:421
    call 00597h                               ; e8 34 cc                    ; 0xc3960
    mov dx, word [bp-006h]                    ; 8b 56 fa                    ; 0xc3963
    add dx, strict byte 0002ah                ; 83 c2 2a                    ; 0xc3966
    mov bx, ax                                ; 89 c3                       ; 0xc3969
    mov ax, si                                ; 89 f0                       ; 0xc396b
    call 031e8h                               ; e8 78 f8                    ; 0xc396d
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc3970 vbe.c:423
    jmp short 03978h                          ; eb 03                       ; 0xc3973 vbe.c:424
    mov ax, 00100h                            ; b8 00 01                    ; 0xc3975 vbe.c:428
    push SS                                   ; 16                          ; 0xc3978 vbe.c:431
    pop ES                                    ; 07                          ; 0xc3979
    mov bx, word [bp-00ah]                    ; 8b 5e f6                    ; 0xc397a
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc397d
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3980 vbe.c:432
    pop di                                    ; 5f                          ; 0xc3983
    pop si                                    ; 5e                          ; 0xc3984
    pop bp                                    ; 5d                          ; 0xc3985
    retn                                      ; c3                          ; 0xc3986
  ; disGetNextSymbol 0xc3987 LB 0x358 -> off=0x0 cb=00000000000000eb uValue=00000000000c3987 'vbe_biosfn_set_mode'
vbe_biosfn_set_mode:                         ; 0xc3987 LB 0xeb
    push bp                                   ; 55                          ; 0xc3987 vbe.c:444
    mov bp, sp                                ; 89 e5                       ; 0xc3988
    push si                                   ; 56                          ; 0xc398a
    push di                                   ; 57                          ; 0xc398b
    sub sp, strict byte 00006h                ; 83 ec 06                    ; 0xc398c
    mov si, ax                                ; 89 c6                       ; 0xc398f
    mov word [bp-00ah], dx                    ; 89 56 f6                    ; 0xc3991
    test byte [bp-009h], 040h                 ; f6 46 f7 40                 ; 0xc3994 vbe.c:452
    je short 0399fh                           ; 74 05                       ; 0xc3998
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc399a
    jmp short 039a1h                          ; eb 02                       ; 0xc399d
    xor ax, ax                                ; 31 c0                       ; 0xc399f
    mov dx, ax                                ; 89 c2                       ; 0xc39a1
    test ax, ax                               ; 85 c0                       ; 0xc39a3 vbe.c:453
    je short 039aah                           ; 74 03                       ; 0xc39a5
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc39a7
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc39aa
    test byte [bp-009h], 080h                 ; f6 46 f7 80                 ; 0xc39ad vbe.c:454
    je short 039b8h                           ; 74 05                       ; 0xc39b1
    mov ax, 00080h                            ; b8 80 00                    ; 0xc39b3
    jmp short 039bah                          ; eb 02                       ; 0xc39b6
    xor ax, ax                                ; 31 c0                       ; 0xc39b8
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc39ba
    and byte [bp-009h], 001h                  ; 80 66 f7 01                 ; 0xc39bd vbe.c:456
    cmp word [bp-00ah], 00100h                ; 81 7e f6 00 01              ; 0xc39c1 vbe.c:459
    jnc short 039dbh                          ; 73 13                       ; 0xc39c6
    xor ax, ax                                ; 31 c0                       ; 0xc39c8 vbe.c:463
    call 00600h                               ; e8 33 cc                    ; 0xc39ca
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc39cd vbe.c:467
    xor ah, ah                                ; 30 e4                       ; 0xc39d0
    call 01019h                               ; e8 44 d6                    ; 0xc39d2
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc39d5 vbe.c:468
    jmp near 03a68h                           ; e9 8d 00                    ; 0xc39d8 vbe.c:469
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc39db vbe.c:472
    call 03746h                               ; e8 65 fd                    ; 0xc39de
    mov bx, ax                                ; 89 c3                       ; 0xc39e1
    test ax, ax                               ; 85 c0                       ; 0xc39e3 vbe.c:474
    jne short 039eah                          ; 75 03                       ; 0xc39e5
    jmp near 03a65h                           ; e9 7b 00                    ; 0xc39e7
    lea dx, [bx+014h]                         ; 8d 57 14                    ; 0xc39ea vbe.c:479
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc39ed
    call 036c6h                               ; e8 d3 fc                    ; 0xc39f0
    mov cx, ax                                ; 89 c1                       ; 0xc39f3
    lea dx, [bx+016h]                         ; 8d 57 16                    ; 0xc39f5 vbe.c:480
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc39f8
    call 036c6h                               ; e8 c8 fc                    ; 0xc39fb
    mov di, ax                                ; 89 c7                       ; 0xc39fe
    lea dx, [bx+01bh]                         ; 8d 57 1b                    ; 0xc3a00 vbe.c:481
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3a03
    call 036d8h                               ; e8 cf fc                    ; 0xc3a06
    mov bl, al                                ; 88 c3                       ; 0xc3a09
    mov dl, al                                ; 88 c2                       ; 0xc3a0b
    xor ax, ax                                ; 31 c0                       ; 0xc3a0d vbe.c:489
    call 00600h                               ; e8 ee cb                    ; 0xc3a0f
    cmp bl, 004h                              ; 80 fb 04                    ; 0xc3a12 vbe.c:491
    jne short 03a1dh                          ; 75 06                       ; 0xc3a15
    mov ax, strict word 0006ah                ; b8 6a 00                    ; 0xc3a17 vbe.c:493
    call 01019h                               ; e8 fc d5                    ; 0xc3a1a
    mov al, dl                                ; 88 d0                       ; 0xc3a1d vbe.c:496
    xor ah, ah                                ; 30 e4                       ; 0xc3a1f
    call 0363dh                               ; e8 19 fc                    ; 0xc3a21
    mov ax, cx                                ; 89 c8                       ; 0xc3a24 vbe.c:497
    call 035e6h                               ; e8 bd fb                    ; 0xc3a26
    mov ax, di                                ; 89 f8                       ; 0xc3a29 vbe.c:498
    call 03605h                               ; e8 d7 fb                    ; 0xc3a2b
    xor ax, ax                                ; 31 c0                       ; 0xc3a2e vbe.c:499
    call 00626h                               ; e8 f3 cb                    ; 0xc3a30
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc3a33 vbe.c:500
    or AL, strict byte 001h                   ; 0c 01                       ; 0xc3a36
    xor ah, ah                                ; 30 e4                       ; 0xc3a38
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc3a3a
    or al, dl                                 ; 08 d0                       ; 0xc3a3d
    call 00600h                               ; e8 be cb                    ; 0xc3a3f
    call 006f8h                               ; e8 b3 cc                    ; 0xc3a42 vbe.c:501
    mov bx, word [bp-00ah]                    ; 8b 5e f6                    ; 0xc3a45 vbe.c:503
    mov dx, 000bah                            ; ba ba 00                    ; 0xc3a48
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3a4b
    call 031e8h                               ; e8 97 f7                    ; 0xc3a4e
    mov bl, byte [bp-008h]                    ; 8a 5e f8                    ; 0xc3a51 vbe.c:504
    or bl, 060h                               ; 80 cb 60                    ; 0xc3a54
    xor bh, bh                                ; 30 ff                       ; 0xc3a57
    mov dx, 00087h                            ; ba 87 00                    ; 0xc3a59
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3a5c
    call 031cch                               ; e8 6a f7                    ; 0xc3a5f
    jmp near 039d5h                           ; e9 70 ff                    ; 0xc3a62
    mov ax, 00100h                            ; b8 00 01                    ; 0xc3a65 vbe.c:513
    mov word [ss:si], ax                      ; 36 89 04                    ; 0xc3a68 vbe.c:517
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3a6b vbe.c:518
    pop di                                    ; 5f                          ; 0xc3a6e
    pop si                                    ; 5e                          ; 0xc3a6f
    pop bp                                    ; 5d                          ; 0xc3a70
    retn                                      ; c3                          ; 0xc3a71
  ; disGetNextSymbol 0xc3a72 LB 0x26d -> off=0x0 cb=0000000000000008 uValue=00000000000c3a72 'vbe_biosfn_read_video_state_size'
vbe_biosfn_read_video_state_size:            ; 0xc3a72 LB 0x8
    push bp                                   ; 55                          ; 0xc3a72 vbe.c:520
    mov bp, sp                                ; 89 e5                       ; 0xc3a73
    mov ax, strict word 00012h                ; b8 12 00                    ; 0xc3a75 vbe.c:523
    pop bp                                    ; 5d                          ; 0xc3a78
    retn                                      ; c3                          ; 0xc3a79
  ; disGetNextSymbol 0xc3a7a LB 0x265 -> off=0x0 cb=000000000000005b uValue=00000000000c3a7a 'vbe_biosfn_save_video_state'
vbe_biosfn_save_video_state:                 ; 0xc3a7a LB 0x5b
    push bp                                   ; 55                          ; 0xc3a7a vbe.c:525
    mov bp, sp                                ; 89 e5                       ; 0xc3a7b
    push bx                                   ; 53                          ; 0xc3a7d
    push cx                                   ; 51                          ; 0xc3a7e
    push si                                   ; 56                          ; 0xc3a7f
    push di                                   ; 57                          ; 0xc3a80
    push ax                                   ; 50                          ; 0xc3a81
    mov di, ax                                ; 89 c7                       ; 0xc3a82
    mov cx, dx                                ; 89 d1                       ; 0xc3a84
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc3a86 vbe.c:529
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3a89
    out DX, ax                                ; ef                          ; 0xc3a8c
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3a8d vbe.c:530
    in ax, DX                                 ; ed                          ; 0xc3a90
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc3a91
    mov bx, ax                                ; 89 c3                       ; 0xc3a94 vbe.c:531
    mov dx, cx                                ; 89 ca                       ; 0xc3a96
    mov ax, di                                ; 89 f8                       ; 0xc3a98
    call 031e8h                               ; e8 4b f7                    ; 0xc3a9a
    inc cx                                    ; 41                          ; 0xc3a9d vbe.c:532
    inc cx                                    ; 41                          ; 0xc3a9e
    test byte [bp-00ah], 001h                 ; f6 46 f6 01                 ; 0xc3a9f vbe.c:533
    je short 03acch                           ; 74 27                       ; 0xc3aa3
    mov si, strict word 00001h                ; be 01 00                    ; 0xc3aa5 vbe.c:535
    jmp short 03aafh                          ; eb 05                       ; 0xc3aa8
    cmp si, strict byte 00009h                ; 83 fe 09                    ; 0xc3aaa
    jnbe short 03acch                         ; 77 1d                       ; 0xc3aad
    cmp si, strict byte 00004h                ; 83 fe 04                    ; 0xc3aaf vbe.c:536
    je short 03ac9h                           ; 74 15                       ; 0xc3ab2
    mov ax, si                                ; 89 f0                       ; 0xc3ab4 vbe.c:537
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3ab6
    out DX, ax                                ; ef                          ; 0xc3ab9
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3aba vbe.c:538
    in ax, DX                                 ; ed                          ; 0xc3abd
    mov bx, ax                                ; 89 c3                       ; 0xc3abe
    mov dx, cx                                ; 89 ca                       ; 0xc3ac0
    mov ax, di                                ; 89 f8                       ; 0xc3ac2
    call 031e8h                               ; e8 21 f7                    ; 0xc3ac4
    inc cx                                    ; 41                          ; 0xc3ac7 vbe.c:539
    inc cx                                    ; 41                          ; 0xc3ac8
    inc si                                    ; 46                          ; 0xc3ac9 vbe.c:541
    jmp short 03aaah                          ; eb de                       ; 0xc3aca
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc3acc vbe.c:542
    pop di                                    ; 5f                          ; 0xc3acf
    pop si                                    ; 5e                          ; 0xc3ad0
    pop cx                                    ; 59                          ; 0xc3ad1
    pop bx                                    ; 5b                          ; 0xc3ad2
    pop bp                                    ; 5d                          ; 0xc3ad3
    retn                                      ; c3                          ; 0xc3ad4
  ; disGetNextSymbol 0xc3ad5 LB 0x20a -> off=0x0 cb=000000000000009b uValue=00000000000c3ad5 'vbe_biosfn_restore_video_state'
vbe_biosfn_restore_video_state:              ; 0xc3ad5 LB 0x9b
    push bp                                   ; 55                          ; 0xc3ad5 vbe.c:545
    mov bp, sp                                ; 89 e5                       ; 0xc3ad6
    push bx                                   ; 53                          ; 0xc3ad8
    push cx                                   ; 51                          ; 0xc3ad9
    push si                                   ; 56                          ; 0xc3ada
    push ax                                   ; 50                          ; 0xc3adb
    mov cx, ax                                ; 89 c1                       ; 0xc3adc
    mov bx, dx                                ; 89 d3                       ; 0xc3ade
    call 031dah                               ; e8 f7 f6                    ; 0xc3ae0 vbe.c:549
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc3ae3
    inc bx                                    ; 43                          ; 0xc3ae6 vbe.c:550
    inc bx                                    ; 43                          ; 0xc3ae7
    test byte [bp-008h], 001h                 ; f6 46 f8 01                 ; 0xc3ae8 vbe.c:552
    jne short 03afeh                          ; 75 10                       ; 0xc3aec
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc3aee vbe.c:553
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3af1
    out DX, ax                                ; ef                          ; 0xc3af4
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc3af5 vbe.c:554
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3af8
    out DX, ax                                ; ef                          ; 0xc3afb
    jmp short 03b68h                          ; eb 6a                       ; 0xc3afc vbe.c:555
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc3afe vbe.c:556
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3b01
    out DX, ax                                ; ef                          ; 0xc3b04
    mov dx, bx                                ; 89 da                       ; 0xc3b05 vbe.c:557
    mov ax, cx                                ; 89 c8                       ; 0xc3b07
    call 031dah                               ; e8 ce f6                    ; 0xc3b09
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3b0c
    out DX, ax                                ; ef                          ; 0xc3b0f
    inc bx                                    ; 43                          ; 0xc3b10 vbe.c:558
    inc bx                                    ; 43                          ; 0xc3b11
    mov ax, strict word 00002h                ; b8 02 00                    ; 0xc3b12
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3b15
    out DX, ax                                ; ef                          ; 0xc3b18
    mov dx, bx                                ; 89 da                       ; 0xc3b19 vbe.c:560
    mov ax, cx                                ; 89 c8                       ; 0xc3b1b
    call 031dah                               ; e8 ba f6                    ; 0xc3b1d
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3b20
    out DX, ax                                ; ef                          ; 0xc3b23
    inc bx                                    ; 43                          ; 0xc3b24 vbe.c:561
    inc bx                                    ; 43                          ; 0xc3b25
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc3b26
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3b29
    out DX, ax                                ; ef                          ; 0xc3b2c
    mov dx, bx                                ; 89 da                       ; 0xc3b2d vbe.c:563
    mov ax, cx                                ; 89 c8                       ; 0xc3b2f
    call 031dah                               ; e8 a6 f6                    ; 0xc3b31
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3b34
    out DX, ax                                ; ef                          ; 0xc3b37
    inc bx                                    ; 43                          ; 0xc3b38 vbe.c:564
    inc bx                                    ; 43                          ; 0xc3b39
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc3b3a
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3b3d
    out DX, ax                                ; ef                          ; 0xc3b40
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc3b41 vbe.c:566
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3b44
    out DX, ax                                ; ef                          ; 0xc3b47
    mov si, strict word 00005h                ; be 05 00                    ; 0xc3b48 vbe.c:568
    jmp short 03b52h                          ; eb 05                       ; 0xc3b4b
    cmp si, strict byte 00009h                ; 83 fe 09                    ; 0xc3b4d
    jnbe short 03b68h                         ; 77 16                       ; 0xc3b50
    mov ax, si                                ; 89 f0                       ; 0xc3b52 vbe.c:569
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3b54
    out DX, ax                                ; ef                          ; 0xc3b57
    mov dx, bx                                ; 89 da                       ; 0xc3b58 vbe.c:570
    mov ax, cx                                ; 89 c8                       ; 0xc3b5a
    call 031dah                               ; e8 7b f6                    ; 0xc3b5c
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3b5f
    out DX, ax                                ; ef                          ; 0xc3b62
    inc bx                                    ; 43                          ; 0xc3b63 vbe.c:571
    inc bx                                    ; 43                          ; 0xc3b64
    inc si                                    ; 46                          ; 0xc3b65 vbe.c:572
    jmp short 03b4dh                          ; eb e5                       ; 0xc3b66
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc3b68 vbe.c:574
    pop si                                    ; 5e                          ; 0xc3b6b
    pop cx                                    ; 59                          ; 0xc3b6c
    pop bx                                    ; 5b                          ; 0xc3b6d
    pop bp                                    ; 5d                          ; 0xc3b6e
    retn                                      ; c3                          ; 0xc3b6f
  ; disGetNextSymbol 0xc3b70 LB 0x16f -> off=0x0 cb=000000000000008d uValue=00000000000c3b70 'vbe_biosfn_save_restore_state'
vbe_biosfn_save_restore_state:               ; 0xc3b70 LB 0x8d
    push bp                                   ; 55                          ; 0xc3b70 vbe.c:590
    mov bp, sp                                ; 89 e5                       ; 0xc3b71
    push si                                   ; 56                          ; 0xc3b73
    push di                                   ; 57                          ; 0xc3b74
    push ax                                   ; 50                          ; 0xc3b75
    mov si, ax                                ; 89 c6                       ; 0xc3b76
    mov word [bp-006h], dx                    ; 89 56 fa                    ; 0xc3b78
    mov ax, bx                                ; 89 d8                       ; 0xc3b7b
    mov bx, word [bp+004h]                    ; 8b 5e 04                    ; 0xc3b7d
    mov di, strict word 0004fh                ; bf 4f 00                    ; 0xc3b80 vbe.c:595
    xor ah, ah                                ; 30 e4                       ; 0xc3b83 vbe.c:596
    cmp ax, strict word 00002h                ; 3d 02 00                    ; 0xc3b85
    je short 03bd0h                           ; 74 46                       ; 0xc3b88
    cmp ax, strict word 00001h                ; 3d 01 00                    ; 0xc3b8a
    je short 03bb4h                           ; 74 25                       ; 0xc3b8d
    test ax, ax                               ; 85 c0                       ; 0xc3b8f
    jne short 03bech                          ; 75 59                       ; 0xc3b91
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc3b93 vbe.c:598
    call 02aa7h                               ; e8 0e ef                    ; 0xc3b96
    mov cx, ax                                ; 89 c1                       ; 0xc3b99
    test byte [bp-006h], 008h                 ; f6 46 fa 08                 ; 0xc3b9b vbe.c:602
    je short 03ba6h                           ; 74 05                       ; 0xc3b9f
    call 03a72h                               ; e8 ce fe                    ; 0xc3ba1 vbe.c:603
    add ax, cx                                ; 01 c8                       ; 0xc3ba4
    add ax, strict word 0003fh                ; 05 3f 00                    ; 0xc3ba6 vbe.c:604
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc3ba9
    shr ax, CL                                ; d3 e8                       ; 0xc3bab
    push SS                                   ; 16                          ; 0xc3bad
    pop ES                                    ; 07                          ; 0xc3bae
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3baf
    jmp short 03befh                          ; eb 3b                       ; 0xc3bb2 vbe.c:605
    push SS                                   ; 16                          ; 0xc3bb4 vbe.c:607
    pop ES                                    ; 07                          ; 0xc3bb5
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc3bb6
    mov dx, cx                                ; 89 ca                       ; 0xc3bb9 vbe.c:608
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc3bbb
    call 02adch                               ; e8 1b ef                    ; 0xc3bbe
    test byte [bp-006h], 008h                 ; f6 46 fa 08                 ; 0xc3bc1 vbe.c:612
    je short 03befh                           ; 74 28                       ; 0xc3bc5
    mov dx, ax                                ; 89 c2                       ; 0xc3bc7 vbe.c:613
    mov ax, cx                                ; 89 c8                       ; 0xc3bc9
    call 03a7ah                               ; e8 ac fe                    ; 0xc3bcb
    jmp short 03befh                          ; eb 1f                       ; 0xc3bce vbe.c:614
    push SS                                   ; 16                          ; 0xc3bd0 vbe.c:616
    pop ES                                    ; 07                          ; 0xc3bd1
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc3bd2
    mov dx, cx                                ; 89 ca                       ; 0xc3bd5 vbe.c:617
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc3bd7
    call 02e5dh                               ; e8 80 f2                    ; 0xc3bda
    test byte [bp-006h], 008h                 ; f6 46 fa 08                 ; 0xc3bdd vbe.c:621
    je short 03befh                           ; 74 0c                       ; 0xc3be1
    mov dx, ax                                ; 89 c2                       ; 0xc3be3 vbe.c:622
    mov ax, cx                                ; 89 c8                       ; 0xc3be5
    call 03ad5h                               ; e8 eb fe                    ; 0xc3be7
    jmp short 03befh                          ; eb 03                       ; 0xc3bea vbe.c:623
    mov di, 00100h                            ; bf 00 01                    ; 0xc3bec vbe.c:626
    push SS                                   ; 16                          ; 0xc3bef vbe.c:629
    pop ES                                    ; 07                          ; 0xc3bf0
    mov word [es:si], di                      ; 26 89 3c                    ; 0xc3bf1
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3bf4 vbe.c:630
    pop di                                    ; 5f                          ; 0xc3bf7
    pop si                                    ; 5e                          ; 0xc3bf8
    pop bp                                    ; 5d                          ; 0xc3bf9
    retn 00002h                               ; c2 02 00                    ; 0xc3bfa
  ; disGetNextSymbol 0xc3bfd LB 0xe2 -> off=0x0 cb=00000000000000e2 uValue=00000000000c3bfd 'vbe_biosfn_get_set_scanline_length'
vbe_biosfn_get_set_scanline_length:          ; 0xc3bfd LB 0xe2
    push bp                                   ; 55                          ; 0xc3bfd vbe.c:651
    mov bp, sp                                ; 89 e5                       ; 0xc3bfe
    push si                                   ; 56                          ; 0xc3c00
    push di                                   ; 57                          ; 0xc3c01
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc3c02
    push ax                                   ; 50                          ; 0xc3c05
    mov di, dx                                ; 89 d7                       ; 0xc3c06
    mov word [bp-006h], bx                    ; 89 5e fa                    ; 0xc3c08
    mov si, cx                                ; 89 ce                       ; 0xc3c0b
    call 0365ch                               ; e8 4c fa                    ; 0xc3c0d vbe.c:660
    cmp AL, strict byte 00fh                  ; 3c 0f                       ; 0xc3c10 vbe.c:661
    jne short 03c19h                          ; 75 05                       ; 0xc3c12
    mov cx, strict word 00010h                ; b9 10 00                    ; 0xc3c14
    jmp short 03c1dh                          ; eb 04                       ; 0xc3c17
    xor ah, ah                                ; 30 e4                       ; 0xc3c19
    mov cx, ax                                ; 89 c1                       ; 0xc3c1b
    mov ch, cl                                ; 88 cd                       ; 0xc3c1d
    call 03694h                               ; e8 72 fa                    ; 0xc3c1f vbe.c:662
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc3c22
    mov word [bp-00ch], strict word 0004fh    ; c7 46 f4 4f 00              ; 0xc3c25 vbe.c:663
    push SS                                   ; 16                          ; 0xc3c2a vbe.c:664
    pop ES                                    ; 07                          ; 0xc3c2b
    mov bx, word [bp-006h]                    ; 8b 5e fa                    ; 0xc3c2c
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc3c2f
    mov al, byte [es:di]                      ; 26 8a 05                    ; 0xc3c32 vbe.c:665
    cmp AL, strict byte 002h                  ; 3c 02                       ; 0xc3c35 vbe.c:669
    je short 03c44h                           ; 74 0b                       ; 0xc3c37
    cmp AL, strict byte 001h                  ; 3c 01                       ; 0xc3c39
    je short 03c6dh                           ; 74 30                       ; 0xc3c3b
    test al, al                               ; 84 c0                       ; 0xc3c3d
    je short 03c68h                           ; 74 27                       ; 0xc3c3f
    jmp near 03cc8h                           ; e9 84 00                    ; 0xc3c41
    cmp ch, 004h                              ; 80 fd 04                    ; 0xc3c44 vbe.c:671
    jne short 03c4fh                          ; 75 06                       ; 0xc3c47
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc3c49 vbe.c:672
    sal bx, CL                                ; d3 e3                       ; 0xc3c4b
    jmp short 03c68h                          ; eb 19                       ; 0xc3c4d vbe.c:673
    mov al, ch                                ; 88 e8                       ; 0xc3c4f vbe.c:674
    xor ah, ah                                ; 30 e4                       ; 0xc3c51
    cwd                                       ; 99                          ; 0xc3c53
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc3c54
    sal dx, CL                                ; d3 e2                       ; 0xc3c56
    db  01bh, 0c2h
    ; sbb ax, dx                                ; 1b c2                     ; 0xc3c58
    sar ax, CL                                ; d3 f8                       ; 0xc3c5a
    mov word [bp-00eh], ax                    ; 89 46 f2                    ; 0xc3c5c
    mov ax, bx                                ; 89 d8                       ; 0xc3c5f
    xor dx, dx                                ; 31 d2                       ; 0xc3c61
    div word [bp-00eh]                        ; f7 76 f2                    ; 0xc3c63
    mov bx, ax                                ; 89 c3                       ; 0xc3c66
    mov ax, bx                                ; 89 d8                       ; 0xc3c68 vbe.c:677
    call 03675h                               ; e8 08 fa                    ; 0xc3c6a
    call 03694h                               ; e8 24 fa                    ; 0xc3c6d vbe.c:680
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc3c70
    push SS                                   ; 16                          ; 0xc3c73 vbe.c:681
    pop ES                                    ; 07                          ; 0xc3c74
    mov bx, word [bp-006h]                    ; 8b 5e fa                    ; 0xc3c75
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3c78
    cmp ch, 004h                              ; 80 fd 04                    ; 0xc3c7b vbe.c:682
    jne short 03c88h                          ; 75 08                       ; 0xc3c7e
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc3c80 vbe.c:683
    mov bx, ax                                ; 89 c3                       ; 0xc3c82
    shr bx, CL                                ; d3 eb                       ; 0xc3c84
    jmp short 03c9eh                          ; eb 16                       ; 0xc3c86 vbe.c:684
    mov al, ch                                ; 88 e8                       ; 0xc3c88 vbe.c:685
    xor ah, ah                                ; 30 e4                       ; 0xc3c8a
    cwd                                       ; 99                          ; 0xc3c8c
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc3c8d
    sal dx, CL                                ; d3 e2                       ; 0xc3c8f
    db  01bh, 0c2h
    ; sbb ax, dx                                ; 1b c2                     ; 0xc3c91
    sar ax, CL                                ; d3 f8                       ; 0xc3c93
    mov bx, ax                                ; 89 c3                       ; 0xc3c95
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc3c97
    mul bx                                    ; f7 e3                       ; 0xc3c9a
    mov bx, ax                                ; 89 c3                       ; 0xc3c9c
    add bx, strict byte 00003h                ; 83 c3 03                    ; 0xc3c9e vbe.c:686
    and bl, 0fch                              ; 80 e3 fc                    ; 0xc3ca1
    push SS                                   ; 16                          ; 0xc3ca4 vbe.c:687
    pop ES                                    ; 07                          ; 0xc3ca5
    mov word [es:di], bx                      ; 26 89 1d                    ; 0xc3ca6
    call 036adh                               ; e8 01 fa                    ; 0xc3ca9 vbe.c:688
    push SS                                   ; 16                          ; 0xc3cac
    pop ES                                    ; 07                          ; 0xc3cad
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3cae
    call 03624h                               ; e8 70 f9                    ; 0xc3cb1 vbe.c:689
    push SS                                   ; 16                          ; 0xc3cb4
    pop ES                                    ; 07                          ; 0xc3cb5
    cmp ax, word [es:si]                      ; 26 3b 04                    ; 0xc3cb6
    jbe short 03ccdh                          ; 76 12                       ; 0xc3cb9
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc3cbb vbe.c:690
    call 03675h                               ; e8 b4 f9                    ; 0xc3cbe
    mov word [bp-00ch], 00200h                ; c7 46 f4 00 02              ; 0xc3cc1 vbe.c:691
    jmp short 03ccdh                          ; eb 05                       ; 0xc3cc6 vbe.c:693
    mov word [bp-00ch], 00100h                ; c7 46 f4 00 01              ; 0xc3cc8 vbe.c:696
    push SS                                   ; 16                          ; 0xc3ccd vbe.c:699
    pop ES                                    ; 07                          ; 0xc3cce
    mov ax, word [bp-00ch]                    ; 8b 46 f4                    ; 0xc3ccf
    mov bx, word [bp-010h]                    ; 8b 5e f0                    ; 0xc3cd2
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3cd5
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3cd8 vbe.c:700
    pop di                                    ; 5f                          ; 0xc3cdb
    pop si                                    ; 5e                          ; 0xc3cdc
    pop bp                                    ; 5d                          ; 0xc3cdd
    retn                                      ; c3                          ; 0xc3cde

  ; Padding 0x721 bytes at 0xc3cdf
  times 1825 db 0

section VBE32 progbits vstart=0x4400 align=1 ; size=0x115 class=CODE group=AUTO
  ; disGetNextSymbol 0xc4400 LB 0x115 -> off=0x0 cb=0000000000000114 uValue=00000000000c0000 'vesa_pm_start'
vesa_pm_start:                               ; 0xc4400 LB 0x114
    sbb byte [bx+si], al                      ; 18 00                       ; 0xc4400
    dec di                                    ; 4f                          ; 0xc4402
    add byte [bx+si], dl                      ; 00 10                       ; 0xc4403
    add word [bx+si], cx                      ; 01 08                       ; 0xc4405
    add dh, cl                                ; 00 ce                       ; 0xc4407
    add di, cx                                ; 01 cf                       ; 0xc4409
    add di, cx                                ; 01 cf                       ; 0xc440b
    add ax, dx                                ; 01 d0                       ; 0xc440d
    add word [bp-048fdh], si                  ; 01 b6 03 b7                 ; 0xc440f
    db  003h, 0ffh
    ; add di, di                                ; 03 ff                     ; 0xc4413
    db  0ffh
    db  0ffh
    jmp word [bp-07dh]                        ; ff 66 83                    ; 0xc4417
    sti                                       ; fb                          ; 0xc441a
    add byte [si+005h], dh                    ; 00 74 05                    ; 0xc441b
    mov eax, strict dword 066c30100h          ; 66 b8 00 01 c3 66           ; 0xc441e vberom.asm:825
    db  08bh, 0c2h
    ; mov ax, dx                                ; 8b c2                     ; 0xc4424
    push edx                                  ; 66 52                       ; 0xc4426 vberom.asm:829
    push eax                                  ; 66 50                       ; 0xc4428 vberom.asm:830
    mov edx, strict dword 0b86601ceh          ; 66 ba ce 01 66 b8           ; 0xc442a vberom.asm:831
    add ax, 06600h                            ; 05 00 66                    ; 0xc4430
    out DX, ax                                ; ef                          ; 0xc4433
    pop eax                                   ; 66 58                       ; 0xc4434 vberom.asm:834
    mov edx, strict dword 0ef6601cfh          ; 66 ba cf 01 66 ef           ; 0xc4436 vberom.asm:835
    in eax, DX                                ; 66 ed                       ; 0xc443c vberom.asm:837
    pop edx                                   ; 66 5a                       ; 0xc443e vberom.asm:838
    db  066h, 03bh, 0d0h
    ; cmp edx, eax                              ; 66 3b d0                  ; 0xc4440 vberom.asm:839
    jne short 0444ah                          ; 75 05                       ; 0xc4443 vberom.asm:840
    mov eax, strict dword 066c3004fh          ; 66 b8 4f 00 c3 66           ; 0xc4445 vberom.asm:841
    mov ax, 0014fh                            ; b8 4f 01                    ; 0xc444b
    retn                                      ; c3                          ; 0xc444e vberom.asm:845
    cmp bl, 080h                              ; 80 fb 80                    ; 0xc444f vberom.asm:847
    je short 0445eh                           ; 74 0a                       ; 0xc4452 vberom.asm:848
    cmp bl, 000h                              ; 80 fb 00                    ; 0xc4454 vberom.asm:849
    je short 0446eh                           ; 74 15                       ; 0xc4457 vberom.asm:850
    mov eax, strict dword 052c30100h          ; 66 b8 00 01 c3 52           ; 0xc4459 vberom.asm:851
    mov edx, strict dword 0a8ec03dah          ; 66 ba da 03 ec a8           ; 0xc445f vberom.asm:855
    or byte [di-005h], dh                     ; 08 75 fb                    ; 0xc4465
    in AL, DX                                 ; ec                          ; 0xc4468 vberom.asm:861
    test AL, strict byte 008h                 ; a8 08                       ; 0xc4469 vberom.asm:862
    je short 04468h                           ; 74 fb                       ; 0xc446b vberom.asm:863
    pop dx                                    ; 5a                          ; 0xc446d vberom.asm:864
    push ax                                   ; 50                          ; 0xc446e vberom.asm:868
    push cx                                   ; 51                          ; 0xc446f vberom.asm:869
    push dx                                   ; 52                          ; 0xc4470 vberom.asm:870
    push si                                   ; 56                          ; 0xc4471 vberom.asm:871
    push di                                   ; 57                          ; 0xc4472 vberom.asm:872
    sal dx, 010h                              ; c1 e2 10                    ; 0xc4473 vberom.asm:873
    and cx, strict word 0ffffh                ; 81 e1 ff ff                 ; 0xc4476 vberom.asm:874
    add byte [bx+si], al                      ; 00 00                       ; 0xc447a
    db  00bh, 0cah
    ; or cx, dx                                 ; 0b ca                     ; 0xc447c vberom.asm:875
    sal cx, 002h                              ; c1 e1 02                    ; 0xc447e vberom.asm:876
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1                     ; 0xc4481 vberom.asm:877
    push ax                                   ; 50                          ; 0xc4483 vberom.asm:878
    mov edx, strict dword 0b86601ceh          ; 66 ba ce 01 66 b8           ; 0xc4484 vberom.asm:879
    push ES                                   ; 06                          ; 0xc448a
    add byte [bp-011h], ah                    ; 00 66 ef                    ; 0xc448b
    mov edx, strict dword 0ed6601cfh          ; 66 ba cf 01 66 ed           ; 0xc448e vberom.asm:882
    db  00fh, 0b7h, 0c8h
    ; movzx cx, ax                              ; 0f b7 c8                  ; 0xc4494 vberom.asm:884
    mov edx, strict dword 0b86601ceh          ; 66 ba ce 01 66 b8           ; 0xc4497 vberom.asm:885
    add ax, word [bx+si]                      ; 03 00                       ; 0xc449d
    out DX, eax                               ; 66 ef                       ; 0xc449f vberom.asm:887
    mov edx, strict dword 0ed6601cfh          ; 66 ba cf 01 66 ed           ; 0xc44a1 vberom.asm:888
    db  00fh, 0b7h, 0f0h
    ; movzx si, ax                              ; 0f b7 f0                  ; 0xc44a7 vberom.asm:890
    pop ax                                    ; 58                          ; 0xc44aa vberom.asm:891
    cmp si, strict byte 00004h                ; 83 fe 04                    ; 0xc44ab vberom.asm:893
    je short 044c7h                           ; 74 17                       ; 0xc44ae vberom.asm:894
    add si, strict byte 00007h                ; 83 c6 07                    ; 0xc44b0 vberom.asm:895
    shr si, 003h                              ; c1 ee 03                    ; 0xc44b3 vberom.asm:896
    imul cx, si                               ; 0f af ce                    ; 0xc44b6 vberom.asm:897
    db  033h, 0d2h
    ; xor dx, dx                                ; 33 d2                     ; 0xc44b9 vberom.asm:898
    div cx                                    ; f7 f1                       ; 0xc44bb vberom.asm:899
    db  08bh, 0f8h
    ; mov di, ax                                ; 8b f8                     ; 0xc44bd vberom.asm:900
    db  08bh, 0c2h
    ; mov ax, dx                                ; 8b c2                     ; 0xc44bf vberom.asm:901
    db  033h, 0d2h
    ; xor dx, dx                                ; 33 d2                     ; 0xc44c1 vberom.asm:902
    div si                                    ; f7 f6                       ; 0xc44c3 vberom.asm:903
    jmp short 044d3h                          ; eb 0c                       ; 0xc44c5 vberom.asm:904
    shr cx, 1                                 ; d1 e9                       ; 0xc44c7 vberom.asm:907
    db  033h, 0d2h
    ; xor dx, dx                                ; 33 d2                     ; 0xc44c9 vberom.asm:908
    div cx                                    ; f7 f1                       ; 0xc44cb vberom.asm:909
    db  08bh, 0f8h
    ; mov di, ax                                ; 8b f8                     ; 0xc44cd vberom.asm:910
    db  08bh, 0c2h
    ; mov ax, dx                                ; 8b c2                     ; 0xc44cf vberom.asm:911
    sal ax, 1                                 ; d1 e0                       ; 0xc44d1 vberom.asm:912
    push edx                                  ; 66 52                       ; 0xc44d3 vberom.asm:915
    push eax                                  ; 66 50                       ; 0xc44d5 vberom.asm:916
    mov edx, strict dword 0b86601ceh          ; 66 ba ce 01 66 b8           ; 0xc44d7 vberom.asm:917
    or byte [bx+si], al                       ; 08 00                       ; 0xc44dd
    out DX, eax                               ; 66 ef                       ; 0xc44df vberom.asm:919
    pop eax                                   ; 66 58                       ; 0xc44e1 vberom.asm:920
    mov edx, strict dword 0ef6601cfh          ; 66 ba cf 01 66 ef           ; 0xc44e3 vberom.asm:921
    pop edx                                   ; 66 5a                       ; 0xc44e9 vberom.asm:923
    db  066h, 08bh, 0c7h
    ; mov eax, edi                              ; 66 8b c7                  ; 0xc44eb vberom.asm:925
    push edx                                  ; 66 52                       ; 0xc44ee vberom.asm:926
    push eax                                  ; 66 50                       ; 0xc44f0 vberom.asm:927
    mov edx, strict dword 0b86601ceh          ; 66 ba ce 01 66 b8           ; 0xc44f2 vberom.asm:928
    or word [bx+si], ax                       ; 09 00                       ; 0xc44f8
    out DX, eax                               ; 66 ef                       ; 0xc44fa vberom.asm:930
    pop eax                                   ; 66 58                       ; 0xc44fc vberom.asm:931
    mov edx, strict dword 0ef6601cfh          ; 66 ba cf 01 66 ef           ; 0xc44fe vberom.asm:932
    pop edx                                   ; 66 5a                       ; 0xc4504 vberom.asm:934
    pop di                                    ; 5f                          ; 0xc4506 vberom.asm:936
    pop si                                    ; 5e                          ; 0xc4507 vberom.asm:937
    pop dx                                    ; 5a                          ; 0xc4508 vberom.asm:938
    pop cx                                    ; 59                          ; 0xc4509 vberom.asm:939
    pop ax                                    ; 58                          ; 0xc450a vberom.asm:940
    mov eax, strict dword 066c3004fh          ; 66 b8 4f 00 c3 66           ; 0xc450b vberom.asm:941
    mov ax, 0014fh                            ; b8 4f 01                    ; 0xc4511
  ; disGetNextSymbol 0xc4514 LB 0x1 -> off=0x0 cb=0000000000000001 uValue=0000000000000114 'vesa_pm_end'
vesa_pm_end:                                 ; 0xc4514 LB 0x1
    retn                                      ; c3                          ; 0xc4514 vberom.asm:946

  ; Padding 0xeb bytes at 0xc4515
  times 235 db 0

section _DATA progbits vstart=0x4600 align=1 ; size=0x3732 class=DATA group=DGROUP
  ; disGetNextSymbol 0xc4600 LB 0x3732 -> off=0x0 cb=0000000000000034 uValue=00000000000c0000 '_msg_vga_init'
_msg_vga_init:                               ; 0xc4600 LB 0x34
    db  'Oracle VM VirtualBox Version 6.0.0_BETA1 VGA BIOS', 00dh, 00ah, 000h
  ; disGetNextSymbol 0xc4634 LB 0x36fe -> off=0x0 cb=0000000000000080 uValue=00000000000c0034 'vga_modes'
vga_modes:                                   ; 0xc4634 LB 0x80
    db  000h, 000h, 000h, 004h, 000h, 0b8h, 0ffh, 002h, 001h, 000h, 000h, 004h, 000h, 0b8h, 0ffh, 002h
    db  002h, 000h, 000h, 004h, 000h, 0b8h, 0ffh, 002h, 003h, 000h, 000h, 004h, 000h, 0b8h, 0ffh, 002h
    db  004h, 001h, 002h, 002h, 000h, 0b8h, 0ffh, 001h, 005h, 001h, 002h, 002h, 000h, 0b8h, 0ffh, 001h
    db  006h, 001h, 002h, 001h, 000h, 0b8h, 0ffh, 001h, 007h, 000h, 001h, 004h, 000h, 0b0h, 0ffh, 000h
    db  00dh, 001h, 004h, 004h, 000h, 0a0h, 0ffh, 001h, 00eh, 001h, 004h, 004h, 000h, 0a0h, 0ffh, 001h
    db  00fh, 001h, 003h, 001h, 000h, 0a0h, 0ffh, 000h, 010h, 001h, 004h, 004h, 000h, 0a0h, 0ffh, 002h
    db  011h, 001h, 003h, 001h, 000h, 0a0h, 0ffh, 002h, 012h, 001h, 004h, 004h, 000h, 0a0h, 0ffh, 002h
    db  013h, 001h, 005h, 008h, 000h, 0a0h, 0ffh, 003h, 06ah, 001h, 004h, 004h, 000h, 0a0h, 0ffh, 002h
  ; disGetNextSymbol 0xc46b4 LB 0x367e -> off=0x0 cb=0000000000000010 uValue=00000000000c00b4 'line_to_vpti'
line_to_vpti:                                ; 0xc46b4 LB 0x10
    db  017h, 017h, 018h, 018h, 004h, 005h, 006h, 007h, 00dh, 00eh, 011h, 012h, 01ah, 01bh, 01ch, 01dh
  ; disGetNextSymbol 0xc46c4 LB 0x366e -> off=0x0 cb=0000000000000004 uValue=00000000000c00c4 'dac_regs'
dac_regs:                                    ; 0xc46c4 LB 0x4
    dd  0ff3f3f3fh
  ; disGetNextSymbol 0xc46c8 LB 0x366a -> off=0x0 cb=0000000000000780 uValue=00000000000c00c8 'video_param_table'
video_param_table:                           ; 0xc46c8 LB 0x780
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
  ; disGetNextSymbol 0xc4e48 LB 0x2eea -> off=0x0 cb=00000000000000c0 uValue=00000000000c0848 'palette0'
palette0:                                    ; 0xc4e48 LB 0xc0
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
  ; disGetNextSymbol 0xc4f08 LB 0x2e2a -> off=0x0 cb=00000000000000c0 uValue=00000000000c0908 'palette1'
palette1:                                    ; 0xc4f08 LB 0xc0
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
  ; disGetNextSymbol 0xc4fc8 LB 0x2d6a -> off=0x0 cb=00000000000000c0 uValue=00000000000c09c8 'palette2'
palette2:                                    ; 0xc4fc8 LB 0xc0
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
  ; disGetNextSymbol 0xc5088 LB 0x2caa -> off=0x0 cb=0000000000000300 uValue=00000000000c0a88 'palette3'
palette3:                                    ; 0xc5088 LB 0x300
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
  ; disGetNextSymbol 0xc5388 LB 0x29aa -> off=0x0 cb=0000000000000010 uValue=00000000000c0d88 'static_functionality'
static_functionality:                        ; 0xc5388 LB 0x10
    db  0ffh, 0e0h, 00fh, 000h, 000h, 000h, 000h, 007h, 002h, 008h, 0e7h, 00ch, 000h, 000h, 000h, 000h
  ; disGetNextSymbol 0xc5398 LB 0x299a -> off=0x0 cb=0000000000000024 uValue=00000000000c0d98 '_dcc_table'
_dcc_table:                                  ; 0xc5398 LB 0x24
    db  010h, 001h, 007h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h
  ; disGetNextSymbol 0xc53bc LB 0x2976 -> off=0x0 cb=000000000000001a uValue=00000000000c0dbc '_secondary_save_area'
_secondary_save_area:                        ; 0xc53bc LB 0x1a
    db  01ah, 000h, 098h, 053h, 000h, 0c0h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
  ; disGetNextSymbol 0xc53d6 LB 0x295c -> off=0x0 cb=000000000000001c uValue=00000000000c0dd6 '_video_save_pointer_table'
_video_save_pointer_table:                   ; 0xc53d6 LB 0x1c
    db  0c8h, 046h, 000h, 0c0h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  0bch, 053h, 000h, 0c0h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
  ; disGetNextSymbol 0xc53f2 LB 0x2940 -> off=0x0 cb=0000000000000800 uValue=00000000000c0df2 'vgafont8'
vgafont8:                                    ; 0xc53f2 LB 0x800
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
  ; disGetNextSymbol 0xc5bf2 LB 0x2140 -> off=0x0 cb=0000000000000e00 uValue=00000000000c15f2 'vgafont14'
vgafont14:                                   ; 0xc5bf2 LB 0xe00
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
  ; disGetNextSymbol 0xc69f2 LB 0x1340 -> off=0x0 cb=0000000000001000 uValue=00000000000c23f2 'vgafont16'
vgafont16:                                   ; 0xc69f2 LB 0x1000
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
  ; disGetNextSymbol 0xc79f2 LB 0x340 -> off=0x0 cb=000000000000012d uValue=00000000000c33f2 'vgafont14alt'
vgafont14alt:                                ; 0xc79f2 LB 0x12d
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
  ; disGetNextSymbol 0xc7b1f LB 0x213 -> off=0x0 cb=0000000000000144 uValue=00000000000c351f 'vgafont16alt'
vgafont16alt:                                ; 0xc7b1f LB 0x144
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
  ; disGetNextSymbol 0xc7c63 LB 0xcf -> off=0x0 cb=0000000000000009 uValue=00000000000c3663 '_cga_msr'
_cga_msr:                                    ; 0xc7c63 LB 0x9
    db  02ch, 028h, 02dh, 029h, 02ah, 02eh, 01eh, 029h, 000h
  ; disGetNextSymbol 0xc7c6c LB 0xc6 -> off=0x0 cb=0000000000000015 uValue=00000000000c366c '_vbebios_copyright'
_vbebios_copyright:                          ; 0xc7c6c LB 0x15
    db  'VirtualBox VESA BIOS', 000h
  ; disGetNextSymbol 0xc7c81 LB 0xb1 -> off=0x0 cb=0000000000000013 uValue=00000000000c3681 '_vbebios_vendor_name'
_vbebios_vendor_name:                        ; 0xc7c81 LB 0x13
    db  'Oracle Corporation', 000h
  ; disGetNextSymbol 0xc7c94 LB 0x9e -> off=0x0 cb=0000000000000021 uValue=00000000000c3694 '_vbebios_product_name'
_vbebios_product_name:                       ; 0xc7c94 LB 0x21
    db  'Oracle VM VirtualBox VBE Adapter', 000h
  ; disGetNextSymbol 0xc7cb5 LB 0x7d -> off=0x0 cb=0000000000000029 uValue=00000000000c36b5 '_vbebios_product_revision'
_vbebios_product_revision:                   ; 0xc7cb5 LB 0x29
    db  'Oracle VM VirtualBox Version 6.0.0_BETA1', 000h
  ; disGetNextSymbol 0xc7cde LB 0x54 -> off=0x0 cb=000000000000002b uValue=00000000000c36de '_vbebios_info_string'
_vbebios_info_string:                        ; 0xc7cde LB 0x2b
    db  'VirtualBox VBE Display Adapter enabled', 00dh, 00ah, 00dh, 00ah, 000h
  ; disGetNextSymbol 0xc7d09 LB 0x29 -> off=0x0 cb=0000000000000029 uValue=00000000000c3709 '_no_vbebios_info_string'
_no_vbebios_info_string:                     ; 0xc7d09 LB 0x29
    db  'No VirtualBox VBE support available!', 00dh, 00ah, 00dh, 00ah, 000h

section CONST progbits vstart=0x7d32 align=1 ; size=0x0 class=DATA group=DGROUP

section CONST2 progbits vstart=0x7d32 align=1 ; size=0x0 class=DATA group=DGROUP

  ; Padding 0x2ce bytes at 0xc7d32
    db  001h, 000h, 000h, 000h, 000h, 001h, 000h, 000h, 000h, 000h, 000h, 000h, 045h, 03ah, 05ch, 076h
    db  062h, 06fh, 078h, 05ch, 073h, 076h, 06eh, 05ch, 074h, 072h, 075h, 06eh, 06bh, 05ch, 06fh, 075h
    db  074h, 05ch, 077h, 069h, 06eh, 02eh, 061h, 06dh, 064h, 036h, 034h, 05ch, 072h, 065h, 06ch, 065h
    db  061h, 073h, 065h, 05ch, 06fh, 062h, 06ah, 05ch, 056h, 042h, 06fh, 078h, 056h, 067h, 061h, 042h
    db  069h, 06fh, 073h, 038h, 030h, 038h, 036h, 05ch, 056h, 042h, 06fh, 078h, 056h, 067h, 061h, 042h
    db  069h, 06fh, 073h, 038h, 030h, 038h, 036h, 02eh, 073h, 079h, 06dh, 000h, 000h, 000h, 000h, 000h
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
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 060h
