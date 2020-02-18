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





section VGAROM progbits vstart=0x0 align=1 ; size=0x93f class=CODE group=AUTO
  ; disGetNextSymbol 0xc0000 LB 0x93f -> off=0x22 cb=000000000000056e uValue=00000000000c0022 'vgabios_int10_handler'
    db  055h, 0aah, 040h, 0e9h, 0e4h, 009h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 049h, 042h
    db  04dh, 000h
vgabios_int10_handler:                       ; 0xc0022 LB 0x56e
    pushfw                                    ; 9c                          ; 0xc0022 vgarom.asm:84
    cmp ah, 00fh                              ; 80 fc 0f                    ; 0xc0023 vgarom.asm:97
    jne short 0002eh                          ; 75 06                       ; 0xc0026 vgarom.asm:98
    call 00187h                               ; e8 5c 01                    ; 0xc0028 vgarom.asm:99
    jmp near 000f7h                           ; e9 c9 00                    ; 0xc002b vgarom.asm:100
    cmp ah, 01ah                              ; 80 fc 1a                    ; 0xc002e vgarom.asm:102
    jne short 00039h                          ; 75 06                       ; 0xc0031 vgarom.asm:103
    call 0055ah                               ; e8 24 05                    ; 0xc0033 vgarom.asm:104
    jmp near 000f7h                           ; e9 be 00                    ; 0xc0036 vgarom.asm:105
    cmp ah, 00bh                              ; 80 fc 0b                    ; 0xc0039 vgarom.asm:107
    jne short 00044h                          ; 75 06                       ; 0xc003c vgarom.asm:108
    call 000f9h                               ; e8 b8 00                    ; 0xc003e vgarom.asm:109
    jmp near 000f7h                           ; e9 b3 00                    ; 0xc0041 vgarom.asm:110
    cmp ax, 01103h                            ; 3d 03 11                    ; 0xc0044 vgarom.asm:112
    jne short 0004fh                          ; 75 06                       ; 0xc0047 vgarom.asm:113
    call 0044eh                               ; e8 02 04                    ; 0xc0049 vgarom.asm:114
    jmp near 000f7h                           ; e9 a8 00                    ; 0xc004c vgarom.asm:115
    cmp ah, 012h                              ; 80 fc 12                    ; 0xc004f vgarom.asm:117
    jne short 00093h                          ; 75 3f                       ; 0xc0052 vgarom.asm:118
    cmp bl, 010h                              ; 80 fb 10                    ; 0xc0054 vgarom.asm:119
    jne short 0005fh                          ; 75 06                       ; 0xc0057 vgarom.asm:120
    call 0045bh                               ; e8 ff 03                    ; 0xc0059 vgarom.asm:121
    jmp near 000f7h                           ; e9 98 00                    ; 0xc005c vgarom.asm:122
    cmp bl, 030h                              ; 80 fb 30                    ; 0xc005f vgarom.asm:124
    jne short 0006ah                          ; 75 06                       ; 0xc0062 vgarom.asm:125
    call 0047eh                               ; e8 17 04                    ; 0xc0064 vgarom.asm:126
    jmp near 000f7h                           ; e9 8d 00                    ; 0xc0067 vgarom.asm:127
    cmp bl, 031h                              ; 80 fb 31                    ; 0xc006a vgarom.asm:129
    jne short 00075h                          ; 75 06                       ; 0xc006d vgarom.asm:130
    call 004d1h                               ; e8 5f 04                    ; 0xc006f vgarom.asm:131
    jmp near 000f7h                           ; e9 82 00                    ; 0xc0072 vgarom.asm:132
    cmp bl, 032h                              ; 80 fb 32                    ; 0xc0075 vgarom.asm:134
    jne short 0007fh                          ; 75 05                       ; 0xc0078 vgarom.asm:135
    call 004f6h                               ; e8 79 04                    ; 0xc007a vgarom.asm:136
    jmp short 000f7h                          ; eb 78                       ; 0xc007d vgarom.asm:137
    cmp bl, 033h                              ; 80 fb 33                    ; 0xc007f vgarom.asm:139
    jne short 00089h                          ; 75 05                       ; 0xc0082 vgarom.asm:140
    call 00514h                               ; e8 8d 04                    ; 0xc0084 vgarom.asm:141
    jmp short 000f7h                          ; eb 6e                       ; 0xc0087 vgarom.asm:142
    cmp bl, 034h                              ; 80 fb 34                    ; 0xc0089 vgarom.asm:144
    jne short 000ddh                          ; 75 4f                       ; 0xc008c vgarom.asm:145
    call 00538h                               ; e8 a7 04                    ; 0xc008e vgarom.asm:146
    jmp short 000f7h                          ; eb 64                       ; 0xc0091 vgarom.asm:147
    cmp ax, 0101bh                            ; 3d 1b 10                    ; 0xc0093 vgarom.asm:149
    je short 000ddh                           ; 74 45                       ; 0xc0096 vgarom.asm:150
    cmp ah, 010h                              ; 80 fc 10                    ; 0xc0098 vgarom.asm:151
    jne short 000a2h                          ; 75 05                       ; 0xc009b vgarom.asm:155
    call 001aeh                               ; e8 0e 01                    ; 0xc009d vgarom.asm:157
    jmp short 000f7h                          ; eb 55                       ; 0xc00a0 vgarom.asm:158
    cmp ah, 04fh                              ; 80 fc 4f                    ; 0xc00a2 vgarom.asm:161
    jne short 000ddh                          ; 75 36                       ; 0xc00a5 vgarom.asm:162
    cmp AL, strict byte 003h                  ; 3c 03                       ; 0xc00a7 vgarom.asm:163
    jne short 000b0h                          ; 75 05                       ; 0xc00a9 vgarom.asm:164
    call 007fbh                               ; e8 4d 07                    ; 0xc00ab vgarom.asm:165
    jmp short 000f7h                          ; eb 47                       ; 0xc00ae vgarom.asm:166
    cmp AL, strict byte 005h                  ; 3c 05                       ; 0xc00b0 vgarom.asm:168
    jne short 000b9h                          ; 75 05                       ; 0xc00b2 vgarom.asm:169
    call 00820h                               ; e8 69 07                    ; 0xc00b4 vgarom.asm:170
    jmp short 000f7h                          ; eb 3e                       ; 0xc00b7 vgarom.asm:171
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc00b9 vgarom.asm:173
    jne short 000c2h                          ; 75 05                       ; 0xc00bb vgarom.asm:174
    call 0084dh                               ; e8 8d 07                    ; 0xc00bd vgarom.asm:175
    jmp short 000f7h                          ; eb 35                       ; 0xc00c0 vgarom.asm:176
    cmp AL, strict byte 008h                  ; 3c 08                       ; 0xc00c2 vgarom.asm:178
    jne short 000cbh                          ; 75 05                       ; 0xc00c4 vgarom.asm:179
    call 00881h                               ; e8 b8 07                    ; 0xc00c6 vgarom.asm:180
    jmp short 000f7h                          ; eb 2c                       ; 0xc00c9 vgarom.asm:181
    cmp AL, strict byte 009h                  ; 3c 09                       ; 0xc00cb vgarom.asm:183
    jne short 000d4h                          ; 75 05                       ; 0xc00cd vgarom.asm:184
    call 008b8h                               ; e8 e6 07                    ; 0xc00cf vgarom.asm:185
    jmp short 000f7h                          ; eb 23                       ; 0xc00d2 vgarom.asm:186
    cmp AL, strict byte 00ah                  ; 3c 0a                       ; 0xc00d4 vgarom.asm:188
    jne short 000ddh                          ; 75 05                       ; 0xc00d6 vgarom.asm:189
    call 0092bh                               ; e8 50 08                    ; 0xc00d8 vgarom.asm:190
    jmp short 000f7h                          ; eb 1a                       ; 0xc00db vgarom.asm:191
    push ES                                   ; 06                          ; 0xc00dd vgarom.asm:195
    push DS                                   ; 1e                          ; 0xc00de vgarom.asm:196
    push ax                                   ; 50                          ; 0xc00df vgarom.asm:99
    push cx                                   ; 51                          ; 0xc00e0 vgarom.asm:100
    push dx                                   ; 52                          ; 0xc00e1 vgarom.asm:101
    push bx                                   ; 53                          ; 0xc00e2 vgarom.asm:102
    push sp                                   ; 54                          ; 0xc00e3 vgarom.asm:103
    push bp                                   ; 55                          ; 0xc00e4 vgarom.asm:104
    push si                                   ; 56                          ; 0xc00e5 vgarom.asm:105
    push di                                   ; 57                          ; 0xc00e6 vgarom.asm:106
    push CS                                   ; 0e                          ; 0xc00e7 vgarom.asm:200
    pop DS                                    ; 1f                          ; 0xc00e8 vgarom.asm:201
    cld                                       ; fc                          ; 0xc00e9 vgarom.asm:202
    call 036f0h                               ; e8 03 36                    ; 0xc00ea vgarom.asm:203
    pop di                                    ; 5f                          ; 0xc00ed vgarom.asm:116
    pop si                                    ; 5e                          ; 0xc00ee vgarom.asm:117
    pop bp                                    ; 5d                          ; 0xc00ef vgarom.asm:118
    pop bx                                    ; 5b                          ; 0xc00f0 vgarom.asm:119
    pop bx                                    ; 5b                          ; 0xc00f1 vgarom.asm:120
    pop dx                                    ; 5a                          ; 0xc00f2 vgarom.asm:121
    pop cx                                    ; 59                          ; 0xc00f3 vgarom.asm:122
    pop ax                                    ; 58                          ; 0xc00f4 vgarom.asm:123
    pop DS                                    ; 1f                          ; 0xc00f5 vgarom.asm:206
    pop ES                                    ; 07                          ; 0xc00f6 vgarom.asm:207
    popfw                                     ; 9d                          ; 0xc00f7 vgarom.asm:209
    iret                                      ; cf                          ; 0xc00f8 vgarom.asm:210
    cmp bh, 000h                              ; 80 ff 00                    ; 0xc00f9 vgarom.asm:215
    je short 00104h                           ; 74 06                       ; 0xc00fc vgarom.asm:216
    cmp bh, 001h                              ; 80 ff 01                    ; 0xc00fe vgarom.asm:217
    je short 00155h                           ; 74 52                       ; 0xc0101 vgarom.asm:218
    retn                                      ; c3                          ; 0xc0103 vgarom.asm:222
    push ax                                   ; 50                          ; 0xc0104 vgarom.asm:224
    push bx                                   ; 53                          ; 0xc0105 vgarom.asm:225
    push cx                                   ; 51                          ; 0xc0106 vgarom.asm:226
    push dx                                   ; 52                          ; 0xc0107 vgarom.asm:227
    push DS                                   ; 1e                          ; 0xc0108 vgarom.asm:228
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc0109 vgarom.asm:229
    mov ds, dx                                ; 8e da                       ; 0xc010c vgarom.asm:230
    mov dx, 003dah                            ; ba da 03                    ; 0xc010e vgarom.asm:231
    in AL, DX                                 ; ec                          ; 0xc0111 vgarom.asm:232
    cmp byte [word 00049h], 003h              ; 80 3e 49 00 03              ; 0xc0112 vgarom.asm:233
    jbe short 00148h                          ; 76 2f                       ; 0xc0117 vgarom.asm:234
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0119 vgarom.asm:235
    mov AL, strict byte 000h                  ; b0 00                       ; 0xc011c vgarom.asm:236
    out DX, AL                                ; ee                          ; 0xc011e vgarom.asm:237
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc011f vgarom.asm:238
    and AL, strict byte 00fh                  ; 24 0f                       ; 0xc0121 vgarom.asm:239
    test AL, strict byte 008h                 ; a8 08                       ; 0xc0123 vgarom.asm:240
    je short 00129h                           ; 74 02                       ; 0xc0125 vgarom.asm:241
    add AL, strict byte 008h                  ; 04 08                       ; 0xc0127 vgarom.asm:242
    out DX, AL                                ; ee                          ; 0xc0129 vgarom.asm:244
    mov CL, strict byte 001h                  ; b1 01                       ; 0xc012a vgarom.asm:245
    and bl, 010h                              ; 80 e3 10                    ; 0xc012c vgarom.asm:246
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc012f vgarom.asm:248
    db  08ah, 0c1h
    ; mov al, cl                                ; 8a c1                     ; 0xc0132 vgarom.asm:249
    out DX, AL                                ; ee                          ; 0xc0134 vgarom.asm:250
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc0135 vgarom.asm:251
    in AL, DX                                 ; ec                          ; 0xc0138 vgarom.asm:252
    and AL, strict byte 0efh                  ; 24 ef                       ; 0xc0139 vgarom.asm:253
    db  00ah, 0c3h
    ; or al, bl                                 ; 0a c3                     ; 0xc013b vgarom.asm:254
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc013d vgarom.asm:255
    out DX, AL                                ; ee                          ; 0xc0140 vgarom.asm:256
    db  0feh, 0c1h
    ; inc cl                                    ; fe c1                     ; 0xc0141 vgarom.asm:257
    cmp cl, 004h                              ; 80 f9 04                    ; 0xc0143 vgarom.asm:258
    jne short 0012fh                          ; 75 e7                       ; 0xc0146 vgarom.asm:259
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc0148 vgarom.asm:261
    out DX, AL                                ; ee                          ; 0xc014a vgarom.asm:262
    mov dx, 003dah                            ; ba da 03                    ; 0xc014b vgarom.asm:264
    in AL, DX                                 ; ec                          ; 0xc014e vgarom.asm:265
    pop DS                                    ; 1f                          ; 0xc014f vgarom.asm:267
    pop dx                                    ; 5a                          ; 0xc0150 vgarom.asm:268
    pop cx                                    ; 59                          ; 0xc0151 vgarom.asm:269
    pop bx                                    ; 5b                          ; 0xc0152 vgarom.asm:270
    pop ax                                    ; 58                          ; 0xc0153 vgarom.asm:271
    retn                                      ; c3                          ; 0xc0154 vgarom.asm:272
    push ax                                   ; 50                          ; 0xc0155 vgarom.asm:274
    push bx                                   ; 53                          ; 0xc0156 vgarom.asm:275
    push cx                                   ; 51                          ; 0xc0157 vgarom.asm:276
    push dx                                   ; 52                          ; 0xc0158 vgarom.asm:277
    mov dx, 003dah                            ; ba da 03                    ; 0xc0159 vgarom.asm:278
    in AL, DX                                 ; ec                          ; 0xc015c vgarom.asm:279
    mov CL, strict byte 001h                  ; b1 01                       ; 0xc015d vgarom.asm:280
    and bl, 001h                              ; 80 e3 01                    ; 0xc015f vgarom.asm:281
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0162 vgarom.asm:283
    db  08ah, 0c1h
    ; mov al, cl                                ; 8a c1                     ; 0xc0165 vgarom.asm:284
    out DX, AL                                ; ee                          ; 0xc0167 vgarom.asm:285
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc0168 vgarom.asm:286
    in AL, DX                                 ; ec                          ; 0xc016b vgarom.asm:287
    and AL, strict byte 0feh                  ; 24 fe                       ; 0xc016c vgarom.asm:288
    db  00ah, 0c3h
    ; or al, bl                                 ; 0a c3                     ; 0xc016e vgarom.asm:289
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0170 vgarom.asm:290
    out DX, AL                                ; ee                          ; 0xc0173 vgarom.asm:291
    db  0feh, 0c1h
    ; inc cl                                    ; fe c1                     ; 0xc0174 vgarom.asm:292
    cmp cl, 004h                              ; 80 f9 04                    ; 0xc0176 vgarom.asm:293
    jne short 00162h                          ; 75 e7                       ; 0xc0179 vgarom.asm:294
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc017b vgarom.asm:295
    out DX, AL                                ; ee                          ; 0xc017d vgarom.asm:296
    mov dx, 003dah                            ; ba da 03                    ; 0xc017e vgarom.asm:298
    in AL, DX                                 ; ec                          ; 0xc0181 vgarom.asm:299
    pop dx                                    ; 5a                          ; 0xc0182 vgarom.asm:301
    pop cx                                    ; 59                          ; 0xc0183 vgarom.asm:302
    pop bx                                    ; 5b                          ; 0xc0184 vgarom.asm:303
    pop ax                                    ; 58                          ; 0xc0185 vgarom.asm:304
    retn                                      ; c3                          ; 0xc0186 vgarom.asm:305
    push DS                                   ; 1e                          ; 0xc0187 vgarom.asm:310
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0188 vgarom.asm:311
    mov ds, ax                                ; 8e d8                       ; 0xc018b vgarom.asm:312
    push bx                                   ; 53                          ; 0xc018d vgarom.asm:313
    mov bx, strict word 00062h                ; bb 62 00                    ; 0xc018e vgarom.asm:314
    mov al, byte [bx]                         ; 8a 07                       ; 0xc0191 vgarom.asm:315
    pop bx                                    ; 5b                          ; 0xc0193 vgarom.asm:316
    db  08ah, 0f8h
    ; mov bh, al                                ; 8a f8                     ; 0xc0194 vgarom.asm:317
    push bx                                   ; 53                          ; 0xc0196 vgarom.asm:318
    mov bx, 00087h                            ; bb 87 00                    ; 0xc0197 vgarom.asm:319
    mov ah, byte [bx]                         ; 8a 27                       ; 0xc019a vgarom.asm:320
    and ah, 080h                              ; 80 e4 80                    ; 0xc019c vgarom.asm:321
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc019f vgarom.asm:322
    mov al, byte [bx]                         ; 8a 07                       ; 0xc01a2 vgarom.asm:323
    db  00ah, 0c4h
    ; or al, ah                                 ; 0a c4                     ; 0xc01a4 vgarom.asm:324
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc01a6 vgarom.asm:325
    mov ah, byte [bx]                         ; 8a 27                       ; 0xc01a9 vgarom.asm:326
    pop bx                                    ; 5b                          ; 0xc01ab vgarom.asm:327
    pop DS                                    ; 1f                          ; 0xc01ac vgarom.asm:328
    retn                                      ; c3                          ; 0xc01ad vgarom.asm:329
    cmp AL, strict byte 000h                  ; 3c 00                       ; 0xc01ae vgarom.asm:334
    jne short 001b4h                          ; 75 02                       ; 0xc01b0 vgarom.asm:335
    jmp short 00215h                          ; eb 61                       ; 0xc01b2 vgarom.asm:336
    cmp AL, strict byte 001h                  ; 3c 01                       ; 0xc01b4 vgarom.asm:338
    jne short 001bah                          ; 75 02                       ; 0xc01b6 vgarom.asm:339
    jmp short 00233h                          ; eb 79                       ; 0xc01b8 vgarom.asm:340
    cmp AL, strict byte 002h                  ; 3c 02                       ; 0xc01ba vgarom.asm:342
    jne short 001c0h                          ; 75 02                       ; 0xc01bc vgarom.asm:343
    jmp short 0023bh                          ; eb 7b                       ; 0xc01be vgarom.asm:344
    cmp AL, strict byte 003h                  ; 3c 03                       ; 0xc01c0 vgarom.asm:346
    jne short 001c7h                          ; 75 03                       ; 0xc01c2 vgarom.asm:347
    jmp near 0026ch                           ; e9 a5 00                    ; 0xc01c4 vgarom.asm:348
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc01c7 vgarom.asm:350
    jne short 001ceh                          ; 75 03                       ; 0xc01c9 vgarom.asm:351
    jmp near 00299h                           ; e9 cb 00                    ; 0xc01cb vgarom.asm:352
    cmp AL, strict byte 008h                  ; 3c 08                       ; 0xc01ce vgarom.asm:354
    jne short 001d5h                          ; 75 03                       ; 0xc01d0 vgarom.asm:355
    jmp near 002c1h                           ; e9 ec 00                    ; 0xc01d2 vgarom.asm:356
    cmp AL, strict byte 009h                  ; 3c 09                       ; 0xc01d5 vgarom.asm:358
    jne short 001dch                          ; 75 03                       ; 0xc01d7 vgarom.asm:359
    jmp near 002cfh                           ; e9 f3 00                    ; 0xc01d9 vgarom.asm:360
    cmp AL, strict byte 010h                  ; 3c 10                       ; 0xc01dc vgarom.asm:362
    jne short 001e3h                          ; 75 03                       ; 0xc01de vgarom.asm:363
    jmp near 00314h                           ; e9 31 01                    ; 0xc01e0 vgarom.asm:364
    cmp AL, strict byte 012h                  ; 3c 12                       ; 0xc01e3 vgarom.asm:366
    jne short 001eah                          ; 75 03                       ; 0xc01e5 vgarom.asm:367
    jmp near 0032dh                           ; e9 43 01                    ; 0xc01e7 vgarom.asm:368
    cmp AL, strict byte 013h                  ; 3c 13                       ; 0xc01ea vgarom.asm:370
    jne short 001f1h                          ; 75 03                       ; 0xc01ec vgarom.asm:371
    jmp near 00355h                           ; e9 64 01                    ; 0xc01ee vgarom.asm:372
    cmp AL, strict byte 015h                  ; 3c 15                       ; 0xc01f1 vgarom.asm:374
    jne short 001f8h                          ; 75 03                       ; 0xc01f3 vgarom.asm:375
    jmp near 003a8h                           ; e9 b0 01                    ; 0xc01f5 vgarom.asm:376
    cmp AL, strict byte 017h                  ; 3c 17                       ; 0xc01f8 vgarom.asm:378
    jne short 001ffh                          ; 75 03                       ; 0xc01fa vgarom.asm:379
    jmp near 003c3h                           ; e9 c4 01                    ; 0xc01fc vgarom.asm:380
    cmp AL, strict byte 018h                  ; 3c 18                       ; 0xc01ff vgarom.asm:382
    jne short 00206h                          ; 75 03                       ; 0xc0201 vgarom.asm:383
    jmp near 003ebh                           ; e9 e5 01                    ; 0xc0203 vgarom.asm:384
    cmp AL, strict byte 019h                  ; 3c 19                       ; 0xc0206 vgarom.asm:386
    jne short 0020dh                          ; 75 03                       ; 0xc0208 vgarom.asm:387
    jmp near 003f6h                           ; e9 e9 01                    ; 0xc020a vgarom.asm:388
    cmp AL, strict byte 01ah                  ; 3c 1a                       ; 0xc020d vgarom.asm:390
    jne short 00214h                          ; 75 03                       ; 0xc020f vgarom.asm:391
    jmp near 00401h                           ; e9 ed 01                    ; 0xc0211 vgarom.asm:392
    retn                                      ; c3                          ; 0xc0214 vgarom.asm:397
    cmp bl, 014h                              ; 80 fb 14                    ; 0xc0215 vgarom.asm:400
    jnbe short 00232h                         ; 77 18                       ; 0xc0218 vgarom.asm:401
    push ax                                   ; 50                          ; 0xc021a vgarom.asm:402
    push dx                                   ; 52                          ; 0xc021b vgarom.asm:403
    mov dx, 003dah                            ; ba da 03                    ; 0xc021c vgarom.asm:404
    in AL, DX                                 ; ec                          ; 0xc021f vgarom.asm:405
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0220 vgarom.asm:406
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc0223 vgarom.asm:407
    out DX, AL                                ; ee                          ; 0xc0225 vgarom.asm:408
    db  08ah, 0c7h
    ; mov al, bh                                ; 8a c7                     ; 0xc0226 vgarom.asm:409
    out DX, AL                                ; ee                          ; 0xc0228 vgarom.asm:410
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc0229 vgarom.asm:411
    out DX, AL                                ; ee                          ; 0xc022b vgarom.asm:412
    mov dx, 003dah                            ; ba da 03                    ; 0xc022c vgarom.asm:414
    in AL, DX                                 ; ec                          ; 0xc022f vgarom.asm:415
    pop dx                                    ; 5a                          ; 0xc0230 vgarom.asm:417
    pop ax                                    ; 58                          ; 0xc0231 vgarom.asm:418
    retn                                      ; c3                          ; 0xc0232 vgarom.asm:420
    push bx                                   ; 53                          ; 0xc0233 vgarom.asm:425
    mov BL, strict byte 011h                  ; b3 11                       ; 0xc0234 vgarom.asm:426
    call 00215h                               ; e8 dc ff                    ; 0xc0236 vgarom.asm:427
    pop bx                                    ; 5b                          ; 0xc0239 vgarom.asm:428
    retn                                      ; c3                          ; 0xc023a vgarom.asm:429
    push ax                                   ; 50                          ; 0xc023b vgarom.asm:434
    push bx                                   ; 53                          ; 0xc023c vgarom.asm:435
    push cx                                   ; 51                          ; 0xc023d vgarom.asm:436
    push dx                                   ; 52                          ; 0xc023e vgarom.asm:437
    db  08bh, 0dah
    ; mov bx, dx                                ; 8b da                     ; 0xc023f vgarom.asm:438
    mov dx, 003dah                            ; ba da 03                    ; 0xc0241 vgarom.asm:439
    in AL, DX                                 ; ec                          ; 0xc0244 vgarom.asm:440
    mov CL, strict byte 000h                  ; b1 00                       ; 0xc0245 vgarom.asm:441
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0247 vgarom.asm:442
    db  08ah, 0c1h
    ; mov al, cl                                ; 8a c1                     ; 0xc024a vgarom.asm:444
    out DX, AL                                ; ee                          ; 0xc024c vgarom.asm:445
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc024d vgarom.asm:446
    out DX, AL                                ; ee                          ; 0xc0250 vgarom.asm:447
    inc bx                                    ; 43                          ; 0xc0251 vgarom.asm:448
    db  0feh, 0c1h
    ; inc cl                                    ; fe c1                     ; 0xc0252 vgarom.asm:449
    cmp cl, 010h                              ; 80 f9 10                    ; 0xc0254 vgarom.asm:450
    jne short 0024ah                          ; 75 f1                       ; 0xc0257 vgarom.asm:451
    mov AL, strict byte 011h                  ; b0 11                       ; 0xc0259 vgarom.asm:452
    out DX, AL                                ; ee                          ; 0xc025b vgarom.asm:453
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc025c vgarom.asm:454
    out DX, AL                                ; ee                          ; 0xc025f vgarom.asm:455
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc0260 vgarom.asm:456
    out DX, AL                                ; ee                          ; 0xc0262 vgarom.asm:457
    mov dx, 003dah                            ; ba da 03                    ; 0xc0263 vgarom.asm:459
    in AL, DX                                 ; ec                          ; 0xc0266 vgarom.asm:460
    pop dx                                    ; 5a                          ; 0xc0267 vgarom.asm:462
    pop cx                                    ; 59                          ; 0xc0268 vgarom.asm:463
    pop bx                                    ; 5b                          ; 0xc0269 vgarom.asm:464
    pop ax                                    ; 58                          ; 0xc026a vgarom.asm:465
    retn                                      ; c3                          ; 0xc026b vgarom.asm:466
    push ax                                   ; 50                          ; 0xc026c vgarom.asm:471
    push bx                                   ; 53                          ; 0xc026d vgarom.asm:472
    push dx                                   ; 52                          ; 0xc026e vgarom.asm:473
    mov dx, 003dah                            ; ba da 03                    ; 0xc026f vgarom.asm:474
    in AL, DX                                 ; ec                          ; 0xc0272 vgarom.asm:475
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0273 vgarom.asm:476
    mov AL, strict byte 010h                  ; b0 10                       ; 0xc0276 vgarom.asm:477
    out DX, AL                                ; ee                          ; 0xc0278 vgarom.asm:478
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc0279 vgarom.asm:479
    in AL, DX                                 ; ec                          ; 0xc027c vgarom.asm:480
    and AL, strict byte 0f7h                  ; 24 f7                       ; 0xc027d vgarom.asm:481
    and bl, 001h                              ; 80 e3 01                    ; 0xc027f vgarom.asm:482
    sal bl, 1                                 ; d0 e3                       ; 0xc0282 vgarom.asm:486
    sal bl, 1                                 ; d0 e3                       ; 0xc0284 vgarom.asm:487
    sal bl, 1                                 ; d0 e3                       ; 0xc0286 vgarom.asm:488
    db  00ah, 0c3h
    ; or al, bl                                 ; 0a c3                     ; 0xc0288 vgarom.asm:490
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc028a vgarom.asm:491
    out DX, AL                                ; ee                          ; 0xc028d vgarom.asm:492
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc028e vgarom.asm:493
    out DX, AL                                ; ee                          ; 0xc0290 vgarom.asm:494
    mov dx, 003dah                            ; ba da 03                    ; 0xc0291 vgarom.asm:496
    in AL, DX                                 ; ec                          ; 0xc0294 vgarom.asm:497
    pop dx                                    ; 5a                          ; 0xc0295 vgarom.asm:499
    pop bx                                    ; 5b                          ; 0xc0296 vgarom.asm:500
    pop ax                                    ; 58                          ; 0xc0297 vgarom.asm:501
    retn                                      ; c3                          ; 0xc0298 vgarom.asm:502
    cmp bl, 014h                              ; 80 fb 14                    ; 0xc0299 vgarom.asm:507
    jnbe short 002c0h                         ; 77 22                       ; 0xc029c vgarom.asm:508
    push ax                                   ; 50                          ; 0xc029e vgarom.asm:509
    push dx                                   ; 52                          ; 0xc029f vgarom.asm:510
    mov dx, 003dah                            ; ba da 03                    ; 0xc02a0 vgarom.asm:511
    in AL, DX                                 ; ec                          ; 0xc02a3 vgarom.asm:512
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc02a4 vgarom.asm:513
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc02a7 vgarom.asm:514
    out DX, AL                                ; ee                          ; 0xc02a9 vgarom.asm:515
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc02aa vgarom.asm:516
    in AL, DX                                 ; ec                          ; 0xc02ad vgarom.asm:517
    db  08ah, 0f8h
    ; mov bh, al                                ; 8a f8                     ; 0xc02ae vgarom.asm:518
    mov dx, 003dah                            ; ba da 03                    ; 0xc02b0 vgarom.asm:519
    in AL, DX                                 ; ec                          ; 0xc02b3 vgarom.asm:520
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc02b4 vgarom.asm:521
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc02b7 vgarom.asm:522
    out DX, AL                                ; ee                          ; 0xc02b9 vgarom.asm:523
    mov dx, 003dah                            ; ba da 03                    ; 0xc02ba vgarom.asm:525
    in AL, DX                                 ; ec                          ; 0xc02bd vgarom.asm:526
    pop dx                                    ; 5a                          ; 0xc02be vgarom.asm:528
    pop ax                                    ; 58                          ; 0xc02bf vgarom.asm:529
    retn                                      ; c3                          ; 0xc02c0 vgarom.asm:531
    push ax                                   ; 50                          ; 0xc02c1 vgarom.asm:536
    push bx                                   ; 53                          ; 0xc02c2 vgarom.asm:537
    mov BL, strict byte 011h                  ; b3 11                       ; 0xc02c3 vgarom.asm:538
    call 00299h                               ; e8 d1 ff                    ; 0xc02c5 vgarom.asm:539
    db  08ah, 0c7h
    ; mov al, bh                                ; 8a c7                     ; 0xc02c8 vgarom.asm:540
    pop bx                                    ; 5b                          ; 0xc02ca vgarom.asm:541
    db  08ah, 0f8h
    ; mov bh, al                                ; 8a f8                     ; 0xc02cb vgarom.asm:542
    pop ax                                    ; 58                          ; 0xc02cd vgarom.asm:543
    retn                                      ; c3                          ; 0xc02ce vgarom.asm:544
    push ax                                   ; 50                          ; 0xc02cf vgarom.asm:549
    push bx                                   ; 53                          ; 0xc02d0 vgarom.asm:550
    push cx                                   ; 51                          ; 0xc02d1 vgarom.asm:551
    push dx                                   ; 52                          ; 0xc02d2 vgarom.asm:552
    db  08bh, 0dah
    ; mov bx, dx                                ; 8b da                     ; 0xc02d3 vgarom.asm:553
    mov CL, strict byte 000h                  ; b1 00                       ; 0xc02d5 vgarom.asm:554
    mov dx, 003dah                            ; ba da 03                    ; 0xc02d7 vgarom.asm:556
    in AL, DX                                 ; ec                          ; 0xc02da vgarom.asm:557
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc02db vgarom.asm:558
    db  08ah, 0c1h
    ; mov al, cl                                ; 8a c1                     ; 0xc02de vgarom.asm:559
    out DX, AL                                ; ee                          ; 0xc02e0 vgarom.asm:560
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc02e1 vgarom.asm:561
    in AL, DX                                 ; ec                          ; 0xc02e4 vgarom.asm:562
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc02e5 vgarom.asm:563
    inc bx                                    ; 43                          ; 0xc02e8 vgarom.asm:564
    db  0feh, 0c1h
    ; inc cl                                    ; fe c1                     ; 0xc02e9 vgarom.asm:565
    cmp cl, 010h                              ; 80 f9 10                    ; 0xc02eb vgarom.asm:566
    jne short 002d7h                          ; 75 e7                       ; 0xc02ee vgarom.asm:567
    mov dx, 003dah                            ; ba da 03                    ; 0xc02f0 vgarom.asm:568
    in AL, DX                                 ; ec                          ; 0xc02f3 vgarom.asm:569
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc02f4 vgarom.asm:570
    mov AL, strict byte 011h                  ; b0 11                       ; 0xc02f7 vgarom.asm:571
    out DX, AL                                ; ee                          ; 0xc02f9 vgarom.asm:572
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc02fa vgarom.asm:573
    in AL, DX                                 ; ec                          ; 0xc02fd vgarom.asm:574
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc02fe vgarom.asm:575
    mov dx, 003dah                            ; ba da 03                    ; 0xc0301 vgarom.asm:576
    in AL, DX                                 ; ec                          ; 0xc0304 vgarom.asm:577
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0305 vgarom.asm:578
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc0308 vgarom.asm:579
    out DX, AL                                ; ee                          ; 0xc030a vgarom.asm:580
    mov dx, 003dah                            ; ba da 03                    ; 0xc030b vgarom.asm:582
    in AL, DX                                 ; ec                          ; 0xc030e vgarom.asm:583
    pop dx                                    ; 5a                          ; 0xc030f vgarom.asm:585
    pop cx                                    ; 59                          ; 0xc0310 vgarom.asm:586
    pop bx                                    ; 5b                          ; 0xc0311 vgarom.asm:587
    pop ax                                    ; 58                          ; 0xc0312 vgarom.asm:588
    retn                                      ; c3                          ; 0xc0313 vgarom.asm:589
    push ax                                   ; 50                          ; 0xc0314 vgarom.asm:594
    push dx                                   ; 52                          ; 0xc0315 vgarom.asm:595
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc0316 vgarom.asm:596
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc0319 vgarom.asm:597
    out DX, AL                                ; ee                          ; 0xc031b vgarom.asm:598
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc031c vgarom.asm:599
    pop ax                                    ; 58                          ; 0xc031f vgarom.asm:600
    push ax                                   ; 50                          ; 0xc0320 vgarom.asm:601
    db  08ah, 0c4h
    ; mov al, ah                                ; 8a c4                     ; 0xc0321 vgarom.asm:602
    out DX, AL                                ; ee                          ; 0xc0323 vgarom.asm:603
    db  08ah, 0c5h
    ; mov al, ch                                ; 8a c5                     ; 0xc0324 vgarom.asm:604
    out DX, AL                                ; ee                          ; 0xc0326 vgarom.asm:605
    db  08ah, 0c1h
    ; mov al, cl                                ; 8a c1                     ; 0xc0327 vgarom.asm:606
    out DX, AL                                ; ee                          ; 0xc0329 vgarom.asm:607
    pop dx                                    ; 5a                          ; 0xc032a vgarom.asm:608
    pop ax                                    ; 58                          ; 0xc032b vgarom.asm:609
    retn                                      ; c3                          ; 0xc032c vgarom.asm:610
    push ax                                   ; 50                          ; 0xc032d vgarom.asm:615
    push bx                                   ; 53                          ; 0xc032e vgarom.asm:616
    push cx                                   ; 51                          ; 0xc032f vgarom.asm:617
    push dx                                   ; 52                          ; 0xc0330 vgarom.asm:618
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc0331 vgarom.asm:619
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc0334 vgarom.asm:620
    out DX, AL                                ; ee                          ; 0xc0336 vgarom.asm:621
    pop dx                                    ; 5a                          ; 0xc0337 vgarom.asm:622
    push dx                                   ; 52                          ; 0xc0338 vgarom.asm:623
    db  08bh, 0dah
    ; mov bx, dx                                ; 8b da                     ; 0xc0339 vgarom.asm:624
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc033b vgarom.asm:625
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc033e vgarom.asm:627
    out DX, AL                                ; ee                          ; 0xc0341 vgarom.asm:628
    inc bx                                    ; 43                          ; 0xc0342 vgarom.asm:629
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0343 vgarom.asm:630
    out DX, AL                                ; ee                          ; 0xc0346 vgarom.asm:631
    inc bx                                    ; 43                          ; 0xc0347 vgarom.asm:632
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0348 vgarom.asm:633
    out DX, AL                                ; ee                          ; 0xc034b vgarom.asm:634
    inc bx                                    ; 43                          ; 0xc034c vgarom.asm:635
    dec cx                                    ; 49                          ; 0xc034d vgarom.asm:636
    jne short 0033eh                          ; 75 ee                       ; 0xc034e vgarom.asm:637
    pop dx                                    ; 5a                          ; 0xc0350 vgarom.asm:638
    pop cx                                    ; 59                          ; 0xc0351 vgarom.asm:639
    pop bx                                    ; 5b                          ; 0xc0352 vgarom.asm:640
    pop ax                                    ; 58                          ; 0xc0353 vgarom.asm:641
    retn                                      ; c3                          ; 0xc0354 vgarom.asm:642
    push ax                                   ; 50                          ; 0xc0355 vgarom.asm:647
    push bx                                   ; 53                          ; 0xc0356 vgarom.asm:648
    push dx                                   ; 52                          ; 0xc0357 vgarom.asm:649
    mov dx, 003dah                            ; ba da 03                    ; 0xc0358 vgarom.asm:650
    in AL, DX                                 ; ec                          ; 0xc035b vgarom.asm:651
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc035c vgarom.asm:652
    mov AL, strict byte 010h                  ; b0 10                       ; 0xc035f vgarom.asm:653
    out DX, AL                                ; ee                          ; 0xc0361 vgarom.asm:654
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc0362 vgarom.asm:655
    in AL, DX                                 ; ec                          ; 0xc0365 vgarom.asm:656
    and bl, 001h                              ; 80 e3 01                    ; 0xc0366 vgarom.asm:657
    jne short 00383h                          ; 75 18                       ; 0xc0369 vgarom.asm:658
    and AL, strict byte 07fh                  ; 24 7f                       ; 0xc036b vgarom.asm:659
    sal bh, 1                                 ; d0 e7                       ; 0xc036d vgarom.asm:663
    sal bh, 1                                 ; d0 e7                       ; 0xc036f vgarom.asm:664
    sal bh, 1                                 ; d0 e7                       ; 0xc0371 vgarom.asm:665
    sal bh, 1                                 ; d0 e7                       ; 0xc0373 vgarom.asm:666
    sal bh, 1                                 ; d0 e7                       ; 0xc0375 vgarom.asm:667
    sal bh, 1                                 ; d0 e7                       ; 0xc0377 vgarom.asm:668
    sal bh, 1                                 ; d0 e7                       ; 0xc0379 vgarom.asm:669
    db  00ah, 0c7h
    ; or al, bh                                 ; 0a c7                     ; 0xc037b vgarom.asm:671
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc037d vgarom.asm:672
    out DX, AL                                ; ee                          ; 0xc0380 vgarom.asm:673
    jmp short 0039dh                          ; eb 1a                       ; 0xc0381 vgarom.asm:674
    push ax                                   ; 50                          ; 0xc0383 vgarom.asm:676
    mov dx, 003dah                            ; ba da 03                    ; 0xc0384 vgarom.asm:677
    in AL, DX                                 ; ec                          ; 0xc0387 vgarom.asm:678
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0388 vgarom.asm:679
    mov AL, strict byte 014h                  ; b0 14                       ; 0xc038b vgarom.asm:680
    out DX, AL                                ; ee                          ; 0xc038d vgarom.asm:681
    pop ax                                    ; 58                          ; 0xc038e vgarom.asm:682
    and AL, strict byte 080h                  ; 24 80                       ; 0xc038f vgarom.asm:683
    jne short 00397h                          ; 75 04                       ; 0xc0391 vgarom.asm:684
    sal bh, 1                                 ; d0 e7                       ; 0xc0393 vgarom.asm:688
    sal bh, 1                                 ; d0 e7                       ; 0xc0395 vgarom.asm:689
    and bh, 00fh                              ; 80 e7 0f                    ; 0xc0397 vgarom.asm:692
    db  08ah, 0c7h
    ; mov al, bh                                ; 8a c7                     ; 0xc039a vgarom.asm:693
    out DX, AL                                ; ee                          ; 0xc039c vgarom.asm:694
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc039d vgarom.asm:696
    out DX, AL                                ; ee                          ; 0xc039f vgarom.asm:697
    mov dx, 003dah                            ; ba da 03                    ; 0xc03a0 vgarom.asm:699
    in AL, DX                                 ; ec                          ; 0xc03a3 vgarom.asm:700
    pop dx                                    ; 5a                          ; 0xc03a4 vgarom.asm:702
    pop bx                                    ; 5b                          ; 0xc03a5 vgarom.asm:703
    pop ax                                    ; 58                          ; 0xc03a6 vgarom.asm:704
    retn                                      ; c3                          ; 0xc03a7 vgarom.asm:705
    push ax                                   ; 50                          ; 0xc03a8 vgarom.asm:710
    push dx                                   ; 52                          ; 0xc03a9 vgarom.asm:711
    mov dx, 003c7h                            ; ba c7 03                    ; 0xc03aa vgarom.asm:712
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc03ad vgarom.asm:713
    out DX, AL                                ; ee                          ; 0xc03af vgarom.asm:714
    pop ax                                    ; 58                          ; 0xc03b0 vgarom.asm:715
    db  08ah, 0e0h
    ; mov ah, al                                ; 8a e0                     ; 0xc03b1 vgarom.asm:716
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc03b3 vgarom.asm:717
    in AL, DX                                 ; ec                          ; 0xc03b6 vgarom.asm:718
    xchg al, ah                               ; 86 e0                       ; 0xc03b7 vgarom.asm:719
    push ax                                   ; 50                          ; 0xc03b9 vgarom.asm:720
    in AL, DX                                 ; ec                          ; 0xc03ba vgarom.asm:721
    db  08ah, 0e8h
    ; mov ch, al                                ; 8a e8                     ; 0xc03bb vgarom.asm:722
    in AL, DX                                 ; ec                          ; 0xc03bd vgarom.asm:723
    db  08ah, 0c8h
    ; mov cl, al                                ; 8a c8                     ; 0xc03be vgarom.asm:724
    pop dx                                    ; 5a                          ; 0xc03c0 vgarom.asm:725
    pop ax                                    ; 58                          ; 0xc03c1 vgarom.asm:726
    retn                                      ; c3                          ; 0xc03c2 vgarom.asm:727
    push ax                                   ; 50                          ; 0xc03c3 vgarom.asm:732
    push bx                                   ; 53                          ; 0xc03c4 vgarom.asm:733
    push cx                                   ; 51                          ; 0xc03c5 vgarom.asm:734
    push dx                                   ; 52                          ; 0xc03c6 vgarom.asm:735
    mov dx, 003c7h                            ; ba c7 03                    ; 0xc03c7 vgarom.asm:736
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc03ca vgarom.asm:737
    out DX, AL                                ; ee                          ; 0xc03cc vgarom.asm:738
    pop dx                                    ; 5a                          ; 0xc03cd vgarom.asm:739
    push dx                                   ; 52                          ; 0xc03ce vgarom.asm:740
    db  08bh, 0dah
    ; mov bx, dx                                ; 8b da                     ; 0xc03cf vgarom.asm:741
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc03d1 vgarom.asm:742
    in AL, DX                                 ; ec                          ; 0xc03d4 vgarom.asm:744
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc03d5 vgarom.asm:745
    inc bx                                    ; 43                          ; 0xc03d8 vgarom.asm:746
    in AL, DX                                 ; ec                          ; 0xc03d9 vgarom.asm:747
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc03da vgarom.asm:748
    inc bx                                    ; 43                          ; 0xc03dd vgarom.asm:749
    in AL, DX                                 ; ec                          ; 0xc03de vgarom.asm:750
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc03df vgarom.asm:751
    inc bx                                    ; 43                          ; 0xc03e2 vgarom.asm:752
    dec cx                                    ; 49                          ; 0xc03e3 vgarom.asm:753
    jne short 003d4h                          ; 75 ee                       ; 0xc03e4 vgarom.asm:754
    pop dx                                    ; 5a                          ; 0xc03e6 vgarom.asm:755
    pop cx                                    ; 59                          ; 0xc03e7 vgarom.asm:756
    pop bx                                    ; 5b                          ; 0xc03e8 vgarom.asm:757
    pop ax                                    ; 58                          ; 0xc03e9 vgarom.asm:758
    retn                                      ; c3                          ; 0xc03ea vgarom.asm:759
    push ax                                   ; 50                          ; 0xc03eb vgarom.asm:764
    push dx                                   ; 52                          ; 0xc03ec vgarom.asm:765
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc03ed vgarom.asm:766
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc03f0 vgarom.asm:767
    out DX, AL                                ; ee                          ; 0xc03f2 vgarom.asm:768
    pop dx                                    ; 5a                          ; 0xc03f3 vgarom.asm:769
    pop ax                                    ; 58                          ; 0xc03f4 vgarom.asm:770
    retn                                      ; c3                          ; 0xc03f5 vgarom.asm:771
    push ax                                   ; 50                          ; 0xc03f6 vgarom.asm:776
    push dx                                   ; 52                          ; 0xc03f7 vgarom.asm:777
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc03f8 vgarom.asm:778
    in AL, DX                                 ; ec                          ; 0xc03fb vgarom.asm:779
    db  08ah, 0d8h
    ; mov bl, al                                ; 8a d8                     ; 0xc03fc vgarom.asm:780
    pop dx                                    ; 5a                          ; 0xc03fe vgarom.asm:781
    pop ax                                    ; 58                          ; 0xc03ff vgarom.asm:782
    retn                                      ; c3                          ; 0xc0400 vgarom.asm:783
    push ax                                   ; 50                          ; 0xc0401 vgarom.asm:788
    push dx                                   ; 52                          ; 0xc0402 vgarom.asm:789
    mov dx, 003dah                            ; ba da 03                    ; 0xc0403 vgarom.asm:790
    in AL, DX                                 ; ec                          ; 0xc0406 vgarom.asm:791
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0407 vgarom.asm:792
    mov AL, strict byte 010h                  ; b0 10                       ; 0xc040a vgarom.asm:793
    out DX, AL                                ; ee                          ; 0xc040c vgarom.asm:794
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc040d vgarom.asm:795
    in AL, DX                                 ; ec                          ; 0xc0410 vgarom.asm:796
    db  08ah, 0d8h
    ; mov bl, al                                ; 8a d8                     ; 0xc0411 vgarom.asm:797
    shr bl, 1                                 ; d0 eb                       ; 0xc0413 vgarom.asm:801
    shr bl, 1                                 ; d0 eb                       ; 0xc0415 vgarom.asm:802
    shr bl, 1                                 ; d0 eb                       ; 0xc0417 vgarom.asm:803
    shr bl, 1                                 ; d0 eb                       ; 0xc0419 vgarom.asm:804
    shr bl, 1                                 ; d0 eb                       ; 0xc041b vgarom.asm:805
    shr bl, 1                                 ; d0 eb                       ; 0xc041d vgarom.asm:806
    shr bl, 1                                 ; d0 eb                       ; 0xc041f vgarom.asm:807
    mov dx, 003dah                            ; ba da 03                    ; 0xc0421 vgarom.asm:809
    in AL, DX                                 ; ec                          ; 0xc0424 vgarom.asm:810
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0425 vgarom.asm:811
    mov AL, strict byte 014h                  ; b0 14                       ; 0xc0428 vgarom.asm:812
    out DX, AL                                ; ee                          ; 0xc042a vgarom.asm:813
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc042b vgarom.asm:814
    in AL, DX                                 ; ec                          ; 0xc042e vgarom.asm:815
    db  08ah, 0f8h
    ; mov bh, al                                ; 8a f8                     ; 0xc042f vgarom.asm:816
    and bh, 00fh                              ; 80 e7 0f                    ; 0xc0431 vgarom.asm:817
    test bl, 001h                             ; f6 c3 01                    ; 0xc0434 vgarom.asm:818
    jne short 0043dh                          ; 75 04                       ; 0xc0437 vgarom.asm:819
    shr bh, 1                                 ; d0 ef                       ; 0xc0439 vgarom.asm:823
    shr bh, 1                                 ; d0 ef                       ; 0xc043b vgarom.asm:824
    mov dx, 003dah                            ; ba da 03                    ; 0xc043d vgarom.asm:827
    in AL, DX                                 ; ec                          ; 0xc0440 vgarom.asm:828
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0441 vgarom.asm:829
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc0444 vgarom.asm:830
    out DX, AL                                ; ee                          ; 0xc0446 vgarom.asm:831
    mov dx, 003dah                            ; ba da 03                    ; 0xc0447 vgarom.asm:833
    in AL, DX                                 ; ec                          ; 0xc044a vgarom.asm:834
    pop dx                                    ; 5a                          ; 0xc044b vgarom.asm:836
    pop ax                                    ; 58                          ; 0xc044c vgarom.asm:837
    retn                                      ; c3                          ; 0xc044d vgarom.asm:838
    push ax                                   ; 50                          ; 0xc044e vgarom.asm:843
    push dx                                   ; 52                          ; 0xc044f vgarom.asm:844
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc0450 vgarom.asm:845
    db  08ah, 0e3h
    ; mov ah, bl                                ; 8a e3                     ; 0xc0453 vgarom.asm:846
    mov AL, strict byte 003h                  ; b0 03                       ; 0xc0455 vgarom.asm:847
    out DX, ax                                ; ef                          ; 0xc0457 vgarom.asm:848
    pop dx                                    ; 5a                          ; 0xc0458 vgarom.asm:849
    pop ax                                    ; 58                          ; 0xc0459 vgarom.asm:850
    retn                                      ; c3                          ; 0xc045a vgarom.asm:851
    push DS                                   ; 1e                          ; 0xc045b vgarom.asm:856
    push ax                                   ; 50                          ; 0xc045c vgarom.asm:857
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc045d vgarom.asm:858
    mov ds, ax                                ; 8e d8                       ; 0xc0460 vgarom.asm:859
    db  032h, 0edh
    ; xor ch, ch                                ; 32 ed                     ; 0xc0462 vgarom.asm:860
    mov bx, 00088h                            ; bb 88 00                    ; 0xc0464 vgarom.asm:861
    mov cl, byte [bx]                         ; 8a 0f                       ; 0xc0467 vgarom.asm:862
    and cl, 00fh                              ; 80 e1 0f                    ; 0xc0469 vgarom.asm:863
    mov bx, strict word 00063h                ; bb 63 00                    ; 0xc046c vgarom.asm:864
    mov ax, word [bx]                         ; 8b 07                       ; 0xc046f vgarom.asm:865
    mov bx, strict word 00003h                ; bb 03 00                    ; 0xc0471 vgarom.asm:866
    cmp ax, 003b4h                            ; 3d b4 03                    ; 0xc0474 vgarom.asm:867
    jne short 0047bh                          ; 75 02                       ; 0xc0477 vgarom.asm:868
    mov BH, strict byte 001h                  ; b7 01                       ; 0xc0479 vgarom.asm:869
    pop ax                                    ; 58                          ; 0xc047b vgarom.asm:871
    pop DS                                    ; 1f                          ; 0xc047c vgarom.asm:872
    retn                                      ; c3                          ; 0xc047d vgarom.asm:873
    push DS                                   ; 1e                          ; 0xc047e vgarom.asm:881
    push bx                                   ; 53                          ; 0xc047f vgarom.asm:882
    push dx                                   ; 52                          ; 0xc0480 vgarom.asm:883
    db  08ah, 0d0h
    ; mov dl, al                                ; 8a d0                     ; 0xc0481 vgarom.asm:884
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0483 vgarom.asm:885
    mov ds, ax                                ; 8e d8                       ; 0xc0486 vgarom.asm:886
    mov bx, 00089h                            ; bb 89 00                    ; 0xc0488 vgarom.asm:887
    mov al, byte [bx]                         ; 8a 07                       ; 0xc048b vgarom.asm:888
    mov bx, 00088h                            ; bb 88 00                    ; 0xc048d vgarom.asm:889
    mov ah, byte [bx]                         ; 8a 27                       ; 0xc0490 vgarom.asm:890
    cmp dl, 001h                              ; 80 fa 01                    ; 0xc0492 vgarom.asm:891
    je short 004ach                           ; 74 15                       ; 0xc0495 vgarom.asm:892
    jc short 004b6h                           ; 72 1d                       ; 0xc0497 vgarom.asm:893
    cmp dl, 002h                              ; 80 fa 02                    ; 0xc0499 vgarom.asm:894
    je short 004a0h                           ; 74 02                       ; 0xc049c vgarom.asm:895
    jmp short 004cah                          ; eb 2a                       ; 0xc049e vgarom.asm:905
    and AL, strict byte 07fh                  ; 24 7f                       ; 0xc04a0 vgarom.asm:911
    or AL, strict byte 010h                   ; 0c 10                       ; 0xc04a2 vgarom.asm:912
    and ah, 0f0h                              ; 80 e4 f0                    ; 0xc04a4 vgarom.asm:913
    or ah, 009h                               ; 80 cc 09                    ; 0xc04a7 vgarom.asm:914
    jne short 004c0h                          ; 75 14                       ; 0xc04aa vgarom.asm:915
    and AL, strict byte 06fh                  ; 24 6f                       ; 0xc04ac vgarom.asm:921
    and ah, 0f0h                              ; 80 e4 f0                    ; 0xc04ae vgarom.asm:922
    or ah, 009h                               ; 80 cc 09                    ; 0xc04b1 vgarom.asm:923
    jne short 004c0h                          ; 75 0a                       ; 0xc04b4 vgarom.asm:924
    and AL, strict byte 0efh                  ; 24 ef                       ; 0xc04b6 vgarom.asm:930
    or AL, strict byte 080h                   ; 0c 80                       ; 0xc04b8 vgarom.asm:931
    and ah, 0f0h                              ; 80 e4 f0                    ; 0xc04ba vgarom.asm:932
    or ah, 008h                               ; 80 cc 08                    ; 0xc04bd vgarom.asm:933
    mov bx, 00089h                            ; bb 89 00                    ; 0xc04c0 vgarom.asm:935
    mov byte [bx], al                         ; 88 07                       ; 0xc04c3 vgarom.asm:936
    mov bx, 00088h                            ; bb 88 00                    ; 0xc04c5 vgarom.asm:937
    mov byte [bx], ah                         ; 88 27                       ; 0xc04c8 vgarom.asm:938
    mov ax, 01212h                            ; b8 12 12                    ; 0xc04ca vgarom.asm:940
    pop dx                                    ; 5a                          ; 0xc04cd vgarom.asm:941
    pop bx                                    ; 5b                          ; 0xc04ce vgarom.asm:942
    pop DS                                    ; 1f                          ; 0xc04cf vgarom.asm:943
    retn                                      ; c3                          ; 0xc04d0 vgarom.asm:944
    push DS                                   ; 1e                          ; 0xc04d1 vgarom.asm:953
    push bx                                   ; 53                          ; 0xc04d2 vgarom.asm:954
    push dx                                   ; 52                          ; 0xc04d3 vgarom.asm:955
    db  08ah, 0d0h
    ; mov dl, al                                ; 8a d0                     ; 0xc04d4 vgarom.asm:956
    and dl, 001h                              ; 80 e2 01                    ; 0xc04d6 vgarom.asm:957
    sal dl, 1                                 ; d0 e2                       ; 0xc04d9 vgarom.asm:961
    sal dl, 1                                 ; d0 e2                       ; 0xc04db vgarom.asm:962
    sal dl, 1                                 ; d0 e2                       ; 0xc04dd vgarom.asm:963
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc04df vgarom.asm:965
    mov ds, ax                                ; 8e d8                       ; 0xc04e2 vgarom.asm:966
    mov bx, 00089h                            ; bb 89 00                    ; 0xc04e4 vgarom.asm:967
    mov al, byte [bx]                         ; 8a 07                       ; 0xc04e7 vgarom.asm:968
    and AL, strict byte 0f7h                  ; 24 f7                       ; 0xc04e9 vgarom.asm:969
    db  00ah, 0c2h
    ; or al, dl                                 ; 0a c2                     ; 0xc04eb vgarom.asm:970
    mov byte [bx], al                         ; 88 07                       ; 0xc04ed vgarom.asm:971
    mov ax, 01212h                            ; b8 12 12                    ; 0xc04ef vgarom.asm:972
    pop dx                                    ; 5a                          ; 0xc04f2 vgarom.asm:973
    pop bx                                    ; 5b                          ; 0xc04f3 vgarom.asm:974
    pop DS                                    ; 1f                          ; 0xc04f4 vgarom.asm:975
    retn                                      ; c3                          ; 0xc04f5 vgarom.asm:976
    push bx                                   ; 53                          ; 0xc04f6 vgarom.asm:980
    push dx                                   ; 52                          ; 0xc04f7 vgarom.asm:981
    db  08ah, 0d8h
    ; mov bl, al                                ; 8a d8                     ; 0xc04f8 vgarom.asm:982
    and bl, 001h                              ; 80 e3 01                    ; 0xc04fa vgarom.asm:983
    xor bl, 001h                              ; 80 f3 01                    ; 0xc04fd vgarom.asm:984
    sal bl, 1                                 ; d0 e3                       ; 0xc0500 vgarom.asm:985
    mov dx, 003cch                            ; ba cc 03                    ; 0xc0502 vgarom.asm:986
    in AL, DX                                 ; ec                          ; 0xc0505 vgarom.asm:987
    and AL, strict byte 0fdh                  ; 24 fd                       ; 0xc0506 vgarom.asm:988
    db  00ah, 0c3h
    ; or al, bl                                 ; 0a c3                     ; 0xc0508 vgarom.asm:989
    mov dx, 003c2h                            ; ba c2 03                    ; 0xc050a vgarom.asm:990
    out DX, AL                                ; ee                          ; 0xc050d vgarom.asm:991
    mov ax, 01212h                            ; b8 12 12                    ; 0xc050e vgarom.asm:992
    pop dx                                    ; 5a                          ; 0xc0511 vgarom.asm:993
    pop bx                                    ; 5b                          ; 0xc0512 vgarom.asm:994
    retn                                      ; c3                          ; 0xc0513 vgarom.asm:995
    push DS                                   ; 1e                          ; 0xc0514 vgarom.asm:999
    push bx                                   ; 53                          ; 0xc0515 vgarom.asm:1000
    push dx                                   ; 52                          ; 0xc0516 vgarom.asm:1001
    db  08ah, 0d0h
    ; mov dl, al                                ; 8a d0                     ; 0xc0517 vgarom.asm:1002
    and dl, 001h                              ; 80 e2 01                    ; 0xc0519 vgarom.asm:1003
    xor dl, 001h                              ; 80 f2 01                    ; 0xc051c vgarom.asm:1004
    sal dl, 1                                 ; d0 e2                       ; 0xc051f vgarom.asm:1005
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0521 vgarom.asm:1006
    mov ds, ax                                ; 8e d8                       ; 0xc0524 vgarom.asm:1007
    mov bx, 00089h                            ; bb 89 00                    ; 0xc0526 vgarom.asm:1008
    mov al, byte [bx]                         ; 8a 07                       ; 0xc0529 vgarom.asm:1009
    and AL, strict byte 0fdh                  ; 24 fd                       ; 0xc052b vgarom.asm:1010
    db  00ah, 0c2h
    ; or al, dl                                 ; 0a c2                     ; 0xc052d vgarom.asm:1011
    mov byte [bx], al                         ; 88 07                       ; 0xc052f vgarom.asm:1012
    mov ax, 01212h                            ; b8 12 12                    ; 0xc0531 vgarom.asm:1013
    pop dx                                    ; 5a                          ; 0xc0534 vgarom.asm:1014
    pop bx                                    ; 5b                          ; 0xc0535 vgarom.asm:1015
    pop DS                                    ; 1f                          ; 0xc0536 vgarom.asm:1016
    retn                                      ; c3                          ; 0xc0537 vgarom.asm:1017
    push DS                                   ; 1e                          ; 0xc0538 vgarom.asm:1021
    push bx                                   ; 53                          ; 0xc0539 vgarom.asm:1022
    push dx                                   ; 52                          ; 0xc053a vgarom.asm:1023
    db  08ah, 0d0h
    ; mov dl, al                                ; 8a d0                     ; 0xc053b vgarom.asm:1024
    and dl, 001h                              ; 80 e2 01                    ; 0xc053d vgarom.asm:1025
    xor dl, 001h                              ; 80 f2 01                    ; 0xc0540 vgarom.asm:1026
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0543 vgarom.asm:1027
    mov ds, ax                                ; 8e d8                       ; 0xc0546 vgarom.asm:1028
    mov bx, 00089h                            ; bb 89 00                    ; 0xc0548 vgarom.asm:1029
    mov al, byte [bx]                         ; 8a 07                       ; 0xc054b vgarom.asm:1030
    and AL, strict byte 0feh                  ; 24 fe                       ; 0xc054d vgarom.asm:1031
    db  00ah, 0c2h
    ; or al, dl                                 ; 0a c2                     ; 0xc054f vgarom.asm:1032
    mov byte [bx], al                         ; 88 07                       ; 0xc0551 vgarom.asm:1033
    mov ax, 01212h                            ; b8 12 12                    ; 0xc0553 vgarom.asm:1034
    pop dx                                    ; 5a                          ; 0xc0556 vgarom.asm:1035
    pop bx                                    ; 5b                          ; 0xc0557 vgarom.asm:1036
    pop DS                                    ; 1f                          ; 0xc0558 vgarom.asm:1037
    retn                                      ; c3                          ; 0xc0559 vgarom.asm:1038
    cmp AL, strict byte 000h                  ; 3c 00                       ; 0xc055a vgarom.asm:1043
    je short 00563h                           ; 74 05                       ; 0xc055c vgarom.asm:1044
    cmp AL, strict byte 001h                  ; 3c 01                       ; 0xc055e vgarom.asm:1045
    je short 00578h                           ; 74 16                       ; 0xc0560 vgarom.asm:1046
    retn                                      ; c3                          ; 0xc0562 vgarom.asm:1050
    push DS                                   ; 1e                          ; 0xc0563 vgarom.asm:1052
    push ax                                   ; 50                          ; 0xc0564 vgarom.asm:1053
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0565 vgarom.asm:1054
    mov ds, ax                                ; 8e d8                       ; 0xc0568 vgarom.asm:1055
    mov bx, 0008ah                            ; bb 8a 00                    ; 0xc056a vgarom.asm:1056
    mov al, byte [bx]                         ; 8a 07                       ; 0xc056d vgarom.asm:1057
    db  08ah, 0d8h
    ; mov bl, al                                ; 8a d8                     ; 0xc056f vgarom.asm:1058
    db  032h, 0ffh
    ; xor bh, bh                                ; 32 ff                     ; 0xc0571 vgarom.asm:1059
    pop ax                                    ; 58                          ; 0xc0573 vgarom.asm:1060
    db  08ah, 0c4h
    ; mov al, ah                                ; 8a c4                     ; 0xc0574 vgarom.asm:1061
    pop DS                                    ; 1f                          ; 0xc0576 vgarom.asm:1062
    retn                                      ; c3                          ; 0xc0577 vgarom.asm:1063
    push DS                                   ; 1e                          ; 0xc0578 vgarom.asm:1065
    push ax                                   ; 50                          ; 0xc0579 vgarom.asm:1066
    push bx                                   ; 53                          ; 0xc057a vgarom.asm:1067
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc057b vgarom.asm:1068
    mov ds, ax                                ; 8e d8                       ; 0xc057e vgarom.asm:1069
    db  08bh, 0c3h
    ; mov ax, bx                                ; 8b c3                     ; 0xc0580 vgarom.asm:1070
    mov bx, 0008ah                            ; bb 8a 00                    ; 0xc0582 vgarom.asm:1071
    mov byte [bx], al                         ; 88 07                       ; 0xc0585 vgarom.asm:1072
    pop bx                                    ; 5b                          ; 0xc0587 vgarom.asm:1082
    pop ax                                    ; 58                          ; 0xc0588 vgarom.asm:1083
    db  08ah, 0c4h
    ; mov al, ah                                ; 8a c4                     ; 0xc0589 vgarom.asm:1084
    pop DS                                    ; 1f                          ; 0xc058b vgarom.asm:1085
    retn                                      ; c3                          ; 0xc058c vgarom.asm:1086
    times 0x3 db 0
  ; disGetNextSymbol 0xc0590 LB 0x3af -> off=0x0 cb=0000000000000007 uValue=00000000000c0590 'do_out_dx_ax'
do_out_dx_ax:                                ; 0xc0590 LB 0x7
    xchg ah, al                               ; 86 c4                       ; 0xc0590 vberom.asm:69
    out DX, AL                                ; ee                          ; 0xc0592 vberom.asm:70
    xchg ah, al                               ; 86 c4                       ; 0xc0593 vberom.asm:71
    out DX, AL                                ; ee                          ; 0xc0595 vberom.asm:72
    retn                                      ; c3                          ; 0xc0596 vberom.asm:73
  ; disGetNextSymbol 0xc0597 LB 0x3a8 -> off=0x0 cb=0000000000000043 uValue=00000000000c0597 'do_in_ax_dx'
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
  ; disGetNextSymbol 0xc05da LB 0x365 -> off=0x0 cb=0000000000000026 uValue=00000000000c05da '_dispi_get_max_bpp'
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
  ; disGetNextSymbol 0xc0600 LB 0x33f -> off=0x0 cb=0000000000000026 uValue=00000000000c0600 'dispi_set_enable_'
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
  ; disGetNextSymbol 0xc0626 LB 0x319 -> off=0x0 cb=0000000000000026 uValue=00000000000c0626 'dispi_set_bank_'
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
  ; disGetNextSymbol 0xc064c LB 0x2f3 -> off=0x0 cb=00000000000000ac uValue=00000000000c064c '_dispi_set_bank_farcall'
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
  ; disGetNextSymbol 0xc06f8 LB 0x247 -> off=0x0 cb=00000000000000f0 uValue=00000000000c06f8 '_vga_compat_setup'
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
  ; disGetNextSymbol 0xc07e8 LB 0x157 -> off=0x0 cb=0000000000000013 uValue=00000000000c07e8 '_vbe_has_vbe_display'
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
  ; disGetNextSymbol 0xc07fb LB 0x144 -> off=0x0 cb=0000000000000025 uValue=00000000000c07fb 'vbe_biosfn_return_current_mode'
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
  ; disGetNextSymbol 0xc0820 LB 0x11f -> off=0x0 cb=000000000000002d uValue=00000000000c0820 'vbe_biosfn_display_window_control'
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
  ; disGetNextSymbol 0xc084d LB 0xf2 -> off=0x0 cb=0000000000000034 uValue=00000000000c084d 'vbe_biosfn_set_get_display_start'
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
  ; disGetNextSymbol 0xc0881 LB 0xbe -> off=0x0 cb=0000000000000037 uValue=00000000000c0881 'vbe_biosfn_set_get_dac_palette_format'
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
  ; disGetNextSymbol 0xc08b8 LB 0x87 -> off=0x0 cb=0000000000000073 uValue=00000000000c08b8 'vbe_biosfn_set_get_palette_data'
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
  ; disGetNextSymbol 0xc092b LB 0x14 -> off=0x0 cb=0000000000000014 uValue=00000000000c092b 'vbe_biosfn_return_protected_mode_interface'
vbe_biosfn_return_protected_mode_interface: ; 0xc092b LB 0x14
    test bl, bl                               ; 84 db                       ; 0xc092b vberom.asm:780
    jne short 0093bh                          ; 75 0c                       ; 0xc092d vberom.asm:781
    push CS                                   ; 0e                          ; 0xc092f vberom.asm:782
    pop ES                                    ; 07                          ; 0xc0930 vberom.asm:783
    mov di, 04600h                            ; bf 00 46                    ; 0xc0931 vberom.asm:784
    mov cx, 00115h                            ; b9 15 01                    ; 0xc0934 vberom.asm:785
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc0937 vberom.asm:786
    retn                                      ; c3                          ; 0xc093a vberom.asm:787
    mov ax, 0014fh                            ; b8 4f 01                    ; 0xc093b vberom.asm:789
    retn                                      ; c3                          ; 0xc093e vberom.asm:790

  ; Padding 0x41 bytes at 0xc093f
  times 65 db 0

section _TEXT progbits vstart=0x980 align=1 ; size=0x3914 class=CODE group=AUTO
  ; disGetNextSymbol 0xc0980 LB 0x3914 -> off=0x0 cb=000000000000001c uValue=00000000000c0980 'set_int_vector'
set_int_vector:                              ; 0xc0980 LB 0x1c
    push dx                                   ; 52                          ; 0xc0980 vgabios.c:88
    push bp                                   ; 55                          ; 0xc0981
    mov bp, sp                                ; 89 e5                       ; 0xc0982
    mov dx, bx                                ; 89 da                       ; 0xc0984
    mov bl, al                                ; 88 c3                       ; 0xc0986 vgabios.c:92
    xor bh, bh                                ; 30 ff                       ; 0xc0988
    sal bx, 1                                 ; d1 e3                       ; 0xc098a
    sal bx, 1                                 ; d1 e3                       ; 0xc098c
    xor ax, ax                                ; 31 c0                       ; 0xc098e
    mov es, ax                                ; 8e c0                       ; 0xc0990
    mov word [es:bx], dx                      ; 26 89 17                    ; 0xc0992
    mov word [es:bx+002h], cx                 ; 26 89 4f 02                 ; 0xc0995
    pop bp                                    ; 5d                          ; 0xc0999 vgabios.c:93
    pop dx                                    ; 5a                          ; 0xc099a
    retn                                      ; c3                          ; 0xc099b
  ; disGetNextSymbol 0xc099c LB 0x38f8 -> off=0x0 cb=000000000000001c uValue=00000000000c099c 'init_vga_card'
init_vga_card:                               ; 0xc099c LB 0x1c
    push bp                                   ; 55                          ; 0xc099c vgabios.c:144
    mov bp, sp                                ; 89 e5                       ; 0xc099d
    push dx                                   ; 52                          ; 0xc099f
    mov AL, strict byte 0c3h                  ; b0 c3                       ; 0xc09a0 vgabios.c:147
    mov dx, 003c2h                            ; ba c2 03                    ; 0xc09a2
    out DX, AL                                ; ee                          ; 0xc09a5
    mov AL, strict byte 004h                  ; b0 04                       ; 0xc09a6 vgabios.c:150
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc09a8
    out DX, AL                                ; ee                          ; 0xc09ab
    mov AL, strict byte 002h                  ; b0 02                       ; 0xc09ac vgabios.c:151
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc09ae
    out DX, AL                                ; ee                          ; 0xc09b1
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc09b2 vgabios.c:156
    pop dx                                    ; 5a                          ; 0xc09b5
    pop bp                                    ; 5d                          ; 0xc09b6
    retn                                      ; c3                          ; 0xc09b7
  ; disGetNextSymbol 0xc09b8 LB 0x38dc -> off=0x0 cb=0000000000000032 uValue=00000000000c09b8 'init_bios_area'
init_bios_area:                              ; 0xc09b8 LB 0x32
    push bx                                   ; 53                          ; 0xc09b8 vgabios.c:165
    push bp                                   ; 55                          ; 0xc09b9
    mov bp, sp                                ; 89 e5                       ; 0xc09ba
    xor bx, bx                                ; 31 db                       ; 0xc09bc vgabios.c:169
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc09be
    mov es, ax                                ; 8e c0                       ; 0xc09c1
    mov al, byte [es:bx+010h]                 ; 26 8a 47 10                 ; 0xc09c3 vgabios.c:172
    and AL, strict byte 0cfh                  ; 24 cf                       ; 0xc09c7
    or AL, strict byte 020h                   ; 0c 20                       ; 0xc09c9
    mov byte [es:bx+010h], al                 ; 26 88 47 10                 ; 0xc09cb
    mov byte [es:bx+00085h], 010h             ; 26 c6 87 85 00 10           ; 0xc09cf vgabios.c:176
    mov word [es:bx+00087h], 0f960h           ; 26 c7 87 87 00 60 f9        ; 0xc09d5 vgabios.c:178
    mov byte [es:bx+00089h], 051h             ; 26 c6 87 89 00 51           ; 0xc09dc vgabios.c:182
    mov byte [es:bx+065h], 009h               ; 26 c6 47 65 09              ; 0xc09e2 vgabios.c:184
    pop bp                                    ; 5d                          ; 0xc09e7 vgabios.c:185
    pop bx                                    ; 5b                          ; 0xc09e8
    retn                                      ; c3                          ; 0xc09e9
  ; disGetNextSymbol 0xc09ea LB 0x38aa -> off=0x0 cb=0000000000000031 uValue=00000000000c09ea 'vgabios_init_func'
vgabios_init_func:                           ; 0xc09ea LB 0x31
    inc bp                                    ; 45                          ; 0xc09ea vgabios.c:225
    push bp                                   ; 55                          ; 0xc09eb
    mov bp, sp                                ; 89 e5                       ; 0xc09ec
    call 0099ch                               ; e8 ab ff                    ; 0xc09ee vgabios.c:227
    call 009b8h                               ; e8 c4 ff                    ; 0xc09f1 vgabios.c:228
    call 03c1ch                               ; e8 25 32                    ; 0xc09f4 vgabios.c:230
    mov bx, strict word 00022h                ; bb 22 00                    ; 0xc09f7 vgabios.c:232
    mov cx, 0c000h                            ; b9 00 c0                    ; 0xc09fa
    mov ax, strict word 00010h                ; b8 10 00                    ; 0xc09fd
    call 00980h                               ; e8 7d ff                    ; 0xc0a00
    mov bx, strict word 00022h                ; bb 22 00                    ; 0xc0a03 vgabios.c:233
    mov cx, 0c000h                            ; b9 00 c0                    ; 0xc0a06
    mov ax, strict word 0006dh                ; b8 6d 00                    ; 0xc0a09
    call 00980h                               ; e8 71 ff                    ; 0xc0a0c
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc0a0f vgabios.c:259
    db  032h, 0e4h
    ; xor ah, ah                                ; 32 e4                     ; 0xc0a12
    int 010h                                  ; cd 10                       ; 0xc0a14
    mov sp, bp                                ; 89 ec                       ; 0xc0a16 vgabios.c:262
    pop bp                                    ; 5d                          ; 0xc0a18
    dec bp                                    ; 4d                          ; 0xc0a19
    retf                                      ; cb                          ; 0xc0a1a
  ; disGetNextSymbol 0xc0a1b LB 0x3879 -> off=0x0 cb=0000000000000040 uValue=00000000000c0a1b 'vga_get_cursor_pos'
vga_get_cursor_pos:                          ; 0xc0a1b LB 0x40
    push si                                   ; 56                          ; 0xc0a1b vgabios.c:331
    push di                                   ; 57                          ; 0xc0a1c
    push bp                                   ; 55                          ; 0xc0a1d
    mov bp, sp                                ; 89 e5                       ; 0xc0a1e
    mov si, dx                                ; 89 d6                       ; 0xc0a20
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc0a22 vgabios.c:333
    jbe short 00a34h                          ; 76 0e                       ; 0xc0a24
    push SS                                   ; 16                          ; 0xc0a26 vgabios.c:334
    pop ES                                    ; 07                          ; 0xc0a27
    mov word [es:si], strict word 00000h      ; 26 c7 04 00 00              ; 0xc0a28
    mov word [es:bx], strict word 00000h      ; 26 c7 07 00 00              ; 0xc0a2d vgabios.c:335
    jmp short 00a57h                          ; eb 23                       ; 0xc0a32 vgabios.c:336
    mov di, strict word 00060h                ; bf 60 00                    ; 0xc0a34 vgabios.c:47
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc0a37
    mov es, dx                                ; 8e c2                       ; 0xc0a3a
    mov di, word [es:di]                      ; 26 8b 3d                    ; 0xc0a3c
    push SS                                   ; 16                          ; 0xc0a3f vgabios.c:48
    pop ES                                    ; 07                          ; 0xc0a40
    mov word [es:si], di                      ; 26 89 3c                    ; 0xc0a41
    xor ah, ah                                ; 30 e4                       ; 0xc0a44 vgabios.c:339
    mov si, ax                                ; 89 c6                       ; 0xc0a46
    sal si, 1                                 ; d1 e6                       ; 0xc0a48
    add si, strict byte 00050h                ; 83 c6 50                    ; 0xc0a4a
    mov es, dx                                ; 8e c2                       ; 0xc0a4d vgabios.c:47
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc0a4f
    push SS                                   ; 16                          ; 0xc0a52 vgabios.c:48
    pop ES                                    ; 07                          ; 0xc0a53
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc0a54
    pop bp                                    ; 5d                          ; 0xc0a57 vgabios.c:341
    pop di                                    ; 5f                          ; 0xc0a58
    pop si                                    ; 5e                          ; 0xc0a59
    retn                                      ; c3                          ; 0xc0a5a
  ; disGetNextSymbol 0xc0a5b LB 0x3839 -> off=0x0 cb=000000000000005e uValue=00000000000c0a5b 'vga_find_glyph'
vga_find_glyph:                              ; 0xc0a5b LB 0x5e
    push bp                                   ; 55                          ; 0xc0a5b vgabios.c:344
    mov bp, sp                                ; 89 e5                       ; 0xc0a5c
    push si                                   ; 56                          ; 0xc0a5e
    push di                                   ; 57                          ; 0xc0a5f
    push ax                                   ; 50                          ; 0xc0a60
    push ax                                   ; 50                          ; 0xc0a61
    push dx                                   ; 52                          ; 0xc0a62
    push bx                                   ; 53                          ; 0xc0a63
    mov bl, cl                                ; 88 cb                       ; 0xc0a64
    mov word [bp-006h], strict word 00000h    ; c7 46 fa 00 00              ; 0xc0a66 vgabios.c:346
    dec word [bp+004h]                        ; ff 4e 04                    ; 0xc0a6b vgabios.c:348
    cmp word [bp+004h], strict byte 0ffffh    ; 83 7e 04 ff                 ; 0xc0a6e
    je short 00aadh                           ; 74 39                       ; 0xc0a72
    mov cl, byte [bp+006h]                    ; 8a 4e 06                    ; 0xc0a74 vgabios.c:349
    xor ch, ch                                ; 30 ed                       ; 0xc0a77
    mov dx, ss                                ; 8c d2                       ; 0xc0a79
    mov si, word [bp-00ch]                    ; 8b 76 f4                    ; 0xc0a7b
    mov di, word [bp-008h]                    ; 8b 7e f8                    ; 0xc0a7e
    mov es, [bp-00ah]                         ; 8e 46 f6                    ; 0xc0a81
    push DS                                   ; 1e                          ; 0xc0a84
    mov ds, dx                                ; 8e da                       ; 0xc0a85
    rep cmpsb                                 ; f3 a6                       ; 0xc0a87
    pop DS                                    ; 1f                          ; 0xc0a89
    mov ax, strict word 00000h                ; b8 00 00                    ; 0xc0a8a
    je short 00a91h                           ; 74 02                       ; 0xc0a8d
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc0a8f
    test ax, ax                               ; 85 c0                       ; 0xc0a91
    jne short 00aa1h                          ; 75 0c                       ; 0xc0a93
    mov al, bl                                ; 88 d8                       ; 0xc0a95 vgabios.c:350
    xor ah, ah                                ; 30 e4                       ; 0xc0a97
    or ah, 080h                               ; 80 cc 80                    ; 0xc0a99
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc0a9c
    jmp short 00aadh                          ; eb 0c                       ; 0xc0a9f vgabios.c:351
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc0aa1 vgabios.c:353
    xor ah, ah                                ; 30 e4                       ; 0xc0aa4
    add word [bp-008h], ax                    ; 01 46 f8                    ; 0xc0aa6
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3                     ; 0xc0aa9 vgabios.c:354
    jmp short 00a6bh                          ; eb be                       ; 0xc0aab vgabios.c:355
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc0aad vgabios.c:357
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0ab0
    pop di                                    ; 5f                          ; 0xc0ab3
    pop si                                    ; 5e                          ; 0xc0ab4
    pop bp                                    ; 5d                          ; 0xc0ab5
    retn 00004h                               ; c2 04 00                    ; 0xc0ab6
  ; disGetNextSymbol 0xc0ab9 LB 0x37db -> off=0x0 cb=0000000000000046 uValue=00000000000c0ab9 'vga_read_glyph_planar'
vga_read_glyph_planar:                       ; 0xc0ab9 LB 0x46
    push bp                                   ; 55                          ; 0xc0ab9 vgabios.c:359
    mov bp, sp                                ; 89 e5                       ; 0xc0aba
    push si                                   ; 56                          ; 0xc0abc
    push di                                   ; 57                          ; 0xc0abd
    push ax                                   ; 50                          ; 0xc0abe
    push ax                                   ; 50                          ; 0xc0abf
    mov si, ax                                ; 89 c6                       ; 0xc0ac0
    mov word [bp-006h], dx                    ; 89 56 fa                    ; 0xc0ac2
    mov word [bp-008h], bx                    ; 89 5e f8                    ; 0xc0ac5
    mov bx, cx                                ; 89 cb                       ; 0xc0ac8
    mov ax, 00805h                            ; b8 05 08                    ; 0xc0aca vgabios.c:366
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc0acd
    out DX, ax                                ; ef                          ; 0xc0ad0
    dec byte [bp+004h]                        ; fe 4e 04                    ; 0xc0ad1 vgabios.c:368
    cmp byte [bp+004h], 0ffh                  ; 80 7e 04 ff                 ; 0xc0ad4
    je short 00aefh                           ; 74 15                       ; 0xc0ad8
    mov es, [bp-006h]                         ; 8e 46 fa                    ; 0xc0ada vgabios.c:369
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc0add
    not al                                    ; f6 d0                       ; 0xc0ae0
    mov di, bx                                ; 89 df                       ; 0xc0ae2
    inc bx                                    ; 43                          ; 0xc0ae4
    push SS                                   ; 16                          ; 0xc0ae5
    pop ES                                    ; 07                          ; 0xc0ae6
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc0ae7
    add si, word [bp-008h]                    ; 03 76 f8                    ; 0xc0aea vgabios.c:370
    jmp short 00ad1h                          ; eb e2                       ; 0xc0aed vgabios.c:371
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc0aef vgabios.c:374
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc0af2
    out DX, ax                                ; ef                          ; 0xc0af5
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0af6 vgabios.c:375
    pop di                                    ; 5f                          ; 0xc0af9
    pop si                                    ; 5e                          ; 0xc0afa
    pop bp                                    ; 5d                          ; 0xc0afb
    retn 00002h                               ; c2 02 00                    ; 0xc0afc
  ; disGetNextSymbol 0xc0aff LB 0x3795 -> off=0x0 cb=000000000000002f uValue=00000000000c0aff 'vga_char_ofs_planar'
vga_char_ofs_planar:                         ; 0xc0aff LB 0x2f
    push si                                   ; 56                          ; 0xc0aff vgabios.c:377
    push bp                                   ; 55                          ; 0xc0b00
    mov bp, sp                                ; 89 e5                       ; 0xc0b01
    mov ch, al                                ; 88 c5                       ; 0xc0b03
    mov al, dl                                ; 88 d0                       ; 0xc0b05
    xor ah, ah                                ; 30 e4                       ; 0xc0b07 vgabios.c:381
    mul bx                                    ; f7 e3                       ; 0xc0b09
    mov bl, byte [bp+006h]                    ; 8a 5e 06                    ; 0xc0b0b
    xor bh, bh                                ; 30 ff                       ; 0xc0b0e
    mul bx                                    ; f7 e3                       ; 0xc0b10
    mov bl, ch                                ; 88 eb                       ; 0xc0b12
    add bx, ax                                ; 01 c3                       ; 0xc0b14
    mov si, strict word 0004ch                ; be 4c 00                    ; 0xc0b16 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0b19
    mov es, ax                                ; 8e c0                       ; 0xc0b1c
    mov si, word [es:si]                      ; 26 8b 34                    ; 0xc0b1e
    mov al, cl                                ; 88 c8                       ; 0xc0b21 vgabios.c:48
    xor ah, ah                                ; 30 e4                       ; 0xc0b23
    mul si                                    ; f7 e6                       ; 0xc0b25
    add ax, bx                                ; 01 d8                       ; 0xc0b27
    pop bp                                    ; 5d                          ; 0xc0b29 vgabios.c:385
    pop si                                    ; 5e                          ; 0xc0b2a
    retn 00002h                               ; c2 02 00                    ; 0xc0b2b
  ; disGetNextSymbol 0xc0b2e LB 0x3766 -> off=0x0 cb=0000000000000045 uValue=00000000000c0b2e 'vga_read_char_planar'
vga_read_char_planar:                        ; 0xc0b2e LB 0x45
    push bp                                   ; 55                          ; 0xc0b2e vgabios.c:387
    mov bp, sp                                ; 89 e5                       ; 0xc0b2f
    push cx                                   ; 51                          ; 0xc0b31
    push si                                   ; 56                          ; 0xc0b32
    sub sp, strict byte 00012h                ; 83 ec 12                    ; 0xc0b33
    mov si, ax                                ; 89 c6                       ; 0xc0b36
    mov ax, dx                                ; 89 d0                       ; 0xc0b38
    mov byte [bp-006h], bl                    ; 88 5e fa                    ; 0xc0b3a vgabios.c:391
    mov byte [bp-005h], 000h                  ; c6 46 fb 00                 ; 0xc0b3d
    push word [bp-006h]                       ; ff 76 fa                    ; 0xc0b41
    lea cx, [bp-016h]                         ; 8d 4e ea                    ; 0xc0b44
    mov bx, si                                ; 89 f3                       ; 0xc0b47
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc0b49
    call 00ab9h                               ; e8 6a ff                    ; 0xc0b4c
    push word [bp-006h]                       ; ff 76 fa                    ; 0xc0b4f vgabios.c:394
    mov ax, 00100h                            ; b8 00 01                    ; 0xc0b52
    push ax                                   ; 50                          ; 0xc0b55
    mov bx, 0010ch                            ; bb 0c 01                    ; 0xc0b56 vgabios.c:57
    xor ax, ax                                ; 31 c0                       ; 0xc0b59
    mov es, ax                                ; 8e c0                       ; 0xc0b5b
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0b5d
    mov dx, word [es:bx+002h]                 ; 26 8b 57 02                 ; 0xc0b60
    xor cx, cx                                ; 31 c9                       ; 0xc0b64 vgabios.c:58
    lea bx, [bp-016h]                         ; 8d 5e ea                    ; 0xc0b66
    call 00a5bh                               ; e8 ef fe                    ; 0xc0b69
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0b6c vgabios.c:395
    pop si                                    ; 5e                          ; 0xc0b6f
    pop cx                                    ; 59                          ; 0xc0b70
    pop bp                                    ; 5d                          ; 0xc0b71
    retn                                      ; c3                          ; 0xc0b72
  ; disGetNextSymbol 0xc0b73 LB 0x3721 -> off=0x0 cb=0000000000000027 uValue=00000000000c0b73 'vga_char_ofs_linear'
vga_char_ofs_linear:                         ; 0xc0b73 LB 0x27
    push bp                                   ; 55                          ; 0xc0b73 vgabios.c:397
    mov bp, sp                                ; 89 e5                       ; 0xc0b74
    push ax                                   ; 50                          ; 0xc0b76
    mov byte [bp-002h], al                    ; 88 46 fe                    ; 0xc0b77
    mov al, dl                                ; 88 d0                       ; 0xc0b7a vgabios.c:401
    xor ah, ah                                ; 30 e4                       ; 0xc0b7c
    mul bx                                    ; f7 e3                       ; 0xc0b7e
    mov dl, byte [bp+004h]                    ; 8a 56 04                    ; 0xc0b80
    xor dh, dh                                ; 30 f6                       ; 0xc0b83
    mul dx                                    ; f7 e2                       ; 0xc0b85
    mov dx, ax                                ; 89 c2                       ; 0xc0b87
    mov al, byte [bp-002h]                    ; 8a 46 fe                    ; 0xc0b89
    xor ah, ah                                ; 30 e4                       ; 0xc0b8c
    add ax, dx                                ; 01 d0                       ; 0xc0b8e
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc0b90 vgabios.c:402
    sal ax, CL                                ; d3 e0                       ; 0xc0b92
    mov sp, bp                                ; 89 ec                       ; 0xc0b94 vgabios.c:404
    pop bp                                    ; 5d                          ; 0xc0b96
    retn 00002h                               ; c2 02 00                    ; 0xc0b97
  ; disGetNextSymbol 0xc0b9a LB 0x36fa -> off=0x0 cb=000000000000004e uValue=00000000000c0b9a 'vga_read_glyph_linear'
vga_read_glyph_linear:                       ; 0xc0b9a LB 0x4e
    push si                                   ; 56                          ; 0xc0b9a vgabios.c:406
    push di                                   ; 57                          ; 0xc0b9b
    push bp                                   ; 55                          ; 0xc0b9c
    mov bp, sp                                ; 89 e5                       ; 0xc0b9d
    push ax                                   ; 50                          ; 0xc0b9f
    push ax                                   ; 50                          ; 0xc0ba0
    mov si, ax                                ; 89 c6                       ; 0xc0ba1
    mov word [bp-002h], dx                    ; 89 56 fe                    ; 0xc0ba3
    mov word [bp-004h], bx                    ; 89 5e fc                    ; 0xc0ba6
    mov bx, cx                                ; 89 cb                       ; 0xc0ba9
    dec byte [bp+008h]                        ; fe 4e 08                    ; 0xc0bab vgabios.c:412
    cmp byte [bp+008h], 0ffh                  ; 80 7e 08 ff                 ; 0xc0bae
    je short 00be0h                           ; 74 2c                       ; 0xc0bb2
    xor dh, dh                                ; 30 f6                       ; 0xc0bb4 vgabios.c:413
    mov DL, strict byte 080h                  ; b2 80                       ; 0xc0bb6 vgabios.c:414
    xor ax, ax                                ; 31 c0                       ; 0xc0bb8 vgabios.c:415
    jmp short 00bc1h                          ; eb 05                       ; 0xc0bba
    cmp ax, strict word 00008h                ; 3d 08 00                    ; 0xc0bbc
    jnl short 00bd5h                          ; 7d 14                       ; 0xc0bbf
    mov es, [bp-002h]                         ; 8e 46 fe                    ; 0xc0bc1 vgabios.c:416
    mov di, si                                ; 89 f7                       ; 0xc0bc4
    add di, ax                                ; 01 c7                       ; 0xc0bc6
    cmp byte [es:di], 000h                    ; 26 80 3d 00                 ; 0xc0bc8
    je short 00bd0h                           ; 74 02                       ; 0xc0bcc
    or dh, dl                                 ; 08 d6                       ; 0xc0bce vgabios.c:417
    shr dl, 1                                 ; d0 ea                       ; 0xc0bd0 vgabios.c:418
    inc ax                                    ; 40                          ; 0xc0bd2 vgabios.c:419
    jmp short 00bbch                          ; eb e7                       ; 0xc0bd3
    mov di, bx                                ; 89 df                       ; 0xc0bd5 vgabios.c:420
    inc bx                                    ; 43                          ; 0xc0bd7
    mov byte [ss:di], dh                      ; 36 88 35                    ; 0xc0bd8
    add si, word [bp-004h]                    ; 03 76 fc                    ; 0xc0bdb vgabios.c:421
    jmp short 00babh                          ; eb cb                       ; 0xc0bde vgabios.c:422
    mov sp, bp                                ; 89 ec                       ; 0xc0be0 vgabios.c:423
    pop bp                                    ; 5d                          ; 0xc0be2
    pop di                                    ; 5f                          ; 0xc0be3
    pop si                                    ; 5e                          ; 0xc0be4
    retn 00002h                               ; c2 02 00                    ; 0xc0be5
  ; disGetNextSymbol 0xc0be8 LB 0x36ac -> off=0x0 cb=0000000000000049 uValue=00000000000c0be8 'vga_read_char_linear'
vga_read_char_linear:                        ; 0xc0be8 LB 0x49
    push bp                                   ; 55                          ; 0xc0be8 vgabios.c:425
    mov bp, sp                                ; 89 e5                       ; 0xc0be9
    push cx                                   ; 51                          ; 0xc0beb
    push si                                   ; 56                          ; 0xc0bec
    sub sp, strict byte 00012h                ; 83 ec 12                    ; 0xc0bed
    mov si, ax                                ; 89 c6                       ; 0xc0bf0
    mov ax, dx                                ; 89 d0                       ; 0xc0bf2
    mov byte [bp-006h], bl                    ; 88 5e fa                    ; 0xc0bf4 vgabios.c:429
    mov byte [bp-005h], 000h                  ; c6 46 fb 00                 ; 0xc0bf7
    push word [bp-006h]                       ; ff 76 fa                    ; 0xc0bfb
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc0bfe
    mov bx, si                                ; 89 f3                       ; 0xc0c00
    sal bx, CL                                ; d3 e3                       ; 0xc0c02
    lea cx, [bp-016h]                         ; 8d 4e ea                    ; 0xc0c04
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc0c07
    call 00b9ah                               ; e8 8d ff                    ; 0xc0c0a
    push word [bp-006h]                       ; ff 76 fa                    ; 0xc0c0d vgabios.c:432
    mov ax, 00100h                            ; b8 00 01                    ; 0xc0c10
    push ax                                   ; 50                          ; 0xc0c13
    mov bx, 0010ch                            ; bb 0c 01                    ; 0xc0c14 vgabios.c:57
    xor ax, ax                                ; 31 c0                       ; 0xc0c17
    mov es, ax                                ; 8e c0                       ; 0xc0c19
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0c1b
    mov dx, word [es:bx+002h]                 ; 26 8b 57 02                 ; 0xc0c1e
    xor cx, cx                                ; 31 c9                       ; 0xc0c22 vgabios.c:58
    lea bx, [bp-016h]                         ; 8d 5e ea                    ; 0xc0c24
    call 00a5bh                               ; e8 31 fe                    ; 0xc0c27
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0c2a vgabios.c:433
    pop si                                    ; 5e                          ; 0xc0c2d
    pop cx                                    ; 59                          ; 0xc0c2e
    pop bp                                    ; 5d                          ; 0xc0c2f
    retn                                      ; c3                          ; 0xc0c30
  ; disGetNextSymbol 0xc0c31 LB 0x3663 -> off=0x0 cb=0000000000000036 uValue=00000000000c0c31 'vga_read_2bpp_char'
vga_read_2bpp_char:                          ; 0xc0c31 LB 0x36
    push bp                                   ; 55                          ; 0xc0c31 vgabios.c:435
    mov bp, sp                                ; 89 e5                       ; 0xc0c32
    push bx                                   ; 53                          ; 0xc0c34
    push cx                                   ; 51                          ; 0xc0c35
    mov bx, ax                                ; 89 c3                       ; 0xc0c36
    mov es, dx                                ; 8e c2                       ; 0xc0c38
    mov cx, 0c000h                            ; b9 00 c0                    ; 0xc0c3a vgabios.c:441
    mov DH, strict byte 080h                  ; b6 80                       ; 0xc0c3d vgabios.c:442
    xor dl, dl                                ; 30 d2                       ; 0xc0c3f vgabios.c:443
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0c41 vgabios.c:444
    xchg ah, al                               ; 86 c4                       ; 0xc0c44
    xor bx, bx                                ; 31 db                       ; 0xc0c46 vgabios.c:446
    jmp short 00c4fh                          ; eb 05                       ; 0xc0c48
    cmp bx, strict byte 00008h                ; 83 fb 08                    ; 0xc0c4a
    jnl short 00c5eh                          ; 7d 0f                       ; 0xc0c4d
    test ax, cx                               ; 85 c8                       ; 0xc0c4f vgabios.c:447
    je short 00c55h                           ; 74 02                       ; 0xc0c51
    or dl, dh                                 ; 08 f2                       ; 0xc0c53 vgabios.c:448
    shr dh, 1                                 ; d0 ee                       ; 0xc0c55 vgabios.c:449
    shr cx, 1                                 ; d1 e9                       ; 0xc0c57 vgabios.c:450
    shr cx, 1                                 ; d1 e9                       ; 0xc0c59
    inc bx                                    ; 43                          ; 0xc0c5b vgabios.c:451
    jmp short 00c4ah                          ; eb ec                       ; 0xc0c5c
    mov al, dl                                ; 88 d0                       ; 0xc0c5e vgabios.c:453
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0c60
    pop cx                                    ; 59                          ; 0xc0c63
    pop bx                                    ; 5b                          ; 0xc0c64
    pop bp                                    ; 5d                          ; 0xc0c65
    retn                                      ; c3                          ; 0xc0c66
  ; disGetNextSymbol 0xc0c67 LB 0x362d -> off=0x0 cb=0000000000000084 uValue=00000000000c0c67 'vga_read_glyph_cga'
vga_read_glyph_cga:                          ; 0xc0c67 LB 0x84
    push bp                                   ; 55                          ; 0xc0c67 vgabios.c:455
    mov bp, sp                                ; 89 e5                       ; 0xc0c68
    push cx                                   ; 51                          ; 0xc0c6a
    push si                                   ; 56                          ; 0xc0c6b
    push di                                   ; 57                          ; 0xc0c6c
    push ax                                   ; 50                          ; 0xc0c6d
    mov si, dx                                ; 89 d6                       ; 0xc0c6e
    cmp bl, 006h                              ; 80 fb 06                    ; 0xc0c70 vgabios.c:463
    je short 00cafh                           ; 74 3a                       ; 0xc0c73
    mov bx, ax                                ; 89 c3                       ; 0xc0c75 vgabios.c:465
    sal bx, 1                                 ; d1 e3                       ; 0xc0c77
    mov word [bp-008h], 0b800h                ; c7 46 f8 00 b8              ; 0xc0c79
    xor cx, cx                                ; 31 c9                       ; 0xc0c7e vgabios.c:467
    jmp short 00c87h                          ; eb 05                       ; 0xc0c80
    cmp cx, strict byte 00004h                ; 83 f9 04                    ; 0xc0c82
    jnl short 00ce3h                          ; 7d 5c                       ; 0xc0c85
    mov ax, bx                                ; 89 d8                       ; 0xc0c87 vgabios.c:468
    mov dx, word [bp-008h]                    ; 8b 56 f8                    ; 0xc0c89
    call 00c31h                               ; e8 a2 ff                    ; 0xc0c8c
    mov di, si                                ; 89 f7                       ; 0xc0c8f
    inc si                                    ; 46                          ; 0xc0c91
    push SS                                   ; 16                          ; 0xc0c92
    pop ES                                    ; 07                          ; 0xc0c93
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc0c94
    lea ax, [bx+02000h]                       ; 8d 87 00 20                 ; 0xc0c97 vgabios.c:469
    mov dx, word [bp-008h]                    ; 8b 56 f8                    ; 0xc0c9b
    call 00c31h                               ; e8 90 ff                    ; 0xc0c9e
    mov di, si                                ; 89 f7                       ; 0xc0ca1
    inc si                                    ; 46                          ; 0xc0ca3
    push SS                                   ; 16                          ; 0xc0ca4
    pop ES                                    ; 07                          ; 0xc0ca5
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc0ca6
    add bx, strict byte 00050h                ; 83 c3 50                    ; 0xc0ca9 vgabios.c:470
    inc cx                                    ; 41                          ; 0xc0cac vgabios.c:471
    jmp short 00c82h                          ; eb d3                       ; 0xc0cad
    mov bx, ax                                ; 89 c3                       ; 0xc0caf vgabios.c:473
    mov word [bp-008h], 0b800h                ; c7 46 f8 00 b8              ; 0xc0cb1
    xor cx, cx                                ; 31 c9                       ; 0xc0cb6 vgabios.c:474
    jmp short 00cbfh                          ; eb 05                       ; 0xc0cb8
    cmp cx, strict byte 00004h                ; 83 f9 04                    ; 0xc0cba
    jnl short 00ce3h                          ; 7d 24                       ; 0xc0cbd
    mov di, si                                ; 89 f7                       ; 0xc0cbf vgabios.c:475
    inc si                                    ; 46                          ; 0xc0cc1
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc0cc2
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0cc5
    push SS                                   ; 16                          ; 0xc0cc8
    pop ES                                    ; 07                          ; 0xc0cc9
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc0cca
    mov di, si                                ; 89 f7                       ; 0xc0ccd vgabios.c:476
    inc si                                    ; 46                          ; 0xc0ccf
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc0cd0
    mov al, byte [es:bx+02000h]               ; 26 8a 87 00 20              ; 0xc0cd3
    push SS                                   ; 16                          ; 0xc0cd8
    pop ES                                    ; 07                          ; 0xc0cd9
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc0cda
    add bx, strict byte 00050h                ; 83 c3 50                    ; 0xc0cdd vgabios.c:477
    inc cx                                    ; 41                          ; 0xc0ce0 vgabios.c:478
    jmp short 00cbah                          ; eb d7                       ; 0xc0ce1
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc0ce3 vgabios.c:480
    pop di                                    ; 5f                          ; 0xc0ce6
    pop si                                    ; 5e                          ; 0xc0ce7
    pop cx                                    ; 59                          ; 0xc0ce8
    pop bp                                    ; 5d                          ; 0xc0ce9
    retn                                      ; c3                          ; 0xc0cea
  ; disGetNextSymbol 0xc0ceb LB 0x35a9 -> off=0x0 cb=000000000000001b uValue=00000000000c0ceb 'vga_char_ofs_cga'
vga_char_ofs_cga:                            ; 0xc0ceb LB 0x1b
    push cx                                   ; 51                          ; 0xc0ceb vgabios.c:482
    push bp                                   ; 55                          ; 0xc0cec
    mov bp, sp                                ; 89 e5                       ; 0xc0ced
    mov cl, al                                ; 88 c1                       ; 0xc0cef
    mov al, dl                                ; 88 d0                       ; 0xc0cf1
    xor ah, ah                                ; 30 e4                       ; 0xc0cf3 vgabios.c:487
    mul bx                                    ; f7 e3                       ; 0xc0cf5
    mov bx, ax                                ; 89 c3                       ; 0xc0cf7
    sal bx, 1                                 ; d1 e3                       ; 0xc0cf9
    sal bx, 1                                 ; d1 e3                       ; 0xc0cfb
    mov al, cl                                ; 88 c8                       ; 0xc0cfd
    xor ah, ah                                ; 30 e4                       ; 0xc0cff
    add ax, bx                                ; 01 d8                       ; 0xc0d01
    pop bp                                    ; 5d                          ; 0xc0d03 vgabios.c:488
    pop cx                                    ; 59                          ; 0xc0d04
    retn                                      ; c3                          ; 0xc0d05
  ; disGetNextSymbol 0xc0d06 LB 0x358e -> off=0x0 cb=000000000000006b uValue=00000000000c0d06 'vga_read_char_cga'
vga_read_char_cga:                           ; 0xc0d06 LB 0x6b
    push bp                                   ; 55                          ; 0xc0d06 vgabios.c:490
    mov bp, sp                                ; 89 e5                       ; 0xc0d07
    push bx                                   ; 53                          ; 0xc0d09
    push cx                                   ; 51                          ; 0xc0d0a
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc0d0b
    mov bl, dl                                ; 88 d3                       ; 0xc0d0e vgabios.c:496
    xor bh, bh                                ; 30 ff                       ; 0xc0d10
    lea dx, [bp-00eh]                         ; 8d 56 f2                    ; 0xc0d12
    call 00c67h                               ; e8 4f ff                    ; 0xc0d15
    mov ax, strict word 00008h                ; b8 08 00                    ; 0xc0d18 vgabios.c:499
    push ax                                   ; 50                          ; 0xc0d1b
    mov ax, 00080h                            ; b8 80 00                    ; 0xc0d1c
    push ax                                   ; 50                          ; 0xc0d1f
    mov bx, 0010ch                            ; bb 0c 01                    ; 0xc0d20 vgabios.c:57
    xor ax, ax                                ; 31 c0                       ; 0xc0d23
    mov es, ax                                ; 8e c0                       ; 0xc0d25
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0d27
    mov dx, word [es:bx+002h]                 ; 26 8b 57 02                 ; 0xc0d2a
    xor cx, cx                                ; 31 c9                       ; 0xc0d2e vgabios.c:58
    lea bx, [bp-00eh]                         ; 8d 5e f2                    ; 0xc0d30
    call 00a5bh                               ; e8 25 fd                    ; 0xc0d33
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc0d36
    test ah, 080h                             ; f6 c4 80                    ; 0xc0d39 vgabios.c:501
    jne short 00d67h                          ; 75 29                       ; 0xc0d3c
    mov bx, strict word 0007ch                ; bb 7c 00                    ; 0xc0d3e vgabios.c:57
    xor ax, ax                                ; 31 c0                       ; 0xc0d41
    mov es, ax                                ; 8e c0                       ; 0xc0d43
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0d45
    mov dx, word [es:bx+002h]                 ; 26 8b 57 02                 ; 0xc0d48
    test dx, dx                               ; 85 d2                       ; 0xc0d4c vgabios.c:505
    jne short 00d54h                          ; 75 04                       ; 0xc0d4e
    test ax, ax                               ; 85 c0                       ; 0xc0d50
    je short 00d67h                           ; 74 13                       ; 0xc0d52
    mov bx, strict word 00008h                ; bb 08 00                    ; 0xc0d54 vgabios.c:506
    push bx                                   ; 53                          ; 0xc0d57
    mov bx, 00080h                            ; bb 80 00                    ; 0xc0d58
    push bx                                   ; 53                          ; 0xc0d5b
    mov cx, bx                                ; 89 d9                       ; 0xc0d5c
    lea bx, [bp-00eh]                         ; 8d 5e f2                    ; 0xc0d5e
    call 00a5bh                               ; e8 f7 fc                    ; 0xc0d61
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc0d64
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc0d67 vgabios.c:509
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0d6a
    pop cx                                    ; 59                          ; 0xc0d6d
    pop bx                                    ; 5b                          ; 0xc0d6e
    pop bp                                    ; 5d                          ; 0xc0d6f
    retn                                      ; c3                          ; 0xc0d70
  ; disGetNextSymbol 0xc0d71 LB 0x3523 -> off=0x0 cb=0000000000000147 uValue=00000000000c0d71 'vga_read_char_attr'
vga_read_char_attr:                          ; 0xc0d71 LB 0x147
    push bp                                   ; 55                          ; 0xc0d71 vgabios.c:511
    mov bp, sp                                ; 89 e5                       ; 0xc0d72
    push bx                                   ; 53                          ; 0xc0d74
    push cx                                   ; 51                          ; 0xc0d75
    push si                                   ; 56                          ; 0xc0d76
    push di                                   ; 57                          ; 0xc0d77
    sub sp, strict byte 00012h                ; 83 ec 12                    ; 0xc0d78
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc0d7b
    mov si, dx                                ; 89 d6                       ; 0xc0d7e
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc0d80 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0d83
    mov es, ax                                ; 8e c0                       ; 0xc0d86
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0d88
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc0d8b vgabios.c:38
    xor ah, ah                                ; 30 e4                       ; 0xc0d8e vgabios.c:519
    call 03630h                               ; e8 9d 28                    ; 0xc0d90
    mov cl, al                                ; 88 c1                       ; 0xc0d93
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc0d95 vgabios.c:520
    jne short 00d9ch                          ; 75 03                       ; 0xc0d97
    jmp near 00eafh                           ; e9 13 01                    ; 0xc0d99
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc0d9c vgabios.c:524
    mov byte [bp-014h], al                    ; 88 46 ec                    ; 0xc0d9f
    mov byte [bp-013h], 000h                  ; c6 46 ed 00                 ; 0xc0da2
    lea bx, [bp-01ah]                         ; 8d 5e e6                    ; 0xc0da6
    lea dx, [bp-018h]                         ; 8d 56 e8                    ; 0xc0da9
    mov ax, word [bp-014h]                    ; 8b 46 ec                    ; 0xc0dac
    call 00a1bh                               ; e8 69 fc                    ; 0xc0daf
    mov ch, byte [bp-01ah]                    ; 8a 6e e6                    ; 0xc0db2 vgabios.c:525
    mov ax, word [bp-01ah]                    ; 8b 46 e6                    ; 0xc0db5 vgabios.c:526
    mov al, ah                                ; 88 e0                       ; 0xc0db8
    xor ah, ah                                ; 30 e4                       ; 0xc0dba
    mov word [bp-012h], ax                    ; 89 46 ee                    ; 0xc0dbc
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc0dbf
    mov byte [bp-00eh], al                    ; 88 46 f2                    ; 0xc0dc2
    mov bx, 00084h                            ; bb 84 00                    ; 0xc0dc5 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0dc8
    mov es, ax                                ; 8e c0                       ; 0xc0dcb
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0dcd
    xor ah, ah                                ; 30 e4                       ; 0xc0dd0 vgabios.c:38
    mov dx, ax                                ; 89 c2                       ; 0xc0dd2
    inc dx                                    ; 42                          ; 0xc0dd4
    mov di, strict word 0004ah                ; bf 4a 00                    ; 0xc0dd5 vgabios.c:47
    mov di, word [es:di]                      ; 26 8b 3d                    ; 0xc0dd8
    mov word [bp-016h], di                    ; 89 7e ea                    ; 0xc0ddb vgabios.c:48
    mov bl, cl                                ; 88 cb                       ; 0xc0dde vgabios.c:532
    xor bh, bh                                ; 30 ff                       ; 0xc0de0
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc0de2
    sal bx, CL                                ; d3 e3                       ; 0xc0de4
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc0de6
    jne short 00e23h                          ; 75 36                       ; 0xc0deb
    mov ax, di                                ; 89 f8                       ; 0xc0ded vgabios.c:534
    mul dx                                    ; f7 e2                       ; 0xc0def
    sal ax, 1                                 ; d1 e0                       ; 0xc0df1
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc0df3
    mov dl, byte [bp-00ch]                    ; 8a 56 f4                    ; 0xc0df5
    xor dh, dh                                ; 30 f6                       ; 0xc0df8
    inc ax                                    ; 40                          ; 0xc0dfa
    mul dx                                    ; f7 e2                       ; 0xc0dfb
    mov word [bp-010h], ax                    ; 89 46 f0                    ; 0xc0dfd
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc0e00
    xor ah, ah                                ; 30 e4                       ; 0xc0e03
    mul di                                    ; f7 e7                       ; 0xc0e05
    mov dl, ch                                ; 88 ea                       ; 0xc0e07
    xor dh, dh                                ; 30 f6                       ; 0xc0e09
    add ax, dx                                ; 01 d0                       ; 0xc0e0b
    sal ax, 1                                 ; d1 e0                       ; 0xc0e0d
    mov di, word [bp-010h]                    ; 8b 7e f0                    ; 0xc0e0f
    add di, ax                                ; 01 c7                       ; 0xc0e12
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc0e14 vgabios.c:45
    mov ax, word [es:di]                      ; 26 8b 05                    ; 0xc0e18
    push SS                                   ; 16                          ; 0xc0e1b vgabios.c:48
    pop ES                                    ; 07                          ; 0xc0e1c
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc0e1d
    jmp near 00eafh                           ; e9 8c 00                    ; 0xc0e20 vgabios.c:536
    mov bl, byte [bx+047b0h]                  ; 8a 9f b0 47                 ; 0xc0e23 vgabios.c:537
    cmp bl, 005h                              ; 80 fb 05                    ; 0xc0e27
    je short 00e82h                           ; 74 56                       ; 0xc0e2a
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc0e2c
    jc short 00e38h                           ; 72 07                       ; 0xc0e2f
    jbe short 00e3ah                          ; 76 07                       ; 0xc0e31
    cmp bl, 004h                              ; 80 fb 04                    ; 0xc0e33
    jbe short 00e55h                          ; 76 1d                       ; 0xc0e36
    jmp short 00eafh                          ; eb 75                       ; 0xc0e38
    mov dl, byte [bp-00eh]                    ; 8a 56 f2                    ; 0xc0e3a vgabios.c:540
    xor dh, dh                                ; 30 f6                       ; 0xc0e3d
    mov al, ch                                ; 88 e8                       ; 0xc0e3f
    xor ah, ah                                ; 30 e4                       ; 0xc0e41
    mov bx, word [bp-016h]                    ; 8b 5e ea                    ; 0xc0e43
    call 00cebh                               ; e8 a2 fe                    ; 0xc0e46
    mov dl, byte [bp-00ah]                    ; 8a 56 f6                    ; 0xc0e49 vgabios.c:541
    xor dh, dh                                ; 30 f6                       ; 0xc0e4c
    call 00d06h                               ; e8 b5 fe                    ; 0xc0e4e
    xor ah, ah                                ; 30 e4                       ; 0xc0e51
    jmp short 00e1bh                          ; eb c6                       ; 0xc0e53
    mov bx, 00085h                            ; bb 85 00                    ; 0xc0e55 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0e58
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc0e5b vgabios.c:546
    mov byte [bp-00fh], 000h                  ; c6 46 f1 00                 ; 0xc0e5e
    push word [bp-010h]                       ; ff 76 f0                    ; 0xc0e62
    mov dl, byte [bp-012h]                    ; 8a 56 ee                    ; 0xc0e65
    xor dh, dh                                ; 30 f6                       ; 0xc0e68
    mov al, ch                                ; 88 e8                       ; 0xc0e6a
    xor ah, ah                                ; 30 e4                       ; 0xc0e6c
    mov cx, word [bp-014h]                    ; 8b 4e ec                    ; 0xc0e6e
    mov bx, di                                ; 89 fb                       ; 0xc0e71
    call 00affh                               ; e8 89 fc                    ; 0xc0e73
    mov bx, word [bp-010h]                    ; 8b 5e f0                    ; 0xc0e76 vgabios.c:547
    mov dx, ax                                ; 89 c2                       ; 0xc0e79
    mov ax, di                                ; 89 f8                       ; 0xc0e7b
    call 00b2eh                               ; e8 ae fc                    ; 0xc0e7d
    jmp short 00e51h                          ; eb cf                       ; 0xc0e80
    mov bx, 00085h                            ; bb 85 00                    ; 0xc0e82 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0e85
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc0e88 vgabios.c:551
    mov byte [bp-00fh], 000h                  ; c6 46 f1 00                 ; 0xc0e8b
    push word [bp-010h]                       ; ff 76 f0                    ; 0xc0e8f
    mov dl, byte [bp-012h]                    ; 8a 56 ee                    ; 0xc0e92
    xor dh, dh                                ; 30 f6                       ; 0xc0e95
    mov al, ch                                ; 88 e8                       ; 0xc0e97
    xor ah, ah                                ; 30 e4                       ; 0xc0e99
    mov cx, word [bp-014h]                    ; 8b 4e ec                    ; 0xc0e9b
    mov bx, di                                ; 89 fb                       ; 0xc0e9e
    call 00b73h                               ; e8 d0 fc                    ; 0xc0ea0
    mov bx, word [bp-010h]                    ; 8b 5e f0                    ; 0xc0ea3 vgabios.c:552
    mov dx, ax                                ; 89 c2                       ; 0xc0ea6
    mov ax, di                                ; 89 f8                       ; 0xc0ea8
    call 00be8h                               ; e8 3b fd                    ; 0xc0eaa
    jmp short 00e51h                          ; eb a2                       ; 0xc0ead
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc0eaf vgabios.c:561
    pop di                                    ; 5f                          ; 0xc0eb2
    pop si                                    ; 5e                          ; 0xc0eb3
    pop cx                                    ; 59                          ; 0xc0eb4
    pop bx                                    ; 5b                          ; 0xc0eb5
    pop bp                                    ; 5d                          ; 0xc0eb6
    retn                                      ; c3                          ; 0xc0eb7
  ; disGetNextSymbol 0xc0eb8 LB 0x33dc -> off=0x10 cb=0000000000000083 uValue=00000000000c0ec8 'vga_get_font_info'
    db  0dfh, 00eh, 024h, 00fh, 029h, 00fh, 030h, 00fh, 035h, 00fh, 03ah, 00fh, 03fh, 00fh, 044h, 00fh
vga_get_font_info:                           ; 0xc0ec8 LB 0x83
    push si                                   ; 56                          ; 0xc0ec8 vgabios.c:563
    push di                                   ; 57                          ; 0xc0ec9
    push bp                                   ; 55                          ; 0xc0eca
    mov bp, sp                                ; 89 e5                       ; 0xc0ecb
    mov si, dx                                ; 89 d6                       ; 0xc0ecd
    mov di, bx                                ; 89 df                       ; 0xc0ecf
    cmp ax, strict word 00007h                ; 3d 07 00                    ; 0xc0ed1 vgabios.c:568
    jnbe short 00f1eh                         ; 77 48                       ; 0xc0ed4
    mov bx, ax                                ; 89 c3                       ; 0xc0ed6
    sal bx, 1                                 ; d1 e3                       ; 0xc0ed8
    jmp word [cs:bx+00eb8h]                   ; 2e ff a7 b8 0e              ; 0xc0eda
    mov bx, strict word 0007ch                ; bb 7c 00                    ; 0xc0edf vgabios.c:57
    xor ax, ax                                ; 31 c0                       ; 0xc0ee2
    mov es, ax                                ; 8e c0                       ; 0xc0ee4
    mov dx, word [es:bx]                      ; 26 8b 17                    ; 0xc0ee6
    mov ax, word [es:bx+002h]                 ; 26 8b 47 02                 ; 0xc0ee9
    push SS                                   ; 16                          ; 0xc0eed vgabios.c:571
    pop ES                                    ; 07                          ; 0xc0eee
    mov word [es:di], dx                      ; 26 89 15                    ; 0xc0eef
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc0ef2
    mov bx, 00085h                            ; bb 85 00                    ; 0xc0ef5
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0ef8
    mov es, ax                                ; 8e c0                       ; 0xc0efb
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0efd
    xor ah, ah                                ; 30 e4                       ; 0xc0f00
    push SS                                   ; 16                          ; 0xc0f02
    pop ES                                    ; 07                          ; 0xc0f03
    mov bx, cx                                ; 89 cb                       ; 0xc0f04
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc0f06
    mov bx, 00084h                            ; bb 84 00                    ; 0xc0f09
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0f0c
    mov es, ax                                ; 8e c0                       ; 0xc0f0f
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0f11
    xor ah, ah                                ; 30 e4                       ; 0xc0f14
    push SS                                   ; 16                          ; 0xc0f16
    pop ES                                    ; 07                          ; 0xc0f17
    mov bx, word [bp+008h]                    ; 8b 5e 08                    ; 0xc0f18
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc0f1b
    pop bp                                    ; 5d                          ; 0xc0f1e
    pop di                                    ; 5f                          ; 0xc0f1f
    pop si                                    ; 5e                          ; 0xc0f20
    retn 00002h                               ; c2 02 00                    ; 0xc0f21
    mov bx, 0010ch                            ; bb 0c 01                    ; 0xc0f24 vgabios.c:57
    jmp short 00ee2h                          ; eb b9                       ; 0xc0f27
    mov dx, 05d6ch                            ; ba 6c 5d                    ; 0xc0f29 vgabios.c:576
    mov ax, ds                                ; 8c d8                       ; 0xc0f2c
    jmp short 00eedh                          ; eb bd                       ; 0xc0f2e vgabios.c:577
    mov dx, 0556ch                            ; ba 6c 55                    ; 0xc0f30 vgabios.c:579
    jmp short 00f2ch                          ; eb f7                       ; 0xc0f33
    mov dx, 0596ch                            ; ba 6c 59                    ; 0xc0f35 vgabios.c:582
    jmp short 00f2ch                          ; eb f2                       ; 0xc0f38
    mov dx, 07b6ch                            ; ba 6c 7b                    ; 0xc0f3a vgabios.c:585
    jmp short 00f2ch                          ; eb ed                       ; 0xc0f3d
    mov dx, 06b6ch                            ; ba 6c 6b                    ; 0xc0f3f vgabios.c:588
    jmp short 00f2ch                          ; eb e8                       ; 0xc0f42
    mov dx, 07c99h                            ; ba 99 7c                    ; 0xc0f44 vgabios.c:591
    jmp short 00f2ch                          ; eb e3                       ; 0xc0f47
    jmp short 00f1eh                          ; eb d3                       ; 0xc0f49 vgabios.c:597
  ; disGetNextSymbol 0xc0f4b LB 0x3349 -> off=0x0 cb=000000000000016d uValue=00000000000c0f4b 'vga_read_pixel'
vga_read_pixel:                              ; 0xc0f4b LB 0x16d
    push bp                                   ; 55                          ; 0xc0f4b vgabios.c:610
    mov bp, sp                                ; 89 e5                       ; 0xc0f4c
    push si                                   ; 56                          ; 0xc0f4e
    push di                                   ; 57                          ; 0xc0f4f
    sub sp, strict byte 0000ch                ; 83 ec 0c                    ; 0xc0f50
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc0f53
    mov si, dx                                ; 89 d6                       ; 0xc0f56
    mov word [bp-010h], bx                    ; 89 5e f0                    ; 0xc0f58
    mov word [bp-00eh], cx                    ; 89 4e f2                    ; 0xc0f5b
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc0f5e vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0f61
    mov es, ax                                ; 8e c0                       ; 0xc0f64
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0f66
    xor ah, ah                                ; 30 e4                       ; 0xc0f69 vgabios.c:617
    call 03630h                               ; e8 c2 26                    ; 0xc0f6b
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc0f6e
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc0f71 vgabios.c:618
    je short 00f84h                           ; 74 0f                       ; 0xc0f73
    mov bl, al                                ; 88 c3                       ; 0xc0f75 vgabios.c:620
    xor bh, bh                                ; 30 ff                       ; 0xc0f77
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc0f79
    sal bx, CL                                ; d3 e3                       ; 0xc0f7b
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc0f7d
    jne short 00f87h                          ; 75 03                       ; 0xc0f82
    jmp near 010b1h                           ; e9 2a 01                    ; 0xc0f84 vgabios.c:621
    mov ch, byte [bx+047b0h]                  ; 8a af b0 47                 ; 0xc0f87 vgabios.c:624
    cmp ch, cl                                ; 38 cd                       ; 0xc0f8b
    jc short 00f9eh                           ; 72 0f                       ; 0xc0f8d
    jbe short 00fa6h                          ; 76 15                       ; 0xc0f8f
    cmp ch, 005h                              ; 80 fd 05                    ; 0xc0f91
    je short 00fdfh                           ; 74 49                       ; 0xc0f94
    cmp ch, 004h                              ; 80 fd 04                    ; 0xc0f96
    je short 00fa6h                           ; 74 0b                       ; 0xc0f99
    jmp near 010a7h                           ; e9 09 01                    ; 0xc0f9b
    cmp ch, 002h                              ; 80 fd 02                    ; 0xc0f9e
    je short 01013h                           ; 74 70                       ; 0xc0fa1
    jmp near 010a7h                           ; e9 01 01                    ; 0xc0fa3
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc0fa6 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0fa9
    mov es, ax                                ; 8e c0                       ; 0xc0fac
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc0fae
    mov ax, word [bp-010h]                    ; 8b 46 f0                    ; 0xc0fb1 vgabios.c:48
    mul bx                                    ; f7 e3                       ; 0xc0fb4
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc0fb6
    mov bx, si                                ; 89 f3                       ; 0xc0fb8
    shr bx, CL                                ; d3 eb                       ; 0xc0fba
    add bx, ax                                ; 01 c3                       ; 0xc0fbc
    mov di, strict word 0004ch                ; bf 4c 00                    ; 0xc0fbe vgabios.c:47
    mov ax, word [es:di]                      ; 26 8b 05                    ; 0xc0fc1
    mov cl, byte [bp-00ch]                    ; 8a 4e f4                    ; 0xc0fc4 vgabios.c:48
    xor ch, ch                                ; 30 ed                       ; 0xc0fc7
    mul cx                                    ; f7 e1                       ; 0xc0fc9
    add bx, ax                                ; 01 c3                       ; 0xc0fcb
    mov cx, si                                ; 89 f1                       ; 0xc0fcd vgabios.c:629
    and cx, strict byte 00007h                ; 83 e1 07                    ; 0xc0fcf
    mov ax, 00080h                            ; b8 80 00                    ; 0xc0fd2
    sar ax, CL                                ; d3 f8                       ; 0xc0fd5
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc0fd7
    mov byte [bp-008h], ch                    ; 88 6e f8                    ; 0xc0fda vgabios.c:631
    jmp short 00fe8h                          ; eb 09                       ; 0xc0fdd
    jmp near 01087h                           ; e9 a5 00                    ; 0xc0fdf
    cmp byte [bp-008h], 004h                  ; 80 7e f8 04                 ; 0xc0fe2
    jnc short 01010h                          ; 73 28                       ; 0xc0fe6
    mov ah, byte [bp-008h]                    ; 8a 66 f8                    ; 0xc0fe8 vgabios.c:632
    xor al, al                                ; 30 c0                       ; 0xc0feb
    or AL, strict byte 004h                   ; 0c 04                       ; 0xc0fed
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc0fef
    out DX, ax                                ; ef                          ; 0xc0ff2
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc0ff3 vgabios.c:37
    mov es, ax                                ; 8e c0                       ; 0xc0ff6
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0ff8
    and al, byte [bp-00ah]                    ; 22 46 f6                    ; 0xc0ffb vgabios.c:38
    test al, al                               ; 84 c0                       ; 0xc0ffe vgabios.c:634
    jbe short 0100bh                          ; 76 09                       ; 0xc1000
    mov cl, byte [bp-008h]                    ; 8a 4e f8                    ; 0xc1002 vgabios.c:635
    mov AL, strict byte 001h                  ; b0 01                       ; 0xc1005
    sal al, CL                                ; d2 e0                       ; 0xc1007
    or ch, al                                 ; 08 c5                       ; 0xc1009
    inc byte [bp-008h]                        ; fe 46 f8                    ; 0xc100b vgabios.c:636
    jmp short 00fe2h                          ; eb d2                       ; 0xc100e
    jmp near 010a9h                           ; e9 96 00                    ; 0xc1010
    mov al, byte [bx+047b1h]                  ; 8a 87 b1 47                 ; 0xc1013 vgabios.c:639
    xor ah, ah                                ; 30 e4                       ; 0xc1017
    mov cx, strict word 00004h                ; b9 04 00                    ; 0xc1019
    sub cx, ax                                ; 29 c1                       ; 0xc101c
    mov ax, dx                                ; 89 d0                       ; 0xc101e
    shr ax, CL                                ; d3 e8                       ; 0xc1020
    mov cx, ax                                ; 89 c1                       ; 0xc1022
    mov ax, word [bp-010h]                    ; 8b 46 f0                    ; 0xc1024
    shr ax, 1                                 ; d1 e8                       ; 0xc1027
    mov bx, strict word 00050h                ; bb 50 00                    ; 0xc1029
    mul bx                                    ; f7 e3                       ; 0xc102c
    mov bx, cx                                ; 89 cb                       ; 0xc102e
    add bx, ax                                ; 01 c3                       ; 0xc1030
    test byte [bp-010h], 001h                 ; f6 46 f0 01                 ; 0xc1032 vgabios.c:640
    je short 0103bh                           ; 74 03                       ; 0xc1036
    add bh, 020h                              ; 80 c7 20                    ; 0xc1038 vgabios.c:641
    mov ax, 0b800h                            ; b8 00 b8                    ; 0xc103b vgabios.c:37
    mov es, ax                                ; 8e c0                       ; 0xc103e
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc1040
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc1043 vgabios.c:643
    xor bh, bh                                ; 30 ff                       ; 0xc1046
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc1048
    sal bx, CL                                ; d3 e3                       ; 0xc104a
    cmp byte [bx+047b1h], 002h                ; 80 bf b1 47 02              ; 0xc104c
    jne short 0106eh                          ; 75 1b                       ; 0xc1051
    mov cx, si                                ; 89 f1                       ; 0xc1053 vgabios.c:644
    xor ch, ch                                ; 30 ed                       ; 0xc1055
    and cl, 003h                              ; 80 e1 03                    ; 0xc1057
    mov bx, strict word 00003h                ; bb 03 00                    ; 0xc105a
    sub bx, cx                                ; 29 cb                       ; 0xc105d
    mov cx, bx                                ; 89 d9                       ; 0xc105f
    sal cx, 1                                 ; d1 e1                       ; 0xc1061
    xor ah, ah                                ; 30 e4                       ; 0xc1063
    sar ax, CL                                ; d3 f8                       ; 0xc1065
    mov ch, al                                ; 88 c5                       ; 0xc1067
    and ch, 003h                              ; 80 e5 03                    ; 0xc1069
    jmp short 010a9h                          ; eb 3b                       ; 0xc106c vgabios.c:645
    mov cx, si                                ; 89 f1                       ; 0xc106e vgabios.c:646
    xor ch, ch                                ; 30 ed                       ; 0xc1070
    and cl, 007h                              ; 80 e1 07                    ; 0xc1072
    mov bx, strict word 00007h                ; bb 07 00                    ; 0xc1075
    sub bx, cx                                ; 29 cb                       ; 0xc1078
    mov cx, bx                                ; 89 d9                       ; 0xc107a
    xor ah, ah                                ; 30 e4                       ; 0xc107c
    sar ax, CL                                ; d3 f8                       ; 0xc107e
    mov ch, al                                ; 88 c5                       ; 0xc1080
    and ch, 001h                              ; 80 e5 01                    ; 0xc1082
    jmp short 010a9h                          ; eb 22                       ; 0xc1085 vgabios.c:647
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc1087 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc108a
    mov es, ax                                ; 8e c0                       ; 0xc108d
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc108f
    sal bx, CL                                ; d3 e3                       ; 0xc1092 vgabios.c:48
    mov ax, word [bp-010h]                    ; 8b 46 f0                    ; 0xc1094
    mul bx                                    ; f7 e3                       ; 0xc1097
    mov bx, si                                ; 89 f3                       ; 0xc1099
    add bx, ax                                ; 01 c3                       ; 0xc109b
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc109d vgabios.c:37
    mov es, ax                                ; 8e c0                       ; 0xc10a0
    mov ch, byte [es:bx]                      ; 26 8a 2f                    ; 0xc10a2
    jmp short 010a9h                          ; eb 02                       ; 0xc10a5 vgabios.c:651
    xor ch, ch                                ; 30 ed                       ; 0xc10a7 vgabios.c:656
    push SS                                   ; 16                          ; 0xc10a9 vgabios.c:658
    pop ES                                    ; 07                          ; 0xc10aa
    mov bx, word [bp-00eh]                    ; 8b 5e f2                    ; 0xc10ab
    mov byte [es:bx], ch                      ; 26 88 2f                    ; 0xc10ae
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc10b1 vgabios.c:659
    pop di                                    ; 5f                          ; 0xc10b4
    pop si                                    ; 5e                          ; 0xc10b5
    pop bp                                    ; 5d                          ; 0xc10b6
    retn                                      ; c3                          ; 0xc10b7
  ; disGetNextSymbol 0xc10b8 LB 0x31dc -> off=0x0 cb=000000000000009f uValue=00000000000c10b8 'biosfn_perform_gray_scale_summing'
biosfn_perform_gray_scale_summing:           ; 0xc10b8 LB 0x9f
    push bp                                   ; 55                          ; 0xc10b8 vgabios.c:664
    mov bp, sp                                ; 89 e5                       ; 0xc10b9
    push bx                                   ; 53                          ; 0xc10bb
    push cx                                   ; 51                          ; 0xc10bc
    push si                                   ; 56                          ; 0xc10bd
    push di                                   ; 57                          ; 0xc10be
    push ax                                   ; 50                          ; 0xc10bf
    push ax                                   ; 50                          ; 0xc10c0
    mov bx, ax                                ; 89 c3                       ; 0xc10c1
    mov di, dx                                ; 89 d7                       ; 0xc10c3
    mov dx, 003dah                            ; ba da 03                    ; 0xc10c5 vgabios.c:669
    in AL, DX                                 ; ec                          ; 0xc10c8
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc10c9
    xor al, al                                ; 30 c0                       ; 0xc10cb vgabios.c:670
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc10cd
    out DX, AL                                ; ee                          ; 0xc10d0
    xor si, si                                ; 31 f6                       ; 0xc10d1 vgabios.c:672
    cmp si, di                                ; 39 fe                       ; 0xc10d3
    jnc short 0113ch                          ; 73 65                       ; 0xc10d5
    mov al, bl                                ; 88 d8                       ; 0xc10d7 vgabios.c:675
    mov dx, 003c7h                            ; ba c7 03                    ; 0xc10d9
    out DX, AL                                ; ee                          ; 0xc10dc
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc10dd vgabios.c:677
    in AL, DX                                 ; ec                          ; 0xc10e0
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc10e1
    mov cx, ax                                ; 89 c1                       ; 0xc10e3
    in AL, DX                                 ; ec                          ; 0xc10e5 vgabios.c:678
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc10e6
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc10e8
    in AL, DX                                 ; ec                          ; 0xc10eb vgabios.c:679
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc10ec
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc10ee
    mov al, cl                                ; 88 c8                       ; 0xc10f1 vgabios.c:682
    xor ah, ah                                ; 30 e4                       ; 0xc10f3
    mov cx, strict word 0004dh                ; b9 4d 00                    ; 0xc10f5
    imul cx                                   ; f7 e9                       ; 0xc10f8
    mov cx, ax                                ; 89 c1                       ; 0xc10fa
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc10fc
    xor ah, ah                                ; 30 e4                       ; 0xc10ff
    mov dx, 00097h                            ; ba 97 00                    ; 0xc1101
    imul dx                                   ; f7 ea                       ; 0xc1104
    add cx, ax                                ; 01 c1                       ; 0xc1106
    mov word [bp-00ah], cx                    ; 89 4e f6                    ; 0xc1108
    mov cl, byte [bp-00ch]                    ; 8a 4e f4                    ; 0xc110b
    xor ch, ch                                ; 30 ed                       ; 0xc110e
    mov ax, cx                                ; 89 c8                       ; 0xc1110
    mov dx, strict word 0001ch                ; ba 1c 00                    ; 0xc1112
    imul dx                                   ; f7 ea                       ; 0xc1115
    add ax, word [bp-00ah]                    ; 03 46 f6                    ; 0xc1117
    add ax, 00080h                            ; 05 80 00                    ; 0xc111a
    mov al, ah                                ; 88 e0                       ; 0xc111d
    cbw                                       ; 98                          ; 0xc111f
    mov cx, ax                                ; 89 c1                       ; 0xc1120
    cmp ax, strict word 0003fh                ; 3d 3f 00                    ; 0xc1122 vgabios.c:684
    jbe short 0112ah                          ; 76 03                       ; 0xc1125
    mov cx, strict word 0003fh                ; b9 3f 00                    ; 0xc1127
    mov al, bl                                ; 88 d8                       ; 0xc112a vgabios.c:687
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc112c
    out DX, AL                                ; ee                          ; 0xc112f
    mov al, cl                                ; 88 c8                       ; 0xc1130 vgabios.c:689
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc1132
    out DX, AL                                ; ee                          ; 0xc1135
    out DX, AL                                ; ee                          ; 0xc1136 vgabios.c:690
    out DX, AL                                ; ee                          ; 0xc1137 vgabios.c:691
    inc bx                                    ; 43                          ; 0xc1138 vgabios.c:692
    inc si                                    ; 46                          ; 0xc1139 vgabios.c:693
    jmp short 010d3h                          ; eb 97                       ; 0xc113a
    mov dx, 003dah                            ; ba da 03                    ; 0xc113c vgabios.c:694
    in AL, DX                                 ; ec                          ; 0xc113f
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc1140
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc1142 vgabios.c:695
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc1144
    out DX, AL                                ; ee                          ; 0xc1147
    mov dx, 003dah                            ; ba da 03                    ; 0xc1148 vgabios.c:697
    in AL, DX                                 ; ec                          ; 0xc114b
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc114c
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc114e vgabios.c:699
    pop di                                    ; 5f                          ; 0xc1151
    pop si                                    ; 5e                          ; 0xc1152
    pop cx                                    ; 59                          ; 0xc1153
    pop bx                                    ; 5b                          ; 0xc1154
    pop bp                                    ; 5d                          ; 0xc1155
    retn                                      ; c3                          ; 0xc1156
  ; disGetNextSymbol 0xc1157 LB 0x313d -> off=0x0 cb=00000000000000fc uValue=00000000000c1157 'biosfn_set_cursor_shape'
biosfn_set_cursor_shape:                     ; 0xc1157 LB 0xfc
    push bp                                   ; 55                          ; 0xc1157 vgabios.c:702
    mov bp, sp                                ; 89 e5                       ; 0xc1158
    push bx                                   ; 53                          ; 0xc115a
    push cx                                   ; 51                          ; 0xc115b
    push si                                   ; 56                          ; 0xc115c
    push ax                                   ; 50                          ; 0xc115d
    push ax                                   ; 50                          ; 0xc115e
    mov ah, al                                ; 88 c4                       ; 0xc115f
    mov bl, dl                                ; 88 d3                       ; 0xc1161
    mov dh, al                                ; 88 c6                       ; 0xc1163 vgabios.c:708
    mov si, strict word 00060h                ; be 60 00                    ; 0xc1165 vgabios.c:52
    mov cx, strict word 00040h                ; b9 40 00                    ; 0xc1168
    mov es, cx                                ; 8e c1                       ; 0xc116b
    mov word [es:si], dx                      ; 26 89 14                    ; 0xc116d
    mov si, 00087h                            ; be 87 00                    ; 0xc1170 vgabios.c:37
    mov dl, byte [es:si]                      ; 26 8a 14                    ; 0xc1173
    test dl, 008h                             ; f6 c2 08                    ; 0xc1176 vgabios.c:38
    jne short 011b8h                          ; 75 3d                       ; 0xc1179
    mov dl, al                                ; 88 c2                       ; 0xc117b vgabios.c:714
    and dl, 060h                              ; 80 e2 60                    ; 0xc117d
    cmp dl, 020h                              ; 80 fa 20                    ; 0xc1180
    jne short 0118bh                          ; 75 06                       ; 0xc1183
    mov AH, strict byte 01eh                  ; b4 1e                       ; 0xc1185 vgabios.c:716
    xor bl, bl                                ; 30 db                       ; 0xc1187 vgabios.c:717
    jmp short 011b8h                          ; eb 2d                       ; 0xc1189 vgabios.c:718
    mov dl, byte [es:si]                      ; 26 8a 14                    ; 0xc118b vgabios.c:37
    test dl, 001h                             ; f6 c2 01                    ; 0xc118e vgabios.c:38
    jne short 011edh                          ; 75 5a                       ; 0xc1191
    cmp ah, 020h                              ; 80 fc 20                    ; 0xc1193
    jnc short 011edh                          ; 73 55                       ; 0xc1196
    cmp bl, 020h                              ; 80 fb 20                    ; 0xc1198
    jnc short 011edh                          ; 73 50                       ; 0xc119b
    mov si, 00085h                            ; be 85 00                    ; 0xc119d vgabios.c:47
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc11a0
    mov es, dx                                ; 8e c2                       ; 0xc11a3
    mov cx, word [es:si]                      ; 26 8b 0c                    ; 0xc11a5
    mov dx, cx                                ; 89 ca                       ; 0xc11a8 vgabios.c:48
    cmp bl, ah                                ; 38 e3                       ; 0xc11aa vgabios.c:729
    jnc short 011bah                          ; 73 0c                       ; 0xc11ac
    test bl, bl                               ; 84 db                       ; 0xc11ae vgabios.c:731
    je short 011edh                           ; 74 3b                       ; 0xc11b0
    xor ah, ah                                ; 30 e4                       ; 0xc11b2 vgabios.c:732
    mov bl, cl                                ; 88 cb                       ; 0xc11b4 vgabios.c:733
    db  0feh, 0cbh
    ; dec bl                                    ; fe cb                     ; 0xc11b6
    jmp short 011edh                          ; eb 33                       ; 0xc11b8 vgabios.c:735
    mov byte [bp-008h], bl                    ; 88 5e f8                    ; 0xc11ba vgabios.c:736
    xor al, al                                ; 30 c0                       ; 0xc11bd
    mov byte [bp-007h], al                    ; 88 46 f9                    ; 0xc11bf
    mov byte [bp-00ah], ah                    ; 88 66 f6                    ; 0xc11c2
    mov byte [bp-009h], al                    ; 88 46 f7                    ; 0xc11c5
    mov si, word [bp-008h]                    ; 8b 76 f8                    ; 0xc11c8
    or si, word [bp-00ah]                     ; 0b 76 f6                    ; 0xc11cb
    cmp si, cx                                ; 39 ce                       ; 0xc11ce
    jnc short 011efh                          ; 73 1d                       ; 0xc11d0
    mov byte [bp-008h], bl                    ; 88 5e f8                    ; 0xc11d2
    mov byte [bp-007h], al                    ; 88 46 f9                    ; 0xc11d5
    mov si, cx                                ; 89 ce                       ; 0xc11d8
    dec si                                    ; 4e                          ; 0xc11da
    cmp si, word [bp-008h]                    ; 3b 76 f8                    ; 0xc11db
    je short 01229h                           ; 74 49                       ; 0xc11de
    mov byte [bp-008h], ah                    ; 88 66 f8                    ; 0xc11e0
    mov byte [bp-007h], al                    ; 88 46 f9                    ; 0xc11e3
    dec cx                                    ; 49                          ; 0xc11e6
    dec cx                                    ; 49                          ; 0xc11e7
    cmp cx, word [bp-008h]                    ; 3b 4e f8                    ; 0xc11e8
    jne short 011efh                          ; 75 02                       ; 0xc11eb
    jmp short 01229h                          ; eb 3a                       ; 0xc11ed
    cmp bl, 003h                              ; 80 fb 03                    ; 0xc11ef vgabios.c:738
    jbe short 01229h                          ; 76 35                       ; 0xc11f2
    mov cl, ah                                ; 88 e1                       ; 0xc11f4 vgabios.c:739
    xor ch, ch                                ; 30 ed                       ; 0xc11f6
    mov byte [bp-008h], bl                    ; 88 5e f8                    ; 0xc11f8
    mov byte [bp-007h], ch                    ; 88 6e f9                    ; 0xc11fb
    mov si, cx                                ; 89 ce                       ; 0xc11fe
    inc si                                    ; 46                          ; 0xc1200
    inc si                                    ; 46                          ; 0xc1201
    mov cl, dl                                ; 88 d1                       ; 0xc1202
    db  0feh, 0c9h
    ; dec cl                                    ; fe c9                     ; 0xc1204
    cmp si, word [bp-008h]                    ; 3b 76 f8                    ; 0xc1206
    jl short 0121eh                           ; 7c 13                       ; 0xc1209
    sub ah, bl                                ; 28 dc                       ; 0xc120b vgabios.c:741
    add ah, dl                                ; 00 d4                       ; 0xc120d
    db  0feh, 0cch
    ; dec ah                                    ; fe cc                     ; 0xc120f
    mov bl, cl                                ; 88 cb                       ; 0xc1211 vgabios.c:742
    cmp dx, strict byte 0000eh                ; 83 fa 0e                    ; 0xc1213 vgabios.c:743
    jc short 01229h                           ; 72 11                       ; 0xc1216
    db  0feh, 0cbh
    ; dec bl                                    ; fe cb                     ; 0xc1218 vgabios.c:745
    db  0feh, 0cch
    ; dec ah                                    ; fe cc                     ; 0xc121a vgabios.c:746
    jmp short 01229h                          ; eb 0b                       ; 0xc121c vgabios.c:748
    cmp ah, 002h                              ; 80 fc 02                    ; 0xc121e
    jbe short 01227h                          ; 76 04                       ; 0xc1221
    shr dx, 1                                 ; d1 ea                       ; 0xc1223 vgabios.c:750
    mov ah, dl                                ; 88 d4                       ; 0xc1225
    mov bl, cl                                ; 88 cb                       ; 0xc1227 vgabios.c:754
    mov si, strict word 00063h                ; be 63 00                    ; 0xc1229 vgabios.c:47
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc122c
    mov es, dx                                ; 8e c2                       ; 0xc122f
    mov cx, word [es:si]                      ; 26 8b 0c                    ; 0xc1231
    mov AL, strict byte 00ah                  ; b0 0a                       ; 0xc1234 vgabios.c:765
    mov dx, cx                                ; 89 ca                       ; 0xc1236
    out DX, AL                                ; ee                          ; 0xc1238
    mov si, cx                                ; 89 ce                       ; 0xc1239 vgabios.c:766
    inc si                                    ; 46                          ; 0xc123b
    mov al, ah                                ; 88 e0                       ; 0xc123c
    mov dx, si                                ; 89 f2                       ; 0xc123e
    out DX, AL                                ; ee                          ; 0xc1240
    mov AL, strict byte 00bh                  ; b0 0b                       ; 0xc1241 vgabios.c:767
    mov dx, cx                                ; 89 ca                       ; 0xc1243
    out DX, AL                                ; ee                          ; 0xc1245
    mov al, bl                                ; 88 d8                       ; 0xc1246 vgabios.c:768
    mov dx, si                                ; 89 f2                       ; 0xc1248
    out DX, AL                                ; ee                          ; 0xc124a
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc124b vgabios.c:769
    pop si                                    ; 5e                          ; 0xc124e
    pop cx                                    ; 59                          ; 0xc124f
    pop bx                                    ; 5b                          ; 0xc1250
    pop bp                                    ; 5d                          ; 0xc1251
    retn                                      ; c3                          ; 0xc1252
  ; disGetNextSymbol 0xc1253 LB 0x3041 -> off=0x0 cb=000000000000008d uValue=00000000000c1253 'biosfn_set_cursor_pos'
biosfn_set_cursor_pos:                       ; 0xc1253 LB 0x8d
    push bp                                   ; 55                          ; 0xc1253 vgabios.c:772
    mov bp, sp                                ; 89 e5                       ; 0xc1254
    push bx                                   ; 53                          ; 0xc1256
    push cx                                   ; 51                          ; 0xc1257
    push si                                   ; 56                          ; 0xc1258
    push di                                   ; 57                          ; 0xc1259
    push ax                                   ; 50                          ; 0xc125a
    mov bl, al                                ; 88 c3                       ; 0xc125b
    mov cx, dx                                ; 89 d1                       ; 0xc125d
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc125f vgabios.c:778
    jnbe short 012d7h                         ; 77 74                       ; 0xc1261
    xor ah, ah                                ; 30 e4                       ; 0xc1263 vgabios.c:781
    mov si, ax                                ; 89 c6                       ; 0xc1265
    sal si, 1                                 ; d1 e6                       ; 0xc1267
    add si, strict byte 00050h                ; 83 c6 50                    ; 0xc1269
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc126c vgabios.c:52
    mov es, ax                                ; 8e c0                       ; 0xc126f
    mov word [es:si], dx                      ; 26 89 14                    ; 0xc1271
    mov si, strict word 00062h                ; be 62 00                    ; 0xc1274 vgabios.c:37
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc1277
    cmp bl, al                                ; 38 c3                       ; 0xc127a vgabios.c:785
    jne short 012d7h                          ; 75 59                       ; 0xc127c
    mov si, strict word 0004ah                ; be 4a 00                    ; 0xc127e vgabios.c:47
    mov si, word [es:si]                      ; 26 8b 34                    ; 0xc1281
    mov di, 00084h                            ; bf 84 00                    ; 0xc1284 vgabios.c:37
    mov al, byte [es:di]                      ; 26 8a 05                    ; 0xc1287
    xor ah, ah                                ; 30 e4                       ; 0xc128a vgabios.c:38
    mov di, ax                                ; 89 c7                       ; 0xc128c
    inc di                                    ; 47                          ; 0xc128e
    mov ax, dx                                ; 89 d0                       ; 0xc128f vgabios.c:791
    mov al, dh                                ; 88 f0                       ; 0xc1291
    xor ah, dh                                ; 30 f4                       ; 0xc1293
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc1295
    mov ax, si                                ; 89 f0                       ; 0xc1298 vgabios.c:794
    mul di                                    ; f7 e7                       ; 0xc129a
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc129c
    xor bh, bh                                ; 30 ff                       ; 0xc129e
    inc ax                                    ; 40                          ; 0xc12a0
    mul bx                                    ; f7 e3                       ; 0xc12a1
    mov bx, ax                                ; 89 c3                       ; 0xc12a3
    mov al, cl                                ; 88 c8                       ; 0xc12a5
    xor ah, ah                                ; 30 e4                       ; 0xc12a7
    add bx, ax                                ; 01 c3                       ; 0xc12a9
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc12ab
    mul si                                    ; f7 e6                       ; 0xc12ae
    mov si, bx                                ; 89 de                       ; 0xc12b0
    add si, ax                                ; 01 c6                       ; 0xc12b2
    mov bx, strict word 00063h                ; bb 63 00                    ; 0xc12b4 vgabios.c:47
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc12b7
    mov AL, strict byte 00eh                  ; b0 0e                       ; 0xc12ba vgabios.c:798
    mov dx, bx                                ; 89 da                       ; 0xc12bc
    out DX, AL                                ; ee                          ; 0xc12be
    mov ax, si                                ; 89 f0                       ; 0xc12bf vgabios.c:799
    mov al, ah                                ; 88 e0                       ; 0xc12c1
    lea cx, [bx+001h]                         ; 8d 4f 01                    ; 0xc12c3
    mov dx, cx                                ; 89 ca                       ; 0xc12c6
    out DX, AL                                ; ee                          ; 0xc12c8
    mov AL, strict byte 00fh                  ; b0 0f                       ; 0xc12c9 vgabios.c:800
    mov dx, bx                                ; 89 da                       ; 0xc12cb
    out DX, AL                                ; ee                          ; 0xc12cd
    and si, 000ffh                            ; 81 e6 ff 00                 ; 0xc12ce vgabios.c:801
    mov ax, si                                ; 89 f0                       ; 0xc12d2
    mov dx, cx                                ; 89 ca                       ; 0xc12d4
    out DX, AL                                ; ee                          ; 0xc12d6
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc12d7 vgabios.c:803
    pop di                                    ; 5f                          ; 0xc12da
    pop si                                    ; 5e                          ; 0xc12db
    pop cx                                    ; 59                          ; 0xc12dc
    pop bx                                    ; 5b                          ; 0xc12dd
    pop bp                                    ; 5d                          ; 0xc12de
    retn                                      ; c3                          ; 0xc12df
  ; disGetNextSymbol 0xc12e0 LB 0x2fb4 -> off=0x0 cb=00000000000000d5 uValue=00000000000c12e0 'biosfn_set_active_page'
biosfn_set_active_page:                      ; 0xc12e0 LB 0xd5
    push bp                                   ; 55                          ; 0xc12e0 vgabios.c:806
    mov bp, sp                                ; 89 e5                       ; 0xc12e1
    push bx                                   ; 53                          ; 0xc12e3
    push cx                                   ; 51                          ; 0xc12e4
    push dx                                   ; 52                          ; 0xc12e5
    push si                                   ; 56                          ; 0xc12e6
    push di                                   ; 57                          ; 0xc12e7
    sub sp, strict byte 00006h                ; 83 ec 06                    ; 0xc12e8
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc12eb
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc12ee vgabios.c:812
    jnbe short 01308h                         ; 77 16                       ; 0xc12f0
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc12f2 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc12f5
    mov es, ax                                ; 8e c0                       ; 0xc12f8
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc12fa
    xor ah, ah                                ; 30 e4                       ; 0xc12fd vgabios.c:816
    call 03630h                               ; e8 2e 23                    ; 0xc12ff
    mov cl, al                                ; 88 c1                       ; 0xc1302
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc1304 vgabios.c:817
    jne short 0130bh                          ; 75 03                       ; 0xc1306
    jmp near 013abh                           ; e9 a0 00                    ; 0xc1308
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc130b vgabios.c:820
    xor ah, ah                                ; 30 e4                       ; 0xc130e
    lea bx, [bp-010h]                         ; 8d 5e f0                    ; 0xc1310
    lea dx, [bp-00eh]                         ; 8d 56 f2                    ; 0xc1313
    call 00a1bh                               ; e8 02 f7                    ; 0xc1316
    mov bl, cl                                ; 88 cb                       ; 0xc1319 vgabios.c:822
    xor bh, bh                                ; 30 ff                       ; 0xc131b
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc131d
    mov si, bx                                ; 89 de                       ; 0xc131f
    sal si, CL                                ; d3 e6                       ; 0xc1321
    cmp byte [si+047afh], 000h                ; 80 bc af 47 00              ; 0xc1323
    jne short 01365h                          ; 75 3b                       ; 0xc1328
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc132a vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc132d
    mov es, ax                                ; 8e c0                       ; 0xc1330
    mov dx, word [es:bx]                      ; 26 8b 17                    ; 0xc1332
    mov bx, 00084h                            ; bb 84 00                    ; 0xc1335 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc1338
    xor ah, ah                                ; 30 e4                       ; 0xc133b vgabios.c:38
    mov bx, ax                                ; 89 c3                       ; 0xc133d
    inc bx                                    ; 43                          ; 0xc133f
    mov ax, dx                                ; 89 d0                       ; 0xc1340 vgabios.c:829
    mul bx                                    ; f7 e3                       ; 0xc1342
    mov di, ax                                ; 89 c7                       ; 0xc1344
    sal ax, 1                                 ; d1 e0                       ; 0xc1346
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc1348
    mov bl, byte [bp-00ch]                    ; 8a 5e f4                    ; 0xc134a
    xor bh, bh                                ; 30 ff                       ; 0xc134d
    inc ax                                    ; 40                          ; 0xc134f
    mul bx                                    ; f7 e3                       ; 0xc1350
    mov cx, ax                                ; 89 c1                       ; 0xc1352
    mov si, strict word 0004eh                ; be 4e 00                    ; 0xc1354 vgabios.c:52
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc1357
    or di, 000ffh                             ; 81 cf ff 00                 ; 0xc135a vgabios.c:833
    lea ax, [di+001h]                         ; 8d 45 01                    ; 0xc135e
    mul bx                                    ; f7 e3                       ; 0xc1361
    jmp short 01376h                          ; eb 11                       ; 0xc1363 vgabios.c:835
    mov bl, byte [bx+0482eh]                  ; 8a 9f 2e 48                 ; 0xc1365 vgabios.c:837
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc1369
    sal bx, CL                                ; d3 e3                       ; 0xc136b
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc136d
    xor ah, ah                                ; 30 e4                       ; 0xc1370
    mul word [bx+04845h]                      ; f7 a7 45 48                 ; 0xc1372
    mov cx, ax                                ; 89 c1                       ; 0xc1376
    mov bx, strict word 00063h                ; bb 63 00                    ; 0xc1378 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc137b
    mov es, ax                                ; 8e c0                       ; 0xc137e
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc1380
    mov AL, strict byte 00ch                  ; b0 0c                       ; 0xc1383 vgabios.c:842
    mov dx, bx                                ; 89 da                       ; 0xc1385
    out DX, AL                                ; ee                          ; 0xc1387
    mov al, ch                                ; 88 e8                       ; 0xc1388 vgabios.c:843
    lea si, [bx+001h]                         ; 8d 77 01                    ; 0xc138a
    mov dx, si                                ; 89 f2                       ; 0xc138d
    out DX, AL                                ; ee                          ; 0xc138f
    mov AL, strict byte 00dh                  ; b0 0d                       ; 0xc1390 vgabios.c:844
    mov dx, bx                                ; 89 da                       ; 0xc1392
    out DX, AL                                ; ee                          ; 0xc1394
    xor ch, ch                                ; 30 ed                       ; 0xc1395 vgabios.c:845
    mov ax, cx                                ; 89 c8                       ; 0xc1397
    mov dx, si                                ; 89 f2                       ; 0xc1399
    out DX, AL                                ; ee                          ; 0xc139b
    mov bx, strict word 00062h                ; bb 62 00                    ; 0xc139c vgabios.c:42
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc139f
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc13a2
    mov dx, word [bp-010h]                    ; 8b 56 f0                    ; 0xc13a5 vgabios.c:855
    call 01253h                               ; e8 a8 fe                    ; 0xc13a8
    lea sp, [bp-00ah]                         ; 8d 66 f6                    ; 0xc13ab vgabios.c:856
    pop di                                    ; 5f                          ; 0xc13ae
    pop si                                    ; 5e                          ; 0xc13af
    pop dx                                    ; 5a                          ; 0xc13b0
    pop cx                                    ; 59                          ; 0xc13b1
    pop bx                                    ; 5b                          ; 0xc13b2
    pop bp                                    ; 5d                          ; 0xc13b3
    retn                                      ; c3                          ; 0xc13b4
  ; disGetNextSymbol 0xc13b5 LB 0x2edf -> off=0x0 cb=0000000000000387 uValue=00000000000c13b5 'biosfn_set_video_mode'
biosfn_set_video_mode:                       ; 0xc13b5 LB 0x387
    push bp                                   ; 55                          ; 0xc13b5 vgabios.c:876
    mov bp, sp                                ; 89 e5                       ; 0xc13b6
    push bx                                   ; 53                          ; 0xc13b8
    push cx                                   ; 51                          ; 0xc13b9
    push dx                                   ; 52                          ; 0xc13ba
    push si                                   ; 56                          ; 0xc13bb
    push di                                   ; 57                          ; 0xc13bc
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc13bd
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc13c0
    and AL, strict byte 080h                  ; 24 80                       ; 0xc13c3 vgabios.c:880
    mov byte [bp-012h], al                    ; 88 46 ee                    ; 0xc13c5
    call 007e8h                               ; e8 1d f4                    ; 0xc13c8 vgabios.c:888
    test ax, ax                               ; 85 c0                       ; 0xc13cb
    je short 013dbh                           ; 74 0c                       ; 0xc13cd
    mov AL, strict byte 007h                  ; b0 07                       ; 0xc13cf vgabios.c:890
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc13d1
    out DX, AL                                ; ee                          ; 0xc13d4
    xor al, al                                ; 30 c0                       ; 0xc13d5 vgabios.c:891
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc13d7
    out DX, AL                                ; ee                          ; 0xc13da
    and byte [bp-00ch], 07fh                  ; 80 66 f4 7f                 ; 0xc13db vgabios.c:896
    cmp byte [bp-00ch], 007h                  ; 80 7e f4 07                 ; 0xc13df vgabios.c:900
    jne short 013e9h                          ; 75 04                       ; 0xc13e3
    mov byte [bp-00ch], 000h                  ; c6 46 f4 00                 ; 0xc13e5 vgabios.c:901
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc13e9 vgabios.c:904
    xor ah, ah                                ; 30 e4                       ; 0xc13ec
    call 03630h                               ; e8 3f 22                    ; 0xc13ee
    mov bl, al                                ; 88 c3                       ; 0xc13f1
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc13f3
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc13f6 vgabios.c:910
    je short 01456h                           ; 74 5c                       ; 0xc13f8
    xor bh, bh                                ; 30 ff                       ; 0xc13fa vgabios.c:913
    mov al, byte [bx+0482eh]                  ; 8a 87 2e 48                 ; 0xc13fc
    mov byte [bp-00eh], al                    ; 88 46 f2                    ; 0xc1400
    mov si, 00089h                            ; be 89 00                    ; 0xc1403 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1406
    mov es, ax                                ; 8e c0                       ; 0xc1409
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc140b
    mov ch, al                                ; 88 c5                       ; 0xc140e vgabios.c:38
    test AL, strict byte 008h                 ; a8 08                       ; 0xc1410 vgabios.c:930
    jne short 01459h                          ; 75 45                       ; 0xc1412
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc1414 vgabios.c:932
    sal bx, CL                                ; d3 e3                       ; 0xc1416
    mov al, byte [bx+047b4h]                  ; 8a 87 b4 47                 ; 0xc1418
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc141c
    out DX, AL                                ; ee                          ; 0xc141f
    xor al, al                                ; 30 c0                       ; 0xc1420 vgabios.c:935
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc1422
    out DX, AL                                ; ee                          ; 0xc1425
    mov bl, byte [bx+047b5h]                  ; 8a 9f b5 47                 ; 0xc1426 vgabios.c:938
    cmp bl, 001h                              ; 80 fb 01                    ; 0xc142a
    jc short 0143ch                           ; 72 0d                       ; 0xc142d
    jbe short 01445h                          ; 76 14                       ; 0xc142f
    cmp bl, cl                                ; 38 cb                       ; 0xc1431
    je short 0144fh                           ; 74 1a                       ; 0xc1433
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc1435
    je short 0144ah                           ; 74 10                       ; 0xc1438
    jmp short 01452h                          ; eb 16                       ; 0xc143a
    test bl, bl                               ; 84 db                       ; 0xc143c
    jne short 01452h                          ; 75 12                       ; 0xc143e
    mov di, 04fc2h                            ; bf c2 4f                    ; 0xc1440 vgabios.c:940
    jmp short 01452h                          ; eb 0d                       ; 0xc1443 vgabios.c:941
    mov di, 05082h                            ; bf 82 50                    ; 0xc1445 vgabios.c:943
    jmp short 01452h                          ; eb 08                       ; 0xc1448 vgabios.c:944
    mov di, 05142h                            ; bf 42 51                    ; 0xc144a vgabios.c:946
    jmp short 01452h                          ; eb 03                       ; 0xc144d vgabios.c:947
    mov di, 05202h                            ; bf 02 52                    ; 0xc144f vgabios.c:949
    xor bx, bx                                ; 31 db                       ; 0xc1452 vgabios.c:953
    jmp short 01461h                          ; eb 0b                       ; 0xc1454
    jmp near 01732h                           ; e9 d9 02                    ; 0xc1456
    jmp short 014adh                          ; eb 52                       ; 0xc1459
    cmp bx, 00100h                            ; 81 fb 00 01                 ; 0xc145b
    jnc short 014a0h                          ; 73 3f                       ; 0xc145f
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1461 vgabios.c:954
    xor ah, ah                                ; 30 e4                       ; 0xc1464
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc1466
    mov si, ax                                ; 89 c6                       ; 0xc1468
    sal si, CL                                ; d3 e6                       ; 0xc146a
    mov al, byte [si+047b5h]                  ; 8a 84 b5 47                 ; 0xc146c
    mov si, ax                                ; 89 c6                       ; 0xc1470
    mov al, byte [si+0483eh]                  ; 8a 84 3e 48                 ; 0xc1472
    cmp bx, ax                                ; 39 c3                       ; 0xc1476
    jnbe short 01495h                         ; 77 1b                       ; 0xc1478
    mov ax, bx                                ; 89 d8                       ; 0xc147a vgabios.c:955
    mov dx, strict word 00003h                ; ba 03 00                    ; 0xc147c
    mul dx                                    ; f7 e2                       ; 0xc147f
    mov si, di                                ; 89 fe                       ; 0xc1481
    add si, ax                                ; 01 c6                       ; 0xc1483
    mov al, byte [si]                         ; 8a 04                       ; 0xc1485
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc1487
    out DX, AL                                ; ee                          ; 0xc148a
    mov al, byte [si+001h]                    ; 8a 44 01                    ; 0xc148b vgabios.c:956
    out DX, AL                                ; ee                          ; 0xc148e
    mov al, byte [si+002h]                    ; 8a 44 02                    ; 0xc148f vgabios.c:957
    out DX, AL                                ; ee                          ; 0xc1492
    jmp short 0149dh                          ; eb 08                       ; 0xc1493 vgabios.c:959
    xor al, al                                ; 30 c0                       ; 0xc1495 vgabios.c:960
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc1497
    out DX, AL                                ; ee                          ; 0xc149a
    out DX, AL                                ; ee                          ; 0xc149b vgabios.c:961
    out DX, AL                                ; ee                          ; 0xc149c vgabios.c:962
    inc bx                                    ; 43                          ; 0xc149d vgabios.c:964
    jmp short 0145bh                          ; eb bb                       ; 0xc149e
    test ch, 002h                             ; f6 c5 02                    ; 0xc14a0 vgabios.c:965
    je short 014adh                           ; 74 08                       ; 0xc14a3
    mov dx, 00100h                            ; ba 00 01                    ; 0xc14a5 vgabios.c:967
    xor ax, ax                                ; 31 c0                       ; 0xc14a8
    call 010b8h                               ; e8 0b fc                    ; 0xc14aa
    mov dx, 003dah                            ; ba da 03                    ; 0xc14ad vgabios.c:972
    in AL, DX                                 ; ec                          ; 0xc14b0
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc14b1
    xor bx, bx                                ; 31 db                       ; 0xc14b3 vgabios.c:975
    jmp short 014bch                          ; eb 05                       ; 0xc14b5
    cmp bx, strict byte 00013h                ; 83 fb 13                    ; 0xc14b7
    jnbe short 014d7h                         ; 77 1b                       ; 0xc14ba
    mov al, bl                                ; 88 d8                       ; 0xc14bc vgabios.c:976
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc14be
    out DX, AL                                ; ee                          ; 0xc14c1
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc14c2 vgabios.c:977
    xor ah, ah                                ; 30 e4                       ; 0xc14c5
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc14c7
    mov si, ax                                ; 89 c6                       ; 0xc14c9
    sal si, CL                                ; d3 e6                       ; 0xc14cb
    add si, bx                                ; 01 de                       ; 0xc14cd
    mov al, byte [si+04865h]                  ; 8a 84 65 48                 ; 0xc14cf
    out DX, AL                                ; ee                          ; 0xc14d3
    inc bx                                    ; 43                          ; 0xc14d4 vgabios.c:978
    jmp short 014b7h                          ; eb e0                       ; 0xc14d5
    mov AL, strict byte 014h                  ; b0 14                       ; 0xc14d7 vgabios.c:979
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc14d9
    out DX, AL                                ; ee                          ; 0xc14dc
    xor al, al                                ; 30 c0                       ; 0xc14dd vgabios.c:980
    out DX, AL                                ; ee                          ; 0xc14df
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc14e0 vgabios.c:983
    out DX, AL                                ; ee                          ; 0xc14e3
    mov AL, strict byte 003h                  ; b0 03                       ; 0xc14e4 vgabios.c:984
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc14e6
    out DX, AL                                ; ee                          ; 0xc14e9
    mov bx, strict word 00001h                ; bb 01 00                    ; 0xc14ea vgabios.c:985
    jmp short 014f4h                          ; eb 05                       ; 0xc14ed
    cmp bx, strict byte 00004h                ; 83 fb 04                    ; 0xc14ef
    jnbe short 01512h                         ; 77 1e                       ; 0xc14f2
    mov al, bl                                ; 88 d8                       ; 0xc14f4 vgabios.c:986
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc14f6
    out DX, AL                                ; ee                          ; 0xc14f9
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc14fa vgabios.c:987
    xor ah, ah                                ; 30 e4                       ; 0xc14fd
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc14ff
    mov si, ax                                ; 89 c6                       ; 0xc1501
    sal si, CL                                ; d3 e6                       ; 0xc1503
    add si, bx                                ; 01 de                       ; 0xc1505
    mov al, byte [si+04846h]                  ; 8a 84 46 48                 ; 0xc1507
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc150b
    out DX, AL                                ; ee                          ; 0xc150e
    inc bx                                    ; 43                          ; 0xc150f vgabios.c:988
    jmp short 014efh                          ; eb dd                       ; 0xc1510
    xor bx, bx                                ; 31 db                       ; 0xc1512 vgabios.c:991
    jmp short 0151bh                          ; eb 05                       ; 0xc1514
    cmp bx, strict byte 00008h                ; 83 fb 08                    ; 0xc1516
    jnbe short 01539h                         ; 77 1e                       ; 0xc1519
    mov al, bl                                ; 88 d8                       ; 0xc151b vgabios.c:992
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc151d
    out DX, AL                                ; ee                          ; 0xc1520
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1521 vgabios.c:993
    xor ah, ah                                ; 30 e4                       ; 0xc1524
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc1526
    mov si, ax                                ; 89 c6                       ; 0xc1528
    sal si, CL                                ; d3 e6                       ; 0xc152a
    add si, bx                                ; 01 de                       ; 0xc152c
    mov al, byte [si+04879h]                  ; 8a 84 79 48                 ; 0xc152e
    mov dx, 003cfh                            ; ba cf 03                    ; 0xc1532
    out DX, AL                                ; ee                          ; 0xc1535
    inc bx                                    ; 43                          ; 0xc1536 vgabios.c:994
    jmp short 01516h                          ; eb dd                       ; 0xc1537
    mov bl, byte [bp-010h]                    ; 8a 5e f0                    ; 0xc1539 vgabios.c:997
    xor bh, bh                                ; 30 ff                       ; 0xc153c
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc153e
    sal bx, CL                                ; d3 e3                       ; 0xc1540
    cmp byte [bx+047b0h], 001h                ; 80 bf b0 47 01              ; 0xc1542
    jne short 0154eh                          ; 75 05                       ; 0xc1547
    mov dx, 003b4h                            ; ba b4 03                    ; 0xc1549
    jmp short 01551h                          ; eb 03                       ; 0xc154c
    mov dx, 003d4h                            ; ba d4 03                    ; 0xc154e
    mov si, dx                                ; 89 d6                       ; 0xc1551
    mov ax, strict word 00011h                ; b8 11 00                    ; 0xc1553 vgabios.c:1000
    out DX, ax                                ; ef                          ; 0xc1556
    xor bx, bx                                ; 31 db                       ; 0xc1557 vgabios.c:1002
    jmp short 01560h                          ; eb 05                       ; 0xc1559
    cmp bx, strict byte 00018h                ; 83 fb 18                    ; 0xc155b
    jnbe short 0157fh                         ; 77 1f                       ; 0xc155e
    mov al, bl                                ; 88 d8                       ; 0xc1560 vgabios.c:1003
    mov dx, si                                ; 89 f2                       ; 0xc1562
    out DX, AL                                ; ee                          ; 0xc1564
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1565 vgabios.c:1004
    xor ah, ah                                ; 30 e4                       ; 0xc1568
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc156a
    sal ax, CL                                ; d3 e0                       ; 0xc156c
    mov cx, ax                                ; 89 c1                       ; 0xc156e
    mov di, ax                                ; 89 c7                       ; 0xc1570
    add di, bx                                ; 01 df                       ; 0xc1572
    lea dx, [si+001h]                         ; 8d 54 01                    ; 0xc1574
    mov al, byte [di+0484ch]                  ; 8a 85 4c 48                 ; 0xc1577
    out DX, AL                                ; ee                          ; 0xc157b
    inc bx                                    ; 43                          ; 0xc157c vgabios.c:1005
    jmp short 0155bh                          ; eb dc                       ; 0xc157d
    mov bx, cx                                ; 89 cb                       ; 0xc157f vgabios.c:1008
    mov al, byte [bx+0484bh]                  ; 8a 87 4b 48                 ; 0xc1581
    mov dx, 003c2h                            ; ba c2 03                    ; 0xc1585
    out DX, AL                                ; ee                          ; 0xc1588
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc1589 vgabios.c:1011
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc158b
    out DX, AL                                ; ee                          ; 0xc158e
    mov dx, 003dah                            ; ba da 03                    ; 0xc158f vgabios.c:1012
    in AL, DX                                 ; ec                          ; 0xc1592
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc1593
    cmp byte [bp-012h], 000h                  ; 80 7e ee 00                 ; 0xc1595 vgabios.c:1014
    jne short 015f9h                          ; 75 5e                       ; 0xc1599
    mov bl, byte [bp-010h]                    ; 8a 5e f0                    ; 0xc159b vgabios.c:1016
    xor bh, ch                                ; 30 ef                       ; 0xc159e
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc15a0
    sal bx, CL                                ; d3 e3                       ; 0xc15a2
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc15a4
    jne short 015bdh                          ; 75 12                       ; 0xc15a9
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc15ab vgabios.c:1018
    mov cx, 04000h                            ; b9 00 40                    ; 0xc15af
    mov ax, 00720h                            ; b8 20 07                    ; 0xc15b2
    xor di, di                                ; 31 ff                       ; 0xc15b5
    jcxz 015bbh                               ; e3 02                       ; 0xc15b7
    rep stosw                                 ; f3 ab                       ; 0xc15b9
    jmp short 015f9h                          ; eb 3c                       ; 0xc15bb vgabios.c:1020
    cmp byte [bp-00ch], 00dh                  ; 80 7e f4 0d                 ; 0xc15bd vgabios.c:1022
    jnc short 015d4h                          ; 73 11                       ; 0xc15c1
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc15c3 vgabios.c:1024
    mov cx, 04000h                            ; b9 00 40                    ; 0xc15c7
    xor ax, ax                                ; 31 c0                       ; 0xc15ca
    xor di, di                                ; 31 ff                       ; 0xc15cc
    jcxz 015d2h                               ; e3 02                       ; 0xc15ce
    rep stosw                                 ; f3 ab                       ; 0xc15d0
    jmp short 015f9h                          ; eb 25                       ; 0xc15d2 vgabios.c:1026
    mov AL, strict byte 002h                  ; b0 02                       ; 0xc15d4 vgabios.c:1028
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc15d6
    out DX, AL                                ; ee                          ; 0xc15d9
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc15da vgabios.c:1029
    in AL, DX                                 ; ec                          ; 0xc15dd
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc15de
    mov word [bp-014h], ax                    ; 89 46 ec                    ; 0xc15e0
    mov AL, strict byte 00fh                  ; b0 0f                       ; 0xc15e3 vgabios.c:1030
    out DX, AL                                ; ee                          ; 0xc15e5
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc15e6 vgabios.c:1031
    mov cx, 08000h                            ; b9 00 80                    ; 0xc15ea
    xor ax, ax                                ; 31 c0                       ; 0xc15ed
    xor di, di                                ; 31 ff                       ; 0xc15ef
    jcxz 015f5h                               ; e3 02                       ; 0xc15f1
    rep stosw                                 ; f3 ab                       ; 0xc15f3
    mov al, byte [bp-014h]                    ; 8a 46 ec                    ; 0xc15f5 vgabios.c:1032
    out DX, AL                                ; ee                          ; 0xc15f8
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc15f9 vgabios.c:42
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc15fc
    mov es, ax                                ; 8e c0                       ; 0xc15ff
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1601
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc1604
    mov bl, byte [bp-00eh]                    ; 8a 5e f2                    ; 0xc1607 vgabios.c:1039
    xor bh, bh                                ; 30 ff                       ; 0xc160a
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc160c
    sal bx, CL                                ; d3 e3                       ; 0xc160e
    mov al, byte [bx+04842h]                  ; 8a 87 42 48                 ; 0xc1610
    xor ah, ah                                ; 30 e4                       ; 0xc1614
    mov di, strict word 0004ah                ; bf 4a 00                    ; 0xc1616 vgabios.c:52
    mov word [es:di], ax                      ; 26 89 05                    ; 0xc1619
    mov ax, word [bx+04845h]                  ; 8b 87 45 48                 ; 0xc161c vgabios.c:50
    mov di, strict word 0004ch                ; bf 4c 00                    ; 0xc1620 vgabios.c:52
    mov word [es:di], ax                      ; 26 89 05                    ; 0xc1623
    mov di, strict word 00063h                ; bf 63 00                    ; 0xc1626 vgabios.c:52
    mov word [es:di], si                      ; 26 89 35                    ; 0xc1629
    mov al, byte [bx+04843h]                  ; 8a 87 43 48                 ; 0xc162c vgabios.c:40
    mov si, 00084h                            ; be 84 00                    ; 0xc1630 vgabios.c:42
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc1633
    mov al, byte [bx+04844h]                  ; 8a 87 44 48                 ; 0xc1636 vgabios.c:1043
    xor ah, ah                                ; 30 e4                       ; 0xc163a
    mov bx, 00085h                            ; bb 85 00                    ; 0xc163c vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc163f
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc1642 vgabios.c:1044
    or AL, strict byte 060h                   ; 0c 60                       ; 0xc1645
    mov bx, 00087h                            ; bb 87 00                    ; 0xc1647 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc164a
    mov bx, 00088h                            ; bb 88 00                    ; 0xc164d vgabios.c:42
    mov byte [es:bx], 0f9h                    ; 26 c6 07 f9                 ; 0xc1650
    mov bx, 00089h                            ; bb 89 00                    ; 0xc1654 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc1657
    and AL, strict byte 07fh                  ; 24 7f                       ; 0xc165a vgabios.c:38
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc165c vgabios.c:42
    mov bx, 0008ah                            ; bb 8a 00                    ; 0xc165f vgabios.c:42
    mov byte [es:bx], 008h                    ; 26 c6 07 08                 ; 0xc1662
    mov ax, ds                                ; 8c d8                       ; 0xc1666 vgabios.c:1050
    mov bx, 000a8h                            ; bb a8 00                    ; 0xc1668 vgabios.c:62
    mov word [es:bx], 05550h                  ; 26 c7 07 50 55              ; 0xc166b
    mov [es:bx+002h], ds                      ; 26 8c 5f 02                 ; 0xc1670
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1674 vgabios.c:1052
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc1677
    jnbe short 016a1h                         ; 77 26                       ; 0xc1679
    mov bl, al                                ; 88 c3                       ; 0xc167b vgabios.c:1054
    xor bh, bh                                ; 30 ff                       ; 0xc167d
    mov al, byte [bx+07dddh]                  ; 8a 87 dd 7d                 ; 0xc167f vgabios.c:40
    mov bx, strict word 00065h                ; bb 65 00                    ; 0xc1683 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc1686
    cmp cl, byte [bp-00ch]                    ; 3a 4e f4                    ; 0xc1689 vgabios.c:1055
    jne short 01693h                          ; 75 05                       ; 0xc168c
    mov ax, strict word 0003fh                ; b8 3f 00                    ; 0xc168e
    jmp short 01696h                          ; eb 03                       ; 0xc1691
    mov ax, strict word 00030h                ; b8 30 00                    ; 0xc1693
    mov bx, strict word 00066h                ; bb 66 00                    ; 0xc1696 vgabios.c:42
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc1699
    mov es, dx                                ; 8e c2                       ; 0xc169c
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc169e
    mov bl, byte [bp-010h]                    ; 8a 5e f0                    ; 0xc16a1 vgabios.c:1059
    xor bh, bh                                ; 30 ff                       ; 0xc16a4
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc16a6
    sal bx, CL                                ; d3 e3                       ; 0xc16a8
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc16aa
    jne short 016bah                          ; 75 09                       ; 0xc16af
    mov dx, strict word 00007h                ; ba 07 00                    ; 0xc16b1 vgabios.c:1061
    mov ax, strict word 00006h                ; b8 06 00                    ; 0xc16b4
    call 01157h                               ; e8 9d fa                    ; 0xc16b7
    xor bx, bx                                ; 31 db                       ; 0xc16ba vgabios.c:1065
    jmp short 016c3h                          ; eb 05                       ; 0xc16bc
    cmp bx, strict byte 00008h                ; 83 fb 08                    ; 0xc16be
    jnc short 016cfh                          ; 73 0c                       ; 0xc16c1
    mov al, bl                                ; 88 d8                       ; 0xc16c3 vgabios.c:1066
    xor ah, ah                                ; 30 e4                       ; 0xc16c5
    xor dx, dx                                ; 31 d2                       ; 0xc16c7
    call 01253h                               ; e8 87 fb                    ; 0xc16c9
    inc bx                                    ; 43                          ; 0xc16cc
    jmp short 016beh                          ; eb ef                       ; 0xc16cd
    xor ax, ax                                ; 31 c0                       ; 0xc16cf vgabios.c:1069
    call 012e0h                               ; e8 0c fc                    ; 0xc16d1
    mov bl, byte [bp-010h]                    ; 8a 5e f0                    ; 0xc16d4 vgabios.c:1072
    xor bh, bh                                ; 30 ff                       ; 0xc16d7
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc16d9
    sal bx, CL                                ; d3 e3                       ; 0xc16db
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc16dd
    jne short 016f4h                          ; 75 10                       ; 0xc16e2
    xor dx, dx                                ; 31 d2                       ; 0xc16e4 vgabios.c:1074
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc16e6
    call 02d42h                               ; e8 56 16                    ; 0xc16e9
    xor bl, bl                                ; 30 db                       ; 0xc16ec vgabios.c:1075
    mov al, cl                                ; 88 c8                       ; 0xc16ee
    mov AH, strict byte 011h                  ; b4 11                       ; 0xc16f0
    int 06dh                                  ; cd 6d                       ; 0xc16f2
    mov bx, 0596ch                            ; bb 6c 59                    ; 0xc16f4 vgabios.c:1079
    mov cx, ds                                ; 8c d9                       ; 0xc16f7
    mov ax, strict word 0001fh                ; b8 1f 00                    ; 0xc16f9
    call 00980h                               ; e8 81 f2                    ; 0xc16fc
    mov bl, byte [bp-00eh]                    ; 8a 5e f2                    ; 0xc16ff vgabios.c:1081
    xor bh, bh                                ; 30 ff                       ; 0xc1702
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc1704
    sal bx, CL                                ; d3 e3                       ; 0xc1706
    mov dl, byte [bx+04844h]                  ; 8a 97 44 48                 ; 0xc1708
    cmp dl, 010h                              ; 80 fa 10                    ; 0xc170c
    je short 0172dh                           ; 74 1c                       ; 0xc170f
    cmp dl, 00eh                              ; 80 fa 0e                    ; 0xc1711
    je short 01728h                           ; 74 12                       ; 0xc1714
    cmp dl, 008h                              ; 80 fa 08                    ; 0xc1716
    jne short 01732h                          ; 75 17                       ; 0xc1719
    mov bx, 0556ch                            ; bb 6c 55                    ; 0xc171b vgabios.c:1083
    mov cx, ds                                ; 8c d9                       ; 0xc171e
    mov ax, strict word 00043h                ; b8 43 00                    ; 0xc1720
    call 00980h                               ; e8 5a f2                    ; 0xc1723
    jmp short 01732h                          ; eb 0a                       ; 0xc1726 vgabios.c:1084
    mov bx, 05d6ch                            ; bb 6c 5d                    ; 0xc1728 vgabios.c:1086
    jmp short 0171eh                          ; eb f1                       ; 0xc172b
    mov bx, 06b6ch                            ; bb 6c 6b                    ; 0xc172d vgabios.c:1089
    jmp short 0171eh                          ; eb ec                       ; 0xc1730
    lea sp, [bp-00ah]                         ; 8d 66 f6                    ; 0xc1732 vgabios.c:1092
    pop di                                    ; 5f                          ; 0xc1735
    pop si                                    ; 5e                          ; 0xc1736
    pop dx                                    ; 5a                          ; 0xc1737
    pop cx                                    ; 59                          ; 0xc1738
    pop bx                                    ; 5b                          ; 0xc1739
    pop bp                                    ; 5d                          ; 0xc173a
    retn                                      ; c3                          ; 0xc173b
  ; disGetNextSymbol 0xc173c LB 0x2b58 -> off=0x0 cb=000000000000008e uValue=00000000000c173c 'vgamem_copy_pl4'
vgamem_copy_pl4:                             ; 0xc173c LB 0x8e
    push bp                                   ; 55                          ; 0xc173c vgabios.c:1095
    mov bp, sp                                ; 89 e5                       ; 0xc173d
    push si                                   ; 56                          ; 0xc173f
    push di                                   ; 57                          ; 0xc1740
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc1741
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc1744
    mov al, dl                                ; 88 d0                       ; 0xc1747
    mov byte [bp-00ah], bl                    ; 88 5e f6                    ; 0xc1749
    mov byte [bp-006h], cl                    ; 88 4e fa                    ; 0xc174c
    xor ah, ah                                ; 30 e4                       ; 0xc174f vgabios.c:1101
    mov dl, byte [bp+006h]                    ; 8a 56 06                    ; 0xc1751
    xor dh, dh                                ; 30 f6                       ; 0xc1754
    mov cx, dx                                ; 89 d1                       ; 0xc1756
    imul dx                                   ; f7 ea                       ; 0xc1758
    mov dl, byte [bp+004h]                    ; 8a 56 04                    ; 0xc175a
    xor dh, dh                                ; 30 f6                       ; 0xc175d
    mov si, dx                                ; 89 d6                       ; 0xc175f
    imul dx                                   ; f7 ea                       ; 0xc1761
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc1763
    xor dh, dh                                ; 30 f6                       ; 0xc1766
    mov bx, dx                                ; 89 d3                       ; 0xc1768
    add ax, dx                                ; 01 d0                       ; 0xc176a
    mov word [bp-00eh], ax                    ; 89 46 f2                    ; 0xc176c
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc176f vgabios.c:1102
    xor ah, ah                                ; 30 e4                       ; 0xc1772
    imul cx                                   ; f7 e9                       ; 0xc1774
    imul si                                   ; f7 ee                       ; 0xc1776
    add ax, bx                                ; 01 d8                       ; 0xc1778
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc177a
    mov ax, 00105h                            ; b8 05 01                    ; 0xc177d vgabios.c:1103
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1780
    out DX, ax                                ; ef                          ; 0xc1783
    xor bl, bl                                ; 30 db                       ; 0xc1784 vgabios.c:1104
    cmp bl, byte [bp+006h]                    ; 3a 5e 06                    ; 0xc1786
    jnc short 017bah                          ; 73 2f                       ; 0xc1789
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc178b vgabios.c:1106
    xor ah, ah                                ; 30 e4                       ; 0xc178e
    mov cx, ax                                ; 89 c1                       ; 0xc1790
    mov al, bl                                ; 88 d8                       ; 0xc1792
    mov dx, ax                                ; 89 c2                       ; 0xc1794
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1796
    mov si, ax                                ; 89 c6                       ; 0xc1799
    mov ax, dx                                ; 89 d0                       ; 0xc179b
    imul si                                   ; f7 ee                       ; 0xc179d
    mov si, word [bp-00eh]                    ; 8b 76 f2                    ; 0xc179f
    add si, ax                                ; 01 c6                       ; 0xc17a2
    mov di, word [bp-00ch]                    ; 8b 7e f4                    ; 0xc17a4
    add di, ax                                ; 01 c7                       ; 0xc17a7
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc17a9
    mov es, dx                                ; 8e c2                       ; 0xc17ac
    jcxz 017b6h                               ; e3 06                       ; 0xc17ae
    push DS                                   ; 1e                          ; 0xc17b0
    mov ds, dx                                ; 8e da                       ; 0xc17b1
    rep movsb                                 ; f3 a4                       ; 0xc17b3
    pop DS                                    ; 1f                          ; 0xc17b5
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3                     ; 0xc17b6 vgabios.c:1107
    jmp short 01786h                          ; eb cc                       ; 0xc17b8
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc17ba vgabios.c:1108
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc17bd
    out DX, ax                                ; ef                          ; 0xc17c0
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc17c1 vgabios.c:1109
    pop di                                    ; 5f                          ; 0xc17c4
    pop si                                    ; 5e                          ; 0xc17c5
    pop bp                                    ; 5d                          ; 0xc17c6
    retn 00004h                               ; c2 04 00                    ; 0xc17c7
  ; disGetNextSymbol 0xc17ca LB 0x2aca -> off=0x0 cb=000000000000007b uValue=00000000000c17ca 'vgamem_fill_pl4'
vgamem_fill_pl4:                             ; 0xc17ca LB 0x7b
    push bp                                   ; 55                          ; 0xc17ca vgabios.c:1112
    mov bp, sp                                ; 89 e5                       ; 0xc17cb
    push si                                   ; 56                          ; 0xc17cd
    push di                                   ; 57                          ; 0xc17ce
    sub sp, strict byte 00006h                ; 83 ec 06                    ; 0xc17cf
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc17d2
    mov al, dl                                ; 88 d0                       ; 0xc17d5
    mov byte [bp-006h], bl                    ; 88 5e fa                    ; 0xc17d7
    mov bh, cl                                ; 88 cf                       ; 0xc17da
    xor ah, ah                                ; 30 e4                       ; 0xc17dc vgabios.c:1118
    mov dx, ax                                ; 89 c2                       ; 0xc17de
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc17e0
    mov cx, ax                                ; 89 c1                       ; 0xc17e3
    mov ax, dx                                ; 89 d0                       ; 0xc17e5
    imul cx                                   ; f7 e9                       ; 0xc17e7
    mov dl, bh                                ; 88 fa                       ; 0xc17e9
    xor dh, dh                                ; 30 f6                       ; 0xc17eb
    imul dx                                   ; f7 ea                       ; 0xc17ed
    mov dx, ax                                ; 89 c2                       ; 0xc17ef
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc17f1
    xor ah, ah                                ; 30 e4                       ; 0xc17f4
    add dx, ax                                ; 01 c2                       ; 0xc17f6
    mov word [bp-00ah], dx                    ; 89 56 f6                    ; 0xc17f8
    mov ax, 00205h                            ; b8 05 02                    ; 0xc17fb vgabios.c:1119
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc17fe
    out DX, ax                                ; ef                          ; 0xc1801
    xor bl, bl                                ; 30 db                       ; 0xc1802 vgabios.c:1120
    cmp bl, byte [bp+004h]                    ; 3a 5e 04                    ; 0xc1804
    jnc short 01835h                          ; 73 2c                       ; 0xc1807
    mov cl, byte [bp-006h]                    ; 8a 4e fa                    ; 0xc1809 vgabios.c:1122
    xor ch, ch                                ; 30 ed                       ; 0xc180c
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc180e
    xor ah, ah                                ; 30 e4                       ; 0xc1811
    mov si, ax                                ; 89 c6                       ; 0xc1813
    mov al, bl                                ; 88 d8                       ; 0xc1815
    mov dx, ax                                ; 89 c2                       ; 0xc1817
    mov al, bh                                ; 88 f8                       ; 0xc1819
    mov di, ax                                ; 89 c7                       ; 0xc181b
    mov ax, dx                                ; 89 d0                       ; 0xc181d
    imul di                                   ; f7 ef                       ; 0xc181f
    mov di, word [bp-00ah]                    ; 8b 7e f6                    ; 0xc1821
    add di, ax                                ; 01 c7                       ; 0xc1824
    mov ax, si                                ; 89 f0                       ; 0xc1826
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc1828
    mov es, dx                                ; 8e c2                       ; 0xc182b
    jcxz 01831h                               ; e3 02                       ; 0xc182d
    rep stosb                                 ; f3 aa                       ; 0xc182f
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3                     ; 0xc1831 vgabios.c:1123
    jmp short 01804h                          ; eb cf                       ; 0xc1833
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc1835 vgabios.c:1124
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1838
    out DX, ax                                ; ef                          ; 0xc183b
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc183c vgabios.c:1125
    pop di                                    ; 5f                          ; 0xc183f
    pop si                                    ; 5e                          ; 0xc1840
    pop bp                                    ; 5d                          ; 0xc1841
    retn 00004h                               ; c2 04 00                    ; 0xc1842
  ; disGetNextSymbol 0xc1845 LB 0x2a4f -> off=0x0 cb=00000000000000b6 uValue=00000000000c1845 'vgamem_copy_cga'
vgamem_copy_cga:                             ; 0xc1845 LB 0xb6
    push bp                                   ; 55                          ; 0xc1845 vgabios.c:1128
    mov bp, sp                                ; 89 e5                       ; 0xc1846
    push si                                   ; 56                          ; 0xc1848
    push di                                   ; 57                          ; 0xc1849
    sub sp, strict byte 0000eh                ; 83 ec 0e                    ; 0xc184a
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc184d
    mov byte [bp-008h], bl                    ; 88 5e f8                    ; 0xc1850
    mov byte [bp-00ah], cl                    ; 88 4e f6                    ; 0xc1853
    mov al, dl                                ; 88 d0                       ; 0xc1856 vgabios.c:1134
    xor ah, ah                                ; 30 e4                       ; 0xc1858
    mov bx, ax                                ; 89 c3                       ; 0xc185a
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc185c
    mov si, ax                                ; 89 c6                       ; 0xc185f
    mov ax, bx                                ; 89 d8                       ; 0xc1861
    imul si                                   ; f7 ee                       ; 0xc1863
    mov bl, byte [bp+004h]                    ; 8a 5e 04                    ; 0xc1865
    mov di, bx                                ; 89 df                       ; 0xc1868
    imul bx                                   ; f7 eb                       ; 0xc186a
    mov dx, ax                                ; 89 c2                       ; 0xc186c
    sar dx, 1                                 ; d1 fa                       ; 0xc186e
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1870
    xor ah, ah                                ; 30 e4                       ; 0xc1873
    mov bx, ax                                ; 89 c3                       ; 0xc1875
    add dx, ax                                ; 01 c2                       ; 0xc1877
    mov word [bp-00eh], dx                    ; 89 56 f2                    ; 0xc1879
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc187c vgabios.c:1135
    imul si                                   ; f7 ee                       ; 0xc187f
    imul di                                   ; f7 ef                       ; 0xc1881
    sar ax, 1                                 ; d1 f8                       ; 0xc1883
    add ax, bx                                ; 01 d8                       ; 0xc1885
    mov word [bp-010h], ax                    ; 89 46 f0                    ; 0xc1887
    mov byte [bp-006h], bh                    ; 88 7e fa                    ; 0xc188a vgabios.c:1136
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc188d
    xor ah, ah                                ; 30 e4                       ; 0xc1890
    cwd                                       ; 99                          ; 0xc1892
    db  02bh, 0c2h
    ; sub ax, dx                                ; 2b c2                     ; 0xc1893
    sar ax, 1                                 ; d1 f8                       ; 0xc1895
    mov bx, ax                                ; 89 c3                       ; 0xc1897
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1899
    xor ah, ah                                ; 30 e4                       ; 0xc189c
    cmp ax, bx                                ; 39 d8                       ; 0xc189e
    jnl short 018f2h                          ; 7d 50                       ; 0xc18a0
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc18a2 vgabios.c:1138
    xor bh, bh                                ; 30 ff                       ; 0xc18a5
    mov word [bp-012h], bx                    ; 89 5e ee                    ; 0xc18a7
    mov bl, byte [bp+004h]                    ; 8a 5e 04                    ; 0xc18aa
    imul bx                                   ; f7 eb                       ; 0xc18ad
    mov bx, ax                                ; 89 c3                       ; 0xc18af
    mov si, word [bp-00eh]                    ; 8b 76 f2                    ; 0xc18b1
    add si, ax                                ; 01 c6                       ; 0xc18b4
    mov di, word [bp-010h]                    ; 8b 7e f0                    ; 0xc18b6
    add di, ax                                ; 01 c7                       ; 0xc18b9
    mov cx, word [bp-012h]                    ; 8b 4e ee                    ; 0xc18bb
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc18be
    mov es, dx                                ; 8e c2                       ; 0xc18c1
    jcxz 018cbh                               ; e3 06                       ; 0xc18c3
    push DS                                   ; 1e                          ; 0xc18c5
    mov ds, dx                                ; 8e da                       ; 0xc18c6
    rep movsb                                 ; f3 a4                       ; 0xc18c8
    pop DS                                    ; 1f                          ; 0xc18ca
    mov si, word [bp-00eh]                    ; 8b 76 f2                    ; 0xc18cb vgabios.c:1139
    add si, 02000h                            ; 81 c6 00 20                 ; 0xc18ce
    add si, bx                                ; 01 de                       ; 0xc18d2
    mov di, word [bp-010h]                    ; 8b 7e f0                    ; 0xc18d4
    add di, 02000h                            ; 81 c7 00 20                 ; 0xc18d7
    add di, bx                                ; 01 df                       ; 0xc18db
    mov cx, word [bp-012h]                    ; 8b 4e ee                    ; 0xc18dd
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc18e0
    mov es, dx                                ; 8e c2                       ; 0xc18e3
    jcxz 018edh                               ; e3 06                       ; 0xc18e5
    push DS                                   ; 1e                          ; 0xc18e7
    mov ds, dx                                ; 8e da                       ; 0xc18e8
    rep movsb                                 ; f3 a4                       ; 0xc18ea
    pop DS                                    ; 1f                          ; 0xc18ec
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc18ed vgabios.c:1140
    jmp short 0188dh                          ; eb 9b                       ; 0xc18f0
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc18f2 vgabios.c:1141
    pop di                                    ; 5f                          ; 0xc18f5
    pop si                                    ; 5e                          ; 0xc18f6
    pop bp                                    ; 5d                          ; 0xc18f7
    retn 00004h                               ; c2 04 00                    ; 0xc18f8
  ; disGetNextSymbol 0xc18fb LB 0x2999 -> off=0x0 cb=0000000000000094 uValue=00000000000c18fb 'vgamem_fill_cga'
vgamem_fill_cga:                             ; 0xc18fb LB 0x94
    push bp                                   ; 55                          ; 0xc18fb vgabios.c:1144
    mov bp, sp                                ; 89 e5                       ; 0xc18fc
    push si                                   ; 56                          ; 0xc18fe
    push di                                   ; 57                          ; 0xc18ff
    sub sp, strict byte 0000ch                ; 83 ec 0c                    ; 0xc1900
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc1903
    mov al, dl                                ; 88 d0                       ; 0xc1906
    mov byte [bp-00ch], bl                    ; 88 5e f4                    ; 0xc1908
    mov byte [bp-008h], cl                    ; 88 4e f8                    ; 0xc190b
    xor ah, ah                                ; 30 e4                       ; 0xc190e vgabios.c:1150
    mov dx, ax                                ; 89 c2                       ; 0xc1910
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1912
    mov bx, ax                                ; 89 c3                       ; 0xc1915
    mov ax, dx                                ; 89 d0                       ; 0xc1917
    imul bx                                   ; f7 eb                       ; 0xc1919
    mov dl, cl                                ; 88 ca                       ; 0xc191b
    xor dh, dh                                ; 30 f6                       ; 0xc191d
    imul dx                                   ; f7 ea                       ; 0xc191f
    mov dx, ax                                ; 89 c2                       ; 0xc1921
    sar dx, 1                                 ; d1 fa                       ; 0xc1923
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc1925
    xor ah, ah                                ; 30 e4                       ; 0xc1928
    add dx, ax                                ; 01 c2                       ; 0xc192a
    mov word [bp-00eh], dx                    ; 89 56 f2                    ; 0xc192c
    mov byte [bp-006h], ah                    ; 88 66 fa                    ; 0xc192f vgabios.c:1151
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1932
    xor ah, ah                                ; 30 e4                       ; 0xc1935
    cwd                                       ; 99                          ; 0xc1937
    db  02bh, 0c2h
    ; sub ax, dx                                ; 2b c2                     ; 0xc1938
    sar ax, 1                                 ; d1 f8                       ; 0xc193a
    mov dx, ax                                ; 89 c2                       ; 0xc193c
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc193e
    xor ah, ah                                ; 30 e4                       ; 0xc1941
    cmp ax, dx                                ; 39 d0                       ; 0xc1943
    jnl short 01986h                          ; 7d 3f                       ; 0xc1945
    mov bl, byte [bp-00ch]                    ; 8a 5e f4                    ; 0xc1947 vgabios.c:1153
    xor bh, bh                                ; 30 ff                       ; 0xc194a
    mov dl, byte [bp+006h]                    ; 8a 56 06                    ; 0xc194c
    xor dh, dh                                ; 30 f6                       ; 0xc194f
    mov si, dx                                ; 89 d6                       ; 0xc1951
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc1953
    imul dx                                   ; f7 ea                       ; 0xc1956
    mov word [bp-010h], ax                    ; 89 46 f0                    ; 0xc1958
    mov di, word [bp-00eh]                    ; 8b 7e f2                    ; 0xc195b
    add di, ax                                ; 01 c7                       ; 0xc195e
    mov cx, bx                                ; 89 d9                       ; 0xc1960
    mov ax, si                                ; 89 f0                       ; 0xc1962
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc1964
    mov es, dx                                ; 8e c2                       ; 0xc1967
    jcxz 0196dh                               ; e3 02                       ; 0xc1969
    rep stosb                                 ; f3 aa                       ; 0xc196b
    mov di, word [bp-00eh]                    ; 8b 7e f2                    ; 0xc196d vgabios.c:1154
    add di, 02000h                            ; 81 c7 00 20                 ; 0xc1970
    add di, word [bp-010h]                    ; 03 7e f0                    ; 0xc1974
    mov cx, bx                                ; 89 d9                       ; 0xc1977
    mov ax, si                                ; 89 f0                       ; 0xc1979
    mov es, dx                                ; 8e c2                       ; 0xc197b
    jcxz 01981h                               ; e3 02                       ; 0xc197d
    rep stosb                                 ; f3 aa                       ; 0xc197f
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc1981 vgabios.c:1155
    jmp short 01932h                          ; eb ac                       ; 0xc1984
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc1986 vgabios.c:1156
    pop di                                    ; 5f                          ; 0xc1989
    pop si                                    ; 5e                          ; 0xc198a
    pop bp                                    ; 5d                          ; 0xc198b
    retn 00004h                               ; c2 04 00                    ; 0xc198c
  ; disGetNextSymbol 0xc198f LB 0x2905 -> off=0x0 cb=0000000000000083 uValue=00000000000c198f 'vgamem_copy_linear'
vgamem_copy_linear:                          ; 0xc198f LB 0x83
    push bp                                   ; 55                          ; 0xc198f vgabios.c:1159
    mov bp, sp                                ; 89 e5                       ; 0xc1990
    push si                                   ; 56                          ; 0xc1992
    push di                                   ; 57                          ; 0xc1993
    sub sp, strict byte 0000ch                ; 83 ec 0c                    ; 0xc1994
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc1997
    mov al, dl                                ; 88 d0                       ; 0xc199a
    mov byte [bp-00ah], bl                    ; 88 5e f6                    ; 0xc199c
    mov bx, cx                                ; 89 cb                       ; 0xc199f
    xor ah, ah                                ; 30 e4                       ; 0xc19a1 vgabios.c:1165
    mov si, ax                                ; 89 c6                       ; 0xc19a3
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc19a5
    mov word [bp-010h], ax                    ; 89 46 f0                    ; 0xc19a8
    mov ax, si                                ; 89 f0                       ; 0xc19ab
    imul word [bp-010h]                       ; f7 6e f0                    ; 0xc19ad
    mul word [bp+004h]                        ; f7 66 04                    ; 0xc19b0
    mov si, ax                                ; 89 c6                       ; 0xc19b3
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc19b5
    xor ah, ah                                ; 30 e4                       ; 0xc19b8
    mov di, ax                                ; 89 c7                       ; 0xc19ba
    add si, ax                                ; 01 c6                       ; 0xc19bc
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc19be
    sal si, CL                                ; d3 e6                       ; 0xc19c0
    mov word [bp-00ch], si                    ; 89 76 f4                    ; 0xc19c2
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc19c5 vgabios.c:1166
    imul word [bp-010h]                       ; f7 6e f0                    ; 0xc19c8
    mul word [bp+004h]                        ; f7 66 04                    ; 0xc19cb
    add ax, di                                ; 01 f8                       ; 0xc19ce
    sal ax, CL                                ; d3 e0                       ; 0xc19d0
    mov word [bp-00eh], ax                    ; 89 46 f2                    ; 0xc19d2
    sal bx, CL                                ; d3 e3                       ; 0xc19d5 vgabios.c:1167
    sal word [bp+004h], CL                    ; d3 66 04                    ; 0xc19d7 vgabios.c:1168
    mov byte [bp-006h], 000h                  ; c6 46 fa 00                 ; 0xc19da vgabios.c:1169
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc19de
    cmp al, byte [bp+006h]                    ; 3a 46 06                    ; 0xc19e1
    jnc short 01a09h                          ; 73 23                       ; 0xc19e4
    xor ah, ah                                ; 30 e4                       ; 0xc19e6 vgabios.c:1171
    mul word [bp+004h]                        ; f7 66 04                    ; 0xc19e8
    mov si, word [bp-00ch]                    ; 8b 76 f4                    ; 0xc19eb
    add si, ax                                ; 01 c6                       ; 0xc19ee
    mov di, word [bp-00eh]                    ; 8b 7e f2                    ; 0xc19f0
    add di, ax                                ; 01 c7                       ; 0xc19f3
    mov cx, bx                                ; 89 d9                       ; 0xc19f5
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc19f7
    mov es, dx                                ; 8e c2                       ; 0xc19fa
    jcxz 01a04h                               ; e3 06                       ; 0xc19fc
    push DS                                   ; 1e                          ; 0xc19fe
    mov ds, dx                                ; 8e da                       ; 0xc19ff
    rep movsb                                 ; f3 a4                       ; 0xc1a01
    pop DS                                    ; 1f                          ; 0xc1a03
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc1a04 vgabios.c:1172
    jmp short 019deh                          ; eb d5                       ; 0xc1a07
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc1a09 vgabios.c:1173
    pop di                                    ; 5f                          ; 0xc1a0c
    pop si                                    ; 5e                          ; 0xc1a0d
    pop bp                                    ; 5d                          ; 0xc1a0e
    retn 00004h                               ; c2 04 00                    ; 0xc1a0f
  ; disGetNextSymbol 0xc1a12 LB 0x2882 -> off=0x0 cb=000000000000006c uValue=00000000000c1a12 'vgamem_fill_linear'
vgamem_fill_linear:                          ; 0xc1a12 LB 0x6c
    push bp                                   ; 55                          ; 0xc1a12 vgabios.c:1176
    mov bp, sp                                ; 89 e5                       ; 0xc1a13
    push si                                   ; 56                          ; 0xc1a15
    push di                                   ; 57                          ; 0xc1a16
    sub sp, strict byte 00008h                ; 83 ec 08                    ; 0xc1a17
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc1a1a
    mov al, dl                                ; 88 d0                       ; 0xc1a1d
    mov si, cx                                ; 89 ce                       ; 0xc1a1f
    xor ah, ah                                ; 30 e4                       ; 0xc1a21 vgabios.c:1182
    mov dx, ax                                ; 89 c2                       ; 0xc1a23
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1a25
    mov di, ax                                ; 89 c7                       ; 0xc1a28
    mov ax, dx                                ; 89 d0                       ; 0xc1a2a
    imul di                                   ; f7 ef                       ; 0xc1a2c
    mul cx                                    ; f7 e1                       ; 0xc1a2e
    mov dx, ax                                ; 89 c2                       ; 0xc1a30
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1a32
    xor ah, ah                                ; 30 e4                       ; 0xc1a35
    add ax, dx                                ; 01 d0                       ; 0xc1a37
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc1a39
    sal ax, CL                                ; d3 e0                       ; 0xc1a3b
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc1a3d
    sal bx, CL                                ; d3 e3                       ; 0xc1a40 vgabios.c:1183
    sal si, CL                                ; d3 e6                       ; 0xc1a42 vgabios.c:1184
    mov byte [bp-008h], 000h                  ; c6 46 f8 00                 ; 0xc1a44 vgabios.c:1185
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1a48
    cmp al, byte [bp+004h]                    ; 3a 46 04                    ; 0xc1a4b
    jnc short 01a75h                          ; 73 25                       ; 0xc1a4e
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc1a50 vgabios.c:1187
    xor ah, ah                                ; 30 e4                       ; 0xc1a53
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc1a55
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1a58
    mul si                                    ; f7 e6                       ; 0xc1a5b
    mov di, word [bp-00ah]                    ; 8b 7e f6                    ; 0xc1a5d
    add di, ax                                ; 01 c7                       ; 0xc1a60
    mov cx, bx                                ; 89 d9                       ; 0xc1a62
    mov ax, word [bp-00ch]                    ; 8b 46 f4                    ; 0xc1a64
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc1a67
    mov es, dx                                ; 8e c2                       ; 0xc1a6a
    jcxz 01a70h                               ; e3 02                       ; 0xc1a6c
    rep stosb                                 ; f3 aa                       ; 0xc1a6e
    inc byte [bp-008h]                        ; fe 46 f8                    ; 0xc1a70 vgabios.c:1188
    jmp short 01a48h                          ; eb d3                       ; 0xc1a73
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc1a75 vgabios.c:1189
    pop di                                    ; 5f                          ; 0xc1a78
    pop si                                    ; 5e                          ; 0xc1a79
    pop bp                                    ; 5d                          ; 0xc1a7a
    retn 00004h                               ; c2 04 00                    ; 0xc1a7b
  ; disGetNextSymbol 0xc1a7e LB 0x2816 -> off=0x0 cb=00000000000006a3 uValue=00000000000c1a7e 'biosfn_scroll'
biosfn_scroll:                               ; 0xc1a7e LB 0x6a3
    push bp                                   ; 55                          ; 0xc1a7e vgabios.c:1192
    mov bp, sp                                ; 89 e5                       ; 0xc1a7f
    push si                                   ; 56                          ; 0xc1a81
    push di                                   ; 57                          ; 0xc1a82
    sub sp, strict byte 00020h                ; 83 ec 20                    ; 0xc1a83
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc1a86
    mov byte [bp-010h], dl                    ; 88 56 f0                    ; 0xc1a89
    mov byte [bp-00ch], bl                    ; 88 5e f4                    ; 0xc1a8c
    mov byte [bp-008h], cl                    ; 88 4e f8                    ; 0xc1a8f
    mov ch, byte [bp+006h]                    ; 8a 6e 06                    ; 0xc1a92
    cmp bl, byte [bp+004h]                    ; 3a 5e 04                    ; 0xc1a95 vgabios.c:1201
    jnbe short 01ab5h                         ; 77 1b                       ; 0xc1a98
    cmp ch, cl                                ; 38 cd                       ; 0xc1a9a vgabios.c:1202
    jc short 01ab5h                           ; 72 17                       ; 0xc1a9c
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc1a9e vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1aa1
    mov es, ax                                ; 8e c0                       ; 0xc1aa4
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc1aa6
    xor ah, ah                                ; 30 e4                       ; 0xc1aa9 vgabios.c:1206
    call 03630h                               ; e8 82 1b                    ; 0xc1aab
    mov byte [bp-012h], al                    ; 88 46 ee                    ; 0xc1aae
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc1ab1 vgabios.c:1207
    jne short 01ab8h                          ; 75 03                       ; 0xc1ab3
    jmp near 02118h                           ; e9 60 06                    ; 0xc1ab5
    mov bx, 00084h                            ; bb 84 00                    ; 0xc1ab8 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1abb
    mov es, ax                                ; 8e c0                       ; 0xc1abe
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc1ac0
    xor ah, ah                                ; 30 e4                       ; 0xc1ac3 vgabios.c:38
    inc ax                                    ; 40                          ; 0xc1ac5
    mov word [bp-024h], ax                    ; 89 46 dc                    ; 0xc1ac6
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc1ac9 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc1acc
    mov word [bp-018h], ax                    ; 89 46 e8                    ; 0xc1acf vgabios.c:48
    cmp byte [bp+008h], 0ffh                  ; 80 7e 08 ff                 ; 0xc1ad2 vgabios.c:1214
    jne short 01ae1h                          ; 75 09                       ; 0xc1ad6
    mov bx, strict word 00062h                ; bb 62 00                    ; 0xc1ad8 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc1adb
    mov byte [bp+008h], al                    ; 88 46 08                    ; 0xc1ade vgabios.c:38
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1ae1 vgabios.c:1217
    xor ah, ah                                ; 30 e4                       ; 0xc1ae4
    cmp ax, word [bp-024h]                    ; 3b 46 dc                    ; 0xc1ae6
    jc short 01af3h                           ; 72 08                       ; 0xc1ae9
    mov al, byte [bp-024h]                    ; 8a 46 dc                    ; 0xc1aeb
    db  0feh, 0c8h
    ; dec al                                    ; fe c8                     ; 0xc1aee
    mov byte [bp+004h], al                    ; 88 46 04                    ; 0xc1af0
    mov al, ch                                ; 88 e8                       ; 0xc1af3 vgabios.c:1218
    xor ah, ah                                ; 30 e4                       ; 0xc1af5
    cmp ax, word [bp-018h]                    ; 3b 46 e8                    ; 0xc1af7
    jc short 01b01h                           ; 72 05                       ; 0xc1afa
    mov ch, byte [bp-018h]                    ; 8a 6e e8                    ; 0xc1afc
    db  0feh, 0cdh
    ; dec ch                                    ; fe cd                     ; 0xc1aff
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1b01 vgabios.c:1219
    xor ah, ah                                ; 30 e4                       ; 0xc1b04
    cmp ax, word [bp-024h]                    ; 3b 46 dc                    ; 0xc1b06
    jbe short 01b0eh                          ; 76 03                       ; 0xc1b09
    mov byte [bp-006h], ah                    ; 88 66 fa                    ; 0xc1b0b
    mov al, ch                                ; 88 e8                       ; 0xc1b0e vgabios.c:1220
    sub al, byte [bp-008h]                    ; 2a 46 f8                    ; 0xc1b10
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc1b13
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc1b15
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc1b18 vgabios.c:1222
    mov byte [bp-01eh], al                    ; 88 46 e2                    ; 0xc1b1b
    mov byte [bp-01dh], 000h                  ; c6 46 e3 00                 ; 0xc1b1e
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc1b22
    mov bx, word [bp-01eh]                    ; 8b 5e e2                    ; 0xc1b24
    sal bx, CL                                ; d3 e3                       ; 0xc1b27
    mov ax, word [bp-018h]                    ; 8b 46 e8                    ; 0xc1b29
    dec ax                                    ; 48                          ; 0xc1b2c
    mov word [bp-020h], ax                    ; 89 46 e0                    ; 0xc1b2d
    mov ax, word [bp-024h]                    ; 8b 46 dc                    ; 0xc1b30
    dec ax                                    ; 48                          ; 0xc1b33
    mov word [bp-022h], ax                    ; 89 46 de                    ; 0xc1b34
    mov ax, word [bp-018h]                    ; 8b 46 e8                    ; 0xc1b37
    mul word [bp-024h]                        ; f7 66 dc                    ; 0xc1b3a
    mov di, ax                                ; 89 c7                       ; 0xc1b3d
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc1b3f
    jne short 01b90h                          ; 75 4a                       ; 0xc1b44
    sal ax, 1                                 ; d1 e0                       ; 0xc1b46 vgabios.c:1225
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc1b48
    mov dl, byte [bp+008h]                    ; 8a 56 08                    ; 0xc1b4a
    xor dh, dh                                ; 30 f6                       ; 0xc1b4d
    inc ax                                    ; 40                          ; 0xc1b4f
    mul dx                                    ; f7 e2                       ; 0xc1b50
    mov word [bp-01ch], ax                    ; 89 46 e4                    ; 0xc1b52
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1b55 vgabios.c:1230
    jne short 01b93h                          ; 75 38                       ; 0xc1b59
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00                 ; 0xc1b5b
    jne short 01b93h                          ; 75 32                       ; 0xc1b5f
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1b61
    jne short 01b93h                          ; 75 2c                       ; 0xc1b65
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1b67
    xor ah, ah                                ; 30 e4                       ; 0xc1b6a
    cmp ax, word [bp-022h]                    ; 3b 46 de                    ; 0xc1b6c
    jne short 01b93h                          ; 75 22                       ; 0xc1b6f
    mov al, ch                                ; 88 e8                       ; 0xc1b71
    cmp ax, word [bp-020h]                    ; 3b 46 e0                    ; 0xc1b73
    jne short 01b93h                          ; 75 1b                       ; 0xc1b76
    mov ah, byte [bp-010h]                    ; 8a 66 f0                    ; 0xc1b78 vgabios.c:1232
    xor al, ch                                ; 30 e8                       ; 0xc1b7b
    add ax, strict word 00020h                ; 05 20 00                    ; 0xc1b7d
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc1b80
    mov cx, di                                ; 89 f9                       ; 0xc1b84
    mov di, word [bp-01ch]                    ; 8b 7e e4                    ; 0xc1b86
    jcxz 01b8dh                               ; e3 02                       ; 0xc1b89
    rep stosw                                 ; f3 ab                       ; 0xc1b8b
    jmp near 02118h                           ; e9 88 05                    ; 0xc1b8d vgabios.c:1234
    jmp near 01d1dh                           ; e9 8a 01                    ; 0xc1b90
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc1b93 vgabios.c:1236
    jne short 01bfeh                          ; 75 65                       ; 0xc1b97
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1b99 vgabios.c:1237
    xor ah, ah                                ; 30 e4                       ; 0xc1b9c
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc1b9e
    mov dl, byte [bp+004h]                    ; 8a 56 04                    ; 0xc1ba1
    xor dh, dh                                ; 30 f6                       ; 0xc1ba4
    cmp dx, word [bp-016h]                    ; 3b 56 ea                    ; 0xc1ba6
    jc short 01c00h                           ; 72 55                       ; 0xc1ba9
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1bab vgabios.c:1239
    xor ah, ah                                ; 30 e4                       ; 0xc1bae
    add ax, word [bp-016h]                    ; 03 46 ea                    ; 0xc1bb0
    cmp ax, dx                                ; 39 d0                       ; 0xc1bb3
    jnbe short 01bbdh                         ; 77 06                       ; 0xc1bb5
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1bb7
    jne short 01c03h                          ; 75 46                       ; 0xc1bbb
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc1bbd vgabios.c:1240
    mov byte [bp-01ah], al                    ; 88 46 e6                    ; 0xc1bc0
    xor al, al                                ; 30 c0                       ; 0xc1bc3
    mov byte [bp-019h], al                    ; 88 46 e7                    ; 0xc1bc5
    mov ah, byte [bp-010h]                    ; 8a 66 f0                    ; 0xc1bc8
    mov si, ax                                ; 89 c6                       ; 0xc1bcb
    add si, strict byte 00020h                ; 83 c6 20                    ; 0xc1bcd
    mov ax, word [bp-016h]                    ; 8b 46 ea                    ; 0xc1bd0
    mul word [bp-018h]                        ; f7 66 e8                    ; 0xc1bd3
    mov dx, ax                                ; 89 c2                       ; 0xc1bd6
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1bd8
    xor ah, ah                                ; 30 e4                       ; 0xc1bdb
    add ax, dx                                ; 01 d0                       ; 0xc1bdd
    sal ax, 1                                 ; d1 e0                       ; 0xc1bdf
    mov di, word [bp-01ch]                    ; 8b 7e e4                    ; 0xc1be1
    add di, ax                                ; 01 c7                       ; 0xc1be4
    mov bl, byte [bp-012h]                    ; 8a 5e ee                    ; 0xc1be6
    xor bh, bh                                ; 30 ff                       ; 0xc1be9
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc1beb
    sal bx, CL                                ; d3 e3                       ; 0xc1bed
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc1bef
    mov cx, word [bp-01ah]                    ; 8b 4e e6                    ; 0xc1bf3
    mov ax, si                                ; 89 f0                       ; 0xc1bf6
    jcxz 01bfch                               ; e3 02                       ; 0xc1bf8
    rep stosw                                 ; f3 ab                       ; 0xc1bfa
    jmp short 01c4ch                          ; eb 4e                       ; 0xc1bfc vgabios.c:1241
    jmp short 01c52h                          ; eb 52                       ; 0xc1bfe
    jmp near 02118h                           ; e9 15 05                    ; 0xc1c00
    mov dl, byte [bp-00ah]                    ; 8a 56 f6                    ; 0xc1c03 vgabios.c:1242
    mov byte [bp-014h], dl                    ; 88 56 ec                    ; 0xc1c06
    mov byte [bp-013h], dh                    ; 88 76 ed                    ; 0xc1c09
    mul word [bp-018h]                        ; f7 66 e8                    ; 0xc1c0c
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc1c0f
    mov byte [bp-01ah], dl                    ; 88 56 e6                    ; 0xc1c12
    mov byte [bp-019h], 000h                  ; c6 46 e7 00                 ; 0xc1c15
    mov si, ax                                ; 89 c6                       ; 0xc1c19
    add si, word [bp-01ah]                    ; 03 76 e6                    ; 0xc1c1b
    sal si, 1                                 ; d1 e6                       ; 0xc1c1e
    mov bl, byte [bp-012h]                    ; 8a 5e ee                    ; 0xc1c20
    xor bh, bh                                ; 30 ff                       ; 0xc1c23
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc1c25
    sal bx, CL                                ; d3 e3                       ; 0xc1c27
    mov bx, word [bx+047b2h]                  ; 8b 9f b2 47                 ; 0xc1c29
    mov ax, word [bp-016h]                    ; 8b 46 ea                    ; 0xc1c2d
    mul word [bp-018h]                        ; f7 66 e8                    ; 0xc1c30
    add ax, word [bp-01ah]                    ; 03 46 e6                    ; 0xc1c33
    sal ax, 1                                 ; d1 e0                       ; 0xc1c36
    mov di, word [bp-01ch]                    ; 8b 7e e4                    ; 0xc1c38
    add di, ax                                ; 01 c7                       ; 0xc1c3b
    mov cx, word [bp-014h]                    ; 8b 4e ec                    ; 0xc1c3d
    mov dx, bx                                ; 89 da                       ; 0xc1c40
    mov es, bx                                ; 8e c3                       ; 0xc1c42
    jcxz 01c4ch                               ; e3 06                       ; 0xc1c44
    push DS                                   ; 1e                          ; 0xc1c46
    mov ds, dx                                ; 8e da                       ; 0xc1c47
    rep movsw                                 ; f3 a5                       ; 0xc1c49
    pop DS                                    ; 1f                          ; 0xc1c4b
    inc word [bp-016h]                        ; ff 46 ea                    ; 0xc1c4c vgabios.c:1243
    jmp near 01ba1h                           ; e9 4f ff                    ; 0xc1c4f
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1c52 vgabios.c:1246
    xor ah, ah                                ; 30 e4                       ; 0xc1c55
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc1c57
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1c5a
    xor ah, ah                                ; 30 e4                       ; 0xc1c5d
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1c5f
    jnbe short 01c00h                         ; 77 9c                       ; 0xc1c62
    mov dl, al                                ; 88 c2                       ; 0xc1c64 vgabios.c:1248
    xor dh, dh                                ; 30 f6                       ; 0xc1c66
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1c68
    add ax, dx                                ; 01 d0                       ; 0xc1c6b
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1c6d
    jnbe short 01c78h                         ; 77 06                       ; 0xc1c70
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1c72
    jne short 01cb8h                          ; 75 40                       ; 0xc1c76
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc1c78 vgabios.c:1249
    xor bh, bh                                ; 30 ff                       ; 0xc1c7b
    mov ah, byte [bp-010h]                    ; 8a 66 f0                    ; 0xc1c7d
    xor al, al                                ; 30 c0                       ; 0xc1c80
    mov si, ax                                ; 89 c6                       ; 0xc1c82
    add si, strict byte 00020h                ; 83 c6 20                    ; 0xc1c84
    mov ax, word [bp-016h]                    ; 8b 46 ea                    ; 0xc1c87
    mul word [bp-018h]                        ; f7 66 e8                    ; 0xc1c8a
    mov dx, ax                                ; 89 c2                       ; 0xc1c8d
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1c8f
    xor ah, ah                                ; 30 e4                       ; 0xc1c92
    add ax, dx                                ; 01 d0                       ; 0xc1c94
    sal ax, 1                                 ; d1 e0                       ; 0xc1c96
    mov dx, word [bp-01ch]                    ; 8b 56 e4                    ; 0xc1c98
    add dx, ax                                ; 01 c2                       ; 0xc1c9b
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc1c9d
    xor ah, ah                                ; 30 e4                       ; 0xc1ca0
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc1ca2
    mov di, ax                                ; 89 c7                       ; 0xc1ca4
    sal di, CL                                ; d3 e7                       ; 0xc1ca6
    mov es, [di+047b2h]                       ; 8e 85 b2 47                 ; 0xc1ca8
    mov cx, bx                                ; 89 d9                       ; 0xc1cac
    mov ax, si                                ; 89 f0                       ; 0xc1cae
    mov di, dx                                ; 89 d7                       ; 0xc1cb0
    jcxz 01cb6h                               ; e3 02                       ; 0xc1cb2
    rep stosw                                 ; f3 ab                       ; 0xc1cb4
    jmp short 01d0dh                          ; eb 55                       ; 0xc1cb6 vgabios.c:1250
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc1cb8 vgabios.c:1251
    mov byte [bp-01ah], al                    ; 88 46 e6                    ; 0xc1cbb
    mov byte [bp-019h], dh                    ; 88 76 e7                    ; 0xc1cbe
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1cc1
    xor ah, ah                                ; 30 e4                       ; 0xc1cc4
    mov dx, word [bp-016h]                    ; 8b 56 ea                    ; 0xc1cc6
    sub dx, ax                                ; 29 c2                       ; 0xc1cc9
    mov ax, dx                                ; 89 d0                       ; 0xc1ccb
    mul word [bp-018h]                        ; f7 66 e8                    ; 0xc1ccd
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc1cd0
    mov byte [bp-014h], dl                    ; 88 56 ec                    ; 0xc1cd3
    mov byte [bp-013h], 000h                  ; c6 46 ed 00                 ; 0xc1cd6
    mov si, ax                                ; 89 c6                       ; 0xc1cda
    add si, word [bp-014h]                    ; 03 76 ec                    ; 0xc1cdc
    sal si, 1                                 ; d1 e6                       ; 0xc1cdf
    mov bl, byte [bp-012h]                    ; 8a 5e ee                    ; 0xc1ce1
    xor bh, bh                                ; 30 ff                       ; 0xc1ce4
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc1ce6
    sal bx, CL                                ; d3 e3                       ; 0xc1ce8
    mov bx, word [bx+047b2h]                  ; 8b 9f b2 47                 ; 0xc1cea
    mov ax, word [bp-016h]                    ; 8b 46 ea                    ; 0xc1cee
    mul word [bp-018h]                        ; f7 66 e8                    ; 0xc1cf1
    add ax, word [bp-014h]                    ; 03 46 ec                    ; 0xc1cf4
    sal ax, 1                                 ; d1 e0                       ; 0xc1cf7
    mov di, word [bp-01ch]                    ; 8b 7e e4                    ; 0xc1cf9
    add di, ax                                ; 01 c7                       ; 0xc1cfc
    mov cx, word [bp-01ah]                    ; 8b 4e e6                    ; 0xc1cfe
    mov dx, bx                                ; 89 da                       ; 0xc1d01
    mov es, bx                                ; 8e c3                       ; 0xc1d03
    jcxz 01d0dh                               ; e3 06                       ; 0xc1d05
    push DS                                   ; 1e                          ; 0xc1d07
    mov ds, dx                                ; 8e da                       ; 0xc1d08
    rep movsw                                 ; f3 a5                       ; 0xc1d0a
    pop DS                                    ; 1f                          ; 0xc1d0c
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1d0d vgabios.c:1252
    xor ah, ah                                ; 30 e4                       ; 0xc1d10
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1d12
    jc short 01d4bh                           ; 72 34                       ; 0xc1d15
    dec word [bp-016h]                        ; ff 4e ea                    ; 0xc1d17 vgabios.c:1253
    jmp near 01c5ah                           ; e9 3d ff                    ; 0xc1d1a
    mov si, word [bp-01eh]                    ; 8b 76 e2                    ; 0xc1d1d vgabios.c:1259
    mov al, byte [si+0482eh]                  ; 8a 84 2e 48                 ; 0xc1d20
    xor ah, ah                                ; 30 e4                       ; 0xc1d24
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc1d26
    mov si, ax                                ; 89 c6                       ; 0xc1d28
    sal si, CL                                ; d3 e6                       ; 0xc1d2a
    mov al, byte [si+04844h]                  ; 8a 84 44 48                 ; 0xc1d2c
    mov byte [bp-00eh], al                    ; 88 46 f2                    ; 0xc1d30
    mov al, byte [bx+047b0h]                  ; 8a 87 b0 47                 ; 0xc1d33 vgabios.c:1260
    cmp AL, strict byte 003h                  ; 3c 03                       ; 0xc1d37
    jc short 01d47h                           ; 72 0c                       ; 0xc1d39
    jbe short 01d4eh                          ; 76 11                       ; 0xc1d3b
    cmp AL, strict byte 005h                  ; 3c 05                       ; 0xc1d3d
    je short 01d7bh                           ; 74 3a                       ; 0xc1d3f
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc1d41
    je short 01d4eh                           ; 74 09                       ; 0xc1d43
    jmp short 01d4bh                          ; eb 04                       ; 0xc1d45
    cmp AL, strict byte 002h                  ; 3c 02                       ; 0xc1d47
    je short 01d7eh                           ; 74 33                       ; 0xc1d49
    jmp near 02118h                           ; e9 ca 03                    ; 0xc1d4b
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1d4e vgabios.c:1264
    jne short 01d79h                          ; 75 25                       ; 0xc1d52
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00                 ; 0xc1d54
    jne short 01dbch                          ; 75 62                       ; 0xc1d58
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1d5a
    jne short 01dbch                          ; 75 5c                       ; 0xc1d5e
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1d60
    xor ah, ah                                ; 30 e4                       ; 0xc1d63
    mov dx, word [bp-024h]                    ; 8b 56 dc                    ; 0xc1d65
    dec dx                                    ; 4a                          ; 0xc1d68
    cmp ax, dx                                ; 39 d0                       ; 0xc1d69
    jne short 01dbch                          ; 75 4f                       ; 0xc1d6b
    mov al, ch                                ; 88 e8                       ; 0xc1d6d
    xor ah, dh                                ; 30 f4                       ; 0xc1d6f
    mov dx, word [bp-018h]                    ; 8b 56 e8                    ; 0xc1d71
    dec dx                                    ; 4a                          ; 0xc1d74
    cmp ax, dx                                ; 39 d0                       ; 0xc1d75
    je short 01d81h                           ; 74 08                       ; 0xc1d77
    jmp short 01dbch                          ; eb 41                       ; 0xc1d79
    jmp near 01ffch                           ; e9 7e 02                    ; 0xc1d7b
    jmp near 01ea8h                           ; e9 27 01                    ; 0xc1d7e
    mov ax, 00205h                            ; b8 05 02                    ; 0xc1d81 vgabios.c:1266
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1d84
    out DX, ax                                ; ef                          ; 0xc1d87
    mov ax, word [bp-024h]                    ; 8b 46 dc                    ; 0xc1d88 vgabios.c:1267
    mul word [bp-018h]                        ; f7 66 e8                    ; 0xc1d8b
    mov dl, byte [bp-00eh]                    ; 8a 56 f2                    ; 0xc1d8e
    xor dh, dh                                ; 30 f6                       ; 0xc1d91
    mul dx                                    ; f7 e2                       ; 0xc1d93
    mov dx, ax                                ; 89 c2                       ; 0xc1d95
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1d97
    xor ah, ah                                ; 30 e4                       ; 0xc1d9a
    mov bl, byte [bp-012h]                    ; 8a 5e ee                    ; 0xc1d9c
    xor bh, bh                                ; 30 ff                       ; 0xc1d9f
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc1da1
    sal bx, CL                                ; d3 e3                       ; 0xc1da3
    mov bx, word [bx+047b2h]                  ; 8b 9f b2 47                 ; 0xc1da5
    mov cx, dx                                ; 89 d1                       ; 0xc1da9
    xor di, di                                ; 31 ff                       ; 0xc1dab
    mov es, bx                                ; 8e c3                       ; 0xc1dad
    jcxz 01db3h                               ; e3 02                       ; 0xc1daf
    rep stosb                                 ; f3 aa                       ; 0xc1db1
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc1db3 vgabios.c:1268
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1db6
    out DX, ax                                ; ef                          ; 0xc1db9
    jmp short 01d4bh                          ; eb 8f                       ; 0xc1dba vgabios.c:1270
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc1dbc vgabios.c:1272
    jne short 01e2eh                          ; 75 6c                       ; 0xc1dc0
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1dc2 vgabios.c:1273
    xor ah, ah                                ; 30 e4                       ; 0xc1dc5
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc1dc7
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1dca
    xor ah, ah                                ; 30 e4                       ; 0xc1dcd
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1dcf
    jc short 01e2bh                           ; 72 57                       ; 0xc1dd2
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc1dd4 vgabios.c:1275
    xor dh, dh                                ; 30 f6                       ; 0xc1dd7
    add dx, word [bp-016h]                    ; 03 56 ea                    ; 0xc1dd9
    cmp dx, ax                                ; 39 c2                       ; 0xc1ddc
    jnbe short 01de6h                         ; 77 06                       ; 0xc1dde
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1de0
    jne short 01e07h                          ; 75 21                       ; 0xc1de4
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1de6 vgabios.c:1276
    xor ah, ah                                ; 30 e4                       ; 0xc1de9
    push ax                                   ; 50                          ; 0xc1deb
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1dec
    push ax                                   ; 50                          ; 0xc1def
    mov cl, byte [bp-018h]                    ; 8a 4e e8                    ; 0xc1df0
    xor ch, ch                                ; 30 ed                       ; 0xc1df3
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc1df5
    xor bh, bh                                ; 30 ff                       ; 0xc1df8
    mov dl, byte [bp-016h]                    ; 8a 56 ea                    ; 0xc1dfa
    xor dh, dh                                ; 30 f6                       ; 0xc1dfd
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1dff
    call 017cah                               ; e8 c5 f9                    ; 0xc1e02
    jmp short 01e26h                          ; eb 1f                       ; 0xc1e05 vgabios.c:1277
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1e07 vgabios.c:1278
    push ax                                   ; 50                          ; 0xc1e0a
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc1e0b
    push ax                                   ; 50                          ; 0xc1e0e
    mov cl, byte [bp-00ah]                    ; 8a 4e f6                    ; 0xc1e0f
    xor ch, ch                                ; 30 ed                       ; 0xc1e12
    mov bl, byte [bp-016h]                    ; 8a 5e ea                    ; 0xc1e14
    xor bh, bh                                ; 30 ff                       ; 0xc1e17
    mov dl, bl                                ; 88 da                       ; 0xc1e19
    add dl, byte [bp-006h]                    ; 02 56 fa                    ; 0xc1e1b
    xor dh, dh                                ; 30 f6                       ; 0xc1e1e
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1e20
    call 0173ch                               ; e8 16 f9                    ; 0xc1e23
    inc word [bp-016h]                        ; ff 46 ea                    ; 0xc1e26 vgabios.c:1279
    jmp short 01dcah                          ; eb 9f                       ; 0xc1e29
    jmp near 02118h                           ; e9 ea 02                    ; 0xc1e2b
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1e2e vgabios.c:1282
    xor ah, ah                                ; 30 e4                       ; 0xc1e31
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc1e33
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1e36
    xor ah, ah                                ; 30 e4                       ; 0xc1e39
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1e3b
    jnbe short 01e2bh                         ; 77 eb                       ; 0xc1e3e
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc1e40 vgabios.c:1284
    xor dh, dh                                ; 30 f6                       ; 0xc1e43
    add ax, dx                                ; 01 d0                       ; 0xc1e45
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1e47
    jnbe short 01e50h                         ; 77 04                       ; 0xc1e4a
    test dl, dl                               ; 84 d2                       ; 0xc1e4c
    jne short 01e71h                          ; 75 21                       ; 0xc1e4e
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1e50 vgabios.c:1285
    xor ah, ah                                ; 30 e4                       ; 0xc1e53
    push ax                                   ; 50                          ; 0xc1e55
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1e56
    push ax                                   ; 50                          ; 0xc1e59
    mov cl, byte [bp-018h]                    ; 8a 4e e8                    ; 0xc1e5a
    xor ch, ch                                ; 30 ed                       ; 0xc1e5d
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc1e5f
    xor bh, bh                                ; 30 ff                       ; 0xc1e62
    mov dl, byte [bp-016h]                    ; 8a 56 ea                    ; 0xc1e64
    xor dh, dh                                ; 30 f6                       ; 0xc1e67
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1e69
    call 017cah                               ; e8 5b f9                    ; 0xc1e6c
    jmp short 01e99h                          ; eb 28                       ; 0xc1e6f vgabios.c:1286
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1e71 vgabios.c:1287
    xor ah, ah                                ; 30 e4                       ; 0xc1e74
    push ax                                   ; 50                          ; 0xc1e76
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc1e77
    push ax                                   ; 50                          ; 0xc1e7a
    mov cl, byte [bp-00ah]                    ; 8a 4e f6                    ; 0xc1e7b
    xor ch, ch                                ; 30 ed                       ; 0xc1e7e
    mov bl, byte [bp-016h]                    ; 8a 5e ea                    ; 0xc1e80
    xor bh, bh                                ; 30 ff                       ; 0xc1e83
    mov dl, bl                                ; 88 da                       ; 0xc1e85
    sub dl, byte [bp-006h]                    ; 2a 56 fa                    ; 0xc1e87
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1e8a
    mov byte [bp-01ah], al                    ; 88 46 e6                    ; 0xc1e8d
    mov byte [bp-019h], dh                    ; 88 76 e7                    ; 0xc1e90
    mov ax, word [bp-01ah]                    ; 8b 46 e6                    ; 0xc1e93
    call 0173ch                               ; e8 a3 f8                    ; 0xc1e96
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1e99 vgabios.c:1288
    xor ah, ah                                ; 30 e4                       ; 0xc1e9c
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1e9e
    jc short 01ef1h                           ; 72 4e                       ; 0xc1ea1
    dec word [bp-016h]                        ; ff 4e ea                    ; 0xc1ea3 vgabios.c:1289
    jmp short 01e36h                          ; eb 8e                       ; 0xc1ea6
    mov cl, byte [bx+047b1h]                  ; 8a 8f b1 47                 ; 0xc1ea8 vgabios.c:1294
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1eac vgabios.c:1295
    jne short 01ef4h                          ; 75 42                       ; 0xc1eb0
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00                 ; 0xc1eb2
    jne short 01ef4h                          ; 75 3c                       ; 0xc1eb6
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1eb8
    jne short 01ef4h                          ; 75 36                       ; 0xc1ebc
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1ebe
    cmp ax, word [bp-022h]                    ; 3b 46 de                    ; 0xc1ec1
    jne short 01ef4h                          ; 75 2e                       ; 0xc1ec4
    mov al, ch                                ; 88 e8                       ; 0xc1ec6
    cmp ax, word [bp-020h]                    ; 3b 46 e0                    ; 0xc1ec8
    jne short 01ef4h                          ; 75 27                       ; 0xc1ecb
    mov dl, byte [bp-00eh]                    ; 8a 56 f2                    ; 0xc1ecd vgabios.c:1297
    xor dh, dh                                ; 30 f6                       ; 0xc1ed0
    mov ax, di                                ; 89 f8                       ; 0xc1ed2
    mul dx                                    ; f7 e2                       ; 0xc1ed4
    mov dl, cl                                ; 88 ca                       ; 0xc1ed6
    xor dh, dh                                ; 30 f6                       ; 0xc1ed8
    mul dx                                    ; f7 e2                       ; 0xc1eda
    mov dl, byte [bp-010h]                    ; 8a 56 f0                    ; 0xc1edc
    xor dh, dh                                ; 30 f6                       ; 0xc1edf
    mov bx, word [bx+047b2h]                  ; 8b 9f b2 47                 ; 0xc1ee1
    mov cx, ax                                ; 89 c1                       ; 0xc1ee5
    mov ax, dx                                ; 89 d0                       ; 0xc1ee7
    xor di, di                                ; 31 ff                       ; 0xc1ee9
    mov es, bx                                ; 8e c3                       ; 0xc1eeb
    jcxz 01ef1h                               ; e3 02                       ; 0xc1eed
    rep stosb                                 ; f3 aa                       ; 0xc1eef
    jmp near 02118h                           ; e9 24 02                    ; 0xc1ef1 vgabios.c:1299
    cmp cl, 002h                              ; 80 f9 02                    ; 0xc1ef4 vgabios.c:1301
    jne short 01f02h                          ; 75 09                       ; 0xc1ef7
    sal byte [bp-008h], 1                     ; d0 66 f8                    ; 0xc1ef9 vgabios.c:1303
    sal byte [bp-00ah], 1                     ; d0 66 f6                    ; 0xc1efc vgabios.c:1304
    sal word [bp-018h], 1                     ; d1 66 e8                    ; 0xc1eff vgabios.c:1305
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc1f02 vgabios.c:1308
    jne short 01f71h                          ; 75 69                       ; 0xc1f06
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1f08 vgabios.c:1309
    xor ah, ah                                ; 30 e4                       ; 0xc1f0b
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc1f0d
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1f10
    xor ah, ah                                ; 30 e4                       ; 0xc1f13
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1f15
    jc short 01ef1h                           ; 72 d7                       ; 0xc1f18
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc1f1a vgabios.c:1311
    xor dh, dh                                ; 30 f6                       ; 0xc1f1d
    add dx, word [bp-016h]                    ; 03 56 ea                    ; 0xc1f1f
    cmp dx, ax                                ; 39 c2                       ; 0xc1f22
    jnbe short 01f2ch                         ; 77 06                       ; 0xc1f24
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1f26
    jne short 01f4dh                          ; 75 21                       ; 0xc1f2a
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1f2c vgabios.c:1312
    xor ah, ah                                ; 30 e4                       ; 0xc1f2f
    push ax                                   ; 50                          ; 0xc1f31
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1f32
    push ax                                   ; 50                          ; 0xc1f35
    mov cl, byte [bp-018h]                    ; 8a 4e e8                    ; 0xc1f36
    xor ch, ch                                ; 30 ed                       ; 0xc1f39
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc1f3b
    xor bh, bh                                ; 30 ff                       ; 0xc1f3e
    mov dl, byte [bp-016h]                    ; 8a 56 ea                    ; 0xc1f40
    xor dh, dh                                ; 30 f6                       ; 0xc1f43
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1f45
    call 018fbh                               ; e8 b0 f9                    ; 0xc1f48
    jmp short 01f6ch                          ; eb 1f                       ; 0xc1f4b vgabios.c:1313
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1f4d vgabios.c:1314
    push ax                                   ; 50                          ; 0xc1f50
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc1f51
    push ax                                   ; 50                          ; 0xc1f54
    mov cl, byte [bp-00ah]                    ; 8a 4e f6                    ; 0xc1f55
    xor ch, ch                                ; 30 ed                       ; 0xc1f58
    mov bl, byte [bp-016h]                    ; 8a 5e ea                    ; 0xc1f5a
    xor bh, bh                                ; 30 ff                       ; 0xc1f5d
    mov dl, bl                                ; 88 da                       ; 0xc1f5f
    add dl, byte [bp-006h]                    ; 02 56 fa                    ; 0xc1f61
    xor dh, dh                                ; 30 f6                       ; 0xc1f64
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1f66
    call 01845h                               ; e8 d9 f8                    ; 0xc1f69
    inc word [bp-016h]                        ; ff 46 ea                    ; 0xc1f6c vgabios.c:1315
    jmp short 01f10h                          ; eb 9f                       ; 0xc1f6f
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1f71 vgabios.c:1318
    xor ah, ah                                ; 30 e4                       ; 0xc1f74
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc1f76
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1f79
    xor ah, ah                                ; 30 e4                       ; 0xc1f7c
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1f7e
    jnbe short 01fc1h                         ; 77 3e                       ; 0xc1f81
    mov dl, al                                ; 88 c2                       ; 0xc1f83 vgabios.c:1320
    xor dh, dh                                ; 30 f6                       ; 0xc1f85
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1f87
    add ax, dx                                ; 01 d0                       ; 0xc1f8a
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1f8c
    jnbe short 01f97h                         ; 77 06                       ; 0xc1f8f
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1f91
    jne short 01fc4h                          ; 75 2d                       ; 0xc1f95
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1f97 vgabios.c:1321
    xor ah, ah                                ; 30 e4                       ; 0xc1f9a
    push ax                                   ; 50                          ; 0xc1f9c
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1f9d
    push ax                                   ; 50                          ; 0xc1fa0
    mov cl, byte [bp-018h]                    ; 8a 4e e8                    ; 0xc1fa1
    xor ch, ch                                ; 30 ed                       ; 0xc1fa4
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc1fa6
    xor bh, bh                                ; 30 ff                       ; 0xc1fa9
    mov dl, byte [bp-016h]                    ; 8a 56 ea                    ; 0xc1fab
    xor dh, dh                                ; 30 f6                       ; 0xc1fae
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1fb0
    mov byte [bp-014h], al                    ; 88 46 ec                    ; 0xc1fb3
    mov byte [bp-013h], ah                    ; 88 66 ed                    ; 0xc1fb6
    mov ax, word [bp-014h]                    ; 8b 46 ec                    ; 0xc1fb9
    call 018fbh                               ; e8 3c f9                    ; 0xc1fbc
    jmp short 01fech                          ; eb 2b                       ; 0xc1fbf vgabios.c:1322
    jmp near 02118h                           ; e9 54 01                    ; 0xc1fc1
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1fc4 vgabios.c:1323
    xor ah, ah                                ; 30 e4                       ; 0xc1fc7
    push ax                                   ; 50                          ; 0xc1fc9
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc1fca
    push ax                                   ; 50                          ; 0xc1fcd
    mov cl, byte [bp-00ah]                    ; 8a 4e f6                    ; 0xc1fce
    xor ch, ch                                ; 30 ed                       ; 0xc1fd1
    mov bl, byte [bp-016h]                    ; 8a 5e ea                    ; 0xc1fd3
    xor bh, bh                                ; 30 ff                       ; 0xc1fd6
    mov dl, bl                                ; 88 da                       ; 0xc1fd8
    sub dl, byte [bp-006h]                    ; 2a 56 fa                    ; 0xc1fda
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1fdd
    mov byte [bp-014h], al                    ; 88 46 ec                    ; 0xc1fe0
    mov byte [bp-013h], dh                    ; 88 76 ed                    ; 0xc1fe3
    mov ax, word [bp-014h]                    ; 8b 46 ec                    ; 0xc1fe6
    call 01845h                               ; e8 59 f8                    ; 0xc1fe9
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1fec vgabios.c:1324
    xor ah, ah                                ; 30 e4                       ; 0xc1fef
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1ff1
    jc short 0203bh                           ; 72 45                       ; 0xc1ff4
    dec word [bp-016h]                        ; ff 4e ea                    ; 0xc1ff6 vgabios.c:1325
    jmp near 01f79h                           ; e9 7d ff                    ; 0xc1ff9
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1ffc vgabios.c:1330
    jne short 0203eh                          ; 75 3c                       ; 0xc2000
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00                 ; 0xc2002
    jne short 0203eh                          ; 75 36                       ; 0xc2006
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc2008
    jne short 0203eh                          ; 75 30                       ; 0xc200c
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc200e
    cmp ax, word [bp-022h]                    ; 3b 46 de                    ; 0xc2011
    jne short 0203eh                          ; 75 28                       ; 0xc2014
    mov al, ch                                ; 88 e8                       ; 0xc2016
    cmp ax, word [bp-020h]                    ; 3b 46 e0                    ; 0xc2018
    jne short 0203eh                          ; 75 21                       ; 0xc201b
    mov dl, byte [bp-00eh]                    ; 8a 56 f2                    ; 0xc201d vgabios.c:1332
    xor dh, dh                                ; 30 f6                       ; 0xc2020
    mov ax, di                                ; 89 f8                       ; 0xc2022
    mul dx                                    ; f7 e2                       ; 0xc2024
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc2026
    sal ax, CL                                ; d3 e0                       ; 0xc2028
    mov cx, ax                                ; 89 c1                       ; 0xc202a
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc202c
    xor ah, ah                                ; 30 e4                       ; 0xc202f
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc2031
    xor di, di                                ; 31 ff                       ; 0xc2035
    jcxz 0203bh                               ; e3 02                       ; 0xc2037
    rep stosb                                 ; f3 aa                       ; 0xc2039
    jmp near 02118h                           ; e9 da 00                    ; 0xc203b vgabios.c:1334
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc203e vgabios.c:1337
    jne short 020aah                          ; 75 66                       ; 0xc2042
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc2044 vgabios.c:1338
    xor ah, ah                                ; 30 e4                       ; 0xc2047
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc2049
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc204c
    xor ah, ah                                ; 30 e4                       ; 0xc204f
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc2051
    jc short 0203bh                           ; 72 e5                       ; 0xc2054
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc2056 vgabios.c:1340
    xor dh, dh                                ; 30 f6                       ; 0xc2059
    add dx, word [bp-016h]                    ; 03 56 ea                    ; 0xc205b
    cmp dx, ax                                ; 39 c2                       ; 0xc205e
    jnbe short 02068h                         ; 77 06                       ; 0xc2060
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc2062
    jne short 02087h                          ; 75 1f                       ; 0xc2066
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc2068 vgabios.c:1341
    xor ah, ah                                ; 30 e4                       ; 0xc206b
    push ax                                   ; 50                          ; 0xc206d
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc206e
    push ax                                   ; 50                          ; 0xc2071
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc2072
    xor bh, bh                                ; 30 ff                       ; 0xc2075
    mov dl, byte [bp-016h]                    ; 8a 56 ea                    ; 0xc2077
    xor dh, dh                                ; 30 f6                       ; 0xc207a
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc207c
    mov cx, word [bp-018h]                    ; 8b 4e e8                    ; 0xc207f
    call 01a12h                               ; e8 8d f9                    ; 0xc2082
    jmp short 020a5h                          ; eb 1e                       ; 0xc2085 vgabios.c:1342
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc2087 vgabios.c:1343
    push ax                                   ; 50                          ; 0xc208a
    push word [bp-018h]                       ; ff 76 e8                    ; 0xc208b
    mov cl, byte [bp-00ah]                    ; 8a 4e f6                    ; 0xc208e
    xor ch, ch                                ; 30 ed                       ; 0xc2091
    mov bl, byte [bp-016h]                    ; 8a 5e ea                    ; 0xc2093
    xor bh, bh                                ; 30 ff                       ; 0xc2096
    mov dl, bl                                ; 88 da                       ; 0xc2098
    add dl, byte [bp-006h]                    ; 02 56 fa                    ; 0xc209a
    xor dh, dh                                ; 30 f6                       ; 0xc209d
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc209f
    call 0198fh                               ; e8 ea f8                    ; 0xc20a2
    inc word [bp-016h]                        ; ff 46 ea                    ; 0xc20a5 vgabios.c:1344
    jmp short 0204ch                          ; eb a2                       ; 0xc20a8
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc20aa vgabios.c:1347
    xor ah, ah                                ; 30 e4                       ; 0xc20ad
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc20af
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc20b2
    xor ah, ah                                ; 30 e4                       ; 0xc20b5
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc20b7
    jnbe short 02118h                         ; 77 5c                       ; 0xc20ba
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc20bc vgabios.c:1349
    xor dh, dh                                ; 30 f6                       ; 0xc20bf
    add ax, dx                                ; 01 d0                       ; 0xc20c1
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc20c3
    jnbe short 020cch                         ; 77 04                       ; 0xc20c6
    test dl, dl                               ; 84 d2                       ; 0xc20c8
    jne short 020ebh                          ; 75 1f                       ; 0xc20ca
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc20cc vgabios.c:1350
    xor ah, ah                                ; 30 e4                       ; 0xc20cf
    push ax                                   ; 50                          ; 0xc20d1
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc20d2
    push ax                                   ; 50                          ; 0xc20d5
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc20d6
    xor bh, bh                                ; 30 ff                       ; 0xc20d9
    mov dl, byte [bp-016h]                    ; 8a 56 ea                    ; 0xc20db
    xor dh, dh                                ; 30 f6                       ; 0xc20de
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc20e0
    mov cx, word [bp-018h]                    ; 8b 4e e8                    ; 0xc20e3
    call 01a12h                               ; e8 29 f9                    ; 0xc20e6
    jmp short 02109h                          ; eb 1e                       ; 0xc20e9 vgabios.c:1351
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc20eb vgabios.c:1352
    xor ah, ah                                ; 30 e4                       ; 0xc20ee
    push ax                                   ; 50                          ; 0xc20f0
    push word [bp-018h]                       ; ff 76 e8                    ; 0xc20f1
    mov cl, byte [bp-00ah]                    ; 8a 4e f6                    ; 0xc20f4
    xor ch, ch                                ; 30 ed                       ; 0xc20f7
    mov bl, byte [bp-016h]                    ; 8a 5e ea                    ; 0xc20f9
    xor bh, bh                                ; 30 ff                       ; 0xc20fc
    mov dl, bl                                ; 88 da                       ; 0xc20fe
    sub dl, byte [bp-006h]                    ; 2a 56 fa                    ; 0xc2100
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2103
    call 0198fh                               ; e8 86 f8                    ; 0xc2106
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc2109 vgabios.c:1353
    xor ah, ah                                ; 30 e4                       ; 0xc210c
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc210e
    jc short 02118h                           ; 72 05                       ; 0xc2111
    dec word [bp-016h]                        ; ff 4e ea                    ; 0xc2113 vgabios.c:1354
    jmp short 020b2h                          ; eb 9a                       ; 0xc2116
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc2118 vgabios.c:1365
    pop di                                    ; 5f                          ; 0xc211b
    pop si                                    ; 5e                          ; 0xc211c
    pop bp                                    ; 5d                          ; 0xc211d
    retn 00008h                               ; c2 08 00                    ; 0xc211e
  ; disGetNextSymbol 0xc2121 LB 0x2173 -> off=0x0 cb=0000000000000112 uValue=00000000000c2121 'write_gfx_char_pl4'
write_gfx_char_pl4:                          ; 0xc2121 LB 0x112
    push bp                                   ; 55                          ; 0xc2121 vgabios.c:1368
    mov bp, sp                                ; 89 e5                       ; 0xc2122
    push si                                   ; 56                          ; 0xc2124
    push di                                   ; 57                          ; 0xc2125
    sub sp, strict byte 00010h                ; 83 ec 10                    ; 0xc2126
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc2129
    mov byte [bp-00ah], dl                    ; 88 56 f6                    ; 0xc212c
    mov byte [bp-006h], bl                    ; 88 5e fa                    ; 0xc212f
    mov al, cl                                ; 88 c8                       ; 0xc2132
    mov bx, 0010ch                            ; bb 0c 01                    ; 0xc2134 vgabios.c:57
    xor cx, cx                                ; 31 c9                       ; 0xc2137
    mov es, cx                                ; 8e c1                       ; 0xc2139
    mov cx, word [es:bx]                      ; 26 8b 0f                    ; 0xc213b
    mov bx, word [es:bx+002h]                 ; 26 8b 5f 02                 ; 0xc213e
    mov word [bp-014h], cx                    ; 89 4e ec                    ; 0xc2142 vgabios.c:58
    mov word [bp-010h], bx                    ; 89 5e f0                    ; 0xc2145
    xor ah, ah                                ; 30 e4                       ; 0xc2148 vgabios.c:1377
    mov cl, byte [bp+006h]                    ; 8a 4e 06                    ; 0xc214a
    xor ch, ch                                ; 30 ed                       ; 0xc214d
    imul cx                                   ; f7 e9                       ; 0xc214f
    mov bl, byte [bp+004h]                    ; 8a 5e 04                    ; 0xc2151
    xor bh, bh                                ; 30 ff                       ; 0xc2154
    imul bx                                   ; f7 eb                       ; 0xc2156
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc2158
    mov si, bx                                ; 89 de                       ; 0xc215b
    add si, ax                                ; 01 c6                       ; 0xc215d
    mov bx, strict word 0004ch                ; bb 4c 00                    ; 0xc215f vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2162
    mov es, ax                                ; 8e c0                       ; 0xc2165
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc2167
    mov bl, byte [bp+008h]                    ; 8a 5e 08                    ; 0xc216a vgabios.c:48
    xor bh, bh                                ; 30 ff                       ; 0xc216d
    mul bx                                    ; f7 e3                       ; 0xc216f
    add si, ax                                ; 01 c6                       ; 0xc2171
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc2173 vgabios.c:1379
    xor ah, ah                                ; 30 e4                       ; 0xc2176
    imul cx                                   ; f7 e9                       ; 0xc2178
    mov word [bp-012h], ax                    ; 89 46 ee                    ; 0xc217a
    mov ax, 00f02h                            ; b8 02 0f                    ; 0xc217d vgabios.c:1380
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc2180
    out DX, ax                                ; ef                          ; 0xc2183
    mov ax, 00205h                            ; b8 05 02                    ; 0xc2184 vgabios.c:1381
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2187
    out DX, ax                                ; ef                          ; 0xc218a
    test byte [bp-00ah], 080h                 ; f6 46 f6 80                 ; 0xc218b vgabios.c:1382
    je short 02197h                           ; 74 06                       ; 0xc218f
    mov ax, 01803h                            ; b8 03 18                    ; 0xc2191 vgabios.c:1384
    out DX, ax                                ; ef                          ; 0xc2194
    jmp short 0219bh                          ; eb 04                       ; 0xc2195 vgabios.c:1386
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc2197 vgabios.c:1388
    out DX, ax                                ; ef                          ; 0xc219a
    xor ch, ch                                ; 30 ed                       ; 0xc219b vgabios.c:1390
    cmp ch, byte [bp+006h]                    ; 3a 6e 06                    ; 0xc219d
    jnc short 021b7h                          ; 73 15                       ; 0xc21a0
    mov al, ch                                ; 88 e8                       ; 0xc21a2 vgabios.c:1392
    xor ah, ah                                ; 30 e4                       ; 0xc21a4
    mov bl, byte [bp+004h]                    ; 8a 5e 04                    ; 0xc21a6
    xor bh, bh                                ; 30 ff                       ; 0xc21a9
    imul bx                                   ; f7 eb                       ; 0xc21ab
    mov bx, si                                ; 89 f3                       ; 0xc21ad
    add bx, ax                                ; 01 c3                       ; 0xc21af
    mov byte [bp-008h], 000h                  ; c6 46 f8 00                 ; 0xc21b1 vgabios.c:1393
    jmp short 021cbh                          ; eb 14                       ; 0xc21b5
    jmp short 0221bh                          ; eb 62                       ; 0xc21b7 vgabios.c:1402
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc21b9 vgabios.c:42
    mov es, ax                                ; 8e c0                       ; 0xc21bc
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc21be
    inc byte [bp-008h]                        ; fe 46 f8                    ; 0xc21c2 vgabios.c:1406
    cmp byte [bp-008h], 008h                  ; 80 7e f8 08                 ; 0xc21c5
    jnc short 02217h                          ; 73 4c                       ; 0xc21c9
    mov cl, byte [bp-008h]                    ; 8a 4e f8                    ; 0xc21cb
    mov ax, 00080h                            ; b8 80 00                    ; 0xc21ce
    sar ax, CL                                ; d3 f8                       ; 0xc21d1
    mov byte [bp-00eh], al                    ; 88 46 f2                    ; 0xc21d3
    mov byte [bp-00dh], 000h                  ; c6 46 f3 00                 ; 0xc21d6
    mov ax, word [bp-00eh]                    ; 8b 46 f2                    ; 0xc21da
    mov ah, al                                ; 88 c4                       ; 0xc21dd
    xor al, al                                ; 30 c0                       ; 0xc21df
    or AL, strict byte 008h                   ; 0c 08                       ; 0xc21e1
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc21e3
    out DX, ax                                ; ef                          ; 0xc21e6
    mov dx, bx                                ; 89 da                       ; 0xc21e7
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc21e9
    call 0365bh                               ; e8 6c 14                    ; 0xc21ec
    mov al, ch                                ; 88 e8                       ; 0xc21ef
    xor ah, ah                                ; 30 e4                       ; 0xc21f1
    add ax, word [bp-012h]                    ; 03 46 ee                    ; 0xc21f3
    mov es, [bp-010h]                         ; 8e 46 f0                    ; 0xc21f6
    mov di, word [bp-014h]                    ; 8b 7e ec                    ; 0xc21f9
    add di, ax                                ; 01 c7                       ; 0xc21fc
    mov al, byte [es:di]                      ; 26 8a 05                    ; 0xc21fe
    xor ah, ah                                ; 30 e4                       ; 0xc2201
    test word [bp-00eh], ax                   ; 85 46 f2                    ; 0xc2203
    je short 021b9h                           ; 74 b1                       ; 0xc2206
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2208
    and AL, strict byte 00fh                  ; 24 0f                       ; 0xc220b
    mov di, 0a000h                            ; bf 00 a0                    ; 0xc220d
    mov es, di                                ; 8e c7                       ; 0xc2210
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2212
    jmp short 021c2h                          ; eb ab                       ; 0xc2215
    db  0feh, 0c5h
    ; inc ch                                    ; fe c5                     ; 0xc2217 vgabios.c:1407
    jmp short 0219dh                          ; eb 82                       ; 0xc2219
    mov ax, 0ff08h                            ; b8 08 ff                    ; 0xc221b vgabios.c:1408
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc221e
    out DX, ax                                ; ef                          ; 0xc2221
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc2222 vgabios.c:1409
    out DX, ax                                ; ef                          ; 0xc2225
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc2226 vgabios.c:1410
    out DX, ax                                ; ef                          ; 0xc2229
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc222a vgabios.c:1411
    pop di                                    ; 5f                          ; 0xc222d
    pop si                                    ; 5e                          ; 0xc222e
    pop bp                                    ; 5d                          ; 0xc222f
    retn 00006h                               ; c2 06 00                    ; 0xc2230
  ; disGetNextSymbol 0xc2233 LB 0x2061 -> off=0x0 cb=0000000000000112 uValue=00000000000c2233 'write_gfx_char_cga'
write_gfx_char_cga:                          ; 0xc2233 LB 0x112
    push si                                   ; 56                          ; 0xc2233 vgabios.c:1414
    push di                                   ; 57                          ; 0xc2234
    push bp                                   ; 55                          ; 0xc2235
    mov bp, sp                                ; 89 e5                       ; 0xc2236
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc2238
    mov ch, al                                ; 88 c5                       ; 0xc223b
    mov byte [bp-002h], dl                    ; 88 56 fe                    ; 0xc223d
    mov al, bl                                ; 88 d8                       ; 0xc2240
    mov si, 0556ch                            ; be 6c 55                    ; 0xc2242 vgabios.c:1421
    xor ah, ah                                ; 30 e4                       ; 0xc2245 vgabios.c:1422
    mov bl, byte [bp+00ah]                    ; 8a 5e 0a                    ; 0xc2247
    xor bh, bh                                ; 30 ff                       ; 0xc224a
    imul bx                                   ; f7 eb                       ; 0xc224c
    mov bx, ax                                ; 89 c3                       ; 0xc224e
    mov al, cl                                ; 88 c8                       ; 0xc2250
    xor ah, ah                                ; 30 e4                       ; 0xc2252
    mov di, 00140h                            ; bf 40 01                    ; 0xc2254
    imul di                                   ; f7 ef                       ; 0xc2257
    add bx, ax                                ; 01 c3                       ; 0xc2259
    mov word [bp-004h], bx                    ; 89 5e fc                    ; 0xc225b
    mov al, ch                                ; 88 e8                       ; 0xc225e vgabios.c:1423
    xor ah, ah                                ; 30 e4                       ; 0xc2260
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc2262
    sal ax, CL                                ; d3 e0                       ; 0xc2264
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc2266
    xor ch, ch                                ; 30 ed                       ; 0xc2269 vgabios.c:1424
    jmp near 0228ah                           ; e9 1c 00                    ; 0xc226b
    mov al, ch                                ; 88 e8                       ; 0xc226e vgabios.c:1439
    xor ah, ah                                ; 30 e4                       ; 0xc2270
    add ax, word [bp-008h]                    ; 03 46 f8                    ; 0xc2272
    mov di, si                                ; 89 f7                       ; 0xc2275
    add di, ax                                ; 01 c7                       ; 0xc2277
    mov al, byte [di]                         ; 8a 05                       ; 0xc2279
    mov di, 0b800h                            ; bf 00 b8                    ; 0xc227b vgabios.c:42
    mov es, di                                ; 8e c7                       ; 0xc227e
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2280
    db  0feh, 0c5h
    ; inc ch                                    ; fe c5                     ; 0xc2283 vgabios.c:1443
    cmp ch, 008h                              ; 80 fd 08                    ; 0xc2285
    jnc short 022e2h                          ; 73 58                       ; 0xc2288
    mov al, ch                                ; 88 e8                       ; 0xc228a
    xor ah, ah                                ; 30 e4                       ; 0xc228c
    sar ax, 1                                 ; d1 f8                       ; 0xc228e
    mov bx, strict word 00050h                ; bb 50 00                    ; 0xc2290
    imul bx                                   ; f7 eb                       ; 0xc2293
    mov bx, word [bp-004h]                    ; 8b 5e fc                    ; 0xc2295
    add bx, ax                                ; 01 c3                       ; 0xc2298
    test ch, 001h                             ; f6 c5 01                    ; 0xc229a
    je short 022a2h                           ; 74 03                       ; 0xc229d
    add bh, 020h                              ; 80 c7 20                    ; 0xc229f
    mov DL, strict byte 080h                  ; b2 80                       ; 0xc22a2
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc22a4
    jne short 022c8h                          ; 75 1e                       ; 0xc22a8
    test byte [bp-002h], dl                   ; 84 56 fe                    ; 0xc22aa
    je short 0226eh                           ; 74 bf                       ; 0xc22ad
    mov ax, 0b800h                            ; b8 00 b8                    ; 0xc22af
    mov es, ax                                ; 8e c0                       ; 0xc22b2
    mov dl, byte [es:bx]                      ; 26 8a 17                    ; 0xc22b4
    mov al, ch                                ; 88 e8                       ; 0xc22b7
    xor ah, ah                                ; 30 e4                       ; 0xc22b9
    add ax, word [bp-008h]                    ; 03 46 f8                    ; 0xc22bb
    mov di, si                                ; 89 f7                       ; 0xc22be
    add di, ax                                ; 01 c7                       ; 0xc22c0
    mov al, byte [di]                         ; 8a 05                       ; 0xc22c2
    xor al, dl                                ; 30 d0                       ; 0xc22c4
    jmp short 0227bh                          ; eb b3                       ; 0xc22c6
    test dl, dl                               ; 84 d2                       ; 0xc22c8 vgabios.c:1445
    jbe short 02283h                          ; 76 b7                       ; 0xc22ca
    test byte [bp-002h], 080h                 ; f6 46 fe 80                 ; 0xc22cc vgabios.c:1447
    je short 022dch                           ; 74 0a                       ; 0xc22d0
    mov ax, 0b800h                            ; b8 00 b8                    ; 0xc22d2 vgabios.c:37
    mov es, ax                                ; 8e c0                       ; 0xc22d5
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc22d7
    jmp short 022deh                          ; eb 02                       ; 0xc22da vgabios.c:1451
    xor al, al                                ; 30 c0                       ; 0xc22dc vgabios.c:1453
    xor ah, ah                                ; 30 e4                       ; 0xc22de vgabios.c:1455
    jmp short 022e9h                          ; eb 07                       ; 0xc22e0
    jmp short 0233dh                          ; eb 59                       ; 0xc22e2
    cmp ah, 004h                              ; 80 fc 04                    ; 0xc22e4
    jnc short 02332h                          ; 73 49                       ; 0xc22e7
    mov byte [bp-006h], ch                    ; 88 6e fa                    ; 0xc22e9 vgabios.c:1457
    mov byte [bp-005h], 000h                  ; c6 46 fb 00                 ; 0xc22ec
    mov di, word [bp-008h]                    ; 8b 7e f8                    ; 0xc22f0
    add di, word [bp-006h]                    ; 03 7e fa                    ; 0xc22f3
    add di, si                                ; 01 f7                       ; 0xc22f6
    mov cl, byte [di]                         ; 8a 0d                       ; 0xc22f8
    mov byte [bp-00ah], cl                    ; 88 4e f6                    ; 0xc22fa
    mov byte [bp-009h], 000h                  ; c6 46 f7 00                 ; 0xc22fd
    mov byte [bp-006h], dl                    ; 88 56 fa                    ; 0xc2301
    mov byte [bp-005h], 000h                  ; c6 46 fb 00                 ; 0xc2304
    mov di, word [bp-00ah]                    ; 8b 7e f6                    ; 0xc2308
    test word [bp-006h], di                   ; 85 7e fa                    ; 0xc230b
    je short 0232ch                           ; 74 1c                       ; 0xc230e
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc2310 vgabios.c:1458
    sub cl, ah                                ; 28 e1                       ; 0xc2312
    mov dh, byte [bp-002h]                    ; 8a 76 fe                    ; 0xc2314
    and dh, 003h                              ; 80 e6 03                    ; 0xc2317
    sal cl, 1                                 ; d0 e1                       ; 0xc231a
    sal dh, CL                                ; d2 e6                       ; 0xc231c
    mov cl, dh                                ; 88 f1                       ; 0xc231e
    test byte [bp-002h], 080h                 ; f6 46 fe 80                 ; 0xc2320 vgabios.c:1459
    je short 0232ah                           ; 74 04                       ; 0xc2324
    xor al, dh                                ; 30 f0                       ; 0xc2326 vgabios.c:1461
    jmp short 0232ch                          ; eb 02                       ; 0xc2328 vgabios.c:1463
    or al, dh                                 ; 08 f0                       ; 0xc232a vgabios.c:1465
    shr dl, 1                                 ; d0 ea                       ; 0xc232c vgabios.c:1468
    db  0feh, 0c4h
    ; inc ah                                    ; fe c4                     ; 0xc232e vgabios.c:1469
    jmp short 022e4h                          ; eb b2                       ; 0xc2330
    mov di, 0b800h                            ; bf 00 b8                    ; 0xc2332 vgabios.c:42
    mov es, di                                ; 8e c7                       ; 0xc2335
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2337
    inc bx                                    ; 43                          ; 0xc233a vgabios.c:1471
    jmp short 022c8h                          ; eb 8b                       ; 0xc233b vgabios.c:1472
    mov sp, bp                                ; 89 ec                       ; 0xc233d vgabios.c:1475
    pop bp                                    ; 5d                          ; 0xc233f
    pop di                                    ; 5f                          ; 0xc2340
    pop si                                    ; 5e                          ; 0xc2341
    retn 00004h                               ; c2 04 00                    ; 0xc2342
  ; disGetNextSymbol 0xc2345 LB 0x1f4f -> off=0x0 cb=00000000000000a1 uValue=00000000000c2345 'write_gfx_char_lin'
write_gfx_char_lin:                          ; 0xc2345 LB 0xa1
    push si                                   ; 56                          ; 0xc2345 vgabios.c:1478
    push di                                   ; 57                          ; 0xc2346
    push bp                                   ; 55                          ; 0xc2347
    mov bp, sp                                ; 89 e5                       ; 0xc2348
    sub sp, strict byte 00008h                ; 83 ec 08                    ; 0xc234a
    mov bh, al                                ; 88 c7                       ; 0xc234d
    mov ch, dl                                ; 88 d5                       ; 0xc234f
    mov al, cl                                ; 88 c8                       ; 0xc2351
    mov di, 0556ch                            ; bf 6c 55                    ; 0xc2353 vgabios.c:1485
    xor ah, ah                                ; 30 e4                       ; 0xc2356 vgabios.c:1486
    mov dl, byte [bp+008h]                    ; 8a 56 08                    ; 0xc2358
    xor dh, dh                                ; 30 f6                       ; 0xc235b
    imul dx                                   ; f7 ea                       ; 0xc235d
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc235f
    mov dx, ax                                ; 89 c2                       ; 0xc2361
    sal dx, CL                                ; d3 e2                       ; 0xc2363
    mov al, bl                                ; 88 d8                       ; 0xc2365
    xor ah, ah                                ; 30 e4                       ; 0xc2367
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc2369
    sal ax, CL                                ; d3 e0                       ; 0xc236b
    add ax, dx                                ; 01 d0                       ; 0xc236d
    mov word [bp-002h], ax                    ; 89 46 fe                    ; 0xc236f
    mov al, bh                                ; 88 f8                       ; 0xc2372 vgabios.c:1487
    xor ah, ah                                ; 30 e4                       ; 0xc2374
    sal ax, CL                                ; d3 e0                       ; 0xc2376
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc2378
    xor bl, bl                                ; 30 db                       ; 0xc237b vgabios.c:1488
    jmp short 023c1h                          ; eb 42                       ; 0xc237d
    cmp AL, strict byte 008h                  ; 3c 08                       ; 0xc237f vgabios.c:1492
    jnc short 023bah                          ; 73 37                       ; 0xc2381
    xor bh, bh                                ; 30 ff                       ; 0xc2383 vgabios.c:1494
    mov dl, bl                                ; 88 da                       ; 0xc2385 vgabios.c:1495
    xor dh, dh                                ; 30 f6                       ; 0xc2387
    add dx, word [bp-006h]                    ; 03 56 fa                    ; 0xc2389
    mov si, di                                ; 89 fe                       ; 0xc238c
    add si, dx                                ; 01 d6                       ; 0xc238e
    mov dl, byte [si]                         ; 8a 14                       ; 0xc2390
    mov byte [bp-004h], dl                    ; 88 56 fc                    ; 0xc2392
    mov byte [bp-003h], bh                    ; 88 7e fd                    ; 0xc2395
    mov dl, ah                                ; 88 e2                       ; 0xc2398
    xor dh, dh                                ; 30 f6                       ; 0xc239a
    test word [bp-004h], dx                   ; 85 56 fc                    ; 0xc239c
    je short 023a3h                           ; 74 02                       ; 0xc239f
    mov bh, ch                                ; 88 ef                       ; 0xc23a1 vgabios.c:1497
    mov dl, al                                ; 88 c2                       ; 0xc23a3 vgabios.c:1499
    xor dh, dh                                ; 30 f6                       ; 0xc23a5
    mov si, word [bp-008h]                    ; 8b 76 f8                    ; 0xc23a7
    add si, dx                                ; 01 d6                       ; 0xc23aa
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc23ac vgabios.c:42
    mov es, dx                                ; 8e c2                       ; 0xc23af
    mov byte [es:si], bh                      ; 26 88 3c                    ; 0xc23b1
    shr ah, 1                                 ; d0 ec                       ; 0xc23b4 vgabios.c:1500
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc23b6 vgabios.c:1501
    jmp short 0237fh                          ; eb c5                       ; 0xc23b8
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3                     ; 0xc23ba vgabios.c:1502
    cmp bl, 008h                              ; 80 fb 08                    ; 0xc23bc
    jnc short 023deh                          ; 73 1d                       ; 0xc23bf
    mov al, bl                                ; 88 d8                       ; 0xc23c1
    xor ah, ah                                ; 30 e4                       ; 0xc23c3
    mov dl, byte [bp+008h]                    ; 8a 56 08                    ; 0xc23c5
    xor dh, dh                                ; 30 f6                       ; 0xc23c8
    imul dx                                   ; f7 ea                       ; 0xc23ca
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc23cc
    sal ax, CL                                ; d3 e0                       ; 0xc23ce
    mov dx, word [bp-002h]                    ; 8b 56 fe                    ; 0xc23d0
    add dx, ax                                ; 01 c2                       ; 0xc23d3
    mov word [bp-008h], dx                    ; 89 56 f8                    ; 0xc23d5
    mov AH, strict byte 080h                  ; b4 80                       ; 0xc23d8
    xor al, al                                ; 30 c0                       ; 0xc23da
    jmp short 02383h                          ; eb a5                       ; 0xc23dc
    mov sp, bp                                ; 89 ec                       ; 0xc23de vgabios.c:1503
    pop bp                                    ; 5d                          ; 0xc23e0
    pop di                                    ; 5f                          ; 0xc23e1
    pop si                                    ; 5e                          ; 0xc23e2
    retn 00002h                               ; c2 02 00                    ; 0xc23e3
  ; disGetNextSymbol 0xc23e6 LB 0x1eae -> off=0x0 cb=0000000000000172 uValue=00000000000c23e6 'biosfn_write_char_attr'
biosfn_write_char_attr:                      ; 0xc23e6 LB 0x172
    push bp                                   ; 55                          ; 0xc23e6 vgabios.c:1506
    mov bp, sp                                ; 89 e5                       ; 0xc23e7
    push si                                   ; 56                          ; 0xc23e9
    push di                                   ; 57                          ; 0xc23ea
    sub sp, strict byte 0001ah                ; 83 ec 1a                    ; 0xc23eb
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc23ee
    mov byte [bp-00eh], dl                    ; 88 56 f2                    ; 0xc23f1
    mov byte [bp-008h], bl                    ; 88 5e f8                    ; 0xc23f4
    mov si, cx                                ; 89 ce                       ; 0xc23f7
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc23f9 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc23fc
    mov es, ax                                ; 8e c0                       ; 0xc23ff
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc2401
    xor ah, ah                                ; 30 e4                       ; 0xc2404 vgabios.c:1514
    call 03630h                               ; e8 27 12                    ; 0xc2406
    mov cl, al                                ; 88 c1                       ; 0xc2409
    mov byte [bp-014h], al                    ; 88 46 ec                    ; 0xc240b
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc240e vgabios.c:1515
    jne short 02415h                          ; 75 03                       ; 0xc2410
    jmp near 02551h                           ; e9 3c 01                    ; 0xc2412
    mov al, dl                                ; 88 d0                       ; 0xc2415 vgabios.c:1518
    xor ah, ah                                ; 30 e4                       ; 0xc2417
    lea bx, [bp-01ch]                         ; 8d 5e e4                    ; 0xc2419
    lea dx, [bp-01eh]                         ; 8d 56 e2                    ; 0xc241c
    call 00a1bh                               ; e8 f9 e5                    ; 0xc241f
    mov al, byte [bp-01ch]                    ; 8a 46 e4                    ; 0xc2422 vgabios.c:1519
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc2425
    mov ax, word [bp-01ch]                    ; 8b 46 e4                    ; 0xc2428
    mov al, ah                                ; 88 e0                       ; 0xc242b
    xor ah, ah                                ; 30 e4                       ; 0xc242d
    mov word [bp-018h], ax                    ; 89 46 e8                    ; 0xc242f
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc2432
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc2435
    mov bx, 00084h                            ; bb 84 00                    ; 0xc2438 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc243b
    mov es, ax                                ; 8e c0                       ; 0xc243e
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc2440
    xor ah, ah                                ; 30 e4                       ; 0xc2443 vgabios.c:38
    mov dx, ax                                ; 89 c2                       ; 0xc2445
    inc dx                                    ; 42                          ; 0xc2447
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc2448 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc244b
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc244e
    mov word [bp-01ah], ax                    ; 89 46 e6                    ; 0xc2451 vgabios.c:48
    mov bl, cl                                ; 88 cb                       ; 0xc2454 vgabios.c:1525
    xor bh, bh                                ; 30 ff                       ; 0xc2456
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc2458
    mov di, bx                                ; 89 df                       ; 0xc245a
    sal di, CL                                ; d3 e7                       ; 0xc245c
    cmp byte [di+047afh], 000h                ; 80 bd af 47 00              ; 0xc245e
    jne short 024a5h                          ; 75 40                       ; 0xc2463
    mul dx                                    ; f7 e2                       ; 0xc2465 vgabios.c:1528
    sal ax, 1                                 ; d1 e0                       ; 0xc2467
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc2469
    mov dl, byte [bp-00eh]                    ; 8a 56 f2                    ; 0xc246b
    xor dh, dh                                ; 30 f6                       ; 0xc246e
    inc ax                                    ; 40                          ; 0xc2470
    mul dx                                    ; f7 e2                       ; 0xc2471
    mov bx, ax                                ; 89 c3                       ; 0xc2473
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc2475
    xor ah, ah                                ; 30 e4                       ; 0xc2478
    mul word [bp-016h]                        ; f7 66 ea                    ; 0xc247a
    mov dx, ax                                ; 89 c2                       ; 0xc247d
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc247f
    xor ah, ah                                ; 30 e4                       ; 0xc2482
    add ax, dx                                ; 01 d0                       ; 0xc2484
    sal ax, 1                                 ; d1 e0                       ; 0xc2486
    add bx, ax                                ; 01 c3                       ; 0xc2488
    mov ah, byte [bp-008h]                    ; 8a 66 f8                    ; 0xc248a vgabios.c:1530
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc248d
    mov word [bp-01eh], ax                    ; 89 46 e2                    ; 0xc2490
    mov ax, word [bp-01eh]                    ; 8b 46 e2                    ; 0xc2493 vgabios.c:1531
    mov es, [di+047b2h]                       ; 8e 85 b2 47                 ; 0xc2496
    mov cx, si                                ; 89 f1                       ; 0xc249a
    mov di, bx                                ; 89 df                       ; 0xc249c
    jcxz 024a2h                               ; e3 02                       ; 0xc249e
    rep stosw                                 ; f3 ab                       ; 0xc24a0
    jmp near 02551h                           ; e9 ac 00                    ; 0xc24a2 vgabios.c:1533
    mov bl, byte [bx+0482eh]                  ; 8a 9f 2e 48                 ; 0xc24a5 vgabios.c:1536
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc24a9
    sal bx, CL                                ; d3 e3                       ; 0xc24ab
    mov al, byte [bx+04844h]                  ; 8a 87 44 48                 ; 0xc24ad
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc24b1
    mov al, byte [di+047b1h]                  ; 8a 85 b1 47                 ; 0xc24b4 vgabios.c:1537
    mov byte [bp-012h], al                    ; 88 46 ee                    ; 0xc24b8
    dec si                                    ; 4e                          ; 0xc24bb vgabios.c:1538
    cmp si, strict byte 0ffffh                ; 83 fe ff                    ; 0xc24bc
    je short 0250dh                           ; 74 4c                       ; 0xc24bf
    mov bl, byte [bp-014h]                    ; 8a 5e ec                    ; 0xc24c1 vgabios.c:1540
    xor bh, bh                                ; 30 ff                       ; 0xc24c4
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc24c6
    sal bx, CL                                ; d3 e3                       ; 0xc24c8
    mov al, byte [bx+047b0h]                  ; 8a 87 b0 47                 ; 0xc24ca
    cmp al, cl                                ; 38 c8                       ; 0xc24ce
    jc short 024deh                           ; 72 0c                       ; 0xc24d0
    jbe short 024e4h                          ; 76 10                       ; 0xc24d2
    cmp AL, strict byte 005h                  ; 3c 05                       ; 0xc24d4
    je short 02530h                           ; 74 58                       ; 0xc24d6
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc24d8
    je short 024e8h                           ; 74 0c                       ; 0xc24da
    jmp short 0254bh                          ; eb 6d                       ; 0xc24dc
    cmp AL, strict byte 002h                  ; 3c 02                       ; 0xc24de
    je short 0250fh                           ; 74 2d                       ; 0xc24e0
    jmp short 0254bh                          ; eb 67                       ; 0xc24e2
    or byte [bp-008h], 001h                   ; 80 4e f8 01                 ; 0xc24e4 vgabios.c:1543
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc24e8 vgabios.c:1545
    xor ah, ah                                ; 30 e4                       ; 0xc24eb
    push ax                                   ; 50                          ; 0xc24ed
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc24ee
    push ax                                   ; 50                          ; 0xc24f1
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc24f2
    push ax                                   ; 50                          ; 0xc24f5
    mov cl, byte [bp-006h]                    ; 8a 4e fa                    ; 0xc24f6
    xor ch, ch                                ; 30 ed                       ; 0xc24f9
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc24fb
    xor bh, bh                                ; 30 ff                       ; 0xc24fe
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc2500
    xor dh, dh                                ; 30 f6                       ; 0xc2503
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc2505
    call 02121h                               ; e8 16 fc                    ; 0xc2508
    jmp short 0254bh                          ; eb 3e                       ; 0xc250b vgabios.c:1546
    jmp short 02551h                          ; eb 42                       ; 0xc250d
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc250f vgabios.c:1548
    xor ah, ah                                ; 30 e4                       ; 0xc2512
    push ax                                   ; 50                          ; 0xc2514
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc2515
    push ax                                   ; 50                          ; 0xc2518
    mov cl, byte [bp-006h]                    ; 8a 4e fa                    ; 0xc2519
    xor ch, ch                                ; 30 ed                       ; 0xc251c
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc251e
    xor bh, bh                                ; 30 ff                       ; 0xc2521
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc2523
    xor dh, dh                                ; 30 f6                       ; 0xc2526
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc2528
    call 02233h                               ; e8 05 fd                    ; 0xc252b
    jmp short 0254bh                          ; eb 1b                       ; 0xc252e vgabios.c:1549
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc2530 vgabios.c:1551
    xor ah, ah                                ; 30 e4                       ; 0xc2533
    push ax                                   ; 50                          ; 0xc2535
    mov cl, byte [bp-006h]                    ; 8a 4e fa                    ; 0xc2536
    xor ch, ch                                ; 30 ed                       ; 0xc2539
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc253b
    xor bh, bh                                ; 30 ff                       ; 0xc253e
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc2540
    xor dh, dh                                ; 30 f6                       ; 0xc2543
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc2545
    call 02345h                               ; e8 fa fd                    ; 0xc2548
    inc byte [bp-00ah]                        ; fe 46 f6                    ; 0xc254b vgabios.c:1558
    jmp near 024bbh                           ; e9 6a ff                    ; 0xc254e vgabios.c:1559
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc2551 vgabios.c:1561
    pop di                                    ; 5f                          ; 0xc2554
    pop si                                    ; 5e                          ; 0xc2555
    pop bp                                    ; 5d                          ; 0xc2556
    retn                                      ; c3                          ; 0xc2557
  ; disGetNextSymbol 0xc2558 LB 0x1d3c -> off=0x0 cb=0000000000000183 uValue=00000000000c2558 'biosfn_write_char_only'
biosfn_write_char_only:                      ; 0xc2558 LB 0x183
    push bp                                   ; 55                          ; 0xc2558 vgabios.c:1564
    mov bp, sp                                ; 89 e5                       ; 0xc2559
    push si                                   ; 56                          ; 0xc255b
    push di                                   ; 57                          ; 0xc255c
    sub sp, strict byte 0001ah                ; 83 ec 1a                    ; 0xc255d
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc2560
    mov byte [bp-00eh], dl                    ; 88 56 f2                    ; 0xc2563
    mov byte [bp-008h], bl                    ; 88 5e f8                    ; 0xc2566
    mov si, cx                                ; 89 ce                       ; 0xc2569
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc256b vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc256e
    mov es, ax                                ; 8e c0                       ; 0xc2571
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc2573
    xor ah, ah                                ; 30 e4                       ; 0xc2576 vgabios.c:1572
    call 03630h                               ; e8 b5 10                    ; 0xc2578
    mov cl, al                                ; 88 c1                       ; 0xc257b
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc257d
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc2580 vgabios.c:1573
    jne short 02587h                          ; 75 03                       ; 0xc2582
    jmp near 026d4h                           ; e9 4d 01                    ; 0xc2584
    mov al, dl                                ; 88 d0                       ; 0xc2587 vgabios.c:1576
    xor ah, ah                                ; 30 e4                       ; 0xc2589
    lea bx, [bp-01eh]                         ; 8d 5e e2                    ; 0xc258b
    lea dx, [bp-01ch]                         ; 8d 56 e4                    ; 0xc258e
    call 00a1bh                               ; e8 87 e4                    ; 0xc2591
    mov al, byte [bp-01eh]                    ; 8a 46 e2                    ; 0xc2594 vgabios.c:1577
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc2597
    mov ax, word [bp-01eh]                    ; 8b 46 e2                    ; 0xc259a
    mov al, ah                                ; 88 e0                       ; 0xc259d
    xor ah, ah                                ; 30 e4                       ; 0xc259f
    mov word [bp-01ah], ax                    ; 89 46 e6                    ; 0xc25a1
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc25a4
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc25a7
    mov bx, 00084h                            ; bb 84 00                    ; 0xc25aa vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc25ad
    mov es, ax                                ; 8e c0                       ; 0xc25b0
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc25b2
    xor ah, ah                                ; 30 e4                       ; 0xc25b5 vgabios.c:38
    mov dx, ax                                ; 89 c2                       ; 0xc25b7
    inc dx                                    ; 42                          ; 0xc25b9
    mov di, strict word 0004ah                ; bf 4a 00                    ; 0xc25ba vgabios.c:47
    mov di, word [es:di]                      ; 26 8b 3d                    ; 0xc25bd
    mov word [bp-018h], di                    ; 89 7e e8                    ; 0xc25c0 vgabios.c:48
    mov al, cl                                ; 88 c8                       ; 0xc25c3 vgabios.c:1583
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc25c5
    mov bx, ax                                ; 89 c3                       ; 0xc25c7
    sal bx, CL                                ; d3 e3                       ; 0xc25c9
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc25cb
    jne short 02617h                          ; 75 45                       ; 0xc25d0
    mov ax, di                                ; 89 f8                       ; 0xc25d2 vgabios.c:1586
    mul dx                                    ; f7 e2                       ; 0xc25d4
    sal ax, 1                                 ; d1 e0                       ; 0xc25d6
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc25d8
    mov dl, byte [bp-00eh]                    ; 8a 56 f2                    ; 0xc25da
    xor dh, dh                                ; 30 f6                       ; 0xc25dd
    inc ax                                    ; 40                          ; 0xc25df
    mul dx                                    ; f7 e2                       ; 0xc25e0
    mov bx, ax                                ; 89 c3                       ; 0xc25e2
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc25e4
    xor ah, ah                                ; 30 e4                       ; 0xc25e7
    mul di                                    ; f7 e7                       ; 0xc25e9
    mov dx, ax                                ; 89 c2                       ; 0xc25eb
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc25ed
    xor ah, ah                                ; 30 e4                       ; 0xc25f0
    add ax, dx                                ; 01 d0                       ; 0xc25f2
    sal ax, 1                                 ; d1 e0                       ; 0xc25f4
    add bx, ax                                ; 01 c3                       ; 0xc25f6
    dec si                                    ; 4e                          ; 0xc25f8 vgabios.c:1588
    cmp si, strict byte 0ffffh                ; 83 fe ff                    ; 0xc25f9
    je short 02584h                           ; 74 86                       ; 0xc25fc
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc25fe vgabios.c:1589
    xor ah, ah                                ; 30 e4                       ; 0xc2601
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc2603
    mov di, ax                                ; 89 c7                       ; 0xc2605
    sal di, CL                                ; d3 e7                       ; 0xc2607
    mov es, [di+047b2h]                       ; 8e 85 b2 47                 ; 0xc2609 vgabios.c:40
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc260d vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2610
    inc bx                                    ; 43                          ; 0xc2613 vgabios.c:1590
    inc bx                                    ; 43                          ; 0xc2614
    jmp short 025f8h                          ; eb e1                       ; 0xc2615 vgabios.c:1591
    mov di, ax                                ; 89 c7                       ; 0xc2617 vgabios.c:1596
    mov al, byte [di+0482eh]                  ; 8a 85 2e 48                 ; 0xc2619
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc261d
    mov di, ax                                ; 89 c7                       ; 0xc261f
    sal di, CL                                ; d3 e7                       ; 0xc2621
    mov al, byte [di+04844h]                  ; 8a 85 44 48                 ; 0xc2623
    mov byte [bp-012h], al                    ; 88 46 ee                    ; 0xc2627
    mov al, byte [bx+047b1h]                  ; 8a 87 b1 47                 ; 0xc262a vgabios.c:1597
    mov byte [bp-014h], al                    ; 88 46 ec                    ; 0xc262e
    dec si                                    ; 4e                          ; 0xc2631 vgabios.c:1598
    cmp si, strict byte 0ffffh                ; 83 fe ff                    ; 0xc2632
    je short 02687h                           ; 74 50                       ; 0xc2635
    mov bl, byte [bp-010h]                    ; 8a 5e f0                    ; 0xc2637 vgabios.c:1600
    xor bh, bh                                ; 30 ff                       ; 0xc263a
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc263c
    sal bx, CL                                ; d3 e3                       ; 0xc263e
    mov bl, byte [bx+047b0h]                  ; 8a 9f b0 47                 ; 0xc2640
    cmp bl, cl                                ; 38 cb                       ; 0xc2644
    jc short 02657h                           ; 72 0f                       ; 0xc2646
    jbe short 0265eh                          ; 76 14                       ; 0xc2648
    cmp bl, 005h                              ; 80 fb 05                    ; 0xc264a
    je short 026b3h                           ; 74 64                       ; 0xc264d
    cmp bl, 004h                              ; 80 fb 04                    ; 0xc264f
    je short 02662h                           ; 74 0e                       ; 0xc2652
    jmp near 026ceh                           ; e9 77 00                    ; 0xc2654
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc2657
    je short 02689h                           ; 74 2d                       ; 0xc265a
    jmp short 026ceh                          ; eb 70                       ; 0xc265c
    or byte [bp-008h], 001h                   ; 80 4e f8 01                 ; 0xc265e vgabios.c:1603
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc2662 vgabios.c:1605
    xor ah, ah                                ; 30 e4                       ; 0xc2665
    push ax                                   ; 50                          ; 0xc2667
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc2668
    push ax                                   ; 50                          ; 0xc266b
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc266c
    push ax                                   ; 50                          ; 0xc266f
    mov cl, byte [bp-00ch]                    ; 8a 4e f4                    ; 0xc2670
    xor ch, ch                                ; 30 ed                       ; 0xc2673
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc2675
    xor bh, bh                                ; 30 ff                       ; 0xc2678
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc267a
    xor dh, dh                                ; 30 f6                       ; 0xc267d
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc267f
    call 02121h                               ; e8 9c fa                    ; 0xc2682
    jmp short 026ceh                          ; eb 47                       ; 0xc2685 vgabios.c:1606
    jmp short 026d4h                          ; eb 4b                       ; 0xc2687
    mov al, byte [bp-014h]                    ; 8a 46 ec                    ; 0xc2689 vgabios.c:1608
    xor ah, ah                                ; 30 e4                       ; 0xc268c
    push ax                                   ; 50                          ; 0xc268e
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc268f
    push ax                                   ; 50                          ; 0xc2692
    mov cl, byte [bp-00ch]                    ; 8a 4e f4                    ; 0xc2693
    xor ch, ch                                ; 30 ed                       ; 0xc2696
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc2698
    xor bh, bh                                ; 30 ff                       ; 0xc269b
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc269d
    xor dh, dh                                ; 30 f6                       ; 0xc26a0
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc26a2
    mov byte [bp-016h], al                    ; 88 46 ea                    ; 0xc26a5
    mov byte [bp-015h], ah                    ; 88 66 eb                    ; 0xc26a8
    mov ax, word [bp-016h]                    ; 8b 46 ea                    ; 0xc26ab
    call 02233h                               ; e8 82 fb                    ; 0xc26ae
    jmp short 026ceh                          ; eb 1b                       ; 0xc26b1 vgabios.c:1609
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc26b3 vgabios.c:1611
    xor ah, ah                                ; 30 e4                       ; 0xc26b6
    push ax                                   ; 50                          ; 0xc26b8
    mov cl, byte [bp-00ch]                    ; 8a 4e f4                    ; 0xc26b9
    xor ch, ch                                ; 30 ed                       ; 0xc26bc
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc26be
    xor bh, bh                                ; 30 ff                       ; 0xc26c1
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc26c3
    xor dh, dh                                ; 30 f6                       ; 0xc26c6
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc26c8
    call 02345h                               ; e8 77 fc                    ; 0xc26cb
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc26ce vgabios.c:1618
    jmp near 02631h                           ; e9 5d ff                    ; 0xc26d1 vgabios.c:1619
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc26d4 vgabios.c:1621
    pop di                                    ; 5f                          ; 0xc26d7
    pop si                                    ; 5e                          ; 0xc26d8
    pop bp                                    ; 5d                          ; 0xc26d9
    retn                                      ; c3                          ; 0xc26da
  ; disGetNextSymbol 0xc26db LB 0x1bb9 -> off=0x0 cb=000000000000017a uValue=00000000000c26db 'biosfn_write_pixel'
biosfn_write_pixel:                          ; 0xc26db LB 0x17a
    push bp                                   ; 55                          ; 0xc26db vgabios.c:1624
    mov bp, sp                                ; 89 e5                       ; 0xc26dc
    push si                                   ; 56                          ; 0xc26de
    sub sp, strict byte 00008h                ; 83 ec 08                    ; 0xc26df
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc26e2
    mov byte [bp-004h], dl                    ; 88 56 fc                    ; 0xc26e5
    mov word [bp-008h], bx                    ; 89 5e f8                    ; 0xc26e8
    mov word [bp-00ah], cx                    ; 89 4e f6                    ; 0xc26eb
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc26ee vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc26f1
    mov es, ax                                ; 8e c0                       ; 0xc26f4
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc26f6
    xor ah, ah                                ; 30 e4                       ; 0xc26f9 vgabios.c:1631
    call 03630h                               ; e8 32 0f                    ; 0xc26fb
    mov ch, al                                ; 88 c5                       ; 0xc26fe
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc2700 vgabios.c:1632
    je short 0272bh                           ; 74 27                       ; 0xc2702
    mov bl, al                                ; 88 c3                       ; 0xc2704 vgabios.c:1633
    xor bh, bh                                ; 30 ff                       ; 0xc2706
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc2708
    sal bx, CL                                ; d3 e3                       ; 0xc270a
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc270c
    je short 0272bh                           ; 74 18                       ; 0xc2711
    mov al, byte [bx+047b0h]                  ; 8a 87 b0 47                 ; 0xc2713 vgabios.c:1635
    cmp al, cl                                ; 38 c8                       ; 0xc2717
    jc short 02727h                           ; 72 0c                       ; 0xc2719
    jbe short 02731h                          ; 76 14                       ; 0xc271b
    cmp AL, strict byte 005h                  ; 3c 05                       ; 0xc271d
    je short 0272eh                           ; 74 0d                       ; 0xc271f
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc2721
    je short 02731h                           ; 74 0c                       ; 0xc2723
    jmp short 0272bh                          ; eb 04                       ; 0xc2725
    cmp AL, strict byte 002h                  ; 3c 02                       ; 0xc2727
    je short 027a3h                           ; 74 78                       ; 0xc2729
    jmp near 0282eh                           ; e9 00 01                    ; 0xc272b
    jmp near 02834h                           ; e9 03 01                    ; 0xc272e
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc2731 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2734
    mov es, ax                                ; 8e c0                       ; 0xc2737
    mov dx, word [es:bx]                      ; 26 8b 17                    ; 0xc2739
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc273c vgabios.c:48
    mul dx                                    ; f7 e2                       ; 0xc273f
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc2741
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc2743
    shr bx, CL                                ; d3 eb                       ; 0xc2746
    add bx, ax                                ; 01 c3                       ; 0xc2748
    mov si, strict word 0004ch                ; be 4c 00                    ; 0xc274a vgabios.c:47
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc274d
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc2750 vgabios.c:48
    xor dh, dh                                ; 30 f6                       ; 0xc2753
    mul dx                                    ; f7 e2                       ; 0xc2755
    add bx, ax                                ; 01 c3                       ; 0xc2757
    mov cx, word [bp-008h]                    ; 8b 4e f8                    ; 0xc2759 vgabios.c:1641
    and cl, 007h                              ; 80 e1 07                    ; 0xc275c
    mov ax, 00080h                            ; b8 80 00                    ; 0xc275f
    sar ax, CL                                ; d3 f8                       ; 0xc2762
    mov ah, al                                ; 88 c4                       ; 0xc2764 vgabios.c:1642
    xor al, al                                ; 30 c0                       ; 0xc2766
    or AL, strict byte 008h                   ; 0c 08                       ; 0xc2768
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc276a
    out DX, ax                                ; ef                          ; 0xc276d
    mov ax, 00205h                            ; b8 05 02                    ; 0xc276e vgabios.c:1643
    out DX, ax                                ; ef                          ; 0xc2771
    mov dx, bx                                ; 89 da                       ; 0xc2772 vgabios.c:1644
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2774
    call 0365bh                               ; e8 e1 0e                    ; 0xc2777
    test byte [bp-004h], 080h                 ; f6 46 fc 80                 ; 0xc277a vgabios.c:1645
    je short 02787h                           ; 74 07                       ; 0xc277e
    mov ax, 01803h                            ; b8 03 18                    ; 0xc2780 vgabios.c:1647
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2783
    out DX, ax                                ; ef                          ; 0xc2786
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2787 vgabios.c:42
    mov es, ax                                ; 8e c0                       ; 0xc278a
    mov al, byte [bp-004h]                    ; 8a 46 fc                    ; 0xc278c
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc278f
    mov ax, 0ff08h                            ; b8 08 ff                    ; 0xc2792 vgabios.c:1650
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2795
    out DX, ax                                ; ef                          ; 0xc2798
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc2799 vgabios.c:1651
    out DX, ax                                ; ef                          ; 0xc279c
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc279d vgabios.c:1652
    out DX, ax                                ; ef                          ; 0xc27a0
    jmp short 0272bh                          ; eb 88                       ; 0xc27a1 vgabios.c:1653
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc27a3 vgabios.c:1655
    shr ax, 1                                 ; d1 e8                       ; 0xc27a6
    mov dx, strict word 00050h                ; ba 50 00                    ; 0xc27a8
    mul dx                                    ; f7 e2                       ; 0xc27ab
    cmp byte [bx+047b1h], 002h                ; 80 bf b1 47 02              ; 0xc27ad
    jne short 027bdh                          ; 75 09                       ; 0xc27b2
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc27b4 vgabios.c:1657
    shr bx, 1                                 ; d1 eb                       ; 0xc27b7
    shr bx, 1                                 ; d1 eb                       ; 0xc27b9
    jmp short 027c2h                          ; eb 05                       ; 0xc27bb vgabios.c:1659
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc27bd vgabios.c:1661
    shr bx, CL                                ; d3 eb                       ; 0xc27c0
    add bx, ax                                ; 01 c3                       ; 0xc27c2
    test byte [bp-00ah], 001h                 ; f6 46 f6 01                 ; 0xc27c4 vgabios.c:1663
    je short 027cdh                           ; 74 03                       ; 0xc27c8
    add bh, 020h                              ; 80 c7 20                    ; 0xc27ca
    mov ax, 0b800h                            ; b8 00 b8                    ; 0xc27cd vgabios.c:37
    mov es, ax                                ; 8e c0                       ; 0xc27d0
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc27d2
    mov dl, ch                                ; 88 ea                       ; 0xc27d5 vgabios.c:1665
    xor dh, dh                                ; 30 f6                       ; 0xc27d7
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc27d9
    mov si, dx                                ; 89 d6                       ; 0xc27db
    sal si, CL                                ; d3 e6                       ; 0xc27dd
    cmp byte [si+047b1h], 002h                ; 80 bc b1 47 02              ; 0xc27df
    jne short 02800h                          ; 75 1a                       ; 0xc27e4
    mov ah, byte [bp-008h]                    ; 8a 66 f8                    ; 0xc27e6 vgabios.c:1667
    and ah, cl                                ; 20 cc                       ; 0xc27e9
    mov dl, cl                                ; 88 ca                       ; 0xc27eb
    sub dl, ah                                ; 28 e2                       ; 0xc27ed
    mov ah, dl                                ; 88 d4                       ; 0xc27ef
    sal ah, 1                                 ; d0 e4                       ; 0xc27f1
    mov dl, byte [bp-004h]                    ; 8a 56 fc                    ; 0xc27f3
    and dl, cl                                ; 20 ca                       ; 0xc27f6
    mov cl, ah                                ; 88 e1                       ; 0xc27f8
    sal dl, CL                                ; d2 e2                       ; 0xc27fa
    mov AH, strict byte 003h                  ; b4 03                       ; 0xc27fc vgabios.c:1668
    jmp short 02814h                          ; eb 14                       ; 0xc27fe vgabios.c:1670
    mov ah, byte [bp-008h]                    ; 8a 66 f8                    ; 0xc2800 vgabios.c:1672
    and ah, 007h                              ; 80 e4 07                    ; 0xc2803
    mov CL, strict byte 007h                  ; b1 07                       ; 0xc2806
    sub cl, ah                                ; 28 e1                       ; 0xc2808
    mov dl, byte [bp-004h]                    ; 8a 56 fc                    ; 0xc280a
    and dl, 001h                              ; 80 e2 01                    ; 0xc280d
    sal dl, CL                                ; d2 e2                       ; 0xc2810
    mov AH, strict byte 001h                  ; b4 01                       ; 0xc2812 vgabios.c:1673
    sal ah, CL                                ; d2 e4                       ; 0xc2814
    test byte [bp-004h], 080h                 ; f6 46 fc 80                 ; 0xc2816 vgabios.c:1675
    je short 02820h                           ; 74 04                       ; 0xc281a
    xor al, dl                                ; 30 d0                       ; 0xc281c vgabios.c:1677
    jmp short 02826h                          ; eb 06                       ; 0xc281e vgabios.c:1679
    not ah                                    ; f6 d4                       ; 0xc2820 vgabios.c:1681
    and al, ah                                ; 20 e0                       ; 0xc2822
    or al, dl                                 ; 08 d0                       ; 0xc2824 vgabios.c:1682
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc2826 vgabios.c:42
    mov es, dx                                ; 8e c2                       ; 0xc2829
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc282b
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc282e vgabios.c:1685
    pop si                                    ; 5e                          ; 0xc2831
    pop bp                                    ; 5d                          ; 0xc2832
    retn                                      ; c3                          ; 0xc2833
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc2834 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2837
    mov es, ax                                ; 8e c0                       ; 0xc283a
    mov dx, word [es:bx]                      ; 26 8b 17                    ; 0xc283c
    sal dx, CL                                ; d3 e2                       ; 0xc283f vgabios.c:48
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc2841
    mul dx                                    ; f7 e2                       ; 0xc2844
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc2846
    add bx, ax                                ; 01 c3                       ; 0xc2849
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc284b vgabios.c:42
    mov es, ax                                ; 8e c0                       ; 0xc284e
    mov al, byte [bp-004h]                    ; 8a 46 fc                    ; 0xc2850
    jmp short 0282bh                          ; eb d6                       ; 0xc2853
  ; disGetNextSymbol 0xc2855 LB 0x1a3f -> off=0x0 cb=0000000000000263 uValue=00000000000c2855 'biosfn_write_teletype'
biosfn_write_teletype:                       ; 0xc2855 LB 0x263
    push bp                                   ; 55                          ; 0xc2855 vgabios.c:1698
    mov bp, sp                                ; 89 e5                       ; 0xc2856
    push si                                   ; 56                          ; 0xc2858
    sub sp, strict byte 00016h                ; 83 ec 16                    ; 0xc2859
    mov ch, al                                ; 88 c5                       ; 0xc285c
    mov byte [bp-00ah], dl                    ; 88 56 f6                    ; 0xc285e
    mov byte [bp-00ch], bl                    ; 88 5e f4                    ; 0xc2861
    mov byte [bp-004h], cl                    ; 88 4e fc                    ; 0xc2864
    cmp dl, 0ffh                              ; 80 fa ff                    ; 0xc2867 vgabios.c:1706
    jne short 0287ah                          ; 75 0e                       ; 0xc286a
    mov bx, strict word 00062h                ; bb 62 00                    ; 0xc286c vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc286f
    mov es, ax                                ; 8e c0                       ; 0xc2872
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc2874
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc2877 vgabios.c:38
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc287a vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc287d
    mov es, ax                                ; 8e c0                       ; 0xc2880
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc2882
    xor ah, ah                                ; 30 e4                       ; 0xc2885 vgabios.c:1711
    call 03630h                               ; e8 a6 0d                    ; 0xc2887
    mov byte [bp-00eh], al                    ; 88 46 f2                    ; 0xc288a
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc288d vgabios.c:1712
    je short 028f6h                           ; 74 65                       ; 0xc288f
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2891 vgabios.c:1715
    xor ah, ah                                ; 30 e4                       ; 0xc2894
    lea bx, [bp-016h]                         ; 8d 5e ea                    ; 0xc2896
    lea dx, [bp-018h]                         ; 8d 56 e8                    ; 0xc2899
    call 00a1bh                               ; e8 7c e1                    ; 0xc289c
    mov al, byte [bp-016h]                    ; 8a 46 ea                    ; 0xc289f vgabios.c:1716
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc28a2
    mov ax, word [bp-016h]                    ; 8b 46 ea                    ; 0xc28a5
    mov al, ah                                ; 88 e0                       ; 0xc28a8
    xor ah, ah                                ; 30 e4                       ; 0xc28aa
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc28ac
    mov bx, 00084h                            ; bb 84 00                    ; 0xc28af vgabios.c:37
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc28b2
    mov es, dx                                ; 8e c2                       ; 0xc28b5
    mov dl, byte [es:bx]                      ; 26 8a 17                    ; 0xc28b7
    xor dh, dh                                ; 30 f6                       ; 0xc28ba vgabios.c:38
    inc dx                                    ; 42                          ; 0xc28bc
    mov word [bp-014h], dx                    ; 89 56 ec                    ; 0xc28bd
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc28c0 vgabios.c:47
    mov dx, word [es:bx]                      ; 26 8b 17                    ; 0xc28c3
    mov word [bp-012h], dx                    ; 89 56 ee                    ; 0xc28c6 vgabios.c:48
    cmp ch, 008h                              ; 80 fd 08                    ; 0xc28c9 vgabios.c:1722
    jc short 028dch                           ; 72 0e                       ; 0xc28cc
    jbe short 028e4h                          ; 76 14                       ; 0xc28ce
    cmp ch, 00dh                              ; 80 fd 0d                    ; 0xc28d0
    je short 028f9h                           ; 74 24                       ; 0xc28d3
    cmp ch, 00ah                              ; 80 fd 0a                    ; 0xc28d5
    je short 028efh                           ; 74 15                       ; 0xc28d8
    jmp short 028ffh                          ; eb 23                       ; 0xc28da
    cmp ch, 007h                              ; 80 fd 07                    ; 0xc28dc
    jne short 028ffh                          ; 75 1e                       ; 0xc28df
    jmp near 02a07h                           ; e9 23 01                    ; 0xc28e1
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc28e4 vgabios.c:1729
    jbe short 028fch                          ; 76 12                       ; 0xc28e8
    dec byte [bp-006h]                        ; fe 4e fa                    ; 0xc28ea
    jmp short 028fch                          ; eb 0d                       ; 0xc28ed vgabios.c:1730
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc28ef vgabios.c:1733
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc28f1
    jmp short 028fch                          ; eb 06                       ; 0xc28f4 vgabios.c:1734
    jmp near 02ab2h                           ; e9 b9 01                    ; 0xc28f6
    mov byte [bp-006h], ah                    ; 88 66 fa                    ; 0xc28f9 vgabios.c:1737
    jmp near 02a07h                           ; e9 08 01                    ; 0xc28fc vgabios.c:1738
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc28ff vgabios.c:1742
    xor ah, ah                                ; 30 e4                       ; 0xc2902
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc2904
    mov bx, ax                                ; 89 c3                       ; 0xc2906
    sal bx, CL                                ; d3 e3                       ; 0xc2908
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc290a
    jne short 02953h                          ; 75 42                       ; 0xc290f
    mov ax, word [bp-012h]                    ; 8b 46 ee                    ; 0xc2911 vgabios.c:1745
    mul word [bp-014h]                        ; f7 66 ec                    ; 0xc2914
    sal ax, 1                                 ; d1 e0                       ; 0xc2917
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc2919
    mov dl, byte [bp-00ah]                    ; 8a 56 f6                    ; 0xc291b
    xor dh, dh                                ; 30 f6                       ; 0xc291e
    inc ax                                    ; 40                          ; 0xc2920
    mul dx                                    ; f7 e2                       ; 0xc2921
    mov si, ax                                ; 89 c6                       ; 0xc2923
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2925
    xor ah, ah                                ; 30 e4                       ; 0xc2928
    mul word [bp-012h]                        ; f7 66 ee                    ; 0xc292a
    mov dx, ax                                ; 89 c2                       ; 0xc292d
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc292f
    xor ah, ah                                ; 30 e4                       ; 0xc2932
    add ax, dx                                ; 01 d0                       ; 0xc2934
    sal ax, 1                                 ; d1 e0                       ; 0xc2936
    add si, ax                                ; 01 c6                       ; 0xc2938
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc293a vgabios.c:40
    mov byte [es:si], ch                      ; 26 88 2c                    ; 0xc293e vgabios.c:42
    cmp cl, byte [bp-004h]                    ; 3a 4e fc                    ; 0xc2941 vgabios.c:1750
    jne short 02983h                          ; 75 3d                       ; 0xc2944
    inc si                                    ; 46                          ; 0xc2946 vgabios.c:1751
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc2947 vgabios.c:40
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc294b
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc294e
    jmp short 02983h                          ; eb 30                       ; 0xc2951 vgabios.c:1753
    mov si, ax                                ; 89 c6                       ; 0xc2953 vgabios.c:1756
    mov al, byte [si+0482eh]                  ; 8a 84 2e 48                 ; 0xc2955
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc2959
    mov si, ax                                ; 89 c6                       ; 0xc295b
    sal si, CL                                ; d3 e6                       ; 0xc295d
    mov dl, byte [si+04844h]                  ; 8a 94 44 48                 ; 0xc295f
    mov al, byte [bx+047b1h]                  ; 8a 87 b1 47                 ; 0xc2963 vgabios.c:1757
    mov bl, byte [bx+047b0h]                  ; 8a 9f b0 47                 ; 0xc2967 vgabios.c:1758
    cmp bl, 003h                              ; 80 fb 03                    ; 0xc296b
    jc short 0297eh                           ; 72 0e                       ; 0xc296e
    jbe short 02985h                          ; 76 13                       ; 0xc2970
    cmp bl, 005h                              ; 80 fb 05                    ; 0xc2972
    je short 029d5h                           ; 74 5e                       ; 0xc2975
    cmp bl, 004h                              ; 80 fb 04                    ; 0xc2977
    je short 02989h                           ; 74 0d                       ; 0xc297a
    jmp short 029f4h                          ; eb 76                       ; 0xc297c
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc297e
    je short 029b3h                           ; 74 30                       ; 0xc2981
    jmp short 029f4h                          ; eb 6f                       ; 0xc2983
    or byte [bp-00ch], 001h                   ; 80 4e f4 01                 ; 0xc2985 vgabios.c:1761
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2989 vgabios.c:1763
    xor ah, ah                                ; 30 e4                       ; 0xc298c
    push ax                                   ; 50                          ; 0xc298e
    mov al, dl                                ; 88 d0                       ; 0xc298f
    push ax                                   ; 50                          ; 0xc2991
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc2992
    push ax                                   ; 50                          ; 0xc2995
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2996
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc2999
    xor bh, bh                                ; 30 ff                       ; 0xc299c
    mov dl, byte [bp-00ch]                    ; 8a 56 f4                    ; 0xc299e
    xor dh, dh                                ; 30 f6                       ; 0xc29a1
    mov byte [bp-010h], ch                    ; 88 6e f0                    ; 0xc29a3
    mov byte [bp-00fh], ah                    ; 88 66 f1                    ; 0xc29a6
    mov cx, ax                                ; 89 c1                       ; 0xc29a9
    mov ax, word [bp-010h]                    ; 8b 46 f0                    ; 0xc29ab
    call 02121h                               ; e8 70 f7                    ; 0xc29ae
    jmp short 029f4h                          ; eb 41                       ; 0xc29b1 vgabios.c:1764
    push ax                                   ; 50                          ; 0xc29b3 vgabios.c:1766
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc29b4
    push ax                                   ; 50                          ; 0xc29b7
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc29b8
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc29bb
    xor bh, bh                                ; 30 ff                       ; 0xc29be
    mov dl, byte [bp-00ch]                    ; 8a 56 f4                    ; 0xc29c0
    xor dh, dh                                ; 30 f6                       ; 0xc29c3
    mov byte [bp-010h], ch                    ; 88 6e f0                    ; 0xc29c5
    mov byte [bp-00fh], ah                    ; 88 66 f1                    ; 0xc29c8
    mov cx, ax                                ; 89 c1                       ; 0xc29cb
    mov ax, word [bp-010h]                    ; 8b 46 f0                    ; 0xc29cd
    call 02233h                               ; e8 60 f8                    ; 0xc29d0
    jmp short 029f4h                          ; eb 1f                       ; 0xc29d3 vgabios.c:1767
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc29d5 vgabios.c:1769
    push ax                                   ; 50                          ; 0xc29d8
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc29d9
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc29dc
    mov byte [bp-00fh], ah                    ; 88 66 f1                    ; 0xc29df
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc29e2
    xor bh, bh                                ; 30 ff                       ; 0xc29e5
    mov dl, byte [bp-00ch]                    ; 8a 56 f4                    ; 0xc29e7
    xor dh, dh                                ; 30 f6                       ; 0xc29ea
    mov al, ch                                ; 88 e8                       ; 0xc29ec
    mov cx, word [bp-010h]                    ; 8b 4e f0                    ; 0xc29ee
    call 02345h                               ; e8 51 f9                    ; 0xc29f1
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc29f4 vgabios.c:1777
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc29f7 vgabios.c:1779
    xor ah, ah                                ; 30 e4                       ; 0xc29fa
    cmp ax, word [bp-012h]                    ; 3b 46 ee                    ; 0xc29fc
    jne short 02a07h                          ; 75 06                       ; 0xc29ff
    mov byte [bp-006h], ah                    ; 88 66 fa                    ; 0xc2a01 vgabios.c:1780
    inc byte [bp-008h]                        ; fe 46 f8                    ; 0xc2a04 vgabios.c:1781
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2a07 vgabios.c:1786
    xor ah, ah                                ; 30 e4                       ; 0xc2a0a
    cmp ax, word [bp-014h]                    ; 3b 46 ec                    ; 0xc2a0c
    jne short 02a75h                          ; 75 64                       ; 0xc2a0f
    mov bl, byte [bp-00eh]                    ; 8a 5e f2                    ; 0xc2a11 vgabios.c:1788
    xor bh, bh                                ; 30 ff                       ; 0xc2a14
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc2a16
    sal bx, CL                                ; d3 e3                       ; 0xc2a18
    mov cl, byte [bp-014h]                    ; 8a 4e ec                    ; 0xc2a1a
    db  0feh, 0c9h
    ; dec cl                                    ; fe c9                     ; 0xc2a1d
    mov ch, byte [bp-012h]                    ; 8a 6e ee                    ; 0xc2a1f
    db  0feh, 0cdh
    ; dec ch                                    ; fe cd                     ; 0xc2a22
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc2a24
    jne short 02a77h                          ; 75 4c                       ; 0xc2a29
    mov ax, word [bp-012h]                    ; 8b 46 ee                    ; 0xc2a2b vgabios.c:1790
    mul word [bp-014h]                        ; f7 66 ec                    ; 0xc2a2e
    sal ax, 1                                 ; d1 e0                       ; 0xc2a31
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc2a33
    mov dl, byte [bp-00ah]                    ; 8a 56 f6                    ; 0xc2a35
    xor dh, dh                                ; 30 f6                       ; 0xc2a38
    inc ax                                    ; 40                          ; 0xc2a3a
    mul dx                                    ; f7 e2                       ; 0xc2a3b
    mov si, ax                                ; 89 c6                       ; 0xc2a3d
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2a3f
    xor ah, ah                                ; 30 e4                       ; 0xc2a42
    dec ax                                    ; 48                          ; 0xc2a44
    mul word [bp-012h]                        ; f7 66 ee                    ; 0xc2a45
    mov dx, ax                                ; 89 c2                       ; 0xc2a48
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc2a4a
    xor ah, ah                                ; 30 e4                       ; 0xc2a4d
    add ax, dx                                ; 01 d0                       ; 0xc2a4f
    sal ax, 1                                 ; d1 e0                       ; 0xc2a51
    add si, ax                                ; 01 c6                       ; 0xc2a53
    inc si                                    ; 46                          ; 0xc2a55 vgabios.c:1791
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc2a56 vgabios.c:35
    mov dl, byte [es:si]                      ; 26 8a 14                    ; 0xc2a5a vgabios.c:37
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc2a5d vgabios.c:1792
    push ax                                   ; 50                          ; 0xc2a60
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2a61
    xor ah, ah                                ; 30 e4                       ; 0xc2a64
    push ax                                   ; 50                          ; 0xc2a66
    mov al, ch                                ; 88 e8                       ; 0xc2a67
    push ax                                   ; 50                          ; 0xc2a69
    mov al, cl                                ; 88 c8                       ; 0xc2a6a
    push ax                                   ; 50                          ; 0xc2a6c
    xor dh, dh                                ; 30 f6                       ; 0xc2a6d
    xor cx, cx                                ; 31 c9                       ; 0xc2a6f
    xor bx, bx                                ; 31 db                       ; 0xc2a71
    jmp short 02a8dh                          ; eb 18                       ; 0xc2a73 vgabios.c:1794
    jmp short 02a96h                          ; eb 1f                       ; 0xc2a75
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc2a77 vgabios.c:1796
    push ax                                   ; 50                          ; 0xc2a7a
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2a7b
    xor ah, ah                                ; 30 e4                       ; 0xc2a7e
    push ax                                   ; 50                          ; 0xc2a80
    mov al, ch                                ; 88 e8                       ; 0xc2a81
    push ax                                   ; 50                          ; 0xc2a83
    mov al, cl                                ; 88 c8                       ; 0xc2a84
    push ax                                   ; 50                          ; 0xc2a86
    xor cx, cx                                ; 31 c9                       ; 0xc2a87
    xor bx, bx                                ; 31 db                       ; 0xc2a89
    xor dx, dx                                ; 31 d2                       ; 0xc2a8b
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc2a8d
    call 01a7eh                               ; e8 eb ef                    ; 0xc2a90
    dec byte [bp-008h]                        ; fe 4e f8                    ; 0xc2a93 vgabios.c:1798
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2a96 vgabios.c:1802
    xor ah, ah                                ; 30 e4                       ; 0xc2a99
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc2a9b
    mov CL, strict byte 008h                  ; b1 08                       ; 0xc2a9e
    sal word [bp-016h], CL                    ; d3 66 ea                    ; 0xc2aa0
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc2aa3
    add word [bp-016h], ax                    ; 01 46 ea                    ; 0xc2aa6
    mov dx, word [bp-016h]                    ; 8b 56 ea                    ; 0xc2aa9 vgabios.c:1803
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2aac
    call 01253h                               ; e8 a1 e7                    ; 0xc2aaf
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc2ab2 vgabios.c:1804
    pop si                                    ; 5e                          ; 0xc2ab5
    pop bp                                    ; 5d                          ; 0xc2ab6
    retn                                      ; c3                          ; 0xc2ab7
  ; disGetNextSymbol 0xc2ab8 LB 0x17dc -> off=0x0 cb=000000000000002c uValue=00000000000c2ab8 'get_font_access'
get_font_access:                             ; 0xc2ab8 LB 0x2c
    push bp                                   ; 55                          ; 0xc2ab8 vgabios.c:1807
    mov bp, sp                                ; 89 e5                       ; 0xc2ab9
    push dx                                   ; 52                          ; 0xc2abb
    mov ax, 00100h                            ; b8 00 01                    ; 0xc2abc vgabios.c:1809
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc2abf
    out DX, ax                                ; ef                          ; 0xc2ac2
    mov ax, 00402h                            ; b8 02 04                    ; 0xc2ac3 vgabios.c:1810
    out DX, ax                                ; ef                          ; 0xc2ac6
    mov ax, 00704h                            ; b8 04 07                    ; 0xc2ac7 vgabios.c:1811
    out DX, ax                                ; ef                          ; 0xc2aca
    mov ax, 00300h                            ; b8 00 03                    ; 0xc2acb vgabios.c:1812
    out DX, ax                                ; ef                          ; 0xc2ace
    mov ax, 00204h                            ; b8 04 02                    ; 0xc2acf vgabios.c:1813
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2ad2
    out DX, ax                                ; ef                          ; 0xc2ad5
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc2ad6 vgabios.c:1814
    out DX, ax                                ; ef                          ; 0xc2ad9
    mov ax, 00406h                            ; b8 06 04                    ; 0xc2ada vgabios.c:1815
    out DX, ax                                ; ef                          ; 0xc2add
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc2ade vgabios.c:1816
    pop dx                                    ; 5a                          ; 0xc2ae1
    pop bp                                    ; 5d                          ; 0xc2ae2
    retn                                      ; c3                          ; 0xc2ae3
  ; disGetNextSymbol 0xc2ae4 LB 0x17b0 -> off=0x0 cb=000000000000003f uValue=00000000000c2ae4 'release_font_access'
release_font_access:                         ; 0xc2ae4 LB 0x3f
    push bp                                   ; 55                          ; 0xc2ae4 vgabios.c:1818
    mov bp, sp                                ; 89 e5                       ; 0xc2ae5
    push dx                                   ; 52                          ; 0xc2ae7
    mov ax, 00100h                            ; b8 00 01                    ; 0xc2ae8 vgabios.c:1820
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc2aeb
    out DX, ax                                ; ef                          ; 0xc2aee
    mov ax, 00302h                            ; b8 02 03                    ; 0xc2aef vgabios.c:1821
    out DX, ax                                ; ef                          ; 0xc2af2
    mov ax, 00304h                            ; b8 04 03                    ; 0xc2af3 vgabios.c:1822
    out DX, ax                                ; ef                          ; 0xc2af6
    mov ax, 00300h                            ; b8 00 03                    ; 0xc2af7 vgabios.c:1823
    out DX, ax                                ; ef                          ; 0xc2afa
    mov dx, 003cch                            ; ba cc 03                    ; 0xc2afb vgabios.c:1824
    in AL, DX                                 ; ec                          ; 0xc2afe
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2aff
    and ax, strict word 00001h                ; 25 01 00                    ; 0xc2b01
    sal ax, 1                                 ; d1 e0                       ; 0xc2b04
    sal ax, 1                                 ; d1 e0                       ; 0xc2b06
    mov ah, al                                ; 88 c4                       ; 0xc2b08
    or ah, 00ah                               ; 80 cc 0a                    ; 0xc2b0a
    xor al, al                                ; 30 c0                       ; 0xc2b0d
    or AL, strict byte 006h                   ; 0c 06                       ; 0xc2b0f
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2b11
    out DX, ax                                ; ef                          ; 0xc2b14
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc2b15 vgabios.c:1825
    out DX, ax                                ; ef                          ; 0xc2b18
    mov ax, 01005h                            ; b8 05 10                    ; 0xc2b19 vgabios.c:1826
    out DX, ax                                ; ef                          ; 0xc2b1c
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc2b1d vgabios.c:1827
    pop dx                                    ; 5a                          ; 0xc2b20
    pop bp                                    ; 5d                          ; 0xc2b21
    retn                                      ; c3                          ; 0xc2b22
  ; disGetNextSymbol 0xc2b23 LB 0x1771 -> off=0x0 cb=00000000000000b3 uValue=00000000000c2b23 'set_scan_lines'
set_scan_lines:                              ; 0xc2b23 LB 0xb3
    push bp                                   ; 55                          ; 0xc2b23 vgabios.c:1829
    mov bp, sp                                ; 89 e5                       ; 0xc2b24
    push bx                                   ; 53                          ; 0xc2b26
    push cx                                   ; 51                          ; 0xc2b27
    push dx                                   ; 52                          ; 0xc2b28
    push si                                   ; 56                          ; 0xc2b29
    push di                                   ; 57                          ; 0xc2b2a
    mov bl, al                                ; 88 c3                       ; 0xc2b2b
    mov si, strict word 00063h                ; be 63 00                    ; 0xc2b2d vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2b30
    mov es, ax                                ; 8e c0                       ; 0xc2b33
    mov si, word [es:si]                      ; 26 8b 34                    ; 0xc2b35
    mov cx, si                                ; 89 f1                       ; 0xc2b38 vgabios.c:48
    mov AL, strict byte 009h                  ; b0 09                       ; 0xc2b3a vgabios.c:1835
    mov dx, si                                ; 89 f2                       ; 0xc2b3c
    out DX, AL                                ; ee                          ; 0xc2b3e
    lea dx, [si+001h]                         ; 8d 54 01                    ; 0xc2b3f vgabios.c:1836
    in AL, DX                                 ; ec                          ; 0xc2b42
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2b43
    mov ah, al                                ; 88 c4                       ; 0xc2b45 vgabios.c:1837
    and ah, 0e0h                              ; 80 e4 e0                    ; 0xc2b47
    mov al, bl                                ; 88 d8                       ; 0xc2b4a
    db  0feh, 0c8h
    ; dec al                                    ; fe c8                     ; 0xc2b4c
    or al, ah                                 ; 08 e0                       ; 0xc2b4e
    out DX, AL                                ; ee                          ; 0xc2b50 vgabios.c:1838
    cmp bl, 008h                              ; 80 fb 08                    ; 0xc2b51 vgabios.c:1839
    jne short 02b5eh                          ; 75 08                       ; 0xc2b54
    mov dx, strict word 00007h                ; ba 07 00                    ; 0xc2b56 vgabios.c:1841
    mov ax, strict word 00006h                ; b8 06 00                    ; 0xc2b59
    jmp short 02b6bh                          ; eb 0d                       ; 0xc2b5c vgabios.c:1843
    mov dl, bl                                ; 88 da                       ; 0xc2b5e vgabios.c:1845
    sub dl, 003h                              ; 80 ea 03                    ; 0xc2b60
    xor dh, dh                                ; 30 f6                       ; 0xc2b63
    mov al, bl                                ; 88 d8                       ; 0xc2b65
    sub AL, strict byte 004h                  ; 2c 04                       ; 0xc2b67
    xor ah, ah                                ; 30 e4                       ; 0xc2b69
    call 01157h                               ; e8 e9 e5                    ; 0xc2b6b
    xor bh, bh                                ; 30 ff                       ; 0xc2b6e vgabios.c:1847
    mov si, 00085h                            ; be 85 00                    ; 0xc2b70 vgabios.c:52
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2b73
    mov es, ax                                ; 8e c0                       ; 0xc2b76
    mov word [es:si], bx                      ; 26 89 1c                    ; 0xc2b78
    mov AL, strict byte 012h                  ; b0 12                       ; 0xc2b7b vgabios.c:1848
    mov dx, cx                                ; 89 ca                       ; 0xc2b7d
    out DX, AL                                ; ee                          ; 0xc2b7f
    mov si, cx                                ; 89 ce                       ; 0xc2b80 vgabios.c:1849
    inc si                                    ; 46                          ; 0xc2b82
    mov dx, si                                ; 89 f2                       ; 0xc2b83
    in AL, DX                                 ; ec                          ; 0xc2b85
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2b86
    mov di, ax                                ; 89 c7                       ; 0xc2b88
    mov AL, strict byte 007h                  ; b0 07                       ; 0xc2b8a vgabios.c:1850
    mov dx, cx                                ; 89 ca                       ; 0xc2b8c
    out DX, AL                                ; ee                          ; 0xc2b8e
    mov dx, si                                ; 89 f2                       ; 0xc2b8f vgabios.c:1851
    in AL, DX                                 ; ec                          ; 0xc2b91
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2b92
    mov dl, al                                ; 88 c2                       ; 0xc2b94 vgabios.c:1852
    and dl, 002h                              ; 80 e2 02                    ; 0xc2b96
    xor dh, dh                                ; 30 f6                       ; 0xc2b99
    mov CL, strict byte 007h                  ; b1 07                       ; 0xc2b9b
    sal dx, CL                                ; d3 e2                       ; 0xc2b9d
    and AL, strict byte 040h                  ; 24 40                       ; 0xc2b9f
    xor ah, ah                                ; 30 e4                       ; 0xc2ba1
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc2ba3
    sal ax, CL                                ; d3 e0                       ; 0xc2ba5
    add ax, dx                                ; 01 d0                       ; 0xc2ba7
    inc ax                                    ; 40                          ; 0xc2ba9
    add ax, di                                ; 01 f8                       ; 0xc2baa
    xor dx, dx                                ; 31 d2                       ; 0xc2bac vgabios.c:1853
    div bx                                    ; f7 f3                       ; 0xc2bae
    mov dl, al                                ; 88 c2                       ; 0xc2bb0 vgabios.c:1854
    db  0feh, 0cah
    ; dec dl                                    ; fe ca                     ; 0xc2bb2
    mov si, 00084h                            ; be 84 00                    ; 0xc2bb4 vgabios.c:42
    mov byte [es:si], dl                      ; 26 88 14                    ; 0xc2bb7
    mov si, strict word 0004ah                ; be 4a 00                    ; 0xc2bba vgabios.c:47
    mov dx, word [es:si]                      ; 26 8b 14                    ; 0xc2bbd
    xor ah, ah                                ; 30 e4                       ; 0xc2bc0 vgabios.c:1856
    mul dx                                    ; f7 e2                       ; 0xc2bc2
    sal ax, 1                                 ; d1 e0                       ; 0xc2bc4
    mov si, strict word 0004ch                ; be 4c 00                    ; 0xc2bc6 vgabios.c:52
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc2bc9
    lea sp, [bp-00ah]                         ; 8d 66 f6                    ; 0xc2bcc vgabios.c:1857
    pop di                                    ; 5f                          ; 0xc2bcf
    pop si                                    ; 5e                          ; 0xc2bd0
    pop dx                                    ; 5a                          ; 0xc2bd1
    pop cx                                    ; 59                          ; 0xc2bd2
    pop bx                                    ; 5b                          ; 0xc2bd3
    pop bp                                    ; 5d                          ; 0xc2bd4
    retn                                      ; c3                          ; 0xc2bd5
  ; disGetNextSymbol 0xc2bd6 LB 0x16be -> off=0x0 cb=0000000000000084 uValue=00000000000c2bd6 'biosfn_load_text_user_pat'
biosfn_load_text_user_pat:                   ; 0xc2bd6 LB 0x84
    push bp                                   ; 55                          ; 0xc2bd6 vgabios.c:1859
    mov bp, sp                                ; 89 e5                       ; 0xc2bd7
    push si                                   ; 56                          ; 0xc2bd9
    push di                                   ; 57                          ; 0xc2bda
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc2bdb
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc2bde
    mov word [bp-00eh], dx                    ; 89 56 f2                    ; 0xc2be1
    mov word [bp-00ah], bx                    ; 89 5e f6                    ; 0xc2be4
    mov word [bp-00ch], cx                    ; 89 4e f4                    ; 0xc2be7
    call 02ab8h                               ; e8 cb fe                    ; 0xc2bea vgabios.c:1864
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc2bed vgabios.c:1865
    and AL, strict byte 003h                  ; 24 03                       ; 0xc2bf0
    xor ah, ah                                ; 30 e4                       ; 0xc2bf2
    mov CL, strict byte 00eh                  ; b1 0e                       ; 0xc2bf4
    mov bx, ax                                ; 89 c3                       ; 0xc2bf6
    sal bx, CL                                ; d3 e3                       ; 0xc2bf8
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc2bfa
    and AL, strict byte 004h                  ; 24 04                       ; 0xc2bfd
    mov CL, strict byte 00bh                  ; b1 0b                       ; 0xc2bff
    sal ax, CL                                ; d3 e0                       ; 0xc2c01
    add bx, ax                                ; 01 c3                       ; 0xc2c03
    mov word [bp-008h], bx                    ; 89 5e f8                    ; 0xc2c05
    xor bx, bx                                ; 31 db                       ; 0xc2c08 vgabios.c:1866
    cmp bx, word [bp-00ch]                    ; 3b 5e f4                    ; 0xc2c0a
    jnc short 02c40h                          ; 73 31                       ; 0xc2c0d
    mov al, byte [bp+008h]                    ; 8a 46 08                    ; 0xc2c0f vgabios.c:1868
    xor ah, ah                                ; 30 e4                       ; 0xc2c12
    mov si, ax                                ; 89 c6                       ; 0xc2c14
    mov ax, bx                                ; 89 d8                       ; 0xc2c16
    mul si                                    ; f7 e6                       ; 0xc2c18
    add ax, word [bp-00ah]                    ; 03 46 f6                    ; 0xc2c1a
    mov di, word [bp+004h]                    ; 8b 7e 04                    ; 0xc2c1d vgabios.c:1869
    add di, bx                                ; 01 df                       ; 0xc2c20
    mov CL, strict byte 005h                  ; b1 05                       ; 0xc2c22
    sal di, CL                                ; d3 e7                       ; 0xc2c24
    add di, word [bp-008h]                    ; 03 7e f8                    ; 0xc2c26
    mov cx, si                                ; 89 f1                       ; 0xc2c29 vgabios.c:1870
    mov si, ax                                ; 89 c6                       ; 0xc2c2b
    mov dx, word [bp-00eh]                    ; 8b 56 f2                    ; 0xc2c2d
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2c30
    mov es, ax                                ; 8e c0                       ; 0xc2c33
    jcxz 02c3dh                               ; e3 06                       ; 0xc2c35
    push DS                                   ; 1e                          ; 0xc2c37
    mov ds, dx                                ; 8e da                       ; 0xc2c38
    rep movsb                                 ; f3 a4                       ; 0xc2c3a
    pop DS                                    ; 1f                          ; 0xc2c3c
    inc bx                                    ; 43                          ; 0xc2c3d vgabios.c:1871
    jmp short 02c0ah                          ; eb ca                       ; 0xc2c3e
    call 02ae4h                               ; e8 a1 fe                    ; 0xc2c40 vgabios.c:1872
    cmp byte [bp-006h], 010h                  ; 80 7e fa 10                 ; 0xc2c43 vgabios.c:1873
    jc short 02c51h                           ; 72 08                       ; 0xc2c47
    mov al, byte [bp+008h]                    ; 8a 46 08                    ; 0xc2c49 vgabios.c:1875
    xor ah, ah                                ; 30 e4                       ; 0xc2c4c
    call 02b23h                               ; e8 d2 fe                    ; 0xc2c4e
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc2c51 vgabios.c:1877
    pop di                                    ; 5f                          ; 0xc2c54
    pop si                                    ; 5e                          ; 0xc2c55
    pop bp                                    ; 5d                          ; 0xc2c56
    retn 00006h                               ; c2 06 00                    ; 0xc2c57
  ; disGetNextSymbol 0xc2c5a LB 0x163a -> off=0x0 cb=0000000000000075 uValue=00000000000c2c5a 'biosfn_load_text_8_14_pat'
biosfn_load_text_8_14_pat:                   ; 0xc2c5a LB 0x75
    push bp                                   ; 55                          ; 0xc2c5a vgabios.c:1879
    mov bp, sp                                ; 89 e5                       ; 0xc2c5b
    push bx                                   ; 53                          ; 0xc2c5d
    push cx                                   ; 51                          ; 0xc2c5e
    push si                                   ; 56                          ; 0xc2c5f
    push di                                   ; 57                          ; 0xc2c60
    push ax                                   ; 50                          ; 0xc2c61
    push ax                                   ; 50                          ; 0xc2c62
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc2c63
    call 02ab8h                               ; e8 4f fe                    ; 0xc2c66 vgabios.c:1883
    mov al, dl                                ; 88 d0                       ; 0xc2c69 vgabios.c:1884
    and AL, strict byte 003h                  ; 24 03                       ; 0xc2c6b
    xor ah, ah                                ; 30 e4                       ; 0xc2c6d
    mov CL, strict byte 00eh                  ; b1 0e                       ; 0xc2c6f
    mov bx, ax                                ; 89 c3                       ; 0xc2c71
    sal bx, CL                                ; d3 e3                       ; 0xc2c73
    mov al, dl                                ; 88 d0                       ; 0xc2c75
    and AL, strict byte 004h                  ; 24 04                       ; 0xc2c77
    mov CL, strict byte 00bh                  ; b1 0b                       ; 0xc2c79
    sal ax, CL                                ; d3 e0                       ; 0xc2c7b
    add bx, ax                                ; 01 c3                       ; 0xc2c7d
    mov word [bp-00ch], bx                    ; 89 5e f4                    ; 0xc2c7f
    xor bx, bx                                ; 31 db                       ; 0xc2c82 vgabios.c:1885
    jmp short 02c8ch                          ; eb 06                       ; 0xc2c84
    cmp bx, 00100h                            ; 81 fb 00 01                 ; 0xc2c86
    jnc short 02cb7h                          ; 73 2b                       ; 0xc2c8a
    mov ax, bx                                ; 89 d8                       ; 0xc2c8c vgabios.c:1887
    mov si, strict word 0000eh                ; be 0e 00                    ; 0xc2c8e
    mul si                                    ; f7 e6                       ; 0xc2c91
    mov CL, strict byte 005h                  ; b1 05                       ; 0xc2c93 vgabios.c:1888
    mov di, bx                                ; 89 df                       ; 0xc2c95
    sal di, CL                                ; d3 e7                       ; 0xc2c97
    add di, word [bp-00ch]                    ; 03 7e f4                    ; 0xc2c99
    mov si, 05d6ch                            ; be 6c 5d                    ; 0xc2c9c vgabios.c:1889
    add si, ax                                ; 01 c6                       ; 0xc2c9f
    mov cx, strict word 0000eh                ; b9 0e 00                    ; 0xc2ca1
    mov dx, 0c000h                            ; ba 00 c0                    ; 0xc2ca4
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2ca7
    mov es, ax                                ; 8e c0                       ; 0xc2caa
    jcxz 02cb4h                               ; e3 06                       ; 0xc2cac
    push DS                                   ; 1e                          ; 0xc2cae
    mov ds, dx                                ; 8e da                       ; 0xc2caf
    rep movsb                                 ; f3 a4                       ; 0xc2cb1
    pop DS                                    ; 1f                          ; 0xc2cb3
    inc bx                                    ; 43                          ; 0xc2cb4 vgabios.c:1890
    jmp short 02c86h                          ; eb cf                       ; 0xc2cb5
    call 02ae4h                               ; e8 2a fe                    ; 0xc2cb7 vgabios.c:1891
    cmp byte [bp-00ah], 010h                  ; 80 7e f6 10                 ; 0xc2cba vgabios.c:1892
    jc short 02cc6h                           ; 72 06                       ; 0xc2cbe
    mov ax, strict word 0000eh                ; b8 0e 00                    ; 0xc2cc0 vgabios.c:1894
    call 02b23h                               ; e8 5d fe                    ; 0xc2cc3
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc2cc6 vgabios.c:1896
    pop di                                    ; 5f                          ; 0xc2cc9
    pop si                                    ; 5e                          ; 0xc2cca
    pop cx                                    ; 59                          ; 0xc2ccb
    pop bx                                    ; 5b                          ; 0xc2ccc
    pop bp                                    ; 5d                          ; 0xc2ccd
    retn                                      ; c3                          ; 0xc2cce
  ; disGetNextSymbol 0xc2ccf LB 0x15c5 -> off=0x0 cb=0000000000000073 uValue=00000000000c2ccf 'biosfn_load_text_8_8_pat'
biosfn_load_text_8_8_pat:                    ; 0xc2ccf LB 0x73
    push bp                                   ; 55                          ; 0xc2ccf vgabios.c:1898
    mov bp, sp                                ; 89 e5                       ; 0xc2cd0
    push bx                                   ; 53                          ; 0xc2cd2
    push cx                                   ; 51                          ; 0xc2cd3
    push si                                   ; 56                          ; 0xc2cd4
    push di                                   ; 57                          ; 0xc2cd5
    push ax                                   ; 50                          ; 0xc2cd6
    push ax                                   ; 50                          ; 0xc2cd7
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc2cd8
    call 02ab8h                               ; e8 da fd                    ; 0xc2cdb vgabios.c:1902
    mov al, dl                                ; 88 d0                       ; 0xc2cde vgabios.c:1903
    and AL, strict byte 003h                  ; 24 03                       ; 0xc2ce0
    xor ah, ah                                ; 30 e4                       ; 0xc2ce2
    mov CL, strict byte 00eh                  ; b1 0e                       ; 0xc2ce4
    mov bx, ax                                ; 89 c3                       ; 0xc2ce6
    sal bx, CL                                ; d3 e3                       ; 0xc2ce8
    mov al, dl                                ; 88 d0                       ; 0xc2cea
    and AL, strict byte 004h                  ; 24 04                       ; 0xc2cec
    mov CL, strict byte 00bh                  ; b1 0b                       ; 0xc2cee
    sal ax, CL                                ; d3 e0                       ; 0xc2cf0
    add bx, ax                                ; 01 c3                       ; 0xc2cf2
    mov word [bp-00ch], bx                    ; 89 5e f4                    ; 0xc2cf4
    xor bx, bx                                ; 31 db                       ; 0xc2cf7 vgabios.c:1904
    jmp short 02d01h                          ; eb 06                       ; 0xc2cf9
    cmp bx, 00100h                            ; 81 fb 00 01                 ; 0xc2cfb
    jnc short 02d2ah                          ; 73 29                       ; 0xc2cff
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc2d01 vgabios.c:1906
    mov si, bx                                ; 89 de                       ; 0xc2d03
    sal si, CL                                ; d3 e6                       ; 0xc2d05
    mov CL, strict byte 005h                  ; b1 05                       ; 0xc2d07 vgabios.c:1907
    mov di, bx                                ; 89 df                       ; 0xc2d09
    sal di, CL                                ; d3 e7                       ; 0xc2d0b
    add di, word [bp-00ch]                    ; 03 7e f4                    ; 0xc2d0d
    add si, 0556ch                            ; 81 c6 6c 55                 ; 0xc2d10 vgabios.c:1908
    mov cx, strict word 00008h                ; b9 08 00                    ; 0xc2d14
    mov dx, 0c000h                            ; ba 00 c0                    ; 0xc2d17
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2d1a
    mov es, ax                                ; 8e c0                       ; 0xc2d1d
    jcxz 02d27h                               ; e3 06                       ; 0xc2d1f
    push DS                                   ; 1e                          ; 0xc2d21
    mov ds, dx                                ; 8e da                       ; 0xc2d22
    rep movsb                                 ; f3 a4                       ; 0xc2d24
    pop DS                                    ; 1f                          ; 0xc2d26
    inc bx                                    ; 43                          ; 0xc2d27 vgabios.c:1909
    jmp short 02cfbh                          ; eb d1                       ; 0xc2d28
    call 02ae4h                               ; e8 b7 fd                    ; 0xc2d2a vgabios.c:1910
    cmp byte [bp-00ah], 010h                  ; 80 7e f6 10                 ; 0xc2d2d vgabios.c:1911
    jc short 02d39h                           ; 72 06                       ; 0xc2d31
    mov ax, strict word 00008h                ; b8 08 00                    ; 0xc2d33 vgabios.c:1913
    call 02b23h                               ; e8 ea fd                    ; 0xc2d36
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc2d39 vgabios.c:1915
    pop di                                    ; 5f                          ; 0xc2d3c
    pop si                                    ; 5e                          ; 0xc2d3d
    pop cx                                    ; 59                          ; 0xc2d3e
    pop bx                                    ; 5b                          ; 0xc2d3f
    pop bp                                    ; 5d                          ; 0xc2d40
    retn                                      ; c3                          ; 0xc2d41
  ; disGetNextSymbol 0xc2d42 LB 0x1552 -> off=0x0 cb=0000000000000073 uValue=00000000000c2d42 'biosfn_load_text_8_16_pat'
biosfn_load_text_8_16_pat:                   ; 0xc2d42 LB 0x73
    push bp                                   ; 55                          ; 0xc2d42 vgabios.c:1918
    mov bp, sp                                ; 89 e5                       ; 0xc2d43
    push bx                                   ; 53                          ; 0xc2d45
    push cx                                   ; 51                          ; 0xc2d46
    push si                                   ; 56                          ; 0xc2d47
    push di                                   ; 57                          ; 0xc2d48
    push ax                                   ; 50                          ; 0xc2d49
    push ax                                   ; 50                          ; 0xc2d4a
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc2d4b
    call 02ab8h                               ; e8 67 fd                    ; 0xc2d4e vgabios.c:1922
    mov al, dl                                ; 88 d0                       ; 0xc2d51 vgabios.c:1923
    and AL, strict byte 003h                  ; 24 03                       ; 0xc2d53
    xor ah, ah                                ; 30 e4                       ; 0xc2d55
    mov CL, strict byte 00eh                  ; b1 0e                       ; 0xc2d57
    mov bx, ax                                ; 89 c3                       ; 0xc2d59
    sal bx, CL                                ; d3 e3                       ; 0xc2d5b
    mov al, dl                                ; 88 d0                       ; 0xc2d5d
    and AL, strict byte 004h                  ; 24 04                       ; 0xc2d5f
    mov CL, strict byte 00bh                  ; b1 0b                       ; 0xc2d61
    sal ax, CL                                ; d3 e0                       ; 0xc2d63
    add bx, ax                                ; 01 c3                       ; 0xc2d65
    mov word [bp-00ch], bx                    ; 89 5e f4                    ; 0xc2d67
    xor bx, bx                                ; 31 db                       ; 0xc2d6a vgabios.c:1924
    jmp short 02d74h                          ; eb 06                       ; 0xc2d6c
    cmp bx, 00100h                            ; 81 fb 00 01                 ; 0xc2d6e
    jnc short 02d9dh                          ; 73 29                       ; 0xc2d72
    mov CL, strict byte 004h                  ; b1 04                       ; 0xc2d74 vgabios.c:1926
    mov si, bx                                ; 89 de                       ; 0xc2d76
    sal si, CL                                ; d3 e6                       ; 0xc2d78
    mov CL, strict byte 005h                  ; b1 05                       ; 0xc2d7a vgabios.c:1927
    mov di, bx                                ; 89 df                       ; 0xc2d7c
    sal di, CL                                ; d3 e7                       ; 0xc2d7e
    add di, word [bp-00ch]                    ; 03 7e f4                    ; 0xc2d80
    add si, 06b6ch                            ; 81 c6 6c 6b                 ; 0xc2d83 vgabios.c:1928
    mov cx, strict word 00010h                ; b9 10 00                    ; 0xc2d87
    mov dx, 0c000h                            ; ba 00 c0                    ; 0xc2d8a
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2d8d
    mov es, ax                                ; 8e c0                       ; 0xc2d90
    jcxz 02d9ah                               ; e3 06                       ; 0xc2d92
    push DS                                   ; 1e                          ; 0xc2d94
    mov ds, dx                                ; 8e da                       ; 0xc2d95
    rep movsb                                 ; f3 a4                       ; 0xc2d97
    pop DS                                    ; 1f                          ; 0xc2d99
    inc bx                                    ; 43                          ; 0xc2d9a vgabios.c:1929
    jmp short 02d6eh                          ; eb d1                       ; 0xc2d9b
    call 02ae4h                               ; e8 44 fd                    ; 0xc2d9d vgabios.c:1930
    cmp byte [bp-00ah], 010h                  ; 80 7e f6 10                 ; 0xc2da0 vgabios.c:1931
    jc short 02dach                           ; 72 06                       ; 0xc2da4
    mov ax, strict word 00010h                ; b8 10 00                    ; 0xc2da6 vgabios.c:1933
    call 02b23h                               ; e8 77 fd                    ; 0xc2da9
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc2dac vgabios.c:1935
    pop di                                    ; 5f                          ; 0xc2daf
    pop si                                    ; 5e                          ; 0xc2db0
    pop cx                                    ; 59                          ; 0xc2db1
    pop bx                                    ; 5b                          ; 0xc2db2
    pop bp                                    ; 5d                          ; 0xc2db3
    retn                                      ; c3                          ; 0xc2db4
  ; disGetNextSymbol 0xc2db5 LB 0x14df -> off=0x0 cb=0000000000000005 uValue=00000000000c2db5 'biosfn_load_gfx_8_8_chars'
biosfn_load_gfx_8_8_chars:                   ; 0xc2db5 LB 0x5
    push bp                                   ; 55                          ; 0xc2db5 vgabios.c:1937
    mov bp, sp                                ; 89 e5                       ; 0xc2db6
    pop bp                                    ; 5d                          ; 0xc2db8 vgabios.c:1942
    retn                                      ; c3                          ; 0xc2db9
  ; disGetNextSymbol 0xc2dba LB 0x14da -> off=0x0 cb=0000000000000007 uValue=00000000000c2dba 'biosfn_load_gfx_user_chars'
biosfn_load_gfx_user_chars:                  ; 0xc2dba LB 0x7
    push bp                                   ; 55                          ; 0xc2dba vgabios.c:1943
    mov bp, sp                                ; 89 e5                       ; 0xc2dbb
    pop bp                                    ; 5d                          ; 0xc2dbd vgabios.c:1949
    retn 00002h                               ; c2 02 00                    ; 0xc2dbe
  ; disGetNextSymbol 0xc2dc1 LB 0x14d3 -> off=0x0 cb=0000000000000005 uValue=00000000000c2dc1 'biosfn_load_gfx_8_14_chars'
biosfn_load_gfx_8_14_chars:                  ; 0xc2dc1 LB 0x5
    push bp                                   ; 55                          ; 0xc2dc1 vgabios.c:1950
    mov bp, sp                                ; 89 e5                       ; 0xc2dc2
    pop bp                                    ; 5d                          ; 0xc2dc4 vgabios.c:1955
    retn                                      ; c3                          ; 0xc2dc5
  ; disGetNextSymbol 0xc2dc6 LB 0x14ce -> off=0x0 cb=0000000000000005 uValue=00000000000c2dc6 'biosfn_load_gfx_8_8_dd_chars'
biosfn_load_gfx_8_8_dd_chars:                ; 0xc2dc6 LB 0x5
    push bp                                   ; 55                          ; 0xc2dc6 vgabios.c:1956
    mov bp, sp                                ; 89 e5                       ; 0xc2dc7
    pop bp                                    ; 5d                          ; 0xc2dc9 vgabios.c:1961
    retn                                      ; c3                          ; 0xc2dca
  ; disGetNextSymbol 0xc2dcb LB 0x14c9 -> off=0x0 cb=0000000000000005 uValue=00000000000c2dcb 'biosfn_load_gfx_8_16_chars'
biosfn_load_gfx_8_16_chars:                  ; 0xc2dcb LB 0x5
    push bp                                   ; 55                          ; 0xc2dcb vgabios.c:1962
    mov bp, sp                                ; 89 e5                       ; 0xc2dcc
    pop bp                                    ; 5d                          ; 0xc2dce vgabios.c:1967
    retn                                      ; c3                          ; 0xc2dcf
  ; disGetNextSymbol 0xc2dd0 LB 0x14c4 -> off=0x0 cb=0000000000000005 uValue=00000000000c2dd0 'biosfn_alternate_prtsc'
biosfn_alternate_prtsc:                      ; 0xc2dd0 LB 0x5
    push bp                                   ; 55                          ; 0xc2dd0 vgabios.c:1969
    mov bp, sp                                ; 89 e5                       ; 0xc2dd1
    pop bp                                    ; 5d                          ; 0xc2dd3 vgabios.c:1974
    retn                                      ; c3                          ; 0xc2dd4
  ; disGetNextSymbol 0xc2dd5 LB 0x14bf -> off=0x0 cb=0000000000000005 uValue=00000000000c2dd5 'biosfn_switch_video_interface'
biosfn_switch_video_interface:               ; 0xc2dd5 LB 0x5
    push bp                                   ; 55                          ; 0xc2dd5 vgabios.c:1977
    mov bp, sp                                ; 89 e5                       ; 0xc2dd6
    pop bp                                    ; 5d                          ; 0xc2dd8 vgabios.c:1982
    retn                                      ; c3                          ; 0xc2dd9
  ; disGetNextSymbol 0xc2dda LB 0x14ba -> off=0x0 cb=0000000000000005 uValue=00000000000c2dda 'biosfn_enable_video_refresh_control'
biosfn_enable_video_refresh_control:         ; 0xc2dda LB 0x5
    push bp                                   ; 55                          ; 0xc2dda vgabios.c:1983
    mov bp, sp                                ; 89 e5                       ; 0xc2ddb
    pop bp                                    ; 5d                          ; 0xc2ddd vgabios.c:1988
    retn                                      ; c3                          ; 0xc2dde
  ; disGetNextSymbol 0xc2ddf LB 0x14b5 -> off=0x0 cb=000000000000008f uValue=00000000000c2ddf 'biosfn_write_string'
biosfn_write_string:                         ; 0xc2ddf LB 0x8f
    push bp                                   ; 55                          ; 0xc2ddf vgabios.c:1991
    mov bp, sp                                ; 89 e5                       ; 0xc2de0
    push si                                   ; 56                          ; 0xc2de2
    push di                                   ; 57                          ; 0xc2de3
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc2de4
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc2de7
    mov byte [bp-006h], dl                    ; 88 56 fa                    ; 0xc2dea
    mov byte [bp-00ah], bl                    ; 88 5e f6                    ; 0xc2ded
    mov si, cx                                ; 89 ce                       ; 0xc2df0
    mov di, word [bp+00ah]                    ; 8b 7e 0a                    ; 0xc2df2
    mov al, dl                                ; 88 d0                       ; 0xc2df5 vgabios.c:1998
    xor ah, ah                                ; 30 e4                       ; 0xc2df7
    lea bx, [bp-00eh]                         ; 8d 5e f2                    ; 0xc2df9
    lea dx, [bp-00ch]                         ; 8d 56 f4                    ; 0xc2dfc
    call 00a1bh                               ; e8 19 dc                    ; 0xc2dff
    cmp byte [bp+004h], 0ffh                  ; 80 7e 04 ff                 ; 0xc2e02 vgabios.c:2001
    jne short 02e14h                          ; 75 0c                       ; 0xc2e06
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc2e08 vgabios.c:2002
    mov byte [bp+006h], al                    ; 88 46 06                    ; 0xc2e0b
    mov ax, word [bp-00eh]                    ; 8b 46 f2                    ; 0xc2e0e vgabios.c:2003
    mov byte [bp+004h], ah                    ; 88 66 04                    ; 0xc2e11
    mov dh, byte [bp+004h]                    ; 8a 76 04                    ; 0xc2e14 vgabios.c:2006
    mov dl, byte [bp+006h]                    ; 8a 56 06                    ; 0xc2e17
    xor ah, ah                                ; 30 e4                       ; 0xc2e1a
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc2e1c vgabios.c:2007
    call 01253h                               ; e8 31 e4                    ; 0xc2e1f
    dec si                                    ; 4e                          ; 0xc2e22 vgabios.c:2009
    cmp si, strict byte 0ffffh                ; 83 fe ff                    ; 0xc2e23
    je short 02e54h                           ; 74 2c                       ; 0xc2e26
    mov bx, di                                ; 89 fb                       ; 0xc2e28 vgabios.c:2011
    inc di                                    ; 47                          ; 0xc2e2a
    mov es, [bp+008h]                         ; 8e 46 08                    ; 0xc2e2b vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc2e2e
    test byte [bp-008h], 002h                 ; f6 46 f8 02                 ; 0xc2e31 vgabios.c:2012
    je short 02e40h                           ; 74 09                       ; 0xc2e35
    mov bx, di                                ; 89 fb                       ; 0xc2e37 vgabios.c:2013
    inc di                                    ; 47                          ; 0xc2e39
    mov ah, byte [es:bx]                      ; 26 8a 27                    ; 0xc2e3a vgabios.c:37
    mov byte [bp-00ah], ah                    ; 88 66 f6                    ; 0xc2e3d vgabios.c:38
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc2e40 vgabios.c:2015
    xor bh, bh                                ; 30 ff                       ; 0xc2e43
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc2e45
    xor dh, dh                                ; 30 f6                       ; 0xc2e48
    xor ah, ah                                ; 30 e4                       ; 0xc2e4a
    mov cx, strict word 00003h                ; b9 03 00                    ; 0xc2e4c
    call 02855h                               ; e8 03 fa                    ; 0xc2e4f
    jmp short 02e22h                          ; eb ce                       ; 0xc2e52 vgabios.c:2016
    test byte [bp-008h], 001h                 ; f6 46 f8 01                 ; 0xc2e54 vgabios.c:2019
    jne short 02e65h                          ; 75 0b                       ; 0xc2e58
    mov dx, word [bp-00eh]                    ; 8b 56 f2                    ; 0xc2e5a vgabios.c:2020
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc2e5d
    xor ah, ah                                ; 30 e4                       ; 0xc2e60
    call 01253h                               ; e8 ee e3                    ; 0xc2e62
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc2e65 vgabios.c:2021
    pop di                                    ; 5f                          ; 0xc2e68
    pop si                                    ; 5e                          ; 0xc2e69
    pop bp                                    ; 5d                          ; 0xc2e6a
    retn 00008h                               ; c2 08 00                    ; 0xc2e6b
  ; disGetNextSymbol 0xc2e6e LB 0x1426 -> off=0x0 cb=00000000000001f2 uValue=00000000000c2e6e 'biosfn_read_state_info'
biosfn_read_state_info:                      ; 0xc2e6e LB 0x1f2
    push bp                                   ; 55                          ; 0xc2e6e vgabios.c:2024
    mov bp, sp                                ; 89 e5                       ; 0xc2e6f
    push cx                                   ; 51                          ; 0xc2e71
    push si                                   ; 56                          ; 0xc2e72
    push di                                   ; 57                          ; 0xc2e73
    push ax                                   ; 50                          ; 0xc2e74
    push ax                                   ; 50                          ; 0xc2e75
    push dx                                   ; 52                          ; 0xc2e76
    mov si, strict word 00049h                ; be 49 00                    ; 0xc2e77 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2e7a
    mov es, ax                                ; 8e c0                       ; 0xc2e7d
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc2e7f
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc2e82 vgabios.c:38
    mov si, strict word 0004ch                ; be 4c 00                    ; 0xc2e85 vgabios.c:47
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc2e88
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc2e8b vgabios.c:48
    mov ax, ds                                ; 8c d8                       ; 0xc2e8e vgabios.c:2035
    mov es, dx                                ; 8e c2                       ; 0xc2e90 vgabios.c:62
    mov word [es:bx], 05502h                  ; 26 c7 07 02 55              ; 0xc2e92
    mov [es:bx+002h], ds                      ; 26 8c 5f 02                 ; 0xc2e97
    lea di, [bx+004h]                         ; 8d 7f 04                    ; 0xc2e9b vgabios.c:2040
    mov cx, strict word 0001eh                ; b9 1e 00                    ; 0xc2e9e
    mov si, strict word 00049h                ; be 49 00                    ; 0xc2ea1
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc2ea4
    jcxz 02eafh                               ; e3 06                       ; 0xc2ea7
    push DS                                   ; 1e                          ; 0xc2ea9
    mov ds, dx                                ; 8e da                       ; 0xc2eaa
    rep movsb                                 ; f3 a4                       ; 0xc2eac
    pop DS                                    ; 1f                          ; 0xc2eae
    mov si, 00084h                            ; be 84 00                    ; 0xc2eaf vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2eb2
    mov es, ax                                ; 8e c0                       ; 0xc2eb5
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc2eb7
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc2eba vgabios.c:38
    lea si, [bx+022h]                         ; 8d 77 22                    ; 0xc2ebc
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2ebf vgabios.c:42
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc2ec2
    lea di, [bx+023h]                         ; 8d 7f 23                    ; 0xc2ec5 vgabios.c:2042
    mov cx, strict word 00002h                ; b9 02 00                    ; 0xc2ec8
    mov si, 00085h                            ; be 85 00                    ; 0xc2ecb
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc2ece
    jcxz 02ed9h                               ; e3 06                       ; 0xc2ed1
    push DS                                   ; 1e                          ; 0xc2ed3
    mov ds, dx                                ; 8e da                       ; 0xc2ed4
    rep movsb                                 ; f3 a4                       ; 0xc2ed6
    pop DS                                    ; 1f                          ; 0xc2ed8
    mov si, 0008ah                            ; be 8a 00                    ; 0xc2ed9 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2edc
    mov es, ax                                ; 8e c0                       ; 0xc2edf
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc2ee1
    lea si, [bx+025h]                         ; 8d 77 25                    ; 0xc2ee4 vgabios.c:38
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2ee7 vgabios.c:42
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc2eea
    lea si, [bx+026h]                         ; 8d 77 26                    ; 0xc2eed vgabios.c:2045
    mov byte [es:si], 000h                    ; 26 c6 04 00                 ; 0xc2ef0 vgabios.c:42
    lea si, [bx+027h]                         ; 8d 77 27                    ; 0xc2ef4 vgabios.c:2046
    mov word [es:si], strict word 00010h      ; 26 c7 04 10 00              ; 0xc2ef7 vgabios.c:52
    lea si, [bx+029h]                         ; 8d 77 29                    ; 0xc2efc vgabios.c:2047
    mov byte [es:si], 008h                    ; 26 c6 04 08                 ; 0xc2eff vgabios.c:42
    lea si, [bx+02ah]                         ; 8d 77 2a                    ; 0xc2f03 vgabios.c:2048
    mov byte [es:si], 002h                    ; 26 c6 04 02                 ; 0xc2f06 vgabios.c:42
    lea si, [bx+02bh]                         ; 8d 77 2b                    ; 0xc2f0a vgabios.c:2049
    mov byte [es:si], 000h                    ; 26 c6 04 00                 ; 0xc2f0d vgabios.c:42
    lea si, [bx+02ch]                         ; 8d 77 2c                    ; 0xc2f11 vgabios.c:2050
    mov byte [es:si], 000h                    ; 26 c6 04 00                 ; 0xc2f14 vgabios.c:42
    lea si, [bx+02dh]                         ; 8d 77 2d                    ; 0xc2f18 vgabios.c:2051
    mov byte [es:si], 021h                    ; 26 c6 04 21                 ; 0xc2f1b vgabios.c:42
    lea si, [bx+031h]                         ; 8d 77 31                    ; 0xc2f1f vgabios.c:2052
    mov byte [es:si], 003h                    ; 26 c6 04 03                 ; 0xc2f22 vgabios.c:42
    lea si, [bx+032h]                         ; 8d 77 32                    ; 0xc2f26 vgabios.c:2053
    mov byte [es:si], 000h                    ; 26 c6 04 00                 ; 0xc2f29 vgabios.c:42
    mov si, 00089h                            ; be 89 00                    ; 0xc2f2d vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2f30
    mov es, ax                                ; 8e c0                       ; 0xc2f33
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc2f35
    mov dl, al                                ; 88 c2                       ; 0xc2f38 vgabios.c:2058
    and dl, 080h                              ; 80 e2 80                    ; 0xc2f3a
    xor dh, dh                                ; 30 f6                       ; 0xc2f3d
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc2f3f
    sar dx, CL                                ; d3 fa                       ; 0xc2f41
    and AL, strict byte 010h                  ; 24 10                       ; 0xc2f43
    xor ah, ah                                ; 30 e4                       ; 0xc2f45
    mov CL, strict byte 004h                  ; b1 04                       ; 0xc2f47
    sar ax, CL                                ; d3 f8                       ; 0xc2f49
    or ax, dx                                 ; 09 d0                       ; 0xc2f4b
    cmp ax, strict word 00002h                ; 3d 02 00                    ; 0xc2f4d vgabios.c:2059
    je short 02f63h                           ; 74 11                       ; 0xc2f50
    cmp ax, strict word 00001h                ; 3d 01 00                    ; 0xc2f52
    je short 02f5fh                           ; 74 08                       ; 0xc2f55
    test ax, ax                               ; 85 c0                       ; 0xc2f57
    jne short 02f63h                          ; 75 08                       ; 0xc2f59
    mov AL, strict byte 001h                  ; b0 01                       ; 0xc2f5b vgabios.c:2060
    jmp short 02f65h                          ; eb 06                       ; 0xc2f5d
    mov AL, strict byte 002h                  ; b0 02                       ; 0xc2f5f vgabios.c:2061
    jmp short 02f65h                          ; eb 02                       ; 0xc2f61
    xor al, al                                ; 30 c0                       ; 0xc2f63 vgabios.c:2063
    lea si, [bx+02ah]                         ; 8d 77 2a                    ; 0xc2f65 vgabios.c:2065
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2f68 vgabios.c:42
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc2f6b
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2f6e vgabios.c:2068
    cmp AL, strict byte 00eh                  ; 3c 0e                       ; 0xc2f71
    jc short 02f95h                           ; 72 20                       ; 0xc2f73
    cmp AL, strict byte 012h                  ; 3c 12                       ; 0xc2f75
    jnbe short 02f95h                         ; 77 1c                       ; 0xc2f77
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc2f79 vgabios.c:2069
    test ax, ax                               ; 85 c0                       ; 0xc2f7c
    je short 02fd7h                           ; 74 57                       ; 0xc2f7e
    mov si, ax                                ; 89 c6                       ; 0xc2f80 vgabios.c:2070
    shr si, 1                                 ; d1 ee                       ; 0xc2f82
    shr si, 1                                 ; d1 ee                       ; 0xc2f84
    mov ax, 04000h                            ; b8 00 40                    ; 0xc2f86
    xor dx, dx                                ; 31 d2                       ; 0xc2f89
    div si                                    ; f7 f6                       ; 0xc2f8b
    lea si, [bx+029h]                         ; 8d 77 29                    ; 0xc2f8d
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc2f90 vgabios.c:42
    jmp short 02fd7h                          ; eb 42                       ; 0xc2f93 vgabios.c:2071
    lea si, [bx+029h]                         ; 8d 77 29                    ; 0xc2f95
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2f98
    cmp AL, strict byte 013h                  ; 3c 13                       ; 0xc2f9b
    jne short 02fb0h                          ; 75 11                       ; 0xc2f9d
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2f9f vgabios.c:42
    mov byte [es:si], 001h                    ; 26 c6 04 01                 ; 0xc2fa2
    lea si, [bx+027h]                         ; 8d 77 27                    ; 0xc2fa6 vgabios.c:2073
    mov word [es:si], 00100h                  ; 26 c7 04 00 01              ; 0xc2fa9 vgabios.c:52
    jmp short 02fd7h                          ; eb 27                       ; 0xc2fae vgabios.c:2074
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc2fb0
    jc short 02fd7h                           ; 72 23                       ; 0xc2fb2
    cmp AL, strict byte 006h                  ; 3c 06                       ; 0xc2fb4
    jnbe short 02fd7h                         ; 77 1f                       ; 0xc2fb6
    cmp word [bp-00ah], strict byte 00000h    ; 83 7e f6 00                 ; 0xc2fb8 vgabios.c:2076
    je short 02fcch                           ; 74 0e                       ; 0xc2fbc
    mov ax, 04000h                            ; b8 00 40                    ; 0xc2fbe vgabios.c:2077
    xor dx, dx                                ; 31 d2                       ; 0xc2fc1
    div word [bp-00ah]                        ; f7 76 f6                    ; 0xc2fc3
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2fc6 vgabios.c:42
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc2fc9
    lea si, [bx+027h]                         ; 8d 77 27                    ; 0xc2fcc vgabios.c:2078
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2fcf vgabios.c:52
    mov word [es:si], strict word 00004h      ; 26 c7 04 04 00              ; 0xc2fd2
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2fd7 vgabios.c:2080
    cmp AL, strict byte 006h                  ; 3c 06                       ; 0xc2fda
    je short 02fe2h                           ; 74 04                       ; 0xc2fdc
    cmp AL, strict byte 011h                  ; 3c 11                       ; 0xc2fde
    jne short 02fedh                          ; 75 0b                       ; 0xc2fe0
    lea si, [bx+027h]                         ; 8d 77 27                    ; 0xc2fe2 vgabios.c:2081
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2fe5 vgabios.c:52
    mov word [es:si], strict word 00002h      ; 26 c7 04 02 00              ; 0xc2fe8
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2fed vgabios.c:2083
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc2ff0
    jc short 03049h                           ; 72 55                       ; 0xc2ff2
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc2ff4
    je short 03049h                           ; 74 51                       ; 0xc2ff6
    lea si, [bx+02dh]                         ; 8d 77 2d                    ; 0xc2ff8 vgabios.c:2084
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2ffb vgabios.c:42
    mov byte [es:si], 001h                    ; 26 c6 04 01                 ; 0xc2ffe
    mov si, 00084h                            ; be 84 00                    ; 0xc3002 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3005
    mov es, ax                                ; 8e c0                       ; 0xc3008
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc300a
    xor ah, ah                                ; 30 e4                       ; 0xc300d vgabios.c:38
    inc ax                                    ; 40                          ; 0xc300f
    mov si, 00085h                            ; be 85 00                    ; 0xc3010 vgabios.c:37
    mov dl, byte [es:si]                      ; 26 8a 14                    ; 0xc3013
    xor dh, dh                                ; 30 f6                       ; 0xc3016 vgabios.c:38
    imul dx                                   ; f7 ea                       ; 0xc3018
    cmp ax, 0015eh                            ; 3d 5e 01                    ; 0xc301a vgabios.c:2086
    jc short 0302dh                           ; 72 0e                       ; 0xc301d
    jbe short 03036h                          ; 76 15                       ; 0xc301f
    cmp ax, 001e0h                            ; 3d e0 01                    ; 0xc3021
    je short 0303eh                           ; 74 18                       ; 0xc3024
    cmp ax, 00190h                            ; 3d 90 01                    ; 0xc3026
    je short 0303ah                           ; 74 0f                       ; 0xc3029
    jmp short 0303eh                          ; eb 11                       ; 0xc302b
    cmp ax, 000c8h                            ; 3d c8 00                    ; 0xc302d
    jne short 0303eh                          ; 75 0c                       ; 0xc3030
    xor al, al                                ; 30 c0                       ; 0xc3032 vgabios.c:2087
    jmp short 03040h                          ; eb 0a                       ; 0xc3034
    mov AL, strict byte 001h                  ; b0 01                       ; 0xc3036 vgabios.c:2088
    jmp short 03040h                          ; eb 06                       ; 0xc3038
    mov AL, strict byte 002h                  ; b0 02                       ; 0xc303a vgabios.c:2089
    jmp short 03040h                          ; eb 02                       ; 0xc303c
    mov AL, strict byte 003h                  ; b0 03                       ; 0xc303e vgabios.c:2091
    lea si, [bx+02ah]                         ; 8d 77 2a                    ; 0xc3040 vgabios.c:2093
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc3043 vgabios.c:42
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc3046
    lea di, [bx+033h]                         ; 8d 7f 33                    ; 0xc3049 vgabios.c:2096
    mov cx, strict word 0000dh                ; b9 0d 00                    ; 0xc304c
    xor ax, ax                                ; 31 c0                       ; 0xc304f
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc3051
    jcxz 03058h                               ; e3 02                       ; 0xc3054
    rep stosb                                 ; f3 aa                       ; 0xc3056
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc3058 vgabios.c:2097
    pop di                                    ; 5f                          ; 0xc305b
    pop si                                    ; 5e                          ; 0xc305c
    pop cx                                    ; 59                          ; 0xc305d
    pop bp                                    ; 5d                          ; 0xc305e
    retn                                      ; c3                          ; 0xc305f
  ; disGetNextSymbol 0xc3060 LB 0x1234 -> off=0x0 cb=0000000000000023 uValue=00000000000c3060 'biosfn_read_video_state_size2'
biosfn_read_video_state_size2:               ; 0xc3060 LB 0x23
    push dx                                   ; 52                          ; 0xc3060 vgabios.c:2100
    push bp                                   ; 55                          ; 0xc3061
    mov bp, sp                                ; 89 e5                       ; 0xc3062
    mov dx, ax                                ; 89 c2                       ; 0xc3064
    xor ax, ax                                ; 31 c0                       ; 0xc3066 vgabios.c:2104
    test dl, 001h                             ; f6 c2 01                    ; 0xc3068 vgabios.c:2105
    je short 03070h                           ; 74 03                       ; 0xc306b
    mov ax, strict word 00046h                ; b8 46 00                    ; 0xc306d vgabios.c:2106
    test dl, 002h                             ; f6 c2 02                    ; 0xc3070 vgabios.c:2108
    je short 03078h                           ; 74 03                       ; 0xc3073
    add ax, strict word 0002ah                ; 05 2a 00                    ; 0xc3075 vgabios.c:2109
    test dl, 004h                             ; f6 c2 04                    ; 0xc3078 vgabios.c:2111
    je short 03080h                           ; 74 03                       ; 0xc307b
    add ax, 00304h                            ; 05 04 03                    ; 0xc307d vgabios.c:2112
    pop bp                                    ; 5d                          ; 0xc3080 vgabios.c:2115
    pop dx                                    ; 5a                          ; 0xc3081
    retn                                      ; c3                          ; 0xc3082
  ; disGetNextSymbol 0xc3083 LB 0x1211 -> off=0x0 cb=000000000000001b uValue=00000000000c3083 'vga_get_video_state_size'
vga_get_video_state_size:                    ; 0xc3083 LB 0x1b
    push bp                                   ; 55                          ; 0xc3083 vgabios.c:2117
    mov bp, sp                                ; 89 e5                       ; 0xc3084
    push bx                                   ; 53                          ; 0xc3086
    push cx                                   ; 51                          ; 0xc3087
    mov bx, dx                                ; 89 d3                       ; 0xc3088
    call 03060h                               ; e8 d3 ff                    ; 0xc308a vgabios.c:2120
    add ax, strict word 0003fh                ; 05 3f 00                    ; 0xc308d
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc3090
    shr ax, CL                                ; d3 e8                       ; 0xc3092
    mov word [ss:bx], ax                      ; 36 89 07                    ; 0xc3094
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3097 vgabios.c:2121
    pop cx                                    ; 59                          ; 0xc309a
    pop bx                                    ; 5b                          ; 0xc309b
    pop bp                                    ; 5d                          ; 0xc309c
    retn                                      ; c3                          ; 0xc309d
  ; disGetNextSymbol 0xc309e LB 0x11f6 -> off=0x0 cb=00000000000002d8 uValue=00000000000c309e 'biosfn_save_video_state'
biosfn_save_video_state:                     ; 0xc309e LB 0x2d8
    push bp                                   ; 55                          ; 0xc309e vgabios.c:2123
    mov bp, sp                                ; 89 e5                       ; 0xc309f
    push cx                                   ; 51                          ; 0xc30a1
    push si                                   ; 56                          ; 0xc30a2
    push di                                   ; 57                          ; 0xc30a3
    push ax                                   ; 50                          ; 0xc30a4
    push ax                                   ; 50                          ; 0xc30a5
    push ax                                   ; 50                          ; 0xc30a6
    mov cx, dx                                ; 89 d1                       ; 0xc30a7
    mov si, strict word 00063h                ; be 63 00                    ; 0xc30a9 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc30ac
    mov es, ax                                ; 8e c0                       ; 0xc30af
    mov di, word [es:si]                      ; 26 8b 3c                    ; 0xc30b1
    mov si, di                                ; 89 fe                       ; 0xc30b4 vgabios.c:48
    test byte [bp-00ch], 001h                 ; f6 46 f4 01                 ; 0xc30b6 vgabios.c:2128
    je short 03122h                           ; 74 66                       ; 0xc30ba
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc30bc vgabios.c:2129
    in AL, DX                                 ; ec                          ; 0xc30bf
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc30c0
    mov es, cx                                ; 8e c1                       ; 0xc30c2 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc30c4
    inc bx                                    ; 43                          ; 0xc30c7 vgabios.c:2129
    mov dx, di                                ; 89 fa                       ; 0xc30c8
    in AL, DX                                 ; ec                          ; 0xc30ca
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc30cb
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc30cd vgabios.c:42
    inc bx                                    ; 43                          ; 0xc30d0 vgabios.c:2130
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc30d1
    in AL, DX                                 ; ec                          ; 0xc30d4
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc30d5
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc30d7 vgabios.c:42
    inc bx                                    ; 43                          ; 0xc30da vgabios.c:2131
    mov dx, 003dah                            ; ba da 03                    ; 0xc30db
    in AL, DX                                 ; ec                          ; 0xc30de
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc30df
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc30e1 vgabios.c:2133
    in AL, DX                                 ; ec                          ; 0xc30e4
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc30e5
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc30e7
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc30ea vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc30ed
    inc bx                                    ; 43                          ; 0xc30f0 vgabios.c:2134
    mov dx, 003cah                            ; ba ca 03                    ; 0xc30f1
    in AL, DX                                 ; ec                          ; 0xc30f4
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc30f5
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc30f7 vgabios.c:42
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc30fa vgabios.c:2137
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc30fd
    add bx, ax                                ; 01 c3                       ; 0xc3100 vgabios.c:2135
    jmp short 0310ah                          ; eb 06                       ; 0xc3102
    cmp word [bp-008h], strict byte 00004h    ; 83 7e f8 04                 ; 0xc3104
    jnbe short 03125h                         ; 77 1b                       ; 0xc3108
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc310a vgabios.c:2138
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc310d
    out DX, AL                                ; ee                          ; 0xc3110
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc3111 vgabios.c:2139
    in AL, DX                                 ; ec                          ; 0xc3114
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3115
    mov es, cx                                ; 8e c1                       ; 0xc3117 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3119
    inc bx                                    ; 43                          ; 0xc311c vgabios.c:2139
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc311d vgabios.c:2140
    jmp short 03104h                          ; eb e2                       ; 0xc3120
    jmp near 031d2h                           ; e9 ad 00                    ; 0xc3122
    xor al, al                                ; 30 c0                       ; 0xc3125 vgabios.c:2141
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc3127
    out DX, AL                                ; ee                          ; 0xc312a
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc312b vgabios.c:2142
    in AL, DX                                 ; ec                          ; 0xc312e
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc312f
    mov es, cx                                ; 8e c1                       ; 0xc3131 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3133
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc3136 vgabios.c:2144
    inc bx                                    ; 43                          ; 0xc313b vgabios.c:2142
    jmp short 03144h                          ; eb 06                       ; 0xc313c
    cmp word [bp-008h], strict byte 00018h    ; 83 7e f8 18                 ; 0xc313e
    jnbe short 0315bh                         ; 77 17                       ; 0xc3142
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc3144 vgabios.c:2145
    mov dx, si                                ; 89 f2                       ; 0xc3147
    out DX, AL                                ; ee                          ; 0xc3149
    lea dx, [si+001h]                         ; 8d 54 01                    ; 0xc314a vgabios.c:2146
    in AL, DX                                 ; ec                          ; 0xc314d
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc314e
    mov es, cx                                ; 8e c1                       ; 0xc3150 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3152
    inc bx                                    ; 43                          ; 0xc3155 vgabios.c:2146
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc3156 vgabios.c:2147
    jmp short 0313eh                          ; eb e3                       ; 0xc3159
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc315b vgabios.c:2149
    jmp short 03168h                          ; eb 06                       ; 0xc3160
    cmp word [bp-008h], strict byte 00013h    ; 83 7e f8 13                 ; 0xc3162
    jnbe short 0318ch                         ; 77 24                       ; 0xc3166
    mov dx, 003dah                            ; ba da 03                    ; 0xc3168 vgabios.c:2150
    in AL, DX                                 ; ec                          ; 0xc316b
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc316c
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc316e vgabios.c:2151
    and ax, strict word 00020h                ; 25 20 00                    ; 0xc3171
    or ax, word [bp-008h]                     ; 0b 46 f8                    ; 0xc3174
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc3177
    out DX, AL                                ; ee                          ; 0xc317a
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc317b vgabios.c:2152
    in AL, DX                                 ; ec                          ; 0xc317e
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc317f
    mov es, cx                                ; 8e c1                       ; 0xc3181 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3183
    inc bx                                    ; 43                          ; 0xc3186 vgabios.c:2152
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc3187 vgabios.c:2153
    jmp short 03162h                          ; eb d6                       ; 0xc318a
    mov dx, 003dah                            ; ba da 03                    ; 0xc318c vgabios.c:2154
    in AL, DX                                 ; ec                          ; 0xc318f
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3190
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc3192 vgabios.c:2156
    jmp short 0319fh                          ; eb 06                       ; 0xc3197
    cmp word [bp-008h], strict byte 00008h    ; 83 7e f8 08                 ; 0xc3199
    jnbe short 031b7h                         ; 77 18                       ; 0xc319d
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc319f vgabios.c:2157
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc31a2
    out DX, AL                                ; ee                          ; 0xc31a5
    mov dx, 003cfh                            ; ba cf 03                    ; 0xc31a6 vgabios.c:2158
    in AL, DX                                 ; ec                          ; 0xc31a9
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc31aa
    mov es, cx                                ; 8e c1                       ; 0xc31ac vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc31ae
    inc bx                                    ; 43                          ; 0xc31b1 vgabios.c:2158
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc31b2 vgabios.c:2159
    jmp short 03199h                          ; eb e2                       ; 0xc31b5
    mov es, cx                                ; 8e c1                       ; 0xc31b7 vgabios.c:52
    mov word [es:bx], si                      ; 26 89 37                    ; 0xc31b9
    inc bx                                    ; 43                          ; 0xc31bc vgabios.c:2161
    inc bx                                    ; 43                          ; 0xc31bd
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc31be vgabios.c:42
    inc bx                                    ; 43                          ; 0xc31c2 vgabios.c:2164
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc31c3 vgabios.c:42
    inc bx                                    ; 43                          ; 0xc31c7 vgabios.c:2165
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc31c8 vgabios.c:42
    inc bx                                    ; 43                          ; 0xc31cc vgabios.c:2166
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc31cd vgabios.c:42
    inc bx                                    ; 43                          ; 0xc31d1 vgabios.c:2167
    test byte [bp-00ch], 002h                 ; f6 46 f4 02                 ; 0xc31d2 vgabios.c:2169
    jne short 031dbh                          ; 75 03                       ; 0xc31d6
    jmp near 0331ah                           ; e9 3f 01                    ; 0xc31d8
    mov si, strict word 00049h                ; be 49 00                    ; 0xc31db vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc31de
    mov es, ax                                ; 8e c0                       ; 0xc31e1
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc31e3
    mov es, cx                                ; 8e c1                       ; 0xc31e6 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc31e8
    inc bx                                    ; 43                          ; 0xc31eb vgabios.c:2170
    mov si, strict word 0004ah                ; be 4a 00                    ; 0xc31ec vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc31ef
    mov es, ax                                ; 8e c0                       ; 0xc31f2
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc31f4
    mov es, cx                                ; 8e c1                       ; 0xc31f7 vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc31f9
    inc bx                                    ; 43                          ; 0xc31fc vgabios.c:2171
    inc bx                                    ; 43                          ; 0xc31fd
    mov si, strict word 0004ch                ; be 4c 00                    ; 0xc31fe vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3201
    mov es, ax                                ; 8e c0                       ; 0xc3204
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3206
    mov es, cx                                ; 8e c1                       ; 0xc3209 vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc320b
    inc bx                                    ; 43                          ; 0xc320e vgabios.c:2172
    inc bx                                    ; 43                          ; 0xc320f
    mov si, strict word 00063h                ; be 63 00                    ; 0xc3210 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3213
    mov es, ax                                ; 8e c0                       ; 0xc3216
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3218
    mov es, cx                                ; 8e c1                       ; 0xc321b vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc321d
    inc bx                                    ; 43                          ; 0xc3220 vgabios.c:2173
    inc bx                                    ; 43                          ; 0xc3221
    mov si, 00084h                            ; be 84 00                    ; 0xc3222 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3225
    mov es, ax                                ; 8e c0                       ; 0xc3228
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc322a
    mov es, cx                                ; 8e c1                       ; 0xc322d vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc322f
    inc bx                                    ; 43                          ; 0xc3232 vgabios.c:2174
    mov si, 00085h                            ; be 85 00                    ; 0xc3233 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3236
    mov es, ax                                ; 8e c0                       ; 0xc3239
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc323b
    mov es, cx                                ; 8e c1                       ; 0xc323e vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3240
    inc bx                                    ; 43                          ; 0xc3243 vgabios.c:2175
    inc bx                                    ; 43                          ; 0xc3244
    mov si, 00087h                            ; be 87 00                    ; 0xc3245 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3248
    mov es, ax                                ; 8e c0                       ; 0xc324b
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc324d
    mov es, cx                                ; 8e c1                       ; 0xc3250 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3252
    inc bx                                    ; 43                          ; 0xc3255 vgabios.c:2176
    mov si, 00088h                            ; be 88 00                    ; 0xc3256 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3259
    mov es, ax                                ; 8e c0                       ; 0xc325c
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc325e
    mov es, cx                                ; 8e c1                       ; 0xc3261 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3263
    inc bx                                    ; 43                          ; 0xc3266 vgabios.c:2177
    mov si, 00089h                            ; be 89 00                    ; 0xc3267 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc326a
    mov es, ax                                ; 8e c0                       ; 0xc326d
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc326f
    mov es, cx                                ; 8e c1                       ; 0xc3272 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3274
    inc bx                                    ; 43                          ; 0xc3277 vgabios.c:2178
    mov si, strict word 00060h                ; be 60 00                    ; 0xc3278 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc327b
    mov es, ax                                ; 8e c0                       ; 0xc327e
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3280
    mov es, cx                                ; 8e c1                       ; 0xc3283 vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3285
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc3288 vgabios.c:2180
    inc bx                                    ; 43                          ; 0xc328d vgabios.c:2179
    inc bx                                    ; 43                          ; 0xc328e
    jmp short 03297h                          ; eb 06                       ; 0xc328f
    cmp word [bp-008h], strict byte 00008h    ; 83 7e f8 08                 ; 0xc3291
    jnc short 032b3h                          ; 73 1c                       ; 0xc3295
    mov si, word [bp-008h]                    ; 8b 76 f8                    ; 0xc3297 vgabios.c:2181
    sal si, 1                                 ; d1 e6                       ; 0xc329a
    add si, strict byte 00050h                ; 83 c6 50                    ; 0xc329c
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc329f vgabios.c:47
    mov es, ax                                ; 8e c0                       ; 0xc32a2
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc32a4
    mov es, cx                                ; 8e c1                       ; 0xc32a7 vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc32a9
    inc bx                                    ; 43                          ; 0xc32ac vgabios.c:2182
    inc bx                                    ; 43                          ; 0xc32ad
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc32ae vgabios.c:2183
    jmp short 03291h                          ; eb de                       ; 0xc32b1
    mov si, strict word 0004eh                ; be 4e 00                    ; 0xc32b3 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc32b6
    mov es, ax                                ; 8e c0                       ; 0xc32b9
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc32bb
    mov es, cx                                ; 8e c1                       ; 0xc32be vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc32c0
    inc bx                                    ; 43                          ; 0xc32c3 vgabios.c:2184
    inc bx                                    ; 43                          ; 0xc32c4
    mov si, strict word 00062h                ; be 62 00                    ; 0xc32c5 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc32c8
    mov es, ax                                ; 8e c0                       ; 0xc32cb
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc32cd
    mov es, cx                                ; 8e c1                       ; 0xc32d0 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc32d2
    inc bx                                    ; 43                          ; 0xc32d5 vgabios.c:2185
    mov si, strict word 0007ch                ; be 7c 00                    ; 0xc32d6 vgabios.c:47
    xor ax, ax                                ; 31 c0                       ; 0xc32d9
    mov es, ax                                ; 8e c0                       ; 0xc32db
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc32dd
    mov es, cx                                ; 8e c1                       ; 0xc32e0 vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc32e2
    inc bx                                    ; 43                          ; 0xc32e5 vgabios.c:2187
    inc bx                                    ; 43                          ; 0xc32e6
    mov si, strict word 0007eh                ; be 7e 00                    ; 0xc32e7 vgabios.c:47
    xor ax, ax                                ; 31 c0                       ; 0xc32ea
    mov es, ax                                ; 8e c0                       ; 0xc32ec
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc32ee
    mov es, cx                                ; 8e c1                       ; 0xc32f1 vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc32f3
    inc bx                                    ; 43                          ; 0xc32f6 vgabios.c:2188
    inc bx                                    ; 43                          ; 0xc32f7
    mov si, 0010ch                            ; be 0c 01                    ; 0xc32f8 vgabios.c:47
    xor ax, ax                                ; 31 c0                       ; 0xc32fb
    mov es, ax                                ; 8e c0                       ; 0xc32fd
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc32ff
    mov es, cx                                ; 8e c1                       ; 0xc3302 vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3304
    inc bx                                    ; 43                          ; 0xc3307 vgabios.c:2189
    inc bx                                    ; 43                          ; 0xc3308
    mov si, 0010eh                            ; be 0e 01                    ; 0xc3309 vgabios.c:47
    xor ax, ax                                ; 31 c0                       ; 0xc330c
    mov es, ax                                ; 8e c0                       ; 0xc330e
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3310
    mov es, cx                                ; 8e c1                       ; 0xc3313 vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3315
    inc bx                                    ; 43                          ; 0xc3318 vgabios.c:2190
    inc bx                                    ; 43                          ; 0xc3319
    test byte [bp-00ch], 004h                 ; f6 46 f4 04                 ; 0xc331a vgabios.c:2192
    je short 0336ch                           ; 74 4c                       ; 0xc331e
    mov dx, 003c7h                            ; ba c7 03                    ; 0xc3320 vgabios.c:2194
    in AL, DX                                 ; ec                          ; 0xc3323
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3324
    mov es, cx                                ; 8e c1                       ; 0xc3326 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3328
    inc bx                                    ; 43                          ; 0xc332b vgabios.c:2194
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc332c
    in AL, DX                                 ; ec                          ; 0xc332f
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3330
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3332 vgabios.c:42
    inc bx                                    ; 43                          ; 0xc3335 vgabios.c:2195
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc3336
    in AL, DX                                 ; ec                          ; 0xc3339
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc333a
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc333c vgabios.c:42
    inc bx                                    ; 43                          ; 0xc333f vgabios.c:2196
    xor al, al                                ; 30 c0                       ; 0xc3340
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc3342
    out DX, AL                                ; ee                          ; 0xc3345
    xor ah, ah                                ; 30 e4                       ; 0xc3346 vgabios.c:2199
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc3348
    jmp short 03354h                          ; eb 07                       ; 0xc334b
    cmp word [bp-008h], 00300h                ; 81 7e f8 00 03              ; 0xc334d
    jnc short 03365h                          ; 73 11                       ; 0xc3352
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc3354 vgabios.c:2200
    in AL, DX                                 ; ec                          ; 0xc3357
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3358
    mov es, cx                                ; 8e c1                       ; 0xc335a vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc335c
    inc bx                                    ; 43                          ; 0xc335f vgabios.c:2200
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc3360 vgabios.c:2201
    jmp short 0334dh                          ; eb e8                       ; 0xc3363
    mov es, cx                                ; 8e c1                       ; 0xc3365 vgabios.c:42
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc3367
    inc bx                                    ; 43                          ; 0xc336b vgabios.c:2202
    mov ax, bx                                ; 89 d8                       ; 0xc336c vgabios.c:2205
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc336e
    pop di                                    ; 5f                          ; 0xc3371
    pop si                                    ; 5e                          ; 0xc3372
    pop cx                                    ; 59                          ; 0xc3373
    pop bp                                    ; 5d                          ; 0xc3374
    retn                                      ; c3                          ; 0xc3375
  ; disGetNextSymbol 0xc3376 LB 0xf1e -> off=0x0 cb=00000000000002ba uValue=00000000000c3376 'biosfn_restore_video_state'
biosfn_restore_video_state:                  ; 0xc3376 LB 0x2ba
    push bp                                   ; 55                          ; 0xc3376 vgabios.c:2207
    mov bp, sp                                ; 89 e5                       ; 0xc3377
    push cx                                   ; 51                          ; 0xc3379
    push si                                   ; 56                          ; 0xc337a
    push di                                   ; 57                          ; 0xc337b
    sub sp, strict byte 00008h                ; 83 ec 08                    ; 0xc337c
    push ax                                   ; 50                          ; 0xc337f
    mov cx, dx                                ; 89 d1                       ; 0xc3380
    test byte [bp-010h], 001h                 ; f6 46 f0 01                 ; 0xc3382 vgabios.c:2211
    je short 033dfh                           ; 74 57                       ; 0xc3386
    mov dx, 003dah                            ; ba da 03                    ; 0xc3388 vgabios.c:2213
    in AL, DX                                 ; ec                          ; 0xc338b
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc338c
    lea si, [bx+040h]                         ; 8d 77 40                    ; 0xc338e vgabios.c:2215
    mov es, cx                                ; 8e c1                       ; 0xc3391 vgabios.c:47
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3393
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc3396 vgabios.c:48
    mov si, bx                                ; 89 de                       ; 0xc3399 vgabios.c:2216
    mov word [bp-008h], strict word 00001h    ; c7 46 f8 01 00              ; 0xc339b vgabios.c:2219
    add bx, strict byte 00005h                ; 83 c3 05                    ; 0xc33a0 vgabios.c:2217
    jmp short 033abh                          ; eb 06                       ; 0xc33a3
    cmp word [bp-008h], strict byte 00004h    ; 83 7e f8 04                 ; 0xc33a5
    jnbe short 033c1h                         ; 77 16                       ; 0xc33a9
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc33ab vgabios.c:2220
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc33ae
    out DX, AL                                ; ee                          ; 0xc33b1
    mov es, cx                                ; 8e c1                       ; 0xc33b2 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc33b4
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc33b7 vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc33ba
    inc bx                                    ; 43                          ; 0xc33bb vgabios.c:2221
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc33bc vgabios.c:2222
    jmp short 033a5h                          ; eb e4                       ; 0xc33bf
    xor al, al                                ; 30 c0                       ; 0xc33c1 vgabios.c:2223
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc33c3
    out DX, AL                                ; ee                          ; 0xc33c6
    mov es, cx                                ; 8e c1                       ; 0xc33c7 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc33c9
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc33cc vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc33cf
    inc bx                                    ; 43                          ; 0xc33d0 vgabios.c:2224
    mov ax, strict word 00011h                ; b8 11 00                    ; 0xc33d1
    mov dx, word [bp-00ch]                    ; 8b 56 f4                    ; 0xc33d4
    out DX, ax                                ; ef                          ; 0xc33d7
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc33d8 vgabios.c:2229
    jmp short 033e8h                          ; eb 09                       ; 0xc33dd
    jmp near 034bfh                           ; e9 dd 00                    ; 0xc33df
    cmp word [bp-008h], strict byte 00018h    ; 83 7e f8 18                 ; 0xc33e2
    jnbe short 03402h                         ; 77 1a                       ; 0xc33e6
    cmp word [bp-008h], strict byte 00011h    ; 83 7e f8 11                 ; 0xc33e8 vgabios.c:2230
    je short 033fch                           ; 74 0e                       ; 0xc33ec
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc33ee vgabios.c:2231
    mov dx, word [bp-00ch]                    ; 8b 56 f4                    ; 0xc33f1
    out DX, AL                                ; ee                          ; 0xc33f4
    mov es, cx                                ; 8e c1                       ; 0xc33f5 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc33f7
    inc dx                                    ; 42                          ; 0xc33fa vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc33fb
    inc bx                                    ; 43                          ; 0xc33fc vgabios.c:2234
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc33fd vgabios.c:2235
    jmp short 033e2h                          ; eb e0                       ; 0xc3400
    mov dx, 003cch                            ; ba cc 03                    ; 0xc3402 vgabios.c:2237
    in AL, DX                                 ; ec                          ; 0xc3405
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3406
    and AL, strict byte 0feh                  ; 24 fe                       ; 0xc3408
    mov word [bp-00eh], ax                    ; 89 46 f2                    ; 0xc340a
    cmp word [bp-00ch], 003d4h                ; 81 7e f4 d4 03              ; 0xc340d vgabios.c:2238
    jne short 03418h                          ; 75 04                       ; 0xc3412
    or byte [bp-00eh], 001h                   ; 80 4e f2 01                 ; 0xc3414 vgabios.c:2239
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc3418 vgabios.c:2240
    mov dx, 003c2h                            ; ba c2 03                    ; 0xc341b
    out DX, AL                                ; ee                          ; 0xc341e
    mov AL, strict byte 011h                  ; b0 11                       ; 0xc341f vgabios.c:2243
    mov dx, word [bp-00ch]                    ; 8b 56 f4                    ; 0xc3421
    out DX, AL                                ; ee                          ; 0xc3424
    lea di, [word bx-00007h]                  ; 8d bf f9 ff                 ; 0xc3425 vgabios.c:2244
    mov es, cx                                ; 8e c1                       ; 0xc3429 vgabios.c:37
    mov al, byte [es:di]                      ; 26 8a 05                    ; 0xc342b
    inc dx                                    ; 42                          ; 0xc342e vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc342f
    lea di, [si+003h]                         ; 8d 7c 03                    ; 0xc3430 vgabios.c:2247
    mov dl, byte [es:di]                      ; 26 8a 15                    ; 0xc3433 vgabios.c:37
    xor dh, dh                                ; 30 f6                       ; 0xc3436 vgabios.c:38
    mov word [bp-00ah], dx                    ; 89 56 f6                    ; 0xc3438
    mov dx, 003dah                            ; ba da 03                    ; 0xc343b vgabios.c:2248
    in AL, DX                                 ; ec                          ; 0xc343e
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc343f
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc3441 vgabios.c:2249
    jmp short 0344eh                          ; eb 06                       ; 0xc3446
    cmp word [bp-008h], strict byte 00013h    ; 83 7e f8 13                 ; 0xc3448
    jnbe short 03467h                         ; 77 19                       ; 0xc344c
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc344e vgabios.c:2250
    and ax, strict word 00020h                ; 25 20 00                    ; 0xc3451
    or ax, word [bp-008h]                     ; 0b 46 f8                    ; 0xc3454
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc3457
    out DX, AL                                ; ee                          ; 0xc345a
    mov es, cx                                ; 8e c1                       ; 0xc345b vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc345d
    out DX, AL                                ; ee                          ; 0xc3460 vgabios.c:38
    inc bx                                    ; 43                          ; 0xc3461 vgabios.c:2251
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc3462 vgabios.c:2252
    jmp short 03448h                          ; eb e1                       ; 0xc3465
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc3467 vgabios.c:2253
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc346a
    out DX, AL                                ; ee                          ; 0xc346d
    mov dx, 003dah                            ; ba da 03                    ; 0xc346e vgabios.c:2254
    in AL, DX                                 ; ec                          ; 0xc3471
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3472
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc3474 vgabios.c:2256
    jmp short 03481h                          ; eb 06                       ; 0xc3479
    cmp word [bp-008h], strict byte 00008h    ; 83 7e f8 08                 ; 0xc347b
    jnbe short 03497h                         ; 77 16                       ; 0xc347f
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc3481 vgabios.c:2257
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc3484
    out DX, AL                                ; ee                          ; 0xc3487
    mov es, cx                                ; 8e c1                       ; 0xc3488 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc348a
    mov dx, 003cfh                            ; ba cf 03                    ; 0xc348d vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc3490
    inc bx                                    ; 43                          ; 0xc3491 vgabios.c:2258
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc3492 vgabios.c:2259
    jmp short 0347bh                          ; eb e4                       ; 0xc3495
    add bx, strict byte 00006h                ; 83 c3 06                    ; 0xc3497 vgabios.c:2260
    mov es, cx                                ; 8e c1                       ; 0xc349a vgabios.c:37
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc349c
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc349f vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc34a2
    inc si                                    ; 46                          ; 0xc34a3 vgabios.c:2263
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc34a4 vgabios.c:37
    mov dx, word [bp-00ch]                    ; 8b 56 f4                    ; 0xc34a7 vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc34aa
    inc si                                    ; 46                          ; 0xc34ab vgabios.c:2264
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc34ac vgabios.c:37
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc34af vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc34b2
    inc si                                    ; 46                          ; 0xc34b3 vgabios.c:2265
    inc si                                    ; 46                          ; 0xc34b4
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc34b5 vgabios.c:37
    mov dx, word [bp-00ch]                    ; 8b 56 f4                    ; 0xc34b8 vgabios.c:38
    add dx, strict byte 00006h                ; 83 c2 06                    ; 0xc34bb
    out DX, AL                                ; ee                          ; 0xc34be
    test byte [bp-010h], 002h                 ; f6 46 f0 02                 ; 0xc34bf vgabios.c:2269
    jne short 034c8h                          ; 75 03                       ; 0xc34c3
    jmp near 035e3h                           ; e9 1b 01                    ; 0xc34c5
    mov es, cx                                ; 8e c1                       ; 0xc34c8 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc34ca
    mov si, strict word 00049h                ; be 49 00                    ; 0xc34cd vgabios.c:42
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc34d0
    mov es, dx                                ; 8e c2                       ; 0xc34d3
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc34d5
    inc bx                                    ; 43                          ; 0xc34d8 vgabios.c:2270
    mov es, cx                                ; 8e c1                       ; 0xc34d9 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc34db
    mov si, strict word 0004ah                ; be 4a 00                    ; 0xc34de vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc34e1
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc34e3
    inc bx                                    ; 43                          ; 0xc34e6 vgabios.c:2271
    inc bx                                    ; 43                          ; 0xc34e7
    mov es, cx                                ; 8e c1                       ; 0xc34e8 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc34ea
    mov si, strict word 0004ch                ; be 4c 00                    ; 0xc34ed vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc34f0
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc34f2
    inc bx                                    ; 43                          ; 0xc34f5 vgabios.c:2272
    inc bx                                    ; 43                          ; 0xc34f6
    mov es, cx                                ; 8e c1                       ; 0xc34f7 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc34f9
    mov si, strict word 00063h                ; be 63 00                    ; 0xc34fc vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc34ff
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3501
    inc bx                                    ; 43                          ; 0xc3504 vgabios.c:2273
    inc bx                                    ; 43                          ; 0xc3505
    mov es, cx                                ; 8e c1                       ; 0xc3506 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc3508
    mov si, 00084h                            ; be 84 00                    ; 0xc350b vgabios.c:42
    mov es, dx                                ; 8e c2                       ; 0xc350e
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc3510
    inc bx                                    ; 43                          ; 0xc3513 vgabios.c:2274
    mov es, cx                                ; 8e c1                       ; 0xc3514 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3516
    mov si, 00085h                            ; be 85 00                    ; 0xc3519 vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc351c
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc351e
    inc bx                                    ; 43                          ; 0xc3521 vgabios.c:2275
    inc bx                                    ; 43                          ; 0xc3522
    mov es, cx                                ; 8e c1                       ; 0xc3523 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc3525
    mov si, 00087h                            ; be 87 00                    ; 0xc3528 vgabios.c:42
    mov es, dx                                ; 8e c2                       ; 0xc352b
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc352d
    inc bx                                    ; 43                          ; 0xc3530 vgabios.c:2276
    mov es, cx                                ; 8e c1                       ; 0xc3531 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc3533
    mov si, 00088h                            ; be 88 00                    ; 0xc3536 vgabios.c:42
    mov es, dx                                ; 8e c2                       ; 0xc3539
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc353b
    inc bx                                    ; 43                          ; 0xc353e vgabios.c:2277
    mov es, cx                                ; 8e c1                       ; 0xc353f vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc3541
    mov si, 00089h                            ; be 89 00                    ; 0xc3544 vgabios.c:42
    mov es, dx                                ; 8e c2                       ; 0xc3547
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc3549
    inc bx                                    ; 43                          ; 0xc354c vgabios.c:2278
    mov es, cx                                ; 8e c1                       ; 0xc354d vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc354f
    mov si, strict word 00060h                ; be 60 00                    ; 0xc3552 vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc3555
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3557
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc355a vgabios.c:2280
    inc bx                                    ; 43                          ; 0xc355f vgabios.c:2279
    inc bx                                    ; 43                          ; 0xc3560
    jmp short 03569h                          ; eb 06                       ; 0xc3561
    cmp word [bp-008h], strict byte 00008h    ; 83 7e f8 08                 ; 0xc3563
    jnc short 03585h                          ; 73 1c                       ; 0xc3567
    mov es, cx                                ; 8e c1                       ; 0xc3569 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc356b
    mov si, word [bp-008h]                    ; 8b 76 f8                    ; 0xc356e vgabios.c:48
    sal si, 1                                 ; d1 e6                       ; 0xc3571
    add si, strict byte 00050h                ; 83 c6 50                    ; 0xc3573
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc3576 vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc3579
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc357b
    inc bx                                    ; 43                          ; 0xc357e vgabios.c:2282
    inc bx                                    ; 43                          ; 0xc357f
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc3580 vgabios.c:2283
    jmp short 03563h                          ; eb de                       ; 0xc3583
    mov es, cx                                ; 8e c1                       ; 0xc3585 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3587
    mov si, strict word 0004eh                ; be 4e 00                    ; 0xc358a vgabios.c:52
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc358d
    mov es, dx                                ; 8e c2                       ; 0xc3590
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3592
    inc bx                                    ; 43                          ; 0xc3595 vgabios.c:2284
    inc bx                                    ; 43                          ; 0xc3596
    mov es, cx                                ; 8e c1                       ; 0xc3597 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc3599
    mov si, strict word 00062h                ; be 62 00                    ; 0xc359c vgabios.c:42
    mov es, dx                                ; 8e c2                       ; 0xc359f
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc35a1
    inc bx                                    ; 43                          ; 0xc35a4 vgabios.c:2285
    mov es, cx                                ; 8e c1                       ; 0xc35a5 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc35a7
    mov si, strict word 0007ch                ; be 7c 00                    ; 0xc35aa vgabios.c:52
    xor dx, dx                                ; 31 d2                       ; 0xc35ad
    mov es, dx                                ; 8e c2                       ; 0xc35af
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc35b1
    inc bx                                    ; 43                          ; 0xc35b4 vgabios.c:2287
    inc bx                                    ; 43                          ; 0xc35b5
    mov es, cx                                ; 8e c1                       ; 0xc35b6 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc35b8
    mov si, strict word 0007eh                ; be 7e 00                    ; 0xc35bb vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc35be
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc35c0
    inc bx                                    ; 43                          ; 0xc35c3 vgabios.c:2288
    inc bx                                    ; 43                          ; 0xc35c4
    mov es, cx                                ; 8e c1                       ; 0xc35c5 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc35c7
    mov si, 0010ch                            ; be 0c 01                    ; 0xc35ca vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc35cd
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc35cf
    inc bx                                    ; 43                          ; 0xc35d2 vgabios.c:2289
    inc bx                                    ; 43                          ; 0xc35d3
    mov es, cx                                ; 8e c1                       ; 0xc35d4 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc35d6
    mov si, 0010eh                            ; be 0e 01                    ; 0xc35d9 vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc35dc
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc35de
    inc bx                                    ; 43                          ; 0xc35e1 vgabios.c:2290
    inc bx                                    ; 43                          ; 0xc35e2
    test byte [bp-010h], 004h                 ; f6 46 f0 04                 ; 0xc35e3 vgabios.c:2292
    je short 03626h                           ; 74 3d                       ; 0xc35e7
    inc bx                                    ; 43                          ; 0xc35e9 vgabios.c:2293
    mov es, cx                                ; 8e c1                       ; 0xc35ea vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc35ec
    xor ah, ah                                ; 30 e4                       ; 0xc35ef vgabios.c:38
    mov word [bp-00eh], ax                    ; 89 46 f2                    ; 0xc35f1
    inc bx                                    ; 43                          ; 0xc35f4 vgabios.c:2294
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc35f5 vgabios.c:37
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc35f8 vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc35fb
    inc bx                                    ; 43                          ; 0xc35fc vgabios.c:2295
    xor al, al                                ; 30 c0                       ; 0xc35fd
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc35ff
    out DX, AL                                ; ee                          ; 0xc3602
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc3603 vgabios.c:2298
    jmp short 0360fh                          ; eb 07                       ; 0xc3606
    cmp word [bp-008h], 00300h                ; 81 7e f8 00 03              ; 0xc3608
    jnc short 0361eh                          ; 73 0f                       ; 0xc360d
    mov es, cx                                ; 8e c1                       ; 0xc360f vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc3611
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc3614 vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc3617
    inc bx                                    ; 43                          ; 0xc3618 vgabios.c:2299
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc3619 vgabios.c:2300
    jmp short 03608h                          ; eb ea                       ; 0xc361c
    inc bx                                    ; 43                          ; 0xc361e vgabios.c:2301
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc361f
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc3622
    out DX, AL                                ; ee                          ; 0xc3625
    mov ax, bx                                ; 89 d8                       ; 0xc3626 vgabios.c:2305
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc3628
    pop di                                    ; 5f                          ; 0xc362b
    pop si                                    ; 5e                          ; 0xc362c
    pop cx                                    ; 59                          ; 0xc362d
    pop bp                                    ; 5d                          ; 0xc362e
    retn                                      ; c3                          ; 0xc362f
  ; disGetNextSymbol 0xc3630 LB 0xc64 -> off=0x0 cb=000000000000002b uValue=00000000000c3630 'find_vga_entry'
find_vga_entry:                              ; 0xc3630 LB 0x2b
    push bx                                   ; 53                          ; 0xc3630 vgabios.c:2314
    push cx                                   ; 51                          ; 0xc3631
    push dx                                   ; 52                          ; 0xc3632
    push bp                                   ; 55                          ; 0xc3633
    mov bp, sp                                ; 89 e5                       ; 0xc3634
    mov dl, al                                ; 88 c2                       ; 0xc3636
    mov AH, strict byte 0ffh                  ; b4 ff                       ; 0xc3638 vgabios.c:2316
    xor al, al                                ; 30 c0                       ; 0xc363a vgabios.c:2317
    jmp short 03644h                          ; eb 06                       ; 0xc363c
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc363e vgabios.c:2318
    cmp AL, strict byte 00fh                  ; 3c 0f                       ; 0xc3640
    jnbe short 03654h                         ; 77 10                       ; 0xc3642
    mov bl, al                                ; 88 c3                       ; 0xc3644
    xor bh, bh                                ; 30 ff                       ; 0xc3646
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc3648
    sal bx, CL                                ; d3 e3                       ; 0xc364a
    cmp dl, byte [bx+047aeh]                  ; 3a 97 ae 47                 ; 0xc364c
    jne short 0363eh                          ; 75 ec                       ; 0xc3650
    mov ah, al                                ; 88 c4                       ; 0xc3652
    mov al, ah                                ; 88 e0                       ; 0xc3654 vgabios.c:2323
    pop bp                                    ; 5d                          ; 0xc3656
    pop dx                                    ; 5a                          ; 0xc3657
    pop cx                                    ; 59                          ; 0xc3658
    pop bx                                    ; 5b                          ; 0xc3659
    retn                                      ; c3                          ; 0xc365a
  ; disGetNextSymbol 0xc365b LB 0xc39 -> off=0x0 cb=000000000000000e uValue=00000000000c365b 'readx_byte'
readx_byte:                                  ; 0xc365b LB 0xe
    push bx                                   ; 53                          ; 0xc365b vgabios.c:2335
    push bp                                   ; 55                          ; 0xc365c
    mov bp, sp                                ; 89 e5                       ; 0xc365d
    mov bx, dx                                ; 89 d3                       ; 0xc365f
    mov es, ax                                ; 8e c0                       ; 0xc3661 vgabios.c:2337
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc3663
    pop bp                                    ; 5d                          ; 0xc3666 vgabios.c:2338
    pop bx                                    ; 5b                          ; 0xc3667
    retn                                      ; c3                          ; 0xc3668
  ; disGetNextSymbol 0xc3669 LB 0xc2b -> off=0x87 cb=00000000000003f8 uValue=00000000000c36f0 'int10_func'
    db  056h, 04fh, 01ch, 01bh, 013h, 012h, 011h, 010h, 00eh, 00dh, 00ch, 00ah, 009h, 008h, 007h, 006h
    db  005h, 004h, 003h, 002h, 001h, 000h, 0e1h, 03ah, 01ah, 037h, 057h, 037h, 064h, 037h, 072h, 037h
    db  082h, 037h, 092h, 037h, 09ch, 037h, 0c9h, 037h, 0eeh, 037h, 0fch, 037h, 014h, 038h, 02ah, 038h
    db  046h, 038h, 060h, 038h, 076h, 038h, 082h, 038h, 046h, 039h, 0adh, 039h, 0d1h, 039h, 0e6h, 039h
    db  028h, 03ah, 0b3h, 03ah, 030h, 024h, 023h, 022h, 021h, 020h, 014h, 012h, 011h, 010h, 004h, 002h
    db  001h, 000h, 0e1h, 03ah, 0a1h, 038h, 0bfh, 038h, 0ceh, 038h, 0ddh, 038h, 0a1h, 038h, 0bfh, 038h
    db  0ceh, 038h, 0ddh, 038h, 0ech, 038h, 0f8h, 038h, 011h, 039h, 01bh, 039h, 025h, 039h, 02fh, 039h
    db  00ah, 009h, 006h, 004h, 002h, 001h, 000h, 0a5h, 03ah, 04eh, 03ah, 05ch, 03ah, 06dh, 03ah, 07dh
    db  03ah, 092h, 03ah, 0a5h, 03ah, 0a5h, 03ah
int10_func:                                  ; 0xc36f0 LB 0x3f8
    push bp                                   ; 55                          ; 0xc36f0 vgabios.c:2416
    mov bp, sp                                ; 89 e5                       ; 0xc36f1
    push si                                   ; 56                          ; 0xc36f3
    push di                                   ; 57                          ; 0xc36f4
    push ax                                   ; 50                          ; 0xc36f5
    mov si, word [bp+004h]                    ; 8b 76 04                    ; 0xc36f6
    mov al, byte [bp+013h]                    ; 8a 46 13                    ; 0xc36f9 vgabios.c:2421
    xor ah, ah                                ; 30 e4                       ; 0xc36fc
    mov dx, ax                                ; 89 c2                       ; 0xc36fe
    cmp ax, strict word 00056h                ; 3d 56 00                    ; 0xc3700
    jnbe short 0376fh                         ; 77 6a                       ; 0xc3703
    push CS                                   ; 0e                          ; 0xc3705
    pop ES                                    ; 07                          ; 0xc3706
    mov cx, strict word 00017h                ; b9 17 00                    ; 0xc3707
    mov di, 03669h                            ; bf 69 36                    ; 0xc370a
    repne scasb                               ; f2 ae                       ; 0xc370d
    sal cx, 1                                 ; d1 e1                       ; 0xc370f
    mov di, cx                                ; 89 cf                       ; 0xc3711
    mov ax, word [cs:di+0367fh]               ; 2e 8b 85 7f 36              ; 0xc3713
    jmp ax                                    ; ff e0                       ; 0xc3718
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc371a vgabios.c:2424
    xor ah, ah                                ; 30 e4                       ; 0xc371d
    call 013b5h                               ; e8 93 dc                    ; 0xc371f
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3722 vgabios.c:2425
    and ax, strict word 0007fh                ; 25 7f 00                    ; 0xc3725
    cmp ax, strict word 00007h                ; 3d 07 00                    ; 0xc3728
    je short 03742h                           ; 74 15                       ; 0xc372b
    cmp ax, strict word 00006h                ; 3d 06 00                    ; 0xc372d
    je short 03739h                           ; 74 07                       ; 0xc3730
    cmp ax, strict word 00005h                ; 3d 05 00                    ; 0xc3732
    jbe short 03742h                          ; 76 0b                       ; 0xc3735
    jmp short 0374bh                          ; eb 12                       ; 0xc3737
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3739 vgabios.c:2427
    xor al, al                                ; 30 c0                       ; 0xc373c
    or AL, strict byte 03fh                   ; 0c 3f                       ; 0xc373e
    jmp short 03752h                          ; eb 10                       ; 0xc3740 vgabios.c:2428
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3742 vgabios.c:2436
    xor al, al                                ; 30 c0                       ; 0xc3745
    or AL, strict byte 030h                   ; 0c 30                       ; 0xc3747
    jmp short 03752h                          ; eb 07                       ; 0xc3749
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc374b vgabios.c:2439
    xor al, al                                ; 30 c0                       ; 0xc374e
    or AL, strict byte 020h                   ; 0c 20                       ; 0xc3750
    mov word [bp+012h], ax                    ; 89 46 12                    ; 0xc3752
    jmp short 0376fh                          ; eb 18                       ; 0xc3755 vgabios.c:2441
    mov dl, byte [bp+010h]                    ; 8a 56 10                    ; 0xc3757 vgabios.c:2443
    mov al, byte [bp+011h]                    ; 8a 46 11                    ; 0xc375a
    xor ah, ah                                ; 30 e4                       ; 0xc375d
    call 01157h                               ; e8 f5 d9                    ; 0xc375f
    jmp short 0376fh                          ; eb 0b                       ; 0xc3762 vgabios.c:2444
    mov dx, word [bp+00eh]                    ; 8b 56 0e                    ; 0xc3764 vgabios.c:2446
    mov al, byte [bp+00dh]                    ; 8a 46 0d                    ; 0xc3767
    xor ah, ah                                ; 30 e4                       ; 0xc376a
    call 01253h                               ; e8 e4 da                    ; 0xc376c
    jmp near 03ae1h                           ; e9 6f 03                    ; 0xc376f vgabios.c:2447
    lea bx, [bp+00eh]                         ; 8d 5e 0e                    ; 0xc3772 vgabios.c:2449
    lea dx, [bp+010h]                         ; 8d 56 10                    ; 0xc3775
    mov al, byte [bp+00dh]                    ; 8a 46 0d                    ; 0xc3778
    xor ah, ah                                ; 30 e4                       ; 0xc377b
    call 00a1bh                               ; e8 9b d2                    ; 0xc377d
    jmp short 0376fh                          ; eb ed                       ; 0xc3780 vgabios.c:2450
    xor ax, ax                                ; 31 c0                       ; 0xc3782 vgabios.c:2456
    mov word [bp+012h], ax                    ; 89 46 12                    ; 0xc3784
    mov word [bp+00ch], ax                    ; 89 46 0c                    ; 0xc3787 vgabios.c:2457
    mov word [bp+010h], ax                    ; 89 46 10                    ; 0xc378a vgabios.c:2458
    mov word [bp+00eh], ax                    ; 89 46 0e                    ; 0xc378d vgabios.c:2459
    jmp short 0376fh                          ; eb dd                       ; 0xc3790 vgabios.c:2460
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3792 vgabios.c:2462
    xor ah, ah                                ; 30 e4                       ; 0xc3795
    call 012e0h                               ; e8 46 db                    ; 0xc3797
    jmp short 0376fh                          ; eb d3                       ; 0xc379a vgabios.c:2463
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc379c vgabios.c:2465
    push ax                                   ; 50                          ; 0xc379f
    mov ax, 000ffh                            ; b8 ff 00                    ; 0xc37a0
    push ax                                   ; 50                          ; 0xc37a3
    mov al, byte [bp+00eh]                    ; 8a 46 0e                    ; 0xc37a4
    xor ah, ah                                ; 30 e4                       ; 0xc37a7
    push ax                                   ; 50                          ; 0xc37a9
    mov al, byte [bp+00fh]                    ; 8a 46 0f                    ; 0xc37aa
    push ax                                   ; 50                          ; 0xc37ad
    mov al, byte [bp+010h]                    ; 8a 46 10                    ; 0xc37ae
    mov cx, ax                                ; 89 c1                       ; 0xc37b1
    mov al, byte [bp+011h]                    ; 8a 46 11                    ; 0xc37b3
    mov dl, byte [bp+00dh]                    ; 8a 56 0d                    ; 0xc37b6
    mov bl, byte [bp+012h]                    ; 8a 5e 12                    ; 0xc37b9
    xor bh, bh                                ; 30 ff                       ; 0xc37bc
    mov si, bx                                ; 89 de                       ; 0xc37be
    mov bx, ax                                ; 89 c3                       ; 0xc37c0
    mov ax, si                                ; 89 f0                       ; 0xc37c2
    call 01a7eh                               ; e8 b7 e2                    ; 0xc37c4
    jmp short 0376fh                          ; eb a6                       ; 0xc37c7 vgabios.c:2466
    xor ax, ax                                ; 31 c0                       ; 0xc37c9 vgabios.c:2468
    push ax                                   ; 50                          ; 0xc37cb
    mov ax, 000ffh                            ; b8 ff 00                    ; 0xc37cc
    push ax                                   ; 50                          ; 0xc37cf
    mov al, byte [bp+00eh]                    ; 8a 46 0e                    ; 0xc37d0
    xor ah, ah                                ; 30 e4                       ; 0xc37d3
    push ax                                   ; 50                          ; 0xc37d5
    mov al, byte [bp+00fh]                    ; 8a 46 0f                    ; 0xc37d6
    push ax                                   ; 50                          ; 0xc37d9
    mov al, byte [bp+010h]                    ; 8a 46 10                    ; 0xc37da
    mov cx, ax                                ; 89 c1                       ; 0xc37dd
    mov al, byte [bp+011h]                    ; 8a 46 11                    ; 0xc37df
    mov bx, ax                                ; 89 c3                       ; 0xc37e2
    mov al, byte [bp+00dh]                    ; 8a 46 0d                    ; 0xc37e4
    mov dx, ax                                ; 89 c2                       ; 0xc37e7
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc37e9
    jmp short 037c4h                          ; eb d6                       ; 0xc37ec
    lea dx, [bp+012h]                         ; 8d 56 12                    ; 0xc37ee vgabios.c:2471
    mov al, byte [bp+00dh]                    ; 8a 46 0d                    ; 0xc37f1
    xor ah, ah                                ; 30 e4                       ; 0xc37f4
    call 00d71h                               ; e8 78 d5                    ; 0xc37f6
    jmp near 03ae1h                           ; e9 e5 02                    ; 0xc37f9 vgabios.c:2472
    mov cx, word [bp+010h]                    ; 8b 4e 10                    ; 0xc37fc vgabios.c:2474
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc37ff
    xor ah, ah                                ; 30 e4                       ; 0xc3802
    mov bx, ax                                ; 89 c3                       ; 0xc3804
    mov al, byte [bp+00dh]                    ; 8a 46 0d                    ; 0xc3806
    mov dx, ax                                ; 89 c2                       ; 0xc3809
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc380b
    call 023e6h                               ; e8 d5 eb                    ; 0xc380e
    jmp near 03ae1h                           ; e9 cd 02                    ; 0xc3811 vgabios.c:2475
    mov cx, word [bp+010h]                    ; 8b 4e 10                    ; 0xc3814 vgabios.c:2477
    mov bl, byte [bp+00ch]                    ; 8a 5e 0c                    ; 0xc3817
    xor bh, bh                                ; 30 ff                       ; 0xc381a
    mov dl, byte [bp+00dh]                    ; 8a 56 0d                    ; 0xc381c
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc381f
    xor ah, ah                                ; 30 e4                       ; 0xc3822
    call 02558h                               ; e8 31 ed                    ; 0xc3824
    jmp near 03ae1h                           ; e9 b7 02                    ; 0xc3827 vgabios.c:2478
    mov cx, word [bp+00eh]                    ; 8b 4e 0e                    ; 0xc382a vgabios.c:2480
    mov bx, word [bp+010h]                    ; 8b 5e 10                    ; 0xc382d
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3830
    xor ah, ah                                ; 30 e4                       ; 0xc3833
    mov dx, ax                                ; 89 c2                       ; 0xc3835
    mov al, byte [bp+00dh]                    ; 8a 46 0d                    ; 0xc3837
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc383a
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc383d
    call 026dbh                               ; e8 98 ee                    ; 0xc3840
    jmp near 03ae1h                           ; e9 9b 02                    ; 0xc3843 vgabios.c:2481
    lea cx, [bp+012h]                         ; 8d 4e 12                    ; 0xc3846 vgabios.c:2483
    mov bx, word [bp+00eh]                    ; 8b 5e 0e                    ; 0xc3849
    mov dx, word [bp+010h]                    ; 8b 56 10                    ; 0xc384c
    mov al, byte [bp+00dh]                    ; 8a 46 0d                    ; 0xc384f
    xor ah, ah                                ; 30 e4                       ; 0xc3852
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc3854
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc3857
    call 00f4bh                               ; e8 ee d6                    ; 0xc385a
    jmp near 03ae1h                           ; e9 81 02                    ; 0xc385d vgabios.c:2484
    mov cx, strict word 00002h                ; b9 02 00                    ; 0xc3860 vgabios.c:2492
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc3863
    xor ah, ah                                ; 30 e4                       ; 0xc3866
    mov bx, ax                                ; 89 c3                       ; 0xc3868
    mov dx, 000ffh                            ; ba ff 00                    ; 0xc386a
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc386d
    call 02855h                               ; e8 e2 ef                    ; 0xc3870
    jmp near 03ae1h                           ; e9 6b 02                    ; 0xc3873 vgabios.c:2493
    mov dx, word [bp+010h]                    ; 8b 56 10                    ; 0xc3876 vgabios.c:2496
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3879
    call 010b8h                               ; e8 39 d8                    ; 0xc387c
    jmp near 03ae1h                           ; e9 5f 02                    ; 0xc387f vgabios.c:2497
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3882 vgabios.c:2499
    xor ah, ah                                ; 30 e4                       ; 0xc3885
    cmp ax, strict word 00030h                ; 3d 30 00                    ; 0xc3887
    jnbe short 038f5h                         ; 77 69                       ; 0xc388a
    push CS                                   ; 0e                          ; 0xc388c
    pop ES                                    ; 07                          ; 0xc388d
    mov cx, strict word 0000fh                ; b9 0f 00                    ; 0xc388e
    mov di, 036adh                            ; bf ad 36                    ; 0xc3891
    repne scasb                               ; f2 ae                       ; 0xc3894
    sal cx, 1                                 ; d1 e1                       ; 0xc3896
    mov di, cx                                ; 89 cf                       ; 0xc3898
    mov ax, word [cs:di+036bbh]               ; 2e 8b 85 bb 36              ; 0xc389a
    jmp ax                                    ; ff e0                       ; 0xc389f
    mov al, byte [bp+00dh]                    ; 8a 46 0d                    ; 0xc38a1 vgabios.c:2503
    xor ah, ah                                ; 30 e4                       ; 0xc38a4
    push ax                                   ; 50                          ; 0xc38a6
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc38a7
    push ax                                   ; 50                          ; 0xc38aa
    push word [bp+00eh]                       ; ff 76 0e                    ; 0xc38ab
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc38ae
    mov cx, word [bp+010h]                    ; 8b 4e 10                    ; 0xc38b1
    mov bx, word [bp+008h]                    ; 8b 5e 08                    ; 0xc38b4
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc38b7
    call 02bd6h                               ; e8 19 f3                    ; 0xc38ba
    jmp short 038f5h                          ; eb 36                       ; 0xc38bd vgabios.c:2504
    mov dl, byte [bp+00ch]                    ; 8a 56 0c                    ; 0xc38bf vgabios.c:2507
    xor dh, dh                                ; 30 f6                       ; 0xc38c2
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc38c4
    xor ah, ah                                ; 30 e4                       ; 0xc38c7
    call 02c5ah                               ; e8 8e f3                    ; 0xc38c9
    jmp short 038f5h                          ; eb 27                       ; 0xc38cc vgabios.c:2508
    mov dl, byte [bp+00ch]                    ; 8a 56 0c                    ; 0xc38ce vgabios.c:2511
    xor dh, dh                                ; 30 f6                       ; 0xc38d1
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc38d3
    xor ah, ah                                ; 30 e4                       ; 0xc38d6
    call 02ccfh                               ; e8 f4 f3                    ; 0xc38d8
    jmp short 038f5h                          ; eb 18                       ; 0xc38db vgabios.c:2512
    mov dl, byte [bp+00ch]                    ; 8a 56 0c                    ; 0xc38dd vgabios.c:2515
    xor dh, dh                                ; 30 f6                       ; 0xc38e0
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc38e2
    xor ah, ah                                ; 30 e4                       ; 0xc38e5
    call 02d42h                               ; e8 58 f4                    ; 0xc38e7
    jmp short 038f5h                          ; eb 09                       ; 0xc38ea vgabios.c:2516
    mov dx, word [bp+008h]                    ; 8b 56 08                    ; 0xc38ec vgabios.c:2518
    mov ax, word [bp+016h]                    ; 8b 46 16                    ; 0xc38ef
    call 02db5h                               ; e8 c0 f4                    ; 0xc38f2
    jmp near 03ae1h                           ; e9 e9 01                    ; 0xc38f5 vgabios.c:2519
    mov al, byte [bp+00eh]                    ; 8a 46 0e                    ; 0xc38f8 vgabios.c:2521
    xor ah, ah                                ; 30 e4                       ; 0xc38fb
    push ax                                   ; 50                          ; 0xc38fd
    mov cl, byte [bp+00ch]                    ; 8a 4e 0c                    ; 0xc38fe
    xor ch, ch                                ; 30 ed                       ; 0xc3901
    mov bx, word [bp+010h]                    ; 8b 5e 10                    ; 0xc3903
    mov dx, word [bp+008h]                    ; 8b 56 08                    ; 0xc3906
    mov ax, word [bp+016h]                    ; 8b 46 16                    ; 0xc3909
    call 02dbah                               ; e8 ab f4                    ; 0xc390c
    jmp short 038f5h                          ; eb e4                       ; 0xc390f vgabios.c:2522
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc3911 vgabios.c:2524
    xor ah, ah                                ; 30 e4                       ; 0xc3914
    call 02dc1h                               ; e8 a8 f4                    ; 0xc3916
    jmp short 038f5h                          ; eb da                       ; 0xc3919 vgabios.c:2525
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc391b vgabios.c:2527
    xor ah, ah                                ; 30 e4                       ; 0xc391e
    call 02dc6h                               ; e8 a3 f4                    ; 0xc3920
    jmp short 038f5h                          ; eb d0                       ; 0xc3923 vgabios.c:2528
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc3925 vgabios.c:2530
    xor ah, ah                                ; 30 e4                       ; 0xc3928
    call 02dcbh                               ; e8 9e f4                    ; 0xc392a
    jmp short 038f5h                          ; eb c6                       ; 0xc392d vgabios.c:2531
    lea ax, [bp+00eh]                         ; 8d 46 0e                    ; 0xc392f vgabios.c:2533
    push ax                                   ; 50                          ; 0xc3932
    lea cx, [bp+010h]                         ; 8d 4e 10                    ; 0xc3933
    lea bx, [bp+008h]                         ; 8d 5e 08                    ; 0xc3936
    lea dx, [bp+016h]                         ; 8d 56 16                    ; 0xc3939
    mov al, byte [bp+00dh]                    ; 8a 46 0d                    ; 0xc393c
    xor ah, ah                                ; 30 e4                       ; 0xc393f
    call 00ec8h                               ; e8 84 d5                    ; 0xc3941
    jmp short 038f5h                          ; eb af                       ; 0xc3944 vgabios.c:2541
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3946 vgabios.c:2543
    xor ah, ah                                ; 30 e4                       ; 0xc3949
    cmp ax, strict word 00034h                ; 3d 34 00                    ; 0xc394b
    jc short 0395eh                           ; 72 0e                       ; 0xc394e
    jbe short 03968h                          ; 76 16                       ; 0xc3950
    cmp ax, strict word 00036h                ; 3d 36 00                    ; 0xc3952
    je short 039a5h                           ; 74 4e                       ; 0xc3955
    cmp ax, strict word 00035h                ; 3d 35 00                    ; 0xc3957
    je short 03997h                           ; 74 3b                       ; 0xc395a
    jmp short 038f5h                          ; eb 97                       ; 0xc395c
    cmp ax, strict word 00020h                ; 3d 20 00                    ; 0xc395e
    jne short 039ceh                          ; 75 6b                       ; 0xc3961
    call 02dd0h                               ; e8 6a f4                    ; 0xc3963 vgabios.c:2546
    jmp short 039ceh                          ; eb 66                       ; 0xc3966 vgabios.c:2547
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3968 vgabios.c:2549
    xor ah, ah                                ; 30 e4                       ; 0xc396b
    cmp ax, strict word 00002h                ; 3d 02 00                    ; 0xc396d
    jnc short 03992h                          ; 73 20                       ; 0xc3970
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3972 vgabios.c:35
    mov bx, 00087h                            ; bb 87 00                    ; 0xc3975
    mov es, ax                                ; 8e c0                       ; 0xc3978 vgabios.c:37
    mov dl, byte [es:bx]                      ; 26 8a 17                    ; 0xc397a
    and dl, 0feh                              ; 80 e2 fe                    ; 0xc397d vgabios.c:38
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3980
    or dl, al                                 ; 08 c2                       ; 0xc3983
    mov byte [es:bx], dl                      ; 26 88 17                    ; 0xc3985 vgabios.c:42
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3988 vgabios.c:2552
    xor al, al                                ; 30 c0                       ; 0xc398b
    or AL, strict byte 012h                   ; 0c 12                       ; 0xc398d
    jmp near 03752h                           ; e9 c0 fd                    ; 0xc398f
    mov byte [bp+012h], ah                    ; 88 66 12                    ; 0xc3992 vgabios.c:2555
    jmp short 039ceh                          ; eb 37                       ; 0xc3995 vgabios.c:2556
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3997 vgabios.c:2558
    mov bx, word [bp+00eh]                    ; 8b 5e 0e                    ; 0xc399a
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc399d
    call 02dd5h                               ; e8 32 f4                    ; 0xc39a0
    jmp short 03988h                          ; eb e3                       ; 0xc39a3
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc39a5 vgabios.c:2562
    call 02ddah                               ; e8 2f f4                    ; 0xc39a8
    jmp short 03988h                          ; eb db                       ; 0xc39ab
    push word [bp+008h]                       ; ff 76 08                    ; 0xc39ad vgabios.c:2572
    push word [bp+016h]                       ; ff 76 16                    ; 0xc39b0
    mov al, byte [bp+00eh]                    ; 8a 46 0e                    ; 0xc39b3
    xor ah, ah                                ; 30 e4                       ; 0xc39b6
    push ax                                   ; 50                          ; 0xc39b8
    mov al, byte [bp+00fh]                    ; 8a 46 0f                    ; 0xc39b9
    push ax                                   ; 50                          ; 0xc39bc
    mov bl, byte [bp+00ch]                    ; 8a 5e 0c                    ; 0xc39bd
    xor bh, bh                                ; 30 ff                       ; 0xc39c0
    mov dl, byte [bp+00dh]                    ; 8a 56 0d                    ; 0xc39c2
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc39c5
    mov cx, word [bp+010h]                    ; 8b 4e 10                    ; 0xc39c8
    call 02ddfh                               ; e8 11 f4                    ; 0xc39cb
    jmp near 03ae1h                           ; e9 10 01                    ; 0xc39ce vgabios.c:2573
    mov bx, si                                ; 89 f3                       ; 0xc39d1 vgabios.c:2575
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc39d3
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc39d6
    call 02e6eh                               ; e8 92 f4                    ; 0xc39d9
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc39dc vgabios.c:2576
    xor al, al                                ; 30 c0                       ; 0xc39df
    or AL, strict byte 01bh                   ; 0c 1b                       ; 0xc39e1
    jmp near 03752h                           ; e9 6c fd                    ; 0xc39e3
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc39e6 vgabios.c:2579
    xor ah, ah                                ; 30 e4                       ; 0xc39e9
    cmp ax, strict word 00002h                ; 3d 02 00                    ; 0xc39eb
    je short 03a12h                           ; 74 22                       ; 0xc39ee
    cmp ax, strict word 00001h                ; 3d 01 00                    ; 0xc39f0
    je short 03a04h                           ; 74 0f                       ; 0xc39f3
    test ax, ax                               ; 85 c0                       ; 0xc39f5
    jne short 03a1eh                          ; 75 25                       ; 0xc39f7
    lea dx, [bp+00ch]                         ; 8d 56 0c                    ; 0xc39f9 vgabios.c:2582
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc39fc
    call 03083h                               ; e8 81 f6                    ; 0xc39ff
    jmp short 03a1eh                          ; eb 1a                       ; 0xc3a02 vgabios.c:2583
    mov bx, word [bp+00ch]                    ; 8b 5e 0c                    ; 0xc3a04 vgabios.c:2585
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc3a07
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc3a0a
    call 0309eh                               ; e8 8e f6                    ; 0xc3a0d
    jmp short 03a1eh                          ; eb 0c                       ; 0xc3a10 vgabios.c:2586
    mov bx, word [bp+00ch]                    ; 8b 5e 0c                    ; 0xc3a12 vgabios.c:2588
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc3a15
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc3a18
    call 03376h                               ; e8 58 f9                    ; 0xc3a1b
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3a1e vgabios.c:2595
    xor al, al                                ; 30 c0                       ; 0xc3a21
    or AL, strict byte 01ch                   ; 0c 1c                       ; 0xc3a23
    jmp near 03752h                           ; e9 2a fd                    ; 0xc3a25
    call 007e8h                               ; e8 bd cd                    ; 0xc3a28 vgabios.c:2600
    test ax, ax                               ; 85 c0                       ; 0xc3a2b
    je short 03aa3h                           ; 74 74                       ; 0xc3a2d
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3a2f vgabios.c:2601
    xor ah, ah                                ; 30 e4                       ; 0xc3a32
    cmp ax, strict word 0000ah                ; 3d 0a 00                    ; 0xc3a34
    jnbe short 03aa5h                         ; 77 6c                       ; 0xc3a37
    push CS                                   ; 0e                          ; 0xc3a39
    pop ES                                    ; 07                          ; 0xc3a3a
    mov cx, strict word 00008h                ; b9 08 00                    ; 0xc3a3b
    mov di, 036d9h                            ; bf d9 36                    ; 0xc3a3e
    repne scasb                               ; f2 ae                       ; 0xc3a41
    sal cx, 1                                 ; d1 e1                       ; 0xc3a43
    mov di, cx                                ; 89 cf                       ; 0xc3a45
    mov ax, word [cs:di+036e0h]               ; 2e 8b 85 e0 36              ; 0xc3a47
    jmp ax                                    ; ff e0                       ; 0xc3a4c
    mov bx, si                                ; 89 f3                       ; 0xc3a4e vgabios.c:2604
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc3a50
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3a53
    call 03c9bh                               ; e8 42 02                    ; 0xc3a56
    jmp near 03ae1h                           ; e9 85 00                    ; 0xc3a59 vgabios.c:2605
    mov cx, si                                ; 89 f1                       ; 0xc3a5c vgabios.c:2607
    mov bx, word [bp+016h]                    ; 8b 5e 16                    ; 0xc3a5e
    mov dx, word [bp+010h]                    ; 8b 56 10                    ; 0xc3a61
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3a64
    call 03dc6h                               ; e8 5c 03                    ; 0xc3a67
    jmp near 03ae1h                           ; e9 74 00                    ; 0xc3a6a vgabios.c:2608
    mov cx, si                                ; 89 f1                       ; 0xc3a6d vgabios.c:2610
    mov bx, word [bp+016h]                    ; 8b 5e 16                    ; 0xc3a6f
    mov dx, word [bp+00ch]                    ; 8b 56 0c                    ; 0xc3a72
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3a75
    call 03e65h                               ; e8 ea 03                    ; 0xc3a78
    jmp short 03ae1h                          ; eb 64                       ; 0xc3a7b vgabios.c:2611
    lea ax, [bp+00ch]                         ; 8d 46 0c                    ; 0xc3a7d vgabios.c:2613
    push ax                                   ; 50                          ; 0xc3a80
    mov cx, word [bp+016h]                    ; 8b 4e 16                    ; 0xc3a81
    mov bx, word [bp+00eh]                    ; 8b 5e 0e                    ; 0xc3a84
    mov dx, word [bp+010h]                    ; 8b 56 10                    ; 0xc3a87
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3a8a
    call 0402eh                               ; e8 9e 05                    ; 0xc3a8d
    jmp short 03ae1h                          ; eb 4f                       ; 0xc3a90 vgabios.c:2614
    lea cx, [bp+00eh]                         ; 8d 4e 0e                    ; 0xc3a92 vgabios.c:2616
    lea bx, [bp+010h]                         ; 8d 5e 10                    ; 0xc3a95
    lea dx, [bp+00ch]                         ; 8d 56 0c                    ; 0xc3a98
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3a9b
    call 040bbh                               ; e8 1a 06                    ; 0xc3a9e
    jmp short 03ae1h                          ; eb 3e                       ; 0xc3aa1 vgabios.c:2617
    jmp short 03aach                          ; eb 07                       ; 0xc3aa3
    mov word [bp+012h], 00100h                ; c7 46 12 00 01              ; 0xc3aa5 vgabios.c:2639
    jmp short 03ae1h                          ; eb 35                       ; 0xc3aaa vgabios.c:2642
    mov word [bp+012h], 00100h                ; c7 46 12 00 01              ; 0xc3aac vgabios.c:2644
    jmp short 03ae1h                          ; eb 2e                       ; 0xc3ab1 vgabios.c:2646
    call 007e8h                               ; e8 32 cd                    ; 0xc3ab3 vgabios.c:2648
    test ax, ax                               ; 85 c0                       ; 0xc3ab6
    je short 03adch                           ; 74 22                       ; 0xc3ab8
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3aba vgabios.c:2649
    xor ah, ah                                ; 30 e4                       ; 0xc3abd
    cmp ax, strict word 00042h                ; 3d 42 00                    ; 0xc3abf
    jne short 03ad5h                          ; 75 11                       ; 0xc3ac2
    lea cx, [bp+00eh]                         ; 8d 4e 0e                    ; 0xc3ac4 vgabios.c:2652
    lea bx, [bp+010h]                         ; 8d 5e 10                    ; 0xc3ac7
    lea dx, [bp+00ch]                         ; 8d 56 0c                    ; 0xc3aca
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3acd
    call 0419dh                               ; e8 ca 06                    ; 0xc3ad0
    jmp short 03ae1h                          ; eb 0c                       ; 0xc3ad3 vgabios.c:2653
    mov word [bp+012h], 00100h                ; c7 46 12 00 01              ; 0xc3ad5 vgabios.c:2655
    jmp short 03ae1h                          ; eb 05                       ; 0xc3ada vgabios.c:2658
    mov word [bp+012h], 00100h                ; c7 46 12 00 01              ; 0xc3adc vgabios.c:2660
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3ae1 vgabios.c:2670
    pop di                                    ; 5f                          ; 0xc3ae4
    pop si                                    ; 5e                          ; 0xc3ae5
    pop bp                                    ; 5d                          ; 0xc3ae6
    retn                                      ; c3                          ; 0xc3ae7
  ; disGetNextSymbol 0xc3ae8 LB 0x7ac -> off=0x0 cb=000000000000001f uValue=00000000000c3ae8 'dispi_set_xres'
dispi_set_xres:                              ; 0xc3ae8 LB 0x1f
    push bp                                   ; 55                          ; 0xc3ae8 vbe.c:100
    mov bp, sp                                ; 89 e5                       ; 0xc3ae9
    push bx                                   ; 53                          ; 0xc3aeb
    push dx                                   ; 52                          ; 0xc3aec
    mov bx, ax                                ; 89 c3                       ; 0xc3aed
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc3aef vbe.c:105
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3af2
    call 00590h                               ; e8 98 ca                    ; 0xc3af5
    mov ax, bx                                ; 89 d8                       ; 0xc3af8 vbe.c:106
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3afa
    call 00590h                               ; e8 90 ca                    ; 0xc3afd
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3b00 vbe.c:107
    pop dx                                    ; 5a                          ; 0xc3b03
    pop bx                                    ; 5b                          ; 0xc3b04
    pop bp                                    ; 5d                          ; 0xc3b05
    retn                                      ; c3                          ; 0xc3b06
  ; disGetNextSymbol 0xc3b07 LB 0x78d -> off=0x0 cb=000000000000001f uValue=00000000000c3b07 'dispi_set_yres'
dispi_set_yres:                              ; 0xc3b07 LB 0x1f
    push bp                                   ; 55                          ; 0xc3b07 vbe.c:109
    mov bp, sp                                ; 89 e5                       ; 0xc3b08
    push bx                                   ; 53                          ; 0xc3b0a
    push dx                                   ; 52                          ; 0xc3b0b
    mov bx, ax                                ; 89 c3                       ; 0xc3b0c
    mov ax, strict word 00002h                ; b8 02 00                    ; 0xc3b0e vbe.c:114
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3b11
    call 00590h                               ; e8 79 ca                    ; 0xc3b14
    mov ax, bx                                ; 89 d8                       ; 0xc3b17 vbe.c:115
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3b19
    call 00590h                               ; e8 71 ca                    ; 0xc3b1c
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3b1f vbe.c:116
    pop dx                                    ; 5a                          ; 0xc3b22
    pop bx                                    ; 5b                          ; 0xc3b23
    pop bp                                    ; 5d                          ; 0xc3b24
    retn                                      ; c3                          ; 0xc3b25
  ; disGetNextSymbol 0xc3b26 LB 0x76e -> off=0x0 cb=0000000000000019 uValue=00000000000c3b26 'dispi_get_yres'
dispi_get_yres:                              ; 0xc3b26 LB 0x19
    push bp                                   ; 55                          ; 0xc3b26 vbe.c:118
    mov bp, sp                                ; 89 e5                       ; 0xc3b27
    push dx                                   ; 52                          ; 0xc3b29
    mov ax, strict word 00002h                ; b8 02 00                    ; 0xc3b2a vbe.c:120
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3b2d
    call 00590h                               ; e8 5d ca                    ; 0xc3b30
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3b33 vbe.c:121
    call 00597h                               ; e8 5e ca                    ; 0xc3b36
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3b39 vbe.c:122
    pop dx                                    ; 5a                          ; 0xc3b3c
    pop bp                                    ; 5d                          ; 0xc3b3d
    retn                                      ; c3                          ; 0xc3b3e
  ; disGetNextSymbol 0xc3b3f LB 0x755 -> off=0x0 cb=000000000000001f uValue=00000000000c3b3f 'dispi_set_bpp'
dispi_set_bpp:                               ; 0xc3b3f LB 0x1f
    push bp                                   ; 55                          ; 0xc3b3f vbe.c:124
    mov bp, sp                                ; 89 e5                       ; 0xc3b40
    push bx                                   ; 53                          ; 0xc3b42
    push dx                                   ; 52                          ; 0xc3b43
    mov bx, ax                                ; 89 c3                       ; 0xc3b44
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc3b46 vbe.c:129
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3b49
    call 00590h                               ; e8 41 ca                    ; 0xc3b4c
    mov ax, bx                                ; 89 d8                       ; 0xc3b4f vbe.c:130
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3b51
    call 00590h                               ; e8 39 ca                    ; 0xc3b54
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3b57 vbe.c:131
    pop dx                                    ; 5a                          ; 0xc3b5a
    pop bx                                    ; 5b                          ; 0xc3b5b
    pop bp                                    ; 5d                          ; 0xc3b5c
    retn                                      ; c3                          ; 0xc3b5d
  ; disGetNextSymbol 0xc3b5e LB 0x736 -> off=0x0 cb=0000000000000019 uValue=00000000000c3b5e 'dispi_get_bpp'
dispi_get_bpp:                               ; 0xc3b5e LB 0x19
    push bp                                   ; 55                          ; 0xc3b5e vbe.c:133
    mov bp, sp                                ; 89 e5                       ; 0xc3b5f
    push dx                                   ; 52                          ; 0xc3b61
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc3b62 vbe.c:135
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3b65
    call 00590h                               ; e8 25 ca                    ; 0xc3b68
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3b6b vbe.c:136
    call 00597h                               ; e8 26 ca                    ; 0xc3b6e
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3b71 vbe.c:137
    pop dx                                    ; 5a                          ; 0xc3b74
    pop bp                                    ; 5d                          ; 0xc3b75
    retn                                      ; c3                          ; 0xc3b76
  ; disGetNextSymbol 0xc3b77 LB 0x71d -> off=0x0 cb=000000000000001f uValue=00000000000c3b77 'dispi_set_virt_width'
dispi_set_virt_width:                        ; 0xc3b77 LB 0x1f
    push bp                                   ; 55                          ; 0xc3b77 vbe.c:139
    mov bp, sp                                ; 89 e5                       ; 0xc3b78
    push bx                                   ; 53                          ; 0xc3b7a
    push dx                                   ; 52                          ; 0xc3b7b
    mov bx, ax                                ; 89 c3                       ; 0xc3b7c
    mov ax, strict word 00006h                ; b8 06 00                    ; 0xc3b7e vbe.c:144
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3b81
    call 00590h                               ; e8 09 ca                    ; 0xc3b84
    mov ax, bx                                ; 89 d8                       ; 0xc3b87 vbe.c:145
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3b89
    call 00590h                               ; e8 01 ca                    ; 0xc3b8c
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3b8f vbe.c:146
    pop dx                                    ; 5a                          ; 0xc3b92
    pop bx                                    ; 5b                          ; 0xc3b93
    pop bp                                    ; 5d                          ; 0xc3b94
    retn                                      ; c3                          ; 0xc3b95
  ; disGetNextSymbol 0xc3b96 LB 0x6fe -> off=0x0 cb=0000000000000019 uValue=00000000000c3b96 'dispi_get_virt_width'
dispi_get_virt_width:                        ; 0xc3b96 LB 0x19
    push bp                                   ; 55                          ; 0xc3b96 vbe.c:148
    mov bp, sp                                ; 89 e5                       ; 0xc3b97
    push dx                                   ; 52                          ; 0xc3b99
    mov ax, strict word 00006h                ; b8 06 00                    ; 0xc3b9a vbe.c:150
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3b9d
    call 00590h                               ; e8 ed c9                    ; 0xc3ba0
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3ba3 vbe.c:151
    call 00597h                               ; e8 ee c9                    ; 0xc3ba6
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3ba9 vbe.c:152
    pop dx                                    ; 5a                          ; 0xc3bac
    pop bp                                    ; 5d                          ; 0xc3bad
    retn                                      ; c3                          ; 0xc3bae
  ; disGetNextSymbol 0xc3baf LB 0x6e5 -> off=0x0 cb=0000000000000019 uValue=00000000000c3baf 'dispi_get_virt_height'
dispi_get_virt_height:                       ; 0xc3baf LB 0x19
    push bp                                   ; 55                          ; 0xc3baf vbe.c:154
    mov bp, sp                                ; 89 e5                       ; 0xc3bb0
    push dx                                   ; 52                          ; 0xc3bb2
    mov ax, strict word 00007h                ; b8 07 00                    ; 0xc3bb3 vbe.c:156
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3bb6
    call 00590h                               ; e8 d4 c9                    ; 0xc3bb9
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3bbc vbe.c:157
    call 00597h                               ; e8 d5 c9                    ; 0xc3bbf
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3bc2 vbe.c:158
    pop dx                                    ; 5a                          ; 0xc3bc5
    pop bp                                    ; 5d                          ; 0xc3bc6
    retn                                      ; c3                          ; 0xc3bc7
  ; disGetNextSymbol 0xc3bc8 LB 0x6cc -> off=0x0 cb=0000000000000012 uValue=00000000000c3bc8 'in_word'
in_word:                                     ; 0xc3bc8 LB 0x12
    push bp                                   ; 55                          ; 0xc3bc8 vbe.c:160
    mov bp, sp                                ; 89 e5                       ; 0xc3bc9
    push bx                                   ; 53                          ; 0xc3bcb
    mov bx, ax                                ; 89 c3                       ; 0xc3bcc
    mov ax, dx                                ; 89 d0                       ; 0xc3bce
    mov dx, bx                                ; 89 da                       ; 0xc3bd0 vbe.c:162
    out DX, ax                                ; ef                          ; 0xc3bd2
    in ax, DX                                 ; ed                          ; 0xc3bd3 vbe.c:163
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3bd4 vbe.c:164
    pop bx                                    ; 5b                          ; 0xc3bd7
    pop bp                                    ; 5d                          ; 0xc3bd8
    retn                                      ; c3                          ; 0xc3bd9
  ; disGetNextSymbol 0xc3bda LB 0x6ba -> off=0x0 cb=0000000000000014 uValue=00000000000c3bda 'in_byte'
in_byte:                                     ; 0xc3bda LB 0x14
    push bp                                   ; 55                          ; 0xc3bda vbe.c:166
    mov bp, sp                                ; 89 e5                       ; 0xc3bdb
    push bx                                   ; 53                          ; 0xc3bdd
    mov bx, ax                                ; 89 c3                       ; 0xc3bde
    mov ax, dx                                ; 89 d0                       ; 0xc3be0
    mov dx, bx                                ; 89 da                       ; 0xc3be2 vbe.c:168
    out DX, ax                                ; ef                          ; 0xc3be4
    in AL, DX                                 ; ec                          ; 0xc3be5 vbe.c:169
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3be6
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3be8 vbe.c:170
    pop bx                                    ; 5b                          ; 0xc3beb
    pop bp                                    ; 5d                          ; 0xc3bec
    retn                                      ; c3                          ; 0xc3bed
  ; disGetNextSymbol 0xc3bee LB 0x6a6 -> off=0x0 cb=0000000000000014 uValue=00000000000c3bee 'dispi_get_id'
dispi_get_id:                                ; 0xc3bee LB 0x14
    push bp                                   ; 55                          ; 0xc3bee vbe.c:173
    mov bp, sp                                ; 89 e5                       ; 0xc3bef
    push dx                                   ; 52                          ; 0xc3bf1
    xor ax, ax                                ; 31 c0                       ; 0xc3bf2 vbe.c:175
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3bf4
    out DX, ax                                ; ef                          ; 0xc3bf7
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3bf8 vbe.c:176
    in ax, DX                                 ; ed                          ; 0xc3bfb
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3bfc vbe.c:177
    pop dx                                    ; 5a                          ; 0xc3bff
    pop bp                                    ; 5d                          ; 0xc3c00
    retn                                      ; c3                          ; 0xc3c01
  ; disGetNextSymbol 0xc3c02 LB 0x692 -> off=0x0 cb=000000000000001a uValue=00000000000c3c02 'dispi_set_id'
dispi_set_id:                                ; 0xc3c02 LB 0x1a
    push bp                                   ; 55                          ; 0xc3c02 vbe.c:179
    mov bp, sp                                ; 89 e5                       ; 0xc3c03
    push bx                                   ; 53                          ; 0xc3c05
    push dx                                   ; 52                          ; 0xc3c06
    mov bx, ax                                ; 89 c3                       ; 0xc3c07
    xor ax, ax                                ; 31 c0                       ; 0xc3c09 vbe.c:181
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3c0b
    out DX, ax                                ; ef                          ; 0xc3c0e
    mov ax, bx                                ; 89 d8                       ; 0xc3c0f vbe.c:182
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3c11
    out DX, ax                                ; ef                          ; 0xc3c14
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3c15 vbe.c:183
    pop dx                                    ; 5a                          ; 0xc3c18
    pop bx                                    ; 5b                          ; 0xc3c19
    pop bp                                    ; 5d                          ; 0xc3c1a
    retn                                      ; c3                          ; 0xc3c1b
  ; disGetNextSymbol 0xc3c1c LB 0x678 -> off=0x0 cb=000000000000002a uValue=00000000000c3c1c 'vbe_init'
vbe_init:                                    ; 0xc3c1c LB 0x2a
    push bp                                   ; 55                          ; 0xc3c1c vbe.c:188
    mov bp, sp                                ; 89 e5                       ; 0xc3c1d
    push bx                                   ; 53                          ; 0xc3c1f
    mov ax, 0b0c0h                            ; b8 c0 b0                    ; 0xc3c20 vbe.c:190
    call 03c02h                               ; e8 dc ff                    ; 0xc3c23
    call 03beeh                               ; e8 c5 ff                    ; 0xc3c26 vbe.c:191
    cmp ax, 0b0c0h                            ; 3d c0 b0                    ; 0xc3c29
    jne short 03c40h                          ; 75 12                       ; 0xc3c2c
    mov bx, 000b9h                            ; bb b9 00                    ; 0xc3c2e vbe.c:42
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3c31
    mov es, ax                                ; 8e c0                       ; 0xc3c34
    mov byte [es:bx], 001h                    ; 26 c6 07 01                 ; 0xc3c36
    mov ax, 0b0c4h                            ; b8 c4 b0                    ; 0xc3c3a vbe.c:194
    call 03c02h                               ; e8 c2 ff                    ; 0xc3c3d
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3c40 vbe.c:199
    pop bx                                    ; 5b                          ; 0xc3c43
    pop bp                                    ; 5d                          ; 0xc3c44
    retn                                      ; c3                          ; 0xc3c45
  ; disGetNextSymbol 0xc3c46 LB 0x64e -> off=0x0 cb=0000000000000055 uValue=00000000000c3c46 'mode_info_find_mode'
mode_info_find_mode:                         ; 0xc3c46 LB 0x55
    push bp                                   ; 55                          ; 0xc3c46 vbe.c:202
    mov bp, sp                                ; 89 e5                       ; 0xc3c47
    push bx                                   ; 53                          ; 0xc3c49
    push cx                                   ; 51                          ; 0xc3c4a
    push si                                   ; 56                          ; 0xc3c4b
    push di                                   ; 57                          ; 0xc3c4c
    mov di, ax                                ; 89 c7                       ; 0xc3c4d
    mov si, dx                                ; 89 d6                       ; 0xc3c4f
    xor dx, dx                                ; 31 d2                       ; 0xc3c51 vbe.c:208
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3c53
    call 03bc8h                               ; e8 6f ff                    ; 0xc3c56
    cmp ax, 077cch                            ; 3d cc 77                    ; 0xc3c59 vbe.c:209
    jne short 03c90h                          ; 75 32                       ; 0xc3c5c
    mov bx, strict word 00004h                ; bb 04 00                    ; 0xc3c5e vbe.c:213
    mov dx, bx                                ; 89 da                       ; 0xc3c61 vbe.c:218
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3c63
    call 03bc8h                               ; e8 5f ff                    ; 0xc3c66
    mov cx, ax                                ; 89 c1                       ; 0xc3c69
    cmp cx, strict byte 0ffffh                ; 83 f9 ff                    ; 0xc3c6b vbe.c:219
    je short 03c90h                           ; 74 20                       ; 0xc3c6e
    lea dx, [bx+002h]                         ; 8d 57 02                    ; 0xc3c70 vbe.c:221
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3c73
    call 03bc8h                               ; e8 4f ff                    ; 0xc3c76
    lea dx, [bx+044h]                         ; 8d 57 44                    ; 0xc3c79
    cmp cx, di                                ; 39 f9                       ; 0xc3c7c vbe.c:223
    jne short 03c8ch                          ; 75 0c                       ; 0xc3c7e
    test si, si                               ; 85 f6                       ; 0xc3c80 vbe.c:225
    jne short 03c88h                          ; 75 04                       ; 0xc3c82
    mov ax, bx                                ; 89 d8                       ; 0xc3c84 vbe.c:226
    jmp short 03c92h                          ; eb 0a                       ; 0xc3c86
    test AL, strict byte 080h                 ; a8 80                       ; 0xc3c88 vbe.c:227
    jne short 03c84h                          ; 75 f8                       ; 0xc3c8a
    mov bx, dx                                ; 89 d3                       ; 0xc3c8c vbe.c:230
    jmp short 03c63h                          ; eb d3                       ; 0xc3c8e vbe.c:235
    xor ax, ax                                ; 31 c0                       ; 0xc3c90 vbe.c:238
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc3c92 vbe.c:239
    pop di                                    ; 5f                          ; 0xc3c95
    pop si                                    ; 5e                          ; 0xc3c96
    pop cx                                    ; 59                          ; 0xc3c97
    pop bx                                    ; 5b                          ; 0xc3c98
    pop bp                                    ; 5d                          ; 0xc3c99
    retn                                      ; c3                          ; 0xc3c9a
  ; disGetNextSymbol 0xc3c9b LB 0x5f9 -> off=0x0 cb=000000000000012b uValue=00000000000c3c9b 'vbe_biosfn_return_controller_information'
vbe_biosfn_return_controller_information: ; 0xc3c9b LB 0x12b
    push bp                                   ; 55                          ; 0xc3c9b vbe.c:270
    mov bp, sp                                ; 89 e5                       ; 0xc3c9c
    push cx                                   ; 51                          ; 0xc3c9e
    push si                                   ; 56                          ; 0xc3c9f
    push di                                   ; 57                          ; 0xc3ca0
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc3ca1
    mov si, ax                                ; 89 c6                       ; 0xc3ca4
    mov word [bp-00ah], dx                    ; 89 56 f6                    ; 0xc3ca6
    mov di, bx                                ; 89 df                       ; 0xc3ca9
    mov word [bp-00ch], strict word 00022h    ; c7 46 f4 22 00              ; 0xc3cab vbe.c:275
    call 005dah                               ; e8 27 c9                    ; 0xc3cb0 vbe.c:278
    mov word [bp-010h], ax                    ; 89 46 f0                    ; 0xc3cb3
    mov bx, di                                ; 89 fb                       ; 0xc3cb6 vbe.c:281
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc3cb8
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc3cbb
    xor dx, dx                                ; 31 d2                       ; 0xc3cbe vbe.c:284
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3cc0
    call 03bc8h                               ; e8 02 ff                    ; 0xc3cc3
    cmp ax, 077cch                            ; 3d cc 77                    ; 0xc3cc6 vbe.c:285
    je short 03cd5h                           ; 74 0a                       ; 0xc3cc9
    push SS                                   ; 16                          ; 0xc3ccb vbe.c:287
    pop ES                                    ; 07                          ; 0xc3ccc
    mov word [es:si], 00100h                  ; 26 c7 04 00 01              ; 0xc3ccd
    jmp near 03dbeh                           ; e9 e9 00                    ; 0xc3cd2 vbe.c:291
    mov cx, strict word 00004h                ; b9 04 00                    ; 0xc3cd5 vbe.c:293
    mov word [bp-00eh], strict word 00000h    ; c7 46 f2 00 00              ; 0xc3cd8 vbe.c:300
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc3cdd vbe.c:308
    cmp word [es:bx+002h], 03245h             ; 26 81 7f 02 45 32           ; 0xc3ce0
    jne short 03cefh                          ; 75 07                       ; 0xc3ce6
    cmp word [es:bx], 04256h                  ; 26 81 3f 56 42              ; 0xc3ce8
    je short 03cfeh                           ; 74 0f                       ; 0xc3ced
    cmp word [es:bx+002h], 04153h             ; 26 81 7f 02 53 41           ; 0xc3cef
    jne short 03d03h                          ; 75 0c                       ; 0xc3cf5
    cmp word [es:bx], 04556h                  ; 26 81 3f 56 45              ; 0xc3cf7
    jne short 03d03h                          ; 75 05                       ; 0xc3cfc
    mov word [bp-00eh], strict word 00001h    ; c7 46 f2 01 00              ; 0xc3cfe vbe.c:310
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc3d03 vbe.c:318
    mov word [es:bx], 04556h                  ; 26 c7 07 56 45              ; 0xc3d06
    mov word [es:bx+002h], 04153h             ; 26 c7 47 02 53 41           ; 0xc3d0b vbe.c:320
    mov word [es:bx+004h], 00200h             ; 26 c7 47 04 00 02           ; 0xc3d11 vbe.c:324
    mov word [es:bx+006h], 07de6h             ; 26 c7 47 06 e6 7d           ; 0xc3d17 vbe.c:327
    mov [es:bx+008h], ds                      ; 26 8c 5f 08                 ; 0xc3d1d
    mov word [es:bx+00ah], strict word 00001h ; 26 c7 47 0a 01 00           ; 0xc3d21 vbe.c:330
    mov word [es:bx+00ch], strict word 00000h ; 26 c7 47 0c 00 00           ; 0xc3d27 vbe.c:332
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc3d2d vbe.c:336
    mov word [es:bx+010h], ax                 ; 26 89 47 10                 ; 0xc3d30
    lea ax, [di+022h]                         ; 8d 45 22                    ; 0xc3d34 vbe.c:337
    mov word [es:bx+00eh], ax                 ; 26 89 47 0e                 ; 0xc3d37
    mov dx, strict word 0ffffh                ; ba ff ff                    ; 0xc3d3b vbe.c:340
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3d3e
    call 03bc8h                               ; e8 84 fe                    ; 0xc3d41
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc3d44
    mov word [es:bx+012h], ax                 ; 26 89 47 12                 ; 0xc3d47
    cmp word [bp-00eh], strict byte 00000h    ; 83 7e f2 00                 ; 0xc3d4b vbe.c:342
    je short 03d75h                           ; 74 24                       ; 0xc3d4f
    mov word [es:bx+014h], strict word 00003h ; 26 c7 47 14 03 00           ; 0xc3d51 vbe.c:345
    mov word [es:bx+016h], 07dfbh             ; 26 c7 47 16 fb 7d           ; 0xc3d57 vbe.c:346
    mov [es:bx+018h], ds                      ; 26 8c 5f 18                 ; 0xc3d5d
    mov word [es:bx+01ah], 07e0eh             ; 26 c7 47 1a 0e 7e           ; 0xc3d61 vbe.c:347
    mov [es:bx+01ch], ds                      ; 26 8c 5f 1c                 ; 0xc3d67
    mov word [es:bx+01eh], 07e2fh             ; 26 c7 47 1e 2f 7e           ; 0xc3d6b vbe.c:348
    mov [es:bx+020h], ds                      ; 26 8c 5f 20                 ; 0xc3d71
    mov dx, cx                                ; 89 ca                       ; 0xc3d75 vbe.c:355
    add dx, strict byte 0001bh                ; 83 c2 1b                    ; 0xc3d77
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3d7a
    call 03bdah                               ; e8 5a fe                    ; 0xc3d7d
    xor ah, ah                                ; 30 e4                       ; 0xc3d80 vbe.c:356
    cmp ax, word [bp-010h]                    ; 3b 46 f0                    ; 0xc3d82
    jnbe short 03d9eh                         ; 77 17                       ; 0xc3d85
    mov dx, cx                                ; 89 ca                       ; 0xc3d87 vbe.c:358
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3d89
    call 03bc8h                               ; e8 39 fe                    ; 0xc3d8c
    mov bx, word [bp-00ch]                    ; 8b 5e f4                    ; 0xc3d8f vbe.c:362
    add bx, di                                ; 01 fb                       ; 0xc3d92
    mov es, [bp-00ah]                         ; 8e 46 f6                    ; 0xc3d94 vbe.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3d97
    add word [bp-00ch], strict byte 00002h    ; 83 46 f4 02                 ; 0xc3d9a vbe.c:364
    add cx, strict byte 00044h                ; 83 c1 44                    ; 0xc3d9e vbe.c:366
    mov dx, cx                                ; 89 ca                       ; 0xc3da1 vbe.c:367
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3da3
    call 03bc8h                               ; e8 1f fe                    ; 0xc3da6
    cmp ax, strict word 0ffffh                ; 3d ff ff                    ; 0xc3da9 vbe.c:368
    jne short 03d75h                          ; 75 c7                       ; 0xc3dac
    add di, word [bp-00ch]                    ; 03 7e f4                    ; 0xc3dae vbe.c:371
    mov es, [bp-00ah]                         ; 8e 46 f6                    ; 0xc3db1 vbe.c:52
    mov word [es:di], ax                      ; 26 89 05                    ; 0xc3db4
    push SS                                   ; 16                          ; 0xc3db7 vbe.c:372
    pop ES                                    ; 07                          ; 0xc3db8
    mov word [es:si], strict word 0004fh      ; 26 c7 04 4f 00              ; 0xc3db9
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc3dbe vbe.c:373
    pop di                                    ; 5f                          ; 0xc3dc1
    pop si                                    ; 5e                          ; 0xc3dc2
    pop cx                                    ; 59                          ; 0xc3dc3
    pop bp                                    ; 5d                          ; 0xc3dc4
    retn                                      ; c3                          ; 0xc3dc5
  ; disGetNextSymbol 0xc3dc6 LB 0x4ce -> off=0x0 cb=000000000000009f uValue=00000000000c3dc6 'vbe_biosfn_return_mode_information'
vbe_biosfn_return_mode_information:          ; 0xc3dc6 LB 0x9f
    push bp                                   ; 55                          ; 0xc3dc6 vbe.c:385
    mov bp, sp                                ; 89 e5                       ; 0xc3dc7
    push si                                   ; 56                          ; 0xc3dc9
    push di                                   ; 57                          ; 0xc3dca
    push ax                                   ; 50                          ; 0xc3dcb
    push ax                                   ; 50                          ; 0xc3dcc
    mov ax, dx                                ; 89 d0                       ; 0xc3dcd
    mov si, bx                                ; 89 de                       ; 0xc3dcf
    mov bx, cx                                ; 89 cb                       ; 0xc3dd1
    test dh, 040h                             ; f6 c6 40                    ; 0xc3dd3 vbe.c:396
    je short 03dddh                           ; 74 05                       ; 0xc3dd6
    mov dx, strict word 00001h                ; ba 01 00                    ; 0xc3dd8
    jmp short 03ddfh                          ; eb 02                       ; 0xc3ddb
    xor dx, dx                                ; 31 d2                       ; 0xc3ddd
    and ah, 001h                              ; 80 e4 01                    ; 0xc3ddf vbe.c:397
    call 03c46h                               ; e8 61 fe                    ; 0xc3de2 vbe.c:399
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc3de5
    test ax, ax                               ; 85 c0                       ; 0xc3de8 vbe.c:401
    je short 03e53h                           ; 74 67                       ; 0xc3dea
    mov cx, 00100h                            ; b9 00 01                    ; 0xc3dec vbe.c:406
    xor ax, ax                                ; 31 c0                       ; 0xc3def
    mov di, bx                                ; 89 df                       ; 0xc3df1
    mov es, si                                ; 8e c6                       ; 0xc3df3
    jcxz 03df9h                               ; e3 02                       ; 0xc3df5
    rep stosb                                 ; f3 aa                       ; 0xc3df7
    xor cx, cx                                ; 31 c9                       ; 0xc3df9 vbe.c:407
    jmp short 03e02h                          ; eb 05                       ; 0xc3dfb
    cmp cx, strict byte 00042h                ; 83 f9 42                    ; 0xc3dfd
    jnc short 03e1bh                          ; 73 19                       ; 0xc3e00
    mov dx, word [bp-006h]                    ; 8b 56 fa                    ; 0xc3e02 vbe.c:410
    inc dx                                    ; 42                          ; 0xc3e05
    inc dx                                    ; 42                          ; 0xc3e06
    add dx, cx                                ; 01 ca                       ; 0xc3e07
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3e09
    call 03bdah                               ; e8 cb fd                    ; 0xc3e0c
    mov di, bx                                ; 89 df                       ; 0xc3e0f vbe.c:411
    add di, cx                                ; 01 cf                       ; 0xc3e11
    mov es, si                                ; 8e c6                       ; 0xc3e13 vbe.c:42
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc3e15
    inc cx                                    ; 41                          ; 0xc3e18 vbe.c:412
    jmp short 03dfdh                          ; eb e2                       ; 0xc3e19
    lea di, [bx+002h]                         ; 8d 7f 02                    ; 0xc3e1b vbe.c:413
    mov es, si                                ; 8e c6                       ; 0xc3e1e vbe.c:37
    mov al, byte [es:di]                      ; 26 8a 05                    ; 0xc3e20
    test AL, strict byte 001h                 ; a8 01                       ; 0xc3e23 vbe.c:414
    je short 03e37h                           ; 74 10                       ; 0xc3e25
    lea di, [bx+00ch]                         ; 8d 7f 0c                    ; 0xc3e27 vbe.c:415
    mov word [es:di], 0064ch                  ; 26 c7 05 4c 06              ; 0xc3e2a vbe.c:52
    lea di, [bx+00eh]                         ; 8d 7f 0e                    ; 0xc3e2f vbe.c:417
    mov word [es:di], 0c000h                  ; 26 c7 05 00 c0              ; 0xc3e32 vbe.c:52
    mov ax, strict word 0000bh                ; b8 0b 00                    ; 0xc3e37 vbe.c:420
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3e3a
    call 00590h                               ; e8 50 c7                    ; 0xc3e3d
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3e40 vbe.c:421
    call 00597h                               ; e8 51 c7                    ; 0xc3e43
    add bx, strict byte 0002ah                ; 83 c3 2a                    ; 0xc3e46
    mov es, si                                ; 8e c6                       ; 0xc3e49 vbe.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3e4b
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc3e4e vbe.c:423
    jmp short 03e56h                          ; eb 03                       ; 0xc3e51 vbe.c:424
    mov ax, 00100h                            ; b8 00 01                    ; 0xc3e53 vbe.c:428
    push SS                                   ; 16                          ; 0xc3e56 vbe.c:431
    pop ES                                    ; 07                          ; 0xc3e57
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc3e58
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3e5b
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3e5e vbe.c:432
    pop di                                    ; 5f                          ; 0xc3e61
    pop si                                    ; 5e                          ; 0xc3e62
    pop bp                                    ; 5d                          ; 0xc3e63
    retn                                      ; c3                          ; 0xc3e64
  ; disGetNextSymbol 0xc3e65 LB 0x42f -> off=0x0 cb=00000000000000e7 uValue=00000000000c3e65 'vbe_biosfn_set_mode'
vbe_biosfn_set_mode:                         ; 0xc3e65 LB 0xe7
    push bp                                   ; 55                          ; 0xc3e65 vbe.c:444
    mov bp, sp                                ; 89 e5                       ; 0xc3e66
    push si                                   ; 56                          ; 0xc3e68
    push di                                   ; 57                          ; 0xc3e69
    sub sp, strict byte 00006h                ; 83 ec 06                    ; 0xc3e6a
    mov si, ax                                ; 89 c6                       ; 0xc3e6d
    mov word [bp-00ah], dx                    ; 89 56 f6                    ; 0xc3e6f
    test byte [bp-009h], 040h                 ; f6 46 f7 40                 ; 0xc3e72 vbe.c:452
    je short 03e7dh                           ; 74 05                       ; 0xc3e76
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc3e78
    jmp short 03e7fh                          ; eb 02                       ; 0xc3e7b
    xor ax, ax                                ; 31 c0                       ; 0xc3e7d
    mov dx, ax                                ; 89 c2                       ; 0xc3e7f
    test ax, ax                               ; 85 c0                       ; 0xc3e81 vbe.c:453
    je short 03e88h                           ; 74 03                       ; 0xc3e83
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3e85
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc3e88
    test byte [bp-009h], 080h                 ; f6 46 f7 80                 ; 0xc3e8b vbe.c:454
    je short 03e96h                           ; 74 05                       ; 0xc3e8f
    mov ax, 00080h                            ; b8 80 00                    ; 0xc3e91
    jmp short 03e98h                          ; eb 02                       ; 0xc3e94
    xor ax, ax                                ; 31 c0                       ; 0xc3e96
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc3e98
    and byte [bp-009h], 001h                  ; 80 66 f7 01                 ; 0xc3e9b vbe.c:456
    cmp word [bp-00ah], 00100h                ; 81 7e f6 00 01              ; 0xc3e9f vbe.c:459
    jnc short 03eb9h                          ; 73 13                       ; 0xc3ea4
    xor ax, ax                                ; 31 c0                       ; 0xc3ea6 vbe.c:463
    call 00600h                               ; e8 55 c7                    ; 0xc3ea8
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc3eab vbe.c:467
    xor ah, ah                                ; 30 e4                       ; 0xc3eae
    call 013b5h                               ; e8 02 d5                    ; 0xc3eb0
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc3eb3 vbe.c:468
    jmp near 03f40h                           ; e9 87 00                    ; 0xc3eb6 vbe.c:469
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc3eb9 vbe.c:472
    call 03c46h                               ; e8 87 fd                    ; 0xc3ebc
    mov bx, ax                                ; 89 c3                       ; 0xc3ebf
    test ax, ax                               ; 85 c0                       ; 0xc3ec1 vbe.c:474
    je short 03f3dh                           ; 74 78                       ; 0xc3ec3
    lea dx, [bx+014h]                         ; 8d 57 14                    ; 0xc3ec5 vbe.c:479
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3ec8
    call 03bc8h                               ; e8 fa fc                    ; 0xc3ecb
    mov cx, ax                                ; 89 c1                       ; 0xc3ece
    lea dx, [bx+016h]                         ; 8d 57 16                    ; 0xc3ed0 vbe.c:480
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3ed3
    call 03bc8h                               ; e8 ef fc                    ; 0xc3ed6
    mov di, ax                                ; 89 c7                       ; 0xc3ed9
    lea dx, [bx+01bh]                         ; 8d 57 1b                    ; 0xc3edb vbe.c:481
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3ede
    call 03bdah                               ; e8 f6 fc                    ; 0xc3ee1
    mov bl, al                                ; 88 c3                       ; 0xc3ee4
    mov dl, al                                ; 88 c2                       ; 0xc3ee6
    xor ax, ax                                ; 31 c0                       ; 0xc3ee8 vbe.c:489
    call 00600h                               ; e8 13 c7                    ; 0xc3eea
    cmp bl, 004h                              ; 80 fb 04                    ; 0xc3eed vbe.c:491
    jne short 03ef8h                          ; 75 06                       ; 0xc3ef0
    mov ax, strict word 0006ah                ; b8 6a 00                    ; 0xc3ef2 vbe.c:493
    call 013b5h                               ; e8 bd d4                    ; 0xc3ef5
    mov al, dl                                ; 88 d0                       ; 0xc3ef8 vbe.c:496
    xor ah, ah                                ; 30 e4                       ; 0xc3efa
    call 03b3fh                               ; e8 40 fc                    ; 0xc3efc
    mov ax, cx                                ; 89 c8                       ; 0xc3eff vbe.c:497
    call 03ae8h                               ; e8 e4 fb                    ; 0xc3f01
    mov ax, di                                ; 89 f8                       ; 0xc3f04 vbe.c:498
    call 03b07h                               ; e8 fe fb                    ; 0xc3f06
    xor ax, ax                                ; 31 c0                       ; 0xc3f09 vbe.c:499
    call 00626h                               ; e8 18 c7                    ; 0xc3f0b
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc3f0e vbe.c:500
    or dl, 001h                               ; 80 ca 01                    ; 0xc3f11
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc3f14
    xor ah, ah                                ; 30 e4                       ; 0xc3f17
    or al, dl                                 ; 08 d0                       ; 0xc3f19
    call 00600h                               ; e8 e2 c6                    ; 0xc3f1b
    call 006f8h                               ; e8 d7 c7                    ; 0xc3f1e vbe.c:501
    mov bx, 000bah                            ; bb ba 00                    ; 0xc3f21 vbe.c:52
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3f24
    mov es, ax                                ; 8e c0                       ; 0xc3f27
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc3f29
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3f2c
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc3f2f vbe.c:504
    or AL, strict byte 060h                   ; 0c 60                       ; 0xc3f32
    mov bx, 00087h                            ; bb 87 00                    ; 0xc3f34 vbe.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3f37
    jmp near 03eb3h                           ; e9 76 ff                    ; 0xc3f3a
    mov ax, 00100h                            ; b8 00 01                    ; 0xc3f3d vbe.c:513
    push SS                                   ; 16                          ; 0xc3f40 vbe.c:517
    pop ES                                    ; 07                          ; 0xc3f41
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3f42
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3f45 vbe.c:518
    pop di                                    ; 5f                          ; 0xc3f48
    pop si                                    ; 5e                          ; 0xc3f49
    pop bp                                    ; 5d                          ; 0xc3f4a
    retn                                      ; c3                          ; 0xc3f4b
  ; disGetNextSymbol 0xc3f4c LB 0x348 -> off=0x0 cb=0000000000000008 uValue=00000000000c3f4c 'vbe_biosfn_read_video_state_size'
vbe_biosfn_read_video_state_size:            ; 0xc3f4c LB 0x8
    push bp                                   ; 55                          ; 0xc3f4c vbe.c:520
    mov bp, sp                                ; 89 e5                       ; 0xc3f4d
    mov ax, strict word 00012h                ; b8 12 00                    ; 0xc3f4f vbe.c:523
    pop bp                                    ; 5d                          ; 0xc3f52
    retn                                      ; c3                          ; 0xc3f53
  ; disGetNextSymbol 0xc3f54 LB 0x340 -> off=0x0 cb=000000000000004b uValue=00000000000c3f54 'vbe_biosfn_save_video_state'
vbe_biosfn_save_video_state:                 ; 0xc3f54 LB 0x4b
    push bp                                   ; 55                          ; 0xc3f54 vbe.c:525
    mov bp, sp                                ; 89 e5                       ; 0xc3f55
    push bx                                   ; 53                          ; 0xc3f57
    push cx                                   ; 51                          ; 0xc3f58
    push si                                   ; 56                          ; 0xc3f59
    mov si, ax                                ; 89 c6                       ; 0xc3f5a
    mov bx, dx                                ; 89 d3                       ; 0xc3f5c
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc3f5e vbe.c:529
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3f61
    out DX, ax                                ; ef                          ; 0xc3f64
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3f65 vbe.c:530
    in ax, DX                                 ; ed                          ; 0xc3f68
    mov es, si                                ; 8e c6                       ; 0xc3f69 vbe.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3f6b
    inc bx                                    ; 43                          ; 0xc3f6e vbe.c:532
    inc bx                                    ; 43                          ; 0xc3f6f
    test AL, strict byte 001h                 ; a8 01                       ; 0xc3f70 vbe.c:533
    je short 03f97h                           ; 74 23                       ; 0xc3f72
    mov cx, strict word 00001h                ; b9 01 00                    ; 0xc3f74 vbe.c:535
    jmp short 03f7eh                          ; eb 05                       ; 0xc3f77
    cmp cx, strict byte 00009h                ; 83 f9 09                    ; 0xc3f79
    jnbe short 03f97h                         ; 77 19                       ; 0xc3f7c
    cmp cx, strict byte 00004h                ; 83 f9 04                    ; 0xc3f7e vbe.c:536
    je short 03f94h                           ; 74 11                       ; 0xc3f81
    mov ax, cx                                ; 89 c8                       ; 0xc3f83 vbe.c:537
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3f85
    out DX, ax                                ; ef                          ; 0xc3f88
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3f89 vbe.c:538
    in ax, DX                                 ; ed                          ; 0xc3f8c
    mov es, si                                ; 8e c6                       ; 0xc3f8d vbe.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3f8f
    inc bx                                    ; 43                          ; 0xc3f92 vbe.c:539
    inc bx                                    ; 43                          ; 0xc3f93
    inc cx                                    ; 41                          ; 0xc3f94 vbe.c:541
    jmp short 03f79h                          ; eb e2                       ; 0xc3f95
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc3f97 vbe.c:542
    pop si                                    ; 5e                          ; 0xc3f9a
    pop cx                                    ; 59                          ; 0xc3f9b
    pop bx                                    ; 5b                          ; 0xc3f9c
    pop bp                                    ; 5d                          ; 0xc3f9d
    retn                                      ; c3                          ; 0xc3f9e
  ; disGetNextSymbol 0xc3f9f LB 0x2f5 -> off=0x0 cb=000000000000008f uValue=00000000000c3f9f 'vbe_biosfn_restore_video_state'
vbe_biosfn_restore_video_state:              ; 0xc3f9f LB 0x8f
    push bp                                   ; 55                          ; 0xc3f9f vbe.c:545
    mov bp, sp                                ; 89 e5                       ; 0xc3fa0
    push bx                                   ; 53                          ; 0xc3fa2
    push cx                                   ; 51                          ; 0xc3fa3
    push si                                   ; 56                          ; 0xc3fa4
    push ax                                   ; 50                          ; 0xc3fa5
    mov cx, ax                                ; 89 c1                       ; 0xc3fa6
    mov bx, dx                                ; 89 d3                       ; 0xc3fa8
    mov es, ax                                ; 8e c0                       ; 0xc3faa vbe.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3fac
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc3faf
    inc bx                                    ; 43                          ; 0xc3fb2 vbe.c:550
    inc bx                                    ; 43                          ; 0xc3fb3
    test byte [bp-008h], 001h                 ; f6 46 f8 01                 ; 0xc3fb4 vbe.c:552
    jne short 03fcah                          ; 75 10                       ; 0xc3fb8
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc3fba vbe.c:553
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3fbd
    out DX, ax                                ; ef                          ; 0xc3fc0
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc3fc1 vbe.c:554
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3fc4
    out DX, ax                                ; ef                          ; 0xc3fc7
    jmp short 04026h                          ; eb 5c                       ; 0xc3fc8 vbe.c:555
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc3fca vbe.c:556
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3fcd
    out DX, ax                                ; ef                          ; 0xc3fd0
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3fd1 vbe.c:47
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3fd4 vbe.c:48
    out DX, ax                                ; ef                          ; 0xc3fd7
    inc bx                                    ; 43                          ; 0xc3fd8 vbe.c:558
    inc bx                                    ; 43                          ; 0xc3fd9
    mov ax, strict word 00002h                ; b8 02 00                    ; 0xc3fda
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3fdd
    out DX, ax                                ; ef                          ; 0xc3fe0
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3fe1 vbe.c:47
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3fe4 vbe.c:48
    out DX, ax                                ; ef                          ; 0xc3fe7
    inc bx                                    ; 43                          ; 0xc3fe8 vbe.c:561
    inc bx                                    ; 43                          ; 0xc3fe9
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc3fea
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3fed
    out DX, ax                                ; ef                          ; 0xc3ff0
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3ff1 vbe.c:47
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3ff4 vbe.c:48
    out DX, ax                                ; ef                          ; 0xc3ff7
    inc bx                                    ; 43                          ; 0xc3ff8 vbe.c:564
    inc bx                                    ; 43                          ; 0xc3ff9
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc3ffa
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3ffd
    out DX, ax                                ; ef                          ; 0xc4000
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc4001 vbe.c:566
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc4004
    out DX, ax                                ; ef                          ; 0xc4007
    mov si, strict word 00005h                ; be 05 00                    ; 0xc4008 vbe.c:568
    jmp short 04012h                          ; eb 05                       ; 0xc400b
    cmp si, strict byte 00009h                ; 83 fe 09                    ; 0xc400d
    jnbe short 04026h                         ; 77 14                       ; 0xc4010
    mov ax, si                                ; 89 f0                       ; 0xc4012 vbe.c:569
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc4014
    out DX, ax                                ; ef                          ; 0xc4017
    mov es, cx                                ; 8e c1                       ; 0xc4018 vbe.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc401a
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc401d vbe.c:48
    out DX, ax                                ; ef                          ; 0xc4020
    inc bx                                    ; 43                          ; 0xc4021 vbe.c:571
    inc bx                                    ; 43                          ; 0xc4022
    inc si                                    ; 46                          ; 0xc4023 vbe.c:572
    jmp short 0400dh                          ; eb e7                       ; 0xc4024
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc4026 vbe.c:574
    pop si                                    ; 5e                          ; 0xc4029
    pop cx                                    ; 59                          ; 0xc402a
    pop bx                                    ; 5b                          ; 0xc402b
    pop bp                                    ; 5d                          ; 0xc402c
    retn                                      ; c3                          ; 0xc402d
  ; disGetNextSymbol 0xc402e LB 0x266 -> off=0x0 cb=000000000000008d uValue=00000000000c402e 'vbe_biosfn_save_restore_state'
vbe_biosfn_save_restore_state:               ; 0xc402e LB 0x8d
    push bp                                   ; 55                          ; 0xc402e vbe.c:590
    mov bp, sp                                ; 89 e5                       ; 0xc402f
    push si                                   ; 56                          ; 0xc4031
    push di                                   ; 57                          ; 0xc4032
    push ax                                   ; 50                          ; 0xc4033
    mov si, ax                                ; 89 c6                       ; 0xc4034
    mov word [bp-006h], dx                    ; 89 56 fa                    ; 0xc4036
    mov ax, bx                                ; 89 d8                       ; 0xc4039
    mov bx, word [bp+004h]                    ; 8b 5e 04                    ; 0xc403b
    mov di, strict word 0004fh                ; bf 4f 00                    ; 0xc403e vbe.c:595
    xor ah, ah                                ; 30 e4                       ; 0xc4041 vbe.c:596
    cmp ax, strict word 00002h                ; 3d 02 00                    ; 0xc4043
    je short 0408eh                           ; 74 46                       ; 0xc4046
    cmp ax, strict word 00001h                ; 3d 01 00                    ; 0xc4048
    je short 04072h                           ; 74 25                       ; 0xc404b
    test ax, ax                               ; 85 c0                       ; 0xc404d
    jne short 040aah                          ; 75 59                       ; 0xc404f
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc4051 vbe.c:598
    call 03060h                               ; e8 09 f0                    ; 0xc4054
    mov cx, ax                                ; 89 c1                       ; 0xc4057
    test byte [bp-006h], 008h                 ; f6 46 fa 08                 ; 0xc4059 vbe.c:602
    je short 04064h                           ; 74 05                       ; 0xc405d
    call 03f4ch                               ; e8 ea fe                    ; 0xc405f vbe.c:603
    add ax, cx                                ; 01 c8                       ; 0xc4062
    add ax, strict word 0003fh                ; 05 3f 00                    ; 0xc4064 vbe.c:604
    mov CL, strict byte 006h                  ; b1 06                       ; 0xc4067
    shr ax, CL                                ; d3 e8                       ; 0xc4069
    push SS                                   ; 16                          ; 0xc406b
    pop ES                                    ; 07                          ; 0xc406c
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc406d
    jmp short 040adh                          ; eb 3b                       ; 0xc4070 vbe.c:605
    push SS                                   ; 16                          ; 0xc4072 vbe.c:607
    pop ES                                    ; 07                          ; 0xc4073
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc4074
    mov dx, cx                                ; 89 ca                       ; 0xc4077 vbe.c:608
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc4079
    call 0309eh                               ; e8 1f f0                    ; 0xc407c
    test byte [bp-006h], 008h                 ; f6 46 fa 08                 ; 0xc407f vbe.c:612
    je short 040adh                           ; 74 28                       ; 0xc4083
    mov dx, ax                                ; 89 c2                       ; 0xc4085 vbe.c:613
    mov ax, cx                                ; 89 c8                       ; 0xc4087
    call 03f54h                               ; e8 c8 fe                    ; 0xc4089
    jmp short 040adh                          ; eb 1f                       ; 0xc408c vbe.c:614
    push SS                                   ; 16                          ; 0xc408e vbe.c:616
    pop ES                                    ; 07                          ; 0xc408f
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc4090
    mov dx, cx                                ; 89 ca                       ; 0xc4093 vbe.c:617
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc4095
    call 03376h                               ; e8 db f2                    ; 0xc4098
    test byte [bp-006h], 008h                 ; f6 46 fa 08                 ; 0xc409b vbe.c:621
    je short 040adh                           ; 74 0c                       ; 0xc409f
    mov dx, ax                                ; 89 c2                       ; 0xc40a1 vbe.c:622
    mov ax, cx                                ; 89 c8                       ; 0xc40a3
    call 03f9fh                               ; e8 f7 fe                    ; 0xc40a5
    jmp short 040adh                          ; eb 03                       ; 0xc40a8 vbe.c:623
    mov di, 00100h                            ; bf 00 01                    ; 0xc40aa vbe.c:626
    push SS                                   ; 16                          ; 0xc40ad vbe.c:629
    pop ES                                    ; 07                          ; 0xc40ae
    mov word [es:si], di                      ; 26 89 3c                    ; 0xc40af
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc40b2 vbe.c:630
    pop di                                    ; 5f                          ; 0xc40b5
    pop si                                    ; 5e                          ; 0xc40b6
    pop bp                                    ; 5d                          ; 0xc40b7
    retn 00002h                               ; c2 02 00                    ; 0xc40b8
  ; disGetNextSymbol 0xc40bb LB 0x1d9 -> off=0x0 cb=00000000000000e2 uValue=00000000000c40bb 'vbe_biosfn_get_set_scanline_length'
vbe_biosfn_get_set_scanline_length:          ; 0xc40bb LB 0xe2
    push bp                                   ; 55                          ; 0xc40bb vbe.c:651
    mov bp, sp                                ; 89 e5                       ; 0xc40bc
    push si                                   ; 56                          ; 0xc40be
    push di                                   ; 57                          ; 0xc40bf
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc40c0
    push ax                                   ; 50                          ; 0xc40c3
    mov di, dx                                ; 89 d7                       ; 0xc40c4
    mov word [bp-006h], bx                    ; 89 5e fa                    ; 0xc40c6
    mov si, cx                                ; 89 ce                       ; 0xc40c9
    call 03b5eh                               ; e8 90 fa                    ; 0xc40cb vbe.c:660
    cmp AL, strict byte 00fh                  ; 3c 0f                       ; 0xc40ce vbe.c:661
    jne short 040d7h                          ; 75 05                       ; 0xc40d0
    mov cx, strict word 00010h                ; b9 10 00                    ; 0xc40d2
    jmp short 040dbh                          ; eb 04                       ; 0xc40d5
    xor ah, ah                                ; 30 e4                       ; 0xc40d7
    mov cx, ax                                ; 89 c1                       ; 0xc40d9
    mov ch, cl                                ; 88 cd                       ; 0xc40db
    call 03b96h                               ; e8 b6 fa                    ; 0xc40dd vbe.c:662
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc40e0
    mov word [bp-00ch], strict word 0004fh    ; c7 46 f4 4f 00              ; 0xc40e3 vbe.c:663
    push SS                                   ; 16                          ; 0xc40e8 vbe.c:664
    pop ES                                    ; 07                          ; 0xc40e9
    mov bx, word [bp-006h]                    ; 8b 5e fa                    ; 0xc40ea
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc40ed
    mov al, byte [es:di]                      ; 26 8a 05                    ; 0xc40f0 vbe.c:665
    cmp AL, strict byte 002h                  ; 3c 02                       ; 0xc40f3 vbe.c:669
    je short 04102h                           ; 74 0b                       ; 0xc40f5
    cmp AL, strict byte 001h                  ; 3c 01                       ; 0xc40f7
    je short 0412bh                           ; 74 30                       ; 0xc40f9
    test al, al                               ; 84 c0                       ; 0xc40fb
    je short 04126h                           ; 74 27                       ; 0xc40fd
    jmp near 04186h                           ; e9 84 00                    ; 0xc40ff
    cmp ch, 004h                              ; 80 fd 04                    ; 0xc4102 vbe.c:671
    jne short 0410dh                          ; 75 06                       ; 0xc4105
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc4107 vbe.c:672
    sal bx, CL                                ; d3 e3                       ; 0xc4109
    jmp short 04126h                          ; eb 19                       ; 0xc410b vbe.c:673
    mov al, ch                                ; 88 e8                       ; 0xc410d vbe.c:674
    xor ah, ah                                ; 30 e4                       ; 0xc410f
    cwd                                       ; 99                          ; 0xc4111
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc4112
    sal dx, CL                                ; d3 e2                       ; 0xc4114
    db  01bh, 0c2h
    ; sbb ax, dx                                ; 1b c2                     ; 0xc4116
    sar ax, CL                                ; d3 f8                       ; 0xc4118
    mov word [bp-00eh], ax                    ; 89 46 f2                    ; 0xc411a
    mov ax, bx                                ; 89 d8                       ; 0xc411d
    xor dx, dx                                ; 31 d2                       ; 0xc411f
    div word [bp-00eh]                        ; f7 76 f2                    ; 0xc4121
    mov bx, ax                                ; 89 c3                       ; 0xc4124
    mov ax, bx                                ; 89 d8                       ; 0xc4126 vbe.c:677
    call 03b77h                               ; e8 4c fa                    ; 0xc4128
    call 03b96h                               ; e8 68 fa                    ; 0xc412b vbe.c:680
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc412e
    push SS                                   ; 16                          ; 0xc4131 vbe.c:681
    pop ES                                    ; 07                          ; 0xc4132
    mov bx, word [bp-006h]                    ; 8b 5e fa                    ; 0xc4133
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc4136
    cmp ch, 004h                              ; 80 fd 04                    ; 0xc4139 vbe.c:682
    jne short 04146h                          ; 75 08                       ; 0xc413c
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc413e vbe.c:683
    mov bx, ax                                ; 89 c3                       ; 0xc4140
    shr bx, CL                                ; d3 eb                       ; 0xc4142
    jmp short 0415ch                          ; eb 16                       ; 0xc4144 vbe.c:684
    mov al, ch                                ; 88 e8                       ; 0xc4146 vbe.c:685
    xor ah, ah                                ; 30 e4                       ; 0xc4148
    cwd                                       ; 99                          ; 0xc414a
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc414b
    sal dx, CL                                ; d3 e2                       ; 0xc414d
    db  01bh, 0c2h
    ; sbb ax, dx                                ; 1b c2                     ; 0xc414f
    sar ax, CL                                ; d3 f8                       ; 0xc4151
    mov bx, ax                                ; 89 c3                       ; 0xc4153
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc4155
    mul bx                                    ; f7 e3                       ; 0xc4158
    mov bx, ax                                ; 89 c3                       ; 0xc415a
    add bx, strict byte 00003h                ; 83 c3 03                    ; 0xc415c vbe.c:686
    and bl, 0fch                              ; 80 e3 fc                    ; 0xc415f
    push SS                                   ; 16                          ; 0xc4162 vbe.c:687
    pop ES                                    ; 07                          ; 0xc4163
    mov word [es:di], bx                      ; 26 89 1d                    ; 0xc4164
    call 03bafh                               ; e8 45 fa                    ; 0xc4167 vbe.c:688
    push SS                                   ; 16                          ; 0xc416a
    pop ES                                    ; 07                          ; 0xc416b
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc416c
    call 03b26h                               ; e8 b4 f9                    ; 0xc416f vbe.c:689
    push SS                                   ; 16                          ; 0xc4172
    pop ES                                    ; 07                          ; 0xc4173
    cmp ax, word [es:si]                      ; 26 3b 04                    ; 0xc4174
    jbe short 0418bh                          ; 76 12                       ; 0xc4177
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc4179 vbe.c:690
    call 03b77h                               ; e8 f8 f9                    ; 0xc417c
    mov word [bp-00ch], 00200h                ; c7 46 f4 00 02              ; 0xc417f vbe.c:691
    jmp short 0418bh                          ; eb 05                       ; 0xc4184 vbe.c:693
    mov word [bp-00ch], 00100h                ; c7 46 f4 00 01              ; 0xc4186 vbe.c:696
    push SS                                   ; 16                          ; 0xc418b vbe.c:699
    pop ES                                    ; 07                          ; 0xc418c
    mov ax, word [bp-00ch]                    ; 8b 46 f4                    ; 0xc418d
    mov bx, word [bp-010h]                    ; 8b 5e f0                    ; 0xc4190
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc4193
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc4196 vbe.c:700
    pop di                                    ; 5f                          ; 0xc4199
    pop si                                    ; 5e                          ; 0xc419a
    pop bp                                    ; 5d                          ; 0xc419b
    retn                                      ; c3                          ; 0xc419c
  ; disGetNextSymbol 0xc419d LB 0xf7 -> off=0x0 cb=00000000000000f7 uValue=00000000000c419d 'private_biosfn_custom_mode'
private_biosfn_custom_mode:                  ; 0xc419d LB 0xf7
    push bp                                   ; 55                          ; 0xc419d vbe.c:726
    mov bp, sp                                ; 89 e5                       ; 0xc419e
    push si                                   ; 56                          ; 0xc41a0
    push di                                   ; 57                          ; 0xc41a1
    sub sp, strict byte 00006h                ; 83 ec 06                    ; 0xc41a2
    push ax                                   ; 50                          ; 0xc41a5
    mov si, dx                                ; 89 d6                       ; 0xc41a6
    mov di, cx                                ; 89 cf                       ; 0xc41a8
    mov word [bp-00ah], strict word 0004fh    ; c7 46 f6 4f 00              ; 0xc41aa vbe.c:739
    push SS                                   ; 16                          ; 0xc41af vbe.c:740
    pop ES                                    ; 07                          ; 0xc41b0
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc41b1
    test al, al                               ; 84 c0                       ; 0xc41b4 vbe.c:741
    jne short 041d8h                          ; 75 20                       ; 0xc41b6
    push SS                                   ; 16                          ; 0xc41b8 vbe.c:743
    pop ES                                    ; 07                          ; 0xc41b9
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc41ba
    mov ax, word [es:di]                      ; 26 8b 05                    ; 0xc41bd vbe.c:744
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc41c0
    mov al, byte [es:si+001h]                 ; 26 8a 44 01                 ; 0xc41c3 vbe.c:745
    and ax, strict word 0007fh                ; 25 7f 00                    ; 0xc41c7
    mov ch, al                                ; 88 c5                       ; 0xc41ca
    cmp AL, strict byte 008h                  ; 3c 08                       ; 0xc41cc vbe.c:750
    je short 041e0h                           ; 74 10                       ; 0xc41ce
    cmp AL, strict byte 010h                  ; 3c 10                       ; 0xc41d0
    je short 041e0h                           ; 74 0c                       ; 0xc41d2
    cmp AL, strict byte 020h                  ; 3c 20                       ; 0xc41d4
    je short 041e0h                           ; 74 08                       ; 0xc41d6
    mov word [bp-00ah], 00100h                ; c7 46 f6 00 01              ; 0xc41d8 vbe.c:751
    jmp near 04282h                           ; e9 a2 00                    ; 0xc41dd vbe.c:752
    push SS                                   ; 16                          ; 0xc41e0 vbe.c:756
    pop ES                                    ; 07                          ; 0xc41e1
    test byte [es:si+001h], 080h              ; 26 f6 44 01 80              ; 0xc41e2
    je short 041eeh                           ; 74 05                       ; 0xc41e7
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc41e9
    jmp short 041f0h                          ; eb 02                       ; 0xc41ec
    xor ax, ax                                ; 31 c0                       ; 0xc41ee
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc41f0
    cmp bx, 00280h                            ; 81 fb 80 02                 ; 0xc41f3 vbe.c:759
    jnc short 041feh                          ; 73 05                       ; 0xc41f7
    mov bx, 00280h                            ; bb 80 02                    ; 0xc41f9 vbe.c:760
    jmp short 04207h                          ; eb 09                       ; 0xc41fc vbe.c:761
    cmp bx, 00a00h                            ; 81 fb 00 0a                 ; 0xc41fe
    jbe short 04207h                          ; 76 03                       ; 0xc4202
    mov bx, 00a00h                            ; bb 00 0a                    ; 0xc4204 vbe.c:762
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc4207 vbe.c:763
    cmp ax, 001e0h                            ; 3d e0 01                    ; 0xc420a
    jnc short 04216h                          ; 73 07                       ; 0xc420d
    mov word [bp-008h], 001e0h                ; c7 46 f8 e0 01              ; 0xc420f vbe.c:764
    jmp short 04220h                          ; eb 0a                       ; 0xc4214 vbe.c:765
    cmp ax, 00780h                            ; 3d 80 07                    ; 0xc4216
    jbe short 04220h                          ; 76 05                       ; 0xc4219
    mov word [bp-008h], 00780h                ; c7 46 f8 80 07              ; 0xc421b vbe.c:766
    mov dx, strict word 0ffffh                ; ba ff ff                    ; 0xc4220 vbe.c:772
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc4223
    call 03bc8h                               ; e8 9f f9                    ; 0xc4226
    mov si, ax                                ; 89 c6                       ; 0xc4229
    mov al, ch                                ; 88 e8                       ; 0xc422b vbe.c:775
    xor ah, ah                                ; 30 e4                       ; 0xc422d
    cwd                                       ; 99                          ; 0xc422f
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc4230
    sal dx, CL                                ; d3 e2                       ; 0xc4232
    db  01bh, 0c2h
    ; sbb ax, dx                                ; 1b c2                     ; 0xc4234
    sar ax, CL                                ; d3 f8                       ; 0xc4236
    mov dx, ax                                ; 89 c2                       ; 0xc4238
    mov ax, bx                                ; 89 d8                       ; 0xc423a
    mul dx                                    ; f7 e2                       ; 0xc423c
    add ax, strict word 00003h                ; 05 03 00                    ; 0xc423e vbe.c:776
    and AL, strict byte 0fch                  ; 24 fc                       ; 0xc4241
    mov dx, word [bp-008h]                    ; 8b 56 f8                    ; 0xc4243 vbe.c:778
    mul dx                                    ; f7 e2                       ; 0xc4246
    cmp dx, si                                ; 39 f2                       ; 0xc4248 vbe.c:780
    jnbe short 04252h                         ; 77 06                       ; 0xc424a
    jne short 04259h                          ; 75 0b                       ; 0xc424c
    test ax, ax                               ; 85 c0                       ; 0xc424e
    jbe short 04259h                          ; 76 07                       ; 0xc4250
    mov word [bp-00ah], 00200h                ; c7 46 f6 00 02              ; 0xc4252 vbe.c:782
    jmp short 04282h                          ; eb 29                       ; 0xc4257 vbe.c:783
    xor ax, ax                                ; 31 c0                       ; 0xc4259 vbe.c:787
    call 00600h                               ; e8 a2 c3                    ; 0xc425b
    mov al, ch                                ; 88 e8                       ; 0xc425e vbe.c:788
    xor ah, ah                                ; 30 e4                       ; 0xc4260
    call 03b3fh                               ; e8 da f8                    ; 0xc4262
    mov ax, bx                                ; 89 d8                       ; 0xc4265 vbe.c:789
    call 03ae8h                               ; e8 7e f8                    ; 0xc4267
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc426a vbe.c:790
    call 03b07h                               ; e8 97 f8                    ; 0xc426d
    xor ax, ax                                ; 31 c0                       ; 0xc4270 vbe.c:791
    call 00626h                               ; e8 b1 c3                    ; 0xc4272
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc4275 vbe.c:792
    or AL, strict byte 001h                   ; 0c 01                       ; 0xc4278
    xor ah, ah                                ; 30 e4                       ; 0xc427a
    call 00600h                               ; e8 81 c3                    ; 0xc427c
    call 006f8h                               ; e8 76 c4                    ; 0xc427f vbe.c:793
    push SS                                   ; 16                          ; 0xc4282 vbe.c:801
    pop ES                                    ; 07                          ; 0xc4283
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc4284
    mov bx, word [bp-00ch]                    ; 8b 5e f4                    ; 0xc4287
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc428a
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc428d vbe.c:802
    pop di                                    ; 5f                          ; 0xc4290
    pop si                                    ; 5e                          ; 0xc4291
    pop bp                                    ; 5d                          ; 0xc4292
    retn                                      ; c3                          ; 0xc4293

  ; Padding 0x36c bytes at 0xc4294
  times 876 db 0

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
    db  'Oracle VM VirtualBox Version 6.1.3 VGA BIOS', 00dh, 00ah, 000h
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
    db  'Oracle VM VirtualBox Version 6.1.3', 000h
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
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 0fdh
